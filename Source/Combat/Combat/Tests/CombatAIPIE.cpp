#include "CoreMinimal.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "Tests/AutomationCommon.h"
#include "Editor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "NavigationSystem.h"
#include "Combat/AI/CombatAIBrainComponent.h"
#include "Combat/Attack/CombatAttackComponent.h"
#include "Combat/Demo/CombatAIDemoArena.h"
#include "Combat/Order/CombatOrderComponent.h"
#include "Combat/Unit/CombatUnitCharacter.h"

/** 地图切换后先让编辑器完成导航生成，避免 PIE 复制尚未建成的 World Partition 导航。 */
class FCombatAIEditorNavigationReady : public IAutomationLatentCommand
{
public:
	explicit FCombatAIEditorNavigationReady(FAutomationTestBase* InTest) : Test(InTest) {}
	virtual bool Update() override
	{
		if (!Started) Started = FPlatformTime::Seconds();
		auto* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
		if (World && !UNavigationSystemV1::IsNavigationBeingBuiltOrLocked(World)) return true;
		if (FPlatformTime::Seconds() - Started > 30)
		{
			Test->AddError(TEXT("AI PIE editor navigation setup timed out")); return true;
		}
		return false;
	}
private:
	FAutomationTestBase* Test;
	double Started = 0;
};

/** 在真实编辑器 PIE 帧循环观察保存后的演示资产；不手动 Tick Brain 或 Scheduler。 */
class FCombatAIPlaySessionProbe : public IAutomationLatentCommand
{
public:
	explicit FCombatAIPlaySessionProbe(FAutomationTestBase* InTest) : Test(InTest) {}
	virtual bool Update() override
	{
		if (!Started) Started = FPlatformTime::Seconds();
		if (FPlatformTime::Seconds() - Started > 45)
		{
			Test->AddError(TEXT("AI PIE timed out waiting for real StateTree attack/return sequence")); return true;
		}
		UWorld* World = GEditor ? GEditor->PlayWorld.Get() : nullptr;
		if (!World) return false;
		for (TActorIterator<ACombatAIDemoArena> It(World); It; ++It)
		{
			if (!It->Agent) continue;
			auto* Brain = It->Agent->GetCombatAIBrainComponent();
			const float Displacement = FVector::Dist2D(It->Agent->GetActorLocation(), It->GetReturnLocation());
			if (!bReturned && It->Agent->GetCombatAttackComponent()->GetLastFinalizedResult().AppliedDamage > 0 && Displacement > 100)
			{
				Test->TestTrue(TEXT("PIE brain executes saved tree"), Brain->IsRunning());
				Test->AddInfo(FString::Printf(TEXT("AIPIE AttackLanded=1 OutwardDistance=%.1f"), Displacement));
				It->RequestReturn(); bReturned = true;
			}
			if (bReturned && Brain->GetSubmittedCount() >= 2 && Brain->GetResolvedCount() >= 2)
			{
				Test->TestTrue(TEXT("PIE return completes through navigation/Order"), Brain->GetLastReceipt().Result.bSuccess);
				Test->TestFalse(TEXT("PIE leaves no executing order"), It->Agent->GetCombatOrderComponent()->GetCurrentOrderHandle().IsValid());
				Test->TestTrue(TEXT("PIE physically returns to origin"), Displacement < 100);
				Test->AddInfo(FString::Printf(TEXT("AIPIE Result=Pass Submitted=%llu Resolved=%llu"), Brain->GetSubmittedCount(), Brain->GetResolvedCount()));
				return true;
			}
		}
		return false;
	}
private:
	FAutomationTestBase* Test;
	double Started = 0;
	bool bReturned = false;
};

/** 真实 PIE 会话补足同步 World fixture 无法证明的组件休眠/唤醒与导航帧调度。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatAIPIEAssetTest, "Combat.AI.PIE.PlayableArena", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCombatAIPIEAssetTest::RunTest(const FString& Parameters)
{
	ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/Combat/Demo/AI/L_CombatAI")));
	FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FCombatAIEditorNavigationReady>(this));
	ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
	FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FCombatAIPlaySessionProbe>(this));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.0f));
	return true;
}
#endif
