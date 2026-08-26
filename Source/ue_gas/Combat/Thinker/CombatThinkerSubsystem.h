#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"

#include "Combat/Scheduling/CombatSchedulerSubsystem.h"
#include "Combat/Thinker/CombatThinkerTypes.h"

#include "CombatThinkerSubsystem.generated.h"

class ACombatThinker;

/** 单条活动 Thinker 的 Spec、Actor、Schedule 与累计结果。 */
struct FCombatThinkerRuntimeRecord
{
	FCombatThinkerHandle Handle;
	FCombatThinkerSpec Spec;
	TWeakObjectPtr<ACombatThinker> Actor;
	FCombatScheduleHandle PulseSchedule;
	FCombatScheduleHandle FinishSchedule;
	int32 AffectedTargetCount = 0;
};

/** 持有无 Tick Thinker registry，并只用 Combat Scheduler 驱动 delay/pulse/duration。 */
UCLASS()
class UE_GAS_API UCombatThinkerSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** 校验 Spec、生成无 Tick Actor 并建立受句柄保护的 Scheduler 任务。 */
	FCombatThinkerResult CreateThinker(const FCombatThinkerSpec& Spec);
	/** 显式取消活动 Thinker。 */
	FCombatThinkerResult CancelThinker(FCombatThinkerHandle Handle);
	/** Ability End 时取消显式绑定同一 ActivationId 的 Thinker。 */
	int32 CancelThinkersForAbility(ACombatUnitCharacter* Source, FCombatEventId ActivationId);
	/** 外部 Actor EndPlay 回报。 */
	void NotifyThinkerEndPlay(FCombatThinkerHandle Handle);
	/** 返回 Handle 是否仍活动。 */
	bool IsThinkerActive(FCombatThinkerHandle Handle) const;
	/** 返回活动数量。 */
	int32 GetActiveThinkerCount() const { return ActiveThinkers.Num(); }
	/** 返回最近 Finish 结果。 */
	const FCombatThinkerResult& GetLastFinishedResult() const { return LastFinishedResult; }
	/** 返回 Finish 观察委托。 */
	FOnCombatThinkerFinished& OnThinkerFinished() { return ThinkerFinishedDelegate; }
	/** World teardown 清理 Actor 与全部 Scheduler 句柄。 */
	virtual void Deinitialize() override;

private:
	/** Scheduler pulse：服务器稳定查询目标并走 DamageSubsystem。 */
	void HandlePulse(FCombatThinkerHandle Handle, const FCombatScheduledTickContext& TickContext);
	/** Duration 到期回调。 */
	void HandleFinished(FCombatThinkerHandle Handle, const FCombatScheduledTickContext& TickContext);
	/** exactly-once Finish、取消任务、广播并销毁 Actor。 */
	FCombatThinkerResult FinishThinker(FCombatThinkerHandle Handle, ECombatThinkerFinishReason Reason, FGameplayTag FailureTag);
	/** 输出 Pulse/Finished 结构化日志。 */
	void EmitThinkerLog(const FCombatThinkerRuntimeRecord& Record, FGameplayTag EventType, int32 PulseTargets, FGameplayTag FailureTag) const;

	/** Handle Id -> runtime record。 */
	TMap<uint64, FCombatThinkerRuntimeRecord> ActiveThinkers;
	uint64 NextThinkerId = 1;
	uint32 ThinkerGeneration = 1;
	bool bDeinitializing = false;
	FCombatThinkerResult LastFinishedResult;
	FOnCombatThinkerFinished ThinkerFinishedDelegate;
};
