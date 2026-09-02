// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "ue_gasPlayerController.generated.h"

class ACombatUnitCharacter;
class Aue_gasCharacter;
class UNiagaraSystem;
class UInputAction;
class UInputMappingContext;

DECLARE_LOG_CATEGORY_EXTERN(LogTemplateCharacter, Log, All);

/** 顶视角玩家控制器；Possess 无战斗碰撞的 Command Pawn，并向显式 CommandedUnit 提交服务器 Order。 */
UCLASS(Blueprintable)
class Aue_gasPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	/** 配置鼠标、Command Pawn 默认类与服务器权威绑定初值。 */
	Aue_gasPlayerController();

	/** 注册 owner-only CommandedUnit 与 BindingGeneration。 */
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** 返回当前显式主控 Combat Unit；输入不得从 GetPawn 推断该对象。 */
	UFUNCTION(BlueprintPure, Category="Combat|Command", meta=(DisplayName="获取主控战斗单位", ToolTip="返回由服务器绑定并仅复制给拥有者的主控 Combat Unit。"))
	ACombatUnitCharacter* GetCommandedUnit() const { return CommandedUnit; }

	/** 返回每次真实绑定变化都单调递增的非零代次。 */
	UFUNCTION(BlueprintPure, Category="Combat|Command", meta=(DisplayName="获取指挥绑定代次", ToolTip="用于 UI、镜头和本地异步回执淘汰旧绑定。"))
	int32 GetCommandBindingGeneration() const { return CommandBindingGeneration; }

	/** 返回 GameMode 应生成并交给本 Controller 占有的无碰撞 Command Pawn 类。 */
	TSubclassOf<Aue_gasCharacter> GetCommandPawnClass() const { return CommandPawnClass; }

	/**
	 * 服务器原子切换主控 Unit：先取消旧 Order/Owner，再建立新 Unit 的 AIController 与 owning connection。
	 * 幂等设置同一 Unit 不提升代次；失败时保持无绑定状态。
	 */
	UFUNCTION(BlueprintCallable, Category="Combat|Command", meta=(DisplayName="设置主控战斗单位", ToolTip="仅服务器调用；切换时取消旧单位命令并建立新的网络 Owner。"))
	bool SetCommandedUnitAuthority(UPARAM(DisplayName="新主控单位") ACombatUnitCharacter* NewUnit);

	/** Unit EndPlay 时清空弱生命周期边界，避免 Controller 保留已销毁 Actor。 */
	void HandleCommandedUnitEndPlay(ACombatUnitCharacter* EndingUnit);

protected:
	/**
	 * Command Pawn 占有完成后刷新本地跟随目标；异常收到 Combat Unit 时拒绝建立玩家占有。
	 * 正常出生必须由 GameMode 先建立 CommandedUnit，再只把 Command Pawn 交给 Possess。
	 */
	virtual void OnPossess(APawn* InPawn) override;

	/** Controller teardown 时取消旧 Unit Order、清除 Owner 并提升绑定代次。 */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** 初始化 Enhanced Input 映射与移动/技能输入。 */
	virtual void SetupInputComponent() override;

	/** CommandedUnit 指针复制后幂等刷新本地相机与输入就绪状态。 */
	UFUNCTION()
	void OnRep_CommandedUnit();

	/** BindingGeneration 与指针跨属性乱序到达时再次幂等刷新。 */
	UFUNCTION()
	void OnRep_CommandBindingGeneration();

	/** 点击确认移动目标时生成的可丢弃反馈特效。 */
	UPROPERTY(EditAnywhere, Category="Input", meta=(DisplayName="移动光标特效", ToolTip="只提供本地输入反馈，不参与服务器移动裁决。"))
	TObjectPtr<UNiagaraSystem> FXCursor;

	/** 按住并拖动时，两次移动 Order 之间的最短间隔。 */
	UPROPERTY(EditAnywhere, Category="Input|Movement", meta=(ClampMin="0.05", Units="s", DisplayName="移动命令刷新间隔", ToolTip="限制 Reliable Order RPC 的拖动发送频率。"))
	float MoveOrderRefreshInterval = 0.20f;

	/** 按住并拖动时，目标至少变化该距离才重发移动 Order。 */
	UPROPERTY(EditAnywhere, Category="Input|Movement", meta=(ClampMin="1.0", Units="cm", DisplayName="移动目标唤醒距离", ToolTip="目标变化达到该距离后才替换服务器移动命令。"))
	float MoveOrderWakeDistance = 25.0f;

	/** 玩家实际 Possess 的无碰撞相机 Pawn 类。 */
	UPROPERTY(EditDefaultsOnly, Category="Combat|Command", meta=(DisplayName="命令 Pawn 类", ToolTip="PlayerController 唯一 Possess 的相机 Pawn；不得包含 Combat gameplay 组件。"))
	TSubclassOf<Aue_gasCharacter> CommandPawnClass;

	/** 仅拥有者复制的显式输入、镜头与 UI 目标。 */
	UPROPERTY(ReplicatedUsing=OnRep_CommandedUnit, BlueprintReadOnly, Category="Combat|Command", meta=(DisplayName="主控战斗单位", ToolTip="服务器绑定并只复制给 owning client 的 Combat Unit。"))
	TObjectPtr<ACombatUnitCharacter> CommandedUnit;

	/** 每次真实切换、清空或失效时递增；0 只表示尚未建立过绑定。 */
	UPROPERTY(ReplicatedUsing=OnRep_CommandBindingGeneration, BlueprintReadOnly, Category="Combat|Command", meta=(DisplayName="指挥绑定代次", ToolTip="客户端用它淘汰旧 UI、镜头和本地回执。"))
	int32 CommandBindingGeneration = 0;

	/** 输入映射上下文。 */
	UPROPERTY(EditAnywhere, Category="Input", meta=(DisplayName="默认输入映射", ToolTip="Command Pawn 使用的 Enhanced Input 映射。"))
	TObjectPtr<UInputMappingContext> DefaultMappingContext;

	/** 鼠标目的地输入。 */
	UPROPERTY(EditAnywhere, Category="Input", meta=(DisplayName="鼠标目的地输入", ToolTip="点击或拖动地面时提交 MoveToPoint Order。"))
	TObjectPtr<UInputAction> SetDestinationClickAction;

	/** 触摸目的地输入。 */
	UPROPERTY(EditAnywhere, Category="Input", meta=(DisplayName="触摸目的地输入", ToolTip="触摸地面时提交 MoveToPoint Order。"))
	TObjectPtr<UInputAction> SetDestinationTouchAction;

	/** Q 技能槽输入。 */
	UPROPERTY(EditAnywhere, Category="Input|Abilities") TObjectPtr<UInputAction> AbilitySlotQAction;
	/** W 技能槽输入。 */
	UPROPERTY(EditAnywhere, Category="Input|Abilities") TObjectPtr<UInputAction> AbilitySlotWAction;
	/** E 技能槽输入。 */
	UPROPERTY(EditAnywhere, Category="Input|Abilities") TObjectPtr<UInputAction> AbilitySlotEAction;
	/** R 技能槽输入。 */
	UPROPERTY(EditAnywhere, Category="Input|Abilities") TObjectPtr<UInputAction> AbilitySlotRAction;

private:
	/** 指针或代次任一复制到达时刷新 Command Pawn 跟随目标。 */
	void RefreshCommandBinding();
	/** 提升绑定代次并跳过保留值 0。 */
	void AdvanceCommandBindingGeneration();
	/** 返回 Owner 复制也已到达、可以安全发 Unit RPC 的当前目标。 */
	ACombatUnitCharacter* GetReadyCommandedUnit() const;

	/** 鼠标/触摸移动输入入口。 */
	void OnInputStarted();
	/** 拖动过程中按频率/距离阈值更新目标。 */
	void OnSetDestinationTriggered();
	/** 松开时提交最终目标并播放本地反馈。 */
	void OnSetDestinationReleased();
	/** 触摸开始入口。 */
	void OnTouchStarted();
	/** 触摸拖动入口。 */
	void OnTouchTriggered();
	/** 触摸结束入口。 */
	void OnTouchReleased();

	/** 激活第一个技能槽。 */
	void OnAbilitySlotQ();
	/** 激活第二个技能槽。 */
	void OnAbilitySlotW();
	/** 激活第三个技能槽。 */
	void OnAbilitySlotE();
	/** 激活第四个技能槽。 */
	void OnAbilitySlotR();
	/** 把指定槽位转换为 Cast Order。 */
	void ActivateCombatAbilitySlot(int32 SlotIndex);
	/** 查询光标命中或最近的合法 Combat Unit。 */
	ACombatUnitCharacter* FindCombatUnitUnderCursor(const FVector& CursorWorldLocation) const;
	/** 向 CommandedUnit 提交替换型 MoveToPoint 批次。 */
	bool IssueCombatMoveOrder();
	/** 查询鼠标或触摸命中的有限世界位置。 */
	bool UpdateCachedDestination();

	/** 当前输入是否来自触摸。 */
	uint32 bIsTouch : 1;
	/** 当前一次点击/触摸命中的世界移动目标。 */
	FVector CachedDestination = FVector::ZeroVector;
	/** 最近一次已提交移动 Order 的目标，用于拖动节流。 */
	FVector LastIssuedMoveDestination = FVector::ZeroVector;
	/** 距离最近一次移动 Order 的累计时间。 */
	float MoveOrderRefreshElapsed = 0.0f;
	/** 当前输入手势是否获得了有效世界目标。 */
	bool bHasCachedDestination = false;
	/** 当前输入手势是否至少提交过一个移动 Order。 */
	bool bHasIssuedMoveOrder = false;
	/** PlayerController 连接维度单调递增的非零 RPC replay id。 */
	int32 NextCombatOrderRequestId = 1;
};
