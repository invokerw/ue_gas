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
class UCombatHUDSlotWidget;
class UButton;

/** 槽位请求主 HUD 显示详情；空文本结束悬停，固定由详情入口触发。 */
DECLARE_MULTICAST_DELEGATE_TwoParams(FCombatHUDDetailRequested, const FText&, bool);
/** 技能升级按钮请求主 HUD 向服务器提交一次技能加点。 */
DECLARE_MULTICAST_DELEGATE_OneParam(FCombatHUDUpgradeRequested, UCombatHUDSlotWidget*);
/** 技能槽请求主 HUD 使用对应的 Q/W/E/R 槽位；只传递本地 UI 来源，不直接触发战斗结算。 */
DECLARE_MULTICAST_DELEGATE_OneParam(FCombatHUDAbilityUseRequested, UCombatHUDSlotWidget*);

/** 技能与 Buff 蓝图的展示适配；技能左键只发本地使用意图，所有控件和样式由 Designer 提供。 */
UCLASS(Blueprintable, meta=(DisplayName="战斗 HUD 槽位", ToolTip="显示技能或 Buff；技能左键发出本地使用意图，Buff 只展示详情。"))
class COMBAT_API UCombatHUDSlotWidget : public UUserWidget
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
	FCombatHUDUpgradeRequested OnUpgradeRequested;
	FCombatHUDAbilityUseRequested OnAbilityUseRequested;
	const FText& GetDetailText() const { return DetailText; }
	/** 公共显示规则，供展示与自动化共用；剩余时间不会为负或 NaN。 */
	static float Remaining(double EndTime, double ServerTime);
	/** 返回 Designer 按钮或运行时兼容按钮，供父 HUD 阻止加点区域点击穿透。 */
	UButton* GetEffectiveUpgradeButton() const;
protected:
	virtual void NativeConstruct() override;
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
	/** Designer 可选的加点按钮；缺失时会在技能槽根 CanvasPanel 上动态创建。 */
	UPROPERTY(meta=(BindWidgetOptional, DisplayName="技能升级按钮", ToolTip="可选的技能加点按钮；点击只提交服务器技能升级请求。")) TObjectPtr<class UButton> UpgradeButton;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UCombatRadialProgress> DurationRing;
	UPROPERTY(EditDefaultsOnly, Category="Appearance", meta=(DisplayName="增益颜色", ToolTip="增益图标和持续时间环的颜色。"))
	FLinearColor BuffColor = FLinearColor(0.42f, 0.65f, 0.29f);
	UPROPERTY(EditDefaultsOnly, Category="Appearance", meta=(DisplayName="减益颜色", ToolTip="减益图标和持续时间环的颜色。"))
	FLinearColor DebuffColor = FLinearColor(0.75f, 0.31f, 0.26f);
private:
	friend class FCombatHUDSkillClickTest;
	/** 为旧版技能槽蓝图补建位于技能图标上方的“+”按钮。 */
	void CreateRuntimeUpgradeButton();
	/** UButton 点击回调；只广播 UI 请求，不直接修改 ASC。 */
	UFUNCTION() void HandleUpgradeClicked();
protected:
	/** 使用配置纹理；缺失美术时保留名称首字占位。 */
	void SetIcon(UTexture2D* Texture, const FText& Name);
	FText DetailText;
private:
	UPROPERTY(Transient) TObjectPtr<class UButton> RuntimeUpgradeButton;
	/** 当前内容是否为已复制的 Ability；Buff 或空槽点击仍只处理详情/输入消费。 */
	bool bIsAbilityEntry = false;
};
