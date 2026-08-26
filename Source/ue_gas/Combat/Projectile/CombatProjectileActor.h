#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "Combat/Core/CombatTypes.h"

#include "CombatProjectileActor.generated.h"

class USphereComponent;

/** 只负责权威连续运动与位置复制，全部 gameplay 状态由 ProjectileSubsystem 持有。 */
UCLASS(Blueprintable)
class UE_GAS_API ACombatProjectileActor : public AActor
{
	GENERATED_BODY()

public:
	/** 创建无物理碰撞的表现根组件，并启用服务器 Tick 与 movement replication。 */
	ACombatProjectileActor();

	/** Spawn 后写入稳定 Handle、DefinitionId 与表现半径。 */
	void InitializeProjectile(FCombatProjectileHandle InHandle, FPrimaryAssetId InDefinitionId, float Radius);
	/** Subsystem 主动 Finish 前设置，防止 Destroy 反向产生第二次 EndPlay Finish。 */
	void PrepareForSubsystemDestroy() { bSubsystemDestroying = true; }
	/** 返回当前复制句柄。 */
	FCombatProjectileHandle GetProjectileHandle() const { return ProjectileHandle; }

	/** Authority 每帧把 DeltaSeconds 交给 registry 推进。 */
	virtual void Tick(float DeltaSeconds) override;
	/** 外部 Destroy/World teardown 时请求 registry exactly-once Finish。 */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	/** 复制 Handle、DefinitionId 和 Actor movement。 */
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	/** 仅提供表现尺寸，不参与物理回调；命中由手工稳定 sweep 决定。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Combat|Projectile")
	TObjectPtr<USphereComponent> VisualRoot;

	/** 用于客户端 reconcile 与调试的稳定句柄。 */
	UPROPERTY(Replicated, BlueprintReadOnly, Category="Combat|Projectile")
	FCombatProjectileHandle ProjectileHandle;
	/** 客户端本地解析表现的稳定资产 ID。 */
	UPROPERTY(Replicated, BlueprintReadOnly, Category="Combat|Projectile")
	FPrimaryAssetId ProjectileDefinitionId;

private:
	/** true 表示 Destroy 由 Subsystem Finish 发起，不再反向通知。 */
	bool bSubsystemDestroying = false;
};
