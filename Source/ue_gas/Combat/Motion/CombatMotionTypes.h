#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

#include "Combat/Core/CombatTypes.h"
#include "Combat/Log/CombatEventSubsystem.h"

#include "CombatMotionTypes.generated.h"

class ACombatUnitCharacter;

/** 强制位移要占用的方向通道；每个单位的水平和垂直通道各只能由一条活动请求控制。 */
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

/** 一次强制位移最终结束的原因；每条活动请求只产生一次结束通知。 */
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

/**
 * 推动单位向固定世界坐标移动的服务器请求。组件按通道和优先级决定是否接受，接受后复制请求并逐帧推进。
 * 请求到达、受阻、被抢占、取消或单位死亡时结束；它只处理连续位移，不附带周期伤害。
 */
USTRUCT(BlueprintType)
struct UE_GAS_API FCombatMotionRequest
{
	GENERATED_BODY()

	/** 选择要控制的方向；水平移动保持当前高度，垂直移动保持当前平面位置，同时占用则三维移动。 */
	UPROPERTY(BlueprintReadWrite, Category="Combat|Motion") ECombatMotionChannel Channel = ECombatMotionChannel::Horizontal;
	/** 竞争所需通道的优先级；新值必须严格高于每个冲突请求才被接受，随后中断这些旧请求。相同值不能抢占。 */
	UPROPERTY(BlueprintReadWrite, Category="Combat|Motion") int32 Priority = 0;
	/** 位移终点的固定世界坐标，单位为厘米；开始后不会继续跟随来源或目标 Actor。 */
	UPROPERTY(BlueprintReadWrite, Category="Combat|Motion") FVector TargetLocation = FVector::ZeroVector;
	/** 产生位移的来源，仅用于事件身份和日志；为空时回退到正在移动的单位，不影响目的地。 */
	UPROPERTY(BlueprintReadWrite, Category="Combat|Motion") TObjectPtr<ACombatUnitCharacter> Source = nullptr;
	/** 沿请求通道前进的速度，单位为厘米/秒，必须有限且大于 0。 */
	UPROPERTY(BlueprintReadWrite, Category="Combat|Motion", meta=(ClampMin="0", Units="cm/s")) float Speed = 0.0f;
	/** 单位与终点在所用通道上的距离不超过此值时完成，单位为厘米，必须有限且非负。 */
	UPROPERTY(BlueprintReadWrite, Category="Combat|Motion", meta=(ClampMin="0", Units="cm")) float StopDistance = 2.0f;
	/** 启用后每步通过 CharacterMovement 扫掠移动，遇到阻挡立即结束；关闭时不做阻挡扫掠，但仍由服务器 CharacterMovement 移动。 */
	UPROPERTY(BlueprintReadWrite, Category="Combat|Motion") bool bSweep = true;
	/** 最后一条强制位移结束后，是否在当前位置附近寻找导航点并校正单位；投影失败时保留当前位置，随后仍重新评估当前命令。 */
	UPROPERTY(BlueprintReadWrite, Category="Combat|Motion") bool bProjectToNavigation = true;
	/** 位移日志所属的技能、弹体或其他父事件；未提供有效事件时，接受请求时创建新的根事件。 */
	UPROPERTY(BlueprintReadWrite, Category="Combat|Motion") FCombatEventContext ParentEvent;
	/** 保留 Ability、Modifier 与 Projectile 的来源定义链。 */
	UPROPERTY(BlueprintReadWrite, Category="Combat|Motion") FCombatSourceContext SourceContext;
};

/** 强制位移获取或最终结束的结果；获取成功只表示已占用通道，单位尚未到达目标。 */
USTRUCT(BlueprintType)
struct UE_GAS_API FCombatMotionResult
{
	GENERATED_BODY()

	/** 获取时表示请求已登记；结束时表示成功关闭活动记录。受阻、被抢占或死亡结束也可能为 true。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Motion") bool bSuccess = false;
	/** 对应 MotionHandle。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Motion") FCombatMotionHandle Handle;
	/** 最终通知中的结束原因；获取结果中的默认值不表示位移已经取消。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Motion") ECombatMotionFinishReason FinishReason = ECombatMotionFinishReason::Cancelled;
	/** 失败或阻挡时稳定原因。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Motion") FGameplayTag FailureTag;
	/** 服务器结束本次位移、发送通知时的单位世界坐标，记录在可选导航落点修正之前；获取请求的返回结果保持零向量。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Motion") FVector FinalLocation = FVector::ZeroVector;
};

/** 强制位移记录移除后的服务器本地通知；同一请求只广播一次，随后才可能恢复原命令。 */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnCombatMotionFinished, const FCombatMotionResult&);
