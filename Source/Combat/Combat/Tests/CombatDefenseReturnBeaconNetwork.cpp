#include "Combat/Tests/CombatDefenseNetworkScenario.h"
#include "Combat/Defense/DefensePlayerController.h"
#include "Combat/Defense/DefenseGameState.h"
#include "Combat/Defense/DefenseGameMode.h"
#include "Combat/Defense/DefenseDirectorComponent.h"
#include "Combat/Economy/CombatEconomyComponent.h"
#include "Combat/Items/CombatInventoryComponent.h"
#include "Combat/Unit/CombatUnitCharacter.h"
#include "Combat/Unit/CombatUnitLifecycleComponent.h"
#include "Combat/View/CombatUnitViewComponent.h"
#include "Combat/Modifiers/CombatModifierComponent.h"
#include "Combat/Ability/CombatAbilitySystemComponent.h"
#include "Combat/Attributes/CombatAttributeSet.h"
#include "Combat/AI/CombatAIBrainComponent.h"
#include "Combat/Motion/CombatMotionComponent.h"
#include "Combat/Combat/CombatDamageSubsystem.h"
#include "Combat/Order/CombatOrderComponent.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

void ACombatDefenseNetworkScenario::ExerciseReturnBeacon()
{
    const auto* State=GetWorld()->GetGameState<ADefenseGameState>(); if(!State)return;
    const auto Run=State->GetRunView(); const double Now=GetWorld()->GetTimeSeconds();
    const FPrimaryAssetId Recall(TEXT("CombatAbility"),TEXT("defense_recall"));
    if(HasAuthority())
    {
        auto* Mode=GetWorld()->GetAuthGameMode<ADefenseGameMode>(); if(!Mode)return; auto* D=Mode->GetDefenseDirector();
        TArray<ADefensePlayerController*> Players;
        for(TActorIterator<ADefensePlayerController> It(GetWorld());It;++It)if(It->GetCommandedUnit())Players.Add(*It);
        if(Players.Num()!=ExpectedPlayers)return;
        // 用例只隔离回城，不把敌人伤害/奖励误记为技能效果；真实波次和复活Monitor继续运行。
        for(TActorIterator<ACombatUnitCharacter> It(GetWorld());It;++It)if(It->GetCombatAIBrainComponent()->IsAutonomous()&&It->GetLifeState()==ECombatLifeState::Alive)Kill(*It);
        if(D->GetTerminalResult().bTerminal){Finish(false,TEXT("RecallRunEndedEarly"));return;}
        if(ReturnBeaconPhase==0)
        {
            for(int32 Index=0;Index<Players.Num();++Index)
            {
                auto* PC=Players[Index]; FName Failure;
                if(Index==1&&!D->SelectStartingHero(PC,Mode->GetGuardianClass(),Failure)){Finish(false,TEXT("RecallGuardianSelection ")+Failure.ToString());return;}
                auto* H=PC->GetCommandedUnit(); PC->GetCombatEconomyComponent()->AddGold(1000,TEXT("ReturnBeaconNetworkFixture"));
                FVector Home; if(!D->ResolveRecallDestination(H,Home)||!H->GetCombatMotionComponent()->TryTeleport(Home+FVector(1100,500+300*Index,0),{},{}).bSuccess)
                {Finish(false,TEXT("RecallFixtureDisplacement"));return;}
                H->GetCharacterMovement()->DisableMovement();
                FCombatOrderRequest Stop; Stop.Type=ECombatOrderType::Stop; H->GetCombatOrderComponent()->IssueOrder(Stop,false);
                ReturnBeaconOrigins.Add(H,H->GetActorLocation()); ReturnBeaconLives.Add(H,H->GetLifeGeneration());
            }
            ReturnBeaconAt=0; ReturnBeaconPhase=1; return;
        }
        bool AllChannel=true,AllItems=true,AllReady=true,AllHome=true,AllRevived=true;
        for(auto* PC:Players)
        {
            auto* H=PC->GetCommandedUnit(); auto* A=H->GetCombatAbilitySystemComponent(); const auto* Spec=A->FindCombatAbilitySpecByDefinitionId(Recall);
            if(!Spec){Finish(false,TEXT("RecallMissingProductionAbility"));return;}
            const auto& V=H->GetCombatUnitViewComponent()->GetUnitView();
            AllChannel &= Spec->IsActive()&&V.AbilityPhase==ECombatAbilityViewPhase::Channeling;
            AllItems &= H->GetCombatInventoryComponent()->GetItemCount()==1&&FMath::IsNearlyEqual(H->GetCombatModifierComponent()->GetActiveRuntimeParameterSum(TEXT("defense_recall_reduction_seconds")),1.f);
            AllReady &= A->GetCombatAbilityCooldownRemaining(Spec->Handle)<=0;
            AllHome &= !Spec->IsActive()&&FVector::Dist2D(H->GetActorLocation(),ReturnBeaconOrigins.FindChecked(H))>700;
            AllRevived &= H->GetLifeState()==ECombatLifeState::Alive&&H->GetLifeGeneration()==ReturnBeaconLives.FindChecked(H)+1;
            if(AllChannel && (ReturnBeaconPhase==1||ReturnBeaconPhase==3))
            {
                const float Expected=ReturnBeaconPhase==1?3.f:2.f;
                if(!FMath::IsNearlyEqual(float(V.AbilityServerEndTime-V.AbilityServerStartTime),Expected,.01f)) {Finish(false,TEXT("RecallServerChannelMismatch"));return;}
                double End=0;float Duration=0;A->GetCombatAbilityCooldownWindow(Spec->Handle,End,Duration);
                if(!FMath::IsNearlyEqual(Duration,30.f)){Finish(false,TEXT("RecallServerCooldownMismatch"));return;}
            }
        }
        if(ReturnBeaconPhase==1&&AllChannel)
        {
            if(ReturnBeaconAt==0)ReturnBeaconAt=Now;
            if(Now-ReturnBeaconAt<1)return;
            for(auto* PC:Players)
            {FCombatDamageRequest R;R.Source=R.Target=PC->GetCommandedUnit();R.Amount=10;R.DamageType=ECombatDamageType::Pure;GetWorld()->GetSubsystem<UCombatDamageSubsystem>()->DealDamage(R);}
            ReturnBeaconPhase=2; ReturnBeaconAt=0;
        }
        else if(ReturnBeaconPhase==2&&AllItems&&AllReady) {ReturnBeaconPhase=3;ReturnBeaconAt=0;}
        else if(ReturnBeaconPhase==3)
        {
            if(AllChannel&&ReturnBeaconAt==0)ReturnBeaconAt=Now;
            if(ReturnBeaconAt>0&&Now-ReturnBeaconAt>2.5&&AllHome){ReturnBeaconPhase=4;ReturnBeaconAt=Now;}
        }
        else if(ReturnBeaconPhase==4&&Now-ReturnBeaconAt>2)
        {
            ReturnBeaconDeathAt=Now;for(auto* PC:Players)Kill(PC->GetCommandedUnit());ReturnBeaconPhase=5;
        }
        else if(ReturnBeaconPhase==5&&AllRevived)
        {
            const double Delay=Now-ReturnBeaconDeathAt;
            if(Delay<5.9||Delay>7){Finish(false,FString::Printf(TEXT("RecallRespawnDelayMismatch %.3f"),Delay));return;}
            for(auto* PC:Players)
            {
                auto* H=PC->GetCommandedUnit(); const auto* Spec=H->GetCombatAbilitySystemComponent()->FindCombatAbilitySpecByDefinitionId(Recall);
                if(!FMath::IsNearlyEqual(H->GetCombatModifierComponent()->GetActiveRuntimeParameterSum(TEXT("defense_respawn_reduction_seconds")),2.f)
                    ||H->GetCombatAbilitySystemComponent()->GetCombatAbilityCooldownRemaining(Spec->Handle)<=10)
                {Finish(false,TEXT("RecallRespawnRebuildOrCooldown"));return;}
            }
            UE_LOG(LogTemp,Display,TEXT("ReturnBeaconActualRespawn Delay=%.3f Players=%d"),Delay,Players.Num());ReturnBeaconPhase=6;ReturnBeaconAt=Now;
        }
        else if(ReturnBeaconPhase==6&&Now-ReturnBeaconAt>2)
            Finish(true,FString::Printf(TEXT("ReturnBeaconNetwork Server=1 Players=%d Base=3 Equipped=2 Cooldown=30 DamageCancel=1 SafeReturn=1 Respawn=6 Life=1 EquipmentRebuilt=1 Run=%s"),ExpectedPlayers,*Run.RunId.ToString()));
        return;
    }
    auto* PC=Cast<ADefensePlayerController>(GetWorld()->GetFirstPlayerController()); auto* H=PC?PC->GetCommandedUnit():nullptr;
    if(!PC||!PC->IsLocalController()||!H)return;
    auto* Views=H->GetCombatUnitViewComponent(); const auto& V=Views->GetUnitView(); const auto& HUD=Views->GetHUDOwnerView();
    const auto* Ability=HUD.Abilities.FindByPredicate([&](const auto& A){return A.DefinitionId==Recall;});if(!Ability)return;
    if(H->HasAuthority()||H->GetCombatModifierComponent()->GetActiveModifierCount()!=0){Finish(false,TEXT("RecallClientAuthorityLeak"));return;}
    auto Submit=[&]()
    { FCombatOrderRequest O;O.Type=ECombatOrderType::CastNoTarget;O.AbilitySpecHandle=Ability->SpecHandle;
      FCombatOrderBatchRequest B;B.RequestId=PC->ReserveCombatRequestId();B.Orders.Add(O);H->ServerIssueOrderBatch(B);H->ServerIssueOrderBatch(B); };
    if(ReturnBeaconStep==0&&ReturnBeaconPhase==1&&Elapsed>1)
    {ReturnBeaconOrigin=H->GetActorLocation();ReturnBeaconLife=H->GetLifeGeneration();Submit();ReturnBeaconStep=1;}
    if(ReturnBeaconStep==1&&V.AbilityPhase==ECombatAbilityViewPhase::Channeling)
    {
        if(!FMath::IsNearlyEqual(float(V.AbilityServerEndTime-V.AbilityServerStartTime),3.f,.01f)||!FMath::IsNearlyEqual(Ability->CooldownDuration,30.f))return;
        bReturnBeaconBaseSeen=true;ReturnBeaconStep=2;
    }
    if(ReturnBeaconStep==2&&ReturnBeaconPhase==2&&V.AbilityPhase!=ECombatAbilityViewPhase::Channeling)
    {
        if(FVector::Dist2D(H->GetActorLocation(),ReturnBeaconOrigin)>10){Finish(false,TEXT("RecallInterruptedMoved"));return;}
        auto* E=PC->GetCombatEconomyComponent();const FPrimaryAssetId Item(TEXT("CombatItem"),TEXT("item_return_beacon"));const auto* Offer=E->FindShopOffer(Item);
        if(!Offer||!Offer->bCanPurchase)return;FCombatEconomyRequest R;R.RequestId=PC->ReserveCombatRequestId();R.ItemDefinitionId=Item;
        R.CommandBindingGeneration=PC->GetCommandBindingGeneration();R.ExpectedEconomyRevision=E->GetEconomyRevision();R.ExpectedInventoryRevision=E->GetInventoryRevision();
        R.PurchaseContextId=Offer->ContextId;R.PurchaseContextRevision=Offer->Revision;ReturnBeaconRequest=R.RequestId;
        PC->ServerSubmitEconomyRequest(R);PC->ServerSubmitEconomyRequest(R);ReturnBeaconStep=3;
    }
    if(ReturnBeaconStep==3)
    {
        const auto R=PC->GetLastEconomyResult();if(R.RequestId!=ReturnBeaconRequest)return;
        if(!R.bSuccess||R.GoldDelta!=-150){Finish(false,TEXT("RecallBeaconPurchase ")+R.FailureTag.ToString());return;}
        if(ReturnBeaconPhase==3){Submit();ReturnBeaconStep=4;}
    }
    if(ReturnBeaconStep==4&&V.AbilityPhase==ECombatAbilityViewPhase::Channeling)
    {
        if(!FMath::IsNearlyEqual(float(V.AbilityServerEndTime-V.AbilityServerStartTime),2.f,.01f)||!FMath::IsNearlyEqual(Ability->CooldownDuration,30.f))return;
        bReturnBeaconShortSeen=true;ReturnBeaconStep=5;
    }
    if(ReturnBeaconStep==5&&ReturnBeaconPhase==4&&V.AbilityPhase!=ECombatAbilityViewPhase::Channeling)
    {
        if(H->GetLifeGeneration()!=ReturnBeaconLife||FVector::Dist2D(H->GetActorLocation(),ReturnBeaconOrigin)<700){Finish(false,TEXT("RecallClientArrivalMismatch"));return;}
        ReturnBeaconStep=6;
    }
    if(ReturnBeaconStep==6&&V.LifeState==ECombatLifeState::Dead)ReturnBeaconStep=7;
    if(ReturnBeaconStep==7&&ReturnBeaconPhase==6&&V.LifeState==ECombatLifeState::Alive&&H->GetLifeGeneration()==ReturnBeaconLife+1)
    {
        int32 Items=0;for(const auto& I:PC->GetCombatEconomyComponent()->GetHeroInventoryView().Items)if(I.Handle.IsValid())++Items;
        if(!bReturnBeaconBaseSeen||!bReturnBeaconShortSeen||Items!=1||Ability->CooldownEndTime<=Now+10)return;
        int32 Seat=0;FParse::Value(FCommandLine::Get(),TEXT("DefenseSmokeSeat="),Seat);
        Finish(true,FString::Printf(TEXT("ReturnBeaconNetwork Client=1 Seat=%d Base=3 Equipped=2 Cooldown=30 DamageCancel=1 SafeReturn=1 Respawn=6 Life=1 Inventory=1 Requests=3 Replays=3 ClientAuthority=0 Run=%s"),Seat,*Run.RunId.ToString()));
    }
}
