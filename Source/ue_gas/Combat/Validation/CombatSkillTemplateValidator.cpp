#include "Combat/Validation/CombatSkillTemplateValidator.h"

#include "Combat/Ability/CombatGameplayAbility.h"
#include "Combat/Data/CombatDefinitionData.h"

bool FCombatSkillTemplateValidator::ValidateAbilityTemplate(
	const TSubclassOf<UCombatGameplayAbility> AbilityClass,
	const UCombatAbilityData* AbilityData,
	const FCombatSkillTemplateRequirements& Requirements,
	TArray<FString>& OutErrors)
{
	const int32 StartErrorCount = OutErrors.Num();
	const UCombatGameplayAbility* AbilityCdo = AbilityClass
		? Cast<UCombatGameplayAbility>(AbilityClass->GetDefaultObject()) : nullptr;
	if (!AbilityCdo || !AbilityData)
	{
		OutErrors.Add(TEXT("Ability template requires a class CDO and AbilityData"));
		return false;
	}
	if (AbilityCdo->GetAbilityData() != AbilityData)
	{
		OutErrors.Add(TEXT("Ability Class CDO must reference the exact validated AbilityData"));
	}
	FString Diagnostic;
	if (!AbilityData->ValidateRuntime(Diagnostic))
	{
		OutErrors.Add(FString::Printf(TEXT("AbilityData runtime schema failed: %s"), *Diagnostic));
	}
	if (!AbilityData->BehaviorTags.HasAllExact(Requirements.RequiredBehaviorTags))
	{
		OutErrors.Add(TEXT("AbilityData is missing required behavior tags"));
	}
	if (Requirements.bRequireIntrinsicModifier && !AbilityData->IntrinsicModifier)
	{
		OutErrors.Add(TEXT("Ability template requires an Intrinsic Modifier"));
	}
	if (Requirements.bRequirePublicAction && AbilityData->Actions.IsEmpty())
	{
		OutErrors.Add(TEXT("Ability template requires at least one public DataDriven Action"));
	}
	for (const FName Key : Requirements.RequiredSpecialKeys)
	{
		const FCombatSpecialValue* Value = AbilityData->SpecialValues.Find(Key);
		if (Key.IsNone() || !Value || !Value->IsValidForMaxLevel(AbilityData->MaxLevel))
		{
			OutErrors.Add(FString::Printf(TEXT("Ability template requires valid special '%s'"), *Key.ToString()));
		}
	}
	return OutErrors.Num() == StartErrorCount;
}

bool FCombatSkillTemplateValidator::ValidateDefinitions(
	const TArray<const UCombatDefinitionData*>& Definitions,
	TArray<FString>& OutErrors)
{
	return UCombatDefinitionData::ValidateDefinitionSet(Definitions, OutErrors);
}

bool FCombatSkillTemplateValidator::ValidateEventSequence(
	const TArray<FGameplayTag>& Actual,
	const TArray<FGameplayTag>& Expected,
	TArray<FString>& OutErrors)
{
	if (Actual.Num() != Expected.Num())
	{
		OutErrors.Add(FString::Printf(TEXT("Event count mismatch: actual=%d expected=%d"), Actual.Num(), Expected.Num()));
		return false;
	}
	for (int32 Index = 0; Index < Expected.Num(); ++Index)
	{
		if (Actual[Index] != Expected[Index])
		{
			OutErrors.Add(FString::Printf(TEXT("Event[%d] mismatch: actual=%s expected=%s"),
				Index, *Actual[Index].ToString(), *Expected[Index].ToString()));
			return false;
		}
	}
	return true;
}

const TArray<FString>& FCombatSkillTemplateValidator::GetForbiddenBypassPatterns()
{
	static const TArray<FString> Patterns = {
		TEXT("SetHealth("),
		TEXT("SetActorLocation("),
		TEXT("GetTimerManager("),
		TEXT("SetTimer("),
		TEXT("ProjectileImpact(")
	};
	return Patterns;
}
