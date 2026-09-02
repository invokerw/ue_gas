// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "ue_gasGameMode.generated.h"

/**
 * 顶视角 Combat GameMode 的服务器出生编排器。
 * DefaultPawnClass 表示玩家的 Combat Unit 类；实际返回给 PlayerController Possess 的始终是无碰撞 Command Pawn。
 */
UCLASS(Blueprintable)
class Aue_gasGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:

	/** 配置 Combat PlayerController 与玩家默认 Unit 类。 */
	Aue_gasGameMode();

	/**
	 * 在服务器生成或复用主控 Unit、建立唯一 AIController/Owner 绑定，并返回 Command Pawn。
	 * 任一步失败都会清理本次新建 Actor，绝不把 Combat Unit 退化为 PlayerController Pawn。
	 */
	virtual APawn* SpawnDefaultPawnAtTransform_Implementation(
		AController* NewPlayer,
		const FTransform& SpawnTransform) override;
};



