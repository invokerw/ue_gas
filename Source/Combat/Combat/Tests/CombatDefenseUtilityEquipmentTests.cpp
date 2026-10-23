#include "CoreMinimal.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Combat/Items/CombatItemData.h"
#include "Combat/Economy/CombatShopData.h"
#include "Combat/Data/CombatDefinitionData.h"
#include "Combat/Ability/CombatCooldownPolicy.h"
#include "Combat/Ability/CombatAbilitySystemComponent.h"
#include "Combat/Ability/CombatGameplayAbility.h"
#include "Combat/Defense/DefenseGameMode.h"
#include "Combat/Defense/DefenseDirectorComponent.h"
#include "Combat/Defense/DefenseGameState.h"
#include "Combat/Defense/DefenseData.h"
#include "Combat/Defense/DefenseObjective.h"
#include "Combat/Items/CombatInventoryComponent.h"
#include "Combat/Items/CombatItemSubsystem.h"
#include "Combat/Economy/CombatEconomyComponent.h"
#include "Combat/Economy/CombatEconomyData.h"
#include "Combat/Modifiers/CombatModifierComponent.h"
#include "Combat/Attributes/CombatAttributeSet.h"
#include "Combat/Combat/CombatDamageSubsystem.h"
#include "Combat/Combat/CombatHealSubsystem.h"
#include "Combat/Combat/CombatEffectUtilities.h"
#include "Combat/Tests/CombatAutomationWorldFixture.h"
#include "Combat/Unit/CombatUnitCharacter.h"
#include "Combat/Unit/CombatUnitLifecycleComponent.h"
#include "Combat/Network/CombatNetworkSecuritySubsystem.h"
#include "Combat/Core/CombatTags.h"
#include "CombatPlayerController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/World.h"


/** 正式两件装备和可选冷却策略必须存在，禁止用瞬态资产占位。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDefenseUtilityCatalogTest,"Combat.Defense.UtilityEquipment.AuthoredCatalog",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDefenseUtilityCatalogTest::RunTest(const FString&)
{
    auto* Shop=LoadObject<UCombatShopData>(nullptr,TEXT("/Game/Combat/Demo/Defense/DA_DefenseUnifiedShop.DA_DefenseUnifiedShop"));
    if(!TestNotNull(TEXT("Original catalog"),Shop))return false;
    TestNotNull(TEXT("Optional cooldown policy property"),UCombatAbilityData::StaticClass()->FindPropertyByName(TEXT("CooldownPolicy")));
    const TCHAR* Names[]={TEXT("item_mana_sigil"),TEXT("item_repair_lens")}; const int64 Prices[]={240,200};
    for(int32 Index=0;Index<2;++Index)
    {
        const FString Asset=FString::Printf(TEXT("DA_%s"),Names[Index]);
        auto* Item=LoadObject<UCombatItemData>(nullptr,*FString::Printf(TEXT("/Game/Combat/Demo/Defense/Equipment/%s.%s"),*Asset,*Asset));
        if(!TestNotNull(*Asset,Item))continue;
        TestEqual(TEXT("Stable item identity"),Item->GetPrimaryAssetId(),FPrimaryAssetId(TEXT("CombatItem"),Names[Index]));
        TestEqual(TEXT("Actual price"),Item->PurchasePrice,Prices[Index]);TestEqual(TEXT("Holding cap"),Item->MaxOwnedQuantity,1);
        TestTrue(TEXT("Shop references exact production item"),Shop->FindItem(Item->GetPrimaryAssetId())==Item);
        TestFalse(TEXT("Real equipment"),Item->bUseOnAcquire);FString Error;TestTrue(TEXT("Valid definition"),Item->ValidateRuntime(Error));
    }
    return true;
}

/** 测试只编排既有事件开启与清理，不增加生产调试入口。 */
struct FDefenseUtilityTestAccess
{
    static void Oath(UDefenseDirectorComponent* D){D->OpenEventAfterWave(3);}
    static int32 ReceiptCount(UDefenseDirectorComponent* D){return D->EventReceipts.Num();}
};
namespace DefenseUtilityTests
{
    constexpr auto Flags=EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter;
    /** 真实模式/英雄/经济夹具；没有手工改CDO、Health或冷却账本。 */
    struct FFixture
    {
        FCombatAutomationWorldFixture World;
        ADefenseGameMode* Mode=nullptr; ADefenseGameState* State=nullptr; UDefenseDirectorComponent* D=nullptr;
        ADefenseObjective* Core=nullptr; ACombatPlayerController* PC=nullptr; ACombatUnitCharacter* Hero=nullptr;
        UCombatShopData* Shop=nullptr;
        bool Start(FAutomationTestBase& T)
        {
            auto* W=World.GetWorld();Mode=W->SpawnActorDeferred<ADefenseGameMode>(ADefenseGameMode::StaticClass(),FTransform::Identity);
            State=W->SpawnActor<ADefenseGameState>();W->CopyGameState(Mode,State);D=Mode->GetDefenseDirector();if(!D->IsRegistered())D->RegisterComponent();
            auto* Run=UDefenseRunData::CreatePhase1Default(Mode);Run->PreparationDuration=60;
            Core=W->SpawnActor<ADefenseObjective>();Core->InitializeFromUnitData(Run->ObjectiveData);if(!Core->HasActorBegunPlay())Core->DispatchBeginPlay();
            if(!T.TestTrue(TEXT("Initialize real director"),D->InitializeForRun(Run,Core,State)))return false;
            Shop=LoadObject<UCombatShopData>(nullptr,TEXT("/Game/Combat/Demo/Defense/DA_DefenseUnifiedShop.DA_DefenseUnifiedShop"));
            if(!T.TestTrue(TEXT("Initialize original catalog"),D->InitializeTeamItems(Shop)))return false;
            PC=AddPlayer(T);Hero=PC?PC->GetCommandedUnit():nullptr;return Hero!=nullptr;
        }
        /** 第二名合法玩家用于本人/队友反例。 */
        ACombatPlayerController* AddPlayer(FAutomationTestBase& T)
        {
            auto* W=World.GetWorld();auto* Player=W->SpawnActor<ACombatPlayerController>();
            FActorSpawnParameters P;P.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
            auto* H=W->SpawnActor<ACombatUnitCharacter>(FVector(1000,500,100),FRotator::ZeroRotator,P);
            auto* Source=LoadObject<UCombatUnitData>(nullptr,TEXT("/Game/Combat/Demo/Defense/Guardian/DA_DefenseGuardian.DA_DefenseGuardian"));
            if(!T.TestNotNull(TEXT("Real guardian definition"),Source))return nullptr;
            auto* Data=DuplicateObject<UCombatUnitData>(Source,H);Data->BaseStats.HealthRegen=Data->BaseStats.ManaRegen=0;
            if(!T.TestTrue(TEXT("Initialize real guardian skills"),H->InitializeFromUnitData(Data)))return nullptr;
            if(!H->HasActorBegunPlay())H->DispatchBeginPlay();H->GetCharacterMovement()->DisableMovement();
            if(!T.TestTrue(TEXT("Bind current hero"),Player->SetCommandedUnitAuthority(H)))return nullptr;
            auto* Rules=NewObject<UCombatEconomyData>(Player);Rules->DefinitionName=TEXT("utility_rules");Rules->StartingGold=3000;Rules->PassiveGoldPerMinute=0;
            FString Error;if(!T.TestTrue(TEXT("Initialize real economy"),Player->GetCombatEconomyComponent()->InitializeForMatch(Rules,Shop,true,Error)))return nullptr;
            W->GetSubsystem<UCombatNetworkSecuritySubsystem>()->BurstCapacity=1000;
            return T.TestTrue(TEXT("Register actual hero"),D->RegisterPlayer(Player,H))?Player:nullptr;
        }
        ~FFixture(){if(D)D->Shutdown();}
        /** 原服务器连接入口，价格/效果不在请求中。 */
        FCombatEconomyResult Buy(ACombatPlayerController* P,int32 Id,const TCHAR* Name)
        {
            auto* E=P->GetCombatEconomyComponent();E->RefreshShopOffers();FCombatEconomyRequest R;
            R.RequestId=Id;R.CommandBindingGeneration=P->GetCommandBindingGeneration();R.ItemDefinitionId=FPrimaryAssetId(TEXT("CombatItem"),Name);
            R.ExpectedEconomyRevision=E->GetEconomyRevision();R.ExpectedInventoryRevision=E->GetInventoryRevision();
            if(const auto* O=E->FindShopOffer(R.ItemDefinitionId)){R.PurchaseContextId=O->ContextId;R.PurchaseContextRevision=O->Revision;}
            return P->ProcessEconomyRequestForConnection(P,R);
        }
        /** 真实公共伤害形成核心生命缺口。 */
        void Hurt(ACombatUnitCharacter* Unit,float Amount)
        {FCombatDamageRequest R;R.Source=R.Target=Unit;R.Amount=Amount;R.DamageType=ECombatDamageType::Pure;World.GetWorld()->GetSubsystem<UCombatDamageSubsystem>()->DealDamage(R);}
        /** 精确驱动游戏时间，不依赖墙钟。 */
        void Tick(float Seconds){for(int32 N=0;N<FMath::CeilToInt(Seconds/.05f);++N)World.GetWorld()->Tick(LEVELTICK_All,.05f);}
        /** 带正式局/阶段/生命/绑定身份的事件意图。 */
        FDefenseEventRequest Event(ACombatPlayerController* P,int32 Id,EDefenseEventOption Option) const
        {
            const auto V=D->BuildEventView();FDefenseEventRequest R;R.RequestId=Id;R.Option=Option;
            R.RunId=State->GetRunView().RunId;R.PhaseGeneration=D->GetPhaseGeneration();R.EventId=V.EventId;R.EventRevision=V.Revision;
            R.BindingGeneration=P->GetCommandBindingGeneration();R.LifeGeneration=P->GetCommandedUnit()->GetLifeGeneration();return R;
        }
    };
    /** 从真实已授予稳定定义查技能，不根据指针顺序猜Q/W/E。 */
    FGameplayAbilitySpecHandle Skill(ACombatUnitCharacter* H,const TCHAR* Name)
    {auto* S=H->GetCombatAbilitySystemComponent()->FindCombatAbilitySpecByDefinitionId(FPrimaryAssetId(TEXT("CombatAbility"),Name));return S?S->Handle:FGameplayAbilitySpecHandle();}
    /** 进入公开唯一冷却提交点，返回真实已冻结的剩余秒数。 */
    float Commit(FAutomationTestBase& T,ACombatUnitCharacter* H,const TCHAR* Name)
    {
        auto* A=H->GetCombatAbilitySystemComponent();const auto Handle=Skill(H,Name);const auto* Data=A->GetCombatAbilityData(Handle);
        if(!T.TestNotNull(TEXT("Granted production skill data"),Data))return -1;
        bool Cost=false,CD=false;FGameplayTag Fail;
        T.TestTrue(TEXT("Commit actual cooldown GE"),A->CommitCombatAbilityStage(Handle,*Data,1,ECombatAbilityCommitStage::SpellStarted,Cost,CD,Fail));
        T.TestTrue(TEXT("Cooldown committed once"),CD);return A->GetCombatAbilityCooldownRemaining(Handle);
    }
}

/** 条件CDR在原提交点冻结，背包/等待/死亡复活与清理都走原库存。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDefenseUtilityCooldownLifecycleTest,"Combat.Defense.UtilityEquipment.CooldownSnapshotsAndLifecycle",DefenseUtilityTests::Flags)
bool FDefenseUtilityCooldownLifecycleTest::RunTest(const FString&)
{
    DefenseUtilityTests::FFixture F;if(!F.Start(*this))return false;
    const auto Purchase=F.Buy(F.PC,1,TEXT("item_mana_sigil"));if(!TestTrue(TEXT("Buy real sigil"),Purchase.bSuccess))return false;
    auto* A=F.Hero->GetCombatAbilitySystemComponent();auto* M=F.Hero->GetCombatModifierComponent();auto* I=F.Hero->GetCombatInventoryComponent();
    TestTrue(TEXT("Q reduced by eight percent"),FMath::IsNearlyEqual(DefenseUtilityTests::Commit(*this,F.Hero,TEXT("guardian_shield_bash")),7.36f,.001f));
    TestTrue(TEXT("E reduced by eight percent"),FMath::IsNearlyEqual(DefenseUtilityTests::Commit(*this,F.Hero,TEXT("guardian_ground_slam")),12.88f,.001f));
    TestTrue(TEXT("W unaffected by sigil"),FMath::IsNearlyEqual(DefenseUtilityTests::Commit(*this,F.Hero,TEXT("guardian_fortify")),12.f,.001f));
    TestEqual(TEXT("Global ASC CDR unchanged"),F.Hero->GetCombatAttributeSet()->GetCooldownReductionPct(),0.f);
    const auto Q=DefenseUtilityTests::Skill(F.Hero,TEXT("guardian_shield_bash"));const float Frozen=A->GetCombatAbilityCooldownRemaining(Q);
    auto* Item=F.World.GetWorld()->GetSubsystem<UCombatItemSubsystem>()->FindItem(Purchase.ItemHandle);const int32 Slot=Item->GetSlot();FGameplayTag Fail;
    TestTrue(TEXT("Move sigil to backpack"),I->TrySwap(Slot,6,I->GetRevision(),Purchase.ItemHandle,{},Fail));
    TestEqual(TEXT("Backpack removes conditional contribution"),M->GetActiveRuntimeParameterSum(TEXT("defense_qe_cooldown_reduction")),0.f);
    TestEqual(TEXT("Existing cooldown never rescheduled"),A->GetCombatAbilityCooldownRemaining(Q),Frozen);
    TestTrue(TEXT("Reequip"),I->TrySwap(6,Slot,I->GetRevision(),Purchase.ItemHandle,{},Fail));
    TestEqual(TEXT("Six-second sleep has no contribution"),M->GetActiveRuntimeParameterSum(TEXT("defense_qe_cooldown_reduction")),0.f);
    F.Tick(6.1f);TestTrue(TEXT("Contribution restored exactly once"),FMath::IsNearlyEqual(M->GetActiveRuntimeParameterSum(TEXT("defense_qe_cooldown_reduction")),.08f));
    F.Hurt(F.Hero,100000);TestEqual(TEXT("Death removes contribution"),M->GetActiveRuntimeParameterSum(TEXT("defense_qe_cooldown_reduction")),0.f);
    TestTrue(TEXT("Public respawn"),F.Hero->GetCombatLifecycleComponent()->RespawnAtLocation(FVector(1000,500,100)));
    TestTrue(TEXT("Respawn restores once"),FMath::IsNearlyEqual(M->GetActiveRuntimeParameterSum(TEXT("defense_qe_cooldown_reduction")),.08f));
    I->ClearInventory();TestEqual(TEXT("Teardown contribution zero"),M->GetActiveRuntimeParameterSum(TEXT("defense_qe_cooldown_reduction")),0.f);return true;
}

/** 全局与条件相加一次、40%封顶；物品主动和旧空策略保留原全局公式。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDefenseUtilityCooldownBoundsTest,"Combat.Defense.UtilityEquipment.CooldownCapLegacyAndItems",DefenseUtilityTests::Flags)
bool FDefenseUtilityCooldownBoundsTest::RunTest(const FString&)
{
    DefenseUtilityTests::FFixture F;if(!F.Start(*this))return false;
    if(!TestTrue(TEXT("Buy sigil"),F.Buy(F.PC,1,TEXT("item_mana_sigil")).bSuccess))return false;
    auto* A=F.Hero->GetCombatAbilitySystemComponent();CombatEffectUtilities::ApplyAttributeAdditive(F.Hero,*A,UCombatAttributeSet::GetCooldownReductionPctAttribute(),.38f);
    TestTrue(TEXT("Q capped at forty percent"),FMath::IsNearlyEqual(DefenseUtilityTests::Commit(*this,F.Hero,TEXT("guardian_shield_bash")),4.8f,.001f));
    TestTrue(TEXT("E capped at forty percent"),FMath::IsNearlyEqual(DefenseUtilityTests::Commit(*this,F.Hero,TEXT("guardian_ground_slam")),8.4f,.001f));
    TestTrue(TEXT("W only global thirty-eight percent"),FMath::IsNearlyEqual(DefenseUtilityTests::Commit(*this,F.Hero,TEXT("guardian_fortify")),7.44f,.001f));
    auto* Item=NewObject<UCombatItemData>(F.PC);Item->DefinitionName=TEXT("utility_active_control");
    Item->ActiveAbility=LoadClass<UCombatGameplayAbility>(nullptr,TEXT("/Game/Combat/Demo/Defense/Guardian/GA_guardian_shield_bash.GA_guardian_shield_bash_C"));
    FCombatItemHandle Handle;FGameplayTag Fail;auto* I=F.Hero->GetCombatInventoryComponent();
    if(TestTrue(TEXT("Grant independent active item"),I->GiveItem(Item,1,Handle,Fail)))
    {
        const auto* Instance=F.World.GetWorld()->GetSubsystem<UCombatItemSubsystem>()->FindItem(Handle);
        const auto* Data=A->GetCombatAbilityData(Instance->GetAbilityHandle());bool Cost=false,CD=false;
        TestTrue(TEXT("Item cooldown commits"),A->CommitCombatAbilityStage(Instance->GetAbilityHandle(),*Data,1,ECombatAbilityCommitStage::SpellStarted,Cost,CD,Fail));
        TestTrue(TEXT("Item ignores hero scoped policy"),FMath::IsNearlyEqual(A->GetCombatAbilityCooldownRemaining(Instance->GetAbilityHandle()),4.96f,.001f));
    }
    auto* Data=NewObject<UCombatAbilityData>();Data->DefinitionName=TEXT("legacy_cooldown_control");Data->SpecialValues.FindOrAdd(TEXT("cooldown")).Values={10.f};
    TestNull(TEXT("Old definitions default to no policy"),Data->CooldownPolicy.Get());
    CombatEffectUtilities::ApplyAttributeAdditive(F.Hero,*A,UCombatAttributeSet::GetCooldownReductionPctAttribute(),.32f);
    const auto R=DefenseUtilityTests::Skill(F.Hero,TEXT("guardian_resolve"));bool Cost=false,CD=false;
    TestTrue(TEXT("Empty policy commits old formula"),A->CommitCombatAbilityStage(R,*Data,1,ECombatAbilityCommitStage::SpellStarted,Cost,CD,Fail));
    TestTrue(TEXT("Old formula permits seventy percent"),FMath::IsNearlyEqual(A->GetCombatAbilityCooldownRemaining(R),3.f,.001f));
    auto* Invalid=NewObject<UCombatCooldownPolicy>();Invalid->MaxReduction=-.1f;FString Error;TestFalse(TEXT("Invalid cap rejected"),Invalid->ValidateDefinition(Error));return true;
}

/** 本人应急石900、队友/背包750；真实Health差值与来源日志证明只归属购买者。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDefenseUtilityCoreHealTest,"Combat.Defense.UtilityEquipment.CoreRepairOwnership",DefenseUtilityTests::Flags)
bool FDefenseUtilityCoreHealTest::RunTest(const FString&)
{
    for(int32 Mode=0;Mode<3;++Mode)
    {
        DefenseUtilityTests::FFixture F;if(!F.Start(*this))return false;auto* Other=Mode==1?F.AddPlayer(*this):F.PC;if(!Other)return false;
        const auto Lens=F.Buy(Other,1,TEXT("item_repair_lens"));if(!TestTrue(TEXT("Buy actual lens"),Lens.bSuccess))return false;
        if(Mode==2){auto* I=F.Hero->GetCombatInventoryComponent();FGameplayTag Fail;const auto* Item=F.World.GetWorld()->GetSubsystem<UCombatItemSubsystem>()->FindItem(Lens.ItemHandle);TestTrue(TEXT("Lens in backpack"),I->TrySwap(Item->GetSlot(),6,I->GetRevision(),Lens.ItemHandle,{},Fail));}
        F.Hurt(F.Core,2000);const float Before=F.Core->GetCombatAttributeSet()->GetHealth();
        const auto Receipt=F.Buy(F.PC,2,TEXT("team_emergency_stone"));TestTrue(TEXT("Purchase real core repair"),Receipt.bSuccess);
        TestTrue(TEXT("Actual repair attribution"),FMath::IsNearlyEqual(F.Core->GetCombatAttributeSet()->GetHealth()-Before,Mode==0?900.f:750.f,.01f));
        TestEqual(TEXT("Exact core stone cost"),Receipt.GoldDelta,int64(-180));
    }
    return true;
}

/** 明确本人安全选择720，队友/自动超时/取消/背包均600，不改变其他人的事件选择。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDefenseUtilityEventHealTest,"Combat.Defense.UtilityEquipment.EventRepairAttribution",DefenseUtilityTests::Flags)
bool FDefenseUtilityEventHealTest::RunTest(const FString&)
{
    for(int32 Mode=0;Mode<5;++Mode)
    {
        DefenseUtilityTests::FFixture F;if(!F.Start(*this))return false;
        const auto Lens=F.Buy(F.PC,1,TEXT("item_repair_lens"));if(!TestTrue(TEXT("Equip lens"),Lens.bSuccess))return false;
        auto* Actor=Mode==1?F.AddPlayer(*this):F.PC;if(!Actor)return false;
        if(Mode==4){auto* I=F.Hero->GetCombatInventoryComponent();FGameplayTag Fail;const auto* Item=F.World.GetWorld()->GetSubsystem<UCombatItemSubsystem>()->FindItem(Lens.ItemHandle);I->TrySwap(Item->GetSlot(),6,I->GetRevision(),Lens.ItemHandle,{},Fail);}
        F.Hurt(F.Core,2000);const float Before=F.Core->GetCombatAttributeSet()->GetHealth();FDefenseUtilityTestAccess::Oath(F.D);
        if(Mode==2)F.Tick(20.1f);
        else
        {
            if(Mode==3)TestTrue(TEXT("Enter cancellable risk"),F.D->ProcessDefenseEvent(Actor,F.Event(Actor,100,EDefenseEventOption::Risk)).bSuccess);
            const auto Request=F.Event(Actor,101,Mode==3?EDefenseEventOption::Cancel:EDefenseEventOption::Safe);
            const auto Result=F.D->ProcessDefenseEvent(Actor,Request);TestTrue(TEXT("Real event request"),Result.bSuccess);
            TestTrue(TEXT("Exact event repeat returns original receipt"),F.D->ProcessDefenseEvent(Actor,Request).bSuccess);
        }
        TestTrue(TEXT("Actual event heal attribution"),FMath::IsNearlyEqual(F.Core->GetCombatAttributeSet()->GetHealth()-Before,Mode==0?720.f:600.f,.01f));
    }
    return true;
}

/** 公共治疗的普通目标和过量反例，观察者Shutdown后不复活事件状态。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDefenseUtilityHealBoundaryTest,"Combat.Defense.UtilityEquipment.HealBoundariesAndCommitShutdown",DefenseUtilityTests::Flags)
bool FDefenseUtilityHealBoundaryTest::RunTest(const FString&)
{
    DefenseUtilityTests::FFixture F;if(!F.Start(*this))return false;if(!TestTrue(TEXT("Equip lens"),F.Buy(F.PC,1,TEXT("item_repair_lens")).bSuccess))return false;
    F.Hurt(F.Hero,200);FCombatHealRequest R;R.Source=R.Target=F.Hero;R.Amount=100;
    auto* Heal=F.World.GetWorld()->GetSubsystem<UCombatHealSubsystem>();TestEqual(TEXT("Ordinary target not boosted"),Heal->Heal(R).Event.AppliedAmount,100.f);
    F.Hurt(F.Core,10);R.Target=F.Core;TestEqual(TEXT("Overheal limited by actual missing health"),Heal->Heal(R).Event.AppliedAmount,10.f);
    F.Hurt(F.Core,2000);FDefenseUtilityTestAccess::Oath(F.D);auto* Events=F.World.GetWorld()->GetSubsystem<UCombatEventSubsystem>();bool Observed=false;
    const auto Handle=Events->OnRecord().AddLambda([&](const FCombatLogRecord& Log)
    {if(!Observed&&Log.EventType==CombatTags::Event_Combat_HealApplied&&Log.TargetActorId==F.Core->GetUniqueID()){Observed=true;F.D->Shutdown();}});
    F.D->ProcessDefenseEvent(F.PC,F.Event(F.PC,101,EDefenseEventOption::Safe));Events->OnRecord().Remove(Handle);
    TestTrue(TEXT("Real heal observer ended run"),Observed);TestEqual(TEXT("Shutdown event remains inactive"),F.D->BuildEventView().State,EDefenseEventState::Inactive);TestEqual(TEXT("Shutdown does not resurrect receipt cache"),FDefenseUtilityTestAccess::ReceiptCount(F.D),0);return true;
}

/** 两件共用原整备配额，完整重放不二扣；出售撤销新效果而不重排已提交冷却。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDefenseUtilityTransactionTest,"Combat.Defense.UtilityEquipment.PurchaseReplayQuotaSellAndEndPlay",DefenseUtilityTests::Flags)
bool FDefenseUtilityTransactionTest::RunTest(const FString&)
{
    DefenseUtilityTests::FFixture F;if(!F.Start(*this))return false;auto* E=F.PC->GetCombatEconomyComponent();E->RefreshShopOffers();
    FCombatEconomyRequest R;R.RequestId=1;R.CommandBindingGeneration=F.PC->GetCommandBindingGeneration();R.ItemDefinitionId=FPrimaryAssetId(TEXT("CombatItem"),TEXT("item_mana_sigil"));
    R.ExpectedEconomyRevision=E->GetEconomyRevision();R.ExpectedInventoryRevision=E->GetInventoryRevision();const auto* O=E->FindShopOffer(R.ItemDefinitionId);
    if(!TestNotNull(TEXT("Real scoped offer"),O))return false;R.PurchaseContextId=O->ContextId;R.PurchaseContextRevision=O->Revision;
    const auto First=F.PC->ProcessEconomyRequestForConnection(F.PC,R);if(!TestTrue(TEXT("Buy sigil through connection"),First.bSuccess))return false;
    const auto Repeat=F.PC->ProcessEconomyRequestForConnection(F.PC,R);TestTrue(TEXT("Full replay success"),Repeat.bSuccess);TestEqual(TEXT("Same original handle"),Repeat.ItemHandle,First.ItemHandle);TestEqual(TEXT("Charged exactly once"),E->GetGold(),int64(2760));
    TestFalse(TEXT("Duplicate holding refused"),F.Buy(F.PC,2,TEXT("item_mana_sigil")).bSuccess);
    TestTrue(TEXT("Second utility item allowed"),F.Buy(F.PC,3,TEXT("item_repair_lens")).bSuccess);TestEqual(TEXT("Both cost440"),E->GetGold(),int64(2560));
    TestFalse(TEXT("Shared first batch quota applies"),F.Buy(F.PC,4,TEXT("item_stone_guard")).bSuccess);
    const float CD=DefenseUtilityTests::Commit(*this,F.Hero,TEXT("guardian_shield_bash"));FGameplayTag Fail;
    TestTrue(TEXT("Sell owned sigil"),E->SellInventoryItem(First.ItemHandle,E->FindInventoryItemRevision(First.ItemHandle),E->GetEconomyRevision(),E->GetInventoryRevision(),Fail));
    TestEqual(TEXT("Sold conditional effect removed"),F.Hero->GetCombatModifierComponent()->GetActiveRuntimeParameterSum(TEXT("defense_qe_cooldown_reduction")),0.f);
    TestEqual(TEXT("Selling preserves frozen cooldown"),F.Hero->GetCombatAbilitySystemComponent()->GetCombatAbilityCooldownRemaining(DefenseUtilityTests::Skill(F.Hero,TEXT("guardian_shield_bash"))),CD);
    TestFalse(TEXT("Selling cannot refill window quota"),F.Buy(F.PC,5,TEXT("item_mana_sigil")).bSuccess);
    auto* I=F.Hero->GetCombatInventoryComponent();F.Hero->Destroy();TestEqual(TEXT("Hero EndPlay clears remaining real item"),I->GetItemCount(),0);return true;
}
#endif
