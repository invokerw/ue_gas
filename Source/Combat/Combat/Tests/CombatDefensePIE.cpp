#include "CoreMinimal.h"
#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Tests/AutomationEditorCommon.h"
#include "Tests/AutomationCommon.h"
#include "Editor.h"
#include "EngineUtils.h"
#include "UnrealClient.h"
#include "NavigationSystem.h"
#include "Combat/Defense/DefenseGameState.h"
#include "Combat/Defense/DefenseObjective.h"
#include "Combat/AI/CombatAIBrainComponent.h"

/** 等待编辑器导航载入结束，避免将仍被加载锁定的导航复制到 PIE。 */
class FDefenseNavReady : public IAutomationLatentCommand
{
public:
    explicit FDefenseNavReady(FAutomationTestBase* T) : Test(T), Started(FPlatformTime::Seconds()) {}
    virtual bool Update() override
    {
        UWorld* W = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
        if (W && !UNavigationSystemV1::IsNavigationBeingBuiltOrLocked(W)) return true;
        if (FPlatformTime::Seconds()-Started>35) { Test->AddError(TEXT("Defense editor navigation not ready")); return true; }
        return false;
    }
private:
    FAutomationTestBase* Test;
    double Started;
};
/** 观察真实 PIE 导航、核心受伤与可读投影；不注入死亡或改写波次。 */
class FDefensePIEProbe : public IAutomationLatentCommand
{
public:
	explicit FDefensePIEProbe(FAutomationTestBase* InTest) : Test(InTest),Started(FPlatformTime::Seconds()) {}
	virtual bool Update() override
	{
		if(FPlatformTime::Seconds()-Started>90) { Test->AddError(TEXT("Defense PIE navigation timeout")); return true; }
		auto* World=GEditor?GEditor->PlayWorld.Get():nullptr;
		auto* State=World?World->GetGameState<ADefenseGameState>():nullptr;
		if(!State) return false;
		const auto& V=State->GetRunView();
		if(V.Phase==EDefensePhase::Defeat) { Test->AddError(TEXT("Unexpected defense startup failure: ")+V.TerminalReason.ToString()); return true; }
		for(TActorIterator<ADefenseEnemy> It(World);It;++It)
		{
			if(It->IsA<ADefenseObjective>()) continue;
			if(const FVector* P=Initial.Find(*It)) bMoved |= FVector::Dist2D(*P,It->GetActorLocation())>300;
			else Initial.Add(*It,It->GetActorLocation());
		}
		if(bMoved && V.ObjectiveHealth>0 && V.ObjectiveHealth<V.ObjectiveMaxHealth)
		{
			Test->TestEqual(TEXT("First real wave"),V.WaveIndex,0);
			Test->TestEqual(TEXT("All two-route spawns resolved"),V.PendingSpawns,0);
			Test->AddInfo(FString::Printf(TEXT("DefensePIE Movement=1 CoreDamage=1 Alive=%d CoreHP=%.0f"),V.RemainingEnemies,V.ObjectiveHealth));
			if(!FParse::Param(FCommandLine::Get(),TEXT("NullRHI")))
				FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("DEF-002/defense-pie.png"),true,false);
			return true;
		}
		return false;
	}
private:
	FAutomationTestBase* Test;
	double Started;
	bool bMoved=false;
	TMap<TWeakObjectPtr<ADefenseEnemy>,FVector> Initial;
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDefensePIETest,"Combat.Defense.PIE.RealMapNavigation",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDefensePIETest::RunTest(const FString&)
{
	ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/Combat/Demo/Defense/L_Defense")));
	FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FDefenseNavReady>(this));
	ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
	FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FDefensePIEProbe>(this));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1));
	return true;
}
#endif
