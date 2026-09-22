#include "Combat/AI/CombatAITacticalTasks.h"

#include "Combat/AI/CombatAIProfileData.h"
#include "StateTreeExecutionContext.h"

EStateTreeRunStatus FCombatAIEvaluateTacticsTask::EnterState(
	FStateTreeExecutionContext& Context, const FStateTreeTransitionResult&) const
{
	auto& Brain = Context.GetExternalData(BrainHandle);
	const UCombatAIProfileData* Profile = Brain.GetProfile();
	if (!Profile || !Profile->IsTacticsEnabled())
	{
		return EStateTreeRunStatus::Failed;
	}
	Brain.EvaluateTacticalCandidates();
	return EStateTreeRunStatus::Succeeded;
}

EStateTreeRunStatus FCombatAIPrepareTacticalTask::EnterState(
	FStateTreeExecutionContext& Context, const FStateTreeTransitionResult&) const
{
	auto& Brain = Context.GetExternalData(BrainHandle);
	auto& Data = Context.GetInstanceData(*this);
	Data.Activation = Brain.AllocateActivation();
	return Brain.PrepareTacticalOrder(Data.Action, Data.ConsumerSlot, Data.Activation)
		? EStateTreeRunStatus::Succeeded : EStateTreeRunStatus::Failed;
}

void FCombatAIPrepareTacticalTask::ExitState(
	FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	if (Transition.CurrentRunStatus != EStateTreeRunStatus::Succeeded)
	{
		Context.GetExternalData(BrainHandle).DiscardPrepared(Context.GetInstanceData(*this).Activation);
	}
}

EStateTreeRunStatus FCombatAIQueryTacticalLocationTask::EnterState(
	FStateTreeExecutionContext& Context, const FStateTreeTransitionResult&) const
{
	auto& Brain = Context.GetExternalData(BrainHandle);
	auto& Data = Context.GetInstanceData(*this);
	Data.Activation = Brain.AllocateActivation();
	if (!Brain.BeginTacticalLocationQuery(Brain.GetDecisionScope(), Data.ConsumerSlot, Data.Activation))
	{
		return EStateTreeRunStatus::Failed;
	}
	return Brain.PollTacticalLocationQuery(Data.Activation);
}

EStateTreeRunStatus FCombatAIQueryTacticalLocationTask::Tick(
	FStateTreeExecutionContext& Context, float) const
{
	return Context.GetExternalData(BrainHandle).PollTacticalLocationQuery(
		Context.GetInstanceData(*this).Activation);
}

void FCombatAIQueryTacticalLocationTask::ExitState(
	FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	if (Transition.CurrentRunStatus != EStateTreeRunStatus::Succeeded)
	{
		auto& Brain = Context.GetExternalData(BrainHandle);
		const uint64 Activation = Context.GetInstanceData(*this).Activation;
		Brain.CancelTacticalLocationQuery(Activation);
		Brain.DiscardPrepared(Activation);
	}
}

EStateTreeRunStatus FCombatAIExecuteTacticalTask::EnterState(
	FStateTreeExecutionContext& Context, const FStateTreeTransitionResult&) const
{
	auto& Brain = Context.GetExternalData(BrainHandle);
	auto& Data = Context.GetInstanceData(*this);
	Data.Activation = Brain.AllocateActivation();
	if (!Brain.BeginTacticalAction(Brain.GetDecisionScope(), Data.ConsumerSlot, Data.Activation, Data.Action))
	{
		return EStateTreeRunStatus::Failed;
	}
	return Brain.PollTacticalAction(Data.Activation);
}

EStateTreeRunStatus FCombatAIExecuteTacticalTask::Tick(FStateTreeExecutionContext& Context, float) const
{
	return Context.GetExternalData(BrainHandle).PollTacticalAction(Context.GetInstanceData(*this).Activation);
}

EStateTreeRunStatus FCombatAIResolveTacticalTask::EnterState(
	FStateTreeExecutionContext& Context, const FStateTreeTransitionResult&) const
{
	return Context.GetExternalData(BrainHandle).ResolveTacticalReceipt()
		? EStateTreeRunStatus::Succeeded : EStateTreeRunStatus::Failed;
}

bool FCombatAITacticalCondition::Link(FStateTreeLinker& Linker)
{
	Linker.LinkExternalData(BrainHandle);
	return true;
}

bool FCombatAITacticalCondition::TestCondition(FStateTreeExecutionContext& Context) const
{
	return Context.GetExternalData(BrainHandle).IsTacticalActionAvailable(Context.GetInstanceData(*this).Action);
}

bool FCombatAITacticalConsideration::Link(FStateTreeLinker& Linker)
{
	Linker.LinkExternalData(BrainHandle);
	return true;
}

float FCombatAITacticalConsideration::GetScore(FStateTreeExecutionContext& Context) const
{
	return Context.GetExternalData(BrainHandle).GetTacticalActionScore(Context.GetInstanceData(*this).Action);
}
