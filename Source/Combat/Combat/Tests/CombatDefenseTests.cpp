#include "CoreMinimal.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Combat/Defense/DefenseData.h"
#include "Combat/Defense/DefensePlayerController.h"
#include "Combat/AI/CombatAIProfileData.h"
#include "Combat/Defense/DefenseAugmentData.h"
#include "Combat/Defense/DefenseOfferSampler.h"
#include "Combat/Defense/DefenseDirectorComponent.h"
#include "Combat/Defense/DefenseGameState.h"
#include "Combat/Defense/DefenseObjective.h"
#include "Combat/Tests/CombatAutomationWorldFixture.h"
#include "Combat/Combat/CombatDamageSubsystem.h"
#include "Combat/Unit/CombatUnitLifecycleComponent.h"
#include "Combat/Unit/CombatProgressionComponent.h"
#include "Combat/Modifiers/CombatModifierComponent.h"
#include "Combat/Log/CombatEventSubsystem.h"
#include "Combat/Economy/CombatEconomyComponent.h"
#include "Combat/Economy/CombatEconomyData.h"
#include "Combat/Economy/CombatShopData.h"
#include "Combat/Items/CombatItemData.h"
#include "Combat/Scheduling/CombatSchedulerSubsystem.h"
#include "CombatPlayerController.h"
#include "Engine/World.h"

/** 测试访问只编译进 Automation；注入登记成功的敌人，导航由真实地图 smoke 独立验证。 */
struct FDefenseTestAccess
{
	static void Advance(UDefenseDirectorComponent* D, FGuid Run, uint32 Gen)
	{ FCombatScheduledTickContext T; T.ActualTime=D->GetWorld()->GetTimeSeconds(); D->Advance(T,Run,Gen); }
	static void Register(UDefenseDirectorComponent* D,ACombatUnitCharacter* U)
	{ D->RegisterEnemy(U); D->View.PendingSpawns=0; }
	static void Check(UDefenseDirectorComponent* D)
	{ FCombatScheduledTickContext T; T.ActualTime=D->GetWorld()->GetTimeSeconds(); D->Monitor(T); }
	static void DuplicateDeath(UDefenseDirectorComponent* D,ACombatUnitCharacter* U)
	{ D->OnEnemyDied(U,FCombatEventContext()); }
	static void Rewards(UDefenseDirectorComponent* D) { D->RewardWave(); }
	static int32 Pending(UDefenseDirectorComponent* D,ACombatPlayerController* PC)
	{ return D->Players.FindChecked(PC).PendingExperience; }
	static bool Work(UDefenseDirectorComponent* D)
	{ return D->PhaseSchedule.IsValid() || D->MonitorSchedule.IsValid(); }
	static void OpenEvent(UDefenseDirectorComponent* D,int32 Wave) { D->OpenEventAfterWave(Wave); }
	static bool ResolveEvent(UDefenseDirectorComponent* D,EDefenseEventOption Option,EDefenseEventState State) { return D->ResolveEvent(Option,State,nullptr); }
	static const FDefenseEventView& Event(UDefenseDirectorComponent* D) { return D->EventView; }
	static const FDefenseRiskLedger& Risk(UDefenseDirectorComponent* D) { return D->RiskLedger; }
	static void ExpireEvent(UDefenseDirectorComponent* D) { FCombatScheduledTickContext T; T.ActualTime=D->EventView.Deadline+.01; D->Advance(T,D->View.RunId,D->GetPhaseGeneration()); }
	static bool RiskReward(UDefenseDirectorComponent* D,int32 Wave) { return D->RewardRiskEvent(Wave); }
	static float BossBonus(UDefenseDirectorComponent* D) { return D->EarnedBossRewardBonus; }
};
namespace DefenseTests
{
	constexpr auto Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;
	/** fixture 不依赖真实导航，不改变 CDO；所有世界对象由作用域清理。 */
	struct FFixture
	{
		FCombatAutomationWorldFixture WorldFixture;
		UWorld* W;
		AActor* Owner;
		UDefenseRunData* Run;
		ADefenseObjective* Core;
		ADefenseGameState* State;
		UDefenseDirectorComponent* D;
		FFixture(ENetMode Mode=NM_Standalone) : WorldFixture(Mode)
		{
			W=WorldFixture.GetWorld(); Owner=W->SpawnActor<AActor>();
			Run=UDefenseRunData::CreatePhase1Default(Owner);
			Core=W->SpawnActor<ADefenseObjective>(); Core->InitializeFromUnitData(Run->ObjectiveData);
			State=W->SpawnActor<ADefenseGameState>();
			D=NewObject<UDefenseDirectorComponent>(Owner); Owner->AddInstanceComponent(D); D->RegisterComponent();
			if(!Owner->HasActorBegunPlay()) Owner->DispatchBeginPlay();
			if(Mode!=NM_Client && !Core->HasActorBegunPlay()) Core->DispatchBeginPlay();
		}
		bool Start() { return D->InitializeForRun(Run,Core,State); }
		void Advance() { FDefenseTestAccess::Advance(D,State->GetRunView().RunId,D->GetPhaseGeneration()); }
		/** 用固定定义初始化并登记，不绕过真实死亡结算。 */
		ACombatUnitCharacter* Enemy()
		{
			FActorSpawnParameters P; P.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			auto* U=W->SpawnActor<ACombatUnitCharacter>(FVector(500,0,100),FRotator::ZeroRotator,P);
			U->InitializeFromUnitData(Run->Waves[D->GetCurrentWaveIndex()]->SpawnGroups[0].UnitData);
			if(!U->HasActorBegunPlay()) U->DispatchBeginPlay();
			FDefenseTestAccess::Register(D,U); return U;
		}
		/** 致死测试走真实 Damage -> GE -> Lifecycle，不直接写 Health。 */
		void Kill(ACombatUnitCharacter* U)
		{
			FCombatDamageRequest R; R.Source=U; R.Target=U; R.Amount=100000; R.DamageType=ECombatDamageType::Pure;
			W->GetSubsystem<UCombatDamageSubsystem>()->DealDamage(R);
		}
		void ToBoss()
		{
			Advance();
			for(int32 I=0;I<5;++I)
			{
				Kill(Enemy()); Advance();
				if(D->GetPhase()==EDefensePhase::EventChoice)
					FDefenseTestAccess::ResolveEvent(D,EDefenseEventOption::Safe,EDefenseEventState::ResolvedSafe);
				Advance();
			}
		}
	};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDefenseDataTest,"Combat.Defense.DataValidationAndDefaultWavePlan",DefenseTests::Flags)
bool FDefenseDataTest::RunTest(const FString&)
{
	auto* R=UDefenseRunData::CreatePhase1Default(GetTransientPackage()); FString E;
	TestTrue(TEXT("Six nonempty waves valid"),R->ValidateRuntime(E));
	R->Waves.Last()->SpawnGroups[0].Count=0;
	TestFalse(TEXT("Empty Boss rejected"),R->ValidateRuntime(E)); R->Waves.Last()->SpawnGroups[0].Count=1;
	R->Waves[0]->SpawnGroups[0].RouteIndex=2;
	TestFalse(TEXT("Missing route rejected"),R->ValidateRuntime(E)); R->Waves[0]->SpawnGroups[0].RouteIndex=0;
	R->Routes[0].SpawnLocation.X=std::numeric_limits<double>::infinity();
	TestFalse(TEXT("Infinite coordinate rejected"),R->ValidateRuntime(E)); R->Routes[0].SpawnLocation.X=-1000;
	R->Waves[0]->SpawnGroups[0].UnitData->GoldReward=1;
	TestFalse(TEXT("Last-hit reward duplication rejected"),R->ValidateRuntime(E));
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDefenseGenerationTest,"Combat.Defense.FrozenPlanAndStalePhase",DefenseTests::Flags)
bool FDefenseGenerationTest::RunTest(const FString&)
{
	DefenseTests::FFixture F; if(!TestTrue(TEXT("Start"),F.Start())) return false;
	const auto Id=F.State->GetRunView().RunId; const uint32 Old=F.D->GetPhaseGeneration();
	F.Run->Waves[0]->SpawnGroups[0].Count=30;
	TestEqual(TEXT("Running plan frozen"),F.D->GetFrozenRunData()->Waves[0]->SpawnGroups[0].Count,1);
	F.Advance(); TestEqual(TEXT("Starts wave"),F.D->GetPhase(),EDefensePhase::WaveCombat);
	FDefenseTestAccess::Advance(F.D,Id,Old);
	TestEqual(TEXT("Old phase cannot skip wave"),F.D->GetCurrentWaveIndex(),0);
	FDefenseTestAccess::Advance(F.D,FGuid::NewGuid(),F.D->GetPhaseGeneration());
	TestEqual(TEXT("Wrong RunId ignored"),F.D->GetActiveEnemyCount(),0);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDefenseDeathTest,"Combat.Defense.ObjectiveDeathExactlyOnce",DefenseTests::Flags)
bool FDefenseDeathTest::RunTest(const FString&)
{
	DefenseTests::FFixture F; if(!F.Start()) return false;
	F.Kill(F.Core); TestEqual(TEXT("Core Damage triggers defeat"),F.D->GetPhase(),EDefensePhase::Defeat);
	const auto G=F.D->GetPhaseGeneration(); F.Kill(F.Core); F.Advance();
	TestEqual(TEXT("No second terminal generation"),F.D->GetPhaseGeneration(),G);
	TestFalse(TEXT("No schedules after defeat"),FDefenseTestAccess::Work(F.D));
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDefenseSimultaneousTest,"Combat.Defense.BossAndCoreSameBatchDefeat",DefenseTests::Flags)
bool FDefenseSimultaneousTest::RunTest(const FString&)
{
	DefenseTests::FFixture F; if(!F.Start()) return false; F.ToBoss();
	TestEqual(TEXT("Sixth wave"),F.D->GetCurrentWaveIndex(),5);
	F.Kill(F.Enemy()); TestFalse(TEXT("Boss death only queues completion"),F.D->GetTerminalResult().bTerminal);
	F.Kill(F.Core); F.Advance();
	TestEqual(TEXT("Core wins same-batch priority"),F.D->GetPhase(),EDefensePhase::Defeat);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDefenseVictoryTest,"Combat.Defense.SixWaveVictoryAndDuplicateDeath",DefenseTests::Flags)
bool FDefenseVictoryTest::RunTest(const FString&)
{
	DefenseTests::FFixture F; if(!F.Start()) return false; F.ToBoss(); auto* Boss=F.Enemy();
	F.Kill(Boss); FDefenseTestAccess::DuplicateDeath(F.D,Boss);
	TestEqual(TEXT("Death counted once"),F.D->GetActiveEnemyCount(),0); F.Advance();
	TestTrue(TEXT("Legal Boss clear wins"),F.D->GetTerminalResult().bVictory);
	TestTrue(TEXT("Owned Boss cleaned"),Boss->IsActorBeingDestroyed());
	TestFalse(TEXT("Terminal work cleared"),FDefenseTestAccess::Work(F.D));
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDefenseSpawnFailureTest,"Combat.Defense.SpawnFailureCannotWinOrStall",DefenseTests::Flags)
bool FDefenseSpawnFailureTest::RunTest(const FString&)
{
	DefenseTests::FFixture F; F.Run->SpawnRetryLimit=1; if(!F.Start()) return false;
	F.Advance(); F.Advance(); F.Advance();
	TestEqual(TEXT("Missing navigation exhausts retries"),F.D->GetPhase(),EDefensePhase::Defeat);
	TestEqual(TEXT("Actionable reason"),F.D->GetTerminalResult().Reason,FName(TEXT("SpawnFailed")));
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDefenseEndPlayTest,"Combat.Defense.EnemyEndPlayAndLifeGeneration",DefenseTests::Flags)
bool FDefenseEndPlayTest::RunTest(const FString&)
{
	{
		DefenseTests::FFixture F; if(!F.Start()) return false; F.Advance(); auto* U=F.Enemy(); U->Destroy();
		FDefenseTestAccess::Check(F.D);
		TestFalse(TEXT("Destroy is not kill"),F.D->GetTerminalResult().bVictory);
		TestEqual(TEXT("Lost enemy fails"),F.D->GetPhase(),EDefensePhase::Defeat);
	}
	{
		DefenseTests::FFixture F; if(!F.Start()) return false; F.Advance(); auto* U=F.Enemy(); F.Kill(U);
		TestTrue(TEXT("Respawn creates new life"),U->GetCombatLifecycleComponent()->RespawnAtLocation(FVector(500,0,100)));
		F.Advance(); TestEqual(TEXT("Old life clear rejected"),F.D->GetPhase(),EDefensePhase::Defeat);
	}
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDefenseTeardownTest,"Combat.Defense.OwnerAndWorldTeardown",DefenseTests::Flags)
bool FDefenseTeardownTest::RunTest(const FString&)
{
	DefenseTests::FFixture F; if(!F.Start()) return false;
	F.Advance(); auto* U=F.Enemy(); F.Owner->Destroy();
	TestFalse(TEXT("Owner EndPlay clears schedules"),FDefenseTestAccess::Work(F.D));
	TestTrue(TEXT("Owned enemy destroyed"),U->IsActorBeingDestroyed());
	F.D->Shutdown();
	F.W->TimeSeconds=50; F.W->GetSubsystem<UCombatSchedulerSubsystem>()->RunDueTasks(50);
	TestFalse(TEXT("No late terminal result"),F.D->GetTerminalResult().bTerminal);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDefenseClientTest,"Combat.Defense.ClientAndCrossWorldRejected",DefenseTests::Flags)
bool FDefenseClientTest::RunTest(const FString&)
{
	DefenseTests::FFixture Client(NM_Client);
	TestFalse(TEXT("Client cannot initialize"),Client.Start());
	DefenseTests::FFixture A,B;
	TestFalse(TEXT("Cross world core rejected"),A.D->InitializeForRun(A.Run,B.Core,A.State));
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDefenseTimeoutTest,"Combat.Defense.TimeoutAndCoreLossDuringPreparation",DefenseTests::Flags)
bool FDefenseTimeoutTest::RunTest(const FString&)
{
	{ DefenseTests::FFixture F; if(!F.Start()) return false; F.Advance(); F.W->TimeSeconds=1000; FDefenseTestAccess::Check(F.D);
	TestEqual(TEXT("Timeout fails"),F.D->GetTerminalResult().Reason,FName(TEXT("WaveTimeout"))); }
	{ DefenseTests::FFixture F; if(!F.Start()) return false; F.Core->Destroy(); FDefenseTestAccess::Check(F.D);
	TestEqual(TEXT("Preparation core disappearance fails"),F.D->GetPhase(),EDefensePhase::Defeat); }
	return true;
}
/** 奖励按玩家/波次去重，死亡经验和固定强化在复活后领取。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDefenseRewardsTest,"Combat.Defense.PlayerRewardsDeathAndDisconnect",DefenseTests::Flags)
bool FDefenseRewardsTest::RunTest(const FString&)
{
	DefenseTests::FFixture F;
	auto* Mod=NewObject<UCombatModifierData>(F.Run); Mod->DefinitionName=TEXT("defense_fixture_armor");
	Mod->bRemoveOnDeath=false; Mod->MaxStacks=1;
	FCombatModifierAttributeChange Change; Change.Attribute=UCombatAttributeSet::GetArmorAttribute(); Change.Magnitude=3;
	Mod->AttributeChanges.Add(Change); F.Run->Waves[0]->FixedAugment=Mod;
	if(!F.Start()) return false;
	TArray<ACombatPlayerController*> Players;
	TArray<ACombatUnitCharacter*> Heroes;
	for(int32 I=0;I<2;++I)
	{
		auto* PC=F.W->SpawnActor<ACombatPlayerController>();
		auto* Hero=F.W->SpawnActor<ACombatUnitCharacter>();
		auto* Data=NewObject<UCombatUnitData>(Hero); Data->DefinitionName=FName(FString::Printf(TEXT("defense_hero_%d"),I));
		Hero->InitializeFromUnitData(Data); if(!Hero->HasActorBegunPlay()) Hero->DispatchBeginPlay();
		if(!TestTrue(TEXT("Bind hero"),PC->SetCommandedUnitAuthority(Hero))) return false;
		auto* Economy=NewObject<UCombatEconomyData>(PC); Economy->DefinitionName=TEXT("defense_fixture_economy"); Economy->PassiveGoldPerMinute=0;
		auto* Shop=NewObject<UCombatShopData>(PC); Shop->DefinitionName=TEXT("defense_fixture_shop");
		auto* Item=NewObject<UCombatItemData>(Shop); Item->DefinitionName=TEXT("defense_fixture_item"); Item->bPurchasable=true; Item->PurchasePrice=10;
		auto& Category=Shop->Categories.AddDefaulted_GetRef(); Category.CategoryId=TEXT("basics"); Category.DisplayName=FText::FromString(TEXT("基础")); Category.Items.Add(Item);
		FString Error;
		if(!TestTrue(TEXT("Economy ready"),PC->GetCombatEconomyComponent()->InitializeForMatch(Economy,Shop,false,Error))) return false;
		TestTrue(TEXT("Register opening player"),F.D->RegisterPlayer(PC,Hero));
		Players.Add(PC); Heroes.Add(Hero);
	}
	const auto Gold=Players[0]->GetCombatEconomyComponent()->GetGold();
	F.Advance(); F.Kill(Heroes[1]); F.Kill(F.Enemy());
	FDefenseTestAccess::Rewards(F.D); FDefenseTestAccess::Rewards(F.D);
	for(auto* PC:Players) TestEqual(TEXT("Gold credited once per player"),PC->GetCombatEconomyComponent()->GetGold(),Gold+100);
	TestEqual(TEXT("Living hero XP"),Heroes[0]->GetCombatProgressionComponent()->GetExperience(),int64(200));
	TestEqual(TEXT("Dead hero XP pending"),FDefenseTestAccess::Pending(F.D,Players[1]),200);
	TestEqual(TEXT("Dead hero no early XP"),Heroes[1]->GetCombatProgressionComponent()->GetExperience(),int64(0));
	TestEqual(TEXT("Living fixed buff once"),Heroes[0]->GetCombatModifierComponent()->GetActiveModifierCount(),1);
	TestTrue(TEXT("Public respawn"),Heroes[1]->GetCombatLifecycleComponent()->RespawnAtLocation(FVector(1000,600,100)));
	FDefenseTestAccess::Check(F.D);
	TestEqual(TEXT("Pending XP delivered"),Heroes[1]->GetCombatProgressionComponent()->GetExperience(),int64(200));
	TestEqual(TEXT("Pending buff delivered"),Heroes[1]->GetCombatModifierComponent()->GetActiveModifierCount(),1);
	F.D->RemovePlayer(Players[1]); F.D->RemovePlayer(Players[1]);
	TestEqual(TEXT("Disconnect removes owned augment"),Heroes[1]->GetCombatModifierComponent()->GetActiveModifierCount(),0);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDefensePhase2ThreeBuildBossTest,"Combat.Defense.Phase2.ThreeBuildPlansClearBoss",DefenseTests::Flags)
bool FDefensePhase2ThreeBuildBossTest::RunTest(const FString&)
{
	const TArray<EDefenseAugmentFamily> Plans={EDefenseAugmentFamily::Frost,EDefenseAugmentFamily::Skill,EDefenseAugmentFamily::Guardian};
	auto* Pool=LoadObject<UDefenseAugmentPoolData>(nullptr,TEXT("/Game/Combat/Demo/Defense/Augments/DA_DefensePool.DA_DefensePool"));
	if(!TestNotNull(TEXT("Authored Phase2 pool for build plans"),Pool)) return false;
	for(const auto Family:Plans)
	{
		TSet<FPrimaryAssetId> Owned; const UDefenseAugmentData* Final=nullptr; int32 Base=0;
		for(const auto& Ptr:Pool->Augments){const auto* Card=Ptr.Get(); if(!Card||Card->Family!=Family) continue; if(Card->Rarity==EDefenseAugmentRarity::Final) Final=Card; else if(Base<2){Owned.Add(Card->GetPrimaryAssetId()); ++Base;}}
		TestTrue(TEXT("Family plan has two starter cards"),Base==2); TestNotNull(TEXT("Family plan has finale"),Final);
		if(Final) TestTrue(TEXT("Family plan is Boss-eligible after wave3"),FDefenseOfferSampler::IsEligible(*Final,*Pool,FPrimaryAssetId(TEXT("CombatUnit"),TEXT("phase2_hero")),2,Owned));
		DefenseTests::FFixture F; F.Run->bEnablePrototypeEvents=false; if(!F.Start()) return false;
		F.ToBoss(); auto* Boss=F.Enemy(); F.Kill(Boss); FDefenseTestAccess::DuplicateDeath(F.D,Boss); F.Advance();
		TestTrue(TEXT("Deterministic family plan reaches prototype Boss victory"),F.D->GetTerminalResult().bVictory);
		TestEqual(TEXT("Boss outcome is exactly once"),F.D->GetPhase(),EDefensePhase::Victory);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDefenseEventTest,"Combat.Defense.Event.RiskConfirmationAndExactlyOnce",DefenseTests::Flags)
bool FDefenseEventTest::RunTest(const FString&)
{
	DefenseTests::FFixture F; if(!F.Start()) return false;
	FDefenseTestAccess::OpenEvent(F.D,1);
	TestEqual(TEXT("Second-wave event opens"),F.D->GetPhase(),EDefensePhase::EventChoice);
	TestEqual(TEXT("Vault event has an offer"),FDefenseTestAccess::Event(F.D).State,EDefenseEventState::Offered);
	TestTrue(TEXT("Risk request enters confirmation"),FDefenseTestAccess::ResolveEvent(F.D,EDefenseEventOption::Risk,EDefenseEventState::PendingRisk));
	TestEqual(TEXT("Risk confirmation is cancellable"),FDefenseTestAccess::Event(F.D).State,EDefenseEventState::PendingRisk);
	TestTrue(TEXT("Risk confirmation commits once"),FDefenseTestAccess::ResolveEvent(F.D,EDefenseEventOption::Risk,EDefenseEventState::PendingRisk));
	TestEqual(TEXT("Event resumes intermission"),F.D->GetPhase(),EDefensePhase::Intermission);
	TestEqual(TEXT("Risk ledger targets next wave"),FDefenseTestAccess::Risk(F.D).TargetWave,2);
	const auto Generation=F.D->GetPhaseGeneration();
	TestFalse(TEXT("Old event cannot resolve again"),FDefenseTestAccess::ResolveEvent(F.D,EDefenseEventOption::Risk,EDefenseEventState::PendingRisk));
	TestEqual(TEXT("No duplicate phase transition"),F.D->GetPhaseGeneration(),Generation);
	return true;
}


/** 独立请求玩家使用同一编号也不能串回执；危险确认只允许发起人，任意队员可取消。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDefenseEventPlayerIsolationTest,"Combat.Defense.Event.PlayerIsolationAndReplay",DefenseTests::Flags)
bool FDefenseEventPlayerIsolationTest::RunTest(const FString&)
{
 DefenseTests::FFixture F;if(!F.Start()) return false;
 TArray<ACombatPlayerController*> P;
 for(int32 I=0;I<2;++I)
 {
  auto* PC=F.W->SpawnActor<ACombatPlayerController>();auto* Hero=F.W->SpawnActor<ACombatUnitCharacter>();
  auto* Data=NewObject<UCombatUnitData>(Hero);Data->DefinitionName=FName(FString::Printf(TEXT("event_hero_%d"),I));
  Hero->InitializeFromUnitData(Data);if(!Hero->HasActorBegunPlay()) Hero->DispatchBeginPlay();
  TestTrue(TEXT("Bind event hero"),PC->SetCommandedUnitAuthority(Hero));
  auto* Economy=NewObject<UCombatEconomyData>(PC);Economy->DefinitionName=TEXT("event_economy");Economy->PassiveGoldPerMinute=0;
  auto* Shop=NewObject<UCombatShopData>(PC);Shop->DefinitionName=TEXT("event_shop");
  auto* Item=NewObject<UCombatItemData>(Shop);Item->DefinitionName=TEXT("event_item");Item->bPurchasable=true;Item->PurchasePrice=10;
  auto& Category=Shop->Categories.AddDefaulted_GetRef();Category.CategoryId=TEXT("basics");Category.DisplayName=FText::FromString(TEXT("基础"));Category.Items.Add(Item);
  FString Error;TestTrue(TEXT("Event economy ready"),PC->GetCombatEconomyComponent()->InitializeForMatch(Economy,Shop,false,Error));
  TestTrue(TEXT("Register event hero"),F.D->RegisterPlayer(PC,Hero));P.Add(PC);
 }
 FDefenseTestAccess::OpenEvent(F.D,1);
 auto Request=[&](int32 Player,int32 Id,EDefenseEventOption Option)
 {
  FDefenseEventRequest R;R.RequestId=Id;R.RunId=F.State->GetRunView().RunId;R.PhaseGeneration=F.D->GetPhaseGeneration();
  const auto V=F.D->BuildEventView();R.EventId=V.EventId;R.EventRevision=V.Revision;R.BindingGeneration=P[Player]->GetCommandBindingGeneration();R.LifeGeneration=P[Player]->GetCommandedUnit()->GetLifeGeneration();R.Option=Option;return R;
 };
 const auto Original=Request(0,1,EDefenseEventOption::Risk);TestTrue(TEXT("Risk proposed"),F.D->ProcessDefenseEvent(P[0],Original).bSuccess);
 TestTrue(TEXT("Same payload replay accepted"),F.D->ProcessDefenseEvent(P[0],Original).bSuccess);
 auto Changed=Original;Changed.Option=EDefenseEventOption::Safe;
 TestEqual(TEXT("Payload substitution refused"),F.D->ProcessDefenseEvent(P[0],Changed).FailureReason,FName(TEXT("RequestPayloadMismatch")));
 auto ForeignConfirm=Request(1,1,EDefenseEventOption::Risk);
 TestFalse(TEXT("Another player cannot confirm proposer risk"),F.D->ProcessDefenseEvent(P[1],ForeignConfirm).bSuccess);
 auto Cancel=Request(1,2,EDefenseEventOption::Cancel);TestTrue(TEXT("Other player may cancel with independent request sequence"),F.D->ProcessDefenseEvent(P[1],Cancel).bSuccess);
 TestEqual(TEXT("Cancelled resumes once"),F.D->GetPhase(),EDefensePhase::Intermission);
 const auto Generation=F.D->GetPhaseGeneration();TestTrue(TEXT("Cancellation replay uses original receipt"),F.D->ProcessDefenseEvent(P[1],Cancel).bSuccess);TestEqual(TEXT("No duplicate transition"),F.D->GetPhaseGeneration(),Generation);
 FDefenseTestAccess::OpenEvent(F.D,3);auto NewRisk=Request(0,2,EDefenseEventOption::Risk);TestTrue(TEXT("Next risk opens"),F.D->ProcessDefenseEvent(P[0],NewRisk).bSuccess);
 F.D->RemovePlayer(P[0]);TestEqual(TEXT("Proposer disconnect cancels"),F.D->BuildEventView().State,EDefenseEventState::Cancelled);return true;
}
/** 调度截止走同一安全终态，成功风险波只贡献一次可跨事件保留的Boss倍率。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDefenseEventTimeoutRewardTest,"Combat.Defense.Event.TimeoutAndAccumulatedReward",DefenseTests::Flags)
bool FDefenseEventTimeoutRewardTest::RunTest(const FString&)
{
 DefenseTests::FFixture F;if(!F.Start()) return false;
 FDefenseTestAccess::OpenEvent(F.D,1);FDefenseTestAccess::ResolveEvent(F.D,EDefenseEventOption::Risk,EDefenseEventState::PendingRisk);
 FDefenseTestAccess::ExpireEvent(F.D);TestEqual(TEXT("Risk timeout selects safe"),F.D->BuildEventView().State,EDefenseEventState::Expired);
 FDefenseTestAccess::OpenEvent(F.D,1);FDefenseTestAccess::ResolveEvent(F.D,EDefenseEventOption::Risk,EDefenseEventState::PendingRisk);FDefenseTestAccess::ResolveEvent(F.D,EDefenseEventOption::Risk,EDefenseEventState::ResolvedRisk);
 TestTrue(TEXT("Risk wave reward succeeds"),FDefenseTestAccess::RiskReward(F.D,2));TestTrue(TEXT("Repeated risk reward succeeds without extra credit"),FDefenseTestAccess::RiskReward(F.D,2));TestEqual(TEXT("One earned bonus"),FDefenseTestAccess::BossBonus(F.D),.1f);
 FDefenseTestAccess::OpenEvent(F.D,3);TestEqual(TEXT("Opening next event retains prior boss bonus"),FDefenseTestAccess::BossBonus(F.D),.1f);
 FDefenseTestAccess::ResolveEvent(F.D,EDefenseEventOption::Risk,EDefenseEventState::PendingRisk);FDefenseTestAccess::ResolveEvent(F.D,EDefenseEventOption::Risk,EDefenseEventState::ResolvedRisk);FDefenseTestAccess::RiskReward(F.D,4);
 TestEqual(TEXT("Two successful event bonuses accumulate"),FDefenseTestAccess::BossBonus(F.D),.2f);return true;
}

/** 可选角色先校验且本局参数独立冻结，不能让UnitData早启动第二个命令来源。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDefenseRoleProfileTest,"Combat.Defense.Assassin.RoleProfileValidationAndFreeze",DefenseTests::Flags)
bool FDefenseRoleProfileTest::RunTest(const FString&)
{
	DefenseTests::FFixture F;
	auto* Original=LoadObject<UCombatAIProfileData>(nullptr,TEXT("/Game/Combat/Demo/AI/Defense/DA_DefenseAssassinAI.DA_DefenseAssassinAI"));
	if(!TestNotNull(TEXT("Authored assassin role"),Original)) return false;
	auto* P=DuplicateObject<UCombatAIProfileData>(Original,F.Owner); auto& G=F.Run->Waves[0]->SpawnGroups[0]; G.RoleProfile=P;
	FString Error; TestTrue(TEXT("Optional role accepted"),F.Run->ValidateRuntime(Error));
	P->bEnablePerception=false; TestFalse(TEXT("No perception rejected"),F.Run->ValidateRuntime(Error)); P->bEnablePerception=true;
	G.UnitData->AIProfile=P; TestFalse(TEXT("Early second writer rejected"),F.Run->ValidateRuntime(Error)); G.UnitData->AIProfile=nullptr;
	F.Run->Waves.Last()->SpawnGroups[0].RoleProfile=P; TestFalse(TEXT("Boss role rejected"),F.Run->ValidateRuntime(Error)); F.Run->Waves.Last()->SpawnGroups[0].RoleProfile=nullptr;
	if(!TestTrue(TEXT("Run starts"),F.Start())) return false;
	const auto* Frozen=F.D->GetFrozenRunData()->Waves[0]->SpawnGroups[0].RoleProfile.Get();
	TestTrue(TEXT("Config independently owned"),Frozen&&Frozen!=P&&Frozen!=Original);
	P->MissingHealthWeight=0; P->TargetPriorities.Reset();
	TestEqual(TEXT("Running selection weight frozen"),Frozen->MissingHealthWeight,100.f);
	TestEqual(TEXT("Running priority frozen"),Frozen->TargetPriorities.Num(),Original->TargetPriorities.Num());
	F.D->Shutdown(); TestFalse(TEXT("Director schedules cleared"),FDefenseTestAccess::Work(F.D)); return true;
}
/** 开局入口严格限制空构筑；没有导航时保留旧角色与资源，不退化为任意位置出生。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDefenseStartingHeroGateTest,"Combat.Defense.Guardian.SelectionGatesAndSpawnFailure",DefenseTests::Flags)
bool FDefenseStartingHeroGateTest::RunTest(const FString&)
{
	DefenseTests::FFixture F; F.Run->PreparationDuration=40; if(!F.Start()) return false;
	auto* PC=F.W->SpawnActor<ADefensePlayerController>();
	FActorSpawnParameters P; P.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	auto* Old=F.W->SpawnActor<ACombatUnitCharacter>(FVector(1000,0,100),FRotator::ZeroRotator,P);
	auto* Data=NewObject<UCombatUnitData>(Old); Data->DefinitionName=TEXT("guardian_selection_original"); Data->ExperienceReward=0; Old->InitializeFromUnitData(Data);
	if(!Old->HasActorBegunPlay()) Old->DispatchBeginPlay(); TestTrue(TEXT("Bind initial hero"),PC->SetCommandedUnitAuthority(Old));
	TestTrue(TEXT("Register player"),F.D->RegisterPlayer(PC,Old)); TestTrue(TEXT("Fresh prep eligible"),F.D->CanSelectStartingHero(PC));
	auto* Class=LoadClass<ACombatUnitCharacter>(nullptr,TEXT("/Game/Combat/Demo/Defense/Guardian/BP_DefenseGuardian.BP_DefenseGuardian_C")); if(!TestNotNull(TEXT("Guardian whitelist class"),Class)) return false;
	const int64 Gold=PC->GetCombatEconomyComponent()->GetGold(); FName Failure;
	TestFalse(TEXT("Missing navigation safely refuses spawn"),F.D->SelectStartingHero(PC,Class,Failure)); TestEqual(TEXT("Stable spawn failure"),Failure,FName(TEXT("HeroSpawnBlocked")));
	TestTrue(TEXT("Old hero remains commanded and alive"),PC->GetCommandedUnit()==Old&&Old->GetLifeState()==ECombatLifeState::Alive&&!Old->IsActorBeingDestroyed());
	TestEqual(TEXT("No payment or loss"),PC->GetCombatEconomyComponent()->GetGold(),Gold);
	TestFalse(TEXT("Unknown player not eligible"),F.D->CanSelectStartingHero(F.W->SpawnActor<ADefensePlayerController>()));
	Old->GetCombatProgressionComponent()->AddExperience(1); TestFalse(TEXT("Progressed hero cannot be discarded"),F.D->CanSelectStartingHero(PC));
	F.Advance(); TestFalse(TEXT("Combat closes selection"),F.D->CanSelectStartingHero(PC)); return true;
}
#endif
