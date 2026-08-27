#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

#include "Combat/Combat/CombatTransactionTypes.h"
#include "Combat/Core/CombatTypes.h"
#include "Combat/Log/CombatEventSubsystem.h"
#include "Combat/Targeting/CombatTargetingTypes.h"

#include "CombatThinkerTypes.generated.h"

class ACombatUnitCharacter;
class UCombatModifierData;

/** Thinker exactly-once 结束分类。 */
UENUM(BlueprintType)
enum class ECombatThinkerFinishReason : uint8
{
	/** Duration 到期或一次性 pulse 完成。 */
	Completed,
	/** 显式取消。 */
	Cancelled,
	/** Actor/World 正在结束。 */
	EndPlay
};

/** Spawn 时冻结的服务器 AoE pulse Spec。 */
USTRUCT(BlueprintType)
struct UE_GAS_API FCombatThinkerSpec
{
	GENERATED_BODY()

	/** 创建 Thinker 的权威来源 Unit。 */
	UPROPERTY(BlueprintReadWrite, Category="Combat|Thinker") TObjectPtr<ACombatUnitCharacter> Source = nullptr;
	/** AoE 中心。 */
	UPROPERTY(BlueprintReadWrite, Category="Combat|Thinker") FVector Location = FVector::ZeroVector;
	/** 服务器查询半径。 */
	UPROPERTY(BlueprintReadWrite, Category="Combat|Thinker", meta=(ClampMin="0", Units="cm")) float Radius = 0.0f;
	/** 首次 pulse 延迟。 */
	UPROPERTY(BlueprintReadWrite, Category="Combat|Thinker", meta=(ClampMin="0", Units="s")) float InitialDelay = 0.0f;
	/** 0 表示只 pulse 一次；正数使用 Scheduler repeating。 */
	UPROPERTY(BlueprintReadWrite, Category="Combat|Thinker", meta=(ClampMin="0", Units="s")) float PulseInterval = 0.0f;
	/** 0 表示一次 pulse 后完成；正数按绝对时间结束。 */
	UPROPERTY(BlueprintReadWrite, Category="Combat|Thinker", meta=(ClampMin="0", Units="s")) float Duration = 0.0f;
	/** 每个目标每个逻辑 pulse 的伤害。 */
	UPROPERTY(BlueprintReadWrite, Category="Combat|Thinker", meta=(ClampMin="0")) float DamagePerPulse = 0.0f;
	/** true 时 Thinker 只承担复制/表现生命周期，不查询或结算 gameplay 目标。 */
	UPROPERTY(BlueprintReadWrite, Category="Combat|Thinker", meta=(DisplayName="仅视觉表现", ToolTip="启用后 Thinker 只维护复制与表现生命周期，不查询目标，也不结算伤害或 Modifier。")) bool bVisualOnly = false;
	/** pulse 伤害类型。 */
	UPROPERTY(BlueprintReadWrite, Category="Combat|Thinker") ECombatDamageType DamageType = ECombatDamageType::Magical;
	/** 非空时每个 pulse 通过 ModifierComponent 施加或刷新该定义。 */
	UPROPERTY(BlueprintReadWrite, Category="Combat|Thinker") TObjectPtr<UCombatModifierData> ModifierPerPulse = nullptr;
	/** Thinker 施加 Modifier 的持续时间覆盖；小于 0 使用定义值。 */
	UPROPERTY(BlueprintReadWrite, Category="Combat|Thinker") float ModifierDurationOverride = -1.0f;
	/** 阵营与状态过滤规则；CastRange 在 AoE 查询中忽略。 */
	UPROPERTY(BlueprintReadWrite, Category="Combat|Thinker") FCombatTargetingRules TargetingRules;
	/** RootEvent 链。 */
	UPROPERTY(BlueprintReadWrite, Category="Combat|Thinker") FCombatEventContext ParentEvent;
	/** Ability/Projectile/Modifier 来源链。 */
	UPROPERTY(BlueprintReadWrite, Category="Combat|Thinker") FCombatSourceContext SourceContext;
	/** Ability 激活 ID，供显式 cancel-with-source。 */
	UPROPERTY(BlueprintReadWrite, Category="Combat|Thinker") FCombatEventId AbilityActivationId;
	/** true 时 Ability End 可取消；默认 fire-and-forget。 */
	UPROPERTY(BlueprintReadWrite, Category="Combat|Thinker") bool bCancelWithSourceAbility = false;
};

/** Thinker Spawn/Finish 的结构化结果。 */
USTRUCT(BlueprintType)
struct UE_GAS_API FCombatThinkerResult
{
	GENERATED_BODY()

	/** Spawn 是否接受，或 Finish 是否来自活动记录。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Thinker") bool bSuccess = false;
	/** 对应稳定句柄。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Thinker") FCombatThinkerHandle Handle;
	/** 结束分类。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Thinker") ECombatThinkerFinishReason FinishReason = ECombatThinkerFinishReason::Cancelled;
	/** 所有 pulse 实际命中的目标次数。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Thinker") int32 AffectedTargetCount = 0;
	/** 失败稳定标签。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Thinker") FGameplayTag FailureTag;
};

/** Thinker exactly-once Finish 观察委托。 */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnCombatThinkerFinished, const FCombatThinkerResult&);
