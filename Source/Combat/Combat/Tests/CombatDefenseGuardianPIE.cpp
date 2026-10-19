#include "CoreMinimal.h"
#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "Tests/AutomationCommon.h"
#include "Editor.h"
#include "UnrealClient.h"
#include "Misc/Paths.h"
#include "NavigationSystem.h"
#include "Combat/Defense/DefenseGuardian.h"
#include "Combat/Defense/DefenseGameMode.h"
#include "Combat/Defense/DefenseGameState.h"
#include "Combat/Defense/DefensePlayerController.h"
#include "Combat/Defense/DefenseHeroSelectionComponent.h"
#include "Combat/Defense/DefenseChoiceComponent.h"
#include "Combat/Defense/DefenseHUD.h"
#include "Combat/Defense/DefenseBuildWidget.h"
#include "Combat/Ability/CombatAbilitySystemComponent.h"
#include "Combat/Order/CombatOrderComponent.h"
#include "Combat/Modifiers/CombatModifierComponent.h"
#include "Combat/Core/CombatTags.h"
#include "Combat/View/CombatUnitViewComponent.h"
#include "Combat/Log/CombatEventSubsystem.h"
#include "Combat/Economy/CombatEconomyComponent.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"

/** 先等待编辑器导航解锁，再复制为PIE世界。 */
class FGuardianNavReady : public IAutomationLatentCommand
{
public:
	explicit FGuardianNavReady(FAutomationTestBase* T):Test(T),At(FPlatformTime::Seconds()) {}
	bool Update() override
	{
		auto* W=GEditor?GEditor->GetEditorWorldContext().World():nullptr;
		if(W&&!UNavigationSystemV1::IsNavigationBeingBuiltOrLocked(W)) return true;
		if(FPlatformTime::Seconds()-At>35) { Test->AddError(TEXT("Guardian navigation not ready")); return true; } return false;
	}
private: FAutomationTestBase* Test; double At;
};
/** 真实Slate选英雄与开始；用显式测试靶证明QWE和公共普攻，不改生产波次数据。 */
class FDefenseGuardianPIEProbe : public IAutomationLatentCommand
{
public:
	explicit FDefenseGuardianPIEProbe(FAutomationTestBase* T):Test(T),At(FPlatformTime::Seconds()) {}
	~FDefenseGuardianPIEProbe() override { if(Events.IsValid()) Events->OnRecord().Remove(Record); }
	bool Update() override
	{
		if(FPlatformTime::Seconds()-At>90) { Test->AddError(FString::Printf(TEXT("Guardian PIE timeout stage%d mask%d"),Stage,Mask)); return true; }
		auto* W=GEditor?GEditor->PlayWorld.Get():nullptr; if(!W) return false;
		auto* PC=Cast<ADefensePlayerController>(W->GetFirstPlayerController()); auto* HUD=PC?Cast<ADefenseHUD>(PC->GetHUD()):nullptr; auto* Widget=HUD?HUD->GetBuildWidget():nullptr;
		if(!Widget||!PC->GetCommandedUnit()) return false; auto* Hero=PC->GetCommandedUnit();
		if(!Events.IsValid())
		{
			Events=W->GetSubsystem<UCombatEventSubsystem>(); Record=Events->OnRecord().AddLambda([this](const FCombatLogRecord& R)
			{
				if(R.EventType==CombatTags::Event_Combat_DamageApplied&&R.AppliedAmount>0&&Target.IsValid()&&R.TargetActorId==Target->GetUniqueID())
				{ if(R.Source.AbilityDefinitionId.PrimaryAssetName==TEXT("guardian_shield_bash")) Mask|=1; if(R.Source.AbilityDefinitionId.PrimaryAssetName==TEXT("guardian_ground_slam")) Mask|=4; }
			});
		}
		if(Stage==0&&Widget->GuardianButton&&Widget->GuardianButton->IsEnabled()&&Widget->GuardianButton->GetCachedGeometry().GetLocalSize().X>1&&Widget->SeedInput)
		{
			OldHero=Hero; OldAI=Hero->GetController(); CommandPawn=PC->GetPawn(); Gold=PC->GetCombatEconomyComponent()->GetGold();
			Widget->SeedInput->SetText(FText::FromString(TEXT("831907"))); Click(Widget->GuardianButton); Stage=1; return false;
		}
		if(Stage==1&&PC->GetHeroSelection()->GetView().LastRequestId>0&&!PC->GetHeroSelection()->GetView().bLastSuccess)
		{ Test->AddError(TEXT("Hero selection refused: ")+PC->GetHeroSelection()->GetView().FailureReason.ToString()); return true; }
		if(Stage==1&&Hero->IsA<ADefenseGuardianHero>()&&PC->GetHeroSelection()->GetView().bGuardianSelected&&Widget->StartButton&&Widget->SeedInput)
		{
			Test->TestTrue(TEXT("Old avatar and AI retired"),(!OldHero.IsValid()||OldHero->IsActorBeingDestroyed())&&(!OldAI.IsValid()||OldAI->IsActorBeingDestroyed()));
			Test->TestTrue(TEXT("Command pawn retained"),PC->GetPawn()==CommandPawn.Get()); Test->TestEqual(TEXT("Gold retained"),PC->GetCombatEconomyComponent()->GetGold(),Gold);
			Test->TestEqual(TEXT("Hero selection preserves seed input"),Widget->SeedInput->GetText().ToString(),FString(TEXT("831907")));
			Test->TestEqual(TEXT("Three active HUD slots"),Hero->GetCombatUnitViewComponent()->GetHUDOwnerView().Abilities.Num(),3);
			Click(Widget->StartButton); Stage=2; return false;
		}
		if(Stage==2&&Widget->CardButtons.Num()==3&&PC->GetChoiceComponent()->GetView().bCanChoose)
		{
			Click(Widget->CardButtons[0]);
			FActorSpawnParameters P; P.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			Target=W->SpawnActor<ACombatUnitCharacter>(Hero->GetActorLocation()+FVector(210,0,0),FRotator::ZeroRotator,P);
			auto* Data=NewObject<UCombatUnitData>(Target.Get()); Data->DefinitionName=TEXT("guardian_pie_target"); Data->InitialTeamId=FCombatTeamId(2); Data->BaseStats.MaxHealth=10000;
			Data->BaseStats.AttackDamage=10; Data->BaseStats.AttackRange=300; Data->BaseStats.BaseAttackTime=.3f; Data->BaseAttackPoint=.1f; Data->AttackFacingToleranceDegrees=180; Data->ExperienceReward=0;
			Target->InitializeFromUnitData(Data); FCombatOrderRequest Attack; Attack.Type=ECombatOrderType::AttackTarget; Attack.TargetUnit=Hero; Target->GetCombatOrderComponent()->IssueOrder(Attack);
			PC->ActivateCombatAbilitySlotFromHUD(1); Stage=3; return false;
		}
		const auto Modifiers=Hero->GetCombatUnitViewComponent()->GetVisibleModifiers();
		if(Stage==3&&Modifiers.ContainsByPredicate([](const auto& V){return V.DefinitionId.PrimaryAssetName==TEXT("guardian_fortify_shield");}))
		{
			Mask|=2; auto* Spec=Hero->GetCombatAbilitySystemComponent()->FindCombatAbilitySpecByDefinitionId(FPrimaryAssetId(TEXT("CombatAbility"),TEXT("guardian_shield_bash")));
			if(!Spec||!Target.IsValid()) return false; FCombatOrderBatchRequest Batch; Batch.RequestId=PC->ReserveCombatRequestId(); FCombatOrderRequest Cast;
			Cast.Type=ECombatOrderType::CastPoint; Cast.AbilitySpecHandle=Spec->Handle; Cast.bHasTargetLocation=true; Cast.TargetLocation=Target->GetActorLocation(); Batch.Orders.Add(Cast); Hero->ServerIssueOrderBatch(Batch); Stage=4; return false;
		}
		if(Stage==4&&(Mask&1)) { PC->ActivateCombatAbilitySlotFromHUD(2); Stage=5; return false; }
		if(Stage==5&&(Mask&4))
		{
			FCombatOrderBatchRequest Batch; Batch.RequestId=PC->ReserveCombatRequestId(); FCombatOrderRequest Attack; Attack.Type=ECombatOrderType::AttackTarget; Attack.TargetUnit=Target.Get(); Batch.Orders.Add(Attack); Hero->ServerIssueOrderBatch(Batch); Stage=6; return false;
		}
		if(Stage==6&&Modifiers.ContainsByPredicate([](const auto& V){return V.DefinitionId.PrimaryAssetName==TEXT("guardian_resolve_shield");}))
		{
			Test->TestEqual(TEXT("All actual QWE effects observed"),Mask,7); Test->TestFalse(TEXT("Selected hero cannot be selected again"),PC->GetHeroSelection()->GetView().bCanChoose);
			PC->RelocateCameraFromMinimap(FVector2D(Hero->GetActorLocation())); CaptureAt=FPlatformTime::Seconds()+.5; Stage=7; return false;
		}
		if(Stage==7&&FPlatformTime::Seconds()>=CaptureAt)
		{
			FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("DEF-012/guardian-pie.png"),true,false);
			Test->AddInfo(TEXT("GuardianPIE ActualSelectionButton=Pass CommandPawnRetained=1 QDamage=1 WShield=1 EDamage=1 PassiveRealAttackShield=1")); return true;
		}
		return false;
	}
private:
	/** 使用实际Slate几何的按钮事件，不绕过选择RPC入口。 */
	void Click(const TSharedPtr<SButton>& B)
	{ const auto G=B->GetCachedGeometry(); const auto P=G.LocalToAbsolute(G.GetLocalSize()*.5f); FPointerEvent E(0,P,P,TSet<FKey>{EKeys::LeftMouseButton},EKeys::LeftMouseButton,0,FModifierKeysState()); Test->TestTrue(TEXT("Visible button handles click"),B->OnMouseButtonDown(G,E).IsEventHandled()); }
	FAutomationTestBase* Test; double At,CaptureAt=0; int32 Stage=0,Mask=0; int64 Gold=0;
	TWeakObjectPtr<ACombatUnitCharacter> OldHero,Target; TWeakObjectPtr<AController> OldAI; TWeakObjectPtr<APawn> CommandPawn;
	TWeakObjectPtr<UCombatEventSubsystem> Events; FDelegateHandle Record;
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDefenseGuardianPIETest,"Combat.Defense.PIE.GuardianSelectionAndSkills",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDefenseGuardianPIETest::RunTest(const FString&)
{
	ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/Combat/Demo/Defense/L_Defense")));
	FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FGuardianNavReady>(this)); ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
	FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FDefenseGuardianPIEProbe>(this)); ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1)); ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand()); ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1)); return true;
}
#endif
