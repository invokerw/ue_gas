#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Combat/AI/CombatAIBrainComponent.h"
#include "Combat/AI/CombatAIProfileData.h"
#include "Combat/Ability/CombatGameplayAbility.h"
#include "Combat/Ability/CombatAbilitySystemComponent.h"
#include "Combat/Attack/CombatAttackComponent.h"
#include "Combat/AI/CombatAIStateTreeSchema.h"
#include "Combat/AI/CombatAIStateTreeTasks.h"
#include "Combat/Demo/CombatDemoAbilities.h"
#include "StateTree.h"
#include "StateTreeCompiler.h"
#include "StateTreeCompilerLog.h"
#include "StateTreeEditorData.h"
#include "StateTreeState.h"
#include "Combat/Validation/CombatAIAssetBuilder.h"
#include "Combat/Core/CombatTags.h"
#include "Combat/Log/CombatEventSubsystem.h"
#include "Combat/Order/CombatOrderComponent.h"
#include "Combat/Scheduling/CombatSchedulerSubsystem.h"
#include "Combat/Tests/CombatAutomationWorldFixture.h"
#include "Combat/Unit/CombatUnitCharacter.h"
#include "Combat/Unit/CombatUnitAIController.h"
#include "Combat/Unit/CombatUnitLifecycleComponent.h"

namespace CombatAIStateTreeTests
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;
	/** 构造真实编译的根树和链接执行资产，所有测试共用生产节点。 */
	UCombatAIProfileData* Profile(UObject* Outer)
	{
		auto* Profile = NewObject<UCombatAIProfileData>(Outer);
		Profile->DefinitionName = TEXT("ai_fixture");
		Profile->RootTree = FCombatAIAssetBuilder::BuildRootTree(Outer, FCombatAIAssetBuilder::BuildActionTree(Outer));
		return Profile;
	}
	/** 单独运行一个长等待，供工作区协议测试主动调用节点 API，不与根树的 Scope 抢占。 */
	UCombatAIProfileData* PassiveProfile(UObject* Outer)
	{
		auto* Result = NewObject<UCombatAIProfileData>(Outer);
		Result->DefinitionName = TEXT("ai_protocol_fixture");
		auto* Tree = NewObject<UStateTree>(Outer);
		auto* Data = NewObject<UStateTreeEditorData>(Tree);
		Tree->EditorData = Data;
		Data->Schema = NewObject<UCombatAIStateTreeSchema>(Data);
		auto& Task = Data->AddRootState().AddTask<FCombatAIWaitTask>();
		Task.GetInstanceData().WaitSeconds = 30;
		FStateTreeCompilerLog Log;
		FStateTreeCompiler Compiler(Log);
		Result->RootTree = Compiler.Compile(*Tree) ? Tree : nullptr;
		return Result;
	}
	/** 初始化无重力的单位，避免无地板 World 对导航外的测试施加坠落干扰。 */
	ACombatUnitCharacter* Spawn(UWorld& World)
	{
		auto* Unit = World.SpawnActor<ACombatUnitCharacter>();
		auto* Data = NewObject<UCombatUnitData>(Unit);
		Data->DefinitionName = TEXT("ai_tree_unit");
		Unit->InitializeFromUnitData(Data);
		Unit->GetCharacterMovement()->GravityScale = 0;
		return Unit;
	}
	/** 分帧运行实际 StateTreeComponent 和 Scheduler。 */
	void Advance(UWorld& World, int32 Frames = 12)
	{
		for (int32 Index = 0; Index < Frames; ++Index)
		{
			World.Tick(LEVELTICK_All, 0.05f);
			World.GetSubsystem<UCombatSchedulerSubsystem>()->RunDueTasks(World.GetTimeSeconds());
			// 同步 Automation 的 GFrameCounter 不推进，显式派发已被原生调度唤醒的组件。
			// 不唤醒休眠树；实际 PIE/Dedicated 另行验证引擎帧调度。
			for (TActorIterator<ACombatUnitCharacter> It(&World); It; ++It)
			{
				auto* Brain = It->GetCombatAIBrainComponent();
				if (Brain->IsRunning() && Brain->IsComponentTickEnabled()) Brain->TickComponent(0.05f, LEVELTICK_All, nullptr);
			}
		}
	}
	/** 原地 Move 会在 IssueOrder 返回前产生完成通知。 */
	FCombatOrderRequest Move(FVector Location = FVector::ZeroVector)
	{
		FCombatOrderRequest Request;
		Request.Type = ECombatOrderType::MoveToPoint;
		Request.bHasTargetLocation = true;
		Request.TargetLocation = Location;
		return Request;
	}
}

/** 证明 Prepare 的实例退出后，链接资产仍能消费意图；同步完成只能记账一次。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatAIStateTreeHandoffTest, "Combat.AI.StateTree.LinkedSynchronousReceipt", CombatAIStateTreeTests::Flags)
bool FCombatAIStateTreeHandoffTest::RunTest(const FString& Parameters)
{
	FCombatAutomationWorldFixture Fixture;
	if (!Fixture.IsValid()) return false;
	auto* Unit = CombatAIStateTreeTests::Spawn(*Fixture.GetWorld());
	auto* Brain = Unit->GetCombatAIBrainComponent();
	auto* Profile = CombatAIStateTreeTests::Profile(Unit);
	TestNotNull(TEXT("Compiled root"), Profile->RootTree.Get());
	Brain->ConfigureProfile(Profile);
	TestTrue(TEXT("Server StateTree runs"), Brain->IsRunning());
	Brain->SetObjective(CombatAIStateTreeTests::Move(Unit->GetActorLocation()));
	CombatAIStateTreeTests::Advance(*Fixture.GetWorld());
	TestEqual(TEXT("One submission through linked task"), Brain->GetSubmittedCount(), uint64(1));
	TestEqual(TEXT("Synchronous receipt resolved once"), Brain->GetResolvedCount(), uint64(1));
	TestTrue(TEXT("Move result survives task Exit"), Brain->GetLastReceipt().Result.bSuccess);
	Brain->Wake(); Brain->Wake();
	CombatAIStateTreeTests::Advance(*Fixture.GetWorld());
	TestEqual(TEXT("Duplicate wake does not resubmit"), Brain->GetSubmittedCount(), uint64(1));
	TestFalse(TEXT("Idle scope is released"), Brain->GetDecisionScope().IsValid());
	TestEqual(TEXT("Scheduler waits cleaned"), Brain->GetActiveWaitCount(), 0);
	return true;
}

/** 非法命令不夺权，合法追加接管后旧树清理不能误取消新的手动 FIFO。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatAIControlTest, "Combat.AI.Lifecycle.ManualTakeoverAndRespawn", CombatAIStateTreeTests::Flags)
bool FCombatAIControlTest::RunTest(const FString& Parameters)
{
	FCombatAutomationWorldFixture Fixture;
	if (!Fixture.IsValid()) return false;
	auto& World = *Fixture.GetWorld();
	auto* Unit = CombatAIStateTreeTests::Spawn(World);
	auto* Brain = Unit->GetCombatAIBrainComponent();
	auto* Orders = Unit->GetCombatOrderComponent();
	Orders->SetNavigationDeferredForTesting(true);
	Brain->SetObjective(CombatAIStateTreeTests::Move(FVector(1000, 0, 0)));
	Brain->ConfigureProfile(CombatAIStateTreeTests::Profile(Unit));
	CombatAIStateTreeTests::Advance(World, 3);
	const auto OldAI = Orders->GetCurrentOrderHandle();
	TestTrue(TEXT("AI has active order"), OldAI.IsValid());
	FCombatOrderRequest Invalid;
	Invalid.Type = ECombatOrderType::CastNoTarget;
	Invalid.AbilitySpecHandle = FGameplayAbilitySpec(UCombatGameplayAbility::StaticClass(), 1).Handle;
	TestFalse(TEXT("Ungranted cast is rejected"), Orders->IssueOrder(Invalid).bSuccess);
	TestTrue(TEXT("Rejected request preserves autonomous run"), Brain->IsRunning());
	const auto Player = Orders->IssueOrder(CombatAIStateTreeTests::Move(FVector(2000, 0, 0)), true);
	const auto Queued = Orders->IssueOrder(CombatAIStateTreeTests::Move(FVector(3000, 0, 0)), true);
	TestFalse(TEXT("Accepted append stops Brain"), Brain->IsRunning());
	TestEqual(TEXT("Player takes current position in queue"), Orders->GetCurrentOrderHandle(), Player.Handle);
	TestEqual(TEXT("Later player append survives"), Orders->GetPendingOrderCount(), 1);
	Brain->StopLogic(TEXT("Repeated cleanup"));
	TestEqual(TEXT("Old cleanup preserves manual current"), Orders->GetCurrentOrderHandle(), Player.Handle);
	TestFalse(TEXT("Old AI cancellation is stale"), Orders->CancelCurrentOrderIfMatches(OldAI, {}));
	TestTrue(TEXT("Explicit resume works"), Brain->ResumeAutonomous());
	CombatAIStateTreeTests::Advance(World, 3);
	const uint64 Run = Brain->GetRunSerial();
	Unit->GetCombatLifecycleComponent()->RequestDeath(World.GetSubsystem<UCombatEventSubsystem>()->CreateRootEvent(), nullptr);
	TestFalse(TEXT("Death stops StateTree immediately"), Brain->IsRunning());
	TestFalse(TEXT("Death clears Scope"), Brain->GetDecisionScope().IsValid());
	Unit->GetCombatLifecycleComponent()->RespawnAtLocation(FVector::ZeroVector);
	CombatAIStateTreeTests::Advance(World, 3);
	TestTrue(TEXT("Autonomous respawn starts new run"), Brain->IsRunning() && Brain->GetRunSerial() > Run);
	const uint64 BeforeControllerChange = Brain->GetRunSerial();
	auto* Controller = CastChecked<ACombatUnitAIController>(Unit->GetController());
	Controller->UnPossess();
	TestFalse(TEXT("Losing navigation controller stops tree"), Brain->IsRunning());
	TestFalse(TEXT("Controller loss cleans old order"), Orders->GetCurrentOrderHandle().IsValid());
	Controller->Possess(Unit);
	CombatAIStateTreeTests::Advance(World, 3);
	TestTrue(TEXT("Controller return starts a fresh autonomous run"), Brain->IsRunning() && Brain->GetRunSerial() > BeforeControllerChange);
	Unit->Destroy();
	TestEqual(TEXT("EndPlay cancels all Brain waits"), Brain->GetActiveWaitCount(), 0);
	return true;
}

/** 准备数据的消费槽、修订、有效期、目标生命与单写入者是协议约束，不依赖树的接线正确。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatAIWorkspaceTest, "Combat.AI.Workspace.StaleAndSingleConsumption", CombatAIStateTreeTests::Flags)
bool FCombatAIWorkspaceTest::RunTest(const FString& Parameters)
{
	FCombatAutomationWorldFixture Fixture;
	if (!Fixture.IsValid()) return false;
	auto& World = *Fixture.GetWorld();
	auto* Unit = CombatAIStateTreeTests::Spawn(World);
	auto* Brain = Unit->GetCombatAIBrainComponent();
	auto* Profile = CombatAIStateTreeTests::PassiveProfile(Unit);
	Brain->ConfigureProfile(Profile);
	const auto Scope = Brain->BeginDecisionScope();
	Brain->SetObjective(CombatAIStateTreeTests::Move(Unit->GetActorLocation()));
	TestTrue(TEXT("Prepare before sibling exit"), Brain->Prepare(Scope, TEXT("Action"), Brain->AllocateActivation()));
	const auto Activation = Brain->AllocateActivation();
	TestTrue(TEXT("Consume prepared intent"), Brain->BeginAction(Scope, TEXT("Action"), Activation));
	TestTrue(TEXT("Same activation is idempotent"), Brain->BeginAction(Scope, TEXT("Action"), Activation));
	TestFalse(TEXT("Parallel writer rejected"), Brain->BeginAction(Scope, TEXT("Action"), Brain->AllocateActivation()));
	Brain->EndAction(Activation);
	TestTrue(TEXT("Receipt survives Execute exit"), Brain->ResolveReceipt(Scope));
	TestFalse(TEXT("Receipt cannot be acknowledged twice"), Brain->ResolveReceipt(Scope));
	TestEqual(TEXT("Only one actual submission"), Brain->GetSubmittedCount(), uint64(1));
	Brain->Prepare(Scope, TEXT("Action"), Brain->AllocateActivation());
	TestFalse(TEXT("Wrong consumer slot fails"), Brain->BeginAction(Scope, TEXT("Other"), Brain->AllocateActivation()));
	TestTrue(TEXT("Wrong slot creates failure receipt"), Brain->ResolveReceipt(Scope));
	TestFalse(TEXT("Wrong slot receipt is not success"), Brain->GetLastReceipt().Result.bSuccess);
	Brain->Prepare(Scope, TEXT("Action"), Brain->AllocateActivation());
	Brain->SetObjective(CombatAIStateTreeTests::Move(FVector(1000, 0, 0)));
	TestFalse(TEXT("Changed objective invalidates preparation"), Brain->BeginAction(Scope, TEXT("Action"), Brain->AllocateActivation()));
	Brain->ResolveReceipt(Scope);
	Profile->IntentLifetime = 0.01f;
	Brain->Prepare(Scope, TEXT("Action"), Brain->AllocateActivation());
	CombatAIStateTreeTests::Advance(World, 2);
	TestFalse(TEXT("Expired preparation fails"), Brain->BeginAction(Scope, TEXT("Action"), Brain->AllocateActivation()));
	Brain->ResolveReceipt(Scope);
	auto* Target = CombatAIStateTreeTests::Spawn(World);
	Target->SetCombatTeamId(FCombatTeamId(2));
	FCombatOrderRequest Attack;
	Attack.Type = ECombatOrderType::AttackTarget;
	Attack.TargetUnit = Target;
	Profile->IntentLifetime = 1;
	Brain->SetObjective(Attack);
	TestTrue(TEXT("Live target can be prepared"), Brain->Prepare(Scope, TEXT("Action"), Brain->AllocateActivation()));
	Target->GetCombatLifecycleComponent()->RequestDeath(World.GetSubsystem<UCombatEventSubsystem>()->CreateRootEvent(), Unit);
	Target->GetCombatLifecycleComponent()->RespawnAtLocation(FVector(150, 0, 0));
	TestFalse(TEXT("Same Actor new life invalidates preparation"), Brain->BeginAction(Scope, TEXT("Action"), Brain->AllocateActivation()));
	Brain->ResolveReceipt(Scope);
	Brain->EndDecisionScope(Scope);
	TestFalse(TEXT("Ended scope cannot prepare"), Brain->Prepare(Scope, TEXT("Action"), Brain->AllocateActivation()));
	Brain->StopLogic(TEXT("Protocol test teardown"));
	CombatAIStateTreeTests::Advance(World, 3);
	TestEqual(TEXT("Teardown removes scheduled wait"), Brain->GetActiveWaitCount(), 0);
	return true;
}

/** 持续普攻收到施法目标后等待本次前摇完成，再经公共 OrderReleased 确认施法。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatAIAttackCastTest, "Combat.AI.StateTree.AttackBoundaryToCast", CombatAIStateTreeTests::Flags)
bool FCombatAIAttackCastTest::RunTest(const FString& Parameters)
{
	FCombatAutomationWorldFixture Fixture;
	if (!Fixture.IsValid()) return false;
	auto& World = *Fixture.GetWorld();
	auto* Unit = CombatAIStateTreeTests::Spawn(World);
	auto* Target = CombatAIStateTreeTests::Spawn(World);
	Target->SetCombatTeamId(FCombatTeamId(2));
	Target->SetActorLocation(FVector(150, 0, 0));
	auto* Data = NewObject<UCombatAbilityData>(Unit);
	Data->DefinitionName = TEXT("ai_cast_fixture");
	Data->BehaviorTags.AddTag(CombatTags::Ability_Behavior_NoTarget);
	Data->TargetingRules.TargetTeamTag = CombatTags::TargetTeam_None;
	TGuardValue<TObjectPtr<UCombatAbilityData>> Restore(GetMutableDefault<UCombatSelfHealAbility>()->AbilityData, Data);
	FGameplayAbilitySpecHandle Spec;
	FGameplayTag Failure;
	TestTrue(TEXT("Granted real ability"), Unit->GetCombatAbilitySystemComponent()->GrantCombatAbility(UCombatSelfHealAbility::StaticClass(), 1, false, Spec, Failure));
	auto* Brain = Unit->GetCombatAIBrainComponent();
	FCombatOrderRequest Attack;
	Attack.Type = ECombatOrderType::AttackTarget; Attack.TargetUnit = Target;
	Brain->SetObjective(Attack);
	Brain->ConfigureProfile(CombatAIStateTreeTests::Profile(Unit));
	CombatAIStateTreeTests::Advance(World, 1);
	const auto OldOrder = Unit->GetCombatOrderComponent()->GetCurrentOrderHandle();
	const auto Windup = Unit->GetCombatAttackComponent()->GetCurrentWindupHandle();
	TestTrue(TEXT("Attack enters real windup"), Windup.IsValid());
	Brain->SetObjective(Attack);
	CombatAIStateTreeTests::Advance(World, 1);
	TestEqual(TEXT("Same objective keeps existing attack order"), Unit->GetCombatOrderComponent()->GetCurrentOrderHandle(), OldOrder);
	TestEqual(TEXT("Same objective does not resubmit"), Brain->GetSubmittedCount(), uint64(1));
	int32 Launched = 0;
	const auto Binding = Unit->GetCombatAttackComponent()->OnAttackLaunched().AddLambda([&](FCombatAttackHandle, FCombatOrderHandle) { ++Launched; });
	FCombatOrderRequest Cast;
	Cast.Type = ECombatOrderType::CastNoTarget; Cast.AbilitySpecHandle = Spec;
	Brain->SetObjective(Cast);
	CombatAIStateTreeTests::Advance(World, 1);
	TestEqual(TEXT("Ordinary update preserves protected order"), Unit->GetCombatOrderComponent()->GetCurrentOrderHandle(), OldOrder);
	CombatAIStateTreeTests::Advance(World, 30);
	TestEqual(TEXT("Only protected attack launches"), Launched, 1);
	TestEqual(TEXT("Attack and cast each submitted once"), Brain->GetSubmittedCount(), uint64(2));
	TestEqual(TEXT("Both terminal receipts resolved"), Brain->GetResolvedCount(), uint64(2));
	TestTrue(TEXT("Cast finishes through OrderReleased"), Brain->GetLastReceipt().Result.bSuccess && Brain->GetLastReceipt().Result.Type == ECombatOrderType::CastNoTarget);
	Unit->GetCombatAttackComponent()->OnAttackLaunched().Remove(Binding);
	return true;
}

/** 回调中更新目标或死亡不会丢掉已完成结果，也不会在 IssueOrder 返回后重新安装旧动作。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatAIReentrantCompletionTest, "Combat.AI.StateTree.SynchronousReentry", CombatAIStateTreeTests::Flags)
bool FCombatAIReentrantCompletionTest::RunTest(const FString& Parameters)
{
	FCombatAutomationWorldFixture Fixture;
	if (!Fixture.IsValid()) return false;
	auto& World = *Fixture.GetWorld();
	auto* Unit = CombatAIStateTreeTests::Spawn(World);
	auto* Brain = Unit->GetCombatAIBrainComponent();
	auto* Orders = Unit->GetCombatOrderComponent();
	bool bUpdated = false;
	const auto Binding = Orders->OnOrderFinished().AddLambda([&](const FCombatOrderResult& Result)
	{
		if (!bUpdated && Result.bSuccess) { bUpdated = true; Brain->SetObjective(CombatAIStateTreeTests::Move(Unit->GetActorLocation())); }
	});
	Brain->ConfigureProfile(CombatAIStateTreeTests::Profile(Unit));
	Brain->SetObjective(CombatAIStateTreeTests::Move(Unit->GetActorLocation()));
	CombatAIStateTreeTests::Advance(World, 35);
	TestEqual(TEXT("Completion before ordinary re-evaluation"), Brain->GetResolvedCount(), uint64(2));
	TestEqual(TEXT("Updated objective submitted exactly once"), Brain->GetSubmittedCount(), uint64(2));
	Orders->OnOrderFinished().Remove(Binding);
	const auto DeathBinding = Orders->OnOrderFinished().AddLambda([&](const FCombatOrderResult& Result)
	{
		if (Result.bSuccess) Unit->GetCombatLifecycleComponent()->RequestDeath(World.GetSubsystem<UCombatEventSubsystem>()->CreateRootEvent(), nullptr);
	});
	Brain->SetObjective(CombatAIStateTreeTests::Move(Unit->GetActorLocation()));
	CombatAIStateTreeTests::Advance(World, 8);
	TestFalse(TEXT("Synchronous death leaves Brain stopped"), Brain->IsRunning());
	TestFalse(TEXT("Synchronous death leaves no order"), Orders->GetCurrentOrderHandle().IsValid());
	TestFalse(TEXT("Synchronous death clears workspace"), Brain->GetDecisionScope().IsValid());
	Orders->OnOrderFinished().Remove(DeathBinding);
	return true;
}

/** 即使客户端本地 Spawn 的 Actor 自称 Authority，也不得启动服务器决策。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatAIClientAuthorityTest, "Combat.AI.Lifecycle.ClientCannotRun", CombatAIStateTreeTests::Flags)
bool FCombatAIClientAuthorityTest::RunTest(const FString& Parameters)
{
	FCombatAutomationWorldFixture Fixture(NM_Client);
	if (!Fixture.IsValid()) return false;
	auto* Unit = CombatAIStateTreeTests::Spawn(*Fixture.GetWorld());
	auto* Brain = Unit->GetCombatAIBrainComponent();
	Brain->ConfigureProfile(CombatAIStateTreeTests::Profile(Unit));
	Brain->SetObjective(CombatAIStateTreeTests::Move());
	Brain->StartLogic();
	TestFalse(TEXT("Client refuses explicit resume"), Brain->ResumeAutonomous());
	TestFalse(TEXT("Client tree never runs"), Brain->IsRunning());
	TestEqual(TEXT("Client has no run identity"), Brain->GetRunSerial(), uint64(0));
	return true;
}

/** 静态内容校验拒绝同一路径的多个命令阶段，不能只依赖运行时互斥后静默失败。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatAIAssetContractTest, "Combat.AI.Assets.SingleWriterValidation", CombatAIStateTreeTests::Flags)
bool FCombatAIAssetContractTest::RunTest(const FString& Parameters)
{
	auto* Profile = CombatAIStateTreeTests::Profile(GetTransientPackage());
	FString Diagnostic;
	TestTrue(TEXT("Valid linked tree passes"), Profile->ValidateRuntime(Diagnostic));
	auto* Data = CastChecked<UStateTreeEditorData>(Profile->RootTree->EditorData);
	auto& ScopeTask = Data->SubTrees[0]->Children[1]->Tasks[0].Node.GetMutable<FCombatAIDecisionScopeTask>();
	ScopeTask.bTaskEnabled = false;
	TestFalse(TEXT("Disabled scope cannot satisfy active path contract"), FCombatAIAssetBuilder::ValidateRootTree(Profile->RootTree, Diagnostic));
	ScopeTask.bTaskEnabled = true;
	Data->SubTrees[0]->AddTask<FCombatAIExecuteOrderTask>();
	TestFalse(TEXT("Parent writer without Scope rejected"), FCombatAIAssetBuilder::ValidateRootTree(Profile->RootTree, Diagnostic));
	return true;
}
#endif
