#include "Combat/Ability/CombatAbilitySystemGlobals.h"

#include "Combat/Ability/CombatGameplayEffectContext.h"

FGameplayEffectContext* UCombatAbilitySystemGlobals::AllocGameplayEffectContext() const
{
	return new FCombatGameplayEffectContext();
}
