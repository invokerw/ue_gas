#include "Combat/Core/CombatDeferredOperationQueue.h"

bool FCombatDeferredOperationQueue::BeginPhase(const FName Phase, const FCombatEventId EventId)
{
	if (Phase.IsNone() || PhaseStack.Num() >= MaxDepth)
	{
		return false;
	}
	FCombatPhaseContext& Context = PhaseStack.AddDefaulted_GetRef();
	Context.Phase = Phase;
	Context.EventId = EventId;
	Context.Depth = PhaseStack.Num() - 1;
	return true;
}

bool FCombatDeferredOperationQueue::EndPhase()
{
	if (PhaseStack.IsEmpty())
	{
		return false;
	}
	PhaseStack.Pop(EAllowShrinking::No);
	if (PhaseStack.IsEmpty())
	{
		Flush();
	}
	return true;
}

void FCombatDeferredOperationQueue::Enqueue(TUniqueFunction<void()>&& Operation)
{
	if (!Operation)
	{
		return;
	}
	if (!IsInPhase() && !bFlushing)
	{
		Operation();
		return;
	}
	PendingOperations.Add(MoveTemp(Operation));
}

void FCombatDeferredOperationQueue::Flush()
{
	if (bFlushing || IsInPhase())
	{
		return;
	}
	bFlushing = true;
	// 按索引增长遍历，使回调中新入队的操作仍在同次 Flush 尾部执行，同时保持 FIFO。
	for (int32 Index = 0; Index < PendingOperations.Num(); ++Index)
	{
		TUniqueFunction<void()> Operation = MoveTemp(PendingOperations[Index]);
		if (Operation)
		{
			Operation();
		}
	}
	PendingOperations.Reset();
	bFlushing = false;
}

void FCombatDeferredOperationQueue::Reset()
{
	PhaseStack.Reset();
	PendingOperations.Reset();
	bFlushing = false;
}

FCombatPhaseContext FCombatDeferredOperationQueue::GetCurrentContext() const
{
	return PhaseStack.IsEmpty() ? FCombatPhaseContext() : PhaseStack.Last();
}
