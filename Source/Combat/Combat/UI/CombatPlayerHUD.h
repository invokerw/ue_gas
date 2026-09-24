#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "CombatPlayerHUD.generated.h"

class UCombatHUDWidget;
class UCombatLogWidget;
class UCombatShopWidget;

/** 本地玩家 HUD owner：持有底部界面、战斗记录和全局商店；专用服务器不创建 UMG。 */
UCLASS(Blueprintable, meta=(DisplayName="战斗玩家 HUD", ToolTip="本地创建底部 HUD 蓝图，由 GameMode 的 HUDClass 指定。"))
class COMBAT_API ACombatPlayerHUD : public AHUD
{
	GENERATED_BODY()
#if WITH_DEV_AUTOMATION_TESTS
	friend class FCombatShopWidgetStructureTest;
#endif
public:
	ACombatPlayerHUD();
	/** 绘制本地选中轮廓与拖框，仅消费 Controller 的选择，不改变单位状态。 */
	virtual void DrawHUD() override;
	/** 返回已创建的本地界面；无本地玩家或未配置时为空。 */
	UCombatHUDWidget* GetCombatWidget() const { return CombatWidget; }
	/** 返回左上角记录入口；未配置蓝图或非本地玩家时为空。 */
	UCombatLogWidget* GetLogWidget() const { return LogWidget; }
	/** 返回独立全局商店；初始关闭，未配置或非本地玩家时为空。 */
	UCombatShopWidget* GetShopWidget() const { return ShopWidget; }
protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	UPROPERTY(EditDefaultsOnly, Category="Combat|HUD", meta=(DisplayName="底部 HUD 蓝图", ToolTip="本地玩家创建的 Widget Blueprint；为空时禁用底部 HUD。"))
	TSubclassOf<UCombatHUDWidget> WidgetClass;
	UPROPERTY(EditDefaultsOnly, Category="Combat|HUD", meta=(DisplayName="战斗记录蓝图", ToolTip="左上角入口与日志窗口的 Widget Blueprint；为空时禁用入口，组件仍记录历史。"))
	TSubclassOf<UCombatLogWidget> LogWidgetClass;
	UPROPERTY(EditDefaultsOnly, Category="Combat|HUD", meta=(DisplayName="商店界面类", ToolTip="默认使用原生简洁商店；可由蓝图子类替换外观。"))
	TSubclassOf<UCombatShopWidget> ShopWidgetClass;
private:
	UPROPERTY(Transient) TObjectPtr<UCombatHUDWidget> CombatWidget;
	UPROPERTY(Transient) TObjectPtr<UCombatLogWidget> LogWidget;
	UPROPERTY(Transient) TObjectPtr<UCombatShopWidget> ShopWidget;
};
