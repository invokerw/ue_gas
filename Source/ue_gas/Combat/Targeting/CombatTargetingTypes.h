#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

#include "CombatTargetingTypes.generated.h"

class ACombatUnitCharacter;

/**
 * 决定服务器目标校验是否还要求“战争迷雾中可见”。这与 bRequireLineOfSight 的几何遮挡检查是两项独立规则。
 * 当前版本没有服务器 VisionProvider，因此只能使用 None；选择 RequireVisible 会明确校验失败，而不是信任客户端视野。
 */
UENUM(BlueprintType)
enum class ECombatVisibilityPolicy : uint8
{
	/** 不做战争迷雾判定，仍可独立开启几何 LOS。 */
	None UMETA(DisplayName="不检查可见性"),
	/** 为权威 VisionProvider 预留；当前请求该策略会明确返回 Unsupported。 */
	RequireVisible UMETA(DisplayName="要求权威可见")
};

/**
 * 一组由 UI 提示、Order 预检和 Ability 最终校验共用的目标限制。
 * 客户端可以用它显示可选目标，但服务器会重新检查阵营、生命状态、状态标签、距离和视线；所有启用的条件都通过后目标才有效。
 */
USTRUCT(BlueprintType)
struct UE_GAS_API FCombatTargetingRules
{
	GENERATED_BODY()

	/** UnitTarget 和服务器范围查询使用 Friendly、Enemy 或 Both；NoTarget 必须使用 None，不查询单位的点目标动作会忽略它。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Targeting", meta=(Categories="TargetTeam", DisplayName="目标阵营规则", ToolTip="使用 TargetTeam 下的 Friendly、Enemy、Both 或 None 标签声明允许的目标关系。")) FGameplayTag TargetTeamTag;
	/** 是否允许把来源 Unit 自身作为单位目标。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Targeting", meta=(DisplayName="允许选择自身", ToolTip="启用后，来源单位自身可以作为单位目标；仍需满足其他状态和阵营规则。")) bool bAllowSelf = false;
	/** 允许被队伍关系表显式标为中立的目标；开启后，中立关系可直接通过阵营检查，不要求匹配 TargetTeamTag。其他生命、状态和距离规则仍须通过。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Targeting", meta=(DisplayName="允许中立关系", ToolTip="允许被队伍关系表显式标为中立的目标；开启后，中立关系可直接通过阵营检查，不要求匹配 TargetTeamTag。其他生命、状态和距离规则仍须通过。")) bool bAllowNeutralRelation = false;
	/** Dead 目标是否可被选择；Dying 与 Respawning 始终拒绝。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Targeting", meta=(DisplayName="允许死亡目标", ToolTip="启用后允许选择 Dead 单位；Dying 与 Respawning 状态始终被拒绝。")) bool bAllowDead = false;
	/** 是否绕过 State.Untargetable 的目标校验。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Targeting", meta=(DisplayName="允许不可选中目标", ToolTip="启用后绕过 State.Untargetable 校验，适用于明确允许命中不可选中单位的规则。")) bool bAllowUntargetable = false;
	/** 是否允许 State.Invulnerable；实际 Damage 仍由 DamageSubsystem 决定。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Targeting", meta=(DisplayName="允许无敌目标", ToolTip="启用后 State.Invulnerable 不阻止选中；实际伤害是否生效仍由 DamageSubsystem 决定。")) bool bAllowInvulnerable = false;
	/** 是否允许 State.MagicImmune；魔法伤害仍会在 DamageSubsystem 被阻挡。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Targeting", meta=(DisplayName="允许魔免目标", ToolTip="启用后 State.MagicImmune 不阻止选中；魔法伤害仍会在 DamageSubsystem 中被阻挡。")) bool bAllowMagicImmune = true;
	/** 基础施法边缘距离，单位为厘米，必须有限且非负；单位/点校验会加上来源的施法距离加成与容差。范围查询不使用此距离限制。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Targeting", meta=(ClampMin="0", Units="cm", DisplayName="基础施法范围", ToolTip="基础施法边缘距离，单位为厘米，必须有限且非负；单位/点校验会加上来源的施法距离加成与容差。范围查询不使用此距离限制。")) float CastRange = 0.0f;
	/** 每次目标校验时是否用 CombatTargeting 碰撞通道检查来源到目标的几何遮挡；也作用于范围查询，不只在技能生效时检查，与战争迷雾可见性独立。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Targeting", meta=(DisplayName="需要几何视线", ToolTip="每次目标校验时是否用 CombatTargeting 碰撞通道检查来源到目标的几何遮挡；也作用于范围查询，不只在技能生效时检查，与战争迷雾可见性独立。")) bool bRequireLineOfSight = false;
	/** 当前只支持 None；保留该字段以避免把客户端可见性误作权威。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Targeting", meta=(DisplayName="战争迷雾可见性", ToolTip="选择是否要求服务器权威可见性；当前版本只支持“不检查可见性”。")) ECombatVisibilityPolicy VisibilityPolicy = ECombatVisibilityPolicy::None;
};

/**
 * 客户端随技能请求提交的最小目标意图，只包含单位或世界坐标。
 * 服务器不会信任客户端命中结果，会用 FCombatTargetingRules 从当前权威状态重新校验并查询范围目标。
 */
USTRUCT(BlueprintType)
struct UE_GAS_API FCombatAbilityTargetData
{
	GENERATED_BODY()

	/** UnitTarget 模式提交的 Actor；服务器重新验证其 World、状态和关系。 */
	UPROPERTY(BlueprintReadWrite, Category="Combat|Targeting") TObjectPtr<ACombatUnitCharacter> TargetActor = nullptr;
	/** PointTarget 模式提交的世界坐标。 */
	UPROPERTY(BlueprintReadWrite, Category="Combat|Targeting") FVector TargetLocation = FVector::ZeroVector;
	/** 区分合法原点与没有提交 Point。 */
	UPROPERTY(BlueprintReadWrite, Category="Combat|Targeting") bool bHasTargetLocation = false;
	/** 仅用于显式识别伪造；任何非空客户端命中列表都会被服务器拒绝。 */
	UPROPERTY(BlueprintReadWrite, Category="Combat|Targeting") TArray<TObjectPtr<ACombatUnitCharacter>> ClientClaimedHitActors;
};

/** 服务器目标校验的结果：成功时提供最终采用的位置，失败时提供稳定原因标签和诊断文本。 */
USTRUCT(BlueprintType)
struct UE_GAS_API FCombatTargetValidationResult
{
	GENERATED_BODY()

	/** 当前请求是否满足全部目标规则。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Targeting") bool bValid = false;
	/** 失败时的稳定 Native GameplayTag。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Targeting") FGameplayTag FailureTag;
	/** 服务器最终接受的位置；UnitTarget 时为目标当前位置。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Targeting") FVector AuthoritativeLocation = FVector::ZeroVector;
	/** 面向日志和测试的补充文本。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Targeting") FString Diagnostic;
};
