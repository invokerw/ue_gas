#include "Combat/UI/CombatHUDSlotWidget.h"
#include "Combat/UI/CombatRadialProgress.h"
#include "Combat/Core/CombatTags.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Components/Button.h"
#include "Components/PanelWidget.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Blueprint/WidgetTree.h"
#include "Input/Reply.h"
#include "InputCoreTypes.h"

namespace CombatHUDSlot
{
	/** 集中处理可选文字控件，允许技能和 Buff 使用不同 Designer 树。 */
	void Text(UTextBlock* Widget, const FString& Value) { if (Widget) Widget->SetText(FText::FromString(Value)); }
}

void UCombatHUDSlotWidget::NativeConstruct()
{
	Super::NativeConstruct();
	CreateRuntimeUpgradeButton();
	if (UButton* Button = GetEffectiveUpgradeButton())
	{
		Button->OnClicked.AddUniqueDynamic(this, &UCombatHUDSlotWidget::HandleUpgradeClicked);
		Button->SetVisibility(ESlateVisibility::Collapsed);
	}
}

UButton* UCombatHUDSlotWidget::GetEffectiveUpgradeButton() const
{
	return UpgradeButton ? UpgradeButton.Get() : RuntimeUpgradeButton.Get();
}

void UCombatHUDSlotWidget::CreateRuntimeUpgradeButton()
{
	if (UpgradeButton || RuntimeUpgradeButton || !WidgetTree)
	{
		return;
	}
	// 旧版槽位蓝图的根节点可能是 SizeBox/Overlay；优先向下查找 CanvasPanel，
	// 这样升级按钮仍能稳定挂在图标上方，不要求重新打开并保存每个蓝图。
	UPanelWidget* RootPanel = nullptr;
	WidgetTree->ForEachWidget([&RootPanel](UWidget* Widget)
	{
		if (!RootPanel && Cast<UCanvasPanel>(Widget))
		{
			RootPanel = Cast<UCanvasPanel>(Widget);
		}
	});
	if (!RootPanel) RootPanel = Cast<UPanelWidget>(GetRootWidget());
	if (!RootPanel)
	{
		return;
	}
	UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("RuntimeUpgradeButton"));
	UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("RuntimeUpgradeLabel"));
	if (!Button || !Label)
	{
		return;
	}
	Label->SetText(NSLOCTEXT("CombatHUD", "UpgradeAbility", "+"));
	Label->SetJustification(ETextJustify::Center);
	Button->AddChild(Label);
	UPanelSlot* AddedSlot = RootPanel->AddChild(Button);
	if (!AddedSlot)
	{
		return;
	}
	if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(AddedSlot))
	{
		CanvasSlot->SetAnchors(FAnchors(0.5f, 0.0f, 0.5f, 0.0f));
		CanvasSlot->SetAlignment(FVector2D(0.5f, 1.0f));
		CanvasSlot->SetPosition(FVector2D(0.0f, -2.0f));
		CanvasSlot->SetSize(FVector2D(24.0f, 20.0f));
	}
	RuntimeUpgradeButton = Button;
	Button->OnClicked.AddUniqueDynamic(this, &UCombatHUDSlotWidget::HandleUpgradeClicked);
}

void UCombatHUDSlotWidget::HandleUpgradeClicked()
{
	OnUpgradeRequested.Broadcast(this);
}

float UCombatHUDSlotWidget::Remaining(double EndTime, double ServerTime)
{
	return FMath::IsFinite(EndTime) && FMath::IsFinite(ServerTime)
		? static_cast<float>(FMath::Max(0.0, EndTime - ServerTime)) : 0.0f;
}

void UCombatHUDSlotWidget::SetIcon(UTexture2D* Texture, const FText& Name)
{
	if (IconImage)
	{
		IconImage->SetBrushFromTexture(Texture);
		IconImage->SetVisibility(Texture ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (SymbolText)
	{
		SymbolText->SetText(FText::FromString(Name.ToString().Left(1)));
		SymbolText->SetVisibility(Texture ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
}

void UCombatHUDSlotWidget::ShowAbility(const FCombatHUDAbilityView& Ability, const FCombatUnitView& Unit,
	double ServerTime, const FText& Name, const FText& Description, UTexture2D* Texture, const FText& Key)
{
	using namespace CombatHUDSlot;
	if (!Ability.DefinitionId.IsValid()) { ClearEntry(); if (HotkeyText) HotkeyText->SetText(Key); return; }
	bIsAbilityEntry = true;
	// 某些旧版蓝图在尚未加入视口时不会执行 NativeConstruct；首次显示时补建按钮。
	CreateRuntimeUpgradeButton();
	if (UButton* Button = GetEffectiveUpgradeButton())
	{
		Button->OnClicked.AddUniqueDynamic(this, &UCombatHUDSlotWidget::HandleUpgradeClicked);
		Button->SetVisibility(Ability.bCanUpgrade ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		Button->SetToolTipText(NSLOCTEXT("CombatHUD", "UpgradeAbilityTooltip", "消耗 1 点技能点提升技能等级"));
	}
	SetIcon(Texture, Name);
	if (HotkeyText) HotkeyText->SetText(Key);
	Text(CostText, FString::Printf(TEXT("%.0f"), Ability.ManaCost));
	FString Ranks;
	for (int32 Index = 0; Index < FMath::Min(Ability.MaxLevel, 8); ++Index)
	{
		Ranks += Index < Ability.Level ? TEXT("━") : TEXT("─");
		if (Index + 1 < FMath::Min(Ability.MaxLevel, 8)) Ranks += TEXT(" ");
	}
	Text(RankText, Ranks);
	FString Blocked;
	if (Unit.LifeState != ECombatLifeState::Alive) Blocked = TEXT("阵亡");
	else if (Unit.VisibleStatusTags.HasTagExact(CombatTags::State_Stunned)) Blocked = TEXT("眩晕");
	else if (Unit.VisibleStatusTags.HasTagExact(CombatTags::State_Hexed)) Blocked = TEXT("变形");
	else if (Unit.VisibleStatusTags.HasTagExact(CombatTags::State_Frozen)) Blocked = TEXT("冻结");
	else if (!Ability.bIgnoreSilence && Unit.VisibleStatusTags.HasTagExact(CombatTags::State_Silenced)) Blocked = TEXT("沉默");
	else if (Unit.Mana + KINDA_SMALL_NUMBER < Ability.ManaCost) Blocked = TEXT("缺蓝");
	const float Seconds = Remaining(Ability.CooldownEndTime, ServerTime);
	const FString Countdown = Seconds <= 0.0f ? FString() : Seconds < 10.0f
		? FString::Printf(TEXT("%.1f"), Seconds) : FString::Printf(TEXT("%.0f"), FMath::CeilToFloat(Seconds));
	const bool bAutoCastOff = Ability.bUsesAutoCastToggleInput && !Ability.bAutoCastEnabled;
	const FString AutoCastState = Ability.bUsesAutoCastToggleInput
		? (Ability.bAutoCastEnabled ? TEXT("自动") : TEXT("关闭")) : FString();
	Text(CountText, !Blocked.IsEmpty() ? Blocked : !Countdown.IsEmpty() ? Countdown : AutoCastState);
	if (BlockedShade) BlockedShade->SetVisibility(Blocked.IsEmpty() && !bAutoCastOff ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	if (CooldownShade)
	{
		CooldownShade->SetVisibility(Seconds > 0.0f && Blocked.IsEmpty() && !bAutoCastOff ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		const float Ratio = Ability.CooldownDuration > 0.0f ? FMath::Clamp(Seconds / Ability.CooldownDuration, 0.0f, 1.0f) : 0.0f;
		CooldownShade->SetRenderScale(FVector2D(1.0f, Ratio));
	}
	const FString AutoCastDetail = Ability.bUsesAutoCastToggleInput
		? FString::Printf(TEXT("\n自动施法：%s（按 %s 切换）"), Ability.bAutoCastEnabled ? TEXT("开启") : TEXT("关闭"), *Key.ToString())
		: FString();
	const FString UpgradeDetail = Ability.bCanUpgrade ? TEXT("\n可用技能点：点击上方 + 升级") : TEXT("");
	const FString OptionalDetail = AutoCastDetail + UpgradeDetail
		+ (Ability.SpecHandle.IsValid() ? TEXT("") : TEXT("\n仅查看 · 冷却与自动施法状态未公开"));
	DetailText = FText::FromString(FString::Printf(TEXT("%s  [%s]\n等级 %d / %d\n%s\n法力消耗 %.0f%s%s"),
		*Name.ToString(), *Key.ToString(), Ability.Level, Ability.MaxLevel, *Description.ToString(), Ability.ManaCost,
		Seconds > 0 ? *FString::Printf(TEXT(" · 冷却 %.1f 秒"), Seconds) : TEXT(""), *OptionalDetail));
}

void UCombatHUDSlotWidget::ShowModifier(const FCombatModifierView& Modifier, double ServerTime,
	const FText& Name, UTexture2D* Texture)
{
	using namespace CombatHUDSlot;
	bIsAbilityEntry = false;
	if (UButton* Button = GetEffectiveUpgradeButton()) Button->SetVisibility(ESlateVisibility::Collapsed);
	SetIcon(Texture, Name);
	const bool bInfinite = Modifier.ServerEndTime <= 0.0;
	const float Seconds = Remaining(Modifier.ServerEndTime, ServerTime);
	Text(CountText, bInfinite ? TEXT("∞") : FString::Printf(TEXT("%.0fs"), FMath::CeilToFloat(Seconds)));
	Text(StackText, Modifier.StackCount > 1 ? FString::FromInt(Modifier.StackCount) : TEXT(""));
	const FLinearColor Color = Modifier.bIsDebuff ? DebuffColor : BuffColor;
	if (SymbolText) SymbolText->SetColorAndOpacity(FSlateColor(Color));
	if (DurationRing)
	{
		DurationRing->FillColor = Color;
		DurationRing->SynchronizeProperties();
		const double Duration = Modifier.ServerEndTime - Modifier.ServerStartTime;
		DurationRing->SetProgress(bInfinite ? 1.0f : Duration > 0 ? Seconds / Duration : 0.0f);
	}
	DetailText = FText::FromString(FString::Printf(TEXT("%s\n%s · %s\n层数 %d\n%s"), *Name.ToString(),
		Modifier.bIsDebuff ? TEXT("减益") : TEXT("增益"), Modifier.bDispellable ? TEXT("可驱散") : TEXT("不可驱散"),
		Modifier.StackCount, bInfinite ? TEXT("无限持续") : *FString::Printf(TEXT("剩余 %.1f 秒"), Seconds)));
}

void UCombatHUDSlotWidget::ClearEntry()
{
	bIsAbilityEntry = false;
	DetailText = FText::GetEmpty();
	SetIcon(nullptr, FText::FromString(TEXT("—")));
	for (UTextBlock* Label : { CostText.Get(), CountText.Get(), StackText.Get(), RankText.Get() })
	{
		if (Label) Label->SetText(FText::GetEmpty());
	}
	if (BlockedShade) BlockedShade->SetVisibility(ESlateVisibility::Collapsed);
	if (CooldownShade) CooldownShade->SetVisibility(ESlateVisibility::Collapsed);
	if (UButton* Button = GetEffectiveUpgradeButton()) Button->SetVisibility(ESlateVisibility::Collapsed);
}

void UCombatHUDSlotWidget::NativeOnMouseEnter(const FGeometry& Geometry, const FPointerEvent& Event)
{
	Super::NativeOnMouseEnter(Geometry, Event);
	OnDetailRequested.Broadcast(DetailText, false);
}

void UCombatHUDSlotWidget::NativeOnMouseLeave(const FPointerEvent& Event)
{
	Super::NativeOnMouseLeave(Event);
	OnDetailRequested.Broadcast(FText::GetEmpty(), false);
}

FReply UCombatHUDSlotWidget::NativeOnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event)
{
	if (Event.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		// 升级按钮区域复用同一请求处理，避免父槽位先打开详情；键盘激活仍通过按钮的 OnClicked。
		if (const UButton* Button = GetEffectiveUpgradeButton())
		{
			if (Button->GetVisibility() == ESlateVisibility::Visible
				&& Button->GetCachedGeometry().IsUnderLocation(Event.GetScreenSpacePosition()))
			{
				HandleUpgradeClicked();
				return FReply::Handled();
			}
		}
		if (bIsAbilityEntry)
		{
			// 技能槽左键与 Q/W/E/R 共用同一 Controller 入口；目标技能后续仍由世界确认。
			OnAbilityUseRequested.Broadcast(this);
			return FReply::Handled();
		}
		OnDetailRequested.Broadcast(DetailText, true);
		return FReply::Handled();
	}
	if (Event.GetEffectingButton() == EKeys::RightMouseButton && bIsAbilityEntry)
	{
		// 未瞄准时允许右键固定技能详情；父 HUD 会在瞄准期间优先取消会话。
		OnDetailRequested.Broadcast(DetailText, true);
		return FReply::Handled();
	}
	// 面板内右键也不能透传成地图移动。
	if (Event.GetEffectingButton() == EKeys::RightMouseButton) return FReply::Handled();
	return Super::NativeOnMouseButtonDown(Geometry, Event);
}

void UCombatHUDSlotWidget::NativeDestruct()
{
	if (UButton* Button = GetEffectiveUpgradeButton()) Button->OnClicked.RemoveDynamic(this, &UCombatHUDSlotWidget::HandleUpgradeClicked);
	OnDetailRequested.Clear();
	OnUpgradeRequested.Clear();
	OnAbilityUseRequested.Clear();
	Super::NativeDestruct();
}
