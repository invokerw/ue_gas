#include "Combat/Demo/CombatDemoAbilities.h"

void UCombatChannelProbeAbility::ReceiveChannelTick_Implementation(
	const FCombatAbilityActivationContext& Context,
	const FCombatScheduledTickContext& TickContext)
{
	(void)Context;
	ObservedTickCount += TickContext.TickCount;
}

void UCombatChannelProbeAbility::ReceiveChannelFinish_Implementation(
	const FCombatAbilityActivationContext& Context,
	const bool bInterrupted)
{
	(void)Context;
	++ObservedFinishCount;
	bLastFinishInterrupted = bInterrupted;
}
