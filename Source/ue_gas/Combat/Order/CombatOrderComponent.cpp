#include "Combat/Order/CombatOrderComponent.h"

#include "AIController.h"
#include "Components/CapsuleComponent.h"
#include "EnvironmentQuery/EnvQueryInstanceBlueprintWrapper.h"
#include "EnvironmentQuery/EnvQueryManager.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "Navigation/PathFollowingComponent.h"
#include "NavigationData.h"
#include "NavigationPath.h"
#include "NavigationSystem.h"

#include "Combat/Ability/CombatAbilitySystemComponent.h"
#include "Combat/Attack/CombatAttackComponent.h"
#include "Combat/Attributes/CombatAttributeSet.h"
#include "Combat/Core/CombatNumericPolicy.h"
#include "Combat/Core/CombatTags.h"
#include "Combat/Data/CombatDefinitionData.h"
#include "Combat/Log/CombatEventSubsystem.h"
#include "Combat/Motion/CombatMotionComponent.h"
#include "Combat/Scheduling/CombatSchedulerSubsystem.h"
#include "Combat/Targeting/CombatTargetingSubsystem.h"
#include "Combat/Unit/CombatUnitCharacter.h"
#include "ue_gasPlayerController.h"

UCombatOrderComponent::UCombatOrderComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

FCombatOrderResult UCombatOrderComponent::IssueOrder(
	const FCombatOrderRequest& Request,
	const bool bQueue)
{
	FCombatOrderResult Result;
	ACombatUnitCharacter* Unit = GetOwnerUnit();
	if (!Unit || !Unit->HasAuthority())
	{
		Result.FailureTag = CombatTags::Failure_Authority;
		Result.Diagnostic = TEXT("Orders are server authoritative");
		return Result;
	}
	EnsureRuntimeBindings();
	const FCombatOperationResult Validation = ValidateOrderRequest(Request);
	if (!Validation.bSuccess)
	{
		Result.FailureTag = Validation.FailureTag;
		Result.Diagnostic = Validation.Diagnostic;
		return Result;
	}
	if (Request.Type == ECombatOrderType::Stop)
	{
		StopAllOrders(CombatTags::Order_Failure_Cancelled);
		Result.bSuccess = true;
		Result.Handle = AllocateOrderHandle();
		Result.State = ECombatOrderState::Completed;
		return Result;
	}
	if (bQueue && PendingOrders.Num() >= MaxQueuedOrders)
	{
		Result.FailureTag = CombatTags::Order_Failure_QueueFull;
		Result.Diagnostic = TEXT("Order FIFO reached MaxQueuedOrders");
		return Result;
	}
	if (!bQueue)
	{
		AdvanceGenerationAndCancel(CombatTags::Order_Failure_Cancelled, true);
	}

	FCombatQueuedOrder Queued;
	Queued.Request = Request;
	Queued.Handle = AllocateOrderHandle();
	Queued.UnitLifeGeneration = Unit->GetLifeGeneration();
	Result.bSuccess = true;
	Result.Handle = Queued.Handle;
	Result.State = ECombatOrderState::Validating;
	PendingOrders.Add(MoveTemp(Queued));
	PumpCurrentOrder();
	return Result;
}

void UCombatOrderComponent::StopAllOrders(const FGameplayTag Reason)
{
	AdvanceGenerationAndCancel(
		Reason.IsValid() ? Reason : CombatTags::Order_Failure_Cancelled.GetTag(),
		true);
}

void UCombatOrderComponent::PumpCurrentOrder()
{
	if (bPumping)
	{
		return;
	}
	TGuardValue<bool> PumpGuard(bPumping, true);
	ACombatUnitCharacter* Unit = GetOwnerUnit();
	while (Unit && Unit->HasAuthority())
	{
		if (!CurrentOrder.IsSet() && !BeginNextOrder())
		{
			TransitionTo(ECombatOrderState::Idle);
			return;
		}
		if (!CurrentOrder.IsSet())
		{
			return;
		}
		if (Unit->GetCombatMotionComponent() && Unit->GetCombatMotionComponent()->HasActiveMotion())
		{
			TransitionTo(ECombatOrderState::Paused, CombatTags::Order_Failure_UnitStateBlocked,
				TEXT("Forced motion temporarily pauses the current order"));
			return;
		}
		const FCombatQueuedOrder& Order = CurrentOrder.GetValue();
		if (Order.Handle.Key.Generation != OrderGeneration
			|| Order.UnitLifeGeneration != Unit->GetLifeGeneration()
			|| Unit->GetLifeState() != ECombatLifeState::Alive)
		{
			CompleteCurrentOrder(false, CombatTags::Failure_Life_NotAlive, TEXT("Order life generation is stale"));
			continue;
		}

		const ECombatOrderType Type = Order.Request.Type;
		const bool bMoveOrder = Type == ECombatOrderType::MoveToPoint || Type == ECombatOrderType::MoveToUnit;
		const bool bCastOrder = Type == ECombatOrderType::CastNoTarget
			|| Type == ECombatOrderType::CastPoint || Type == ECombatOrderType::CastTarget;
		if ((bMoveOrder && Unit->IsMovementBlocked())
			|| (Type == ECombatOrderType::AttackTarget && Unit->IsAttackBlocked())
			|| (bCastOrder && Unit->IsAbilityBlocked()))
		{
			TransitionTo(ECombatOrderState::Paused, CombatTags::Order_Failure_UnitStateBlocked,
				TEXT("Current unit state temporarily blocks the order"));
			return;
		}

		TransitionTo(ECombatOrderState::Validating);
		switch (Type)
		{
		case ECombatOrderType::MoveToPoint:
		case ECombatOrderType::MoveToUnit:
			if (Type == ECombatOrderType::MoveToUnit
				&& (!Order.Request.TargetUnit || Order.Request.TargetUnit->GetLifeState() != ECombatLifeState::Alive))
			{
				CompleteCurrentOrder(false, CombatTags::Order_Failure_TargetInvalid, TEXT("Move target is no longer alive"));
				continue;
			}
			if (IsCurrentDestinationReached())
			{
				CompleteCurrentOrder(true, FGameplayTag(), TEXT("Movement destination reached"));
				continue;
			}
			if (!BeginMovement(Type == ECombatOrderType::MoveToUnit))
			{
				CompleteCurrentOrder(false, CombatTags::Order_Failure_PathFailed, TEXT("Could not start navigation movement"));
				continue;
			}
			return;

		case ECombatOrderType::CastNoTarget:
		case ECombatOrderType::CastPoint:
		case ECombatOrderType::CastTarget:
		{
			UCombatAbilitySystemComponent* Asc = Unit->GetCombatAbilitySystemComponent();
			const UCombatAbilityData* Data = Asc ? Asc->GetCombatAbilityData(Order.Request.AbilitySpecHandle) : nullptr;
			UCombatTargetingSubsystem* Targeting = GetWorld()->GetSubsystem<UCombatTargetingSubsystem>();
			if (!Asc || !Data || !Targeting)
			{
				CompleteCurrentOrder(false, CombatTags::Order_Failure_AbilityRejected, TEXT("AbilitySpec is no longer granted"));
				continue;
			}
			FCombatAbilityTargetData TargetData;
			TargetData.TargetActor = Order.Request.TargetUnit;
			TargetData.TargetLocation = Order.Request.TargetLocation;
			TargetData.bHasTargetLocation = Order.Request.bHasTargetLocation;
			const FCombatTargetValidationResult Validation = Targeting->ValidateAbilityTarget(
				Unit, Data->BehaviorTags, Data->TargetingRules, TargetData);
			if (!Validation.bValid)
			{
				if (Validation.FailureTag == CombatTags::Failure_Target_OutOfRange
					&& Type != ECombatOrderType::CastNoTarget && BeginMovement(true))
				{
					return;
				}
				CompleteCurrentOrder(false, Validation.FailureTag.IsValid()
					? Validation.FailureTag : CombatTags::Order_Failure_TargetInvalid.GetTag(), Validation.Diagnostic);
				continue;
			}
			CancelMovementAsync();
			if (!FaceCurrentTarget())
			{
				CompleteCurrentOrder(false, CombatTags::Order_Failure_TargetInvalid, TEXT("Cannot face cast target"));
				continue;
			}
			DispatchCurrentAbility();
			return;
		}

		case ECombatOrderType::AttackTarget:
			if (!Order.Request.TargetUnit || Order.Request.TargetUnit->GetLifeState() != ECombatLifeState::Alive)
			{
				CompleteCurrentOrder(false, CombatTags::Order_Failure_TargetInvalid, TEXT("Attack target is invalid"));
				continue;
			}
			if (!IsCurrentDestinationReached())
			{
				if (BeginMovement(true)) { return; }
				CompleteCurrentOrder(false, CombatTags::Order_Failure_PathFailed, TEXT("Could not chase attack target"));
				continue;
			}
			CancelMovementAsync();
			if (!FaceCurrentTarget())
			{
				CompleteCurrentOrder(false, CombatTags::Order_Failure_TargetInvalid, TEXT("Cannot face attack target"));
				continue;
			}
			StartCurrentAttack();
			return;

		default:
			CompleteCurrentOrder(false, CombatTags::Order_Failure_InvalidRequest, TEXT("Unsupported order type"));
			continue;
		}
	}
}

void UCombatOrderComponent::RefreshControllerBinding()
{
	if (UPathFollowingComponent* Previous = BoundPathFollowing.Get())
	{
		Previous->OnRequestFinished.RemoveAll(this);
	}
	BoundPathFollowing.Reset();
	if (UPathFollowingComponent* PathFollowing = ResolvePathFollowingComponent())
	{
		PathFollowing->OnRequestFinished.AddUObject(this, &UCombatOrderComponent::HandleMoveFinished);
		BoundPathFollowing = PathFollowing;
	}
}

void UCombatOrderComponent::HandleOwnerStatusChanged()
{
	ACombatUnitCharacter* Unit = GetOwnerUnit();
	if (!Unit || !CurrentOrder.IsSet())
	{
		return;
	}
	if (Unit->GetLifeState() != ECombatLifeState::Alive)
	{
		HandleOwnerDeath();
		return;
	}
	const ECombatOrderType Type = CurrentOrder->Request.Type;
	if (Unit->IsMovementBlocked()
		&& (CurrentState == ECombatOrderState::Moving || CurrentState == ECombatOrderState::Chasing
			|| Type == ECombatOrderType::MoveToPoint || Type == ECombatOrderType::MoveToUnit))
	{
		CancelMovementAsync();
		TransitionTo(ECombatOrderState::Paused, CombatTags::Order_Failure_UnitStateBlocked, TEXT("Movement paused by state"));
		return;
	}
	PumpCurrentOrder();
}

void UCombatOrderComponent::HandleOwnerDeath()
{
	AdvanceGenerationAndCancel(CombatTags::Failure_Life_NotAlive, true);
}

void UCombatOrderComponent::HandleOwnerRespawn()
{
	AdvanceGenerationAndCancel(CombatTags::Order_Failure_Cancelled, false);
}

void UCombatOrderComponent::HandleOwnerMotionStarted()
{
	if (!CurrentOrder.IsSet())
	{
		return;
	}
	CancelMovementAsync();
	TransitionTo(ECombatOrderState::Paused, CombatTags::Order_Failure_UnitStateBlocked,
		TEXT("Current order paused by forced motion"));
}

void UCombatOrderComponent::HandleOwnerMotionFinished()
{
	if (CurrentOrder.IsSet())
	{
		TransitionTo(ECombatOrderState::Validating);
		PumpCurrentOrder();
	}
}

void UCombatOrderComponent::HandleGameplayBlockerChanged(const FBox& BlockerBounds)
{
	ACombatUnitCharacter* Unit = GetOwnerUnit();
	if (!Unit || !CurrentOrder.IsSet()
		|| (CurrentState != ECombatOrderState::Moving && CurrentState != ECombatOrderState::Chasing
			&& CurrentState != ECombatOrderState::Querying))
	{
		return;
	}
	const float Radius = Unit->GetCapsuleComponent()->GetScaledCapsuleRadius();
	const FBox ExpandedBounds = BlockerBounds.ExpandBy(FVector(Radius, Radius, 0.0));
	const FVector Start = Unit->GetActorLocation();
	const FVector End = CurrentMoveGoal;
	if (!ExpandedBounds.IsInsideOrOn(Start)
		&& !FMath::LineBoxIntersection(ExpandedBounds, Start, End, End - Start))
	{
		return;
	}
	CancelMovementAsync();
	TransitionTo(ECombatOrderState::Validating, FGameplayTag(), TEXT("Gameplay blocker changed; repath current order"));
	PumpCurrentOrder();
}

FCombatOrderHandle UCombatOrderComponent::GetCurrentOrderHandle() const
{
	return CurrentOrder.IsSet() ? CurrentOrder->Handle : FCombatOrderHandle();
}

bool UCombatOrderComponent::CompleteMovementForTesting(
	const FCombatOrderHandle Handle,
	const bool bSuccess,
	const bool bPartial)
{
	return CompleteMovementAttemptForTesting(
		Handle, NavigationAttemptGeneration, bSuccess, bPartial);
}

bool UCombatOrderComponent::CompleteMovementAttemptForTesting(
	const FCombatOrderHandle Handle,
	const uint32 AttemptGeneration,
	const bool bSuccess,
	const bool bPartial)
{
	if (!bNavigationDeferredForTesting || !CurrentOrder.IsSet() || CurrentOrder->Handle != Handle
		|| AttemptGeneration != NavigationAttemptGeneration
		|| (CurrentState != ECombatOrderState::Moving && CurrentState != ECombatOrderState::Chasing
			&& CurrentState != ECombatOrderState::Querying))
	{
		return false;
	}
	if (bSuccess)
	{
		if (ACombatUnitCharacter* Unit = GetOwnerUnit())
		{
			Unit->SetActorLocation(CurrentMoveGoal, false, nullptr, ETeleportType::TeleportPhysics);
		}
	}
	ResolveMoveCompletion(bSuccess, bPartial, CombatTags::Order_Failure_PathFailed,
		bSuccess ? TEXT("Injected navigation completion") : TEXT("Injected navigation failure"));
	return true;
}

void UCombatOrderComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	AdvanceGenerationAndCancel(CombatTags::Order_Failure_Cancelled, false);
	if (UPathFollowingComponent* PathFollowing = BoundPathFollowing.Get())
	{
		PathFollowing->OnRequestFinished.RemoveAll(this);
	}
	if (ACombatUnitCharacter* Unit = GetOwnerUnit())
	{
		if (UCombatAbilitySystemComponent* Asc = Unit->GetCombatAbilitySystemComponent())
		{
			Asc->OnCombatAbilityOrderReleased().RemoveAll(this);
		}
		if (UCombatAttackComponent* Attacks = Unit->GetCombatAttackComponent())
		{
			Attacks->OnAttackLaunched().RemoveAll(this);
			Attacks->OnAttackReady().RemoveAll(this);
		}
	}
	Super::EndPlay(EndPlayReason);
}

void UCombatOrderComponent::BeginPlay()
{
	Super::BeginPlay();
	EnsureRuntimeBindings();
	RefreshControllerBinding();
	PumpCurrentOrder();
}

ACombatUnitCharacter* UCombatOrderComponent::GetOwnerUnit() const
{
	return Cast<ACombatUnitCharacter>(GetOwner());
}

UPathFollowingComponent* UCombatOrderComponent::ResolvePathFollowingComponent() const
{
	const ACombatUnitCharacter* Unit = GetOwnerUnit();
	AController* Controller = Unit ? Unit->GetController() : nullptr;
	if (AAIController* AiController = Cast<AAIController>(Controller))
	{
		return AiController->GetPathFollowingComponent();
	}
	return Controller ? Controller->FindComponentByClass<UPathFollowingComponent>() : nullptr;
}

void UCombatOrderComponent::EnsureRuntimeBindings()
{
	ACombatUnitCharacter* Unit = GetOwnerUnit();
	if (!Unit)
	{
		return;
	}
	if (UCombatAbilitySystemComponent* Asc = Unit->GetCombatAbilitySystemComponent())
	{
		Asc->OnCombatAbilityOrderReleased().RemoveAll(this);
		Asc->OnCombatAbilityOrderReleased().AddUObject(this, &UCombatOrderComponent::HandleAbilityOrderReleased);
	}
	if (UCombatAttackComponent* Attacks = Unit->GetCombatAttackComponent())
	{
		Attacks->OnAttackLaunched().RemoveAll(this);
		Attacks->OnAttackReady().RemoveAll(this);
		Attacks->OnAttackLaunched().AddUObject(this, &UCombatOrderComponent::HandleAttackLaunched);
		Attacks->OnAttackReady().AddUObject(this, &UCombatOrderComponent::HandleAttackReady);
	}
}

FCombatOperationResult UCombatOrderComponent::ValidateOrderRequest(const FCombatOrderRequest& Request) const
{
	ACombatUnitCharacter* Unit = GetOwnerUnit();
	if (!Unit || Unit->GetLifeState() != ECombatLifeState::Alive)
	{
		return FCombatOperationResult::Failure(CombatTags::Failure_Life_NotAlive, TEXT("Unit is not alive"));
	}
	const bool bFiniteLocation = !Request.TargetLocation.ContainsNaN()
		&& FMath::IsFinite(Request.TargetLocation.X) && FMath::IsFinite(Request.TargetLocation.Y)
		&& FMath::IsFinite(Request.TargetLocation.Z);
	switch (Request.Type)
	{
	case ECombatOrderType::MoveToPoint:
		if (!Request.bHasTargetLocation || !bFiniteLocation || Request.TargetUnit || Request.AbilitySpecHandle.IsValid())
		{
			return FCombatOperationResult::Failure(CombatTags::Order_Failure_InvalidRequest, TEXT("MoveToPoint payload is invalid"));
		}
		break;
	case ECombatOrderType::MoveToUnit:
	case ECombatOrderType::AttackTarget:
		if (!Request.TargetUnit || Request.TargetUnit->GetWorld() != GetWorld()
			|| Request.bHasTargetLocation || Request.AbilitySpecHandle.IsValid())
		{
			return FCombatOperationResult::Failure(CombatTags::Order_Failure_InvalidRequest, TEXT("Unit movement/attack payload is invalid"));
		}
		break;
	case ECombatOrderType::CastNoTarget:
		if (!Request.AbilitySpecHandle.IsValid() || Request.TargetUnit || Request.bHasTargetLocation)
		{
			return FCombatOperationResult::Failure(CombatTags::Order_Failure_InvalidRequest, TEXT("CastNoTarget payload is invalid"));
		}
		break;
	case ECombatOrderType::CastPoint:
		if (!Request.AbilitySpecHandle.IsValid() || !Request.bHasTargetLocation || !bFiniteLocation || Request.TargetUnit)
		{
			return FCombatOperationResult::Failure(CombatTags::Order_Failure_InvalidRequest, TEXT("CastPoint payload is invalid"));
		}
		break;
	case ECombatOrderType::CastTarget:
		if (!Request.AbilitySpecHandle.IsValid() || !Request.TargetUnit
			|| Request.TargetUnit->GetWorld() != GetWorld() || Request.bHasTargetLocation)
		{
			return FCombatOperationResult::Failure(CombatTags::Order_Failure_InvalidRequest, TEXT("CastTarget payload is invalid"));
		}
		break;
	case ECombatOrderType::Stop:
		if (Request.TargetUnit || Request.bHasTargetLocation || Request.AbilitySpecHandle.IsValid())
		{
			return FCombatOperationResult::Failure(CombatTags::Order_Failure_InvalidRequest, TEXT("Stop payload must be empty"));
		}
		break;
	default:
		return FCombatOperationResult::Failure(CombatTags::Order_Failure_InvalidRequest, TEXT("Unknown order type"));
	}
	return FCombatOperationResult::Success();
}

FCombatOrderHandle UCombatOrderComponent::AllocateOrderHandle() const
{
	FCombatOrderHandle Handle;
	const ACombatUnitCharacter* Unit = GetOwnerUnit();
	Handle.Key.Id = NextOrderId++;
	Handle.Key.Generation = OrderGeneration;
	Handle.Key.LifeGeneration = Unit ? Unit->GetLifeGeneration() : 0;
	return Handle;
}

bool UCombatOrderComponent::BeginNextOrder()
{
	if (PendingOrders.IsEmpty())
	{
		return false;
	}
	CurrentOrder = MoveTemp(PendingOrders[0]);
	PendingOrders.RemoveAt(0, 1, EAllowShrinking::No);
	MoveRetryCount = 0;
	ChaseStartedAt = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	TransitionTo(ECombatOrderState::Validating);
	return true;
}

void UCombatOrderComponent::CompleteCurrentOrder(
	const bool bSuccess,
	const FGameplayTag FailureTag,
	const FString& Diagnostic)
{
	if (!CurrentOrder.IsSet())
	{
		return;
	}
	const FCombatOrderHandle Handle = CurrentOrder->Handle;
	CancelCurrentAsync(FailureTag);
	TransitionTo(bSuccess ? ECombatOrderState::Completed : ECombatOrderState::Failed, FailureTag, Diagnostic);
	FCombatOrderResult Result;
	Result.bSuccess = bSuccess;
	Result.Handle = Handle;
	Result.State = CurrentState;
	Result.FailureTag = FailureTag;
	Result.Diagnostic = Diagnostic;
	CurrentOrder.Reset();
	CurrentState = ECombatOrderState::Idle;
	OrderFinishedDelegate.Broadcast(Result);
	if (!bPumping)
	{
		PumpCurrentOrder();
	}
}

void UCombatOrderComponent::AdvanceGenerationAndCancel(
	const FGameplayTag Reason,
	const bool bBroadcastCurrent)
{
	TOptional<FCombatQueuedOrder> Previous = CurrentOrder;
	++OrderGeneration;
	if (OrderGeneration == 0) { OrderGeneration = 1; }
	CancelCurrentAsync(Reason);
	PendingOrders.Reset();
	CurrentOrder.Reset();
	CurrentState = ECombatOrderState::Idle;
	if (bBroadcastCurrent && Previous.IsSet())
	{
		FCombatOrderResult Result;
		Result.Handle = Previous->Handle;
		Result.State = ECombatOrderState::Cancelled;
		Result.FailureTag = Reason;
		Result.Diagnostic = TEXT("Order cancelled by generation advance");
		OrderFinishedDelegate.Broadcast(Result);
	}
}

void UCombatOrderComponent::CancelCurrentAsync(const FGameplayTag Reason)
{
	CancelMovementAsync();
	ACombatUnitCharacter* Unit = GetOwnerUnit();
	const FGameplayAbilitySpecHandle AbilityToCancel = ActiveAbilitySpecHandle;
	ActiveAbilitySpecHandle = FGameplayAbilitySpecHandle();
	if (Unit && AbilityToCancel.IsValid())
	{
		Unit->GetCombatAbilitySystemComponent()->CancelAbilityHandle(AbilityToCancel);
	}
	if (Unit && CurrentOrder.IsSet())
	{
		Unit->GetCombatAttackComponent()->CancelWindupForOrder(CurrentOrder->Handle, Reason);
	}
}

void UCombatOrderComponent::CancelMovementAsync()
{
	if (ActiveQuery)
	{
		ActiveQuery->GetOnQueryFinishedEvent().RemoveAll(this);
	}
	ActiveQuery = nullptr;
	ActiveQueryOrderHandle = FCombatOrderHandle();
	const FAIRequestID MoveToAbort = ActiveMoveRequestId;
	ActiveMoveRequestId = FAIRequestID::InvalidRequest;
	ActiveMoveOrderHandle = FCombatOrderHandle();
	ActiveMovePath.Reset();
	if (MoveToAbort.IsValid())
	{
		if (ACombatUnitCharacter* Unit = GetOwnerUnit())
		{
			if (AController* Controller = Unit->GetController())
			{
				Controller->StopMovement();
				if (Aue_gasPlayerController* PlayerController = Cast<Aue_gasPlayerController>(Controller);
					PlayerController && !PlayerController->IsLocalController())
				{
					PlayerController->ClientStopCombatOrderNavigation();
				}
			}
		}
	}
	if (UCombatSchedulerSubsystem* Scheduler = GetWorld() ? GetWorld()->GetSubsystem<UCombatSchedulerSubsystem>() : nullptr)
	{
		Scheduler->Cancel(ChaseSchedule);
		Scheduler->Cancel(RetrySchedule);
	}
	ChaseSchedule = FCombatScheduleHandle();
	RetrySchedule = FCombatScheduleHandle();
}

bool UCombatOrderComponent::IsCurrentDestinationReached() const
{
	const ACombatUnitCharacter* Unit = GetOwnerUnit();
	if (!Unit || !CurrentOrder.IsSet())
	{
		return false;
	}
	const FCombatOrderRequest& Request = CurrentOrder->Request;
	const float SourceRadius = Unit->GetCapsuleComponent()->GetScaledCapsuleRadius();
	float EdgeDistance = 0.0f;
	if (Request.TargetUnit)
	{
		const float TargetRadius = Request.TargetUnit->GetCapsuleComponent()->GetScaledCapsuleRadius();
		EdgeDistance = FMath::Max(0.0f, FVector::Dist2D(Unit->GetActorLocation(), Request.TargetUnit->GetActorLocation())
			- SourceRadius - TargetRadius);
	}
	else
	{
		EdgeDistance = FMath::Max(0.0f, FVector::Dist2D(Unit->GetActorLocation(), Request.TargetLocation) - SourceRadius);
	}
	return EdgeDistance <= GetCurrentDesiredRange() + FCombatNumericPolicyV1::RangeToleranceCm;
}

float UCombatOrderComponent::GetCurrentDesiredRange() const
{
	const ACombatUnitCharacter* Unit = GetOwnerUnit();
	if (!Unit || !CurrentOrder.IsSet())
	{
		return 0.0f;
	}
	const FCombatOrderRequest& Request = CurrentOrder->Request;
	if (Request.Type == ECombatOrderType::AttackTarget)
	{
		return Unit->GetCombatAbilitySystemComponent()->GetNumericAttribute(UCombatAttributeSet::GetAttackRangeAttribute());
	}
	if (Request.Type == ECombatOrderType::CastPoint || Request.Type == ECombatOrderType::CastTarget)
	{
		const UCombatAbilitySystemComponent* Asc = Unit->GetCombatAbilitySystemComponent();
		const UCombatAbilityData* Data = Asc ? Asc->GetCombatAbilityData(Request.AbilitySpecHandle) : nullptr;
		return Data ? Data->TargetingRules.CastRange
			+ Asc->GetNumericAttribute(UCombatAttributeSet::GetCastRangeBonusAttribute()) : 0.0f;
	}
	return MovementAcceptanceRadius;
}

bool UCombatOrderComponent::FaceCurrentTarget()
{
	ACombatUnitCharacter* Unit = GetOwnerUnit();
	if (!Unit || !CurrentOrder.IsSet())
	{
		return false;
	}
	TransitionTo(ECombatOrderState::Facing);
	const FVector TargetLocation = CurrentOrder->Request.TargetUnit
		? CurrentOrder->Request.TargetUnit->GetActorLocation() : CurrentOrder->Request.TargetLocation;
	const FVector Direction = (TargetLocation - Unit->GetActorLocation()).GetSafeNormal2D();
	if (!Direction.IsNearlyZero())
	{
		Unit->SetActorRotation(Direction.Rotation());
	}
	return true;
}

bool UCombatOrderComponent::BeginMovement(const bool bChasing)
{
	ACombatUnitCharacter* Unit = GetOwnerUnit();
	if (!Unit || !CurrentOrder.IsSet() || Unit->IsMovementBlocked())
	{
		return false;
	}
	CancelMovementAsync();
	++NavigationAttemptGeneration;
	if (NavigationAttemptGeneration == 0) { NavigationAttemptGeneration = 1; }
	CurrentMoveGoal = CurrentOrder->Request.TargetUnit
		? CurrentOrder->Request.TargetUnit->GetActorLocation() : CurrentOrder->Request.TargetLocation;
	LastChaseTargetLocation = CurrentMoveGoal;
	if (ChaseStartedAt <= 0.0)
	{
		ChaseStartedAt = GetWorld()->GetTimeSeconds();
	}
	if (bNavigationDeferredForTesting)
	{
		TransitionTo(bChasing ? ECombatOrderState::Chasing : ECombatOrderState::Moving);
		if (bChasing) { EnsureChaseSchedule(); }
		return true;
	}
	if (!bChasing && MoveDestinationQuery)
	{
		ActiveQueryOrderHandle = CurrentOrder->Handle;
		ActiveQuery = UEnvQueryManager::RunEQSQuery(
			this, MoveDestinationQuery, Unit, EEnvQueryRunMode::SingleResult,
			UEnvQueryInstanceBlueprintWrapper::StaticClass());
		if (!ActiveQuery)
		{
			ActiveQueryOrderHandle = FCombatOrderHandle();
			return false;
		}
		ActiveQuery->GetOnQueryFinishedEvent().AddDynamic(this, &UCombatOrderComponent::HandleEqsFinished);
		TransitionTo(ECombatOrderState::Querying);
		return true;
	}
	return StartNavigationMove(bChasing);
}

bool UCombatOrderComponent::StartNavigationMove(const bool bChasing)
{
	ACombatUnitCharacter* Unit = GetOwnerUnit();
	AController* Controller = Unit ? Unit->GetController() : nullptr;
	if (!Unit || !Controller || !CurrentOrder.IsSet())
	{
		return false;
	}
	RefreshControllerBinding();
	UPathFollowingComponent* PathFollowing = BoundPathFollowing.Get();
	if (!PathFollowing || !PathFollowing->IsPathFollowingAllowed())
	{
		return false;
	}

	FAIMoveRequest MoveRequest;
	// 动态 Actor 也使用位置快照；EnsureChaseSchedule 负责位移阈值唤醒，避免 GoalActor 自带的到达半径
	// 与 CombatUnit 自定义碰撞响应重复计算并让 PathFollowing 停在无有效速度的状态。
	MoveRequest.SetGoalLocation(CurrentMoveGoal);
	MoveRequest.SetAcceptanceRadius(GetCurrentDesiredRange());
	MoveRequest.SetReachTestIncludesAgentRadius(true);
	MoveRequest.SetReachTestIncludesGoalRadius(true);
	MoveRequest.SetAllowPartialPath(true);
	MoveRequest.SetUsePathfinding(true);
	MoveRequest.SetProjectGoalLocation(true);
	MoveRequest.SetRequireNavigableEndLocation(true);
	MoveRequest.SetCanStrafe(false);

	FNavPathSharedPtr FollowedPath;
	FAIRequestID MoveRequestId = FAIRequestID::InvalidRequest;
	if (AAIController* AiController = Cast<AAIController>(Controller))
	{
		MoveRequest.SetNavigationFilter(AiController->GetDefaultNavigationFilterClass());
		const FPathFollowingRequestResult RequestResult = AiController->MoveTo(MoveRequest, &FollowedPath);
		if (RequestResult.Code == EPathFollowingRequestResult::Failed)
		{
			return false;
		}
		if (RequestResult.Code == EPathFollowingRequestResult::AlreadyAtGoal)
		{
			TransitionTo(ECombatOrderState::Validating);
			PumpCurrentOrder();
			return true;
		}
		MoveRequestId = RequestResult.MoveId;
	}
	else
	{
		// PlayerController 没有 AAIController::MoveTo，因此复用同一 FAIMoveRequest 同步求路后交给其 PathFollowing。
		UNavigationSystemV1* NavigationSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
		if (!NavigationSystem)
		{
			return false;
		}
		FNavLocation ProjectedGoal;
		if (MoveRequest.IsProjectingGoal()
			&& !NavigationSystem->ProjectPointToNavigation(
				MoveRequest.GetGoalLocation(), ProjectedGoal, INVALID_NAVEXTENT,
				&Controller->GetNavAgentPropertiesRef()))
		{
			return false;
		}
		if (MoveRequest.IsProjectingGoal())
		{
			MoveRequest.UpdateGoalLocation(ProjectedGoal.Location);
		}
		if (PathFollowing->HasReached(MoveRequest))
		{
			TransitionTo(ECombatOrderState::Validating);
			PumpCurrentOrder();
			return true;
		}

		const FVector AgentLocation = Controller->GetNavAgentLocation();
		const ANavigationData* NavigationData = NavigationSystem->GetNavDataForProps(
			Controller->GetNavAgentPropertiesRef(), AgentLocation);
		if (!NavigationData)
		{
			return false;
		}
		FPathFindingQuery Query(
			Controller, *NavigationData, AgentLocation, MoveRequest.GetGoalLocation());
		Query.SetAllowPartialPaths(MoveRequest.IsUsingPartialPaths());
		Query.SetRequireNavigableEndLocation(MoveRequest.IsNavigableEndLocationRequired());
		PathFollowing->OnPathfindingQuery(Query);
		FPathFindingResult PathResult = NavigationSystem->FindPathSync(Query);
		if (!PathResult.IsSuccessful() || !PathResult.Path.IsValid())
		{
			return false;
		}
		FollowedPath = PathResult.Path;
		MoveRequestId = PathFollowing->RequestMove(MoveRequest, FollowedPath);
	}
	if (!MoveRequestId.IsValid())
	{
		return false;
	}

	ActiveMoveRequestId = MoveRequestId;
	ActiveMoveOrderHandle = CurrentOrder->Handle;
	ActiveMovePath = FollowedPath;
	if (Aue_gasPlayerController* PlayerController = Cast<Aue_gasPlayerController>(Controller);
		PlayerController && !PlayerController->IsLocalController() && FollowedPath.IsValid())
	{
		TArray<FVector_NetQuantize10> ReplicatedPathPoints;
		ReplicatedPathPoints.Reserve(FollowedPath->GetPathPoints().Num());
		for (const FNavPathPoint& PathPoint : FollowedPath->GetPathPoints())
		{
			ReplicatedPathPoints.Emplace(PathPoint.Location);
		}
		PlayerController->ClientFollowCombatOrderPath(
			ReplicatedPathPoints,
			MoveRequest.GetGoalLocation(),
			MoveRequest.GetAcceptanceRadius());
	}
	FString PathDiagnostic;
	if (FollowedPath.IsValid())
	{
		PathDiagnostic = FString::Printf(TEXT("PathPoints=%d Start=%s End=%s Partial=%d"),
			FollowedPath->GetPathPoints().Num(), *FollowedPath->GetStartLocation().ToCompactString(),
			*FollowedPath->GetEndLocation().ToCompactString(), FollowedPath->IsPartial());
	}
	TransitionTo(bChasing ? ECombatOrderState::Chasing : ECombatOrderState::Moving,
		FGameplayTag(), PathDiagnostic);
	if (bChasing) { EnsureChaseSchedule(); }
	return ActiveMoveRequestId.IsValid();
}

void UCombatOrderComponent::EnsureChaseSchedule()
{
	if (ChaseSchedule.IsValid() || !CurrentOrder.IsSet())
	{
		return;
	}
	if (UCombatSchedulerSubsystem* Scheduler = GetWorld()->GetSubsystem<UCombatSchedulerSubsystem>())
	{
		const FCombatOrderHandle Handle = CurrentOrder->Handle;
		ChaseSchedule = Scheduler->ScheduleRepeating(
			this, ChaseCheckInterval, ChaseCheckInterval, 0, ECombatCatchUpPolicy::Coalesce,
			FCombatScheduledDelegate::CreateWeakLambda(this,
				[this, Handle](const FCombatScheduledTickContext& Context) { HandleChaseCheck(Handle, Context); }));
	}
}

void UCombatOrderComponent::HandleChaseCheck(
	const FCombatOrderHandle Handle,
	const FCombatScheduledTickContext& TickContext)
{
	(void)TickContext;
	if (!CurrentOrder.IsSet() || CurrentOrder->Handle != Handle
		|| Handle.Key.Generation != OrderGeneration || !CurrentOrder->Request.TargetUnit)
	{
		return;
	}
	if (GetWorld()->GetTimeSeconds() - ChaseStartedAt > MaxChaseDuration)
	{
		CompleteCurrentOrder(false, CombatTags::Order_Failure_RetryExhausted, TEXT("Chase deadline exceeded"));
		return;
	}
	ACombatUnitCharacter* Target = CurrentOrder->Request.TargetUnit;
	if (Target->GetLifeState() != ECombatLifeState::Alive)
	{
		CompleteCurrentOrder(false, CombatTags::Order_Failure_TargetInvalid, TEXT("Chase target became invalid"));
		return;
	}
	if (IsCurrentDestinationReached())
	{
		CancelMovementAsync();
		TransitionTo(ECombatOrderState::Validating);
		PumpCurrentOrder();
		return;
	}
	const FVector NewLocation = Target->GetActorLocation();
	if (FVector::DistSquared2D(NewLocation, LastChaseTargetLocation) >= FMath::Square(ChaseWakeDistance))
	{
		LastChaseTargetLocation = NewLocation;
		CurrentMoveGoal = NewLocation;
		const FAIRequestID PreviousRequest = ActiveMoveRequestId;
		ActiveMoveRequestId = FAIRequestID::InvalidRequest;
		ActiveMoveOrderHandle = FCombatOrderHandle();
		ActiveMovePath.Reset();
		if (PreviousRequest.IsValid())
		{
			if (AController* Controller = GetOwnerUnit()->GetController())
			{
				Controller->StopMovement();
			}
		}
		StartNavigationMove(true);
	}
}

void UCombatOrderComponent::ScheduleMoveRetry(
	const FGameplayTag FailureTag,
	const FString& Diagnostic)
{
	if (!CurrentOrder.IsSet())
	{
		return;
	}
	CancelMovementAsync();
	++MoveRetryCount;
	if (MoveRetryCount > MaxMoveRetries)
	{
		CompleteCurrentOrder(false, CombatTags::Order_Failure_RetryExhausted,
			FString::Printf(TEXT("%s; retry limit reached"), *Diagnostic));
		return;
	}
	TransitionTo(ECombatOrderState::Paused, FailureTag, Diagnostic);
	if (UCombatSchedulerSubsystem* Scheduler = GetWorld()->GetSubsystem<UCombatSchedulerSubsystem>())
	{
		const FCombatOrderHandle Handle = CurrentOrder->Handle;
		RetrySchedule = Scheduler->ScheduleOnce(
			this, 0.20, 0,
			FCombatScheduledDelegate::CreateWeakLambda(this,
				[this, Handle](const FCombatScheduledTickContext& Context) { HandleMoveRetry(Handle, Context); }));
	}
	if (!RetrySchedule.IsValid())
	{
		CompleteCurrentOrder(false, CombatTags::Order_Failure_PathFailed, TEXT("Could not schedule move retry"));
	}
}

void UCombatOrderComponent::HandleMoveRetry(
	const FCombatOrderHandle Handle,
	const FCombatScheduledTickContext& TickContext)
{
	(void)TickContext;
	RetrySchedule = FCombatScheduleHandle();
	if (!CurrentOrder.IsSet() || CurrentOrder->Handle != Handle
		|| Handle.Key.Generation != OrderGeneration)
	{
		return;
	}
	TransitionTo(ECombatOrderState::Validating);
	PumpCurrentOrder();
}

void UCombatOrderComponent::DispatchCurrentAbility()
{
	ACombatUnitCharacter* Unit = GetOwnerUnit();
	if (!Unit || !CurrentOrder.IsSet())
	{
		return;
	}
	TransitionTo(ECombatOrderState::DispatchingAbility);
	const FCombatOrderRequest& Request = CurrentOrder->Request;
	FCombatAbilityTargetData TargetData;
	TargetData.TargetActor = Request.TargetUnit;
	TargetData.TargetLocation = Request.TargetLocation;
	TargetData.bHasTargetLocation = Request.bHasTargetLocation;
	ActiveAbilitySpecHandle = Request.AbilitySpecHandle;
	FGameplayTag FailureTag;
	if (!Unit->GetCombatAbilitySystemComponent()->TryActivateCombatAbility(
		ActiveAbilitySpecHandle, TargetData, FailureTag))
	{
		ActiveAbilitySpecHandle = FGameplayAbilitySpecHandle();
		CompleteCurrentOrder(false,
			FailureTag.IsValid() ? FailureTag : CombatTags::Order_Failure_AbilityRejected.GetTag(),
			TEXT("Ability activation rejected"));
		return;
	}
	// 零前摇 Ability 可能同步释放；回调已经完成当前项时不得覆盖新状态。
	if (CurrentOrder.IsSet() && ActiveAbilitySpecHandle.IsValid())
	{
		MoveRetryCount = 0;
		TransitionTo(ECombatOrderState::WaitingOrderRelease);
	}
}

void UCombatOrderComponent::StartCurrentAttack()
{
	ACombatUnitCharacter* Unit = GetOwnerUnit();
	if (!Unit || !CurrentOrder.IsSet())
	{
		return;
	}
	TransitionTo(ECombatOrderState::StartingAttack);
	const FCombatAttackResult Result = Unit->GetCombatAttackComponent()->StartMeleeAttack(
		CurrentOrder->Request.TargetUnit, CurrentOrder->Handle);
	if (Result.bSuccess)
	{
		// 成功进入新一轮前摇后清零连续移动失败计数，后续目标位移拥有独立重试预算。
		MoveRetryCount = 0;
		return;
	}
	if (Result.FailureTag == CombatTags::Failure_Attack_NotReady)
	{
		TransitionTo(ECombatOrderState::WaitingAttackReady);
		return;
	}
	if (Result.FailureTag == CombatTags::Failure_Target_OutOfRange && BeginMovement(true))
	{
		return;
	}
	if (Result.FailureTag == CombatTags::Failure_Attack_Blocked)
	{
		if (Unit->IsAttackBlocked())
		{
			TransitionTo(ECombatOrderState::Paused, Result.FailureTag, TEXT("Unit state temporarily blocks attack start"));
		}
		else
		{
			// 导航 Move 完成与 CharacterMovement 刹停可能相差一帧；有界延迟后重新验证距离、朝向和速度。
			ScheduleMoveRetry(Result.FailureTag, TEXT("Waiting for movement to settle before attack start"));
		}
		return;
	}
	CompleteCurrentOrder(false,
		Result.FailureTag.IsValid() ? Result.FailureTag : CombatTags::Order_Failure_TargetInvalid.GetTag(),
		TEXT("Attack start rejected"));
}

void UCombatOrderComponent::HandleAbilityOrderReleased(
	const FGameplayAbilitySpecHandle Handle,
	const bool bSuccess,
	const FGameplayTag FailureTag,
	const ECombatChannelInterruptOrderPolicy InterruptPolicy)
{
	if (!CurrentOrder.IsSet() || !ActiveAbilitySpecHandle.IsValid()
		|| Handle != ActiveAbilitySpecHandle || CurrentOrder->Handle.Key.Generation != OrderGeneration)
	{
		return;
	}
	ActiveAbilitySpecHandle = FGameplayAbilitySpecHandle();
	if (!bSuccess && InterruptPolicy == ECombatChannelInterruptOrderPolicy::ClearQueuedOrders)
	{
		PendingOrders.Reset();
	}
	CompleteCurrentOrder(bSuccess,
		bSuccess ? FGameplayTag() : (FailureTag.IsValid() ? FailureTag : CombatTags::Order_Failure_AbilityRejected.GetTag()),
		bSuccess ? TEXT("Ability released order") : TEXT("Ability interrupted or rejected"));
}

void UCombatOrderComponent::HandleAttackLaunched(
	const FCombatAttackHandle AttackHandle,
	const FCombatOrderHandle OrderHandle)
{
	(void)AttackHandle;
	if (!CurrentOrder.IsSet() || CurrentOrder->Handle != OrderHandle
		|| CurrentOrder->Request.Type != ECombatOrderType::AttackTarget
		|| OrderHandle.Key.Generation != OrderGeneration)
	{
		return;
	}
	TransitionTo(ECombatOrderState::WaitingAttackReady);
}

void UCombatOrderComponent::HandleAttackReady(const FCombatOrderHandle OrderHandle)
{
	if (!CurrentOrder.IsSet() || CurrentOrder->Handle != OrderHandle
		|| CurrentOrder->Request.Type != ECombatOrderType::AttackTarget
		|| OrderHandle.Key.Generation != OrderGeneration)
	{
		return;
	}
	TransitionTo(ECombatOrderState::Validating);
	PumpCurrentOrder();
}

void UCombatOrderComponent::HandleEqsFinished(
	UEnvQueryInstanceBlueprintWrapper* QueryInstance,
	const EEnvQueryStatus::Type QueryStatus)
{
	if (!CurrentOrder.IsSet() || !QueryInstance || QueryInstance != ActiveQuery
		|| ActiveQueryOrderHandle != CurrentOrder->Handle
		|| ActiveQueryOrderHandle.Key.Generation != OrderGeneration)
	{
		return;
	}
	ActiveQuery->GetOnQueryFinishedEvent().RemoveAll(this);
	ActiveQuery = nullptr;
	ActiveQueryOrderHandle = FCombatOrderHandle();
	if (QueryStatus != EEnvQueryStatus::Success)
	{
		ScheduleMoveRetry(CombatTags::Order_Failure_PathFailed, TEXT("EQS did not finish successfully"));
		return;
	}
	TArray<FVector> Locations;
	if (!QueryInstance->GetQueryResultsAsLocations(Locations) || Locations.IsEmpty()
		|| Locations[0].ContainsNaN())
	{
		ScheduleMoveRetry(CombatTags::Order_Failure_PathFailed, TEXT("EQS returned no finite location"));
		return;
	}
	CurrentMoveGoal = Locations[0];
	if (!StartNavigationMove(false))
	{
		ScheduleMoveRetry(CombatTags::Order_Failure_PathFailed, TEXT("Navigation Move rejected EQS location"));
	}
}

void UCombatOrderComponent::HandleMoveFinished(
	const FAIRequestID RequestId,
	const FPathFollowingResult& Result)
{
	if (!CurrentOrder.IsSet() || !RequestId.IsValid() || RequestId != ActiveMoveRequestId
		|| ActiveMoveOrderHandle != CurrentOrder->Handle
		|| ActiveMoveOrderHandle.Key.Generation != OrderGeneration
		|| ActiveMoveOrderHandle.Key.LifeGeneration != GetOwnerUnit()->GetLifeGeneration())
	{
		return;
	}
	const bool bPartial = ActiveMovePath.IsValid() && ActiveMovePath->IsPartial();
	ActiveMoveRequestId = FAIRequestID::InvalidRequest;
	ActiveMoveOrderHandle = FCombatOrderHandle();
	ActiveMovePath.Reset();
	const FGameplayTag FailureTag = Result.Code == EPathFollowingResult::Blocked
		? CombatTags::Order_Failure_Blocked.GetTag() : CombatTags::Order_Failure_PathFailed.GetTag();
	FString Diagnostic = FString::Printf(TEXT("Move finished Code=%d Partial=%d"),
		static_cast<int32>(Result.Code), bPartial);
	if (const ACombatUnitCharacter* Unit = GetOwnerUnit(); Unit && CurrentOrder->Request.TargetUnit)
	{
		const ACombatUnitCharacter* Target = CurrentOrder->Request.TargetUnit;
		const float EdgeDistance = FMath::Max(0.0f,
			FVector::Dist2D(Unit->GetActorLocation(), Target->GetActorLocation())
			- Unit->GetCapsuleComponent()->GetScaledCapsuleRadius()
			- Target->GetCapsuleComponent()->GetScaledCapsuleRadius());
		const UCharacterMovementComponent* Movement = Unit->GetCharacterMovement();
		Diagnostic += FString::Printf(
			TEXT(" Source=%s Target=%s Edge=%.2f Desired=%.2f Speed=%.2f Accel=%.2f Mode=%d Velocity=%s Requested=%s Input=%s"),
			*Unit->GetActorLocation().ToCompactString(), *Target->GetActorLocation().ToCompactString(),
			EdgeDistance, GetCurrentDesiredRange(), Movement ? Movement->MaxWalkSpeed : -1.0f,
			Movement ? Movement->GetMaxAcceleration() : -1.0f,
			Movement ? static_cast<int32>(Movement->MovementMode) : -1,
			Movement ? *Movement->Velocity.ToCompactString() : TEXT("None"),
			Movement ? *Movement->GetLastUpdateRequestedVelocity().ToCompactString() : TEXT("None"),
			Movement ? *Movement->GetLastInputVector().ToCompactString() : TEXT("None"));

		const FVector ProbeDirection = (Target->GetActorLocation() - Unit->GetActorLocation()).GetSafeNormal2D();
		FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(CombatOrderMoveProbe), false, Unit);
		QueryParams.AddIgnoredActor(Target);
		FCollisionResponseParams ResponseParams;
		Unit->GetCapsuleComponent()->InitSweepCollisionParams(QueryParams, ResponseParams);
		FHitResult ProbeHit;
		const bool bProbeBlocked = GetWorld()->SweepSingleByChannel(
			ProbeHit, Unit->GetActorLocation(), Unit->GetActorLocation() + ProbeDirection * 10.0f,
			FQuat::Identity, Unit->GetCapsuleComponent()->GetCollisionObjectType(),
			FCollisionShape::MakeCapsule(Unit->GetCapsuleComponent()->GetScaledCapsuleRadius(),
				Unit->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()),
			QueryParams, ResponseParams);
		if (bProbeBlocked)
		{
			const FBoxSphereBounds HitBounds = ProbeHit.GetComponent()->Bounds;
			Diagnostic += FString::Printf(
				TEXT(" ProbeHit=%s Component=%s Penetrating=%d Normal=%s BoundsOrigin=%s BoundsExtent=%s"),
				*GetNameSafe(ProbeHit.GetActor()), *GetNameSafe(ProbeHit.GetComponent()),
				ProbeHit.bStartPenetrating, *ProbeHit.Normal.ToCompactString(),
				*HitBounds.Origin.ToCompactString(), *HitBounds.BoxExtent.ToCompactString());
		}
	}
	ResolveMoveCompletion(Result.Code == EPathFollowingResult::Success, bPartial, FailureTag,
		Diagnostic);
}

void UCombatOrderComponent::ResolveMoveCompletion(
	const bool bSuccess,
	const bool bPartial,
	const FGameplayTag FailureTag,
	const FString& Diagnostic)
{
	// 目标 Actor 周围通常不可导航，因此成功路径可能标记 partial；以 gameplay 边缘距离作最终裁决。
	if (bSuccess && (!bPartial || IsCurrentDestinationReached()))
	{
		MoveRetryCount = 0;
		CancelMovementAsync();
		TransitionTo(ECombatOrderState::Validating);
		PumpCurrentOrder();
		return;
	}
	ScheduleMoveRetry(FailureTag, Diagnostic);
}

void UCombatOrderComponent::TransitionTo(
	const ECombatOrderState NewState,
	const FGameplayTag FailureTag,
	const FString& Diagnostic)
{
	if (CurrentState == NewState && Diagnostic.IsEmpty() && !FailureTag.IsValid())
	{
		return;
	}
	CurrentState = NewState;
	UCombatEventSubsystem* Events = GetWorld() ? GetWorld()->GetSubsystem<UCombatEventSubsystem>() : nullptr;
	ACombatUnitCharacter* Unit = GetOwnerUnit();
	if (!Events || !Unit || !CurrentOrder.IsSet())
	{
		return;
	}
	FCombatLogRecord Record;
	Record.Context = Events->CreateRootEvent();
	Record.EventType = CombatTags::Event_Combat_OrderStateChanged;
	Record.FailureTag = FailureTag;
	Record.SourceActorId = Unit->GetUniqueID();
	Record.TargetActorId = CurrentOrder->Request.TargetUnit ? CurrentOrder->Request.TargetUnit->GetUniqueID() : 0;
	Record.UnitLifeGeneration = Unit->GetLifeGeneration();
	Record.Diagnostic = FString::Printf(TEXT("Order=%s Type=%d State=%d %s"),
		*CurrentOrder->Handle.ToString(), static_cast<int32>(CurrentOrder->Request.Type),
		static_cast<int32>(NewState), *Diagnostic);
	Events->Emit(Record);
}
