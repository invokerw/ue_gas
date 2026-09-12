// Copyright Epic Games, Inc. All Rights Reserved.

#include "CombatPlayerController.h"

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
#include "Combat/Targeting/CombatTargetingSubsystem.h"
#include "Combat/Unit/CombatUnitAIController.h"
#include "Combat/Unit/CombatUnitCharacter.h"
#include "Combat.h"
#include "CombatCharacter.h"

ACombatPlayerController::ACombatPlayerController()
{
	bIsTouch = false;
	bShowMouseCursor = true;
	DefaultMouseCursor = EMouseCursor::Default;
	CommandPawnClass = ACombatCharacter::StaticClass();
}

void ACombatPlayerController::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(ACombatPlayerController, CommandedUnit, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(ACombatPlayerController, CommandBindingGeneration, COND_OwnerOnly);
}

bool ACombatPlayerController::SetCommandedUnitAuthority(ACombatUnitCharacter* NewUnit)
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
		if (ACombatPlayerController* PreviousController =
			Cast<ACombatPlayerController>(NewUnit->GetCommandingPlayerController());
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

void ACombatPlayerController::HandleCommandedUnitEndPlay(ACombatUnitCharacter* EndingUnit)
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

void ACombatPlayerController::OnPossess(APawn* InPawn)
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

void ACombatPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	CancelAttackTargeting();
	ResetDestinationInput();
	if (HasAuthority())
	{
		SetCommandedUnitAuthority(nullptr);
	}
	Super::EndPlay(EndPlayReason);
}

void ACombatPlayerController::OnRep_CommandedUnit()
{
	RefreshCommandBinding();
}

void ACombatPlayerController::OnRep_CommandBindingGeneration()
{
	RefreshCommandBinding();
}

void ACombatPlayerController::RefreshCommandBinding()
{
	CancelAttackTargeting();
	ResetDestinationInput();
	if (ACombatCharacter* CommandPawn = Cast<ACombatCharacter>(GetPawn()))
	{
		// Unit Owner 可能比 CommandedUnit 晚到；相机可以先安全观察，输入仍由 GetReadyCommandedUnit 阻止。
		CommandPawn->SetFollowTarget(CommandedUnit);
	}
}

void ACombatPlayerController::AdvanceCommandBindingGeneration()
{
	CommandBindingGeneration = CommandBindingGeneration >= MAX_int32 ? 1 : CommandBindingGeneration + 1;
	if (CommandBindingGeneration <= 0)
	{
		CommandBindingGeneration = 1;
	}
}

ACombatUnitCharacter* ACombatPlayerController::GetReadyCommandedUnit() const
{
	return IsValid(CommandedUnit) && CommandedUnit->GetCommandingPlayerController() == this
		? CommandedUnit.Get() : nullptr;
}

void ACombatPlayerController::SetupInputComponent()
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
		UE_LOG(LogCombatGame, Error, TEXT("'%s' failed to find an Enhanced Input Component"), *GetNameSafe(this));
		return;
	}
	EnhancedInputComponent->BindAction(SetDestinationClickAction, ETriggerEvent::Started, this, &ACombatPlayerController::OnInputStarted);
	EnhancedInputComponent->BindAction(SetDestinationClickAction, ETriggerEvent::Triggered, this, &ACombatPlayerController::OnSetDestinationTriggered);
	EnhancedInputComponent->BindAction(SetDestinationClickAction, ETriggerEvent::Completed, this, &ACombatPlayerController::OnSetDestinationReleased);
	EnhancedInputComponent->BindAction(SetDestinationClickAction, ETriggerEvent::Canceled, this, &ACombatPlayerController::OnSetDestinationReleased);
	EnhancedInputComponent->BindAction(SetDestinationTouchAction, ETriggerEvent::Started, this, &ACombatPlayerController::OnTouchStarted);
	EnhancedInputComponent->BindAction(SetDestinationTouchAction, ETriggerEvent::Triggered, this, &ACombatPlayerController::OnTouchTriggered);
	EnhancedInputComponent->BindAction(SetDestinationTouchAction, ETriggerEvent::Completed, this, &ACombatPlayerController::OnTouchReleased);
	EnhancedInputComponent->BindAction(SetDestinationTouchAction, ETriggerEvent::Canceled, this, &ACombatPlayerController::OnTouchReleased);
	if (AbilitySlotQAction) EnhancedInputComponent->BindAction(AbilitySlotQAction, ETriggerEvent::Started, this, &ACombatPlayerController::OnAbilitySlotQ);
	if (AbilitySlotWAction) EnhancedInputComponent->BindAction(AbilitySlotWAction, ETriggerEvent::Started, this, &ACombatPlayerController::OnAbilitySlotW);
	if (AbilitySlotEAction) EnhancedInputComponent->BindAction(AbilitySlotEAction, ETriggerEvent::Started, this, &ACombatPlayerController::OnAbilitySlotE);
	if (AbilitySlotRAction) EnhancedInputComponent->BindAction(AbilitySlotRAction, ETriggerEvent::Started, this, &ACombatPlayerController::OnAbilitySlotR);
	BindCombatCommandActions(*EnhancedInputComponent);
}

void ACombatPlayerController::BindCombatCommandActions(UEnhancedInputComponent& EnhancedInputComponent)
{
	if (AttackTargetAction) EnhancedInputComponent.BindAction(AttackTargetAction, ETriggerEvent::Started, this, &ACombatPlayerController::OnAttackTargetingStarted);
	if (ConfirmAttackTargetAction) EnhancedInputComponent.BindAction(ConfirmAttackTargetAction, ETriggerEvent::Started, this, &ACombatPlayerController::OnAttackTargetConfirmed);
	if (CancelAttackTargetAction) EnhancedInputComponent.BindAction(CancelAttackTargetAction, ETriggerEvent::Started, this, &ACombatPlayerController::CancelAttackTargeting);
	if (StopCommandAction) EnhancedInputComponent.BindAction(StopCommandAction, ETriggerEvent::Started, this, &ACombatPlayerController::OnStopCommand);
}

void ACombatPlayerController::OnInputStarted()
{
	FHitResult Hit;
	if (bIsTouch)
	{
		GetHitResultUnderFinger(ETouchIndex::Touch1, ECC_Visibility, true, Hit);
	}
	else
	{
		GetHitResultUnderCursor(ECC_Visibility, true, Hit);
	}
	BeginDestinationInput(Hit);
}

void ACombatPlayerController::BeginDestinationInput(const FHitResult& Hit)
{
	CancelAttackTargeting();
	ResetDestinationInput();
	if (!Hit.bBlockingHit || Hit.Location.ContainsNaN())
	{
		return;
	}
	// 只认射线实际点到的单位；技能的“附近目标”辅助会把地面右键误判成普攻。
	if (!bIsTouch && IssueCombatAttackOrder(Cast<ACombatUnitCharacter>(Hit.GetActor())))
	{
		return;
	}
	CachedDestination = Hit.Location;
	bHasCachedDestination = true;
	bDestinationInputActive = true;
	IssueCombatMoveOrder();
}

void ACombatPlayerController::ResetDestinationInput()
{
	bDestinationInputActive = false;
	MoveOrderRefreshElapsed = 0.0f;
	bHasCachedDestination = false;
	bHasIssuedMoveOrder = false;
}

void ACombatPlayerController::OnSetDestinationTriggered()
{
	if (!bDestinationInputActive)
	{
		return;
	}
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

void ACombatPlayerController::OnSetDestinationReleased()
{
	if (!bDestinationInputActive)
	{
		return;
	}
	UpdateCachedDestination();
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
	ResetDestinationInput();
}

void ACombatPlayerController::OnTouchStarted()
{
	bIsTouch = true;
	OnInputStarted();
}

void ACombatPlayerController::OnTouchTriggered()
{
	bIsTouch = true;
	OnSetDestinationTriggered();
}

void ACombatPlayerController::OnTouchReleased()
{
	OnSetDestinationReleased();
	bIsTouch = false;
}

void ACombatPlayerController::OnAttackTargetingStarted()
{
	ResetDestinationInput();
	bAttackTargeting = GetReadyCommandedUnit() != nullptr;
	CurrentMouseCursor = bAttackTargeting ? EMouseCursor::Crosshairs : DefaultMouseCursor.GetValue();
}

void ACombatPlayerController::OnAttackTargetConfirmed()
{
	if (!bAttackTargeting)
	{
		return;
	}
	FHitResult Hit;
	GetHitResultUnderCursor(ECC_Visibility, true, Hit);
	ConfirmAttackTarget(Hit);
}

void ACombatPlayerController::ConfirmAttackTarget(const FHitResult& Hit)
{
	if (bAttackTargeting && Hit.bBlockingHit
		&& IssueCombatAttackOrder(Cast<ACombatUnitCharacter>(Hit.GetActor())))
	{
		CancelAttackTargeting();
	}
}

void ACombatPlayerController::CancelAttackTargeting()
{
	bAttackTargeting = false;
	CurrentMouseCursor = DefaultMouseCursor;
}

void ACombatPlayerController::OnStopCommand()
{
	CancelAttackTargeting();
	ResetDestinationInput();
	FCombatOrderRequest Order;
	Order.Type = ECombatOrderType::Stop;
	SubmitCombatOrder(Order);
}

bool ACombatPlayerController::IssueCombatAttackOrder(ACombatUnitCharacter* Target)
{
	ACombatUnitCharacter* Unit = GetReadyCommandedUnit();
	UCombatTargetingSubsystem* Targeting = GetWorld() ? GetWorld()->GetSubsystem<UCombatTargetingSubsystem>() : nullptr;
	if (!Unit || !IsValid(Target) || !Targeting)
	{
		return false;
	}
	FCombatTargetingRules Rules;
	Rules.TargetTeamTag = CombatTags::TargetTeam_Enemy;
	// 输入只预选阵营和可选中状态；允许超出攻击范围的请求进入服务器追击。
	Rules.CastRange = TNumericLimits<float>::Max() * 0.5f;
	if (!Targeting->ValidateUnitTarget(Unit, Target, Rules).bValid)
	{
		return false;
	}
	FCombatOrderRequest Order;
	Order.Type = ECombatOrderType::AttackTarget;
	Order.TargetUnit = Target;
	return SubmitCombatOrder(Order);
}

bool ACombatPlayerController::SubmitCombatOrder(const FCombatOrderRequest& Order)
{
	ACombatUnitCharacter* Unit = GetReadyCommandedUnit();
	if (!Unit)
	{
		return false;
	}
	FCombatOrderBatchRequest Batch;
	Batch.RequestId = NextCombatOrderRequestId;
	Batch.bAppendToExistingQueue = false;
	Batch.Orders.Add(Order);
	NextCombatOrderRequestId = NextCombatOrderRequestId == MAX_int32 ? 1 : NextCombatOrderRequestId + 1;
	Unit->ServerIssueOrderBatch(MoveTemp(Batch));
	return true;
}

void ACombatPlayerController::OnAbilitySlotQ() { ActivateCombatAbilitySlot(0); }
void ACombatPlayerController::OnAbilitySlotW() { ActivateCombatAbilitySlot(1); }
void ACombatPlayerController::OnAbilitySlotE() { ActivateCombatAbilitySlot(2); }
void ACombatPlayerController::OnAbilitySlotR() { ActivateCombatAbilitySlot(3); }

void ACombatPlayerController::ActivateCombatAbilitySlot(const int32 SlotIndex)
{
	CancelAttackTargeting();
	ResetDestinationInput();
	ACombatUnitCharacter* Unit = GetReadyCommandedUnit();
	UCombatAbilitySystemComponent* Asc = Unit ? Unit->GetCombatAbilitySystemComponent() : nullptr;
	if (!Unit || !Asc)
	{
		UE_LOG(LogCombatGame, Warning, TEXT("Combat ability slot %d ignored: CommandedUnit is not ready"), SlotIndex + 1);
		return;
	}

	TArray<const FGameplayAbilitySpec*> SlottedAbilities;
	for (const FGameplayAbilitySpec& Spec : Asc->GetActivatableAbilities())
	{
		const UCombatGameplayAbility* CombatAbility = Cast<UCombatGameplayAbility>(Spec.Ability);
		const UCombatAbilityData* AbilityData = CombatAbility ? CombatAbility->GetAbilityData() : nullptr;
		if (!AbilityData || !AbilityData->ShouldOccupyPlayerAbilitySlot())
		{
			continue;
		}
		SlottedAbilities.Add(&Spec);
	}
	if (!SlottedAbilities.IsValidIndex(SlotIndex))
	{
		UE_LOG(LogCombatGame, Display, TEXT("Combat ability slot %d is empty"), SlotIndex + 1);
		return;
	}

	const FGameplayAbilitySpec& Spec = *SlottedAbilities[SlotIndex];
	const UCombatAbilityData* AbilityData = Asc->GetCombatAbilityData(Spec.Handle);
	if (!AbilityData)
	{
		return;
	}
	if (AbilityData->UsesAutoCastToggleInput())
	{
		// Toggle 由服务器读取当前值后原子翻转；客户端不依赖可能滞后的 HUD 投影猜测下一状态。
		Asc->ServerToggleAutoCastEnabled(Spec.Handle);
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

	SubmitCombatOrder(Order);
}

bool ACombatPlayerController::IssueCombatMoveOrder()
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
	if (!SubmitCombatOrder(Order))
	{
		return false;
	}
	LastIssuedMoveDestination = CachedDestination;
	MoveOrderRefreshElapsed = 0.0f;
	bHasIssuedMoveOrder = true;
	return true;
}

ACombatUnitCharacter* ACombatPlayerController::FindCombatUnitUnderCursor(const FVector& CursorWorldLocation) const
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

bool ACombatPlayerController::UpdateCachedDestination()
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
