#pragma once

#include "CoreMinimal.h"

#include "Combat/Modifiers/CombatModifierRuntime.h"

#include "CombatDemoModifierRuntimes.generated.h"

/** 魔法护盾示例：在魔法伤害经过抗性计算后吸收伤害，余量只存在服务器效果实例中；物理、纯粹和生命移除伤害不消耗此护盾。 */
UCLASS(Blueprintable)
class UE_GAS_API UCombatMagicShieldRuntime : public UCombatModifierRuntime
{
	GENERATED_BODY()

public:
	/** 返回当前只保存在服务器 Runtime 的护盾余量。 */
	UFUNCTION(BlueprintPure, Category="Combat|Demo|Shield", meta=(DisplayName="获取剩余护盾值", ToolTip="返回仅保存在服务器 Runtime 中的当前护盾余量。")) float GetRemainingShield() const { return RemainingShield; }

protected:
	/** 从 shield_amount 参数初始化护盾余量。 */
	virtual void OnCreated_Implementation() override;
	/** 抵消不超过当前护盾余量的魔法伤害，同时减少待扣血量并累计吸收量；耗尽后请求移除，实际删除等当前效果回调阶段结束。 */
	virtual void OnDamageBlock_Implementation(FCombatDamageEvent& Event) override;

private:
	/** 不复制给客户端的权威护盾余量。 */
	float RemainingShield = 0.0f;
};

/** 持续伤害示例：每次效果周期回调调用公共伤害入口；若调度合并了多个到期周期，则把单次伤害乘 TickCount 后一次结算。 */
UCLASS(Blueprintable)
class UE_GAS_API UCombatPeriodicDamageRuntime : public UCombatModifierRuntime
{
	GENERATED_BODY()

protected:
	/** 读取 damage_per_tick 作为每周期伤害，乘本次合并次数后创建新的根伤害事件；damage_type 四舍五入并限制为 0 物理、1 魔法、2 纯粹，默认魔法。 */
	virtual void OnThink_Implementation(const FCombatScheduledTickContext& TickContext) override;
};

/** 反伤示例：按实际受伤量向原来源发起同一事件链的子伤害；跳过已标记为反射的伤害，避免双方反伤无限互相触发。 */
UCLASS(Blueprintable)
class UE_GAS_API UCombatDamageReflectionRuntime : public UCombatModifierRuntime
{
	GENERATED_BODY()

protected:
	/** 按实际扣血量乘 reflection_pct 反射给原来源，保留原伤害类型并禁止这次反伤吸血；自伤、零实际伤害和反射伤害不触发。 */
	virtual void OnPostTakeDamage_Implementation(const FCombatDamageEvent& Event) override;
};

/** 普攻附加效果示例：先只读检查是否可参与同组竞争，只有被选中的效果才扣除法力，并把本次额外伤害和命中动作保存到攻击记录。 */
UCLASS(Blueprintable)
class UE_GAS_API UCombatDemoOrbRuntime : public UCombatModifierRuntime
{
	GENERATED_BODY()

public:
	/** 返回本效果被选中且成功扣费、提交攻击附加数据的次数，供测试与调试使用。 */
	UFUNCTION(BlueprintPure, Category="Combat|Demo|Orb", meta=(DisplayName="获取法球成功声明次数", ToolTip="返回本效果被选中且成功扣费、提交攻击附加数据的次数，供测试与调试使用。")) int32 GetSuccessfulClaimCount() const { return SuccessfulClaimCount; }

protected:
	/** 示例法球统一参与 Orb.Primary 互斥组。 */
	virtual FName GetAttackOrbExclusiveGroup_Implementation() const override;
	/** 只读检查 orb_enabled、数值与当前 Mana，不产生任何资源副作用。 */
	virtual bool CanClaimAttack_Implementation(const FCombatAttackCandidateContext& Context) const override;
	/** 再次检查条件后扣除法力，成功时输出本次攻击的额外伤害、伤害类型覆盖和命中动作；失败不写 OutSnapshot。 */
	virtual bool OnAttackClaimed_Implementation(
		const FCombatAttackCandidateContext& Context,
		FCombatOrbSnapshot& OutSnapshot) override;

private:
	/** 只在提交成功后递增；未胜出和失败候选保持不变。 */
	int32 SuccessfulClaimCount = 0;
};

/** 冰霜之箭附加效果：根据授予技能的当前等级和自动施放开关决定能否参与普攻，成功扣蓝后把额外伤害、减速参数与弹体定义引用存入本次攻击记录。 */
UCLASS(Blueprintable)
class UE_GAS_API UCombatFrostArrowsRuntime : public UCombatModifierRuntime
{
	GENERATED_BODY()

public:
	/** 返回成功提交 Mana 并冻结法球快照的次数。 */
	UFUNCTION(BlueprintPure, Category="Combat|Demo|FrostArrows", meta=(DisplayName="获取冰箭成功声明次数", ToolTip="返回冰箭成功提交 Mana 并冻结法球快照的次数。")) int32 GetSuccessfulClaimCount() const { return SuccessfulClaimCount; }

protected:
	/** Frost Arrows 与其他主法球共享 Orb.Primary。 */
	virtual FName GetAttackOrbExclusiveGroup_Implementation() const override;
	/** 只读检查自动施放开关、沉默、破坏状态、当前技能等级数值和可用法力；候选阶段不扣费。 */
	virtual bool CanClaimAttack_Implementation(const FCombatAttackCandidateContext& Context) const override;
	/** 原子提交 Mana，并冻结 bonus、slow 参数、Modifier 与 ProjectileData。 */
	virtual bool OnAttackClaimed_Implementation(
		const FCombatAttackCandidateContext& Context,
		FCombatOrbSnapshot& OutSnapshot) override;

private:
	/** 只统计 winner 成功提交，未胜出、资源失败和 Break 均不递增。 */
	int32 SuccessfulClaimCount = 0;
};

/** 一次性技能格挡示例：技能已经提交费用和冷却后，阻止带有可格挡标签的单位目标技能；不返还施法费用。 */
UCLASS(Blueprintable)
class UE_GAS_API UCombatSpellBlockRuntime : public UCombatModifierRuntime
{
	GENERATED_BODY()

protected:
	/** 来源不是自身时请求移除整个当前效果实例，并返回 true 表示本次格挡已认领；不只是减少一层，删除延迟到当前效果回调阶段结束。 */
	virtual bool TryBlockAbility_Implementation(
		const FPrimaryAssetId& AbilityDefinitionId,
		ACombatUnitCharacter* Caster,
		const FCombatEventContext& Context) override;
};

/** 肉钩拖拽效果：创建时使用施加请求携带的强制位移参数，优先级和方向由请求决定；位移结束时移除自身，效果先被移除时也释放所持位移。 */
UCLASS()
class UE_GAS_API UCombatHookDragRuntime : public UCombatModifierRuntime
{
	GENERATED_BODY()

protected:
	/** 消费 Apply 注入的 Motion 快照；获取失败立即请求移除。 */
	virtual void OnCreated_Implementation() override;
	/** Modifier 提前结束时解绑并释放仍活动的 Motion。 */
	virtual void OnDestroyed_Implementation() override;

private:
	/** 只处理属于本 Runtime 的 exactly-once Motion 结束结果。 */
	void HandleMotionFinished(const FCombatMotionResult& Result);
	/** 当前持有的 Motion 句柄。 */
	FCombatMotionHandle MotionHandle;
};
