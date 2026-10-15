#include "Combat/Defense/DefenseHUD.h"
#include "Combat/Defense/DefenseGameState.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
void ADefenseHUD::DrawHUD()
{
	Super::DrawHUD();
	const auto* State = GetWorld()->GetGameState<ADefenseGameState>();
	if (!State || !Canvas) return;
	const auto& V = State->GetRunView();
	const FString Phase = StaticEnum<EDefensePhase>()->GetDisplayNameTextByValue(int64(V.Phase)).ToString();
	const int32 Remaining = FMath::Max(0, FMath::CeilToInt(V.Deadline - State->GetServerWorldTimeSeconds()));
	const float X = FMath::Max(8.0f, (Canvas->SizeX - 580.0f) * 0.5f);
	DrawRect(FLinearColor(0.02f,0.03f,0.06f,0.9f), X, 16, 580, 88);
	DrawText(FString::Printf(TEXT("守护神殿   %s   波次 %d / %d   %d 秒"), *Phase,
		FMath::Max(1,V.WaveIndex+1), V.TotalWaves, Remaining), FLinearColor::White, X+16, 24, GEngine->GetMediumFont());
	DrawRect(FLinearColor(0.12f,0.13f,0.16f,1), X+16, 53, 548, 12);
	DrawRect(FLinearColor(0.1f,0.65f,0.6f,1), X+16, 53,
		548*FMath::Clamp(V.ObjectiveHealth/FMath::Max(1.0f,V.ObjectiveMaxHealth),0.0f,1.0f),12);
	DrawText(FString::Printf(TEXT("核心 %.0f / %.0f    敌人 %d    待出生 %d    %s"), V.ObjectiveHealth,
		V.ObjectiveMaxHealth,V.RemainingEnemies,V.PendingSpawns, V.TerminalReason.IsNone()?TEXT(""):*V.TerminalReason.ToString()),
		FLinearColor::White,X+16,72,GEngine->GetSmallFont());
}
