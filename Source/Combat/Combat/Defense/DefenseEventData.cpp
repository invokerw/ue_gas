#include "Combat/Defense/DefenseEventData.h"
#include "Misc/DataValidation.h"
FPrimaryAssetType UDefenseEventData::GetCombatPrimaryAssetType() const { return FPrimaryAssetType(TEXT("DefenseEvent")); }
bool UDefenseEventData::ValidateRuntime(FString& OutDiagnostic) const
{
	OutDiagnostic = TEXT("防守事件身份、触发波次、文本或收益配置非法");
	if (!GetPrimaryAssetId().IsValid() || SchemaVersion != FCombatDefinitionRegistry::CombatContentVersion
		|| DefenseSchemaVersion != 1 || (TriggerAfterWave != 2 && TriggerAfterWave != 4)
		|| Title.IsEmpty() || SafeDescription.IsEmpty() || RiskDescription.IsEmpty()
		|| SafeGold < 0 || SafeExperience < 0 || RiskGold < 0 || RiskExperience < 0
		|| !FMath::IsFinite(SafeCoreHealFraction) || SafeCoreHealFraction < 0 || SafeCoreHealFraction > 1
		|| RiskBudgetBonus < 0 || RiskBudgetBonus > 1000
		|| !FMath::IsFinite(BossRewardBonusFraction) || BossRewardBonusFraction < 0 || BossRewardBonusFraction > 1)
		return false;
	OutDiagnostic.Reset(); return true;
}
#if WITH_EDITOR
EDataValidationResult UDefenseEventData::IsDataValid(FDataValidationContext& Context) const
{
	const auto Base = Super::IsDataValid(Context); FString Error;
	if (!ValidateRuntime(Error)) { Context.AddError(FText::FromString(Error)); return EDataValidationResult::Invalid; }
	return Base;
}
#endif
