#include "Combat/Economy/CombatEconomyTypes.h"

bool FCombatEconomyView::operator==(const FCombatEconomyView& Other) const
{
	return Gold == Other.Gold && GoldCap == Other.GoldCap
		&& PassiveGoldPerMinute == Other.PassiveGoldPerMinute
		&& EconomyRevision == Other.EconomyRevision
		&& InventoryRevision == Other.InventoryRevision
		&& ShopDefinitionId == Other.ShopDefinitionId
		&& InventoryItems == Other.InventoryItems;
}
