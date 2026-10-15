#include "Combat/Tests/CombatDefenseNetworkScenario.h"
#include "Combat/Defense/DefenseGameState.h"
#include "Combat/Defense/DefenseObjective.h"
#include "Combat/Combat/CombatDamageSubsystem.h"
#include "Combat/AI/CombatAIBrainComponent.h"
#include "Combat/Attributes/CombatAttributeSet.h"
#include "Combat/Unit/CombatUnitLifecycleComponent.h"
#include "Combat/Unit/CombatProgressionComponent.h"
#include "Combat/Economy/CombatEconomyComponent.h"
#include "CombatPlayerController.h"
#include "EngineUtils.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
ACombatDefenseNetworkScenario::ACombatDefenseNetworkScenario()
{ PrimaryActorTick.bCanEverTick=true; bReplicates=true; bAlwaysRelevant=true; }
void ACombatDefenseNetworkScenario::BeginPlay()
{
	Super::BeginPlay();
#if UE_BUILD_SHIPPING
	SetActorTickEnabled(false);
#else
	SetActorTickEnabled(FParse::Param(FCommandLine::Get(),TEXT("DefenseSmoke")));
	FParse::Value(FCommandLine::Get(),TEXT("DefenseSmokePlayers="),ExpectedPlayers);
#endif
}
void ACombatDefenseNetworkScenario::Kill(ACombatUnitCharacter* Unit)
{
	FCombatDamageRequest R; R.Source=Unit; R.Target=Unit; R.Amount=1000000; R.DamageType=ECombatDamageType::Pure;
	GetWorld()->GetSubsystem<UCombatDamageSubsystem>()->DealDamage(R);
}
void ACombatDefenseNetworkScenario::Finish(const bool bPass,const FString& Detail)
{
	if(bDone) return; bDone=true; SetActorTickEnabled(false);
	UE_LOG(LogTemp,Display,TEXT("DefenseSmoke Result=%s NetMode=%d %s"),bPass?TEXT("Pass"):TEXT("Fail"),int32(GetNetMode()),*Detail);
}
void ACombatDefenseNetworkScenario::Tick(float Delta)
{
	Super::Tick(Delta); Elapsed+=Delta;
	if(Elapsed>240) { Finish(false,TEXT("Timeout")); return; }
	auto* State=GetWorld()->GetGameState<ADefenseGameState>();
	if(!State || !State->GetRunView().RunId.IsValid()) return;
	const auto& V=State->GetRunView();
	const bool DefeatCase=FParse::Param(FCommandLine::Get(),TEXT("DefenseSmokeDefeat"));
	if(GetNetMode()==NM_Client)
	{
		for(TActorIterator<ACombatPlayerController> It(GetWorld());It;++It)
		{
			if(!It->IsLocalController()) continue;
			auto* Hero=It->GetCommandedUnit(); if(!Hero || Hero->GetOwner()!=*It) continue;
			if(!bSentOrder)
			{
				FCombatOrderBatchRequest R; R.RequestId=19000001;
				FCombatOrderRequest Move; Move.Type=ECombatOrderType::MoveToPoint;
				Move.bHasTargetLocation=true; Move.TargetLocation=FVector(1100,300,0); R.Orders.Add(Move);
				Hero->ServerIssueOrderBatch(R); bSentOrder=true;
			}
			bRPC |= Hero->GetLastOrderBatchResult().bAccepted;
			if(Hero->HasAuthority() || Hero->GetCombatAIBrainComponent()->IsRunning()) { Finish(false,TEXT("ClientAuthorityLeak")); return; }
		}
	}
	else
	{
		TArray<ACombatUnitCharacter*> Heroes;
		for(TActorIterator<ACombatPlayerController> It(GetWorld());It;++It) if(It->GetCommandedUnit()) Heroes.Add(It->GetCommandedUnit());
		if(V.Phase==EDefensePhase::WaveCombat && Heroes.Num()!=ExpectedPlayers)
		{ Finish(false,FString::Printf(TEXT("Players=%d Expected=%d"),Heroes.Num(),ExpectedPlayers)); return; }
		if(V.Phase==EDefensePhase::WaveCombat && LastWave!=V.WaveIndex)
		{
			LastWave=V.WaveIndex; WaveStarted=Elapsed; CoreAtWaveStart=V.ObjectiveHealth; InitialPositions.Reset();
			if(LastWave==1 && !Heroes.IsEmpty())
			{ RespawnHero=Heroes[0]; PreviousLife=Heroes[0]->GetLifeGeneration(); Kill(Heroes[0]); }
		}
		if(RespawnHero.IsValid() && RespawnHero->GetLifeGeneration()>PreviousLife && RespawnHero->GetLifeState()==ECombatLifeState::Alive) bRespawn=true;
		if(V.Phase==EDefensePhase::WaveCombat)
		{
			TArray<ACombatUnitCharacter*> Enemies;
			for(TActorIterator<ADefenseEnemy> It(GetWorld());It;++It)
			{
				if(It->IsA<ADefenseObjective>() || It->GetLifeState()!=ECombatLifeState::Alive) continue;
				Enemies.Add(*It);
				if(const FVector* Initial=InitialPositions.Find(*It)) bSawMovement |= FVector::Dist2D(*Initial,It->GetActorLocation())>200;
				else InitialPositions.Add(*It,It->GetActorLocation());
			}
			const bool WaveDamage=V.ObjectiveHealth<CoreAtWaveStart-1;
			bSawDamage |= WaveDamage;
			if(V.PendingSpawns==0 && WaveDamage && Elapsed-WaveStarted>10 && ClearedWaves==V.WaveIndex)
			{
				if(DefeatCase) Kill(V.Objective);
				else { for(auto* Enemy:Enemies) Kill(Enemy); ++ClearedWaves; }
			}
		}
	}
	if(V.Phase==EDefensePhase::Victory || V.Phase==EDefensePhase::Defeat)
	{
		const bool Expected=DefeatCase?V.Phase==EDefensePhase::Defeat:V.Phase==EDefensePhase::Victory;
		bool Rewards=true;
		if(GetNetMode()!=NM_Client && !DefeatCase)
			for(TActorIterator<ACombatPlayerController> It(GetWorld());It;++It)
				Rewards &= It->GetCombatEconomyComponent()->GetGold()==1370 && It->GetCommandedUnit()
					&& It->GetCommandedUnit()->GetCombatProgressionComponent()->GetExperience()==2000;
		const bool Evidence=GetNetMode()==NM_Client ? bRPC : bSawMovement && bSawDamage && (DefeatCase || (ClearedWaves==6 && bRespawn));
		Finish(Expected && Evidence && Rewards,FString::Printf(TEXT("Wave=%d CoreHP=%.0f Movement=%d Damage=%d Respawn=%d RPC=%d Cleared=%d Rewards=%d Reason=%s"),
			V.WaveIndex,V.ObjectiveHealth,bSawMovement,bSawDamage,bRespawn,bRPC,ClearedWaves,Rewards,*V.TerminalReason.ToString()));
	}
}
