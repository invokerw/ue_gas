#include "CoreMinimal.h"
#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "Tests/AutomationCommon.h"
#include "Combat/Tests/CombatDefensePIEHelpers.h"
#include "Editor.h"
#include "Misc/Paths.h"
#include "Combat/Defense/DefenseHUD.h"
#include "Combat/Defense/DefensePlayerController.h"
#include "Combat/Defense/DefenseHeroSelectionComponent.h"
#include "Combat/Defense/DefenseBuildWidget.h"
#include "Combat/Defense/DefenseChoiceComponent.h"
#include "Combat/UI/CombatShopWidget.h"
#include "Combat/Economy/CombatEconomyComponent.h"
#include "Combat/Items/CombatInventoryComponent.h"
#include "Combat/Items/CombatItemData.h"
#include "Combat/Unit/CombatUnitCharacter.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Layout/WidgetPath.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Text/STextBlock.h"

class FDefenseMerchantSealUIProbe : public IAutomationLatentCommand
{
public:
    explicit FDefenseMerchantSealUIProbe(FAutomationTestBase* In) : Test(In) {}
    bool Update() override
    {
        if (Started == 0.0) Started = FPlatformTime::Seconds();
        if (FPlatformTime::Seconds() - Started > 60.0)
        {
            Test->AddError(FString::Printf(TEXT("Merchant seal PIE timed out stage %d"), Stage));
            return true;
        }
        UWorld* World = GEditor ? GEditor->PlayWorld.Get() : nullptr;
        ADefensePlayerController* PC = World ? Cast<ADefensePlayerController>(World->GetFirstPlayerController()) : nullptr;
        ADefenseHUD* HUD = PC ? Cast<ADefenseHUD>(PC->GetHUD()) : nullptr;
        UCombatShopWidget* Shop = HUD ? HUD->GetShopWidget() : nullptr;
        ACombatUnitCharacter* Hero = PC ? PC->GetCommandedUnit() : nullptr;
        if (!Hero || !Shop) return false;
        if (Hero->GetUnitDefinitionId().PrimaryAssetName != TEXT("defense_guardian"))
        {
            if (!Selected && PC->GetHeroSelection()->GetView().bCanChoose)
            {
                PC->GetHeroSelection()->RequestGuardian();
                Selected = true;
            }
            return false;
        }
        UCombatEconomyComponent* Economy = PC->GetCombatEconomyComponent();
        if (Stage == 0 && Shop->GoldButton && Shop->GoldButton->GetCachedGeometry().GetLocalSize().X > 1.0f)
        {
            Economy->AddGold(1000, TEXT("MerchantSealPIEFixture"));
            Shop->ToggleShop();
            Stage = 1;
            return false;
        }
        auto Click = [&](const TCHAR* Name)
        {
            const FPrimaryAssetId Id(TEXT("CombatItem"), Name);
            const auto* Entry = Shop->CatalogEntries.FindByPredicate([&](const auto& Candidate) { return Candidate.DefinitionId == Id; });
            if (!Entry || !Entry->HitWidget) return false;
            if (Scrolled != Id)
            {
                FWidgetPath Path;
                if (!FSlateApplication::Get().GeneratePathToWidgetUnchecked(Entry->HitWidget.ToSharedRef(), Path, EVisibility::All)) return false;
                for (int32 Index = Path.Widgets.Num() - 1; Index >= 0; --Index)
                {
                    if (Path.Widgets[Index].Widget->GetType() == FName(TEXT("SScrollBox")))
                    {
                        StaticCastSharedRef<SScrollBox>(Path.Widgets[Index].Widget)->ScrollDescendantIntoView(Entry->HitWidget, false, EDescendantScrollDestination::Center);
                        Scrolled = Id;
                        return false;
                    }
                }
                return false;
            }
            const FGeometry Geometry = Entry->HitWidget->GetCachedGeometry();
            if (Geometry.GetLocalSize().X < 1.0f) return false;
            FPointerEvent Event(0, Geometry.LocalToAbsolute(Geometry.GetLocalSize() * 0.5f), Geometry.LocalToAbsolute(Geometry.GetLocalSize() * 0.5f), TSet<FKey>{EKeys::RightMouseButton}, EKeys::RightMouseButton, 0, FModifierKeysState());
            return Shop->NativeOnPreviewMouseButtonDown(Geometry, Event).IsEventHandled();
        };
        if (Stage == 1 && Click(TEXT("item_merchants_seal")))
        {
            Stage = 2;
            return false;
        }
        if (Stage == 2 && Hero->GetCombatInventoryComponent()->GetItemCount() == 1)
        {
            int64 Price = 0;
            FString Error;
            Test->TestTrue(TEXT("Merchant seal activates in PIE"), FMath::IsNearlyEqual(Economy->GetShopDiscountFraction(), 0.05f));
            Test->TestTrue(TEXT("PIE owner quote succeeds"), Economy->CalculateCurrentItemPrice(FPrimaryAssetId(TEXT("CombatItem"), TEXT("item_repair_lens")), Price, Error));
            Test->TestEqual(TEXT("PIE repair lens quote is 190"), Price, int64(190));
            const UCombatItemData* Repair = Economy->GetShopData()->FindItem(FPrimaryAssetId(TEXT("CombatItem"), TEXT("item_repair_lens")));
            Test->TestTrue(TEXT("PIE tooltip displays discounted price"), Repair && Shop->BuildItemToolTip(*Repair).ToString().Contains(TEXT("价格 190")));
            FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir() / TEXT("DEF-016/merchant-seal-pie.png"), true, false);
            CaptureAt = FPlatformTime::Seconds();
            Stage = 3;
            return false;
        }
        auto* Build = HUD->GetBuildWidget();
        auto ClickButton = [&](const TSharedPtr<SButton>& Button)
        {
            if (!Button || Button->GetCachedGeometry().GetLocalSize().X < 1.0f) return false;
            const auto G = Button->GetCachedGeometry(); const auto Pos = G.LocalToAbsolute(G.GetLocalSize() * .5f);
            FPointerEvent Event(0, Pos, Pos, TSet<FKey>{EKeys::LeftMouseButton}, EKeys::LeftMouseButton, 0, FModifierKeysState());
            return Button->OnMouseButtonDown(G, Event).IsEventHandled();
        };
        if (Stage == 3 && FPlatformTime::Seconds() - CaptureAt > 1.0 && Click(TEXT("item_repair_lens")))
        { Stage = 4; return false; }
        if (Stage == 4 && Hero->GetCombatInventoryComponent()->GetItemCount() == 2 && Build && Build->SeedInput)
        {
            Test->TestEqual(TEXT("Second real UI purchase costs 190"), Economy->GetGold(), int64(820));
            Shop->ToggleShop(); Build->SeedInput->SetText(FText::FromString(TEXT("831907")));
            if (ClickButton(Build->StartButton)) Stage = 5;
            return false;
        }
        const auto Choice = PC->GetChoiceComponent()->GetView();
        auto HasRerollText = [&](const TCHAR* Expected)
        {
            if (!Build || !Build->RerollButton) return false;
            auto Content = Build->RerollButton->GetChildren()->GetChildAt(0);
            return Content->GetType() == FName(TEXT("STextBlock")) && StaticCastSharedRef<STextBlock>(Content)->GetText().ToString().Contains(Expected);
        };
        if (Stage == 5 && Choice.bCanReroll && HasRerollText(TEXT("重抽 · 57 金币")))
        {
            Test->TestEqual(TEXT("Actual reroll button displays 57"), Choice.RerollCost, 57);
            if (ClickButton(Build->RerollButton)) Stage = 6;
            return false;
        }
        if (Stage == 6 && Choice.RerollsUsed == 1 && HasRerollText(TEXT("重抽 · 95 金币")))
        {
            Test->TestEqual(TEXT("Actual reroll click deducts 57"), Economy->GetGold(), int64(763));
            Test->TestEqual(TEXT("Second button text is 95"), Choice.RerollCost, 95);
            FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir() / TEXT("DEF-016/merchant-reroll-pie.png"), true, false);
            CaptureAt = FPlatformTime::Seconds(); Stage = 7; return false;
        }
        if (Stage == 7 && FPlatformTime::Seconds() - CaptureAt > 1.0)
        {
            Test->AddInfo(TEXT("DefenseMerchantSealPIE CatalogPurchase=Pass Discount=5% Quote=190 Tooltip=Pass RerollText=57/95 RerollDebit=57"));
            return true;
        }
        return false;
    }
private:
    FAutomationTestBase* Test = nullptr;
    double Started = 0.0;
    double CaptureAt = 0.0;
    int32 Stage = 0;
    bool Selected = false;
    FPrimaryAssetId Scrolled;
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDefenseMerchantSealPIETest, "Combat.Defense.PIE.MerchantSeal", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDefenseMerchantSealPIETest::RunTest(const FString&)
{
    ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/Combat/Demo/Defense/L_Defense")));
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FDefensePIENavigationReady>(this));
    ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FDefenseMerchantSealUIProbe>(this));
    ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
    ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1));
    return true;
}
#endif
