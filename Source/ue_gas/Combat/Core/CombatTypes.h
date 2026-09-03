#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UObject/PrimaryAssetId.h"

#include "CombatTypes.generated.h"

/** 描述两个战斗队伍之间的外交关系。 */
UENUM(BlueprintType)
enum class ECombatTeamRelation : uint8
{
	/** 双方属于友方关系。 */
	Friendly,
	/** 双方属于敌对关系。 */
	Hostile,
	/** 双方没有友好或敌对关系。 */
	Neutral,
	/** 至少一个队伍标识无效，无法计算关系。 */
	Invalid
};

/** 描述战斗单位从存活到复活的权威生命周期状态。 */
UENUM(BlueprintType)
enum class ECombatLifeState : uint8
{
	/** 单位可以参与正常战斗。 */
	Alive,
	/** 单位正在执行只允许一次的死亡清理。 */
	Dying,
	/** 单位已经死亡且不能参与战斗。 */
	Dead,
	/** 单位正在建立下一次生命。 */
	Respawning
};

/** 标识一次战斗结果最直接的来源类型。 */
UENUM(BlueprintType)
enum class ECombatDirectSourceType : uint8
{
	/** 来源是战斗单位本身。 */
	Unit,
	/** 来源是技能定义。 */
	Ability,
	/** 来源是 Modifier 定义。 */
	Modifier,
	/** 来源是弹体定义。 */
	Projectile,
	/** 来源是一次攻击记录。 */
	Attack
};

/** 决定调度任务落后于当前游戏时间时的补帧策略。 */
UENUM(BlueprintType)
enum class ECombatCatchUpPolicy : uint8
{
	/** 在单任务预算内逐次执行所有到期回调。 */
	ExecuteAllBounded,
	/** 将多个过期 tick 合并成一次回调。 */
	Coalesce,
	/** 跳过已过期 tick，仅推进到未来触发点。 */
	SkipExpired
};

/** 战斗队伍的稳定值类型；0 表示中立，255 表示无效。 */
USTRUCT(BlueprintType)
struct UE_GAS_API FCombatTeamId
{
	GENERATED_BODY()

	/** 中立阵营的保留值。 */
	static constexpr uint8 NeutralValue = 0;
	/** 无效队伍的保留值。 */
	static constexpr uint8 InvalidValue = MAX_uint8;

	/** 队伍的紧凑网络值。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Combat|Team")
	uint8 Value = InvalidValue;

	FCombatTeamId() = default;
	explicit FCombatTeamId(const uint8 InValue) : Value(InValue) {}

	/** 返回当前值是否不是无效保留值。 */
	bool IsValid() const { return Value != InvalidValue; }
	/** 返回当前值是否表示中立阵营。 */
	bool IsNeutralCamp() const { return Value == NeutralValue; }
	/** 返回适合日志输出的稳定文本。 */
	FString ToString() const;

	/** 比较两个队伍标识的紧凑值。 */
	bool operator==(const FCombatTeamId& Other) const { return Value == Other.Value; }
	/** 返回两个队伍标识是否不同。 */
	bool operator!=(const FCombatTeamId& Other) const { return !(*this == Other); }
};

/** 为队伍标识生成容器哈希值。 */
FORCEINLINE uint32 GetTypeHash(const FCombatTeamId& TeamId)
{
	return GetTypeHash(TeamId.Value);
}

/** 单个 World 内单调递增的战斗事件标识。 */
USTRUCT(BlueprintType)
struct UE_GAS_API FCombatEventId
{
	GENERATED_BODY()

	/** 0 表示无效，正数由事件子系统顺序分配。 */
	uint64 Sequence = 0;

	/** 返回事件序号是否已分配。 */
	bool IsValid() const { return Sequence != 0; }
	/** 返回适合日志输出的稳定文本。 */
	FString ToString() const;

	/** 比较两个事件的序号。 */
	bool operator==(const FCombatEventId& Other) const { return Sequence == Other.Sequence; }
	/** 返回两个事件标识是否不同。 */
	bool operator!=(const FCombatEventId& Other) const { return !(*this == Other); }
};

/** 为事件标识生成容器哈希值。 */
FORCEINLINE uint32 GetTypeHash(const FCombatEventId& EventId)
{
	return GetTypeHash(EventId.Sequence);
}

/** 所有异步战斗句柄共享的稳定身份键。 */
USTRUCT(BlueprintType)
struct UE_GAS_API FCombatHandleKey
{
	GENERATED_BODY()

	/** 对象槽位的稳定编号；0 表示未分配。 */
	uint64 Id = 0;

	/** 槽位复用或重排时递增，用于淘汰旧回调。 */
	uint32 Generation = 0;

	/** 可选的单位生命代次，用于淘汰跨死亡/复活回调。 */
	uint32 LifeGeneration = 0;

	/** 返回句柄槽位和 generation 是否均有效。 */
	bool IsValid() const { return Id != 0 && Generation != 0; }
	/** 清空全部身份字段，使所有旧引用立即失效。 */
	void Invalidate() { Id = 0; Generation = 0; LifeGeneration = 0; }
	/** 使用业务类型名生成结构化日志文本。 */
	FString ToString(const TCHAR* TypeName) const;

	/** 比较完整身份键，包括生命代次。 */
	bool operator==(const FCombatHandleKey& Other) const
	{
		return Id == Other.Id && Generation == Other.Generation && LifeGeneration == Other.LifeGeneration;
	}
	/** 返回两个完整身份键是否不同。 */
	bool operator!=(const FCombatHandleKey& Other) const { return !(*this == Other); }
};

/** 为完整身份键生成容器哈希值。 */
FORCEINLINE uint32 GetTypeHash(const FCombatHandleKey& Key)
{
	return HashCombineFast(HashCombineFast(GetTypeHash(Key.Id), GetTypeHash(Key.Generation)), GetTypeHash(Key.LifeGeneration));
}

/** 标识一个 Modifier Runtime 实例。 */
USTRUCT(BlueprintType)
struct UE_GAS_API FCombatModifierHandle
{
	GENERATED_BODY()
	/** Modifier 的共享身份键。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Handle") FCombatHandleKey Key;
	/** 返回 Modifier 句柄是否有效。 */
	bool IsValid() const { return Key.IsValid(); }
	/** 返回适合日志输出的 Modifier 句柄文本。 */
	FString ToString() const { return Key.ToString(TEXT("Modifier")); }
	/** 比较两个 Modifier 句柄的完整身份。 */
	bool operator==(const FCombatModifierHandle& Other) const { return Key == Other.Key; }
};

/** 为 Modifier 句柄生成包含 generation 与生命代次的哈希。 */
FORCEINLINE uint32 GetTypeHash(const FCombatModifierHandle& Handle) { return GetTypeHash(Handle.Key); }

/** 标识一次绑定单位生命代次的攻击记录。 */
USTRUCT(BlueprintType)
struct UE_GAS_API FCombatAttackHandle
{
	GENERATED_BODY()
	/** 攻击记录的共享身份键。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Handle") FCombatHandleKey Key;
	/** 返回攻击句柄及其生命代次是否有效。 */
	bool IsValid() const { return Key.IsValid() && Key.LifeGeneration != 0; }
	/** 返回适合日志输出的攻击句柄文本。 */
	FString ToString() const { return Key.ToString(TEXT("Attack")); }
	/** 比较两个攻击句柄的完整身份。 */
	bool operator==(const FCombatAttackHandle& Other) const { return Key == Other.Key; }
};

/** 为 Attack 句柄生成包含 generation 与生命代次的哈希。 */
FORCEINLINE uint32 GetTypeHash(const FCombatAttackHandle& Handle) { return GetTypeHash(Handle.Key); }

/** 标识一条绑定单位生命代次的 Order。 */
USTRUCT(BlueprintType)
struct UE_GAS_API FCombatOrderHandle
{
	GENERATED_BODY()
	/** Order 的共享身份键。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Handle") FCombatHandleKey Key;
	/** 返回 Order 句柄及其生命代次是否有效。 */
	bool IsValid() const { return Key.IsValid() && Key.LifeGeneration != 0; }
	/** 返回适合日志输出的 Order 句柄文本。 */
	FString ToString() const { return Key.ToString(TEXT("Order")); }
	/** 比较两个 Order 句柄的完整身份。 */
	bool operator==(const FCombatOrderHandle& Other) const { return Key == Other.Key; }
};

/** 为 Order 句柄生成包含 generation 与生命代次的哈希。 */
FORCEINLINE uint32 GetTypeHash(const FCombatOrderHandle& Handle) { return GetTypeHash(Handle.Key); }

/** 标识一个由 Subsystem 持有的弹体实例。 */
USTRUCT(BlueprintType)
struct UE_GAS_API FCombatProjectileHandle
{
	GENERATED_BODY()
	/** 弹体的共享身份键。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Handle") FCombatHandleKey Key;
	/** 返回弹体句柄是否有效。 */
	bool IsValid() const { return Key.IsValid(); }
	/** 返回适合日志输出的弹体句柄文本。 */
	FString ToString() const { return Key.ToString(TEXT("Projectile")); }
	/** 比较两个弹体句柄的完整身份。 */
	bool operator==(const FCombatProjectileHandle& Other) const { return Key == Other.Key; }
	/** 返回两个弹体句柄是否不同。 */
	bool operator!=(const FCombatProjectileHandle& Other) const { return !(*this == Other); }
};

/** 为 Projectile 句柄生成包含 generation 与生命代次的哈希。 */
FORCEINLINE uint32 GetTypeHash(const FCombatProjectileHandle& Handle) { return GetTypeHash(Handle.Key); }

/** 标识一个由 ThinkerSubsystem 持有的权威区域对象。 */
USTRUCT(BlueprintType)
struct UE_GAS_API FCombatThinkerHandle
{
	GENERATED_BODY()
	/** Thinker 的共享身份键。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Handle") FCombatHandleKey Key;
	/** 返回 Thinker 句柄是否有效。 */
	bool IsValid() const { return Key.IsValid(); }
	/** 返回适合日志输出的 Thinker 句柄文本。 */
	FString ToString() const { return Key.ToString(TEXT("Thinker")); }
	/** 比较两个 Thinker 句柄的完整身份。 */
	bool operator==(const FCombatThinkerHandle& Other) const { return Key == Other.Key; }
	/** 返回两个 Thinker 句柄是否不同。 */
	bool operator!=(const FCombatThinkerHandle& Other) const { return !(*this == Other); }
};

/** 为 Thinker 句柄生成包含 generation 与生命代次的哈希。 */
FORCEINLINE uint32 GetTypeHash(const FCombatThinkerHandle& Handle) { return GetTypeHash(Handle.Key); }

/** 标识一个绑定 Unit life generation 的强制位移请求。 */
USTRUCT(BlueprintType)
struct UE_GAS_API FCombatMotionHandle
{
	GENERATED_BODY()
	/** Motion 的共享身份键。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Handle") FCombatHandleKey Key;
	/** 返回 Motion 句柄及其生命代次是否有效。 */
	bool IsValid() const { return Key.IsValid() && Key.LifeGeneration != 0; }
	/** 返回适合日志输出的 Motion 句柄文本。 */
	FString ToString() const { return Key.ToString(TEXT("Motion")); }
	/** 比较两个 Motion 句柄的完整身份。 */
	bool operator==(const FCombatMotionHandle& Other) const { return Key == Other.Key; }
	/** 返回两个 Motion 句柄是否不同。 */
	bool operator!=(const FCombatMotionHandle& Other) const { return !(*this == Other); }
};

/** 为 Motion 句柄生成包含 generation 与生命代次的哈希。 */
FORCEINLINE uint32 GetTypeHash(const FCombatMotionHandle& Handle) { return GetTypeHash(Handle.Key); }

/** 标识一个由 AuraSubsystem 持有并绑定 Owner 生命代次的 Aura。 */
USTRUCT(BlueprintType)
struct UE_GAS_API FCombatAuraHandle
{
	GENERATED_BODY()
	/** Aura 的共享身份键。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Handle") FCombatHandleKey Key;
	/** 返回 Aura 句柄及 Owner 生命代次是否有效。 */
	bool IsValid() const { return Key.IsValid() && Key.LifeGeneration != 0; }
	/** 返回适合日志输出的 Aura 句柄文本。 */
	FString ToString() const { return Key.ToString(TEXT("Aura")); }
	/** 比较两个 Aura 句柄的完整身份。 */
	bool operator==(const FCombatAuraHandle& Other) const { return Key == Other.Key; }
	/** 返回两个 Aura 句柄是否不同。 */
	bool operator!=(const FCombatAuraHandle& Other) const { return !(*this == Other); }
};

/** 为 Aura 句柄生成包含 generation 与生命代次的哈希。 */
FORCEINLINE uint32 GetTypeHash(const FCombatAuraHandle& Handle) { return GetTypeHash(Handle.Key); }

/** 标识一个 Combat Scheduler 槽位。 */
USTRUCT(BlueprintType)
struct UE_GAS_API FCombatScheduleHandle
{
	GENERATED_BODY()
	/** 调度槽位的共享身份键。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Handle") FCombatHandleKey Key;
	/** 返回调度句柄是否有效。 */
	bool IsValid() const { return Key.IsValid(); }
	/** 返回适合日志输出的调度句柄文本。 */
	FString ToString() const { return Key.ToString(TEXT("Schedule")); }
	/** 比较两个调度句柄的完整身份。 */
	bool operator==(const FCombatScheduleHandle& Other) const { return Key == Other.Key; }
	/** 返回两个调度句柄是否不同。 */
	bool operator!=(const FCombatScheduleHandle& Other) const { return !(*this == Other); }
};

/** 为调度句柄生成容器哈希值。 */
FORCEINLINE uint32 GetTypeHash(const FCombatScheduleHandle& Handle)
{
	return GetTypeHash(Handle.Key);
}

/** 可复制、可记录的战斗来源身份快照。 */
USTRUCT(BlueprintType)
struct UE_GAS_API FCombatSourceContext
{
	GENERATED_BODY()

	/** 本次结果最直接的来源类别。 */
	UPROPERTY(BlueprintReadWrite, Category="Combat|Source")
	ECombatDirectSourceType DirectSourceType = ECombatDirectSourceType::Unit;

	/** 关联技能的稳定定义 ID；不适用时为空。 */
	UPROPERTY(BlueprintReadWrite, Category="Combat|Source")
	FPrimaryAssetId AbilityDefinitionId;

	/** 关联 Modifier 的稳定定义 ID；不适用时为空。 */
	UPROPERTY(BlueprintReadWrite, Category="Combat|Source")
	FPrimaryAssetId ModifierDefinitionId;

	/** 关联弹体的稳定定义 ID；不适用时为空。 */
	UPROPERTY(BlueprintReadWrite, Category="Combat|Source")
	FPrimaryAssetId ProjectileDefinitionId;

	/** 比较来源类别和全部稳定定义 ID。 */
	bool operator==(const FCombatSourceContext& Other) const;
};

/** 公共战斗操作的成功状态、失败标签与诊断文本。 */
USTRUCT(BlueprintType)
struct UE_GAS_API FCombatOperationResult
{
	GENERATED_BODY()

	/** 操作是否成功完成。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Result")
	bool bSuccess = false;

	/** 失败时提供机器可判定的原因标签。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Result")
	FGameplayTag FailureTag;

	/** 面向日志和调试人员的补充信息。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Result")
	FString Diagnostic;

	/** 构造不带失败信息的成功结果。 */
	static FCombatOperationResult Success();
	/** 使用指定失败标签和诊断文本构造失败结果。 */
	static FCombatOperationResult Failure(const FGameplayTag& InFailureTag, FString InDiagnostic = FString());
};

/** Scheduler 回调收到的确定性 tick 上下文。 */
USTRUCT(BlueprintType)
struct UE_GAS_API FCombatScheduledTickContext
{
	GENERATED_BODY()

	/** 当前逻辑 tick 原计划触发的 World Game Time。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Scheduling") double ScheduledTime = 0.0;
	/** Scheduler 实际执行回调时的 World Game Time。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Scheduling") double ActualTime = 0.0;
	/** 重复任务的固定间隔；单次任务为 0。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Scheduling") float Interval = 0.0f;
	/** 当前任务从 0 开始的逻辑 tick 序号。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Scheduling") int32 TickIndex = 0;
	/** 本次合并回调代表的逻辑 tick 数量。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Scheduling") int32 TickCount = 1;
};
