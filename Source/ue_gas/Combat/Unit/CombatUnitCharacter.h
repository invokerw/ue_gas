#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/Character.h"

#include "Combat/Core/CombatTypes.h"

#include "CombatUnitCharacter.generated.h"

class UCombatAbilitySystemComponent;
class UCombatAttributeSet;
class UCombatModifierComponent;
class UCombatRegenerationComponent;
class UCombatUnitData;
class UCombatUnitLifecycleComponent;
struct FOnAttributeChangeData;

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

	/** 返回当前复制的战斗队伍。 */
	FCombatTeamId GetCombatTeamId() const { return TeamId; }
	/** 返回当前复制的生命状态。 */
	ECombatLifeState GetLifeState() const { return LifeState; }
	/** 返回当前生命代次，用于淘汰复活前创建的回调。 */
	uint32 GetLifeGeneration() const { return LifeGeneration; }
	/** 返回当前 UnitData；未配置时为空。 */
	const UCombatUnitData* GetUnitData() const { return UnitData; }

	/** 服务器从 UnitData 幂等初始化基础属性、队伍、胶囊和 AbilitySet。 */
	UFUNCTION(BlueprintCallable, Category="Combat|Unit")
	bool InitializeFromUnitData(UCombatUnitData* InUnitData);
	/** 返回当前状态标签是否禁止移动。 */
	UFUNCTION(BlueprintPure, Category="Combat|State") bool IsMovementBlocked() const;
	/** 返回当前状态标签是否禁止普通攻击。 */
	UFUNCTION(BlueprintPure, Category="Combat|State") bool IsAttackBlocked() const;
	/** 返回当前状态标签是否禁止普通 Ability 激活。 */
	UFUNCTION(BlueprintPure, Category="Combat|State") bool IsAbilityBlocked() const;

	/** 仅在 Authority 上设置有效 TeamId，并广播 TeamChanged 结构化事件。 */
	UFUNCTION(BlueprintCallable, Category="Combat|Team")
	bool SetCombatTeamId(FCombatTeamId NewTeamId);

	/** 注册 Team、LifeState 与 LifeGeneration 的复制字段。 */
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	/** 获得 Controller 后刷新 Owner/Avatar ActorInfo。 */
	virtual void PossessedBy(AController* NewController) override;
	/** 失去 Controller 时重新评估 ActorInfo，避免保留陈旧 Owner。 */
	virtual void UnPossessed() override;
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
	void OnRep_TeamId();

	/** LifeState 复制后同步 ASC 的唯一生命状态标签。 */
	UFUNCTION()
	void OnRep_LifeState();

	/** 根据 NetMode、Owner 与 Controller 重新建立或清理 ASC ActorInfo。 */
	void RefreshAbilityActorInfo();
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
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Combat|Components")
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

	/** 服务器初始化使用的稳定 Unit 定义。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Combat|Unit")
	TObjectPtr<UCombatUnitData> UnitData;
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
};
