#include "Combat/Combat/CombatTransactionSubsystem.h"

#include "Combat/Unit/CombatUnitCharacter.h"

bool UCombatTransactionSubsystem::BeginSlot(
	const FCombatEventContext& Context,
	const ECombatTransactionKind Kind,
	ACombatUnitCharacter* Target)
{
	if (!Context.IsValid() || !IsValid(Target) || Slots.Contains(Context.EventId.Sequence))
	{
		return false;
	}
	FSlot& Slot = Slots.Add(Context.EventId.Sequence);
	Slot.Kind = Kind;
	Slot.Target = Target;
	return true;
}

bool UCombatTransactionSubsystem::ReportDelta(
	const FCombatEventId EventId,
	const ECombatTransactionKind Kind,
	const FCombatTransactionDelta& Delta)
{
	FSlot* Slot = Slots.Find(EventId.Sequence);
	if (!Slot || Slot->Kind != Kind || Slot->bReported || !Slot->Target.IsValid())
	{
		return false;
	}
	Slot->Delta = Delta;
	Slot->bReported = true;
	return true;
}

bool UCombatTransactionSubsystem::ConsumeSlot(
	const FCombatEventId EventId,
	const ECombatTransactionKind Kind,
	FCombatTransactionDelta& OutDelta)
{
	FSlot* Slot = Slots.Find(EventId.Sequence);
	if (!Slot || Slot->Kind != Kind || !Slot->bReported)
	{
		return false;
	}
	OutDelta = Slot->Delta;
	Slots.Remove(EventId.Sequence);
	return true;
}

bool UCombatTransactionSubsystem::CancelSlot(const FCombatEventId EventId)
{
	return Slots.Remove(EventId.Sequence) > 0;
}
