#include "Combat/Defense/DefenseGameState.h"
#include "Combat/Defense/DefenseObjective.h"
#include "Net/UnrealNetwork.h"
void ADefenseGameState::Publish(const FDefenseRunView& Snapshot)
{
	if (!HasAuthority() || GetNetMode() == NM_Client) return;
	View = Snapshot;
	ForceNetUpdate();
}
void ADefenseGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ADefenseGameState, View);
}
