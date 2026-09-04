#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

#include "Combat/Core/CombatTypes.h"
#include "Combat/Log/CombatEventSubsystem.h"

#include "CombatTransactionTypes.generated.h"

class ACombatUnitCharacter;

/**
 * 决定伤害使用的抗性公式与魔法免疫规则；当前结算也只对非物理伤害应用技能伤害增幅。
 * 纯粹伤害仍受无敌和护盾影响；HPLoss（生命移除）标志另行跳过这些防御与效果回调，不由此枚举控制。
 */
UENUM(BlueprintType)
enum class ECombatDamageType : uint8
{
	/** 使用 Armor 公式结算的物理伤害。 */
	Physical UMETA(DisplayName="物理伤害"),
	/** 使用 MagicResist 结算并受魔免阻挡的魔法伤害。 */
	Magical UMETA(DisplayName="魔法伤害"),
	/** 跳过 Armor 与 MagicResist 的纯粹伤害。 */
	Pure UMETA(DisplayName="纯粹伤害")
};

/** 标识同步结果槽等待 AttributeSet 回报的元属性类型。 */
UENUM()
enum class ECombatTransactionKind : uint8
{
	/** 槽位等待 IncomingDamage 的真实生命变化。 */
	Damage,
	/** 槽位等待 IncomingHealing 的真实生命变化。 */
	Heal
};

/** 调用 DamageSubsystem 所需的服务器权威输入。 */
USTRUCT(BlueprintType)
struct UE_GAS_API FCombatDamageRequest
{
	GENERATED_BODY()

	/** 伤害直接来源单位。 */
	UPROPERTY(BlueprintReadWrite, Category="Combat|Damage") TObjectPtr<ACombatUnitCharacter> Source = nullptr;
	/** 承受伤害的目标单位。 */
	UPROPERTY(BlueprintReadWrite, Category="Combat|Damage") TObjectPtr<ACombatUnitCharacter> Target = nullptr;
	/** 效果回调和防御计算前的请求伤害；必须是有限非负数，实际扣血可能更少或为 0。 */
	UPROPERTY(BlueprintReadWrite, Category="Combat|Damage") float Amount = 0.0f;
	/** 选择物理、魔法或纯粹抗性分支。 */
	UPROPERTY(BlueprintReadWrite, Category="Combat|Damage") ECombatDamageType DamageType = ECombatDamageType::Physical;
	/** 伤害语义标签，例如 HPLoss 表示直接移除生命，NoLifesteal 禁止吸血；后续伤害回调还可据此防止反射递归。 */
	UPROPERTY(BlueprintReadWrite, Category="Combat|Damage") FGameplayTagContainer Flags;
	/** 可记录和复制的直接来源身份。 */
	UPROPERTY(BlueprintReadWrite, Category="Combat|Damage") FCombatSourceContext SourceContext;
	/** 有效时创建该事件的子事件并共享根事件 ID，超过事件深度上限会失败；无效时创建新的根事件。 */
	UPROPERTY(BlueprintReadWrite, Category="Combat|Damage") FCombatEventContext ParentEvent;
};

/** 伤害各阶段共享的事件数据。伤害前回调调整 Amount；护盾回调还需累计 AbsorbedAmount。来源、目标和事件身份由结算入口建立，不应由效果回调改写。 */
USTRUCT(BlueprintType)
struct UE_GAS_API FCombatDamageEvent
{
	GENERATED_BODY()

	/** 当前结算的事件树身份。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Damage") FCombatEventContext Context;
	/** 当前伤害来源单位。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Damage") TObjectPtr<ACombatUnitCharacter> Source = nullptr;
	/** 当前伤害目标单位。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Damage") TObjectPtr<ACombatUnitCharacter> Target = nullptr;
	/** 进入流水线时的原始请求量。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Damage") float RequestedAmount = 0.0f;
	/** 当前阶段待扣除的伤害量；伤害前和护盾回调可调整，最终仍受目标剩余生命限制。 */
	UPROPERTY(BlueprintReadWrite, Category="Combat|Damage") float Amount = 0.0f;
	/** 抗性计算前后之差，或免疫完全挡下的请求量；负护甲/负魔抗放大伤害时可为负数，不包含护盾吸收。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Damage") float MitigatedAmount = 0.0f;
	/** 护盾回调累计抵消的伤害；护盾应同步减少 Amount 并增加本字段。 */
	UPROPERTY(BlueprintReadWrite, Category="Combat|Damage") float AbsorbedAmount = 0.0f;
	/** AttributeSet 回报的真实生命减少量。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Damage") float AppliedAmount = 0.0f;
	/** 本次伤害的抗性类型。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Damage") ECombatDamageType DamageType = ECombatDamageType::Physical;
	/** 本次伤害的语义标志。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Damage") FGameplayTagContainer Flags;
	/** 本次事件的稳定来源身份。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Damage") FCombatSourceContext SourceContext;
};

/** DamageSubsystem 返回给调用者的完整同步结果。 */
USTRUCT(BlueprintType)
struct UE_GAS_API FCombatDamageResult
{
	GENERATED_BODY()

	/** 流水线是否成功完成；免疫阻挡仍属于成功结果。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Damage") bool bSuccess = false;
	/** 是否在写入 Health 前被免疫或无敌完全阻挡。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Damage") bool bBlocked = false;
	/** 失败时提供稳定的机器原因。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Damage") FGameplayTag FailureTag;
	/** 流水线最终事件快照。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Damage") FCombatDamageEvent Event;
};

/** 调用 HealSubsystem 所需的服务器权威输入。 */
USTRUCT(BlueprintType)
struct UE_GAS_API FCombatHealRequest
{
	GENERATED_BODY()

	/** 治疗直接来源单位。 */
	UPROPERTY(BlueprintReadWrite, Category="Combat|Heal") TObjectPtr<ACombatUnitCharacter> Source = nullptr;
	/** 接受治疗的目标单位。 */
	UPROPERTY(BlueprintReadWrite, Category="Combat|Heal") TObjectPtr<ACombatUnitCharacter> Target = nullptr;
	/** Hook 前的非负请求治疗量。 */
	UPROPERTY(BlueprintReadWrite, Category="Combat|Heal") float Amount = 0.0f;
	/** 可记录和复制的直接来源身份。 */
	UPROPERTY(BlueprintReadWrite, Category="Combat|Heal") FCombatSourceContext SourceContext;
	/** 有效时创建该事件的子事件并共享根事件 ID，超过事件深度上限会失败；无效时创建新的根事件。 */
	UPROPERTY(BlueprintReadWrite, Category="Combat|Heal") FCombatEventContext ParentEvent;
};

/** 治疗各阶段共享的事件数据；治疗前回调通过 Amount 调整待恢复量，来源、目标与事件身份不应改写。 */
USTRUCT(BlueprintType)
struct UE_GAS_API FCombatHealEvent
{
	GENERATED_BODY()

	/** 当前结算的事件树身份。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Heal") FCombatEventContext Context;
	/** 当前治疗来源单位。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Heal") TObjectPtr<ACombatUnitCharacter> Source = nullptr;
	/** 当前治疗目标单位。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Heal") TObjectPtr<ACombatUnitCharacter> Target = nullptr;
	/** 进入流水线时的原始请求量。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Heal") float RequestedAmount = 0.0f;
	/** Hook 当前可继续调整的治疗量。 */
	UPROPERTY(BlueprintReadWrite, Category="Combat|Heal") float Amount = 0.0f;
	/** 超出 MaxHealth 而未实际写入的治疗量。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Heal") float OverhealAmount = 0.0f;
	/** AttributeSet 回报的真实生命增加量。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Heal") float AppliedAmount = 0.0f;
	/** 本次事件的稳定来源身份。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Heal") FCombatSourceContext SourceContext;
};

/** HealSubsystem 返回给调用者的完整同步结果。 */
USTRUCT(BlueprintType)
struct UE_GAS_API FCombatHealResult
{
	GENERATED_BODY()

	/** 流水线是否成功完成。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Heal") bool bSuccess = false;
	/** 失败时提供稳定的机器原因。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Heal") FGameplayTag FailureTag;
	/** 流水线最终事件快照。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Heal") FCombatHealEvent Event;
};

/** AttributeSet 向同步事务槽回报的真实生命变化。 */
struct UE_GAS_API FCombatTransactionDelta
{
	/** 变化前的 Health。 */
	float PreviousHealth = 0.0f;
	/** 变化后的 Health。 */
	float NewHealth = 0.0f;
	/** 限制在 0 到最大生命之间后实际减少或增加的生命量；例如剩余 30 点生命承受 100 点伤害，此处为 30。 */
	float AppliedAmount = 0.0f;
	/** 本次 Damage 是否首次跨过致死阈值。 */
	bool bLethal = false;
};
