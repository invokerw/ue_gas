#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Combat/Defense/DefenseAugmentData.h"
#include "Combat/Defense/DefenseTypes.h"
#include "DefenseBuildWidget.generated.h"
class SBorder;
class SButton;
class SVerticalBox;
class SEditableTextBox;
class UDefenseChoiceComponent;
class UDefenseEventComponent;
/** 可玩的个人强化面板；只消费复制快照，按钮只发送意图，Tick 仅刷新表现。 */
UCLASS(meta=(DisplayName="防守构筑面板", ToolTip="显示初始/波后三选一、重抽费用、已选强化和开发开始入口。"))
class COMBAT_API UDefenseBuildWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	UDefenseBuildWidget(const FObjectInitializer& Init);
	/** 使用实际可见面板几何阻断命令和边缘滚屏穿透。 */
	bool IsScreenPositionOverUI(FVector2D Position) const;
protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeTick(const FGeometry& Geometry,float Delta) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& Geometry,const FPointerEvent& Event) override;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;
private:
#if WITH_DEV_AUTOMATION_TESTS
	friend class FDefenseChoiceWidgetTest;
	friend class FDefenseChoiceUIProbe;
	friend class FDefenseEventPIEProbe;
	friend class FDefenseGuardianPIEProbe;
	friend class FDefenseTeamShopUIProbe;
#endif
	/** 解析拥有者组件；没有本地玩家时返回空。 */
	UDefenseChoiceComponent* GetChoice() const;
	/** 按当前快照刷新按钮内容；不生成候选/扣费。 */
	void RefreshContents();
	/** 开局英雄入口仅消费拥有者资格；按钮只发送固定守卫选择意图。 */
	void AddHeroPicker();
	/** 显示服务器截止减校准服务器时间，不能驱动默认选择。 */
	FText CountdownText() const;
	/** 本地开发服务器锁种子/开始；客户端不会调用服务器种子 API。 */
	FReply StartRun();
	/** 把服务器失败原因翻译为可读提示。 */
	static FText ExplainFailure(FName Reason);
	/** 事件按钮只发送安全、风险或取消意图，不直接改写对局。 */
	void RefreshEventPanel();
	TSharedPtr<SBorder> ChoicePanel,OwnedPanel,EventPanel;
	TSharedPtr<SButton> StartButton,RerollButton,GuardianButton;
	/** 当前事件的真实 Slate 按钮；每次视图重建时替换，teardown 释放。 */
	TSharedPtr<SButton> EventSafeButton,EventRiskButton,EventCancelButton;
	TArray<TSharedPtr<SButton>> CardButtons;
	TSharedPtr<SVerticalBox> ChoiceBody,OwnedBody,EventBody;
	TSharedPtr<SEditableTextBox> SeedInput;
	int32 SeenRevision=INDEX_NONE,SeenHeroRevision=INDEX_NONE,SeenEventRevision=INDEX_NONE;
	/** 事件编号和阶段也触发刷新；不同事件的局部修订可能相同。 */
	FGuid SeenEventId;
	EDefensePhase SeenPhase=EDefensePhase::Preparation;
	bool bSeenWaiting=false;
	FText LocalError;
};
