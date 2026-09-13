#include "Combat/UI/CombatHUDWidget.h"

#include "Combat/UI/CombatHUDSlotWidget.h"
#include "Combat/UI/CombatRadialProgress.h"
#include "Combat/View/CombatUnitViewComponent.h"
#include "Combat/Unit/CombatProgressionComponent.h"
#include "Combat/Unit/CombatUnitCharacter.h"
#include "Combat/Data/CombatDefinitionData.h"
#include "CombatPlayerController.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/PanelWidget.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Input/Reply.h"
#include "InputCoreTypes.h"
#include "Framework/Application/SlateApplication.h"
#include "Combat/UI/CombatPlayerHUD.h"
#include "Combat/UI/CombatLogWidget.h"

namespace CombatHUD
{
	/** 过滤异常数值，防止资源条除零或把 NaN 送入 Slate。 */
	float Safe(float Value) { return FMath::IsFinite(Value) ? Value : 0.0f; }
	/** 更新可选文本控件，Blueprint 可自由省略未使用的显示部分。 */
	void Text(UTextBlock* Label, const FString& Value) { if (Label) Label->SetText(FText::FromString(Value)); }
	/** 显示服务器实际资源；无最大值时显示空进度。 */
	void Resource(UProgressBar* Bar, UTextBlock* Label, float Value, float Maximum)
	{
		Maximum = FMath::Max(0.0f, Safe(Maximum));
		Value = FMath::Clamp(Safe(Value), 0.0f, Maximum);
		if (Bar) Bar->SetPercent(Maximum > 0.0f ? Value / Maximum : 0.0f);
		Text(Label, FString::Printf(TEXT("%.0f / %.0f"), Value, Maximum));
	}
}

TArray<UCombatHUDSlotWidget*> UCombatHUDWidget::GetSkillWidgets() const
{
	return { SkillQ, SkillW, SkillE, SkillR };
}

void UCombatHUDWidget::NativeConstruct()
{
	Super::NativeConstruct();
	bConstructed = true;
	SetIsFocusable(true);
	if (CloseDetailButton) CloseDetailButton->OnClicked.AddUniqueDynamic(this, &UCombatHUDWidget::CloseDetail);
	for (UCombatHUDSlotWidget* Entry : GetSkillWidgets())
	{
		if (!Entry) continue;
		Entry->OnDetailRequested.RemoveAll(this);
		Entry->OnDetailRequested.AddUObject(this, &UCombatHUDWidget::HandleDetail, Entry);
		Entry->OnUpgradeRequested.RemoveAll(this);
		Entry->OnUpgradeRequested.AddUObject(this, &UCombatHUDWidget::HandleUpgradeRequested);
	}
	CombatHUD::Text(LevelText, TEXT("—"));
	CombatHUD::Text(ExperienceText, TEXT(""));
	CombatHUD::Text(AbilityPointsText, TEXT(""));
	if (ExperienceRing)
	{
		ExperienceRing->SetProgress(0.0f);
		ExperienceRing->SetToolTipText(NSLOCTEXT("CombatHUD", "GrowthSyncing", "等级与经验同步中"));
	}
	if (LevelText) LevelText->SetToolTipText(NSLOCTEXT("CombatHUD", "GrowthSyncing", "等级与经验同步中"));
	CloseDetail();
	ACombatUnitCharacter* PreviousUnit = BoundUnit.Get();
	UnbindView();
	BoundUnit.Reset();
	InitializeForUnit(PreviousUnit);
	if (!PreviousUnit && HUDPanel) HUDPanel->SetVisibility(ESlateVisibility::Hidden);
}

void UCombatHUDWidget::InitializeForUnit(ACombatUnitCharacter* Unit)
{
	if (IsDesignTime()) return;
	if (!IsValid(Unit) || Unit == EndedUnit.Get() || Unit->IsActorBeingDestroyed()) Unit = nullptr;
	if (BoundUnit.Get() == Unit && (Unit ? BoundView.IsValid() : !BoundView.IsValid() && DisplayLifeGeneration == 0)) return;
	UnbindView();
	ResetPresentation();
	BoundUnit = Unit;
	BoundView = Unit ? Unit->GetCombatUnitViewComponent() : nullptr;
	if (Unit)
	{
		Unit->OnEndPlay.AddUniqueDynamic(this, &UCombatHUDWidget::HandleUnitEndPlay);
		if (BoundView.IsValid())
		{
			BoundView->OnUnitViewChanged.AddUniqueDynamic(this, &UCombatHUDWidget::RefreshDisplay);
			BoundView->OnModifierViewsChanged.AddUniqueDynamic(this, &UCombatHUDWidget::RefreshDisplay);
			BoundView->OnHUDOwnerViewChanged.AddUniqueDynamic(this, &UCombatHUDWidget::RefreshDisplay);
		}
	}
	RefreshDisplay();
}

void UCombatHUDWidget::UnbindView()
{
	++BindingRevision;
	if (DefinitionLoad) { DefinitionLoad->CancelHandle(); DefinitionLoad.Reset(); }
	RequestedDefinitions.Reset();
	if (BoundView.IsValid())
	{
		BoundView->OnUnitViewChanged.RemoveAll(this);
		BoundView->OnModifierViewsChanged.RemoveAll(this);
		BoundView->OnHUDOwnerViewChanged.RemoveAll(this);
	}
	if (BoundUnit.IsValid()) BoundUnit->OnEndPlay.RemoveDynamic(this, &UCombatHUDWidget::HandleUnitEndPlay);
	BoundView.Reset();
}

void UCombatHUDWidget::ResetPresentation()
{
	CloseDetail();
	for (UCombatHUDSlotWidget* Entry : BuffWidgets) if (Entry) Entry->OnDetailRequested.RemoveAll(this);
	BuffWidgets.Reset();
	ModifierIdentities.Reset();
	if (BuffPanel) BuffPanel->ClearChildren();
	for (UCombatHUDSlotWidget* Entry : GetSkillWidgets()) if (Entry) Entry->ClearEntry();
	DisplaySnapshot = FCombatHUDOwnerView();
	DisplayLifeGeneration = 0;
	if (HUDPanel) HUDPanel->SetVisibility(ESlateVisibility::Hidden);
}

void UCombatHUDWidget::HandleUnitEndPlay(AActor* Actor, EEndPlayReason::Type Reason)
{
	(void)Reason;
	if (Actor != BoundUnit.Get()) return;
	EndedUnit = BoundUnit;
	InitializeForUnit(nullptr);
}

void UCombatHUDWidget::NativeDestruct()
{
	if (const ACombatPlayerController* PC = Cast<ACombatPlayerController>(GetOwningPlayer())) PC->GetAbilityAimComponent()->SetHoveredSlot(INDEX_NONE);
	bConstructed = false;
	UnbindView();
	ResetPresentation();
	for (UCombatHUDSlotWidget* Entry : GetSkillWidgets())
	{
		if (!Entry) continue;
		Entry->OnDetailRequested.RemoveAll(this);
		Entry->OnUpgradeRequested.RemoveAll(this);
	}
	if (CloseDetailButton) CloseDetailButton->OnClicked.RemoveDynamic(this, &UCombatHUDWidget::CloseDetail);
	Super::NativeDestruct();
}

void UCombatHUDWidget::NativeTick(const FGeometry& Geometry, float DeltaTime)
{
	Super::NativeTick(Geometry, DeltaTime);
	const ACombatPlayerController* Controller = Cast<ACombatPlayerController>(GetOwningPlayer());
	if (Controller && Controller->IsLocalController()) InitializeForUnit(Controller->GetCommandedUnit());
	if (Controller && FSlateApplication::IsInitialized())
	{
		const FVector2D PointerPosition = FSlateApplication::Get().GetCursorPos();
		const ACombatPlayerHUD* HUD = Cast<ACombatPlayerHUD>(Controller->GetHUD());
		const bool bOverLog = HUD && HUD->GetLogWidget() && HUD->GetLogWidget()->IsScreenPositionOverUI(PointerPosition);
		Controller->GetAbilityAimComponent()->SetHoveredSlot(bOverLog ? INDEX_NONE : GetHoveredAbilitySlot(PointerPosition));
	}
	RefreshAccumulator += DeltaTime;
	if (RefreshAccumulator >= 0.05f)
	{
		RefreshAccumulator = 0.0f;
		// 同时覆盖 Spec 与 HUD 快照的跨流到达；这里只推进 UI，服务器状态仍来自 View。
		RefreshDisplay();
	}
}

FText UCombatHUDWidget::ResolveName(const FPrimaryAssetId& Id)
{
	const UCombatDefinitionData* Data = Cast<UCombatDefinitionData>(UAssetManager::Get().GetPrimaryAssetObject(Id));
	if (Data && !Data->DisplayNameText.IsEmpty()) return Data->DisplayNameText;
	return Id.IsValid() ? FText::FromName(Id.PrimaryAssetName) : FText::GetEmpty();
}

UTexture2D* UCombatHUDWidget::FindIcon(const FPrimaryAssetId& Id) const
{
	const TObjectPtr<UTexture2D>* Texture = DefinitionIcons.Find(Id);
	return Texture ? Texture->Get() : nullptr;
}

void UCombatHUDWidget::RequestDefinitions(const TArray<FPrimaryAssetId>& Ids)
{
	TArray<FPrimaryAssetId> Desired = Ids;
	Desired.Sort([](const FPrimaryAssetId& A, const FPrimaryAssetId& B) { return A.ToString() < B.ToString(); });
	if (Desired == RequestedDefinitions) return;
	RequestedDefinitions = MoveTemp(Desired);
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
	const int64 Generation = DisplayLifeGeneration;
	DefinitionLoad = UAssetManager::GetStreamableManager().RequestAsyncLoad(Paths,
		FStreamableDelegate::CreateWeakLambda(this, [this, Revision, Generation]()
		{
			if (bConstructed && Revision == BindingRevision && Generation == DisplayLifeGeneration) RefreshDisplay();
		}));
}

void UCombatHUDWidget::RefreshDisplay()
{
	if (!bConstructed || !BoundView.IsValid()) return;
	const FCombatUnitView& Unit = BoundView->GetUnitView();
	if (Unit.LifeGeneration <= 0 || !Unit.UnitDefinitionId.IsValid())
	{
		ResetPresentation();
		return;
	}
	if (DisplayLifeGeneration != Unit.LifeGeneration)
	{
		++BindingRevision;
		if (DefinitionLoad) { DefinitionLoad->CancelHandle(); DefinitionLoad.Reset(); }
		RequestedDefinitions.Reset();
		ResetPresentation();
		DisplayLifeGeneration = Unit.LifeGeneration;
	}
	if (HUDPanel) HUDPanel->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	const FCombatHUDOwnerView Candidate = BoundView->GetHUDOwnerView();
	const bool bOwnerReady = Candidate.LifeGeneration == Unit.LifeGeneration && Candidate.UnitDefinitionId == Unit.UnitDefinitionId;
	DisplaySnapshot = bOwnerReady ? Candidate : FCombatHUDOwnerView();
	if (bOwnerReady)
	{
		CombatHUD::Text(LevelText, FString::Printf(TEXT("%d"), DisplaySnapshot.Level));
		CombatHUD::Text(ExperienceText, DisplaySnapshot.ExperienceToNextLevel > 0
			? FString::Printf(TEXT("%lld / %lld"), static_cast<long long>(DisplaySnapshot.ExperienceIntoLevel),
				static_cast<long long>(DisplaySnapshot.ExperienceIntoLevel + DisplaySnapshot.ExperienceToNextLevel))
			: TEXT("满级"));
		CombatHUD::Text(AbilityPointsText, DisplaySnapshot.UnspentAbilityPoints > 0
			? FString::Printf(TEXT("技能点 %d"), DisplaySnapshot.UnspentAbilityPoints) : TEXT(""));
		if (ExperienceRing)
		{
			ExperienceRing->SetProgress(DisplaySnapshot.ExperienceProgress);
			ExperienceRing->SetToolTipText(FText::FromString(DisplaySnapshot.ExperienceToNextLevel > 0
				? FString::Printf(TEXT("经验 %lld / %lld · 距离升级还需 %lld"),
					static_cast<long long>(DisplaySnapshot.ExperienceIntoLevel),
					static_cast<long long>(DisplaySnapshot.ExperienceIntoLevel + DisplaySnapshot.ExperienceToNextLevel),
					static_cast<long long>(DisplaySnapshot.ExperienceToNextLevel))
				: TEXT("已达到最高等级")));
		}
	}
	else
	{
		CombatHUD::Text(LevelText, TEXT("—"));
		CombatHUD::Text(ExperienceText, TEXT(""));
		CombatHUD::Text(AbilityPointsText, TEXT(""));
		if (ExperienceRing)
		{
			ExperienceRing->SetProgress(0.0f);
			ExperienceRing->SetToolTipText(NSLOCTEXT("CombatHUD", "GrowthSyncing", "等级与经验同步中"));
		}
		if (LevelText) LevelText->SetToolTipText(NSLOCTEXT("CombatHUD", "GrowthSyncing", "等级与经验同步中"));
	}
	TArray<FPrimaryAssetId> Ids = { Unit.UnitDefinitionId };
	for (const FCombatHUDAbilityView& Ability : DisplaySnapshot.Abilities) if (Ability.DefinitionId.IsValid()) Ids.AddUnique(Ability.DefinitionId);
	TArray<FCombatModifierView> Modifiers = BoundView->GetVisibleModifiers();
	Modifiers.StableSort([](const FCombatModifierView& A, const FCombatModifierView& B) { return !A.bIsDebuff && B.bIsDebuff; });
	for (const FCombatModifierView& Modifier : Modifiers) Ids.AddUnique(Modifier.DefinitionId);
	RequestDefinitions(Ids);
	const FText HeroName = ResolveName(Unit.UnitDefinitionId);
	if (HeroNameText) HeroNameText->SetText(HeroName);
	UTexture2D* Portrait = FindIcon(Unit.UnitDefinitionId);
	if (HeroPortrait)
	{
		HeroPortrait->SetBrushFromTexture(Portrait);
		HeroPortrait->SetVisibility(Portrait ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		HeroPortrait->SetRenderOpacity(Unit.LifeState == ECombatLifeState::Alive ? 1.0f : 0.4f);
	}
	if (HeroSymbol)
	{
		HeroSymbol->SetText(FText::FromString(HeroName.ToString().Left(1)));
		HeroSymbol->SetVisibility(Portrait ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
	CombatHUD::Resource(HealthBar, HealthText, Unit.Health, Unit.MaxHealth);
	CombatHUD::Resource(ManaBar, ManaText, Unit.Mana, Unit.MaxMana);
	const bool bAlive = Unit.LifeState == ECombatLifeState::Alive;
	CombatHUD::Text(HealthRegenText, bAlive && bOwnerReady ? FString::Printf(TEXT("+%.1f/s"), DisplaySnapshot.HealthRegen) : TEXT("—"));
	CombatHUD::Text(ManaRegenText, bAlive && bOwnerReady ? FString::Printf(TEXT("+%.1f/s"), DisplaySnapshot.ManaRegen) : TEXT("—"));
	CombatHUD::Text(StatsText, bOwnerReady ? FString::Printf(TEXT("攻  %.0f\n甲  %.0f\n抗  %.0f%%\n速  %.0f"),
		DisplaySnapshot.AttackDamage, DisplaySnapshot.Armor, DisplaySnapshot.MagicResist * 100.0f, DisplaySnapshot.MoveSpeed) : TEXT("攻  —\n甲  —\n抗  —\n速  —"));
	const double Now = BoundView->GetEstimatedServerTimeSeconds();
	const TArray<UCombatHUDSlotWidget*> Slots = GetSkillWidgets();
	const TCHAR* Keys[] = { TEXT("Q"), TEXT("W"), TEXT("E"), TEXT("R") };
	for (int32 Index = 0; Index < Slots.Num(); ++Index)
	{
		if (!Slots[Index]) continue;
		const FCombatHUDAbilityView Ability = DisplaySnapshot.Abilities.IsValidIndex(Index)
			? DisplaySnapshot.Abilities[Index] : FCombatHUDAbilityView();
		FText Description;
		if (const UCombatAbilityData* Data = Cast<UCombatAbilityData>(UAssetManager::Get().GetPrimaryAssetObject(Ability.DefinitionId)))
		{
			Description = FText::FromString(FString::Printf(TEXT("施法前摇 %.2f 秒%s"), Data->CastPoint,
				Data->ChannelDuration > 0 ? *FString::Printf(TEXT(" · 引导 %.1f 秒"), Data->ChannelDuration) : TEXT("")));
		}
		Slots[Index]->ShowAbility(Ability, Unit, Now, ResolveName(Ability.DefinitionId), Description, FindIcon(Ability.DefinitionId), FText::FromString(Keys[Index]));
	}
	TArray<FCombatModifierHandle> Handles;
	for (int32 Index = 0; Index < FMath::Min(Modifiers.Num(), 10); ++Index) Handles.Add(Modifiers[Index].Handle);
	if (Handles != ModifierIdentities)
	{
		CloseDetail();
		for (UCombatHUDSlotWidget* Entry : BuffWidgets) if (Entry) Entry->OnDetailRequested.RemoveAll(this);
		BuffWidgets.Reset();
		if (BuffPanel) BuffPanel->ClearChildren();
		ModifierIdentities = Handles;
		if (BuffPanel && BuffWidgetClass)
		{
			for (int32 Index = 0; Index < Handles.Num(); ++Index)
			{
				UCombatHUDSlotWidget* Entry = CreateWidget<UCombatHUDSlotWidget>(GetOwningPlayer(), BuffWidgetClass);
				if (!Entry) break;
				BuffPanel->AddChild(Entry);
				Entry->OnDetailRequested.AddUObject(this, &UCombatHUDWidget::HandleDetail, Entry);
				BuffWidgets.Add(Entry);
			}
		}
	}
	for (int32 Index = 0; Index < BuffWidgets.Num(); ++Index)
	{
		BuffWidgets[Index]->ShowModifier(Modifiers[Index], Now, ResolveName(Modifiers[Index].DefinitionId), FindIcon(Modifiers[Index].DefinitionId));
	}
	CombatHUD::Text(BuffOverflowText, Modifiers.Num() > 10 ? FString::Printf(TEXT("+%d"), Modifiers.Num() - 10) : TEXT(""));
	if (BuffOverflowText)
	{
		FString Overflow;
		for (int32 Index = 10; Index < Modifiers.Num(); ++Index) Overflow += ResolveName(Modifiers[Index].DefinitionId).ToString() + TEXT("\n");
		BuffOverflowText->SetToolTipText(FText::FromString(Overflow));
	}
	FString Activity;
	if (const ACombatPlayerController* PC = Cast<ACombatPlayerController>(GetOwningPlayer())) Activity = PC->GetAbilityAimComponent()->GetStatusText().ToString();
	if (!bAlive) Activity = TEXT("已阵亡");
	else if (Unit.AbilityPhase != ECombatAbilityViewPhase::None && Unit.ActiveAbilityDefinitionId.IsValid())
	{
		Activity = FString::Printf(TEXT("%s · %s %.1fs"), *ResolveName(Unit.ActiveAbilityDefinitionId).ToString(),
			Unit.AbilityPhase == ECombatAbilityViewPhase::Channeling ? TEXT("引导中") : TEXT("施法中"),
			UCombatHUDSlotWidget::Remaining(Unit.AbilityServerEndTime, Now));
	}
	CombatHUD::Text(ActivityText, Activity);
	if (DetailText && DetailPanel && DetailPanel->GetVisibility() == ESlateVisibility::Visible)
		DetailText->SetText(DetailSource.IsValid() ? DetailSource->GetDetailText() : BuildHeroDetail());
}

void UCombatHUDWidget::HandleUpgradeRequested(UCombatHUDSlotWidget* Source)
{
	if (!Source || !BoundUnit.IsValid() || !DisplaySnapshot.LifeGeneration)
	{
		return;
	}
	const TArray<UCombatHUDSlotWidget*> Slots = GetSkillWidgets();
	const int32 Index = Slots.IndexOfByKey(Source);
	if (!DisplaySnapshot.Abilities.IsValidIndex(Index))
	{
		return;
	}
	if (UCombatProgressionComponent* Progression = BoundUnit->GetCombatProgressionComponent())
	{
		Progression->RequestAbilityUpgrade(DisplaySnapshot.Abilities[Index].SpecHandle);
	}
}

FText UCombatHUDWidget::BuildHeroDetail() const
{
	if (!BoundView.IsValid()) return FText::GetEmpty();
	const FString Name = ResolveName(BoundView->GetUnitView().UnitDefinitionId).ToString();
	if (DisplaySnapshot.LifeGeneration == 0) return FText::FromString(Name + TEXT("\n属性同步中"));
	return FText::FromString(FString::Printf(TEXT("%s\n等级 %d · 经验 %lld\n未使用技能点 %d\n攻击力 %.0f · 护甲 %.1f\n魔法抗性 %.0f%% · 移动速度 %.0f\n生命恢复 %.1f / 秒\n法力恢复 %.1f / 秒"),
		*Name, DisplaySnapshot.Level, static_cast<long long>(DisplaySnapshot.Experience), DisplaySnapshot.UnspentAbilityPoints,
		DisplaySnapshot.AttackDamage, DisplaySnapshot.Armor, DisplaySnapshot.MagicResist * 100.0f,
		DisplaySnapshot.MoveSpeed, DisplaySnapshot.HealthRegen, DisplaySnapshot.ManaRegen));
}

void UCombatHUDWidget::HandleDetail(const FText& Text, bool bPin, UCombatHUDSlotWidget* Source)
{
	if (bDetailPinned && !bPin) return;
	if (Text.IsEmpty() || (bPin && bDetailPinned && DetailSource.Get() == Source)) { CloseDetail(); return; }
	bDetailPinned = bPin;
	DetailSource = Source;
	if (DetailText) DetailText->SetText(Text);
	if (DetailPanel) DetailPanel->SetVisibility(ESlateVisibility::Visible);
	if (bPin) SetKeyboardFocus();
}

void UCombatHUDWidget::CloseDetail()
{
	bDetailPinned = false;
	bHeroHovered = false;
	DetailSource.Reset();
	if (DetailPanel) DetailPanel->SetVisibility(ESlateVisibility::Collapsed);
}

FReply UCombatHUDWidget::NativeOnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event)
{
	if (Event.GetKey() == EKeys::Escape)
	{
		if (ACombatPlayerController* PC = Cast<ACombatPlayerController>(GetOwningPlayer())) PC->CancelCombatTargeting();
		if (bDetailPinned) CloseDetail();
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(Geometry, Event);
}

FReply UCombatHUDWidget::NativeOnPreviewMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event)
{
	if (Event.GetEffectingButton() == EKeys::RightMouseButton && IsScreenPositionOverUI(Event.GetScreenSpacePosition()))
	{
		if (ACombatPlayerController* PC = Cast<ACombatPlayerController>(GetOwningPlayer())) PC->CancelCombatTargeting();
		return FReply::Handled();
	}
	return Super::NativeOnPreviewMouseButtonDown(Geometry, Event);
}

FReply UCombatHUDWidget::NativeOnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event)
{
	if (StatsText && StatsText->GetCachedGeometry().IsUnderLocation(Event.GetScreenSpacePosition()) && Event.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		HandleDetail(BuildHeroDetail(), true, nullptr);
		return FReply::Handled();
	}
	if (IsScreenPositionOverUI(Event.GetScreenSpacePosition())) return FReply::Handled();
	return Super::NativeOnMouseButtonDown(Geometry, Event);
}

bool UCombatHUDWidget::IsScreenPositionOverUI(const FVector2D Position) const
{
	if (!IsVisible() || (HUDPanel && !HUDPanel->IsVisible())) return false;
	if (HUDFrame && HUDFrame->IsVisible() && HUDFrame->GetCachedGeometry().IsUnderLocation(Position)) return true;
	if (DetailPanel && DetailPanel->IsVisible() && DetailPanel->GetCachedGeometry().IsUnderLocation(Position)) return true;
	for (UCombatHUDSlotWidget* Entry : GetSkillWidgets())
	{
		if (!Entry || !Entry->IsVisible()) continue;
		if (Entry->GetCachedGeometry().IsUnderLocation(Position)) return true;
		const UButton* Upgrade = Entry->GetEffectiveUpgradeButton();
		if (Upgrade && Upgrade->IsVisible() && Upgrade->GetCachedGeometry().IsUnderLocation(Position)) return true;
	}
	return false;
}

int32 UCombatHUDWidget::GetHoveredAbilitySlot(const FVector2D Position) const
{
	if (!IsVisible() || (HUDPanel && !HUDPanel->IsVisible())) return INDEX_NONE;
	const TArray<UCombatHUDSlotWidget*> Slots = GetSkillWidgets();
	for (int32 Index = 0; Index < Slots.Num(); ++Index)
	{
		if (Slots[Index] && Slots[Index]->IsVisible() && Slots[Index]->GetCachedGeometry().IsUnderLocation(Position)) return Index;
	}
	return INDEX_NONE;
}

FReply UCombatHUDWidget::NativeOnMouseMove(const FGeometry& Geometry, const FPointerEvent& Event)
{
	const bool bOverHero = StatsText && StatsText->GetCachedGeometry().IsUnderLocation(Event.GetScreenSpacePosition());
	if (bOverHero && !bHeroHovered) HandleDetail(BuildHeroDetail(), false, nullptr);
	else if (!bOverHero && bHeroHovered && !bDetailPinned) CloseDetail();
	bHeroHovered = bOverHero;
	return Super::NativeOnMouseMove(Geometry, Event);
}

void UCombatHUDWidget::NativeOnMouseLeave(const FPointerEvent& Event)
{
	if (bHeroHovered && !bDetailPinned) CloseDetail();
	bHeroHovered = false;
	Super::NativeOnMouseLeave(Event);
}
