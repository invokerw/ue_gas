#include "Combat/Projectile/CombatProjectileActor.h"

#include "Components/SphereComponent.h"
#include "Net/UnrealNetwork.h"

#include "Combat/Projectile/CombatProjectileSubsystem.h"
#include "Combat/Projectile/CombatProjectilePresentationSubsystem.h"

ACombatProjectileActor::ACombatProjectileActor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
	bReplicates = true;
	SetReplicateMovement(true);
	VisualRoot = CreateDefaultSubobject<USphereComponent>(TEXT("ProjectileVisualRoot"));
	VisualRoot->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	VisualRoot->SetGenerateOverlapEvents(false);
	SetRootComponent(VisualRoot);
}

void ACombatProjectileActor::InitializeProjectile(
	const FCombatProjectileHandle InHandle,
	const FPrimaryAssetId InDefinitionId,
	const float Radius,
	const int32 InPredictionKey)
{
	ProjectileHandle = InHandle;
	ProjectileDefinitionId = InDefinitionId;
	PredictionKey = InPredictionKey;
	VisualRoot->SetSphereRadius(FMath::Max(1.0f, Radius));
}

void ACombatProjectileActor::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (HasAuthority() && ProjectileHandle.IsValid())
	{
		if (UCombatProjectileSubsystem* Projectiles = GetWorld()->GetSubsystem<UCombatProjectileSubsystem>())
		{
			Projectiles->AdvanceProjectile(ProjectileHandle, DeltaSeconds);
		}
	}
}

void ACombatProjectileActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (!HasAuthority())
	{
		if (UCombatProjectilePresentationSubsystem* Presentation = GetWorld()
			? GetWorld()->GetSubsystem<UCombatProjectilePresentationSubsystem>() : nullptr)
		{
			Presentation->NotifyServerProjectileEnded(this);
		}
	}
	if (HasAuthority() && !bSubsystemDestroying && ProjectileHandle.IsValid())
	{
		if (UCombatProjectileSubsystem* Projectiles = GetWorld() ? GetWorld()->GetSubsystem<UCombatProjectileSubsystem>() : nullptr)
		{
			Projectiles->NotifyProjectileActorEndPlay(ProjectileHandle);
		}
	}
	Super::EndPlay(EndPlayReason);
}

void ACombatProjectileActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ACombatProjectileActor, ProjectileHandle);
	DOREPLIFETIME(ACombatProjectileActor, ProjectileDefinitionId);
	DOREPLIFETIME(ACombatProjectileActor, PredictionKey);
}

void ACombatProjectileActor::OnRep_ProjectileIdentity()
{
	if (ProjectileHandle.IsValid())
	{
		if (UCombatProjectilePresentationSubsystem* Presentation = GetWorld()
			? GetWorld()->GetSubsystem<UCombatProjectilePresentationSubsystem>() : nullptr)
		{
			Presentation->ReconcileServerProjectile(this);
		}
	}
}
