#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemGlobals.h"

#include "CombatAbilitySystemGlobals.generated.h"

/** 为项目配置自定义 GameplayEffectContext 分配策略。 */
UCLASS(Config=Game)
class UE_GAS_API UCombatAbilitySystemGlobals : public UAbilitySystemGlobals
{
	GENERATED_BODY()

public:
	/** 为每个 GameplayEffectSpec 分配包含 Combat 身份链的上下文。 */
	virtual FGameplayEffectContext* AllocGameplayEffectContext() const override;
};
