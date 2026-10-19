#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Combat/AI/CombatAIProfileData.h"
#include "Combat/AI/CombatAIBrainComponent.h"
#include "Combat/AI/CombatAIStateTreeSchema.h"
#include "Combat/AI/CombatAIRoleTasks.h"
#include "Combat/Ability/CombatAbilitySystemComponent.h"
#include "Combat/Core/CombatTags.h"
#include "Combat/Combat/CombatDamageSubsystem.h"
#include "Combat/Attributes/CombatAttributeSet.h"
#include "Combat/Log/CombatEventSubsystem.h"
#include "Combat/Order/CombatOrderComponent.h"
#include "Combat/Scheduling/CombatSchedulerSubsystem.h"
#include "Combat/Tests/CombatAutomationWorldFixture.h"
#include "Combat/Unit/CombatUnitCharacter.h"
#include "Combat/Unit/CombatUnitLifecycleComponent.h"
#include "Combat/Validation/CombatAIAssetBuilder.h"
#include "Components/BoxComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "StateTree.h"
#include "StateTreeCompiler.h"
#include "StateTreeCompilerLog.h"
#include "StateTreeEditorData.h"
#include "StateTreeState.h"

namespace CombatAIRoleTests
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;
	/** 生成无重力、长生命单位，测试仅编排空间和异步回执，不替代生产移动。 */
	ACombatUnitCharacter* Spawn(UWorld& World, FVector Location = FVector::ZeroVector, uint8 Team = 1, float MaxHealth = 10000)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		auto* Unit = World.SpawnActor<ACombatUnitCharacter>(Location, FRotator::ZeroRotator, Params);
		auto* Data = NewObject<UCombatUnitData>(Unit);
		Data->DefinitionName = TEXT("ai_role_fixture_unit");
		Data->BaseStats.MaxHealth = MaxHealth;
		Data->InitialTeamId = FCombatTeamId(Team);
		Unit->InitializeFromUnitData(Data);
		Unit->GetCharacterMovement()->GravityScale = 0;
		Unit->GetCombatOrderComponent()->SetNavigationDeferredForTesting(true);
		return Unit;
	}
	/** 使用真实角色节点和链接资产，测试不是直接调用角色分支的替代执行器。 */
	UCombatAIProfileData* Profile(UObject* Outer, bool bRoute = false, bool bPassive = false)
	{
		auto* Result = NewObject<UCombatAIProfileData>(Outer);
		Result->DefinitionName = TEXT("ai_role_fixture");
		Result->bEnablePerception = true;
		Result->bReturnAfterCombat = !bRoute;
		Result->Perception.IdleInterval = Result->Perception.ActiveInterval = 0.05f;
		Result->Perception.bRequireLineOfSight = false;
		Result->Perception.MemorySeconds = 0.3f;
		if (bPassive)
		{
			auto* Tree = NewObject<UStateTree>(Outer);
			auto* Data = NewObject<UStateTreeEditorData>(Tree);
			Tree->EditorData = Data;
			Data->Schema = NewObject<UCombatAIStateTreeSchema>(Data);
			Data->AddRootState().AddTask<FCombatAIWaitTask>().GetInstanceData().WaitSeconds = 30;
			FStateTreeCompilerLog Log;
			FStateTreeCompiler Compiler(Log);
			Result->RootTree = Compiler.Compile(*Tree) ? Tree : nullptr;
		}
		else
		{
			auto* Home = FCombatAIAssetBuilder::BuildRoleActionTree(Outer, ECombatAIRoleOperation::Home, TEXT("HomeMove"));
			auto* Attack = FCombatAIAssetBuilder::BuildRoleActionTree(Outer, ECombatAIRoleOperation::Attack, TEXT("BasicAttack"));
			auto* Route = bRoute ? FCombatAIAssetBuilder::BuildRoleActionTree(Outer, ECombatAIRoleOperation::Route, TEXT("RouteMove")) : nullptr;
			Result->RootTree = FCombatAIAssetBuilder::BuildRoleRootTree(Outer, Home, Attack, Route, FCombatAIAssetBuilder::BuildGuardTree(Outer));
		}
		return Result;
	}
	/** 同步 Automation 补发被引擎调度唤醒的组件；真实帧调度另由 PIE/Dedicated 证明。 */
	void Advance(UWorld& World, int32 Frames = 4)
	{
		for (int32 Index = 0; Index < Frames; ++Index)
		{
			World.Tick(LEVELTICK_All, 0.05f);
			World.GetSubsystem<UCombatSchedulerSubsystem>()->RunDueTasks(World.GetTimeSeconds());
			for (TActorIterator<ACombatUnitCharacter> It(&World); It; ++It)
			{
				auto* Brain = It->GetCombatAIBrainComponent();
				if (Brain->IsRunning() && Brain->IsComponentTickEnabled()) Brain->TickComponent(0.05f, LEVELTICK_All, nullptr);
			}
		}
	}
	/** 通过事件中心发送已结算受伤观察，隔离知识策略，不修改 Health。 */
	void ObserveHit(ACombatUnitCharacter& Target, ACombatUnitCharacter& Source, float Amount = 12)
	{
		auto* Events = Target.GetWorld()->GetSubsystem<UCombatEventSubsystem>();
		FCombatLogRecord Record;
		Record.Context = Events->CreateRootEvent();
		Record.EventType = CombatTags::Event_Combat_DamageApplied;
		Record.SourceActorId = Source.GetUniqueID(); Record.TargetActorId = Target.GetUniqueID();
		Record.UnitLifeGeneration = Target.GetLifeGeneration(); Record.AppliedAmount = Amount;
		Events->Emit(Record);
	}
}

/** 阶段 B 必须交付可加载且真实编译的角色配置，不能仅以 C++ 示例替代资产。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatAIRoleAssetsTest, "Combat.AI.Roles.AuthoredAssets",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCombatAIRoleAssetsTest::RunTest(const FString& Parameters)
{
	for (const TCHAR* Name : { TEXT("DA_AI_NeutralCamp"), TEXT("DA_AI_Lane"), TEXT("DA_AI_Patrol") })
	{
		const FString Path = FString(TEXT("/Game/Combat/Demo/AI/Roles/")) + Name + TEXT(".") + Name;
		auto* Profile = LoadObject<UCombatAIProfileData>(nullptr, *Path);
		if (!TestNotNull(*Path, Profile)) continue;
		FString Diagnostic;
		TestTrue(*FString::Printf(TEXT("Role profile compiled and valid: %s %s"), Name, *Diagnostic), Profile->ValidateRuntime(Diagnostic));
	}
	return true;
}

/** 候选确定排序、保持和硬失效不受同分枚举顺序影响；非法 Profile 明确拒绝。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatAIRoleSelectionTest, "Combat.AI.Roles.SelectionAndValidation", CombatAIRoleTests::Flags)
bool FCombatAIRoleSelectionTest::RunTest(const FString& Parameters)
{
	FCombatAutomationWorldFixture Fixture;
	auto* A = CombatAIRoleTests::Spawn(*Fixture.GetWorld());
	auto* B = CombatAIRoleTests::Spawn(*Fixture.GetWorld(), FVector(200, 0, 0));
	FCombatAIKnownTarget First, Second;
	First.Unit = A; First.Life = 1; First.StableId = 1; First.Score = 10;
	Second.Unit = B; Second.Life = 1; Second.StableId = 2; Second.Score = 10;
	TArray<FCombatAIKnownTarget> Candidates{ Second, First };
	FCombatAITargetSelection::Sort(Candidates);
	TestEqual(TEXT("Stable tie uses identity"), Candidates[0].StableId, uint32(1));
	TestEqual(TEXT("Minimum hold retains current"), FCombatAITargetSelection::Select(Candidates, B, 1, 0.2, 1, 5), 1);
	TestEqual(TEXT("Margin prevents churn after hold"), FCombatAITargetSelection::Select(Candidates, B, 1, 2, 1, 5), 1);
	TestEqual(TEXT("Old life cannot retain selection"), FCombatAITargetSelection::Select(Candidates, B, 2, 0, 1, 5), 0);
	auto* Profile = CombatAIRoleTests::Profile(A);
	FString Diagnostic;
	TestTrue(TEXT("Role tree compiled and valid"), Profile->ValidateRuntime(Diagnostic));
	Profile->Perception.VisibilityPolicy = ECombatVisibilityPolicy::RequireVisible;
	TestFalse(TEXT("No omniscient fallback for missing vision provider"), Profile->ValidateRuntime(Diagnostic));
	Profile->Perception.VisibilityPolicy = ECombatVisibilityPolicy::None;
	Profile->RetryDelay = 0;
	TestFalse(TEXT("Zero delay recovery loop rejected"), Profile->ValidateRuntime(Diagnostic));
	FCombatAIAssignment Invalid;
	Invalid.Route.SetNum(65);
	TestFalse(TEXT("Unbounded route rejected"), A->GetCombatAIBrainComponent()->SetAssignment(Invalid));
	return true;
}

/** 范围/LOS 之外不更新知识，隐藏来源不加威胁，历史位置过期，重生重新观察不继承威胁。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatAIKnowledgeTest, "Combat.AI.Roles.KnowledgePermissionAndLifetime", CombatAIRoleTests::Flags)
bool FCombatAIKnowledgeTest::RunTest(const FString& Parameters)
{
	FCombatAutomationWorldFixture Fixture;
	auto& World = *Fixture.GetWorld();
	auto* Unit = CombatAIRoleTests::Spawn(World);
	auto* Target = CombatAIRoleTests::Spawn(World, FVector(300, 0, 0), 2);
	CombatAIRoleTests::Spawn(World, FVector(200, 150, 0), 1);
	auto* Brain = Unit->GetCombatAIBrainComponent();
	auto* Profile = CombatAIRoleTests::Profile(Unit, false, true);
	Profile->Perception.bRequireLineOfSight = true;
	Brain->ConfigureProfile(Profile);
	TestEqual(TEXT("Only public hostile candidate observed"), Brain->GetKnowledge().Candidates.Num(), 1);
	CombatAIRoleTests::ObserveHit(*Unit, *Target);
	TestEqual(TEXT("Visible damage contributes threat"), Brain->GetKnowledge().Memories[0].Threat, 12.0f);
	auto* Wall = World.SpawnActor<AActor>();
	auto* Box = NewObject<UBoxComponent>(Wall);
	Wall->SetRootComponent(Box);
	Box->SetBoxExtent(FVector(25, 200, 200));
	Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Box->SetCollisionResponseToAllChannels(ECR_Ignore);
	Box->SetCollisionResponseToChannel(ECC_GameTraceChannel4, ECR_Block);
	Box->RegisterComponent();
	Wall->SetActorLocation(FVector(150, 0, 0));
	CombatAIRoleTests::ObserveHit(*Unit, *Target);
	TestEqual(TEXT("Newly occluded attacker cannot reveal threat between samples"), Brain->GetKnowledge().Memories[0].Threat, 12.0f);
	CombatAIRoleTests::Advance(World, 2);
	TestEqual(TEXT("LOS removes current candidate"), Brain->GetKnowledge().Candidates.Num(), 0);
	Target->SetActorLocation(FVector(600, 0, 0));
	CombatAIRoleTests::Advance(World, 2);
	TestTrue(TEXT("Hidden movement leaves historical position unchanged"), Brain->GetKnowledge().Memories.Num() == 1
		&& Brain->GetKnowledge().Memories[0].LastSeenPosition.Equals(FVector(300, 0, 0), 1));
	CombatAIRoleTests::Advance(World, 8);
	TestEqual(TEXT("Expired knowledge removed"), Brain->GetKnowledge().Memories.Num(), 0);
	Wall->Destroy();
	CombatAIRoleTests::Advance(World, 2);
	const uint32 OldLife = Target->GetLifeGeneration();
	Target->GetCombatLifecycleComponent()->RequestDeath(World.GetSubsystem<UCombatEventSubsystem>()->CreateRootEvent(), Unit);
	Target->GetCombatLifecycleComponent()->RespawnAtLocation(FVector(300, 0, 0));
	CombatAIRoleTests::Advance(World, 3);
	TestTrue(TEXT("Newly observed life replaces old identity"), Brain->GetKnowledge().Candidates.Num() == 1
		&& Brain->GetKnowledge().Candidates[0].Life > OldLife && Brain->GetKnowledge().Candidates[0].Threat == 0);
	Unit->Destroy();
	TestFalse(TEXT("EndPlay cancels perception"), Brain->HasPerceptionSchedule());
	TestFalse(TEXT("EndPlay removes event subscription"), Brain->HasPerceptionSubscription());
	TestEqual(TEXT("EndPlay clears weak knowledge"), Brain->GetKnowledge().Memories.Num(), 0);
	return true;
}

/** 真正编译的野怪根树自动索敌，丢失后归位；归位中的普通感知和受击不能重新下攻击。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatAIGuardRoleTest, "Combat.AI.Roles.GuardAttackReturn", CombatAIRoleTests::Flags)
bool FCombatAIGuardRoleTest::RunTest(const FString& Parameters)
{
	FCombatAutomationWorldFixture Fixture;
	auto& World = *Fixture.GetWorld();
	auto* Unit = CombatAIRoleTests::Spawn(World);
	auto* Enemy = CombatAIRoleTests::Spawn(World, FVector(2000, 0, 0), 2);
	auto* Brain = Unit->GetCombatAIBrainComponent();
	auto* Orders = Unit->GetCombatOrderComponent();
	Brain->SetAssignment({});
	Brain->ConfigureProfile(CombatAIRoleTests::Profile(Unit));
	CombatAIRoleTests::Advance(World);
	TestEqual(TEXT("Guard waits without explicit objective"), Brain->GetSubmittedCount(), uint64(0));
	Enemy->SetActorLocation(FVector(400, 0, 0));
	CombatAIRoleTests::Advance(World, 8);
	TestEqual(TEXT("Automatic acquisition submits one attack"), Brain->GetSubmittedCount(), uint64(1));
	const auto Attack = Orders->GetCurrentOrderHandle();
	CombatAIRoleTests::Advance(World, 8);
	TestEqual(TEXT("Repeated sensing keeps original attack"), Orders->GetCurrentOrderHandle(), Attack);
	Unit->SetActorLocation(FVector(500, 0, 0));
	Enemy->SetActorLocation(FVector(2200, 0, 0));
	CombatAIRoleTests::Advance(World, 8);
	TestTrue(TEXT("Lost perception requests return"), Brain->IsReturnRequested());
	TestEqual(TEXT("One return command"), Brain->GetSubmittedCount(), uint64(2));
	const auto Returning = Orders->GetCurrentOrderHandle();
	Enemy->SetActorLocation(FVector(650, 0, 0));
	CombatAIRoleTests::Advance(World, 3);
	CombatAIRoleTests::ObserveHit(*Unit, *Enemy);
	CombatAIRoleTests::Advance(World, 4);
	TestEqual(TEXT("New enemies and damage preserve exact return order"), Orders->GetCurrentOrderHandle(), Returning);
	Unit->SetActorLocation(FVector::ZeroVector);
	Orders->CompleteMovementForTesting(Returning, true);
	CombatAIRoleTests::Advance(World, 1);
	TestEqual(TEXT("Home arrival committed exactly once"), Brain->GetHomeCommitCount(), uint64(1));
	TestFalse(TEXT("Return request cleared only on arrival"), Brain->IsReturnRequested());
	Brain->SuspendForManualCommand();
	TestFalse(TEXT("Takeover stops role tree"), Brain->IsRunning());
	TestFalse(TEXT("Takeover clears sensor"), Brain->HasPerceptionSchedule());
	TestFalse(TEXT("Takeover clears scope"), Brain->GetDecisionScope().IsValid());
	return true;
}

/** 交战取消不推进航点，目标结束后恢复原航点，最后一点成功后不重复发原地 Move。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatAILaneRoleTest, "Combat.AI.Roles.LaneResumeAndPatrol", CombatAIRoleTests::Flags)
bool FCombatAILaneRoleTest::RunTest(const FString& Parameters)
{
	FCombatAutomationWorldFixture Fixture;
	auto& World = *Fixture.GetWorld();
	auto* Unit = CombatAIRoleTests::Spawn(World);
	auto* Enemy = CombatAIRoleTests::Spawn(World, FVector(3000, 0, 0), 2);
	auto* Brain = Unit->GetCombatAIBrainComponent();
	auto* Orders = Unit->GetCombatOrderComponent();
	FCombatAIAssignment Assignment;
	Assignment.Route = { FVector(1000, 0, 0), FVector(1800, 0, 0) };
	Brain->SetAssignment(Assignment);
	Brain->ConfigureProfile(CombatAIRoleTests::Profile(Unit, true));
	CombatAIRoleTests::Advance(World);
	const auto FirstMove = Orders->GetCurrentOrderHandle();
	Enemy->SetActorLocation(FVector(300, 0, 0));
	CombatAIRoleTests::Advance(World, 12);
	TestEqual(TEXT("Combat interruption does not advance cursor"), Brain->GetRouteCursor(), 0);
	TestTrue(TEXT("Lane changed from move to attack"), Brain->GetSubmittedCount() == 2 && !(Orders->GetCurrentOrderHandle() == FirstMove));
	Enemy->GetCombatLifecycleComponent()->RequestDeath(World.GetSubsystem<UCombatEventSubsystem>()->CreateRootEvent(), Unit);
	CombatAIRoleTests::Advance(World, 8);
	TestEqual(TEXT("After combat original waypoint resumes"), Orders->GetCurrentMoveGoal(), Assignment.Route[0]);
	TestEqual(TEXT("Resume preserves cursor"), Brain->GetRouteCursor(), 0);
	Unit->SetActorLocation(Assignment.Route[0]);
	Orders->CompleteMovementForTesting(Orders->GetCurrentOrderHandle(), true);
	CombatAIRoleTests::Advance(World, 8);
	TestEqual(TEXT("Successful arrival advances once"), Brain->GetRouteCursor(), 1);
	TestFalse(TEXT("Old movement callback rejected"), Orders->CompleteMovementForTesting(FirstMove, true));
	Unit->SetActorLocation(Assignment.Route[1]);
	Orders->CompleteMovementForTesting(Orders->GetCurrentOrderHandle(), true);
	CombatAIRoleTests::Advance(World, 8);
	TestEqual(TEXT("Final point completes route"), Brain->GetRouteCursor(), 2);
	const uint64 FinishedCount = Brain->GetSubmittedCount();
	CombatAIRoleTests::Advance(World, 20);
	TestEqual(TEXT("Finished route remains idle"), Brain->GetSubmittedCount(), FinishedCount);
	Assignment.Home = Unit->GetActorLocation(); Assignment.Route = { Unit->GetActorLocation() }; Assignment.bLoopRoute = true;
	Brain->SetAssignment(Assignment);
	CombatAIRoleTests::Advance(World, 14);
	TestEqual(TEXT("Patrol wraps confirmed point"), Brain->GetRouteCursor(), 0);
	TestTrue(TEXT("Coincident patrol points are bounded by Scheduler pause"), Brain->GetSubmittedCount() > FinishedCount && Brain->GetSubmittedCount() < FinishedCount + 8);
	return true;
}

/** 路径失败计数不被感知刷新重置；三次后等待新职责，旧定时器不能解锁新控制者。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatAIRoleRetryTest, "Combat.AI.Roles.BoundedRecovery", CombatAIRoleTests::Flags)
bool FCombatAIRoleRetryTest::RunTest(const FString& Parameters)
{
	FCombatAutomationWorldFixture Fixture;
	auto& World = *Fixture.GetWorld();
	auto* Unit = CombatAIRoleTests::Spawn(World);
	auto* Brain = Unit->GetCombatAIBrainComponent();
	auto* Orders = Unit->GetCombatOrderComponent();
	Orders->MaxMoveRetries = 0;
	FCombatAIAssignment Assignment;
	Assignment.Home = FVector(2000, 0, 0);
	Brain->SetAssignment(Assignment);
	Brain->ConfigureProfile(CombatAIRoleTests::Profile(Unit));
	CombatAIRoleTests::Advance(World, 3);
	for (int32 Attempt = 1; Attempt <= 3; ++Attempt)
	{
		TestTrue(TEXT("Attempt has live home move"), Orders->GetCurrentOrderHandle().IsValid());
		Orders->CompleteMovementForTesting(Orders->GetCurrentOrderHandle(), false);
		CombatAIRoleTests::Advance(World, 3);
		TestEqual(TEXT("One receipt counts one attempt"), Brain->GetFailureCount(ECombatAIRoleOperation::Home), Attempt);
		TestEqual(TEXT("Sensing cannot bypass backoff"), Brain->GetSubmittedCount(), uint64(Attempt));
		CombatAIRoleTests::Advance(World, 12);
	}
	TestTrue(TEXT("Three attempts enter blocked wait"), Brain->IsRetryBlocked());
	const uint64 Count = Brain->GetSubmittedCount();
	CombatAIRoleTests::Advance(World, 35);
	TestEqual(TEXT("Ordinary snapshots never restart exhausted operation"), Brain->GetSubmittedCount(), Count);
	Brain->SetAssignment(Assignment);
	CombatAIRoleTests::Advance(World, 4);
	TestEqual(TEXT("New valid assignment explicitly retries"), Brain->GetSubmittedCount(), Count + 1);
	const uint64 OldRun = Brain->GetRunSerial();
	Unit->GetCombatLifecycleComponent()->RequestDeath(World.GetSubsystem<UCombatEventSubsystem>()->CreateRootEvent(), nullptr);
	TestFalse(TEXT("Death stops perception immediately"), Brain->HasPerceptionSchedule());
	TestEqual(TEXT("Death clears scheduler waits"), Brain->GetActiveWaitCount(), 0);
	Unit->GetCombatLifecycleComponent()->RespawnAtLocation(FVector::ZeroVector);
	CombatAIRoleTests::Advance(World, 4);
	TestTrue(TEXT("Respawn restores duty in fresh run"), Brain->IsRunning() && Brain->GetRunSerial() > OldRun);
	TestEqual(TEXT("Old life retry failures cleared"), Brain->GetFailureCount(ECombatAIRoleOperation::Home), 0);
	return true;
}
/** 针对候选预算和巡逻回绕的对抗用例，防止低优先级输入被误当成职责中断。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatAIRoleEdgesTest, "Combat.AI.Roles.CandidateBudgetAndPatrolAnchor", CombatAIRoleTests::Flags)
bool FCombatAIRoleEdgesTest::RunTest(const FString& Parameters)
{
	FCombatAutomationWorldFixture Fixture;
	auto& World = *Fixture.GetWorld();
	auto* Unit = CombatAIRoleTests::Spawn(World);
	CombatAIRoleTests::Spawn(World, FVector(400, 0, 0), 2);
	auto* Brain = Unit->GetCombatAIBrainComponent();
	auto* Orders = Unit->GetCombatOrderComponent();
	auto* Profile = CombatAIRoleTests::Profile(Unit);
	Profile->Perception.MaxCandidates = 1;
	Brain->SetAssignment({}); Brain->ConfigureProfile(Profile);
	CombatAIRoleTests::Advance(World, 4);
	const auto Attack = Orders->GetCurrentOrderHandle();
	auto* Closer = CombatAIRoleTests::Spawn(World, FVector(150, 0, 0), 2);
	CombatAIRoleTests::Advance(World, 24);
	TestEqual(TEXT("Candidate cap cannot evict still-visible active target"), Orders->GetCurrentOrderHandle(), Attack);
	TestEqual(TEXT("Ordinary closer target does not restart combat"), Brain->GetSubmittedCount(), uint64(1));
	Closer->Destroy();
	for (TActorIterator<ACombatUnitCharacter> It(&World); It; ++It) if (*It != Unit) It->Destroy();
	FCombatAIAssignment Duty;
	Duty.Route = { FVector(1000, 0, 0), FVector(2000, 0, 0) }; Duty.bLoopRoute = true;
	Brain->SetAssignment(Duty); Brain->ConfigureProfile(CombatAIRoleTests::Profile(Unit, true));
	CombatAIRoleTests::Advance(World, 4);
	for (const FVector& Point : Duty.Route)
	{
		Unit->SetActorLocation(Point);
		Orders->CompleteMovementForTesting(Orders->GetCurrentOrderHandle(), true);
		CombatAIRoleTests::Advance(World, 7);
	}
	TestEqual(TEXT("Patrol wrap keeps last reached anchor and moves toward first point"), Orders->GetCurrentMoveGoal(), Duty.Route[0]);
	TestFalse(TEXT("Wrapped patrol must not spuriously return to original spawn"), Brain->NeedsReturn());
	Profile = CombatAIRoleTests::Profile(Unit);
	Profile->bEnablePerception = false;
	FString Diagnostic;
	TestFalse(TEXT("Role tree without required perception rejected"), Profile->ValidateRuntime(Diagnostic));
	return true;
}
/** 完成回执与新职责同轮时不提交旧航点；导航成功后离开到达容差也不能虚报归位。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatAIRoleReceiptEdgesTest, "Combat.AI.Roles.ArrivalAndRevisionArbitration", CombatAIRoleTests::Flags)
bool FCombatAIRoleReceiptEdgesTest::RunTest(const FString& Parameters)
{
	FCombatAutomationWorldFixture Fixture;
	auto& World = *Fixture.GetWorld();
	auto* Unit = CombatAIRoleTests::Spawn(World);
	auto* Brain = Unit->GetCombatAIBrainComponent();
	auto* Orders = Unit->GetCombatOrderComponent();
	FCombatAIAssignment First; First.Route = { FVector(1000, 0, 0) };
	FCombatAIAssignment Replaced; Replaced.Route = { FVector(2000, 0, 0) };
	Brain->SetAssignment(First); Brain->ConfigureProfile(CombatAIRoleTests::Profile(Unit, true));
	CombatAIRoleTests::Advance(World);
	const auto Move = Orders->GetCurrentOrderHandle();
	bool bReplaced = false;
	const auto Binding = Orders->OnOrderFinished().AddLambda([&](const FCombatOrderResult& Result)
	{
		if (!bReplaced && Result.Handle == Move && Result.bSuccess) { bReplaced = true; Brain->SetAssignment(Replaced); }
	});
	Unit->SetActorLocation(First.Route[0]); Orders->CompleteMovementForTesting(Move, true);
	CombatAIRoleTests::Advance(World, 8);
	TestEqual(TEXT("Old completion cannot advance new assignment"), Brain->GetRouteCommitCount(), uint64(0));
	TestEqual(TEXT("New route begins at its first point"), Orders->GetCurrentMoveGoal(), Replaced.Route[0]);
	Orders->OnOrderFinished().Remove(Binding);
	Brain->SetAssignment({}); Brain->ConfigureProfile(CombatAIRoleTests::Profile(Unit));
	Unit->SetActorLocation(FVector(2000, 0, 0));
	CombatAIRoleTests::Advance(World, 4);
	Unit->SetActorLocation(FVector::ZeroVector);
	Orders->CompleteMovementForTesting(Orders->GetCurrentOrderHandle(), true);
	Unit->SetActorLocation(FVector(300, 0, 0));
	CombatAIRoleTests::Advance(World, 1);
	TestEqual(TEXT("Successful navigation receipt still requires actual arrival"), Brain->GetHomeCommitCount(), uint64(0));
	TestEqual(TEXT("Arrival mismatch is one bounded failure"), Brain->GetFailureCount(ECombatAIRoleOperation::Home), 1);
	return true;
}
/** 默认不观察生命；显式启用后按比例排序，隐藏目标不泄露变化，实际攻击也消费同一选择。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatAILowHealthTest, "Combat.AI.Roles.LowHealthSelectionAndPrivacy", CombatAIRoleTests::Flags)
bool FCombatAILowHealthTest::RunTest(const FString&)
{
	FCombatAutomationWorldFixture Fixture; auto& W=*Fixture.GetWorld();
	auto* Unit=CombatAIRoleTests::Spawn(W);
	auto* Near=CombatAIRoleTests::Spawn(W,FVector(200,200,0),2);
	auto* Low=CombatAIRoleTests::Spawn(W,FVector(500,0,0),2,20000);
	FCombatDamageRequest Damage; Damage.Source=Unit; Damage.Target=Near; Damage.Amount=5000; Damage.DamageType=ECombatDamageType::Pure;
	TestTrue(TEXT("Wound near target through public Damage"),W.GetSubsystem<UCombatDamageSubsystem>()->DealDamage(Damage).bSuccess);
	Damage.Target=Low; Damage.Amount=14000; TestTrue(TEXT("Wound high maximum-health target"),W.GetSubsystem<UCombatDamageSubsystem>()->DealDamage(Damage).bSuccess);
	TestTrue(TEXT("Selected target has MORE absolute health"),Low->GetCombatAttributeSet()->GetHealth()>Near->GetCombatAttributeSet()->GetHealth());
	auto* Brain=Unit->GetCombatAIBrainComponent(); auto* P=CombatAIRoleTests::Profile(Unit,false,true);
	Brain->SetAssignment({}); Brain->ConfigureProfile(P);
	if(!TestEqual(TEXT("Two visible enemies"),Brain->GetKnowledge().Candidates.Num(),2)) return false;
	TestTrue(TEXT("Default zero preserves nearest selection"),Brain->GetKnowledge().Candidates[0].Unit==Near);
	P->MissingHealthWeight=100; P->DistanceWeight=P->ThreatWeight=0; P->Perception.MemorySeconds=5;
	Brain->ConfigureProfile(P);
	TestTrue(TEXT("Lower health ratio selected despite distance"),Brain->GetKnowledge().Candidates[0].Unit==Low);
	TestTrue(TEXT("Actual ASC ratio gives seventy points"),FMath::IsNearlyEqual(Brain->GetKnowledge().Candidates[0].Score,70.f,.01f));
	P->Perception.bRequireLineOfSight=true;
	auto* Wall=W.SpawnActor<AActor>(); auto* Box=NewObject<UBoxComponent>(Wall); Wall->SetRootComponent(Box);
	Box->SetBoxExtent(FVector(25,70,200)); Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Box->SetCollisionResponseToAllChannels(ECR_Ignore); Box->SetCollisionResponseToChannel(ECC_GameTraceChannel4,ECR_Block);
	Box->RegisterComponent(); Wall->SetActorLocation(FVector(250,0,0));
	Damage.Amount=1000; W.GetSubsystem<UCombatDamageSubsystem>()->DealDamage(Damage); CombatAIRoleTests::Advance(W,2);
	TestEqual(TEXT("Occluded low-health target excluded"),Brain->GetKnowledge().Candidates.Num(),1);
	const auto* Memory=Brain->GetKnowledge().Memories.FindByPredicate([&](const auto& M){return M.Unit==Low;});
	TestTrue(TEXT("Hidden damage does not update memory score"),Memory&&!Memory->bVisible&&FMath::IsNearlyEqual(Memory->Score,70.f,.01f));
	Wall->Destroy();
	auto* Active=CombatAIRoleTests::Profile(Unit,true); Active->MissingHealthWeight=100; Active->DistanceWeight=Active->ThreatWeight=0;
	Brain->ConfigureProfile(Active); CombatAIRoleTests::Advance(W,8);
	TestTrue(TEXT("Compiled role tree submits attack"),Brain->GetSubmittedCount()>0);
	TestTrue(TEXT("Actual attack pursues wounded enemy"),Unit->GetCombatOrderComponent()->GetCurrentMoveGoal().Equals(Low->GetActorLocation(),1));
	const uint32 OldLife=Low->GetLifeGeneration(); Damage.Amount=100000; W.GetSubsystem<UCombatDamageSubsystem>()->DealDamage(Damage);
	CombatAIRoleTests::Advance(W,6);
	TestTrue(TEXT("Dead target removed"),!Brain->GetKnowledge().Candidates.ContainsByPredicate([&](const auto& M){return M.Unit==Low;}));
	Low->GetCombatLifecycleComponent()->RespawnAtLocation(FVector(500,0,0)); CombatAIRoleTests::Advance(W,4);
	const auto* Reborn=Brain->GetKnowledge().Candidates.FindByPredicate([&](const auto& M){return M.Unit==Low;});
	TestTrue(TEXT("New life reobserved at full health"),Reborn&&Reborn->Life>OldLife&&FMath::IsNearlyZero(Reborn->Score));
	FString Error; Active->MissingHealthWeight=-1; TestFalse(TEXT("Negative weight rejected"),Active->ValidateRuntime(Error));
	Active->MissingHealthWeight=std::numeric_limits<float>::infinity(); TestFalse(TEXT("Nonfinite weight rejected"),Active->ValidateRuntime(Error));
	Unit->Destroy(); TestFalse(TEXT("Sensor schedule canceled"),Brain->HasPerceptionSchedule());
	TestFalse(TEXT("Observer canceled"),Brain->HasPerceptionSubscription()); TestEqual(TEXT("Knowledge cleared"),Brain->GetKnowledge().Memories.Num(),0);
	return true;
}
/** 巡线只响应高优先级定义，完成原路线后允许核心类低优先级敌人，防止反复攻核归位。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatAIRoutePriorityTest,"Combat.AI.Roles.RoutePriorityAndFallback",CombatAIRoleTests::Flags)
bool FCombatAIRoutePriorityTest::RunTest(const FString&)
{
	FCombatAutomationWorldFixture F; auto& W=*F.GetWorld();
	auto* Unit=CombatAIRoleTests::Spawn(W); auto* Enemy=CombatAIRoleTests::Spawn(W,FVector(500,0,0),2);
	auto* Brain=Unit->GetCombatAIBrainComponent(); auto* Orders=Unit->GetCombatOrderComponent();
	FCombatAIAssignment Duty; Duty.Route={FVector(200,100,0)}; Brain->SetAssignment(Duty);
	auto* P=CombatAIRoleTests::Profile(Unit,true); P->RouteEngagementMinPriority=10;
	Brain->ConfigureProfile(P); CombatAIRoleTests::Advance(W,4);
	TestEqual(TEXT("Low priority cannot interrupt route"),Brain->GetKnowledge().Candidates.Num(),0);
	TestTrue(TEXT("Actual move follows assigned waypoint"),Orders->GetCurrentMoveGoal().Equals(Duty.Route[0],1));
	Unit->SetActorLocation(Duty.Route[0]); TestTrue(TEXT("Fixture confirms genuine issued move"),Orders->CompleteMovementForTesting(Orders->GetCurrentOrderHandle(),true));
	CombatAIRoleTests::Advance(W,10);
	TestEqual(TEXT("Route fully committed"),Brain->GetRouteCursor(),1);
	TestTrue(TEXT("Finished route admits fallback enemy"),Brain->GetKnowledge().Candidates.Num()==1&&Brain->GetKnowledge().Candidates[0].Unit==Enemy);
	TestTrue(TEXT("Fallback attack pursues enemy"),Orders->GetCurrentMoveGoal().Equals(Enemy->GetActorLocation(),1));
	Brain->SuspendForManualCommand(); Brain->SetAssignment(Duty);
	FCombatAITargetPriority Priority; Priority.Definition=Enemy->GetUnitDefinitionId(); Priority.Priority=10; P->TargetPriorities.Add(Priority);
	Brain->ConfigureProfile(P); Brain->ResumeAutonomous(); CombatAIRoleTests::Advance(W,5);
	TestTrue(TEXT("High priority interrupts unfinished route"),Brain->GetRouteCursor()==0&&Brain->GetKnowledge().Candidates.Num()==1);
	FString Error; P->RouteEngagementMinPriority=101; TestFalse(TEXT("Unsupported priority rejected"),P->ValidateRuntime(Error));
	return true;
}
/** 交战期限确实取消原攻击并归位；旧移动回调和新可见敌人都不能夺走归位命令。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatAIEngagementDeadlineTest,"Combat.AI.Roles.EngagementDeadlineAndCleanup",CombatAIRoleTests::Flags)
bool FCombatAIEngagementDeadlineTest::RunTest(const FString&)
{
	FCombatAutomationWorldFixture F; auto& W=*F.GetWorld();
	auto* Unit=CombatAIRoleTests::Spawn(W,FVector(200,0,0)); CombatAIRoleTests::Spawn(W,FVector(600,0,0),2);
	auto* Brain=Unit->GetCombatAIBrainComponent(); auto* Orders=Unit->GetCombatOrderComponent();
	auto* P=CombatAIRoleTests::Profile(Unit,true); P->MaxEngagementSeconds=1;
	Brain->SetAssignment({}); Brain->ConfigureProfile(P); CombatAIRoleTests::Advance(W,4);
	const auto Old=Orders->GetCurrentOrderHandle(); TestTrue(TEXT("Attack began"),Old.IsValid());
	CombatAIRoleTests::Advance(W,24);
	TestTrue(TEXT("Deadline forces return even with visible enemy"),Brain->IsReturnRequested());
	const auto Home=Orders->GetCurrentOrderHandle(); TestTrue(TEXT("Return replaces exact old attack"),Home.IsValid()&&!(Home==Old));
	TestTrue(TEXT("Return uses duty anchor"),Orders->GetCurrentMoveGoal().Equals(FVector::ZeroVector,1));
	TestFalse(TEXT("Stale attack movement callback rejected"),Orders->CompleteMovementForTesting(Old,true));
	CombatAIRoleTests::Advance(W,5); TestEqual(TEXT("Fresh observations preserve return order"),Orders->GetCurrentOrderHandle(),Home);
	Unit->Destroy(); TestFalse(TEXT("Destroy cancels perception"),Brain->HasPerceptionSchedule());
	TestFalse(TEXT("Destroy removes delegate"),Brain->HasPerceptionSubscription());
	TestFalse(TEXT("Destroy invalidates role scope"),Brain->GetDecisionScope().IsValid()); return true;
}
#endif
