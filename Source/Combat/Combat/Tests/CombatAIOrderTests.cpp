#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Engine/World.h"
#include "Combat/Attack/CombatAttackComponent.h"
#include "Combat/Core/CombatTags.h"
#include "Combat/Data/CombatDefinitionData.h"
#include "Combat/Order/CombatOrderComponent.h"
#include "Combat/Log/CombatEventSubsystem.h"
#include "Combat/Scheduling/CombatSchedulerSubsystem.h"
#include "Combat/Tests/CombatAutomationWorldFixture.h"
#include "Combat/Unit/CombatUnitCharacter.h"

namespace CombatAIOrderTests
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;
	/** 小步推进真实世界时钟，避免 World 对单帧大 Delta 的钳制改变 deadline 断言。 */
	void Advance(UWorld& World, float Seconds)
	{
		for (int32 Step = 0; Step < FMath::CeilToInt(Seconds / 0.05f); ++Step) World.Tick(LEVELTICK_All, 0.05f);
	}
	/** 使用真实 Unit/Attack/Order，避免用替身掩盖同步回调和攻击调度顺序。 */
	ACombatUnitCharacter* Spawn(UWorld& World, uint8 Team, FVector Location)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		auto* Unit = World.SpawnActor<ACombatUnitCharacter>(Location, FRotator::ZeroRotator, Params);
		auto* Data = NewObject<UCombatUnitData>(Unit);
		Data->DefinitionName = TEXT("ai_order_fixture");
		Data->InitialTeamId = FCombatTeamId(Team);
		Data->BaseAttackPoint = 0.2f;
		Data->BaseStats.BaseAttackTime = 1.0f;
		Data->BaseStats.MaxHealth = 10000;
		return Unit->InitializeFromUnitData(Data) ? Unit : nullptr;
	}
	/** 构造可注入导航完成的远端点命令。 */
	FCombatOrderRequest Move(double X)
	{
		FCombatOrderRequest Request;
		Request.Type = ECombatOrderType::MoveToPoint;
		Request.bHasTargetLocation = true;
		Request.TargetLocation = FVector(X, 0, 0);
		return Request;
	}
}

/** 精确取消只结束指定当前项，不能吞掉新控制者的排队项，也不能被旧回调再次取消。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatAIOrderCancelTest, "Combat.AI.Order.CancelPreservesQueue", CombatAIOrderTests::Flags)
bool FCombatAIOrderCancelTest::RunTest(const FString& Parameters)
{
	FCombatAutomationWorldFixture Fixture;
	if (!Fixture.IsValid()) return false;
	auto* Unit = CombatAIOrderTests::Spawn(*Fixture.GetWorld(), 1, FVector::ZeroVector);
	if (!TestNotNull(TEXT("Unit initialized"), Unit)) return false;
	auto* Orders = Unit->GetCombatOrderComponent();
	Orders->SetNavigationDeferredForTesting(true);
	const auto First = Orders->IssueOrder(CombatAIOrderTests::Move(1000));
	const auto Next = Orders->IssueOrder(CombatAIOrderTests::Move(2000), true);
	int32 CancelCount = 0;
	const auto Binding = Orders->OnOrderFinished().AddLambda([&](const FCombatOrderResult& Result)
	{
		if (Result.Handle == First.Handle && Result.State == ECombatOrderState::Cancelled) ++CancelCount;
	});
	TestTrue(TEXT("Matched cancellation succeeds"), Orders->CancelCurrentOrderIfMatches(First.Handle, CombatTags::Order_Failure_Cancelled));
	TestEqual(TEXT("Queued command continues with original identity"), Orders->GetCurrentOrderHandle(), Next.Handle);
	TestFalse(TEXT("Old cancellation cannot cancel successor"), Orders->CancelCurrentOrderIfMatches(First.Handle, {}));
	TestEqual(TEXT("One terminal cancellation"), CancelCount, 1);
	Orders->OnOrderFinished().Remove(Binding);
	return true;
}

/** 前摇保护到 Launch，Ready 期间阻止下一次起手；Keep 不重新下单，超时自动释放。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatAIAttackBoundaryTest, "Combat.AI.Order.AttackBoundary", CombatAIOrderTests::Flags)
bool FCombatAIAttackBoundaryTest::RunTest(const FString& Parameters)
{
	FCombatAutomationWorldFixture Fixture;
	if (!Fixture.IsValid()) return false;
	auto& World = *Fixture.GetWorld();
	auto* Unit = CombatAIOrderTests::Spawn(World, 1, FVector::ZeroVector);
	auto* Target = CombatAIOrderTests::Spawn(World, 2, FVector(150, 0, 0));
	if (!Unit || !Target) return false;
	auto* Orders = Unit->GetCombatOrderComponent();
	auto* Attacks = Unit->GetCombatAttackComponent();
	FCombatOrderRequest Request;
	Request.Type = ECombatOrderType::AttackTarget;
	Request.TargetUnit = Target;
	const auto Accepted = Orders->IssueOrder(Request);
	const auto Ticket = Orders->RequestExecutionBoundary(Accepted.Handle, 2.0f);
	TestTrue(TEXT("Boundary request is valid"), Ticket.IsValid());
	TestFalse(TEXT("Windup is protected"), Orders->IsExecutionBoundaryReady(Ticket));
	CombatAIOrderTests::Advance(World, 0.25f);
	TestTrue(TEXT("Launch makes boundary ready"), Orders->IsExecutionBoundaryReady(Ticket));
	CombatAIOrderTests::Advance(World, 0.85f);
	TestTrue(TEXT("Native attack clock still becomes ready"), Attacks->IsAttackReady());
	TestFalse(TEXT("Hold prevents next windup"), Attacks->GetCurrentWindupHandle().IsValid());
	TestTrue(TEXT("Keep releases exact ticket"), Orders->ReleaseExecutionBoundary(Ticket));
	TestEqual(TEXT("Keep preserves Order handle"), Orders->GetCurrentOrderHandle(), Accepted.Handle);
	TestTrue(TEXT("Same order resumes once"), Attacks->GetCurrentWindupHandle().IsValid());
	TestFalse(TEXT("Old ticket cannot release another hold"), Orders->ReleaseExecutionBoundary(Ticket));
	const auto Timeout = Orders->RequestExecutionBoundary(Accepted.Handle, 0.1f);
	CombatAIOrderTests::Advance(World, 0.25f);
	TestTrue(TEXT("Deadline begins on ready"), Orders->IsExecutionBoundaryReady(Timeout));
	CombatAIOrderTests::Advance(World, 0.15f);
	TestFalse(TEXT("Timeout retires ticket"), Orders->IsExecutionBoundaryReady(Timeout));
	CombatAIOrderTests::Advance(World, 1.0f);
	TestTrue(TEXT("Timeout does not strand continuous order"), Orders->GetCurrentOrderHandle() == Accepted.Handle);
	return true;
}
/** 取消前摇会同步广播 AttackFinalized；回调中的新命令不能被旧 Order 的清理覆盖。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatAIOrderCancelReentryTest, "Combat.AI.Order.CancelReentry", CombatAIOrderTests::Flags)
bool FCombatAIOrderCancelReentryTest::RunTest(const FString& Parameters)
{
	FCombatAutomationWorldFixture Fixture;
	if (!Fixture.IsValid()) return false;
	auto* Unit = CombatAIOrderTests::Spawn(*Fixture.GetWorld(), 1, FVector::ZeroVector);
	auto* Target = CombatAIOrderTests::Spawn(*Fixture.GetWorld(), 2, FVector(150, 0, 0));
	if (!Unit || !Target) return false;
	auto* Orders = Unit->GetCombatOrderComponent();
	Orders->SetNavigationDeferredForTesting(true);
	FCombatOrderRequest Attack;
	Attack.Type = ECombatOrderType::AttackTarget;
	Attack.TargetUnit = Target;
	const auto Old = Orders->IssueOrder(Attack);
	FCombatOrderHandle Replacement;
	int32 OldFinishedCount = 0;
	const auto FinishBinding = Orders->OnOrderFinished().AddLambda([&](const FCombatOrderResult& Result)
	{
		if (Result.Handle == Old.Handle) ++OldFinishedCount;
	});
	const auto AttackBinding = Unit->GetCombatAttackComponent()->OnAttackFinalized().AddLambda([&](const FCombatAttackResult& Result)
	{
		Replacement = Orders->IssueOrder(CombatAIOrderTests::Move(2000)).Handle;
	});
	TestTrue(TEXT("Old exact cancellation succeeds"), Orders->CancelCurrentOrderIfMatches(Old.Handle, CombatTags::Order_Failure_Cancelled));
	TestTrue(TEXT("Synchronous callback submitted replacement"), Replacement.IsValid());
	TestEqual(TEXT("Old cleanup preserves replacement"), Orders->GetCurrentOrderHandle(), Replacement);
	TestEqual(TEXT("Old order finalizes exactly once"), OldFinishedCount, 1);
	Unit->GetCombatAttackComponent()->OnAttackFinalized().Remove(AttackBinding);
	Orders->OnOrderFinished().Remove(FinishBinding);
	const auto BeforeLogReentry = Orders->GetCurrentOrderHandle();
	auto* Events = Fixture.GetWorld()->GetSubsystem<UCombatEventSubsystem>();
	bool bLogReentered = false;
	FCombatOrderHandle FromLog;
	const auto LogBinding = Events->OnRecord().AddLambda([&](const FCombatLogRecord& Record)
	{
		if (!bLogReentered && Record.EventType == CombatTags::Event_Combat_OrderStateChanged
			&& Record.FailureTag == CombatTags::Order_Failure_Cancelled && Record.Diagnostic.Contains(BeforeLogReentry.ToString()))
		{
			bLogReentered = true;
			FromLog = Orders->IssueOrder(CombatAIOrderTests::Move(3000)).Handle;
		}
	});
	Orders->CancelCurrentOrderIfMatches(BeforeLogReentry, CombatTags::Order_Failure_Cancelled);
	TestTrue(TEXT("Terminal log observer synchronously submits"), bLogReentered && FromLog.IsValid());
	TestEqual(TEXT("Terminal log reentry preserves replacement"), Orders->GetCurrentOrderHandle(), FromLog);
	Events->OnRecord().Remove(LogBinding);
	return true;
}
#endif
