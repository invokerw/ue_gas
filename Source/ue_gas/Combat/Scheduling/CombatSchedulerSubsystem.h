#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"

#include "Combat/Core/CombatTypes.h"

#include "CombatSchedulerSubsystem.generated.h"

/** 战斗任务到期时的本地回调；补执行策略可能让一次回调代表多个周期，具体见 TickCount。 */
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
	/** 最近一次调度处理中发现积压多个周期的重复任务数；不等于执行结束时尚未处理的全部到期任务数。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Scheduling") int32 OverdueSlots = 0;
	/** 最近一次处理中因同一所有者回调额度耗尽而暂缓的节点数；全局额度耗尽后尚未取出的节点不计入此值。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Scheduling") int32 BudgetDeferrals = 0;
};

/**
 * 用世界游戏时间统一驱动服务器上的前摇、周期效果、过期和范围检查，客户端不执行这些权威回调。
 * 按计划时刻、优先级从高到低、创建先后顺序执行，并以预算限制一轮工作量；连续位移仍由相应组件推进。
 * 任务仅弱引用所有者，所有者失效的任务不会执行；取消或重排后通过句柄代次拒绝旧节点，世界关闭时释放全部任务。
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
	virtual TStatId GetStatId() const override;

	/** 安排 Delay 秒后的单次回调；延迟为 0 也只在后续调度时执行。所有者无效、延迟非有限或为负、回调为空时返回无效句柄。 */
	FCombatScheduleHandle ScheduleOnce(
		UObject* Owner,
		double Delay,
		int32 Priority,
		FCombatScheduledDelegate Callback);

	/**
	 * 安排 InitialDelay 秒后的首次回调，此后每 Interval 秒触发；时间必须有限，首次延迟非负且间隔大于 0。
	 * 积压触发由 Policy 决定如何补执行；所有者或回调无效、参数非法时返回无效句柄。
	 */
	FCombatScheduleHandle ScheduleRepeating(
		UObject* Owner,
		double InitialDelay,
		double Interval,
		int32 Priority,
		ECombatCatchUpPolicy Policy,
		FCombatScheduledDelegate Callback);

	/**
	 * 从当前游戏时间重新计算下一次触发，并为重复任务更新间隔，返回新代次的句柄；调用方必须保存返回值，原句柄随之失效。
	 * 不会重置周期序号或改变单次/重复类型，单次任务忽略 NewInterval；参数或原句柄无效时返回无效句柄。
	 */
	FCombatScheduleHandle Reschedule(FCombatScheduleHandle Handle, double NewDelay, double NewInterval);
	/** 移除匹配完整身份的任务，成功返回 true；重复取消或旧句柄返回 false，不再执行该任务后续回调。 */
	bool Cancel(FCombatScheduleHandle Handle);
	/** 取消指定 Owner 的全部任务，并返回取消数量。 */
	int32 CancelAllForOwner(const UObject* Owner);
	/** 检查句柄是否仍对应活动槽位。 */
	bool IsHandleActive(FCombatScheduleHandle Handle) const;

	/**
	 * 按排序与预算处理截至 Now 秒已到期的任务，返回实际调用次数；生产传世界游戏时间，测试可传可控时间。
	 * 超预算任务保留到下一轮，回调中新建或重排的任务也留待下一轮；客户端或非有限时间不执行。
	 */
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
		/** 按时间较早、优先级较高、创建序号较小的顺序比较两个节点，为任务堆提供同一排序规则。 */
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
