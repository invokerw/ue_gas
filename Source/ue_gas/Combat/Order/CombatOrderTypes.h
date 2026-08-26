#pragma once

#include "CoreMinimal.h"
#include "GameplayAbilitySpec.h"
#include "GameplayTagContainer.h"

#include "Combat/Core/CombatTypes.h"

#include "CombatOrderTypes.generated.h"

class ACombatUnitCharacter;

/** 玩家输入、AI 意图和战斗执行共用的服务器 Order 类型。 */
UENUM(BlueprintType)
enum class ECombatOrderType : uint8
{
	/** 移动到有限世界位置。 */
	MoveToPoint,
	/** 移动到当前单位位置。 */
	MoveToUnit,
	/** 持续追击并普通攻击目标。 */
	AttackTarget,
	/** 激活无目标 Ability。 */
	CastNoTarget,
	/** 激活点目标 Ability。 */
	CastPoint,
	/** 激活单位目标 Ability。 */
	CastTarget,
	/** 提升 generation 并取消全部当前与排队行为。 */
	Stop
};

/** 当前 Order 只允许由 PumpCurrentOrder 迁移的状态。 */
UENUM(BlueprintType)
enum class ECombatOrderState : uint8
{
	/** 尚无当前 Order。 */
	Idle,
	/** 已出队，等待服务器校验。 */
	Validating,
	/** 等待 EQS 解析移动目的点。 */
	Querying,
	/** 等待 AI Move 完成。 */
	Moving,
	/** 动态目标追击并定期复核。 */
	Chasing,
	/** 到达后正在服务器设置并复核朝向。 */
	Facing,
	/** 正在调用 ASC 激活 Ability。 */
	DispatchingAbility,
	/** Ability 已进入 cast，等待 OrderReleased。 */
	WaitingOrderRelease,
	/** 正在请求创建 AttackRecord。 */
	StartingAttack,
	/** 单轮已发射，等待 AttackComponent 再次 Ready。 */
	WaitingAttackReady,
	/** 临时状态阻止，保留当前队首等待恢复。 */
	Paused,
	/** 当前项成功完成。 */
	Completed,
	/** 当前项永久失败。 */
	Failed,
	/** 当前项被 replace、Stop 或生命周期取消。 */
	Cancelled
};

/** 客户端允许提交、但服务器必须完整复核的最小 Order payload。 */
USTRUCT(BlueprintType)
struct UE_GAS_API FCombatOrderRequest
{
	GENERATED_BODY()

	/** 决定 Move、Cast、Attack 或 Stop 状态机分支。 */
	UPROPERTY(BlueprintReadWrite, Category="Combat|Order") ECombatOrderType Type = ECombatOrderType::MoveToPoint;
	/** MoveToUnit、AttackTarget 或 CastTarget 的 Actor 身份。 */
	UPROPERTY(BlueprintReadWrite, Category="Combat|Order") TObjectPtr<ACombatUnitCharacter> TargetUnit = nullptr;
	/** MoveToPoint 或 CastPoint 的有限服务器复核位置。 */
	UPROPERTY(BlueprintReadWrite, Category="Combat|Order") FVector TargetLocation = FVector::ZeroVector;
	/** 区分合法世界原点与未提交位置。 */
	UPROPERTY(BlueprintReadWrite, Category="Combat|Order") bool bHasTargetLocation = false;
	/** Cast Order 必须引用本单位已授予的 AbilitySpec。 */
	UPROPERTY(BlueprintReadWrite, Category="Combat|Order") FGameplayAbilitySpecHandle AbilitySpecHandle;
};

/** OrderComponent 接受请求或完成当前项时返回的结构化结果。 */
USTRUCT(BlueprintType)
struct UE_GAS_API FCombatOrderResult
{
	GENERATED_BODY()

	/** 请求是否成功接受或当前项是否成功完成。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Order") bool bSuccess = false;
	/** 对应的稳定 OrderHandle。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Order") FCombatOrderHandle Handle;
	/** 结束时的状态；接受请求时通常为 Validating。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Order") ECombatOrderState State = ECombatOrderState::Idle;
	/** 失败、取消或拒绝时的 Native GameplayTag。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Order") FGameplayTag FailureTag;
	/** 面向日志和自动化的补充文本。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Order") FString Diagnostic;
};

/** FIFO 队列中保存的请求、句柄与原始生命代次。 */
USTRUCT()
struct UE_GAS_API FCombatQueuedOrder
{
	GENERATED_BODY()

	/** 服务器接受后的不可变请求副本。 */
	FCombatOrderRequest Request;
	/** 为该队列项分配的稳定句柄。 */
	FCombatOrderHandle Handle;
	/** 接受请求时的 Unit life generation。 */
	uint32 UnitLifeGeneration = 0;
};

/** 每个 Order 完成、失败或取消后广播的原生结果。 */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnCombatOrderFinished, const FCombatOrderResult&);
