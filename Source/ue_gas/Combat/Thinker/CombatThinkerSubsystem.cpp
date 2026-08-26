#include "Combat/Thinker/CombatThinkerSubsystem.h"

#include "Engine/World.h"

#include "Combat/Combat/CombatDamageSubsystem.h"
#include "Combat/Core/CombatNumericPolicy.h"
#include "Combat/Core/CombatTags.h"
#include "Combat/Modifiers/CombatModifierComponent.h"
#include "Combat/Targeting/CombatTargetingSubsystem.h"
#include "Combat/Thinker/CombatThinker.h"
#include "Combat/Unit/CombatUnitCharacter.h"

FCombatThinkerResult UCombatThinkerSubsystem::CreateThinker(const FCombatThinkerSpec& Spec)
{
	FCombatThinkerResult Result;
	if (!IsValid(Spec.Source) || !Spec.Source->HasAuthority() || Spec.Source->GetWorld() != GetWorld())
	{
		Result.FailureTag = CombatTags::Failure_Authority;
		return Result;
	}
	if (Spec.Location.ContainsNaN() || !FCombatNumericPolicyV1::IsValidNonNegativeRequest(Spec.DamagePerPulse)
		|| !FMath::IsFinite(Spec.Radius) || Spec.Radius < 0.0f
		|| !FMath::IsFinite(Spec.InitialDelay) || Spec.InitialDelay < 0.0f
		|| !FMath::IsFinite(Spec.PulseInterval) || Spec.PulseInterval < 0.0f
		|| !FMath::IsFinite(Spec.Duration) || Spec.Duration < 0.0f
		|| !FMath::IsFinite(Spec.ModifierDurationOverride) || Spec.ModifierDurationOverride < -1.0f)
	{
		Result.FailureTag = CombatTags::Failure_InvalidNumber;
		return Result;
	}
	UCombatSchedulerSubsystem* Scheduler = GetWorld()->GetSubsystem<UCombatSchedulerSubsystem>();
	if (!Scheduler)
	{
		Result.FailureTag = CombatTags::Failure_ActionUnsupported;
		return Result;
	}

	FActorSpawnParameters Params;
	Params.Owner = Spec.Source;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ACombatThinker* Actor = GetWorld()->SpawnActor<ACombatThinker>(
		ACombatThinker::StaticClass(), Spec.Location, FRotator::ZeroRotator, Params);
	if (!Actor)
	{
		Result.FailureTag = CombatTags::Failure_ActionUnsupported;
		return Result;
	}

	FCombatThinkerRuntimeRecord Record;
	Record.Handle.Key.Id = NextThinkerId++;
	Record.Handle.Key.Generation = ThinkerGeneration;
	Record.Handle.Key.LifeGeneration = Spec.Source->GetLifeGeneration();
	Record.Spec = Spec;
	if (!Record.Spec.ParentEvent.IsValid())
	{
		if (UCombatEventSubsystem* Events = GetWorld()->GetSubsystem<UCombatEventSubsystem>())
		{
			Record.Spec.ParentEvent = Events->CreateRootEvent();
		}
	}
	Record.Actor = Actor;
	Actor->InitializeThinker(Record.Handle);
	const FCombatThinkerHandle Handle = Record.Handle;
	ActiveThinkers.Add(Handle.Key.Id, MoveTemp(Record));
	FCombatThinkerRuntimeRecord& Stored = ActiveThinkers[Handle.Key.Id];

	if (Spec.Duration > 0.0f && Spec.PulseInterval > 0.0f)
	{
		Stored.PulseSchedule = Scheduler->ScheduleRepeating(
			this, Spec.InitialDelay, Spec.PulseInterval, 0, ECombatCatchUpPolicy::ExecuteAllBounded,
			FCombatScheduledDelegate::CreateWeakLambda(this,
				[this, Handle](const FCombatScheduledTickContext& Context) { HandlePulse(Handle, Context); }));
	}
	else
	{
		Stored.PulseSchedule = Scheduler->ScheduleOnce(
			this, Spec.InitialDelay, 0,
			FCombatScheduledDelegate::CreateWeakLambda(this,
				[this, Handle](const FCombatScheduledTickContext& Context) { HandlePulse(Handle, Context); }));
	}
	if (Spec.Duration > 0.0f)
	{
		Stored.FinishSchedule = Scheduler->ScheduleOnce(
			this, Spec.Duration, -1,
			FCombatScheduledDelegate::CreateWeakLambda(this,
				[this, Handle](const FCombatScheduledTickContext& Context) { HandleFinished(Handle, Context); }));
	}
	if (!Stored.PulseSchedule.IsValid() || (Spec.Duration > 0.0f && !Stored.FinishSchedule.IsValid()))
	{
		FinishThinker(Handle, ECombatThinkerFinishReason::Cancelled, CombatTags::Failure_ActionUnsupported);
		Result.Handle = Handle;
		Result.FailureTag = CombatTags::Failure_ActionUnsupported;
		return Result;
	}

	Result.bSuccess = true;
	Result.Handle = Handle;
	return Result;
}

FCombatThinkerResult UCombatThinkerSubsystem::CancelThinker(const FCombatThinkerHandle Handle)
{
	return FinishThinker(Handle, ECombatThinkerFinishReason::Cancelled, CombatTags::Order_Failure_Cancelled);
}

int32 UCombatThinkerSubsystem::CancelThinkersForAbility(
	ACombatUnitCharacter* Source,
	const FCombatEventId ActivationId)
{
	TArray<FCombatThinkerHandle> Handles;
	for (const TPair<uint64, FCombatThinkerRuntimeRecord>& Pair : ActiveThinkers)
	{
		if (Pair.Value.Spec.bCancelWithSourceAbility && Pair.Value.Spec.Source == Source
			&& Pair.Value.Spec.AbilityActivationId == ActivationId)
		{
			Handles.Add(Pair.Value.Handle);
		}
	}
	for (const FCombatThinkerHandle Handle : Handles) { CancelThinker(Handle); }
	return Handles.Num();
}

void UCombatThinkerSubsystem::NotifyThinkerEndPlay(const FCombatThinkerHandle Handle)
{
	if (!bDeinitializing)
	{
		FinishThinker(Handle, ECombatThinkerFinishReason::EndPlay, CombatTags::Order_Failure_Cancelled);
	}
}

bool UCombatThinkerSubsystem::IsThinkerActive(const FCombatThinkerHandle Handle) const
{
	const FCombatThinkerRuntimeRecord* Record = ActiveThinkers.Find(Handle.Key.Id);
	return Record && Record->Handle == Handle && Handle.Key.Generation == ThinkerGeneration;
}

void UCombatThinkerSubsystem::Deinitialize()
{
	bDeinitializing = true;
	TArray<FCombatThinkerHandle> Handles;
	for (const TPair<uint64, FCombatThinkerRuntimeRecord>& Pair : ActiveThinkers) { Handles.Add(Pair.Value.Handle); }
	for (const FCombatThinkerHandle Handle : Handles)
	{
		FinishThinker(Handle, ECombatThinkerFinishReason::EndPlay, CombatTags::Order_Failure_Cancelled);
	}
	ActiveThinkers.Reset();
	++ThinkerGeneration;
	if (ThinkerGeneration == 0) { ThinkerGeneration = 1; }
	Super::Deinitialize();
}

void UCombatThinkerSubsystem::HandlePulse(
	const FCombatThinkerHandle Handle,
	const FCombatScheduledTickContext& TickContext)
{
	FCombatThinkerRuntimeRecord* Record = ActiveThinkers.Find(Handle.Key.Id);
	if (!Record || Record->Handle != Handle || Handle.Key.Generation != ThinkerGeneration)
	{
		return;
	}
	ACombatUnitCharacter* Source = Record->Spec.Source;
	UCombatTargetingSubsystem* Targeting = GetWorld()->GetSubsystem<UCombatTargetingSubsystem>();
	if (!IsValid(Source) || !Targeting)
	{
		FinishThinker(Handle, ECombatThinkerFinishReason::Cancelled, CombatTags::Failure_Target_Invalid);
		return;
	}
	const TArray<ACombatUnitCharacter*> Targets = Targeting->QueryUnitsInRadius(
		Source, Record->Spec.Location, Record->Spec.Radius, Record->Spec.TargetingRules);
	int32 PulseTargets = 0;
	for (ACombatUnitCharacter* Target : Targets)
	{
		bool bAffected = false;
		FCombatDamageRequest Request;
		Request.Source = Source;
		Request.Target = Target;
		Request.Amount = Record->Spec.DamagePerPulse * TickContext.TickCount;
		Request.DamageType = Record->Spec.DamageType;
		Request.ParentEvent = Record->Spec.ParentEvent;
		Request.SourceContext = Record->Spec.SourceContext;
		if (GetWorld()->GetSubsystem<UCombatDamageSubsystem>()->DealDamage(Request).bSuccess)
		{
			bAffected = true;
		}
		if (Record->Spec.ModifierPerPulse)
		{
			FCombatModifierApplyRequest ModifierRequest;
			ModifierRequest.Source = Source;
			ModifierRequest.ModifierData = Record->Spec.ModifierPerPulse;
			ModifierRequest.DurationOverride = Record->Spec.ModifierDurationOverride;
			bAffected |= Target->GetCombatModifierComponent()->ApplyModifier(ModifierRequest).bSuccess;
		}
		PulseTargets += bAffected ? 1 : 0;
	}
	Record->AffectedTargetCount += PulseTargets;
	EmitThinkerLog(*Record, CombatTags::Event_Combat_ThinkerPulse, PulseTargets, FGameplayTag());
	if (Record->Spec.Duration <= 0.0f)
	{
		FinishThinker(Handle, ECombatThinkerFinishReason::Completed, FGameplayTag());
	}
}

void UCombatThinkerSubsystem::HandleFinished(
	const FCombatThinkerHandle Handle,
	const FCombatScheduledTickContext& TickContext)
{
	(void)TickContext;
	FinishThinker(Handle, ECombatThinkerFinishReason::Completed, FGameplayTag());
}

FCombatThinkerResult UCombatThinkerSubsystem::FinishThinker(
	const FCombatThinkerHandle Handle,
	const ECombatThinkerFinishReason Reason,
	const FGameplayTag FailureTag)
{
	FCombatThinkerResult Result;
	FCombatThinkerRuntimeRecord* Record = ActiveThinkers.Find(Handle.Key.Id);
	if (!Record || Record->Handle != Handle || Handle.Key.Generation != ThinkerGeneration)
	{
		Result.Handle = Handle;
		Result.FailureTag = CombatTags::Failure_Thinker_StaleHandle;
		return Result;
	}
	const FCombatThinkerRuntimeRecord Snapshot = *Record;
	ACombatThinker* Actor = Record->Actor.Get();
	if (UCombatSchedulerSubsystem* Scheduler = GetWorld()->GetSubsystem<UCombatSchedulerSubsystem>())
	{
		Scheduler->Cancel(Record->PulseSchedule);
		Scheduler->Cancel(Record->FinishSchedule);
	}
	ActiveThinkers.Remove(Handle.Key.Id);
	Result.bSuccess = true;
	Result.Handle = Handle;
	Result.FinishReason = Reason;
	Result.AffectedTargetCount = Snapshot.AffectedTargetCount;
	Result.FailureTag = FailureTag;
	LastFinishedResult = Result;
	EmitThinkerLog(Snapshot, CombatTags::Event_Combat_ThinkerFinished, 0, FailureTag);
	ThinkerFinishedDelegate.Broadcast(LastFinishedResult);
	if (Actor && !Actor->IsActorBeingDestroyed())
	{
		Actor->PrepareForSubsystemDestroy();
		Actor->Destroy();
	}
	return Result;
}

void UCombatThinkerSubsystem::EmitThinkerLog(
	const FCombatThinkerRuntimeRecord& Record,
	const FGameplayTag EventType,
	const int32 PulseTargets,
	const FGameplayTag FailureTag) const
{
	UCombatEventSubsystem* Events = GetWorld() ? GetWorld()->GetSubsystem<UCombatEventSubsystem>() : nullptr;
	if (!Events || !IsValid(Record.Spec.Source)) { return; }
	FCombatLogRecord Log;
	Log.Context = Record.Spec.ParentEvent;
	Log.EventType = EventType;
	Log.FailureTag = FailureTag;
	Log.Source = Record.Spec.SourceContext;
	Log.SourceActorId = Record.Spec.Source->GetUniqueID();
	Log.UnitLifeGeneration = Record.Handle.Key.LifeGeneration;
	Log.RequestedAmount = Record.Spec.DamagePerPulse;
	Log.AppliedAmount = PulseTargets;
	Log.Diagnostic = FString::Printf(TEXT("Thinker=%s Radius=%.2f TotalTargets=%d"),
		*Record.Handle.ToString(), Record.Spec.Radius, Record.AffectedTargetCount);
	Events->Emit(Log);
}
