#pragma once

#include "CoreMinimal.h"
#include "Abilities/Tasks/AbilityTask.h"

#include "Combat/Core/CombatTypes.h"

#include "AbilityTask_WaitCombatInterval.generated.h"

/** 报告一次引导周期到点，包含计划时刻和实际调度时刻；卡顿补执行时两者可能不同。 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCombatIntervalTickDelegate, FCombatScheduledTickContext, TickContext);
/** 引导计时正常到期、周期任务已取消后发出的完成通知；任务被提前销毁时不广播。 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FCombatIntervalFinishedDelegate);

/**
 * 在技能引导期间按间隔通知调用方，并在总时长到期时通知完成。
 * 首次周期发生在一个间隔之后，恰好落在结束时刻的周期不执行；例如间隔 1 秒、总长 3 秒，只在第 1、2 秒触发周期，第 3 秒完成。
 * 全部计时使用 Combat Scheduler，技能取消或任务销毁时清理周期与结束任务。
 */
UCLASS()
class UE_GAS_API UAbilityTask_WaitCombatInterval : public UAbilityTask
{
	GENERATED_BODY()

public:
	/** 构造引导计时任务；间隔与总时长须有限且大于 0，间隔不能大于总时长。激活时参数非法会直接结束任务，不发送正常完成通知。 */
	UFUNCTION(BlueprintCallable, Category="Combat|Ability|Tasks", meta=(DisplayName="等待战斗引导周期", ToolTip="构造引导计时任务；间隔与总时长须有限且大于 0，间隔不能大于总时长。激活时参数非法会直接结束任务，不发送正常完成通知。", HidePin="OwningAbility", DefaultToSelf="OwningAbility", BlueprintInternalUseOnly="true"))
	static UAbilityTask_WaitCombatInterval* WaitCombatInterval(
		UPARAM(DisplayName="所属技能") UGameplayAbility* OwningAbility,
		UPARAM(DisplayName="周期间隔") float Interval,
		UPARAM(DisplayName="总时长") float Duration);

	/** 注册周期任务与优先执行的到期任务；任一注册失败都取消已注册任务并结束。 */
	virtual void Activate() override;
	/** 技能结束、取消或调用 EndTask 时取消两个计时任务，防止引导结束后继续触发。 */
	virtual void OnDestroy(bool bInOwnerFinished) override;

	/** 每个尚未到总时长终点的计划周期触发；卡顿时按预算补执行，结束边界的周期始终跳过。 */
	UPROPERTY(BlueprintAssignable) FCombatIntervalTickDelegate OnTick;
	/** 总时长正常到期时广播一次；提前取消不广播，是否中断由技能自身的结束流程通知。 */
	UPROPERTY(BlueprintAssignable) FCombatIntervalFinishedDelegate OnFinished;

	/** 检查周期或到期任务是否至少一个仍活动，供调用方确认计时任务启动成功或已经清理。 */
	bool HasActiveSchedule() const;

private:
	/** 转发未到结束边界的周期回调；补执行批次中也逐次检查计划时刻，避免补出结束之后的效果。 */
	void HandleTick(const FCombatScheduledTickContext& TickContext);
	/** 正常到期时先取消周期和到期任务，再广播完成并结束自身，重复完成回调不再次通知。 */
	void HandleFinished(const FCombatScheduledTickContext& TickContext);
	/** 取消周期和到期任务并清空本地句柄，可在正常结束和销毁路径重复调用。 */
	void CancelSchedules();

	/** 引导相邻两次周期通知之间的游戏秒数。 */
	float TickInterval = 0.0f;
	/** 从任务激活开始计算的引导总秒数。 */
	float TotalDuration = 0.0f;
	/** 任务结束的世界游戏时间，单位为秒；补执行时用它排除计划时刻已达到终点的周期。 */
	double FinishAt = 0.0;
	/** 引导周期任务的句柄，任务销毁或到期时取消。 */
	FCombatScheduleHandle TickSchedule;
	/** 到期任务的句柄；与周期同刻到期时优先结束，从而不执行边界周期。 */
	FCombatScheduleHandle FinishSchedule;
	/** 标记正常到期通知已发出，防止重复完成回调再次广播。 */
	bool bFinished = false;
};
