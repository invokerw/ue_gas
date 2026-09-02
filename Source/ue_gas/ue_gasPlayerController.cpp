// Copyright Epic Games, Inc. All Rights Reserved.

#include "ue_gasPlayerController.h"

#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "Net/UnrealNetwork.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"

#include "Combat/Ability/CombatAbilitySystemComponent.h"
#include "Combat/Ability/CombatGameplayAbility.h"
#include "Combat/Core/CombatTags.h"
#include "Combat/Data/CombatDefinitionData.h"
#include "Combat/Log/CombatEventSubsystem.h"
#include "Combat/Network/CombatNetworkTypes.h"
#include "Combat/Order/CombatOrderComponent.h"
#include "Combat/Unit/CombatUnitAIController.h"
#include "Combat/Unit/CombatUnitCharacter.h"
#include "ue_gas.h"
#include "ue_gasCharacter.h"

Aue_gasPlayerController::Aue_gasPlayerController()
{
	bIsTouch = false;
	bShowMouseCursor = true;
	DefaultMouseCursor = EMouseCursor::Default;
	CommandPawnClass = Aue_gasCharacter::StaticClass();
}

void Aue_gasPlayerController::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(Aue_gasPlayerController, CommandedUnit, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(Aue_gasPlayerController, CommandBindingGeneration, COND_OwnerOnly);
}

bool Aue_gasPlayerController::SetCommandedUnitAuthority(ACombatUnitCharacter* NewUnit)
{
	if (!HasAuthority() || (NewUnit && NewUnit->GetWorld() != GetWorld()))
	{
		return false;
	}
	if (CommandedUnit == NewUnit)
	{
		if (NewUnit && NewUnit->GetCommandingPlayerController() != this
			&& !NewUnit->SetCommandingPlayerController(this))
		{
			return false;
		}
		RefreshCommandBinding();
		return true;
	}

	ACombatUnitCharacter* PreviousUnit = CommandedUnit;
	if (NewUnit)
	{
		if (Aue_gasPlayerController* PreviousController =
			Cast<Aue_gasPlayerController>(NewUnit->GetCommandingPlayerController());
			PreviousController && PreviousController != this)
		{
			PreviousController->SetCommandedUnitAuthority(nullptr);
		}
		else if (APlayerController* PreviousOwner = NewUnit->GetCommandingPlayerController(); PreviousOwner != this)
		{
			if (UCombatOrderComponent* Orders = NewUnit->GetCombatOrderComponent())
			{
				Orders->StopAllOrders(CombatTags::Order_Failure_Cancelled);
			}
			NewUnit->SetCommandingPlayerController(nullptr);
		}
	}

	// 指针只在清理完成后一次性发布；旧连接随后到达的 Unit RPC 会被 Owner 校验拒绝。
	CommandedUnit = nullptr;
	if (PreviousUnit)
	{
		if (UCombatOrderComponent* Orders = PreviousUnit->GetCombatOrderComponent())
		{
			Orders->StopAllOrders(CombatTags::Order_Failure_Cancelled);
		}
		PreviousUnit->SetCommandingPlayerController(nullptr);
	}

	bool bSuccess = true;
	if (NewUnit)
	{
		bSuccess = NewUnit->SetCommandingPlayerController(this);
		if (bSuccess)
		{
			CommandedUnit = NewUnit;
		}
	}
	AdvanceCommandBindingGeneration();
	ForceNetUpdate();
	RefreshCommandBinding();

	UE_LOG(LogCombat, Log,
		TEXT("SAMCommandBinding Controller=%s OldUnit=%s NewUnit=%s CommandBindingGeneration=%d Success=%s"),
		*GetName(), *GetNameSafe(PreviousUnit), *GetNameSafe(CommandedUnit), CommandBindingGeneration,
		bSuccess ? TEXT("Yes") : TEXT("No"));
	if (CommandedUnit)
	{
		CommandedUnit->LogServerMovementTopology(TEXT("CommandBindingChanged"));
	}
	return bSuccess;
}

void Aue_gasPlayerController::HandleCommandedUnitEndPlay(ACombatUnitCharacter* EndingUnit)
{
	if (!HasAuthority() || !EndingUnit || CommandedUnit != EndingUnit)
	{
		return;
	}
	CommandedUnit = nullptr;
	AdvanceCommandBindingGeneration();
	ForceNetUpdate();
	RefreshCommandBinding();
}

void Aue_gasPlayerController::OnPossess(APawn* InPawn)
{
	if (HasAuthority())
	{
		if (ACombatUnitCharacter* CombatUnit = Cast<ACombatUnitCharacter>(InPawn))
		{
			// 这是配置错误的兜底；生产 GameMode 不再把 Unit 作为待占有 Pawn 返回。
			UE_LOG(LogCombat, Error,
				TEXT("SAMDirectCombatUnitPossessRejected Controller=%s Unit=%s"),
				*GetName(), *GetNameSafe(InPawn));
			if (!CombatUnit->GetController())
			{
				CombatUnit->SpawnDefaultController();
			}
			return;
		}
	}
	Super::OnPossess(InPawn);
	RefreshCommandBinding();
}

void Aue_gasPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (HasAuthority())
	{
		SetCommandedUnitAuthority(nullptr);
	}
	Super::EndPlay(EndPlayReason);
}

void Aue_gasPlayerController::OnRep_CommandedUnit()
{
	RefreshCommandBinding();
}

void Aue_gasPlayerController::OnRep_CommandBindingGeneration()
{
	RefreshCommandBinding();
}

void Aue_gasPlayerController::RefreshCommandBinding()
{
	if (Aue_gasCharacter* CommandPawn = Cast<Aue_gasCharacter>(GetPawn()))
	{
		// Unit Owner 可能比 CommandedUnit 晚到；相机可以先安全观察，输入仍由 GetReadyCommandedUnit 阻止。
		CommandPawn->SetFollowTarget(CommandedUnit);
	}
}

void Aue_gasPlayerController::AdvanceCommandBindingGeneration()
{
	CommandBindingGeneration = CommandBindingGeneration >= MAX_int32 ? 1 : CommandBindingGeneration + 1;
	if (CommandBindingGeneration <= 0)
	{
		CommandBindingGeneration = 1;
	}
}

ACombatUnitCharacter* Aue_gasPlayerController::GetReadyCommandedUnit() const
{
	return CommandedUnit && CommandedUnit->GetCommandingPlayerController() == this
		? CommandedUnit.Get() : nullptr;
}

void Aue_gasPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	if (!IsLocalPlayerController())
	{
		return;
	}
	if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
		ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
	{
		Subsystem->AddMappingContext(DefaultMappingContext, 0);
	}

	UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(InputComponent);
	if (!EnhancedInputComponent)
	{
		UE_LOG(Logue_gas, Error, TEXT("'%s' failed to find an Enhanced Input Component"), *GetNameSafe(this));
		return;
	}
	EnhancedInputComponent->BindAction(SetDestinationClickAction, ETriggerEvent::Started, this, &Aue_gasPlayerController::OnInputStarted);
	EnhancedInputComponent->BindAction(SetDestinationClickAction, ETriggerEvent::Triggered, this, &Aue_gasPlayerController::OnSetDestinationTriggered);
	EnhancedInputComponent->BindAction(SetDestinationClickAction, ETriggerEvent::Completed, this, &Aue_gasPlayerController::OnSetDestinationReleased);
	EnhancedInputComponent->BindAction(SetDestinationClickAction, ETriggerEvent::Canceled, this, &Aue_gasPlayerController::OnSetDestinationReleased);
	EnhancedInputComponent->BindAction(SetDestinationTouchAction, ETriggerEvent::Started, this, &Aue_gasPlayerController::OnTouchStarted);
	EnhancedInputComponent->BindAction(SetDestinationTouchAction, ETriggerEvent::Triggered, this, &Aue_gasPlayerController::OnTouchTriggered);
	EnhancedInputComponent->BindAction(SetDestinationTouchAction, ETriggerEvent::Completed, this, &Aue_gasPlayerController::OnTouchReleased);
	EnhancedInputComponent->BindAction(SetDestinationTouchAction, ETriggerEvent::Canceled, this, &Aue_gasPlayerController::OnTouchReleased);
	if (AbilitySlotQAction) EnhancedInputComponent->BindAction(AbilitySlotQAction, ETriggerEvent::Started, this, &Aue_gasPlayerController::OnAbilitySlotQ);
	if (AbilitySlotWAction) EnhancedInputComponent->BindAction(AbilitySlotWAction, ETriggerEvent::Started, this, &Aue_gasPlayerController::OnAbilitySlotW);
	if (AbilitySlotEAction) EnhancedInputComponent->BindAction(AbilitySlotEAction, ETriggerEvent::Started, this, &Aue_gasPlayerController::OnAbilitySlotE);
	if (AbilitySlotRAction) EnhancedInputComponent->BindAction(AbilitySlotRAction, ETriggerEvent::Started, this, &Aue_gasPlayerController::OnAbilitySlotR);
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
	if (bHasCachedDestination
		&& (!bHasIssuedMoveOrder
			|| FVector::DistSquared2D(CachedDestination, LastIssuedMoveDestination)
				>= FMath::Square(MoveOrderWakeDistance)))
	{
		IssueCombatMoveOrder();
	}
	if (bHasCachedDestination && FXCursor)
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

void Aue_gasPlayerController::OnAbilitySlotQ() { ActivateCombatAbilitySlot(0); }
void Aue_gasPlayerController::OnAbilitySlotW() { ActivateCombatAbilitySlot(1); }
void Aue_gasPlayerController::OnAbilitySlotE() { ActivateCombatAbilitySlot(2); }
void Aue_gasPlayerController::OnAbilitySlotR() { ActivateCombatAbilitySlot(3); }

void Aue_gasPlayerController::ActivateCombatAbilitySlot(const int32 SlotIndex)
{
	ACombatUnitCharacter* Unit = GetReadyCommandedUnit();
	UCombatAbilitySystemComponent* Asc = Unit ? Unit->GetCombatAbilitySystemComponent() : nullptr;
	if (!Unit || !Asc)
	{
		UE_LOG(Logue_gas, Warning, TEXT("Combat ability slot %d ignored: CommandedUnit is not ready"), SlotIndex + 1);
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
			return;
		}
	}
	else if (AbilityData->BehaviorTags.HasTagExact(CombatTags::Ability_Behavior_PointTarget))
	{
		if (!bHasCursorHit)
		{
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
	ACombatUnitCharacter* Unit = GetReadyCommandedUnit();
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
	const ACombatUnitCharacter* SourceUnit = CommandedUnit;
	ACombatUnitCharacter* BestTarget = nullptr;
	float BestDistanceSquared = FMath::Square(175.0f);
	ACombatUnitCharacter* NearestTarget = nullptr;
	float NearestDistanceSquared = FMath::Square(1000.0f);
	for (TActorIterator<ACombatUnitCharacter> It(GetWorld()); It; ++It)
	{
		ACombatUnitCharacter* Candidate = *It;
		if (!Candidate || Candidate == SourceUnit)
		{
			continue;
		}
		const float DistanceSquared = FVector::DistSquared2D(CursorWorldLocation, Candidate->GetActorLocation());
		if (DistanceSquared < BestDistanceSquared)
		{
			BestDistanceSquared = DistanceSquared;
			BestTarget = Candidate;
		}
		if (SourceUnit)
		{
			const float SourceDistanceSquared = FVector::DistSquared2D(SourceUnit->GetActorLocation(), Candidate->GetActorLocation());
			if (SourceDistanceSquared < NearestDistanceSquared)
			{
				NearestDistanceSquared = SourceDistanceSquared;
				NearestTarget = Candidate;
			}
		}
	}
	return BestTarget ? BestTarget : NearestTarget;
}

bool Aue_gasPlayerController::UpdateCachedDestination()
{
	FHitResult Hit;
	const bool bHitSuccessful = bIsTouch
		? GetHitResultUnderFinger(ETouchIndex::Touch1, ECollisionChannel::ECC_Visibility, true, Hit)
		: GetHitResultUnderCursor(ECollisionChannel::ECC_Visibility, true, Hit);
	if (!bHitSuccessful || Hit.Location.ContainsNaN())
	{
		return false;
	}
	CachedDestination = Hit.Location;
	return true;
}
