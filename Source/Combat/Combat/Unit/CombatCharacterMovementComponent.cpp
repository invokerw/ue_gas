#include "Combat/Unit/CombatCharacterMovementComponent.h"

#include "Combat/Order/CombatOrderComponent.h"
#include "Combat/Unit/CombatUnitCharacter.h"

UCombatCharacterMovementComponent::UCombatCharacterMovementComponent()
{
	bOrientRotationToMovement = true;
}

void UCombatCharacterMovementComponent::TickComponent(
	const float DeltaTime, const ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	ACombatUnitCharacter* Unit = Cast<ACombatUnitCharacter>(CharacterOwner);
	UCombatOrderComponent* Orders = Unit ? Unit->GetCombatOrderComponent() : nullptr;
	TGuardValue<bool> FacingGuard(bUpdatingOrderFacing,
		Unit && Unit->HasAuthority() && Orders && Orders->GetCurrentState() == ECombatOrderState::Facing);
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	FVector Direction;
	if (!bUpdatingOrderFacing || !HasValidData() || !IsValid(Unit) || !Unit->HasAuthority()
		|| !IsValid(Orders) || !Orders->GetFacingDirection(Direction)
		|| Direction.IsNearlyZero()
		|| !FMath::IsFinite(DeltaTime) || DeltaTime <= 0.0f
		|| !FMath::IsFinite(RotationRate.Yaw) || RotationRate.Yaw <= 0.0f
		|| UpdatedComponent->IsSimulatingPhysics())
	{
		return;
	}

	// 放在普通移动之后，既覆盖定身的 MOVE_None，也避免残余加速度与原地转向争用朝向。
	FRotator Rotation = UpdatedComponent->GetComponentRotation();
	Rotation.Yaw = FMath::FixedTurn(Rotation.Yaw, Direction.Rotation().Yaw, GetDeltaRotation(DeltaTime).Yaw);
	MoveUpdatedComponent(FVector::ZeroVector, Rotation, false);
}

void UCombatCharacterMovementComponent::PhysicsRotation(const float DeltaTime)
{
	if (!bUpdatingOrderFacing)
	{
		Super::PhysicsRotation(DeltaTime);
	}
}
