#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Combat/Economy/CombatEconomyTypes.h"
#include "CombatStashWidget.generated.h"

class ACombatPlayerController;
class UCombatEconomyComponent;
class UCombatItemData;
class UCombatShopWidget;
class UTexture2D;
class SButton;
class SImage;
class STextBlock;
class SWidget;
struct FSlateBrush;

/**
 * 常驻在 HUD 右下角的玩家储藏室。
 *
 * 该 Widget 只投影拥有者的 Economy View，不保存或计算经济状态；六个槽位、
 * 金币入口和“全部拿走”都是本地 Slate 控件，所有转移/出售仍提交给 Controller
 * 的既有服务器权威事务。金币按钮通过弱引用打开独立的 UCombatShopWidget。
 */
UCLASS(meta=(DisplayName="战斗储藏室界面", ToolTip="显示玩家金币和六格储藏室；交易仍由服务器复核。"))
class COMBAT_API UCombatStashWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UCombatStashWidget(const FObjectInitializer& Initializer);

	/** 连接 HUD 同生命周期内的商店 Widget；不转移所有权，也不形成强引用。 */
	void SetShopWidget(UCombatShopWidget* InShopWidget);
	UCombatShopWidget* GetShopWidget() const { return ShopWidget.Get(); }
	/** 储藏室实际可见几何命中时阻止世界订单/落点输入。 */
	bool IsScreenPositionOverUI(FVector2D Position) const;
	/** 紧凑储藏格的固定外框尺寸，仅用于本地表现和 Automation 断言。 */
	static FVector2D GetCompactSlotSize();
	/** “全部拿走”和金币入口共用的固定外框尺寸。 */
	static FVector2D GetCompactActionSize();

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual FReply NativeOnPreviewMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event) override;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;

private:
#if WITH_DEV_AUTOMATION_TESTS
	friend class FCombatShopWidgetStructureTest;
#endif

	ACombatPlayerController* GetCombatPlayer() const;
	UCombatEconomyComponent* GetEconomy() const;
	void BindEconomy();
	void UnbindEconomy();
	void RefreshSlot(int32 SlotIndex);
	void TransferStashSlot(int32 SlotIndex);
	void SellStashSlot(int32 SlotIndex);
	void SetItemVisual(int32 SlotIndex, const FCombatItemView& View);
	static UCombatItemData* ResolveItem(const FPrimaryAssetId& DefinitionId);
	static FText ResolveItemName(const UCombatItemData* Item);

	UFUNCTION() void RefreshFromEconomy();

	TWeakObjectPtr<UCombatEconomyComponent> BoundEconomy;
	TWeakObjectPtr<UCombatShopWidget> ShopWidget;
	FCombatEconomyView DisplayView;

	TSharedPtr<SWidget> StashPanel;
	TSharedPtr<SButton> GoldButton;
	TSharedPtr<SButton> TakeAllButton;
	TSharedPtr<STextBlock> GoldText;
	TArray<TSharedPtr<SWidget>> StashHitWidgets;
	TArray<TSharedPtr<SImage>> StashImages;
	TArray<TSharedPtr<STextBlock>> StashGlyphs;
	TArray<TSharedPtr<STextBlock>> StashQuantities;
	TArray<TSharedPtr<FSlateBrush>> StashBrushes;
	UPROPERTY(Transient) TArray<TObjectPtr<UTexture2D>> IconResources;
};
