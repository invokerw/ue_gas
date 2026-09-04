#pragma once

#include "CoreMinimal.h"

#include "Combat/Combat/CombatTransactionTypes.h"
#include "Combat/Core/CombatTypes.h"
#include "Combat/Log/CombatEventSubsystem.h"
#include "Combat/Targeting/CombatTargetingTypes.h"

#include "CombatProjectileTypes.generated.h"

class ACombatProjectileActor;
class ACombatUnitCharacter;
class UCombatModifierData;
class UCombatProjectileData;

/** 决定服务器弹体每帧是沿发射方向直线前进，还是持续朝一个有效目标修正方向。 */
UENUM(BlueprintType)
enum class ECombatProjectileMovementType : uint8
{
	/** 使用生成时记录的方向匀速直线飞行，之后不会追随目标位置。 */
	Linear UMETA(DisplayName="直线运动"),
	/** 每帧朝目标当前权威位置飞行；目标失效时按 TargetLostPolicy 结束或继续。 */
	Tracking UMETA(DisplayName="追踪目标")
};

/** 追踪弹体的目标死亡、换 World 或因生命代次变化而失效时，服务器如何处理尚未命中的弹体。 */
UENUM(BlueprintType)
enum class ECombatProjectileTargetLostPolicy : uint8
{
	/** 立即以 TargetLost 结束，不执行任何命中动作。 */
	Fizzle UMETA(DisplayName="立即消散"),
	/** 继续飞向最后一次有效位置，到点后以目标丢失结束；到达该位置本身不结算命中，途中仍会按命中策略检测单位和阻挡。 */
	UseLastKnownPoint UMETA(DisplayName="飞向最后已知位置")
};

/**
 * 弹体结束时记录的唯一原因，用于日志、攻击收尾和表现回收。
 * 每个权威弹体只完成一次；后续碰撞、取消或 EndPlay 不会再次派发完成结果。
 */
UENUM(BlueprintType)
enum class ECombatProjectileFinishReason : uint8
{
	/** 配置为命中首个单位后停止的弹体碰到合格单位并结束；是否实际扣血由伤害结果决定。 */
	Hit,
	/** 命中 WorldStatic、WorldDynamic 或 CombatBlocker。 */
	Blocked,
	/** 达到最大飞行距离。 */
	MaxDistance,
	/** 达到最大生存时间。 */
	Timeout,
	/** Tracking 目标失效或到达最后已知位置。 */
	TargetLost,
	/** 技能或其他服务器调用者明确请求停止弹体。 */
	Cancelled,
	/** 权威 Actor 或 World 正在结束。 */
	EndPlay
};

/** 选择弹体命中合法单位后，是通过公共伤害管线造成伤害，还是通过目标的 ModifierComponent 施加效果。 */
UENUM(BlueprintType)
enum class ECombatProjectileImpactActionType : uint8
{
	/** 通过 DamageSubsystem 造成伤害。 */
	Damage UMETA(DisplayName="造成伤害"),
	/** 通过 ModifierComponent 施加 Modifier。 */
	ApplyModifier UMETA(DisplayName="施加 Modifier")
};

/**
 * 配置弹体 sweep 遇到单位或世界阻挡时哪些对象算合法命中，以及命中后是否继续飞行。
 * 策略在弹体生成时复制到 Spec，飞行过程中不会随 DataAsset 修改。
 */
USTRUCT(BlueprintType)
struct UE_GAS_API FCombatProjectileHitPolicy
{
	GENERATED_BODY()

	/** 是否允许命中敌对单位。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat|Projectile|Hit", meta=(DisplayName="可命中敌方", ToolTip="启用后，弹体 sweep 可以把与来源队伍关系为 Hostile 的单位判定为合法命中。")) bool bHitHostile = true;
	/** 是否允许命中友方单位。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat|Projectile|Hit", meta=(DisplayName="可命中友方", ToolTip="启用后，弹体 sweep 可以把与来源队伍关系为 Friendly 的单位判定为合法命中。")) bool bHitFriendly = false;
	/** 是否允许命中 Source 自身；默认始终忽略。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat|Projectile|Hit", meta=(DisplayName="可命中来源自身", ToolTip="启用后允许弹体命中发射它的来源单位；关闭时始终忽略来源自身。")) bool bHitSelf = false;
	/** 第一个合法 Unit 命中后是否结束 Projectile。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat|Projectile|Hit", meta=(DisplayName="首个单位命中后结束", ToolTip="启用后，命中第一个合法单位并执行动作后结束弹体；关闭时允许继续穿透并命中其他单位。")) bool bDestroyOnFirstUnitHit = true;
	/** WorldStatic、WorldDynamic 或 CombatBlocker 是否结束 Projectile。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat|Projectile|Hit", meta=(DisplayName="世界阻挡时结束", ToolTip="启用后，命中 WorldStatic、WorldDynamic 或 CombatBlocker 时结束弹体。")) bool bStopOnWorld = true;
};

/**
 * 弹体命中合格单位后执行的一项伤害或效果动作，多个动作按数组顺序处理。
 * 可选拉拽只随效果施加传入强制位移请求，目的地取命中时来源单位的位置，位移开始后不继续追随来源。
 */
USTRUCT(BlueprintType)
struct UE_GAS_API FCombatProjectileImpactAction
{
	GENERATED_BODY()

	/** 选择 Damage 或 ApplyModifier 公共入口。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat|Projectile|Impact", meta=(DisplayName="命中动作类型", ToolTip="选择命中后通过公共伤害入口造成伤害，或通过 ModifierComponent 施加 Modifier。")) ECombatProjectileImpactActionType Type = ECombatProjectileImpactActionType::Damage;
	/** Damage 动作使用的非负值。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat|Projectile|Impact", meta=(ClampMin="0", DisplayName="伤害数值", ToolTip="Damage 命中动作提交给 DamageSubsystem 的非负伤害值；ApplyModifier 动作会忽略此字段。")) float Magnitude = 0.0f;
	/** Damage 动作使用的类型。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat|Projectile|Impact", meta=(DisplayName="伤害类型", ToolTip="Damage 命中动作使用的物理、魔法或纯粹结算分支。")) ECombatDamageType DamageType = ECombatDamageType::Magical;
	/** ApplyModifier 动作使用的稳定定义。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat|Projectile|Impact", meta=(DisplayName="Modifier 定义", ToolTip="ApplyModifier 命中动作施加的稳定 ModifierData；该动作类型下不能为空。")) TObjectPtr<UCombatModifierData> ModifierData = nullptr;
	/** 命中效果的持续秒数：[-1,0) 使用效果定义，0 为无限，正数覆盖定义；小于 -1 非法，负面效果可能再受状态抗性缩短。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat|Projectile|Impact", meta=(ClampMin="-1", Units="s", DisplayName="持续时间覆盖", ToolTip="命中效果的持续秒数：[-1,0) 使用效果定义，0 为无限，正数覆盖定义；小于 -1 非法，负面效果可能再受状态抗性缩短。")) float DurationOverride = -1.0f;
	/** 启用后在命中时记录来源单位的位置，随新建效果传入拉向该位置的强制位移请求；需效果运行时的创建回调实际消费。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat|Projectile|Impact", meta=(DisplayName="拖向来源位置", ToolTip="启用后在命中时记录来源单位的位置，随新建效果传入拉向该位置的强制位移请求；需效果运行时的创建回调实际消费。")) bool bMotionToSource = false;
	/** 拉拽速度，单位为厘米/秒；仅在启用拉向来源且效果实际发起位移时使用，位移入口要求速度大于 0。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat|Projectile|Impact", meta=(ClampMin="0", Units="cm/s", DisplayName="运动速度", ToolTip="拉拽速度，单位为厘米/秒；仅在启用拉向来源且效果实际发起位移时使用，位移入口要求速度大于 0。")) float MotionSpeed = 0.0f;
	/** 拉拽竞争目标水平位移通道的优先级，必须严格高于当前占用者才可抢占。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat|Projectile|Impact", meta=(DisplayName="运动优先级", ToolTip="拉拽竞争目标水平位移通道的优先级，必须严格高于当前占用者才可抢占。")) int32 MotionPriority = 100;
};

/**
 * 服务器生成弹体时冻结的完整运行参数，包括来源、目标、运动、命中动作和事件身份。
 * 生成后弹体只读取该 Spec，不再依赖 Ability 实例；因此 Ability 结束不会改变已发射弹体，除非显式启用按 ActivationId 取消。
 */
USTRUCT(BlueprintType)
struct UE_GAS_API FCombatProjectileSpec
{
	GENERATED_BODY()

	/** 必填的弹体定义，提供 Actor、速度、半径、飞行上限和碰撞配置；直接调用生成接口时，运动类型和命中策略仍取本请求相应字段。 */
	UPROPERTY(BlueprintReadWrite, Category="Combat|Projectile") TObjectPtr<UCombatProjectileData> ProjectileData = nullptr;
	/** 发射来源，必须有效且属于本世界服务器；来源 Actor 销毁后，弹体在后续推进中结束。 */
	UPROPERTY(BlueprintReadWrite, Category="Combat|Projectile") TObjectPtr<ACombatUnitCharacter> Source = nullptr;
	/** 发射时由子系统写入来源当前队伍，用于后续命中筛选；调用者填写的值会被覆盖，来源换队不改写已发射快照。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Projectile") FCombatTeamId SourceTeam;
	/** 追踪弹体或普攻弹体的单位目标；追踪创建要求有效且同世界，普攻命中只能交回原攻击目标。 */
	UPROPERTY(BlueprintReadWrite, Category="Combat|Projectile") TObjectPtr<ACombatUnitCharacter> Target = nullptr;
	/** 服务器发射位置的世界坐标，单位为厘米，必须是有限向量。 */
	UPROPERTY(BlueprintReadWrite, Category="Combat|Projectile") FVector SpawnLocation = FVector::ZeroVector;
	/** 直线弹体的初始方向，生成时归一化；零向量回退到世界正 X 方向，非有限向量拒绝。 */
	UPROPERTY(BlueprintReadWrite, Category="Combat|Projectile") FVector Direction = FVector::ForwardVector;
	/** 运动类型。 */
	UPROPERTY(BlueprintReadWrite, Category="Combat|Projectile") ECombatProjectileMovementType MovementType = ECombatProjectileMovementType::Linear;
	/** Tracking 失效策略。 */
	UPROPERTY(BlueprintReadWrite, Category="Combat|Projectile") ECombatProjectileTargetLostPolicy TargetLostPolicy = ECombatProjectileTargetLostPolicy::Fizzle;
	/** 阵营、穿透与阻挡策略快照。 */
	UPROPERTY(BlueprintReadWrite, Category="Combat|Projectile") FCombatProjectileHitPolicy HitPolicy;
	/** 飞行速度覆盖，单位为厘米/秒；有限负数使用定义默认速度，正数覆盖，0 会因最终速度必须大于 0 而拒绝生成。 */
	UPROPERTY(BlueprintReadWrite, Category="Combat|Projectile") float SpeedOverride = -1.0f;
	/** 碰撞半径覆盖，单位为厘米；有限负数使用定义值，非负值覆盖，实际球形扫掠半径至少为 0.1 厘米。 */
	UPROPERTY(BlueprintReadWrite, Category="Combat|Projectile") float RadiusOverride = -1.0f;
	/** 累计飞行距离上限覆盖，单位为厘米；有限负数使用定义值，正数覆盖，0 会因最终距离必须大于 0 而拒绝生成。 */
	UPROPERTY(BlueprintReadWrite, Category="Combat|Projectile") float MaxDistanceOverride = -1.0f;
	/** 每次命中合格单位时按数组顺序执行的动作副本；普攻弹体使用 AttackHandle 的攻击结算，不执行这组动作。 */
	UPROPERTY(BlueprintReadWrite, Category="Combat|Projectile") TArray<FCombatProjectileImpactAction> ImpactActions;
	/** 弹体所属的技能或攻击事件上下文，后续伤害继承事件链；未提供时在生成阶段创建根事件。 */
	UPROPERTY(BlueprintReadWrite, Category="Combat|Projectile") FCombatEventContext ParentEvent;
	/** 完整 Ability/Modifier/Projectile 来源链。 */
	UPROPERTY(BlueprintReadWrite, Category="Combat|Projectile") FCombatSourceContext SourceContext;
	/** 普攻弹体关联的唯一攻击记录；有效时命中交回攻击组件，闪避、暴击和主伤害在那里结算。 */
	UPROPERTY(BlueprintReadWrite, Category="Combat|Projectile") FCombatAttackHandle AttackHandle;
	/** 创建弹体的技能激活编号；与来源单位及取消选项一起用于技能结束时筛选弹体。 */
	UPROPERTY(BlueprintReadWrite, Category="Combat|Projectile") FCombatEventId AbilityActivationId;
	/** 启用后，来源技能的清理入口可按激活编号取消弹体；默认关闭，弹体在技能结束后继续按自身规则飞行。 */
	UPROPERTY(BlueprintReadWrite, Category="Combat|Projectile") bool bCancelWithSourceAbility = false;
	/** 可选客户端预测视觉键；只用于表现 reconcile，不参与命中和伤害。 */
	UPROPERTY(BlueprintReadWrite, Category="Combat|Projectile", meta=(DisplayName="预测视觉键", ToolTip="大于 0 时用于服务器弹体首次复制后替换同键客户端预测视觉；不参与 gameplay。"))
	int32 PredictionKey = 0;
};

/** 弹体生成或结束操作的结果；结束时包含唯一 FinishReason、命中对象和实际造成的生命减少量。 */
USTRUCT(BlueprintType)
struct UE_GAS_API FCombatProjectileResult
{
	GENERATED_BODY()

	/** 生成时表示弹体已创建；结束时表示成功结束了活动记录，因超时、阻挡或取消结束也可能为 true，不等于造成伤害。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Projectile") bool bSuccess = false;
	/** 对应 ProjectileHandle。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Projectile") FCombatProjectileHandle Handle;
	/** Finish 分类；Spawn 成功时保持默认。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Projectile") ECombatProjectileFinishReason FinishReason = ECombatProjectileFinishReason::Cancelled;
	/** 使弹体最终结束的单位或阻挡 Actor；超时、距离耗尽、取消及生成结果为空，不保留此前穿透命中的目标。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Projectile") TObjectPtr<AActor> HitActor = nullptr;
	/** 导致弹体以命中结束的那次结算所造成的实际扣血合计；其他结束原因和生成结果为 0，不累计此前穿透命中的伤害。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Projectile") float AppliedDamage = 0.0f;
	/** 失败或结束的稳定原因。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Projectile") FGameplayTag FailureTag;
};

/** 弹体最终结束的服务器本地通知；不用于生成通知或穿透途中逐次命中，后两者有各自日志事件。 */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnCombatProjectileFinished, const FCombatProjectileResult&);
