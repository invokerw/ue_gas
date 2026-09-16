#include "Combat/Economy/CombatEconomyTypes.h"

bool FCombatEconomyView::operator==(const FCombatEconomyView& Other) const
{
	return Gold == Other.Gold && GoldCap == Other.GoldCap
		&& PassiveGoldPerMinute == Other.PassiveGoldPerMinute
		&& EconomyRevision == Other.EconomyRevision && StashRevision == Other.StashRevision
		&& ShopDefinitionId == Other.ShopDefinitionId && StashItems == Other.StashItems;
}
