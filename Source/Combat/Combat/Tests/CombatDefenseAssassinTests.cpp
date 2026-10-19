#include "CoreMinimal.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Combat/Defense/DefenseData.h"
#include "Combat/AI/CombatAIProfileData.h"
#include "Combat/Defense/DefenseObjective.h"
#include "Components/StaticMeshComponent.h"
#include "Combat/UI/CombatOverheadWidgetComponent.h"

/** 读取实际关卡引用；内容缺失必须失败，不能用瞬态夹具冒充生产资产。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDefenseAssassinAssetsTest,"Combat.Defense.Assassin.AuthoredWaveComposition",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDefenseAssassinAssetsTest::RunTest(const FString&)
{
	auto* Run=LoadObject<UDefenseRunData>(nullptr,TEXT("/Game/Combat/Demo/Defense/DA_DefenseRun.DA_DefenseRun"));
	if(!TestNotNull(TEXT("Production run loads"),Run)) return false;
	FString Error; TestTrue(TEXT("Production plan validates"),Run->ValidateRuntime(Error));
	for(int32 Index=0;Index<Run->Waves.Num();++Index)
	{
		const auto* Wave=Run->Waves[Index].Get(); int32 AssassinCount=0,Total=0; TSet<int32> AssassinRoutes;
		for(const auto& G:Wave->SpawnGroups)
		{
			Total+=G.Count;
			if(G.UnitData->GetPrimaryAssetId()==FPrimaryAssetId(TEXT("CombatUnit"),TEXT("defense_assassin")))
			{ AssassinCount+=G.Count; AssassinRoutes.Add(G.RouteIndex); TestFalse(TEXT("Assassin is not boss"),G.bBoss); }
		}
		const bool Expected=Index==3||Index==4;
		TestEqual(FString::Printf(TEXT("Wave%d assassin count"),Index+1),AssassinCount,Expected?2:0);
		if(Expected)
		{
			TestEqual(TEXT("Both lanes carry one assassin"),AssassinRoutes.Num(),2);
			TestEqual(TEXT("Original total enemy count preserved"),Total,Index==3?10:12);
			TestEqual(TEXT("Original gold preserved"),Wave->GoldReward,Index==3?190:240);
			TestEqual(TEXT("Original XP preserved"),Wave->ExperienceReward,Index==3?500:600);
		}
	}
	return true;
}
/** 数值配置与纯表现碰撞边界在冷加载后检查。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDefenseAssassinDefinitionTest,"Combat.Defense.Assassin.UnitDefinitionAndVisual",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDefenseAssassinDefinitionTest::RunTest(const FString&)
{
	auto* Data=LoadObject<UCombatUnitData>(nullptr,TEXT("/Game/Combat/Demo/Defense/DA_DefenseAssassin.DA_DefenseAssassin"));
	auto* Class=LoadClass<ADefenseAssassinEnemy>(nullptr,TEXT("/Game/Combat/Demo/Defense/BP_DefenseAssassin.BP_DefenseAssassin_C"));
	if(!TestNotNull(TEXT("Assassin definition"),Data)||!TestNotNull(TEXT("Assassin class"),Class)) return false;
	TestEqual(TEXT("Stable identity"),Data->GetPrimaryAssetId(),FPrimaryAssetId(TEXT("CombatUnit"),TEXT("defense_assassin")));
	TestEqual(TEXT("Health320"),Data->BaseStats.MaxHealth,320.f); TestEqual(TEXT("Speed360"),Data->BaseStats.MoveSpeed,360.f);
	TestEqual(TEXT("Attack28"),Data->BaseStats.AttackDamage,28.f); TestEqual(TEXT("Attack interval1.2"),Data->BaseStats.BaseAttackTime,1.2f);
	TestEqual(TEXT("Armor1"),Data->BaseStats.Armor,1.f); TestEqual(TEXT("Point0.2"),Data->BaseAttackPoint,.2f);
	TestEqual(TEXT("No extra gold"),Data->GoldReward,int64(0)); TestEqual(TEXT("No extra XP"),Data->ExperienceReward,0);
	TestNull(TEXT("UnitData cannot start AI before route assignment"),Data->AIProfile.Get()); TestNull(TEXT("Assassin melee attack has no projectile"),Data->AttackProjectileData.Get());
	const auto* CDO=Class->GetDefaultObject<ADefenseAssassinEnemy>(); TestTrue(TEXT("Class uses exact unit definition"),CDO->GetUnitData()==Data);
	TestNotNull(TEXT("Assassin reuses visible name and health widget"),CDO->GetCombatOverheadWidgetComponent()->GetWidgetClass().Get());
	TArray<UStaticMeshComponent*> Parts; CDO->GetComponents(Parts); bool Body=false;
	for(const auto* Part:Parts)
	{
		TestEqual(TEXT("Visual mesh cannot block navigation"),Part->GetCollisionEnabled(),ECollisionEnabled::NoCollision);
		TestFalse(TEXT("Visual mesh has no nav influence"),Part->CanEverAffectNavigation());
		Body |= Part->GetName()==TEXT("DefenseBody")&&Part->GetMaterial(0)!=nullptr;
	}

	TestTrue(TEXT("Visible body distinguishes enemy"),Body);
	auto* Profile=LoadObject<UCombatAIProfileData>(nullptr,TEXT("/Game/Combat/Demo/AI/Defense/DA_DefenseAssassinAI.DA_DefenseAssassinAI"));
	auto* Hero=LoadObject<UCombatUnitData>(nullptr,TEXT("/Game/Combat/Demo/Defense/DA_DefenseHero.DA_DefenseHero"));
	if(!TestNotNull(TEXT("Actual role profile"),Profile)||!TestNotNull(TEXT("Actual hero definition"),Hero)) return false;
	FString Error; TestTrue(TEXT("Role has compiled valid tree"),Profile->ValidateRuntime(Error));
	TestTrue(TEXT("Priority uses real hero identity"),Profile->TargetPriorities.ContainsByPredicate([&](const auto& P){return P.Definition==Hero->GetPrimaryAssetId()&&P.Priority==10;}));
	TestEqual(TEXT("Low health weight100"),Profile->MissingHealthWeight,100.f);
	TestEqual(TEXT("Route only engages heroes"),Profile->RouteEngagementMinPriority,10);
	TestEqual(TEXT("Eight second engagement limit"),Profile->MaxEngagementSeconds,8.f);
	TestEqual(TEXT("Bounded duty1600"),Profile->LeashDistance,1600.f);
	TestEqual(TEXT("Perception radius1200"),Profile->Perception.Radius,1200.f);
	TestTrue(TEXT("LOS permission required"),Profile->Perception.bRequireLineOfSight);
	TestFalse(TEXT("Lost target resumes route"),Profile->bReturnAfterCombat);
	return true;
}
#endif
