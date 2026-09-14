#pragma once

#include "CoreMinimal.h"
#include "GameplayAbilitySpec.h"
#include "GameplayTagContainer.h"

#include "Combat/Core/CombatTypes.h"

#include "CombatOrderTypes.generated.h"

class ACombatUnitCharacter;

/** 客户端或 AI 可请求的单位命令类型；客户端只表达意图，指令组件在服务器验证并执行。 */
UENUM(BlueprintType)
enum class ECombatOrderType : uint8
{
	/** 移动到有限世界位置。 */
	MoveToPoint,
	/** 持续更新目的地并移动到目标单位附近；目标死亡或失效时失败。 */
	MoveToUnit,
	/** 目标超出攻击范围时追击，进入范围后重复普攻，直到替换、停止、状态或目标失效结束。 */
	AttackTarget,
	/** 激活无目标 Ability。 */
	CastNoTarget,
	/** 激活点目标 Ability。 */
	CastPoint,
	/** 激活单位目标 Ability。 */
	CastTarget,
	/** 取消当前命令及其异步行为、丢弃全部排队命令，并使旧回调失效；请求不得携带目标或技能。 */
	Stop,
	/** 按稳定实例身份追近地面物品，到达后再次争用拾取。 */
	PickupItem,
	/** 走到指定导航落点后再把原实例移出背包。 */
	DropItem,
	/** 原子交换两槽，不替换移动、攻击或施法命令。 */
	SwapItems
};

/** 服务器指令状态机的阶段；状态用于诊断当前在校验、移动、施法、攻击还是暂停，不表示客户端已获得最终结果。 */
UENUM(BlueprintType)
enum class ECombatOrderState : uint8
{
	/** 尚无当前 Order。 */
	Idle,
	/** 已出队，等待服务器校验。 */
	Validating,
	/** 等待 EQS 解析移动目的点。 */
	Querying,
	/** 等待当前 Controller 的导航 Move 完成。 */
	Moving,
	/** 动态目标追击并定期复核。 */
	Chasing,
	/** 到达后按移动组件的水平转速逐帧转身，服务器复核对准后才开始技能或普攻前摇。 */
	Facing,
	/** 正在调用 ASC 激活 Ability。 */
	DispatchingAbility,
	/** Ability 已进入 cast，等待 OrderReleased。 */
	WaitingOrderRelease,
	/** 正在请求创建 AttackRecord。 */
	StartingAttack,
	/** 单轮已发射，等待 AttackComponent 再次 Ready。 */
	WaitingAttackReady,
	/** 单位状态、强制位移或有界重试等待暂时阻止执行；仍保留当前命令，解除后重新验证。 */
	Paused,
	/** 当前项成功完成。 */
	Completed,
	/** 当前项永久失败。 */
	Failed,
	/** 当前项被 replace、Stop 或生命周期取消。 */
	Cancelled
};

/**
 * 一条命令请求。字段按 Type 互斥：点移动/点技能携带位置，单位移动/攻击/单位技能携带目标，施法携带已授予技能句柄，停止命令不带载荷。
 * 客户端提交的 Actor、位置和技能只作为意图，服务器仍检查所有权、状态、范围、视线及资源。
 */
USTRUCT(BlueprintType)
struct COMBAT_API FCombatOrderRequest
{
	GENERATED_BODY()

	/** 决定 Move、Cast、Attack 或 Stop 状态机分支。 */
	UPROPERTY(BlueprintReadWrite, Category="Combat|Order") ECombatOrderType Type = ECombatOrderType::MoveToPoint;
	/** MoveToUnit、AttackTarget 或 CastTarget 的 Actor 身份。 */
	UPROPERTY(BlueprintReadWrite, Category="Combat|Order") TObjectPtr<ACombatUnitCharacter> TargetUnit = nullptr;
	/** 点移动或点目标技能的世界坐标，单位为厘米；只在 bHasTargetLocation 为 true 时表示调用者明确提供。 */
	UPROPERTY(BlueprintReadWrite, Category="Combat|Order") FVector TargetLocation = FVector::ZeroVector;
	/** 标记 TargetLocation 是否属于请求载荷，避免把合法的世界原点误当作空值。 */
	UPROPERTY(BlueprintReadWrite, Category="Combat|Order") bool bHasTargetLocation = false;
	/** 施法命令引用本单位当前已授予的技能记录；服务器检查它仍存在并重新验证技能条件。 */
	UPROPERTY(BlueprintReadWrite, Category="Combat|Order") FGameplayAbilitySpecHandle AbilitySpecHandle;
	/** 物品操作与物品施法携带的身份和修订；普通命令必须为空。 */
	UPROPERTY(BlueprintReadWrite, Category="Combat|Order", meta=(DisplayName="物品句柄", ToolTip="拾取、丢弃、换位或物品施法的原实例身份。")) FCombatItemHandle ItemHandle;
	UPROPERTY(BlueprintReadWrite, Category="Combat|Order", meta=(DisplayName="物品修订", ToolTip="操作时看到的物品修订；服务器拒绝已变化实例。")) int32 ItemRevision = 0;
	UPROPERTY(BlueprintReadWrite, Category="Combat|Order", meta=(DisplayName="目标槽原物品", ToolTip="换位时目标槽的原句柄；空槽填无效句柄。")) FCombatItemHandle OtherItemHandle;
	UPROPERTY(BlueprintReadWrite, Category="Combat|Order", meta=(DisplayName="来源槽", ToolTip="换位来源索引，0..5 装备、6..8 背包；其他命令为 -1。")) int32 FromItemSlot = INDEX_NONE;
	UPROPERTY(BlueprintReadWrite, Category="Combat|Order", meta=(DisplayName="目标槽", ToolTip="换位目标索引，0..5 装备、6..8 背包；其他命令为 -1。")) int32 ToItemSlot = INDEX_NONE;
	UPROPERTY(BlueprintReadWrite, Category="Combat|Order", meta=(DisplayName="库存修订", ToolTip="换位时看到的整个背包修订；其他命令为 0。")) int32 InventoryRevision = 0;
	/** 仅服务器在安全信封通过后填写，不从网络复制这些关联字段。 */
	int32 RequestCorrelationId = 0;
	int32 RequestOrderIndex = 0;
	int32 RequestControlGeneration = 0;
};

/** 命令的初始接收或最终完成结果。IssueOrder 返回的是初始状态，异步移动、施法和攻击的最终结果通过完成委托另行通知。 */
USTRUCT(BlueprintType)
struct COMBAT_API FCombatOrderResult
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category="Combat|Order", meta=(DisplayName="完成时生命代次", ToolTip="来自原指令句柄的生命身份；拥有者客户端用于淘汰旧生命回执。")) int64 UnitLifeGeneration = 0;

	/** 初始结果中表示命令已进入状态机，不保证动作最终成功；完成结果中表示当前命令达成目标。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Order") bool bSuccess = false;
	/** 本条命令的身份，用于匹配异步回调和最终通知；Stop 成功也返回一个已完成但未登记为活动项的句柄。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Order") FCombatOrderHandle Handle;
	/** 结束时的状态；接受请求时通常为 Validating。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Order") ECombatOrderState State = ECombatOrderState::Idle;
	/** 失败、取消或拒绝时的 Native GameplayTag。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Order") FGameplayTag FailureTag;
	/** 面向日志和自动化的补充文本。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Order") FString Diagnostic;
	UPROPERTY(BlueprintReadOnly, Category="Combat|Order", meta=(DisplayName="原请求", ToolTip="初始接收与最终结果共用的请求编号。")) int32 RequestId = 0;
	UPROPERTY(BlueprintReadOnly, Category="Combat|Order", meta=(DisplayName="批次内序号", ToolTip="从 0 开始对应批次中的请求。")) int32 RequestOrderIndex = 0;
	UPROPERTY(BlueprintReadOnly, Category="Combat|Order", meta=(DisplayName="控制绑定代次", ToolTip="只有与当前控制绑定一致时客户端才显示结果。")) int32 ControlGeneration = 0;
	UPROPERTY(BlueprintReadOnly, Category="Combat|Order", meta=(DisplayName="命令类型", ToolTip="最终完成的是拾取、丢弃、换位或其他命令。")) ECombatOrderType Type = ECombatOrderType::MoveToPoint;
	UPROPERTY(BlueprintReadOnly, Category="Combat|Order", meta=(DisplayName="原物品", ToolTip="用于与玩家操作对应的物品身份快照。")) FCombatItemHandle ItemHandle;
};

/** FIFO 队列中保存的请求、句柄与原始生命代次。 */
USTRUCT()
struct COMBAT_API FCombatQueuedOrder
{
	GENERATED_BODY()

	/** 服务器接受后的不可变请求副本。 */
	FCombatOrderRequest Request;
	/** 为该队列项分配的稳定句柄。 */
	FCombatOrderHandle Handle;
	/** 接受请求时的 Unit life generation。 */
	uint32 UnitLifeGeneration = 0;
};

/** 当前正在执行的命令最终完成、失败或被替换/停止时广播；尚在队列中就被整体清空的命令不会逐条广播。 */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnCombatOrderFinished, const FCombatOrderResult&);
