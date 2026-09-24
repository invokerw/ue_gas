#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS
#include <limits>
#include "Misc/AutomationTest.h"
#include "Engine/World.h"
#include "Engine/LocalPlayer.h"
#include "Engine/Engine.h"
#include "Combat/Tests/CombatAutomationWorldFixture.h"
#include "Combat/Unit/CombatUnitCharacter.h"
#include "Combat/Data/CombatDefinitionData.h"
#include "Combat/Core/CombatTags.h"
#include "Combat/Ability/CombatAbilitySystemComponent.h"
#include "Combat/Demo/CombatDemoAbilities.h"
#include "Combat/Items/CombatInventoryComponent.h"
#include "Combat/Items/CombatItemData.h"
#include "Combat/Network/CombatNetworkSecuritySubsystem.h"
#include "Combat/Order/CombatOrderComponent.h"
#include "Combat/View/CombatUnitViewComponent.h"
#include "Combat/UI/CombatHUDWidget.h"
#include "EngineUtils.h"
#include "CombatPlayerController.h"

namespace CombatSelectionTests
{
	/** 使用真实 ASC/Owner/Order，仅将导航完成延后以观察组内指令独立存在。 */
	ACombatUnitCharacter* Spawn(UWorld* World, uint8 Team, FVector Location = FVector::ZeroVector)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		auto* Unit = World->SpawnActor<ACombatUnitCharacter>(Location, FRotator::ZeroRotator, Params);
		auto* Data = NewObject<UCombatUnitData>(Unit);
		Data->DefinitionName = TEXT("selection_unit");
		Data->InitialTeamId = FCombatTeamId(Team);
		if (!Unit->InitializeFromUnitData(Data)) return nullptr;
		if (!Unit->HasActorBegunPlay()) Unit->DispatchBeginPlay();
		Unit->GetCombatOrderComponent()->SetNavigationDeferredForTesting(true);
		return Unit;
	}
	/** 生成固定上限内的合法移动信封，变异测试只改变一个安全字段。 */
	FCombatGroupOrderRequest Move(ACombatUnitCharacter* A, ACombatUnitCharacter* B, int32 Id)
	{
		FCombatGroupOrderRequest Request;
		Request.RequestId = Id;
		Request.Units = {{A, int64(A->GetLifeGeneration())}, {B, int64(B->GetLifeGeneration())}};
		Request.Order.Type = ECombatOrderType::MoveToPoint;
		Request.Order.TargetLocation = FVector(1500, 0, 0);
		Request.Order.bHasTargetLocation = true;
		return Request;
	}
}

/** 蓝图选择入口必须只切换观察对象；查看其他玩家单位不能抢占其网络 Owner。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatSelectionInspectionTest,
	"Combat.Input.Selection.InspectionDoesNotGrantControl",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCombatSelectionInspectionTest::RunTest(const FString& Parameters)
{
	FCombatAutomationWorldFixture Fixture;
	if (!Fixture.IsValid()) return false;
	UWorld* World = Fixture.GetWorld();
	ACombatPlayerController* Player = World->SpawnActor<ACombatPlayerController>();
	Player->SetAsLocalPlayerController();
	ACombatPlayerController* Other = World->SpawnActor<ACombatPlayerController>();
	ACombatUnitCharacter* Enemy = World->SpawnActor<ACombatUnitCharacter>();
	UCombatUnitData* Data = NewObject<UCombatUnitData>(Enemy);
	Data->DefinitionName = TEXT("selection_enemy");
	Data->InitialTeamId = FCombatTeamId(2);
	if (!Enemy->InitializeFromUnitData(Data) || !Other->SetCommandedUnitAuthority(Enemy)) return false;
	UFunction* Select = Player->FindFunction(TEXT("SelectCombatUnit"));
	UFunction* Inspect = Player->FindFunction(TEXT("GetInspectedUnit"));
	if (!TestNotNull(TEXT("World click has a public local selection entry"), Select)
		|| !TestNotNull(TEXT("HUD has an independent inspection source"), Inspect)) return false;
	struct FSelectParams { ACombatUnitCharacter* Unit; bool bToggle; } Request{Enemy, false};
	Player->ProcessEvent(Select, &Request);
	struct FInspectParams { ACombatUnitCharacter* ReturnValue = nullptr; } Result;
	Player->ProcessEvent(Inspect, &Result);
	TestEqual(TEXT("HUD observes the clicked enemy"), Result.ReturnValue, Enemy);
	TestEqual(TEXT("Inspection does not transfer control"), Enemy->GetCommandingPlayerController(), static_cast<APlayerController*>(Other));
	return true;
}

/** 单选/追加/移除/只读切换保留 Owner 与旧 Order，群体动作只消费一个请求额度。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatSelectionGroupTest, "Combat.Input.Selection.GroupOrdersAndPrimary",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCombatSelectionGroupTest::RunTest(const FString& Parameters)
{
	using namespace CombatSelectionTests;
	FCombatAutomationWorldFixture Fixture;
	UWorld* World = Fixture.GetWorld();
	if (!World) return false;
	auto* Player = World->SpawnActor<ACombatPlayerController>();
	Player->SetAsLocalPlayerController();
	auto* A = Spawn(World, 1);
	auto* B = Spawn(World, 1, FVector(0, 200, 0));
	auto* Enemy = Spawn(World, 2, FVector(2000, 0, 0));
	if (!A || !B || !Enemy) return false;
	TestTrue(TEXT("Initial owner"), Player->SetCommandedUnitAuthority(A));
	TestTrue(TEXT("Additional control does not unbind initial hero"), Player->GrantUnitControlAuthority(B));
	Player->SelectCombatUnit(A);
	Player->SelectCombatUnit(B, true);
	TestEqual(TEXT("Shift adds owned hero"), Player->GetSelectedUnits().Num(), 2);
	Player->SelectCombatUnit(Enemy, true);
	TestEqual(TEXT("Shift cannot append enemy"), Player->GetSelectedUnits().Num(), 2);
	const auto MoveRequest = Move(A, B, 1);
	const auto Before = World->GetSubsystem<UCombatNetworkSecuritySubsystem>()->GetSecurityStats();
	Player->SubmitCombatOrder(MoveRequest.Order);
	TestEqual(TEXT("Group consumes one shared budget"), World->GetSubsystem<UCombatNetworkSecuritySubsystem>()->GetSecurityStats().AcceptedRequests, Before.AcceptedRequests + 1);
	TestEqual(TEXT("A receives move"), A->GetCombatOrderComponent()->GetCurrentState(), ECombatOrderState::Moving);
	TestEqual(TEXT("B receives move"), B->GetCombatOrderComponent()->GetCurrentState(), ECombatOrderState::Moving);
	const auto OldOrder = A->GetCombatOrderComponent()->GetCurrentOrderHandle();
	Player->SelectCombatUnit(B);
	TestEqual(TEXT("Primary switched"), Player->GetCommandedUnit(), B);
	TestTrue(TEXT("Old hero remains controlled"), Player->CanControlUnit(A));
	TestEqual(TEXT("Old hero keeps exact running order"), A->GetCombatOrderComponent()->GetCurrentOrderHandle(), OldOrder);
	Player->SelectCombatUnits({B, A, B, Enemy}, false);
	TestEqual(TEXT("Box selection deduplicates and filters permission"), Player->GetSelectedUnits().Num(), 2);
	TestTrue(TEXT("Group attack submitted"), Player->IssueCombatAttackOrder(Enemy));
	TestEqual(TEXT("A chases"), A->GetCombatOrderComponent()->GetCurrentState(), ECombatOrderState::Chasing);
	TestEqual(TEXT("B chases"), B->GetCombatOrderComponent()->GetCurrentState(), ECombatOrderState::Chasing);
	Player->OnStopCommand();
	TestEqual(TEXT("A stopped"), A->GetCombatOrderComponent()->GetCurrentState(), ECombatOrderState::Idle);
	TestEqual(TEXT("B stopped"), B->GetCombatOrderComponent()->GetCurrentState(), ECombatOrderState::Idle);
	Player->SelectCombatUnit(Enemy);
	const int32 Next = Player->NextCombatOrderRequestId;
	TestNull(TEXT("Read-only target disables old primary"), Player->GetReadyCommandedUnit());
	TestFalse(TEXT("Read-only target cannot move old heroes"), Player->SubmitCombatOrder(MoveRequest.Order));
	Player->ActivateCombatAbilitySlotFromHUD(0);
	World->Tick(LEVELTICK_All, 0.02f);
	TestEqual(TEXT("Read-only HUD sent no request"), Player->NextCombatOrderRequestId, Next);
	Player->SelectCombatUnit(A);
	Player->SelectCombatUnit(A, true);
	Player->RefreshLocalSelection();
	TestTrue(TEXT("Removing final member remains empty"), Player->GetSelectedUnits().IsEmpty());
	const int32 BeforeRapidReturn = Player->NextCombatOrderRequestId;
	Player->PendingPrimaryRequestId = BeforeRapidReturn - 1;
	Player->LastRequestedPrimary = B;
	Player->SelectCombatUnit(Enemy);
	Player->SelectCombatUnit(A);
	TestEqual(TEXT("Returning to replicated primary still sends latest intent after in-flight selection"),
		Player->NextCombatOrderRequestId, BeforeRapidReturn + 1);
	TestTrue(TEXT("Latest primary acknowledgment restores operations"), Player->CanOperateInspectedUnit());
	FVector2D Start, End;
	Player->bSelectionGesture = true;
	Player->SelectionStart = FVector2D(40, 40);
	Player->SelectionEnd = FVector2D(100, 100);
	TestTrue(TEXT("Drag threshold produces a rectangle"), Player->GetSelectionRectangle(Start, End));
	Player->CancelCombatTargeting();
	TestFalse(TEXT("Cancel removes old rectangle"), Player->GetSelectionRectangle(Start, End));
	return true;
}

/** 组内任何非法身份都必须在执行前失败，且与单单位/经济共享重放和限频。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatSelectionSecurityTest, "Combat.Network.Selection.GroupSecurity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCombatSelectionSecurityTest::RunTest(const FString& Parameters)
{
	using namespace CombatSelectionTests;
	FCombatAutomationWorldFixture Fixture;
	UWorld* World = Fixture.GetWorld();
	if (!World) return false;
	auto* Player = World->SpawnActor<ACombatPlayerController>();
	auto* A = Spawn(World, 1);
	auto* B = Spawn(World, 1);
	auto* Enemy = Spawn(World, 2);
	if (!A || !B || !Enemy) return false;
	Player->SetCommandedUnitAuthority(A);
	Player->GrantUnitControlAuthority(B);
	auto Request = Move(A, Enemy, 1);
	TestEqual(TEXT("Unowned member rejected"), Player->ProcessGroupOrderRequest(Request).FailureTag, CombatTags::Failure_Network_Ownership.GetTag());
	TestEqual(TEXT("Reject does not execute earlier member"), A->GetCombatOrderComponent()->GetCurrentState(), ECombatOrderState::Idle);
	Request = Move(A, A, 2);
	TestFalse(TEXT("Duplicate unit rejected"), Player->ProcessGroupOrderRequest(Request).bAccepted);
	Request = Move(A, B, 3); ++Request.Units[1].LifeGeneration;
	TestFalse(TEXT("Old life rejected"), Player->ProcessGroupOrderRequest(Request).bAccepted);
	Request = Move(A, B, 4); Request.Order.Type = ECombatOrderType::CastNoTarget;
	TestFalse(TEXT("Cannot batch arbitrary abilities"), Player->ProcessGroupOrderRequest(Request).bAccepted);
	Request = Move(A, B, 5); Request.Order.TargetLocation.X = std::numeric_limits<double>::infinity();
	TestFalse(TEXT("Non-finite location rejected"), Player->ProcessGroupOrderRequest(Request).bAccepted);
	Request = Move(A, B, 0);
	TestEqual(TEXT("Positive replay ID required"), Player->ProcessGroupOrderRequest(Request).FailureTag, CombatTags::Failure_Network_InvalidRequestId.GetTag());
	Request = Move(A, B, 6); Request.Units.SetNum(9);
	TestFalse(TEXT("Oversized group rejected"), Player->ProcessGroupOrderRequest(Request).bAccepted);
	Request = Move(A, B, 7);
	TestEqual(TEXT("Valid group enters two public Orders"), Player->ProcessGroupOrderRequest(Request).AcceptedOrderCount, 2);
	TestEqual(TEXT("Replay rejected"), Player->ProcessGroupOrderRequest(Request).FailureTag, CombatTags::Failure_Network_DuplicateRequest.GetTag());
	FGameplayTag Failure;
	FString Diagnostic;
	auto* Security = World->GetSubsystem<UCombatNetworkSecuritySubsystem>();
	TestFalse(TEXT("Primary change cannot replay a group ID"), Security->ValidateAndConsumePrimarySelection(Player, A, 7, Failure, Diagnostic));
	TestFalse(TEXT("Primary change cannot acquire an enemy"), Security->ValidateAndConsumePrimarySelection(Player, Enemy, 8, Failure, Diagnostic));
	Security->BurstCapacity = 1;
	Request.RequestId = 9;
	TestTrue(TEXT("Last token accepted"), Player->ProcessGroupOrderRequest(Request).bAccepted);
	Request.RequestId = 10;
	TestEqual(TEXT("Group obeys connection rate limit"), Player->ProcessGroupOrderRequest(Request).FailureTag, CombatTags::Failure_Network_RateLimited.GetTag());
	return true;
}

/** 公开信息按白名单投影；无 Owner 的单位也有信息，私人冷却/经验/操作身份不公开。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatSelectionProjectionTest, "Combat.UI.HUD.PublicInspection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCombatSelectionProjectionTest::RunTest(const FString& Parameters)
{
	using namespace CombatSelectionTests;
	FCombatAutomationWorldFixture Fixture;
	UWorld* World = Fixture.GetWorld();
	if (!World) return false;
	auto* Unit = Spawn(World, 2);
	auto* Player = World->SpawnActor<ACombatPlayerController>();
	// 本地 UMG 会查询 LocalPlayer；仅标记 IsLocalController 的无视口夹具不足以构造 Slate。
	Player->Player = NewObject<ULocalPlayer>(GEngine);
	Player->SetAsLocalPlayerController();
	if (!Unit) return false;
	auto* Ability = NewObject<UCombatAbilityData>(Unit);
	Ability->DefinitionName = TEXT("selection_ability");
	Ability->BehaviorTags.AddTag(CombatTags::Ability_Behavior_NoTarget);
	Ability->TargetingRules.TargetTeamTag = CombatTags::TargetTeam_None;
	TGuardValue<TObjectPtr<UCombatAbilityData>> Restore(GetMutableDefault<UCombatSelfHealAbility>()->AbilityData, Ability);
	FGameplayAbilitySpecHandle Handle;
	FGameplayTag Failure;
	TestTrue(TEXT("Grant real ability"), Unit->GetCombatAbilitySystemComponent()->GrantCombatAbility(UCombatSelfHealAbility::StaticClass(), 1, false, Handle, Failure));
	auto* Item = NewObject<UCombatItemData>(Unit);
	Item->DefinitionName = TEXT("selection_item");
	FCombatItemHandle ItemHandle;
	TestTrue(TEXT("Give real inventory item"), Unit->GetCombatInventoryComponent()->GiveItem(Item, 1, ItemHandle, Failure));
	auto* View = Unit->GetCombatUnitViewComponent();
	View->RefreshHUDOwnerView();
	const auto Public = View->GetHUDInspectionView();
	TestEqual(TEXT("Unowned hero has real identity"), Public.UnitDefinitionId, Unit->GetUnitDefinitionId());
	TestEqual(TEXT("Public level"), Public.Level, 1);
	TestEqual(TEXT("No private experience"), Public.Experience, int64(0));
	TestEqual(TEXT("No private skill points"), Public.UnspentAbilityPoints, 0);
	TestEqual(TEXT("No inventory revision"), Public.InventoryRevision, 0);
	if (!TestEqual(TEXT("Public ability exists"), Public.Abilities.Num(), 1)) return false;
	TestEqual(TEXT("Public ability identity"), Public.Abilities[0].DefinitionId, Ability->GetPrimaryAssetId());
	TestFalse(TEXT("No ability operation handle"), Public.Abilities[0].SpecHandle.IsValid());
	TestFalse(TEXT("No upgrade permission"), Public.Abilities[0].bCanUpgrade);
	TestEqual(TEXT("No cooldown window"), Public.Abilities[0].CooldownEndTime, 0.0);
	if (!TestEqual(TEXT("All public inventory slots retained"), Public.Items.Num(), 9)) return false;
	TestEqual(TEXT("Public item identity"), Public.Items[0].DefinitionId, Item->GetPrimaryAssetId());
	TestFalse(TEXT("No item operation identity"), Public.Items[0].Handle.IsValid());
	TestEqual(TEXT("No item revision"), Public.Items[0].Revision, 0);
	Player->SelectCombatUnit(Unit);
	UClass* HUDClass = LoadClass<UCombatHUDWidget>(nullptr, TEXT("/Game/Combat/Demo/UI/WBP_CombatHUD.WBP_CombatHUD_C"));
	if (!TestNotNull(TEXT("Real HUD Blueprint class loads"), HUDClass)) return false;
	auto* Widget = NewObject<UCombatHUDWidget>(Player, HUDClass);
	Widget->Initialize();
	Widget->SetOwningPlayer(Player);
	if (!TestNotNull(TEXT("Real HUD loads"), Widget)) return false;
	auto Slate = Widget->TakeWidget();
	Widget->InitializeForUnit(Unit);
	TestEqual(TEXT("HUD displays public hero"), Widget->GetDisplaySnapshot().UnitDefinitionId, Unit->GetUnitDefinitionId());
	TestFalse(TEXT("HUD is read-only for this viewer"), Widget->CanOperateObservedUnit());
	Widget->ReleaseSlateResources(true);
	return true;
}

/** 撤销非主选控制权不能清空另一个主选；单位和 Controller teardown 清掉全组 Owner。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatSelectionLifecycleTest, "Combat.Input.Selection.ControlLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCombatSelectionLifecycleTest::RunTest(const FString& Parameters)
{
	using namespace CombatSelectionTests;
	FCombatAutomationWorldFixture Fixture;
	UWorld* World = Fixture.GetWorld();
	if (!World) return false;
	auto* Player = World->SpawnActor<ACombatPlayerController>();
	Player->SetAsLocalPlayerController();
	if (!Player->HasActorBegunPlay()) Player->DispatchBeginPlay();
	auto* Other = World->SpawnActor<ACombatPlayerController>();
	auto* A = Spawn(World, 1);
	auto* B = Spawn(World, 1);
	auto* C = Spawn(World, 1);
	if (!A || !B || !C) return false;
	Player->SetCommandedUnitAuthority(A);
	Player->GrantUnitControlAuthority(B);
	Player->GrantUnitControlAuthority(C);
	Player->SelectCombatUnits({A, B, C}, false);
	Other->SetCommandedUnitAuthority(B);
	Player->RefreshLocalSelection();
	TestEqual(TEXT("Transferring non-primary preserves main hero"), Player->GetCommandedUnit(), A);
	TestEqual(TEXT("Revoked member removed"), Player->GetSelectedUnits().Num(), 2);
	A->Destroy();
	Player->RefreshLocalSelection();
	TestEqual(TEXT("EndPlay selects surviving owned hero"), Player->GetInspectedUnit(), C);
	Player->Destroy();
	TestNull(TEXT("All remaining owned heroes released"), C->GetCommandingPlayerController());
	TestNull(TEXT("Resource anchor released"), C->GetResourceOwnerPlayerController());
	TestEqual(TEXT("Other player's hero unchanged"), B->GetCommandingPlayerController(), static_cast<APlayerController*>(Other));
	return true;
}
#endif
