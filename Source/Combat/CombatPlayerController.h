// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Combat/UI/CombatAbilityAimComponent.h"
#include "Combat/Network/CombatNetworkTypes.h"
#include "CombatPlayerController.generated.h"

class ACombatUnitCharacter;
class ACombatCharacter;
class UNiagaraSystem;
class UInputAction;
class UInputMappingContext;
class UEnhancedInputComponent;
class UCombatLogComponent;
struct FCombatOrderRequest;
struct FCombatHUDOwnerView;
struct FCombatItemView;

DECLARE_LOG_CATEGORY_EXTERN(LogTemplateCharacter, Log, All);

/**
 * 顶视角玩家的连接、输入和镜头入口。
 * Controller 只 Possess 无战斗碰撞的 Command Pawn，通过 owner-only CommandedUnit 绑定选择业务单位；移动和技能输入最终提交给服务器 Order/RPC 校验链，不直接驱动 Unit Transform 或战斗结算。
 */
UCLASS(Blueprintable)
class ACombatPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ACombatPlayerController();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	/** 引擎因视口失焦冲刷按键时废弃旧瞄准与手势，防止恢复焦点后误确认。 */
	virtual void FlushPressedKeys() override;

	/** 返回当前显式主控 Combat Unit；输入不得从 GetPawn 推断该对象。 */
	UFUNCTION(BlueprintPure, Category="Combat|Command", meta=(DisplayName="获取主控战斗单位", ToolTip="返回由服务器绑定并仅复制给拥有者的主控 Combat Unit。"))
	ACombatUnitCharacter* GetCommandedUnit() const { return CommandedUnit; }

	/** 返回每次真实绑定变化都单调递增的非零代次。 */
	UFUNCTION(BlueprintPure, Category="Combat|Command", meta=(DisplayName="获取指挥绑定代次", ToolTip="用于 UI、镜头和本地异步回执淘汰旧绑定。"))
	int32 GetCommandBindingGeneration() const { return CommandBindingGeneration; }

	/** 返回 GameMode 应生成并交给本 Controller 占有的无碰撞 Command Pawn 类。 */
	TSubclassOf<ACombatCharacter> GetCommandPawnClass() const { return CommandPawnClass; }

	/**
	 * 服务器原子切换主控 Unit：先取消旧 Order/Owner，再建立新 Unit 的 AIController 与 owning connection。
	 * 幂等设置同一 Unit 不提升代次；失败时保持无绑定状态。
	 */
	UFUNCTION(BlueprintCallable, Category="Combat|Command", meta=(DisplayName="设置主控战斗单位", ToolTip="仅服务器调用；切换时取消旧单位命令并建立新的网络 Owner。"))
	bool SetCommandedUnitAuthority(UPARAM(DisplayName="新主控单位") ACombatUnitCharacter* NewUnit);

	/** Unit EndPlay 时清空弱生命周期边界，避免 Controller 保留已销毁 Actor。 */
	void HandleCommandedUnitEndPlay(ACombatUnitCharacter* EndingUnit);

	UCombatAbilityAimComponent* GetAbilityAimComponent() const { return AbilityAimComponent; }
	ECombatAbilityCastMode GetAbilityCastMode() const { return AbilityCastMode; }
	/** 使用实际 Slate 几何检测本玩家 HUD/日志区域，不把全屏根控件当成 UI 阻挡。 */
	bool IsPointerOverCombatUI() const;
	/** HUD 消费 Escape 或应用失焦时也能取消本地意图，不发送 Stop。 */
	void CancelCombatTargeting();
	/** 使用一个精确物品快照；HUD 与快捷键都只经统一 Order 入口提交。 */
	bool UseInventoryItem(int32 Slot, const FCombatItemView& Expected);
	/** 请求交换快照中的两槽；不会替换服务器当前指令。 */
	bool SwapInventoryItems(int32 From, int32 To, const FCombatHUDOwnerView& Expected);
	/** 选择丢弃落点或把拖拽的精确实例投递到当前鼠标所指地面。 */
	void BeginDropInventoryItem(const FCombatItemView& Expected);
	bool DropInventoryItemAtCursor(const FCombatItemView& Expected);
	/** 拖放使用释放事件的屏幕位置，避免拖拽期间视口鼠标查询失效；仍通过服务器地面/导航复核。 */
	bool DropInventoryItemAtScreenPosition(const FCombatItemView& Expected, const FVector2D& ScreenPosition);
	bool IsChoosingItemDrop() const { return PendingDropItem.IsValid(); }
	/** 从 Enhanced Input 当前映射取得物品热键，不在 HUD 写死物理键。 */
	FText GetItemHotkeyText(int32 Slot) const;
	/** 拾取与放置最终结果，仅用于本地 HUD。 */
	FText GetItemStatusText() const;

protected:
	/**
	 * Command Pawn 占有完成后刷新本地跟随目标；异常收到 Combat Unit 时拒绝建立玩家占有。
	 * 正常出生必须由 GameMode 先建立 CommandedUnit，再只把 Command Pawn 交给 Possess。
	 */
	virtual void OnPossess(APawn* InPawn) override;

	/** Controller teardown 时取消旧 Unit Order、清除 Owner 并提升绑定代次。 */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** 初始化 Enhanced Input 映射与移动、普攻、停止及技能输入。 */
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
	TSubclassOf<ACombatCharacter> CommandPawnClass;

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
	UPROPERTY(EditAnywhere, Category="Input", meta=(DisplayName="鼠标目的地输入", ToolTip="点击敌方单位时提交持续普攻；点击或拖动地面时提交移动指令。"))
	TObjectPtr<UInputAction> SetDestinationClickAction;

	/** 触摸目的地输入。 */
	UPROPERTY(EditAnywhere, Category="Input", meta=(DisplayName="触摸目的地输入", ToolTip="触摸地面时提交 MoveToPoint Order。"))
	TObjectPtr<UInputAction> SetDestinationTouchAction;

	/** 进入普攻选敌模式的输入；具体按键由映射上下文决定，默认 A。 */
	UPROPERTY(EditAnywhere, Category="Input|Combat", meta=(DisplayName="普攻选敌输入", ToolTip="开始选择普通攻击目标；在输入映射中配置按键，Demo 默认 A。为空时禁用此输入。"))
	TObjectPtr<UInputAction> AttackTargetAction;

	/** 共享普攻选敌与标准技能瞄准的确认输入，默认鼠标左键。 */
	UPROPERTY(EditAnywhere, Category="Input|Combat", meta=(DisplayName="确认战斗目标输入", ToolTip="在普攻选敌或技能瞄准模式中确认实际命中，Demo 默认鼠标左键；UI 上不提交。为空时禁用。"))
	TObjectPtr<UInputAction> ConfirmAttackTargetAction;

	/** 取消本地选敌模式的输入，不停止正在执行的服务器命令，默认 Escape。 */
	UPROPERTY(EditAnywhere, Category="Input|Combat", meta=(DisplayName="取消战斗选敌输入", ToolTip="退出本地普攻选敌或技能瞄准，不停止单位当前命令，Demo 默认 Escape。为空时禁用此输入。"))
	TObjectPtr<UInputAction> CancelAttackTargetAction;

	/** 提交服务器停止命令的输入，默认 S。 */
	UPROPERTY(EditAnywhere, Category="Input|Combat", meta=(DisplayName="停止命令输入", ToolTip="取消本地选敌和拖动，并向主控单位提交停止命令，Demo 默认 S。为空时禁用此输入。"))
	TObjectPtr<UInputAction> StopCommandAction;

	/** 全部目标技能共用的本地确认方式；无目标与 AutoCast 仍按下立即处理。 */
	UPROPERTY(EditAnywhere, Category="Input|Abilities", meta=(DisplayName="技能施法方式", ToolTip="标准：按下瞄准、左键确认；按下快施：按下确认；松开快施：松开同一技能键确认。取消不会停止服务器命令。"))
	ECombatAbilityCastMode AbilityCastMode = ECombatAbilityCastMode::Standard;

	/** Q 技能槽输入。 */
	UPROPERTY(EditAnywhere, Category="Input|Abilities") TObjectPtr<UInputAction> AbilitySlotQAction;
	/** W 技能槽输入。 */
	UPROPERTY(EditAnywhere, Category="Input|Abilities") TObjectPtr<UInputAction> AbilitySlotWAction;
	/** E 技能槽输入。 */
	UPROPERTY(EditAnywhere, Category="Input|Abilities") TObjectPtr<UInputAction> AbilitySlotEAction;
	/** R 技能槽输入。 */
	UPROPERTY(EditAnywhere, Category="Input|Abilities") TObjectPtr<UInputAction> AbilitySlotRAction;
	UPROPERTY(EditAnywhere, Category="Input|Items", meta=(DisplayName="物品快捷键动作", ToolTip="六个装备槽的 Enhanced Input Action，默认映射为数字 1 到 6，可在映射上下文中修改。")) TArray<TObjectPtr<UInputAction>> ItemSlotActions;

private:
	friend class ACombatAbilityAimScenarioActor;
	/** 本地意图适配默认子对象，Dedicated 禁用 Tick 且不生成视觉 Actor。 */
	UPROPERTY(VisibleAnywhere, Category="Combat|Indicator", meta=(DisplayName="技能瞄准", ToolTip="只在本地玩家上更新的技能指示器会话。"))
	TObjectPtr<UCombatAbilityAimComponent> AbilityAimComponent;
	/** 默认子对象记录本连接可见事件；生命周期跟随 Controller，HUD 关闭不停止记录。 */
	UPROPERTY(VisibleAnywhere, Category="Combat|Log", meta=(DisplayName="战斗记录组件", ToolTip="服务器生成并仅向本连接复制的只读战斗历史。"))
	TObjectPtr<UCombatLogComponent> CombatLogComponent;
#if WITH_DEV_AUTOMATION_TESTS
	friend class FCombatPlayerAttackInputTest;
	friend class FCombatPlayerAttackInputCancellationTest;
	friend class FCombatPlayerAutoCastAbilityInputTest;
	friend class FCombatAbilityAimInputTest;
	friend class FCombatAbilityUnitAimTest;
#endif

	/** 指针或代次任一复制到达时刷新 Command Pawn 跟随目标。 */
	void RefreshCommandBinding();
	/** 提升绑定代次并跳过保留值 0。 */
	void AdvanceCommandBindingGeneration();
	/** 返回 Owner 复制也已到达、可以安全发 Unit RPC 的当前目标。 */
	ACombatUnitCharacter* GetReadyCommandedUnit() const;

	/** 为已配置的普攻与停止 Action 绑定 Started 事件；不绑定物理按键，空引用跳过。 */
	void BindCombatCommandActions(UEnhancedInputComponent& EnhancedInputComponent);
	/** 鼠标/触摸输入入口；鼠标命中合法敌人时优先提交普攻。 */
	void OnInputStarted();
	/** 按本次实际命中选择普攻或移动；普攻不启用后续拖动移动。 */
	void BeginDestinationInput(const FHitResult& Hit);
	/** 清除当前拖动手势，防止后续 Triggered/Released 覆盖新的攻击、技能或停止命令。 */
	void ResetDestinationInput();
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
	/** 选敌输入只改变本地光标，确认目标后才发送普攻。 */
	void OnAttackTargetingStarted();
	/** 确认输入在选敌模式下读取实际命中；未选敌时不提交命令。 */
	void OnAttackTargetConfirmed();
	/** 确认实际命中的敌方单位；无效目标保留选敌模式，不退化为移动或自动选附近单位。 */
	void ConfirmAttackTarget(const FHitResult& Hit);
	/** 退出本地选敌模式并恢复默认光标；不取消服务器当前命令。 */
	void CancelAttackTargeting();
	/** 停止输入清除本地手势并通过统一 RPC 提交 Stop。 */
	void OnStopCommand();
	/** 按公共目标规则预选敌人并提交 AttackTarget；距离与 LOS 留给服务器执行/追击时复核。返回值仅表示请求已发送。 */
	bool IssueCombatAttackOrder(ACombatUnitCharacter* Target);
	/** 向就绪主控单位提交单条替换型命令，共用连接维度 RequestId；不表示服务器已接受。 */
	bool SubmitCombatOrder(const FCombatOrderRequest& Order);

	/** 激活第一个技能槽。 */
	void OnAbilitySlotQ();
	/** 激活第二个技能槽。 */
	void OnAbilitySlotW();
	/** 激活第三个技能槽。 */
	void OnAbilitySlotE();
	/** 激活第四个技能槽。 */
	void OnAbilitySlotR();
	/** 开始本地技能瞄准或处理无目标/AutoCast 的立即输入。 */
	void ActivateCombatAbilitySlot(int32 SlotIndex);
	/** 同一技能的 Completed 才能完成松开快施；旧会话释放无效。 */
	void OnAbilitySlotReleased(int32 SlotIndex);
	/** Enhanced Input 的 Canceled 只取消，永远不按 Completed 提交。 */
	void OnAbilityInputCanceled(int32 SlotIndex);
	/** 六个物品动作的 Started/Completed 适配，与技能共用当前施法模式。 */
	void OnItemSlotPressed(int32 Slot);
	void OnItemSlotReleased(int32 Slot);
	void OnItemInputCanceled(int32 Slot);
	uint64 ItemPressSerials[6] = {};
	FCombatItemHandle PendingDropItem;
	/** 请求提交前登记，兼容 listen server 同栈先收到最终结果。 */
	void TrackItemRequest(int32 RequestId, const FCombatOrderRequest& Order);
	UFUNCTION() void HandleItemBatchResult(FCombatOrderBatchResult Result);
	UFUNCTION() void HandleItemFinalResult(FCombatOrderResult Result);
	TWeakObjectPtr<ACombatUnitCharacter> ItemFeedbackUnit;
	FCombatItemHandle FeedbackItem;
	int32 ItemFeedbackRequest = 0;
	int32 ItemFeedbackBinding = 0;
	uint32 ItemFeedbackLife = 0;
	bool bItemFeedbackFinal = false;
	FText ItemFeedbackText;
	double ItemFeedbackUntil = 0;
	int32 PendingDropRevision = 0;
	int32 PendingDropBinding = 0;
	uint32 PendingDropLife = 0;
	/** 用当前实际命中和会话号尝试确认一次；保持唯一 SubmitCombatOrder 入口。 */
	void ConfirmAbilityTarget(const FHitResult& Hit, uint64 Serial);
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
	/** 只有从移动命中开始的有效手势可以继续发送拖动/松开移动。 */
	bool bDestinationInputActive = false;
	/** 本地 A 键选敌模式；确认、取消、其他命令或控制绑定刷新时清除，不复制。 */
	bool bAttackTargeting = false;
	/** PlayerController 连接维度单调递增的非零 RPC replay id。 */
	int32 NextCombatOrderRequestId = 1;
	/** 每个物理按住手势记录开始时的会话号，取消/换技能后不能复活旧目标。 */
	uint64 AbilityPressSerials[4] = {};
};
