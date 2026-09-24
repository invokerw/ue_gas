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

bool UCombatCharacterMovementComponent::IsWalkable(const FHitResult& Hit) const
{
	// CanCharacterStepUpOn 只禁止主动跨上胶囊，不能阻止 FindFloor/落地将其当作地面。
	// Character 随后的 BaseChange 会对不可站立 Pawn 调用 JumpOff，额外注入水平和向上速度。
	return !Cast<ACombatUnitCharacter>(Hit.GetActor()) && Super::IsWalkable(Hit);
}
