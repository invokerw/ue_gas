// Copyright Epic Games, Inc. All Rights Reserved.

#include "ue_gasPlayerController.h"
#include "GameFramework/Pawn.h"
#include "Blueprint/AIBlueprintHelperLibrary.h"
#include "NiagaraSystem.h"
#include "NiagaraFunctionLibrary.h"
#include "ue_gasCharacter.h"
#include "Engine/World.h"
#include "EnhancedInputComponent.h"
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
	bMoveToMouseCursor = false;

	// create the path following comp
	PathFollowingComponent = CreateDefaultSubobject<UPathFollowingComponent>(TEXT("Path Following Component"));

	// configure the controller
	bShowMouseCursor = true;
	DefaultMouseCursor = EMouseCursor::Default;
	CachedDestination = FVector::ZeroVector;
	FollowTime = 0.f;
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
			EnhancedInputComponent->BindAction(SetDestinationTouchAction, ETriggerEvent::Started, this, &Aue_gasPlayerController::OnInputStarted);
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
	StopMovement();

	// Update the move destination to wherever the cursor is pointing at
	UpdateCachedDestination();
}

void Aue_gasPlayerController::OnSetDestinationTriggered()
{
	// We flag that the input is being pressed
	FollowTime += GetWorld()->GetDeltaSeconds();
	
	// Update the move destination to wherever the cursor is pointing at
	UpdateCachedDestination();
	
	// Move towards mouse pointer or touch
	APawn* ControlledPawn = GetPawn();
	if (ControlledPawn != nullptr)
	{
		FVector WorldDirection = (CachedDestination - ControlledPawn->GetActorLocation()).GetSafeNormal();
		ControlledPawn->AddMovementInput(WorldDirection, 1.0, false);
	}
}

void Aue_gasPlayerController::OnSetDestinationReleased()
{
	// If it was a short press
	if (FollowTime <= ShortPressThreshold)
	{
		// We move there and spawn some particles
		UAIBlueprintHelperLibrary::SimpleMoveToLocation(this, CachedDestination);
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, FXCursor, CachedDestination, FRotator::ZeroRotator, FVector(1.f, 1.f, 1.f), true, true, ENCPoolMethod::None, true);
	}

	FollowTime = 0.f;
}

// Triggered every frame when the input is held down
void Aue_gasPlayerController::OnTouchTriggered()
{
	bIsTouch = true;
	OnSetDestinationTriggered();
}

void Aue_gasPlayerController::OnTouchReleased()
{
	bIsTouch = false;
	OnSetDestinationReleased();
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

void Aue_gasPlayerController::UpdateCachedDestination()
{
	// We look for the location in the world where the player has pressed the input
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

	// If we hit a surface, cache the location
	if (bHitSuccessful)
	{
		CachedDestination = Hit.Location;
	}
}
