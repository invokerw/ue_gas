#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"

#include "Combat/Core/CombatTypes.h"

#include "CombatSchedulerSubsystem.generated.h"

/** Scheduler 在每个逻辑 tick 执行的原生回调。 */
DECLARE_DELEGATE_OneParam(FCombatScheduledDelegate, const FCombatScheduledTickContext&);

/** 暴露当前 Scheduler 负载和预算延期情况。 */
USTRUCT(BlueprintType)
struct UE_GAS_API FCombatSchedulerStats
{
	GENERATED_BODY()

	/** 当前仍可执行的任务槽位数量。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Scheduling") int32 ActiveSlots = 0;
	/** 最近一次 RunDueTasks 实际执行的回调数量。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Scheduling") int32 CallbacksLastFrame = 0;
	/** 最近一次执行结束后仍已到期的槽位数量。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Scheduling") int32 OverdueSlots = 0;
	/** 最近一次执行因全局或 Owner 预算推迟的槽位数量。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Scheduling") int32 BudgetDeferrals = 0;
};

/**
 * 按 World Game Time 驱动服务器权威的离散战斗任务。
 * Owner 或 World 结束时取消任务，generation 用于淘汰旧堆节点和过期回调。
 */
UCLASS()
class UE_GAS_API UCombatSchedulerSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	/** 初始化空调度器及统计状态。 */
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	/** 取消全部任务并释放回调，防止跨 World 泄漏。 */
	virtual void Deinitialize() override;
	/** 在支持的游戏 World 中以当前 World Game Time 执行到期任务。 */
	virtual void Tick(float DeltaTime) override;
	/** 返回 Scheduler Tick 的统计 ID。 */
	virtual TStatId GetStatId() const override;

	/** 注册一次性任务；无效 Owner、负 Delay 或空回调返回无效句柄。 */
	FCombatScheduleHandle ScheduleOnce(
		UObject* Owner,
		double Delay,
		int32 Priority,
		FCombatScheduledDelegate Callback);

	/** 注册重复任务，并使用指定 catch-up 策略处理落后 tick。 */
	FCombatScheduleHandle ScheduleRepeating(
		UObject* Owner,
		double InitialDelay,
		double Interval,
		int32 Priority,
		ECombatCatchUpPolicy Policy,
		FCombatScheduledDelegate Callback);

	/** 更新现有任务的下一触发时间和间隔，并递增 generation 使旧堆节点失效。 */
	FCombatScheduleHandle Reschedule(FCombatScheduleHandle Handle, double NewDelay, double NewInterval);
	/** 取消与完整身份匹配的单个任务。 */
	bool Cancel(FCombatScheduleHandle Handle);
	/** 取消指定 Owner 的全部任务，并返回取消数量。 */
	int32 CancelAllForOwner(const UObject* Owner);
	/** 检查句柄是否仍对应活动槽位。 */
	bool IsHandleActive(FCombatScheduleHandle Handle) const;

	/** 确定性执行全部到期任务；生产 Tick 传入 World Game Time，Automation 可注入固定时间。 */
	int32 RunDueTasks(double Now);

	/** 返回最近一次执行后的统计快照。 */
	FCombatSchedulerStats GetStats() const { return Stats; }

	/** 单帧允许执行的全局回调上限。 */
	UPROPERTY(EditAnywhere, Category="Combat|Scheduling", meta=(ClampMin="1"))
	int32 MaxCallbacksPerFrame = 256;

	/** 单个 Owner 每帧允许执行的回调上限。 */
	UPROPERTY(EditAnywhere, Category="Combat|Scheduling", meta=(ClampMin="1"))
	int32 MaxCallbacksPerOwnerPerFrame = 64;

	/** ExecuteAllBounded 单任务每帧允许补执行的 tick 上限。 */
	UPROPERTY(EditAnywhere, Category="Combat|Scheduling", meta=(ClampMin="1"))
	int32 MaxCatchUpCallbacksPerTask = 8;

protected:
	/** 仅允许 Game、PIE 和 GamePreview World 创建并运行调度器。 */
	virtual bool DoesSupportWorldType(EWorldType::Type WorldType) const override;

private:
	/** 保存一个活动任务的权威运行时状态。 */
	struct FScheduleSlot
	{
		/** 对外句柄和当前 generation。 */
		FCombatScheduleHandle Handle;
		/** 任务生命周期 Owner；失效时自动取消。 */
		TWeakObjectPtr<UObject> Owner;
		/** 下一次计划触发的 World Game Time。 */
		double NextFireTime = 0.0;
		/** 重复任务间隔；一次性任务为 0。 */
		double Interval = 0.0;
		/** 同一触发时间的第一排序键，数值越大越先执行。 */
		int32 Priority = 0;
		/** 同优先级时保持创建顺序的稳定序号。 */
		uint64 ApplySequence = 0;
		/** 下一个逻辑 tick 的 0-based 序号。 */
		int32 TickIndex = 0;
		/** 任务落后时采用的补帧策略。 */
		ECombatCatchUpPolicy Policy = ECombatCatchUpPolicy::ExecuteAllBounded;
		/** 到期时执行的原生回调。 */
		FCombatScheduledDelegate Callback;
		/** 是否在执行后继续安排下一 tick。 */
		bool bRepeating = false;
	};

	/** 堆中用于排序和验证 generation 的轻量节点。 */
	struct FHeapNode
	{
		/** 节点代表的计划触发时间。 */
		double NextFireTime = 0.0;
		/** 同一时间的优先级排序键。 */
		int32 Priority = 0;
		/** 同优先级的稳定创建顺序。 */
		uint64 ApplySequence = 0;
		/** 对应 Slots 中的槽位 ID。 */
		uint64 Id = 0;
		/** 入堆时的 generation，用于识别陈旧节点。 */
		uint32 Generation = 0;
	};

	/** 将 TArray Heap 解释为最早时间、最高优先级、最早序号优先的堆。 */
	struct FHeapPredicate
	{
		/** 返回 A 是否应排在 B 之后。 */
		bool operator()(const FHeapNode& A, const FHeapNode& B) const;
	};

	/** 校验输入、创建槽位并把首个节点加入堆。 */
	FCombatScheduleHandle AddSlot(
		UObject* Owner,
		double Delay,
		double Interval,
		int32 Priority,
		ECombatCatchUpPolicy Policy,
		bool bRepeating,
		FCombatScheduledDelegate Callback);

	/** 将槽位当前 generation 对应的节点排入主堆或待处理队列。 */
	void QueueSlotNode(const FScheduleSlot& Slot);
	/** 在 dispatch 结束后把回调中新建/重排的节点加入主堆。 */
	void FlushPendingHeapNodes();
	/** 返回当前 World Game Time；无 World 时返回 0。 */
	double GetCurrentGameTime() const;
	/** 返回当前 World 是否允许执行服务器权威回调。 */
	bool CanRunAuthorityCallbacks() const;

	/** 以稳定 ID 索引的活动任务槽位。 */
	TMap<uint64, FScheduleSlot> Slots;
	/** 按触发时间、优先级和 ApplySequence 排序的任务堆。 */
	TArray<FHeapNode> Heap;
	/** dispatch 期间生成的节点，避免修改正在遍历的堆。 */
	TArray<FHeapNode> PendingHeapNodes;
	/** 下一个任务槽位的稳定 ID。 */
	uint64 NextId = 1;
	/** 下一个任务使用的稳定创建序号。 */
	uint64 NextApplySequence = 1;
	/** 标记当前是否正在执行回调。 */
	bool bDispatching = false;
	/** 最近一次 RunDueTasks 的统计快照。 */
	FCombatSchedulerStats Stats;
};
