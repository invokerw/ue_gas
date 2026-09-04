#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

#include "Combat/Core/CombatTypes.h"
#include "Combat/Log/CombatEventSubsystem.h"
#include "Combat/Targeting/CombatTargetingTypes.h"

#include "CombatAuraTypes.generated.h"

class ACombatUnitCharacter;
class UCombatModifierData;

/** 光环停止维护范围效果的原因；每个光环只发送一次最终结束通知。 */
UENUM(BlueprintType)
enum class ECombatAuraFinishReason : uint8
{
	/** 外部显式取消。 */
	Cancelled,
	/** 产生光环的单位死亡、进入另一条生命或已经失效，旧光环随之结束。 */
	OwnerInvalid,
	/** 产生光环的单位退出场景，或所属世界正在关闭。 */
	EndPlay
};

/**
 * 一次光环的创建参数。启动时复制这些参数；之后仍读取所有者的当前位置和目标的当前状态。
 * 光环给合格目标施加普通 Modifier（增益或减益效果），这些效果在本模块中称为子效果。
 */
USTRUCT(BlueprintType)
struct UE_GAS_API FCombatAuraSpec
{
	GENERATED_BODY()

	/** 产生光环的单位，必须在本世界的服务器上存活；查询以其当前位置为中心，子效果也以它作为来源。 */
	UPROPERTY(BlueprintReadWrite, Category="Combat|Aura", meta=(DisplayName="Aura 所有者", ToolTip="产生光环的单位，必须在本世界的服务器上存活；查询以其当前位置为中心，子效果也以它作为来源。")) TObjectPtr<ACombatUnitCharacter> Owner = nullptr;
	/** 以所有者当前位置为中心搜索目标的半径，单位为厘米；必须有限且非负，目标是否合格仍由目标规则决定。 */
	UPROPERTY(BlueprintReadWrite, Category="Combat|Aura", meta=(DisplayName="作用半径", ToolTip="以所有者当前位置为中心搜索目标的半径，单位为厘米；必须有限且非负，目标是否合格仍由目标规则决定。", ClampMin="0", Units="cm")) float Radius = 0.0f;
	/**
	 * 重新检查目标并增删子效果的间隔，单位为秒，必须有限且大于 0。启动时先检查一次；
	 * 例如设为 0.25，之后约每 0.25 秒再检查。卡顿漏过多轮时只检查当前状态一次，不逐轮补查。
	 */
	UPROPERTY(BlueprintReadWrite, Category="Combat|Aura", meta=(DisplayName="协调间隔", ToolTip="重新检查目标并增删子效果的间隔，单位为秒，必须有限且大于 0。启动时先检查一次； 例如设为 0.25，之后约每 0.25 秒再检查。卡顿漏过多轮时只检查当前状态一次，不逐轮补查。", ClampMin="0.02", Units="s")) float ReconcileInterval = 0.25f;
	/** 决定哪些单位可以获得子效果，包括与所有者的队伍关系及目标状态；移动后是否仍在范围内由每轮查询重新判断。 */
	UPROPERTY(BlueprintReadWrite, Category="Combat|Aura", meta=(DisplayName="目标规则", ToolTip="决定哪些单位可以获得子效果，包括与所有者的队伍关系及目标状态；移动后是否仍在范围内由每轮查询重新判断。")) FCombatTargetingRules TargetingRules;
	/**
	 * 要施加到目标身上的增益或减益定义，不能为空。每个光环对每个目标最多记录一个效果句柄；
	 * 已有有效效果保持不变，效果被移除或过期后，后续检查会尝试补回，多个来源是否合并仍由 Modifier 的叠层规则决定。
	 */
	UPROPERTY(BlueprintReadWrite, Category="Combat|Aura", meta=(DisplayName="子 Modifier 定义", ToolTip="要施加到目标身上的增益或减益定义，不能为空。每个光环对每个目标最多记录一个效果句柄； 已有有效效果保持不变，效果被移除或过期后，后续检查会尝试补回，多个来源是否合并仍由 Modifier 的叠层规则决定。")) TObjectPtr<UCombatModifierData> ChildModifierData = nullptr;
	/**
	 * 子效果单次施加的持续时间，单位为秒：[-1, 0) 使用定义值，0 表示无限，正数覆盖定义；小于 -1 被拒绝。
	 * 已有子效果不会每轮续期；目标不再合格或光环结束时，即使尚未到期也会被移除。
	 */
	UPROPERTY(BlueprintReadWrite, Category="Combat|Aura", meta=(DisplayName="子 Modifier 持续时间覆盖", ToolTip="子效果单次施加的持续时间，单位为秒：[-1, 0) 使用定义值，0 表示无限，正数覆盖定义；小于 -1 被拒绝。 已有子效果不会每轮续期；目标不再合格或光环结束时，即使尚未到期也会被移除。", Units="s")) float ChildDurationOverride = -1.0f;
	/**
	 * 是否受被动禁用状态 State.Broken（Break）影响。启用后，检查发现所有者处于该状态时移除全部子效果，
	 * 但保留光环及定期检查；状态解除后的检查会重新施加。关闭时此状态不影响光环。
	 */
	UPROPERTY(BlueprintReadWrite, Category="Combat|Aura", meta=(DisplayName="受 Break 禁用", ToolTip="是否受被动禁用状态 State.Broken（Break）影响。启用后，检查发现所有者处于该状态时移除全部子效果， 但保留光环及定期检查；状态解除后的检查会重新施加。关闭时此状态不影响光环。")) bool bDisabledByBreak = true;
	/** 光环日志归属的事件上下文，用于追踪是哪次技能或效果创建了光环；未提供有效上下文时，启动时创建根事件。 */
	UPROPERTY(BlueprintReadWrite, Category="Combat|Aura", meta=(DisplayName="父事件", ToolTip="光环日志归属的事件上下文，用于追踪是哪次技能或效果创建了光环；未提供有效上下文时，启动时创建根事件。")) FCombatEventContext ParentEvent;
	/** 光环日志中的技能、弹体和效果定义身份；用于追查来源，不改变子效果的来源单位。 */
	UPROPERTY(BlueprintReadWrite, Category="Combat|Aura", meta=(DisplayName="来源上下文", ToolTip="光环日志中的技能、弹体和效果定义身份；用于追查来源，不改变子效果的来源单位。")) FCombatSourceContext SourceContext;
};

/** 启动、取消或最终结束光环时返回的结果；各字段含义取决于产生结果的操作。 */
USTRUCT(BlueprintType)
struct UE_GAS_API FCombatAuraResult
{
	GENERATED_BODY()

	/** 启动时表示光环已登记，不保证任何目标已获得效果；取消时也可能仅表示请求已排队，最终清理由结束通知确认。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Aura", meta=(DisplayName="成功", ToolTip="启动时表示光环已登记，不保证任何目标已获得效果；取消时也可能仅表示请求已排队，最终清理由结束通知确认。")) bool bSuccess = false;
	/** 用于查询或取消本次光环的身份凭证；字段非零不代表光环仍在运行，应向光环子系统查询。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Aura", meta=(DisplayName="Aura 句柄", ToolTip="用于查询或取消本次光环的身份凭证；字段非零不代表光环仍在运行，应向光环子系统查询。")) FCombatAuraHandle Handle;
	/** 最终结束通知中的原因；启动结果中的默认值不表示光环已经取消。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Aura", meta=(DisplayName="结束原因", ToolTip="最终结束通知中的原因；启动结果中的默认值不表示光环已经取消。")) ECombatAuraFinishReason FinishReason = ECombatAuraFinishReason::Cancelled;
	/**
	 * 启动时为首次检查后记录的子效果数；取消排队时为当前记录数；最终结束时为清理前记录数。
	 * 这是光环保存的映射数量，外部移除效果后可能要等下一轮检查才更新。
	 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Aura", meta=(DisplayName="子 Modifier 数量", ToolTip="启动时为首次检查后记录的子效果数；取消排队时为当前记录数；最终结束时为清理前记录数。 这是光环保存的映射数量，外部移除效果后可能要等下一轮检查才更新。")) int32 ChildCount = 0;
	/** 请求失败或光环因异常结束时的原因标签，可用于程序分支判断；不能只凭此字段判断光环是否仍然活动。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Aura", meta=(DisplayName="失败标签", ToolTip="请求失败或光环因异常结束时的原因标签，可用于程序分支判断；不能只凭此字段判断光环是否仍然活动。")) FGameplayTag FailureTag;
};

/** 光环最终结束时的服务器本地通知；子效果和检查任务已清理，同一光环只广播一次。 */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnCombatAuraFinished, const FCombatAuraResult&);
