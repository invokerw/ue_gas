#include "CoreMinimal.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Combat/Defense/DefenseData.h"
#include "Combat/Defense/DefenseCoreShieldRuntime.h"
#include "Combat/Defense/DefenseConditionalDamageRuntime.h"
#include "Combat/Ability/CombatAbilitySystemComponent.h"
#include "Combat/Combat/CombatEffectUtilities.h"
#include "Combat/Demo/CombatDemoModifierRuntimes.h"
#include "Combat/Defense/DefenseDirectorComponent.h"
#include "Combat/Defense/DefenseGameState.h"
#include "Combat/Defense/DefenseObjective.h"
#include "Combat/Defense/DefenseTeamItemEffect.h"
#include "Combat/Defense/DefensePlayerController.h"
#include "Combat/Economy/CombatEconomyComponent.h"
#include "Combat/Economy/CombatEconomyData.h"
#include "Combat/Economy/CombatShopData.h"
#include "Combat/Items/CombatItemData.h"
#include "Combat/Modifiers/CombatModifierComponent.h"
#include "Combat/Attributes/CombatAttributeSet.h"
#include "Combat/Combat/CombatDamageSubsystem.h"
#include "Combat/Unit/CombatUnitLifecycleComponent.h"
#include "Combat/Unit/CombatProgressionComponent.h"
#include "Combat/Scheduling/CombatSchedulerSubsystem.h"
#include "Combat/Network/CombatNetworkSecuritySubsystem.h"
#include "Combat/Core/CombatTags.h"
#include "UObject/GarbageCollection.h"
#include "Combat/Tests/CombatAutomationWorldFixture.h"

/** 测试驱动生产阶段转换和调度入口，避免为验证增加玩家调试旁路。 */
struct FDefenseTeamShopTestAccess
{
	static void Phase(UDefenseDirectorComponent* D,EDefensePhase Phase,int32 Wave) { D->EnterPhase(Phase,Wave,25); D->ExpireTeamShop(); D->PublishTeamShop(); D->Publish(); }
	static void Service(UDefenseDirectorComponent* D) { D->ServicePlayers(D->GetWorld()->GetTimeSeconds()); D->PublishTeamShop(); }
	static void Finish(UDefenseDirectorComponent* D) { D->Finish(false,TEXT("TestFinished")); }
	static void Refresh(UDefenseDirectorComponent* D) { D->ExpireTeamShop(); D->PublishTeamShop(); D->Publish(); }
	static FCombatModifierHandle FirstHandle(UDefenseDirectorComponent* D,FPrimaryAssetId Id)
	{
		const auto* Record=D->TeamPurchaseLedger.Find(Id);
		return Record&&!Record->AppliedModifiers.IsEmpty()?Record->AppliedModifiers[0].Handle:FCombatModifierHandle();
	}
	static int32 AidLeft(UDefenseDirectorComponent* D,FPrimaryAssetId Id) { const auto* R=D->TeamPurchaseLedger.Find(Id); return R?R->RemainingReinforcements:0; }
	static double RespawnAt(UDefenseDirectorComponent* D,ACombatPlayerController* P) { return D->Players.FindChecked(P).RespawnAt; }
	static void DiedAgain(UDefenseDirectorComponent* D,ACombatUnitCharacter* H) { D->OnHeroDied(H,FCombatEventContext()); }
	static void RespawnedAgain(UDefenseDirectorComponent* D,ACombatUnitCharacter* H) { D->OnHeroRespawned(H,FCombatEventContext()); }
	static int32 Ledger(UDefenseDirectorComponent* D) { return D->TeamPurchaseLedger.Num(); }
	static int32 Cache(UDefenseDirectorComponent* D,ACombatPlayerController* PC) { return PC->GetCombatEconomyComponent()->GetAcquireReceiptCount(); }
};
namespace DefenseTeamShopTests
{
	constexpr auto Flags=EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter;
	/** 两名固定英雄与无被动收入的独立权威 World，作用域退出执行真实 teardown。 */
	struct FFixture
	{
		FCombatAutomationWorldFixture World;
		AActor* Owner;
		UDefenseRunData* Run;
		UCombatShopData* Shop;
		TArray<UCombatItemData*> Items;
		TArray<UDefenseTeamItemEffect*> Effects;
		ADefenseObjective* Core;
		ADefenseGameState* State;
		UDefenseDirectorComponent* Director;
		TArray<ADefensePlayerController*> Players;
		FFixture()
		{
			auto* W=World.GetWorld(); Owner=W->SpawnActor<AActor>(); Run=UDefenseRunData::CreatePhase1Default(Owner); Run->PreparationDuration=60;
			Shop=NewObject<UCombatShopData>(Run); Shop->DefinitionName=TEXT("team_shop_fixture");
			auto& Category=Shop->Categories.AddDefaulted_GetRef(); Category.CategoryId=TEXT("team"); Category.DisplayName=FText::FromString(TEXT("团队"));
			for(int32 I=0;I<6;++I)
			{
				auto* Item=NewObject<UCombatItemData>(Shop); Item->DefinitionName=FName(FString::Printf(TEXT("team_item_%d"),I));
				Item->DisplayNameText=FText::FromString(TEXT("团队物品")); Item->Description=FText::FromString(TEXT("下一波护甲 +4")); Item->PurchasePrice=100; Item->bPurchasable=true; Item->bUseOnAcquire=true;
				auto* Effect=NewObject<UDefenseTeamItemEffect>(Item); Item->AcquireEffect=Effect;
				Effect->AllowedPhases={EDefensePhase::Preparation,EDefensePhase::Intermission,EDefensePhase::BossPreparation};
				Category.Items.Add(Item); Items.Add(Item); Effects.Add(Effect);
				if(I==5) { Effect->ObjectiveHeal=750; continue; }
				auto* M=NewObject<UCombatModifierData>(Effect); M->DefinitionName=FName(FString::Printf(TEXT("team_effect_%d"),I));
				M->bRemoveOnDeath=false; M->DispelRule=ECombatModifierDispelRule::NotDispellable;
				FCombatModifierAttributeChange Change; Change.Attribute=UCombatAttributeSet::GetArmorAttribute(); Change.Magnitude=4;
				M->AttributeChanges.Add(Change); Effect->ModifierData=M; Effect->DurationWaves=1;
			}
			Core=W->SpawnActor<ADefenseObjective>(); Core->InitializeFromUnitData(Run->ObjectiveData);
			if(!Core->HasActorBegunPlay()) Core->DispatchBeginPlay(); State=W->SpawnActor<ADefenseGameState>();
			Director=NewObject<UDefenseDirectorComponent>(Owner); Owner->AddInstanceComponent(Director); Director->RegisterComponent();
			if(!Owner->HasActorBegunPlay()) Owner->DispatchBeginPlay();
		}
		/** 把一项属性原型换为正式核心屏障，所有数值仍通过获得效果配置。 */
		void ConfigureShield()
		{
			auto* E=Effects[0]; E->ModifierData=nullptr; E->ObjectiveShieldFraction=0.08f; E->DurationWaves=1;
			auto* M=NewObject<UCombatModifierData>(E); M->DefinitionName=TEXT("core_barrier_effect");
			M->RuntimeClass=UDefenseCoreShieldRuntime::StaticClass(); M->DispelRule=ECombatModifierDispelRule::NotDispellable;
			E->CoreShieldData=M; Items[0]->PurchasePrice=220;
		}
		/** 配置三件正式Phase2条件团队效果；测试仍从原经济入口购买。 */
		void ConfigureFormal()
		{
			for(int32 I=0;I<3;++I) { Effects[I]->ModifierData=nullptr; Effects[I]->DurationWaves=1; Effects[I]->ConditionalTargetRole=EDefenseTeamTargetRole::None; Effects[I]->ConditionalDamageFraction=0; }
			auto MakeConditional=[&](int32 Index,FName Name,EDefenseTeamTargetRole Role,float Fraction)
			{
				auto* M=NewObject<UCombatModifierData>(Effects[Index]); M->DefinitionName=Name; M->RuntimeClass=UDefenseConditionalDamageRuntime::StaticClass();
				M->RuntimeParameters.Add(TEXT("target_role"),float(Role)); M->RuntimeParameters.Add(TEXT("damage_fraction"),Fraction); M->bRemoveOnDeath=false; M->DispelRule=ECombatModifierDispelRule::NotDispellable;
				Effects[Index]->ModifierData=M; Effects[Index]->ConditionalTargetRole=Role; Effects[Index]->ConditionalDamageFraction=Fraction; Items[Index]->PurchasePrice=Role==EDefenseTeamTargetRole::SiegeOrAssassin?260:280;
			};
			MakeConditional(0,TEXT("hunt_horn_effect"),EDefenseTeamTargetRole::SiegeOrAssassin,.15f);
			auto* Agile=NewObject<UCombatModifierData>(Effects[1]); Agile->DefinitionName=TEXT("agile_circuit_effect"); Agile->bRemoveOnDeath=false; Agile->DispelRule=ECombatModifierDispelRule::NotDispellable; FCombatModifierAttributeChange C; C.Attribute=UCombatAttributeSet::GetCooldownReductionPctAttribute(); C.Magnitude=.05f; Agile->AttributeChanges.Add(C); Effects[1]->ModifierData=Agile; Items[1]->PurchasePrice=240;
			MakeConditional(2,TEXT("breach_inscription_effect"),EDefenseTeamTargetRole::Boss,.12f);
		}
		/** 通过正式获得效果配置援军，测试仍从原经济入口购买。 */
		void ConfigureReinforcement()
		{
			auto* E=Effects[0]; E->ModifierData=nullptr; E->DurationWaves=0;
			E->ReinforcementDeaths=2; E->RespawnReduction=3; E->RespawnShieldFraction=.2f;
			auto* M=NewObject<UCombatModifierData>(E); M->DefinitionName=TEXT("reinforcement_test_shield");
			M->RuntimeClass=UDefenseShieldRuntime::StaticClass(); M->Duration=4; M->DispelRule=ECombatModifierDispelRule::NotDispellable;
			E->ReinforcementShieldData=M; Items[0]->PurchasePrice=200;
		}
		/** 初始化经济及控制绑定，任何失败都带诊断，禁止仅返回 false 隐藏夹具问题。 */
		bool Start(FAutomationTestBase& Test)
		{
			if(!Test.TestTrue(TEXT("Initialize defense run"),Director->InitializeForRun(Run,Core,State))) return false;
			if(!Test.TestTrue(TEXT("Initialize shared catalog"),Director->InitializeTeamItems(Shop))) { FString Diag; Shop->ValidateRuntime(Diag); Test.AddError(Diag); return false; }
			World.GetWorld()->GetSubsystem<UCombatNetworkSecuritySubsystem>()->BurstCapacity=1000;
			for(int32 I=0;I<2;++I)
			{
				auto* PC=World.GetWorld()->SpawnActor<ADefensePlayerController>();
				FActorSpawnParameters Spawn; Spawn.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
				auto* Hero=World.GetWorld()->SpawnActor<ACombatUnitCharacter>(FVector(1000,300*I,100),FRotator::ZeroRotator,Spawn);
				auto* Data=NewObject<UCombatUnitData>(Hero); Data->DefinitionName=FName(FString::Printf(TEXT("team_hero_%d"),I));
				if(!Test.TestTrue(TEXT("Initialize hero"),Hero->InitializeFromUnitData(Data))) return false;
				if(!Hero->HasActorBegunPlay()) Hero->DispatchBeginPlay();
				if(!Test.TestTrue(TEXT("Bind hero"),PC->SetCommandedUnitAuthority(Hero))) return false;
				auto* Rules=NewObject<UCombatEconomyData>(PC); Rules->DefinitionName=TEXT("team_rules"); Rules->StartingGold=500; Rules->PassiveGoldPerMinute=0;
				FString Error; if(!Test.TestTrue(TEXT("Initialize economy"),PC->GetCombatEconomyComponent()->InitializeForMatch(Rules,Shop,false,Error))) { Test.AddError(Error); return false; }
				if(!Test.TestTrue(TEXT("Register player"),Director->RegisterPlayer(PC,Hero))) return false;
				Players.Add(PC);
			}
			return true;
		}
		/** 从当前服务器视图建立完整意图；每次测试自行指定 RequestId 和玩家。 */
		FCombatEconomyRequest Request(int32 Id=1,int32 ItemIndex=0,int32 PlayerIndex=0) const
		{
			const auto* PC=Players[PlayerIndex]; auto* Economy=PC->GetCombatEconomyComponent(); FCombatEconomyRequest R;
			R.RequestId=Id; R.ItemDefinitionId=Items[ItemIndex]->GetPrimaryAssetId(); R.CommandBindingGeneration=PC->GetCommandBindingGeneration();
			R.ExpectedEconomyRevision=Economy->GetEconomyRevision(); R.ExpectedInventoryRevision=Economy->GetInventoryRevision();
			if(const auto* Offer=Economy->FindShopOffer(R.ItemDefinitionId)) { R.PurchaseContextId=Offer->ContextId; R.PurchaseContextRevision=Offer->Revision; } return R;
		}
		/** 所有购买测试经过原 Controller 经济信封入口。 */
		FCombatEconomyResult Buy(ACombatPlayerController* PC,const FCombatEconomyRequest& R) { return PC->ProcessEconomyRequestForConnection(PC,R); }
		/** 生命周期测试使用公共伤害结算，保留真实死亡代次和 Modifier 保留规则。 */
		void Damage(ACombatUnitCharacter* Hero,float Amount)
		{
			FCombatDamageRequest R; R.Source=Hero; R.Target=Hero; R.Amount=Amount; R.DamageType=ECombatDamageType::Pure;
			World.GetWorld()->GetSubsystem<UCombatDamageSubsystem>()->DealDamage(R);
		}
	};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDefenseTeamShopDataTest,"Combat.Defense.TeamShop.DataValidation",DefenseTeamShopTests::Flags)
bool FDefenseTeamShopDataTest::RunTest(const FString&)
{
	DefenseTeamShopTests::FFixture F; FString Error; TestTrue(TEXT("Six real item definitions valid"),F.Shop->ValidateRuntime(Error));
	F.Items[0]->PurchasePrice=-1; TestFalse(TEXT("Negative price"),F.Shop->ValidateRuntime(Error)); F.Items[0]->PurchasePrice=100;
	F.Effects[1]->ModifierData=F.Effects[0]->ModifierData;
	TestTrue(TEXT("Run for duplicate effect check"),F.Director->InitializeForRun(F.Run,F.Core,F.State));
	TestFalse(TEXT("Shared team effect rejected"),F.Director->InitializeTeamItems(F.Shop));
	DefenseTeamShopTests::FFixture G; G.Effects[0]->ModifierData->AttributeChanges[0].Magnitude=NAN;
	TestFalse(TEXT("NaN magnitude"),G.Shop->ValidateRuntime(Error)); G.Effects[0]->ModifierData->AttributeChanges[0].Magnitude=4;
	G.Effects[0]->ModifierData->AttributeChanges[0].Attribute=UCombatAttributeSet::GetIncomingDamageAttribute();
	TestFalse(TEXT("Meta damage disallowed"),G.Shop->ValidateRuntime(Error));
	DefenseTeamShopTests::FFixture H; H.Effects[5]->AllowedPhases={EDefensePhase::WaveCombat};
	TestFalse(TEXT("Combat-only effect invalid"),H.Shop->ValidateRuntime(Error)); return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDefenseTeamShopLedgerTest,"Combat.Defense.TeamShop.PurchaseLedgerIdempotency",DefenseTeamShopTests::Flags)
bool FDefenseTeamShopLedgerTest::RunTest(const FString&)
{
	DefenseTeamShopTests::FFixture F; if(!F.Start(*this)) return false; auto* PC=F.Players[0]; auto* Hero=PC->GetCommandedUnit(); const float Armor=Hero->GetCombatAttributeSet()->GetArmor();
	const auto R=F.Request(41); const auto OtherRequest=F.Request(1,0,1);
	const auto First=F.Buy(PC,R); TestTrue(TEXT("First accepted"),First.bSuccess);
	const auto Repeat=F.Buy(PC,R); TestTrue(TEXT("Same complete payload returns success"),Repeat.bSuccess); TestEqual(TEXT("Original revision"),Repeat.EconomyRevision,First.EconomyRevision);
	auto Changed=R; Changed.ItemDefinitionId=F.Items[1]->GetPrimaryAssetId(); TestEqual(TEXT("Changed payload rejected"),F.Buy(PC,Changed).FailureTag,CombatTags::Failure_Network_DuplicateRequest.GetTag());
	TestFalse(TEXT("Concurrent stale purchase rejected"),F.Buy(F.Players[1],OtherRequest).bSuccess);
	TestFalse(TEXT("Fresh other payer cannot repurchase"),F.Buy(F.Players[1],F.Request(2,0,1)).bSuccess);
	TestEqual(TEXT("Charged once"),PC->GetCombatEconomyComponent()->GetGold(),int64(400)); TestEqual(TEXT("Other payer unchanged"),F.Players[1]->GetCombatEconomyComponent()->GetGold(),int64(500));
	TestEqual(TEXT("One effect"),Hero->GetCombatAttributeSet()->GetArmor(),Armor+4); return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDefenseTeamShopPhaseTest,"Combat.Defense.TeamShop.PhaseAndCrossWorldReject",DefenseTeamShopTests::Flags)
bool FDefenseTeamShopPhaseTest::RunTest(const FString&)
{
	DefenseTeamShopTests::FFixture F,G; if(!F.Start(*this)||!G.Start(*this)) return false; auto* PC=F.Players[0];
	TestFalse(TEXT("Cross world rejected"),F.Players[0]->ProcessEconomyRequestForConnection(G.Players[0],G.Request()).bSuccess);
	auto R=F.Request(1); R.PurchaseContextId=FGuid::NewGuid(); TestFalse(TEXT("Stale run"),F.Buy(PC,R).bSuccess);
	R=F.Request(2); ++R.CommandBindingGeneration; TestFalse(TEXT("Stale binding"),F.Buy(PC,R).bSuccess);
	FDefenseTeamShopTestAccess::Phase(F.Director,EDefensePhase::WaveCombat,0); TestFalse(TEXT("Combat phase"),F.Buy(PC,F.Request(3)).bSuccess);
	TestFalse(TEXT("Owner snapshot closed"),PC->GetCombatEconomyComponent()->FindShopOffer(F.Items[0]->GetPrimaryAssetId())->bCanPurchase);
	FDefenseTeamShopTestAccess::Phase(F.Director,EDefensePhase::Intermission,0); R=F.Request(4); --R.ExpectedEconomyRevision; TestFalse(TEXT("Stale economy"),F.Buy(PC,R).bSuccess);
	FDefenseTeamShopTestAccess::Finish(F.Director); TestFalse(TEXT("Terminal"),F.Buy(PC,F.Request(5)).bSuccess);
	TestEqual(TEXT("All rejects unchanged gold"),PC->GetCombatEconomyComponent()->GetGold(),int64(500)); return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDefenseTeamShopExpiryTest,"Combat.Defense.TeamShop.ExpiryAndTeardown",DefenseTeamShopTests::Flags)
bool FDefenseTeamShopExpiryTest::RunTest(const FString&)
{
	DefenseTeamShopTests::FFixture F; if(!F.Start(*this)) return false; auto* Hero=F.Players[0]->GetCommandedUnit(); const float Armor=Hero->GetCombatAttributeSet()->GetArmor();
	TestTrue(TEXT("Buy preparation buff"),F.Buy(F.Players[0],F.Request()).bSuccess);
	FDefenseTeamShopTestAccess::Phase(F.Director,EDefensePhase::WaveCombat,0); TestEqual(TEXT("Buff throughout first wave"),Hero->GetCombatAttributeSet()->GetArmor(),Armor+4);
	FDefenseTeamShopTestAccess::Phase(F.Director,EDefensePhase::Intermission,0); TestEqual(TEXT("Expires at first clear"),Hero->GetCombatAttributeSet()->GetArmor(),Armor);
	TestTrue(TEXT("Expired stays purchased"),F.Players[0]->GetCombatEconomyComponent()->FindShopOffer(F.Items[0]->GetPrimaryAssetId())->RemainingPurchases==0);
	TestTrue(TEXT("Buy new wave buff"),F.Buy(F.Players[0],F.Request(2,1)).bSuccess);
	FDefenseTeamShopTestAccess::Phase(F.Director,EDefensePhase::WaveCombat,1); TestEqual(TEXT("New buff survives second wave"),Hero->GetCombatAttributeSet()->GetArmor(),Armor+4);
	FDefenseTeamShopTestAccess::Finish(F.Director); TestEqual(TEXT("Terminal revokes attribute"),Hero->GetCombatAttributeSet()->GetArmor(),Armor);
	TestTrue(TEXT("Terminal retains purchase record"),F.Players[0]->GetCombatEconomyComponent()->FindShopOffer(F.Items[1]->GetPrimaryAssetId())->RemainingPurchases==0);
	F.Director->Shutdown(); F.Director->Shutdown(); TestTrue(TEXT("Shutdown clears owner snapshot"),!F.Players[0]->GetCombatEconomyComponent()->FindShopOffer(F.Items[0]->GetPrimaryAssetId())->bCanPurchase); return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDefenseTeamShopDeathTest,"Combat.Defense.TeamShop.DeathDisconnectAndFrozenEffects",DefenseTeamShopTests::Flags)
bool FDefenseTeamShopDeathTest::RunTest(const FString&)
{
	DefenseTeamShopTests::FFixture F; if(!F.Start(*this)) return false; auto* Hero=F.Players[1]->GetCommandedUnit(); const float Armor=Hero->GetCombatAttributeSet()->GetArmor();
	F.Effects[0]->ModifierData->AttributeChanges[0].Magnitude=999; F.Items[0]->PurchasePrice=9999;
	F.Damage(Hero,100000); TestTrue(TEXT("Buy while teammate dead"),F.Buy(F.Players[0],F.Request()).bSuccess);
	TestTrue(TEXT("Respawn via lifecycle"),Hero->GetCombatLifecycleComponent()->RespawnAtLocation(FVector(1000,300,100)));
	FDefenseTeamShopTestAccess::Service(F.Director); TestEqual(TEXT("Dead teammate gets frozen effect"),Hero->GetCombatAttributeSet()->GetArmor(),Armor+4);
	F.Director->RemovePlayer(F.Players[0]); TestEqual(TEXT("Buyer disconnect preserves teammate effect"),Hero->GetCombatAttributeSet()->GetArmor(),Armor+4);
	F.Owner->Destroy(); TestEqual(TEXT("Owner EndPlay removes exact effect"),Hero->GetCombatAttributeSet()->GetArmor(),Armor); return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDefenseTeamShopCacheTest,"Combat.Defense.TeamShop.CacheBoundAndReentrancy",DefenseTeamShopTests::Flags)
bool FDefenseTeamShopCacheTest::RunTest(const FString&)
{
	DefenseTeamShopTests::FFixture F; if(!F.Start(*this)) return false; auto* PC=F.Players[0]; const auto Original=F.Request();
	bool Reentered=false; bool NestedAccepted=false;
	const auto Handle=PC->GetCommandedUnit()->GetCombatModifierComponent()->OnModifierCollectionChanged().AddLambda([&]
	{
		if(Reentered) return; Reentered=true; NestedAccepted=F.Buy(PC,F.Request(2,1)).bSuccess;
	});
	TestTrue(TEXT("Outer accepted"),F.Buy(PC,Original).bSuccess);
	PC->GetCommandedUnit()->GetCombatModifierComponent()->OnModifierCollectionChanged().Remove(Handle);
	TestTrue(TEXT("Effect observer ran"),Reentered); TestFalse(TEXT("Nested purchase rejected"),NestedAccepted);
	for(int32 I=3;I<150;++I) { auto R=F.Request(I); R.ExpectedEconomyRevision=1; F.Buy(PC,R); }
	TestEqual(TEXT("Cache capped"),FDefenseTeamShopTestAccess::Cache(F.Director,PC),32);
	TestFalse(TEXT("Evicted old ID cannot execute"),F.Buy(PC,Original).bSuccess);
	TestEqual(TEXT("Only outer charged"),PC->GetCombatEconomyComponent()->GetGold(),int64(400)); return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDefenseTeamShopHealTest,"Combat.Defense.TeamShop.RepairUsesPublicHeal",DefenseTeamShopTests::Flags)
bool FDefenseTeamShopHealTest::RunTest(const FString&)
{
	DefenseTeamShopTests::FFixture F; if(!F.Start(*this)) return false; F.Damage(F.Core,300);
	TestTrue(TEXT("Repair accepted"),F.Buy(F.Players[0],F.Request(1,5)).bSuccess);
	TestEqual(TEXT("Heal capped at maximum"),F.Core->GetCombatAttributeSet()->GetHealth(),F.Core->GetCombatAttributeSet()->GetMaxHealth());
	TestEqual(TEXT("Repair gives no armor"),F.Players[0]->GetCommandedUnit()->GetCombatAttributeSet()->GetArmor(),F.Players[1]->GetCommandedUnit()->GetCombatAttributeSet()->GetArmor());
	DefenseTeamShopTests::FFixture G; if(!G.Start(*this)) return false; G.Damage(G.Core,300);
	auto* Events=G.World.GetWorld()->GetSubsystem<UCombatEventSubsystem>(); bool Ended=false;
	const auto Delegate=Events->OnRecord().AddLambda([&](const FCombatLogRecord& Record)
	{ if(!Ended&&Record.EventType==CombatTags::Event_Combat_HealApplied) { Ended=true; G.Director->Shutdown(); } });
	TestTrue(TEXT("Committed heal remains consumed even when observer ends run"),G.Buy(G.Players[0],G.Request(1,5)).bSuccess);
	Events->OnRecord().Remove(Delegate);
	TestTrue(TEXT("Heal observer ended run"),Ended); TestEqual(TEXT("No ledger resurrected after shutdown"),FDefenseTeamShopTestAccess::Ledger(G.Director),0); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDefenseTeamShopHandleTest,"Combat.Defense.TeamShop.ExactHandlesAndRollback",DefenseTeamShopTests::Flags)
bool FDefenseTeamShopHandleTest::RunTest(const FString&)
{
	DefenseTeamShopTests::FFixture F; if(!F.Start(*this)) return false;
	auto* Hero=F.Players[1]->GetCommandedUnit(); const float Armor=Hero->GetCombatAttributeSet()->GetArmor();
	auto* Personal=NewObject<UCombatModifierData>(Hero); Personal->DefinitionName=TEXT("personal_armor"); Personal->bRemoveOnDeath=false;
	FCombatModifierAttributeChange Change; Change.Attribute=UCombatAttributeSet::GetArmorAttribute(); Change.Magnitude=7; Personal->AttributeChanges.Add(Change);
	FCombatModifierApplyRequest Apply; Apply.ModifierData=Personal; Apply.Source=Hero; const auto Own=Hero->GetCombatModifierComponent()->ApplyModifier(Apply);
	TestTrue(TEXT("Personal effect applied first"),Own.bSuccess);
	TestTrue(TEXT("Purchase adds a distinct handle on each hero"),F.Buy(F.Players[0],F.Request()).bSuccess);
	TestEqual(TEXT("Both effects stack"),Hero->GetCombatAttributeSet()->GetArmor(),Armor+11);
	FDefenseTeamShopTestAccess::Phase(F.Director,EDefensePhase::Intermission,0);
	TestEqual(TEXT("Expiry preserves teammate personal effect with matching numeric handle"),Hero->GetCombatAttributeSet()->GetArmor(),Armor+7);
	TestNotNull(TEXT("Personal handle remains active"),Hero->GetCombatModifierComponent()->FindRuntime(Own.Handle));
	// 从实际效果通知中关闭导演，检验提交失败撤销部分效果且归还本次扣费。
	bool Shutdown=false; const int64 Gold=F.Players[0]->GetCombatEconomyComponent()->GetGold();
	const auto Delegate=F.Players[0]->GetCommandedUnit()->GetCombatModifierComponent()->OnModifierCollectionChanged().AddLambda([&]
	{ if(!Shutdown) { Shutdown=true; F.Director->Shutdown(); } });
	TestFalse(TEXT("Teardown during apply fails purchase"),F.Buy(F.Players[0],F.Request(2,1)).bSuccess);
	F.Players[0]->GetCommandedUnit()->GetCombatModifierComponent()->OnModifierCollectionChanged().Remove(Delegate);
	TestEqual(TEXT("Failed apply refunds gold"),F.Players[0]->GetCombatEconomyComponent()->GetGold(),Gold);
	TestEqual(TEXT("Rollback preserves other effect"),Hero->GetCombatAttributeSet()->GetArmor(),Armor+7); return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDefenseTeamShopSecurityTest,"Combat.Defense.TeamShop.OwnerBudgetAndClientReject",DefenseTeamShopTests::Flags)
bool FDefenseTeamShopSecurityTest::RunTest(const FString&)
{
	DefenseTeamShopTests::FFixture F; if(!F.Start(*this)) return false;
	auto* Security=F.World.GetWorld()->GetSubsystem<UCombatNetworkSecuritySubsystem>(); auto* C=F.Players[0]->GetCombatEconomyComponent(); FGameplayTag Failure;
	Security->BurstCapacity=2;
	TestFalse(TEXT("Wrong connection"),Security->ValidateAndConsumeComponentRequest(F.Players[1],C,1,false,Failure));
	TestTrue(TEXT("Owner first request"),Security->ValidateAndConsumeComponentRequest(F.Players[0],C,1,false,Failure));
	TestTrue(TEXT("Replay shares rate budget"),Security->ValidateAndConsumeComponentRequest(F.Players[0],C,1,true,Failure));
	TestFalse(TEXT("Replay flood denied"),Security->ValidateAndConsumeComponentRequest(F.Players[0],C,1,true,Failure));
	FCombatAutomationWorldFixture Client(NM_Client); auto* Owner=Client.GetWorld()->SpawnActor<AActor>();
	auto* D=NewObject<UDefenseDirectorComponent>(Owner); Owner->AddInstanceComponent(D); D->RegisterComponent();
	TestFalse(TEXT("Client world cannot buy"),D->InitializeForRun(F.Run,F.Core,F.State));
	F.Director->Shutdown(); TestFalse(TEXT("Shutdown closes shared offer"),C->FindShopOffer(F.Items[0]->GetPrimaryAssetId())->bCanPurchase); return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDefenseTeamShopGCTest,"Combat.Defense.TeamShop.FrozenDefinitionsSurviveGC",DefenseTeamShopTests::Flags)
bool FDefenseTeamShopGCTest::RunTest(const FString&)
{
	DefenseTeamShopTests::FFixture F; if(!F.Start(*this)) return false; const auto R=F.Request();
	CollectGarbage(RF_NoFlags);
	TestTrue(TEXT("Frozen catalog retained by Director"),F.Buy(F.Players[0],R).bSuccess);
	TestEqual(TEXT("Frozen effect still usable"),F.Players[0]->GetCommandedUnit()->GetCombatAttributeSet()->GetArmor(),4.0f); return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDefenseCoreShieldTest,"Combat.Defense.CoreRescue.ShieldAbsorption",DefenseTeamShopTests::Flags)
bool FDefenseCoreShieldTest::RunTest(const FString&)
{
	DefenseTeamShopTests::FFixture F;
	auto* M=NewObject<UCombatModifierData>(F.Core); M->DefinitionName=TEXT("core_shield_test"); M->RuntimeClass=UDefenseCoreShieldRuntime::StaticClass();
	FCombatModifierApplyRequest A; A.ModifierData=M; A.Source=F.Core; A.RuntimeParameterOverrides.Add(TEXT("shield_amount"),400);
	const auto Applied=F.Core->GetCombatModifierComponent()->ApplyModifier(A); TestTrue(TEXT("Public shield apply"),Applied.bSuccess);
	const float Before=F.Core->GetCombatAttributeSet()->GetHealth();
	FCombatDamageRequest D; D.Source=F.Core; D.Target=F.Core; D.Amount=100;
	float Remaining=400;
	for(const auto Type:{ECombatDamageType::Physical,ECombatDamageType::Magical,ECombatDamageType::Pure})
	{
		D.DamageType=Type; const auto Hit=F.World.GetWorld()->GetSubsystem<UCombatDamageSubsystem>()->DealDamage(D);
		TestTrue(TEXT("Public damage accepted"),Hit.bSuccess);
		TestEqual(TEXT("Shield absorbs post-mitigation damage"),Hit.Event.AbsorbedAmount,100-Hit.Event.MitigatedAmount);
		TestEqual(TEXT("Shield prevents health damage"),F.Core->GetCombatAttributeSet()->GetHealth(),Before);
		Remaining-=Hit.Event.AbsorbedAmount;
	}
	auto* Shield=Cast<UDefenseCoreShieldRuntime>(F.Core->GetCombatModifierComponent()->FindRuntime(Applied.Handle));
	if(!TestNotNull(TEXT("Partially consumed shield remains"),Shield)) return false;
	TestEqual(TEXT("Shield remaining correct"),Shield->GetRemainingShield(),Remaining);
	D.Amount=0; F.World.GetWorld()->GetSubsystem<UCombatDamageSubsystem>()->DealDamage(D);
	TestEqual(TEXT("Zero damage preserves shield"),Shield->GetRemainingShield(),Remaining);
	D.Amount=20; D.DamageType=ECombatDamageType::Magical;
	auto* Asc=F.Core->GetCombatAbilitySystemComponent(); Asc->AddLooseGameplayTag(CombatTags::State_MagicImmune);
	TestTrue(TEXT("Immunity blocks before shield"),F.World.GetWorld()->GetSubsystem<UCombatDamageSubsystem>()->DealDamage(D).bBlocked);
	TestEqual(TEXT("Immunity preserves shield"),Shield->GetRemainingShield(),Remaining);
	D.Flags.AddTag(CombatTags::Damage_Flag_HPLoss);
	const auto Loss=F.World.GetWorld()->GetSubsystem<UCombatDamageSubsystem>()->DealDamage(D);
	TestEqual(TEXT("HPLoss bypasses shield"),Loss.Event.AppliedAmount,20.0f); TestEqual(TEXT("HPLoss preserves shield"),Shield->GetRemainingShield(),Remaining);
	Asc->RemoveLooseGameplayTag(CombatTags::State_MagicImmune); D.Flags.Reset(); D.DamageType=ECombatDamageType::Pure; D.Amount=Remaining+50;
	const auto Depleted=F.World.GetWorld()->GetSubsystem<UCombatDamageSubsystem>()->DealDamage(D);
	TestEqual(TEXT("Overflow reaches health"),Depleted.Event.AppliedAmount,50.0f);
	TestNull(TEXT("Exhausted runtime removed"),F.Core->GetCombatModifierComponent()->FindRuntime(Applied.Handle));
	TestFalse(TEXT("Old shield handle cannot remove twice"),F.Core->GetCombatModifierComponent()->RemoveModifier(Applied.Handle)); return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDefenseCoreRepairTest,"Combat.Defense.CoreRescue.PercentRepair",DefenseTeamShopTests::Flags)
bool FDefenseCoreRepairTest::RunTest(const FString&)
{
	DefenseTeamShopTests::FFixture F; F.Effects[5]->ObjectiveHeal=0; F.Effects[5]->ObjectiveHealFraction=0.15f; F.Items[5]->PurchasePrice=180;
	if(!F.Start(*this)) return false;
	CombatEffectUtilities::ApplyAttributeAdditive(F.Core,*F.Core->GetCombatAbilitySystemComponent(),UCombatAttributeSet::GetMaxHealthAttribute(),1000);
	F.Damage(F.Core,1500); const float Before=F.Core->GetCombatAttributeSet()->GetHealth();
	const auto R=F.Request(1,5); TestTrue(TEXT("Percent repair purchased"),F.Buy(F.Players[0],R).bSuccess);
	TestEqual(TEXT("Current max health6000 repairs900"),F.Core->GetCombatAttributeSet()->GetHealth(),Before+900);
	TestTrue(TEXT("Same request replay returns receipt"),F.Buy(F.Players[0],R).bSuccess);
	TestEqual(TEXT("Replay does not heal twice"),F.Core->GetCombatAttributeSet()->GetHealth(),Before+900);
	TestEqual(TEXT("Charged180 once"),F.Players[0]->GetCombatEconomyComponent()->GetGold(),int64(320));
	DefenseTeamShopTests::FFixture G; G.Effects[5]->ObjectiveHeal=0; G.Effects[5]->ObjectiveHealFraction=0.15f;
	if(!G.Start(*this)) return false;
	TestTrue(TEXT("Full health repair still consumed"),G.Buy(G.Players[0],G.Request(1,5)).bSuccess);
	TestEqual(TEXT("Full health remains capped"),G.Core->GetCombatAttributeSet()->GetHealth(),5000.0f); return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDefenseCoreRescueValidationTest,"Combat.Defense.CoreRescue.ConfigurationBounds",DefenseTeamShopTests::Flags)
bool FDefenseCoreRescueValidationTest::RunTest(const FString&)
{
	DefenseTeamShopTests::FFixture F; F.ConfigureShield(); FString Error;
	TestTrue(TEXT("Shield definition valid"),F.Shop->ValidateRuntime(Error));
	auto* E=F.Effects[0]; auto* M=E->CoreShieldData.Get();
	for(float Value:{-0.1f,1.01f,NAN,INFINITY}) { E->ObjectiveShieldFraction=Value; TestFalse(TEXT("Invalid fraction rejected"),E->ValidateDefinition(Error)); }
	E->ObjectiveShieldFraction=.08f; E->ObjectiveHealFraction=.15f;
	TestFalse(TEXT("Shield and heal cannot partly commit together"),E->ValidateDefinition(Error)); E->ObjectiveHealFraction=0;
	E->DurationWaves=2; TestFalse(TEXT("Core shield only one wave"),E->ValidateDefinition(Error)); E->DurationWaves=1;
	M->bRemoveOnDeath=false; TestFalse(TEXT("Core shield cannot survive core death"),E->ValidateDefinition(Error)); M->bRemoveOnDeath=true;
	M->RuntimeClass=UCombatMagicShieldRuntime::StaticClass(); TestFalse(TEXT("Magic-only runtime cannot impersonate barrier"),E->ValidateDefinition(Error)); M->RuntimeClass=UDefenseCoreShieldRuntime::StaticClass();
	M->RuntimeParameters.Add(TEXT("shield_amount"),1000); TestFalse(TEXT("Amount cannot override percentage"),E->ValidateDefinition(Error)); M->RuntimeParameters.Reset();
	F.Effects[5]->ObjectiveHeal=0; F.Effects[5]->ObjectiveHealFraction=NAN; TestFalse(TEXT("Nonfinite heal rejected"),F.Effects[5]->ValidateDefinition(Error));
	F.Effects[5]->ObjectiveHealFraction=.15f; TestTrue(TEXT("Percent heal valid"),F.Shop->ValidateRuntime(Error));
	F.Effects[1]->ModifierData=nullptr; F.Effects[1]->CoreShieldData=DuplicateObject<UCombatModifierData>(M,F.Effects[1]);
	F.Effects[1]->CoreShieldData->DefinitionName=TEXT("second_core_barrier"); F.Effects[1]->ObjectiveShieldFraction=.08f;
	TestTrue(TEXT("Initialize run"),F.Director->InitializeForRun(F.Run,F.Core,F.State));
	TestFalse(TEXT("Two shield products cannot stack"),F.Director->InitializeTeamItems(F.Shop)); return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDefenseCoreShieldPurchaseTest,"Combat.Defense.CoreRescue.PurchaseSnapshotAndExpiry",DefenseTeamShopTests::Flags)
bool FDefenseCoreShieldPurchaseTest::RunTest(const FString&)
{
	DefenseTeamShopTests::FFixture F; F.ConfigureShield(); if(!F.Start(*this)) return false;
	const auto Request=F.Request(); const auto Competitor=F.Request(1,0,1);
	F.Effects[0]->ObjectiveShieldFraction=.5f; F.Effects[0]->CoreShieldData->RuntimeClass=nullptr;
	TestTrue(TEXT("Frozen shield purchased"),F.Buy(F.Players[0],Request).bSuccess);
	TestTrue(TEXT("Replay succeeds"),F.Buy(F.Players[0],Request).bSuccess);
	TestFalse(TEXT("Competing buyer rejected"),F.Buy(F.Players[1],Competitor).bSuccess);
	TestEqual(TEXT("Exact220 charge"),F.Players[0]->GetCombatEconomyComponent()->GetGold(),int64(280));
	TestEqual(TEXT("Core shield projected400"),F.State->GetRunView().ObjectiveShield,400.0f);
	TestEqual(TEXT("Independent view schema with event projection"),F.State->GetRunView().SchemaVersion,4);
	const auto Handle=FDefenseTeamShopTestAccess::FirstHandle(F.Director,F.Items[0]->GetPrimaryAssetId());
	for(auto* PC:F.Players) TestEqual(TEXT("No shield on heroes"),PC->GetCommandedUnit()->GetCombatModifierComponent()->GetActiveModifierCount(),0);
	CombatEffectUtilities::ApplyAttributeAdditive(F.Core,*F.Core->GetCombatAbilitySystemComponent(),UCombatAttributeSet::GetMaxHealthAttribute(),1000);
	FDefenseTeamShopTestAccess::Refresh(F.Director); TestEqual(TEXT("Max health growth does not replenish snapshot"),F.State->GetRunView().ObjectiveShield,400.0f);
	F.Director->RemovePlayer(F.Players[0]); TestNotNull(TEXT("Buyer disconnect preserves core shield"),F.Core->GetCombatModifierComponent()->FindRuntime(Handle));
	FDefenseTeamShopTestAccess::Phase(F.Director,EDefensePhase::WaveCombat,0); TestEqual(TEXT("First battle retains shield"),F.State->GetRunView().ObjectiveShield,400.0f);
	FDefenseTeamShopTestAccess::Phase(F.Director,EDefensePhase::Intermission,0); TestNull(TEXT("First clear expires exact core handle"),F.Core->GetCombatModifierComponent()->FindRuntime(Handle));
	TestEqual(TEXT("Expired shield projection cleared"),F.State->GetRunView().ObjectiveShield,0.0f);
	TestFalse(TEXT("Once-per-run after expiry"),F.Buy(F.Players[1],F.Request(2,0,1)).bSuccess); return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDefenseCoreShieldLifecycleTest,"Combat.Defense.CoreRescue.DepletionDeathAndShutdown",DefenseTeamShopTests::Flags)
bool FDefenseCoreShieldLifecycleTest::RunTest(const FString&)
{
	DefenseTeamShopTests::FFixture F; F.ConfigureShield(); if(!F.Start(*this)) return false;
	TestTrue(TEXT("Buy shield"),F.Buy(F.Players[0],F.Request()).bSuccess);
	FDefenseTeamShopTestAccess::Phase(F.Director,EDefensePhase::WaveCombat,0); F.Damage(F.Core,450);
	FDefenseTeamShopTestAccess::Refresh(F.Director);
	TestEqual(TEXT("Core takes only50"),F.Core->GetCombatAttributeSet()->GetHealth(),4950.0f);
	TestFalse(TEXT("Depleted offer stops claiming active"),F.Director->BuildTeamItemOffer(F.Players[0],F.Items[0]->GetPrimaryAssetId()).bEffectActive);
	FDefenseTeamShopTestAccess::Service(F.Director); TestEqual(TEXT("Hero service does not regrant core shield"),F.Core->GetCombatModifierComponent()->GetActiveModifierCount(),0);
	F.Director->Shutdown(); F.Director->Shutdown(); TestEqual(TEXT("Shutdown ledger cleared once"),FDefenseTeamShopTestAccess::Ledger(F.Director),0);
	DefenseTeamShopTests::FFixture G; G.ConfigureShield(); if(!G.Start(*this)) return false;
	FDefenseTeamShopTestAccess::Phase(G.Director,EDefensePhase::BossPreparation,4);
	TestTrue(TEXT("Boss preparation can buy"),G.Buy(G.Players[0],G.Request()).bSuccess);
	TestEqual(TEXT("Covers final boss wave"),G.Director->BuildTeamItemOffer(G.Players[0],G.Items[0]->GetPrimaryAssetId()).ExpireWave,6);
	G.Damage(G.Core,100000); TestTrue(TEXT("Core death terminates run"),G.Director->GetTerminalResult().bTerminal);
	TestEqual(TEXT("Terminal removes shield"),G.Core->GetCombatModifierComponent()->GetActiveModifierCount(),0);
	TestEqual(TEXT("Terminal shield projected zero"),G.State->GetRunView().ObjectiveShield,0.0f);
	TestFalse(TEXT("Terminal cannot buy other rescue"),G.Buy(G.Players[0],G.Request(2,5)).bSuccess); return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDefenseCoreShieldRollbackTest,"Combat.Defense.CoreRescue.ApplyRollbackAndHookReentry",DefenseTeamShopTests::Flags)
bool FDefenseCoreShieldRollbackTest::RunTest(const FString&)
{
	DefenseTeamShopTests::FFixture F; F.ConfigureShield(); if(!F.Start(*this)) return false;
	bool Shutdown=false; const auto Delegate=F.Core->GetCombatModifierComponent()->OnModifierCollectionChanged().AddLambda([&]
	{ if(!Shutdown) { Shutdown=true; F.Director->Shutdown(); } });
	TestFalse(TEXT("Shutdown during core apply rolls back"),F.Buy(F.Players[0],F.Request()).bSuccess);
	F.Core->GetCombatModifierComponent()->OnModifierCollectionChanged().Remove(Delegate);
	TestEqual(TEXT("No charged gold on rollback"),F.Players[0]->GetCombatEconomyComponent()->GetGold(),int64(500));
	TestEqual(TEXT("No orphan shield"),F.Core->GetCombatModifierComponent()->GetActiveModifierCount(),0);
	DefenseTeamShopTests::FFixture G; G.ConfigureShield(); if(!G.Start(*this)) return false;
	auto* Reflection=NewObject<UCombatModifierData>(G.Core); Reflection->DefinitionName=TEXT("core_test_reflection"); Reflection->RuntimeClass=UCombatDamageReflectionRuntime::StaticClass(); Reflection->RuntimeParameters.Add(TEXT("reflection_pct"),.1f);
	FCombatModifierApplyRequest A; A.ModifierData=Reflection; A.Source=G.Core;
	const auto ReflectHandle=G.Core->GetCombatModifierComponent()->ApplyModifier(A).Handle;
	auto* Events=G.World.GetWorld()->GetSubsystem<UCombatEventSubsystem>(); bool Attempted=false; FCombatEconomyResult Result;
	const auto EventDelegate=Events->OnRecord().AddLambda([&](const FCombatLogRecord& Record)
	{
		if(!Attempted&&Record.EventType==CombatTags::Event_Combat_DamageApplied&&Record.Flags.HasTagExact(CombatTags::Damage_Flag_Reflection))
		{ Attempted=true; Result=G.Buy(G.Players[0],G.Request()); }
	});
	FCombatDamageRequest D; D.Source=G.Players[0]->GetCommandedUnit(); D.Target=G.Core; D.Amount=20; D.DamageType=ECombatDamageType::Pure;
	G.World.GetWorld()->GetSubsystem<UCombatDamageSubsystem>()->DealDamage(D); Events->OnRecord().Remove(EventDelegate);
	TestTrue(TEXT("Nested reflection attempted acquisition"),Attempted); TestFalse(TEXT("Deferred hook acquisition rejected"),Result.bSuccess);
	TestEqual(TEXT("Deferred failure did not enqueue orphan shield"),G.Core->GetCombatModifierComponent()->GetActiveModifierCount(),1);
	TestEqual(TEXT("Deferred failure kept gold"),G.Players[0]->GetCombatEconomyComponent()->GetGold(),int64(500));
	G.Core->GetCombatModifierComponent()->RemoveModifier(ReflectHandle); return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDefenseCoreShieldGCTest,"Combat.Defense.CoreRescue.FrozenShieldSurvivesGC",DefenseTeamShopTests::Flags)
bool FDefenseCoreShieldGCTest::RunTest(const FString&)
{
	DefenseTeamShopTests::FFixture F; F.ConfigureShield(); if(!F.Start(*this)) return false;
	const auto Request=F.Request(); CollectGarbage(RF_NoFlags);
	TestTrue(TEXT("Frozen shield retained through GC"),F.Buy(F.Players[0],Request).bSuccess);
	TestEqual(TEXT("Frozen shield remains effective"),F.State->GetRunView().ObjectiveShield,400.0f);
	F.Director->Shutdown(); TestEqual(TEXT("Shutdown revokes core shield"),F.Core->GetCombatModifierComponent()->GetActiveModifierCount(),0);
	TestEqual(TEXT("Shutdown clears projected shield"),F.State->GetRunView().ObjectiveShield,0.0f); return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDefenseCoreShieldOwnerEndPlayTest,"Combat.Defense.CoreRescue.CoreEndPlayDuringApply",DefenseTeamShopTests::Flags)
bool FDefenseCoreShieldOwnerEndPlayTest::RunTest(const FString&)
{
	DefenseTeamShopTests::FFixture F; F.ConfigureShield(); if(!F.Start(*this)) return false;
	bool Destroyed=false;
	const auto Delegate=F.Core->GetCombatModifierComponent()->OnModifierCollectionChanged().AddLambda([&]
	{ if(!Destroyed) { Destroyed=true; F.Core->Destroy(); } });
	TestFalse(TEXT("Lost core cannot commit a purchase"),F.Buy(F.Players[0],F.Request()).bSuccess);
	F.Core->GetCombatModifierComponent()->OnModifierCollectionChanged().Remove(Delegate);
	TestTrue(TEXT("Actual core EndPlay exercised"),Destroyed);
	TestEqual(TEXT("Lost core refunds purchase"),F.Players[0]->GetCombatEconomyComponent()->GetGold(),int64(500));
	TestEqual(TEXT("Lost core creates no purchase ledger"),FDefenseTeamShopTestAccess::Ledger(F.Director),0); return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDefenseReinforcementPurchaseTest,"Combat.Defense.Reinforcement.PurchaseAndRevival",DefenseTeamShopTests::Flags)
bool FDefenseReinforcementPurchaseTest::RunTest(const FString&)
{
	DefenseTeamShopTests::FFixture F; F.ConfigureReinforcement(); if(!F.Start(*this)) return false;
	const auto Id=F.Items[0]->GetPrimaryAssetId(); const auto R=F.Request(); const auto Rival=F.Request(1,0,1);
	TestTrue(TEXT("Reinforcement bought from original economy"),F.Buy(F.Players[0],R).bSuccess);
	TestTrue(TEXT("Same request receipt"),F.Buy(F.Players[0],R).bSuccess);
	TestFalse(TEXT("Competing purchase rejected"),F.Buy(F.Players[1],Rival).bSuccess);
	TestEqual(TEXT("One200 charge"),F.Players[0]->GetCombatEconomyComponent()->GetGold(),int64(300));
	TestEqual(TEXT("Two initial reinforcements"),FDefenseTeamShopTestAccess::AidLeft(F.Director,Id),2);
	auto* Hero=F.Players[0]->GetCommandedUnit(); F.Damage(Hero,100000);
	TestEqual(TEXT("First death takes one charge"),FDefenseTeamShopTestAccess::AidLeft(F.Director,Id),1);
	TestEqual(TEXT("Default8 reduced to5"),FDefenseTeamShopTestAccess::RespawnAt(F.Director,F.Players[0])-F.World.GetWorld()->GetTimeSeconds(),5.0);
	FDefenseTeamShopTestAccess::DiedAgain(F.Director,Hero);
	TestEqual(TEXT("Repeated death event cannot take another"),FDefenseTeamShopTestAccess::AidLeft(F.Director,Id),1);
	TestTrue(TEXT("Pending rescue visible"),F.Director->BuildTeamItemOffer(F.Players[0],Id).Status.ToString().Contains(TEXT("等待复活 1 人")));
	CombatEffectUtilities::ApplyAttributeAdditive(Hero,*Hero->GetCombatAbilitySystemComponent(),UCombatAttributeSet::GetMaxHealthAttribute(),100);
	const float Amount=Hero->GetCombatAttributeSet()->GetMaxHealth()*.2f;
	TestTrue(TEXT("Public respawn"),Hero->GetCombatLifecycleComponent()->RespawnAtLocation(FVector(1000,0,100)));
	const auto Handle=FDefenseTeamShopTestAccess::FirstHandle(F.Director,Id);
	auto* Shield=Cast<UDefenseShieldRuntime>(Hero->GetCombatModifierComponent()->FindRuntime(Handle));
	if(!TestNotNull(TEXT("Respawn grants shield"),Shield)) return false;
	TestEqual(TEXT("Shield snapshots respawn max health"),Shield->GetRemainingShield(),Amount);
	TestEqual(TEXT("Shield uses four second Scheduler duration"),Shield->GetExpireAt()-Shield->GetAppliedAt(),4.0);
	const double ExpireAt=Shield->GetExpireAt(); FDefenseTeamShopTestAccess::RespawnedAgain(F.Director,Hero); FDefenseTeamShopTestAccess::Service(F.Director);
	TestEqual(TEXT("Repeated respawn does not duplicate shield"),Hero->GetCombatModifierComponent()->GetActiveModifierCount(),1);
	TestEqual(TEXT("Repeated respawn does not refresh duration"),Shield->GetExpireAt(),ExpireAt);
	const float Health=Hero->GetCombatAttributeSet()->GetHealth(); F.Damage(Hero,10);
	TestEqual(TEXT("Shield absorbs actual damage"),Hero->GetCombatAttributeSet()->GetHealth(),Health);
	F.World.GetWorld()->GetSubsystem<UCombatSchedulerSubsystem>()->RunDueTasks(ExpireAt+.01);
	FDefenseTeamShopTestAccess::Refresh(F.Director);
	TestNull(TEXT("Timed shield removed"),Hero->GetCombatModifierComponent()->FindRuntime(Handle));
	TestFalse(TEXT("Old handle stays invalid"),Hero->GetCombatModifierComponent()->RemoveModifier(Handle));
	TestTrue(TEXT("Unused charge stays across expiry"),F.Director->BuildTeamItemOffer(F.Players[0],Id).bEffectActive); return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDefenseReinforcementThreeDeathsTest,"Combat.Defense.Reinforcement.ThreeDeathsAndWavePersistence",DefenseTeamShopTests::Flags)
bool FDefenseReinforcementThreeDeathsTest::RunTest(const FString&)
{
	DefenseTeamShopTests::FFixture F; F.ConfigureReinforcement(); if(!F.Start(*this)) return false;
	const auto Id=F.Items[0]->GetPrimaryAssetId(); TestTrue(TEXT("Purchase"),F.Buy(F.Players[0],F.Request()).bSuccess);
	FDefenseTeamShopTestAccess::Phase(F.Director,EDefensePhase::Intermission,1);
	TestEqual(TEXT("Charges persist across waves"),FDefenseTeamShopTestAccess::AidLeft(F.Director,Id),2);
	auto* Hero=F.Players[0]->GetCommandedUnit();
	for(int32 I=0;I<3;++I)
	{
		F.Damage(Hero,100000);
		TestEqual(TEXT("Only next two deaths consume charges"),FDefenseTeamShopTestAccess::AidLeft(F.Director,Id),FMath::Max(0,1-I));
		TestEqual(TEXT("Delays5/5/8"),FDefenseTeamShopTestAccess::RespawnAt(F.Director,F.Players[0])-F.World.GetWorld()->GetTimeSeconds(),I<2?5.0:8.0);
		TestTrue(TEXT("Successive public respawn"),Hero->GetCombatLifecycleComponent()->RespawnAtLocation(FVector(1000,0,100)));
		TestEqual(TEXT("No shield on third revival"),Hero->GetCombatModifierComponent()->GetActiveModifierCount(),I<2?1:0);
		FDefenseTeamShopTestAccess::Refresh(F.Director);
	}
	TestFalse(TEXT("Exhausted reinforcement not active"),F.Director->BuildTeamItemOffer(F.Players[0],Id).bEffectActive);
	TestFalse(TEXT("Cannot buy same item again"),F.Buy(F.Players[0],F.Request(2)).bSuccess); return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDefenseReinforcementPreexistingTest,"Combat.Defense.Reinforcement.PreexistingDeathAndLogReentry",DefenseTeamShopTests::Flags)
bool FDefenseReinforcementPreexistingTest::RunTest(const FString&)
{
	for(bool InDeathLog:{false,true})
	{
		DefenseTeamShopTests::FFixture F; F.ConfigureReinforcement(); if(!F.Start(*this)) return false;
		auto* Hero=F.Players[0]->GetCommandedUnit(); const auto Id=F.Items[0]->GetPrimaryAssetId();
		auto* Events=F.World.GetWorld()->GetSubsystem<UCombatEventSubsystem>(); bool Bought=false;
		const auto Hook=Events->OnRecord().AddLambda([&](const FCombatLogRecord& Record)
		{
			if(InDeathLog&&!Bought&&Record.EventType==CombatTags::Event_Combat_UnitDeath&&Record.TargetActorId==Hero->GetUniqueID())
			{ Bought=true; TestTrue(TEXT("Can purchase while newly dead"),F.Buy(F.Players[0],F.Request()).bSuccess); }
		});
		F.Damage(Hero,100000); Events->OnRecord().Remove(Hook);
		if(!InDeathLog) TestTrue(TEXT("Dead buyer purchase"),F.Buy(F.Players[0],F.Request()).bSuccess);
		else TestTrue(TEXT("Death log observer really ran"),Bought);
		TestEqual(TEXT("Already dead is not retroactively shortened"),FDefenseTeamShopTestAccess::RespawnAt(F.Director,F.Players[0])-F.World.GetWorld()->GetTimeSeconds(),8.0);
		TestEqual(TEXT("Two charges preserved"),FDefenseTeamShopTestAccess::AidLeft(F.Director,Id),2);
		TestTrue(TEXT("Old death respawn"),Hero->GetCombatLifecycleComponent()->RespawnAtLocation(FVector(1000,0,100)));
		TestEqual(TEXT("No shield for old death"),Hero->GetCombatModifierComponent()->GetActiveModifierCount(),0);
		F.Damage(Hero,100000); TestEqual(TEXT("Next new death eligible"),FDefenseTeamShopTestAccess::AidLeft(F.Director,Id),1);
	}
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDefenseReinforcementDisconnectTest,"Combat.Defense.Reinforcement.DisconnectAndTeardown",DefenseTeamShopTests::Flags)
bool FDefenseReinforcementDisconnectTest::RunTest(const FString&)
{
	DefenseTeamShopTests::FFixture F; F.ConfigureReinforcement(); if(!F.Start(*this)) return false;
	const auto Id=F.Items[0]->GetPrimaryAssetId(); TestTrue(TEXT("Buy"),F.Buy(F.Players[0],F.Request()).bSuccess);
	auto* Buyer=F.Players[0]->GetCommandedUnit(); F.Director->RemovePlayer(F.Players[0]);
	TestFalse(TEXT("Buyer death delegate unbound"),Buyer->GetCombatLifecycleComponent()->OnDied().IsBoundToObject(F.Director));
	auto* Hero=F.Players[1]->GetCommandedUnit(); F.Damage(Hero,100000);
	TestEqual(TEXT("Buyer departure preserves teammate rescue"),FDefenseTeamShopTestAccess::AidLeft(F.Director,Id),1);
	TestTrue(TEXT("Teammate respawn"),Hero->GetCombatLifecycleComponent()->RespawnAtLocation(FVector(1000,300,100)));
	TestEqual(TEXT("Teammate protected"),Hero->GetCombatModifierComponent()->GetActiveModifierCount(),1);
	F.Director->RemovePlayer(F.Players[1]);
	TestEqual(TEXT("Disconnected receiver shield removed"),Hero->GetCombatModifierComponent()->GetActiveModifierCount(),0);
	TestEqual(TEXT("Spent charge not refunded"),FDefenseTeamShopTestAccess::AidLeft(F.Director,Id),1);
	TestFalse(TEXT("Receiver respawn delegate unbound"),Hero->GetCombatLifecycleComponent()->OnRespawned().IsBoundToObject(F.Director));
	F.Director->Shutdown(); F.Director->Shutdown(); TestEqual(TEXT("Ledger cleared"),FDefenseTeamShopTestAccess::Ledger(F.Director),0); return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDefenseReinforcementTerminalTest,"Combat.Defense.Reinforcement.PendingTerminalAndApplyReentry",DefenseTeamShopTests::Flags)
bool FDefenseReinforcementTerminalTest::RunTest(const FString&)
{
	for(int32 Case=0;Case<3;++Case)
	{
		DefenseTeamShopTests::FFixture F; F.ConfigureReinforcement(); if(!F.Start(*this)) return false;
		TestTrue(TEXT("Buy"),F.Buy(F.Players[0],F.Request()).bSuccess); auto* Hero=F.Players[0]->GetCommandedUnit();
		F.Damage(Hero,100000); bool Changed=false;
		FDelegateHandle Delegate;
		if(Case==0) FDefenseTeamShopTestAccess::Finish(F.Director);
		else Delegate=Hero->GetCombatModifierComponent()->OnModifierCollectionChanged().AddLambda([&]
		{ if(!Changed) { Changed=true; if(Case==1) F.Director->Shutdown(); else F.Director->RemovePlayer(F.Players[0]); } });
		TestTrue(TEXT("Lifecycle itself still respawns"),Hero->GetCombatLifecycleComponent()->RespawnAtLocation(FVector(1000,0,100)));
		if(Delegate.IsValid()) Hero->GetCombatModifierComponent()->OnModifierCollectionChanged().Remove(Delegate);
		if(Case>0) TestTrue(TEXT("Real application observer executed"),Changed);
		TestEqual(TEXT("No shield leaks from canceled recipient"),Hero->GetCombatModifierComponent()->GetActiveModifierCount(),0);
		TestFalse(TEXT("Canceled lifecycle delegate removed"),Hero->GetCombatLifecycleComponent()->OnRespawned().IsBoundToObject(F.Director));
	}
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDefenseReinforcementValidationTest,"Combat.Defense.Reinforcement.ValidationAndFrozenGC",DefenseTeamShopTests::Flags)
bool FDefenseReinforcementValidationTest::RunTest(const FString&)
{
	DefenseTeamShopTests::FFixture F; F.ConfigureReinforcement(); FString Error; auto* E=F.Effects[0];
	TestTrue(TEXT("Valid aid"),E->ValidateDefinition(Error));
	E->ReinforcementDeaths=9; TestFalse(TEXT("Counts bounded"),E->ValidateDefinition(Error)); E->ReinforcementDeaths=2;
	E->RespawnReduction=NAN; TestFalse(TEXT("Finite reduction"),E->ValidateDefinition(Error)); E->RespawnReduction=3;
	E->RespawnShieldFraction=1.01f; TestFalse(TEXT("Fraction bounded"),E->ValidateDefinition(Error)); E->RespawnShieldFraction=.2f;
	E->ReinforcementShieldData->Duration=0; TestFalse(TEXT("Infinite revival shield forbidden"),E->ValidateDefinition(Error)); E->ReinforcementShieldData->Duration=4;
	E->ObjectiveHeal=10; TestFalse(TEXT("Combined partial commit forbidden"),E->ValidateDefinition(Error)); E->ObjectiveHeal=0;
	if(!F.Start(*this)) return false;
	const auto Request=F.Request(); auto* Hero=F.Players[0]->GetCommandedUnit();
	E->RespawnReduction=1; E->RespawnShieldFraction=.9f; E->ReinforcementShieldData->Duration=30;
	CollectGarbage(RF_NoFlags); TestTrue(TEXT("Frozen definitions retained"),F.Buy(F.Players[0],Request).bSuccess);
	F.Damage(Hero,100000);
	TestEqual(TEXT("Frozen reduction remains3"),FDefenseTeamShopTestAccess::RespawnAt(F.Director,F.Players[0])-F.World.GetWorld()->GetTimeSeconds(),5.0);
	TestTrue(TEXT("Frozen revival"),Hero->GetCombatLifecycleComponent()->RespawnAtLocation(FVector(1000,0,100)));
	const auto Handle=FDefenseTeamShopTestAccess::FirstHandle(F.Director,Request.ItemDefinitionId);
	const auto* Shield=Cast<UDefenseShieldRuntime>(Hero->GetCombatModifierComponent()->FindRuntime(Handle));
	if(!TestNotNull(TEXT("Frozen shield applied"),Shield)) return false;
	TestEqual(TEXT("Frozen percentage"),Shield->GetRemainingShield(),Hero->GetCombatAttributeSet()->GetMaxHealth()*.2f);
	TestEqual(TEXT("Frozen duration"),Shield->GetExpireAt()-Shield->GetAppliedAt(),4.0); return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDefenseReinforcementOldDeathTest,"Combat.Defense.Reinforcement.OldDeathAfterReregister",DefenseTeamShopTests::Flags)
bool FDefenseReinforcementOldDeathTest::RunTest(const FString&)
{
	DefenseTeamShopTests::FFixture F; F.ConfigureReinforcement(); if(!F.Start(*this)) return false;
	auto* Hero=F.Players[0]->GetCommandedUnit(); F.Damage(Hero,100000);
	TestTrue(TEXT("Purchase after death"),F.Buy(F.Players[1],F.Request(1,0,1)).bSuccess);
	F.Director->RemovePlayer(F.Players[0]); TestTrue(TEXT("Preparation reregisters same hero"),F.Director->RegisterPlayer(F.Players[0],Hero));
	FDefenseTeamShopTestAccess::DiedAgain(F.Director,Hero);
	TestEqual(TEXT("Old life exclusion survives registration churn"),FDefenseTeamShopTestAccess::AidLeft(F.Director,F.Items[0]->GetPrimaryAssetId()),2); return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDefenseReinforcementDeferredTest,"Combat.Defense.Reinforcement.RespawnInsideHookDefersSafely",DefenseTeamShopTests::Flags)
bool FDefenseReinforcementDeferredTest::RunTest(const FString&)
{
	DefenseTeamShopTests::FFixture F; F.ConfigureReinforcement(); if(!F.Start(*this)) return false;
	TestTrue(TEXT("Purchase"),F.Buy(F.Players[0],F.Request()).bSuccess); auto* Hero=F.Players[0]->GetCommandedUnit();
	auto* M=NewObject<UCombatModifierData>(Hero); M->DefinitionName=TEXT("aid_reflection_fixture"); M->RuntimeClass=UCombatDamageReflectionRuntime::StaticClass(); M->RuntimeParameters.Add(TEXT("reflection_pct"),.1f);
	FCombatModifierApplyRequest Apply; Apply.ModifierData=M; Apply.Source=Hero; TestTrue(TEXT("Reflection applied"),Hero->GetCombatModifierComponent()->ApplyModifier(Apply).bSuccess);
	auto* Events=F.World.GetWorld()->GetSubsystem<UCombatEventSubsystem>(); bool Ran=false;
	const auto Delegate=Events->OnRecord().AddLambda([&](const FCombatLogRecord& R)
	{
		if(!Ran&&R.EventType==CombatTags::Event_Combat_DamageApplied&&R.Flags.HasTagExact(CombatTags::Damage_Flag_Reflection))
		{
			Ran=true; TestTrue(TEXT("Hero hook is deferring"),Hero->GetCombatModifierComponent()->IsDeferringOperations());
			TestTrue(TEXT("Public death inside observer"),Hero->GetCombatLifecycleComponent()->RequestDeath(R.Context,nullptr));
			TestTrue(TEXT("Public revival inside observer"),Hero->GetCombatLifecycleComponent()->RespawnAtLocation(FVector(1000,0,100)));
		}
	});
	FCombatDamageRequest Damage; Damage.Source=F.Core; Damage.Target=Hero; Damage.Amount=10; Damage.DamageType=ECombatDamageType::Pure;
	F.World.GetWorld()->GetSubsystem<UCombatDamageSubsystem>()->DealDamage(Damage); Events->OnRecord().Remove(Delegate);
	TestTrue(TEXT("Actual nested hook tested"),Ran);
	TestEqual(TEXT("No queued orphan shield"),Hero->GetCombatModifierComponent()->GetActiveModifierCount(),0);
	FDefenseTeamShopTestAccess::Service(F.Director); TestEqual(TEXT("Next monitor grants exactly one shield"),Hero->GetCombatModifierComponent()->GetActiveModifierCount(),1);
	TestEqual(TEXT("One death charged once"),FDefenseTeamShopTestAccess::AidLeft(F.Director,F.Items[0]->GetPrimaryAssetId()),1); return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDefenseFormalTeamItemsTest,"Combat.Defense.Phase2.FormalTeamItems",DefenseTeamShopTests::Flags)
bool FDefenseFormalTeamItemsTest::RunTest(const FString&)
{
	DefenseTeamShopTests::FFixture F; F.ConfigureFormal(); if(!F.Start(*this)) return false;
	auto* Source=F.Players[0]->GetCommandedUnit();
	FActorSpawnParameters Spawn; Spawn.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	auto MakeTarget=[&](const TCHAR* Name)
	{
		auto* Unit=F.World.GetWorld()->SpawnActor<ACombatUnitCharacter>(FVector(1500,0,100),FRotator::ZeroRotator,Spawn); auto* Data=NewObject<UCombatUnitData>(Unit); Data->DefinitionName=Name; Unit->InitializeFromUnitData(Data); if(!Unit->HasActorBegunPlay()) Unit->DispatchBeginPlay(); return Unit;
	};
	auto* Siege=MakeTarget(TEXT("siege_enemy")); auto* Normal=MakeTarget(TEXT("normal_enemy")); auto* Boss=MakeTarget(TEXT("boss_enemy"));
	F.Players[0]->GetCombatEconomyComponent()->AddGold(500,TEXT("FormalTest")); const float BaseCooldown=Source->GetCombatAttributeSet()->GetCooldownReductionPct();
	TestTrue(TEXT("Hunt horn purchase"),F.Buy(F.Players[0],F.Request(1,0)).bSuccess);
	TestTrue(TEXT("Agile circuit purchase"),F.Buy(F.Players[0],F.Request(2,1)).bSuccess);
	TestTrue(TEXT("Breach inscription purchase"),F.Buy(F.Players[0],F.Request(3,2)).bSuccess);
	TestEqual(TEXT("Agile circuit adds five percent cooldown reduction"),Source->GetCombatAttributeSet()->GetCooldownReductionPct(),BaseCooldown+.05f);
	FCombatDamageRequest D; D.Source=Source; D.Amount=100; D.DamageType=ECombatDamageType::Pure;
	D.Target=Siege; const auto Hunt=F.World.GetWorld()->GetSubsystem<UCombatDamageSubsystem>()->DealDamage(D);
	D.Target=Normal; const auto Plain=F.World.GetWorld()->GetSubsystem<UCombatDamageSubsystem>()->DealDamage(D);
	D.Target=Boss; const auto BossHit=F.World.GetWorld()->GetSubsystem<UCombatDamageSubsystem>()->DealDamage(D);
	TestTrue(TEXT("Hunt horn increases siege damage"),Hunt.Event.Amount>Plain.Event.Amount);
	TestTrue(TEXT("Unknown target is not increased"),Plain.Event.AppliedAmount>0&&Plain.Event.AppliedAmount<115.0f);
	TestTrue(TEXT("Breach inscription increases boss damage"),BossHit.Event.Amount>Plain.Event.Amount);
	TestEqual(TEXT("All three formal items occupy one shared ledger each"),FDefenseTeamShopTestAccess::Ledger(F.Director),3); return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDefenseFormalTeamItemsValidationTest,"Combat.Defense.Phase2.FormalTeamItemsValidation",DefenseTeamShopTests::Flags)
bool FDefenseFormalTeamItemsValidationTest::RunTest(const FString&)
{
	DefenseTeamShopTests::FFixture F; F.ConfigureFormal(); FString Error; TestTrue(TEXT("Formal definitions valid"),F.Shop->ValidateRuntime(Error));
	F.Effects[0]->ConditionalDamageFraction=1.01f; TestFalse(TEXT("Conditional fraction bounded"),F.Effects[0]->ValidateDefinition(Error)); F.Effects[0]->ConditionalDamageFraction=.15f;
	F.Effects[0]->ModifierData->RuntimeParameters.Reset(); TestFalse(TEXT("Conditional runtime needs target parameters"),F.Effects[0]->ValidateDefinition(Error)); F.Effects[0]->ModifierData->RuntimeParameters.Add(TEXT("target_role"),1); F.Effects[0]->ModifierData->RuntimeParameters.Add(TEXT("damage_fraction"),.15f);
	F.Effects[1]->DurationWaves=0; TestFalse(TEXT("Agile is wave scoped"),F.Effects[1]->ValidateDefinition(Error)); return true;
}
/** 真实攻城资产承受公共Damage，用实际Health差验证号角，而非只比较请求Amount。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDefenseSiegeHornTest,"Combat.Defense.Siege.HornActualDamageAndExpiry",DefenseTeamShopTests::Flags)
bool FDefenseSiegeHornTest::RunTest(const FString&)
{
	DefenseTeamShopTests::FFixture F;
	auto* Horn=LoadObject<UCombatItemData>(nullptr,TEXT("/Game/Combat/Demo/Defense/TeamShop/DA_Item_team_hunt_horn.DA_Item_team_hunt_horn"));
	auto* Data=LoadObject<UCombatUnitData>(nullptr,TEXT("/Game/Combat/Demo/Defense/DA_DefenseSiege.DA_DefenseSiege"));
	auto* Class=LoadClass<ADefenseEnemy>(nullptr,TEXT("/Game/Combat/Demo/Defense/BP_DefenseSiege.BP_DefenseSiege_C"));
	if(!TestNotNull(TEXT("Actual horn asset"),Horn)||!TestNotNull(TEXT("Actual siege asset"),Data)||!TestNotNull(TEXT("Actual siege blueprint"),Class)) return false;
	F.Items[0]=Horn; F.Shop->Categories[0].Items[0]=Horn;
	if(!F.Start(*this)) return false;
	auto* W=F.World.GetWorld(); FActorSpawnParameters Spawn; Spawn.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	auto* Target=W->SpawnActor<ADefenseEnemy>(Class,FVector(1500,0,100),FRotator::ZeroRotator,Spawn);
	if(!TestNotNull(TEXT("Production siege spawned"),Target)) return false;
	TestTrue(TEXT("Initialize real definition"),Target->InitializeFromUnitData(Data)); if(!Target->HasActorBegunPlay()) Target->DispatchBeginPlay();
	auto* Source=F.Players[0]->GetCommandedUnit(); auto* Plain=F.Players[1]->GetCommandedUnit();
	const int64 BeforeGold=F.Players[0]->GetCombatEconomyComponent()->GetGold(); const auto XP=Source->GetCombatProgressionComponent()->GetExperience();
	FCombatDamageRequest R; R.Source=Source; R.DamageType=ECombatDamageType::Pure; R.Amount=20;
	R.Target=Target; const auto Base=W->GetSubsystem<UCombatDamageSubsystem>()->DealDamage(R);
	R.Target=Plain; const auto PlainBase=W->GetSubsystem<UCombatDamageSubsystem>()->DealDamage(R);
	const auto Request=F.Request(); TestTrue(TEXT("Horn original economy purchase"),F.Buy(F.Players[0],Request).bSuccess);
	TestTrue(TEXT("Horn repeated receipt"),F.Buy(F.Players[0],Request).bSuccess);
	R.Target=Target; const float HP=Target->GetCombatAttributeSet()->GetHealth(); const auto Boosted=W->GetSubsystem<UCombatDamageSubsystem>()->DealDamage(R);
	TestTrue(TEXT("Real siege dealt damage"),Base.bSuccess&&Boosted.bSuccess&&Base.Event.AppliedAmount>0);
	TestEqual(TEXT("Siege actual damage +15 percent"),Boosted.Event.AppliedAmount,Base.Event.AppliedAmount*1.15f);
	TestEqual(TEXT("Health confirms actual delta"),HP-Target->GetCombatAttributeSet()->GetHealth(),Boosted.Event.AppliedAmount);
	R.Target=Plain; const auto PlainBoosted=W->GetSubsystem<UCombatDamageSubsystem>()->DealDamage(R);
	TestEqual(TEXT("Ordinary target no amplification"),PlainBoosted.Event.AppliedAmount,PlainBase.Event.AppliedAmount);
	TestEqual(TEXT("One260 payment"),F.Players[0]->GetCombatEconomyComponent()->GetGold(),BeforeGold-260);
	FDefenseTeamShopTestAccess::Phase(F.Director,EDefensePhase::Intermission,0);
	R.Target=Target; const auto After=W->GetSubsystem<UCombatDamageSubsystem>()->DealDamage(R);
	TestEqual(TEXT("Expiry removes hunt amplification"),After.Event.AppliedAmount,Base.Event.AppliedAmount);
	R.Amount=100000; W->GetSubsystem<UCombatDamageSubsystem>()->DealDamage(R);
	TestEqual(TEXT("No per-kill gold duplicates wave reward"),F.Players[0]->GetCombatEconomyComponent()->GetGold(),BeforeGold-260);
	TestEqual(TEXT("No per-kill XP"),Source->GetCombatProgressionComponent()->GetExperience(),XP);
	F.Director->Shutdown(); TestEqual(TEXT("Hero modifiers cleaned"),Source->GetCombatModifierComponent()->GetActiveModifierCount(),0); return true;
}
/** 真实刺客资产承受公共Damage，用实际Health差验证号角，而非只比较请求Amount。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDefenseAssassinHornTest,"Combat.Defense.Assassin.HornActualDamageAndExpiry",DefenseTeamShopTests::Flags)
bool FDefenseAssassinHornTest::RunTest(const FString&)
{
	DefenseTeamShopTests::FFixture F;
	auto* Horn=LoadObject<UCombatItemData>(nullptr,TEXT("/Game/Combat/Demo/Defense/TeamShop/DA_Item_team_hunt_horn.DA_Item_team_hunt_horn"));
	auto* Data=LoadObject<UCombatUnitData>(nullptr,TEXT("/Game/Combat/Demo/Defense/DA_DefenseAssassin.DA_DefenseAssassin"));
	auto* Class=LoadClass<ADefenseEnemy>(nullptr,TEXT("/Game/Combat/Demo/Defense/BP_DefenseAssassin.BP_DefenseAssassin_C"));
	if(!TestNotNull(TEXT("Actual horn asset"),Horn)||!TestNotNull(TEXT("Actual assassin asset"),Data)||!TestNotNull(TEXT("Actual assassin blueprint"),Class)) return false;
	F.Items[0]=Horn; F.Shop->Categories[0].Items[0]=Horn;
	if(!F.Start(*this)) return false;
	auto* W=F.World.GetWorld(); FActorSpawnParameters Spawn; Spawn.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	auto* Target=W->SpawnActor<ADefenseEnemy>(Class,FVector(1500,0,100),FRotator::ZeroRotator,Spawn);
	if(!TestNotNull(TEXT("Production assassin spawned"),Target)) return false;
	TestTrue(TEXT("Initialize real definition"),Target->InitializeFromUnitData(Data)); if(!Target->HasActorBegunPlay()) Target->DispatchBeginPlay();
	auto* Source=F.Players[0]->GetCommandedUnit(); auto* Plain=F.Players[1]->GetCommandedUnit();
	const int64 BeforeGold=F.Players[0]->GetCombatEconomyComponent()->GetGold(); const auto XP=Source->GetCombatProgressionComponent()->GetExperience();
	FCombatDamageRequest R; R.Source=Source; R.DamageType=ECombatDamageType::Pure; R.Amount=20;
	R.Target=Target; const auto Base=W->GetSubsystem<UCombatDamageSubsystem>()->DealDamage(R);
	R.Target=Plain; const auto PlainBase=W->GetSubsystem<UCombatDamageSubsystem>()->DealDamage(R);
	const auto Request=F.Request(); TestTrue(TEXT("Horn original economy purchase"),F.Buy(F.Players[0],Request).bSuccess);
	TestTrue(TEXT("Horn repeated receipt"),F.Buy(F.Players[0],Request).bSuccess);
	R.Target=Target; const float HP=Target->GetCombatAttributeSet()->GetHealth(); const auto Boosted=W->GetSubsystem<UCombatDamageSubsystem>()->DealDamage(R);
	TestTrue(TEXT("Real assassin dealt damage"),Base.bSuccess&&Boosted.bSuccess&&Base.Event.AppliedAmount>0);
	TestEqual(TEXT("Assassin actual damage +15 percent"),Boosted.Event.AppliedAmount,Base.Event.AppliedAmount*1.15f);
	TestEqual(TEXT("Health confirms actual delta"),HP-Target->GetCombatAttributeSet()->GetHealth(),Boosted.Event.AppliedAmount);
	R.Target=Plain; const auto PlainBoosted=W->GetSubsystem<UCombatDamageSubsystem>()->DealDamage(R);
	TestEqual(TEXT("Ordinary target no amplification"),PlainBoosted.Event.AppliedAmount,PlainBase.Event.AppliedAmount);
	TestEqual(TEXT("One260 payment"),F.Players[0]->GetCombatEconomyComponent()->GetGold(),BeforeGold-260);
	FDefenseTeamShopTestAccess::Phase(F.Director,EDefensePhase::Intermission,0);
	R.Target=Target; const auto After=W->GetSubsystem<UCombatDamageSubsystem>()->DealDamage(R);
	TestEqual(TEXT("Expiry removes hunt amplification"),After.Event.AppliedAmount,Base.Event.AppliedAmount);
	R.Amount=100000; W->GetSubsystem<UCombatDamageSubsystem>()->DealDamage(R);
	TestEqual(TEXT("No per-kill gold duplicates wave reward"),F.Players[0]->GetCombatEconomyComponent()->GetGold(),BeforeGold-260);
	TestEqual(TEXT("No per-kill XP"),Source->GetCombatProgressionComponent()->GetExperience(),XP);
	F.Director->Shutdown(); TestEqual(TEXT("Hero modifiers cleaned"),Source->GetCombatModifierComponent()->GetActiveModifierCount(),0); return true;
}
#endif
