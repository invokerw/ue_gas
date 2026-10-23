#include "Combat/Tests/CombatDefenseNetworkScenario.h"
#include "Combat/Defense/DefensePlayerController.h"
#include "Combat/Defense/DefenseGameState.h"
#include "Combat/Defense/DefenseGameMode.h"
#include "Combat/Defense/DefenseDirectorComponent.h"
#include "Combat/Defense/DefenseChoiceComponent.h"
#include "Combat/Economy/CombatEconomyComponent.h"
#include "Combat/Items/CombatInventoryComponent.h"
#include "Combat/Unit/CombatUnitCharacter.h"
#include "Combat/View/CombatUnitViewComponent.h"
#include "Combat/Modifiers/CombatModifierComponent.h"
#include "EngineUtils.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

void ACombatDefenseNetworkScenario::ExerciseMerchantSeal()
{
    const ADefenseGameState* State = GetWorld()->GetGameState<ADefenseGameState>();
    if (!State) return;
    const auto View = State->GetRunView();
    const double Now = GetWorld()->GetTimeSeconds();
    const FPrimaryAssetId Seal(TEXT("CombatItem"), TEXT("item_merchants_seal"));
    const FPrimaryAssetId Lens(TEXT("CombatItem"), TEXT("item_repair_lens"));
    if (HasAuthority())
    {
        TArray<ADefensePlayerController*> Players;
        for (TActorIterator<ADefensePlayerController> It(GetWorld()); It; ++It)
            if (It->GetCommandedUnit()) Players.Add(*It);
        if (Players.Num() != ExpectedPlayers) return;
        ADefenseGameMode* Mode = GetWorld()->GetAuthGameMode<ADefenseGameMode>();
        if (!Mode) return;
        if (!bMerchantPrepared)
        {
            for (ADefensePlayerController* PC : Players)
            {
                FName Failure;
                if (!Mode->GetDefenseDirector()->SelectStartingHero(PC, Mode->GetGuardianClass(), Failure))
                {
                    Finish(false, TEXT("MerchantGuardianSelectionFailed ") + Failure.ToString());
                    return;
                }
                UCombatEconomyComponent* Economy = PC->GetCombatEconomyComponent();
                Economy->AddGold(1000, TEXT("MerchantSealNetworkFixture"));
                MerchantInitialGoldByPlayer.Add(PC, Economy->GetGold());
            }
            bMerchantPrepared = true;
            return;
        }
        if (View.Phase != EDefensePhase::Preparation) { Finish(false, TEXT("MerchantFixtureMissedPreparation")); return; }
        for (ADefensePlayerController* PC : Players)
        {
            UCombatEconomyComponent* Economy = PC->GetCombatEconomyComponent();
            const FDefenseChoiceView Choice = Mode->GetDefenseDirector()->BuildChoiceView(PC);
            if (Choice.RerollsUsed != 2) return;
            if (!FMath::IsNearlyEqual(Economy->GetShopDiscountFraction(), 0.05f)
                || Economy->ApplyShopDiscount(200) != 190 || Choice.RerollCost != 95
                || PC->GetCommandedUnit()->GetCombatInventoryComponent()->GetItemCount() != 2)
            {
                Finish(false, TEXT("MerchantServerFinalStateMismatch")); return;
            }
            const int64 Paid = MerchantInitialGoldByPlayer.FindChecked(PC) - Economy->GetGold();
            if (Paid != 582) { Finish(false, FString::Printf(TEXT("MerchantGoldMismatch Paid=%lld"), Paid)); return; }
        }
        if (MerchantFinishAt <= 0.0) MerchantFinishAt = Now + 2.0;
        else if (Now >= MerchantFinishAt)
            Finish(true, FString::Printf(TEXT("MerchantSealNetwork Server=1 Players=%d Discount=5%% Quote=190 Reroll1=57 Reroll2=95 ExactGold=1 Run=%s"), ExpectedPlayers, *View.RunId.ToString()));
        return;
    }

    ADefensePlayerController* PC = Cast<ADefensePlayerController>(GetWorld()->GetFirstPlayerController());
    ACombatUnitCharacter* Hero = PC ? PC->GetCommandedUnit() : nullptr;
    if (!PC || !PC->IsLocalController() || !Hero) return;
    if (Hero->GetCombatUnitViewComponent()->GetHUDOwnerView().UnitDefinitionId.PrimaryAssetName != TEXT("defense_guardian")) return;
    if (Hero->HasAuthority() || Hero->GetCombatModifierComponent()->GetActiveModifierCount() != 0)
    {
        Finish(false, TEXT("MerchantClientAuthorityLeak")); return;
    }
    UCombatEconomyComponent* Economy = PC->GetCombatEconomyComponent();
    UDefenseChoiceComponent* Choice = PC->GetChoiceComponent();
    if (bMerchantAwaiting)
    {
        const FCombatEconomyResult Receipt = PC->GetLastEconomyResult();
        if (Receipt.RequestId != MerchantRequest) return;
        if (Receipt.bSuccess != bMerchantExpectSuccess)
        {
            Finish(false, FString::Printf(TEXT("MerchantReceiptMismatch Step=%d Failure=%s"), MerchantStep, *Receipt.FailureTag.ToString())); return;
        }
        if (Receipt.bSuccess && (Economy->GetEconomyRevision() != Receipt.EconomyRevision
            || Economy->GetInventoryRevision() != Receipt.InventoryRevision)) return;
        bMerchantAwaiting = false;
        ++MerchantStep;
    }
    auto SubmitPurchase = [&](const FPrimaryAssetId& Id, const bool bExpected)
    {
        const FCombatShopOfferView* Offer = Economy->FindShopOffer(Id);
        if (!Offer) return;
        if (bExpected && !Offer->bCanPurchase) return;
        FCombatEconomyRequest Request;
        Request.RequestId = PC->ReserveCombatRequestId(); Request.ItemDefinitionId = Id;
        Request.CommandBindingGeneration = PC->GetCommandBindingGeneration();
        Request.ExpectedEconomyRevision = Economy->GetEconomyRevision(); Request.ExpectedInventoryRevision = Economy->GetInventoryRevision();
        Request.PurchaseContextId = Offer->ContextId; Request.PurchaseContextRevision = Offer->Revision;
        MerchantRequest = Request.RequestId; bMerchantAwaiting = true; bMerchantExpectSuccess = bExpected;
        PC->ServerSubmitEconomyRequest(Request); PC->ServerSubmitEconomyRequest(Request);
    };
    if (MerchantStep == 0 && Economy->GetGold() > 1000) { MerchantInitialGold = Economy->GetGold(); SubmitPurchase(Seal, true); }
    else if (MerchantStep == 1) SubmitPurchase(Seal, false);
    else if (MerchantStep == 2)
    {
        if (Economy->GetShopPriceView().DiscountBasisPoints != 500) return;
        int64 Quote = 0; FString Error;
        if (!Economy->CalculateCurrentItemPrice(Lens, Quote, Error) || Quote != 190)
        { Finish(false, TEXT("MerchantClientCatalogQuoteMismatch ") + Error); return; }
        SubmitPurchase(Lens, true);
    }
    else if (MerchantStep == 3 && Choice)
    {
        const FDefenseChoiceView& V = Choice->GetView();
        if (!V.bCanChoose || V.Offer.bResolved || V.RerollCost != 57) return;
        FDefenseChoiceRequest Request;
        Request.RequestId = PC->ReserveCombatRequestId(); Request.RunId = V.Offer.RunId; Request.ChoiceRevision = V.Offer.ChoiceRevision;
        Request.Action = EDefenseChoiceAction::Reroll; Request.LifeGeneration = Hero->GetLifeGeneration();
        Request.BindingGeneration = PC->GetCommandBindingGeneration(); Request.EconomyRevision = Economy->GetEconomyRevision();
        MerchantChoiceRequest = Request.RequestId; Choice->ServerSubmitChoice(Request); Choice->ServerSubmitChoice(Request); MerchantStep = 4;
    }
    else if (MerchantStep == 4 && Choice)
    {
        const FDefenseChoiceView& V = Choice->GetView();
        if (V.LastResult.RequestId != MerchantChoiceRequest) return;
        if (!V.LastResult.bAccepted) { Finish(false, TEXT("MerchantFirstRerollRejected ") + V.LastResult.FailureReason.ToString()); return; }
        if (V.RerollsUsed != 1 || Economy->GetGold() != MerchantInitialGold - 240 - 190 - 57) return;
        if (!V.bCanChoose || V.Offer.bResolved || V.RerollCost != 95) return;
        FDefenseChoiceRequest Request;
        Request.RequestId = PC->ReserveCombatRequestId(); Request.RunId = V.Offer.RunId; Request.ChoiceRevision = V.Offer.ChoiceRevision;
        Request.Action = EDefenseChoiceAction::Reroll; Request.LifeGeneration = Hero->GetLifeGeneration();
        Request.BindingGeneration = PC->GetCommandBindingGeneration(); Request.EconomyRevision = Economy->GetEconomyRevision();
        MerchantChoiceRequest = Request.RequestId; Choice->ServerSubmitChoice(Request); Choice->ServerSubmitChoice(Request); MerchantStep = 5;
    }
    else if (MerchantStep == 5 && Choice)
    {
        const FDefenseChoiceView& V = Choice->GetView();
        if (V.LastResult.RequestId != MerchantChoiceRequest) return;
        if (!V.LastResult.bAccepted) { Finish(false, TEXT("MerchantSecondRerollRejected ") + V.LastResult.FailureReason.ToString()); return; }
        if (V.RerollsUsed != 2 || Economy->GetGold() != MerchantInitialGold - 582) return;
        const FCombatHeroInventoryView Inventory = Economy->GetHeroInventoryView();
        int32 Count = 0; for (const FCombatItemView& Item : Inventory.Items) if (Item.Handle.IsValid()) ++Count;
        if (Count != 2 || !FMath::IsNearlyEqual(Economy->GetShopDiscountFraction(), 0.05f)) { Finish(false, TEXT("MerchantOwnerProjectionMismatch")); return; }
        const int64 Paid = MerchantInitialGold - Economy->GetGold();
        if (Paid != 582) { Finish(false, FString::Printf(TEXT("MerchantOwnerGoldMismatch Paid=%lld"), Paid)); return; }
        int32 Seat = 1; FParse::Value(FCommandLine::Get(), TEXT("DefenseSmokeSeat="), Seat);
        Finish(true, FString::Printf(TEXT("MerchantSealNetwork Client=1 Seat=%d Requests=5 Replays=5 Discount=5%% Quote=190 Reroll1=57 Reroll2=95 ExactGold=1 ClientAuthority=0 Run=%s"), Seat, *View.RunId.ToString()));
    }
}
