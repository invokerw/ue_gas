#include "Combat/Motion/CombatMotionComponent.h"

#include "GameFramework/CharacterMovementComponent.h"
#include "NavigationSystem.h"

#include "Combat/Core/CombatTags.h"
#include "Combat/Log/CombatEventSubsystem.h"
#include "Combat/Order/CombatOrderComponent.h"
#include "Combat/Unit/CombatUnitCharacter.h"

UCombatMotionComponent::UCombatMotionComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

FCombatMotionResult UCombatMotionComponent::TryAcquireMotion(const FCombatMotionRequest& Request)
{
	FCombatMotionResult Result;
	ACombatUnitCharacter* Unit = GetOwnerUnit();
	if (!Unit || !Unit->HasAuthority())
	{
		Result.FailureTag = CombatTags::Failure_Authority;
		return Result;
	}
	if (Unit->GetLifeState() != ECombatLifeState::Alive)
	{
		Result.FailureTag = CombatTags::Failure_Life_NotAlive;
		return Result;
	}
	if (Request.TargetLocation.ContainsNaN() || !FMath::IsFinite(Request.Speed) || Request.Speed <= 0.0f
		|| !FMath::IsFinite(Request.StopDistance) || Request.StopDistance < 0.0f)
	{
		Result.FailureTag = CombatTags::Failure_InvalidNumber;
		return Result;
	}

	TArray<FCombatMotionHandle> Conflicts;
	if (UsesHorizontal(Request.Channel) && HorizontalOwner.IsValid()) { Conflicts.AddUnique(HorizontalOwner); }
	if (UsesVertical(Request.Channel) && VerticalOwner.IsValid()) { Conflicts.AddUnique(VerticalOwner); }
	for (const FCombatMotionHandle Conflict : Conflicts)
	{
		const FActiveMotion* Existing = ActiveMotions.Find(Conflict.Key.Id);
		if (Existing && Existing->Request.Priority >= Request.Priority)
		{
			Result.FailureTag = CombatTags::Failure_Motion_ChannelBusy;
			return Result;
		}
	}
	{
		// 抢占期间可能暂时清空 registry；直到新请求登记完成前都不能恢复被暂停的 Order。
		TGuardValue<bool> ReplacementGuard(bAcquiringReplacement, true);
		for (const FCombatMotionHandle Conflict : Conflicts)
		{
			FinishMotion(Conflict, ECombatMotionFinishReason::Interrupted, CombatTags::Failure_Motion_ChannelBusy);
		}
	}

	FActiveMotion Motion;
	Motion.Handle.Key.Id = NextMotionId++;
	Motion.Handle.Key.Generation = MotionGeneration;
	Motion.Handle.Key.LifeGeneration = Unit->GetLifeGeneration();
	Motion.Request = Request;
	Motion.StartLocation = Unit->GetActorLocation();
	if (!Motion.Request.Source)
	{
		Motion.Request.Source = Unit;
	}
	if (!Motion.Request.ParentEvent.IsValid())
	{
		if (UCombatEventSubsystem* Events = GetWorld()->GetSubsystem<UCombatEventSubsystem>())
		{
			Motion.Request.ParentEvent = Events->CreateRootEvent();
		}
	}
	const FCombatMotionHandle Handle = Motion.Handle;
	ActiveMotions.Add(Handle.Key.Id, MoveTemp(Motion));
	if (UsesHorizontal(Request.Channel)) { HorizontalOwner = Handle; }
	if (UsesVertical(Request.Channel)) { VerticalOwner = Handle; }
	if (UCharacterMovementComponent* Movement = Unit->GetCharacterMovement())
	{
		Movement->StopMovementImmediately();
	}
	if (UCombatOrderComponent* Orders = Unit->GetCombatOrderComponent())
	{
		Orders->HandleOwnerMotionStarted();
	}
	// Motion 抢占普通移动时立即暂停 Crowd steering，避免同帧继续施加旧 corridor 速度。
	Unit->RefreshServerMovementState();
	SetComponentTickEnabled(true);
	EmitMotionLog(ActiveMotions.FindChecked(Handle.Key.Id), CombatTags::Event_Combat_MotionStarted,
		ECombatMotionFinishReason::Completed, FGameplayTag());
	Result.bSuccess = true;
	Result.Handle = Handle;
	return Result;
}

bool UCombatMotionComponent::ReleaseMotion(
	const FCombatMotionHandle Handle,
	const ECombatMotionFinishReason Reason)
{
	return FinishMotion(Handle, Reason, FGameplayTag());
}

bool UCombatMotionComponent::IsMotionActive(const FCombatMotionHandle Handle) const
{
	const FActiveMotion* Motion = ActiveMotions.Find(Handle.Key.Id);
	const ACombatUnitCharacter* Unit = GetOwnerUnit();
	return Motion && Unit && Motion->Handle == Handle && Handle.Key.Generation == MotionGeneration
		&& Handle.Key.LifeGeneration == Unit->GetLifeGeneration();
}

void UCombatMotionComponent::HandleOwnerDeath()
{
	TArray<FCombatMotionHandle> Handles;
	for (const TPair<uint64, FActiveMotion>& Pair : ActiveMotions) { Handles.Add(Pair.Value.Handle); }
	for (const FCombatMotionHandle Handle : Handles)
	{
		FinishMotion(Handle, ECombatMotionFinishReason::Death, CombatTags::Failure_Life_NotAlive);
	}
	++MotionGeneration;
	if (MotionGeneration == 0) { MotionGeneration = 1; }
}

void UCombatMotionComponent::HandleOwnerRespawn()
{
	ActiveMotions.Reset();
	HorizontalOwner = FCombatMotionHandle();
	VerticalOwner = FCombatMotionHandle();
	++MotionGeneration;
	if (MotionGeneration == 0) { MotionGeneration = 1; }
	SetComponentTickEnabled(false);
}

void UCombatMotionComponent::TickComponent(
	const float DeltaTime,
	const ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	ACombatUnitCharacter* Unit = GetOwnerUnit();
	UCharacterMovementComponent* Movement = Unit ? Unit->GetCharacterMovement() : nullptr;
	if (!Unit || !Movement || !Unit->HasAuthority() || !FMath::IsFinite(DeltaTime) || DeltaTime <= 0.0f)
	{
		return;
	}
	TArray<FCombatMotionHandle> Handles;
	for (const TPair<uint64, FActiveMotion>& Pair : ActiveMotions) { Handles.Add(Pair.Value.Handle); }
	for (const FCombatMotionHandle Handle : Handles)
	{
		FActiveMotion* Motion = ActiveMotions.Find(Handle.Key.Id);
		if (!Motion || !IsMotionActive(Handle))
		{
			continue;
		}
		FVector DeltaToTarget = Motion->Request.TargetLocation - Unit->GetActorLocation();
		if (Motion->Request.Channel == ECombatMotionChannel::Horizontal) { DeltaToTarget.Z = 0.0f; }
		else if (Motion->Request.Channel == ECombatMotionChannel::Vertical) { DeltaToTarget.X = 0.0f; DeltaToTarget.Y = 0.0f; }
		const float Distance = DeltaToTarget.Size();
		if (Distance <= Motion->Request.StopDistance + KINDA_SMALL_NUMBER)
		{
			FinishMotion(Handle, ECombatMotionFinishReason::Completed, FGameplayTag());
			continue;
		}
		const FVector Delta = DeltaToTarget.GetSafeNormal() * FMath::Min(Distance, Motion->Request.Speed * DeltaTime);
		FHitResult Hit;
		Movement->SafeMoveUpdatedComponent(
			Delta, Unit->GetActorQuat(), Motion->Request.bSweep, Hit, ETeleportType::None);
		Movement->Velocity = Delta / DeltaTime;
		if (Hit.bBlockingHit)
		{
			FinishMotion(Handle, ECombatMotionFinishReason::Blocked, CombatTags::Failure_Motion_Blocked);
		}
		else if (Delta.SizeSquared() + KINDA_SMALL_NUMBER >= FMath::Square(Distance))
		{
			FinishMotion(Handle, ECombatMotionFinishReason::Completed, FGameplayTag());
		}
	}
}

void UCombatMotionComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	bEnding = true;
	TArray<FCombatMotionHandle> Handles;
	for (const TPair<uint64, FActiveMotion>& Pair : ActiveMotions) { Handles.Add(Pair.Value.Handle); }
	for (const FCombatMotionHandle Handle : Handles)
	{
		FinishMotion(Handle, ECombatMotionFinishReason::EndPlay, CombatTags::Order_Failure_Cancelled);
	}
	ActiveMotions.Reset();
	++MotionGeneration;
	Super::EndPlay(EndPlayReason);
}

ACombatUnitCharacter* UCombatMotionComponent::GetOwnerUnit() const
{
	return Cast<ACombatUnitCharacter>(GetOwner());
}

bool UCombatMotionComponent::FinishMotion(
	const FCombatMotionHandle Handle,
	const ECombatMotionFinishReason Reason,
	const FGameplayTag FailureTag)
{
	FActiveMotion* Motion = ActiveMotions.Find(Handle.Key.Id);
	if (!Motion || Motion->Handle != Handle || Handle.Key.Generation != MotionGeneration)
	{
		return false;
	}
	const FActiveMotion Snapshot = *Motion;
	const bool bProject = Snapshot.Request.bProjectToNavigation;
	if (HorizontalOwner == Handle) { HorizontalOwner = FCombatMotionHandle(); }
	if (VerticalOwner == Handle) { VerticalOwner = FCombatMotionHandle(); }
	ActiveMotions.Remove(Handle.Key.Id);

	ACombatUnitCharacter* Unit = GetOwnerUnit();
	if (Unit)
	{
		// 最后一条 Motion 结束后恢复 Crowd；Order 恢复仍由 RestoreNavigationAndOrder 统一裁决。
		Unit->RefreshServerMovementState();
	}
	LastMotionResult.bSuccess = true;
	LastMotionResult.Handle = Handle;
	LastMotionResult.FinishReason = Reason;
	LastMotionResult.FailureTag = FailureTag;
	LastMotionResult.FinalLocation = Unit ? Unit->GetActorLocation() : FVector::ZeroVector;
	EmitMotionLog(Snapshot, CombatTags::Event_Combat_MotionFinished, Reason, FailureTag);
	MotionFinishedDelegate.Broadcast(LastMotionResult);
	if (ActiveMotions.IsEmpty())
	{
		SetComponentTickEnabled(false);
		if (!bAcquiringReplacement)
		{
			RestoreNavigationAndOrder(bProject);
		}
	}
	return true;
}

bool UCombatMotionComponent::UsesHorizontal(const ECombatMotionChannel Channel)
{
	return Channel == ECombatMotionChannel::Horizontal || Channel == ECombatMotionChannel::Both;
}

bool UCombatMotionComponent::UsesVertical(const ECombatMotionChannel Channel)
{
	return Channel == ECombatMotionChannel::Vertical || Channel == ECombatMotionChannel::Both;
}

void UCombatMotionComponent::RestoreNavigationAndOrder(const bool bProjectToNavigation)
{
	ACombatUnitCharacter* Unit = GetOwnerUnit();
	if (!Unit || bEnding)
	{
		return;
	}
	if (UCharacterMovementComponent* Movement = Unit->GetCharacterMovement())
	{
		Movement->StopMovementImmediately();
	}
	if (bProjectToNavigation)
	{
		if (UNavigationSystemV1* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld()))
		{
			FNavLocation Projected;
			if (Navigation->ProjectPointToNavigation(Unit->GetActorLocation(), Projected, FVector(100.0f, 100.0f, 300.0f)))
			{
				Unit->SetActorLocation(Projected.Location, false, nullptr, ETeleportType::TeleportPhysics);
			}
		}
	}
	if (UCombatOrderComponent* Orders = Unit->GetCombatOrderComponent())
	{
		Orders->HandleOwnerMotionFinished();
	}
}

void UCombatMotionComponent::EmitMotionLog(
	const FActiveMotion& Motion,
	const FGameplayTag EventType,
	const ECombatMotionFinishReason FinishReason,
	const FGameplayTag FailureTag) const
{
	UCombatEventSubsystem* Events = GetWorld() ? GetWorld()->GetSubsystem<UCombatEventSubsystem>() : nullptr;
	ACombatUnitCharacter* Unit = GetOwnerUnit();
	ACombatUnitCharacter* Source = Motion.Request.Source;
	if (!Events || !Unit || !Motion.Request.ParentEvent.IsValid())
	{
		return;
	}
	FCombatLogRecord Log;
	Log.Context = Motion.Request.ParentEvent;
	Log.EventType = EventType;
	Log.FailureTag = FailureTag;
	Log.Source = Motion.Request.SourceContext;
	Log.SourceActorId = Source ? Source->GetUniqueID() : Unit->GetUniqueID();
	Log.TargetActorId = Unit->GetUniqueID();
	Log.UnitLifeGeneration = Motion.Handle.Key.LifeGeneration;
	Log.RequestedAmount = FVector::Distance(Motion.StartLocation, Motion.Request.TargetLocation);
	Log.AppliedAmount = FVector::Distance(Motion.StartLocation, Unit->GetActorLocation());
	Log.Diagnostic = FString::Printf(TEXT("Motion=%s Channel=%d Priority=%d Finish=%d"),
		*Motion.Handle.ToString(), static_cast<int32>(Motion.Request.Channel),
		Motion.Request.Priority, static_cast<int32>(FinishReason));
	Events->Emit(Log);
}
