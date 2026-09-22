#include "Combat/AI/CombatAITacticalTargetContext.h"

#include "Combat/AI/CombatAIBrainComponent.h"
#include "Combat/Unit/CombatUnitCharacter.h"
#include "EnvironmentQuery/Items/EnvQueryItemType_Actor.h"
#include "EnvironmentQuery/Items/EnvQueryItemType_Point.h"

void UCombatAITacticalTargetContext::ProvideContext(
	FEnvQueryInstance& QueryInstance, FEnvQueryContextData& ContextData) const
{
	const ACombatUnitCharacter* Unit = Cast<ACombatUnitCharacter>(QueryInstance.Owner.Get());
	const UCombatAIBrainComponent* Brain = Unit ? Unit->GetCombatAIBrainComponent() : nullptr;
	if (const ACombatUnitCharacter* Target = Brain ? Brain->GetTacticalQueryTarget() : nullptr)
	{
		UEnvQueryItemType_Actor::SetContextHelper(ContextData, Target);
	}
}

UCombatAITacticalLocationGenerator::UCombatAITacticalLocationGenerator(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ItemType = UEnvQueryItemType_Point::StaticClass();
	OptionName = TEXT("Tactical target outside point");
}

void UCombatAITacticalLocationGenerator::GenerateItems(FEnvQueryInstance& QueryInstance) const
{
	const ACombatUnitCharacter* Querier = Cast<ACombatUnitCharacter>(QueryInstance.Owner.Get());
	TArray<FVector> TargetLocations;
	if (!Querier || !FMath::IsFinite(Distance) || Distance <= 0.0f
		|| !QueryInstance.PrepareContext(UCombatAITacticalTargetContext::StaticClass(), TargetLocations)
		|| TargetLocations.Num() != 1 || TargetLocations[0].ContainsNaN())
	{
		return;
	}

	const FVector TargetLocation = TargetLocations[0];
	FVector Away = TargetLocation - Querier->GetActorLocation();
	Away.Z = 0.0f;
	if (!Away.Normalize())
	{
		Away = Querier->GetActorForwardVector();
		Away.Z = 0.0f;
		if (!Away.Normalize()) Away = FVector::ForwardVector;
	}
	FVector Result = TargetLocation + Away * Distance;
	Result.Z = TargetLocation.Z;
	if (!Result.ContainsNaN())
	{
		QueryInstance.AddItemData<UEnvQueryItemType_Point>(Result);
	}
}
