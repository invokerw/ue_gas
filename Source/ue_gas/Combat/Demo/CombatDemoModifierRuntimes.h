#pragma once

#include "CoreMinimal.h"

#include "Combat/Modifiers/CombatModifierRuntime.h"

#include "CombatDemoModifierRuntimes.generated.h"

/** Magic Shield 示例 Runtime：只在 Magical Damage 的 Block 阶段消耗护盾。 */
UCLASS(Blueprintable)
class UE_GAS_API UCombatMagicShieldRuntime : public UCombatModifierRuntime
{
	GENERATED_BODY()

public:
	/** 返回当前只保存在服务器 Runtime 的护盾余量。 */
	UFUNCTION(BlueprintPure, Category="Combat|Demo|Shield") float GetRemainingShield() const { return RemainingShield; }

protected:
	/** 从 shield_amount 参数初始化护盾余量。 */
	virtual void OnCreated_Implementation() override;
	/** Magical Damage 时稳定吸收并在耗尽后 deferred remove。 */
	virtual void OnDamageBlock_Implementation(FCombatDamageEvent& Event) override;

private:
	/** 不复制给客户端的权威护盾余量。 */
	float RemainingShield = 0.0f;
};

/** DOT 示例 Runtime：每个 Scheduler 逻辑 tick 调用公共 DamageSubsystem。 */
UCLASS(Blueprintable)
class UE_GAS_API UCombatPeriodicDamageRuntime : public UCombatModifierRuntime
{
	GENERATED_BODY()

protected:
	/** 使用 damage_per_tick 与 damage_type 参数创建独立 Damage 事务。 */
	virtual void OnThink_Implementation(const FCombatScheduledTickContext& TickContext) override;
};

/** 反伤示例 Runtime：真实受伤后创建同 RootEventId 的防递归子事务。 */
UCLASS(Blueprintable)
class UE_GAS_API UCombatDamageReflectionRuntime : public UCombatModifierRuntime
{
	GENERATED_BODY()

protected:
	/** 按 reflection_pct 和真实 AppliedDamage 反射给原来源。 */
	virtual void OnPostTakeDamage_Implementation(const FCombatDamageEvent& Event) override;
};

/** M4 基础法球 Runtime：无副作用预检，winner 提交 Mana 并写入伤害/OnHit 快照。 */
UCLASS(Blueprintable)
class UE_GAS_API UCombatDemoOrbRuntime : public UCombatModifierRuntime
{
	GENERATED_BODY()

public:
	/** 返回测试和调试可观察的 winner 提交次数。 */
	UFUNCTION(BlueprintPure, Category="Combat|Demo|Orb") int32 GetSuccessfulClaimCount() const { return SuccessfulClaimCount; }

protected:
	/** 第一版示例统一参与 Orb.Primary exclusive group。 */
	virtual FName GetAttackOrbExclusiveGroup_Implementation() const override;
	/** 只读检查 orb_enabled、数值与当前 Mana，不产生任何资源副作用。 */
	virtual bool CanClaimAttack_Implementation(const FCombatAttackCandidateContext& Context) const override;
	/** 再次原子预检后扣除 Mana，并输出 bonus/on-hit 的不可变快照。 */
	virtual bool OnAttackClaimed_Implementation(
		const FCombatAttackCandidateContext& Context,
		FCombatOrbSnapshot& OutSnapshot) override;

private:
	/** 只在提交成功后递增；未胜出和失败候选保持不变。 */
	int32 SuccessfulClaimCount = 0;
};

/** Meat Hook 命中后获取高优先级 Horizontal Motion，并在任意结束路径移除自身。 */
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
