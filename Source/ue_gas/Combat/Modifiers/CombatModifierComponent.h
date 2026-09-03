#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayAbilitySpec.h"
#include "GameplayEffectTypes.h"

#include "Combat/Attack/CombatAttackTypes.h"
#include "Combat/Combat/CombatTransactionTypes.h"
#include "Combat/Core/CombatDeferredOperationQueue.h"
#include "Combat/Data/CombatDefinitionData.h"
#include "Combat/Motion/CombatMotionTypes.h"
#include "Combat/View/CombatUnitViewTypes.h"

#include "CombatModifierComponent.generated.h"

class ACombatUnitCharacter;
class UCombatModifierRuntime;
class UGameplayEffect;

/** Modifier 集合创建、刷新或移除后通知 View 与诊断工具。 */
DECLARE_MULTICAST_DELEGATE(FOnCombatModifierCollectionChanged);

/**
 * 调用驱散接口时选择的实际驱散强度，用来与每个 ModifierData 的 DispelRule 比较。
 * Basic 只处理普通可驱散效果，Strong 还能处理要求强驱散的效果；两者都不能移除 NotDispellable。
 */
UENUM(BlueprintType)
enum class ECombatDispelStrength : uint8
{
	/** 普通驱散：只移除 DispelRule=Basic 的 Modifier。 */
	Basic UMETA(DisplayName="普通驱散"),
	/** 强驱散：移除 DispelRule=Basic 或 StrongOnly 的 Modifier。 */
	Strong UMETA(DisplayName="强驱散")
};

/**
 * 向目标的 ModifierComponent 施加效果时使用的服务器请求。
 * 定义资产提供默认行为，本请求提供来源、单次持续时间和参数覆盖；同一来源、定义和 AbilityOwnerHandle 会刷新已有实例。
 */
USTRUCT(BlueprintType)
struct UE_GAS_API FCombatModifierApplyRequest
{
	GENERATED_BODY()

	/** 提供叠层、过期、驱散、属性修改和 Runtime 类型的只读定义资产。 */
	UPROPERTY(BlueprintReadWrite, Category="Combat|Modifier", meta=(DisplayName="Modifier 定义", ToolTip="提供叠层、过期、驱散、属性修改和 Runtime 类型的 Modifier 数据资产。")) TObjectPtr<UCombatModifierData> ModifierData = nullptr;
	/** 施加 Modifier 的来源单位。 */
	UPROPERTY(BlueprintReadWrite, Category="Combat|Modifier", meta=(DisplayName="来源单位", ToolTip="施加该 Modifier 的战斗单位。")) TObjectPtr<ACombatUnitCharacter> Source = nullptr;
	/** 小于 0 使用定义 Duration；0 表示本次无限；正数覆盖定义持续时间。 */
	UPROPERTY(BlueprintReadWrite, Category="Combat|Modifier", meta=(DisplayName="持续时间覆盖", ToolTip="小于 0 使用定义值；0 表示无限；正数覆盖定义持续时间。", Units="s")) float DurationOverride = -1.0f;
	/** 固有 Modifier 用它区分所属 AbilitySpec；普通效果保持无效，避免无关施加被误判为同一刷新对象。 */
	UPROPERTY(BlueprintReadWrite, Category="Combat|Modifier", meta=(DisplayName="所属技能句柄", ToolTip="Intrinsic Modifier 使用的 AbilitySpec 所有者键；普通 Modifier 保持无效。")) FGameplayAbilitySpecHandle AbilityOwnerHandle;
	/** true 时把一次性 Motion 请求注入新建 Runtime。 */
	UPROPERTY(BlueprintReadWrite, Category="Combat|Modifier", meta=(DisplayName="包含初始位移请求", ToolTip="启用后把一次性 Motion 请求注入新建 Runtime。")) bool bHasInitialMotionRequest = false;
	/** Meat Hook 等 Runtime 在 OnCreated 消费的强制位移快照。 */
	UPROPERTY(BlueprintReadWrite, Category="Combat|Modifier", meta=(DisplayName="初始位移请求", ToolTip="Runtime 在创建阶段消费的强制位移快照。")) FCombatMotionRequest InitialMotionRequest;
	/** 本次施加的参数覆盖；同名键优先于 ModifierData.RuntimeParameters 和属性修改中的默认 Magnitude。 */
	UPROPERTY(BlueprintReadWrite, Category="Combat|Modifier", meta=(DisplayName="运行时参数覆盖", ToolTip="本次施加的参数覆盖；同名键优先于 ModifierData 的运行时参数和属性修改默认幅值。")) TMap<FName, float> RuntimeParameterOverrides;
};

/** 返回 Modifier 是否成功新建或刷新，以及调用者后续移除或查询该实例所需的稳定句柄。 */
USTRUCT(BlueprintType)
struct UE_GAS_API FCombatModifierApplyResult
{
	GENERATED_BODY()

	/** 属性效果与运行时对象是否成功创建，或已有实例是否成功刷新。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Modifier", meta=(DisplayName="成功", ToolTip="属性效果与运行时对象是否成功创建，或已有 Modifier 实例是否成功刷新。")) bool bSuccess = false;
	/** true 表示没有创建新实例，而是给同一实例加层并更新参数、幅值和持续时间。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Modifier", meta=(DisplayName="已刷新", ToolTip="启用表示没有创建新实例，而是给同一实例加层并更新参数、属性幅值和持续时间。")) bool bRefreshed = false;
	/** 成功后用于查询或明确移除该 Modifier 实例的句柄。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Modifier", meta=(DisplayName="Modifier 句柄", ToolTip="成功后用于查询或明确移除该 Modifier 实例的稳定句柄。")) FCombatModifierHandle Handle;
	/** 失败时的机器可判定原因。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Modifier", meta=(DisplayName="失败标签", ToolTip="失败时可由程序稳定判定的原因标签。")) FGameplayTag FailureTag;
};

/**
 * 管理一个 Unit 上所有 Modifier 的服务器权威生命周期。
 * 每个活动 Modifier 由一项 Active GameplayEffect 和一个 ModifierRuntime 组成：前者让属性修改与状态标签进入 GAS 聚合，后者保存状态并接收战斗 Hook。
 * 每次 Hook 前先按 Priority 降序、ApplySequence 升序复制活动列表；Hook 中请求的施加、刷新和移除会排队到遍历结束，避免当前回调集合在执行途中变化。
 * 组件通过 Combat Scheduler 维护周期和过期，ASC 聚合结果始终是最终属性来源。
 */
UCLASS(ClassGroup=(Combat), meta=(BlueprintSpawnableComponent))
class UE_GAS_API UCombatModifierComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCombatModifierComponent();

	/**
	 * 仅在服务器为组件所属单位施加 Modifier；相同来源、定义和 Ability owner 已存在时改为刷新。
	 * 校验失败时返回 FailureTag；在 Hook 内调用时只排队，返回成功但暂时没有可用句柄。
	 */
	UFUNCTION(BlueprintCallable, Category="Combat|Modifier", meta=(DisplayName="施加 Modifier", ToolTip="在服务器施加或刷新 Modifier，并返回句柄与失败原因。"))
	FCombatModifierApplyResult ApplyModifier(UPARAM(DisplayName="施加请求") const FCombatModifierApplyRequest& Request);
	/**
	 * 按句柄结束一个 Modifier，同时撤销其属性、标签、周期任务和 Runtime。
	 * 句柄无效或活动实例不存在时返回 false；Hook 内只要句柄格式有效就返回 true 并排队，遍历结束时再确认实例是否仍存在。
	 */
	UFUNCTION(BlueprintCallable, Category="Combat|Modifier", meta=(DisplayName="移除 Modifier", ToolTip="按句柄结束一个 Modifier，并撤销其属性、标签、周期任务和 Runtime；句柄无效或实例不存在时失败，Hook 内调用会延迟到当前遍历结束。"))
	bool RemoveModifier(UPARAM(DisplayName="Modifier 句柄") FCombatModifierHandle Handle);
	/**
	 * 尝试移除强度允许的活动 Modifier，并返回实际移除数量；bDebuffsOnly=true 时只检查负面效果。
	 * State.DispelImmune 会拒绝整次操作，NotDispellable 始终跳过。在 Hook 内调用会延迟执行，因此本次立即返回 0。
	 */
	UFUNCTION(BlueprintCallable, Category="Combat|Modifier", meta=(DisplayName="驱散 Modifier", ToolTip="按所选强度移除允许驱散的 Modifier；可限制为只处理负面效果。驱散免疫会拒绝整次操作，不可驱散效果始终跳过。"))
	int32 Dispel(
		UPARAM(DisplayName="驱散强度") ECombatDispelStrength Strength,
		UPARAM(DisplayName="仅驱散减益") bool bDebuffsOnly = true);
	/** 返回最近一次 Dispel 是否因 State.DispelImmune 被整次拒绝；没有该失败时为空标签。 */
	UFUNCTION(BlueprintPure, Category="Combat|Modifier", meta=(DisplayName="获取最近驱散失败标签", ToolTip="最近一次驱散因 State.DispelImmune 被整次拒绝时返回对应原因，否则为空标签。"))
	FGameplayTag GetLastDispelFailureTag() const { return LastDispelFailureTag; }
	/** 返回当前仍在生效的 Modifier 实例数量；叠层数不额外计为多个实例。 */
	UFUNCTION(BlueprintPure, Category="Combat|Modifier", meta=(DisplayName="获取活动 Modifier 数量", ToolTip="返回当前仍在生效的 Modifier 实例数量；同一实例的叠层不额外计数。"))
	int32 GetActiveModifierCount() const { return ActiveModifiers.Num(); }
	/** 按句柄返回仍在活动容器中的 Runtime；句柄过期、已移除或不存在时返回 nullptr。 */
	UCombatModifierRuntime* FindRuntime(FCombatModifierHandle Handle) const;
	/** 构建不含 Runtime UObject 的稳定 UI View 快照。 */
	void BuildModifierViews(TArray<FCombatModifierView>& OutViews) const;
	/** 返回 Modifier 集合变化观察委托。 */
	FOnCombatModifierCollectionChanged& OnModifierCollectionChanged() { return ModifierCollectionChangedDelegate; }

	/** 依稳定快照执行来源伤害前置 Hook。 */
	void ExecutePreDealDamage(FCombatDamageEvent& Event);
	/** 依稳定快照执行目标伤害前置 Hook。 */
	void ExecutePreTakeDamage(FCombatDamageEvent& Event);
	/** 依稳定快照执行目标 Shield/Block Hook。 */
	void ExecuteDamageBlock(FCombatDamageEvent& Event);
	/** 依稳定快照执行来源伤害后置 Hook。 */
	void ExecutePostDealDamage(const FCombatDamageEvent& Event);
	/** 依稳定快照执行目标伤害后置 Hook。 */
	void ExecutePostTakeDamage(const FCombatDamageEvent& Event);
	/** 依稳定快照执行来源治疗前置 Hook。 */
	void ExecutePreDealHeal(FCombatHealEvent& Event);
	/** 依稳定快照执行目标治疗前置 Hook。 */
	void ExecutePreTakeHeal(FCombatHealEvent& Event);
	/** 依稳定快照执行来源治疗后置 Hook。 */
	void ExecutePostDealHeal(const FCombatHealEvent& Event);
	/** 依稳定快照执行目标治疗后置 Hook。 */
	void ExecutePostTakeHeal(const FCombatHealEvent& Event);
	/** 技能进入 SpellStarted 后向来源 Unit 的 Modifier 稳定派发一次。 */
	void ExecuteAbilityExecuted(const FPrimaryAssetId& AbilityDefinitionId, const FCombatEventContext& Context);
	/** 以两阶段协议按 exclusive group 提交法球 winner，并返回不可变快照。 */
	void ClaimAttackOrbs(const FCombatAttackCandidateContext& Context, TArray<FCombatOrbSnapshot>& OutSnapshots);
	/** SpellStarted commit 后按稳定优先级消耗一个 SpellBlock Runtime。 */
	bool TryConsumeSpellBlock(const FPrimaryAssetId& AbilityDefinitionId, ACombatUnitCharacter* Caster, const FCombatEventContext& Context);

	/** 生命周期进入 Dying 时移除死亡清理 Modifier，并暂停保留 Runtime 的 Hook/Think。 */
	void HandleOwnerDeath();
	/** 生命周期重新 Alive 时恢复未过期保留 Runtime 的调度。 */
	void HandleOwnerRespawn();
	/** Actor EndPlay 时无 Hook 地清理全部 ActiveGE 与调度。 */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	/** 先同步刷新所属 Unit 的复制 View，再通知其他诊断观察者。 */
	void NotifyModifierCollectionChanged();
	/** 创建动态无限 ActiveGE 并建立 Runtime 一一映射。 */
	FCombatModifierApplyResult ApplyNewModifier(const FCombatModifierApplyRequest& Request, float EffectiveDuration);
	/** 复用现有句柄，增加层数并替换参数和属性幅值，再按策略更新过期与周期计时。 */
	FCombatModifierApplyResult RefreshModifier(
		UCombatModifierRuntime& Runtime,
		const FCombatModifierApplyRequest& Request,
		float EffectiveDuration);
	/** 立即移除一一映射；调用者负责确保不在 Hook 遍历中。 */
	bool RemoveModifierImmediate(FCombatModifierHandle Handle, bool bCallDestroyed = true);
	/** 查找来源、ModifierData 和 AbilityOwnerHandle 都相同的活动实例，作为本次施加的刷新对象。 */
	UCombatModifierRuntime* FindRefreshCandidate(const FCombatModifierApplyRequest& Request) const;
	/** 按状态抗性计算本次持续时间快照。 */
	float CalculateEffectiveDuration(const FCombatModifierApplyRequest& Request) const;
	/** 总是按新 ExpireAt 重排过期任务；仅在要求重置或没有有效任务时重新开始 OnThink 周期。 */
	void ScheduleRuntime(UCombatModifierRuntime& Runtime, bool bResetThinkPhase);
	/** 取消 Runtime 持有的全部 Scheduler 句柄。 */
	void CancelRuntimeSchedules(UCombatModifierRuntime& Runtime);
	/** 周期任务到点时调用 OnThink；已越过过期时间时跳过，同刻是否执行由 bTickOnExpire 决定。 */
	void HandleRuntimeThink(FCombatModifierHandle Handle, const FCombatScheduledTickContext& TickContext);
	/** 到达绝对过期时间时走正常移除流程，撤销属性、标签、调度并调用 OnDestroyed。 */
	void HandleRuntimeExpired(FCombatModifierHandle Handle, const FCombatScheduledTickContext& TickContext);
	/** 返回 Priority desc、ApplySequence asc 的活动快照。 */
	TArray<UCombatModifierRuntime*> MakeSortedSnapshot() const;
	/** 返回组件所属 Combat Unit。 */
	ACombatUnitCharacter* GetOwnerUnit() const;
	/** 为 Apply/Refresh/Remove 写入包含 DefinitionId、层数与 Handle 的结构化日志。 */
	void EmitModifierLog(const UCombatModifierRuntime& Runtime, bool bRemoved, bool bRefreshed = false) const;

	/** 强引用全部活动 Runtime，直到延迟阶段完整提交。 */
	UPROPERTY(Transient) TArray<TObjectPtr<UCombatModifierRuntime>> ActiveModifiers;
	/** 保存动态 GE 定义，确保 ActiveGameplayEffectSpec 生命周期内不会被 GC。 */
	UPROPERTY(Transient) TArray<TObjectPtr<UGameplayEffect>> RuntimeEffectDefinitions;
	/** Hook 中的 Apply/Remove/Refresh/Purge FIFO 延迟队列。 */
	FCombatDeferredOperationQueue DeferredOperations;
	/** 下一个 Modifier 句柄 ID。 */
	uint64 NextHandleId = 1;
	/** 下一个 Hook 稳定施加序号。 */
	uint64 NextApplySequence = 1;
	/** 非 Alive 阶段禁止 Hook 与 Think。 */
	bool bHooksPaused = false;
	/** 最近一次 Dispel 的高级状态拒绝原因；成功或正常无目标时为空。 */
	FGameplayTag LastDispelFailureTag;
	/** View 与诊断工具订阅的集合变化委托。 */
	FOnCombatModifierCollectionChanged ModifierCollectionChangedDelegate;
};
