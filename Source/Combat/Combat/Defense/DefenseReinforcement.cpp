#include "Combat/Defense/DefenseDirectorComponent.h"
#include "Combat/Defense/DefenseData.h"
#include "Combat/Defense/DefenseTeamItemEffect.h"
#include "Combat/Items/CombatItemData.h"
#include "Combat/Modifiers/CombatModifierComponent.h"
#include "Combat/Unit/CombatUnitLifecycleComponent.h"
#include "Combat/Unit/CombatUnitCharacter.h"
#include "Combat/Attributes/CombatAttributeSet.h"
#include "CombatPlayerController.h"
#include "Engine/World.h"

void UDefenseDirectorComponent::UnbindHeroLifecycle(ACombatUnitCharacter* Hero)
{
	if(!Hero) return;
	Hero->GetCombatLifecycleComponent()->OnDied().RemoveAll(this);
	Hero->GetCombatLifecycleComponent()->OnRespawned().RemoveAll(this);
}
int32 UDefenseDirectorComponent::CountPendingReinforcements(const FPrimaryAssetId Id) const
{
	int32 Count=0;
	for(const auto& Pair:Players) if(Pair.Key.IsValid()&&Pair.Value.Hero.IsValid()&&Pair.Value.PendingReinforcement==Id) ++Count;
	return Count;
}
void UDefenseDirectorComponent::OnHeroDied(ACombatUnitCharacter* Hero,const FCombatEventContext& Cause)
{
	if(!IsCurrent(View.RunId,GetPhaseGeneration())||!IsCoreAlive()||!IsValid(Hero)||Hero->GetWorld()!=GetWorld()||Hero->GetLifeState()!=ECombatLifeState::Dead) return;
	for(auto& Player:Players)
	{
		auto& R=Player.Value; if(!Player.Key.IsValid()||R.Hero.Get()!=Hero) continue;
		const uint32 Life=Hero->GetLifeGeneration(); if(R.DeadLife==Life) return;
		R.DeadLife=Life; R.RespawnAt=GetWorld()->GetTimeSeconds()+RunData->RespawnDelay; R.RespawnRetries=0;
		R.PendingReinforcement={}; R.ReinforcementDeathLife=0;
		for(auto& Pair:TeamPurchaseLedger)
		{
			auto& Purchase=Pair.Value;
			const auto* Item=FrozenTeamItems.Find(Pair.Key); const auto* E=Item?Cast<UDefenseTeamItemEffect>((*Item)->AcquireEffect):nullptr;
			if(!E||!E->ReinforcementShieldData||!Purchase.bEffectActive||Purchase.RemainingReinforcements<=0||Purchase.ExcludedDeaths.FindRef(Hero)==Life) continue;
			--Purchase.RemainingReinforcements; R.PendingReinforcement=Pair.Key; R.ReinforcementDeathLife=Life;
			R.RespawnAt=GetWorld()->GetTimeSeconds()+FMath::Max(0.0f,RunData->RespawnDelay-E->RespawnReduction);
			UE_LOG(LogCombat,Display,TEXT("DefenseReinforcementReserved Item=%s Hero=%s DeathLife=%u Remaining=%d Delay=%.2f"),
				*Pair.Key.ToString(),*Hero->GetName(),Life,Purchase.RemainingReinforcements,FMath::Max(0.0f,RunData->RespawnDelay-E->RespawnReduction));
			break;
		}
		++TeamShopRevision; PublishTeamShop(); return;
	}
}
void UDefenseDirectorComponent::OnHeroRespawned(ACombatUnitCharacter* Hero,const FCombatEventContext& Cause)
{
	if(!ApplyPendingReinforcement(Hero)&&IsCurrent(View.RunId,GetPhaseGeneration())) Finish(false,TEXT("ReinforcementEffectFailed"));
}
bool UDefenseDirectorComponent::ApplyPendingReinforcement(ACombatUnitCharacter* Hero)
{
	if(!IsCurrent(View.RunId,GetPhaseGeneration())||!IsValid(Hero)||Hero->GetWorld()!=GetWorld()||Hero->GetLifeState()!=ECombatLifeState::Alive) return true;
	TWeakObjectPtr<ACombatPlayerController> Key;
	for(const auto& Pair:Players) if(Pair.Key.IsValid()&&Pair.Value.Hero.Get()==Hero) { Key=Pair.Key; break; }
	auto* Player=Players.Find(Key); if(!Player||!Player->PendingReinforcement.IsValid()) return true;
	if(Hero->GetLifeGeneration()-1!=Player->ReinforcementDeathLife)
	{ Player->PendingReinforcement={}; Player->ReinforcementDeathLife=0; ++TeamShopRevision; PublishTeamShop(); return true; }
	// 生命周期回调可能发生在另一个Modifier Hook内；保留凭证到下一监控回合，不排入无句柄的延迟Apply。
	if(bTeamShopMutating||Hero->GetCombatModifierComponent()->IsDeferringOperations()) return true;
	const auto Id=Player->PendingReinforcement; const uint32 Life=Hero->GetLifeGeneration();
	const auto* Item=FrozenTeamItems.Find(Id); const auto* E=Item?Cast<UDefenseTeamItemEffect>((*Item)->AcquireEffect):nullptr;
	auto* Record=TeamPurchaseLedger.Find(Id);
	Player->PendingReinforcement={}; Player->ReinforcementDeathLife=0;
	if(!E||!Record||!Record->bEffectActive||!E->ReinforcementShieldData) return true;
	TGuardValue<bool> Guard(bTeamShopMutating,true);
	FCombatModifierApplyRequest Request; Request.ModifierData=E->ReinforcementShieldData; Request.Source=Hero; Request.SourceContext=Record->Source;
	const float Amount=Hero->GetCombatAttributeSet()->GetMaxHealth()*E->RespawnShieldFraction;
	Request.RuntimeParameterOverrides.Add(TEXT("shield_amount"),Amount);
	const auto Applied=Hero->GetCombatModifierComponent()->ApplyModifier(Request);
	// 公开Apply可触发断线、死亡或Shutdown；跨该调用后不使用先前的TMap元素引用。
	Player=Players.Find(Key); Record=TeamPurchaseLedger.Find(Id);
	const bool Current=IsCurrent(View.RunId,GetPhaseGeneration())&&IsCoreAlive()&&IsValid(Hero)&&!Hero->IsActorBeingDestroyed()
		&&Hero->GetLifeState()==ECombatLifeState::Alive&&Hero->GetLifeGeneration()==Life&&Player&&Player->Hero.Get()==Hero&&Record&&Record->bEffectActive;
	if(!Current||!Applied.bSuccess||!Applied.Handle.IsValid())
	{
		if(IsValid(Hero)&&Applied.Handle.IsValid()) Hero->GetCombatModifierComponent()->RemoveModifier(Applied.Handle);
		return !Current;
	}
	Record->AppliedModifiers.Add({Hero,Applied.Handle}); ++TeamShopRevision; PublishTeamShop();
	UE_LOG(LogCombat,Display,TEXT("DefenseReinforcementShield Item=%s Hero=%s Life=%u Amount=%.2f Duration=%.2f"),*Id.ToString(),*Hero->GetName(),Life,Amount,E->ReinforcementShieldData->Duration);
	return true;
}
void UDefenseDirectorComponent::RemoveHeroReinforcement(ACombatUnitCharacter* Hero)
{
	if(!Hero) return;
	TArray<FCombatModifierHandle> Removed;
	for(auto& Pair:TeamPurchaseLedger)
	{
		const auto* Item=FrozenTeamItems.Find(Pair.Key); const auto* E=Item?Cast<UDefenseTeamItemEffect>((*Item)->AcquireEffect):nullptr;
		if(!E||!E->ReinforcementShieldData) continue;
		// 购买时的旧生命排除表保留到整局结束，解除登记不能让迟到死亡事件重新获得资格。
		Pair.Value.AppliedModifiers.RemoveAll([&](const auto& Applied)
		{ if(Applied.Unit.Get()!=Hero) return false; Removed.Add(Applied.Handle); return true; });
	}
	TGuardValue<bool> Guard(bTeamShopMutating,true);
	for(const auto Handle:Removed) Hero->GetCombatModifierComponent()->RemoveModifier(Handle);
}
