#include "Combat/UI/CombatAbilityIndicatorGround.h"

#include "Components/PrimitiveComponent.h"
#include "GameFramework/Pawn.h"

bool CombatAbilityIndicatorGround::IsGroundHit(const FHitResult& Hit)
{
	const UPrimitiveComponent* Surface = Hit.GetComponent();
	return Hit.bBlockingHit && !Hit.bStartPenetrating && !Hit.Location.ContainsNaN()
		&& !Hit.ImpactNormal.ContainsNaN() && Hit.ImpactNormal.Z >= MinNormalZ
		&& IsValid(Surface) && !Cast<APawn>(Hit.GetActor())
		&& Surface->GetCollisionResponseToChannel(TraceChannel) == ECR_Block
		&& Surface->bRenderCustomDepth && (Surface->CustomDepthStencilValue & StencilBit) != 0;
}
