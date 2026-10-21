#include "Combat/Defense/DefenseEventComponent.h"
#include "Combat/Defense/DefenseGameMode.h"
#include "Combat/Defense/DefenseDirectorComponent.h"
#include "Combat/Defense/DefenseGameState.h"
#include "Combat/Network/CombatNetworkSecuritySubsystem.h"
#include "CombatPlayerController.h"
#include "Combat/Unit/CombatUnitCharacter.h"
#include "Net/UnrealNetwork.h"
UDefenseEventComponent::UDefenseEventComponent() { SetIsReplicatedByDefault(true); PrimaryComponentTick.bCanEverTick=false; }
UDefenseDirectorComponent* UDefenseEventComponent::ResolveDirector() const
{
	const auto* Mode=GetWorld()?GetWorld()->GetAuthGameMode<ADefenseGameMode>():nullptr;
	return Mode?Mode->GetDefenseDirector():nullptr;
}
void UDefenseEventComponent::RequestOption(const EDefenseEventOption Option)
{
	auto* PC=Cast<ACombatPlayerController>(GetOwner()); const auto* State=GetWorld()?GetWorld()->GetGameState<ADefenseGameState>():nullptr;
	if(!PC||!PC->IsLocalController()||!State||!PC->GetCommandedUnit()||bEnding) return;
	const auto& V=State->GetRunView().Event;
	if(!V.EventId.IsValid()||V.Revision<=0) return;
	FDefenseEventRequest R; R.RequestId=PC->ReserveCombatRequestId(); R.RunId=State->GetRunView().RunId;
	R.PhaseGeneration=State->GetRunView().PhaseGeneration; R.EventId=V.EventId; R.EventRevision=V.Revision;
	R.BindingGeneration=PC->GetCommandBindingGeneration(); R.LifeGeneration=PC->GetCommandedUnit()->GetLifeGeneration(); R.Option=Option;
	ServerSubmitEvent(R);
}
void UDefenseEventComponent::ServerSubmitEvent_Implementation(FDefenseEventRequest Request)
{ ProcessForConnection(Cast<ACombatPlayerController>(GetOwner()),Request); }
FDefenseEventResult UDefenseEventComponent::ProcessForConnection(ACombatPlayerController* PC,const FDefenseEventRequest& Request)
{
	FDefenseEventResult Result; Result.RequestId=Request.RequestId; Result.FailureReason=TEXT("InvalidEnvelope");
	auto* Director=ResolveDirector();
	if(bEnding||!PC||PC!=GetOwner()||!Director||GetNetMode()==NM_Client||!PC->HasAuthority()||Request.RequestId<=0
		||!Request.RunId.IsValid()||!Request.EventId.IsValid()||Request.PhaseGeneration<=0||Request.EventRevision<=0
		||Request.BindingGeneration<=0||Request.LifeGeneration==0||uint8(Request.Option)>uint8(EDefenseEventOption::Cancel)) return Result;
	const int32 Replay=Director->InspectEventReplay(PC,Request); FGameplayTag Failure;
	if(!GetWorld()->GetSubsystem<UCombatNetworkSecuritySubsystem>()->ValidateAndConsumeComponentRequest(PC,this,Request.RequestId,Replay!=0,Failure)) Result.FailureReason=Failure.GetTagName();
	else if(Replay<0) Result.FailureReason=TEXT("RequestPayloadMismatch");
	else Result=Director->ProcessDefenseEvent(PC,Request);
	LastResult=Result; GetOwner()->ForceNetUpdate(); return Result;
}
void UDefenseEventComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{ Super::GetLifetimeReplicatedProps(OutLifetimeProps); DOREPLIFETIME_CONDITION(UDefenseEventComponent,LastResult,COND_OwnerOnly); }
void UDefenseEventComponent::EndPlay(const EEndPlayReason::Type Reason)
{ bEnding=true; LastResult={}; Super::EndPlay(Reason); }
