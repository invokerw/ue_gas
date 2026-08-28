// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
//#include "Templates/SubclassOf.h"
#include "GameFramework/PlayerController.h"
#include "ue_gasPlayerController.generated.h"

class UNiagaraSystem;
class UInputMappingContext;
class UInputAction;
class UPathFollowingComponent;
class ACombatUnitCharacter;

DECLARE_LOG_CATEGORY_EXTERN(LogTemplateCharacter, Log, All);

/**
 *  Player controller for a top-down perspective game.
 *  Implements point and click based controls
 */
UCLASS(abstract)
class Aue_gasPlayerController : public APlayerController
{
	GENERATED_BODY()

protected:

	/** Component used for moving along a NavMesh path. */
	UPROPERTY(VisibleDefaultsOnly, Category = AI)
	TObjectPtr<UPathFollowingComponent> PathFollowingComponent;

	/** Time Threshold to know if it was a short press */
	UPROPERTY(EditAnywhere, Category="Input")
	float ShortPressThreshold;

	/** FX Class that we will spawn when clicking */
	UPROPERTY(EditAnywhere, Category="Input")
	TObjectPtr<UNiagaraSystem> FXCursor;

	/** MappingContext */
	UPROPERTY(EditAnywhere, Category="Input")
	TObjectPtr<UInputMappingContext> DefaultMappingContext;
	
	/** Jump Input Action */
	UPROPERTY(EditAnywhere, Category="Input")
	TObjectPtr<UInputAction> SetDestinationClickAction;

	/** Jump Input Action */
	UPROPERTY(EditAnywhere, Category="Input")
	TObjectPtr<UInputAction> SetDestinationTouchAction;

	/** Combat ability slot input actions. */
	UPROPERTY(EditAnywhere, Category="Input|Abilities")
	TObjectPtr<UInputAction> AbilitySlotQAction;

	UPROPERTY(EditAnywhere, Category="Input|Abilities")
	TObjectPtr<UInputAction> AbilitySlotWAction;

	UPROPERTY(EditAnywhere, Category="Input|Abilities")
	TObjectPtr<UInputAction> AbilitySlotEAction;

	UPROPERTY(EditAnywhere, Category="Input|Abilities")
	TObjectPtr<UInputAction> AbilitySlotRAction;

	/** True if the controlled character should navigate to the mouse cursor. */
	uint32 bMoveToMouseCursor : 1;

	/** Set to true if we're using touch input */
	uint32 bIsTouch : 1;

	/** Saved location of the character movement destination */
	FVector CachedDestination;

	/** Time that the click input has been pressed */
	float FollowTime = 0.0f;

public:

	/** Constructor */
	Aue_gasPlayerController();

protected:

	/** Initialize input bindings */
	virtual void SetupInputComponent() override;
	
	/** Input handlers */
	void OnInputStarted();
	void OnSetDestinationTriggered();
	void OnSetDestinationReleased();
	void OnTouchTriggered();
	void OnTouchReleased();

	/** Activate the first four granted, non-passive combat abilities with Q/W/E/R. */
	void OnAbilitySlotQ();
	void OnAbilitySlotW();
	void OnAbilitySlotE();
	void OnAbilitySlotR();
	void ActivateCombatAbilitySlot(int32 SlotIndex);
	ACombatUnitCharacter* FindCombatUnitUnderCursor(const FVector& CursorWorldLocation) const;

	/** Helper function to get the move destination */
	void UpdateCachedDestination();

	/** Monotonic, non-zero request id used by the combat order RPC replay guard. */
	int32 NextCombatOrderRequestId = 1;
};


