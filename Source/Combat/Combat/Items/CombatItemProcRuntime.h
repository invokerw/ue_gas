#pragma once
#include "CoreMinimal.h"
#include "Combat/Modifiers/CombatModifierRuntime.h"
#include "CombatItemProcRuntime.generated.h"

/** 真实普攻造成伤害后追加一次魔法伤害；继承事件根与物品来源，禁止追加伤害再次触发。 */
UCLASS(meta=(DisplayName="物品攻击附伤", ToolTip="物品被动：普通攻击实际造成伤害后，按 bonus_damage 参数追加魔法伤害。"))
class COMBAT_API UCombatItemProcRuntime : public UCombatModifierRuntime
{
	GENERATED_BODY()
protected:
	virtual void OnPostDealDamage_Implementation(const FCombatDamageEvent& Event) override;
};
