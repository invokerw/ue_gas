// Copyright Epic Games, Inc. All Rights Reserved.

#include "CombatPlayerController.h"
#include "Combat/Items/CombatWorldItem.h"
#include "Combat/Items/CombatInventoryComponent.h"
#include "Combat/Economy/CombatEconomyComponent.h"
#include "Combat/View/CombatUnitViewComponent.h"
#include "Combat/UI/CombatAbilityIndicatorGround.h"

#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
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
#include "Combat/Log/CombatLogComponent.h"
#include "Combat/Network/CombatNetworkTypes.h"
#include "Combat/Network/CombatNetworkSecuritySubsystem.h"
#include "Combat/Order/CombatOrderComponent.h"
#include "Combat/Targeting/CombatTargetingSubsystem.h"
#include "Combat/UI/CombatPlayerHUD.h"
#include "Combat/UI/CombatHUDWidget.h"
#include "Combat/UI/CombatLogWidget.h"
#include "Combat/UI/CombatShopWidget.h"
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
	CombatLogComponent = CreateDefaultSubobject<UCombatLogComponent>(TEXT("CombatLog"));
	CombatEconomyComponent = CreateDefaultSubobject<UCombatEconomyComponent>(TEXT("CombatEconomy"));
	AbilityAimComponent = CreateDefaultSubobject<UCombatAbilityAimComponent>(TEXT("AbilityAim"));
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
	if (CombatEconomyComponent) CombatEconomyComponent->RefreshInventoryProjection();

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
	if (CombatEconomyComponent) CombatEconomyComponent->RefreshInventoryProjection();
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
	CancelCombatTargeting();
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
	if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(PendingHUDAbilityTimer);
	ItemFeedbackRequest = 0;
	ItemFeedbackText = FText::GetEmpty();
	if (AbilityAimComponent) AbilityAimComponent->ResetLocalState();
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
	EnhancedInputComponent->BindAction(SetDestinationClickAction, ETriggerEvent::Canceled, this, &ACombatPlayerController::ResetDestinationInput);
	EnhancedInputComponent->BindAction(SetDestinationTouchAction, ETriggerEvent::Started, this, &ACombatPlayerController::OnTouchStarted);
	EnhancedInputComponent->BindAction(SetDestinationTouchAction, ETriggerEvent::Triggered, this, &ACombatPlayerController::OnTouchTriggered);
	EnhancedInputComponent->BindAction(SetDestinationTouchAction, ETriggerEvent::Completed, this, &ACombatPlayerController::OnTouchReleased);
	EnhancedInputComponent->BindAction(SetDestinationTouchAction, ETriggerEvent::Canceled, this, &ACombatPlayerController::ResetDestinationInput);
	if (AbilitySlotQAction) EnhancedInputComponent->BindAction(AbilitySlotQAction, ETriggerEvent::Started, this, &ACombatPlayerController::OnAbilitySlotQ);
	if (AbilitySlotWAction) EnhancedInputComponent->BindAction(AbilitySlotWAction, ETriggerEvent::Started, this, &ACombatPlayerController::OnAbilitySlotW);
	if (AbilitySlotEAction) EnhancedInputComponent->BindAction(AbilitySlotEAction, ETriggerEvent::Started, this, &ACombatPlayerController::OnAbilitySlotE);
	if (AbilitySlotRAction) EnhancedInputComponent->BindAction(AbilitySlotRAction, ETriggerEvent::Started, this, &ACombatPlayerController::OnAbilitySlotR);
	const UInputAction* SlotActions[] = { AbilitySlotQAction, AbilitySlotWAction, AbilitySlotEAction, AbilitySlotRAction };
	for (int32 Slot = 0; Slot < 4; ++Slot)
	{
		if (!SlotActions[Slot]) continue;
		EnhancedInputComponent->BindAction(SlotActions[Slot], ETriggerEvent::Completed, this, &ACombatPlayerController::OnAbilitySlotReleased, Slot);
		EnhancedInputComponent->BindAction(SlotActions[Slot], ETriggerEvent::Canceled, this, &ACombatPlayerController::OnAbilityInputCanceled, Slot);
	}
	BindCombatCommandActions(*EnhancedInputComponent);
	for (int32 Slot = 0; Slot < FMath::Min(6, ItemSlotActions.Num()); ++Slot)
	{
		if (!ItemSlotActions[Slot]) continue;
		EnhancedInputComponent->BindAction(ItemSlotActions[Slot], ETriggerEvent::Started, this, &ACombatPlayerController::OnItemSlotPressed, Slot);
		EnhancedInputComponent->BindAction(ItemSlotActions[Slot], ETriggerEvent::Completed, this, &ACombatPlayerController::OnItemSlotReleased, Slot);
		EnhancedInputComponent->BindAction(ItemSlotActions[Slot], ETriggerEvent::Canceled, this, &ACombatPlayerController::OnItemInputCanceled, Slot);
	}
}

void ACombatPlayerController::BindCombatCommandActions(UEnhancedInputComponent& EnhancedInputComponent)
{
	if (AttackTargetAction) EnhancedInputComponent.BindAction(AttackTargetAction, ETriggerEvent::Started, this, &ACombatPlayerController::OnAttackTargetingStarted);
	if (ConfirmAttackTargetAction) EnhancedInputComponent.BindAction(ConfirmAttackTargetAction, ETriggerEvent::Started, this, &ACombatPlayerController::OnAttackTargetConfirmed);
	if (CancelAttackTargetAction) EnhancedInputComponent.BindAction(CancelAttackTargetAction, ETriggerEvent::Started, this, &ACombatPlayerController::CancelCombatTargeting);
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
	if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(PendingHUDAbilityTimer);
	const bool bWasAbilityAiming = AbilityAimComponent && AbilityAimComponent->IsAiming();
	if (bWasAbilityAiming) AbilityAimComponent->ResetLocalState();
	CancelAttackTargeting();
	ResetDestinationInput();
	// 右键取消消费整个手势，后续 Triggered/Completed 没有可恢复的移动目标。
	if (bWasAbilityAiming || IsPointerOverCombatUI()) return;
	if (!Hit.bBlockingHit || Hit.Location.ContainsNaN())
	{
		return;
	}
	// 只认射线实际点到的单位；技能的“附近目标”辅助会把地面右键误判成普攻。
	if (!bIsTouch)
	{
		if (const ACombatWorldItem* Item = Cast<ACombatWorldItem>(Hit.GetActor()))
		{
			FCombatOrderRequest Order;
			Order.Type = ECombatOrderType::PickupItem;
			Order.ItemHandle = Item->GetItemHandle();
			Order.ItemRevision = Item->GetItemRevision();
			if (Order.ItemHandle.IsValid()) SubmitCombatOrder(Order);
			return;
		}
	}
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
	if (IsPointerOverCombatUI()) { ResetDestinationInput(); return; }
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
	if (IsPointerOverCombatUI()) { ResetDestinationInput(); return; }
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
	if (AbilityAimComponent) AbilityAimComponent->ResetLocalState();
	ResetDestinationInput();
	bAttackTargeting = GetReadyCommandedUnit() != nullptr;
	CurrentMouseCursor = bAttackTargeting ? EMouseCursor::Crosshairs : DefaultMouseCursor.GetValue();
}

void ACombatPlayerController::OnAttackTargetConfirmed()
{
	if (AbilityAimComponent && AbilityAimComponent->IsAiming())
	{
		FHitResult Hit;
		AbilityAimComponent->TraceAimHit(Hit);
		ConfirmAbilityTarget(Hit, AbilityAimComponent->GetSessionSerial());
		return;
	}
	if (IsPointerOverCombatUI()) return;
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
	if (AbilityAimComponent) AbilityAimComponent->ResetLocalState();
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
	Batch.RequestId = AllocateCombatRequestId();
	Batch.UnitLifeGeneration = Unit->GetLifeGeneration();
	Batch.CommandBindingGeneration = GetCommandBindingGeneration();
	Batch.bAppendToExistingQueue = false;
	Batch.Orders.Add(Order);
	if (Order.ItemHandle.IsValid()) TrackItemRequest(Batch.RequestId, Order);
	Unit->ServerIssueOrderBatch(MoveTemp(Batch));
	return true;
}

int32 ACombatPlayerController::AllocateCombatRequestId()
{
	const int32 Result = NextCombatOrderRequestId;
	NextCombatOrderRequestId = NextCombatOrderRequestId == MAX_int32 ? 1 : NextCombatOrderRequestId + 1;
	return Result;
}

bool ACombatPlayerController::PurchaseShopItem(const FPrimaryAssetId ItemDefinitionId)
{
	if (!IsLocalController() || !CombatEconomyComponent || !ItemDefinitionId.IsValid()) return false;
	FCombatEconomyRequest Request;
	Request.Action = ECombatEconomyAction::Purchase;
	Request.ItemDefinitionId = ItemDefinitionId;
	return SubmitEconomyRequest(MoveTemp(Request));
}

bool ACombatPlayerController::SellInventoryItem(const FCombatItemView& ExpectedItem)
{
	if (!IsLocalController() || !CombatEconomyComponent || !ExpectedItem.Handle.IsValid()) return false;
	FCombatEconomyRequest Request;
	Request.Action = ECombatEconomyAction::SellInventoryItem;
	Request.ItemHandle = ExpectedItem.Handle;
	Request.ItemRevision = ExpectedItem.Revision;
	return SubmitEconomyRequest(MoveTemp(Request));
}

bool ACombatPlayerController::ToggleInventoryItemLock(const FCombatItemView& ExpectedItem)
{
	if (!IsLocalController() || !CombatEconomyComponent || !ExpectedItem.Handle.IsValid()) return false;
	FCombatEconomyRequest Request;
	Request.Action = ECombatEconomyAction::ToggleInventoryItemLock;
	Request.ItemHandle = ExpectedItem.Handle;
	Request.ItemRevision = ExpectedItem.Revision;
	return SubmitEconomyRequest(MoveTemp(Request));
}

bool ACombatPlayerController::SubmitEconomyRequest(FCombatEconomyRequest Request)
{
	if (!CombatEconomyComponent || !CombatEconomyComponent->IsInitialized()) return false;
	Request.RequestId = AllocateCombatRequestId();
	Request.CommandBindingGeneration = CommandBindingGeneration;
	Request.ExpectedEconomyRevision = CombatEconomyComponent->GetEconomyRevision();
	// 经济投影在自动合成结束后才广播最终库存修订；不要读取客户端 Unit 组件的旧副本覆盖它。
	Request.ExpectedInventoryRevision = CombatEconomyComponent->GetInventoryRevision();
	ServerSubmitEconomyRequest(MoveTemp(Request));
	return true;
}

void ACombatPlayerController::ServerSubmitEconomyRequest_Implementation(FCombatEconomyRequest Request)
{
	ClientReceiveEconomyResult(ProcessEconomyRequestForConnection(this, Request));
}

FCombatEconomyResult ACombatPlayerController::ProcessEconomyRequestForConnection(
	APlayerController* RequestingController, const FCombatEconomyRequest& Request)
{
	FCombatEconomyResult Result;
	Result.RequestId = Request.RequestId;
	Result.CommandBindingGeneration = Request.CommandBindingGeneration;
	Result.ItemDefinitionId = Request.ItemDefinitionId;
	if (CombatEconomyComponent)
	{
		Result.EconomyRevision = CombatEconomyComponent->GetEconomyRevision();
		Result.InventoryRevision = CombatEconomyComponent->GetInventoryRevision();
	}
	UCombatNetworkSecuritySubsystem* Security = GetWorld()
		? GetWorld()->GetSubsystem<UCombatNetworkSecuritySubsystem>() : nullptr;
	FString Diagnostic;
	if (!Security || !Security->ValidateAndConsumeEconomyRequest(
		RequestingController, CombatEconomyComponent, Request, Result.FailureTag, Diagnostic))
	{
		if (!Result.FailureTag.IsValid()) Result.FailureTag = CombatTags::Failure_ActionUnsupported;
		return Result;
	}
	if (Request.CommandBindingGeneration != CommandBindingGeneration)
	{
		Result.FailureTag = CombatTags::Failure_Economy_Stale;
		return Result;
	}
	if (!CombatEconomyComponent)
	{
		Result.FailureTag = CombatTags::Failure_Economy_Uninitialized;
		return Result;
	}

	const int64 BeforeGold = CombatEconomyComponent->GetGold();
	switch (Request.Action)
	{
	case ECombatEconomyAction::Purchase:
		Result.bSuccess = CombatEconomyComponent->PurchaseItem(Request.ItemDefinitionId,
			Request.ExpectedEconomyRevision, Request.ExpectedInventoryRevision, Result.ItemHandle, Result.FailureTag);
		break;
	case ECombatEconomyAction::SellInventoryItem:
		Result.ItemHandle = Request.ItemHandle;
		Result.bSuccess = CombatEconomyComponent->SellInventoryItem(Request.ItemHandle, Request.ItemRevision,
			Request.ExpectedEconomyRevision, Request.ExpectedInventoryRevision, Result.FailureTag);
		break;
	case ECombatEconomyAction::ToggleInventoryItemLock:
		Result.ItemHandle = Request.ItemHandle;
		Result.bSuccess = CombatEconomyComponent->ToggleInventoryItemLock(Request.ItemHandle, Request.ItemRevision,
			Request.ExpectedEconomyRevision, Request.ExpectedInventoryRevision, Result.bLocked, Result.FailureTag);
		break;
	default:
		Result.FailureTag = CombatTags::Failure_ActionUnsupported;
		break;
	}
	Result.GoldDelta = CombatEconomyComponent->GetGold() - BeforeGold;
	Result.EconomyRevision = CombatEconomyComponent->GetEconomyRevision();
	Result.InventoryRevision = CombatEconomyComponent->GetInventoryRevision();
	return Result;
}

void ACombatPlayerController::ClientReceiveEconomyResult_Implementation(FCombatEconomyResult Result)
{
	if (Result.CommandBindingGeneration != CommandBindingGeneration) return;
	LastEconomyResult = Result;
	OnEconomyResult.Broadcast(Result);
}

void ACombatPlayerController::OnAbilitySlotQ() { ActivateCombatAbilitySlot(0); }
void ACombatPlayerController::OnAbilitySlotW() { ActivateCombatAbilitySlot(1); }
void ACombatPlayerController::OnAbilitySlotE() { ActivateCombatAbilitySlot(2); }
void ACombatPlayerController::OnAbilitySlotR() { ActivateCombatAbilitySlot(3); }

void ACombatPlayerController::ActivateCombatAbilitySlotFromHUD(const int32 SlotIndex)
{
	if (!IsLocalController() || SlotIndex < 0 || SlotIndex >= 4)
	{
		return;
	}
	if (!GetWorld())
	{
		ActivateCombatAbilitySlot(SlotIndex);
		return;
	}
	GetWorld()->GetTimerManager().ClearTimer(PendingHUDAbilityTimer);
	TWeakObjectPtr<ACombatPlayerController> WeakThis(this);
	PendingHUDAbilityTimer = GetWorld()->GetTimerManager().SetTimerForNextTick(
		FTimerDelegate::CreateWeakLambda(this, [WeakThis, SlotIndex]()
		{
			if (ACombatPlayerController* PC = WeakThis.Get())
			{
				PC->ActivateCombatAbilitySlot(SlotIndex);
			}
		}));
}

void ACombatPlayerController::ActivateCombatAbilitySlot(const int32 SlotIndex)
{
	CancelAttackTargeting();
	ResetDestinationInput();
	AbilityAimComponent->CancelAim();
	ACombatUnitCharacter* Unit = GetReadyCommandedUnit();
	UCombatAbilitySystemComponent* Asc = Unit ? Unit->GetCombatAbilitySystemComponent() : nullptr;
	if (!Unit || !Asc)
	{
		UE_LOG(LogCombatGame, Warning, TEXT("Combat ability slot %d ignored: CommandedUnit is not ready"), SlotIndex + 1);
		return;
	}

	const FGameplayAbilitySpec* Spec = UCombatAbilityAimComponent::ResolveSlot(Unit, SlotIndex);
	if (!Spec)
	{
		UE_LOG(LogCombatGame, Display, TEXT("Combat ability slot %d is empty"), SlotIndex + 1);
		return;
	}

	const UCombatAbilityData* AbilityData = Asc->GetCombatAbilityData(Spec->Handle);
	if (!AbilityData)
	{
		return;
	}
	if (AbilityData->UsesAutoCastToggleInput())
	{
		// Toggle 由服务器读取当前值后原子翻转；客户端不依赖可能滞后的 HUD 投影猜测下一状态。
		Asc->ServerToggleAutoCastEnabled(Spec->Handle);
		return;
	}

	if (AbilityData->BehaviorTags.HasTagExact(CombatTags::Ability_Behavior_NoTarget))
	{
		FCombatOrderRequest Order;
		Order.AbilitySpecHandle = Spec->Handle;
		Order.Type = ECombatOrderType::CastNoTarget;
		SubmitCombatOrder(Order);
		return;
	}
	if (!AbilityAimComponent->BeginAim(SlotIndex)) return;
	AbilityPressSerials[SlotIndex] = AbilityAimComponent->GetSessionSerial();
	FHitResult Hit;
	AbilityAimComponent->TraceAimHit(Hit);
	AbilityAimComponent->UpdatePreview(Hit, !IsPointerOverCombatUI());
	if (AbilityCastMode == ECombatAbilityCastMode::QuickPress)
	{
		ConfirmAbilityTarget(Hit, AbilityPressSerials[SlotIndex]);
		if (AbilityAimComponent->IsAiming()) AbilityAimComponent->FinishQuickCastAttempt();
	}
}

void ACombatPlayerController::OnAbilitySlotReleased(const int32 SlotIndex)
{
	if (SlotIndex < 0 || SlotIndex >= 4) return;
	const uint64 Serial = AbilityPressSerials[SlotIndex];
	AbilityPressSerials[SlotIndex] = 0;
	if (AbilityCastMode != ECombatAbilityCastMode::QuickRelease || !Serial
		|| !AbilityAimComponent->IsAiming() || AbilityAimComponent->GetActiveSlot() != SlotIndex
		|| AbilityAimComponent->GetSessionSerial() != Serial) return;
	FHitResult Hit;
	AbilityAimComponent->TraceAimHit(Hit);
	ConfirmAbilityTarget(Hit, Serial);
	if (AbilityAimComponent->IsAiming()) AbilityAimComponent->FinishQuickCastAttempt();
}

void ACombatPlayerController::OnAbilityInputCanceled(const int32 SlotIndex)
{
	if (SlotIndex < 0 || SlotIndex >= 4) return;
	if (AbilityAimComponent->IsAiming() && AbilityAimComponent->GetActiveSlot() == SlotIndex
		&& AbilityAimComponent->GetSessionSerial() == AbilityPressSerials[SlotIndex]) AbilityAimComponent->CancelAim();
	AbilityPressSerials[SlotIndex] = 0;
}

void ACombatPlayerController::ConfirmAbilityTarget(const FHitResult& Hit, const uint64 Serial)
{
	FCombatOrderRequest Order;
	if (!GetReadyCommandedUnit() || !AbilityAimComponent->BuildConfirmedOrder(Serial, Hit, !IsPointerOverCombatUI(), Order)) return;
	AbilityAimComponent->MarkSubmitted(NextCombatOrderRequestId);
	SubmitCombatOrder(Order);
}

void ACombatPlayerController::CancelCombatTargeting()
{
	if (!bFlushingPressedKeys && GetWorld()) GetWorld()->GetTimerManager().ClearTimer(PendingHUDAbilityTimer);
	for (uint64& Serial : ItemPressSerials) Serial = 0;
	CancelAttackTargeting();
	ResetDestinationInput();
	if (AbilityAimComponent) AbilityAimComponent->ResetLocalState();
	for (uint64& Serial : AbilityPressSerials) Serial = 0;
}

void ACombatPlayerController::FlushPressedKeys()
{
	bFlushingPressedKeys = true;
	CancelCombatTargeting();
	bFlushingPressedKeys = false;
	Super::FlushPressedKeys();
}

bool ACombatPlayerController::IsPointerOverCombatUI() const
{
	const ACombatPlayerHUD* HUD = Cast<ACombatPlayerHUD>(GetHUD());
	if (!HUD || !FSlateApplication::IsInitialized()) return false;
	const FVector2D Cursor = FSlateApplication::Get().GetCursorPos();
	return (HUD->GetCombatWidget() && HUD->GetCombatWidget()->IsScreenPositionOverUI(Cursor))
		|| (HUD->GetLogWidget() && HUD->GetLogWidget()->IsScreenPositionOverUI(Cursor))
		|| (HUD->GetShopWidget() && HUD->GetShopWidget()->IsScreenPositionOverUI(Cursor));
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
