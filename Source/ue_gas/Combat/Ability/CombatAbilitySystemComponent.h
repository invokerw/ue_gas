#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"

#include "CombatAbilitySystemComponent.generated.h"

/** 扩展 GAS ASC，统一维护 Combat ActorInfo 的初始化与清理契约。 */
UCLASS(ClassGroup=(Combat), meta=(BlueprintSpawnableComponent))
class UE_GAS_API UCombatAbilitySystemComponent : public UAbilitySystemComponent
{
	GENERATED_BODY()

public:
	/** 构造默认开启复制的 Combat ASC。 */
	UCombatAbilitySystemComponent();

	/** 使用明确的 Owner/Avatar 初始化 ActorInfo，并避免重复初始化同一对 Actor。 */
	void InitializeCombatActorInfo(AActor* InOwnerActor, AActor* InAvatarActor);
	/** 清空 ActorInfo，供失去控制权或 EndPlay 时停止后续 GAS 操作。 */
	void ClearCombatActorInfo();
	/** 返回 ActorInfo 是否同时具有有效 Owner 与 Avatar。 */
	bool IsCombatActorInfoInitialized() const;
};
