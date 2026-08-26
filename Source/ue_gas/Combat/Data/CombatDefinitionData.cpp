#include "Combat/Data/CombatDefinitionData.h"

#include "Abilities/GameplayAbility.h"
#include "Algo/AllOf.h"
#include "Misc/DataValidation.h"

/** Combat Definition 派生类型使用的固定 PrimaryAssetType，不能由资产路径推导。 */
namespace CombatPrimaryAssetTypes
{
	const FPrimaryAssetType Unit(TEXT("CombatUnit"));
	const FPrimaryAssetType Ability(TEXT("CombatAbility"));
	const FPrimaryAssetType Modifier(TEXT("CombatModifier"));
	const FPrimaryAssetType Projectile(TEXT("CombatProjectile"));
	const FPrimaryAssetType AbilitySet(TEXT("CombatAbilitySet"));
}

bool FCombatDefinitionRegistry::ValidateRedirects(
	const TArray<FCombatDefinitionRedirect>& Redirects,
	const TSet<FPrimaryAssetId>& KnownIds,
	TArray<FString>& OutErrors)
{
	TSet<FPrimaryAssetId> RedirectSources;
	// 第一遍先验证源身份并建立完整源集合，第二遍才能可靠识别链和环。
	for (const FCombatDefinitionRedirect& Redirect : Redirects)
	{
		if (!Redirect.OldId.IsValid() || !Redirect.NewId.IsValid() || Redirect.OldId == Redirect.NewId)
		{
			OutErrors.Add(TEXT("Definition redirect requires distinct valid ids"));
			continue;
		}
		if (Redirect.IntroducedInVersion < 1 || Redirect.IntroducedInVersion > CombatContentVersion)
		{
			OutErrors.Add(FString::Printf(TEXT("Unsupported redirect version for %s"), *Redirect.OldId.ToString()));
		}
		if (RedirectSources.Contains(Redirect.OldId))
		{
			OutErrors.Add(FString::Printf(TEXT("Duplicate redirect source: %s"), *Redirect.OldId.ToString()));
		}
		RedirectSources.Add(Redirect.OldId);
	}

	for (const FCombatDefinitionRedirect& Redirect : Redirects)
	{
		if (RedirectSources.Contains(Redirect.NewId))
		{
			OutErrors.Add(FString::Printf(TEXT("Redirect chain or cycle is forbidden: %s -> %s"),
				*Redirect.OldId.ToString(), *Redirect.NewId.ToString()));
		}
		if (!KnownIds.Contains(Redirect.NewId))
		{
			OutErrors.Add(FString::Printf(TEXT("Redirect target is missing: %s"), *Redirect.NewId.ToString()));
		}
	}
	return OutErrors.IsEmpty();
}

bool FCombatDefinitionRegistry::ResolveDefinitionId(
	const FPrimaryAssetId& RequestedId,
	const TArray<FCombatDefinitionRedirect>& Redirects,
	const TSet<FPrimaryAssetId>& KnownIds,
	FPrimaryAssetId& OutResolvedId)
{
	OutResolvedId = FPrimaryAssetId();
	if (KnownIds.Contains(RequestedId))
	{
		OutResolvedId = RequestedId;
		return true;
	}

	for (const FCombatDefinitionRedirect& Redirect : Redirects)
	{
		if (Redirect.OldId == RequestedId && KnownIds.Contains(Redirect.NewId))
		{
			OutResolvedId = Redirect.NewId;
			return true;
		}
	}
	return false;
}

FString FCombatDefinitionRegistry::MakeMissingPlaceholder(const FPrimaryAssetId& MissingId)
{
	return FString::Printf(TEXT("MissingDefinition[%s]"), *MissingId.ToString());
}

float FCombatSpecialValue::GetValueAtLevel(const int32 Level) const
{
	if (Values.IsEmpty() || Level <= 0)
	{
		return 0.0f;
	}
	return Values.Num() == 1 ? Values[0] : Values[FMath::Clamp(Level - 1, 0, Values.Num() - 1)];
}

bool FCombatSpecialValue::IsValidForMaxLevel(const int32 MaxLevel) const
{
	if (Values.Num() != 1 && Values.Num() != MaxLevel)
	{
		return false;
	}
	return Algo::AllOf(Values, [](const float Value) { return FMath::IsFinite(Value); });
}

FPrimaryAssetId UCombatDefinitionData::GetPrimaryAssetId() const
{
	const FPrimaryAssetType AssetType = GetCombatPrimaryAssetType();
	return AssetType.IsValid() && IsValidDefinitionName(DefinitionName)
		? FPrimaryAssetId(AssetType, DefinitionName)
		: FPrimaryAssetId();
}

FPrimaryAssetType UCombatDefinitionData::GetCombatPrimaryAssetType() const
{
	return FPrimaryAssetType();
}

bool UCombatDefinitionData::IsValidDefinitionName(const FName Name)
{
	const FString Value = Name.ToString();
	if (Value.IsEmpty() || Value[0] < TCHAR('a') || Value[0] > TCHAR('z'))
	{
		return false;
	}
	for (const TCHAR Character : Value)
	{
		if (!((Character >= TCHAR('a') && Character <= TCHAR('z'))
			|| (Character >= TCHAR('0') && Character <= TCHAR('9'))
			|| Character == TCHAR('_')))
		{
			return false;
		}
	}
	return true;
}

bool UCombatDefinitionData::ValidateDefinitionSet(const TArray<const UCombatDefinitionData*>& Definitions, TArray<FString>& OutErrors)
{
	TSet<FPrimaryAssetId> SeenIds;
	// Ability Class 到 DefinitionId 必须一对一，否则 AbilitySpec 无法反向解析稳定身份。
	TMap<UClass*, FPrimaryAssetId> AbilityClassOwners;
	for (const UCombatDefinitionData* Definition : Definitions)
	{
		if (!IsValid(Definition))
		{
			OutErrors.Add(TEXT("Null combat definition"));
			continue;
		}

		const FPrimaryAssetId Id = Definition->GetPrimaryAssetId();
		if (!Id.IsValid())
		{
			OutErrors.Add(FString::Printf(TEXT("Invalid definition identity: %s"), *Definition->GetPathName()));
			continue;
		}
		if (SeenIds.Contains(Id))
		{
			OutErrors.Add(FString::Printf(TEXT("Duplicate definition identity: %s"), *Id.ToString()));
			continue;
		}
		SeenIds.Add(Id);

		if (const UCombatAbilityData* AbilityDefinition = Cast<UCombatAbilityData>(Definition))
		{
			UClass* AbilityClass = AbilityDefinition->AbilityClass.Get();
			if (!AbilityClass)
			{
				OutErrors.Add(FString::Printf(TEXT("Ability definition has no class: %s"), *Id.ToString()));
			}
			else if (const FPrimaryAssetId* ExistingId = AbilityClassOwners.Find(AbilityClass))
			{
				OutErrors.Add(FString::Printf(TEXT("Ability class maps to multiple definitions: %s and %s"),
					*ExistingId->ToString(), *Id.ToString()));
			}
			else
			{
				AbilityClassOwners.Add(AbilityClass, Id);
			}
		}
	}
	return OutErrors.IsEmpty();
}

#if WITH_EDITOR
EDataValidationResult UCombatDefinitionData::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	if (!IsValidDefinitionName(DefinitionName))
	{
		Context.AddError(FText::FromString(TEXT("DefinitionName must be non-empty lower_snake_case")));
		Result = EDataValidationResult::Invalid;
	}
	if (SchemaVersion != 1)
	{
		Context.AddError(FText::FromString(FString::Printf(TEXT("Unsupported combat schema version: %d"), SchemaVersion)));
		Result = EDataValidationResult::Invalid;
	}
	if (!GetCombatPrimaryAssetType().IsValid())
	{
		Context.AddError(FText::FromString(TEXT("Combat definition has no fixed PrimaryAssetType")));
		Result = EDataValidationResult::Invalid;
	}
	return Result == EDataValidationResult::NotValidated ? EDataValidationResult::Valid : Result;
}
#endif

FPrimaryAssetType UCombatUnitData::GetCombatPrimaryAssetType() const { return CombatPrimaryAssetTypes::Unit; }
FPrimaryAssetType UCombatAbilityData::GetCombatPrimaryAssetType() const { return CombatPrimaryAssetTypes::Ability; }
FPrimaryAssetType UCombatModifierData::GetCombatPrimaryAssetType() const { return CombatPrimaryAssetTypes::Modifier; }
FPrimaryAssetType UCombatProjectileData::GetCombatPrimaryAssetType() const { return CombatPrimaryAssetTypes::Projectile; }
FPrimaryAssetType UCombatAbilitySet::GetCombatPrimaryAssetType() const { return CombatPrimaryAssetTypes::AbilitySet; }

#if WITH_EDITOR
EDataValidationResult UCombatUnitData::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	FString Diagnostic;
	if (!BaseStats.IsValid(&Diagnostic))
	{
		Context.AddError(FText::FromString(Diagnostic));
		Result = EDataValidationResult::Invalid;
	}
	if (!InitialTeamId.IsValid() || !FMath::IsFinite(CapsuleRadiusOverride) || CapsuleRadiusOverride < 0.0f)
	{
		Context.AddError(FText::FromString(TEXT("Unit team or capsule radius is invalid")));
		Result = EDataValidationResult::Invalid;
	}
	for (const TSoftObjectPtr<UCombatAbilitySet>& AbilitySet : AbilitySets)
	{
		if (AbilitySet.IsNull())
		{
			Context.AddError(FText::FromString(TEXT("UnitData contains an empty AbilitySet reference")));
			Result = EDataValidationResult::Invalid;
		}
	}
	return Result;
}

EDataValidationResult UCombatAbilityData::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	if (!AbilityClass)
	{
		Context.AddError(FText::FromString(TEXT("AbilityClass is required for one-to-one ability identity mapping")));
		Result = EDataValidationResult::Invalid;
	}
	if (MaxLevel < 1)
	{
		Context.AddError(FText::FromString(TEXT("MaxLevel must be at least one")));
		Result = EDataValidationResult::Invalid;
	}
	for (const TPair<FName, FCombatSpecialValue>& Pair : SpecialValues)
	{
		if (Pair.Key.IsNone() || !Pair.Value.IsValidForMaxLevel(MaxLevel))
		{
			Context.AddError(FText::FromString(FString::Printf(TEXT("Invalid special value '%s'"), *Pair.Key.ToString())));
			Result = EDataValidationResult::Invalid;
		}
	}
	return Result;
}

EDataValidationResult UCombatModifierData::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	if (!FMath::IsFinite(ThinkInterval) || ThinkInterval < 0.0f
		|| !FMath::IsFinite(Duration) || Duration < 0.0f || MaxStacks < 1)
	{
		Context.AddError(FText::FromString(TEXT("Modifier timing or stack values are invalid")));
		Result = EDataValidationResult::Invalid;
	}
	if (bDurationAffectedByStatusResistance && !bIsDebuff)
	{
		Context.AddError(FText::FromString(TEXT("Only a debuff can be affected by status resistance")));
		Result = EDataValidationResult::Invalid;
	}
	for (const FCombatModifierAttributeChange& Change : AttributeChanges)
	{
		if (!Change.Attribute.IsValid() || !FMath::IsFinite(Change.Magnitude))
		{
			Context.AddError(FText::FromString(TEXT("Modifier contains an invalid Attribute change")));
			Result = EDataValidationResult::Invalid;
		}
	}
	for (const TPair<FName, float>& Parameter : RuntimeParameters)
	{
		if (Parameter.Key.IsNone() || !FMath::IsFinite(Parameter.Value))
		{
			Context.AddError(FText::FromString(TEXT("Modifier contains an invalid Runtime parameter")));
			Result = EDataValidationResult::Invalid;
		}
	}
	return Result;
}

EDataValidationResult UCombatAbilitySet::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	TSet<UClass*> SeenClasses;
	for (const FCombatAbilitySetEntry& Entry : Abilities)
	{
		UClass* AbilityClass = Entry.AbilityClass.Get();
		if (!AbilityClass || Entry.InitialLevel < 1)
		{
			Context.AddError(FText::FromString(TEXT("AbilitySet entry requires an AbilityClass and positive InitialLevel")));
			Result = EDataValidationResult::Invalid;
			continue;
		}
		if (SeenClasses.Contains(AbilityClass))
		{
			Context.AddError(FText::FromString(FString::Printf(TEXT("Duplicate AbilityClass: %s"), *AbilityClass->GetPathName())));
			Result = EDataValidationResult::Invalid;
		}
		SeenClasses.Add(AbilityClass);
	}
	return Result;
}
#endif
