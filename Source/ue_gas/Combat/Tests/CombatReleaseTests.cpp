#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Combat/Ability/CombatGameplayAbility.h"
#include "Combat/Aura/CombatAuraSubsystem.h"
#include "Combat/Combat/CombatTransactionSubsystem.h"
#include "Combat/Data/CombatDefinitionData.h"
#include "Combat/Log/CombatEventSubsystem.h"
#include "Combat/Modifiers/CombatModifierComponent.h"
#include "Combat/Modifiers/CombatModifierRuntime.h"
#include "Combat/Release/CombatReleaseContract.h"
#include "Combat/Scheduling/CombatSchedulerSubsystem.h"
#include "Combat/Tests/CombatAutomationWorldFixture.h"
#include "Combat/Thinker/CombatThinkerSubsystem.h"
#include "Combat/Unit/CombatUnitCharacter.h"

namespace CombatReleaseTests
{
	/** M8 候选发布测试统一使用 EditorContext 与 EngineFilter。 */
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;

	/** 在真实测试 World 中创建已经完成 Combat 初始化的服务器单位。 */
	ACombatUnitCharacter* SpawnUnit(UWorld& World, const FName DefinitionName)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		ACombatUnitCharacter* Unit = World.SpawnActor<ACombatUnitCharacter>(FVector::ZeroVector, FRotator::ZeroRotator, Params);
		if (!Unit)
		{
			return nullptr;
		}
		UCombatUnitData* Data = NewObject<UCombatUnitData>(Unit);
		Data->DefinitionName = DefinitionName;
		return Unit->InitializeFromUnitData(Data) ? Unit : nullptr;
	}

	/** 创建无限持续且不改变属性的最小 Modifier 定义。 */
	UCombatModifierData* MakeModifier(UObject& Outer, const FName DefinitionName)
	{
		UCombatModifierData* Data = NewObject<UCombatModifierData>(&Outer);
		Data->DefinitionName = DefinitionName;
		Data->RuntimeClass = UCombatModifierRuntime::StaticClass();
		Data->Duration = 0.0f;
		Data->MaxStacks = 1;
		return Data;
	}

	/** 检查蓝图扩展事件仍存在并保留中文显示名与说明。 */
	bool HasDocumentedBlueprintFunction(
		FAutomationTestBase& Test,
		UClass* OwnerClass,
		const FName FunctionName)
	{
		const UFunction* Function = OwnerClass ? OwnerClass->FindFunctionByName(FunctionName) : nullptr;
		if (!Test.TestNotNull(*FString::Printf(TEXT("%s.%s exists"), *GetNameSafe(OwnerClass), *FunctionName.ToString()), Function))
		{
			return false;
		}
		// 反射元数据只在 Editor 构建中保留；Server/Client 开发目标仍验证公开函数符号存在。
#if WITH_EDITOR
		Test.TestFalse(*FString::Printf(TEXT("%s has Chinese DisplayName"), *FunctionName.ToString()), Function->GetMetaData(TEXT("DisplayName")).IsEmpty());
		Test.TestFalse(*FString::Printf(TEXT("%s has ToolTip"), *FunctionName.ToString()), Function->GetMetaData(TEXT("ToolTip")).IsEmpty());
#endif
		return true;
	}
}

/** 验证代码、资产、公式、随机和事件 schema 共用唯一的 v1 发布契约。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCombatM8ReleaseContractTest,
	"Combat.Release.M8.ContractAndDeferredBoundaries",
	CombatReleaseTests::Flags)

bool FCombatM8ReleaseContractTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	const FCombatReleaseContract Contract = UCombatReleaseContractLibrary::GetCombatReleaseContract();
	FString Error;
	TestTrue(TEXT("Frozen release contract matches code constants"), Contract.IsSelfConsistent(Error));
	TestTrue(TEXT("Consistent contract has no diagnostic"), Error.IsEmpty());
	TestTrue(TEXT("Gameplay remains server authoritative"), Contract.bServerAuthoritativeGameplay);
	TestTrue(TEXT("Projectile prediction is presentation only"), Contract.bProjectileVisualPrediction);
	TestFalse(TEXT("Gameplay rollback is deferred after v1"), Contract.bGameplayRollback);
	TestFalse(TEXT("Deterministic replay is deferred after v1"), Contract.bDeterministicReplay);
	TestFalse(TEXT("Summons and illusions are deferred after v1"), Contract.bSummonsAndIllusions);
	TestFalse(TEXT("Items and economy are deferred after v1"), Contract.bItemsAndEconomy);

	FCombatReleaseContract Drifted = Contract;
	++Drifted.EventSchemaVersion;
	TestFalse(TEXT("Schema drift is rejected"), Drifted.IsSelfConsistent(Error));
	TestTrue(TEXT("Schema drift returns a stable diagnostic"), Error.Contains(TEXT("事件结构版本")));
	return true;
}

/** 验证主要运行时对象的显式退出、过期句柄与 exactly-once 广播共同收敛为零。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCombatM8LifecycleOwnershipTeardownTest,
	"Combat.Release.M8.LifecycleOwnershipAndTeardown",
	CombatReleaseTests::Flags)

bool FCombatM8LifecycleOwnershipTeardownTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FCombatAutomationWorldFixture Fixture(NM_DedicatedServer);
	UWorld* World = Fixture.GetWorld();
	ACombatUnitCharacter* Unit = World ? CombatReleaseTests::SpawnUnit(*World, TEXT("m8_lifecycle_unit")) : nullptr;
	if (!TestNotNull(TEXT("Dedicated lifecycle world exists"), World)
		|| !TestNotNull(TEXT("Lifecycle owner unit exists"), Unit))
	{
		return false;
	}

	UCombatSchedulerSubsystem* Scheduler = World->GetSubsystem<UCombatSchedulerSubsystem>();
	UCombatThinkerSubsystem* Thinkers = World->GetSubsystem<UCombatThinkerSubsystem>();
	UCombatAuraSubsystem* Auras = World->GetSubsystem<UCombatAuraSubsystem>();
	UCombatTransactionSubsystem* Transactions = World->GetSubsystem<UCombatTransactionSubsystem>();
	UCombatEventSubsystem* Events = World->GetSubsystem<UCombatEventSubsystem>();
	if (!TestNotNull(TEXT("Scheduler exists"), Scheduler)
		|| !TestNotNull(TEXT("Thinker registry exists"), Thinkers)
		|| !TestNotNull(TEXT("Aura registry exists"), Auras)
		|| !TestNotNull(TEXT("Transaction registry exists"), Transactions)
		|| !TestNotNull(TEXT("Event subsystem exists"), Events))
	{
		return false;
	}

	int32 CallbackCount = 0;
	const FCombatScheduleHandle Schedule = Scheduler->ScheduleOnce(
		Unit, 100.0, 0,
		FCombatScheduledDelegate::CreateLambda([&CallbackCount](const FCombatScheduledTickContext&) { ++CallbackCount; }));
	TestTrue(TEXT("Owner schedule is registered"), Schedule.IsValid() && Scheduler->IsHandleActive(Schedule));

	UCombatModifierData* ModifierData = CombatReleaseTests::MakeModifier(*Unit, TEXT("m8_lifecycle_modifier"));
	FCombatModifierApplyRequest Apply;
	Apply.Source = Unit;
	Apply.ModifierData = ModifierData;
	const FCombatModifierApplyResult Modifier = Unit->GetCombatModifierComponent()->ApplyModifier(Apply);
	TestTrue(TEXT("Modifier runtime is created"), Modifier.bSuccess);

	int32 ThinkerFinishedCount = 0;
	const FDelegateHandle ThinkerDelegate = Thinkers->OnThinkerFinished().AddLambda(
		[&ThinkerFinishedCount](const FCombatThinkerResult&) { ++ThinkerFinishedCount; });
	FCombatThinkerSpec ThinkerSpec;
	ThinkerSpec.Source = Unit;
	ThinkerSpec.Location = Unit->GetActorLocation();
	ThinkerSpec.InitialDelay = 10.0f;
	ThinkerSpec.PulseInterval = 10.0f;
	ThinkerSpec.Duration = 20.0f;
	ThinkerSpec.bVisualOnly = true;
	const FCombatThinkerResult Thinker = Thinkers->CreateThinker(ThinkerSpec);
	TestTrue(TEXT("Visual-only thinker is registered"), Thinker.bSuccess && Thinkers->IsThinkerActive(Thinker.Handle));

	int32 AuraFinishedCount = 0;
	const FDelegateHandle AuraDelegate = Auras->OnAuraFinished().AddLambda(
		[&AuraFinishedCount](const FCombatAuraResult&) { ++AuraFinishedCount; });
	FCombatAuraSpec AuraSpec;
	AuraSpec.Owner = Unit;
	AuraSpec.Radius = 0.0f;
	AuraSpec.ReconcileInterval = 10.0f;
	AuraSpec.ChildModifierData = CombatReleaseTests::MakeModifier(*Unit, TEXT("m8_lifecycle_aura_child"));
	AuraSpec.ChildDurationOverride = 0.0f;
	const FCombatAuraResult Aura = Auras->StartAura(AuraSpec);
	TestTrue(TEXT("Aura registry owns one active record"), Aura.bSuccess && Auras->IsAuraActive(Aura.Handle));

	const FCombatEventContext TransactionContext = Events->CreateRootEvent();
	TestTrue(TEXT("Transaction slot is opened"), Transactions->BeginSlot(TransactionContext, ECombatTransactionKind::Damage, Unit));
	TestEqual(TEXT("One transaction slot is open"), Transactions->GetOpenSlotCount(), 1);

	TestTrue(TEXT("Modifier explicit exit succeeds"), Unit->GetCombatModifierComponent()->RemoveModifier(Modifier.Handle));
	TestFalse(TEXT("Stale modifier handle cannot settle twice"), Unit->GetCombatModifierComponent()->RemoveModifier(Modifier.Handle));
	TestTrue(TEXT("Thinker explicit exit succeeds"), Thinkers->CancelThinker(Thinker.Handle).bSuccess);
	TestFalse(TEXT("Stale thinker handle cannot finish twice"), Thinkers->CancelThinker(Thinker.Handle).bSuccess);
	TestTrue(TEXT("Aura explicit exit succeeds"), Auras->CancelAura(Aura.Handle).bSuccess);
	TestFalse(TEXT("Stale aura handle cannot finish twice"), Auras->CancelAura(Aura.Handle).bSuccess);
	TestTrue(TEXT("Transaction explicit exit succeeds"), Transactions->CancelSlot(TransactionContext.EventId));
	TestFalse(TEXT("Closed transaction cannot settle twice"), Transactions->CancelSlot(TransactionContext.EventId));
	TestTrue(TEXT("Owner schedule explicit exit succeeds"), Scheduler->Cancel(Schedule));
	TestFalse(TEXT("Stale schedule cannot execute or cancel twice"), Scheduler->Cancel(Schedule));

	TestEqual(TEXT("Thinker finish broadcasts exactly once"), ThinkerFinishedCount, 1);
	TestEqual(TEXT("Aura finish broadcasts exactly once"), AuraFinishedCount, 1);
	TestEqual(TEXT("No scheduled callback executed during cancellation"), CallbackCount, 0);
	TestEqual(TEXT("All modifier runtimes are released"), Unit->GetCombatModifierComponent()->GetActiveModifierCount(), 0);
	TestEqual(TEXT("Thinker registry returns to zero"), Thinkers->GetActiveThinkerCount(), 0);
	TestEqual(TEXT("Aura registry returns to zero"), Auras->GetActiveAuraCount(), 0);
	TestEqual(TEXT("Aura child registry returns to zero"), Auras->GetTotalChildCount(), 0);
	TestEqual(TEXT("Transaction registry returns to zero"), Transactions->GetOpenSlotCount(), 0);
	TestEqual(TEXT("Scheduler releases every lifecycle task"), Scheduler->GetStats().ActiveSlots, 0);

	Thinkers->OnThinkerFinished().Remove(ThinkerDelegate);
	Auras->OnAuraFinished().Remove(AuraDelegate);
	return true;
}

/** 验证新技能依赖的 DataAsset 基类与蓝图扩展事件在候选发布中保持公开。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCombatM8PublicExtensionSurfaceTest,
	"Combat.Release.M8.PublicExtensionSurface",
	CombatReleaseTests::Flags)

bool FCombatM8PublicExtensionSurfaceTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	TestTrue(TEXT("UnitData derives from public definition base"), UCombatUnitData::StaticClass()->IsChildOf(UCombatDefinitionData::StaticClass()));
	TestTrue(TEXT("AbilityData derives from public definition base"), UCombatAbilityData::StaticClass()->IsChildOf(UCombatDefinitionData::StaticClass()));
	TestTrue(TEXT("ModifierData derives from public definition base"), UCombatModifierData::StaticClass()->IsChildOf(UCombatDefinitionData::StaticClass()));
	TestTrue(TEXT("ProjectileData derives from public definition base"), UCombatProjectileData::StaticClass()->IsChildOf(UCombatDefinitionData::StaticClass()));
	TestTrue(TEXT("AbilitySet derives from public definition base"), UCombatAbilitySet::StaticClass()->IsChildOf(UCombatDefinitionData::StaticClass()));

	CombatReleaseTests::HasDocumentedBlueprintFunction(*this, UCombatGameplayAbility::StaticClass(), GET_FUNCTION_NAME_CHECKED(UCombatGameplayAbility, ReceiveSpellStart));
	CombatReleaseTests::HasDocumentedBlueprintFunction(*this, UCombatGameplayAbility::StaticClass(), GET_FUNCTION_NAME_CHECKED(UCombatGameplayAbility, ReceiveChannelTick));
	CombatReleaseTests::HasDocumentedBlueprintFunction(*this, UCombatGameplayAbility::StaticClass(), GET_FUNCTION_NAME_CHECKED(UCombatGameplayAbility, ReceiveChannelFinish));
	CombatReleaseTests::HasDocumentedBlueprintFunction(*this, UCombatModifierRuntime::StaticClass(), GET_FUNCTION_NAME_CHECKED(UCombatModifierRuntime, OnCreated));
	CombatReleaseTests::HasDocumentedBlueprintFunction(*this, UCombatModifierRuntime::StaticClass(), GET_FUNCTION_NAME_CHECKED(UCombatModifierRuntime, OnRefreshed));
	CombatReleaseTests::HasDocumentedBlueprintFunction(*this, UCombatModifierRuntime::StaticClass(), GET_FUNCTION_NAME_CHECKED(UCombatModifierRuntime, OnDestroyed));
	CombatReleaseTests::HasDocumentedBlueprintFunction(*this, UCombatModifierRuntime::StaticClass(), GET_FUNCTION_NAME_CHECKED(UCombatModifierRuntime, OnThink));
	CombatReleaseTests::HasDocumentedBlueprintFunction(*this, UCombatReleaseContractLibrary::StaticClass(), GET_FUNCTION_NAME_CHECKED(UCombatReleaseContractLibrary, GetCombatReleaseContract));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
