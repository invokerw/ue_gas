#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "EngineUtils.h"
#include "Misc/AutomationTest.h"

#include "Combat/Ability/CombatAbilitySystemComponent.h"
#include "Combat/Attack/CombatAttackComponent.h"
#include "Combat/Attributes/CombatAttributeSet.h"
#include "Combat/Aura/CombatAuraSubsystem.h"
#include "Combat/Core/CombatTags.h"
#include "Combat/Data/CombatDefinitionData.h"
#include "Combat/Demo/CombatDemoAbilities.h"
#include "Combat/Demo/CombatDemoModifierRuntimes.h"
#include "Combat/Demo/CombatFissureBlocker.h"
#include "Combat/Log/CombatEventSubsystem.h"
#include "Combat/Modifiers/CombatModifierComponent.h"
#include "Combat/Motion/CombatMotionComponent.h"
#include "Combat/Order/CombatOrderComponent.h"
#include "Combat/Projectile/CombatProjectileSubsystem.h"
#include "Combat/Scheduling/CombatSchedulerSubsystem.h"
#include "Combat/Tests/CombatAutomationWorldFixture.h"
#include "Combat/Thinker/CombatThinkerSubsystem.h"
#include "Combat/Unit/CombatUnitCharacter.h"
#include "Combat/Unit/CombatUnitLifecycleComponent.h"
#include "Combat/Validation/CombatSkillTemplateValidator.h"

namespace CombatContentExtensionTests
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;

	/** 生成带稳定定义、队伍、属性和快速 attack point 的 Combat Unit。 */
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
		Data->BaseAttackPoint = 0.05f;
		if (OverrideStats)
		{
			Data->BaseStats = *OverrideStats;
		}
		return Unit->InitializeFromUnitData(Data) ? Unit : nullptr;
	}

	/** 向 AbilityData 写入覆盖全部等级的 special。 */
	void AddSpecial(UCombatAbilityData& Data, const FName Key, std::initializer_list<float> Values)
	{
		FCombatSpecialValue Special;
		for (const float Value : Values) { Special.Values.Add(Value); }
		Data.SpecialValues.Add(Key, MoveTemp(Special));
	}

	/** 把瞬态 Data 绑定到示例 Ability CDO 后通过正式 ASC 授予。 */
	template<typename TAbility>
	FGameplayAbilitySpecHandle GrantAbility(
		UCombatAbilitySystemComponent& Asc,
		UCombatAbilityData& Data,
		const int32 Level = 1,
		const bool bAutoCast = false)
	{
		GetMutableDefault<TAbility>()->AbilityData = &Data;
		FGameplayAbilitySpecHandle Handle;
		FGameplayTag FailureTag;
		Asc.GrantCombatAbility(TAbility::StaticClass(), Level, bAutoCast, Handle, FailureTag);
		return Handle;
	}

	/** 创建满足运行时校验的 Tracking ProjectileData。 */
	UCombatProjectileData* MakeTrackingProjectile(UObject& Outer, const FName DefinitionName)
	{
		UCombatProjectileData* Data = NewObject<UCombatProjectileData>(&Outer);
		Data->DefinitionName = DefinitionName;
		Data->MovementType = ECombatProjectileMovementType::Tracking;
		Data->Speed = 1000.0f;
		Data->Radius = 20.0f;
		Data->MaxDistance = 1000.0f;
		Data->MaxLifetime = 5.0f;
		Data->MaxSimulationStep = 100.0f;
		Data->HitPolicy.bStopOnWorld = false;
		return Data;
	}

	/** 创建 MoveToPoint 请求。 */
	FCombatOrderRequest MakeMoveOrder(const FVector Location)
	{
		FCombatOrderRequest Request;
		Request.Type = ECombatOrderType::MoveToPoint;
		Request.TargetLocation = Location;
		Request.bHasTargetLocation = true;
		return Request;
	}
}

/** 验证 Frost Arrows winner 唯一扣蓝，升级/移除不改变已发射 Record。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCombatFrostArrowsSnapshotTest,
	"Combat.ContentExtension.FrostArrows.OrbProjectileSnapshot",
	CombatContentExtensionTests::Flags)

bool FCombatFrostArrowsSnapshotTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FCombatAutomationWorldFixture Fixture;
	if (!Fixture.IsValid()) { AddError(TEXT("Could not create M6 Frost Arrows world")); return false; }
	UWorld& World = *Fixture.GetWorld();
	FCombatUnitBaseStats AttackerStats;
	AttackerStats.MaxMana = 100.0f;
	AttackerStats.AttackRange = 300.0f;
	ACombatUnitCharacter* Attacker = CombatContentExtensionTests::SpawnUnit(
		World, TEXT("frost_attacker"), FVector::ZeroVector, 1, &AttackerStats);
	ACombatUnitCharacter* Target = CombatContentExtensionTests::SpawnUnit(
		World, TEXT("frost_target"), FVector(120.0, 0.0, 0.0), 2);
	if (!Attacker || !Target) { return false; }

	UCombatModifierData* Slow = NewObject<UCombatModifierData>(Attacker);
	Slow->DefinitionName = TEXT("modifier_frost_slow");
	Slow->RuntimeClass = UCombatModifierRuntime::StaticClass();
	Slow->bIsDebuff = true;
	FCombatModifierAttributeChange SlowChange;
	SlowChange.Attribute = UCombatAttributeSet::GetMoveSpeedAttribute();
	SlowChange.ModifierOp = EGameplayModOp::Multiplicitive;
	SlowChange.Magnitude = 1.0f;
	SlowChange.MagnitudeParameterKey = TEXT("slow_pct");
	Slow->AttributeChanges.Add(SlowChange);

	UCombatModifierData* Intrinsic = NewObject<UCombatModifierData>(Attacker);
	Intrinsic->DefinitionName = TEXT("modifier_frost_arrows");
	Intrinsic->RuntimeClass = UCombatFrostArrowsRuntime::StaticClass();
	Intrinsic->bRemoveOnDeath = false;
	Intrinsic->bDisabledByBreak = true;
	UCombatProjectileData* Projectile = CombatContentExtensionTests::MakeTrackingProjectile(
		*Attacker, TEXT("frost_arrow_projectile"));

	UCombatAbilityData* AbilityData = NewObject<UCombatAbilityData>(Attacker);
	AbilityData->DefinitionName = TEXT("frost_arrows");
	AbilityData->MaxLevel = 2;
	AbilityData->BehaviorTags.AddTag(CombatTags::Ability_Behavior_NoTarget);
	AbilityData->BehaviorTags.AddTag(CombatTags::Ability_Behavior_Passive);
	AbilityData->BehaviorTags.AddTag(CombatTags::Ability_Behavior_Attack);
	AbilityData->BehaviorTags.AddTag(CombatTags::Ability_Behavior_AutoCast);
	AbilityData->TargetingRules.TargetTeamTag = CombatTags::TargetTeam_None;
	AbilityData->IntrinsicModifier = Intrinsic;
	AbilityData->AttackOrbProjectileData = Projectile;
	AbilityData->AttackOrbOnHitModifierData = Slow;
	CombatContentExtensionTests::AddSpecial(*AbilityData, TEXT("mana_cost"), { 10.0f, 20.0f });
	CombatContentExtensionTests::AddSpecial(*AbilityData, TEXT("bonus_damage"), { 5.0f, 15.0f });
	CombatContentExtensionTests::AddSpecial(*AbilityData, TEXT("slow_duration"), { 2.0f, 4.0f });
	CombatContentExtensionTests::AddSpecial(*AbilityData, TEXT("slow_pct"), { 0.2f, 0.4f });

	UCombatAbilitySystemComponent* Asc = Attacker->GetCombatAbilitySystemComponent();
	const FGameplayAbilitySpecHandle AbilityHandle = CombatContentExtensionTests::GrantAbility<UCombatFrostArrowsAbility>(
		*Asc, *AbilityData, 1, true);
	TestTrue(TEXT("Frost Arrows grants with intrinsic/autocast"), AbilityHandle.IsValid());
	const FCombatAttackResult Started = Attacker->GetCombatAttackComponent()->StartMeleeAttack(
		Target, FCombatOrderHandle());
	TestTrue(TEXT("Frost Arrow attack starts"), Started.bSuccess);
	FCombatAttackRecord Snapshot;
	TestTrue(TEXT("AttackRecord snapshot remains queryable"),
		Attacker->GetCombatAttackComponent()->GetAttackRecordSnapshot(Started.Handle, Snapshot));
	TestTrue(TEXT("Level 1 bonus is frozen"), FMath::IsNearlyEqual(Snapshot.BonusDamage, 5.0f));
	TestTrue(TEXT("Projectile override is frozen"), Snapshot.ProjectileDataOverride == Projectile);
	TestEqual(TEXT("Slow action is frozen exactly once"), Snapshot.OnHitActions.Num(), 1);
	if (!Snapshot.OnHitActions.IsEmpty())
	{
		TestTrue(TEXT("Slow duration uses launch level"), FMath::IsNearlyEqual(Snapshot.OnHitActions[0].DurationOverride, 2.0f));
		const float* SlowMultiplier = Snapshot.OnHitActions[0].RuntimeParameterOverrides.Find(TEXT("slow_pct"));
		TestTrue(TEXT("Slow multiplier is immutable"), SlowMultiplier && FMath::IsNearlyEqual(*SlowMultiplier, 0.8f));
	}
	TestTrue(TEXT("Only winner deducts level 1 mana"), FMath::IsNearlyEqual(
		Asc->GetNumericAttribute(UCombatAttributeSet::GetManaAttribute()), 90.0f));

	UCombatSchedulerSubsystem* Scheduler = World.GetSubsystem<UCombatSchedulerSubsystem>();
	Scheduler->RunDueTasks(World.GetTimeSeconds() + 0.1);
	TestTrue(TEXT("Attack point spawns the frozen projectile"),
		World.GetSubsystem<UCombatProjectileSubsystem>()->GetActiveProjectileCount() == 1);
	FGameplayTag FailureTag;
	TestTrue(TEXT("Ability can level after launch"), Asc->SetCombatAbilityLevel(AbilityHandle, 2, FailureTag));
	TestTrue(TEXT("Ability can be removed after launch"), Asc->RemoveCombatAbility(AbilityHandle, FailureTag));
	const FCombatAttackResult Landed = Attacker->GetCombatAttackComponent()->FinalizeAttackFromProjectile(
		Started.Handle, Target);
	TestTrue(TEXT("Launched Record lands after Ability removal"), Landed.bSuccess);
	TestTrue(TEXT("Old level slow remains applied"), FMath::IsNearlyEqual(
		Target->GetCombatAbilitySystemComponent()->GetNumericAttribute(UCombatAttributeSet::GetMoveSpeedAttribute()), 240.0f));
	World.GetSubsystem<UCombatProjectileSubsystem>()->CancelProjectile(
		World.GetSubsystem<UCombatProjectileSubsystem>()->GetLastSpawnedHandle());
	return true;
}

/** 验证 Fissure 线段去重、公共伤害/控制、视觉 Thinker 和 blocker repath。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCombatFissureSliceTest,
	"Combat.ContentExtension.Fissure.LineControlBlockerRepath",
	CombatContentExtensionTests::Flags)

bool FCombatFissureSliceTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FCombatAutomationWorldFixture Fixture;
	if (!Fixture.IsValid()) { AddError(TEXT("Could not create M6 Fissure world")); return false; }
	UWorld& World = *Fixture.GetWorld();
	ACombatUnitCharacter* Caster = CombatContentExtensionTests::SpawnUnit(
		World, TEXT("fissure_caster"), FVector::ZeroVector, 1);
	ACombatUnitCharacter* EnemyA = CombatContentExtensionTests::SpawnUnit(
		World, TEXT("fissure_enemy_a"), FVector(200.0, 0.0, 0.0), 2);
	ACombatUnitCharacter* EnemyB = CombatContentExtensionTests::SpawnUnit(
		World, TEXT("fissure_enemy_b"), FVector(400.0, 20.0, 0.0), 2);
	ACombatUnitCharacter* Friendly = CombatContentExtensionTests::SpawnUnit(
		World, TEXT("fissure_friendly"), FVector(300.0, 0.0, 0.0), 1);
	ACombatUnitCharacter* Outside = CombatContentExtensionTests::SpawnUnit(
		World, TEXT("fissure_outside"), FVector(300.0, 200.0, 0.0), 2);
	ACombatUnitCharacter* Mover = CombatContentExtensionTests::SpawnUnit(
		World, TEXT("fissure_mover"), FVector(300.0, -300.0, 0.0), 1);
	if (!Caster || !EnemyA || !EnemyB || !Friendly || !Outside || !Mover) { return false; }

	UCombatOrderComponent* MoverOrders = Mover->GetCombatOrderComponent();
	MoverOrders->SetNavigationDeferredForTesting(true);
	const FCombatOrderResult MoveResult = MoverOrders->IssueOrder(
		CombatContentExtensionTests::MakeMoveOrder(FVector(300.0, 300.0, 0.0)));
	const uint32 OldAttempt = MoverOrders->GetNavigationAttemptGenerationForTesting();
	TestTrue(TEXT("Mover begins a deferred path crossing future blocker"), MoveResult.bSuccess);

	UCombatModifierData* Stun = NewObject<UCombatModifierData>(Caster);
	Stun->DefinitionName = TEXT("modifier_fissure_stun");
	Stun->RuntimeClass = UCombatModifierRuntime::StaticClass();
	Stun->bIsDebuff = true;
	Stun->GrantedTags.AddTag(CombatTags::State_Stunned);
	UCombatAbilityData* Data = NewObject<UCombatAbilityData>(Caster);
	Data->DefinitionName = TEXT("earthshaker_fissure");
	Data->BehaviorTags.AddTag(CombatTags::Ability_Behavior_PointTarget);
	Data->BehaviorTags.AddTag(CombatTags::Ability_Behavior_AoE);
	Data->TargetingRules.TargetTeamTag = CombatTags::TargetTeam_Enemy;
	Data->TargetingRules.CastRange = 1000.0f;
	CombatContentExtensionTests::AddSpecial(*Data, TEXT("fissure_length"), { 600.0f });
	CombatContentExtensionTests::AddSpecial(*Data, TEXT("fissure_half_width"), { 40.0f });
	CombatContentExtensionTests::AddSpecial(*Data, TEXT("damage"), { 20.0f });
	CombatContentExtensionTests::AddSpecial(*Data, TEXT("stun_duration"), { 1.0f });
	CombatContentExtensionTests::AddSpecial(*Data, TEXT("knockback_distance"), { 80.0f });
	CombatContentExtensionTests::AddSpecial(*Data, TEXT("knockback_speed"), { 400.0f });
	CombatContentExtensionTests::AddSpecial(*Data, TEXT("visual_duration"), { 2.0f });
	CombatContentExtensionTests::AddSpecial(*Data, TEXT("blocker_duration"), { 2.0f });
	CombatContentExtensionTests::AddSpecial(*Data, TEXT("blocker_height"), { 200.0f });
	UCombatFissureAbility* FissureCdo = GetMutableDefault<UCombatFissureAbility>();
	FissureCdo->StunModifierData = Stun;
	const FGameplayAbilitySpecHandle AbilityHandle = CombatContentExtensionTests::GrantAbility<UCombatFissureAbility>(
		*Caster->GetCombatAbilitySystemComponent(), *Data);
	FCombatAbilityTargetData TargetData;
	TargetData.TargetLocation = FVector(600.0, 0.0, 0.0);
	TargetData.bHasTargetLocation = true;
	FGameplayTag FailureTag;
	TestTrue(TEXT("Fissure activates through the public ASC entry"),
		Caster->GetCombatAbilitySystemComponent()->TryActivateCombatAbility(AbilityHandle, TargetData, FailureTag));
	FGameplayAbilitySpec* Spec = Caster->GetCombatAbilitySystemComponent()->FindAbilitySpecFromHandle(AbilityHandle);
	UCombatFissureAbility* Ability = Spec ? Cast<UCombatFissureAbility>(Spec->GetPrimaryInstance()) : nullptr;
	TestNotNull(TEXT("Fissure has an InstancedPerActor runtime"), Ability);
	if (!Ability) { return false; }
	TestEqual(TEXT("Line query deduplicates and filters to two enemies"), Ability->GetLastTargetCount(), 2);
	TestEqual(TEXT("Each surviving hit target receives one Motion"), Ability->GetLastMotionCount(), 2);
	TestTrue(TEXT("Visual-only Thinker is created"), Ability->WasVisualThinkerCreated());
	TestTrue(TEXT("Physical blocker is created"), Ability->WasBlockerCreated());
	TestTrue(TEXT("Enemy A takes common-pipeline damage"),
		EnemyA->GetCombatAbilitySystemComponent()->GetNumericAttribute(UCombatAttributeSet::GetHealthAttribute()) < 100.0f);
	TestTrue(TEXT("Enemy B receives Stun"), EnemyB->IsMovementBlocked());
	TestTrue(TEXT("Friendly remains undamaged"), FMath::IsNearlyEqual(
		Friendly->GetCombatAbilitySystemComponent()->GetNumericAttribute(UCombatAttributeSet::GetHealthAttribute()), 100.0f));
	TestTrue(TEXT("Outside enemy remains undamaged"), FMath::IsNearlyEqual(
		Outside->GetCombatAbilitySystemComponent()->GetNumericAttribute(UCombatAttributeSet::GetHealthAttribute()), 100.0f));
	const uint32 RepathAttempt = MoverOrders->GetNavigationAttemptGenerationForTesting();
	TestTrue(TEXT("Blocker creation starts a new navigation attempt"), RepathAttempt > OldAttempt);
	TestFalse(TEXT("Old blocker-before attempt cannot complete the current Order"),
		MoverOrders->CompleteMovementAttemptForTesting(MoveResult.Handle, OldAttempt, true));

	int32 LiveBlockers = 0;
	for (TActorIterator<ACombatFissureBlocker> It(&World); It; ++It)
	{
		if (*It && !It->IsActorBeingDestroyed()) { ++LiveBlockers; }
	}
	TestEqual(TEXT("Exactly one live Fissure blocker exists"), LiveBlockers, 1);
	World.GetSubsystem<UCombatSchedulerSubsystem>()->RunDueTasks(World.GetTimeSeconds() + 3.0);
	TestEqual(TEXT("Visual Thinker finishes by Scheduler"),
		World.GetSubsystem<UCombatThinkerSubsystem>()->GetActiveThinkerCount(), 0);
	return true;
}

/** 验证 Aura 进入、离开、换队、Break、死亡和取消均无 child 残留。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCombatAuraReconcileTest,
	"Combat.ContentExtension.Aura.OwnerChildReconcile",
	CombatContentExtensionTests::Flags)

bool FCombatAuraReconcileTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FCombatAutomationWorldFixture Fixture;
	if (!Fixture.IsValid()) { AddError(TEXT("Could not create M6 Aura world")); return false; }
	UWorld& World = *Fixture.GetWorld();
	ACombatUnitCharacter* Owner = CombatContentExtensionTests::SpawnUnit(
		World, TEXT("aura_owner"), FVector::ZeroVector, 1);
	ACombatUnitCharacter* Ally = CombatContentExtensionTests::SpawnUnit(
		World, TEXT("aura_ally"), FVector(100.0, 0.0, 0.0), 1);
	ACombatUnitCharacter* Enemy = CombatContentExtensionTests::SpawnUnit(
		World, TEXT("aura_enemy"), FVector(150.0, 0.0, 0.0), 2);
	ACombatUnitCharacter* FarAlly = CombatContentExtensionTests::SpawnUnit(
		World, TEXT("aura_far_ally"), FVector(1000.0, 0.0, 0.0), 1);
	if (!Owner || !Ally || !Enemy || !FarAlly) { return false; }
	UCombatModifierData* Child = NewObject<UCombatModifierData>(Owner);
	Child->DefinitionName = TEXT("modifier_aura_child");
	Child->RuntimeClass = UCombatModifierRuntime::StaticClass();
	Child->bRemoveOnDeath = true;

	FCombatAuraSpec AuraSpec;
	AuraSpec.Owner = Owner;
	AuraSpec.Radius = 300.0f;
	AuraSpec.ReconcileInterval = 0.25f;
	AuraSpec.TargetingRules.TargetTeamTag = CombatTags::TargetTeam_Friendly;
	AuraSpec.ChildModifierData = Child;
	AuraSpec.bDisabledByBreak = true;
	UCombatAuraSubsystem* Auras = World.GetSubsystem<UCombatAuraSubsystem>();
	const FCombatAuraResult Started = Auras->StartAura(AuraSpec);
	TestTrue(TEXT("Aura starts"), Started.bSuccess);
	TestEqual(TEXT("Only nearby non-self ally receives child"), Auras->GetChildCount(Started.Handle), 1);
	TestEqual(TEXT("Enemy is filtered by Team subsystem"), Enemy->GetCombatModifierComponent()->GetActiveModifierCount(), 0);

	Ally->SetActorLocation(FVector(800.0, 0.0, 0.0));
	FarAlly->SetActorLocation(FVector(120.0, 0.0, 0.0));
	TestTrue(TEXT("Manual reconcile succeeds after movement"), Auras->ReconcileNow(Started.Handle));
	TestEqual(TEXT("Leaving child is removed"), Ally->GetCombatModifierComponent()->GetActiveModifierCount(), 0);
	TestEqual(TEXT("Entering child is applied once"), FarAlly->GetCombatModifierComponent()->GetActiveModifierCount(), 1);
	Auras->ReconcileNow(Started.Handle);
	TestEqual(TEXT("Repeated reconcile does not duplicate child"), FarAlly->GetCombatModifierComponent()->GetActiveModifierCount(), 1);

	TestTrue(TEXT("Authority team change succeeds"), FarAlly->SetCombatTeamId(FCombatTeamId(2)));
	TestEqual(TEXT("Team change immediately removes child"), FarAlly->GetCombatModifierComponent()->GetActiveModifierCount(), 0);
	TestTrue(TEXT("Changing back to friendly succeeds"), FarAlly->SetCombatTeamId(FCombatTeamId(1)));
	TestEqual(TEXT("Friendly team change restores child"), FarAlly->GetCombatModifierComponent()->GetActiveModifierCount(), 1);
	Owner->GetCombatAbilitySystemComponent()->AddLooseGameplayTag(CombatTags::State_Broken);
	TestEqual(TEXT("Break removes all Aura children"), Auras->GetChildCount(Started.Handle), 0);
	Owner->GetCombatAbilitySystemComponent()->RemoveLooseGameplayTag(CombatTags::State_Broken);
	TestEqual(TEXT("Break removal restores eligible child"), Auras->GetChildCount(Started.Handle), 1);

	const FCombatEventContext DeathEvent = World.GetSubsystem<UCombatEventSubsystem>()->CreateRootEvent();
	TestTrue(TEXT("Aura owner death succeeds"), Owner->GetCombatLifecycleComponent()->RequestDeath(DeathEvent, Enemy));
	TestFalse(TEXT("Owner death finishes Aura"), Auras->IsAuraActive(Started.Handle));
	TestEqual(TEXT("Owner death leaves no child Modifier"), FarAlly->GetCombatModifierComponent()->GetActiveModifierCount(), 0);
	TestFalse(TEXT("Stale cancel is harmless"), Auras->CancelAura(Started.Handle).bSuccess);
	return true;
}

/** 验证 SpellBlock、Break、Debuff/Dispel Immunity 各自在独立阶段生效。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCombatAdvancedStatusMatrixTest,
	"Combat.ContentExtension.Status.AdvancedInteractionMatrix",
	CombatContentExtensionTests::Flags)

bool FCombatAdvancedStatusMatrixTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FCombatAutomationWorldFixture Fixture;
	if (!Fixture.IsValid()) { AddError(TEXT("Could not create M6 advanced status world")); return false; }
	UWorld& World = *Fixture.GetWorld();
	ACombatUnitCharacter* Caster = CombatContentExtensionTests::SpawnUnit(
		World, TEXT("status_caster"), FVector::ZeroVector, 1);
	ACombatUnitCharacter* Target = CombatContentExtensionTests::SpawnUnit(
		World, TEXT("status_target"), FVector(100.0, 0.0, 0.0), 2);
	if (!Caster || !Target) { return false; }

	UCombatModifierData* SpellBlock = NewObject<UCombatModifierData>(Target);
	SpellBlock->DefinitionName = TEXT("modifier_spell_block");
	SpellBlock->RuntimeClass = UCombatSpellBlockRuntime::StaticClass();
	SpellBlock->DispelRule = ECombatModifierDispelRule::NotDispellable;
	SpellBlock->GrantedTags.AddTag(CombatTags::State_SpellBlock);
	FCombatModifierApplyRequest Apply;
	Apply.Source = Target;
	Apply.ModifierData = SpellBlock;
	TestTrue(TEXT("SpellBlock applies"), Target->GetCombatModifierComponent()->ApplyModifier(Apply).bSuccess);

	UCombatAbilityData* Spell = NewObject<UCombatAbilityData>(Caster);
	Spell->DefinitionName = TEXT("spell_blockable_bolt");
	Spell->BehaviorTags.AddTag(CombatTags::Ability_Behavior_UnitTarget);
	Spell->BehaviorTags.AddTag(CombatTags::Ability_Behavior_SpellBlockable);
	Spell->TargetingRules.TargetTeamTag = CombatTags::TargetTeam_Enemy;
	Spell->TargetingRules.CastRange = 500.0f;
	CombatContentExtensionTests::AddSpecial(*Spell, TEXT("mana_cost"), { 10.0f });
	CombatContentExtensionTests::AddSpecial(*Spell, TEXT("damage"), { 20.0f });
	FCombatAbilityAction& DamageAction = Spell->Actions.AddDefaulted_GetRef();
	DamageAction.Type = ECombatAbilityActionType::Damage;
	DamageAction.Target = ECombatAbilityActionTarget::UnitTarget;
	DamageAction.MagnitudeKey = TEXT("damage");
	DamageAction.DamageType = ECombatDamageType::Physical;
	const FGameplayAbilitySpecHandle SpellHandle = CombatContentExtensionTests::GrantAbility<UCombatUnitDamageAbility>(
		*Caster->GetCombatAbilitySystemComponent(), *Spell);
	FCombatAbilityTargetData TargetData;
	TargetData.TargetActor = Target;
	FGameplayTag FailureTag;
	TestTrue(TEXT("Blockable spell activates"), Caster->GetCombatAbilitySystemComponent()->TryActivateCombatAbility(
		SpellHandle, TargetData, FailureTag));
	TestTrue(TEXT("SpellBlock prevents action damage"), FMath::IsNearlyEqual(
		Target->GetCombatAbilitySystemComponent()->GetNumericAttribute(UCombatAttributeSet::GetHealthAttribute()), 100.0f));
	TestEqual(TEXT("SpellBlock consumes itself exactly once"), Target->GetCombatModifierComponent()->GetActiveModifierCount(), 0);
	TestTrue(TEXT("SpellStarted commit still consumes Mana"), FMath::IsNearlyEqual(
		Caster->GetCombatAbilitySystemComponent()->GetNumericAttribute(UCombatAttributeSet::GetManaAttribute()), 90.0f));

	Apply.ModifierData = SpellBlock;
	Target->GetCombatModifierComponent()->ApplyModifier(Apply);
	Spell->BehaviorTags.RemoveTag(CombatTags::Ability_Behavior_SpellBlockable);
	TestTrue(TEXT("Non-blockable spell activates"), Caster->GetCombatAbilitySystemComponent()->TryActivateCombatAbility(
		SpellHandle, TargetData, FailureTag));
	TestTrue(TEXT("Non-blockable spell bypasses SpellBlock stage"),
		Target->GetCombatAbilitySystemComponent()->GetNumericAttribute(UCombatAttributeSet::GetHealthAttribute()) < 100.0f);
	TestEqual(TEXT("Non-blockable spell does not consume SpellBlock"), Target->GetCombatModifierComponent()->GetActiveModifierCount(), 1);
	Target->GetCombatModifierComponent()->RemoveModifier(
		Target->GetCombatModifierComponent()->ApplyModifier(Apply).Handle);

	UCombatModifierData* Debuff = NewObject<UCombatModifierData>(Target);
	Debuff->DefinitionName = TEXT("modifier_advanced_debuff");
	Debuff->RuntimeClass = UCombatModifierRuntime::StaticClass();
	Debuff->bIsDebuff = true;
	Target->GetCombatAbilitySystemComponent()->AddLooseGameplayTag(CombatTags::State_DebuffImmune);
	Apply.Source = Caster;
	Apply.ModifierData = Debuff;
	const FCombatModifierApplyResult ImmuneApply = Target->GetCombatModifierComponent()->ApplyModifier(Apply);
	TestFalse(TEXT("Debuff Immunity rejects new debuff"), ImmuneApply.bSuccess);
	TestEqual(TEXT("Debuff Immunity returns dedicated failure"),
		ImmuneApply.FailureTag, CombatTags::Failure_Modifier_DebuffImmune.GetTag());
	Target->GetCombatAbilitySystemComponent()->RemoveLooseGameplayTag(CombatTags::State_DebuffImmune);
	const FCombatModifierApplyResult DebuffApply = Target->GetCombatModifierComponent()->ApplyModifier(Apply);
	TestTrue(TEXT("Debuff applies after immunity removal"), DebuffApply.bSuccess);
	Target->GetCombatAbilitySystemComponent()->AddLooseGameplayTag(CombatTags::State_DispelImmune);
	TestEqual(TEXT("Dispel Immunity rejects Strong Dispel"),
		Target->GetCombatModifierComponent()->Dispel(ECombatDispelStrength::Strong), 0);
	TestEqual(TEXT("Dispel Immunity returns dedicated failure"),
		Target->GetCombatModifierComponent()->GetLastDispelFailureTag(),
		CombatTags::Failure_Modifier_DispelImmune.GetTag());
	Target->GetCombatAbilitySystemComponent()->RemoveLooseGameplayTag(CombatTags::State_DispelImmune);
	TestEqual(TEXT("Strong Dispel works after immunity removal"),
		Target->GetCombatModifierComponent()->Dispel(ECombatDispelStrength::Strong), 1);

	UCombatModifierData* Orb = NewObject<UCombatModifierData>(Caster);
	Orb->DefinitionName = TEXT("modifier_breakable_orb");
	Orb->RuntimeClass = UCombatDemoOrbRuntime::StaticClass();
	Orb->bDisabledByBreak = true;
	Orb->RuntimeParameters.Add(TEXT("mana_cost"), 0.0f);
	Orb->RuntimeParameters.Add(TEXT("bonus_damage"), 5.0f);
	Orb->RuntimeParameters.Add(TEXT("on_hit_damage"), 0.0f);
	Apply.Source = Caster;
	Apply.ModifierData = Orb;
	Caster->GetCombatModifierComponent()->ApplyModifier(Apply);
	FCombatAttackCandidateContext Candidate;
	Candidate.Attacker = Caster;
	Candidate.Target = Target;
	TArray<FCombatOrbSnapshot> Orbs;
	Caster->GetCombatModifierComponent()->ClaimAttackOrbs(Candidate, Orbs);
	TestEqual(TEXT("Breakable passive participates before Break"), Orbs.Num(), 1);
	Caster->GetCombatAbilitySystemComponent()->AddLooseGameplayTag(CombatTags::State_Broken);
	Caster->GetCombatModifierComponent()->ClaimAttackOrbs(Candidate, Orbs);
	TestEqual(TEXT("Break pauses flagged Runtime without removing it"), Orbs.Num(), 0);
	Caster->GetCombatAbilitySystemComponent()->RemoveLooseGameplayTag(CombatTags::State_Broken);
	Caster->GetCombatModifierComponent()->ClaimAttackOrbs(Candidate, Orbs);
	TestEqual(TEXT("Break removal restores flagged Runtime"), Orbs.Num(), 1);
	return true;
}

/** 验证可复用技能模板 validator、事件顺序与旁路扫描规则。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCombatSkillTemplateValidatorTest,
	"Combat.ContentExtension.Tool.SkillTemplateValidator",
	CombatContentExtensionTests::Flags)

bool FCombatSkillTemplateValidatorTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	UCombatModifierData* Intrinsic = NewObject<UCombatModifierData>();
	Intrinsic->DefinitionName = TEXT("template_intrinsic");
	Intrinsic->RuntimeClass = UCombatFrostArrowsRuntime::StaticClass();
	UCombatAbilityData* Data = NewObject<UCombatAbilityData>();
	Data->DefinitionName = TEXT("template_frost_arrows");
	Data->BehaviorTags.AddTag(CombatTags::Ability_Behavior_NoTarget);
	Data->BehaviorTags.AddTag(CombatTags::Ability_Behavior_Passive);
	Data->BehaviorTags.AddTag(CombatTags::Ability_Behavior_Attack);
	Data->BehaviorTags.AddTag(CombatTags::Ability_Behavior_AutoCast);
	Data->TargetingRules.TargetTeamTag = CombatTags::TargetTeam_None;
	Data->IntrinsicModifier = Intrinsic;
	CombatContentExtensionTests::AddSpecial(*Data, TEXT("mana_cost"), { 10.0f });
	CombatContentExtensionTests::AddSpecial(*Data, TEXT("bonus_damage"), { 5.0f });
	CombatContentExtensionTests::AddSpecial(*Data, TEXT("slow_duration"), { 2.0f });
	CombatContentExtensionTests::AddSpecial(*Data, TEXT("slow_pct"), { 0.2f });
	GetMutableDefault<UCombatFrostArrowsAbility>()->AbilityData = Data;
	FCombatSkillTemplateRequirements Requirements;
	Requirements.RequiredSpecialKeys = { TEXT("mana_cost"), TEXT("bonus_damage"), TEXT("slow_duration"), TEXT("slow_pct") };
	Requirements.RequiredBehaviorTags.AddTag(CombatTags::Ability_Behavior_Passive);
	Requirements.RequiredBehaviorTags.AddTag(CombatTags::Ability_Behavior_Attack);
	Requirements.RequiredBehaviorTags.AddTag(CombatTags::Ability_Behavior_AutoCast);
	Requirements.bRequireIntrinsicModifier = true;
	TArray<FString> Errors;
	TestTrue(TEXT("Valid Frost template passes"), FCombatSkillTemplateValidator::ValidateAbilityTemplate(
		UCombatFrostArrowsAbility::StaticClass(), Data, Requirements, Errors));
	TestEqual(TEXT("Valid template has no diagnostics"), Errors.Num(), 0);

	const TArray<FGameplayTag> Expected = {
		CombatTags::Event_Combat_AbilityCastStarted,
		CombatTags::Event_Combat_AbilitySpellStarted,
		CombatTags::Event_Combat_AbilityOrderReleased,
		CombatTags::Event_Combat_AbilityEnded
	};
	TArray<FString> EventErrors;
	TestTrue(TEXT("Exact expected event sequence passes"),
		FCombatSkillTemplateValidator::ValidateEventSequence(Expected, Expected, EventErrors));
	TArray<FGameplayTag> WrongOrder = Expected;
	Swap(WrongOrder[1], WrongOrder[2]);
	TestFalse(TEXT("Wrong event order is rejected"),
		FCombatSkillTemplateValidator::ValidateEventSequence(WrongOrder, Expected, EventErrors));

	TArray<const UCombatDefinitionData*> DuplicateDefinitions = { Data, Data };
	TArray<FString> DefinitionErrors;
	TestFalse(TEXT("Duplicate DefinitionId is rejected"),
		FCombatSkillTemplateValidator::ValidateDefinitions(DuplicateDefinitions, DefinitionErrors));
	const TArray<FString>& Patterns = FCombatSkillTemplateValidator::GetForbiddenBypassPatterns();
	TestTrue(TEXT("Bypass scan includes direct Transform writes"), Patterns.Contains(TEXT("SetActorLocation(")));
	TestTrue(TEXT("Bypass scan includes Actor Timer gameplay"), Patterns.Contains(TEXT("GetTimerManager(")));
	TestTrue(TEXT("Bypass scan includes template ProjectileImpact"), Patterns.Contains(TEXT("ProjectileImpact(")));
	return true;
}

#endif
