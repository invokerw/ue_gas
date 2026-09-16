#include "Combat/UI/CombatStashWidget.h"

#include "Combat/Economy/CombatEconomyComponent.h"
#include "Combat/Items/CombatItemData.h"
#include "Combat/UI/CombatShopWidget.h"
#include "CombatPlayerController.h"
#include "Engine/AssetManager.h"
#include "Engine/Texture2D.h"
#include "Input/Reply.h"
#include "InputCoreTypes.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

namespace CombatStashUI
{
	constexpr float SlotWidth = 42.0f;
	constexpr float SlotHeight = 32.0f;
	constexpr float ActionWidth = 112.0f;
	constexpr float ActionHeight = 34.0f;
	const FLinearColor PanelColor(0.018f, 0.026f, 0.038f, 0.96f);
	const FLinearColor SectionColor(0.035f, 0.052f, 0.068f, 0.98f);
	const FLinearColor GoldColor(0.94f, 0.72f, 0.24f, 1.0f);
}

UCombatStashWidget::UCombatStashWidget(const FObjectInitializer& Initializer) : Super(Initializer)
{
	SetVisibility(ESlateVisibility::SelfHitTestInvisible);
}

FVector2D UCombatStashWidget::GetCompactSlotSize()
{
	return FVector2D(CombatStashUI::SlotWidth, CombatStashUI::SlotHeight);
}

FVector2D UCombatStashWidget::GetCompactActionSize()
{
	return FVector2D(CombatStashUI::ActionWidth, CombatStashUI::ActionHeight);
}

TSharedRef<SWidget> UCombatStashWidget::RebuildWidget()
{
	StashHitWidgets.Reset();
	StashImages.Reset();
	StashGlyphs.Reset();
	StashQuantities.Reset();
	StashBrushes.Reset();
	IconResources.Reset();

	TSharedRef<SHorizontalBox> StashRow = SNew(SHorizontalBox);
	for (int32 SlotIndex = 0; SlotIndex < CombatEconomy::StashSlots; ++SlotIndex)
	{
		TSharedPtr<SButton> Button;
		TSharedPtr<SImage> Image;
		TSharedPtr<STextBlock> Glyph;
		TSharedPtr<STextBlock> Quantity;
		StashRow->AddSlot().AutoWidth().Padding(SlotIndex == 0 ? 0.0f : 2.0f, 0.0f, 0.0f, 0.0f)
		[
			SNew(SBox)
			.WidthOverride(CombatStashUI::SlotWidth)
			.HeightOverride(CombatStashUI::SlotHeight)
			[
				SAssignNew(Button, SButton)
				.ButtonColorAndOpacity(CombatStashUI::SectionColor)
				.ContentPadding(FMargin(1.0f))
				.OnClicked_Lambda([this, SlotIndex]()
				{
					TransferStashSlot(SlotIndex);
					return FReply::Handled();
				})
				[
					SNew(SOverlay)
					+ SOverlay::Slot()
					[
						SAssignNew(Image, SImage)
						.Visibility(EVisibility::Collapsed)
					]
					+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
					[
						SAssignNew(Glyph, STextBlock)
						.Text(NSLOCTEXT("CombatStash", "EmptySlot", "空"))
						.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
						.Justification(ETextJustify::Center)
						.ColorAndOpacity(FLinearColor(0.48f, 0.54f, 0.60f))
					]
					+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Bottom).Padding(FMargin(0.0f, 0.0f, 2.0f, 1.0f))
					[
						SAssignNew(Quantity, STextBlock)
						.Font(FCoreStyle::GetDefaultFontStyle("Bold", 8))
						.ColorAndOpacity(FLinearColor::White)
					]
				]
			]
		];
		StashHitWidgets.Add(Button);
		StashImages.Add(Image);
		StashGlyphs.Add(Glyph);
		StashQuantities.Add(Quantity);
		StashBrushes.Add(MakeShared<FSlateBrush>());
		IconResources.Add(nullptr);
	}

	TSharedRef<SWidget> Root =
		SNew(SOverlay)
		+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Bottom).Padding(FMargin(0.0f, 0.0f, 28.0f, 28.0f))
		[
			SAssignNew(StashPanel, SBorder)
			.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
			.BorderBackgroundColor(CombatStashUI::PanelColor)
			.Padding(FMargin(6.0f))
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Left)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth()
					[
						SNew(SBox).WidthOverride(CombatStashUI::ActionWidth).HeightOverride(CombatStashUI::ActionHeight)
						[
							SAssignNew(TakeAllButton, SButton)
							.ButtonColorAndOpacity(CombatStashUI::SectionColor)
							.ContentPadding(FMargin(8.0f, 2.0f))
							.Text(NSLOCTEXT("CombatStash", "TakeAll", "全部拿走"))
							.OnClicked_Lambda([this]()
							{
								if (ACombatPlayerController* Player = GetCombatPlayer()) Player->TakeAllStashItems();
								return FReply::Handled();
							})
						]
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(4.0f, 0.0f, 0.0f, 0.0f)
					[
						SNew(SBox).WidthOverride(CombatStashUI::ActionWidth).HeightOverride(CombatStashUI::ActionHeight)
						[
							SAssignNew(GoldButton, SButton)
							.ButtonColorAndOpacity(CombatStashUI::SectionColor)
							.ContentPadding(FMargin(8.0f, 2.0f))
							.ToolTipText(NSLOCTEXT("CombatStash", "GoldTip", "打开或关闭全局商店"))
							.OnClicked_Lambda([this]()
							{
								if (UCombatShopWidget* Shop = ShopWidget.Get()) Shop->SetShopOpen(!Shop->IsShopOpen());
								return FReply::Handled();
							})
							[
								SAssignNew(GoldText, STextBlock)
								.Text(NSLOCTEXT("CombatStash", "GoldSyncing", "金币 —"))
								.Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
								.ColorAndOpacity(CombatStashUI::GoldColor)
							]
						]
					]
				]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Left).Padding(0.0f, 4.0f, 0.0f, 0.0f)
				[
					StashRow
				]
			]
		];

	RefreshFromEconomy();
	return Root;
}

void UCombatStashWidget::NativeConstruct()
{
	Super::NativeConstruct();
	BindEconomy();
	RefreshFromEconomy();
}

void UCombatStashWidget::NativeDestruct()
{
	UnbindEconomy();
	ShopWidget.Reset();
	Super::NativeDestruct();
}

void UCombatStashWidget::ReleaseSlateResources(const bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);
	StashPanel.Reset();
	GoldButton.Reset();
	TakeAllButton.Reset();
	GoldText.Reset();
	StashHitWidgets.Reset();
	StashImages.Reset();
	StashGlyphs.Reset();
	StashQuantities.Reset();
	StashBrushes.Reset();
	IconResources.Reset();
}

void UCombatStashWidget::SetShopWidget(UCombatShopWidget* InShopWidget)
{
	ShopWidget = InShopWidget;
}

ACombatPlayerController* UCombatStashWidget::GetCombatPlayer() const
{
	return Cast<ACombatPlayerController>(GetOwningPlayer());
}

UCombatEconomyComponent* UCombatStashWidget::GetEconomy() const
{
	const ACombatPlayerController* Player = GetCombatPlayer();
	return Player ? Player->GetCombatEconomyComponent() : nullptr;
}

void UCombatStashWidget::BindEconomy()
{
	UCombatEconomyComponent* Economy = GetEconomy();
	if (BoundEconomy.Get() == Economy) return;
	UnbindEconomy();
	BoundEconomy = Economy;
	if (Economy) Economy->OnEconomyViewChanged.AddUniqueDynamic(this, &UCombatStashWidget::RefreshFromEconomy);
}

void UCombatStashWidget::UnbindEconomy()
{
	if (BoundEconomy.IsValid()) BoundEconomy->OnEconomyViewChanged.RemoveDynamic(this, &UCombatStashWidget::RefreshFromEconomy);
	BoundEconomy.Reset();
}

UCombatItemData* UCombatStashWidget::ResolveItem(const FPrimaryAssetId& DefinitionId)
{
	if (!DefinitionId.IsValid()) return nullptr;
	if (UCombatItemData* Item = Cast<UCombatItemData>(UAssetManager::Get().GetPrimaryAssetObject(DefinitionId))) return Item;
	return Cast<UCombatItemData>(UAssetManager::Get().GetPrimaryAssetPath(DefinitionId).TryLoad());
}

FText UCombatStashWidget::ResolveItemName(const UCombatItemData* Item)
{
	if (!Item) return NSLOCTEXT("CombatStash", "MissingItem", "缺失物品");
	return Item->DisplayNameText.IsEmpty() ? FText::FromName(Item->DefinitionName) : Item->DisplayNameText;
}

void UCombatStashWidget::RefreshFromEconomy()
{
	BindEconomy();
	if (UCombatEconomyComponent* Economy = GetEconomy()) DisplayView = Economy->GetEconomyView();
	if (GoldText)
	{
		GoldText->SetText(FText::Format(NSLOCTEXT("CombatStash", "GoldValue", "金币 {0}"), FText::AsNumber(DisplayView.Gold)));
	}
	for (int32 SlotIndex = 0; SlotIndex < CombatEconomy::StashSlots; ++SlotIndex) RefreshSlot(SlotIndex);
}

void UCombatStashWidget::RefreshSlot(const int32 SlotIndex)
{
	const FCombatItemView View = DisplayView.StashItems.IsValidIndex(SlotIndex)
		? DisplayView.StashItems[SlotIndex]
		: FCombatItemView();
	SetItemVisual(SlotIndex, View);
	if (!StashHitWidgets.IsValidIndex(SlotIndex) || !StashHitWidgets[SlotIndex]) return;
	UCombatItemData* Item = ResolveItem(View.DefinitionId);
	StashHitWidgets[SlotIndex]->SetToolTipText(View.Handle.IsValid()
		? FText::Format(NSLOCTEXT("CombatStash", "SlotTip", "{0}\n左键取出 · 右键出售"), ResolveItemName(Item))
		: NSLOCTEXT("CombatStash", "EmptySlotTip", "空储藏格"));
}

void UCombatStashWidget::SetItemVisual(const int32 SlotIndex, const FCombatItemView& View)
{
	if (!StashImages.IsValidIndex(SlotIndex) || !StashGlyphs.IsValidIndex(SlotIndex)
		|| !StashQuantities.IsValidIndex(SlotIndex) || !StashBrushes.IsValidIndex(SlotIndex)
		|| !IconResources.IsValidIndex(SlotIndex)) return;

	UCombatItemData* Item = View.Handle.IsValid() ? ResolveItem(View.DefinitionId) : nullptr;
	UTexture2D* Texture = Item && !Item->Icon.IsNull() ? Item->Icon.LoadSynchronous() : nullptr;
	IconResources[SlotIndex] = Texture;
	StashBrushes[SlotIndex] = MakeShared<FSlateBrush>();
	if (Texture)
	{
		StashBrushes[SlotIndex]->DrawAs = ESlateBrushDrawType::Image;
		StashBrushes[SlotIndex]->SetResourceObject(Texture);
		StashImages[SlotIndex]->SetImage(StashBrushes[SlotIndex].Get());
		StashImages[SlotIndex]->SetVisibility(EVisibility::HitTestInvisible);
		StashGlyphs[SlotIndex]->SetVisibility(EVisibility::Collapsed);
	}
	else
	{
		StashImages[SlotIndex]->SetImage(nullptr);
		StashImages[SlotIndex]->SetVisibility(EVisibility::Collapsed);
		const FText Name = ResolveItemName(Item);
		const FText Glyph = Item && !Item->Glyph.IsEmpty()
			? Item->Glyph
			: (Item ? FText::FromString(Name.ToString().Left(2)) : NSLOCTEXT("CombatStash", "EmptySlot", "空"));
		StashGlyphs[SlotIndex]->SetText(Glyph);
		StashGlyphs[SlotIndex]->SetColorAndOpacity(Item ? Item->Tint : FLinearColor(0.48f, 0.54f, 0.60f));
		StashGlyphs[SlotIndex]->SetVisibility(EVisibility::HitTestInvisible);
	}
	StashQuantities[SlotIndex]->SetText(View.Handle.IsValid() && View.Quantity > 1
		? FText::FromString(FString::Printf(TEXT("×%d"), View.Quantity))
		: FText::GetEmpty());
}

void UCombatStashWidget::TransferStashSlot(const int32 SlotIndex)
{
	if (!DisplayView.StashItems.IsValidIndex(SlotIndex) || !DisplayView.StashItems[SlotIndex].Handle.IsValid()) return;
	if (ACombatPlayerController* Player = GetCombatPlayer()) Player->TransferStashItem(DisplayView.StashItems[SlotIndex]);
}

void UCombatStashWidget::SellStashSlot(const int32 SlotIndex)
{
	if (!DisplayView.StashItems.IsValidIndex(SlotIndex) || !DisplayView.StashItems[SlotIndex].Handle.IsValid()) return;
	if (ACombatPlayerController* Player = GetCombatPlayer()) Player->SellStashItem(DisplayView.StashItems[SlotIndex]);
}

FReply UCombatStashWidget::NativeOnPreviewMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event)
{
	(void)Geometry;
	const FVector2D Position = Event.GetScreenSpacePosition();
	if (Event.GetEffectingButton() == EKeys::RightMouseButton)
	{
		for (int32 SlotIndex = 0; SlotIndex < StashHitWidgets.Num(); ++SlotIndex)
		{
			if (StashHitWidgets[SlotIndex] && StashHitWidgets[SlotIndex]->GetCachedGeometry().IsUnderLocation(Position))
			{
				SellStashSlot(SlotIndex);
				return FReply::Handled();
			}
		}
		if (IsScreenPositionOverUI(Position)) return FReply::Handled();
	}
	return Super::NativeOnPreviewMouseButtonDown(Geometry, Event);
}

FReply UCombatStashWidget::NativeOnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event)
{
	if (IsScreenPositionOverUI(Event.GetScreenSpacePosition())) return FReply::Handled();
	return Super::NativeOnMouseButtonDown(Geometry, Event);
}

bool UCombatStashWidget::IsScreenPositionOverUI(const FVector2D Position) const
{
	return IsVisible() && StashPanel && StashPanel->GetVisibility().IsVisible()
		&& StashPanel->GetCachedGeometry().IsUnderLocation(Position);
}
