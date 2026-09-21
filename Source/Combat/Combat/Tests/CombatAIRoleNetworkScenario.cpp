#include "Combat/Tests/CombatAIRoleNetworkScenario.h"
#include "Combat/AI/CombatAIBrainComponent.h"
#include "Combat/Attack/CombatAttackComponent.h"
#include "Combat/Attributes/CombatAttributeSet.h"
#include "Combat/Demo/CombatAIRoleDemoArena.h"
#include "Combat/Unit/CombatUnitCharacter.h"
#include "EngineUtils.h"

ACombatAIRoleNetworkScenario::ACombatAIRoleNetworkScenario() { PrimaryActorTick.bCanEverTick = true; }
void ACombatAIRoleNetworkScenario::Finish(bool bPassed, const FString& Detail)
{
	UE_LOG(LogTemp, Display, TEXT("AIRolesNetworkSmoke Result=%s NetMode=%d %s"), bPassed ? TEXT("Pass") : TEXT("Fail"), int32(GetNetMode()), *Detail);
	SetActorTickEnabled(false);
}
void ACombatAIRoleNetworkScenario::Tick(float DeltaSeconds)
{
	Elapsed += DeltaSeconds;
	if (Elapsed > 70) { Finish(false, TEXT("Timed out waiting for role damage/movement/return")); return; }
	for (TActorIterator<ACombatAIRoleDemoArena> It(GetWorld()); It; ++It)
	{
		if (!IsValid(It->GuardAgent) || !IsValid(It->LaneAgent) || !IsValid(It->GuardTarget) || !IsValid(It->LaneTarget)) continue;
		auto* Guard = It->GuardAgent->GetCombatAIBrainComponent();
		auto* Lane = It->LaneAgent->GetCombatAIBrainComponent();
		bGuardMoved |= FVector::Dist2D(It->GuardAgent->GetActorLocation(), It->GetActorLocation() + FVector(0, -600, 0)) > 100;
		bLaneMoved |= FVector::Dist2D(It->LaneAgent->GetActorLocation(), It->GetLaneStart()) > 100;
		const auto* GuardHealth = It->GuardTarget->GetCombatAttributeSet();
		const auto* LaneHealth = It->LaneTarget->GetCombatAttributeSet();
		const bool bDamage = GuardHealth->GetHealth() < GuardHealth->GetMaxHealth() && LaneHealth->GetHealth() < LaneHealth->GetMaxHealth();
		if (GetNetMode() == NM_Client)
		{
			if (Elapsed > 18 && bDamage && bLaneMoved)
				Finish(!Guard->IsRunning() && !Lane->IsRunning() && !Guard->HasPerceptionSchedule() && !Lane->HasPerceptionSubscription()
					&& Guard->GetRunSerial() == 0 && Lane->GetRunSerial() == 0 && !It->GuardAgent->HasAuthority(),
					FString::Printf(TEXT("ClientBrain=Stopped LaneMoved=1 GuardHP=%.1f LaneHP=%.1f"), GuardHealth->GetHealth(), LaneHealth->GetHealth()));
		}
		else if (Guard->GetHomeCommitCount() > 0 && Lane->GetRouteCommitCount() > 0)
		{
			Finish(Guard->IsRunning() && Lane->IsRunning() && bDamage && bGuardMoved && bLaneMoved,
				FString::Printf(TEXT("ServerBrain=Running GuardMoved=%d LaneMoved=%d HomeCommits=%llu RouteCommits=%llu"),
					bGuardMoved, bLaneMoved, Guard->GetHomeCommitCount(), Lane->GetRouteCommitCount()));
		}
	}
}
