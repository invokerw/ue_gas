#include "Combat/Modifiers/CombatModifierRuntime.h"

#include "Combat/Data/CombatDefinitionData.h"
#include "Combat/Modifiers/CombatModifierComponent.h"

void UCombatModifierRuntime::OnCreated_Implementation() {}
void UCombatModifierRuntime::OnRefreshed_Implementation() {}
void UCombatModifierRuntime::OnDestroyed_Implementation() {}
void UCombatModifierRuntime::OnThink_Implementation(const FCombatScheduledTickContext& TickContext) { (void)TickContext; }
void UCombatModifierRuntime::OnPreDealDamage_Implementation(FCombatDamageEvent& Event) { (void)Event; }
void UCombatModifierRuntime::OnPreTakeDamage_Implementation(FCombatDamageEvent& Event) { (void)Event; }
void UCombatModifierRuntime::OnDamageBlock_Implementation(FCombatDamageEvent& Event) { (void)Event; }
void UCombatModifierRuntime::OnPostDealDamage_Implementation(const FCombatDamageEvent& Event) { (void)Event; }
void UCombatModifierRuntime::OnPostTakeDamage_Implementation(const FCombatDamageEvent& Event) { (void)Event; }
void UCombatModifierRuntime::OnPreDealHeal_Implementation(FCombatHealEvent& Event) { (void)Event; }
void UCombatModifierRuntime::OnPreTakeHeal_Implementation(FCombatHealEvent& Event) { (void)Event; }
void UCombatModifierRuntime::OnPostDealHeal_Implementation(const FCombatHealEvent& Event) { (void)Event; }
void UCombatModifierRuntime::OnPostTakeHeal_Implementation(const FCombatHealEvent& Event) { (void)Event; }
void UCombatModifierRuntime::OnAbilityExecuted_Implementation(
	const FPrimaryAssetId& AbilityDefinitionId,
	const FCombatEventContext& Context)
{
	(void)AbilityDefinitionId;
	(void)Context;
}

bool UCombatModifierRuntime::RequestRemoveSelf()
{
	return OwningComponent && bActive ? OwningComponent->RemoveModifier(Handle) : false;
}

float UCombatModifierRuntime::GetRuntimeParameter(const FName Key, const float DefaultValue) const
{
	if (!ModifierData)
	{
		return DefaultValue;
	}
	if (const float* Value = ModifierData->RuntimeParameters.Find(Key))
	{
		return *Value;
	}
	return DefaultValue;
}
