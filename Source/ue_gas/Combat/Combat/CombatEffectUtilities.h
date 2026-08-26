#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "GameplayTagContainer.h"

#include "Combat/Core/CombatTypes.h"
#include "Combat/Log/CombatEventSubsystem.h"

class UCombatAbilitySystemComponent;

/** 为 Damage、Heal 与恢复提供只经 GAS 写入 Attribute 的内部工具。 */
namespace CombatEffectUtilities
{
	/** 以 SetByCaller Instant GE 写入 Damage/Heal 元属性，并携带自定义事务 Context。 */
	UE_GAS_API bool ApplyMetaEffect(
		UObject* EffectOuter,
		UCombatAbilitySystemComponent& TargetAsc,
		UCombatAbilitySystemComponent& SourceAsc,
		const FGameplayAttribute& MetaAttribute,
		const FGameplayTag& SetByCallerTag,
		float Amount,
		const FCombatEventContext& EventContext,
		const FCombatSourceContext& SourceContext);

	/** 以 Instant GE 对普通 Attribute 做 Additive 修改，供 Mana 恢复等非 Health 路径使用。 */
	UE_GAS_API bool ApplyAttributeAdditive(
		UObject* EffectOuter,
		UCombatAbilitySystemComponent& TargetAsc,
		const FGameplayAttribute& Attribute,
		float Magnitude);

	/** 以单个 Instant GE 按顺序覆盖一组基础 Attribute，供 UnitData 原子初始化使用。 */
	UE_GAS_API bool ApplyAttributeOverrides(
		UObject* EffectOuter,
		UCombatAbilitySystemComponent& TargetAsc,
		const TArray<TPair<FGameplayAttribute, float>>& AttributeValues);
}
