// Copyright Epic Games, Inc. All Rights Reserved.

#include "CombatCharacter.h"

#include "Camera/CameraComponent.h"
#include "Components/SceneComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"

#include "Combat/Unit/CombatUnitCharacter.h"
#include "CombatPlayerController.h"

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

	// Controller 在输入完成后只推进一次，避免 Pawn Tick 与输入回调重复移动镜头。
	PrimaryActorTick.bCanEverTick = false;
}

void ACombatCharacter::BeginPlay()
{
	Super::BeginPlay();
	SetActorEnableCollision(false);
}

void ACombatCharacter::EndPlay(const EEndPlayReason::Type Reason)
{
	ResetCameraInput();
	FollowTarget.Reset();
	Super::EndPlay(Reason);
}

bool ACombatCharacter::CanUpdateLocalCamera() const
{
	const APlayerController* PC = Cast<APlayerController>(GetController());
	return GetNetMode() != NM_DedicatedServer && PC && PC->IsLocalController() && PC->GetPawn() == this;
}

void ACombatCharacter::ResetCameraInput()
{
	FollowPressSerial = 0;
	PanVelocity = FVector::ZeroVector;
	CameraMode = ECombatCameraMode::Free;
	bWasAtEdge = false;
	bIgnoreEdgeUntilExit = false;
}

void ACombatCharacter::SetFollowTarget(ACombatUnitCharacter* NewTarget, const int32 BindingGeneration)
{
	if (!CanUpdateLocalCamera()) return;
	const ACombatPlayerController* PC = Cast<ACombatPlayerController>(GetController());
	if (NewTarget && (!IsValid(NewTarget) || !PC || PC->GetCommandedUnit() != NewTarget
		|| NewTarget->GetCommandingPlayerController() != PC)) NewTarget = nullptr;
	if (FollowTarget.Get() == NewTarget && CameraBindingGeneration == BindingGeneration) return;
	const bool bNewTarget = FollowTarget.Get() != NewTarget;
	ResetCameraInput();
	FollowTarget = NewTarget;
	CameraBindingGeneration = BindingGeneration;
	if (bNewTarget && NewTarget)
	{
		SetActorLocation(ClampCameraLocation(NewTarget->GetActorLocation()), false, nullptr, ETeleportType::TeleportPhysics);
	}
}

uint64 ACombatCharacter::BeginCameraFollow()
{
	if (!CanUpdateLocalCamera() || FollowPressSerial != 0) return 0;
	const ACombatPlayerController* PC = Cast<ACombatPlayerController>(GetController());
	ACombatUnitCharacter* Target = FollowTarget.Get();
	if (!Target || !PC || PC->GetCommandedUnit() != Target || Target->GetCommandingPlayerController() != PC
		|| PC->GetCommandBindingGeneration() != CameraBindingGeneration) return 0;
	if (++NextFollowSerial == 0) ++NextFollowSerial;
	FollowPressSerial = NextFollowSerial;
	bIgnoreEdgeUntilExit = bWasAtEdge;
	PanVelocity = FVector::ZeroVector;
	CameraMode = ECombatCameraMode::FollowHeld;
	return FollowPressSerial;
}

void ACombatCharacter::EndCameraFollow(const uint64 PressSerial)
{
	if (PressSerial == 0 || PressSerial != FollowPressSerial) return;
	FollowPressSerial = 0;
	bIgnoreEdgeUntilExit = false;
	if (CameraMode == ECombatCameraMode::FollowHeld) CameraMode = ECombatCameraMode::Free;
}

FVector ACombatCharacter::ClampCameraLocation(FVector Location) const
{
	if (bClampCameraBounds && !CameraBoundsMin.ContainsNaN() && !CameraBoundsMax.ContainsNaN()
		&& CameraBoundsMin.X < CameraBoundsMax.X && CameraBoundsMin.Y < CameraBoundsMax.Y)
	{
		Location.X = FMath::Clamp(Location.X, CameraBoundsMin.X, CameraBoundsMax.X);
		Location.Y = FMath::Clamp(Location.Y, CameraBoundsMin.Y, CameraBoundsMax.Y);
	}
	return Location;
}

FVector2D ACombatCharacter::GetEdgePanInput(const FVector2D Cursor, const FVector2D ViewportSize) const
{
	if (!bEnableEdgePan || EdgePanSpeed <= 0 || !FMath::IsFinite(EdgePanSpeed)
		|| Cursor.ContainsNaN() || ViewportSize.ContainsNaN() || !FMath::IsFinite(EdgePanScreenThreshold)
		|| ViewportSize.X <= 1 || ViewportSize.Y <= 1 || Cursor.X < 0 || Cursor.Y < 0
		|| Cursor.X > ViewportSize.X || Cursor.Y > ViewportSize.Y) return FVector2D::ZeroVector;
	const double Band = FMath::Min(ViewportSize.X, ViewportSize.Y) * FMath::Clamp(EdgePanScreenThreshold, 0.001f, 0.25f);
	const auto Strength = [Band](double Distance) { return FMath::Clamp(1.0 - Distance / Band, 0.0, 1.0); };
	return FVector2D(Strength(ViewportSize.X - Cursor.X) - Strength(Cursor.X),
		Strength(Cursor.Y) - Strength(ViewportSize.Y - Cursor.Y));
}

void ACombatCharacter::UpdateCamera(const float DeltaSeconds, FVector2D EdgeInput, const bool bAllowEdgePan)
{
	if (!CanUpdateLocalCamera()) { ResetCameraInput(); return; }
	if (!FMath::IsFinite(DeltaSeconds) || DeltaSeconds <= 0) return;
	const ACombatPlayerController* PC = Cast<ACombatPlayerController>(GetController());
	ACombatUnitCharacter* Target = FollowTarget.Get();
	if (CameraMode == ECombatCameraMode::FollowHeld && (!Target || !PC || PC->GetCommandedUnit() != Target
		|| Target->GetCommandingPlayerController() != PC || CameraBindingGeneration != PC->GetCommandBindingGeneration()))
	{
		ResetCameraInput();
	}
	const bool bCanPan = bAllowEdgePan && bEnableEdgePan && FMath::IsFinite(EdgePanSpeed) && EdgePanSpeed > 0;
	if (!bCanPan || EdgeInput.ContainsNaN()) { EdgeInput = FVector2D::ZeroVector; PanVelocity = FVector::ZeroVector; }
	EdgeInput.X = FMath::Clamp(EdgeInput.X, -1.0, 1.0);
	EdgeInput.Y = FMath::Clamp(EdgeInput.Y, -1.0, 1.0);
	bWasAtEdge = !EdgeInput.IsNearlyZero();
	if (!bWasAtEdge) bIgnoreEdgeUntilExit = false;
	if (CameraMode == ECombatCameraMode::FollowHeld && bIgnoreEdgeUntilExit) EdgeInput = FVector2D::ZeroVector;
	FVector Displacement = FVector::ZeroVector;
	if (!EdgeInput.IsNearlyZero())
	{
		// 屏幕向上对应相机前向在地面的投影；各基向量先归一，俯仰不会使纵向速度缩水。
		const FVector Right = TopDownCameraComponent->GetRightVector().GetSafeNormal2D();
		const FVector Forward = TopDownCameraComponent->GetForwardVector().GetSafeNormal2D();
		const double Strength = FMath::Max(FMath::Abs(EdgeInput.X), FMath::Abs(EdgeInput.Y));
		PanVelocity = (Right * EdgeInput.X + Forward * EdgeInput.Y).GetSafeNormal2D() * EdgePanSpeed * Strength;
		CameraMode = ECombatCameraMode::EdgePan;
		Displacement = PanVelocity * DeltaSeconds;
	}
	else if (CameraMode == ECombatCameraMode::FollowHeld && Target)
	{
		FVector Destination = Target->GetActorLocation();
		Destination.Z = GetActorLocation().Z;
		const float Alpha = CameraFollowSpeed > 0 && FMath::IsFinite(CameraFollowSpeed)
			? 1.0f - FMath::Exp(-CameraFollowSpeed * DeltaSeconds) : 1.0f;
		Displacement = (Destination - GetActorLocation()) * Alpha;
	}
	else
	{
		if (bCanPan && EdgePanDeceleration > 0 && FMath::IsFinite(EdgePanDeceleration) && !PanVelocity.IsNearlyZero(0.1))
		{
			// 指数衰减的积分保证相同时间内的惯性位移不依赖帧率。
			const double Decay = FMath::Exp(-EdgePanDeceleration * DeltaSeconds);
			Displacement = PanVelocity * ((1.0 - Decay) / EdgePanDeceleration);
			PanVelocity *= Decay;
		}
		else PanVelocity = FVector::ZeroVector;
		CameraMode = PanVelocity.IsNearlyZero() ? ECombatCameraMode::Free : ECombatCameraMode::EdgePan;
	}
	if (!Displacement.ContainsNaN() && !Displacement.IsNearlyZero())
		SetActorLocation(ClampCameraLocation(GetActorLocation() + Displacement), false, nullptr, ETeleportType::None);
}
