#include "Combat/Defense/DefenseHeroSelectionComponent.h"
#include "Combat/Defense/DefenseGameMode.h"
#include "Combat/Defense/DefenseGameState.h"
#include "Combat/Defense/DefenseDirectorComponent.h"
#include "Combat/Unit/CombatUnitCharacter.h"
#include "Combat/Network/CombatNetworkSecuritySubsystem.h"
#include "CombatPlayerController.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
UDefenseHeroSelectionComponent::UDefenseHeroSelectionComponent() { SetIsReplicatedByDefault(true); PrimaryComponentTick.bCanEverTick=false; }
void UDefenseHeroSelectionComponent::RequestGuardian()
{
	auto* PC=Cast<ACombatPlayerController>(GetOwner()); const auto* State=GetWorld()?GetWorld()->GetGameState<ADefenseGameState>():nullptr;
	if(!PC||!PC->IsLocalController()||!State||!PC->GetCommandedUnit()||bEnding) return;
	FDefenseHeroChoiceRequest R; R.RequestId=PC->ReserveCombatRequestId(); R.RunId=State->GetRunView().RunId;
	R.BindingGeneration=PC->GetCommandBindingGeneration(); R.LifeGeneration=PC->GetCommandedUnit()->GetLifeGeneration(); ServerChooseGuardian(R);
}
void UDefenseHeroSelectionComponent::ServerChooseGuardian_Implementation(FDefenseHeroChoiceRequest R) { ProcessForConnection(Cast<ACombatPlayerController>(GetOwner()),R); }
FDefenseHeroChoiceView UDefenseHeroSelectionComponent::ProcessForConnection(ACombatPlayerController* PC,const FDefenseHeroChoiceRequest& R)
{
	FDefenseHeroChoiceView Result=View; Result.LastRequestId=R.RequestId; Result.bLastSuccess=false; Result.FailureReason=TEXT("InvalidEnvelope");
	auto* Mode=GetWorld()?GetWorld()->GetAuthGameMode<ADefenseGameMode>():nullptr;
	if(bEnding||bMutating||!IsValid(PC)||PC!=GetOwner()||GetNetMode()==NM_Client||!PC->HasAuthority()||!Mode
		||R.RequestId<=0||!R.RunId.IsValid()||R.BindingGeneration<=0||R.LifeGeneration==0) return Result;
	TGuardValue<bool> Guard(bMutating,true);
	const auto* Cached=Receipts.FindByPredicate([&](const auto& Entry){return Entry.Request.RequestId==R.RequestId;});
	FGameplayTag Failure;
	if(!GetWorld()->GetSubsystem<UCombatNetworkSecuritySubsystem>()->ValidateAndConsumeComponentRequest(PC,this,R.RequestId,Cached!=nullptr,Failure)) Result.FailureReason=Failure.GetTagName();
	else if(Cached)
	{
		if(Cached->Request==R) Result=Cached->Result; else Result.FailureReason=TEXT("RequestPayloadMismatch");
	}
	else if(R.RequestId<=HighRequestId) Result.FailureReason=TEXT("StaleRequest");
	else
	{
		HighRequestId=R.RequestId;
		const auto* State=GetWorld()->GetGameState<ADefenseGameState>(); auto* Hero=PC->GetCommandedUnit();
		if(!State||State->GetRunView().RunId!=R.RunId||!Hero||Hero->GetLifeGeneration()!=R.LifeGeneration||PC->GetCommandBindingGeneration()!=R.BindingGeneration) Result.FailureReason=TEXT("StaleIdentity");
		else Result.bLastSuccess=Mode->GetDefenseDirector()->SelectStartingHero(PC,Mode->GetGuardianClass(),Result.FailureReason);
		Receipts.Add({R,Result}); if(Receipts.Num()>32) Receipts.RemoveAt(0);
	}
	UE_LOG(LogCombat,Display,TEXT("DefenseHeroChoice Request=%d Success=%d Reason=%s"),R.RequestId,Result.bLastSuccess,*Result.FailureReason.ToString());
	PublishResult(Result); return View;
}
void UDefenseHeroSelectionComponent::PublishResult(const FDefenseHeroChoiceView& Result)
{
	View.LastRequestId=Result.LastRequestId; View.bLastSuccess=Result.bLastSuccess; View.FailureReason=Result.FailureReason;
	View.Revision=View.Revision==MAX_int32?1:View.Revision+1; GetOwner()->ForceNetUpdate();
}
void UDefenseHeroSelectionComponent::PublishEligibility(const bool Can,const bool Selected)
{
	if(bEnding||!GetOwner()||!GetOwner()->HasAuthority()||GetNetMode()==NM_Client||(View.bCanChoose==Can&&View.bGuardianSelected==Selected)) return;
	View.bCanChoose=Can; View.bGuardianSelected=Selected; View.Revision=View.Revision==MAX_int32?1:View.Revision+1; GetOwner()->ForceNetUpdate();
}
void UDefenseHeroSelectionComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{ Super::GetLifetimeReplicatedProps(OutLifetimeProps); DOREPLIFETIME_CONDITION(UDefenseHeroSelectionComponent,View,COND_OwnerOnly); }
void UDefenseHeroSelectionComponent::EndPlay(const EEndPlayReason::Type Reason)
{ PublishEligibility(false,View.bGuardianSelected); bEnding=true; Receipts.Reset(); Super::EndPlay(Reason); }
