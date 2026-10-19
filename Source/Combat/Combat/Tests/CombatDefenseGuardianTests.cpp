#include "CoreMinimal.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Combat/Defense/DefenseGuardian.h"
#include "Combat/Defense/DefenseCoreShieldRuntime.h"
#include "Combat/Defense/DefenseGameMode.h"
#include "Combat/Ability/CombatAbilitySystemComponent.h"
#include "Combat/Attributes/CombatAttributeSet.h"
#include "Combat/Combat/CombatDamageSubsystem.h"
#include "Combat/Combat/CombatEffectUtilities.h"
#include "Combat/Attack/CombatAttackComponent.h"
#include "Combat/Core/CombatTags.h"
#include "Combat/Modifiers/CombatModifierComponent.h"
#include "Combat/Scheduling/CombatSchedulerSubsystem.h"
#include "Combat/Unit/CombatUnitLifecycleComponent.h"
#include "Combat/Tests/CombatAutomationWorldFixture.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/BoxComponent.h"
#include "Engine/World.h"

namespace GuardianTests
{
	constexpr auto Flags=EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter;
	/** 加载正式定义，缺失时测试报错而不制造替代资产。 */
	template<class T> T* Asset(const TCHAR* Name)
	{ return LoadObject<T>(nullptr,*FString::Printf(TEXT("/Game/Combat/Demo/Defense/Guardian/%s.%s"),Name,Name)); }
	/** 用真实技能组创建守卫；只在测试实例关闭恢复，隔离属性差值，不修改资产或CDO。 */
	ACombatUnitCharacter* Hero(UWorld& W,FVector Location=FVector::ZeroVector)
	{
		auto* Original=Asset<UCombatUnitData>(TEXT("DA_DefenseGuardian")); if(!Original) return nullptr;
		FActorSpawnParameters P; P.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		auto* U=W.SpawnActor<ACombatUnitCharacter>(Location,FRotator::ZeroRotator,P);
		auto* Data=DuplicateObject<UCombatUnitData>(Original,U); Data->BaseStats.HealthRegen=Data->BaseStats.ManaRegen=0;
		if(!U->InitializeFromUnitData(Data)) return nullptr;
		if(!U->HasActorBegunPlay()) U->DispatchBeginPlay(); U->GetCharacterMovement()->GravityScale=0; return U;
	}
	/** 可控队伍/位置/生命的靶子，属性通过初始化公共入口建立。 */
	ACombatUnitCharacter* Enemy(UWorld& W,FVector Location=FVector(200,0,0),bool Boss=false,uint8 Team=2)
	{
		FActorSpawnParameters P; P.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		auto* U=W.SpawnActor<ACombatUnitCharacter>(Location,FRotator::ZeroRotator,P); auto* D=NewObject<UCombatUnitData>(U);
		D->DefinitionName=Boss?TEXT("defense_boss"):TEXT("guardian_target"); D->InitialTeamId=FCombatTeamId(Team);
		D->BaseStats.MaxHealth=10000; D->BaseStats.Armor=0; D->BaseStats.HealthRegen=0; D->BaseStats.AttackDamage=10; D->BaseStats.BaseAttackTime=.1f;
		D->BaseStats.AttackRange=300; D->BaseAttackPoint=0; D->AttackFacingToleranceDegrees=180;
		U->InitializeFromUnitData(D); if(!U->HasActorBegunPlay()) U->DispatchBeginPlay(); U->GetCharacterMovement()->GravityScale=0; return U;
	}
	/** 可控游戏时钟运行真实Scheduler；无需墙钟等待。 */
	void Advance(UWorld& W,float Seconds)
	{ for(int32 I=0;I<FMath::CeilToInt(Seconds/.05f);++I) { W.Tick(LEVELTICK_All,.05f); W.GetSubsystem<UCombatSchedulerSubsystem>()->RunDueTasks(W.GetTimeSeconds()); } }
	/** 使用实际已授予的Spec发起施法，保持成本/冷却/生命周期。 */
	bool Cast(ACombatUnitCharacter* H,const TCHAR* Name,bool Point=false)
	{
		if(!H) return false; auto* ASC=H->GetCombatAbilitySystemComponent(); const auto* S=ASC->FindCombatAbilitySpecByDefinitionId(FPrimaryAssetId(TEXT("CombatAbility"),Name)); if(!S) return false;
		FGameplayTag Failure; FCombatAbilityTargetData T; T.bHasTargetLocation=Point; T.TargetLocation=H->GetActorLocation()+FVector(350,0,0);
		return ASC->TryActivateCombatAbility(S->Handle,T,Failure);
	}
	/** 只按真实View中的定义找到本服务器Runtime，用于核对余量和到期。 */
	UCombatModifierRuntime* Effect(ACombatUnitCharacter* U,const TCHAR* Name)
	{
		TArray<FCombatModifierView> Views; U->GetCombatModifierComponent()->BuildModifierViews(Views);
		for(const auto& V:Views) if(V.DefinitionId==FPrimaryAssetId(TEXT("CombatModifier"),Name)) return U->GetCombatModifierComponent()->FindRuntime(V.Handle);
		return nullptr;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGuardianAssetsTest,"Combat.Defense.Guardian.AuthoredPackage",GuardianTests::Flags)
bool FGuardianAssetsTest::RunTest(const FString&)
{
	auto* Data=GuardianTests::Asset<UCombatUnitData>(TEXT("DA_DefenseGuardian")); auto* Set=GuardianTests::Asset<UCombatAbilitySet>(TEXT("DA_GuardianAbilitySet"));
	if(!TestNotNull(TEXT("Real hero definition"),Data)||!TestNotNull(TEXT("Real ability set"),Set)) return false;
	TestEqual(TEXT("Guardian identity"),Data->GetPrimaryAssetId(),FPrimaryAssetId(TEXT("CombatUnit"),TEXT("defense_guardian")));
	TestEqual(TEXT("Health1400"),Data->BaseStats.MaxHealth,1400.f); TestEqual(TEXT("Mana240"),Data->BaseStats.MaxMana,240.f);
	TestNull(TEXT("Melee attack has no projectile"),Data->AttackProjectileData.Get()); TestEqual(TEXT("Four abilities"),Set->Abilities.Num(),4);
	int32 Active=0,Passive=0; for(const auto& Entry:Set->Abilities)
	{
		const auto* A=Entry.AbilityClass?Entry.AbilityClass->GetDefaultObject<UCombatGameplayAbility>()->GetAbilityData():nullptr;
		if(!TestNotNull(TEXT("Blueprint Class has exact data"),A)) continue;
		FString Error; TestTrue(TEXT("Valid data"),A->ValidateRuntime(Error)); TestEqual(TEXT("Initial skill level1"),Entry.InitialLevel,1);
		if(A->ShouldOccupyPlayerAbilitySlot()) ++Active; else ++Passive;
	}
	TestEqual(TEXT("QWE slots"),Active,3); TestEqual(TEXT("Passive hidden fromR"),Passive,1); return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGuardianBashTest,"Combat.Defense.Guardian.BashGeometryLevelsAndCancellation",GuardianTests::Flags)
bool FGuardianBashTest::RunTest(const FString&)
{
	for(int32 Level=1;Level<=3;++Level)
	{
		FCombatAutomationWorldFixture F; auto& W=*F.GetWorld(); auto* H=GuardianTests::Hero(W); if(!TestNotNull(TEXT("Hero"),H)) return false;
		auto* Front=GuardianTests::Enemy(W); auto* Behind=GuardianTests::Enemy(W,FVector(-200,0,0)); auto* Side=GuardianTests::Enemy(W,FVector(80,300,0)); auto* Friend=GuardianTests::Enemy(W,FVector(200,100,0),false,1);
		auto* ASC=H->GetCombatAbilitySystemComponent(); auto* Spec=ASC->FindCombatAbilitySpecByDefinitionId(FPrimaryAssetId(TEXT("CombatAbility"),TEXT("guardian_shield_bash"))); FGameplayTag Failure;
		if(!TestNotNull(TEXT("Actual grantedQ"),Spec)) return false;
		TestTrue(TEXT("Set valid level"),ASC->SetCombatAbilityLevel(Spec->Handle,Level,Failure));
		TestTrue(TEXT("Q initial activation"),GuardianTests::Cast(H,TEXT("guardian_shield_bash"),true)); GuardianTests::Advance(W,.35f);
		TestEqual(TEXT("Physical damage matches level"),10000-Front->GetCombatAttributeSet()->GetHealth(),float(120+60*Level));
		for(auto* U:{Behind,Side,Friend}) TestEqual(TEXT("Outside cone or friendly untouched"),U->GetCombatAttributeSet()->GetHealth(),10000.f);
		TestEqual(TEXT("One mana cost"),H->GetCombatAttributeSet()->GetMana(),210.f); TestFalse(TEXT("Cooldown rejects replay"),GuardianTests::Cast(H,TEXT("guardian_shield_bash"),true));
	}
	FCombatAutomationWorldFixture F; auto& W=*F.GetWorld(); auto* H=GuardianTests::Hero(W); auto* Target=GuardianTests::Enemy(W);
	auto* ASC=H->GetCombatAbilitySystemComponent(); auto* Spec=ASC->FindCombatAbilitySpecByDefinitionId(FPrimaryAssetId(TEXT("CombatAbility"),TEXT("guardian_shield_bash")));
	TestTrue(TEXT("Begin cancelable cast"),GuardianTests::Cast(H,TEXT("guardian_shield_bash"),true)); ASC->CancelAbilityHandle(Spec->Handle); GuardianTests::Advance(W,1);
	TestEqual(TEXT("Canceled before commit costs nothing"),H->GetCombatAttributeSet()->GetMana(),240.f); TestEqual(TEXT("Old cast callback cannot damage"),Target->GetCombatAttributeSet()->GetHealth(),10000.f);
	CombatEffectUtilities::ApplyAttributeAdditive(ASC,*ASC,UCombatAttributeSet::GetManaAttribute(),-240);
	TestFalse(TEXT("Insufficient mana rejected"),GuardianTests::Cast(H,TEXT("guardian_shield_bash"),true)); return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGuardianShieldTest,"Combat.Defense.Guardian.FortifyActualAbsorptionCapAndExpiry",GuardianTests::Flags)
bool FGuardianShieldTest::RunTest(const FString&)
{
	FCombatAutomationWorldFixture F; auto& W=*F.GetWorld(); auto* H=GuardianTests::Hero(W); if(!H) return false; auto* Enemy=GuardianTests::Enemy(W);
	TestTrue(TEXT("Actual W"),GuardianTests::Cast(H,TEXT("guardian_fortify"))); GuardianTests::Advance(W,.2f);
	auto* Shield=Cast<UDefenseShieldRuntime>(GuardianTests::Effect(H,TEXT("guardian_fortify_shield"))); if(!TestNotNull(TEXT("Shield runtime"),Shield)) return false;
	TestEqual(TEXT("Twenty percent of maxHP"),Shield->GetRemainingShield(),280.f);
	FCombatDamageRequest Hit; Hit.Source=Enemy; Hit.Target=H; Hit.Amount=100; Hit.DamageType=ECombatDamageType::Pure;
	const float HP=H->GetCombatAttributeSet()->GetHealth(); auto Result=W.GetSubsystem<UCombatDamageSubsystem>()->DealDamage(Hit);
	TestEqual(TEXT("Shield absorbs real damage"),Result.Event.AbsorbedAmount,100.f); TestEqual(TEXT("No health loss"),H->GetCombatAttributeSet()->GetHealth(),HP);
	auto* Fixed=NewObject<UCombatModifierData>(H); Fixed->DefinitionName=TEXT("guardian_cap_fixture"); Fixed->RuntimeClass=UDefenseShieldRuntime::StaticClass(); Fixed->Duration=3; Fixed->RuntimeParameters.Add(TEXT("shield_amount"),1000);
	FCombatModifierApplyRequest R; R.Source=H; R.ModifierData=Fixed; const auto Added=H->GetCombatModifierComponent()->ApplyModifier(R);
	auto* Other=Cast<UDefenseShieldRuntime>(H->GetCombatModifierComponent()->FindRuntime(Added.Handle)); if(!TestNotNull(TEXT("Same-family shield"),Other)) return false;
	TestEqual(TEXT("Combined remaining shield cap40percent"),Other->GetRemainingShield()+Shield->GetRemainingShield(),560.f);
	GuardianTests::Advance(W,3.1f); TestNull(TEXT("W expired through scheduler"),GuardianTests::Effect(H,TEXT("guardian_fortify_shield")));
	Hit.Amount=10; const auto After=W.GetSubsystem<UCombatDamageSubsystem>()->DealDamage(Hit); TestEqual(TEXT("After expiry health takes actual damage"),After.Event.AppliedAmount,10.f); return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGuardianStaggerTest,"Combat.Defense.Guardian.SlamAndCooperativeStagger",GuardianTests::Flags)
bool FGuardianStaggerTest::RunTest(const FString&)
{
	FCombatAutomationWorldFixture F; auto& W=*F.GetWorld(); auto* Boss=GuardianTests::Enemy(W,FVector(220,0,0),true); auto* Normal=GuardianTests::Enemy(W,FVector(220,100,0));
	for(int32 I=0;I<4;++I)
	{
		auto* H=GuardianTests::Hero(W,FVector(-100,I*80,0)); if(!H) return false;
		TestTrue(TEXT("Different guardian casts E"),GuardianTests::Cast(H,TEXT("guardian_ground_slam"))); GuardianTests::Advance(W,.4f);
		if(I==0)
		{
			TestTrue(TEXT("Ordinary target stunned"),Normal->GetCombatAbilitySystemComponent()->HasMatchingGameplayTag(CombatTags::State_Stunned));
			TestFalse(TEXT("Boss not hard stunned by single E"),Boss->GetCombatAbilitySystemComponent()->HasMatchingGameplayTag(CombatTags::State_Stunned));
			auto* Stagger=GuardianTests::Effect(Boss,TEXT("defense_boss_stagger")); if(!TestNotNull(TEXT("Boss stack visible"),Stagger)) return false; TestEqual(TEXT("Exactly one layer per cast"),Stagger->GetStackCount(),1);
		}
	}
	TestTrue(TEXT("Four sources share one stagger threshold"),Boss->GetCombatAbilitySystemComponent()->HasMatchingGameplayTag(CombatTags::State_Stunned));
	TestNotNull(TEXT("Protection installed"),GuardianTests::Effect(Boss,TEXT("defense_boss_stagger_recovery")));
	GuardianTests::Advance(W,2.1f); TestFalse(TEXT("Two second stun expires"),Boss->GetCombatAbilitySystemComponent()->HasMatchingGameplayTag(CombatTags::State_Stunned));
	auto* H=GuardianTests::Hero(W,FVector(0,-100,0)); TestTrue(TEXT("Cast during protection"),GuardianTests::Cast(H,TEXT("guardian_ground_slam"))); GuardianTests::Advance(W,.4f);
	TestNull(TEXT("Protection removes attempted layers"),GuardianTests::Effect(Boss,TEXT("defense_boss_stagger")));
	GuardianTests::Advance(W,6); TestNull(TEXT("Protection expires"),GuardianTests::Effect(Boss,TEXT("defense_boss_stagger_recovery"))); return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGuardianWindowTest,"Combat.Defense.Guardian.StaggerWindowAndVisibility",GuardianTests::Flags)
bool FGuardianWindowTest::RunTest(const FString&)
{
	FCombatAutomationWorldFixture F; auto& W=*F.GetWorld(); auto* Boss=GuardianTests::Enemy(W,FVector(220,0,0),true);
	auto* A=GuardianTests::Hero(W); auto* B=GuardianTests::Hero(W,FVector(0,100,0)); if(!A||!B) return false;
	GuardianTests::Cast(A,TEXT("guardian_ground_slam")); GuardianTests::Advance(W,.4f); auto* First=GuardianTests::Effect(Boss,TEXT("defense_boss_stagger")); if(!First) return false;
	const double End=First->GetExpireAt(); GuardianTests::Advance(W,5); GuardianTests::Cast(B,TEXT("guardian_ground_slam")); GuardianTests::Advance(W,.4f);
	TestTrue(TEXT("Second source does not extend first deadline"),FMath::IsNearlyEqual(First->GetExpireAt(),End,.001)); GuardianTests::Advance(W,3);
	TestNull(TEXT("Fixed window expired"),GuardianTests::Effect(Boss,TEXT("defense_boss_stagger")));
	auto* H=GuardianTests::Hero(W,FVector(0,-300,0)); auto* Hidden=GuardianTests::Enemy(W,FVector(200,-300,0));
	auto* Wall=W.SpawnActor<AActor>(); auto* Box=NewObject<UBoxComponent>(Wall); Wall->SetRootComponent(Box); Box->SetBoxExtent(FVector(20,80,200));
	Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly); Box->SetCollisionResponseToAllChannels(ECR_Ignore); Box->SetCollisionResponseToChannel(ECC_GameTraceChannel4,ECR_Block); Box->RegisterComponent(); Wall->SetActorLocation(FVector(100,-300,0));
	GuardianTests::Cast(H,TEXT("guardian_ground_slam")); GuardianTests::Advance(W,.4f);
	TestEqual(TEXT("LOS blocks area damage"),Hidden->GetCombatAttributeSet()->GetHealth(),10000.f); TestFalse(TEXT("LOS blocks control"),Hidden->GetCombatAbilitySystemComponent()->HasMatchingGameplayTag(CombatTags::State_Stunned)); return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGuardianPassiveTest,"Combat.Defense.Guardian.PassiveRealAttacksCooldownAndLifecycle",GuardianTests::Flags)
bool FGuardianPassiveTest::RunTest(const FString&)
{
	FCombatAutomationWorldFixture F; auto& W=*F.GetWorld(); auto* H=GuardianTests::Hero(W); if(!H) return false; auto* E=GuardianTests::Enemy(W,FVector(100,0,0));
	auto* Passive=Cast<UDefenseGuardianPassiveRuntime>(GuardianTests::Effect(H,TEXT("guardian_resolve_intrinsic"))); if(!TestNotNull(TEXT("Intrinsic installed"),Passive)) return false;
	const auto Old=Passive->GetHandle();
	for(int32 I=0;I<3;++I) { TestTrue(TEXT("Real incoming attack starts"),E->GetCombatAttackComponent()->StartMeleeAttack(H,{}).bSuccess); GuardianTests::Advance(W,.25f); }
	TestTrue(TEXT("Three direct attacks arm passive"),Passive->IsArmed());
	TestTrue(TEXT("Actual guardian basic attack"),H->GetCombatAttackComponent()->StartMeleeAttack(E,{}).bSuccess); GuardianTests::Advance(W,.4f);
	auto* Shield=Cast<UDefenseShieldRuntime>(GuardianTests::Effect(H,TEXT("guardian_resolve_shield"))); if(!TestNotNull(TEXT("Next attack produces shield"),Shield)) return false;
	TestEqual(TEXT("Five percent shield"),Shield->GetRemainingShield(),70.f); TestFalse(TEXT("Single use consumed"),Passive->IsArmed());
	for(int32 I=0;I<3;++I) { E->GetCombatAttackComponent()->StartMeleeAttack(H,{}); GuardianTests::Advance(W,.25f); }
	TestEqual(TEXT("ICD ignores further attacks"),Passive->GetReceivedHits(),0);
	GuardianTests::Advance(W,4); FCombatDamageRequest Hit; Hit.Source=E; Hit.Target=H; Hit.Amount=1; Hit.DamageType=ECombatDamageType::Pure;
	Hit.SourceContext.DirectSourceType=ECombatDirectSourceType::Modifier; W.GetSubsystem<UCombatDamageSubsystem>()->DealDamage(Hit); TestEqual(TEXT("Derived damage ignored"),Passive->GetReceivedHits(),0);
	Hit.SourceContext.DirectSourceType=ECombatDirectSourceType::Attack; Hit.ParentEvent=W.GetSubsystem<UCombatEventSubsystem>()->CreateRootEvent();
	for(int32 I=0;I<3;++I) W.GetSubsystem<UCombatDamageSubsystem>()->DealDamage(Hit);
	TestEqual(TEXT("Same root counts only once"),Passive->GetReceivedHits(),1);
	auto* Broken=NewObject<UCombatModifierData>(H); Broken->DefinitionName=TEXT("guardian_break_fixture"); Broken->GrantedTags.AddTag(CombatTags::State_Broken); Broken->Duration=1;
	FCombatModifierApplyRequest R; R.Source=E; R.ModifierData=Broken; H->GetCombatModifierComponent()->ApplyModifier(R); Hit.ParentEvent={}; W.GetSubsystem<UCombatDamageSubsystem>()->DealDamage(Hit);
	TestEqual(TEXT("Break disables counter"),Passive->GetReceivedHits(),1);
	H->GetCombatLifecycleComponent()->RequestDeath(W.GetSubsystem<UCombatEventSubsystem>()->CreateRootEvent(),E);
	TestNull(TEXT("Old passive handle removed at death"),H->GetCombatModifierComponent()->FindRuntime(Old));
	TestTrue(TEXT("Public respawn"),H->GetCombatLifecycleComponent()->RespawnAtLocation(FVector::ZeroVector));
	auto* Fresh=Cast<UDefenseGuardianPassiveRuntime>(GuardianTests::Effect(H,TEXT("guardian_resolve_intrinsic"))); if(!TestNotNull(TEXT("Intrinsic rebuilt"),Fresh)) return false;
	TestTrue(TEXT("New lifetime starts empty"),!Fresh->IsArmed()&&Fresh->GetReceivedHits()==0&&!(Fresh->GetHandle()==Old));
	H->Destroy(); TestEqual(TEXT("EndPlay releases all modifiers"),H->GetCombatModifierComponent()->GetActiveModifierCount(),0); return true;
}
#endif
