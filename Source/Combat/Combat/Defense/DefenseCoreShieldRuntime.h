#pragma once
#include "CoreMinimal.h"
#include "Combat/Modifiers/CombatModifierRuntime.h"
#include "DefenseCoreShieldRuntime.generated.h"

/** 防守核心的有限护盾；服务器 ModifierComponent 唯一持有，公共伤害 Hook 在抗性后吸收，耗尽延迟移除。 */
UCLASS(meta=(DisplayName="防守核心屏障", ToolTip="吸收物理、魔法与纯粹伤害；生命流失沿用公共规则绕过。余量只由服务器保存。"))
class COMBAT_API UDefenseCoreShieldRuntime : public UCombatModifierRuntime
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
