#include "Combat/UI/CombatOverheadWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Fonts/SlateFontInfo.h"
#include "Styling/SlateTypes.h"

#include "Combat/Core/CombatTags.h"
#include "Combat/Unit/CombatUnitCharacter.h"
#include "Combat/View/CombatUnitViewComponent.h"

namespace CombatOverheadWidgetPrivate
{
	constexpr float WidgetWidth = 250.0f;
	constexpr float WidgetHeight = 180.0f;
	constexpr float ContentWidth = 220.0f;

	void ConfigureText(UTextBlock& Text, const int32 Size, const FLinearColor& Color)
	{
		FSlateFontInfo Font = Text.GetFont();
		Font.Size = Size;
		Font.OutlineSettings.OutlineSize = 1;
		Font.OutlineSettings.OutlineColor = FLinearColor(0.0f, 0.0f, 0.0f, 0.95f);
		Text.SetFont(Font);
		Text.SetColorAndOpacity(FSlateColor(Color));
		Text.SetShadowOffset(FVector2D(1.0f, 1.0f));
		Text.SetShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.85f));
		Text.SetJustification(ETextJustify::Center);
		Text.SetVisibility(ESlateVisibility::HitTestInvisible);
	}

	USizeBox* MakeBarRow(UWidgetTree& Tree, const FName Name, const float Height, UOverlay*& OutOverlay)
	{
		USizeBox* Size = Tree.ConstructWidget<USizeBox>(USizeBox::StaticClass(), Name);
		Size->SetHeightOverride(Height);
		OutOverlay = Tree.ConstructWidget<UOverlay>(UOverlay::StaticClass());
		Size->AddChild(OutOverlay);
		return Size;
	}

	void FillOverlay(UOverlay& Overlay, UWidget& Widget)
	{
		if (UOverlaySlot* Slot = Overlay.AddChildToOverlay(&Widget))
		{
			Slot->SetHorizontalAlignment(HAlign_Fill);
			Slot->SetVerticalAlignment(VAlign_Fill);
		}
	}

	void AddStackRow(UVerticalBox& Stack, UWidget& Widget, const FMargin Padding = FMargin(0.0f))
	{
		if (UVerticalBoxSlot* Slot = Stack.AddChildToVerticalBox(&Widget))
		{
			Slot->SetHorizontalAlignment(HAlign_Fill);
			Slot->SetVerticalAlignment(VAlign_Center);
			Slot->SetPadding(Padding);
		}
	}
}

void UCombatOverheadWidget::InitializeForUnit(ACombatUnitCharacter* InUnit)
{
	if (BoundView.IsValid())
	{
		BoundView->OnUnitViewChanged.RemoveDynamic(this, &UCombatOverheadWidget::HandleViewChanged);
		BoundView->OnModifierViewsChanged.RemoveDynamic(this, &UCombatOverheadWidget::HandleViewChanged);
	}

	BoundUnit = InUnit;
	BoundView = InUnit ? InUnit->GetCombatUnitViewComponent() : nullptr;
	if (BoundView.IsValid())
	{
		BoundView->OnUnitViewChanged.AddUniqueDynamic(this, &UCombatOverheadWidget::HandleViewChanged);
		BoundView->OnModifierViewsChanged.AddUniqueDynamic(this, &UCombatOverheadWidget::HandleViewChanged);
	}
	HandleViewChanged();
}

void UCombatOverheadWidget::AddFloatingText(const float Amount, const ECombatFloatingTextType Type)
{
	if (!FloatingCanvas || !WidgetTree || !FMath::IsFinite(Amount) || Amount <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	while (FloatingEntries.Num() >= 12)
	{
		if (UTextBlock* Oldest = FloatingEntries[0].Text.Get())
		{
			FloatingCanvas->RemoveChild(Oldest);
		}
		FloatingEntries.RemoveAt(0);
	}

	UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	FLinearColor Color;
	FString Prefix;
	switch (Type)
	{
	case ECombatFloatingTextType::MagicalDamage:
		Color = FLinearColor(0.72f, 0.43f, 1.0f, 1.0f);
		break;
	case ECombatFloatingTextType::PureDamage:
		Color = FLinearColor(1.0f, 0.91f, 0.48f, 1.0f);
		break;
	case ECombatFloatingTextType::Healing:
		Color = FLinearColor(0.35f, 1.0f, 0.42f, 1.0f);
		Prefix = TEXT("+");
		break;
	case ECombatFloatingTextType::PhysicalDamage:
	default:
		Color = FLinearColor(1.0f, 0.35f, 0.20f, 1.0f);
		break;
	}
	CombatOverheadWidgetPrivate::ConfigureText(
		*Text, Type == ECombatFloatingTextType::Healing ? 21 : 23, Color);
	Text->SetText(FText::FromString(FString::Printf(TEXT("%s%d"), *Prefix, FMath::RoundToInt(Amount))));

	UCanvasPanelSlot* CanvasSlot = FloatingCanvas->AddChildToCanvas(Text);
	CanvasSlot->SetAutoSize(true);
	CanvasSlot->SetAlignment(FVector2D(0.5f, 1.0f));
	const float OffsetPattern[] = { 0.0f, -18.0f, 18.0f, -9.0f, 10.0f };
	FFloatingEntry& Entry = FloatingEntries.AddDefaulted_GetRef();
	Entry.Text = Text;
	Entry.HorizontalOffset = OffsetPattern[FloatingSequence++ % UE_ARRAY_COUNT(OffsetPattern)];
	CanvasSlot->SetPosition(FVector2D(CombatOverheadWidgetPrivate::WidgetWidth * 0.5f + Entry.HorizontalOffset, 98.0f));
}

TSharedRef<SWidget> UCombatOverheadWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		BuildWidgetTree();
		HandleViewChanged();
	}
	return Super::RebuildWidget();
}

void UCombatOverheadWidget::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	const double Now = BoundView.IsValid() ? BoundView->GetEstimatedServerTimeSeconds()
		: (GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0);

	if (StatusContainer && StatusContainer->GetVisibility() != ESlateVisibility::Collapsed && StatusBar)
	{
		if (StatusEndTime > StatusStartTime)
		{
			const double Remaining = FMath::Max(0.0, StatusEndTime - Now);
			const float Percent = static_cast<float>(Remaining / (StatusEndTime - StatusStartTime));
			StatusBar->SetPercent(FMath::Clamp(Percent, 0.0f, 1.0f));
			StatusText->SetText(FText::FromString(FString::Printf(TEXT("%s  %.1fs"), *StatusBaseLabel, Remaining)));
			if (Remaining <= 0.0)
			{
				StatusContainer->SetVisibility(ESlateVisibility::Collapsed);
				RefreshNameVisibility();
			}
		}
		else
		{
			StatusBar->SetPercent(1.0f);
		}
	}

	if (AbilityContainer && AbilityContainer->GetVisibility() != ESlateVisibility::Collapsed && AbilityBar)
	{
		const double Duration = AbilityEndTime - AbilityStartTime;
		const double Remaining = FMath::Max(0.0, AbilityEndTime - Now);
		if (Duration > UE_DOUBLE_SMALL_NUMBER && Remaining > 0.0)
		{
			AbilityBar->SetPercent(FMath::Clamp(static_cast<float>(Remaining / Duration), 0.0f, 1.0f));
			AbilityText->SetText(FText::FromString(FString::Printf(TEXT("%s  %.1fs"), *AbilityBaseLabel, Remaining)));
		}
		else
		{
			AbilityContainer->SetVisibility(ESlateVisibility::Collapsed);
			RefreshNameVisibility();
		}
	}

	if (HealthLagBar)
	{
		if (DisplayedLagHealthPercent < HealthPercent)
		{
			DisplayedLagHealthPercent = HealthPercent;
		}
		else
		{
			DisplayedLagHealthPercent = FMath::FInterpConstantTo(
				DisplayedLagHealthPercent, HealthPercent, InDeltaTime, 0.32f);
		}
		HealthLagBar->SetPercent(DisplayedLagHealthPercent);
	}

	for (int32 Index = FloatingEntries.Num() - 1; Index >= 0; --Index)
	{
		FFloatingEntry& Entry = FloatingEntries[Index];
		UTextBlock* Text = Entry.Text.Get();
		Entry.Age += InDeltaTime;
		if (!Text || Entry.Age >= Entry.Lifetime)
		{
			if (Text && FloatingCanvas)
			{
				FloatingCanvas->RemoveChild(Text);
			}
			FloatingEntries.RemoveAtSwap(Index, 1, EAllowShrinking::No);
			continue;
		}

		const float NormalizedAge = Entry.Age / Entry.Lifetime;
		const float Drift = FMath::Sin(Entry.Age * 4.5f) * 4.0f;
		if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Text->Slot))
		{
			CanvasSlot->SetPosition(FVector2D(
				CombatOverheadWidgetPrivate::WidgetWidth * 0.5f + Entry.HorizontalOffset + Drift,
				98.0f - NormalizedAge * 58.0f));
		}
		const float Opacity = NormalizedAge < 0.68f ? 1.0f : 1.0f - (NormalizedAge - 0.68f) / 0.32f;
		Text->SetRenderOpacity(FMath::Clamp(Opacity, 0.0f, 1.0f));
		const float Scale = FMath::Lerp(1.22f, 1.0f, FMath::Clamp(NormalizedAge * 5.0f, 0.0f, 1.0f));
		Text->SetRenderTransform(FWidgetTransform(FVector2D::ZeroVector, FVector2D(Scale), FVector2D::ZeroVector, 0.0f));
	}
}

void UCombatOverheadWidget::NativeDestruct()
{
	if (BoundView.IsValid())
	{
		BoundView->OnUnitViewChanged.RemoveDynamic(this, &UCombatOverheadWidget::HandleViewChanged);
		BoundView->OnModifierViewsChanged.RemoveDynamic(this, &UCombatOverheadWidget::HandleViewChanged);
	}
	BoundView.Reset();
	BoundUnit.Reset();
	Super::NativeDestruct();
}

void UCombatOverheadWidget::HandleViewChanged()
{
	if (!InfoStack || !BoundView.IsValid())
	{
		return;
	}
	RefreshResources();
	RefreshControlState();
	RefreshAbilityState();
	RefreshNameVisibility();
}

void UCombatOverheadWidget::BuildWidgetTree()
{
	using namespace CombatOverheadWidgetPrivate;

	USizeBox* RootSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("OverheadRoot"));
	RootSize->SetWidthOverride(WidgetWidth);
	RootSize->SetHeightOverride(WidgetHeight);
	RootSize->SetVisibility(ESlateVisibility::HitTestInvisible);
	WidgetTree->RootWidget = RootSize;

	UOverlay* RootOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
	RootSize->AddChild(RootOverlay);

	FloatingCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("FloatingNumbers"));
	FillOverlay(*RootOverlay, *FloatingCanvas);

	InfoStack = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("UnitInfo"));
	if (UOverlaySlot* OverlaySlot = RootOverlay->AddChildToOverlay(InfoStack))
	{
		OverlaySlot->SetHorizontalAlignment(HAlign_Fill);
		OverlaySlot->SetVerticalAlignment(VAlign_Bottom);
		OverlaySlot->SetPadding(FMargin(15.0f, 0.0f, 15.0f, 4.0f));
	}

	NameText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("UnitName"));
	ConfigureText(*NameText, 12, FLinearColor::White);
	AddStackRow(*InfoStack, *NameText, FMargin(0.0f, 0.0f, 0.0f, 1.0f));

	UOverlay* StatusOverlay = nullptr;
	StatusContainer = MakeBarRow(*WidgetTree, TEXT("ControlStatusRow"), 16.0f, StatusOverlay);
	StatusContainer->SetVisibility(ESlateVisibility::Collapsed);
	StatusBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("ControlStatusBar"));
	StatusBar->SetFillColorAndOpacity(FLinearColor(0.95f, 0.55f, 0.08f, 1.0f));
	FillOverlay(*StatusOverlay, *StatusBar);
	StatusText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ControlStatusText"));
	ConfigureText(*StatusText, 10, FLinearColor::White);
	FillOverlay(*StatusOverlay, *StatusText);
	AddStackRow(*InfoStack, *StatusContainer, FMargin(0.0f, 0.0f, 0.0f, 2.0f));

	UOverlay* AbilityOverlay = nullptr;
	AbilityContainer = MakeBarRow(*WidgetTree, TEXT("AbilityProgressRow"), 16.0f, AbilityOverlay);
	AbilityContainer->SetVisibility(ESlateVisibility::Collapsed);
	AbilityBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("AbilityProgressBar"));
	AbilityBar->SetFillColorAndOpacity(FLinearColor(0.18f, 0.78f, 1.0f, 1.0f));
	FillOverlay(*AbilityOverlay, *AbilityBar);
	AbilityText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("AbilityProgressText"));
	ConfigureText(*AbilityText, 10, FLinearColor::White);
	FillOverlay(*AbilityOverlay, *AbilityText);
	AddStackRow(*InfoStack, *AbilityContainer, FMargin(0.0f, 0.0f, 0.0f, 2.0f));

	USizeBox* HealthSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("HealthRow"));
	HealthSize->SetHeightOverride(20.0f);
	UBorder* HealthFrame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("HealthFrame"));
	HealthFrame->SetBrushColor(FLinearColor(0.015f, 0.018f, 0.02f, 0.98f));
	HealthFrame->SetPadding(FMargin(2.0f));
	HealthSize->AddChild(HealthFrame);
	UOverlay* HealthOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("HealthOverlay"));
	HealthFrame->AddChild(HealthOverlay);
	HealthLagBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("HealthLagBar"));
	HealthLagBar->SetFillColorAndOpacity(FLinearColor(1.0f, 0.78f, 0.30f, 0.9f));
	FillOverlay(*HealthOverlay, *HealthLagBar);
	HealthBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("HealthBar"));
	FillOverlay(*HealthOverlay, *HealthBar);
	HealthSegmentCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("HealthSegments"));
	FillOverlay(*HealthOverlay, *HealthSegmentCanvas);
	HealthText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("HealthText"));
	ConfigureText(*HealthText, 10, FLinearColor::White);
	FillOverlay(*HealthOverlay, *HealthText);
	AddStackRow(*InfoStack, *HealthSize);

	UOverlay* ManaOverlay = nullptr;
	ManaContainer = MakeBarRow(*WidgetTree, TEXT("ManaRow"), 8.0f, ManaOverlay);
	ManaBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("ManaBar"));
	ManaBar->SetFillColorAndOpacity(FLinearColor(0.16f, 0.46f, 1.0f, 1.0f));
	FillOverlay(*ManaOverlay, *ManaBar);
	AddStackRow(*InfoStack, *ManaContainer, FMargin(0.0f, 2.0f, 0.0f, 0.0f));
}

void UCombatOverheadWidget::RefreshResources()
{
	const FCombatUnitView& View = BoundView->GetUnitView();
	const bool bHiddenByState = View.VisibleStatusTags.HasTagExact(CombatTags::State_NoHealthBar);
	const bool bAlive = View.LifeState == ECombatLifeState::Alive;
	InfoStack->SetVisibility(!bHiddenByState && bAlive
		? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);

	const FLinearColor TeamColor = View.TeamId.Value == 1
		? FLinearColor(0.24f, 0.76f, 0.25f, 1.0f)
		: View.TeamId.Value == 2
			? FLinearColor(0.86f, 0.20f, 0.16f, 1.0f)
			: FLinearColor(0.88f, 0.70f, 0.18f, 1.0f);
	HealthBar->SetFillColorAndOpacity(TeamColor);
	NameText->SetColorAndOpacity(FSlateColor(FLinearColor(
		FMath::Min(TeamColor.R * 1.18f, 1.0f),
		FMath::Min(TeamColor.G * 1.18f, 1.0f),
		FMath::Min(TeamColor.B * 1.18f, 1.0f), 1.0f)));
	NameText->SetText(FText::FromString(FormatDefinitionName(View.UnitDefinitionId)));

	const float NewHealthPercent = View.MaxHealth > KINDA_SMALL_NUMBER
		? FMath::Clamp(View.Health / View.MaxHealth, 0.0f, 1.0f) : 0.0f;
	if (CachedMaxHealth < 0.0f || NewHealthPercent > HealthPercent)
	{
		DisplayedLagHealthPercent = NewHealthPercent;
	}
	HealthPercent = NewHealthPercent;
	HealthBar->SetPercent(HealthPercent);
	DisplayedLagHealthPercent = FMath::Max(DisplayedLagHealthPercent, HealthPercent);
	HealthLagBar->SetPercent(DisplayedLagHealthPercent);
	HealthText->SetText(FText::FromString(FString::Printf(
		TEXT("%d / %d"), FMath::RoundToInt(View.Health), FMath::RoundToInt(View.MaxHealth))));
	if (!FMath::IsNearlyEqual(CachedMaxHealth, View.MaxHealth))
	{
		CachedMaxHealth = View.MaxHealth;
		RebuildHealthSegments(View.MaxHealth);
	}

	const bool bUsesMana = View.MaxMana > KINDA_SMALL_NUMBER;
	ManaContainer->SetVisibility(bUsesMana ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	ManaBar->SetPercent(bUsesMana ? FMath::Clamp(View.Mana / View.MaxMana, 0.0f, 1.0f) : 0.0f);
}

void UCombatOverheadWidget::RefreshControlState()
{
	const FCombatUnitView& View = BoundView->GetUnitView();
	FGameplayTag PrimaryTag;
	int32 PrimaryPriority = MIN_int32;
	FLinearColor PrimaryColor = FLinearColor::White;
	TArray<FString> Labels;

	for (const FGameplayTag& Tag : View.VisibleStatusTags)
	{
		FString Label;
		int32 TagPriority = 0;
		FLinearColor Color;
		if (!DescribeControlTag(Tag, Label, TagPriority, Color))
		{
			continue;
		}
		Labels.AddUnique(Label);
		if (TagPriority > PrimaryPriority)
		{
			PrimaryTag = Tag;
			PrimaryPriority = TagPriority;
			PrimaryColor = Color;
		}
	}

	if (!PrimaryTag.IsValid())
	{
		StatusContainer->SetVisibility(ESlateVisibility::Collapsed);
		StatusStartTime = 0.0;
		StatusEndTime = 0.0;
		return;
	}

	Labels.Sort();
	StatusBaseLabel = FString::Join(Labels, TEXT(" · "));
	StatusStartTime = 0.0;
	StatusEndTime = 0.0;
	for (const FCombatModifierView& Modifier : BoundView->GetVisibleModifiers())
	{
		if (!Modifier.ControlTags.HasTagExact(PrimaryTag))
		{
			continue;
		}
		if (Modifier.ServerEndTime <= 0.0 || Modifier.ServerEndTime > StatusEndTime)
		{
			StatusStartTime = Modifier.ServerStartTime;
			StatusEndTime = Modifier.ServerEndTime;
		}
	}
	StatusBar->SetFillColorAndOpacity(PrimaryColor);
	StatusBar->SetPercent(1.0f);
	StatusText->SetText(FText::FromString(StatusBaseLabel));
	StatusContainer->SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UCombatOverheadWidget::RefreshAbilityState()
{
	const FCombatUnitView& View = BoundView->GetUnitView();
	if (!View.ActiveAbilityDefinitionId.IsValid() || !View.AbilityActivationId.IsValid()
		|| View.AbilityServerEndTime <= View.AbilityServerStartTime)
	{
		AbilityContainer->SetVisibility(ESlateVisibility::Collapsed);
		AbilityStartTime = 0.0;
		AbilityEndTime = 0.0;
		return;
	}

	AbilityStartTime = View.AbilityServerStartTime;
	AbilityEndTime = View.AbilityServerEndTime;
	bAbilityChanneling = View.bChanneling;
	AbilityBaseLabel = FString::Printf(TEXT("%s: %s"),
		bAbilityChanneling ? TEXT("引导") : TEXT("施法"),
		*FormatDefinitionName(View.ActiveAbilityDefinitionId));
	AbilityBar->SetFillColorAndOpacity(bAbilityChanneling
		? FLinearColor(0.14f, 0.72f, 1.0f, 1.0f)
		: FLinearColor(1.0f, 0.70f, 0.12f, 1.0f));
	AbilityText->SetText(FText::FromString(AbilityBaseLabel));
	AbilityContainer->SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UCombatOverheadWidget::RefreshNameVisibility()
{
	if (!NameText || !StatusContainer || !AbilityContainer)
	{
		return;
	}
	const bool bHasProgressBar = StatusContainer->GetVisibility() != ESlateVisibility::Collapsed
		|| AbilityContainer->GetVisibility() != ESlateVisibility::Collapsed;
	NameText->SetVisibility(bHasProgressBar
		? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
}

void UCombatOverheadWidget::RebuildHealthSegments(const float MaxHealth)
{
	if (!HealthSegmentCanvas)
	{
		return;
	}
	HealthSegmentCanvas->ClearChildren();
	const int32 SegmentCount = FMath::Clamp(FMath::CeilToInt(MaxHealth / 250.0f), 1, 12);
	for (int32 Index = 1; Index < SegmentCount; ++Index)
	{
		UBorder* Divider = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Divider->SetBrushColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.52f));
		UCanvasPanelSlot* CanvasSlot = HealthSegmentCanvas->AddChildToCanvas(Divider);
		CanvasSlot->SetPosition(FVector2D(
			CombatOverheadWidgetPrivate::ContentWidth * static_cast<float>(Index) / SegmentCount, 0.0f));
		CanvasSlot->SetSize(FVector2D(1.0f, 16.0f));
	}
}

FString UCombatOverheadWidget::FormatDefinitionName(const FPrimaryAssetId& DefinitionId)
{
	if (!DefinitionId.IsValid())
	{
		return TEXT("UNIT");
	}
	FString Name = DefinitionId.PrimaryAssetName.ToString();
	Name.ReplaceInline(TEXT("_"), TEXT(" "));
	return Name.ToUpper();
}

bool UCombatOverheadWidget::DescribeControlTag(
	const FGameplayTag& Tag,
	FString& OutLabel,
	int32& OutPriority,
	FLinearColor& OutColor)
{
	if (Tag == CombatTags::State_Stunned)
	{
		OutLabel = TEXT("眩晕"); OutPriority = 100; OutColor = FLinearColor(1.0f, 0.56f, 0.08f, 1.0f); return true;
	}
	if (Tag == CombatTags::State_Hexed)
	{
		OutLabel = TEXT("妖术"); OutPriority = 95; OutColor = FLinearColor(0.80f, 0.30f, 1.0f, 1.0f); return true;
	}
	if (Tag == CombatTags::State_Frozen)
	{
		OutLabel = TEXT("冻结"); OutPriority = 90; OutColor = FLinearColor(0.18f, 0.80f, 1.0f, 1.0f); return true;
	}
	if (Tag == CombatTags::State_Rooted)
	{
		OutLabel = TEXT("缠绕"); OutPriority = 70; OutColor = FLinearColor(0.33f, 0.82f, 0.26f, 1.0f); return true;
	}
	if (Tag == CombatTags::State_Silenced)
	{
		OutLabel = TEXT("沉默"); OutPriority = 60; OutColor = FLinearColor(0.58f, 0.38f, 0.92f, 1.0f); return true;
	}
	if (Tag == CombatTags::State_Disarmed)
	{
		OutLabel = TEXT("缴械"); OutPriority = 50; OutColor = FLinearColor(0.92f, 0.27f, 0.20f, 1.0f); return true;
	}
	return false;
}
