#include "Combat/UI/CombatLogWidget.h"
#include "Combat/Log/CombatLogComponent.h"
#include "Combat/Data/CombatDefinitionData.h"
#include "Blueprint/WidgetTree.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Components/Button.h"
#include "Components/CheckBox.h"
#include "Components/ComboBoxString.h"
#include "Components/Slider.h"
#include "Components/TextBlock.h"
#include "Components/RichTextBlock.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/PanelWidget.h"
#include "Components/CanvasPanelSlot.h"
#include "Engine/AssetManager.h"
#include "Engine/DataTable.h"
#include "Engine/StreamableManager.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "Styling/CoreStyle.h"
#include "Input/Reply.h"
#include "InputCoreTypes.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Application/SlateUser.h"

FVector2D CombatLogWindowLayout::ClampTranslation(const FGeometry& Viewport, const FGeometry& PanelParent, FVector2D Translation)
{
	const FVector2D ViewSize = Viewport.GetLocalSize();
	const FVector2D PanelSize = PanelParent.GetLocalSize();
	if (Translation.ContainsNaN()) Translation = FVector2D::ZeroVector;
	if (ViewSize.X <= 0.0 || ViewSize.Y <= 0.0 || PanelSize.X <= 0.0 || PanelSize.Y <= 0.0) return Translation;
	const FVector2D Origin = Viewport.AbsoluteToLocal(PanelParent.LocalToAbsolute(FVector2D::ZeroVector));
	const FVector2D Extent = Viewport.AbsoluteToLocal(PanelParent.LocalToAbsolute(PanelSize)) - Origin;
	const FVector2D Position = Viewport.AbsoluteToLocal(PanelParent.LocalToAbsolute(Translation));
	const FVector2D Clamped(
		FMath::Clamp(Position.X, 0.0, FMath::Max(0.0, ViewSize.X - Extent.X)),
		FMath::Clamp(Position.Y, 0.0, FMath::Max(0.0, ViewSize.Y - Extent.Y)));
	return PanelParent.AbsoluteToLocal(Viewport.LocalToAbsolute(Clamped));
}

UCombatLogWidget::UCombatLogWidget(const FObjectInitializer& Initializer) : Super(Initializer)
{
	LogFont = FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 16);
}

void UCombatLogWidget::NativeConstruct()
{
	Super::NativeConstruct();
	bConstructed = true;
	SetIsFocusable(true);
	BuildTextStyles();
	if (LogEntryButton) LogEntryButton->OnClicked.AddUniqueDynamic(this, &UCombatLogWidget::ToggleLog);
	if (CloseLogButton) CloseLogButton->OnClicked.AddUniqueDynamic(this, &UCombatLogWidget::CloseLog);
	bUpdatingControls = true;
	for (UCheckBox* Check : { DamageCheck.Get(), HealingCheck.Get(), AbilityCheck.Get(), StatusCheck.Get(), NonHeroCheck.Get() })
	{
		if (!Check) continue;
		Check->SetIsChecked(true);
		Check->OnCheckStateChanged.AddUniqueDynamic(this, &UCombatLogWidget::HandleCategoryChanged);
	}
	Filter = FCombatLogFilter();
	UnitOptions.Reset();
	bOptionsDirty = true;
	if (ItemCheck)
	{
		ItemCheck->SetIsChecked(false);
		ItemCheck->SetIsEnabled(false);
		ItemCheck->SetToolTipText(NSLOCTEXT("CombatLog", "ItemsUnavailable", "物品系统尚未接入"));
	}
	if (FollowLatestCheck)
	{
		FollowLatestCheck->SetIsChecked(true);
		FollowLatestCheck->OnCheckStateChanged.AddUniqueDynamic(this, &UCombatLogWidget::HandleFollowChanged);
	}
	if (AttackerCombo) AttackerCombo->OnSelectionChanged.AddUniqueDynamic(this, &UCombatLogWidget::HandleAttackerChanged);
	if (TargetCombo) TargetCombo->OnSelectionChanged.AddUniqueDynamic(this, &UCombatLogWidget::HandleTargetChanged);
	if (TimeRangeSlider)
	{
		TimeRangeSlider->SetStepSize(0.25f);
		TimeRangeSlider->SetValue(0.0f);
		TimeRangeSlider->OnValueChanged.AddUniqueDynamic(this, &UCombatLogWidget::HandleTimeRangeChanged);
	}
	if (LogScrollBox) LogScrollBox->OnUserScrolled.AddUniqueDynamic(this, &UCombatLogWidget::HandleUserScrolled);
	bUpdatingControls = false;
	HandleTimeRangeChanged(0.0f);
	UCombatLogComponent* Previous = BoundLog.Get();
	UnbindLog();
	InitializeForLog(Previous ? Previous : (GetOwningPlayer() ? GetOwningPlayer()->FindComponentByClass<UCombatLogComponent>() : nullptr));
	SetLogOpen(false);
}

void UCombatLogWidget::InitializeForLog(UCombatLogComponent* Component)
{
	if (IsDesignTime() || (BoundLog.Get() == Component && (!Component || Component->OnHistoryChanged().IsBoundToObject(this)))) return;
	UnbindLog();
	BoundLog = Component;
	if (bConstructed && Component) Component->OnHistoryChanged().AddUObject(this, &UCombatLogWidget::HandleHistoryChanged);
	HandleHistoryChanged();
}

void UCombatLogWidget::UnbindLog()
{
	++BindingRevision;
	if (BoundLog.IsValid()) BoundLog->OnHistoryChanged().RemoveAll(this);
	BoundLog.Reset();
	if (DefinitionLoad) { DefinitionLoad->CancelHandle(); DefinitionLoad.Reset(); }
	RequestedDefinitions.Reset();
}

void UCombatLogWidget::NativeDestruct()
{
	CancelWindowDrag();
	bConstructed = false;
	bLogOpen = false;
	// Slate 重建后继续使用显式指定的历史源；仅保留弱引用，订阅与旧加载必须立即释放。
	const TWeakObjectPtr<UCombatLogComponent> Previous = BoundLog;
	UnbindLog();
	BoundLog = Previous;
	if (LogEntryButton) LogEntryButton->OnClicked.RemoveAll(this);
	if (CloseLogButton) CloseLogButton->OnClicked.RemoveAll(this);
	for (UCheckBox* Check : { DamageCheck.Get(), HealingCheck.Get(), AbilityCheck.Get(), StatusCheck.Get(), NonHeroCheck.Get(), FollowLatestCheck.Get() })
		if (Check) Check->OnCheckStateChanged.RemoveAll(this);
	if (AttackerCombo) AttackerCombo->OnSelectionChanged.RemoveAll(this);
	if (TargetCombo) TargetCombo->OnSelectionChanged.RemoveAll(this);
	if (TimeRangeSlider) TimeRangeSlider->OnValueChanged.RemoveAll(this);
	if (LogScrollBox) { LogScrollBox->OnUserScrolled.RemoveAll(this); LogScrollBox->ClearChildren(); }
	Rows.Reset();
	VisibleSequences.Reset();
	Super::NativeDestruct();
}

void UCombatLogWidget::NativeTick(const FGeometry& Geometry, const float DeltaTime)
{
	Super::NativeTick(Geometry, DeltaTime);
	// 使用不含平移的布局锚点和窗口尺寸约束边界，视口变化时重复应用也不会漂移。
	if (bLogOpen && LogScale) MoveLogPanel(Geometry, LogScale->GetRenderTransform().Translation);
	RefreshAccumulator += DeltaTime;
	if (bLogOpen && RefreshAccumulator >= 0.1f)
	{
		RefreshAccumulator = 0.0f;
		RefreshDisplay();
	}
}

void UCombatLogWidget::SetLogOpen(const bool bOpen)
{
	if (!bOpen) CancelWindowDrag();
	bLogOpen = bOpen;
	if (LogPanel) LogPanel->SetVisibility(bOpen ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	if (bOpen)
	{
		bFollowLatest = true;
		if (FollowLatestCheck) FollowLatestCheck->SetIsChecked(true);
		bDirty = true;
		RefreshDisplay();
		SetKeyboardFocus();
	}
}

void UCombatLogWidget::ToggleLog() { if (bLogOpen) CloseLog(); else SetLogOpen(true); }
void UCombatLogWidget::CloseLog()
{
	SetLogOpen(false);
	UWidgetBlueprintLibrary::SetFocusToGameViewport();
}

FReply UCombatLogWidget::NativeOnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& Event)
{
	if (bLogOpen && Event.GetKey() == EKeys::Escape) { CloseLog(); return FReply::Handled(); }
	return Super::NativeOnPreviewKeyDown(Geometry, Event);
}

FReply UCombatLogWidget::NativeOnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event)
{
	if (bDraggingWindow) return FReply::Handled();
	if (!Event.IsTouchEvent() && Event.GetEffectingButton() == EKeys::LeftMouseButton && IsTitleDragLocation(Event.GetScreenSpacePosition()))
	{
		if (LogScale && Geometry.GetLocalSize().GetMin() > 0.0f)
		{
			bDraggingWindow = true;
			DragStartScreenPosition = Event.GetScreenSpacePosition();
			DragStartTranslation = LogScale->GetRenderTransform().Translation;
			DragUserIndex = Event.GetUserIndex();
			DragPointerIndex = Event.GetPointerIndex();
			return FReply::Handled().CaptureMouse(TakeWidget()).SetUserFocus(TakeWidget(), EFocusCause::Mouse);
		}
	}
	if (LogEntryButton && LogEntryButton->GetCachedGeometry().IsUnderLocation(Event.GetScreenSpacePosition())) return FReply::Handled();
	if (bLogOpen && LogPanel && LogPanel->GetCachedGeometry().IsUnderLocation(Event.GetScreenSpacePosition())) return FReply::Handled();
	return Super::NativeOnMouseButtonDown(Geometry, Event);
}

bool UCombatLogWidget::IsTitleDragLocation(const FVector2D& ScreenPosition) const
{
	if (!bLogOpen || !LogPanel || !LogTitleBar || !LogTitleBar->GetCachedGeometry().IsUnderLocation(ScreenPosition)) return false;
	for (const UWidget* Control : { static_cast<UWidget*>(CloseLogButton.Get()), static_cast<UWidget*>(FollowLatestCheck.Get()), static_cast<UWidget*>(LogEntryButton.Get()) })
		if (Control && Control->IsVisible() && Control->GetCachedGeometry().IsUnderLocation(ScreenPosition)) return false;
	return true;
}

void UCombatLogWidget::MoveLogPanel(const FGeometry& Viewport, const FVector2D& Translation)
{
	const UCanvasPanelSlot* WindowSlot = LogScale ? Cast<UCanvasPanelSlot>(LogScale->Slot) : nullptr;
	if (!LogPanel || !WindowSlot) return;
	const FGeometry& PanelGeometry = LogPanel->GetCachedGeometry();
	const FVector2D PanelSize = Viewport.AbsoluteToLocal(PanelGeometry.LocalToAbsolute(PanelGeometry.GetLocalSize()))
		- Viewport.AbsoluteToLocal(PanelGeometry.LocalToAbsolute(FVector2D::ZeroVector));
	const FGeometry UnmovedWindow = Viewport.MakeChild(FVector2f(PanelSize), FSlateLayoutTransform(FVector2f(WindowSlot->GetPosition())));
	const FVector2D Clamped = CombatLogWindowLayout::ClampTranslation(Viewport, UnmovedWindow, Translation);
	// 必须移动最外层窗口容器；只移动内部 Border 会让祖先的点击区域仍留在旧位置。
	if (!Clamped.Equals(LogScale->GetRenderTransform().Translation, 0.01)) LogScale->SetRenderTranslation(Clamped);
}

FReply UCombatLogWidget::NativeOnMouseMove(const FGeometry& Geometry, const FPointerEvent& Event)
{
	if (!bDraggingWindow) return Super::NativeOnMouseMove(Geometry, Event);
	if (Event.GetUserIndex() != DragUserIndex || Event.GetPointerIndex() != DragPointerIndex) return FReply::Unhandled();
	if (!bLogOpen || !LogScale || !Event.IsMouseButtonDown(EKeys::LeftMouseButton))
	{
		CancelWindowDrag();
		return FReply::Handled();
	}
	const FVector2D Delta = Geometry.AbsoluteToLocal(Event.GetScreenSpacePosition()) - Geometry.AbsoluteToLocal(DragStartScreenPosition);
	MoveLogPanel(Geometry, DragStartTranslation + Delta);
	return FReply::Handled();
}

FReply UCombatLogWidget::NativeOnMouseButtonUp(const FGeometry& Geometry, const FPointerEvent& Event)
{
	if (bDraggingWindow && Event.GetUserIndex() == DragUserIndex && Event.GetPointerIndex() == DragPointerIndex)
	{
		if (Event.GetEffectingButton() == EKeys::LeftMouseButton)
		{
			bDraggingWindow = false;
			return FReply::Handled().ReleaseMouseCapture();
		}
		return FReply::Handled();
	}
	return Super::NativeOnMouseButtonUp(Geometry, Event);
}

void UCombatLogWidget::CancelWindowDrag()
{
	bDraggingWindow = false;
	if (FSlateApplication::IsInitialized() && HasMouseCaptureByUser(DragUserIndex, DragPointerIndex))
	{
		if (const TSharedPtr<FSlateUser> User = FSlateApplication::Get().GetUser(DragUserIndex)) User->ReleaseCapture(DragPointerIndex);
	}
}

void UCombatLogWidget::NativeOnMouseCaptureLost(const FCaptureLostEvent& Event)
{
	bDraggingWindow = false;
	Super::NativeOnMouseCaptureLost(Event);
}

FReply UCombatLogWidget::NativeOnMouseButtonDoubleClick(const FGeometry& Geometry, const FPointerEvent& Event)
{
	return NativeOnMouseButtonDown(Geometry, Event);
}

void UCombatLogWidget::HandleHistoryChanged() { bDirty = true; bOptionsDirty = true; }

double UCombatLogWidget::GetServerTime() const
{
	const UWorld* World = GetWorld();
	const AGameStateBase* State = World ? World->GetGameState() : nullptr;
	return State ? State->GetServerWorldTimeSeconds() : (World ? World->GetTimeSeconds() : 0.0);
}

FString UCombatLogWidget::ResolveName(const FPrimaryAssetId& Id)
{
	const UCombatDefinitionData* Data = Cast<UCombatDefinitionData>(UAssetManager::Get().GetPrimaryAssetObject(Id));
	if (Data && !Data->DisplayNameText.IsEmpty()) return Data->DisplayNameText.ToString();
	return Id.IsValid() ? Id.PrimaryAssetName.ToString() : FString();
}

void UCombatLogWidget::BuildTextStyles()
{
	TextStyles = NewObject<UDataTable>(this);
	TextStyles->RowStruct = FRichTextStyleRow::StaticStruct();
	const TPair<FName, FLinearColor> Styles[] = {
		{ TEXT("Default"), TextColor }, { TEXT("time"), FLinearColor(0.39f, 0.43f, 0.49f) },
		{ TEXT("source"), SourceColor }, { TEXT("target"), FLinearColor(0.92f, 0.93f, 0.96f) },
		{ TEXT("effect"), EffectColor }, { TEXT("amount"), EffectColor },
		{ TEXT("health"), HealthColor }, { TEXT("status"), StatusColor }
	};
	for (const auto& Style : Styles)
	{
		FRichTextStyleRow Row;
		Row.TextStyle.SetFont(LogFont).SetColorAndOpacity(Style.Value);
		TextStyles->AddRow(Style.Key, Row);
	}
}

void UCombatLogWidget::RequestDefinitions(const TArray<FCombatLogEntry>& Entries)
{
	TArray<FPrimaryAssetId> Ids;
	for (const FCombatLogEntry& Entry : Entries)
		for (const FPrimaryAssetId& Id : { Entry.SourceDefinitionId, Entry.TargetDefinitionId, Entry.EffectDefinitionId })
			if (Id.IsValid()) Ids.AddUnique(Id);
	Ids.Sort([](const FPrimaryAssetId& A, const FPrimaryAssetId& B) { return A.ToString() < B.ToString(); });
	if (Ids == RequestedDefinitions) return;
	RequestedDefinitions = MoveTemp(Ids);
	++BindingRevision;
	if (DefinitionLoad) { DefinitionLoad->CancelHandle(); DefinitionLoad.Reset(); }
	TArray<FSoftObjectPath> Paths;
	for (const FPrimaryAssetId& Id : RequestedDefinitions)
	{
		const FSoftObjectPath Path = UAssetManager::Get().GetPrimaryAssetPath(Id);
		if (Path.IsValid()) Paths.AddUnique(Path);
	}
	if (Paths.IsEmpty()) return;
	const uint64 Revision = BindingRevision;
	DefinitionLoad = UAssetManager::GetStreamableManager().RequestAsyncLoad(Paths,
		FStreamableDelegate::CreateWeakLambda(this, [this, Revision]()
		{
			if (bConstructed && Revision == BindingRevision) HandleHistoryChanged();
		}));
}

void UCombatLogWidget::RefreshUnitOptions(const TArray<FCombatLogEntry>& Entries)
{
	TMap<int32, FString> Names;
	for (const FCombatLogEntry& Entry : Entries)
	{
		if (Entry.SourceActorId) Names.Add(Entry.SourceActorId, ResolveName(Entry.SourceDefinitionId));
		if (Entry.TargetActorId) Names.Add(Entry.TargetActorId, ResolveName(Entry.TargetDefinitionId));
	}
	TMap<FString, int32> Counts;
	for (auto& Pair : Names)
	{
		if (Pair.Value.IsEmpty()) Pair.Value = TEXT("未知单位");
		++Counts.FindOrAdd(Pair.Value);
	}
	TMap<FString, int32> NextOptions;
	NextOptions.Add(TEXT("全部"), 0);
	// 已离场的当前选择也先占位，避免覆盖仍在场单位的同名标签。
	for (const int32 SelectedId : { Filter.SourceActorId, Filter.TargetActorId })
		if (SelectedId && !Names.Contains(SelectedId)) NextOptions.Add(FString::Printf(TEXT("已离场 · #%d"), SelectedId), SelectedId);
	for (const auto& Pair : Names)
	{
		const FString Name = CombatLogPresentation::BuildUnitOptionLabel(Pair.Value, Pair.Key, Counts[Pair.Value] > 1, NextOptions);
		NextOptions.Add(Name, Pair.Key);
	}
	bOptionsDirty = false;
	// 高频 DOT 不能反复清空正在展开的下拉列表；只有单位集合或本地名称变化时才更新选项。
	if (NextOptions.OrderIndependentCompareEqual(UnitOptions)) return;
	UnitOptions = MoveTemp(NextOptions);
	TArray<FString> Options;
	UnitOptions.GetKeys(Options);
	Options.Sort();
	bUpdatingControls = true;
	for (UComboBoxString* Combo : { AttackerCombo.Get(), TargetCombo.Get() })
	{
		if (!Combo) continue;
		Combo->ClearOptions();
		Combo->AddOption(TEXT("全部"));
		const int32 SelectedId = Combo == AttackerCombo ? Filter.SourceActorId : Filter.TargetActorId;
		for (const FString& Option : Options) if (Option != TEXT("全部")) Combo->AddOption(Option);
		for (const auto& Pair : UnitOptions) if (Pair.Value == SelectedId) { Combo->SetSelectedOption(Pair.Key); break; }
	}
	bUpdatingControls = false;
	bOptionsDirty = false;
}

void UCombatLogWidget::RefreshDisplay()
{
	if (!bConstructed || !bLogOpen || !LogScrollBox) return;
	const TArray<FCombatLogEntry> Empty;
	const TArray<FCombatLogEntry>& Entries = BoundLog.IsValid() ? BoundLog->GetEntries() : Empty;
	RequestDefinitions(Entries);
	if (bOptionsDirty) RefreshUnitOptions(Entries);
	TArray<const FCombatLogEntry*> Matches;
	TArray<int64> Sequences;
	const double Now = GetServerTime();
	for (const FCombatLogEntry& Entry : Entries)
		if (Filter.Matches(Entry, Now)) { Matches.Add(&Entry); Sequences.Add(Entry.Sequence); }
	if (!bDirty && VisibleSequences == Sequences) return;
	VisibleSequences = MoveTemp(Sequences);
	bDirty = false;
	for (int32 Index = 0; Index < Matches.Num(); ++Index)
	{
		if (!Rows.IsValidIndex(Index))
		{
			URichTextBlock* Row = WidgetTree->ConstructWidget<URichTextBlock>();
			Row->SetTextStyleSet(TextStyles);
			Row->SetAutoWrapText(true);
			Row->SetVisibility(ESlateVisibility::HitTestInvisible);
			if (UScrollBoxSlot* RowSlot = Cast<UScrollBoxSlot>(LogScrollBox->AddChild(Row))) RowSlot->SetPadding(FMargin(4.0f, 2.0f));
			Rows.Add(Row);
		}
		const FCombatLogEntry& Entry = *Matches[Index];
		Rows[Index]->SetVisibility(ESlateVisibility::HitTestInvisible);
		Rows[Index]->SetText(FText::FromString(CombatLogPresentation::BuildRichText(Entry,
			ResolveName(Entry.SourceDefinitionId), ResolveName(Entry.TargetDefinitionId), ResolveName(Entry.EffectDefinitionId))));
	}
	for (int32 Index = Matches.Num(); Index < Rows.Num(); ++Index) Rows[Index]->SetVisibility(ESlateVisibility::Collapsed);
	if (EmptyStateText)
	{
		EmptyStateText->SetVisibility(Matches.IsEmpty() ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		EmptyStateText->SetText(Entries.IsEmpty()
			? NSLOCTEXT("CombatLog", "WaitingForCombat", "暂无战斗记录\n战斗发生后将在这里显示")
			: NSLOCTEXT("CombatLog", "NoMatches", "当前筛选条件下没有记录"));
	}
	if (RecordCountText) RecordCountText->SetText(FText::FromString(FString::Printf(TEXT("显示 %d 条 · 保留最近 %d / %d 条"), Matches.Num(), Entries.Num(), CombatLogPresentation::MaxEntries)));
	if (bFollowLatest) LogScrollBox->ScrollToEnd();
}

void UCombatLogWidget::HandleCategoryChanged(const bool bChecked)
{
	if (bUpdatingControls) return;
	Filter.bDamage = DamageCheck && DamageCheck->IsChecked();
	Filter.bHealing = HealingCheck && HealingCheck->IsChecked();
	Filter.bAbility = AbilityCheck && AbilityCheck->IsChecked();
	Filter.bStatus = StatusCheck && StatusCheck->IsChecked();
	Filter.bIncludeNonHeroes = NonHeroCheck && NonHeroCheck->IsChecked();
	bDirty = true;
	RefreshDisplay();
}

void UCombatLogWidget::HandleAttackerChanged(FString Option, ESelectInfo::Type SelectionType)
{
	if (bUpdatingControls) return;
	Filter.SourceActorId = UnitOptions.FindRef(Option);
	bDirty = true;
	RefreshDisplay();
}

void UCombatLogWidget::HandleTargetChanged(FString Option, ESelectInfo::Type SelectionType)
{
	if (bUpdatingControls) return;
	Filter.TargetActorId = UnitOptions.FindRef(Option);
	bDirty = true;
	RefreshDisplay();
}

void UCombatLogWidget::HandleTimeRangeChanged(const float Value)
{
	if (bUpdatingControls) return;
	const int32 Index = FMath::Clamp(FMath::RoundToInt((FMath::IsFinite(Value) ? Value : 0.0f) * 4.0f), 0, 4);
	constexpr double Windows[] = { 30.0, 60.0, 120.0, 300.0, 0.0 };
	Filter.TimeWindowSeconds = Windows[Index];
	if (TimeRangeText) TimeRangeText->SetText(FText::FromString(Index == 4 ? TEXT("间隔：全部") : FString::Printf(TEXT("间隔：%d 秒"), static_cast<int32>(Windows[Index]))));
	bUpdatingControls = true;
	if (TimeRangeSlider) TimeRangeSlider->SetValue(Index / 4.0f);
	bUpdatingControls = false;
	bDirty = true;
	RefreshDisplay();
}

void UCombatLogWidget::HandleUserScrolled(const float Offset)
{
	bFollowLatest = LogScrollBox && Offset >= LogScrollBox->GetScrollOffsetOfEnd() - 4.0f;
	if (FollowLatestCheck) FollowLatestCheck->SetIsChecked(bFollowLatest);
}

void UCombatLogWidget::HandleFollowChanged(const bool bChecked)
{
	if (bUpdatingControls) return;
	bFollowLatest = bChecked;
	if (bChecked && LogScrollBox) LogScrollBox->ScrollToEnd();
}
