#include "Combat/Defense/DefenseBuildWidget.h"
#include "Combat/Defense/DefenseChoiceComponent.h"
#include "Combat/Defense/DefenseEventComponent.h"
#include "Combat/Defense/DefenseHeroSelectionComponent.h"
#include "Combat/Defense/DefensePlayerController.h"
#include "Combat/Defense/DefenseGameState.h"
#include "Combat/Defense/DefenseGameMode.h"
#include "Combat/Defense/DefenseDirectorComponent.h"
#include "Combat/UI/CombatPlayerHUD.h"
#include "Combat/UI/CombatShopWidget.h"
#include "Combat/Economy/CombatEconomyComponent.h"
#include "Engine/World.h"
#include "HAL/PlatformApplicationMisc.h"
#include "Input/Reply.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

namespace DefenseBuildUI
{
	const FLinearColor Background(0.025f,0.04f,0.07f,0.97f);
	const FLinearColor Accent(0.25f,0.85f,0.7f,1);
	/** 用可本地化文本构造统一面板标题。 */
	TSharedRef<STextBlock> Label(const FText& Text,int32 Size=14)
	{
		return SNew(STextBlock).Text(Text).Font(FCoreStyle::GetDefaultFontStyle("Regular",Size)).ColorAndOpacity(FLinearColor::White).AutoWrapText(true);
	}
}
UDefenseBuildWidget::UDefenseBuildWidget(const FObjectInitializer& Init):Super(Init)
{
	SetVisibility(ESlateVisibility::SelfHitTestInvisible);
}
UDefenseChoiceComponent* UDefenseBuildWidget::GetChoice() const
{
	const auto* PC=Cast<ADefensePlayerController>(GetOwningPlayer());
	return PC?PC->GetChoiceComponent():nullptr;
}
TSharedRef<SWidget> UDefenseBuildWidget::RebuildWidget()
{
	const auto Root=SNew(SOverlay)
	+SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center).Padding(24)
	[
		SNew(SScaleBox).IgnoreInheritedScale(true).Stretch(EStretch::ScaleToFit).StretchDirection(EStretchDirection::DownOnly)
		[
			SNew(SBox).WidthOverride(860)
			[
				SAssignNew(ChoicePanel,SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
				.BorderBackgroundColor(DefenseBuildUI::Background).Padding(22)
				[SAssignNew(ChoiceBody,SVerticalBox)]
			]
		]
	]
	+SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Top).Padding(FMargin(0,120,16,0))
	[
		SNew(SBox).WidthOverride(230)
		[
			SAssignNew(OwnedPanel,SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
			.BorderBackgroundColor(DefenseBuildUI::Background).Padding(12)
			[SAssignNew(OwnedBody,SVerticalBox)]
		]
	]
	+SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Top).Padding(FMargin(0,140,0,0))
	[
		SNew(SBox).WidthOverride(620)
		[
			SAssignNew(EventPanel,SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
			.BorderBackgroundColor(FLinearColor(0.10f,0.05f,0.02f,0.97f)).Padding(16)
			[SAssignNew(EventBody,SVerticalBox)]
		]
	];
	SeenRevision=INDEX_NONE; RefreshContents(); return Root;
}
FText UDefenseBuildWidget::CountdownText() const
{
	const auto* C=GetChoice(); const auto* State=GetWorld()?GetWorld()->GetGameState<ADefenseGameState>():nullptr;
	if(!C || !State) return FText::GetEmpty();
	const int32 Seconds=FMath::Max(0,FMath::CeilToInt(C->GetView().Offer.Deadline-State->GetServerWorldTimeSeconds()));
	return FText::FromString(FString::Printf(TEXT("%d 秒后默认选择第一张 · 世界继续运行"),Seconds));
}
FText UDefenseBuildWidget::ExplainFailure(const FName Reason)
{
	if(Reason.IsNone()) return FText::GetEmpty();
	const TMap<FName,FString> Messages={
		{TEXT("GoldOrRevision"),TEXT("金币不足或余额已更新，请重试")},{TEXT("NoAlternative"),TEXT("没有其他合法强化，未扣金币")},
		{TEXT("RerollLimit"),TEXT("本局两次重抽已用完")},{TEXT("HeroDead"),TEXT("阵亡期间不能操作；超时默认选择会在复活后生效")},
		{TEXT("ChoiceExpired"),TEXT("选择时间已结束")},{TEXT("StaleChoiceRevision"),TEXT("候选已更新，请选择新卡")},
		{TEXT("ChoiceAlreadyResolved"),TEXT("本轮已经选择完成")},{TEXT("StaleIdentity"),TEXT("英雄或对局已变化，请使用当前面板")},
		{TEXT("RequestPayloadMismatch"),TEXT("重复请求内容不一致，已拒绝")},
		{TEXT("AlreadyPurchased"),TEXT("该团队物品本局已经购买")},{TEXT("PhaseClosed"),TEXT("战斗阶段关闭团队商店")},
		{TEXT("StaleShopRevision"),TEXT("团队商店已更新，请刷新后重试")},{TEXT("InvalidDefinition"),TEXT("团队物品定义无效")},
		{TEXT("EffectFailed"),TEXT("团队效果未能完整生效，金币已退回")},{TEXT("StaleIdentity"),TEXT("英雄绑定已变化，请刷新面板")}
	};
	if(const auto* Message=Messages.Find(Reason)) return FText::FromString(*Message);
	return FText::FromString(TEXT("请求未通过服务器校验，请使用当前候选"));
}
void UDefenseBuildWidget::RefreshContents()
{
	if(!ChoiceBody || !OwnedBody) return;
	const FText PreservedSeed=SeedInput?SeedInput->GetText():FText::GetEmpty();
	GuardianButton.Reset(); ChoiceBody->ClearChildren(); OwnedBody->ClearChildren(); SeedInput.Reset(); StartButton.Reset(); RerollButton.Reset(); CardButtons.Reset();
	const auto* State=GetWorld()?GetWorld()->GetGameState<ADefenseGameState>():nullptr;
	const bool Waiting=State && State->GetRunView().bWaitingForStart;
	const auto* C=GetChoice(); const auto View=C?C->GetView():FDefenseChoiceView();
	const bool Open=View.Offer.ChoiceRevision>0 && !View.Offer.bResolved && !View.Offer.bCancelled;
	ChoicePanel->SetVisibility(Waiting||Open?EVisibility::Visible:EVisibility::Collapsed);
	OwnedPanel->SetVisibility(C?EVisibility::Visible:EVisibility::Collapsed);
	RefreshEventPanel();
	OwnedBody->AddSlot().AutoHeight()[DefenseBuildUI::Label(FText::FromString(TEXT("本局构筑")),17)];
	for(const auto& Card:View.Owned)
	{
		const FText Family=StaticEnum<EDefenseAugmentFamily>()->GetDisplayNameTextByValue(int64(Card.Family));
		OwnedBody->AddSlot().AutoHeight().Padding(0,7)[DefenseBuildUI::Label(FText::FromString(Card.Name.ToString()+TEXT(" · ")+Family.ToString()+(Card.bPending?TEXT("（待复活生效）"):TEXT(""))))];
	}
	if(View.Owned.IsEmpty()) OwnedBody->AddSlot().AutoHeight().Padding(0,6)[DefenseBuildUI::Label(FText::FromString(TEXT("尚未选择强化")),12)];
	else
	{
		TMap<EDefenseAugmentFamily,int32> FamilyCounts;
		for(const auto& Card:View.Owned) if(!Card.bPending) ++FamilyCounts.FindOrAdd(Card.Family);
		FString Summary(TEXT("家族："));
		for(const auto& Pair:FamilyCounts) Summary+=StaticEnum<EDefenseAugmentFamily>()->GetDisplayNameTextByValue(int64(Pair.Key)).ToString()+FString::Printf(TEXT(" x%d  "),Pair.Value);
		OwnedBody->AddSlot().AutoHeight().Padding(0,2)[DefenseBuildUI::Label(FText::FromString(Summary),11)];
	}
	OwnedBody->AddSlot().AutoHeight().Padding(0,5)[SNew(STextBlock).ColorAndOpacity(DefenseBuildUI::Accent).Text_Lambda([this]()
	{
		const auto* PC=Cast<ACombatPlayerController>(GetOwningPlayer());
		return FText::FromString(FString::Printf(TEXT("金币 %lld"),PC?PC->GetCombatEconomyComponent()->GetGold():0));
	})];
	if(State && !Waiting)
	{
		OwnedBody->AddSlot().AutoHeight().Padding(0,6)[DefenseBuildUI::Label(FText::FromString(
			FString::Printf(TEXT("种子摘要 %s · 已抽取 %lld"),*State->GetRunView().SeedSummary,State->GetRunView().OfferDrawCount)),10)];
#if !UE_BUILD_SHIPPING
		if(auto* Mode=GetWorld()->GetAuthGameMode<ADefenseGameMode>())
		{
			const uint64 Seed=Mode->GetDefenseDirector()->GetRunSeedForTesting();
			OwnedBody->AddSlot().AutoHeight().Padding(0,4)[SNew(SButton).OnClicked_Lambda([Seed]
			{ FPlatformApplicationMisc::ClipboardCopy(*LexToString(Seed)); return FReply::Handled(); })
			[DefenseBuildUI::Label(FText::FromString(TEXT("复制本局种子（服务器）")),10)]];
		}
#endif
	}
	if(Waiting)
	{
		ChoiceBody->AddSlot().AutoHeight()[DefenseBuildUI::Label(FText::FromString(TEXT("守护神殿")),26)];
		AddHeroPicker();
		if(GetWorld()->GetNetMode()!=NM_Client)
		{
			ChoiceBody->AddSlot().AutoHeight().Padding(0,12)[DefenseBuildUI::Label(FText::FromString(TEXT("输入种子复现同一局，或留空随机。开始后种子锁定。")))];
			ChoiceBody->AddSlot().AutoHeight().Padding(0,6)[SAssignNew(SeedInput,SEditableTextBox).Text(PreservedSeed).HintText(FText::FromString(TEXT("随机种子（可选）：12345 或 0xABCD")))];
			ChoiceBody->AddSlot().AutoHeight().Padding(0,12)[SAssignNew(StartButton,SButton).ClickMethod(EButtonClickMethod::MouseDown).ContentPadding(12).OnClicked_UObject(this,&ThisClass::StartRun)
				[DefenseBuildUI::Label(FText::FromString(TEXT("锁定种子并开始")),18)]];
			ChoiceBody->AddSlot().AutoHeight()[SNew(STextBlock).ColorAndOpacity(FLinearColor(1,0.45f,0.3f)).Text_Lambda([this]{return LocalError;})];
		}
		else ChoiceBody->AddSlot().AutoHeight().Padding(0,18)[DefenseBuildUI::Label(FText::FromString(TEXT("等待服务器开始对局")))];
		return;
	}
	if(!Open) return;
	if(State&&State->GetWaveIndex()==INDEX_NONE) AddHeroPicker();
	ChoiceBody->AddSlot().AutoHeight()[DefenseBuildUI::Label(FText::FromString(TEXT("选择本轮强化")),24)];
	ChoiceBody->AddSlot().AutoHeight().Padding(0,8)[SNew(STextBlock).ColorAndOpacity(DefenseBuildUI::Accent).Text_UObject(this,&ThisClass::CountdownText)];
	TSharedRef<SHorizontalBox> Cards=SNew(SHorizontalBox);
	for(const auto& Card:View.Cards)
	{
		TSharedPtr<SButton> CardButton;
		const FPrimaryAssetId Id=Card.DefinitionId;
		const FText Family=StaticEnum<EDefenseAugmentFamily>()->GetDisplayNameTextByValue(int64(Card.Family));
		Cards->AddSlot().FillWidth(1).Padding(5)
		[
			SAssignNew(CardButton,SButton).ClickMethod(EButtonClickMethod::MouseDown).ContentPadding(16).IsEnabled(View.bCanChoose).OnClicked_Lambda([this,Id]
			{ if(auto* Choice=GetChoice()) Choice->RequestChoice(EDefenseChoiceAction::Select,Id); return FReply::Handled(); })
			[SNew(SBox).MinDesiredHeight(160)[SNew(SVerticalBox)
			+SVerticalBox::Slot().AutoHeight()[DefenseBuildUI::Label(Family,12)]
			+SVerticalBox::Slot().AutoHeight().Padding(0,12)[DefenseBuildUI::Label(Card.Name,21)]
			+SVerticalBox::Slot().AutoHeight()[DefenseBuildUI::Label(Card.Description,15)]]]
		];
		CardButtons.Add(CardButton);
	}
	if(View.Offer.bSupply) Cards->AddSlot().FillWidth(1)[SNew(SButton).IsEnabled(View.bCanChoose).OnClicked_Lambda([this]
	{if(auto* Choice=GetChoice()) Choice->RequestChoice(EDefenseChoiceAction::Select); return FReply::Handled();})
		[DefenseBuildUI::Label(FText::FromString(TEXT("补给 · 领取 80 金币")),20)]];
	ChoiceBody->AddSlot().AutoHeight().Padding(0,12)[Cards];
	ChoiceBody->AddSlot().AutoHeight().Padding(0,6)[SAssignNew(RerollButton,SButton).ClickMethod(EButtonClickMethod::MouseDown).IsEnabled(View.bCanReroll).ContentPadding(10).OnClicked_Lambda([this]
	{if(auto* Choice=GetChoice()) Choice->RequestChoice(EDefenseChoiceAction::Reroll); return FReply::Handled();})
		[DefenseBuildUI::Label(FText::FromString(FString::Printf(TEXT("重抽 · %d 金币     本局剩余 %d 次"),View.RerollCost,FMath::Max(0,2-View.RerollsUsed))))]];
	if(!View.bCanChoose) ChoiceBody->AddSlot().AutoHeight().Padding(0,8)[DefenseBuildUI::Label(FText::FromString(TEXT("阵亡或窗口已过期：等待服务器确认，复活后应用已选强化。")),13)];
	ChoiceBody->AddSlot().AutoHeight().Padding(0,4)[DefenseBuildUI::Label(ExplainFailure(View.LastResult.FailureReason),13)];
}

void UDefenseBuildWidget::RefreshEventPanel()
{
	if (!EventBody || !EventPanel) return;
	EventSafeButton.Reset(); EventRiskButton.Reset(); EventCancelButton.Reset();
	EventBody->ClearChildren();
	const auto* State=GetWorld()?GetWorld()->GetGameState<ADefenseGameState>():nullptr;
	const auto* PC=Cast<ADefensePlayerController>(GetOwningPlayer());
	auto* Event=PC&&PC->GetEventComponent()?PC->GetEventComponent():nullptr;
	const FDefenseEventView V=State?State->GetRunView().Event:FDefenseEventView();
	const bool Open=State&&State->GetDefensePhase()==EDefensePhase::EventChoice&&V.EventId.IsValid()&&V.State!=EDefenseEventState::ResolvedSafe&&V.State!=EDefenseEventState::ResolvedRisk&&V.State!=EDefenseEventState::Cancelled&&V.State!=EDefenseEventState::Expired;
	EventPanel->SetVisibility(Open?EVisibility::Visible:EVisibility::Collapsed);
	if(!Open||!Event) return;
	EventBody->AddSlot().AutoHeight()[DefenseBuildUI::Label(V.Title,22)];
	EventBody->AddSlot().AutoHeight().Padding(0,4)[SNew(STextBlock).ColorAndOpacity(FLinearColor::White).Text_Lambda([this]
	{
		const auto* Current=GetWorld()?GetWorld()->GetGameState<ADefenseGameState>():nullptr;
		if(!Current) return FText::GetEmpty();
		const auto E=Current->GetRunView().Event;
		const int32 Seconds=FMath::Max(0,FMath::CeilToInt(E.Deadline-Current->GetServerWorldTimeSeconds()));
		return FText::FromString(FString::Printf(TEXT("%s · %d 秒"),E.State==EDefenseEventState::PendingRisk?TEXT("等待发起人确认，任一队员可取消"):TEXT("全队可选择"),Seconds));
	})];
	if(V.State!=EDefenseEventState::PendingRisk) EventBody->AddSlot().AutoHeight().Padding(0,8)[DefenseBuildUI::Label(V.SafeDescription,14)];
	EventBody->AddSlot().AutoHeight().Padding(0,8)[DefenseBuildUI::Label(V.RiskDescription,14)];
	if(V.State==EDefenseEventState::PendingRisk)
	{
		EventBody->AddSlot().AutoHeight().Padding(0,4)[SAssignNew(EventRiskButton,SButton).ClickMethod(EButtonClickMethod::MouseDown).OnClicked_Lambda([Event]{ Event->RequestOption(EDefenseEventOption::Risk); return FReply::Handled(); })[DefenseBuildUI::Label(FText::FromString(TEXT("确认风险选项")),16)]];
		EventBody->AddSlot().AutoHeight().Padding(0,4)[SAssignNew(EventCancelButton,SButton).ClickMethod(EButtonClickMethod::MouseDown).OnClicked_Lambda([Event]{ Event->RequestOption(EDefenseEventOption::Cancel); return FReply::Handled(); })[DefenseBuildUI::Label(FText::FromString(TEXT("取消风险，采用安全选项")),16)]];
	}
	else
	{
		EventBody->AddSlot().AutoHeight().Padding(0,4)[SAssignNew(EventSafeButton,SButton).ClickMethod(EButtonClickMethod::MouseDown).OnClicked_Lambda([Event]{ Event->RequestOption(EDefenseEventOption::Safe); return FReply::Handled(); })[DefenseBuildUI::Label(FText::FromString(TEXT("选择安全选项")),16)]];
		EventBody->AddSlot().AutoHeight().Padding(0,4)[SAssignNew(EventRiskButton,SButton).ClickMethod(EButtonClickMethod::MouseDown).OnClicked_Lambda([Event]{ Event->RequestOption(EDefenseEventOption::Risk); return FReply::Handled(); })[DefenseBuildUI::Label(FText::FromString(TEXT("发起风险选项（10秒可取消）")),16)]];
	}
}

FReply UDefenseBuildWidget::StartRun()
{
	auto* Mode=GetWorld()?GetWorld()->GetAuthGameMode<ADefenseGameMode>():nullptr;
	FString Error;
	if(Mode && SeedInput && !Mode->GetDefenseDirector()->StartWithSeed(SeedInput->GetText().ToString(),Error)) LocalError=FText::FromString(Error);
	return FReply::Handled();
}
void UDefenseBuildWidget::NativeTick(const FGeometry& Geometry,float Delta)
{
	Super::NativeTick(Geometry,Delta);
	const auto* C=GetChoice(); const auto* State=GetWorld()?GetWorld()->GetGameState<ADefenseGameState>():nullptr;
	const int32 Revision=C?C->GetView().Revision:0; const bool Waiting=State&&State->GetRunView().bWaitingForStart; const int32 EventRevision=State?State->GetRunView().Event.ViewRevision:0;
	const auto* PC=Cast<ADefensePlayerController>(GetOwningPlayer());
	const int32 HeroRevision=PC&&PC->GetHeroSelection()?PC->GetHeroSelection()->GetView().Revision:0;
	if(Revision!=SeenRevision || Waiting!=bSeenWaiting || HeroRevision!=SeenHeroRevision || EventRevision!=SeenEventRevision || (State&&State->GetDefensePhase()!=SeenPhase) || (State&&State->GetRunView().Event.EventId!=SeenEventId)) { SeenRevision=Revision; bSeenWaiting=Waiting; SeenHeroRevision=HeroRevision; SeenEventRevision=EventRevision; if(State) { SeenPhase=State->GetDefensePhase(); SeenEventId=State->GetRunView().Event.EventId; } RefreshContents(); }
	const auto* HUD=GetOwningPlayer()?Cast<ACombatPlayerHUD>(GetOwningPlayer()->GetHUD()):nullptr;
	const bool Shopping=HUD&&HUD->GetShopWidget()&&HUD->GetShopWidget()->IsShopOpen();
	if(OwnedPanel) OwnedPanel->SetVisibility(C&&!Shopping?EVisibility::Visible:EVisibility::Collapsed);

}
bool UDefenseBuildWidget::IsScreenPositionOverUI(const FVector2D Position) const
{
	for(const auto& Panel:{ChoicePanel,OwnedPanel,EventPanel})
		if(Panel && Panel->GetVisibility().IsVisible() && Panel->GetCachedGeometry().IsUnderLocation(Position)) return true;
	return false;
}
FReply UDefenseBuildWidget::NativeOnMouseButtonDown(const FGeometry& Geometry,const FPointerEvent& Event)
{
	return IsScreenPositionOverUI(Event.GetScreenSpacePosition())?FReply::Handled():Super::NativeOnMouseButtonDown(Geometry,Event);
}
void UDefenseBuildWidget::ReleaseSlateResources(bool bReleaseChildren)
{
	EventSafeButton.Reset(); EventRiskButton.Reset(); EventCancelButton.Reset();
	GuardianButton.Reset(); ChoicePanel.Reset(); OwnedPanel.Reset(); EventPanel.Reset(); ChoiceBody.Reset(); OwnedBody.Reset(); EventBody.Reset(); SeedInput.Reset(); StartButton.Reset(); RerollButton.Reset(); CardButtons.Reset();
	Super::ReleaseSlateResources(bReleaseChildren);
}

void UDefenseBuildWidget::AddHeroPicker()
{
	auto* PC=Cast<ADefensePlayerController>(GetOwningPlayer()); auto* Selection=PC?PC->GetHeroSelection():nullptr;
	if(!Selection||!ChoiceBody) return;
	const auto& V=Selection->GetView();
	ChoiceBody->AddSlot().AutoHeight().Padding(0,8)[DefenseBuildUI::Label(FText::FromString(V.bGuardianSelected?TEXT("当前英雄：神殿守卫 · 近战承伤"):TEXT("当前英雄：神殿游侠 · 远程输出")),18)];
	if(V.bGuardianSelected)
	{
		ChoiceBody->AddSlot().AutoHeight()[DefenseBuildUI::Label(FText::FromString(TEXT("Q 指向盾击 · W 固守护盾 · E 震地控制；被动：三次普攻受击后，下次普攻蓄盾。")),13)];
		return;
	}
	ChoiceBody->AddSlot().AutoHeight().Padding(0,4)[SAssignNew(GuardianButton,SButton).ClickMethod(EButtonClickMethod::MouseDown).ContentPadding(10)
		.IsEnabled(V.bCanChoose).OnClicked_Lambda([this]()
		{ if(auto* Owner=Cast<ADefensePlayerController>(GetOwningPlayer())) if(auto* S=Owner->GetHeroSelection()) S->RequestGuardian(); return FReply::Handled(); })
		[DefenseBuildUI::Label(FText::FromString(TEXT("改用神殿守卫 · 本局一次")),16)]];
	ChoiceBody->AddSlot().AutoHeight()[DefenseBuildUI::Label(FText::FromString(TEXT("请先选英雄，再选强化或购物；首次强化到期、获得经验、携带物品或团队购买后锁定。")),11)];
	if(!V.FailureReason.IsNone()) ChoiceBody->AddSlot().AutoHeight()[DefenseBuildUI::Label(FText::FromString(TEXT("未能更换：资格或身份已变化，或出生点被阻挡；原英雄与金币保留。")),12)];
}
