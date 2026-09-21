#include "Combat/AI/CombatAIRoleTasks.h"
#include "Combat/AI/CombatAIProfileData.h"
#include "StateTreeExecutionContext.h"

EStateTreeRunStatus FCombatAIPrepareRoleTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult&) const
{
	auto& Brain = Context.GetExternalData(BrainHandle);
	auto& Data = Context.GetInstanceData(*this);
	Data.Activation = Brain.AllocateActivation();
	return Brain.PrepareRoleOrder(Data.Operation, Data.ConsumerSlot, Data.Activation) ? EStateTreeRunStatus::Succeeded : EStateTreeRunStatus::Failed;
}

void FCombatAIPrepareRoleTask::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	if (Transition.CurrentRunStatus != EStateTreeRunStatus::Succeeded)
		Context.GetExternalData(BrainHandle).DiscardPrepared(Context.GetInstanceData(*this).Activation);
}

EStateTreeRunStatus FCombatAIExecuteRoleTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult&) const
{
	auto& Brain = Context.GetExternalData(BrainHandle);
	auto& Data = Context.GetInstanceData(*this);
	Data.Activation = Brain.AllocateActivation();
	if (!Brain.BeginAction(Brain.GetDecisionScope(), Data.ConsumerSlot, Data.Activation)) return EStateTreeRunStatus::Failed;
	return Brain.PollRoleAction(Data.Activation);
}

EStateTreeRunStatus FCombatAIExecuteRoleTask::Tick(FStateTreeExecutionContext& Context, float) const
{
	return Context.GetExternalData(BrainHandle).PollRoleAction(Context.GetInstanceData(*this).Activation);
}

EStateTreeRunStatus FCombatAIResolveRoleTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult&) const
{
	return Context.GetExternalData(BrainHandle).ResolveRoleReceipt() ? EStateTreeRunStatus::Succeeded : EStateTreeRunStatus::Failed;
}

EStateTreeRunStatus FCombatAIRoleWaitTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult&) const
{
	auto& Brain = Context.GetExternalData(BrainHandle);
	auto& Data = Context.GetInstanceData(*this);
	Data.Activation = Brain.AllocateActivation();
	Data.Revision = Brain.GetContextData().ObjectiveRevision;
	Data.Snapshot = Brain.GetKnowledge().Revision;
	if (Data.Wait == ECombatAIRoleWait::Retry || Data.Wait == ECombatAIRoleWait::RoutePause)
	{
		const auto* Profile = Brain.GetProfile();
		if (!Profile || !Brain.BeginWait(Data.Activation, Data.Wait == ECombatAIRoleWait::Retry ? Profile->RetryDelay : Profile->RoutePause)) return EStateTreeRunStatus::Failed;
	}
	if (Data.Wait == ECombatAIRoleWait::Assignment && Brain.HasAssignment()) return EStateTreeRunStatus::Succeeded;
	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FCombatAIRoleWaitTask::Tick(FStateTreeExecutionContext& Context, float) const
{
	auto& Brain = Context.GetExternalData(BrainHandle);
	const auto& Data = Context.GetInstanceData(*this);
	if (Brain.HasAssignment() && Brain.GetContextData().ObjectiveRevision != Data.Revision) return EStateTreeRunStatus::Succeeded;
	if (Data.Wait == ECombatAIRoleWait::Assignment && Brain.HasAssignment()) return EStateTreeRunStatus::Succeeded;
	if (Data.Wait == ECombatAIRoleWait::Decision && Brain.GetKnowledge().Revision != Data.Snapshot) return EStateTreeRunStatus::Succeeded;
	if (Brain.IsWaitComplete(Data.Activation))
	{
		if (Data.Wait == ECombatAIRoleWait::Retry) Brain.FinishRoleRetry(Data.Revision);
		return EStateTreeRunStatus::Succeeded;
	}
	return EStateTreeRunStatus::Running;
}

void FCombatAIRoleWaitTask::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult&) const
{
	Context.GetExternalData(BrainHandle).EndWait(Context.GetInstanceData(*this).Activation);
}

bool FCombatAIRoleCondition::Link(FStateTreeLinker& Linker) { Linker.LinkExternalData(BrainHandle); return true; }
bool FCombatAIRoleCondition::TestCondition(FStateTreeExecutionContext& Context) const
{
	const auto& Brain = Context.GetExternalData(BrainHandle);
	switch (Context.GetInstanceData(*this).Fact)
	{
	case ECombatAIRoleFact::NeedReturn: return Brain.NeedsReturn();
	case ECombatAIRoleFact::CanEngage: return Brain.CanEngageKnownTarget();
	case ECombatAIRoleFact::HasRoute: return Brain.HasRoutePoint();
	case ECombatAIRoleFact::RetryPending: return Brain.IsRetryPending();
	case ECombatAIRoleFact::RetryBlocked: return Brain.IsRetryBlocked();
	}
	return false;
}
