#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "HAL/IConsoleManager.h"
#include "Misc/AutomationTest.h"

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
#include "Combat/UI/CombatStashWidget.h"
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
		Request.ExpectedStashRevision = Economy.GetStashRevision();
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

/** 单一金币、储藏处交付、自动合成、退款和上限在同一服务器组件闭环。 */
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
		Branch->GetPrimaryAssetId(), Economy->GetEconomyRevision(), Economy->GetStashRevision(), ResultHandle, Failure));
	TestEqual(TEXT("Basic purchase charges one price"), Economy->GetGold(), int64(500));
	TestEqual(TEXT("Basic purchase enters stash"), Economy->GetStashItemCount(), 1);
	TestTrue(TEXT("Upgrade purchase consumes owned branch and buys missing leaves"), Economy->PurchaseItem(
		Blade->GetPrimaryAssetId(), Economy->GetEconomyRevision(), Economy->GetStashRevision(), ResultHandle, Failure));
	TestEqual(TEXT("Upgrade charges only missing leaves"), Economy->GetGold(), int64(200));
	TestEqual(TEXT("Nested result replaces its components"), Economy->GetStashItemCount(), 1);

	const int32 ResultRevision = Economy->FindStashItemRevision(ResultHandle);
	TestTrue(TEXT("Fresh purchase receives full refund"), Economy->SellStashItem(
		ResultHandle, ResultRevision, Economy->GetEconomyRevision(), Economy->GetStashRevision(), Failure));
	TestEqual(TEXT("Refund restores the complete purchase chain"), Economy->GetGold(), int64(600));
	TestEqual(TEXT("Sold item leaves stash"), Economy->GetStashItemCount(), 0);
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
		Branch->GetPrimaryAssetId(), Economy->GetEconomyRevision(), Economy->GetStashRevision(),
		DelayedRefundHandle, Failure));
	TestEqual(TEXT("Delayed refund purchase charges price"), Economy->GetGold(), int64(0));
	const double PurchaseTime = World.GetTimeSeconds();
	World.TimeSeconds = PurchaseTime + 11.0;
	TestTrue(TEXT("Temporary world advances beyond refund window"), World.GetTimeSeconds() >= PurchaseTime + 10.0);
	const int32 DelayedItemRevision = Economy->FindStashItemRevision(DelayedRefundHandle);
	TestTrue(TEXT("Delayed refund sale succeeds"), Economy->SellStashItem(
		DelayedRefundHandle, DelayedItemRevision, Economy->GetEconomyRevision(), Economy->GetStashRevision(), Failure));
	TestEqual(TEXT("Delayed sale returns configured half value"), Economy->GetGold(), int64(50));
	TestTrue(TEXT("Set balance for unsellable item"), Economy->SetGoldForDebug(100, Error));
	FCombatItemHandle UnsellableHandle;
	TestTrue(TEXT("Unsellable purchase succeeds"), Economy->PurchaseItem(
		Unsellable->GetPrimaryAssetId(), Economy->GetEconomyRevision(), Economy->GetStashRevision(),
		UnsellableHandle, Failure));
	const int32 UnsellableRevision = Economy->FindStashItemRevision(UnsellableHandle);
	TestFalse(TEXT("Unsellable item cannot be sold"), Economy->SellStashItem(
		UnsellableHandle, UnsellableRevision, Economy->GetEconomyRevision(), Economy->GetStashRevision(), Failure));
	TestEqual(TEXT("Rejected unsellable sale keeps balance"), Economy->GetGold(), int64(0));
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

/** 储藏处单件与全部取出保持实例，容量不足时只移动可容纳的部分。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatEconomyStashTransferTest,
	"Combat.Economy.StashTransferAndTakeAll", CombatEconomyTests::Flags)
bool FCombatEconomyStashTransferTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FCombatAutomationWorldFixture Fixture;
	if (!Fixture.IsValid()) return false;
	UWorld& World = *Fixture.GetWorld();
	ACombatPlayerController* Player = World.SpawnActor<ACombatPlayerController>();
	ACombatUnitCharacter* Unit = CombatEconomyTests::SpawnUnit(World, Player, TEXT("stash_transfer_unit"));
	if (!TestNotNull(TEXT("Commanded unit"), Unit)) return false;
	UCombatItemData* Leaf = CombatEconomyTests::MakeLeaf(Player, TEXT("stash_transfer_leaf"), 100);
	UCombatShopData* Shop = CombatEconomyTests::MakeShop(Player, {Leaf});
	UCombatEconomyData* Rules = CombatEconomyTests::MakeRules(Player);
	UCombatEconomyComponent* Economy = Player->GetCombatEconomyComponent();
	FString Error;
	if (!TestTrue(TEXT("Transfer economy initializes"), Economy->InitializeForMatch(Rules, Shop, false, Error))) return false;

	FGameplayTag Failure;
	FCombatItemHandle StashHandle;
	TestTrue(TEXT("First stash purchase"), Economy->PurchaseItem(Leaf->GetPrimaryAssetId(),
		Economy->GetEconomyRevision(), Economy->GetStashRevision(), StashHandle, Failure));
	const int32 StashItemRevision = Economy->FindStashItemRevision(StashHandle);
	FCombatItemHandle InventoryHandle;
	TestTrue(TEXT("Single stash transfer"), Economy->TransferStashItem(StashHandle, StashItemRevision,
		Economy->GetEconomyRevision(), Economy->GetStashRevision(), InventoryHandle, Failure));
	TestEqual(TEXT("Single transfer empties stash"), Economy->GetStashItemCount(), 0);
	TestEqual(TEXT("Single transfer preserves one inventory instance"), Unit->GetCombatInventoryComponent()->GetItemCount(), 1);
	TArray<FCombatItemView> InventoryViews;
	Unit->GetCombatInventoryComponent()->BuildViews(InventoryViews);
	const FCombatItemView* TransferredView = InventoryViews.FindByPredicate(
		[InventoryHandle](const FCombatItemView& View) { return View.Handle == InventoryHandle; });
	TestTrue(TEXT("Entering an equipped slot applies the existing re-equip wait"),
		TransferredView && TransferredView->EnabledAt > World.GetTimeSeconds());

	for (int32 Index = 0; Index < 7; ++Index)
	{
		FCombatItemHandle Given;
		TestTrue(TEXT("Fill hero inventory"), Unit->GetCombatInventoryComponent()->GiveItem(Leaf, 1, Given, Failure));
	}
	FCombatItemHandle FirstQueued;
	FCombatItemHandle SecondQueued;
	TestTrue(TEXT("Queue first remaining stash item"), Economy->PurchaseItem(Leaf->GetPrimaryAssetId(),
		Economy->GetEconomyRevision(), Economy->GetStashRevision(), FirstQueued, Failure));
	TestTrue(TEXT("Queue second remaining stash item"), Economy->PurchaseItem(Leaf->GetPrimaryAssetId(),
		Economy->GetEconomyRevision(), Economy->GetStashRevision(), SecondQueued, Failure));
	int32 Moved = 0;
	TestTrue(TEXT("Take all succeeds when at least one item moves"), Economy->TakeAllStashItems(
		Economy->GetEconomyRevision(), Economy->GetStashRevision(), Moved, Failure));
	TestEqual(TEXT("Take all fills the final hero slot"), Moved, 1);
	TestEqual(TEXT("One item remains safely in stash"), Economy->GetStashItemCount(), 1);
	TestEqual(TEXT("Hero inventory reaches nine slots"), Unit->GetCombatInventoryComponent()->GetItemCount(), CombatItems::TotalSlots);
	const int32 StableStashRevision = Economy->GetStashRevision();
	TestFalse(TEXT("Full inventory rejects a second take-all"), Economy->TakeAllStashItems(
		Economy->GetEconomyRevision(), StableStashRevision, Moved, Failure));
	TestEqual(TEXT("Rejected take-all keeps the stash revision"), Economy->GetStashRevision(), StableStashRevision);
	TestEqual(TEXT("Rejected take-all loses no item"), Economy->GetStashItemCount(), 1);
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

/** 原生商店与储藏室分别构造，并保持紧凑节点尺寸和独立生命周期。 */
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
	UCombatStashWidget* Stash = NewObject<UCombatStashWidget>(GetTransientPackage());
	if (!TestNotNull(TEXT("Native shop widget"), Widget) || !TestNotNull(TEXT("Native stash widget"), Stash)) return false;
	const ACombatPlayerHUD* HUDDefaults = GetDefault<ACombatPlayerHUD>();
	TestTrue(TEXT("HUD defaults to the native shop class"), HUDDefaults->ShopWidgetClass == UCombatShopWidget::StaticClass());
	TestTrue(TEXT("HUD defaults to the native stash class"), HUDDefaults->StashWidgetClass == UCombatStashWidget::StaticClass());
	Widget->DisplayView = Player->GetCombatEconomyComponent()->GetEconomyView();
	Widget->DisplayShopData = Shop;
	Stash->DisplayView = Widget->DisplayView;
	if (!TestTrue(TEXT("Shop widget initializes"), Widget->Initialize())
		|| !TestTrue(TEXT("Stash widget initializes"), Stash->Initialize())) return false;
	TSharedPtr<SWidget> ShopSlate = Widget->TakeWidget();
	TSharedPtr<SWidget> StashSlate = Stash->TakeWidget();
	Widget->NativeConstruct();
	Stash->NativeConstruct();
	TestTrue(TEXT("Exactly one search input exists"), Widget->SearchBox.IsValid());
	TestTrue(TEXT("Catalog host exists"), Widget->CatalogBox.IsValid());
	TestTrue(TEXT("Recipe host exists"), Widget->RecipeBox.IsValid());
	TestFalse(TEXT("Recipe panel has no scroll container"), Widget->RecipeScrollBox.IsValid());
	TestEqual(TEXT("Recipe panel starts with no auxiliary content"), Widget->RecipeBox->NumSlots(), 0);
	TestTrue(TEXT("Shop and stash have separate Slate roots"), ShopSlate.Get() != StashSlate.Get());
	TestTrue(TEXT("Stash has gold and take-all controls"), Stash->GoldButton.IsValid() && Stash->TakeAllButton.IsValid());
	TestEqual(TEXT("Six stash hit targets remain constructed"), Stash->StashHitWidgets.Num(), CombatEconomy::StashSlots);
	TestEqual(TEXT("Initial shop balance comes from level rules"), Widget->DisplayView.Gold, int64(600));
	TestEqual(TEXT("Initial stash balance comes from level rules"), Stash->DisplayView.Gold, int64(600));
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
	TestEqual(TEXT("Stash slot width is fixed"), static_cast<double>(UCombatStashWidget::GetCompactSlotSize().X), 42.0);
	TestEqual(TEXT("Stash slot height is fixed"), static_cast<double>(UCombatStashWidget::GetCompactSlotSize().Y), 32.0);
	TestEqual(TEXT("Stash action width is fixed"), static_cast<double>(UCombatStashWidget::GetCompactActionSize().X), 112.0);
	TestEqual(TEXT("Stash action height is fixed"), static_cast<double>(UCombatStashWidget::GetCompactActionSize().Y), 34.0);
	TestFalse(TEXT("Shop starts closed while stash stays resident"), Widget->IsShopOpen());
	TestEqual(TEXT("Closed main panel is collapsed"), Widget->ShopPanel->GetVisibility(), EVisibility::Collapsed);
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
	Stash->SetShopWidget(Widget);
	TestTrue(TEXT("Stash links to shop through a weak UI reference"), Stash->ShopWidget.Get() == Widget);
	Stash->SetShopWidget(nullptr);
	TestFalse(TEXT("Stash link can be detached before teardown"), Stash->ShopWidget.IsValid());
	TestEqual(TEXT("Unowned structure does not bind a server delegate"),
		Player->GetCombatEconomyComponent()->OnEconomyViewChanged.GetAllObjects().Num(), 0);
	Widget->NativeDestruct();
	Stash->NativeDestruct();
	TestEqual(TEXT("Destruct releases economy subscription"),
		Player->GetCombatEconomyComponent()->OnEconomyViewChanged.GetAllObjects().Num(), 0);
	Widget->ReleaseSlateResources(true);
	Stash->ReleaseSlateResources(true);
	RebuiltShopSlate.Reset();
	ShopSlate.Reset();
	StashSlate.Reset();
	return true;
}

#endif
