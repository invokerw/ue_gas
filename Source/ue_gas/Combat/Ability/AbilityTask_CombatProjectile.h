#pragma once

#include "CoreMinimal.h"
#include "Abilities/Tasks/AbilityTask.h"

#include "Combat/Projectile/CombatProjectileTypes.h"

#include "AbilityTask_CombatProjectile.generated.h"

/** 向蓝图报告弹体创建或结束结果；具体是哪一阶段由所绑定的任务输出决定。 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCombatProjectileTaskDelegate, FCombatProjectileResult, Result);

/** 创建一枚直线弹体的技能任务；激活时向服务器弹体子系统提交参数并报告结果，任务随即结束，不等待弹体命中。 */
UCLASS()
class UE_GAS_API UAbilityTask_SpawnLinearProjectile : public UAbilityTask
{
	GENERATED_BODY()

public:
	/** 构造直线弹体创建任务，激活后才发射；任务选择直线运动，目标丢失和命中策略从提供的弹体定义复制。 */
	UFUNCTION(BlueprintCallable, Category="Combat|Ability|Tasks", meta=(DisplayName="生成战斗直线弹体", ToolTip="构造直线弹体创建任务，激活后才发射；任务选择直线运动，目标丢失和命中策略从提供的弹体定义复制。", HidePin="OwningAbility", DefaultToSelf="OwningAbility", BlueprintInternalUseOnly="true"))
	static UAbilityTask_SpawnLinearProjectile* SpawnLinearProjectile(
		UPARAM(DisplayName="所属技能") UGameplayAbility* OwningAbility,
		UPARAM(DisplayName="弹体规格") FCombatProjectileSpec Spec);
	/** 提交弹体创建请求，按结果广播 OnSpawned 或 OnFailed，然后结束任务；不在此处结算命中。 */
	virtual void Activate() override;

	/** 弹体已创建并返回可跟踪的句柄；此通知不表示命中成功。 */
	UPROPERTY(BlueprintAssignable) FCombatProjectileTaskDelegate OnSpawned;
	/** 弹体创建请求失败；原因见结果标签，未取得活动弹体。 */
	UPROPERTY(BlueprintAssignable) FCombatProjectileTaskDelegate OnFailed;

private:
	/** 任务保存的发射参数副本，激活时交给弹体子系统；调用者之后修改原请求不会改变该副本。 */
	FCombatProjectileSpec PendingSpec;
};

/** 创建一枚追踪单位目标的弹体任务；目标和来源由服务器弹体子系统校验，报告创建结果后任务结束，不等待飞行完成。 */
UCLASS()
class UE_GAS_API UAbilityTask_SpawnTrackingProjectile : public UAbilityTask
{
	GENERATED_BODY()

public:
	/** 构造追踪弹体创建任务，激活后才发射；需提供合法目标，目标丢失和命中策略从提供的弹体定义复制。 */
	UFUNCTION(BlueprintCallable, Category="Combat|Ability|Tasks", meta=(DisplayName="生成战斗追踪弹体", ToolTip="构造追踪弹体创建任务，激活后才发射；需提供合法目标，目标丢失和命中策略从提供的弹体定义复制。", HidePin="OwningAbility", DefaultToSelf="OwningAbility", BlueprintInternalUseOnly="true"))
	static UAbilityTask_SpawnTrackingProjectile* SpawnTrackingProjectile(
		UPARAM(DisplayName="所属技能") UGameplayAbility* OwningAbility,
		UPARAM(DisplayName="弹体规格") FCombatProjectileSpec Spec);
	/** 提交弹体创建请求，按结果广播 OnSpawned 或 OnFailed，然后结束任务；不在此处结算命中。 */
	virtual void Activate() override;

	/** 弹体已创建并返回可跟踪的句柄；此通知不表示命中成功。 */
	UPROPERTY(BlueprintAssignable) FCombatProjectileTaskDelegate OnSpawned;
	/** 弹体创建请求失败；原因见结果标签，未取得活动弹体。 */
	UPROPERTY(BlueprintAssignable) FCombatProjectileTaskDelegate OnFailed;

private:
	/** 任务保存的发射参数副本，激活时交给弹体子系统；调用者之后修改原请求不会改变该副本。 */
	FCombatProjectileSpec PendingSpec;
};

/**
 * 观察一枚已创建弹体的最终结果；只在弹体最终结束时通知，穿透途中命中不会逐次通知。
 * 所属技能提前结束时只解除监听，不取消弹体；若开始监听时弹体已结束，则报告失败而不补发历史结果。
 */
UCLASS()
class UE_GAS_API UAbilityTask_WaitProjectileResult : public UAbilityTask
{
	GENERATED_BODY()

public:
	/** 构造等待指定弹体结束的任务，激活时检查句柄并开始监听；不会创建或取消弹体。 */
	UFUNCTION(BlueprintCallable, Category="Combat|Ability|Tasks", meta=(DisplayName="等待战斗弹体结果", ToolTip="构造等待指定弹体结束的任务，激活时检查句柄并开始监听；不会创建或取消弹体。", HidePin="OwningAbility", DefaultToSelf="OwningAbility", BlueprintInternalUseOnly="true"))
	static UAbilityTask_WaitProjectileResult* WaitProjectileResult(
		UPARAM(DisplayName="所属技能") UGameplayAbility* OwningAbility,
		UPARAM(DisplayName="弹体句柄") FCombatProjectileHandle Handle);
	/** 确认弹体仍活动后订阅子系统结束通知；不存在或已经结束时广播 OnFailed 并结束任务。 */
	virtual void Activate() override;
	/** 技能或任务结束时解除本任务注册的弹体结束监听，避免回调已销毁的任务。 */
	virtual void OnDestroy(bool bInOwnerFinished) override;

	/** 观察的弹体最终结束时广播一次；若适用，先广播 OnHit 或 OnFizzled，再广播此通知。 */
	UPROPERTY(BlueprintAssignable) FCombatProjectileTaskDelegate OnFinished;
	/** 弹体因命中单位而最终停止时广播；不表示穿透弹体途中每次命中。 */
	UPROPERTY(BlueprintAssignable) FCombatProjectileTaskDelegate OnHit;
	/** 弹体因阻挡、距离耗尽、超时或目标丢失等非命中原因结束时广播；显式取消和退出场景不走此通知。 */
	UPROPERTY(BlueprintAssignable) FCombatProjectileTaskDelegate OnFizzled;
	/** 开始监听时找不到活动弹体或子系统；任务随即结束，不会再发送 OnFinished。 */
	UPROPERTY(BlueprintAssignable) FCombatProjectileTaskDelegate OnFailed;

private:
	/** 从子系统全部弹体的结束通知中筛选所观察的完整句柄，发送本任务结果并解除监听。 */
	void HandleProjectileFinished(const FCombatProjectileResult& Result);
	/** 待观察弹体的完整身份；通过编号和代次匹配，防止接收其他弹体的结束结果。 */
	FCombatProjectileHandle ObservedHandle;
};
