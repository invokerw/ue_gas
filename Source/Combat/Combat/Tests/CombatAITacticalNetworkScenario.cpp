#include "Combat/Tests/CombatAITacticalNetworkScenario.h"

#include "Combat/AI/CombatAIBrainComponent.h"
#include "Combat/AI/CombatAIWorldSubsystem.h"
#include "Combat/Attributes/CombatAttributeSet.h"
#include "Combat/Demo/CombatAITacticalDemoArena.h"
#include "Combat/Unit/CombatUnitCharacter.h"
#include "Combat/View/CombatUnitViewComponent.h"
#include "EngineUtils.h"

ACombatAITacticalNetworkScenario::ACombatAITacticalNetworkScenario()
{
	PrimaryActorTick.bCanEverTick = true;
}

void ACombatAITacticalNetworkScenario::Finish(const bool bPassed, const FString& Detail)
{
	UE_LOG(LogTemp, Display, TEXT("AITacticsNetworkSmoke Result=%s NetMode=%d %s"),
		bPassed ? TEXT("Pass") : TEXT("Fail"), int32(GetNetMode()), *Detail);
	SetActorTickEnabled(false);
}

void ACombatAITacticalNetworkScenario::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Elapsed += DeltaSeconds;
	if (Elapsed > 75.0f)
	{
		Finish(false, TEXT("Timed out waiting for tactical cast, EQS movement and replicated damage"));
		return;
	}

	if (GetNetMode() == NM_Client)
	{
		ACombatUnitCharacter* Hero = nullptr;
		ACombatUnitCharacter* Ranged = nullptr;
		int32 DamagedTargets = 0;
		for (TActorIterator<ACombatUnitCharacter> It(GetWorld()); It; ++It)
		{
			const FCombatUnitView& UnitView = It->GetCombatUnitViewComponent()->GetUnitView();
			const FPrimaryAssetId Id = UnitView.UnitDefinitionId;
			if (Id == FPrimaryAssetId(TEXT("CombatUnit"), TEXT("ai_tactical_hero_unit"))) Hero = *It;
			else if (Id == FPrimaryAssetId(TEXT("CombatUnit"), TEXT("ai_tactical_ranged_unit"))) Ranged = *It;
			else if (Id == FPrimaryAssetId(TEXT("CombatUnit"), TEXT("wooden_dummy")))
			{
				DamagedTargets += UnitView.Health < UnitView.MaxHealth ? 1 : 0;
			}
		}
		if (Elapsed < 20.0f || !Hero || !Ranged || DamagedTargets < 2)
		{
			return;
		}
		const auto* HeroBrain = Hero->GetCombatAIBrainComponent();
		const auto* RangedBrain = Ranged->GetCombatAIBrainComponent();
		const bool bRangedMoved = FMath::Abs(Ranged->GetActorLocation().X) > 300.0f;
		Finish(!Hero->HasAuthority() && !Ranged->HasAuthority() && !HeroBrain->IsRunning() && !RangedBrain->IsRunning()
			&& HeroBrain->GetRunSerial() == 0 && RangedBrain->GetRunSerial() == 0 && bRangedMoved,
			FString::Printf(TEXT("ClientBrains=Stopped RangedMoved=%d DamagedTargets=%d Hero=%s Ranged=%s"),
				bRangedMoved ? 1 : 0, DamagedTargets, *Hero->GetActorLocation().ToCompactString(),
				*Ranged->GetActorLocation().ToCompactString()));
		return;
	}

	for (TActorIterator<ACombatAITacticalDemoArena> It(GetWorld()); It; ++It)
	{
		if (!IsValid(It->HeroAgent) || !IsValid(It->RangedAgent) || !IsValid(It->HeroTarget) || !IsValid(It->RangedTarget))
		{
			continue;
		}
		const auto* HeroBrain = It->HeroAgent->GetCombatAIBrainComponent();
		const auto* RangedBrain = It->RangedAgent->GetCombatAIBrainComponent();
		const UCombatAttributeSet* HeroAttributes = It->HeroAgent->GetCombatAttributeSet();
		const UCombatAttributeSet* HeroTargetAttributes = It->HeroTarget->GetCombatAttributeSet();
		const UCombatAttributeSet* RangedTargetAttributes = It->RangedTarget->GetCombatAttributeSet();
		const bool bRangedMoved = FVector::Dist2D(It->RangedAgent->GetActorLocation(), It->GetRangedStart()) > 300.0f;
		const bool bBothTargetsDamaged = HeroTargetAttributes->GetHealth() < HeroTargetAttributes->GetMaxHealth()
			&& RangedTargetAttributes->GetHealth() < RangedTargetAttributes->GetMaxHealth();
		const bool bHeroHealed = It->WasHeroInjuryApplied()
			&& HeroAttributes->GetHealth() > It->GetHeroHealthAfterInjury();
		const FCombatAIWorldBudgetSnapshot Budget = GetWorld()->GetSubsystem<UCombatAIWorldSubsystem>()->GetSnapshot();
		if (It->GetHeroAttackLaunchCount() > 0 && It->GetHeroCastAfterInjuryCount() > 0 && bHeroHealed
			&& bRangedMoved && bBothTargetsDamaged && Budget.EQSSucceeded > 0)
		{
			Finish(HeroBrain->IsRunning() && RangedBrain->IsRunning() && Budget.ActiveEQS == 0
				&& HeroBrain->GetResolvedCount() >= 2 && RangedBrain->GetSubmittedCount() >= 2,
				FString::Printf(TEXT("ServerBrains=Running HeroLaunches=%llu HeroCasts=%llu HeroHealed=%d RangedMoved=%d EQSSuccess=%llu ActiveEQS=%d HeroOrders=%llu RangedOrders=%llu"),
					It->GetHeroAttackLaunchCount(), It->GetHeroCastAfterInjuryCount(), bHeroHealed ? 1 : 0, bRangedMoved ? 1 : 0,
					Budget.EQSSucceeded, Budget.ActiveEQS, HeroBrain->GetSubmittedCount(), RangedBrain->GetSubmittedCount()));
		}
		return;
	}
}
