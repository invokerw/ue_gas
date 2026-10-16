#include "Combat/Defense/DefenseChoiceComponent.h"
#include "Combat/Defense/DefenseGameMode.h"
#include "Combat/Defense/DefenseDirectorComponent.h"
#include "Combat/Network/CombatNetworkSecuritySubsystem.h"
#include "Combat/Economy/CombatEconomyComponent.h"
#include "Combat/Unit/CombatUnitCharacter.h"
#include "CombatPlayerController.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
UDefenseChoiceComponent::UDefenseChoiceComponent() { SetIsReplicatedByDefault(true); PrimaryComponentTick.bCanEverTick=false; }
UDefenseDirectorComponent* UDefenseChoiceComponent::ResolveDirector() const
{
	const auto* Mode=GetWorld()?GetWorld()->GetAuthGameMode<ADefenseGameMode>():nullptr;
	return Mode?Mode->GetDefenseDirector():nullptr;
}
void UDefenseChoiceComponent::RequestChoice(const EDefenseChoiceAction Action,const FPrimaryAssetId Id)
{
	auto* PC=Cast<ACombatPlayerController>(GetOwner());
	if(!PC || !PC->IsLocalController() || bEnding || !PC->GetCommandedUnit()) return;
	FDefenseChoiceRequest R;
	R.RequestId=PC->ReserveCombatRequestId(); R.RunId=View.Offer.RunId; R.ChoiceRevision=View.Offer.ChoiceRevision;
	R.Action=Action; R.DefinitionId=Id; R.LifeGeneration=PC->GetCommandedUnit()->GetLifeGeneration();
	R.BindingGeneration=PC->GetCommandBindingGeneration(); R.EconomyRevision=PC->GetCombatEconomyComponent()->GetEconomyRevision();
	ServerSubmitChoice(R);
}
void UDefenseChoiceComponent::ServerSubmitChoice_Implementation(FDefenseChoiceRequest Request)
{
	ProcessForConnection(Cast<ACombatPlayerController>(GetOwner()),Request);
}
FDefenseChoiceResult UDefenseChoiceComponent::ProcessForConnection(ACombatPlayerController* PC,const FDefenseChoiceRequest& Request)
{
	FDefenseChoiceResult Result; Result.RequestId=Request.RequestId; Result.ChoiceRevision=Request.ChoiceRevision;
	Result.FailureReason=TEXT("InvalidEnvelope");
	auto* Director=ResolveDirector();
	if(bEnding || !PC || PC!=GetOwner() || !Director || GetNetMode()==NM_Client || !PC->HasAuthority()
		|| Request.RequestId<=0 || !Request.RunId.IsValid() || Request.ChoiceRevision<=0
		|| Request.LifeGeneration<=0 || Request.BindingGeneration<=0 || Request.EconomyRevision<=0
		|| uint8(Request.Action)>uint8(EDefenseChoiceAction::Reroll)
		|| Request.DefinitionId.ToString().Len()>160
		|| (Request.Action==EDefenseChoiceAction::Reroll && Request.DefinitionId.IsValid())) return Result;
	const int32 Replay=Director->InspectChoiceReplay(PC,Request);
	FGameplayTag Failure;
	if(!GetWorld()->GetSubsystem<UCombatNetworkSecuritySubsystem>()->ValidateAndConsumeComponentRequest(PC,this,Request.RequestId,Replay!=0,Failure))
		Result.FailureReason=Failure.GetTagName();
	else if(Replay<0) Result.FailureReason=TEXT("RequestPayloadMismatch");
	else Result=Director->ProcessChoice(PC,Request);
	FDefenseChoiceView Next=Director->BuildChoiceView(PC); Next.LastResult=Result;
	PublishAuthority(Next);
	return Result;
}
void UDefenseChoiceComponent::PublishAuthority(const FDefenseChoiceView& Snapshot)
{
	if(bEnding || !GetOwner() || !GetOwner()->HasAuthority() || GetNetMode()==NM_Client) return;
	bool Same=Snapshot.Offer.RunId==View.Offer.RunId && Snapshot.Offer.ChoiceRevision==View.Offer.ChoiceRevision
		&& Snapshot.Offer.bResolved==View.Offer.bResolved && Snapshot.Offer.bCancelled==View.Offer.bCancelled
		&& Snapshot.Offer.Deadline==View.Offer.Deadline && Snapshot.bCanChoose==View.bCanChoose && Snapshot.bCanReroll==View.bCanReroll
		&& Snapshot.Owned.Num()==View.Owned.Num() && Snapshot.LastResult.RequestId==View.LastResult.RequestId
		&& Snapshot.LastResult.FailureReason==View.LastResult.FailureReason;
	if(Same) for(int32 I=0;I<Snapshot.Owned.Num();++I) Same &= Snapshot.Owned[I].bPending==View.Owned[I].bPending;
	if(Same) return;
	const int32 Revision=View.Revision; View=Snapshot; View.Revision=Revision==MAX_int32?1:Revision+1;
	GetOwner()->ForceNetUpdate();
}
void UDefenseChoiceComponent::ClearAuthority() { PublishAuthority(FDefenseChoiceView()); }
void UDefenseChoiceComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps); DOREPLIFETIME_CONDITION(UDefenseChoiceComponent,View,COND_OwnerOnly);
}
void UDefenseChoiceComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	ClearAuthority(); bEnding=true; Super::EndPlay(Reason);
}
