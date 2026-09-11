#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Components/CapsuleComponent.h"
#include "EnhancedInputComponent.h"
#include "Engine/World.h"
#include "InputMappingContext.h"
#include "Misc/AutomationTest.h"

#include "Combat/Ability/CombatAbilitySystemComponent.h"
#include "Combat/Attributes/CombatAttributeSet.h"
#include "Combat/Core/CombatTags.h"
#include "Combat/Data/CombatDefinitionData.h"
#include "Combat/Demo/CombatDemoAbilities.h"
#include "Combat/Order/CombatOrderComponent.h"
#include "Combat/Scheduling/CombatSchedulerSubsystem.h"
#include "Combat/Tests/CombatAutomationWorldFixture.h"
#include "Combat/Unit/CombatUnitCharacter.h"
#include "Combat/Unit/CombatUnitLifecycleComponent.h"
#include "ue_gasPlayerController.h"

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
	Aue_gasPlayerController* PC = World.SpawnActor<Aue_gasPlayerController>();
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
	UClass* ControllerClass = LoadClass<Aue_gasPlayerController>(nullptr,
		TEXT("/Game/Combat/Demo/Framework/BP_CombatDemoPlayerController.BP_CombatDemoPlayerController_C"));
	if (!TestNotNull(TEXT("Demo controller blueprint exists"), ControllerClass)) { return false; }
	Aue_gasPlayerController* PC = World.SpawnActor<Aue_gasPlayerController>(ControllerClass);
	ACombatUnitCharacter* Unit = SpawnUnit(World, FVector::ZeroVector, 1);
	ACombatUnitCharacter* Enemy = SpawnUnit(World, FVector(150, 0, 0), 2);
	ACombatUnitCharacter* Friend = SpawnUnit(World, FVector(0, 300, 0), 1);
	if (!PC || !Unit || !Enemy || !Friend) { return false; }
	PC->SetCommandedUnitAuthority(Unit);
	UEnhancedInputComponent* Input = NewObject<UEnhancedInputComponent>(PC);
	PC->InputComponent = Input;
	PC->BindCombatCommandActions(*Input);
	TestEqual(TEXT("No legacy physical key bindings"), Input->KeyBindings.Num(), 0);
	TestEqual(TEXT("Four command actions are bound"), Input->GetActionEventBindings().Num(), 4);
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
	Aue_gasPlayerController* PC = World.SpawnActor<Aue_gasPlayerController>();
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

#endif
