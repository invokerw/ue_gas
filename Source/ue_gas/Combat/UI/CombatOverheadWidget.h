#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "Combat/UI/CombatOverheadTypes.h"

#include "CombatOverheadWidget.generated.h"

class ACombatUnitCharacter;
class UCanvasPanel;
class UCombatUnitViewComponent;
class UHorizontalBox;
class UOverlay;
class UProgressBar;
class USizeBox;
class UTextBlock;
class UVerticalBox;
struct FCombatModifierView;
struct FGameplayTag;

/**
 * 无需 Blueprint 资产即可使用的 DOTA 风格 Unit 头顶信息 Widget。
 * Widget 只消费 CombatUnitView 的复制快照和可丢弃跳字事件，负责资源条、施法/控制状态与本地动画；它不读取 Runtime UObject，也不能修改 gameplay 状态。
 */
UCLASS(BlueprintType)
class UE_GAS_API UCombatOverheadWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 绑定 WidgetComponent 所属 Unit 及其 UI 安全复制 View。 */
	void InitializeForUnit(ACombatUnitCharacter* InUnit);
	/** 在本地添加按伤害/治疗类型着色的跳字，显示值四舍五入为整数；最多同时保留 12 条，超限时移除最旧项。控件未就绪、数值非有限或接近 0 时忽略。 */
	void AddFloatingText(float Amount, ECombatFloatingTextType Type);

protected:
	/** 在纯 C++ Widget 类第一次构建 Slate 树前创建 UMG 控件层级。 */
	virtual TSharedRef<SWidget> RebuildWidget() override;
	/** 驱动技能条、控制条、血量拖影和浮动数字动画。 */
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	/** 解绑复制 View 委托。 */
	virtual void NativeDestruct() override;

private:
	/** Unit View 或 Modifier View 变化后的统一刷新入口。 */
	UFUNCTION()
	void HandleViewChanged();

	/** 创建固定尺寸的头顶 UI 控件树。 */
	void BuildWidgetTree();
	/** 刷新名称、资源、队伍颜色和可见性。 */
	void RefreshResources();
	/** 从可见状态中选最高优先级控制状态决定颜色，并从对应效果快照获取时间窗；文字列出当前所有可识别控制状态。这里只选择展示内容，不改变控制效果。 */
	void RefreshControlState();
	/** 从当前 Ability 时间窗刷新技能施法/引导条。 */
	void RefreshAbilityState();
	/** 施法/引导条或控制条出现时隐藏名字；两者都消失后恢复。 */
	void RefreshNameVisibility();
	/** 按最大生命每 250 点估算分段数，限制为 1 到 12 段后均分条宽；这是视觉刻度，不保证每段始终精确代表 250 点生命。 */
	void RebuildHealthSegments(float MaxHealth);
	/** 将定义名中的下划线替换为空格并转大写，供临时界面展示；无效 ID 返回 UNIT，不查询本地化名称资产。 */
	static FString FormatDefinitionName(const FPrimaryAssetId& DefinitionId);
	/** 将已知控制标签映射为固定中文短名称、展示优先级和颜色；未知标签返回 false，输出参数不变。 */
	static bool DescribeControlTag(const FGameplayTag& Tag, FString& OutLabel, int32& OutPriority, FLinearColor& OutColor);

	/** 单条本地浮动文字的控件引用、动画时间与错位信息。 */
	struct FFloatingEntry
	{
		TWeakObjectPtr<UTextBlock> Text;
		float Age = 0.0f;
		float Lifetime = 1.15f;
		float HorizontalOffset = 0.0f;
	};

	/** 当前绑定的 Unit 与 UI 安全 View；任一失效后停止刷新。 */
	TWeakObjectPtr<ACombatUnitCharacter> BoundUnit;
	TWeakObjectPtr<UCombatUnitViewComponent> BoundView;

	/** 运行时构建的 UMG 控件树引用，只用于本地表现更新。 */
	UPROPERTY(Transient) TObjectPtr<UVerticalBox> InfoStack;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> NameText;
	UPROPERTY(Transient) TObjectPtr<USizeBox> StatusContainer;
	UPROPERTY(Transient) TObjectPtr<UProgressBar> StatusBar;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> StatusText;
	UPROPERTY(Transient) TObjectPtr<USizeBox> AbilityContainer;
	UPROPERTY(Transient) TObjectPtr<UProgressBar> AbilityBar;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> AbilityText;
	UPROPERTY(Transient) TObjectPtr<UProgressBar> HealthLagBar;
	UPROPERTY(Transient) TObjectPtr<UProgressBar> HealthBar;
	UPROPERTY(Transient) TObjectPtr<UCanvasPanel> HealthSegmentCanvas;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> HealthText;
	UPROPERTY(Transient) TObjectPtr<USizeBox> ManaContainer;
	UPROPERTY(Transient) TObjectPtr<UProgressBar> ManaBar;
	UPROPERTY(Transient) TObjectPtr<UCanvasPanel> FloatingCanvas;

	/** 当前仍在播放的可丢弃浮动文字。 */
	TArray<FFloatingEntry> FloatingEntries;
	/** 生命条即时值、拖影值和分段重建缓存。 */
	float HealthPercent = 1.0f;
	float DisplayedLagHealthPercent = 1.0f;
	float CachedMaxHealth = -1.0f;
	/** 当前控制状态与技能状态的服务器绝对时间窗。 */
	double StatusStartTime = 0.0;
	double StatusEndTime = 0.0;
	double AbilityStartTime = 0.0;
	double AbilityEndTime = 0.0;
	/** 时间窗对应的显示文本和技能引导状态。 */
	FString StatusBaseLabel;
	FString AbilityBaseLabel;
	bool bAbilityChanneling = false;
	/** 为同帧跳字提供稳定的本地错位序号。 */
	int32 FloatingSequence = 0;
};
