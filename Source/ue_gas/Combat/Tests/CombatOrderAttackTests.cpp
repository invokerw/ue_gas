#include "CoreMinimal.h"

#include <limits>

#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/World.h"
#include "Misc/AutomationTest.h"

#include "Combat/Ability/CombatAbilitySystemComponent.h"
#include "Combat/Attack/CombatAttackComponent.h"
#include "Combat/Attack/CombatAttackTimingPolicy.h"
#include "Combat/Attributes/CombatAttributeSet.h"
#include "Combat/Core/CombatTags.h"
#include "Combat/Data/CombatDefinitionData.h"
#include "Combat/Demo/CombatDemoAbilities.h"
#include "Combat/Demo/CombatDemoModifierRuntimes.h"
#include "Combat/Log/CombatEventSubsystem.h"
#include "Combat/Modifiers/CombatModifierComponent.h"
#include "Combat/Order/CombatOrderComponent.h"
#include "Combat/Scheduling/CombatSchedulerSubsystem.h"
#include "Combat/Tests/CombatAutomationWorldFixture.h"
#include "Combat/Unit/CombatUnitCharacter.h"
#include "Combat/Unit/CombatUnitLifecycleComponent.h"

namespace CombatOrderAttackTests
{
	/** M4 自动化统一使用 EditorContext 与 EngineFilter。 */
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;

	/** 生成指定队伍、属性和攻击策略的已初始化 Combat Unit。 */
	ACombatUnitCharacter* SpawnUnit(
		UWorld& World,
		const FName DefinitionName,
		const FVector Location,
		const uint8 Team,
		const FCombatUnitBaseStats& Stats,
		const float AttackPoint = 0.20f)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		ACombatUnitCharacter* Unit = World.SpawnActor<ACombatUnitCharacter>(Location, FRotator::ZeroRotator, Params);
		if (!Unit)
		{
			return nullptr;
		}
		UCombatUnitData* Data = NewObject<UCombatUnitData>(Unit);
		Data->DefinitionName = DefinitionName;
		Data->InitialTeamId = FCombatTeamId(Team);
		Data->BaseStats = Stats;
		Data->BaseAttackPoint = AttackPoint;
		return Unit->InitializeFromUnitData(Data) ? Unit : nullptr;
	}

	/** 创建一个有限点位置的 MoveToPoint Order。 */
	FCombatOrderRequest MakeMoveOrder(const FVector Location)
	{
		FCombatOrderRequest Request;
		Request.Type = ECombatOrderType::MoveToPoint;
		Request.TargetLocation = Location;
		Request.bHasTargetLocation = true;
		return Request;
	}

	/** 创建使用 Demo Orb Runtime 的最小 ModifierData。 */
	UCombatModifierData* MakeOrbData(
		UObject& Outer,
		const FName DefinitionName,
		const int32 Priority,
		const float ManaCost,
		const float BonusDamage,
		const float OnHitDamage,
		const bool bCommitSucceeds = true)
	{
		UCombatModifierData* Data = NewObject<UCombatModifierData>(&Outer);
		Data->DefinitionName = DefinitionName;
		Data->RuntimeClass = UCombatDemoOrbRuntime::StaticClass();
		Data->Priority = Priority;
		Data->RuntimeParameters.Add(TEXT("mana_cost"), ManaCost);
		Data->RuntimeParameters.Add(TEXT("bonus_damage"), BonusDamage);
		Data->RuntimeParameters.Add(TEXT("on_hit_damage"), OnHitDamage);
		Data->RuntimeParameters.Add(TEXT("on_hit_damage_type"), 1.0f);
		Data->RuntimeParameters.Add(TEXT("commit_succeeds"), bCommitSucceeds ? 1.0f : 0.0f);
		return Data;
	}

	/** 通过正式 ModifierComponent 入口施加法球并返回 Runtime。 */
	UCombatDemoOrbRuntime* ApplyOrb(
		ACombatUnitCharacter& Unit,
		UCombatModifierData& Data,
		FCombatModifierHandle& OutHandle)
	{
		FCombatModifierApplyRequest Request;
		Request.Source = &Unit;
		Request.ModifierData = &Data;
		const FCombatModifierApplyResult Result = Unit.GetCombatModifierComponent()->ApplyModifier(Request);
		OutHandle = Result.Handle;
		return Result.bSuccess
			? Cast<UCombatDemoOrbRuntime>(Unit.GetCombatModifierComponent()->FindRuntime(Result.Handle)) : nullptr;
	}
}

/** 验证 GAP-010 冻结公式、clamp 和非法输入拒绝。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCombatAttackTimingPolicyTest,
	"Combat.OrderAttack.Timing.PolicyV1",
	CombatOrderAttackTests::Flags)

bool FCombatAttackTimingPolicyTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FCombatAttackTiming Timing;
	TestTrue(TEXT("IAS 100 timing is valid"), FCombatAttackTimingPolicyV1::Calculate(1.7f, 100.0f, 0.3f, Timing));
	TestTrue(TEXT("IAS 100 keeps BAT"), FMath::IsNearlyEqual(Timing.AttackInterval, 1.7f));
	TestTrue(TEXT("IAS 100 keeps base attack point"), FMath::IsNearlyEqual(Timing.AttackPoint, 0.3f));
	TestTrue(TEXT("Recovery is interval minus attack point"), FMath::IsNearlyEqual(Timing.Recovery, 1.4f, 0.0001f));

	TestTrue(TEXT("IAS 200 timing is valid"), FCombatAttackTimingPolicyV1::Calculate(1.7f, 200.0f, 0.3f, Timing));
	TestTrue(TEXT("IAS 200 halves interval"), FMath::IsNearlyEqual(Timing.AttackInterval, 0.85f));
	TestTrue(TEXT("IAS 200 halves attack point"), FMath::IsNearlyEqual(Timing.AttackPoint, 0.15f));
	TestTrue(TEXT("High IAS is clamped to minimum interval"),
		FCombatAttackTimingPolicyV1::Calculate(1.0f, 5000.0f, 0.8f, Timing)
		&& FMath::IsNearlyEqual(Timing.AttackInterval, 0.20f));
	TestTrue(TEXT("Attack point never exceeds interval"), Timing.AttackPoint <= Timing.AttackInterval);
	TestFalse(TEXT("NaN BAT is rejected"), FCombatAttackTimingPolicyV1::Calculate(
		std::numeric_limits<float>::quiet_NaN(), 100.0f, 0.3f, Timing));
	TestFalse(TEXT("Negative attack point is rejected"), FCombatAttackTimingPolicyV1::Calculate(1.7f, 100.0f, -0.1f, Timing));
	return true;
}

/** 验证 FIFO、replace、Stop 与旧 generation 移动回调。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCombatOrderQueueGenerationTest,
	"Combat.OrderAttack.Order.FifoReplaceStopAndStaleMove",
	CombatOrderAttackTests::Flags)

bool FCombatOrderQueueGenerationTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FCombatAutomationWorldFixture Fixture;
	if (!Fixture.IsValid()) { AddError(TEXT("Could not create M4 order world")); return false; }
	UWorld& World = *Fixture.GetWorld();
	FCombatUnitBaseStats Stats;
	ACombatUnitCharacter* Unit = CombatOrderAttackTests::SpawnUnit(
		World, TEXT("order_unit"), FVector::ZeroVector, 1, Stats);
	if (!Unit) { AddError(TEXT("Could not spawn order unit")); return false; }
	UCombatOrderComponent* Orders = Unit->GetCombatOrderComponent();
	Orders->SetNavigationDeferredForTesting(true);
	TArray<FCombatOrderResult> Finished;
	Orders->OnOrderFinished().AddLambda([&Finished](const FCombatOrderResult& Result) { Finished.Add(Result); });

	const FCombatOrderResult First = Orders->IssueOrder(
		CombatOrderAttackTests::MakeMoveOrder(FVector(1000.0, 0.0, 0.0)), false);
	const FCombatOrderResult Queued = Orders->IssueOrder(
		CombatOrderAttackTests::MakeMoveOrder(FVector(2000.0, 0.0, 0.0)), true);
	TestTrue(TEXT("First move is accepted and deferred"), First.bSuccess && First.Handle.IsValid());
	TestTrue(TEXT("Second move is queued"), Queued.bSuccess && Orders->GetPendingOrderCount() == 1);

	const FCombatOrderResult Replacement = Orders->IssueOrder(
		CombatOrderAttackTests::MakeMoveOrder(FVector(3000.0, 0.0, 0.0)), false);
	TestTrue(TEXT("Replacement advances generation"), Replacement.Handle.Key.Generation != First.Handle.Key.Generation);
	TestEqual(TEXT("Replacement clears old pending FIFO"), Orders->GetPendingOrderCount(), 0);
	TestFalse(TEXT("Old movement callback cannot affect replacement"),
		Orders->CompleteMovementForTesting(First.Handle, true));
	TestEqual(TEXT("Replacement remains current after stale callback"), Orders->GetCurrentOrderHandle(), Replacement.Handle);

	FCombatOrderRequest Stop;
	Stop.Type = ECombatOrderType::Stop;
	TestTrue(TEXT("Stop is accepted"), Orders->IssueOrder(Stop, false).bSuccess);
	TestEqual(TEXT("Stop leaves state idle"), Orders->GetCurrentState(), ECombatOrderState::Idle);
	TestEqual(TEXT("Stop leaves no pending orders"), Orders->GetPendingOrderCount(), 0);

	const FCombatOrderResult FifoA = Orders->IssueOrder(
		CombatOrderAttackTests::MakeMoveOrder(FVector(100.0, 0.0, 0.0)), false);
	const FCombatOrderResult FifoB = Orders->IssueOrder(
		CombatOrderAttackTests::MakeMoveOrder(FVector(200.0, 0.0, 0.0)), true);
	TestTrue(TEXT("Reached partial path is accepted as a valid FIFO completion"),
		Orders->CompleteMovementForTesting(FifoA.Handle, true, true));
	TestEqual(TEXT("Second FIFO item becomes current only after first completion"), Orders->GetCurrentOrderHandle(), FifoB.Handle);
	TestTrue(TEXT("Injected second FIFO completion is accepted"), Orders->CompleteMovementForTesting(FifoB.Handle, true));
	TestEqual(TEXT("FIFO drains to idle"), Orders->GetCurrentState(), ECombatOrderState::Idle);
	TestTrue(TEXT("Both FIFO results were broadcast in order"), Finished.Num() >= 2
		&& Finished[Finished.Num() - 2].Handle == FifoA.Handle && Finished.Last().Handle == FifoB.Handle
		&& Finished[Finished.Num() - 2].bSuccess && Finished.Last().bSuccess);
	return true;
}

/** 验证零前摇 Cast 通过同步 OrderReleased 推进且不等待 cooldown。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCombatOrderAbilityReleaseTest,
	"Combat.OrderAttack.Order.AbilityOrderReleased",
	CombatOrderAttackTests::Flags)

bool FCombatOrderAbilityReleaseTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FCombatAutomationWorldFixture Fixture;
	if (!Fixture.IsValid()) { AddError(TEXT("Could not create M4 ability order world")); return false; }
	UWorld& World = *Fixture.GetWorld();
	FCombatUnitBaseStats Stats;
	ACombatUnitCharacter* Unit = CombatOrderAttackTests::SpawnUnit(
		World, TEXT("ability_order_unit"), FVector::ZeroVector, 1, Stats);
	if (!Unit) { return false; }

	UCombatAbilityData* Data = NewObject<UCombatAbilityData>(Unit);
	Data->DefinitionName = TEXT("order_no_target_ability");
	Data->BehaviorTags.AddTag(CombatTags::Ability_Behavior_NoTarget);
	Data->TargetingRules.TargetTeamTag = CombatTags::TargetTeam_None;
	GetMutableDefault<UCombatSelfHealAbility>()->AbilityData = Data;
	FGameplayAbilitySpecHandle AbilityHandle;
	FGameplayTag FailureTag;
	TestTrue(TEXT("Order test ability is granted"), Unit->GetCombatAbilitySystemComponent()->GrantCombatAbility(
		UCombatSelfHealAbility::StaticClass(), 1, false, AbilityHandle, FailureTag));

	TArray<FCombatOrderResult> Finished;
	Unit->GetCombatOrderComponent()->OnOrderFinished().AddLambda(
		[&Finished](const FCombatOrderResult& Result) { Finished.Add(Result); });
	FCombatOrderRequest Request;
	Request.Type = ECombatOrderType::CastNoTarget;
	Request.AbilitySpecHandle = AbilityHandle;
	const FCombatOrderResult Accepted = Unit->GetCombatOrderComponent()->IssueOrder(Request, false);
	TestTrue(TEXT("CastNoTarget order is accepted"), Accepted.bSuccess);
	TestEqual(TEXT("Synchronous OrderReleased drains current order"),
		Unit->GetCombatOrderComponent()->GetCurrentState(), ECombatOrderState::Idle);
	TestTrue(TEXT("Cast order reports success exactly once"),
		Finished.Num() == 1 && Finished[0].bSuccess && Finished[0].Handle == Accepted.Handle);
	TestEqual(TEXT("Cast does not wait for cooldown to release order"),
		Unit->GetCombatAbilitySystemComponent()->GetCombatAbilityCooldownRemaining(AbilityHandle), 0.0f);
	return true;
}

/** 验证 AttackRecord 命中、重复 finalize、Death/Respawn 与旧 Handle。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCombatAttackRegistryLifecycleTest,
	"Combat.OrderAttack.Attack.RegistryExactlyOnceAndLifeGeneration",
	CombatOrderAttackTests::Flags)

bool FCombatAttackRegistryLifecycleTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FCombatAutomationWorldFixture Fixture;
	if (!Fixture.IsValid()) { AddError(TEXT("Could not create M4 attack registry world")); return false; }
	UWorld& World = *Fixture.GetWorld();
	FCombatUnitBaseStats AttackerStats;
	AttackerStats.AttackDamage = 40.0f;
	AttackerStats.BaseAttackTime = 1.0f;
	AttackerStats.AttackSpeed = 100.0f;
	FCombatUnitBaseStats TargetStats;
	TargetStats.MaxHealth = 200.0f;
	TargetStats.Evasion = 0.0f;
	ACombatUnitCharacter* Attacker = CombatOrderAttackTests::SpawnUnit(
		World, TEXT("registry_attacker"), FVector::ZeroVector, 1, AttackerStats, 0.20f);
	ACombatUnitCharacter* Target = CombatOrderAttackTests::SpawnUnit(
		World, TEXT("registry_target"), FVector(150.0, 0.0, 0.0), 2, TargetStats, 0.20f);
	if (!Attacker || !Target) { return false; }

	UCombatAttackComponent* Attacks = Attacker->GetCombatAttackComponent();
	const FCombatAttackResult Started = Attacks->StartMeleeAttack(Target, FCombatOrderHandle());
	TestTrue(TEXT("Melee AttackRecord starts"), Started.bSuccess && Started.Handle.IsValid());
	World.Tick(LEVELTICK_All, 0.25f);
	TestEqual(TEXT("Attack point deals physical damage once"),
		Target->GetCombatAbilitySystemComponent()->GetNumericAttribute(UCombatAttributeSet::GetHealthAttribute()), 160.0f);
	TestEqual(TEXT("Landed record is removed from registry"), Attacks->GetActiveAttackCount(), 0);
	TestFalse(TEXT("Duplicate finalize of removed handle is rejected"), Attacks->FinalizeAttack(Started.Handle).bSuccess);
	TestEqual(TEXT("Duplicate finalize cannot deal damage"),
		Target->GetCombatAbilitySystemComponent()->GetNumericAttribute(UCombatAttributeSet::GetHealthAttribute()), 160.0f);

	World.GetSubsystem<UCombatSchedulerSubsystem>()->RunDueTasks(World.GetTimeSeconds() + 2.0);
	TestTrue(TEXT("Absolute BAT schedule becomes ready once"), Attacks->IsAttackReady());
	const FCombatAttackResult Pending = Attacks->StartMeleeAttack(Target, FCombatOrderHandle());
	TestTrue(TEXT("Second life attack enters windup"), Pending.bSuccess);
	const FCombatEventContext DeathEvent = World.GetSubsystem<UCombatEventSubsystem>()->CreateRootEvent();
	TestTrue(TEXT("Owner death cancels pending AttackRecord"),
		Attacker->GetCombatLifecycleComponent()->RequestDeath(DeathEvent, Target));
	TestEqual(TEXT("Death leaves no AttackRecord"), Attacks->GetActiveAttackCount(), 0);
	TestTrue(TEXT("Respawn creates a new life generation"),
		Attacker->GetCombatLifecycleComponent()->RespawnAtLocation(FVector::ZeroVector));
	TestFalse(TEXT("Old life AttackHandle cannot finalize after respawn"), Attacks->FinalizeAttack(Pending.Handle).bSuccess);
	World.Tick(LEVELTICK_All, 1.0f);
	TestEqual(TEXT("Old windup callback cannot damage after respawn"),
		Target->GetCombatAbilitySystemComponent()->GetNumericAttribute(UCombatAttributeSet::GetHealthAttribute()), 160.0f);
	return true;
}

/** 验证 AttackTarget 是持续 Order，Landed 不出队且 Stop 终止后续循环。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCombatMeleeOrderLoopTest,
	"Combat.OrderAttack.Attack.ContinuousMeleeOrder",
	CombatOrderAttackTests::Flags)

bool FCombatMeleeOrderLoopTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FCombatAutomationWorldFixture Fixture;
	if (!Fixture.IsValid()) { AddError(TEXT("Could not create M4 melee order world")); return false; }
	UWorld& World = *Fixture.GetWorld();
	FCombatUnitBaseStats AttackerStats;
	AttackerStats.AttackDamage = 20.0f;
	AttackerStats.BaseAttackTime = 1.0f;
	FCombatUnitBaseStats TargetStats;
	TargetStats.MaxHealth = 200.0f;
	ACombatUnitCharacter* Attacker = CombatOrderAttackTests::SpawnUnit(
		World, TEXT("loop_attacker"), FVector::ZeroVector, 1, AttackerStats, 0.20f);
	ACombatUnitCharacter* Target = CombatOrderAttackTests::SpawnUnit(
		World, TEXT("loop_target"), FVector(150.0, 0.0, 0.0), 2, TargetStats, 0.20f);
	if (!Attacker || !Target) { return false; }

	FCombatOrderRequest AttackOrder;
	AttackOrder.Type = ECombatOrderType::AttackTarget;
	AttackOrder.TargetUnit = Target;
	const FCombatOrderResult Accepted = Attacker->GetCombatOrderComponent()->IssueOrder(AttackOrder, false);
	TestTrue(TEXT("AttackTarget order is accepted"), Accepted.bSuccess);
	World.Tick(LEVELTICK_All, 0.25f);
	TestEqual(TEXT("First attack lands"),
		Target->GetCombatAbilitySystemComponent()->GetNumericAttribute(UCombatAttributeSet::GetHealthAttribute()), 180.0f);
	TestEqual(TEXT("Single Landed does not pop AttackTarget"),
		Attacker->GetCombatOrderComponent()->GetCurrentOrderHandle(), Accepted.Handle);
	TestEqual(TEXT("Order waits for attack ready"),
		Attacker->GetCombatOrderComponent()->GetCurrentState(), ECombatOrderState::WaitingAttackReady);

	UCombatSchedulerSubsystem* Scheduler = World.GetSubsystem<UCombatSchedulerSubsystem>();
	Scheduler->RunDueTasks(World.GetTimeSeconds() + 2.0);
	Scheduler->RunDueTasks(World.GetTimeSeconds() + 2.0);
	TestEqual(TEXT("Ready starts a second independent attack cycle"),
		Target->GetCombatAbilitySystemComponent()->GetNumericAttribute(UCombatAttributeSet::GetHealthAttribute()), 160.0f);
	FCombatOrderRequest Stop;
	Stop.Type = ECombatOrderType::Stop;
	TestTrue(TEXT("Stop cancels continuous AttackTarget"),
		Attacker->GetCombatOrderComponent()->IssueOrder(Stop, false).bSuccess);
	TestEqual(TEXT("Stop leaves OrderComponent idle"),
		Attacker->GetCombatOrderComponent()->GetCurrentState(), ECombatOrderState::Idle);
	World.Tick(LEVELTICK_All, 2.0f);
	TestEqual(TEXT("Stop prevents later attack cycles"),
		Target->GetCombatAbilitySystemComponent()->GetNumericAttribute(UCombatAttributeSet::GetHealthAttribute()), 160.0f);
	return true;
}

/** 验证法球提交失败降级、未胜出无资源副作用和 OnHit 快照。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCombatOrbArbitrationSnapshotTest,
	"Combat.OrderAttack.Orb.TwoPhaseResourceAndSnapshot",
	CombatOrderAttackTests::Flags)

bool FCombatOrbArbitrationSnapshotTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FCombatAutomationWorldFixture Fixture;
	if (!Fixture.IsValid()) { AddError(TEXT("Could not create M4 orb world")); return false; }
	UWorld& World = *Fixture.GetWorld();
	FCombatUnitBaseStats AttackerStats;
	AttackerStats.AttackDamage = 10.0f;
	AttackerStats.BaseAttackTime = 1.0f;
	AttackerStats.MaxMana = 30.0f;
	FCombatUnitBaseStats TargetStats;
	TargetStats.MaxHealth = 200.0f;
	ACombatUnitCharacter* Attacker = CombatOrderAttackTests::SpawnUnit(
		World, TEXT("orb_attacker"), FVector::ZeroVector, 1, AttackerStats, 0.20f);
	ACombatUnitCharacter* Target = CombatOrderAttackTests::SpawnUnit(
		World, TEXT("orb_target"), FVector(150.0, 0.0, 0.0), 2, TargetStats, 0.20f);
	if (!Attacker || !Target) { return false; }

	UCombatModifierData* FailedData = CombatOrderAttackTests::MakeOrbData(
		*Attacker, TEXT("orb_failed_winner"), 30, 5.0f, 100.0f, 0.0f, false);
	UCombatModifierData* WinnerData = CombatOrderAttackTests::MakeOrbData(
		*Attacker, TEXT("orb_winner"), 20, 10.0f, 5.0f, 7.0f, true);
	UCombatModifierData* LoserData = CombatOrderAttackTests::MakeOrbData(
		*Attacker, TEXT("orb_unselected"), 10, 5.0f, 50.0f, 0.0f, true);
	FCombatModifierHandle FailedHandle;
	FCombatModifierHandle WinnerHandle;
	FCombatModifierHandle LoserHandle;
	UCombatDemoOrbRuntime* FailedRuntime = CombatOrderAttackTests::ApplyOrb(*Attacker, *FailedData, FailedHandle);
	UCombatDemoOrbRuntime* WinnerRuntime = CombatOrderAttackTests::ApplyOrb(*Attacker, *WinnerData, WinnerHandle);
	UCombatDemoOrbRuntime* LoserRuntime = CombatOrderAttackTests::ApplyOrb(*Attacker, *LoserData, LoserHandle);
	if (!FailedRuntime || !WinnerRuntime || !LoserRuntime) { AddError(TEXT("Could not apply orb candidates")); return false; }

	const FCombatAttackResult Started = Attacker->GetCombatAttackComponent()->StartMeleeAttack(Target, FCombatOrderHandle());
	TestTrue(TEXT("Orb attack starts"), Started.bSuccess);
	TestEqual(TEXT("Failed winner does not mutate successful claim count"), FailedRuntime->GetSuccessfulClaimCount(), 0);
	TestEqual(TEXT("Fallback candidate commits exactly once"), WinnerRuntime->GetSuccessfulClaimCount(), 1);
	TestEqual(TEXT("Unselected lower candidate has no side effect"), LoserRuntime->GetSuccessfulClaimCount(), 0);
	TestEqual(TEXT("Only winner spends Mana"),
		Attacker->GetCombatAbilitySystemComponent()->GetNumericAttribute(UCombatAttributeSet::GetManaAttribute()), 20.0f);

	FCombatAttackRecord Snapshot;
	TestTrue(TEXT("Pending AttackRecord exposes immutable orb snapshot"),
		Attacker->GetCombatAttackComponent()->GetAttackRecordSnapshot(Started.Handle, Snapshot));
	TestTrue(TEXT("Exactly one exclusive-group winner is stored"),
		Snapshot.ClaimedOrbs.Num() == 1 && Snapshot.ClaimedOrbs[0].SourceModifier == WinnerHandle);
	TestTrue(TEXT("Winner bonus and OnHit are copied before impact"),
		FMath::IsNearlyEqual(Snapshot.BonusDamage, 5.0f) && Snapshot.OnHitActions.Num() == 1
		&& FMath::IsNearlyEqual(Snapshot.OnHitActions[0].Magnitude, 7.0f));
	TestTrue(TEXT("Winner Modifier can be removed during windup"),
		Attacker->GetCombatModifierComponent()->RemoveModifier(WinnerHandle));
	World.Tick(LEVELTICK_All, 0.25f);
	TestTrue(TEXT("Removed winner still resolves snapshotted main and OnHit damage"),
		FMath::IsNearlyEqual(
			Target->GetCombatAbilitySystemComponent()->GetNumericAttribute(UCombatAttributeSet::GetHealthAttribute()),
			179.75f,
			0.01f));
	TestTrue(TEXT("Final result reports main plus on-hit applied damage"),
		FMath::IsNearlyEqual(Attacker->GetCombatAttackComponent()->GetLastFinalizedResult().AppliedDamage, 20.25f, 0.01f));
	return true;
}

#endif
