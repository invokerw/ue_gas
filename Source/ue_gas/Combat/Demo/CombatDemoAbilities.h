#pragma once

#include "CoreMinimal.h"

#include "Combat/Ability/CombatGameplayAbility.h"

#include "CombatDemoAbilities.generated.h"

class ACombatFissureBlocker;
class UCombatModifierData;

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

/** M6 Frost Arrows：Passive/Attack/AutoCast Ability，实际法球行为由 Intrinsic Runtime 提供。 */
UCLASS()
class UE_GAS_API UCombatFrostArrowsAbility : public UCombatGameplayAbility
{
	GENERATED_BODY()
};

/** M6 Earthshaker Fissure：统一执行线伤、Stun、Knockback、视觉 Thinker 与物理 blocker。 */
UCLASS()
class UE_GAS_API UCombatFissureAbility : public UCombatGameplayAbility
{
	GENERATED_BODY()

public:
	/** 默认 blocker 使用无 Tick 的 ACombatFissureBlocker。 */
	UCombatFissureAbility();
	/** Spec 授予时把 Fissure 专属 CDO 引用同步到 InstancedPerActor 实例。 */
	virtual void OnGiveAbility(
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilitySpec& Spec) override;
	/** 返回最近一次服务器线段去重后的目标数。 */
	UFUNCTION(BlueprintPure, Category="Combat|Demo|Fissure", meta=(DisplayName="获取最近裂沟目标数", ToolTip="返回最近一次服务器线段查询去重后的目标数量。")) int32 GetLastTargetCount() const { return LastTargetCount; }
	/** 返回最近一次成功提交的 Knockback Motion 数。 */
	UFUNCTION(BlueprintPure, Category="Combat|Demo|Fissure", meta=(DisplayName="获取最近裂沟击退数", ToolTip="返回最近一次成功提交的击退 Motion 数量。")) int32 GetLastMotionCount() const { return LastMotionCount; }
	/** 返回最近一次是否创建视觉 Thinker。 */
	UFUNCTION(BlueprintPure, Category="Combat|Demo|Fissure", meta=(DisplayName="最近裂沟是否创建视觉 Thinker", ToolTip="返回最近一次裂沟是否成功创建仅视觉 Thinker。")) bool WasVisualThinkerCreated() const { return bVisualThinkerCreated; }
	/** 返回最近一次是否创建物理 blocker。 */
	UFUNCTION(BlueprintPure, Category="Combat|Demo|Fissure", meta=(DisplayName="最近裂沟是否创建阻挡物", ToolTip="返回最近一次裂沟是否成功创建物理阻挡 Actor。")) bool WasBlockerCreated() const { return bBlockerCreated; }

	/** Fissure Stun 使用的普通 Debuff 定义。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Demo|Fissure", meta=(DisplayName="眩晕 Modifier 定义", ToolTip="裂沟命中单位时施加的普通减益 Modifier。"))
	TObjectPtr<UCombatModifierData> StunModifierData = nullptr;
	/** 第一版物理 blocker Actor 类型。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Demo|Fissure", meta=(DisplayName="阻挡物类型", ToolTip="裂沟创建的物理阻挡 Actor 类型；为空时使用默认实现。"))
	TSubclassOf<ACombatFissureBlocker> BlockerClass;

protected:
	/** SpellStarted 时以公共子系统完成一条确定性 Fissure 切片。 */
	virtual void ReceiveSpellStart_Implementation(const FCombatAbilityActivationContext& Context) override;

private:
	/** 最近一次施法的可观察结果，不参与 gameplay 权威。 */
	int32 LastTargetCount = 0;
	int32 LastMotionCount = 0;
	bool bVisualThinkerCreated = false;
	bool bBlockerCreated = false;
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
