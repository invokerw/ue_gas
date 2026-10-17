#pragma once
#include "CoreMinimal.h"
#include "Combat/Modifiers/CombatModifierRuntime.h"
#include "DefenseCoreShieldRuntime.generated.h"

/** 防守模式共享有限护盾；服务器 ModifierComponent 唯一持有，公共伤害 Hook 在抗性后吸收，耗尽延迟移除。 */
UCLASS(meta=(DisplayName="防守有限护盾", ToolTip="吸收物理、魔法与纯粹伤害；生命流失沿用公共规则绕过。余量只由服务器保存。"))
class COMBAT_API UDefenseShieldRuntime : public UCombatModifierRuntime
{
	GENERATED_BODY()
public:
	/** 只读余量供服务器导演投影；客户端不创建此 Runtime。 */
	float GetRemainingShield() const { return RemainingShield; }
protected:
	virtual void OnCreated_Implementation() override;
	virtual void OnDamageBlock_Implementation(FCombatDamageEvent& Event) override;
private:
	/** 领取时从最大生命计算并通过 shield_amount 参数快照；刷新不会补充。 */
	float RemainingShield=0;
};

/** 保留已发布核心屏障的类身份；与英雄复活盾共用吸收实现，目标和时限由获得效果配置。 */
UCLASS(meta=(DisplayName="防守核心屏障", ToolTip="沿用防守有限护盾吸收；核心目标与按波到期由Director管理。"))
class COMBAT_API UDefenseCoreShieldRuntime : public UDefenseShieldRuntime
{
	GENERATED_BODY()
};
