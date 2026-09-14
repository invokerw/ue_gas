#pragma once

#include "CoreMinimal.h"
#include "Combat/UI/CombatHUDSlotWidget.h"
#include "Blueprint/DragDropOperation.h"
#include "CombatHUDItemSlotWidget.generated.h"

class ACombatUnitCharacter;
class ACombatPlayerController;
class UCombatHUDWidget;

/** 拖拽只保存开始时的拥有者快照；服务器按物品和背包版本复核，取消不会丢弃物品。 */
UCLASS()
class COMBAT_API UCombatItemDragOperation : public UDragDropOperation
{
	GENERATED_BODY()
public:
	UPROPERTY(Transient) FCombatHUDOwnerView Snapshot;
	TWeakObjectPtr<ACombatUnitCharacter> Unit;
	TWeakObjectPtr<UCombatHUDWidget> HUD;
	int32 SourceSlot = INDEX_NONE;
	int64 ControlGeneration = 0;
	/** 控制关系和生命必须仍与拖拽开始时一致。 */
	bool IsCurrent(const ACombatPlayerController* PC) const;
	virtual void Drop_Implementation(const FPointerEvent& PointerEvent) override;
	virtual void DragCancelled_Implementation(const FPointerEvent& PointerEvent) override;
};

/** 物品槽消费拥有者投影；鼠标动作提交带版本的 Order，不预测库存或属性。 */
UCLASS(Blueprintable, meta=(DisplayName="战斗物品槽", ToolTip="显示物品、独立冷却和数量，支持点击使用、右键菜单和拖拽。"))
class COMBAT_API UCombatHUDItemSlotWidget : public UCombatHUDSlotWidget
{
	GENERATED_BODY()
public:
	/** 由主 HUD 绑定固定索引；布局与各文字位置仍在 Designer 中维护。 */
	void BindInventorySlot(UCombatHUDWidget* InHUD, int32 InSlot);
	/** 从只读快照刷新显示，倒计时只影响表现。 */
	void ShowItem(const FCombatHUDOwnerView& Owner, const FCombatUnitView& Unit, double Now, const FText& Key);
protected:
	virtual FReply NativeOnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& Geometry, const FPointerEvent& Event) override;
	virtual void NativeOnDragDetected(const FGeometry& Geometry, const FPointerEvent& Event, UDragDropOperation*& Operation) override;
	virtual bool NativeOnDrop(const FGeometry& Geometry, const FDragDropEvent& Event, UDragDropOperation* Operation) override;
private:
	/** 菜单闭包持有操作开始时的单位、生命和库存版本，避免对新物品执行旧命令。 */
	void OpenItemMenu();
	TWeakObjectPtr<UCombatHUDWidget> HUD;
	int32 SlotIndex = INDEX_NONE;
	UPROPERTY(Transient) FCombatHUDOwnerView Snapshot;
	UPROPERTY(Transient) FCombatHUDOwnerView PressSnapshot;
	TWeakObjectPtr<ACombatUnitCharacter> PressUnit;
	int64 PressControl = 0;
	bool bPressed = false;
};
