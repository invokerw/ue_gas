#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Components/CapsuleComponent.h"
#include "Components/BoxComponent.h"
#include "EnhancedInputComponent.h"
#include "Engine/World.h"
#include "InputMappingContext.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#include "Combat/Ability/CombatAbilitySystemComponent.h"
#include "Combat/Attributes/CombatAttributeSet.h"
#include "Combat/Core/CombatTags.h"
#include "Combat/Data/CombatDefinitionData.h"
#include "Combat/Demo/CombatDemoAbilities.h"
#include "Combat/Order/CombatOrderComponent.h"
#include "Combat/Scheduling/CombatSchedulerSubsystem.h"
#include "Combat/Tests/CombatAutomationWorldFixture.h"
#include "Combat/UI/CombatAbilityIndicatorGround.h"
#include "Combat/Items/CombatInventoryComponent.h"
#include "Combat/Items/CombatItemData.h"
#include "Combat/Unit/CombatUnitCharacter.h"
#include "Combat/Unit/CombatUnitLifecycleComponent.h"
#include "Combat/View/CombatUnitViewComponent.h"
#include "CombatPlayerController.h"

namespace CombatPlayerInputTests
{
	/** 创建真实 ASC/Order/Attack 单位；只将导航完成延后，输入、RPC 安全层和伤害仍走生产路径。 */
	ACombatUnitCharacter* SpawnUnit(UWorld& World, const FVector& Location, const uint8 Team)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		ACombatUnitCharacter* Unit = World.SpawnActor<ACombatUnitCharacter>(Location, FRotator::ZeroRotator, Params);
		if (!Unit) { return nullptr; }
		UCombatUnitData* Data = NewObject<UCombatUnitData>(Unit);
		Data->DefinitionName = TEXT("input_unit");
		Data->InitialTeamId = FCombatTeamId(Team);
		Data->BaseStats.MaxHealth = 200.0f;
		Data->BaseStats.AttackDamage = 20.0f;
		Data->BaseStats.BaseAttackTime = 1.0f;
		Data->BaseAttackPoint = 0.20f;
		if (!Unit->InitializeFromUnitData(Data)) { return nullptr; }
		Unit->GetCombatOrderComponent()->SetNavigationDeferredForTesting(true);
		return Unit;
	}

	/** 构造已经由光标射线得到的单位命中，供输入路由使用。 */
	FHitResult HitUnit(ACombatUnitCharacter* Unit)
	{
		FHitResult Hit(Unit, Unit->GetCapsuleComponent(), Unit->GetActorLocation(), FVector::UpVector);
		Hit.bBlockingHit = true;
		return Hit;
	}

	/** 构造地面命中；即使附近有敌人也不能变成单位攻击。 */
	FHitResult HitGround(const FVector& Location)
	{
		FHitResult Hit;
		Hit.bBlockingHit = true;
		Hit.Location = Location;
		return Hit;
	}

	/** 派发实际配置的 Action Started 事件，验证资产引用与输入状态机的连通性。 */
	bool StartAction(UEnhancedInputComponent& Input, const UInputAction* Action)
	{
		for (const TUniquePtr<FEnhancedInputActionEventBinding>& Binding : Input.GetActionEventBindings())
		{
			if (Action && Binding->GetAction() == Action && Binding->GetTriggerEvent() == ETriggerEvent::Started)
			{
				Binding->Execute(FInputActionInstance(Action));
				return true;
			}
		}
		return false;
	}
}

/** 验证右键普攻经 RPC 进入连续攻击，拖动/松开不覆盖，点地面移动且远距离追击。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatPlayerAttackInputTest,
	"Combat.Input.Attack.RightClickAndContinuousOrder",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCombatPlayerAttackInputTest::RunTest(const FString& Parameters)
{
	using namespace CombatPlayerInputTests;
	FCombatAutomationWorldFixture Fixture;
	if (!Fixture.IsValid()) { return false; }
	UWorld& World = *Fixture.GetWorld();
	ACombatPlayerController* PC = World.SpawnActor<ACombatPlayerController>();
	ACombatUnitCharacter* Unit = SpawnUnit(World, FVector::ZeroVector, 1);
	ACombatUnitCharacter* Enemy = SpawnUnit(World, FVector(150, 0, 0), 2);
	ACombatUnitCharacter* FarEnemy = SpawnUnit(World, FVector(1500, 0, 0), 2);
	if (!PC || !Unit || !Enemy || !FarEnemy) { return false; }
	TestTrue(TEXT("Player owns commanded unit"), PC->SetCommandedUnitAuthority(Unit));
	UCombatOrderComponent* Orders = Unit->GetCombatOrderComponent();
	PC->BeginDestinationInput(HitUnit(Enemy));
	TestEqual(TEXT("Right click submits one accepted order"), Unit->GetLastOrderBatchResult().AcceptedOrderCount, 1);
	const FCombatOrderHandle AttackHandle = Orders->GetCurrentOrderHandle();
	TestTrue(TEXT("Attack remains active"), AttackHandle.IsValid());
	TestFalse(TEXT("Attack gesture cannot drag-move"), PC->bDestinationInputActive);
	const int32 NextRequest = PC->NextCombatOrderRequestId;
	for (int32 Index = 0; Index < 10; ++Index) { PC->OnSetDestinationTriggered(); }
	PC->OnSetDestinationReleased();
	TestEqual(TEXT("Holding and releasing attack sends no move RPC"), PC->NextCombatOrderRequestId, NextRequest);
	World.Tick(LEVELTICK_All, 0.25f);
	TestEqual(TEXT("First normal attack applies damage"), Enemy->GetCombatAbilitySystemComponent()->GetNumericAttribute(
		UCombatAttributeSet::GetHealthAttribute()), 180.0f);
	TestEqual(TEXT("Landed does not release continuous order"), Orders->GetCurrentOrderHandle(), AttackHandle);
	UCombatSchedulerSubsystem* Scheduler = World.GetSubsystem<UCombatSchedulerSubsystem>();
	Scheduler->RunDueTasks(World.GetTimeSeconds() + 2.0);
	Scheduler->RunDueTasks(World.GetTimeSeconds() + 2.0);
	TestEqual(TEXT("Second attack needs no further input"), Enemy->GetCombatAbilitySystemComponent()->GetNumericAttribute(
		UCombatAttributeSet::GetHealthAttribute()), 160.0f);

	PC->OnStopCommand();
	TestEqual(TEXT("Stop ends continuous attack"), Orders->GetCurrentState(), ECombatOrderState::Idle);
	World.Tick(LEVELTICK_All, 2.0f);
	TestEqual(TEXT("Stop prevents later damage"), Enemy->GetCombatAbilitySystemComponent()->GetNumericAttribute(
		UCombatAttributeSet::GetHealthAttribute()), 160.0f);
	PC->BeginDestinationInput(HitGround(FVector(160, 30, 0)));
	TestEqual(TEXT("Ground near enemy stays a move"), Orders->GetCurrentState(), ECombatOrderState::Moving);
	PC->BeginDestinationInput(HitUnit(FarEnemy));
	TestEqual(TEXT("Out-of-range target enters server chase"), Orders->GetCurrentState(), ECombatOrderState::Chasing);
	TestFalse(TEXT("Chase gesture cannot become drag move"), PC->bDestinationInputActive);
	TestEqual(TEXT("Chase resolves clicked target position"), Orders->GetCurrentMoveGoal(), FarEnemy->GetActorLocation());
	return true;
}

/** 右键菜单不再提供放地面旁路，拖拽屏幕落点入口必须保留。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatPlayerItemDropPathTest,
	"Combat.Input.ItemDrop.RightClickMenuRemovedAndDragRetained",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCombatPlayerItemDropPathTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace CombatPlayerInputTests;
	const FString SlotSourcePath = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir(),
		TEXT("Source/Combat/Combat/UI/CombatHUDItemSlotWidget.cpp"));
	const FString InputSourcePath = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir(),
		TEXT("Source/Combat/Combat/Items/CombatItemInput.cpp"));
	const FString HUDSourcePath = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir(),
		TEXT("Source/Combat/Combat/UI/CombatHUDWidget.cpp"));
	const FString ControllerHeaderPath = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir(),
		TEXT("Source/Combat/CombatPlayerController.h"));
	FString SlotSource, InputSource, HUDSource, ControllerHeader;
	if (!TestTrue(TEXT("Item slot source is available for the menu contract scan"),
		FFileHelper::LoadFileToString(SlotSource, *SlotSourcePath))) return false;
	if (!TestTrue(TEXT("Item input source is available for the drag contract scan"),
		FFileHelper::LoadFileToString(InputSource, *InputSourcePath))) return false;
	if (!TestTrue(TEXT("HUD source is available for the drag route scan"),
		FFileHelper::LoadFileToString(HUDSource, *HUDSourcePath))) return false;
	if (!TestTrue(TEXT("Player controller header is available for the input contract scan"),
		FFileHelper::LoadFileToString(ControllerHeader, *ControllerHeaderPath))) return false;
	TestFalse(TEXT("Right-click menu has no ground-drop entry"), SlotSource.Contains(
		TEXT("AddMenuEntry(NSLOCTEXT(\"CombatItems\", \"Drop\", \"放到地面\")")));
	TestFalse(TEXT("Right-click immediate drop API is removed from the controller"),
		ControllerHeader.Contains(TEXT("BeginDropInventoryItem")));
	TestFalse(TEXT("Near-feet drop helper is removed from item input"), InputSource.Contains(TEXT("BuildDropLocationNearFeet")));
	TestFalse(TEXT("Near-feet drop constant is removed from item input"), InputSource.Contains(TEXT("DropNearFeetDistance")));
	TestTrue(TEXT("Screen-position drag drop path remains"), InputSource.Contains(TEXT("DropInventoryItemAtScreenPosition")));
	TestTrue(TEXT("HUD drag release still routes to screen-position drop"), HUDSource.Contains(TEXT("DropInventoryItemAtScreenPosition")));

	FCombatAutomationWorldFixture Fixture;
	if (!Fixture.IsValid()) return false;
	UWorld& World = *Fixture.GetWorld();
	ACombatPlayerController* PC = World.SpawnActor<ACombatPlayerController>();
	ACombatUnitCharacter* Unit = SpawnUnit(World, FVector(100.0f, 200.0f, 0.0f), 1);
	if (!PC || !Unit || !PC->SetCommandedUnitAuthority(Unit)) return false;
	PC->SetAsLocalPlayerController();
	UCombatItemData* Data = NewObject<UCombatItemData>(Unit);
	Data->DefinitionName = TEXT("input_drop");
	Data->bCanDrop = true;
	FCombatItemHandle Handle;
	FGameplayTag Failure;
	if (!TestTrue(TEXT("Drop item enters inventory"), Unit->GetCombatInventoryComponent()->GiveItem(Data, 1, Handle, Failure))) return false;
	const FCombatHUDOwnerView View = Unit->GetCombatUnitViewComponent()->GetHUDOwnerView();
	if (!TestEqual(TEXT("Inventory snapshot has dropped item"), View.Items.Num(), CombatItems::TotalSlots)) return false;
	const FCombatItemView* Item = View.Items.FindByPredicate([Handle](const FCombatItemView& Entry) { return Entry.Handle == Handle; });
	if (!TestNotNull(TEXT("Drop item snapshot exists"), Item)) return false;
	const int32 Before = PC->NextCombatOrderRequestId;
	TestFalse(TEXT("Invalid drag screen position is rejected locally"),
		PC->DropInventoryItemAtScreenPosition(*Item, FVector2D(-1.0f, -1.0f)));
	TestEqual(TEXT("Rejected drag does not submit an order"), PC->NextCombatOrderRequestId, Before);
	TestEqual(TEXT("Rejected drag keeps the item"), Unit->GetCombatInventoryComponent()->GetItemAt(0), Handle);
	return true;
}

/** 验证 A/左键选敌、无效目标、S/Escape、技能与控制权切换清理，不让旧手势给新单位发命令。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatPlayerAttackInputCancellationTest,
	"Combat.Input.Attack.TargetSelectionAndCancellation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCombatPlayerAttackInputCancellationTest::RunTest(const FString& Parameters)
{
	using namespace CombatPlayerInputTests;
	FCombatAutomationWorldFixture Fixture;
	if (!Fixture.IsValid()) { return false; }
	UWorld& World = *Fixture.GetWorld();
	UClass* ControllerClass = LoadClass<ACombatPlayerController>(nullptr,
		TEXT("/Game/Combat/Demo/Framework/BP_CombatDemoPlayerController.BP_CombatDemoPlayerController_C"));
	if (!TestNotNull(TEXT("Demo controller blueprint exists"), ControllerClass)) { return false; }
	ACombatPlayerController* PC = World.SpawnActor<ACombatPlayerController>(ControllerClass);
	ACombatUnitCharacter* Unit = SpawnUnit(World, FVector::ZeroVector, 1);
	ACombatUnitCharacter* Enemy = SpawnUnit(World, FVector(150, 0, 0), 2);
	ACombatUnitCharacter* Friend = SpawnUnit(World, FVector(0, 300, 0), 1);
	if (!PC || !Unit || !Enemy || !Friend) { return false; }
	PC->SetCommandedUnitAuthority(Unit);
	UEnhancedInputComponent* Input = NewObject<UEnhancedInputComponent>(PC);
	PC->InputComponent = Input;
	PC->BindCombatCommandActions(*Input);
	TestEqual(TEXT("No legacy physical key bindings"), Input->KeyBindings.Num(), 0);
	TestEqual(TEXT("Command actions include selection drag lifecycle and optional Shift"),
		Input->GetActionEventBindings().Num(), PC->AddToSelectionAction ? 10 : 7);
	if (!TestNotNull(TEXT("Demo mapping context is configured"), PC->DefaultMappingContext.Get())) { return false; }
	const TPair<const UInputAction*, FKey> ExpectedMappings[] = {
		{ PC->AttackTargetAction, EKeys::A },
		{ PC->ConfirmAttackTargetAction, EKeys::LeftMouseButton },
		{ PC->CancelAttackTargetAction, EKeys::Escape },
		{ PC->StopCommandAction, EKeys::S }
	};
	for (const auto& Expected : ExpectedMappings)
	{
		if (!TestNotNull(TEXT("Demo command action reference is assigned"), Expected.Key)) { return false; }
		TestEqual(TEXT("Command action is a Boolean input"), Expected.Key->ValueType, EInputActionValueType::Boolean);
		int32 MatchingKeys = 0;
		for (const FEnhancedActionKeyMapping& Mapping : PC->DefaultMappingContext->GetMappings())
		{
			MatchingKeys += Mapping.Action == Expected.Key && Mapping.Key == Expected.Value ? 1 : 0;
		}
		TestEqual(TEXT("Default key is mapped exactly once in IMC_Default"), MatchingKeys, 1);
	}
	TestTrue(TEXT("Normal confirmation is bound"), StartAction(*Input, PC->ConfirmAttackTargetAction));
	TestEqual(TEXT("Normal left click sends no order"), PC->NextCombatOrderRequestId, 1);
	TestTrue(TEXT("Attack selection action is bound"), StartAction(*Input, PC->AttackTargetAction));
	TestTrue(TEXT("A enters targeting"), PC->bAttackTargeting);
	TestEqual(TEXT("A displays crosshair"), PC->CurrentMouseCursor.GetValue(), EMouseCursor::Crosshairs);
	PC->ConfirmAttackTarget(HitGround(FVector(150, 20, 0)));
	PC->ConfirmAttackTarget(HitUnit(Friend));
	PC->ConfirmAttackTarget(HitUnit(Unit));
	Enemy->GetCombatAbilitySystemComponent()->AddLooseGameplayTag(CombatTags::State_Untargetable);
	PC->ConfirmAttackTarget(HitUnit(Enemy));
	Enemy->GetCombatAbilitySystemComponent()->RemoveLooseGameplayTag(CombatTags::State_Untargetable);
	TestEqual(TEXT("Ground, friendly, self and untargetable do not send attacks"), PC->NextCombatOrderRequestId, 1);
	TestTrue(TEXT("Invalid confirmation keeps targeting"), PC->bAttackTargeting);
	PC->ConfirmAttackTarget(HitUnit(Enemy));
	TestEqual(TEXT("A confirmation accepted by server"), Unit->GetLastOrderBatchResult().AcceptedOrderCount, 1);
	TestFalse(TEXT("Successful confirmation exits targeting"), PC->bAttackTargeting);
	TestTrue(TEXT("Stop action is bound"), StartAction(*Input, PC->StopCommandAction));
	TestEqual(TEXT("S stops orders"), Unit->GetCombatOrderComponent()->GetCurrentState(), ECombatOrderState::Idle);
	StartAction(*Input, PC->AttackTargetAction);
	const int32 BeforeEscape = PC->NextCombatOrderRequestId;
	TestTrue(TEXT("Cancel action is bound"), StartAction(*Input, PC->CancelAttackTargetAction));
	TestFalse(TEXT("Escape cancels selection"), PC->bAttackTargeting);
	TestEqual(TEXT("Escape sends no gameplay command"), PC->NextCombatOrderRequestId, BeforeEscape);

	PC->BeginDestinationInput(HitGround(FVector(500, 0, 0)));
	StartAction(*Input, PC->StopCommandAction);
	const int32 AfterStop = PC->NextCombatOrderRequestId;
	PC->OnSetDestinationTriggered();
	PC->OnSetDestinationReleased();
	TestEqual(TEXT("Old held movement cannot undo S"), PC->NextCombatOrderRequestId, AfterStop);
	StartAction(*Input, PC->AttackTargetAction);
	PC->OnAbilitySlotQ();
	TestFalse(TEXT("Ability input cancels attack selection"), PC->bAttackTargeting);
	StartAction(*Input, PC->AttackTargetAction);
	PC->BeginDestinationInput(HitGround(FVector(500, 0, 0)));
	TestFalse(TEXT("Right click cancels selection"), PC->bAttackTargeting);
	PC->SetCommandedUnitAuthority(Friend);
	const int32 AfterTransfer = PC->NextCombatOrderRequestId;
	PC->OnSetDestinationTriggered();
	PC->OnSetDestinationReleased();
	TestEqual(TEXT("Control transfer discards old movement gesture"), PC->NextCombatOrderRequestId, AfterTransfer);
	PC->OnAttackTargetingStarted();
	// 最小测试 World 不保证给运行时生成的 Actor 派发 BeginPlay；补齐后才会走真实 Destroy -> EndPlay。
	if (!Friend->HasActorBegunPlay()) { Friend->DispatchBeginPlay(); }
	TestTrue(TEXT("World destroys commanded unit"), World.DestroyActor(Friend, true));
	World.Tick(LEVELTICK_All, 0.0f);
	TestFalse(TEXT("Unit EndPlay clears targeting"), PC->bAttackTargeting);
	TestFalse(TEXT("No ready unit cannot submit attack"), PC->IssueCombatAttackOrder(Enemy));
	PC->SetCommandedUnitAuthority(Unit);
	FCombatEventContext DeathEvent;
	Enemy->GetCombatLifecycleComponent()->RequestDeath(DeathEvent, Unit);
	TestFalse(TEXT("Dead target cannot be attacked"), PC->IssueCombatAttackOrder(Enemy));
	return true;
}

/** AutoCast 被动占用技能槽时，快捷键只切换法球状态，不生成无目标施法命令。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatPlayerAutoCastAbilityInputTest,
	"Combat.Input.Ability.PassiveAutoCastToggle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCombatPlayerAutoCastAbilityInputTest::RunTest(const FString& Parameters)
{
	using namespace CombatPlayerInputTests;
	(void)Parameters;
	FCombatAutomationWorldFixture Fixture;
	if (!Fixture.IsValid()) { return false; }
	UWorld& World = *Fixture.GetWorld();
	ACombatPlayerController* PC = World.SpawnActor<ACombatPlayerController>();
	ACombatUnitCharacter* Unit = SpawnUnit(World, FVector::ZeroVector, 1);
	if (!PC || !Unit || !TestTrue(TEXT("Player owns commanded unit"), PC->SetCommandedUnitAuthority(Unit)))
	{
		return false;
	}

	UCombatAbilityData* Data = NewObject<UCombatAbilityData>(Unit);
	Data->DefinitionName = TEXT("input_autocast");
	Data->BehaviorTags.AddTag(CombatTags::Ability_Behavior_NoTarget);
	Data->BehaviorTags.AddTag(CombatTags::Ability_Behavior_Passive);
	Data->BehaviorTags.AddTag(CombatTags::Ability_Behavior_AutoCast);
	Data->TargetingRules.TargetTeamTag = CombatTags::TargetTeam_None;
	TGuardValue<TObjectPtr<UCombatAbilityData>> RestoreData(
		GetMutableDefault<UCombatFrostArrowsAbility>()->AbilityData, Data);
	UCombatAbilitySystemComponent* Asc = Unit->GetCombatAbilitySystemComponent();
	FGameplayAbilitySpecHandle Handle;
	FGameplayTag Failure;
	if (!TestTrue(TEXT("Grant passive AutoCast"), Asc->GrantCombatAbility(
		UCombatFrostArrowsAbility::StaticClass(), 1, true, Handle, Failure)))
	{
		return false;
	}

	const int32 RequestIdBeforeToggle = PC->NextCombatOrderRequestId;
	PC->OnAbilitySlotQ();
	TestFalse(TEXT("First Q press disables AutoCast"), Asc->IsAutoCastEnabled(Handle));
	TestEqual(TEXT("AutoCast toggle sends no Cast Order"), PC->NextCombatOrderRequestId, RequestIdBeforeToggle);
	PC->OnAbilitySlotQ();
	TestTrue(TEXT("Second Q press enables AutoCast"), Asc->IsAutoCastEnabled(Handle));
	TestEqual(TEXT("Repeated AutoCast toggle still sends no Cast Order"), PC->NextCombatOrderRequestId, RequestIdBeforeToggle);
	return true;
}

/** 标准施法先进入本地瞄准；右键仅取消本次瞄准，不产生移动请求。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatAbilityAimInputTest,
	"Combat.Input.AbilityAim.StandardAndCancel",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCombatAbilityAimInputTest::RunTest(const FString& Parameters)
{
	using namespace CombatPlayerInputTests;
	FCombatAutomationWorldFixture Fixture;
	if (!Fixture.IsValid()) { return false; }
	UWorld& World = *Fixture.GetWorld();
	ACombatPlayerController* PC = World.SpawnActor<ACombatPlayerController>();
	ACombatUnitCharacter* Unit = SpawnUnit(World, FVector::ZeroVector, 1);
	if (!PC || !Unit || !PC->SetCommandedUnitAuthority(Unit)) { return false; }
	// 临时 PIE World 没有 GameMode/LocalPlayer，由测试明确标记拥有本地输入的 Controller。
	PC->SetAsLocalPlayerController();
	UCombatAbilityData* Data = NewObject<UCombatAbilityData>(Unit);
	Data->DefinitionName = TEXT("aim_point");
	Data->BehaviorTags.AddTag(CombatTags::Ability_Behavior_PointTarget);
	Data->TargetingRules.TargetTeamTag = CombatTags::TargetTeam_None;
	Data->TargetingRules.CastRange = 500.0f;
	TGuardValue<TObjectPtr<UCombatAbilityData>> RestoreData(GetMutableDefault<UCombatPointAoeAbility>()->AbilityData, Data);
	FGameplayAbilitySpecHandle Handle;
	FGameplayTag Failure;
	if (!TestTrue(TEXT("Grant point skill"), Unit->GetCombatAbilitySystemComponent()->GrantCombatAbility(
		UCombatPointAoeAbility::StaticClass(), 1, false, Handle, Failure))) { return false; }
	const int32 Before = PC->NextCombatOrderRequestId;
	PC->OnAbilitySlotQ();
	TestEqual(TEXT("Standard input enters aiming crosshair"), PC->CurrentMouseCursor.GetValue(), EMouseCursor::Crosshairs);
	TestEqual(TEXT("Press does not submit targeted Cast"), PC->NextCombatOrderRequestId, Before);
	PC->BeginDestinationInput(HitGround(FVector(800, 0, 0)));
	PC->OnSetDestinationTriggered();
	PC->OnSetDestinationReleased();
	TestEqual(TEXT("Right gesture only cancels aim"), PC->NextCombatOrderRequestId, Before);
	TestEqual(TEXT("Cancel restores cursor"), PC->CurrentMouseCursor.GetValue(), EMouseCursor::Default);
	UCombatAbilityAimComponent* Aim = PC->GetAbilityAimComponent();
	UCombatAbilitySystemComponent* Asc = Unit->GetCombatAbilitySystemComponent();
	UCombatUnitViewComponent* View = Unit->GetCombatUnitViewComponent();
	Asc->SetNumericAttributeBase(UCombatAttributeSet::GetCastRangeBonusAttribute(), 75.0f);
	View->RefreshUnitView();
	View->RefreshHUDOwnerView();
	PC->OnAbilitySlotQ();
	const uint64 FirstSerial = Aim->GetSessionSerial();
	// 点目标命中现在必须携带真实地面组件，不能用任意坐标伪造有效落点。
	AActor* Floor = World.SpawnActor<AActor>();
	UBoxComponent* Surface = NewObject<UBoxComponent>(Floor);
	Floor->SetRootComponent(Surface);
	Surface->SetBoxExtent(FVector(100, 100, 10));
	Surface->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Surface->SetCollisionResponseToAllChannels(ECR_Ignore);
	Surface->SetCollisionResponseToChannel(CombatAbilityIndicatorGround::TraceChannel, ECR_Block);
	Surface->SetRenderCustomDepth(true);
	Surface->SetCustomDepthStencilValue(CombatAbilityIndicatorGround::StencilBit);
	Surface->RegisterComponent();
	Floor->SetActorLocation(FVector(1700, 25, -10));
	FHitResult FarPoint;
	TestTrue(TEXT("Trace real marked floor"), World.LineTraceSingleByChannel(FarPoint,
		FVector(1700, 25, 100), FVector(1700, 25, -100), CombatAbilityIndicatorGround::TraceChannel));
	Aim->UpdatePreview(FarPoint, true);
	TestEqual(TEXT("Far point is amber, still requestable"), Aim->GetPreview().Status, ECombatAbilityAimStatus::OutOfRange);
	TestEqual(TEXT("Cast circle includes owner bonus and source capsule"), Aim->GetPreview().CastRadius,
		575.0f + Unit->GetCapsuleComponent()->GetScaledCapsuleRadius());
	FCombatOrderRequest PreviewOrder;
	TestFalse(TEXT("Hero body cannot become a point target"), Aim->BuildConfirmedOrder(FirstSerial, HitUnit(Unit), true, PreviewOrder));
	TestFalse(TEXT("Rejected hero hit clears the old ground preview"), Aim->GetPreview().bHasTarget);
	FHitResult SteepHit = FarPoint;
	SteepHit.ImpactNormal = FVector::ForwardVector;
	TestFalse(TEXT("Platform side cannot become ground"), Aim->BuildConfirmedOrder(FirstSerial, SteepHit, true, PreviewOrder));
	Surface->SetRenderCustomDepth(false);
	TestFalse(TEXT("Unmarked surface cannot confirm"), Aim->BuildConfirmedOrder(FirstSerial, FarPoint, true, PreviewOrder));
	Surface->SetRenderCustomDepth(true);
	TestFalse(TEXT("UI confirmation cannot reuse ground underneath"), Aim->BuildConfirmedOrder(FirstSerial, FarPoint, false, PreviewOrder));
	TestFalse(TEXT("Missing hit cannot reuse old point"), Aim->BuildConfirmedOrder(FirstSerial, FHitResult(), true, PreviewOrder));
	TestTrue(TEXT("Out of range confirmation preserves original request"), Aim->BuildConfirmedOrder(FirstSerial, FarPoint, true, PreviewOrder));
	TestTrue(TEXT("Point is not clamped"), PreviewOrder.TargetLocation.Equals(FarPoint.Location));
	PC->ConfirmAbilityTarget(FarPoint, FirstSerial);
	TestEqual(TEXT("Exactly one request"), PC->NextCombatOrderRequestId, Before + 1);
	TestEqual(TEXT("Server enters chase for original point"), Unit->GetCombatOrderComponent()->GetCurrentState(), ECombatOrderState::Chasing);
	TestFalse(TEXT("Submission ends local aim"), Aim->IsAiming());
	PC->ConfirmAbilityTarget(FarPoint, FirstSerial);
	TestEqual(TEXT("Repeated confirmation never resends"), PC->NextCombatOrderRequestId, Before + 1);
	TestEqual(TEXT("Accepted receipt does not claim spell success"), Aim->GetStatusText().ToString(), FString(TEXT("指令已接收")));
	PC->OnAbilitySlotQ();
	const uint64 SecondSerial = Aim->GetSessionSerial();
	TestFalse(TEXT("Old session cannot confirm new selection"), Aim->BuildConfirmedOrder(FirstSerial, FarPoint, true, PreviewOrder));
	Asc->AddLooseGameplayTag(CombatTags::State_Silenced);
	View->RefreshUnitView();
	Aim->UpdatePreview(FarPoint, true);
	TestEqual(TEXT("Silence blocks even an out of range target"), Aim->GetPreview().Status, ECombatAbilityAimStatus::Blocked);
	TestFalse(TEXT("Silence submits no request"), Aim->BuildConfirmedOrder(SecondSerial, FarPoint, true, PreviewOrder));
	Asc->RemoveLooseGameplayTag(CombatTags::State_Silenced);
	View->RefreshUnitView();
	PC->AbilityCastMode = ECombatAbilityCastMode::QuickRelease;
	PC->OnAbilitySlotQ();
	PC->OnAbilityInputCanceled(0);
	PC->OnAbilitySlotReleased(0);
	TestFalse(TEXT("Canceled input cannot become a release cast"), Aim->IsAiming());
	TestEqual(TEXT("Canceled release sent nothing"), PC->NextCombatOrderRequestId, Before + 1);
	PC->OnAbilitySlotQ();
	PC->OnAbilitySlotW();
	PC->OnAbilitySlotReleased(0);
	TestFalse(TEXT("Empty slot cancels previous aim and stale release"), Aim->IsAiming());
	PC->OnAbilitySlotQ();
	PC->OnAttackTargetingStarted();
	TestFalse(TEXT("Attack selection replaces skill aim"), Aim->IsAiming());
	PC->AbilityCastMode = ECombatAbilityCastMode::Standard;
	PC->OnAbilitySlotQ();
	Aim->ResetLocalState();
	TestFalse(TEXT("Focus reset cleans up local aim"), Aim->IsAiming());
	PC->OnAbilitySlotQ();
	TestTrue(TEXT("Remove currently aimed skill"), Asc->RemoveCombatAbility(Handle, Failure));
	Aim->UpdatePreview(FarPoint, true);
	TestFalse(TEXT("Revoked skill discards its aim"), Aim->IsAiming());
	TestTrue(TEXT("Regrant skill"), Asc->GrantCombatAbility(UCombatPointAoeAbility::StaticClass(), 1, false, Handle, Failure));
	View->RefreshHUDOwnerView();
	PC->OnAbilitySlotQ();
	// HUD 点击先排到下一帧，等待视口焦点切换触发的 FlushPressedKeys 收尾，不能被该清理误取消。
	PC->CancelCombatTargeting();
	PC->ActivateCombatAbilitySlotFromHUD(0);
	PC->FlushPressedKeys();
	TestFalse(TEXT("Queued HUD skill waits through focus flush"), Aim->IsAiming());
	World.Tick(LEVELTICK_All, 0.01f);
	World.GetTimerManager().Tick(0.01f);
	TestTrue(TEXT("Queued HUD skill starts on next tick"), Aim->IsAiming());
	PC->CancelCombatTargeting();
	FCombatEventContext Death;
	Unit->GetCombatLifecycleComponent()->RequestDeath(Death, nullptr);
	Aim->UpdatePreview(FarPoint, true);
	TestFalse(TEXT("Death invalidates aim"), Aim->IsAiming());
	TestTrue(TEXT("No stale shape after death"), !Aim->GetPreview().bVisible);
	return true;
}

/** 单位施法只认命中 Actor，资源与冷却预检不提交，旧 Owner/EndPlay 不能保留本地会话。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatAbilityUnitAimTest,
	"Combat.Input.AbilityAim.UnitResourcesAndOwnership",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCombatAbilityUnitAimTest::RunTest(const FString& Parameters)
{
	using namespace CombatPlayerInputTests;
	FCombatAutomationWorldFixture Fixture;
	if (!Fixture.IsValid()) return false;
	UWorld& World = *Fixture.GetWorld();
	ACombatPlayerController* PC = World.SpawnActor<ACombatPlayerController>();
	ACombatUnitCharacter* Unit = SpawnUnit(World, FVector::ZeroVector, 1);
	ACombatUnitCharacter* Enemy = SpawnUnit(World, FVector(150, 0, 0), 2);
	ACombatUnitCharacter* Friendly = SpawnUnit(World, FVector(180, 0, 0), 1);
	if (!PC || !Unit || !Enemy || !Friendly || !PC->SetCommandedUnitAuthority(Unit)) return false;
	PC->SetAsLocalPlayerController();
	UCombatAbilityData* Data = NewObject<UCombatAbilityData>(Unit);
	Data->DefinitionName = TEXT("aim_unit");
	Data->BehaviorTags.AddTag(CombatTags::Ability_Behavior_UnitTarget);
	Data->TargetingRules.TargetTeamTag = CombatTags::TargetTeam_Enemy;
	Data->TargetingRules.CastRange = 500.0f;
	Data->SpecialValues.FindOrAdd(TEXT("mana_cost")).Values = { 30.0f };
	Data->SpecialValues.FindOrAdd(TEXT("cooldown")).Values = { 5.0f };
	TGuardValue<TObjectPtr<UCombatAbilityData>> RestoreData(GetMutableDefault<UCombatUnitDamageAbility>()->AbilityData, Data);
	UCombatAbilitySystemComponent* Asc = Unit->GetCombatAbilitySystemComponent();
	FGameplayAbilitySpecHandle Handle;
	FGameplayTag Failure;
	if (!Asc->GrantCombatAbility(UCombatUnitDamageAbility::StaticClass(), 1, false, Handle, Failure)) return false;
	Asc->SetNumericAttributeBase(UCombatAttributeSet::GetMaxManaAttribute(), 100.0f);
	Asc->SetNumericAttributeBase(UCombatAttributeSet::GetManaAttribute(), 100.0f);
	Unit->GetCombatUnitViewComponent()->RefreshUnitView();
	Unit->GetCombatUnitViewComponent()->RefreshHUDOwnerView();
	PC->OnAbilitySlotQ();
	UCombatAbilityAimComponent* Aim = PC->GetAbilityAimComponent();
	FCombatOrderRequest Order;
	const uint64 Serial = Aim->GetSessionSerial();
	TestFalse(TEXT("Ground near enemy never selects nearest unit"), Aim->BuildConfirmedOrder(Serial, HitGround(Enemy->GetActorLocation()), true, Order));
	TestFalse(TEXT("Friendly target rejected"), Aim->BuildConfirmedOrder(Serial, HitUnit(Friendly), true, Order));
	TestTrue(TEXT("Direct enemy hit accepted for request"), Aim->BuildConfirmedOrder(Serial, HitUnit(Enemy), true, Order));
	TestEqual(TEXT("Request contains only actual hit unit"), Order.TargetUnit.Get(), Enemy);
	Asc->SetNumericAttributeBase(UCombatAttributeSet::GetManaAttribute(), 0.0f);
	Unit->GetCombatUnitViewComponent()->RefreshUnitView();
	TestFalse(TEXT("No mana blocks confirm"), Aim->BuildConfirmedOrder(Serial, HitUnit(Enemy), true, Order));
	TestEqual(TEXT("Mana reason visible"), Aim->GetStatusText().ToString(), FString(TEXT("法力不足")));
	Asc->SetNumericAttributeBase(UCombatAttributeSet::GetManaAttribute(), 100.0f);
	bool CostCommitted = false, CooldownCommitted = false;
	TestTrue(TEXT("Commit real cooldown through public ASC"), Asc->CommitCombatAbilityStage(Handle, *Data, 1,
		ECombatAbilityCommitStage::SpellStarted, CostCommitted, CooldownCommitted, Failure));
	Unit->GetCombatUnitViewComponent()->RefreshUnitView();
	Unit->GetCombatUnitViewComponent()->RefreshHUDOwnerView();
	TestFalse(TEXT("Active cooldown blocks confirm"), Aim->BuildConfirmedOrder(Serial, HitUnit(Enemy), true, Order));
	TestEqual(TEXT("Cooldown reason visible"), Aim->GetStatusText().ToString(), FString(TEXT("技能冷却中")));
	TestEqual(TEXT("Local prechecks never submitted orders"), PC->NextCombatOrderRequestId, 1);
	TestTrue(TEXT("Transfer owner through production binding"), PC->SetCommandedUnitAuthority(Friendly));
	TestFalse(TEXT("Transfer cleans local aim"), Aim->IsAiming());
	TestFalse(TEXT("Old owner confirmation fails"), Aim->BuildConfirmedOrder(Serial, HitUnit(Enemy), true, Order));
	TestTrue(TEXT("Restore binding"), PC->SetCommandedUnitAuthority(Unit));
	PC->OnAbilitySlotQ();
	if (!Unit->HasActorBegunPlay()) Unit->DispatchBeginPlay();
	TestTrue(TEXT("Destroy actual owner"), World.DestroyActor(Unit, true));
	TestFalse(TEXT("Owner EndPlay immediately clears aim"), Aim->IsAiming());
	TestNull(TEXT("Owner EndPlay clears commanded pointer"), PC->GetCommandedUnit());
	return true;
}

#endif
