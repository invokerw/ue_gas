#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "CombatPlayerHUD.generated.h"

class UCombatHUDWidget;
class UCombatLogWidget;

/** 本地玩家 HUD owner：持有底部界面和左上角战斗记录，EndPlay 时移除；专用服务器不创建 UMG。 */
UCLASS(Blueprintable, meta=(DisplayName="战斗玩家 HUD", ToolTip="本地创建底部 HUD 蓝图，由 GameMode 的 HUDClass 指定。"))
class COMBAT_API ACombatPlayerHUD : public AHUD
{
	GENERATED_BODY()
public:
	/** 返回已创建的本地界面；无本地玩家或未配置时为空。 */
	UCombatHUDWidget* GetCombatWidget() const { return CombatWidget; }
	/** 返回左上角记录入口；未配置蓝图或非本地玩家时为空。 */
	UCombatLogWidget* GetLogWidget() const { return LogWidget; }
protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	UPROPERTY(EditDefaultsOnly, Category="Combat|HUD", meta=(DisplayName="底部 HUD 蓝图", ToolTip="本地玩家创建的 Widget Blueprint；为空时禁用底部 HUD。"))
	TSubclassOf<UCombatHUDWidget> WidgetClass;
	UPROPERTY(EditDefaultsOnly, Category="Combat|HUD", meta=(DisplayName="战斗记录蓝图", ToolTip="左上角入口与日志窗口的 Widget Blueprint；为空时禁用入口，组件仍记录历史。"))
	TSubclassOf<UCombatLogWidget> LogWidgetClass;
private:
	UPROPERTY(Transient) TObjectPtr<UCombatHUDWidget> CombatWidget;
	UPROPERTY(Transient) TObjectPtr<UCombatLogWidget> LogWidget;
};
