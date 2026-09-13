#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Combat/Log/CombatLogTypes.h"
#include "Styling/SlateTypes.h"
#include "CombatLogWidget.generated.h"

class UCombatLogComponent;
class UButton;
class UCheckBox;
class UComboBoxString;
class USlider;
class UTextBlock;
class UScrollBox;
class URichTextBlock;
class UDataTable;
struct FStreamableHandle;

/** 根据未移动的窗口几何约束平移，避免缓存几何形成逐帧反馈。 */
namespace CombatLogWindowLayout
{
	/** 将父容器本地平移限制在玩家视口内，包含 DPI 与 ScaleBox 缩放；尚未排布时保留原值。 */
	COMBAT_API FVector2D ClampTranslation(const FGeometry& Viewport, const FGeometry& PanelParent, FVector2D Translation);
}

/**
 * 战斗记录的本地只读界面。Blueprint Designer 提供入口、窗口和筛选布局，本类负责数据订阅、文字和过滤。
 * HUD 强持有 Widget；Widget 弱观察 Controller 的日志组件。关闭不清空历史，销毁/重建显式解绑和取消加载。
 */
UCLASS(Blueprintable, meta=(DisplayName="战斗记录窗口", ToolTip="服务器历史的本地筛选与彩色文字展示；控件布局由 Widget Blueprint 维护。"))
class COMBAT_API UCombatLogWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	UCombatLogWidget(const FObjectInitializer& Initializer);
	/** 幂等绑定当前玩家历史，空值清空显示；不允许写入服务器记录。 */
	void InitializeForLog(UCombatLogComponent* Component);
	/** 开关记录窗口；打开时聚焦并滚动至最新，不暂停世界或改变战斗。 */
	void SetLogOpen(bool bOpen);
	bool IsLogOpen() const { return bLogOpen; }
	int32 GetVisibleEntryCount() const { return VisibleSequences.Num(); }

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& Geometry, float DeltaTime) override;
	virtual FReply NativeOnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& Geometry, const FPointerEvent& Event) override;
	virtual FReply NativeOnMouseMove(const FGeometry& Geometry, const FPointerEvent& Event) override;
	virtual void NativeOnMouseCaptureLost(const FCaptureLostEvent& Event) override;
	virtual FReply NativeOnMouseButtonDoubleClick(const FGeometry& Geometry, const FPointerEvent& Event) override;

	UPROPERTY(meta=(BindWidget)) TObjectPtr<UButton> LogEntryButton;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UWidget> LogPanel;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UWidget> LogScale;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UWidget> LogTitleBar;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UButton> CloseLogButton;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UScrollBox> LogScrollBox;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UComboBoxString> AttackerCombo;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UComboBoxString> TargetCombo;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UCheckBox> DamageCheck;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UCheckBox> HealingCheck;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UCheckBox> AbilityCheck;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UCheckBox> ItemCheck;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UCheckBox> StatusCheck;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UCheckBox> NonHeroCheck;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UCheckBox> FollowLatestCheck;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<USlider> TimeRangeSlider;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> TimeRangeText;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> RecordCountText;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> EmptyStateText;
	UPROPERTY(EditDefaultsOnly, Category="Combat|Log|Style", meta=(DisplayName="记录字体", ToolTip="彩色记录行使用的字体和字号，窗口整体缩放由 Designer 控制。"))
	FSlateFontInfo LogFont;
	UPROPERTY(EditDefaultsOnly, Category="Combat|Log|Style", meta=(DisplayName="正文颜色", ToolTip="记录中叙述文字的默认颜色。"))
	FLinearColor TextColor = FLinearColor(0.80f, 0.83f, 0.88f);
	UPROPERTY(EditDefaultsOnly, Category="Combat|Log|Style", meta=(DisplayName="来源颜色", ToolTip="攻击者和效果来源名称的颜色。"))
	FLinearColor SourceColor = FLinearColor(0.20f, 0.48f, 1.0f);
	UPROPERTY(EditDefaultsOnly, Category="Combat|Log|Style", meta=(DisplayName="效果颜色", ToolTip="技能与持续效果名称的颜色。"))
	FLinearColor EffectColor = FLinearColor(0.35f, 0.91f, 0.92f);
	UPROPERTY(EditDefaultsOnly, Category="Combat|Log|Style", meta=(DisplayName="生命颜色", ToolTip="治疗量和生命前后值的颜色。"))
	FLinearColor HealthColor = FLinearColor(0.30f, 1.0f, 0.32f);
	UPROPERTY(EditDefaultsOnly, Category="Combat|Log|Style", meta=(DisplayName="状态颜色", ToolTip="效果获得、移除或中断文字的颜色。"))
	FLinearColor StatusColor = FLinearColor(0.70f, 0.58f, 1.0f);

private:
	/** 只允许标题栏空白或文字区域启动拖动，排除标题栏按钮与固定入口。 */
	bool IsTitleDragLocation(const FVector2D& ScreenPosition) const;
	/** 应用本地位移并约束视口边界；不改变 Designer 的锚点、尺寸或入口位置。 */
	void MoveLogPanel(const FGeometry& Viewport, const FVector2D& Translation);
	/** 结束拖动，只释放本 Widget 持有的对应用户/指针捕获，关闭与销毁可重复调用。 */
	void CancelWindowDrag();
	/** 释放组件订阅及定义加载；允许 Destruct 和重新绑定重复调用。 */
	void UnbindLog();
	/** 标记内容变化，合并到下一次本地 UI 刷新，不按每个伤害事件重建控件。 */
	void HandleHistoryChanged();
	/** 按当前条件更新复用的文字行与空态，不修改历史。 */
	void RefreshDisplay();
	/** 根据历史中的稳定实例构建下拉列表，同名单位追加实例编号。 */
	void RefreshUnitOptions(const TArray<FCombatLogEntry>& Entries);
	/** 异步加载记录涉及的定义；回调校验本次绑定修订号。 */
	void RequestDefinitions(const TArray<FCombatLogEntry>& Entries);
	/** 生成瞬态富文本样式，颜色与字体来自 Designer 默认值。 */
	void BuildTextStyles();
	/** 从本地定义读取名称，未加载时回退稳定定义名。 */
	static FString ResolveName(const FPrimaryAssetId& Id);
	/** 使用校准的服务器游戏时间计算筛选窗口。 */
	double GetServerTime() const;
	/** 控件事件仅改变本地显示状态。 */
	UFUNCTION() void ToggleLog();
	/** 关闭并归还游戏视口焦点。 */
	UFUNCTION() void CloseLog();
	/** 读取全部分类复选框，组合更新本地条件。 */
	UFUNCTION() void HandleCategoryChanged(bool bChecked);
	/** 选择来源实例，不使用名字作为身份键。 */
	UFUNCTION() void HandleAttackerChanged(FString Option, ESelectInfo::Type SelectionType);
	/** 选择目标实例。 */
	UFUNCTION() void HandleTargetChanged(FString Option, ESelectInfo::Type SelectionType);
	/** 滑条吸附到 30/60/120/300 秒与全部保留历史。 */
	UFUNCTION() void HandleTimeRangeChanged(float Value);
	/** 手动上滚暂停跟随，回到底部恢复跟随。 */
	UFUNCTION() void HandleUserScrolled(float Offset);
	/** 显式恢复或暂停最新记录跟随。 */
	UFUNCTION() void HandleFollowChanged(bool bChecked);

	/** 当前历史源；Slate 销毁时保留弱选择以便重建，但立即移除所有订阅。 */
	TWeakObjectPtr<UCombatLogComponent> BoundLog;
	TSharedPtr<FStreamableHandle> DefinitionLoad;
	TArray<FPrimaryAssetId> RequestedDefinitions;
	TMap<FString, int32> UnitOptions;
	TArray<int64> VisibleSequences;
	FCombatLogFilter Filter;
	uint64 BindingRevision = 0;
	float RefreshAccumulator = 0.0f;
	bool bConstructed = false;
	bool bLogOpen = false;
	bool bDirty = true;
	bool bOptionsDirty = true;
	bool bUpdatingControls = false;
	bool bFollowLatest = true;
	/** 一次本地左键拖动的固定起点；位置保存在面板变换中，关闭窗口不重置。 */
	FVector2D DragStartScreenPosition = FVector2D::ZeroVector;
	FVector2D DragStartTranslation = FVector2D::ZeroVector;
	int32 DragUserIndex = 0;
	uint32 DragPointerIndex = 0;
	bool bDraggingWindow = false;
	UPROPERTY(Transient) TObjectPtr<UDataTable> TextStyles;
	UPROPERTY(Transient) TArray<TObjectPtr<URichTextBlock>> Rows;
};
