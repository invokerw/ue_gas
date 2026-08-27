#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

#include "Combat/Core/CombatTypes.h"
#include "Combat/Log/CombatEventSubsystem.h"
#include "Combat/Targeting/CombatTargetingTypes.h"

#include "CombatAuraTypes.generated.h"

class ACombatUnitCharacter;
class UCombatModifierData;

/** Aura exactly-once 结束原因。 */
UENUM(BlueprintType)
enum class ECombatAuraFinishReason : uint8
{
	/** 外部显式取消。 */
	Cancelled,
	/** Owner 死亡、换生命或失效。 */
	OwnerInvalid,
	/** Owner/World EndPlay。 */
	EndPlay
};

/** 启动 Aura 时冻结的 Owner、查询和 child Modifier 策略。 */
USTRUCT(BlueprintType)
struct UE_GAS_API FCombatAuraSpec
{
	GENERATED_BODY()

	/** Aura 的权威 Owner 与 child Modifier Source。 */
	UPROPERTY(BlueprintReadWrite, Category="Combat|Aura", meta=(DisplayName="Aura 所有者", ToolTip="Aura 的服务器权威所有者，同时作为子 Modifier 的来源单位。")) TObjectPtr<ACombatUnitCharacter> Owner = nullptr;
	/** 每次 reconcile 的统一服务器查询半径。 */
	UPROPERTY(BlueprintReadWrite, Category="Combat|Aura", meta=(DisplayName="作用半径", ToolTip="每次协调时由服务器查询目标的半径。", ClampMin="0", Units="cm")) float Radius = 0.0f;
	/** Combat Scheduler Coalesce 查询间隔。 */
	UPROPERTY(BlueprintReadWrite, Category="Combat|Aura", meta=(DisplayName="协调间隔", ToolTip="Combat Scheduler 驱动 Aura 目标协调的固定间隔。", ClampMin="0.02", Units="s")) float ReconcileInterval = 0.25f;
	/** child 目标的阵营与状态过滤。 */
	UPROPERTY(BlueprintReadWrite, Category="Combat|Aura", meta=(DisplayName="目标规则", ToolTip="筛选子 Modifier 目标时使用的阵营与状态规则。")) FCombatTargetingRules TargetingRules;
	/** 每个合法目标最多持有一个普通 child Modifier。 */
	UPROPERTY(BlueprintReadWrite, Category="Combat|Aura", meta=(DisplayName="子 Modifier 定义", ToolTip="Aura 为每个合法目标至多维持一个的普通 Modifier。")) TObjectPtr<UCombatModifierData> ChildModifierData = nullptr;
	/** child 持续时间覆盖；小于 0 使用 child 定义。 */
	UPROPERTY(BlueprintReadWrite, Category="Combat|Aura", meta=(DisplayName="子 Modifier 持续时间覆盖", ToolTip="小于 0 使用子 Modifier 定义值；0 表示无限；正数覆盖定义持续时间。", Units="s")) float ChildDurationOverride = -1.0f;
	/** true 时 State.Broken 暂停 Aura 并移除全部 child。 */
	UPROPERTY(BlueprintReadWrite, Category="Combat|Aura", meta=(DisplayName="受 Break 禁用", ToolTip="启用后，所有者具有 State.Broken 时暂停 Aura 并移除全部子 Modifier。")) bool bDisabledByBreak = true;
	/** 继承 Ability/Modifier 的事件树；无效时 Aura 创建根事件。 */
	UPROPERTY(BlueprintReadWrite, Category="Combat|Aura", meta=(DisplayName="父事件", ToolTip="继承 Ability 或 Modifier 的事件树；无效时 Aura 创建新的根事件。")) FCombatEventContext ParentEvent;
	/** 保留创建 Aura 的稳定来源链。 */
	UPROPERTY(BlueprintReadWrite, Category="Combat|Aura", meta=(DisplayName="来源上下文", ToolTip="保留创建 Aura 的稳定 Ability、Projectile 与 Modifier 来源链。")) FCombatSourceContext SourceContext;
};

/** Aura Start/Finish 的结构化结果。 */
USTRUCT(BlueprintType)
struct UE_GAS_API FCombatAuraResult
{
	GENERATED_BODY()

	/** Start 被接受或 Finish 命中活动记录。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Aura", meta=(DisplayName="成功", ToolTip="启动请求是否被接受，或结束请求是否命中活动 Aura。")) bool bSuccess = false;
	/** 对应稳定 Aura 句柄。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Aura", meta=(DisplayName="Aura 句柄", ToolTip="对应 Aura 的稳定外部句柄。")) FCombatAuraHandle Handle;
	/** exactly-once 结束分类。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Aura", meta=(DisplayName="结束原因", ToolTip="Aura 的唯一结束分类。")) ECombatAuraFinishReason FinishReason = ECombatAuraFinishReason::Cancelled;
	/** 结束前最后一次 reconcile 后的 child 数量。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Aura", meta=(DisplayName="子 Modifier 数量", ToolTip="结束前最后一次协调后仍在维护的子 Modifier 数量。")) int32 ChildCount = 0;
	/** 失败时的稳定标签。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Aura", meta=(DisplayName="失败标签", ToolTip="失败时可由程序稳定判定的原因标签。")) FGameplayTag FailureTag;
};

/** Aura exactly-once Finish 观察委托。 */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnCombatAuraFinished, const FCombatAuraResult&);
