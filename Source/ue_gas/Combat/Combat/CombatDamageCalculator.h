#pragma once

#include "CoreMinimal.h"

#include "Combat/Combat/CombatTransactionTypes.h"

/** 计算第 1 版伤害公式中的抗性倍率；只做数值运算，不检查免疫、护盾或生命状态，也不修改属性。 */
struct UE_GAS_API FCombatDamageCalculator
{
	/** 返回物理伤害倍率：正护甲为 1 / (1 + 0.06 × 护甲)，负护甲为 2 - 0.94 的负护甲次方。例如护甲 10 时受到原伤害的 62.5%。输入先按数值策略限制范围。 */
	static float CalculateArmorMultiplier(float Armor);
	/** 物理伤害乘护甲倍率，魔法伤害乘 (1 - 魔抗)，纯粹伤害保持原量；负护甲或负魔抗可放大伤害。请求量先限制为有限的非负数。 */
	static float CalculateAfterResistance(float Amount, ECombatDamageType DamageType, float Armor, float MagicResist);
};
