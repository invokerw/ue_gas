#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Combat/Economy/CombatEconomyTypes.h"
#include "Combat/Economy/CombatShopData.h"
#include "CombatShopWidget.generated.h"

class ACombatPlayerController;
class UCombatEconomyComponent;
class UCombatItemData;
class UTexture2D;
class SEditableTextBox;
class SScrollBox;
class SVerticalBox;
class SWidget;
struct FSlateBrush;

/**
 * 独立的紧凑全局商店。
 *
 * 商店只展示搜索、基础/升级目录和“结果 → 直接组件”的合成方式；金币与六格
 * 储藏室由 UCombatStashWidget 常驻展示。目录和合成节点只提交稳定物品定义意图，
 * 价格、组件消费、空间与事务结果仍由服务器权威 Economy 重新计算。
 */
UCLASS(meta=(DisplayName="战斗商店界面", ToolTip="紧凑全局商店；左键查看配方，右键提交购买意图。"))
class COMBAT_API UCombatShopWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UCombatShopWidget(const FObjectInitializer& Initializer);
	/** 当前指针是否命中已打开商店的实际 Slate 几何。 */
	bool IsScreenPositionOverUI(FVector2D Position) const;
	bool IsShopOpen() const { return bShopOpen; }
	/** 打开时刷新目录与配方；关闭只恢复游戏焦点，不清空本地筛选和选择。 */
	void SetShopOpen(bool bOpen);
	/** 目录和配方节点共用的固定外框尺寸，仅用于本地表现和 Automation 断言。 */
	static FVector2D GetCompactNodeSize();
	/** 商店布局的设计分辨率；所有面板百分比先在该基准上换算。 */
	static FVector2D GetReferenceViewportSize();
	/** 当前商店内容面板尺寸，供布局回归测试读取。 */
	static FVector2D GetCompactPanelSize();
	/** 当前物品区与合成区的固定高度；0 表示仍为弹性/内容自适应。 */
	static FVector2D GetFixedRegionHeights();
	/** 当前面板顶部间距；旧底部锚定布局按设计分辨率换算为等效顶距。 */
	static float GetPanelTopOffset();
	/** 当前商店宽度相对 v0.1 760 宽基准的比例。 */
	static float GetPanelWidthScale();

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual FReply NativeOnPreviewMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event) override;
	virtual FReply NativeOnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;

private:
#if WITH_DEV_AUTOMATION_TESTS
	friend class FCombatShopWidgetStructureTest;
#endif

	struct FItemEntry
	{
		FPrimaryAssetId DefinitionId;
		TSharedPtr<SWidget> HitWidget;
		/** SImage 保存裸 Brush 指针，因此条目必须至少与对应节点拥有相同生命周期。 */
		TSharedPtr<FSlateBrush> IconBrush;
	};

	ACombatPlayerController* GetCombatPlayer() const;
	UCombatEconomyComponent* GetEconomy() const;
	UCombatShopData* ResolveShopData();
	UCombatItemData* ResolveItem(const FPrimaryAssetId& DefinitionId) const;
	static FText ResolveItemName(const UCombatItemData* Item);
	static FText ResolveItemGlyph(const UCombatItemData& Item);
	void BindEconomy();
	void UnbindEconomy();
	void RebuildCatalog();
	void RebuildRecipe();
	void SelectItem(FPrimaryAssetId DefinitionId);
	void PurchaseItem(FPrimaryAssetId DefinitionId);
	int32 CountOwned(const FPrimaryAssetId& DefinitionId) const;
	FText BuildItemToolTip(const UCombatItemData& Item) const;
	TSharedRef<SWidget> BuildItemNode(UCombatItemData& Item, const FText& BadgeText,
		const FText& ToolTip, TArray<FItemEntry>& Entries, TArray<TObjectPtr<UTexture2D>>& IconResources,
		bool bSelected = false);

	UFUNCTION() void RefreshFromEconomy();

	TWeakObjectPtr<UCombatEconomyComponent> BoundEconomy;
	UPROPERTY(Transient) TObjectPtr<UCombatShopData> DisplayShopData;
	FCombatEconomyView DisplayView;
	ECombatShopPage CurrentPage = ECombatShopPage::Basic;
	FPrimaryAssetId SelectedItem;
	FString SearchText;
	bool bShopOpen = false;

	TSharedPtr<SWidget> ShopPanel;
	TSharedPtr<SEditableTextBox> SearchBox;
	TSharedPtr<SVerticalBox> CatalogBox;
	/** 仅用于结构回归；v0.3 完成后应始终为空，证明合成区没有滚动容器。 */
	TSharedPtr<SScrollBox> RecipeScrollBox;
	TSharedPtr<SVerticalBox> RecipeBox;
	TArray<FItemEntry> CatalogEntries;
	TArray<FItemEntry> RecipeEntries;
	UPROPERTY(Transient) TArray<TObjectPtr<UTexture2D>> CatalogIconResources;
	UPROPERTY(Transient) TArray<TObjectPtr<UTexture2D>> RecipeIconResources;
};
