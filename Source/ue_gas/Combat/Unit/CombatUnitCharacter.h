#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/Character.h"

#include "Combat/Core/CombatTypes.h"

#include "CombatUnitCharacter.generated.h"

class UCombatAbilitySystemComponent;

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

	/** 返回当前复制的战斗队伍。 */
	FCombatTeamId GetCombatTeamId() const { return TeamId; }
	/** 返回当前复制的生命状态。 */
	ECombatLifeState GetLifeState() const { return LifeState; }
	/** 返回当前生命代次，用于淘汰复活前创建的回调。 */
	uint32 GetLifeGeneration() const { return LifeGeneration; }

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

	/** Unit 自持并复制的 Combat ASC。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Combat|Components")
	TObjectPtr<UCombatAbilitySystemComponent> CombatAbilitySystemComponent;

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
