#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Subsystems/WorldSubsystem.h"

#include "Combat/Core/CombatNumericPolicy.h"
#include "Combat/Core/CombatTypes.h"

#include "CombatEventSubsystem.generated.h"

/** Combat 模块统一使用的结构化日志分类。 */
DECLARE_LOG_CATEGORY_EXTERN(LogCombat, Log, All);

/** 描述一条战斗事件在根事件树中的身份和深度。 */
USTRUCT(BlueprintType)
struct COMBAT_API FCombatEventContext
{
	GENERATED_BODY()

	/** 当前节点的唯一事件 ID。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Event") FCombatEventId EventId;
	/** 整条因果链共享的根事件 ID。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Event") FCombatEventId RootEventId;
	/** 根事件为 0，子事件逐层加一。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Event") int32 Depth = 0;

	/** 只检查两个 ID 非零且深度非负；不验证父子关系、事件是否由本 World 分配或是否超过深度上限。 */
	bool IsValid() const { return EventId.IsValid() && RootEventId.IsValid() && Depth >= 0; }
};

/** 可保存到环形诊断缓冲区的一条结构化 Combat 日志。 */
USTRUCT(BlueprintType)
struct COMBAT_API FCombatLogRecord
{
	GENERATED_BODY()

	/** 当前结构化日志字段布局版本，便于离线工具拒绝不兼容记录。 */
	UPROPERTY(BlueprintReadWrite, Category="Combat|Log") int32 SchemaVersion = 3;
	/** 本条记录采用的冻结数值公式版本。 */
	UPROPERTY(BlueprintReadWrite, Category="Combat|Log") int32 FormulaVersion = FCombatNumericPolicyV1::FormulaVersion;
	/** 日志所属的事件树上下文。 */
	UPROPERTY(BlueprintReadWrite, Category="Combat|Log") FCombatEventContext Context;
	/** DamageApplied、UnitDeath 等机器可筛选事件类型。 */
	UPROPERTY(BlueprintReadWrite, Category="Combat|Log") FGameplayTag EventType;
	/** 失败时的机器可判定原因标签。 */
	UPROPERTY(BlueprintReadWrite, Category="Combat|Log") FGameplayTag FailureTag;
	/** 本次事件的稳定来源身份。 */
	UPROPERTY(BlueprintReadWrite, Category="Combat|Log") FCombatSourceContext Source;
	UPROPERTY(BlueprintReadOnly, Category="Combat|Log", meta=(DisplayName="物品操作", ToolTip="物品事件的稳定动作名；其他事件为空。")) FName ItemAction;
	UPROPERTY(BlueprintReadOnly, Category="Combat|Log", meta=(DisplayName="物品数量", ToolTip="变化后的堆叠数量。")) int32 ItemQuantity = 0;
	UPROPERTY(BlueprintReadOnly, Category="Combat|Log", meta=(DisplayName="物品充能", ToolTip="变化后的可用次数，独立于堆叠数量。")) int32 ItemCharges = 0;
	/** 经济事件中本次单一金币变化；购买为负，收入和出售为正。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Log", meta=(DisplayName="金币变化", ToolTip="经济事务造成的精确 int64 金币差值；非经济事件为 0。")) int64 GoldDelta = 0;
	/** 经济事件提交后的单一金币余额。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Log", meta=(DisplayName="金币余额", ToolTip="经济事务完成后的服务器权威余额；非经济事件为 0。")) int64 GoldBalance = 0;
	/** 服务器进程内用于关联来源 Actor 的调试 ID。 */
	UPROPERTY(BlueprintReadWrite, Category="Combat|Log") int32 SourceActorId = 0;
	/** 服务器进程内用于关联目标 Actor 的调试 ID。 */
	UPROPERTY(BlueprintReadWrite, Category="Combat|Log") int32 TargetActorId = 0;
	/** 结果发生时目标单位的生命代次。 */
	UPROPERTY(BlueprintReadWrite, Category="Combat|Log") int64 UnitLifeGeneration = 0;
	/** 事件相关的请求数值：伤害/治疗为原始请求量，其他事件可用于数量、距离或旧队伍值，须结合 EventType 解读。 */
	UPROPERTY(BlueprintReadWrite, Category="Combat|Log") float RequestedAmount = 0.0f;
	/** 伤害事件中抗性或免疫抵消的量；负抗性放大伤害时可为负数，不含护盾吸收。 */
	UPROPERTY(BlueprintReadWrite, Category="Combat|Log") float MitigatedAmount = 0.0f;
	/** 伤害事件中护盾吸收的量。 */
	UPROPERTY(BlueprintReadWrite, Category="Combat|Log") float AbsorbedAmount = 0.0f;
	/** 事件相关的结果数值：伤害/治疗为实际生命变化量，其他事件可表示数量、距离或新队伍值，须结合 EventType 解读。 */
	UPROPERTY(BlueprintReadWrite, Category="Combat|Log") float AppliedAmount = 0.0f;
	/** HPLoss、Reflection、NoLifesteal 等结果标志。 */
	UPROPERTY(BlueprintReadWrite, Category="Combat|Log") FGameplayTagContainer Flags;
	/** 服务器 World Game Time。 */
	UPROPERTY(BlueprintReadWrite, Category="Combat|Log") double ServerTime = 0.0;
	/** 日志提交顺序；与事件 ID 分开计数。 */
	uint64 Sequence = 0;
	/** 面向调试人员的补充诊断文本。 */
	UPROPERTY(BlueprintReadWrite, Category="Combat|Log") FString Diagnostic;

	/** 序列化为包含事件、来源、数值槽和失败原因的单行文本。 */
	FString ToString() const;
};

/** 在一条结构化日志提交后同步通知观察者。 */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnCombatLogRecord, const FCombatLogRecord&);

/** 同步展示观察者的可选资源快照；仅随 Emit 调用存活，不进入核心日志 schema 或持有 gameplay 对象。 */
struct COMBAT_API FCombatLogResourceChange
{
	/** 仅用于玩家展示路由；不属于核心事件 schema，也不进入复制 FastArray。 */
	int32 OwningPlayerId = 0;
	bool bHasHealthChange = false;
	float PreviousHealth = 0.0f;
	float NewHealth = 0.0f;
};

/** 在核心诊断订阅者前通知只读显示投影，确保重入 Emit 仍遵守提交顺序。 */
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnCombatLogPresentationRecord, const FCombatLogRecord&, const FCombatLogResourceChange&);

/**
 * 当前 World 的战斗事件编号和结构化日志中心。根事件表示一次行为的起点，子事件表示由它引发的后续伤害、治疗等；同一链共享根 ID，并限制最大嵌套深度。
 * 日志只保留最近窗口并同步通知订阅者，用于诊断和观察；接口本身不检查服务器权限，调用方负责仅在正确的一端记录权威结果。
 */
UCLASS()
class COMBAT_API UCombatEventSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** 当前结构化 Combat Event 字段布局版本。 */
	static constexpr int32 CurrentSchemaVersion = 3;

	/** 创建深度为 0 且 RootEventId 等于自身的新根事件。 */
	FCombatEventContext CreateRootEvent();
	/** 从有效父事件创建子事件；超过 MaxDepth 时返回无效上下文。 */
	FCombatEventContext CreateChildEvent(const FCombatEventContext& Parent);
	/** 补齐序号和时间后写入最近记录并广播；超限时淘汰最旧记录。 */
	void Emit(FCombatLogRecord Record, const FCombatLogResourceChange& ResourceChange = FCombatLogResourceChange());

	/** 返回当前 World 的只读最近日志缓冲区。 */
	const TArray<FCombatLogRecord>& GetRecentRecords() const { return RecentRecords; }
	/** 返回当前诊断窗口内具有相同根事件 ID 的记录，并保持日志提交顺序；较早记录可能已被淘汰，因此不保证因果链完整。 */
	TArray<FCombatLogRecord> GetRecordsForRootEvent(FCombatEventId RootEventId) const;
	/** 返回 World 生命周期内累计提交的日志数量，不受环形窗口淘汰影响。 */
	uint64 GetTotalEmittedRecordCount() const { return NextLogSequence - 1; }
	/** 返回日志提交委托，供 UI 和测试订阅。 */
	FOnCombatLogRecord& OnRecord() { return RecordDelegate; }
	/** 返回只读玩家展示委托；可选资源值来自真实事务，不允许订阅者驱动 gameplay。 */
	FOnCombatLogPresentationRecord& OnPresentationRecord() { return PresentationDelegate; }

	/** 事件因果链允许的最大深度。 */
	UPROPERTY(EditAnywhere, Category="Combat|Log", meta=(ClampMin="1"))
	int32 MaxDepth = 16;

	/** 当前 World 保留的最近日志条数上限。 */
	UPROPERTY(EditAnywhere, Category="Combat|Log", meta=(ClampMin="16"))
	int32 MaxRecentRecords = 512;

private:
	/** 分配单调递增且非 0 的事件 ID。 */
	FCombatEventId AllocateEventId();

	/** 下一条事件使用的序号。 */
	uint64 NextEventSequence = 1;
	/** 下一条日志记录使用的提交序号。 */
	uint64 NextLogSequence = 1;
	/** 受 MaxRecentRecords 限制的 World 内诊断缓冲区。 */
	UPROPERTY(Transient) TArray<FCombatLogRecord> RecentRecords;
	/** 日志提交后的同步观察者列表。 */
	FOnCombatLogRecord RecordDelegate;
	/** 同步只读投影订阅者，组件 EndPlay 必须显式解绑。 */
	FOnCombatLogPresentationRecord PresentationDelegate;
};
