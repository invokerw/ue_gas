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
#include "Combat/Defense/DefenseGameMode.h"
#include "Combat/Defense/DefensePlayerController.h"
#include "Combat/Defense/DefenseHUD.h"
#include "Combat/Defense/DefenseBuildWidget.h"
#include "Combat/Defense/DefenseChoiceComponent.h"
#include "Combat/Unit/CombatUnitCharacter.h"
#include "Combat/Modifiers/CombatModifierComponent.h"
#include "Combat/Economy/CombatEconomyComponent.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SBorder.h"

/** 在真实 PIE 实例上向实际 Slate 按钮注入 MouseDown，观察同一 RPC/Modifier 链，不直接提交导演选择。 */
class FDefenseChoiceUIProbe : public IAutomationLatentCommand
{
public:
	explicit FDefenseChoiceUIProbe(FAutomationTestBase* T):Test(T),Started(FPlatformTime::Seconds()) {}
	virtual bool Update() override
	{
		if(FPlatformTime::Seconds()-Started>35) { Test->AddError(TEXT("Choice UI flow timed out")); return true; }
		auto* World=GEditor?GEditor->PlayWorld.Get():nullptr; if(!World) return false;
		ADefensePlayerController* PC=nullptr;
		for(TActorIterator<ADefensePlayerController> It(World);It;++It) if(It->IsLocalController()) PC=*It;
		auto* H=PC?Cast<ADefenseHUD>(PC->GetHUD()):nullptr; auto* Widget=H?H->GetBuildWidget():nullptr;
		if(!Widget) return false;
		const auto& View=PC->GetChoiceComponent()->GetView();
		if(Stage==0 && Widget->StartButton && Widget->StartButton->GetCachedGeometry().GetLocalSize().X>1)
		{
			FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("DEF-004/start-panel.png"),true,false);
			Stage=1; StageTime=FPlatformTime::Seconds(); return false;
		}
		if(Stage==1 && FPlatformTime::Seconds()-StageTime>1)
		{
			Widget->SeedInput->SetText(FText::FromString(TEXT("831907")));
			Click(Widget->StartButton); Stage=2; return false;
		}
		if(Stage==2 && Widget->CardButtons.Num()==3 && Widget->RerollButton && View.bCanChoose)
		{
			const auto Pos=Widget->CardButtons[0]->GetCachedGeometry().GetAbsolutePosition();
			Test->TestTrue(TEXT("Actual card geometry blocks world input"),Widget->IsScreenPositionOverUI(Pos+FVector2D(8,8)));
			FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("DEF-004/choice-panel.png"),true,false);
			Stage=3; StageTime=FPlatformTime::Seconds(); return false;
		}
		if(Stage==3 && FPlatformTime::Seconds()-StageTime>1)
		{ Click(Widget->RerollButton); Stage=4; return false; }
		if(Stage==4 && View.RerollsUsed==1 && Widget->CardButtons.Num()==3)
		{
			Test->TestEqual(TEXT("UI reroll charges 60"),PC->GetCombatEconomyComponent()->GetGold(),int64(190));
			BeforeEffects=PC->GetCommandedUnit()->GetCombatModifierComponent()->GetActiveModifierCount();
			Click(Widget->CardButtons[0]); Stage=5; return false;
		}
		if(Stage==5 && View.Offer.bResolved && View.Owned.Num()==1)
		{
			Test->TestEqual(TEXT("UI choice applies one real effect"),PC->GetCommandedUnit()->GetCombatModifierComponent()->GetActiveModifierCount(),BeforeEffects+1);
			Test->TestFalse(TEXT("Effect not pending"),View.Owned[0].bPending);
			Test->AddInfo(TEXT("DefenseChoicePIE StartButton=Pass CardButtons=3 Reroll=60 Modifier=Applied")); return true;
		}
		return false;
	}
private:
	/** 使用按钮的实际几何和事件入口；不模拟物理设备，也不修改生产点击方法。 */
	void Click(const TSharedPtr<SButton>& Button)
	{
		if(!Button) { Test->AddError(TEXT("Missing Slate button")); return; }
		const auto& G=Button->GetCachedGeometry(); const auto P=G.LocalToAbsolute(G.GetLocalSize()*0.5f);
		FPointerEvent E(0,P,P,TSet<FKey>{EKeys::LeftMouseButton},EKeys::LeftMouseButton,0,FModifierKeysState());
		Test->TestTrue(TEXT("Slate button handles click"),Button->OnMouseButtonDown(G,E).IsEventHandled());
	}
	FAutomationTestBase* Test;
	double Started,StageTime=0;
	int32 Stage=0,BeforeEffects=0;
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDefenseChoicePIETest,"Combat.Defense.PIE.ChoiceButtons",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDefenseChoicePIETest::RunTest(const FString&)
{
	ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/Combat/Demo/Defense/L_Defense")));
	ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
	FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FDefenseChoiceUIProbe>(this));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1)); return true;
}
#endif
