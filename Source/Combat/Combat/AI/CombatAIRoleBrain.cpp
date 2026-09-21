#include "Combat/AI/CombatAIBrainComponent.h"
#include "Combat/AI/CombatAIProfileData.h"
#include "Combat/Core/CombatTags.h"
#include "Combat/Log/CombatEventSubsystem.h"
#include "Combat/Order/CombatOrderComponent.h"
#include "Combat/Scheduling/CombatSchedulerSubsystem.h"
#include "Combat/Targeting/CombatTargetingSubsystem.h"
#include "Combat/Unit/CombatUnitCharacter.h"
#include "Engine/World.h"

namespace CombatAIRoleBrain
{
	/** 感知与受击信息许可复用完全相同的公共规则，不借伤害日志绕过 LOS。 */
	FCombatTargetingRules Rules(const UCombatAIProfileData& Profile)
	{
		FCombatTargetingRules Result;
		Result.TargetTeamTag = CombatTags::TargetTeam_Enemy;
		Result.bRequireLineOfSight = Profile.Perception.bRequireLineOfSight;
		Result.VisibilityPolicy = Profile.Perception.VisibilityPolicy;
		return Result;
	}
}

bool UCombatAIBrainComponent::SetAssignment(const FCombatAIAssignment& NewAssignment)
{
	if (GetNetMode() == NM_Client || !GetOwner() || !GetOwner()->HasAuthority() || bEnding || !NewAssignment.IsValid()) return false;
	Assignment = NewAssignment;
	bHasAssignment = true;
	++Context.ObjectiveRevision;
	RouteCursor = 0;
	LastReachedRouteIndex = INDEX_NONE;
	bReturnRequested = bRetryPending = false;
	Failures.Reset();
	SelectedTarget.Reset(); SelectedTargetLife = 0;
	Wake();
	return true;
}

void UCombatAIBrainComponent::StartPerception()
{
	if (!Profile || !Profile->bEnablePerception || !CanSubmit(ControlEpoch)) return;
	SampleKnowledge();
	if (Profile->Perception.bObserveDamageThreat)
		PerceptionBinding = GetWorld()->GetSubsystem<UCombatEventSubsystem>()->OnRecord().AddUObject(this, &ThisClass::ObserveThreat);
	SchedulePerception();
}

void UCombatAIBrainComponent::ClearPerception()
{
	if (auto* Events = GetWorld() ? GetWorld()->GetSubsystem<UCombatEventSubsystem>() : nullptr) Events->OnRecord().Remove(PerceptionBinding);
	PerceptionBinding.Reset();
	if (auto* Scheduler = GetWorld() ? GetWorld()->GetSubsystem<UCombatSchedulerSubsystem>() : nullptr) Scheduler->Cancel(PerceptionSchedule);
	PerceptionSchedule = {};
	Knowledge = {};
	SelectedTarget.Reset(); SelectedTargetLife = 0;
	RoleStopReason = ECombatAIRoleStopReason::None;
	bReturnRequested = bRetryPending = false;
	Failures.Reset();
	RouteCursor = 0;
	LastReachedRouteIndex = INDEX_NONE;
}

void UCombatAIBrainComponent::SchedulePerception()
{
	if (!Profile || !Profile->bEnablePerception || !CanSubmit(ControlEpoch)) return;
	auto* Scheduler = GetWorld()->GetSubsystem<UCombatSchedulerSubsystem>();
	const float Interval = ActionActivation && ActionIntent.Operation == ECombatAIRoleOperation::Attack
		? Profile->Perception.ActiveInterval : Profile->Perception.IdleInterval;
	const uint64 Epoch = ControlEpoch;
	const uint32 Life = Context.Unit->GetLifeGeneration();
	const auto Ticket = MakeShared<FCombatScheduleHandle>();
	*Ticket = Scheduler->ScheduleOnce(this, Interval, 0, FCombatScheduledDelegate::CreateWeakLambda(this,
		[this, Epoch, Life, Ticket](const FCombatScheduledTickContext&)
		{
			if (!CanSubmit(Epoch) || Context.Unit->GetLifeGeneration() != Life || !(PerceptionSchedule == *Ticket)) return;
			PerceptionSchedule = {};
			SampleKnowledge();
			Wake();
			SchedulePerception();
		}));
	PerceptionSchedule = *Ticket;
}

void UCombatAIBrainComponent::SampleKnowledge()
{
	if (!Profile || !Profile->bEnablePerception || !CanSubmit(ControlEpoch)) return;
	const double Now = GetWorld()->GetTimeSeconds();
	FCombatAIKnowledgeSnapshot Next;
	Next.Revision = Knowledge.Revision + 1;
	Next.ObservedAt = Now;
	Next.Memories = Knowledge.Memories;
	for (auto& Memory : Next.Memories) Memory.bVisible = false;
	const auto Seen = GetWorld()->GetSubsystem<UCombatTargetingSubsystem>()->QueryUnitsInRadius(Context.Unit,
		Context.Unit->GetActorLocation(), Profile->Perception.Radius, CombatAIRoleBrain::Rules(*Profile));
	for (auto* Target : Seen)
	{
		const uint32 Life = Target->GetLifeGeneration();
		// 身份相同但生命更新时只保留新观察；旧生命的威胁不能转移给重生目标。
		Next.Memories.RemoveAll([&](const auto& Memory) { return Memory.Unit == Target && Memory.Life != Life; });
		auto* Memory = Next.Memories.FindByPredicate([&](const auto& Item) { return Item.Unit == Target && Item.Life == Life; });
		if (!Memory) Memory = &Next.Memories.AddDefaulted_GetRef();
		Memory->Unit = Target;
		Memory->Life = Life;
		Memory->StableId = Target->GetUniqueID();
		Memory->Definition = Target->GetUnitDefinitionId();
		Memory->LastSeenPosition = Target->GetActorLocation();
		Memory->LastSeenAt = Now;
		Memory->bVisible = true;
		Memory->Priority = 0;
		for (const auto& Entry : Profile->TargetPriorities)
			if (Entry.Definition == Memory->Definition) { Memory->Priority = Entry.Priority; break; }
		Memory->Score = Memory->Threat * Profile->ThreatWeight
			- FVector::Dist2D(Context.Unit->GetActorLocation(), Memory->LastSeenPosition) * Profile->DistanceWeight;
		if (!bHasAssignment || FVector::Dist2D(GetDutyAnchor(), Memory->LastSeenPosition) <= Profile->LeashDistance)
			Next.Candidates.Add(*Memory);
	}
	FCombatAITargetSelection::Sort(Next.Candidates);
	if (Next.Candidates.Num() > Profile->Perception.MaxCandidates)
	{
		// 候选预算不是失感知。持续攻击不做普通换敌，必须保留仍有资格的当前身份。
		const int32 Active = ActionActivation && ActionIntent.Operation == ECombatAIRoleOperation::Attack
			? Next.Candidates.IndexOfByPredicate([&](const auto& Item) { return Item.Unit == ActionIntent.Target && Item.Life == ActionIntent.TargetLife; })
			: FCombatAITargetSelection::Select(Next.Candidates, SelectedTarget, SelectedTargetLife, Now - SelectedAt, Profile->MinTargetHold, Profile->TargetSwitchMargin);
		if (Active >= Profile->Perception.MaxCandidates) Next.Candidates[Profile->Perception.MaxCandidates - 1] = Next.Candidates[Active];
		Next.Candidates.SetNum(Profile->Perception.MaxCandidates);
		FCombatAITargetSelection::Sort(Next.Candidates);
	}
	Next.Memories.RemoveAll([&](const auto& Memory)
	{
		// 丢失后不读取 Actor 的实时坐标/生命/属性；弱引用失效与历史时间已经足够淘汰。
		return !Memory.Unit.IsValid() || (!Memory.bVisible && Now - Memory.LastSeenAt >= Profile->Perception.MemorySeconds);
	});
	Next.Memories.Sort([&](const auto& A, const auto& B)
	{
		const bool bA = Next.Candidates.ContainsByPredicate([&](const auto& Item) { return Item.StableId == A.StableId; });
		const bool bB = Next.Candidates.ContainsByPredicate([&](const auto& Item) { return Item.StableId == B.StableId; });
		if (bA != bB) return bA;
		if (A.LastSeenAt != B.LastSeenAt) return A.LastSeenAt > B.LastSeenAt;
		return A.StableId < B.StableId;
	});
	if (Next.Memories.Num() > Profile->Perception.MaxMemories) Next.Memories.SetNum(Profile->Perception.MaxMemories);
	Knowledge = MoveTemp(Next);
}

void UCombatAIBrainComponent::ObserveThreat(const FCombatLogRecord& Record)
{
	if (!CanSubmit(ControlEpoch) || !Profile || !Profile->Perception.bObserveDamageThreat
		|| Record.EventType != CombatTags::Event_Combat_DamageApplied || Record.AppliedAmount <= 0
		|| Record.TargetActorId != static_cast<int32>(Context.Unit->GetUniqueID())
		|| Record.UnitLifeGeneration != Context.Unit->GetLifeGeneration()) return;
	auto* Memory = Knowledge.Memories.FindByPredicate([&](const auto& Item)
		{ return Item.bVisible && Item.StableId == static_cast<uint32>(Record.SourceActorId); });
	if (!Memory || !Memory->Unit.IsValid()) return;
	const auto Seen = GetWorld()->GetSubsystem<UCombatTargetingSubsystem>()->QueryUnitsInRadius(Context.Unit,
		Context.Unit->GetActorLocation(), Profile->Perception.Radius, CombatAIRoleBrain::Rules(*Profile));
	if (!Seen.Contains(Memory->Unit.Get()) || Memory->Unit->GetLifeGeneration() != Memory->Life) return;
	Memory->Threat = FMath::Min(10000.0f, Memory->Threat + Record.AppliedAmount);
	// 下一次采样发布完整排序；事件不递归推进树，也不替换正执行的归位 Order。
	Wake();
}

FVector UCombatAIBrainComponent::GetDutyAnchor() const
{
	if (Assignment.Route.IsValidIndex(LastReachedRouteIndex)) return Assignment.Route[LastReachedRouteIndex];
	return Assignment.Home;
}

bool UCombatAIBrainComponent::NeedsReturn() const
{
	return Profile && bHasAssignment && Context.Unit && (bReturnRequested
		|| FVector::Dist2D(Context.Unit->GetActorLocation(), GetDutyAnchor()) > Profile->LeashDistance);
}

const FCombatAIKnownTarget* UCombatAIBrainComponent::SelectKnownTarget() const
{
	if (!Profile) return nullptr;
	const int32 Index = FCombatAITargetSelection::Select(Knowledge.Candidates, SelectedTarget, SelectedTargetLife,
		GetWorld()->GetTimeSeconds() - SelectedAt, Profile->MinTargetHold, Profile->TargetSwitchMargin);
	return Knowledge.Candidates.IsValidIndex(Index) ? &Knowledge.Candidates[Index] : nullptr;
}

bool UCombatAIBrainComponent::CanEngageKnownTarget() const
{
	if (!bHasAssignment || NeedsReturn() || !Profile || !Profile->bEnablePerception) return false;
	const auto* Candidate = SelectKnownTarget();
	return Candidate && Candidate->Unit.IsValid();
}

bool UCombatAIBrainComponent::HasRoutePoint() const { return bHasAssignment && Assignment.Route.IsValidIndex(RouteCursor); }
int32 UCombatAIBrainComponent::GetFailureCount(ECombatAIRoleOperation Operation) const { return Failures.FindRef(Operation); }
bool UCombatAIBrainComponent::IsRetryBlocked() const { return bRetryPending && Profile && GetFailureCount(RetryOperation) >= Profile->MaxAttempts; }
void UCombatAIBrainComponent::FinishRoleRetry(uint64 Revision) { if (Revision == Context.ObjectiveRevision) bRetryPending = false; }

bool UCombatAIBrainComponent::PrepareRoleOrder(ECombatAIRoleOperation Operation, FName Slot, uint64 Producer)
{
	FCombatOrderRequest Request;
	if (Operation == ECombatAIRoleOperation::Attack)
	{
		Request.Type = ECombatOrderType::AttackTarget;
		const auto* Candidate = SelectKnownTarget();
		if (Candidate && Candidate->Unit.IsValid() && Candidate->Life == Candidate->Unit->GetLifeGeneration())
		{
			Request.TargetUnit = Candidate->Unit.Get();
			if (SelectedTarget != Candidate->Unit || SelectedTargetLife != Candidate->Life) SelectedAt = GetWorld()->GetTimeSeconds();
			SelectedTarget = Candidate->Unit;
			SelectedTargetLife = Candidate->Life;
		}
	}
	else
	{
		Request.Type = ECombatOrderType::MoveToPoint;
		Request.bHasTargetLocation = Operation == ECombatAIRoleOperation::Home || (Operation == ECombatAIRoleOperation::Route && HasRoutePoint());
		Request.TargetLocation = Operation == ECombatAIRoleOperation::Route && HasRoutePoint() ? Assignment.Route[RouteCursor] : GetDutyAnchor();
	}
	return PrepareRequest(Scope, Slot, Producer, Request, Operation, Operation == ECombatAIRoleOperation::Route ? RouteCursor : INDEX_NONE);
}

EStateTreeRunStatus UCombatAIBrainComponent::PollRoleAction(uint64 Activation)
{
	if (Receipt.bValid && Receipt.Activation == Activation) return EStateTreeRunStatus::Succeeded;
	if (ActionActivation != Activation || !IsScopeCurrent(ActionIntent.Scope)) return EStateTreeRunStatus::Failed;
	ECombatAIRoleStopReason Reason = ECombatAIRoleStopReason::None;
	if (ActionIntent.ObjectiveRevision != Context.ObjectiveRevision) Reason = ECombatAIRoleStopReason::AssignmentChanged;
	else if (ActionIntent.Operation == ECombatAIRoleOperation::Attack)
	{
		const auto* Known = Knowledge.Memories.FindByPredicate([&](const auto& Item) { return Item.Unit == ActionIntent.Target && Item.Life == ActionIntent.TargetLife && Item.bVisible; });
		if (NeedsReturn() || (Known && FVector::Dist2D(GetDutyAnchor(), Known->LastSeenPosition) > Profile->LeashDistance)
			|| GetWorld()->GetTimeSeconds() - EngagementStartedAt >= Profile->MaxEngagementSeconds)
			Reason = ECombatAIRoleStopReason::LeashExceeded;
		else if (!Knowledge.Candidates.ContainsByPredicate([&](const auto& Item) { return Item.Unit == ActionIntent.Target && Item.Life == ActionIntent.TargetLife; }))
			Reason = ECombatAIRoleStopReason::LostPerception;
	}
	else if (ActionIntent.Operation == ECombatAIRoleOperation::Route && CanEngageKnownTarget()) Reason = ECombatAIRoleStopReason::EngageOpportunity;
	if (Reason != ECombatAIRoleStopReason::None)
	{
		RoleStopReason = Reason;
		Context.Unit->GetCombatOrderComponent()->CancelCurrentOrderIfMatches(ActionOrder, CombatTags::Order_Failure_Cancelled);
		if (Receipt.bValid) return EStateTreeRunStatus::Succeeded;
	}
	return EStateTreeRunStatus::Running;
}

bool UCombatAIBrainComponent::ResolveRoleReceipt()
{
	if (!IsScopeCurrent(Scope) || !Receipt.bValid || !(Receipt.Scope == Scope)) return false;
	const auto Completed = Receipt;
	bool bFailure = false;
	if (Completed.ObjectiveRevision == Context.ObjectiveRevision)
	{
		const bool bInterrupted = Completed.StopReason != ECombatAIRoleStopReason::None;
		if (Completed.Operation == ECombatAIRoleOperation::Attack)
		{
			const FGameplayTag Failure = Completed.Result.FailureTag;
			const bool bTargetEnded = Failure == CombatTags::Order_Failure_TargetInvalid || Failure.MatchesTag(CombatTags::Failure_Target_Invalid.GetTag().RequestDirectParent());
			bFailure = !Completed.Result.bSuccess && !bInterrupted && !bTargetEnded;
			if (!bFailure && (Profile->bReturnAfterCombat || Completed.StopReason == ECombatAIRoleStopReason::LeashExceeded || NeedsReturn())) bReturnRequested = true;
			// 排除已结束身份，防止 Order 比下一次感知更早获知死亡时立即重试同一失效快照。
			Knowledge.Candidates.RemoveAll([&](const auto& Item) { return Item.Unit == SelectedTarget && Item.Life == SelectedTargetLife; });
		}
		else if (!bInterrupted)
		{
			const FVector Goal = Completed.Operation == ECombatAIRoleOperation::Route && Assignment.Route.IsValidIndex(Completed.RouteIndex)
				? Assignment.Route[Completed.RouteIndex] : GetDutyAnchor();
			bFailure = !Completed.Result.bSuccess || FVector::Dist2D(Context.Unit->GetActorLocation(), Goal) > Profile->ArrivalTolerance
				|| (Completed.Operation == ECombatAIRoleOperation::Route && Completed.RouteIndex != RouteCursor);
			if (!bFailure && Completed.Operation == ECombatAIRoleOperation::Home)
			{
				bReturnRequested = false;
				++HomeCommitCount;
				if (Profile->bClearMemoryOnReturn) { Knowledge.Candidates.Reset(); Knowledge.Memories.Reset(); ++Knowledge.Revision; }
			}
			else if (!bFailure && Completed.Operation == ECombatAIRoleOperation::Route)
			{
				LastReachedRouteIndex = Completed.RouteIndex;
				++RouteCursor; ++RouteCommitCount;
				if (Assignment.bLoopRoute && RouteCursor == Assignment.Route.Num()) RouteCursor = 0;
			}
		}
		if (bFailure)
		{
			++Failures.FindOrAdd(Completed.Operation);
			RetryOperation = Completed.Operation;
			bRetryPending = true;
		}
		else Failures.Remove(Completed.Operation);
	}
	UE_LOG(LogTemp, Display, TEXT("AIRoleReceipt Unit=%s Operation=%d Reason=%d Revision=%llu Failed=%d Attempts=%d Cursor=%d Return=%d"),
		*GetNameSafe(GetOwner()), int32(Completed.Operation), int32(Completed.StopReason), Completed.ObjectiveRevision,
		bFailure, GetFailureCount(Completed.Operation), RouteCursor, bReturnRequested);
	ResolveReceipt(Scope);
	return !bFailure;
}
