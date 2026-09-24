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
	ReplicatedView.InventoryItems.SetNum(CombatItems::TotalSlots);
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

void UCombatEconomyComponent::RefreshInventoryProjection()
{
	if (bInitialized && !bEnding) RefreshView(false, true);
}

FCombatPlayerResourceView UCombatEconomyComponent::GetPlayerResourceView() const
{
	FCombatPlayerResourceView View;
	View.Gold = ReplicatedView.Gold;
	View.GoldCap = ReplicatedView.GoldCap;
	View.PassiveGoldPerMinute = ReplicatedView.PassiveGoldPerMinute;
	View.ResourceRevision = ReplicatedView.EconomyRevision;
	return View;
}

FCombatHeroInventoryView UCombatEconomyComponent::GetHeroInventoryView() const
{
	FCombatHeroInventoryView View;
	View.InventoryRevision = ReplicatedView.InventoryRevision;
	View.Items = ReplicatedView.InventoryItems;
	return View;
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
	PassiveOriginTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	CreditedPassiveGold = 0;
	RefreshView(true, true);
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
	RefreshView(true, false);
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
		RefreshView(true, false);
	}
	return true;
#endif
}

UCombatInventoryComponent* UCombatEconomyComponent::GetCommandedInventory() const
{
	const ACombatPlayerController* Player = GetCombatPlayer();
	const ACombatUnitCharacter* Unit = Player ? Player->GetCommandedUnit() : nullptr;
	return Unit ? Unit->GetCombatInventoryComponent() : nullptr;
}

bool UCombatEconomyComponent::ValidateInventoryTransactionRevisions(
	const int32 ExpectedEconomyRevision, const int32 ExpectedInventoryRevision,
	FGameplayTag& OutFailure, UCombatInventoryComponent*& OutInventory) const
{
	OutFailure = {};
	OutInventory = nullptr;
	const ACombatPlayerController* Player = GetCombatPlayer();
	ACombatUnitCharacter* Unit = Player ? Player->GetCommandedUnit() : nullptr;
	UCombatInventoryComponent* Inventory = Unit ? Unit->GetCombatInventoryComponent() : nullptr;
	if (!Player || !Player->HasAuthority()) { OutFailure = CombatTags::Failure_Authority; return false; }
	if (!bInitialized || bEnding || !ShopData || !GetItems()) { OutFailure = CombatTags::Failure_Economy_Uninitialized; return false; }
	if (!Unit || Unit->GetLifeState() != ECombatLifeState::Alive || !Inventory)
	{
		OutFailure = CombatTags::Failure_Life_NotAlive;
		return false;
	}
	if (bMutating || Inventory->bMutating || Inventory->bEnding)
	{
		OutFailure = CombatTags::Failure_Item_Busy;
		return false;
	}
	if (ExpectedEconomyRevision != ReplicatedView.EconomyRevision
		|| ExpectedInventoryRevision != Inventory->GetRevision())
	{
		OutFailure = CombatTags::Failure_Economy_Stale;
		return false;
	}
	OutInventory = Inventory;
	return true;
}

bool UCombatEconomyComponent::ResolveInventoryConsumedEntries(UCombatInventoryComponent& Inventory,
	const TArray<FPrimaryAssetId>& Definitions, TArray<FConsumedEntry>& OutEntries,
	FGameplayTag& OutFailure) const
{
	OutEntries.Reset();
	OutFailure = {};
	UCombatItemSubsystem* Items = GetItems();
	ACombatUnitCharacter* Unit = Inventory.GetUnit();
	for (const FPrimaryAssetId& DefinitionId : Definitions)
	{
		FConsumedEntry* Chosen = nullptr;
		for (const FCombatItemHandle Handle : Inventory.Slots)
		{
			UCombatItemInstance* Item = Items ? Items->FindMutable(Handle) : nullptr;
			if (!Item || Item->Holder != Unit || Item->bLocked || !Item->Definition
				|| Item->Definition->GetPrimaryAssetId() != DefinitionId || Inventory.IsCasting(*Item)) continue;
			FConsumedEntry* Existing = OutEntries.FindByPredicate(
				[Item](const FConsumedEntry& Entry) { return Entry.Item == Item; });
			const int32 Reserved = Existing ? Existing->Quantity : 0;
			if (Reserved >= Item->Quantity) continue;
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

bool UCombatEconomyComponent::CommitInventoryResult(UCombatInventoryComponent& Inventory,
	UCombatItemData& TargetDefinition, const TArray<FConsumedEntry>& Consumed,
	const int64 AdditionalPaidGold, FCombatItemHandle& OutHandle, FGameplayTag& OutFailure,
	const TCHAR* Action)
{
	OutHandle = {};
	OutFailure = {};
	ACombatUnitCharacter* Unit = Inventory.GetUnit();
	UCombatItemSubsystem* Items = GetItems();
	if (!Unit || !Items || Inventory.bEnding || AdditionalPaidGold < 0)
	{
		OutFailure = CombatTags::Failure_ActionUnsupported;
		return false;
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
		|| (!CombatItems::IsEquipped(DestinationSlot) && !TargetDefinition.bCanEnterBackpack))
	{
		OutFailure = CombatTags::Failure_Item_Full;
		return false;
	}

	const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	double RefundStart = Now;
	int64 RefundPaid = AdditionalPaidGold;
	bool bRefundEligible = AdditionalPaidGold > 0;
	bool bNeedsReequipDelay = false;
	float InheritedCooldown = 0.0f;
	double InheritedEnabledAt = 0.0;
	bool bRequiresUnitBinding = false;
	bool bRequiresTeamBinding = false;
	for (const FConsumedEntry& Entry : Consumed)
	{
		UCombatItemInstance* Item = Entry.Item;
		if (!Item || Entry.Quantity <= 0 || Item->Holder != Unit || Item->bLocked
			|| Entry.Quantity > Item->Quantity || Inventory.IsCasting(*Item))
		{
			OutFailure = CombatTags::Failure_Economy_Stale;
			return false;
		}
		const int32 OriginalQuantity = Item->Quantity;
		const int64 PaidShare = OriginalQuantity > 0
			? (Item->PurchasePaidGold * Entry.Quantity) / OriginalQuantity : 0;
		if (PaidShare < 0 || RefundPaid > MAX_int64 - PaidShare)
		{
			OutFailure = CombatTags::Failure_InvalidNumber;
			return false;
		}
		RefundPaid += PaidShare;
		bRefundEligible &= Item->bRefundEligible && PaidShare > 0;
		RefundStart = FMath::Min(RefundStart, Item->PurchaseWorldTime);
		InheritedCooldown = FMath::Max(InheritedCooldown, Item->GetCooldownRemaining(Now));
		InheritedEnabledAt = FMath::Max(InheritedEnabledAt, Item->EnabledAt);
		bNeedsReequipDelay |= Item->bNeedsReequipDelay || !CombatItems::IsEquipped(Item->Slot);
		if (Item->bBoundUnitAssigned)
		{
			if (Item->BoundUnit != Unit) { OutFailure = CombatTags::Failure_Item_Bound; return false; }
			bRequiresUnitBinding = true;
		}
		if (Item->BoundTeam.IsValid())
		{
			if (Item->BoundTeam != Unit->GetCombatTeamId()) { OutFailure = CombatTags::Failure_Item_Bound; return false; }
			bRequiresTeamBinding = true;
		}
	}
	if ((bRequiresUnitBinding && TargetDefinition.Sharing != ECombatItemSharing::BoundUnit)
		|| (bRequiresTeamBinding && TargetDefinition.Sharing == ECombatItemSharing::Public))
	{
		OutFailure = CombatTags::Failure_Item_Bound;
		return false;
	}

	UCombatItemInstance* Crafted = Items->CreateItem(&TargetDefinition, 1);
	if (!Crafted)
	{
		OutFailure = CombatTags::Failure_ActionUnsupported;
		return false;
	}
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
	Crafted->bLocked = false;
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
		OutFailure = AbilityFailure.IsValid() ? AbilityFailure : CombatTags::Failure_ActionUnsupported;
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
	Inventory.NotifyChanged(Crafted, Action ? Action : TEXT("Crafted"));
	return true;
}

bool UCombatEconomyComponent::PurchaseItem(const FPrimaryAssetId& ItemDefinitionId,
	const int32 ExpectedEconomyRevision, const int32 ExpectedInventoryRevision,
	FCombatItemHandle& OutResultHandle, FGameplayTag& OutFailure)
{
	OutResultHandle = {};
	UCombatInventoryComponent* Inventory = nullptr;
	if (!ValidateInventoryTransactionRevisions(ExpectedEconomyRevision, ExpectedInventoryRevision, OutFailure, Inventory)) return false;

	TArray<FPrimaryAssetId> OwnedDefinitions;
	UCombatItemSubsystem* Items = GetItems();
	for (const FCombatItemHandle Handle : Inventory->Slots)
	{
		const UCombatItemInstance* Item = Items->FindItem(Handle);
		if (!Item || Item->Holder != Inventory->GetUnit() || Item->bLocked || !Item->Definition) continue;
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
	if (!ResolveInventoryConsumedEntries(*Inventory, Plan.ConsumedDefinitions, Consumed, OutFailure)) return false;
	const int64 PreviousGold = ReplicatedView.Gold;
	FCombatSourceContext PurchasedSource;
	{
		TGuardValue<bool> EconomyGuard(bMutating, true);
		TGuardValue<bool> InventoryGuard(Inventory->bMutating, true);
		if (!CommitInventoryResult(*Inventory, *TargetDefinition, Consumed, Plan.TotalCost,
			OutResultHandle, OutFailure, TEXT("Purchased"))) return false;
		const UCombatItemInstance* Result = Items->FindItem(OutResultHandle);
		if (Result) PurchasedSource = Result->MakeSource();
		ReplicatedView.Gold -= Plan.TotalCost;
	}
	AdvanceEconomyRevision();
	FCombatItemHandle TrackedHandle = OutResultHandle;
	StabilizeInventoryCrafting(*Inventory, TrackedHandle);
	OutResultHandle = TrackedHandle;
	Inventory->ReconcileEffects();
	EmitEconomyEvent(CombatTags::Event_Combat_ItemPurchased, PurchasedSource, TEXT("Purchased"), PreviousGold);
	if (!TargetDefinition->Recipe.IsEmpty())
	{
		EmitEconomyEvent(CombatTags::Event_Combat_ItemCrafted, PurchasedSource, TEXT("Crafted"), ReplicatedView.Gold);
	}
	RefreshView(true, true);
	return true;
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
			if (!Item || Item->Holder != Unit || Item->bLocked || !Item->Definition
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
	{
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
	// 自动合成会在库存修订上产生额外变化；退出事务保护后立即刷新给 HUD/RPC 使用的投影。
	RefreshView(false, true);
}

bool UCombatEconomyComponent::SellInventoryItem(const FCombatItemHandle ItemHandle,
	const int32 ExpectedItemRevision, const int32 ExpectedEconomyRevision,
	const int32 ExpectedInventoryRevision, FGameplayTag& OutFailure)
{
	UCombatInventoryComponent* Inventory = nullptr;
	if (!ValidateInventoryTransactionRevisions(ExpectedEconomyRevision, ExpectedInventoryRevision, OutFailure, Inventory)) return false;
	UCombatItemSubsystem* Items = GetItems();
	UCombatItemInstance* Item = Items ? Items->FindMutable(ItemHandle) : nullptr;
	if (!Item || Item->Holder != Inventory->GetUnit() || Item->Revision != ExpectedItemRevision
		|| !CombatItems::IsSlot(Item->Slot) || !Inventory->Slots.IsValidIndex(Item->Slot)
		|| Inventory->Slots[Item->Slot] != ItemHandle)
	{
		OutFailure = CombatTags::Failure_Economy_Stale;
		return false;
	}
	if (!Item->Definition || !Item->Definition->bSellable)
	{
		OutFailure = CombatTags::Failure_Economy_NotSellable;
		return false;
	}
	if (Inventory->IsCasting(*Item))
	{
		OutFailure = CombatTags::Failure_Item_Busy;
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
	Refund = FMath::Max<int64>(0, FMath::Min(Refund, FrozenGoldCap - ReplicatedView.Gold));

	const FCombatSourceContext SoldSource = Item->MakeSource();
	const int64 PreviousGold = ReplicatedView.Gold;
	{
		TGuardValue<bool> EconomyGuard(bMutating, true);
		TGuardValue<bool> InventoryGuard(Inventory->bMutating, true);
		Inventory->RemoveItem(*Item);
		ReplicatedView.Gold += Refund;
	}
	AdvanceEconomyRevision();
	EmitEconomyEvent(CombatTags::Event_Combat_ItemSold, SoldSource, TEXT("SoldInventory"), PreviousGold);
	Inventory->ReconcileEffects();
	RefreshView(true, true);
	return true;
}

bool UCombatEconomyComponent::ToggleInventoryItemLock(const FCombatItemHandle ItemHandle,
	const int32 ExpectedItemRevision, const int32 ExpectedEconomyRevision,
	const int32 ExpectedInventoryRevision, bool& OutLocked, FGameplayTag& OutFailure)
{
	OutLocked = false;
	UCombatInventoryComponent* Inventory = nullptr;
	if (!ValidateInventoryTransactionRevisions(ExpectedEconomyRevision, ExpectedInventoryRevision, OutFailure, Inventory)) return false;
	UCombatItemInstance* Item = GetItems() ? GetItems()->FindMutable(ItemHandle) : nullptr;
	if (!Item || Item->Holder != Inventory->GetUnit() || Item->Revision != ExpectedItemRevision
		|| !CombatItems::IsSlot(Item->Slot) || !Inventory->Slots.IsValidIndex(Item->Slot)
		|| Inventory->Slots[Item->Slot] != ItemHandle)
	{
		OutFailure = CombatTags::Failure_Economy_Stale;
		return false;
	}
	{
		TGuardValue<bool> EconomyGuard(bMutating, true);
		TGuardValue<bool> InventoryGuard(Inventory->bMutating, true);
		Item->bLocked = !Item->bLocked;
		AdvanceRevision(Item->Revision);
		OutLocked = Item->bLocked;
		Inventory->NotifyChanged(Item, OutLocked ? TEXT("Locked") : TEXT("Unlocked"));
	}
	if (!OutLocked)
	{
		// 解锁后立即复用库存顶层事务的稳定合成；没有完整配方时只刷新投影，不改变库存内容。
		FCombatItemHandle TrackedHandle = ItemHandle;
		StabilizeInventoryCrafting(*Inventory, TrackedHandle);
		Inventory->ReconcileEffects();
	}
	else
	{
		RefreshView(false, true);
	}
	return true;
}

int32 UCombatEconomyComponent::FindInventoryItemRevision(const FCombatItemHandle Handle) const
{
	const UCombatInventoryComponent* Inventory = GetCommandedInventory();
	const UCombatItemInstance* Item = GetItems() ? GetItems()->FindItem(Handle) : nullptr;
	return Inventory && Item && Item->Holder == Inventory->GetUnit() && CombatItems::IsSlot(Item->Slot)
		&& Inventory->Slots.IsValidIndex(Item->Slot) && Inventory->Slots[Item->Slot] == Handle ? Item->Revision : 0;
}

void UCombatEconomyComponent::RefreshView(const bool bResourceChanged, const bool bInventoryChanged)
{
	ReplicatedView.InventoryItems.Reset();
	ReplicatedView.InventoryItems.SetNum(CombatItems::TotalSlots);
	ReplicatedView.InventoryRevision = 0;
	if (const UCombatInventoryComponent* Inventory = GetCommandedInventory())
	{
		Inventory->BuildViews(ReplicatedView.InventoryItems);
		ReplicatedView.InventoryRevision = Inventory->GetRevision();
	}
	if (ACombatPlayerController* Player = GetCombatPlayer()) Player->ForceNetUpdate();
	OnEconomyViewChanged.Broadcast();
	if (bResourceChanged) OnPlayerResourceViewChanged.Broadcast();
	if (bInventoryChanged) OnHeroInventoryViewChanged.Broadcast();
}

void UCombatEconomyComponent::OnRep_EconomyView()
{
	OnEconomyViewChanged.Broadcast();
	// Replication does not expose field-level dirty information; conservatively notify both projections.
	OnPlayerResourceViewChanged.Broadcast();
	OnHeroInventoryViewChanged.Broadcast();
}

void UCombatEconomyComponent::EmitEconomyEvent(const FGameplayTag EventType,
	const FCombatSourceContext& Source, const FName Action, const int64 PreviousGold) const
{
	UCombatEventSubsystem* Events = GetWorld() ? GetWorld()->GetSubsystem<UCombatEventSubsystem>() : nullptr;
	if (!Events) return;
	const ACombatPlayerController* Player = GetCombatPlayer();
	FCombatLogRecord Record;
	Record.Context = Events->CreateRootEvent();
	Record.EventType = EventType;
	Record.Source = Source;
	Record.ItemAction = Action;
	Record.GoldDelta = ReplicatedView.Gold - PreviousGold;
	Record.GoldBalance = ReplicatedView.Gold;
	// Gold and other player resources have no hero endpoint. Inventory transactions retain
	// the current hero endpoint for attribution, while their recipient remains the player.
	if (EventType != CombatTags::Event_Combat_GoldChanged
		&& Player && Player->GetCommandedUnit())
	{
		const ACombatUnitCharacter* Unit = Player->GetCommandedUnit();
		Record.SourceActorId = Unit->GetUniqueID();
		Record.TargetActorId = Unit->GetUniqueID();
		Record.UnitLifeGeneration = Unit->GetLifeGeneration();
	}
	Record.Diagnostic = FString::Printf(TEXT("Action=%s GoldDelta=%lld Gold=%lld EconomyRevision=%d InventoryRevision=%d"),
		*Action.ToString(), static_cast<long long>(Record.GoldDelta), static_cast<long long>(Record.GoldBalance),
		ReplicatedView.EconomyRevision, ReplicatedView.InventoryRevision);
	FCombatLogResourceChange Presentation;
	Presentation.OwningPlayerId = Player ? static_cast<int32>(Player->GetUniqueID()) : 0;
	Events->Emit(Record, Presentation);
}

void UCombatEconomyComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	bEnding = true;
	if (UCombatSchedulerSubsystem* Scheduler = GetWorld() ? GetWorld()->GetSubsystem<UCombatSchedulerSubsystem>() : nullptr)
	{
		Scheduler->CancelAllForOwner(this);
	}
	ShopData = nullptr;
	Super::EndPlay(EndPlayReason);
}
