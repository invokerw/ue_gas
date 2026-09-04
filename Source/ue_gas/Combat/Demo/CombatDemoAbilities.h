#pragma once

#include "CoreMinimal.h"

#include "Combat/Ability/CombatGameplayAbility.h"

#include "CombatDemoAbilities.generated.h"

class ACombatFissureBlocker;
class UCombatModifierData;

/** 无目标自我治疗示例 Ability。 */
UCLASS()
class UE_GAS_API UCombatSelfHealAbility : public UCombatGameplayAbility
{
	GENERATED_BODY()
};

/** 敌方单位目标魔法伤害示例 Ability。 */
UCLASS()
class UE_GAS_API UCombatUnitDamageAbility : public UCombatGameplayAbility
{
	GENERATED_BODY()
};

/** 由服务器查询命中集合的点目标 AoE 伤害示例 Ability。 */
UCLASS()
class UE_GAS_API UCombatPointAoeAbility : public UCombatGameplayAbility
{
	GENERATED_BODY()
};

/** 龙破斩示例：通过技能定义中的公共动作生成穿透直线弹体，发射时按本次技能等级读取伤害、宽度、射程与速度。 */
UCLASS()
class UE_GAS_API UCombatDragonSlaveAbility : public UCombatGameplayAbility
{
	GENERATED_BODY()
};

/** 肉钩示例：通过公共动作生成首命中即结束的弹体，命中后先造成伤害，再施加拖拽效果，由效果申请水平强制位移。 */
UCLASS()
class UE_GAS_API UCombatMeatHookAbility : public UCombatGameplayAbility
{
	GENERATED_BODY()
};

/** 冰霜之箭示例：技能只提供被动、普攻附加效果和自动施放开关；授予技能时附带的持续效果负责扣蓝、追加伤害和命中减速。 */
UCLASS()
class UE_GAS_API UCombatFrostArrowsAbility : public UCombatGameplayAbility
{
	GENERATED_BODY()
};

/** 沟壑示例：技能生效时查询线段内目标，依次提交伤害、眩晕与侧向击退，再创建限时视觉区域和物理阻挡物；各步骤复用公共战斗入口。 */
UCLASS()
class UE_GAS_API UCombatFissureAbility : public UCombatGameplayAbility
{
	GENERATED_BODY()

public:
	UCombatFissureAbility();
	/** 技能授予单位时，将类默认对象上的眩晕效果和阻挡物类型复制到该单位的技能实例，保持热重载和测试动态配置一致。 */
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
	/** 裂沟使用的权威物理 blocker Actor 类型。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Demo|Fissure", meta=(DisplayName="阻挡物类型", ToolTip="裂沟创建的物理阻挡 Actor 类型；为空时使用默认实现。"))
	TSubclassOf<ACombatFissureBlocker> BlockerClass;

protected:
	/** 技能生效时执行沟壑的目标查询、伤害、控制、视觉与阻挡物创建；诊断字段分别记录命中候选和成功创建项，不代表所有步骤全部成功。 */
	virtual void ReceiveSpellStart_Implementation(const FCombatAbilityActivationContext& Context) override;

private:
	/** 最近一次施法的可观察结果，不参与 gameplay 权威。 */
	int32 LastTargetCount = 0;
	int32 LastMotionCount = 0;
	bool bVisualThinkerCreated = false;
	bool bBlockerCreated = false;
};

/** 引导调度测试使用的技能：记录收到的逻辑周期数、结束次数和中断标记，供自动化断言。 */
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
