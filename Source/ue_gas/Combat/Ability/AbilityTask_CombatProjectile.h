#pragma once

#include "CoreMinimal.h"
#include "Abilities/Tasks/AbilityTask.h"

#include "Combat/Projectile/CombatProjectileTypes.h"

#include "AbilityTask_CombatProjectile.generated.h"

/** Spawn Task 返回 Projectile 结果的蓝图委托。 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCombatProjectileTaskDelegate, FCombatProjectileResult, Result);

/** 校验 Spec 并从统一 Subsystem 生成 Linear Projectile，随后立即结束。 */
UCLASS()
class UE_GAS_API UAbilityTask_SpawnLinearProjectile : public UAbilityTask
{
	GENERATED_BODY()

public:
	/** 创建只负责一次 Spawn 的 AbilityTask。 */
	UFUNCTION(BlueprintCallable, Category="Combat|Ability|Tasks", meta=(DisplayName="Spawn Combat Linear Projectile", HidePin="OwningAbility", DefaultToSelf="OwningAbility", BlueprintInternalUseOnly="true"))
	static UAbilityTask_SpawnLinearProjectile* SpawnLinearProjectile(UGameplayAbility* OwningAbility, FCombatProjectileSpec Spec);
	/** 调用 ProjectileSubsystem 并广播成功或失败一次。 */
	virtual void Activate() override;

	/** Spawn 成功。 */
	UPROPERTY(BlueprintAssignable) FCombatProjectileTaskDelegate OnSpawned;
	/** Spawn 拒绝。 */
	UPROPERTY(BlueprintAssignable) FCombatProjectileTaskDelegate OnFailed;

private:
	/** 待提交的不可变副本。 */
	FCombatProjectileSpec PendingSpec;
};

/** 校验 Target 并从统一 Subsystem 生成 Tracking Projectile，随后立即结束。 */
UCLASS()
class UE_GAS_API UAbilityTask_SpawnTrackingProjectile : public UAbilityTask
{
	GENERATED_BODY()

public:
	/** 创建只负责一次 Spawn 的 Tracking AbilityTask。 */
	UFUNCTION(BlueprintCallable, Category="Combat|Ability|Tasks", meta=(DisplayName="Spawn Combat Tracking Projectile", HidePin="OwningAbility", DefaultToSelf="OwningAbility", BlueprintInternalUseOnly="true"))
	static UAbilityTask_SpawnTrackingProjectile* SpawnTrackingProjectile(UGameplayAbility* OwningAbility, FCombatProjectileSpec Spec);
	/** 调用 ProjectileSubsystem 并广播成功或失败一次。 */
	virtual void Activate() override;

	/** Spawn 成功。 */
	UPROPERTY(BlueprintAssignable) FCombatProjectileTaskDelegate OnSpawned;
	/** Spawn 拒绝。 */
	UPROPERTY(BlueprintAssignable) FCombatProjectileTaskDelegate OnFailed;

private:
	/** 待提交的不可变副本。 */
	FCombatProjectileSpec PendingSpec;
};

/** 可选等待一个 Projectile Finish；Ability 提前结束只解绑，不取消 Projectile。 */
UCLASS()
class UE_GAS_API UAbilityTask_WaitProjectileResult : public UAbilityTask
{
	GENERATED_BODY()

public:
	/** 创建只观察指定 Handle 的等待 Task。 */
	UFUNCTION(BlueprintCallable, Category="Combat|Ability|Tasks", meta=(DisplayName="Wait Combat Projectile Result", HidePin="OwningAbility", DefaultToSelf="OwningAbility", BlueprintInternalUseOnly="true"))
	static UAbilityTask_WaitProjectileResult* WaitProjectileResult(UGameplayAbility* OwningAbility, FCombatProjectileHandle Handle);
	/** 验证 Handle 并绑定全局 Finish 委托。 */
	virtual void Activate() override;
	/** Ability End/Task Destroy 时幂等解绑。 */
	virtual void OnDestroy(bool bInOwnerFinished) override;

	/** 指定 Projectile exactly-once Finish。 */
	UPROPERTY(BlueprintAssignable) FCombatProjectileTaskDelegate OnFinished;
	/** 指定 Projectile 以命中单位结束。 */
	UPROPERTY(BlueprintAssignable) FCombatProjectileTaskDelegate OnHit;
	/** 指定 Projectile 因阻挡、距离、超时或目标丢失而结束。 */
	UPROPERTY(BlueprintAssignable) FCombatProjectileTaskDelegate OnFizzled;
	/** Handle 在绑定时已经无效。 */
	UPROPERTY(BlueprintAssignable) FCombatProjectileTaskDelegate OnFailed;

private:
	/** 过滤全局 Finish 委托。 */
	void HandleProjectileFinished(const FCombatProjectileResult& Result);
	/** 正在观察的完整 Handle。 */
	FCombatProjectileHandle ObservedHandle;
};
