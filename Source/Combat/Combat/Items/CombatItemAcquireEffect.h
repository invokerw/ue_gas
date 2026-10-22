#pragma once
#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Combat/Core/CombatTypes.h"
#include "CombatItemAcquireEffect.generated.h"
class ACombatUnitCharacter;
class ACombatPlayerController;
class UCombatItemData;

/** 商店的只读购买条件；不复制效果对象，Revision 属于对应领域上下文。 */
USTRUCT(BlueprintType)
struct COMBAT_API FCombatShopOfferView
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category="Combat|Shop", meta=(DisplayName="物品定义", ToolTip="当前商品的稳定 CombatItem ID。")) FPrimaryAssetId DefinitionId;
	UPROPERTY() FGuid ContextId;
	UPROPERTY() int32 Revision=0;
	UPROPERTY(BlueprintReadOnly, Category="Combat|Shop", meta=(DisplayName="可购买", ToolTip="服务器确认的当前领域条件；余额仍由经济系统检查。")) bool bCanPurchase=false;
	UPROPERTY(BlueprintReadOnly, Category="Combat|Shop", meta=(DisplayName="剩余次数", ToolTip="-1 表示不限购，0 表示已用尽，正数为剩余可购买次数。")) int32 RemainingPurchases=-1;
	UPROPERTY(BlueprintReadOnly, Category="Combat|Shop", meta=(DisplayName="效果生效中", ToolTip="是否有已领取的持续效果仍在生效。")) bool bEffectActive=false;
	UPROPERTY(BlueprintReadOnly, Category="Combat|Shop", meta=(DisplayName="最后覆盖波", ToolTip="1-based 最后覆盖波次；0 表示立即使用或不按波次到期。")) int32 ExpireWave=0;
	UPROPERTY(BlueprintReadOnly, Category="Combat|Shop", meta=(DisplayName="领取者", ToolTip="最近一次成功领取者的名称。")) FString RecipientName;
	UPROPERTY(BlueprintReadOnly, Category="Combat|Shop", meta=(DisplayName="状态说明", ToolTip="服务器给出的阶段、次数和使用结果说明。")) FText Status;
	bool operator==(const FCombatShopOfferView& Other) const;
};

/** 服务器同步领取的上下文；所有指针只活在本次调用，不进入网络。 */
struct FCombatItemAcquireContext
{
	ACombatUnitCharacter* Recipient=nullptr;
	ACombatPlayerController* Player=nullptr;
	const UCombatItemData* Definition=nullptr;
	FCombatSourceContext Source;
};

/**
 * 物品拥有的同步领取行为。只能通过公共 ItemSubsystem 消费实例，不能在此扣金币或写库存。
 * CanUse 无副作用；Use 必须同步结束，失败时不留效果。普通 ActiveAbility 仍负责手动施法。
 */
UCLASS(Abstract, BlueprintType, EditInlineNew, DefaultToInstanced, meta=(DisplayName="获得即用效果", ToolTip="物品被购买、拾取或授予时的同步效果；不需要装备槽或施法输入。"))
class COMBAT_API UCombatItemAcquireEffect : public UObject
{
	GENERATED_BODY()
public:
	/** 校验配置；不允许异步或部分成功语义。 */
	virtual bool ValidateDefinition(FString& Error) const;
	/** 构建服务器只读商店条件，默认要求接收单位存活。 */
	virtual FCombatShopOfferView QueryOffer(ACombatUnitCharacter* Recipient,const UCombatItemData* Definition) const;
	/** 无副作用地确认本次领取仍可执行。 */
	virtual bool CanUse(const FCombatItemAcquireContext& Context,FGameplayTag& Failure) const;
	/** 只由物品登记表在领取锁内调用；成功后登记表消耗实例。 */
	virtual bool Use(const FCombatItemAcquireContext& Context,FGameplayTag& Failure) const;
};

/** 通用的获得即治疗；通过公共 Heal 结算自身，不直接写生命。 */
UCLASS(meta=(DisplayName="获得时治疗自身", ToolTip="领取成功即治疗接收者，不占库存；满生命时允许使用，治疗量受上限限制。"))
class COMBAT_API UCombatHealOnAcquireEffect : public UCombatItemAcquireEffect
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Combat|Item", meta=(DisplayName="治疗量", ToolTip="提交给公共治疗入口的基础生命量，范围 1 到 100000。", ClampMin="1", ClampMax="100000")) float Amount=100;
	virtual bool ValidateDefinition(FString& Error) const override;
	virtual bool Use(const FCombatItemAcquireContext& Context,FGameplayTag& Failure) const override;
};
