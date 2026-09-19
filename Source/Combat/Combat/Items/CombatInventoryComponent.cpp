#include "Combat/Items/CombatInventoryComponent.h"
#include "Combat/Items/CombatItemData.h"
#include "Combat/Items/CombatItemSubsystem.h"
#include "Combat/Items/CombatWorldItem.h"
#include "Combat/Economy/CombatEconomyComponent.h"
#include "Combat/Ability/CombatAbilitySystemComponent.h"
#include "Combat/Ability/CombatGameplayAbility.h"
#include "Combat/Aura/CombatAuraSubsystem.h"
#include "Combat/Core/CombatTags.h"
#include "Combat/Attributes/CombatAttributeSet.h"
#include "Combat/Combat/CombatEffectUtilities.h"
#include "Combat/Log/CombatEventSubsystem.h"
#include "Combat/Modifiers/CombatModifierComponent.h"
#include "Combat/Modifiers/CombatModifierRuntime.h"
#include "Combat/Scheduling/CombatSchedulerSubsystem.h"
#include "Combat/Targeting/CombatTargetingSubsystem.h"
#include "Combat/Targeting/CombatTeamSubsystem.h"
#include "Combat/Unit/CombatUnitCharacter.h"
#include "Combat/View/CombatUnitViewComponent.h"
#include "CombatPlayerController.h"
#include "Engine/World.h"

UCombatInventoryComponent::UCombatInventoryComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	Slots.SetNum(CombatItems::TotalSlots);
}

ACombatUnitCharacter* UCombatInventoryComponent::GetUnit() const { return Cast<ACombatUnitCharacter>(GetOwner()); }
UCombatItemSubsystem* UCombatInventoryComponent::GetItems() const { return GetWorld() ? GetWorld()->GetSubsystem<UCombatItemSubsystem>() : nullptr; }

int32 UCombatInventoryComponent::GetItemCount() const
{
	int32 Count = 0;
	for (const FCombatItemHandle Handle : Slots) if (Handle.IsValid()) ++Count;
	return Count;
}

bool UCombatInventoryComponent::GiveItem(UCombatItemData* Definition, int32 Quantity, FCombatItemHandle& OutHandle, FGameplayTag& Failure)
{
	OutHandle = {};
	Failure = {};
	ACombatUnitCharacter* Unit = GetUnit();
	UCombatItemSubsystem* Items = GetItems();
	if (!Unit || !Unit->HasAuthority() || !Items) { Failure = CombatTags::Failure_Authority; return false; }
	if (bEnding || bMutating) { Failure = CombatTags::Failure_Item_Busy; return false; }
	bool bAccepted = false;
	{
		TGuardValue<bool> Guard(bMutating, true);
		UCombatItemInstance* Item = Items->CreateItem(Definition, Quantity);
		if (!Item) { Failure = CombatTags::Failure_InvalidNumber; return false; }
		const FCombatItemHandle Created = Item->Handle;
		bAccepted = AcceptItem(*Item, OutHandle, Failure);
		if (!bAccepted) Items->DestroyItem(Created);
	}
	if (bAccepted)
	{
		if (ACombatPlayerController* Player = Cast<ACombatPlayerController>(Unit->GetCommandingPlayerController()))
		{
			if (UCombatEconomyComponent* Economy = Player->GetCombatEconomyComponent())
			{
				Economy->StabilizeInventoryCrafting(*this, OutHandle);
			}
		}
		ReconcileEffects();
	}
	return bAccepted;
}

bool UCombatInventoryComponent::CanMerge(const UCombatItemInstance& Into, const UCombatItemInstance& From) const
{
	const double Now = GetWorld()->GetTimeSeconds();
	return Into.Definition == From.Definition && Into.Definition->MaxStack > 1
		&& Into.Quantity > 0 && Into.Quantity + From.Quantity <= Into.Definition->MaxStack
		&& Into.bLocked == From.bLocked
		&& Into.Charges == From.Charges && !IsCasting(Into)
		&& Into.GetCooldownRemaining(Now) <= 0.0f && From.GetCooldownRemaining(Now) <= 0.0f
		&& Into.EnabledAt <= Now && From.EnabledAt <= Now && !From.bNeedsReequipDelay
		&& (!From.bBoundUnitAssigned || (Into.bBoundUnitAssigned && From.BoundUnit == Into.BoundUnit))
		&& (!From.BoundTeam.IsValid() || From.BoundTeam == Into.BoundTeam);
}

bool UCombatInventoryComponent::AcceptItem(UCombatItemInstance& Item, FCombatItemHandle& OutHandle, FGameplayTag& Failure)
{
	ACombatUnitCharacter* Unit = GetUnit();
	UCombatItemSubsystem* Items = GetItems();
	UCombatAbilitySystemComponent* Asc = Unit->GetCombatAbilitySystemComponent();
	if (Item.Holder.IsValid()) { Failure = CombatTags::Failure_Item_Stale; return false; }
	if (Item.Definition->Sharing == ECombatItemSharing::BoundUnit && Item.bBoundUnitAssigned && Item.BoundUnit != Unit)
	{
		Failure = CombatTags::Failure_Item_Bound; return false;
	}
	if (Item.Definition->Sharing == ECombatItemSharing::AlliedTeam && Item.BoundTeam.IsValid())
	{
		UCombatTeamSubsystem* Teams = GetWorld()->GetSubsystem<UCombatTeamSubsystem>();
		if (!Teams || Teams->GetRelation(Unit->GetCombatTeamId(), Item.BoundTeam) != ECombatTeamRelation::Friendly)
		{
			Failure = CombatTags::Failure_Item_Bound; return false;
		}
	}
	for (const FCombatItemHandle Handle : Slots)
	{
		UCombatItemInstance* Into = Items->FindMutable(Handle);
		if (!Into || !CanMerge(*Into, Item)) continue;
		const int32 Previous = Into->Quantity;
		Into->Quantity += Item.Quantity;
		Into->Revision = Into->Revision == MAX_int32 ? 1 : Into->Revision + 1;
		OutHandle = Into->Handle;
		ACombatWorldItem* Actor = Item.WorldActor.Get();
		Item.WorldActor.Reset();
		NotifyChanged(Into, TEXT("Merged"), Previous);
		Items->DestroyItem(Item.Handle);
		if (Actor) Actor->Destroy();
		return true;
	}
	const int32 Slot = Slots.IndexOfByPredicate([](const FCombatItemHandle& Value) { return !Value.IsValid(); });
	if (Slot == INDEX_NONE || (!CombatItems::IsEquipped(Slot) && !Item.Definition->bCanEnterBackpack))
	{
		Failure = CombatTags::Failure_Item_Full; return false;
	}
	Item.Holder = Unit;
	Item.Slot = Slot;
	Slots[Slot] = Item.Handle;
	if (Item.Definition->ActiveAbility && !Asc->GrantItemAbility(Item.Definition->ActiveAbility, Item.Handle, Item.AbilityHandle, Failure))
	{
		Slots[Slot] = {};
		Item.Slot = INDEX_NONE;
		Item.Holder.Reset();
		return false;
	}
	if (Item.Definition->Sharing == ECombatItemSharing::BoundUnit && !Item.bBoundUnitAssigned)
	{
		Item.BoundUnit = Unit;
		Item.bBoundUnitAssigned = true;
	}
	if (Item.Definition->Sharing == ECombatItemSharing::AlliedTeam && !Item.BoundTeam.IsValid()) Item.BoundTeam = Unit->GetCombatTeamId();
	ChangeSlot(Item, Slot);
	ACombatWorldItem* Actor = Item.WorldActor.Get();
	Item.WorldActor.Reset();
	OutHandle = Item.Handle;
	NotifyChanged(&Item, Actor ? TEXT("PickedUp") : TEXT("Granted"));
	if (Actor) Actor->Destroy();
	return true;
}

bool UCombatInventoryComponent::TryPickup(FCombatItemHandle Handle, int32 ExpectedRevision, FGameplayTag& Failure)
{
	Failure = {};
	ACombatUnitCharacter* Unit = GetUnit();
	UCombatItemSubsystem* Items = GetItems();
	if (!Unit || !Unit->HasAuthority() || !Items) { Failure = CombatTags::Failure_Authority; return false; }
	if (bMutating || bEnding) { Failure = CombatTags::Failure_Item_Busy; return false; }
	UCombatItemInstance* Item = Items->FindMutable(Handle);
	if (!Item || Item->Revision != ExpectedRevision || Item->Holder.IsValid() || !Item->WorldActor.IsValid())
	{
		Failure = CombatTags::Failure_Item_Stale; return false;
	}
	UCombatTargetingSubsystem* Targeting = GetWorld()->GetSubsystem<UCombatTargetingSubsystem>();
	const auto Validation = Targeting->ValidateItemInteraction(Unit, Item->WorldActor->GetActorLocation(), Item->WorldActor.Get(), true, false);
	if (!Validation.bValid) { Failure = Validation.FailureTag; return false; }
	bool bAccepted;
	FCombatItemHandle ResultHandle;
	{
		TGuardValue<bool> Guard(bMutating, true);
		bAccepted = AcceptItem(*Item, ResultHandle, Failure);
	}
	if (bAccepted)
	{
		if (ACombatPlayerController* Player = Cast<ACombatPlayerController>(Unit->GetCommandingPlayerController()))
		{
			if (UCombatEconomyComponent* Economy = Player->GetCombatEconomyComponent())
			{
				Economy->StabilizeInventoryCrafting(*this, ResultHandle);
			}
		}
		ReconcileEffects();
	}
	return bAccepted;
}

void UCombatInventoryComponent::ChangeSlot(UCombatItemInstance& Item, int32 Slot)
{
	const double Now = GetWorld()->GetTimeSeconds();
	Item.CooldownRemaining = Item.GetCooldownRemaining(Now);
	Item.CooldownCheckpoint = Now;
	Item.CooldownRate = CombatItems::IsEquipped(Slot) ? 1.0f : CombatItems::BackpackCooldownRate;
	Item.Slot = Slot;
	if (Slot >= CombatItems::EquippedSlots) Item.bNeedsReequipDelay = true;
	if (CombatItems::IsEquipped(Slot) && Item.bNeedsReequipDelay)
	{
		Item.EnabledAt = FMath::Max(Item.EnabledAt, Now + CombatItems::ReequipDelay);
		Item.bNeedsReequipDelay = false;
	}
	Item.Revision = Item.Revision == MAX_int32 ? 1 : Item.Revision + 1;
}

bool UCombatInventoryComponent::IsCasting(const UCombatItemInstance& Item) const
{
	const ACombatUnitCharacter* Unit = GetUnit();
	const FGameplayAbilitySpec* Spec = Unit ? Unit->GetCombatAbilitySystemComponent()->FindAbilitySpecFromHandle(Item.AbilityHandle) : nullptr;
	return Spec && Spec->IsActive();
}

bool UCombatInventoryComponent::TrySwap(int32 From, int32 To, int32 ExpectedRevision, FCombatItemHandle ExpectedFrom,
	FCombatItemHandle ExpectedTo, FGameplayTag& Failure)
{
	Failure = {};
	ACombatUnitCharacter* Unit = GetUnit();
	if (!Unit || !Unit->HasAuthority()) { Failure = CombatTags::Failure_Authority; return false; }
	if (Unit->GetLifeState() != ECombatLifeState::Alive) { Failure = CombatTags::Failure_Life_NotAlive; return false; }
	if (bMutating || bEnding) { Failure = CombatTags::Failure_Item_Busy; return false; }
	if (!CombatItems::IsSlot(From) || !CombatItems::IsSlot(To) || From == To || ExpectedRevision != InventoryRevision
		|| Slots[From] != ExpectedFrom || Slots[To] != ExpectedTo || !ExpectedFrom.IsValid())
	{
		Failure = CombatTags::Failure_Item_Stale; return false;
	}
	UCombatItemInstance* First = GetItems()->FindMutable(Slots[From]);
	UCombatItemInstance* Second = GetItems()->FindMutable(Slots[To]);
	if ((First && !CombatItems::IsEquipped(To) && !First->Definition->bCanEnterBackpack)
		|| (Second && !CombatItems::IsEquipped(From) && !Second->Definition->bCanEnterBackpack))
	{
		Failure = CombatTags::Failure_Item_NotEquipped; return false;
	}
	if (!First || (Second && IsCasting(*Second) && !CombatItems::IsEquipped(From))
		|| (IsCasting(*First) && !CombatItems::IsEquipped(To)))
	{
		Failure = CombatTags::Failure_Item_Busy; return false;
	}
	{
		TGuardValue<bool> Guard(bMutating, true);
		Slots.Swap(From, To);
		ChangeSlot(*First, To);
		if (Second) ChangeSlot(*Second, From);
		NotifyChanged(First, TEXT("Swapped"));
	}
	ReconcileEffects();
	return true;
}

bool UCombatInventoryComponent::TryDrop(FCombatItemHandle Handle, int32 ExpectedRevision, const FVector& Location, FGameplayTag& Failure)
{
	Failure = {};
	ACombatUnitCharacter* Unit = GetUnit();
	UCombatItemSubsystem* Items = GetItems();
	if (!Unit || !Unit->HasAuthority() || !Items) { Failure = CombatTags::Failure_Authority; return false; }
	if (bMutating || bEnding) { Failure = CombatTags::Failure_Item_Busy; return false; }
	UCombatItemInstance* Item = Items->FindMutable(Handle);
	if (!Item || Item->Holder != Unit || Item->Revision != ExpectedRevision || !CombatItems::IsSlot(Item->Slot))
	{
		Failure = CombatTags::Failure_Item_Stale; return false;
	}
	if (IsCasting(*Item)) { Failure = CombatTags::Failure_Item_Busy; return false; }
	if (!Item->Definition->bCanDrop) { Failure = CombatTags::Failure_Item_Bound; return false; }
	UCombatTargetingSubsystem* Targeting = GetWorld()->GetSubsystem<UCombatTargetingSubsystem>();
	const auto Validation = Targeting->ValidateItemInteraction(Unit, Location, nullptr, true, true);
	if (!Validation.bValid) { Failure = Validation.FailureTag; return false; }
	{
		TGuardValue<bool> Guard(bMutating, true);
		ACombatWorldItem* Actor = Items->CreateWorldActor(*Item, Validation.AuthoritativeLocation + FVector(0, 0, 24));
		if (!Actor) { Failure = CombatTags::Failure_ActionUnsupported; return false; }
		Slots[Item->Slot] = {};
		ChangeSlot(*Item, INDEX_NONE);
		// 撤销被动可能同步触发死亡或 EndPlay；回调看到的归属必须已经完整交给地面。
		Item->Holder.Reset();
		Item->WorldActor = Actor;
		RemoveEffects(*Item);
		if (Item->AbilityHandle.IsValid()) Unit->GetCombatAbilitySystemComponent()->RemoveCombatAbility(Item->AbilityHandle, Failure);
		Item->AbilityHandle = {};
		Actor->InitializeProjection(Handle, Item->Revision, Item->Definition, Item->Quantity);
		NotifyChanged(Item, TEXT("Dropped"));
	}
	ReconcileEffects();
	return true;
}

bool UCombatInventoryComponent::ValidateActive(FCombatItemHandle Handle, bool bCheckCost, FGameplayTag& Failure) const
{
	Failure = {};
	ACombatUnitCharacter* Unit = GetUnit();
	const UCombatItemInstance* Item = GetItems() ? GetItems()->FindItem(Handle) : nullptr;
	if (!Unit || !Unit->HasAuthority()) { Failure = CombatTags::Failure_Authority; return false; }
	if (Unit->GetLifeState() != ECombatLifeState::Alive) { Failure = CombatTags::Failure_Life_NotAlive; return false; }
	if (!Item || Item->Holder != Unit || !CombatItems::IsSlot(Item->Slot) || Slots[Item->Slot] != Handle)
	{
		Failure = CombatTags::Failure_Item_Stale; return false;
	}
	if (!CombatItems::IsEquipped(Item->Slot)) { Failure = CombatTags::Failure_Item_NotEquipped; return false; }
	if (Item->EnabledAt > GetWorld()->GetTimeSeconds() || Unit->GetCombatAbilitySystemComponent()->HasMatchingGameplayTag(CombatTags::State_Muted))
	{
		Failure = CombatTags::Failure_Item_Muted; return false;
	}
	if (Item->Definition->bMovementActive && Unit->IsMovementBlocked())
	{
		Failure = CombatTags::Failure_Ability_UnitStateBlocked; return false;
	}
	if (bCheckCost && (Item->Quantity <= 0 || Item->Quantity < Item->Definition->QuantityPerUse || Item->Charges < Item->Definition->ChargesPerUse))
	{
		Failure = CombatTags::Failure_Item_Empty; return false;
	}
	return true;
}

void UCombatInventoryComponent::CommitConsumption(FCombatItemHandle Handle)
{
	UCombatItemInstance* Item = GetItems() ? GetItems()->FindMutable(Handle) : nullptr;
	FGameplayTag Failure;
	if (!Item || !ValidateActive(Handle, true, Failure)) return;
	const int32 Previous = Item->Quantity;
	Item->Quantity -= Item->Definition->QuantityPerUse;
	Item->Charges -= Item->Definition->ChargesPerUse;
	if (Item->Definition->bDestroyWhenChargesEmpty && Item->Charges == 0) Item->Quantity = 0;
	Item->Revision = Item->Revision == MAX_int32 ? 1 : Item->Revision + 1;
	NotifyChanged(Item, TEXT("Consumed"), Previous);
}

void UCombatInventoryComponent::CommitCooldown(FCombatItemHandle Handle, float Duration)
{
	UCombatItemInstance* Item = GetItems() ? GetItems()->FindMutable(Handle) : nullptr;
	if (!GetUnit() || !GetUnit()->HasAuthority() || !Item || Item->Holder != GetUnit() || !FMath::IsFinite(Duration) || Duration < 0.0f) return;
	const double Now = GetWorld()->GetTimeSeconds();
	const FName Group = Item->Definition->SharedCooldownGroup;
	for (const FCombatItemHandle Candidate : Slots)
	{
		UCombatItemInstance* Other = GetItems()->FindMutable(Candidate);
		if (!Other || (Other != Item && (Group.IsNone() || Other->Definition->SharedCooldownGroup != Group))) continue;
		Other->CooldownRemaining = FMath::Max(Other->GetCooldownRemaining(Now), Duration);
		Other->CooldownDuration = Other->CooldownRemaining;
		Other->CooldownCheckpoint = Now;
		Other->Revision = Other->Revision == MAX_int32 ? 1 : Other->Revision + 1;
	}
	NotifyChanged(Item, TEXT("Cooldown"));
}

bool UCombatInventoryComponent::CommitActiveStage(FCombatItemHandle Handle, float ManaCost, float Cooldown, bool bConsume,
	bool bStartCooldown, bool& bCostCommitted, bool& bCooldownCommitted, FGameplayTag& Failure)
{
	if (bEnding || bMutating) { Failure = CombatTags::Failure_Item_Busy; return false; }
	if (!ValidateActive(Handle, bConsume, Failure)) return false;
	if (!bConsume && !bStartCooldown) return true;
	ACombatUnitCharacter* Unit = GetUnit();
	UCombatAbilitySystemComponent* Asc = Unit->GetCombatAbilitySystemComponent();
	if (!FMath::IsFinite(ManaCost) || ManaCost < 0 || !FMath::IsFinite(Cooldown) || Cooldown < 0
		|| (bConsume && Asc->GetNumericAttribute(UCombatAttributeSet::GetManaAttribute()) + KINDA_SMALL_NUMBER < ManaCost))
	{ Failure = CombatTags::Failure_Ability_Cost; return false; }
	TGuardValue<bool> Guard(bMutating, true);
	UCombatItemInstance* Item = GetItems()->FindMutable(Handle);
	const int32 PreviousQuantity = Item->Quantity, PreviousCharges = Item->Charges, PreviousRevision = Item->Revision;
	const bool PreviousCostCommitted = bCostCommitted, PreviousCooldownCommitted = bCooldownCommitted;
	struct FCooldownSnapshot
	{
		UCombatItemInstance* Instance;
		float Remaining, Duration;
		double Checkpoint;
		int32 Revision;
	};
	TArray<FCooldownSnapshot> PreviousCooldowns;
	const double Now = GetWorld()->GetTimeSeconds();
	if (bStartCooldown)
	{
		const FName Group = Item->Definition->SharedCooldownGroup;
		for (const auto Candidate : Slots)
		{
			UCombatItemInstance* Other = GetItems()->FindMutable(Candidate);
			if (!Other || (Other != Item && (Group.IsNone() || Other->Definition->SharedCooldownGroup != Group))) continue;
			PreviousCooldowns.Add({Other, Other->CooldownRemaining, Other->CooldownDuration, Other->CooldownCheckpoint, Other->Revision});
			Other->CooldownRemaining = FMath::Max(Other->GetCooldownRemaining(Now), Cooldown);
			Other->CooldownDuration = Other->CooldownRemaining;
			Other->CooldownCheckpoint = Now;
			Other->Revision = Other->Revision == MAX_int32 ? 1 : Other->Revision + 1;
		}
		bCooldownCommitted = true;
	}
	if (bConsume)
	{
		Item->Quantity -= Item->Definition->QuantityPerUse;
		Item->Charges -= Item->Definition->ChargesPerUse;
		if (Item->Definition->bDestroyWhenChargesEmpty && Item->Charges == 0) Item->Quantity = 0;
		Item->Revision = Item->Revision == MAX_int32 ? 1 : Item->Revision + 1;
		bCostCommitted = true;
	}
	// 法力 GE 会同步广播属性变化。数量、共享冷却和一次性标记必须在广播前提交。
	if (bConsume && ManaCost > 0 && !CombatEffectUtilities::ApplyAttributeAdditive(this, *Asc, UCombatAttributeSet::GetManaAttribute(), -ManaCost))
	{
		for (const auto& Before : PreviousCooldowns)
		{
			Before.Instance->CooldownRemaining = Before.Remaining;
			Before.Instance->CooldownDuration = Before.Duration;
			Before.Instance->CooldownCheckpoint = Before.Checkpoint;
			Before.Instance->Revision = Before.Revision;
		}
		Item->Quantity = PreviousQuantity; Item->Charges = PreviousCharges; Item->Revision = PreviousRevision;
		bCostCommitted = PreviousCostCommitted; bCooldownCommitted = PreviousCooldownCommitted;
		Failure = CombatTags::Failure_Ability_CommitFailed; return false;
	}
	if (bEnding) { Failure = CombatTags::Failure_Item_Stale; return false; }
	if (!ValidateActive(Handle, false, Failure)) return false;
	NotifyChanged(Item, bConsume ? TEXT("Consumed") : TEXT("Cooldown"), PreviousQuantity);
	if (bEnding) { Failure = CombatTags::Failure_Item_Stale; return false; }
	if (!ValidateActive(Handle, false, Failure)) return false;
	if (Asc->IsCombatAbilityStateBlocked(Item->AbilityHandle)) { Failure = CombatTags::Failure_Ability_UnitStateBlocked; return false; }
	return true;
}

void UCombatInventoryComponent::NotifyAbilityEnded(FCombatItemHandle Handle)
{
	const UCombatItemInstance* Item = GetItems() ? GetItems()->FindItem(Handle) : nullptr;
	if (!Item || Item->Holder != GetUnit() || Item->Quantity > 0 || bEnding) return;
	if (UCombatSchedulerSubsystem* Scheduler = GetWorld()->GetSubsystem<UCombatSchedulerSubsystem>())
	{
		Scheduler->ScheduleOnce(this, 0.0, 0, FCombatScheduledDelegate::CreateWeakLambda(this,
			[this, Handle](const FCombatScheduledTickContext&)
			{
				UCombatItemInstance* Current = GetItems() ? GetItems()->FindMutable(Handle) : nullptr;
				if (Current && Current->Holder == GetUnit() && Current->Quantity == 0 && !IsCasting(*Current))
				{
					RemoveItem(*Current);
					ReconcileEffects();
				}
			}));
	}
}

void UCombatInventoryComponent::RemoveEffects(UCombatItemInstance& Item)
{
	const TMap<int32, FCombatModifierHandle> Handles = MoveTemp(Item.PassiveHandles);
	const FCombatAuraHandle Aura = Item.AuraHandle;
	Item.AuraHandle = {};
	if (ACombatUnitCharacter* Unit = GetUnit())
	{
		for (const auto& Pair : Handles) Unit->GetCombatModifierComponent()->RemoveModifier(Pair.Value);
	}
	if (Aura.IsValid()) if (UCombatAuraSubsystem* Auras = GetWorld()->GetSubsystem<UCombatAuraSubsystem>()) Auras->CancelAura(Aura);
}

void UCombatInventoryComponent::ReconcileEffects()
{
	ACombatUnitCharacter* Unit = GetUnit();
	if (!Unit || !Unit->HasAuthority() || bEnding || bMutating || !GetItems()) return;
	{
		TGuardValue<bool> Guard(bMutating, true);
		TSet<FName> ClaimedGroups;
		const TArray<FCombatItemHandle> Snapshot = Slots;
		for (const FCombatItemHandle Handle : Snapshot)
		{
			if (bEnding) break;
			UCombatItemInstance* Item = GetItems()->FindMutable(Handle);
			if (!Item) continue;
			if (!CombatItems::IsEquipped(Item->Slot) || Item->EnabledAt > GetWorld()->GetTimeSeconds()
				|| Unit->GetLifeState() != ECombatLifeState::Alive || Item->Quantity <= 0)
			{
				RemoveEffects(*Item); continue;
			}
			TArray<int32> Desired;
			for (int32 Index = 0; Index < Item->Definition->Passives.Num(); ++Index)
			{
				const FCombatItemPassive& Passive = Item->Definition->Passives[Index];
				if (!Passive.UniqueGroup.IsNone() && ClaimedGroups.Contains(Passive.UniqueGroup)) continue;
				if (!Passive.UniqueGroup.IsNone()) ClaimedGroups.Add(Passive.UniqueGroup);
				Desired.Add(Index);
			}
			const UCombatAbilityData* AbilityData = Unit->GetCombatAbilitySystemComponent()->GetCombatAbilityData(Item->AbilityHandle);
			if (AbilityData && AbilityData->IntrinsicModifier) Desired.Add(INDEX_NONE);
			TArray<int32> RemoveKeys;
			for (const auto& Pair : Item->PassiveHandles) if (!Desired.Contains(Pair.Key)) RemoveKeys.Add(Pair.Key);
			for (const int32 Key : RemoveKeys)
			{
				const FCombatModifierHandle Old = Item->PassiveHandles.FindAndRemoveChecked(Key);
				Unit->GetCombatModifierComponent()->RemoveModifier(Old);
			}
			for (const int32 Index : Desired)
			{
				const FCombatModifierHandle* Existing = Item->PassiveHandles.Find(Index);
				if (Existing && Unit->GetCombatModifierComponent()->FindRuntime(*Existing)) continue;
				if (Unit->GetLifeState() != ECombatLifeState::Alive) break;
				FCombatModifierApplyRequest Request;
				Request.Source = Unit;
				Request.ModifierData = Index == INDEX_NONE ? AbilityData->IntrinsicModifier.Get() : Item->Definition->Passives[Index].Modifier.Get();
				Request.ItemOwnerHandle = Handle;
				Request.SourceContext = Item->MakeSource();
				const auto Applied = Unit->GetCombatModifierComponent()->ApplyModifier(Request);
				if (bEnding || Item->Holder != Unit || Unit->GetLifeState() != ECombatLifeState::Alive)
				{
					if (Applied.Handle.IsValid()) Unit->GetCombatModifierComponent()->RemoveModifier(Applied.Handle);
					break;
				}
				if (Applied.bSuccess && Applied.Handle.IsValid()) Item->PassiveHandles.Add(Index, Applied.Handle);
			}
			if (bEnding || Item->Holder != Unit) continue;
			UCombatAuraSubsystem* Auras = GetWorld()->GetSubsystem<UCombatAuraSubsystem>();
			if (Item->Definition->AuraModifier && Auras && !Auras->IsAuraActive(Item->AuraHandle) && Unit->GetLifeState() == ECombatLifeState::Alive)
			{
				FCombatAuraSpec Aura;
				Aura.Owner = Unit;
				Aura.Radius = Item->Definition->AuraRadius;
				Aura.TargetingRules = Item->Definition->AuraTargeting;
				Aura.ChildModifierData = Item->Definition->AuraModifier;
				Aura.ChildDurationOverride = 0.0f;
				Aura.SourceContext = Item->MakeSource();
				const FCombatAuraHandle CreatedAura = Auras->StartAura(Aura).Handle;
				if (bEnding || Item->Holder != Unit || Unit->GetLifeState() != ECombatLifeState::Alive) Auras->CancelAura(CreatedAura);
				else Item->AuraHandle = CreatedAura;
			}
			if (Unit->GetLifeState() != ECombatLifeState::Alive) RemoveEffects(*Item);
		}
	}
	ScheduleReconcile();
	if (Unit->GetCombatUnitViewComponent()) Unit->GetCombatUnitViewComponent()->RefreshHUDOwnerView();
}

void UCombatInventoryComponent::ScheduleReconcile()
{
	UCombatSchedulerSubsystem* Scheduler = GetWorld()->GetSubsystem<UCombatSchedulerSubsystem>();
	if (!Scheduler) return;
	Scheduler->Cancel(ReconcileSchedule);
	ReconcileSchedule = {};
	if (bEnding || !GetUnit() || GetUnit()->GetLifeState() != ECombatLifeState::Alive) return;
	const double Now = GetWorld()->GetTimeSeconds();
	double Next = TNumericLimits<double>::Max();
	for (const FCombatItemHandle Handle : Slots)
	{
		const UCombatItemInstance* Item = GetItems()->FindItem(Handle);
		if (Item && CombatItems::IsEquipped(Item->Slot) && Item->EnabledAt > Now) Next = FMath::Min(Next, Item->EnabledAt);
	}
	if (Next == TNumericLimits<double>::Max()) return;
	const uint32 Life = GetUnit()->GetLifeGeneration();
	ReconcileSchedule = Scheduler->ScheduleOnce(this, Next - Now, 0, FCombatScheduledDelegate::CreateWeakLambda(this,
		[this, Life](const FCombatScheduledTickContext&)
		{
			if (GetUnit() && GetUnit()->GetLifeGeneration() == Life) ReconcileEffects();
		}));
}

void UCombatInventoryComponent::RemoveItem(UCombatItemInstance& Item)
{
	if (CombatItems::IsSlot(Item.Slot) && Slots[Item.Slot] == Item.Handle) Slots[Item.Slot] = {};
	Item.Slot = INDEX_NONE;
	RemoveEffects(Item);
	FGameplayTag Failure;
	if (Item.AbilityHandle.IsValid() && GetUnit()) GetUnit()->GetCombatAbilitySystemComponent()->RemoveCombatAbility(Item.AbilityHandle, Failure);
	Item.AbilityHandle = {};
	NotifyChanged(&Item, TEXT("Removed"));
	GetItems()->DestroyItem(Item.Handle);
}

void UCombatInventoryComponent::HandleOwnerDeath()
{
	if (!GetUnit() || !GetUnit()->HasAuthority() || !GetItems()) return;
	if (UCombatSchedulerSubsystem* Scheduler = GetWorld()->GetSubsystem<UCombatSchedulerSubsystem>()) Scheduler->Cancel(ReconcileSchedule);
	const TArray<FCombatItemHandle> Snapshot = Slots;
	for (const FCombatItemHandle Handle : Snapshot)
	{
		UCombatItemInstance* Item = GetItems()->FindMutable(Handle);
		if (!Item) continue;
		RemoveEffects(*Item);
		if (bEnding || Item->Holder != GetUnit()) continue;
		if (Item->Quantity == 0) { NotifyAbilityEnded(Handle); continue; }
		if (!Item->Definition->bDropOnDeath) continue;
		ACombatWorldItem* Actor = GetItems()->CreateWorldActor(*Item, GetUnit()->GetNavAgentLocation() + FVector(0, 0, 24));
		if (!Actor) continue;
		Slots[Item->Slot] = {};
		ChangeSlot(*Item, INDEX_NONE);
		FGameplayTag Failure;
		if (Item->AbilityHandle.IsValid()) GetUnit()->GetCombatAbilitySystemComponent()->RemoveCombatAbility(Item->AbilityHandle, Failure);
		Item->AbilityHandle = {};
		Item->Holder.Reset();
		Item->WorldActor = Actor;
		Actor->InitializeProjection(Item->Handle, Item->Revision, Item->Definition, Item->Quantity);
		NotifyChanged(Item, TEXT("DeathDrop"));
	}
}

void UCombatInventoryComponent::ClearInventory()
{
	if (bEnding) return;
	bEnding = true;
	if (UCombatSchedulerSubsystem* Scheduler = GetWorld() ? GetWorld()->GetSubsystem<UCombatSchedulerSubsystem>() : nullptr) Scheduler->CancelAllForOwner(this);
	if (GetUnit() && GetUnit()->HasAuthority() && GetItems())
	{
		const TArray<FCombatItemHandle> Snapshot = Slots;
		for (const FCombatItemHandle Handle : Snapshot) if (UCombatItemInstance* Item = GetItems()->FindMutable(Handle)) RemoveItem(*Item);
	}
	for (FCombatItemHandle& Handle : Slots) Handle = {};
}

void UCombatInventoryComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	ClearInventory();
	Super::EndPlay(Reason);
}

void UCombatInventoryComponent::NotifyChanged(UCombatItemInstance* Item, const TCHAR* Action, int32 PreviousQuantity)
{
	InventoryRevision = InventoryRevision == MAX_int32 ? 1 : InventoryRevision + 1;
	ACombatUnitCharacter* Unit = GetUnit();
	if (!Unit) return;
	if (UCombatEventSubsystem* Events = GetWorld()->GetSubsystem<UCombatEventSubsystem>(); Events && Item)
	{
		FCombatLogRecord Record;
		Record.Context = Events->CreateRootEvent();
		Record.EventType = CombatTags::Event_Combat_ItemChanged;
		Record.Source = Item->MakeSource();
		Record.ItemAction = Action;
		Record.ItemQuantity = Item->Quantity;
		Record.ItemCharges = Item->Charges;
		Record.SourceActorId = Unit->GetUniqueID();
		Record.TargetActorId = Unit->GetUniqueID();
		Record.UnitLifeGeneration = Unit->GetLifeGeneration();
		Record.RequestedAmount = PreviousQuantity;
		Record.AppliedAmount = Item->Quantity;
		Record.Diagnostic = FString::Printf(TEXT("%s Item=%s Definition=%s Slot=%d Revision=%d Quantity=%d Charges=%d"),
			Action, *Item->Handle.ToString(), *Item->Definition->GetPrimaryAssetId().ToString(), Item->Slot, Item->Revision, Item->Quantity, Item->Charges);
		Events->Emit(Record);
	}
	if (Unit->GetCombatUnitViewComponent()) Unit->GetCombatUnitViewComponent()->RefreshHUDOwnerView();
	Unit->ForceNetUpdate();
	if (ACombatPlayerController* Player = Cast<ACombatPlayerController>(Unit->GetCommandingPlayerController()))
	{
		if (UCombatEconomyComponent* Economy = Player->GetCombatEconomyComponent();
			Economy && Economy->bInitialized && !Economy->bMutating && !Economy->bEnding)
		{
			Economy->RefreshView();
		}
	}
}

void UCombatInventoryComponent::BuildViews(TArray<FCombatItemView>& Out) const
{
	Out.Reset();
	Out.SetNum(CombatItems::TotalSlots);
	if (!GetItems() || !GetUnit() || !GetUnit()->HasAuthority()) return;
	for (int32 Slot = 0; Slot < Slots.Num(); ++Slot)
	{
		const UCombatItemInstance* Item = GetItems()->FindItem(Slots[Slot]);
		if (!Item) continue;
		FCombatItemView& View = Out[Slot];
		View.Handle = Item->Handle;
		View.DefinitionId = Item->Definition->GetPrimaryAssetId();
		View.AbilityHandle = Item->AbilityHandle;
		View.Revision = Item->Revision;
		View.Quantity = Item->Quantity;
		View.Charges = Item->Charges;
		View.bLocked = Item->bLocked;
		View.CooldownCheckpoint = Item->CooldownCheckpoint;
		View.CooldownRemaining = Item->CooldownRemaining;
		View.CooldownDuration = Item->CooldownDuration;
		View.CooldownRate = Item->CooldownRate;
		View.EnabledAt = Item->EnabledAt;
		const UCombatAbilityData* Data = GetUnit()->GetCombatAbilitySystemComponent()->GetCombatAbilityData(Item->AbilityHandle);
		View.ManaCost = Data ? Data->GetSpecialValue(TEXT("mana_cost"), 1) : 0.0f;
		ValidateActive(Item->Handle, true, View.FailureTag);
	}
}
