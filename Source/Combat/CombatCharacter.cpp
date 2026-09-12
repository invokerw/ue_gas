// Copyright Epic Games, Inc. All Rights Reserved.

#include "CombatCharacter.h"

#include "Camera/CameraComponent.h"
#include "Components/SceneComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"

#include "Combat/Unit/CombatUnitCharacter.h"

ACombatCharacter::ACombatCharacter()
{
	bReplicates = true;
	SetReplicateMovement(false);
	SetActorEnableCollision(false);
	CommandRoot = CreateDefaultSubobject<USceneComponent>(TEXT("CommandRoot"));
	SetRootComponent(CommandRoot);

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(CommandRoot);
	CameraBoom->SetUsingAbsoluteRotation(true);
	CameraBoom->TargetArmLength = 800.f;
	CameraBoom->SetRelativeRotation(FRotator(-60.f, 0.f, 0.f));
	CameraBoom->bDoCollisionTest = false;

	TopDownCameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("TopDownCamera"));
	TopDownCameraComponent->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	TopDownCameraComponent->bUsePawnControlRotation = false;

	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
}

void ACombatCharacter::BeginPlay()
{
	Super::BeginPlay();
	SetActorEnableCollision(false);
}

void ACombatCharacter::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const APlayerController* PlayerController = Cast<APlayerController>(GetController());
	ACombatUnitCharacter* Target = FollowTarget.Get();
	if (!PlayerController || !PlayerController->IsLocalController() || !Target)
	{
		return;
	}
	const FVector TargetLocation = Target->GetActorLocation();
	const FVector NewLocation = CameraFollowSpeed <= 0.0f
		? TargetLocation
		: FMath::VInterpTo(GetActorLocation(), TargetLocation, DeltaSeconds, CameraFollowSpeed);
	SetActorLocation(NewLocation, false, nullptr, ETeleportType::None);
}

void ACombatCharacter::SetFollowTarget(ACombatUnitCharacter* NewTarget)
{
	FollowTarget = NewTarget;
	if (NewTarget)
	{
		SetActorLocation(NewTarget->GetActorLocation(), false, nullptr, ETeleportType::TeleportPhysics);
	}
}
