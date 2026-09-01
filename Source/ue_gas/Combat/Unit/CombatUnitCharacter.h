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
	/** 玩家 owning connection 接收完整 ActiveGE，其他客户端接收最小数据。 */
	Mixed UMETA(DisplayName="Mixed（拥有者完整）"),
	/** 不向客户端复制完整 ActiveGE，适用于中立或纯服务器 AI。 */
	Minimal UMETA(DisplayName="Minimal（仅最小复制）"),
	/** 向所有客户端复制完整 ActiveGE，仅允许调试和自动化使用。 */
	Full UMETA(DisplayName="Full（仅调试）")
};

/** 具备服务器权威 Team/Life 状态和自持 ASC 的基础战斗单位。 */
UCLASS(Blueprintable)
class UE_GAS_API ACombatUnitCharacter : public ACharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	/** 创建复制的 Combat ASC，并设置单位默认碰撞与网络属性。 */
	ACombatUnitCharacter();

	/** 通过 IAbilitySystemInterface 返回单位持有的 ASC。 */
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	/** 返回强类型 Combat ASC。 */
	UCombatAbilitySystemComponent* GetCombatAbilitySystemComponent() const { return CombatAbilitySystemComponent; }
	/** 返回 Unit 注册到 ASC 的 Combat AttributeSet。 */
	UCombatAttributeSet* GetCombatAttributeSet() const { return CombatAttributeSet; }
	/** 返回 ActiveGE/Runtime 一一映射组件。 */
	UCombatModifierComponent* GetCombatModifierComponent() const { return CombatModifierComponent; }
	/** 返回服务器权威生命状态机组件。 */
	UCombatUnitLifecycleComponent* GetCombatLifecycleComponent() const { return CombatLifecycleComponent; }
	/** 返回 Scheduler 驱动的恢复组件。 */
	UCombatRegenerationComponent* GetCombatRegenerationComponent() const { return CombatRegenerationComponent; }
	/** 返回唯一 AttackRecord registry 与普攻时序组件。 */
	UCombatAttackComponent* GetCombatAttackComponent() const { return CombatAttackComponent; }
	/** 返回统一 Move/Cast/Attack FIFO 状态机组件。 */
	UCombatOrderComponent* GetCombatOrderComponent() const { return CombatOrderComponent; }
	/** 返回水平/垂直强制位移通道组件。 */
	UCombatMotionComponent* GetCombatMotionComponent() const { return CombatMotionComponent; }
	/** 返回 Owner 与非 Owner UI 共用的扁平复制 View。 */
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
	/** 在服务器建立或清除 owning connection，并重新计算 ASC 复制模式。 */
	UFUNCTION(BlueprintCallable, Category="Combat|Network", meta=(DisplayName="设置指挥玩家", ToolTip="仅在服务器设置拥有该战斗单位的 PlayerController；为空时恢复纯 AI 所有权。"))
	bool SetCommandingPlayerController(UPARAM(DisplayName="玩家控制器") APlayerController* NewController);
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

	/** 服务器从 UnitData 幂等初始化基础属性、队伍、胶囊和 AbilitySet。 */
	UFUNCTION(BlueprintCallable, Category="Combat|Unit", meta=(DisplayName="从单位数据初始化", ToolTip="仅在服务器从 UnitData 幂等初始化基础属性、队伍、碰撞体和 AbilitySet。"))
	bool InitializeFromUnitData(UPARAM(DisplayName="单位数据") UCombatUnitData* InUnitData);
	/** 返回当前状态标签是否禁止移动。 */
	UFUNCTION(BlueprintPure, Category="Combat|State", meta=(DisplayName="移动是否被禁止", ToolTip="返回当前聚合状态标签是否禁止自主移动。")) bool IsMovementBlocked() const;
	/** 返回当前状态标签是否禁止普通攻击。 */
	UFUNCTION(BlueprintPure, Category="Combat|State", meta=(DisplayName="普通攻击是否被禁止", ToolTip="返回当前聚合状态标签是否禁止普通攻击。")) bool IsAttackBlocked() const;
	/** 返回当前状态标签是否禁止普通 Ability 激活。 */
	UFUNCTION(BlueprintPure, Category="Combat|State", meta=(DisplayName="普通技能是否被禁止", ToolTip="返回当前聚合状态标签是否禁止普通 Ability 激活。")) bool IsAbilityBlocked() const;

	/** 仅在 Authority 上设置有效 TeamId，并广播 TeamChanged 结构化事件。 */
	UFUNCTION(BlueprintCallable, Category="Combat|Team", meta=(DisplayName="设置战斗队伍", ToolTip="仅在服务器设置有效队伍 ID，并广播结构化的队伍变更事件。"))
	bool SetCombatTeamId(UPARAM(DisplayName="新队伍 ID") FCombatTeamId NewTeamId);

	/** 注册 Team、LifeState 与 LifeGeneration 的复制字段。 */
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

	/** TeamId 复制后输出 TeamChanged 诊断日志。 */
	UFUNCTION()
	void OnRep_TeamId(FCombatTeamId PreviousTeamId);

	/** LifeState 复制后同步 ASC 的唯一生命状态标签。 */
	UFUNCTION()
	void OnRep_LifeState();

	/** 根据 NetMode、Owner 与 Controller 重新建立或清理 ASC ActorInfo。 */
	void RefreshAbilityActorInfo();
	/** 仅在服务器根据产品策略设置 GAS GameplayEffect 复制模式。 */
	void RefreshCombatReplicationPolicy();
	/** 移除旧生命标签并添加与 LifeState 一致的 Native Tag。 */
	void RefreshLifeStateTag();
	/** 根据状态 Tag count 更新移动、碰撞和 Ability 取消响应。 */
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
	/** Unit 的 Modifier ActiveGE/Runtime 管理组件。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Combat|Components")
	TObjectPtr<UCombatModifierComponent> CombatModifierComponent;
	/** Unit 的 Alive/Dying/Dead/Respawning 状态机组件。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Combat|Components")
	TObjectPtr<UCombatUnitLifecycleComponent> CombatLifecycleComponent;
	/** Unit 的 Health/Mana 恢复调度组件。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Combat|Components")
	TObjectPtr<UCombatRegenerationComponent> CombatRegenerationComponent;
	/** Unit 的唯一 AttackRecord registry 与 attack timing 执行器。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Combat|Components")
	TObjectPtr<UCombatAttackComponent> CombatAttackComponent;
	/** Unit 的服务器权威 Order FIFO 与异步状态机。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Combat|Components")
	TObjectPtr<UCombatOrderComponent> CombatOrderComponent;
	/** Unit 的水平/垂直强制位移通道与抢占执行器。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Combat|Components")
	TObjectPtr<UCombatMotionComponent> CombatMotionComponent;
	/** Unit 的 UI 安全扁平复制 View。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Combat|Components", meta=(DisplayName="战斗单位 View 组件", ToolTip="向 Owner 与非 Owner UI 复制相同的安全扁平战斗投影。"))
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
	/** owning client 最近一次收到的安全层与逐项业务结果。 */
	UPROPERTY(Transient)
	FCombatOrderBatchResult LastOrderBatchResult;
	/** 服务器把批次结果只返回 owning client。 */
	UFUNCTION(Client, Reliable)
	void ClientReceiveOrderBatchResult(FCombatOrderBatchResult Result);
};
