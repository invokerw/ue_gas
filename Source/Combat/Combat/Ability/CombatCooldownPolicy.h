#pragma once
#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "CombatCooldownPolicy.generated.h"
class UCombatAbilitySystemComponent;

/** 普通英雄技能的可选冷却取值策略（契约1）；只读ASC及有效Modifier，在原提交点冻结，不重排旧冷却。 */
UCLASS(BlueprintType, EditInlineNew, DefaultToInstanced, meta=(DisplayName="条件冷却策略", ToolTip="全局冷却缩减与可选效果参数相加，再应用本技能上限；物品主动不读取英雄策略。"))
class COMBAT_API UCombatCooldownPolicy : public UObject
{
    GENERATED_BODY()
public:
    /** 本技能允许的总缩减上限；不改变ASC的全局属性。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Cooldown", meta=(DisplayName="总冷却缩减上限", ToolTip="比例0到0.9；例如0.4将10秒冷却最低限制为6秒。全局与条件缩减相加后统一封顶。", ClampMin="0", ClampMax="0.9"))
    float MaxReduction=0.9f;
    /** 条件贡献来自当前有效ActiveGE绑定Runtime；None表示只用全局CDR。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Cooldown", meta=(DisplayName="额外缩减参数键", ToolTip="读取当前有效Modifier的同名参数并相加，每个实例计一次、不再乘层数；空值只读取ASC全局冷却缩减。"))
    FName BonusParameterKey;
    /** 校验本策略的有限上限，非法配置不得进入冷却提交。 */
    bool ValidateDefinition(FString& Error) const;
    /** 返回本次应冻结的秒数；无效配置/权限/输入返回-1，不修改ASC或效果。 */
    float CalculateDuration(UCombatAbilitySystemComponent* Asc, float BaseDuration) const;
};
