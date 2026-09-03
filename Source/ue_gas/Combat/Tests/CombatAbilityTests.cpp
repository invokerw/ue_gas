#include "CoreMinimal.h"

#include <limits>

#if WITH_DEV_AUTOMATION_TESTS

#include "Abilities/GameplayAbility.h"
#include "Components/BoxComponent.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"

#include "Combat/Ability/CombatAbilitySystemComponent.h"
#include "Combat/Ability/CombatGameplayAbility.h"
#include "Combat/Attributes/CombatAttributeSet.h"
#include "Combat/Core/CombatTags.h"
#include "Combat/Data/CombatDefinitionData.h"
#include "Combat/Demo/CombatDemoAbilities.h"
#include "Combat/Log/CombatEventSubsystem.h"
#include "Combat/Modifiers/CombatModifierComponent.h"
#include "Combat/Modifiers/CombatModifierRuntime.h"
#include "Combat/Scheduling/CombatSchedulerSubsystem.h"
#include "Combat/Targeting/CombatTargetingSubsystem.h"
#include "Combat/Targeting/CombatTeamSubsystem.h"
#include "Combat/Tests/CombatAutomationWorldFixture.h"
#include "Combat/Unit/CombatUnitCharacter.h"

namespace CombatAbilityTests
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;

	/** 在测试 World 中生成带稳定定义、队伍和基础属性的 Combat Unit。 */
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

	/** 创建恰好包含一种目标模式的最小合法 AbilityData。 */
	UCombatAbilityData* MakeAbilityData(
		UObject& Outer,
		const FName DefinitionName,
		const FGameplayTag& TargetBehavior,
		const FGameplayTag& TargetTeam)
	{
		UCombatAbilityData* Data = NewObject<UCombatAbilityData>(&Outer);
		Data->DefinitionName = DefinitionName;
		Data->BehaviorTags.AddTag(TargetBehavior);
		Data->TargetingRules.TargetTeamTag = TargetTeam;
		return Data;
	}

	/** 添加单级 special，避免测试绕过与正式资产一致的数值查询路径。 */
	void AddSpecial(UCombatAbilityData& Data, const FName Key, const float Value)
	{
		FCombatSpecialValue Special;
		Special.Values.Add(Value);
		Data.SpecialValues.Add(Key, Special);
	}

	/** 把测试 Data 绑定到 Class CDO 后通过服务器公共入口授予 Ability。 */
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

	/** 返回 InstancedPerActor Ability 的权威主实例。 */
	template<typename TAbility>
	TAbility* GetAbilityInstance(UCombatAbilitySystemComponent& Asc, const FGameplayAbilitySpecHandle Handle)
	{
		FGameplayAbilitySpec* Spec = Asc.FindAbilitySpecFromHandle(Handle);
		return Spec ? Cast<TAbility>(Spec->GetPrimaryInstance()) : nullptr;
	}

	/** 从指定日志下标开始，只收集目标 Activation 的 Ability 生命周期事件。 */
	TArray<FGameplayTag> CollectLifecycleEvents(
		const UCombatEventSubsystem& Events,
		const int32 StartIndex,
		const FPrimaryAssetId& AbilityId)
	{
		TArray<FGameplayTag> Result;
		for (int32 Index = StartIndex; Index < Events.GetRecentRecords().Num(); ++Index)
		{
			const FCombatLogRecord& Record = Events.GetRecentRecords()[Index];
			if (Record.Source.AbilityDefinitionId != AbilityId)
			{
				continue;
			}
			const bool bLifecycle = Record.EventType == CombatTags::Event_Combat_AbilityCastStarted
				|| Record.EventType == CombatTags::Event_Combat_AbilitySpellStarted
				|| Record.EventType == CombatTags::Event_Combat_AbilityChannelEnded
				|| Record.EventType == CombatTags::Event_Combat_AbilityOrderReleased
				|| Record.EventType == CombatTags::Event_Combat_AbilityInterrupted
				|| Record.EventType == CombatTags::Event_Combat_AbilityEnded;
			if (bLifecycle)
			{
				Result.Add(Record.EventType);
			}
		}
		return Result;
	}
}

/** 验证服务器目标规则、伪造数据拒绝、LOS 与稳定 AoE 查询。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCombatAbilityTargetingTest,
	"Combat.Ability.Targeting.AuthorityRulesAndAoe",
	CombatAbilityTests::Flags)

/** 执行 TGT-001 的关系、状态、距离、位置、可见性与 LOS 边界断言。 */
bool FCombatAbilityTargetingTest::RunTest(const FString& Parameters)
{
	FCombatAutomationWorldFixture Fixture;
	if (!Fixture.IsValid()) { AddError(TEXT("Could not create M3 targeting world")); return false; }
	UWorld& World = *Fixture.GetWorld();
	ACombatUnitCharacter* Source = CombatAbilityTests::SpawnUnit(World, TEXT("target_source"), FVector::ZeroVector, 1);
	ACombatUnitCharacter* Friendly = CombatAbilityTests::SpawnUnit(World, TEXT("target_friendly"), FVector(200.0, 200.0, 0.0), 1);
	ACombatUnitCharacter* Enemy = CombatAbilityTests::SpawnUnit(World, TEXT("target_enemy"), FVector(300.0, 0.0, 0.0), 2);
	ACombatUnitCharacter* FarEnemy = CombatAbilityTests::SpawnUnit(World, TEXT("target_far_enemy"), FVector(700.0, 0.0, 0.0), 2);
	ACombatUnitCharacter* Neutral = CombatAbilityTests::SpawnUnit(World, TEXT("target_neutral"), FVector(250.0, -200.0, 0.0), 3);
	if (!Source || !Friendly || !Enemy || !FarEnemy || !Neutral)
	{
		AddError(TEXT("Could not spawn M3 targeting units"));
		return false;
	}

	UCombatTargetingSubsystem* Targeting = World.GetSubsystem<UCombatTargetingSubsystem>();
	UCombatTeamSubsystem* Teams = World.GetSubsystem<UCombatTeamSubsystem>();
	TestNotNull(TEXT("Targeting subsystem is available"), Targeting);
	TestTrue(TEXT("Explicit Neutral diplomacy is accepted during initialization"),
		Teams && Teams->AddInitialRelation(FCombatTeamId(1), FCombatTeamId(3), ECombatTeamRelation::Neutral));
	if (!Targeting || !Teams) { return false; }

	FCombatTargetingRules Rules;
	Rules.TargetTeamTag = CombatTags::TargetTeam_Enemy;
	Rules.CastRange = 500.0f;
	TestTrue(TEXT("Hostile unit inside edge range is accepted"), Targeting->ValidateUnitTarget(Source, Enemy, Rules).bValid);
	TestEqual(TEXT("Friendly unit is rejected by Enemy rule"),
		Targeting->ValidateUnitTarget(Source, Friendly, Rules).FailureTag,
		CombatTags::Failure_Target_FriendlyNotAllowed.GetTag());
	TestEqual(TEXT("Self is rejected independently of team relation"),
		Targeting->ValidateUnitTarget(Source, Source, Rules).FailureTag,
		CombatTags::Failure_Target_SelfNotAllowed.GetTag());
	TestEqual(TEXT("Unit outside edge range is rejected"),
		Targeting->ValidateUnitTarget(Source, FarEnemy, Rules).FailureTag,
		CombatTags::Failure_Target_OutOfRange.GetTag());
	TestEqual(TEXT("Explicit Neutral relation requires opt-in"),
		Targeting->ValidateUnitTarget(Source, Neutral, Rules).FailureTag,
		CombatTags::Failure_Target_NeutralNotAllowed.GetTag());
	Rules.bAllowNeutralRelation = true;
	TestTrue(TEXT("Explicit Neutral relation is accepted after opt-in"),
		Targeting->ValidateUnitTarget(Source, Neutral, Rules).bValid);
	Rules.bAllowNeutralRelation = false;

	UCombatAbilitySystemComponent* EnemyAsc = Enemy->GetCombatAbilitySystemComponent();
	EnemyAsc->AddLooseGameplayTag(CombatTags::State_Untargetable);
	TestEqual(TEXT("Untargetable is rejected"), Targeting->ValidateUnitTarget(Source, Enemy, Rules).FailureTag,
		CombatTags::Failure_Target_Untargetable.GetTag());
	EnemyAsc->RemoveLooseGameplayTag(CombatTags::State_Untargetable);
	EnemyAsc->AddLooseGameplayTag(CombatTags::State_Invulnerable);
	TestEqual(TEXT("Invulnerable is rejected by default"), Targeting->ValidateUnitTarget(Source, Enemy, Rules).FailureTag,
		CombatTags::Failure_Target_Invulnerable.GetTag());
	EnemyAsc->RemoveLooseGameplayTag(CombatTags::State_Invulnerable);
	Rules.bAllowMagicImmune = false;
	EnemyAsc->AddLooseGameplayTag(CombatTags::State_MagicImmune);
	TestEqual(TEXT("MagicImmune obeys its explicit target policy"),
		Targeting->ValidateUnitTarget(Source, Enemy, Rules).FailureTag,
		CombatTags::Failure_Target_MagicImmune.GetTag());
	EnemyAsc->RemoveLooseGameplayTag(CombatTags::State_MagicImmune);
	Rules.bAllowMagicImmune = true;
	EnemyAsc->AddLooseGameplayTag(CombatTags::State_OutOfGame);
	TestEqual(TEXT("OutOfGame is always rejected"), Targeting->ValidateUnitTarget(Source, Enemy, Rules).FailureTag,
		CombatTags::Failure_Target_OutOfGame.GetTag());
	EnemyAsc->RemoveLooseGameplayTag(CombatTags::State_OutOfGame);

	TestTrue(TEXT("Finite point inside edge range is accepted"),
		Targeting->ValidatePointTarget(Source, FVector(500.0, 0.0, 0.0), Rules).bValid);
	TestEqual(TEXT("Non-finite point is rejected"),
		Targeting->ValidatePointTarget(Source,
			FVector(std::numeric_limits<double>::quiet_NaN(), 0.0, 0.0), Rules).FailureTag,
		CombatTags::Failure_Target_LocationInvalid.GetTag());
	Rules.VisibilityPolicy = ECombatVisibilityPolicy::RequireVisible;
	TestEqual(TEXT("Unavailable authoritative visibility is explicit Unsupported"),
		Targeting->ValidatePointTarget(Source, FVector(100.0, 0.0, 0.0), Rules).FailureTag,
		CombatTags::Failure_ActionUnsupported.GetTag());
	Rules.VisibilityPolicy = ECombatVisibilityPolicy::None;

	FGameplayTagContainer UnitTargetBehavior;
	UnitTargetBehavior.AddTag(CombatTags::Ability_Behavior_UnitTarget);
	FCombatAbilityTargetData ForgedTargetData;
	ForgedTargetData.TargetActor = Enemy;
	ForgedTargetData.ClientClaimedHitActors.Add(FarEnemy);
	TestEqual(TEXT("Client supplied hit list is never trusted"),
		Targeting->ValidateAbilityTarget(Source, UnitTargetBehavior, Rules, ForgedTargetData).FailureTag,
		CombatTags::Failure_Target_Invalid.GetTag());

	// LOS blocker 使用正式 CombatBlocker profile，验证 World query 而不是测试专用分支。
	AActor* Blocker = World.SpawnActor<AActor>(FVector(150.0, 0.0, 0.0), FRotator::ZeroRotator);
	UBoxComponent* BlockerBox = Blocker ? NewObject<UBoxComponent>(Blocker, TEXT("TargetingLosBlocker")) : nullptr;
	if (Blocker && BlockerBox)
	{
		Blocker->SetRootComponent(BlockerBox);
		BlockerBox->SetBoxExtent(FVector(20.0, 100.0, 100.0));
		BlockerBox->SetCollisionProfileName(TEXT("CombatBlocker"));
		BlockerBox->RegisterComponent();
		Rules.bRequireLineOfSight = true;
		TestEqual(TEXT("CombatBlocker rejects blocked LOS"),
			Targeting->ValidateUnitTarget(Source, Enemy, Rules).FailureTag,
			CombatTags::Failure_Target_LineOfSightBlocked.GetTag());
		Blocker->Destroy();
		Rules.bRequireLineOfSight = false;
	}
	else
	{
		AddError(TEXT("Could not create LOS blocker"));
	}

	const TArray<ACombatUnitCharacter*> FirstQuery = Targeting->QueryUnitsInRadius(
		Source, FVector(300.0, 0.0, 0.0), 250.0f, Rules);
	const TArray<ACombatUnitCharacter*> SecondQuery = Targeting->QueryUnitsInRadius(
		Source, FVector(300.0, 0.0, 0.0), 250.0f, Rules);
	TestEqual(TEXT("Server AoE query keeps only valid hostile units"), FirstQuery.Num(), 1);
	TestTrue(TEXT("Server AoE query contains the expected enemy"), FirstQuery.Num() == 1 && FirstQuery[0] == Enemy);
	TestTrue(TEXT("Repeated AoE query has stable order"), FirstQuery == SecondQuery);
	return true;
}

/** 验证 AbilityData schema 与服务器 grant/level/autocast/intrinsic/remove 契约。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCombatAbilityGrantLifecycleTest,
	"Combat.Ability.Grant.LevelAutocastIntrinsicAndRemoval",
	CombatAbilityTests::Flags)

/** 执行 ABL-001/002/005/006 的管理面与清理断言。 */
bool FCombatAbilityGrantLifecycleTest::RunTest(const FString& Parameters)
{
	FCombatAutomationWorldFixture Fixture;
	if (!Fixture.IsValid()) { AddError(TEXT("Could not create M3 grant world")); return false; }
	UWorld& World = *Fixture.GetWorld();
	ACombatUnitCharacter* Unit = CombatAbilityTests::SpawnUnit(World, TEXT("grant_unit"), FVector::ZeroVector, 1);
	if (!Unit) { AddError(TEXT("Could not spawn M3 grant unit")); return false; }
	UCombatAbilitySystemComponent* Asc = Unit->GetCombatAbilitySystemComponent();

	UCombatModifierData* Intrinsic = NewObject<UCombatModifierData>(Unit);
	Intrinsic->DefinitionName = TEXT("grant_intrinsic");
	Intrinsic->RuntimeClass = UCombatModifierRuntime::StaticClass();
	Intrinsic->bRemoveOnDeath = false;
	UCombatAbilityData* Data = CombatAbilityTests::MakeAbilityData(
		*Unit, TEXT("grant_autocast"), CombatTags::Ability_Behavior_NoTarget, CombatTags::TargetTeam_None);
	Data->MaxLevel = 3;
	Data->CastPoint = 10.0f;
	Data->BehaviorTags.AddTag(CombatTags::Ability_Behavior_AutoCast);
	Data->IntrinsicModifier = Intrinsic;
	CombatAbilityTests::AddSpecial(*Data, TEXT("mana_cost"), 0.0f);
	CombatAbilityTests::AddSpecial(*Data, TEXT("cooldown"), 0.0f);
	GetMutableDefault<UCombatSelfHealAbility>()->AbilityData = Data;

	FGameplayAbilitySpecHandle Handle;
	FGameplayTag FailureTag;
	TestTrue(TEXT("Server grants a valid Combat Ability"), Asc->GrantCombatAbility(
		UCombatSelfHealAbility::StaticClass(), 2, true, Handle, FailureTag));
	TestTrue(TEXT("Granted handle is valid"), Handle.IsValid());
	TestTrue(TEXT("Initial per-Spec AutoCast is enabled"), Asc->IsAutoCastEnabled(Handle));
	TestEqual(TEXT("Intrinsic modifier is applied exactly once"), Unit->GetCombatModifierComponent()->GetActiveModifierCount(), 1);
	Asc->ReconcileIntrinsicModifiers();
	Asc->ReconcileIntrinsicModifiers();
	TestEqual(TEXT("Repeated intrinsic reconcile is idempotent"), Unit->GetCombatModifierComponent()->GetActiveModifierCount(), 1);

	FGameplayAbilitySpecHandle DuplicateHandle;
	TestFalse(TEXT("Duplicate DefinitionId grant is rejected"), Asc->GrantCombatAbility(
		UCombatSelfHealAbility::StaticClass(), 1, false, DuplicateHandle, FailureTag));
	TestEqual(TEXT("Duplicate grant returns stable failure"), FailureTag,
		CombatTags::Failure_Ability_DuplicateDefinition.GetTag());
	TestTrue(TEXT("Ability level changes inside 1..MaxLevel"), Asc->SetCombatAbilityLevel(Handle, 3, FailureTag));
	TestFalse(TEXT("Ability level above MaxLevel is rejected"), Asc->SetCombatAbilityLevel(Handle, 4, FailureTag));
	TestEqual(TEXT("Invalid level returns stable failure"), FailureTag, CombatTags::Failure_Ability_InvalidLevel.GetTag());
	TestEqual(TEXT("Rejected level change leaves Spec.Level unchanged"), Asc->FindAbilitySpecFromHandle(Handle)->Level, 3);
	TestTrue(TEXT("AutoCast can be disabled on an Alive owner"), Asc->SetAutoCastEnabled(Handle, false, FailureTag));
	TestFalse(TEXT("Disabled AutoCast state is retained per Spec"), Asc->IsAutoCastEnabled(Handle));

	FCombatAbilityTargetData EmptyTarget;
	TestTrue(TEXT("Granted ability can enter its cast point"), Asc->TryActivateCombatAbility(Handle, EmptyTarget, FailureTag));
	UCombatGameplayAbility* Instance = CombatAbilityTests::GetAbilityInstance<UCombatGameplayAbility>(*Asc, Handle);
	TestTrue(TEXT("Active cast owns a Scheduler handle"), Instance && Instance->HasActiveCombatSchedule());
	TestTrue(TEXT("Removing a granted ability succeeds"), Asc->RemoveCombatAbility(Handle, FailureTag));
	TestNull(TEXT("Removed AbilitySpec is no longer discoverable"), Asc->FindAbilitySpecFromHandle(Handle));
	TestFalse(TEXT("Removing Ability cancels its Scheduler handle"), Instance && Instance->HasActiveCombatSchedule());
	TestEqual(TEXT("Removing Ability also removes its intrinsic modifier"),
		Unit->GetCombatModifierComponent()->GetActiveModifierCount(), 0);

	UCombatAbilityData* ProjectileAbility = CombatAbilityTests::MakeAbilityData(
		*Unit, TEXT("m5_projectile_action"), CombatTags::Ability_Behavior_NoTarget, CombatTags::TargetTeam_None);
	UCombatProjectileData* ProjectileData = NewObject<UCombatProjectileData>(Unit);
	ProjectileData->DefinitionName = TEXT("m5_projectile_schema");
	ProjectileData->Speed = 1000.0f;
	ProjectileData->Radius = 25.0f;
	ProjectileData->MaxDistance = 1000.0f;
	CombatAbilityTests::AddSpecial(*ProjectileAbility, TEXT("damage"), 10.0f);
	FCombatAbilityAction ProjectileAction;
	ProjectileAction.Type = ECombatAbilityActionType::SpawnLinearProjectile;
	ProjectileAction.Target = ECombatAbilityActionTarget::Caster;
	ProjectileAction.MagnitudeKey = TEXT("damage");
	ProjectileAction.ProjectileData = ProjectileData;
	ProjectileAbility->Actions.Add(ProjectileAction);
	FString Diagnostic;
	TestTrue(TEXT("M5 enables the frozen projectile action schema"), ProjectileAbility->ValidateRuntime(Diagnostic));
	ProjectileAction.ProjectileData = nullptr;
	ProjectileAbility->Actions[0] = ProjectileAction;
	TestFalse(TEXT("Projectile action without ProjectileData is rejected"), ProjectileAbility->ValidateRuntime(Diagnostic));
	TestTrue(TEXT("Invalid projectile action supplies a diagnostic"), !Diagnostic.IsEmpty());
	return true;
}

/** 验证无目标治疗的前摇、提交快照、共享 Class 隔离和生命周期日志。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCombatAbilitySelfHealTest,
	"Combat.Ability.VerticalSlice.SelfHealCommitAndLogs",
	CombatAbilityTests::Flags)

/** 执行 DEMO-301 与 ABL-003 的成功、并发隔离、CDR 和重入断言。 */
bool FCombatAbilitySelfHealTest::RunTest(const FString& Parameters)
{
	FCombatAutomationWorldFixture Fixture;
	if (!Fixture.IsValid()) { AddError(TEXT("Could not create M3 self-heal world")); return false; }
	UWorld& World = *Fixture.GetWorld();
	FCombatUnitBaseStats Stats;
	Stats.MaxHealth = 200.0f;
	Stats.MaxMana = 100.0f;
	Stats.CooldownReductionPct = 0.25f;
	ACombatUnitCharacter* First = CombatAbilityTests::SpawnUnit(World, TEXT("heal_first"), FVector::ZeroVector, 1, &Stats);
	ACombatUnitCharacter* Second = CombatAbilityTests::SpawnUnit(World, TEXT("heal_second"), FVector(400.0, 0.0, 0.0), 1, &Stats);
	ACombatUnitCharacter* LowMana = CombatAbilityTests::SpawnUnit(World, TEXT("heal_low_mana"), FVector(800.0, 0.0, 0.0), 1, &Stats);
	if (!First || !Second || !LowMana) { AddError(TEXT("Could not spawn self-heal units")); return false; }
	First->GetCombatAbilitySystemComponent()->SetNumericAttributeBase(UCombatAttributeSet::GetHealthAttribute(), 100.0f);
	Second->GetCombatAbilitySystemComponent()->SetNumericAttributeBase(UCombatAttributeSet::GetHealthAttribute(), 120.0f);
	LowMana->GetCombatAbilitySystemComponent()->SetNumericAttributeBase(UCombatAttributeSet::GetManaAttribute(), 5.0f);

	UCombatAbilityData* Data = CombatAbilityTests::MakeAbilityData(
		*First, TEXT("self_heal_commit"), CombatTags::Ability_Behavior_NoTarget, CombatTags::TargetTeam_None);
	Data->CastPoint = 1.0f;
	CombatAbilityTests::AddSpecial(*Data, TEXT("heal"), 40.0f);
	CombatAbilityTests::AddSpecial(*Data, TEXT("mana_cost"), 10.0f);
	CombatAbilityTests::AddSpecial(*Data, TEXT("cooldown"), 4.0f);
	FCombatAbilityAction HealAction;
	HealAction.Type = ECombatAbilityActionType::Heal;
	HealAction.Target = ECombatAbilityActionTarget::Caster;
	HealAction.MagnitudeKey = TEXT("heal");
	Data->Actions.Add(HealAction);

	UCombatAbilitySystemComponent* FirstAsc = First->GetCombatAbilitySystemComponent();
	UCombatAbilitySystemComponent* SecondAsc = Second->GetCombatAbilitySystemComponent();
	const FGameplayAbilitySpecHandle FirstHandle = CombatAbilityTests::GrantAbility<UCombatSelfHealAbility>(*FirstAsc, *Data);
	const FGameplayAbilitySpecHandle SecondHandle = CombatAbilityTests::GrantAbility<UCombatSelfHealAbility>(*SecondAsc, *Data);
	UCombatAbilitySystemComponent* LowManaAsc = LowMana->GetCombatAbilitySystemComponent();
	const FGameplayAbilitySpecHandle LowManaHandle = CombatAbilityTests::GrantAbility<UCombatSelfHealAbility>(*LowManaAsc, *Data);
	TestTrue(TEXT("Two units can receive the same Ability class independently"), FirstHandle.IsValid() && SecondHandle.IsValid());
	UCombatEventSubsystem* Events = World.GetSubsystem<UCombatEventSubsystem>();
	const int32 LogStart = Events->GetRecentRecords().Num();
	FGameplayTag FailureTag;
	FCombatAbilityTargetData EmptyTarget;
	TestFalse(TEXT("Insufficient Mana rejects activation before either same-stage commit"),
		LowManaAsc->TryActivateCombatAbility(LowManaHandle, EmptyTarget, FailureTag));
	TestEqual(TEXT("Insufficient Mana returns stable Cost failure"), FailureTag, CombatTags::Failure_Ability_Cost.GetTag());
	TestEqual(TEXT("Atomic preflight leaves low Mana unchanged"),
		LowManaAsc->GetNumericAttribute(UCombatAttributeSet::GetManaAttribute()), 5.0f);
	TestEqual(TEXT("Atomic preflight does not start cooldown"),
		LowManaAsc->GetCombatAbilityCooldownRemaining(LowManaHandle), 0.0f);
	TestTrue(TEXT("First self-heal starts cast point"), FirstAsc->TryActivateCombatAbility(FirstHandle, EmptyTarget, FailureTag));
	TestTrue(TEXT("Second self-heal starts an independent cast point"), SecondAsc->TryActivateCombatAbility(SecondHandle, EmptyTarget, FailureTag));
	TestEqual(TEXT("Default SpellStarted commit does not spend Mana during cast point"),
		FirstAsc->GetNumericAttribute(UCombatAttributeSet::GetManaAttribute()), 100.0f);
	TestEqual(TEXT("Heal does not execute before cast point"),
		FirstAsc->GetNumericAttribute(UCombatAttributeSet::GetHealthAttribute()), 100.0f);
	TestFalse(TEXT("InstancedPerActor ability rejects re-entry"),
		FirstAsc->TryActivateCombatAbility(FirstHandle, EmptyTarget, FailureTag));
	TestEqual(TEXT("Re-entry returns stable failure"), FailureTag, CombatTags::Failure_Ability_AlreadyActive.GetTag());

	World.GetSubsystem<UCombatSchedulerSubsystem>()->RunDueTasks(World.GetTimeSeconds() + 1.1);
	TestEqual(TEXT("First unit receives its own heal"),
		FirstAsc->GetNumericAttribute(UCombatAttributeSet::GetHealthAttribute()), 140.0f);
	TestEqual(TEXT("Second unit receives its own heal"),
		SecondAsc->GetNumericAttribute(UCombatAttributeSet::GetHealthAttribute()), 160.0f);
	TestEqual(TEXT("Mana cost commits once at SpellStarted"),
		FirstAsc->GetNumericAttribute(UCombatAttributeSet::GetManaAttribute()), 90.0f);
	TestTrue(TEXT("Cooldown snapshots the current 25 percent CDR"),
		FMath::IsNearlyEqual(FirstAsc->GetCombatAbilityCooldownRemaining(FirstHandle), 3.0f, 0.05f));
	TestFalse(TEXT("Cooldown blocks immediate reactivation"),
		FirstAsc->TryActivateCombatAbility(FirstHandle, EmptyTarget, FailureTag));
	TestEqual(TEXT("Cooldown rejection returns stable failure"), FailureTag, CombatTags::Failure_Ability_Cooldown.GetTag());

	const TArray<FGameplayTag> Lifecycle = CombatAbilityTests::CollectLifecycleEvents(*Events, LogStart, Data->GetPrimaryAssetId());
	const TArray<FGameplayTag> Expected = {
		CombatTags::Event_Combat_AbilityCastStarted,
		CombatTags::Event_Combat_AbilityCastStarted,
		CombatTags::Event_Combat_AbilitySpellStarted,
		CombatTags::Event_Combat_AbilityOrderReleased,
		CombatTags::Event_Combat_AbilityEnded,
		CombatTags::Event_Combat_AbilitySpellStarted,
		CombatTags::Event_Combat_AbilityOrderReleased,
		CombatTags::Event_Combat_AbilityEnded
	};
	TestTrue(TEXT("Shared class activations retain deterministic per-instance lifecycle"), Lifecycle == Expected);
	return true;
}

/** 验证单位目标前摇复核，以及点目标服务器 AoE 数据驱动伤害。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCombatAbilityDamageAoeTest,
	"Combat.Ability.VerticalSlice.UnitDamageAndPointAoe",
	CombatAbilityTests::Flags)

/** 执行 DEMO-302/303 的目标丢失中断、魔法结算与权威 AoE 命中断言。 */
bool FCombatAbilityDamageAoeTest::RunTest(const FString& Parameters)
{
	FCombatAutomationWorldFixture Fixture;
	if (!Fixture.IsValid()) { AddError(TEXT("Could not create M3 damage/AoE world")); return false; }
	UWorld& World = *Fixture.GetWorld();
	ACombatUnitCharacter* Source = CombatAbilityTests::SpawnUnit(World, TEXT("damage_ability_source"), FVector::ZeroVector, 1);
	ACombatUnitCharacter* EnemyA = CombatAbilityTests::SpawnUnit(World, TEXT("damage_ability_enemy_a"), FVector(300.0, 0.0, 0.0), 2);
	ACombatUnitCharacter* EnemyB = CombatAbilityTests::SpawnUnit(World, TEXT("damage_ability_enemy_b"), FVector(520.0, 0.0, 0.0), 2);
	ACombatUnitCharacter* FarEnemy = CombatAbilityTests::SpawnUnit(World, TEXT("damage_ability_far"), FVector(850.0, 0.0, 0.0), 2);
	ACombatUnitCharacter* Friendly = CombatAbilityTests::SpawnUnit(World, TEXT("damage_ability_friendly"), FVector(420.0, 100.0, 0.0), 1);
	if (!Source || !EnemyA || !EnemyB || !FarEnemy || !Friendly)
	{
		AddError(TEXT("Could not spawn damage/AoE units"));
		return false;
	}
	UCombatAbilitySystemComponent* Asc = Source->GetCombatAbilitySystemComponent();
	FGameplayTag FailureTag;

	UCombatAbilityData* UnitDamage = CombatAbilityTests::MakeAbilityData(
		*Source, TEXT("unit_magic_damage"), CombatTags::Ability_Behavior_UnitTarget, CombatTags::TargetTeam_Enemy);
	UnitDamage->CastPoint = 1.0f;
	UnitDamage->TargetingRules.CastRange = 500.0f;
	CombatAbilityTests::AddSpecial(*UnitDamage, TEXT("damage"), 20.0f);
	CombatAbilityTests::AddSpecial(*UnitDamage, TEXT("mana_cost"), 10.0f);
	CombatAbilityTests::AddSpecial(*UnitDamage, TEXT("cooldown"), 0.0f);
	FCombatAbilityAction DamageAction;
	DamageAction.Type = ECombatAbilityActionType::Damage;
	DamageAction.Target = ECombatAbilityActionTarget::UnitTarget;
	DamageAction.MagnitudeKey = TEXT("damage");
	DamageAction.DamageType = ECombatDamageType::Magical;
	UnitDamage->Actions.Add(DamageAction);
	const FGameplayAbilitySpecHandle UnitDamageHandle =
		CombatAbilityTests::GrantAbility<UCombatUnitDamageAbility>(*Asc, *UnitDamage);
	TestTrue(TEXT("Unit damage ability is granted"), UnitDamageHandle.IsValid());

	FCombatAbilityTargetData UnitTarget;
	UnitTarget.TargetActor = EnemyA;
	TestTrue(TEXT("Unit damage enters cast point with a valid enemy"),
		Asc->TryActivateCombatAbility(UnitDamageHandle, UnitTarget, FailureTag));
	EnemyA->SetActorLocation(FVector(1200.0, 0.0, 0.0));
	World.GetSubsystem<UCombatSchedulerSubsystem>()->RunDueTasks(World.GetTimeSeconds() + 1.1);
	TestEqual(TEXT("Target loss before SpellStarted does not deal damage"),
		EnemyA->GetCombatAbilitySystemComponent()->GetNumericAttribute(UCombatAttributeSet::GetHealthAttribute()), 100.0f);
	TestEqual(TEXT("Default SpellStarted commit does not spend Mana on interrupted cast"),
		Asc->GetNumericAttribute(UCombatAttributeSet::GetManaAttribute()), 100.0f);
	TestEqual(TEXT("Interrupted cast does not start cooldown"), Asc->GetCombatAbilityCooldownRemaining(UnitDamageHandle), 0.0f);

	EnemyA->SetActorLocation(FVector(300.0, 0.0, 0.0));
	EnemyA->GetCombatAbilitySystemComponent()->AddLooseGameplayTag(CombatTags::State_MagicImmune);
	TestTrue(TEXT("Magic-immune enemy remains targetable when policy permits"),
		Asc->TryActivateCombatAbility(UnitDamageHandle, UnitTarget, FailureTag));
	World.GetSubsystem<UCombatSchedulerSubsystem>()->RunDueTasks(World.GetTimeSeconds() + 1.1);
	TestEqual(TEXT("Common damage pipeline blocks magical damage on MagicImmune"),
		EnemyA->GetCombatAbilitySystemComponent()->GetNumericAttribute(UCombatAttributeSet::GetHealthAttribute()), 100.0f);
	EnemyA->GetCombatAbilitySystemComponent()->RemoveLooseGameplayTag(CombatTags::State_MagicImmune);
	TestTrue(TEXT("Unit damage can be cast again after target-loss interruption"),
		Asc->TryActivateCombatAbility(UnitDamageHandle, UnitTarget, FailureTag));
	World.GetSubsystem<UCombatSchedulerSubsystem>()->RunDueTasks(World.GetTimeSeconds() + 1.1);
	TestEqual(TEXT("Magical Damage action uses the common resistance pipeline"),
		EnemyA->GetCombatAbilitySystemComponent()->GetNumericAttribute(UCombatAttributeSet::GetHealthAttribute()), 85.0f);
	TestEqual(TEXT("Each SpellStarted commits Mana once, including blocked Damage"),
		Asc->GetNumericAttribute(UCombatAttributeSet::GetManaAttribute()), 80.0f);

	UCombatAbilityData* PointAoe = CombatAbilityTests::MakeAbilityData(
		*Source, TEXT("point_aoe_damage"), CombatTags::Ability_Behavior_PointTarget, CombatTags::TargetTeam_Enemy);
	PointAoe->BehaviorTags.AddTag(CombatTags::Ability_Behavior_AoE);
	PointAoe->TargetingRules.CastRange = 600.0f;
	CombatAbilityTests::AddSpecial(*PointAoe, TEXT("damage"), 20.0f);
	CombatAbilityTests::AddSpecial(*PointAoe, TEXT("radius"), 200.0f);
	FCombatAbilityAction AoeAction;
	AoeAction.Type = ECombatAbilityActionType::Damage;
	AoeAction.Target = ECombatAbilityActionTarget::UnitsInRadius;
	AoeAction.MagnitudeKey = TEXT("damage");
	AoeAction.RadiusKey = TEXT("radius");
	AoeAction.DamageType = ECombatDamageType::Pure;
	PointAoe->Actions.Add(AoeAction);
	const FGameplayAbilitySpecHandle PointAoeHandle =
		CombatAbilityTests::GrantAbility<UCombatPointAoeAbility>(*Asc, *PointAoe);
	TestTrue(TEXT("Point AoE ability is granted"), PointAoeHandle.IsValid());

	FCombatAbilityTargetData PointTarget;
	PointTarget.TargetLocation = FVector(420.0, 0.0, 0.0);
	PointTarget.bHasTargetLocation = true;
	PointTarget.ClientClaimedHitActors.Add(FarEnemy);
	TestFalse(TEXT("Point AoE rejects a client-claimed hit list"),
		Asc->TryActivateCombatAbility(PointAoeHandle, PointTarget, FailureTag));
	PointTarget.ClientClaimedHitActors.Reset();
	TestTrue(TEXT("Point AoE executes from only the server point"),
		Asc->TryActivateCombatAbility(PointAoeHandle, PointTarget, FailureTag));
	TestEqual(TEXT("First enemy in radius receives pure damage after prior spell"),
		EnemyA->GetCombatAbilitySystemComponent()->GetNumericAttribute(UCombatAttributeSet::GetHealthAttribute()), 65.0f);
	TestEqual(TEXT("Second enemy in radius receives AoE damage"),
		EnemyB->GetCombatAbilitySystemComponent()->GetNumericAttribute(UCombatAttributeSet::GetHealthAttribute()), 80.0f);
	TestEqual(TEXT("Far enemy is excluded from AoE"),
		FarEnemy->GetCombatAbilitySystemComponent()->GetNumericAttribute(UCombatAttributeSet::GetHealthAttribute()), 100.0f);
	TestEqual(TEXT("Friendly unit is excluded by the shared targeting rule"),
		Friendly->GetCombatAbilitySystemComponent()->GetNumericAttribute(UCombatAttributeSet::GetHealthAttribute()), 100.0f);
	UCombatGameplayAbility* AoeInstance = CombatAbilityTests::GetAbilityInstance<UCombatGameplayAbility>(*Asc, PointAoeHandle);
	TestEqual(TEXT("AoE aggregate reports exactly two server-authoritative targets"),
		AoeInstance ? AoeInstance->GetLastActionResult().AffectedTargetCount : 0, 2);
	return true;
}

/** 验证 Scheduler Channel 补帧、边界、固定中断顺序与退出清理。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCombatAbilityChannelTest,
	"Combat.Ability.Lifecycle.ChannelSchedulerAndCleanup",
	CombatAbilityTests::Flags)

/** 执行 ABL-004 的引导 tick、正常结束、中断 exactly-once 和 Handle 清理断言。 */
bool FCombatAbilityChannelTest::RunTest(const FString& Parameters)
{
	FCombatAutomationWorldFixture Fixture;
	if (!Fixture.IsValid()) { AddError(TEXT("Could not create M3 channel world")); return false; }
	UWorld& World = *Fixture.GetWorld();
	ACombatUnitCharacter* Unit = CombatAbilityTests::SpawnUnit(World, TEXT("channel_unit"), FVector::ZeroVector, 1);
	if (!Unit) { AddError(TEXT("Could not spawn channel unit")); return false; }
	UCombatAbilitySystemComponent* Asc = Unit->GetCombatAbilitySystemComponent();
	UCombatAbilityData* Data = CombatAbilityTests::MakeAbilityData(
		*Unit, TEXT("channel_probe"), CombatTags::Ability_Behavior_NoTarget, CombatTags::TargetTeam_None);
	Data->BehaviorTags.AddTag(CombatTags::Ability_Behavior_Channelled);
	Data->ChannelDuration = 3.0f;
	Data->ChannelInterval = 1.0f;
	const FGameplayAbilitySpecHandle Handle =
		CombatAbilityTests::GrantAbility<UCombatChannelProbeAbility>(*Asc, *Data);
	TestTrue(TEXT("Channel ability is granted"), Handle.IsValid());
	UCombatChannelProbeAbility* Instance =
		CombatAbilityTests::GetAbilityInstance<UCombatChannelProbeAbility>(*Asc, Handle);
	TestNotNull(TEXT("Instanced channel probe is available"), Instance);
	if (!Instance) { return false; }

	UCombatEventSubsystem* Events = World.GetSubsystem<UCombatEventSubsystem>();
	const int32 InterruptedLogStart = Events->GetRecentRecords().Num();
	FGameplayTag FailureTag;
	FCombatAbilityTargetData EmptyTarget;
	TestTrue(TEXT("Channel activates and owns schedules"), Asc->TryActivateCombatAbility(Handle, EmptyTarget, FailureTag));
	TestTrue(TEXT("Channel repeating and finish handles are active"), Instance->HasActiveCombatSchedule());
	World.GetSubsystem<UCombatSchedulerSubsystem>()->RunDueTasks(World.GetTimeSeconds() + 2.1);
	TestEqual(TEXT("ExecuteAllBounded emits two logical ticks after a 2.1 second jump"), Instance->GetObservedTickCount(), 2);
	TestTrue(TEXT("Channel remains active before duration"), Asc->FindAbilitySpecFromHandle(Handle)->IsActive());
	// 状态响应必须通过公共 ASC 规则中断 Channel，而不是由测试直接调用 Cancel。
	Asc->AddLooseGameplayTag(CombatTags::State_Stunned);
	TestEqual(TEXT("Interrupted channel finishes exactly once"), Instance->GetObservedFinishCount(), 1);
	TestTrue(TEXT("Interrupted finish receives bInterrupted=true"), Instance->WasLastFinishInterrupted());
	TestFalse(TEXT("Interrupted channel cancels every Scheduler handle"), Instance->HasActiveCombatSchedule());
	World.GetSubsystem<UCombatSchedulerSubsystem>()->RunDueTasks(World.GetTimeSeconds() + 10.0);
	TestEqual(TEXT("Cancelled channel cannot emit stale ticks"), Instance->GetObservedTickCount(), 2);

	const TArray<FGameplayTag> InterruptedLifecycle = CombatAbilityTests::CollectLifecycleEvents(
		*Events, InterruptedLogStart, Data->GetPrimaryAssetId());
	const TArray<FGameplayTag> ExpectedInterrupted = {
		CombatTags::Event_Combat_AbilityCastStarted,
		CombatTags::Event_Combat_AbilitySpellStarted,
		CombatTags::Event_Combat_AbilityInterrupted,
		CombatTags::Event_Combat_AbilityChannelEnded,
		CombatTags::Event_Combat_AbilityOrderReleased,
		CombatTags::Event_Combat_AbilityEnded
	};
	TestTrue(TEXT("Interrupted channel emits the frozen lifecycle order exactly once"),
		InterruptedLifecycle == ExpectedInterrupted);

	Asc->RemoveLooseGameplayTag(CombatTags::State_Stunned);
	TestTrue(TEXT("Channel can reactivate after complete cleanup"),
		Asc->TryActivateCombatAbility(Handle, EmptyTarget, FailureTag));
	World.GetSubsystem<UCombatSchedulerSubsystem>()->RunDueTasks(World.GetTimeSeconds() + 3.1);
	TestEqual(TEXT("Boundary finish cancels the tick exactly at duration"), Instance->GetObservedTickCount(), 4);
	TestEqual(TEXT("Normal channel finish is also exactly once per activation"), Instance->GetObservedFinishCount(), 2);
	TestFalse(TEXT("Normal finish reports bInterrupted=false"), Instance->WasLastFinishInterrupted());
	TestFalse(TEXT("Normally completed channel leaves no Scheduler handle"), Instance->HasActiveCombatSchedule());
	return true;
}

#endif
