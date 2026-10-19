#include "Combat/Defense/DefenseDirectorComponent.h"
#include "Combat/Defense/DefenseData.h"
#include "Combat/Unit/CombatUnitCharacter.h"
#include "Combat/Unit/CombatUnitLifecycleComponent.h"
#include "Combat/Unit/CombatUnitAIController.h"
#include "Combat/Unit/CombatProgressionComponent.h"
#include "Combat/Items/CombatInventoryComponent.h"
#include "Combat/Targeting/CombatTeamSubsystem.h"
#include "CombatPlayerController.h"
#include "Engine/World.h"

bool UDefenseDirectorComponent::CanSelectStartingHero(ACombatPlayerController* PC) const
{
	if(!IsCurrent(View.RunId,GetPhaseGeneration())||bStartingHeroMutating||bChoiceMutating||bTeamShopMutating
		||View.Phase!=EDefensePhase::Preparation||View.WaveIndex!=INDEX_NONE||!IsValid(PC)||PC->GetWorld()!=GetWorld()||!PC->HasAuthority()||!TeamPurchaseLedger.IsEmpty()) return false;
	const auto* R=Players.Find(PC); const auto* H=R?R->Hero.Get():nullptr;
	const double Now=GetWorld()->GetTimeSeconds();
	if(!View.bWaitingForStart&&(Now>=View.Deadline||(R&&R->CurrentOffer.ChoiceRevision>0&&Now>=R->CurrentOffer.Deadline))) return false;
	return R&&!R->bStartingHeroSelected&&R->OwnedAugments.IsEmpty()&&R->RerollsUsed==0&&!R->CurrentOffer.bResolved
		&&R->CurrentOffer.ChoiceRevision<MAX_int32&&IsValid(H)&&H==PC->GetCommandedUnit()&&H->GetLifeState()==ECombatLifeState::Alive
		&&H->GetResourceOwnerPlayerController()==PC&&H->GetCombatInventoryComponent()->GetItemCount()==0&&H->GetCombatProgressionComponent()->GetExperience()==0;
}
bool UDefenseDirectorComponent::SelectStartingHero(ACombatPlayerController* PC,TSubclassOf<ACombatUnitCharacter> Class,FName& Failure)
{
	Failure=TEXT("HeroSelectionClosed"); if(!CanSelectStartingHero(PC)) return false;
	Failure=TEXT("InvalidHeroDefinition");
	if(!Class||Class->HasAnyClassFlags(CLASS_Abstract)) return false;
	const auto* CDO=Class->GetDefaultObject<ACombatUnitCharacter>(); auto* Data=const_cast<UCombatUnitData*>(CDO->GetUnitData());
	auto* Old=Players.FindChecked(PC).Hero.Get(); const uint32 OldLife=Old->GetLifeGeneration();
	if(!Data||!Data->GetPrimaryAssetId().IsValid()||!Data->BaseStats.IsValid()||Data->AIProfile||!Data->InitialItems.IsEmpty()
		||Data->InitialExperience!=0||Data->GoldReward||Data->ExperienceReward
		||GetWorld()->GetSubsystem<UCombatTeamSubsystem>()->GetRelation(Data->InitialTeamId,Old->GetCombatTeamId())!=ECombatTeamRelation::Friendly) return false;
	const FGuid ExpectedRun=View.RunId; const uint32 ExpectedPhase=GetPhaseGeneration();
	TGuardValue<bool> Guard(bStartingHeroMutating,true);
	FVector Position; Failure=TEXT("HeroSpawnBlocked"); if(!FindSafeLocation(*CDO,RunData->RespawnLocation,Position)) return false;
	const FTransform Transform(FRotator::ZeroRotator,Position);
	auto* Hero=GetWorld()->SpawnActorDeferred<ACombatUnitCharacter>(Class,Transform,nullptr,nullptr,ESpawnActorCollisionHandlingMethod::DontSpawnIfColliding);
	if(!Hero) return false;
	// 只清理本事务创建的Actor和专属AI，不触碰玩家的Command Pawn。
	auto DestroyUnit=[](ACombatUnitCharacter* Unit)
	{
		if(!IsValid(Unit)) return;
		auto* AI=Cast<ACombatUnitAIController>(Unit->GetController()); Unit->Destroy(); if(IsValid(AI)) AI->Destroy();
	};
	// 技能授予依赖ASC ActorInfo；先完成组件注册/BeginPlay，再复核白名单定义的幂等初始化。
	// 与无技能敌人不同，不能在Deferred Spawn尚未完成时调用GrantCombatAbility。
	Hero->FinishSpawning(Transform);
	if(!IsValid(Hero)||Hero->IsActorBeingDestroyed()) return false;
	if(!Hero->InitializeFromUnitData(Data)) { DestroyUnit(Hero); Failure=TEXT("HeroInitializationFailed"); return false; }
	if(!PC->SetCommandedUnitAuthority(Hero)) { PC->SetCommandedUnitAuthority(Old); DestroyUnit(Hero); Failure=TEXT("HeroBindingFailed"); return false; }
	auto* Record=Players.Find(PC);
	if(!Record||!IsCurrent(ExpectedRun,ExpectedPhase)||!Record->OwnedAugments.IsEmpty()||Record->RerollsUsed>0||Record->CurrentOffer.bResolved||!TeamPurchaseLedger.IsEmpty()||!IsValid(Old)||Old->GetLifeGeneration()!=OldLife||Old->GetLifeState()!=ECombatLifeState::Alive||Record->Hero.Get()!=Old)
	{ if(IsValid(Old)) PC->SetCommandedUnitAuthority(Old); DestroyUnit(Hero); Failure=TEXT("StaleIdentity"); return false; }
	const double OldDeadline=Record->CurrentOffer.Deadline;
	UnbindHeroLifecycle(Old); Record->Hero=Hero; Record->bStartingHeroSelected=true;
	Hero->GetCombatLifecycleComponent()->OnDied().AddUObject(this,&ThisClass::OnHeroDied);
	Hero->GetCombatLifecycleComponent()->OnRespawned().AddUObject(this,&ThisClass::OnHeroRespawned);
	if(!View.bWaitingForStart&&RunData->AugmentPool)
	{
		Record->LastOfferWave=MIN_int32;
		if(!GenerateAugmentOffer(*Record,INDEX_NONE)) { Finish(false,TEXT("HeroOfferFailed")); DestroyUnit(Old); return false; }
		if(OldDeadline>0) Record->CurrentOffer.Deadline=FMath::Min(OldDeadline,Record->CurrentOffer.Deadline);
	}
	DestroyUnit(Old); Failure=NAME_None; PublishChoices(); PublishTeamShop(); Publish();
	UE_LOG(LogCombat,Display,TEXT("DefenseStartingHeroSelected Player=%s Hero=%s Definition=%s"),*PC->GetName(),*Hero->GetName(),*Hero->GetUnitDefinitionId().ToString());
	return true;
}
