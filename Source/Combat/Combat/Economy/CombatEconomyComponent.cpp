#include "Combat/Economy/CombatEconomyComponent.h"

#include "Combat/Economy/CombatEconomyData.h"
#include "Combat/Economy/CombatShopData.h"
#include "Combat/Items/CombatItemData.h"
#include "Combat/Items/CombatItemSubsystem.h"
#include "Combat/Items/CombatInventoryComponent.h"
#include "Combat/Ability/CombatAbilitySystemComponent.h"
#include "Combat/Ability/CombatGameplayAbility.h"
#include "Combat/Core/CombatTags.h"
#include "Combat/Log/CombatEventSubsystem.h"
#include "Combat/Scheduling/CombatSchedulerSubsystem.h"
#include "CombatPlayerController.h"
#include "Combat/Unit/CombatUnitCharacter.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"

UCombatEconomyComponent::UCombatEconomyComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
	StashSlots.SetNum(CombatEconomy::StashSlots);
	ReplicatedView.StashItems.SetNum(CombatEconomy::StashSlots);
}

void UCombatEconomyComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(UCombatEconomyComponent, ReplicatedView, COND_OwnerOnly);
}

ACombatPlayerController* UCombatEconomyComponent::GetCombatPlayer() const
{
	return Cast<ACombatPlayerController>(GetOwner());
}

UCombatItemSubsystem* UCombatEconomyComponent::GetItems() const
{
	return GetWorld() ? GetWorld()->GetSubsystem<UCombatItemSubsystem>() : nullptr;
}

void UCombatEconomyComponent::AdvanceRevision(int32& Revision)
{
	Revision = Revision >= MAX_int32 ? 1 : Revision + 1;
	if (Revision <= 0) Revision = 1;
}

void UCombatEconomyComponent::AdvanceEconomyRevision()
{
	AdvanceRevision(ReplicatedView.EconomyRevision);
}

void UCombatEconomyComponent::AdvanceStashRevision()
{
	AdvanceRevision(ReplicatedView.StashRevision);
}

bool UCombatEconomyComponent::InitializeForMatch(UCombatEconomyData* EconomyData, UCombatShopData* InShopData,
	const bool bEnableDebugCommands, FString& OutError)
{
	OutError.Reset();
	ACombatPlayerController* Player = GetCombatPlayer();
	if (!Player || !Player->HasAuthority() || bEnding)
	{
		OutError = TEXT("Economy initialization requires an authoritative PlayerController");
		return false;
	}
	if (bInitialized)
	{
		OutError = TEXT("Economy component is already initialized for this match");
		return false;
	}
	if (!EconomyData || !InShopData || !EconomyData->ValidateRuntime(OutError) || !InShopData->ValidateRuntime(OutError))
	{
		if (OutError.IsEmpty()) OutError = TEXT("Economy and shop data are required");
		return false;
	}

	FrozenGoldCap = EconomyData->GoldCap;
	FrozenPassiveGoldPerMinute = EconomyData->PassiveGoldPerMinute;
	FrozenFullRefundSeconds = EconomyData->FullRefundSeconds;
	FrozenSellValueBasisPoints = EconomyData->SellValueBasisPoints;
	ShopData = InShopData;
	bDebugCommandsEnabled = bEnableDebugCommands;
	bInitialized = true;
	ReplicatedView.Gold = EconomyData->StartingGold;
	ReplicatedView.GoldCap = FrozenGoldCap;
	ReplicatedView.PassiveGoldPerMinute = FrozenPassiveGoldPerMinute;
	ReplicatedView.ShopDefinitionId = InShopData->GetPrimaryAssetId();
	ReplicatedView.EconomyRevision = 1;
	ReplicatedView.StashRevision = 1;
	PassiveOriginTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	CreditedPassiveGold = 0;
	RefreshView();
	StartPassiveIncome();
	return true;
}

void UCombatEconomyComponent::StartPassiveIncome()
{
	if (!bInitialized || FrozenPassiveGoldPerMinute <= 0 || !GetWorld()) return;
	if (UCombatSchedulerSubsystem* Scheduler = GetWorld()->GetSubsystem<UCombatSchedulerSubsystem>())
	{
		PassiveIncomeSchedule = Scheduler->ScheduleRepeating(this, 1.0, 1.0, 0,
			ECombatCatchUpPolicy::Coalesce,
			FCombatScheduledDelegate::CreateWeakLambda(this,
				[this](const FCombatScheduledTickContext& Context) { ApplyPassiveIncome(Context.ActualTime); }));
	}
}

void UCombatEconomyComponent::ApplyPassiveIncome(const double CurrentGameTime)
{
	if (!bInitialized || bEnding || FrozenPassiveGoldPerMinute <= 0 || !GetWorld()) return;
	const double WorldGameTime = CurrentGameTime >= 0.0 && FMath::IsFinite(CurrentGameTime)
		? CurrentGameTime : static_cast<double>(GetWorld()->GetTimeSeconds());
	const double Elapsed = FMath::Max(0.0, WorldGameTime - PassiveOriginTime);
	const long double Accumulated = static_cast<long double>(Elapsed)
		* static_cast<long double>(FrozenPassiveGoldPerMinute) / 60.0L;
	const int64 OwedTotal = static_cast<int64>(FMath::FloorToDouble(static_cast<double>(Accumulated)));
	if (OwedTotal <= CreditedPassiveGold) return;
	const int64 Delta = OwedTotal - CreditedPassiveGold;
	CreditedPassiveGold = OwedTotal;
	AddGold(Delta, TEXT("PassiveIncome"));
}

bool UCombatEconomyComponent::AddGold(const int64 Amount, const FName Reason)
{
	ACombatPlayerController* Player = GetCombatPlayer();
	if (!bInitialized || bEnding || bMutating || !Player || !Player->HasAuthority() || Amount < 0) return false;
	if (Amount == 0 || ReplicatedView.Gold >= FrozenGoldCap) return true;
	const int64 Room = FrozenGoldCap - ReplicatedView.Gold;
	const int64 Applied = FMath::Min(Amount, Room);
	if (Applied <= 0) return true;
	const int64 PreviousGold = ReplicatedView.Gold;
	ReplicatedView.Gold += Applied;
	AdvanceEconomyRevision();
	EmitEconomyEvent(CombatTags::Event_Combat_GoldChanged, {}, Reason, PreviousGold);
	RefreshView();
	return true;
}

bool UCombatEconomyComponent::SetGoldForDebug(const int64 Amount, FString& OutError)
{
	OutError.Reset();
#if UE_BUILD_SHIPPING
	OutError = TEXT("Economy debug commands are unavailable in Shipping builds");
	return false;
#else
	ACombatPlayerController* Player = GetCombatPlayer();
	if (!bDebugCommandsEnabled)
	{
		OutError = TEXT("Economy debug commands are disabled for this match");
		return false;
	}
	if (!bInitialized || bEnding || bMutating || !Player || !Player->HasAuthority())
	{
		OutError = TEXT("Debug gold requires an initialized authoritative economy");
		return false;
	}
	if (Amount < 0 || Amount > FrozenGoldCap)
	{
		OutError = FString::Printf(TEXT("Gold must be in [0, %lld]"), FrozenGoldCap);
		return false;
	}
	if (ReplicatedView.Gold != Amount)
	{
		const int64 PreviousGold = ReplicatedView.Gold;
		ReplicatedView.Gold = Amount;
		AdvanceEconomyRevision();
		EmitEconomyEvent(CombatTags::Event_Combat_GoldChanged, {}, TEXT("DebugCommand"), PreviousGold);
		RefreshView();
	}
	return true;
#endif
}

bool UCombatEconomyComponent::ValidateTransactionRevisions(const int32 ExpectedEconomyRevision,
	const int32 ExpectedStashRevision, FGameplayTag& OutFailure) const
{
	OutFailure = {};
	const ACombatPlayerController* Player = GetCombatPlayer();
	if (!Player || !Player->HasAuthority()) { OutFailure = CombatTags::Failure_Authority; return false; }
	if (!bInitialized || bEnding || !ShopData || !GetItems()) { OutFailure = CombatTags::Failure_Economy_Uninitialized; return false; }
	if (bMutating) { OutFailure = CombatTags::Failure_Item_Busy; return false; }
	if (ExpectedEconomyRevision != ReplicatedView.EconomyRevision
		|| ExpectedStashRevision != ReplicatedView.StashRevision)
	{
		OutFailure = CombatTags::Failure_Economy_Stale;
		return false;
	}
	return true;
}

bool UCombatEconomyComponent::ResolveConsumedEntries(const TArray<FPrimaryAssetId>& Definitions,
	TArray<FConsumedEntry>& OutEntries, FGameplayTag& OutFailure) const
{
	OutEntries.Reset();
	UCombatItemSubsystem* Items = GetItems();
	for (const FPrimaryAssetId& DefinitionId : Definitions)
	{
		FConsumedEntry* Chosen = nullptr;
		for (const FCombatItemHandle Handle : StashSlots)
		{
			UCombatItemInstance* Item = Items ? Items->FindMutable(Handle) : nullptr;
			if (!Item || Item->StashOwner != GetCombatPlayer()
				|| !Item->Definition || Item->Definition->GetPrimaryAssetId() != DefinitionId) continue;
			FConsumedEntry* Existing = OutEntries.FindByPredicate(
				[Item](const FConsumedEntry& Entry) { return Entry.Item == Item; });
			const int32 AlreadyReserved = Existing ? Existing->Quantity : 0;
			if (AlreadyReserved >= Item->Quantity) continue;
			if (!Existing) Existing = &OutEntries.AddDefaulted_GetRef();
			Existing->Item = Item;
			Chosen = Existing;
			break;
		}
		if (!Chosen)
		{
			OutFailure = CombatTags::Failure_Economy_Stale;
			OutEntries.Reset();
			return false;
		}
		++Chosen->Quantity;
	}
	return true;
}

bool UCombatEconomyComponent::PurchaseItem(const FPrimaryAssetId& ItemDefinitionId,
	const int32 ExpectedEconomyRevision, const int32 ExpectedStashRevision,
	FCombatItemHandle& OutResultHandle, FGameplayTag& OutFailure)
{
	OutResultHandle = {};
	if (!ValidateTransactionRevisions(ExpectedEconomyRevision, ExpectedStashRevision, OutFailure)) return false;

	TArray<FPrimaryAssetId> OwnedDefinitions;
	UCombatItemSubsystem* Items = GetItems();
	for (const FCombatItemHandle Handle : StashSlots)
	{
		const UCombatItemInstance* Item = Items->FindItem(Handle);
		if (!Item || Item->StashOwner != GetCombatPlayer() || !Item->Definition) continue;
		for (int32 Quantity = 0; Quantity < Item->Quantity; ++Quantity)
		{
			OwnedDefinitions.Add(Item->Definition->GetPrimaryAssetId());
		}
	}

	FCombatPurchasePlan Plan;
	FString Error;
	if (!ShopData->BuildPurchasePlan(ItemDefinitionId, OwnedDefinitions, Plan, Error))
	{
		OutFailure = ShopData->FindItem(ItemDefinitionId)
			? CombatTags::Failure_Economy_RecipeInvalid : CombatTags::Failure_Economy_CatalogUnavailable;
		return false;
	}
	if (Plan.TotalCost < 0 || ReplicatedView.Gold < Plan.TotalCost)
	{
		OutFailure = CombatTags::Failure_Economy_InsufficientGold;
		return false;
	}
	UCombatItemData* TargetDefinition = ShopData->FindItem(ItemDefinitionId);
	if (!TargetDefinition)
	{
		OutFailure = CombatTags::Failure_Economy_CatalogUnavailable;
		return false;
	}

	TArray<FConsumedEntry> Consumed;
	if (!ResolveConsumedEntries(Plan.ConsumedDefinitions, Consumed, OutFailure)) return false;
	int32 SlotsFreed = 0;
	for (const FConsumedEntry& Entry : Consumed)
	{
		if (Entry.Item && Entry.Quantity == Entry.Item->Quantity) ++SlotsFreed;
	}
	if (GetStashItemCount() - SlotsFreed >= CombatEconomy::StashSlots)
	{
		OutFailure = CombatTags::Failure_Economy_StashFull;
		return false;
	}

	TGuardValue<bool> Guard(bMutating, true);
	UCombatItemInstance* Result = Items->CreateItem(TargetDefinition, 1);
	if (!Result)
	{
		OutFailure = CombatTags::Failure_ActionUnsupported;
		return false;
	}

	double RefundStart = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	int64 RefundPaid = Plan.TotalCost;
	bool bRefundEligible = true;
	float InheritedCooldown = 0.0f;
	double InheritedEnabledAt = 0.0;
	int32 PreferredSlot = INDEX_NONE;
	for (const FConsumedEntry& Entry : Consumed)
	{
		UCombatItemInstance* Item = Entry.Item;
		if (!Item) continue;
		const int32 OriginalQuantity = Item->Quantity;
		const int64 PaidShare = OriginalQuantity > 0
			? (Item->PurchasePaidGold * Entry.Quantity) / OriginalQuantity : 0;
		if (PaidShare > MAX_int64 - RefundPaid)
		{
			Items->DestroyItem(Result->Handle);
			OutFailure = CombatTags::Failure_InvalidNumber;
			return false;
		}
		RefundPaid += PaidShare;
		bRefundEligible &= Item->bRefundEligible && PaidShare > 0;
		RefundStart = FMath::Min(RefundStart, Item->PurchaseWorldTime);
		InheritedCooldown = FMath::Max(InheritedCooldown, Item->GetCooldownRemaining(GetWorld()->GetTimeSeconds()));
		InheritedEnabledAt = FMath::Max(InheritedEnabledAt, Item->EnabledAt);
		if (Entry.Quantity == OriginalQuantity && (PreferredSlot == INDEX_NONE || Item->StashSlot < PreferredSlot))
		{
			PreferredSlot = Item->StashSlot;
		}
	}
	if (PreferredSlot == INDEX_NONE)
	{
		PreferredSlot = StashSlots.IndexOfByPredicate([](const FCombatItemHandle& Handle) { return !Handle.IsValid(); });
	}
	if (!StashSlots.IsValidIndex(PreferredSlot))
	{
		Items->DestroyItem(Result->Handle);
		OutFailure = CombatTags::Failure_Economy_StashFull;
		return false;
	}

	for (const FConsumedEntry& Entry : Consumed)
	{
		UCombatItemInstance* Item = Entry.Item;
		Item->Quantity -= Entry.Quantity;
		AdvanceRevision(Item->Revision);
		if (Item->Quantity == 0)
		{
			if (StashSlots.IsValidIndex(Item->StashSlot) && StashSlots[Item->StashSlot] == Item->Handle)
				StashSlots[Item->StashSlot] = {};
			Items->DestroyItem(Item->Handle);
		}
		else
		{
			Item->PurchasePaidGold = FMath::Max<int64>(0, Item->PurchasePaidGold
				- (Item->PurchasePaidGold * Entry.Quantity) / (Item->Quantity + Entry.Quantity));
		}
	}

	Result->StashOwner = GetCombatPlayer();
	Result->StashSlot = PreferredSlot;
	Result->Slot = INDEX_NONE;
	Result->CooldownRemaining = InheritedCooldown;
	Result->CooldownDuration = InheritedCooldown;
	Result->CooldownCheckpoint = GetWorld()->GetTimeSeconds();
	Result->CooldownRate = CombatItems::BackpackCooldownRate;
	Result->EnabledAt = InheritedEnabledAt;
	Result->PurchasePaidGold = RefundPaid;
	Result->PurchaseWorldTime = RefundStart;
	Result->bRefundEligible = bRefundEligible;
	StashSlots[PreferredSlot] = Result->Handle;
	const int64 PreviousGold = ReplicatedView.Gold;
	ReplicatedView.Gold -= Plan.TotalCost;
	const FCombatSourceContext PurchasedSource = Result->MakeSource();
	OutResultHandle = Result->Handle;
	StabilizeStashCrafting(OutResultHandle);
	AdvanceEconomyRevision();
	AdvanceStashRevision();
	EmitEconomyEvent(CombatTags::Event_Combat_ItemPurchased, PurchasedSource, TEXT("Purchased"), PreviousGold);
	if (!TargetDefinition->Recipe.IsEmpty())
	{
		EmitEconomyEvent(CombatTags::Event_Combat_ItemCrafted, PurchasedSource, TEXT("Crafted"), ReplicatedView.Gold);
	}
	RefreshView();
	return true;
}

bool UCombatEconomyComponent::TryCraftStashRecipe(UCombatItemData& TargetDefinition, FCombatItemHandle& OutHandle)
{
	if (TargetDefinition.Recipe.IsEmpty()) return false;
	TArray<FPrimaryAssetId> DirectIngredients;
	for (const FCombatItemRecipeIngredient& Ingredient : TargetDefinition.Recipe)
	{
		UCombatItemData* Definition = Ingredient.Item.LoadSynchronous();
		if (!Definition) return false;
		for (int32 Index = 0; Index < Ingredient.Quantity; ++Index)
		{
			DirectIngredients.Add(Definition->GetPrimaryAssetId());
		}
	}
	FGameplayTag IgnoredFailure;
	TArray<FConsumedEntry> Consumed;
	if (!ResolveConsumedEntries(DirectIngredients, Consumed, IgnoredFailure)) return false;

	int32 SlotsFreed = 0;
	for (const FConsumedEntry& Entry : Consumed)
	{
		if (Entry.Item && Entry.Quantity == Entry.Item->Quantity) ++SlotsFreed;
	}
	if (GetStashItemCount() - SlotsFreed >= CombatEconomy::StashSlots) return false;
	UCombatItemSubsystem* Items = GetItems();
	UCombatItemInstance* Crafted = Items ? Items->CreateItem(&TargetDefinition, 1) : nullptr;
	if (!Crafted) return false;

	const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	double RefundStart = Now;
	int64 RefundPaid = 0;
	bool bRefundEligible = true;
	float InheritedCooldown = 0.0f;
	double InheritedEnabledAt = 0.0;
	int32 PreferredSlot = INDEX_NONE;
	for (const FConsumedEntry& Entry : Consumed)
	{
		UCombatItemInstance* Item = Entry.Item;
		const int32 OriginalQuantity = Item->Quantity;
		const int64 PaidShare = OriginalQuantity > 0
			? (Item->PurchasePaidGold * Entry.Quantity) / OriginalQuantity : 0;
		if (PaidShare > MAX_int64 - RefundPaid)
		{
			Items->DestroyItem(Crafted->Handle);
			return false;
		}
		RefundPaid += PaidShare;
		bRefundEligible &= Item->bRefundEligible && PaidShare > 0;
		RefundStart = FMath::Min(RefundStart, Item->PurchaseWorldTime);
		InheritedCooldown = FMath::Max(InheritedCooldown, Item->GetCooldownRemaining(Now));
		InheritedEnabledAt = FMath::Max(InheritedEnabledAt, Item->EnabledAt);
		if (Entry.Quantity == OriginalQuantity && (PreferredSlot == INDEX_NONE || Item->StashSlot < PreferredSlot))
		{
			PreferredSlot = Item->StashSlot;
		}
	}
	if (PreferredSlot == INDEX_NONE)
	{
		PreferredSlot = StashSlots.IndexOfByPredicate([](const FCombatItemHandle& Handle) { return !Handle.IsValid(); });
	}
	if (!StashSlots.IsValidIndex(PreferredSlot))
	{
		Items->DestroyItem(Crafted->Handle);
		return false;
	}

	for (const FConsumedEntry& Entry : Consumed)
	{
		UCombatItemInstance* Item = Entry.Item;
		const int32 OriginalQuantity = Item->Quantity;
		const int64 PaidShare = OriginalQuantity > 0
			? (Item->PurchasePaidGold * Entry.Quantity) / OriginalQuantity : 0;
		Item->Quantity -= Entry.Quantity;
		AdvanceRevision(Item->Revision);
		Item->PurchasePaidGold = FMath::Max<int64>(0, Item->PurchasePaidGold - PaidShare);
		if (Item->Quantity == 0)
		{
			if (StashSlots.IsValidIndex(Item->StashSlot) && StashSlots[Item->StashSlot] == Item->Handle)
				StashSlots[Item->StashSlot] = {};
			Items->DestroyItem(Item->Handle);
		}
	}

	Crafted->StashOwner = GetCombatPlayer();
	Crafted->StashSlot = PreferredSlot;
	Crafted->CooldownRemaining = InheritedCooldown;
	Crafted->CooldownDuration = InheritedCooldown;
	Crafted->CooldownCheckpoint = Now;
	Crafted->CooldownRate = CombatItems::BackpackCooldownRate;
	Crafted->EnabledAt = InheritedEnabledAt;
	Crafted->PurchasePaidGold = RefundPaid;
	Crafted->PurchaseWorldTime = RefundStart;
	Crafted->bRefundEligible = bRefundEligible;
	StashSlots[PreferredSlot] = Crafted->Handle;
	OutHandle = Crafted->Handle;
	EmitEconomyEvent(CombatTags::Event_Combat_ItemCrafted, Crafted->MakeSource(), TEXT("AutoCrafted"), ReplicatedView.Gold);
	return true;
}

void UCombatEconomyComponent::StabilizeStashCrafting(FCombatItemHandle& InOutLastHandle)
{
	if (!ShopData) return;
	TArray<UCombatItemData*> Catalog;
	ShopData->GetCatalogItems(Catalog);
	Catalog.RemoveAll([](const UCombatItemData* Item) { return !Item || Item->Recipe.IsEmpty(); });
	Catalog.Sort([](const UCombatItemData& A, const UCombatItemData& B)
	{
		if (A.CraftPriority != B.CraftPriority) return A.CraftPriority > B.CraftPriority;
		return A.GetPrimaryAssetId().ToString() < B.GetPrimaryAssetId().ToString();
	});
	for (int32 Iteration = 0; Iteration < 256; ++Iteration)
	{
		bool bCrafted = false;
		for (UCombatItemData* Candidate : Catalog)
		{
			FCombatItemHandle CraftedHandle;
			if (!TryCraftStashRecipe(*Candidate, CraftedHandle)) continue;
			// 自动合成可能处理另一组早已存在的组件；仅当调用方跟踪的实例确实被消费时替换回执句柄。
			if (InOutLastHandle.IsValid() && (!GetItems() || !GetItems()->FindItem(InOutLastHandle)))
			{
				InOutLastHandle = CraftedHandle;
			}
			bCrafted = true;
			break;
		}
		if (!bCrafted) break;
	}
}

bool UCombatEconomyComponent::TryCraftInventoryRecipe(UCombatInventoryComponent& Inventory,
	UCombatItemData& TargetDefinition, FCombatItemHandle& OutHandle)
{
	OutHandle = {};
	ACombatUnitCharacter* Unit = Inventory.GetUnit();
	UCombatItemSubsystem* Items = GetItems();
	if (!Unit || !Items || TargetDefinition.Recipe.IsEmpty()) return false;

	TArray<FPrimaryAssetId> DirectIngredients;
	for (const FCombatItemRecipeIngredient& Ingredient : TargetDefinition.Recipe)
	{
		UCombatItemData* Definition = Ingredient.Item.LoadSynchronous();
		if (!Definition) return false;
		for (int32 Index = 0; Index < Ingredient.Quantity; ++Index)
		{
			DirectIngredients.Add(Definition->GetPrimaryAssetId());
		}
	}

	TArray<FConsumedEntry> Consumed;
	for (const FPrimaryAssetId& DefinitionId : DirectIngredients)
	{
		FConsumedEntry* Chosen = nullptr;
		for (const FCombatItemHandle Handle : Inventory.Slots)
		{
			UCombatItemInstance* Item = Items->FindMutable(Handle);
			if (!Item || Item->Holder != Unit || !Item->Definition
				|| Item->Definition->GetPrimaryAssetId() != DefinitionId || Inventory.IsCasting(*Item)) continue;
			FConsumedEntry* Existing = Consumed.FindByPredicate(
				[Item](const FConsumedEntry& Entry) { return Entry.Item == Item; });
			const int32 Reserved = Existing ? Existing->Quantity : 0;
			if (Reserved >= Item->Quantity) continue;
			if (!Existing) Existing = &Consumed.AddDefaulted_GetRef();
			Existing->Item = Item;
			Chosen = Existing;
			break;
		}
		if (!Chosen) return false;
		++Chosen->Quantity;
	}

	int32 DestinationSlot = INDEX_NONE;
	for (int32 Slot = 0; Slot < Inventory.Slots.Num(); ++Slot)
	{
		const FCombatItemHandle Handle = Inventory.Slots[Slot];
		const FConsumedEntry* Entry = Consumed.FindByPredicate(
			[Handle](const FConsumedEntry& Candidate) { return Candidate.Item && Candidate.Item->Handle == Handle; });
		if (!Handle.IsValid() || (Entry && Entry->Quantity == Entry->Item->Quantity))
		{
			DestinationSlot = Slot;
			break;
		}
	}
	if (!CombatItems::IsSlot(DestinationSlot)
		|| (!CombatItems::IsEquipped(DestinationSlot) && !TargetDefinition.bCanEnterBackpack)) return false;

	const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	double RefundStart = Now;
	int64 RefundPaid = 0;
	bool bRefundEligible = true;
	bool bNeedsReequipDelay = false;
	float InheritedCooldown = 0.0f;
	double InheritedEnabledAt = 0.0;
	bool bRequiresUnitBinding = false;
	bool bRequiresTeamBinding = false;
	for (const FConsumedEntry& Entry : Consumed)
	{
		const UCombatItemInstance* Item = Entry.Item;
		if (!Item) return false;
		const int32 OriginalQuantity = Item->Quantity;
		const int64 PaidShare = OriginalQuantity > 0
			? (Item->PurchasePaidGold * Entry.Quantity) / OriginalQuantity : 0;
		if (PaidShare < 0 || RefundPaid > MAX_int64 - PaidShare) return false;
		RefundPaid += PaidShare;
		bRefundEligible &= Item->bRefundEligible && PaidShare > 0;
		RefundStart = FMath::Min(RefundStart, Item->PurchaseWorldTime);
		InheritedCooldown = FMath::Max(InheritedCooldown, Item->GetCooldownRemaining(Now));
		InheritedEnabledAt = FMath::Max(InheritedEnabledAt, Item->EnabledAt);
		bNeedsReequipDelay |= Item->bNeedsReequipDelay || !CombatItems::IsEquipped(Item->Slot);
		if (Item->bBoundUnitAssigned)
		{
			if (Item->BoundUnit != Unit) return false;
			bRequiresUnitBinding = true;
		}
		if (Item->BoundTeam.IsValid())
		{
			if (Item->BoundTeam != Unit->GetCombatTeamId()) return false;
			bRequiresTeamBinding = true;
		}
	}
	// 合成不得把已有的单位/队伍绑定降级为可公开转移的物品。
	if ((bRequiresUnitBinding && TargetDefinition.Sharing != ECombatItemSharing::BoundUnit)
		|| (bRequiresTeamBinding && TargetDefinition.Sharing == ECombatItemSharing::Public)) return false;

	UCombatItemInstance* Crafted = Items->CreateItem(&TargetDefinition, 1);
	if (!Crafted) return false;
	Crafted->Holder = Unit;
	Crafted->Slot = DestinationSlot;
	Crafted->CooldownRemaining = InheritedCooldown;
	Crafted->CooldownDuration = InheritedCooldown;
	Crafted->CooldownCheckpoint = Now;
	Crafted->CooldownRate = CombatItems::IsEquipped(DestinationSlot) ? 1.0f : CombatItems::BackpackCooldownRate;
	Crafted->EnabledAt = InheritedEnabledAt;
	Crafted->bNeedsReequipDelay = bNeedsReequipDelay && !CombatItems::IsEquipped(DestinationSlot);
	if (bNeedsReequipDelay && CombatItems::IsEquipped(DestinationSlot))
	{
		Crafted->EnabledAt = FMath::Max(Crafted->EnabledAt, Now + CombatItems::ReequipDelay);
	}
	Crafted->PurchasePaidGold = RefundPaid;
	Crafted->PurchaseWorldTime = RefundStart;
	Crafted->bRefundEligible = bRefundEligible;
	if (TargetDefinition.Sharing == ECombatItemSharing::BoundUnit)
	{
		Crafted->BoundUnit = Unit;
		Crafted->bBoundUnitAssigned = true;
	}
	else if (TargetDefinition.Sharing == ECombatItemSharing::AlliedTeam)
	{
		Crafted->BoundTeam = Unit->GetCombatTeamId();
	}

	const FCombatItemHandle ReplacedHandle = Inventory.Slots[DestinationSlot];
	Inventory.Slots[DestinationSlot] = Crafted->Handle;
	FGameplayTag AbilityFailure;
	if (TargetDefinition.ActiveAbility
		&& !Unit->GetCombatAbilitySystemComponent()->GrantItemAbility(
			TargetDefinition.ActiveAbility, Crafted->Handle, Crafted->AbilityHandle, AbilityFailure))
	{
		Inventory.Slots[DestinationSlot] = ReplacedHandle;
		Crafted->Holder.Reset();
		Crafted->Slot = INDEX_NONE;
		Items->DestroyItem(Crafted->Handle);
		return false;
	}

	for (const FConsumedEntry& Entry : Consumed)
	{
		UCombatItemInstance* Item = Entry.Item;
		const int32 OriginalQuantity = Item->Quantity;
		const int64 PaidShare = OriginalQuantity > 0
			? (Item->PurchasePaidGold * Entry.Quantity) / OriginalQuantity : 0;
		Item->Quantity -= Entry.Quantity;
		Item->PurchasePaidGold = FMath::Max<int64>(0, Item->PurchasePaidGold - PaidShare);
		AdvanceRevision(Item->Revision);
		if (Item->Quantity > 0) continue;
		if (CombatItems::IsSlot(Item->Slot) && Inventory.Slots[Item->Slot] == Item->Handle)
		{
			Inventory.Slots[Item->Slot] = {};
		}
		Inventory.RemoveEffects(*Item);
		if (Item->AbilityHandle.IsValid())
		{
			FGameplayTag RemoveFailure;
			Unit->GetCombatAbilitySystemComponent()->RemoveCombatAbility(Item->AbilityHandle, RemoveFailure);
		}
		Item->AbilityHandle = {};
		Item->Holder.Reset();
		Item->Slot = INDEX_NONE;
		Items->DestroyItem(Item->Handle);
	}
	Inventory.Slots[DestinationSlot] = Crafted->Handle;
	AdvanceRevision(Crafted->Revision);
	OutHandle = Crafted->Handle;
	Inventory.NotifyChanged(Crafted, TEXT("Crafted"));
	EmitEconomyEvent(CombatTags::Event_Combat_ItemCrafted, Crafted->MakeSource(), TEXT("InventoryCrafted"), ReplicatedView.Gold);
	return true;
}

void UCombatEconomyComponent::StabilizeInventoryCrafting(UCombatInventoryComponent& Inventory,
	FCombatItemHandle& InOutTrackedHandle)
{
	if (!bInitialized || bEnding || bMutating || !ShopData || Inventory.bEnding || Inventory.bMutating) return;
	TArray<UCombatItemData*> Catalog;
	ShopData->GetCatalogItems(Catalog);
	Catalog.RemoveAll([](const UCombatItemData* Item) { return !Item || Item->Recipe.IsEmpty(); });
	Catalog.Sort([](const UCombatItemData& A, const UCombatItemData& B)
	{
		if (A.CraftPriority != B.CraftPriority) return A.CraftPriority > B.CraftPriority;
		return A.GetPrimaryAssetId().ToString() < B.GetPrimaryAssetId().ToString();
	});
	TGuardValue<bool> EconomyGuard(bMutating, true);
	TGuardValue<bool> InventoryGuard(Inventory.bMutating, true);
	for (int32 Iteration = 0; Iteration < 256; ++Iteration)
	{
		bool bCrafted = false;
		for (UCombatItemData* Candidate : Catalog)
		{
			const FCombatItemHandle PreviousTrackedHandle = InOutTrackedHandle;
			FCombatItemHandle CraftedHandle;
			if (!TryCraftInventoryRecipe(Inventory, *Candidate, CraftedHandle)) continue;
			if (PreviousTrackedHandle.IsValid() && !GetItems()->FindItem(PreviousTrackedHandle))
			{
				InOutTrackedHandle = CraftedHandle;
			}
			bCrafted = true;
			break;
		}
		if (!bCrafted) break;
	}
}

bool UCombatEconomyComponent::SellStashItem(const FCombatItemHandle ItemHandle, const int32 ExpectedItemRevision,
	const int32 ExpectedEconomyRevision, const int32 ExpectedStashRevision, FGameplayTag& OutFailure)
{
	if (!ValidateTransactionRevisions(ExpectedEconomyRevision, ExpectedStashRevision, OutFailure)) return false;
	UCombatItemSubsystem* Items = GetItems();
	UCombatItemInstance* Item = Items ? Items->FindMutable(ItemHandle) : nullptr;
	if (!Item || Item->StashOwner != GetCombatPlayer() || Item->Revision != ExpectedItemRevision
		|| !StashSlots.IsValidIndex(Item->StashSlot) || StashSlots[Item->StashSlot] != ItemHandle)
	{
		OutFailure = CombatTags::Failure_Economy_Stale;
		return false;
	}
	if (!Item->Definition || !Item->Definition->bSellable)
	{
		OutFailure = CombatTags::Failure_Economy_NotSellable;
		return false;
	}

	const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	int64 Refund = 0;
	if (Item->bRefundEligible && Item->PurchasePaidGold > 0 && FrozenFullRefundSeconds > 0.0f
		&& Now - Item->PurchaseWorldTime <= FrozenFullRefundSeconds)
	{
		Refund = Item->PurchasePaidGold;
	}
	else
	{
		int64 FullPrice = 0;
		FString Error;
		if (!ShopData->CalculateItemPrice(Item->Definition->GetPrimaryAssetId(), FullPrice, Error)
			|| FullPrice > MAX_int64 / FMath::Max(1, FrozenSellValueBasisPoints))
		{
			OutFailure = CombatTags::Failure_ActionUnsupported;
			return false;
		}
		Refund = (FullPrice * FrozenSellValueBasisPoints) / 10000;
	}
	Refund = FMath::Min(Refund, FrozenGoldCap - ReplicatedView.Gold);

	const FCombatSourceContext SoldSource = Item->MakeSource();
	const int64 PreviousGold = ReplicatedView.Gold;
	TGuardValue<bool> Guard(bMutating, true);
	StashSlots[Item->StashSlot] = {};
	Items->DestroyItem(ItemHandle);
	ReplicatedView.Gold += Refund;
	AdvanceEconomyRevision();
	AdvanceStashRevision();
	EmitEconomyEvent(CombatTags::Event_Combat_ItemSold, SoldSource, TEXT("Sold"), PreviousGold);
	RefreshView();
	return true;
}

bool UCombatEconomyComponent::TransferStashItem(const FCombatItemHandle ItemHandle,
	const int32 ExpectedItemRevision, const int32 ExpectedEconomyRevision, const int32 ExpectedStashRevision,
	FCombatItemHandle& OutInventoryHandle, FGameplayTag& OutFailure)
{
	OutInventoryHandle = {};
	if (!ValidateTransactionRevisions(ExpectedEconomyRevision, ExpectedStashRevision, OutFailure)) return false;
	ACombatUnitCharacter* Unit = GetCombatPlayer() ? GetCombatPlayer()->GetCommandedUnit() : nullptr;
	UCombatInventoryComponent* Inventory = Unit ? Unit->GetCombatInventoryComponent() : nullptr;
	UCombatItemSubsystem* Items = GetItems();
	if (!Unit || Unit->GetLifeState() != ECombatLifeState::Alive || !Inventory)
	{
		OutFailure = CombatTags::Failure_Life_NotAlive;
		return false;
	}
	if (Inventory->bMutating || Inventory->bEnding)
	{
		OutFailure = CombatTags::Failure_Item_Busy;
		return false;
	}
	UCombatItemInstance* Item = Items ? Items->FindMutable(ItemHandle) : nullptr;
	if (!Item || Item->StashOwner != GetCombatPlayer() || Item->Revision != ExpectedItemRevision
		|| !StashSlots.IsValidIndex(Item->StashSlot) || StashSlots[Item->StashSlot] != ItemHandle)
	{
		OutFailure = CombatTags::Failure_Economy_Stale;
		return false;
	}

	const int32 PreviousStashSlot = Item->StashSlot;
	const bool bPreviousNeedsReequipDelay = Item->bNeedsReequipDelay;
	bool bAccepted = false;
	{
		TGuardValue<bool> EconomyGuard(bMutating, true);
		TGuardValue<bool> InventoryGuard(Inventory->bMutating, true);
		Item->StashOwner.Reset();
		Item->StashSlot = INDEX_NONE;
		// 储藏处等同非装备域；进入装备槽必须沿用物品系统既有的重新启用等待。
		Item->bNeedsReequipDelay = true;
		bAccepted = Inventory->AcceptItem(*Item, OutInventoryHandle, OutFailure);
		if (!bAccepted)
		{
			Item->StashOwner = GetCombatPlayer();
			Item->StashSlot = PreviousStashSlot;
			Item->bNeedsReequipDelay = bPreviousNeedsReequipDelay;
		}
		else
		{
			StashSlots[PreviousStashSlot] = {};
			AdvanceStashRevision();
			RefreshView();
		}
	}
	if (bAccepted)
	{
		StabilizeInventoryCrafting(*Inventory, OutInventoryHandle);
		Inventory->ReconcileEffects();
	}
	return bAccepted;
}

bool UCombatEconomyComponent::TakeAllStashItems(const int32 ExpectedEconomyRevision,
	const int32 ExpectedStashRevision, int32& OutMovedItemCount, FGameplayTag& OutFailure)
{
	OutMovedItemCount = 0;
	if (!ValidateTransactionRevisions(ExpectedEconomyRevision, ExpectedStashRevision, OutFailure)) return false;
	ACombatUnitCharacter* Unit = GetCombatPlayer() ? GetCombatPlayer()->GetCommandedUnit() : nullptr;
	UCombatInventoryComponent* Inventory = Unit ? Unit->GetCombatInventoryComponent() : nullptr;
	UCombatItemSubsystem* Items = GetItems();
	if (!Unit || Unit->GetLifeState() != ECombatLifeState::Alive || !Inventory)
	{
		OutFailure = CombatTags::Failure_Life_NotAlive;
		return false;
	}
	if (Inventory->bMutating || Inventory->bEnding)
	{
		OutFailure = CombatTags::Failure_Item_Busy;
		return false;
	}

	{
		TGuardValue<bool> EconomyGuard(bMutating, true);
		TGuardValue<bool> InventoryGuard(Inventory->bMutating, true);
		for (int32 Slot = 0; Slot < StashSlots.Num(); ++Slot)
		{
			UCombatItemInstance* Item = Items ? Items->FindMutable(StashSlots[Slot]) : nullptr;
			if (!Item || Item->StashOwner != GetCombatPlayer()) continue;
			const TWeakObjectPtr<ACombatPlayerController> PreviousOwner = Item->StashOwner;
			const bool bPreviousNeedsReequipDelay = Item->bNeedsReequipDelay;
			Item->StashOwner.Reset();
			Item->StashSlot = INDEX_NONE;
			Item->bNeedsReequipDelay = true;
			FCombatItemHandle ResultHandle;
			FGameplayTag ItemFailure;
			if (!Inventory->AcceptItem(*Item, ResultHandle, ItemFailure))
			{
				Item->StashOwner = PreviousOwner;
				Item->StashSlot = Slot;
				Item->bNeedsReequipDelay = bPreviousNeedsReequipDelay;
				if (!OutFailure.IsValid()) OutFailure = ItemFailure;
				continue;
			}
			StashSlots[Slot] = {};
			++OutMovedItemCount;
		}
		if (OutMovedItemCount > 0)
		{
			AdvanceStashRevision();
			RefreshView();
		}
	}
	if (OutMovedItemCount > 0)
	{
		OutFailure = {};
		FCombatItemHandle UntrackedHandle;
		StabilizeInventoryCrafting(*Inventory, UntrackedHandle);
		Inventory->ReconcileEffects();
		return true;
	}
	if (!OutFailure.IsValid()) OutFailure = CombatTags::Failure_Item_Full;
	return false;
}

int32 UCombatEconomyComponent::GetStashItemCount() const
{
	int32 Count = 0;
	for (const FCombatItemHandle Handle : StashSlots) if (Handle.IsValid()) ++Count;
	return Count;
}

int32 UCombatEconomyComponent::FindStashItemRevision(const FCombatItemHandle Handle) const
{
	const UCombatItemInstance* Item = GetItems() ? GetItems()->FindItem(Handle) : nullptr;
	return Item && Item->StashOwner == GetCombatPlayer() ? Item->Revision : 0;
}

void UCombatEconomyComponent::RefreshView()
{
	ReplicatedView.StashItems.Reset();
	ReplicatedView.StashItems.SetNum(CombatEconomy::StashSlots);
	UCombatItemSubsystem* Items = GetItems();
	const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	for (int32 Slot = 0; Slot < StashSlots.Num(); ++Slot)
	{
		const UCombatItemInstance* Item = Items ? Items->FindItem(StashSlots[Slot]) : nullptr;
		if (!Item || !Item->Definition || Item->StashOwner != GetCombatPlayer()) continue;
		FCombatItemView& View = ReplicatedView.StashItems[Slot];
		View.Handle = Item->Handle;
		View.DefinitionId = Item->Definition->GetPrimaryAssetId();
		View.Revision = Item->Revision;
		View.Quantity = Item->Quantity;
		View.Charges = Item->Charges;
		View.CooldownCheckpoint = Now;
		View.CooldownRemaining = Item->GetCooldownRemaining(Now);
		View.CooldownDuration = Item->CooldownDuration;
		View.CooldownRate = Item->CooldownRate;
		View.EnabledAt = Item->EnabledAt;
	}
	if (ACombatPlayerController* Player = GetCombatPlayer()) Player->ForceNetUpdate();
	OnEconomyViewChanged.Broadcast();
}

void UCombatEconomyComponent::OnRep_EconomyView()
{
	OnEconomyViewChanged.Broadcast();
}

void UCombatEconomyComponent::EmitEconomyEvent(const FGameplayTag EventType,
	const FCombatSourceContext& Source, const FName Action, const int64 PreviousGold) const
{
	UCombatEventSubsystem* Events = GetWorld() ? GetWorld()->GetSubsystem<UCombatEventSubsystem>() : nullptr;
	if (!Events) return;
	FCombatLogRecord Record;
	Record.Context = Events->CreateRootEvent();
	Record.EventType = EventType;
	Record.Source = Source;
	Record.ItemAction = Action;
	Record.GoldDelta = ReplicatedView.Gold - PreviousGold;
	Record.GoldBalance = ReplicatedView.Gold;
	if (const ACombatUnitCharacter* Unit = GetCombatPlayer() ? GetCombatPlayer()->GetCommandedUnit() : nullptr)
	{
		Record.SourceActorId = Unit->GetUniqueID();
		Record.TargetActorId = Unit->GetUniqueID();
		Record.UnitLifeGeneration = Unit->GetLifeGeneration();
	}
	Record.Diagnostic = FString::Printf(TEXT("Action=%s GoldDelta=%lld Gold=%lld EconomyRevision=%d StashRevision=%d"),
		*Action.ToString(), static_cast<long long>(Record.GoldDelta), static_cast<long long>(Record.GoldBalance),
		ReplicatedView.EconomyRevision, ReplicatedView.StashRevision);
	Events->Emit(Record);
}

void UCombatEconomyComponent::ClearStash()
{
	if (UCombatItemSubsystem* Items = GetItems())
	{
		for (const FCombatItemHandle Handle : StashSlots)
		{
			if (UCombatItemInstance* Item = Items->FindMutable(Handle); Item && Item->StashOwner == GetCombatPlayer())
			{
				Items->DestroyItem(Handle);
			}
		}
	}
	for (FCombatItemHandle& Handle : StashSlots) Handle = {};
}

void UCombatEconomyComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	bEnding = true;
	if (UCombatSchedulerSubsystem* Scheduler = GetWorld() ? GetWorld()->GetSubsystem<UCombatSchedulerSubsystem>() : nullptr)
	{
		Scheduler->CancelAllForOwner(this);
	}
	ClearStash();
	ShopData = nullptr;
	Super::EndPlay(EndPlayReason);
}
