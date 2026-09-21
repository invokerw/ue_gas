#pragma once

#include "Combat/AI/CombatAIStateTreeTasks.h"
#include "StateTreeConditionBase.h"
#include "CombatAIRoleTasks.generated.h"

/** 原子等待的唤醒条件；只决定任务何时完成，后续行为由资产完成转移表达。 */
UENUM()
enum class ECombatAIRoleWait : uint8
{
	Decision UMETA(DisplayName="新观察或职责"),
	Assignment UMETA(DisplayName="首个合法职责"),
	NewAssignment UMETA(DisplayName="新的合法职责"),
	Retry UMETA(DisplayName="故障退避"),
	RoutePause UMETA(DisplayName="航点停留")
};

/** 角色节点配置和单次激活身份；跨状态结果只保存在 Brain 工作区。 */
USTRUCT()
struct COMBAT_API FCombatAIRoleTaskInstanceData : public FCombatAITaskInstanceData
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, Category="Parameter", meta=(DisplayName="操作类别", ToolTip="准备 Home/Route/Attack 意图，用于到达确认和故障隔离；不要用于替代树的行为选择。")) ECombatAIRoleOperation Operation = ECombatAIRoleOperation::Attack;
	UPROPERTY(EditAnywhere, Category="Parameter", meta=(DisplayName="等待条件", ToolTip="选择等待新观察、职责或 Scheduler；故障等待不会被普通感知或受击解除。")) ECombatAIRoleWait Wait = ECombatAIRoleWait::Decision;
	UPROPERTY(Transient) uint64 Revision = 0;
	UPROPERTY(Transient) uint64 Snapshot = 0;
};

/** 从已知候选、锚点或当前航点准备一条命令；失败仍留下可确认凭证。 */
USTRUCT(meta=(DisplayName="AI 准备职责命令", ToolTip="只冻结一个 Home/Route/Attack 意图；成功转执行，失败转职责结果确认。"))
struct COMBAT_API FCombatAIPrepareRoleTask : public FCombatAIPrepareOrderTask
{
	GENERATED_BODY()
	using FInstanceDataType = FCombatAIRoleTaskInstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext&, const FStateTreeTransitionResult&) const override;
	virtual void ExitState(FStateTreeExecutionContext&, const FStateTreeTransitionResult&) const override;
};

/** 保留单写入 Bridge；强制职责/失感知取消也形成完成凭证，归位忽略普通索敌事件。 */
USTRUCT(meta=(DisplayName="AI 执行职责命令", ToolTip="消费准备意图并执行公共 Order；失感知不继续追踪 Actor，完成出口必须确认回执。"))
struct COMBAT_API FCombatAIExecuteRoleTask : public FCombatAIExecuteOrderTask
{
	GENERATED_BODY()
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext&, const FStateTreeTransitionResult&) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext&, float) const override;
};

/** 单次确认角色回执并复核到达；错误进入有界故障处理，不能从 Task Exit 推进游标。 */
USTRUCT(meta=(DisplayName="AI 确认职责结果", ToolTip="成功到点才推进路线或清归位；策略取消不计故障，同一失败只累计一次。"))
struct COMBAT_API FCombatAIResolveRoleTask : public FCombatAIResolveReceiptTask
{
	GENERATED_BODY()
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext&, const FStateTreeTransitionResult&) const override;
};

/** 观察等待不轮询，定时等待使用 Scheduler；退出、接管和死亡取消自身等待。 */
USTRUCT(meta=(DisplayName="AI 职责等待", ToolTip="Guard 等待观察变化；故障超限只接受新职责，退避和航点停留使用 Combat Scheduler。"))
struct COMBAT_API FCombatAIRoleWaitTask : public FCombatAITaskBase
{
	GENERATED_BODY()
	using FInstanceDataType = FCombatAIRoleTaskInstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext&, const FStateTreeTransitionResult&) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext&, float) const override;
	virtual void ExitState(FStateTreeExecutionContext&, const FStateTreeTransitionResult&) const override;
};

/** 提供给 StateTree 有序分支的事实条件，不写工作区、不提交动作。 */
UENUM()
enum class ECombatAIRoleFact : uint8
{
	NeedReturn UMETA(DisplayName="需要归位"),
	CanEngage UMETA(DisplayName="存在可交战目标"),
	HasRoute UMETA(DisplayName="存在未完成航点"),
	RetryPending UMETA(DisplayName="需要故障退避"),
	RetryBlocked UMETA(DisplayName="同操作尝试已用尽")
};

/** 条件实例为共享只读配置，不存放每只单位的可变行为状态。 */
USTRUCT()
struct COMBAT_API FCombatAIRoleConditionData
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, Category="Parameter", meta=(DisplayName="职责事实", ToolTip="读取 Brain 已发布的职责/知识/故障事实，由状态树的顺序和转移决定优先级。")) ECombatAIRoleFact Fact = ECombatAIRoleFact::CanEngage;
};

USTRUCT(meta=(DisplayName="AI 职责事实条件", ToolTip="只读事实条件；无任何命令、计时或副作用。"))
struct COMBAT_API FCombatAIRoleCondition : public FStateTreeConditionCommonBase
{
	GENERATED_BODY()
	using FInstanceDataType = FCombatAIRoleConditionData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual bool Link(FStateTreeLinker&) override;
	virtual bool TestCondition(FStateTreeExecutionContext&) const override;
	TStateTreeExternalDataHandle<UCombatAIBrainComponent> BrainHandle;
};
