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
	CastStarted UMETA(DisplayName="施法开始时"),
	/** cast point 完成、目标复核通过后提交。 */
	SpellStarted UMETA(DisplayName="法术生效时"),
	/** Ability 正常结束前提交；中断不提交。 */
	AbilityEnded UMETA(DisplayName="技能正常结束时")
};

/** UnitTarget 在 cast point 丢失时采用的处理规则。 */
UENUM(BlueprintType)
enum class ECombatTargetLostPolicy : uint8
{
	/** 目标失效立即中断 Ability。 */
	Fail UMETA(DisplayName="中断技能"),
	/** 仅允许点/AoE 行为继续使用激活时的最后已知位置。 */
	UseLastKnownPoint UMETA(DisplayName="使用最后已知位置")
};

/** 引导被中断后 OrderComponent 应如何处理后续队列。 */
UENUM(BlueprintType)
enum class ECombatChannelInterruptOrderPolicy : uint8
{
	/** 释放当前施法 Order，保留后续队列。 */
	Continue UMETA(DisplayName="保留后续命令"),
	/** 通知 OrderComponent 清除尚未执行的排队项。 */
	ClearQueuedOrders UMETA(DisplayName="清空后续命令")
};

/** DataDriven Action schema v1 的全部动作类型。 */
UENUM(BlueprintType)
enum class ECombatAbilityActionType : uint8
{
	/** 调用 CombatDamageSubsystem。 */
	Damage UMETA(DisplayName="造成伤害"),
	/** 调用 CombatHealSubsystem。 */
	Heal UMETA(DisplayName="造成治疗"),
	/** 调用目标 ModifierComponent。 */
	ApplyModifier UMETA(DisplayName="施加 Modifier"),
	/** 向目标 ASC 发送 GameplayEvent。 */
	SendGameplayEvent UMETA(DisplayName="发送 Gameplay Event"),
	/** 生成服务器权威直线 Projectile。 */
	SpawnLinearProjectile UMETA(DisplayName="生成直线弹体"),
	/** 生成服务器权威追踪 Projectile。 */
	SpawnTrackingProjectile UMETA(DisplayName="生成追踪弹体"),
	/** 创建由 Combat Scheduler 驱动的区域 Thinker。 */
	CreateThinker UMETA(DisplayName="创建区域 Thinker")
};

/** 声明 Action 应使用施法者、单位目标还是服务器 AoE 查询结果。 */
UENUM(BlueprintType)
enum class ECombatAbilityActionTarget : uint8
{
	/** Action 只作用于施法者。 */
	Caster UMETA(DisplayName="施法者"),
	/** Action 作用于权威 UnitTarget 快照。 */
	UnitTarget UMETA(DisplayName="单位目标"),
	/** Action 在权威点位置执行服务器半径查询。 */
	UnitsInRadius UMETA(DisplayName="范围内单位")
};

/** AbilityData 中一条可由公共服务器入口执行的动作。 */
USTRUCT(BlueprintType)
struct UE_GAS_API FCombatAbilityAction
{
	GENERATED_BODY()

	/** 选择 Damage、Heal、Modifier、Event、Projectile 或 Thinker 动作。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Ability|Action", meta=(DisplayName="动作类型", ToolTip="选择本条动作通过哪个公共服务器入口执行；其他字段是否生效取决于该类型。")) ECombatAbilityActionType Type = ECombatAbilityActionType::Damage;
	/** 选择本动作的权威目标集合。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Ability|Action", meta=(DisplayName="动作目标", ToolTip="选择施法者、权威单位目标或服务器半径查询结果作为本动作的目标集合。")) ECombatAbilityActionTarget Target = ECombatAbilityActionTarget::UnitTarget;
	/** 从 AbilityData.SpecialValues 按当前 Spec.Level 读取的数值键。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Ability|Action", meta=(DisplayName="数值参数键", ToolTip="从 AbilityData.SpecialValues 按当前技能等级读取主数值；伤害、治疗、弹体和 Thinker 动作必须引用已有键。")) FName MagnitudeKey;
	/** Damage Action 使用的物理、魔法或纯粹类型。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Ability|Action", meta=(DisplayName="伤害类型", ToolTip="Damage 或 Projectile 伤害使用的物理、魔法或纯粹结算分支。")) ECombatDamageType DamageType = ECombatDamageType::Magical;
	/** ApplyModifier Action 使用的稳定 Modifier 定义。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Ability|Action", meta=(DisplayName="Modifier 定义", ToolTip="ApplyModifier 动作施加的定义；弹体动作也可在命中后施加该 Modifier，其他动作会忽略此字段。")) TObjectPtr<UCombatModifierData> ModifierData = nullptr;
	/** UnitsInRadius 从 SpecialValues 读取服务器查询半径的键。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Ability|Action", meta=(DisplayName="半径参数键", ToolTip="从 SpecialValues 读取服务器查询或弹体半径；范围目标和 Thinker 必填，弹体中为可选覆盖，单位为厘米。")) FName RadiusKey;
	/** SendGameplayEvent 使用的事件标签。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Ability|Action", meta=(DisplayName="Gameplay Event 标签", ToolTip="SendGameplayEvent 动作发送到目标 ASC 的事件标签；该动作类型下不能为空。")) FGameplayTag EventTag;
	/** SpawnLinear/TrackingProjectile 使用的稳定 ProjectileData。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Ability|Action", meta=(DisplayName="弹体定义", ToolTip="直线或追踪弹体动作使用的 ProjectileData；弹体动作类型下不能为空。")) TObjectPtr<UCombatProjectileData> ProjectileData = nullptr;
	/** Projectile 从 SpecialValues 读取速度覆盖的可选键。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Ability|Action", meta=(DisplayName="弹体速度参数键", ToolTip="可选；从 SpecialValues 读取弹体速度覆盖，单位为厘米/秒；None 表示使用 ProjectileData 的速度。")) FName ProjectileSpeedKey;
	/** Projectile 从 SpecialValues 读取最大距离覆盖的可选键。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Ability|Action", meta=(DisplayName="弹体距离参数键", ToolTip="可选；从 SpecialValues 读取最大飞行距离覆盖，单位为厘米；None 表示使用 ProjectileData 的距离。")) FName ProjectileRangeKey;
	/** CreateThinker 从 SpecialValues 读取持续时间的键。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Ability|Action", meta=(DisplayName="持续时间参数键", ToolTip="CreateThinker 从 SpecialValues 读取持续时间的键，单位为秒；Thinker 动作下必须引用已有键。")) FName DurationKey;
	/** CreateThinker 从 SpecialValues 读取 pulse 间隔的键；None 表示只 pulse 一次。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Ability|Action", meta=(DisplayName="触发间隔参数键", ToolTip="CreateThinker 从 SpecialValues 读取周期触发间隔的可选键，单位为秒；None 表示只触发一次。")) FName IntervalKey;
	/** Projectile 命中后是否把 Modifier 与拖向 Source 的 Motion 请求一起快照。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Ability|Action", meta=(DisplayName="命中后拖向来源", ToolTip="仅弹体 Modifier 命中动作使用；启用后将目标拖向发射时快照的来源位置，并要求配置 Modifier 与运动速度键。")) bool bMotionToSource = false;
	/** Hook Motion 从 SpecialValues 读取速度的键。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Ability|Action", meta=(DisplayName="运动速度参数键", ToolTip="从 SpecialValues 读取 Hook Motion 速度，单位为厘米/秒；启用命中后拖向来源时必须引用正数值。")) FName MotionSpeedKey;
	/** Hook Motion 获取水平通道时使用的优先级。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Ability|Action", meta=(DisplayName="运动优先级", ToolTip="Hook Motion 申请水平运动通道时使用的抢占优先级；数值越大越容易抢占。")) int32 MotionPriority = 100;
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
