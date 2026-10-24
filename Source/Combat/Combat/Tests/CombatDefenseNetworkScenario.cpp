#include "Combat/Tests/CombatDefenseNetworkScenario.h"
#include "Combat/Defense/DefenseGameState.h"
#include "Combat/Defense/DefenseHeroSelectionComponent.h"
#include "Combat/Defense/DefenseGuardian.h"
#include "Combat/Order/CombatOrderComponent.h"
#include "Combat/Defense/DefenseObjective.h"
#include "Combat/Combat/CombatDamageSubsystem.h"
#include "Combat/AI/CombatAIBrainComponent.h"
#include "Combat/Attributes/CombatAttributeSet.h"
#include "Combat/Unit/CombatUnitLifecycleComponent.h"
#include "Combat/Unit/CombatProgressionComponent.h"
#include "Combat/Economy/CombatEconomyComponent.h"
#include "CombatPlayerController.h"
#include "Combat/Defense/DefenseChoiceComponent.h"
#include "Combat/Core/CombatTags.h"
#include "Combat/Defense/DefensePlayerController.h"
#include "Combat/Defense/DefenseGameMode.h"
#include "Combat/Defense/DefenseDirectorComponent.h"
#include "Combat/Defense/DefenseData.h"
#include "Combat/Defense/DefenseCoreShieldRuntime.h"
#include "Combat/View/CombatUnitViewComponent.h"
#include "Combat/Modifiers/CombatModifierComponent.h"
#include "Combat/Log/CombatEventSubsystem.h"
#include "EngineUtils.h"
#include "Net/UnrealNetwork.h"
#include "Combat/Ability/CombatAbilitySystemComponent.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
namespace DefenseChoiceSmoke
{
	/** 选择卡使用的十二种属性，统一顺序以便三端对照，不读取客户端 GE 实例。 */
	TArray<float> ReadAttributes(const ACombatUnitCharacter& Hero)
	{
		TArray<float> Values;
		for(const auto& Attribute:{UCombatAttributeSet::GetAttackDamageAttribute(),UCombatAttributeSet::GetAttackSpeedAttribute(),
			UCombatAttributeSet::GetMoveSpeedAttribute(),UCombatAttributeSet::GetAttackRangeAttribute(),UCombatAttributeSet::GetSpellAmplifyPctAttribute(),
			UCombatAttributeSet::GetMaxManaAttribute(),UCombatAttributeSet::GetManaRegenAttribute(),UCombatAttributeSet::GetCooldownReductionPctAttribute(),
			UCombatAttributeSet::GetArmorAttribute(),UCombatAttributeSet::GetMaxHealthAttribute(),UCombatAttributeSet::GetHealthRegenAttribute(),UCombatAttributeSet::GetMagicResistAttribute()})
			Values.Add(Hero.GetCombatAbilitySystemComponent()->GetNumericAttribute(Attribute));
		return Values;
	}
}
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
	if(FParse::Param(FCommandLine::Get(),TEXT("DefenseSmoke"))&&FParse::Param(FCommandLine::Get(),TEXT("DefenseSiegeSmoke"))&&HasAuthority())
		GetWorld()->GetSubsystem<UCombatEventSubsystem>()->OnRecord().AddUObject(this,&ThisClass::OnSiegeCombatRecord);
	if(FParse::Param(FCommandLine::Get(),TEXT("DefenseSmoke"))&&FParse::Param(FCommandLine::Get(),TEXT("DefenseAssassinSmoke"))&&HasAuthority())
		GetWorld()->GetSubsystem<UCombatEventSubsystem>()->OnRecord().AddUObject(this,&ThisClass::OnAssassinCombatRecord);
	if(FParse::Param(FCommandLine::Get(),TEXT("DefenseSmoke"))&&FParse::Param(FCommandLine::Get(),TEXT("DefenseGuardianSmoke"))&&HasAuthority())
		GetWorld()->GetSubsystem<UCombatEventSubsystem>()->OnRecord().AddUObject(this,&ThisClass::OnGuardianCombatRecord);
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
	if(Elapsed>390) { Finish(false,TEXT("Timeout")); return; }
	auto* State=GetWorld()->GetGameState<ADefenseGameState>();
	if(!State || !State->GetRunView().RunId.IsValid()) return;
	const auto& V=State->GetRunView();
	if(FParse::Param(FCommandLine::Get(),TEXT("DefenseReturnBeaconSmoke"))) { ExerciseReturnBeacon(); return; }
	if(FParse::Param(FCommandLine::Get(),TEXT("DefenseMerchantSealSmoke"))) { ExerciseMerchantSeal(); return; }
	if(FParse::Param(FCommandLine::Get(),TEXT("DefenseUtilityEquipmentSmoke"))) { ExerciseUtilityEquipment(); return; }
	if(FParse::Param(FCommandLine::Get(),TEXT("DefenseEquipmentSmoke"))) { ExerciseEquipment(); return; }
	if(FParse::Param(FCommandLine::Get(),TEXT("DefenseEventSmoke"))) { ExerciseEvents(); return; }
	const bool Siege=FParse::Param(FCommandLine::Get(),TEXT("DefenseSiegeSmoke"));
	if(Siege) ObserveSiege();
	const bool Assassin=FParse::Param(FCommandLine::Get(),TEXT("DefenseAssassinSmoke"));
	if(Assassin) ObserveAssassins();
	const bool Guardian=FParse::Param(FCommandLine::Get(),TEXT("DefenseGuardianSmoke"));
	if(Guardian) ExerciseGuardian();
	const bool Choices=FParse::Param(FCommandLine::Get(),TEXT("DefenseChoiceSmoke"));
	if(Choices) ExerciseChoices();
	const bool TeamShop=FParse::Param(FCommandLine::Get(),TEXT("DefenseTeamShopSmoke"));
	if(TeamShop) ExerciseTeamShop();
	const bool Reinforcement=FParse::Param(FCommandLine::Get(),TEXT("DefenseReinforcementSmoke"));
	if(Reinforcement) ExerciseReinforcement();
	if(bDone) return;
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
			if(LastWave==1 && !Heroes.IsEmpty() && !Reinforcement)
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
			if(V.PendingSpawns==0 && WaveDamage && Elapsed-WaveStarted>10 && ClearedWaves==V.WaveIndex && (!Siege||IsSiegeWaveObserved()) && (!Assassin||IsAssassinWaveObserved()))
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
				Rewards &= (TeamShop || Reinforcement || It->GetCombatEconomyComponent()->GetGold()==(Choices?1210:1370)) && It->GetCommandedUnit()
					&& It->GetCommandedUnit()->GetCombatProgressionComponent()->GetExperience()==2000;
		if(Choices && GetNetMode()!=NM_Client)
		{
			auto* Mode=GetWorld()->GetAuthGameMode<ADefenseGameMode>();
			for(TActorIterator<ACombatPlayerController> It(GetWorld());It;++It)
			{
				const auto Build=Mode->GetDefenseDirector()->BuildChoiceView(*It);
				Rewards &= Build.Owned.Num()==6 && Build.RerollsUsed==2;
				TArray<FCombatModifierView> Effects; It->GetCommandedUnit()->GetCombatModifierComponent()->BuildModifierViews(Effects);
				for(const auto& Card:Build.Owned)
				{
					bool Found=false;
					for(const UDefenseAugmentData* Definition:Mode->GetDefenseDirector()->GetFrozenRunData()->AugmentPool->Augments)
						if(Definition->GetPrimaryAssetId()==Card.DefinitionId)
							for(const auto& Effect:Effects) Found |= Effect.DefinitionId==Definition->ModifierData->GetPrimaryAssetId();
					Rewards &= Found;
				}
			}
		}
		if((Choices||TeamShop) && GetNetMode()!=NM_Client && FinalAttributes.IsEmpty())
		{
			for(TActorIterator<ACombatPlayerController> It(GetWorld());It;++It)
			{
				if(auto* Hero=It->GetCommandedUnit()) { auto& S=FinalAttributes.AddDefaulted_GetRef(); S.Hero=Hero; S.Attributes=DefenseChoiceSmoke::ReadAttributes(*Hero); }
			}
			ForceNetUpdate();
		}
		if((Choices||TeamShop) && GetNetMode()==NM_Client)
		{
			if(Choices) Rewards &= bChoiceEvidence;
			if(FinalAttributes.Num()!=ExpectedPlayers) return;
			for(const auto& Sample:FinalAttributes)
			{
				if(!Sample.Hero) return;
				const auto Values=DefenseChoiceSmoke::ReadAttributes(*Sample.Hero);
				if(Values.Num()!=Sample.Attributes.Num()) return;
				for(int32 I=0;I<Values.Num();++I) if(!FMath::IsNearlyEqual(Values[I],Sample.Attributes[I],0.001f)) return;
			}
			UE_LOG(LogTemp,Display,TEXT("DefenseChoiceAttributeReplication Players=%d AttributesPerHero=12 Match=1"),FinalAttributes.Num());
		}
		if(TeamShop)
		{
			if(GetNetMode()==NM_Client) Rewards &= bShopActiveSeen&&bShopSupplySeen&&bShopExpiredSeen&&bShopClosedRejected&&bCoreShieldFullSeen&&bCoreShieldPartialSeen&&bCoreShieldDepletedSeen;
			else
			{
				TArray<FCombatShopOfferView> Offers;
				int64 TotalGold=0; for(TActorIterator<ACombatPlayerController> It(GetWorld());It;++It) { TotalGold+=It->GetCombatEconomyComponent()->GetGold(); Offers=It->GetCombatEconomyComponent()->GetEconomyView().ShopOffers; }
				int32 Bought=0; for(const auto& Entry:Offers) { Bought+=Entry.RemainingPurchases==0?1:0; Rewards &= !Entry.bEffectActive; }
				Rewards &= Bought==2 && TotalGold==ExpectedPlayers*1370-460 && bShopServerActive;
			}
			UE_LOG(LogTemp,Display,TEXT("DefenseCoreShieldNetwork NetMode=%d Full=%d Partial=%d Depleted=%d Schema=%d"),int32(GetNetMode()),bCoreShieldFullSeen,bCoreShieldPartialSeen,bCoreShieldDepletedSeen,V.SchemaVersion);
			UE_LOG(LogTemp,Display,TEXT("DefenseTeamShopNetwork NetMode=%d ActiveSeen=%d SupplySeen=%d ExpiredSeen=%d CombatRejected=%d ServerEffect=%d Match=%d"),
				int32(GetNetMode()),bShopActiveSeen,bShopSupplySeen,bShopExpiredSeen,bShopClosedRejected,bShopServerActive,Rewards);
		}
		if(Reinforcement)
		{
			if(GetNetMode()==NM_Client) Rewards &= bAidTwo&&bAidOne&&bAidZero&&bAidExpired&&AidClientShields.Num()>=2;
			else
			{
				int64 Gold=0; bool Finished=false;
				for(TActorIterator<ACombatPlayerController> It(GetWorld());It;++It)
				{
					Gold+=It->GetCombatEconomyComponent()->GetGold();
					if(const auto* O=It->GetCombatEconomyComponent()->FindShopOffer(FPrimaryAssetId(TEXT("CombatItem"),TEXT("team_reinforcement_mark")))) Finished=O->RemainingPurchases==0&&!O->bEffectActive;
				}
				Rewards &= Gold==ExpectedPlayers*1370-200&&AidRevivals==3&&AidExpiries==2&&Finished;
			}
			UE_LOG(LogTemp,Display,TEXT("DefenseReinforcementNetwork NetMode=%d Revivals=%d Expiries=%d Two=%d One=%d Zero=%d ClientShields=%d Expired=%d Match=%d"),
				int32(GetNetMode()),AidRevivals,AidExpiries,bAidTwo,bAidOne,bAidZero,AidClientShields.Num(),bAidExpired,Rewards);
		}
		if(Siege)
		{
			Rewards &= SiegeInitialPositions.Num()==4&&SiegeMoved.Num()==4;
			if(GetNetMode()!=NM_Client) Rewards &= SiegeCoreHit.Num()==4;
			UE_LOG(LogTemp,Display,TEXT("DefenseSiegeNetwork NetMode=%d Spawned=%d Moved=%d CoreAttackers=%d Match=%d"),int32(GetNetMode()),SiegeInitialPositions.Num(),SiegeMoved.Num(),SiegeCoreHit.Num(),Rewards);
		}
		if(Assassin)
		{
			Rewards &= AssassinInitialPositions.Num()==4&&AssassinMoved.Num()==4;
			if(GetNetMode()!=NM_Client) Rewards &= AssassinHeroHit.Num()==4;
			UE_LOG(LogTemp,Display,TEXT("DefenseAssassinNetwork NetMode=%d Spawned=%d Moved=%d HeroAttackers=%d Match=%d"),int32(GetNetMode()),AssassinInitialPositions.Num(),AssassinMoved.Num(),AssassinHeroHit.Num(),Rewards);
		}
		if(Guardian)
		{
			Rewards &= bGuardianWSeen&&bGuardianPassiveSeen&&bGuardianStunSeen&&bGuardianDamageSeen;
			if(GetNetMode()!=NM_Client) Rewards &= GuardianSkillMask==7&&bGuardianAbsorbed&&bGuardianSecurity&&bGuardianCombatReady;
			else
			{
				int32 Seat=0; FParse::Value(FCommandLine::Get(),TEXT("DefenseSmokeSeat="),Seat);
				if(Seat==1) Rewards &= bGuardianPhaseRejected;
			}
			UE_LOG(LogTemp,Display,TEXT("DefenseGuardianNetwork NetMode=%d Skills=%d W=%d Passive=%d Stun=%d Damage=%d Absorb=%d Security=%d CombatReady=%d Closed=%d Match=%d"),
				int32(GetNetMode()),GuardianSkillMask,bGuardianWSeen,bGuardianPassiveSeen,bGuardianStunSeen,bGuardianDamageSeen,bGuardianAbsorbed,bGuardianSecurity,bGuardianCombatReady,bGuardianPhaseRejected,Rewards);
		}
		const bool Evidence=GetNetMode()==NM_Client ? bRPC : bSawMovement && bSawDamage && (DefeatCase || (ClearedWaves==6 && bRespawn));
		Finish(Expected && Evidence && Rewards,FString::Printf(TEXT("Wave=%d CoreHP=%.0f Movement=%d Damage=%d Respawn=%d RPC=%d Cleared=%d Rewards=%d Reason=%s"),
			V.WaveIndex,V.ObjectiveHealth,bSawMovement,bSawDamage,bRespawn,bRPC,ClearedWaves,Rewards,*V.TerminalReason.ToString()));
	}
}

void ACombatDefenseNetworkScenario::ExerciseChoices()
{
	if(GetNetMode()!=NM_Client) return;
	for(TActorIterator<ADefensePlayerController> It(GetWorld());It;++It)
	{
		if(!It->IsLocalController()) continue;
		auto* Choice=It->GetChoiceComponent(); const auto& V=Choice->GetView();
		auto* Hero=It->GetCommandedUnit(); if(!Hero || Hero->GetOwner()!=*It) continue;
		bChoiceEvidence |= V.Owned.Num()==6 && V.RerollsUsed==2;
		if(!V.bCanChoose || V.Offer.bResolved) continue;
		if(V.Offer.WaveIndex<0 && V.RerollsUsed<2)
		{
			if(RerollStep>V.RerollsUsed) continue;
			FDefenseChoiceRequest Request;
			Request.RequestId=It->ReserveCombatRequestId(); Request.RunId=V.Offer.RunId; Request.ChoiceRevision=V.Offer.ChoiceRevision;
			Request.Action=EDefenseChoiceAction::Reroll; Request.LifeGeneration=Hero->GetLifeGeneration();
			Request.BindingGeneration=It->GetCommandBindingGeneration(); Request.EconomyRevision=It->GetCombatEconomyComponent()->GetEconomyRevision();
			Choice->ServerSubmitChoice(Request); Choice->ServerSubmitChoice(Request);
			++RerollStep; continue;
		}
		if(SentChoiceRevision==V.Offer.ChoiceRevision) continue;
		SentChoiceRevision=V.Offer.ChoiceRevision;
		Choice->RequestChoice(EDefenseChoiceAction::Select,V.Offer.Candidates.IsEmpty()?FPrimaryAssetId():V.Offer.Candidates[0]);
	}
}

void ACombatDefenseNetworkScenario::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ACombatDefenseNetworkScenario,FinalAttributes);
    DOREPLIFETIME(ACombatDefenseNetworkScenario,ReturnBeaconPhase);
    DOREPLIFETIME(ACombatDefenseNetworkScenario,EquipmentSamples);
	DOREPLIFETIME(ACombatDefenseNetworkScenario,GuardianSmokeHero);
	DOREPLIFETIME(ACombatDefenseNetworkScenario,GuardianTrainingTarget);
}


void ACombatDefenseNetworkScenario::ExerciseTeamShop()
{
	const auto* State=GetWorld()->GetGameState<ADefenseGameState>(); if(!State) return;
	if(GetNetMode()!=NM_Client)
	{
		FCombatShopOfferView GuardValue; for(TActorIterator<ACombatPlayerController> It(GetWorld());It;++It) if(const auto* O=It->GetCombatEconomyComponent()->FindShopOffer(FPrimaryAssetId(TEXT("CombatItem"),TEXT("team_shrine_barrier")))) { GuardValue=*O; break; }
		const auto* Guard=&GuardValue;
		if(Guard->bEffectActive&&State->GetRunView().Objective)
		{
			auto* Core=State->GetRunView().Objective.Get(); TArray<FCombatModifierView> Effects;
			Core->GetCombatModifierComponent()->BuildModifierViews(Effects);
			for(const auto& E:Effects) if(E.DefinitionId==FPrimaryAssetId(TEXT("CombatModifier"),TEXT("team_shrine_barrier_effect")))
				if(const auto* Runtime=Cast<UDefenseCoreShieldRuntime>(Core->GetCombatModifierComponent()->FindRuntime(E.Handle)))
					bShopServerActive |= FMath::IsNearlyEqual(Runtime->GetRemainingShield(),400.0f)&&FMath::IsNearlyEqual(State->GetRunView().ObjectiveShield,400.0f);
		}
		return;
	}
	for(TActorIterator<ADefensePlayerController> It(GetWorld());It;++It)
	{
		if(!It->IsLocalController() || !It->GetCommandedUnit()) continue;
		auto* C=It->GetCombatEconomyComponent();
		const auto* Guard=C->FindShopOffer(FPrimaryAssetId(TEXT("CombatItem"),TEXT("team_shrine_barrier")));
		const auto* Supply=C->FindShopOffer(FPrimaryAssetId(TEXT("CombatItem"),TEXT("team_agile_circuit")));
		if(!Guard||!Supply) continue;
		bShopActiveSeen |= (Guard->RemainingPurchases==0)&&Guard->bEffectActive;
		const auto& CoreView=State->GetRunView();
		bCoreShieldFullSeen |= CoreView.SchemaVersion==3&&FMath::IsNearlyEqual(CoreView.ObjectiveShield,400.0f);
		bCoreShieldPartialSeen |= bCoreShieldFullSeen&&CoreView.ObjectiveShield>0&&CoreView.ObjectiveShield<399;
		bCoreShieldDepletedSeen |= bCoreShieldPartialSeen&&CoreView.ObjectiveShield==0&&!Guard->bEffectActive;
		bShopSupplySeen |= (Supply->RemainingPurchases==0)&&Supply->bEffectActive;
		bShopExpiredSeen |= (Guard->RemainingPurchases==0)&&!Guard->bEffectActive&&(Supply->RemainingPurchases==0)&&!Supply->bEffectActive;
		const auto& Last=It->GetLastEconomyResult();
		// GameState 与拥有者商品快照分属不同 Actor，阶段到达次序不确定；旧修订也应安全拒绝。
		bShopClosedRejected |= ShopClosedRequestId>0&&Last.RequestId==ShopClosedRequestId&&!Last.bSuccess
			&&(Last.FailureTag==CombatTags::Failure_ActionUnsupported||Last.FailureTag==CombatTags::Failure_Economy_Stale);
		FName Item;
		if(State->GetDefensePhase()==EDefensePhase::Preparation&&!(Guard->RemainingPurchases==0)) Item=TEXT("team_shrine_barrier");
		if(State->GetDefensePhase()==EDefensePhase::Intermission && State->GetWaveIndex()==0 && !(Supply->RemainingPurchases==0)) Item=TEXT("team_agile_circuit");
		if(State->GetDefensePhase()==EDefensePhase::WaveCombat && State->GetWaveIndex()==0 && !bShopClosedSent)
		{ Item=TEXT("team_hunt_horn"); }
		if(Item.IsNone() || (Item!=TEXT("team_hunt_horn") && ShopAttemptRevision==Guard->Revision) || (Item==TEXT("team_hunt_horn") && bShopClosedSent) || (Item==TEXT("team_agile_circuit") && bShopAgileSent)) continue;
		ShopAttemptRevision=Guard->Revision;
		FCombatEconomyRequest R; R.RequestId=It->ReserveCombatRequestId(); R.ItemDefinitionId=FPrimaryAssetId(TEXT("CombatItem"),Item);
		const auto* SelectedOffer=Item==TEXT("team_hunt_horn")?Guard:Supply; R.PurchaseContextId=SelectedOffer->ContextId; R.PurchaseContextRevision=SelectedOffer->Revision; R.CommandBindingGeneration=It->GetCommandBindingGeneration();
		R.ExpectedEconomyRevision=C->GetEconomyRevision(); R.ExpectedInventoryRevision=C->GetInventoryRevision();
		if(Item==TEXT("team_hunt_horn")) { bShopClosedSent=true; ShopClosedRequestId=R.RequestId; }
		if(Item==TEXT("team_agile_circuit")) bShopAgileSent=true;
		It->ServerSubmitEconomyRequest(R); It->ServerSubmitEconomyRequest(R);
	}
}

void ACombatDefenseNetworkScenario::ExerciseReinforcement()
{
	const auto* State=GetWorld()->GetGameState<ADefenseGameState>(); if(!State) return;
	const FPrimaryAssetId Item(TEXT("CombatItem"),TEXT("team_reinforcement_mark"));
	const FPrimaryAssetId Effect(TEXT("CombatModifier"),TEXT("team_reinforcement_shield"));
	if(GetNetMode()==NM_Client)
	{
		for(TActorIterator<ACombatPlayerController> It(GetWorld());It;++It)
		{
			if(!It->IsLocalController()||!It->GetCommandedUnit()) continue;
			auto* C=It->GetCombatEconomyComponent(); const auto* O=C->FindShopOffer(Item); if(!O) continue;
			const auto Status=O->Status.ToString();
			bAidTwo |= O->RemainingPurchases==0&&Status.Contains(TEXT("剩余援军 2 次"));
			bAidOne |= O->RemainingPurchases==0&&Status.Contains(TEXT("剩余援军 1 次"));
			bAidZero |= O->RemainingPurchases==0&&Status.Contains(TEXT("剩余援军 0 次"));
			bAidExpired |= bAidZero&&!O->bEffectActive;
			if(State->GetDefensePhase()==EDefensePhase::Preparation&&O->bCanPurchase&&AidAttemptRevision!=O->Revision)
			{
				AidAttemptRevision=O->Revision;
				FCombatEconomyRequest R; R.RequestId=It->ReserveCombatRequestId(); R.ItemDefinitionId=Item;
				R.PurchaseContextId=O->ContextId; R.PurchaseContextRevision=O->Revision; R.CommandBindingGeneration=It->GetCommandBindingGeneration();
				R.ExpectedEconomyRevision=C->GetEconomyRevision(); R.ExpectedInventoryRevision=C->GetInventoryRevision();
				It->ServerSubmitEconomyRequest(R); It->ServerSubmitEconomyRequest(R);
			}
		}
		for(TActorIterator<ACombatUnitCharacter> It(GetWorld());It;++It)
			for(const auto& M:It->GetCombatUnitViewComponent()->GetVisibleModifiers())
				if(M.DefinitionId==Effect) AidClientShields.Add(FString::Printf(TEXT("%d:%u"),It->GetUniqueID(),It->GetLifeGeneration()));
		return;
	}
	const double Now=GetWorld()->GetTimeSeconds();
	if(AidShieldHandle.IsValid()&&Now>AidShieldExpires+.5)
	{
		if(auto* Hero=AidProtectedHero.Get();Hero&&Hero->GetCombatModifierComponent()->FindRuntime(AidShieldHandle))
		{ Finish(false,TEXT("AidShieldDidNotExpire")); return; }
		++AidExpiries; AidShieldHandle={};
	}
	if(auto* Hero=AidHero.Get();Hero&&Hero->GetLifeState()==ECombatLifeState::Alive&&Hero->GetLifeGeneration()>AidDeathLife)
	{
		const double Delay=Now-AidDeathAt; const double Expected=AidRevivals<2?5.0:8.0;
		if(Delay<Expected-.05||Delay>Expected+.8) { Finish(false,FString::Printf(TEXT("AidDelay Actual=%.3f Expected=%.1f"),Delay,Expected)); return; }
		TArray<FCombatModifierView> Views; Hero->GetCombatModifierComponent()->BuildModifierViews(Views);
		const auto* Shield=Views.FindByPredicate([&](const auto& V){return V.DefinitionId==Effect;});
		if((AidRevivals<2)!=(Shield!=nullptr)) { Finish(false,TEXT("AidRevivalShieldMismatch")); return; }
		if(Shield)
		{
			const auto* Runtime=Cast<UDefenseShieldRuntime>(Hero->GetCombatModifierComponent()->FindRuntime(Shield->Handle));
			if(!Runtime||!FMath::IsNearlyEqual(Runtime->GetRemainingShield(),Hero->GetCombatAttributeSet()->GetMaxHealth()*.2f,.01f)
				||!FMath::IsNearlyEqual(Runtime->GetExpireAt()-Runtime->GetAppliedAt(),4.0,.01)) { Finish(false,TEXT("AidShieldValueMismatch")); return; }
			AidProtectedHero=Hero; AidShieldHandle=Shield->Handle; AidShieldExpires=Runtime->GetExpireAt();
		}
		UE_LOG(LogTemp,Display,TEXT("DefenseReinforcementRevival Index=%d Delay=%.3f Expected=%.1f Shield=%d"),AidRevivals,Delay,Expected,Shield!=nullptr);
		++AidRevivals; bRespawn=true; AidHero.Reset();
	}
	const auto& V=State->GetRunView();
	if(V.Phase!=EDefensePhase::WaveCombat||V.WaveIndex>2||AidDeaths>V.WaveIndex||AidHero.IsValid()) return;
	TArray<ACombatUnitCharacter*> Heroes; bool Bought=false;
	for(TActorIterator<ACombatPlayerController> It(GetWorld());It;++It)
	{
		if(It->GetCommandedUnit()) Heroes.Add(It->GetCommandedUnit());
		if(const auto* O=It->GetCombatEconomyComponent()->FindShopOffer(Item)) Bought |= O->RemainingPurchases==0;
	}
	if(!Bought||Heroes.Num()!=ExpectedPlayers) return;
	AidHero=Heroes[AidDeaths%Heroes.Num()]; AidDeathLife=AidHero->GetLifeGeneration(); AidDeathAt=Now;
	Kill(AidHero.Get()); ++AidDeaths;
}

void ACombatDefenseNetworkScenario::EndPlay(const EEndPlayReason::Type Reason)
{
	if(auto* Events=GetWorld()?GetWorld()->GetSubsystem<UCombatEventSubsystem>():nullptr) Events->OnRecord().RemoveAll(this);
	if(HasAuthority()&&IsValid(GuardianTrainingTarget)) { auto* Controller=GuardianTrainingTarget->GetController(); GuardianTrainingTarget->Destroy(); if(IsValid(Controller)) Controller->Destroy(); }
	Super::EndPlay(Reason);
}
void ACombatDefenseNetworkScenario::ObserveSiege()
{
	const auto* State=GetWorld()->GetGameState<ADefenseGameState>(); if(!State) return;
	for(TActorIterator<ADefenseSiegeEnemy> It(GetWorld());It;++It)
	{
		const int32 Id=It->GetUniqueID(); const int32 Wave=State->GetWaveIndex();
		if(const auto* Initial=SiegeInitialPositions.Find(Id)) { if(FVector::Dist2D(*Initial,It->GetActorLocation())>300) SiegeMoved.Add(Id); }
		else { SiegeInitialPositions.Add(Id,It->GetActorLocation()); SiegeByWave.FindOrAdd(Wave).Add(Id); }
		if(GetNetMode()==NM_Client&&(It->HasAuthority()||It->GetCombatAIBrainComponent()->IsRunning())) Finish(false,TEXT("SiegeClientAuthorityLeak"));
	}
}
void ACombatDefenseNetworkScenario::OnSiegeCombatRecord(const FCombatLogRecord& R)
{
	const auto* State=GetWorld()->GetGameState<ADefenseGameState>(); if(!State||!State->GetRunView().Objective) return;
	if(R.EventType==CombatTags::Event_Combat_DamageApplied&&R.AppliedAmount>0
		&&R.Source.DirectSourceType==ECombatDirectSourceType::Attack&&R.TargetActorId==State->GetRunView().Objective->GetUniqueID()
		&&SiegeInitialPositions.Contains(R.SourceActorId)) SiegeCoreHit.Add(R.SourceActorId);
}
bool ACombatDefenseNetworkScenario::IsSiegeWaveObserved() const
{
	const auto* State=GetWorld()->GetGameState<ADefenseGameState>(); if(!State) return false;
	const int32 Wave=State->GetWaveIndex(); if(Wave!=2&&Wave!=4) return true;
	const auto* Ids=SiegeByWave.Find(Wave); if(!Ids||Ids->Num()!=2) return false;
	for(const int32 Id:*Ids) if(!SiegeCoreHit.Contains(Id)||!SiegeMoved.Contains(Id)) return false;
	return true;
}

void ACombatDefenseNetworkScenario::ObserveAssassins()
{
	const auto* State=GetWorld()->GetGameState<ADefenseGameState>(); if(!State) return;
	for(TActorIterator<ADefenseAssassinEnemy> It(GetWorld());It;++It)
	{
		const int32 Id=It->GetUniqueID(); const int32 Wave=State->GetWaveIndex();
		if(const auto* Initial=AssassinInitialPositions.Find(Id)) { if(FVector::Dist2D(*Initial,It->GetActorLocation())>300) AssassinMoved.Add(Id); }
		else { AssassinInitialPositions.Add(Id,It->GetActorLocation()); AssassinByWave.FindOrAdd(Wave).Add(Id); }
		if(GetNetMode()==NM_Client&&(It->HasAuthority()||It->GetCombatAIBrainComponent()->IsRunning())) Finish(false,TEXT("AssassinClientAuthorityLeak"));
	}
}
void ACombatDefenseNetworkScenario::OnAssassinCombatRecord(const FCombatLogRecord& R)
{
	if(R.EventType!=CombatTags::Event_Combat_DamageApplied||R.AppliedAmount<=0
		||R.Source.DirectSourceType!=ECombatDirectSourceType::Attack||!AssassinInitialPositions.Contains(R.SourceActorId)) return;
	for(TActorIterator<ACombatPlayerController> It(GetWorld());It;++It)
		if(It->GetCommandedUnit()&&It->GetCommandedUnit()->GetUniqueID()==R.TargetActorId) AssassinHeroHit.Add(R.SourceActorId);
}
bool ACombatDefenseNetworkScenario::IsAssassinWaveObserved() const
{
	const auto* State=GetWorld()->GetGameState<ADefenseGameState>(); if(!State) return false;
	const int32 Wave=State->GetWaveIndex(); if(Wave!=3&&Wave!=4) return true;
	const auto* Ids=AssassinByWave.Find(Wave); if(!Ids||Ids->Num()!=2) return false;
	for(const int32 Id:*Ids) if(!AssassinHeroHit.Contains(Id)||!AssassinMoved.Contains(Id)) return false;
	return true;
}
