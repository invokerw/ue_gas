#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"

#include "Combat/Attack/CombatAttackTypes.h"
#include "Combat/Targeting/CombatTargetingTypes.h"

#include "CombatAttackComponent.generated.h"

class ACombatUnitCharacter;

/** Attack point 到达并产生一次权威发射后广播。 */
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnCombatAttackLaunched, FCombatAttackHandle, FCombatOrderHandle);
/** 本轮 BAT 到达绝对 ready 时间后广播。 */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnCombatAttackReady, FCombatOrderHandle);
/** AttackRecord 以任意 outcome exactly-once 结束后广播。 */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnCombatAttackFinalized, const FCombatAttackResult&);

/** 持有单位唯一 AttackRecord registry，并统一前摇、ready、随机与命中结算。 */
UCLASS(ClassGroup=(Combat), meta=(BlueprintSpawnableComponent))
class UE_GAS_API UCombatAttackComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	/** 默认关闭 Tick；前摇与 ready 只由 Combat Scheduler 驱动。 */
	UCombatAttackComponent();

	/** 无副作用检查当前状态、目标、距离、LOS、朝向与 ready。 */
	FCombatOperationResult CanStartMeleeAttack(ACombatUnitCharacter* Target) const;
	/** 创建 Pending Record、提交法球 winner 并安排唯一 attack point。 */
	FCombatAttackResult StartMeleeAttack(ACombatUnitCharacter* Target, FCombatOrderHandle OrderHandle);
	/** 幂等终结仍活动的 Record；主要供 M5 Projectile 回调使用。 */
	FCombatAttackResult FinalizeAttack(FCombatAttackHandle Handle);
	/** Attack Projectile 命中原目标后结算；跳过已由弹道承担的距离与 LOS 复核。 */
	FCombatAttackResult FinalizeAttackFromProjectile(FCombatAttackHandle Handle, ACombatUnitCharacter* ImpactTarget);
	/** Attack Projectile fizzle、阻挡或超时后 exactly-once 失败对应已发射记录。 */
	bool FailLaunchedAttackFromProjectile(FCombatAttackHandle Handle, FGameplayTag FailureTag);
	/** 仅取消尚未越过 attack point 且属于指定 Order 的前摇。 */
	bool CancelWindupForOrder(FCombatOrderHandle OrderHandle, FGameplayTag FailureTag);
	/** 返回 Handle 是否仍能解析当前生命代次的活动 Record。 */
	bool IsAttackActive(FCombatAttackHandle Handle) const;
	/** 返回当前是否允许创建下一轮 AttackRecord。 */
	bool IsAttackReady() const { return bAttackReady; }
	/** 返回当前 registry 数量，供生命周期与泄漏验收。 */
	int32 GetActiveAttackCount() const { return ActiveRecords.Num(); }
	/** 返回最近一次 exactly-once 结束结果。 */
	const FCombatAttackResult& GetLastFinalizedResult() const { return LastFinalizedResult; }
	/** 返回当前前摇 Handle；没有前摇时无效。 */
	FCombatAttackHandle GetCurrentWindupHandle() const { return CurrentWindupHandle; }
	/** 复制一条活动记录的不可变调试快照；过期 Handle 返回 false。 */
	bool GetAttackRecordSnapshot(FCombatAttackHandle Handle, FCombatAttackRecord& OutRecord) const;

	/** 生命周期 Dying 屏障调用，取消全部记录与调度并淘汰旧 generation。 */
	void HandleOwnerDeath();
	/** 新生命建立后恢复 Ready，旧 Handle 仍因 life generation 无效。 */
	void HandleOwnerRespawn();
	/** 状态 Tag 变化后在攻击阻止时取消当前前摇。 */
	void HandleOwnerStatusChanged();
	/** Actor 结束时 exactly-once 清空 registry 与全部 Scheduler 句柄。 */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** 返回 AttackLaunched 原生委托。 */
	FOnCombatAttackLaunched& OnAttackLaunched() { return AttackLaunchedDelegate; }
	/** 返回 AttackReady 原生委托。 */
	FOnCombatAttackReady& OnAttackReady() { return AttackReadyDelegate; }
	/** 返回 AttackFinalized 原生委托。 */
	FOnCombatAttackFinalized& OnAttackFinalized() { return AttackFinalizedDelegate; }

private:
	/** 返回组件所属 Combat Unit。 */
	ACombatUnitCharacter* GetOwnerUnit() const;
	/** 使用完整 Handle/generation/life generation 查找可写记录。 */
	FCombatAttackRecord* FindRecord(FCombatAttackHandle Handle);
	/** attack point Scheduler 回调：发射、安排 ready 并执行近战 impact。 */
	void HandleAttackPoint(FCombatAttackHandle Handle, const FCombatScheduledTickContext& TickContext);
	/** recovery 到期后切换 Ready；旧 OrderHandle 仍由 OrderComponent 复核。 */
	void HandleAttackReady(FCombatOrderHandle OrderHandle, const FCombatScheduledTickContext& TickContext);
	/** 执行 impact 合法性、Evasion、Crit、Damage 与快照 OnHit。 */
	FCombatAttackResult FinalizeAttackInternal(
		FCombatAttackHandle Handle,
		bool bProjectileImpact = false,
		ACombatUnitCharacter* ImpactTarget = nullptr);
	/** 以指定失败 outcome 原子结束一条记录。 */
	FCombatAttackResult AbortRecord(FCombatAttackHandle Handle, ECombatAttackOutcome Outcome, FGameplayTag FailureTag);
	/** 写入最终状态、日志、移除 registry 并广播一次。 */
	FCombatAttackResult CompleteRecord(
		FCombatAttackRecord& Record,
		ECombatAttackOutcome Outcome,
		FGameplayTag FailureTag,
		float AppliedDamage);
	/** 对 winner 中的 Damage/ApplyModifier 快照按顺序走公共入口。 */
	float ExecuteOnHitActions(const FCombatAttackRecord& Record, const FCombatEventContext& ParentEvent) const;
	/** 构造 Enemy/Alive/Range/LOS 共用普攻目标规则。 */
	FCombatTargetingRules MakeAttackTargetingRules() const;
	/** 检查 Owner 当前 XY 朝向是否落在 UnitData 容差内。 */
	bool IsFacingTarget(const ACombatUnitCharacter& Target) const;
	/** 为 AttackLaunched/Landed/Failed 输出结构化记录。 */
	void EmitAttackLog(const FCombatAttackRecord& Record, const FCombatAttackResult* Result) const;
	/** 取消当前 windup/ready 句柄，不触发业务回调。 */
	void CancelSchedules();

	/** 以记录 ID 索引的唯一权威 registry。 */
	TMap<uint64, FCombatAttackRecord> ActiveRecords;
	/** 下一个 AttackHandle 槽位 ID。 */
	uint64 NextRecordId = 1;
	/** Death/EndPlay 时递增，淘汰仍携带旧组件 generation 的回调。 */
	uint32 RegistryGeneration = 1;
	/** 当前尚未越过 attack point 的记录。 */
	FCombatAttackHandle CurrentWindupHandle;
	/** 当前 attack point 的单次调度句柄。 */
	FCombatScheduleHandle WindupSchedule;
	/** 当前 recovery 的单次调度句柄。 */
	FCombatScheduleHandle ReadySchedule;
	/** recovery 对应的持续 Order。 */
	FCombatOrderHandle ReadyOrderHandle;
	/** 没有前摇或 recovery 时为 true。 */
	bool bAttackReady = true;
	/** 防止 EndPlay 清理期间继续输出或重新调度。 */
	bool bEnding = false;
	/** 最近一次结束结果的调试快照。 */
	FCombatAttackResult LastFinalizedResult;
	/** 三类外部观察委托。 */
	FOnCombatAttackLaunched AttackLaunchedDelegate;
	FOnCombatAttackReady AttackReadyDelegate;
	FOnCombatAttackFinalized AttackFinalizedDelegate;
};
