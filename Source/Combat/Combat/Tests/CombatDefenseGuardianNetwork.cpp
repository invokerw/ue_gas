#include "Combat/Tests/CombatDefenseNetworkScenario.h"
#include "Combat/Defense/DefenseGuardian.h"
#include "Combat/Defense/DefenseGameState.h"
#include "Combat/Defense/DefensePlayerController.h"
#include "Combat/Defense/DefenseHeroSelectionComponent.h"
#include "Combat/Ability/CombatAbilitySystemComponent.h"
#include "Combat/Attributes/CombatAttributeSet.h"
#include "Combat/Modifiers/CombatModifierComponent.h"
#include "Combat/Order/CombatOrderComponent.h"
#include "Combat/View/CombatUnitViewComponent.h"
#include "Combat/Core/CombatTags.h"
#include "Combat/Log/CombatEventSubsystem.h"
#include "EngineUtils.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

void ACombatDefenseNetworkScenario::OnGuardianCombatRecord(const FCombatLogRecord& R)
{
	if(!GuardianSmokeHero) return;
	if(R.EventType==CombatTags::Event_Combat_AbilitySpellStarted&&R.SourceActorId==GuardianSmokeHero->GetUniqueID()
		&&R.Source.AbilityDefinitionId.PrimaryAssetName==TEXT("guardian_fortify")) GuardianSkillMask|=2;
	if(R.EventType!=CombatTags::Event_Combat_DamageApplied) return;
	if(R.TargetActorId==GuardianSmokeHero->GetUniqueID()&&R.AbsorbedAmount>0) bGuardianAbsorbed=true;
	if(!GuardianTrainingTarget||R.TargetActorId!=GuardianTrainingTarget->GetUniqueID()||R.SourceActorId!=GuardianSmokeHero->GetUniqueID()||R.AppliedAmount<=0) return;
	if(R.Source.AbilityDefinitionId.PrimaryAssetName==TEXT("guardian_shield_bash")) GuardianSkillMask|=1;
	if(R.Source.AbilityDefinitionId.PrimaryAssetName==TEXT("guardian_ground_slam")) GuardianSkillMask|=4;
}
void ACombatDefenseNetworkScenario::ExerciseGuardian()
{
	const auto* State=GetWorld()->GetGameState<ADefenseGameState>(); if(!State) return;
	if(GetNetMode()!=NM_Client)
	{
		TArray<ADefensePlayerController*> Players;
		for(TActorIterator<ADefensePlayerController> It(GetWorld());It;++It)
		{
			Players.Add(*It); auto* H=It->GetCommandedUnit();
			if(H&&H->GetUnitDefinitionId()==FPrimaryAssetId(TEXT("CombatUnit"),TEXT("defense_guardian"))) GuardianSmokeHero=H;
		}
		if(!GuardianSmokeHero) return;
		if(!GuardianTrainingTarget)
		{
			FActorSpawnParameters P; P.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			GuardianTrainingTarget=GetWorld()->SpawnActor<ACombatUnitCharacter>(GuardianSmokeHero->GetActorLocation()+FVector(200,80,0),FRotator::ZeroRotator,P);
			auto* Data=NewObject<UCombatUnitData>(GuardianTrainingTarget); Data->DefinitionName=TEXT("guardian_network_target"); Data->InitialTeamId=FCombatTeamId(2);
			Data->BaseStats.MaxHealth=10000; Data->BaseStats.AttackDamage=10; Data->BaseStats.AttackRange=600; Data->BaseStats.BaseAttackTime=.6f; Data->BaseAttackPoint=.1f; Data->AttackFacingToleranceDegrees=180; Data->ExperienceReward=0;
			GuardianTrainingTarget->InitializeFromUnitData(Data);
			FCombatOrderRequest Attack; Attack.Type=ECombatOrderType::AttackTarget; Attack.TargetUnit=GuardianSmokeHero; GuardianTrainingTarget->GetCombatOrderComponent()->IssueOrder(Attack); ForceNetUpdate();
		}
		if(!bGuardianSecurity&&Players.Num()==ExpectedPlayers)
		{
			auto* HeroOwner=Cast<ADefensePlayerController>(GuardianSmokeHero->GetCommandingPlayerController()); auto* S=HeroOwner?HeroOwner->GetHeroSelection():nullptr;
			if(S&&!S->Receipts.IsEmpty())
			{
				const auto Original=S->Receipts[0].Request;
				const auto Replay=S->ProcessForConnection(HeroOwner,Original);
				auto Changed=Original; ++Changed.BindingGeneration; const auto Mismatch=S->ProcessForConnection(HeroOwner,Changed);
				bool Foreign=true; for(auto* PC:Players) if(PC!=HeroOwner) Foreign &= !S->ProcessForConnection(PC,Original).bLastSuccess;
				int32 Count=0; for(TActorIterator<ADefenseGuardianHero> It(GetWorld());It;++It) if(!It->IsActorBeingDestroyed()) ++Count;
				bGuardianSecurity=Replay.bLastSuccess&&!Mismatch.bLastSuccess&&Mismatch.FailureReason==TEXT("RequestPayloadMismatch")&&Foreign&&Count==1;
				if(!bGuardianSecurity) { Finish(false,TEXT("GuardianSelectionReplayOrOwnerFailure")); return; }
				UE_LOG(LogTemp,Display,TEXT("GuardianSelectionSecurity Replay=1 Mismatch=1 ForeignOwner=1 ExactlyOneHero=1"));
			}
		}
	}
	if(GuardianSmokeHero)
	{
		const auto Views=GuardianSmokeHero->GetCombatUnitViewComponent()->GetVisibleModifiers();
		for(const auto& M:Views) { bGuardianWSeen|=M.DefinitionId.PrimaryAssetName==TEXT("guardian_fortify_shield"); bGuardianPassiveSeen|=M.DefinitionId.PrimaryAssetName==TEXT("guardian_resolve_shield"); }
		if(GetNetMode()==NM_Client&&(GuardianSmokeHero->HasAuthority()||GuardianSmokeHero->GetCombatModifierComponent()->GetActiveModifierCount()!=0)) { Finish(false,TEXT("GuardianClientAuthorityLeak")); return; }
	}
	if(GuardianTrainingTarget)
	{
		bGuardianDamageSeen|=GuardianTrainingTarget->GetCombatAttributeSet()->GetHealth()<9999;
		for(const auto& M:GuardianTrainingTarget->GetCombatUnitViewComponent()->GetVisibleModifiers()) bGuardianStunSeen|=M.DefinitionId.PrimaryAssetName==TEXT("guardian_slam_stun");
	}
	if(GetNetMode()!=NM_Client)
	{
		if(!bGuardianCombatReady&&GuardianSkillMask==7&&bGuardianWSeen&&bGuardianPassiveSeen&&bGuardianStunSeen&&bGuardianAbsorbed)
		{
			bGuardianCombatReady=true; GuardianTrainingTarget->GetCombatOrderComponent()->StopAllOrders(CombatTags::Order_Failure_Cancelled); GuardianSmokeHero->GetCombatOrderComponent()->StopAllOrders(CombatTags::Order_Failure_Cancelled);
			UE_LOG(LogTemp,Display,TEXT("GuardianActualSkills QDamage=1 WAbsorb=1 EStun=1 PassiveAttackShield=1"));
		}
		return;
	}
	int32 Seat=0; FParse::Value(FCommandLine::Get(),TEXT("DefenseSmokeSeat="),Seat); if(Seat!=1) return;
	for(TActorIterator<ADefensePlayerController> It(GetWorld());It;++It)
	{
		if(!It->IsLocalController()) continue; auto* PC=*It; auto* S=PC->GetHeroSelection(); auto* Hero=PC->GetCommandedUnit(); if(!S||!Hero||Hero->GetOwner()!=PC) continue;
		if(!bGuardianRequestSent&&S->GetView().bCanChoose)
		{
			FDefenseHeroChoiceRequest R; R.RequestId=PC->ReserveCombatRequestId(); R.RunId=State->GetRunView().RunId; R.BindingGeneration=PC->GetCommandBindingGeneration(); R.LifeGeneration=Hero->GetLifeGeneration();
			S->ServerChooseGuardian(R); S->ServerChooseGuardian(R); auto Mismatch=R; ++Mismatch.BindingGeneration; S->ServerChooseGuardian(Mismatch);
			auto Stale=R; Stale.RequestId=PC->ReserveCombatRequestId(); S->ServerChooseGuardian(Stale); bGuardianRequestSent=true; return;
		}
		if(!S->GetView().bGuardianSelected||Hero!=GuardianSmokeHero||!GuardianTrainingTarget||Hero->GetCombatUnitViewComponent()->GetHUDOwnerView().Abilities.Num()!=3) continue;
		if(State->GetWaveIndex()>=0&&!bGuardianPhaseRejected)
		{
			if(GuardianStage<5) { FDefenseHeroChoiceRequest R; R.RequestId=PC->ReserveCombatRequestId(); R.RunId=State->GetRunView().RunId; R.BindingGeneration=PC->GetCommandBindingGeneration(); R.LifeGeneration=Hero->GetLifeGeneration(); S->ServerChooseGuardian(R); GuardianStage=5; }
			else bGuardianPhaseRejected=!S->GetView().bLastSuccess&&S->GetView().FailureReason==TEXT("HeroSelectionClosed");
		}
		const double Now=GetWorld()->GetTimeSeconds(); if(Now<GuardianNextAction||GuardianStage>=4) continue;
		const auto HUD=Hero->GetCombatUnitViewComponent()->GetHUDOwnerView();
		if(GuardianStage==1&&!bGuardianWSeen) continue;
		if(GuardianStage==2&&!HUD.Abilities.ContainsByPredicate([](const auto& A){return A.DefinitionId.PrimaryAssetName==TEXT("guardian_shield_bash")&&A.CooldownEndTime>0;})) continue;
		if(GuardianStage==3&&!bGuardianStunSeen) continue;
		const TCHAR* Ability=GuardianStage==0?TEXT("guardian_fortify"):GuardianStage==1?TEXT("guardian_shield_bash"):TEXT("guardian_ground_slam");
		FCombatOrderRequest O;
		if(GuardianStage<3)
		{
			const auto* Spec=Hero->GetCombatAbilitySystemComponent()->FindCombatAbilitySpecByDefinitionId(FPrimaryAssetId(TEXT("CombatAbility"),Ability)); if(!Spec) continue;
			O.AbilitySpecHandle=Spec->Handle; O.Type=GuardianStage==1?ECombatOrderType::CastPoint:ECombatOrderType::CastNoTarget;
			if(GuardianStage==1) { O.bHasTargetLocation=true; O.TargetLocation=GuardianTrainingTarget->GetActorLocation(); }
		}
		else { O.Type=ECombatOrderType::AttackTarget; O.TargetUnit=GuardianTrainingTarget; }
		FCombatOrderBatchRequest Batch; Batch.RequestId=PC->ReserveCombatRequestId(); Batch.Orders.Add(O); Hero->ServerIssueOrderBatch(Batch);
		++GuardianStage; GuardianNextAction=Now+.25;
	}
}
