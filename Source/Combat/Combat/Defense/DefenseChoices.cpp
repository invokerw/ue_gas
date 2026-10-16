#include "Combat/Defense/DefenseDirectorComponent.h"
#include "Combat/Defense/DefenseData.h"
#include "Combat/Defense/DefenseGameState.h"
#include "Combat/Defense/DefenseChoiceComponent.h"
#include "Combat/Defense/DefenseOfferSampler.h"
#include "Combat/Modifiers/CombatModifierComponent.h"
#include "Combat/Unit/CombatUnitCharacter.h"
#include "Combat/Economy/CombatEconomyComponent.h"
#include "Combat/Core/CombatRngSubsystem.h"
#include "CombatPlayerController.h"
#include "Engine/World.h"

bool UDefenseDirectorComponent::ParseSeed(const FString& Input, uint64& Out)
{
	const FString S=Input.TrimStartAndEnd();
	const bool Hex=S.StartsWith(TEXT("0x"),ESearchCase::IgnoreCase);
	const uint64 Base=Hex?16:10; uint64 Value=0;
	if(S.Len() <= (Hex?2:0) || S.Len()>22) return false;
	for(int32 I=Hex?2:0;I<S.Len();++I)
	{
		const TCHAR C=FChar::ToLower(S[I]);
		const int32 Digit=C>='0' && C<='9'?C-'0':C>='a' && C<='f'?C-'a'+10:-1;
		if(Digit<0 || uint64(Digit)>=Base || Value>(MAX_uint64-uint64(Digit))/Base) return false;
		Value=Value*Base+uint64(Digit);
	}
	if(Value==0) return false;
	Out=Value; return true;
}
bool UDefenseDirectorComponent::StartWithSeed(const FString& Input, FString& Error)
{
	Error=TEXT("当前不允许修改种子");
#if UE_BUILD_SHIPPING
	return false;
#else
	if(!IsCurrent(View.RunId,GetPhaseGeneration()) || !View.bWaitingForStart || View.Phase!=EDefensePhase::Preparation) return false;
	uint64 Seed=SeedState.RunSeed;
	if(!Input.TrimStartAndEnd().IsEmpty() && !ParseSeed(Input,Seed)) { Error=TEXT("请输入非零十进制或 0x 十六进制整数"); return false; }
	SeedState.RunSeed=Seed; View.bWaitingForStart=false;
	View.Deadline=GetWorld()->GetTimeSeconds()+RunData->PreparationDuration;
	UE_LOG(LogCombat,Display,TEXT("DefenseSeedLocked Run=%s Seed=%llu"),*View.RunId.ToString(),Seed);
	OpenChoiceWindows(); SchedulePhase(RunData->PreparationDuration); Publish(); Error.Reset(); return true;
#endif
}
bool UDefenseDirectorComponent::IsChoicePhase() const
{
	return !View.bWaitingForStart && (View.Phase==EDefensePhase::Preparation || View.Phase==EDefensePhase::Intermission || View.Phase==EDefensePhase::BossPreparation);
}
UDefenseAugmentData* UDefenseDirectorComponent::FindCard(const FPrimaryAssetId Id) const
{
	if(RunData && RunData->AugmentPool)
		for(UDefenseAugmentData* A:RunData->AugmentPool->Augments) if(A->GetPrimaryAssetId()==Id) return A;
	return nullptr;
}
bool UDefenseDirectorComponent::HasOwnedAugment(const FDefensePlayerRecord& R,const FPrimaryAssetId& Id) { return R.OwnedAugments.Contains(Id); }
bool UDefenseDirectorComponent::GenerateAugmentOffer(FDefensePlayerRecord& R,const int32 Wave)
{
	if(!RunData || !RunData->AugmentPool || !IsChoicePhase() || !R.Hero.IsValid() || R.CurrentOffer.ChoiceRevision==MAX_int32) return false;
	if(R.LastOfferWave==Wave && R.CurrentOffer.ChoiceRevision>0) return true;
	FDefenseAugmentOffer Offer;
	Offer.RunId=View.RunId; Offer.PhaseGeneration=View.PhaseGeneration;
	Offer.ChoiceRevision=R.CurrentOffer.ChoiceRevision+1; Offer.WaveIndex=Wave;
	Offer.Deadline=FMath::Min(View.Deadline,GetWorld()->GetTimeSeconds()+RunData->ChoiceDuration);
	Offer.Candidates=FDefenseOfferSampler::Draw(*RunData->AugmentPool,SeedState.RunSeed,R.PlayerOrdinal,Wave,
		Offer.ChoiceRevision,R.Hero->GetUnitDefinitionId(),R.OwnedAugments,R.PreviousUnchosen,false);
	Offer.bSupply=Offer.Candidates.IsEmpty();
	SeedState.DomainOrdinals.FindOrAdd(TEXT("Run.AugmentOffer"))+=Offer.Candidates.Num();
	R.CurrentOffer=Offer; R.LastOfferWave=Wave;
	UE_LOG(LogCombat,Display,TEXT("DefenseChoiceOpened Seat=%d Wave=%d Revision=%d Count=%d"),R.PlayerOrdinal,Wave,Offer.ChoiceRevision,Offer.Candidates.Num());
	return true;
}
void UDefenseDirectorComponent::OpenChoiceWindows()
{
	if(!IsCurrent(View.RunId,GetPhaseGeneration()) || !IsChoicePhase()) return;
	for(auto& Pair:Players) GenerateAugmentOffer(Pair.Value,View.WaveIndex);
	PublishChoices();
}
bool UDefenseDirectorComponent::GetOrCreateAugmentOffer(ACombatPlayerController* PC,FDefenseAugmentOffer& Out)
{
	if(!IsCurrent(View.RunId,GetPhaseGeneration()) || !IsValid(PC) || PC->GetWorld()!=GetWorld()) return false;
	auto* R=Players.Find(PC);
	if(!R || !GenerateAugmentOffer(*R,View.WaveIndex)) return false;
	Out=R->CurrentOffer; return true;
}
bool UDefenseDirectorComponent::ApplyPendingCards(FDefensePlayerRecord& R)
{
	auto* Hero=R.Hero.Get();
	if(!Hero || Hero->GetLifeState()!=ECombatLifeState::Alive) return true;
	TArray<FPrimaryAssetId> Pending=R.PendingCards.Array();
	Pending.Sort([](const auto& A,const auto& B){return A.ToString()<B.ToString();});
	for(const auto& Id:Pending)
	{
		auto* Card=FindCard(Id); if(!Card) return false;
		FCombatModifierApplyRequest Request; Request.ModifierData=Card->ModifierData; Request.Source=Hero;
		const auto Result=Hero->GetCombatModifierComponent()->ApplyModifier(Request);
		if(!Result.bSuccess || !Result.Handle.IsValid()) return false;
		R.AugmentHandles.Add(Result.Handle); R.PendingCards.Remove(Id);
	}
	return true;
}
bool UDefenseDirectorComponent::CommitChoice(ACombatPlayerController* PC,FDefensePlayerRecord& R,const FPrimaryAssetId Id)
{
	if(R.CurrentOffer.bResolved || R.CurrentOffer.bCancelled) return false;
	if(R.CurrentOffer.bSupply)
	{
		if(Id.IsValid()) return false;
		R.CurrentOffer.bResolved=true;
		if(!PC->GetCombatEconomyComponent()->AddGold(80,TEXT("DefenseSupply"))) { R.CurrentOffer.bResolved=false; return false; }
	}
	else
	{
		auto* Card=FindCard(Id);
		if(!Card || !R.CurrentOffer.Candidates.Contains(Id) || !R.Hero.IsValid()
			|| !FDefenseOfferSampler::IsEligible(*Card,*RunData->AugmentPool,R.Hero->GetUnitDefinitionId(),View.WaveIndex,R.OwnedAugments)) return false;
		R.CurrentOffer.bResolved=true; R.OwnedAugments.Add(Id); R.ChosenCards.Add(Id); R.PendingCards.Add(Id);
		if(!ApplyPendingCards(R)) { Finish(false,TEXT("AugmentFailed")); return false; }
	}
	R.PreviousUnchosen=R.CurrentOffer.Candidates; R.PreviousUnchosen.Remove(Id);
	UE_LOG(LogCombat,Display,TEXT("DefenseChoiceCommitted Seat=%d Revision=%d Card=%s"),R.PlayerOrdinal,R.CurrentOffer.ChoiceRevision,*Id.ToString());
	return true;
}
void UDefenseDirectorComponent::ExpireChoices(const double Now,const bool bForce)
{
	if(!IsChoicePhase() || bChoiceMutating || !RunData->AugmentPool || Terminal.bTerminal) return;
	TGuardValue<bool> Guard(bChoiceMutating,true);
	for(auto& Pair:Players)
	{
		auto& R=Pair.Value; auto* PC=Pair.Key.Get();
		if(!PC || !R.CurrentOffer.ChoiceRevision || R.CurrentOffer.bResolved || R.CurrentOffer.bCancelled) continue;
		if(!bForce && Now<R.CurrentOffer.Deadline) continue;
		const auto Id=R.CurrentOffer.Candidates.IsEmpty()?FPrimaryAssetId():R.CurrentOffer.Candidates[0];
		if(!CommitChoice(PC,R,Id)) { Finish(false,TEXT("DefaultChoiceFailed")); return; }
	}
	PublishChoices();
}
int32 UDefenseDirectorComponent::InspectChoiceReplay(ACombatPlayerController* PC,const FDefenseChoiceRequest& Request) const
{
	const auto* R=Players.Find(PC); if(!R) return 0;
	for(const auto& Receipt:R->ChoiceResults)
		if(Receipt.Request.RequestId==Request.RequestId) return Receipt.Request==Request?1:-1;
	return 0;
}
FDefenseChoiceResult UDefenseDirectorComponent::ProcessChoice(ACombatPlayerController* PC,const FDefenseChoiceRequest& Request)
{
	FDefenseChoiceResult Result; Result.RequestId=Request.RequestId; Result.ChoiceRevision=Request.ChoiceRevision;
	Result.FailureReason=TEXT("InvalidAuthorityOrRequest");
	if(!IsCurrent(View.RunId,GetPhaseGeneration()) || !IsValid(PC) || PC->GetWorld()!=GetWorld() || Request.RequestId<=0 || bChoiceMutating || bTeamShopMutating) return Result;
	auto* R=Players.Find(PC);
	if(!R || !R->Hero.IsValid() || PC->GetCommandedUnit()!=R->Hero || R->Hero->GetResourceOwnerPlayerController()!=PC
		|| R->Hero->GetOwner()!=PC) { Result.FailureReason=TEXT("UnknownPlayerOrHero"); return Result; }
	if(Request.RunId!=View.RunId || Request.LifeGeneration!=R->Hero->GetLifeGeneration()
		|| Request.BindingGeneration!=PC->GetCommandBindingGeneration()) { Result.FailureReason=TEXT("StaleIdentity"); return Result; }
	for(const auto& Receipt:R->ChoiceResults)
		if(Receipt.Request.RequestId==Request.RequestId)
		{
			if(Receipt.Request==Request) return Receipt.Result;
			Result.FailureReason=TEXT("RequestPayloadMismatch"); return Result;
		}
	if(Request.RequestId<=R->HighRequestId) { Result.FailureReason=TEXT("OldRequest"); return Result; }
	TGuardValue<bool> Guard(bChoiceMutating,true);
	R->HighRequestId=Request.RequestId;
	if(!IsChoicePhase() || R->CurrentOffer.PhaseGeneration!=View.PhaseGeneration || GetWorld()->GetTimeSeconds()>=R->CurrentOffer.Deadline)
		Result.FailureReason=TEXT("ChoiceExpired");
	else if(Request.ChoiceRevision!=R->CurrentOffer.ChoiceRevision || Request.ChoiceRevision<=0) Result.FailureReason=TEXT("StaleChoiceRevision");
	else if(R->CurrentOffer.bResolved || R->CurrentOffer.bCancelled) Result.FailureReason=TEXT("ChoiceAlreadyResolved");
	else if(R->Hero->GetLifeState()!=ECombatLifeState::Alive) Result.FailureReason=TEXT("HeroDead");
	else if(Request.Action==EDefenseChoiceAction::Select)
	{
		Result.bAccepted=CommitChoice(PC,*R,Request.DefinitionId);
		Result.FailureReason=Result.bAccepted?NAME_None:FName(TEXT("InvalidCandidate"));
		if(Result.bAccepted) Result.ChosenDefinition=Request.DefinitionId;
	}
	else if(Request.Action==EDefenseChoiceAction::Reroll && !Request.DefinitionId.IsValid())
	{
		if(R->RerollsUsed>=2 || R->CurrentOffer.ChoiceRevision==MAX_int32) Result.FailureReason=TEXT("RerollLimit");
		else
		{
			const auto Candidates=FDefenseOfferSampler::Draw(*RunData->AugmentPool,SeedState.RunSeed,R->PlayerOrdinal,View.WaveIndex,
				R->CurrentOffer.ChoiceRevision+1,R->Hero->GetUnitDefinitionId(),R->OwnedAugments,R->CurrentOffer.Candidates,true);
			if(Candidates.IsEmpty()) Result.FailureReason=TEXT("NoAlternative");
			else
			{
				const int32 Price=R->RerollsUsed?100:60;
				if(!PC->GetCombatEconomyComponent()->TrySpendGold(Price,Request.EconomyRevision,TEXT("DefenseReroll"))) Result.FailureReason=TEXT("GoldOrRevision");
				else
				{
					++R->RerollsUsed; ++R->CurrentOffer.ChoiceRevision;
					R->CurrentOffer.Candidates=Candidates; R->CurrentOffer.bSupply=false;
					SeedState.DomainOrdinals.FindOrAdd(TEXT("Run.AugmentOffer"))+=Candidates.Num();
					Result.bAccepted=true; Result.FailureReason=NAME_None;
				}
			}
		}
	}
	else Result.FailureReason=TEXT("InvalidAction");
	if(R->ChoiceResults.Num()==32) R->ChoiceResults.RemoveAt(0);
	R->ChoiceResults.Add({Request,Result}); PublishChoices(); Publish(); return Result;
}
FDefenseChoiceResult UDefenseDirectorComponent::ChooseAugment(ACombatPlayerController* PC,const int32 RequestId,
	const int32 Revision,const FPrimaryAssetId& Id)
{
	FDefenseChoiceRequest Request; Request.RequestId=RequestId; Request.ChoiceRevision=Revision; Request.DefinitionId=Id; Request.RunId=View.RunId;
	if(PC) { Request.BindingGeneration=PC->GetCommandBindingGeneration(); if(PC->GetCommandedUnit()) Request.LifeGeneration=PC->GetCommandedUnit()->GetLifeGeneration(); }
	return ProcessChoice(PC,Request);
}
FDefenseChoiceView UDefenseDirectorComponent::BuildChoiceView(ACombatPlayerController* PC) const
{
	FDefenseChoiceView Out;
	const auto* R=Players.Find(PC); if(!R || !RunData->AugmentPool || !GetOwner()->HasAuthority() || GetNetMode()==NM_Client) return Out;
	Out.Offer=R->CurrentOffer; Out.RerollsUsed=R->RerollsUsed; Out.RerollCost=R->RerollsUsed?100:60;
	if(!R->ChoiceResults.IsEmpty()) Out.LastResult=R->ChoiceResults.Last().Result;
	const auto MakeView=[this,R](FPrimaryAssetId Id)
	{
		FDefenseCardView V; V.DefinitionId=Id;
		if(const auto* Card=FindCard(Id)) { V.Name=Card->CardName.IsEmpty()?Card->DisplayNameText:Card->CardName; V.Description=Card->ShortDescription; V.Family=Card->Family; }
		V.bPending=R->PendingCards.Contains(Id); return V;
	};
	for(const auto& Id:Out.Offer.Candidates) Out.Cards.Add(MakeView(Id));
	for(const auto& Id:R->ChosenCards) Out.Owned.Add(MakeView(Id));
	Out.bCanChoose=!Terminal.bTerminal && IsChoicePhase() && Out.Offer.ChoiceRevision>0 && !Out.Offer.bResolved && !Out.Offer.bCancelled
		&& GetWorld()->GetTimeSeconds()<Out.Offer.Deadline && R->Hero.IsValid() && R->Hero->GetLifeState()==ECombatLifeState::Alive;
	if(Out.bCanChoose && R->RerollsUsed<2)
		Out.bCanReroll=PC->GetCombatEconomyComponent()->GetGold()>=Out.RerollCost && !FDefenseOfferSampler::Draw(*RunData->AugmentPool,
			SeedState.RunSeed,R->PlayerOrdinal,View.WaveIndex,Out.Offer.ChoiceRevision+1,R->Hero->GetUnitDefinitionId(),R->OwnedAugments,Out.Offer.Candidates,true).IsEmpty();
	return Out;
}
void UDefenseDirectorComponent::PublishChoices()
{
	for(auto& Pair:Players)
		if(auto* PC=Pair.Key.Get())
			if(auto* C=PC->FindComponentByClass<UDefenseChoiceComponent>()) C->PublishAuthority(BuildChoiceView(PC));
}
