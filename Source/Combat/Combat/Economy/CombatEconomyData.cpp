#include "Combat/Economy/CombatEconomyData.h"

#include "Misc/DataValidation.h"

FPrimaryAssetType UCombatEconomyData::GetCombatPrimaryAssetType() const
{
	static const FPrimaryAssetType EconomyType(TEXT("CombatEconomy"));
	return EconomyType;
}

bool UCombatEconomyData::ValidateRuntime(FString& OutError) const
{
	OutError.Reset();
	if (!GetPrimaryAssetId().IsValid())
	{
		OutError = TEXT("Economy rules require a valid lower_snake_case identity");
		return false;
	}
	if (GoldCap < 1 || StartingGold < 0 || StartingGold > GoldCap
		|| PassiveGoldPerMinute < 0 || !FMath::IsFinite(FullRefundSeconds)
		|| FullRefundSeconds < 0.0f || SellValueBasisPoints < 0 || SellValueBasisPoints > 10000)
	{
		OutError = TEXT("Economy rules contain an invalid gold, refund or sell boundary");
		return false;
	}
	return true;
}

#if WITH_EDITOR
EDataValidationResult UCombatEconomyData::IsDataValid(FDataValidationContext& Context) const
{
	const EDataValidationResult Base = Super::IsDataValid(Context);
	FString Error;
	if (!ValidateRuntime(Error))
	{
		Context.AddError(FText::FromString(Error));
		return EDataValidationResult::Invalid;
	}
	return Base;
}
#endif
