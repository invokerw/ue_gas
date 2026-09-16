#include "Combat/UI/CombatShopWidget.h"

#include "Combat/Economy/CombatEconomyComponent.h"
#include "Combat/Items/CombatItemData.h"
#include "CombatPlayerController.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Engine/AssetManager.h"
#include "Engine/Texture2D.h"
#include "Input/Reply.h"
#include "InputCoreTypes.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateBrush.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

namespace CombatShopUI
{
	constexpr float RoundToSlateUnit(const float Value)
	{
		return static_cast<float>(static_cast<int32>(Value + 0.5f));
	}

	constexpr float ReferenceViewportWidth = 1920.0f;
	constexpr float ReferenceViewportHeight = 1080.0f;
	constexpr float ReferencePanelWidth = 760.0f;
	constexpr float PanelWidthScale = 0.60f;
	constexpr float PanelTopOffsetRatio = 0.05f;
	constexpr float PanelHeightRatio = 0.78f;
	constexpr float CatalogRegionHeightRatio = 0.55f;
	constexpr float RecipeRegionHeightRatio = 0.14f;
	constexpr float NodeWidth = 48.0f;
	constexpr float NodeHeight = 34.0f;
	constexpr float PanelWidth = ReferencePanelWidth * PanelWidthScale;
	// 基准分辨率上的比例值四舍五入到完整 Slate 单位，避免亚像素边缘。
	constexpr float PanelHeight = RoundToSlateUnit(ReferenceViewportHeight * PanelHeightRatio);
	constexpr float PanelTopOffset = ReferenceViewportHeight * PanelTopOffsetRatio;
	constexpr float CatalogRegionHeight = ReferenceViewportHeight * CatalogRegionHeightRatio;
	constexpr float RecipeRegionHeight = RoundToSlateUnit(ReferenceViewportHeight * RecipeRegionHeightRatio);
	const FLinearColor PanelColor(0.018f, 0.026f, 0.038f, 0.97f);
	const FLinearColor PanelSoftColor(0.052f, 0.078f, 0.105f, 0.98f);
	const FLinearColor SectionColor(0.08f, 0.12f, 0.16f, 0.98f);
	const FLinearColor SelectedColor(0.20f, 0.30f, 0.42f, 1.0f);
	const FLinearColor GoldColor(0.94f, 0.72f, 0.24f, 1.0f);
}

UCombatShopWidget::UCombatShopWidget(const FObjectInitializer& Initializer) : Super(Initializer)
{
	SetIsFocusable(true);
	SetVisibility(ESlateVisibility::SelfHitTestInvisible);
}

FVector2D UCombatShopWidget::GetCompactNodeSize()
{
	return FVector2D(CombatShopUI::NodeWidth, CombatShopUI::NodeHeight);
}

FVector2D UCombatShopWidget::GetReferenceViewportSize()
{
	return FVector2D(CombatShopUI::ReferenceViewportWidth, CombatShopUI::ReferenceViewportHeight);
}

FVector2D UCombatShopWidget::GetCompactPanelSize()
{
	return FVector2D(CombatShopUI::PanelWidth, CombatShopUI::PanelHeight);
}

FVector2D UCombatShopWidget::GetFixedRegionHeights()
{
	return FVector2D(CombatShopUI::CatalogRegionHeight, CombatShopUI::RecipeRegionHeight);
}

float UCombatShopWidget::GetPanelTopOffset()
{
	return CombatShopUI::PanelTopOffset;
}

float UCombatShopWidget::GetPanelWidthScale()
{
	return CombatShopUI::PanelWidthScale;
}

TSharedRef<SWidget> UCombatShopWidget::RebuildWidget()
{
	CatalogEntries.Reset();
	RecipeEntries.Reset();
	CatalogIconResources.Reset();
	RecipeIconResources.Reset();
	RecipeScrollBox.Reset();

	TSharedPtr<SBorder> PanelBorder;
	TSharedRef<SWidget> Root =
		SNew(SOverlay)
		+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Top).Padding(FMargin(0.0f, CombatShopUI::PanelTopOffset, 28.0f, 0.0f))
		[
			SAssignNew(PanelBorder, SBorder)
			.Visibility_Lambda([this]() { return bShopOpen ? EVisibility::Visible : EVisibility::Collapsed; })
			.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
			.BorderBackgroundColor(CombatShopUI::PanelColor)
			.Padding(FMargin(10.0f))
			[
				SNew(SBox).WidthOverride(CombatShopUI::PanelWidth).HeightOverride(CombatShopUI::PanelHeight)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 8.0f)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(2.0f, 0.0f, 12.0f, 0.0f)
						[
							SNew(STextBlock)
							.Text(NSLOCTEXT("CombatShop", "Title", "商店"))
							.Font(FCoreStyle::GetDefaultFontStyle("Bold", 14))
							.ColorAndOpacity(FLinearColor(0.93f, 0.96f, 0.98f))
						]
						+ SHorizontalBox::Slot().FillWidth(1.0f)
						[
							SAssignNew(SearchBox, SEditableTextBox)
							.MinDesiredWidth(250.0f)
							.HintText(NSLOCTEXT("CombatShop", "SearchHint", "搜索物品"))
							.OnTextChanged_Lambda([this](const FText& Text)
							{
								SearchText = Text.ToString();
								RebuildCatalog();
							})
						]
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 7.0f)
					[
						SNew(SUniformGridPanel).SlotPadding(FMargin(5.0f, 0.0f))
						+ SUniformGridPanel::Slot(0, 0)
						[
							SNew(SButton)
							.Text(NSLOCTEXT("CombatShop", "BasicPage", "基础物品"))
							.ButtonColorAndOpacity_Lambda([this]()
							{
								return CurrentPage == ECombatShopPage::Basic ? CombatShopUI::SelectedColor : CombatShopUI::SectionColor;
							})
							.OnClicked_Lambda([this]()
							{
								CurrentPage = ECombatShopPage::Basic;
								SelectedItem = FPrimaryAssetId();
								RebuildCatalog();
								RebuildRecipe();
								return FReply::Handled();
							})
						]
						+ SUniformGridPanel::Slot(1, 0)
						[
							SNew(SButton)
							.Text(NSLOCTEXT("CombatShop", "UpgradePage", "升级物品"))
							.ButtonColorAndOpacity_Lambda([this]()
							{
								return CurrentPage == ECombatShopPage::Upgrade ? CombatShopUI::SelectedColor : CombatShopUI::SectionColor;
							})
							.OnClicked_Lambda([this]()
							{
								CurrentPage = ECombatShopPage::Upgrade;
								SelectedItem = FPrimaryAssetId();
								RebuildCatalog();
								RebuildRecipe();
								return FReply::Handled();
							})
						]
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 5.0f)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().FillWidth(1.0f)
						[
							SNew(STextBlock)
							.Text_Lambda([this]()
							{
								return CurrentPage == ECombatShopPage::Basic
									? NSLOCTEXT("CombatShop", "BasicCatalog", "基础物品")
									: NSLOCTEXT("CombatShop", "UpgradeCatalog", "升级物品");
							})
							.ColorAndOpacity(FLinearColor(0.72f, 0.77f, 0.82f))
						]
						+ SHorizontalBox::Slot().AutoWidth()
						[
							SNew(STextBlock)
							.Text(NSLOCTEXT("CombatShop", "CatalogHelp", "左键查看 · 右键购买"))
							.ColorAndOpacity(FLinearColor(0.55f, 0.61f, 0.67f))
						]
					]
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(SBox)
						.HeightOverride(CombatShopUI::CatalogRegionHeight)
						[
							SNew(SBorder)
							.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
							.BorderBackgroundColor(CombatShopUI::PanelSoftColor)
							.Padding(FMargin(6.0f))
							[
								SNew(SScrollBox)
								+ SScrollBox::Slot()[SAssignNew(CatalogBox, SVerticalBox)]
							]
						]
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 7.0f, 0.0f, 0.0f)
					[
						SNew(SBox)
						.HeightOverride(CombatShopUI::RecipeRegionHeight)
						.Clipping(EWidgetClipping::ClipToBounds)
						[
							SNew(SBorder)
							.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
							.BorderBackgroundColor(CombatShopUI::PanelSoftColor)
							.Padding(FMargin(7.0f))
							[
								SAssignNew(RecipeBox, SVerticalBox)
							]
						]
					]
				]
			]
		];

	ShopPanel = PanelBorder;
	// 属性绑定要等下一次 Slate 布局才会求值；同步当前状态可避免首帧闪现，也不会在重建时误关已打开商店。
	PanelBorder->SetVisibility(bShopOpen ? EVisibility::Visible : EVisibility::Collapsed);
	RebuildCatalog();
	RebuildRecipe();
	RefreshFromEconomy();
	return Root;
}

void UCombatShopWidget::NativeConstruct()
{
	Super::NativeConstruct();
	BindEconomy();
	RefreshFromEconomy();
}

void UCombatShopWidget::NativeDestruct()
{
	UnbindEconomy();
	Super::NativeDestruct();
}

void UCombatShopWidget::ReleaseSlateResources(const bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);
	ShopPanel.Reset();
	SearchBox.Reset();
	CatalogBox.Reset();
	RecipeScrollBox.Reset();
	RecipeBox.Reset();
	CatalogEntries.Reset();
	RecipeEntries.Reset();
	CatalogIconResources.Reset();
	RecipeIconResources.Reset();
}

ACombatPlayerController* UCombatShopWidget::GetCombatPlayer() const
{
	return Cast<ACombatPlayerController>(GetOwningPlayer());
}

UCombatEconomyComponent* UCombatShopWidget::GetEconomy() const
{
	const ACombatPlayerController* Player = GetCombatPlayer();
	return Player ? Player->GetCombatEconomyComponent() : nullptr;
}

void UCombatShopWidget::BindEconomy()
{
	UCombatEconomyComponent* Economy = GetEconomy();
	if (BoundEconomy.Get() == Economy) return;
	UnbindEconomy();
	BoundEconomy = Economy;
	if (Economy) Economy->OnEconomyViewChanged.AddUniqueDynamic(this, &UCombatShopWidget::RefreshFromEconomy);
}

void UCombatShopWidget::UnbindEconomy()
{
	if (BoundEconomy.IsValid()) BoundEconomy->OnEconomyViewChanged.RemoveDynamic(this, &UCombatShopWidget::RefreshFromEconomy);
	BoundEconomy.Reset();
}

UCombatShopData* UCombatShopWidget::ResolveShopData()
{
	if (DisplayShopData && DisplayShopData->GetPrimaryAssetId() == DisplayView.ShopDefinitionId) return DisplayShopData;
	DisplayShopData = nullptr;
	if (UCombatEconomyComponent* Economy = GetEconomy()) DisplayShopData = Economy->GetShopData();
	if (!DisplayShopData && DisplayView.ShopDefinitionId.IsValid())
	{
		DisplayShopData = Cast<UCombatShopData>(UAssetManager::Get().GetPrimaryAssetObject(DisplayView.ShopDefinitionId));
		if (!DisplayShopData)
		{
			const FSoftObjectPath Path = UAssetManager::Get().GetPrimaryAssetPath(DisplayView.ShopDefinitionId);
			if (Path.IsValid()) DisplayShopData = Cast<UCombatShopData>(Path.TryLoad());
		}
	}
	return DisplayShopData;
}

UCombatItemData* UCombatShopWidget::ResolveItem(const FPrimaryAssetId& DefinitionId) const
{
	if (!DefinitionId.IsValid()) return nullptr;
	// 先复用当前目录中的已解析对象；测试瞬态定义和尚未进入 AssetRegistry 的本地目录也应可展示。
	if (DisplayShopData)
	{
		for (const FCombatShopCategory& Category : DisplayShopData->Categories)
		{
			for (const TSoftObjectPtr<UCombatItemData>& Reference : Category.Items)
			{
				UCombatItemData* Candidate = Reference.Get();
				if (!Candidate) Candidate = Reference.LoadSynchronous();
				if (Candidate && Candidate->GetPrimaryAssetId() == DefinitionId) return Candidate;
			}
		}
	}
	if (UCombatItemData* Item = Cast<UCombatItemData>(UAssetManager::Get().GetPrimaryAssetObject(DefinitionId))) return Item;
	const FSoftObjectPath Path = UAssetManager::Get().GetPrimaryAssetPath(DefinitionId);
	return Path.IsValid() ? Cast<UCombatItemData>(Path.TryLoad()) : nullptr;
}

FText UCombatShopWidget::ResolveItemName(const UCombatItemData* Item)
{
	if (!Item) return NSLOCTEXT("CombatShop", "MissingItem", "缺失物品");
	return Item->DisplayNameText.IsEmpty() ? FText::FromName(Item->DefinitionName) : Item->DisplayNameText;
}

FText UCombatShopWidget::ResolveItemGlyph(const UCombatItemData& Item)
{
	if (!Item.Glyph.IsEmpty()) return Item.Glyph;
	return ResolveItemName(&Item).ToString().Left(2).IsEmpty()
		? NSLOCTEXT("CombatShop", "FallbackGlyph", "物")
		: FText::FromString(ResolveItemName(&Item).ToString().Left(2));
}

void UCombatShopWidget::RefreshFromEconomy()
{
	BindEconomy();
	if (UCombatEconomyComponent* Economy = GetEconomy()) DisplayView = Economy->GetEconomyView();
	ResolveShopData();
	RebuildCatalog();
	RebuildRecipe();
}

int32 UCombatShopWidget::CountOwned(const FPrimaryAssetId& DefinitionId) const
{
	int32 Owned = 0;
	for (const FCombatItemView& View : DisplayView.StashItems)
		if (View.Handle.IsValid() && View.DefinitionId == DefinitionId) Owned += View.Quantity;
	return Owned;
}

FText UCombatShopWidget::BuildItemToolTip(const UCombatItemData& Item) const
{
	int64 Price = 0;
	FString Error;
	if (DisplayShopData) DisplayShopData->CalculateItemPrice(Item.GetPrimaryAssetId(), Price, Error);
	const FString Description = Item.Description.IsEmpty() ? TEXT("") : TEXT("\n") + Item.Description.ToString();
	return FText::FromString(FString::Printf(TEXT("%s\n价格 %lld%s\n左键查看配方 · 右键购买"),
		*ResolveItemName(&Item).ToString(), static_cast<long long>(Price), *Description));
}

TSharedRef<SWidget> UCombatShopWidget::BuildItemNode(UCombatItemData& Item, const FText& BadgeText,
	const FText& ToolTip, TArray<FItemEntry>& Entries, TArray<TObjectPtr<UTexture2D>>& IconResources,
	const bool bSelected)
{
	TSharedPtr<SButton> Button;
	TSharedPtr<SImage> Image;
	TSharedPtr<STextBlock> Glyph;
	TSharedPtr<STextBlock> Badge;
	const FPrimaryAssetId DefinitionId = Item.GetPrimaryAssetId();
	const FLinearColor Base = Item.Tint;
	const FLinearColor NodeColor = bSelected
		? CombatShopUI::SelectedColor
		: FLinearColor(FMath::Clamp(Base.R * 0.42f + 0.035f, 0.035f, 0.34f),
			FMath::Clamp(Base.G * 0.42f + 0.050f, 0.050f, 0.36f),
			FMath::Clamp(Base.B * 0.42f + 0.065f, 0.065f, 0.40f), 1.0f);
	UTexture2D* Texture = !Item.Icon.IsNull() ? Item.Icon.LoadSynchronous() : nullptr;
	TSharedPtr<FSlateBrush> Brush;
	if (Texture)
	{
		Brush = MakeShared<FSlateBrush>();
		Brush->DrawAs = ESlateBrushDrawType::Image;
		Brush->SetResourceObject(Texture);
		IconResources.Add(Texture);
	}

	TSharedRef<SWidget> NodeContent =
		SNew(SOverlay)
		+ SOverlay::Slot()
		[
			SAssignNew(Image, SImage)
			.Image(Brush.IsValid() ? Brush.Get() : nullptr)
			.Visibility(Texture ? EVisibility::HitTestInvisible : EVisibility::Collapsed)
		]
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
		[
			SAssignNew(Glyph, STextBlock)
			.Text(ResolveItemGlyph(Item))
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", 11))
			.Justification(ETextJustify::Center)
			.ColorAndOpacity(Item.Tint)
			.Visibility(Texture ? EVisibility::Collapsed : EVisibility::HitTestInvisible)
		]
		+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Bottom).Padding(FMargin(0.0f, 0.0f, 2.0f, 0.0f))
		[
			SAssignNew(Badge, STextBlock)
			.Text(BadgeText)
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", 8))
			.ColorAndOpacity(FLinearColor::White)
		];

	if (BadgeText.IsEmpty()) Badge->SetVisibility(EVisibility::Collapsed);
	TSharedRef<SWidget> FixedNode =
		SNew(SBox).WidthOverride(CombatShopUI::NodeWidth).HeightOverride(CombatShopUI::NodeHeight)
		[
			SAssignNew(Button, SButton)
			.ButtonColorAndOpacity(NodeColor)
			.ContentPadding(FMargin(1.0f))
			.ToolTipText(ToolTip)
			.OnClicked_Lambda([this, DefinitionId]()
			{
				SelectItem(DefinitionId);
				return FReply::Handled();
			})
			[NodeContent]
		];

	FItemEntry& Entry = Entries.AddDefaulted_GetRef();
	Entry.DefinitionId = DefinitionId;
	Entry.HitWidget = Button;
	Entry.IconBrush = MoveTemp(Brush);
	return FixedNode;
}

void UCombatShopWidget::RebuildCatalog()
{
	if (!CatalogBox) return;
	CatalogBox->ClearChildren();
	CatalogEntries.Reset();
	CatalogIconResources.Reset();
	UCombatShopData* Shop = ResolveShopData();
	if (!Shop)
	{
		CatalogBox->AddSlot().AutoHeight()[SNew(STextBlock).Text(NSLOCTEXT("CombatShop", "CatalogUnavailable", "当前关卡未配置商店"))];
		return;
	}

	TArray<const FCombatShopCategory*> Categories;
	for (const FCombatShopCategory& Category : Shop->Categories)
		if (Category.Page == CurrentPage) Categories.Add(&Category);
	Categories.Sort([](const FCombatShopCategory& A, const FCombatShopCategory& B)
	{
		return A.SortOrder == B.SortOrder ? A.CategoryId.LexicalLess(B.CategoryId) : A.SortOrder < B.SortOrder;
	});

	const FString Needle = SearchText.TrimStartAndEnd().ToLower();
	TSharedRef<SUniformGridPanel> CategoryGrid = SNew(SUniformGridPanel).SlotPadding(FMargin(2.0f));
	int32 CategoryIndex = 0;
	for (const FCombatShopCategory* Category : Categories)
	{
		TSharedRef<SWrapBox> Items = SNew(SWrapBox).UseAllottedSize(false).InnerSlotPadding(FVector2D(5.0f, 5.0f));
		int32 Added = 0;
		for (const TSoftObjectPtr<UCombatItemData>& Reference : Category->Items)
		{
			UCombatItemData* Item = Reference.LoadSynchronous();
			if (!Item) continue;
			FString Haystack = ResolveItemName(Item).ToString() + TEXT(" ") + Item->DefinitionName.ToString();
			for (const FString& Keyword : Item->SearchKeywords) Haystack += TEXT(" ") + Keyword;
			if (!Needle.IsEmpty() && !Haystack.ToLower().Contains(Needle)) continue;
			int64 Price = 0;
			FString Error;
			Shop->CalculateItemPrice(Item->GetPrimaryAssetId(), Price, Error);
			const FText Badge = FText::AsNumber(Price);
			Items->AddSlot()
			[
				BuildItemNode(*Item, Badge, BuildItemToolTip(*Item), CatalogEntries, CatalogIconResources,
					Item->GetPrimaryAssetId() == SelectedItem)
			];
			++Added;
		}
		if (Added == 0) continue;
		TSharedRef<SVerticalBox> CategoryCard = SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(2.0f, 0.0f, 0.0f, 4.0f)
			[
				SNew(STextBlock).Text(Category->DisplayName).ColorAndOpacity(FLinearColor(0.72f, 0.77f, 0.82f))
			]
			+ SVerticalBox::Slot().AutoHeight()[Items];
		CategoryGrid->AddSlot(CategoryIndex % 2, CategoryIndex / 2).VAlign(VAlign_Top)[CategoryCard];
		++CategoryIndex;
	}
	if (CategoryIndex == 0)
	{
		CatalogBox->AddSlot().AutoHeight().Padding(FMargin(8.0f))
		[
			SNew(STextBlock).Text(NSLOCTEXT("CombatShop", "NoResults", "没有匹配的物品")).ColorAndOpacity(FLinearColor(0.55f, 0.61f, 0.67f))
		];
	}
	else
	{
		CatalogBox->AddSlot().AutoHeight()[CategoryGrid];
	}
}

void UCombatShopWidget::RebuildRecipe()
{
	if (!RecipeBox) return;
	RecipeBox->ClearChildren();
	RecipeEntries.Reset();
	RecipeIconResources.Reset();
	UCombatItemData* Item = ResolveItem(SelectedItem);
	if (!Item || Item->Recipe.IsEmpty()) return;

	TSharedRef<SHorizontalBox> IngredientRow = SNew(SHorizontalBox);
	for (int32 Index = 0; Index < Item->Recipe.Num(); ++Index)
	{
		const FCombatItemRecipeIngredient& Ingredient = Item->Recipe[Index];
		UCombatItemData* Child = Ingredient.Item.LoadSynchronous();
		if (!Child)
		{
			IngredientRow->AddSlot().AutoWidth().Padding(3.0f, 0.0f)
			[
				SNew(STextBlock).Text(NSLOCTEXT("CombatShop", "MissingIngredient", "缺失组件")).ColorAndOpacity(FLinearColor(0.75f, 0.35f, 0.35f))
			];
			continue;
		}
		if (Index > 0)
		{
			IngredientRow->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(2.0f, 0.0f)
			[
				SNew(STextBlock).Text(NSLOCTEXT("CombatShop", "RecipeJoin", "＋")).ColorAndOpacity(FLinearColor(0.72f, 0.60f, 0.32f))
			];
		}
		const FText OwnedBadge = FText::FromString(FString::Printf(TEXT("%d/%d"), CountOwned(Child->GetPrimaryAssetId()), Ingredient.Quantity));
		IngredientRow->AddSlot().AutoWidth().VAlign(VAlign_Bottom)
		[
			BuildItemNode(*Child, OwnedBadge, BuildItemToolTip(*Child), RecipeEntries, RecipeIconResources)
		];
	}
	RecipeBox->AddSlot().FillHeight(1.0f).Padding(FMargin(2.0f, 2.0f))
	[
		SNew(SBorder)
		.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
		.BorderBackgroundColor(FLinearColor(0.025f, 0.040f, 0.055f, 0.70f))
		.Padding(FMargin(3.0f))
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.0f, 2.0f)
			[
				BuildItemNode(*Item, NSLOCTEXT("CombatShop", "ResultBadge", "结果"), BuildItemToolTip(*Item),
					RecipeEntries, RecipeIconResources, true)
			]
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
			[
				SNew(STextBlock).Text(NSLOCTEXT("CombatShop", "RecipeArrow", "↓"))
				.ColorAndOpacity(FLinearColor(0.72f, 0.60f, 0.32f))
			]
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.0f, 1.0f, 0.0f, 2.0f)
			[
				IngredientRow
			]
		]
	];
}

void UCombatShopWidget::SelectItem(const FPrimaryAssetId DefinitionId)
{
	SelectedItem = DefinitionId;
	RebuildCatalog();
	RebuildRecipe();
}

void UCombatShopWidget::PurchaseItem(const FPrimaryAssetId DefinitionId)
{
	if (ACombatPlayerController* Player = GetCombatPlayer()) Player->PurchaseShopItem(DefinitionId);
}

void UCombatShopWidget::SetShopOpen(const bool bOpen)
{
	bShopOpen = bOpen;
	if (ShopPanel) ShopPanel->SetVisibility(bShopOpen ? EVisibility::Visible : EVisibility::Collapsed);
	if (bShopOpen)
	{
		SetKeyboardFocus();
		RebuildCatalog();
		RebuildRecipe();
	}
	else
	{
		UWidgetBlueprintLibrary::SetFocusToGameViewport();
	}
}

FReply UCombatShopWidget::NativeOnPreviewMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event)
{
	(void)Geometry;
	if (Event.GetEffectingButton() == EKeys::RightMouseButton && bShopOpen)
	{
		const FVector2D Position = Event.GetScreenSpacePosition();
		for (const FItemEntry& Entry : CatalogEntries)
		{
			if (Entry.HitWidget && Entry.HitWidget->GetCachedGeometry().IsUnderLocation(Position))
			{
				PurchaseItem(Entry.DefinitionId);
				return FReply::Handled();
			}
		}
		for (const FItemEntry& Entry : RecipeEntries)
		{
			if (Entry.HitWidget && Entry.HitWidget->GetCachedGeometry().IsUnderLocation(Position))
			{
				PurchaseItem(Entry.DefinitionId);
				return FReply::Handled();
			}
		}
		if (IsScreenPositionOverUI(Position)) return FReply::Handled();
	}
	return Super::NativeOnPreviewMouseButtonDown(Geometry, Event);
}

FReply UCombatShopWidget::NativeOnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event)
{
	if (IsScreenPositionOverUI(Event.GetScreenSpacePosition())) return FReply::Handled();
	return Super::NativeOnMouseButtonDown(Geometry, Event);
}

FReply UCombatShopWidget::NativeOnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& Event)
{
	if (Event.GetKey() == EKeys::Escape && bShopOpen)
	{
		SetShopOpen(false);
		return FReply::Handled();
	}
	return Super::NativeOnPreviewKeyDown(Geometry, Event);
}

bool UCombatShopWidget::IsScreenPositionOverUI(const FVector2D Position) const
{
	return IsVisible() && bShopOpen && ShopPanel && ShopPanel->GetVisibility().IsVisible()
		&& ShopPanel->GetCachedGeometry().IsUnderLocation(Position);
}
