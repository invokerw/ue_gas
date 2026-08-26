#pragma once

#include "CoreMinimal.h"
#include "Abilities/Tasks/AbilityTask.h"

#include "Combat/Core/CombatTypes.h"

#include "AbilityTask_WaitCombatInterval.generated.h"

/** 向蓝图或 C++ 广播一次确定性 Channel tick。 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCombatIntervalTickDelegate, FCombatScheduledTickContext, TickContext);
/** Channel duration 到期且任务已取消 repeating Handle 后广播。 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FCombatIntervalFinishedDelegate);

/** 用 Combat Scheduler 驱动 Ability Channel interval，并在销毁时统一清理句柄。 */
UCLASS()
class UE_GAS_API UAbilityTask_WaitCombatInterval : public UAbilityTask
{
	GENERATED_BODY()

public:
	/** 创建 interval/duration 均为正数的受管理 Channel Task。 */
	UFUNCTION(BlueprintCallable, Category="Combat|Ability|Tasks", meta=(DisplayName="Wait Combat Interval", HidePin="OwningAbility", DefaultToSelf="OwningAbility", BlueprintInternalUseOnly="true"))
	static UAbilityTask_WaitCombatInterval* WaitCombatInterval(
		UGameplayAbility* OwningAbility,
		float Interval,
		float Duration);

	/** 注册 repeating tick 与高优先级 finish task。 */
	virtual void Activate() override;
	/** Ability End/Cancel 或显式 EndTask 时取消全部 Scheduler Handle。 */
	virtual void OnDestroy(bool bInOwnerFinished) override;

	/** 每次逻辑 interval 到达时广播。 */
	UPROPERTY(BlueprintAssignable) FCombatIntervalTickDelegate OnTick;
	/** duration 到期时广播一次。 */
	UPROPERTY(BlueprintAssignable) FCombatIntervalFinishedDelegate OnFinished;

	/** 返回 repeating 与 finish 是否仍有任一活动句柄。 */
	bool HasActiveSchedule() const;

private:
	/** 接收 Scheduler repeating 回调并转发 TickContext。 */
	void HandleTick(const FCombatScheduledTickContext& TickContext);
	/** 先取消 repeating，再广播完成并结束 Task。 */
	void HandleFinished(const FCombatScheduledTickContext& TickContext);
	/** 幂等取消两个 Scheduler Handle。 */
	void CancelSchedules();

	/** 固定逻辑 tick 间隔。 */
	float TickInterval = 0.0f;
	/** Channel 总时长。 */
	float TotalDuration = 0.0f;
	/** Channel 的绝对结束时刻，用于补帧批次内淘汰恰好位于边界的 tick。 */
	double FinishAt = 0.0;
	/** repeating tick 句柄。 */
	FCombatScheduleHandle TickSchedule;
	/** 高优先级 finish 句柄，确保边界 tick 不在 duration 时刻执行。 */
	FCombatScheduleHandle FinishSchedule;
	/** 防止 finish 或 OnDestroy 重复广播。 */
	bool bFinished = false;
};
