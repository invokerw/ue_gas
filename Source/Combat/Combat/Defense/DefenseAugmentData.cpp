#include "Combat/Defense/DefenseAugmentData.h"
#include "Combat/Modifiers/CombatModifierRuntime.h"
#include "Misc/DataValidation.h"

FPrimaryAssetType UDefenseAugmentData::GetCombatPrimaryAssetType() const { return FPrimaryAssetType(TEXT("DefenseAugment")); }
FPrimaryAssetType UDefenseAugmentPoolData::GetCombatPrimaryAssetType() const { return FPrimaryAssetType(TEXT("DefenseAugmentPool")); }

bool UDefenseAugmentData::ValidateRuntime(FString& OutDiagnostic) const
{
	OutDiagnostic = TEXT("强化定义的身份、门槛、权重、引用或永久属性效果非法");
	if (!GetPrimaryAssetId().IsValid() || SchemaVersion != FCombatDefinitionRegistry::CombatContentVersion
		|| EarliestWave < 0 || EarliestWave > 5 || Weight < 1 || Weight > 100000 || MaxStacks != 1
		|| uint8(Family) > uint8(EDefenseAugmentFamily::Economy) || uint8(Rarity) > uint8(EDefenseAugmentRarity::Final)
		|| MutuallyExclusive.Num() > 64 || Prerequisites.Num() > 64 || CompatibleHeroes.Num() > 16) return false;
	// 这一批原型卡只允许无副作用的永久属性 Modifier；复杂 Hook 卡另行扩展，避免在选择事务重入。
	if (!ModifierData || !ModifierData->GetPrimaryAssetId().IsValid()
		|| ModifierData->SchemaVersion != FCombatDefinitionRegistry::CombatContentVersion
		|| ModifierData->bIsDebuff || ModifierData->bRemoveOnDeath || ModifierData->Duration != 0
		|| ModifierData->ThinkInterval != 0 || ModifierData->MaxStacks != 1
		|| (ModifierData->RuntimeClass && ModifierData->RuntimeClass != UCombatModifierRuntime::StaticClass())
		|| !ModifierData->GrantedTags.IsEmpty()
		|| ModifierData->AttributeChanges.IsEmpty() || ModifierData->AttributeChanges.Num() > 8) return false;
	for (const auto& Change : ModifierData->AttributeChanges)
		if (!Change.Attribute.IsValid() || !FMath::IsFinite(Change.Magnitude)
			|| FMath::Abs(Change.Magnitude)>1000000 || Change.ModifierOp!=EGameplayModOp::Additive
			|| Change.Attribute == UCombatAttributeSet::GetIncomingDamageAttribute()
			|| Change.Attribute == UCombatAttributeSet::GetIncomingHealingAttribute()
			|| Change.Attribute == UCombatAttributeSet::GetHealthAttribute()
			|| Change.Attribute == UCombatAttributeSet::GetManaAttribute()) return false;
	for (const auto* List : { &MutuallyExclusive, &Prerequisites })
	{
		TSet<FPrimaryAssetId> Seen;
		for (const auto& Id : *List)
		{
			if (!Id.IsValid() || Id.PrimaryAssetType != GetCombatPrimaryAssetType() || Id == GetPrimaryAssetId() || Seen.Contains(Id)) return false;
			Seen.Add(Id);
		}
	}
	for (const auto& Id : Prerequisites) if (MutuallyExclusive.Contains(Id)) return false;
	for (const auto& Id : CompatibleHeroes) if (!Id.IsValid() || Id.PrimaryAssetType != FPrimaryAssetType(TEXT("CombatUnit"))) return false;
	OutDiagnostic.Reset(); return true;
}
bool UDefenseAugmentPoolData::ValidateRuntime(FString& OutDiagnostic) const
{
	OutDiagnostic = TEXT("强化池身份、版本、重复 ID 或引用闭包非法");
	if (!GetPrimaryAssetId().IsValid() || SchemaVersion != FCombatDefinitionRegistry::CombatContentVersion
		|| PoolSchemaVersion != 1 || Augments.IsEmpty() || Augments.Num() > 64) return false;
	TSet<FPrimaryAssetId> Seen, Effects;
	for (const UDefenseAugmentData* A : Augments)
	{
		FString Error;
		if (!A || Seen.Contains(A->GetPrimaryAssetId()) || !A->ValidateRuntime(Error)) return false;
		if (Effects.Contains(A->ModifierData->GetPrimaryAssetId())) return false;
		Seen.Add(A->GetPrimaryAssetId()); Effects.Add(A->ModifierData->GetPrimaryAssetId());
	}
	// 前置必须能从无前置卡逐步解锁；拒绝循环或指向不存在卡的死配置。
	TSet<FPrimaryAssetId> Reachable;
	for (int32 Pass=0; Pass<Augments.Num(); ++Pass)
		for (const UDefenseAugmentData* A : Augments)
		{
			bool bReady = true;
			for (const auto& Id : A->Prerequisites) bReady &= Reachable.Contains(Id);
			if (bReady) Reachable.Add(A->GetPrimaryAssetId());
		}
	if (Reachable.Num() != Augments.Num()) return false;
	for (const UDefenseAugmentData* A : Augments)
		for (const auto& Id : A->MutuallyExclusive) if (!Seen.Contains(Id)) return false;
	OutDiagnostic.Reset(); return true;
}
#if WITH_EDITOR
EDataValidationResult UDefenseAugmentData::IsDataValid(FDataValidationContext& Context) const
{
	const auto Result=Super::IsDataValid(Context); FString Error;
	if (!ValidateRuntime(Error)) { Context.AddError(FText::FromString(Error)); return EDataValidationResult::Invalid; }
	return Result;
}
EDataValidationResult UDefenseAugmentPoolData::IsDataValid(FDataValidationContext& Context) const
{
	const auto Result=Super::IsDataValid(Context); FString Error;
	if (!ValidateRuntime(Error)) { Context.AddError(FText::FromString(Error)); return EDataValidationResult::Invalid; }
	return Result;
}
#endif
bool FDefenseChoiceRequest::operator==(const FDefenseChoiceRequest& R) const
{
	return RequestId==R.RequestId && RunId==R.RunId && ChoiceRevision==R.ChoiceRevision && Action==R.Action
		&& DefinitionId==R.DefinitionId && LifeGeneration==R.LifeGeneration && BindingGeneration==R.BindingGeneration
		&& EconomyRevision==R.EconomyRevision;
}
