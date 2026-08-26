// Copyright Epic Games, Inc. All Rights Reserved.


#include "TwinStickProjectile.h"
#include "Components/SphereComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Components/StaticMeshComponent.h"

ATwinStickProjectile::ATwinStickProjectile()
{
	PrimaryActorTick.bCanEverTick = false;

	// 表现弹体超时后由 Actor 生命周期自动销毁。
	InitialLifeSpan = 2.0f;

	// 创建根碰撞球；它只用于模板表现碰撞。
	RootComponent = CollisionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("Collision Sphere"));

	CollisionSphere->SetSphereRadius(35.0f);
	CollisionSphere->SetNotifyRigidBodyCollision(true);
	CollisionSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	CollisionSphere->SetCollisionObjectType(ECC_WorldDynamic);
	CollisionSphere->SetCollisionResponseToAllChannels(ECR_Block);

	// 创建无碰撞的表现网格。
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(RootComponent);

	Mesh->SetCollisionProfileName(FName("NoCollision"));

	// ProjectileMovement 只负责表现轨迹，不执行 Combat gameplay。
	ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("Projectile Movement"));

	ProjectileMovement->InitialSpeed = 2000.0f;
	ProjectileMovement->MaxSpeed = 15000.0f;
	ProjectileMovement->bRotationFollowsVelocity = true;
	ProjectileMovement->bRotationRemainsVertical = true;
	ProjectileMovement->ProjectileGravityScale = 0.0f;
	ProjectileMovement->bShouldBounce = true;
	ProjectileMovement->bForceSubStepping = true;

	ProjectileMovement->OnProjectileStop.AddDynamic(this, &ATwinStickProjectile::OnProjectileStop);
}

void ATwinStickProjectile::NotifyHit(class UPrimitiveComponent* MyComp, AActor* Other, class UPrimitiveComponent* OtherComp, bool bSelfMoved, FVector HitLocation, FVector HitNormal, FVector NormalImpulse, const FHitResult& Hit)
{
	Super::NotifyHit(MyComp, Other, OtherComp, bSelfMoved, HitLocation, HitNormal, NormalImpulse, Hit);

	// 真正命中由 CombatProjectileSubsystem 的稳定 sweep 结算；模板 Actor 只结束自身表现。
	Destroy();
}

void ATwinStickProjectile::OnProjectileStop(const FHitResult& ImpactResult)
{
	// 运动停止后立即结束表现。
	Destroy();
}
