#include "CoreMinimal.h"

#include <limits>

#if WITH_DEV_AUTOMATION_TESTS

#include "AbilitySystemGlobals.h"
#include "Abilities/GameplayAbility.h"
#include "AssetRegistry/AssetData.h"
#include "Engine/AssetManager.h"
#include "Engine/CollisionProfile.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "Misc/PackageName.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"

#include "Combat/Ability/CombatAbilitySystemComponent.h"
#include "Combat/Ability/CombatGameplayEffectContext.h"
#include "Combat/Core/CombatDeferredOperationQueue.h"
#include "Combat/Core/CombatNumericPolicy.h"
#include "Combat/Core/CombatRngSubsystem.h"
#include "Combat/Core/CombatTags.h"
#include "Combat/Data/CombatDefinitionData.h"
#include "Combat/Log/CombatEventSubsystem.h"
#include "Combat/Scheduling/CombatSchedulerSubsystem.h"
#include "Combat/Targeting/CombatTeamSubsystem.h"
#include "Combat/Tests/CombatAutomationWorldFixture.h"
#include "Combat/Unit/CombatUnitCharacter.h"

namespace CombatFoundationTests
{
	/** Combat 基座自动化统一使用 EditorContext 与 EngineFilter。 */
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;

	/** 验证 fixture 已创建真实 World；失败时把统一诊断写入当前测试。 */
	bool RequireWorld(FAutomationTestBase& Test, const FCombatAutomationWorldFixture& Fixture)
	{
		if (Fixture.IsValid())
		{
			return true;
		}
		Test.AddError(TEXT("Combat automation world fixture could not create a UWorld"));
		return false;
	}
}

/** 验证 Native GameplayTag 与 Combat Collision Channel/Profile 的冻结配置。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCombatConfigurationTest,
	"Combat.Foundation.Configuration.TagsAndCollision",
	CombatFoundationTests::Flags)

/** 执行标签唯一性、Damage.Type 约束和碰撞矩阵断言。 */
bool FCombatConfigurationTest::RunTest(const FString& Parameters)
{
	using namespace CombatTags;
	const FGameplayTag NativeTags[] = {
		State_Alive, State_Dying, State_Dead, State_Respawning, State_Stunned, State_Silenced,
		State_Rooted, State_Disarmed, State_Hexed, State_Invisible, State_Invulnerable,
		State_OutOfGame, State_MagicImmune, State_Untargetable, State_NoUnitCollision,
		State_NoHealthBar, State_Frozen, Ability_Behavior_NoTarget, Ability_Behavior_UnitTarget,
		Ability_Behavior_PointTarget, Ability_Behavior_Passive, Ability_Behavior_Channelled,
		Ability_Behavior_AoE, Ability_Behavior_Attack, Ability_Behavior_AutoCast,
		Ability_Behavior_IgnoreSilence, Ability_Behavior_IgnoreMagicImmune,
		Ability_Behavior_IgnoreUntargetable, TargetTeam_None, TargetTeam_Enemy,
		TargetTeam_Friendly, TargetTeam_Both, Damage_Type_Physical, Damage_Type_Magical,
		Damage_Type_Pure, Damage_Flag_BypassMagicImmune, Damage_Flag_HPLoss,
		Damage_Flag_NoSpellAmplification, Damage_Flag_Reflection, Damage_Flag_NoLifesteal,
		Damage_Flag_NoCrit, Data_Damage_Final, Data_Heal_Final, Cue_Combat,
		Event_Combat_DamageApplied, Event_Combat_HealApplied, Event_Combat_ModifierApplied,
		Event_Combat_ModifierRemoved, Event_Combat_UnitDeath,
		Event_Combat_UnitRespawned, Event_Combat_TeamChanged, Event_Combat_AbilityGranted,
		Event_Combat_AbilityRemoved, Event_Combat_AbilityLevelChanged, Event_Combat_AutoCastChanged,
		Failure_Authority, Failure_InvalidNumber, Failure_ActionUnsupported, Failure_Target_Invalid,
		Failure_Target_TeamInvalid, Failure_Target_SelfNotAllowed, Failure_Target_FriendlyNotAllowed,
		Failure_Target_HostileNotAllowed, Failure_Target_NeutralNotAllowed, Failure_Target_Dying,
		Failure_Target_Dead, Failure_Target_Respawning, Failure_Target_Untargetable,
		Failure_Target_OutOfGame, Failure_Target_Invulnerable, Failure_Target_MagicImmune,
		Failure_Target_OutOfRange, Failure_Target_LocationInvalid, Failure_Target_LineOfSightBlocked,
		Failure_Life_NotAlive, Failure_Life_InvalidTransition, Failure_Ability_DuplicateDefinition,
		Failure_Ability_InvalidLevel, Failure_Ability_NotGranted, Order_Failure_Cancelled,
		Order_Failure_QueueFull, Order_Failure_PathFailed, Order_Failure_Blocked,
		Order_Failure_AbilityRejected, Order_Failure_UnitStateBlocked, RNG_Attack_Evasion,
		RNG_Attack_Crit, RNG_Modifier_Proc
	};
	for (const FGameplayTag& Tag : NativeTags)
	{
		TestTrue(FString::Printf(TEXT("Native tag is registered: %s"), *Tag.ToString()), Tag.IsValid());
	}

	FGameplayTag DamageType;
	FGameplayTagContainer DamageTags;
	TestFalse(TEXT("Zero damage types is rejected"), TryGetSingleDamageType(DamageTags, DamageType));
	DamageTags.AddTag(Damage_Type_Physical);
	TestTrue(TEXT("Exactly one damage type is accepted"), TryGetSingleDamageType(DamageTags, DamageType));
	TestEqual(TEXT("Accepted damage type is returned"), DamageType, Damage_Type_Physical.GetTag());
	DamageTags.AddTag(Damage_Type_Magical);
	TestFalse(TEXT("Two damage types are rejected"), TryGetSingleDamageType(DamageTags, DamageType));

	const UCollisionProfile* CollisionProfile = UCollisionProfile::Get();
	TestNotNull(TEXT("Collision profile singleton exists"), CollisionProfile);
	if (!CollisionProfile)
	{
		return false;
	}
	TestEqual(TEXT("CombatUnit channel name"),
		CollisionProfile->ReturnChannelNameFromContainerIndex(ECC_GameTraceChannel1), FName(TEXT("CombatUnit")));
	TestEqual(TEXT("CombatProjectile channel name"),
		CollisionProfile->ReturnChannelNameFromContainerIndex(ECC_GameTraceChannel2), FName(TEXT("CombatProjectile")));
	TestEqual(TEXT("CombatBlocker channel name"),
		CollisionProfile->ReturnChannelNameFromContainerIndex(ECC_GameTraceChannel3), FName(TEXT("CombatBlocker")));
	TestEqual(TEXT("CombatTargeting channel name"),
		CollisionProfile->ReturnChannelNameFromContainerIndex(ECC_GameTraceChannel4), FName(TEXT("CombatTargeting")));

	// 用同一个检查器读取真实 Profile，避免仅验证 ini 文本而漏掉引擎解析差异。
	auto CheckResponse = [this, CollisionProfile](const FName ProfileName, const ECollisionChannel Channel,
		const ECollisionResponse Expected)
	{
		FCollisionResponseTemplate Template;
		if (!TestTrue(FString::Printf(TEXT("Profile exists: %s"), *ProfileName.ToString()),
			CollisionProfile->GetProfileTemplate(ProfileName, Template)))
		{
			return;
		}
		TestEqual(FString::Printf(TEXT("%s response to channel %d"), *ProfileName.ToString(), Channel),
			Template.ResponseToChannels.GetResponse(Channel), Expected);
	};

	CheckResponse(TEXT("CombatUnit"), ECC_WorldStatic, ECR_Block);
	CheckResponse(TEXT("CombatUnit"), ECC_GameTraceChannel1, ECR_Block);
	CheckResponse(TEXT("CombatUnit"), ECC_GameTraceChannel2, ECR_Overlap);
	CheckResponse(TEXT("CombatUnit"), ECC_GameTraceChannel3, ECR_Block);
	CheckResponse(TEXT("CombatUnit"), ECC_GameTraceChannel4, ECR_Ignore);
	CheckResponse(TEXT("CombatUnitNoCollision"), ECC_GameTraceChannel1, ECR_Ignore);
	CheckResponse(TEXT("CombatUnitNoCollision"), ECC_GameTraceChannel2, ECR_Overlap);
	CheckResponse(TEXT("CombatProjectile"), ECC_GameTraceChannel1, ECR_Overlap);
	CheckResponse(TEXT("CombatProjectile"), ECC_GameTraceChannel3, ECR_Block);
	CheckResponse(TEXT("CombatBlocker"), ECC_GameTraceChannel1, ECR_Block);
	CheckResponse(TEXT("CombatBlocker"), ECC_GameTraceChannel2, ECR_Block);
	CheckResponse(TEXT("CombatBlocker"), ECC_GameTraceChannel4, ECR_Block);
	CheckResponse(TEXT("CombatCorpse"), ECC_WorldStatic, ECR_Ignore);
	CheckResponse(TEXT("CombatCorpse"), ECC_GameTraceChannel1, ECR_Ignore);
	CheckResponse(TEXT("CombatCorpse"), ECC_GameTraceChannel4, ECR_Ignore);
	return true;
}

/** 验证稳定 DefinitionId、重复身份拒绝和显式 redirect 规则。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCombatDataIdentityTest,
	"Combat.Foundation.Data.IdentityAndRedirects",
	CombatFoundationTests::Flags)

/** 执行命名、资产重命名稳定性、唯一性与 redirect 边界断言。 */
bool FCombatDataIdentityTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("lower_snake_case identity is valid"), UCombatDefinitionData::IsValidDefinitionName(TEXT("basic_unit_2")));
	TestFalse(TEXT("empty identity is invalid"), UCombatDefinitionData::IsValidDefinitionName(NAME_None));
	TestFalse(TEXT("uppercase identity is invalid"), UCombatDefinitionData::IsValidDefinitionName(TEXT("BasicUnit")));
	TestFalse(TEXT("leading digit identity is invalid"), UCombatDefinitionData::IsValidDefinitionName(TEXT("2unit")));

	UCombatUnitData* Unit = NewObject<UCombatUnitData>(GetTransientPackage(), TEXT("TransientUnitAsset"));
	Unit->DefinitionName = TEXT("basic_unit");
	const FPrimaryAssetId BeforeRename = Unit->GetPrimaryAssetId();
	Unit->Rename(TEXT("MovedTransientUnitAsset"), GetTransientPackage(), REN_DontCreateRedirectors);
	TestEqual(TEXT("Asset path/name changes do not change DefinitionId"), Unit->GetPrimaryAssetId(), BeforeRename);
	TestEqual(TEXT("Unit PrimaryAssetType is fixed"), BeforeRename.PrimaryAssetType.ToString(), FString(TEXT("CombatUnit")));

	UCombatUnitData* DuplicateUnit = NewObject<UCombatUnitData>(GetTransientPackage());
	DuplicateUnit->DefinitionName = TEXT("basic_unit");
	TArray<const UCombatDefinitionData*> DuplicateDefinitions = { Unit, DuplicateUnit };
	TArray<FString> Errors;
	TestFalse(TEXT("Duplicate DefinitionId is rejected"),
		UCombatDefinitionData::ValidateDefinitionSet(DuplicateDefinitions, Errors));
	TestTrue(TEXT("Duplicate validation returns a diagnostic"), !Errors.IsEmpty());

	UCombatAbilityData* AbilityA = NewObject<UCombatAbilityData>(GetTransientPackage());
	AbilityA->DefinitionName = TEXT("ability_a");
	UCombatAbilityData* AbilityB = NewObject<UCombatAbilityData>(GetTransientPackage());
	AbilityB->DefinitionName = TEXT("ability_b");
	Errors.Reset();
	TestTrue(TEXT("AbilityData identities do not require a reverse AbilityClass reference"),
		UCombatDefinitionData::ValidateDefinitionSet({ AbilityA, AbilityB }, Errors));

	const FPrimaryAssetType UnitType(TEXT("CombatUnit"));
	const FPrimaryAssetId OldId(UnitType, TEXT("old_unit"));
	const FPrimaryAssetId NewId(UnitType, TEXT("new_unit"));
	const FPrimaryAssetId MissingId(UnitType, TEXT("missing_unit"));
	TSet<FPrimaryAssetId> KnownIds = { NewId };
	FCombatDefinitionRedirect ValidRedirect;
	ValidRedirect.OldId = OldId;
	ValidRedirect.NewId = NewId;
	TArray<FCombatDefinitionRedirect> Redirects = { ValidRedirect };
	Errors.Reset();
	TestTrue(TEXT("One-hop redirect with existing target is valid"),
		FCombatDefinitionRegistry::ValidateRedirects(Redirects, KnownIds, Errors));
	FPrimaryAssetId ResolvedId;
	TestTrue(TEXT("Valid redirect resolves"),
		FCombatDefinitionRegistry::ResolveDefinitionId(OldId, Redirects, KnownIds, ResolvedId));
	TestEqual(TEXT("Redirect resolves to canonical id"), ResolvedId, NewId);
	TestFalse(TEXT("Missing definition does not resolve"),
		FCombatDefinitionRegistry::ResolveDefinitionId(MissingId, Redirects, KnownIds, ResolvedId));
	TestTrue(TEXT("Missing placeholder preserves stable id"),
		FCombatDefinitionRegistry::MakeMissingPlaceholder(MissingId).Contains(MissingId.ToString()));

	FCombatDefinitionRedirect MissingTargetRedirect;
	MissingTargetRedirect.OldId = OldId;
	MissingTargetRedirect.NewId = MissingId;
	Errors.Reset();
	TestFalse(TEXT("Redirect with missing target is rejected"),
		FCombatDefinitionRegistry::ValidateRedirects({ MissingTargetRedirect }, KnownIds, Errors));

	FCombatDefinitionRedirect ChainedRedirect;
	ChainedRedirect.OldId = NewId;
	ChainedRedirect.NewId = FPrimaryAssetId(UnitType, TEXT("final_unit"));
	KnownIds.Add(ChainedRedirect.NewId);
	Errors.Reset();
	TestFalse(TEXT("Redirect chains and cycles are rejected"),
		FCombatDefinitionRegistry::ValidateRedirects({ ValidRedirect, ChainedRedirect }, KnownIds, Errors));
	return true;
}

/** 验证 AssetManager 可冷启动发现 Unit 定义与 M1 测试地图。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCombatContentDiscoveryTest,
	"Combat.Foundation.Content.AssetManagerAndTestMap",
	CombatFoundationTests::Flags)

/** 查询实际 AssetManager 注册表和磁盘包，验证 M1 内容可发现性。 */
bool FCombatContentDiscoveryTest::RunTest(const FString& Parameters)
{
	UAssetManager& AssetManager = UAssetManager::Get();
	const FPrimaryAssetType UnitType(TEXT("CombatUnit"));
	const FPrimaryAssetId TeamOneId(UnitType, TEXT("team_one"));
	const FPrimaryAssetId TeamTwoId(UnitType, TEXT("team_two"));
	FAssetData TeamOneData;
	FAssetData TeamTwoData;
	TestTrue(TEXT("AssetManager discovers CombatUnit:team_one"),
		AssetManager.GetPrimaryAssetData(TeamOneId, TeamOneData));
	TestTrue(TEXT("AssetManager discovers CombatUnit:team_two"),
		AssetManager.GetPrimaryAssetData(TeamTwoId, TeamTwoData));
	TestEqual(TEXT("team_one resolves to the expected asset"),
		TeamOneData.PackageName, FName(TEXT("/Game/Combat/Definitions/Units/DA_CombatUnit_TeamOne")));
	TestEqual(TEXT("team_two resolves to the expected asset"),
		TeamTwoData.PackageName, FName(TEXT("/Game/Combat/Definitions/Units/DA_CombatUnit_TeamTwo")));
	TestTrue(TEXT("L_CombatTest package exists on disk"),
		FPackageName::DoesPackageExist(TEXT("/Game/Combat/Tests/L_CombatTest")));
	return true;
}

/** 验证 Numeric Policy 边界和 Combat RNG v1 的独立冻结向量。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCombatNumericRngTest,
	"Combat.Foundation.NumericRng.PolicyAndReplay",
	CombatFoundationTests::Flags)

/** 执行有限值、Clamp、key 隔离与确定性重放断言。 */
bool FCombatNumericRngTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("Request zero is valid"), FCombatNumericPolicyV1::IsValidNonNegativeRequest(0.0f));
	TestTrue(TEXT("Request upper boundary is valid"),
		FCombatNumericPolicyV1::IsValidNonNegativeRequest(FCombatNumericPolicyV1::MaxAbsoluteValue));
	TestFalse(TEXT("Negative request is rejected"), FCombatNumericPolicyV1::IsValidNonNegativeRequest(-1.0f));
	TestFalse(TEXT("NaN request is rejected"),
		FCombatNumericPolicyV1::IsValidNonNegativeRequest(std::numeric_limits<float>::quiet_NaN()));
	TestFalse(TEXT("Infinite request is rejected"),
		FCombatNumericPolicyV1::IsValidNonNegativeRequest(std::numeric_limits<float>::infinity()));
	TestFalse(TEXT("Over-limit request is rejected rather than clamped"),
		FCombatNumericPolicyV1::IsValidNonNegativeRequest(FCombatNumericPolicyV1::MaxAbsoluteValue * 2.0f));
	TestEqual(TEXT("Health clamps low"), FCombatNumericPolicyV1::ClampHealth(-5.0f, 100.0f), 0.0f);
	TestEqual(TEXT("Health clamps high"), FCombatNumericPolicyV1::ClampHealth(120.0f, 100.0f), 100.0f);
	TestEqual(TEXT("Armor clamps"), FCombatNumericPolicyV1::ClampArmor(20000.0f), 10000.0f);
	TestEqual(TEXT("Magic resistance clamps"), FCombatNumericPolicyV1::ClampMagicResistance(2.0f), 0.95f);
	TestEqual(TEXT("Chance clamps"), FCombatNumericPolicyV1::ClampChance(-0.25f), 0.0f);
	TestEqual(TEXT("Amplification clamps"), FCombatNumericPolicyV1::ClampAmplification(20.0f), 10.0f);
	TestEqual(TEXT("Lifesteal clamps"), FCombatNumericPolicyV1::ClampLifesteal(20.0f), 10.0f);
	TestEqual(TEXT("Reduction clamps"), FCombatNumericPolicyV1::ClampReduction(2.0f), 0.90f);

	FCombatEventId RootEventId;
	RootEventId.Sequence = 42;
	FCombatRngSubjectId SubjectId;
	SubjectId.High = 0x1111222233334444ull;
	SubjectId.Low = 0xaaaabbbbccccddddull;
	const uint64 ExpectedRawBits = 0xadb9a918c54fc21dull;
	const uint64 RawBits = UCombatRngSubsystem::CombatHash64V1(
		0x123456789abcdef0ull, RootEventId, CombatTags::RNG_Attack_Crit, SubjectId, 7);
	TestEqual(TEXT("RNG v1 matches frozen independent vector"), RawBits, ExpectedRawBits);
	TestEqual(TEXT("Same RNG key replays exactly"),
		UCombatRngSubsystem::CombatHash64V1(
			0x123456789abcdef0ull, RootEventId, CombatTags::RNG_Attack_Crit, SubjectId, 7), RawBits);
	TestNotEqual(TEXT("Different domain is isolated"),
		UCombatRngSubsystem::CombatHash64V1(
			0x123456789abcdef0ull, RootEventId, CombatTags::RNG_Attack_Evasion, SubjectId, 7), RawBits);
	TestNotEqual(TEXT("Different ordinal is isolated"),
		UCombatRngSubsystem::CombatHash64V1(
			0x123456789abcdef0ull, RootEventId, CombatTags::RNG_Attack_Crit, SubjectId, 8), RawBits);

	FCombatAutomationWorldFixture Fixture;
	if (!CombatFoundationTests::RequireWorld(*this, Fixture))
	{
		return false;
	}
	UCombatRngSubsystem* Rng = Fixture.GetWorld()->GetSubsystem<UCombatRngSubsystem>();
	TestNotNull(TEXT("RNG subsystem is available"), Rng);
	if (Rng)
	{
		Rng->SetMatchSeedForTesting(0x123456789abcdef0ull);
		FCombatRngRollRecord Record;
		TestTrue(TEXT("Authority world produces a roll"),
			Rng->Roll(RootEventId, CombatTags::RNG_Attack_Crit, SubjectId, 7, 2.0f, Record));
		TestEqual(TEXT("Roll record preserves raw chance"), Record.ChanceRaw, 2.0f);
		TestEqual(TEXT("Roll record clamps chance at consumption"), Record.ChanceClamped, 1.0f);
		TestEqual(TEXT("Roll record preserves raw bits"), Record.RawBits, ExpectedRawBits);
		TestTrue(TEXT("Chance one always succeeds for [0,1) roll"), Record.bSuccess);
	}
	return true;
}

/** 验证自定义 EffectContext、ASC ActorInfo 和 Team 关系基座。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCombatContextAscTeamTest,
	"Combat.Foundation.Runtime.ContextAscAndTeam",
	CombatFoundationTests::Flags)

/** 执行 EffectContext 拷贝/序列化、四种 NetMode ActorInfo 与 Team 规则断言。 */
bool FCombatContextAscTeamTest::RunTest(const FString& Parameters)
{
	FGameplayEffectContext* AllocatedContext = UAbilitySystemGlobals::Get().AllocGameplayEffectContext();
	TestNotNull(TEXT("AbilitySystemGlobals allocates an effect context"), AllocatedContext);
	if (AllocatedContext)
	{
		TestEqual(TEXT("Configured globals allocate the combat context struct"),
			AllocatedContext->GetScriptStruct(), FCombatGameplayEffectContext::StaticStruct());
		delete AllocatedContext;
	}
	TestTrue(TEXT("Combat context opts into copy traits"),
		static_cast<bool>(TStructOpsTypeTraits<FCombatGameplayEffectContext>::WithCopy));
	TestTrue(TEXT("Combat context opts into net serializer traits"),
		static_cast<bool>(TStructOpsTypeTraits<FCombatGameplayEffectContext>::WithNetSerializer));

	FCombatGameplayEffectContext SourceContext;
	SourceContext.EventId.Sequence = 101;
	SourceContext.RootEventId.Sequence = 100;
	SourceContext.AttackHandle.Key = { 9, 2, 3 };
	SourceContext.Source.DirectSourceType = ECombatDirectSourceType::Projectile;
	SourceContext.Source.AbilityDefinitionId = FPrimaryAssetId(TEXT("CombatAbility"), TEXT("fireball"));
	SourceContext.Source.ModifierDefinitionId = FPrimaryAssetId(TEXT("CombatModifier"), TEXT("burn"));
	SourceContext.Source.ProjectileDefinitionId = FPrimaryAssetId(TEXT("CombatProjectile"), TEXT("fireball_projectile"));

	TUniquePtr<FGameplayEffectContext> Duplicated(SourceContext.Duplicate());
	const FCombatGameplayEffectContext* CombatDuplicate = static_cast<const FCombatGameplayEffectContext*>(Duplicated.Get());
	TestEqual(TEXT("Duplicate retains event id"), CombatDuplicate->EventId.Sequence, SourceContext.EventId.Sequence);
	TestTrue(TEXT("Duplicate retains source ids"), CombatDuplicate->Source == SourceContext.Source);
	TestTrue(TEXT("Duplicate retains server-only attack handle"), CombatDuplicate->AttackHandle == SourceContext.AttackHandle);

	TArray<uint8> Bytes;
	FMemoryWriter Writer(Bytes, true);
	bool bWriteSuccess = false;
	TestTrue(TEXT("Combat context serializes"), SourceContext.NetSerialize(Writer, nullptr, bWriteSuccess));
	TestTrue(TEXT("Combat context write reports success"), bWriteSuccess);
	FCombatGameplayEffectContext RoundTripContext;
	FMemoryReader Reader(Bytes, true);
	bool bReadSuccess = false;
	TestTrue(TEXT("Combat context deserializes"), RoundTripContext.NetSerialize(Reader, nullptr, bReadSuccess));
	TestTrue(TEXT("Combat context read reports success"), bReadSuccess);
	TestEqual(TEXT("EventId round-trips"), RoundTripContext.EventId.Sequence, SourceContext.EventId.Sequence);
	TestEqual(TEXT("RootEventId round-trips"), RoundTripContext.RootEventId.Sequence, SourceContext.RootEventId.Sequence);
	TestTrue(TEXT("Source identity round-trips"), RoundTripContext.Source == SourceContext.Source);
	TestFalse(TEXT("AttackHandle intentionally remains server-only"), RoundTripContext.AttackHandle.IsValid());

	for (const ENetMode Mode : { NM_Standalone, NM_ListenServer, NM_DedicatedServer, NM_Client })
	{
		FCombatAutomationWorldFixture Fixture(Mode);
		if (!CombatFoundationTests::RequireWorld(*this, Fixture))
		{
			return false;
		}
		TestEqual(FString::Printf(TEXT("Fixture exposes requested net mode %d"), Mode),
			Fixture.GetWorld()->GetNetMode(), Mode);
		ACombatUnitCharacter* Unit = Fixture.GetWorld()->SpawnActor<ACombatUnitCharacter>();
		TestNotNull(FString::Printf(TEXT("Combat unit spawns in mode %d"), Mode), Unit);
		if (!Unit)
		{
			continue;
		}
		UCombatAbilitySystemComponent* Asc = Unit->GetCombatAbilitySystemComponent();
		TestNotNull(TEXT("Combat unit owns ASC"), Asc);
		if (Asc)
		{
			Asc->InitializeCombatActorInfo(Unit, Unit);
			Asc->InitializeCombatActorInfo(Unit, Unit);
			TestTrue(FString::Printf(TEXT("ActorInfo initializes idempotently in mode %d"), Mode),
				Asc->IsCombatActorInfoInitialized());
			TestEqual(TEXT("ASC owner actor is the unit"), Asc->GetOwnerActor(), static_cast<AActor*>(Unit));
			TestEqual(TEXT("ASC avatar actor is the unit"), Asc->GetAvatarActor(), static_cast<AActor*>(Unit));
		}
		TestEqual(TEXT("Initial life generation is one"), Unit->GetLifeGeneration(), static_cast<uint32>(1));
		TestEqual(TEXT("Initial life state is Alive"), Unit->GetLifeState(), ECombatLifeState::Alive);
		if (Mode != NM_Client)
		{
			TestTrue(FString::Printf(TEXT("Authority can change team in mode %d"), Mode),
				Unit->SetCombatTeamId(FCombatTeamId(2)));
		}
	}

	FCombatAutomationWorldFixture TeamFixture;
	UCombatTeamSubsystem* Teams = TeamFixture.GetWorld()->GetSubsystem<UCombatTeamSubsystem>();
	TestEqual(TEXT("Same team is friendly"), Teams->GetRelation(FCombatTeamId(1), FCombatTeamId(1)),
		ECombatTeamRelation::Friendly);
	TestEqual(TEXT("Different team is hostile"), Teams->GetRelation(FCombatTeamId(1), FCombatTeamId(2)),
		ECombatTeamRelation::Hostile);
	TestEqual(TEXT("Neutral camp defaults hostile to other teams"), Teams->GetRelation(FCombatTeamId(0), FCombatTeamId(1)),
		ECombatTeamRelation::Hostile);
	TestEqual(TEXT("NoTeam is invalid"), Teams->GetRelation(FCombatTeamId(255), FCombatTeamId(1)),
		ECombatTeamRelation::Invalid);
	TestTrue(TEXT("Initial diplomacy override is accepted"),
		Teams->AddInitialRelation(FCombatTeamId(1), FCombatTeamId(2), ECombatTeamRelation::Neutral));
	TestEqual(TEXT("Explicit neutral diplomacy is returned"), Teams->GetRelation(FCombatTeamId(1), FCombatTeamId(2)),
		ECombatTeamRelation::Neutral);
	TestFalse(TEXT("TargetTeam.Both excludes explicit neutral relation"),
		Teams->IsTargetTeamAllowed(FCombatTeamId(1), FCombatTeamId(2), CombatTags::TargetTeam_Both));
	return true;
}

/** 验证 Scheduler 时序、补帧、预算、generation、重入和 teardown。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCombatSchedulerTest,
	"Combat.Foundation.Scheduler.TimingCatchUpBudgetAndTeardown",
	CombatFoundationTests::Flags)

/** 执行 Scheduler 的冻结顺序与完整生命周期边界断言。 */
bool FCombatSchedulerTest::RunTest(const FString& Parameters)
{
	FCombatAutomationWorldFixture Fixture;
	if (!CombatFoundationTests::RequireWorld(*this, Fixture))
	{
		return false;
	}
	UCombatSchedulerSubsystem* Scheduler = Fixture.GetWorld()->GetSubsystem<UCombatSchedulerSubsystem>();
	TestNotNull(TEXT("Scheduler subsystem is available"), Scheduler);
	if (!Scheduler)
	{
		return false;
	}

	UObject* OwnerA = NewObject<UCombatUnitData>(GetTransientPackage());
	OwnerA->AddToRoot();
	UObject* OwnerB = NewObject<UCombatUnitData>(GetTransientPackage());
	OwnerB->AddToRoot();
	TArray<int32> StableOrder;
	Scheduler->ScheduleOnce(OwnerA, 1.0, 0,
		FCombatScheduledDelegate::CreateLambda([&StableOrder](const FCombatScheduledTickContext&) { StableOrder.Add(1); }));
	Scheduler->ScheduleOnce(OwnerA, 1.0, 10,
		FCombatScheduledDelegate::CreateLambda([&StableOrder](const FCombatScheduledTickContext&) { StableOrder.Add(2); }));
	Scheduler->ScheduleOnce(OwnerA, 1.0, 10,
		FCombatScheduledDelegate::CreateLambda([&StableOrder](const FCombatScheduledTickContext&) { StableOrder.Add(3); }));
	TestEqual(TEXT("Nothing fires before absolute due time"), Scheduler->RunDueTasks(0.99), 0);
	TestEqual(TEXT("All simultaneous tasks fire"), Scheduler->RunDueTasks(1.0), 3);
	TestEqual(TEXT("Stable ordering count"), StableOrder.Num(), 3);
	if (StableOrder.Num() == 3)
	{
		TestEqual(TEXT("Higher priority fires first"), StableOrder[0], 2);
		TestEqual(TEXT("Apply sequence breaks equal-priority tie"), StableOrder[1], 3);
		TestEqual(TEXT("Lower priority fires last"), StableOrder[2], 1);
	}

	TArray<FCombatScheduledTickContext> ExecuteAllTicks;
	FCombatScheduleHandle ExecuteAllHandle = Scheduler->ScheduleRepeating(OwnerA, 1.0, 1.0, 0,
		ECombatCatchUpPolicy::ExecuteAllBounded,
		FCombatScheduledDelegate::CreateLambda([&ExecuteAllTicks](const FCombatScheduledTickContext& Context)
		{
			ExecuteAllTicks.Add(Context);
		}));
	Scheduler->MaxCatchUpCallbacksPerTask = 8;
	TestEqual(TEXT("ExecuteAllBounded emits each overdue logical tick"), Scheduler->RunDueTasks(4.4), 4);
	TestEqual(TEXT("ExecuteAllBounded tick count"), ExecuteAllTicks.Num(), 4);
	if (ExecuteAllTicks.Num() == 4)
	{
		TestEqual(TEXT("Repeating schedule does not drift"), ExecuteAllTicks[3].ScheduledTime, 4.0);
		TestEqual(TEXT("Logical tick index advances"), ExecuteAllTicks[3].TickIndex, 3);
		TestEqual(TEXT("ExecuteAll tick count is one"), ExecuteAllTicks[3].TickCount, 1);
	}
	Scheduler->Cancel(ExecuteAllHandle);

	FCombatScheduledTickContext CoalescedContext;
	int32 CoalescedCallbacks = 0;
	FCombatScheduleHandle CoalescedHandle = Scheduler->ScheduleRepeating(OwnerA, 1.0, 1.0, 0,
		ECombatCatchUpPolicy::Coalesce,
		FCombatScheduledDelegate::CreateLambda([&](const FCombatScheduledTickContext& Context)
		{
			++CoalescedCallbacks;
			CoalescedContext = Context;
		}));
	TestEqual(TEXT("Coalesce emits one callback"), Scheduler->RunDueTasks(4.4), 1);
	TestEqual(TEXT("Coalesce callback count"), CoalescedCallbacks, 1);
	TestEqual(TEXT("Coalesce reports all covered ticks"), CoalescedContext.TickCount, 4);
	Scheduler->Cancel(CoalescedHandle);

	FCombatScheduledTickContext SkipContext;
	FCombatScheduleHandle SkipHandle = Scheduler->ScheduleRepeating(OwnerA, 1.0, 1.0, 0,
		ECombatCatchUpPolicy::SkipExpired,
		FCombatScheduledDelegate::CreateLambda([&SkipContext](const FCombatScheduledTickContext& Context)
		{
			SkipContext = Context;
		}));
	TestEqual(TEXT("SkipExpired emits one callback"), Scheduler->RunDueTasks(4.4), 1);
	TestEqual(TEXT("SkipExpired reports latest scheduled time"), SkipContext.ScheduledTime, 4.0);
	TestEqual(TEXT("SkipExpired reports latest logical index"), SkipContext.TickIndex, 3);
	TestEqual(TEXT("SkipExpired never aggregates gameplay ticks"), SkipContext.TickCount, 1);
	Scheduler->Cancel(SkipHandle);

	Scheduler->MaxCallbacksPerFrame = 2;
	Scheduler->MaxCallbacksPerOwnerPerFrame = 1;
	int32 BudgetCallbacks = 0;
	for (int32 Index = 0; Index < 2; ++Index)
	{
		Scheduler->ScheduleOnce(OwnerA, 0.0, 0,
			FCombatScheduledDelegate::CreateLambda([&BudgetCallbacks](const FCombatScheduledTickContext&) { ++BudgetCallbacks; }));
	}
	Scheduler->ScheduleOnce(OwnerB, 0.0, 0,
		FCombatScheduledDelegate::CreateLambda([&BudgetCallbacks](const FCombatScheduledTickContext&) { ++BudgetCallbacks; }));
	TestEqual(TEXT("Global and per-owner budgets limit one frame"), Scheduler->RunDueTasks(4.4), 2);
	TestEqual(TEXT("Budget-limited callbacks execute next frame"), Scheduler->RunDueTasks(4.4), 1);
	TestEqual(TEXT("No budget-limited callback is lost"), BudgetCallbacks, 3);

	Scheduler->MaxCallbacksPerFrame = 256;
	Scheduler->MaxCallbacksPerOwnerPerFrame = 64;
	Scheduler->MaxCatchUpCallbacksPerTask = 2;
	int32 PerTaskCallbacks = 0;
	FCombatScheduleHandle PerTaskHandle = Scheduler->ScheduleRepeating(OwnerA, 1.0, 1.0, 0,
		ECombatCatchUpPolicy::ExecuteAllBounded,
		FCombatScheduledDelegate::CreateLambda([&PerTaskCallbacks](const FCombatScheduledTickContext&) { ++PerTaskCallbacks; }));
	TestEqual(TEXT("Per-task catch-up budget limits a frame"), Scheduler->RunDueTasks(4.4), 2);
	TestEqual(TEXT("Overdue logical ticks remain on original timeline"), Scheduler->RunDueTasks(4.4), 2);
	TestEqual(TEXT("Per-task budget preserves all due ticks"), PerTaskCallbacks, 4);
	Scheduler->Cancel(PerTaskHandle);

	int32 CancelCallbacks = 0;
	FCombatScheduleHandle CancelHandle;
	CancelHandle = Scheduler->ScheduleRepeating(OwnerA, 0.0, 1.0, 0,
		ECombatCatchUpPolicy::ExecuteAllBounded,
		FCombatScheduledDelegate::CreateLambda([&](const FCombatScheduledTickContext&)
		{
			++CancelCallbacks;
			Scheduler->Cancel(CancelHandle);
		}));
	Scheduler->RunDueTasks(4.4);
	TestEqual(TEXT("Cancel inside callback invalidates later catch-up callbacks"), CancelCallbacks, 1);

	int32 NestedCallbacks = 0;
	Scheduler->ScheduleOnce(OwnerA, 0.0, 0,
		FCombatScheduledDelegate::CreateLambda([&](const FCombatScheduledTickContext&)
		{
			++NestedCallbacks;
			Scheduler->ScheduleOnce(OwnerA, 0.0, 0,
				FCombatScheduledDelegate::CreateLambda([&NestedCallbacks](const FCombatScheduledTickContext&)
				{
					++NestedCallbacks;
				}));
		}));
	TestEqual(TEXT("Callback-created due task does not synchronously re-enter"), Scheduler->RunDueTasks(4.4), 1);
	TestEqual(TEXT("Deferred due task runs next scheduler round"), Scheduler->RunDueTasks(4.4), 1);
	TestEqual(TEXT("Both nested callbacks execute exactly once"), NestedCallbacks, 2);

	int32 RescheduledCallbacks = 0;
	FCombatScheduleHandle OldHandle = Scheduler->ScheduleOnce(OwnerA, 10.0, 0,
		FCombatScheduledDelegate::CreateLambda([&RescheduledCallbacks](const FCombatScheduledTickContext&) { ++RescheduledCallbacks; }));
	FCombatScheduleHandle NewHandle = Scheduler->Reschedule(OldHandle, 1.0, 0.0);
	TestTrue(TEXT("Reschedule returns a valid new generation"), NewHandle.IsValid());
	TestNotEqual(TEXT("Reschedule increments generation"), NewHandle.Key.Generation, OldHandle.Key.Generation);
	TestFalse(TEXT("Old generation is inactive"), Scheduler->IsHandleActive(OldHandle));
	TestEqual(TEXT("New generation fires once"), Scheduler->RunDueTasks(1.0), 1);
	TestEqual(TEXT("Stale heap node does not duplicate callback"), Scheduler->RunDueTasks(20.0), 0);

	int32 DeadOwnerCallbacks = 0;
	UObject* EphemeralOwner = NewObject<UCombatUnitData>(GetTransientPackage());
	Scheduler->ScheduleOnce(EphemeralOwner, 0.0, 0,
		FCombatScheduledDelegate::CreateLambda([&DeadOwnerCallbacks](const FCombatScheduledTickContext&) { ++DeadOwnerCallbacks; }));
	EphemeralOwner->MarkAsGarbage();
	TestEqual(TEXT("Destroyed owner callback is discarded"), Scheduler->RunDueTasks(20.0), 0);
	TestEqual(TEXT("Destroyed owner callback never executes"), DeadOwnerCallbacks, 0);

	Scheduler->ScheduleOnce(OwnerA, 100.0, 0,
		FCombatScheduledDelegate::CreateLambda([](const FCombatScheduledTickContext&) {}));
	TestEqual(TEXT("Owner teardown clears remaining scheduler slots"), Scheduler->CancelAllForOwner(OwnerA), 1);
	OwnerA->RemoveFromRoot();
	OwnerB->RemoveFromRoot();
	{
		FCombatAutomationWorldFixture TeardownFixture;
		UCombatSchedulerSubsystem* TeardownScheduler = TeardownFixture.GetWorld()->GetSubsystem<UCombatSchedulerSubsystem>();
		UObject* TeardownOwner = NewObject<UCombatUnitData>(GetTransientPackage());
		TeardownOwner->AddToRoot();
		TeardownScheduler->ScheduleOnce(TeardownOwner, 100.0, 0,
			FCombatScheduledDelegate::CreateLambda([](const FCombatScheduledTickContext&) {}));
		TeardownOwner->RemoveFromRoot();
	}
	TestTrue(TEXT("World teardown with an active task completes safely"), true);

	FCombatAutomationWorldFixture ClientFixture(NM_Client);
	UCombatSchedulerSubsystem* ClientScheduler = ClientFixture.GetWorld()->GetSubsystem<UCombatSchedulerSubsystem>();
	UObject* ClientOwner = NewObject<UCombatUnitData>(GetTransientPackage());
	ClientOwner->AddToRoot();
	int32 ClientCallbacks = 0;
	ClientScheduler->ScheduleOnce(ClientOwner, 0.0, 0,
		FCombatScheduledDelegate::CreateLambda([&ClientCallbacks](const FCombatScheduledTickContext&) { ++ClientCallbacks; }));
	TestEqual(TEXT("Pure client does not execute authoritative callbacks"), ClientScheduler->RunDueTasks(100.0), 0);
	TestEqual(TEXT("Pure client callback remains unexecuted"), ClientCallbacks, 0);
	ClientOwner->RemoveFromRoot();
	return true;
}

/** 验证 deferred 操作、typed handle 与结构化事件日志基座。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCombatDeferredEventsTest,
	"Combat.Foundation.Operations.DeferredAndEvents",
	CombatFoundationTests::Flags)

/** 执行嵌套阶段、稳定快照、失败结果和事件深度断言。 */
bool FCombatDeferredEventsTest::RunTest(const FString& Parameters)
{
	FCombatDeferredOperationQueue Queue;
	TArray<int32> Order;
	FCombatEventId EventId;
	EventId.Sequence = 7;
	TestTrue(TEXT("Outer phase begins"), Queue.BeginPhase(TEXT("PreDamage"), EventId));
	TestTrue(TEXT("Nested phase begins"), Queue.BeginPhase(TEXT("BeforeApply"), EventId));
	TestEqual(TEXT("Nested phase depth is tracked"), Queue.GetCurrentContext().Depth, 1);
	Queue.Enqueue([&Order]() { Order.Add(1); });
	Queue.Enqueue([&Queue, &Order]()
	{
		Order.Add(2);
		Queue.Enqueue([&Order]() { Order.Add(3); });
	});
	TestTrue(TEXT("Nested phase ends"), Queue.EndPhase());
	TestEqual(TEXT("Nested end does not flush outer transaction"), Order.Num(), 0);
	TestTrue(TEXT("Outermost phase ends"), Queue.EndPhase());
	TestEqual(TEXT("Outermost end flushes queued and callback-created operations"), Order.Num(), 3);
	if (Order.Num() == 3)
	{
		TestEqual(TEXT("Deferred order 1"), Order[0], 1);
		TestEqual(TEXT("Deferred order 2"), Order[1], 2);
		TestEqual(TEXT("Callback-created operation is deferred, then ordered"), Order[2], 3);
	}
	TestFalse(TEXT("Ending an absent phase safely fails"), Queue.EndPhase());

	const TArray<int32> Source = { 1, 2, 3 };
	TArray<int32> Snapshot = MakeCombatStableSnapshot(Source);
	Snapshot.RemoveAt(0);
	TestEqual(TEXT("Stable snapshot mutation does not affect source"), Source.Num(), 3);

	FCombatModifierHandle DefaultModifierHandle;
	FCombatAttackHandle DefaultAttackHandle;
	FCombatOrderHandle DefaultOrderHandle;
	FCombatProjectileHandle DefaultProjectileHandle;
	FCombatScheduleHandle DefaultScheduleHandle;
	TestFalse(TEXT("Default modifier handle is invalid"), DefaultModifierHandle.IsValid());
	TestFalse(TEXT("Default attack handle is invalid"), DefaultAttackHandle.IsValid());
	TestFalse(TEXT("Default order handle is invalid"), DefaultOrderHandle.IsValid());
	TestFalse(TEXT("Default projectile handle is invalid"), DefaultProjectileHandle.IsValid());
	TestFalse(TEXT("Default schedule handle is invalid"), DefaultScheduleHandle.IsValid());
	FCombatAttackHandle AttackHandle;
	AttackHandle.Key = { 5, 1, 2 };
	TestTrue(TEXT("Attack handle requires and accepts life generation"), AttackHandle.IsValid());
	TestTrue(TEXT("Typed handle ToString includes identity"), AttackHandle.ToString().Contains(TEXT("Id=5")));
	AttackHandle.Key.LifeGeneration = 0;
	TestFalse(TEXT("Attack handle rejects life generation zero"), AttackHandle.IsValid());

	FCombatOperationResult Failure = FCombatOperationResult::Failure(
		CombatTags::Failure_InvalidNumber, TEXT("Requested=NaN"));
	TestFalse(TEXT("Failure result is unsuccessful"), Failure.bSuccess);
	TestEqual(TEXT("Failure result retains stable tag"), Failure.FailureTag, CombatTags::Failure_InvalidNumber.GetTag());
	TestTrue(TEXT("Failure result retains invalid reason"), Failure.Diagnostic.Contains(TEXT("NaN")));

	FCombatAutomationWorldFixture Fixture;
	if (!CombatFoundationTests::RequireWorld(*this, Fixture))
	{
		return false;
	}
	UCombatEventSubsystem* Events = Fixture.GetWorld()->GetSubsystem<UCombatEventSubsystem>();
	FCombatEventContext Root = Events->CreateRootEvent();
	FCombatEventContext Child = Events->CreateChildEvent(Root);
	TestTrue(TEXT("Root event is valid"), Root.IsValid());
	TestTrue(TEXT("Child event is valid"), Child.IsValid());
	TestEqual(TEXT("Child inherits RootEventId"), Child.RootEventId.Sequence, Root.EventId.Sequence);
	TestEqual(TEXT("Child depth increments"), Child.Depth, 1);

	FCombatLogRecord Record;
	Record.Context = Child;
	Record.EventType = CombatTags::Event_Combat_DamageApplied;
	Record.FailureTag = Failure.FailureTag;
	Record.Diagnostic = Failure.Diagnostic;
	Events->Emit(Record);
	TestEqual(TEXT("Structured event is retained"), Events->GetRecentRecords().Num(), 1);
	const FString LogText = Events->GetRecentRecords()[0].ToString();
	TestTrue(TEXT("Structured log includes RootEventId"), LogText.Contains(Child.RootEventId.ToString()));
	TestTrue(TEXT("Structured log includes invalid reason"), LogText.Contains(Failure.Diagnostic));

	Events->MaxDepth = 1;
	TestFalse(TEXT("Event recursion depth is bounded"), Events->CreateChildEvent(Child).IsValid());
	return true;
}

#endif
