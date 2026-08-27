#include "Combat/Attack/CombatAttackComponent.h"

#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

#include "Combat/Ability/CombatAbilitySystemComponent.h"
#include "Combat/Attack/CombatAttackTimingPolicy.h"
#include "Combat/Attributes/CombatAttributeSet.h"
#include "Combat/Combat/CombatDamageSubsystem.h"
#include "Combat/Core/CombatRngSubsystem.h"
#include "Combat/Core/CombatTags.h"
#include "Combat/Data/CombatDefinitionData.h"
#include "Combat/Log/CombatEventSubsystem.h"
#include "Combat/Modifiers/CombatModifierComponent.h"
#include "Combat/Projectile/CombatProjectileSubsystem.h"
#include "Combat/Scheduling/CombatSchedulerSubsystem.h"
#include "Combat/Targeting/CombatTargetingSubsystem.h"
#include "Combat/Unit/CombatUnitCharacter.h"

UCombatAttackComponent::UCombatAttackComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

FCombatOperationResult UCombatAttackComponent::CanStartMeleeAttack(ACombatUnitCharacter* Target) const
{
	ACombatUnitCharacter* OwnerUnit = GetOwnerUnit();
	if (!OwnerUnit || !OwnerUnit->HasAuthority() || !Target)
	{
		return FCombatOperationResult::Failure(CombatTags::Failure_Authority, TEXT("Attack requires authority and a target"));
	}
	if (OwnerUnit->IsAttackBlocked())
	{
		return FCombatOperationResult::Failure(CombatTags::Failure_Attack_Blocked, TEXT("Unit state blocks attack"));
	}
	if (!bAttackReady || CurrentWindupHandle.IsValid())
	{
		return FCombatOperationResult::Failure(CombatTags::Failure_Attack_NotReady, TEXT("Attack interval is not ready"));
	}
	const UCombatUnitData* Data = OwnerUnit->GetUnitData();
	const UCharacterMovementComponent* Movement = OwnerUnit->GetCharacterMovement();
	if ((!Data || !Data->bAllowAttackWhileMoving) && Movement && Movement->Velocity.SizeSquared2D() > 1.0f)
	{
		return FCombatOperationResult::Failure(CombatTags::Failure_Attack_Blocked, TEXT("Moving attack start is disabled"));
	}
	const UCombatTargetingSubsystem* Targeting = GetWorld() ? GetWorld()->GetSubsystem<UCombatTargetingSubsystem>() : nullptr;
	const FCombatTargetValidationResult TargetResult = Targeting
		? Targeting->ValidateUnitTarget(OwnerUnit, Target, MakeAttackTargetingRules()) : FCombatTargetValidationResult();
	if (!TargetResult.bValid)
	{
		return FCombatOperationResult::Failure(
			TargetResult.FailureTag.IsValid() ? TargetResult.FailureTag : CombatTags::Failure_Target_Invalid.GetTag(),
			TargetResult.Diagnostic);
	}
	if (!IsFacingTarget(*Target))
	{
		return FCombatOperationResult::Failure(CombatTags::Failure_Attack_Blocked, TEXT("Target is outside attack facing tolerance"));
	}

	UCombatAbilitySystemComponent* Asc = OwnerUnit->GetCombatAbilitySystemComponent();
	FCombatAttackTiming Timing;
	const float BaseAttackPoint = Data ? Data->BaseAttackPoint : 0.3f;
	if (!Asc || !FCombatAttackTimingPolicyV1::Calculate(
		Asc->GetNumericAttribute(UCombatAttributeSet::GetBaseAttackTimeAttribute()),
		Asc->GetNumericAttribute(UCombatAttributeSet::GetAttackSpeedAttribute()),
		BaseAttackPoint,
		Timing))
	{
		return FCombatOperationResult::Failure(CombatTags::Failure_InvalidNumber, TEXT("Attack timing input is invalid"));
	}
	return FCombatOperationResult::Success();
}

FCombatAttackResult UCombatAttackComponent::StartMeleeAttack(
	ACombatUnitCharacter* Target,
	const FCombatOrderHandle OrderHandle)
{
	FCombatAttackResult Result;
	const FCombatOperationResult Preflight = CanStartMeleeAttack(Target);
	if (!Preflight.bSuccess)
	{
		Result.FailureTag = Preflight.FailureTag;
		return Result;
	}
	ACombatUnitCharacter* OwnerUnit = GetOwnerUnit();
	UCombatAbilitySystemComponent* Asc = OwnerUnit->GetCombatAbilitySystemComponent();
	UCombatEventSubsystem* Events = GetWorld()->GetSubsystem<UCombatEventSubsystem>();
	UCombatSchedulerSubsystem* Scheduler = GetWorld()->GetSubsystem<UCombatSchedulerSubsystem>();
	if (!Asc || !Events || !Scheduler || NextRecordId == 0 || RegistryGeneration == 0)
	{
		Result.FailureTag = CombatTags::Failure_ActionUnsupported;
		return Result;
	}

	FCombatAttackRecord Record;
	Record.Handle.Key.Id = NextRecordId++;
	Record.Handle.Key.Generation = RegistryGeneration;
	Record.Handle.Key.LifeGeneration = OwnerUnit->GetLifeGeneration();
	Record.Attacker = OwnerUnit;
	Record.Target = Target;
	Record.TargetLifeGeneration = Target->GetLifeGeneration();
	Record.OrderHandle = OrderHandle;
	Record.EventContext = Events->CreateRootEvent();
	Record.BaseDamage = Asc->GetNumericAttribute(UCombatAttributeSet::GetAttackDamageAttribute());
	Record.DamageType = ECombatDamageType::Physical;
	Record.StartedAt = GetWorld()->GetTimeSeconds();
	const UCombatUnitData* Data = OwnerUnit->GetUnitData();
	Record.CriticalChance = Data ? Data->CriticalStrikeChance : 0.0f;
	Record.CriticalMultiplier = Data ? Data->CriticalStrikeMultiplier : 2.0f;
	if (!FMath::IsFinite(Record.BaseDamage) || Record.BaseDamage < 0.0f
		|| !Record.EventContext.IsValid()
		|| !FCombatAttackTimingPolicyV1::Calculate(
			Asc->GetNumericAttribute(UCombatAttributeSet::GetBaseAttackTimeAttribute()),
			Asc->GetNumericAttribute(UCombatAttributeSet::GetAttackSpeedAttribute()),
			Data ? Data->BaseAttackPoint : 0.3f,
			Record.Timing))
	{
		Result.FailureTag = CombatTags::Failure_InvalidNumber;
		return Result;
	}

	FCombatAttackCandidateContext Candidate;
	Candidate.Handle = Record.Handle;
	Candidate.Attacker = OwnerUnit;
	Candidate.Target = Target;
	Candidate.BaseDamage = Record.BaseDamage;
	OwnerUnit->GetCombatModifierComponent()->ClaimAttackOrbs(Candidate, Record.ClaimedOrbs);
	for (const FCombatOrbSnapshot& Orb : Record.ClaimedOrbs)
	{
		Record.BonusDamage += Orb.BonusDamage;
		if (Orb.bOverrideDamageType)
		{
			Record.DamageType = Orb.DamageType;
		}
		Record.OnHitActions.Append(Orb.OnHitActions);
		if (!Record.ProjectileDataOverride && Orb.ProjectileDataOverride)
		{
			Record.ProjectileDataOverride = Orb.ProjectileDataOverride;
		}
	}
	Record.State = ECombatAttackState::Windup;
	const FCombatAttackHandle Handle = Record.Handle;
	ActiveRecords.Add(Handle.Key.Id, MoveTemp(Record));
	CurrentWindupHandle = Handle;
	bAttackReady = false;
	WindupSchedule = Scheduler->ScheduleOnce(
		this,
		ActiveRecords[Handle.Key.Id].Timing.AttackPoint,
		0,
		FCombatScheduledDelegate::CreateWeakLambda(this,
			[this, Handle](const FCombatScheduledTickContext& Context) { HandleAttackPoint(Handle, Context); }));
	if (!WindupSchedule.IsValid())
	{
		return AbortRecord(Handle, ECombatAttackOutcome::Cancelled, CombatTags::Failure_ActionUnsupported);
	}
	Result.bSuccess = true;
	Result.Handle = Handle;
	return Result;
}

FCombatAttackResult UCombatAttackComponent::FinalizeAttack(const FCombatAttackHandle Handle)
{
	return FinalizeAttackInternal(Handle);
}

FCombatAttackResult UCombatAttackComponent::FinalizeAttackFromProjectile(
	const FCombatAttackHandle Handle,
	ACombatUnitCharacter* ImpactTarget)
{
	return FinalizeAttackInternal(Handle, true, ImpactTarget);
}

bool UCombatAttackComponent::FailLaunchedAttackFromProjectile(
	const FCombatAttackHandle Handle,
	const FGameplayTag FailureTag)
{
	const FCombatAttackRecord* Record = FindRecord(Handle);
	if (!Record || Record->State != ECombatAttackState::Launched)
	{
		return false;
	}
	AbortRecord(Handle, ECombatAttackOutcome::TargetInvalid,
		FailureTag.IsValid() ? FailureTag : CombatTags::Failure_Projectile_TargetLost.GetTag());
	return true;
}

bool UCombatAttackComponent::CancelWindupForOrder(
	const FCombatOrderHandle OrderHandle,
	const FGameplayTag FailureTag)
{
	FCombatAttackRecord* Record = FindRecord(CurrentWindupHandle);
	if (!Record || Record->State != ECombatAttackState::Windup || Record->OrderHandle != OrderHandle)
	{
		return false;
	}
	if (UCombatSchedulerSubsystem* Scheduler = GetWorld() ? GetWorld()->GetSubsystem<UCombatSchedulerSubsystem>() : nullptr)
	{
		Scheduler->Cancel(WindupSchedule);
	}
	WindupSchedule = FCombatScheduleHandle();
	AbortRecord(Record->Handle, ECombatAttackOutcome::Cancelled,
		FailureTag.IsValid() ? FailureTag : CombatTags::Order_Failure_Cancelled.GetTag());
	bAttackReady = true;
	return true;
}

bool UCombatAttackComponent::IsAttackActive(const FCombatAttackHandle Handle) const
{
	const ACombatUnitCharacter* Unit = GetOwnerUnit();
	const FCombatAttackRecord* Record = ActiveRecords.Find(Handle.Key.Id);
	return Unit && Record && Handle.IsValid() && Record->Handle == Handle
		&& Handle.Key.Generation == RegistryGeneration
		&& Handle.Key.LifeGeneration == Unit->GetLifeGeneration();
}

bool UCombatAttackComponent::GetAttackRecordSnapshot(
	const FCombatAttackHandle Handle,
	FCombatAttackRecord& OutRecord) const
{
	const ACombatUnitCharacter* Unit = GetOwnerUnit();
	const FCombatAttackRecord* Record = ActiveRecords.Find(Handle.Key.Id);
	if (!Unit || !Record || Record->Handle != Handle || Handle.Key.Generation != RegistryGeneration
		|| Handle.Key.LifeGeneration != Unit->GetLifeGeneration())
	{
		return false;
	}
	OutRecord = *Record;
	return true;
}

void UCombatAttackComponent::HandleOwnerDeath()
{
	CancelSchedules();
	TArray<FCombatAttackHandle> Handles;
	for (const TPair<uint64, FCombatAttackRecord>& Pair : ActiveRecords)
	{
		Handles.Add(Pair.Value.Handle);
	}
	for (const FCombatAttackHandle Handle : Handles)
	{
		AbortRecord(Handle, ECombatAttackOutcome::Cancelled, CombatTags::Failure_Life_NotAlive);
	}
	++RegistryGeneration;
	if (RegistryGeneration == 0) { RegistryGeneration = 1; }
	bAttackReady = false;
}

void UCombatAttackComponent::HandleOwnerRespawn()
{
	CancelSchedules();
	ActiveRecords.Reset();
	++RegistryGeneration;
	if (RegistryGeneration == 0) { RegistryGeneration = 1; }
	bAttackReady = true;
}

void UCombatAttackComponent::HandleOwnerStatusChanged()
{
	ACombatUnitCharacter* Unit = GetOwnerUnit();
	if (Unit && Unit->IsAttackBlocked() && CurrentWindupHandle.IsValid())
	{
		if (FCombatAttackRecord* Record = FindRecord(CurrentWindupHandle))
		{
			CancelWindupForOrder(Record->OrderHandle, CombatTags::Failure_Attack_Blocked);
		}
	}
}

void UCombatAttackComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	bEnding = true;
	CancelSchedules();
	TArray<FCombatAttackHandle> Handles;
	for (const TPair<uint64, FCombatAttackRecord>& Pair : ActiveRecords) { Handles.Add(Pair.Value.Handle); }
	for (const FCombatAttackHandle Handle : Handles)
	{
		AbortRecord(Handle, ECombatAttackOutcome::Cancelled, CombatTags::Order_Failure_Cancelled);
	}
	ActiveRecords.Reset();
	++RegistryGeneration;
	Super::EndPlay(EndPlayReason);
}

ACombatUnitCharacter* UCombatAttackComponent::GetOwnerUnit() const
{
	return Cast<ACombatUnitCharacter>(GetOwner());
}

FCombatAttackRecord* UCombatAttackComponent::FindRecord(const FCombatAttackHandle Handle)
{
	ACombatUnitCharacter* Unit = GetOwnerUnit();
	FCombatAttackRecord* Record = ActiveRecords.Find(Handle.Key.Id);
	return Unit && Record && Handle.IsValid() && Record->Handle == Handle
		&& Handle.Key.Generation == RegistryGeneration
		&& Handle.Key.LifeGeneration == Unit->GetLifeGeneration() ? Record : nullptr;
}

void UCombatAttackComponent::HandleAttackPoint(
	const FCombatAttackHandle Handle,
	const FCombatScheduledTickContext& TickContext)
{
	(void)TickContext;
	FCombatAttackRecord* Record = FindRecord(Handle);
	if (!Record || Record->State != ECombatAttackState::Windup || CurrentWindupHandle != Handle)
	{
		return;
	}
	WindupSchedule = FCombatScheduleHandle();
	CurrentWindupHandle = FCombatAttackHandle();
	Record->State = ECombatAttackState::Launched;
	ReadyOrderHandle = Record->OrderHandle;
	bool bReadyImmediately = true;
	if (UCombatSchedulerSubsystem* Scheduler = GetWorld()->GetSubsystem<UCombatSchedulerSubsystem>())
	{
		// ready 使用起手绝对时间；若 attack point 因卡顿迟到，只调度剩余时间且绝不补发攻击。
		const double ReadyAt = Record->StartedAt + Record->Timing.AttackInterval;
		const double ReadyDelay = FMath::Max(0.0, ReadyAt - GetWorld()->GetTimeSeconds());
		ReadySchedule = Scheduler->ScheduleOnce(
			this,
			ReadyDelay,
			0,
			FCombatScheduledDelegate::CreateWeakLambda(this,
				[this, OrderHandle = Record->OrderHandle](const FCombatScheduledTickContext& Context)
				{
					HandleAttackReady(OrderHandle, Context);
				}));
		bReadyImmediately = !ReadySchedule.IsValid();
	}
	if (bReadyImmediately)
	{
		// 正常路径始终由 Scheduler 驱动；此兜底只防止子系统异常时单位永久卡在 NotReady。
		ReadySchedule = FCombatScheduleHandle();
		ReadyOrderHandle = FCombatOrderHandle();
		bAttackReady = true;
	}
	EmitAttackLog(*Record, nullptr);
	AttackLaunchedDelegate.Broadcast(Handle, Record->OrderHandle);
	if (bReadyImmediately)
	{
		AttackReadyDelegate.Broadcast(Record->OrderHandle);
	}
	const UCombatUnitData* UnitData = Record->Attacker.IsValid() ? Record->Attacker->GetUnitData() : nullptr;
	UCombatProjectileData* ProjectileData = Record->ProjectileDataOverride
		? Record->ProjectileDataOverride.Get() : (UnitData ? UnitData->AttackProjectileData.Get() : nullptr);
	if (ProjectileData)
	{
		// 远程普攻只把 AttackHandle 交给弹体；伤害、闪避、暴击仍由唯一 AttackRecord 结算。
		FCombatProjectileSpec ProjectileSpec;
		ProjectileSpec.ProjectileData = ProjectileData;
		ProjectileSpec.Source = Record->Attacker.Get();
		ProjectileSpec.Target = Record->Target.Get();
		ProjectileSpec.SpawnLocation = ProjectileSpec.Source
			? ProjectileSpec.Source->GetActorLocation() : FVector::ZeroVector;
		ProjectileSpec.Direction = ProjectileSpec.Target
			? ProjectileSpec.Target->GetActorLocation() - ProjectileSpec.SpawnLocation : FVector::ForwardVector;
		ProjectileSpec.MovementType = ECombatProjectileMovementType::Tracking;
		ProjectileSpec.TargetLostPolicy = ProjectileData->TargetLostPolicy;
		ProjectileSpec.HitPolicy = ProjectileData->HitPolicy;
		ProjectileSpec.ParentEvent = Record->EventContext;
		ProjectileSpec.SourceContext.DirectSourceType = ECombatDirectSourceType::Attack;
		ProjectileSpec.AttackHandle = Handle;
		UCombatProjectileSubsystem* Projectiles = GetWorld()->GetSubsystem<UCombatProjectileSubsystem>();
		const FCombatProjectileResult SpawnResult = Projectiles
			? Projectiles->SpawnProjectile(ProjectileSpec) : FCombatProjectileResult();
		if (!SpawnResult.bSuccess)
		{
			AbortRecord(Handle, ECombatAttackOutcome::DamageFailed,
				SpawnResult.FailureTag.IsValid() ? SpawnResult.FailureTag : CombatTags::Failure_ActionUnsupported.GetTag());
		}
		return;
	}
	// 没有 ProjectileData 的单位保持 M4 近战语义，在 attack point 立即 impact。
	FinalizeAttackInternal(Handle);
}

void UCombatAttackComponent::HandleAttackReady(
	const FCombatOrderHandle OrderHandle,
	const FCombatScheduledTickContext& TickContext)
{
	(void)TickContext;
	if (ReadyOrderHandle != OrderHandle)
	{
		return;
	}
	ReadySchedule = FCombatScheduleHandle();
	ReadyOrderHandle = FCombatOrderHandle();
	bAttackReady = true;
	AttackReadyDelegate.Broadcast(OrderHandle);
}

FCombatAttackResult UCombatAttackComponent::FinalizeAttackInternal(
	const FCombatAttackHandle Handle,
	const bool bProjectileImpact,
	ACombatUnitCharacter* ImpactTarget)
{
	FCombatAttackRecord* Record = FindRecord(Handle);
	if (!Record || Record->State != ECombatAttackState::Launched)
	{
		FCombatAttackResult Result;
		Result.Handle = Handle;
		Result.FailureTag = CombatTags::Failure_Attack_StaleHandle;
		return Result;
	}
	ACombatUnitCharacter* Attacker = Record->Attacker.Get();
	ACombatUnitCharacter* Target = Record->Target.Get();
	UCombatTargetingSubsystem* Targeting = GetWorld()->GetSubsystem<UCombatTargetingSubsystem>();
	FCombatTargetingRules ImpactRules = MakeAttackTargetingRules();
	if (bProjectileImpact)
	{
		// 飞行路径已经承担距离与 LOS；impact 只复核原目标、阵营、生命与可选状态。
		ImpactRules.CastRange = TNumericLimits<float>::Max() * 0.5f;
		ImpactRules.bRequireLineOfSight = false;
	}
	if (!Attacker || !Target || Target->GetLifeGeneration() != Record->TargetLifeGeneration || !Targeting
		|| (bProjectileImpact && ImpactTarget != Target)
		|| !Targeting->ValidateUnitTarget(Attacker, Target, ImpactRules).bValid)
	{
		return AbortRecord(Handle, ECombatAttackOutcome::TargetInvalid, CombatTags::Order_Failure_TargetInvalid);
	}

	UCombatRngSubsystem* Rng = GetWorld()->GetSubsystem<UCombatRngSubsystem>();
	const UCombatAbilitySystemComponent* TargetAsc = Target->GetCombatAbilitySystemComponent();
	FCombatRngSubjectId Subject;
	Subject.High = static_cast<uint64>(Attacker->GetUniqueID());
	Subject.Low = Handle.Key.Id ^ (static_cast<uint64>(Handle.Key.LifeGeneration) << 32);
	FCombatRngRollRecord EvasionRoll;
	const float Evasion = TargetAsc
		? TargetAsc->GetNumericAttribute(UCombatAttributeSet::GetEvasionAttribute()) : 0.0f;
	if (!Rng || !Rng->Roll(Record->EventContext.RootEventId, CombatTags::RNG_Attack_Evasion,
		Subject, 0, Evasion, EvasionRoll))
	{
		return AbortRecord(Handle, ECombatAttackOutcome::DamageFailed, CombatTags::Failure_ActionUnsupported);
	}
	if (EvasionRoll.bSuccess)
	{
		return AbortRecord(Handle, ECombatAttackOutcome::Evaded, CombatTags::Failure_Attack_Evaded);
	}

	FCombatRngRollRecord CritRoll;
	if (!Rng->Roll(Record->EventContext.RootEventId, CombatTags::RNG_Attack_Crit,
		Subject, 0, Record->CriticalChance, CritRoll))
	{
		return AbortRecord(Handle, ECombatAttackOutcome::DamageFailed, CombatTags::Failure_ActionUnsupported);
	}
	Record->bCritical = CritRoll.bSuccess;
	const float DamageAmount = (Record->BaseDamage + Record->BonusDamage)
		* (Record->bCritical ? Record->CriticalMultiplier : 1.0f);
	FCombatDamageRequest DamageRequest;
	DamageRequest.Source = Attacker;
	DamageRequest.Target = Target;
	DamageRequest.Amount = DamageAmount;
	DamageRequest.DamageType = Record->DamageType;
	DamageRequest.ParentEvent = Record->EventContext;
	DamageRequest.SourceContext.DirectSourceType = ECombatDirectSourceType::Attack;
	const FCombatDamageResult DamageResult = GetWorld()->GetSubsystem<UCombatDamageSubsystem>()->DealDamage(DamageRequest);
	if (!DamageResult.bSuccess)
	{
		return AbortRecord(Handle, ECombatAttackOutcome::DamageFailed, DamageResult.FailureTag);
	}
	const float AppliedDamage = DamageResult.Event.AppliedAmount
		+ ExecuteOnHitActions(*Record, DamageResult.Event.Context);
	return CompleteRecord(*Record, ECombatAttackOutcome::Landed, FGameplayTag(), AppliedDamage);
}

FCombatAttackResult UCombatAttackComponent::AbortRecord(
	const FCombatAttackHandle Handle,
	const ECombatAttackOutcome Outcome,
	const FGameplayTag FailureTag)
{
	FCombatAttackRecord* Record = FindRecord(Handle);
	if (!Record)
	{
		FCombatAttackResult Result;
		Result.Handle = Handle;
		Result.Outcome = Outcome;
		Result.FailureTag = CombatTags::Failure_Attack_StaleHandle;
		return Result;
	}
	return CompleteRecord(*Record, Outcome, FailureTag, 0.0f);
}

FCombatAttackResult UCombatAttackComponent::CompleteRecord(
	FCombatAttackRecord& Record,
	const ECombatAttackOutcome Outcome,
	const FGameplayTag FailureTag,
	const float AppliedDamage)
{
	FCombatAttackResult Result;
	Result.bSuccess = Outcome == ECombatAttackOutcome::Landed;
	Result.Handle = Record.Handle;
	Result.Outcome = Outcome;
	Result.FailureTag = FailureTag;
	Result.AppliedDamage = AppliedDamage;
	Record.State = Result.bSuccess ? ECombatAttackState::Landed : ECombatAttackState::Failed;
	const uint64 RecordId = Record.Handle.Key.Id;
	EmitAttackLog(Record, &Result);
	ActiveRecords.Remove(RecordId);
	if (CurrentWindupHandle == Result.Handle)
	{
		CurrentWindupHandle = FCombatAttackHandle();
	}
	LastFinalizedResult = Result;
	if (!bEnding)
	{
		AttackFinalizedDelegate.Broadcast(LastFinalizedResult);
	}
	return Result;
}

float UCombatAttackComponent::ExecuteOnHitActions(
	const FCombatAttackRecord& Record,
	const FCombatEventContext& ParentEvent) const
{
	ACombatUnitCharacter* Source = Record.Attacker.Get();
	ACombatUnitCharacter* Target = Record.Target.Get();
	if (!Source || !Target)
	{
		return 0.0f;
	}
	float AppliedDamage = 0.0f;
	for (const FCombatOnHitAction& Action : Record.OnHitActions)
	{
		if (Action.Type == ECombatOnHitActionType::Damage)
		{
			FCombatDamageRequest Request;
			Request.Source = Source;
			Request.Target = Target;
			Request.Amount = Action.Magnitude;
			Request.DamageType = Action.DamageType;
			Request.ParentEvent = ParentEvent;
			Request.SourceContext.DirectSourceType = ECombatDirectSourceType::Attack;
			const FCombatDamageResult Result = GetWorld()->GetSubsystem<UCombatDamageSubsystem>()->DealDamage(Request);
			if (Result.bSuccess) { AppliedDamage += Result.Event.AppliedAmount; }
		}
		else if (Action.Type == ECombatOnHitActionType::ApplyModifier && Action.ModifierData)
		{
			FCombatModifierApplyRequest Request;
			Request.Source = Source;
			Request.ModifierData = Action.ModifierData;
			Request.DurationOverride = Action.DurationOverride;
			Request.RuntimeParameterOverrides = Action.RuntimeParameterOverrides;
			Target->GetCombatModifierComponent()->ApplyModifier(Request);
		}
	}
	return AppliedDamage;
}

FCombatTargetingRules UCombatAttackComponent::MakeAttackTargetingRules() const
{
	FCombatTargetingRules Rules;
	Rules.TargetTeamTag = CombatTags::TargetTeam_Enemy;
	Rules.bAllowMagicImmune = true;
	const ACombatUnitCharacter* Unit = GetOwnerUnit();
	const UCombatAbilitySystemComponent* Asc = Unit ? Unit->GetCombatAbilitySystemComponent() : nullptr;
	Rules.CastRange = Asc ? Asc->GetNumericAttribute(UCombatAttributeSet::GetAttackRangeAttribute()) : 0.0f;
	Rules.bRequireLineOfSight = Unit && Unit->GetUnitData() ? Unit->GetUnitData()->bRequireAttackLineOfSight : false;
	return Rules;
}

bool UCombatAttackComponent::IsFacingTarget(const ACombatUnitCharacter& Target) const
{
	const ACombatUnitCharacter* Unit = GetOwnerUnit();
	if (!Unit)
	{
		return false;
	}
	const FVector ToTarget = (Target.GetActorLocation() - Unit->GetActorLocation()).GetSafeNormal2D();
	if (ToTarget.IsNearlyZero())
	{
		return true;
	}
	const float Dot = FMath::Clamp(FVector::DotProduct(Unit->GetActorForwardVector().GetSafeNormal2D(), ToTarget), -1.0f, 1.0f);
	const float AngleDegrees = FMath::RadiansToDegrees(FMath::Acos(Dot));
	const float Tolerance = Unit->GetUnitData() ? Unit->GetUnitData()->AttackFacingToleranceDegrees : 15.0f;
	return AngleDegrees <= Tolerance + KINDA_SMALL_NUMBER;
}

void UCombatAttackComponent::EmitAttackLog(
	const FCombatAttackRecord& Record,
	const FCombatAttackResult* Result) const
{
	UCombatEventSubsystem* Events = GetWorld() ? GetWorld()->GetSubsystem<UCombatEventSubsystem>() : nullptr;
	ACombatUnitCharacter* Source = Record.Attacker.Get();
	ACombatUnitCharacter* Target = Record.Target.Get();
	if (!Events || !Source)
	{
		return;
	}
	FCombatLogRecord Log;
	Log.Context = Record.EventContext;
	Log.EventType = !Result ? CombatTags::Event_Combat_AttackLaunched.GetTag()
		: (Result->bSuccess ? CombatTags::Event_Combat_AttackLanded.GetTag() : CombatTags::Event_Combat_AttackFailed.GetTag());
	Log.FailureTag = Result ? Result->FailureTag : FGameplayTag();
	Log.Source.DirectSourceType = ECombatDirectSourceType::Attack;
	Log.SourceActorId = Source->GetUniqueID();
	Log.TargetActorId = Target ? Target->GetUniqueID() : 0;
	Log.UnitLifeGeneration = Record.Handle.Key.LifeGeneration;
	Log.RequestedAmount = Record.BaseDamage + Record.BonusDamage;
	Log.AppliedAmount = Result ? Result->AppliedDamage : 0.0f;
	Log.Diagnostic = FString::Printf(TEXT("Handle=%s Order=%s State=%d Outcome=%d Crit=%d Orbs=%d"),
		*Record.Handle.ToString(), *Record.OrderHandle.ToString(), static_cast<int32>(Record.State),
		Result ? static_cast<int32>(Result->Outcome) : -1, Record.bCritical, Record.ClaimedOrbs.Num());
	Events->Emit(Log);
}

void UCombatAttackComponent::CancelSchedules()
{
	if (UCombatSchedulerSubsystem* Scheduler = GetWorld() ? GetWorld()->GetSubsystem<UCombatSchedulerSubsystem>() : nullptr)
	{
		Scheduler->Cancel(WindupSchedule);
		Scheduler->Cancel(ReadySchedule);
	}
	WindupSchedule = FCombatScheduleHandle();
	ReadySchedule = FCombatScheduleHandle();
	CurrentWindupHandle = FCombatAttackHandle();
	ReadyOrderHandle = FCombatOrderHandle();
}
