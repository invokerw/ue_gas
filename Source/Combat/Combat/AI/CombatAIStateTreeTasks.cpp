#include "Combat/AI/CombatAIStateTreeTasks.h"
#include "StateTreeExecutionContext.h"

FCombatAITaskBase::FCombatAITaskBase()
{
	bShouldCallTick = false;
	bShouldCallTickOnlyOnEvents = true;
	bShouldStateChangeOnReselect = false;
	bShouldCopyBoundPropertiesOnTick = false;
	bShouldCopyBoundPropertiesOnExitState = false;
}

bool FCombatAITaskBase::Link(FStateTreeLinker& Linker) { Linker.LinkExternalData(BrainHandle); return true; }

FCombatAIDecisionScopeTask::FCombatAIDecisionScopeTask()
{
	bShouldCallTickOnlyOnEvents = false;
	bConsideredForScheduling = false;
#if WITH_EDITORONLY_DATA
	bConsideredForCompletion = false;
	bCanEditConsideredForCompletion = false;
#endif
}

EStateTreeRunStatus FCombatAIDecisionScopeTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult&) const
{
	auto& Data = Context.GetInstanceData(*this);
	Data.OwnedScope = Context.GetExternalData(BrainHandle).BeginDecisionScope();
	return Data.OwnedScope.IsValid() ? EStateTreeRunStatus::Running : EStateTreeRunStatus::Failed;
}

void FCombatAIDecisionScopeTask::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult&) const
{
	Context.GetExternalData(BrainHandle).EndDecisionScope(Context.GetInstanceData(*this).OwnedScope);
}

EStateTreeRunStatus FCombatAIWaitObjectiveTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult&) const { return Tick(Context, 0); }
EStateTreeRunStatus FCombatAIWaitObjectiveTask::Tick(FStateTreeExecutionContext& Context, float) const
{
	return Context.GetExternalData(BrainHandle).HasPendingObjective() ? EStateTreeRunStatus::Succeeded : EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FCombatAIPrepareOrderTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult&) const
{
	auto& Brain = Context.GetExternalData(BrainHandle);
	auto& Data = Context.GetInstanceData(*this);
	Data.Activation = Brain.AllocateActivation();
	return Brain.Prepare(Brain.GetDecisionScope(), Data.ConsumerSlot, Data.Activation) ? EStateTreeRunStatus::Succeeded : EStateTreeRunStatus::Failed;
}

void FCombatAIPrepareOrderTask::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	if (Transition.CurrentRunStatus != EStateTreeRunStatus::Succeeded)
		Context.GetExternalData(BrainHandle).DiscardPrepared(Context.GetInstanceData(*this).Activation);
}

EStateTreeRunStatus FCombatAIExecuteOrderTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult&) const
{
	auto& Brain = Context.GetExternalData(BrainHandle);
	auto& Data = Context.GetInstanceData(*this);
	Data.Activation = Brain.AllocateActivation();
	if (!Brain.BeginAction(Brain.GetDecisionScope(), Data.ConsumerSlot, Data.Activation)) return EStateTreeRunStatus::Failed;
	return Brain.PollAction(Data.Activation);
}

EStateTreeRunStatus FCombatAIExecuteOrderTask::Tick(FStateTreeExecutionContext& Context, float) const
{
	return Context.GetExternalData(BrainHandle).PollAction(Context.GetInstanceData(*this).Activation);
}

void FCombatAIExecuteOrderTask::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult&) const
{
	Context.GetExternalData(BrainHandle).EndAction(Context.GetInstanceData(*this).Activation);
}

EStateTreeRunStatus FCombatAIResolveReceiptTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult&) const
{
	auto& Brain = Context.GetExternalData(BrainHandle);
	return Brain.ResolveReceipt(Brain.GetDecisionScope()) ? EStateTreeRunStatus::Succeeded : EStateTreeRunStatus::Failed;
}

EStateTreeRunStatus FCombatAIWaitTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult&) const
{
	auto& Brain = Context.GetExternalData(BrainHandle);
	auto& Data = Context.GetInstanceData(*this);
	Data.Activation = Brain.AllocateActivation();
	return Brain.BeginWait(Data.Activation, Data.WaitSeconds) ? EStateTreeRunStatus::Running : EStateTreeRunStatus::Failed;
}

EStateTreeRunStatus FCombatAIWaitTask::Tick(FStateTreeExecutionContext& Context, float) const
{
	return Context.GetExternalData(BrainHandle).IsWaitComplete(Context.GetInstanceData(*this).Activation) ? EStateTreeRunStatus::Succeeded : EStateTreeRunStatus::Running;
}

void FCombatAIWaitTask::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult&) const
{
	Context.GetExternalData(BrainHandle).EndWait(Context.GetInstanceData(*this).Activation);
}
