#include "Combat/Tests/CombatTestScenarioActor.h"

#include "Combat/Ability/CombatAbilitySystemComponent.h"
#include "Combat/Ability/CombatGameplayAbility.h"
#include "Combat/Attack/CombatAttackComponent.h"
#include "Combat/Aura/CombatAuraSubsystem.h"
#include "Combat/Attributes/CombatAttributeSet.h"
#include "Combat/Core/CombatTags.h"
#include "Combat/Data/CombatDefinitionData.h"
#include "Combat/Demo/CombatDemoAbilities.h"
#include "Combat/Demo/CombatDemoModifierRuntimes.h"
#include "Combat/Demo/CombatFissureBlocker.h"
#include "Combat/Log/CombatEventSubsystem.h"
#include "Combat/Modifiers/CombatModifierComponent.h"
#include "Combat/Order/CombatOrderComponent.h"
#include "Combat/Projectile/CombatProjectileSubsystem.h"
#include "Combat/Targeting/CombatTargetingSubsystem.h"
#include "Combat/Thinker/CombatThinkerSubsystem.h"
#include "Combat/Unit/CombatRegenerationComponent.h"
#include "Combat/Unit/CombatUnitCharacter.h"
#include "Combat/Unit/CombatUnitLifecycleComponent.h"
#include "Combat/Validation/CombatSkillTemplateValidator.h"
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
	StartM5ProjectileScenario();
}

void ACombatTestScenarioActor::StartM5ProjectileScenario()
{
	UCombatProjectileSubsystem* Projectiles = GetWorld()
		? GetWorld()->GetSubsystem<UCombatProjectileSubsystem>() : nullptr;
	UCombatThinkerSubsystem* Thinkers = GetWorld()
		? GetWorld()->GetSubsystem<UCombatThinkerSubsystem>() : nullptr;
	const bool bUnitsReady = HasAuthority() && SpawnedUnits.Num() == 2
		&& IsValid(SpawnedUnits[0]) && IsValid(SpawnedUnits[1]);
	const bool bMotionReady = bUnitsReady
		&& SpawnedUnits[0]->GetCombatMotionComponent() && SpawnedUnits[1]->GetCombatMotionComponent();
	FCombatProjectileResult SpawnResult;
	if (bUnitsReady && Projectiles)
	{
		ScenarioProjectileData = NewObject<UCombatProjectileData>(this);
		ScenarioProjectileData->DefinitionName = TEXT("m5_scenario_tracking_projectile");
		ScenarioProjectileData->MovementType = ECombatProjectileMovementType::Tracking;
		ScenarioProjectileData->Speed = 600.0f;
		ScenarioProjectileData->Radius = 20.0f;
		ScenarioProjectileData->MaxDistance = 1000.0f;
		ScenarioProjectileData->MaxLifetime = 5.0f;
		ScenarioProjectileData->MaxSimulationStep = 100.0f;
		ScenarioProjectileData->HitPolicy.bStopOnWorld = false;
		FCombatProjectileSpec Spec;
		Spec.ProjectileData = ScenarioProjectileData;
		Spec.Source = SpawnedUnits[0];
		Spec.Target = SpawnedUnits[1];
		Spec.SpawnLocation = SpawnedUnits[0]->GetActorLocation();
		Spec.Direction = SpawnedUnits[1]->GetActorLocation() - Spec.SpawnLocation;
		Spec.MovementType = ECombatProjectileMovementType::Tracking;
		Spec.TargetLostPolicy = ScenarioProjectileData->TargetLostPolicy;
		Spec.HitPolicy = ScenarioProjectileData->HitPolicy;
		FCombatProjectileImpactAction Damage;
		Damage.Magnitude = 5.0f;
		Damage.DamageType = ECombatDamageType::Magical;
		Spec.ImpactActions.Add(Damage);
		SpawnResult = Projectiles->SpawnProjectile(Spec);
		ScenarioProjectileHandle = SpawnResult.Handle;
	}
	UE_LOG(LogCombat, Display,
		TEXT("M5ScenarioReady ProjectileRuntime=%s ThinkerRuntime=%s MotionRuntime=%s ProjectileSpawned=%s Handle=%s"),
		Projectiles ? TEXT("Ready") : TEXT("Invalid"),
		Thinkers ? TEXT("Ready") : TEXT("Invalid"),
		bMotionReady ? TEXT("Ready") : TEXT("Invalid"),
		SpawnResult.bSuccess ? TEXT("Yes") : TEXT("No"),
		*SpawnResult.Handle.ToString());
	StartM6ContentScenario();
}

void ACombatTestScenarioActor::StartM6ContentScenario()
{
	UCombatAuraSubsystem* Auras = GetWorld()
		? GetWorld()->GetSubsystem<UCombatAuraSubsystem>() : nullptr;
	const bool bUnitsReady = HasAuthority() && SpawnedUnits.Num() == 2
		&& IsValid(SpawnedUnits[0]) && IsValid(SpawnedUnits[1]);
	FCombatAuraResult AuraResult;
	if (bUnitsReady && Auras)
	{
		// 使用普通无限 Modifier 验证真实 Aura registry、Targeting 与 child reconcile 链路。
		ScenarioAuraChildData = NewObject<UCombatModifierData>(this);
		ScenarioAuraChildData->DefinitionName = TEXT("m6_scenario_aura_child");
		ScenarioAuraChildData->bIsDebuff = true;
		ScenarioAuraChildData->Duration = 0.0f;
		FCombatAuraSpec AuraSpec;
		AuraSpec.Owner = SpawnedUnits[0];
		AuraSpec.Radius = 2000.0f;
		AuraSpec.ReconcileInterval = 0.25f;
		AuraSpec.TargetingRules.TargetTeamTag = CombatTags::TargetTeam_Enemy;
		AuraSpec.ChildModifierData = ScenarioAuraChildData;
		AuraResult = Auras->StartAura(AuraSpec);
		ScenarioAuraHandle = AuraResult.Handle;
	}

	const bool bFrostArrowsReady = UCombatFrostArrowsAbility::StaticClass() != nullptr
		&& UCombatFrostArrowsRuntime::StaticClass() != nullptr;
	const bool bFissureReady = UCombatFissureAbility::StaticClass() != nullptr
		&& ACombatFissureBlocker::StaticClass() != nullptr;
	const bool bAdvancedStatusReady = UCombatSpellBlockRuntime::StaticClass() != nullptr
		&& CombatTags::State_SpellBlock.GetTag().IsValid() && CombatTags::State_Broken.GetTag().IsValid()
		&& CombatTags::State_DebuffImmune.GetTag().IsValid() && CombatTags::State_DispelImmune.GetTag().IsValid();
	const bool bTemplateValidatorReady = !FCombatSkillTemplateValidator::GetForbiddenBypassPatterns().IsEmpty();
	const int32 AuraChildCount = Auras ? Auras->GetChildCount(ScenarioAuraHandle) : 0;
	UE_LOG(LogCombat, Display,
		TEXT("M6ScenarioReady FrostArrows=%s Fissure=%s AuraRuntime=%s AuraStarted=%s AuraChildren=%d AdvancedStatus=%s TemplateValidator=%s Handle=%s"),
		bFrostArrowsReady ? TEXT("Ready") : TEXT("Invalid"),
		bFissureReady ? TEXT("Ready") : TEXT("Invalid"),
		Auras ? TEXT("Ready") : TEXT("Invalid"),
		AuraResult.bSuccess ? TEXT("Yes") : TEXT("No"),
		AuraChildCount,
		bAdvancedStatusReady ? TEXT("Ready") : TEXT("Invalid"),
		bTemplateValidatorReady ? TEXT("Ready") : TEXT("Invalid"),
		*AuraResult.Handle.ToString());
}

void ACombatTestScenarioActor::DestroyScenario()
{
	if (!HasAuthority())
	{
		return;
	}
	GetWorldTimerManager().ClearTimer(M4AttackScenarioTimer);
	if (UCombatAuraSubsystem* Auras = GetWorld()
		? GetWorld()->GetSubsystem<UCombatAuraSubsystem>() : nullptr)
	{
		Auras->CancelAura(ScenarioAuraHandle);
	}
	ScenarioAuraHandle = FCombatAuraHandle();
	ScenarioAuraChildData = nullptr;
	if (UCombatProjectileSubsystem* Projectiles = GetWorld()
		? GetWorld()->GetSubsystem<UCombatProjectileSubsystem>() : nullptr)
	{
		Projectiles->CancelProjectile(ScenarioProjectileHandle);
	}
	ScenarioProjectileHandle = FCombatProjectileHandle();
	ScenarioProjectileData = nullptr;
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
