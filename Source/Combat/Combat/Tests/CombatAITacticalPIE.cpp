#include "CoreMinimal.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "Combat/AI/CombatAIBrainComponent.h"
#include "Combat/AI/CombatAIWorldSubsystem.h"
#include "Combat/Attributes/CombatAttributeSet.h"
#include "Combat/Demo/CombatAITacticalDemoArena.h"
#include "Combat/Unit/CombatUnitCharacter.h"
#include "Editor.h"
#include "EngineUtils.h"
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "NavigationSystem.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "UnrealClient.h"

/** 编辑器导航完成后才创建 PIE World，避免复制尚未生成的导航数据。 */
class FCombatAITacticalNavReady : public IAutomationLatentCommand
{
public:
	explicit FCombatAITacticalNavReady(FAutomationTestBase* InTest)
		: Test(InTest), Started(FPlatformTime::Seconds()) {}
	virtual bool Update() override
	{
		auto* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
		if (World && !UNavigationSystemV1::IsNavigationBeingBuiltOrLocked(World)) return true;
		if (FPlatformTime::Seconds() - Started > 35.0)
		{
			Test->AddError(TEXT("Tactics editor navigation timed out"));
			return true;
		}
		return false;
	}
private:
	FAutomationTestBase* Test;
	double Started;
};

/** 只观察真实 PIE 帧中的攻击边界、施法、EQS、导航和伤害，不注入 Order 或完成回执。 */
class FCombatAITacticalPIEProbe : public IAutomationLatentCommand
{
public:
	explicit FCombatAITacticalPIEProbe(FAutomationTestBase* InTest) : Test(InTest) {}
	virtual bool Update() override
	{
		if (Started == 0.0) Started = FPlatformTime::Seconds();
		if (FPlatformTime::Seconds() - Started > 70.0)
		{
			Test->AddError(TEXT("Tactics PIE timed out waiting for cast and ranged reposition"));
			return true;
		}
		auto* World = GEditor ? GEditor->PlayWorld.Get() : nullptr;
		if (!World) return false;
		for (TActorIterator<ACombatAITacticalDemoArena> It(World); It; ++It)
		{
			if (!IsValid(It->HeroAgent.Get()) || !IsValid(It->RangedAgent.Get())
				|| !IsValid(It->HeroTarget.Get()) || !IsValid(It->RangedTarget.Get()))
			{
				continue;
			}
			const auto* HeroBrain = It->HeroAgent->GetCombatAIBrainComponent();
			const auto* RangedBrain = It->RangedAgent->GetCombatAIBrainComponent();
			const UCombatAttributeSet* HeroAttributes = It->HeroAgent->GetCombatAttributeSet();
			const UCombatAttributeSet* HeroTargetAttributes = It->HeroTarget->GetCombatAttributeSet();
			const UCombatAttributeSet* RangedTargetAttributes = It->RangedTarget->GetCombatAttributeSet();
			const bool bHeroHealed = It->WasHeroInjuryApplied()
				&& HeroAttributes->GetHealth() > It->GetHeroHealthAfterInjury();
			const bool bRangedMoved = FVector::Dist2D(It->RangedAgent->GetActorLocation(), It->GetRangedStart()) > 300.0f;
			const bool bTargetsDamaged = HeroTargetAttributes->GetHealth() < HeroTargetAttributes->GetMaxHealth()
				&& RangedTargetAttributes->GetHealth() < RangedTargetAttributes->GetMaxHealth();
			const FCombatAIWorldBudgetSnapshot Budget = World->GetSubsystem<UCombatAIWorldSubsystem>()->GetSnapshot();
			if (It->GetHeroAttackLaunchCount() > 0 && It->GetHeroCastAfterInjuryCount() > 0 && bHeroHealed
				&& bRangedMoved && bTargetsDamaged && Budget.EQSSucceeded > 0)
			{
				Test->TestTrue(TEXT("Hero and ranged StateTrees remain server-running"),
					HeroBrain->IsRunning() && RangedBrain->IsRunning());
				Test->TestTrue(TEXT("Hero resolves attack and successful active cast"), HeroBrain->GetResolvedCount() >= 2);
				Test->TestTrue(TEXT("Ranged guard submits EQS Move then Attack"), RangedBrain->GetSubmittedCount() >= 2);
				Test->TestEqual(TEXT("Tactical query leaves no active token"), Budget.ActiveEQS, 0);
				if (!bScreenshot && !FParse::Param(FCommandLine::Get(), TEXT("NullRHI")))
				{
					FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir() / TEXT("AI-004/tactics-pie.png"), true, false);
					bScreenshot = true;
				}
				Test->AddInfo(FString::Printf(TEXT("AITacticsPIE Result=Pass HeroLaunches=%llu HeroCasts=%llu HeroHealed=%d RangedMoved=%d EQSSuccess=%llu HeroOrders=%llu RangedOrders=%llu"),
					It->GetHeroAttackLaunchCount(), It->GetHeroCastAfterInjuryCount(), bHeroHealed ? 1 : 0, bRangedMoved ? 1 : 0,
					Budget.EQSSucceeded, HeroBrain->GetSubmittedCount(), RangedBrain->GetSubmittedCount()));
				return true;
			}
		}
		return false;
	}
private:
	FAutomationTestBase* Test;
	double Started = 0.0;
	bool bScreenshot = false;
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatAITacticalPIETest,
	"Combat.AI.PIE.TacticsArena", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCombatAITacticalPIETest::RunTest(const FString& Parameters)
{
	ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/Combat/Demo/AI/Tactics/L_CombatAI_Tactics")));
	FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FCombatAITacticalNavReady>(this));
	ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
	FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FCombatAITacticalPIEProbe>(this));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.0f));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.0f));
	return true;
}
#endif
