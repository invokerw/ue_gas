#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Combat/View/CombatHUDViewTypes.h"
#include "Combat/View/CombatUnitViewTypes.h"
#include "CombatHUDSlotWidget.generated.h"

class UTextBlock;
class UImage;
class UCombatRadialProgress;
class UTexture2D;

/** 槽位请求主 HUD 显示详情；空文本结束悬停，点击请求固定。 */
DECLARE_MULTICAST_DELEGATE_TwoParams(FCombatHUDDetailRequested, const FText&, bool);

/** 技能与 Buff 蓝图的只读适配；所有控件和样式由 Designer 提供，空绑定安全跳过。 */
UCLASS(Blueprintable, meta=(DisplayName="战斗 HUD 槽位", ToolTip="显示技能或 Buff 的只读内容，不提交战斗请求。"))
class UE_GAS_API UCombatHUDSlotWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	/** 显示一个技能；未复制完成时显示空槽，时间只用于本地冷却动画。 */
	void ShowAbility(const FCombatHUDAbilityView& Ability, const FCombatUnitView& Unit,
		double ServerTime, const FText& Name, const FText& Description, UTexture2D* Texture, const FText& Key);
	/** 显示可见效果的剩余时间和层数；到零仍等待权威 FastArray 移除。 */
	void ShowModifier(const FCombatModifierView& Modifier, double ServerTime, const FText& Name, UTexture2D* Texture);
	/** 丢弃旧单位或旧生命的详情与内容，快捷键由下一次快照恢复。 */
	void ClearEntry();
	FCombatHUDDetailRequested OnDetailRequested;
	const FText& GetDetailText() const { return DetailText; }
	/** 公共显示规则，供展示与自动化共用；剩余时间不会为负或 NaN。 */
	static float Remaining(double EndTime, double ServerTime);
protected:
	virtual void NativeOnMouseEnter(const FGeometry& Geometry, const FPointerEvent& Event) override;
	virtual void NativeOnMouseLeave(const FPointerEvent& Event) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event) override;
	virtual void NativeDestruct() override;

	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UImage> IconImage;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UImage> BlockedShade;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UImage> CooldownShade;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> SymbolText;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> HotkeyText;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> CostText;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> CountText;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> StackText;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> RankText;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UCombatRadialProgress> DurationRing;
	UPROPERTY(EditDefaultsOnly, Category="Appearance", meta=(DisplayName="增益颜色", ToolTip="增益图标和持续时间环的颜色。"))
	FLinearColor BuffColor = FLinearColor(0.42f, 0.65f, 0.29f);
	UPROPERTY(EditDefaultsOnly, Category="Appearance", meta=(DisplayName="减益颜色", ToolTip="减益图标和持续时间环的颜色。"))
	FLinearColor DebuffColor = FLinearColor(0.75f, 0.31f, 0.26f);
private:
	/** 使用配置纹理；缺失美术时保留名称首字占位。 */
	void SetIcon(UTexture2D* Texture, const FText& Name);
	FText DetailText;
};
