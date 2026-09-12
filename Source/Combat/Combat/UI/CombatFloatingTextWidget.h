#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Combat/UI/CombatOverheadTypes.h"
#include "CombatFloatingTextWidget.generated.h"

/** 单条跳字的数据桥梁；头顶 Widget 创建并持有，文字、轨迹和动画完成移除均由蓝图实现。 */
UCLASS(BlueprintType, Blueprintable, meta=(DisplayName="战斗跳字界面", ToolTip="接收真实结果供蓝图显示，完全不参与战斗结算。"))
class COMBAT_API UCombatFloatingTextWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	/** 在添加到父容器前写入只读载荷；构建完成后通知蓝图。 */
	void InitializeFloatingText(const FCombatFloatingTextPayload& InData) { FloatingData = InData; }
protected:
	virtual void NativeConstruct() override;
	/** 每次构建时重置蓝图动画，保证预先传入的数据不会丢失。 */
	UFUNCTION(BlueprintImplementableEvent, BlueprintCosmetic, Category="Combat|UI", meta=(DisplayName="初始化跳字表现", ToolTip="控件构建后设置文字和样式，并从零开始本地动画。"))
	void OnFloatingTextInitialized(UPARAM(DisplayName="跳字数据") const FCombatFloatingTextPayload& Data);
	/** 仅本地可见，蓝图不修改真实结算值。 */
	UPROPERTY(Transient, BlueprintReadOnly, Category="Combat|UI", meta=(DisplayName="跳字数据", ToolTip="当前跳字的只读服务器结果。"))
	FCombatFloatingTextPayload FloatingData;
};
