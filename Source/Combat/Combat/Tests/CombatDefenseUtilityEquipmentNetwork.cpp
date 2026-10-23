#include "Combat/Tests/CombatDefenseNetworkScenario.h"
#include "Combat/Defense/DefensePlayerController.h"
#include "Combat/Defense/DefenseGameState.h"
#include "Combat/Defense/DefenseGameMode.h"
#include "Combat/Defense/DefenseDirectorComponent.h"
#include "Combat/Defense/DefenseObjective.h"
#include "Combat/Economy/CombatEconomyComponent.h"
#include "Combat/Items/CombatInventoryComponent.h"
#include "Combat/Attributes/CombatAttributeSet.h"
#include "Combat/Ability/CombatAbilitySystemComponent.h"
#include "Combat/Modifiers/CombatModifierComponent.h"
#include "Combat/Order/CombatOrderComponent.h"
#include "Combat/Targeting/CombatTargetingSubsystem.h"
#include "Combat/Data/CombatDefinitionData.h"
#include "Combat/Combat/CombatDamageSubsystem.h"
#include "Combat/View/CombatUnitViewComponent.h"
#include "Combat/Unit/CombatUnitCharacter.h"
#include "EngineUtils.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

void ACombatDefenseNetworkScenario::ExerciseUtilityEquipment()
{
    const auto* State=GetWorld()->GetGameState<ADefenseGameState>();if(!State)return;const auto View=State->GetRunView();const double Now=GetWorld()->GetTimeSeconds();
    if(HasAuthority())
    {
        TArray<ADefensePlayerController*> Players;for(TActorIterator<ADefensePlayerController> It(GetWorld());It;++It)if(It->GetCommandedUnit())Players.Add(*It);
        if(Players.Num()!=ExpectedPlayers)return;auto* Mode=GetWorld()->GetAuthGameMode<ADefenseGameMode>();auto* Core=Mode->GetDefenseObjective();
        if(!bUtilityPrepared)
        {
            // 仅显式fixture通过正式开局入口选择守卫与补给预算；生产不自动选角、购物或伤害核心。
            for(auto* PC:Players)
            {
                FName Failure;if(!Mode->GetDefenseDirector()->SelectStartingHero(PC,Mode->GetGuardianClass(),Failure))
                {Finish(false,TEXT("UtilityGuardianSelectionFailed ")+Failure.ToString());return;}
                auto* E=PC->GetCombatEconomyComponent();E->AddGold(1500,TEXT("UtilityNetworkFixture"));EquipmentInitialGold.Add(PC,E->GetGold());
            }
            FCombatDamageRequest R;R.Source=R.Target=Core;R.Amount=2000;R.DamageType=ECombatDamageType::Pure;
            GetWorld()->GetSubsystem<UCombatDamageSubsystem>()->DealDamage(R);CoreAtWaveStart=Core->GetCombatAttributeSet()->GetHealth();bUtilityPrepared=true;return;
        }
        if(View.Phase!=EDefensePhase::Preparation){Finish(false,TEXT("UtilityFixtureMissedPreparation"));return;}
        for(auto* PC:Players)if(PC->GetCommandedUnit()->GetCombatInventoryComponent()->GetItemCount()!=2)return;
        if(Core->GetCombatAttributeSet()->GetHealth()<CoreAtWaveStart+899)return;
        if(UtilityServerStage<2)
        {
            if(UtilityServerStage==1)for(auto* PC:Players)
            {
                auto* A=PC->GetCommandedUnit()->GetCombatAbilitySystemComponent();const auto* Q=A->FindCombatAbilitySpecByDefinitionId(FPrimaryAssetId(TEXT("CombatAbility"),TEXT("guardian_shield_bash")));
                if(!Q||A->GetCombatAbilityCooldownRemaining(Q->Handle)<=0)return;
            }
            for(auto* PC:Players)
            {
                auto* Hero=PC->GetCommandedUnit();auto* A=Hero->GetCombatAbilitySystemComponent();const auto* Skill=A->FindCombatAbilitySpecByDefinitionId(FPrimaryAssetId(TEXT("CombatAbility"),UtilityServerStage==0?TEXT("guardian_shield_bash"):TEXT("guardian_ground_slam")));
                if(!Skill){Finish(false,TEXT("UtilitySkillMissing"));return;}
                FCombatOrderRequest O;O.AbilitySpecHandle=Skill->Handle;O.Type=UtilityServerStage==0?ECombatOrderType::CastPoint:ECombatOrderType::CastNoTarget;
                if(UtilityServerStage==0)
                {
                    // 两英雄相邻，固定向前点可能被队友挡住。自身落点是合法Q意图，仍经过正式LOS/范围及Order复核。
                    O.bHasTargetLocation=true;O.TargetLocation=Hero->GetActorLocation();
                    const auto* Data=A->GetCombatAbilityData(Skill->Handle);
                    if(!Data||!GetWorld()->GetSubsystem<UCombatTargetingSubsystem>()->ValidatePointTarget(Hero,O.TargetLocation,Data->TargetingRules).bValid)
                    {Finish(false,TEXT("UtilityFixtureNoLegalQPoint"));return;}
                }
                if(!Hero->GetCombatOrderComponent()->IssueOrder(O).bSuccess){Finish(false,TEXT("UtilityOrderRejected"));return;}
            }
            ++UtilityServerStage;return;
        }
        if(EquipmentSamples.IsEmpty())
        {
            int32 StonePayers=0;
            for(auto* PC:Players)
            {
                auto* Hero=PC->GetCommandedUnit();auto* A=Hero->GetCombatAbilitySystemComponent();TArray<float> Durations;
                for(const TCHAR* Name:{TEXT("guardian_shield_bash"),TEXT("guardian_ground_slam")})
                {
                    const auto* S=A->FindCombatAbilitySpecByDefinitionId(FPrimaryAssetId(TEXT("CombatAbility"),Name));double End=0;float Duration=0;
                    if(S)A->GetCombatAbilityCooldownWindow(S->Handle,End,Duration);if(Duration==0)return;Durations.Add(Duration);
                }
                const int64 Paid=EquipmentInitialGold.FindChecked(PC)-PC->GetCombatEconomyComponent()->GetGold();
                if(Paid==620)++StonePayers;else if(Paid!=440){Finish(false,TEXT("UtilityGoldMismatch"));return;}
                if(!FMath::IsNearlyEqual(Durations[0],7.36f,.001f)||!FMath::IsNearlyEqual(Durations[1],12.88f,.001f))
                {Finish(false,TEXT("UtilityServerCooldownMismatch"));return;}
            }
            if(StonePayers!=1||!FMath::IsNearlyEqual(Core->GetCombatAttributeSet()->GetHealth()-CoreAtWaveStart,900.f,.01f))
            {Finish(false,TEXT("UtilityCoreHealMismatch"));return;}
            for(auto* PC:Players){auto& S=EquipmentSamples.AddDefaulted_GetRef();S.Hero=PC->GetCommandedUnit();S.Attributes={7.36f,12.88f,900.f};}
            ForceNetUpdate();UtilityFinishAt=Now+3;
        }
        if(Now>=UtilityFinishAt)Finish(true,FString::Printf(TEXT("UtilityEquipmentNetwork Server=1 Players=%d TwoItems=1 ExactGold=1 CoreRepair=900 Q=7.36 E=12.88 Run=%s"),ExpectedPlayers,*View.RunId.ToString()));return;
    }
    auto* PC=Cast<ADefensePlayerController>(GetWorld()->GetFirstPlayerController());auto* Hero=PC?PC->GetCommandedUnit():nullptr;
    if(!PC||!PC->IsLocalController()||!Hero)return;
    // 客户端只读复制的单位/HUD身份；InitializedUnitDefinitionId是服务器缓存，不能用于客户端等待条件。
    if(Hero->GetCombatUnitViewComponent()->GetHUDOwnerView().UnitDefinitionId.PrimaryAssetName!=TEXT("defense_guardian"))return;
    if(Hero->HasAuthority()||Hero->GetCombatModifierComponent()->GetActiveModifierCount()!=0){Finish(false,TEXT("UtilityClientAuthorityLeak"));return;}
    auto* E=PC->GetCombatEconomyComponent();int32 Seat=0;FParse::Value(FCommandLine::Get(),TEXT("DefenseSmokeSeat="),Seat);
    if(bUtilityAwaiting)
    {
        const auto Receipt=PC->GetLastEconomyResult();if(Receipt.RequestId!=UtilityRequest)return;
        if(Receipt.bSuccess!=bUtilityExpectSuccess){Finish(false,FString::Printf(TEXT("UtilityReceiptMismatch Step=%d Failure=%s"),UtilityStep,*Receipt.FailureTag.ToString()));return;}
        if(Receipt.bSuccess&&(E->GetEconomyRevision()!=Receipt.EconomyRevision||E->GetInventoryRevision()!=Receipt.InventoryRevision))return;
        bUtilityAwaiting=false;++UtilityStep;
    }
    auto Submit=[&](const TCHAR* Name,bool Expected)
    {
        const FPrimaryAssetId Id(TEXT("CombatItem"),Name);const auto* Offer=E->FindShopOffer(Id);if(!Offer)return;
        // 团队商品的修订属于团队账本，装备才使用阶段代次；始终原样回传各自Offer身份。
        if(Expected&&(!Offer->bCanPurchase||Offer->ContextId!=View.RunId||Offer->Revision<=0))return;
        FCombatEconomyRequest R;R.RequestId=PC->ReserveCombatRequestId();R.ItemDefinitionId=Id;R.CommandBindingGeneration=PC->GetCommandBindingGeneration();
        R.ExpectedEconomyRevision=E->GetEconomyRevision();R.ExpectedInventoryRevision=E->GetInventoryRevision();R.PurchaseContextId=Offer->ContextId;R.PurchaseContextRevision=Offer->Revision;
        UtilityRequest=R.RequestId;bUtilityAwaiting=true;bUtilityExpectSuccess=Expected;
        UE_LOG(LogTemp,Display,TEXT("UtilityNetworkRequest Seat=%d Step=%d Request=%d Item=%s Expected=%d"),Seat,UtilityStep,R.RequestId,Name,Expected);
        PC->ServerSubmitEconomyRequest(R);PC->ServerSubmitEconomyRequest(R);
    };
    if(UtilityStep==0&&E->GetGold()>1000){UtilityInitialGold=E->GetGold();Submit(TEXT("item_mana_sigil"),true);}
    else if(UtilityStep==1)Submit(TEXT("item_mana_sigil"),false);
    else if(UtilityStep==2)Submit(TEXT("item_repair_lens"),true);
    else if(UtilityStep==3)Submit(TEXT("item_stone_guard"),false);
    else if(UtilityStep==4){if(Seat==1)Submit(TEXT("team_emergency_stone"),true);else ++UtilityStep;}
    if(UtilityStep!=5||EquipmentSamples.Num()!=ExpectedPlayers)return;
    const auto HUD=Hero->GetCombatUnitViewComponent()->GetHUDOwnerView();const auto* Q=HUD.Abilities.FindByPredicate([](const auto& A){return A.DefinitionId.PrimaryAssetName==TEXT("guardian_shield_bash");});
    const auto* S=HUD.Abilities.FindByPredicate([](const auto& A){return A.DefinitionId.PrimaryAssetName==TEXT("guardian_ground_slam");});
    if(!Q||!S||!FMath::IsNearlyEqual(Q->CooldownDuration,7.36f,.001f)||!FMath::IsNearlyEqual(S->CooldownDuration,12.88f,.001f)||!FMath::IsNearlyEqual(View.ObjectiveHealth,3900.f,.01f))return;
    int32 Count=0;for(const auto& Item:E->GetEconomyView().InventoryItems)if(Item.Handle.IsValid())++Count;
    if(Count!=2||E->GetGold()!=UtilityInitialGold-(Seat==1?620:440)){Finish(false,TEXT("UtilityOwnerProjectionMismatch"));return;}
    Finish(true,FString::Printf(TEXT("UtilityEquipmentNetwork Client=1 Seat=%d RPCSteps=%d TwoItems=1 CoreRepair=900 Q=7.36 E=12.88 ClientAuthority=0 Run=%s"),Seat,Seat==1?5:4,*View.RunId.ToString()));
}
