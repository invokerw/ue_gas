#include "Combat/UI/CombatPlayerHUD.h"
#include "Combat/UI/CombatHUDWidget.h"
#include "GameFramework/PlayerController.h"

void ACombatPlayerHUD::BeginPlay()
{
	Super::BeginPlay();
	APlayerController* Player = GetOwningPlayerController();
	if (GetNetMode() == NM_DedicatedServer || !Player || !Player->IsLocalController() || !WidgetClass) return;
	CombatWidget = CreateWidget<UCombatHUDWidget>(Player, WidgetClass);
	if (CombatWidget)
	{
		CombatWidget->AddToPlayerScreen(10);
		// 让 HUD 先消费自身区域点击，未处理的输入仍交回游戏与原 Enhanced Input 映射。
		FInputModeGameAndUI Mode;
		Mode.SetHideCursorDuringCapture(false);
		Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		Player->SetInputMode(Mode);
	}
}

void ACombatPlayerHUD::EndPlay(const EEndPlayReason::Type Reason)
{
	if (CombatWidget)
	{
		CombatWidget->InitializeForUnit(nullptr);
		CombatWidget->RemoveFromParent();
		CombatWidget = nullptr;
	}
	Super::EndPlay(Reason);
}
