#include "Combat/UI/CombatPlayerHUD.h"
#include "Combat/UI/CombatHUDWidget.h"
#include "Combat/UI/CombatLogWidget.h"
#include "Combat/UI/CombatShopWidget.h"
#include "GameFramework/PlayerController.h"
#include "CombatPlayerController.h"
#include "Combat/Unit/CombatUnitCharacter.h"
#include "Components/CapsuleComponent.h"

void ACombatPlayerHUD::DrawHUD()
{
	Super::DrawHUD();
	auto* Player = Cast<ACombatPlayerController>(GetOwningPlayerController());
	if (!Player || !Player->IsLocalController() || !Canvas) return;
	FVector2D Start, End;
	if (Player->GetSelectionRectangle(Start, End))
	{
		const float X = FMath::Min(Start.X, End.X), Y = FMath::Min(Start.Y, End.Y);
		const float W = FMath::Abs(End.X - Start.X), H = FMath::Abs(End.Y - Start.Y);
		DrawRect(FLinearColor(0.15f, 0.85f, 0.4f, 0.12f), X, Y, W, H);
		const FLinearColor Color(0.2f, 1.0f, 0.5f);
		DrawLine(X, Y, X + W, Y, Color, 1.5f);
		DrawLine(X + W, Y, X + W, Y + H, Color, 1.5f);
		DrawLine(X + W, Y + H, X, Y + H, Color, 1.5f);
		DrawLine(X, Y + H, X, Y, Color, 1.5f);
	}
	auto Units = Player->GetSelectedUnits();
	if (Units.IsEmpty() && IsValid(Player->GetInspectedUnit())) Units.Add(Player->GetInspectedUnit());
	for (const auto* Unit : Units)
	{
		if (!IsValid(Unit) || Unit->IsHidden()) continue;
		const bool bPrimary = Unit == Player->GetInspectedUnit();
		const FLinearColor Color = !Player->CanControlUnit(Unit) ? FLinearColor(1, 0.65f, 0.2f)
			: bPrimary ? FLinearColor(0.3f, 1, 0.45f) : FLinearColor(0.2f, 0.6f, 1);
		const float Radius = Unit->GetCapsuleComponent()->GetScaledCapsuleRadius() + 10.0f;
		const FVector Center = Unit->GetActorLocation() - FVector(0, 0, Unit->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() - 3.0f);
		for (int32 Index = 0; Index < 32; ++Index)
		{
			FVector2D A, B;
			const float Angle = Index * 2.0f * PI / 32.0f, Next = (Index + 1) * 2.0f * PI / 32.0f;
			if (Player->ProjectWorldLocationToScreen(Center + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0) * Radius, A)
				&& Player->ProjectWorldLocationToScreen(Center + FVector(FMath::Cos(Next), FMath::Sin(Next), 0) * Radius, B))
				DrawLine(A.X, A.Y, B.X, B.Y, Color, bPrimary ? 2.5f : 1.5f);
		}
	}
}

ACombatPlayerHUD::ACombatPlayerHUD()
{
	ShopWidgetClass = UCombatShopWidget::StaticClass();
}

void ACombatPlayerHUD::BeginPlay()
{
	Super::BeginPlay();
	APlayerController* Player = GetOwningPlayerController();
	if (GetNetMode() == NM_DedicatedServer || !Player || !Player->IsLocalController()) return;
	if (WidgetClass) CombatWidget = CreateWidget<UCombatHUDWidget>(Player, WidgetClass);
	if (LogWidgetClass) LogWidget = CreateWidget<UCombatLogWidget>(Player, LogWidgetClass);
	if (ShopWidgetClass) ShopWidget = CreateWidget<UCombatShopWidget>(Player, ShopWidgetClass);
	if (LogWidget) LogWidget->AddToPlayerScreen(20);
	if (ShopWidget) ShopWidget->AddToPlayerScreen(15);
	if (CombatWidget)
	{
		CombatWidget->AddToPlayerScreen(10);
	}
	if (CombatWidget || LogWidget || ShopWidget)
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
	if (ShopWidget)
	{
		ShopWidget->RemoveFromParent();
		ShopWidget = nullptr;
	}
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
