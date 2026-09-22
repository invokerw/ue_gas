#include "CoreMinimal.h"

#include <limits>

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Misc/PackageName.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"

#include "Combat/Ability/CombatAbilitySystemComponent.h"
#include "Combat/AI/CombatAIBrainComponent.h"
#include "Combat/AI/CombatAIProfileData.h"
#include "Combat/AI/CombatAIRoleTasks.h"
#include "Combat/AI/CombatAITacticalTargetContext.h"
#include "Combat/AI/CombatAITacticalTypes.h"
#include "Combat/AI/CombatAIWorldSubsystem.h"
#include "Combat/Attack/CombatAttackComponent.h"
#include "Combat/Core/CombatTags.h"
#include "Combat/Data/CombatDefinitionData.h"
#include "Combat/Demo/CombatDemoAbilities.h"
#include "Combat/Log/CombatEventSubsystem.h"
#include "Combat/Order/CombatOrderComponent.h"
#include "Combat/Scheduling/CombatSchedulerSubsystem.h"
#include "Combat/Tests/CombatAutomationWorldFixture.h"
#include "Combat/Unit/CombatUnitCharacter.h"
#include "Combat/Unit/CombatUnitLifecycleComponent.h"
#include "Combat/Validation/CombatAIAssetBuilder.h"
#include "EnvironmentQuery/EnvQuery.h"
#include "EnvironmentQuery/EnvQueryOption.h"
#include "EnvironmentQuery/EnvQueryTypes.h"
#include "StateTree.h"

namespace CombatAITacticsTests
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;

	ACombatUnitCharacter* Spawn(UWorld& World)
	{
		auto* Unit = World.SpawnActor<ACombatUnitCharacter>();
		auto* Data = NewObject<UCombatUnitData>(Unit);
		Data->DefinitionName = TEXT("ai_tactics_unit");
		Unit->InitializeFromUnitData(Data);
		Unit->GetCharacterMovement()->GravityScale = 0.0f;
		return Unit;
	}

	UCombatAIProfileData* Profile(UObject* Outer)
	{
		auto* Result = NewObject<UCombatAIProfileData>(Outer);
		Result->DefinitionName = TEXT("ai_tactics_profile_fixture");
		Result->RootTree = FCombatAIAssetBuilder::BuildRootTree(
			Result, FCombatAIAssetBuilder::BuildActionTree(Result));
		Result->AIProfileVersion = 2;
		Result->bEnableTactics = true;
		return Result;
	}

	void Advance(UWorld& World, const int32 Frames)
	{
		for (int32 Index = 0; Index < Frames; ++Index)
		{
			World.Tick(LEVELTICK_All, 0.05f);
			World.GetSubsystem<UCombatSchedulerSubsystem>()->RunDueTasks(World.GetTimeSeconds());
			for (TActorIterator<ACombatUnitCharacter> It(&World); It; ++It)
			{
				auto* Brain = It->GetCombatAIBrainComponent();
				if (Brain->IsRunning() && Brain->IsComponentTickEnabled())
				{
					Brain->TickComponent(0.05f, LEVELTICK_All, nullptr);
				}
			}
		}
	}

	bool AdvanceUntilTacticalQueryStarts(UWorld& World, UCombatAIBrainComponent& Brain, const int32 MaxFrames = 16)
	{
		for (int32 Index = 0; Index < MaxFrames; ++Index)
		{
			Advance(World, 1);
			if (Brain.GetActiveTacticalQueryCount() == 1) return true;
		}
		return false;
	}

	bool RunCapacity(FAutomationTestBase& Test, const int32 UnitCount)
	{
		FCombatAutomationWorldFixture Fixture;
		if (!Fixture.IsValid())
		{
			Test.AddError(TEXT("Capacity fixture could not create a World"));
			return false;
		}
		auto& World = *Fixture.GetWorld();
		if (!World.CreateAISystem())
		{
			Test.AddError(TEXT("Capacity fixture could not create the engine AI/EQS service"));
			return false;
		}
		auto* ProfileData = Profile(&World);
		ProfileData->RootTree = FCombatAIAssetBuilder::BuildTacticalRootTree(ProfileData);
		ProfileData->TacticalLocationQuery = FCombatAIAssetBuilder::BuildTacticalLocationQuery(ProfileData, 600.0f);
		ProfileData->bEnablePerception = true;
		ProfileData->Perception.Radius = 500.0f;
		ProfileData->Perception.ActiveInterval = 0.2f;
		ProfileData->Perception.IdleInterval = 0.8f;
		ProfileData->RepositionTriggerDistance = 300.0f;
		ProfileData->RepositionUtility = 0.8f;
		ProfileData->AttackUtility = 0.0f;
		auto* Budget = World.GetSubsystem<UCombatAIWorldSubsystem>();
		Budget->SetFrameLimitsForTesting(16, 4);
		TArray<UCombatAIBrainComponent*> Brains;
		Brains.Reserve(UnitCount);
		for (int32 Index = 0; Index < UnitCount; ++Index)
		{
			const FVector Origin(static_cast<double>(Index) * 2000.0, 0.0, 0.0);
			auto* Unit = Spawn(World);
			auto* Target = Spawn(World);
			if (!Unit || !Target)
			{
				Test.AddError(FString::Printf(TEXT("Capacity actor spawn failed at %d"), Index));
				return false;
			}
			Unit->SetActorLocation(Origin);
			Target->SetActorLocation(Origin + FVector(150.0f, 0.0f, 0.0f));
			Unit->SetCombatTeamId(FCombatTeamId(1));
			Target->SetCombatTeamId(FCombatTeamId(2));
			Unit->GetCombatOrderComponent()->SetNavigationDeferredForTesting(true);
			FCombatAIAssignment AssignmentData;
			AssignmentData.Home = Origin;
			auto* Brain = Unit->GetCombatAIBrainComponent();
			if (!Brain->SetAssignment(AssignmentData))
			{
				Test.AddError(FString::Printf(TEXT("Capacity assignment failed at %d"), Index));
				return false;
			}
			Brain->ConfigureProfile(ProfileData);
			Brains.Add(Brain);
		}

		uint64 Submitted = 0;
		const int32 MaxFrames = 80 + UnitCount / 2;
		for (int32 Frame = 0; Frame < MaxFrames; ++Frame)
		{
			Advance(World, 1);
			Submitted = 0;
			for (const auto* Brain : Brains) Submitted += Brain->GetSubmittedCount();
			const FCombatAIWorldBudgetSnapshot Snapshot = Budget->GetSnapshot();
			if (Submitted == static_cast<uint64>(UnitCount)
				&& Snapshot.EQSSucceeded == static_cast<uint64>(UnitCount) && Snapshot.ActiveEQS == 0) break;
		}
		const FCombatAIWorldBudgetSnapshot Snapshot = Budget->GetSnapshot();
		Test.TestEqual(TEXT("Every tactical AI receives one EQS grant"), Snapshot.EQSGranted, static_cast<uint64>(UnitCount));
		Test.TestEqual(TEXT("Every tactical EQS completes successfully"), Snapshot.EQSSucceeded, static_cast<uint64>(UnitCount));
		Test.TestEqual(TEXT("Every tactical AI submits one exact Move"), Submitted, static_cast<uint64>(UnitCount));
		Test.TestEqual(TEXT("Capacity run leaves no in-flight EQS"), Snapshot.ActiveEQS, 0);
		Test.TestTrue(TEXT("Capacity pressure produces observable EQS deferral"), Snapshot.EQSDeferred > 0);
		Test.TestTrue(TEXT("Perception grants eventually cover all AI"), Snapshot.PerceptionGranted >= static_cast<uint64>(UnitCount));
		Test.TestTrue(TEXT("EQS percentile metrics remain finite and ordered"),
			FMath::IsFinite(Snapshot.EQSP95Milliseconds) && FMath::IsFinite(Snapshot.EQSP99Milliseconds)
			&& Snapshot.EQSP95Milliseconds >= 0.0f && Snapshot.EQSP99Milliseconds >= Snapshot.EQSP95Milliseconds);
		Test.AddInfo(FString::Printf(TEXT("AITacticsCapacity Units=%d Perception={Requests=%llu Granted=%llu Deferred=%llu} EQS={Requests=%llu Granted=%llu Deferred=%llu Success=%llu Peak=%d P95Ms=%.3f P99Ms=%.3f} Commands=%llu"),
			UnitCount, Snapshot.PerceptionRequests, Snapshot.PerceptionGranted, Snapshot.PerceptionDeferred,
			Snapshot.EQSRequests, Snapshot.EQSGranted, Snapshot.EQSDeferred, Snapshot.EQSSucceeded,
			Snapshot.PeakActiveEQS, Snapshot.EQSP95Milliseconds, Snapshot.EQSP99Milliseconds, Submitted));
		for (auto* Brain : Brains) Brain->SuspendForManualCommand();
		Test.TestEqual(TEXT("Capacity cleanup releases every EQS token"), Budget->GetSnapshot().ActiveEQS, 0);
		for (const auto* Brain : Brains)
		{
			Test.TestEqual(TEXT("Capacity cleanup leaves no unit query"), Brain->GetActiveTacticalQueryCount(), 0);
			Test.TestEqual(TEXT("Capacity cleanup leaves no unit wait"), Brain->GetActiveWaitCount(), 0);
		}
		return !Test.HasAnyErrors();
	}
}

/** Utility 数学必须与实验性 StateTree 适配层分离，并对异常输入给出确定结果。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatAIUtilityScoringTest,
	"Combat.AI.Tactics.UtilityScoring", CombatAITacticsTests::Flags)
bool FCombatAIUtilityScoringTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("Negative score clamps to zero"), FCombatAIUtilityScoring::ClampScore(-2.0f), 0.0f);
	TestEqual(TEXT("Score clamps to one"), FCombatAIUtilityScoring::ClampScore(3.0f), 1.0f);
	TestEqual(TEXT("NaN cannot enter StateTree utility"),
		FCombatAIUtilityScoring::ClampScore(std::numeric_limits<float>::quiet_NaN()), 0.0f);
	TestFalse(TEXT("Minimum hold blocks ordinary switch"),
		FCombatAIUtilityScoring::ShouldSwitch(0.3f, 0.9f, 0.1, 0.2f, 0.05f));
	TestFalse(TEXT("Margin blocks a near tie"),
		FCombatAIUtilityScoring::ShouldSwitch(0.3f, 0.34f, 1.0, 0.2f, 0.05f));
	TestTrue(TEXT("Held action switches when challenger clears margin"),
		FCombatAIUtilityScoring::ShouldSwitch(0.3f, 0.36f, 1.0, 0.2f, 0.05f));
	return true;
}

/** v1 资产继续按阶段 A/B 运行，只有显式 v2 才能打开战术字段。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatAITacticsProfileVersionTest,
	"Combat.AI.Tactics.ProfileVersion", CombatAITacticsTests::Flags)
bool FCombatAITacticsProfileVersionTest::RunTest(const FString& Parameters)
{
	auto* Profile = NewObject<UCombatAIProfileData>(GetTransientPackage());
	Profile->DefinitionName = TEXT("ai_tactics_profile_fixture");
	Profile->RootTree = FCombatAIAssetBuilder::BuildRootTree(
		Profile, FCombatAIAssetBuilder::BuildActionTree(Profile));
	FString Diagnostic;
	TestTrue(TEXT("Legacy v1 remains valid"), Profile->ValidateRuntime(Diagnostic));
	Profile->bEnableTactics = true;
	TestFalse(TEXT("v1 ignores newly serialized tactics flag"), Profile->IsTacticsEnabled());
	TestTrue(TEXT("Ignored v1 tactical fields do not invalidate legacy asset"), Profile->ValidateRuntime(Diagnostic));
	Profile->AIProfileVersion = 2;
	TestTrue(TEXT("Explicit v2 enables tactics"), Profile->IsTacticsEnabled());
	TestTrue(TEXT("Empty v2 tactics profile is valid"), Profile->ValidateRuntime(Diagnostic));
	Profile->TacticalLocationQuery = FCombatAIAssetBuilder::BuildTacticalLocationQuery(Profile, 600.0f);
	TestFalse(TEXT("Tactical query requires a positive trigger distance"), Profile->ValidateRuntime(Diagnostic));
	Profile->RepositionTriggerDistance = 300.0f;
	TestFalse(TEXT("Tactical positioning requires perception"), Profile->ValidateRuntime(Diagnostic));
	Profile->bEnablePerception = true;
	TestTrue(TEXT("Paired tactical query configuration is valid"), Profile->ValidateRuntime(Diagnostic));
	Profile->TacticalQueryRetrySeconds = 0.0f;
	TestFalse(TEXT("Zero query retry would create an unbounded loop"), Profile->ValidateRuntime(Diagnostic));
	Profile->TacticalQueryRetrySeconds = 0.05f;
	Profile->TacticalLocationQuery = nullptr;
	Profile->RepositionTriggerDistance = 0.0f;
	Profile->bEnablePerception = false;
	FCombatAIAbilityUsageRule Rule;
	Rule.AbilityDefinitionId = FPrimaryAssetId(TEXT("CombatAbility"), TEXT("ai_active_fixture"));
	Profile->AbilityUsageRules.Add(Rule);
	TestTrue(TEXT("Unique bounded ability rule is valid"), Profile->ValidateRuntime(Diagnostic));
	Profile->AbilityUsageRules.Add(Rule);
	TestFalse(TEXT("Duplicate ability definition is rejected"), Profile->ValidateRuntime(Diagnostic));
	Profile->AbilityUsageRules.SetNum(1);
	Profile->AbilityUsageRules[0].BaseUtility = std::numeric_limits<float>::infinity();
	TestFalse(TEXT("Non-finite utility is rejected"), Profile->ValidateRuntime(Diagnostic));
	Profile->AbilityUsageRules[0].BaseUtility = 0.5f;
	Profile->AbilityUsageRules[0].TargetPolicy = ECombatAIAbilityTargetPolicy::TacticalLocation;
	TestFalse(TEXT("Reserved tactical-location cast policy is rejected"), Profile->ValidateRuntime(Diagnostic));
	Profile->AbilityUsageRules[0].TargetPolicy = ECombatAIAbilityTargetPolicy::CurrentEnemy;
	Profile->AbilityUsageRules[0].AbilityDefinitionId = FPrimaryAssetId(TEXT("CombatUnit"), TEXT("wrong_type"));
	TestFalse(TEXT("Non-ability identity is rejected"), Profile->ValidateRuntime(Diagnostic));
	Profile->AIProfileVersion = 3;
	TestFalse(TEXT("Unknown profile version is rejected"), Profile->ValidateRuntime(Diagnostic));
	return true;
}

/** 保存后的阶段 C 资产必须形成可冷加载闭包；脚本生成的单位不能依赖编辑器瞬态对象。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatAITacticsAssetReadbackTest,
	"Combat.AI.Tactics.Assets.Readback", CombatAITacticsTests::Flags)
bool FCombatAITacticsAssetReadbackTest::RunTest(const FString& Parameters)
{
	const auto* HeroProfile = LoadObject<UCombatAIProfileData>(nullptr,
		TEXT("/Game/Combat/Demo/AI/Tactics/DA_AI_HeroTactics.DA_AI_HeroTactics"));
	const auto* RangedProfile = LoadObject<UCombatAIProfileData>(nullptr,
		TEXT("/Game/Combat/Demo/AI/Tactics/DA_AI_RangedGuard.DA_AI_RangedGuard"));
	const auto* HeroSet = LoadObject<UCombatAbilitySet>(nullptr,
		TEXT("/Game/Combat/Demo/AI/Tactics/DA_AI_HeroAbilitySet.DA_AI_HeroAbilitySet"));
	const auto* HeroUnit = LoadObject<UCombatUnitData>(nullptr,
		TEXT("/Game/Combat/Demo/AI/Tactics/DA_AI_HeroUnit.DA_AI_HeroUnit"));
	const auto* RangedUnit = LoadObject<UCombatUnitData>(nullptr,
		TEXT("/Game/Combat/Demo/AI/Tactics/DA_AI_RangedUnit.DA_AI_RangedUnit"));
	const auto* HeroClass = LoadClass<ACombatUnitCharacter>(nullptr,
		TEXT("/Game/Combat/Demo/AI/Tactics/BP_AI_HeroUnit.BP_AI_HeroUnit_C"));
	const auto* RangedClass = LoadClass<ACombatUnitCharacter>(nullptr,
		TEXT("/Game/Combat/Demo/AI/Tactics/BP_AI_RangedUnit.BP_AI_RangedUnit_C"));
	const auto* ArenaClass = LoadClass<AActor>(nullptr,
		TEXT("/Game/Combat/Demo/AI/Tactics/BP_AI_TacticsArena.BP_AI_TacticsArena_C"));

	TestNotNull(TEXT("Hero tactical profile cold-loads"), HeroProfile);
	TestNotNull(TEXT("Ranged tactical profile cold-loads"), RangedProfile);
	TestNotNull(TEXT("Hero ability set cold-loads"), HeroSet);
	TestNotNull(TEXT("Hero unit data cold-loads"), HeroUnit);
	TestNotNull(TEXT("Ranged unit data cold-loads"), RangedUnit);
	TestNotNull(TEXT("Hero unit Blueprint cold-loads"), HeroClass);
	TestNotNull(TEXT("Ranged unit Blueprint cold-loads"), RangedClass);
	TestNotNull(TEXT("Tactics arena Blueprint cold-loads"), ArenaClass);
	TestTrue(TEXT("Tactics map package exists"),
		FPackageName::DoesPackageExist(TEXT("/Game/Combat/Demo/AI/Tactics/L_CombatAI_Tactics")));

	if (!HeroProfile || !RangedProfile || !HeroSet || !HeroUnit || !RangedUnit)
	{
		return false;
	}
	FString Diagnostic;
	TestTrue(TEXT("Hero profile remains runtime-valid"), HeroProfile->ValidateRuntime(Diagnostic));
	TestTrue(TEXT("Ranged profile remains runtime-valid"), RangedProfile->ValidateRuntime(Diagnostic));
	TestEqual(TEXT("Hero profile has one explicit active-skill rule"), HeroProfile->AbilityUsageRules.Num(), 1);
	if (HeroProfile->AbilityUsageRules.Num() == 1)
	{
		const FCombatAIAbilityUsageRule& Rule = HeroProfile->AbilityUsageRules[0];
		TestTrue(TEXT("Hero rule targets the saved heal definition"),
			Rule.AbilityDefinitionId == FPrimaryAssetId(TEXT("CombatAbility"), TEXT("indicator_heal")));
		TestEqual(TEXT("Hero heal uses the self target policy"), Rule.TargetPolicy, ECombatAIAbilityTargetPolicy::Self);
	}
	TestTrue(TEXT("Hero unit references its tactical profile"), HeroUnit->AIProfile == HeroProfile);
	TestEqual(TEXT("Hero unit has one authored ability set"), HeroUnit->AbilitySets.Num(), 1);
	if (HeroUnit->AbilitySets.Num() == 1)
	{
		TestTrue(TEXT("Hero unit references the tactical ability set"), HeroUnit->AbilitySets[0].LoadSynchronous() == HeroSet);
	}
	TestTrue(TEXT("Hero ability set grants the active heal used by the rule"),
		HeroSet->Abilities.ContainsByPredicate([](const FCombatAbilitySetEntry& Entry)
		{
			const auto* Ability = Entry.AbilityClass ? Entry.AbilityClass->GetDefaultObject<UCombatGameplayAbility>() : nullptr;
			return Ability && Ability->GetAbilityData()
				&& Ability->GetAbilityData()->GetPrimaryAssetId()
					== FPrimaryAssetId(TEXT("CombatAbility"), TEXT("indicator_heal"));
		}));
	TestTrue(TEXT("Ranged unit references its tactical profile"), RangedUnit->AIProfile == RangedProfile);
	TestNotNull(TEXT("Ranged profile keeps its tactical EQS"), RangedProfile->TacticalLocationQuery.Get());
	int32 RoleConditionCount = 0;
	int32 RolePrepareCount = 0;
	for (int32 Index = 0; Index < HeroProfile->RootTree->GetNodes().Num(); ++Index)
	{
		const UScriptStruct* Type = HeroProfile->RootTree->GetNode(Index).GetScriptStruct();
		RoleConditionCount += Type && Type->IsChildOf(FCombatAIRoleCondition::StaticStruct()) ? 1 : 0;
		RolePrepareCount += Type && Type->IsChildOf(FCombatAIPrepareRoleTask::StaticStruct()) ? 1 : 0;
	}
	TestTrue(TEXT("Saved tactical tree includes hard role conditions"), RoleConditionCount >= 3);
	TestTrue(TEXT("Saved tactical tree includes the hard return command chain"), RolePrepareCount >= 1);
	return !HasAnyErrors();
}

/** 候选评估只调用公共预检；不会用真实激活探测技能，也不会把被动或未授予规则变成 Cast。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatAIAbilityCandidateTest,
	"Combat.AI.Tactics.AbilityCandidateReadOnly", CombatAITacticsTests::Flags)
bool FCombatAIAbilityCandidateTest::RunTest(const FString& Parameters)
{
	FCombatAutomationWorldFixture Fixture;
	if (!Fixture.IsValid()) return false;
	auto& World = *Fixture.GetWorld();
	auto* Unit = CombatAITacticsTests::Spawn(World);
	auto* Enemy = CombatAITacticsTests::Spawn(World);
	Unit->SetCombatTeamId(FCombatTeamId(1));
	Enemy->SetCombatTeamId(FCombatTeamId(2));
	Enemy->SetActorLocation(FVector(200.0f, 0.0f, 0.0f));
	auto* AbilityData = NewObject<UCombatAbilityData>(Unit);
	AbilityData->DefinitionName = TEXT("ai_tactical_active");
	AbilityData->BehaviorTags.AddTag(CombatTags::Ability_Behavior_NoTarget);
	AbilityData->TargetingRules.TargetTeamTag = CombatTags::TargetTeam_None;
	AbilityData->TargetingRules.CastRange = 1000.0f;
	TGuardValue<TObjectPtr<UCombatAbilityData>> Restore(
		GetMutableDefault<UCombatSelfHealAbility>()->AbilityData, AbilityData);
	FGameplayAbilitySpecHandle Handle;
	FGameplayTag Failure;
	TestTrue(TEXT("Grant active fixture ability"), Unit->GetCombatAbilitySystemComponent()->GrantCombatAbility(
		UCombatSelfHealAbility::StaticClass(), 1, false, Handle, Failure));

	auto* Profile = CombatAITacticsTests::Profile(Unit);
	Profile->bEnablePerception = true;
	Profile->Perception.ActiveInterval = Profile->Perception.IdleInterval = 0.05f;
	FCombatAIAbilityUsageRule Rule;
	Rule.AbilityDefinitionId = AbilityData->GetPrimaryAssetId();
	Rule.TargetPolicy = ECombatAIAbilityTargetPolicy::Self;
	Rule.BaseUtility = 0.8f;
	Profile->AbilityUsageRules.Add(Rule);
	auto* Brain = Unit->GetCombatAIBrainComponent();
	Brain->ConfigureProfile(Profile);
	CombatAITacticsTests::Advance(World, 20);
	TestEqual(TEXT("Fixture observes one enemy for target-invalid coverage"), Brain->GetKnowledge().Candidates.Num(), 1);
	auto* AbilitySystem = Unit->GetCombatAbilitySystemComponent();
	const auto AssertReadOnlyRejection = [&](const FString& Label)
	{
		const float ManaBefore = AbilitySystem->GetNumericAttribute(UCombatAttributeSet::GetManaAttribute());
		const float CooldownBefore = AbilitySystem->GetCombatAbilityCooldownRemaining(Handle);
		const uint64 SubmittedBefore = Brain->GetSubmittedCount();
		TestFalse(Label, Brain->EvaluateTacticalCandidates());
		const FGameplayAbilitySpec* RejectedSpec = AbilitySystem->FindAbilitySpecFromHandle(Handle);
		TestTrue(Label + TEXT(" leaves ability inactive"), RejectedSpec && !RejectedSpec->IsActive());
		TestTrue(Label + TEXT(" leaves mana unchanged"), FMath::IsNearlyEqual(
			AbilitySystem->GetNumericAttribute(UCombatAttributeSet::GetManaAttribute()), ManaBefore));
		TestTrue(Label + TEXT(" does not start or replace cooldown"), FMath::IsNearlyEqual(
			AbilitySystem->GetCombatAbilityCooldownRemaining(Handle), CooldownBefore));
		TestEqual(Label + TEXT(" submits no order"), Brain->GetSubmittedCount(), SubmittedBefore);
	};
	TestTrue(TEXT("Active granted ability produces a candidate"), Brain->EvaluateTacticalCandidates());
	const FCombatAIAbilityCandidate& Candidate = Brain->GetTacticalSnapshot().BestAbility;
	TestTrue(TEXT("Candidate carries granted spec"), Candidate.bValid && Candidate.SpecHandle == Handle);
	TestEqual(TEXT("No-target definition produces no-target order"), Candidate.OrderType, ECombatOrderType::CastNoTarget);
	const FGameplayAbilitySpec* Spec = Unit->GetCombatAbilitySystemComponent()->FindAbilitySpecFromHandle(Handle);
	TestTrue(TEXT("Candidate evaluation leaves ability inactive"), Spec && !Spec->IsActive());
	TestEqual(TEXT("Candidate evaluation submits no order"), Brain->GetSubmittedCount(), uint64(0));
	TestEqual(TEXT("Candidate evaluation does not start cooldown"),
		AbilitySystem->GetCombatAbilityCooldownRemaining(Handle), 0.0f);

	AbilityData->BehaviorTags.AddTag(CombatTags::Ability_Behavior_Passive);
	AssertReadOnlyRejection(TEXT("Passive ability is not a cast candidate"));
	AbilityData->BehaviorTags.RemoveTag(CombatTags::Ability_Behavior_Passive);
	AbilityData->BehaviorTags.AddTag(CombatTags::Ability_Behavior_AutoCast);
	AssertReadOnlyRejection(TEXT("AutoCast ability is not an active cast candidate"));
	AbilityData->BehaviorTags.RemoveTag(CombatTags::Ability_Behavior_AutoCast);
	AbilityData->BehaviorTags.AddTag(CombatTags::Ability_Behavior_Attack);
	AssertReadOnlyRejection(TEXT("Attack modifier ability is not a cast candidate"));
	AbilityData->BehaviorTags.RemoveTag(CombatTags::Ability_Behavior_Attack);

	Profile->AbilityUsageRules[0].IntentRole = ECombatAIAbilityIntentRole::Heal;
	AbilitySystem->SetNumericAttributeBase(UCombatAttributeSet::GetHealthAttribute(),
		AbilitySystem->GetNumericAttribute(UCombatAttributeSet::GetMaxHealthAttribute()));
	AssertReadOnlyRejection(TEXT("Full-health Heal rule is not a cast candidate"));
	AbilitySystem->SetNumericAttributeBase(UCombatAttributeSet::GetHealthAttribute(),
		AbilitySystem->GetNumericAttribute(UCombatAttributeSet::GetMaxHealthAttribute()) * 0.5f);
	TestTrue(TEXT("Injured unit enables the same Heal rule"), Brain->EvaluateTacticalCandidates());
	Profile->AbilityUsageRules[0].IntentRole = ECombatAIAbilityIntentRole::Defense;
	AbilitySystem->SetNumericAttributeBase(UCombatAttributeSet::GetHealthAttribute(),
		AbilitySystem->GetNumericAttribute(UCombatAttributeSet::GetMaxHealthAttribute()));
	TestTrue(TEXT("Full health does not suppress a Defense rule"), Brain->EvaluateTacticalCandidates());
	Profile->AbilityUsageRules[0].IntentRole = ECombatAIAbilityIntentRole::SingleTargetDamage;

	AbilityData->SpecialValues.FindOrAdd(TEXT("mana_cost")).Values = { 50.0f };
	AbilitySystem->SetNumericAttributeBase(UCombatAttributeSet::GetManaAttribute(), 0.0f);
	AssertReadOnlyRejection(TEXT("Insufficient resource rejects the candidate"));
	AbilitySystem->SetNumericAttributeBase(UCombatAttributeSet::GetManaAttribute(), 100.0f);
	AbilityData->SpecialValues.FindOrAdd(TEXT("mana_cost")).Values = { 0.0f };

	AbilitySystem->AddLooseGameplayTag(CombatTags::State_Silenced);
	AssertReadOnlyRejection(TEXT("Silence rejects the candidate during public preflight"));
	AbilitySystem->RemoveLooseGameplayTag(CombatTags::State_Silenced);

	AbilityData->BehaviorTags.RemoveTag(CombatTags::Ability_Behavior_NoTarget);
	AbilityData->BehaviorTags.AddTag(CombatTags::Ability_Behavior_UnitTarget);
	AbilityData->TargetingRules.TargetTeamTag = CombatTags::TargetTeam_Enemy;
	Profile->AbilityUsageRules[0].TargetPolicy = ECombatAIAbilityTargetPolicy::CurrentEnemy;
	TestTrue(TEXT("Observed valid unit target produces a candidate"), Brain->EvaluateTacticalCandidates());
	Enemy->GetCombatAbilitySystemComponent()->AddLooseGameplayTag(CombatTags::State_Untargetable);
	AssertReadOnlyRejection(TEXT("Target invalidation rejects the candidate"));
	Enemy->GetCombatAbilitySystemComponent()->RemoveLooseGameplayTag(CombatTags::State_Untargetable);

	AbilityData->BehaviorTags.RemoveTag(CombatTags::Ability_Behavior_UnitTarget);
	AbilityData->BehaviorTags.AddTag(CombatTags::Ability_Behavior_NoTarget);
	AbilityData->TargetingRules.TargetTeamTag = CombatTags::TargetTeam_None;
	Profile->AbilityUsageRules[0].TargetPolicy = ECombatAIAbilityTargetPolicy::Self;
	AbilityData->SpecialValues.FindOrAdd(TEXT("cooldown")).Values = { 5.0f };
	bool bCostCommitted = false;
	bool bCooldownCommitted = false;
	TestTrue(TEXT("Fixture establishes cooldown before read-only evaluation"), AbilitySystem->CommitCombatAbilityStage(
		Handle, *AbilityData, 1, ECombatAbilityCommitStage::SpellStarted,
		bCostCommitted, bCooldownCommitted, Failure));
	TestTrue(TEXT("Fixture cooldown is active"), AbilitySystem->GetCombatAbilityCooldownRemaining(Handle) > 0.0f);
	AssertReadOnlyRejection(TEXT("Cooldown rejects the candidate"));

	Profile->AbilityUsageRules[0].AbilityDefinitionId = FPrimaryAssetId(TEXT("CombatAbility"), TEXT("not_granted"));
	AssertReadOnlyRejection(TEXT("Ungranted definition is ignored"));
	TestEqual(TEXT("Rejected candidates still submit no order"), Brain->GetSubmittedCount(), uint64(0));
	return true;
}

/** World 配额在扫描或查询启动前拒绝超额工作，并保持可观测的在途/延期统计。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatAIWorldBudgetTest,
	"Combat.AI.Tactics.WorldBudget", CombatAITacticsTests::Flags)
bool FCombatAIWorldBudgetTest::RunTest(const FString& Parameters)
{
	FCombatAutomationWorldFixture Fixture;
	if (!Fixture.IsValid()) return false;
	auto* Budget = Fixture.GetWorld()->GetSubsystem<UCombatAIWorldSubsystem>();
	TestNotNull(TEXT("World owns one AI budget subsystem"), Budget);
	Budget->SetFrameLimitsForTesting(1, 1);
	TestTrue(TEXT("First perception scan gets frame budget"), Budget->TryAcquirePerception());
	TestFalse(TEXT("Second perception scan is deferred"), Budget->TryAcquirePerception());
	TestTrue(TEXT("First EQS start gets frame budget"), Budget->TryStartEQS(101));
	TestFalse(TEXT("Second EQS start is deferred"), Budget->TryStartEQS(102));
	const FCombatAIWorldBudgetSnapshot First = Budget->GetSnapshot();
	TestEqual(TEXT("Perception requests counted"), First.PerceptionRequests, uint64(2));
	TestEqual(TEXT("Perception deferral counted"), First.PerceptionDeferred, uint64(1));
	TestEqual(TEXT("One active EQS"), First.ActiveEQS, 1);
	TestEqual(TEXT("EQS peak tracked"), First.PeakActiveEQS, 1);
	TestTrue(TEXT("First EQS finish consumes its exact token"),
		Budget->FinishEQS(101, ECombatAIEQSResult::Succeeded, 4.0));
	TestFalse(TEXT("Duplicate EQS finish cannot consume the token twice"),
		Budget->FinishEQS(101, ECombatAIEQSResult::Failed, 5.0));
	TestEqual(TEXT("EQS finish clears active identity"), Budget->GetSnapshot().ActiveEQS, 0);
	TestEqual(TEXT("Duplicate finish does not add a second terminal count"),
		Budget->GetSnapshot().EQSSucceeded + Budget->GetSnapshot().EQSFailed, uint64(1));
	Fixture.GetWorld()->Tick(LEVELTICK_All, 0.05f);
	TestTrue(TEXT("Next world time slice replenishes perception"), Budget->TryAcquirePerception());
	const float DelayA = UCombatAIWorldSubsystem::ComputeStableInitialDelay(77, 0.8f);
	const float DelayB = UCombatAIWorldSubsystem::ComputeStableInitialDelay(77, 0.8f);
	TestTrue(TEXT("Stable delay is deterministic and bounded"), DelayA == DelayB && DelayA >= 0.0f && DelayA < 0.8f);
	return true;
}

/** v2 感知从稳定错峰开始，并在 World 配额拒绝时延期而不是先扫描再记账。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatAIPerceptionBudgetIntegrationTest,
	"Combat.AI.Tactics.PerceptionBudgetIntegration", CombatAITacticsTests::Flags)
bool FCombatAIPerceptionBudgetIntegrationTest::RunTest(const FString& Parameters)
{
	FCombatAutomationWorldFixture Fixture;
	if (!Fixture.IsValid()) return false;
	auto& World = *Fixture.GetWorld();
	auto* Unit = CombatAITacticsTests::Spawn(World);
	auto* Enemy = CombatAITacticsTests::Spawn(World);
	Unit->SetCombatTeamId(FCombatTeamId(1));
	Enemy->SetCombatTeamId(FCombatTeamId(2));
	Enemy->SetActorLocation(FVector(200.0f, 0.0f, 0.0f));
	auto* Budget = World.GetSubsystem<UCombatAIWorldSubsystem>();
	Budget->SetFrameLimitsForTesting(0, 1);
	auto* Profile = CombatAITacticsTests::Profile(Unit);
	Profile->bEnablePerception = true;
	Profile->Perception.IdleInterval = 0.05f;
	Profile->Perception.ActiveInterval = 0.05f;
	Profile->PerceptionBudgetRetrySeconds = 0.05f;
	auto* Brain = Unit->GetCombatAIBrainComponent();
	Brain->ConfigureProfile(Profile);
	TestEqual(TEXT("v2 startup does not synchronously scan"), Brain->GetKnowledge().Candidates.Num(), 0);
	CombatAITacticsTests::Advance(World, 4);
	TestTrue(TEXT("Denied scan is recorded before work"), Budget->GetSnapshot().PerceptionDeferred > 0);
	TestEqual(TEXT("Denied scan publishes no omniscient candidate"), Brain->GetKnowledge().Candidates.Num(), 0);
	Budget->SetFrameLimitsForTesting(1, 1);
	CombatAITacticsTests::Advance(World, 12);
	TestEqual(TEXT("Later budget opportunity publishes candidate"), Brain->GetKnowledge().Candidates.Num(), 1);
	return true;
}

/** 阶段 B 的 v1 Profile 不进入阶段 C 的 World 预算路径，首次和周期感知保持原有时序。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatAILegacyPerceptionCompatibilityTest,
	"Combat.AI.Tactics.LegacyPerceptionCompatibility", CombatAITacticsTests::Flags)
bool FCombatAILegacyPerceptionCompatibilityTest::RunTest(const FString& Parameters)
{
	FCombatAutomationWorldFixture Fixture;
	if (!Fixture.IsValid()) return false;
	auto& World = *Fixture.GetWorld();
	auto* Unit = CombatAITacticsTests::Spawn(World);
	auto* Enemy = CombatAITacticsTests::Spawn(World);
	Unit->SetCombatTeamId(FCombatTeamId(1));
	Enemy->SetCombatTeamId(FCombatTeamId(2));
	Enemy->SetActorLocation(FVector(200.0f, 0.0f, 0.0f));
	auto* Budget = World.GetSubsystem<UCombatAIWorldSubsystem>();
	Budget->SetFrameLimitsForTesting(0, 0);
	auto* Profile = CombatAITacticsTests::Profile(Unit);
	Profile->AIProfileVersion = 1;
	Profile->bEnableTactics = false;
	Profile->bEnablePerception = true;
	Profile->Perception.ActiveInterval = Profile->Perception.IdleInterval = 0.05f;
	auto* Brain = Unit->GetCombatAIBrainComponent();
	Brain->ConfigureProfile(Profile);
	TestEqual(TEXT("v1 startup publishes its first perception synchronously"), Brain->GetKnowledge().Candidates.Num(), 1);
	TestEqual(TEXT("v1 startup does not consume the v2 perception budget"), Budget->GetSnapshot().PerceptionRequests, uint64(0));
	const uint64 InitialRevision = Brain->GetKnowledge().Revision;
	CombatAITacticsTests::Advance(World, 4);
	TestTrue(TEXT("v1 periodic perception continues while the v2 budget is closed"),
		Brain->GetKnowledge().Revision > InitialRevision);
	TestEqual(TEXT("v1 periodic perception has no v2 budget deferral"), Budget->GetSnapshot().PerceptionDeferred, uint64(0));
	return true;
}

/** Tactical 根树先处理 Blocked/Retry/Return，只有硬职责均不成立时才进入 Utility。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatAITacticalHardPriorityTest,
	"Combat.AI.Tactics.HardPriority", CombatAITacticsTests::Flags)
bool FCombatAITacticalHardPriorityTest::RunTest(const FString& Parameters)
{
	FCombatAutomationWorldFixture Fixture;
	if (!Fixture.IsValid()) return false;
	auto& World = *Fixture.GetWorld();
	auto* Unit = CombatAITacticsTests::Spawn(World);
	Unit->SetActorLocation(FVector(1000.0f, 0.0f, 0.0f));
	auto* Profile = CombatAITacticsTests::Profile(Unit);
	Profile->RootTree = FCombatAIAssetBuilder::BuildTacticalRootTree(Profile);
	Profile->bEnablePerception = true;
	Profile->LeashDistance = 100.0f;
	Profile->MaxAttempts = 1;
	Profile->RetryDelay = 0.05f;
	auto* Brain = Unit->GetCombatAIBrainComponent();
	FCombatAIAssignment Assignment;
	Assignment.Home = FVector::ZeroVector;
	TestTrue(TEXT("Hard-priority fixture owns a distant home"), Brain->SetAssignment(Assignment));
	auto* Orders = Unit->GetCombatOrderComponent();
	Orders->SetNavigationDeferredForTesting(true);
	Orders->MaxMoveRetries = 0;
	Brain->ConfigureProfile(Profile);
	for (int32 Index = 0; Index < 8 && !Orders->GetCurrentOrderHandle().IsValid(); ++Index)
	{
		CombatAITacticsTests::Advance(World, 1);
	}
	TestTrue(TEXT("Hard Return submits before any Utility evaluation"), Orders->GetCurrentOrderHandle().IsValid());
	TestEqual(TEXT("Hard Return is the only submitted command"), Brain->GetSubmittedCount(), uint64(1));
	TestEqual(TEXT("Utility snapshot remains untouched while Return is required"),
		Brain->GetTacticalSnapshot().Revision, uint64(0));
	TestTrue(TEXT("Fixture injects the terminal Return failure"),
		Orders->CompleteMovementForTesting(Orders->GetCurrentOrderHandle(), false));
	for (int32 Index = 0; Index < 8 && !Brain->IsRetryBlocked(); ++Index)
	{
		CombatAITacticsTests::Advance(World, 1);
	}
	TestTrue(TEXT("Failed Return reaches the bounded blocked state"), Brain->IsRetryBlocked());
	TestEqual(TEXT("Blocked state does not fall through to Utility"), Brain->GetTacticalSnapshot().Revision, uint64(0));
	TestEqual(TEXT("Blocked state does not resubmit the failed command"), Brain->GetSubmittedCount(), uint64(1));
	CombatAITacticsTests::Advance(World, 8);
	TestEqual(TEXT("Ordinary observation cannot bypass the blocked state"), Brain->GetSubmittedCount(), uint64(1));

	FCombatAIAssignment Replacement;
	Replacement.Home = Unit->GetActorLocation();
	TestTrue(TEXT("A new assignment releases the hard blocked state"), Brain->SetAssignment(Replacement));
	CombatAITacticsTests::Advance(World, 4);
	TestTrue(TEXT("Utility becomes reachable after hard facts clear"), Brain->GetTacticalSnapshot().Revision > 0);
	return true;
}

/** 真实 StateTree 最高效用选择只比较通过条件的分支，并由选中分支走公共 Cast Order。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatAIUtilityStateTreeTest,
	"Combat.AI.Tactics.UtilityStateTree", CombatAITacticsTests::Flags)
bool FCombatAIUtilityStateTreeTest::RunTest(const FString& Parameters)
{
	FCombatAutomationWorldFixture Fixture;
	if (!Fixture.IsValid()) return false;
	auto& World = *Fixture.GetWorld();
	auto* Unit = CombatAITacticsTests::Spawn(World);
	auto* AbilityData = NewObject<UCombatAbilityData>(Unit);
	AbilityData->DefinitionName = TEXT("ai_utility_cast");
	AbilityData->BehaviorTags.AddTag(CombatTags::Ability_Behavior_NoTarget);
	AbilityData->TargetingRules.TargetTeamTag = CombatTags::TargetTeam_None;
	AbilityData->CastPoint = 0.5f;
	AbilityData->SpecialValues.FindOrAdd(TEXT("cooldown")).Values = { 5.0f };
	TGuardValue<TObjectPtr<UCombatAbilityData>> Restore(
		GetMutableDefault<UCombatSelfHealAbility>()->AbilityData, AbilityData);
	FGameplayAbilitySpecHandle Handle;
	FGameplayTag Failure;
	TestTrue(TEXT("Grant utility fixture ability"), Unit->GetCombatAbilitySystemComponent()->GrantCombatAbility(
		UCombatSelfHealAbility::StaticClass(), 1, false, Handle, Failure));

	auto* Profile = CombatAITacticsTests::Profile(Unit);
	Profile->RootTree = FCombatAIAssetBuilder::BuildTacticalRootTree(Profile);
	Profile->bEnablePerception = true;
	FCombatAIAbilityUsageRule Rule;
	Rule.AbilityDefinitionId = AbilityData->GetPrimaryAssetId();
	Rule.TargetPolicy = ECombatAIAbilityTargetPolicy::Self;
	Rule.BaseUtility = 0.9f;
	Profile->AbilityUsageRules.Add(Rule);
	TestNotNull(TEXT("Utility tree compiles with project consideration"), Profile->RootTree.Get());
	FString Diagnostic;
	TestTrue(TEXT("Utility tree passes AI asset contract"), Profile->ValidateRuntime(Diagnostic));
	auto* Brain = Unit->GetCombatAIBrainComponent();
	TestTrue(TEXT("Tactical role owns a spatial assignment"), Brain->SetAssignment({}));
	Brain->ConfigureProfile(Profile);
	TestTrue(TEXT("Utility StateTree starts"), Brain->IsRunning());
	for (int32 Index = 0; Index < 8 && Brain->GetTacticalSnapshot().Revision == 0; ++Index)
	{
		CombatAITacticsTests::Advance(World, 1);
	}
	TestTrue(TEXT("Evaluate task publishes a snapshot"), Brain->GetTacticalSnapshot().Revision > 0);
	TestTrue(TEXT("Published snapshot contains legal cast"), Brain->GetTacticalSnapshot().BestAbility.bValid);
	CombatAITacticsTests::Advance(World, 12);
	TestEqual(TEXT("Highest legal utility submits one cast"), Brain->GetSubmittedCount(), uint64(1));
	TestTrue(TEXT("Cast completion reaches shared receipt"), Brain->GetLastReceipt().Result.bSuccess
		&& Brain->GetLastReceipt().Result.Type == ECombatOrderType::CastNoTarget);
	return true;
}

/** 持续攻击出现高效用技能后只在真实发射边界切换，旧 Attack 与新 Cast 各有一个终态。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatAITacticalBoundarySwitchTest,
	"Combat.AI.Tactics.AttackBoundarySwitch", CombatAITacticsTests::Flags)
bool FCombatAITacticalBoundarySwitchTest::RunTest(const FString& Parameters)
{
	FCombatAutomationWorldFixture Fixture;
	if (!Fixture.IsValid()) return false;
	auto& World = *Fixture.GetWorld();
	auto* Unit = CombatAITacticsTests::Spawn(World);
	auto* Target = CombatAITacticsTests::Spawn(World);
	Unit->SetCombatTeamId(FCombatTeamId(1));
	Target->SetCombatTeamId(FCombatTeamId(2));
	Target->SetActorLocation(FVector(150.0f, 0.0f, 0.0f));
	Target->GetCombatAbilitySystemComponent()->SetNumericAttributeBase(
		UCombatAttributeSet::GetMaxHealthAttribute(), 100000.0f);
	Target->GetCombatAbilitySystemComponent()->SetNumericAttributeBase(
		UCombatAttributeSet::GetHealthAttribute(), 100000.0f);
	auto* AbilityData = NewObject<UCombatAbilityData>(Unit);
	AbilityData->DefinitionName = TEXT("ai_tactical_boundary_cast");
	AbilityData->BehaviorTags.AddTag(CombatTags::Ability_Behavior_NoTarget);
	AbilityData->TargetingRules.TargetTeamTag = CombatTags::TargetTeam_None;
	AbilityData->SpecialValues.FindOrAdd(TEXT("cooldown")).Values = { 5.0f };
	TGuardValue<TObjectPtr<UCombatAbilityData>> Restore(
		GetMutableDefault<UCombatSelfHealAbility>()->AbilityData, AbilityData);
	FGameplayAbilitySpecHandle Handle;
	FGameplayTag Failure;
	TestTrue(TEXT("Grant boundary fixture ability"), Unit->GetCombatAbilitySystemComponent()->GrantCombatAbility(
		UCombatSelfHealAbility::StaticClass(), 1, false, Handle, Failure));

	auto* Profile = CombatAITacticsTests::Profile(Unit);
	Profile->RootTree = FCombatAIAssetBuilder::BuildTacticalRootTree(Profile);
	Profile->bEnablePerception = true;
	Profile->Perception.ActiveInterval = 0.05f;
	Profile->Perception.IdleInterval = 0.05f;
	Profile->ActionMinHoldSeconds = 0.0f;
	Profile->BoundaryHoldSeconds = 0.5f;
	auto* Brain = Unit->GetCombatAIBrainComponent();
	auto* Orders = Unit->GetCombatOrderComponent();
	TestTrue(TEXT("Tactical attacker owns a spatial assignment"), Brain->SetAssignment({}));
	int32 Launched = 0;
	int32 CastFinished = 0;
	const auto LaunchBinding = Unit->GetCombatAttackComponent()->OnAttackLaunched().AddLambda(
		[&](FCombatAttackHandle, FCombatOrderHandle) { ++Launched; });
	const auto FinishBinding = Orders->OnOrderFinished().AddLambda([&](const FCombatOrderResult& Result)
	{
		if (Result.bSuccess && Result.Type == ECombatOrderType::CastNoTarget) ++CastFinished;
	});
	Brain->ConfigureProfile(Profile);
	for (int32 Index = 0; Index < 12 && !Orders->GetCurrentOrderHandle().IsValid(); ++Index)
	{
		CombatAITacticsTests::Advance(World, 1);
	}
	const FCombatOrderHandle AttackOrder = Orders->GetCurrentOrderHandle();
	TestTrue(TEXT("Utility starts with a real continuous attack"), AttackOrder.IsValid());

	FCombatAIAbilityUsageRule Rule;
	Rule.AbilityDefinitionId = AbilityData->GetPrimaryAssetId();
	Rule.TargetPolicy = ECombatAIAbilityTargetPolicy::Self;
	Rule.BaseUtility = 0.9f;
	Profile->AbilityUsageRules.Add(Rule);
	CombatAITacticsTests::Advance(World, 30);
	TestEqual(TEXT("Only one cast is submitted after the protected attack boundary"), CastFinished, 1);
	TestTrue(TEXT("The protected attack launches before tactical replacement"), Launched >= 1);
	TestTrue(TEXT("Attack and cast terminal receipts are both resolved"), Brain->GetResolvedCount() >= uint64(2));
	TestTrue(TEXT("The original attack order is no longer active"), Orders->GetCurrentOrderHandle() != AttackOrder);
	Orders->OnOrderFinished().Remove(FinishBinding);
	Unit->GetCombatAttackComponent()->OnAttackLaunched().Remove(LaunchBinding);
	return true;
}

/** Ready 后候选跌破分差或不再允许边界切换时，释放同一票据并保留原 Attack。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatAITacticalBoundaryKeepTest,
	"Combat.AI.Tactics.AttackBoundaryKeep", CombatAITacticsTests::Flags)
bool FCombatAITacticalBoundaryKeepTest::RunTest(const FString& Parameters)
{
	FCombatAutomationWorldFixture Fixture;
	if (!Fixture.IsValid()) return false;
	auto& World = *Fixture.GetWorld();
	auto* Unit = CombatAITacticsTests::Spawn(World);
	auto* Target = CombatAITacticsTests::Spawn(World);
	Unit->SetCombatTeamId(FCombatTeamId(1));
	Target->SetCombatTeamId(FCombatTeamId(2));
	Target->SetActorLocation(FVector(150.0f, 0.0f, 0.0f));
	Target->GetCombatAbilitySystemComponent()->SetNumericAttributeBase(
		UCombatAttributeSet::GetMaxHealthAttribute(), 100000.0f);
	Target->GetCombatAbilitySystemComponent()->SetNumericAttributeBase(
		UCombatAttributeSet::GetHealthAttribute(), 100000.0f);
	auto* AbilityData = NewObject<UCombatAbilityData>(Unit);
	AbilityData->DefinitionName = TEXT("ai_tactical_keep_cast");
	AbilityData->BehaviorTags.AddTag(CombatTags::Ability_Behavior_NoTarget);
	AbilityData->TargetingRules.TargetTeamTag = CombatTags::TargetTeam_None;
	TGuardValue<TObjectPtr<UCombatAbilityData>> Restore(
		GetMutableDefault<UCombatSelfHealAbility>()->AbilityData, AbilityData);
	FGameplayAbilitySpecHandle Handle;
	FGameplayTag Failure;
	TestTrue(TEXT("Grant keep fixture ability"), Unit->GetCombatAbilitySystemComponent()->GrantCombatAbility(
		UCombatSelfHealAbility::StaticClass(), 1, false, Handle, Failure));

	auto* Profile = CombatAITacticsTests::Profile(Unit);
	Profile->RootTree = FCombatAIAssetBuilder::BuildTacticalRootTree(Profile);
	Profile->bEnablePerception = true;
	Profile->Perception.ActiveInterval = 0.05f;
	Profile->Perception.IdleInterval = 0.05f;
	Profile->ActionMinHoldSeconds = 0.0f;
	Profile->BoundaryHoldSeconds = 0.5f;
	auto* Brain = Unit->GetCombatAIBrainComponent();
	auto* Orders = Unit->GetCombatOrderComponent();
	TestTrue(TEXT("Tactical keeper owns a spatial assignment"), Brain->SetAssignment({}));
	int32 ReadyCount = 0;
	int32 CastFinished = 0;
	const auto ReadyBinding = Orders->OnExecutionBoundaryReady().AddLambda(
		[&](FCombatExecutionBoundaryTicket)
		{
			++ReadyCount;
			if (Profile->AbilityUsageRules.IsEmpty()) return;
			if (ReadyCount == 1) Profile->AbilityUsageRules[0].BaseUtility = 0.1f;
			else if (ReadyCount == 2) Profile->AbilityUsageRules[0].InterruptPreference = ECombatAIInterruptPreference::Never;
		});
	const auto FinishBinding = Orders->OnOrderFinished().AddLambda([&](const FCombatOrderResult& Result)
	{
		if (Result.Type == ECombatOrderType::CastNoTarget) ++CastFinished;
	});
	Brain->ConfigureProfile(Profile);
	for (int32 Index = 0; Index < 12 && !Orders->GetCurrentOrderHandle().IsValid(); ++Index)
	{
		CombatAITacticsTests::Advance(World, 1);
	}
	const FCombatOrderHandle AttackOrder = Orders->GetCurrentOrderHandle();
	TestTrue(TEXT("Keep scenario starts with attack"), AttackOrder.IsValid());
	FCombatAIAbilityUsageRule Rule;
	Rule.AbilityDefinitionId = AbilityData->GetPrimaryAssetId();
	Rule.TargetPolicy = ECombatAIAbilityTargetPolicy::Self;
	Rule.BaseUtility = 0.9f;
	Profile->AbilityUsageRules.Add(Rule);
	CombatAITacticsTests::Advance(World, 30);
	TestEqual(TEXT("A real attack boundary was offered once"), ReadyCount, 1);
	TestEqual(TEXT("Invalidated candidate never submits cast"), CastFinished, 0);
	TestEqual(TEXT("Keep does not resubmit the attack"), Brain->GetSubmittedCount(), uint64(1));
	TestEqual(TEXT("Keep preserves the original attack order"), Orders->GetCurrentOrderHandle(), AttackOrder);
	Profile->AbilityUsageRules[0].BaseUtility = 0.9f;
	Profile->AbilityUsageRules[0].InterruptPreference = ECombatAIInterruptPreference::AttackBoundary;
	CombatAITacticsTests::Advance(World, 30);
	TestEqual(TEXT("A second real boundary rechecks the current interrupt preference"), ReadyCount, 2);
	TestEqual(TEXT("A Never candidate cannot consume an already-ready boundary"), CastFinished, 0);
	TestEqual(TEXT("Preference Keep still submits no replacement order"), Brain->GetSubmittedCount(), uint64(1));
	TestEqual(TEXT("Preference Keep preserves the original attack order"), Orders->GetCurrentOrderHandle(), AttackOrder);
	Orders->OnExecutionBoundaryReady().Remove(ReadyBinding);
	Orders->OnOrderFinished().Remove(FinishBinding);
	return true;
}

/** 战术 EQS 先取得 World 配额，再把唯一结果冻结成精确 MoveToPoint；Order 不得二次运行自己的 EQS。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatAITacticalEQSStateTreeTest,
	"Combat.AI.Tactics.EQS.BudgetedMove", CombatAITacticsTests::Flags)
bool FCombatAITacticalEQSStateTreeTest::RunTest(const FString& Parameters)
{
	FCombatAutomationWorldFixture Fixture;
	if (!Fixture.IsValid()) return false;
	auto& World = *Fixture.GetWorld();
	TestNotNull(TEXT("Fixture creates the engine AI/EQS service"), World.CreateAISystem());
	auto* Unit = CombatAITacticsTests::Spawn(World);
	auto* Target = CombatAITacticsTests::Spawn(World);
	Unit->SetCombatTeamId(FCombatTeamId(1));
	Target->SetCombatTeamId(FCombatTeamId(2));
	Target->SetActorLocation(FVector(150.0f, 0.0f, 0.0f));
	auto* Profile = CombatAITacticsTests::Profile(Unit);
	Profile->RootTree = FCombatAIAssetBuilder::BuildTacticalRootTree(Profile);
	Profile->TacticalLocationQuery = FCombatAIAssetBuilder::BuildTacticalLocationQuery(Profile, 600.0f);
	Profile->bEnablePerception = true;
	Profile->Perception.ActiveInterval = 0.05f;
	Profile->Perception.IdleInterval = 0.05f;
	Profile->RepositionTriggerDistance = 300.0f;
	Profile->RepositionUtility = 0.8f;
	Profile->TacticalQueryRetrySeconds = 0.05f;
	FString Diagnostic;
	TestTrue(TEXT("Tactical EQS profile is valid"), Profile->ValidateRuntime(Diagnostic));

	auto* Brain = Unit->GetCombatAIBrainComponent();
	auto* Orders = Unit->GetCombatOrderComponent();
	auto* Budget = World.GetSubsystem<UCombatAIWorldSubsystem>();
	Orders->SetNavigationDeferredForTesting(true);
	TestNull(TEXT("Tactical point is not re-queried by Order"), Orders->MoveDestinationQuery.Get());
	TestTrue(TEXT("Ranged guard owns a spatial assignment"), Brain->SetAssignment({}));
	Budget->SetFrameLimitsForTesting(16, 0);
	Brain->ConfigureProfile(Profile);
	CombatAITacticsTests::Advance(World, 8);
	TestTrue(TEXT("Denied EQS is recorded before execution"), Budget->GetSnapshot().EQSDeferred > 0);
	TestEqual(TEXT("Budget denial submits no Move order"), Brain->GetSubmittedCount(), uint64(0));
	TestEqual(TEXT("Budget denial has no active engine query"), Brain->GetActiveTacticalQueryCount(), 0);

	Budget->SetFrameLimitsForTesting(16, 1);
	CombatAITacticsTests::Advance(World, 16);
	const FVector Goal = Orders->GetCurrentMoveGoal();
	TestEqual(TEXT("One tactical query is granted"), Budget->GetSnapshot().EQSGranted, uint64(1));
	TestEqual(TEXT("Completed query leaves no in-flight token"), Budget->GetSnapshot().ActiveEQS, 0);
	TestEqual(TEXT("One exact Move order is submitted"), Brain->GetSubmittedCount(), uint64(1));
	TestTrue(TEXT("EQS selects a finite point at configured target distance"),
		!Goal.ContainsNaN() && FMath::IsNearlyEqual(FVector::Dist2D(Goal, Target->GetActorLocation()), 600.0f, 1.0f));
	Unit->SetActorLocation(Goal);
	TestTrue(TEXT("Injected navigation completion matches tactical Move"),
		Orders->CompleteMovementForTesting(Orders->GetCurrentOrderHandle(), true));
	Profile->RepositionTriggerDistance = 0.0f;
	Profile->TacticalLocationQuery = nullptr;
	CombatAITacticsTests::Advance(World, 4);
	TestTrue(TEXT("Tactical Move completion reaches shared receipt"), Brain->GetLastReceipt().Result.bSuccess
		&& Brain->GetLastReceipt().Result.Type == ECombatOrderType::MoveToPoint);
	return true;
}

/** EQS 已成功但精确 Move 失败时，Reposition 必须进入共用有界故障状态，不能立即重查。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatAITacticalRepositionFailureBoundTest,
	"Combat.AI.Tactics.EQS.MoveFailureBound", CombatAITacticsTests::Flags)
bool FCombatAITacticalRepositionFailureBoundTest::RunTest(const FString& Parameters)
{
	FCombatAutomationWorldFixture Fixture;
	if (!Fixture.IsValid()) return false;
	auto& World = *Fixture.GetWorld();
	TestNotNull(TEXT("Fixture creates the engine AI/EQS service"), World.CreateAISystem());
	auto* Unit = CombatAITacticsTests::Spawn(World);
	auto* Target = CombatAITacticsTests::Spawn(World);
	Unit->SetCombatTeamId(FCombatTeamId(1));
	Target->SetCombatTeamId(FCombatTeamId(2));
	Target->SetActorLocation(FVector(150.0f, 0.0f, 0.0f));
	auto* Profile = CombatAITacticsTests::Profile(Unit);
	Profile->RootTree = FCombatAIAssetBuilder::BuildTacticalRootTree(Profile);
	Profile->TacticalLocationQuery = FCombatAIAssetBuilder::BuildTacticalLocationQuery(Profile, 600.0f);
	Profile->bEnablePerception = true;
	Profile->Perception.ActiveInterval = Profile->Perception.IdleInterval = 0.05f;
	Profile->RepositionTriggerDistance = 300.0f;
	Profile->RepositionUtility = 0.8f;
	Profile->AttackUtility = 0.0f;
	Profile->MaxAttempts = 1;
	auto* Brain = Unit->GetCombatAIBrainComponent();
	auto* Orders = Unit->GetCombatOrderComponent();
	auto* Budget = World.GetSubsystem<UCombatAIWorldSubsystem>();
	Orders->SetNavigationDeferredForTesting(true);
	Orders->MaxMoveRetries = 0;
	Budget->SetFrameLimitsForTesting(16, 1);
	TestTrue(TEXT("Tactical unit owns an assignment"), Brain->SetAssignment({}));
	Brain->ConfigureProfile(Profile);
	for (int32 Index = 0; Index < 24 && !Orders->GetCurrentOrderHandle().IsValid(); ++Index)
	{
		CombatAITacticsTests::Advance(World, 1);
	}
	const FCombatOrderHandle MoveOrder = Orders->GetCurrentOrderHandle();
	TestTrue(TEXT("Fixture starts one exact tactical Move"), MoveOrder.IsValid());
	TestEqual(TEXT("Only the first tactical Move was submitted"), Brain->GetSubmittedCount(), uint64(1));
	Budget->SetFrameLimitsForTesting(16, 0);
	TestTrue(TEXT("Fixture injects the terminal tactical Move failure"),
		Orders->CompleteMovementForTesting(MoveOrder, false));
	for (int32 Index = 0; Index < 8 && !Brain->IsRetryBlocked(); ++Index)
	{
		CombatAITacticsTests::Advance(World, 1);
	}
	TestEqual(TEXT("Tactical Move failure is counted once"),
		Brain->GetFailureCount(ECombatAIRoleOperation::Reposition), 1);
	TestTrue(TEXT("Configured attempt limit blocks immediate tactical re-query"), Brain->IsRetryBlocked());
	TestEqual(TEXT("Blocked tactical failure submits no replacement Move"), Brain->GetSubmittedCount(), uint64(1));
	TestEqual(TEXT("Blocked tactical failure receives no replacement EQS grant"), Budget->GetSnapshot().EQSGranted, uint64(1));
	return true;
}

/** 在途上限和职责取消都以查询身份为准；Abort 后的引擎回调不得提交旧点。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatAITacticalEQSCancellationTest,
	"Combat.AI.Tactics.EQS.AssignmentCancellation", CombatAITacticsTests::Flags)
bool FCombatAITacticalEQSCancellationTest::RunTest(const FString& Parameters)
{
	FCombatAutomationWorldFixture Fixture;
	if (!Fixture.IsValid()) return false;
	auto& World = *Fixture.GetWorld();
	TestNotNull(TEXT("Fixture creates the engine AI/EQS service"), World.CreateAISystem());
	auto* Unit = CombatAITacticsTests::Spawn(World);
	auto* Target = CombatAITacticsTests::Spawn(World);
	Unit->SetCombatTeamId(FCombatTeamId(1));
	Target->SetCombatTeamId(FCombatTeamId(2));
	Target->SetActorLocation(FVector(150.0f, 0.0f, 0.0f));
	auto* Profile = CombatAITacticsTests::Profile(Unit);
	Profile->RootTree = FCombatAIAssetBuilder::BuildTacticalRootTree(Profile);
	Profile->TacticalLocationQuery = FCombatAIAssetBuilder::BuildTacticalLocationQuery(Profile, 600.0f);
	Profile->bEnablePerception = true;
	Profile->Perception.ActiveInterval = Profile->Perception.IdleInterval = 0.05f;
	Profile->RepositionTriggerDistance = 300.0f;
	Profile->RepositionUtility = 0.8f;
	Profile->AttackUtility = 0.0f;
	auto* Brain = Unit->GetCombatAIBrainComponent();
	auto* Budget = World.GetSubsystem<UCombatAIWorldSubsystem>();
	Unit->GetCombatOrderComponent()->SetNavigationDeferredForTesting(true);
	Budget->SetFrameLimitsForTesting(16, 1);
	TestTrue(TEXT("Tactical unit owns an initial assignment"), Brain->SetAssignment({}));
	Brain->ConfigureProfile(Profile);
	TestTrue(TEXT("One engine query becomes active before its manager tick"),
		CombatAITacticsTests::AdvanceUntilTacticalQueryStarts(World, *Brain));
	const uint64 GrantedBeforeDuplicate = Budget->GetSnapshot().EQSGranted;
	TestFalse(TEXT("A second activation cannot start beside the active query"),
		Brain->BeginTacticalLocationQuery(Brain->GetDecisionScope(), TEXT("TacticalAction"), MAX_uint64));
	TestEqual(TEXT("Rejected duplicate consumes no second World grant"),
		Budget->GetSnapshot().EQSGranted, GrantedBeforeDuplicate);

	FCombatAIAssignment NewAssignment;
	NewAssignment.Home = FVector(25.0f, 0.0f, 0.0f);
	TestTrue(TEXT("Changing assignment succeeds and cancels the old query"), Brain->SetAssignment(NewAssignment));
	TestEqual(TEXT("Assignment cancellation clears the engine query"), Brain->GetActiveTacticalQueryCount(), 0);
	TestEqual(TEXT("Assignment cancellation clears the World token"), Budget->GetSnapshot().ActiveEQS, 0);
	TestEqual(TEXT("Assignment cancellation is counted once"), Budget->GetSnapshot().EQSCancelled, uint64(1));
	Profile->RepositionTriggerDistance = 0.0f;
	Profile->TacticalLocationQuery = nullptr;
	CombatAITacticsTests::Advance(World, 2);
	TestEqual(TEXT("Aborted query callback cannot submit an old Move"), Brain->GetSubmittedCount(), uint64(0));
	return true;
}

/** 旧 Abort 回调晚于新代预算等待到达时，必须按引擎 QueryId 淘汰，不能消费新代工作区。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatAITacticalEQSStaleCallbackIsolationTest,
	"Combat.AI.Tactics.EQS.StaleCallbackIsolation", CombatAITacticsTests::Flags)
bool FCombatAITacticalEQSStaleCallbackIsolationTest::RunTest(const FString& Parameters)
{
	FCombatAutomationWorldFixture Fixture;
	if (!Fixture.IsValid()) return false;
	auto& World = *Fixture.GetWorld();
	TestNotNull(TEXT("Fixture creates the engine AI/EQS service"), World.CreateAISystem());
	auto* Unit = CombatAITacticsTests::Spawn(World);
	auto* Target = CombatAITacticsTests::Spawn(World);
	Unit->SetCombatTeamId(FCombatTeamId(1));
	Target->SetCombatTeamId(FCombatTeamId(2));
	Target->SetActorLocation(FVector(150.0f, 0.0f, 0.0f));
	auto* Profile = CombatAITacticsTests::Profile(Unit);
	Profile->RootTree = FCombatAIAssetBuilder::BuildTacticalRootTree(Profile);
	Profile->TacticalLocationQuery = FCombatAIAssetBuilder::BuildTacticalLocationQuery(Profile, 600.0f);
	Profile->bEnablePerception = true;
	Profile->Perception.ActiveInterval = Profile->Perception.IdleInterval = 0.05f;
	Profile->RepositionTriggerDistance = 300.0f;
	Profile->RepositionUtility = 0.8f;
	Profile->AttackUtility = 0.0f;
	auto* Brain = Unit->GetCombatAIBrainComponent();
	auto* Budget = World.GetSubsystem<UCombatAIWorldSubsystem>();
	Unit->GetCombatOrderComponent()->SetNavigationDeferredForTesting(true);
	Budget->SetFrameLimitsForTesting(16, 1);
	TestTrue(TEXT("Tactical unit owns an initial assignment"), Brain->SetAssignment({}));
	Brain->ConfigureProfile(Profile);
	TestTrue(TEXT("Old query starts before cancellation"),
		CombatAITacticsTests::AdvanceUntilTacticalQueryStarts(World, *Brain));
	const int32 OldQueryId = Brain->TacticalQuery.QueryId;
	const uint64 OldGeneration = Brain->TacticalQuery.Generation;
	TestTrue(TEXT("Old engine query has a concrete identity"), OldQueryId != INDEX_NONE);

	Budget->SetFrameLimitsForTesting(16, 0);
	FCombatAIAssignment NewAssignment;
	NewAssignment.Home = FVector(25.0f, 0.0f, 0.0f);
	TestTrue(TEXT("Changing assignment aborts the old query"), Brain->SetAssignment(NewAssignment));
	for (int32 Index = 0; Index < 16
		&& !(Brain->TacticalQuery.IsValid() && !Brain->TacticalQuery.HasEngineQuery()); ++Index)
	{
		CombatAITacticsTests::Advance(World, 1);
	}
	TestTrue(TEXT("Replacement query waits for World budget without an engine QueryId"),
		Brain->TacticalQuery.IsValid() && !Brain->TacticalQuery.HasEngineQuery());
	const uint64 NewGeneration = Brain->TacticalQuery.Generation;
	const uint64 NewActivation = Brain->TacticalQuery.Activation;
	TestTrue(TEXT("Replacement query owns a new generation"), NewGeneration != 0 && NewGeneration != OldGeneration);

	auto LateAbort = MakeShared<FEnvQueryResult>(EEnvQueryStatus::Aborted);
	LateAbort->QueryID = OldQueryId;
	Brain->OnTacticalLocationQueryFinished(LateAbort, OldGeneration);
	TestTrue(TEXT("Old callback cannot consume the replacement query"),
		Brain->TacticalQuery.IsValid() && Brain->TacticalQuery.Generation == NewGeneration
		&& Brain->TacticalQuery.Activation == NewActivation && !Brain->TacticalQuery.HasEngineQuery());
	TestFalse(TEXT("Old callback cannot publish a replacement-query inbox result"), Brain->TacticalQueryInbox.bReady);
	const FCombatAIWorldBudgetSnapshot AfterLateAbort = Budget->GetSnapshot();
	TestEqual(TEXT("Old callback does not finish a non-existent replacement token"), AfterLateAbort.ActiveEQS, 0);
	TestEqual(TEXT("Only the explicit old-query cancellation is counted"), AfterLateAbort.EQSCancelled, uint64(1));
	TestEqual(TEXT("Rejected old callback is not counted as a new stale result"), AfterLateAbort.EQSStale, uint64(0));

	Budget->SetFrameLimitsForTesting(16, 1);
	CombatAITacticsTests::Advance(World, 24);
	TestEqual(TEXT("Replacement query eventually receives its own grant"), Budget->GetSnapshot().EQSGranted, uint64(2));
	TestEqual(TEXT("Replacement query completes exactly once"), Budget->GetSnapshot().EQSSucceeded, uint64(1));
	TestEqual(TEXT("Replacement query submits one exact Move"), Brain->GetSubmittedCount(), uint64(1));
	return true;
}

/** 目标在查询运行中失效时，完成回调只形成陈旧诊断，不把旧点写入命令工作区。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatAITacticalEQSStaleTargetTest,
	"Combat.AI.Tactics.EQS.StaleTarget", CombatAITacticsTests::Flags)
bool FCombatAITacticalEQSStaleTargetTest::RunTest(const FString& Parameters)
{
	FCombatAutomationWorldFixture Fixture;
	if (!Fixture.IsValid()) return false;
	auto& World = *Fixture.GetWorld();
	TestNotNull(TEXT("Fixture creates the engine AI/EQS service"), World.CreateAISystem());
	auto* Unit = CombatAITacticsTests::Spawn(World);
	auto* Target = CombatAITacticsTests::Spawn(World);
	Unit->SetCombatTeamId(FCombatTeamId(1));
	Target->SetCombatTeamId(FCombatTeamId(2));
	Target->SetActorLocation(FVector(150.0f, 0.0f, 0.0f));
	auto* Profile = CombatAITacticsTests::Profile(Unit);
	Profile->RootTree = FCombatAIAssetBuilder::BuildTacticalRootTree(Profile);
	Profile->TacticalLocationQuery = FCombatAIAssetBuilder::BuildTacticalLocationQuery(Profile, 600.0f);
	Profile->bEnablePerception = true;
	Profile->Perception.ActiveInterval = Profile->Perception.IdleInterval = 0.05f;
	Profile->RepositionTriggerDistance = 300.0f;
	Profile->RepositionUtility = 0.8f;
	Profile->AttackUtility = 0.0f;
	auto* Brain = Unit->GetCombatAIBrainComponent();
	auto* Budget = World.GetSubsystem<UCombatAIWorldSubsystem>();
	Budget->SetFrameLimitsForTesting(16, 1);
	TestTrue(TEXT("Tactical unit owns an assignment"), Brain->SetAssignment({}));
	Brain->ConfigureProfile(Profile);
	TestTrue(TEXT("Query starts before target invalidation"),
		CombatAITacticsTests::AdvanceUntilTacticalQueryStarts(World, *Brain));
	Target->Destroy();
	CombatAITacticsTests::Advance(World, 2);
	TestEqual(TEXT("Stale result releases the World token"), Budget->GetSnapshot().ActiveEQS, 0);
	TestEqual(TEXT("Target invalidation counts one stale result"), Budget->GetSnapshot().EQSStale, uint64(1));
	TestEqual(TEXT("Stale target submits no Move"), Brain->GetSubmittedCount(), uint64(0));
	TestTrue(TEXT("Stale query reaches a shared failure receipt"), !Brain->GetLastReceipt().Result.bSuccess
		&& Brain->GetLastReceipt().Result.Type == ECombatOrderType::MoveToPoint);
	return true;
}

/** 合法查询若没有生成点，只失败一次并等待新观察；不能在同一快照内形成忙循环。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatAITacticalEQSFailureTest,
	"Combat.AI.Tactics.EQS.EmptyResult", CombatAITacticsTests::Flags)
bool FCombatAITacticalEQSFailureTest::RunTest(const FString& Parameters)
{
	FCombatAutomationWorldFixture Fixture;
	if (!Fixture.IsValid()) return false;
	auto& World = *Fixture.GetWorld();
	TestNotNull(TEXT("Fixture creates the engine AI/EQS service"), World.CreateAISystem());
	auto* Unit = CombatAITacticsTests::Spawn(World);
	auto* Target = CombatAITacticsTests::Spawn(World);
	Unit->SetCombatTeamId(FCombatTeamId(1));
	Target->SetCombatTeamId(FCombatTeamId(2));
	Target->SetActorLocation(FVector(150.0f, 0.0f, 0.0f));
	auto* Profile = CombatAITacticsTests::Profile(Unit);
	Profile->RootTree = FCombatAIAssetBuilder::BuildTacticalRootTree(Profile);
	Profile->TacticalLocationQuery = FCombatAIAssetBuilder::BuildTacticalLocationQuery(Profile, 600.0f);
	auto* Generator = Profile->TacticalLocationQuery && !Profile->TacticalLocationQuery->GetOptions().IsEmpty()
		? Cast<UCombatAITacticalLocationGenerator>(Profile->TacticalLocationQuery->GetOptions()[0]->Generator) : nullptr;
	TestNotNull(TEXT("Fixture query uses the native tactical generator"), Generator);
	if (!Generator) return false;
	Generator->Distance = 0.0f;
	Profile->bEnablePerception = true;
	Profile->Perception.ActiveInterval = Profile->Perception.IdleInterval = 0.05f;
	Profile->RepositionTriggerDistance = 300.0f;
	Profile->RepositionUtility = 0.8f;
	Profile->AttackUtility = 0.0f;
	auto* Brain = Unit->GetCombatAIBrainComponent();
	auto* Budget = World.GetSubsystem<UCombatAIWorldSubsystem>();
	Budget->SetFrameLimitsForTesting(16, 1);
	TestTrue(TEXT("Tactical unit owns an assignment"), Brain->SetAssignment({}));
	Brain->ConfigureProfile(Profile);
	TestTrue(TEXT("Empty-result query starts"),
		CombatAITacticsTests::AdvanceUntilTacticalQueryStarts(World, *Brain));
	Budget->SetFrameLimitsForTesting(16, 0);
	CombatAITacticsTests::Advance(World, 2);
	TestEqual(TEXT("Empty result is counted as one failed engine query"), Budget->GetSnapshot().EQSFailed, uint64(1));
	TestEqual(TEXT("Failed query submits no Move"), Brain->GetSubmittedCount(), uint64(0));
	TestTrue(TEXT("Failed query reaches the shared failure receipt"), !Brain->GetLastReceipt().Result.bSuccess
		&& Brain->GetLastReceipt().Result.Type == ECombatOrderType::MoveToPoint);
	return true;
}

/** 接管、Profile 更换与 EndPlay 都在旧回调前清理查询 token 和引擎 QueryId。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatAITacticalEQSLifecycleTest,
	"Combat.AI.Tactics.EQS.LifecycleCancellation", CombatAITacticsTests::Flags)
bool FCombatAITacticalEQSLifecycleTest::RunTest(const FString& Parameters)
{
	FCombatAutomationWorldFixture Fixture;
	if (!Fixture.IsValid()) return false;
	auto& World = *Fixture.GetWorld();
	TestNotNull(TEXT("Fixture creates the engine AI/EQS service"), World.CreateAISystem());
	auto* Unit = CombatAITacticsTests::Spawn(World);
	auto* Target = CombatAITacticsTests::Spawn(World);
	Unit->SetCombatTeamId(FCombatTeamId(1));
	Target->SetCombatTeamId(FCombatTeamId(2));
	Target->SetActorLocation(FVector(150.0f, 0.0f, 0.0f));
	auto* Profile = CombatAITacticsTests::Profile(Unit);
	Profile->RootTree = FCombatAIAssetBuilder::BuildTacticalRootTree(Profile);
	Profile->TacticalLocationQuery = FCombatAIAssetBuilder::BuildTacticalLocationQuery(Profile, 600.0f);
	Profile->bEnablePerception = true;
	Profile->Perception.ActiveInterval = Profile->Perception.IdleInterval = 0.05f;
	Profile->RepositionTriggerDistance = 300.0f;
	Profile->RepositionUtility = 0.8f;
	auto* Brain = Unit->GetCombatAIBrainComponent();
	auto* Budget = World.GetSubsystem<UCombatAIWorldSubsystem>();
	Budget->SetFrameLimitsForTesting(16, 1);
	TestTrue(TEXT("Tactical unit owns an assignment"), Brain->SetAssignment({}));
	Brain->ConfigureProfile(Profile);
	TestTrue(TEXT("Manual cancellation fixture starts a query"),
		CombatAITacticsTests::AdvanceUntilTacticalQueryStarts(World, *Brain));
	Brain->SuspendForManualCommand();
	TestEqual(TEXT("Manual takeover clears query"), Budget->GetSnapshot().ActiveEQS, 0);

	TestTrue(TEXT("Explicit resume starts a fresh run"), Brain->ResumeAutonomous());
	TestTrue(TEXT("Profile replacement fixture starts a query"),
		CombatAITacticsTests::AdvanceUntilTacticalQueryStarts(World, *Brain));
	Brain->ConfigureProfile(nullptr);
	TestEqual(TEXT("Profile removal clears query"), Budget->GetSnapshot().ActiveEQS, 0);

	Brain->ConfigureProfile(Profile);
	TestTrue(TEXT("Death fixture starts a query"),
		CombatAITacticsTests::AdvanceUntilTacticalQueryStarts(World, *Brain));
	TestTrue(TEXT("Owner death enters the normal lifecycle path"),
		Unit->GetCombatLifecycleComponent()->RequestDeath(
			World.GetSubsystem<UCombatEventSubsystem>()->CreateRootEvent(), nullptr));
	TestFalse(TEXT("Owner death stops tactical StateTree execution"), Brain->IsRunning());
	TestEqual(TEXT("Owner death clears its query"), Budget->GetSnapshot().ActiveEQS, 0);
	TestTrue(TEXT("Owner respawn restores the autonomous lifecycle"),
		Unit->GetCombatLifecycleComponent()->RespawnAtLocation(Unit->GetActorLocation()));
	TestTrue(TEXT("EndPlay fixture starts a query after respawn"),
		CombatAITacticsTests::AdvanceUntilTacticalQueryStarts(World, *Brain));
	Unit->Destroy();
	TestEqual(TEXT("EndPlay clears the final query"), Budget->GetSnapshot().ActiveEQS, 0);
	TestEqual(TEXT("Each lifecycle path cancels exactly one query"), Budget->GetSnapshot().EQSCancelled, uint64(4));
	return true;
}

/** 64 个自主单位在共享 World 配额下完成独立战术查询和清理。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatAITacticsCapacity64Test,
	"Combat.AI.Tactics.Capacity64", CombatAITacticsTests::Flags)
bool FCombatAITacticsCapacity64Test::RunTest(const FString& Parameters)
{
	return CombatAITacticsTests::RunCapacity(*this, 64);
}

/** 128 个自主单位验证延期队列不丢失工作，且不会突破单单位查询上限。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatAITacticsCapacity128Test,
	"Combat.AI.Tactics.Capacity128", CombatAITacticsTests::Flags)
bool FCombatAITacticsCapacity128Test::RunTest(const FString& Parameters)
{
	return CombatAITacticsTests::RunCapacity(*this, 128);
}

/** 256 个自主单位给出阶段 C 的最高档 World 查询与命令容量样本。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatAITacticsCapacity256Test,
	"Combat.AI.Tactics.Capacity256", CombatAITacticsTests::Flags)
bool FCombatAITacticsCapacity256Test::RunTest(const FString& Parameters)
{
	return CombatAITacticsTests::RunCapacity(*this, 256);
}
#endif
