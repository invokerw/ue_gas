#include "CoreMinimal.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Combat/Tests/CombatAutomationWorldFixture.h"
#include "Combat/Items/CombatItemData.h"
#include "Combat/Items/CombatInventoryComponent.h"
#include "Combat/Items/CombatItemSubsystem.h"
#include "Combat/Unit/CombatUnitCharacter.h"
#include "Combat/Items/CombatItemAcquireEffect.h"
#include "Combat/Economy/CombatEconomyComponent.h"
#include "Combat/Economy/CombatEconomyData.h"
#include "Combat/Economy/CombatShopData.h"
#include "Combat/Ability/CombatAbilitySystemComponent.h"
#include "Combat/Log/CombatEventSubsystem.h"
#include "CombatPlayerController.h"
#include "Combat/Core/CombatTags.h"
#include "Combat/Combat/CombatDamageSubsystem.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatAcquireGroundTest,"Combat.Items.Acquire.GroundUsesWithoutSlot",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCombatAcquireGroundTest::RunTest(const FString&)
{
	FCombatAutomationWorldFixture World; auto* W=World.GetWorld(); auto* Unit=W->SpawnActor<ACombatUnitCharacter>();
	auto* Data=NewObject<UCombatUnitData>(Unit); Data->DefinitionName=TEXT("acquire_hero"); Data->BaseStats.MaxHealth=500;
	TestTrue(TEXT("Initialize"),Unit->InitializeFromUnitData(Data)); if(!Unit->HasActorBegunPlay()) Unit->DispatchBeginPlay();
	auto* Item=NewObject<UCombatItemData>(); Item->DefinitionName=TEXT("acquire_potion"); Item->bUseOnAcquire=true;
	Item->AcquireEffect=NewObject<UCombatHealOnAcquireEffect>(Item);
	FCombatDamageRequest Damage; Damage.Source=Unit; Damage.Target=Unit; Damage.Amount=200; Damage.DamageType=ECombatDamageType::Pure;
	W->GetSubsystem<UCombatDamageSubsystem>()->DealDamage(Damage);
	auto* Items=W->GetSubsystem<UCombatItemSubsystem>(); const auto Handle=Items->SpawnItem(Item,1,Unit->GetActorLocation()+FVector(50,0,0));
	TestTrue(TEXT("Spawn acquire item"),Handle.IsValid()); FGameplayTag Failure;
	TestTrue(TEXT("Ground acquisition succeeds"),Unit->GetCombatInventoryComponent()->TryPickup(Handle,1,Failure));
	TestEqual(TEXT("One heal"),Unit->GetCombatAttributeSet()->GetHealth(),400.0f);
	TestEqual(TEXT("No slot"),Unit->GetCombatInventoryComponent()->GetItemCount(),0);
	TestEqual(TEXT("No transient item remains"),Items->GetInstanceCount(),0); return true;
}

namespace AcquireTests
{
	/** 真正九槽库存与金币夹具；所有获取均走公开入口，默认无被动回血/收入。 */
	struct FFixture
	{
		FCombatAutomationWorldFixture World;
		ACombatPlayerController* PC;
		ACombatUnitCharacter* Hero;
		UCombatItemData* Potion;
		UCombatItemData* Leaf;
		UCombatShopData* Shop;
		FFixture()
		{
			auto* W=World.GetWorld(); PC=W->SpawnActor<ACombatPlayerController>(); Hero=W->SpawnActor<ACombatUnitCharacter>();
			auto* D=NewObject<UCombatUnitData>(Hero); D->DefinitionName=TEXT("acquire_fixture"); D->BaseStats.MaxHealth=500; D->BaseStats.HealthRegen=0;
			Hero->InitializeFromUnitData(D); if(!Hero->HasActorBegunPlay()) Hero->DispatchBeginPlay(); PC->SetCommandedUnitAuthority(Hero);
			Shop=NewObject<UCombatShopData>(PC); Shop->DefinitionName=TEXT("acquire_shop");
			Potion=NewObject<UCombatItemData>(Shop); Potion->DefinitionName=TEXT("acquire_heal_item"); Potion->bUseOnAcquire=true; Potion->bPurchasable=true; Potion->PurchasePrice=100;
			Potion->AcquireEffect=NewObject<UCombatHealOnAcquireEffect>(Potion);
			Leaf=NewObject<UCombatItemData>(Shop); Leaf->DefinitionName=TEXT("acquire_leaf"); Leaf->bPurchasable=true; Leaf->PurchasePrice=1;
			auto& Cat=Shop->Categories.AddDefaulted_GetRef(); Cat.CategoryId=TEXT("items"); Cat.DisplayName=FText::FromString(TEXT("物品")); Cat.Items={Potion,Leaf};
		}
		/** 资源初始化失败必须作为断言，不能让测试空跑。 */
		bool Start(FAutomationTestBase& Test,bool Full=false)
		{
			auto* Rules=NewObject<UCombatEconomyData>(PC); Rules->DefinitionName=TEXT("acquire_rules"); Rules->StartingGold=500; Rules->PassiveGoldPerMinute=0;
			FString Error; if(!Test.TestTrue(TEXT("Initialize economy"),PC->GetCombatEconomyComponent()->InitializeForMatch(Rules,Shop,false,Error))) {Test.AddError(Error);return false;}
			if(Full) for(int32 I=0;I<9;++I) { FCombatItemHandle H; FGameplayTag F; if(!Test.TestTrue(TEXT("Fill inventory"),Hero->GetCombatInventoryComponent()->GiveItem(Leaf,1,H,F))) return false; }
			return true;
		}
		/** 构造普通经济 RPC 的完整意图。 */
		FCombatEconomyRequest Request(int32 Id=1) const
		{
			FCombatEconomyRequest R; R.RequestId=Id; R.ItemDefinitionId=Potion->GetPrimaryAssetId(); R.CommandBindingGeneration=PC->GetCommandBindingGeneration();
			R.ExpectedEconomyRevision=PC->GetCombatEconomyComponent()->GetEconomyRevision(); R.ExpectedInventoryRevision=Hero->GetCombatInventoryComponent()->GetRevision(); return R;
		}
		/** 通过公共 Damage 制造需要治疗的生命差额。 */
		void Damage(float Amount)
		{
			FCombatDamageRequest D; D.Source=Hero; D.Target=Hero; D.Amount=Amount; D.DamageType=ECombatDamageType::Pure;
			World.GetWorld()->GetSubsystem<UCombatDamageSubsystem>()->DealDamage(D);
		}
	};
	constexpr auto Flags=EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatAcquireFullTest,"Combat.Items.Acquire.FullInventoryPurchasePickupGrant",AcquireTests::Flags)
bool FCombatAcquireFullTest::RunTest(const FString&)
{
	AcquireTests::FFixture F; if(!F.Start(*this,true)) return false; F.Damage(300); auto* Inv=F.Hero->GetCombatInventoryComponent(); auto* Items=F.World.GetWorld()->GetSubsystem<UCombatItemSubsystem>();
	const int32 Revision=Inv->GetRevision(); const auto Result=F.PC->ProcessEconomyRequestForConnection(F.PC,F.Request());
	TestTrue(TEXT("Purchase consumed"),Result.bSuccess&&Result.bConsumedOnAcquire&&!Result.ItemHandle.IsValid());
	TestEqual(TEXT("Purchase gold"),F.PC->GetCombatEconomyComponent()->GetGold(),int64(400));
	const auto Ground=Items->SpawnItem(F.Potion,1,F.Hero->GetActorLocation()+FVector(50,0,0)); FGameplayTag Failure; FCombatItemHandle Out;
	TestTrue(TEXT("Full inventory pickup"),Inv->TryPickup(Ground,1,Failure));
	TestFalse(TEXT("Old ground handle cannot be claimed twice"),Inv->TryPickup(Ground,1,Failure));
	TestTrue(TEXT("Full inventory reward"),Inv->GiveItem(F.Potion,1,Out,Failure)); TestFalse(TEXT("No returned live handle"),Out.IsValid());
	TestEqual(TEXT("Three actual heals"),F.Hero->GetCombatAttributeSet()->GetHealth(),500.0f);
	TestEqual(TEXT("Inventory unchanged"),Inv->GetRevision(),Revision); TestEqual(TEXT("Exactly nine existing instances"),Items->GetInstanceCount(),9); return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatAcquireFailureTest,"Combat.Items.Acquire.FailureRetainsGoldAndGround",AcquireTests::Flags)
bool FCombatAcquireFailureTest::RunTest(const FString&)
{
	AcquireTests::FFixture F; if(!F.Start(*this)) return false; F.Damage(200);
	F.Hero->GetCombatAbilitySystemComponent()->AddLooseGameplayTag(CombatTags::State_OutOfGame);
	const int32 Before=F.PC->GetCombatEconomyComponent()->GetEconomyRevision(); const auto R=F.PC->ProcessEconomyRequestForConnection(F.PC,F.Request());
	TestFalse(TEXT("Heal execution fails"),R.bSuccess); TestEqual(TEXT("No gold charged"),F.PC->GetCombatEconomyComponent()->GetGold(),int64(500));
	TestEqual(TEXT("Resource revision restored"),F.PC->GetCombatEconomyComponent()->GetEconomyRevision(),Before);
	auto* Items=F.World.GetWorld()->GetSubsystem<UCombatItemSubsystem>(); const auto Ground=Items->SpawnItem(F.Potion,1,F.Hero->GetActorLocation()+FVector(50,0,0)); FGameplayTag Failure;
	TestFalse(TEXT("Invalid state pickup fails"),F.Hero->GetCombatInventoryComponent()->TryPickup(Ground,1,Failure)); TestNotNull(TEXT("Ground instance retained"),Items->FindItem(Ground));
	F.Hero->GetCombatAbilitySystemComponent()->RemoveLooseGameplayTag(CombatTags::State_OutOfGame);
	TestTrue(TEXT("Can pick up when state recovers"),F.Hero->GetCombatInventoryComponent()->TryPickup(Ground,1,Failure)); TestEqual(TEXT("No leaked failed purchase"),Items->GetInstanceCount(),0); return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatAcquireReplayTest,"Combat.Items.Acquire.OriginalRPCReplayAndPayload",AcquireTests::Flags)
bool FCombatAcquireReplayTest::RunTest(const FString&)
{
	AcquireTests::FFixture F; if(!F.Start(*this)) return false; F.Damage(200); const auto Request=F.Request();
	const auto First=F.PC->ProcessEconomyRequestForConnection(F.PC,Request); const auto Replay=F.PC->ProcessEconomyRequestForConnection(F.PC,Request);
	TestTrue(TEXT("Receipt replay"),First.bSuccess&&Replay.bSuccess&&Replay.bConsumedOnAcquire); TestEqual(TEXT("Original gold delta"),Replay.GoldDelta,int64(-100));
	auto Changed=Request; ++Changed.ExpectedInventoryRevision; TestFalse(TEXT("Changed payload"),F.PC->ProcessEconomyRequestForConnection(F.PC,Changed).bSuccess);
	TestEqual(TEXT("One charge"),F.PC->GetCombatEconomyComponent()->GetGold(),int64(400)); TestEqual(TEXT("One heal"),F.Hero->GetCombatAttributeSet()->GetHealth(),400.0f);
	AcquireTests::FFixture G; if(!G.Start(*this)) return false;
	TestFalse(TEXT("Other connection rejected"),F.PC->ProcessEconomyRequestForConnection(G.PC,F.Request(2)).bSuccess); return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatAcquireReentrantTest,"Combat.Items.Acquire.ReentrantGrantAndDefinitionBounds",AcquireTests::Flags)
bool FCombatAcquireReentrantTest::RunTest(const FString&)
{
	AcquireTests::FFixture F; if(!F.Start(*this)) return false; F.Damage(200); bool Called=false,Nested=false;
	auto* Events=F.World.GetWorld()->GetSubsystem<UCombatEventSubsystem>();
	const auto Delegate=Events->OnRecord().AddLambda([&](const FCombatLogRecord& R)
	{
		if(Called||R.EventType!=CombatTags::Event_Combat_HealApplied) return; Called=true; FCombatItemHandle H; FGameplayTag Failure;
		Nested=F.Hero->GetCombatInventoryComponent()->GiveItem(F.Potion,1,H,Failure);
	});
	FCombatItemHandle H; FGameplayTag Failure; TestTrue(TEXT("Outer reward succeeds"),F.Hero->GetCombatInventoryComponent()->GiveItem(F.Potion,1,H,Failure)); Events->OnRecord().Remove(Delegate);
	TestTrue(TEXT("Real heal observer executed"),Called); TestFalse(TEXT("Nested reward rejected"),Nested);
	TestEqual(TEXT("Single heal and no instances"),F.World.GetWorld()->GetSubsystem<UCombatItemSubsystem>()->GetInstanceCount(),0);
	FString Error; F.Potion->bSellable=true; TestFalse(TEXT("Cannot sell consumed-on-acquire item"),F.Potion->ValidateRuntime(Error)); F.Potion->bSellable=false;
	F.Potion->MaxStack=2; TestFalse(TEXT("Batch acquisition unsupported"),F.Potion->ValidateRuntime(Error)); F.Potion->MaxStack=1;
	TestTrue(TEXT("Valid after restoring constraints"),F.Potion->ValidateRuntime(Error)); return true;
}
#endif
