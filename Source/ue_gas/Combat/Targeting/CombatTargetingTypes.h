#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

#include "CombatTargetingTypes.generated.h"

class ACombatUnitCharacter;

/** 定义目标校验是否需要额外的战争迷雾可见性信息。 */
UENUM(BlueprintType)
enum class ECombatVisibilityPolicy : uint8
{
	/** M3 基线：不做战争迷雾判定，仍可独立开启几何 LOS。 */
	None,
	/** 为未来 VisionProvider 预留；M3 请求该策略会明确返回 Unsupported。 */
	RequireVisible
};

/** UI、Order 与 Ability 共用的一组服务器目标规则。 */
USTRUCT(BlueprintType)
struct UE_GAS_API FCombatTargetingRules
{
	GENERATED_BODY()

	/** Friendly、Enemy、Both 或 NoTarget 使用的 None。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Targeting") FGameplayTag TargetTeamTag;
	/** 是否允许把来源 Unit 自身作为单位目标。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Targeting") bool bAllowSelf = false;
	/** 显式 Neutral relation 是否可被选择。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Targeting") bool bAllowNeutralRelation = false;
	/** Dead 目标是否可被选择；Dying 与 Respawning 始终拒绝。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Targeting") bool bAllowDead = false;
	/** 是否绕过 State.Untargetable 的目标校验。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Targeting") bool bAllowUntargetable = false;
	/** 是否允许 State.Invulnerable；实际 Damage 仍由 DamageSubsystem 决定。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Targeting") bool bAllowInvulnerable = false;
	/** 是否允许 State.MagicImmune；魔法伤害仍会在 DamageSubsystem 被阻挡。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Targeting") bool bAllowMagicImmune = true;
	/** 基础施法范围，运行时再叠加来源 CastRangeBonus，单位为厘米。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Targeting", meta=(ClampMin="0", Units="cm")) float CastRange = 0.0f;
	/** cast point 是否使用 CombatTargeting trace channel 重新检查几何视线。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Targeting") bool bRequireLineOfSight = false;
	/** M3 只支持 None，保留该字段避免未来把客户端可见性误作权威。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Targeting") ECombatVisibilityPolicy VisibilityPolicy = ECombatVisibilityPolicy::None;
};

/** 客户端可提交给服务器复核的最小 TargetData。 */
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
	/** 仅用于显式识别伪造；M3 任何非空客户端命中列表都会被服务器拒绝。 */
	UPROPERTY(BlueprintReadWrite, Category="Combat|Targeting") TArray<TObjectPtr<ACombatUnitCharacter>> ClientClaimedHitActors;
};

/** Target Filter 的结构化成功、失败和服务器修正位置。 */
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
