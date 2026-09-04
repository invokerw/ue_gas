#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"

#include "Combat/Attack/CombatAttackTypes.h"
#include "Combat/Targeting/CombatTargetingTypes.h"

#include "CombatAttackComponent.generated.h"

class ACombatUnitCharacter;

/** 普攻前摇结束、攻击进入已发射状态时的服务器通知；近战接着结算命中，远程接着尝试创建弹体。 */
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnCombatAttackLaunched, FCombatAttackHandle, FCombatOrderHandle);
/** 从攻击起手计算的攻击间隔结束、允许开始下一轮时通知指令组件；不等待上一枚远程弹体命中。 */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnCombatAttackReady, FCombatOrderHandle);
/** 一次普攻的最终命中、闪避、失败或取消结果；同一攻击记录只广播一次。 */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnCombatAttackFinalized, const FCombatAttackResult&);

/**
 * 单位普通攻击的服务器执行器，管理每次攻击的独立记录。
 * 起手时保存基础伤害、目标生命编号、攻速时序、暴击配置和已提交的法球参数；前摇结束后近战直接结算，远程发射弹体。
 * 真正结算命中时才读取目标当前闪避并进行闪避、暴击判定。攻击间隔从起手计时，到点可开始下一轮，无需等待旧弹体命中。
 * 调度器控制前摇和再次就绪，所有最终结果汇入同一结束入口；死亡或退出场景清理记录及任务。
 */
UCLASS(ClassGroup=(Combat), meta=(BlueprintSpawnableComponent))
class UE_GAS_API UCombatAttackComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCombatAttackComponent();

	/** 预检单位是否能开始一轮普攻：服务器权限、存活、控制状态、距离、视线、朝向、移动状态及攻击间隔。名称沿用 Melee，但远程起手也走此入口。 */
	FCombatOperationResult CanStartMeleeAttack(ACombatUnitCharacter* Target) const;
	/** 预检后保存攻击参数、选择并提交法球资源，登记前摇任务。成功表示攻击已起手，尚未命中；单位配置决定前摇结束后近战结算还是发射弹体。 */
	FCombatAttackResult StartMeleeAttack(ACombatUnitCharacter* Target, FCombatOrderHandle OrderHandle);
	/** 对已发射的活动攻击做普通命中结算，包含距离与视线复核；前摇中、已结束或旧句柄失败。远程碰撞应使用专门的弹体命中入口。 */
	FCombatAttackResult FinalizeAttack(FCombatAttackHandle Handle);
	/** 普攻弹体碰到原目标时调用，复核目标身份、生命和当前可攻击状态，再处理闪避、暴击和伤害；距离与视线已由飞行路径负责。 */
	FCombatAttackResult FinalizeAttackFromProjectile(FCombatAttackHandle Handle, ACombatUnitCharacter* ImpactTarget);
	/** 普攻弹体因消散、阻挡、超时等未命中原因结束时，关闭对应已发射攻击；旧句柄或状态不符返回 false，不重复结算。 */
	bool FailLaunchedAttackFromProjectile(FCombatAttackHandle Handle, FGameplayTag FailureTag);
	/** 取消指定命令尚未发射的一轮普攻前摇；已经发射的弹体和攻击记录不由此接口取消。 */
	bool CancelWindupForOrder(FCombatOrderHandle OrderHandle, FGameplayTag FailureTag);
	/** 返回 Handle 是否仍能解析当前生命代次的活动 Record。 */
	bool IsAttackActive(FCombatAttackHandle Handle) const;
	/** 返回当前是否允许创建下一轮 AttackRecord。 */
	bool IsAttackReady() const { return bAttackReady; }
	/** 返回尚未最终结束的攻击记录数，包含前摇和飞行中的远程攻击；用于检查清理是否完整。 */
	int32 GetActiveAttackCount() const { return ActiveRecords.Num(); }
	/** 取得本组件最近一次攻击最终结果；首次结束前为默认值，下次结束会覆盖。 */
	const FCombatAttackResult& GetLastFinalizedResult() const { return LastFinalizedResult; }
	/** 返回当前前摇 Handle；没有前摇时无效。 */
	FCombatAttackHandle GetCurrentWindupHandle() const { return CurrentWindupHandle; }
	/** 复制一条活动记录的不可变调试快照；过期 Handle 返回 false。 */
	bool GetAttackRecordSnapshot(FCombatAttackHandle Handle, FCombatAttackRecord& OutRecord) const;

	/** 单位开始死亡时取消全部未完成攻击及前摇/就绪任务，提升组件代次使旧回调失效。 */
	void HandleOwnerDeath();
	/** 复活时恢复可开始攻击状态；上一条生命的攻击句柄仍失效，不能由旧弹体继续结算。 */
	void HandleOwnerRespawn();
	/** 状态 Tag 变化后在攻击阻止时取消当前前摇。 */
	void HandleOwnerStatusChanged();
	/** 单位退出场景时结束全部未完成攻击并取消前摇/就绪调度，结束过程中不再安排新任务。 */
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
	/** 前摇到点后复核目标，标记已发射并按起手时间安排再次就绪；按配置创建远程弹体或执行近战命中。 */
	void HandleAttackPoint(FCombatAttackHandle Handle, const FCombatScheduledTickContext& TickContext);
	/** 攻击间隔到期后允许下一次起手并通知指令组件；是否仍应继续攻击由当前命令身份复核决定。 */
	void HandleAttackReady(FCombatOrderHandle OrderHandle, const FCombatScheduledTickContext& TickContext);
	/** 结算一次已发射攻击：验证目标、判定闪避与暴击、提交主伤害及起手时保存的命中附加动作，随后结束记录。 */
	FCombatAttackResult FinalizeAttackInternal(
		FCombatAttackHandle Handle,
		bool bProjectileImpact = false,
		ACombatUnitCharacter* ImpactTarget = nullptr);
	/** 按指定失败或取消原因结束活动攻击；若仍在前摇则一并取消前摇并恢复可起手状态。 */
	FCombatAttackResult AbortRecord(FCombatAttackHandle Handle, ECombatAttackOutcome Outcome, FGameplayTag FailureTag);
	/** 形成最终攻击结果并移除活动记录、记录日志和通知观察者；调用方不得再使用被移除的记录引用。 */
	FCombatAttackResult CompleteRecord(
		FCombatAttackRecord& Record,
		ECombatAttackOutcome Outcome,
		FGameplayTag FailureTag,
		float AppliedDamage);
	/** 按起手时保存的命中动作顺序提交额外伤害或效果，返回额外伤害实际扣血合计；不重新查询法球当前配置。 */
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
	/** 当前仍在前摇、尚未发射的攻击句柄，用于 Stop 或状态阻止时取消。 */
	FCombatAttackHandle CurrentWindupHandle;
	/** 当前攻击前摇结束任务的句柄。 */
	FCombatScheduleHandle WindupSchedule;
	/** 到达本轮攻击间隔终点后恢复可起手状态的任务句柄。 */
	FCombatScheduleHandle ReadySchedule;
	/** 本轮攻击间隔结束时要通知的持续攻击命令，由指令组件检查是否仍有效。 */
	FCombatOrderHandle ReadyOrderHandle;
	/** 表示当前攻击间隔允许起手；还须通过状态、目标、距离及朝向预检，可能仍有上一轮远程弹体在飞行。 */
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
