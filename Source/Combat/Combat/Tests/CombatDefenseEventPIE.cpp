#include "CoreMinimal.h"
#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "Tests/AutomationCommon.h"
#include "Combat/Tests/CombatDefensePIEHelpers.h"
#include "Combat/Defense/DefenseGameMode.h"
#include "Combat/Defense/DefenseGameState.h"
#include "Combat/Defense/DefenseDirectorComponent.h"
#include "Combat/Defense/DefenseData.h"
#include "Combat/Defense/DefenseObjective.h"
#include "Combat/Defense/DefensePlayerController.h"
#include "Combat/Defense/DefenseChoiceComponent.h"
#include "Combat/Defense/DefenseEventComponent.h"
#include "Combat/Defense/DefenseHUD.h"
#include "Combat/Defense/DefenseBuildWidget.h"
#include "Combat/Combat/CombatDamageSubsystem.h"
#include "Combat/Economy/CombatEconomyComponent.h"
#include "Combat/Attributes/CombatAttributeSet.h"
#include "EngineUtils.h"
#include "UnrealClient.h"
#include "Misc/Paths.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Input/SEditableTextBox.h"

/** 只缩短 PIE 本局深拷贝中的等待，保留正式资产、自然出生和完整状态机。 */
struct FDefenseEventPIEAccess
{
 static void ShortenWaits(UDefenseDirectorComponent* D)
 {
  D->RunData->PreparationDuration=1;D->RunData->IntermissionDuration=1;D->RunData->ChoiceDuration=1;
  for(UDefenseWaveData* Wave:D->RunData->Waves) for(auto& Group:Wave->SpawnGroups) Group.SpawnInterval=.1f;
 }
};

/** 在实际渲染 PIE 中点击正式 Slate 按钮；通过公共 Damage 加速清场，不直接调用事件状态机。 */
class FDefenseEventPIEProbe : public IAutomationLatentCommand
{
public:
 FDefenseEventPIEProbe(FAutomationTestBase* InTest,int32 InMode):Test(InTest),Mode(InMode) {}
 bool Update() override
 {
  const double Now=FPlatformTime::Seconds();if(Started==0) Started=Now;
  if(Now-Started>150) { Test->AddError(FString::Printf(TEXT("Event PIE timeout stage=%d mode=%d"),Stage,Mode));return true; }
  auto* W=GEditor?GEditor->PlayWorld.Get():nullptr;auto* State=W?W->GetGameState<ADefenseGameState>():nullptr;
  auto* PC=W?Cast<ADefensePlayerController>(W->GetFirstPlayerController()):nullptr;
  auto* HUD=PC?Cast<ADefenseHUD>(PC->GetHUD()):nullptr;auto* UI=HUD?HUD->GetBuildWidget():nullptr;
  if(!State||!UI||!PC->GetCommandedUnit()) return false;
  const auto V=State->GetRunView();
  if(V.Phase==EDefensePhase::Defeat) { Test->AddError(TEXT("Event PIE defeated: ")+V.TerminalReason.ToString());return true; }
  if(Stage==0&&Ready(UI->StartButton))
  {
   auto* D=W->GetAuthGameMode<ADefenseGameMode>()->GetDefenseDirector();
   Test->TestEqual(TEXT("Authored event count"),D->GetFrozenRunData()->Events.Num(),2);
   Test->TestTrue(TEXT("Frozen event references a real package before duplication"),!D->GetFrozenRunData()->Events[0]->GetPrimaryAssetId().PrimaryAssetName.IsNone());
   FDefenseEventPIEAccess::ShortenWaits(D);UI->SeedInput->SetText(FText::FromString(TEXT("831907")));Click(UI->StartButton);Stage=1;return false;
  }
  if(Stage==1&&V.Phase==EDefensePhase::EventChoice&&V.WaveIndex==1&&Ready(UI->EventSafeButton))
  {
   Test->TestEqual(TEXT("Vault definition"),V.Event.DefinitionId,FPrimaryAssetId(TEXT("DefenseEvent"),TEXT("defense_event_vault")));
   Gold=PC->GetCombatEconomyComponent()->GetGold();
   Test->TestTrue(TEXT("Event panel blocks world click"),UI->IsScreenPositionOverUI(UI->EventSafeButton->GetCachedGeometry().LocalToAbsolute(FVector2D(8,8))));
   Capture(TEXT("vault-safe"));Stage=2;ActionAt=Now+.3;return false;
  }
  if(Stage==2&&Now>=ActionAt) { Click(UI->EventSafeButton);Stage=3;return false; }
  if(Stage==3&&V.Phase==EDefensePhase::Intermission&&V.WaveIndex==1)
  {
   Test->TestEqual(TEXT("Vault safe reward once"),PC->GetCombatEconomyComponent()->GetGold(),Gold+240);
   Test->TestTrue(TEXT("Owning event receipt succeeds"),PC->GetEventComponent()->GetLastResult().bSuccess);Stage=4;
  }
  if(Stage==4&&V.Phase==EDefensePhase::EventChoice&&V.WaveIndex==3&&Ready(UI->EventRiskButton))
  {
   Test->TestEqual(TEXT("Second event redraws despite repeated local revision"),V.Event.DefinitionId,FPrimaryAssetId(TEXT("DefenseEvent"),TEXT("defense_event_oath")));
   Damage(W,V.Objective,1000);CoreBefore=V.Objective->GetCombatAttributeSet()->GetHealth();
   Gold=PC->GetCombatEconomyComponent()->GetGold();Click(UI->EventRiskButton);Stage=5;return false;
  }
  if(Stage==5&&V.Event.State==EDefenseEventState::PendingRisk&&Ready(UI->EventCancelButton))
  {
   Test->TestTrue(TEXT("Ten-second cancellation deadline"),V.Event.Deadline-State->GetServerWorldTimeSeconds()>8.0);
   Capture(Mode==0?TEXT("oath-cancel"):Mode==1?TEXT("oath-timeout"):TEXT("oath-confirm"));Stage=6;ActionAt=Now+.3;return false;
  }
  if(Stage==6&&Now>=ActionAt)
  {
   if(Mode==0) Click(UI->EventCancelButton);
   if(Mode==2) Click(UI->EventRiskButton);
   Stage=7;return false;
  }
  if(Stage==7&&V.Phase==EDefensePhase::Intermission&&V.WaveIndex==3)
  {
   Test->TestEqual(TEXT("Event terminal state"),V.Event.State,Mode==0?EDefenseEventState::Cancelled:Mode==1?EDefenseEventState::Expired:EDefenseEventState::ResolvedRisk);
   Test->TestEqual(TEXT("No immediate risk gold"),PC->GetCombatEconomyComponent()->GetGold(),Gold);
   if(Mode!=2) Test->TestEqual(TEXT("Public core heal safe fallback"),V.Objective->GetCombatAttributeSet()->GetHealth(),FMath::Min(CoreBefore+V.ObjectiveMaxHealth*.12f,V.ObjectiveMaxHealth));
   Stage=8;
  }
  if(Stage==8&&V.Phase==EDefensePhase::BossPreparation)
  {
   const int64 BaseGold=LoadObject<UDefenseRunData>(nullptr,TEXT("/Game/Combat/Demo/Defense/DA_DefenseRun.DA_DefenseRun"))->Waves[4]->GoldReward;
   Test->TestEqual(TEXT("Risk delayed reward exactly once"),PC->GetCombatEconomyComponent()->GetGold(),Gold+BaseGold+(Mode==2?300:0));
   Test->TestTrue(TEXT("Natural enemy movement before public clear"),Moved);
   Test->AddInfo(FString::Printf(TEXT("DefenseEventPIE Navigation=Ready RenderedSlate=1 SafeGold=240 OathMode=%d CoreHeal=%d DelayedRisk=%d Movement=1"),Mode,Mode!=2,Mode==2));return true;
  }
  // 先观察正式出生和导航移动，再经伤害管线清场；不会直接更改波次或剩余计数。
  if(V.Phase==EDefensePhase::WaveCombat)
  {
   if(LastWave!=V.WaveIndex) { LastWave=V.WaveIndex;WaveAt=Now;Positions.Reset(); }
   TArray<ADefenseEnemy*> Enemies;
   for(TActorIterator<ADefenseEnemy> It(W);It;++It)
   {
    if(It->IsA<ADefenseObjective>()||It->GetLifeState()!=ECombatLifeState::Alive) continue;
    Enemies.Add(*It);
    if(auto* Old=Positions.Find(*It)) Moved|=FVector::Dist2D(*Old,It->GetActorLocation())>100;
    else Positions.Add(*It,It->GetActorLocation());
   }
   if(V.PendingSpawns==0&&Now-WaveAt>2.5) for(auto* Enemy:Enemies) Damage(W,Enemy,100000);
  }
  return false;
 }
private:
 /** 截图证明实际事件面板布局；不能以 NullRHI 数据回读替代。 */
 void Capture(const TCHAR* Name) { FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/FString::Printf(TEXT("DEF-013/%s-mode%d.png"),Name,Mode),true,false); }
 /** 仅在按钮已布局时发鼠标按下，进入与实际点击相同的 Slate 回调。 */
 bool Ready(const TSharedPtr<SButton>& B) const { return B&&B->GetVisibility().IsVisible()&&B->GetCachedGeometry().GetLocalSize().X>1; }
 void Click(TSharedPtr<SButton> B)
 {
  if(!Ready(B)) { Test->AddError(TEXT("Event Slate button not laid out"));return; }
  const auto G=B->GetCachedGeometry();const auto P=G.LocalToAbsolute(G.GetLocalSize()*.5f);
  FPointerEvent E(0,P,P,TSet<FKey>{EKeys::LeftMouseButton},EKeys::LeftMouseButton,0,FModifierKeysState());
  Test->TestTrue(TEXT("Actual Slate button handles click"),B->OnMouseButtonDown(G,E).IsEventHandled());
 }
 /** 测试清场/损伤仍复用生产伤害事务，不直接写生命。 */
 void Damage(UWorld* W,ACombatUnitCharacter* Unit,float Amount)
 { FCombatDamageRequest R;R.Source=Unit;R.Target=Unit;R.Amount=Amount;R.DamageType=ECombatDamageType::Pure;W->GetSubsystem<UCombatDamageSubsystem>()->DealDamage(R); }
 FAutomationTestBase* Test;int32 Mode,Stage=0,LastWave=INDEX_NONE;double Started=0,ActionAt=0,WaveAt=0;
 int64 Gold=0;float CoreBefore=0;bool Moved=false;TMap<TWeakObjectPtr<ADefenseEnemy>,FVector> Positions;
};

/** 三条真实事件路径共用相同地图和按钮入口，独立 PIE 会话隔离状态。 */
static bool QueueDefenseEventPIE(FAutomationTestBase* Test,int32 Mode)
{
 ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/Combat/Demo/Defense/L_Defense")));
 FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FDefensePIENavigationReady>(Test));
 ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
 FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FDefenseEventPIEProbe>(Test,Mode));
 ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1));return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDefenseEventCancelPIETest,"Combat.Defense.PIE.EventSafeAndCancel",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDefenseEventCancelPIETest::RunTest(const FString&) { return QueueDefenseEventPIE(this,0); }
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDefenseEventTimeoutPIETest,"Combat.Defense.PIE.EventRiskTimeout",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDefenseEventTimeoutPIETest::RunTest(const FString&) { return QueueDefenseEventPIE(this,1); }
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDefenseEventConfirmPIETest,"Combat.Defense.PIE.EventRiskConfirm",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDefenseEventConfirmPIETest::RunTest(const FString&) { return QueueDefenseEventPIE(this,2); }
#endif
