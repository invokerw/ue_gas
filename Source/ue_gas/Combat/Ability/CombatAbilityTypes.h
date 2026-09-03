#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

#include "Combat/Combat/CombatTransactionTypes.h"
#include "Combat/Core/CombatTypes.h"
#include "Combat/Targeting/CombatTargetingTypes.h"

#include "CombatAbilityTypes.generated.h"

class ACombatUnitCharacter;
class UCombatModifierData;
class UCombatProjectileData;

/** Cost 与 Cooldown 可以选择的三个冻结提交阶段。 */
UENUM(BlueprintType)
enum class ECombatAbilityCommitStage : uint8
{
	/** 进入前摇并发出 CastStarted 时提交。 */
	CastStarted,
	/** cast point 完成、目标复核通过后提交。 */
	SpellStarted,
	/** Ability 正常结束前提交；中断不提交。 */
	AbilityEnded
};

/** UnitTarget 在 cast point 丢失时采用的处理规则。 */
UENUM(BlueprintType)
enum class ECombatTargetLostPolicy : uint8
{
	/** 目标失效立即中断 Ability。 */
	Fail,
	/** 仅允许点/AoE 行为继续使用激活时的最后已知位置。 */
	UseLastKnownPoint
};

/** 引导被中断后 OrderComponent 应如何处理后续队列。 */
UENUM(BlueprintType)
enum class ECombatChannelInterruptOrderPolicy : uint8
{
	/** 释放当前施法 Order，保留后续队列。 */
	Continue,
	/** 通知 OrderComponent 清除尚未执行的排队项。 */
	ClearQueuedOrders
};

/** DataDriven Action schema v1 的全部动作类型。 */
UENUM(BlueprintType)
enum class ECombatAbilityActionType : uint8
{
	/** 调用 CombatDamageSubsystem。 */
	Damage,
	/** 调用 CombatHealSubsystem。 */
	Heal,
	/** 调用目标 ModifierComponent。 */
	ApplyModifier,
	/** 向目标 ASC 发送 GameplayEvent。 */
	SendGameplayEvent,
	/** 生成服务器权威直线 Projectile。 */
	SpawnLinearProjectile,
	/** 生成服务器权威追踪 Projectile。 */
	SpawnTrackingProjectile,
	/** 创建由 Combat Scheduler 驱动的区域 Thinker。 */
	CreateThinker
};

/** 声明 Action 应使用施法者、单位目标还是服务器 AoE 查询结果。 */
UENUM(BlueprintType)
enum class ECombatAbilityActionTarget : uint8
{
	/** Action 只作用于施法者。 */
	Caster,
	/** Action 作用于权威 UnitTarget 快照。 */
	UnitTarget,
	/** Action 在权威点位置执行服务器半径查询。 */
	UnitsInRadius
};

/** AbilityData 中一条可由公共服务器入口执行的动作。 */
USTRUCT(BlueprintType)
struct UE_GAS_API FCombatAbilityAction
{
	GENERATED_BODY()

	/** 选择 Damage、Heal、Modifier、Event、Projectile 或 Thinker 动作。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Ability|Action") ECombatAbilityActionType Type = ECombatAbilityActionType::Damage;
	/** 选择本动作的权威目标集合。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Ability|Action") ECombatAbilityActionTarget Target = ECombatAbilityActionTarget::UnitTarget;
	/** 从 AbilityData.SpecialValues 按当前 Spec.Level 读取的数值键。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Ability|Action") FName MagnitudeKey;
	/** Damage Action 使用的物理、魔法或纯粹类型。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Ability|Action") ECombatDamageType DamageType = ECombatDamageType::Magical;
	/** ApplyModifier Action 使用的稳定 Modifier 定义。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Ability|Action") TObjectPtr<UCombatModifierData> ModifierData = nullptr;
	/** UnitsInRadius 从 SpecialValues 读取服务器查询半径的键。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Ability|Action") FName RadiusKey;
	/** SendGameplayEvent 使用的事件标签。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Ability|Action") FGameplayTag EventTag;
	/** SpawnLinear/TrackingProjectile 使用的稳定 ProjectileData。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Ability|Action") TObjectPtr<UCombatProjectileData> ProjectileData = nullptr;
	/** Projectile 从 SpecialValues 读取速度覆盖的可选键。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Ability|Action") FName ProjectileSpeedKey;
	/** Projectile 从 SpecialValues 读取最大距离覆盖的可选键。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Ability|Action") FName ProjectileRangeKey;
	/** CreateThinker 从 SpecialValues 读取持续时间的键。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Ability|Action") FName DurationKey;
	/** CreateThinker 从 SpecialValues 读取 pulse 间隔的键；None 表示只 pulse 一次。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Ability|Action") FName IntervalKey;
	/** Projectile 命中后是否把 Modifier 与拖向 Source 的 Motion 请求一起快照。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Ability|Action") bool bMotionToSource = false;
	/** Hook Motion 从 SpecialValues 读取速度的键。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Ability|Action") FName MotionSpeedKey;
	/** Hook Motion 获取水平通道时使用的优先级。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Ability|Action") int32 MotionPriority = 100;
};

/** 每次 Instanced Ability 保存的服务器权威激活快照。 */
USTRUCT(BlueprintType)
struct UE_GAS_API FCombatAbilityActivationContext
{
	GENERATED_BODY()

	/** 本次激活对应的根 Combat Event。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Ability") FCombatEventContext EventContext;
	/** 激活来源 Unit。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Ability") TObjectPtr<ACombatUnitCharacter> Caster = nullptr;
	/** UnitTarget cast point 复核后的 Actor 快照。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Ability") TObjectPtr<ACombatUnitCharacter> TargetActor = nullptr;
	/** PointTarget 或 UnitTarget 激活时记录的权威位置。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Ability") FVector TargetLocation = FVector::ZeroVector;
	/** 施法者激活时的生命代次，用于淘汰跨复活回调。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Ability") int64 CasterLifeGeneration = 0;
	/** TargetActor 激活时的生命代次；没有单位目标时为 0。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Ability") int64 TargetLifeGeneration = 0;
	/** 当前 AbilitySpec 的权威等级快照。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Ability") int32 AbilityLevel = 1;
};

/** DataDriven Action 执行后的聚合结果。 */
USTRUCT(BlueprintType)
struct UE_GAS_API FCombatAbilityActionResult
{
	GENERATED_BODY()

	/** 全部动作是否通过公共入口成功执行。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Ability") bool bSuccess = false;
	/** 失败时第一条稳定原因标签。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Ability") FGameplayTag FailureTag;
	/** 成功产生同步目标效果或异步 Projectile/Thinker 实例的次数。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Ability") int32 AffectedTargetCount = 0;
};
