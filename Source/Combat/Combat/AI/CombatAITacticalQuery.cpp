#include "Combat/AI/CombatAIBrainComponent.h"

#include "Combat/AI/CombatAIProfileData.h"
#include "Combat/AI/CombatAIWorldSubsystem.h"
#include "Combat/Core/CombatTags.h"
#include "Combat/Order/CombatOrderComponent.h"
#include "Combat/Scheduling/CombatSchedulerSubsystem.h"
#include "Combat/Unit/CombatUnitCharacter.h"
#include "EnvironmentQuery/EnvQuery.h"
#include "EnvironmentQuery/EnvQueryManager.h"
#include "EnvironmentQuery/EnvQueryTypes.h"

bool UCombatAIBrainComponent::IsTacticalQueryCurrent(const FCombatAITacticalQueryWork& Identity) const
{
	if (!Identity.IsValid() || !Profile || !Profile->IsTacticsEnabled() || !IsScopeCurrent(Identity.Scope)
		|| Identity.AssignmentRevision != Context.ObjectiveRevision || Identity.SelfLife != Context.Unit->GetLifeGeneration()
		|| Identity.QueryTemplate.Get() != Profile->TacticalLocationQuery || !Identity.Target.IsValid()
		|| Identity.Target->GetLifeGeneration() != Identity.TargetLife)
	{
		return false;
	}
	const FCombatAIKnownTarget* CurrentTarget = SelectKnownTarget();
	return CurrentTarget && CurrentTarget->Unit == Identity.Target && CurrentTarget->Life == Identity.TargetLife;
}

ACombatUnitCharacter* UCombatAIBrainComponent::GetTacticalQueryTarget() const
{
	return TacticalQuery.HasEngineQuery() && IsTacticalQueryCurrent(TacticalQuery)
		? TacticalQuery.Target.Get() : nullptr;
}

void UCombatAIBrainComponent::PublishTacticalLocationQueryFailure(
	const FCombatAITacticalQueryWork& Identity, const FString& Diagnostic)
{
	const FCombatAITacticalQueryWork Completed = Identity;
	if (TacticalQuery.Generation == Completed.Generation)
	{
		TacticalQuery = {};
	}
	if (auto* Scheduler = GetWorld() ? GetWorld()->GetSubsystem<UCombatSchedulerSubsystem>() : nullptr)
	{
		Scheduler->Cancel(TacticalQueryRetrySchedule);
	}
	TacticalQueryRetrySchedule = {};
	if (Completed.Token)
	{
		if (auto* Budget = GetWorld() ? GetWorld()->GetSubsystem<UCombatAIWorldSubsystem>() : nullptr)
		{
			const double Elapsed = GetWorld()
				? FMath::Max(0.0, (GetWorld()->GetTimeSeconds() - Completed.StartedAt) * 1000.0) : 0.0;
			Budget->FinishEQS(Completed.Token, ECombatAIEQSResult::Failed, Elapsed);
		}
	}
	TacticalQueryInbox = {};
	TacticalQueryInbox.Identity = Completed;
	TacticalQueryInbox.Diagnostic = Diagnostic;
	TacticalQueryInbox.bReady = true;
	Wake();
}

void UCombatAIBrainComponent::ScheduleTacticalLocationQueryRetry(const uint64 ExpectedGeneration)
{
	if (!TacticalQuery.IsValid() || TacticalQuery.Generation != ExpectedGeneration || TacticalQueryRetrySchedule.IsValid())
	{
		return;
	}
	auto* Scheduler = GetWorld() ? GetWorld()->GetSubsystem<UCombatSchedulerSubsystem>() : nullptr;
	if (!Scheduler || !Profile)
	{
		PublishTacticalLocationQueryFailure(TacticalQuery, TEXT("Tactical EQS retry scheduler is unavailable"));
		return;
	}
	const auto Ticket = MakeShared<FCombatScheduleHandle>();
	const float Spread = UCombatAIWorldSubsystem::ComputeStableInitialDelay(
		Context.Unit->GetUniqueID() ^ TacticalQuery.BudgetDeferrals, Profile->TacticalQueryRetrySeconds * 8.0f);
	*Ticket = Scheduler->ScheduleOnce(this, Profile->TacticalQueryRetrySeconds + Spread, 0,
		FCombatScheduledDelegate::CreateWeakLambda(this,
			[this, ExpectedGeneration, Ticket](const FCombatScheduledTickContext&)
			{
				if (!(TacticalQueryRetrySchedule == *Ticket)) return;
				TacticalQueryRetrySchedule = {};
				if (!TacticalQuery.IsValid() || TacticalQuery.Generation != ExpectedGeneration) return;
				TryStartTacticalLocationQuery(ExpectedGeneration);
				Wake();
			}));
	TacticalQueryRetrySchedule = *Ticket;
	if (!TacticalQueryRetrySchedule.IsValid())
	{
		PublishTacticalLocationQueryFailure(TacticalQuery, TEXT("Tactical EQS retry could not be scheduled"));
	}
}

bool UCombatAIBrainComponent::TryStartTacticalLocationQuery(const uint64 ExpectedGeneration)
{
	if (!TacticalQuery.IsValid() || TacticalQuery.Generation != ExpectedGeneration || TacticalQuery.HasEngineQuery())
	{
		return false;
	}
	if (!IsTacticalQueryCurrent(TacticalQuery))
	{
		PublishTacticalLocationQueryFailure(TacticalQuery, TEXT("Tactical EQS identity became stale before launch"));
		return true;
	}
	auto* Budget = GetWorld()->GetSubsystem<UCombatAIWorldSubsystem>();
	if (!Budget)
	{
		PublishTacticalLocationQueryFailure(TacticalQuery, TEXT("Tactical EQS world budget is unavailable"));
		return true;
	}
	const uint64 Token = Budget->AllocateEQSQueryToken();
	if (!Budget->TryStartEQS(Token))
	{
		++TacticalQuery.BudgetDeferrals;
		ScheduleTacticalLocationQueryRetry(ExpectedGeneration);
		return true;
	}

	TacticalQuery.Token = Token;
	TacticalQuery.StartedAt = GetWorld()->GetTimeSeconds();
	FEnvQueryRequest Request(TacticalQuery.QueryTemplate.Get(), Context.Unit);
	const FQueryFinishedSignature Finished = FQueryFinishedSignature::CreateUObject(
		this, &UCombatAIBrainComponent::OnTacticalLocationQueryFinished, ExpectedGeneration);
	const int32 QueryId = Request.Execute(EEnvQueryRunMode::SingleResult, Finished);
	// Manager 会先分配 QueryId 再排队执行；回调只在下方发布完整引擎身份后才有资格消费工作区。
	if (!TacticalQuery.IsValid() || TacticalQuery.Generation != ExpectedGeneration)
	{
		return true;
	}
	if (QueryId == INDEX_NONE)
	{
		PublishTacticalLocationQueryFailure(TacticalQuery, TEXT("Tactical EQS could not be started"));
		return true;
	}
	TacticalQuery.QueryId = QueryId;
	return true;
}

bool UCombatAIBrainComponent::BeginTacticalLocationQuery(
	const FCombatAIDecisionScope Expected, const FName ConsumerSlot, const uint64 Activation)
{
	if (TacticalQuery.IsValid())
	{
		return TacticalQuery.Activation == Activation;
	}
	if (TacticalQueryInbox.bReady || !Activation || ConsumerSlot.IsNone() || !IsScopeCurrent(Expected)
		|| ActionActivation || Prepared.Preparation || Receipt.bValid || !IsTacticalActionAvailable(ECombatAITacticalAction::Reposition))
	{
		FCombatOrderResult Result;
		Result.Type = ECombatOrderType::MoveToPoint;
		Result.State = ECombatOrderState::Failed;
		Result.FailureTag = CombatTags::Order_Failure_InvalidRequest;
		Result.Diagnostic = TEXT("Tactical EQS preparation rejected by current scope, target or configuration");
		RecordReceipt(Result, Activation, Context.ObjectiveRevision, 0);
		Receipt.Operation = ECombatAIRoleOperation::Reposition;
		return false;
	}

	const FCombatAIKnownTarget* Target = SelectKnownTarget();
	if (!Target || !Target->Unit.IsValid())
	{
		FCombatOrderResult Result;
		Result.Type = ECombatOrderType::MoveToPoint;
		Result.State = ECombatOrderState::Failed;
		Result.FailureTag = CombatTags::Order_Failure_TargetInvalid;
		Result.Diagnostic = TEXT("Tactical EQS target disappeared before launch");
		RecordReceipt(Result, Activation, Context.ObjectiveRevision, 0);
		Receipt.Operation = ECombatAIRoleOperation::Reposition;
		RepositionFailureAssignmentRevision = Context.ObjectiveRevision;
		RepositionFailureSnapshotRevision = Knowledge.Revision;
		return false;
	}
	TacticalQuery = {};
	TacticalQuery.Scope = Expected;
	TacticalQuery.Generation = ++NextTacticalQueryGeneration;
	if (TacticalQuery.Generation == 0) TacticalQuery.Generation = ++NextTacticalQueryGeneration;
	TacticalQuery.Activation = Activation;
	TacticalQuery.AssignmentRevision = Context.ObjectiveRevision;
	TacticalQuery.SnapshotRevision = Knowledge.Revision;
	TacticalQuery.ConsumerSlot = ConsumerSlot;
	TacticalQuery.SelfLife = Context.Unit->GetLifeGeneration();
	TacticalQuery.Target = Target->Unit;
	TacticalQuery.TargetLife = Target->Life;
	TacticalQuery.QueryTemplate = Profile->TacticalLocationQuery.Get();
	return TryStartTacticalLocationQuery(TacticalQuery.Generation);
}

void UCombatAIBrainComponent::OnTacticalLocationQueryFinished(
	TSharedPtr<FEnvQueryResult> Result, const uint64 ExpectedGeneration)
{
	if (!Result.IsValid() || !TacticalQuery.IsValid()
		|| TacticalQuery.Generation != ExpectedGeneration || !TacticalQuery.HasEngineQuery()
		|| TacticalQuery.QueryId != Result->QueryID)
	{
		return;
	}
	FCombatAITacticalQueryWork Completed = TacticalQuery;
	Completed.QueryId = Result->QueryID;
	const bool bIdentityCurrent = IsTacticalQueryCurrent(Completed);
	const FVector Location = Result->IsSuccessful() && Result->Items.IsValidIndex(0)
		? Result->GetItemAsLocation(0) : FVector::ZeroVector;
	const bool bSuccess = bIdentityCurrent && Result->IsSuccessful()
		&& Result->Items.IsValidIndex(0) && !Location.ContainsNaN();
	TacticalQuery = {};

	if (auto* Budget = GetWorld() ? GetWorld()->GetSubsystem<UCombatAIWorldSubsystem>() : nullptr)
	{
		const double Elapsed = GetWorld()
			? FMath::Max(0.0, (GetWorld()->GetTimeSeconds() - Completed.StartedAt) * 1000.0) : 0.0;
		Budget->FinishEQS(Completed.Token, bIdentityCurrent
			? (bSuccess ? ECombatAIEQSResult::Succeeded : ECombatAIEQSResult::Failed)
			: ECombatAIEQSResult::Stale, Elapsed);
	}
	TacticalQueryInbox = {};
	TacticalQueryInbox.Identity = Completed;
	TacticalQueryInbox.Location = Location;
	TacticalQueryInbox.Diagnostic = bIdentityCurrent
		? (bSuccess ? FString() : TEXT("Tactical EQS returned no finite location"))
		: TEXT("Tactical EQS result was stale");
	TacticalQueryInbox.bReady = true;
	TacticalQueryInbox.bSuccess = bSuccess;
	Wake();
}

EStateTreeRunStatus UCombatAIBrainComponent::PollTacticalLocationQuery(const uint64 Activation)
{
	if (TacticalQueryInbox.bReady && TacticalQueryInbox.Identity.Activation == Activation)
	{
		const FCombatAITacticalQueryInbox Completed = TacticalQueryInbox;
		TacticalQueryInbox = {};
		if (!Completed.bSuccess || !IsTacticalQueryCurrent(Completed.Identity))
		{
			RepositionFailureAssignmentRevision = Context.ObjectiveRevision;
			RepositionFailureSnapshotRevision = Knowledge.Revision;
			FCombatOrderResult Result;
			Result.Type = ECombatOrderType::MoveToPoint;
			Result.State = ECombatOrderState::Failed;
			Result.FailureTag = CombatTags::Order_Failure_InvalidRequest;
			Result.Diagnostic = Completed.Diagnostic.IsEmpty()
				? TEXT("Tactical EQS result was rejected") : Completed.Diagnostic;
			RecordReceipt(Result, Activation, Completed.Identity.AssignmentRevision, 0);
			Receipt.Operation = ECombatAIRoleOperation::Reposition;
			return EStateTreeRunStatus::Failed;
		}
		FCombatOrderRequest Request;
		Request.Type = ECombatOrderType::MoveToPoint;
		Request.TargetLocation = Completed.Location;
		Request.bHasTargetLocation = true;
		if (PrepareRequest(Completed.Identity.Scope, Completed.Identity.ConsumerSlot, Activation, Request,
			ECombatAIRoleOperation::Reposition))
		{
			return EStateTreeRunStatus::Succeeded;
		}
		RepositionFailureAssignmentRevision = Context.ObjectiveRevision;
		RepositionFailureSnapshotRevision = Knowledge.Revision;
		return EStateTreeRunStatus::Failed;
	}
	return TacticalQuery.IsValid() && TacticalQuery.Activation == Activation
		? EStateTreeRunStatus::Running : EStateTreeRunStatus::Failed;
}

void UCombatAIBrainComponent::CancelTacticalLocationQuery(const uint64 Activation)
{
	if (TacticalQuery.IsValid() && TacticalQuery.Activation == Activation)
	{
		ClearTacticalLocationQuery();
	}
	else if (TacticalQueryInbox.bReady && TacticalQueryInbox.Identity.Activation == Activation)
	{
		TacticalQueryInbox = {};
	}
}

void UCombatAIBrainComponent::ClearTacticalLocationQuery()
{
	if (auto* Scheduler = GetWorld() ? GetWorld()->GetSubsystem<UCombatSchedulerSubsystem>() : nullptr)
	{
		Scheduler->Cancel(TacticalQueryRetrySchedule);
	}
	TacticalQueryRetrySchedule = {};
	const FCombatAITacticalQueryWork Cancelled = TacticalQuery;
	TacticalQuery = {};
	TacticalQueryInbox = {};
	if (!Cancelled.IsValid()) return;
	if (Cancelled.Token)
	{
		if (auto* Budget = GetWorld() ? GetWorld()->GetSubsystem<UCombatAIWorldSubsystem>() : nullptr)
		{
			const double Elapsed = GetWorld()
				? FMath::Max(0.0, (GetWorld()->GetTimeSeconds() - Cancelled.StartedAt) * 1000.0) : 0.0;
			Budget->FinishEQS(Cancelled.Token, ECombatAIEQSResult::Cancelled, Elapsed);
		}
	}
	if (Cancelled.HasEngineQuery())
	{
		if (auto* Manager = UEnvQueryManager::GetCurrent(GetWorld()))
		{
			Manager->AbortQuery(Cancelled.QueryId);
		}
	}
}
