#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

#include "Combat/Combat/CombatTransactionTypes.h"
#include "Combat/Core/CombatTypes.h"
#include "Combat/Log/CombatEventSubsystem.h"

#include "CombatAttackTypes.generated.h"

class ACombatUnitCharacter;
class UCombatModifierData;
class UCombatProjectileData;

/** 描述一条 AttackRecord 当前所处的权威生命周期阶段。 */
UENUM(BlueprintType)
enum class ECombatAttackState : uint8
{
	/** 已完成快照，等待前摇调度。 */
	Pending,
	/** 正在等待 attack point。 */
	Windup,
	/** 已越过 attack point，近战会立即请求命中。 */
	Launched,
	/** Damage 与 OnHit 已完成。 */
	Landed,
	/** 取消、闪避、目标失效或伤害入口失败。 */
	Failed
};

/** AttackRecord exactly-once 结束时使用的稳定结果分类。 */
UENUM(BlueprintType)
enum class ECombatAttackOutcome : uint8
{
	/** 攻击成功落地。 */
	Landed,
	/** 目标通过 Evasion roll 闪避。 */
	Evaded,
	/** attack point 或 impact 时目标已失效。 */
	TargetInvalid,
	/** 前摇被 Stop、状态或生命周期取消。 */
	Cancelled,
	/** DamageSubsystem 拒绝或无法完成结算。 */
	DamageFailed
};

/** 法球 winner 可以写入 AttackRecord 的 OnHit 快照动作类型。 */
UENUM(BlueprintType)
enum class ECombatOnHitActionType : uint8
{
	/** 命中后通过 DamageSubsystem 追加一次伤害。 */
	Damage,
	/** 命中后通过 ModifierComponent 施加 Modifier。 */
	ApplyModifier
};

/** 不依赖后续 Ability/Modifier 状态的不可变 OnHit 动作快照。 */
USTRUCT(BlueprintType)
struct UE_GAS_API FCombatOnHitAction
{
	GENERATED_BODY()

	/** 选择额外伤害或施加 Modifier。 */
	UPROPERTY(BlueprintReadWrite, Category="Combat|Attack|OnHit") ECombatOnHitActionType Type = ECombatOnHitActionType::Damage;
	/** Damage 动作使用的非负快照数值。 */
	UPROPERTY(BlueprintReadWrite, Category="Combat|Attack|OnHit") float Magnitude = 0.0f;
	/** Damage 动作使用的抗性分支。 */
	UPROPERTY(BlueprintReadWrite, Category="Combat|Attack|OnHit") ECombatDamageType DamageType = ECombatDamageType::Physical;
	/** ApplyModifier 动作使用的稳定定义对象。 */
	UPROPERTY(BlueprintReadWrite, Category="Combat|Attack|OnHit") TObjectPtr<UCombatModifierData> ModifierData = nullptr;
	/** ApplyModifier 的持续时间覆盖；小于 0 使用定义值。 */
	UPROPERTY(BlueprintReadWrite, Category="Combat|Attack|OnHit") float DurationOverride = -1.0f;
	/** ApplyModifier 创建 Runtime 与动态 GE 时使用的不可变参数覆盖。 */
	UPROPERTY(BlueprintReadWrite, Category="Combat|Attack|OnHit", meta=(DisplayName="运行时参数覆盖", ToolTip="施加 OnHit Modifier 时冻结的参数覆盖；同名键覆盖 Modifier 定义值。")) TMap<FName, float> RuntimeParameterOverrides;
};

/** Modifier 无副作用评估法球候选时读取的攻击上下文。 */
USTRUCT(BlueprintType)
struct UE_GAS_API FCombatAttackCandidateContext
{
	GENERATED_BODY()

	/** 本轮待创建或已创建的稳定 AttackHandle。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Attack") FCombatAttackHandle Handle;
	/** 发起普通攻击的单位。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Attack") TObjectPtr<ACombatUnitCharacter> Attacker = nullptr;
	/** 本轮权威目标。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Attack") TObjectPtr<ACombatUnitCharacter> Target = nullptr;
	/** 收集法球前的基础攻击伤害。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Attack") float BaseDamage = 0.0f;
};

/** 单个 exclusive group 的法球 winner 提交后产生的不可变快照。 */
USTRUCT(BlueprintType)
struct UE_GAS_API FCombatOrbSnapshot
{
	GENERATED_BODY()

	/** 同组只能有一个 winner；None 表示不是合法法球。 */
	UPROPERTY(BlueprintReadWrite, Category="Combat|Attack|Orb") FName ExclusiveGroup;
	/** 成功提交该快照的 Modifier。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Attack|Orb") FCombatModifierHandle SourceModifier;
	/** 合并到主攻击伤害的快照加值。 */
	UPROPERTY(BlueprintReadWrite, Category="Combat|Attack|Orb") float BonusDamage = 0.0f;
	/** true 时覆盖主攻击 DamageType。 */
	UPROPERTY(BlueprintReadWrite, Category="Combat|Attack|Orb") bool bOverrideDamageType = false;
	/** 覆盖主攻击时使用的 DamageType。 */
	UPROPERTY(BlueprintReadWrite, Category="Combat|Attack|Orb") ECombatDamageType DamageType = ECombatDamageType::Physical;
	/** 命中后按快照顺序执行的公共动作。 */
	UPROPERTY(BlueprintReadWrite, Category="Combat|Attack|Orb") TArray<FCombatOnHitAction> OnHitActions;
	/** 非空时覆盖 UnitData 的普攻弹体定义，并随 Record 冻结。 */
	UPROPERTY(BlueprintReadWrite, Category="Combat|Attack|Orb", meta=(DisplayName="弹体定义覆盖", ToolTip="非空时由法球覆盖 UnitData 的普通攻击弹体定义，并随攻击记录冻结。")) TObjectPtr<UCombatProjectileData> ProjectileDataOverride = nullptr;
};

/** AttackTiming policy 对同一次前摇计算出的完整冻结结果。 */
USTRUCT(BlueprintType)
struct UE_GAS_API FCombatAttackTiming
{
	GENERATED_BODY()

	/** clamp 后参与公式的攻击速度。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Attack|Timing") float EffectiveAttackSpeed = 100.0f;
	/** 两次攻击起手之间的权威间隔。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Attack|Timing") float AttackInterval = 1.7f;
	/** 本轮从起手到 AttackLaunched 的前摇。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Attack|Timing") float AttackPoint = 0.3f;
	/** AttackLaunched 到再次 Ready 的剩余时间。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Attack|Timing") float Recovery = 1.4f;
	/** Montage 可读取但不能反向驱动 gameplay 的播放速率。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Attack|Timing") float AnimationRate = 1.0f;
};

/** AttackComponent registry 中一条仍可被句柄解析的权威记录。 */
USTRUCT(BlueprintType)
struct UE_GAS_API FCombatAttackRecord
{
	GENERATED_BODY()

	/** 记录的稳定外部句柄。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Attack") FCombatAttackHandle Handle;
	/** 创建记录的单位。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Attack") TWeakObjectPtr<ACombatUnitCharacter> Attacker;
	/** 本轮权威目标。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Attack") TWeakObjectPtr<ACombatUnitCharacter> Target;
	/** 创建记录时快照的目标生命代次。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Attack") int64 TargetLifeGeneration = 0;
	/** 使本轮攻击被持续 Order 识别的 OrderHandle。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Attack") FCombatOrderHandle OrderHandle;
	/** 攻击创建时分配的根事件。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Attack") FCombatEventContext EventContext;
	/** 不含法球的属性快照伤害。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Attack") float BaseDamage = 0.0f;
	/** 全部法球 winner 合并后的快照加值。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Attack") float BonusDamage = 0.0f;
	/** 主攻击最终使用的伤害类型。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Attack") ECombatDamageType DamageType = ECombatDamageType::Physical;
	/** 创建记录时冻结的暴击概率。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Attack") float CriticalChance = 0.0f;
	/** 暴击成功时乘到主伤害上的倍率。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Attack") float CriticalMultiplier = 2.0f;
	/** 本轮是否通过 Crit roll。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Attack") bool bCritical = false;
	/** 本轮集中计算的 attack point、interval 与表现速率。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Attack") FCombatAttackTiming Timing;
	/** 成功提交且写入记录的法球 winner。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Attack") TArray<FCombatOrbSnapshot> ClaimedOrbs;
	/** 从 winner 复制出的不可变 OnHit 动作。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Attack") TArray<FCombatOnHitAction> OnHitActions;
	/** 法球 winner 选择的普攻弹体；为空时回退 UnitData。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Attack", meta=(DisplayName="弹体定义覆盖", ToolTip="法球胜者冻结到本次攻击记录的弹体定义；为空时回退 UnitData。")) TObjectPtr<UCombatProjectileData> ProjectileDataOverride = nullptr;
	/** 当前 exactly-once 状态。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Attack") ECombatAttackState State = ECombatAttackState::Pending;
	/** 起手时的绝对服务器 World Game Time。 */
	double StartedAt = 0.0;
};

/** 一次攻击开始、命中或失败返回给 Order/测试的结构化结果。 */
USTRUCT(BlueprintType)
struct UE_GAS_API FCombatAttackResult
{
	GENERATED_BODY()

	/** 请求是否完成其预期操作。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Attack") bool bSuccess = false;
	/** 本次结果对应的 AttackHandle。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Attack") FCombatAttackHandle Handle;
	/** exactly-once 结束分类。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Attack") ECombatAttackOutcome Outcome = ECombatAttackOutcome::Cancelled;
	/** 失败时提供的稳定原因。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Attack") FGameplayTag FailureTag;
	/** 主伤害与 OnHit 真实减少的生命总量。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Attack") float AppliedDamage = 0.0f;
};
