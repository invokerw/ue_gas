#include "CombatPlayerController.h"
#include "Combat/Ability/CombatAbilitySystemComponent.h"
#include "Combat/Core/CombatTags.h"
#include "Combat/Data/CombatDefinitionData.h"
#include "Combat/Items/CombatItemTypes.h"
#include "Combat/UI/CombatAbilityIndicatorGround.h"
#include "Combat/Unit/CombatUnitCharacter.h"
#include "Combat/View/CombatUnitViewComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "InputAction.h"
#include "Blueprint/SlateBlueprintLibrary.h"
#include "Combat/UI/CombatPlayerHUD.h"
#include "Combat/UI/CombatHUDWidget.h"
#include "Combat/UI/CombatLogWidget.h"
#include "Combat/UI/CombatShopWidget.h"

bool ACombatPlayerController::UseInventoryItem(int32 Slot, const FCombatItemView& Expected)
{
	CancelCombatTargeting();
	ACombatUnitCharacter* Unit = GetReadyCommandedUnit();
	if (!Unit || !IsLocalController() || !CombatItems::IsEquipped(Slot) || !Expected.Handle.IsValid()) return false;
	const auto View = Unit->GetCombatUnitViewComponent()->GetHUDOwnerView();
	if (!View.Items.IsValidIndex(Slot) || View.Items[Slot].Handle != Expected.Handle || View.Items[Slot].Revision != Expected.Revision
		|| View.LifeGeneration != Unit->GetLifeGeneration()) return false;
	const UCombatAbilityData* Data = Unit->GetCombatAbilitySystemComponent()->GetCombatAbilityData(Expected.AbilityHandle);
	if (!Data) return false;
	if (Data->BehaviorTags.HasTagExact(CombatTags::Ability_Behavior_NoTarget))
	{
		FCombatOrderRequest Order;
		Order.Type = ECombatOrderType::CastNoTarget;
		Order.AbilitySpecHandle = Expected.AbilityHandle;
		Order.ItemHandle = Expected.Handle;
		Order.ItemRevision = Expected.Revision;
		return SubmitCombatOrder(Order);
	}
	if (!AbilityAimComponent->BeginAim(4 + Slot)) return false;
	ItemPressSerials[Slot] = AbilityAimComponent->GetSessionSerial();
	FHitResult Hit;
	AbilityAimComponent->TraceAimHit(Hit);
	AbilityAimComponent->UpdatePreview(Hit, !IsPointerOverCombatUI());
	// 鼠标点击物品始终进入标准瞄准；键盘 Started 再按配置决定快施。
	return true;
}

bool ACombatPlayerController::SwapInventoryItems(int32 From, int32 To, const FCombatHUDOwnerView& Expected)
{
	ACombatUnitCharacter* Unit = GetReadyCommandedUnit();
	if (!Unit || !IsLocalController() || Expected.LifeGeneration != Unit->GetLifeGeneration()
		|| !Expected.Items.IsValidIndex(From) || !Expected.Items.IsValidIndex(To)) return false;
	FCombatOrderRequest Order;
	Order.Type = ECombatOrderType::SwapItems;
	Order.ItemHandle = Expected.Items[From].Handle;
	Order.ItemRevision = Expected.Items[From].Revision;
	Order.OtherItemHandle = Expected.Items[To].Handle;
	Order.FromItemSlot = From;
	Order.ToItemSlot = To;
	Order.InventoryRevision = Expected.InventoryRevision;
	return SubmitCombatOrder(Order);
}

bool ACombatPlayerController::DropInventoryItemAtScreenPosition(const FCombatItemView& Expected, const FVector2D& ScreenPosition)
{
	if (!GetReadyCommandedUnit() || !IsLocalController() || !Expected.Handle.IsValid()) return false;
	const ACombatPlayerHUD* HUD = Cast<ACombatPlayerHUD>(GetHUD());
	if (HUD && ((HUD->GetCombatWidget() && HUD->GetCombatWidget()->IsScreenPositionOverUI(ScreenPosition))
		|| (HUD->GetLogWidget() && HUD->GetLogWidget()->IsScreenPositionOverUI(ScreenPosition))
		|| (HUD->GetShopWidget() && HUD->GetShopWidget()->IsScreenPositionOverUI(ScreenPosition)))) return false;
	FVector2D Pixel, Logical;
	USlateBlueprintLibrary::AbsoluteToViewport(this, ScreenPosition, Pixel, Logical);
	int32 Width = 0, Height = 0;
	GetViewportSize(Width, Height);
	if (Pixel.X < 0 || Pixel.Y < 0 || Pixel.X >= Width || Pixel.Y >= Height) return false;
	FVector Origin, Direction;
	if (!DeprojectScreenPositionToWorld(Pixel.X, Pixel.Y, Origin, Direction)) return false;
	FHitResult Hit;
	const FCollisionQueryParams Params(SCENE_QUERY_STAT(CombatItemDrop), true, GetReadyCommandedUnit());
	if (!GetWorld()->LineTraceSingleByChannel(Hit, Origin, Origin + Direction * 1000000.0, CombatAbilityIndicatorGround::TraceChannel, Params)
		|| !CombatAbilityIndicatorGround::IsGroundHit(Hit)) return false;
	FCombatOrderRequest Order;
	Order.Type = ECombatOrderType::DropItem;
	Order.ItemHandle = Expected.Handle;
	Order.ItemRevision = Expected.Revision;
	Order.TargetLocation = Hit.Location;
	Order.bHasTargetLocation = true;
	return SubmitCombatOrder(Order);
}

void ACombatPlayerController::OnItemSlotPressed(int32 Slot)
{
	ACombatUnitCharacter* Unit = GetReadyCommandedUnit();
	if (!Unit || !CombatItems::IsEquipped(Slot)) return;
	const auto View = Unit->GetCombatUnitViewComponent()->GetHUDOwnerView();
	if (!View.Items.IsValidIndex(Slot) || !UseInventoryItem(Slot, View.Items[Slot])) return;
	if (AbilityCastMode == ECombatAbilityCastMode::QuickPress && AbilityAimComponent->IsAiming())
	{
		FHitResult Hit;
		AbilityAimComponent->TraceAimHit(Hit);
		ConfirmAbilityTarget(Hit, ItemPressSerials[Slot]);
		if (AbilityAimComponent->IsAiming()) AbilityAimComponent->FinishQuickCastAttempt();
	}
}

void ACombatPlayerController::OnItemSlotReleased(int32 Slot)
{
	if (!CombatItems::IsEquipped(Slot)) return;
	const uint64 Serial = ItemPressSerials[Slot];
	ItemPressSerials[Slot] = 0;
	if (!Serial || AbilityCastMode != ECombatAbilityCastMode::QuickRelease || !AbilityAimComponent->IsAiming()
		|| AbilityAimComponent->GetActiveSlot() != 4 + Slot || AbilityAimComponent->GetSessionSerial() != Serial) return;
	FHitResult Hit;
	AbilityAimComponent->TraceAimHit(Hit);
	ConfirmAbilityTarget(Hit, Serial);
	if (AbilityAimComponent->IsAiming()) AbilityAimComponent->FinishQuickCastAttempt();
}

void ACombatPlayerController::OnItemInputCanceled(int32 Slot)
{
	if (!CombatItems::IsEquipped(Slot)) return;
	if (ItemPressSerials[Slot] && AbilityAimComponent->GetSessionSerial() == ItemPressSerials[Slot]) AbilityAimComponent->CancelAim();
	ItemPressSerials[Slot] = 0;
}

FText ACombatPlayerController::GetItemHotkeyText(int32 Slot) const
{
	if (!ItemSlotActions.IsValidIndex(Slot) || !ItemSlotActions[Slot]) return FText::GetEmpty();
	const UEnhancedInputLocalPlayerSubsystem* Input = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer());
	const TArray<FKey> Keys = Input ? Input->QueryKeysMappedToAction(ItemSlotActions[Slot]) : TArray<FKey>();
	return Keys.IsEmpty() ? FText::GetEmpty() : Keys[0].GetDisplayName();
}

namespace CombatItemFeedback
{
	/** 稳定失败标签翻译为玩家可执行的提示。 */
	FText Failure(FGameplayTag Tag)
	{
		if (Tag == CombatTags::Failure_Item_Full) return FText::FromString(TEXT("物品栏已满"));
		if (Tag == CombatTags::Failure_Item_Stale) return FText::FromString(TEXT("物品已变化或被其他单位拾取"));
		if (Tag == CombatTags::Failure_Item_NotEquipped) return FText::FromString(TEXT("物品需要放入装备栏"));
		if (Tag == CombatTags::Failure_Item_Muted) return FText::FromString(TEXT("物品暂时禁用"));
		if (Tag == CombatTags::Failure_Item_Busy) return FText::FromString(TEXT("物品正在使用，暂时无法移出装备"));
		if (Tag == CombatTags::Failure_Item_Empty) return FText::FromString(TEXT("物品数量或充能不足"));
		if (Tag == CombatTags::Failure_Item_Bound) return FText::FromString(TEXT("无法拾取他人绑定的物品"));
		if (Tag == CombatTags::Order_Failure_Cancelled) return FText::FromString(TEXT("物品操作已取消"));
		return FText::FromString(FString::Printf(TEXT("物品操作失败：%s"), *Tag.ToString()));
	}
}
void ACombatPlayerController::TrackItemRequest(int32 RequestId, const FCombatOrderRequest& Order)
{
	if (ItemFeedbackUnit.IsValid())
	{
		ItemFeedbackUnit->OnOrderBatchResult.RemoveDynamic(this, &ACombatPlayerController::HandleItemBatchResult);
		ItemFeedbackUnit->OnOrderFinalResult.RemoveDynamic(this, &ACombatPlayerController::HandleItemFinalResult);
	}
	ItemFeedbackUnit = GetReadyCommandedUnit();
	if (!ItemFeedbackUnit.IsValid()) return;
	ItemFeedbackUnit->OnOrderBatchResult.AddUniqueDynamic(this, &ACombatPlayerController::HandleItemBatchResult);
	ItemFeedbackUnit->OnOrderFinalResult.AddUniqueDynamic(this, &ACombatPlayerController::HandleItemFinalResult);
	ItemFeedbackRequest = RequestId;
	ItemFeedbackLife = ItemFeedbackUnit->GetLifeGeneration();
	ItemFeedbackBinding = GetCommandBindingGeneration();
	FeedbackItem = Order.ItemHandle;
	bItemFeedbackFinal = false;
	ItemFeedbackUntil = GetWorld()->GetTimeSeconds() + 35.0;
	ItemFeedbackText = FText::FromString(Order.Type == ECombatOrderType::PickupItem ? TEXT("正在拾取…")
		: Order.Type == ECombatOrderType::DropItem ? TEXT("正在放置…") : TEXT("物品操作中…"));
}
FText ACombatPlayerController::GetItemStatusText() const
{
	return ItemFeedbackUnit.IsValid() && ItemFeedbackUnit.Get() == GetReadyCommandedUnit()
		&& ItemFeedbackUnit->GetLifeGeneration() == ItemFeedbackLife && GetCommandBindingGeneration() == ItemFeedbackBinding
		&& GetWorld()->GetTimeSeconds() <= ItemFeedbackUntil ? ItemFeedbackText : FText::GetEmpty();
}
void ACombatPlayerController::HandleItemBatchResult(FCombatOrderBatchResult Result)
{
	if (Result.RequestId != ItemFeedbackRequest || bItemFeedbackFinal || GetItemStatusText().IsEmpty()) return;
	FGameplayTag Failure = Result.FailureTag;
	if (Result.bAccepted && !Result.OrderResults.IsEmpty())
	{
		if (Result.OrderResults[0].bSuccess) return;
		Failure = Result.OrderResults[0].FailureTag;
	}
	ItemFeedbackText = CombatItemFeedback::Failure(Failure);
	ItemFeedbackUntil = GetWorld()->GetTimeSeconds() + 3;
	bItemFeedbackFinal = true;
}
void ACombatPlayerController::HandleItemFinalResult(FCombatOrderResult Result)
{
	if (Result.RequestId != ItemFeedbackRequest || Result.ItemHandle != FeedbackItem || Result.ControlGeneration != ItemFeedbackBinding
		|| bItemFeedbackFinal || GetItemStatusText().IsEmpty()) return;
	bItemFeedbackFinal = true;
	ItemFeedbackUntil = GetWorld()->GetTimeSeconds() + 3;
	ItemFeedbackText = Result.bSuccess ? FText::FromString(Result.Type == ECombatOrderType::PickupItem ? TEXT("已拾取物品")
		: Result.Type == ECombatOrderType::DropItem ? TEXT("已放到地面") : Result.Type == ECombatOrderType::SwapItems ? TEXT("物品已换位") : TEXT("物品已使用"))
		: CombatItemFeedback::Failure(Result.FailureTag);
}
