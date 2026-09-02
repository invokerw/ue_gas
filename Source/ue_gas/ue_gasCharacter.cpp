// Copyright Epic Games, Inc. All Rights Reserved.

#include "ue_gasCharacter.h"

#include "Camera/CameraComponent.h"
#include "Components/SceneComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"

#include "Combat/Unit/CombatUnitCharacter.h"

Aue_gasCharacter::Aue_gasCharacter()
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

void Aue_gasCharacter::BeginPlay()
{
	Super::BeginPlay();
	SetActorEnableCollision(false);
}

void Aue_gasCharacter::Tick(const float DeltaSeconds)
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

void Aue_gasCharacter::SetFollowTarget(ACombatUnitCharacter* NewTarget)
{
	FollowTarget = NewTarget;
	if (NewTarget)
	{
		SetActorLocation(NewTarget->GetActorLocation(), false, nullptr, ETeleportType::TeleportPhysics);
	}
}
