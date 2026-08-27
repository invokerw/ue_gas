#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"

#include "Combat/Aura/CombatAuraTypes.h"
#include "Combat/Scheduling/CombatSchedulerSubsystem.h"

#include "CombatAuraSubsystem.generated.h"

/** Aura 当前目标对应的 child Modifier 与目标生命快照。 */
struct FCombatAuraChildRecord
{
	/** 目标当前由 Aura 创建或刷新的一一映射 Modifier。 */
	FCombatModifierHandle ModifierHandle;
	/** Apply 时目标生命代次；复活后旧 child 必须被替换。 */
	uint32 TargetLifeGeneration = 0;
};

/** Aura registry 内一条活动记录。 */
struct FCombatAuraRuntimeRecord
{
	/** registry 内稳定 Aura 身份。 */
	FCombatAuraHandle Handle;
	/** Start 时冻结的 Owner、Targeting 与 child 策略。 */
	FCombatAuraSpec Spec;
	/** Coalesce reconcile 的唯一 Scheduler 句柄。 */
	FCombatScheduleHandle ReconcileSchedule;
	/** 合法目标到 child Modifier/生命代次的一一映射。 */
	TMap<TWeakObjectPtr<ACombatUnitCharacter>, FCombatAuraChildRecord> Children;
};

/** 每 World 唯一 Aura registry，使用 Targeting + Scheduler 对 child Modifier 做幂等 reconcile。 */
UCLASS()
class UE_GAS_API UCombatAuraSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** 校验并启动 Aura，随后立即执行一次 reconcile。 */
	FCombatAuraResult StartAura(const FCombatAuraSpec& Spec);
	/** 显式结束 Aura 并移除全部 child。 */
	FCombatAuraResult CancelAura(FCombatAuraHandle Handle);
	/** 立即重算指定 Aura，供换队和自动化使用。 */
	bool ReconcileNow(FCombatAuraHandle Handle);
	/** Unit 队伍或生命状态变化后主动重算可能受影响的 Aura。 */
	void NotifyUnitChanged(ACombatUnitCharacter* Unit);
	/** Unit EndPlay 前清理其拥有的 Aura，并从其他 Aura child 中移除。 */
	void NotifyUnitEndPlay(ACombatUnitCharacter* Unit);
	/** 返回 Handle 是否仍在当前 registry generation 活动。 */
	bool IsAuraActive(FCombatAuraHandle Handle) const;
	/** 返回指定 Aura 当前 child 数量。 */
	int32 GetChildCount(FCombatAuraHandle Handle) const;
	/** 返回 World 当前活动 Aura 数量。 */
	int32 GetActiveAuraCount() const { return ActiveAuras.Num(); }
	/** 返回最近一次 exactly-once Finish。 */
	const FCombatAuraResult& GetLastFinishedResult() const { return LastFinishedResult; }
	/** 返回 Finish 观察委托。 */
	FOnCombatAuraFinished& OnAuraFinished() { return AuraFinishedDelegate; }
	/** World teardown 移除 child、调度和 registry。 */
	virtual void Deinitialize() override;

private:
	/** Scheduler 回调，只接受完整有效 AuraHandle。 */
	void HandleReconcile(FCombatAuraHandle Handle, const FCombatScheduledTickContext& TickContext);
	/** 对当前 Targeting 快照执行 add/remove/repair。 */
	bool ReconcileRecord(FCombatAuraRuntimeRecord& Record);
	/** 幂等移除一条记录的全部 child。 */
	void RemoveAllChildren(FCombatAuraRuntimeRecord& Record);
	/** exactly-once 结束记录。 */
	FCombatAuraResult FinishAura(FCombatAuraHandle Handle, ECombatAuraFinishReason Reason, FGameplayTag FailureTag);
	/** 最外层 registry 变更结束后执行回调期间请求的 Aura 取消。 */
	void FlushDeferredCancels();
	/** 输出 Started/Reconciled/Finished 结构化日志。 */
	void EmitAuraLog(const FCombatAuraRuntimeRecord& Record, FGameplayTag EventType, FGameplayTag FailureTag) const;

	/** Handle Id 到活动 Aura 记录。 */
	TMap<uint64, FCombatAuraRuntimeRecord> ActiveAuras;
	uint64 NextAuraId = 1;
	uint32 AuraGeneration = 1;
	/** 防止 child Tag/生命周期回调在 registry 遍历中重入 reconcile 或 Finish。 */
	bool bMutatingRegistry = false;
	bool bDeinitializing = false;
	/** registry 变更回调中到达的显式 Cancel，待外层操作完成后按 FIFO 执行。 */
	TArray<FCombatAuraHandle> DeferredCancelHandles;
	FCombatAuraResult LastFinishedResult;
	FOnCombatAuraFinished AuraFinishedDelegate;
};
