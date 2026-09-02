// Copyright Epic Games, Inc. All Rights Reserved.

#include "ue_gasPlayerController.h"
#include "GameFramework/Pawn.h"
#include "NiagaraSystem.h"
#include "NiagaraFunctionLibrary.h"
#include "ue_gasCharacter.h"
#include "Engine/World.h"
#include "EnhancedInputComponent.h"
#include "NavigationPath.h"
#include "Navigation/PathFollowingComponent.h"
#include "InputActionValue.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "EngineUtils.h"

#include "Combat/Ability/CombatAbilitySystemComponent.h"
#include "Combat/Ability/CombatGameplayAbility.h"
#include "Combat/Core/CombatTags.h"
#include "Combat/Data/CombatDefinitionData.h"
#include "Combat/Network/CombatNetworkTypes.h"
#include "Combat/Unit/CombatUnitCharacter.h"
#include "ue_gas.h"

Aue_gasPlayerController::Aue_gasPlayerController()
{
	bIsTouch = false;

	// 当前 Demo 由 PlayerController 直接占有 Unit；服务器与 owning client 用同一组件衔接 Order 路径。
	PathFollowingComponent = CreateDefaultSubobject<UPathFollowingComponent>(TEXT("Path Following Component"));

	// configure the controller
	bShowMouseCursor = true;
	DefaultMouseCursor = EMouseCursor::Default;
	CachedDestination = FVector::ZeroVector;
}

void Aue_gasPlayerController::ClientFollowCombatOrderPath_Implementation(
	const TArray<FVector_NetQuantize10>& PathPoints,
	const FVector_NetQuantize10 GoalLocation,
	const float AcceptanceRadius)
{
	if (!IsLocalController() || !GetPawn() || !PathFollowingComponent || PathPoints.Num() < 2)
	{
		return;
	}

	// 远端 PlayerController 的 CharacterMovement 只在拥有客户端生成预测移动；路径仍由服务器计算并批准。
	PathFollowingComponent->Initialize();
	TArray<FVector> LocalPathPoints;
	LocalPathPoints.Reserve(PathPoints.Num());
	for (const FVector_NetQuantize10& Point : PathPoints)
	{
		LocalPathPoints.Add(Point);
	}
	// 避免网络延迟造成首点轻微落后于客户端预测位置，从当前导航位置接入服务器路径。
	LocalPathPoints[0] = GetNavAgentLocation();

	FAIMoveRequest MoveRequest;
	MoveRequest.SetGoalLocation(GoalLocation);
	MoveRequest.SetAcceptanceRadius(AcceptanceRadius);
	MoveRequest.SetReachTestIncludesAgentRadius(true);
	MoveRequest.SetReachTestIncludesGoalRadius(true);
	MoveRequest.SetAllowPartialPath(true);
	MoveRequest.SetUsePathfinding(false);
	MoveRequest.SetProjectGoalLocation(false);
	MoveRequest.SetCanStrafe(false);

	const FNavPathSharedPtr ClientPath = MakeShared<FNavigationPath, ESPMode::ThreadSafe>(LocalPathPoints, nullptr);
	PathFollowingComponent->RequestMove(MoveRequest, ClientPath);
}

void Aue_gasPlayerController::ClientStopCombatOrderNavigation_Implementation()
{
	if (PathFollowingComponent && PathFollowingComponent->GetStatus() != EPathFollowingStatus::Idle)
	{
		PathFollowingComponent->AbortMove(
			*this,
			FPathFollowingResultFlags::ForcedScript,
			FAIRequestID::AnyRequest,
			EPathFollowingVelocityMode::Reset);
	}
}

void Aue_gasPlayerController::SetupInputComponent()
{
	// set up gameplay key bindings
	Super::SetupInputComponent();

	// Only set up input on local player controllers
	if (IsLocalPlayerController())
	{
		// Add Input Mapping Context
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
		{
			Subsystem->AddMappingContext(DefaultMappingContext, 0);
		}

		// Set up action bindings
		if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(InputComponent))
		{
			// Setup mouse input events
			EnhancedInputComponent->BindAction(SetDestinationClickAction, ETriggerEvent::Started, this, &Aue_gasPlayerController::OnInputStarted);
			EnhancedInputComponent->BindAction(SetDestinationClickAction, ETriggerEvent::Triggered, this, &Aue_gasPlayerController::OnSetDestinationTriggered);
			EnhancedInputComponent->BindAction(SetDestinationClickAction, ETriggerEvent::Completed, this, &Aue_gasPlayerController::OnSetDestinationReleased);
			EnhancedInputComponent->BindAction(SetDestinationClickAction, ETriggerEvent::Canceled, this, &Aue_gasPlayerController::OnSetDestinationReleased);

			// Setup touch input events
			EnhancedInputComponent->BindAction(SetDestinationTouchAction, ETriggerEvent::Started, this, &Aue_gasPlayerController::OnTouchStarted);
			EnhancedInputComponent->BindAction(SetDestinationTouchAction, ETriggerEvent::Triggered, this, &Aue_gasPlayerController::OnTouchTriggered);
			EnhancedInputComponent->BindAction(SetDestinationTouchAction, ETriggerEvent::Completed, this, &Aue_gasPlayerController::OnTouchReleased);
			EnhancedInputComponent->BindAction(SetDestinationTouchAction, ETriggerEvent::Canceled, this, &Aue_gasPlayerController::OnTouchReleased);

			if (AbilitySlotQAction)
			{
				EnhancedInputComponent->BindAction(AbilitySlotQAction, ETriggerEvent::Started, this, &Aue_gasPlayerController::OnAbilitySlotQ);
			}
			if (AbilitySlotWAction)
			{
				EnhancedInputComponent->BindAction(AbilitySlotWAction, ETriggerEvent::Started, this, &Aue_gasPlayerController::OnAbilitySlotW);
			}
			if (AbilitySlotEAction)
			{
				EnhancedInputComponent->BindAction(AbilitySlotEAction, ETriggerEvent::Started, this, &Aue_gasPlayerController::OnAbilitySlotE);
			}
			if (AbilitySlotRAction)
			{
				EnhancedInputComponent->BindAction(AbilitySlotRAction, ETriggerEvent::Started, this, &Aue_gasPlayerController::OnAbilitySlotR);
			}

		}
		else
		{
			UE_LOG(Logue_gas, Error, TEXT("'%s' Failed to find an Enhanced Input Component! This template is built to use the Enhanced Input system. If you intend to use the legacy system, then you will need to update this C++ file."), *GetNameSafe(this));
		}
	}
}

void Aue_gasPlayerController::OnInputStarted()
{
	MoveOrderRefreshElapsed = 0.0f;
	bHasIssuedMoveOrder = false;
	bHasCachedDestination = UpdateCachedDestination();
	if (bHasCachedDestination)
	{
		IssueCombatMoveOrder();
	}
}

void Aue_gasPlayerController::OnSetDestinationTriggered()
{
	MoveOrderRefreshElapsed += GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.0f;
	if (UpdateCachedDestination())
	{
		bHasCachedDestination = true;
	}

	// 按住拖动只在固定节奏且目标明显变化时替换 Order，避免 Reliable RPC 按帧发送。
	if (bHasCachedDestination
		&& (!bHasIssuedMoveOrder
			|| (MoveOrderRefreshElapsed >= MoveOrderRefreshInterval
				&& FVector::DistSquared2D(CachedDestination, LastIssuedMoveDestination)
					>= FMath::Square(MoveOrderWakeDistance))))
	{
		IssueCombatMoveOrder();
	}
}

void Aue_gasPlayerController::OnSetDestinationReleased()
{
	// 拖动结束时补交最后一个明显不同的目标，确保服务器收到最终落点。
	if (bHasCachedDestination
		&& (!bHasIssuedMoveOrder
			|| FVector::DistSquared2D(CachedDestination, LastIssuedMoveDestination)
				>= FMath::Square(MoveOrderWakeDistance)))
	{
		IssueCombatMoveOrder();
	}
	if (bHasCachedDestination)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			this, FXCursor, CachedDestination, FRotator::ZeroRotator, FVector::OneVector,
			true, true, ENCPoolMethod::None, true);
	}
	MoveOrderRefreshElapsed = 0.0f;
	bHasCachedDestination = false;
	bHasIssuedMoveOrder = false;
}

void Aue_gasPlayerController::OnTouchStarted()
{
	bIsTouch = true;
	OnInputStarted();
}

void Aue_gasPlayerController::OnTouchTriggered()
{
	bIsTouch = true;
	OnSetDestinationTriggered();
}

void Aue_gasPlayerController::OnTouchReleased()
{
	OnSetDestinationReleased();
	bIsTouch = false;
}

void Aue_gasPlayerController::OnAbilitySlotQ()
{
	ActivateCombatAbilitySlot(0);
}

void Aue_gasPlayerController::OnAbilitySlotW()
{
	ActivateCombatAbilitySlot(1);
}

void Aue_gasPlayerController::OnAbilitySlotE()
{
	ActivateCombatAbilitySlot(2);
}

void Aue_gasPlayerController::OnAbilitySlotR()
{
	ActivateCombatAbilitySlot(3);
}

void Aue_gasPlayerController::ActivateCombatAbilitySlot(const int32 SlotIndex)
{
	ACombatUnitCharacter* Unit = Cast<ACombatUnitCharacter>(GetPawn());
	UCombatAbilitySystemComponent* Asc = Unit ? Unit->GetCombatAbilitySystemComponent() : nullptr;
	if (!Unit || !Asc)
	{
		UE_LOG(Logue_gas, Warning, TEXT("Combat ability slot %d ignored: controlled pawn has no Combat ASC"), SlotIndex + 1);
		return;
	}

	TArray<const FGameplayAbilitySpec*> SlottedAbilities;
	for (const FGameplayAbilitySpec& Spec : Asc->GetActivatableAbilities())
	{
		const UCombatGameplayAbility* CombatAbility = Cast<UCombatGameplayAbility>(Spec.Ability);
		const UCombatAbilityData* AbilityData = CombatAbility ? CombatAbility->GetAbilityData() : nullptr;
		if (!AbilityData || AbilityData->BehaviorTags.HasTagExact(CombatTags::Ability_Behavior_Passive))
		{
			continue;
		}
		SlottedAbilities.Add(&Spec);
	}

	if (!SlottedAbilities.IsValidIndex(SlotIndex))
	{
		UE_LOG(Logue_gas, Display, TEXT("Combat ability slot %d is empty"), SlotIndex + 1);
		return;
	}

	const FGameplayAbilitySpec& Spec = *SlottedAbilities[SlotIndex];
	const UCombatAbilityData* AbilityData = Asc->GetCombatAbilityData(Spec.Handle);
	if (!AbilityData)
	{
		return;
	}

	FHitResult CursorHit;
	const bool bHasCursorHit = GetHitResultUnderCursor(ECC_Visibility, true, CursorHit);
	FCombatOrderRequest Order;
	Order.AbilitySpecHandle = Spec.Handle;

	if (AbilityData->BehaviorTags.HasTagExact(CombatTags::Ability_Behavior_UnitTarget))
	{
		Order.Type = ECombatOrderType::CastTarget;
		Order.TargetUnit = bHasCursorHit ? FindCombatUnitUnderCursor(CursorHit.Location) : nullptr;
		if (!Order.TargetUnit)
		{
			UE_LOG(Logue_gas, Display, TEXT("Combat ability slot %d needs a unit under the mouse cursor"), SlotIndex + 1);
			return;
		}
	}
	else if (AbilityData->BehaviorTags.HasTagExact(CombatTags::Ability_Behavior_PointTarget))
	{
		if (!bHasCursorHit)
		{
			UE_LOG(Logue_gas, Display, TEXT("Combat ability slot %d needs a world position under the mouse cursor"), SlotIndex + 1);
			return;
		}
		Order.Type = ECombatOrderType::CastPoint;
		Order.TargetLocation = CursorHit.Location;
		Order.bHasTargetLocation = true;
	}
	else
	{
		Order.Type = ECombatOrderType::CastNoTarget;
	}

	FCombatOrderBatchRequest Batch;
	Batch.RequestId = NextCombatOrderRequestId;
	Batch.Orders.Add(Order);
	NextCombatOrderRequestId = NextCombatOrderRequestId == MAX_int32 ? 1 : NextCombatOrderRequestId + 1;
	Unit->ServerIssueOrderBatch(MoveTemp(Batch));
}

bool Aue_gasPlayerController::IssueCombatMoveOrder()
{
	ACombatUnitCharacter* Unit = Cast<ACombatUnitCharacter>(GetPawn());
	if (!Unit || !bHasCachedDestination || CachedDestination.ContainsNaN())
	{
		return false;
	}

	FCombatOrderRequest Order;
	Order.Type = ECombatOrderType::MoveToPoint;
	Order.TargetLocation = CachedDestination;
	Order.bHasTargetLocation = true;

	FCombatOrderBatchRequest Batch;
	Batch.RequestId = NextCombatOrderRequestId;
	Batch.bAppendToExistingQueue = false;
	Batch.Orders.Add(Order);
	NextCombatOrderRequestId = NextCombatOrderRequestId == MAX_int32 ? 1 : NextCombatOrderRequestId + 1;
	Unit->ServerIssueOrderBatch(MoveTemp(Batch));

	LastIssuedMoveDestination = CachedDestination;
	MoveOrderRefreshElapsed = 0.0f;
	bHasIssuedMoveOrder = true;
	return true;
}

ACombatUnitCharacter* Aue_gasPlayerController::FindCombatUnitUnderCursor(const FVector& CursorWorldLocation) const
{
	FHitResult CursorHit;
	if (GetHitResultUnderCursor(ECC_Visibility, true, CursorHit))
	{
		if (ACombatUnitCharacter* DirectTarget = Cast<ACombatUnitCharacter>(CursorHit.GetActor()))
		{
			return DirectTarget;
		}
	}

	// The demo dummy can have thin visible geometry. Accept a nearby unit around
	// the cursor impact so the Q/W/E/R controls remain pleasant to use. If the
	// cursor is not over a unit, fall back to the nearest unit in demo cast range.
	ACombatUnitCharacter* BestTarget = nullptr;
	float BestDistanceSquared = FMath::Square(175.0f);
	ACombatUnitCharacter* NearestTarget = nullptr;
	float NearestDistanceSquared = FMath::Square(1000.0f);
	for (TActorIterator<ACombatUnitCharacter> It(GetWorld()); It; ++It)
	{
		ACombatUnitCharacter* Candidate = *It;
		if (!Candidate || Candidate == GetPawn())
		{
			continue;
		}
		const float DistanceSquared = FVector::DistSquared2D(CursorWorldLocation, Candidate->GetActorLocation());
		if (DistanceSquared < BestDistanceSquared)
		{
			BestDistanceSquared = DistanceSquared;
			BestTarget = Candidate;
		}

		const float DistanceFromPawnSquared = FVector::DistSquared2D(GetPawn()->GetActorLocation(), Candidate->GetActorLocation());
		if (DistanceFromPawnSquared < NearestDistanceSquared)
		{
			NearestDistanceSquared = DistanceFromPawnSquared;
			NearestTarget = Candidate;
		}
	}
	return BestTarget ? BestTarget : NearestTarget;
}

bool Aue_gasPlayerController::UpdateCachedDestination()
{
	FHitResult Hit;
	bool bHitSuccessful = false;
	if (bIsTouch)
	{
		bHitSuccessful = GetHitResultUnderFinger(ETouchIndex::Touch1, ECollisionChannel::ECC_Visibility, true, Hit);
	}
	else
	{
		bHitSuccessful = GetHitResultUnderCursor(ECollisionChannel::ECC_Visibility, true, Hit);
	}

	if (!bHitSuccessful || Hit.Location.ContainsNaN())
	{
		return false;
	}
	CachedDestination = Hit.Location;
	return true;
}
