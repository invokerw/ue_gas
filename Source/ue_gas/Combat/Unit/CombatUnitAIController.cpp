#include "Combat/Unit/CombatUnitAIController.h"

#include "Navigation/CrowdFollowingComponent.h"

#include "Combat/Ability/CombatAbilitySystemComponent.h"
#include "Combat/Core/CombatTags.h"
#include "Combat/Motion/CombatMotionComponent.h"
#include "Combat/Order/CombatOrderComponent.h"
#include "Combat/Unit/CombatUnitCharacter.h"

ACombatUnitAIController::ACombatUnitAIController(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UCrowdFollowingComponent>(TEXT("PathFollowingComponent")))
{
	bReplicates = false;

	// 普通移动统一由 Detour Crowd 计算绕行方向；不再叠加 CharacterMovement 的 RVO 避让，避免两套算法反复修正同一速度。
	if (UCrowdFollowingComponent* Crowd = GetCombatCrowdFollowing())
	{
		Crowd->SetCrowdAnticipateTurns(true, false);
		Crowd->SetCrowdObstacleAvoidance(true, false);
		Crowd->SetCrowdSeparation(true, false);
		Crowd->SetCrowdOptimizeVisibility(true, false);
		Crowd->SetCrowdOptimizeTopology(true, false);
		Crowd->SetCrowdPathOffset(true, false);
		Crowd->SetCrowdSlowdownAtGoal(true, false);
		Crowd->SetCrowdSeparationWeight(2.0f, false);
		Crowd->SetCrowdCollisionQueryRange(600.0f, false);
		Crowd->SetCrowdPathOptimizationRange(3000.0f, false);
		Crowd->SetCrowdAvoidanceQuality(ECrowdAvoidanceQuality::Good, false);
		Crowd->SetCrowdAvoidanceRangeMultiplier(1.0f, false);
		Crowd->SetCrowdRotateToVelocity(true);
	}
}

void ACombatUnitAIController::RefreshCrowdParticipation()
{
	if (!HasAuthority())
	{
		return;
	}
	UCrowdFollowingComponent* Crowd = GetCombatCrowdFollowing();
	ACombatUnitCharacter* Unit = Cast<ACombatUnitCharacter>(GetPawn());
	if (!Crowd)
	{
		return;
	}

	ECrowdSimulationState DesiredState = ECrowdSimulationState::Disabled;
	if (Unit && Unit->GetLifeState() == ECombatLifeState::Alive)
	{
		const UCombatAbilitySystemComponent* Asc = Unit->GetCombatAbilitySystemComponent();
		const bool bIgnoresUnits = Asc && Asc->HasMatchingGameplayTag(CombatTags::State_NoUnitCollision);
		const UCombatMotionComponent* Motion = Unit->GetCombatMotionComponent();
		if (!bIgnoresUnits)
		{
			// 定身、眩晕或强制位移时，其他单位仍应绕开它，但人群导航不能继续驱动它移动。
			DesiredState = Unit->IsMovementBlocked() || (Motion && Motion->HasActiveMotion())
				? ECrowdSimulationState::ObstacleOnly
				: ECrowdSimulationState::Enabled;
		}
	}

	DesiredCrowdSimulationState = DesiredState;
	Crowd->SetCrowdSimulationState(DesiredState);
	Crowd->SuspendCrowdSteering(DesiredState != ECrowdSimulationState::Enabled);
}

UCrowdFollowingComponent* ACombatUnitAIController::GetCombatCrowdFollowing() const
{
	return Cast<UCrowdFollowingComponent>(GetPathFollowingComponent());
}

bool ACombatUnitAIController::IsCrowdSteeringActive() const
{
	const UCrowdFollowingComponent* Crowd = GetCombatCrowdFollowing();
	return Crowd && Crowd->IsCrowdSimulationActive();
}

void ACombatUnitAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	RefreshCrowdParticipation();
	if (ACombatUnitCharacter* Unit = Cast<ACombatUnitCharacter>(InPawn))
	{
		if (UCombatOrderComponent* Orders = Unit->GetCombatOrderComponent())
		{
			Orders->RefreshControllerBinding();
		}
		Unit->LogServerMovementTopology(TEXT("AIControllerPossess"));
	}
}

void ACombatUnitAIController::OnUnPossess()
{
	if (UCrowdFollowingComponent* Crowd = GetCombatCrowdFollowing())
	{
		DesiredCrowdSimulationState = ECrowdSimulationState::Disabled;
		Crowd->SuspendCrowdSteering(true);
		Crowd->SetCrowdSimulationState(ECrowdSimulationState::Disabled);
	}
	Super::OnUnPossess();
}
