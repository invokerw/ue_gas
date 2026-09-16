#include "Combat/Items/CombatItemData.h"
#include "Combat/Ability/CombatGameplayAbility.h"
#include "Combat/Core/CombatTags.h"
#include "Misc/DataValidation.h"

UCombatItemData::UCombatItemData()
{
	AuraTargeting.TargetTeamTag = CombatTags::TargetTeam_Friendly;
	AuraTargeting.bAllowSelf = true;
}

FPrimaryAssetType UCombatItemData::GetCombatPrimaryAssetType() const { return FPrimaryAssetType(TEXT("CombatItem")); }

bool UCombatItemData::ValidateRuntime(FString& OutError) const
{
	OutError.Reset();
	if (!GetPrimaryAssetId().IsValid() || MaxStack < 1 || MaxStack > 99 || InitialCharges < 0 || InitialCharges > 9999
		|| QuantityPerUse < 0 || QuantityPerUse > MaxStack || ChargesPerUse < 0 || ChargesPerUse > InitialCharges
		|| (InitialCharges > 0 && MaxStack != 1) || (QuantityPerUse > 0 && ChargesPerUse > 0)
		|| (bDestroyWhenChargesEmpty && (InitialCharges == 0 || ChargesPerUse == 0))
		|| PurchasePrice < 0 || (bPurchasable && PurchasePrice <= 0) || (!Recipe.IsEmpty() && bPurchasable)
		|| (bRecipeScroll && (!bPurchasable || !Recipe.IsEmpty()))
		|| !FMath::IsFinite(AuraRadius) || AuraRadius < 0.0f
		|| (Sharing != ECombatItemSharing::Public && Sharing != ECombatItemSharing::BoundUnit && Sharing != ECombatItemSharing::AlliedTeam))
	{
		OutError = TEXT("Invalid item identity, stack, charges or aura radius");
		return false;
	}
	for (const FCombatItemRecipeIngredient& Ingredient : Recipe)
	{
		if (Ingredient.Item.IsNull() || Ingredient.Quantity < 1 || Ingredient.Quantity > 99)
		{
			OutError = TEXT("Item recipe requires valid components with quantity from 1 to 99");
			return false;
		}
	}
	const UCombatGameplayAbility* Ability = ActiveAbility ? ActiveAbility->GetDefaultObject<UCombatGameplayAbility>() : nullptr;
	if (ActiveAbility && (!Ability || !Ability->GetAbilityData() || !Ability->GetAbilityData()->ValidateRuntime(OutError)
		|| Ability->GetAbilityData()->BehaviorTags.HasTagExact(CombatTags::Ability_Behavior_Passive)
		|| Ability->GetAbilityData()->BehaviorTags.HasTagExact(CombatTags::Ability_Behavior_AutoCast)))
	{
		OutError = TEXT("Item active must bind valid non-passive AbilityData on its class CDO");
		return false;
	}
	if (!ActiveAbility && (QuantityPerUse > 0 || ChargesPerUse > 0))
	{
		OutError = TEXT("Consumable requires an active ability");
		return false;
	}
	TSet<const UCombatModifierData*> Seen;
	if (Ability && Ability->GetAbilityData()->IntrinsicModifier)
	{
		const UCombatModifierData* Intrinsic = Ability->GetAbilityData()->IntrinsicModifier;
		if (!Intrinsic->GetPrimaryAssetId().IsValid() || Intrinsic->Duration != 0.0f || Intrinsic->DispelRule != ECombatModifierDispelRule::NotDispellable)
		{
			OutError = TEXT("Item intrinsic modifier must be valid, infinite and non-dispellable");
			return false;
		}
		Seen.Add(Intrinsic);
	}
	for (const FCombatItemPassive& Passive : Passives)
	{
		if (!Passive.Modifier || !Passive.Modifier->GetPrimaryAssetId().IsValid() || Seen.Contains(Passive.Modifier)
			|| Passive.Modifier->Duration != 0.0f || Passive.Modifier->DispelRule != ECombatModifierDispelRule::NotDispellable)
		{
			OutError = TEXT("Item passives require distinct, valid, infinite and non-dispellable modifiers");
			return false;
		}
		Seen.Add(Passive.Modifier);
	}
	if (AuraModifier && (!AuraModifier->GetPrimaryAssetId().IsValid() || AuraRadius <= 0.0f
		|| Seen.Contains(AuraModifier) || AuraModifier->DispelRule != ECombatModifierDispelRule::NotDispellable))
	{
		OutError = TEXT("Item aura requires a distinct, valid, non-dispellable modifier and positive radius");
		return false;
	}
	return true;
}

#if WITH_EDITOR
EDataValidationResult UCombatItemData::IsDataValid(FDataValidationContext& Context) const
{
	const EDataValidationResult Base = Super::IsDataValid(Context);
	FString Error;
	if (!ValidateRuntime(Error)) { Context.AddError(FText::FromString(Error)); return EDataValidationResult::Invalid; }
	return Base;
}
#endif
