#include "Combat/Ability/AbilityTask_WaitCombatInterval.h"

#include "Combat/Scheduling/CombatSchedulerSubsystem.h"

UAbilityTask_WaitCombatInterval* UAbilityTask_WaitCombatInterval::WaitCombatInterval(
	UGameplayAbility* OwningAbility,
	const float Interval,
	const float Duration)
{
	UAbilityTask_WaitCombatInterval* Task = NewAbilityTask<UAbilityTask_WaitCombatInterval>(OwningAbility);
	Task->TickInterval = Interval;
	Task->TotalDuration = Duration;
	return Task;
}

void UAbilityTask_WaitCombatInterval::Activate()
{
	Super::Activate();
	UCombatSchedulerSubsystem* Scheduler = GetWorld() ? GetWorld()->GetSubsystem<UCombatSchedulerSubsystem>() : nullptr;
	if (!Scheduler || !FMath::IsFinite(TickInterval) || TickInterval <= 0.0f
		|| !FMath::IsFinite(TotalDuration) || TotalDuration <= 0.0f || TickInterval > TotalDuration)
	{
		EndTask();
		return;
	}
	FinishAt = GetWorld()->GetTimeSeconds() + TotalDuration;
	// Finish 使用更高优先级，使恰好等于 duration 的 repeating tick 被取消而不执行。
	FinishSchedule = Scheduler->ScheduleOnce(
		this, TotalDuration, 1,
		FCombatScheduledDelegate::CreateUObject(this, &UAbilityTask_WaitCombatInterval::HandleFinished));
	TickSchedule = Scheduler->ScheduleRepeating(
		this, TickInterval, TickInterval, 0, ECombatCatchUpPolicy::ExecuteAllBounded,
		FCombatScheduledDelegate::CreateUObject(this, &UAbilityTask_WaitCombatInterval::HandleTick));
	if (!FinishSchedule.IsValid() || !TickSchedule.IsValid())
	{
		CancelSchedules();
		EndTask();
	}
}

void UAbilityTask_WaitCombatInterval::OnDestroy(const bool bInOwnerFinished)
{
	CancelSchedules();
	Super::OnDestroy(bInOwnerFinished);
}

bool UAbilityTask_WaitCombatInterval::HasActiveSchedule() const
{
	const UCombatSchedulerSubsystem* Scheduler = GetWorld() ? GetWorld()->GetSubsystem<UCombatSchedulerSubsystem>() : nullptr;
	return Scheduler && (Scheduler->IsHandleActive(TickSchedule) || Scheduler->IsHandleActive(FinishSchedule));
}

void UAbilityTask_WaitCombatInterval::HandleTick(const FCombatScheduledTickContext& TickContext)
{
	// Scheduler 补帧会在一个 repeating 回调批次中消费多个到期 tick，必须在 Task 层再拒绝 duration 边界。
	if (!bFinished && TickContext.ScheduledTime < FinishAt - UE_DOUBLE_SMALL_NUMBER
		&& ShouldBroadcastAbilityTaskDelegates())
	{
		OnTick.Broadcast(TickContext);
	}
}

void UAbilityTask_WaitCombatInterval::HandleFinished(const FCombatScheduledTickContext& TickContext)
{
	(void)TickContext;
	if (bFinished)
	{
		return;
	}
	bFinished = true;
	CancelSchedules();
	if (ShouldBroadcastAbilityTaskDelegates())
	{
		OnFinished.Broadcast();
	}
	EndTask();
}

void UAbilityTask_WaitCombatInterval::CancelSchedules()
{
	if (UCombatSchedulerSubsystem* Scheduler = GetWorld() ? GetWorld()->GetSubsystem<UCombatSchedulerSubsystem>() : nullptr)
	{
		Scheduler->Cancel(TickSchedule);
		Scheduler->Cancel(FinishSchedule);
	}
	TickSchedule = FCombatScheduleHandle();
	FinishSchedule = FCombatScheduleHandle();
}
