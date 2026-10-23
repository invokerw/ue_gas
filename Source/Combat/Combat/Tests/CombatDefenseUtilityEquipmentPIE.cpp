#include "CoreMinimal.h"
#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "Tests/AutomationCommon.h"
#include "Combat/Tests/CombatDefensePIEHelpers.h"
#include "Editor.h"
#include "UnrealClient.h"
#include "Misc/Paths.h"
#include "Combat/Defense/DefenseHUD.h"
#include "Combat/Defense/DefenseGameMode.h"
#include "Combat/Defense/DefenseObjective.h"
#include "Combat/Defense/DefensePlayerController.h"
#include "Combat/Defense/DefenseHeroSelectionComponent.h"
#include "Combat/UI/CombatShopWidget.h"
#include "Combat/Economy/CombatEconomyComponent.h"
#include "Combat/Items/CombatInventoryComponent.h"
#include "Combat/Attributes/CombatAttributeSet.h"
#include "Combat/Unit/CombatUnitCharacter.h"
#include "Combat/Ability/CombatAbilitySystemComponent.h"
#include "Combat/Order/CombatOrderComponent.h"
#include "Combat/Combat/CombatDamageSubsystem.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Framework/Application/SlateApplication.h"
#include "Layout/WidgetPath.h"

/** 正式地图与原商店几何，真实选角、库存购买、核心修复及Order施法冷却。 */
class FDefenseUtilityEquipmentUIProbe : public IAutomationLatentCommand
{
public:
    explicit FDefenseUtilityEquipmentUIProbe(FAutomationTestBase* In):Test(In){}
    bool Update() override
    {
        if(Started==0)Started=FPlatformTime::Seconds();
        if(FPlatformTime::Seconds()-Started>70){Test->AddError(FString::Printf(TEXT("Utility PIE timed out stage %d"),Stage));return true;}
        auto* W=GEditor?GEditor->PlayWorld.Get():nullptr;auto* PC=W?Cast<ADefensePlayerController>(W->GetFirstPlayerController()):nullptr;
        auto* HUD=PC?Cast<ADefenseHUD>(PC->GetHUD()):nullptr;auto* Shop=HUD?HUD->GetShopWidget():nullptr;auto* Hero=PC?PC->GetCommandedUnit():nullptr;
        if(!Hero||!Shop)return false;
        if(Hero->GetUnitDefinitionId().PrimaryAssetName!=TEXT("defense_guardian"))
        {if(!bSelected&&PC->GetHeroSelection()->GetView().bCanChoose){PC->GetHeroSelection()->RequestGuardian();bSelected=true;}return false;}
        auto* E=PC->GetCombatEconomyComponent();auto* A=Hero->GetCombatAbilitySystemComponent();auto* Core=W->GetAuthGameMode<ADefenseGameMode>()->GetDefenseObjective();
        if(Stage==0&&Shop->GoldButton&&Shop->GoldButton->GetCachedGeometry().GetLocalSize().X>1)
        {
            E->AddGold(1500,TEXT("UtilityPIEFixture"));Gold=E->GetGold();FCombatDamageRequest R;R.Source=R.Target=Core;R.Amount=2000;R.DamageType=ECombatDamageType::Pure;
            W->GetSubsystem<UCombatDamageSubsystem>()->DealDamage(R);CoreBefore=Core->GetCombatAttributeSet()->GetHealth();Shop->ToggleShop();Stage=1;return false;
        }
        auto Click=[&](const TCHAR* Name)
        {
            const FPrimaryAssetId Id(TEXT("CombatItem"),Name);const auto* Entry=Shop->CatalogEntries.FindByPredicate([&](const auto& V){return V.DefinitionId==Id;});if(!Entry||!Entry->HitWidget)return false;
            if(Scrolled!=Id)
            {
                FWidgetPath Path;if(!FSlateApplication::Get().GeneratePathToWidgetUnchecked(Entry->HitWidget.ToSharedRef(),Path,EVisibility::All))return false;
                for(int32 N=Path.Widgets.Num()-1;N>=0;--N)if(Path.Widgets[N].Widget->GetType()==FName(TEXT("SScrollBox")))
                {StaticCastSharedRef<SScrollBox>(Path.Widgets[N].Widget)->ScrollDescendantIntoView(Entry->HitWidget,false,EDescendantScrollDestination::Center);Scrolled=Id;return false;}
                return false;
            }
            const auto G=Entry->HitWidget->GetCachedGeometry();if(G.GetLocalSize().X<1)return false;const auto P=G.LocalToAbsolute(G.GetLocalSize()*.5f);
            Test->TestTrue(TEXT("Visible original shop blocks world"),Shop->IsScreenPositionOverUI(P));FPointerEvent R(0,P,P,TSet<FKey>{EKeys::RightMouseButton},EKeys::RightMouseButton,0,FModifierKeysState());
            Test->TestTrue(TEXT("Real item geometry right click handled"),Shop->NativeOnPreviewMouseButtonDown(G,R).IsEventHandled());return true;
        };
        if(Stage==1&&Click(TEXT("item_mana_sigil"))){Stage=2;return false;}
        if(Stage==2&&Hero->GetCombatInventoryComponent()->GetItemCount()==1&&Click(TEXT("item_repair_lens"))){Stage=3;return false;}
        if(Stage==3&&Hero->GetCombatInventoryComponent()->GetItemCount()==2)
        {Test->TestEqual(TEXT("Two real items cost440"),E->GetGold(),Gold-440);if(Click(TEXT("team_emergency_stone")))Stage=4;return false;}
        auto Skill=[&](const TCHAR* Name){return A->FindCombatAbilitySpecByDefinitionId(FPrimaryAssetId(TEXT("CombatAbility"),Name));};
        if(Stage==4&&PC->GetLastEconomyResult().bConsumedOnAcquire)
        {
            Test->TestEqual(TEXT("Original stone cost180"),E->GetGold(),Gold-620);Test->TestTrue(TEXT("Lens actual core repair900"),FMath::IsNearlyEqual(Core->GetCombatAttributeSet()->GetHealth()-CoreBefore,900.f,.01f));
            const auto* Q=Skill(TEXT("guardian_shield_bash"));if(!Q)return false;FCombatOrderRequest R;R.Type=ECombatOrderType::CastPoint;R.AbilitySpecHandle=Q->Handle;R.bHasTargetLocation=true;R.TargetLocation=Hero->GetActorLocation()+FVector(200,0,0);
            Test->TestTrue(TEXT("Q through real Order"),Hero->GetCombatOrderComponent()->IssueOrder(R).bSuccess);Stage=5;return false;
        }
        if(Stage==5)
        {
            const auto* Q=Skill(TEXT("guardian_shield_bash"));double End=0;float Duration=0;A->GetCombatAbilityCooldownWindow(Q->Handle,End,Duration);if(Duration==0)return false;
            Test->TestTrue(TEXT("Actual Q duration7.36"),FMath::IsNearlyEqual(Duration,7.36f,.001f));const auto* S=Skill(TEXT("guardian_ground_slam"));if(!S)return false;
            FCombatOrderRequest R;R.Type=ECombatOrderType::CastNoTarget;R.AbilitySpecHandle=S->Handle;Test->TestTrue(TEXT("E through real Order"),Hero->GetCombatOrderComponent()->IssueOrder(R).bSuccess);Stage=6;return false;
        }
        if(Stage==6)
        {
            const auto* S=Skill(TEXT("guardian_ground_slam"));double End=0;float Duration=0;A->GetCombatAbilityCooldownWindow(S->Handle,End,Duration);if(Duration==0)return false;
            Test->TestTrue(TEXT("Actual E duration12.88"),FMath::IsNearlyEqual(Duration,12.88f,.001f));
            FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("DEF-015/utility-shop-pie.png"),true,false);CaptureAt=FPlatformTime::Seconds();Stage=7;return false;
        }
        if(Stage==7&&FPlatformTime::Seconds()-CaptureAt>1)
        {Test->AddInfo(TEXT("DefenseUtilityPIE VisibleRightClicks=3 Items=2 Gold=620 CoreRepair=900 Q=7.36 E=12.88 OriginalOrder=1"));return true;}
        return false;
    }
private:
    FAutomationTestBase* Test;double Started=0,CaptureAt=0;int32 Stage=0;int64 Gold=0;float CoreBefore=0;bool bSelected=false;FPrimaryAssetId Scrolled;
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDefenseUtilityEquipmentPIETest,"Combat.Defense.PIE.UtilityEquipment",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDefenseUtilityEquipmentPIETest::RunTest(const FString&)
{
    ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/Combat/Demo/Defense/L_Defense")));
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FDefensePIENavigationReady>(this));ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FDefenseUtilityEquipmentUIProbe>(this));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1));return true;
}
#endif
