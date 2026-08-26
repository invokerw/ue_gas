#include "Combat/Tests/CombatTestScenarioActor.h"

#include "Combat/Ability/CombatAbilitySystemComponent.h"
#include "Combat/Ability/CombatGameplayAbility.h"
#include "Combat/Attack/CombatAttackComponent.h"
#include "Combat/Attributes/CombatAttributeSet.h"
#include "Combat/Core/CombatTags.h"
#include "Combat/Log/CombatEventSubsystem.h"
#include "Combat/Modifiers/CombatModifierComponent.h"
#include "Combat/Order/CombatOrderComponent.h"
#include "Combat/Targeting/CombatTargetingSubsystem.h"
#include "Combat/Unit/CombatRegenerationComponent.h"
#include "Combat/Unit/CombatUnitCharacter.h"
#include "Combat/Unit/CombatUnitLifecycleComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"

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
	if (GetWorld())
	{
		GetWorldTimerManager().ClearTimer(M4AttackScenarioTimer);
	}
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

	// Character 先在平台顶面完成落地，再基于稳定 feet location 生成 NavMesh 路径。
	GetWorldTimerManager().SetTimer(
		M4AttackScenarioTimer, this, &ACombatTestScenarioActor::StartM4AttackScenario, 1.0f, false);
}

void ACombatTestScenarioActor::StartM4AttackScenario()
{
	const bool bM4OrderAttackReady = HasAuthority() && SpawnedUnits.Num() == 2
		&& IsValid(SpawnedUnits[0]) && IsValid(SpawnedUnits[1])
		&& SpawnedUnits[0]->GetCombatOrderComponent() && SpawnedUnits[0]->GetCombatAttackComponent()
		&& SpawnedUnits[1]->GetCombatOrderComponent() && SpawnedUnits[1]->GetCombatAttackComponent();
	FCombatOrderResult AttackOrderResult;
	if (bM4OrderAttackReady)
	{
		FCombatOrderRequest AttackOrder;
		AttackOrder.Type = ECombatOrderType::AttackTarget;
		AttackOrder.TargetUnit = SpawnedUnits[1];
		AttackOrderResult = SpawnedUnits[0]->GetCombatOrderComponent()->IssueOrder(AttackOrder, false);
	}
	UE_LOG(LogCombat, Display,
		TEXT("M4ScenarioReady Units=%d OrderAttackComponents=%s AttackOrderAccepted=%s Order=%s State=%d Source=%s Target=%s"),
		SpawnedUnits.Num(),
		bM4OrderAttackReady ? TEXT("Ready") : TEXT("Invalid"),
		AttackOrderResult.bSuccess ? TEXT("Yes") : TEXT("No"),
		*AttackOrderResult.Handle.ToString(),
		SpawnedUnits.Num() > 0 && SpawnedUnits[0] && SpawnedUnits[0]->GetCombatOrderComponent()
			? static_cast<int32>(SpawnedUnits[0]->GetCombatOrderComponent()->GetCurrentState()) : -1,
		SpawnedUnits.Num() > 0 && SpawnedUnits[0]
			? *SpawnedUnits[0]->GetActorLocation().ToCompactString() : TEXT("Invalid"),
		SpawnedUnits.Num() > 1 && SpawnedUnits[1]
			? *SpawnedUnits[1]->GetActorLocation().ToCompactString() : TEXT("Invalid"));
}

void ACombatTestScenarioActor::DestroyScenario()
{
	if (!HasAuthority())
	{
		return;
	}
	GetWorldTimerManager().ClearTimer(M4AttackScenarioTimer);
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
	// 测试地图包含带厚度的 StaticMesh；让 UE 先寻找非穿透位置，避免 CharacterMovement 被初始重叠锁死。
	Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	const FVector SpawnLocation = GetActorTransform().TransformPosition(RelativeOffset);
	ACombatUnitCharacter* Unit = GetWorld()->SpawnActor<ACombatUnitCharacter>(UnitClass, SpawnLocation, GetActorRotation(), Parameters);
	if (Unit && Unit->GetCombatTeamId() != FCombatTeamId(TeamValue))
	{
		Unit->SetCombatTeamId(FCombatTeamId(TeamValue));
	}
	return Unit;
}
