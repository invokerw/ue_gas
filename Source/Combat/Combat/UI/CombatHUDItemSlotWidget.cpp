#include "Combat/UI/CombatHUDItemSlotWidget.h"
#include "Combat/UI/CombatHUDWidget.h"
#include "Combat/Items/CombatItemData.h"
#include "Combat/Core/CombatTags.h"
#include "Combat/Unit/CombatUnitCharacter.h"
#include "CombatPlayerController.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Engine/AssetManager.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Widgets/Text/STextBlock.h"

bool UCombatItemDragOperation::IsCurrent(const ACombatPlayerController* PC) const
{
	return PC && Unit.IsValid() && PC->GetCommandedUnit() == Unit.Get()
		&& PC->GetCommandBindingGeneration() == ControlGeneration && Unit->GetLifeGeneration() == Snapshot.LifeGeneration;
}
void UCombatItemDragOperation::Drop_Implementation(const FPointerEvent& Event)
{
	if (HUD.IsValid()) HUD->FinishItemDrag();
	Super::Drop_Implementation(Event);
}
void UCombatItemDragOperation::DragCancelled_Implementation(const FPointerEvent& Event)
{
	if (HUD.IsValid()) HUD->FinishItemDrag();
	Super::DragCancelled_Implementation(Event);
}
void UCombatHUDItemSlotWidget::BindInventorySlot(UCombatHUDWidget* InHUD, int32 InSlot)
{
	HUD = InHUD;
	SlotIndex = InSlot;
}
void UCombatHUDItemSlotWidget::ShowItem(const FCombatHUDOwnerView& Owner, const FCombatUnitView& Unit, double Now, const FText& Key)
{
	Snapshot = Owner;
	ClearEntry();
	if (HotkeyText) HotkeyText->SetText(Key);
	if (!Owner.Items.IsValidIndex(SlotIndex) || !Owner.Items[SlotIndex].Handle.IsValid()) return;
	const FCombatItemView& Item = Owner.Items[SlotIndex];
	const UCombatItemData* Data = Cast<UCombatItemData>(UAssetManager::Get().GetPrimaryAssetObject(Item.DefinitionId));
	const FText Name = Data ? Data->DisplayNameText : FText::FromName(Item.DefinitionId.PrimaryAssetName);
	SetIcon(Data ? Data->Icon.Get() : nullptr, Name);
	if (SymbolText && Data)
	{
		SymbolText->SetText(Data->Glyph.IsEmpty() ? FText::FromString(Name.ToString().Left(1)) : Data->Glyph);
		SymbolText->SetColorAndOpacity(Data->Tint);
	}
	const float Cooldown = Item.GetRemaining(Now);
	const float Wait = Remaining(Item.EnabledAt, Now);
	FString Reason;
	if (!CombatItems::IsEquipped(SlotIndex)) Reason = TEXT("背包");
	else if (Unit.LifeState != ECombatLifeState::Alive) Reason = TEXT("阵亡");
	else if (Wait > 0.0f) Reason = FString::Printf(TEXT("启用 %.1f"), Wait);
	else if (Unit.VisibleStatusTags.HasTagExact(CombatTags::State_Muted)) Reason = TEXT("禁用");
	else if (Unit.VisibleStatusTags.HasTagExact(CombatTags::State_Stunned) || Unit.VisibleStatusTags.HasTagExact(CombatTags::State_Hexed)
		|| Unit.VisibleStatusTags.HasTagExact(CombatTags::State_Frozen) || Unit.VisibleStatusTags.HasTagExact(CombatTags::State_OutOfGame)) Reason = TEXT("受控");
	else if (Item.AbilityHandle.IsValid() && Unit.Mana + KINDA_SMALL_NUMBER < Item.ManaCost) Reason = TEXT("缺蓝");
	else if (Data && Data->InitialCharges > 0 && Item.Charges < Data->ChargesPerUse) Reason = TEXT("能量耗尽");
	if (CountText) CountText->SetText(FText::FromString(!Reason.IsEmpty() ? Reason : Cooldown > 0.0f ? FString::Printf(TEXT("%.1f"), Cooldown) : FString()));
	if (CostText) CostText->SetText(FText::FromString(Item.ManaCost > 0 ? FString::Printf(TEXT("%.0f"), Item.ManaCost) : FString()));
	if (StackText) StackText->SetText(FText::FromString(Data && Data->InitialCharges > 0 ? FString::Printf(TEXT("%d"), Item.Charges)
		: Item.Quantity > 1 ? FString::Printf(TEXT("×%d"), Item.Quantity) : FString()));
	if (BlockedShade) BlockedShade->SetVisibility(Reason.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	if (CooldownShade)
	{
		CooldownShade->SetVisibility(Cooldown > 0.0f ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		CooldownShade->SetRenderScale(FVector2D(1, Item.CooldownDuration > 0 ? FMath::Clamp(Cooldown / Item.CooldownDuration, 0.0f, 1.0f) : 0));
	}
	DetailText = FText::FromString(FString::Printf(TEXT("%s%s\n%s\n%s\n数量 %d%s%s\n%s\n右键：操作菜单 · 拖拽：换槽或放到地面"),
		*Name.ToString(), Key.IsEmpty() ? TEXT("") : *FString::Printf(TEXT(" [%s]"), *Key.ToString()),
		Data ? *Data->Description.ToString() : TEXT("定义加载中"), Item.AbilityHandle.IsValid() ? TEXT("主动：左键或快捷键使用") : TEXT("被动：装备时生效"),
		Item.Quantity, Item.Charges > 0 ? *FString::Printf(TEXT(" · 充能 %d"), Item.Charges) : TEXT(""),
		Item.ManaCost > 0 ? *FString::Printf(TEXT(" · 法力 %.0f"), Item.ManaCost) : TEXT(""),
		!CombatItems::IsEquipped(SlotIndex) ? TEXT("背包中效果禁用，冷却以半速恢复；装备后等待 6 秒") : *Reason));
}

FReply UCombatHUDItemSlotWidget::NativeOnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event)
{
	if (Event.GetEffectingButton() == EKeys::RightMouseButton) { OpenItemMenu(); return FReply::Handled(); }
	if (Event.GetEffectingButton() != EKeys::LeftMouseButton) return FReply::Handled();
	ACombatPlayerController* PC = Cast<ACombatPlayerController>(GetOwningPlayer());
	if (!PC || !Snapshot.Items.IsValidIndex(SlotIndex) || !Snapshot.Items[SlotIndex].Handle.IsValid()) return FReply::Handled();
	PC->CancelCombatTargeting();
	PressSnapshot = Snapshot;
	PressUnit = PC->GetCommandedUnit();
	PressControl = PC->GetCommandBindingGeneration();
	bPressed = true;
	return FReply::Handled().DetectDrag(TakeWidget(), EKeys::LeftMouseButton);
}
FReply UCombatHUDItemSlotWidget::NativeOnMouseButtonUp(const FGeometry& Geometry, const FPointerEvent& Event)
{
	if (Event.GetEffectingButton() == EKeys::LeftMouseButton && bPressed)
	{
		bPressed = false;
		ACombatPlayerController* PC = Cast<ACombatPlayerController>(GetOwningPlayer());
		if (PC && PressUnit.IsValid() && PC->GetCommandedUnit() == PressUnit.Get() && PC->GetCommandBindingGeneration() == PressControl
			&& PressUnit->GetLifeGeneration() == PressSnapshot.LifeGeneration && Geometry.IsUnderLocation(Event.GetScreenSpacePosition()))
			PC->UseInventoryItem(SlotIndex, PressSnapshot.Items[SlotIndex]);
	}
	return FReply::Handled();
}
void UCombatHUDItemSlotWidget::NativeOnDragDetected(const FGeometry& Geometry, const FPointerEvent& Event, UDragDropOperation*& Operation)
{
	if (!bPressed || !HUD.IsValid()) return;
	bPressed = false;
	UCombatItemDragOperation* Drag = NewObject<UCombatItemDragOperation>();
	Drag->Snapshot = PressSnapshot;
	Drag->SourceSlot = SlotIndex;
	Drag->Unit = PressUnit;
	Drag->ControlGeneration = PressControl;
	Drag->HUD = HUD;
	if (UUserWidget* Visual = CreateWidget<UUserWidget>(GetOwningPlayer(), GetClass()))
	{
		// 拖影只复制文字和颜色，不绑定交互或库存。
		if (auto* ItemVisual = Cast<UCombatHUDItemSlotWidget>(Visual))
		{
			ItemVisual->BindInventorySlot(nullptr, SlotIndex);
			ItemVisual->ShowItem(PressSnapshot, FCombatUnitView(), 0, FText::GetEmpty());
		}
		Visual->SetRenderOpacity(0.75f);
		Drag->DefaultDragVisual = Visual;
	}
	HUD->BeginItemDrag();
	Operation = Drag;
}
bool UCombatHUDItemSlotWidget::NativeOnDrop(const FGeometry& Geometry, const FDragDropEvent& Event, UDragDropOperation* Operation)
{
	UCombatItemDragOperation* Drag = Cast<UCombatItemDragOperation>(Operation);
	if (!Drag) return false;
	ACombatPlayerController* PC = Cast<ACombatPlayerController>(GetOwningPlayer());
	if (Drag->IsCurrent(PC)) PC->SwapInventoryItems(Drag->SourceSlot, SlotIndex, Drag->Snapshot);
	if (Drag->HUD.IsValid()) Drag->HUD->FinishItemDrag();
	return true;
}
void UCombatHUDItemSlotWidget::OpenItemMenu()
{
	ACombatPlayerController* PC = Cast<ACombatPlayerController>(GetOwningPlayer());
	if (!PC || !Snapshot.Items.IsValidIndex(SlotIndex) || !Snapshot.Items[SlotIndex].Handle.IsValid()) return;
	PC->CancelCombatTargeting();
	const auto Frozen = Snapshot;
	const int32 From = SlotIndex;
	const int64 Generation = PC->GetCommandBindingGeneration();
	const TWeakObjectPtr<ACombatPlayerController> WeakPC = PC;
	const TWeakObjectPtr<ACombatUnitCharacter> Unit = PC->GetCommandedUnit();
	const auto Current = [WeakPC, Unit, Generation, Frozen]() { return WeakPC.IsValid() && Unit.IsValid()
		&& WeakPC->GetCommandedUnit() == Unit.Get() && WeakPC->GetCommandBindingGeneration() == Generation
		&& Unit->GetLifeGeneration() == Frozen.LifeGeneration; };
	FMenuBuilder Menu(true, nullptr);
	const UCombatItemData* Definition = Cast<UCombatItemData>(UAssetManager::Get().GetPrimaryAssetObject(Frozen.Items[From].DefinitionId));
	if (CombatItems::IsEquipped(From) && Frozen.Items[From].AbilityHandle.IsValid())
		Menu.AddMenuEntry(NSLOCTEXT("CombatItems", "Use", "使用"), FText::GetEmpty(), FSlateIcon(), FUIAction(FExecuteAction::CreateLambda([=]() { if (Current()) WeakPC->UseInventoryItem(From, Frozen.Items[From]); })));
	if (Definition && Definition->bCanDrop)
		Menu.AddMenuEntry(NSLOCTEXT("CombatItems", "Drop", "放到地面…"), FText::GetEmpty(), FSlateIcon(), FUIAction(FExecuteAction::CreateLambda([=]() { if (Current()) WeakPC->BeginDropInventoryItem(Frozen.Items[From]); })));
	const bool bEquipped = CombatItems::IsEquipped(From);
	for (int32 To = bEquipped ? 6 : 0; To < (bEquipped ? 9 : 6); ++To)
	{
		if (bEquipped && (!Definition || !Definition->bCanEnterBackpack)) break;
		if (!Frozen.Items.IsValidIndex(To) || Frozen.Items[To].Handle.IsValid()) continue;
		Menu.AddMenuEntry(FText::FromString(bEquipped ? TEXT("移入背包") : TEXT("移入装备栏")), FText::GetEmpty(), FSlateIcon(), FUIAction(FExecuteAction::CreateLambda([=]() { if (Current()) WeakPC->SwapInventoryItems(From, To, Frozen); })));
		break;
	}
	FSlateApplication::Get().PushMenu(TakeWidget(), FWidgetPath(), Menu.MakeWidget(), FSlateApplication::Get().GetCursorPos(), FPopupTransitionEffect(FPopupTransitionEffect::ContextMenu));
}
