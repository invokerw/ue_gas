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
