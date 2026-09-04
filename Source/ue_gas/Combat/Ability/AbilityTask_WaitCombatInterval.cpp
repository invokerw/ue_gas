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
	// 引导终点不再产生周期效果；同一计划时刻先执行完成任务，取消落在终点的周期。
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
	// 补执行可能先处理较早周期并在同一批中推进到结束边界；不能只靠到期任务的排序，转发前还要逐次核对计划时刻。
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
