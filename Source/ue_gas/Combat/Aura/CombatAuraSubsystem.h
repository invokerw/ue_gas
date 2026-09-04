#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"

#include "Combat/Aura/CombatAuraTypes.h"
#include "Combat/Scheduling/CombatSchedulerSubsystem.h"

#include "CombatAuraSubsystem.generated.h"

/** 记录光环给一个目标维护的子效果，以及施加时目标属于哪一次生命。 */
struct FCombatAuraChildRecord
{
	/** 通过目标的 ModifierComponent 查询和移除子效果的句柄；效果仍存在时不重复施加。 */
	FCombatModifierHandle ModifierHandle;
	/** 施加效果时目标的生命编号；同一个 Actor 复活后编号变化，旧效果记录不能继续当作本次生命的效果。 */
	uint32 TargetLifeGeneration = 0;
};

/** 一个活动光环的服务器记录，集中持有创建参数、检查任务及各目标的子效果记录。 */
struct FCombatAuraRuntimeRecord
{
	/** 本次光环的身份，包含记录编号、子系统代次及所有者的生命编号。 */
	FCombatAuraHandle Handle;
	/** 启动时复制的参数；保存所有者和效果定义的引用，不冻结单位之后的位置与队伍。 */
	FCombatAuraSpec Spec;
	/** 定期检查目标的调度任务；多轮检查因卡顿到期时合并为一次当前状态检查，结束光环时取消。 */
	FCombatScheduleHandle ReconcileSchedule;
	/** 每个已施加目标对应的效果记录。弱引用不延长目标寿命；下一轮检查淘汰失效目标和旧生命记录。 */
	TMap<TWeakObjectPtr<ACombatUnitCharacter>, FCombatAuraChildRecord> Children;
};

/**
 * 在服务器上管理以单位为中心的光环。光环定期检查范围内合格的单位，维护施加在这些单位身上的普通增益或减益效果（子效果）。
 * 每轮移除不再合格的目标所对应的效果，并为缺少效果的合格目标尝试补上；已有有效效果保持不变。这一检查和增删过程称为 Reconcile。
 * 目标筛选交给 TargetingSubsystem，效果施加交给目标的 ModifierComponent，检查时机交给 Combat Scheduler。
 * 光环取消、所有者死亡或退出场景、世界关闭时，统一清理子效果和检查任务；客户端只观察战斗状态的复制结果。
 */
UCLASS()
class UE_GAS_API UCombatAuraSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/**
	 * 校验所有者权限、存活状态、效果定义及数值，登记光环并立即检查目标。
	 * 成功仅表示光环仍已登记，目标为空或子效果施加被拒绝时也可能成功；失败原因见结果标签。
	 */
	FCombatAuraResult StartAura(const FCombatAuraSpec& Spec);
	/**
	 * 请求停止光环并清理其子效果。通常同步完成；在光环更新的回调中调用时，先排队，外层操作结束后执行。
	 * 返回成功可能仅表示已接受取消请求；无效或已结束的句柄返回失败，不重复广播结束。
	 */
	FCombatAuraResult CancelAura(FCombatAuraHandle Handle);
	/**
	 * 立即检查指定光环的目标及子效果；所有者失效时会同时结束光环。
	 * 句柄失效、正在更新或世界关闭、缺少目标查询服务时返回 false；true 不保证存在受影响目标或每个效果都施加成功。
	 */
	bool ReconcileNow(FCombatAuraHandle Handle);
	/**
	 * 单位队伍、生命或相关状态变化后检查全部活动光环，因为它可能是任一光环的所有者或目标。
	 * 若通知来自当前增删效果的回调，则留待后续定期检查处理，避免递归更新。
	 */
	void NotifyUnitChanged(ACombatUnitCharacter* Unit);
	/** 单位退出场景时，结束它产生的光环，并移除其他光环在它身上的效果记录；更新期间的重入通知留待后续检查或世界清理。 */
	void NotifyUnitEndPlay(ACombatUnitCharacter* Unit);
	/** 查询句柄是否匹配本世界仍登记的光环；Break 暂停且没有子效果的光环仍属于活动光环。 */
	bool IsAuraActive(FCombatAuraHandle Handle) const;
	/** 返回指定光环保存的目标效果记录数；句柄不匹配时返回 0，因此 0 不能用于判断光环已结束。 */
	int32 GetChildCount(FCombatAuraHandle Handle) const;
	/** 返回 World 当前活动 Aura 数量。 */
	int32 GetActiveAuraCount() const { return ActiveAuras.Num(); }
	/** 统计本世界全部光环保存的目标效果记录数；同一目标被多个光环记录时分别计数。 */
	int32 GetTotalChildCount() const;
	/** 取得本子系统最近一次最终结束的结果；首次结束前为默认值，下次结束会覆盖该引用所指的数据。 */
	const FCombatAuraResult& GetLastFinishedResult() const { return LastFinishedResult; }
	/** 订阅光环完成清理后的本地通知；订阅者应在自身退出时解绑。 */
	FOnCombatAuraFinished& OnAuraFinished() { return AuraFinishedDelegate; }
	/** 世界关闭时结束全部光环，清理子效果、调度任务和待取消请求，并使本代光环句柄失效。 */
	virtual void Deinitialize() override;

private:
	/** 定期检查入口，将创建时捕获的完整光环句柄交给 ReconcileNow 校验，旧任务不能更新新光环。 */
	void HandleReconcile(FCombatAuraHandle Handle, const FCombatScheduledTickContext& TickContext);
	/**
	 * 先验证所有者及 Break 状态，再查询当前合格目标；移除不合格或失效的子效果记录，尝试为缺少效果的目标施加。
	 * 已有有效效果不续期；所有者失效时结束光环并返回 false，Break 暂停时清空效果但返回 true。
	 */
	bool ReconcileRecord(FCombatAuraRuntimeRecord& Record);
	/** 按保存的句柄请求移除全部子效果，再清空目标映射；目标或效果已经失效时也可安全清理。 */
	void RemoveAllChildren(FCombatAuraRuntimeRecord& Record);
	/** 验证完整身份后清理效果和检查任务，删除活动记录，再记录结果并广播一次结束通知；重复结束返回旧句柄失败。 */
	FCombatAuraResult FinishAura(FCombatAuraHandle Handle, ECombatAuraFinishReason Reason, FGameplayTag FailureTag);
	/** 在外层光环更新完成后，按请求顺序处理回调中排队的取消；跳过已经结束的光环。 */
	void FlushDeferredCancels();
	/** 记录光环启动、目标检查或结束事件；日志数值槽分别保存查询半径与目标效果记录数。 */
	void EmitAuraLog(const FCombatAuraRuntimeRecord& Record, FGameplayTag EventType, FGameplayTag FailureTag) const;

	/** 以光环编号索引活动记录；查询后仍须核对完整句柄，防止旧生命或旧子系统代次误匹配。 */
	TMap<uint64, FCombatAuraRuntimeRecord> ActiveAuras;
	uint64 NextAuraId = 1;
	uint32 AuraGeneration = 1;
	/** 增删子效果可能同步触发状态或生命周期通知；此标志让嵌套检查退出、取消请求排队，避免遍历中的记录被递归修改。 */
	bool bMutatingRegistry = false;
	bool bDeinitializing = false;
	/** 更新期间收到的取消请求；同一句柄只排队一次，外层操作完成后按先来后到顺序处理。 */
	TArray<FCombatAuraHandle> DeferredCancelHandles;
	FCombatAuraResult LastFinishedResult;
	FOnCombatAuraFinished AuraFinishedDelegate;
};
