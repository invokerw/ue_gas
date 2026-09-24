// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "CombatGameMode.generated.h"

class UCombatEconomyData;
class UCombatShopData;

/**
 * 顶视角 Combat GameMode 的服务器出生编排器。
 * DefaultPawnClass 表示玩家的 Combat Unit 类；实际返回给 PlayerController Possess 的始终是无碰撞 Command Pawn。
 */
UCLASS(Blueprintable)
class ACombatGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:

	ACombatGameMode();

	/**
	 * 在服务器生成或复用主控 Unit、建立唯一 AIController/Owner 绑定，并返回 Command Pawn。
	 * 任一步失败都会清理本次新建 Actor，绝不把 Combat Unit 退化为 PlayerController Pawn。
	 */
	virtual APawn* SpawnDefaultPawnAtTransform_Implementation(
		AController* NewPlayer,
		const FTransform& SpawnTransform) override;
	/** 玩家进入时用当前关卡选择的规则初始化其连接级经济组件。 */
	virtual void PostLogin(APlayerController* NewPlayer) override;
	UCombatEconomyData* GetEconomyData() const { return EconomyData; }
	UCombatShopData* GetShopData() const { return ShopData; }
	bool AreEconomyDebugCommandsEnabled() const { return bEnableEconomyDebugCommands; }

protected:
	/** 初次出生时额外生成同类英雄并授予本玩家；重建 Command Pawn 不重复生成。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Command", meta=(ClampMin="0", ClampMax="7", DisplayName="额外初始英雄数", ToolTip="默认 0 保留单英雄；演示可设 1 体验多操，最多 7。额外英雄具有独立库存。"))
	int32 AdditionalControlledUnitCount = 0;
	/** 本关卡冻结为每个玩家规则快照的经济资产；为空时经济 fail-closed。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Economy", meta=(DisplayName="关卡经济规则", ToolTip="设置本关卡金币上限、起始金币、被动收入与出售规则。"))
	TObjectPtr<UCombatEconomyData> EconomyData;
	/** 本关卡唯一的全局 UI 商店目录；不创建场景商店 Actor。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Economy", meta=(DisplayName="关卡商店目录", ToolTip="设置本关卡基础/升级页使用的唯一商店目录。"))
	TObjectPtr<UCombatShopData> ShopData;
	/** 仅 Demo GameMode 开启；允许非 Shipping 服务器执行 combat.Debug.SetGold。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Economy", meta=(DisplayName="启用经济调试命令", ToolTip="仅 Demo 关卡应启用；其他关卡保持关闭。"))
	bool bEnableEconomyDebugCommands = false;
};



