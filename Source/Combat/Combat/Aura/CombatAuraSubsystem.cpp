#include "Combat/Aura/CombatAuraSubsystem.h"

#include "Combat/Ability/CombatAbilitySystemComponent.h"
#include "Combat/Core/CombatNumericPolicy.h"
#include "Combat/Core/CombatTags.h"
#include "Combat/Data/CombatDefinitionData.h"
#include "Combat/Modifiers/CombatModifierComponent.h"
#include "Combat/Targeting/CombatTargetingSubsystem.h"
#include "Combat/Unit/CombatUnitCharacter.h"

FCombatAuraResult UCombatAuraSubsystem::StartAura(const FCombatAuraSpec& Spec)
{
	FCombatAuraResult Result;
	if (!IsValid(Spec.Owner) || !Spec.Owner->HasAuthority() || Spec.Owner->GetWorld() != GetWorld())
	{
		Result.FailureTag = CombatTags::Failure_Authority;
		return Result;
	}
	if (Spec.Owner->GetLifeState() != ECombatLifeState::Alive || !Spec.ChildModifierData
		|| !FMath::IsFinite(Spec.Radius) || Spec.Radius < 0.0f
		|| !FMath::IsFinite(Spec.ReconcileInterval) || Spec.ReconcileInterval <= 0.0f
		|| !FMath::IsFinite(Spec.ChildDurationOverride) || Spec.ChildDurationOverride < -1.0f)
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

	FCombatAuraRuntimeRecord Record;
	Record.Handle.Key.Id = NextAuraId++;
	Record.Handle.Key.Generation = AuraGeneration;
	Record.Handle.Key.LifeGeneration = Spec.Owner->GetLifeGeneration();
	Record.Spec = Spec;
	if (!Record.Spec.ParentEvent.IsValid())
	{
		if (UCombatEventSubsystem* Events = GetWorld()->GetSubsystem<UCombatEventSubsystem>())
		{
			Record.Spec.ParentEvent = Events->CreateRootEvent();
		}
	}
	const FCombatAuraHandle Handle = Record.Handle;
	Record.ReconcileSchedule = Scheduler->ScheduleRepeating(
		this, Spec.ReconcileInterval, Spec.ReconcileInterval, 0, ECombatCatchUpPolicy::Coalesce,
		FCombatScheduledDelegate::CreateWeakLambda(this,
			[this, Handle](const FCombatScheduledTickContext& Context) { HandleReconcile(Handle, Context); }));
	if (!Record.ReconcileSchedule.IsValid())
	{
		Result.Handle = Handle;
		Result.FailureTag = CombatTags::Failure_ActionUnsupported;
		return Result;
	}
	ActiveAuras.Add(Handle.Key.Id, MoveTemp(Record));
	if (const FCombatAuraRuntimeRecord* Stored = ActiveAuras.Find(Handle.Key.Id))
	{
		EmitAuraLog(*Stored, CombatTags::Event_Combat_AuraStarted, FGameplayTag());
	}
	ReconcileNow(Handle);
	const FCombatAuraRuntimeRecord* Stored = ActiveAuras.Find(Handle.Key.Id);
	if (!Stored || Stored->Handle != Handle)
	{
		Result.Handle = Handle;
		Result.FailureTag = CombatTags::Failure_Aura_StaleHandle;
		return Result;
	}

	Result.bSuccess = true;
	Result.Handle = Handle;
	Result.ChildCount = Stored->Children.Num();
	return Result;
}

FCombatAuraResult UCombatAuraSubsystem::CancelAura(const FCombatAuraHandle Handle)
{
	if (bMutatingRegistry)
	{
		FCombatAuraResult DeferredResult;
		DeferredResult.Handle = Handle;
		if (const FCombatAuraRuntimeRecord* Record = ActiveAuras.Find(Handle.Key.Id);
			Record && Record->Handle == Handle)
		{
			DeferredResult.bSuccess = true;
			DeferredResult.ChildCount = Record->Children.Num();
			DeferredCancelHandles.AddUnique(Handle);
		}
		else
		{
			DeferredResult.FailureTag = CombatTags::Failure_Aura_StaleHandle;
		}
		return DeferredResult;
	}
	bMutatingRegistry = true;
	FCombatAuraResult Result = FinishAura(Handle, ECombatAuraFinishReason::Cancelled, FGameplayTag());
	bMutatingRegistry = false;
	FlushDeferredCancels();
	return Result;
}

bool UCombatAuraSubsystem::ReconcileNow(const FCombatAuraHandle Handle)
{
	if (bMutatingRegistry || bDeinitializing)
	{
		return false;
	}
	bMutatingRegistry = true;
	FCombatAuraRuntimeRecord* Record = ActiveAuras.Find(Handle.Key.Id);
	const bool bResult = Record && Record->Handle == Handle && Handle.Key.Generation == AuraGeneration
		? ReconcileRecord(*Record) : false;
	bMutatingRegistry = false;
	FlushDeferredCancels();
	return bResult;
}

void UCombatAuraSubsystem::NotifyUnitChanged(ACombatUnitCharacter* Unit)
{
	// 施加或移除子效果会同步改变状态标签，可能再次通知光环。当前检查尚未完成时不递归查询，新变化由下一轮检查处理。
	if (!IsValid(Unit) || bDeinitializing || bMutatingRegistry)
	{
		return;
	}
	TArray<FCombatAuraHandle> Handles;
	for (const TPair<uint64, FCombatAuraRuntimeRecord>& Pair : ActiveAuras)
	{
		Handles.Add(Pair.Value.Handle);
	}
	for (const FCombatAuraHandle Handle : Handles)
	{
		ReconcileNow(Handle);
	}
}

void UCombatAuraSubsystem::NotifyUnitEndPlay(ACombatUnitCharacter* Unit)
{
	// 子效果回调可能销毁单位；在当前遍历中直接删除光环会破坏记录引用，因此延后到下一轮有效性检查或世界清理。
	if (!Unit || bDeinitializing || bMutatingRegistry)
	{
		return;
	}
	bMutatingRegistry = true;
	TArray<FCombatAuraHandle> OwnedHandles;
	for (const TPair<uint64, FCombatAuraRuntimeRecord>& Pair : ActiveAuras)
	{
		if (Pair.Value.Spec.Owner == Unit)
		{
			OwnedHandles.Add(Pair.Value.Handle);
		}
		else if (FCombatAuraRuntimeRecord* Record = ActiveAuras.Find(Pair.Key))
		{
			if (const FCombatAuraChildRecord* Child = Record->Children.Find(Unit))
			{
				Unit->GetCombatModifierComponent()->RemoveModifier(Child->ModifierHandle);
				Record->Children.Remove(Unit);
			}
		}
	}
	for (const FCombatAuraHandle Handle : OwnedHandles)
	{
		FinishAura(Handle, ECombatAuraFinishReason::EndPlay, FGameplayTag());
	}
	bMutatingRegistry = false;
	FlushDeferredCancels();
}

bool UCombatAuraSubsystem::IsAuraActive(const FCombatAuraHandle Handle) const
{
	const FCombatAuraRuntimeRecord* Record = ActiveAuras.Find(Handle.Key.Id);
	return Record && Record->Handle == Handle && Handle.Key.Generation == AuraGeneration;
}

int32 UCombatAuraSubsystem::GetTotalChildCount() const
{
	int32 Total = 0;
	for (const TPair<uint64, FCombatAuraRuntimeRecord>& Pair : ActiveAuras)
	{
		Total += Pair.Value.Children.Num();
	}
	return Total;
}

int32 UCombatAuraSubsystem::GetChildCount(const FCombatAuraHandle Handle) const
{
	const FCombatAuraRuntimeRecord* Record = ActiveAuras.Find(Handle.Key.Id);
	return Record && Record->Handle == Handle ? Record->Children.Num() : 0;
}

void UCombatAuraSubsystem::Deinitialize()
{
	bDeinitializing = true;
	bMutatingRegistry = true;
	TArray<FCombatAuraHandle> Handles;
	for (const TPair<uint64, FCombatAuraRuntimeRecord>& Pair : ActiveAuras)
	{
		Handles.Add(Pair.Value.Handle);
	}
	for (const FCombatAuraHandle Handle : Handles)
	{
		FinishAura(Handle, ECombatAuraFinishReason::EndPlay, FGameplayTag());
	}
	ActiveAuras.Reset();
	DeferredCancelHandles.Reset();
	bMutatingRegistry = false;
	++AuraGeneration;
	if (AuraGeneration == 0) { AuraGeneration = 1; }
	Super::Deinitialize();
}

void UCombatAuraSubsystem::HandleReconcile(
	const FCombatAuraHandle Handle,
	const FCombatScheduledTickContext& TickContext)
{
	(void)TickContext;
	ReconcileNow(Handle);
}

bool UCombatAuraSubsystem::ReconcileRecord(FCombatAuraRuntimeRecord& Record)
{
	ACombatUnitCharacter* Owner = Record.Spec.Owner;
	if (!IsValid(Owner) || Owner->GetLifeState() != ECombatLifeState::Alive
		|| Owner->GetLifeGeneration() != Record.Handle.Key.LifeGeneration)
	{
		const FCombatAuraHandle Handle = Record.Handle;
		FinishAura(Handle, ECombatAuraFinishReason::OwnerInvalid, CombatTags::Failure_Life_NotAlive);
		return false;
	}
	const UCombatAbilitySystemComponent* OwnerAsc = Owner->GetCombatAbilitySystemComponent();
	if (Record.Spec.bDisabledByBreak && OwnerAsc
		&& OwnerAsc->HasMatchingGameplayTag(CombatTags::State_Broken))
	{
		RemoveAllChildren(Record);
		EmitAuraLog(Record, CombatTags::Event_Combat_AuraReconciled, CombatTags::Failure_Aura_Broken);
		return true;
	}
	UCombatTargetingSubsystem* Targeting = GetWorld()->GetSubsystem<UCombatTargetingSubsystem>();
	if (!Targeting)
	{
		return false;
	}
	const TArray<ACombatUnitCharacter*> Targets = Targeting->QueryUnitsInRadius(
		Owner, Owner->GetActorLocation(), Record.Spec.Radius, Record.Spec.TargetingRules);
	TSet<TWeakObjectPtr<ACombatUnitCharacter>> Desired;
	for (ACombatUnitCharacter* Target : Targets)
	{
		if (IsValid(Target)) { Desired.Add(Target); }
	}

	// 先清掉已离开、已换生命或已丢失效果的记录，后续才能为仍合格的目标补回效果。
	TArray<TWeakObjectPtr<ACombatUnitCharacter>> ExistingTargets;
	Record.Children.GetKeys(ExistingTargets);
	for (const TWeakObjectPtr<ACombatUnitCharacter>& WeakTarget : ExistingTargets)
	{
		ACombatUnitCharacter* Target = WeakTarget.Get();
		FCombatAuraChildRecord* Child = Record.Children.Find(WeakTarget);
		const bool bStillValid = Target && Child && Desired.Contains(WeakTarget)
			&& Child->TargetLifeGeneration == Target->GetLifeGeneration()
			&& Target->GetCombatModifierComponent()->FindRuntime(Child->ModifierHandle);
		if (!bStillValid)
		{
			if (Target && Child)
			{
				Target->GetCombatModifierComponent()->RemoveModifier(Child->ModifierHandle);
			}
			Record.Children.Remove(WeakTarget);
		}
	}

	// 有效子效果不重复施加，以免每轮累加层数或把有限持续时间无限续期。
	for (ACombatUnitCharacter* Target : Targets)
	{
		if (!Target || Record.Children.Contains(Target))
		{
			continue;
		}
		FCombatModifierApplyRequest Request;
		Request.Source = Owner;
		Request.ModifierData = Record.Spec.ChildModifierData;
		Request.DurationOverride = Record.Spec.ChildDurationOverride;
		const FCombatModifierApplyResult ApplyResult = Target->GetCombatModifierComponent()->ApplyModifier(Request);
		if (ApplyResult.bSuccess && ApplyResult.Handle.IsValid())
		{
			FCombatAuraChildRecord Child;
			Child.ModifierHandle = ApplyResult.Handle;
			Child.TargetLifeGeneration = Target->GetLifeGeneration();
			Record.Children.Add(Target, Child);
		}
	}
	EmitAuraLog(Record, CombatTags::Event_Combat_AuraReconciled, FGameplayTag());
	return true;
}

void UCombatAuraSubsystem::RemoveAllChildren(FCombatAuraRuntimeRecord& Record)
{
	for (const TPair<TWeakObjectPtr<ACombatUnitCharacter>, FCombatAuraChildRecord>& Pair : Record.Children)
	{
		if (ACombatUnitCharacter* Target = Pair.Key.Get())
		{
			Target->GetCombatModifierComponent()->RemoveModifier(Pair.Value.ModifierHandle);
		}
	}
	Record.Children.Reset();
}

FCombatAuraResult UCombatAuraSubsystem::FinishAura(
	const FCombatAuraHandle Handle,
	const ECombatAuraFinishReason Reason,
	const FGameplayTag FailureTag)
{
	FCombatAuraResult Result;
	FCombatAuraRuntimeRecord* Record = ActiveAuras.Find(Handle.Key.Id);
	if (!Record || Record->Handle != Handle || Handle.Key.Generation != AuraGeneration)
	{
		Result.Handle = Handle;
		Result.FailureTag = CombatTags::Failure_Aura_StaleHandle;
		return Result;
	}
	FCombatAuraRuntimeRecord Snapshot = *Record;
	Result.ChildCount = Record->Children.Num();
	RemoveAllChildren(*Record);
	if (UCombatSchedulerSubsystem* Scheduler = GetWorld()->GetSubsystem<UCombatSchedulerSubsystem>())
	{
		Scheduler->Cancel(Record->ReconcileSchedule);
	}
	ActiveAuras.Remove(Handle.Key.Id);
	Result.bSuccess = true;
	Result.Handle = Handle;
	Result.FinishReason = Reason;
	Result.FailureTag = FailureTag;
	LastFinishedResult = Result;
	EmitAuraLog(Snapshot, CombatTags::Event_Combat_AuraFinished, FailureTag);
	AuraFinishedDelegate.Broadcast(LastFinishedResult);
	return Result;
}

void UCombatAuraSubsystem::FlushDeferredCancels()
{
	if (bMutatingRegistry || bDeinitializing || DeferredCancelHandles.IsEmpty())
	{
		return;
	}
	TArray<FCombatAuraHandle> Handles = MoveTemp(DeferredCancelHandles);
	DeferredCancelHandles.Reset();
	for (const FCombatAuraHandle Handle : Handles)
	{
		if (IsAuraActive(Handle))
		{
			CancelAura(Handle);
		}
	}
}

void UCombatAuraSubsystem::EmitAuraLog(
	const FCombatAuraRuntimeRecord& Record,
	const FGameplayTag EventType,
	const FGameplayTag FailureTag) const
{
	UCombatEventSubsystem* Events = GetWorld() ? GetWorld()->GetSubsystem<UCombatEventSubsystem>() : nullptr;
	ACombatUnitCharacter* Owner = Record.Spec.Owner;
	if (!Events || !Owner)
	{
		return;
	}
	FCombatLogRecord Log;
	Log.Context = Record.Spec.ParentEvent.IsValid() ? Record.Spec.ParentEvent : Events->CreateRootEvent();
	Log.EventType = EventType;
	Log.FailureTag = FailureTag;
	Log.Source = Record.Spec.SourceContext;
	Log.SourceActorId = Owner->GetUniqueID();
	Log.TargetActorId = Owner->GetUniqueID();
	Log.UnitLifeGeneration = Owner->GetLifeGeneration();
	Log.RequestedAmount = Record.Spec.Radius;
	Log.AppliedAmount = static_cast<float>(Record.Children.Num());
	Log.Diagnostic = FString::Printf(TEXT("Aura=%s Children=%d Child=%s"),
		*Record.Handle.ToString(), Record.Children.Num(), *GetNameSafe(Record.Spec.ChildModifierData));
	Events->Emit(Log);
}
