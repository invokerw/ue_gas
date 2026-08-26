#pragma once

#include "CoreMinimal.h"
#include "GameplayEffectTypes.h"
#include "UObject/Object.h"

#include "Combat/Combat/CombatTransactionTypes.h"
#include "Combat/Core/CombatTypes.h"
#include "Combat/Scheduling/CombatSchedulerSubsystem.h"

#include "CombatModifierRuntime.generated.h"

class ACombatUnitCharacter;
class UCombatModifierComponent;
class UCombatModifierData;
class UGameplayEffect;

/** 一个 ActiveGameplayEffect 对应的可扩展 Modifier 运行时对象。 */
UCLASS(Blueprintable, BlueprintType)
class UE_GAS_API UCombatModifierRuntime : public UObject
{
	GENERATED_BODY()

public:
	/** Runtime 与 ActiveGE 建立一一映射后调用一次。 */
	UFUNCTION(BlueprintNativeEvent, Category="Combat|Modifier")
	void OnCreated();
	/** 同定义再次施加并刷新层数/持续时间后调用。 */
	UFUNCTION(BlueprintNativeEvent, Category="Combat|Modifier")
	void OnRefreshed();
	/** ActiveGE 移除前调用一次，供 Runtime 释放内部状态。 */
	UFUNCTION(BlueprintNativeEvent, Category="Combat|Modifier")
	void OnDestroyed();
	/** Combat Scheduler 到达逻辑周期时调用。 */
	UFUNCTION(BlueprintNativeEvent, Category="Combat|Modifier")
	void OnThink(const FCombatScheduledTickContext& TickContext);

	/** 来源单位 Modifier 的伤害增幅 Hook。 */
	UFUNCTION(BlueprintNativeEvent, Category="Combat|Modifier|Damage")
	void OnPreDealDamage(UPARAM(ref) FCombatDamageEvent& Event);
	/** 目标单位抗性前的伤害 Hook。 */
	UFUNCTION(BlueprintNativeEvent, Category="Combat|Modifier|Damage")
	void OnPreTakeDamage(UPARAM(ref) FCombatDamageEvent& Event);
	/** 目标单位抗性后的 Shield/Block Hook。 */
	UFUNCTION(BlueprintNativeEvent, Category="Combat|Modifier|Damage")
	void OnDamageBlock(UPARAM(ref) FCombatDamageEvent& Event);
	/** 来源单位获得真实伤害结果后的 Hook。 */
	UFUNCTION(BlueprintNativeEvent, Category="Combat|Modifier|Damage")
	void OnPostDealDamage(const FCombatDamageEvent& Event);
	/** 目标单位获得真实伤害结果后的 Hook。 */
	UFUNCTION(BlueprintNativeEvent, Category="Combat|Modifier|Damage")
	void OnPostTakeDamage(const FCombatDamageEvent& Event);

	/** 来源单位治疗增幅前的 Hook。 */
	UFUNCTION(BlueprintNativeEvent, Category="Combat|Modifier|Heal")
	void OnPreDealHeal(UPARAM(ref) FCombatHealEvent& Event);
	/** 目标单位接受治疗前的 Hook。 */
	UFUNCTION(BlueprintNativeEvent, Category="Combat|Modifier|Heal")
	void OnPreTakeHeal(UPARAM(ref) FCombatHealEvent& Event);
	/** 来源单位获得真实治疗结果后的 Hook。 */
	UFUNCTION(BlueprintNativeEvent, Category="Combat|Modifier|Heal")
	void OnPostDealHeal(const FCombatHealEvent& Event);
	/** 目标单位获得真实治疗结果后的 Hook。 */
	UFUNCTION(BlueprintNativeEvent, Category="Combat|Modifier|Heal")
	void OnPostTakeHeal(const FCombatHealEvent& Event);

	/** 请求在当前 Hook 阶段结束后移除自身 ActiveGE 与 Runtime。 */
	UFUNCTION(BlueprintCallable, Category="Combat|Modifier")
	bool RequestRemoveSelf();
	/** 读取 ModifierData 的只读 Runtime 参数，缺失时返回默认值。 */
	UFUNCTION(BlueprintPure, Category="Combat|Modifier")
	float GetRuntimeParameter(FName Key, float DefaultValue = 0.0f) const;

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
	/** 返回绝对 World Game Time；0 表示无限持续。 */
	double GetExpireAt() const { return ExpireAt; }
	/** 返回 Think 调度句柄，供调试和刷新相位自动化断言。 */
	FCombatScheduleHandle GetThinkScheduleHandle() const { return ThinkSchedule; }
	/** 返回 Expire 调度句柄，供调试和刷新边界自动化断言。 */
	FCombatScheduleHandle GetExpireScheduleHandle() const { return ExpireSchedule; }
	/** 返回来源单位；来源结束后可能为空。 */
	ACombatUnitCharacter* GetSourceUnit() const { return SourceUnit.Get(); }
	/** 返回承载该 Runtime 的目标单位。 */
	ACombatUnitCharacter* GetTargetUnit() const { return TargetUnit.Get(); }
	/** 返回只读 Modifier 定义。 */
	const UCombatModifierData* GetModifierData() const { return ModifierData; }
	/** 返回 Runtime 是否仍处于 ActiveModifiers 容器中。 */
	bool IsActive() const { return bActive; }

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

private:
	/** 仅 ModifierComponent 可以建立或更新一一映射的内部状态。 */
	friend class UCombatModifierComponent;

	/** 持有该 Runtime 的组件。 */
	UPROPERTY(Transient) TObjectPtr<UCombatModifierComponent> OwningComponent;
	/** 提供静态规则和参数的定义。 */
	UPROPERTY(Transient) TObjectPtr<const UCombatModifierData> ModifierData;
	/** 施加 Modifier 的单位。 */
	TWeakObjectPtr<ACombatUnitCharacter> SourceUnit;
	/** 承载 Modifier 的单位。 */
	TWeakObjectPtr<ACombatUnitCharacter> TargetUnit;
	/** 与 Runtime 一一对应的公共句柄。 */
	FCombatModifierHandle Handle;
	/** Hook 第一稳定排序键。 */
	int32 Priority = 0;
	/** Hook 第二稳定排序键。 */
	uint64 ApplySequence = 0;
	/** 当前聚合层数。 */
	int32 StackCount = 1;
	/** 与 Runtime 同生共死的 ActiveGameplayEffect 句柄。 */
	FActiveGameplayEffectHandle ActiveEffectHandle;
	/** 创建该 ActiveGE 的动态定义，用于精确释放强引用。 */
	UPROPERTY(Transient) TObjectPtr<UGameplayEffect> EffectDefinition;
	/** 0 表示无限，否则为绝对 World Game Time。 */
	double ExpireAt = 0.0;
	/** Think 调度句柄。 */
	FCombatScheduleHandle ThinkSchedule;
	/** 自然过期调度句柄。 */
	FCombatScheduleHandle ExpireSchedule;
	/** 防止销毁路径重复执行。 */
	bool bActive = false;
};
