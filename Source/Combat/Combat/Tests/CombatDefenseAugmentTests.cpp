#include "CoreMinimal.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Algo/Reverse.h"
#include "Combat/Defense/DefenseAugmentData.h"
#include "Combat/Defense/DefenseData.h"
#include "Combat/Defense/DefenseDirectorComponent.h"
#include "Combat/Defense/DefenseGameState.h"
#include "Combat/Defense/DefenseObjective.h"
#include "Combat/Tests/CombatAutomationWorldFixture.h"
#include "Combat/Unit/CombatUnitCharacter.h"
#include "CombatPlayerController.h"
#include "Combat/Defense/DefenseOfferSampler.h"
#include "Combat/Defense/DefenseBuildWidget.h"
#include "Combat/Defense/DefenseChoiceComponent.h"
#include "Combat/Defense/DefensePlayerController.h"
#include "Combat/Economy/CombatEconomyComponent.h"
#include "Combat/Economy/CombatEconomyData.h"
#include "Combat/Economy/CombatShopData.h"
#include "Combat/Items/CombatItemData.h"
#include "Combat/Unit/CombatUnitLifecycleComponent.h"
#include "Combat/Modifiers/CombatModifierComponent.h"
#include "Combat/Combat/CombatDamageSubsystem.h"
#include "Combat/Network/CombatNetworkSecuritySubsystem.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SBoxPanel.h"
#include "Engine/World.h"

/** 自动化只访问内部时间入口，不在生产提供直接改变选择状态的调试旁路。 */
struct FDefenseChoiceTestAccess
{
	static void Expire(UDefenseDirectorComponent* D,double Now) { D->ExpireChoices(Now); }
	static void Service(UDefenseDirectorComponent* D,double Now) { D->ServicePlayers(Now); }
	static int32 Cached(UDefenseDirectorComponent* D,ACombatPlayerController* PC) { return D->Players.FindChecked(PC).ChoiceResults.Num(); }
	static void Advance(UDefenseDirectorComponent* D) { FCombatScheduledTickContext T; D->Advance(T,D->View.RunId,D->GetPhaseGeneration()); }
};
namespace DefenseAugmentTests
{
	constexpr auto Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;
	/** 每个用例持有独立 World、数据池与两个固定加入顺序玩家，避免 CDO 残留。 */
	struct FFixture
	{
		FCombatAutomationWorldFixture World;
		UDefenseRunData* Run;
		UDefenseAugmentPoolData* Pool;
		UDefenseDirectorComponent* Director;
		ADefenseGameState* State;
		ADefenseObjective* Core;
		TArray<ACombatPlayerController*> Players;
		FFixture()
		{
			auto* W = World.GetWorld();
			auto* Owner = W->SpawnActor<AActor>();
			Run = UDefenseRunData::CreatePhase1Default(Owner);
			Run->PreparationDuration = 60; Run->IntermissionDuration=25;
			Run->DevelopmentSeedOverride = 831907;
			Pool = NewObject<UDefenseAugmentPoolData>(Run); Pool->DefinitionName=TEXT("augment_fixture_pool");
			for(int32 I=0; I<12; ++I)
			{
				auto* Card = NewObject<UDefenseAugmentData>(Pool);
				Card->DefinitionName = FName(FString::Printf(TEXT("augment_fixture_%02d"),I));
				Card->Family=static_cast<EDefenseAugmentFamily>(I%4); Card->Weight=100+I;
				auto* Mod=NewObject<UCombatModifierData>(Card); Mod->DefinitionName=FName(Card->DefinitionName.ToString()+TEXT("_effect"));
				Mod->bRemoveOnDeath=false;
				FCombatModifierAttributeChange Change; Change.Attribute=UCombatAttributeSet::GetArmorAttribute(); Change.Magnitude=5;
				Mod->AttributeChanges.Add(Change); Card->ModifierData=Mod;
				Pool->Augments.Add(Card);
			}
			Run->AugmentPool=Pool;
			Core=W->SpawnActor<ADefenseObjective>(); Core->InitializeFromUnitData(Run->ObjectiveData);
			if(!Core->HasActorBegunPlay()) Core->DispatchBeginPlay();
			State=W->SpawnActor<ADefenseGameState>();
			Director=NewObject<UDefenseDirectorComponent>(Owner); Owner->AddInstanceComponent(Director); Director->RegisterComponent();
			if(!Owner->HasActorBegunPlay()) Owner->DispatchBeginPlay();
		}
		/** 显式开局使测试可以先修改输入，随后验证运行时冻结。 */
		bool Start(bool Wait=false)
		{
			if(!Director->InitializeForRun(Run,Core,State,Wait)) return false;
			for(int32 I=0; I<2; ++I)
			{
				auto* PC=World.GetWorld()->SpawnActor<ACombatPlayerController>();
				FActorSpawnParameters P; P.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
				auto* Hero=World.GetWorld()->SpawnActor<ACombatUnitCharacter>(FVector(1000,300*I,100),FRotator::ZeroRotator,P);
				auto* Data=NewObject<UCombatUnitData>(Hero); Data->DefinitionName=FName(FString::Printf(TEXT("augment_hero_%d"),I));
				if(!Hero->InitializeFromUnitData(Data)) return false;
				if(!Hero->HasActorBegunPlay()) Hero->DispatchBeginPlay();
				if(!PC->SetCommandedUnitAuthority(Hero) || !Director->RegisterPlayer(PC,Hero)) return false;
				auto* Rules=NewObject<UCombatEconomyData>(PC); Rules->DefinitionName=TEXT("choice_rules"); Rules->StartingGold=250; Rules->PassiveGoldPerMinute=0;
				auto* Shop=NewObject<UCombatShopData>(PC); Shop->DefinitionName=TEXT("choice_shop");
				auto* Item=NewObject<UCombatItemData>(Shop); Item->DefinitionName=TEXT("choice_leaf"); Item->PurchasePrice=10; Item->bPurchasable=true;
				auto& Category=Shop->Categories.AddDefaulted_GetRef(); Category.CategoryId=TEXT("basic"); Category.DisplayName=FText::FromString(TEXT("基础")); Category.Items.Add(Item);
				FString Error; if(!PC->GetCombatEconomyComponent()->InitializeForMatch(Rules,Shop,false,Error)) return false;
				Players.Add(PC);
			}
			return true;
		}
		/** 构造当前候选对应的完整服务器意图。 */
		FDefenseChoiceRequest Request(int32 Id=1, EDefenseChoiceAction Action=EDefenseChoiceAction::Select)
		{
			FDefenseAugmentOffer Offer; Director->GetOrCreateAugmentOffer(Players[0],Offer);
			FDefenseChoiceRequest R; R.RequestId=Id; R.RunId=State->GetRunView().RunId; R.ChoiceRevision=Offer.ChoiceRevision;
			R.Action=Action; R.LifeGeneration=Players[0]->GetCommandedUnit()->GetLifeGeneration(); R.BindingGeneration=Players[0]->GetCommandBindingGeneration();
			R.EconomyRevision=Players[0]->GetCombatEconomyComponent()->GetEconomyRevision();
			if(Action==EDefenseChoiceAction::Select && !Offer.Candidates.IsEmpty()) R.DefinitionId=Offer.Candidates[0];
			return R;
		}
	};
}

/** 全部配置上限必须由运行时校验，而不只由 Details 元数据限制。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDefenseAugmentBoundsTest,"Combat.Defense.Augment.DataBounds",DefenseAugmentTests::Flags)
bool FDefenseAugmentBoundsTest::RunTest(const FString&)
{
	DefenseAugmentTests::FFixture F; FString Error;
	TestTrue(TEXT("Valid pool"),F.Pool->ValidateRuntime(Error));
	F.Pool->Augments[0]->Weight=100001;
	TestFalse(TEXT("Overweight rejected"),F.Pool->ValidateRuntime(Error));
	return true;
}
/** 同一加入席位的候选不能因为另一名玩家先请求而改变。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDefenseAugmentOrderTest,"Combat.Defense.Augment.OtherPlayerRequestOrderIsolation",DefenseAugmentTests::Flags)
bool FDefenseAugmentOrderTest::RunTest(const FString&)
{
	DefenseAugmentTests::FFixture A,B; if(!A.Start() || !B.Start()) return false;
	FDefenseAugmentOffer A0,A1,B0,B1;
	A.Director->GetOrCreateAugmentOffer(A.Players[0],A0); A.Director->GetOrCreateAugmentOffer(A.Players[1],A1);
	B.Director->GetOrCreateAugmentOffer(B.Players[1],B1); B.Director->GetOrCreateAugmentOffer(B.Players[0],B0);
	TestTrue(TEXT("Player zero reproducible across request order"),A0.Candidates==B0.Candidates);
	TestTrue(TEXT("Player one reproducible across request order"),A1.Candidates==B1.Candidates);
	return true;
}
/** 选择请求按 ChoiceRevision/RequestId 幂等，旧修订和伪造 DefinitionId 无副作用。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDefenseAugmentChoiceIdempotencyTest,"Combat.Defense.Augment.ChoiceRevisionIdempotency",DefenseAugmentTests::Flags)
bool FDefenseAugmentChoiceIdempotencyTest::RunTest(const FString&)
{
	DefenseAugmentTests::FFixture F; if(!F.Start()) return false;
	FDefenseAugmentOffer Offer; TestTrue(TEXT("Create offer"),F.Director->GetOrCreateAugmentOffer(F.Players[0],Offer));
	if(!TestTrue(TEXT("Offer has candidates"),Offer.Candidates.Num()>0)) return false;
	const FDefenseChoiceResult First=F.Director->ChooseAugment(F.Players[0],1001,Offer.ChoiceRevision,Offer.Candidates[0]);
	TestTrue(TEXT("First choice accepted"),First.bAccepted);
	const FDefenseChoiceResult Repeat=F.Director->ChooseAugment(F.Players[0],1001,Offer.ChoiceRevision,Offer.Candidates[0]);
	TestTrue(TEXT("Repeated request returns original result"),Repeat.bAccepted && Repeat.ChosenDefinition==First.ChosenDefinition);
	const FDefenseChoiceResult Old=F.Director->ChooseAugment(F.Players[0],1002,Offer.ChoiceRevision,Offer.Candidates[0]);
	TestFalse(TEXT("Resolved offer rejects second request"),Old.bAccepted);
	const FDefenseChoiceResult Invalid=F.Director->ChooseAugment(F.Players[1],1003,Offer.ChoiceRevision,FPrimaryAssetId(TEXT("DefenseAugment"),TEXT("forged")));
	TestFalse(TEXT("Forged DefinitionId rejected"),Invalid.bAccepted);
	return true;
}

/** 接入可玩选择前，完整请求的重放不能悄悄接受不同卡。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDefenseChoicePayloadTest,"Combat.Defense.Choice.ReplayPayloadAndEffect",DefenseAugmentTests::Flags)
bool FDefenseChoicePayloadTest::RunTest(const FString&)
{
	DefenseAugmentTests::FFixture F;
	for(UDefenseAugmentData* Card:F.Pool->Augments)
	{
		auto* Modifier=NewObject<UCombatModifierData>(Card);
		Modifier->DefinitionName=FName(Card->DefinitionName.ToString()+TEXT("_effect"));
		Modifier->bRemoveOnDeath=false;
		FCombatModifierAttributeChange Change; Change.Attribute=UCombatAttributeSet::GetArmorAttribute(); Change.Magnitude=5;
		Modifier->AttributeChanges.Add(Change); Card->ModifierData=Modifier;
	}
	if(!F.Start()) return false;
	FDefenseAugmentOffer Offer; F.Director->GetOrCreateAugmentOffer(F.Players[0],Offer);
	if(Offer.Candidates.Num()<2) return false;
	auto* Hero=F.Players[0]->GetCommandedUnit(); const float Before=Hero->GetCombatAttributeSet()->GetArmor();
	const auto Accepted=F.Director->ChooseAugment(F.Players[0],3001,Offer.ChoiceRevision,Offer.Candidates[0]);
	TestTrue(TEXT("Valid choice accepted"),Accepted.bAccepted);
	TestEqual(TEXT("Choice applies public Modifier"),Hero->GetCombatAttributeSet()->GetArmor(),Before+5);
	const auto Forged=F.Director->ChooseAugment(F.Players[0],3001,Offer.ChoiceRevision,Offer.Candidates[1]);
	TestFalse(TEXT("Same request id cannot change its card"),Forged.bAccepted);
	return true;
}

/** 扣款与候选修订必须提交一次，第二次 100，第三次拒绝；重放不二扣。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDefenseRerollTest,"Combat.Defense.Choice.RerollGoldAndReplay",DefenseAugmentTests::Flags)
bool FDefenseRerollTest::RunTest(const FString&)
{
	DefenseAugmentTests::FFixture F; if(!F.Start()) return false;
	auto* PC=F.Players[0]; auto* E=PC->GetCombatEconomyComponent();
	const auto Before=F.Director->BuildChoiceView(PC);
	auto R=F.Request(1,EDefenseChoiceAction::Reroll);
	TestTrue(TEXT("First reroll"),F.Director->ProcessChoice(PC,R).bAccepted);
	TestEqual(TEXT("Price 60"),E->GetGold(),int64(190));
	TestTrue(TEXT("Replay"),F.Director->ProcessChoice(PC,R).bAccepted);
	TestEqual(TEXT("No double debit"),E->GetGold(),int64(190));
	const auto After=F.Director->BuildChoiceView(PC);
	TestEqual(TEXT("Deadline not extended"),After.Offer.Deadline,Before.Offer.Deadline);
	for(const auto& Id:After.Offer.Candidates) TestFalse(TEXT("Old candidates excluded"),Before.Offer.Candidates.Contains(Id));
	R=F.Request(2,EDefenseChoiceAction::Reroll); TestTrue(TEXT("Second reroll"),F.Director->ProcessChoice(PC,R).bAccepted);
	TestEqual(TEXT("Price 100"),E->GetGold(),int64(90));
	R=F.Request(3,EDefenseChoiceAction::Reroll); TestFalse(TEXT("Third rejected"),F.Director->ProcessChoice(PC,R).bAccepted);
	TestEqual(TEXT("Rejected leaves balance"),E->GetGold(),int64(90));
	return true;
}
/** 没有替代卡、旧经济修订和余额不足不消耗次数/金币。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDefenseRerollFailureTest,"Combat.Defense.Choice.RerollRejectNoMutation",DefenseAugmentTests::Flags)
bool FDefenseRerollFailureTest::RunTest(const FString&)
{
	{ DefenseAugmentTests::FFixture F; F.Pool->Augments.SetNum(3); if(!F.Start()) return false;
	  const auto R=F.Request(1,EDefenseChoiceAction::Reroll); const auto Result=F.Director->ProcessChoice(F.Players[0],R);
	  TestEqual(TEXT("No alternative"),Result.FailureReason,FName(TEXT("NoAlternative")));
	  TestEqual(TEXT("Gold unchanged"),F.Players[0]->GetCombatEconomyComponent()->GetGold(),int64(250)); }
	{ DefenseAugmentTests::FFixture F; if(!F.Start()) return false;
	  auto R=F.Request(1,EDefenseChoiceAction::Reroll); --R.EconomyRevision;
	  TestFalse(TEXT("Stale money rejected"),F.Director->ProcessChoice(F.Players[0],R).bAccepted);
	  auto* E=F.Players[0]->GetCombatEconomyComponent(); TestTrue(TEXT("Spend all through public API"),E->TrySpendGold(250,E->GetEconomyRevision(),TEXT("Test")));
	  R=F.Request(2,EDefenseChoiceAction::Reroll); TestFalse(TEXT("Insufficient gold"),F.Director->ProcessChoice(F.Players[0],R).bAccepted);
	  TestEqual(TEXT("No quota consumed"),F.Director->BuildChoiceView(F.Players[0]).RerollsUsed,0); }
	return true;
}
/** 冻结后的源卡修改不影响当前候选或最终属性。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDefenseFrozenPoolTest,"Combat.Defense.Choice.FrozenPoolAndStrictValidation",DefenseAugmentTests::Flags)
bool FDefenseFrozenPoolTest::RunTest(const FString&)
{
	DefenseAugmentTests::FFixture F; if(!F.Start()) return false;
	for(UDefenseAugmentData* Card:F.Pool->Augments) { Card->EarliestWave=5; Card->ModifierData->AttributeChanges[0].Magnitude=999; }
	const auto R=F.Request(); TestTrue(TEXT("Frozen candidate accepted"),F.Director->ProcessChoice(F.Players[0],R).bAccepted);
	TestEqual(TEXT("Frozen magnitude"),F.Players[0]->GetCommandedUnit()->GetCombatAttributeSet()->GetArmor(),5.0f);
	FString Error; F.Pool->Augments[0]->MaxStacks=2; TestFalse(TEXT("Unsupported stack limit"),F.Pool->ValidateRuntime(Error));
	F.Pool->Augments[0]->MaxStacks=1; F.Pool->Augments[0]->ModifierData->bRemoveOnDeath=true;
	TestFalse(TEXT("Death-removing augment rejected"),F.Pool->ValidateRuntime(Error));
	F.Pool->Augments[0]->ModifierData->bRemoveOnDeath=false;
	F.Pool->Augments[0]->ModifierData->AttributeChanges[0].Attribute=UCombatAttributeSet::GetIncomingDamageAttribute();
	TestFalse(TEXT("Meta damage cannot be an augment attribute"),F.Pool->ValidateRuntime(Error)); return true;
}
/** 超时默认卡死亡暂存，复活只生效一次，退出撤销持有句柄。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDefenseChoiceDeathTest,"Combat.Defense.Choice.TimeoutDeathRespawnAndShutdown",DefenseAugmentTests::Flags)
bool FDefenseChoiceDeathTest::RunTest(const FString&)
{
	DefenseAugmentTests::FFixture F; if(!F.Start()) return false; auto* PC=F.Players[0]; auto* H=PC->GetCommandedUnit();
	FCombatDamageRequest Kill; Kill.Source=H; Kill.Target=H; Kill.Amount=100000; Kill.DamageType=ECombatDamageType::Pure;
	F.World.GetWorld()->GetSubsystem<UCombatDamageSubsystem>()->DealDamage(Kill);
	TestFalse(TEXT("Dead cannot choose"),F.Director->ProcessChoice(PC,F.Request()).bAccepted);
	F.World.GetWorld()->TimeSeconds=16; FDefenseChoiceTestAccess::Expire(F.Director,16);
	auto View=F.Director->BuildChoiceView(PC); TestTrue(TEXT("Default selected"),View.Offer.bResolved);
	if(!TestEqual(TEXT("One owned card"),View.Owned.Num(),1)) return false;
	TestTrue(TEXT("Pending while dead"),View.Owned[0].bPending); TestEqual(TEXT("No dead effect"),H->GetCombatModifierComponent()->GetActiveModifierCount(),0);
	TestTrue(TEXT("Respawn public lifecycle"),H->GetCombatLifecycleComponent()->RespawnAtLocation(FVector(1000,300,100)));
	FDefenseChoiceTestAccess::Service(F.Director,16); FDefenseChoiceTestAccess::Service(F.Director,17);
	TestEqual(TEXT("Effect once"),H->GetCombatAttributeSet()->GetArmor(),5.0f);
	F.Director->Shutdown(); F.Director->Shutdown(); TestEqual(TEXT("Effect removed"),H->GetCombatAttributeSet()->GetArmor(),0.0f);
	return true;
}
/** 没有合法卡提供一次金币补给；错误身份及过期选择永远不产生效果。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDefenseChoiceIdentityTest,"Combat.Defense.Choice.SupplyIdentityExpiry",DefenseAugmentTests::Flags)
bool FDefenseChoiceIdentityTest::RunTest(const FString&)
{
	{ DefenseAugmentTests::FFixture F; for(UDefenseAugmentData* A:F.Pool->Augments) A->EarliestWave=5;
	  if(!F.Start()) return false; const auto R=F.Request();
	  TestTrue(TEXT("Supply fallback"),F.Director->BuildChoiceView(F.Players[0]).Offer.bSupply);
	  TestTrue(TEXT("Supply claimed"),F.Director->ProcessChoice(F.Players[0],R).bAccepted);
	  F.Director->ProcessChoice(F.Players[0],R); TestEqual(TEXT("Supply paid once"),F.Players[0]->GetCombatEconomyComponent()->GetGold(),int64(330)); }
	{ DefenseAugmentTests::FFixture F; if(!F.Start()) return false;
	  auto R=F.Request(); R.RunId=FGuid::NewGuid(); TestFalse(TEXT("Old run"),F.Director->ProcessChoice(F.Players[0],R).bAccepted);
	  R=F.Request(2); ++R.LifeGeneration; TestFalse(TEXT("Old life"),F.Director->ProcessChoice(F.Players[0],R).bAccepted);
	  R=F.Request(3); ++R.BindingGeneration; TestFalse(TEXT("Old binding"),F.Director->ProcessChoice(F.Players[0],R).bAccepted);
	  R=F.Request(4); F.World.GetWorld()->TimeSeconds=16; TestFalse(TEXT("Deadline"),F.Director->ProcessChoice(F.Players[0],R).bAccepted); }
	return true;
}
/** 有界缓存淘汰后的旧请求也拒绝，不因缓存删除恢复操作权限。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDefenseChoiceCacheTest,"Combat.Defense.Choice.BoundedCacheAndRateBudget",DefenseAugmentTests::Flags)
bool FDefenseChoiceCacheTest::RunTest(const FString&)
{
	DefenseAugmentTests::FFixture F; if(!F.Start()) return false;
	for(int32 I=1;I<=50;++I) { auto R=F.Request(I); ++R.ChoiceRevision; F.Director->ProcessChoice(F.Players[0],R); }
	TestEqual(TEXT("Cache bounded"),FDefenseChoiceTestAccess::Cached(F.Director,F.Players[0]),32);
	TestFalse(TEXT("Evicted old id"),F.Director->ProcessChoice(F.Players[0],F.Request(1)).bAccepted);
	auto* C=NewObject<UDefenseChoiceComponent>(F.Players[0]); F.Players[0]->AddInstanceComponent(C); C->RegisterComponent();
	auto* Security=F.World.GetWorld()->GetSubsystem<UCombatNetworkSecuritySubsystem>(); FGameplayTag Failure;
	Security->BurstCapacity=2;
	TestFalse(TEXT("Other connection rejected"),Security->ValidateAndConsumeComponentRequest(F.Players[1],C,100,false,Failure));
	TestTrue(TEXT("First envelope"),Security->ValidateAndConsumeComponentRequest(F.Players[0],C,100,false,Failure));
	TestTrue(TEXT("Cached replay consumes token"),Security->ValidateAndConsumeComponentRequest(F.Players[0],C,100,true,Failure));
	TestFalse(TEXT("Replay rate limit"),Security->ValidateAndConsumeComponentRequest(F.Players[0],C,100,true,Failure));
	return true;
}
/** 本地开发开局严格解析并锁种子；启动后再写种子拒绝。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDefenseSeedLockTest,"Combat.Defense.Choice.SeedParsingAndLock",DefenseAugmentTests::Flags)
bool FDefenseSeedLockTest::RunTest(const FString&)
{
	uint64 Value=0; TestTrue(TEXT("Hex"),UDefenseDirectorComponent::ParseSeed(TEXT("0xABCD"),Value)); TestEqual(TEXT("Hex value"),Value,uint64(43981));
	for(const auto* Invalid:{TEXT("0"),TEXT("-1"),TEXT("18446744073709551616"),TEXT("1.5"),TEXT("0xGG")}) TestFalse(TEXT("Reject malformed"),UDefenseDirectorComponent::ParseSeed(Invalid,Value));
	DefenseAugmentTests::FFixture F; if(!F.Start(true)) return false; FString Error;
	TestTrue(TEXT("Waiting start"),F.State->GetRunView().bWaitingForStart);
	TestTrue(TEXT("Start locks"),F.Director->StartWithSeed(TEXT("0xABCD"),Error)); TestEqual(TEXT("Locked value"),F.Director->GetRunSeedForTesting(),uint64(43981));
	TestFalse(TEXT("Cannot rewrite"),F.Director->StartWithSeed(TEXT("123"),Error)); return true;
}
/** UI 根由两个实际面板组成，重建/释放不持有 gameplay 状态。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDefenseChoiceWidgetTest,"Combat.Defense.Choice.WidgetStructure",DefenseAugmentTests::Flags)
bool FDefenseChoiceWidgetTest::RunTest(const FString&)
{
	auto* W=NewObject<UDefenseBuildWidget>(); W->Initialize(); auto Slate=W->TakeWidget();
	TestTrue(TEXT("Choice panel exists"),W->ChoicePanel.IsValid()); TestTrue(TEXT("Owned panel exists"),W->OwnedPanel.IsValid());
	TestEqual(TEXT("No unsolicited card without owner"),W->ChoicePanel->GetVisibility(),EVisibility::Collapsed);
	W->ReleaseSlateResources(true); TestFalse(TEXT("Slate released"),W->ChoicePanel.IsValid()); return true;
}

/** 容器顺序、英雄门槛、前置和双向互斥不会被偏好槽绕过。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDefenseSamplerRulesTest,"Combat.Defense.Choice.SamplerRulesAndOrdering",DefenseAugmentTests::Flags)
bool FDefenseSamplerRulesTest::RunTest(const FString&)
{
	DefenseAugmentTests::FFixture F;
	const FPrimaryAssetId Hero(TEXT("CombatUnit"),TEXT("sampler_hero"));
	TSet<FPrimaryAssetId> Owned;
	const auto First=FDefenseOfferSampler::Draw(*F.Pool,831907,0,0,1,Hero,Owned,{},false);
	Algo::Reverse(F.Pool->Augments);
	const auto Reordered=FDefenseOfferSampler::Draw(*F.Pool,831907,0,0,1,Hero,Owned,{},false);
	TestTrue(TEXT("Definition order stable"),First==Reordered);
	UDefenseAugmentData* A=F.Pool->Augments[0]; UDefenseAugmentData* B=F.Pool->Augments[1];
	A->Rarity=EDefenseAugmentRarity::Final; A->Family=EDefenseAugmentFamily::Guardian;
	TestFalse(TEXT("No early final"),FDefenseOfferSampler::IsEligible(*A,*F.Pool,Hero,0,Owned));
	B->Family=A->Family; F.Pool->Augments[2]->Family=A->Family;
	Owned.Add(B->GetPrimaryAssetId()); Owned.Add(F.Pool->Augments[2]->GetPrimaryAssetId());
	TestTrue(TEXT("Two family cards unlock final after wave three"),FDefenseOfferSampler::IsEligible(*A,*F.Pool,Hero,2,Owned));
	B->MutuallyExclusive.Add(A->GetPrimaryAssetId());
	TestFalse(TEXT("Reverse mutual exclusion"),FDefenseOfferSampler::IsEligible(*A,*F.Pool,Hero,2,Owned));
	B->MutuallyExclusive.Reset(); A->CompatibleHeroes.Add(FPrimaryAssetId(TEXT("CombatUnit"),TEXT("another_hero")));
	TestFalse(TEXT("Hero compatibility"),FDefenseOfferSampler::IsEligible(*A,*F.Pool,Hero,2,Owned));
	A->CompatibleHeroes.Reset(); A->Prerequisites.Add(F.Pool->Augments[3]->GetPrimaryAssetId());
	TestFalse(TEXT("Missing prerequisite"),FDefenseOfferSampler::IsEligible(*A,*F.Pool,Hero,2,Owned));
	return true;
}
/** 核心终局取消未决候选，旧请求不能复活一个选择窗口。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDefenseChoiceTerminalTest,"Combat.Defense.Choice.TerminalCancelsOffer",DefenseAugmentTests::Flags)
bool FDefenseChoiceTerminalTest::RunTest(const FString&)
{
	DefenseAugmentTests::FFixture F; if(!F.Start()) return false;
	const auto R=F.Request();
	FCombatDamageRequest Kill; Kill.Source=F.Core; Kill.Target=F.Core; Kill.Amount=100000; Kill.DamageType=ECombatDamageType::Pure;
	F.World.GetWorld()->GetSubsystem<UCombatDamageSubsystem>()->DealDamage(Kill);
	TestTrue(TEXT("Offer cancelled"),F.Director->BuildChoiceView(F.Players[0]).Offer.bCancelled);
	TestFalse(TEXT("Old request after terminal"),F.Director->ProcessChoice(F.Players[0],R).bAccepted);
	FDefenseChoiceTestAccess::Expire(F.Director,100);
	TestEqual(TEXT("No default after defeat"),F.Director->BuildChoiceView(F.Players[0]).Owned.Num(),0);
	DefenseAugmentTests::FFixture Waiting; if(!Waiting.Start(true)) return false;
	Kill.Source=Waiting.Core; Kill.Target=Waiting.Core;
	Waiting.World.GetWorld()->GetSubsystem<UCombatDamageSubsystem>()->DealDamage(Kill);
	TestFalse(TEXT("Pre-start defeat dismisses start panel"),Waiting.State->GetRunView().bWaitingForStart);
	return true;
}
#endif
