#pragma once

#include "CoreMinimal.h"

#include "Combat/Combat/CombatTransactionTypes.h"

/** 实现 FormulaVersion=1 的无副作用 Damage 抗性计算。 */
struct UE_GAS_API FCombatDamageCalculator
{
	/** 返回 Armor v1 对物理伤害的倍率，支持正负护甲。 */
	static float CalculateArmorMultiplier(float Armor);
	/** 返回指定 DamageType 在目标抗性下的最终数值。 */
	static float CalculateAfterResistance(float Amount, ECombatDamageType DamageType, float Armor, float MagicResist);
};
