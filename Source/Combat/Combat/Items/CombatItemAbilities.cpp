#include "Combat/Ability/CombatAbilitySystemComponent.h"
#include "Combat/Ability/CombatGameplayAbility.h"
#include "Combat/Core/CombatTags.h"
#include "Combat/Items/CombatInventoryComponent.h"
#include "Combat/Items/CombatItemData.h"
#include "Combat/Items/CombatItemSubsystem.h"
#include "Combat/Unit/CombatUnitCharacter.h"

bool UCombatAbilitySystemComponent::GrantItemAbility(TSubclassOf<UCombatGameplayAbility> AbilityClass, FCombatItemHandle Item,
	FGameplayAbilitySpecHandle& OutHandle, FGameplayTag& OutFailureTag)
{
	OutHandle = {};
	OutFailureTag = {};
	ACombatUnitCharacter* Unit = GetCombatAvatar();
	const UCombatItemSubsystem* Items = GetWorld() ? GetWorld()->GetSubsystem<UCombatItemSubsystem>() : nullptr;
	const UCombatItemInstance* Instance = Items ? Items->FindItem(Item) : nullptr;
	const UCombatGameplayAbility* Cdo = AbilityClass ? AbilityClass->GetDefaultObject<UCombatGameplayAbility>() : nullptr;
	FString Error;
	if (!Unit || !Unit->HasAuthority() || !Instance || Instance->GetHolder() != Unit
		|| Instance->GetAbilityHandle().IsValid() || !Cdo || !Cdo->GetAbilityData() || !Cdo->GetAbilityData()->ValidateRuntime(Error))
	{
		OutFailureTag = CombatTags::Failure_Item_Stale;
		return false;
	}
	FGameplayAbilitySpec Spec(AbilityClass, 1);
	Spec.GetDynamicSpecSourceTags().AddTag(CombatTags::Ability_Source_Item);
	// 在 GiveAbility 的 OnGive 回调前登记来源，防止固有被动走英雄技能路径重复施加。
	ItemAbilityOwners.Add(Spec.Handle, Item);
	OutHandle = GiveAbility(Spec);
	if (!OutHandle.IsValid()) { ItemAbilityOwners.Remove(Spec.Handle); OutFailureTag = CombatTags::Failure_ActionUnsupported; return false; }
	SetInitialAutoCastState(OutHandle, false);
	EmitAbilitySpecLog(OutHandle, CombatTags::Event_Combat_AbilityGranted, TEXT("Granted from item instance"));
	return true;
}

bool UCombatAbilitySystemComponent::IsItemAbility(FGameplayAbilitySpecHandle Handle) const
{
	const FGameplayAbilitySpec* Spec = FindAbilitySpecFromHandle(Handle);
	return ItemAbilityOwners.Contains(Handle) || (Spec && Spec->GetDynamicSpecSourceTags().HasTagExact(CombatTags::Ability_Source_Item));
}

FCombatItemHandle UCombatAbilitySystemComponent::GetAbilityItem(FGameplayAbilitySpecHandle Handle) const
{
	const FCombatItemHandle* Item = ItemAbilityOwners.Find(Handle);
	return Item ? *Item : FCombatItemHandle();
}

FCombatSourceContext UCombatAbilitySystemComponent::MakeAbilitySource(FGameplayAbilitySpecHandle Handle) const
{
	FCombatSourceContext Source;
	Source.DirectSourceType = ECombatDirectSourceType::Ability;
	if (const UCombatItemSubsystem* Items = GetWorld() ? GetWorld()->GetSubsystem<UCombatItemSubsystem>() : nullptr)
		if (const UCombatItemInstance* Item = Items->FindItem(GetAbilityItem(Handle))) Source = Item->MakeSource();
	if (const UCombatAbilityData* Data = GetCombatAbilityData(Handle)) Source.AbilityDefinitionId = Data->GetPrimaryAssetId();
	return Source;
}

bool UCombatAbilitySystemComponent::IsCombatAbilityStateBlocked(FGameplayAbilitySpecHandle Handle) const
{
	const ACombatUnitCharacter* Unit = GetCombatAvatar();
	if (!Unit || Unit->GetLifeState() != ECombatLifeState::Alive || HasMatchingGameplayTag(CombatTags::State_Stunned)
		|| HasMatchingGameplayTag(CombatTags::State_Hexed) || HasMatchingGameplayTag(CombatTags::State_Frozen)
		|| HasMatchingGameplayTag(CombatTags::State_OutOfGame)) return true;
	if (IsItemAbility(Handle)) return HasMatchingGameplayTag(CombatTags::State_Muted);
	const UCombatAbilityData* Data = GetCombatAbilityData(Handle);
	return HasMatchingGameplayTag(CombatTags::State_Silenced) && (!Data || !Data->BehaviorTags.HasTagExact(CombatTags::Ability_Behavior_IgnoreSilence));
}
