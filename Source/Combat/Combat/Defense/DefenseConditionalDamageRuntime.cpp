#include "Combat/Defense/DefenseConditionalDamageRuntime.h"
#include "Combat/Unit/CombatUnitCharacter.h"
#include "Combat/Data/CombatDefinitionData.h"

bool UDefenseConditionalDamageRuntime::MatchesTarget(const ACombatUnitCharacter* Target,const int32 Role)
{
	if(!Target) return false;
	const FString Id=Target->GetUnitDefinitionId().PrimaryAssetName.ToString().ToLower();
	if(Role==1) return Id.Contains(TEXT("siege"))||Id.Contains(TEXT("assassin"));
	if(Role==2) return Id.Contains(TEXT("boss"));
	return false;
}
void UDefenseConditionalDamageRuntime::OnPreDealDamage_Implementation(FCombatDamageEvent& Event)
{
	const int32 Role=FMath::RoundToInt(GetRuntimeParameter(TEXT("target_role")));
	const float Fraction=FMath::Clamp(GetRuntimeParameter(TEXT("damage_fraction")),0.0f,1.0f);
	if(Fraction>0 && MatchesTarget(Event.Target,Role) && Event.Amount>0) Event.Amount*=1.0f+Fraction;
}
