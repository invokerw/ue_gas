#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"

#include "Combat/Scheduling/CombatSchedulerSubsystem.h"
#include "Combat/Thinker/CombatThinkerTypes.h"

#include "CombatThinkerSubsystem.generated.h"

class ACombatThinker;

/** 一个活动区域的服务器记录，持有创建参数、弱 Actor 引用、作用及结束任务和累计受影响次数。 */
struct FCombatThinkerRuntimeRecord
{
	FCombatThinkerHandle Handle;
	FCombatThinkerSpec Spec;
	TWeakObjectPtr<ACombatThinker> Actor;
	FCombatScheduleHandle PulseSchedule;
	FCombatScheduleHandle FinishSchedule;
	int32 AffectedTargetCount = 0;
};

/**
 * 在服务器管理固定位置的持续区域，例如按间隔伤害范围内单位的地面效果。
 * 区域 Actor 承载位置和复制表现，调度器控制首次延迟、重复作用及寿命；每轮经公共目标筛选、伤害和效果入口结算。
 * 取消、到期或世界关闭时清理任务、记录及 Actor；已施加到目标的普通效果继续按自身生命周期运行。
 */
UCLASS()
class UE_GAS_API UCombatThinkerSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** 校验来源和数值后创建区域 Actor，并登记作用与结束任务；失败返回原因标签并清理已创建资源。成功不表示首次作用已经执行。 */
	FCombatThinkerResult CreateThinker(const FCombatThinkerSpec& Spec);
	/** 取消区域并清理任务、广播结束、销毁 Actor；过期或已结束句柄返回失败，不重复广播。 */
	FCombatThinkerResult CancelThinker(FCombatThinkerHandle Handle);
	/** 取消同时匹配来源单位、激活编号且启用了随技能取消选项的区域，返回本次匹配数量。 */
	int32 CancelThinkersForAbility(ACombatUnitCharacter* Source, FCombatEventId ActivationId);
	/** 区域 Actor 被外部销毁时通知子系统结束对应记录；世界整体清理由 Deinitialize 负责。 */
	void NotifyThinkerEndPlay(FCombatThinkerHandle Handle);
	/** 返回 Handle 是否仍活动。 */
	bool IsThinkerActive(FCombatThinkerHandle Handle) const;
	/** 返回活动数量。 */
	int32 GetActiveThinkerCount() const { return ActiveThinkers.Num(); }
	/** 取得最近一次区域最终结束结果；首次结束前为默认值，下次结束会覆盖。 */
	const FCombatThinkerResult& GetLastFinishedResult() const { return LastFinishedResult; }
	/** 订阅区域最终结束的本地通知；订阅者退出时应解绑。 */
	FOnCombatThinkerFinished& OnThinkerFinished() { return ThinkerFinishedDelegate; }
	/** 世界关闭时结束全部区域，取消作用和寿命任务，销毁 Actor 并使本代句柄失效。 */
	virtual void Deinitialize() override;

private:
	/** 一次区域作用：校验句柄和来源，查询当前目标并提交伤害及可选效果；纯视觉区域跳过目标处理，未设寿命时本次作用后结束。 */
	void HandlePulse(FCombatThinkerHandle Handle, const FCombatScheduledTickContext& TickContext);
	/** 寿命到期时结束对应区域；同刻的区域作用具有更高调度优先级，先于本回调执行。 */
	void HandleFinished(FCombatThinkerHandle Handle, const FCombatScheduledTickContext& TickContext);
	/** 统一结束入口：校验身份、取消调度、删除记录并广播结果，随后销毁 Actor；重复调用只返回旧句柄失败。 */
	FCombatThinkerResult FinishThinker(FCombatThinkerHandle Handle, ECombatThinkerFinishReason Reason, FGameplayTag FailureTag);
	/** 输出 Pulse/Finished 结构化日志。 */
	void EmitThinkerLog(const FCombatThinkerRuntimeRecord& Record, FGameplayTag EventType, int32 PulseTargets, FGameplayTag FailureTag) const;

	/** 以区域编号索引活动记录；使用记录前仍须核对完整句柄及子系统代次。 */
	TMap<uint64, FCombatThinkerRuntimeRecord> ActiveThinkers;
	uint64 NextThinkerId = 1;
	uint32 ThinkerGeneration = 1;
	bool bDeinitializing = false;
	FCombatThinkerResult LastFinishedResult;
	FOnCombatThinkerFinished ThinkerFinishedDelegate;
};
