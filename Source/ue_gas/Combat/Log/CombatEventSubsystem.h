#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Subsystems/WorldSubsystem.h"

#include "Combat/Core/CombatTypes.h"

#include "CombatEventSubsystem.generated.h"

/** Combat 模块统一使用的结构化日志分类。 */
DECLARE_LOG_CATEGORY_EXTERN(LogCombat, Log, All);

/** 描述一条战斗事件在根事件树中的身份和深度。 */
USTRUCT(BlueprintType)
struct UE_GAS_API FCombatEventContext
{
	GENERATED_BODY()

	/** 当前节点的唯一事件 ID。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Event") FCombatEventId EventId;
	/** 整条因果链共享的根事件 ID。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Event") FCombatEventId RootEventId;
	/** 根事件为 0，子事件逐层加一。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Event") int32 Depth = 0;

	/** 检查当前 ID、根 ID 与深度是否组成有效上下文。 */
	bool IsValid() const { return EventId.IsValid() && RootEventId.IsValid() && Depth >= 0; }
};

/** 可保存到环形诊断缓冲区的一条结构化 Combat 日志。 */
USTRUCT(BlueprintType)
struct UE_GAS_API FCombatLogRecord
{
	GENERATED_BODY()

	/** 日志所属的事件树上下文。 */
	UPROPERTY(BlueprintReadWrite, Category="Combat|Log") FCombatEventContext Context;
	/** DamageApplied、UnitDeath 等机器可筛选事件类型。 */
	UPROPERTY(BlueprintReadWrite, Category="Combat|Log") FGameplayTag EventType;
	/** 失败时的机器可判定原因标签。 */
	UPROPERTY(BlueprintReadWrite, Category="Combat|Log") FGameplayTag FailureTag;
	/** 本次事件的稳定来源身份。 */
	UPROPERTY(BlueprintReadWrite, Category="Combat|Log") FCombatSourceContext Source;
	/** 服务器 World Game Time。 */
	UPROPERTY(BlueprintReadWrite, Category="Combat|Log") double ServerTime = 0.0;
	/** 日志提交顺序；与事件 ID 分开计数。 */
	uint64 Sequence = 0;
	/** 面向调试人员的补充诊断文本。 */
	UPROPERTY(BlueprintReadWrite, Category="Combat|Log") FString Diagnostic;

	/** 序列化为包含事件、来源和失败原因的单行文本。 */
	FString ToString() const;
};

/** 在一条结构化日志提交后同步通知观察者。 */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnCombatLogRecord, const FCombatLogRecord&);

/** 为当前 World 分配事件 ID、维护因果深度并广播结构化日志。 */
UCLASS()
class UE_GAS_API UCombatEventSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** 创建深度为 0 且 RootEventId 等于自身的新根事件。 */
	FCombatEventContext CreateRootEvent();
	/** 从有效父事件创建子事件；超过 MaxDepth 时返回无效上下文。 */
	FCombatEventContext CreateChildEvent(const FCombatEventContext& Parent);
	/** 补齐序号和时间后写入最近记录并广播；超限时淘汰最旧记录。 */
	void Emit(FCombatLogRecord Record);

	/** 返回当前 World 的只读最近日志缓冲区。 */
	const TArray<FCombatLogRecord>& GetRecentRecords() const { return RecentRecords; }
	/** 返回日志提交委托，供 UI 和测试订阅。 */
	FOnCombatLogRecord& OnRecord() { return RecordDelegate; }

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
};
