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
#include "Combat/UI/CombatShopWidget.h"
#include "Combat/Economy/CombatEconomyComponent.h"
#include "Combat/Items/CombatInventoryComponent.h"
#include "Combat/Attributes/CombatAttributeSet.h"
#include "Combat/Unit/CombatUnitCharacter.h"
#include "CombatPlayerController.h"
#include "Widgets/Input/SButton.h"

/** 真实地图、Slate几何右键和原商店RPC；无导航或直接导演购物旁路。 */
class FDefenseEquipmentUIProbe : public IAutomationLatentCommand
{
public:
    explicit FDefenseEquipmentUIProbe(FAutomationTestBase* In):Test(In) {}
    bool Update() override
    {
        if(Started==0)Started=FPlatformTime::Seconds();
        if(FPlatformTime::Seconds()-Started>60){Test->AddError(FString::Printf(TEXT("Equipment shop PIE timed out stage %d"),Stage));return true;}
        auto* World=GEditor?GEditor->PlayWorld.Get():nullptr;
        auto* PC=World?Cast<ACombatPlayerController>(World->GetFirstPlayerController()):nullptr;
        auto* HUD=PC?Cast<ADefenseHUD>(PC->GetHUD()):nullptr;
        auto* Shop=HUD?HUD->GetShopWidget():nullptr;
        auto* Hero=PC?PC->GetCommandedUnit():nullptr;if(!Shop||!Hero)return false;
        auto* Economy=PC->GetCombatEconomyComponent();
        if(Stage==0&&Shop->GoldButton&&Shop->GoldButton->GetCachedGeometry().GetLocalSize().X>1)
        {
            // 显式测试补给金币用于同窗购买两件；不修改正式经济资产和玩家初始金币。
            Economy->AddGold(1000,TEXT("EquipmentPIEFixture"));Gold=Economy->GetGold();Health=Hero->GetCombatAttributeSet()->GetHealth();Cap=Hero->GetCombatAttributeSet()->GetMaxHealth();
            Shop->ToggleShop();Stage=1;return false;
        }
        auto RightClick=[&](const TCHAR* Name)
        {
            const auto* Entry=Shop->CatalogEntries.FindByPredicate([&](const auto& E){return E.DefinitionId==FPrimaryAssetId(TEXT("CombatItem"),Name);});
            if(!Entry||!Entry->HitWidget||Entry->HitWidget->GetCachedGeometry().GetLocalSize().X<1)return false;
            const auto G=Entry->HitWidget->GetCachedGeometry();const auto P=G.LocalToAbsolute(G.GetLocalSize()*.5f);
            if(!Shop->IsScreenPositionOverUI(P))return false;
            FPointerEvent E(0,P,P,TSet<FKey>{EKeys::RightMouseButton},EKeys::RightMouseButton,0,FModifierKeysState());
            Test->TestTrue(TEXT("Original visible shop receives right click"),Shop->NativeOnPreviewMouseButtonDown(G,E).IsEventHandled());return true;
        };
        if(Stage==1&&Shop->IsShopOpen()&&RightClick(TEXT("item_stone_guard"))){Stage=2;return false;}
        if(Stage==2&&Hero->GetCombatInventoryComponent()->GetItemCount()==1)
        {
            Test->TestEqual(TEXT("Stone cost from production asset"),Economy->GetGold(),Gold-160);Test->TestEqual(TEXT("Actual cap increased"),Hero->GetCombatAttributeSet()->GetMaxHealth(),Cap+120);
            Test->TestTrue(TEXT("Buying health cap does not heal"),Hero->GetCombatAttributeSet()->GetHealth()<=Health+1);
            if(RightClick(TEXT("item_iron_bow"))){Stage=3;return false;}
        }
        if(Stage==3&&Hero->GetCombatInventoryComponent()->GetItemCount()==2)
        {
            Test->TestEqual(TEXT("Two items charged exactly"),Economy->GetGold(),Gold-340);
            const auto* Offer=Economy->FindShopOffer(FPrimaryAssetId(TEXT("CombatItem"),TEXT("item_hunter_scope")));
            Test->TestTrue(TEXT("Quota status is server projected"),Offer&&!Offer->bCanPurchase&&Offer->RemainingPurchases==0);
            if(RightClick(TEXT("item_hunter_scope"))){Stage=4;return false;}
        }
        if(Stage==4)
        {
            Test->TestFalse(TEXT("Third equipment purchase rejected"),PC->GetLastEconomyResult().bSuccess);Test->TestEqual(TEXT("Rejected purchase kept money"),Economy->GetGold(),Gold-340);
            Test->TestEqual(TEXT("Exactly two real inventory slots"),Hero->GetCombatInventoryComponent()->GetItemCount(),2);
            FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("DEF-014/equipment-shop-pie.png"),true,false);CaptureAt=FPlatformTime::Seconds();Stage=5;return false;
        }
        if(Stage==5&&FPlatformTime::Seconds()-CaptureAt>1)
        {Test->AddInfo(TEXT("EquipmentPIE OriginalShop=1 VisibleRightClicks=3 RealItems=2 Gold=340 MaxHealth=120 RejectedThird=1"));return true;}
        return false;
    }
private:
    FAutomationTestBase* Test;double Started=0,CaptureAt=0;int32 Stage=0;int64 Gold=0;float Health=0,Cap=0;
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDefenseEquipmentPIETest,"Combat.Defense.PIE.EquipmentShop",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDefenseEquipmentPIETest::RunTest(const FString&)
{
    ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/Combat/Demo/Defense/L_Defense")));
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FDefensePIENavigationReady>(this));
    ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FDefenseEquipmentUIProbe>(this));
    ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1));return true;
}
#endif
