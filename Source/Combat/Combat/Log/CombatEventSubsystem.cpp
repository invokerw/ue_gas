#include "Combat/Log/CombatEventSubsystem.h"

#include "Engine/World.h"

DEFINE_LOG_CATEGORY(LogCombat);

FString FCombatLogRecord::ToString() const
{
	return FString::Printf(
		TEXT("Schema=%d Formula=%d Seq=%llu Event=%s Root=%s Depth=%d Type=%s Source=%d Target=%d Life=%lld Requested=%.3f Mitigated=%.3f Absorbed=%.3f Applied=%.3f GoldDelta=%lld GoldBalance=%lld Flags=%s Failure=%s Detail=%s"),
		SchemaVersion,
		FormulaVersion,
		Sequence,
		*Context.EventId.ToString(),
		*Context.RootEventId.ToString(),
		Context.Depth,
		*EventType.ToString(),
		SourceActorId,
		TargetActorId,
		static_cast<long long>(UnitLifeGeneration),
		RequestedAmount,
		MitigatedAmount,
		AbsorbedAmount,
		AppliedAmount,
		static_cast<long long>(GoldDelta),
		static_cast<long long>(GoldBalance),
		*Flags.ToStringSimple(),
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

void UCombatEventSubsystem::Emit(FCombatLogRecord Record, const FCombatLogResourceChange& ResourceChange)
{
	// 写入时统一覆盖 schema，防止调用者无意提交旧版或未知布局。
	Record.SchemaVersion = CurrentSchemaVersion;
	Record.Sequence = NextLogSequence++;
	Record.ServerTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	RecentRecords.Add(Record);
	// 只保留尾部窗口，避免 Dedicated Server 的诊断缓冲区无上限增长。
	if (RecentRecords.Num() > MaxRecentRecords)
	{
		RecentRecords.RemoveAt(0, RecentRecords.Num() - MaxRecentRecords, EAllowShrinking::No);
	}
	UE_LOG(LogCombat, Log, TEXT("%s"), *Record.ToString());
	PresentationDelegate.Broadcast(Record, ResourceChange);
	// 使用本次栈上快照：其他订阅者重入 Emit 时，环形数组可能扩容或淘汰当前项。
	RecordDelegate.Broadcast(Record);
}

TArray<FCombatLogRecord> UCombatEventSubsystem::GetRecordsForRootEvent(const FCombatEventId RootEventId) const
{
	TArray<FCombatLogRecord> Result;
	if (!RootEventId.IsValid())
	{
		return Result;
	}

	for (const FCombatLogRecord& Record : RecentRecords)
	{
		if (Record.Context.RootEventId == RootEventId)
		{
			Result.Add(Record);
		}
	}
	return Result;
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
