#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "GameplayTagContainer.h"

#include "Combat/Core/CombatTypes.h"
#include "Combat/Log/CombatEventSubsystem.h"

class UCombatAbilitySystemComponent;

/** 伤害、治疗、恢复和单位初始化共用的 GAS 属性写入工具。调用方负责权限与业务校验；工具返回效果是否应用成功，不代表实际属性变化量。 */
namespace CombatEffectUtilities
{
	/** 创建立即生效的 GameplayEffect，用调用时传入的数值写入临时伤害或治疗属性，同时携带事件与来源身份。需要项目自定义效果上下文；实际生命变化由 AttributeSet 另行回报给事务槽。 */
	UE_GAS_API bool ApplyMetaEffect(
		UObject* EffectOuter,
		UCombatAbilitySystemComponent& TargetAsc,
		UCombatAbilitySystemComponent& SourceAsc,
		const FGameplayAttribute& MetaAttribute,
		const FGameplayTag& SetByCallerTag,
		float Amount,
		const FCombatEventContext& EventContext,
		const FCombatSourceContext& SourceContext);

	/** 通过立即生效的 GameplayEffect 增减一个普通属性，例如恢复法力；数值必须有限。生命变化应走伤害或治疗入口，以保留实际变化回报和相关事件。 */
	UE_GAS_API bool ApplyAttributeAdditive(
		UObject* EffectOuter,
		UCombatAbilitySystemComponent& TargetAsc,
		const FGameplayAttribute& Attribute,
		float Magnitude);

	/** 先校验整组属性与数值，再用一个立即生效的 GameplayEffect 按数组顺序覆盖基础值。初始化时先放最大生命/法力，再放当前值，使当前值按新上限限制。 */
	UE_GAS_API bool ApplyAttributeOverrides(
		UObject* EffectOuter,
		UCombatAbilitySystemComponent& TargetAsc,
		const TArray<TPair<FGameplayAttribute, float>>& AttributeValues);
}
