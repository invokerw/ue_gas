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
#include "Combat/Defense/DefenseChoiceComponent.h"
#include "Combat/Core/CombatTags.h"
#include "Combat/Defense/DefensePlayerController.h"
#include "Combat/Defense/DefenseGameMode.h"
#include "Combat/Defense/DefenseDirectorComponent.h"
#include "Combat/Defense/DefenseData.h"
#include "Combat/Defense/DefenseCoreShieldRuntime.h"
#include "Combat/Modifiers/CombatModifierComponent.h"
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
	const bool Choices=FParse::Param(FCommandLine::Get(),TEXT("DefenseChoiceSmoke"));
	if(Choices) ExerciseChoices();
	const bool TeamShop=FParse::Param(FCommandLine::Get(),TEXT("DefenseTeamShopSmoke"));
	if(TeamShop) ExerciseTeamShop();
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
				Rewards &= (TeamShop || It->GetCombatEconomyComponent()->GetGold()==(Choices?1210:1370)) && It->GetCommandedUnit()
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
				Rewards &= Bought==2 && TotalGold==ExpectedPlayers*1370-420 && bShopServerActive;
			}
			UE_LOG(LogTemp,Display,TEXT("DefenseCoreShieldNetwork NetMode=%d Full=%d Partial=%d Depleted=%d Schema=%d"),int32(GetNetMode()),bCoreShieldFullSeen,bCoreShieldPartialSeen,bCoreShieldDepletedSeen,V.SchemaVersion);
			UE_LOG(LogTemp,Display,TEXT("DefenseTeamShopNetwork NetMode=%d ActiveSeen=%d SupplySeen=%d ExpiredSeen=%d CombatRejected=%d ServerEffect=%d Match=%d"),
				int32(GetNetMode()),bShopActiveSeen,bShopSupplySeen,bShopExpiredSeen,bShopClosedRejected,bShopServerActive,Rewards);
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
		const auto* Supply=C->FindShopOffer(FPrimaryAssetId(TEXT("CombatItem"),TEXT("team_supply")));
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
		if(State->GetDefensePhase()==EDefensePhase::Intermission && State->GetWaveIndex()==0 && !(Supply->RemainingPurchases==0)) Item=TEXT("team_supply");
		if(State->GetDefensePhase()==EDefensePhase::WaveCombat && State->GetWaveIndex()==0 && !bShopClosedSent)
		{ Item=TEXT("team_rally"); }
		if(Item.IsNone() || ShopAttemptRevision==Guard->Revision) continue;
		ShopAttemptRevision=Guard->Revision;
		FCombatEconomyRequest R; R.RequestId=It->ReserveCombatRequestId(); R.ItemDefinitionId=FPrimaryAssetId(TEXT("CombatItem"),Item);
		R.PurchaseContextId=Guard->ContextId; R.PurchaseContextRevision=Guard->Revision; R.CommandBindingGeneration=It->GetCommandBindingGeneration();
		R.ExpectedEconomyRevision=C->GetEconomyRevision(); R.ExpectedInventoryRevision=C->GetInventoryRevision();
		if(Item==TEXT("team_rally")) { bShopClosedSent=true; ShopClosedRequestId=R.RequestId; }
		It->ServerSubmitEconomyRequest(R); It->ServerSubmitEconomyRequest(R);
	}
}
