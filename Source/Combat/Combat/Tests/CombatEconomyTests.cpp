#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "HAL/IConsoleManager.h"
#include "Misc/AutomationTest.h"

#include "Combat/Ability/CombatAbilitySystemComponent.h"
#include "Combat/Attributes/CombatAttributeSet.h"
#include "Combat/Combat/CombatDamageSubsystem.h"
#include "Combat/Core/CombatTags.h"
#include "Combat/Data/CombatDefinitionData.h"
#include "Combat/Economy/CombatEconomyComponent.h"
#include "Combat/Economy/CombatEconomyData.h"
#include "Combat/Economy/CombatShopData.h"
#include "Combat/Items/CombatInventoryComponent.h"
#include "Combat/Items/CombatItemData.h"
#include "Combat/Items/CombatItemSubsystem.h"
#include "Combat/Network/CombatNetworkSecuritySubsystem.h"
#include "Combat/Scheduling/CombatSchedulerSubsystem.h"
#include "Combat/Tests/CombatAutomationWorldFixture.h"
#include "Combat/UI/CombatShopWidget.h"
#include "Combat/UI/CombatPlayerHUD.h"
#include "Combat/Unit/CombatUnitCharacter.h"
#include "CombatPlayerController.h"
#include "Engine/World.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SWidget.h"

namespace CombatEconomyTests
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;

	UCombatItemData* MakeLeaf(UObject* Outer, const FName Name, const int64 Price)
	{
		UCombatItemData* Item = NewObject<UCombatItemData>(Outer);
		Item->DefinitionName = Name;
		Item->bPurchasable = true;
		Item->bSellable = true;
		Item->PurchasePrice = Price;
		return Item;
	}

	UCombatItemData* MakeRecipe(UObject* Outer, const FName Name,
		std::initializer_list<TPair<UCombatItemData*, int32>> Ingredients, const int32 Priority = 0)
	{
		UCombatItemData* Item = NewObject<UCombatItemData>(Outer);
		Item->DefinitionName = Name;
		Item->CraftPriority = Priority;
		Item->bSellable = true;
		for (const TPair<UCombatItemData*, int32>& Entry : Ingredients)
		{
			FCombatItemRecipeIngredient& Ingredient = Item->Recipe.AddDefaulted_GetRef();
			Ingredient.Item = Entry.Key;
			Ingredient.Quantity = Entry.Value;
		}
		return Item;
	}

	UCombatShopData* MakeShop(UObject* Outer, const TArray<UCombatItemData*>& Items)
	{
		UCombatShopData* Shop = NewObject<UCombatShopData>(Outer);
		Shop->DefinitionName = TEXT("test_shop");
		TArray<TSoftObjectPtr<UCombatItemData>> BasicItems;
		TArray<TSoftObjectPtr<UCombatItemData>> UpgradeItems;
		for (UCombatItemData* Item : Items)
		{
			(Item->Recipe.IsEmpty() ? BasicItems : UpgradeItems).Add(Item);
		}
		if (!BasicItems.IsEmpty())
		{
			FCombatShopCategory& Basics = Shop->Categories.AddDefaulted_GetRef();
			Basics.CategoryId = TEXT("basics");
			Basics.DisplayName = FText::FromString(TEXT("基础"));
			Basics.Page = ECombatShopPage::Basic;
			Basics.Items = MoveTemp(BasicItems);
		}
		if (!UpgradeItems.IsEmpty())
		{
			FCombatShopCategory& Upgrades = Shop->Categories.AddDefaulted_GetRef();
			Upgrades.CategoryId = TEXT("upgrades");
			Upgrades.DisplayName = FText::FromString(TEXT("升级"));
			Upgrades.Page = ECombatShopPage::Upgrade;
			Upgrades.Items = MoveTemp(UpgradeItems);
		}
		return Shop;
	}

	UCombatEconomyData* MakeRules(UObject* Outer)
	{
		UCombatEconomyData* Rules = NewObject<UCombatEconomyData>(Outer);
		Rules->DefinitionName = TEXT("test_economy");
		Rules->GoldCap = 99999;
		Rules->StartingGold = 600;
		Rules->PassiveGoldPerMinute = 100;
		return Rules;
	}

	ACombatUnitCharacter* SpawnUnit(UWorld& World, ACombatPlayerController* Player,
		const FName Name, const int64 GoldReward = 0)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		ACombatUnitCharacter* Unit = World.SpawnActor<ACombatUnitCharacter>(
			FVector::ZeroVector, FRotator::ZeroRotator, Params);
		if (!Unit) return nullptr;
		UCombatUnitData* Data = NewObject<UCombatUnitData>(Unit);
		Data->DefinitionName = Name;
		Data->InitialTeamId = FCombatTeamId(Player ? 1 : 2);
		Data->GoldReward = GoldReward;
		Data->BaseStats.MaxHealth = 500.0f;
		if (!Unit->InitializeFromUnitData(Data)) return nullptr;
		if (!Unit->HasActorBegunPlay()) Unit->DispatchBeginPlay();
		if (Player && !Player->SetCommandedUnitAuthority(Unit)) return nullptr;
		return Unit;
	}

	FCombatEconomyRequest MakePurchaseRequest(const ACombatPlayerController& Player,
		const UCombatEconomyComponent& Economy, const FPrimaryAssetId ItemId, const int32 RequestId)
	{
		FCombatEconomyRequest Request;
		Request.RequestId = RequestId;
		Request.CommandBindingGeneration = Player.GetCommandBindingGeneration();
		Request.ExpectedEconomyRevision = Economy.GetEconomyRevision();
		Request.ExpectedInventoryRevision = Economy.GetInventoryRevision();
		Request.Action = ECombatEconomyAction::Purchase;
		Request.ItemDefinitionId = ItemId;
		return Request;
	}
}

/** 价格只由服务器目录递归解析，已有组件按确定顺序抵扣缺失成本。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatEconomyRecipePlanTest,
	"Combat.Economy.RecipeGraphValidationAndPurchasePlan", CombatEconomyTests::Flags)
bool FCombatEconomyRecipePlanTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	UObject* Root = GetTransientPackage();
	UCombatItemData* Branch = CombatEconomyTests::MakeLeaf(Root, TEXT("economy_branch"), 100);
	UCombatItemData* Crystal = CombatEconomyTests::MakeLeaf(Root, TEXT("economy_crystal"), 200);
	UCombatItemData* Wand = CombatEconomyTests::MakeRecipe(Root, TEXT("economy_wand"), {{Branch, 2}}, 10);
	UCombatItemData* Blade = CombatEconomyTests::MakeRecipe(Root, TEXT("economy_blade"), {{Wand, 1}, {Crystal, 1}}, 20);
	UCombatShopData* Shop = CombatEconomyTests::MakeShop(Root, {Branch, Crystal, Wand, Blade});
	FString Error;
	TestTrue(TEXT("Acyclic catalog is valid"), Shop->ValidateRuntime(Error));
	int64 FullPrice = 0;
	TestTrue(TEXT("Nested price resolves"), Shop->CalculateItemPrice(Blade->GetPrimaryAssetId(), FullPrice, Error));
	TestEqual(TEXT("Nested price sums purchasable leaves"), FullPrice, int64(400));
	FCombatPurchasePlan Plan;
	TestTrue(TEXT("Owned component produces a plan"), Shop->BuildPurchasePlan(
		Blade->GetPrimaryAssetId(), {Branch->GetPrimaryAssetId()}, Plan, Error));
	TestEqual(TEXT("Owned branch is consumed"), Plan.ConsumedDefinitions.Num(), 1);
	TestEqual(TEXT("Only missing leaves are charged"), Plan.TotalCost, int64(300));
	FCombatPurchasePlan RepeatedBasic;
	TestTrue(TEXT("Buying a basic item ignores an already owned copy"), Shop->BuildPurchasePlan(
		Branch->GetPrimaryAssetId(), {Branch->GetPrimaryAssetId()}, RepeatedBasic, Error));
	TestEqual(TEXT("Repeated basic purchase still charges its price"), RepeatedBasic.TotalCost, int64(100));
	TestTrue(TEXT("Repeated basic purchase consumes no existing instance"), RepeatedBasic.ConsumedDefinitions.IsEmpty());
	TestEqual(TEXT("Repeated basic purchase creates one new leaf"), RepeatedBasic.PurchasedDefinitions.Num(), 1);
	return true;
}

	/** 单一金币、物品栏直达、自动合成、退款和上限在同一服务器组件闭环。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatEconomyTransactionTest,
	"Combat.Economy.GoldPurchaseCraftSellAndLimits", CombatEconomyTests::Flags)
bool FCombatEconomyTransactionTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FCombatAutomationWorldFixture Fixture;
	if (!Fixture.IsValid()) return false;
	UWorld& World = *Fixture.GetWorld();
	ACombatPlayerController* Player = World.SpawnActor<ACombatPlayerController>();
	if (!TestNotNull(TEXT("Player controller"), Player)) return false;
	ACombatUnitCharacter* Unit = CombatEconomyTests::SpawnUnit(World, Player, TEXT("transaction_unit"));
	if (!TestNotNull(TEXT("Commanded unit"), Unit)) return false;
	UCombatEconomyComponent* Economy = Player->GetCombatEconomyComponent();
	if (!TestNotNull(TEXT("Economy component"), Economy)) return false;

	UCombatItemData* Branch = CombatEconomyTests::MakeLeaf(Player, TEXT("transaction_branch"), 100);
	UCombatItemData* Crystal = CombatEconomyTests::MakeLeaf(Player, TEXT("transaction_crystal"), 200);
	UCombatItemData* Wand = CombatEconomyTests::MakeRecipe(Player, TEXT("transaction_wand"), {{Branch, 2}}, 10);
	UCombatItemData* Blade = CombatEconomyTests::MakeRecipe(Player, TEXT("transaction_blade"), {{Wand, 1}, {Crystal, 1}}, 20);
	UCombatItemData* Unsellable = CombatEconomyTests::MakeLeaf(Player, TEXT("transaction_unsellable"), 100);
	Unsellable->bSellable = false;
	UCombatShopData* Shop = CombatEconomyTests::MakeShop(Player, {Branch, Crystal, Wand, Blade, Unsellable});
	UCombatEconomyData* Rules = CombatEconomyTests::MakeRules(Player);
	FString Error;
	TestTrue(TEXT("Transient match rules initialize"), Economy->InitializeForMatch(Rules, Shop, true, Error));
	TestEqual(TEXT("Starting gold comes from level rules"), Economy->GetGold(), int64(600));

	FGameplayTag Failure;
	FCombatItemHandle ResultHandle;
	TestTrue(TEXT("Basic item purchase succeeds"), Economy->PurchaseItem(
		Branch->GetPrimaryAssetId(), Economy->GetEconomyRevision(), Economy->GetInventoryRevision(), ResultHandle, Failure));
	TestEqual(TEXT("Basic purchase charges one price"), Economy->GetGold(), int64(500));
	TestEqual(TEXT("Basic purchase enters the commanded inventory"), Unit->GetCombatInventoryComponent()->GetItemCount(), 1);
	TestTrue(TEXT("A second purchased component enters inventory"), Economy->PurchaseItem(
		Branch->GetPrimaryAssetId(), Economy->GetEconomyRevision(), Economy->GetInventoryRevision(), ResultHandle, Failure));
	TestEqual(TEXT("Second component is charged independently"), Economy->GetGold(), int64(400));
	TestEqual(TEXT("Entering a purchased component automatically crafts the recipe"), Unit->GetCombatInventoryComponent()->GetItemCount(), 1);
	const UCombatItemInstance* AutoCrafted = World.GetSubsystem<UCombatItemSubsystem>()->FindItem(ResultHandle);
	TestTrue(TEXT("Purchase result handle follows automatic craft"), AutoCrafted && AutoCrafted->GetDefinition() == Wand);
	TestTrue(TEXT("Upgrade purchase consumes owned branch and buys missing leaves"), Economy->PurchaseItem(
		Blade->GetPrimaryAssetId(), Economy->GetEconomyRevision(), Economy->GetInventoryRevision(), ResultHandle, Failure));
	TestEqual(TEXT("Upgrade charges only the missing crystal"), Economy->GetGold(), int64(200));
	TestEqual(TEXT("Nested result replaces its components in inventory"), Unit->GetCombatInventoryComponent()->GetItemCount(), 1);

	const int32 ResultRevision = Economy->FindInventoryItemRevision(ResultHandle);
	TestTrue(TEXT("Fresh purchase receives full refund"), Economy->SellInventoryItem(
		ResultHandle, ResultRevision, Economy->GetEconomyRevision(), Economy->GetInventoryRevision(), Failure));
	TestEqual(TEXT("Refund restores the complete purchase chain"), Economy->GetGold(), int64(600));
	TestEqual(TEXT("Sold item leaves inventory"), Unit->GetCombatInventoryComponent()->GetItemCount(), 0);
	TestTrue(TEXT("Debug command path sets exact gold"), Economy->SetGoldForDebug(5000, Error));
	TestEqual(TEXT("Debug value is visible"), Economy->GetGold(), int64(5000));
	TestFalse(TEXT("Level cap rejects debug overflow"), Economy->SetGoldForDebug(100000, Error));
	TestEqual(TEXT("Rejected debug value has no effect"), Economy->GetGold(), int64(5000));
	TestTrue(TEXT("Income clamps at level cap"), Economy->AddGold(100000, TEXT("TestIncome")));
	TestEqual(TEXT("Gold cannot exceed level cap"), Economy->GetGold(), int64(99999));

	// 退款窗口外按目录总价的 50% 返还；直接推进临时 World 时间即可复核出售政策。
	TestTrue(TEXT("Set balance for delayed refund"), Economy->SetGoldForDebug(100, Error));
	FCombatItemHandle DelayedRefundHandle;
	TestTrue(TEXT("Delayed refund purchase succeeds"), Economy->PurchaseItem(
		Branch->GetPrimaryAssetId(), Economy->GetEconomyRevision(), Economy->GetInventoryRevision(),
		DelayedRefundHandle, Failure));
	TestEqual(TEXT("Delayed refund purchase charges price"), Economy->GetGold(), int64(0));
	const double PurchaseTime = World.GetTimeSeconds();
	World.TimeSeconds = PurchaseTime + 11.0;
	TestTrue(TEXT("Temporary world advances beyond refund window"), World.GetTimeSeconds() >= PurchaseTime + 10.0);
	const int32 DelayedItemRevision = Economy->FindInventoryItemRevision(DelayedRefundHandle);
	TestTrue(TEXT("Delayed refund sale succeeds"), Economy->SellInventoryItem(
		DelayedRefundHandle, DelayedItemRevision, Economy->GetEconomyRevision(), Economy->GetInventoryRevision(), Failure));
	TestEqual(TEXT("Delayed sale returns configured half value"), Economy->GetGold(), int64(50));
	TestTrue(TEXT("Set balance for unsellable item"), Economy->SetGoldForDebug(100, Error));
	FCombatItemHandle UnsellableHandle;
	TestTrue(TEXT("Unsellable purchase succeeds"), Economy->PurchaseItem(
		Unsellable->GetPrimaryAssetId(), Economy->GetEconomyRevision(), Economy->GetInventoryRevision(),
		UnsellableHandle, Failure));
	const int32 UnsellableRevision = Economy->FindInventoryItemRevision(UnsellableHandle);
	TestFalse(TEXT("Unsellable item cannot be sold"), Economy->SellInventoryItem(
		UnsellableHandle, UnsellableRevision, Economy->GetEconomyRevision(), Economy->GetInventoryRevision(), Failure));
	TestEqual(TEXT("Rejected unsellable sale keeps balance"), Economy->GetGold(), int64(0));
	return true;
}

/** 购买结果必须直接落入当前主控单位物品栏。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatEconomyPurchaseDirectInventoryTest,
	"Combat.Economy.PurchaseDirectToInventory", CombatEconomyTests::Flags)
bool FCombatEconomyPurchaseDirectInventoryTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FCombatAutomationWorldFixture Fixture;
	if (!Fixture.IsValid()) return false;
	UWorld& World = *Fixture.GetWorld();
	ACombatPlayerController* Player = World.SpawnActor<ACombatPlayerController>();
	ACombatUnitCharacter* Unit = CombatEconomyTests::SpawnUnit(World, Player, TEXT("purchase_inventory_red"));
	if (!TestNotNull(TEXT("Commanded unit exists"), Unit)) return false;
	UCombatItemData* Leaf = CombatEconomyTests::MakeLeaf(Player, TEXT("purchase_inventory_leaf"), 100);
	UCombatShopData* Shop = CombatEconomyTests::MakeShop(Player, {Leaf});
	UCombatEconomyData* Rules = CombatEconomyTests::MakeRules(Player);
	UCombatEconomyComponent* Economy = Player->GetCombatEconomyComponent();
	FString Error;
	if (!TestTrue(TEXT("Direct purchase economy initializes"), Economy->InitializeForMatch(Rules, Shop, false, Error))) return false;
	FGameplayTag Failure;
	FCombatItemHandle Purchased;
	TestTrue(TEXT("Purchase request succeeds"), Economy->PurchaseItem(
		Leaf->GetPrimaryAssetId(), Economy->GetEconomyRevision(), Economy->GetInventoryRevision(), Purchased, Failure));
	TestEqual(TEXT("Purchase is delivered to the active inventory"), Unit->GetCombatInventoryComponent()->GetItemCount(), 1);
	return true;
}

/** 被动金币读取绝对 World Game Time；单帧跨过多个周期也只按累计应得值补齐一次。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatEconomyPassiveIncomeTest,
	"Combat.Economy.PassiveIncomeAbsoluteAccumulation", CombatEconomyTests::Flags)
bool FCombatEconomyPassiveIncomeTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FCombatAutomationWorldFixture Fixture;
	if (!Fixture.IsValid()) return false;
	UWorld& World = *Fixture.GetWorld();
	ACombatPlayerController* Player = World.SpawnActor<ACombatPlayerController>();
	UCombatItemData* Leaf = CombatEconomyTests::MakeLeaf(Player, TEXT("passive_leaf"), 100);
	UCombatShopData* Shop = CombatEconomyTests::MakeShop(Player, {Leaf});
	UCombatEconomyData* Rules = CombatEconomyTests::MakeRules(Player);
	Rules->PassiveGoldPerMinute = 600;
	FString Error;
	if (!TestTrue(TEXT("Passive economy initializes"), Player->GetCombatEconomyComponent()->InitializeForMatch(
		Rules, Shop, false, Error))) return false;
	// 临时 PIE World 的直接 Tick 不保证驱动 UTickableWorldSubsystem；显式推进同一
	// World Game Time，验证生产 Scheduler 的绝对时间/Coalesce 语义而不是测试夹具的 Tick 拓扑。
	UCombatSchedulerSubsystem* Scheduler = World.GetSubsystem<UCombatSchedulerSubsystem>();
	TestNotNull(TEXT("Combat scheduler exists"), Scheduler);
	if (!Scheduler) return false;
	Scheduler->RunDueTasks(2.51);
	TestEqual(TEXT("Long frame credits the absolute accumulated amount"),
		Player->GetCombatEconomyComponent()->GetGold(), int64(625));
	Scheduler->RunDueTasks(3.11);
	TestEqual(TEXT("Later callback credits only the missing delta"),
		Player->GetCombatEconomyComponent()->GetGold(), int64(631));
	return true;
}

/** 锁定物品不会参与自动合成；解锁时立即检测当前库存并恢复可用的合成。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatEconomyInventoryLockSellTest,
	"Combat.Economy.InventoryLockSellAndCraft", CombatEconomyTests::Flags)
bool FCombatEconomyInventoryLockSellTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FCombatAutomationWorldFixture Fixture;
	if (!Fixture.IsValid()) return false;
	UWorld& World = *Fixture.GetWorld();
	ACombatPlayerController* Player = World.SpawnActor<ACombatPlayerController>();
	ACombatUnitCharacter* Unit = CombatEconomyTests::SpawnUnit(World, Player, TEXT("inventory_lock_unit"));
	if (!TestNotNull(TEXT("Commanded unit"), Unit)) return false;
	UCombatItemData* Branch = CombatEconomyTests::MakeLeaf(Player, TEXT("inventory_lock_branch"), 100);
	UCombatItemData* Wand = CombatEconomyTests::MakeRecipe(Player, TEXT("inventory_lock_wand"), {{Branch, 2}}, 10);
	// 使用真实护甲被动验证解锁合成会撤销组件效果，并立即启用结果效果。
	for (UCombatItemData* Definition : {Branch, Wand})
	{
		UCombatModifierData* Passive = NewObject<UCombatModifierData>(Definition);
		Passive->DefinitionName = FName(Definition->DefinitionName.ToString() + TEXT("_armor"));
		Passive->DispelRule = ECombatModifierDispelRule::NotDispellable;
		FCombatModifierAttributeChange Change;
		Change.Attribute = UCombatAttributeSet::GetArmorAttribute();
		Change.Magnitude = Definition == Branch ? 1.0f : 5.0f;
		Passive->AttributeChanges.Add(Change);
		Definition->Passives.AddDefaulted_GetRef().Modifier = Passive;
	}
	UCombatShopData* Shop = CombatEconomyTests::MakeShop(Player, {Branch, Wand});
	UCombatEconomyData* Rules = CombatEconomyTests::MakeRules(Player);
	UCombatEconomyComponent* Economy = Player->GetCombatEconomyComponent();
	FString Error;
	if (!TestTrue(TEXT("Inventory economy initializes"), Economy->InitializeForMatch(Rules, Shop, false, Error))) return false;
	const int64 InitialGold = Economy->GetGold();
	UCombatInventoryComponent* Inventory = Unit->GetCombatInventoryComponent();
	UCombatAbilitySystemComponent* Asc = Unit->GetCombatAbilitySystemComponent();
	const float InitialArmor = Asc->GetNumericAttribute(UCombatAttributeSet::GetArmorAttribute());

	FGameplayTag Failure;
	FCombatItemHandle LockedBranch;
	TestTrue(TEXT("Give first component"), Unit->GetCombatInventoryComponent()->GiveItem(Branch, 1, LockedBranch, Failure));
	TArray<FCombatItemView> InventoryViews;
	Unit->GetCombatInventoryComponent()->BuildViews(InventoryViews);
	const FCombatItemView* LockedView = InventoryViews.FindByPredicate(
		[LockedBranch](const FCombatItemView& View) { return View.Handle == LockedBranch; });
	TestNotNull(TEXT("First component has a view"), LockedView);
	if (!LockedView) return false;
	bool bLocked = false;
	TestTrue(TEXT("Locking an inventory item succeeds"), Economy->ToggleInventoryItemLock(LockedBranch, LockedView->Revision,
		Economy->GetEconomyRevision(), Economy->GetInventoryRevision(), bLocked, Failure));
	TestTrue(TEXT("Item reports locked"), bLocked);
	InventoryViews.Reset();
	Unit->GetCombatInventoryComponent()->BuildViews(InventoryViews);
	LockedView = InventoryViews.FindByPredicate([LockedBranch](const FCombatItemView& View) { return View.Handle == LockedBranch; });
	TestTrue(TEXT("Lock state is visible in inventory projection"), LockedView && LockedView->bLocked);
	if (!LockedView) return false;

	const int32 BeforeIncompleteUnlock = Inventory->GetRevision();
	TestTrue(TEXT("Unlock succeeds with an incomplete recipe"), Economy->ToggleInventoryItemLock(LockedBranch, LockedView->Revision,
		Economy->GetEconomyRevision(), Economy->GetInventoryRevision(), bLocked, Failure));
	TestFalse(TEXT("Incomplete recipe leaves the component unlocked"), bLocked);
	TestEqual(TEXT("Incomplete recipe keeps the original handle"), Inventory->GetItemAt(0), LockedBranch);
	TestEqual(TEXT("Incomplete recipe keeps the only component"), Inventory->GetItemCount(), 1);
	TestEqual(TEXT("Incomplete recipe only advances the unlock revision"), Inventory->GetRevision(), BeforeIncompleteUnlock + 1);
	TestEqual(TEXT("Incomplete recipe preserves component effects"), Asc->GetNumericAttribute(UCombatAttributeSet::GetArmorAttribute()), InitialArmor + 1.0f);
	TestTrue(TEXT("Unlocked component is projected without consumption"), Economy->GetEconomyView().InventoryItems.ContainsByPredicate(
		[LockedBranch](const FCombatItemView& View) { return View.Handle == LockedBranch && !View.bLocked && View.Quantity == 1; }));
	TestTrue(TEXT("Component can be locked again"), Economy->ToggleInventoryItemLock(LockedBranch, Economy->FindInventoryItemRevision(LockedBranch),
		Economy->GetEconomyRevision(), Economy->GetInventoryRevision(), bLocked, Failure));
	TestTrue(TEXT("Relocked component remains protected"), bLocked);

	FCombatItemHandle SecondBranch;
	TestTrue(TEXT("Give second component"), Unit->GetCombatInventoryComponent()->GiveItem(Branch, 1, SecondBranch, Failure));
	TestEqual(TEXT("Locked component blocks automatic craft"), Unit->GetCombatInventoryComponent()->GetItemCount(), 2);
	TestTrue(TEXT("Both component handles remain"), World.GetSubsystem<UCombatItemSubsystem>()->FindItem(LockedBranch)
		&& World.GetSubsystem<UCombatItemSubsystem>()->FindItem(SecondBranch));

	InventoryViews.Reset();
	Unit->GetCombatInventoryComponent()->BuildViews(InventoryViews);
	LockedView = InventoryViews.FindByPredicate([LockedBranch](const FCombatItemView& View) { return View.Handle == LockedBranch; });
	TestNotNull(TEXT("Locked component remains addressable"), LockedView);
	if (!LockedView) return false;
	const int32 BeforeStaleUnlock = Inventory->GetRevision();
	TestFalse(TEXT("Stale unlock cannot trigger a complete recipe"), Economy->ToggleInventoryItemLock(LockedBranch, LockedView->Revision - 1,
		Economy->GetEconomyRevision(), Economy->GetInventoryRevision(), bLocked, Failure));
	TestTrue(TEXT("Stale unlock is rejected by the authority"), Failure == CombatTags::Failure_Economy_Stale);
	TestEqual(TEXT("Stale unlock does not change inventory revision"), Inventory->GetRevision(), BeforeStaleUnlock);
	TestEqual(TEXT("Stale unlock keeps both components"), Inventory->GetItemCount(), 2);
	TestTrue(TEXT("Unlocking checks available recipes"), Economy->ToggleInventoryItemLock(LockedBranch, LockedView->Revision,
		Economy->GetEconomyRevision(), Economy->GetInventoryRevision(), bLocked, Failure));
	TestFalse(TEXT("Unlocked state is reported"), bLocked);
	InventoryViews.Reset();
	Unit->GetCombatInventoryComponent()->BuildViews(InventoryViews);
	const bool bHasWand = InventoryViews.ContainsByPredicate([Wand](const FCombatItemView& View)
		{ return View.Handle.IsValid() && View.DefinitionId == Wand->GetPrimaryAssetId(); });
	TestTrue(TEXT("Unlock immediately creates the available recipe result"), bHasWand);
	TestEqual(TEXT("Unlock-triggered craft consumes both components"), Unit->GetCombatInventoryComponent()->GetItemCount(), 1);
	int32 LiveComponentHandles = 0;
	for (const FCombatItemHandle Handle : {LockedBranch, SecondBranch})
	{
		if (World.GetSubsystem<UCombatItemSubsystem>()->FindItem(Handle)) ++LiveComponentHandles;
	}
	TestEqual(TEXT("Unlock-triggered craft consumes both component handles"), LiveComponentHandles, 0);
	TestEqual(TEXT("Unlock and automatic craft spend no gold"), Economy->GetGold(), InitialGold);
	TestEqual(TEXT("Economy projection uses the final crafted revision"), Economy->GetInventoryRevision(), Inventory->GetRevision());
	TestTrue(TEXT("Economy projection contains the unlocked result"), Economy->GetEconomyView().InventoryItems.ContainsByPredicate(
		[Wand](const FCombatItemView& View) { return View.Handle.IsValid() && View.DefinitionId == Wand->GetPrimaryAssetId() && !View.bLocked; }));
	TestEqual(TEXT("Result passive replaces both component passives immediately"), Asc->GetNumericAttribute(UCombatAttributeSet::GetArmorAttribute()), InitialArmor + 5.0f);

	const FCombatItemView* WandView = InventoryViews.FindByPredicate([Wand](const FCombatItemView& View)
		{ return View.Handle.IsValid() && View.DefinitionId == Wand->GetPrimaryAssetId(); });
	TestNotNull(TEXT("Crafted result has a view"), WandView);
	if (WandView)
	{
		const int64 BeforeSell = Economy->GetGold();
		TestTrue(TEXT("Inventory item can be sold"), Economy->SellInventoryItem(WandView->Handle, WandView->Revision,
			Economy->GetEconomyRevision(), Economy->GetInventoryRevision(), Failure));
		TestTrue(TEXT("Selling does not reduce gold"), Economy->GetGold() >= BeforeSell);
		TestEqual(TEXT("Selling the crafted item removes its passive"), Asc->GetNumericAttribute(UCombatAttributeSet::GetArmorAttribute()), InitialArmor);
	}
	return true;
}

/** 英雄九格库存是独立合成域，连续授予组件后按相同目录稳定合成为一个新句柄。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatEconomyInventoryCraftTest,
	"Combat.Economy.InventoryDomainAutomaticCrafting", CombatEconomyTests::Flags)
bool FCombatEconomyInventoryCraftTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FCombatAutomationWorldFixture Fixture;
	if (!Fixture.IsValid()) return false;
	UWorld& World = *Fixture.GetWorld();
	ACombatPlayerController* Player = World.SpawnActor<ACombatPlayerController>();
	ACombatUnitCharacter* Unit = CombatEconomyTests::SpawnUnit(World, Player, TEXT("inventory_craft_unit"));
	if (!TestNotNull(TEXT("Commanded unit"), Unit)) return false;
	UCombatItemData* Branch = CombatEconomyTests::MakeLeaf(Player, TEXT("inventory_craft_branch"), 100);
	UCombatItemData* Wand = CombatEconomyTests::MakeRecipe(Player, TEXT("inventory_craft_wand"), {{Branch, 2}}, 10);
	UCombatShopData* Shop = CombatEconomyTests::MakeShop(Player, {Branch, Wand});
	UCombatEconomyData* Rules = CombatEconomyTests::MakeRules(Player);
	FString Error;
	if (!TestTrue(TEXT("Craft economy initializes"), Player->GetCombatEconomyComponent()->InitializeForMatch(
		Rules, Shop, false, Error))) return false;
	FGameplayTag Failure;
	FCombatItemHandle First;
	FCombatItemHandle Result;
	TestTrue(TEXT("Give first component"), Unit->GetCombatInventoryComponent()->GiveItem(Branch, 1, First, Failure));
	TestTrue(TEXT("Give second component"), Unit->GetCombatInventoryComponent()->GiveItem(Branch, 1, Result, Failure));
	TestEqual(TEXT("Two components become one inventory result"), Unit->GetCombatInventoryComponent()->GetItemCount(), 1);
	const UCombatItemInstance* Crafted = World.GetSubsystem<UCombatItemSubsystem>()->FindItem(Result);
	TestTrue(TEXT("Returned handle follows the crafted result"), Crafted && Crafted->GetDefinition() == Wand);
	TestNull(TEXT("First component handle is invalidated"), World.GetSubsystem<UCombatItemSubsystem>()->FindItem(First));
	return true;
}

/** 经济 RPC 与 Order 共用连接级重放和限频状态，越权及畸形载荷不进入业务事务。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatEconomyRpcSecurityTest,
	"Combat.Economy.RpcReplayRateLimitOwnershipAndPayload", CombatEconomyTests::Flags)
bool FCombatEconomyRpcSecurityTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FCombatAutomationWorldFixture Fixture(NM_DedicatedServer);
	if (!Fixture.IsValid()) return false;
	UWorld& World = *Fixture.GetWorld();
	ACombatPlayerController* Owner = World.SpawnActor<ACombatPlayerController>();
	ACombatPlayerController* Other = World.SpawnActor<ACombatPlayerController>();
	UCombatItemData* Leaf = CombatEconomyTests::MakeLeaf(Owner, TEXT("rpc_leaf"), 100);
	UCombatShopData* Shop = CombatEconomyTests::MakeShop(Owner, {Leaf});
	UCombatEconomyData* Rules = CombatEconomyTests::MakeRules(Owner);
	FString Error;
	if (!TestTrue(TEXT("Owner economy initializes"), Owner->GetCombatEconomyComponent()->InitializeForMatch(
		Rules, Shop, false, Error))
		|| !TestTrue(TEXT("Other economy initializes"), Other->GetCombatEconomyComponent()->InitializeForMatch(
			Rules, Shop, false, Error))) return false;
	ACombatUnitCharacter* OwnerUnit = CombatEconomyTests::SpawnUnit(World, Owner, TEXT("rpc_owner_unit"));
	if (!TestNotNull(TEXT("Owner commanded unit"), OwnerUnit)) return false;

	FCombatEconomyRequest Purchase = CombatEconomyTests::MakePurchaseRequest(
		*Owner, *Owner->GetCombatEconomyComponent(), Leaf->GetPrimaryAssetId(), 10);
	const FCombatEconomyResult Accepted = Owner->ProcessEconomyRequestForConnection(Owner, Purchase);
	TestTrue(TEXT("Owned bounded economy request succeeds"), Accepted.bSuccess);
	TestEqual(TEXT("Accepted request charges exactly once"), Owner->GetCombatEconomyComponent()->GetGold(), int64(500));
	const FCombatEconomyResult Replay = Owner->ProcessEconomyRequestForConnection(Owner, Purchase);
	TestFalse(TEXT("Duplicate economy request is rejected"), Replay.bSuccess);
	TestEqual(TEXT("Duplicate request has stable replay tag"), Replay.FailureTag,
		CombatTags::Failure_Network_DuplicateRequest.GetTag());
	TestEqual(TEXT("Replay does not charge again"), Owner->GetCombatEconomyComponent()->GetGold(), int64(500));

	FCombatEconomyRequest OwnershipRequest = CombatEconomyTests::MakePurchaseRequest(
		*Owner, *Owner->GetCombatEconomyComponent(), Leaf->GetPrimaryAssetId(), 11);
	const FCombatEconomyResult Ownership = Owner->ProcessEconomyRequestForConnection(Other, OwnershipRequest);
	TestEqual(TEXT("Non-owner has stable ownership tag"), Ownership.FailureTag,
		CombatTags::Failure_Network_Ownership.GetTag());
	FCombatEconomyRequest InvalidPayload = CombatEconomyTests::MakePurchaseRequest(
		*Owner, *Owner->GetCombatEconomyComponent(), FPrimaryAssetId(), 12);
	const FCombatEconomyResult Payload = Owner->ProcessEconomyRequestForConnection(Owner, InvalidPayload);
	TestEqual(TEXT("Malformed fixed payload is rejected before business logic"), Payload.FailureTag,
		CombatTags::Failure_Network_PayloadTooLarge.GetTag());

	ACombatUnitCharacter* OtherUnit = CombatEconomyTests::SpawnUnit(World, Other, TEXT("rpc_shared_budget_unit"));
	if (!TestNotNull(TEXT("Other commanded unit"), OtherUnit)) return false;
	UCombatNetworkSecuritySubsystem* Security = World.GetSubsystem<UCombatNetworkSecuritySubsystem>();
	Security->BurstCapacity = 1.0f;
	Security->RequestsPerSecond = 1.0f;
	FCombatOrderBatchRequest OrderRequest;
	OrderRequest.RequestId = 100;
	OrderRequest.CommandBindingGeneration = Other->GetCommandBindingGeneration();
	OrderRequest.UnitLifeGeneration = OtherUnit->GetLifeGeneration();
	FCombatOrderRequest& Order = OrderRequest.Orders.AddDefaulted_GetRef();
	Order.Type = ECombatOrderType::MoveToPoint;
	Order.TargetLocation = OtherUnit->GetActorLocation();
	Order.bHasTargetLocation = true;
	TestTrue(TEXT("Order consumes the only shared connection token"),
		OtherUnit->ProcessOrderBatchForConnection(Other, OrderRequest).bAccepted);
	FCombatEconomyRequest RateRequest = CombatEconomyTests::MakePurchaseRequest(
		*Other, *Other->GetCombatEconomyComponent(), Leaf->GetPrimaryAssetId(), 101);
	const FCombatEconomyResult RateLimited = Other->ProcessEconomyRequestForConnection(Other, RateRequest);
	TestEqual(TEXT("Economy cannot double the Order request budget"), RateLimited.FailureTag,
		CombatTags::Failure_Network_RateLimited.GetTag());
	TestTrue(TEXT("Security metrics include economy rejection families"), Security->GetSecurityStats().RejectedRequests >= 4);
	return true;
}

/** 击杀金币只在死亡转换成功后发给击杀者玩家，受害者与自杀者均不扣金币。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatEconomyKillAndDeathTest,
	"Combat.Economy.KillRewardAndDeathNoPenalty", CombatEconomyTests::Flags)
bool FCombatEconomyKillAndDeathTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FCombatAutomationWorldFixture Fixture;
	if (!Fixture.IsValid()) return false;
	UWorld& World = *Fixture.GetWorld();
	ACombatPlayerController* KillerPlayer = World.SpawnActor<ACombatPlayerController>();
	ACombatPlayerController* VictimPlayer = World.SpawnActor<ACombatPlayerController>();
	UCombatItemData* Leaf = CombatEconomyTests::MakeLeaf(KillerPlayer, TEXT("kill_leaf"), 100);
	UCombatShopData* Shop = CombatEconomyTests::MakeShop(KillerPlayer, {Leaf});
	UCombatEconomyData* Rules = CombatEconomyTests::MakeRules(KillerPlayer);
	Rules->PassiveGoldPerMinute = 0;
	FString Error;
	if (!KillerPlayer->GetCombatEconomyComponent()->InitializeForMatch(Rules, Shop, false, Error)
		|| !VictimPlayer->GetCombatEconomyComponent()->InitializeForMatch(Rules, Shop, false, Error)) return false;
	ACombatUnitCharacter* Killer = CombatEconomyTests::SpawnUnit(World, KillerPlayer, TEXT("gold_killer"));
	ACombatUnitCharacter* Victim = CombatEconomyTests::SpawnUnit(World, VictimPlayer, TEXT("gold_victim"), 150);
	if (!TestNotNull(TEXT("Killer"), Killer) || !TestNotNull(TEXT("Victim"), Victim)) return false;

	FCombatDamageRequest Lethal;
	Lethal.Source = Killer;
	Lethal.Target = Victim;
	Lethal.Amount = 100000.0f;
	Lethal.DamageType = ECombatDamageType::Pure;
	TestTrue(TEXT("Lethal damage succeeds"), World.GetSubsystem<UCombatDamageSubsystem>()->DealDamage(Lethal).bSuccess);
	TestEqual(TEXT("Killer receives configured gold once"), KillerPlayer->GetCombatEconomyComponent()->GetGold(), int64(750));
	TestEqual(TEXT("Victim death does not deduct gold"), VictimPlayer->GetCombatEconomyComponent()->GetGold(), int64(600));
	TestFalse(TEXT("Repeated lethal request is rejected"), World.GetSubsystem<UCombatDamageSubsystem>()->DealDamage(Lethal).bSuccess);
	TestEqual(TEXT("Repeated death does not duplicate reward"), KillerPlayer->GetCombatEconomyComponent()->GetGold(), int64(750));

	FCombatDamageRequest Suicide = Lethal;
	Suicide.Source = Killer;
	Suicide.Target = Killer;
	TestTrue(TEXT("Self lethal damage still uses normal death pipeline"),
		World.GetSubsystem<UCombatDamageSubsystem>()->DealDamage(Suicide).bSuccess);
	TestEqual(TEXT("Self death neither rewards nor deducts gold"), KillerPlayer->GetCombatEconomyComponent()->GetGold(), int64(750));
	return true;
}

/** Demo 改金命令覆盖 0/上限、非法参数、显式目标以及关卡开关。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatEconomyDebugCommandTest,
	"Combat.Economy.DebugSetGoldCommandBoundaries", CombatEconomyTests::Flags)
bool FCombatEconomyDebugCommandTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FCombatAutomationWorldFixture Fixture;
	if (!Fixture.IsValid()) return false;
	UWorld* World = Fixture.GetWorld();
	ACombatPlayerController* Enabled = World->SpawnActor<ACombatPlayerController>();
	ACombatPlayerController* Disabled = World->SpawnActor<ACombatPlayerController>();
	UCombatItemData* Leaf = CombatEconomyTests::MakeLeaf(Enabled, TEXT("debug_leaf"), 100);
	UCombatShopData* Shop = CombatEconomyTests::MakeShop(Enabled, {Leaf});
	UCombatEconomyData* Rules = CombatEconomyTests::MakeRules(Enabled);
	FString Error;
	if (!Enabled->GetCombatEconomyComponent()->InitializeForMatch(Rules, Shop, true, Error)
		|| !Disabled->GetCombatEconomyComponent()->InitializeForMatch(Rules, Shop, false, Error)) return false;
	IConsoleObject* ConsoleObject = IConsoleManager::Get().FindConsoleObject(TEXT("combat.Debug.SetGold"));
	IConsoleCommand* Command = ConsoleObject ? ConsoleObject->AsCommand() : nullptr;
	if (!TestNotNull(TEXT("Development gold command is registered"), Command)) return false;

	TestTrue(TEXT("Explicit target command executes"), Command->Execute({TEXT("5000"), Enabled->GetName()}, World, *GLog));
	TestEqual(TEXT("Explicit target receives exact amount"), Enabled->GetCombatEconomyComponent()->GetGold(), int64(5000));
	TestTrue(TEXT("Zero boundary executes"), Command->Execute({TEXT("0"), Enabled->GetName()}, World, *GLog));
	TestEqual(TEXT("Zero boundary is accepted"), Enabled->GetCombatEconomyComponent()->GetGold(), int64(0));
	TestTrue(TEXT("Cap boundary executes"), Command->Execute({TEXT("99999"), Enabled->GetName()}, World, *GLog));
	TestEqual(TEXT("Cap boundary is accepted"), Enabled->GetCombatEconomyComponent()->GetGold(), int64(99999));
	const int32 StableRevision = Enabled->GetCombatEconomyComponent()->GetEconomyRevision();
	for (const TArray<FString>& Invalid : {
		TArray<FString>{TEXT("-1"), Enabled->GetName()},
		TArray<FString>{TEXT("100000"), Enabled->GetName()},
		TArray<FString>{TEXT("not_a_number"), Enabled->GetName()},
		TArray<FString>{TEXT("1"), Enabled->GetName(), TEXT("extra")}})
	{
		TestTrue(TEXT("Invalid command is handled without mutation"), Command->Execute(Invalid, World, *GLog));
	}
	TestEqual(TEXT("Invalid commands do not advance revision"), Enabled->GetCombatEconomyComponent()->GetEconomyRevision(), StableRevision);
	TestTrue(TEXT("Disabled target command is handled"), Command->Execute({TEXT("100"), Disabled->GetName()}, World, *GLog));
	TestEqual(TEXT("Non-Demo target remains unchanged"), Disabled->GetCombatEconomyComponent()->GetGold(), int64(600));
	TestFalse(TEXT("Component itself also rejects a disabled bypass"), Disabled->GetCombatEconomyComponent()->SetGoldForDebug(100, Error));
	return true;
}

	/** 原生商店包含常驻金币入口；运行时只保留商店和库存 HUD。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatShopWidgetStructureTest,
	"Combat.UI.Shop.NativeStructureAndLifecycle", CombatEconomyTests::Flags)
bool FCombatShopWidgetStructureTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FCombatAutomationWorldFixture Fixture;
	if (!Fixture.IsValid()) return false;
	UWorld& World = *Fixture.GetWorld();
	ACombatPlayerController* Player = World.SpawnActor<ACombatPlayerController>();
	UCombatItemData* Leaf = CombatEconomyTests::MakeLeaf(Player, TEXT("widget_leaf"), 100);
	UCombatItemData* Upgrade = CombatEconomyTests::MakeRecipe(Player, TEXT("widget_upgrade"), {{Leaf, 2}});
	UCombatShopData* Shop = CombatEconomyTests::MakeShop(Player, {Leaf, Upgrade});
	UCombatEconomyData* Rules = CombatEconomyTests::MakeRules(Player);
	FString Error;
	if (!Player->GetCombatEconomyComponent()->InitializeForMatch(Rules, Shop, false, Error)) return false;
	// 临时 PIE World 没有 LocalPlayer，CreateWidget 会按引擎规则拒绝服务器 Controller；
	// 这里直接构造原生 Widget，并注入同一只读快照，验证 Slate 结构和关闭/打开几何。
	UCombatShopWidget* Widget = NewObject<UCombatShopWidget>(GetTransientPackage());
	if (!TestNotNull(TEXT("Native shop widget"), Widget)) return false;
	const ACombatPlayerHUD* HUDDefaults = GetDefault<ACombatPlayerHUD>();
	TestTrue(TEXT("HUD defaults to the native shop class"), HUDDefaults->ShopWidgetClass == UCombatShopWidget::StaticClass());
	Widget->DisplayView = Player->GetCombatEconomyComponent()->GetEconomyView();
	Widget->DisplayShopData = Shop;
	if (!TestTrue(TEXT("Shop widget initializes"), Widget->Initialize())) return false;
	TSharedPtr<SWidget> ShopSlate = Widget->TakeWidget();
	Widget->NativeConstruct();
	TestTrue(TEXT("Exactly one search input exists"), Widget->SearchBox.IsValid());
	TestTrue(TEXT("Catalog host exists"), Widget->CatalogBox.IsValid());
	TestTrue(TEXT("Recipe host exists"), Widget->RecipeBox.IsValid());
	TestTrue(TEXT("Persistent gold button exists"), Widget->GoldButton.IsValid());
	TestFalse(TEXT("Recipe panel has no scroll container"), Widget->RecipeScrollBox.IsValid());
	TestEqual(TEXT("Recipe panel starts with no auxiliary content"), Widget->RecipeBox->NumSlots(), 0);
	TestEqual(TEXT("Initial shop balance comes from level rules"), Widget->DisplayView.Gold, int64(600));
	TestEqual(TEXT("Catalog node width is compact"), static_cast<double>(UCombatShopWidget::GetCompactNodeSize().X), 48.0);
	TestEqual(TEXT("Catalog node height is compact"), static_cast<double>(UCombatShopWidget::GetCompactNodeSize().Y), 34.0);
	TestEqual(TEXT("Shop layout reference width is 1920"), static_cast<double>(UCombatShopWidget::GetReferenceViewportSize().X), 1920.0);
	TestEqual(TEXT("Shop layout reference height is 1080"), static_cast<double>(UCombatShopWidget::GetReferenceViewportSize().Y), 1080.0);
	TestEqual(TEXT("Shop width is sixty percent of the v0.1 panel"), static_cast<double>(UCombatShopWidget::GetPanelWidthScale()), 0.60);
	TestEqual(TEXT("Shop panel width is fixed from the 1920 baseline"), static_cast<double>(UCombatShopWidget::GetCompactPanelSize().X), 456.0);
	TestEqual(TEXT("Shop panel height is seventy-eight percent of 1080 rounded"), static_cast<double>(UCombatShopWidget::GetCompactPanelSize().Y), 842.0);
	TestEqual(TEXT("Shop top offset is five percent of 1080"), static_cast<double>(UCombatShopWidget::GetPanelTopOffset()), 54.0);
	TestEqual(TEXT("Catalog region height is fifty-five percent of 1080"), static_cast<double>(UCombatShopWidget::GetFixedRegionHeights().X), 594.0);
	TestEqual(TEXT("Recipe region height is fourteen percent of 1080 rounded"), static_cast<double>(UCombatShopWidget::GetFixedRegionHeights().Y), 151.0);
	TestFalse(TEXT("Shop starts closed"), Widget->IsShopOpen());
	TestEqual(TEXT("Closed main panel is collapsed"), Widget->ShopPanel->GetVisibility(), EVisibility::Collapsed);
	TestTrue(TEXT("Gold button remains visible while shop is closed"), Widget->GoldButton->GetVisibility().IsVisible());
	Widget->SetShopOpen(true);
	TestEqual(TEXT("Open main panel is visible"), Widget->ShopPanel->GetVisibility(), EVisibility::Visible);
	TSharedPtr<SWidget> RebuiltShopSlate = Widget->RebuildWidget();
	TestEqual(TEXT("Slate rebuild preserves the open state"), Widget->ShopPanel->GetVisibility(), EVisibility::Visible);
	Widget->SelectItem(Upgrade->GetPrimaryAssetId());
	TestEqual(TEXT("Recipe contains result and direct component nodes"), Widget->RecipeEntries.Num(), 2);
	TestEqual(TEXT("Recipe panel contains only the recipe graph"), Widget->RecipeBox->NumSlots(), 1);
	Widget->SelectItem(Leaf->GetPrimaryAssetId());
	TestEqual(TEXT("Leaf selection clears all prior recipe nodes"), Widget->RecipeEntries.Num(), 0);
	TestEqual(TEXT("Leaf without a recipe leaves the panel blank"), Widget->RecipeBox->NumSlots(), 0);
	TestEqual(TEXT("Unowned structure does not bind a server delegate"),
		Player->GetCombatEconomyComponent()->OnEconomyViewChanged.GetAllObjects().Num(), 0);
	Widget->NativeDestruct();
	TestEqual(TEXT("Destruct releases economy subscription"),
		Player->GetCombatEconomyComponent()->OnEconomyViewChanged.GetAllObjects().Num(), 0);
	Widget->ReleaseSlateResources(true);
	RebuiltShopSlate.Reset();
	ShopSlate.Reset();
	return true;
}

#endif
