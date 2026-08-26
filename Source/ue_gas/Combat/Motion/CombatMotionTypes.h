#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

#include "Combat/Core/CombatTypes.h"
#include "Combat/Log/CombatEventSubsystem.h"

#include "CombatMotionTypes.generated.h"

class ACombatUnitCharacter;

/** MotionComponent 的两个独占通道或同时占用。 */
UENUM(BlueprintType)
enum class ECombatMotionChannel : uint8
{
	/** 只修改 XY，保持当前 Z。 */
	Horizontal,
	/** 只修改 Z，保持当前 XY。 */
	Vertical,
	/** 同时占用两个通道并做完整三维移动。 */
	Both
};

/** Motion exactly-once 结束原因。 */
UENUM(BlueprintType)
enum class ECombatMotionFinishReason : uint8
{
	/** 到达目标位置。 */
	Completed,
	/** 被严格更高优先级请求抢占。 */
	Interrupted,
	/** sweep 遇到阻挡。 */
	Blocked,
	/** owner 显式释放。 */
	Cancelled,
	/** Unit 进入死亡清理。 */
	Death,
	/** Component/Actor 正在 EndPlay。 */
	EndPlay
};

/** 向 Unit MotionComponent 提交的不可变请求。 */
USTRUCT(BlueprintType)
struct UE_GAS_API FCombatMotionRequest
{
	GENERATED_BODY()

	/** 请求占用的水平、垂直或全部通道。 */
	UPROPERTY(BlueprintReadWrite, Category="Combat|Motion") ECombatMotionChannel Channel = ECombatMotionChannel::Horizontal;
	/** 严格更高值才能抢占已有通道 owner。 */
	UPROPERTY(BlueprintReadWrite, Category="Combat|Motion") int32 Priority = 0;
	/** Spawn/impact 时快照的世界目标位置。 */
	UPROPERTY(BlueprintReadWrite, Category="Combat|Motion") FVector TargetLocation = FVector::ZeroVector;
	/** 发起位移的来源单位；为空时使用被移动单位。 */
	UPROPERTY(BlueprintReadWrite, Category="Combat|Motion") TObjectPtr<ACombatUnitCharacter> Source = nullptr;
	/** 连续运动速度。 */
	UPROPERTY(BlueprintReadWrite, Category="Combat|Motion", meta=(ClampMin="0", Units="cm/s")) float Speed = 0.0f;
	/** 小于该距离视为完成。 */
	UPROPERTY(BlueprintReadWrite, Category="Combat|Motion", meta=(ClampMin="0", Units="cm")) float StopDistance = 2.0f;
	/** true 时用 CharacterMovement sweep 并在 blocking hit 结束。 */
	UPROPERTY(BlueprintReadWrite, Category="Combat|Motion") bool bSweep = true;
	/** 结束后是否尝试投影回 NavMesh。 */
	UPROPERTY(BlueprintReadWrite, Category="Combat|Motion") bool bProjectToNavigation = true;
	/** 继承 Projectile/Ability 的根事件；为空时 Motion 创建根事件。 */
	UPROPERTY(BlueprintReadWrite, Category="Combat|Motion") FCombatEventContext ParentEvent;
	/** 保留 Ability、Modifier 与 Projectile 的来源定义链。 */
	UPROPERTY(BlueprintReadWrite, Category="Combat|Motion") FCombatSourceContext SourceContext;
};

/** Acquire 与 exactly-once Finish 的结构化结果。 */
USTRUCT(BlueprintType)
struct UE_GAS_API FCombatMotionResult
{
	GENERATED_BODY()

	/** Acquire 是否成功，或 Finish 是否来自活动记录。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Motion") bool bSuccess = false;
	/** 对应 MotionHandle。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Motion") FCombatMotionHandle Handle;
	/** 结束分类。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Motion") ECombatMotionFinishReason FinishReason = ECombatMotionFinishReason::Cancelled;
	/** 失败或阻挡时稳定原因。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Motion") FGameplayTag FailureTag;
	/** 结束时 Unit 的权威位置。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Motion") FVector FinalLocation = FVector::ZeroVector;
};

/** Motion 完成、中断、阻挡或清理后的原生观察委托。 */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnCombatMotionFinished, const FCombatMotionResult&);
