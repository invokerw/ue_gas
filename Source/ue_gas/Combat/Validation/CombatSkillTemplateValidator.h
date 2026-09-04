#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

class UCombatAbilityData;
class UCombatDefinitionData;
class UCombatGameplayAbility;

/** 新技能复制模板时声明的必需 special 与行为约束。 */
struct UE_GAS_API FCombatSkillTemplateRequirements
{
	/** 必须存在且覆盖 MaxLevel 的平衡数值键。 */
	TArray<FName> RequiredSpecialKeys;
	/** 必须存在的 Ability Behavior 标签。 */
	FGameplayTagContainer RequiredBehaviorTags;
	/** true 时要求 Intrinsic Modifier，适用于被动/法球模板。 */
	bool bRequireIntrinsicModifier = false;
	/** true 时要求至少一个公共 DataDriven Action。 */
	bool bRequirePublicAction = false;
};

/** 不依赖 Editor-only 模块的技能模板身份、schema、事件与旁路检查工具。 */
struct UE_GAS_API FCombatSkillTemplateValidator
{
	/** 检查技能类默认对象是否引用传入的同一份技能定义，并检查运行时配置和模板必需字段；错误追加到 OutErrors，返回值只表示本次是否新增错误。 */
	static bool ValidateAbilityTemplate(
		TSubclassOf<UCombatGameplayAbility> AbilityClass,
		const UCombatAbilityData* AbilityData,
		const FCombatSkillTemplateRequirements& Requirements,
		TArray<FString>& OutErrors);
	/** 校验一组 DefinitionId 无空值或重复。 */
	static bool ValidateDefinitions(
		const TArray<const UCombatDefinitionData*>& Definitions,
		TArray<FString>& OutErrors);
	/** 校验实际 Combat Event 是否按完全一致顺序匹配模板期望。 */
	static bool ValidateEventSequence(
		const TArray<FGameplayTag>& Actual,
		const TArray<FGameplayTag>& Expected,
		TArray<FString>& OutErrors);
	/** 返回需要由外部源码扫描检查的禁用调用文本，如直接写生命、直接移位和 Actor Timer；本函数只提供清单，不实际扫描或阻止调用。 */
	static const TArray<FString>& GetForbiddenBypassPatterns();
};
