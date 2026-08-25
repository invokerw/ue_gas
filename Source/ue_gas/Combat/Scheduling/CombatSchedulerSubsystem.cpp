#include "Combat/Scheduling/CombatSchedulerSubsystem.h"

#include "Engine/World.h"

void UCombatSchedulerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Slots.Reset();
	Heap.Reset();
	PendingHeapNodes.Reset();
	Stats = FCombatSchedulerStats();
}

void UCombatSchedulerSubsystem::Deinitialize()
{
	Slots.Reset();
	Heap.Reset();
	PendingHeapNodes.Reset();
	bDispatching = false;
	Stats = FCombatSchedulerStats();
	Super::Deinitialize();
}

void UCombatSchedulerSubsystem::Tick(float DeltaTime)
{
	if (CanRunAuthorityCallbacks())
	{
		RunDueTasks(GetCurrentGameTime());
	}
}

TStatId UCombatSchedulerSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UCombatSchedulerSubsystem, STATGROUP_Tickables);
}

FCombatScheduleHandle UCombatSchedulerSubsystem::ScheduleOnce(
	UObject* Owner,
	const double Delay,
	const int32 Priority,
	FCombatScheduledDelegate Callback)
{
	return AddSlot(Owner, Delay, 0.0, Priority, ECombatCatchUpPolicy::ExecuteAllBounded, false, MoveTemp(Callback));
}

FCombatScheduleHandle UCombatSchedulerSubsystem::ScheduleRepeating(
	UObject* Owner,
	const double InitialDelay,
	const double Interval,
	const int32 Priority,
	const ECombatCatchUpPolicy Policy,
	FCombatScheduledDelegate Callback)
{
	if (!FMath::IsFinite(Interval) || Interval <= 0.0)
	{
		return FCombatScheduleHandle();
	}
	return AddSlot(Owner, InitialDelay, Interval, Priority, Policy, true, MoveTemp(Callback));
}

FCombatScheduleHandle UCombatSchedulerSubsystem::Reschedule(
	const FCombatScheduleHandle Handle,
	const double NewDelay,
	const double NewInterval)
{
	FScheduleSlot* Slot = Slots.Find(Handle.Key.Id);
	if (!Slot || Slot->Handle != Handle || !FMath::IsFinite(NewDelay) || NewDelay < 0.0
		|| (Slot->bRepeating && (!FMath::IsFinite(NewInterval) || NewInterval <= 0.0)))
	{
		return FCombatScheduleHandle();
	}

	// generation 递增后旧堆节点无需原地删除，弹出时会被稳定丢弃。
	++Slot->Handle.Key.Generation;
	if (Slot->Handle.Key.Generation == 0)
	{
		Slots.Remove(Handle.Key.Id);
		return FCombatScheduleHandle();
	}
	Slot->NextFireTime = GetCurrentGameTime() + NewDelay;
	if (Slot->bRepeating)
	{
		Slot->Interval = NewInterval;
	}
	QueueSlotNode(*Slot);
	return Slot->Handle;
}

bool UCombatSchedulerSubsystem::Cancel(const FCombatScheduleHandle Handle)
{
	FScheduleSlot* Slot = Slots.Find(Handle.Key.Id);
	if (!Slot || Slot->Handle != Handle)
	{
		return false;
	}
	++Slot->Handle.Key.Generation;
	Slots.Remove(Handle.Key.Id);
	return true;
}

int32 UCombatSchedulerSubsystem::CancelAllForOwner(const UObject* Owner)
{
	if (!Owner)
	{
		return 0;
	}
	TArray<uint64> Ids;
	for (const TPair<uint64, FScheduleSlot>& Pair : Slots)
	{
		if (Pair.Value.Owner.Get() == Owner)
		{
			Ids.Add(Pair.Key);
		}
	}
	for (const uint64 Id : Ids)
	{
		Slots.Remove(Id);
	}
	return Ids.Num();
}

bool UCombatSchedulerSubsystem::IsHandleActive(const FCombatScheduleHandle Handle) const
{
	const FScheduleSlot* Slot = Slots.Find(Handle.Key.Id);
	return Slot && Slot->Handle == Handle && Slot->Owner.IsValid();
}

int32 UCombatSchedulerSubsystem::RunDueTasks(const double Now)
{
	Stats.CallbacksLastFrame = 0;
	Stats.OverdueSlots = 0;
	Stats.BudgetDeferrals = 0;

	if (!FMath::IsFinite(Now) || !CanRunAuthorityCallbacks())
	{
		Stats.ActiveSlots = Slots.Num();
		return 0;
	}

	bDispatching = true;
	TMap<TWeakObjectPtr<UObject>, int32> OwnerCallbackCounts;
	TArray<FHeapNode> BudgetDeferredNodes;
	const FHeapPredicate Predicate;

	// dispatch 期间只消费进入本轮前已有的堆节点；回调创建的新任务延迟到下一轮，防止同步重入。
	while (!Heap.IsEmpty() && Stats.CallbacksLastFrame < MaxCallbacksPerFrame)
	{
		if (Heap[0].NextFireTime > Now)
		{
			break;
		}

		FHeapNode Node;
		Heap.HeapPop(Node, Predicate, EAllowShrinking::No);
		FScheduleSlot* Slot = Slots.Find(Node.Id);
		if (!Slot || Slot->Handle.Key.Generation != Node.Generation || !Slot->Owner.IsValid())
		{
			if (Slot && !Slot->Owner.IsValid())
			{
				Slots.Remove(Node.Id);
			}
			continue;
		}

		int32& OwnerCount = OwnerCallbackCounts.FindOrAdd(Slot->Owner);
		if (OwnerCount >= MaxCallbacksPerOwnerPerFrame)
		{
			BudgetDeferredNodes.Add(Node);
			++Stats.BudgetDeferrals;
			continue;
		}

		// 回调可能取消或重排自身，后续每次访问槽位都必须重新查找并校验 generation。
		const uint32 ExpectedGeneration = Slot->Handle.Key.Generation;
		const bool bRepeating = Slot->bRepeating;
		const double Interval = Slot->Interval;
		int32 DueCount = 1;
		if (bRepeating)
		{
			DueCount = FMath::Max(1, FMath::FloorToInt((Now - Slot->NextFireTime) / Interval) + 1);
			if (DueCount > 1)
			{
				++Stats.OverdueSlots;
			}
		}

		// Coalesce/SkipExpired 每轮只回调一次；ExecuteAllBounded 同时受三层预算约束。
		int32 CallbackCount = 1;
		if (bRepeating && Slot->Policy == ECombatCatchUpPolicy::ExecuteAllBounded)
		{
			CallbackCount = FMath::Min3(
				DueCount,
				MaxCatchUpCallbacksPerTask,
				FMath::Min(MaxCallbacksPerFrame - Stats.CallbacksLastFrame, MaxCallbacksPerOwnerPerFrame - OwnerCount));
		}

		for (int32 CallbackIndex = 0; CallbackIndex < CallbackCount; ++CallbackIndex)
		{
			Slot = Slots.Find(Node.Id);
			if (!Slot || Slot->Handle.Key.Generation != ExpectedGeneration || !Slot->Owner.IsValid())
			{
				break;
			}

			FCombatScheduledTickContext Context;
			Context.ActualTime = Now;
			Context.Interval = static_cast<float>(Slot->Interval);
			Context.ScheduledTime = Slot->NextFireTime;
			Context.TickIndex = Slot->TickIndex;

			int32 ConsumedTicks = 1;
			if (Slot->bRepeating && Slot->Policy == ECombatCatchUpPolicy::Coalesce)
			{
				ConsumedTicks = DueCount;
				Context.TickCount = DueCount;
			}
			else if (Slot->bRepeating && Slot->Policy == ECombatCatchUpPolicy::SkipExpired)
			{
				ConsumedTicks = DueCount;
				Context.ScheduledTime += Slot->Interval * (DueCount - 1);
				Context.TickIndex += DueCount - 1;
			}

			FCombatScheduledDelegate Callback = Slot->Callback;
			Callback.ExecuteIfBound(Context);
			++Stats.CallbacksLastFrame;
			++OwnerCount;

			Slot = Slots.Find(Node.Id);
			if (!Slot || Slot->Handle.Key.Generation != ExpectedGeneration)
			{
				break;
			}
			if (!Slot->bRepeating)
			{
				Slots.Remove(Node.Id);
				break;
			}
			Slot->NextFireTime += Slot->Interval * ConsumedTicks;
			Slot->TickIndex += ConsumedTicks;
		}

		Slot = Slots.Find(Node.Id);
		if (Slot && Slot->Handle.Key.Generation == ExpectedGeneration && Slot->bRepeating)
		{
			QueueSlotNode(*Slot);
		}
	}

	// Owner 预算推迟的节点保持原触发时间和排序键，下一轮仍按确定性顺序竞争。
	for (const FHeapNode& DeferredNode : BudgetDeferredNodes)
	{
		Heap.HeapPush(DeferredNode, Predicate);
	}
	bDispatching = false;
	FlushPendingHeapNodes();
	Stats.ActiveSlots = Slots.Num();
	return Stats.CallbacksLastFrame;
}

bool UCombatSchedulerSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE || WorldType == EWorldType::GamePreview;
}

FCombatScheduleHandle UCombatSchedulerSubsystem::AddSlot(
	UObject* Owner,
	const double Delay,
	const double Interval,
	const int32 Priority,
	const ECombatCatchUpPolicy Policy,
	const bool bRepeating,
	FCombatScheduledDelegate Callback)
{
	if (!IsValid(Owner) || !Callback.IsBound() || !FMath::IsFinite(Delay) || Delay < 0.0)
	{
		return FCombatScheduleHandle();
	}

	FScheduleSlot Slot;
	Slot.Handle.Key.Id = NextId++;
	Slot.Handle.Key.Generation = 1;
	Slot.Owner = Owner;
	Slot.NextFireTime = GetCurrentGameTime() + Delay;
	Slot.Interval = Interval;
	Slot.Priority = Priority;
	Slot.ApplySequence = NextApplySequence++;
	Slot.Policy = Policy;
	Slot.Callback = MoveTemp(Callback);
	Slot.bRepeating = bRepeating;

	if (Slot.Handle.Key.Id == 0 || NextId == 0 || NextApplySequence == 0)
	{
		return FCombatScheduleHandle();
	}

	const FCombatScheduleHandle Handle = Slot.Handle;
	Slots.Add(Handle.Key.Id, MoveTemp(Slot));
	QueueSlotNode(Slots.FindChecked(Handle.Key.Id));
	return Handle;
}

void UCombatSchedulerSubsystem::QueueSlotNode(const FScheduleSlot& Slot)
{
	FHeapNode Node;
	Node.NextFireTime = Slot.NextFireTime;
	Node.Priority = Slot.Priority;
	Node.ApplySequence = Slot.ApplySequence;
	Node.Id = Slot.Handle.Key.Id;
	Node.Generation = Slot.Handle.Key.Generation;
	if (bDispatching)
	{
		PendingHeapNodes.Add(Node);
	}
	else
	{
		Heap.HeapPush(Node, FHeapPredicate());
	}
}

void UCombatSchedulerSubsystem::FlushPendingHeapNodes()
{
	const FHeapPredicate Predicate;
	for (const FHeapNode& Node : PendingHeapNodes)
	{
		Heap.HeapPush(Node, Predicate);
	}
	PendingHeapNodes.Reset();
}

double UCombatSchedulerSubsystem::GetCurrentGameTime() const
{
	return GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
}

bool UCombatSchedulerSubsystem::CanRunAuthorityCallbacks() const
{
	return GetWorld() && GetWorld()->GetNetMode() != NM_Client;
}

bool UCombatSchedulerSubsystem::FHeapPredicate::operator()(const FHeapNode& A, const FHeapNode& B) const
{
	if (A.NextFireTime != B.NextFireTime)
	{
		return A.NextFireTime < B.NextFireTime;
	}
	if (A.Priority != B.Priority)
	{
		return A.Priority > B.Priority;
	}
	return A.ApplySequence < B.ApplySequence;
}
