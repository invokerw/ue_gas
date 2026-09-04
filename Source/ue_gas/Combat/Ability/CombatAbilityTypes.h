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

/**
 * 决定技能费用或冷却在哪个服务器生命周期阶段正式提交。
 * 在所选阶段之前失败或中断时不会提交；一旦提交，后续中断不会自动退还费用或取消冷却。
 */
UENUM(BlueprintType)
enum class ECombatAbilityCommitStage : uint8
{
	/** 技能通过初始校验、进入施法前摇时立即提交。 */
	CastStarted UMETA(DisplayName="施法开始时"),
	/** 施法前摇结束且目标复核通过、法术即将生效时提交。 */
	SpellStarted UMETA(DisplayName="法术生效时"),
	/** 只有技能正常完成时才提交；此前被打断或取消则不提交。 */
	AbilityEnded UMETA(DisplayName="技能正常结束时")
};

/**
 * 单位目标技能在前摇结束或引导周期重新校验目标失败时的处理方式。
 * 最后已知位置初次取激活时的位置，后续每次校验成功都会更新；丢弃目标后不再追踪，需要单位对象的动作仍无法执行。
 */
UENUM(BlueprintType)
enum class ECombatTargetLostPolicy : uint8
{
	/** 中断本次技能，不执行后续动作。 */
	Fail UMETA(DisplayName="中断技能"),
	/** 丢弃失效单位，保留最近一次校验成功的位置供点或范围动作使用；不是持续追踪目标，也不会让需要单位对象的动作变得合法。 */
	UseLastKnownPoint UMETA(DisplayName="使用最后已知位置")
};

/** 决定引导技能被中断并释放当前施法命令后，OrderComponent 是否继续执行玩家已经排队的后续命令。 */
UENUM(BlueprintType)
enum class ECombatChannelInterruptOrderPolicy : uint8
{
	/** 释放当前施法 Order，保留后续队列。 */
	Continue UMETA(DisplayName="保留后续命令"),
	/** 通知 OrderComponent 清除尚未执行的排队项。 */
	ClearQueuedOrders UMETA(DisplayName="清空后续命令")
};

/**
 * DataDriven Action 通过哪一个公共战斗入口产生效果。
 * 选择类型后只读取该动作所需字段，其余字段会被忽略并由 AbilityData 校验阻止矛盾配置。
 */
UENUM(BlueprintType)
enum class ECombatAbilityActionType : uint8
{
	/** 向公共伤害管线提交请求，统一处理抗性、护盾和实际扣血。 */
	Damage UMETA(DisplayName="造成伤害"),
	/** 向公共治疗管线提交请求，统一处理治疗增幅和最大生命上限。 */
	Heal UMETA(DisplayName="造成治疗"),
	/** 在目标身上施加指定增益或减益，重复施加遵循该效果的叠层和刷新规则。 */
	ApplyModifier UMETA(DisplayName="施加 Modifier"),
	/** 向目标的能力系统发送指定标签的玩法事件，供技能监听者处理。 */
	SendGameplayEvent UMETA(DisplayName="发送 Gameplay Event"),
	/** 生成服务器权威直线 Projectile。 */
	SpawnLinearProjectile UMETA(DisplayName="生成直线弹体"),
	/** 生成服务器权威追踪 Projectile。 */
	SpawnTrackingProjectile UMETA(DisplayName="生成追踪弹体"),
	/** 在固定位置创建区域效果，由调度器控制范围作用的时机和寿命。 */
	CreateThinker UMETA(DisplayName="创建区域 Thinker")
};

/** 决定一条 Action 作用于施法者、服务器确认的单位目标，还是服务器在目标点重新查询到的范围内单位。 */
UENUM(BlueprintType)
enum class ECombatAbilityActionTarget : uint8
{
	/** Action 只作用于施法者。 */
	Caster UMETA(DisplayName="施法者"),
	/** 作用于服务器确认的单位目标；目标已丢失并降级为位置时，需要单位的动作不能执行。 */
	UnitTarget UMETA(DisplayName="单位目标"),
	/** 在服务器确认的位置重新查询半径内的单位，不使用客户端提交的命中列表。 */
	UnitsInRadius UMETA(DisplayName="范围内单位")
};

/**
 * AbilityData 在 SpellStarted 阶段执行的一条服务器动作配置。
 * Type 决定使用哪些字段，Target 决定目标集合，数值键从 AbilityData.SpecialValues 按本次技能等级读取；数组中的动作按配置顺序执行。
 */
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
	/** 区域重复作用间隔的数值键，单位为秒；未设置时只作用一次，重复作用还要求区域寿命为正数。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Ability|Action", meta=(DisplayName="触发间隔参数键", ToolTip="区域重复作用间隔的数值键，单位为秒；未设置时只作用一次，重复作用还要求区域寿命为正数。")) FName IntervalKey;
	/** 启用后把命中效果与拉向发射来源的强制位移参数存入弹体；由命中时施加的效果运行时处理拉拽。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Ability|Action", meta=(DisplayName="命中后拖向来源", ToolTip="启用后把命中效果与拉向发射来源的强制位移参数存入弹体；由命中时施加的效果运行时处理拉拽。")) bool bMotionToSource = false;
	/** 弹体命中后拉拽目标的速度数值键，单位为厘米/秒，用于生成强制位移请求。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Ability|Action", meta=(DisplayName="运动速度参数键", ToolTip="弹体命中后拉拽目标的速度数值键，单位为厘米/秒，用于生成强制位移请求。")) FName MotionSpeedKey;
	/** 拉拽请求竞争目标水平位移通道的优先级；只有严格高于当前请求时才可抢占。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Ability|Action", meta=(DisplayName="运动优先级", ToolTip="拉拽请求竞争目标水平位移通道的优先级；只有严格高于当前请求时才可抢占。")) int32 MotionPriority = 100;
};

/** 一次技能激活的服务器上下文；技能实例保存它，目标位置可随执行点复核更新，等级和生命编号用于隔离后续变化。 */
USTRUCT(BlueprintType)
struct UE_GAS_API FCombatAbilityActivationContext
{
	GENERATED_BODY()

	/** 本次激活对应的根 Combat Event。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Ability") FCombatEventContext EventContext;
	/** 激活来源 Unit。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Ability") TObjectPtr<ACombatUnitCharacter> Caster = nullptr;
	/** 激活时记录的单位目标；执行点和引导周期继续复核，选择失去目标后保留位置时可被清空。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Ability") TObjectPtr<ACombatUnitCharacter> TargetActor = nullptr;
	/** 服务器最近一次目标校验通过的位置，初始来自激活校验；后续复核成功会更新，失去目标降级时保留。 */
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

	/** 是否执行完全部配置动作；首个失败即停止，之前已产生的结果保留，异步对象的后续命中不计入此值。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Ability") bool bSuccess = false;
	/** 失败时第一条稳定原因标签。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Ability") FGameplayTag FailureTag;
	/** 成功产生同步目标效果或异步 Projectile/Thinker 实例的次数。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Ability") int32 AffectedTargetCount = 0;
};
