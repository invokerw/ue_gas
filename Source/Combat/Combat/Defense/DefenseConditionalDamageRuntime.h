#pragma once
#include "CoreMinimal.h"
#include "Combat/Modifiers/CombatModifierRuntime.h"
#include "DefenseConditionalDamageRuntime.generated.h"

/** 防守团队条件增伤；只在来源英雄的公共PreDealDamage阶段对稳定目标身份加成。未知目标不会误触发。 */
UCLASS(meta=(DisplayName="防守条件增伤", ToolTip="通过稳定目标DefinitionId名称匹配攻城/刺客或Boss；不改变客户端结算，也不创建第二套伤害入口。"))
class COMBAT_API UDefenseConditionalDamageRuntime : public UCombatModifierRuntime
{
	GENERATED_BODY()
protected:
	virtual void OnPreDealDamage_Implementation(FCombatDamageEvent& Event) override;
private:
	/** 仅测试与阶段内容使用的稳定角色规则；Phase3敌人资产沿用包含 siege/assassin/boss 的DefinitionId。 */
	static bool MatchesTarget(const ACombatUnitCharacter* Target, int32 Role);
};
