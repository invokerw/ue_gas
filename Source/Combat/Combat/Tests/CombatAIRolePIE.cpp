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
#include "NavigationSystem.h"
#include "UnrealClient.h"
#include "Combat/AI/CombatAIBrainComponent.h"
#include "Combat/Attack/CombatAttackComponent.h"
#include "Combat/Demo/CombatAIRoleDemoArena.h"
#include "Combat/Unit/CombatUnitCharacter.h"

/** 编辑器导航就绪后再复制到 PIE；真实引擎帧负责树的所有唤醒。 */
class FCombatAIRoleNavReady : public IAutomationLatentCommand
{
public:
	explicit FCombatAIRoleNavReady(FAutomationTestBase* InTest) : Test(InTest), Started(FPlatformTime::Seconds()) {}
	virtual bool Update() override
	{
		auto* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
		if (World && !UNavigationSystemV1::IsNavigationBeingBuiltOrLocked(World)) return true;
		if (FPlatformTime::Seconds() - Started > 35) { Test->AddError(TEXT("Role editor navigation timed out")); return true; }
		return false;
	}
private:
	FAutomationTestBase* Test;
	double Started;
};

/** 不提交目标、不注入导航回执：观察野怪到家和小兵真正打完继续路线的完整轨迹。 */
class FCombatAIRolePIEProbe : public IAutomationLatentCommand
{
public:
	explicit FCombatAIRolePIEProbe(FAutomationTestBase* InTest) : Test(InTest) {}
	virtual bool Update() override
	{
		if (!Started) Started = FPlatformTime::Seconds();
		if (FPlatformTime::Seconds() - Started > 65) { Test->AddError(TEXT("Role PIE timed out waiting for guard return and lane continuation")); return true; }
		auto* World = GEditor ? GEditor->PlayWorld.Get() : nullptr;
		if (!World) return false;
		for (TActorIterator<ACombatAIRoleDemoArena> It(World); It; ++It)
		{
			if (!IsValid(It->GuardAgent) || !IsValid(It->LaneAgent)) continue;
			auto* Guard = It->GuardAgent->GetCombatAIBrainComponent();
			auto* Lane = It->LaneAgent->GetCombatAIBrainComponent();
			bGuardMoved |= FVector::Dist2D(It->GuardAgent->GetActorLocation(), It->GetGuardHome()) > 100 && Guard->IsRunning();
			bLaneMoved |= FVector::Dist2D(It->LaneAgent->GetActorLocation(), It->GetLaneStart()) > 100;
			bGuardHit |= It->GuardAgent->GetCombatAttackComponent()->GetLastFinalizedResult().AppliedDamage > 0;
			bLaneHit |= It->LaneAgent->GetCombatAttackComponent()->GetLastFinalizedResult().AppliedDamage > 0;
			if (!bScreenshot && bGuardHit && bLaneHit && !FParse::Param(FCommandLine::Get(), TEXT("NullRHI")))
			{
				FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir() / TEXT("AI-003/roles-pie.png"), true, false);
				bScreenshot = true;
			}
			if (Guard->GetHomeCommitCount() > 0 && Lane->GetRouteCommitCount() > 0)
			{
				Test->TestTrue(TEXT("Real navigation moves both roles"), bGuardMoved && bLaneMoved);
				Test->TestTrue(TEXT("Both roles actually land attacks"), bGuardHit && bLaneHit);
				Test->TestTrue(TEXT("Guard physically returns"), FVector::Dist2D(It->GuardAgent->GetActorLocation(), It->GetGuardHome()) < 100);
				Test->TestTrue(TEXT("Lane retains route progress after combat"), Lane->GetRouteCursor() == 1 && Lane->GetSubmittedCount() >= 3);
				Test->AddInfo(FString::Printf(TEXT("AIRolesPIE GuardMoved=%d LaneMoved=%d GuardHit=%d LaneHit=%d HomeCommits=%llu RouteCommits=%llu GuardOrders=%llu LaneOrders=%llu"),
					bGuardMoved, bLaneMoved, bGuardHit, bLaneHit, Guard->GetHomeCommitCount(), Lane->GetRouteCommitCount(), Guard->GetSubmittedCount(), Lane->GetSubmittedCount()));
				return true;
			}
		}
		return false;
	}
private:
	FAutomationTestBase* Test;
	double Started = 0;
	bool bGuardMoved = false, bLaneMoved = false, bGuardHit = false, bLaneHit = false, bScreenshot = false;
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatAIRolePIETest, "Combat.AI.PIE.RolesArena", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCombatAIRolePIETest::RunTest(const FString& Parameters)
{
	ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/Combat/Demo/AI/Roles/L_CombatAI_Roles")));
	FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FCombatAIRoleNavReady>(this));
	ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
	FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FCombatAIRolePIEProbe>(this));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1));
	return true;
}
#endif
