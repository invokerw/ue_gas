#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "HAL/IConsoleManager.h"

#include "Combat/Ability/CombatAbilitySystemComponent.h"
#include "Combat/Ability/CombatGameplayAbility.h"
#include "Combat/Combat/CombatDamageSubsystem.h"
#include "Combat/Core/CombatTags.h"
#include "Combat/Data/CombatDefinitionData.h"
#include "Combat/Demo/CombatDemoAbilities.h"
#include "Combat/Tests/CombatAutomationWorldFixture.h"
#include "Combat/Unit/CombatProgressionComponent.h"
#include "Combat/Unit/CombatUnitCharacter.h"

namespace CombatProgressionTests
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;

	ACombatUnitCharacter* SpawnUnit(UWorld& World, const FName Name, const int32 ExperienceReward = 100)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		ACombatUnitCharacter* Unit = World.SpawnActor<ACombatUnitCharacter>(FVector::ZeroVector, FRotator::ZeroRotator, Params);
		if (!Unit) return nullptr;
		UCombatUnitData* Data = NewObject<UCombatUnitData>(Unit);
		Data->DefinitionName = Name;
		Data->InitialTeamId = FCombatTeamId(1);
		Data->ExperienceReward = ExperienceReward;
		if (!Unit->InitializeFromUnitData(Data)) return nullptr;
		if (!Unit->HasActorBegunPlay()) Unit->DispatchBeginPlay();
		return Unit;
	}
}

/** Dota 风格累计经验曲线和跨级技能点。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatProgressionCurveTest, "Combat.Progression.ExperienceCurveAndPoints", CombatProgressionTests::Flags)
bool FCombatProgressionCurveTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FCombatAutomationWorldFixture Fixture;
	if (!Fixture.IsValid()) return false;
	ACombatUnitCharacter* Unit = CombatProgressionTests::SpawnUnit(*Fixture.GetWorld(), TEXT("progression_curve"));
	if (!TestNotNull(TEXT("Unit"), Unit)) return false;
	UCombatProgressionComponent* Progression = Unit->GetCombatProgressionComponent();
	if (!TestNotNull(TEXT("Progression component"), Progression)) return false;
	TestEqual(TEXT("Level 1 threshold"), Progression->GetExperienceForLevel(1), int64(0));
	TestEqual(TEXT("Level 2 threshold"), Progression->GetExperienceForLevel(2), int64(200));
	TestEqual(TEXT("Level 3 threshold"), Progression->GetExperienceForLevel(3), int64(500));
	TestEqual(TEXT("Level 30 threshold"), Progression->GetExperienceForLevel(30), int64(46400));
	TestEqual(TEXT("Initial level"), Progression->GetLevel(), 1);
	TestTrue(TEXT("199 XP is accepted"), Progression->AddExperience(199));
	TestEqual(TEXT("Still level 1 before threshold"), Progression->GetLevel(), 1);
	TestEqual(TEXT("Current level XP"), Progression->GetExperienceIntoLevel(), int64(199));
	TestEqual(TEXT("Progress is 199/200"), Progression->GetExperienceProgress(), 0.995f);
	TestTrue(TEXT("Cross level 2"), Progression->AddExperience(1));
	TestEqual(TEXT("Level 2 reached"), Progression->GetLevel(), 2);
	TestEqual(TEXT("One point per level"), Progression->GetUnspentAbilityPoints(), 1);
	TestTrue(TEXT("Cross level 3"), Progression->AddExperience(300));
	TestEqual(TEXT("Level 3 reached"), Progression->GetLevel(), 3);
	TestEqual(TEXT("Two points after two level ups"), Progression->GetUnspentAbilityPoints(), 2);
	TestEqual(TEXT("Level 3 next threshold"), Progression->GetExperienceToNextLevel(), int64(400));
	return true;
}

/** 技能加点只能在服务器消耗技能点，并受英雄等级与技能上限约束。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatProgressionAbilityUpgradeTest, "Combat.Progression.AbilityUpgradeAuthorization", CombatProgressionTests::Flags)
bool FCombatProgressionAbilityUpgradeTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FCombatAutomationWorldFixture Fixture;
	if (!Fixture.IsValid()) return false;
	ACombatUnitCharacter* Unit = CombatProgressionTests::SpawnUnit(*Fixture.GetWorld(), TEXT("progression_upgrade"));
	if (!TestNotNull(TEXT("Unit"), Unit)) return false;
	UCombatProgressionComponent* Progression = Unit->GetCombatProgressionComponent();
	UCombatAbilitySystemComponent* Asc = Unit->GetCombatAbilitySystemComponent();
	FGameplayTag Failure;
	TestFalse(TEXT("No point cannot upgrade"), Progression->UpgradeAbility(FGameplayAbilitySpecHandle(), Failure));
	TestEqual(TEXT("No point failure"), Failure, CombatTags::Failure_Progression_NoAbilityPoints.GetTag());

	UCombatAbilityData* Data = NewObject<UCombatAbilityData>(Unit);
	Data->DefinitionName = TEXT("progression_upgrade_skill");
	Data->MaxLevel = 4;
	Data->BehaviorTags.AddTag(CombatTags::Ability_Behavior_NoTarget);
	Data->TargetingRules.TargetTeamTag = CombatTags::TargetTeam_None;
	TGuardValue<TObjectPtr<UCombatAbilityData>> RestoreData(GetMutableDefault<UCombatSelfHealAbility>()->AbilityData, Data);
	FGameplayAbilitySpecHandle Handle;
	TestTrue(TEXT("Grant skill"), Asc->GrantCombatAbility(UCombatSelfHealAbility::StaticClass(), 1, false, Handle, Failure));
	TestTrue(TEXT("Gain one level and point"), Progression->AddExperience(200));
	TestEqual(TEXT("Hero level 2"), Progression->GetLevel(), 2);
	TestTrue(TEXT("Upgrade skill with point"), Progression->UpgradeAbility(Handle, Failure));
	const FGameplayAbilitySpec* Spec = Asc->FindAbilitySpecFromHandle(Handle);
	TestNotNull(TEXT("Upgraded spec"), Spec);
	if (Spec) TestEqual(TEXT("Ability reaches level 2"), Spec->Level, 2);
	TestEqual(TEXT("Point consumed"), Progression->GetUnspentAbilityPoints(), 0);
	TestFalse(TEXT("Second upgrade requires another point"), Progression->UpgradeAbility(Handle, Failure));
	TestEqual(TEXT("Second upgrade failure"), Failure, CombatTags::Failure_Progression_NoAbilityPoints.GetTag());
	return true;
}

/** 致死伤害只在死亡转换成功后把被击杀单位的经验奖励发给击杀者。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatProgressionKillRewardTest, "Combat.Progression.KillExperienceReward", CombatProgressionTests::Flags)
bool FCombatProgressionKillRewardTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FCombatAutomationWorldFixture Fixture;
	if (!Fixture.IsValid()) return false;
	ACombatUnitCharacter* Killer = CombatProgressionTests::SpawnUnit(*Fixture.GetWorld(), TEXT("progression_killer"));
	ACombatUnitCharacter* Victim = CombatProgressionTests::SpawnUnit(*Fixture.GetWorld(), TEXT("progression_victim"), 175);
	if (!TestNotNull(TEXT("Killer"), Killer) || !TestNotNull(TEXT("Victim"), Victim)) return false;
	UCombatProgressionComponent* KillerProgression = Killer->GetCombatProgressionComponent();
	FCombatDamageRequest Request;
	Request.Source = Killer;
	Request.Target = Victim;
	Request.Amount = 100000.0f;
	Request.DamageType = ECombatDamageType::Pure;
	const FCombatDamageResult Result = Fixture.GetWorld()->GetSubsystem<UCombatDamageSubsystem>()->DealDamage(Request);
	TestTrue(TEXT("Lethal damage succeeds"), Result.bSuccess);
	TestEqual(TEXT("Victim is dead"), Victim->GetLifeState(), ECombatLifeState::Dead);
	TestEqual(TEXT("Killer receives configured reward"), KillerProgression->GetExperience(), int64(175));
	TestEqual(TEXT("Killer remains level 1 below threshold"), KillerProgression->GetLevel(), 1);
	const FCombatDamageResult Repeat = Fixture.GetWorld()->GetSubsystem<UCombatDamageSubsystem>()->DealDamage(Request);
	TestFalse(TEXT("Repeated death request is rejected"), Repeat.bSuccess);
	TestEqual(TEXT("Repeated death does not award experience"), KillerProgression->GetExperience(), int64(175));
	return true;
}

/** 开发加经验命令复用服务器成长入口，并支持默认主控单位和显式 UniqueID。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatProgressionDebugCommandTest, "Combat.Progression.DebugAddExperienceCommand", CombatProgressionTests::Flags)
bool FCombatProgressionDebugCommandTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FCombatAutomationWorldFixture Fixture;
	if (!Fixture.IsValid()) return false;

	UWorld* World = Fixture.GetWorld();
	APlayerController* Player = World->SpawnActor<APlayerController>();
	ACombatUnitCharacter* DefaultUnit = CombatProgressionTests::SpawnUnit(*World, TEXT("progression_debug_default"));
	ACombatUnitCharacter* ExplicitUnit = CombatProgressionTests::SpawnUnit(*World, TEXT("progression_debug_explicit"));
	if (!TestNotNull(TEXT("Player controller"), Player)
		|| !TestNotNull(TEXT("Default target"), DefaultUnit)
		|| !TestNotNull(TEXT("Explicit target"), ExplicitUnit))
	{
		return false;
	}
	if (!TestTrue(TEXT("Bind default target to player"), DefaultUnit->SetCommandingPlayerController(Player))) return false;

	IConsoleObject* ConsoleObject = IConsoleManager::Get().FindConsoleObject(TEXT("combat.Debug.AddExperience"));
	IConsoleCommand* Command = ConsoleObject ? ConsoleObject->AsCommand() : nullptr;
	if (!TestNotNull(TEXT("Development command is registered"), Command)) return false;

	TArray<FString> DefaultArgs{TEXT("200")};
	TestTrue(TEXT("Default target command executes"), Command->Execute(DefaultArgs, World, *GLog));
	TestEqual(TEXT("Default target reaches level 2"), DefaultUnit->GetCombatProgressionComponent()->GetLevel(), 2);
	TestEqual(TEXT("Default target receives one point"), DefaultUnit->GetCombatProgressionComponent()->GetUnspentAbilityPoints(), 1);

	TArray<FString> ExplicitArgs{TEXT("200"), FString::FromInt(ExplicitUnit->GetUniqueID())};
	TestTrue(TEXT("Explicit UniqueID command executes"), Command->Execute(ExplicitArgs, World, *GLog));
	TestEqual(TEXT("Explicit target receives experience"), ExplicitUnit->GetCombatProgressionComponent()->GetExperience(), int64(200));
	TestEqual(TEXT("Explicit target reaches level 2"), ExplicitUnit->GetCombatProgressionComponent()->GetLevel(), 2);

	const int64 ExperienceBeforeInvalidAmount = ExplicitUnit->GetCombatProgressionComponent()->GetExperience();
	TArray<FString> InvalidArgs{TEXT("-1"), ExplicitUnit->GetName()};
	TestTrue(TEXT("Invalid amount command is handled"), Command->Execute(InvalidArgs, World, *GLog));
	TestEqual(TEXT("Invalid amount does not change experience"), ExplicitUnit->GetCombatProgressionComponent()->GetExperience(), ExperienceBeforeInvalidAmount);
	return true;
}

#endif
