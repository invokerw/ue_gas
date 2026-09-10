#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "CombatPlayerHUD.generated.h"

class UCombatHUDWidget;

/** 本地玩家 HUD owner：创建并持有底部蓝图界面，EndPlay 时移除；专用服务器不创建 UMG。 */
UCLASS(Blueprintable, meta=(DisplayName="战斗玩家 HUD", ToolTip="本地创建底部 HUD 蓝图，由 GameMode 的 HUDClass 指定。"))
class UE_GAS_API ACombatPlayerHUD : public AHUD
{
	GENERATED_BODY()
public:
	/** 返回已创建的本地界面；无本地玩家或未配置时为空。 */
	UCombatHUDWidget* GetCombatWidget() const { return CombatWidget; }
protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	UPROPERTY(EditDefaultsOnly, Category="Combat|HUD", meta=(DisplayName="底部 HUD 蓝图", ToolTip="本地玩家创建的 Widget Blueprint；为空时禁用底部 HUD。"))
	TSubclassOf<UCombatHUDWidget> WidgetClass;
private:
	UPROPERTY(Transient) TObjectPtr<UCombatHUDWidget> CombatWidget;
};
