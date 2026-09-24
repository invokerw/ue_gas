// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Combat/UI/CombatAbilityAimComponent.h"
#include "Combat/Network/CombatNetworkTypes.h"
#include "Combat/Economy/CombatEconomyTypes.h"
#include "CombatPlayerController.generated.h"

class ACombatUnitCharacter;
class ACombatCharacter;
class UNiagaraSystem;
class UInputAction;
class UInputMappingContext;
class UEnhancedInputComponent;
class UCombatLogComponent;
class UCombatEconomyComponent;
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

	/** 选择只改变本地 HUD/命令目标；无权单位仅供查看，Shift 只增删已有控制权的成员。 */
	UFUNCTION(BlueprintCallable, Category="Combat|Selection", meta=(DisplayName="选择战斗单位", ToolTip="本地点击查看；追加模式切换有控制权的组成员，不会授予控制权。"))
	void SelectCombatUnit(UPARAM(DisplayName="单位") ACombatUnitCharacter* Unit, UPARAM(DisplayName="切换组成员") bool bToggle = false);
	/** 返回本地查看对象；尚未建立选择时回退初始主控，不授予任何控制许可。 */
	UFUNCTION(BlueprintPure, Category="Combat|Selection", meta=(DisplayName="获取查看单位", ToolTip="本地 HUD 的观察对象，可以是无控制权的单位。"))
	ACombatUnitCharacter* GetInspectedUnit() const;
	/** 返回有效的有权选中组，首个为主选；不包含仅供查看的其他玩家单位。 */
	UFUNCTION(BlueprintPure, Category="Combat|Selection", meta=(DisplayName="获取选中单位", ToolTip="返回本地有控制权的选中组，最多八个。"))
	TArray<ACombatUnitCharacter*> GetSelectedUnits() const;
	/** 同时核对观察对象、主选和已复制的 Owner，防止 HUD 操作发给旧英雄。 */
	bool CanOperateInspectedUnit() const;
	/** 网络 Owner 是控制权来源；战斗阵营相同不等于可控制。 */
	bool CanControlUnit(const ACombatUnitCharacter* Unit) const;
	/** 服务器授予额外单位，不改变已选主控，也不取消其他单位的命令。 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Combat|Command", meta=(DisplayName="授予单位控制权", ToolTip="仅服务器调用；转移此单位旧控制权并取消其旧命令，保留玩家其他单位。"))
	bool GrantUnitControlAuthority(UPARAM(DisplayName="单位") ACombatUnitCharacter* Unit);
	/** 服务器撤销指定单位，取消该单位命令；其他有权单位保持运行。 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Combat|Command", meta=(DisplayName="撤销单位控制权", ToolTip="仅服务器调用；取消指定单位命令并清除其 Owner，不影响其他单位。"))
	bool RevokeUnitControlAuthority(UPARAM(DisplayName="单位") ACombatUnitCharacter* Unit);
	/** 服务器只切换已有控制权的主选，不转移 Owner、不停止旧英雄。 */
	bool SetPrimaryUnitAuthority(ACombatUnitCharacter* Unit);
	/** 框选只接纳当前有权单位；排序由屏幕选择器提供，去重并限制八个。 */
	void SelectCombatUnits(const TArray<ACombatUnitCharacter*>& Units, bool bAppend);
	/** HUD 使用的本地选择框；返回 false 时没有有效拖框。 */
	bool GetSelectionRectangle(FVector2D& Start, FVector2D& End) const;
	/** 服务器群体请求入口；安全检查全部通过后才逐单位进入公共 Order。 */
	FCombatOrderBatchResult ProcessGroupOrderRequest(const FCombatGroupOrderRequest& Request);
	static constexpr int32 MaxSelectedUnits = 8;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	/** 在 Enhanced Input 本帧处理完后更新本地镜头；不提交任何单位命令。 */
	virtual void PlayerTick(float DeltaTime) override;
	/** 引擎因视口失焦冲刷按键时废弃旧瞄准与手势，防止恢复焦点后误确认。 */
	virtual void FlushPressedKeys() override;

	/** 返回当前显式主控 Combat Unit；输入不得从 GetPawn 推断该对象。 */
	UFUNCTION(BlueprintPure, Category="Combat|Command", meta=(DisplayName="获取主控战斗单位", ToolTip="返回由服务器绑定并仅复制给拥有者的主控 Combat Unit。"))
	ACombatUnitCharacter* GetCommandedUnit() const { return CommandedUnit; }
	/** 返回跟随玩家连接的战略资源与商店事务组件；购买结果仍交给当前主控英雄库存。 */
	UFUNCTION(BlueprintPure, Category="Combat|Economy", meta=(DisplayName="获取战斗经济组件"))
	UCombatEconomyComponent* GetCombatEconomyComponent() const { return CombatEconomyComponent; }

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
	/** 本地 HUD 技能槽点击入口；只转发槽位索引，Ability/目标/权限仍由现有输入与服务器链路处理。 */
	void ActivateCombatAbilitySlotFromHUD(int32 SlotIndex);
	/** 使用一个精确物品快照；HUD 与快捷键都只经统一 Order 入口提交。 */
	bool UseInventoryItem(int32 Slot, const FCombatItemView& Expected);
	/** 请求交换快照中的两槽；不会替换服务器当前指令。 */
	bool SwapInventoryItems(int32 From, int32 To, const FCombatHUDOwnerView& Expected);
	/** 拖放使用释放事件的屏幕位置，避免拖拽期间视口鼠标查询失效；仍通过服务器地面/导航复核。 */
	bool DropInventoryItemAtScreenPosition(const FCombatItemView& Expected, const FVector2D& ScreenPosition);
	/** 从 Enhanced Input 当前映射取得物品热键，不在 HUD 写死物理键。 */
	FText GetItemHotkeyText(int32 Slot) const;
	/** 拾取与放置最终结果，仅用于本地 HUD。 */
	FText GetItemStatusText() const;
	/** 本地商店购买入口；返回仅表示请求已发送，最终结果读取 OnEconomyResult。 */
	UFUNCTION(BlueprintCallable, Category="Combat|Economy", meta=(DisplayName="购买商店物品"))
	bool PurchaseShopItem(FPrimaryAssetId ItemDefinitionId);
	/** 本地物品栏出售入口；服务器重新校验拥有者、修订、出售资格和退款。 */
	UFUNCTION(BlueprintCallable, Category="Combat|Economy", meta=(DisplayName="出售物品栏物品", ToolTip="出售精确物品栏实例；金币与清理结果由服务器决定。"))
	bool SellInventoryItem(const FCombatItemView& ExpectedItem);
	/** 本地物品栏锁定入口；服务器切换状态，锁定物品不参与合成。 */
	UFUNCTION(BlueprintCallable, Category="Combat|Economy", meta=(DisplayName="切换物品锁定", ToolTip="锁定或解锁精确物品栏实例；锁定只影响合成与购买抵扣。"))
	bool ToggleInventoryItemLock(const FCombatItemView& ExpectedItem);
	/** 服务器业务入口，供可靠 RPC 与自动化共用。 */
	FCombatEconomyResult ProcessEconomyRequestForConnection(APlayerController* RequestingController,
		const FCombatEconomyRequest& Request);
	const FCombatEconomyResult& GetLastEconomyResult() const { return LastEconomyResult; }
	UPROPERTY(BlueprintAssignable, Category="Combat|Economy", meta=(DisplayName="经济事务结果"))
	FCombatEconomyResultDelegate OnEconomyResult;

protected:
	/**
	 * Command Pawn 占有完成后刷新本地跟随目标；异常收到 Combat Unit 时拒绝建立玩家占有。
	 * 正常出生必须由 GameMode 先建立 CommandedUnit，再只把 Command Pawn 交给 Possess。
	 */
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;

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

	/** 仅在按住期间跟随主控单位；具体键位由 Demo IMC 配置为 Space。 */
	UPROPERTY(EditAnywhere, Category="Input|Camera", meta=(DisplayName="临时跟随输入", ToolTip="Boolean Enhanced Input Action：按住跟随、松开停留。空值禁用跟随键；边缘滚屏仍可用。"))
	TObjectPtr<UInputAction> CameraFollowAction;

	/** 进入普攻选敌模式的输入；具体按键由映射上下文决定，默认 A。 */
	UPROPERTY(EditAnywhere, Category="Input|Combat", meta=(DisplayName="普攻选敌输入", ToolTip="开始选择普通攻击目标；在输入映射中配置按键，Demo 默认 A。为空时禁用此输入。"))
	TObjectPtr<UInputAction> AttackTargetAction;

	/** 普通点击/框选与战斗瞄准共用的输入，默认鼠标左键；瞄准确认优先。 */
	UPROPERTY(EditAnywhere, Category="Input|Combat", meta=(DisplayName="确认战斗目标输入", ToolTip="普通状态点击查看或拖动框选；普攻/技能瞄准时确认实际命中。Demo 默认左键，UI 上不提交，空值禁用。"))
	TObjectPtr<UInputAction> ConfirmAttackTargetAction;
	/** 与点击/框选配合的追加动作，由 IMC 默认映射到左右 Shift。 */
	UPROPERTY(EditAnywhere, Category="Input|Selection", meta=(DisplayName="追加选择输入", ToolTip="按住时点击增删组成员、框选追加；空值禁用追加选择。"))
	TObjectPtr<UInputAction> AddToSelectionAction;

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
	/** 本地弱选择不参与复制；Owner 和主选继续由服务器确认。 */
	TArray<TWeakObjectPtr<ACombatUnitCharacter>> SelectedUnits;
	TWeakObjectPtr<ACombatUnitCharacter> InspectedUnit;
	bool bSelectionInitialized = false;
	bool bSelectionGesture = false;
	bool bSelectionAdditive = false;
	bool bSelectionModifierDown = false;
	FVector2D SelectionStart = FVector2D::ZeroVector;
	FVector2D SelectionEnd = FVector2D::ZeroVector;
	/** 保留最近发送的主选意图，跨只读查看期间也能纠正尚未到达的旧复制。 */
	TWeakObjectPtr<ACombatUnitCharacter> LastRequestedPrimary;
	int32 PendingPrimaryRequestId = 0;
	/** 选择输入与瞄准复用同一确认 Action，只有普通世界点击启动选择。 */
	void BeginSelectionGesture();
	void UpdateSelectionGesture();
	void FinishSelectionGesture();
	void CancelSelectionGesture();
	void OnSelectionModifierStarted();
	void OnSelectionModifierReleased();
	/** 清理失效/撤权成员并在初始 Owner 到达后建立单选，不恢复已取消的手势。 */
	void RefreshLocalSelection();
	/** 本地选择变化只取消旧输入意图，再请求服务器切换已有权限的主选。 */
	void PublishLocalSelection();
	/** 多选共用一次连接预算；单选继续沿既有 Unit RPC。 */
	bool SubmitSelectedGroupOrder(const FCombatOrderRequest& Order);
	UFUNCTION(Server, Reliable) void ServerSelectPrimaryUnit(ACombatUnitCharacter* Unit, int32 RequestId);
	UFUNCTION(Client, Reliable) void ClientPrimarySelectionResult(int32 RequestId, bool bAccepted);
	UFUNCTION(Server, Reliable) void ServerIssueGroupOrder(FCombatGroupOrderRequest Request);
#if WITH_DEV_AUTOMATION_TESTS
	friend class FCombatSelectionGroupTest;
	friend class FCombatSelectionLifecycleTest;
#endif
	friend class ACombatAbilityAimScenarioActor;
	friend class ACombatSelectionNetworkScenario;
	/** 本地意图适配默认子对象，Dedicated 禁用 Tick 且不生成视觉 Actor。 */
	UPROPERTY(VisibleAnywhere, Category="Combat|Indicator", meta=(DisplayName="技能瞄准", ToolTip="只在本地玩家上更新的技能指示器会话。"))
	TObjectPtr<UCombatAbilityAimComponent> AbilityAimComponent;
	/** 默认子对象记录本连接可见事件；生命周期跟随 Controller，HUD 关闭不停止记录。 */
	UPROPERTY(VisibleAnywhere, Category="Combat|Log", meta=(DisplayName="战斗记录组件", ToolTip="服务器生成并仅向本连接复制的只读战斗历史。"))
	TObjectPtr<UCombatLogComponent> CombatLogComponent;
	/** 玩家级单一金币和商店事务状态；库存内容仍由当前主控单位权威持有。 */
	UPROPERTY(VisibleAnywhere, Category="Combat|Economy", meta=(DisplayName="战斗经济组件"))
	TObjectPtr<UCombatEconomyComponent> CombatEconomyComponent;
#if WITH_DEV_AUTOMATION_TESTS
	friend class FCombatPlayerItemDropPathTest;
	friend class FCombatPlayerAttackInputTest;
	friend class FCombatPlayerAttackInputCancellationTest;
	friend class FCombatPlayerAutoCastAbilityInputTest;
	friend class FCombatAbilityAimInputTest;
	friend class FCombatAbilityUnitAimTest;
	friend class FCombatCameraInputTest;
#endif
	/** 以当前 Pawn/就绪目标/绑定代次幂等刷新，补齐客户端复制先后不确定的情况。 */
	void RefreshLocalCameraBinding();
	/** 采样视口绝对位置、焦点和 UI 捕获，再只调用一次相机更新。 */
	void UpdateLocalCamera(float DeltaSeconds);
	/** 检查当前本地视口是否接受世界相机输入；控制台与应用失焦都拒绝。 */
	bool IsCameraViewportFocused() const;
	/** 三种 Enhanced Input 事件使用同一 Action，释放只结束保存的按住号。 */
	void BindCameraActions(UEnhancedInputComponent& EnhancedInputComponent);
	void OnCameraFollowStarted();
	void OnCameraFollowReleased();
	/** 清理旧 Pawn 的本地会话和保存的释放号；不清理战斗命令。 */
	void ResetLocalCameraInput();
	TWeakObjectPtr<ACombatCharacter> LocalCameraPawn;
	uint64 CameraFollowPressSerial = 0;

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
	/** HUD 技能点击等待本帧焦点收尾后执行，避免 FlushPressedKeys 清掉新会话。 */
	FTimerHandle PendingHUDAbilityTimer;
	/** FlushPressedKeys 只清理旧输入，不能取消同一焦点切换中已排队的 HUD 技能点击。 */
	bool bFlushingPressedKeys = false;
	/** 用当前实际命中和会话号尝试确认一次；保持唯一 SubmitCombatOrder 入口。 */
	void ConfirmAbilityTarget(const FHitResult& Hit, uint64 Serial);
	int32 AllocateCombatRequestId();
	bool SubmitEconomyRequest(FCombatEconomyRequest Request);
	UFUNCTION(Server, Reliable) void ServerSubmitEconomyRequest(FCombatEconomyRequest Request);
	UFUNCTION(Client, Reliable) void ClientReceiveEconomyResult(FCombatEconomyResult Result);
	UPROPERTY(Transient) FCombatEconomyResult LastEconomyResult;
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
