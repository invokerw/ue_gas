#include "CoreMinimal.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Combat/Items/CombatItemData.h"
#include "Combat/Economy/CombatShopData.h"
#include "Combat/Defense/DefenseEquipment.h"
#include "Combat/Defense/DefenseGameMode.h"
#include "Combat/Defense/DefenseDirectorComponent.h"
#include "Combat/Defense/DefenseData.h"
#include "Combat/Defense/DefenseGameState.h"
#include "Combat/Defense/DefenseObjective.h"
#include "Combat/Items/CombatInventoryComponent.h"
#include "Combat/Items/CombatItemSubsystem.h"
#include "Combat/Economy/CombatEconomyComponent.h"
#include "Combat/Economy/CombatEconomyData.h"
#include "Combat/Ability/CombatAbilitySystemComponent.h"
#include "Combat/Attributes/CombatAttributeSet.h"
#include "Combat/Attack/CombatAttackComponent.h"
#include "Combat/Attack/CombatAttackTimingPolicy.h"
#include "Combat/Combat/CombatDamageSubsystem.h"
#include "Combat/Modifiers/CombatModifierComponent.h"
#include "Combat/Log/CombatEventSubsystem.h"
#include "Combat/Unit/CombatUnitLifecycleComponent.h"
#include "Combat/Network/CombatNetworkSecuritySubsystem.h"
#include "Combat/Tests/CombatAutomationWorldFixture.h"
#include "Combat/Core/CombatTags.h"
#include "CombatPlayerController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/World.h"

/** 正式内容必须冷加载成功；缺少资产或策略不能用瞬态占位冒充完成。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDefenseEquipmentAssetsTest, "Combat.Defense.Equipment.AuthoredCatalog", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDefenseEquipmentAssetsTest::RunTest(const FString&)
{
    const TCHAR* Names[] = {TEXT("item_iron_bow"), TEXT("item_swift_string"), TEXT("item_stone_guard"), TEXT("item_hunter_scope")};
    const int64 Prices[] = {180, 220, 160, 260};
    auto* Shop = LoadObject<UCombatShopData>(nullptr, TEXT("/Game/Combat/Demo/Defense/DA_DefenseUnifiedShop.DA_DefenseUnifiedShop"));
    if (!TestNotNull(TEXT("Production shop loads"), Shop)) return false;
    TestNotNull(TEXT("Item purchase policy extension exists"), UCombatItemData::StaticClass()->FindPropertyByName(TEXT("ShopPolicy")));
    for (int32 Index = 0; Index < 4; ++Index)
    {
        const FString Name = FString::Printf(TEXT("DA_%s"), Names[Index]);
        const FString Path = FString::Printf(TEXT("/Game/Combat/Demo/Defense/Equipment/%s.%s"), *Name, *Name);
        auto* Item = LoadObject<UCombatItemData>(nullptr, *Path);
        if (!TestNotNull(*Name, Item)) continue;
        TestEqual(TEXT("Stable item identity"), Item->GetPrimaryAssetId(), FPrimaryAssetId(TEXT("CombatItem"), FName(Names[Index])));
        TestEqual(TEXT("Authored price"), Item->PurchasePrice, Prices[Index]);
        TestTrue(TEXT("Original catalog resolves new item"), Shop->FindItem(Item->GetPrimaryAssetId()) == Item);
        TestFalse(TEXT("Equipment occupies inventory"), Item->bUseOnAcquire);
        TestEqual(TEXT("Exactly one passive"), Item->Passives.Num(), 1);
        FString Error; TestTrue(TEXT("Definition validates"), Item->ValidateRuntime(Error));
    }
    TestNotNull(TEXT("Team supply preserved"), Shop->FindItem(FPrimaryAssetId(TEXT("CombatItem"), TEXT("team_shrine_barrier"))));
    return true;
}

/** 仅测试驱动正式阶段入口，生产没有调试阶段RPC。 */
struct FDefenseEquipmentTestAccess
{
    static void Phase(UDefenseDirectorComponent* D, EDefensePhase P, int32 Wave = 0) { D->EnterPhase(P, Wave, 60); }
    static void Finish(UDefenseDirectorComponent* D) { D->Finish(false, TEXT("EquipmentTest")); }
    /** 成功事务的计数不依赖英雄是否已在同步通知中结束。 */
    static int32 Count(UDefenseDirectorComponent* D, ACombatPlayerController* P) { const auto* R=D->Players.Find(P);return R?R->EquipmentPurchases:0; }
};
namespace DefenseEquipmentTests
{
    constexpr auto Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;
    /** 真实已保存装备，测试不回写CDO或资产。 */
    UCombatItemData* Item(const TCHAR* Name)
    {
        const FString Asset = FString::Printf(TEXT("DA_%s"), Name);
        return LoadObject<UCombatItemData>(nullptr, *FString::Printf(TEXT("/Game/Combat/Demo/Defense/Equipment/%s.%s"), *Asset, *Asset));
    }
    /** 独立数值单位，关闭恢复和无地板移动，只验证公共战斗与物品接口。 */
    ACombatUnitCharacter* Unit(UWorld& W, FName Name, bool Enemy = false)
    {
        FActorSpawnParameters Params; Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        auto* Hero = W.SpawnActor<ACombatUnitCharacter>(FVector(1000,0,100), FRotator::ZeroRotator, Params);
        auto* Data = NewObject<UCombatUnitData>(Hero); Data->DefinitionName = Name;
        Data->InitialTeamId = FCombatTeamId(Enemy ? 2 : 1);
        Data->BaseStats.MaxHealth = 1000; Data->BaseStats.HealthRegen = Data->BaseStats.ManaRegen = 0;
        Data->BaseStats.Armor = 0; Data->BaseStats.AttackDamage = 100; Data->BaseStats.AttackSpeed = 100;
        Data->BaseStats.BaseAttackTime = 1.7f; Data->BaseStats.AttackRange = 1000;
        Data->AttackFacingToleranceDegrees = 180; Data->BaseAttackPoint = 0;
        Hero->InitializeFromUnitData(Data);
        if (!Hero->HasActorBegunPlay()) Hero->DispatchBeginPlay();
        Hero->GetCharacterMovement()->DisableMovement(); return Hero;
    }
    /** 请求公共伤害事务，返回真实AppliedAmount。 */
    float Damage(ACombatUnitCharacter* Hero, ACombatUnitCharacter* Target, ECombatDirectSourceType Source = ECombatDirectSourceType::Attack)
    {
        FCombatDamageRequest R; R.Source=Hero; R.Target=Target; R.Amount=100; R.SourceContext.DirectSourceType=Source;
        return Hero->GetWorld()->GetSubsystem<UCombatDamageSubsystem>()->DealDamage(R).Event.AppliedAmount;
    }
    /** 单独World中创建正式Mode组件与公开Director状态，避免导航/PIE依赖掩盖事务错误。 */
    struct FFixture
    {
        FCombatAutomationWorldFixture World;
        ADefenseGameMode* Mode=nullptr;
        UDefenseDirectorComponent* Director=nullptr;
        ACombatPlayerController* Player=nullptr;
        ACombatUnitCharacter* Hero=nullptr;
        UCombatShopData* Shop=nullptr;
        bool Start(FAutomationTestBase& T)
        {
            auto* W=World.GetWorld();
            // Deferred Mode不调用地图BeginPlay；用引擎公开上下文接入实际Mode，Director仍走正式Initialize。
            Mode=W->SpawnActorDeferred<ADefenseGameMode>(ADefenseGameMode::StaticClass(), FTransform::Identity);
            auto* State=W->SpawnActor<ADefenseGameState>(); W->CopyGameState(Mode,State);
            Director=Mode->GetDefenseDirector(); if (!Director->IsRegistered()) Director->RegisterComponent();
            auto* Run=UDefenseRunData::CreatePhase1Default(Mode); Run->PreparationDuration=60; Run->bEnablePrototypeEvents=false;
            auto* Core=W->SpawnActor<ADefenseObjective>(); Core->InitializeFromUnitData(Run->ObjectiveData);
            if (!Core->HasActorBegunPlay()) Core->DispatchBeginPlay();
            if (!T.TestTrue(TEXT("Initialize actual director"),Director->InitializeForRun(Run,Core,State))) return false;
            Shop=LoadObject<UCombatShopData>(nullptr,TEXT("/Game/Combat/Demo/Defense/DA_DefenseUnifiedShop.DA_DefenseUnifiedShop"));
            if (!T.TestNotNull(TEXT("Authored equipment catalog"),Shop) || !T.TestTrue(TEXT("Initialize domain catalog"),Director->InitializeTeamItems(Shop))) return false;
            Player=W->SpawnActor<ACombatPlayerController>(); Hero=Unit(*W,TEXT("equipment_hero"));
            if (!T.TestTrue(TEXT("Bind hero"),Player->SetCommandedUnitAuthority(Hero))) return false;
            auto* Rules=NewObject<UCombatEconomyData>(Player); Rules->DefinitionName=TEXT("equipment_rules"); Rules->StartingGold=3000; Rules->PassiveGoldPerMinute=0;
            FString Error;
            if (!T.TestTrue(TEXT("Initialize real economy"),Player->GetCombatEconomyComponent()->InitializeForMatch(Rules,Shop,true,Error))) { T.AddError(Error); return false; }
            W->GetSubsystem<UCombatNetworkSecuritySubsystem>()->BurstCapacity=1000;
            return T.TestTrue(TEXT("Register same hero"),Director->RegisterPlayer(Player,Hero));
        }
        ~FFixture() { if (Director) Director->Shutdown(); }
        /** 每次意图从服务器当前投影构造，使用正式连接处理入口。 */
        FCombatEconomyRequest Request(int32 Id,const TCHAR* Name) const
        {
            auto* Economy=Player->GetCombatEconomyComponent(); FCombatEconomyRequest R; R.RequestId=Id;
            R.ItemDefinitionId=FPrimaryAssetId(TEXT("CombatItem"),Name); R.CommandBindingGeneration=Player->GetCommandBindingGeneration();
            R.ExpectedEconomyRevision=Economy->GetEconomyRevision(); R.ExpectedInventoryRevision=Economy->GetInventoryRevision();
            if (const auto* Offer=Economy->FindShopOffer(R.ItemDefinitionId)) { R.PurchaseContextId=Offer->ContextId;R.PurchaseContextRevision=Offer->Revision; }
            return R;
        }
        FCombatEconomyResult Buy(int32 Id,const TCHAR* Name) { return Player->ProcessEconomyRequestForConnection(Player,Request(Id,Name)); }
    };
}

/** 铁弓只影响直接普攻；瞄准镜按稳定敌人身份处理每个伤害事务。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDefenseEquipmentDamageTest,"Combat.Defense.Equipment.ActualDamageAndSourceBoundaries",DefenseEquipmentTests::Flags)
bool FDefenseEquipmentDamageTest::RunTest(const FString&)
{
    FCombatAutomationWorldFixture F; auto& W=*F.GetWorld();
    auto* Hero=DefenseEquipmentTests::Unit(W,TEXT("bow_hero")); auto* Target=DefenseEquipmentTests::Unit(W,TEXT("unknown_target"),true);
    FCombatItemHandle Handle; FGameplayTag Failure;
    if (!TestTrue(TEXT("Equip real bow"),Hero->GetCombatInventoryComponent()->GiveItem(DefenseEquipmentTests::Item(TEXT("item_iron_bow")),1,Handle,Failure))) return false;
    TestTrue(TEXT("Attack gets twelve percent"),FMath::IsNearlyEqual(DefenseEquipmentTests::Damage(Hero,Target),112.f,.01f));
    for (auto Source : {ECombatDirectSourceType::Ability,ECombatDirectSourceType::Item,ECombatDirectSourceType::Modifier})
        TestTrue(TEXT("Derived damage is not multiplied"),FMath::IsNearlyEqual(DefenseEquipmentTests::Damage(Hero,Target,Source),100.f,.01f));
    const auto* Instance=W.GetSubsystem<UCombatItemSubsystem>()->FindItem(Handle); auto* Inventory=Hero->GetCombatInventoryComponent();
    TestTrue(TEXT("Move bow to backpack"),Inventory->TrySwap(Instance->GetSlot(),6,Inventory->GetRevision(),Handle,{},Failure));
    TestTrue(TEXT("Backpack removes bow hook"),FMath::IsNearlyEqual(DefenseEquipmentTests::Damage(Hero,Target),100.f,.01f));
    Inventory->ClearInventory();
    auto* Hunter=DefenseEquipmentTests::Unit(W,TEXT("scope_hero"));
    if (!TestTrue(TEXT("Equip real scope"),Hunter->GetCombatInventoryComponent()->GiveItem(DefenseEquipmentTests::Item(TEXT("item_hunter_scope")),1,Handle,Failure))) return false;
    for (auto Name : {TEXT("defense_invader"),TEXT("defense_assassin"),TEXT("defense_siege"),TEXT("defense_boss"),TEXT("defense_elite"),TEXT("unknown")})
    {
        auto* Enemy=DefenseEquipmentTests::Unit(W,FName(Name),true);
        const bool Special=FString(Name).Contains(TEXT("siege"))||FString(Name).Contains(TEXT("boss"))||FString(Name).Contains(TEXT("elite"));
        TestTrue(FString::Printf(TEXT("Scope role %s"),Name),FMath::IsNearlyEqual(DefenseEquipmentTests::Damage(Hunter,Enemy,ECombatDirectSourceType::Ability),Special?112.f:100.f,.01f));
    }
    return true;
}
/** 迅弦进入公共攻击时序，石卫只改变上限；换装等待、死亡复活和清理不复制效果。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDefenseEquipmentStatsTest,"Combat.Defense.Equipment.AttackTimingHealthAndLifecycle",DefenseEquipmentTests::Flags)
bool FDefenseEquipmentStatsTest::RunTest(const FString&)
{
    FCombatAutomationWorldFixture F; auto& W=*F.GetWorld();auto* H=DefenseEquipmentTests::Unit(W,TEXT("stats_hero"));auto* I=H->GetCombatInventoryComponent();
    FCombatItemHandle String,Stone;FGameplayTag Fail;
    if(!TestTrue(TEXT("Equip swift"),I->GiveItem(DefenseEquipmentTests::Item(TEXT("item_swift_string")),1,String,Fail)) || !TestTrue(TEXT("Equip stone"),I->GiveItem(DefenseEquipmentTests::Item(TEXT("item_stone_guard")),1,Stone,Fail)))return false;
    const auto* A=H->GetCombatAttributeSet();TestTrue(TEXT("BAT is 92 percent"),FMath::IsNearlyEqual(A->GetBaseAttackTime(),1.7f*.92f));
    TestEqual(TEXT("Cooldown unchanged"),A->GetCooldownReductionPct(),0.f);TestEqual(TEXT("Health cap increases"),A->GetMaxHealth(),1120.f);TestEqual(TEXT("Current health not granted"),A->GetHealth(),1000.f);
    auto* Target=DefenseEquipmentTests::Unit(W,TEXT("timing_target"),true);Target->SetActorEnableCollision(false);
    const auto Attack=H->GetCombatAttackComponent()->StartMeleeAttack(Target, FCombatOrderHandle());FCombatAttackRecord R;
    TestTrue(TEXT("Actual attack starts"),Attack.bSuccess);TestTrue(TEXT("Actual timing snapshot"),H->GetCombatAttackComponent()->GetAttackRecordSnapshot(Attack.Handle,R));
    TestTrue(TEXT("Actual interval shortened eight percent"),FMath::IsNearlyEqual(R.Timing.AttackInterval,1.7f*.92f));
    const int32 Slot=W.GetSubsystem<UCombatItemSubsystem>()->FindItem(Stone)->GetSlot();
    TestTrue(TEXT("Stone to backpack"),I->TrySwap(Slot,6,I->GetRevision(),Stone,{},Fail));TestEqual(TEXT("Backpack removes health cap"),A->GetMaxHealth(),1000.f);
    TestTrue(TEXT("Return stone to equipment"),I->TrySwap(6,Slot,I->GetRevision(),Stone,{},Fail));TestEqual(TEXT("Reequip waits"),A->GetMaxHealth(),1000.f);
    for(int32 N=0;N<125;++N) W.Tick(LEVELTICK_All,.05f);
    TestEqual(TEXT("Reequip restores cap once"),A->GetMaxHealth(),1120.f);TestTrue(TEXT("Reequip does not heal"),A->GetHealth()<=1000.f);
    FCombatDamageRequest Death;Death.Source=H;Death.Target=H;Death.Amount=100000;Death.DamageType=ECombatDamageType::Pure;
    W.GetSubsystem<UCombatDamageSubsystem>()->DealDamage(Death);TestEqual(TEXT("Dead removes equipment"),A->GetMaxHealth(),1000.f);
    TestTrue(TEXT("Respawn through public lifecycle"),H->GetCombatLifecycleComponent()->RespawnAtLocation(FVector(1000,0,100)));
    TestEqual(TEXT("Respawn restores equipment once"),A->GetMaxHealth(),1120.f);I->ClearInventory();
    TestEqual(TEXT("Explicit teardown clears inventory"),I->GetItemCount(),0);TestEqual(TEXT("No equipment runtimes remain"),H->GetCombatModifierComponent()->GetActiveModifierCount(),0);
    return true;
}
/** 普通购买、缓存重放、背包锁定限持有和两次窗口配额均经过真实经济入口。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDefenseEquipmentPurchaseTest,"Combat.Defense.Equipment.PurchaseReplayQuotaAndOwnership",DefenseEquipmentTests::Flags)
bool FDefenseEquipmentPurchaseTest::RunTest(const FString&)
{
    DefenseEquipmentTests::FFixture F;if(!F.Start(*this))return false;auto* E=F.Player->GetCombatEconomyComponent();
    const auto Request=F.Request(1,TEXT("item_iron_bow"));const auto First=F.Player->ProcessEconomyRequestForConnection(F.Player,Request);
    if(!TestTrue(TEXT("Purchase ordinary equipment"),First.bSuccess))return false;TestFalse(TEXT("Ordinary result remains in inventory"),First.bConsumedOnAcquire);TestTrue(TEXT("Result has instance"),First.ItemHandle.IsValid());
    const auto Repeat=F.Player->ProcessEconomyRequestForConnection(F.Player,Request);TestTrue(TEXT("Exact repeat returns original success"),Repeat.bSuccess);TestEqual(TEXT("Same stable handle"),Repeat.ItemHandle,First.ItemHandle);TestEqual(TEXT("Gold charged once"),E->GetGold(),int64(2820));
    auto Changed=Request;Changed.ItemDefinitionId=FPrimaryAssetId(TEXT("CombatItem"),TEXT("item_stone_guard"));TestEqual(TEXT("Same ID different payload rejected"),F.Player->ProcessEconomyRequestForConnection(F.Player,Changed).FailureTag,CombatTags::Failure_Network_DuplicateRequest.GetTag());
    auto* I=F.Hero->GetCombatInventoryComponent();auto* Items=F.World.GetWorld()->GetSubsystem<UCombatItemSubsystem>();auto* Instance=Items->FindItem(First.ItemHandle);FGameplayTag Failure;
    TestTrue(TEXT("Bow moves to backpack"),I->TrySwap(Instance->GetSlot(),6,I->GetRevision(),First.ItemHandle,{},Failure));
    bool Locked=false;TestTrue(TEXT("Lock backpack bow"),E->ToggleInventoryItemLock(First.ItemHandle,E->FindInventoryItemRevision(First.ItemHandle),E->GetEconomyRevision(),E->GetInventoryRevision(),Locked,Failure));
    TestFalse(TEXT("Locked backpack cannot bypass same definition cap"),F.Buy(2,TEXT("item_iron_bow")).bSuccess);
    FCombatItemHandle Gift;TestFalse(TEXT("Trusted grant also respects holding cap"),I->GiveItem(DefenseEquipmentTests::Item(TEXT("item_iron_bow")),1,Gift,Failure));
    TestTrue(TEXT("Second distinct equipment"),F.Buy(3,TEXT("item_stone_guard")).bSuccess);TestFalse(TEXT("Third hits window quota"),F.Buy(4,TEXT("item_swift_string")).bSuccess);
    TestTrue(TEXT("Sell bow"),E->SellInventoryItem(First.ItemHandle,E->FindInventoryItemRevision(First.ItemHandle),E->GetEconomyRevision(),E->GetInventoryRevision(),Failure));
    TestFalse(TEXT("Selling does not restore quota"),F.Buy(5,TEXT("item_swift_string")).bSuccess);
    FDefenseEquipmentTestAccess::Phase(F.Director,EDefensePhase::WaveCombat);TestFalse(TEXT("Combat purchase denied"),F.Buy(6,TEXT("item_swift_string")).bSuccess);
    FDefenseEquipmentTestAccess::Phase(F.Director,EDefensePhase::Intermission);TestTrue(TEXT("Next window renews quota"),F.Buy(7,TEXT("item_swift_string")).bSuccess);
    auto Stale=F.Request(8,TEXT("item_hunter_scope"));--Stale.PurchaseContextRevision;TestEqual(TEXT("Stale phase refused"),F.Player->ProcessEconomyRequestForConnection(F.Player,Stale).FailureTag,CombatTags::Failure_Economy_Stale.GetTag());
    FDefenseEquipmentTestAccess::Finish(F.Director);TestFalse(TEXT("Terminal denies"),F.Buy(9,TEXT("item_hunter_scope")).bSuccess);return true;
}
/** 缺钱、满库存、外来连接和提交观察者重入不能扣款或消耗窗口次数。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDefenseEquipmentAtomicTest,"Combat.Defense.Equipment.AtomicFailuresAndReentry",DefenseEquipmentTests::Flags)
bool FDefenseEquipmentAtomicTest::RunTest(const FString&)
{
    DefenseEquipmentTests::FFixture F;if(!F.Start(*this))return false;auto* E=F.Player->GetCombatEconomyComponent();FString Error;
    E->SetGoldForDebug(0,Error);TestFalse(TEXT("No money"),F.Buy(1,TEXT("item_stone_guard")).bSuccess);TestEqual(TEXT("No quota lost"),E->FindShopOffer(FPrimaryAssetId(TEXT("CombatItem"),TEXT("item_iron_bow")))->RemainingPurchases,2);
    E->SetGoldForDebug(3000,Error);auto* Other=F.World.GetWorld()->SpawnActor<ACombatPlayerController>();
    TestEqual(TEXT("Foreign owner denied"),F.Player->ProcessEconomyRequestForConnection(Other,F.Request(2,TEXT("item_stone_guard"))).FailureTag,CombatTags::Failure_Network_Ownership.GetTag());
    auto* Events=F.World.GetWorld()->GetSubsystem<UCombatEventSubsystem>();bool Reentered=false,Accepted=false;
    const auto Delegate=Events->OnRecord().AddLambda([&](const FCombatLogRecord& R)
    {
        if(!Reentered && R.EventType==CombatTags::Event_Combat_ItemChanged && R.ItemAction==TEXT("Purchased"))
        {Reentered=true;Accepted=F.Buy(11,TEXT("item_swift_string")).bSuccess;}
    });
    TestTrue(TEXT("Outer purchase"),F.Buy(10,TEXT("item_stone_guard")).bSuccess);Events->OnRecord().Remove(Delegate);
    TestTrue(TEXT("Observed committed inventory"),Reentered);TestFalse(TEXT("Nested purchase denied"),Accepted);TestEqual(TEXT("Only actual purchase charged"),E->GetGold(),int64(2840));
    TestTrue(TEXT("Second purchase remains possible"),F.Buy(12,TEXT("item_iron_bow")).bSuccess);
    DefenseEquipmentTests::FFixture G;if(!G.Start(*this))return false;auto* Inventory=G.Hero->GetCombatInventoryComponent();FGameplayTag Failure;FCombatItemHandle H;
    auto* Filler=NewObject<UCombatItemData>(G.Player);Filler->DefinitionName=TEXT("equipment_inventory_filler");
    for(int32 N=0;N<9;++N)TestTrue(TEXT("Fill real inventory"),Inventory->GiveItem(Filler,1,H,Failure));
    TestFalse(TEXT("Full rejects purchase"),G.Buy(1,TEXT("item_stone_guard")).bSuccess);TestEqual(TEXT("Full kept gold"),G.Player->GetCombatEconomyComponent()->GetGold(),int64(3000));
    TestEqual(TEXT("Full kept quota"),G.Player->GetCombatEconomyComponent()->FindShopOffer(FPrimaryAssetId(TEXT("CombatItem"),TEXT("item_stone_guard")))->RemainingPurchases,2);return true;
}
/** 安全默认值与非法组合，阻止配方绕过领域购买策略。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDefenseEquipmentValidationTest,"Combat.Defense.Equipment.DefinitionAndRecipeBoundaries",DefenseEquipmentTests::Flags)
bool FDefenseEquipmentValidationTest::RunTest(const FString&)
{
    auto* Item=NewObject<UCombatItemData>();Item->DefinitionName=TEXT("equipment_validation");FString Error;
    TestTrue(TEXT("Legacy defaults remain valid"),Item->ValidateRuntime(Error));Item->MaxOwnedQuantity=-1;TestFalse(TEXT("Negative holding cap"),Item->ValidateRuntime(Error));
    Item->MaxOwnedQuantity=1;Item->MaxStack=2;TestFalse(TEXT("Stack exceeds holding cap"),Item->ValidateRuntime(Error));Item->MaxStack=1;
    Item->ShopPolicy=NewObject<UDefenseEquipmentShopPolicy>(Item);TestFalse(TEXT("Policy cannot attach to non purchasable"),Item->ValidateRuntime(Error));
    Item->bPurchasable=true;Item->PurchasePrice=10;TestTrue(TEXT("Valid bounded leaf"),Item->ValidateRuntime(Error));
    auto* Shop=NewObject<UCombatShopData>();Shop->DefinitionName=TEXT("equipment_recipe_check");auto& Category=Shop->Categories.AddDefaulted_GetRef();Category.CategoryId=TEXT("gear");Category.DisplayName=FText::FromString(TEXT("装备"));Category.Items.Add(Item);
    auto* Recipe=NewObject<UCombatItemData>(Shop);Recipe->DefinitionName=TEXT("forbidden_equipment_recipe");auto& Ingredient=Recipe->Recipe.AddDefaulted_GetRef();Ingredient.Item=Item;Category.Items.Add(Recipe);
    TestFalse(TEXT("Policy equipment cannot be crafted through another item"),Shop->ValidateRuntime(Error));return true;
}
/** 库存提交观察者推进阶段时仍记录原窗口，结束英雄时仍按稳定付款人记一次。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDefenseEquipmentCommitLifecycleTest,"Combat.Defense.Equipment.CommitObserverLifecycle",DefenseEquipmentTests::Flags)
bool FDefenseEquipmentCommitLifecycleTest::RunTest(const FString&)
{
    DefenseEquipmentTests::FFixture F;if(!F.Start(*this))return false;auto* Events=F.World.GetWorld()->GetSubsystem<UCombatEventSubsystem>();bool Done=false;
    const auto H=Events->OnRecord().AddLambda([&](const FCombatLogRecord& R)
    {
        if(!Done&&R.EventType==CombatTags::Event_Combat_ItemChanged&&R.ItemAction==TEXT("Purchased"))
        {Done=true;FDefenseEquipmentTestAccess::Phase(F.Director,EDefensePhase::Intermission);}
    });
    TestTrue(TEXT("Accepted purchase survives synchronous phase observer"),F.Buy(1,TEXT("item_stone_guard")).bSuccess);Events->OnRecord().Remove(H);
    const auto* O=F.Player->GetCombatEconomyComponent()->FindShopOffer(FPrimaryAssetId(TEXT("CombatItem"),TEXT("item_iron_bow")));
    TestTrue(TEXT("Prior window purchase did not charge next window quota"),O&&O->RemainingPurchases==2);
    DefenseEquipmentTests::FFixture G;if(!G.Start(*this))return false;auto* OtherEvents=G.World.GetWorld()->GetSubsystem<UCombatEventSubsystem>();bool Destroyed=false;
    const auto J=OtherEvents->OnRecord().AddLambda([&](const FCombatLogRecord& R)
    {
        if(!Destroyed&&R.EventType==CombatTags::Event_Combat_ItemChanged&&R.ItemAction==TEXT("Purchased"))
        {Destroyed=true;G.Hero->Destroy();}
    });
    G.Buy(1,TEXT("item_stone_guard"));OtherEvents->OnRecord().Remove(J);
    TestTrue(TEXT("Observed hero EndPlay"),Destroyed);TestEqual(TEXT("Stable payer recorded committed quota"),FDefenseEquipmentTestAccess::Count(G.Director,G.Player),1);
    return true;
}
#endif
