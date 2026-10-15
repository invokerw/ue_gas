#pragma once
#include "CoreMinimal.h"
#include "Combat/UI/CombatPlayerHUD.h"
#include "DefenseHUD.generated.h"
/** 顶部防守状态投影；仅消费 GameState，保留原战斗 HUD，不产生玩法输入。 */
UCLASS(Blueprintable)
class COMBAT_API ADefenseHUD : public ACombatPlayerHUD
{
	GENERATED_BODY()
public:
	virtual void DrawHUD() override;
};
