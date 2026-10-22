#include "Combat/Tests/CombatDefenseNetworkScenario.h"
#include "Combat/Defense/DefensePlayerController.h"
#include "Combat/Defense/DefenseGameState.h"
#include "Combat/Defense/DefenseGameMode.h"
#include "Combat/Defense/DefenseDirectorComponent.h"
#include "Combat/Defense/DefenseData.h"
#include "Combat/Defense/DefenseObjective.h"
#include "Combat/Economy/CombatEconomyComponent.h"
#include "Combat/Items/CombatInventoryComponent.h"
#include "Combat/Attributes/CombatAttributeSet.h"
#include "Combat/Unit/CombatUnitCharacter.h"
#include "EngineUtils.h"

void ACombatDefenseNetworkScenario::ExerciseEquipment()
{
    const auto* State=GetWorld()->GetGameState<ADefenseGameState>(); if(!State)return;
    const auto View=State->GetRunView();const double Now=GetWorld()->GetTimeSeconds();
    if(View.Phase==EDefensePhase::Defeat){Finish(false,TEXT("EquipmentRunDefeated"));return;}
    if(HasAuthority())
    {
        TArray<ADefensePlayerController*> Players;
        for(TActorIterator<ADefensePlayerController> It(GetWorld());It;++It)if(It->GetCommandedUnit())Players.Add(*It);
        if(Players.Num()!=ExpectedPlayers)return;
        auto* Run=const_cast<UDefenseRunData*>(GetWorld()->GetAuthGameMode<ADefenseGameMode>()->GetDefenseDirector()->GetFrozenRunData());
        if(!bEquipmentPrepared)
        {
            // 只调整显式测试当前局的等待与预算；生产资产和玩法不自动购物或清场。
            Run->IntermissionDuration=5;Run->ChoiceDuration=1;
            for(UDefenseWaveData* Wave:Run->Waves)for(auto& Group:Wave->SpawnGroups)Group.SpawnInterval=.15f;
            for(auto* PC:Players)
            {
                PC->GetCombatEconomyComponent()->AddGold(1500,TEXT("EquipmentSmokeFixture"));
                EquipmentInitialGold.Add(PC,PC->GetCombatEconomyComponent()->GetGold());
            }
            bEquipmentPrepared=true;
        }
        if(View.Phase==EDefensePhase::WaveCombat)
        {
            if(EquipmentWave!=View.WaveIndex){EquipmentWave=View.WaveIndex;EquipmentWaveAt=Now;}
            if(View.PendingSpawns==0&&Now-EquipmentWaveAt>4)
            {
                TArray<ADefenseEnemy*> Enemies;
                for(TActorIterator<ADefenseEnemy> It(GetWorld());It;++It)if(!It->IsA<ADefenseObjective>()&&It->GetLifeState()==ECombatLifeState::Alive)Enemies.Add(*It);
                for(auto* Enemy:Enemies)Kill(Enemy);
            }
        }
        if(View.Phase==EDefensePhase::Victory)
        {
            if(EquipmentFinishAt==0)
            {
                int64 Rewards=240;for(const UDefenseWaveData* Wave:Run->Waves)Rewards+=Wave->GoldReward;
                for(auto* PC:Players)
                {
                    const int64 Expected=EquipmentInitialGold.FindChecked(PC)+Rewards-820;
                    if(PC->GetCombatEconomyComponent()->GetGold()!=Expected||PC->GetCommandedUnit()->GetCombatInventoryComponent()->GetItemCount()!=4)
                    {Finish(false,FString::Printf(TEXT("EquipmentGoldOrCount Gold=%lld Expected=%lld Count=%d"),PC->GetCombatEconomyComponent()->GetGold(),Expected,PC->GetCommandedUnit()->GetCombatInventoryComponent()->GetItemCount()));return;}
                    const auto* Stats=PC->GetCommandedUnit()->GetCombatAttributeSet();
                    auto& Sample=EquipmentSamples.AddDefaulted_GetRef();Sample.Hero=PC->GetCommandedUnit();
                    Sample.Attributes={Stats->GetBaseAttackTime(),Stats->GetMaxHealth(),Stats->GetCooldownReductionPct()};
                }
                ForceNetUpdate();EquipmentFinishAt=Now+1;
            }
            if(Now>=EquipmentFinishAt)Finish(true,FString::Printf(TEXT("EquipmentNetwork Server=1 Players=%d FourItems=1 ExactGold=1 Run=%s"),ExpectedPlayers,*View.RunId.ToString()));
        }
        return;
    }
    auto* PC=Cast<ADefensePlayerController>(GetWorld()->GetFirstPlayerController());if(!PC||!PC->IsLocalController()||!PC->GetCommandedUnit())return;
    if(PC->GetCommandedUnit()->HasAuthority()){Finish(false,TEXT("EquipmentClientAuthorityLeak"));return;}
    auto* Economy=PC->GetCombatEconomyComponent();
    if(bEquipmentAwaiting)
    {
        const auto Result=PC->GetLastEconomyResult();
        if(Result.RequestId!=EquipmentRequestId)return;
        if(Result.bSuccess!=bEquipmentExpectSuccess)
        {Finish(false,FString::Printf(TEXT("EquipmentReceipt Step=%d Expected=%d Success=%d Failure=%s"),EquipmentStep,bEquipmentExpectSuccess,Result.bSuccess,*Result.FailureTag.ToString()));return;}
        if (Result.bSuccess && (Economy->GetEconomyRevision()!=Result.EconomyRevision || Economy->GetInventoryRevision()!=Result.InventoryRevision)) return;
        bEquipmentAwaiting=false;++EquipmentStep;
    }
    auto Submit=[&](const TCHAR* Name,bool Expected)
    {
        const FPrimaryAssetId Id(TEXT("CombatItem"),Name);const auto* Offer=Economy->FindShopOffer(Id);if(!Offer)return;
        if (Expected && (!Offer->bCanPurchase || Offer->Revision != View.PhaseGeneration || Offer->ContextId != View.RunId)) return;
        FCombatEconomyRequest Request;Request.RequestId=PC->ReserveCombatRequestId();Request.CommandBindingGeneration=PC->GetCommandBindingGeneration();
        Request.ItemDefinitionId=Id;Request.ExpectedEconomyRevision=Economy->GetEconomyRevision();Request.ExpectedInventoryRevision=Economy->GetInventoryRevision();
        Request.PurchaseContextId=Offer->ContextId;Request.PurchaseContextRevision=Offer->Revision;
        EquipmentRequestId=Request.RequestId;bEquipmentAwaiting=true;bEquipmentExpectSuccess=Expected;
        PC->ServerSubmitEconomyRequest(Request);PC->ServerSubmitEconomyRequest(Request);
    };
    if(EquipmentStep==0&&View.Phase==EDefensePhase::Preparation)
    {
        const auto* Offer=Economy->FindShopOffer(FPrimaryAssetId(TEXT("CombatItem"),TEXT("item_stone_guard")));
        if(Offer&&Offer->bCanPurchase&&Economy->GetGold()>820)Submit(TEXT("item_stone_guard"),true);
    }
    else if(EquipmentStep==1)Submit(TEXT("item_stone_guard"),false);
    else if(EquipmentStep==2)Submit(TEXT("item_iron_bow"),true);
    else if(EquipmentStep==3)Submit(TEXT("item_swift_string"),false);
    else if(EquipmentStep==4&&View.Phase==EDefensePhase::WaveCombat&&View.WaveIndex==0)Submit(TEXT("item_swift_string"),false);
    else if(EquipmentStep==5&&View.Phase==EDefensePhase::Intermission&&View.WaveIndex==0)Submit(TEXT("item_swift_string"),true);
    else if(EquipmentStep==6)Submit(TEXT("item_hunter_scope"),true);
    if(View.Phase==EDefensePhase::Victory&&EquipmentStep==7&&EquipmentSamples.Num()==ExpectedPlayers)
    {
        int32 Owned=0;for(const auto& Item:Economy->GetEconomyView().InventoryItems)if(Item.Handle.IsValid())++Owned;
        if(Owned!=4){Finish(false,TEXT("EquipmentOwnerInventoryMismatch"));return;}
        for(const auto& Sample:EquipmentSamples)
        {
            if(!Sample.Hero||Sample.Attributes.Num()!=3)return;
            const auto* Stats=Sample.Hero->GetCombatAttributeSet();
            if(!FMath::IsNearlyEqual(Stats->GetBaseAttackTime(),Sample.Attributes[0],.01f)
                ||!FMath::IsNearlyEqual(Stats->GetMaxHealth(),Sample.Attributes[1],.01f)
                ||!FMath::IsNearlyEqual(Stats->GetCooldownReductionPct(),Sample.Attributes[2],.01f))return;
        }
        Finish(true,FString::Printf(TEXT("EquipmentNetwork Client=1 RPCSteps=7 FourItems=1 PublicAttributes=1 ClientAuthority=0 Run=%s"),*View.RunId.ToString()));
    }
}
