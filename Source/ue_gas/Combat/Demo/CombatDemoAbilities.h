#pragma once

#include "CoreMinimal.h"

#include "Combat/Ability/CombatGameplayAbility.h"

#include "CombatDemoAbilities.generated.h"

/** M3 无目标自我治疗纵向切片使用的 Ability Class。 */
UCLASS()
class UE_GAS_API UCombatSelfHealAbility : public UCombatGameplayAbility
{
	GENERATED_BODY()
};

/** M3 敌方单位目标魔法伤害纵向切片使用的 Ability Class。 */
UCLASS()
class UE_GAS_API UCombatUnitDamageAbility : public UCombatGameplayAbility
{
	GENERATED_BODY()
};

/** M3 点目标服务器 AoE 查询纵向切片使用的 Ability Class。 */
UCLASS()
class UE_GAS_API UCombatPointAoeAbility : public UCombatGameplayAbility
{
	GENERATED_BODY()
};

/** M5 Dragon Slave：由 AbilityData 生成可穿透直线弹体并按 special 冻结伤害、宽度、距离与速度。 */
UCLASS()
class UE_GAS_API UCombatDragonSlaveAbility : public UCombatGameplayAbility
{
	GENERATED_BODY()
};

/** M5 Meat Hook：由 AbilityData 生成首命中弹体，依次伤害、施加 Hook Modifier 并请求水平拖拽。 */
UCLASS()
class UE_GAS_API UCombatMeatHookAbility : public UCombatGameplayAbility
{
	GENERATED_BODY()
};

/** M3 Scheduler Channel 自动化使用的可观察 Ability Class。 */
UCLASS()
class UE_GAS_API UCombatChannelProbeAbility : public UCombatGameplayAbility
{
	GENERATED_BODY()

public:
	/** 返回当前实例收到的逻辑 Channel tick 数量。 */
	int32 GetObservedTickCount() const { return ObservedTickCount; }
	/** 返回正常或中断 ChannelFinish 次数。 */
	int32 GetObservedFinishCount() const { return ObservedFinishCount; }
	/** 返回最近一次完成是否为中断。 */
	bool WasLastFinishInterrupted() const { return bLastFinishInterrupted; }

protected:
	/** 累计 TickCount，验证卡顿补帧不随渲染帧率漂移。 */
	virtual void ReceiveChannelTick_Implementation(
		const FCombatAbilityActivationContext& Context,
		const FCombatScheduledTickContext& TickContext) override;
	/** 记录 exactly-once ChannelFinish 与中断标记。 */
	virtual void ReceiveChannelFinish_Implementation(
		const FCombatAbilityActivationContext& Context,
		bool bInterrupted) override;

private:
	/** 当前实例累计的逻辑 tick 数。 */
	int32 ObservedTickCount = 0;
	/** 当前实例累计的 finish 次数。 */
	int32 ObservedFinishCount = 0;
	/** 最近一次 finish 的中断状态。 */
	bool bLastFinishInterrupted = false;
};
