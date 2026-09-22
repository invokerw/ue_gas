#include "Combat/AI/CombatAITacticalTypes.h"

#include "Combat/Unit/CombatUnitCharacter.h"

bool FCombatAIAbilityUsageRule::Validate(FString* OutDiagnostic) const
{
	const bool bValid = AbilityDefinitionId.IsValid()
		&& AbilityDefinitionId.PrimaryAssetType == FPrimaryAssetType(TEXT("CombatAbility"))
		&& TargetPolicy != ECombatAIAbilityTargetPolicy::TacticalLocation
		&& FMath::IsFinite(BaseUtility) && BaseUtility >= 0.0f && BaseUtility <= 1.0f
		&& MinimumEffectiveTargets >= 1 && MinimumEffectiveTargets <= 64
		&& FMath::IsFinite(ManaReserveRatio) && ManaReserveRatio >= 0.0f && ManaReserveRatio <= 1.0f;
	if (!bValid && OutDiagnostic)
	{
		*OutDiagnostic = TEXT("AI ability rule requires a CombatAbility identity, a supported self/current-enemy target policy, utility/reserve in [0,1] and target count in [1,64]");
	}
	return bValid;
}

FCombatOrderRequest FCombatAIAbilityCandidate::MakeOrderRequest() const
{
	FCombatOrderRequest Request;
	Request.Type = OrderType;
	Request.AbilitySpecHandle = SpecHandle;
	Request.TargetUnit = Target.Get();
	Request.TargetLocation = TargetLocation;
	Request.bHasTargetLocation = bHasTargetLocation;
	return Request;
}

float FCombatAITacticalSnapshot::GetScore(const ECombatAITacticalAction Action) const
{
	switch (Action)
	{
	case ECombatAITacticalAction::Guard: return GuardUtility;
	case ECombatAITacticalAction::Attack: return AttackUtility;
	case ECombatAITacticalAction::Cast: return BestAbility.bValid ? BestAbility.FinalUtility : 0.0f;
	case ECombatAITacticalAction::Reposition: return RepositionUtility;
	}
	return 0.0f;
}

float FCombatAIUtilityScoring::ClampScore(const float Score)
{
	return FMath::IsFinite(Score) ? FMath::Clamp(Score, 0.0f, 1.0f) : 0.0f;
}

bool FCombatAIUtilityScoring::ShouldSwitch(const float CurrentScore, const float ChallengerScore,
	const double HeldSeconds, const float MinHoldSeconds, const float SwitchMargin)
{
	if (!FMath::IsFinite(HeldSeconds) || !FMath::IsFinite(MinHoldSeconds) || !FMath::IsFinite(SwitchMargin))
	{
		return false;
	}

	if (HeldSeconds < FMath::Max(0.0f, MinHoldSeconds))
	{
		return false;
	}

	return ClampScore(ChallengerScore) > ClampScore(CurrentScore) + FMath::Max(0.0f, SwitchMargin);
}
