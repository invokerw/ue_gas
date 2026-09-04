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

/**
 * 服务器卡顿使周期任务积压多次触发时，决定本轮如何补执行；一次性任务不使用此策略。
 * 例如每秒一次的任务积压了第 1、2、3 秒三个触发点：逐次补执行最多回调三次，合并只回调一次并报告 TickCount=3，跳过只执行第 3 秒一次。
 */
UENUM(BlueprintType)
enum class ECombatCatchUpPolicy : uint8
{
	/** 按原定触发顺序逐次补执行，每次 TickCount=1；同时受全局、所有者和单任务预算限制，未执行部分留待后续调度。 */
	ExecuteAllBounded,
	/** 把积压周期合为一次回调，TickCount 为积压次数，ScheduledTime 为最早积压时刻；下次触发推进到未来。 */
	Coalesce,
	/** 丢弃较早的积压周期，只对最近一次到期点执行回调，TickCount=1；随后推进到未来触发点。 */
	SkipExpired
};

/** 战斗队伍的稳定编号；0 是中立营地编号，255 无效。营地编号不等于外交关系：是否中立、友军或敌军仍由队伍关系子系统判定。 */
USTRUCT(BlueprintType)
struct UE_GAS_API FCombatTeamId
{
	GENERATED_BODY()

	/** 中立阵营的保留值。 */
	static constexpr uint8 NeutralValue = 0;
	/** 无效队伍的保留值。 */
	static constexpr uint8 InvalidValue = MAX_uint8;

	/** 队伍编号：0 为中立营地，1 到 254 为其他有效队伍，255 无效；编号本身不决定与另一队的外交关系。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Combat|Team", meta=(ClampMin="0", ClampMax="255", DisplayName="队伍 ID", ToolTip="队伍编号：0 为中立营地，1 到 254 为其他有效队伍，255 无效；编号本身不决定与另一队的外交关系。"))
	uint8 Value = InvalidValue;

	FCombatTeamId() = default;
	explicit FCombatTeamId(const uint8 InValue) : Value(InValue) {}

	/** 返回当前值是否不是无效保留值。 */
	bool IsValid() const { return Value != InvalidValue; }
	/** 返回当前值是否表示中立阵营。 */
	bool IsNeutralCamp() const { return Value == NeutralValue; }
	FString ToString() const;

	bool operator==(const FCombatTeamId& Other) const { return Value == Other.Value; }
	bool operator!=(const FCombatTeamId& Other) const { return !(*this == Other); }
};

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

	/** 只检查事件序号非零，不验证是否由事件子系统分配或属于当前 World。 */
	bool IsValid() const { return Sequence != 0; }
	FString ToString() const;

	bool operator==(const FCombatEventId& Other) const { return Sequence == Other.Sequence; }
	bool operator!=(const FCombatEventId& Other) const { return !(*this == Other); }
};

FORCEINLINE uint32 GetTypeHash(const FCombatEventId& EventId)
{
	return GetTypeHash(EventId.Sequence);
}

/**
 * 用于查找异步战斗记录的身份凭证。Id 标识记录，Generation 区分记录版本，LifeGeneration 可区分单位复活前后。
 * IsValid 只检查身份字段是否非零；记录是否仍活动必须向持有它的组件或子系统查询。
 */
USTRUCT(BlueprintType)
struct UE_GAS_API FCombatHandleKey
{
	GENERATED_BODY()

	/** 对象槽位的稳定编号；0 表示未分配。 */
	uint64 Id = 0;

	/** 槽位复用或重排时递增，用于淘汰旧回调。 */
	uint32 Generation = 0;

	/** 关联单位的生命编号；0 表示未绑定。需要生命隔离的持有者以它拒绝复活前的旧请求和回调。 */
	uint32 LifeGeneration = 0;

	/** 仅检查编号和代次非零；不查询记录是否已取消、对象是否还存在。 */
	bool IsValid() const { return Id != 0 && Generation != 0; }
	/** 清空当前这份身份值；不会取消对应任务，也不会修改其他地方保存的句柄副本。 */
	void Invalidate() { Id = 0; Generation = 0; LifeGeneration = 0; }
	/** 使用业务类型名生成结构化日志文本。 */
	FString ToString(const TCHAR* TypeName) const;

	/** 比较完整身份键，包括生命代次。 */
	bool operator==(const FCombatHandleKey& Other) const
	{
		return Id == Other.Id && Generation == Other.Generation && LifeGeneration == Other.LifeGeneration;
	}
	bool operator!=(const FCombatHandleKey& Other) const { return !(*this == Other); }
};

FORCEINLINE uint32 GetTypeHash(const FCombatHandleKey& Key)
{
	return HashCombineFast(HashCombineFast(GetTypeHash(Key.Id), GetTypeHash(Key.Generation)), GetTypeHash(Key.LifeGeneration));
}

/** 标识一个由持续效果组件持有的效果实例；句柄是查询凭证，不延长实例生命。 */
USTRUCT(BlueprintType)
struct UE_GAS_API FCombatModifierHandle
{
	GENERATED_BODY()
	/** Modifier 的共享身份键。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Handle") FCombatHandleKey Key;
	/** 返回 Modifier 句柄是否有效。 */
	bool IsValid() const { return Key.IsValid(); }
	FString ToString() const { return Key.ToString(TEXT("Modifier")); }
	bool operator==(const FCombatModifierHandle& Other) const { return Key == Other.Key; }
};

FORCEINLINE uint32 GetTypeHash(const FCombatModifierHandle& Handle) { return GetTypeHash(Handle.Key); }

/** 标识一次绑定单位生命代次的攻击记录。 */
USTRUCT(BlueprintType)
struct UE_GAS_API FCombatAttackHandle
{
	GENERATED_BODY()
	/** 攻击记录的共享身份键。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Handle") FCombatHandleKey Key;
	/** 只检查编号、版本和生命代次非零，不比较单位当前生命或查询活动记录。 */
	bool IsValid() const { return Key.IsValid() && Key.LifeGeneration != 0; }
	FString ToString() const { return Key.ToString(TEXT("Attack")); }
	bool operator==(const FCombatAttackHandle& Other) const { return Key == Other.Key; }
};

FORCEINLINE uint32 GetTypeHash(const FCombatAttackHandle& Handle) { return GetTypeHash(Handle.Key); }

/** 标识一条绑定单位当前生命的指令；复活后旧指令回调应被拒绝。 */
USTRUCT(BlueprintType)
struct UE_GAS_API FCombatOrderHandle
{
	GENERATED_BODY()
	/** Order 的共享身份键。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Handle") FCombatHandleKey Key;
	/** 返回 Order 句柄及其生命代次是否有效。 */
	bool IsValid() const { return Key.IsValid() && Key.LifeGeneration != 0; }
	FString ToString() const { return Key.ToString(TEXT("Order")); }
	bool operator==(const FCombatOrderHandle& Other) const { return Key == Other.Key; }
};

FORCEINLINE uint32 GetTypeHash(const FCombatOrderHandle& Handle) { return GetTypeHash(Handle.Key); }

/** 标识一个由 Subsystem 持有的弹体实例。 */
USTRUCT(BlueprintType)
struct UE_GAS_API FCombatProjectileHandle
{
	GENERATED_BODY()
	/** 弹体的共享身份键。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Handle") FCombatHandleKey Key;
	/** 只检查句柄身份字段；是否仍有活动记录须向对应持有者查询。 */
	bool IsValid() const { return Key.IsValid(); }
	FString ToString() const { return Key.ToString(TEXT("Projectile")); }
	bool operator==(const FCombatProjectileHandle& Other) const { return Key == Other.Key; }
	bool operator!=(const FCombatProjectileHandle& Other) const { return !(*this == Other); }
};

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
	FString ToString() const { return Key.ToString(TEXT("Thinker")); }
	bool operator==(const FCombatThinkerHandle& Other) const { return Key == Other.Key; }
	bool operator!=(const FCombatThinkerHandle& Other) const { return !(*this == Other); }
};

FORCEINLINE uint32 GetTypeHash(const FCombatThinkerHandle& Handle) { return GetTypeHash(Handle.Key); }

/** 标识一次绑定单位当前生命的强制位移；复活前的请求不能控制复活后的单位。 */
USTRUCT(BlueprintType)
struct UE_GAS_API FCombatMotionHandle
{
	GENERATED_BODY()
	/** Motion 的共享身份键。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Handle") FCombatHandleKey Key;
	/** 返回 Motion 句柄及其生命代次是否有效。 */
	bool IsValid() const { return Key.IsValid() && Key.LifeGeneration != 0; }
	FString ToString() const { return Key.ToString(TEXT("Motion")); }
	bool operator==(const FCombatMotionHandle& Other) const { return Key == Other.Key; }
	bool operator!=(const FCombatMotionHandle& Other) const { return !(*this == Other); }
};

FORCEINLINE uint32 GetTypeHash(const FCombatMotionHandle& Handle) { return GetTypeHash(Handle.Key); }

/** 标识一个由光环子系统维护、绑定产生者当前生命的光环。 */
USTRUCT(BlueprintType)
struct UE_GAS_API FCombatAuraHandle
{
	GENERATED_BODY()
	/** Aura 的共享身份键。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Handle") FCombatHandleKey Key;
	/** 只检查编号、版本和生命代次非零，不比较单位当前生命或查询活动记录。 */
	bool IsValid() const { return Key.IsValid() && Key.LifeGeneration != 0; }
	FString ToString() const { return Key.ToString(TEXT("Aura")); }
	bool operator==(const FCombatAuraHandle& Other) const { return Key == Other.Key; }
	bool operator!=(const FCombatAuraHandle& Other) const { return !(*this == Other); }
};

FORCEINLINE uint32 GetTypeHash(const FCombatAuraHandle& Handle) { return GetTypeHash(Handle.Key); }

/** 标识一个 Combat Scheduler 槽位。 */
USTRUCT(BlueprintType)
struct UE_GAS_API FCombatScheduleHandle
{
	GENERATED_BODY()
	/** 调度槽位的共享身份键。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Handle") FCombatHandleKey Key;
	/** 只检查句柄身份字段；是否仍有活动记录须向对应持有者查询。 */
	bool IsValid() const { return Key.IsValid(); }
	FString ToString() const { return Key.ToString(TEXT("Schedule")); }
	bool operator==(const FCombatScheduleHandle& Other) const { return Key == Other.Key; }
	bool operator!=(const FCombatScheduleHandle& Other) const { return !(*this == Other); }
};

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

/** 一次调度回调的计划时间、实际时间及周期计数；时间采用世界游戏时间，单位为秒，卡顿时计划时间可能早于实际时间。 */
USTRUCT(BlueprintType)
struct UE_GAS_API FCombatScheduledTickContext
{
	GENERATED_BODY()

	/** 本次代表的计划触发时刻，单位为游戏秒；合并策略取最早积压点，跳过策略取最近到期点。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Scheduling") double ScheduledTime = 0.0;
	/** 本轮执行调度时的世界游戏时间，单位为秒；同一轮补执行的多个回调可能共享此值。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Scheduling") double ActualTime = 0.0;
	/** 重复任务的固定间隔；单次任务为 0。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Scheduling") float Interval = 0.0f;
	/** 当前任务从 0 开始的逻辑 tick 序号。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Scheduling") int32 TickIndex = 0;
	/** 本次回调代表的周期数，通常为 1；合并积压策略下可能大于 1，调用方自行决定是否据此累加效果。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Scheduling") int32 TickCount = 1;
};
