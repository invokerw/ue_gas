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

/** 无需 Blueprint 资产即可使用的 DOTA 风格 Unit 头顶信息 Widget。 */
UCLASS(BlueprintType)
class UE_GAS_API UCombatOverheadWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 绑定 WidgetComponent 所属 Unit 及其 UI 安全复制 View。 */
	void InitializeForUnit(ACombatUnitCharacter* InUnit);
	/** 添加一条带颜色区分的伤害或治疗浮动数字。 */
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
	/** 从安全状态标签与 Modifier 时间窗刷新控制状态条。 */
	void RefreshControlState();
	/** 从当前 Ability 时间窗刷新技能施法/引导条。 */
	void RefreshAbilityState();
	/** 施法/引导条或控制条出现时隐藏名字；两者都消失后恢复。 */
	void RefreshNameVisibility();
	/** 按最大生命值重建 DOTA 风格分段刻度。 */
	void RebuildHealthSegments(float MaxHealth);
	/** 返回 UI 显示用的 DefinitionName。 */
	static FString FormatDefinitionName(const FPrimaryAssetId& DefinitionId);
	/** 返回控制标签的本地化短名称、优先级与颜色。 */
	static bool DescribeControlTag(const FGameplayTag& Tag, FString& OutLabel, int32& OutPriority, FLinearColor& OutColor);

	struct FFloatingEntry
	{
		TWeakObjectPtr<UTextBlock> Text;
		float Age = 0.0f;
		float Lifetime = 1.15f;
		float HorizontalOffset = 0.0f;
	};

	TWeakObjectPtr<ACombatUnitCharacter> BoundUnit;
	TWeakObjectPtr<UCombatUnitViewComponent> BoundView;

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

	TArray<FFloatingEntry> FloatingEntries;
	float HealthPercent = 1.0f;
	float DisplayedLagHealthPercent = 1.0f;
	float CachedMaxHealth = -1.0f;
	double StatusStartTime = 0.0;
	double StatusEndTime = 0.0;
	double AbilityStartTime = 0.0;
	double AbilityEndTime = 0.0;
	FString StatusBaseLabel;
	FString AbilityBaseLabel;
	bool bAbilityChanneling = false;
	int32 FloatingSequence = 0;
};
