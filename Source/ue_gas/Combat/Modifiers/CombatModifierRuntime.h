#pragma once

#include "CoreMinimal.h"
#include "GameplayAbilitySpec.h"
#include "GameplayEffectTypes.h"
#include "UObject/Object.h"

#include "Combat/Attack/CombatAttackTypes.h"
#include "Combat/Combat/CombatTransactionTypes.h"
#include "Combat/Core/CombatTypes.h"
#include "Combat/Motion/CombatMotionTypes.h"
#include "Combat/Scheduling/CombatSchedulerSubsystem.h"

#include "CombatModifierRuntime.generated.h"

class ACombatUnitCharacter;
class UCombatModifierComponent;
class UCombatModifierData;
class UGameplayEffect;

/**
 * 一个活动 Modifier 的可扩展运行时对象，用于保存护盾余量、每跳伤害等无法只靠 DataAsset 表达的实例状态。
 * 它与一项 Active GameplayEffect 一一对应：属性和标签由 GameplayEffect 聚合，生命周期、周期与 Hook 顺序由 ModifierComponent 管理。
 * 派生类只实现所需的自定义 Hook，并通过公共请求修改战斗状态，不能自行维护第二套属性结果。
 */
UCLASS(Blueprintable, BlueprintType)
class UE_GAS_API UCombatModifierRuntime : public UObject
{
	GENERATED_BODY()

public:
	/** 新 Modifier 的 Runtime 与 Active GameplayEffect 都建立成功后调用一次，可在此初始化实例状态。 */
	UFUNCTION(BlueprintNativeEvent, Category="Combat|Modifier", meta=(DisplayName="Modifier 已创建", ToolTip="新 Modifier 的运行时对象与属性效果都建立成功后调用一次，可在此初始化实例状态。"))
	void OnCreated();
	/** 同一刷新对象被再次施加，且层数、参数、属性幅值与持续时间更新后调用。 */
	UFUNCTION(BlueprintNativeEvent, Category="Combat|Modifier", meta=(DisplayName="Modifier 已刷新", ToolTip="同一 Modifier 实例被再次施加，且层数、参数、属性幅值和持续时间更新后调用。"))
	void OnRefreshed();
	/** Modifier 因到期、驱散、死亡清理或明确移除而结束时，在移除 Active GameplayEffect 前调用一次。 */
	UFUNCTION(BlueprintNativeEvent, Category="Combat|Modifier", meta=(DisplayName="Modifier 将销毁", ToolTip="Modifier 因到期、驱散、死亡清理或明确移除而结束时，在撤销属性效果前调用一次。"))
	void OnDestroyed();
	/** ThinkInterval 大于 0 时由 Combat Scheduler 周期调用；暂停、过期边界和刷新相位由组件统一处理。 */
	UFUNCTION(BlueprintNativeEvent, Category="Combat|Modifier", meta=(DisplayName="Modifier 周期执行", ToolTip="周期触发间隔大于 0 时调用；过期边界和刷新后何时再次触发由 ModifierData 配置。"))
	void OnThink(UPARAM(DisplayName="周期上下文") const FCombatScheduledTickContext& TickContext);

	/** 伤害来源的效果在抗性计算前收到的回调，可调整本次请求数值；不应直接写目标生命。 */
	UFUNCTION(BlueprintNativeEvent, Category="Combat|Modifier|Damage", meta=(DisplayName="造成伤害前", ToolTip="伤害来源的效果在抗性计算前收到的回调，可调整本次请求数值；不应直接写目标生命。"))
	void OnPreDealDamage(UPARAM(ref, DisplayName="伤害事件") FCombatDamageEvent& Event);
	/** 受伤目标的效果在抗性计算前收到的回调，可调整本次请求数值。 */
	UFUNCTION(BlueprintNativeEvent, Category="Combat|Modifier|Damage", meta=(DisplayName="承受伤害前", ToolTip="受伤目标的效果在抗性计算前收到的回调，可调整本次请求数值。"))
	void OnPreTakeDamage(UPARAM(ref, DisplayName="伤害事件") FCombatDamageEvent& Event);
	/** 目标在抗性计算后处理护盾或格挡的回调，可消耗护盾并减少待扣伤害。 */
	UFUNCTION(BlueprintNativeEvent, Category="Combat|Modifier|Damage", meta=(DisplayName="格挡伤害", ToolTip="目标在抗性计算后处理护盾或格挡的回调，可消耗护盾并减少待扣伤害。"))
	void OnDamageBlock(UPARAM(ref, DisplayName="伤害事件") FCombatDamageEvent& Event);
	/** 来源在实际扣血确定后收到的回调；后续吸血等计算应使用实际伤害而非请求值。 */
	UFUNCTION(BlueprintNativeEvent, Category="Combat|Modifier|Damage", meta=(DisplayName="造成伤害后", ToolTip="来源在实际扣血确定后收到的回调；后续吸血等计算应使用实际伤害而非请求值。"))
	void OnPostDealDamage(UPARAM(DisplayName="伤害事件") const FCombatDamageEvent& Event);
	/** 目标在实际扣血确定后收到的回调；可据此请求反伤等后续效果，须沿用事件链限制递归。 */
	UFUNCTION(BlueprintNativeEvent, Category="Combat|Modifier|Damage", meta=(DisplayName="承受伤害后", ToolTip="目标在实际扣血确定后收到的回调；可据此请求反伤等后续效果，须沿用事件链限制递归。"))
	void OnPostTakeDamage(UPARAM(DisplayName="伤害事件") const FCombatDamageEvent& Event);

	/** 来源在治疗增幅计算前收到的回调，可调整治疗请求值。 */
	UFUNCTION(BlueprintNativeEvent, Category="Combat|Modifier|Heal", meta=(DisplayName="造成治疗前", ToolTip="来源在治疗增幅计算前收到的回调，可调整治疗请求值。"))
	void OnPreDealHeal(UPARAM(ref, DisplayName="治疗事件") FCombatHealEvent& Event);
	/** 目标在双方治疗增幅计算后、实际恢复生命前收到的回调，可继续调整待恢复量。 */
	UFUNCTION(BlueprintNativeEvent, Category="Combat|Modifier|Heal", meta=(DisplayName="接受治疗前", ToolTip="目标在双方治疗增幅计算后、实际恢复生命前收到的回调，可继续调整待恢复量。"))
	void OnPreTakeHeal(UPARAM(ref, DisplayName="治疗事件") FCombatHealEvent& Event);
	/** 来源在实际恢复生命确定后收到的回调；请求治疗成功也可能因满血而实际恢复 0。 */
	UFUNCTION(BlueprintNativeEvent, Category="Combat|Modifier|Heal", meta=(DisplayName="造成治疗后", ToolTip="来源在实际恢复生命确定后收到的回调；请求治疗成功也可能因满血而实际恢复 0。"))
	void OnPostDealHeal(UPARAM(DisplayName="治疗事件") const FCombatHealEvent& Event);
	/** 目标在实际恢复生命确定后收到的回调，结果已受到最大生命上限限制。 */
	UFUNCTION(BlueprintNativeEvent, Category="Combat|Modifier|Heal", meta=(DisplayName="接受治疗后", ToolTip="目标在实际恢复生命确定后收到的回调，结果已受到最大生命上限限制。"))
	void OnPostTakeHeal(UPARAM(DisplayName="治疗事件") const FCombatHealEvent& Event);
	/** 来源技能通过生效阶段提交、未被法术格挡且数据动作执行成功后调用；引导技能此时才准备启动引导计时。 */
	UFUNCTION(BlueprintNativeEvent, Category="Combat|Modifier|Ability", meta=(DisplayName="技能已执行", ToolTip="来源技能通过生效阶段提交、未被法术格挡且数据动作执行成功后调用；引导技能此时才准备启动引导计时。"))
	void OnAbilityExecuted(
		UPARAM(DisplayName="技能定义 ID") const FPrimaryAssetId& AbilityDefinitionId,
		UPARAM(DisplayName="事件上下文") const FCombatEventContext& Context);

	/** 返回普攻法球的互斥组名，同组每次攻击最多选中一个效果；None 表示不参与法球竞争。 */
	UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category="Combat|Modifier|Attack", meta=(DisplayName="获取法球互斥组", ToolTip="返回普攻法球的互斥组名，同组每次攻击最多选中一个效果；None 表示不参与法球竞争。"))
	FName GetAttackOrbExclusiveGroup() const;
	/** 只检查这次普攻能否使用本效果的法球；不得在候选检查时扣资源或改变战斗状态。 */
	UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category="Combat|Modifier|Attack", meta=(DisplayName="可声明本次攻击", ToolTip="只检查这次普攻能否使用本效果的法球；不得在候选检查时扣资源或改变战斗状态。"))
	bool CanClaimAttack(UPARAM(DisplayName="攻击候选上下文") const FCombatAttackCandidateContext& Context) const;
	/** 候选轮到本效果时提交资源并填写本次攻击快照；true 表示已提交并锁定互斥组，false 表示未提交，可尝试同组下一个候选。 */
	UFUNCTION(BlueprintNativeEvent, Category="Combat|Modifier|Attack", meta=(DisplayName="法球已声明攻击", ToolTip="候选轮到本效果时提交资源并填写本次攻击快照；true 表示已提交并锁定互斥组，false 表示未提交，可尝试同组下一个候选。"))
	bool OnAttackClaimed(
		UPARAM(DisplayName="攻击候选上下文") const FCombatAttackCandidateContext& Context,
		UPARAM(ref, DisplayName="输出法球快照") FCombatOrbSnapshot& OutSnapshot);
	/** 尝试消耗本效果来格挡一次单位目标技能；true 表示已经格挡并阻止技能动作，false 允许继续寻找其他格挡效果。 */
	UFUNCTION(BlueprintNativeEvent, Category="Combat|Modifier|Ability", meta=(DisplayName="尝试格挡技能", ToolTip="尝试消耗本效果来格挡一次单位目标技能；true 表示已经格挡并阻止技能动作，false 允许继续寻找其他格挡效果。"))
	bool TryBlockAbility(
		UPARAM(DisplayName="技能定义 ID") const FPrimaryAssetId& AbilityDefinitionId,
		UPARAM(DisplayName="施法者") ACombatUnitCharacter* Caster,
		UPARAM(DisplayName="事件上下文") const FCombatEventContext& Context);

	/** 请求结束自身并撤销属性、标签和周期任务；Runtime 已失效或不再由组件持有时返回 false，Hook 内会延迟到遍历结束。 */
	UFUNCTION(BlueprintCallable, Category="Combat|Modifier", meta=(DisplayName="请求移除自身", ToolTip="请求结束自身并撤销属性、标签和周期任务；Runtime 已失效时返回失败，Hook 内会延迟到当前遍历结束。"))
	bool RequestRemoveSelf();
	/** 按“本次 Apply 覆盖值 → ModifierData 默认参数 → DefaultValue”的顺序读取参数。 */
	UFUNCTION(BlueprintPure, Category="Combat|Modifier", meta=(DisplayName="获取 Modifier 运行时参数", ToolTip="优先读取本次施加的同名覆盖值，其次读取 ModifierData 默认参数；都不存在时返回传入的默认值。"))
	float GetRuntimeParameter(
		UPARAM(DisplayName="参数键") FName Key,
		UPARAM(DisplayName="默认值") float DefaultValue = 0.0f) const;

	/** 返回 Runtime 的稳定外部句柄。 */
	FCombatModifierHandle GetHandle() const { return Handle; }
	/** 返回 Hook 稳定排序的 Priority。 */
	int32 GetPriority() const { return Priority; }
	/** 返回 Hook 稳定排序的 ApplySequence。 */
	uint64 GetApplySequence() const { return ApplySequence; }
	/** 返回当前叠加层数。 */
	int32 GetStackCount() const { return StackCount; }
	/** 返回与 Runtime 一一对应的 GAS ActiveGameplayEffect 句柄。 */
	FActiveGameplayEffectHandle GetActiveEffectHandle() const { return ActiveEffectHandle; }
	/** 返回效果的世界游戏时间终点，单位为秒；0 表示无限持续，不是剩余秒数。 */
	double GetExpireAt() const { return ExpireAt; }
	/** 返回最近一次创建或刷新效果的世界游戏时间，单位为秒。 */
	double GetAppliedAt() const { return AppliedAt; }
	/** 返回 Think 调度句柄，供调试和刷新相位自动化断言。 */
	FCombatScheduleHandle GetThinkScheduleHandle() const { return ThinkSchedule; }
	/** 返回 Expire 调度句柄，供调试和刷新边界自动化断言。 */
	FCombatScheduleHandle GetExpireScheduleHandle() const { return ExpireSchedule; }
	/** 返回来源单位；来源结束后可能为空。 */
	ACombatUnitCharacter* GetSourceUnit() const { return SourceUnit.Get(); }
	/** 返回此固有效果所属的已授予技能句柄；普通效果没有该归属，返回无效句柄。 */
	FGameplayAbilitySpecHandle GetAbilityOwnerHandle() const { return AbilityOwnerHandle; }
	/** 返回承载该 Runtime 的目标单位。 */
	ACombatUnitCharacter* GetTargetUnit() const { return TargetUnit.Get(); }
	/** 返回只读 Modifier 定义。 */
	const UCombatModifierData* GetModifierData() const { return ModifierData; }
	/** 返回 Runtime 是否仍处于 ActiveModifiers 容器中。 */
	bool IsActive() const { return bActive; }
	/** 返回 Apply 时是否注入一次性 Motion 请求。 */
	bool HasInitialMotionRequest() const { return bHasInitialMotionRequest; }
	/** 返回新建 Runtime 使用的不可变 Motion 快照。 */
	const FCombatMotionRequest& GetInitialMotionRequest() const { return InitialMotionRequest; }

protected:
	/** C++ 默认生命周期与 Hook 实现均为空，派生类只重写所需阶段。 */
	virtual void OnCreated_Implementation();
	virtual void OnRefreshed_Implementation();
	virtual void OnDestroyed_Implementation();
	virtual void OnThink_Implementation(const FCombatScheduledTickContext& TickContext);
	virtual void OnPreDealDamage_Implementation(FCombatDamageEvent& Event);
	virtual void OnPreTakeDamage_Implementation(FCombatDamageEvent& Event);
	virtual void OnDamageBlock_Implementation(FCombatDamageEvent& Event);
	virtual void OnPostDealDamage_Implementation(const FCombatDamageEvent& Event);
	virtual void OnPostTakeDamage_Implementation(const FCombatDamageEvent& Event);
	virtual void OnPreDealHeal_Implementation(FCombatHealEvent& Event);
	virtual void OnPreTakeHeal_Implementation(FCombatHealEvent& Event);
	virtual void OnPostDealHeal_Implementation(const FCombatHealEvent& Event);
	virtual void OnPostTakeHeal_Implementation(const FCombatHealEvent& Event);
	virtual void OnAbilityExecuted_Implementation(const FPrimaryAssetId& AbilityDefinitionId, const FCombatEventContext& Context);
	/** 默认 Runtime 不是法球且不会提交 AttackRecord。 */
	virtual FName GetAttackOrbExclusiveGroup_Implementation() const;
	virtual bool CanClaimAttack_Implementation(const FCombatAttackCandidateContext& Context) const;
	virtual bool OnAttackClaimed_Implementation(const FCombatAttackCandidateContext& Context, FCombatOrbSnapshot& OutSnapshot);
	/** 默认 Runtime 不提供 SpellBlock。 */
	virtual bool TryBlockAbility_Implementation(const FPrimaryAssetId& AbilityDefinitionId, ACombatUnitCharacter* Caster, const FCombatEventContext& Context);

private:
	/** 仅 ModifierComponent 可以建立或更新一一映射的内部状态。 */
	friend class UCombatModifierComponent;

	/** 持有该 Runtime 的组件。 */
	UPROPERTY(Transient) TObjectPtr<UCombatModifierComponent> OwningComponent;
	/** 提供静态规则和参数的定义。 */
	UPROPERTY(Transient) TObjectPtr<const UCombatModifierData> ModifierData;
	/** 施加 Modifier 的单位。 */
	TWeakObjectPtr<ACombatUnitCharacter> SourceUnit;
	/** 固有效果所属的技能句柄，与来源和定义共同区分刷新对象；普通效果为无效值。 */
	FGameplayAbilitySpecHandle AbilityOwnerHandle;
	/** 承载 Modifier 的单位。 */
	TWeakObjectPtr<ACombatUnitCharacter> TargetUnit;
	/** 与 Runtime 一一对应的公共句柄。 */
	FCombatModifierHandle Handle;
	/** 战斗回调优先级，数值越大越先处理。 */
	int32 Priority = 0;
	/** 施加时分配的递增序号，相同优先级的效果按先施加先处理。 */
	uint64 ApplySequence = 0;
	/** 当前聚合层数。 */
	int32 StackCount = 1;
	/** 与 Runtime 同生共死的 ActiveGameplayEffect 句柄。 */
	FActiveGameplayEffectHandle ActiveEffectHandle;
	/** 创建该 ActiveGE 的动态定义，用于精确释放强引用。 */
	UPROPERTY(Transient) TObjectPtr<UGameplayEffect> EffectDefinition;
	/** 自然过期的世界游戏时间终点，单位为秒；0 表示不会自然过期。 */
	double ExpireAt = 0.0;
	/** 最近一次创建或刷新该 Runtime 的绝对 World Game Time。 */
	double AppliedAt = 0.0;
	/** 周期 OnThink 回调的任务句柄，刷新是否重排由相位策略决定，效果结束时取消。 */
	FCombatScheduleHandle ThinkSchedule;
	/** 自然过期调度句柄。 */
	FCombatScheduleHandle ExpireSchedule;
	/** 防止销毁路径重复执行。 */
	bool bActive = false;
	/** Apply 请求是否携带一次性 Motion 快照。 */
	bool bHasInitialMotionRequest = false;
	/** 只供派生 Runtime 在 OnCreated 消费的强制位移请求。 */
	FCombatMotionRequest InitialMotionRequest;
	/** 最近一次施加或刷新保存的参数覆盖值；读取同名参数时优先于定义默认值。 */
	TMap<FName, float> RuntimeParameterOverrides;
};
