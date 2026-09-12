#include "CoreMinimal.h"

#include <limits>

#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/World.h"
#include "Misc/AutomationTest.h"

#include "Combat/Ability/CombatAbilitySystemComponent.h"
#include "Combat/Attack/CombatAttackComponent.h"
#include "Combat/Attributes/CombatAttributeSet.h"
#include "Combat/Core/CombatTags.h"
#include "Combat/Data/CombatDefinitionData.h"
#include "Combat/Demo/CombatDemoAbilities.h"
#include "Combat/Log/CombatEventSubsystem.h"
#include "Combat/Motion/CombatMotionComponent.h"
#include "Combat/Order/CombatOrderComponent.h"
#include "Combat/Scheduling/CombatSchedulerSubsystem.h"
#include "Combat/Tests/CombatAutomationWorldFixture.h"
#include "Combat/Unit/CombatCharacterMovementComponent.h"
#include "Combat/Unit/CombatUnitCharacter.h"
#include "Combat/Unit/CombatUnitLifecycleComponent.h"

namespace CombatFacingTests
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;

	/** 持有真实服务器测试 World、最小技能及结果监听；退出前恢复测试临时替换的 CDO，并解绑栈上监听。 */
	struct FFixture
	{
		FCombatAutomationWorldFixture WorldFixture{NM_DedicatedServer};
		UWorld* World = WorldFixture.GetWorld();
		ACombatUnitCharacter* Unit = nullptr;
		UCombatOrderComponent* Orders = nullptr;
		UCombatCharacterMovementComponent* Movement = nullptr;
		UCombatAbilitySystemComponent* Asc = nullptr;
		UCombatAbilityData* Data = nullptr;
		TObjectPtr<UCombatAbilityData> PreviousData = GetMutableDefault<UCombatPointAoeAbility>()->AbilityData;
		FGameplayAbilitySpecHandle Ability;
		TArray<FCombatOrderResult> Finished;
		FDelegateHandle FinishedDelegate;

		FFixture()
		{
			if (!World) { return; }
			Unit = SpawnUnit(TEXT("facing_caster"), FVector::ZeroVector, 1);
			if (!Unit) { return; }
			Orders = Unit->GetCombatOrderComponent();
			Movement = Cast<UCombatCharacterMovementComponent>(Unit->GetCharacterMovement());
			Asc = Unit->GetCombatAbilitySystemComponent();
			if (!Movement) { return; }
			// 同步 Automation 内多次 World.Tick 共享 GFrameCounter，TickTaskManager 只派发一次组件 Tick。
			// 本夹具自行驱动真实移动组件，每步仍由 World 推进 Scheduler；关闭自动派发避免首步重复旋转。
			Movement->SetComponentTickEnabled(false);
			Movement->RotationRate.Yaw = 180.0;
			Data = NewObject<UCombatAbilityData>(Unit);
			Data->DefinitionName = TEXT("facing_test_spell");
			Data->BehaviorTags.AddTag(CombatTags::Ability_Behavior_PointTarget);
			Data->TargetingRules.TargetTeamTag = CombatTags::TargetTeam_Enemy;
			Data->TargetingRules.CastRange = 500.0f;
			Data->CastPoint = 0.20f;
			Data->CostCommitPoint = ECombatAbilityCommitStage::CastStarted;
			FCombatSpecialValue Cost;
			Cost.Values.Add(10.0f);
			Data->SpecialValues.Add(TEXT("mana_cost"), Cost);
			GetMutableDefault<UCombatPointAoeAbility>()->AbilityData = Data;
			FGameplayTag Failure;
			Asc->GrantCombatAbility(UCombatPointAoeAbility::StaticClass(), 1, false, Ability, Failure);
			FinishedDelegate = Orders->OnOrderFinished().AddLambda(
				[this](const FCombatOrderResult& Result) { Finished.Add(Result); });
		}

		~FFixture()
		{
			if (Orders)
			{
				Orders->OnOrderFinished().Remove(FinishedDelegate);
				Orders->StopAllOrders(CombatTags::Order_Failure_Cancelled);
			}
			GetMutableDefault<UCombatPointAoeAbility>()->AbilityData = PreviousData;
		}

		/** 在无地面夹具中使用无重力飞行，仍执行真实 CharacterMovement Tick；不注入转向结果。 */
		ACombatUnitCharacter* SpawnUnit(const FName Name, const FVector& Location, const uint8 Team)
		{
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			ACombatUnitCharacter* Spawned = World->SpawnActor<ACombatUnitCharacter>(Location, FRotator::ZeroRotator, Params);
			if (!Spawned) { return nullptr; }
			UCombatUnitData* UnitData = NewObject<UCombatUnitData>(Spawned);
			UnitData->DefinitionName = Name;
			UnitData->InitialTeamId = FCombatTeamId(Team);
			UnitData->BaseStats.MaxMana = 100.0f;
			UnitData->BaseStats.ManaRegen = 0.0f;
			if (!Spawned->InitializeFromUnitData(UnitData)) { return nullptr; }
			if (!Spawned->HasActorBegunPlay()) { Spawned->DispatchBeginPlay(); }
			if (!Spawned->GetController()) { Spawned->SpawnDefaultController(); }
			Spawned->GetCharacterMovement()->GravityScale = 0.0f;
			Spawned->GetCharacterMovement()->SetMovementMode(MOVE_Flying);
			return Spawned;
		}

		/** 构造点目标技能，目标保持在施法范围内。 */
		FCombatOrderRequest PointRequest(const FVector& Location = FVector(0.0, 300.0, 0.0)) const
		{
			FCombatOrderRequest Request;
			Request.Type = ECombatOrderType::CastPoint;
			Request.TargetLocation = Location;
			Request.bHasTargetLocation = true;
			Request.AbilitySpecHandle = Ability;
			return Request;
		}

		/** 每步调用一次真实移动 Tick，再推进 World/Scheduler；不直接设置转角或注入对准结果。 */
		void Advance(const int32 Frames, const float Step = 0.02f) const
		{
			for (int32 Index = 0; Index < Frames; ++Index)
			{
				Movement->TickComponent(Step, LEVELTICK_All, nullptr);
				World->Tick(LEVELTICK_All, Step);
			}
		}

		/** 返回该测试技能已产生的指定生命周期事件数量。 */
		int32 CountEvent(const FGameplayTag Event) const
		{
			int32 Count = 0;
			for (const FCombatLogRecord& Record : World->GetSubsystem<UCombatEventSubsystem>()->GetRecentRecords())
			{
				Count += Record.EventType == Event && Record.Source.AbilityDefinitionId == Data->GetPrimaryAssetId() ? 1 : 0;
			}
			return Count;
		}
	};
}

/** 验证转速的逐帧上限、动态转速、最短旋转方向，以及转身完成后才提交资源并开始前摇。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatFacingRateAndCastTest,
	"Combat.OrderAttack.Facing.RateAndCastTiming", CombatFacingTests::Flags)

bool FCombatFacingRateAndCastTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	CombatFacingTests::FFixture F;
	if (!F.Ability.IsValid() || !F.Movement) { AddError(TEXT("Facing fixture setup failed")); return false; }
	F.Orders->IssueOrder(F.PointRequest(), false);
	TestEqual(TEXT("Cast starts in Facing"), F.Orders->GetCurrentState(), ECombatOrderState::Facing);
	TestTrue(TEXT("Issuing the order never snaps rotation"), FMath::IsNearlyZero(F.Unit->GetActorRotation().Yaw));
	F.Advance(5);
	TestTrue(TEXT("180 degrees per second turns 18 degrees in 0.1 seconds"),
		FMath::IsNearlyEqual(F.Unit->GetActorRotation().Yaw, 18.0, 0.1));
	TestEqual(TEXT("No CastStarted during turning"), F.CountEvent(CombatTags::Event_Combat_AbilityCastStarted), 0);
	TestEqual(TEXT("CastStarted cost is not paid during turning"), F.Asc->GetNumericAttribute(UCombatAttributeSet::GetManaAttribute()), 100.0f);
	F.Movement->RotationRate.Yaw = 360.0;
	F.Advance(7);
	TestEqual(TEXT("Cast still waits outside the shared 15 degree tolerance"), F.Orders->GetCurrentState(), ECombatOrderState::Facing);
	F.Advance(2);
	TestEqual(TEXT("Cast begins inside the shared 15 degree tolerance"), F.Orders->GetCurrentState(), ECombatOrderState::WaitingOrderRelease);
	const double FacingError = FMath::Abs(FMath::FindDeltaAngleDegrees(F.Unit->GetActorRotation().Yaw, 90.0));
	TestTrue(TEXT("Cast no longer waits for one degree alignment"), FacingError > 1.0 && FacingError <= 15.0);
	TestEqual(TEXT("CastStarted occurs exactly once"), F.CountEvent(CombatTags::Event_Combat_AbilityCastStarted), 1);
	TestEqual(TEXT("CastStarted commits cost after alignment"), F.Asc->GetNumericAttribute(UCombatAttributeSet::GetManaAttribute()), 90.0f);
	TestEqual(TEXT("Cast point has not elapsed yet"), F.CountEvent(CombatTags::Event_Combat_AbilitySpellStarted), 0);
	F.Advance(12);
	TestEqual(TEXT("Spell starts once after its own cast point"), F.CountEvent(CombatTags::Event_Combat_AbilitySpellStarted), 1);
	TestTrue(TEXT("Order finishes exactly once"), F.Finished.Num() == 1 && F.Finished[0].bSuccess);

	F.Unit->SetActorRotation(FRotator(0.0, 170.0, 0.0));
	F.Movement->RotationRate.Yaw = 100.0;
	F.Orders->IssueOrder(F.PointRequest(FRotator(0.0, -160.0, 0.0).Vector() * 300.0), false);
	F.Advance(5);
	TestTrue(TEXT("Yaw wrap uses the 30 degree shortest route"),
		FMath::IsNearlyEqual(FMath::Abs(F.Unit->GetActorRotation().Yaw), 180.0, 0.1));
	return true;
}

/** 验证替换、停止、眩晕、定身和 Motion 暂停，不允许旧转身回调激活新命令。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatFacingCancellationTest,
	"Combat.OrderAttack.Facing.CancellationAndControlStates", CombatFacingTests::Flags)

bool FCombatFacingCancellationTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	CombatFacingTests::FFixture F;
	if (!F.Ability.IsValid()) { AddError(TEXT("Facing fixture setup failed")); return false; }
	const FCombatOrderResult First = F.Orders->IssueOrder(F.PointRequest(), false);
	F.Advance(5);
	const FCombatOrderResult Replacement = F.Orders->IssueOrder(F.PointRequest(FVector(0.0, -300.0, 0.0)), false);
	TestTrue(TEXT("Replacement invalidates previous generation"), Replacement.Handle.Key.Generation != First.Handle.Key.Generation);
	F.Advance(5);
	TestTrue(TEXT("Only replacement direction advances"), FMath::IsNearlyZero(F.Unit->GetActorRotation().Yaw, 0.1));
	F.Orders->StopAllOrders(CombatTags::Order_Failure_Cancelled);
	F.Advance(15);
	TestTrue(TEXT("Stop leaves rotation unchanged"), FMath::IsNearlyZero(F.Unit->GetActorRotation().Yaw, 0.1));
	TestEqual(TEXT("Both cancelled orders finish exactly once"), F.Finished.Num(), 2);
	TestEqual(TEXT("Old callbacks never start a cast"), F.CountEvent(CombatTags::Event_Combat_AbilityCastStarted), 0);

	F.Orders->IssueOrder(F.PointRequest(), false);
	F.Advance(5);
	F.Asc->AddLooseGameplayTag(CombatTags::State_Stunned);
	const double PausedYaw = F.Unit->GetActorRotation().Yaw;
	F.Advance(10);
	TestEqual(TEXT("Stun pauses facing"), F.Orders->GetCurrentState(), ECombatOrderState::Paused);
	TestTrue(TEXT("Stunned unit does not rotate"), FMath::IsNearlyEqual(F.Unit->GetActorRotation().Yaw, PausedYaw));
	F.Asc->RemoveLooseGameplayTag(CombatTags::State_Stunned);
	F.Asc->AddLooseGameplayTag(CombatTags::State_Rooted);
	TestEqual(TEXT("Root disables translation"), F.Movement->MovementMode, MOVE_None);
	F.Advance(5);
	TestTrue(TEXT("Root still allows spell facing at the configured rate"),
		FMath::IsNearlyEqual(F.Unit->GetActorRotation().Yaw, PausedYaw + 18.0, 0.1));

	FCombatMotionRequest Motion;
	Motion.TargetLocation = FVector(40.0, 0.0, 0.0);
	Motion.Speed = 20.0f;
	const FCombatMotionResult Acquired = F.Unit->GetCombatMotionComponent()->TryAcquireMotion(Motion);
	TestTrue(TEXT("Forced motion starts"), Acquired.bSuccess);
	const double MotionYaw = F.Unit->GetActorRotation().Yaw;
	F.Advance(5);
	TestEqual(TEXT("Motion pauses the order"), F.Orders->GetCurrentState(), ECombatOrderState::Paused);
	TestTrue(TEXT("Motion has no leftover facing writer"), FMath::IsNearlyEqual(F.Unit->GetActorRotation().Yaw, MotionYaw));
	F.Unit->GetCombatMotionComponent()->ReleaseMotion(Acquired.Handle);
	F.Advance(30);
	TestEqual(TEXT("Rooted cast resumes and starts once after motion"), F.CountEvent(CombatTags::Event_Combat_AbilityCastStarted), 1);
	return true;
}

/** 验证动态目标重新追击、目标丢失、非法转速与转身超时；无目标施法不受转速影响。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatFacingTargetAndBoundaryTest,
	"Combat.OrderAttack.Facing.TargetAndBoundaryRules", CombatFacingTests::Flags)

bool FCombatFacingTargetAndBoundaryTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	CombatFacingTests::FFixture F;
	if (!F.Ability.IsValid()) { AddError(TEXT("Facing fixture setup failed")); return false; }
	for (const double Rate : {0.0, -1.0, std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity()})
	{
		F.Movement->RotationRate.Yaw = Rate;
		F.Orders->IssueOrder(F.PointRequest(), false);
		TestEqual(TEXT("Invalid turn rate cannot hang a facing order"), F.Orders->GetCurrentState(), ECombatOrderState::Idle);
		TestFalse(TEXT("Invalid turn rate reports failure"), F.Finished.Last().bSuccess);
	}
	F.Movement->RotationRate.Yaw = 1.0;
	F.Orders->MaxChaseDuration = 0.1f;
	F.Orders->IssueOrder(F.PointRequest(), false);
	F.Advance(8);
	TestEqual(TEXT("Facing times out through the normal result path"), F.Orders->GetCurrentState(), ECombatOrderState::Idle);
	TestEqual(TEXT("Each invalid/timeout attempt ends exactly once"), F.Finished.Num(), 5);
	F.Orders->MaxChaseDuration = 10.0f;
	F.Movement->RotationRate.Yaw = 90.0;
	F.Data->BehaviorTags.Reset();
	F.Data->BehaviorTags.AddTag(CombatTags::Ability_Behavior_UnitTarget);
	ACombatUnitCharacter* Target = F.SpawnUnit(TEXT("facing_target"), FVector(0.0, 300.0, 0.0), 2);
	if (!Target) { return false; }
	FCombatOrderRequest Request;
	Request.Type = ECombatOrderType::CastTarget;
	Request.TargetUnit = Target;
	Request.AbilitySpecHandle = F.Ability;
	F.Orders->SetNavigationDeferredForTesting(true);
	const FCombatOrderResult Cast = F.Orders->IssueOrder(Request, false);
	F.Advance(5);
	Target->SetActorLocation(FVector(-300.0, 0.0, 0.0));
	F.Advance(5);
	TestEqual(TEXT("Changed direction remains a facing wait"), F.Orders->GetCurrentState(), ECombatOrderState::Facing);
	Target->SetActorLocation(FVector(-1000.0, 0.0, 0.0));
	F.Advance(3);
	TestEqual(TEXT("Out-of-range target restarts chase"), F.Orders->GetCurrentState(), ECombatOrderState::Chasing);
	TestEqual(TEXT("Chase preserves the accepted order"), F.Orders->GetCurrentOrderHandle(), Cast.Handle);
	Target->SetActorLocation(FVector(0.0, 300.0, 0.0));
	F.Orders->IssueOrder(Request, false);
	Target->GetCombatLifecycleComponent()->RequestDeath(F.World->GetSubsystem<UCombatEventSubsystem>()->CreateRootEvent(), F.Unit);
	F.Advance(3);
	TestEqual(TEXT("Lost target fails before cast point"), F.Orders->GetCurrentState(), ECombatOrderState::Idle);
	TestEqual(TEXT("Failed facing never begins ability"), F.CountEvent(CombatTags::Event_Combat_AbilityCastStarted), 0);
	F.Data->TargetingRules.bAllowDead = true;
	F.Movement->RotationRate.Yaw = 180.0;
	F.Orders->IssueOrder(Request, false);
	TestEqual(TEXT("Explicit dead-target policy still permits facing"), F.Orders->GetCurrentState(), ECombatOrderState::Facing);
	F.Advance(40);
	TestTrue(TEXT("Dead-target skill finishes through shared target rules"), F.Finished.Last().bSuccess);
	TestEqual(TEXT("Allowed dead target starts the spell once"), F.CountEvent(CombatTags::Event_Combat_AbilitySpellStarted), 1);

	F.Data->BehaviorTags.Reset();
	F.Data->BehaviorTags.AddTag(CombatTags::Ability_Behavior_NoTarget);
	F.Data->TargetingRules.TargetTeamTag = CombatTags::TargetTeam_None;
	F.Movement->RotationRate.Yaw = 0.0;
	const double Yaw = F.Unit->GetActorRotation().Yaw;
	FCombatOrderRequest NoTarget;
	NoTarget.Type = ECombatOrderType::CastNoTarget;
	NoTarget.AbilitySpecHandle = F.Ability;
	F.Orders->IssueOrder(NoTarget, false);
	TestEqual(TEXT("No-target cast skips facing"), F.Orders->GetCurrentState(), ECombatOrderState::WaitingOrderRelease);
	TestTrue(TEXT("No-target cast preserves orientation"), FMath::IsNearlyEqual(F.Unit->GetActorRotation().Yaw, Yaw));
	return true;
}

/** 验证普通攻击使用同一转速，并覆盖死亡、复活和 Owner EndPlay 对转身复核任务的清理。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatFacingAttackAndTeardownTest,
	"Combat.OrderAttack.Facing.AttackAndTeardown", CombatFacingTests::Flags)

bool FCombatFacingAttackAndTeardownTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	CombatFacingTests::FFixture F;
	if (!F.Ability.IsValid()) { AddError(TEXT("Facing fixture setup failed")); return false; }
	ACombatUnitCharacter* Target = F.SpawnUnit(TEXT("facing_attack_target"), FVector(0.0, 120.0, 0.0), 2);
	if (!Target) { return false; }
	FCombatOrderRequest Attack;
	Attack.Type = ECombatOrderType::AttackTarget;
	Attack.TargetUnit = Target;
	F.Orders->IssueOrder(Attack, false);
	F.Advance(5);
	TestTrue(TEXT("Attack facing obeys the same yaw rate"), FMath::IsNearlyEqual(F.Unit->GetActorRotation().Yaw, 18.0, 0.1));
	TestEqual(TEXT("No AttackRecord before facing tolerance"), F.Unit->GetCombatAttackComponent()->GetActiveAttackCount(), 0);
	F.Advance(17);
	TestTrue(TEXT("Attack starts inside its 15 degree tolerance"), F.Unit->GetCombatAttackComponent()->GetActiveAttackCount() > 0);
	F.Orders->StopAllOrders(CombatTags::Order_Failure_Cancelled);
	F.Unit->SetActorRotation(FRotator::ZeroRotator);
	const FCombatOrderResult Cast = F.Orders->IssueOrder(F.PointRequest(), false);
	const uint32 Life = F.Unit->GetLifeGeneration();
	F.Unit->GetCombatLifecycleComponent()->RequestDeath(F.World->GetSubsystem<UCombatEventSubsystem>()->CreateRootEvent(), Target);
	F.Unit->GetCombatLifecycleComponent()->RespawnAtLocation(FVector::ZeroVector);
	F.Advance(5);
	TestTrue(TEXT("Respawn advances life"), F.Unit->GetLifeGeneration() != Life);
	TestEqual(TEXT("Old life has no active order"), F.Orders->GetCurrentState(), ECombatOrderState::Idle);
	TestEqual(TEXT("Old life callbacks never activate the spell"), F.CountEvent(CombatTags::Event_Combat_AbilityCastStarted), 0);
	TestEqual(TEXT("Death emits one result for the facing cast"), F.Finished.FilterByPredicate(
		[&Cast](const FCombatOrderResult& Result) { return Result.Handle == Cast.Handle; }).Num(), 1);
	F.Unit->SetActorRotation(FRotator::ZeroRotator);
	F.Orders->IssueOrder(F.PointRequest(), false);
	F.Unit->RouteEndPlay(EEndPlayReason::Destroyed);
	TestEqual(TEXT("Owner EndPlay removes every facing schedule"),
		F.World->GetSubsystem<UCombatSchedulerSubsystem>()->CancelAllForOwner(F.Orders), 0);
	TestEqual(TEXT("Owner EndPlay leaves no active order"), F.Orders->GetCurrentState(), ECombatOrderState::Idle);
	return true;
}

#endif
