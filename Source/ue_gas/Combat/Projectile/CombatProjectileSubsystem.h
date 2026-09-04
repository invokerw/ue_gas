#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"

#include "Combat/Projectile/CombatProjectileTypes.h"

#include "CombatProjectileSubsystem.generated.h"

/** 一枚正在飞行的服务器弹体记录；保存发射参数、数值快照、当前位置推进状态及已命中集合，不直接复制给蓝图或网络。 */
struct FCombatProjectileRuntimeRecord
{
	/** 外部稳定句柄。 */
	FCombatProjectileHandle Handle;
	/** 发射请求的副本；其中的对象引用仍指向原单位和定义，飞行数值另存为下方快照。 */
	FCombatProjectileSpec Spec;
	/** 负责连续运动和位置复制的 Actor。 */
	TWeakObjectPtr<ACombatProjectileActor> Actor;
	/** 当前规范化运动方向。 */
	FVector Direction = FVector::ForwardVector;
	/** 追踪期间最近一次目标有效时的世界坐标；目标失效并选择继续飞行后，不再更新此位置。 */
	FVector LastKnownTargetLocation = FVector::ZeroVector;
	/** 记录发射时来源和目标的生命编号，供关联攻击及目标有效性检查区分复活前后。 */
	uint32 SourceLifeGeneration = 0;
	uint32 TargetLifeGeneration = 0;
	/** 发射时确定的速度（厘米/秒）、半径、距离、单步长度（厘米）及寿命（秒）；飞行期间不回读资产数值。 */
	float Speed = 0.0f;
	float Radius = 0.0f;
	float MaxDistance = 0.0f;
	float MaxLifetime = 0.0f;
	float MaxSimulationStep = 100.0f;
	FName CollisionProfileName = TEXT("CombatProjectile");
	/** 累计飞行路径长度（厘米）与经过时间（秒），用于距离和寿命上限检查。 */
	float TravelledDistance = 0.0f;
	float Age = 0.0f;
	/** 已处理过命中的单位集合；穿透弹体再次扫到同一单位时跳过，避免多组件或多帧重复结算。 */
	TSet<TWeakObjectPtr<AActor>> AlreadyHit;
	/** 目标已经失效并切换为飞往最后有效位置；即使目标后来恢复，也不重新追踪。 */
	bool bUsingLastKnownPoint = false;
};

/**
 * 在服务器管理弹体的创建、飞行、碰撞、命中与结束。发射时复制请求和运动数值，弹体 Actor 每帧调用本子系统推进并复制位置。
 * 每段移动拆成有限长度的小步，用球形扫掠检查沿途对象，再按固定顺序筛选目标并提交公共伤害或效果请求。
 * 普攻弹体把命中交回唯一攻击记录处理；命中结束、取消、超时或世界关闭都汇入同一清理入口，每枚弹体只通知一次最终结果。
 */
UCLASS()
class UE_GAS_API UCombatProjectileSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** 校验服务器来源、弹体定义、目标及数值，保存请求和运动参数后创建弹体；成功返回活动句柄，尚未执行飞行或命中。 */
	FCombatProjectileResult SpawnProjectile(const FCombatProjectileSpec& Spec);
	/**
	 * 推进一次服务器飞行并处理路径上的碰撞；返回 true 表示完成本次推进且弹体继续活动。
	 * false 可能表示已经结束、句柄不匹配或时间参数非法；非法 DeltaSeconds 本身不会结束仍有效的记录。
	 */
	bool AdvanceProjectile(FCombatProjectileHandle Handle, float DeltaSeconds);
	/** 停止匹配的活动弹体并广播最终结果、销毁 Actor；若为普攻弹体，还会使对应已发射攻击失败。旧或重复句柄返回失败。 */
	FCombatProjectileResult CancelProjectile(FCombatProjectileHandle Handle, FGameplayTag FailureTag = FGameplayTag());
	/** 按来源单位和技能激活编号取消启用了随技能取消选项的弹体，返回匹配数量；默认独立飞行的弹体不受影响。 */
	int32 CancelProjectilesForAbility(ACombatUnitCharacter* Source, FCombatEventId ActivationId);
	/** 弹体 Actor 被外部销毁时回报，以便结束活动记录和关联攻击；世界整体清理期间避免再次进入结束流程。 */
	void NotifyProjectileActorEndPlay(FCombatProjectileHandle Handle);
	/** 返回 Handle 是否仍能解析活动记录。 */
	bool IsProjectileActive(FCombatProjectileHandle Handle) const;
	/** 返回活动 Projectile 数量，供 Gate 泄漏断言。 */
	int32 GetActiveProjectileCount() const { return ActiveProjectiles.Num(); }
	/** 返回最近一枚弹体的最终结束结果，首次结束前为默认值；后续结束会覆盖，并非可按句柄检索的历史结果库。 */
	const FCombatProjectileResult& GetLastFinishedResult() const { return LastFinishedResult; }
	/** 返回最近一次成功 Spawn 的句柄，供调试、示例与自动化观察。 */
	FCombatProjectileHandle GetLastSpawnedHandle() const { return LastSpawnedHandle; }
	/** 订阅本世界所有弹体的最终结束通知；调用者按完整句柄筛选，穿透途中命中只写命中日志而不广播此委托。 */
	FOnCombatProjectileFinished& OnProjectileFinished() { return ProjectileFinishedDelegate; }
	/** 世界关闭时结束全部弹体和关联未完成攻击，销毁 Actor 并提升代次，使旧句柄失效。 */
	virtual void Deinitialize() override;

private:
	/** 统一结束入口：校验完整身份后先删除活动记录，再处理关联攻击失败、广播结果并销毁 Actor；重复结束不再次通知。 */
	FCombatProjectileResult FinishProjectile(
		FCombatProjectileHandle Handle,
		ECombatProjectileFinishReason Reason,
		AActor* HitActor,
		float AppliedDamage,
		FGameplayTag FailureTag);
	/** 对一小段路径做球形扫掠并按距离、阻挡优先、Actor 编号排序处理；返回 false 表示本段已使弹体结束。 */
	bool SweepStep(FCombatProjectileRuntimeRecord& Record, const FVector& Start, const FVector& End);
	/** 按发射时来源队伍与目标当前状态检查可命中性；普攻弹体只允许原攻击目标，自身命中由单独开关决定。 */
	bool CanHitUnit(const FCombatProjectileRuntimeRecord& Record, ACombatUnitCharacter& Unit) const;
	/** 普攻弹体交回攻击组件结算；普通弹体按动作顺序提交伤害和效果，返回这一次目标命中的实际扣血合计。 */
	float ExecuteImpact(FCombatProjectileRuntimeRecord& Record, ACombatUnitCharacter& Unit);
	/** 检查追踪目标仍有效、属于本世界及原生命，并满足存活和可选取状态；失败时由目标丢失策略决定后续飞行。 */
	bool IsTrackingTargetValid(const FCombatProjectileRuntimeRecord& Record) const;
	/** 为 Spawn/Hit/Finish 输出结构化 CombatLog。 */
	void EmitProjectileLog(
		const FCombatProjectileRuntimeRecord& Record,
		const FGameplayTag& EventType,
		AActor* HitActor,
		float AppliedDamage,
		FGameplayTag FailureTag) const;

	/** 以弹体编号索引当前飞行记录；取出后仍需核对完整句柄及本世界代次。 */
	TMap<uint64, FCombatProjectileRuntimeRecord> ActiveProjectiles;
	/** 下一 Projectile 槽位和 World generation。 */
	uint64 NextProjectileId = 1;
	uint32 ProjectileGeneration = 1;
	/** 世界整体清理期间忽略销毁 Actor 引起的结束回报，避免重复清理。 */
	bool bDeinitializing = false;
	/** 最近创建的句柄、最近最终结束结果及本地结束通知，供调用方观察和调试。 */
	FCombatProjectileHandle LastSpawnedHandle;
	FCombatProjectileResult LastFinishedResult;
	FOnCombatProjectileFinished ProjectileFinishedDelegate;
};
