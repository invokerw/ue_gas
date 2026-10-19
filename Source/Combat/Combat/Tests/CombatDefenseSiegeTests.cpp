#include "CoreMinimal.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Combat/Defense/DefenseData.h"
#include "Combat/Defense/DefenseObjective.h"
#include "Components/StaticMeshComponent.h"
#include "Combat/UI/CombatOverheadWidgetComponent.h"

/** 读取实际关卡引用；内容缺失必须失败，不能用瞬态夹具冒充生产资产。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDefenseSiegeAssetsTest,"Combat.Defense.Siege.AuthoredWaveComposition",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDefenseSiegeAssetsTest::RunTest(const FString&)
{
	auto* Run=LoadObject<UDefenseRunData>(nullptr,TEXT("/Game/Combat/Demo/Defense/DA_DefenseRun.DA_DefenseRun"));
	if(!TestNotNull(TEXT("Production run loads"),Run)) return false;
	FString Error; TestTrue(TEXT("Production plan validates"),Run->ValidateRuntime(Error));
	for(int32 Index=0;Index<Run->Waves.Num();++Index)
	{
		const auto* Wave=Run->Waves[Index].Get(); int32 SiegeCount=0,Total=0; TSet<int32> SiegeRoutes;
		for(const auto& G:Wave->SpawnGroups)
		{
			Total+=G.Count;
			if(G.UnitData->GetPrimaryAssetId()==FPrimaryAssetId(TEXT("CombatUnit"),TEXT("defense_siege")))
			{ SiegeCount+=G.Count; SiegeRoutes.Add(G.RouteIndex); TestFalse(TEXT("Siege is not boss"),G.bBoss); }
		}
		const bool Expected=Index==2||Index==4;
		TestEqual(FString::Printf(TEXT("Wave%d siege count"),Index+1),SiegeCount,Expected?2:0);
		if(Expected)
		{
			TestEqual(TEXT("Both lanes carry one siege"),SiegeRoutes.Num(),2);
			TestEqual(TEXT("Original total enemy count preserved"),Total,Index==2?8:12);
			TestEqual(TEXT("Original gold preserved"),Wave->GoldReward,Index==2?160:240);
			TestEqual(TEXT("Original XP preserved"),Wave->ExperienceReward,Index==2?400:600);
		}
	}
	return true;
}
/** 数值配置与纯表现碰撞边界在冷加载后检查。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDefenseSiegeDefinitionTest,"Combat.Defense.Siege.UnitDefinitionAndVisual",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDefenseSiegeDefinitionTest::RunTest(const FString&)
{
	auto* Data=LoadObject<UCombatUnitData>(nullptr,TEXT("/Game/Combat/Demo/Defense/DA_DefenseSiege.DA_DefenseSiege"));
	auto* Class=LoadClass<ADefenseSiegeEnemy>(nullptr,TEXT("/Game/Combat/Demo/Defense/BP_DefenseSiege.BP_DefenseSiege_C"));
	if(!TestNotNull(TEXT("Siege definition"),Data)||!TestNotNull(TEXT("Siege class"),Class)) return false;
	TestEqual(TEXT("Stable identity"),Data->GetPrimaryAssetId(),FPrimaryAssetId(TEXT("CombatUnit"),TEXT("defense_siege")));
	TestEqual(TEXT("Health650"),Data->BaseStats.MaxHealth,650.f); TestEqual(TEXT("Speed140"),Data->BaseStats.MoveSpeed,140.f);
	TestEqual(TEXT("Attack80"),Data->BaseStats.AttackDamage,80.f); TestEqual(TEXT("Attack interval3"),Data->BaseStats.BaseAttackTime,3.f);
	TestEqual(TEXT("Armor4"),Data->BaseStats.Armor,4.f); TestEqual(TEXT("Point0.65"),Data->BaseAttackPoint,.65f);
	TestEqual(TEXT("No extra gold"),Data->GoldReward,int64(0)); TestEqual(TEXT("No extra XP"),Data->ExperienceReward,0);
	TestNull(TEXT("Director remains sole order writer"),Data->AIProfile.Get()); TestNull(TEXT("Direct ram attack has no projectile"),Data->AttackProjectileData.Get());
	const auto* CDO=Class->GetDefaultObject<ADefenseSiegeEnemy>(); TestTrue(TEXT("Class uses exact unit definition"),CDO->GetUnitData()==Data);
	TestNotNull(TEXT("Siege reuses visible name and health widget"),CDO->GetCombatOverheadWidgetComponent()->GetWidgetClass().Get());
	TArray<UStaticMeshComponent*> Parts; CDO->GetComponents(Parts); bool Body=false,Ram=false;
	for(const auto* Part:Parts)
	{
		TestEqual(TEXT("Visual mesh cannot block navigation"),Part->GetCollisionEnabled(),ECollisionEnabled::NoCollision);
		TestFalse(TEXT("Visual mesh has no nav influence"),Part->CanEverAffectNavigation());
		Body |= Part->GetName()==TEXT("DefenseBody")&&Part->GetMaterial(0)!=nullptr;
		Ram |= Part->GetName()==TEXT("SiegeRam");
	}
	TestTrue(TEXT("Body and ram distinguish enemy"),Body&&Ram); return true;
}
#endif
