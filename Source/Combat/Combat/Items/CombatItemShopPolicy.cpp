#include "Combat/Items/CombatItemShopPolicy.h"
#include "Combat/Items/CombatItemData.h"

FCombatShopOfferView UCombatItemShopPolicy::QueryOffer(ACombatUnitCharacter*, const UCombatItemData* Definition) const
{
    FCombatShopOfferView Offer;
    if (Definition) Offer.DefinitionId = Definition->GetPrimaryAssetId();
    return Offer;
}
void UCombatItemShopPolicy::OnPurchased(ACombatPlayerController*, const FCombatShopOfferView&) const {}
