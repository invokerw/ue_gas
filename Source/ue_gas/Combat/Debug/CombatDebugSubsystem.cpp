#include "Combat/Debug/CombatDebugSubsystem.h"

#include "Combat/Attributes/CombatAttributeSet.h"
#include "Combat/Attack/CombatAttackComponent.h"
#include "Combat/Aura/CombatAuraSubsystem.h"
#include "Combat/Log/CombatEventSubsystem.h"
#include "Combat/Modifiers/CombatModifierComponent.h"
#include "Combat/Motion/CombatMotionComponent.h"
#include "Combat/Network/CombatNetworkSecuritySubsystem.h"
#include "Combat/Order/CombatOrderComponent.h"
#include "Combat/Projectile/CombatProjectileActor.h"
#include "Combat/Projectile/CombatProjectileSubsystem.h"
#include "Combat/Scheduling/CombatSchedulerSubsystem.h"
#include "Combat/Thinker/CombatThinkerSubsystem.h"
#include "Combat/Unit/CombatUnitCharacter.h"
#include "DrawDebugHelpers.h"
#include "Engine/NetConnection.h"
#include "Engine/NetDriver.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"

namespace CombatDebugConsole
{
	/** 允许 Dedicated 控制台与本地 PIE 无需资产即可切换战斗绘制。 */
	static TAutoConsoleVariable<int32> Draw(
		TEXT("combat.Debug.Draw"),
		0,
		TEXT("绘制战斗单位、Order、Motion 与 Projectile。0=关闭，1=启用。"),
		ECVF_Cheat);

	/** 输出当前 World 的容量和安全指标。 */
	static FAutoConsoleCommandWithWorld Metrics(
		TEXT("combat.Debug.Metrics"),
		TEXT("输出当前 World 的战斗运行指标。"),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
		{
			if (UCombatDebugSubsystem* Debug = World ? World->GetSubsystem<UCombatDebugSubsystem>() : nullptr)
			{
				UE_LOG(LogCombat, Display, TEXT("CombatMetrics %s"), *Debug->CaptureMetrics().ToString());
			}
		}));

	/** 按 Actor UniqueID 或对象名输出单位转储。 */
	static FAutoConsoleCommandWithWorldAndArgs Unit(
		TEXT("combat.Debug.Unit"),
		TEXT("combat.Debug.Unit <ActorUniqueId|Name>：输出指定战斗单位。"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (!World || Args.IsEmpty())
			{
				UE_LOG(LogCombat, Warning, TEXT("Usage: combat.Debug.Unit <ActorUniqueId|Name>"));
				return;
			}
			int32 RequestedId = INDEX_NONE;
			LexTryParseString(RequestedId, *Args[0]);
			for (TActorIterator<ACombatUnitCharacter> It(World); It; ++It)
			{
				if (It->GetUniqueID() == RequestedId || It->GetName().Equals(Args[0], ESearchCase::IgnoreCase))
				{
					UE_LOG(LogCombat, Display, TEXT("%s"), *World->GetSubsystem<UCombatDebugSubsystem>()->DumpUnit(*It));
					return;
				}
			}
			UE_LOG(LogCombat, Warning, TEXT("Combat unit not found: %s"), *Args[0]);
		}));

	/** 按根事件序号输出当前诊断窗口内的因果链。 */
	static FAutoConsoleCommandWithWorldAndArgs Event(
		TEXT("combat.Debug.Event"),
		TEXT("combat.Debug.Event <RootSequence>：展开根事件因果链。"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			uint64 Sequence = 0;
			if (!World || Args.IsEmpty() || !LexTryParseString(Sequence, *Args[0]))
			{
				UE_LOG(LogCombat, Warning, TEXT("Usage: combat.Debug.Event <RootSequence>"));
				return;
			}
			FCombatEventId RootId;
			RootId.Sequence = Sequence;
			UE_LOG(LogCombat, Display, TEXT("%s"), *World->GetSubsystem<UCombatDebugSubsystem>()->DumpRootEvent(RootId));
		}));
}

FString FCombatRuntimeMetrics::ToString() const
{
	return FString::Printf(
		TEXT("Units=%d Modifiers=%d Attacks=%d Motions=%d PendingOrders=%d Projectiles=%d Thinkers=%d Auras=%d AuraChildren=%d SchedulerSlots=%d SchedulerCallbacks=%d SchedulerDeferrals=%d Events=%lld RpcRejected=%lld FrameSamples=%d FrameP95Ms=%.3f FrameP99Ms=%.3f Connections=%d MaxOutKiBps=%.3f"),
		Units, Modifiers, Attacks, Motions, PendingOrders, Projectiles, Thinkers, Auras, AuraChildren,
		SchedulerSlots, SchedulerCallbacks, SchedulerDeferrals,
		static_cast<long long>(EmittedEvents), static_cast<long long>(RejectedOrderRequests),
		ServerFrameSamples, ServerFrameP95Ms, ServerFrameP99Ms, NetworkConnections, MaxConnectionOutKiBps);
}

FString UCombatDebugSubsystem::DumpUnit(const ACombatUnitCharacter* Unit) const
{
	if (!IsValid(Unit))
	{
		return TEXT("CombatUnit=<Invalid>");
	}

	const UCombatAttributeSet* Attributes = Unit->GetCombatAttributeSet();
	const UCombatModifierComponent* Modifiers = Unit->GetCombatModifierComponent();
	const UCombatAttackComponent* Attacks = Unit->GetCombatAttackComponent();
	const UCombatOrderComponent* Orders = Unit->GetCombatOrderComponent();
	const UCombatMotionComponent* Motions = Unit->GetCombatMotionComponent();
	FString MovementTopology;
	const bool bMovementTopologyValid = Unit->ValidateServerMovementTopology(MovementTopology);
	return FString::Printf(
		TEXT("CombatUnit Name=%s Id=%d Definition=%s Team=%s Life=%d Generation=%u Health=%.2f/%.2f Mana=%.2f/%.2f AscPolicy=%d Modifiers=%d Attacks=%d Motions=%d OrderState=%d PendingOrders=%d Location=%s MovementTopologyValid=%s MovementTopology={%s}"),
		*Unit->GetName(), Unit->GetUniqueID(), *Unit->GetUnitDefinitionId().ToString(), *Unit->GetCombatTeamId().ToString(),
		static_cast<int32>(Unit->GetLifeState()), Unit->GetLifeGeneration(),
		Attributes ? Attributes->GetHealth() : 0.0f, Attributes ? Attributes->GetMaxHealth() : 0.0f,
		Attributes ? Attributes->GetMana() : 0.0f, Attributes ? Attributes->GetMaxMana() : 0.0f,
		static_cast<int32>(Unit->GetEffectiveAscReplicationPolicy()),
		Modifiers ? Modifiers->GetActiveModifierCount() : 0,
		Attacks ? Attacks->GetActiveAttackCount() : 0,
		Motions ? Motions->GetActiveMotionCount() : 0,
		Orders ? static_cast<int32>(Orders->GetCurrentState()) : 0,
		Orders ? Orders->GetPendingOrderCount() : 0,
		*Unit->GetActorLocation().ToCompactString(),
		bMovementTopologyValid ? TEXT("Yes") : TEXT("No"), *MovementTopology);
}

FString UCombatDebugSubsystem::DumpRootEvent(const FCombatEventId RootEventId) const
{
	const UCombatEventSubsystem* Events = GetWorld() ? GetWorld()->GetSubsystem<UCombatEventSubsystem>() : nullptr;
	if (!Events || !RootEventId.IsValid())
	{
		return FString::Printf(TEXT("RootEvent=%s <Invalid>"), *RootEventId.ToString());
	}

	const TArray<FCombatLogRecord> Records = Events->GetRecordsForRootEvent(RootEventId);
	FString Result = FString::Printf(TEXT("RootEvent=%s Records=%d"), *RootEventId.ToString(), Records.Num());
	for (const FCombatLogRecord& Record : Records)
	{
		Result += FString::Printf(TEXT("\n%*s%s"), FMath::Max(0, Record.Context.Depth * 2), TEXT(""), *Record.ToString());
	}
	return Result;
}

FCombatRuntimeMetrics UCombatDebugSubsystem::CaptureMetrics() const
{
	FCombatRuntimeMetrics Metrics;
	UWorld* World = GetWorld();
	if (!World)
	{
		return Metrics;
	}

	for (TActorIterator<ACombatUnitCharacter> It(World); It; ++It)
	{
		++Metrics.Units;
		Metrics.Modifiers += It->GetCombatModifierComponent() ? It->GetCombatModifierComponent()->GetActiveModifierCount() : 0;
		Metrics.Attacks += It->GetCombatAttackComponent() ? It->GetCombatAttackComponent()->GetActiveAttackCount() : 0;
		Metrics.Motions += It->GetCombatMotionComponent() ? It->GetCombatMotionComponent()->GetActiveMotionCount() : 0;
		Metrics.PendingOrders += It->GetCombatOrderComponent() ? It->GetCombatOrderComponent()->GetPendingOrderCount() : 0;
	}
	if (const UCombatProjectileSubsystem* Projectiles = World->GetSubsystem<UCombatProjectileSubsystem>()) Metrics.Projectiles = Projectiles->GetActiveProjectileCount();
	if (const UCombatThinkerSubsystem* Thinkers = World->GetSubsystem<UCombatThinkerSubsystem>()) Metrics.Thinkers = Thinkers->GetActiveThinkerCount();
	if (const UCombatAuraSubsystem* Auras = World->GetSubsystem<UCombatAuraSubsystem>())
	{
		Metrics.Auras = Auras->GetActiveAuraCount();
		Metrics.AuraChildren = Auras->GetTotalChildCount();
	}
	if (const UCombatSchedulerSubsystem* Scheduler = World->GetSubsystem<UCombatSchedulerSubsystem>())
	{
		const FCombatSchedulerStats Stats = Scheduler->GetStats();
		Metrics.SchedulerSlots = Stats.ActiveSlots;
		Metrics.SchedulerCallbacks = Stats.CallbacksLastFrame;
		Metrics.SchedulerDeferrals = Stats.BudgetDeferrals;
	}
	if (const UCombatEventSubsystem* Events = World->GetSubsystem<UCombatEventSubsystem>()) Metrics.EmittedEvents = static_cast<int64>(Events->GetTotalEmittedRecordCount());
	if (const UCombatNetworkSecuritySubsystem* Security = World->GetSubsystem<UCombatNetworkSecuritySubsystem>()) Metrics.RejectedOrderRequests = static_cast<int64>(Security->GetSecurityStats().RejectedRequests);
	Metrics.ServerFrameSamples = ServerFrameSamplesMs.Num();
	if (!ServerFrameSamplesMs.IsEmpty())
	{
		TArray<float> SortedSamples = ServerFrameSamplesMs;
		SortedSamples.Sort();
		auto Percentile = [&SortedSamples](const float Quantile)
		{
			const int32 Index = FMath::Clamp(FMath::CeilToInt(Quantile * SortedSamples.Num()) - 1, 0, SortedSamples.Num() - 1);
			return SortedSamples[Index];
		};
		Metrics.ServerFrameP95Ms = Percentile(0.95f);
		Metrics.ServerFrameP99Ms = Percentile(0.99f);
	}
	if (const UNetDriver* NetDriver = World->GetNetDriver())
	{
		Metrics.NetworkConnections = NetDriver->ClientConnections.Num();
	}
	Metrics.MaxConnectionOutKiBps = MaxObservedConnectionOutKiBps;
	return Metrics;
}

void UCombatDebugSubsystem::Tick(const float DeltaTime)
{
	UWorld* World = GetWorld();
	RecordPerformanceSample(DeltaTime);
	if (!World || (!bDebugDrawEnabled && CombatDebugConsole::Draw.GetValueOnGameThread() == 0))
	{
		return;
	}

	for (TActorIterator<ACombatUnitCharacter> It(World); It; ++It)
	{
		const FVector Location = It->GetActorLocation();
		DrawDebugString(World, Location + FVector(0.0, 0.0, 120.0), DumpUnit(*It), nullptr, FColor::White, 0.0f, true, 0.7f);
		if (const UCombatOrderComponent* Orders = It->GetCombatOrderComponent(); Orders && Orders->GetCurrentState() != ECombatOrderState::Idle)
		{
			DrawDebugLine(World, Location, Orders->GetCurrentMoveGoal(), FColor::Cyan, false, 0.0f, 0, 2.0f);
		}
		if (const UCombatMotionComponent* Motions = It->GetCombatMotionComponent(); Motions && Motions->GetActiveMotionCount() > 0)
		{
			DrawDebugDirectionalArrow(World, Location, Location + It->GetVelocity().GetSafeNormal() * 150.0f, 30.0f, FColor::Orange, false, 0.0f, 0, 2.0f);
		}
	}
	for (TActorIterator<ACombatProjectileActor> It(World); It; ++It)
	{
		DrawDebugSphere(World, It->GetActorLocation(), 20.0f, 8, FColor::Magenta, false, 0.0f, 0, 1.5f);
		DrawDebugDirectionalArrow(World, It->GetActorLocation(), It->GetActorLocation() + It->GetVelocity().GetSafeNormal() * 100.0f, 20.0f, FColor::Magenta, false, 0.0f, 0, 1.5f);
	}
}

void UCombatDebugSubsystem::RecordPerformanceSample(const float DeltaTime)
{
	UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_Client)
	{
		return;
	}
	if (PerformanceSamplingStartRealTime < 0.0)
	{
		PerformanceSamplingStartRealTime = World->GetRealTimeSeconds();
	}
	// 地图加载、资产发现和 NetDriver 建立不属于稳态战斗帧；固定预热也让不同启动速度可比较。
	if (World->GetRealTimeSeconds() - PerformanceSamplingStartRealTime < 5.0)
	{
		return;
	}
	constexpr int32 MaxRollingFrameSamples = 4096;
	const float FrameTimeMs = DeltaTime * 1000.0f;
	if (FMath::IsFinite(FrameTimeMs) && FrameTimeMs >= 0.0f)
	{
		if (ServerFrameSamplesMs.Num() < MaxRollingFrameSamples)
		{
			ServerFrameSamplesMs.Add(FrameTimeMs);
		}
		else
		{
			ServerFrameSamplesMs[NextServerFrameSampleIndex] = FrameTimeMs;
			NextServerFrameSampleIndex = (NextServerFrameSampleIndex + 1) % MaxRollingFrameSamples;
		}
	}
	if (const UNetDriver* NetDriver = World->GetNetDriver())
	{
		for (const TObjectPtr<UNetConnection>& Connection : NetDriver->ClientConnections)
		{
			if (Connection)
			{
				MaxObservedConnectionOutKiBps = FMath::Max(
					MaxObservedConnectionOutKiBps,
					static_cast<float>(Connection->OutBytesPerSecond) / 1024.0f);
			}
		}
	}
}

TStatId UCombatDebugSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UCombatDebugSubsystem, STATGROUP_Tickables);
}

bool UCombatDebugSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE || WorldType == EWorldType::GamePreview;
}
