#pragma once

#include "CoreMinimal.h"
#include "Combat/Order/CombatOrderTypes.h"
#include "Combat/AI/CombatAIRoleTypes.h"
#include "CombatAITypes.generated.h"

/** 只读观察的身份和版本；不复制 Health/Mana/冷却等权威状态。工作区由 Brain 独立持有。 */
USTRUCT()
struct COMBAT_API FCombatAIContext
{
	GENERATED_BODY()
	UPROPERTY(VisibleAnywhere, Category="AI", meta=(DisplayName="自身单位", ToolTip="当前运行所属单位；仅用于从公共组件读取权威状态。")) TObjectPtr<ACombatUnitCharacter> Unit;
	UPROPERTY(VisibleAnywhere, Category="AI", meta=(DisplayName="目标修订", ToolTip="每次显式设置目标后递增；准备与消费必须使用同一修订。")) uint64 ObjectiveRevision = 0;
};

/** 本地唤醒载荷，不进入 Combat 网络事件或 GameplayTag schema；事实仍以 Brain 工作区为准。 */
USTRUCT()
struct COMBAT_API FCombatAIWakeSignal
{
	GENERATED_BODY()
	UPROPERTY() uint64 Run = 0;
};

/** 一次决策的所有权；树运行、控制权、生命和作用域必须全部匹配。 */
struct COMBAT_API FCombatAIDecisionScope
{
	uint64 Run = 0;
	uint64 ControlEpoch = 0;
	uint32 Life = 0;
	uint64 Serial = 0;
	bool IsValid() const { return Run && ControlEpoch && Life && Serial; }
	bool operator==(const FCombatAIDecisionScope& Other) const
	{
		return Run == Other.Run && ControlEpoch == Other.ControlEpoch && Life == Other.Life && Serial == Other.Serial;
	}
};

/** 跨兄弟状态的单次准备结果；目标使用弱引用，Request 中不保留 Actor 强引用。 */
struct COMBAT_API FCombatAIPreparedIntent
{
	FCombatAIDecisionScope Scope;
	uint64 Preparation = 0;
	uint64 Producer = 0;
	uint64 ObjectiveRevision = 0;
	FName ConsumerSlot;
	FCombatOrderRequest Request;
	TWeakObjectPtr<ACombatUnitCharacter> Target;
	uint32 TargetLife = 0;
	double ExpiresAt = 0;
	bool bHadTarget = false;
	/** 角色意图的到达/重试类别；显式目标保持原协议。 */
	ECombatAIRoleOperation Operation = ECombatAIRoleOperation::Explicit;
	int32 RouteIndex = INDEX_NONE;
};

/** 动作终态或准备失败的唯一凭证；退出执行 Task 后保留，Resolve 确认后清除。 */
struct COMBAT_API FCombatAICompletionReceipt
{
	FCombatAIDecisionScope Scope;
	uint64 Preparation = 0;
	uint64 Activation = 0;
	uint64 ObjectiveRevision = 0;
	FCombatOrderResult Result;
	bool bValid = false;
	ECombatAIRoleOperation Operation = ECombatAIRoleOperation::Explicit;
	ECombatAIRoleStopReason StopReason = ECombatAIRoleStopReason::None;
	int32 RouteIndex = INDEX_NONE;
};
