#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/World.h"
#include "Misc/AutomationTest.h"

#include "Combat/Ability/CombatAbilitySystemComponent.h"
#include "Combat/Attributes/CombatAttributeSet.h"
#include "Combat/Attack/CombatAttackComponent.h"
#include "Combat/Core/CombatTags.h"
#include "Combat/Data/CombatDefinitionData.h"
#include "Combat/Demo/CombatDemoAbilities.h"
#include "Combat/Demo/CombatDemoModifierRuntimes.h"
#include "Combat/Modifiers/CombatModifierComponent.h"
#include "Combat/Motion/CombatMotionComponent.h"
#include "Combat/Projectile/CombatProjectileSubsystem.h"
#include "Combat/Scheduling/CombatSchedulerSubsystem.h"
#include "Combat/Tests/CombatAutomationWorldFixture.h"
#include "Combat/Thinker/CombatThinkerSubsystem.h"
#include "Combat/Unit/CombatUnitCharacter.h"

namespace CombatProjectileMotionTests
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;

	/** 生成带稳定定义、队伍和基础属性的 Combat Unit。 */
	ACombatUnitCharacter* SpawnUnit(
		UWorld& World,
		const FName DefinitionName,
		const FVector Location,
		const uint8 Team,
		const FCombatUnitBaseStats* OverrideStats = nullptr)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		ACombatUnitCharacter* Unit = World.SpawnActor<ACombatUnitCharacter>(Location, FRotator::ZeroRotator, Params);
		if (!Unit)
		{
			return nullptr;
		}
		UCombatUnitData* Data = NewObject<UCombatUnitData>(Unit);
		Data->DefinitionName = DefinitionName;
		Data->InitialTeamId = FCombatTeamId(Team);
		if (OverrideStats)
		{
			Data->BaseStats = *OverrideStats;
		}
		return Unit->InitializeFromUnitData(Data) ? Unit : nullptr;
	}

	/** 创建满足运行时校验的瞬态 ProjectileData。 */
	UCombatProjectileData* MakeProjectileData(
		UObject& Outer,
		const FName DefinitionName,
		const float Speed = 1000.0f,
		const float Radius = 20.0f,
		const float Distance = 600.0f)
	{
		UCombatProjectileData* Data = NewObject<UCombatProjectileData>(&Outer);
		Data->DefinitionName = DefinitionName;
		Data->Speed = Speed;
		Data->Radius = Radius;
		Data->MaxDistance = Distance;
		Data->MaxLifetime = 10.0f;
		Data->MaxSimulationStep = 100.0f;
		Data->HitPolicy.bStopOnWorld = false;
		return Data;
	}

	/** 向 AbilityData 写入单级 special。 */
	void AddSpecial(UCombatAbilityData& Data, const FName Key, const float Value)
	{
		FCombatSpecialValue Special;
		Special.Values.Add(Value);
		Data.SpecialValues.Add(Key, Special);
	}

	/** 通过正式 ASC 入口授予指定示例技能。 */
	template<typename TAbility>
	FGameplayAbilitySpecHandle GrantAbility(UCombatAbilitySystemComponent& Asc, UCombatAbilityData& Data)
	{
		GetMutableDefault<TAbility>()->AbilityData = &Data;
		FGameplayAbilitySpecHandle Handle;
		FGameplayTag FailureTag;
		Asc.GrantCombatAbility(TAbility::StaticClass(), 1, false, Handle, FailureTag);
		return Handle;
	}
}

/** 验证 Linear 穿透、阵营过滤、AlreadyHit 与 Tracking 目标丢失策略。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCombatProjectilePoliciesTest,
	"Combat.ProjectileMotion.Projectile.LinearAndTrackingPolicies",
	CombatProjectileMotionTests::Flags)

bool FCombatProjectilePoliciesTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FCombatAutomationWorldFixture Fixture;
	if (!Fixture.IsValid()) { AddError(TEXT("Could not create M5 projectile world")); return false; }
	UWorld& World = *Fixture.GetWorld();
	ACombatUnitCharacter* Source = CombatProjectileMotionTests::SpawnUnit(
		World, TEXT("projectile_source"), FVector::ZeroVector, 1);
	ACombatUnitCharacter* EnemyA = CombatProjectileMotionTests::SpawnUnit(
		World, TEXT("projectile_enemy_a"), FVector(200.0, 0.0, 0.0), 2);
	ACombatUnitCharacter* Friendly = CombatProjectileMotionTests::SpawnUnit(
		World, TEXT("projectile_friendly"), FVector(300.0, 0.0, 0.0), 1);
	ACombatUnitCharacter* EnemyB = CombatProjectileMotionTests::SpawnUnit(
		World, TEXT("projectile_enemy_b"), FVector(400.0, 0.0, 0.0), 2);
	if (!Source || !EnemyA || !Friendly || !EnemyB) { return false; }

	UCombatProjectileSubsystem* Projectiles = World.GetSubsystem<UCombatProjectileSubsystem>();
	UCombatProjectileData* Data = CombatProjectileMotionTests::MakeProjectileData(
		*Source, TEXT("linear_piercing_projectile"));
	Data->HitPolicy.bDestroyOnFirstUnitHit = false;
	FCombatProjectileSpec Spec;
	Spec.ProjectileData = Data;
	Spec.Source = Source;
	Spec.SpawnLocation = Source->GetActorLocation();
	Spec.Direction = FVector::ForwardVector;
	Spec.MovementType = ECombatProjectileMovementType::Linear;
	Spec.HitPolicy = Data->HitPolicy;
	FCombatProjectileImpactAction Damage;
	Damage.Magnitude = 10.0f;
	Damage.DamageType = ECombatDamageType::Pure;
	Spec.ImpactActions.Add(Damage);
	int32 FinishCount = 0;
	Projectiles->OnProjectileFinished().AddLambda(
		[&FinishCount](const FCombatProjectileResult& Result) { (void)Result; ++FinishCount; });
	const FCombatProjectileResult Spawned = Projectiles->SpawnProjectile(Spec);
	TestTrue(TEXT("Piercing linear projectile spawns"), Spawned.bSuccess);
	TestFalse(TEXT("Advancing to max distance finishes the projectile"),
		Projectiles->AdvanceProjectile(Spawned.Handle, 0.7f));
	TestEqual(TEXT("First hostile is hit once"),
		EnemyA->GetCombatAbilitySystemComponent()->GetNumericAttribute(UCombatAttributeSet::GetHealthAttribute()), 90.0f);
	TestEqual(TEXT("Second hostile is hit once"),
		EnemyB->GetCombatAbilitySystemComponent()->GetNumericAttribute(UCombatAttributeSet::GetHealthAttribute()), 90.0f);
	TestEqual(TEXT("Friendly overlap is filtered by the frozen team policy"),
		Friendly->GetCombatAbilitySystemComponent()->GetNumericAttribute(UCombatAttributeSet::GetHealthAttribute()), 100.0f);
	TestEqual(TEXT("Projectile finishes exactly once"), FinishCount, 1);
	TestFalse(TEXT("Stale projectile handle cannot advance"), Projectiles->AdvanceProjectile(Spawned.Handle, 1.0f));
	TestEqual(TEXT("Stale advance cannot emit a second finish"), FinishCount, 1);

	UCombatProjectileData* TrackingData = CombatProjectileMotionTests::MakeProjectileData(
		*Source, TEXT("tracking_fizzle_projectile"), 100.0f, 10.0f, 1000.0f);
	FCombatProjectileSpec TrackingSpec;
	TrackingSpec.ProjectileData = TrackingData;
	TrackingSpec.Source = Source;
	TrackingSpec.Target = EnemyA;
	TrackingSpec.SpawnLocation = Source->GetActorLocation();
	TrackingSpec.Direction = FVector::ForwardVector;
	TrackingSpec.MovementType = ECombatProjectileMovementType::Tracking;
	TrackingSpec.TargetLostPolicy = ECombatProjectileTargetLostPolicy::Fizzle;
	TrackingSpec.HitPolicy = TrackingData->HitPolicy;
	const FCombatProjectileResult TrackingSpawn = Projectiles->SpawnProjectile(TrackingSpec);
	EnemyA->GetCombatAbilitySystemComponent()->AddLooseGameplayTag(CombatTags::State_Untargetable);
	TestFalse(TEXT("Untargetable target fizzles tracking projectile"),
		Projectiles->AdvanceProjectile(TrackingSpawn.Handle, 0.1f));
	TestEqual(TEXT("Tracking fizzle reports TargetLost"),
		Projectiles->GetLastFinishedResult().FinishReason, ECombatProjectileFinishReason::TargetLost);
	EnemyA->GetCombatAbilitySystemComponent()->RemoveLooseGameplayTag(CombatTags::State_Untargetable);

	TrackingSpec.TargetLostPolicy = ECombatProjectileTargetLostPolicy::UseLastKnownPoint;
	const FCombatProjectileResult LastKnownSpawn = Projectiles->SpawnProjectile(TrackingSpec);
	EnemyA->GetCombatAbilitySystemComponent()->AddLooseGameplayTag(CombatTags::State_OutOfGame);
	TestFalse(TEXT("Last-known tracking flies to the frozen point and then fizzles"),
		Projectiles->AdvanceProjectile(LastKnownSpawn.Handle, 2.1f));
	TestEqual(TEXT("Last-known arrival reports TargetLost without an impact"),
		Projectiles->GetLastFinishedResult().FinishReason, ECombatProjectileFinishReason::TargetLost);
	EnemyA->GetCombatAbilitySystemComponent()->RemoveLooseGameplayTag(CombatTags::State_OutOfGame);

	FCombatProjectileSpec BoundSpec = Spec;
	BoundSpec.AbilityActivationId.Sequence = 999;
	BoundSpec.bCancelWithSourceAbility = true;
	const FCombatProjectileResult BoundSpawn = Projectiles->SpawnProjectile(BoundSpec);
	TestEqual(TEXT("Explicit cancel-with-source removes only its matching activation"),
		Projectiles->CancelProjectilesForAbility(Source, BoundSpec.AbilityActivationId), 1);
	TestFalse(TEXT("Cancelled activation handle leaves the registry"),
		Projectiles->IsProjectileActive(BoundSpawn.Handle));
	return true;
}

/** 验证 Thinker 的 Scheduler pulse，以及 Motion 高优先级抢占和 Hook 清理。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCombatThinkerMotionTest,
	"Combat.ProjectileMotion.ThinkerMotion.SchedulerPreemptionAndHookCleanup",
	CombatProjectileMotionTests::Flags)

bool FCombatThinkerMotionTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FCombatAutomationWorldFixture Fixture;
	if (!Fixture.IsValid()) { AddError(TEXT("Could not create M5 thinker/motion world")); return false; }
	UWorld& World = *Fixture.GetWorld();
	ACombatUnitCharacter* Source = CombatProjectileMotionTests::SpawnUnit(
		World, TEXT("thinker_source"), FVector::ZeroVector, 1);
	ACombatUnitCharacter* Enemy = CombatProjectileMotionTests::SpawnUnit(
		World, TEXT("thinker_enemy"), FVector(300.0, 500.0, 0.0), 2);
	ACombatUnitCharacter* Friendly = CombatProjectileMotionTests::SpawnUnit(
		World, TEXT("thinker_friendly"), FVector(350.0, 500.0, 0.0), 1);
	if (!Source || !Enemy || !Friendly) { return false; }

	FCombatThinkerSpec ThinkerSpec;
	ThinkerSpec.Source = Source;
	ThinkerSpec.Location = FVector(300.0, 500.0, 0.0);
	ThinkerSpec.Radius = 150.0f;
	ThinkerSpec.InitialDelay = 1.0f;
	ThinkerSpec.PulseInterval = 1.0f;
	ThinkerSpec.Duration = 2.0f;
	ThinkerSpec.DamagePerPulse = 10.0f;
	ThinkerSpec.DamageType = ECombatDamageType::Pure;
	ThinkerSpec.TargetingRules.TargetTeamTag = CombatTags::TargetTeam_Enemy;
	UCombatThinkerSubsystem* Thinkers = World.GetSubsystem<UCombatThinkerSubsystem>();
	int32 ThinkerFinishCount = 0;
	Thinkers->OnThinkerFinished().AddLambda(
		[&ThinkerFinishCount](const FCombatThinkerResult& Result) { (void)Result; ++ThinkerFinishCount; });
	const FCombatThinkerResult Created = Thinkers->CreateThinker(ThinkerSpec);
	TestTrue(TEXT("Scheduler thinker is created"), Created.bSuccess);
	World.GetSubsystem<UCombatSchedulerSubsystem>()->RunDueTasks(World.GetTimeSeconds() + 2.1);
	TestEqual(TEXT("Thinker pulses twice before its duration finish"),
		Enemy->GetCombatAbilitySystemComponent()->GetNumericAttribute(UCombatAttributeSet::GetHealthAttribute()), 80.0f);
	TestEqual(TEXT("Thinker excludes friendly units"),
		Friendly->GetCombatAbilitySystemComponent()->GetNumericAttribute(UCombatAttributeSet::GetHealthAttribute()), 100.0f);
	TestEqual(TEXT("Thinker finishes exactly once"), ThinkerFinishCount, 1);
	TestEqual(TEXT("Thinker registry is empty after duration"), Thinkers->GetActiveThinkerCount(), 0);

	UCombatMotionComponent* Motion = Enemy->GetCombatMotionComponent();
	TArray<FCombatMotionResult> MotionResults;
	Motion->OnMotionFinished().AddLambda(
		[&MotionResults](const FCombatMotionResult& Result) { MotionResults.Add(Result); });
	FCombatMotionRequest Low;
	Low.Channel = ECombatMotionChannel::Horizontal;
	Low.Priority = 10;
	Low.TargetLocation = FVector(1000.0, 500.0, 0.0);
	Low.Speed = 1000.0f;
	Low.bProjectToNavigation = false;
	const FCombatMotionResult LowResult = Motion->TryAcquireMotion(Low);
	FCombatMotionRequest High = Low;
	High.Priority = 20;
	High.TargetLocation = FVector(500.0, 500.0, 0.0);
	const FCombatMotionResult HighResult = Motion->TryAcquireMotion(High);
	TestTrue(TEXT("Strictly higher priority motion preempts the channel"),
		LowResult.bSuccess && HighResult.bSuccess && !Motion->IsMotionActive(LowResult.Handle));
	TestTrue(TEXT("Interrupted motion broadcasts exactly once"), MotionResults.Num() == 1
		&& MotionResults[0].Handle == LowResult.Handle
		&& MotionResults[0].FinishReason == ECombatMotionFinishReason::Interrupted);
	Motion->TickComponent(1.0f, LEVELTICK_All, nullptr);
	TestFalse(TEXT("Replacement motion completes and releases its channel"), Motion->HasActiveMotion());
	TestTrue(TEXT("Replacement motion also broadcasts one terminal result"), MotionResults.Num() == 2
		&& MotionResults[1].Handle == HighResult.Handle);

	UCombatModifierData* HookData = NewObject<UCombatModifierData>(Enemy);
	HookData->DefinitionName = TEXT("motion_hook_cleanup");
	HookData->RuntimeClass = UCombatHookDragRuntime::StaticClass();
	HookData->GrantedTags.AddTag(CombatTags::State_Stunned);
	HookData->GrantedTags.AddTag(CombatTags::State_NoUnitCollision);
	FCombatModifierApplyRequest HookRequest;
	HookRequest.Source = Source;
	HookRequest.ModifierData = HookData;
	HookRequest.bHasInitialMotionRequest = true;
	HookRequest.InitialMotionRequest.Channel = ECombatMotionChannel::Horizontal;
	HookRequest.InitialMotionRequest.Priority = 100;
	HookRequest.InitialMotionRequest.TargetLocation = Source->GetActorLocation();
	HookRequest.InitialMotionRequest.Speed = 1000.0f;
	HookRequest.InitialMotionRequest.bProjectToNavigation = false;
	FCombatMotionRequest Blocker = HookRequest.InitialMotionRequest;
	Blocker.TargetLocation = FVector(1000.0, 500.0, 0.0);
	const FCombatMotionResult BlockerResult = Motion->TryAcquireMotion(Blocker);
	const FCombatModifierApplyResult FailedHook = Enemy->GetCombatModifierComponent()->ApplyModifier(HookRequest);
	TestFalse(TEXT("Equal-priority channel owner makes hook application fail cleanly"), FailedHook.bSuccess);
	TestEqual(TEXT("Failed hook acquisition leaves no modifier"),
		Enemy->GetCombatModifierComponent()->GetActiveModifierCount(), 0);
	Motion->ReleaseMotion(BlockerResult.Handle);
	TestTrue(TEXT("Hook modifier acquires motion through its runtime"),
		Enemy->GetCombatModifierComponent()->ApplyModifier(HookRequest).bSuccess && Motion->HasActiveMotion());
	Motion->TickComponent(1.0f, LEVELTICK_All, nullptr);
	TestEqual(TEXT("Hook motion terminal result removes the modifier"),
		Enemy->GetCombatModifierComponent()->GetActiveModifierCount(), 0);
	TestFalse(TEXT("Hook cleanup releases the motion channel"), Motion->HasActiveMotion());
	return true;
}

/** 验证远程普通攻击由 Tracking Projectile exactly-once 终结 AttackRecord。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCombatAttackProjectileTest,
	"Combat.ProjectileMotion.Projectile.AttackRecordFinalize",
	CombatProjectileMotionTests::Flags)

bool FCombatAttackProjectileTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FCombatAutomationWorldFixture Fixture;
	if (!Fixture.IsValid()) { AddError(TEXT("Could not create M5 attack projectile world")); return false; }
	UWorld& World = *Fixture.GetWorld();
	FCombatUnitBaseStats AttackerStats;
	AttackerStats.AttackDamage = 40.0f;
	AttackerStats.BaseAttackTime = 1.0f;
	FCombatUnitBaseStats TargetStats;
	TargetStats.MaxHealth = 200.0f;
	TargetStats.Evasion = 0.0f;
	ACombatUnitCharacter* Attacker = CombatProjectileMotionTests::SpawnUnit(
		World, TEXT("ranged_attacker"), FVector::ZeroVector, 1, &AttackerStats);
	ACombatUnitCharacter* Target = CombatProjectileMotionTests::SpawnUnit(
		World, TEXT("ranged_target"), FVector(150.0, 0.0, 0.0), 2, &TargetStats);
	if (!Attacker || !Target) { return false; }
	UCombatProjectileData* AttackProjectile = CombatProjectileMotionTests::MakeProjectileData(
		*Attacker, TEXT("basic_attack_projectile"), 100.0f, 5.0f, 500.0f);
	AttackProjectile->MovementType = ECombatProjectileMovementType::Tracking;
	const_cast<UCombatUnitData*>(Attacker->GetUnitData())->AttackProjectileData = AttackProjectile;

	UCombatAttackComponent* Attacks = Attacker->GetCombatAttackComponent();
	const FCombatAttackResult Started = Attacks->StartMeleeAttack(Target, FCombatOrderHandle());
	TestTrue(TEXT("Ranged AttackRecord enters windup"), Started.bSuccess);
	World.GetSubsystem<UCombatSchedulerSubsystem>()->RunDueTasks(World.GetTimeSeconds() + 0.4);
	UCombatProjectileSubsystem* Projectiles = World.GetSubsystem<UCombatProjectileSubsystem>();
	TestEqual(TEXT("Attack point keeps the launched record until projectile impact"), Attacks->GetActiveAttackCount(), 1);
	TestEqual(TEXT("Attack point spawns exactly one tracking projectile"), Projectiles->GetActiveProjectileCount(), 1);
	const FCombatProjectileHandle ProjectileHandle = Projectiles->GetLastSpawnedHandle();
	Projectiles->AdvanceProjectile(ProjectileHandle, 2.0f);
	TestEqual(TEXT("Projectile impact applies the frozen attack damage"),
		Target->GetCombatAbilitySystemComponent()->GetNumericAttribute(UCombatAttributeSet::GetHealthAttribute()), 160.0f);
	TestEqual(TEXT("Projectile impact removes the unique AttackRecord"), Attacks->GetActiveAttackCount(), 0);
	TestFalse(TEXT("Finished projectile cannot finalize the attack twice"), Projectiles->AdvanceProjectile(ProjectileHandle, 2.0f));
	return true;
}

/** 验证 Dragon Slave 与 Meat Hook 都只通过公共弹体执行链实现。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCombatProjectileDemoAbilitiesTest,
	"Combat.ProjectileMotion.Demo.DragonSlaveAndMeatHook",
	CombatProjectileMotionTests::Flags)

bool FCombatProjectileDemoAbilitiesTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FCombatAutomationWorldFixture Fixture;
	if (!Fixture.IsValid()) { AddError(TEXT("Could not create M5 demo world")); return false; }
	UWorld& World = *Fixture.GetWorld();
	ACombatUnitCharacter* Caster = CombatProjectileMotionTests::SpawnUnit(
		World, TEXT("demo_caster"), FVector::ZeroVector, 1);
	ACombatUnitCharacter* EnemyA = CombatProjectileMotionTests::SpawnUnit(
		World, TEXT("demo_enemy_a"), FVector(250.0, 0.0, 0.0), 2);
	ACombatUnitCharacter* EnemyB = CombatProjectileMotionTests::SpawnUnit(
		World, TEXT("demo_enemy_b"), FVector(450.0, 0.0, 0.0), 2);
	if (!Caster || !EnemyA || !EnemyB) { return false; }
	UCombatAbilitySystemComponent* Asc = Caster->GetCombatAbilitySystemComponent();
	UCombatProjectileSubsystem* Projectiles = World.GetSubsystem<UCombatProjectileSubsystem>();

	UCombatProjectileData* DragonProjectile = CombatProjectileMotionTests::MakeProjectileData(
		*Caster, TEXT("dragon_slave_projectile"));
	DragonProjectile->HitPolicy.bDestroyOnFirstUnitHit = false;
	UCombatAbilityData* Dragon = NewObject<UCombatAbilityData>(Caster);
	Dragon->DefinitionName = TEXT("dragon_slave");
	Dragon->BehaviorTags.AddTag(CombatTags::Ability_Behavior_PointTarget);
	Dragon->BehaviorTags.AddTag(CombatTags::Ability_Behavior_AoE);
	Dragon->TargetingRules.TargetTeamTag = CombatTags::TargetTeam_Enemy;
	Dragon->TargetingRules.CastRange = 700.0f;
	CombatProjectileMotionTests::AddSpecial(*Dragon, TEXT("damage"), 20.0f);
	CombatProjectileMotionTests::AddSpecial(*Dragon, TEXT("radius"), 20.0f);
	CombatProjectileMotionTests::AddSpecial(*Dragon, TEXT("range"), 600.0f);
	CombatProjectileMotionTests::AddSpecial(*Dragon, TEXT("projectile_speed"), 1000.0f);
	FCombatAbilityAction DragonAction;
	DragonAction.Type = ECombatAbilityActionType::SpawnLinearProjectile;
	DragonAction.Target = ECombatAbilityActionTarget::UnitsInRadius;
	DragonAction.MagnitudeKey = TEXT("damage");
	DragonAction.RadiusKey = TEXT("radius");
	DragonAction.ProjectileRangeKey = TEXT("range");
	DragonAction.ProjectileSpeedKey = TEXT("projectile_speed");
	DragonAction.ProjectileData = DragonProjectile;
	DragonAction.DamageType = ECombatDamageType::Magical;
	Dragon->Actions.Add(DragonAction);
	const FGameplayAbilitySpecHandle DragonHandle =
		CombatProjectileMotionTests::GrantAbility<UCombatDragonSlaveAbility>(*Asc, *Dragon);
	FCombatAbilityTargetData PointTarget;
	PointTarget.TargetLocation = FVector(600.0, 0.0, 0.0);
	PointTarget.bHasTargetLocation = true;
	FGameplayTag FailureTag;
	TestTrue(TEXT("Dragon Slave activates through the common ability entry"),
		Asc->TryActivateCombatAbility(DragonHandle, PointTarget, FailureTag));
	TestTrue(TEXT("Dragon Slave ability ends while its fire-and-forget projectile remains"),
		!Asc->FindAbilitySpecFromHandle(DragonHandle)->IsActive() && Projectiles->GetActiveProjectileCount() == 1);
	Projectiles->AdvanceProjectile(Projectiles->GetLastSpawnedHandle(), 0.7f);
	TestEqual(TEXT("Dragon Slave pierces and damages the first enemy"),
		EnemyA->GetCombatAbilitySystemComponent()->GetNumericAttribute(UCombatAttributeSet::GetHealthAttribute()), 85.0f);
	TestEqual(TEXT("Dragon Slave pierces and damages the second enemy"),
		EnemyB->GetCombatAbilitySystemComponent()->GetNumericAttribute(UCombatAttributeSet::GetHealthAttribute()), 85.0f);

	UCombatModifierData* HookModifier = NewObject<UCombatModifierData>(Caster);
	HookModifier->DefinitionName = TEXT("modifier_hook_drag");
	HookModifier->RuntimeClass = UCombatHookDragRuntime::StaticClass();
	HookModifier->GrantedTags.AddTag(CombatTags::State_Stunned);
	HookModifier->GrantedTags.AddTag(CombatTags::State_NoUnitCollision);
	UCombatProjectileData* HookProjectile = CombatProjectileMotionTests::MakeProjectileData(
		*Caster, TEXT("meat_hook_projectile"));
	HookProjectile->HitPolicy.bDestroyOnFirstUnitHit = true;
	UCombatAbilityData* Hook = NewObject<UCombatAbilityData>(Caster);
	Hook->DefinitionName = TEXT("meat_hook");
	Hook->BehaviorTags.AddTag(CombatTags::Ability_Behavior_PointTarget);
	Hook->TargetingRules.TargetTeamTag = CombatTags::TargetTeam_Enemy;
	Hook->TargetingRules.CastRange = 700.0f;
	CombatProjectileMotionTests::AddSpecial(*Hook, TEXT("damage"), 20.0f);
	CombatProjectileMotionTests::AddSpecial(*Hook, TEXT("width"), 20.0f);
	CombatProjectileMotionTests::AddSpecial(*Hook, TEXT("length"), 600.0f);
	CombatProjectileMotionTests::AddSpecial(*Hook, TEXT("missile_speed"), 1000.0f);
	CombatProjectileMotionTests::AddSpecial(*Hook, TEXT("drag_speed"), 1000.0f);
	FCombatAbilityAction HookAction;
	HookAction.Type = ECombatAbilityActionType::SpawnLinearProjectile;
	HookAction.Target = ECombatAbilityActionTarget::UnitsInRadius;
	HookAction.MagnitudeKey = TEXT("damage");
	HookAction.RadiusKey = TEXT("width");
	HookAction.ProjectileRangeKey = TEXT("length");
	HookAction.ProjectileSpeedKey = TEXT("missile_speed");
	HookAction.ProjectileData = HookProjectile;
	HookAction.DamageType = ECombatDamageType::Magical;
	HookAction.ModifierData = HookModifier;
	HookAction.bMotionToSource = true;
	HookAction.MotionSpeedKey = TEXT("drag_speed");
	HookAction.MotionPriority = 100;
	Hook->Actions.Add(HookAction);
	const FGameplayAbilitySpecHandle HookHandle =
		CombatProjectileMotionTests::GrantAbility<UCombatMeatHookAbility>(*Asc, *Hook);
	TestTrue(TEXT("Meat Hook activates through the common ability entry"),
		Asc->TryActivateCombatAbility(HookHandle, PointTarget, FailureTag));
	Projectiles->AdvanceProjectile(Projectiles->GetLastSpawnedHandle(), 0.7f);
	TestEqual(TEXT("Meat Hook damages only the first enemy"),
		EnemyA->GetCombatAbilitySystemComponent()->GetNumericAttribute(UCombatAttributeSet::GetHealthAttribute()), 70.0f);
	TestEqual(TEXT("Meat Hook stops before the second enemy"),
		EnemyB->GetCombatAbilitySystemComponent()->GetNumericAttribute(UCombatAttributeSet::GetHealthAttribute()), 85.0f);
	TestTrue(TEXT("Meat Hook impact applies drag modifier and acquires motion"),
		EnemyA->GetCombatModifierComponent()->GetActiveModifierCount() == 1
		&& EnemyA->GetCombatMotionComponent()->HasActiveMotion());
	EnemyA->GetCombatMotionComponent()->TickComponent(1.0f, LEVELTICK_All, nullptr);
	TestEqual(TEXT("Meat Hook drag terminal result removes its modifier"),
		EnemyA->GetCombatModifierComponent()->GetActiveModifierCount(), 0);
	TestFalse(TEXT("Meat Hook cleanup releases horizontal motion"),
		EnemyA->GetCombatMotionComponent()->HasActiveMotion());
	return true;
}

#endif
