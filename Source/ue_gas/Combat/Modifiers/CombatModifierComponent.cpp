#include "Combat/Modifiers/CombatModifierComponent.h"

#include "GameplayEffect.h"

#include "Combat/Ability/CombatAbilitySystemComponent.h"
#include "Combat/Attributes/CombatAttributeSet.h"
#include "Combat/Core/CombatNumericPolicy.h"
#include "Combat/Core/CombatTags.h"
#include "Combat/Log/CombatEventSubsystem.h"
#include "Combat/Modifiers/CombatModifierRuntime.h"
#include "Combat/Scheduling/CombatSchedulerSubsystem.h"
#include "Combat/Unit/CombatUnitCharacter.h"

UCombatModifierComponent::UCombatModifierComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

FCombatModifierApplyResult UCombatModifierComponent::ApplyModifier(const FCombatModifierApplyRequest& Request)
{
	FCombatModifierApplyResult Result;
	ACombatUnitCharacter* Target = GetOwnerUnit();
	if (!Target || !Target->HasAuthority())
	{
		Result.FailureTag = CombatTags::Failure_Authority;
		return Result;
	}
	if (!Request.ModifierData || !IsValid(Request.Source) || Target->GetLifeState() != ECombatLifeState::Alive)
	{
		Result.FailureTag = CombatTags::Failure_Life_NotAlive;
		return Result;
	}
	if (!FMath::IsFinite(Request.DurationOverride) || Request.DurationOverride < -1.0f)
	{
		Result.FailureTag = CombatTags::Failure_InvalidNumber;
		return Result;
	}

	// Hook 内不修改 ActiveModifiers；FIFO 在当前最外层阶段结束后执行真实 Apply。
	if (DeferredOperations.IsInPhase())
	{
		TWeakObjectPtr<UCombatModifierComponent> WeakThis(this);
		DeferredOperations.Enqueue([WeakThis, Request]()
		{
			if (WeakThis.IsValid()) { WeakThis->ApplyModifier(Request); }
		});
		Result.bSuccess = true;
		return Result;
	}

	const float EffectiveDuration = CalculateEffectiveDuration(Request);
	if (UCombatModifierRuntime* Existing = FindRefreshCandidate(Request))
	{
		return RefreshModifier(*Existing, EffectiveDuration);
	}
	return ApplyNewModifier(Request, EffectiveDuration);
}

FCombatModifierApplyResult UCombatModifierComponent::ApplyNewModifier(
	const FCombatModifierApplyRequest& Request,
	const float EffectiveDuration)
{
	FCombatModifierApplyResult Result;
	ACombatUnitCharacter* Target = GetOwnerUnit();
	UCombatAbilitySystemComponent* TargetAsc = Target ? Target->GetCombatAbilitySystemComponent() : nullptr;
	if (!TargetAsc)
	{
		Result.FailureTag = CombatTags::Failure_Target_Invalid;
		return Result;
	}

	UClass* RuntimeClass = Request.ModifierData->RuntimeClass
		? Request.ModifierData->RuntimeClass.Get() : UCombatModifierRuntime::StaticClass();
	if (!RuntimeClass || RuntimeClass->HasAnyClassFlags(CLASS_Abstract))
	{
		Result.FailureTag = CombatTags::Failure_ActionUnsupported;
		return Result;
	}

	// ActiveGE 只承担属性与可计数 Tag；周期和 Runtime 状态由本组件统一管理。
	UGameplayEffect* EffectDefinition = NewObject<UGameplayEffect>(this);
	EffectDefinition->DurationPolicy = EGameplayEffectDurationType::Infinite;
	// Runtime 层数与 ActiveGE 层数共享同一来源聚合规则，避免出现第二套 stack 权威。
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	EffectDefinition->StackingType = EGameplayEffectStackingType::AggregateBySource;
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
	EffectDefinition->StackLimitCount = FMath::Max(1, Request.ModifierData->MaxStacks);
	EffectDefinition->bFactorInStackCount = true;
	for (const FCombatModifierAttributeChange& Change : Request.ModifierData->AttributeChanges)
	{
		if (!Change.Attribute.IsValid() || !FMath::IsFinite(Change.Magnitude))
		{
			Result.FailureTag = CombatTags::Failure_InvalidNumber;
			return Result;
		}
		FGameplayModifierInfo& Modifier = EffectDefinition->Modifiers.AddDefaulted_GetRef();
		Modifier.Attribute = Change.Attribute;
		Modifier.ModifierOp = Change.ModifierOp;
		Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(Change.Magnitude));
	}

	UCombatAbilitySystemComponent* SourceAsc = Request.Source->GetCombatAbilitySystemComponent();
	FGameplayEffectContextHandle EffectContext = SourceAsc ? SourceAsc->MakeEffectContext() : TargetAsc->MakeEffectContext();
	FGameplayEffectSpec EffectSpec(EffectDefinition, EffectContext, 1.0f);
	EffectSpec.DynamicGrantedTags.AppendTags(Request.ModifierData->GrantedTags);
	const FActiveGameplayEffectHandle ActiveEffectHandle = TargetAsc->ApplyGameplayEffectSpecToSelf(EffectSpec);
	if (!ActiveEffectHandle.IsValid())
	{
		Result.FailureTag = CombatTags::Failure_ActionUnsupported;
		return Result;
	}

	UCombatModifierRuntime* Runtime = NewObject<UCombatModifierRuntime>(this, RuntimeClass);
	if (!Runtime)
	{
		TargetAsc->RemoveActiveGameplayEffect(ActiveEffectHandle);
		Result.FailureTag = CombatTags::Failure_ActionUnsupported;
		return Result;
	}

	Runtime->OwningComponent = this;
	Runtime->ModifierData = Request.ModifierData;
	Runtime->SourceUnit = Request.Source;
	Runtime->TargetUnit = Target;
	Runtime->Handle.Key.Id = NextHandleId++;
	Runtime->Handle.Key.Generation = 1;
	Runtime->Handle.Key.LifeGeneration = Target->GetLifeGeneration();
	Runtime->Priority = Request.ModifierData->Priority;
	Runtime->ApplySequence = NextApplySequence++;
	Runtime->StackCount = 1;
	Runtime->ExpireAt = EffectiveDuration > 0.0f && GetWorld()
		? GetWorld()->GetTimeSeconds() + EffectiveDuration : 0.0;
	Runtime->ActiveEffectHandle = ActiveEffectHandle;
	Runtime->EffectDefinition = EffectDefinition;
	Runtime->bActive = true;

	RuntimeEffectDefinitions.Add(EffectDefinition);
	ActiveModifiers.Add(Runtime);
	ScheduleRuntime(*Runtime, true);
	Runtime->OnCreated();
	EmitModifierLog(*Runtime, false);

	Result.bSuccess = true;
	Result.Handle = Runtime->Handle;
	return Result;
}

FCombatModifierApplyResult UCombatModifierComponent::RefreshModifier(
	UCombatModifierRuntime& Runtime,
	const float EffectiveDuration)
{
	FCombatModifierApplyResult Result;
	const UCombatModifierData* Data = Runtime.ModifierData;
	if (!Data || !Runtime.bActive)
	{
		Result.FailureTag = CombatTags::Failure_Target_Invalid;
		return Result;
	}
	ACombatUnitCharacter* Target = GetOwnerUnit();
	UCombatAbilitySystemComponent* TargetAsc = Target ? Target->GetCombatAbilitySystemComponent() : nullptr;
	UCombatAbilitySystemComponent* SourceAsc = Runtime.GetSourceUnit()
		? Runtime.GetSourceUnit()->GetCombatAbilitySystemComponent() : nullptr;
	if (!TargetAsc || !SourceAsc || !Runtime.EffectDefinition)
	{
		Result.FailureTag = CombatTags::Failure_Target_Invalid;
		return Result;
	}
	FGameplayEffectSpec RefreshSpec(Runtime.EffectDefinition, SourceAsc->MakeEffectContext(), 1.0f);
	RefreshSpec.DynamicGrantedTags.AppendTags(Data->GrantedTags);
	const FActiveGameplayEffectHandle RefreshedHandle = TargetAsc->ApplyGameplayEffectSpecToSelf(RefreshSpec);
	if (!RefreshedHandle.WasSuccessfullyApplied() || RefreshedHandle != Runtime.ActiveEffectHandle)
	{
		Result.FailureTag = CombatTags::Failure_ActionUnsupported;
		return Result;
	}

	Runtime.StackCount = FMath::Clamp(Runtime.StackCount + 1, 1, FMath::Max(1, Data->MaxStacks));
	Runtime.ExpireAt = EffectiveDuration > 0.0f && GetWorld()
		? GetWorld()->GetTimeSeconds() + EffectiveDuration : 0.0;
	const bool bResetThink = Data->RefreshPolicy == ECombatModifierRefreshPolicy::ResetInterval;
	ScheduleRuntime(Runtime, bResetThink);
	Runtime.OnRefreshed();
	EmitModifierLog(Runtime, false, true);

	Result.bSuccess = true;
	Result.bRefreshed = true;
	Result.Handle = Runtime.Handle;
	return Result;
}

bool UCombatModifierComponent::RemoveModifier(const FCombatModifierHandle Handle)
{
	if (!Handle.IsValid())
	{
		return false;
	}
	if (DeferredOperations.IsInPhase())
	{
		TWeakObjectPtr<UCombatModifierComponent> WeakThis(this);
		DeferredOperations.Enqueue([WeakThis, Handle]()
		{
			if (WeakThis.IsValid()) { WeakThis->RemoveModifierImmediate(Handle); }
		});
		return true;
	}
	return RemoveModifierImmediate(Handle);
}

bool UCombatModifierComponent::RemoveModifierImmediate(const FCombatModifierHandle Handle, const bool bCallDestroyed)
{
	for (int32 Index = 0; Index < ActiveModifiers.Num(); ++Index)
	{
		UCombatModifierRuntime* Runtime = ActiveModifiers[Index];
		if (!Runtime || Runtime->Handle != Handle || !Runtime->bActive)
		{
			continue;
		}

		Runtime->bActive = false;
		CancelRuntimeSchedules(*Runtime);
		if (bCallDestroyed)
		{
			Runtime->OnDestroyed();
			EmitModifierLog(*Runtime, true);
		}
		if (ACombatUnitCharacter* Target = GetOwnerUnit())
		{
			if (UCombatAbilitySystemComponent* Asc = Target->GetCombatAbilitySystemComponent())
			{
				Asc->RemoveActiveGameplayEffect(Runtime->ActiveEffectHandle);
			}
		}
		Runtime->ActiveEffectHandle.Invalidate();
		ActiveModifiers.RemoveAt(Index);
		// ActiveSpec 已删除后同步释放动态定义的强引用，避免长期累计无用 UObject。
		RuntimeEffectDefinitions.RemoveSingleSwap(Runtime->EffectDefinition);
		Runtime->EffectDefinition = nullptr;
		return true;
	}
	return false;
}

int32 UCombatModifierComponent::Dispel(const ECombatDispelStrength Strength, const bool bDebuffsOnly)
{
	if (DeferredOperations.IsInPhase())
	{
		TWeakObjectPtr<UCombatModifierComponent> WeakThis(this);
		DeferredOperations.Enqueue([WeakThis, Strength, bDebuffsOnly]()
		{
			if (WeakThis.IsValid()) { WeakThis->Dispel(Strength, bDebuffsOnly); }
		});
		return 0;
	}

	int32 Removed = 0;
	const TArray<UCombatModifierRuntime*> Snapshot = MakeSortedSnapshot();
	for (UCombatModifierRuntime* Runtime : Snapshot)
	{
		const UCombatModifierData* Data = Runtime ? Runtime->GetModifierData() : nullptr;
		if (!Data || (bDebuffsOnly && !Data->bIsDebuff) || Data->DispelRule == ECombatModifierDispelRule::NotDispellable)
		{
			continue;
		}
		const bool bAllowed = Strength == ECombatDispelStrength::Strong
			|| Data->DispelRule == ECombatModifierDispelRule::Basic;
		if (bAllowed && RemoveModifierImmediate(Runtime->GetHandle()))
		{
			++Removed;
		}
	}
	return Removed;
}

UCombatModifierRuntime* UCombatModifierComponent::FindRuntime(const FCombatModifierHandle Handle) const
{
	for (UCombatModifierRuntime* Runtime : ActiveModifiers)
	{
		if (Runtime && Runtime->IsActive() && Runtime->GetHandle() == Handle)
		{
			return Runtime;
		}
	}
	return nullptr;
}

UCombatModifierRuntime* UCombatModifierComponent::FindRefreshCandidate(const FCombatModifierApplyRequest& Request) const
{
	for (UCombatModifierRuntime* Runtime : ActiveModifiers)
	{
		if (Runtime && Runtime->IsActive() && Runtime->GetModifierData() == Request.ModifierData
			&& Runtime->GetSourceUnit() == Request.Source)
		{
			return Runtime;
		}
	}
	return nullptr;
}

float UCombatModifierComponent::CalculateEffectiveDuration(const FCombatModifierApplyRequest& Request) const
{
	float Duration = Request.DurationOverride >= 0.0f ? Request.DurationOverride : Request.ModifierData->Duration;
	if (Duration <= 0.0f || !Request.ModifierData->bIsDebuff
		|| !Request.ModifierData->bDurationAffectedByStatusResistance)
	{
		return FMath::Max(0.0f, Duration);
	}
	const ACombatUnitCharacter* Target = GetOwnerUnit();
	const UCombatAbilitySystemComponent* Asc = Target ? Target->GetCombatAbilitySystemComponent() : nullptr;
	const float Resistance = Asc ? Asc->GetNumericAttribute(UCombatAttributeSet::GetStatusResistancePctAttribute()) : 0.0f;
	return Duration * (1.0f - FCombatNumericPolicyV1::ClampReduction(Resistance));
}

void UCombatModifierComponent::ScheduleRuntime(UCombatModifierRuntime& Runtime, const bool bResetThinkPhase)
{
	if (bHooksPaused || !Runtime.bActive || !GetWorld())
	{
		return;
	}
	UCombatSchedulerSubsystem* Scheduler = GetWorld()->GetSubsystem<UCombatSchedulerSubsystem>();
	const UCombatModifierData* Data = Runtime.ModifierData;
	if (!Scheduler || !Data)
	{
		return;
	}

	if (Data->ThinkInterval > 0.0f && (bResetThinkPhase || !Scheduler->IsHandleActive(Runtime.ThinkSchedule)))
	{
		if (Runtime.ThinkSchedule.IsValid()) { Scheduler->Cancel(Runtime.ThinkSchedule); }
		const FCombatModifierHandle Handle = Runtime.Handle;
		Runtime.ThinkSchedule = Scheduler->ScheduleRepeating(
			this, Data->ThinkInterval, Data->ThinkInterval, Data->Priority,
			ECombatCatchUpPolicy::ExecuteAllBounded,
			FCombatScheduledDelegate::CreateWeakLambda(this,
				[this, Handle](const FCombatScheduledTickContext& Context) { HandleRuntimeThink(Handle, Context); }));
	}

	if (Runtime.ExpireSchedule.IsValid()) { Scheduler->Cancel(Runtime.ExpireSchedule); }
	Runtime.ExpireSchedule = FCombatScheduleHandle();
	if (Runtime.ExpireAt > 0.0)
	{
		const double Delay = FMath::Max(0.0, Runtime.ExpireAt - GetWorld()->GetTimeSeconds());
		const FCombatModifierHandle Handle = Runtime.Handle;
		Runtime.ExpireSchedule = Scheduler->ScheduleOnce(
			this, Delay, Data->Priority - 1,
			FCombatScheduledDelegate::CreateWeakLambda(this,
				[this, Handle](const FCombatScheduledTickContext& Context) { HandleRuntimeExpired(Handle, Context); }));
	}
}

void UCombatModifierComponent::CancelRuntimeSchedules(UCombatModifierRuntime& Runtime)
{
	if (UCombatSchedulerSubsystem* Scheduler = GetWorld() ? GetWorld()->GetSubsystem<UCombatSchedulerSubsystem>() : nullptr)
	{
		Scheduler->Cancel(Runtime.ThinkSchedule);
		Scheduler->Cancel(Runtime.ExpireSchedule);
	}
	Runtime.ThinkSchedule = FCombatScheduleHandle();
	Runtime.ExpireSchedule = FCombatScheduleHandle();
}

void UCombatModifierComponent::HandleRuntimeThink(
	const FCombatModifierHandle Handle,
	const FCombatScheduledTickContext& TickContext)
{
	UCombatModifierRuntime* Runtime = FindRuntime(Handle);
	const UCombatModifierData* Data = Runtime ? Runtime->GetModifierData() : nullptr;
	if (bHooksPaused || !Runtime || !Data)
	{
		return;
	}
	constexpr double BoundaryTolerance = 1.0e-6;
	if (Runtime->GetExpireAt() > 0.0)
	{
		const double Difference = TickContext.ScheduledTime - Runtime->GetExpireAt();
		if (Difference > BoundaryTolerance || (FMath::Abs(Difference) <= BoundaryTolerance && !Data->bTickOnExpire))
		{
			return;
		}
	}
	Runtime->OnThink(TickContext);
}

void UCombatModifierComponent::HandleRuntimeExpired(
	const FCombatModifierHandle Handle,
	const FCombatScheduledTickContext& TickContext)
{
	(void)TickContext;
	if (!bHooksPaused)
	{
		RemoveModifier(Handle);
	}
}

TArray<UCombatModifierRuntime*> UCombatModifierComponent::MakeSortedSnapshot() const
{
	TArray<UCombatModifierRuntime*> Snapshot;
	if (bHooksPaused)
	{
		return Snapshot;
	}
	for (UCombatModifierRuntime* Runtime : ActiveModifiers)
	{
		if (Runtime && Runtime->IsActive()) { Snapshot.Add(Runtime); }
	}
	Snapshot.StableSort([](const UCombatModifierRuntime& A, const UCombatModifierRuntime& B)
	{
		return A.GetPriority() != B.GetPriority()
			? A.GetPriority() > B.GetPriority()
			: A.GetApplySequence() < B.GetApplySequence();
	});
	return Snapshot;
}

// 每个 Hook 函数都在同一 Deferred 阶段内遍历稳定快照，阶段退出后才提交结构修改。
#define COMBAT_EXECUTE_MUTABLE_HOOK(FunctionName, RuntimeCall, PhaseName, EventType) \
	void UCombatModifierComponent::FunctionName(EventType& Event) \
	{ \
		DeferredOperations.BeginPhase(PhaseName, Event.Context.EventId); \
		for (UCombatModifierRuntime* Runtime : MakeSortedSnapshot()) { Runtime->RuntimeCall(Event); } \
		DeferredOperations.EndPhase(); \
	}

#define COMBAT_EXECUTE_CONST_HOOK(FunctionName, RuntimeCall, PhaseName, EventType) \
	void UCombatModifierComponent::FunctionName(const EventType& Event) \
	{ \
		DeferredOperations.BeginPhase(PhaseName, Event.Context.EventId); \
		for (UCombatModifierRuntime* Runtime : MakeSortedSnapshot()) { Runtime->RuntimeCall(Event); } \
		DeferredOperations.EndPhase(); \
	}

COMBAT_EXECUTE_MUTABLE_HOOK(ExecutePreDealDamage, OnPreDealDamage, TEXT("PreDealDamage"), FCombatDamageEvent)
COMBAT_EXECUTE_MUTABLE_HOOK(ExecutePreTakeDamage, OnPreTakeDamage, TEXT("PreTakeDamage"), FCombatDamageEvent)
COMBAT_EXECUTE_MUTABLE_HOOK(ExecuteDamageBlock, OnDamageBlock, TEXT("DamageBlock"), FCombatDamageEvent)
COMBAT_EXECUTE_CONST_HOOK(ExecutePostDealDamage, OnPostDealDamage, TEXT("PostDealDamage"), FCombatDamageEvent)
COMBAT_EXECUTE_CONST_HOOK(ExecutePostTakeDamage, OnPostTakeDamage, TEXT("PostTakeDamage"), FCombatDamageEvent)
COMBAT_EXECUTE_MUTABLE_HOOK(ExecutePreDealHeal, OnPreDealHeal, TEXT("PreDealHeal"), FCombatHealEvent)
COMBAT_EXECUTE_MUTABLE_HOOK(ExecutePreTakeHeal, OnPreTakeHeal, TEXT("PreTakeHeal"), FCombatHealEvent)
COMBAT_EXECUTE_CONST_HOOK(ExecutePostDealHeal, OnPostDealHeal, TEXT("PostDealHeal"), FCombatHealEvent)
COMBAT_EXECUTE_CONST_HOOK(ExecutePostTakeHeal, OnPostTakeHeal, TEXT("PostTakeHeal"), FCombatHealEvent)

#undef COMBAT_EXECUTE_MUTABLE_HOOK
#undef COMBAT_EXECUTE_CONST_HOOK

void UCombatModifierComponent::HandleOwnerDeath()
{
	TArray<UCombatModifierRuntime*> Snapshot;
	for (UCombatModifierRuntime* Runtime : ActiveModifiers) { if (Runtime) { Snapshot.Add(Runtime); } }
	bHooksPaused = true;
	for (UCombatModifierRuntime* Runtime : Snapshot)
	{
		if (!Runtime) { continue; }
		CancelRuntimeSchedules(*Runtime);
		if (const UCombatModifierData* Data = Runtime->GetModifierData(); Data && Data->bRemoveOnDeath)
		{
			RemoveModifierImmediate(Runtime->GetHandle());
		}
	}
}

void UCombatModifierComponent::HandleOwnerRespawn()
{
	bHooksPaused = false;
	const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	TArray<UCombatModifierRuntime*> Snapshot;
	for (UCombatModifierRuntime* Runtime : ActiveModifiers) { if (Runtime) { Snapshot.Add(Runtime); } }
	for (UCombatModifierRuntime* Runtime : Snapshot)
	{
		if (!Runtime) { continue; }
		if (Runtime->GetExpireAt() > 0.0 && Runtime->GetExpireAt() <= Now)
		{
			RemoveModifierImmediate(Runtime->GetHandle());
		}
		else
		{
			ScheduleRuntime(*Runtime, true);
		}
	}
}

void UCombatModifierComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	DeferredOperations.Reset();
	for (int32 Index = ActiveModifiers.Num() - 1; Index >= 0; --Index)
	{
		if (UCombatModifierRuntime* Runtime = ActiveModifiers[Index])
		{
			RemoveModifierImmediate(Runtime->GetHandle(), false);
		}
	}
	Super::EndPlay(EndPlayReason);
}

ACombatUnitCharacter* UCombatModifierComponent::GetOwnerUnit() const
{
	return Cast<ACombatUnitCharacter>(GetOwner());
}

void UCombatModifierComponent::EmitModifierLog(
	const UCombatModifierRuntime& Runtime,
	const bool bRemoved,
	const bool bRefreshed) const
{
	UCombatEventSubsystem* Events = GetWorld() ? GetWorld()->GetSubsystem<UCombatEventSubsystem>() : nullptr;
	ACombatUnitCharacter* Target = GetOwnerUnit();
	const UCombatModifierData* Data = Runtime.GetModifierData();
	if (!Events || !Target || !Data)
	{
		return;
	}
	FCombatLogRecord Record;
	Record.Context = Events->CreateRootEvent();
	Record.EventType = bRemoved ? CombatTags::Event_Combat_ModifierRemoved : CombatTags::Event_Combat_ModifierApplied;
	Record.Source.DirectSourceType = ECombatDirectSourceType::Modifier;
	Record.Source.ModifierDefinitionId = Data->GetPrimaryAssetId();
	Record.SourceActorId = Runtime.GetSourceUnit() ? Runtime.GetSourceUnit()->GetUniqueID() : 0;
	Record.TargetActorId = Target->GetUniqueID();
	Record.UnitLifeGeneration = Target->GetLifeGeneration();
	Record.RequestedAmount = Data->Duration;
	Record.AppliedAmount = static_cast<float>(Runtime.GetStackCount());
	Record.Diagnostic = FString::Printf(TEXT("%s %s"),
		bRefreshed ? TEXT("Refreshed") : (bRemoved ? TEXT("Removed") : TEXT("Applied")),
		*Runtime.GetHandle().ToString());
	Events->Emit(Record);
}
