#pragma once
#include "CoreMinimal.h"
#include "Combat/Items/CombatItemShopPolicy.h"
#include "Combat/Modifiers/CombatModifierRuntime.h"
#include "DefenseEquipment.generated.h"

/** 防守普通装备的购买适配器；只消费当前服务器Director，不保存第二套对局或金币。 */
UCLASS(meta=(DisplayName="防守装备购买规则", ToolTip="仅当前登记英雄在整备期购买，每窗最多两件；真实库存的同款限额由物品定义配置。"))
class COMBAT_API UDefenseEquipmentShopPolicy : public UCombatItemShopPolicy
{
    GENERATED_BODY()
public:
    virtual FCombatShopOfferView QueryOffer(ACombatUnitCharacter* Recipient, const UCombatItemData* Definition) const override;
    virtual void OnPurchased(ACombatPlayerController* Player, const FCombatShopOfferView& AcceptedOffer) const override;
};

/** 装备常驻增伤；公共PreDealDamage调整当前事件，不创建派生伤害或新计时器。 */
UCLASS(meta=(DisplayName="防守装备增伤", ToolTip="damage_fraction配置增幅；direct_attack_only限制直接普攻，special_targets_only限制精英、攻城与Boss的稳定定义身份。"))
class COMBAT_API UDefenseEquipmentDamageRuntime : public UCombatModifierRuntime
{
    GENERATED_BODY()
protected:
    virtual void OnPreDealDamage_Implementation(FCombatDamageEvent& Event) override;
};
