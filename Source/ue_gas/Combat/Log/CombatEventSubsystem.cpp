#include "Combat/Log/CombatEventSubsystem.h"

#include "Engine/World.h"

DEFINE_LOG_CATEGORY(LogCombat);

FString FCombatLogRecord::ToString() const
{
	return FString::Printf(
		TEXT("Seq=%llu Event=%s Root=%s Depth=%d Type=%s Failure=%s Detail=%s"),
		Sequence,
		*Context.EventId.ToString(),
		*Context.RootEventId.ToString(),
		Context.Depth,
		*EventType.ToString(),
		*FailureTag.ToString(),
		*Diagnostic);
}

FCombatEventContext UCombatEventSubsystem::CreateRootEvent()
{
	FCombatEventContext Context;
	Context.EventId = AllocateEventId();
	Context.RootEventId = Context.EventId;
	return Context;
}

FCombatEventContext UCombatEventSubsystem::CreateChildEvent(const FCombatEventContext& Parent)
{
	FCombatEventContext Context;
	if (!Parent.IsValid() || Parent.Depth >= MaxDepth)
	{
		return Context;
	}
	Context.EventId = AllocateEventId();
	Context.RootEventId = Parent.RootEventId;
	Context.Depth = Parent.Depth + 1;
	return Context;
}

void UCombatEventSubsystem::Emit(FCombatLogRecord Record)
{
	Record.Sequence = NextLogSequence++;
	Record.ServerTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	RecentRecords.Add(Record);
	// 只保留尾部窗口，避免 Dedicated Server 的诊断缓冲区无上限增长。
	if (RecentRecords.Num() > MaxRecentRecords)
	{
		RecentRecords.RemoveAt(0, RecentRecords.Num() - MaxRecentRecords, EAllowShrinking::No);
	}
	UE_LOG(LogCombat, Log, TEXT("%s"), *Record.ToString());
	RecordDelegate.Broadcast(RecentRecords.Last());
}

FCombatEventId UCombatEventSubsystem::AllocateEventId()
{
	FCombatEventId Id;
	Id.Sequence = NextEventSequence++;
	if (NextEventSequence == 0)
	{
		NextEventSequence = 1;
	}
	return Id;
}
