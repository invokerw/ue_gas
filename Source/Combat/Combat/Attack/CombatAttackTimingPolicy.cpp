#include "Combat/Attack/CombatAttackTimingPolicy.h"

bool FCombatAttackTimingPolicyV1::Calculate(
	const float BaseAttackTime,
	const float AttackSpeed,
	const float BaseAttackPoint,
	FCombatAttackTiming& OutTiming)
{
	if (!FMath::IsFinite(BaseAttackTime) || BaseAttackTime <= 0.0f
		|| !FMath::IsFinite(AttackSpeed) || !FMath::IsFinite(BaseAttackPoint) || BaseAttackPoint < 0.0f)
	{
		return false;
	}

	FCombatAttackTiming Timing;
	Timing.EffectiveAttackSpeed = FMath::Clamp(AttackSpeed, MinAttackSpeed, MaxAttackSpeed);
	const float SpeedScale = 100.0f / Timing.EffectiveAttackSpeed;
	Timing.AttackInterval = FMath::Clamp(
		BaseAttackTime * SpeedScale, MinAttackInterval, MaxAttackInterval);
	Timing.AttackPoint = FMath::Clamp(BaseAttackPoint * SpeedScale, 0.0f, Timing.AttackInterval);
	Timing.Recovery = FMath::Max(0.0f, Timing.AttackInterval - Timing.AttackPoint);
	Timing.AnimationRate = FMath::Clamp(Timing.EffectiveAttackSpeed / 100.0f, 0.20f, 7.00f);
	OutTiming = Timing;
	return true;
}
