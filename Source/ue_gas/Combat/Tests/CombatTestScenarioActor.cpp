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
#include "Combat/Debug/CombatDebugSubsystem.h"
#include "Combat/Log/CombatEventSubsystem.h"
#include "Combat/Modifiers/CombatModifierComponent.h"
#include "Combat/Order/CombatOrderComponent.h"
#include "Combat/Network/CombatNetworkSecuritySubsystem.h"
#include "Combat/Performance/CombatPerformanceBudget.h"
#include "Combat/Projectile/CombatProjectileSubsystem.h"
#include "Combat/Release/CombatReleaseContract.h"
#include "Combat/Targeting/CombatTargetingSubsystem.h"
#include "Combat/Thinker/CombatThinkerSubsystem.h"
#include "Combat/Unit/CombatRegenerationComponent.h"
#include "Combat/Unit/CombatUnitCharacter.h"
#include "Combat/Unit/CombatUnitLifecycleComponent.h"
#include "Combat/View/CombatUnitViewComponent.h"
#include "Combat/Validation/CombatAssetValidationCommandlet.h"
#include "Combat/Validation/CombatSkillTemplateValidator.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "TimerManager.h"
#include "ue_gasCharacter.h"
#include "ue_gasPlayerController.h"

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
	else if (!HasAuthority()
		&& (FParse::Param(FCommandLine::Get(), TEXT("CombatM7ClientSmoke"))
			|| FParse::Param(FCommandLine::Get(), TEXT("CombatSAMMovementSmoke"))))
	{
		GetWorldTimerManager().SetTimer(
			M7ClientRpcTimer, this, &ACombatTestScenarioActor::StartM7ClientRpcSmoke, 5.0f, false);
		if (FParse::Param(FCommandLine::Get(), TEXT("CombatSAMMovementSmoke")))
		{
			GetWorldTimerManager().SetTimer(
				SamClientPositionTimer, this, &ACombatTestScenarioActor::LogSamClientPositions, 10.0f, false);
		}
	}
}

void ACombatTestScenarioActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (GetWorld())
	{
		GetWorldTimerManager().ClearTimer(M4AttackScenarioTimer);
		GetWorldTimerManager().ClearTimer(M7NetworkScenarioTimer);
		GetWorldTimerManager().ClearTimer(M7ClientRpcTimer);
		GetWorldTimerManager().ClearTimer(M7PerformanceTimer);
		GetWorldTimerManager().ClearTimer(SamMovementTimer);
		GetWorldTimerManager().ClearTimer(SamClientPositionTimer);
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
	GetWorldTimerManager().SetTimer(
		M7NetworkScenarioTimer, this, &ACombatTestScenarioActor::StartM7NetworkScenario, 3.0f, false);
}

void ACombatTestScenarioActor::StartM7NetworkScenario()
{
	if (!HasAuthority() || SpawnedUnits.Num() < 2)
	{
		return;
	}

	TArray<APlayerController*> Players;
	for (TActorIterator<APlayerController> It(GetWorld()); It; ++It)
	{
		if (IsValid(*It))
		{
			Players.Add(*It);
		}
	}
	Players.Sort([](const APlayerController& A, const APlayerController& B) { return A.GetUniqueID() < B.GetUniqueID(); });
	if (Players.Num() < 2)
	{
		UE_LOG(LogCombat, Display, TEXT("M7ScenarioWaiting Players=%d Required=2"), Players.Num());
		GetWorldTimerManager().SetTimer(
			M7NetworkScenarioTimer, this, &ACombatTestScenarioActor::StartM7NetworkScenario, 1.0f, false);
		return;
	}
	for (int32 Index = 0; Index < FMath::Min(Players.Num(), 2); ++Index)
	{
		// 前序攻击场景可能已击杀 Team 2；联机冒烟前复活，确保两端都能验证业务 Order 已执行。
		if (SpawnedUnits[Index]->GetLifeState() == ECombatLifeState::Dead)
		{
			SpawnedUnits[Index]->GetCombatLifecycleComponent()->RespawnAtLocation(SpawnedUnits[Index]->GetActorLocation());
		}
		if (Aue_gasPlayerController* CombatPlayer = Cast<Aue_gasPlayerController>(Players[Index]))
		{
			ACombatUnitCharacter* InitialDemoUnit = CombatPlayer->GetCommandedUnit();
			CombatPlayer->SetCommandedUnitAuthority(SpawnedUnits[Index]);
			// Combat Demo GameMode 会先生成一个初始单位；测试接管场景单位后将其销毁，保持容量夹具精确为 64 Unit。
			if (IsValid(InitialDemoUnit) && InitialDemoUnit != SpawnedUnits[Index]
				&& !SpawnedUnits.Contains(InitialDemoUnit))
			{
				InitialDemoUnit->Destroy();
			}
		}
		else
		{
			UE_LOG(LogCombat, Error, TEXT("SAMScenarioInvalidPlayerController Controller=%s"), *GetNameSafe(Players[Index]));
		}
	}
	// 保留至少一个无 PlayerController Owner 的单位，验证纯 AI 走 Minimal。
	if (SpawnedUnits.Num() == 2)
	{
		if (ACombatUnitCharacter* AiUnit = SpawnUnit(TeamOneOffset + FVector(0.0, 350.0, 0.0), 1))
		{
			SpawnedUnits.Add(AiUnit);
		}
	}
	const bool bCapacityFixtureReady = ExpandM7CapacityScenario();

	int32 MixedUnits = 0;
	int32 MinimalUnits = 0;
	int32 ReadyViews = 0;
	for (const ACombatUnitCharacter* Unit : SpawnedUnits)
	{
		MixedUnits += Unit && Unit->GetEffectiveAscReplicationPolicy() == ECombatAscReplicationPolicy::Mixed ? 1 : 0;
		MinimalUnits += Unit && Unit->GetEffectiveAscReplicationPolicy() == ECombatAscReplicationPolicy::Minimal ? 1 : 0;
		ReadyViews += Unit && Unit->GetCombatUnitViewComponent() ? 1 : 0;
	}
	const UCombatDebugSubsystem* Debug = GetWorld()->GetSubsystem<UCombatDebugSubsystem>();
	const UCombatNetworkSecuritySubsystem* Security = GetWorld()->GetSubsystem<UCombatNetworkSecuritySubsystem>();
	const UCombatAssetValidationSettings* Validation = GetDefault<UCombatAssetValidationSettings>();
	const FCombatRuntimeMetrics Metrics = Debug ? Debug->CaptureMetrics() : FCombatRuntimeMetrics();
	const FCombatPerformanceBudgetResult BudgetResult = FCombatPerformanceBudgetEvaluator::Evaluate(Metrics, FCombatPerformanceBudget());
	UE_LOG(LogCombat, Display,
		TEXT("M7ScenarioReady Players=%d Units=%d Mixed=%d Minimal=%d UnitViews=%d Security=%s Debug=%s ValidationVersion=%d CapacityFixture=%s CapacityBudget=%s Metrics={%s}"),
		Players.Num(), SpawnedUnits.Num(), MixedUnits, MinimalUnits, ReadyViews,
		Security ? TEXT("Ready") : TEXT("Invalid"), Debug ? TEXT("Ready") : TEXT("Invalid"),
		Validation ? Validation->ContentVersion : 0,
		bCapacityFixtureReady ? TEXT("Ready") : TEXT("Invalid"),
		BudgetResult.bPassed ? TEXT("Pass") : TEXT("Fail"), *Metrics.ToString());
	LogM8ReleaseContract();
	LogM7PerformanceSnapshot();
	if (FParse::Param(FCommandLine::Get(), TEXT("CombatSAMMovementSmoke")))
	{
		StartSamMovementConsistencyScenario();
	}
	GetWorldTimerManager().SetTimer(
		M7PerformanceTimer, this, &ACombatTestScenarioActor::LogM7PerformanceSnapshot, 30.0f, true);
}

void ACombatTestScenarioActor::LogM8ReleaseContract()
{
	const FCombatReleaseContract Contract = UCombatReleaseContractLibrary::GetCombatReleaseContract();
	FString Error;
	const bool bValid = Contract.IsSelfConsistent(Error);
	UE_LOG(LogCombat, Display,
		TEXT("M8ReleaseContract Valid=%s Contract=%d Release=%s Content=%d Tags=%d Formula=%d RNG=%d Event=%d Authority=%s ProjectilePrediction=%s GameplayRollback=%s Replay=%s Summons=%s Economy=%s Error=%s"),
		bValid ? TEXT("Pass") : TEXT("Fail"), Contract.ContractVersion, *Contract.ReleaseId.ToString(),
		Contract.ContentVersion, Contract.GameplayTagSchemaVersion, Contract.FormulaVersion, Contract.RngAlgorithmVersion, Contract.EventSchemaVersion,
		Contract.bServerAuthoritativeGameplay ? TEXT("Server") : TEXT("Invalid"),
		Contract.bProjectileVisualPrediction ? TEXT("VisualOnly") : TEXT("Disabled"),
		Contract.bGameplayRollback ? TEXT("Enabled") : TEXT("DeferredPostV1"),
		Contract.bDeterministicReplay ? TEXT("Enabled") : TEXT("DeferredPostV1"),
		Contract.bSummonsAndIllusions ? TEXT("Enabled") : TEXT("DeferredPostV1"),
		Contract.bItemsAndEconomy ? TEXT("Enabled") : TEXT("DeferredPostV1"),
		Error.IsEmpty() ? TEXT("None") : *Error);
}

void ACombatTestScenarioActor::StartM7ClientRpcSmoke()
{
	if (HasAuthority())
	{
		return;
	}
	Aue_gasPlayerController* LocalController = GetWorld()
		? Cast<Aue_gasPlayerController>(GetWorld()->GetFirstPlayerController()) : nullptr;
	ACombatUnitCharacter* CommandedUnit = LocalController ? LocalController->GetCommandedUnit() : nullptr;
	if (LocalController && CommandedUnit && CommandedUnit->GetOwner() == LocalController)
	{
		FCombatOrderBatchRequest Request;
		Request.RequestId = 7001 + static_cast<int32>(LocalController->GetUniqueID() % 100000u);
		FCombatOrderRequest& Order = Request.Orders.AddDefaulted_GetRef();
		// Stop 不依赖 NavMesh、位置误差或既有命令状态，适合作为双客户端 RPC 冒烟的确定性业务载荷。
		Order.Type = ECombatOrderType::Stop;
		CommandedUnit->ServerIssueOrderBatch(Request);
		UE_LOG(LogCombat, Display,
			TEXT("SAMClientRpcSmoke Submitted RequestId=%d Unit=%s UnitLocalRole=%d Pawn=%s BindingGeneration=%d"),
			Request.RequestId, *CommandedUnit->GetName(), static_cast<int32>(CommandedUnit->GetLocalRole()),
			*GetNameSafe(LocalController->GetPawn()), LocalController->GetCommandBindingGeneration());
		return;
	}
	UE_LOG(LogCombat, Display, TEXT("SAMClientRpcSmoke WaitingForCommandedUnit"));
	GetWorldTimerManager().SetTimer(
		M7ClientRpcTimer, this, &ACombatTestScenarioActor::StartM7ClientRpcSmoke, 1.0f, false);
}

void ACombatTestScenarioActor::StartSamMovementConsistencyScenario()
{
	if (!HasAuthority() || SpawnedUnits.Num() < 2)
	{
		return;
	}
	ACombatUnitCharacter* MovingUnit = SpawnedUnits[0];
	ACombatUnitCharacter* StationaryUnit = SpawnedUnits[1];
	if (!MovingUnit || !StationaryUnit)
	{
		return;
	}
	MovingUnit->GetCombatOrderComponent()->StopAllOrders(CombatTags::Order_Failure_Cancelled);
	StationaryUnit->GetCombatOrderComponent()->StopAllOrders(CombatTags::Order_Failure_Cancelled);
	const FVector Origin = GetActorLocation() + FVector(0.0, 0.0, 288.0);
	MovingUnit->SetActorLocation(Origin + FVector(-400.0, 0.0, 0.0), false, nullptr, ETeleportType::TeleportPhysics);
	StationaryUnit->SetActorLocation(Origin, false, nullptr, ETeleportType::TeleportPhysics);
	SamMovingUnitStart = MovingUnit->GetActorLocation();
	SamStationaryUnitStart = StationaryUnit->GetActorLocation();

	FCombatOrderRequest MoveOrder;
	MoveOrder.Type = ECombatOrderType::MoveToPoint;
	MoveOrder.TargetLocation = SamStationaryUnitStart;
	MoveOrder.bHasTargetLocation = true;
	const FCombatOrderResult Result = MovingUnit->GetCombatOrderComponent()->IssueOrder(MoveOrder, false);
	MovingUnit->LogServerMovementTopology(TEXT("SAMCollisionStartMoving"));
	StationaryUnit->LogServerMovementTopology(TEXT("SAMCollisionStartStationary"));
	UE_LOG(LogCombat, Display,
		TEXT("SAMCollisionStart Accepted=%s Moving=%s Start=%s Stationary=%s Start=%s"),
		Result.bSuccess ? TEXT("Yes") : TEXT("No"), *MovingUnit->GetName(),
		*MovingUnit->GetActorLocation().ToCompactString(), *StationaryUnit->GetName(),
		*SamStationaryUnitStart.ToCompactString());
	GetWorldTimerManager().SetTimer(
		SamMovementTimer, this, &ACombatTestScenarioActor::FinishSamMovementConsistencyScenario, 5.0f, false);
}

void ACombatTestScenarioActor::FinishSamMovementConsistencyScenario()
{
	if (!HasAuthority() || SpawnedUnits.Num() < 2 || !SpawnedUnits[0] || !SpawnedUnits[1])
	{
		return;
	}
	const FVector StationaryEnd = SpawnedUnits[1]->GetActorLocation();
	const FVector MovingEnd = SpawnedUnits[0]->GetActorLocation();
	// 同时要求 A 真实推进，避免“双方都没动”误通过；落地时的 Z 校正属于 CharacterMovement 正常行为。
	const float MovingNetDisplacement = FVector::Dist2D(MovingEnd, SamMovingUnitStart);
	const float StationaryNetDisplacement = FVector::Dist2D(StationaryEnd, SamStationaryUnitStart);
	const bool bPassed = MovingNetDisplacement >= 100.0f && StationaryNetDisplacement <= 1.0f;
	UE_LOG(LogCombat, Display,
		TEXT("SAMCollisionServerResult MovingStart=%s MovingEnd=%s MovingNetDisplacement2D=%.3f Minimum=100.000 StationaryStart=%s StationaryEnd=%s StationaryNetDisplacement2D=%.3f Tolerance=1.000 Result=%s"),
		*SamMovingUnitStart.ToCompactString(), *MovingEnd.ToCompactString(), MovingNetDisplacement,
		*SamStationaryUnitStart.ToCompactString(), *StationaryEnd.ToCompactString(), StationaryNetDisplacement,
		bPassed ? TEXT("Pass") : TEXT("Fail"));
}

void ACombatTestScenarioActor::LogSamClientPositions()
{
	if (HasAuthority() || !GetWorld())
	{
		return;
	}
	const Aue_gasPlayerController* LocalController =
		Cast<Aue_gasPlayerController>(GetWorld()->GetFirstPlayerController());
	const ACombatUnitCharacter* LocalUnit = LocalController ? LocalController->GetCommandedUnit() : nullptr;
	UE_LOG(LogCombat, Display,
		TEXT("SAMCollisionClientTopology Controller=%s Pawn=%s CommandedUnit=%s BindingGeneration=%d UnitLocalRole=%d UnitLocation=%s"),
		*GetNameSafe(LocalController), *GetNameSafe(LocalController ? LocalController->GetPawn() : nullptr),
		*GetNameSafe(LocalUnit), LocalController ? LocalController->GetCommandBindingGeneration() : 0,
		LocalUnit ? static_cast<int32>(LocalUnit->GetLocalRole()) : -1,
		LocalUnit ? *LocalUnit->GetActorLocation().ToCompactString() : TEXT("None"));
	for (TActorIterator<ACombatUnitCharacter> It(GetWorld()); It; ++It)
	{
		UE_LOG(LogCombat, Display, TEXT("SAMCollisionClientUnit Unit=%s Role=%d Location=%s"),
			*It->GetName(), static_cast<int32>(It->GetLocalRole()), *It->GetActorLocation().ToCompactString());
	}
}

void ACombatTestScenarioActor::LogM7PerformanceSnapshot()
{
	if (!HasAuthority() || !GetWorld())
	{
		return;
	}
	const UCombatDebugSubsystem* Debug = GetWorld()->GetSubsystem<UCombatDebugSubsystem>();
	const FCombatRuntimeMetrics Metrics = Debug ? Debug->CaptureMetrics() : FCombatRuntimeMetrics();
	const FCombatPerformanceBudgetResult Result = FCombatPerformanceBudgetEvaluator::Evaluate(Metrics, FCombatPerformanceBudget());
	UE_LOG(LogCombat, Display, TEXT("M7Performance Budget=%s Violations=%s Metrics={%s}"),
		Result.bPassed ? TEXT("Pass") : TEXT("Fail"),
		Result.Violations.IsEmpty() ? TEXT("None") : *FString::Join(Result.Violations, TEXT(" | ")),
		*Metrics.ToString());
}

bool ACombatTestScenarioActor::ExpandM7CapacityScenario()
{
	if (!FParse::Param(FCommandLine::Get(), TEXT("CombatM7CapacitySmoke")))
	{
		return true;
	}
	// 容量边界需要精确 256 个 Modifier；先移除前序 Aura 及其 child，再建立稳定负载。
	if (UCombatAuraSubsystem* Auras = GetWorld()->GetSubsystem<UCombatAuraSubsystem>())
	{
		Auras->CancelAura(ScenarioAuraHandle);
	}
	ScenarioAuraHandle = FCombatAuraHandle();
	ScenarioAuraChildData = nullptr;

	constexpr int32 TargetUnits = 64;
	constexpr int32 ModifiersPerUnit = 4;
	while (SpawnedUnits.Num() < TargetUnits)
	{
		const int32 Index = SpawnedUnits.Num();
		const FVector Offset(
			800.0 + static_cast<double>(Index % 8) * 180.0,
			-700.0 + static_cast<double>(Index / 8) * 180.0,
			288.0);
		ACombatUnitCharacter* Unit = SpawnUnit(Offset, static_cast<uint8>((Index % 2) + 1));
		if (!Unit)
		{
			UE_LOG(LogCombat, Error, TEXT("M7CapacityFixture SpawnFailed Index=%d"), Index);
			return false;
		}
		SpawnedUnits.Add(Unit);
	}

	ScenarioCapacityModifierData.Reserve(TargetUnits * ModifiersPerUnit);
	for (int32 UnitIndex = 0; UnitIndex < TargetUnits; ++UnitIndex)
	{
		ACombatUnitCharacter* Unit = SpawnedUnits[UnitIndex];
		for (int32 ModifierIndex = 0; ModifierIndex < ModifiersPerUnit; ++ModifierIndex)
		{
			UCombatModifierData* Data = NewObject<UCombatModifierData>(this);
			Data->DefinitionName = FName(*FString::Printf(TEXT("m7_capacity_soak_%02d_%d"), UnitIndex, ModifierIndex));
			Data->Duration = 0.0f;
			Data->bIsDebuff = false;
			ScenarioCapacityModifierData.Add(Data);
			FCombatModifierApplyRequest Request;
			Request.Source = Unit;
			Request.ModifierData = Data;
			if (!Unit->GetCombatModifierComponent()->ApplyModifier(Request).bSuccess)
			{
				UE_LOG(LogCombat, Error, TEXT("M7CapacityFixture ModifierFailed Unit=%d Modifier=%d"), UnitIndex, ModifierIndex);
				return false;
			}
		}
	}
	UE_LOG(LogCombat, Display, TEXT("M7CapacityFixtureReady Units=%d Modifiers=%d"), TargetUnits, TargetUnits * ModifiersPerUnit);
	return true;
}

void ACombatTestScenarioActor::DestroyScenario()
{
	if (!HasAuthority())
	{
		return;
	}
	GetWorldTimerManager().ClearTimer(M4AttackScenarioTimer);
	GetWorldTimerManager().ClearTimer(M7NetworkScenarioTimer);
	GetWorldTimerManager().ClearTimer(M7ClientRpcTimer);
	GetWorldTimerManager().ClearTimer(M7PerformanceTimer);
	GetWorldTimerManager().ClearTimer(SamMovementTimer);
	GetWorldTimerManager().ClearTimer(SamClientPositionTimer);
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
	ScenarioCapacityModifierData.Reset();
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
