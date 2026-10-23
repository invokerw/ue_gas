#include "CoreMinimal.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Combat/Economy/CombatEconomyComponent.h"
#include "Combat/Economy/CombatEconomyData.h"
#include "Combat/Economy/CombatShopData.h"
#include "Combat/Items/CombatItemData.h"
#include "Combat/Items/CombatInventoryComponent.h"
#include "Combat/Items/CombatItemSubsystem.h"
#include "Combat/Modifiers/CombatModifierComponent.h"
#include "Combat/Modifiers/CombatModifierRuntime.h"
#include "Combat/Defense/DefenseGameMode.h"
#include "Combat/Defense/DefenseGameState.h"
#include "Combat/Defense/DefenseDirectorComponent.h"
#include "Combat/Defense/DefenseData.h"
#include "Combat/Defense/DefenseObjective.h"
#include "Combat/Defense/DefenseChoiceComponent.h"
#include "UObject/UnrealType.h"
#include "Combat/Attributes/CombatAttributeSet.h"
#include "Combat/Ability/CombatAbilitySystemComponent.h"
#include "Combat/Core/CombatTags.h"
#include "Combat/Combat/CombatDamageSubsystem.h"
#include "Combat/Unit/CombatUnitLifecycleComponent.h"
#include "Combat/Tests/CombatAutomationWorldFixture.h"
#include "Combat/Unit/CombatUnitCharacter.h"
#include "CombatPlayerController.h"
#include "Combat/Network/CombatNetworkSecuritySubsystem.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"

namespace DefenseMerchantSealTests
{
    constexpr auto Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;

    struct FFixture
    {
        FCombatAutomationWorldFixture World;
        ADefenseGameMode* Mode = nullptr;
        ADefenseGameState* State = nullptr;
        UDefenseDirectorComponent* Director = nullptr;
        ADefenseObjective* Core = nullptr;
        ACombatPlayerController* Player = nullptr;
        ACombatUnitCharacter* Hero = nullptr;
        UCombatShopData* Shop = nullptr;
        UDefenseRunData* Run = nullptr;
        FCombatEconomyRequest LastPurchase;

        /** 冻结前构造合法候选池和目录，使用实际生产事务。 */
        bool Start(FAutomationTestBase& Test, UCombatShopData* OverrideShop = nullptr)
        {
            UWorld* W = World.GetWorld();
            Mode = W->SpawnActorDeferred<ADefenseGameMode>(ADefenseGameMode::StaticClass(), FTransform::Identity);
            State = W->SpawnActor<ADefenseGameState>();
            W->CopyGameState(Mode, State);
            Director = Mode->GetDefenseDirector();
            if (!Director->IsRegistered()) Director->RegisterComponent();
            Run = UDefenseRunData::CreatePhase1Default(Mode);
            Run->PreparationDuration = 60.0f;
            Run->IntermissionDuration = 25.0f;
            Core = W->SpawnActor<ADefenseObjective>();
            Core->InitializeFromUnitData(Run->ObjectiveData);
            if (!Core->HasActorBegunPlay()) Core->DispatchBeginPlay();
            UDefenseAugmentPoolData* Pool = NewObject<UDefenseAugmentPoolData>(Run);
            Pool->DefinitionName = TEXT("merchant_seal_fixture_pool");
            for (int32 Index = 0; Index < 8; ++Index)
            {
                UDefenseAugmentData* Card = NewObject<UDefenseAugmentData>(Pool);
                Card->DefinitionName = FName(FString::Printf(TEXT("merchant_fixture_%02d"), Index));
                Card->Family = static_cast<EDefenseAugmentFamily>(Index % 4);
                UCombatModifierData* Modifier = NewObject<UCombatModifierData>(Card);
                Modifier->DefinitionName = FName(FString::Printf(TEXT("merchant_fixture_effect_%02d"), Index));
                Modifier->bRemoveOnDeath = false;
                FCombatModifierAttributeChange Change;
                Change.Attribute = UCombatAttributeSet::GetArmorAttribute();
                Change.Magnitude = 1.0f;
                Modifier->AttributeChanges.Add(Change);
                Card->ModifierData = Modifier;
                Pool->Augments.Add(Card);
            }
            Run->AugmentPool = Pool;
            FString RunError;
            if (!Test.TestTrue(*FString::Printf(TEXT("Valid fixture before freezing: %s"), *RunError), Run->ValidateRuntime(RunError))) return false;
            if (!Test.TestTrue(TEXT("Initialize real defense director"), Director->InitializeForRun(Run, Core, State))) return false;
            Shop = OverrideShop ? OverrideShop : LoadObject<UCombatShopData>(nullptr, TEXT("/Game/Combat/Demo/Defense/DA_DefenseUnifiedShop.DA_DefenseUnifiedShop"));
            if (!Test.TestNotNull(TEXT("Load real defense shop"), Shop)) return false;
            if (!Test.TestTrue(TEXT("Initialize real team shop"), Director->InitializeTeamItems(Shop))) return false;
            Player = AddPlayer(Test);
            return Player && Hero;
        }

        ACombatPlayerController* AddPlayer(FAutomationTestBase& Test)
        {
            UWorld* W = World.GetWorld();
            ACombatPlayerController* PC = W->SpawnActor<ACombatPlayerController>();
            FActorSpawnParameters Params;
            Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
            ACombatUnitCharacter* Unit = W->SpawnActor<ACombatUnitCharacter>(FVector(1000, 500, 100), FRotator::ZeroRotator, Params);
            UCombatUnitData* Source = LoadObject<UCombatUnitData>(nullptr, TEXT("/Game/Combat/Demo/Defense/Guardian/DA_DefenseGuardian.DA_DefenseGuardian"));
            if (!Test.TestNotNull(TEXT("Load real guardian data"), Source)) return nullptr;
            UCombatUnitData* Data = DuplicateObject<UCombatUnitData>(Source, Unit);
            Data->BaseStats.HealthRegen = 0.0f;
            Data->BaseStats.ManaRegen = 0.0f;
            if (!Test.TestTrue(TEXT("Initialize guardian hero"), Unit->InitializeFromUnitData(Data))) return nullptr;
            if (!Unit->HasActorBegunPlay()) Unit->DispatchBeginPlay();
            Unit->GetCharacterMovement()->DisableMovement();
            if (!Test.TestTrue(TEXT("Bind commanded hero"), PC->SetCommandedUnitAuthority(Unit))) return nullptr;
            UCombatEconomyData* Rules = NewObject<UCombatEconomyData>(PC);
            Rules->DefinitionName = TEXT("merchant_seal_rules");
            Rules->StartingGold = 3000;
            Rules->PassiveGoldPerMinute = 0;
            FString Error;
            if (!Test.TestTrue(TEXT("Initialize real economy"), PC->GetCombatEconomyComponent()->InitializeForMatch(Rules, Shop, true, Error))) return nullptr;
            W->GetSubsystem<UCombatNetworkSecuritySubsystem>()->BurstCapacity = 1000;
            if (!Test.TestTrue(TEXT("Register current hero"), Director->RegisterPlayer(PC, Unit))) return nullptr;
            Hero = Unit;
            return PC;
        }

        FCombatEconomyResult Buy(const TCHAR* Name, int32 RequestId)
        {
            UCombatEconomyComponent* Economy = Player->GetCombatEconomyComponent();
            Economy->RefreshShopOffers();
            FCombatEconomyRequest Request;
            Request.RequestId = RequestId;
            Request.CommandBindingGeneration = Player->GetCommandBindingGeneration();
            Request.ExpectedEconomyRevision = Economy->GetEconomyRevision();
            Request.ExpectedInventoryRevision = Economy->GetInventoryRevision();
            Request.ItemDefinitionId = FPrimaryAssetId(TEXT("CombatItem"), Name);
            if (const FCombatShopOfferView* Offer = Economy->FindShopOffer(Request.ItemDefinitionId))
            {
                Request.PurchaseContextId = Offer->ContextId;
                Request.PurchaseContextRevision = Offer->Revision;
            }
            LastPurchase = Request;
            return Player->ProcessEconomyRequestForConnection(Player, Request);
        }

        FDefenseChoiceRequest RerollRequest(int32 RequestId)
        {
            FDefenseAugmentOffer Offer;
            Director->GetOrCreateAugmentOffer(Player, Offer);
            FDefenseChoiceRequest Request;
            Request.RequestId = RequestId;
            Request.RunId = State->GetRunView().RunId;
            Request.ChoiceRevision = Offer.ChoiceRevision;
            Request.Action = EDefenseChoiceAction::Reroll;
            Request.LifeGeneration = Hero->GetLifeGeneration();
            Request.BindingGeneration = Player->GetCommandBindingGeneration();
            Request.EconomyRevision = Player->GetCombatEconomyComponent()->GetEconomyRevision();
            return Request;
        }

        ~FFixture()
        {
            if (Director) Director->Shutdown();
        }
    };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDefenseMerchantSealCatalogTest, "Combat.Defense.MerchantSeal.AuthoredCatalog", DefenseMerchantSealTests::Flags)
bool FDefenseMerchantSealCatalogTest::RunTest(const FString&)
{
    UCombatShopData* Shop = LoadObject<UCombatShopData>(nullptr, TEXT("/Game/Combat/Demo/Defense/DA_DefenseUnifiedShop.DA_DefenseUnifiedShop"));
    if (!TestNotNull(TEXT("Real defense shop"), Shop)) return false;
    UCombatItemData* Item = LoadObject<UCombatItemData>(nullptr, TEXT("/Game/Combat/Demo/Defense/Equipment/DA_item_merchants_seal.DA_item_merchants_seal"));
    UCombatModifierData* Modifier = LoadObject<UCombatModifierData>(nullptr, TEXT("/Game/Combat/Demo/Defense/Equipment/DA_equipment_merchants_seal.DA_equipment_merchants_seal"));
    TestNotNull(TEXT("Merchant seal item"), Item);
    TestNotNull(TEXT("Merchant seal modifier"), Modifier);
    if (!Item || !Modifier) return false;
    TestEqual(TEXT("Stable merchant seal id"), Item->GetPrimaryAssetId(), FPrimaryAssetId(TEXT("CombatItem"), TEXT("item_merchants_seal")));
    TestEqual(TEXT("Merchant seal price"), Item->PurchasePrice, int64(240));
    TestEqual(TEXT("Merchant seal ownership cap"), Item->MaxOwnedQuantity, 1);
    TestFalse(TEXT("Merchant seal is inventory equipment"), Item->bUseOnAcquire);
    TestFalse(TEXT("Merchant seal cannot drop"), Item->bCanDrop);
    TestTrue(TEXT("Shop points at merchant seal"), Shop->FindItem(Item->GetPrimaryAssetId()) == Item);
    FString Error;
    TestTrue(TEXT("Merchant seal definition validates"), Item->ValidateRuntime(Error));
    TestTrue(TEXT("Modifier runtime class"), Modifier->RuntimeClass == UCombatModifierRuntime::StaticClass());
    const float* Discount = Modifier->RuntimeParameters.Find(TEXT("shop_discount_fraction"));
    TestTrue(TEXT("Merchant seal discount parameter"), Discount && FMath::IsNearlyEqual(*Discount, 0.05f));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDefenseMerchantSealPriceTest, "Combat.Defense.MerchantSeal.PurchaseAndPriceProjection", DefenseMerchantSealTests::Flags)
bool FDefenseMerchantSealPriceTest::RunTest(const FString&)
{
    DefenseMerchantSealTests::FFixture Fixture;
    if (!Fixture.Start(*this)) return false;
    UCombatEconomyComponent* Economy = Fixture.Player->GetCombatEconomyComponent();
    TestEqual(TEXT("No discount before purchase"), Economy->GetShopDiscountFraction(), 0.0f);
    TestEqual(TEXT("Base reroll price"), Economy->ApplyShopDiscount(60), int64(60));
    TestEqual(TEXT("Base equipment quote"), Economy->ApplyShopDiscount(200), int64(200));

    const FCombatEconomyResult Seal = Fixture.Buy(TEXT("item_merchants_seal"), 1);
    if (!TestTrue(TEXT("Buy merchant seal"), Seal.bSuccess)) return false;
    TestEqual(TEXT("Seal exact price"), Seal.GoldDelta, int64(-240));
    TestTrue(TEXT("Five percent active"), FMath::IsNearlyEqual(Economy->GetShopDiscountFraction(), 0.05f));
    TestEqual(TEXT("First reroll is floored to 57"), Economy->ApplyShopDiscount(60), int64(57));
    TestEqual(TEXT("Second reroll is floored to 95"), Economy->ApplyShopDiscount(100), int64(95));
    TestEqual(TEXT("Price 59 floors once"), Economy->ApplyShopDiscount(59), int64(56));
    int64 Quote = 0; FString Error;
    TestTrue(TEXT("Projected repair lens quote"), Economy->CalculateCurrentItemPrice(FPrimaryAssetId(TEXT("CombatItem"), TEXT("item_repair_lens")), Quote, Error));
    TestEqual(TEXT("Repair lens quote uses seal"), Quote, int64(190));

    UCombatModifierData* Extra = NewObject<UCombatModifierData>(Fixture.Hero);
    Extra->DefinitionName = TEXT("merchant_extra_discount");
    Extra->RuntimeParameters.Add(TEXT("shop_discount_fraction"), 0.10f);
    FCombatModifierApplyRequest Apply;
    Apply.ModifierData = Extra;
    Apply.Source = Fixture.Hero;
    const FCombatModifierApplyResult ExtraResult = Fixture.Hero->GetCombatModifierComponent()->ApplyModifier(Apply);
    TestTrue(TEXT("Apply additive discount fixture"), ExtraResult.bSuccess);
    TestTrue(TEXT("Fifteen percent additive discount"), FMath::IsNearlyEqual(Economy->GetShopDiscountFraction(), 0.15f));
    TestEqual(TEXT("Stacked 100 quote"), Economy->ApplyShopDiscount(100), int64(85));
    UCombatModifierData* Cap = NewObject<UCombatModifierData>(Fixture.Hero);
    Cap->DefinitionName = TEXT("merchant_cap_discount");
    Cap->RuntimeParameters.Add(TEXT("shop_discount_fraction"), 0.20f);
    Apply.ModifierData = Cap;
    const FCombatModifierApplyResult CapResult = Fixture.Hero->GetCombatModifierComponent()->ApplyModifier(Apply);
    TestTrue(TEXT("Apply cap fixture"), CapResult.bSuccess);
    TestTrue(TEXT("Discount capped at twenty percent"), FMath::IsNearlyEqual(Economy->GetShopDiscountFraction(), 0.20f));
    TestEqual(TEXT("Capped 60 quote"), Economy->ApplyShopDiscount(60), int64(48));

    const FCombatEconomyResult Repair = Fixture.Buy(TEXT("item_repair_lens"), 2);
    TestTrue(TEXT("Buy repair lens at discounted quote"), Repair.bSuccess);
    TestEqual(TEXT("Repair lens charges discounted amount"), Repair.GoldDelta, int64(-160));
    TestEqual(TEXT("Remaining gold after 240 plus 200 at twenty percent"), Economy->GetGold(), int64(2600));
    const FCombatEconomyResult Replay = Fixture.Buy(TEXT("item_merchants_seal"), 3);
    TestFalse(TEXT("Duplicate seal rejected"), Replay.bSuccess);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDefenseMerchantSealRerollTest, "Combat.Defense.MerchantSeal.RerollAndReplay", DefenseMerchantSealTests::Flags)
bool FDefenseMerchantSealRerollTest::RunTest(const FString&)
{
    DefenseMerchantSealTests::FFixture Fixture;
    if (!Fixture.Start(*this)) return false;
    UCombatEconomyComponent* Economy = Fixture.Player->GetCombatEconomyComponent();
    if (!TestTrue(TEXT("Buy seal for reroll test"), Fixture.Buy(TEXT("item_merchants_seal"), 1).bSuccess)) return false;
    const int64 Before = Economy->GetGold();
    FDefenseChoiceRequest First = Fixture.RerollRequest(10);
    const FDefenseChoiceResult Accepted = Fixture.Director->ProcessChoice(Fixture.Player, First);
    TestTrue(TEXT("Discounted first reroll accepted"), Accepted.bAccepted);
    TestEqual(TEXT("First reroll charges 57"), Economy->GetGold(), Before - 57);
    const FDefenseChoiceResult Repeat = Fixture.Director->ProcessChoice(Fixture.Player, First);
    TestTrue(TEXT("Reroll replay returns original result"), Repeat.bAccepted);
    TestEqual(TEXT("Reroll replay does not double debit"), Economy->GetGold(), Before - 57);
    FDefenseChoiceRequest Second = Fixture.RerollRequest(11);
    const FDefenseChoiceResult AcceptedSecond = Fixture.Director->ProcessChoice(Fixture.Player, Second);
    TestTrue(TEXT("Discounted second reroll accepted"), AcceptedSecond.bAccepted);
    TestEqual(TEXT("Second reroll charges 95"), Economy->GetGold(), Before - 57 - 95);
    const FDefenseChoiceView View = Fixture.Director->BuildChoiceView(Fixture.Player);
    TestEqual(TEXT("Reroll view is 95"), View.RerollCost, 95);
    return true;
}

/** 通过反射读取真正被复制的字段，防止只测试权威查询而漏掉拥有者快照。 */
static float MerchantProjectedDiscount(const UCombatEconomyComponent* Economy)
{
    return Economy->GetShopPriceView().DiscountBasisPoints / 10000.0f;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDefenseMerchantSealProjectionTest, "Combat.Defense.MerchantSeal.ProjectionLifecycle", DefenseMerchantSealTests::Flags)
bool FDefenseMerchantSealProjectionTest::RunTest(const FString&)
{
    DefenseMerchantSealTests::FFixture F;
    if (!F.Start(*this)) return false;
    auto* E = F.Player->GetCombatEconomyComponent();
    const auto Bought = F.Buy(TEXT("item_merchants_seal"), 1);
    if (!TestTrue(TEXT("Buy real seal"), Bought.bSuccess)) return false;
    TestTrue(TEXT("Initial owner snapshot five percent"), FMath::IsNearlyEqual(MerchantProjectedDiscount(E), .05f));
    auto* I = F.Hero->GetCombatInventoryComponent();
    const auto* Item = F.World.GetWorld()->GetSubsystem<UCombatItemSubsystem>()->FindItem(Bought.ItemHandle);
    const int32 Slot = Item->GetSlot(); FGameplayTag Failure;
    TestTrue(TEXT("Move to backpack"), I->TrySwap(Slot, 6, I->GetRevision(), Bought.ItemHandle, {}, Failure));
    TestEqual(TEXT("Backpack snapshot immediately zero"), MerchantProjectedDiscount(E), 0.0f);
    TestTrue(TEXT("Reequip"), I->TrySwap(6, Slot, I->GetRevision(), Bought.ItemHandle, {}, Failure));
    TestEqual(TEXT("Waiting snapshot zero"), MerchantProjectedDiscount(E), 0.0f);
    for (int32 N = 0; N < 124; ++N) F.World.GetWorld()->Tick(LEVELTICK_All, .05f);
    TestTrue(TEXT("Scheduler reequipment refreshes owner snapshot"), FMath::IsNearlyEqual(MerchantProjectedDiscount(E), .05f));
    FCombatDamageRequest Damage; Damage.Source = Damage.Target = F.Hero; Damage.Amount = 100000; Damage.DamageType = ECombatDamageType::Pure;
    F.World.GetWorld()->GetSubsystem<UCombatDamageSubsystem>()->DealDamage(Damage);
    TestEqual(TEXT("Real death snapshot zero"), MerchantProjectedDiscount(E), 0.0f);
    TestTrue(TEXT("Public respawn"), F.Hero->GetCombatLifecycleComponent()->RespawnAtLocation(FVector(1000, 500, 100)));
    TestTrue(TEXT("Real respawn restores seal"), FMath::IsNearlyEqual(MerchantProjectedDiscount(E), .05f));
    I->ClearInventory();
    TestEqual(TEXT("Clear snapshot zero"), MerchantProjectedDiscount(E), 0.0f);
    auto* M = F.Hero->GetCombatModifierComponent();
    auto* Data = NewObject<UCombatModifierData>(F.Hero);
    Data->DefinitionName = TEXT("merchant_retained_fixture"); Data->bRemoveOnDeath = false; Data->bDisabledByBreak = true;
    Data->RuntimeParameters.Add(TEXT("shop_discount_fraction"), .10f);
    FCombatModifierApplyRequest R; R.ModifierData = Data; R.Source = F.Hero;
    const auto Applied = M->ApplyModifier(R);
    TestTrue(TEXT("Retained modifier applied"), Applied.bSuccess);
    TestTrue(TEXT("External modifier refreshes owner snapshot"), FMath::IsNearlyEqual(MerchantProjectedDiscount(E), .10f));
    F.Hero->GetCombatAbilitySystemComponent()->AddLooseGameplayTag(CombatTags::State_Broken);
    E->RefreshShopOffers();
    TestEqual(TEXT("Existing monitor refreshes quote when Break changes without collection mutation"), MerchantProjectedDiscount(E), 0.0f);
    F.Hero->GetCombatAbilitySystemComponent()->RemoveLooseGameplayTag(CombatTags::State_Broken);
    E->RefreshShopOffers();
    TestTrue(TEXT("Existing monitor restores quote after Break"), FMath::IsNearlyEqual(MerchantProjectedDiscount(E), .10f));
    M->HandleOwnerDeath();
    TestEqual(TEXT("Paused retained modifier projects zero"), MerchantProjectedDiscount(E), 0.0f);
    M->HandleOwnerRespawn();
    TestTrue(TEXT("Resume retained modifier refreshes snapshot"), FMath::IsNearlyEqual(MerchantProjectedDiscount(E), .10f));
    M->RemoveModifier(Applied.Handle);
    TestEqual(TEXT("Remove snapshot zero"), MerchantProjectedDiscount(E), 0.0f);
    M->ApplyModifier(R);
    ACombatPlayerController* Other = F.AddPlayer(*this);
    if (!Other) return false;
    TestTrue(TEXT("Switch to clean hero"), F.Player->SetCommandedUnitAuthority(Other->GetCommandedUnit()));
    TestEqual(TEXT("Switch unbinds old modifier source"), MerchantProjectedDiscount(E), 0.0f);
    M->HandleOwnerRespawn();
    TestEqual(TEXT("Old source cannot republish discount"), MerchantProjectedDiscount(E), 0.0f);
    F.Player->GetCommandedUnit()->Destroy();
    TestEqual(TEXT("Hero EndPlay clears quote"), MerchantProjectedDiscount(E), 0.0f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDefenseMerchantSealClientQuoteTest, "Combat.Defense.MerchantSeal.ClientCatalogQuote", DefenseMerchantSealTests::Flags)
bool FDefenseMerchantSealClientQuoteTest::RunTest(const FString&)
{
    FCombatAutomationWorldFixture F(NM_Client);
    auto* PC = F.GetWorld()->SpawnActor<ACombatPlayerController>();
    auto* E = PC->GetCombatEconomyComponent();
    // 仅播种服务器投影，不初始化权威目录或客户端运行时效果。
    auto* Snapshot = FindFProperty<FStructProperty>(E->GetClass(), TEXT("ReplicatedView"));
    auto* View = Snapshot->ContainerPtrToValuePtr<FCombatEconomyView>(E);
    View->ShopDefinitionId = FPrimaryAssetId(TEXT("CombatShop"), TEXT("defense_unified_shop")); View->GoldCap = 10000;
    TestNull(TEXT("Client has no frozen authoritative catalog"), E->GetShopData());
    int64 Price = -1; FString Error;
    TestTrue(TEXT("Client resolves real catalog by replicated id"), E->CalculateCurrentItemPrice(FPrimaryAssetId(TEXT("CombatItem"), TEXT("item_repair_lens")), Price, Error));
    TestEqual(TEXT("Client base quote is real catalog price"), Price, int64(200));
    auto* PriceProperty = FindFProperty<FStructProperty>(E->GetClass(), TEXT("ShopPriceView"));
    auto* PriceView = PriceProperty->ContainerPtrToValuePtr<FCombatShopPriceView>(E);
    PriceView->DiscountBasisPoints = 500;
    TestTrue(TEXT("Client discounted quote resolves"), E->CalculateCurrentItemPrice(FPrimaryAssetId(TEXT("CombatItem"), TEXT("item_repair_lens")), Price, Error));
    TestEqual(TEXT("Client uses only replicated five percent"), Price, int64(190));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDefenseMerchantSealChoiceProjectionTest, "Combat.Defense.MerchantSeal.ChoicePriceProjection", DefenseMerchantSealTests::Flags)
bool FDefenseMerchantSealChoiceProjectionTest::RunTest(const FString&)
{
    FCombatAutomationWorldFixture F;
    auto* PC = F.GetWorld()->SpawnActor<ACombatPlayerController>();
    auto* C = NewObject<UDefenseChoiceComponent>(PC); C->RegisterComponent();
    FDefenseChoiceView View; View.bCanChoose = View.bCanReroll = true; View.RerollCost = 60;
    C->PublishAuthority(View); const int32 Before = C->GetView().Revision;
    View.RerollCost = 57; C->PublishAuthority(View);
    TestEqual(TEXT("Cost-only update reaches owner"), C->GetView().RerollCost, 57);
    TestTrue(TEXT("Cost-only update advances projection revision"), C->GetView().Revision > Before);
    View.RerollsUsed = 1; C->PublishAuthority(View);
    TestEqual(TEXT("Count-only update reaches owner"), C->GetView().RerollsUsed, 1);
    return true;
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDefenseMerchantSealTeamRefundTest, "Combat.Defense.MerchantSeal.TeamPurchaseAndRefund", DefenseMerchantSealTests::Flags)
bool FDefenseMerchantSealTeamRefundTest::RunTest(const FString&)
{
    for (int32 OutsideWindow = 0; OutsideWindow < 2; ++OutsideWindow)
    {
        DefenseMerchantSealTests::FFixture F; if (!F.Start(*this)) return false;
        auto* E = F.Player->GetCombatEconomyComponent();
        if (!TestTrue(TEXT("Buy seal"), F.Buy(TEXT("item_merchants_seal"), 1).bSuccess)) return false;
        const auto Lens = F.Buy(TEXT("item_repair_lens"), 2);
        if (!TestTrue(TEXT("Buy discounted lens"), Lens.bSuccess)) return false;
        TestEqual(TEXT("Real lens debit"), Lens.GoldDelta, int64(-190));
        const auto Replay = F.Player->ProcessEconomyRequestForConnection(F.Player, F.LastPurchase);
        TestTrue(TEXT("Exact purchase replay accepted"), Replay.bSuccess);
        TestEqual(TEXT("Replay preserves original paid gold"), Replay.GoldDelta, int64(-190));
        TestEqual(TEXT("Purchase replay does not double debit"), E->GetGold(), int64(2570));
        if (OutsideWindow) for (int32 N = 0; N < 204; ++N) F.World.GetWorld()->Tick(LEVELTICK_All, .05f);
        FGameplayTag Failure; const int64 Before = E->GetGold();
        TestTrue(TEXT("Sell through authoritative inventory transaction"), E->SellInventoryItem(Lens.ItemHandle,
            E->FindInventoryItemRevision(Lens.ItemHandle), E->GetEconomyRevision(), E->GetInventoryRevision(), Failure));
        TestEqual(TEXT("Refund uses paid gold inside window and original half price outside"), E->GetGold() - Before, int64(OutsideWindow ? 100 : 190));
        FCombatDamageRequest Damage; Damage.Source = Damage.Target = F.Core; Damage.Amount = 2000; Damage.DamageType = ECombatDamageType::Pure;
        F.World.GetWorld()->GetSubsystem<UCombatDamageSubsystem>()->DealDamage(Damage);
        const auto Team = F.Buy(TEXT("team_emergency_stone"), 3);
        TestTrue(TEXT("Discounted acquire-only team purchase"), Team.bSuccess && Team.bConsumedOnAcquire);
        TestEqual(TEXT("Team purchase costs floor 180 times .95"), Team.GoldDelta, int64(-171));
        const int64 After = E->GetGold();
        TestTrue(TEXT("Team replay returns original success"), F.Player->ProcessEconomyRequestForConnection(F.Player, F.LastPurchase).bSuccess);
        TestEqual(TEXT("Team replay does not double pay"), E->GetGold(), After);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDefenseMerchantSealRecipeTest, "Combat.Defense.MerchantSeal.RecipeAndIntegerBounds", DefenseMerchantSealTests::Flags)
bool FDefenseMerchantSealRecipeTest::RunTest(const FString&)
{
    DefenseMerchantSealTests::FFixture F;
    auto* Shop = NewObject<UCombatShopData>(); Shop->DefinitionName = TEXT("merchant_recipe_fixture");
    auto* A = NewObject<UCombatItemData>(Shop); A->DefinitionName = TEXT("merchant_leaf_a"); A->bPurchasable = true; A->PurchasePrice = 10;
    auto* B = NewObject<UCombatItemData>(Shop); B->DefinitionName = TEXT("merchant_leaf_b"); B->bPurchasable = true; B->PurchasePrice = 10;
    auto* Target = NewObject<UCombatItemData>(Shop); Target->DefinitionName = TEXT("merchant_recipe");
    FCombatItemRecipeIngredient One; One.Item = A; Target->Recipe.Add(One); One.Item = B; Target->Recipe.Add(One);
    FCombatShopCategory Category; Category.CategoryId = TEXT("fixture"); Category.Items = { A, B, Target }; Shop->Categories.Add(Category);
    if (!F.Start(*this, Shop)) return false;
    auto* E = F.Player->GetCombatEconomyComponent(); auto* Data = NewObject<UCombatModifierData>(F.Hero);
    Data->DefinitionName = TEXT("merchant_recipe_discount"); Data->RuntimeParameters.Add(TEXT("shop_discount_fraction"), .05f);
    FCombatModifierApplyRequest Apply; Apply.ModifierData = Data; Apply.Source = F.Hero;
    if (!TestTrue(TEXT("Apply discount via public modifier"), F.Hero->GetCombatModifierComponent()->ApplyModifier(Apply).bSuccess)) return false;
    int64 Quote = 0; FString Error;
    TestTrue(TEXT("Recipe quote"), E->CalculateCurrentItemPrice(Target->GetPrimaryAssetId(), Quote, Error));
    TestEqual(TEXT("Floor whole 20 once, not each 10 leaf"), Quote, int64(19));
    const auto Purchase = F.Buy(TEXT("merchant_recipe"), 1);
    TestTrue(TEXT("Real recipe transaction succeeds"), Purchase.bSuccess);
    TestEqual(TEXT("Actual recipe debit matches quote"), Purchase.GoldDelta, int64(-19));
    TestEqual(TEXT("Zero quote"), E->ApplyShopDiscount(0), int64(0));
    TestEqual(TEXT("Negative input clamps"), E->ApplyShopDiscount(-1), int64(0));
    TestEqual(TEXT("Single gold floors"), E->ApplyShopDiscount(1), int64(0));
    TestEqual(TEXT("Int64 maximum does not overflow"), E->ApplyShopDiscount(MAX_int64), int64(8762203435012037016LL));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDefenseMerchantSealRerollFailureTest, "Combat.Defense.MerchantSeal.RerollFailureAtomicity", DefenseMerchantSealTests::Flags)
bool FDefenseMerchantSealRerollFailureTest::RunTest(const FString&)
{
    DefenseMerchantSealTests::FFixture F; if (!F.Start(*this)) return false;
    auto* E = F.Player->GetCombatEconomyComponent();
    if (!TestTrue(TEXT("Buy seal"), F.Buy(TEXT("item_merchants_seal"), 1).bSuccess)) return false;
    FString Error; E->SetGoldForDebug(56, Error);
    const auto Before = F.Director->BuildChoiceView(F.Player);
    const auto R = F.RerollRequest(10); const auto Result = F.Director->ProcessChoice(F.Player, R);
    TestFalse(TEXT("56 cannot pay 57"), Result.bAccepted);
    TestEqual(TEXT("Insufficient balance unchanged"), E->GetGold(), int64(56));
    const auto After = F.Director->BuildChoiceView(F.Player);
    TestEqual(TEXT("No reroll consumed"), After.RerollsUsed, Before.RerollsUsed);
    TestEqual(TEXT("No offer revision changed"), After.Offer.ChoiceRevision, Before.Offer.ChoiceRevision);
    E->SetGoldForDebug(1000, Error);
    auto Stale = F.RerollRequest(11); --Stale.EconomyRevision;
    TestFalse(TEXT("Stale resource revision rejected"), F.Director->ProcessChoice(F.Player, Stale).bAccepted);
    TestEqual(TEXT("Stale request has no debit"), E->GetGold(), int64(1000));
    auto Valid = F.RerollRequest(12);
    TestTrue(TEXT("Current request succeeds"), F.Director->ProcessChoice(F.Player, Valid).bAccepted);
    Valid.Action = EDefenseChoiceAction::Select;
    TestFalse(TEXT("Same request id with changed payload rejected"), F.Director->ProcessChoice(F.Player, Valid).bAccepted);
    TestEqual(TEXT("Changed payload cannot double spend"), E->GetGold(), int64(943));
    return true;
}

#endif
