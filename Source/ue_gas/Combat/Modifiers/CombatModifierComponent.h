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
	/** 本次持续时间，单位为秒：[-1,0) 使用定义值，0 为无限，正数覆盖定义；小于 -1 被拒绝。负面效果还可能按目标状态抗性缩短。 */
	UPROPERTY(BlueprintReadWrite, Category="Combat|Modifier", meta=(DisplayName="持续时间覆盖", ToolTip="本次持续时间，单位为秒：[-1,0) 使用定义值，0 为无限，正数覆盖定义；小于 -1 被拒绝。负面效果还可能按目标状态抗性缩短。", Units="s")) float DurationOverride = -1.0f;
	/** 固有 Modifier 用它区分所属 AbilitySpec；普通效果保持无效，避免无关施加被误判为同一刷新对象。 */
	UPROPERTY(BlueprintReadWrite, Category="Combat|Modifier", meta=(DisplayName="所属技能句柄", ToolTip="Intrinsic Modifier 使用的 AbilitySpec 所有者键；普通 Modifier 保持无效。")) FGameplayAbilitySpecHandle AbilityOwnerHandle;
	/** 启用后把初始强制位移请求交给新建效果实例；是否执行由该实例的创建回调决定，刷新已有实例不重新注入。 */
	UPROPERTY(BlueprintReadWrite, Category="Combat|Modifier", meta=(DisplayName="包含初始位移请求", ToolTip="启用后把初始强制位移请求交给新建效果实例；是否执行由该实例的创建回调决定，刷新已有实例不重新注入。")) bool bHasInitialMotionRequest = false;
	/** 新建效果可在 OnCreated 中使用的强制位移参数，例如肉钩拉拽；仅在启用初始位移请求时有效。 */
	UPROPERTY(BlueprintReadWrite, Category="Combat|Modifier", meta=(DisplayName="初始位移请求", ToolTip="新建效果可在 OnCreated 中使用的强制位移参数，例如肉钩拉拽；仅在启用初始位移请求时有效。")) FCombatMotionRequest InitialMotionRequest;
	/** 本次施加的参数覆盖；同名键优先于 ModifierData.RuntimeParameters 和属性修改中的默认 Magnitude。 */
	UPROPERTY(BlueprintReadWrite, Category="Combat|Modifier", meta=(DisplayName="运行时参数覆盖", ToolTip="本次施加的参数覆盖；同名键优先于 ModifierData 的运行时参数和属性修改默认幅值。")) TMap<FName, float> RuntimeParameterOverrides;
};

/** 效果施加结果。正常同步调用返回新建或刷新的实例句柄；战斗回调内调用可能只接受排队，此时成功但没有有效句柄。 */
USTRUCT(BlueprintType)
struct UE_GAS_API FCombatModifierApplyResult
{
	GENERATED_BODY()

	/** 同步调用时表示实例创建或刷新成功；战斗回调内表示施加请求已排队，不保证之后能成功，也尚无可用句柄。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Modifier", meta=(DisplayName="成功", ToolTip="同步调用时表示实例创建或刷新成功；战斗回调内表示施加请求已排队，不保证之后能成功，也尚无可用句柄。")) bool bSuccess = false;
	/** true 表示没有创建新实例，而是给同一实例加层并更新参数、幅值和持续时间。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Modifier", meta=(DisplayName="已刷新", ToolTip="启用表示没有创建新实例，而是给同一实例加层并更新参数、属性幅值和持续时间。")) bool bRefreshed = false;
	/** 已同步创建或刷新的实例句柄；仅排队接受时仍无效，不能立即据此查询或移除。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Modifier", meta=(DisplayName="Modifier 句柄", ToolTip="已同步创建或刷新的实例句柄；仅排队接受时仍无效，不能立即据此查询或移除。")) FCombatModifierHandle Handle;
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
	/** 把当前可见效果整理为 UI 所需的定义 ID、层数和时间等值数据；不把运行时 UObject 交给客户端。 */
	void BuildModifierViews(TArray<FCombatModifierView>& OutViews) const;
	/** 返回 Modifier 集合变化观察委托。 */
	FOnCombatModifierCollectionChanged& OnModifierCollectionChanged() { return ModifierCollectionChangedDelegate; }

	/** 在抗性计算前依优先级通知伤害来源的效果，允许调整本次请求伤害；回调中的增删操作延后执行。 */
	void ExecutePreDealDamage(FCombatDamageEvent& Event);
	/** 在抗性计算前依优先级通知受伤目标的效果，允许调整本次请求伤害；回调中的增删操作延后执行。 */
	void ExecutePreTakeDamage(FCombatDamageEvent& Event);
	/** 抗性计算后让目标的护盾或格挡效果依优先级消耗剩余伤害，多个效果按固定顺序处理。 */
	void ExecuteDamageBlock(FCombatDamageEvent& Event);
	/** 实际扣血已确定后通知来源效果；应读取真实结果来计算吸血或其他后续行为。 */
	void ExecutePostDealDamage(const FCombatDamageEvent& Event);
	/** 实际扣血已确定后通知目标效果；反伤等后续请求应继承事件来源并遵守递归限制。 */
	void ExecutePostTakeDamage(const FCombatDamageEvent& Event);
	/** 治疗增幅计算前，按优先级让来源效果调整本次治疗请求。 */
	void ExecutePreDealHeal(FCombatHealEvent& Event);
	/** 来源与目标的治疗增幅已经计算后，按优先级让目标效果继续调整待恢复量；之后才按最大生命限制实际恢复。 */
	void ExecutePreTakeHeal(FCombatHealEvent& Event);
	/** 实际恢复生命确定后通知来源效果；满血时成功结果也可能恢复 0。 */
	void ExecutePostDealHeal(const FCombatHealEvent& Event);
	/** 实际恢复生命确定后通知目标效果，提供经过最大生命限制的真实结果。 */
	void ExecutePostTakeHeal(const FCombatHealEvent& Event);
	/** 技能通过生效阶段提交、未被格挡且公共动作执行成功后，按固定顺序通知来源单位的效果；异步动作此时可能只是成功创建。 */
	void ExecuteAbilityExecuted(const FPrimaryAssetId& AbilityDefinitionId, const FCombatEventContext& Context);
	/**
	 * 先无副作用筛选普攻法球候选，再按优先级尝试提交资源；同一互斥组只接受一个成功候选。
	 * 返回已提交法球的参数快照，供这次攻击后续命中使用，之后切换技能等级或自动施法不改写快照。
	 */
	void ClaimAttackOrbs(const FCombatAttackCandidateContext& Context, TArray<FCombatOrbSnapshot>& OutSnapshots);
	/** 技能生效阶段费用提交后，按优先级尝试消耗一个可格挡该技能的效果；成功返回 true 并停止寻找，技能动作不再执行。 */
	bool TryConsumeSpellBlock(const FPrimaryAssetId& AbilityDefinitionId, ACombatUnitCharacter* Caster, const FCombatEventContext& Context);

	/** 单位开始死亡时移除配置为随死亡清理的效果；其余实例保留属性/标签，暂停战斗回调及计时，复活时再按原到期时间处理。 */
	void HandleOwnerDeath();
	/** 单位复活后清掉保留实例中已经过期的效果，恢复其余实例的周期和过期调度。 */
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
	/** 根据实例记录的绝对到期时刻重排移除任务；周期回调只在要求重置相位或原任务无效时从现在重新计时。 */
	void ScheduleRuntime(UCombatModifierRuntime& Runtime, bool bResetThinkPhase);
	/** 取消 Runtime 持有的全部 Scheduler 句柄。 */
	void CancelRuntimeSchedules(UCombatModifierRuntime& Runtime);
	/** 周期任务到点时调用 OnThink；已越过过期时间时跳过，同刻是否执行由 bTickOnExpire 决定。 */
	void HandleRuntimeThink(FCombatModifierHandle Handle, const FCombatScheduledTickContext& TickContext);
	/** 到达绝对过期时间时走正常移除流程，撤销属性、标签、调度并调用 OnDestroyed。 */
	void HandleRuntimeExpired(FCombatModifierHandle Handle, const FCombatScheduledTickContext& TickContext);
	/** 复制当前活动列表，按优先级从高到低、相同优先级按施加先后排序，使回调中的修改不改变本次遍历顺序。 */
	TArray<UCombatModifierRuntime*> MakeSortedSnapshot() const;
	/** 返回组件所属 Combat Unit。 */
	ACombatUnitCharacter* GetOwnerUnit() const;
	/** 为 Apply/Refresh/Remove 写入包含 DefinitionId、层数与 Handle 的结构化日志。 */
	void EmitModifierLog(const UCombatModifierRuntime& Runtime, bool bRemoved, bool bRefreshed = false) const;

	/** 强引用全部活动 Runtime，直到延迟阶段完整提交。 */
	UPROPERTY(Transient) TArray<TObjectPtr<UCombatModifierRuntime>> ActiveModifiers;
	/** 保存动态 GE 定义，确保 ActiveGameplayEffectSpec 生命周期内不会被 GC。 */
	UPROPERTY(Transient) TArray<TObjectPtr<UGameplayEffect>> RuntimeEffectDefinitions;
	/** 保存战斗事件回调中请求的效果施加、刷新、移除和驱散操作，最外层回调阶段退出后按请求顺序执行。 */
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
