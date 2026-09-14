#include "Combat/Order/CombatOrderComponent.h"
#include "Combat/Ability/CombatAbilitySystemComponent.h"
#include "Combat/Core/CombatTags.h"
#include "Combat/Items/CombatInventoryComponent.h"
#include "Combat/Items/CombatItemData.h"
#include "Combat/Items/CombatItemSubsystem.h"
#include "Combat/Items/CombatWorldItem.h"
#include "Combat/Targeting/CombatTargetingSubsystem.h"
#include "Combat/Unit/CombatUnitCharacter.h"

FCombatOperationResult UCombatOrderComponent::ValidateItemOrder(const FCombatOrderRequest& Request) const
{
	const ACombatUnitCharacter* Unit = GetOwnerUnit();
	const UCombatItemSubsystem* Items = GetWorld() ? GetWorld()->GetSubsystem<UCombatItemSubsystem>() : nullptr;
	const UCombatItemInstance* Item = Items ? Items->FindItem(Request.ItemHandle) : nullptr;
	if (!Unit || !Item || Request.ItemRevision <= 0 || Item->GetRevision() != Request.ItemRevision)
		return FCombatOperationResult::Failure(CombatTags::Failure_Item_Stale);
	if (Request.Type == ECombatOrderType::PickupItem)
	{
		if (Request.TargetUnit || Request.AbilitySpecHandle.IsValid() || Request.bHasTargetLocation
			|| Item->GetHolder() || !Item->GetWorldActor()) return FCombatOperationResult::Failure(CombatTags::Failure_Item_Stale);
	}
	else
	{
		if (Item->GetHolder() != Unit || Unit->GetCombatInventoryComponent()->GetItemAt(Item->GetSlot()) != Request.ItemHandle)
			return FCombatOperationResult::Failure(CombatTags::Failure_Item_Stale);
		if (Request.Type == ECombatOrderType::DropItem)
		{
			if (!Item->GetDefinition()->bCanDrop) return FCombatOperationResult::Failure(CombatTags::Failure_Item_Bound);
			const FGameplayAbilitySpec* Spec = Unit->GetCombatAbilitySystemComponent()->FindAbilitySpecFromHandle(Item->GetAbilityHandle());
			if (Spec && Spec->IsActive()) return FCombatOperationResult::Failure(CombatTags::Failure_Item_Busy);
			if (Request.TargetUnit || Request.AbilitySpecHandle.IsValid() || !Request.bHasTargetLocation || Request.TargetLocation.ContainsNaN())
				return FCombatOperationResult::Failure(CombatTags::Order_Failure_InvalidRequest);
			const auto Ground = GetWorld()->GetSubsystem<UCombatTargetingSubsystem>()->ValidateItemInteraction(
				const_cast<ACombatUnitCharacter*>(Unit), Request.TargetLocation, nullptr, false, true);
			if (!Ground.bValid) return FCombatOperationResult::Failure(Ground.FailureTag, Ground.Diagnostic);
		}
		else if (Request.Type == ECombatOrderType::SwapItems)
		{
			if (Request.TargetUnit || Request.AbilitySpecHandle.IsValid() || Request.bHasTargetLocation
				|| !CombatItems::IsSlot(Request.FromItemSlot) || !CombatItems::IsSlot(Request.ToItemSlot)
				|| Request.FromItemSlot == Request.ToItemSlot || Request.InventoryRevision <= 0)
				return FCombatOperationResult::Failure(CombatTags::Order_Failure_InvalidRequest);
		}
		else if (Unit->GetCombatAbilitySystemComponent()->GetAbilityItem(Request.AbilitySpecHandle) != Request.ItemHandle)
			return FCombatOperationResult::Failure(CombatTags::Failure_Item_Stale);
	}
	if (Request.Type != ECombatOrderType::SwapItems && (Request.OtherItemHandle.IsValid() || Request.FromItemSlot != INDEX_NONE
		|| Request.ToItemSlot != INDEX_NONE || Request.InventoryRevision != 0))
		return FCombatOperationResult::Failure(CombatTags::Order_Failure_InvalidRequest);
	return FCombatOperationResult::Success();
}
