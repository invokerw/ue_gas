#include "Combat/Tests/CombatDefenseNetworkScenario.h"
#include "Combat/Defense/DefensePlayerController.h"
#include "Combat/Defense/DefenseGameMode.h"
#include "Combat/Defense/DefenseDirectorComponent.h"
#include "Combat/Defense/DefenseGameState.h"
#include "Combat/Defense/DefenseEventComponent.h"
#include "Combat/Defense/DefenseObjective.h"
#include "Combat/Defense/DefenseData.h"
#include "Combat/Economy/CombatEconomyComponent.h"
#include "Combat/Unit/CombatUnitCharacter.h"
#include "EngineUtils.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

void ACombatDefenseNetworkScenario::ExerciseEvents()
{
 auto* State=GetWorld()->GetGameState<ADefenseGameState>();const auto V=State->GetRunView();const auto E=V.Event;
 const double Now=GetWorld()->GetTimeSeconds();
 if(V.SchemaVersion!=4) { Finish(false,TEXT("EventSchemaMismatch"));return; }
 if(V.Phase==EDefensePhase::Defeat) { Finish(false,TEXT("EventRunDefeated"));return; }
 // 属性复制允许合并短暂Offered快照；两个Pending窗口和两个不同终态必须三端均观察到。
 if(E.DefinitionId.PrimaryAssetName==TEXT("defense_event_vault"))
 { if(E.State==EDefenseEventState::Offered) EventNetMask|=1;if(E.State==EDefenseEventState::PendingRisk) EventNetMask|=2;if(E.State==EDefenseEventState::Cancelled) EventNetMask|=4; }
 if(E.DefinitionId.PrimaryAssetName==TEXT("defense_event_oath"))
 { if(E.State==EDefenseEventState::Offered) EventNetMask|=8;if(E.State==EDefenseEventState::PendingRisk) EventNetMask|=16;if(E.State==EDefenseEventState::ResolvedRisk) EventNetMask|=32; }
 if(HasAuthority())
 {
  TArray<ADefensePlayerController*> Players;for(TActorIterator<ADefensePlayerController> It(GetWorld());It;++It) if(It->GetCommandedUnit()) Players.Add(*It);
  if(Players.Num()!=ExpectedPlayers) return;
  if(!bEventNetPrepared)
  {
   // 只改测试开关下当前局深拷贝的整备时长；正式资产不修改，阶段仍由Scheduler推进。
   auto* Run=const_cast<UDefenseRunData*>(GetWorld()->GetAuthGameMode<ADefenseGameMode>()->GetDefenseDirector()->GetFrozenRunData());
   Run->IntermissionDuration=1;Run->ChoiceDuration=1;
   for(UDefenseWaveData* Wave:Run->Waves) for(auto& Group:Wave->SpawnGroups) Group.SpawnInterval=.15f;
   EventNetGold=Players[0]->GetCombatEconomyComponent()->GetGold();bEventNetPrepared=true;
  }
  if(GetNetMode()==NM_Standalone&&V.Phase==EDefensePhase::EventChoice)
  {
   // 打包单机场景也走本地拥有者组件，同一生产请求入口负责校验和结算。
   auto* C=Players[0]->GetEventComponent();
   if(EventNetAt==0) EventNetAt=Now+.5;
   if(Now>=EventNetAt)
   {
    if(E.State==EDefenseEventState::Offered) C->RequestOption(EDefenseEventOption::Risk);
    else if(E.State==EDefenseEventState::PendingRisk) C->RequestOption(V.WaveIndex==1?EDefenseEventOption::Cancel:EDefenseEventOption::Risk);
    EventNetAt=Now+.5;
   }
  }
  if(V.Phase==EDefensePhase::WaveCombat)
  {
   if(EventNetLastWave!=V.WaveIndex) { EventNetLastWave=V.WaveIndex;EventNetWaveAt=Now; }
   if(V.PendingSpawns==0&&Now-EventNetWaveAt>4)
   {
    TArray<ADefenseEnemy*> Enemies;for(TActorIterator<ADefenseEnemy> It(GetWorld());It;++It) if(!It->IsA<ADefenseObjective>()&&It->GetLifeState()==ECombatLifeState::Alive) Enemies.Add(*It);
    for(auto* Enemy:Enemies) Kill(Enemy);
   }
  }
  if(Players.Num()>=2&&V.Phase==EDefensePhase::EventChoice&&E.State==EDefenseEventState::PendingRisk)
  {
   FDefenseEventRequest Invalid;Invalid.RequestId=909;Invalid.RunId=V.RunId;Invalid.EventId=E.EventId;Invalid.PhaseGeneration=V.PhaseGeneration;Invalid.EventRevision=E.Revision;Invalid.BindingGeneration=Players[0]->GetCommandBindingGeneration();Invalid.LifeGeneration=Players[0]->GetCommandedUnit()->GetLifeGeneration();Invalid.Option=EDefenseEventOption::Cancel;
   if(Players[0]->GetEventComponent()->ProcessForConnection(Players[1],Invalid).bSuccess) { Finish(false,TEXT("ForeignOwnerAccepted"));return; }
  }
  if(V.Phase==EDefensePhase::Victory)
  {
   const auto* Run=GetWorld()->GetAuthGameMode<ADefenseGameMode>()->GetDefenseDirector()->GetFrozenRunData();
   int64 Expected=EventNetGold+240+300;for(const UDefenseWaveData* Wave:Run->Waves) Expected+=Wave->GoldReward;
   Expected+=FMath::RoundToInt(Run->Waves.Last()->GoldReward*.1f);
   for(auto* PC:Players) if(PC->GetCombatEconomyComponent()->GetGold()!=Expected) { Finish(false,FString::Printf(TEXT("EventGold actual=%lld expected=%lld"),PC->GetCombatEconomyComponent()->GetGold(),Expected));return; }
   Finish((EventNetMask&54)==54,FString::Printf(TEXT("EventNetwork Schema=4 Players=%d StateMask=%d Gold=%lld OwnerCheck=%s Run=%s"),Players.Num(),EventNetMask,Expected,Players.Num()>=2?TEXT("ForeignRejected"):TEXT("LocalOwner"),*V.RunId.ToString()));
  }
  return;
 }
 int32 Seat=0;FParse::Value(FCommandLine::Get(),TEXT("DefenseSmokeSeat="),Seat);
 auto* PC=Cast<ADefensePlayerController>(GetWorld()->GetFirstPlayerController());if(!PC||!PC->IsLocalController()||!PC->GetCommandedUnit()) return;
 if(PC->GetCommandedUnit()->HasAuthority()) { Finish(false,TEXT("EventClientAuthorityLeak"));return; }
 auto* C=PC->GetEventComponent();if(!C) return;
 auto Submit=[&](int32 Id,EDefenseEventOption Option,bool Replay)
 {
  FDefenseEventRequest R;R.RequestId=Id;R.RunId=V.RunId;R.PhaseGeneration=V.PhaseGeneration;R.EventId=E.EventId;R.EventRevision=E.Revision;R.BindingGeneration=PC->GetCommandBindingGeneration();R.LifeGeneration=PC->GetCommandedUnit()->GetLifeGeneration();R.Option=Option;
  C->ServerSubmitEvent(R);if(Replay) C->ServerSubmitEvent(R);
 };
 if(V.Phase==EDefensePhase::EventChoice&&V.WaveIndex==1)
 {
  if(Seat==1&&EventNetStep==0&&E.State==EDefenseEventState::Offered) { Submit(1,EDefenseEventOption::Risk,true);EventNetStep=1; }
  if(Seat==2&&EventNetStep==0&&E.State==EDefenseEventState::PendingRisk) { EventNetAt=Now+.5;EventNetStep=1; }
  if(Seat==2&&EventNetStep==1&&Now>=EventNetAt&&E.State==EDefenseEventState::PendingRisk) { Submit(1,EDefenseEventOption::Cancel,true);EventNetStep=2; }
 }
 if(E.State==EDefenseEventState::Cancelled&&V.WaveIndex==1) bEventNetReceipt=C->GetLastResult().bSuccess;
 if(V.Phase==EDefensePhase::EventChoice&&V.WaveIndex==3)
 {
  if(Seat==1&&EventNetStep==1&&E.State==EDefenseEventState::Offered) { Submit(2,EDefenseEventOption::Risk,true);EventNetStep=2; }
  if(E.State==EDefenseEventState::PendingRisk)
  {
   if(Seat==1&&EventNetStep==2) { EventNetAt=Now+2;EventNetStep=3; }
   if(Seat==1&&EventNetStep==3&&Now>=EventNetAt) { Submit(3,EDefenseEventOption::Risk,true);EventNetStep=4; }
   if(Seat==2&&EventNetStep==2) { Submit(2,EDefenseEventOption::Risk,false);EventNetStep=3; }
   if(Seat==2&&EventNetStep==3&&C->GetLastResult().RequestId==2) { if(C->GetLastResult().bSuccess) { Finish(false,TEXT("ForeignProposerConfirmed"));return; }EventNetStep=4; }
  }
 }
 if(V.Phase==EDefensePhase::Victory) Finish((EventNetMask&54)==54&&bEventNetReceipt&&EventNetStep==4,FString::Printf(TEXT("EventNetwork Schema=4 StateMask=%d Receipt=%d Step=%d Seat=%d ClientAuthority=0 Run=%s"),EventNetMask,bEventNetReceipt,EventNetStep,Seat,*V.RunId.ToString()));
}
