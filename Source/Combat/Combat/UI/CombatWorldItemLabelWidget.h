#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CombatWorldItemLabelWidget.generated.h"
class STextBlock;
/** 地面物品的屏幕空间文字，使用 Slate 字体回退显示中文，不参与世界命中。 */
UCLASS()
class COMBAT_API UCombatWorldItemLabelWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	UCombatWorldItemLabelWidget(const FObjectInitializer& Initializer) : Super(Initializer) { SetVisibility(ESlateVisibility::HitTestInvisible); }
	/** 只刷新服务器物品投影的名称、数量与颜色。 */
	void ShowItem(const FText& Name, int32 Quantity, FLinearColor Color);
protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;
private:
	TSharedPtr<STextBlock> Label;
	FText DisplayText;
	FLinearColor Tint = FLinearColor::White;
};
