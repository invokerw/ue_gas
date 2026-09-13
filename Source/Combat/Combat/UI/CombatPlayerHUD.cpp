#include "Combat/UI/CombatPlayerHUD.h"
#include "Combat/UI/CombatHUDWidget.h"
#include "Combat/UI/CombatLogWidget.h"
#include "GameFramework/PlayerController.h"

void ACombatPlayerHUD::BeginPlay()
{
	Super::BeginPlay();
	APlayerController* Player = GetOwningPlayerController();
	if (GetNetMode() == NM_DedicatedServer || !Player || !Player->IsLocalController()) return;
	if (WidgetClass) CombatWidget = CreateWidget<UCombatHUDWidget>(Player, WidgetClass);
	if (LogWidgetClass) LogWidget = CreateWidget<UCombatLogWidget>(Player, LogWidgetClass);
	if (LogWidget) LogWidget->AddToPlayerScreen(20);
	if (CombatWidget)
	{
		CombatWidget->AddToPlayerScreen(10);
	}
	if (CombatWidget || LogWidget)
	{
		// 让 HUD 先消费自身区域点击，未处理的输入仍交回游戏与原 Enhanced Input 映射。
		FInputModeGameAndUI Mode;
		Mode.SetHideCursorDuringCapture(false);
		Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		Player->SetInputMode(Mode);
	}
}

void ACombatPlayerHUD::EndPlay(const EEndPlayReason::Type Reason)
{
	if (LogWidget)
	{
		LogWidget->InitializeForLog(nullptr);
		LogWidget->RemoveFromParent();
		LogWidget = nullptr;
	}
	if (CombatWidget)
	{
		CombatWidget->InitializeForUnit(nullptr);
		CombatWidget->RemoveFromParent();
		CombatWidget = nullptr;
	}
	Super::EndPlay(Reason);
}
