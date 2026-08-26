#include "Combat/Tests/CombatTestScenarioActor.h"

#include "Combat/Ability/CombatAbilitySystemComponent.h"
#include "Combat/Ability/CombatGameplayAbility.h"
#include "Combat/Attributes/CombatAttributeSet.h"
#include "Combat/Core/CombatTags.h"
#include "Combat/Log/CombatEventSubsystem.h"
#include "Combat/Modifiers/CombatModifierComponent.h"
#include "Combat/Targeting/CombatTargetingSubsystem.h"
#include "Combat/Unit/CombatRegenerationComponent.h"
#include "Combat/Unit/CombatUnitCharacter.h"
#include "Combat/Unit/CombatUnitLifecycleComponent.h"
#include "Engine/World.h"

ACombatTestScenarioActor::ACombatTestScenarioActor()
{
	PrimaryActorTick.bCanEverTick = false;
#if WITH_EDITOR
	SetIsSpatiallyLoaded(false);
#endif
	UnitClass = ACombatUnitCharacter::StaticClass();
}

void ACombatTestScenarioActor::BeginPlay()
{
	Super::BeginPlay();
	if (bAutoSpawnOnBeginPlay && HasAuthority())
	{
		SpawnScenario();
	}
}

void ACombatTestScenarioActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (EndPlayReason == EEndPlayReason::Destroyed)
	{
		DestroyScenario();
	}
	else
	{
		SpawnedUnits.Reset();
	}
	Super::EndPlay(EndPlayReason);
}

void ACombatTestScenarioActor::SpawnScenario()
{
	if (!HasAuthority() || !UnitClass)
	{
		return;
	}
	DestroyScenario();
	SpawnedUnits.Add(SpawnUnit(TeamOneOffset, 1));
	SpawnedUnits.Add(SpawnUnit(TeamTwoOffset, 2));
	SpawnedUnits.RemoveAll([](const ACombatUnitCharacter* Unit) { return !IsValid(Unit); });

	// 结构化日志同时承担 Dedicated smoke 的场景断言，避免只凭 Actor 数量误判 ASC 已就绪。
	int32 TeamOneCount = 0;
	int32 TeamTwoCount = 0;
	bool bAllActorInfoInitialized = SpawnedUnits.Num() == 2;
	bool bAllAlive = SpawnedUnits.Num() == 2;
	bool bM2CoreReady = SpawnedUnits.Num() == 2;
	const bool bM3AbilityReady = GetWorld()->GetSubsystem<UCombatTargetingSubsystem>() != nullptr
		&& UCombatGameplayAbility::StaticClass() != nullptr;
	for (const ACombatUnitCharacter* Unit : SpawnedUnits)
	{
		TeamOneCount += Unit->GetCombatTeamId() == FCombatTeamId(1) ? 1 : 0;
		TeamTwoCount += Unit->GetCombatTeamId() == FCombatTeamId(2) ? 1 : 0;
		const UCombatAbilitySystemComponent* AbilitySystem = Unit->GetCombatAbilitySystemComponent();
		bAllActorInfoInitialized &= AbilitySystem && AbilitySystem->IsCombatActorInfoInitialized();
		bAllAlive &= AbilitySystem && AbilitySystem->HasMatchingGameplayTag(CombatTags::State_Alive);
		bM2CoreReady &= Unit->GetCombatAttributeSet() && Unit->GetCombatModifierComponent()
			&& Unit->GetCombatLifecycleComponent() && Unit->GetCombatRegenerationComponent()
			&& AbilitySystem
			&& AbilitySystem->GetNumericAttribute(UCombatAttributeSet::GetMaxHealthAttribute()) >= 1.0f;
	}

	UE_LOG(LogCombat, Display,
		TEXT("M3ScenarioReady Units=%d Team1=%d Team2=%d ASCActorInfo=%s State.Alive=%s CoreComponents=%s AbilityRuntime=%s"),
		SpawnedUnits.Num(), TeamOneCount, TeamTwoCount,
		bAllActorInfoInitialized ? TEXT("Ready") : TEXT("Invalid"),
		bAllAlive ? TEXT("Present") : TEXT("Missing"),
		bM2CoreReady ? TEXT("Ready") : TEXT("Invalid"),
		bM3AbilityReady ? TEXT("Ready") : TEXT("Invalid"));
}

void ACombatTestScenarioActor::DestroyScenario()
{
	if (!HasAuthority())
	{
		return;
	}
	for (ACombatUnitCharacter* Unit : SpawnedUnits)
	{
		if (IsValid(Unit))
		{
			Unit->Destroy();
		}
	}
	SpawnedUnits.Reset();
}

void ACombatTestScenarioActor::RespawnScenario()
{
	SpawnScenario();
}

int32 ACombatTestScenarioActor::GetSpawnedUnitCount() const
{
	int32 Count = 0;
	for (const ACombatUnitCharacter* Unit : SpawnedUnits)
	{
		Count += IsValid(Unit) ? 1 : 0;
	}
	return Count;
}

ACombatUnitCharacter* ACombatTestScenarioActor::SpawnUnit(const FVector& RelativeOffset, const uint8 TeamValue)
{
	FActorSpawnParameters Parameters;
	Parameters.Owner = this;
	Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	const FVector SpawnLocation = GetActorTransform().TransformPosition(RelativeOffset);
	ACombatUnitCharacter* Unit = GetWorld()->SpawnActor<ACombatUnitCharacter>(UnitClass, SpawnLocation, GetActorRotation(), Parameters);
	if (Unit && Unit->GetCombatTeamId() != FCombatTeamId(TeamValue))
	{
		Unit->SetCombatTeamId(FCombatTeamId(TeamValue));
	}
	return Unit;
}
