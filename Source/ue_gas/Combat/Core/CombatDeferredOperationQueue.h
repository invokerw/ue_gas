#pragma once

#include "CoreMinimal.h"
#include "Templates/Function.h"

#include "Combat/Core/CombatTypes.h"

/** 记录当前战斗回调阶段；该阶段中的增删操作可先排队，等最外层阶段结束后再执行。 */
struct UE_GAS_API FCombatPhaseContext
{
	/** 阶段的稳定名称。 */
	FName Phase = NAME_None;
	/** 阶段关联的战斗事件。 */
	FCombatEventId EventId;
	/** 从 0 开始的嵌套深度。 */
	int32 Depth = 0;
};

/**
 * 延迟执行战斗回调中的增删操作，避免遍历效果列表时修改同一列表。
 * BeginPhase/EndPhase 可嵌套，只有退出最外层阶段才按入队顺序执行；没有活动阶段时，入队操作通常立即执行。
 */
class UE_GAS_API FCombatDeferredOperationQueue
{
public:
	FCombatDeferredOperationQueue() = default;
	/** 禁止复制，避免同一操作被两个队列重复提交。 */
	FCombatDeferredOperationQueue(const FCombatDeferredOperationQueue&) = delete;
	/** 禁止复制赋值，避免重复拥有回调。 */
	FCombatDeferredOperationQueue& operator=(const FCombatDeferredOperationQueue&) = delete;
	FCombatDeferredOperationQueue(FCombatDeferredOperationQueue&&) = default;
	FCombatDeferredOperationQueue& operator=(FCombatDeferredOperationQueue&&) = default;

	/** 进入一个具名战斗阶段；名称为空或已达到 MaxDepth 时返回 false，不改变阶段栈。 */
	bool BeginPhase(FName Phase, FCombatEventId EventId = FCombatEventId());
	/** 退出最内层阶段；退出最后一层时执行等待的操作，没有活动阶段时返回 false。 */
	bool EndPhase();
	/** 提交操作：处于阶段或正在清空队列时追加到队尾，否则当场调用；空回调被忽略。 */
	void Enqueue(TUniqueFunction<void()>&& Operation);
	/** 没有活动阶段时按入队顺序执行等待操作，回调新入队的操作也在本轮尾部执行；正在执行或仍在阶段中时不做处理。 */
	void Flush();
	/** 丢弃阶段和待处理操作，恢复为空队列。 */
	void Reset();

	/** 返回当前是否至少处于一个战斗阶段中。 */
	bool IsInPhase() const { return PhaseStack.Num() > 0; }
	/** 返回尚未提交的操作数量。 */
	int32 NumPending() const { return PendingOperations.Num(); }
	/** 返回最内层阶段上下文；队列为空时返回默认值。 */
	FCombatPhaseContext GetCurrentContext() const;

	/** 允许的最大嵌套深度，防止错误递归无限增长。 */
	int32 MaxDepth = 16;

private:
	/** 从根阶段到当前阶段的上下文栈。 */
	TArray<FCombatPhaseContext> PhaseStack;
	/** 等待稳定提交的 FIFO 操作列表。 */
	TArray<TUniqueFunction<void()>> PendingOperations;
	/** 防止 Flush 回调重入同一个 Flush 循环。 */
	bool bFlushing = false;
};

/** 复制数组生成稳定遍历快照，使回调可安全修改原容器。 */
template<typename ElementType>
TArray<ElementType> MakeCombatStableSnapshot(const TArray<ElementType>& Source)
{
	return Source;
}
