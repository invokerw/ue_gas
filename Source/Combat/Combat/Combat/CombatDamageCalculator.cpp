#include "Combat/Combat/CombatDamageCalculator.h"

#include "Combat/Core/CombatNumericPolicy.h"

float FCombatDamageCalculator::CalculateArmorMultiplier(const float Armor)
{
	const float ClampedArmor = FCombatNumericPolicyV1::ClampArmor(Armor);
	if (ClampedArmor >= 0.0f)
	{
		const float Reduction = (0.06f * ClampedArmor) / (1.0f + 0.06f * FMath::Abs(ClampedArmor));
		return 1.0f - Reduction;
	}
	return 2.0f - FMath::Pow(0.94f, -ClampedArmor);
}

float FCombatDamageCalculator::CalculateAfterResistance(
	const float Amount,
	const ECombatDamageType DamageType,
	const float Armor,
	const float MagicResist)
{
	const float SafeAmount = FMath::Clamp(FMath::IsFinite(Amount) ? Amount : 0.0f, 0.0f,
		FCombatNumericPolicyV1::MaxAbsoluteValue);
	switch (DamageType)
	{
	case ECombatDamageType::Physical:
		return SafeAmount * CalculateArmorMultiplier(Armor);
	case ECombatDamageType::Magical:
		return SafeAmount * (1.0f - FCombatNumericPolicyV1::ClampMagicResistance(MagicResist));
	case ECombatDamageType::Pure:
	default:
		return SafeAmount;
	}
}
