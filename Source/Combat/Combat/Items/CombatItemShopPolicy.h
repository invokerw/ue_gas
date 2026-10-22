#pragma once
#include "CoreMinimal.h"
#include "Combat/Items/CombatItemAcquireEffect.h"
#include "CombatItemShopPolicy.generated.h"

/**
 * 普通商品的同步购买条件。定义持有此只读策略，经济组件在本局复制定义后使用。
 * 不扣金币、不操作库存、不安排异步任务；仅在原经济事务成功后记录领域配额。
 */
UCLASS(Abstract, BlueprintType, EditInlineNew, DefaultToInstanced, meta=(DisplayName="商品购买策略", ToolTip="服务器判断普通无配方装备的购买窗口与配额；空值保持原商店规则。"))
class COMBAT_API UCombatItemShopPolicy : public UObject
{
    GENERATED_BODY()
public:
    /** 构造服务器只读条件；默认拒绝。不得修改领域状态或读取客户端传入的价格。 */
    virtual FCombatShopOfferView QueryOffer(ACombatUnitCharacter* Recipient, const UCombatItemData* Definition) const;
    /** 在经济与库存互斥内记录成功购买；AcceptedOffer 是预检时冻结的上下文，Player为事务开始时的稳定付款人，不依赖提交通知后的英雄存活；不能失败、扣款或广播。 */
    virtual void OnPurchased(ACombatPlayerController* Player, const FCombatShopOfferView& AcceptedOffer) const;
};
