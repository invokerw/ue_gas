#include "Combat/Economy/CombatEconomyTypes.h"

bool FCombatEconomyView::operator==(const FCombatEconomyView& Other) const
{
	return Gold == Other.Gold && GoldCap == Other.GoldCap
		&& PassiveGoldPerMinute == Other.PassiveGoldPerMinute
		&& EconomyRevision == Other.EconomyRevision
		&& InventoryRevision == Other.InventoryRevision
		&& ShopDefinitionId == Other.ShopDefinitionId
		&& InventoryItems == Other.InventoryItems && ShopOffers==Other.ShopOffers;
}

bool FCombatEconomyRequest::operator==(const FCombatEconomyRequest& O) const
{
	return RequestId==O.RequestId && CommandBindingGeneration==O.CommandBindingGeneration && ExpectedEconomyRevision==O.ExpectedEconomyRevision
		&& ExpectedInventoryRevision==O.ExpectedInventoryRevision && Action==O.Action && ItemDefinitionId==O.ItemDefinitionId && ItemHandle==O.ItemHandle
		&& ItemRevision==O.ItemRevision && PurchaseContextId==O.PurchaseContextId && PurchaseContextRevision==O.PurchaseContextRevision;
}
