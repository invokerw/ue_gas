#include "Combat/Defense/DefenseDirectorComponent.h"
#include "Combat/Defense/DefensePlayerController.h"
#include "Combat/Defense/DefenseChoiceComponent.h"
#include "Combat/Defense/DefenseTeamItemEffect.h"
#include "Combat/Defense/DefenseData.h"
#include "Combat/Modifiers/CombatModifierComponent.h"
#include "Combat/Unit/CombatUnitCharacter.h"
#include "Combat/Unit/CombatUnitLifecycleComponent.h"
#include "CombatPlayerController.h"

bool UDefenseDirectorComponent::ReplacePlayerHero(ACombatPlayerController* Player,ACombatUnitCharacter* NewHero)
{
	if(!IsCurrent(View.RunId,GetPhaseGeneration())||!IsValid(Player)||!IsValid(NewHero)||Player->GetWorld()!=GetWorld()||NewHero->GetWorld()!=GetWorld()
		||NewHero->GetLifeState()!=ECombatLifeState::Alive||Player->HasAuthority()==false) return false;
	auto* Record=Players.Find(Player); if(!Record||Record->Hero.Get()==NewHero||NewHero->GetResourceOwnerPlayerController()&&NewHero->GetResourceOwnerPlayerController()!=Player) return false;
	auto* OldHero=Record->Hero.Get(); if(!OldHero||OldHero->GetLifeState()!=ECombatLifeState::Alive) return false;
	const auto OldHandles=Record->AugmentHandles; const auto OldHeroPtr=Record->Hero;
	for(const auto Handle:OldHandles) OldHero->GetCombatModifierComponent()->RemoveModifier(Handle);
	UnbindHeroLifecycle(OldHero); RemoveHeroReinforcement(OldHero);
	if(!Player->SetCommandedUnitAuthority(NewHero))
	{
		Record->Hero=OldHeroPtr; OldHero->GetResourceOwnerPlayerController();
		Player->SetCommandedUnitAuthority(OldHero);
		Record->AugmentHandles=OldHandles; return false;
	}
	Record=Players.Find(Player); if(!Record||!IsCurrent(View.RunId,GetPhaseGeneration())) return false;
	Record->Hero=NewHero; Record->DeadLife=0; Record->RespawnAt=0; Record->RespawnRetries=0; Record->PendingReinforcement={}; Record->ReinforcementDeathLife=0; Record->AugmentHandles.Reset();
	NewHero->GetCombatLifecycleComponent()->OnDied().AddUObject(this,&UDefenseDirectorComponent::OnHeroDied);
	NewHero->GetCombatLifecycleComponent()->OnRespawned().AddUObject(this,&UDefenseDirectorComponent::OnHeroRespawned);
	for(const auto& Id:Record->OwnedAugments)
	{
		auto* Card=FindCard(Id); if(!Card||!Card->ModifierData) continue;
		if(!Card->CompatibleHeroes.IsEmpty()&&!Card->CompatibleHeroes.Contains(NewHero->GetUnitDefinitionId())) continue;
		FCombatModifierApplyRequest Request; Request.ModifierData=Card->ModifierData; Request.Source=NewHero;
		const auto Applied=NewHero->GetCombatModifierComponent()->ApplyModifier(Request);
		if(!Applied.bSuccess||!Applied.Handle.IsValid()) { Finish(false,TEXT("HeroSwapAugmentFailed")); return false; }
		Record=Players.Find(Player); if(!Record) return false; Record->AugmentHandles.Add(Applied.Handle);
	}
	if(!ApplyTeamShopEffects(NewHero)) { Finish(false,TEXT("HeroSwapTeamEffectFailed")); return false; }
	PublishChoices(); PublishTeamShop(); Publish();
	UE_LOG(LogCombat,Display,TEXT("DefenseHeroReplaced Player=%s Old=%s New=%s Owned=%d"),*Player->GetName(),*GetNameSafe(OldHero),*GetNameSafe(NewHero),Record->OwnedAugments.Num());
	return true;
}
