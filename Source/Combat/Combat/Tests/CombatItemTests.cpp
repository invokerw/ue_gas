#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Combat/Tests/CombatAutomationWorldFixture.h"
#include "Combat/Unit/CombatUnitCharacter.h"
#include "Engine/World.h"
#include "Combat/Ability/CombatAbilitySystemComponent.h"
#include "Combat/Ability/CombatGameplayAbility.h"
#include "Combat/Attributes/CombatAttributeSet.h"
#include "Combat/Aura/CombatAuraSubsystem.h"
#include "Combat/Combat/CombatEffectUtilities.h"
#include "Combat/Core/CombatTags.h"
#include "Combat/Data/CombatDefinitionData.h"
#include "Combat/Demo/CombatDemoAbilities.h"
#include "Combat/Items/CombatInventoryComponent.h"
#include "Combat/Items/CombatItemData.h"
#include "Combat/Items/CombatItemSubsystem.h"
#include "Combat/Items/CombatWorldItem.h"
#include "Combat/Log/CombatEventSubsystem.h"
#include "Combat/Modifiers/CombatModifierComponent.h"
#include "Combat/Order/CombatOrderComponent.h"
#include "Combat/Scheduling/CombatSchedulerSubsystem.h"
#include "Combat/Unit/CombatUnitLifecycleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"

namespace CombatItemTests
{
	constexpr auto Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;
	/** 最小单位保持正常组件，关闭物理移动使游戏时间测试不受无地板 fixture 影响。 */
	ACombatUnitCharacter* Spawn(UWorld& World, const FName Name)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		ACombatUnitCharacter* Unit = World.SpawnActor<ACombatUnitCharacter>(FVector::ZeroVector, FRotator::ZeroRotator, Params);
		UCombatUnitData* Data = NewObject<UCombatUnitData>(Unit);
		Data->DefinitionName = Name;
		Data->BaseStats.MaxHealth = 500.0f;
		Data->BaseStats.MaxMana = 200.0f;
		Data->BaseStats.Armor = 0.0f;
		Data->BaseStats.HealthRegen = 0.0f;
		Data->BaseStats.ManaRegen = 0.0f;
		Unit->InitializeFromUnitData(Data);
		if (!Unit->HasActorBegunPlay()) Unit->DispatchBeginPlay();
		Unit->GetCharacterMovement()->DisableMovement();
		return Unit;
	}
	/** 生成无限、不可驱散的护甲被动定义，仍通过真实 Modifier/GE 聚合。 */
	UCombatItemData* Ring(UObject* Outer)
	{
		UCombatItemData* Item = NewObject<UCombatItemData>(Outer);
		Item->DefinitionName = TEXT("test_armor_ring");
		UCombatModifierData* Modifier = NewObject<UCombatModifierData>(Item);
		Modifier->DefinitionName = TEXT("test_item_armor");
		Modifier->DispelRule = ECombatModifierDispelRule::NotDispellable;
		FCombatModifierAttributeChange Change;
		Change.Attribute = UCombatAttributeSet::GetArmorAttribute();
		Change.Magnitude = 5.0f;
		Modifier->AttributeChanges.Add(Change);
		FCombatItemPassive Passive;
		Passive.Modifier = Modifier;
		Item->Passives.Add(Passive);
		return Item;
	}
	/** 创建自疗主动供各实例独立授予，费用与动作都读取同一 AbilityData。 */
	UCombatAbilityData* Heal(UObject* Outer, float Cooldown, float CastPoint = 0.0f)
	{
		UCombatAbilityData* Data = NewObject<UCombatAbilityData>(Outer);
		Data->DefinitionName = TEXT("test_item_heal");
		Data->BehaviorTags.AddTag(CombatTags::Ability_Behavior_NoTarget);
		Data->TargetingRules.TargetTeamTag = CombatTags::TargetTeam_None;
		Data->TargetingRules.bAllowSelf = true;
		Data->CastPoint = CastPoint;
		Data->SpecialValues.Add(TEXT("mana_cost"), FCombatSpecialValue{{10.0f}});
		Data->SpecialValues.Add(TEXT("cooldown"), FCombatSpecialValue{{Cooldown}});
		Data->SpecialValues.Add(TEXT("heal"), FCombatSpecialValue{{40.0f}});
		FCombatAbilityAction Action;
		Action.Type = ECombatAbilityActionType::Heal;
		Action.Target = ECombatAbilityActionTarget::Caster;
		Action.MagnitudeKey = TEXT("heal");
		Data->Actions.Add(Action);
		return Data;
	}
}

/** 物品能力必须随正常战斗单位存在，不能依赖测试地图或 HUD 临时创建权威状态。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatItemFoundationTest, "Combat.Items.Foundation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCombatItemFoundationTest::RunTest(const FString& Parameters)
{
	FCombatItemHandle Written;
	Written.Key.Id = (uint64(1) << 40) + 3;
	Written.Key.Generation = 27;
	TArray<uint8> Bytes;
	FMemoryWriter Writer(Bytes);
	bool bSuccess = false;
	Written.NetSerialize(Writer, nullptr, bSuccess);
	FCombatItemHandle Read;
	FMemoryReader Reader(Bytes);
	Read.NetSerialize(Reader, nullptr, bSuccess);
	TestTrue(TEXT("Item wire format preserves full identity"), bSuccess && Written == Read);
	UClass* InventoryClass = FindObject<UClass>(nullptr, TEXT("/Script/Combat.CombatInventoryComponent"));
	TestNotNull(TEXT("Combat exposes an inventory component"), InventoryClass);
	TestNotNull(TEXT("Combat exposes a stable item definition"), FindObject<UClass>(nullptr, TEXT("/Script/Combat.CombatItemData")));
	TestNotNull(TEXT("Combat exposes a world pickup actor"), FindObject<UClass>(nullptr, TEXT("/Script/Combat.CombatWorldItem")));
	if (!InventoryClass) return false;
	FCombatAutomationWorldFixture Fixture;
	ACombatUnitCharacter* Unit = Fixture.GetWorld()->SpawnActor<ACombatUnitCharacter>();
	TestNotNull(TEXT("Every combat unit owns an inventory"), Unit->GetComponentByClass(InventoryClass));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatItemEquippedTest, "Combat.Items.EquipmentCooldownAndReequip", CombatItemTests::Flags)
bool FCombatItemEquippedTest::RunTest(const FString& Parameters)
{
	FCombatAutomationWorldFixture Fixture;
	UWorld& World = *Fixture.GetWorld();
	ACombatUnitCharacter* Unit = CombatItemTests::Spawn(World, TEXT("item_equipped"));
	UCombatInventoryComponent* Inventory = Unit->GetCombatInventoryComponent();
	UCombatItemSubsystem* Items = World.GetSubsystem<UCombatItemSubsystem>();
	UCombatItemData* Data = CombatItemTests::Ring(Unit);
	FCombatItemHandle First, Second;
	FGameplayTag Failure;
	TestTrue(TEXT("First ring granted"), Inventory->GiveItem(Data, 1, First, Failure));
	TestTrue(TEXT("Duplicate ring granted"), Inventory->GiveItem(Data, 1, Second, Failure));
	TestTrue(TEXT("Instances have different identities"), First != Second);
	TestEqual(TEXT("Independent passive GE stack"), Unit->GetCombatAbilitySystemComponent()->GetNumericAttribute(UCombatAttributeSet::GetArmorAttribute()), 10.0f);
	Inventory->CommitCooldown(First, 20.0f);
	TestTrue(TEXT("Move first ring to backpack"), Inventory->TrySwap(0, 6, Inventory->GetRevision(), First, {}, Failure));
	TestEqual(TEXT("Only moved passive removed"), Unit->GetCombatAbilitySystemComponent()->GetNumericAttribute(UCombatAttributeSet::GetArmorAttribute()), 5.0f);
	for (int32 Frame = 0; Frame < 80; ++Frame) World.Tick(LEVELTICK_All, 0.05f);
	TestTrue(TEXT("Backpack cooldown advances at half rate"), FMath::IsNearlyEqual(Items->FindItem(First)->GetCooldownRemaining(World.GetTimeSeconds()), 18.0f, 0.01f));
	const int32 StaleRevision = Inventory->GetRevision();
	TestTrue(TEXT("Backpack to equipment succeeds"), Inventory->TrySwap(6, 0, StaleRevision, First, {}, Failure));
	TestFalse(TEXT("Stale inventory cannot swap again"), Inventory->TrySwap(0, 6, StaleRevision, First, {}, Failure));
	TestFalse(TEXT("Reequip waiting disables active"), Inventory->ValidateActive(First, true, Failure));
	TestEqual(TEXT("Reequip reason"), Failure, CombatTags::Failure_Item_Muted.GetTag());
	TestEqual(TEXT("Reequip waiting disables passive"), Unit->GetCombatAbilitySystemComponent()->GetNumericAttribute(UCombatAttributeSet::GetArmorAttribute()), 5.0f);
	for (int32 Frame = 0; Frame < 122; ++Frame) World.Tick(LEVELTICK_All, 0.05f);
	World.GetSubsystem<UCombatSchedulerSubsystem>()->RunDueTasks(World.GetTimeSeconds());
	TestEqual(TEXT("Scheduler restores exactly one passive"), Unit->GetCombatAbilitySystemComponent()->GetNumericAttribute(UCombatAttributeSet::GetArmorAttribute()), 10.0f);
	Inventory->ReconcileEffects();
	TestEqual(TEXT("Repeated reconcile is idempotent"), Unit->GetCombatModifierComponent()->GetActiveModifierCount(), 2);
	TestTrue(TEXT("Cooldown was retained across both moves"), FMath::IsNearlyEqual(Items->FindItem(First)->GetCooldownRemaining(World.GetTimeSeconds()), 11.9f, 0.02f));
	Unit->Destroy();
	TestEqual(TEXT("Owner EndPlay releases all instances"), Items->GetInstanceCount(), 0);
	TestNull(TEXT("Old handle cannot resolve"), Items->FindItem(First));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatItemActiveTest, "Combat.Items.ActiveIsolationAndFinalConsumption", CombatItemTests::Flags)
bool FCombatItemActiveTest::RunTest(const FString& Parameters)
{
	FCombatAutomationWorldFixture Fixture;
	UWorld& World = *Fixture.GetWorld();
	ACombatUnitCharacter* Unit = CombatItemTests::Spawn(World, TEXT("item_active"));
	UCombatAbilitySystemComponent* Asc = Unit->GetCombatAbilitySystemComponent();
	UCombatInventoryComponent* Inventory = Unit->GetCombatInventoryComponent();
	UCombatItemSubsystem* Items = World.GetSubsystem<UCombatItemSubsystem>();
	UCombatAbilityData* Ability = CombatItemTests::Heal(Unit, 20.0f);
	TGuardValue<TObjectPtr<UCombatAbilityData>> CdoGuard(GetMutableDefault<UCombatSelfHealAbility>()->AbilityData, Ability);
	UCombatItemData* Data = NewObject<UCombatItemData>(Unit);
	Data->DefinitionName = TEXT("test_item_wand");
	Data->ActiveAbility = UCombatSelfHealAbility::StaticClass();
	FGameplayTag Failure;
	FCombatItemHandle First, Second;
	TestTrue(TEXT("First active granted"), Inventory->GiveItem(Data, 1, First, Failure));
	TestTrue(TEXT("Duplicate active definition granted"), Inventory->GiveItem(Data, 1, Second, Failure));
	if (!TestNotNull(TEXT("First active instance exists"), Items->FindItem(First))
		|| !TestNotNull(TEXT("Second active instance exists"), Items->FindItem(Second))) return false;
	const auto FirstAbility = Items->FindItem(First)->GetAbilityHandle();
	const auto SecondAbility = Items->FindItem(Second)->GetAbilityHandle();
	TestTrue(TEXT("Duplicate definitions have independent specs"), FirstAbility != SecondAbility);
	TestNull(TEXT("Item ability is not a hero skill by definition"), Asc->FindCombatAbilitySpecByDefinitionId(Ability->GetPrimaryAssetId()));
	CombatEffectUtilities::ApplyAttributeAdditive(Unit, *Asc, UCombatAttributeSet::GetHealthAttribute(), -200.0f);
	Asc->AddLooseGameplayTag(CombatTags::State_Silenced);
	TestTrue(TEXT("Silence does not block item active"), Asc->TryActivateCombatAbility(FirstAbility, {}, Failure));
	TestEqual(TEXT("Item heals through common pipeline"), Asc->GetNumericAttribute(UCombatAttributeSet::GetHealthAttribute()), 340.0f);
	TestEqual(TEXT("Item charges mana once"), Asc->GetNumericAttribute(UCombatAttributeSet::GetManaAttribute()), 190.0f);
	TestTrue(TEXT("Only used instance cools down"), Asc->GetCombatAbilityCooldownRemaining(FirstAbility) > 0.0f);
	TestEqual(TEXT("Other instance is ready"), Asc->GetCombatAbilityCooldownRemaining(SecondAbility), 0.0f);
	Asc->AddLooseGameplayTag(CombatTags::State_Muted);
	TestFalse(TEXT("Mute blocks item active"), Asc->TryActivateCombatAbility(SecondAbility, {}, Failure));
	Asc->RemoveLooseGameplayTag(CombatTags::State_Muted);
	TestFalse(TEXT("Item skill cannot be upgraded"), Asc->SetCombatAbilityLevel(SecondAbility, 1, Failure));
	TestTrue(TEXT("Active can be moved to backpack while idle"), Inventory->TrySwap(1, 6, Inventory->GetRevision(), Second, {}, Failure));
	TestFalse(TEXT("Raw server activation cannot bypass backpack"), Asc->TryActivateCombatAbility(SecondAbility, {}, Failure));
	Data = NewObject<UCombatItemData>(Unit);
	Data->DefinitionName = TEXT("test_consumable");
	Data->ActiveAbility = UCombatSelfHealAbility::StaticClass();
	Data->QuantityPerUse = 1;
	FCombatItemHandle Consumable;
	TestTrue(TEXT("Consumable granted"), Inventory->GiveItem(Data, 1, Consumable, Failure));
	if (!TestNotNull(TEXT("Consumable instance exists"), Items->FindItem(Consumable))) return false;
	const auto ConsumeAbility = Items->FindItem(Consumable)->GetAbilityHandle();
	TestTrue(TEXT("Final item executes fully"), Asc->TryActivateCombatAbility(ConsumeAbility, {}, Failure));
	TestEqual(TEXT("Final item heal lands"), Asc->GetNumericAttribute(UCombatAttributeSet::GetHealthAttribute()), 380.0f);
	World.GetSubsystem<UCombatSchedulerSubsystem>()->RunDueTasks(World.GetTimeSeconds());
	TestNull(TEXT("Final instance released outside ability stack"), Items->FindItem(Consumable));
	TestNull(TEXT("Consumed ability spec released"), Asc->FindAbilitySpecFromHandle(ConsumeAbility));
	bool bSourcePreserved = false;
	for (const auto& Record : World.GetSubsystem<UCombatEventSubsystem>()->GetRecentRecords())
		if (Record.EventType == CombatTags::Event_Combat_HealApplied && Record.Source.ItemHandle == Consumable && Record.Source.ItemDefinitionId == Data->GetPrimaryAssetId()) bSourcePreserved = true;
	TestTrue(TEXT("Consumed item identity retained on heal event"), bSourcePreserved);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatItemPickupTest, "Combat.Items.PickupContentionCapacityAndStaleOrders", CombatItemTests::Flags)
bool FCombatItemPickupTest::RunTest(const FString& Parameters)
{
	FCombatAutomationWorldFixture Fixture;
	UWorld& World = *Fixture.GetWorld();
	ACombatUnitCharacter* First = CombatItemTests::Spawn(World, TEXT("item_picker_a"));
	ACombatUnitCharacter* Second = CombatItemTests::Spawn(World, TEXT("item_picker_b"));
	UCombatItemData* Data = CombatItemTests::Ring(First);
	UCombatItemSubsystem* Items = World.GetSubsystem<UCombatItemSubsystem>();
	FGameplayTag Failure;
	const auto Ground = Items->SpawnItem(Data, 1, FVector(70, 0, 0));
	TestTrue(TEXT("Ground instance created"), Ground.IsValid());
	const int32 Revision = Items->FindItem(Ground)->GetRevision();
	TestTrue(TEXT("First contender picks item"), First->GetCombatInventoryComponent()->TryPickup(Ground, Revision, Failure));
	TestFalse(TEXT("Second contender cannot duplicate it"), Second->GetCombatInventoryComponent()->TryPickup(Ground, Revision, Failure));
	TestEqual(TEXT("Only one instance exists"), Items->GetInstanceCount(), 1);
	TestEqual(TEXT("Identity preserved by pickup"), First->GetCombatInventoryComponent()->GetItemAt(0), Ground);
	for (int32 Index = 1; Index < 9; ++Index)
	{
		FCombatItemHandle Extra;
		TestTrue(TEXT("Fill empty slot"), First->GetCombatInventoryComponent()->GiveItem(Data, 1, Extra, Failure));
	}
	const auto Excess = Items->SpawnItem(Data, 1, FVector(70, 0, 0));
	TestFalse(TEXT("Full inventory rejects ground item"), First->GetCombatInventoryComponent()->TryPickup(Excess, Items->FindItem(Excess)->GetRevision(), Failure));
	TestEqual(TEXT("Full reason"), Failure, CombatTags::Failure_Item_Full.GetTag());
	TestNotNull(TEXT("Full inventory leaves ground actor intact"), Items->FindItem(Excess)->GetWorldActor());
	const auto Far = Items->SpawnItem(Data, 1, FVector(900, 0, 0));
	UCombatOrderComponent* Orders = Second->GetCombatOrderComponent();
	Orders->SetNavigationDeferredForTesting(true);
	FCombatOrderRequest Request;
	Request.Type = ECombatOrderType::PickupItem;
	Request.ItemHandle = Far;
	Request.ItemRevision = Items->FindItem(Far)->GetRevision();
	int32 FinalCount = 0;
	Orders->OnOrderFinished().AddLambda([&FinalCount](const auto&) { ++FinalCount; });
	const auto Accepted = Orders->IssueOrder(Request, false);
	TestTrue(TEXT("Far pickup is initially accepted"), Accepted.bSuccess);
	TestEqual(TEXT("Accepted is not completion"), FinalCount, 0);
	Orders->StopAllOrders(FGameplayTag());
	TestEqual(TEXT("Cancel completes exactly once"), FinalCount, 1);
	TestFalse(TEXT("Late navigation callback rejected"), Orders->CompleteMovementForTesting(Accepted.Handle, true));
	TestNotNull(TEXT("Cancelled pickup leaves item on ground"), Items->FindItem(Far)->GetWorldActor());
	Orders->OnOrderFinished().Clear();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatItemLifecycleTest, "Combat.Items.DeathRespawnAuraAndWorldCleanup", CombatItemTests::Flags)
bool FCombatItemLifecycleTest::RunTest(const FString& Parameters)
{
	FCombatAutomationWorldFixture Fixture;
	UWorld& World = *Fixture.GetWorld();
	ACombatUnitCharacter* Unit = CombatItemTests::Spawn(World, TEXT("item_lifecycle"));
	UCombatInventoryComponent* Inventory = Unit->GetCombatInventoryComponent();
	UCombatItemSubsystem* Items = World.GetSubsystem<UCombatItemSubsystem>();
	UCombatItemData* Data = CombatItemTests::Ring(Unit);
	Data->AuraModifier = Data->Passives[0].Modifier;
	// 光环与本体不能共享同一定义的 Item owner；测试使用独立子定义。
	Data->AuraModifier = DuplicateObject<UCombatModifierData>(Data->AuraModifier, Data);
	Data->AuraModifier->DefinitionName = TEXT("test_item_aura");
	FCombatItemHandle Held;
	FGameplayTag Failure;
	TestTrue(TEXT("Aura item granted"), Inventory->GiveItem(Data, 1, Held, Failure));
	UCombatAuraSubsystem* Auras = World.GetSubsystem<UCombatAuraSubsystem>();
	TestEqual(TEXT("Equipped aura registered"), Auras->GetActiveAuraCount(), 1);
	Inventory->CommitCooldown(Held, 20.0f);
	const auto Cause = World.GetSubsystem<UCombatEventSubsystem>()->CreateRootEvent();
	TestTrue(TEXT("Unit dies"), Unit->GetCombatLifecycleComponent()->RequestDeath(Cause, nullptr));
	TestEqual(TEXT("Normal death retains inventory"), Inventory->GetItemCount(), 1);
	TestEqual(TEXT("Death cancels held aura"), Auras->GetActiveAuraCount(), 0);
	TestEqual(TEXT("Death removes held passives"), Unit->GetCombatModifierComponent()->GetActiveModifierCount(), 0);
	TestTrue(TEXT("Respawn succeeds"), Unit->GetCombatLifecycleComponent()->RespawnAtLocation(FVector::ZeroVector));
	Unit->GetCharacterMovement()->DisableMovement();
	TestEqual(TEXT("Respawn restores aura exactly once"), Auras->GetActiveAuraCount(), 1);
	Inventory->ReconcileEffects();
	TestEqual(TEXT("Repeated reconcile does not duplicate aura"), Auras->GetActiveAuraCount(), 1);
	TestTrue(TEXT("Death preserves item cooldown"), Items->FindItem(Held)->GetCooldownRemaining(World.GetTimeSeconds()) > 0.0f);
	Items->Deinitialize();
	TestEqual(TEXT("World teardown clears instances"), Items->GetInstanceCount(), 0);
	TestEqual(TEXT("World teardown cancels aura"), Auras->GetActiveAuraCount(), 0);
	TestEqual(TEXT("World teardown clears component slots"), Inventory->GetItemCount(), 0);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatItemPolicyTest, "Combat.Items.UniquePassivesResourcesAndSlotPolicy", CombatItemTests::Flags)
bool FCombatItemPolicyTest::RunTest(const FString& Parameters)
{
	FCombatAutomationWorldFixture Fixture;
	UWorld& World = *Fixture.GetWorld();
	ACombatUnitCharacter* Unit = CombatItemTests::Spawn(World, TEXT("item_policy"));
	UCombatInventoryComponent* Inventory = Unit->GetCombatInventoryComponent();
	UCombatAbilitySystemComponent* Asc = Unit->GetCombatAbilitySystemComponent();
	UCombatItemData* Data = CombatItemTests::Ring(Unit);
	Data->Passives[0].UniqueGroup = TEXT("unique_armor");
	Data->bCanDrop = false;
	Data->bCanEnterBackpack = false;
	FCombatItemHandle First, Second;
	FGameplayTag Failure;
	Inventory->GiveItem(Data, 1, First, Failure);
	Inventory->GiveItem(Data, 1, Second, Failure);
	TestEqual(TEXT("Unique group applies once"), Asc->GetNumericAttribute(UCombatAttributeSet::GetArmorAttribute()), 5.0f);
	TestFalse(TEXT("Restricted item cannot enter backpack"), Inventory->TrySwap(0, 6, Inventory->GetRevision(), First, {}, Failure));
	TestFalse(TEXT("Restricted item cannot be dropped"), Inventory->TryDrop(First, World.GetSubsystem<UCombatItemSubsystem>()->FindItem(First)->GetRevision(), FVector::ZeroVector, Failure));
	TestTrue(TEXT("Swapping priority replaces exact unique effect"), Inventory->TrySwap(0, 1, Inventory->GetRevision(), First, Second, Failure));
	TestEqual(TEXT("Unique effect remains single"), Asc->GetNumericAttribute(UCombatAttributeSet::GetArmorAttribute()), 5.0f);
	UCombatItemData* Vitality = CombatItemTests::Ring(Unit);
	Vitality->DefinitionName = TEXT("test_vitality");
	Vitality->Passives[0].Modifier->AttributeChanges[0].Attribute = UCombatAttributeSet::GetMaxHealthAttribute();
	Vitality->Passives[0].Modifier->AttributeChanges[0].Magnitude = 100.0f;
	FCombatItemHandle HealthItem;
	Inventory->GiveItem(Vitality, 1, HealthItem, Failure);
	TestEqual(TEXT("Max health increases through GE"), Asc->GetNumericAttribute(UCombatAttributeSet::GetMaxHealthAttribute()), 600.0f);
	TestEqual(TEXT("Equipping never fills new health"), Asc->GetNumericAttribute(UCombatAttributeSet::GetHealthAttribute()), 500.0f);
	CombatEffectUtilities::ApplyAttributeAdditive(Unit, *Asc, UCombatAttributeSet::GetHealthAttribute(), 90.0f);
	Inventory->TrySwap(2, 6, Inventory->GetRevision(), HealthItem, {}, Failure);
	TestEqual(TEXT("Removing max health clamps current health"), Asc->GetNumericAttribute(UCombatAttributeSet::GetHealthAttribute()), 500.0f);
	Inventory->TrySwap(6, 2, Inventory->GetRevision(), HealthItem, {}, Failure);
	for (int32 Frame = 0; Frame < 122; ++Frame) World.Tick(LEVELTICK_All, 0.05f);
	TestEqual(TEXT("Reequip cannot regenerate the clamped health"), Asc->GetNumericAttribute(UCombatAttributeSet::GetHealthAttribute()), 500.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatItemBoundTest, "Combat.Items.DeathDropRetainsBindingAndCooldown", CombatItemTests::Flags)
bool FCombatItemBoundTest::RunTest(const FString& Parameters)
{
	FCombatAutomationWorldFixture Fixture;
	UWorld& World = *Fixture.GetWorld();
	ACombatUnitCharacter* Unit = CombatItemTests::Spawn(World, TEXT("item_bound_owner"));
	ACombatUnitCharacter* Other = CombatItemTests::Spawn(World, TEXT("item_bound_other"));
	UCombatItemData* Data = CombatItemTests::Ring(Unit);
	Data->Sharing = ECombatItemSharing::BoundUnit;
	Data->bDropOnDeath = true;
	FCombatItemHandle Handle;
	FGameplayTag Failure;
	Unit->GetCombatInventoryComponent()->GiveItem(Data, 1, Handle, Failure);
	Unit->GetCombatInventoryComponent()->CommitCooldown(Handle, 20);
	Unit->GetCombatLifecycleComponent()->RequestDeath(World.GetSubsystem<UCombatEventSubsystem>()->CreateRootEvent(), nullptr);
	UCombatItemSubsystem* Items = World.GetSubsystem<UCombatItemSubsystem>();
	const UCombatItemInstance* Item = Items->FindItem(Handle);
	if (!TestNotNull(TEXT("Death drop keeps registered identity"), Item)) return false;
	TestNotNull(TEXT("Death creates ground projection"), Item->GetWorldActor());
	// 无 GameMode 的临时世界需显式派发新 Actor 的 BeginPlay，才能验证正常 EndPlay 回收。
	if (!Item->GetWorldActor()->HasActorBegunPlay()) Item->GetWorldActor()->DispatchBeginPlay();
	TestEqual(TEXT("Death drop removes held slot"), Unit->GetCombatInventoryComponent()->GetItemCount(), 0);
	for (int32 Frame = 0; Frame < 40; ++Frame) World.Tick(LEVELTICK_All, 0.05f);
	TestTrue(TEXT("Ground cooldown advances at half rate"), FMath::IsNearlyEqual(Item->GetCooldownRemaining(World.GetTimeSeconds()), 19.0f, 0.01f));
	TestFalse(TEXT("Other unit cannot acquire bound item"), Other->GetCombatInventoryComponent()->TryPickup(Handle, Item->GetRevision(), Failure));
	Unit->Destroy();
	TestFalse(TEXT("Expired owner reference cannot clear binding"), Other->GetCombatInventoryComponent()->TryPickup(Handle, Item->GetRevision(), Failure));
	TestEqual(TEXT("Binding is explicit after owner EndPlay"), Failure, CombatTags::Failure_Item_Bound.GetTag());
	Item->GetWorldActor()->Destroy();
	TestEqual(TEXT("Ground EndPlay releases instance"), Items->GetInstanceCount(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatItemChargesTest, "Combat.Items.ChargesAndSharedCooldown", CombatItemTests::Flags)
bool FCombatItemChargesTest::RunTest(const FString& Parameters)
{
	FCombatAutomationWorldFixture Fixture;
	UWorld& World = *Fixture.GetWorld();
	ACombatUnitCharacter* Unit = CombatItemTests::Spawn(World, TEXT("item_charges"));
	UCombatAbilityData* Ability = CombatItemTests::Heal(Unit, 10.0f);
	TGuardValue<TObjectPtr<UCombatAbilityData>> CdoGuard(GetMutableDefault<UCombatSelfHealAbility>()->AbilityData, Ability);
	UCombatItemData* Data = CombatItemTests::Ring(Unit);
	Data->ActiveAbility = UCombatSelfHealAbility::StaticClass();
	Data->InitialCharges = 2;
	Data->ChargesPerUse = 1;
	Data->SharedCooldownGroup = TEXT("shared_wand");
	FCombatItemHandle First, Second;
	FGameplayTag Failure;
	UCombatInventoryComponent* Inventory = Unit->GetCombatInventoryComponent();
	Inventory->GiveItem(Data, 1, First, Failure);
	Inventory->GiveItem(Data, 1, Second, Failure);
	UCombatItemSubsystem* Items = World.GetSubsystem<UCombatItemSubsystem>();
	if (!TestNotNull(TEXT("Charged item exists"), Items->FindItem(First))) return false;
	UCombatAbilitySystemComponent* Asc = Unit->GetCombatAbilitySystemComponent();
	TestTrue(TEXT("Charged ability executes"), Asc->TryActivateCombatAbility(Items->FindItem(First)->GetAbilityHandle(), {}, Failure));
	TestEqual(TEXT("Charge consumption does not change quantity"), Items->FindItem(First)->GetQuantity(), 1);
	TestEqual(TEXT("One charge consumed"), Items->FindItem(First)->GetCharges(), 1);
	TestEqual(TEXT("Shared cooldown does not consume other charges"), Items->FindItem(Second)->GetCharges(), 2);
	TestFalse(TEXT("Shared group prevents switching to ready duplicate"), Asc->TryActivateCombatAbility(Items->FindItem(Second)->GetAbilityHandle(), {}, Failure));
	TestEqual(TEXT("Shared cooldown denial"), Failure, CombatTags::Failure_Ability_Cooldown.GetTag());
	for (int32 Frame = 0; Frame < 202; ++Frame) World.Tick(LEVELTICK_All, 0.05f);
	TestTrue(TEXT("Final charge executes"), Asc->TryActivateCombatAbility(Items->FindItem(First)->GetAbilityHandle(), {}, Failure));
	TestEqual(TEXT("Empty charges retain passive item by default"), Items->FindItem(First)->GetCharges(), 0);
	TestFalse(TEXT("Empty item cannot activate"), Asc->TryActivateCombatAbility(Items->FindItem(First)->GetAbilityHandle(), {}, Failure));
	TestEqual(TEXT("Empty charge reason"), Failure, CombatTags::Failure_Item_Empty.GetTag());
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatItemCommitReentryTest, "Combat.Items.CommitObserverDeathIsAtomic", CombatItemTests::Flags)
bool FCombatItemCommitReentryTest::RunTest(const FString& Parameters)
{
	FCombatAutomationWorldFixture Fixture;
	UWorld& World = *Fixture.GetWorld();
	ACombatUnitCharacter* Unit = CombatItemTests::Spawn(World, TEXT("item_commit_reentry"));
	UCombatAbilitySystemComponent* Asc = Unit->GetCombatAbilitySystemComponent();
	UCombatInventoryComponent* Inventory = Unit->GetCombatInventoryComponent();
	UCombatItemSubsystem* Items = World.GetSubsystem<UCombatItemSubsystem>();
	UCombatAbilityData* Ability = CombatItemTests::Heal(Unit, 20);
	TGuardValue<TObjectPtr<UCombatAbilityData>> CdoGuard(GetMutableDefault<UCombatSelfHealAbility>()->AbilityData, Ability);
	UCombatItemData* Data = NewObject<UCombatItemData>(Unit);
	Data->DefinitionName = TEXT("test_commit_observer");
	Data->ActiveAbility = UCombatSelfHealAbility::StaticClass();
	Data->QuantityPerUse = 1;
	Data->bDropOnDeath = true;
	FGameplayTag Failure;
	FCombatItemHandle Handle;
	if (!TestTrue(TEXT("Consumable granted"), Inventory->GiveItem(Data, 1, Handle, Failure))) return false;
	const auto SpecHandle = Items->FindItem(Handle)->GetAbilityHandle();
	bool bObservedFullCommit = false;
	const FDelegateHandle Observer = Asc->GetGameplayAttributeValueChangeDelegate(UCombatAttributeSet::GetManaAttribute()).AddLambda(
		[&](const FOnAttributeChangeData& Change)
		{
			if (Change.NewValue >= Change.OldValue) return;
			const UCombatItemInstance* Item = Items->FindItem(Handle);
			bObservedFullCommit = Item && Item->GetQuantity() == 0 && Item->GetCooldownRemaining(World.GetTimeSeconds()) > 0;
			Unit->GetCombatLifecycleComponent()->RequestDeath(World.GetSubsystem<UCombatEventSubsystem>()->CreateRootEvent(), nullptr);
		});
	Asc->TryActivateCombatAbility(SpecHandle, {}, Failure);
	Asc->GetGameplayAttributeValueChangeDelegate(UCombatAttributeSet::GetManaAttribute()).Remove(Observer);
	TestTrue(TEXT("Mana observers see quantity and cooldown already committed"), bObservedFullCommit);
	World.GetSubsystem<UCombatSchedulerSubsystem>()->RunDueTasks(World.GetTimeSeconds());
	TestNull(TEXT("Death cannot drop an unconsumed copy"), Items->FindItem(Handle));
	TestNull(TEXT("Interrupted final spec is cleaned"), Asc->FindAbilitySpecFromHandle(SpecHandle));
	for (const auto& Event : World.GetSubsystem<UCombatEventSubsystem>()->GetRecentRecords())
		TestFalse(TEXT("No heal after commit observer killed owner"), Event.EventType == CombatTags::Event_Combat_HealApplied && Event.Source.ItemHandle == Handle);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatItemManaCapTest, "Combat.Items.ManaCostAfterRegenerationAtCap", CombatItemTests::Flags)
bool FCombatItemManaCapTest::RunTest(const FString& Parameters)
{
	FCombatAutomationWorldFixture Fixture;
	UWorld& World = *Fixture.GetWorld();
	ACombatUnitCharacter* Unit = CombatItemTests::Spawn(World, TEXT("item_mana_cap"));
	UCombatAbilitySystemComponent* Asc = Unit->GetCombatAbilitySystemComponent();
	CombatEffectUtilities::ApplyAttributeAdditive(Unit, *Asc, UCombatAttributeSet::GetManaRegenAttribute(), 2);
	for (int32 Frame = 0; Frame < 200; ++Frame) World.Tick(LEVELTICK_All, 0.05f);
	TestEqual(TEXT("Idle regeneration cannot build a hidden mana balance"), Asc->GetNumericAttributeBase(UCombatAttributeSet::GetManaAttribute()), 200.0f);
	UCombatAbilityData* Ability = CombatItemTests::Heal(Unit, 20);
	TGuardValue<TObjectPtr<UCombatAbilityData>> CdoGuard(GetMutableDefault<UCombatSelfHealAbility>()->AbilityData, Ability);
	UCombatItemData* Data = NewObject<UCombatItemData>(Unit);
	Data->DefinitionName = TEXT("test_item_mana_cap");
	Data->ActiveAbility = UCombatSelfHealAbility::StaticClass();
	FCombatItemHandle Handle;
	FGameplayTag Failure;
	if (!TestTrue(TEXT("Active item granted"), Unit->GetCombatInventoryComponent()->GiveItem(Data, 1, Handle, Failure))) return false;
	const auto Spec = World.GetSubsystem<UCombatItemSubsystem>()->FindItem(Handle)->GetAbilityHandle();
	TestTrue(TEXT("Item remains usable after idle"), Asc->TryActivateCombatAbility(Spec, {}, Failure));
	TestEqual(TEXT("Item deducts visible mana after idle at cap"), Asc->GetNumericAttribute(UCombatAttributeSet::GetManaAttribute()), 190.0f);
	return true;
}

#endif
