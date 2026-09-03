#pragma once

#include "CoreMinimal.h"
#include "Templates/Function.h"

#include "Combat/Core/CombatTypes.h"

/** 描述当前 deferred 操作所处的嵌套战斗阶段。 */
struct UE_GAS_API FCombatPhaseContext
{
	/** 阶段的稳定名称。 */
	FName Phase = NAME_None;
	/** 阶段关联的战斗事件。 */
	FCombatEventId EventId;
	/** 从 0 开始的嵌套深度。 */
	int32 Depth = 0;
};

/** 在战斗阶段结束后按入队顺序提交结构修改，避免遍历期间重入。 */
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

	/** 压入一个嵌套阶段；超过 MaxDepth 时拒绝并返回 false。 */
	bool BeginPhase(FName Phase, FCombatEventId EventId = FCombatEventId());
	/** 结束最内层阶段；退出根阶段时自动 Flush。 */
	bool EndPhase();
	/** 将操作追加到稳定 FIFO；不在阶段内时立即 Flush。 */
	void Enqueue(TUniqueFunction<void()>&& Operation);
	/** 按稳定快照批次执行全部待处理操作，并支持回调继续入队。 */
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
