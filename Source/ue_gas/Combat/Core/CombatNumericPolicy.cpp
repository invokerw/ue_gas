#include "Combat/Core/CombatNumericPolicy.h"

bool FCombatNumericPolicyV1::IsValidNonNegativeRequest(const float Value)
{
	return FMath::IsFinite(Value) && Value >= 0.0f && Value <= MaxAbsoluteValue;
}

float FCombatNumericPolicyV1::ClampHealth(const float Value, const float MaxValue)
{
	if (!FMath::IsFinite(Value) || !FMath::IsFinite(MaxValue) || MaxValue < 1.0f)
	{
		return 0.0f;
	}
	return FMath::Clamp(Value, 0.0f, FMath::Min(MaxValue, MaxAbsoluteValue));
}

float FCombatNumericPolicyV1::ClampArmor(const float Value)
{
	return FMath::Clamp(FMath::IsFinite(Value) ? Value : 0.0f, MinArmor, MaxArmor);
}

float FCombatNumericPolicyV1::ClampMagicResistance(const float Value)
{
	return FMath::Clamp(FMath::IsFinite(Value) ? Value : 0.0f, MinMagicResistance, MaxMagicResistance);
}

float FCombatNumericPolicyV1::ClampChance(const float Value)
{
	return FMath::Clamp(FMath::IsFinite(Value) ? Value : 0.0f, 0.0f, 1.0f);
}

float FCombatNumericPolicyV1::ClampAmplification(const float Value)
{
	return FMath::Clamp(FMath::IsFinite(Value) ? Value : 0.0f, MinAmplification, MaxAmplification);
}

float FCombatNumericPolicyV1::ClampLifesteal(const float Value)
{
	return FMath::Clamp(FMath::IsFinite(Value) ? Value : 0.0f, 0.0f, MaxLifesteal);
}

float FCombatNumericPolicyV1::ClampReduction(const float Value)
{
	return FMath::Clamp(FMath::IsFinite(Value) ? Value : 0.0f, 0.0f, MaxReduction);
}
