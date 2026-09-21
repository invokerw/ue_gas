#pragma once

#include "StateTreeTaskBase.h"
#include "StateTreeLinker.h"
#include "Combat/AI/CombatAIBrainComponent.h"
#include "CombatAIStateTreeTasks.generated.h"

/** 节点配置和本次激活私有数据；跨兄弟节点的数据只放 Brain 工作区，不从这里绑定。 */
USTRUCT()
struct COMBAT_API FCombatAITaskInstanceData
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, Category="Parameter", meta=(DisplayName="消费槽", ToolTip="Prepare 和 Execute 使用相同非空名称；默认 Action。链接资产也必须匹配。")) FName ConsumerSlot = TEXT("Action");
	UPROPERTY(EditAnywhere, Category="Parameter", meta=(DisplayName="等待秒数", ToolTip="Wait 任务等待的游戏秒数，必须有限且不小于 0；0 也在 Scheduler 后续回合完成。", Units="s", ClampMin="0")) float WaitSeconds = 0.1f;
	UPROPERTY(Transient) uint64 Activation = 0;
	FCombatAIDecisionScope OwnedScope;
};

/** Combat 原生任务共同外部依赖；只接收事件 Tick，禁止从引擎 Tick 累积玩法时间。 */
USTRUCT(meta=(Hidden))
struct COMBAT_API FCombatAITaskBase : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()
	using FInstanceDataType = FCombatAITaskInstanceData;
	FCombatAITaskBase();
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual bool Link(FStateTreeLinker& Linker) override;
	TStateTreeExternalDataHandle<UCombatAIBrainComponent> BrainHandle;
};

/** 父状态服务持有本轮工作区；不 Tick、不参与状态完成，子状态切换/重选不清理。 */
USTRUCT(meta=(DisplayName="AI 决策作用域", ToolTip="放在 Prepare/Execute/Resolve 的共同父状态；不能放到每个兄弟状态。"))
struct COMBAT_API FCombatAIDecisionScopeTask : public FCombatAITaskBase
{
	GENERATED_BODY()
	FCombatAIDecisionScopeTask();
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext&, const FStateTreeTransitionResult&) const override;
	virtual void ExitState(FStateTreeExecutionContext&, const FStateTreeTransitionResult&) const override;
};

/** 无轮询地等待新的 ObjectiveRevision；已确认目标不因重复事件再次执行。 */
USTRUCT(meta=(DisplayName="AI 等待目标", ToolTip="等待新的服务器目标输入，不逐帧评分、不注册周期计时。"))
struct COMBAT_API FCombatAIWaitObjectiveTask : public FCombatAITaskBase
{
	GENERATED_BODY()
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext&, const FStateTreeTransitionResult&) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext&, float) const override;
};

/** 复核并冻结一次目标命令；成功 Exit 后意图由 Scope 持有，失败出口应连接 Resolve。 */
USTRUCT(meta=(DisplayName="AI 准备命令", ToolTip="将显式 Objective 冻结为一次可消费意图；成功连接 Execute，失败连接 Resolve。"))
struct COMBAT_API FCombatAIPrepareOrderTask : public FCombatAITaskBase
{
	GENERATED_BODY()
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext&, const FStateTreeTransitionResult&) const override;
	virtual void ExitState(FStateTreeExecutionContext&, const FStateTreeTransitionResult&) const override;
};

/** 唯一命令写入任务：消费 Scope 意图，通过 Bridge 执行 Move/Attack/Cast，终态留给 Resolve。 */
USTRUCT(meta=(DisplayName="AI 执行命令", ToolTip="同一活动路径只能有一个；完成或失败均应转 Resolve，普通重评在任务内部遵守攻击边界。"))
struct COMBAT_API FCombatAIExecuteOrderTask : public FCombatAITaskBase
{
	GENERATED_BODY()
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext&, const FStateTreeTransitionResult&) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext&, float) const override;
	virtual void ExitState(FStateTreeExecutionContext&, const FStateTreeTransitionResult&) const override;
};

/** 消费唯一回执后才允许下一轮普通决策；不重复提交伤害或资源事务。 */
USTRUCT(meta=(DisplayName="AI 确认完成凭证", ToolTip="位于 Execute/Prepare 结果出口，确认一次成功或失败结果后才离开当前决策作用域。"))
struct COMBAT_API FCombatAIResolveReceiptTask : public FCombatAITaskBase
{
	GENERATED_BODY()
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext&, const FStateTreeTransitionResult&) const override;
};

/** 使用 Combat Scheduler 的等待，退出/死亡/撤权时取消；不用原生 Delay 的第二套计时语义。 */
USTRUCT(meta=(DisplayName="AI 调度器等待", ToolTip="等待配置的游戏秒数，组件休眠期间由 Combat Scheduler 唤醒；退出会取消等待。"))
struct COMBAT_API FCombatAIWaitTask : public FCombatAITaskBase
{
	GENERATED_BODY()
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext&, const FStateTreeTransitionResult&) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext&, float) const override;
	virtual void ExitState(FStateTreeExecutionContext&, const FStateTreeTransitionResult&) const override;
};
