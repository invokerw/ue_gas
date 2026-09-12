#pragma once

#include "CoreMinimal.h"
#include "Components/Widget.h"
#include "CombatRadialProgress.generated.h"

class SCombatRadialProgress;

/** 可放入 UMG Designer 的圆形进度环；仅绘制本地百分比，用于经验和效果持续时间。 */
UCLASS(meta=(DisplayName="战斗圆形进度环", ToolTip="从顶部顺时针填充的圆环，不维护或结束任何战斗状态。"))
class COMBAT_API UCombatRadialProgress : public UWidget
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Appearance", meta=(DisplayName="进度", ToolTip="0 到 1；非法值显示为空环。", ClampMin="0", ClampMax="1"))
	float Percent = 0.625f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Appearance", meta=(DisplayName="进度颜色", ToolTip="已完成部分的圆环颜色。"))
	FLinearColor FillColor = FLinearColor(0.67f, 0.49f, 0.20f, 1.0f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Appearance", meta=(DisplayName="轨道颜色", ToolTip="未完成部分的圆环颜色。"))
	FLinearColor TrackColor = FLinearColor(0.06f, 0.08f, 0.10f, 1.0f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Appearance", meta=(DisplayName="圆环粗细", ToolTip="圆环的设计厚度，单位为 Slate 布局单位。", ClampMin="1"))
	float Thickness = 3.0f;

	/** 更新显示进度并使 Slate 绘制缓存失效；不发送 gameplay 请求。 */
	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category="Combat|UI", meta=(DisplayName="设置圆环进度", ToolTip="仅刷新本地圆形进度显示。"))
	void SetProgress(UPARAM(DisplayName="进度") float InPercent);
	virtual void SynchronizeProperties() override;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;
protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
private:
	TSharedPtr<SCombatRadialProgress> Ring;
};
