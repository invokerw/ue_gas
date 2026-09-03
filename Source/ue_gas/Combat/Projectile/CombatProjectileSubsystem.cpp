#include "Combat/Projectile/CombatProjectileSubsystem.h"

#include "AbilitySystemComponent.h"
#include "Engine/World.h"

#include "Combat/Attack/CombatAttackComponent.h"
#include "Combat/Ability/CombatAbilitySystemComponent.h"
#include "Combat/Combat/CombatDamageSubsystem.h"
#include "Combat/Core/CombatNumericPolicy.h"
#include "Combat/Core/CombatTags.h"
#include "Combat/Data/CombatDefinitionData.h"
#include "Combat/Modifiers/CombatModifierComponent.h"
#include "Combat/Projectile/CombatProjectileActor.h"
#include "Combat/Targeting/CombatTeamSubsystem.h"
#include "Combat/Unit/CombatUnitCharacter.h"

namespace CombatProjectilePrivate
{
	// 同一推进步内用固定容差合并命中距离，保证并列结果可稳定排序。
	constexpr float HitTieToleranceCm = 0.1f;

	/** Blocker 排在 Unit 前，最后使用 Actor UniqueID 稳定打破并列。 */
	bool IsHitBefore(const FHitResult& Left, const FHitResult& Right)
	{
		if (!FMath::IsNearlyEqual(Left.Distance, Right.Distance, HitTieToleranceCm))
		{
			return Left.Distance < Right.Distance;
		}
		const bool bLeftUnit = Cast<ACombatUnitCharacter>(Left.GetActor()) != nullptr;
		const bool bRightUnit = Cast<ACombatUnitCharacter>(Right.GetActor()) != nullptr;
		if (bLeftUnit != bRightUnit)
		{
			return !bLeftUnit;
		}
		const uint32 LeftId = Left.GetActor() ? Left.GetActor()->GetUniqueID() : 0;
		const uint32 RightId = Right.GetActor() ? Right.GetActor()->GetUniqueID() : 0;
		return LeftId < RightId;
	}
}

FCombatProjectileResult UCombatProjectileSubsystem::SpawnProjectile(const FCombatProjectileSpec& Spec)
{
	FCombatProjectileResult Result;
	ACombatUnitCharacter* Source = Spec.Source;
	const UCombatProjectileData* Data = Spec.ProjectileData;
	if (!IsValid(Source) || !Source->HasAuthority() || Source->GetWorld() != GetWorld())
	{
		Result.FailureTag = CombatTags::Failure_Authority;
		return Result;
	}
	const float RequestedSpeed = Spec.SpeedOverride >= 0.0f ? Spec.SpeedOverride : (Data ? Data->Speed : 0.0f);
	const float RequestedRadius = Spec.RadiusOverride >= 0.0f ? Spec.RadiusOverride : (Data ? Data->Radius : 0.0f);
	const float RequestedDistance = Spec.MaxDistanceOverride >= 0.0f ? Spec.MaxDistanceOverride : (Data ? Data->MaxDistance : 0.0f);
	if (!Data || !Data->GetPrimaryAssetId().IsValid()
		|| !FMath::IsFinite(Spec.SpeedOverride)
		|| !FMath::IsFinite(Spec.RadiusOverride)
		|| !FMath::IsFinite(Spec.MaxDistanceOverride)
		|| !FMath::IsFinite(RequestedSpeed) || RequestedSpeed <= 0.0f
		|| !FMath::IsFinite(RequestedRadius) || RequestedRadius < 0.0f
		|| !FMath::IsFinite(RequestedDistance) || RequestedDistance <= 0.0f
		|| !FMath::IsFinite(Data->MaxLifetime) || Data->MaxLifetime < 0.0f
		|| !FMath::IsFinite(Data->MaxSimulationStep) || Data->MaxSimulationStep < 1.0f
		|| Spec.SpawnLocation.ContainsNaN() || Spec.Direction.ContainsNaN())
	{
		Result.FailureTag = CombatTags::Failure_InvalidNumber;
		return Result;
	}
	if (Spec.MovementType == ECombatProjectileMovementType::Tracking
		&& (!IsValid(Spec.Target) || Spec.Target->GetWorld() != GetWorld()))
	{
		Result.FailureTag = CombatTags::Failure_Target_Invalid;
		return Result;
	}
	for (const FCombatProjectileImpactAction& Action : Spec.ImpactActions)
	{
		if (!FMath::IsFinite(Action.Magnitude) || Action.Magnitude < 0.0f
			|| !FMath::IsFinite(Action.DurationOverride) || Action.DurationOverride < -1.0f
			|| !FMath::IsFinite(Action.MotionSpeed) || Action.MotionSpeed < 0.0f
			|| (Action.Type == ECombatProjectileImpactActionType::ApplyModifier && !Action.ModifierData))
		{
			Result.FailureTag = CombatTags::Failure_InvalidNumber;
			return Result;
		}
	}

	FCombatProjectileRuntimeRecord Record;
	Record.Handle.Key.Id = NextProjectileId++;
	Record.Handle.Key.Generation = ProjectileGeneration;
	Record.Handle.Key.LifeGeneration = Source->GetLifeGeneration();
	Record.Spec = Spec;
	Record.Spec.SourceTeam = Source->GetCombatTeamId();
	Record.Spec.SourceContext.DirectSourceType = ECombatDirectSourceType::Projectile;
	Record.Spec.SourceContext.ProjectileDefinitionId = Data->GetPrimaryAssetId();
	if (!Record.Spec.ParentEvent.IsValid())
	{
		if (UCombatEventSubsystem* Events = GetWorld()->GetSubsystem<UCombatEventSubsystem>())
		{
			Record.Spec.ParentEvent = Events->CreateRootEvent();
		}
	}
	Record.Direction = Spec.Direction.GetSafeNormal();
	if (Record.Direction.IsNearlyZero())
	{
		Record.Direction = FVector::ForwardVector;
	}
	Record.LastKnownTargetLocation = Spec.Target
		? Spec.Target->GetActorLocation() : Spec.SpawnLocation + Record.Direction * RequestedDistance;
	Record.SourceLifeGeneration = Source->GetLifeGeneration();
	Record.TargetLifeGeneration = Spec.Target ? Spec.Target->GetLifeGeneration() : 0;
	Record.Speed = RequestedSpeed;
	Record.Radius = RequestedRadius;
	Record.MaxDistance = RequestedDistance;
	Record.MaxLifetime = Data->MaxLifetime;
	Record.MaxSimulationStep = Data->MaxSimulationStep;
	Record.CollisionProfileName = Data->CollisionProfileName;

	UClass* ActorClass = Data->ProjectileActorClass
		? Data->ProjectileActorClass.Get() : ACombatProjectileActor::StaticClass();
	FActorSpawnParameters Params;
	Params.Owner = Source;
	Params.Instigator = Source;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ACombatProjectileActor* Actor = GetWorld()->SpawnActor<ACombatProjectileActor>(
		ActorClass, Spec.SpawnLocation, Record.Direction.Rotation(), Params);
	if (!Actor)
	{
		Result.FailureTag = CombatTags::Failure_ActionUnsupported;
		return Result;
	}
	Actor->InitializeProjectile(Record.Handle, Data->GetPrimaryAssetId(), Record.Radius, Spec.PredictionKey);
	Record.Actor = Actor;
	const FCombatProjectileHandle Handle = Record.Handle;
	ActiveProjectiles.Add(Handle.Key.Id, MoveTemp(Record));
	EmitProjectileLog(ActiveProjectiles[Handle.Key.Id], CombatTags::Event_Combat_ProjectileSpawned, nullptr, 0.0f, FGameplayTag());

	Result.bSuccess = true;
	Result.Handle = Handle;
	LastSpawnedHandle = Handle;
	return Result;
}

bool UCombatProjectileSubsystem::AdvanceProjectile(
	const FCombatProjectileHandle Handle,
	const float DeltaSeconds)
{
	FCombatProjectileRuntimeRecord* Record = ActiveProjectiles.Find(Handle.Key.Id);
	if (!Record || Record->Handle != Handle || Handle.Key.Generation != ProjectileGeneration
		|| !FMath::IsFinite(DeltaSeconds) || DeltaSeconds < 0.0f)
	{
		return false;
	}
	if (!IsValid(Record->Spec.Source))
	{
		FinishProjectile(Handle, ECombatProjectileFinishReason::EndPlay, nullptr, 0.0f,
			CombatTags::Failure_Projectile_StaleHandle);
		return false;
	}
	ACombatProjectileActor* Actor = Record->Actor.Get();
	if (!Actor)
	{
		FinishProjectile(Handle, ECombatProjectileFinishReason::EndPlay, nullptr, 0.0f,
			CombatTags::Failure_Projectile_StaleHandle);
		return false;
	}
	Record->Age += DeltaSeconds;
	if (Record->MaxLifetime > 0.0f && Record->Age + KINDA_SMALL_NUMBER >= Record->MaxLifetime)
	{
		FinishProjectile(Handle, ECombatProjectileFinishReason::Timeout, nullptr, 0.0f,
			CombatTags::Failure_Projectile_Timeout);
		return false;
	}

	if (Record->Spec.MovementType == ECombatProjectileMovementType::Tracking)
	{
		if (!Record->bUsingLastKnownPoint && IsTrackingTargetValid(*Record))
		{
			Record->LastKnownTargetLocation = Record->Spec.Target->GetActorLocation();
		}
		else if (!Record->bUsingLastKnownPoint)
		{
			if (Record->Spec.TargetLostPolicy == ECombatProjectileTargetLostPolicy::Fizzle)
			{
				FinishProjectile(Handle, ECombatProjectileFinishReason::TargetLost, nullptr, 0.0f,
					CombatTags::Failure_Projectile_TargetLost);
				return false;
			}
			Record->bUsingLastKnownPoint = true;
		}
		const FVector ToGoal = Record->LastKnownTargetLocation - Actor->GetActorLocation();
		if (ToGoal.SizeSquared() <= FMath::Square(FMath::Max(1.0f, Record->Radius)))
		{
			FinishProjectile(Handle, ECombatProjectileFinishReason::TargetLost, nullptr, 0.0f,
				CombatTags::Failure_Projectile_TargetLost);
			return false;
		}
		Record->Direction = ToGoal.GetSafeNormal();
	}

	float RemainingMove = Record->Speed * DeltaSeconds;
	while (RemainingMove > KINDA_SMALL_NUMBER)
	{
		const float DistanceBudget = Record->MaxDistance - Record->TravelledDistance;
		if (DistanceBudget <= KINDA_SMALL_NUMBER)
		{
			FinishProjectile(Handle, ECombatProjectileFinishReason::MaxDistance, nullptr, 0.0f,
				CombatTags::Failure_Projectile_Timeout);
			return false;
		}
		float StepDistance = FMath::Min3(RemainingMove, Record->MaxSimulationStep, DistanceBudget);
		if (Record->Spec.MovementType == ECombatProjectileMovementType::Tracking && Record->bUsingLastKnownPoint)
		{
			StepDistance = FMath::Min(StepDistance,
				FVector::Distance(Actor->GetActorLocation(), Record->LastKnownTargetLocation));
		}
		const FVector Start = Actor->GetActorLocation();
		const FVector End = Start + Record->Direction * StepDistance;
		if (!SweepStep(*Record, Start, End))
		{
			return false;
		}
		Actor->SetActorLocation(End, false, nullptr, ETeleportType::TeleportPhysics);
		Actor->SetActorRotation(Record->Direction.Rotation());
		Record->TravelledDistance += StepDistance;
		RemainingMove -= StepDistance;
		if (Record->Spec.MovementType == ECombatProjectileMovementType::Tracking && Record->bUsingLastKnownPoint
			&& FVector::DistSquared(End, Record->LastKnownTargetLocation) <= 1.0f)
		{
			FinishProjectile(Handle, ECombatProjectileFinishReason::TargetLost, nullptr, 0.0f,
				CombatTags::Failure_Projectile_TargetLost);
			return false;
		}
	}
	if (Record->TravelledDistance + KINDA_SMALL_NUMBER >= Record->MaxDistance)
	{
		FinishProjectile(Handle, ECombatProjectileFinishReason::MaxDistance, nullptr, 0.0f,
			CombatTags::Failure_Projectile_Timeout);
		return false;
	}
	return true;
}

FCombatProjectileResult UCombatProjectileSubsystem::CancelProjectile(
	const FCombatProjectileHandle Handle,
	const FGameplayTag FailureTag)
{
	return FinishProjectile(Handle, ECombatProjectileFinishReason::Cancelled, nullptr, 0.0f,
		FailureTag.IsValid() ? FailureTag : CombatTags::Order_Failure_Cancelled.GetTag());
}

int32 UCombatProjectileSubsystem::CancelProjectilesForAbility(
	ACombatUnitCharacter* Source,
	const FCombatEventId ActivationId)
{
	if (!Source || !ActivationId.IsValid())
	{
		return 0;
	}
	TArray<FCombatProjectileHandle> Handles;
	for (const TPair<uint64, FCombatProjectileRuntimeRecord>& Pair : ActiveProjectiles)
	{
		if (Pair.Value.Spec.bCancelWithSourceAbility && Pair.Value.Spec.Source == Source
			&& Pair.Value.Spec.AbilityActivationId == ActivationId)
		{
			Handles.Add(Pair.Value.Handle);
		}
	}
	for (const FCombatProjectileHandle Handle : Handles)
	{
		CancelProjectile(Handle);
	}
	return Handles.Num();
}

void UCombatProjectileSubsystem::NotifyProjectileActorEndPlay(const FCombatProjectileHandle Handle)
{
	if (!bDeinitializing)
	{
		FinishProjectile(Handle, ECombatProjectileFinishReason::EndPlay, nullptr, 0.0f,
			CombatTags::Order_Failure_Cancelled);
	}
}

bool UCombatProjectileSubsystem::IsProjectileActive(const FCombatProjectileHandle Handle) const
{
	const FCombatProjectileRuntimeRecord* Record = ActiveProjectiles.Find(Handle.Key.Id);
	return Record && Record->Handle == Handle && Handle.Key.Generation == ProjectileGeneration;
}

void UCombatProjectileSubsystem::Deinitialize()
{
	bDeinitializing = true;
	TArray<FCombatProjectileHandle> Handles;
	for (const TPair<uint64, FCombatProjectileRuntimeRecord>& Pair : ActiveProjectiles)
	{
		Handles.Add(Pair.Value.Handle);
	}
	for (const FCombatProjectileHandle Handle : Handles)
	{
		FinishProjectile(Handle, ECombatProjectileFinishReason::EndPlay, nullptr, 0.0f,
			CombatTags::Order_Failure_Cancelled);
	}
	ActiveProjectiles.Reset();
	++ProjectileGeneration;
	if (ProjectileGeneration == 0) { ProjectileGeneration = 1; }
	Super::Deinitialize();
}

FCombatProjectileResult UCombatProjectileSubsystem::FinishProjectile(
	const FCombatProjectileHandle Handle,
	const ECombatProjectileFinishReason Reason,
	AActor* HitActor,
	const float AppliedDamage,
	const FGameplayTag FailureTag)
{
	FCombatProjectileResult Result;
	FCombatProjectileRuntimeRecord* Record = ActiveProjectiles.Find(Handle.Key.Id);
	if (!Record || Record->Handle != Handle || Handle.Key.Generation != ProjectileGeneration)
	{
		Result.Handle = Handle;
		Result.FinishReason = Reason;
		Result.FailureTag = CombatTags::Failure_Projectile_StaleHandle;
		return Result;
	}
	const FCombatProjectileRuntimeRecord Snapshot = *Record;
	ACombatProjectileActor* Actor = Record->Actor.Get();
	ActiveProjectiles.Remove(Handle.Key.Id);

	Result.bSuccess = true;
	Result.Handle = Handle;
	Result.FinishReason = Reason;
	Result.HitActor = HitActor;
	Result.AppliedDamage = AppliedDamage;
	Result.FailureTag = FailureTag;
	LastFinishedResult = Result;
	EmitProjectileLog(Snapshot, CombatTags::Event_Combat_ProjectileFinished, HitActor, AppliedDamage, FailureTag);

	if (Snapshot.Spec.AttackHandle.IsValid() && Reason != ECombatProjectileFinishReason::Hit)
	{
		if (ACombatUnitCharacter* Source = Snapshot.Spec.Source; IsValid(Source))
		{
			Source->GetCombatAttackComponent()->FailLaunchedAttackFromProjectile(
				Snapshot.Spec.AttackHandle,
				FailureTag.IsValid() ? FailureTag : CombatTags::Failure_Projectile_TargetLost.GetTag());
		}
	}
	ProjectileFinishedDelegate.Broadcast(LastFinishedResult);
	if (Actor && !Actor->IsActorBeingDestroyed())
	{
		Actor->PrepareForSubsystemDestroy();
		Actor->Destroy();
	}
	return Result;
}

bool UCombatProjectileSubsystem::SweepStep(
	FCombatProjectileRuntimeRecord& Record,
	const FVector& Start,
	const FVector& End)
{
	TArray<FHitResult> Hits;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(CombatProjectileSweep), false, Record.Actor.Get());
	if (!Record.Spec.HitPolicy.bHitSelf && Record.Spec.Source)
	{
		Params.AddIgnoredActor(Record.Spec.Source);
	}
	GetWorld()->SweepMultiByProfile(
		Hits, Start, End, FQuat::Identity, Record.CollisionProfileName,
		FCollisionShape::MakeSphere(FMath::Max(0.1f, Record.Radius)), Params);
	Hits.Sort(CombatProjectilePrivate::IsHitBefore);

	for (const FHitResult& Hit : Hits)
	{
		AActor* HitActor = Hit.GetActor();
		if (ACombatUnitCharacter* Unit = Cast<ACombatUnitCharacter>(HitActor))
		{
			if (Record.AlreadyHit.Contains(Unit) || !CanHitUnit(Record, *Unit))
			{
				continue;
			}
			Record.AlreadyHit.Add(Unit);
			const float AppliedDamage = ExecuteImpact(Record, *Unit);
			EmitProjectileLog(Record, CombatTags::Event_Combat_ProjectileHit, Unit, AppliedDamage, FGameplayTag());
			if (Record.Spec.HitPolicy.bDestroyOnFirstUnitHit)
			{
				FinishProjectile(Record.Handle, ECombatProjectileFinishReason::Hit, Unit, AppliedDamage, FGameplayTag());
				return false;
			}
			continue;
		}
		if (Hit.bBlockingHit && Record.Spec.HitPolicy.bStopOnWorld)
		{
			FinishProjectile(Record.Handle, ECombatProjectileFinishReason::Blocked, HitActor, 0.0f,
				CombatTags::Failure_Projectile_Blocked);
			return false;
		}
	}
	return true;
}

bool UCombatProjectileSubsystem::CanHitUnit(
	const FCombatProjectileRuntimeRecord& Record,
	ACombatUnitCharacter& Unit) const
{
	if (Unit.GetWorld() != GetWorld() || Unit.GetLifeState() != ECombatLifeState::Alive)
	{
		return false;
	}
	if (&Unit == Record.Spec.Source)
	{
		return Record.Spec.HitPolicy.bHitSelf;
	}
	// 普攻弹体只允许原 AttackRecord 目标触发结算，避免沿途单位截走唯一攻击记录。
	if (Record.Spec.AttackHandle.IsValid() && Record.Spec.Target != &Unit)
	{
		return false;
	}
	const UCombatAbilitySystemComponent* Asc = Unit.GetCombatAbilitySystemComponent();
	if (!Asc || Asc->HasMatchingGameplayTag(CombatTags::State_Untargetable)
		|| Asc->HasMatchingGameplayTag(CombatTags::State_OutOfGame))
	{
		return false;
	}
	const UCombatTeamSubsystem* Teams = GetWorld()->GetSubsystem<UCombatTeamSubsystem>();
	const ECombatTeamRelation Relation = Teams
		? Teams->GetRelation(Record.Spec.SourceTeam, Unit.GetCombatTeamId()) : ECombatTeamRelation::Invalid;
	return (Relation == ECombatTeamRelation::Hostile && Record.Spec.HitPolicy.bHitHostile)
		|| (Relation == ECombatTeamRelation::Friendly && Record.Spec.HitPolicy.bHitFriendly);
}

float UCombatProjectileSubsystem::ExecuteImpact(
	FCombatProjectileRuntimeRecord& Record,
	ACombatUnitCharacter& Unit)
{
	if (Record.Spec.AttackHandle.IsValid())
	{
		ACombatUnitCharacter* Source = Record.Spec.Source;
		const FCombatAttackResult Result = Source
			? Source->GetCombatAttackComponent()->FinalizeAttackFromProjectile(Record.Spec.AttackHandle, &Unit)
			: FCombatAttackResult();
		return Result.AppliedDamage;
	}

	float AppliedDamage = 0.0f;
	for (const FCombatProjectileImpactAction& Action : Record.Spec.ImpactActions)
	{
		if (Action.Type == ECombatProjectileImpactActionType::Damage)
		{
			FCombatDamageRequest Request;
			Request.Source = Record.Spec.Source;
			Request.Target = &Unit;
			Request.Amount = Action.Magnitude;
			Request.DamageType = Action.DamageType;
			Request.ParentEvent = Record.Spec.ParentEvent;
			Request.SourceContext = Record.Spec.SourceContext;
			const FCombatDamageResult Result = GetWorld()->GetSubsystem<UCombatDamageSubsystem>()->DealDamage(Request);
			if (Result.bSuccess) { AppliedDamage += Result.Event.AppliedAmount; }
		}
		else if (Action.Type == ECombatProjectileImpactActionType::ApplyModifier && Action.ModifierData)
		{
			FCombatModifierApplyRequest Request;
			Request.Source = Record.Spec.Source;
			Request.ModifierData = Action.ModifierData;
			Request.DurationOverride = Action.DurationOverride;
			if (Action.bMotionToSource && Record.Spec.Source)
			{
				Request.bHasInitialMotionRequest = true;
				Request.InitialMotionRequest.Channel = ECombatMotionChannel::Horizontal;
				Request.InitialMotionRequest.Priority = Action.MotionPriority;
				Request.InitialMotionRequest.TargetLocation = Record.Spec.Source->GetActorLocation();
				Request.InitialMotionRequest.Source = Record.Spec.Source;
				Request.InitialMotionRequest.Speed = Action.MotionSpeed;
				Request.InitialMotionRequest.bSweep = true;
				Request.InitialMotionRequest.bProjectToNavigation = true;
				Request.InitialMotionRequest.ParentEvent = Record.Spec.ParentEvent;
				Request.InitialMotionRequest.SourceContext = Record.Spec.SourceContext;
			}
			Unit.GetCombatModifierComponent()->ApplyModifier(Request);
		}
	}
	return AppliedDamage;
}

bool UCombatProjectileSubsystem::IsTrackingTargetValid(const FCombatProjectileRuntimeRecord& Record) const
{
	ACombatUnitCharacter* Target = Record.Spec.Target;
	const UCombatAbilitySystemComponent* Asc = Target ? Target->GetCombatAbilitySystemComponent() : nullptr;
	return IsValid(Target) && Target->GetWorld() == GetWorld()
		&& Target->GetLifeGeneration() == Record.TargetLifeGeneration
		&& Target->GetLifeState() == ECombatLifeState::Alive
		&& Asc && !Asc->HasMatchingGameplayTag(CombatTags::State_Untargetable)
		&& !Asc->HasMatchingGameplayTag(CombatTags::State_OutOfGame);
}

void UCombatProjectileSubsystem::EmitProjectileLog(
	const FCombatProjectileRuntimeRecord& Record,
	const FGameplayTag& EventType,
	AActor* HitActor,
	const float AppliedDamage,
	const FGameplayTag FailureTag) const
{
	UCombatEventSubsystem* Events = GetWorld() ? GetWorld()->GetSubsystem<UCombatEventSubsystem>() : nullptr;
	if (!Events || !IsValid(Record.Spec.Source))
	{
		return;
	}
	FCombatLogRecord Log;
	Log.Context = Record.Spec.ParentEvent;
	Log.EventType = EventType;
	Log.FailureTag = FailureTag;
	Log.Source = Record.Spec.SourceContext;
	Log.SourceActorId = Record.Spec.Source->GetUniqueID();
	Log.TargetActorId = HitActor ? HitActor->GetUniqueID() : 0;
	Log.UnitLifeGeneration = Record.SourceLifeGeneration;
	Log.RequestedAmount = Record.TravelledDistance;
	Log.AppliedAmount = AppliedDamage;
	Log.Diagnostic = FString::Printf(TEXT("Projectile=%s Definition=%s Distance=%.2f Hits=%d"),
		*Record.Handle.ToString(), *Record.Spec.SourceContext.ProjectileDefinitionId.ToString(),
		Record.TravelledDistance, Record.AlreadyHit.Num());
	Events->Emit(Log);
}
