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
	/** 继续飞向最后一次有效位置；到达后以 TargetLost 结束，仍不执行命中动作。 */
	UseLastKnownPoint UMETA(DisplayName="飞向最后已知位置")
};

/**
 * 弹体结束时记录的唯一原因，用于日志、攻击收尾和表现回收。
 * 每个权威弹体只完成一次；后续碰撞、取消或 EndPlay 不会再次派发完成结果。
 */
UENUM(BlueprintType)
enum class ECombatProjectileFinishReason : uint8
{
	/** first-hit Projectile 成功命中单位。 */
	Hit,
	/** 命中 WorldStatic、WorldDynamic 或 CombatBlocker。 */
	Blocked,
	/** 达到最大飞行距离。 */
	MaxDistance,
	/** 达到最大生存时间。 */
	Timeout,
	/** Tracking 目标失效或到达最后已知位置。 */
	TargetLost,
	/** Ability/source 显式取消。 */
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
 * 弹体命中一个合法单位后按数组顺序执行的一项效果。
 * Type 决定 Damage 字段或 Modifier 字段哪一组生效；可选 Motion 只随 ApplyModifier 一起把目标拖向发射时记录的来源位置。
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
	/** Modifier 持续时间覆盖；小于 0 使用定义值。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat|Projectile|Impact", meta=(ClampMin="-1", Units="s", DisplayName="持续时间覆盖", ToolTip="ApplyModifier 的持续时间覆盖，单位为秒；小于 0 表示使用 ModifierData 定义值，0 表示无限持续，正数表示指定时长。")) float DurationOverride = -1.0f;
	/** true 时为 Modifier Runtime 注入“拖向 Source 快照位置”的 Motion 请求。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat|Projectile|Impact", meta=(DisplayName="拖向来源位置", ToolTip="启用后，为命中的 Modifier Runtime 注入拖向弹体来源快照位置的 Motion 请求。")) bool bMotionToSource = false;
	/** Hook Motion 的快照速度。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat|Projectile|Impact", meta=(ClampMin="0", Units="cm/s", DisplayName="运动速度", ToolTip="拖向来源位置时使用的 Hook Motion 速度，单位为厘米/秒；启用运动时必须大于 0。")) float MotionSpeed = 0.0f;
	/** Hook Motion 抢占优先级。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat|Projectile|Impact", meta=(DisplayName="运动优先级", ToolTip="Hook Motion 申请水平运动通道时使用的抢占优先级；数值越大越容易抢占。")) int32 MotionPriority = 100;
};

/**
 * 服务器生成弹体时冻结的完整运行参数，包括来源、目标、运动、命中动作和事件身份。
 * 生成后弹体只读取该 Spec，不再依赖 Ability 实例；因此 Ability 结束不会改变已发射弹体，除非显式启用按 ActivationId 取消。
 */
USTRUCT(BlueprintType)
struct UE_GAS_API FCombatProjectileSpec
{
	GENERATED_BODY()

	/** 运动/碰撞默认值来源；registry 在 Spawn 时复制数值。 */
	UPROPERTY(BlueprintReadWrite, Category="Combat|Projectile") TObjectPtr<UCombatProjectileData> ProjectileData = nullptr;
	/** 发射 Projectile 的权威 Unit。 */
	UPROPERTY(BlueprintReadWrite, Category="Combat|Projectile") TObjectPtr<ACombatUnitCharacter> Source = nullptr;
	/** Spawn 时由 Subsystem 覆盖的来源队伍快照。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Projectile") FCombatTeamId SourceTeam;
	/** Tracking 或 Attack Projectile 的目标。 */
	UPROPERTY(BlueprintReadWrite, Category="Combat|Projectile") TObjectPtr<ACombatUnitCharacter> Target = nullptr;
	/** 权威起点。 */
	UPROPERTY(BlueprintReadWrite, Category="Combat|Projectile") FVector SpawnLocation = FVector::ZeroVector;
	/** Linear 方向；Spawn 时规范化。 */
	UPROPERTY(BlueprintReadWrite, Category="Combat|Projectile") FVector Direction = FVector::ForwardVector;
	/** 运动类型。 */
	UPROPERTY(BlueprintReadWrite, Category="Combat|Projectile") ECombatProjectileMovementType MovementType = ECombatProjectileMovementType::Linear;
	/** Tracking 失效策略。 */
	UPROPERTY(BlueprintReadWrite, Category="Combat|Projectile") ECombatProjectileTargetLostPolicy TargetLostPolicy = ECombatProjectileTargetLostPolicy::Fizzle;
	/** 阵营、穿透与阻挡策略快照。 */
	UPROPERTY(BlueprintReadWrite, Category="Combat|Projectile") FCombatProjectileHitPolicy HitPolicy;
	/** 非负时覆盖 ProjectileData.Speed；用于按技能等级冻结速度。 */
	UPROPERTY(BlueprintReadWrite, Category="Combat|Projectile") float SpeedOverride = -1.0f;
	/** 非负时覆盖 ProjectileData.Radius；用于按技能等级冻结宽度。 */
	UPROPERTY(BlueprintReadWrite, Category="Combat|Projectile") float RadiusOverride = -1.0f;
	/** 非负时覆盖 ProjectileData.MaxDistance；用于按技能等级冻结射程。 */
	UPROPERTY(BlueprintReadWrite, Category="Combat|Projectile") float MaxDistanceOverride = -1.0f;
	/** 按顺序执行的不可变 Impact Action。 */
	UPROPERTY(BlueprintReadWrite, Category="Combat|Projectile") TArray<FCombatProjectileImpactAction> ImpactActions;
	/** Ability/Attack 根事件；每次 Damage 派生子事件。 */
	UPROPERTY(BlueprintReadWrite, Category="Combat|Projectile") FCombatEventContext ParentEvent;
	/** 完整 Ability/Modifier/Projectile 来源链。 */
	UPROPERTY(BlueprintReadWrite, Category="Combat|Projectile") FCombatSourceContext SourceContext;
	/** Attack Projectile 使用的唯一 AttackRecord 句柄。 */
	UPROPERTY(BlueprintReadWrite, Category="Combat|Projectile") FCombatAttackHandle AttackHandle;
	/** Ability 激活根 ID，供 cancel-with-source 批量取消。 */
	UPROPERTY(BlueprintReadWrite, Category="Combat|Projectile") FCombatEventId AbilityActivationId;
	/** true 时 Ability End 可按 ActivationId 取消；默认 fire-and-forget。 */
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

	/** Spawn 是否接受，或 Finish 是否来自合法活动记录。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Projectile") bool bSuccess = false;
	/** 对应 ProjectileHandle。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Projectile") FCombatProjectileHandle Handle;
	/** Finish 分类；Spawn 成功时保持默认。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Projectile") ECombatProjectileFinishReason FinishReason = ECombatProjectileFinishReason::Cancelled;
	/** 首个命中或最终阻挡 Actor。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Projectile") TObjectPtr<AActor> HitActor = nullptr;
	/** Impact Action 真实造成的总生命减少。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Projectile") float AppliedDamage = 0.0f;
	/** 失败或结束的稳定原因。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Projectile") FGameplayTag FailureTag;
};

/** Projectile Spawn/Hit/Finish 的原生观察委托。 */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnCombatProjectileFinished, const FCombatProjectileResult&);
