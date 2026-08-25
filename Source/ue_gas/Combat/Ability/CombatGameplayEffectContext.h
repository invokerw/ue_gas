#pragma once

#include "CoreMinimal.h"
#include "GameplayEffectTypes.h"

#include "Combat/Core/CombatTypes.h"

#include "CombatGameplayEffectContext.generated.h"

/** 扩展 GAS EffectContext，携带战斗事件、来源与攻击身份快照。 */
USTRUCT()
struct UE_GAS_API FCombatGameplayEffectContext : public FGameplayEffectContext
{
	GENERATED_BODY()

	/** 当前 Effect 对应的直接战斗事件。 */
	UPROPERTY()
	FCombatEventId EventId;

	/** 当前事件链最外层的根战斗事件。 */
	UPROPERTY()
	FCombatEventId RootEventId;

	/** 仅服务器运行时使用的攻击句柄，不参与网络复制。 */
	UPROPERTY(NotReplicated)
	FCombatAttackHandle AttackHandle;

	/** 可复制的技能、Modifier、弹体来源身份。 */
	UPROPERTY()
	FCombatSourceContext Source;

	/** 深拷贝基础 HitResult 与全部 Combat 扩展字段。 */
	virtual FGameplayEffectContext* Duplicate() const override;
	/** 返回自定义 ScriptStruct，使 GAS 能识别派生上下文。 */
	virtual UScriptStruct* GetScriptStruct() const override;
	/** 复制基础上下文和 Combat 网络字段；AttackHandle 保持仅服务器。 */
	virtual bool NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess) override;
};

/** 启用 FCombatGameplayEffectContext 的拷贝与自定义网络序列化能力。 */
template<>
struct TStructOpsTypeTraits<FCombatGameplayEffectContext> : public TStructOpsTypeTraitsBase2<FCombatGameplayEffectContext>
{
	enum
	{
		/** 允许 GAS 通过拷贝语义复制自定义上下文。 */
		WithCopy = true,
		/** 指示 Unreal 调用自定义 NetSerialize。 */
		WithNetSerializer = true
	};
};
