#include "Combat/Core/CombatTypes.h"

FString FCombatTeamId::ToString() const
{
	if (!IsValid())
	{
		return TEXT("NoTeam");
	}
	return IsNeutralCamp() ? TEXT("Neutral(0)") : FString::Printf(TEXT("Team(%u)"), Value);
}

FString FCombatEventId::ToString() const
{
	return IsValid() ? FString::Printf(TEXT("Event[%llu]"), Sequence) : TEXT("Event[Invalid]");
}

FString FCombatHandleKey::ToString(const TCHAR* TypeName) const
{
	if (!IsValid())
	{
		return FString::Printf(TEXT("%s[Invalid]"), TypeName);
	}
	return FString::Printf(TEXT("%s[Id=%llu Gen=%u Life=%u]"), TypeName, Id, Generation, LifeGeneration);
}

bool FCombatSourceContext::operator==(const FCombatSourceContext& Other) const
{
	return DirectSourceType == Other.DirectSourceType
		&& AbilityDefinitionId == Other.AbilityDefinitionId
		&& ModifierDefinitionId == Other.ModifierDefinitionId
		&& ProjectileDefinitionId == Other.ProjectileDefinitionId;
}

FCombatOperationResult FCombatOperationResult::Success()
{
	FCombatOperationResult Result;
	Result.bSuccess = true;
	return Result;
}

FCombatOperationResult FCombatOperationResult::Failure(const FGameplayTag& InFailureTag, FString InDiagnostic)
{
	FCombatOperationResult Result;
	Result.FailureTag = InFailureTag;
	Result.Diagnostic = MoveTemp(InDiagnostic);
	return Result;
}
