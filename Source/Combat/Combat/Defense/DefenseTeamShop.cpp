#include "Combat/Defense/DefenseDirectorComponent.h"
#include "Combat/Defense/DefenseData.h"
#include "Combat/Defense/DefenseObjective.h"
#include "Combat/Defense/DefenseTeamItemEffect.h"
#include "Combat/Defense/DefenseCoreShieldRuntime.h"
#include "Combat/Attributes/CombatAttributeSet.h"
#include "Combat/Items/CombatItemData.h"
#include "Combat/Items/CombatItemSubsystem.h"
#include "Combat/Economy/CombatShopData.h"
#include "Combat/Core/CombatTags.h"
#include "Combat/Economy/CombatEconomyComponent.h"
#include "Combat/Modifiers/CombatModifierComponent.h"
#include "Combat/Combat/CombatHealSubsystem.h"
#include "CombatPlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Engine/World.h"

namespace DefenseTeamShop
{
	/** 移除前将所有句柄移出权威容器，允许集合观察者重入清理而不重复撤销。 */
	void Revoke(TArray<FDefenseTeamAppliedModifier> Applied)
	{
		for(const auto& Entry:Applied)
			if(auto* Hero=Entry.Unit.Get()) Hero->GetCombatModifierComponent()->RemoveModifier(Entry.Handle);
	}
}

bool UDefenseDirectorComponent::InitializeTeamItems(UCombatShopData* Shop)
{
	if(bTeamItemsInitialized||!Shop||!IsCurrent(View.RunId,GetPhaseGeneration())) return false;
	FString Error; if(!Shop->ValidateRuntime(Error)) return false;
	TArray<UCombatItemData*> Catalog; Shop->GetCatalogItems(Catalog);
	TSet<FPrimaryAssetId> Effects;
	if(RunData->AugmentPool) for(const UDefenseAugmentData* Card:RunData->AugmentPool->Augments) Effects.Add(Card->ModifierData->GetPrimaryAssetId());
	for(const UDefenseWaveData* Wave:RunData->Waves) if(Wave->FixedAugment) Effects.Add(Wave->FixedAugment->GetPrimaryAssetId());
	TArray<UCombatItemData*> Team; bool bHasCoreShield=false,bHasReinforcement=false;
	for(auto* Item:Catalog) if(Item->bUseOnAcquire) if(auto* Effect=Cast<UDefenseTeamItemEffect>(Item->AcquireEffect))
	{
		const auto* Data=Effect->ReinforcementShieldData?Effect->ReinforcementShieldData.Get():(Effect->CoreShieldData?Effect->CoreShieldData.Get():Effect->ModifierData.Get());
		if(Data)
		{
			if(Effects.Contains(Data->GetPrimaryAssetId())) return false;
			Effects.Add(Data->GetPrimaryAssetId());
		}
		// 一个目录只能提供一种核心屏障，阻止编辑出多种不同身份而叠加同类护盾。
		if(Effect->CoreShieldData) { if(bHasCoreShield) return false; bHasCoreShield=true; }
		if(Effect->ReinforcementShieldData) { if(bHasReinforcement) return false; bHasReinforcement=true; }
		Team.Add(Item);
	}
	if(Team.Num()>6) return false;
	for(auto* Item:Team)
	{
		auto* Copy=DuplicateObject<UCombatItemData>(Item,this);
		auto* E=CastChecked<UDefenseTeamItemEffect>(Copy->AcquireEffect);
		if(E->ModifierData) E->ModifierData=DuplicateObject<UCombatModifierData>(E->ModifierData,E);
		if(E->CoreShieldData) E->CoreShieldData=DuplicateObject<UCombatModifierData>(E->CoreShieldData,E);
		if(E->ReinforcementShieldData) E->ReinforcementShieldData=DuplicateObject<UCombatModifierData>(E->ReinforcementShieldData,E);
		FrozenTeamItems.Add(Copy->GetPrimaryAssetId(),Copy);
	}
	bTeamItemsInitialized=true; PublishTeamShop(); return true;
}

bool UDefenseDirectorComponent::IsTeamShopOpen() const
{
	return bTeamItemsInitialized && IsCurrent(View.RunId,GetPhaseGeneration())
		&& (View.Phase==EDefensePhase::Preparation || View.Phase==EDefensePhase::Intermission || View.Phase==EDefensePhase::BossPreparation);
}
FCombatShopOfferView UDefenseDirectorComponent::BuildTeamItemOffer(ACombatPlayerController* Player,const FPrimaryAssetId Id) const
{
	FCombatShopOfferView V; V.DefinitionId=Id; V.ContextId=View.RunId; V.Revision=TeamShopRevision; V.RemainingPurchases=0;
	const auto* Item=FrozenTeamItems.Find(Id);
	const auto* E=Item?Cast<UDefenseTeamItemEffect>((*Item)->AcquireEffect):nullptr;
	if(!E) { V.Status=FText::FromString(TEXT("团队商品尚未配置")); return V; }
	const auto* Record=TeamPurchaseLedger.Find(Id);
	V.RemainingPurchases=Record?0:1; V.bEffectActive=Record&&Record->bEffectActive;
	V.ExpireWave=Record?Record->ExpireWave:(E->DurationWaves?FMath::Min(View.TotalWaves,View.WaveIndex+1+E->DurationWaves):0);
	V.RecipientName=Record?Record->BuyerName:FString();
	V.bCanPurchase=IsValid(Player)&&Player->GetWorld()==GetWorld()&&Players.Contains(Player)&&IsTeamShopOpen()&&IsCoreAlive()
		&&E->AllowedPhases.Contains(View.Phase)&&!Record;
	V.Status=FText::FromString(Record?(Record->bEffectActive?TEXT("全队生效中"):TEXT("本局已使用")):(V.bCanPurchase?TEXT("本人付款 · 全队受益"):TEXT("本阶段不能领取")));
	if(Record&&E->ReinforcementShieldData)
		V.Status=FText::FromString(FString::Printf(TEXT("剩余援军 %d 次 · 等待复活 %d 人 · 护盾保护 %d 人"),
			Record->RemainingReinforcements,CountPendingReinforcements(Id),Record->AppliedModifiers.Num()));
	return V;
}
void UDefenseDirectorComponent::PublishTeamShop()
{
	if(LastShopPhase!=View.PhaseGeneration) { LastShopPhase=View.PhaseGeneration; ++TeamShopRevision; }
	TArray<TWeakObjectPtr<ACombatPlayerController>> Recipients; Players.GetKeys(Recipients);
	for(const auto& Weak:Recipients) if(auto* PC=Weak.Get()) PC->GetCombatEconomyComponent()->RefreshShopOffers();
}

void UDefenseDirectorComponent::ExpireTeamShop()
{
	if(FrozenTeamItems.IsEmpty()) return;
	TArray<FDefenseTeamAppliedModifier> Removed;
	for(auto& Pair:TeamPurchaseLedger)
	{
		auto& Record=Pair.Value;
		if(!Record.bEffectActive) continue;
		const auto* Item=FrozenTeamItems.Find(Pair.Key);
		const auto* Effect=Item?Cast<UDefenseTeamItemEffect>((*Item)->AcquireEffect):nullptr;
		if(Effect&&Effect->ReinforcementShieldData)
		{
			const int32 RemovedCount=Record.AppliedModifiers.RemoveAll([](const auto& Applied)
			{
				const auto* Unit=Applied.Unit.Get();
				return !Unit||!Unit->GetCombatModifierComponent()->FindRuntime(Applied.Handle);
			});
			const bool Active=Record.RemainingReinforcements>0||CountPendingReinforcements(Pair.Key)>0||!Record.AppliedModifiers.IsEmpty();
			if(RemovedCount||Active!=Record.bEffectActive) ++TeamShopRevision;
			Record.bEffectActive=Active; continue;
		}
		const bool bDepleted=Effect&&Effect->CoreShieldData&&!Record.AppliedModifiers.ContainsByPredicate([](const auto& Applied)
		{
			const auto* Unit=Applied.Unit.Get();
			const auto* Shield=Unit?Cast<UDefenseCoreShieldRuntime>(Unit->GetCombatModifierComponent()->FindRuntime(Applied.Handle)):nullptr;
			return Shield&&Shield->GetRemainingShield()>0;
		});
		const bool bExpired=View.Phase!=EDefensePhase::WaveCombat&&Record.ExpireWave>0&&View.WaveIndex+1>=Record.ExpireWave;
		if(!bDepleted&&!bExpired) continue;
		Record.bEffectActive=false; ++TeamShopRevision;
		Removed.Append(MoveTemp(Record.AppliedModifiers)); Record.AppliedModifiers.Reset();
		UE_LOG(LogCombat,Display,TEXT("DefenseTeamShopExpired Run=%s Item=%s EndWave=%d"),*View.RunId.ToString(),*Pair.Key.ToString(),Record.ExpireWave);
	}
	TGuardValue<bool> Guard(bTeamShopMutating,true);
	DefenseTeamShop::Revoke(MoveTemp(Removed));
}

void UDefenseDirectorComponent::ClearTeamShopEffects(const bool bClearLedger)
{
	TArray<FDefenseTeamAppliedModifier> Removed;
	for(auto& Pair:TeamPurchaseLedger)
	{
		Pair.Value.bEffectActive=false; Pair.Value.RemainingReinforcements=0; Pair.Value.ExcludedDeaths.Reset();
		Removed.Append(MoveTemp(Pair.Value.AppliedModifiers)); Pair.Value.AppliedModifiers.Reset();
	}
	for(auto& Pair:Players) { Pair.Value.PendingReinforcement={}; Pair.Value.ReinforcementDeathLife=0; }
	++TeamShopRevision; if(bClearLedger) TeamPurchaseLedger.Reset();
	TGuardValue<bool> Guard(bTeamShopMutating,true);
	DefenseTeamShop::Revoke(MoveTemp(Removed));
}

bool UDefenseDirectorComponent::ApplyTeamShopEffects(ACombatUnitCharacter* Hero)
{
	if(FrozenTeamItems.IsEmpty()||!IsCurrent(View.RunId,GetPhaseGeneration())||!IsValid(Hero)||Hero->GetWorld()!=GetWorld()
		||Hero->GetLifeState()!=ECombatLifeState::Alive||bTeamShopMutating) return true;
	TGuardValue<bool> Guard(bTeamShopMutating,true);
	for(const auto& Pair:FrozenTeamItems)
	{
		const auto* E=CastChecked<UDefenseTeamItemEffect>(Pair.Value->AcquireEffect); auto* Record=TeamPurchaseLedger.Find(Pair.Key);
		if(!E->ModifierData||!Record||!Record->bEffectActive) continue;
		if(Record->AppliedModifiers.ContainsByPredicate([&](const auto& A){return A.Unit.Get()==Hero;})) continue;
		FCombatModifierApplyRequest R; R.ModifierData=E->ModifierData; R.Source=Hero; R.SourceContext=Record->Source;
		if(E->ConditionalTargetRole!=EDefenseTeamTargetRole::None)
		{
			R.RuntimeParameterOverrides.Add(TEXT("target_role"),static_cast<float>(E->ConditionalTargetRole));
			R.RuntimeParameterOverrides.Add(TEXT("damage_fraction"),E->ConditionalDamageFraction);
		}
		const auto Applied=Hero->GetCombatModifierComponent()->ApplyModifier(R);
		if(!Applied.bSuccess||!Applied.Handle.IsValid()) return false;
		Record=TeamPurchaseLedger.Find(Pair.Key);
		if(!IsCurrent(View.RunId,GetPhaseGeneration())||!Record||!Record->bEffectActive)
		{ Hero->GetCombatModifierComponent()->RemoveModifier(Applied.Handle); return false; }
		Record->AppliedModifiers.Add({Hero,Applied.Handle});
	}
	return true;
}

bool UDefenseDirectorComponent::CanReceiveTeamItem(const FCombatItemAcquireContext& C,FGameplayTag& Failure) const
{
	Failure=CombatTags::Failure_ActionUnsupported;
	if(!IsCurrent(View.RunId,GetPhaseGeneration())||bTeamShopMutating||bChoiceMutating||!IsValid(C.Player)||!IsValid(C.Recipient)||!C.Definition
		||C.Player->GetWorld()!=GetWorld()||C.Recipient->GetWorld()!=GetWorld()||!C.Player->HasAuthority()) return false;
	const auto* R=Players.Find(C.Player);
	if(!R||R->Hero!=C.Recipient||C.Player->GetCommandedUnit()!=C.Recipient||C.Recipient->GetResourceOwnerPlayerController()!=C.Player) return false;
	if(!BuildTeamItemOffer(C.Player,C.Definition->GetPrimaryAssetId()).bCanPurchase) return false;
	const auto* E=CastChecked<UDefenseTeamItemEffect>(FrozenTeamItems.FindChecked(C.Definition->GetPrimaryAssetId())->AcquireEffect);
	// 同步领取不能接受 Hook 内仅排队的 Apply，否则失败退款后仍会生成无人持有的效果。
	if(E->CoreShieldData&&Objective->GetCombatModifierComponent()->IsDeferringOperations()) return false;
	if(E->ModifierData) for(const auto& Pair:Players)
		if(const auto* Hero=Pair.Value.Hero.Get();Hero&&Hero->GetLifeState()==ECombatLifeState::Alive&&Hero->GetCombatModifierComponent()->IsDeferringOperations()) return false;
	const auto* Instance=GetWorld()->GetSubsystem<UCombatItemSubsystem>()->FindItem(C.Source.ItemHandle);
	if(!Instance||!Instance->IsAcquiring()||Instance->GetDefinition()->GetPrimaryAssetId()!=C.Definition->GetPrimaryAssetId()) return false;
	Failure={}; return true;
}

bool UDefenseDirectorComponent::ReceiveTeamItem(const FCombatItemAcquireContext& C,FGameplayTag& Failure)
{
	if(!CanReceiveTeamItem(C,Failure)) return false;
	TGuardValue<bool> Guard(bTeamShopMutating,true);
	const auto Id=C.Definition->GetPrimaryAssetId(); const auto* E=CastChecked<UDefenseTeamItemEffect>(FrozenTeamItems.FindChecked(Id)->AcquireEffect);
	FDefenseTeamPurchaseRecord Purchase; Purchase.bPurchased=true; Purchase.bEffectActive=E->ModifierData!=nullptr||E->CoreShieldData!=nullptr||E->ReinforcementShieldData!=nullptr;
	Purchase.RemainingReinforcements=E->ReinforcementDeaths;
	Purchase.BuyerName=C.Player->PlayerState?C.Player->PlayerState->GetPlayerName():C.Player->GetName(); Purchase.Source=C.Source;
	Purchase.ExpireWave=E->DurationWaves?FMath::Min(View.TotalWaves,View.WaveIndex+1+E->DurationWaves):0;
	TArray<TWeakObjectPtr<ACombatUnitCharacter>> Heroes; for(const auto& Pair:Players) Heroes.Add(Pair.Value.Hero);
	if(E->ReinforcementShieldData) for(const auto& Target:Heroes)
		if(auto* Hero=Target.Get();Hero&&(Hero->GetLifeState()==ECombatLifeState::Dead||Hero->GetLifeState()==ECombatLifeState::Dying))
			Purchase.ExcludedDeaths.Add(Hero,Hero->GetLifeGeneration());
	if(E->ModifierData) for(const auto& Target:Heroes)
	{
		auto* Hero=Target.Get(); if(!Hero||Hero->GetLifeState()!=ECombatLifeState::Alive) continue;
		FCombatModifierApplyRequest R; R.ModifierData=E->ModifierData; R.Source=Hero; R.SourceContext=C.Source;
		if(E->ConditionalTargetRole!=EDefenseTeamTargetRole::None)
		{
			R.RuntimeParameterOverrides.Add(TEXT("target_role"),static_cast<float>(E->ConditionalTargetRole));
			R.RuntimeParameterOverrides.Add(TEXT("damage_fraction"),E->ConditionalDamageFraction);
		}
		const auto Applied=Hero->GetCombatModifierComponent()->ApplyModifier(R);
		if(Applied.Handle.IsValid()) Purchase.AppliedModifiers.Add({Hero,Applied.Handle});
		if(!Applied.bSuccess||!Applied.Handle.IsValid()||!IsCurrent(View.RunId,GetPhaseGeneration())||!Players.Contains(C.Player))
		{ DefenseTeamShop::Revoke(MoveTemp(Purchase.AppliedModifiers)); Failure=CombatTags::Failure_ActionUnsupported; return false; }
	}
	if(E->CoreShieldData)
	{
		auto* Core=Objective.Get();
		FCombatModifierApplyRequest R; R.ModifierData=E->CoreShieldData; R.Source=Core; R.SourceContext=C.Source;
		R.RuntimeParameterOverrides.Add(TEXT("shield_amount"),Core->GetCombatAttributeSet()->GetMaxHealth()*E->ObjectiveShieldFraction);
		const auto Applied=Core->GetCombatModifierComponent()->ApplyModifier(R);
		if(Applied.Handle.IsValid()) Purchase.AppliedModifiers.Add({Core,Applied.Handle});
		if(!Applied.bSuccess||!Applied.Handle.IsValid()||!IsCurrent(View.RunId,GetPhaseGeneration())||!Players.Contains(C.Player)||!IsCoreAlive())
		{ DefenseTeamShop::Revoke(MoveTemp(Purchase.AppliedModifiers)); Failure=CombatTags::Failure_ActionUnsupported; return false; }
	}
	if(E->ObjectiveHeal>0||E->ObjectiveHealFraction>0)
	{
		// 存活购买者是本次修复参与者；允许阵亡领取的旧规则仍回退核心自身来源。
        FCombatHealRequest R; R.Source=IsValid(C.Recipient)&&C.Recipient->GetLifeState()==ECombatLifeState::Alive?C.Recipient:Objective.Get(); R.Target=Objective.Get();
		R.Amount=E->ObjectiveHealFraction>0?Objective->GetCombatAttributeSet()->GetMaxHealth()*E->ObjectiveHealFraction:E->ObjectiveHeal;
		R.SourceContext=C.Source;
		const auto Applied=GetWorld()->GetSubsystem<UCombatHealSubsystem>()->Heal(R); if(!Applied.bSuccess) { Failure=Applied.FailureTag; return false; }
	}
	// 治疗已同步提交后，日志观察者可能终结对局。消费仍成功，但不能复活已清空的局内账本。
	if(!IsCurrent(View.RunId,GetPhaseGeneration())) { Failure={}; return true; }
	TeamPurchaseLedger.Add(Id,MoveTemp(Purchase)); ++TeamShopRevision; PublishTeamShop(); Publish();
	UE_LOG(LogCombat,Display,TEXT("DefenseTeamItemAcquired Run=%s Item=%s Player=%s Revision=%d"),*View.RunId.ToString(),*Id.ToString(),*C.Player->GetName(),TeamShopRevision);
	Failure={}; return true;
}

float UDefenseDirectorComponent::ReadObjectiveShield() const
{
	if(!Objective.IsValid()) return 0;
	for(const auto& Pair:TeamPurchaseLedger)
	{
		if(!Pair.Value.bEffectActive) continue;
		for(const auto& Applied:Pair.Value.AppliedModifiers)
			if(Applied.Unit==Objective)
				if(const auto* Shield=Cast<UDefenseCoreShieldRuntime>(Objective->GetCombatModifierComponent()->FindRuntime(Applied.Handle)))
					return Shield->GetRemainingShield();
	}
	return 0;
}
