// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TwinStickProjectile.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UProjectileMovementComponent;

/** TwinStick 模板的纯表现弹体；Combat gameplay 弹体统一由 CombatProjectileSubsystem 管理。 */
UCLASS(abstract)
class ATwinStickProjectile : public AActor
{
	GENERATED_BODY()
	
	/** 提供表现弹体的碰撞球。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	USphereComponent* CollisionSphere;

	/** 提供弹体外观的静态网格。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	UStaticMeshComponent* Mesh;

	/** 只驱动模板表现运动，不负责 Combat 命中结算。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	UProjectileMovementComponent* ProjectileMovement;

public:	

	/** 创建模板表现组件并关闭无用 Actor Tick。 */
	ATwinStickProjectile();

	/** 发生碰撞时只结束表现 Actor，不直接调用任何单位伤害函数。 */
	virtual void NotifyHit(class UPrimitiveComponent* MyComp, AActor* Other, class UPrimitiveComponent* OtherComp, bool bSelfMoved, FVector HitLocation, FVector HitNormal, FVector NormalImpulse, const FHitResult& Hit) override;

protected:
	
	/** ProjectileMovement 停止时销毁表现 Actor。 */
	UFUNCTION()
	void OnProjectileStop(const FHitResult& ImpactResult);

};
