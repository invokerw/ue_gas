#include "Combat/Network/CombatNetworkSecuritySubsystem.h"

#include "GameFramework/PlayerController.h"

#include "Combat/Core/CombatTags.h"
#include "Combat/Economy/CombatEconomyComponent.h"
#include "Combat/Log/CombatEventSubsystem.h"
#include "Combat/Unit/CombatUnitCharacter.h"

bool UCombatNetworkSecuritySubsystem::ValidateAndConsumeOrderRequest(
	APlayerController* RequestingController,
	ACombatUnitCharacter* Unit,
	const FCombatOrderBatchRequest& Request,
	FGameplayTag& OutFailureTag,
	FString& OutDiagnostic)
{
	OutFailureTag = FGameplayTag();
	OutDiagnostic.Reset();
	PruneInvalidConnections();
	if (!RequestingController || !Unit || !Unit->HasAuthority() || Unit->GetOwner() != RequestingController)
	{
		OutFailureTag = CombatTags::Failure_Network_Ownership;
		OutDiagnostic = TEXT("Requesting connection does not own the combat unit");
		RecordRejection(RequestingController, Unit, Request, OutFailureTag, OutDiagnostic);
		return false;
	}
	if (Request.RequestId <= 0)
	{
		OutFailureTag = CombatTags::Failure_Network_InvalidRequestId;
		OutDiagnostic = TEXT("RequestId must be positive");
		RecordRejection(RequestingController, Unit, Request, OutFailureTag, OutDiagnostic);
		return false;
	}
	const int32 EstimatedBytes = EstimatePayloadBytes(Request);
	if (Request.Orders.IsEmpty() || Request.Orders.Num() > MaxOrdersPerRequest
		|| EstimatedBytes > MaxEstimatedPayloadBytes)
	{
		OutFailureTag = CombatTags::Failure_Network_PayloadTooLarge;
		OutDiagnostic = FString::Printf(TEXT("Order payload rejected Count=%d EstimatedBytes=%d"),
			Request.Orders.Num(), EstimatedBytes);
		RecordRejection(RequestingController, Unit, Request, OutFailureTag, OutDiagnostic);
		return false;
	}

	if (!ConsumeConnectionBudget(RequestingController, Request.RequestId, OutFailureTag, OutDiagnostic))
	{
		RecordRejection(RequestingController, Unit, Request, OutFailureTag, OutDiagnostic);
		return false;
	}
	++Stats.AcceptedRequests;
	return true;
}

bool UCombatNetworkSecuritySubsystem::ValidateAndConsumeEconomyRequest(
	APlayerController* RequestingController, UCombatEconomyComponent* Economy,
	const FCombatEconomyRequest& Request, FGameplayTag& OutFailureTag, FString& OutDiagnostic)
{
	OutFailureTag = {};
	OutDiagnostic.Reset();
	PruneInvalidConnections();
	if (!RequestingController || !RequestingController->HasAuthority() || !Economy
		|| Economy->GetOwner() != RequestingController)
	{
		OutFailureTag = CombatTags::Failure_Network_Ownership;
		OutDiagnostic = TEXT("Requesting connection does not own the economy component");
		RecordEconomyRejection(RequestingController, Request, OutFailureTag, OutDiagnostic);
		return false;
	}
	const bool bCommonPayloadValid = Request.ExpectedEconomyRevision > 0
		&& Request.CommandBindingGeneration >= 0;
	bool bActionPayloadValid = false;
	switch (Request.Action)
	{
	case ECombatEconomyAction::Purchase:
		bActionPayloadValid = Request.ExpectedInventoryRevision > 0 && Request.ItemDefinitionId.IsValid()
			&& !Request.ItemHandle.IsValid() && Request.ItemRevision == 0;
		break;
	case ECombatEconomyAction::SellInventoryItem:
	case ECombatEconomyAction::ToggleInventoryItemLock:
		bActionPayloadValid = Request.ExpectedInventoryRevision > 0 && !Request.ItemDefinitionId.IsValid()
			&& Request.ItemHandle.IsValid() && Request.ItemRevision > 0;
		break;
	default:
		break;
	}
	if (!bCommonPayloadValid || !bActionPayloadValid)
	{
		OutFailureTag = CombatTags::Failure_Network_PayloadTooLarge;
		OutDiagnostic = TEXT("Economy request contains an invalid action payload or revision");
		RecordEconomyRejection(RequestingController, Request, OutFailureTag, OutDiagnostic);
		return false;
	}
	if (!ConsumeConnectionBudget(RequestingController, Request.RequestId, OutFailureTag, OutDiagnostic))
	{
		RecordEconomyRejection(RequestingController, Request, OutFailureTag, OutDiagnostic);
		return false;
	}
	++Stats.AcceptedRequests;
	return true;
}

bool UCombatNetworkSecuritySubsystem::ConsumeConnectionBudget(APlayerController* RequestingController,
	const int32 RequestId, FGameplayTag& OutFailureTag, FString& OutDiagnostic)
{
	if (RequestId <= 0)
	{
		OutFailureTag = CombatTags::Failure_Network_InvalidRequestId;
		OutDiagnostic = TEXT("RequestId must be positive");
		return false;
	}
	FConnectionGuardState& State = ConnectionStates.FindOrAdd(RequestingController);
	const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	if (!State.bInitialized)
	{
		State.Tokens = FMath::Max(1.0f, BurstCapacity);
		State.LastRefillTime = Now;
		State.bInitialized = true;
	}
	if (State.RecentRequestIds.Contains(RequestId))
	{
		OutFailureTag = CombatTags::Failure_Network_DuplicateRequest;
		OutDiagnostic = TEXT("RequestId is already present in the replay window");
		return false;
	}
	const double Elapsed = FMath::Max(0.0, Now - State.LastRefillTime);
	State.Tokens = FMath::Min<double>(FMath::Max(1.0f, BurstCapacity),
		State.Tokens + Elapsed * FMath::Max(1.0f, RequestsPerSecond));
	State.LastRefillTime = Now;
	if (State.Tokens < 1.0)
	{
		OutFailureTag = CombatTags::Failure_Network_RateLimited;
		OutDiagnostic = TEXT("Connection token bucket is empty");
		return false;
	}

	State.Tokens -= 1.0;
	RememberRequestId(State, RequestId);
	return true;
}

void UCombatNetworkSecuritySubsystem::Deinitialize()
{
	ConnectionStates.Reset();
	Stats = FCombatRpcSecurityStats();
	Super::Deinitialize();
}

int32 UCombatNetworkSecuritySubsystem::EstimatePayloadBytes(const FCombatOrderBatchRequest& Request)
{
	// 固定字段按对齐后的保守上界估算；实际网络序列化仍由 UE 属性系统约束。
	constexpr int32 HeaderBytes = 32;
	constexpr int32 OrderBytes = 160;
	return HeaderBytes + Request.Orders.Num() * OrderBytes;
}

void UCombatNetworkSecuritySubsystem::RememberRequestId(FConnectionGuardState& State, const int32 RequestId)
{
	State.RecentRequestIds.Add(RequestId);
	State.RequestIdOrder.Add(RequestId);
	while (State.RequestIdOrder.Num() > FMath::Max(8, ReplayWindowSize))
	{
		const int32 Removed = State.RequestIdOrder[0];
		State.RequestIdOrder.RemoveAt(0, 1, EAllowShrinking::No);
		State.RecentRequestIds.Remove(Removed);
	}
}

void UCombatNetworkSecuritySubsystem::RecordRejection(
	APlayerController* RequestingController,
	ACombatUnitCharacter* Unit,
	const FCombatOrderBatchRequest& Request,
	const FGameplayTag FailureTag,
	const FString& Diagnostic)
{
	++Stats.RejectedRequests;
	Stats.OwnershipRejects += FailureTag == CombatTags::Failure_Network_Ownership ? 1 : 0;
	Stats.RateLimitRejects += FailureTag == CombatTags::Failure_Network_RateLimited ? 1 : 0;
	Stats.PayloadRejects += FailureTag == CombatTags::Failure_Network_PayloadTooLarge ? 1 : 0;
	Stats.ReplayRejects += FailureTag == CombatTags::Failure_Network_DuplicateRequest ? 1 : 0;
	if (UCombatEventSubsystem* Events = GetWorld() ? GetWorld()->GetSubsystem<UCombatEventSubsystem>() : nullptr)
	{
		FCombatLogRecord Record;
		Record.Context = Events->CreateRootEvent();
		Record.EventType = CombatTags::Event_Combat_OrderRequestRejected;
		Record.FailureTag = FailureTag;
		Record.SourceActorId = RequestingController ? RequestingController->GetUniqueID() : 0;
		Record.TargetActorId = Unit ? Unit->GetUniqueID() : 0;
		Record.UnitLifeGeneration = Unit ? Unit->GetLifeGeneration() : 0;
		Record.RequestedAmount = static_cast<float>(Request.Orders.Num());
		Record.Diagnostic = FString::Printf(TEXT("RequestId=%d %s"), Request.RequestId, *Diagnostic);
		Events->Emit(Record);
	}
}

void UCombatNetworkSecuritySubsystem::RecordEconomyRejection(APlayerController* RequestingController,
	const FCombatEconomyRequest& Request, const FGameplayTag FailureTag, const FString& Diagnostic)
{
	++Stats.RejectedRequests;
	Stats.OwnershipRejects += FailureTag == CombatTags::Failure_Network_Ownership ? 1 : 0;
	Stats.RateLimitRejects += FailureTag == CombatTags::Failure_Network_RateLimited ? 1 : 0;
	Stats.PayloadRejects += FailureTag == CombatTags::Failure_Network_PayloadTooLarge ? 1 : 0;
	Stats.ReplayRejects += FailureTag == CombatTags::Failure_Network_DuplicateRequest ? 1 : 0;
	if (UCombatEventSubsystem* Events = GetWorld() ? GetWorld()->GetSubsystem<UCombatEventSubsystem>() : nullptr)
	{
		FCombatLogRecord Record;
		Record.Context = Events->CreateRootEvent();
		Record.EventType = CombatTags::Event_Combat_EconomyRequestRejected;
		Record.FailureTag = FailureTag;
		Record.SourceActorId = RequestingController ? RequestingController->GetUniqueID() : 0;
		Record.RequestedAmount = static_cast<float>(Request.Action);
		Record.Diagnostic = FString::Printf(TEXT("RequestId=%d %s"), Request.RequestId, *Diagnostic);
		Events->Emit(Record);
	}
}

void UCombatNetworkSecuritySubsystem::PruneInvalidConnections()
{
	for (auto It = ConnectionStates.CreateIterator(); It; ++It)
	{
		if (!It.Key().IsValid())
		{
			It.RemoveCurrent();
		}
	}
}
