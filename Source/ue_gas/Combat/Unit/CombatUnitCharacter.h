#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/Character.h"

#include "Combat/Core/CombatTypes.h"
#include "Combat/Network/CombatNetworkTypes.h"

#include "CombatUnitCharacter.generated.h"

class UCombatAbilitySystemComponent;
class UCombatAttackComponent;
class UCombatAttributeSet;
class UCombatModifierComponent;
class UCombatMotionComponent;
class UCombatOrderComponent;
class UCombatOverheadWidgetComponent;
class UCombatRegenerationComponent;
class UCombatUnitData;
class UCombatUnitLifecycleComponent;
class UCombatUnitViewComponent;
class ACombatUnitAIController;
class APlayerController;
class UNetConnection;
class UPlayer;
struct FOnAttributeChangeData;

/** Unit 选择 GAS GameplayEffect 复制模式的产品策略。 */
UENUM(BlueprintType)
enum class ECombatAscReplicationPolicy : uint8
{
	/** 服务器根据是否存在 PlayerController Owner 选择 Mixed 或 Minimal。 */
	Automatic UMETA(DisplayName="自动（玩家 Mixed / AI Minimal）"),
	/** 指挥该单位的玩家连接接收完整活动 GameplayEffect，其余接收该单位复制的客户端只接收最小效果信息。 */
	Mixed UMETA(DisplayName="Mixed（拥有者完整）"),
	/** 不向客户端复制完整 ActiveGE，适用于中立或纯服务器 AI。 */
	Minimal UMETA(DisplayName="Minimal（仅最小复制）"),
	/** 向接收该单位复制的客户端提供完整活动 GameplayEffect；项目约定仅用于调试和自动化，设置函数本身不检查构建类型。 */
	Full UMETA(DisplayName="Full（仅调试）")
};

/**
 * 战斗单位的基础 Actor，组合 GAS 技能与属性、指令队列、普攻、持续效果、强制位移、生命状态和界面数据组件。
 * 服务器负责初始化、队伍、生命及指挥归属；客户端读取复制状态。AIController 在服务器驱动导航，玩家控制器通过 Actor 的 Owner 获得指令 RPC 权限，并不直接控制单位的移动输入。
 */
UCLASS(Blueprintable)
class UE_GAS_API ACombatUnitCharacter : public ACharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	ACombatUnitCharacter();

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	/** 返回强类型 Combat ASC。 */
	UCombatAbilitySystemComponent* GetCombatAbilitySystemComponent() const { return CombatAbilitySystemComponent; }
	/** 返回 Unit 注册到 ASC 的 Combat AttributeSet。 */
	UCombatAttributeSet* GetCombatAttributeSet() const { return CombatAttributeSet; }
	/** 返回持续效果组件，统一管理 GAS 活动效果与项目自定义效果实例。 */
	UCombatModifierComponent* GetCombatModifierComponent() const { return CombatModifierComponent; }
	/** 返回服务器权威生命状态机组件。 */
	UCombatUnitLifecycleComponent* GetCombatLifecycleComponent() const { return CombatLifecycleComponent; }
	/** 返回 Scheduler 驱动的恢复组件。 */
	UCombatRegenerationComponent* GetCombatRegenerationComponent() const { return CombatRegenerationComponent; }
	/** 返回管理本单位普攻记录、前摇和攻击间隔的组件。 */
	UCombatAttackComponent* GetCombatAttackComponent() const { return CombatAttackComponent; }
	/** 返回按提交顺序执行移动、施法和普攻指令的组件。 */
	UCombatOrderComponent* GetCombatOrderComponent() const { return CombatOrderComponent; }
	/** 返回水平/垂直强制位移通道组件。 */
	UCombatMotionComponent* GetCombatMotionComponent() const { return CombatMotionComponent; }
	/** 返回向界面提供生命、法力、施法进度和可见效果快照的复制组件。 */
	UCombatUnitViewComponent* GetCombatUnitViewComponent() const { return CombatUnitViewComponent; }
	/** 返回默认挂载在 Unit 头顶的资源条、状态条与跳字组件。 */
	UCombatOverheadWidgetComponent* GetCombatOverheadWidgetComponent() const { return CombatOverheadWidgetComponent; }

	/** 返回当前复制的战斗队伍。 */
	FCombatTeamId GetCombatTeamId() const { return TeamId; }
	/** 返回当前复制的生命状态。 */
	ECombatLifeState GetLifeState() const { return LifeState; }
	/** 返回当前生命代次，用于淘汰复活前创建的回调。 */
	uint32 GetLifeGeneration() const { return LifeGeneration; }
	/** 返回当前 UnitData；未配置时为空。 */
	const UCombatUnitData* GetUnitData() const { return UnitData; }
	/** 返回初始化成功后的稳定 Unit DefinitionId。 */
	FPrimaryAssetId GetUnitDefinitionId() const { return InitializedUnitDefinitionId; }
	/** 返回服务器当前实际采用的 ASC 复制策略。 */
	UFUNCTION(BlueprintPure, Category="Combat|Network", meta=(DisplayName="获取 ASC 复制策略", ToolTip="返回服务器根据配置与 owning connection 计算出的实际复制策略。"))
	ECombatAscReplicationPolicy GetEffectiveAscReplicationPolicy() const { return EffectiveAscReplicationPolicy; }
	/** 在服务器建立或清除 owning connection；该操作不会改变 AI Possession 或移动网络角色。 */
	UFUNCTION(BlueprintCallable, Category="Combat|Network", meta=(DisplayName="设置指挥玩家", ToolTip="仅在服务器设置拥有该战斗单位的 PlayerController；为空时恢复纯 AI 所有权。"))
	bool SetCommandingPlayerController(UPARAM(DisplayName="玩家控制器") APlayerController* NewController);
	/** 返回显式网络 Owner 对应的指挥玩家；AIController 从不被视为指挥玩家。 */
	UFUNCTION(BlueprintPure, Category="Combat|Network", meta=(DisplayName="获取指挥玩家", ToolTip="返回通过 Unit Owner 建立 owning connection 的 PlayerController。"))
	APlayerController* GetCommandingPlayerController() const;
	/**
	 * 验证服务器 AIController、PathFollowing、SimulatedProxy 与单一 Crowd/RVO 不变量。
	 * @param OutDiagnostic 始终返回包含 Controller/Owner/Role/Crowd/Collision/LifeGeneration 的诊断文本。
	 */
	bool ValidateServerMovementTopology(FString& OutDiagnostic) const;
	/** 输出一次结构化移动拓扑日志；不在 Tick 中调用，避免位置类日志刷屏。 */
	void LogServerMovementTopology(const TCHAR* Context) const;
	/** 状态、Motion 或 Controller 改变后刷新服务器 Crowd participation。 */
	void RefreshServerMovementState();
	/** 单位保留 AIController 导航时，优先把显式 PlayerController Owner 作为网络所有者。 */
	virtual const AActor* GetNetOwner() const override;
	/** 单位保留 AIController 导航时，优先返回指挥玩家的 owning connection。 */
	virtual UNetConnection* GetNetConnection() const override;
	/** 返回当前角色下指挥玩家对应的 UPlayer；无指挥玩家时回退到 Pawn 默认规则。 */
	virtual UPlayer* GetNetOwningPlayer() override;
	/** 忽略本地角色限制返回指挥玩家对应的 UPlayer；无指挥玩家时回退到 Pawn 默认规则。 */
	virtual UPlayer* GetNetOwningPlayerAnyRole() override;

	/** owning client 提交有界 Order 批次；服务器再次执行所有权、限频、载荷和重放校验。 */
	UFUNCTION(BlueprintCallable, Server, Reliable, Category="Combat|Network", meta=(DisplayName="提交战斗命令批次", ToolTip="向服务器提交同一单位的有界命令批次；所有目标与技能参数仍由服务器复核。"))
	void ServerIssueOrderBatch(UPARAM(DisplayName="命令批次") FCombatOrderBatchRequest Request);
	/** 服务器业务入口，供 RPC 与自动化共用。 */
	FCombatOrderBatchResult ProcessOrderBatchForConnection(
		APlayerController* RequestingController,
		const FCombatOrderBatchRequest& Request);
	/** 返回 owning client 最近收到的批次结果。 */
	UFUNCTION(BlueprintPure, Category="Combat|Network", meta=(DisplayName="获取最近命令批次结果", ToolTip="返回 owning client 最近一次收到的服务器批次结果。"))
	const FCombatOrderBatchResult& GetLastOrderBatchResult() const { return LastOrderBatchResult; }
	/** owning client 收到批次结果时广播。 */
	UPROPERTY(BlueprintAssignable, Category="Combat|Network", meta=(DisplayName="命令批次结果", ToolTip="owning client 收到服务器批次结果时广播。")) FCombatOrderBatchResultDelegate OnOrderBatchResult;

	/** 服务器按单位定义初始化队伍、胶囊、基础属性和技能授予表。已成功初始化为同一定义时直接返回 true，换定义则拒绝；主要配置先统一校验，但后续效果应用或技能授予失败不会回滚已经写入的部分。 */
	UFUNCTION(BlueprintCallable, Category="Combat|Unit", meta=(DisplayName="从单位数据初始化", ToolTip="服务器按单位定义初始化队伍、胶囊、基础属性和技能授予表。已成功初始化为同一定义时直接返回 true，换定义则拒绝；主要配置先统一校验，但后续效果应用或技能授予失败不会回滚已经写入的部分。"))
	bool InitializeFromUnitData(UPARAM(DisplayName="单位数据") UCombatUnitData* InUnitData);
	/** 非存活、缺少技能组件，或具有眩晕、定身、妖术、冻结状态时禁止普通移动；强制位移占用另由 Motion 组件处理。 */
	UFUNCTION(BlueprintPure, Category="Combat|State", meta=(DisplayName="移动是否被禁止", ToolTip="非存活、缺少技能组件，或具有眩晕、定身、妖术、冻结状态时禁止普通移动；强制位移占用另由 Motion 组件处理。")) bool IsMovementBlocked() const;
	/** 普通移动被禁止，或具有缴械状态时禁止普攻；因此当前规则也会阻止定身单位普攻。 */
	UFUNCTION(BlueprintPure, Category="Combat|State", meta=(DisplayName="普通攻击是否被禁止", ToolTip="普通移动被禁止，或具有缴械状态时禁止普攻；因此当前规则也会阻止定身单位普攻。")) bool IsAttackBlocked() const;
	/** 非存活、缺少技能组件，或具有眩晕、沉默、妖术、冻结状态时禁止普通技能激活；定身本身不在此判断中。 */
	UFUNCTION(BlueprintPure, Category="Combat|State", meta=(DisplayName="普通技能是否被禁止", ToolTip="非存活、缺少技能组件，或具有眩晕、沉默、妖术、冻结状态时禁止普通技能激活；定身本身不在此判断中。")) bool IsAbilityBlocked() const;

	/** 服务器设置有效的新队伍并刷新光环、界面数据与队伍变化日志；权限不足、队伍无效或值未改变时返回 false。 */
	UFUNCTION(BlueprintCallable, Category="Combat|Team", meta=(DisplayName="设置战斗队伍", ToolTip="服务器设置有效的新队伍并刷新光环、界面数据与队伍变化日志；权限不足、队伍无效或值未改变时返回 false。"))
	bool SetCombatTeamId(UPARAM(DisplayName="新队伍 ID") FCombatTeamId NewTeamId);

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	/** 获得 Controller 后刷新 Owner/Avatar ActorInfo。 */
	virtual void PossessedBy(AController* NewController) override;
	/** 失去 Controller 时重新评估 ActorInfo，避免保留陈旧 Owner。 */
	virtual void UnPossessed() override;
	/** Controller 变化时刷新 OrderComponent 的 PathFollowing delegate。 */
	virtual void NotifyControllerChanged() override;
	/** Actor 结束时清空 ASC ActorInfo，终止跨生命周期引用。 */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

protected:
	/** Unit 进入 World 后初始化 ActorInfo 与当前生命状态标签。 */
	virtual void BeginPlay() override;
	/** 客户端 Owner 复制变化后刷新 ActorInfo。 */
	virtual void OnRep_Owner() override;
	/** 客户端 Controller 复制变化后刷新 ActorInfo。 */
	virtual void OnRep_Controller() override;

	/** 队伍变化的共用回调；服务器主动调用时刷新界面数据、通知光环重查并记录日志，客户端复制回调不生成权威日志。 */
	UFUNCTION()
	void OnRep_TeamId(FCombatTeamId PreviousTeamId);

	/** 生命状态变化后同步技能组件的生命标签、移动与碰撞响应；服务器还刷新界面数据并通知光环重查目标。 */
	UFUNCTION()
	void OnRep_LifeState();

	/** 重新绑定技能系统所需的角色信息；技能系统的 OwnerActor 与 AvatarActor 都指向本单位，网络指挥权由 Actor Owner 另行决定。 */
	void RefreshAbilityActorInfo();
	/** 仅在服务器根据产品策略设置 GAS GameplayEffect 复制模式。 */
	void RefreshCombatReplicationPolicy();
	/** 移除旧生命标签并添加与 LifeState 一致的 Native Tag。 */
	void RefreshLifeStateTag();
	/** 根据当前状态标签更新普通移动、胶囊碰撞和人群避让；存活单位同时通知普攻与指令组件重新判断当前行为。 */
	void RefreshStatusResponse();
	/** MoveSpeed 聚合值改变时投影到 CharacterMovement。 */
	void HandleMoveSpeedChanged(const FOnAttributeChangeData& ChangeData);
	/** 仅 LifecycleComponent 可以执行服务器状态转换。 */
	void SetLifeStateFromLifecycle(ECombatLifeState NewState);
	/** 仅 LifecycleComponent 可以为新生命递增 generation。 */
	void IncrementLifeGenerationFromLifecycle();

	/** 生命周期组件需要访问受保护状态转换入口。 */
	friend class UCombatUnitLifecycleComponent;
	/** Combat ASC 需要在 Tag count 变化后刷新移动、碰撞与状态投影。 */
	friend class UCombatAbilitySystemComponent;

	/** Unit 自持并复制的 Combat ASC。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Combat|Components", meta=(DisplayName="战斗能力系统组件", ToolTip="Unit 自持并复制的 Combat Ability System Component。"))
	TObjectPtr<UCombatAbilitySystemComponent> CombatAbilitySystemComponent;
	/** ASC 持有并复制的完整基础战斗属性集合。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Combat|Components")
	TObjectPtr<UCombatAttributeSet> CombatAttributeSet;
	/** 管理本单位的 GAS 活动效果及其自定义行为实例，处理叠层、持续时间和移除。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Combat|Components")
	TObjectPtr<UCombatModifierComponent> CombatModifierComponent;
	/** Unit 的 Alive/Dying/Dead/Respawning 状态机组件。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Combat|Components")
	TObjectPtr<UCombatUnitLifecycleComponent> CombatLifecycleComponent;
	/** Unit 的 Health/Mana 恢复调度组件。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Combat|Components")
	TObjectPtr<UCombatRegenerationComponent> CombatRegenerationComponent;
	/** 管理本单位所有活动普攻记录、命中结算和攻击时序。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Combat|Components")
	TObjectPtr<UCombatAttackComponent> CombatAttackComponent;
	/** 服务器指令队列及执行状态，负责衔接移动、施法和普攻完成通知。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Combat|Components")
	TObjectPtr<UCombatOrderComponent> CombatOrderComponent;
	/** Unit 的水平/垂直强制位移通道与抢占执行器。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Combat|Components")
	TObjectPtr<UCombatMotionComponent> CombatMotionComponent;
	/** 提供给客户端界面的只读单位与效果快照，不包含服务器效果实例。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Combat|Components", meta=(DisplayName="战斗单位 View 组件", ToolTip="提供给客户端界面的只读单位与效果快照，不包含服务器效果实例。"))
	TObjectPtr<UCombatUnitViewComponent> CombatUnitViewComponent;
	/** DOTA 风格的屏幕空间头顶资源、施法、控制状态和战斗跳字表现。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Combat|Components", meta=(DisplayName="战斗头顶 UI", ToolTip="读取 CombatUnitView，并接收服务器伤害/治疗跳字。"))
	TObjectPtr<UCombatOverheadWidgetComponent> CombatOverheadWidgetComponent;

	/** 服务器初始化使用的稳定 Unit 定义。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Combat|Unit")
	TObjectPtr<UCombatUnitData> UnitData;
	/** ASC 复制策略；Automatic 按 PlayerController Owner 选择 Mixed 或 Minimal。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Combat|Network", meta=(DisplayName="ASC 复制策略", ToolTip="Automatic 按指挥 PlayerController Owner 选择 Mixed；无玩家 Owner 时选择 Minimal。"))
	ECombatAscReplicationPolicy AscReplicationPolicy = ECombatAscReplicationPolicy::Automatic;
	/** 成功初始化后缓存 DefinitionId，阻止不同定义重复写入同一 Unit。 */
	FPrimaryAssetId InitializedUnitDefinitionId;

	/** 服务器权威并复制的当前战斗队伍。 */
	UPROPERTY(EditAnywhere, ReplicatedUsing=OnRep_TeamId, BlueprintReadOnly, Category="Combat|Team")
	FCombatTeamId TeamId = FCombatTeamId(1);

	/** 服务器权威并复制的当前生命状态。 */
	UPROPERTY(ReplicatedUsing=OnRep_LifeState, BlueprintReadOnly, Category="Combat|Life")
	ECombatLifeState LifeState = ECombatLifeState::Alive;

	/** 每次进入新生命时递增的服务器权威代次。 */
	UPROPERTY(Replicated)
	uint32 LifeGeneration = 1;

private:
	/** 服务器实际应用到 ASC 的复制策略。 */
	ECombatAscReplicationPolicy EffectiveAscReplicationPolicy = ECombatAscReplicationPolicy::Mixed;
	/** 服务器生命周期锚点；Pawn 销毁或 Controller 重建时不依赖 Actor Owner 的引擎回调顺序。 */
	UPROPERTY(Transient)
	TWeakObjectPtr<APlayerController> CommandingPlayerController;
	/** owning client 最近一次收到的安全层与逐项业务结果。 */
	UPROPERTY(Transient)
	FCombatOrderBatchResult LastOrderBatchResult;
	/** 服务器把批次结果只返回 owning client。 */
	UFUNCTION(Client, Reliable)
	void ClientReceiveOrderBatchResult(FCombatOrderBatchResult Result);
};
