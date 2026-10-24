#include "CoreMinimal.h"
#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "Tests/AutomationCommon.h"
#include "Combat/Tests/CombatDefensePIEHelpers.h"
#include "Editor.h"
#include "Misc/Paths.h"
#include "Combat/Defense/DefenseHUD.h"
#include "Combat/Defense/DefenseData.h"
#include "Combat/Defense/DefenseGameMode.h"
#include "Combat/Defense/DefenseDirectorComponent.h"
#include "Combat/Defense/DefensePlayerController.h"
#include "Combat/Defense/DefenseHeroSelectionComponent.h"
#include "Combat/UI/CombatShopWidget.h"
#include "Combat/UI/CombatHUDWidget.h"
#include "Combat/UI/CombatHUDSlotWidget.h"
#include "Combat/Economy/CombatEconomyComponent.h"
#include "Combat/Items/CombatInventoryComponent.h"
#include "Combat/Unit/CombatUnitCharacter.h"
#include "Combat/Unit/CombatUnitLifecycleComponent.h"
#include "Combat/View/CombatUnitViewComponent.h"
#include "Combat/Ability/CombatAbilitySystemComponent.h"
#include "Combat/Combat/CombatDamageSubsystem.h"
#include "Combat/Combat/CombatEffectUtilities.h"
#include "Combat/Attributes/CombatAttributeSet.h"
#include "Combat/Motion/CombatMotionComponent.h"
#include "Combat/Order/CombatOrderComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Combat/Log/CombatEventSubsystem.h"
#include "Combat/Core/CombatTags.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Framework/Application/SlateApplication.h"
#include "Layout/WidgetPath.h"

/** 真地图、商店可见几何与原技能槽点击；另一个用例在Motion日志回调中换控，验证提交前失效。 */
class FReturnBeaconPIEProbe : public IAutomationLatentCommand
{
public:
    FReturnBeaconPIEProbe(FAutomationTestBase* In,int32 InReentry):Test(In),Reentry(InReentry){}
    bool Update() override
    {
        if(Started==0) Started=FPlatformTime::Seconds();
        if(FPlatformTime::Seconds()-Started>105) { Test->AddError(FString::Printf(TEXT("Return beacon PIE timeout stage %d reentry%d"),Stage,Reentry)); Cleanup(); return true; }
        auto* W=GEditor?GEditor->PlayWorld.Get():nullptr; auto* PC=W?Cast<ADefensePlayerController>(W->GetFirstPlayerController()):nullptr;
        auto* HUD=PC?Cast<ADefenseHUD>(PC->GetHUD()):nullptr; auto* Shop=HUD?HUD->GetShopWidget():nullptr; auto* Hero=ObservedHero.IsValid()?ObservedHero.Get():(PC?PC->GetCommandedUnit():nullptr);
        if(!Hero || !Shop) return false;
        if(Hero->GetUnitDefinitionId().PrimaryAssetName!=TEXT("defense_guardian"))
        { if(!Selected&&PC->GetHeroSelection()->GetView().bCanChoose){PC->GetHeroSelection()->RequestGuardian(); Selected=true;} return false; }
        auto* D=W->GetAuthGameMode<ADefenseGameMode>()->GetDefenseDirector(); auto* A=Hero->GetCombatAbilitySystemComponent();
        const auto* Spec=A->FindCombatAbilitySpecByDefinitionId(FPrimaryAssetId(TEXT("CombatAbility"),TEXT("defense_recall"))); if(!Spec)return false;
        if(Stage==0&&Shop->GoldButton&&Shop->GoldButton->GetCachedGeometry().GetLocalSize().X>1)
        { PC->GetCombatEconomyComponent()->AddGold(1000,TEXT("ReturnBeaconPIEFixture")); Gold=PC->GetCombatEconomyComponent()->GetGold(); Shop->ToggleShop(); Stage=1; return false; }
        if(Stage==1)
        {
            const auto* Entry=Shop->CatalogEntries.FindByPredicate([](const auto& V){return V.DefinitionId.PrimaryAssetName==TEXT("item_return_beacon");}); if(!Entry||!Entry->HitWidget) return false;
            if(!Scrolled)
            {
                FWidgetPath Path; if(!FSlateApplication::Get().GeneratePathToWidgetUnchecked(Entry->HitWidget.ToSharedRef(),Path,EVisibility::All))return false;
                for(int32 I=Path.Widgets.Num()-1;I>=0;--I) if(Path.Widgets[I].Widget->GetType()==FName(TEXT("SScrollBox")))
                { StaticCastSharedRef<SScrollBox>(Path.Widgets[I].Widget)->ScrollDescendantIntoView(Entry->HitWidget,false,EDescendantScrollDestination::Center); Scrolled=true; return false; }
                return false;
            }
            const auto G=Entry->HitWidget->GetCachedGeometry(); if(G.GetLocalSize().X<1)return false; const auto P=G.LocalToAbsolute(G.GetLocalSize()*.5f);
            FPointerEvent E(0,P,P,TSet<FKey>{EKeys::RightMouseButton},EKeys::RightMouseButton,0,FModifierKeysState());
            Test->TestTrue(TEXT("Visible shop geometry purchases beacon"),Shop->NativeOnPreviewMouseButtonDown(G,E).IsEventHandled()); Stage=2; return false;
        }
        if(Stage==2&&Hero->GetCombatInventoryComponent()->GetItemCount()==1)
        {
            Test->TestEqual(TEXT("Real beacon costs150"),PC->GetCombatEconomyComponent()->GetGold(),Gold-150); Shop->ToggleShop();
            FVector Home; if(!Test->TestTrue(TEXT("Actual navigable home"),D->ResolveRecallDestination(Hero,Home)))return true;
            if(!Test->TestTrue(TEXT("Fixture displacement uses public Motion"),Hero->GetCombatMotionComponent()->TryTeleport(Home+FVector(900,500,0),{},{}).bSuccess))return true;
            Hero->GetCharacterMovement()->DisableMovement();
            FCombatOrderRequest Stop; Stop.Type=ECombatOrderType::Stop; Hero->GetCombatOrderComponent()->IssueOrder(Stop,false);
            CombatEffectUtilities::ApplyAttributeAdditive(A,*A,UCombatAttributeSet::GetHealthRegenAttribute(),-Hero->GetCombatAttributeSet()->GetHealthRegen());
            CombatEffectUtilities::ApplyAttributeAdditive(A,*A,UCombatAttributeSet::GetManaRegenAttribute(),-Hero->GetCombatAttributeSet()->GetManaRegen());
            CombatEffectUtilities::ApplyAttributeAdditive(A,*A,UCombatAttributeSet::GetManaAttribute(),-20);
            FCombatDamageRequest R; R.Source=R.Target=Hero; R.Amount=50; R.DamageType=ECombatDamageType::Pure; W->GetSubsystem<UCombatDamageSubsystem>()->DealDamage(R);
            FixtureReadyAt=W->GetTimeSeconds()+.4; Stage=3; return false;
        }
        if(Stage==3&&W->GetTimeSeconds()>=FixtureReadyAt)
        {
            auto* Widget=HUD->GetCombatWidget(); auto* Slot=Widget?Cast<UCombatHUDSlotWidget>(Widget->GetWidgetFromName(TEXT("SkillR"))):nullptr;
            if(!Slot||Slot->GetCachedGeometry().GetLocalSize().X<1)return false;
            Origin=Hero->GetActorLocation(); Life=Hero->GetLifeGeneration(); HP=Hero->GetCombatAttributeSet()->GetHealth(); Mana=Hero->GetCombatAttributeSet()->GetMana();
            ObservedHero=Hero;
            if(Reentry)
            {
                auto* Motion=Hero->GetCombatMotionComponent();
                MotionObserver=Motion->OnMotionStarted().AddLambda([this,PC,D](const FCombatMotionHandle&)
                { ++MotionStarts; if(Reentry==1) PC->SetCommandedUnitAuthority(nullptr); else D->Shutdown(); });
            }
            const auto G=Slot->GetCachedGeometry(); const auto P=G.LocalToAbsolute(G.GetLocalSize()*.5f);
            Test->TestEqual(TEXT("Original HUD identifies R"),Widget->GetHoveredAbilitySlot(P),3);
            FPointerEvent E(0,P,P,TSet<FKey>{EKeys::LeftMouseButton},EKeys::LeftMouseButton,0,FModifierKeysState());
            Test->TestTrue(TEXT("Real R slot handles click"),Slot->TakeWidget()->OnMouseButtonDown(G,E).IsEventHandled()); Stage=4; return false;
        }
        const auto& V=Hero->GetCombatUnitViewComponent()->GetUnitView();
        if(Stage==4&&V.AbilityPhase==ECombatAbilityViewPhase::Channeling)
        {
            Test->TestEqual(TEXT("Actual channel bar shows2seconds"),float(V.AbilityServerEndTime-V.AbilityServerStartTime),2.f);
            double End=0; float Duration=0; A->GetCombatAbilityCooldownWindow(Spec->Handle,End,Duration); Test->TestEqual(TEXT("Actual cooldown window30"),Duration,30.f);
            FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("DEF-017/return-beacon-channel-pie.png"),true,false); Deadline=V.AbilityServerEndTime; Stage=5; return false;
        }
        if(Stage==5&&W->GetTimeSeconds()>Deadline+.4)
        {
            Test->TestFalse(TEXT("Recall ended"),Spec->IsActive()); Test->TestEqual(TEXT("Recall does not change life"),Hero->GetLifeGeneration(),Life);
            Test->TestEqual(TEXT("Recall does not heal"),Hero->GetCombatAttributeSet()->GetHealth(),HP); Test->TestEqual(TEXT("Recall does not restore mana"),Hero->GetCombatAttributeSet()->GetMana(),Mana);
            if(Reentry)
            {
                Test->TestEqual(TEXT("Commit observer ran once"),MotionStarts,1);
                Test->TestTrue(TEXT("Canceled during Motion start cannot teleport"),Hero->GetActorLocation().Equals(Origin,2)); Cleanup(); return true;
            }
            Test->TestTrue(TEXT("Actual return moved safely"),FVector::Dist2D(Origin,Hero->GetActorLocation())>600);
            Test->TestTrue(TEXT("Return is near frozen home"),FVector::Dist2D(Hero->GetActorLocation(),D->GetFrozenRunData()->RespawnLocation)<350);
            FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("DEF-017/return-beacon-arrival-pie.png"),true,false); Stage=6; return false;
        }
        if(Stage==6&&A->GetCombatAbilityCooldownRemaining(Spec->Handle)<=0)
        {
            PC->ActivateCombatAbilitySlotFromHUD(3);
            Stage=7; return false;
        }
        if(Stage==7&&V.AbilityPhase==ECombatAbilityViewPhase::Channeling)
        {
            FCombatOrderRequest R; R.Type=ECombatOrderType::Stop; Test->TestTrue(TEXT("Stop interrupts second recall"),Hero->GetCombatOrderComponent()->IssueOrder(R).bSuccess);
            Test->TestFalse(TEXT("No active recall after Stop"),Spec->IsActive());
            Test->AddInfo(TEXT("DefenseReturnBeaconPIE Shop=150 HUDClick=R Channel=2 Cooldown=30 Arrival=Safe NoHeal=1 NoMana=1 NoLifeChange=1 HotkeyAdapter=R Stop=Pass")); Cleanup(); return true;
        }
        return false;
    }
private:
    /** 用例结束前解绑，避免Latent对象被日志委托悬挂引用。 */
    void Cleanup(){if(ObservedHero.IsValid() && MotionObserver.IsValid()) ObservedHero->GetCombatMotionComponent()->OnMotionStarted().Remove(MotionObserver); if(Events.IsValid())Events->OnRecord().Remove(Observer); Events.Reset();}
    FAutomationTestBase* Test; int32 Reentry=0; bool Selected=false,Scrolled=false; int32 Stage=0,MotionStarts=0; int64 Gold=0;
    double Started=0,Deadline=0,FixtureReadyAt=0; FVector Origin; FDelegateHandle MotionObserver; uint32 Life=0; float HP=0,Mana=0;
    TWeakObjectPtr<ACombatUnitCharacter> ObservedHero; TWeakObjectPtr<UCombatEventSubsystem> Events; FDelegateHandle Observer;
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReturnBeaconPIETest,"Combat.Defense.PIE.ReturnBeacon",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FReturnBeaconPIETest::RunTest(const FString&)
{
    ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/Combat/Demo/Defense/L_Defense")));
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FDefensePIENavigationReady>(this)); ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FReturnBeaconPIEProbe>(this,false)); ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand()); ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1)); return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReturnBeaconReentryPIETest,"Combat.Defense.PIE.ReturnBeaconCommitReentry",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FReturnBeaconReentryPIETest::RunTest(const FString&)
{
    ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/Combat/Demo/Defense/L_Defense")));
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FDefensePIENavigationReady>(this)); ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FReturnBeaconPIEProbe>(this,true)); ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand()); ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1)); return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReturnBeaconTerminalCommitPIETest,"Combat.Defense.PIE.ReturnBeaconTerminalCommit",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FReturnBeaconTerminalCommitPIETest::RunTest(const FString&)
{
    ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/Combat/Demo/Defense/L_Defense")));
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FDefensePIENavigationReady>(this)); ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FReturnBeaconPIEProbe>(this,2)); ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand()); ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1)); return true;
}
#endif
