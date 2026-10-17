#pragma once
#include "CoreMinimal.h"
#include "Combat/Items/CombatItemAcquireEffect.h"
#include "Combat/Defense/DefenseTypes.h"
#include "DefenseTeamItemEffect.generated.h"
class UCombatModifierData;
/** 团队商品的获得效果；物品定义保存价格/身份，本配置仅描述领域效果，不扣金币。 */
UCLASS(meta=(DisplayName="防守团队获得效果", ToolTip="在防守模式中领取一次团队效果；限购和到期由 Director 管理，付款由原经济系统管理。"))
class COMBAT_API UDefenseTeamItemEffect : public UCombatItemAcquireEffect
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Defense", meta=(DisplayName="团队属性效果", ToolTip="无 Hook 的独立永久属性 Modifier，死亡保留、不可驱散；核心道具留空。")) TObjectPtr<UCombatModifierData> ModifierData;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Defense", meta=(DisplayName="持续波数", ToolTip="英雄属性为1–6，核心屏障为1，从下一波起计算；修复和援军商品为0。", ClampMin="0", ClampMax="6")) int32 DurationWaves=0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Defense", meta=(DisplayName="核心修复量", ToolTip="通过公共 Heal 固定量修复核心，范围0–100000；不能与其它效果同时配置。", ClampMin="0", ClampMax="100000")) int32 ObjectiveHeal=0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Defense", meta=(DisplayName="可领取阶段", ToolTip="仅允许准备、波间整备或 Boss 整备；战斗和终局不可领取。")) TArray<EDefensePhase> AllowedPhases;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Defense", meta=(DisplayName="核心护盾效果", ToolTip="仅核心屏障可填：专用护盾 Runtime、无限时长、不可驱散、死亡清理、无属性和额外参数。")) TObjectPtr<UCombatModifierData> CoreShieldData;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Defense", meta=(DisplayName="核心护盾比例", ToolTip="核心护盾按领取时最大生命快照；0禁用，正值不超过1，持续波数必须为1。", ClampMin="0", ClampMax="1")) float ObjectiveShieldFraction=0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Defense", meta=(DisplayName="核心修复比例", ToolTip="按领取时核心最大生命计算公共治疗请求，0禁用，正值不超过1；不能与固定修复、护盾或属性效果同时配置。", ClampMin="0", ClampMax="1")) float ObjectiveHealFraction=0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Defense", meta=(DisplayName="援军复活护盾", ToolTip="援军独立护盾定义：共享有限护盾Runtime、正持续时间、不可驱散且死亡移除；其它商品留空。")) TObjectPtr<UCombatModifierData> ReinforcementShieldData;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Defense", meta=(DisplayName="援军死亡次数", ToolTip="购买后可救援的新死亡次数，1–8；0禁用。已阵亡生命不追溯，使用后断线不退次数。", ClampMin="0", ClampMax="8")) int32 ReinforcementDeaths=0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Defense", meta=(DisplayName="复活缩短秒数", ToolTip="每次救援从普通等待中减去此秒数，最低为0，仍由下一监控回合安全复活；援军启用时须大于0。", ClampMin="0", ClampMax="60", Units="s")) float RespawnReduction=0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Defense", meta=(DisplayName="复活护盾比例", ToolTip="按对应生命复活时最大生命快照；0禁用，援军启用时为(0,1]，不随之后最大生命变化补充。", ClampMin="0", ClampMax="1")) float RespawnShieldFraction=0;
	virtual bool ValidateDefinition(FString& Error) const override;
	virtual FCombatShopOfferView QueryOffer(ACombatUnitCharacter* Recipient,const UCombatItemData* Definition) const override;
	virtual bool CanUse(const FCombatItemAcquireContext& Context,FGameplayTag& Failure) const override;
	virtual bool Use(const FCombatItemAcquireContext& Context,FGameplayTag& Failure) const override;
};
