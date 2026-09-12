#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Combat/UI/CombatOverheadTypes.h"
#include "Combat/View/CombatUnitViewTypes.h"
#include "CombatOverheadWidget.generated.h"

class ACombatUnitCharacter;
class UCombatUnitViewComponent;
class UCombatFloatingTextWidget;
struct FStreamableHandle;

/**
 * 头顶蓝图的只读适配基类：持有 Unit/View 弱绑定、展示快照及名称加载句柄。
 * 控件树和动画在蓝图实现，本类不构造资源条、不读取 Modifier Runtime、不修改战斗。
 * 换单位、换生命及销毁时清理旧表现；控件重建后重绑并重送快照。
 */
UCLASS(BlueprintType, Blueprintable, meta=(DisplayName="战斗头顶界面", ToolTip="提供只读展示数据和生命周期；控件树与动画由蓝图实现。"))
class COMBAT_API UCombatOverheadWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 幂等绑定；空指针立即解绑并清空展示，设计预览不绑定运行时单位。 */
	void InitializeForUnit(ACombatUnitCharacter* InUnit);
	/** 接收服务器结果；数值非法、控件未就绪或生命代次不匹配时忽略。 */
	void AddFloatingText(float Amount, ECombatFloatingTextType Type, int64 LifeGeneration);

	/** 创建配置的跳字并限制最多十二条；蓝图负责添加到容器和播放动画。 */
	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category="Combat|UI", meta=(DisplayName="创建战斗跳字", ToolTip="创建本地跳字，最多保留十二条；返回后由蓝图添加到容器。无配置或旧生命载荷返回空。"))
	UCombatFloatingTextWidget* CreateFloatingText(UPARAM(DisplayName="跳字数据") const FCombatFloatingTextPayload& Data);

	/** 返回最近一次本地只读快照，供蓝图分区刷新。 */
	UFUNCTION(BlueprintPure, Category="Combat|UI", meta=(DisplayName="获取头顶展示数据", ToolTip="返回本地只读展示快照，不查询战斗 Runtime。"))
	const FCombatOverheadDisplayData& GetDisplayData() const { return DisplayData; }

	/** 从安全 View 归并多来源状态和资源，不访问控件，便于独立验证。 */
	static FCombatOverheadDisplayData BuildDisplayData(const FCombatUnitView& View,
		const TArray<FCombatModifierView>& Modifiers, const TArray<FCombatControlPresentationRule>& Rules,
		ECombatTeamRelation Relation, const FText& UnitName, const FText& AbilityName);
	/** 根据服务器绝对时间窗计算进度；无限状态剩余时间为负一。 */
	static FCombatOverheadProgressData ComputeProgress(const FCombatOverheadDisplayData& Data, double ServerTime);

protected:
	virtual void NativePreConstruct() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	/** 数据变化或重建时推送完整快照，蓝图设置资源、文字与状态。 */
	UFUNCTION(BlueprintImplementableEvent, BlueprintCosmetic, Category="Combat|UI", meta=(DisplayName="展示数据已变化", ToolTip="数据变化或重建时推送完整只读展示快照。"))
	void OnDisplayDataChanged(UPARAM(DisplayName="展示数据") const FCombatOverheadDisplayData& Data);
	/** 只更新活动时间窗；蓝图不得据此结束技能。 */
	UFUNCTION(BlueprintImplementableEvent, BlueprintCosmetic, Category="Combat|UI", meta=(DisplayName="展示进度已变化", ToolTip="使用校准服务器时间更新本地进度，不改变技能状态。"))
	void OnProgressChanged(UPARAM(DisplayName="进度数据") const FCombatOverheadProgressData& Progress);
	/** 收到当前生命的真实结果后触发，可在蓝图创建跳字。 */
	UFUNCTION(BlueprintImplementableEvent, BlueprintCosmetic, Category="Combat|UI", meta=(DisplayName="收到战斗跳字", ToolTip="有效的当前生命结果到达时触发，只能用于本地表现。"))
	void OnFloatingTextRequested(UPARAM(DisplayName="跳字数据") const FCombatFloatingTextPayload& Data);
	/** 原生层移除跳字后，蓝图清空自己的动画和缓存。 */
	UFUNCTION(BlueprintImplementableEvent, BlueprintCosmetic, Category="Combat|UI", meta=(DisplayName="清空头顶表现", ToolTip="换单位、换生命、销毁或重建时清除旧动画。"))
	void OnPresentationReset();

	/** 蓝图配置显示策略，C++ 只执行稳定归并。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|UI", meta=(DisplayName="控制状态展示规则", ToolTip="控制标签、名称、优先级和颜色；空数组不显示控制条。", TitleProperty="Label"))
	TArray<FCombatControlPresentationRule> ControlRules;
	/** 单条跳字的视觉蓝图。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|UI", meta=(DisplayName="跳字控件类", ToolTip="用于客户端创建跳字；空值禁用跳字，服务器结算不受影响。"))
	TSubclassOf<UCombatFloatingTextWidget> FloatingTextWidgetClass;
	/** 仅设计器使用，不建立 World 绑定。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|UI", meta=(DisplayName="设计预览数据", ToolTip="仅在 UMG 设计器使用，可预览资源和状态布局。"))
	FCombatOverheadDisplayData PreviewData;

private:
	/** View 变化后的唯一刷新入口。 */
	UFUNCTION() void HandleViewChanged();
	/** 解绑委托并取消名称加载，可重复执行。 */
	void UnbindView();
	/** 移除登记的跳字并通知蓝图复位。 */
	void ResetPresentation();
	/** 解析已加载定义；缺失时使用稳定 ID 占位。 */
	static FText ResolveDefinitionName(const FPrimaryAssetId& DefinitionId);
	/** 定义变化时异步加载名称，回调检查绑定版本和生命代次。 */
	void RequestDefinitionNames(const FCombatUnitView& View);
	/** 用本地指挥单位的复制队伍查询统一 Team 关系。 */
	ECombatTeamRelation ResolveViewerRelation() const;

	/** Unit 弱引用允许 Widget 重建后恢复绑定；组件 EndPlay 会显式清空。 */
	TWeakObjectPtr<ACombatUnitCharacter> BoundUnit;
	TWeakObjectPtr<UCombatUnitViewComponent> BoundView;
	/** 本 Widget 独占的异步加载句柄，换绑定或销毁时取消。 */
	TSharedPtr<FStreamableHandle> DefinitionLoadHandle;
	FPrimaryAssetId RequestedUnitId;
	FPrimaryAssetId RequestedAbilityId;
	/** 异步加载版本；每次解绑递增，拒绝旧回调。 */
	uint64 BindingRevision = 0;
	bool bReadyForEvents = false;
	int32 FloatingSequence = 0;
	/** 本地派生快照，不复制、不写回 ASC。 */
	UPROPERTY(Transient) FCombatOverheadDisplayData DisplayData;
	/** 持有至动画结束或上限淘汰，不跨 Unit 生命。 */
	UPROPERTY(Transient) TArray<TObjectPtr<UCombatFloatingTextWidget>> FloatingWidgets;
};
