#include "Combat/Items/CombatItemTypes.h"

bool FCombatItemView::operator==(const FCombatItemView& Other) const
{
	return Handle == Other.Handle && DefinitionId == Other.DefinitionId && AbilityHandle == Other.AbilityHandle
		&& Revision == Other.Revision && Quantity == Other.Quantity && Charges == Other.Charges
		&& bLocked == Other.bLocked
		&& CooldownCheckpoint == Other.CooldownCheckpoint && CooldownRemaining == Other.CooldownRemaining
		&& CooldownDuration == Other.CooldownDuration && CooldownRate == Other.CooldownRate
		&& EnabledAt == Other.EnabledAt && ManaCost == Other.ManaCost && FailureTag == Other.FailureTag;
}
