#include "CoreMinimal.h"

#include <limits>

#if WITH_DEV_AUTOMATION_TESTS

#include "Abilities/GameplayAbility.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"

#include "Combat/Ability/CombatAbilitySystemComponent.h"
#include "Combat/Attributes/CombatAttributeSet.h"
#include "Combat/Combat/CombatDamageCalculator.h"
#include "Combat/Combat/CombatDamageSubsystem.h"
#include "Combat/Combat/CombatHealSubsystem.h"
#include "Combat/Combat/CombatTransactionSubsystem.h"
#include "Combat/Core/CombatTags.h"
#include "Combat/Data/CombatDefinitionData.h"
#include "Combat/Demo/CombatDemoAbilities.h"
#include "Combat/Demo/CombatDemoModifierRuntimes.h"
#include "Combat/Log/CombatEventSubsystem.h"
#include "Combat/Modifiers/CombatModifierComponent.h"
#include "Combat/Modifiers/CombatModifierRuntime.h"
#include "Combat/Scheduling/CombatSchedulerSubsystem.h"
#include "Combat/Tests/CombatAutomationWorldFixture.h"
#include "Combat/Unit/CombatUnitCharacter.h"
#include "Combat/Unit/CombatUnitLifecycleComponent.h"

namespace CombatCoreTests
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;

	/** 在测试 World 中生成已初始化且互不重叠的 Combat Unit。 */
	ACombatUnitCharacter* SpawnInitializedUnit(
		UWorld& World,
		const FName DefinitionName,
		const FVector Location,
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
		if (OverrideStats)
		{
			Data->BaseStats = *OverrideStats;
		}
		return Unit->InitializeFromUnitData(Data) ? Unit : nullptr;
	}

	/** 将 Unit 当前 Health 作为测试前置条件写入 GAS base attribute。 */
	void SetHealth(ACombatUnitCharacter& Unit, const float Health)
	{
		Unit.GetCombatAbilitySystemComponent()->SetNumericAttributeBase(UCombatAttributeSet::GetHealthAttribute(), Health);
	}

	/** 使用指定定义名创建最小可施加 ModifierData。 */
	UCombatModifierData* MakeModifierData(UObject& Outer, const FName DefinitionName)
	{
		UCombatModifierData* Data = NewObject<UCombatModifierData>(&Outer);
		Data->DefinitionName = DefinitionName;
		Data->RuntimeClass = UCombatModifierRuntime::StaticClass();
		return Data;
	}
}

/** 验证 UnitData 幂等初始化、Attribute clamp、恢复节拍和生命状态机。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCombatAttributeLifecycleTest,
	"Combat.Core.Attributes.InitializationRegenAndLifecycle",
	CombatCoreTests::Flags)

/** 执行 ATR-001/002 与 LIFE-001 的核心边界断言。 */
bool FCombatAttributeLifecycleTest::RunTest(const FString& Parameters)
{
	FCombatAutomationWorldFixture Fixture;
	if (!Fixture.IsValid()) { AddError(TEXT("Could not create M2 automation world")); return false; }
	UWorld& World = *Fixture.GetWorld();

	FCombatUnitBaseStats Stats;
	Stats.MaxHealth = 200.0f;
	Stats.MaxMana = 120.0f;
	Stats.HealthRegen = 4.0f;
	Stats.ManaRegen = 8.0f;
	ACombatUnitCharacter* Unit = CombatCoreTests::SpawnInitializedUnit(
		World, TEXT("attribute_lifecycle_unit"), FVector::ZeroVector, &Stats);
	TestNotNull(TEXT("Initialized unit is spawned"), Unit);
	if (!Unit) { return false; }
	UCombatAbilitySystemComponent* Asc = Unit->GetCombatAbilitySystemComponent();
	TestEqual(TEXT("UnitData initializes MaxHealth"), Asc->GetNumericAttribute(UCombatAttributeSet::GetMaxHealthAttribute()), 200.0f);
	TestEqual(TEXT("UnitData initializes Health to MaxHealth"), Asc->GetNumericAttribute(UCombatAttributeSet::GetHealthAttribute()), 200.0f);
	TestEqual(TEXT("UnitData initializes MaxMana"), Asc->GetNumericAttribute(UCombatAttributeSet::GetMaxManaAttribute()), 120.0f);

	FActorSpawnParameters AbilityUnitParams;
	AbilityUnitParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ACombatUnitCharacter* AbilityUnit = World.SpawnActor<ACombatUnitCharacter>(
		FVector(1000.0, 0.0, 0.0), FRotator::ZeroRotator, AbilityUnitParams);
	UCombatAbilitySet* AbilitySet = NewObject<UCombatAbilitySet>(AbilityUnit);
	AbilitySet->DefinitionName = TEXT("initial_ability_set");
	UCombatAbilityData* InitialAbilityData = NewObject<UCombatAbilityData>(AbilityUnit);
	InitialAbilityData->DefinitionName = TEXT("initial_autocast_ability");
	InitialAbilityData->MaxLevel = 3;
	InitialAbilityData->BehaviorTags.AddTag(CombatTags::Ability_Behavior_NoTarget);
	InitialAbilityData->BehaviorTags.AddTag(CombatTags::Ability_Behavior_AutoCast);
	InitialAbilityData->TargetingRules.TargetTeamTag = CombatTags::TargetTeam_None;
	GetMutableDefault<UCombatSelfHealAbility>()->AbilityData = InitialAbilityData;
	FCombatAbilitySetEntry AbilityEntry;
	AbilityEntry.AbilityClass = UCombatSelfHealAbility::StaticClass();
	AbilityEntry.InitialLevel = 2;
	AbilityEntry.bAutoCastEnabled = true;
	AbilitySet->Abilities.Add(AbilityEntry);
	UCombatUnitData* AbilityUnitData = NewObject<UCombatUnitData>(AbilityUnit);
	AbilityUnitData->DefinitionName = TEXT("ability_initialized_unit");
	AbilityUnitData->AbilitySets.Add(TSoftObjectPtr<UCombatAbilitySet>(AbilitySet));
	TestTrue(TEXT("UnitData grants its initial AbilitySet"), AbilityUnit->InitializeFromUnitData(AbilityUnitData));
	FGameplayAbilitySpec* GrantedSpec = AbilityUnit->GetCombatAbilitySystemComponent()->FindAbilitySpecFromClass(
		UCombatSelfHealAbility::StaticClass());
	TestNotNull(TEXT("Initial AbilitySpec is granted"), GrantedSpec);
	if (GrantedSpec)
	{
		TestEqual(TEXT("AbilitySet initializes Spec.Level"), GrantedSpec->Level, 2);
		TestTrue(TEXT("AbilitySet initializes per-Spec AutoCast"),
			AbilityUnit->GetCombatAbilitySystemComponent()->IsAutoCastEnabled(GrantedSpec->Handle));
	}
	const int32 AbilityCount = AbilityUnit->GetCombatAbilitySystemComponent()->GetActivatableAbilities().Num();
	TestTrue(TEXT("Repeated AbilitySet initialization is idempotent"), AbilityUnit->InitializeFromUnitData(AbilityUnitData));
	TestEqual(TEXT("Idempotent initialization does not duplicate AbilitySpec"),
		AbilityUnit->GetCombatAbilitySystemComponent()->GetActivatableAbilities().Num(), AbilityCount);
	UCombatUnitData* InvalidData = NewObject<UCombatUnitData>(AbilityUnit);
	InvalidData->DefinitionName = TEXT("InvalidName");
	TestFalse(TEXT("Invalid Unit DefinitionName is rejected"), AbilityUnit->InitializeFromUnitData(InvalidData));

	UCombatUnitData* SameData = const_cast<UCombatUnitData*>(Unit->GetUnitData());
	TestTrue(TEXT("Repeated initialization with the same DefinitionId is idempotent"), Unit->InitializeFromUnitData(SameData));
	UCombatUnitData* OtherData = NewObject<UCombatUnitData>(Unit);
	OtherData->DefinitionName = TEXT("different_unit_definition");
	TestFalse(TEXT("A different DefinitionId cannot overwrite an initialized unit"), Unit->InitializeFromUnitData(OtherData));

	Asc->SetNumericAttributeBase(UCombatAttributeSet::GetArmorAttribute(), 20000.0f);
	Asc->SetNumericAttributeBase(UCombatAttributeSet::GetStatusResistancePctAttribute(), 2.0f);
	TestEqual(TEXT("Armor is clamped by Numeric Policy v1"), Asc->GetNumericAttribute(UCombatAttributeSet::GetArmorAttribute()), 10000.0f);
	TestEqual(TEXT("Status resistance is clamped to 0.9"), Asc->GetNumericAttribute(UCombatAttributeSet::GetStatusResistancePctAttribute()), 0.9f);

	CombatCoreTests::SetHealth(*Unit, 50.0f);
	Asc->SetNumericAttributeBase(UCombatAttributeSet::GetManaAttribute(), 50.0f);
	UCombatSchedulerSubsystem* Scheduler = World.GetSubsystem<UCombatSchedulerSubsystem>();
	const double Now = World.GetTimeSeconds();
	Scheduler->RunDueTasks(Now + 1.0);
	TestTrue(TEXT("Health regen compensates four coalesced 0.25s ticks"),
		FMath::IsNearlyEqual(Asc->GetNumericAttribute(UCombatAttributeSet::GetHealthAttribute()), 54.0f));
	TestTrue(TEXT("Mana regen compensates four coalesced 0.25s ticks"),
		FMath::IsNearlyEqual(Asc->GetNumericAttribute(UCombatAttributeSet::GetManaAttribute()), 58.0f));

	FCombatDamageRequest Lethal;
	Lethal.Source = Unit;
	Lethal.Target = Unit;
	Lethal.Amount = 500.0f;
	Lethal.DamageType = ECombatDamageType::Pure;
	UCombatModifierData* PersistentData = CombatCoreTests::MakeModifierData(*Unit, TEXT("persistent_through_death"));
	PersistentData->bRemoveOnDeath = false;
	FCombatModifierApplyRequest PersistentApply;
	PersistentApply.Source = Unit;
	PersistentApply.ModifierData = PersistentData;
	const FCombatModifierHandle PersistentHandle = Unit->GetCombatModifierComponent()->ApplyModifier(PersistentApply).Handle;
	int32 OldLifeCallbackCount = 0;
	Scheduler->ScheduleOnce(Unit, 0.0, 0,
		FCombatScheduledDelegate::CreateLambda([&OldLifeCallbackCount](const FCombatScheduledTickContext&) { ++OldLifeCallbackCount; }));
	const FCombatDamageResult DamageResult = World.GetSubsystem<UCombatDamageSubsystem>()->DealDamage(Lethal);
	TestTrue(TEXT("Lethal damage succeeds"), DamageResult.bSuccess);
	TestEqual(TEXT("Lethal threshold transitions synchronously to Dead"), Unit->GetLifeState(), ECombatLifeState::Dead);
	TestEqual(TEXT("Health reaches exactly zero"), Asc->GetNumericAttribute(UCombatAttributeSet::GetHealthAttribute()), 0.0f);
	TestFalse(TEXT("A second death request is rejected"),
		Unit->GetCombatLifecycleComponent()->RequestDeath(DamageResult.Event.Context, Unit));
	TestEqual(TEXT("Non-RemoveOnDeath modifier remains while Dead"), Unit->GetCombatModifierComponent()->GetActiveModifierCount(), 1);
	Scheduler->RunDueTasks(Now + 1.0);
	TestEqual(TEXT("Dying cancels old life Unit-owned schedules"), OldLifeCallbackCount, 0);
	TestFalse(TEXT("Respawn rejects a NaN location"), Unit->GetCombatLifecycleComponent()->RespawnAtLocation(
		FVector(std::numeric_limits<double>::quiet_NaN(), 0.0, 0.0)));

	const uint32 OldGeneration = Unit->GetLifeGeneration();
	TestTrue(TEXT("Dead unit respawns at a finite position"),
		Unit->GetCombatLifecycleComponent()->RespawnAtLocation(FVector(500.0, 0.0, 100.0)));
	TestEqual(TEXT("Respawn restores Alive"), Unit->GetLifeState(), ECombatLifeState::Alive);
	TestEqual(TEXT("Respawn increments LifeGeneration"), Unit->GetLifeGeneration(), OldGeneration + 1);
	TestEqual(TEXT("Respawn restores Health to MaxHealth"), Asc->GetNumericAttribute(UCombatAttributeSet::GetHealthAttribute()), 200.0f);
	TestEqual(TEXT("Respawn restores Mana to MaxMana"), Asc->GetNumericAttribute(UCombatAttributeSet::GetManaAttribute()), 120.0f);
	TestNotNull(TEXT("Non-RemoveOnDeath modifier survives Respawn"), Unit->GetCombatModifierComponent()->FindRuntime(PersistentHandle));
	TestFalse(TEXT("Alive unit cannot respawn twice"), Unit->GetCombatLifecycleComponent()->RespawnAtLocation(FVector::ZeroVector));
	Unit->GetCombatModifierComponent()->RemoveModifier(PersistentHandle);
	return true;
}

/** 验证 Damage Calculator、Damage/Heal 流水线和免疫边界。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCombatDamageHealPipelineTest,
	"Combat.Core.Transactions.DamageAndHealPipeline",
	CombatCoreTests::Flags)

/** 执行物理正负护甲、魔法、纯粹、HPLoss、魔免和过量治疗断言。 */
bool FCombatDamageHealPipelineTest::RunTest(const FString& Parameters)
{
	FCombatAutomationWorldFixture Fixture;
	if (!Fixture.IsValid()) { AddError(TEXT("Could not create M2 automation world")); return false; }
	UWorld& World = *Fixture.GetWorld();
	ACombatUnitCharacter* Source = CombatCoreTests::SpawnInitializedUnit(World, TEXT("damage_source"), FVector::ZeroVector);
	ACombatUnitCharacter* Target = CombatCoreTests::SpawnInitializedUnit(World, TEXT("damage_target"), FVector(400.0, 0.0, 0.0));
	if (!Source || !Target) { AddError(TEXT("Could not spawn damage test units")); return false; }
	UCombatAbilitySystemComponent* TargetAsc = Target->GetCombatAbilitySystemComponent();
	UCombatDamageSubsystem* Damage = World.GetSubsystem<UCombatDamageSubsystem>();

	TargetAsc->SetNumericAttributeBase(UCombatAttributeSet::GetArmorAttribute(), 10.0f);
	FCombatDamageRequest Request;
	Request.Source = Source;
	Request.Target = Target;
	Request.Amount = 100.0f;
	Request.DamageType = ECombatDamageType::Physical;
	FCombatDamageResult Result = Damage->DealDamage(Request);
	TestTrue(TEXT("Positive armor physical damage succeeds"), Result.bSuccess);
	TestTrue(TEXT("Armor=10 applies 0.625 multiplier"), FMath::IsNearlyEqual(Result.Event.AppliedAmount, 62.5f, 0.01f));

	CombatCoreTests::SetHealth(*Target, 100.0f);
	TargetAsc->SetNumericAttributeBase(UCombatAttributeSet::GetArmorAttribute(), -10.0f);
	Request.Amount = 50.0f;
	Result = Damage->DealDamage(Request);
	const float NegativeArmorExpected = 50.0f * (2.0f - FMath::Pow(0.94f, 10.0f));
	TestTrue(TEXT("Negative armor uses frozen v1 formula"),
		FMath::IsNearlyEqual(Result.Event.AppliedAmount, NegativeArmorExpected, 0.01f));

	CombatCoreTests::SetHealth(*Target, 100.0f);
	Request.Amount = 100.0f;
	TargetAsc->SetNumericAttributeBase(UCombatAttributeSet::GetMagicResistAttribute(), 0.25f);
	Request.DamageType = ECombatDamageType::Magical;
	Result = Damage->DealDamage(Request);
	TestTrue(TEXT("25 percent magic resistance applies 75 damage"), FMath::IsNearlyEqual(Result.Event.AppliedAmount, 75.0f));

	CombatCoreTests::SetHealth(*Target, 100.0f);
	Request.DamageType = ECombatDamageType::Pure;
	Result = Damage->DealDamage(Request);
	TestEqual(TEXT("Pure damage skips resistance"), Result.Event.AppliedAmount, 100.0f);

	Target->GetCombatLifecycleComponent()->RespawnAtLocation(Target->GetActorLocation());
	TargetAsc->SetNumericAttributeBase(UCombatAttributeSet::GetMaxHealthAttribute(), 300.0f);
	CombatCoreTests::SetHealth(*Target, 300.0f);
	TargetAsc->SetNumericAttributeBase(UCombatAttributeSet::GetMagicResistAttribute(), 0.0f);
	Source->GetCombatAbilitySystemComponent()->SetNumericAttributeBase(
		UCombatAttributeSet::GetSpellAmplifyPctAttribute(), 0.5f);
	Request.Amount = 100.0f;
	Request.DamageType = ECombatDamageType::Magical;
	Result = Damage->DealDamage(Request);
	TestEqual(TEXT("SpellAmplify increases spell damage before target resistance"), Result.Event.AppliedAmount, 150.0f);
	CombatCoreTests::SetHealth(*Target, 300.0f);
	Request.Flags.AddTag(CombatTags::Damage_Flag_NoSpellAmplification);
	Result = Damage->DealDamage(Request);
	TestEqual(TEXT("NoSpellAmplification skips source spell amp"), Result.Event.AppliedAmount, 100.0f);
	Request.Flags.Reset();
	TargetAsc->SetNumericAttributeBase(UCombatAttributeSet::GetMagicResistAttribute(), 0.25f);
	CombatCoreTests::SetHealth(*Target, 300.0f);

	TargetAsc->AddLooseGameplayTag(CombatTags::State_MagicImmune);
	Request.DamageType = ECombatDamageType::Magical;
	Result = Damage->DealDamage(Request);
	TestTrue(TEXT("Magic immune result is a successful block"), Result.bSuccess && Result.bBlocked);
	TestEqual(TEXT("Magic immune applies no Health delta"), Result.Event.AppliedAmount, 0.0f);
	Request.Amount = 10.0f;
	Request.Flags.AddTag(CombatTags::Damage_Flag_BypassMagicImmune);
	Result = Damage->DealDamage(Request);
	TestTrue(TEXT("BypassMagicImmune reaches the normal resistance path"), Result.bSuccess && Result.Event.AppliedAmount > 0.0f);
	Request.Flags.Reset();
	Request.Flags.AddTag(CombatTags::Damage_Flag_HPLoss);
	Request.Amount = 10.0f;
	Result = Damage->DealDamage(Request);
	TestTrue(TEXT("HPLoss bypasses magic immunity"), Result.bSuccess && Result.Event.AppliedAmount > 0.0f);
	Request.Flags.Reset();
	TargetAsc->RemoveLooseGameplayTag(CombatTags::State_MagicImmune);
	TargetAsc->AddLooseGameplayTag(CombatTags::State_Invulnerable);
	Result = Damage->DealDamage(Request);
	TestTrue(TEXT("Invulnerable blocks normal damage without side effects"), Result.bSuccess && Result.bBlocked);
	TargetAsc->RemoveLooseGameplayTag(CombatTags::State_Invulnerable);
	TargetAsc->AddLooseGameplayTag(CombatTags::State_OutOfGame);
	Result = Damage->DealDamage(Request);
	TestFalse(TEXT("OutOfGame target is rejected"), Result.bSuccess);
	TestEqual(TEXT("OutOfGame returns its stable failure tag"), Result.FailureTag, CombatTags::Failure_Target_OutOfGame.GetTag());
	TargetAsc->RemoveLooseGameplayTag(CombatTags::State_OutOfGame);

	CombatCoreTests::SetHealth(*Target, 250.0f);
	FCombatHealRequest HealRequest;
	HealRequest.Source = Source;
	HealRequest.Target = Target;
	HealRequest.Amount = 80.0f;
	const FCombatHealResult HealResult = World.GetSubsystem<UCombatHealSubsystem>()->Heal(HealRequest);
	TestTrue(TEXT("Heal succeeds"), HealResult.bSuccess);
	TestEqual(TEXT("Heal reports actual clamped delta"), HealResult.Event.AppliedAmount, 50.0f);
	TestEqual(TEXT("Heal reports overheal"), HealResult.Event.OverhealAmount, 30.0f);
	const FCombatHealResult FullHealthResult = World.GetSubsystem<UCombatHealSubsystem>()->Heal(HealRequest);
	TestTrue(TEXT("Full-health Heal remains a successful transaction"), FullHealthResult.bSuccess);
	TestEqual(TEXT("Full-health Heal reports AppliedHealing zero"), FullHealthResult.Event.AppliedAmount, 0.0f);

	// 同时验证来源治疗增幅与目标受治疗增幅只在 Heal 流水线中组合一次。
	Source->GetCombatAbilitySystemComponent()->SetNumericAttributeBase(
		UCombatAttributeSet::GetHealAmplifyPctAttribute(), 0.5f);
	TargetAsc->SetNumericAttributeBase(UCombatAttributeSet::GetHealReceivedPctAttribute(), -0.5f);
	CombatCoreTests::SetHealth(*Target, 250.0f);
	HealRequest.Amount = 40.0f;
	const FCombatHealResult AmplifiedHealResult = World.GetSubsystem<UCombatHealSubsystem>()->Heal(HealRequest);
	TestTrue(TEXT("Heal amplification transaction succeeds"), AmplifiedHealResult.bSuccess);
	TestEqual(TEXT("HealAmplify and HealReceived combine exactly once"), AmplifiedHealResult.Event.AppliedAmount, 30.0f);
	Source->GetCombatAbilitySystemComponent()->SetNumericAttributeBase(
		UCombatAttributeSet::GetHealAmplifyPctAttribute(), 0.0f);
	TargetAsc->SetNumericAttributeBase(UCombatAttributeSet::GetHealReceivedPctAttribute(), 0.0f);
	HealRequest.Amount = 80.0f;

	TargetAsc->AddLooseGameplayTag(CombatTags::State_OutOfGame);
	TestFalse(TEXT("Heal rejects OutOfGame target"), World.GetSubsystem<UCombatHealSubsystem>()->Heal(HealRequest).bSuccess);
	TargetAsc->RemoveLooseGameplayTag(CombatTags::State_OutOfGame);

	// Heal 不是复活入口：致死后的目标必须保持 Dead 与零生命。
	Request.Amount = 1000.0f;
	Request.DamageType = ECombatDamageType::Pure;
	Request.Flags.Reset();
	TestTrue(TEXT("Lethal setup damage succeeds"), Damage->DealDamage(Request).bSuccess);
	const FCombatHealResult DeadHealResult = World.GetSubsystem<UCombatHealSubsystem>()->Heal(HealRequest);
	TestFalse(TEXT("Heal cannot revive a Dead target"), DeadHealResult.bSuccess);
	TestEqual(TEXT("Dead Heal reports the stable life-state failure"), DeadHealResult.FailureTag, CombatTags::Failure_Life_NotAlive.GetTag());
	TestEqual(TEXT("Rejected Dead Heal leaves Health at zero"), TargetAsc->GetNumericAttribute(UCombatAttributeSet::GetHealthAttribute()), 0.0f);

	Request.Amount = -1.0f;
	TestFalse(TEXT("Negative damage request is rejected"), Damage->DealDamage(Request).bSuccess);
	Request.Amount = std::numeric_limits<float>::quiet_NaN();
	TestFalse(TEXT("NaN damage request is rejected"), Damage->DealDamage(Request).bSuccess);
	Request.Amount = std::numeric_limits<float>::infinity();
	TestFalse(TEXT("Infinite damage request is rejected"), Damage->DealDamage(Request).bSuccess);
	FCombatDamageRequest NullDamageRequest;
	NullDamageRequest.Target = Target;
	NullDamageRequest.Amount = 1.0f;
	TestFalse(TEXT("Null damage source is rejected"), Damage->DealDamage(NullDamageRequest).bSuccess);

	HealRequest.Amount = -1.0f;
	TestFalse(TEXT("Negative Heal request is rejected"), World.GetSubsystem<UCombatHealSubsystem>()->Heal(HealRequest).bSuccess);
	HealRequest.Amount = std::numeric_limits<float>::quiet_NaN();
	TestFalse(TEXT("NaN Heal request is rejected"), World.GetSubsystem<UCombatHealSubsystem>()->Heal(HealRequest).bSuccess);
	HealRequest.Amount = std::numeric_limits<float>::infinity();
	TestFalse(TEXT("Infinite Heal request is rejected"), World.GetSubsystem<UCombatHealSubsystem>()->Heal(HealRequest).bSuccess);
	FCombatHealRequest NullHealRequest;
	NullHealRequest.Target = Target;
	NullHealRequest.Amount = 1.0f;
	TestFalse(TEXT("Null Heal source is rejected"), World.GetSubsystem<UCombatHealSubsystem>()->Heal(NullHealRequest).bSuccess);
	TestEqual(TEXT("All synchronous result slots are closed"),
		World.GetSubsystem<UCombatTransactionSubsystem>()->GetOpenSlotCount(), 0);
	return true;
}

/** 验证 Modifier 一一映射、稳定 Shield 顺序、状态计数、驱散和状态抗性。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCombatModifierCoreTest,
	"Combat.Core.Modifiers.ShieldStatusAndDispel",
	CombatCoreTests::Flags)

/** 执行 MOD-001..005 与 DEMO-201/202 的公共路径断言。 */
bool FCombatModifierCoreTest::RunTest(const FString& Parameters)
{
	FCombatAutomationWorldFixture Fixture;
	if (!Fixture.IsValid()) { AddError(TEXT("Could not create M2 automation world")); return false; }
	UWorld& World = *Fixture.GetWorld();
	ACombatUnitCharacter* Source = CombatCoreTests::SpawnInitializedUnit(World, TEXT("modifier_source"), FVector::ZeroVector);
	ACombatUnitCharacter* Target = CombatCoreTests::SpawnInitializedUnit(World, TEXT("modifier_target"), FVector(400.0, 0.0, 0.0));
	if (!Source || !Target) { AddError(TEXT("Could not spawn modifier test units")); return false; }
	UCombatModifierComponent* Modifiers = Target->GetCombatModifierComponent();
	UCombatAbilitySystemComponent* TargetAsc = Target->GetCombatAbilitySystemComponent();

	auto MakeShield = [&](const FName Name, const int32 Priority, const float Amount)
	{
		UCombatModifierData* Data = CombatCoreTests::MakeModifierData(*Target, Name);
		Data->RuntimeClass = UCombatMagicShieldRuntime::StaticClass();
		Data->Priority = Priority;
		Data->RuntimeParameters.Add(TEXT("shield_amount"), Amount);
		FCombatModifierAttributeChange ResistChange;
		ResistChange.Attribute = UCombatAttributeSet::GetMagicResistAttribute();
		ResistChange.Magnitude = 0.10f;
		Data->AttributeChanges.Add(ResistChange);
		return Data;
	};

	UCombatModifierData* HighShield = MakeShield(TEXT("shield_high"), 10, 30.0f);
	UCombatModifierData* LowShield = MakeShield(TEXT("shield_low"), 10, 50.0f);
	FCombatModifierApplyRequest Apply;
	Apply.Source = Source;
	Apply.ModifierData = HighShield;
	const FCombatModifierApplyResult HighShieldApply = Modifiers->ApplyModifier(Apply);
	TestTrue(TEXT("First same-priority shield applies"), HighShieldApply.bSuccess);
	Apply.ModifierData = LowShield;
	const FCombatModifierApplyResult LowShieldApply = Modifiers->ApplyModifier(Apply);
	TestTrue(TEXT("Second same-priority shield applies"), LowShieldApply.bSuccess);
	TestTrue(TEXT("Both ActiveGE modifiers aggregate MagicResist"),
		FMath::IsNearlyEqual(TargetAsc->GetNumericAttribute(UCombatAttributeSet::GetMagicResistAttribute()), 0.45f));

	FCombatDamageRequest DamageRequest;
	DamageRequest.Source = Source;
	DamageRequest.Target = Target;
	DamageRequest.Amount = 100.0f;
	DamageRequest.DamageType = ECombatDamageType::Magical;
	FCombatDamageResult Shielded = World.GetSubsystem<UCombatDamageSubsystem>()->DealDamage(DamageRequest);
	TestTrue(TEXT("Same-priority shields consume by ApplySequence"), FMath::IsNearlyEqual(Shielded.Event.AbsorbedAmount, 55.0f));
	TestEqual(TEXT("First shield exhausts while second runtime remains"), Modifiers->GetActiveModifierCount(), 1);
	const UCombatMagicShieldRuntime* RemainingShield = Cast<UCombatMagicShieldRuntime>(Modifiers->FindRuntime(LowShieldApply.Handle));
	TestNotNull(TEXT("Second ApplySequence shield is the remaining runtime"), RemainingShield);
	if (RemainingShield) { TestEqual(TEXT("Second shield retains the unconsumed amount"), RemainingShield->GetRemainingShield(), 25.0f); }
	TestTrue(TEXT("Only the remaining shield continues contributing MagicResist"),
		FMath::IsNearlyEqual(TargetAsc->GetNumericAttribute(UCombatAttributeSet::GetMagicResistAttribute()), 0.35f));
	Shielded = World.GetSubsystem<UCombatDamageSubsystem>()->DealDamage(DamageRequest);
	TestTrue(TEXT("Second hit consumes remaining shield before Health"), FMath::IsNearlyEqual(Shielded.Event.AbsorbedAmount, 25.0f));
	TestTrue(TEXT("DamageResult uses true post-shield Health delta"), FMath::IsNearlyEqual(Shielded.Event.AppliedAmount, 40.0f));
	TestEqual(TEXT("Both exhausted shields deferred-remove their runtimes"), Modifiers->GetActiveModifierCount(), 0);
	TestTrue(TEXT("Removing runtimes removes their GE MagicResist"),
		FMath::IsNearlyEqual(TargetAsc->GetNumericAttribute(UCombatAttributeSet::GetMagicResistAttribute()), 0.25f));

	Apply.ModifierData = HighShield;
	const FCombatModifierHandle HpLossShieldHandle = Modifiers->ApplyModifier(Apply).Handle;
	TargetAsc->AddLooseGameplayTag(CombatTags::State_MagicImmune);
	DamageRequest.Amount = 10.0f;
	DamageRequest.Flags.Reset();
	const FCombatDamageResult BlockedShieldDamage = World.GetSubsystem<UCombatDamageSubsystem>()->DealDamage(DamageRequest);
	TestTrue(TEXT("Magic immunity blocks before Shield Hook"), BlockedShieldDamage.bSuccess && BlockedShieldDamage.bBlocked);
	const UCombatMagicShieldRuntime* ShieldRuntime = Cast<UCombatMagicShieldRuntime>(Modifiers->FindRuntime(HpLossShieldHandle));
	TestNotNull(TEXT("Blocked damage does not remove shield"), ShieldRuntime);
	if (ShieldRuntime) { TestEqual(TEXT("Blocked damage does not consume shield"), ShieldRuntime->GetRemainingShield(), 30.0f); }
	DamageRequest.Flags.AddTag(CombatTags::Damage_Flag_HPLoss);
	World.GetSubsystem<UCombatDamageSubsystem>()->DealDamage(DamageRequest);
	ShieldRuntime = Cast<UCombatMagicShieldRuntime>(Modifiers->FindRuntime(HpLossShieldHandle));
	TestNotNull(TEXT("HPLoss does not remove shield"), ShieldRuntime);
	if (ShieldRuntime) { TestEqual(TEXT("HPLoss does not consume shield"), ShieldRuntime->GetRemainingShield(), 30.0f); }
	TargetAsc->RemoveLooseGameplayTag(CombatTags::State_MagicImmune);
	DamageRequest.Flags.Reset();
	Modifiers->RemoveModifier(HpLossShieldHandle);

	UCombatModifierData* RefreshData = CombatCoreTests::MakeModifierData(*Target, TEXT("refresh_stack"));
	RefreshData->Duration = 5.0f;
	RefreshData->ThinkInterval = 1.0f;
	RefreshData->MaxStacks = 2;
	RefreshData->RefreshPolicy = ECombatModifierRefreshPolicy::PreservePhase;
	Apply.ModifierData = RefreshData;
	const FCombatModifierApplyResult FirstApply = Modifiers->ApplyModifier(Apply);
	UCombatModifierRuntime* RefreshRuntime = Modifiers->FindRuntime(FirstApply.Handle);
	const FCombatScheduleHandle OriginalThinkHandle = RefreshRuntime ? RefreshRuntime->GetThinkScheduleHandle() : FCombatScheduleHandle();
	const FCombatModifierApplyResult PreservedRefresh = Modifiers->ApplyModifier(Apply);
	RefreshRuntime = Modifiers->FindRuntime(FirstApply.Handle);
	TestTrue(TEXT("Refresh reuses the same Runtime handle"), PreservedRefresh.bRefreshed && PreservedRefresh.Handle == FirstApply.Handle);
	TestEqual(TEXT("Refresh increments Runtime stack without a second instance"), RefreshRuntime ? RefreshRuntime->GetStackCount() : 0, 2);
	TestEqual(TEXT("ActiveGE stack matches Runtime stack"),
		RefreshRuntime ? TargetAsc->GetCurrentStackCount(RefreshRuntime->GetActiveEffectHandle()) : 0, 2);
	TestTrue(TEXT("PreservePhase keeps the Think schedule handle"),
		RefreshRuntime && RefreshRuntime->GetThinkScheduleHandle() == OriginalThinkHandle);
	RefreshData->RefreshPolicy = ECombatModifierRefreshPolicy::ResetInterval;
	Modifiers->ApplyModifier(Apply);
	RefreshRuntime = Modifiers->FindRuntime(FirstApply.Handle);
	TestTrue(TEXT("ResetInterval establishes a new Think schedule generation"),
		RefreshRuntime && RefreshRuntime->GetThinkScheduleHandle() != OriginalThinkHandle);
	Modifiers->RemoveModifier(FirstApply.Handle);

	UCombatModifierData* StunA = CombatCoreTests::MakeModifierData(*Target, TEXT("stun_a"));
	UCombatModifierData* StunB = CombatCoreTests::MakeModifierData(*Target, TEXT("stun_b"));
	StunA->bIsDebuff = true;
	StunB->bIsDebuff = true;
	StunA->GrantedTags.AddTag(CombatTags::State_Stunned);
	StunB->GrantedTags.AddTag(CombatTags::State_Stunned);
	Apply.ModifierData = StunA;
	const FCombatModifierHandle StunAHandle = Modifiers->ApplyModifier(Apply).Handle;
	Apply.ModifierData = StunB;
	const FCombatModifierHandle StunBHandle = Modifiers->ApplyModifier(Apply).Handle;
	TestEqual(TEXT("Two stun ActiveGEs produce tag count two"), TargetAsc->GetTagCount(CombatTags::State_Stunned), 2);
	TestTrue(TEXT("Stun blocks movement"), Target->IsMovementBlocked());
	Modifiers->RemoveModifier(StunAHandle);
	TestEqual(TEXT("Removing one source preserves remaining stun"), TargetAsc->GetTagCount(CombatTags::State_Stunned), 1);
	Modifiers->RemoveModifier(StunBHandle);
	TestEqual(TEXT("Removing final source clears stun"), TargetAsc->GetTagCount(CombatTags::State_Stunned), 0);

	UCombatModifierData* Slow = CombatCoreTests::MakeModifierData(*Target, TEXT("slow"));
	FCombatModifierAttributeChange SlowChange;
	SlowChange.Attribute = UCombatAttributeSet::GetMoveSpeedAttribute();
	SlowChange.Magnitude = -100.0f;
	Slow->AttributeChanges.Add(SlowChange);
	Apply.ModifierData = Slow;
	const FCombatModifierHandle SlowHandle = Modifiers->ApplyModifier(Apply).Handle;
	TestEqual(TEXT("Slow is expressed by ActiveGE Attribute change"),
		TargetAsc->GetNumericAttribute(UCombatAttributeSet::GetMoveSpeedAttribute()), 200.0f);
	Modifiers->RemoveModifier(SlowHandle);
	TestEqual(TEXT("Slow removal restores aggregate MoveSpeed"),
		TargetAsc->GetNumericAttribute(UCombatAttributeSet::GetMoveSpeedAttribute()), 300.0f);

	TargetAsc->SetNumericAttributeBase(UCombatAttributeSet::GetStatusResistancePctAttribute(), 0.5f);
	UCombatModifierData* Resistible = CombatCoreTests::MakeModifierData(*Target, TEXT("resistible_debuff"));
	Resistible->bIsDebuff = true;
	Resistible->bDurationAffectedByStatusResistance = true;
	Resistible->Duration = 10.0f;
	Apply.ModifierData = Resistible;
	const FCombatModifierHandle ResistibleHandle = Modifiers->ApplyModifier(Apply).Handle;
	const UCombatModifierRuntime* ResistibleRuntime = Modifiers->FindRuntime(ResistibleHandle);
	TestNotNull(TEXT("Resistible debuff applies"), ResistibleRuntime);
	if (ResistibleRuntime)
	{
		TestTrue(TEXT("Status resistance snapshots a five-second duration"),
			FMath::IsNearlyEqual(ResistibleRuntime->GetExpireAt() - World.GetTimeSeconds(), 5.0, 0.01));
	}

	UCombatModifierData* Basic = CombatCoreTests::MakeModifierData(*Target, TEXT("dispel_basic"));
	UCombatModifierData* Strong = CombatCoreTests::MakeModifierData(*Target, TEXT("dispel_strong"));
	UCombatModifierData* Permanent = CombatCoreTests::MakeModifierData(*Target, TEXT("dispel_none"));
	Basic->bIsDebuff = Strong->bIsDebuff = Permanent->bIsDebuff = true;
	Strong->DispelRule = ECombatModifierDispelRule::StrongOnly;
	Permanent->DispelRule = ECombatModifierDispelRule::NotDispellable;
	Apply.ModifierData = Basic; Modifiers->ApplyModifier(Apply);
	Apply.ModifierData = Strong; Modifiers->ApplyModifier(Apply);
	Apply.ModifierData = Permanent;
	const FCombatModifierHandle PermanentHandle = Modifiers->ApplyModifier(Apply).Handle;
	TestEqual(TEXT("Basic dispel removes only Basic rule"), Modifiers->Dispel(ECombatDispelStrength::Basic), 2); // 还包括前面施加、受状态抗性缩短的普通可驱散效果，因此移除数为 2。
	TestEqual(TEXT("Strong dispel removes StrongOnly but not NotDispellable"), Modifiers->Dispel(ECombatDispelStrength::Strong), 1);
	TestEqual(TEXT("NotDispellable runtime remains"), Modifiers->GetActiveModifierCount(), 1);
	Modifiers->RemoveModifier(PermanentHandle);
	int32 ModifierAppliedLogs = 0;
	int32 ModifierRemovedLogs = 0;
	for (const FCombatLogRecord& Record : World.GetSubsystem<UCombatEventSubsystem>()->GetRecentRecords())
	{
		ModifierAppliedLogs += Record.EventType == CombatTags::Event_Combat_ModifierApplied ? 1 : 0;
		ModifierRemovedLogs += Record.EventType == CombatTags::Event_Combat_ModifierRemoved ? 1 : 0;
	}
	TestTrue(TEXT("Modifier apply/refresh emits structured logs"), ModifierAppliedLogs > 0);
	TestTrue(TEXT("Modifier remove/dispel emits structured logs"), ModifierRemovedLogs > 0);
	return true;
}

/** 验证 DOT 边界 tick、反伤防递归、吸血和 Result Log 因果链。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCombatPeriodicFollowupLogTest,
	"Combat.Core.VerticalSlice.PeriodicFollowupsAndLogs",
	CombatCoreTests::Flags)

/** 执行 DEMO-202、Reflection/Lifesteal 与 OBS-002 断言。 */
bool FCombatPeriodicFollowupLogTest::RunTest(const FString& Parameters)
{
	FCombatAutomationWorldFixture Fixture;
	if (!Fixture.IsValid()) { AddError(TEXT("Could not create M2 automation world")); return false; }
	UWorld& World = *Fixture.GetWorld();
	ACombatUnitCharacter* Source = CombatCoreTests::SpawnInitializedUnit(World, TEXT("followup_source"), FVector::ZeroVector);
	ACombatUnitCharacter* Target = CombatCoreTests::SpawnInitializedUnit(World, TEXT("followup_target"), FVector(400.0, 0.0, 0.0));
	if (!Source || !Target) { AddError(TEXT("Could not spawn follow-up test units")); return false; }

	UCombatModifierData* Dot = CombatCoreTests::MakeModifierData(*Target, TEXT("dot_boundary"));
	Dot->RuntimeClass = UCombatPeriodicDamageRuntime::StaticClass();
	Dot->Priority = 10;
	Dot->Duration = 2.0f;
	Dot->ThinkInterval = 1.0f;
	Dot->bTickOnExpire = true;
	Dot->RuntimeParameters.Add(TEXT("damage_per_tick"), 10.0f);
	Dot->RuntimeParameters.Add(TEXT("damage_type"), 2.0f);
	FCombatModifierApplyRequest Apply;
	Apply.Source = Source;
	Apply.ModifierData = Dot;
	TestTrue(TEXT("Boundary DOT applies"), Target->GetCombatModifierComponent()->ApplyModifier(Apply).bSuccess);
	UCombatSchedulerSubsystem* Scheduler = World.GetSubsystem<UCombatSchedulerSubsystem>();
	Scheduler->RunDueTasks(World.GetTimeSeconds() + 2.0);
	TestEqual(TEXT("bTickOnExpire executes ticks at 1s and exactly 2s"),
		Target->GetCombatAbilitySystemComponent()->GetNumericAttribute(UCombatAttributeSet::GetHealthAttribute()), 80.0f);
	TestEqual(TEXT("DOT naturally removes at ExpireAt"), Target->GetCombatModifierComponent()->GetActiveModifierCount(), 0);

	CombatCoreTests::SetHealth(*Source, 50.0f);
	CombatCoreTests::SetHealth(*Target, 100.0f);
	Source->GetCombatAbilitySystemComponent()->SetNumericAttributeBase(UCombatAttributeSet::GetLifestealPctAttribute(), 0.5f);
	UCombatModifierData* Reflect = CombatCoreTests::MakeModifierData(*Target, TEXT("reflection"));
	Reflect->RuntimeClass = UCombatDamageReflectionRuntime::StaticClass();
	Reflect->RuntimeParameters.Add(TEXT("reflection_pct"), 0.5f);
	Apply.ModifierData = Reflect;
	Target->GetCombatModifierComponent()->ApplyModifier(Apply);

	UCombatEventSubsystem* Events = World.GetSubsystem<UCombatEventSubsystem>();
	const int32 LogStart = Events->GetRecentRecords().Num();
	FCombatDamageRequest Request;
	Request.Source = Source;
	Request.Target = Target;
	Request.Amount = 40.0f;
	Request.DamageType = ECombatDamageType::Pure;
	const FCombatDamageResult ParentResult = World.GetSubsystem<UCombatDamageSubsystem>()->DealDamage(Request);
	TestTrue(TEXT("Parent damage with reflection/lifesteal succeeds"), ParentResult.bSuccess);
	TestEqual(TEXT("Reflection deals 20 then lifesteal heals 20, leaving source unchanged"),
		Source->GetCombatAbilitySystemComponent()->GetNumericAttribute(UCombatAttributeSet::GetHealthAttribute()), 50.0f);
	TestEqual(TEXT("Target takes parent damage"),
		Target->GetCombatAbilitySystemComponent()->GetNumericAttribute(UCombatAttributeSet::GetHealthAttribute()), 60.0f);

	int32 DamageLogs = 0;
	int32 HealLogs = 0;
	bool bFoundReflectionChild = false;
	for (int32 Index = LogStart; Index < Events->GetRecentRecords().Num(); ++Index)
	{
		const FCombatLogRecord& Record = Events->GetRecentRecords()[Index];
		if (Record.EventType == CombatTags::Event_Combat_DamageApplied)
		{
			++DamageLogs;
			if (Record.Context.Depth == 1 && Record.Flags.HasTagExact(CombatTags::Damage_Flag_Reflection))
			{
				bFoundReflectionChild = Record.Context.RootEventId == ParentResult.Event.Context.RootEventId
					&& FMath::IsNearlyEqual(Record.AppliedAmount, 20.0f);
			}
		}
		else if (Record.EventType == CombatTags::Event_Combat_HealApplied)
		{
			++HealLogs;
		}
	}
	TestEqual(TEXT("Parent and reflection each emit one DamageResult log"), DamageLogs, 2);
	TestEqual(TEXT("Lifesteal emits one HealResult log"), HealLogs, 1);
	TestTrue(TEXT("Reflection child inherits RootEventId and records AppliedAmount"), bFoundReflectionChild);
	return true;
}

/** 验证同步结果槽拒绝未知、重复和错误生命周期。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCombatTransactionSlotTest,
	"Combat.Core.Transactions.ResultSlotExactlyOnce",
	CombatCoreTests::Flags)

/** 直接执行 CMB-001 的槽位 exactly-once 边界断言。 */
bool FCombatTransactionSlotTest::RunTest(const FString& Parameters)
{
	FCombatAutomationWorldFixture Fixture;
	if (!Fixture.IsValid()) { AddError(TEXT("Could not create M2 automation world")); return false; }
	UWorld& World = *Fixture.GetWorld();
	ACombatUnitCharacter* Unit = CombatCoreTests::SpawnInitializedUnit(World, TEXT("slot_unit"), FVector::ZeroVector);
	UCombatTransactionSubsystem* Transactions = World.GetSubsystem<UCombatTransactionSubsystem>();
	FCombatEventContext Context = World.GetSubsystem<UCombatEventSubsystem>()->CreateRootEvent();
	TestTrue(TEXT("Valid transaction slot opens"), Transactions->BeginSlot(Context, ECombatTransactionKind::Damage, Unit));
	TestFalse(TEXT("Duplicate EventId slot is rejected"), Transactions->BeginSlot(Context, ECombatTransactionKind::Damage, Unit));
	FCombatTransactionDelta Delta;
	Delta.PreviousHealth = 100.0f;
	Delta.NewHealth = 90.0f;
	Delta.AppliedAmount = 10.0f;
	TestTrue(TEXT("First matching report succeeds"), Transactions->ReportDelta(Context.EventId, ECombatTransactionKind::Damage, Delta));
	TestFalse(TEXT("Duplicate report is rejected"), Transactions->ReportDelta(Context.EventId, ECombatTransactionKind::Damage, Delta));
	FCombatTransactionDelta Consumed;
	TestTrue(TEXT("Reported slot consumes once"), Transactions->ConsumeSlot(Context.EventId, ECombatTransactionKind::Damage, Consumed));
	TestEqual(TEXT("Consumed slot preserves true delta"), Consumed.AppliedAmount, 10.0f);
	TestFalse(TEXT("Consumed slot cannot be read twice"), Transactions->ConsumeSlot(Context.EventId, ECombatTransactionKind::Damage, Consumed));
	return true;
}

#endif
