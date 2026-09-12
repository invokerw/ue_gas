#include "Combat/Order/CombatOrderMoveGoalContext.h"

#include "EnvironmentQuery/Items/EnvQueryItemType_Point.h"

#include "Combat/Order/CombatOrderComponent.h"
#include "Combat/Unit/CombatUnitCharacter.h"

void UCombatOrderMoveGoalContext::ProvideContext(
	FEnvQueryInstance& QueryInstance,
	FEnvQueryContextData& ContextData) const
{
	const ACombatUnitCharacter* Unit = Cast<ACombatUnitCharacter>(QueryInstance.Owner.Get());
	const UCombatOrderComponent* Orders = Unit ? Unit->GetCombatOrderComponent() : nullptr;
	if (Orders)
	{
		UEnvQueryItemType_Point::SetContextHelper(ContextData, Orders->GetCurrentMoveGoal());
	}
}
