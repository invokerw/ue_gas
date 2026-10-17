#include "CoreMinimal.h"
#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "Tests/AutomationCommon.h"
#include "Editor.h"
#include "EngineUtils.h"
#include "UnrealClient.h"
#include "Misc/Paths.h"
#include "Combat/Defense/DefenseGameState.h"
#include "Combat/Defense/DefensePlayerController.h"
#include "Combat/UI/CombatShopWidget.h"
#include "Combat/Items/CombatInventoryComponent.h"
#include "Combat/Defense/DefenseHUD.h"
#include "Combat/Defense/DefenseBuildWidget.h"
#include "Combat/Unit/CombatUnitCharacter.h"
#include "Combat/Economy/CombatEconomyComponent.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"

/** 真实地图上点击实际购买按钮，核对扣款、核心屏障、种子输入保留与战斗阶段 UI 禁用。 */
class FDefenseTeamShopUIProbe : public IAutomationLatentCommand
{
public:
	explicit FDefenseTeamShopUIProbe(FAutomationTestBase* In,bool Aid=false):Test(In),Started(FPlatformTime::Seconds()),bAid(Aid) {}
	virtual bool Update() override
	{
		if(FPlatformTime::Seconds()-Started>80) { Test->AddError(TEXT("Team shop UI timed out")); return true; }
		auto* W=GEditor?GEditor->PlayWorld.Get():nullptr; if(!W) return false;
		ADefensePlayerController* PC=nullptr;
		for(TActorIterator<ADefensePlayerController> It(W);It;++It) if(It->IsLocalController()) PC=*It;
		auto* HUD=PC?Cast<ADefenseHUD>(PC->GetHUD()):nullptr; auto* Widget=HUD?HUD->GetBuildWidget():nullptr;
		if(!Widget || !PC->GetCommandedUnit()) return false;
		auto* Shop=HUD->GetShopWidget(); if(!Shop) return false;
		auto* Economy=PC->GetCombatEconomyComponent(); const auto* V=Economy->FindShopOffer(FPrimaryAssetId(TEXT("CombatItem"),bAid?TEXT("team_reinforcement_mark"):TEXT("team_shrine_barrier")));
		if(!V) return false;
		if(Stage==0 && Widget->SeedInput && Shop->GoldButton)
		{
			Test->TestFalse(TEXT("Original shop initially closed"),Shop->IsShopOpen());
			Widget->SeedInput->SetText(FText::FromString(TEXT("831907")));
			Shop->ToggleShop(); Stage=1; return false;
		}
		if(Stage==1)
		{
			const auto* Entry=Shop->CatalogEntries.FindByPredicate([&](const auto& E){return E.DefinitionId==FPrimaryAssetId(TEXT("CombatItem"),bAid?TEXT("team_reinforcement_mark"):TEXT("team_shrine_barrier"));});
			if(!Entry||!Entry->HitWidget||Entry->HitWidget->GetCachedGeometry().GetLocalSize().X<1) return false;
			Gold=Economy->GetGold();
			const auto G=Entry->HitWidget->GetCachedGeometry(); const auto P=G.LocalToAbsolute(G.GetLocalSize()*0.5f);
			Test->TestTrue(TEXT("Original shop blocks world input"),Shop->IsScreenPositionOverUI(P));
			FPointerEvent E(0,P,P,TSet<FKey>{EKeys::RightMouseButton},EKeys::RightMouseButton,0,FModifierKeysState());
			Test->TestTrue(TEXT("Original right-click purchase handled"),Shop->NativeOnPreviewMouseButtonDown(G,E).IsEventHandled());
			Stage=2; return false;
		}
		if(Stage==2 && V->RemainingPurchases==0)
		{
			Test->TestEqual(TEXT("Original shop pays configured cost"),Economy->GetGold(),Gold-(bAid?200:220));
			if(bAid) Test->TestTrue(TEXT("Two reinforcements projected"),V->Status.ToString().Contains(TEXT("剩余援军 2 次")));
			else Test->TestEqual(TEXT("Core shield400 visible in HUD view"),W->GetGameState<ADefenseGameState>()->GetRunView().ObjectiveShield,400.0f);
			Test->TestEqual(TEXT("No inventory slot used"),PC->GetCommandedUnit()->GetCombatInventoryComponent()->GetItemCount(),0);
			Test->TestEqual(TEXT("Seed text preserved"),Widget->SeedInput->GetText().ToString(),FString(TEXT("831907")));
			FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/(bAid?TEXT("DEF-008/reinforcement-shop-pie.png"):TEXT("DEF-008/core-shop-pie.png")),true,false);
			Stage=3; CapturedAt=FPlatformTime::Seconds(); return false;
		}
		if(Stage==3 && FPlatformTime::Seconds()-CapturedAt>1) { Shop->SetShopOpen(false); Click(Widget->StartButton); Stage=4; return false; }
		if(Stage==4 && W->GetGameState<ADefenseGameState>()->GetDefensePhase()==EDefensePhase::WaveCombat)
		{
			const auto* Other=Economy->FindShopOffer(FPrimaryAssetId(TEXT("CombatItem"),TEXT("team_rally")));
			if(!Other||Other->bCanPurchase) return false;
			Test->AddInfo(bAid?TEXT("DefenseReinforcementPIE OriginalWidget=1 RightClick=1 Gold=200 Charges=2 NoSlot=1"):TEXT("DefenseUnifiedShopPIE OriginalWidget=1 RightClick=1 Gold=220 CoreShield=400 NoSlot=1 SeedPreserved=1")); return true;
		}
		return false;
	}
private:
	/** 使用 Slate 实际几何发送鼠标事件，不直接调用购买导演。 */
	void Click(TSharedPtr<SButton> Button)
	{
		const auto& G=Button->GetCachedGeometry(); const auto P=G.LocalToAbsolute(G.GetLocalSize()*0.5f);
		FPointerEvent E(0,P,P,TSet<FKey>{EKeys::LeftMouseButton},EKeys::LeftMouseButton,0,FModifierKeysState());
		Test->TestTrue(TEXT("Button handled"),Button->OnMouseButtonDown(G,E).IsEventHandled());
	}
	FAutomationTestBase* Test; double Started,CapturedAt=0; int32 Stage=0; int64 Gold=0; bool bAid=false;
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDefenseTeamShopPIETest,"Combat.Defense.PIE.TeamShopButtons",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDefenseTeamShopPIETest::RunTest(const FString&)
{
	ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/Combat/Demo/Defense/L_Defense")));
	ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
	FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FDefenseTeamShopUIProbe>(this));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1)); return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDefenseReinforcementPIETest,"Combat.Defense.PIE.ReinforcementShop",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDefenseReinforcementPIETest::RunTest(const FString&)
{
	ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/Combat/Demo/Defense/L_Defense")));
	ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
	FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FDefenseTeamShopUIProbe>(this,true));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1)); return true;
}
#endif
