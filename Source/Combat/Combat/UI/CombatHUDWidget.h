#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Combat/View/CombatHUDViewTypes.h"
#include "Combat/View/CombatUnitViewTypes.h"
#include "CombatHUDWidget.generated.h"

class ACombatUnitCharacter;
class UCombatUnitViewComponent;
class UCombatHUDSlotWidget;
class UCombatRadialProgress;
class UTextBlock;
class UImage;
class UProgressBar;
class UPanelWidget;
class UBorder;
class UButton;
class UTexture2D;
struct FStreamableHandle;

/**
 * 底部 HUD 的本地只读适配：由 HUD Actor 持有，观察显式 CommandedUnit。
 * Blueprint Designer 提供布局和可选控件；本类只把 View 变为文字、进度和可见性，不写 ASC 或发送 Order。
 * 换单位、换生命、观察目标 EndPlay 及 Widget Destruct 时清理详情、效果子控件和异步定义加载。
 */
UCLASS(Blueprintable, meta=(DisplayName="战斗底部 HUD", ToolTip="底部居中 HUD 的只读适配基类，布局在 Widget Blueprint 中维护。"))
class COMBAT_API UCombatHUDWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	/** 原生 owner 使用的幂等绑定入口；空值清理旧表现，不在设计预览绑定 World。 */
	void InitializeForUnit(ACombatUnitCharacter* Unit);
	/** 返回当前观察目标，供本地诊断，不提供控制权。 */
	UFUNCTION(BlueprintPure, Category="Combat|HUD", meta=(DisplayName="获取 HUD 观察单位", ToolTip="当前 HUD 的弱观察目标，可能为空。"))
	ACombatUnitCharacter* GetObservedUnit() const { return BoundUnit.Get(); }
	/** 最近一次有效拥有者快照，用于蓝图扩展与验证。 */
	UFUNCTION(BlueprintPure, Category="Combat|HUD", meta=(DisplayName="获取 HUD 显示快照", ToolTip="返回本地最近一次匹配生命代次的只读快照。"))
	const FCombatHUDOwnerView& GetDisplaySnapshot() const { return DisplaySnapshot; }

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& Geometry, float DeltaTime) override;
	virtual FReply NativeOnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event) override;
	virtual FReply NativeOnMouseMove(const FGeometry& Geometry, const FPointerEvent& Event) override;
	virtual void NativeOnMouseLeave(const FPointerEvent& Event) override;

	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UWidget> HUDPanel;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UWidget> HUDFrame;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> HeroNameText;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UImage> HeroPortrait;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> HeroSymbol;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> StatsText;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UProgressBar> HealthBar;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UProgressBar> ManaBar;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> HealthText;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> ManaText;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> HealthRegenText;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> ManaRegenText;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> LevelText;
	UPROPERTY(meta=(BindWidgetOptional, DisplayName="经验文本", ToolTip="可选的等级内经验文本；显示当前等级区间经验与下一等级所需总经验。")) TObjectPtr<UTextBlock> ExperienceText;
	UPROPERTY(meta=(BindWidgetOptional, DisplayName="技能点文本", ToolTip="可选的未使用技能点文本；有可用技能点时显示在英雄 HUD 上。")) TObjectPtr<UTextBlock> AbilityPointsText;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UCombatRadialProgress> ExperienceRing;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> ActivityText;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UCombatHUDSlotWidget> SkillQ;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UCombatHUDSlotWidget> SkillW;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UCombatHUDSlotWidget> SkillE;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UCombatHUDSlotWidget> SkillR;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UPanelWidget> BuffPanel;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> BuffOverflowText;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UBorder> DetailPanel;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> DetailText;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UButton> CloseDetailButton;

	UPROPERTY(EditDefaultsOnly, Category="Combat|HUD", meta=(DisplayName="效果槽蓝图", ToolTip="动态创建 Buff / Debuff 图标的 Widget Blueprint；为空时不创建效果图标。"))
	TSubclassOf<UCombatHUDSlotWidget> BuffWidgetClass;
	UPROPERTY(EditDefaultsOnly, Category="Combat|HUD", meta=(DisplayName="定义图标", ToolTip="按稳定 Unit / Ability / Modifier ID 指定本地图标。缺失时用名称首字占位，不影响战斗。"))
	TMap<FPrimaryAssetId, TObjectPtr<UTexture2D>> DefinitionIcons;
private:
	/** 刷新同一观察目标的数据；显示连续时间窗，不从倒计时移除 Buff。 */
	UFUNCTION() void RefreshDisplay();
	/** 观察目标离开 World 后立即解绑，即使 Controller 的清空复制尚未到达。 */
	UFUNCTION() void HandleUnitEndPlay(AActor* Actor, EEndPlayReason::Type Reason);
	/** 释放当前订阅和加载句柄，可重复调用。 */
	void UnbindView();
	/** 换绑定、换生命时清空详情和子控件，不保留前一个英雄的数据。 */
	void ResetPresentation();
	/** 只在可见定义集合变化时异步加载名称；回调校验绑定版本与生命代次。 */
	void RequestDefinitions(const TArray<FPrimaryAssetId>& Ids);
	/** 使用已经加载的本地定义名，缺失时显示稳定定义名。 */
	static FText ResolveName(const FPrimaryAssetId& Id);
	/** 展示或固定一个槽位详情，空悬停请求关闭未固定详情。 */
	void HandleDetail(const FText& Text, bool bPin, UCombatHUDSlotWidget* Source);
	/** 关闭详情并清理固定来源。 */
	UFUNCTION() void CloseDetail();
	/** 返回 Designer 中有效的技能子控件，包括空位，维持四槽索引。 */
	TArray<UCombatHUDSlotWidget*> GetSkillWidgets() const;
	/** 将技能槽上方的加点按钮转换为一次服务器权威升级请求。 */
	void HandleUpgradeRequested(UCombatHUDSlotWidget* Source);
	/** 解析可选图标配置。 */
	UTexture2D* FindIcon(const FPrimaryAssetId& Id) const;
	/** 以本次 View 快照生成英雄属性详情，尚未复制的字段保持空白。 */
	FText BuildHeroDetail() const;

	TWeakObjectPtr<ACombatUnitCharacter> BoundUnit;
	TWeakObjectPtr<ACombatUnitCharacter> EndedUnit;
	TWeakObjectPtr<UCombatUnitViewComponent> BoundView;
	TWeakObjectPtr<UCombatHUDSlotWidget> DetailSource;
	TSharedPtr<FStreamableHandle> DefinitionLoad;
	TArray<FPrimaryAssetId> RequestedDefinitions;
	TArray<FCombatModifierHandle> ModifierIdentities;
	uint64 BindingRevision = 0;
	int64 DisplayLifeGeneration = 0;
	bool bConstructed = false;
	bool bDetailPinned = false;
	bool bHeroHovered = false;
	float RefreshAccumulator = 0.0f;
	UPROPERTY(Transient) FCombatHUDOwnerView DisplaySnapshot;
	UPROPERTY(Transient) TArray<TObjectPtr<UCombatHUDSlotWidget>> BuffWidgets;
};
