#include "Combat/Thinker/CombatThinker.h"

#include "Combat/Thinker/CombatThinkerSubsystem.h"

ACombatThinker::ACombatThinker()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetActorEnableCollision(false);
}

void ACombatThinker::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (HasAuthority() && !bSubsystemDestroying && ThinkerHandle.IsValid())
	{
		if (UCombatThinkerSubsystem* Thinkers = GetWorld() ? GetWorld()->GetSubsystem<UCombatThinkerSubsystem>() : nullptr)
		{
			Thinkers->NotifyThinkerEndPlay(ThinkerHandle);
		}
	}
	Super::EndPlay(EndPlayReason);
}
