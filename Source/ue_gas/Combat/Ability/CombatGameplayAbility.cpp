#include "Combat/Ability/CombatGameplayAbility.h"

#include "Abilities/GameplayAbilityTypes.h"

#include "Combat/Ability/AbilityTask_WaitCombatInterval.h"
#include "Combat/Ability/CombatAbilitySystemComponent.h"
#include "Combat/Combat/CombatDamageSubsystem.h"
#include "Combat/Combat/CombatHealSubsystem.h"
#include "Combat/Core/CombatNumericPolicy.h"
#include "Combat/Core/CombatTags.h"
#include "Combat/Data/CombatDefinitionData.h"
#include "Combat/Log/CombatEventSubsystem.h"
#include "Combat/Modifiers/CombatModifierComponent.h"
#include "Combat/Projectile/CombatProjectileSubsystem.h"
#include "Combat/Scheduling/CombatSchedulerSubsystem.h"
#include "Combat/Targeting/CombatTargetingSubsystem.h"
#include "Combat/Thinker/CombatThinkerSubsystem.h"
#include "Combat/Unit/CombatUnitCharacter.h"

UCombatGameplayAbility::UCombatGameplayAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
}

float UCombatGameplayAbility::GetSpecialValue(const FName Key) const
{
	return AbilityData ? AbilityData->GetSpecialValue(Key, CombatContext.AbilityLevel) : 0.0f;
}

bool UCombatGameplayAbility::HasActiveCombatSchedule() const
{
	const UCombatSchedulerSubsystem* Scheduler = GetWorld() ? GetWorld()->GetSubsystem<UCombatSchedulerSubsystem>() : nullptr;
	return (Scheduler && Scheduler->IsHandleActive(CastPointSchedule))
		|| (ChannelTask && ChannelTask->HasActiveSchedule());
}

void UCombatGameplayAbility::OnGiveAbility(
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilitySpec& Spec)
{
	// InstancedPerActor 对象只消费 Class CDO 的单向配置，避免实例构造/热重载时留下空引用。
	if (const UCombatGameplayAbility* AbilityCdo = GetClass()->GetDefaultObject<UCombatGameplayAbility>();
		AbilityCdo && AbilityCdo != this)
	{
		AbilityData = AbilityCdo->AbilityData;
	}
	Super::OnGiveAbility(ActorInfo, Spec);
}

bool UCombatGameplayAbility::CanActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags,
	const FGameplayTagContainer* TargetTags,
	FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		if (OptionalRelevantTags) { OptionalRelevantTags->AddTag(CombatTags::Failure_ActionUnsupported); }
		return false;
	}
	const ACombatUnitCharacter* Unit = ActorInfo ? Cast<ACombatUnitCharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
	const UCombatAbilitySystemComponent* Asc = ActorInfo
		? Cast<UCombatAbilitySystemComponent>(ActorInfo->AbilitySystemComponent.Get()) : nullptr;
	FString Diagnostic;
	if (!Unit || !Asc || Unit->GetLifeState() != ECombatLifeState::Alive)
	{
		if (OptionalRelevantTags) { OptionalRelevantTags->AddTag(CombatTags::Failure_Life_NotAlive); }
		return false;
	}
	if (!AbilityData || !AbilityData->ValidateRuntime(Diagnostic))
	{
		if (OptionalRelevantTags) { OptionalRelevantTags->AddTag(CombatTags::Failure_ActionUnsupported); }
		return false;
	}
	const bool bHardStateBlocked = Asc->HasMatchingGameplayTag(CombatTags::State_Stunned)
		|| Asc->HasMatchingGameplayTag(CombatTags::State_Hexed)
		|| Asc->HasMatchingGameplayTag(CombatTags::State_Frozen);
	const bool bSilenceBlocked = Asc->HasMatchingGameplayTag(CombatTags::State_Silenced)
		&& !AbilityData->BehaviorTags.HasTagExact(CombatTags::Ability_Behavior_IgnoreSilence);
	if (bHardStateBlocked || bSilenceBlocked)
	{
		if (OptionalRelevantTags) { OptionalRelevantTags->AddTag(CombatTags::Failure_Ability_UnitStateBlocked); }
		return false;
	}
	FCombatAbilityTargetData TargetData;
	if (!Asc->PeekPendingTargetData(Handle, TargetData))
	{
		if (OptionalRelevantTags) { OptionalRelevantTags->AddTag(CombatTags::Failure_Ability_InvalidTargetData); }
		return false;
	}
	UCombatTargetingSubsystem* Targeting = Unit->GetWorld()->GetSubsystem<UCombatTargetingSubsystem>();
	const FCombatTargetValidationResult TargetResult = Targeting
		? Targeting->ValidateAbilityTarget(const_cast<ACombatUnitCharacter*>(Unit),
			AbilityData->BehaviorTags, AbilityData->TargetingRules, TargetData)
		: FCombatTargetValidationResult();
	if (!TargetResult.bValid)
	{
		if (OptionalRelevantTags && TargetResult.FailureTag.IsValid()) { OptionalRelevantTags->AddTag(TargetResult.FailureTag); }
		return false;
	}
	const FGameplayAbilitySpec* Spec = Asc->FindAbilitySpecFromHandle(Handle);
	FGameplayTag FailureTag;
	if (!Spec || !Asc->PreflightCombatAbility(Handle, *AbilityData, Spec->Level, FailureTag))
	{
		if (OptionalRelevantTags && FailureTag.IsValid()) { OptionalRelevantTags->AddTag(FailureTag); }
		return false;
	}
	return true;
}

void UCombatGameplayAbility::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	UCombatAbilitySystemComponent* Asc = GetCombatAsc();
	ACombatUnitCharacter* Unit = ActorInfo ? Cast<ACombatUnitCharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
	UCombatEventSubsystem* Events = GetWorld() ? GetWorld()->GetSubsystem<UCombatEventSubsystem>() : nullptr;
	FGameplayAbilitySpec* Spec = Asc ? Asc->FindAbilitySpecFromHandle(Handle) : nullptr;
	if (!Asc || !Unit || !Events || !Spec || !AbilityData
		|| !Asc->ConsumePendingTargetData(Handle, ActivationTargetData))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	EmittedLifecycleEvents.Reset();
	bCostCommitted = false;
	bCooldownCommitted = false;
	bSpellStarted = false;
	bChannelStarted = false;
	bChannelEnded = false;
	bOrderReleased = false;
	bEnding = false;
	LastInterruptFailureTag = FGameplayTag();
	LastActionResult = FCombatAbilityActionResult();
	CombatContext = FCombatAbilityActivationContext();
	CombatContext.EventContext = Events->CreateRootEvent();
	CombatContext.Caster = Unit;
	CombatContext.CasterLifeGeneration = Unit->GetLifeGeneration();
	CombatContext.AbilityLevel = Spec->Level;

	UCombatTargetingSubsystem* Targeting = GetWorld()->GetSubsystem<UCombatTargetingSubsystem>();
	const FCombatTargetValidationResult TargetResult = Targeting->ValidateAbilityTarget(
		Unit, AbilityData->BehaviorTags, AbilityData->TargetingRules, ActivationTargetData);
	if (!TargetResult.bValid)
	{
		InterruptAbility(TargetResult.FailureTag, TargetResult.Diagnostic);
		return;
	}
	CombatContext.TargetActor = ActivationTargetData.TargetActor;
	CombatContext.TargetLocation = TargetResult.AuthoritativeLocation;
	CombatContext.TargetLifeGeneration = CombatContext.TargetActor
		? CombatContext.TargetActor->GetLifeGeneration() : 0;

	FGameplayTag FailureTag;
	if (!CommitStage(ECombatAbilityCommitStage::CastStarted, FailureTag))
	{
		InterruptAbility(FailureTag, TEXT("CastStarted commit failed"));
		return;
	}
	EmitLifecycleEvent(CombatTags::Event_Combat_AbilityCastStarted, FGameplayTag(), TEXT("CastStarted"));
	if (AbilityData->CastPoint <= 0.0f)
	{
		HandleCastPoint(FCombatScheduledTickContext());
		return;
	}
	UCombatSchedulerSubsystem* Scheduler = GetWorld()->GetSubsystem<UCombatSchedulerSubsystem>();
	CastPointSchedule = Scheduler ? Scheduler->ScheduleOnce(
		this, AbilityData->CastPoint, 0,
		FCombatScheduledDelegate::CreateWeakLambda(this,
			[this](const FCombatScheduledTickContext& Context) { HandleCastPoint(Context); })) : FCombatScheduleHandle();
	if (!CastPointSchedule.IsValid())
	{
		InterruptAbility(CombatTags::Failure_ActionUnsupported, TEXT("Could not schedule cast point"));
	}
}

void UCombatGameplayAbility::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const bool bReplicateEndAbility,
	const bool bWasCancelled)
{
	if (bEnding)
	{
		return;
	}
	bEnding = true;
	if (UCombatSchedulerSubsystem* Scheduler = GetWorld() ? GetWorld()->GetSubsystem<UCombatSchedulerSubsystem>() : nullptr)
	{
		Scheduler->Cancel(CastPointSchedule);
	}
	CastPointSchedule = FCombatScheduleHandle();

	if (bWasCancelled)
	{
		EmitLifecycleEvent(CombatTags::Event_Combat_AbilityInterrupted, FGameplayTag(), TEXT("Interrupted"));
		if (bChannelStarted && !bChannelEnded)
		{
			ReceiveChannelFinish(CombatContext, true);
			bChannelEnded = true;
			EmitLifecycleEvent(CombatTags::Event_Combat_AbilityChannelEnded, FGameplayTag(), TEXT("ChannelEnded Interrupted=1"));
		}
		ReleaseCombatOrder(false, LastInterruptFailureTag.IsValid()
			? LastInterruptFailureTag : CombatTags::Order_Failure_Cancelled.GetTag());
	}
	if (ChannelTask)
	{
		ChannelTask->OnTick.RemoveAll(this);
		ChannelTask->OnFinished.RemoveAll(this);
		ChannelTask->EndTask();
		ChannelTask = nullptr;
	}
	if (AbilityData && AbilityData->bCancelProjectilesWithAbility
		&& CombatContext.Caster && CombatContext.EventContext.RootEventId.IsValid())
	{
		// 默认弹体与技能实例解耦；只有资产显式选择绑定时才按本 Activation 批量取消。
		if (UCombatProjectileSubsystem* Projectiles = GetWorld()->GetSubsystem<UCombatProjectileSubsystem>())
		{
			Projectiles->CancelProjectilesForAbility(
				CombatContext.Caster, CombatContext.EventContext.RootEventId);
		}
	}
	EmitLifecycleEvent(CombatTags::Event_Combat_AbilityEnded, FGameplayTag(),
		bWasCancelled ? TEXT("Ended Cancelled=1") : TEXT("Ended Cancelled=0"));
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
	bEnding = false;
}

void UCombatGameplayAbility::ReceiveSpellStart_Implementation(const FCombatAbilityActivationContext& Context) { (void)Context; }
void UCombatGameplayAbility::ReceiveChannelTick_Implementation(
	const FCombatAbilityActivationContext& Context,
	const FCombatScheduledTickContext& TickContext)
{
	(void)Context;
	(void)TickContext;
}
void UCombatGameplayAbility::ReceiveChannelFinish_Implementation(
	const FCombatAbilityActivationContext& Context,
	const bool bInterrupted)
{
	(void)Context;
	(void)bInterrupted;
}

FCombatAbilityActionResult UCombatGameplayAbility::ExecuteDataDrivenActions()
{
	FCombatAbilityActionResult Result;
	ACombatUnitCharacter* Caster = CombatContext.Caster;
	UCombatTargetingSubsystem* Targeting = GetWorld() ? GetWorld()->GetSubsystem<UCombatTargetingSubsystem>() : nullptr;
	if (!Caster || !AbilityData || !Targeting)
	{
		Result.FailureTag = CombatTags::Failure_ActionUnsupported;
		return Result;
	}
	FCombatSourceContext SourceContext;
	SourceContext.DirectSourceType = ECombatDirectSourceType::Ability;
	SourceContext.AbilityDefinitionId = AbilityData->GetPrimaryAssetId();

	for (const FCombatAbilityAction& Action : AbilityData->Actions)
	{
		const bool bProjectileAction = Action.Type == ECombatAbilityActionType::SpawnLinearProjectile
			|| Action.Type == ECombatAbilityActionType::SpawnTrackingProjectile;
		if (bProjectileAction)
		{
			const float Magnitude = GetSpecialValue(Action.MagnitudeKey);
			if (!Action.ProjectileData || !FCombatNumericPolicyV1::IsValidNonNegativeRequest(Magnitude))
			{
				Result.FailureTag = CombatTags::Failure_InvalidNumber;
				return Result;
			}
			FCombatProjectileSpec Spec;
			Spec.ProjectileData = Action.ProjectileData;
			Spec.Source = Caster;
			Spec.SpawnLocation = Caster->GetActorLocation();
			Spec.Direction = CombatContext.TargetLocation - Spec.SpawnLocation;
			if (Spec.Direction.IsNearlyZero())
			{
				Spec.Direction = Caster->GetActorForwardVector();
			}
			Spec.MovementType = Action.Type == ECombatAbilityActionType::SpawnTrackingProjectile
				? ECombatProjectileMovementType::Tracking : ECombatProjectileMovementType::Linear;
			Spec.Target = Spec.MovementType == ECombatProjectileMovementType::Tracking
				? CombatContext.TargetActor.Get() : nullptr;
			Spec.TargetLostPolicy = Action.ProjectileData->TargetLostPolicy;
			Spec.HitPolicy = Action.ProjectileData->HitPolicy;
			Spec.SpeedOverride = Action.ProjectileSpeedKey.IsNone()
				? -1.0f : GetSpecialValue(Action.ProjectileSpeedKey);
			Spec.RadiusOverride = Action.RadiusKey.IsNone()
				? -1.0f : GetSpecialValue(Action.RadiusKey);
			Spec.MaxDistanceOverride = Action.ProjectileRangeKey.IsNone()
				? -1.0f : GetSpecialValue(Action.ProjectileRangeKey);
			FCombatProjectileImpactAction Impact;
			Impact.Type = ECombatProjectileImpactActionType::Damage;
			Impact.Magnitude = Magnitude;
			Impact.DamageType = Action.DamageType;
			Spec.ImpactActions.Add(Impact);
			if (Action.ModifierData)
			{
				const float MotionSpeed = Action.MotionSpeedKey.IsNone()
					? 0.0f : GetSpecialValue(Action.MotionSpeedKey);
				if (Action.bMotionToSource
					&& (!FMath::IsFinite(MotionSpeed) || MotionSpeed <= 0.0f))
				{
					Result.FailureTag = CombatTags::Failure_InvalidNumber;
					return Result;
				}
				FCombatProjectileImpactAction ModifierImpact;
				ModifierImpact.Type = ECombatProjectileImpactActionType::ApplyModifier;
				ModifierImpact.ModifierData = Action.ModifierData;
				ModifierImpact.bMotionToSource = Action.bMotionToSource;
				ModifierImpact.MotionSpeed = MotionSpeed;
				ModifierImpact.MotionPriority = Action.MotionPriority;
				Spec.ImpactActions.Add(ModifierImpact);
			}
			Spec.ParentEvent = CombatContext.EventContext;
			Spec.SourceContext = SourceContext;
			Spec.AbilityActivationId = CombatContext.EventContext.RootEventId;
			Spec.bCancelWithSourceAbility = AbilityData->bCancelProjectilesWithAbility;
			UCombatProjectileSubsystem* Projectiles = GetWorld()->GetSubsystem<UCombatProjectileSubsystem>();
			const FCombatProjectileResult SpawnResult = Projectiles
				? Projectiles->SpawnProjectile(Spec) : FCombatProjectileResult();
			if (!SpawnResult.bSuccess)
			{
				Result.FailureTag = SpawnResult.FailureTag.IsValid()
					? SpawnResult.FailureTag : CombatTags::Failure_ActionUnsupported.GetTag();
				return Result;
			}
			++Result.AffectedTargetCount;
			continue;
		}

		if (Action.Type == ECombatAbilityActionType::CreateThinker)
		{
			const float Radius = GetSpecialValue(Action.RadiusKey);
			const float DamagePerPulse = GetSpecialValue(Action.MagnitudeKey);
			const float Duration = GetSpecialValue(Action.DurationKey);
			const float Interval = Action.IntervalKey.IsNone() ? 0.0f : GetSpecialValue(Action.IntervalKey);
			if (!FCombatNumericPolicyV1::IsValidNonNegativeRequest(Radius)
				|| !FCombatNumericPolicyV1::IsValidNonNegativeRequest(DamagePerPulse)
				|| !FCombatNumericPolicyV1::IsValidNonNegativeRequest(Duration)
				|| !FCombatNumericPolicyV1::IsValidNonNegativeRequest(Interval))
			{
				Result.FailureTag = CombatTags::Failure_InvalidNumber;
				return Result;
			}
			FCombatThinkerSpec Spec;
			Spec.Source = Caster;
			Spec.Location = Action.Target == ECombatAbilityActionTarget::Caster
				? Caster->GetActorLocation() : CombatContext.TargetLocation;
			Spec.Radius = Radius;
			Spec.PulseInterval = Interval;
			Spec.Duration = Duration;
			Spec.DamagePerPulse = DamagePerPulse;
			Spec.DamageType = Action.DamageType;
			Spec.ModifierPerPulse = Action.ModifierData;
			Spec.TargetingRules = AbilityData->TargetingRules;
			Spec.ParentEvent = CombatContext.EventContext;
			Spec.SourceContext = SourceContext;
			Spec.AbilityActivationId = CombatContext.EventContext.RootEventId;
			UCombatThinkerSubsystem* Thinkers = GetWorld()->GetSubsystem<UCombatThinkerSubsystem>();
			const FCombatThinkerResult CreateResult = Thinkers
				? Thinkers->CreateThinker(Spec) : FCombatThinkerResult();
			if (!CreateResult.bSuccess)
			{
				Result.FailureTag = CreateResult.FailureTag.IsValid()
					? CreateResult.FailureTag : CombatTags::Failure_ActionUnsupported.GetTag();
				return Result;
			}
			++Result.AffectedTargetCount;
			continue;
		}

		TArray<ACombatUnitCharacter*> Targets;
		switch (Action.Target)
		{
		case ECombatAbilityActionTarget::Caster:
			Targets.Add(Caster);
			break;
		case ECombatAbilityActionTarget::UnitTarget:
			if (CombatContext.TargetActor) { Targets.Add(CombatContext.TargetActor); }
			else
			{
				Result.FailureTag = CombatTags::Failure_Target_Invalid;
				return Result;
			}
			break;
		case ECombatAbilityActionTarget::UnitsInRadius:
		{
			const float Radius = GetSpecialValue(Action.RadiusKey);
			if (!FCombatNumericPolicyV1::IsValidNonNegativeRequest(Radius))
			{
				Result.FailureTag = CombatTags::Failure_InvalidNumber;
				return Result;
			}
			Targets = Targeting->QueryUnitsInRadius(
				Caster, CombatContext.TargetLocation, Radius, AbilityData->TargetingRules);
			break;
		}
		default:
			Result.FailureTag = CombatTags::Failure_ActionUnsupported;
			return Result;
		}

		const float Magnitude = GetSpecialValue(Action.MagnitudeKey);
		if ((Action.Type == ECombatAbilityActionType::Damage || Action.Type == ECombatAbilityActionType::Heal)
			&& !FCombatNumericPolicyV1::IsValidNonNegativeRequest(Magnitude))
		{
			Result.FailureTag = CombatTags::Failure_InvalidNumber;
			return Result;
		}
		for (ACombatUnitCharacter* Target : Targets)
		{
			bool bActionSucceeded = false;
			switch (Action.Type)
			{
			case ECombatAbilityActionType::Damage:
			{
				FCombatDamageRequest Request;
				Request.Source = Caster;
				Request.Target = Target;
				Request.Amount = Magnitude;
				Request.DamageType = Action.DamageType;
				Request.SourceContext = SourceContext;
				Request.ParentEvent = CombatContext.EventContext;
				bActionSucceeded = GetWorld()->GetSubsystem<UCombatDamageSubsystem>()->DealDamage(Request).bSuccess;
				break;
			}
			case ECombatAbilityActionType::Heal:
			{
				FCombatHealRequest Request;
				Request.Source = Caster;
				Request.Target = Target;
				Request.Amount = Magnitude;
				Request.SourceContext = SourceContext;
				Request.ParentEvent = CombatContext.EventContext;
				bActionSucceeded = GetWorld()->GetSubsystem<UCombatHealSubsystem>()->Heal(Request).bSuccess;
				break;
			}
			case ECombatAbilityActionType::ApplyModifier:
			{
				FCombatModifierApplyRequest Request;
				Request.Source = Caster;
				Request.ModifierData = Action.ModifierData;
				bActionSucceeded = Target->GetCombatModifierComponent()->ApplyModifier(Request).bSuccess;
				break;
			}
			case ECombatAbilityActionType::SendGameplayEvent:
			{
				FGameplayEventData Payload;
				Payload.EventTag = Action.EventTag;
				Payload.Instigator = Caster;
				Payload.Target = Target;
				bActionSucceeded = Target->GetCombatAbilitySystemComponent()->HandleGameplayEvent(Action.EventTag, &Payload) >= 0;
				break;
			}
			default:
				Result.FailureTag = CombatTags::Failure_ActionUnsupported;
				return Result;
			}
			if (!bActionSucceeded)
			{
				Result.FailureTag = CombatTags::Failure_Ability_CommitFailed;
				return Result;
			}
			++Result.AffectedTargetCount;
		}
	}
	Result.bSuccess = true;
	return Result;
}

void UCombatGameplayAbility::HandleCastPoint(const FCombatScheduledTickContext& TickContext)
{
	(void)TickContext;
	CastPointSchedule = FCombatScheduleHandle();
	FGameplayTag FailureTag;
	if (!RevalidateAtExecutionPoint(FailureTag))
	{
		InterruptAbility(FailureTag, TEXT("Target failed cast-point revalidation"));
		return;
	}
	if (!CommitStage(ECombatAbilityCommitStage::SpellStarted, FailureTag))
	{
		InterruptAbility(FailureTag, TEXT("SpellStarted commit failed"));
		return;
	}
	bSpellStarted = true;
	EmitLifecycleEvent(CombatTags::Event_Combat_AbilitySpellStarted, FGameplayTag(), TEXT("SpellStarted"));
	ReceiveSpellStart(CombatContext);
	LastActionResult = ExecuteDataDrivenActions();
	if (!LastActionResult.bSuccess)
	{
		EmitLifecycleEvent(CombatTags::Event_Combat_AbilityActionFailed,
			LastActionResult.FailureTag, TEXT("DataDriven Action failed"));
		InterruptAbility(LastActionResult.FailureTag, TEXT("DataDriven Action failed"));
		return;
	}
	if (ACombatUnitCharacter* Caster = CombatContext.Caster)
	{
		Caster->GetCombatModifierComponent()->ExecuteAbilityExecuted(
			AbilityData->GetPrimaryAssetId(), CombatContext.EventContext);
	}
	if (AbilityData->BehaviorTags.HasTagExact(CombatTags::Ability_Behavior_Channelled))
	{
		bChannelStarted = true;
		ChannelTask = UAbilityTask_WaitCombatInterval::WaitCombatInterval(
			this, AbilityData->ChannelInterval, AbilityData->ChannelDuration);
		ChannelTask->OnTick.AddDynamic(this, &UCombatGameplayAbility::HandleChannelTick);
		ChannelTask->OnFinished.AddDynamic(this, &UCombatGameplayAbility::HandleChannelFinished);
		ChannelTask->ReadyForActivation();
		if (!ChannelTask->HasActiveSchedule())
		{
			InterruptAbility(CombatTags::Failure_ActionUnsupported, TEXT("Could not start channel task"));
		}
		return;
	}
	ReleaseCombatOrder(true, FGameplayTag());
	FinishSuccessfully();
}

void UCombatGameplayAbility::HandleChannelTick(const FCombatScheduledTickContext TickContext)
{
	FGameplayTag FailureTag;
	if (!RevalidateAtExecutionPoint(FailureTag))
	{
		InterruptAbility(FailureTag, TEXT("Channel target became invalid"));
		return;
	}
	ReceiveChannelTick(CombatContext, TickContext);
}

void UCombatGameplayAbility::HandleChannelFinished()
{
	ChannelTask = nullptr;
	if (!bChannelEnded)
	{
		ReceiveChannelFinish(CombatContext, false);
		bChannelEnded = true;
		EmitLifecycleEvent(CombatTags::Event_Combat_AbilityChannelEnded, FGameplayTag(), TEXT("ChannelEnded Interrupted=0"));
	}
	if (!bOrderReleased)
	{
		ReleaseCombatOrder(true, FGameplayTag());
	}
	FinishSuccessfully();
}

bool UCombatGameplayAbility::RevalidateAtExecutionPoint(FGameplayTag& OutFailureTag)
{
	OutFailureTag = FGameplayTag();
	ACombatUnitCharacter* Caster = CombatContext.Caster;
	if (!Caster || Caster->GetLifeState() != ECombatLifeState::Alive
		|| Caster->GetLifeGeneration() != CombatContext.CasterLifeGeneration)
	{
		OutFailureTag = CombatTags::Failure_Life_NotAlive;
		return false;
	}
	if (!CombatContext.TargetActor && AbilityData->TargetLostPolicy == ECombatTargetLostPolicy::UseLastKnownPoint)
	{
		return true;
	}
	UCombatTargetingSubsystem* Targeting = GetWorld()->GetSubsystem<UCombatTargetingSubsystem>();
	const FCombatTargetValidationResult Result = Targeting->ValidateAbilityTarget(
		Caster, AbilityData->BehaviorTags, AbilityData->TargetingRules, ActivationTargetData);
	if (!Result.bValid || (CombatContext.TargetActor
		&& CombatContext.TargetActor->GetLifeGeneration() != CombatContext.TargetLifeGeneration))
	{
		if (AbilityData->TargetLostPolicy == ECombatTargetLostPolicy::UseLastKnownPoint
			&& AbilityData->BehaviorTags.HasTagExact(CombatTags::Ability_Behavior_UnitTarget))
		{
			CombatContext.TargetActor = nullptr;
			return true;
		}
		OutFailureTag = Result.FailureTag.IsValid() ? Result.FailureTag : CombatTags::Failure_Target_Invalid.GetTag();
		return false;
	}
	CombatContext.TargetLocation = Result.AuthoritativeLocation;
	return true;
}

bool UCombatGameplayAbility::CommitStage(
	const ECombatAbilityCommitStage Stage,
	FGameplayTag& OutFailureTag)
{
	UCombatAbilitySystemComponent* Asc = GetCombatAsc();
	return Asc && AbilityData && Asc->CommitCombatAbilityStage(
		GetCurrentAbilitySpecHandle(), *AbilityData, CombatContext.AbilityLevel, Stage,
		bCostCommitted, bCooldownCommitted, OutFailureTag);
}

void UCombatGameplayAbility::FinishSuccessfully()
{
	FGameplayTag FailureTag;
	if (!CommitStage(ECombatAbilityCommitStage::AbilityEnded, FailureTag))
	{
		InterruptAbility(FailureTag, TEXT("AbilityEnded commit failed"));
		return;
	}
	EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, false);
}

void UCombatGameplayAbility::InterruptAbility(const FGameplayTag& FailureTag, const FString& Diagnostic)
{
	LastInterruptFailureTag = FailureTag;
	EmitLifecycleEvent(CombatTags::Event_Combat_AbilityInterrupted, FailureTag, Diagnostic);
	EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, true);
}

void UCombatGameplayAbility::ReleaseCombatOrder(const bool bSuccess, const FGameplayTag& FailureTag)
{
	if (bOrderReleased)
	{
		return;
	}
	bOrderReleased = true;
	EmitLifecycleEvent(CombatTags::Event_Combat_AbilityOrderReleased, FailureTag,
		bSuccess ? TEXT("OrderReleased Success=1") : TEXT("OrderReleased Success=0"));
	if (UCombatAbilitySystemComponent* Asc = GetCombatAsc())
	{
		Asc->NotifyCombatAbilityOrderReleased(
			GetCurrentAbilitySpecHandle(),
			bSuccess,
			FailureTag,
			AbilityData ? AbilityData->ChannelInterruptOrderPolicy : ECombatChannelInterruptOrderPolicy::Continue);
	}
}

void UCombatGameplayAbility::EmitLifecycleEvent(
	const FGameplayTag& EventType,
	const FGameplayTag& FailureTag,
	const FString& Diagnostic)
{
	if (!EventType.IsValid() || EmittedLifecycleEvents.Contains(EventType))
	{
		return;
	}
	EmittedLifecycleEvents.Add(EventType);
	UCombatEventSubsystem* Events = GetWorld() ? GetWorld()->GetSubsystem<UCombatEventSubsystem>() : nullptr;
	if (!Events || !AbilityData || !CombatContext.EventContext.IsValid())
	{
		return;
	}
	FCombatLogRecord Record;
	Record.Context = CombatContext.EventContext;
	Record.EventType = EventType;
	Record.FailureTag = FailureTag;
	Record.Source.DirectSourceType = ECombatDirectSourceType::Ability;
	Record.Source.AbilityDefinitionId = AbilityData->GetPrimaryAssetId();
	Record.SourceActorId = CombatContext.Caster ? CombatContext.Caster->GetUniqueID() : 0;
	Record.TargetActorId = CombatContext.TargetActor ? CombatContext.TargetActor->GetUniqueID() : 0;
	Record.UnitLifeGeneration = CombatContext.CasterLifeGeneration;
	Record.AppliedAmount = LastActionResult.AffectedTargetCount;
	Record.Diagnostic = FString::Printf(TEXT("Activation=%s Ability=%s %s"),
		*CombatContext.EventContext.RootEventId.ToString(), *AbilityData->GetPrimaryAssetId().ToString(), *Diagnostic);
	Events->Emit(Record);
}

UCombatAbilitySystemComponent* UCombatGameplayAbility::GetCombatAsc() const
{
	return Cast<UCombatAbilitySystemComponent>(GetAbilitySystemComponentFromActorInfo());
}
