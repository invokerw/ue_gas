#include "Combat/Defense/DefenseEquipment.h"
#include "Combat/Defense/DefenseDirectorComponent.h"
#include "Combat/Items/CombatItemData.h"
#include "Combat/Items/CombatInventoryComponent.h"
#include "Combat/Defense/DefenseGameMode.h"
#include "Combat/Unit/CombatUnitCharacter.h"
#include "CombatPlayerController.h"
#include "Engine/World.h"

namespace DefenseEquipment
{
    /** 服务器只查同World的正式模式；其他地图或已销毁接收者一律关闭购买。 */
    UDefenseDirectorComponent* Director(ACombatUnitCharacter* Recipient)
    {
        auto* Mode = IsValid(Recipient) && Recipient->HasAuthority() && Recipient->GetWorld()
            ? Recipient->GetWorld()->GetAuthGameMode<ADefenseGameMode>() : nullptr;
        return Mode ? Mode->GetDefenseDirector() : nullptr;
    }
}
FCombatShopOfferView UDefenseEquipmentShopPolicy::QueryOffer(ACombatUnitCharacter* Recipient, const UCombatItemData* Definition) const
{
    auto* Director = DefenseEquipment::Director(Recipient);
    auto* Player = Recipient ? Cast<ACombatPlayerController>(Recipient->GetResourceOwnerPlayerController()) : nullptr;
    return Director ? Director->BuildEquipmentOffer(Player, Definition) : Super::QueryOffer(Recipient, Definition);
}
void UDefenseEquipmentShopPolicy::OnPurchased(ACombatPlayerController* Player, const FCombatShopOfferView& AcceptedOffer) const
{
    auto* Mode = IsValid(Player) && Player->HasAuthority() && Player->GetWorld()
        ? Player->GetWorld()->GetAuthGameMode<ADefenseGameMode>() : nullptr;
    if (Mode) Mode->GetDefenseDirector()->RecordEquipmentPurchase(Player, AcceptedOffer);
}
FCombatShopOfferView UDefenseDirectorComponent::BuildEquipmentOffer(ACombatPlayerController* Player, const UCombatItemData* Item) const
{
    FCombatShopOfferView Offer;
    Offer.DefinitionId = Item ? Item->GetPrimaryAssetId() : FPrimaryAssetId();
    Offer.ContextId = View.RunId;
    Offer.Revision = View.PhaseGeneration > 0 && View.PhaseGeneration <= MAX_int32 ? int32(View.PhaseGeneration) : 0;
    Offer.RemainingPurchases = 0;
    const auto* Record = Players.Find(Player);
    auto* Hero = Record ? Record->Hero.Get() : nullptr;
    if (!Item || !IsValid(Player) || Player->GetWorld() != GetWorld() || !IsValid(Hero)
        || Hero != Player->GetCommandedUnit() || Hero->GetResourceOwnerPlayerController() != Player || !Offer.Revision)
    { Offer.Status = FText::FromString(TEXT("当前英雄未登记到本局")); return Offer; }
    const int32 Used = Record->EquipmentPurchasePhase == Offer.Revision ? Record->EquipmentPurchases : 0;
    Offer.RemainingPurchases = FMath::Max(0, 2 - Used);
    if (!IsTeamShopOpen() || !IsCoreAlive() || Hero->GetLifeState() != ECombatLifeState::Alive)
    { Offer.Status = FText::FromString(TEXT("仅存活英雄可在整备期购买")); return Offer; }
    if (!Hero->GetCombatInventoryComponent()->IsWithinOwnershipLimit(*Item, 1))
    { Offer.Status = FText::FromString(TEXT("同款装备已达持有上限，背包和锁定仍计入")); return Offer; }
    Offer.bCanPurchase = Offer.RemainingPurchases > 0;
    Offer.Status = FText::FromString(FString::Printf(TEXT("本次整备还能购买 %d 件防守装备；出售不返还次数"), Offer.RemainingPurchases));
    return Offer;
}
void UDefenseDirectorComponent::RecordEquipmentPurchase(ACombatPlayerController* Player, const FCombatShopOfferView& AcceptedOffer)
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || GetNetMode() == NM_Client || bEnding
        || AcceptedOffer.ContextId != View.RunId || AcceptedOffer.Revision <= 0) return;
    auto* Record = Players.Find(Player);
    if (!Record) return;
    // 通知可能已推进阶段，仍记录预检时冻结的旧窗口，不能把旧购买扣到新窗口。
    if (Record->EquipmentPurchasePhase != AcceptedOffer.Revision)
    { Record->EquipmentPurchasePhase = AcceptedOffer.Revision; Record->EquipmentPurchases = 0; }
    ++Record->EquipmentPurchases;
}
void UDefenseEquipmentDamageRuntime::OnPreDealDamage_Implementation(FCombatDamageEvent& Event)
{
    auto* Hero = GetTargetUnit();
    if (!IsActive() || !Hero || Event.Source != Hero || !Event.Target || Event.Amount <= 0) return;
    if (GetRuntimeParameter(TEXT("direct_attack_only")) > 0
        && Event.SourceContext.DirectSourceType != ECombatDirectSourceType::Attack) return;
    if (GetRuntimeParameter(TEXT("special_targets_only")) > 0)
    {
        const FString Name = Event.Target->GetUnitDefinitionId().PrimaryAssetName.ToString().ToLower();
        if (!Name.Contains(TEXT("elite")) && !Name.Contains(TEXT("siege")) && !Name.Contains(TEXT("boss"))) return;
    }
    const float Fraction = GetRuntimeParameter(TEXT("damage_fraction"));
    if (FMath::IsFinite(Fraction) && Fraction > 0 && Fraction <= 1) Event.Amount *= 1.0f + Fraction;
}
