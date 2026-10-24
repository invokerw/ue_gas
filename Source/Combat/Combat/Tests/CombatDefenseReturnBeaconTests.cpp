#include "CoreMinimal.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Combat/Ability/CombatAbilitySystemComponent.h"
#include "Combat/Ability/CombatGameplayAbility.h"
#include "Combat/Attributes/CombatAttributeSet.h"
#include "Combat/Combat/CombatDamageSubsystem.h"
#include "Combat/Core/CombatTags.h"
#include "Combat/Defense/DefenseData.h"
#include "Combat/Defense/DefenseDirectorComponent.h"
#include "Combat/Defense/DefenseGameMode.h"
#include "Combat/Defense/DefenseGameState.h"
#include "Combat/Defense/DefenseObjective.h"
#include "Combat/Economy/CombatEconomyComponent.h"
#include "Combat/Economy/CombatEconomyData.h"
#include "Combat/Economy/CombatShopData.h"
#include "Combat/Items/CombatInventoryComponent.h"
#include "Combat/Items/CombatItemData.h"
#include "Combat/Items/CombatItemSubsystem.h"
#include "Combat/Modifiers/CombatModifierComponent.h"
#include "Combat/Tests/CombatAutomationWorldFixture.h"
#include "Combat/Unit/CombatUnitCharacter.h"
#include "Combat/Unit/CombatUnitLifecycleComponent.h"
#include "Combat/View/CombatUnitViewComponent.h"
#include "CombatPlayerController.h"
#include "Engine/World.h"
#include "Combat/Log/CombatEventSubsystem.h"
#include "Combat/Motion/CombatMotionComponent.h"
#include "Combat/Order/CombatOrderComponent.h"
#include "Combat/Defense/DefenseCoreShieldRuntime.h"
#include "Components/BoxComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

/** 只读死亡账本和重复通知探针；不绕过实际死亡、库存或经济事务。 */
struct FReturnBeaconTestAccess
{
    static double RespawnAt(UDefenseDirectorComponent* D,ACombatPlayerController* P) { return D->Players.FindChecked(P).RespawnAt; }
    static void DiedAgain(UDefenseDirectorComponent* D,ACombatUnitCharacter* H) { D->OnHeroDied(H,{}); }
    static void Finish(UDefenseDirectorComponent* D) { D->Finish(false,TEXT("ReturnBeaconTest")); }
};

namespace ReturnBeaconTests
{
    constexpr auto Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;
    /** 只读正式资产；缺失即失败，不用瞬态对象冒充正式内容。 */
    template<class T> T* Asset(const TCHAR* Folder, const TCHAR* Name)
    { return LoadObject<T>(nullptr, *FString::Printf(TEXT("/Game/Combat/Demo/Defense/%s%s.%s"), Folder, Name, Name)); }
    /** 最小防守世界仍使用真实核心、英雄、商店、GAS和库存。 */
    struct FFixture
    {
        FCombatAutomationWorldFixture World;
        UDefenseDirectorComponent* Director = nullptr;
        ACombatPlayerController* PC = nullptr;
        ACombatUnitCharacter* Hero = nullptr;
        ADefenseObjective* Core = nullptr;
        FGameplayAbilitySpecHandle Recall;
        bool Start(FAutomationTestBase& T)
        {
            auto* W = World.GetWorld();
            auto* Mode = W->SpawnActorDeferred<ADefenseGameMode>(ADefenseGameMode::StaticClass(), FTransform::Identity);
            auto* State = W->SpawnActor<ADefenseGameState>(); W->CopyGameState(Mode, State);
            Director = Mode->GetDefenseDirector(); if (!Director->IsRegistered()) Director->RegisterComponent();
            auto* Run = UDefenseRunData::CreatePhase1Default(Mode); Run->PreparationDuration = 120;
            Core = W->SpawnActor<ADefenseObjective>(); Core->InitializeFromUnitData(Run->ObjectiveData);
            if (!Core->HasActorBegunPlay()) Core->DispatchBeginPlay();
            if (!T.TestTrue(TEXT("Initialize actual run"), Director->InitializeForRun(Run, Core, State))) return false;
            auto* Shop = Asset<UCombatShopData>(TEXT(""), TEXT("DA_DefenseUnifiedShop"));
            if (!T.TestNotNull(TEXT("Real shop"), Shop) || !Director->InitializeTeamItems(Shop)) return false;
            PC = W->SpawnActor<ACombatPlayerController>();
            FActorSpawnParameters Spawn; Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
            Hero = W->SpawnActor<ACombatUnitCharacter>(FVector(1800, 500, 100), FRotator::ZeroRotator, Spawn);
            auto* Source = Asset<UCombatUnitData>(TEXT("Guardian/"), TEXT("DA_DefenseGuardian"));
            if (!T.TestNotNull(TEXT("Actual guardian"), Source)) return false;
            auto* Data = DuplicateObject<UCombatUnitData>(Source, Hero); Data->BaseStats.HealthRegen = Data->BaseStats.ManaRegen = 0;
            if (!Hero->InitializeFromUnitData(Data)) return false;
            if (!Hero->HasActorBegunPlay()) Hero->DispatchBeginPlay(); Hero->GetCharacterMovement()->DisableMovement();
            if (!PC->SetCommandedUnitAuthority(Hero)) return false;
            auto* Rules = NewObject<UCombatEconomyData>(PC); Rules->DefinitionName = TEXT("return_beacon_fixture"); Rules->StartingGold = 2000; Rules->PassiveGoldPerMinute = 0;
            FString Error; if (!PC->GetCombatEconomyComponent()->InitializeForMatch(Rules, Shop, true, Error) || !Director->RegisterPlayer(PC, Hero)) return false;
            const auto* Spec = Hero->GetCombatAbilitySystemComponent()->FindCombatAbilitySpecByDefinitionId(FPrimaryAssetId(TEXT("CombatAbility"), TEXT("defense_recall")));
            if (!T.TestNotNull(TEXT("Actual recall granted by production hero"), Spec)) return false;
            Recall = Spec->Handle; return true;
        }
        /** 原主动激活入口；不改CDO或冷却。 */
        bool Cast()
        { FGameplayTag Failure; return Hero->GetCombatAbilitySystemComponent()->TryActivateCombatAbility(Recall, {}, Failure); }
        /** 原库存公开授予，装备效果由真实实例持有。 */
        FCombatItemHandle Equip(FAutomationTestBase& T)
        {
            auto* Item = Asset<UCombatItemData>(TEXT("Equipment/"), TEXT("DA_item_return_beacon"));
            FCombatItemHandle Handle; FGameplayTag Failure;
            T.TestTrue(TEXT("Equip authored beacon"), Item && Hero->GetCombatInventoryComponent()->GiveItem(Item, 1, Handle, Failure)); return Handle;
        }
        /** 使用公开连接入口购买/出售，保留原拥有权、修订和窗口检查。 */
        FCombatEconomyResult Buy(const TCHAR* Name, int32 Id, FCombatItemHandle Sell = {})
        {
            auto* E=PC->GetCombatEconomyComponent(); E->RefreshShopOffers();
            FCombatEconomyRequest R; R.RequestId=Id; R.CommandBindingGeneration=PC->GetCommandBindingGeneration();
            R.ExpectedEconomyRevision=E->GetEconomyRevision(); R.ExpectedInventoryRevision=E->GetInventoryRevision();
            R.ItemDefinitionId=FPrimaryAssetId(TEXT("CombatItem"),Name);
            if (Sell.IsValid()) { R.Action=ECombatEconomyAction::SellInventoryItem; R.ItemHandle=Sell; R.ItemRevision=World.GetWorld()->GetSubsystem<UCombatItemSubsystem>()->FindItem(Sell)->GetRevision(); }
            if (const auto* O=!Sell.IsValid()?E->FindShopOffer(R.ItemDefinitionId):nullptr) { R.PurchaseContextId=O->ContextId; R.PurchaseContextRevision=O->Revision; }
            return PC->ProcessEconomyRequestForConnection(PC,R);
        }
        /** 原库存经济事务出售，带上服务器当前修订并走同一权限入口。 */
        FCombatEconomyResult SellBeacon()
        {
            const auto View=PC->GetCombatEconomyComponent()->GetHeroInventoryView();
            const auto* Beacon=View.Items.FindByPredicate([](const FCombatItemView& V){return V.DefinitionId.PrimaryAssetName==TEXT("item_return_beacon");});
            FCombatEconomyRequest R; if(!Beacon) return {};
            R.Action=ECombatEconomyAction::SellInventoryItem; R.ItemHandle=Beacon->Handle; R.ItemRevision=Beacon->Revision;
            R.RequestId=1; R.CommandBindingGeneration=PC->GetCommandBindingGeneration();
            R.ExpectedEconomyRevision=PC->GetCombatEconomyComponent()->GetEconomyRevision(); R.ExpectedInventoryRevision=PC->GetCombatEconomyComponent()->GetInventoryRevision();
            return PC->ProcessEconomyRequestForConnection(PC,R);
        }
        /** 原Order施法，使Stop/替换命令走生产取消路径。 */
        bool CastOrder()
        { FCombatOrderRequest R; R.Type=ECombatOrderType::CastNoTarget; R.AbilitySpecHandle=Recall; return Hero->GetCombatOrderComponent()->IssueOrder(R,false).bSuccess; }
        /** 可控游戏时间推进真实Scheduler。 */
        void Advance(float Seconds)
        { for (int32 N=0; N<FMath::CeilToInt(Seconds/.025f); ++N) World.GetWorld()->Tick(LEVELTICK_All,.025f); }
        ~FFixture() { if (Director) Director->Shutdown(); }
    };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReturnBeaconAssetsTest, "Combat.Defense.ReturnBeacon.AuthoredPackage", ReturnBeaconTests::Flags)
bool FReturnBeaconAssetsTest::RunTest(const FString&)
{
    auto* Item = ReturnBeaconTests::Asset<UCombatItemData>(TEXT("Equipment/"), TEXT("DA_item_return_beacon"));
    auto* Data = ReturnBeaconTests::Asset<UCombatAbilityData>(TEXT("Recall/"), TEXT("DA_defense_recall"));
    if (!TestNotNull(TEXT("Authored eighth equipment"), Item) || !TestNotNull(TEXT("Authored recall data"), Data)) return false;
    TestEqual(TEXT("Price150"), Item->PurchasePrice, int64(150)); TestEqual(TEXT("Holding cap1"), Item->MaxOwnedQuantity, 1);
    TestFalse(TEXT("Cannot drop"), Item->bCanDrop); TestEqual(TEXT("BoundUnit"), Item->Sharing, ECombatItemSharing::BoundUnit);
    TestEqual(TEXT("Base channel3"), Data->ChannelDuration, 3.f); TestEqual(TEXT("Cooldown30"), Data->GetSpecialValue(TEXT("cooldown"),1),30.f);
    TestEqual(TEXT("Zero mana"), Data->GetSpecialValue(TEXT("mana_cost"),1),0.f);
    auto* Shop = ReturnBeaconTests::Asset<UCombatShopData>(TEXT(""), TEXT("DA_DefenseUnifiedShop"));
    TestTrue(TEXT("Shop references authored beacon"), Shop && Shop->FindItem(Item->GetPrimaryAssetId()) == Item);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReturnBeaconChannelTest, "Combat.Defense.ReturnBeacon.ChannelSnapshotAndCooldown", ReturnBeaconTests::Flags)
bool FReturnBeaconChannelTest::RunTest(const FString&)
{
    for (const bool Equipped : {false, true})
    {
        ReturnBeaconTests::FFixture F; if (!F.Start(*this)) return false;
        const auto Item = Equipped ? F.Equip(*this) : FCombatItemHandle();
        const float Mana = F.Hero->GetCombatAttributeSet()->GetMana();
        if (!TestTrue(TEXT("Activate actual recall"), F.Cast())) return false;
        const auto View = F.Hero->GetCombatUnitViewComponent()->GetUnitView();
        TestEqual(TEXT("Actual channel display phase"), View.AbilityPhase, ECombatAbilityViewPhase::Channeling);
        TestEqual(TEXT("Frozen channel duration3/2"), float(View.AbilityServerEndTime-View.AbilityServerStartTime), Equipped ? 2.f : 3.f);
        TestEqual(TEXT("Original ASC cooldown starts30"), F.Hero->GetCombatAbilitySystemComponent()->GetCombatAbilityCooldownRemaining(F.Recall),30.f);
        TestEqual(TEXT("No mana spent"),F.Hero->GetCombatAttributeSet()->GetMana(),Mana);
        if (Equipped)
        {
            auto* I = F.Hero->GetCombatInventoryComponent(); FGameplayTag Failure;
            const auto* Instance = F.World.GetWorld()->GetSubsystem<UCombatItemSubsystem>()->FindItem(Item);
            TestTrue(TEXT("Backpack during channel"),I->TrySwap(Instance->GetSlot(),6,I->GetRevision(),Item,{},Failure));
            TestEqual(TEXT("In-flight deadline does not extend"),F.Hero->GetCombatUnitViewComponent()->GetUnitView().AbilityServerEndTime,View.AbilityServerEndTime);
        }
        F.Hero->GetCombatAbilitySystemComponent()->CancelAbilityHandle(F.Recall);
        F.Advance(4);
        TestFalse(TEXT("No late callback after cancel"),F.Hero->GetCombatAbilitySystemComponent()->FindAbilitySpecFromHandle(F.Recall)->IsActive());
        TestFalse(TEXT("Cooldown rejects another request"),F.Cast());
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReturnBeaconInterruptTest, "Combat.Defense.ReturnBeacon.RealDamageStatusAndDeathInterrupt", ReturnBeaconTests::Flags)
bool FReturnBeaconInterruptTest::RunTest(const FString&)
{
    for (int32 Mode=0; Mode<4; ++Mode)
    {
        ReturnBeaconTests::FFixture F; if (!F.Start(*this) || !F.Cast()) return false;
        const auto Origin = F.Hero->GetActorLocation();
        if (Mode<2)
        {
            FCombatDamageRequest Damage; Damage.Source=Damage.Target=F.Hero; Damage.Amount=10; Damage.DamageType=ECombatDamageType::Pure;
            if (Mode==1) Damage.Flags.AddTag(CombatTags::Damage_Flag_HPLoss);
            F.World.GetWorld()->GetSubsystem<UCombatDamageSubsystem>()->DealDamage(Damage);
        }
        else if (Mode==2) F.Hero->GetCombatAbilitySystemComponent()->AddLooseGameplayTag(CombatTags::State_Stunned);
        else F.Hero->GetCombatLifecycleComponent()->RequestDeath({},F.Hero);
        TestFalse(TEXT("Current activation ended"), F.Hero->GetCombatAbilitySystemComponent()->FindAbilitySpecFromHandle(F.Recall)->IsActive());
        F.Advance(4); TestTrue(TEXT("Interrupted recall never changes position"),F.Hero->GetActorLocation().Equals(Origin,1));
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReturnBeaconDeathTest, "Combat.Defense.ReturnBeacon.DeathSnapshotAndReinforcement", ReturnBeaconTests::Flags)
bool FReturnBeaconDeathTest::RunTest(const FString&)
{
    for (int32 Mode=0; Mode<8; ++Mode)
    {
        ReturnBeaconTests::FFixture F; if (!F.Start(*this)) return false;
        const bool BeaconEquipped=Mode!=0 && Mode!=2; const bool Aid=Mode==2 || Mode==3;
        const auto Item=BeaconEquipped?F.Equip(*this):FCombatItemHandle();
        F.Advance(.1f);
        if (Aid && !TestTrue(TEXT("Purchase actual aid"),F.Buy(TEXT("team_reinforcement_mark"),1).bSuccess)) return false;
        auto* I=F.Hero->GetCombatInventoryComponent(); auto* M=F.Hero->GetCombatModifierComponent();
        if (Mode==4 || Mode==5)
        {
            const auto* Instance=F.World.GetWorld()->GetSubsystem<UCombatItemSubsystem>()->FindItem(Item); FGameplayTag Failure;
            TestTrue(TEXT("Move beacon to backpack"),I->TrySwap(Instance->GetSlot(),6,I->GetRevision(),Item,{},Failure));
            if (Mode==5) TestTrue(TEXT("Return before six-second equip wait"),I->TrySwap(6,0,I->GetRevision(),Item,{},Failure));
            F.Advance(.1f);
        }
        if (Mode==6)
        {
            TestTrue(TEXT("Actual sell transaction"),F.SellBeacon().bSuccess);
        }
        if (Mode==7) F.Hero->GetCombatAbilitySystemComponent()->AddLooseGameplayTag(CombatTags::State_Broken);
        const float Expected=BeaconEquipped && Mode!=4 && Mode!=5 && Mode!=6 ? 2.f : 0.f;
        TestEqual(TEXT("Effective reduction before death"),M->GetActiveRuntimeParameterSum(TEXT("defense_respawn_reduction_seconds")),Expected);
        auto* L=F.Hero->GetCombatLifecycleComponent(); int32 Deaths=0,Revivals=0;
        L->OnDied().AddLambda([&](ACombatUnitCharacter*,const FCombatEventContext&){++Deaths;});
        L->OnRespawned().AddLambda([&](ACombatUnitCharacter*,const FCombatEventContext&){++Revivals;});
        TestEqual(TEXT("Alive cannot read a previous death"),L->GetDeathRuntimeParameter(TEXT("defense_respawn_reduction_seconds")),0.f);
        TestTrue(TEXT("Explicit real death"),L->RequestDeath({},F.Hero));
        TestEqual(TEXT("Equipment removed before death callbacks"),M->GetActiveRuntimeParameterSum(TEXT("defense_respawn_reduction_seconds")),0.f);
        TestEqual(TEXT("Snapshot survives cleanup"),L->GetDeathRuntimeParameter(TEXT("defense_respawn_reduction_seconds")),Expected);
        const double Deadline=FReturnBeaconTestAccess::RespawnAt(F.Director,F.PC);
        TestEqual(TEXT("Normal8 beacon6 aid5 both3"),float(Deadline-F.World.GetWorld()->GetTimeSeconds()),8.f-Expected-(Aid?3.f:0.f));
        TestFalse(TEXT("Duplicate death rejected"),L->RequestDeath({},F.Hero));
        FReturnBeaconTestAccess::DiedAgain(F.Director,F.Hero);
        TestEqual(TEXT("Duplicate cannot reschedule or reserve twice"),FReturnBeaconTestAccess::RespawnAt(F.Director,F.PC),Deadline);
        const auto Life=F.Hero->GetLifeGeneration();
        TestTrue(TEXT("Public respawn for snapshot generation test"),L->RespawnAtLocation(F.Hero->GetActorLocation()));
        F.Advance(.1f);
        TestEqual(TEXT("Life increments once"),F.Hero->GetLifeGeneration(),Life+1);
        TestEqual(TEXT("New life has no old death snapshot"),L->GetDeathRuntimeParameter(TEXT("defense_respawn_reduction_seconds")),0.f);
        TestEqual(TEXT("One death notification"),Deaths,1); TestEqual(TEXT("One revival notification"),Revivals,1);
        TestEqual(TEXT("Effective equipment rebuilt once"),M->GetActiveRuntimeParameterSum(TEXT("defense_respawn_reduction_seconds")),Expected);
        L->OnDied().Clear(); L->OnRespawned().Clear();
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReturnBeaconAbsorptionTest, "Combat.Defense.ReturnBeacon.AbsorbedAndZeroDamageContinue", ReturnBeaconTests::Flags)
bool FReturnBeaconAbsorptionTest::RunTest(const FString&)
{
    ReturnBeaconTests::FFixture F; if (!F.Start(*this)) return false;
    auto* A=F.Hero->GetCombatAbilitySystemComponent();
    const auto* W=A->FindCombatAbilitySpecByDefinitionId(FPrimaryAssetId(TEXT("CombatAbility"),TEXT("guardian_fortify")));
    FGameplayTag Failure;
    if (!TestTrue(TEXT("Real guardian W"),W && A->TryActivateCombatAbility(W->Handle,{},Failure))) return false;
    F.Advance(.2f); if (!F.Cast()) return false;
    FCombatDamageRequest R; R.Source=R.Target=F.Hero; R.Amount=10; R.DamageType=ECombatDamageType::Pure;
    auto* D=F.World.GetWorld()->GetSubsystem<UCombatDamageSubsystem>(); const auto Result=D->DealDamage(R);
    TestEqual(TEXT("Actual shield absorbs entire hit"),Result.Event.AbsorbedAmount,10.f);
    TestEqual(TEXT("Applied health loss zero"),Result.Event.AppliedAmount,0.f);
    TestTrue(TEXT("Absorbed hit does not interrupt"),A->FindAbilitySpecFromHandle(F.Recall)->IsActive());
    R.Amount=0; D->DealDamage(R);
    TestTrue(TEXT("Zero damage does not interrupt"),A->FindAbilitySpecFromHandle(F.Recall)->IsActive());
    A->CancelAbilityHandle(F.Recall); F.Advance(4); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReturnBeaconCancelTest, "Combat.Defense.ReturnBeacon.OrderBindingTerminalAndEndPlay", ReturnBeaconTests::Flags)
bool FReturnBeaconCancelTest::RunTest(const FString&)
{
    for (int32 Mode=0; Mode<5; ++Mode)
    {
        ReturnBeaconTests::FFixture F; if (!F.Start(*this) || !TestTrue(TEXT("Actual cast order"),F.CastOrder())) return false;
        auto* A=F.Hero->GetCombatAbilitySystemComponent(); auto* Ability=Cast<UCombatGameplayAbility>(A->FindAbilitySpecFromHandle(F.Recall)->GetPrimaryInstance());
        const FVector Origin=F.Hero->GetActorLocation();
        if (Mode==0) { FCombatOrderRequest R; R.Type=ECombatOrderType::Stop; TestTrue(TEXT("Actual Stop"),F.Hero->GetCombatOrderComponent()->IssueOrder(R,false).bSuccess); }
        if (Mode==1) TestTrue(TEXT("Motion causes real displacement"),F.Hero->GetCombatMotionComponent()->TryTeleport(Origin+FVector(500,0,0),{},{}).bSuccess);
        if (Mode==2) TestTrue(TEXT("Unbind current hero"),F.PC->SetCommandedUnitAuthority(nullptr));
        if (Mode==3) FReturnBeaconTestAccess::Finish(F.Director);
        if (Mode==4) F.Hero->Destroy();
        F.Advance(.1f); TestFalse(TEXT("Activation canceled"),A->FindAbilitySpecFromHandle(F.Recall) && A->FindAbilitySpecFromHandle(F.Recall)->IsActive());
        TestFalse(TEXT("No surviving cast/channel schedule"),Ability && Ability->HasActiveCombatSchedule());
        F.Advance(4); if (Mode!=4) TestTrue(TEXT("Old completion cannot teleport"),F.Hero->GetActorLocation().Equals(Origin+(Mode==1?FVector(500,0,0):FVector::ZeroVector),1));
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReturnBeaconNoDestinationTest, "Combat.Defense.ReturnBeacon.UnsafeDestinationEndsExactlyOnce", ReturnBeaconTests::Flags)
bool FReturnBeaconNoDestinationTest::RunTest(const FString&)
{
    ReturnBeaconTests::FFixture F; if (!F.Start(*this)) return false;
    auto* Events=F.World.GetWorld()->GetSubsystem<UCombatEventSubsystem>(); int32 Ended=0,ChannelEnded=0,Motion=0;
    const auto Handle=Events->OnRecord().AddLambda([&](const FCombatLogRecord& R)
    { if (R.SourceActorId!=F.Hero->GetUniqueID()) return; if(R.EventType==CombatTags::Event_Combat_AbilityEnded) ++Ended;
      if(R.EventType==CombatTags::Event_Combat_AbilityChannelEnded) ++ChannelEnded; if(R.EventType==CombatTags::Event_Combat_MotionStarted) ++Motion; });
    const FVector Origin=F.Hero->GetActorLocation(); TestTrue(TEXT("Cast in world without navigation"),F.Cast()); F.Advance(4);
    TestEqual(TEXT("Exactly one ability end"),Ended,1); TestEqual(TEXT("Exactly one channel end"),ChannelEnded,1);
    TestEqual(TEXT("No motion submitted for unsafe destination"),Motion,0); TestTrue(TEXT("No arbitrary fallback"),F.Hero->GetActorLocation().Equals(Origin,1));
    TestFalse(TEXT("No active activation"),F.Hero->GetCombatAbilitySystemComponent()->FindAbilitySpecFromHandle(F.Recall)->IsActive());
    Events->OnRecord().Remove(Handle); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReturnBeaconMotionTest, "Combat.Defense.ReturnBeacon.TeleportAuthorityCollisionAndReentry", ReturnBeaconTests::Flags)
bool FReturnBeaconMotionTest::RunTest(const FString&)
{
    for (int32 Mode=0; Mode<5; ++Mode)
    {
        ReturnBeaconTests::FFixture F; if (!F.Start(*this)) return false;
        auto* M=F.Hero->GetCombatMotionComponent(); const FVector Origin=F.Hero->GetActorLocation(); const FVector Dest=Origin+FVector(500,0,0);
        auto* Events=F.World.GetWorld()->GetSubsystem<UCombatEventSubsystem>(); int32 Started=0,Finished=0,Notified=0;
        FDelegateHandle Observer=Events->OnRecord().AddLambda([&](const FCombatLogRecord& R)
        { if(R.SourceActorId!=F.Hero->GetUniqueID()) return;
          if(R.EventType==CombatTags::Event_Combat_MotionStarted) { ++Started;
            if(Mode==3) F.Hero->GetCombatLifecycleComponent()->RequestDeath({},F.Hero);
            if(Mode==4) TestFalse(TEXT("Synchronous nested teleport rejected"),M->TryTeleport(Dest,{},{}).bSuccess); }
          if(R.EventType==CombatTags::Event_Combat_MotionFinished) ++Finished; });
        const auto Done=M->OnMotionFinished().AddLambda([&](const FCombatMotionResult&){++Notified;});
        if(Mode==1)
        {
            auto* Wall=F.World.GetWorld()->SpawnActor<AActor>(); auto* Box=NewObject<UBoxComponent>(Wall); Wall->SetRootComponent(Box); Box->SetBoxExtent(FVector(100));
            Box->SetCollisionProfileName(TEXT("BlockAll")); Box->RegisterComponent(); Box->SetWorldLocation(Dest);
        }
        if(Mode==2) { FCombatMotionRequest R; R.TargetLocation=Dest; R.Speed=100; TestTrue(TEXT("Continuous motion acquired"),M->TryAcquireMotion(R).bSuccess); }
        const auto R=M->TryTeleport(Dest,{},{});
        TestEqual(TEXT("Success only with unblocked alive owner"),R.bSuccess,Mode==0 || Mode==4);
        TestEqual(TEXT("Failed teleport preserves position"),F.Hero->GetActorLocation().Equals(Origin,1),Mode!=0 && Mode!=4);
        if(Mode!=2) { TestEqual(TEXT("Exactly one start when admitted"),Started,Mode==1?0:1); TestEqual(TEXT("Exactly one finish when admitted"),Finished,Mode==1?0:1); TestEqual(TEXT("Exactly one delegate when admitted"),Notified,Mode==1?0:1); }
        Events->OnRecord().Remove(Observer); M->OnMotionFinished().Remove(Done);
        M->HandleOwnerDeath(); TestEqual(TEXT("Registry drained"),M->GetActiveMotionCount(),0);
    }
    FCombatAutomationWorldFixture Client(NM_Client); auto* H=Client.GetWorld()->SpawnActor<ACombatUnitCharacter>();
    auto* Data=NewObject<UCombatUnitData>(H); Data->DefinitionName=TEXT("return_beacon_client_fixture"); H->InitializeFromUnitData(Data);
    TestFalse(TEXT("Client native entry rejects authority"),H->GetCombatMotionComponent()->TryTeleport(FVector(500,0,0),{},{}).bSuccess);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReturnBeaconStartReentryTest,"Combat.Defense.ReturnBeacon.StartEventCancellationLeavesNoTask",ReturnBeaconTests::Flags)
bool FReturnBeaconStartReentryTest::RunTest(const FString&)
{
    for(int32 Mode=0;Mode<3;++Mode)
    {
        ReturnBeaconTests::FFixture F;if(!F.Start(*this))return false;
        auto* A=F.Hero->GetCombatAbilitySystemComponent();auto* Events=F.World.GetWorld()->GetSubsystem<UCombatEventSubsystem>();int32 Ended=0;
        const auto Observer=Events->OnRecord().AddLambda([&](const FCombatLogRecord& R)
        {
            if(R.SourceActorId!=F.Hero->GetUniqueID())return;
            if(R.EventType==CombatTags::Event_Combat_AbilityEnded)++Ended;
            const auto Stage=Mode==0?CombatTags::Event_Combat_AbilityCastStarted.GetTag():CombatTags::Event_Combat_AbilitySpellStarted.GetTag();
            if(R.EventType==Stage){if(Mode<2)A->CancelAbilityHandle(F.Recall);else F.Hero->GetCombatLifecycleComponent()->RequestDeath({},F.Hero);}
        });
        F.Cast();const auto* Spec=A->FindAbilitySpecFromHandle(F.Recall);
        const auto* Ability=Spec?Cast<UCombatGameplayAbility>(Spec->GetPrimaryInstance()):nullptr;
        TestFalse(TEXT("Canceled start never leaves a live channel task"),Ability&&Ability->HasActiveCombatSchedule());
        TestTrue(TEXT("Canceled start cannot publish channel phase"),F.Hero->GetCombatUnitViewComponent()->GetUnitView().AbilityPhase!=ECombatAbilityViewPhase::Channeling);
        TestEqual(TEXT("One end despite start-event reentry"),Ended,1);F.Advance(4);
        TestFalse(TEXT("No later active task"),Ability&&Ability->HasActiveCombatSchedule());Events->OnRecord().Remove(Observer);
    }
    return true;
}
#endif
