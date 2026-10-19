#include "CoreMinimal.h"
#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "Tests/AutomationCommon.h"
#include "Editor.h"
#include "EngineUtils.h"
#include "UnrealClient.h"
#include "Misc/Paths.h"
#include "NavigationSystem.h"
#include "Combat/Defense/DefenseGameState.h"
#include "Combat/Defense/DefenseGameMode.h"
#include "Combat/Defense/DefenseDirectorComponent.h"
#include "Combat/Defense/DefenseObjective.h"
#include "Combat/Combat/CombatDamageSubsystem.h"
#include "Combat/Log/CombatEventSubsystem.h"
#include "Combat/Core/CombatTags.h"
#include "CombatPlayerController.h"

/** 必须在复制Editor World为PIE前等导航解锁；PIE内等待不能修复已复制的锁定导航。 */
class FDefenseSiegeNavReady : public IAutomationLatentCommand
{
public:
	explicit FDefenseSiegeNavReady(FAutomationTestBase* In):Test(In),Started(FPlatformTime::Seconds()) {}
	virtual bool Update() override
	{
		auto* World=GEditor?GEditor->GetEditorWorldContext().World():nullptr;
		if(World&&!UNavigationSystemV1::IsNavigationBeingBuiltOrLocked(World)) return true;
		if(FPlatformTime::Seconds()-Started>35) { Test->AddError(TEXT("Siege editor navigation not ready")); return true; }
		return false;
	}
private:
	FAutomationTestBase* Test;
	double Started;
};
/** 等待实际导航就绪，然后测试仅清理前两波，第三波完整使用生产出生/路线/攻击。 */
class FDefenseSiegePIEProbe : public IAutomationLatentCommand
{
public:
	explicit FDefenseSiegePIEProbe(FAutomationTestBase* In):Test(In),Started(FPlatformTime::Seconds()) {}
	virtual ~FDefenseSiegePIEProbe() override { if(Events.IsValid()) Events->OnRecord().Remove(RecordHandle); }
	virtual bool Update() override
	{
		if(FPlatformTime::Seconds()-Started>180) { Test->AddError(TEXT("Siege PIE timeout waiting for two natural core attacks")); return true; }
		auto* World=GEditor?GEditor->PlayWorld.Get():nullptr; if(!World) return false;
		auto* State=World->GetGameState<ADefenseGameState>(); if(!State||!State->GetRunView().RunId.IsValid()) return false;
		const auto& V=State->GetRunView();
		if(!Events.IsValid())
		{
			Events=World->GetSubsystem<UCombatEventSubsystem>();
			RecordHandle=Events->OnRecord().AddLambda([this](const FCombatLogRecord& R)
			{
				if(R.EventType==CombatTags::Event_Combat_DamageApplied&&R.Source.DirectSourceType==ECombatDirectSourceType::Attack
					&&R.AppliedAmount>0&&R.TargetActorId==CoreId&&Initial.Contains(R.SourceActorId)) CoreHit.Add(R.SourceActorId);
			});
		}
		if(V.Objective) CoreId=V.Objective->GetUniqueID();
		if(V.bWaitingForStart)
		{
			if(UNavigationSystemV1::IsNavigationBeingBuiltOrLocked(World)) return false;
			FString Error; Test->TestTrue(TEXT("Start real map"),World->GetAuthGameMode<ADefenseGameMode>()->GetDefenseDirector()->StartWithSeed(TEXT("831907"),Error)); return false;
		}
		if(V.Phase==EDefensePhase::Defeat) { Test->AddError(TEXT("Unexpected defeat: ")+V.TerminalReason.ToString()); return true; }
		if(V.WaveIndex<2&&V.Phase==EDefensePhase::WaveCombat)
		{
			TArray<TWeakObjectPtr<ADefenseEnemy>> Targets;
			for(TActorIterator<ADefenseEnemy> It(World);It;++It) if(!It->IsA<ADefenseObjective>()&&It->GetLifeState()==ECombatLifeState::Alive) Targets.Add(*It);
			for(const auto& Weak:Targets) if(auto* Unit=Weak.Get())
			{ FCombatDamageRequest R; R.Source=Unit; R.Target=Unit; R.Amount=100000; R.DamageType=ECombatDamageType::Pure; World->GetSubsystem<UCombatDamageSubsystem>()->DealDamage(R); }
			return false;
		}
		if(V.WaveIndex!=2) return false;
		for(TActorIterator<ADefenseSiegeEnemy> It(World);It;++It)
		{
			const int32 Id=It->GetUniqueID();
			if(const auto* P=Initial.Find(Id)) { if(FVector::Dist2D(*P,It->GetActorLocation())>300) Moved.Add(Id); }
			else Initial.Add(Id,It->GetActorLocation());
		}
		if(Initial.Num()==2&&Moved.Num()==2&&CoreHit.Num()==2)
		{
			// 首次截图相机仍在英雄处，核心在视口外；只移动本地相机，不改变战斗单位位置。
			if(CaptureAfter==0)
			{
				auto* PC=Cast<ACombatPlayerController>(World->GetFirstPlayerController());
				if(!PC||!V.Objective) { Test->AddError(TEXT("Cannot frame siege screenshot")); return true; }
				Test->TestTrue(TEXT("Frame core using local camera API"),PC->RelocateCameraFromMinimap(FVector2D(V.Objective->GetActorLocation())));
				CaptureAfter=FPlatformTime::Seconds()+.5; return false;
			}
			if(FPlatformTime::Seconds()<CaptureAfter) return false;
			Test->TestEqual(TEXT("All actual wave3 spawns resolved"),V.PendingSpawns,0);
			Test->TestTrue(TEXT("Core naturally lost health"),V.ObjectiveHealth<V.ObjectiveMaxHealth&&V.ObjectiveHealth>0);
			Test->AddInfo(TEXT("SiegePIE ProductionWave=3 Siege=2 Moved=2 NaturalCoreAttackers=2 EarlierWavesClearedByFixture=1"));
			FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("DEF-010/siege-pie.png"),true,false);
			return true;
		}
		return false;
	}
private:
	FAutomationTestBase* Test;
	double Started;
	double CaptureAfter=0;
	int32 CoreId=0;
	TWeakObjectPtr<UCombatEventSubsystem> Events;
	FDelegateHandle RecordHandle;
	TMap<int32,FVector> Initial;
	TSet<int32> Moved,CoreHit;
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDefenseSiegePIETest,"Combat.Defense.PIE.SiegeRouteAndCoreAttack",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDefenseSiegePIETest::RunTest(const FString&)
{
	ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/Combat/Demo/Defense/L_Defense")));
	FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FDefenseSiegeNavReady>(this));
	ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
	FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FDefenseSiegePIEProbe>(this));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1)); return true;
}
#endif
