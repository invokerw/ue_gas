#pragma once

#include "CoreMinimal.h"

/** 集中定义 Combat 数值 v1 的有限值、Clamp 和容差规则。 */
struct UE_GAS_API FCombatNumericPolicyV1
{
	/** 写入日志和回放数据的公式版本。 */
	static constexpr uint16 FormulaVersion = 1;
	/** 所有战斗数值允许的最大绝对值。 */
	static constexpr float MaxAbsoluteValue = 1.0e9f;
	/** 护甲输入下界。 */
	static constexpr float MinArmor = -10000.0f;
	/** 护甲输入上界。 */
	static constexpr float MaxArmor = 10000.0f;
	/** 魔法抗性下界，-1 表示最多承受双倍伤害。 */
	static constexpr float MinMagicResistance = -1.0f;
	/** 魔法抗性上界，避免达到完全免疫。 */
	static constexpr float MaxMagicResistance = 0.95f;
	/** 技能增幅下界。 */
	static constexpr float MinAmplification = -1.0f;
	/** 技能增幅上界。 */
	static constexpr float MaxAmplification = 10.0f;
	/** 吸血比例上界。 */
	static constexpr float MaxLifesteal = 10.0f;
	/** 通用减伤比例上界。 */
	static constexpr float MaxReduction = 0.90f;
	/** 范围判定统一使用的厘米容差。 */
	static constexpr float RangeToleranceCm = 5.0f;
	/** 命中距离稳定排序使用的厘米并列容差。 */
	static constexpr float HitTieToleranceCm = 0.1f;

	/** 检查请求值是否有限且非负。 */
	static bool IsValidNonNegativeRequest(float Value);
	/** 将生命值限制在 0 与合法最大值之间。 */
	static float ClampHealth(float Value, float MaxValue);
	/** 将护甲限制到 v1 允许区间，非有限值回退为 0。 */
	static float ClampArmor(float Value);
	/** 将魔法抗性限制到 v1 允许区间，非有限值回退为 0。 */
	static float ClampMagicResistance(float Value);
	/** 将概率限制到 [0, 1]，非有限值回退为 0。 */
	static float ClampChance(float Value);
	/** 将增幅限制到 v1 允许区间，非有限值回退为 0。 */
	static float ClampAmplification(float Value);
	/** 将吸血比例限制到 v1 允许区间，非有限值回退为 0。 */
	static float ClampLifesteal(float Value);
	/** 将通用减伤限制到 [0, MaxReduction]。 */
	static float ClampReduction(float Value);
};
