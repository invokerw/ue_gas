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

/** 区域效果对象（Thinker）最终结束的原因；同一对象只产生一次结束通知。 */
UENUM(BlueprintType)
enum class ECombatThinkerFinishReason : uint8
{
	/** 设定寿命到期，或未设持续寿命的一次区域作用已完成。 */
	Completed,
	/** 显式取消。 */
	Cancelled,
	/** Actor/World 正在结束。 */
	EndPlay
};

/**
 * 创建固定位置区域效果的参数，例如每秒伤害一次的地面区域。一次范围查询并施加伤害/效果称为一次脉冲（Pulse）。
 * 创建时复制参数；只在 Duration 和 PulseInterval 都大于 0 时重复作用，否则最多作用一次。
 */
USTRUCT(BlueprintType)
struct UE_GAS_API FCombatThinkerSpec
{
	GENERATED_BODY()

	/** 创建区域的来源单位，必须有效并位于本世界服务器；每次作用继续使用它进行阵营判断和记录伤害来源。 */
	UPROPERTY(BlueprintReadWrite, Category="Combat|Thinker") TObjectPtr<ACombatUnitCharacter> Source = nullptr;
	/** 区域固定中心的世界坐标，单位为厘米；不会随来源单位移动。 */
	UPROPERTY(BlueprintReadWrite, Category="Combat|Thinker") FVector Location = FVector::ZeroVector;
	/** 从固定中心搜索合格单位的半径，单位为厘米，必须有限且非负。 */
	UPROPERTY(BlueprintReadWrite, Category="Combat|Thinker", meta=(ClampMin="0", Units="cm")) float Radius = 0.0f;
	/** 从创建到首次区域作用的等待时间，单位为秒。0 表示下一轮调度执行；若区域先到寿命终点，首次作用也会被取消。 */
	UPROPERTY(BlueprintReadWrite, Category="Combat|Thinker", meta=(ClampMin="0", Units="s")) float InitialDelay = 0.0f;
	/** 区域重复作用的间隔，单位为秒；只有该值与 Duration 都大于 0 才重复，任一为 0 时最多作用一次。 */
	UPROPERTY(BlueprintReadWrite, Category="Combat|Thinker", meta=(ClampMin="0", Units="s")) float PulseInterval = 0.0f;
	/**
	 * 从创建时刻计算的寿命，单位为秒。0 表示首次作用后立即结束；正数到期后结束，即使首次作用还没发生。
	 * 例如延迟 1 秒、间隔 1 秒、寿命 3 秒，在第 1、2、3 秒作用，随后结束；同刻的作用先于到期清理。
	 */
	UPROPERTY(BlueprintReadWrite, Category="Combat|Thinker", meta=(ClampMin="0", Units="s")) float Duration = 0.0f;
	/** 每轮对每个合格目标请求的基础伤害，必须有限、非负且不超过统一数值上限；实际扣血还经过抗性、护盾等公共结算。 */
	UPROPERTY(BlueprintReadWrite, Category="Combat|Thinker", meta=(ClampMin="0")) float DamagePerPulse = 0.0f;
	/** 启用后只保留区域 Actor 和定时结束，不查询目标、不造成伤害也不施加效果，可用作持续视觉对象。 */
	UPROPERTY(BlueprintReadWrite, Category="Combat|Thinker", meta=(DisplayName="仅视觉表现", ToolTip="启用后只保留区域 Actor 和定时结束，不查询目标、不造成伤害也不施加效果，可用作持续视觉对象。")) bool bVisualOnly = false;
	/** 区域伤害采用物理、魔法或纯粹类型，由公共伤害管线决定抗性和免疫处理。 */
	UPROPERTY(BlueprintReadWrite, Category="Combat|Thinker") ECombatDamageType DamageType = ECombatDamageType::Magical;
	/** 可选的增益或减益定义；每轮对合格目标尝试施加，重复施加如何合并或叠层由该定义决定；为空时只处理伤害。 */
	UPROPERTY(BlueprintReadWrite, Category="Combat|Thinker") TObjectPtr<UCombatModifierData> ModifierPerPulse = nullptr;
	/** 每轮施加效果的持续时间，单位为秒：[-1,0) 使用效果定义，0 为无限，正数覆盖定义；小于 -1 被拒绝。区域结束不会主动移除已施加的效果。 */
	UPROPERTY(BlueprintReadWrite, Category="Combat|Thinker") float ModifierDurationOverride = -1.0f;
	/** 按来源单位的阵营关系和目标状态筛选受影响者；区域范围由 Radius 决定，规则中的 CastRange 不参与此查询。 */
	UPROPERTY(BlueprintReadWrite, Category="Combat|Thinker") FCombatTargetingRules TargetingRules;
	/** 区域伤害和日志所属的父事件；为空时创建根事件，以便追踪一次技能产生的后续结果。 */
	UPROPERTY(BlueprintReadWrite, Category="Combat|Thinker") FCombatEventContext ParentEvent;
	/** 保留创建该区域的技能、弹体或效果定义身份，用于伤害上下文和日志追踪。 */
	UPROPERTY(BlueprintReadWrite, Category="Combat|Thinker") FCombatSourceContext SourceContext;
	/** 创建该区域的技能激活编号；与来源单位共同匹配，供技能结束时选择应取消的区域。 */
	UPROPERTY(BlueprintReadWrite, Category="Combat|Thinker") FCombatEventId AbilityActivationId;
	/** 启用后，来源技能按单位和激活编号清理时可取消此区域；默认关闭，技能结束后区域仍按自己的寿命运行。 */
	UPROPERTY(BlueprintReadWrite, Category="Combat|Thinker") bool bCancelWithSourceAbility = false;
};

/** 区域创建或结束的结果；创建成功只表示已安排任务，实际区域作用发生在后续调度。 */
USTRUCT(BlueprintType)
struct UE_GAS_API FCombatThinkerResult
{
	GENERATED_BODY()

	/** 创建时表示区域及任务已建立；结束时表示找到活动记录并完成清理，不代表造成了正数伤害。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Thinker") bool bSuccess = false;
	/** 对应稳定句柄。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Thinker") FCombatThinkerHandle Handle;
	/** 最终结束通知中的原因；创建结果中的默认值不表示区域已取消。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Thinker") ECombatThinkerFinishReason FinishReason = ECombatThinkerFinishReason::Cancelled;
	/** 累计成功处理的目标次数，同一目标在不同轮可重复计数；某轮伤害请求成功或效果施加成功即计一次，不要求实际扣血大于 0。创建结果为 0。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Thinker") int32 AffectedTargetCount = 0;
	/** 创建失败或结束时的原因标签；主动取消也可携带标签，操作是否完成应结合 bSuccess 判断。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Thinker") FGameplayTag FailureTag;
};

/** 区域结束的服务器本地通知；任务和活动记录已移除，随后销毁区域 Actor，每个区域只广播一次。 */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnCombatThinkerFinished, const FCombatThinkerResult&);
