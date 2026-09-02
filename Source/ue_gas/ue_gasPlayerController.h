// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
//#include "Templates/SubclassOf.h"
#include "GameFramework/PlayerController.h"
#include "ue_gasPlayerController.generated.h"

class UNiagaraSystem;
class UInputMappingContext;
class UInputAction;
class UPathFollowingComponent;
class ACombatUnitCharacter;

DECLARE_LOG_CATEGORY_EXTERN(LogTemplateCharacter, Log, All);

/** 顶视角玩家控制器；把点击移动与技能输入统一转换为服务器权威 Combat Order。 */
UCLASS(abstract)
class Aue_gasPlayerController : public APlayerController
{
	GENERATED_BODY()

protected:

	/** 直接占有 Combat Unit 时，供服务器 Order 观察与 owning client AutonomousProxy 执行使用的寻路组件。 */
	UPROPERTY(VisibleDefaultsOnly, Category = AI)
	TObjectPtr<UPathFollowingComponent> PathFollowingComponent;

	/** 点击确认移动目标时生成的反馈特效。 */
	UPROPERTY(EditAnywhere, Category="Input")
	TObjectPtr<UNiagaraSystem> FXCursor;

	/** 按住并拖动时，两次移动 Order 之间的最短间隔。 */
	UPROPERTY(EditAnywhere, Category="Input|Movement", meta=(ClampMin="0.05", Units="s"))
	float MoveOrderRefreshInterval = 0.20f;

	/** 按住并拖动时，目标至少变化该距离才重发移动 Order。 */
	UPROPERTY(EditAnywhere, Category="Input|Movement", meta=(ClampMin="1.0", Units="cm"))
	float MoveOrderWakeDistance = 25.0f;

	/** MappingContext */
	UPROPERTY(EditAnywhere, Category="Input")
	TObjectPtr<UInputMappingContext> DefaultMappingContext;
	
	/** Jump Input Action */
	UPROPERTY(EditAnywhere, Category="Input")
	TObjectPtr<UInputAction> SetDestinationClickAction;

	/** Jump Input Action */
	UPROPERTY(EditAnywhere, Category="Input")
	TObjectPtr<UInputAction> SetDestinationTouchAction;

	/** Combat ability slot input actions. */
	UPROPERTY(EditAnywhere, Category="Input|Abilities")
	TObjectPtr<UInputAction> AbilitySlotQAction;

	UPROPERTY(EditAnywhere, Category="Input|Abilities")
	TObjectPtr<UInputAction> AbilitySlotWAction;

	UPROPERTY(EditAnywhere, Category="Input|Abilities")
	TObjectPtr<UInputAction> AbilitySlotEAction;

	UPROPERTY(EditAnywhere, Category="Input|Abilities")
	TObjectPtr<UInputAction> AbilitySlotRAction;

	/** 当前输入是否来自触摸。 */
	uint32 bIsTouch : 1;

	/** 当前一次点击/触摸命中的世界移动目标。 */
	FVector CachedDestination;

	/** 最近一次已提交移动 Order 的目标，用于拖动节流。 */
	FVector LastIssuedMoveDestination = FVector::ZeroVector;

	/** 距离最近一次移动 Order 的累计时间。 */
	float MoveOrderRefreshElapsed = 0.0f;

	/** 当前输入手势是否获得了有效世界目标。 */
	bool bHasCachedDestination = false;

	/** 当前输入手势是否至少提交过一个移动 Order。 */
	bool bHasIssuedMoveOrder = false;

public:

	/** 创建直接占有单位所需的 PathFollowingComponent，并配置鼠标。 */
	Aue_gasPlayerController();

	/**
	 * 服务器已接受移动 Order 并完成求路后，把路径下发给拥有该 Pawn 的客户端执行。
	 * 客户端 CharacterMovement 仍走 UE 自带预测/ServerMove 校验；Order 状态与完成裁决保留在服务器。
	 */
	UFUNCTION(Client, Reliable)
	void ClientFollowCombatOrderPath(
		const TArray<FVector_NetQuantize10>& PathPoints,
		FVector_NetQuantize10 GoalLocation,
		float AcceptanceRadius);

	/** 服务器取消或替换移动 Order 时，停止拥有客户端上的对应路径跟随。 */
	UFUNCTION(Client, Reliable)
	void ClientStopCombatOrderNavigation();

protected:

	/** Initialize input bindings */
	virtual void SetupInputComponent() override;
	
	/** 鼠标/触摸移动输入入口。 */
	void OnInputStarted();
	void OnSetDestinationTriggered();
	void OnSetDestinationReleased();
	void OnTouchStarted();
	void OnTouchTriggered();
	void OnTouchReleased();

	/** Activate the first four granted, non-passive combat abilities with Q/W/E/R. */
	void OnAbilitySlotQ();
	void OnAbilitySlotW();
	void OnAbilitySlotE();
	void OnAbilitySlotR();
	void ActivateCombatAbilitySlot(int32 SlotIndex);
	ACombatUnitCharacter* FindCombatUnitUnderCursor(const FVector& CursorWorldLocation) const;

	/** 向当前占有的 Combat Unit 提交一个替换型 MoveToPoint 批次。 */
	bool IssueCombatMoveOrder();

	/** 查询鼠标或触摸命中的世界位置；命中时更新 CachedDestination。 */
	bool UpdateCachedDestination();

	/** Monotonic, non-zero request id used by the combat order RPC replay guard. */
	int32 NextCombatOrderRequestId = 1;
};


