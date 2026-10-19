#include "Combat/Defense/DefenseData.h"
#include "Combat/Defense/DefenseAugmentData.h"
#include "Combat/Unit/CombatUnitCharacter.h"
#include "Misc/DataValidation.h"
#include "Combat/AI/CombatAIProfileData.h"

namespace DefenseData
{
	/** 单位引用只接受可初始化、无自主 Profile 且奖励归导演的固定定义。 */
	bool ValidUnit(const UCombatUnitData* Data)
	{
		return Data && Data->GetPrimaryAssetId().IsValid()
			&& Data->SchemaVersion == FCombatDefinitionRegistry::CombatContentVersion
			&& Data->BaseStats.IsValid() && Data->InitialTeamId.IsValid()
			&& !Data->InitialTeamId.IsNeutralCamp() && !Data->AIProfile
			&& Data->GoldReward == 0 && Data->ExperienceReward == 0;
	}
	/** 单点安全范围同时拒绝 NaN/Inf 与超大世界坐标。 */
	bool ValidPoint(const FVector& P)
	{
		return FMath::IsFinite(P.X) && FMath::IsFinite(P.Y) && FMath::IsFinite(P.Z)
			&& P.GetAbsMax() < 1000000.0;
	}
}
FPrimaryAssetType UDefenseWaveData::GetCombatPrimaryAssetType() const { return FPrimaryAssetType(TEXT("DefenseWave")); }
FPrimaryAssetType UDefenseRunData::GetCombatPrimaryAssetType() const { return FPrimaryAssetType(TEXT("DefenseRun")); }

bool UDefenseWaveData::ValidateRuntime(FString& OutDiagnostic) const
{
	OutDiagnostic = TEXT("波次身份、时限、出生组或强化配置非法");
	if (!GetPrimaryAssetId().IsValid() || SchemaVersion != FCombatDefinitionRegistry::CombatContentVersion
		|| DefenseSchemaVersion != 1 || !FMath::IsFinite(MaxDuration) || MaxDuration < 1 || MaxDuration > 900
		|| SpawnGroups.IsEmpty() || SpawnGroups.Num() > 16 || GoldReward < 0 || GoldReward > 100000
		|| ExperienceReward < 0 || ExperienceReward > 100000) return false;
	int32 Total = 0, Bosses = 0;
	for (const FDefenseSpawnGroup& G : SpawnGroups)
	{
		if (!G.UnitClass || G.UnitClass->HasAnyClassFlags(CLASS_Abstract) || !DefenseData::ValidUnit(G.UnitData)
			|| G.Count < 1 || G.Count > 64 || G.RouteIndex < 0 || !FMath::IsFinite(G.SpawnInterval)
			|| G.SpawnInterval < 0 || G.SpawnInterval > 30) return false;
		if (G.RoleProfile && (G.bBoss || !G.RoleProfile->bEnablePerception || !G.RoleProfile->ValidateRuntime(OutDiagnostic)))
		{ OutDiagnostic = TEXT("路线角色AI必须启用感知、通过编译校验且不能用于Boss组"); return false; }
		const auto* CDO = G.UnitClass->GetDefaultObject<ACombatUnitCharacter>();
		if (CDO->GetUnitData() && CDO->GetUnitData() != G.UnitData) return false;
		Total += G.Count;
		Bosses += G.bBoss ? G.Count : 0;
	}
	if (Total > 128 || (bBossWave ? Bosses != 1 : Bosses != 0)) return false;
	if (FixedAugment)
	{
		if (!FixedAugment->GetPrimaryAssetId().IsValid() || FixedAugment->SchemaVersion != FCombatDefinitionRegistry::CombatContentVersion
			|| FixedAugment->bRemoveOnDeath || FixedAugment->bIsDebuff || FixedAugment->Duration != 0
			|| FixedAugment->MaxStacks != 1) return false;
		for (const auto& Change : FixedAugment->AttributeChanges)
			if (!Change.Attribute.IsValid() || !FMath::IsFinite(Change.Magnitude)) return false;
	}
	OutDiagnostic.Reset();
	return true;
}

bool UDefenseRunData::ValidateRuntime(FString& OutDiagnostic) const
{
	OutDiagnostic = TEXT("对局身份、核心、路线、时限或六波配置非法");
	if (!GetPrimaryAssetId().IsValid() || SchemaVersion != FCombatDefinitionRegistry::CombatContentVersion
		|| DefenseSchemaVersion != 1 || Waves.Num() != 6 || Routes.Num() != 2
		|| !DefenseData::ValidUnit(ObjectiveData) || ObjectiveData->BaseStats.MoveSpeed != 0
		|| ObjectiveData->BaseStats.AttackDamage != 0 || !DefenseData::ValidPoint(ObjectiveLocation)
		|| !DefenseData::ValidPoint(RespawnLocation) || SpawnRetryLimit < 0 || SpawnRetryLimit > 10
		|| !FMath::IsFinite(PreparationDuration) || PreparationDuration < 0 || PreparationDuration > 120
		|| !FMath::IsFinite(IntermissionDuration) || IntermissionDuration < 0 || IntermissionDuration > 120
		|| !FMath::IsFinite(RespawnDelay) || RespawnDelay < 1 || RespawnDelay > 60) return false;
	TSet<FName> RouteNames;
	for (const FDefenseRoute& Route : Routes)
	{
		if (Route.RouteName.IsNone() || RouteNames.Contains(Route.RouteName) || !DefenseData::ValidPoint(Route.SpawnLocation)
			|| Route.Waypoints.Num() > 8) return false;
		RouteNames.Add(Route.RouteName);
		for (const FVector& Point : Route.Waypoints) if (!DefenseData::ValidPoint(Point)) return false;
	}
	TSet<FPrimaryAssetId> Ids;
	for (int32 I = 0; I < Waves.Num(); ++I)
	{
		if (!Waves[I] || Waves[I]->bBossWave != (I == 5) || Ids.Contains(Waves[I]->GetPrimaryAssetId())) return false;
		FString Error;
		if (!Waves[I]->ValidateRuntime(Error)) { OutDiagnostic = Error; return false; }
		Ids.Add(Waves[I]->GetPrimaryAssetId());
		for (const auto& Group : Waves[I]->SpawnGroups) if (!Routes.IsValidIndex(Group.RouteIndex)) return false;
	}
	if (AugmentPool)
	{
		if (!AugmentPool->ValidateRuntime(OutDiagnostic)) return false;
		if(!FMath::IsFinite(ChoiceDuration) || ChoiceDuration<1 || ChoiceDuration>60
			|| PreparationDuration<ChoiceDuration || IntermissionDuration<ChoiceDuration)
		{ OutDiagnostic=TEXT("准备和整备时长必须包含完整选择窗口"); return false; }
	}
	OutDiagnostic.Reset();
	return true;
}

#if WITH_EDITOR
EDataValidationResult UDefenseWaveData::IsDataValid(FDataValidationContext& Context) const
{
	const auto Base = Super::IsDataValid(Context);
	FString Error;
	if (!ValidateRuntime(Error)) { Context.AddError(FText::FromString(Error)); return EDataValidationResult::Invalid; }
	return Base;
}
EDataValidationResult UDefenseRunData::IsDataValid(FDataValidationContext& Context) const
{
	const auto Base = Super::IsDataValid(Context);
	FString Error;
	if (!ValidateRuntime(Error)) { Context.AddError(FText::FromString(Error)); return EDataValidationResult::Invalid; }
	return Base;
}
#endif
#if WITH_DEV_AUTOMATION_TESTS
UDefenseRunData* UDefenseRunData::CreatePhase1Default(UObject* Outer)
{
	auto* Run = NewObject<UDefenseRunData>(Outer);
	Run->DefinitionName = TEXT("defense_fixture");
	Run->PreparationDuration = Run->IntermissionDuration = 0;
	for (int32 I=0; I<2; ++I)
	{
		auto& R = Run->Routes.AddDefaulted_GetRef();
		R.RouteName = I ? TEXT("east") : TEXT("west");
		R.SpawnLocation = FVector(-1000, I ? 500 : -500, 100);
	}
	Run->ObjectiveData = NewObject<UCombatUnitData>(Run);
	Run->ObjectiveData->DefinitionName = TEXT("defense_fixture_core");
	Run->ObjectiveData->BaseStats.MaxHealth = 5000;
	Run->ObjectiveData->BaseStats.AttackDamage = Run->ObjectiveData->BaseStats.MoveSpeed = 0;
	Run->ObjectiveData->ExperienceReward = 0;
	for (int32 I=0; I<6; ++I)
	{
		auto* Wave = NewObject<UDefenseWaveData>(Run);
		Wave->DefinitionName = FName(FString::Printf(TEXT("defense_fixture_wave_%d"), I));
		Wave->bBossWave = I == 5;
		auto& G = Wave->SpawnGroups.AddDefaulted_GetRef();
		G.bBoss = Wave->bBossWave;
		G.UnitClass = ACombatUnitCharacter::StaticClass();
		G.UnitData = NewObject<UCombatUnitData>(Wave);
		G.UnitData->DefinitionName = FName(FString::Printf(TEXT("defense_fixture_enemy_%d"), I));
		G.UnitData->InitialTeamId = FCombatTeamId(2);
		G.UnitData->ExperienceReward = 0;
		G.RouteIndex = I % 2;
		Run->Waves.Add(Wave);
	}
	return Run;
}
#endif
