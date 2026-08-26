#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"

#include "Combat/Projectile/CombatProjectileTypes.h"

#include "CombatProjectileSubsystem.generated.h"

/** 单条活动 Projectile 的服务器运行时快照；不暴露给网络或蓝图。 */
struct FCombatProjectileRuntimeRecord
{
	/** 外部稳定句柄。 */
	FCombatProjectileHandle Handle;
	/** Spawn 时完整复制的业务 Spec。 */
	FCombatProjectileSpec Spec;
	/** 负责连续运动和位置复制的 Actor。 */
	TWeakObjectPtr<ACombatProjectileActor> Actor;
	/** 当前规范化运动方向。 */
	FVector Direction = FVector::ForwardVector;
	/** Tracking 最近一次合法目标位置。 */
	FVector LastKnownTargetLocation = FVector::ZeroVector;
	/** Source 与 Target 创建时生命代次。 */
	uint32 SourceLifeGeneration = 0;
	uint32 TargetLifeGeneration = 0;
	/** ProjectileData 数值的不可变 Spawn 快照。 */
	float Speed = 0.0f;
	float Radius = 0.0f;
	float MaxDistance = 0.0f;
	float MaxLifetime = 0.0f;
	float MaxSimulationStep = 100.0f;
	FName CollisionProfileName = TEXT("CombatProjectile");
	/** 已飞行距离和寿命。 */
	float TravelledDistance = 0.0f;
	float Age = 0.0f;
	/** 穿透时确保同一 Actor 至多执行一次 Impact。 */
	TSet<TWeakObjectPtr<AActor>> AlreadyHit;
	/** LastKnown policy 已停止读取 Target Actor。 */
	bool bUsingLastKnownPoint = false;
};

/** 服务器权威 Projectile Handle registry、稳定 sweep 与 exactly-once Finish 入口。 */
UCLASS()
class UE_GAS_API UCombatProjectileSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** 校验并完整快照 Spec，生成权威 Actor 与活动 Handle。 */
	FCombatProjectileResult SpawnProjectile(const FCombatProjectileSpec& Spec);
	/** Projectile Actor 每帧调用；返回 false 表示记录已 Finish。 */
	bool AdvanceProjectile(FCombatProjectileHandle Handle, float DeltaSeconds);
	/** 显式取消一个活动 Projectile；重复或旧 Handle 安全失败。 */
	FCombatProjectileResult CancelProjectile(FCombatProjectileHandle Handle, FGameplayTag FailureTag = FGameplayTag());
	/** Ability End 时只取消显式绑定同一 ActivationId 的 Projectile。 */
	int32 CancelProjectilesForAbility(ACombatUnitCharacter* Source, FCombatEventId ActivationId);
	/** 外部 Destroy/World teardown 时由 Actor 回报 EndPlay。 */
	void NotifyProjectileActorEndPlay(FCombatProjectileHandle Handle);
	/** 返回 Handle 是否仍能解析活动记录。 */
	bool IsProjectileActive(FCombatProjectileHandle Handle) const;
	/** 返回活动 Projectile 数量，供 Gate 泄漏断言。 */
	int32 GetActiveProjectileCount() const { return ActiveProjectiles.Num(); }
	/** 返回最近一次 exactly-once Finish 结果。 */
	const FCombatProjectileResult& GetLastFinishedResult() const { return LastFinishedResult; }
	/** 返回最近一次成功 Spawn 的句柄，供调试、示例与自动化观察。 */
	FCombatProjectileHandle GetLastSpawnedHandle() const { return LastSpawnedHandle; }
	/** 返回全局 Finish 观察委托。 */
	FOnCombatProjectileFinished& OnProjectileFinished() { return ProjectileFinishedDelegate; }
	/** World teardown 前 Finish 全部记录并淘汰 generation。 */
	virtual void Deinitialize() override;

private:
	/** 完成一条记录，先移出 registry 再广播和销毁 Actor。 */
	FCombatProjectileResult FinishProjectile(
		FCombatProjectileHandle Handle,
		ECombatProjectileFinishReason Reason,
		AActor* HitActor,
		float AppliedDamage,
		FGameplayTag FailureTag);
	/** 对一次 substep 做稳定 sweep；返回 false 表示期间已 Finish。 */
	bool SweepStep(FCombatProjectileRuntimeRecord& Record, const FVector& Start, const FVector& End);
	/** 检查 Unit 是否符合快照阵营、状态与 Self 策略。 */
	bool CanHitUnit(const FCombatProjectileRuntimeRecord& Record, ACombatUnitCharacter& Unit) const;
	/** 按快照执行 Attack finalize 或 Damage/Modifier actions。 */
	float ExecuteImpact(FCombatProjectileRuntimeRecord& Record, ACombatUnitCharacter& Unit);
	/** Tracking Actor 是否仍属于同一合法生命和可跟踪状态。 */
	bool IsTrackingTargetValid(const FCombatProjectileRuntimeRecord& Record) const;
	/** 为 Spawn/Hit/Finish 输出结构化 CombatLog。 */
	void EmitProjectileLog(
		const FCombatProjectileRuntimeRecord& Record,
		const FGameplayTag& EventType,
		AActor* HitActor,
		float AppliedDamage,
		FGameplayTag FailureTag) const;

	/** 以 Handle Id 索引的唯一活动 registry。 */
	TMap<uint64, FCombatProjectileRuntimeRecord> ActiveProjectiles;
	/** 下一 Projectile 槽位和 World generation。 */
	uint64 NextProjectileId = 1;
	uint32 ProjectileGeneration = 1;
	/** teardown 时阻止 Actor EndPlay 反向重入。 */
	bool bDeinitializing = false;
	/** 最近 Finish 快照与观察者。 */
	FCombatProjectileHandle LastSpawnedHandle;
	FCombatProjectileResult LastFinishedResult;
	FOnCombatProjectileFinished ProjectileFinishedDelegate;
};
