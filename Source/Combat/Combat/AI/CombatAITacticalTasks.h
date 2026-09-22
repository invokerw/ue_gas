#pragma once

#include "Combat/AI/CombatAIStateTreeTasks.h"
#include "StateTreeConditionBase.h"
#include "StateTreeConsiderationBase.h"
#include "CombatAITacticalTasks.generated.h"

/** 战术节点的只读动作参数与单次激活身份。 */
USTRUCT()
struct COMBAT_API FCombatAITacticalTaskInstanceData : public FCombatAITaskInstanceData
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, Category="Parameter", meta=(DisplayName="战术动作", ToolTip="选择要评估、准备或执行的 Utility 分支；合法性由对应 Condition 复核。"))
	ECombatAITacticalAction Action = ECombatAITacticalAction::Guard;
};

/** 在 Utility 选择前发布一次无副作用候选快照。 */
USTRUCT(meta=(DisplayName="AI 评估战术候选", ToolTip="解析已授予主动技能并调用公共预检；不激活技能、不扣费、不提交 Order。"))
struct COMBAT_API FCombatAIEvaluateTacticsTask : public FCombatAITaskBase
{
	GENERATED_BODY()
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext&, const FStateTreeTransitionResult&) const override;
};

/** 将 Utility 选中的 Cast 或 Attack 冻结到共享 PreparedIntent。 */
USTRUCT(meta=(DisplayName="AI 准备战术命令", ToolTip="消费已发布的战术快照并再次公共预检；失败仍由结果分支有界处理。"))
struct COMBAT_API FCombatAIPrepareTacticalTask : public FCombatAIPrepareOrderTask
{
	GENERATED_BODY()
	using FInstanceDataType = FCombatAITacticalTaskInstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext&, const FStateTreeTransitionResult&) const override;
	virtual void ExitState(FStateTreeExecutionContext&, const FStateTreeTransitionResult&) const override;
};

/** 取得 World 配额后运行一次可取消 EQS；回调只投递邮箱，安全 Tick 才生成 Move 意图。 */
USTRUCT(meta=(DisplayName="AI 查询战术站位", ToolTip="同单位最多一个在途查询；预算不足由 Combat Scheduler 延期，成功点仍走公共 Move Order。"))
struct COMBAT_API FCombatAIQueryTacticalLocationTask : public FCombatAIPrepareOrderTask
{
	GENERATED_BODY()
	using FInstanceDataType = FCombatAITacticalTaskInstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext&, const FStateTreeTransitionResult&) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext&, float) const override;
	virtual void ExitState(FStateTreeExecutionContext&, const FStateTreeTransitionResult&) const override;
};

/** 执行战术命令；Attack 在普通边界复核切换，Cast 只等待公共 OrderReleased。 */
USTRUCT(meta=(DisplayName="AI 执行战术命令", ToolTip="保持单一 Order 写入者；普通攻击仅在真实执行边界切换到更高效用分支。"))
struct COMBAT_API FCombatAIExecuteTacticalTask : public FCombatAIExecuteOrderTask
{
	GENERATED_BODY()
	using FInstanceDataType = FCombatAITacticalTaskInstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext&, const FStateTreeTransitionResult&) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext&, float) const override;
};

/** 按动作类别确认通用或持续攻击回执，保证每个终态只消费一次。 */
USTRUCT(meta=(DisplayName="AI 确认战术结果", ToolTip="确认一次 Cast/Attack 终态；不重复提交战斗结算。"))
struct COMBAT_API FCombatAIResolveTacticalTask : public FCombatAIResolveReceiptTask
{
	GENERATED_BODY()
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext&, const FStateTreeTransitionResult&) const override;
};

/** Utility 分支的合法性条件；零分不能代替此条件。 */
USTRUCT()
struct COMBAT_API FCombatAITacticalConditionData
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, Category="Parameter", meta=(DisplayName="战术动作", ToolTip="只读取已发布快照和当前公共事实，不启动查询或命令。"))
	ECombatAITacticalAction Action = ECombatAITacticalAction::Guard;
};

USTRUCT(meta=(DisplayName="AI 战术动作可用", ToolTip="只读判断分支是否合法；不会用效用 0 代替非法状态。"))
struct COMBAT_API FCombatAITacticalCondition : public FStateTreeConditionCommonBase
{
	GENERATED_BODY()
	using FInstanceDataType = FCombatAITacticalConditionData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual bool Link(FStateTreeLinker&) override;
	virtual bool TestCondition(FStateTreeExecutionContext&) const override;
	TStateTreeExternalDataHandle<UCombatAIBrainComponent> BrainHandle;
};

/** StateTree 实验性 Utility API 的薄适配，只返回纯评分层已发布的有界分数。 */
USTRUCT()
struct COMBAT_API FCombatAITacticalConsiderationData
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, Category="Parameter", meta=(DisplayName="战术动作", ToolTip="读取对应 Cast、Attack、Reposition 或 Guard 分数。"))
	ECombatAITacticalAction Action = ECombatAITacticalAction::Guard;
};

USTRUCT(meta=(DisplayName="AI 战术效用", ToolTip="读取 Brain 的只读战术快照；合法性必须由 Enter Condition 单独表达。"))
struct COMBAT_API FCombatAITacticalConsideration : public FStateTreeConsiderationBase
{
	GENERATED_BODY()
	using FInstanceDataType = FCombatAITacticalConsiderationData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual bool Link(FStateTreeLinker&) override;
	TStateTreeExternalDataHandle<UCombatAIBrainComponent> BrainHandle;

protected:
	virtual float GetScore(FStateTreeExecutionContext&) const override;
};
