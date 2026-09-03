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
	ACombatProjectileActor();

	/** Spawn 后写入稳定 Handle、DefinitionId 与表现半径。 */
	void InitializeProjectile(FCombatProjectileHandle InHandle, FPrimaryAssetId InDefinitionId, float Radius, int32 InPredictionKey = 0);
	/** Subsystem 主动 Finish 前设置，防止 Destroy 反向产生第二次 EndPlay Finish。 */
	void PrepareForSubsystemDestroy() { bSubsystemDestroying = true; }
	/** 返回当前复制句柄。 */
	FCombatProjectileHandle GetProjectileHandle() const { return ProjectileHandle; }
	/** 返回只参与客户端视觉协调的预测键。 */
	int32 GetPredictionKey() const { return PredictionKey; }

	/** Authority 每帧把 DeltaSeconds 交给 registry 推进。 */
	virtual void Tick(float DeltaSeconds) override;
	/** 外部 Destroy/World teardown 时请求 registry exactly-once Finish。 */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	/** 仅提供表现尺寸，不参与物理回调；命中由手工稳定 sweep 决定。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Combat|Projectile", meta=(DisplayName="弹体视觉根组件", ToolTip="仅提供表现尺寸、不参与物理命中回调的 Sphere 组件。"))
	TObjectPtr<USphereComponent> VisualRoot;

	/** 用于客户端 reconcile 与调试的稳定句柄。 */
	UPROPERTY(ReplicatedUsing=OnRep_ProjectileIdentity, BlueprintReadOnly, Category="Combat|Projectile", meta=(DisplayName="弹体句柄", ToolTip="服务器分配并用于客户端视觉协调的稳定弹体句柄。"))
	FCombatProjectileHandle ProjectileHandle;
	/** 客户端本地解析表现的稳定资产 ID。 */
	UPROPERTY(ReplicatedUsing=OnRep_ProjectileIdentity, BlueprintReadOnly, Category="Combat|Projectile", meta=(DisplayName="弹体定义 ID", ToolTip="客户端用于解析表现资产的稳定 Projectile 定义 ID。"))
	FPrimaryAssetId ProjectileDefinitionId;
	/** 客户端预测视觉与服务器 Actor 的可选协调键。 */
	UPROPERTY(ReplicatedUsing=OnRep_ProjectileIdentity, BlueprintReadOnly, Category="Combat|Projectile", meta=(DisplayName="预测视觉键", ToolTip="用于把客户端本地预测视觉与服务器弹体 Actor 协调为唯一视觉；0 表示未使用。"))
	int32 PredictionKey = 0;

private:
	/** 身份字段首次或再次复制时执行无双视觉协调。 */
	UFUNCTION() void OnRep_ProjectileIdentity();
	/** true 表示 Destroy 由 Subsystem Finish 发起，不再反向通知。 */
	bool bSubsystemDestroying = false;
};
