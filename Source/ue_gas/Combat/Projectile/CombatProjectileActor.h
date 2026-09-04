#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "Combat/Core/CombatTypes.h"

#include "CombatProjectileActor.generated.h"

class USphereComponent;

/** 弹体的位置与复制载体。服务器每帧把推进请求交给弹体子系统，由子系统移动、检查碰撞并结算；客户端只显示复制结果。 */
UCLASS(Blueprintable)
class UE_GAS_API ACombatProjectileActor : public AActor
{
	GENERATED_BODY()

public:
	ACombatProjectileActor();

	/** 服务器创建后设置弹体句柄、定义 ID、预测视觉键和表现尺寸，供身份复制及客户端显示使用。 */
	void InitializeProjectile(FCombatProjectileHandle InHandle, FPrimaryAssetId InDefinitionId, float Radius, int32 InPredictionKey = 0);
	/** 子系统完成结算后、销毁 Actor 前标记清理来源，避免 EndPlay 再次请求结束同一弹体。 */
	void PrepareForSubsystemDestroy() { bSubsystemDestroying = true; }
	/** 返回当前复制句柄。 */
	FCombatProjectileHandle GetProjectileHandle() const { return ProjectileHandle; }
	/** 返回只参与客户端视觉协调的预测键。 */
	int32 GetPredictionKey() const { return PredictionKey; }

	/** 服务器每帧调用弹体子系统推进；客户端不执行命中或伤害判断。 */
	virtual void Tick(float DeltaSeconds) override;
	/** Actor 被外部销毁时通知子系统结束记录；客户端同时清除对应的视觉登记。 */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	/** 球体组件只表示弹体尺寸，不依赖其碰撞回调结算；服务器子系统沿路径做球形扫掠检测命中。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Combat|Projectile", meta=(DisplayName="弹体视觉根组件", ToolTip="球体组件只表示弹体尺寸，不依赖其碰撞回调结算；服务器子系统沿路径做球形扫掠检测命中。"))
	TObjectPtr<USphereComponent> VisualRoot;

	/** 复制到客户端的弹体身份，用于登记同一枚服务器弹体并避免重复接管视觉。 */
	UPROPERTY(ReplicatedUsing=OnRep_ProjectileIdentity, BlueprintReadOnly, Category="Combat|Projectile", meta=(DisplayName="弹体句柄", ToolTip="复制到客户端的弹体身份，用于登记同一枚服务器弹体并避免重复接管视觉。"))
	FCombatProjectileHandle ProjectileHandle;
	/** 客户端本地解析表现的稳定资产 ID。 */
	UPROPERTY(ReplicatedUsing=OnRep_ProjectileIdentity, BlueprintReadOnly, Category="Combat|Projectile", meta=(DisplayName="弹体定义 ID", ToolTip="客户端用于解析表现资产的稳定 Projectile 定义 ID。"))
	FPrimaryAssetId ProjectileDefinitionId;
	/** 客户端预测视觉与服务器 Actor 的可选协调键。 */
	UPROPERTY(ReplicatedUsing=OnRep_ProjectileIdentity, BlueprintReadOnly, Category="Combat|Projectile", meta=(DisplayName="预测视觉键", ToolTip="用于把客户端本地预测视觉与服务器弹体 Actor 协调为唯一视觉；0 表示未使用。"))
	int32 PredictionKey = 0;

private:
	/** 身份复制后通知表现子系统登记服务器弹体，并替换同预测键的本地临时视觉。 */
	UFUNCTION() void OnRep_ProjectileIdentity();
	/** true 表示 Destroy 由 Subsystem Finish 发起，不再反向通知。 */
	bool bSubsystemDestroying = false;
};
