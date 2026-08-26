#include "Combat/Ability/AbilityTask_CombatProjectile.h"

#include "Combat/Data/CombatDefinitionData.h"
#include "Combat/Projectile/CombatProjectileSubsystem.h"

UAbilityTask_SpawnLinearProjectile* UAbilityTask_SpawnLinearProjectile::SpawnLinearProjectile(
	UGameplayAbility* OwningAbility,
	FCombatProjectileSpec Spec)
{
	UAbilityTask_SpawnLinearProjectile* Task = NewAbilityTask<UAbilityTask_SpawnLinearProjectile>(OwningAbility);
	Task->PendingSpec = MoveTemp(Spec);
	Task->PendingSpec.MovementType = ECombatProjectileMovementType::Linear;
	if (Task->PendingSpec.ProjectileData)
	{
		// Task 选择运动类型，其余命中策略仍从稳定 ProjectileData 复制到 Spawn 快照。
		Task->PendingSpec.TargetLostPolicy = Task->PendingSpec.ProjectileData->TargetLostPolicy;
		Task->PendingSpec.HitPolicy = Task->PendingSpec.ProjectileData->HitPolicy;
	}
	return Task;
}

void UAbilityTask_SpawnLinearProjectile::Activate()
{
	Super::Activate();
	UCombatProjectileSubsystem* Projectiles = GetWorld() ? GetWorld()->GetSubsystem<UCombatProjectileSubsystem>() : nullptr;
	const FCombatProjectileResult Result = Projectiles
		? Projectiles->SpawnProjectile(PendingSpec) : FCombatProjectileResult();
	if (ShouldBroadcastAbilityTaskDelegates())
	{
		if (Result.bSuccess)
		{
			OnSpawned.Broadcast(Result);
		}
		else
		{
			OnFailed.Broadcast(Result);
		}
	}
	EndTask();
}

UAbilityTask_SpawnTrackingProjectile* UAbilityTask_SpawnTrackingProjectile::SpawnTrackingProjectile(
	UGameplayAbility* OwningAbility,
	FCombatProjectileSpec Spec)
{
	UAbilityTask_SpawnTrackingProjectile* Task = NewAbilityTask<UAbilityTask_SpawnTrackingProjectile>(OwningAbility);
	Task->PendingSpec = MoveTemp(Spec);
	Task->PendingSpec.MovementType = ECombatProjectileMovementType::Tracking;
	if (Task->PendingSpec.ProjectileData)
	{
		// Tracking Task 同样只在 Spawn 前读取一次定义，飞行期间不再依赖 Ability 实例。
		Task->PendingSpec.TargetLostPolicy = Task->PendingSpec.ProjectileData->TargetLostPolicy;
		Task->PendingSpec.HitPolicy = Task->PendingSpec.ProjectileData->HitPolicy;
	}
	return Task;
}

void UAbilityTask_SpawnTrackingProjectile::Activate()
{
	Super::Activate();
	UCombatProjectileSubsystem* Projectiles = GetWorld() ? GetWorld()->GetSubsystem<UCombatProjectileSubsystem>() : nullptr;
	const FCombatProjectileResult Result = Projectiles
		? Projectiles->SpawnProjectile(PendingSpec) : FCombatProjectileResult();
	if (ShouldBroadcastAbilityTaskDelegates())
	{
		if (Result.bSuccess)
		{
			OnSpawned.Broadcast(Result);
		}
		else
		{
			OnFailed.Broadcast(Result);
		}
	}
	EndTask();
}

UAbilityTask_WaitProjectileResult* UAbilityTask_WaitProjectileResult::WaitProjectileResult(
	UGameplayAbility* OwningAbility,
	const FCombatProjectileHandle Handle)
{
	UAbilityTask_WaitProjectileResult* Task = NewAbilityTask<UAbilityTask_WaitProjectileResult>(OwningAbility);
	Task->ObservedHandle = Handle;
	return Task;
}

void UAbilityTask_WaitProjectileResult::Activate()
{
	Super::Activate();
	UCombatProjectileSubsystem* Projectiles = GetWorld() ? GetWorld()->GetSubsystem<UCombatProjectileSubsystem>() : nullptr;
	if (!Projectiles || !Projectiles->IsProjectileActive(ObservedHandle))
	{
		FCombatProjectileResult Result;
		Result.Handle = ObservedHandle;
		if (ShouldBroadcastAbilityTaskDelegates()) { OnFailed.Broadcast(Result); }
		EndTask();
		return;
	}
	Projectiles->OnProjectileFinished().AddUObject(this, &UAbilityTask_WaitProjectileResult::HandleProjectileFinished);
}

void UAbilityTask_WaitProjectileResult::OnDestroy(const bool bInOwnerFinished)
{
	if (UCombatProjectileSubsystem* Projectiles = GetWorld() ? GetWorld()->GetSubsystem<UCombatProjectileSubsystem>() : nullptr)
	{
		Projectiles->OnProjectileFinished().RemoveAll(this);
	}
	Super::OnDestroy(bInOwnerFinished);
}

void UAbilityTask_WaitProjectileResult::HandleProjectileFinished(const FCombatProjectileResult& Result)
{
	if (Result.Handle != ObservedHandle)
	{
		return;
	}
	if (ShouldBroadcastAbilityTaskDelegates())
	{
		if (Result.FinishReason == ECombatProjectileFinishReason::Hit)
		{
			OnHit.Broadcast(Result);
		}
		else if (Result.FinishReason != ECombatProjectileFinishReason::Cancelled
			&& Result.FinishReason != ECombatProjectileFinishReason::EndPlay)
		{
			OnFizzled.Broadcast(Result);
		}
		OnFinished.Broadcast(Result);
	}
	EndTask();
}
