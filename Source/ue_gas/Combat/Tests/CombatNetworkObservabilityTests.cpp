#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "GameFramework/PlayerController.h"
#include "Misc/AutomationTest.h"

#include "Combat/Core/CombatTags.h"
#include "Combat/Data/CombatDefinitionData.h"
#include "Combat/Debug/CombatDebugSubsystem.h"
#include "Combat/Log/CombatEventSubsystem.h"
#include "Combat/Modifiers/CombatModifierComponent.h"
#include "Combat/Modifiers/CombatModifierRuntime.h"
#include "Combat/Network/CombatNetworkSecuritySubsystem.h"
#include "Combat/Performance/CombatPerformanceBudget.h"
#include "Combat/Projectile/CombatProjectileActor.h"
#include "Combat/Projectile/CombatProjectilePresentationSubsystem.h"
#include "Combat/Tests/CombatAutomationWorldFixture.h"
#include "Combat/Unit/CombatUnitCharacter.h"
#include "Combat/Validation/CombatAssetValidationCommandlet.h"
#include "Combat/View/CombatUnitViewComponent.h"

namespace CombatNetworkObservabilityTests
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;

	/** 在测试 World 中生成带稳定 DefinitionId 的 Combat Unit。 */
	ACombatUnitCharacter* SpawnUnit(UWorld& World, const FName DefinitionName, const FVector Location)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		ACombatUnitCharacter* Unit = World.SpawnActor<ACombatUnitCharacter>(Location, FRotator::ZeroRotator, Params);
		if (!Unit)
		{
			return nullptr;
		}
		UCombatUnitData* Data = NewObject<UCombatUnitData>(Unit);
		Data->DefinitionName = DefinitionName;
		return Unit->InitializeFromUnitData(Data) ? Unit : nullptr;
	}

	/** 创建不会改变属性的最小可施加 Modifier。 */
	UCombatModifierData* MakeModifier(UObject& Outer, const FName DefinitionName)
	{
		UCombatModifierData* Data = NewObject<UCombatModifierData>(&Outer);
		Data->DefinitionName = DefinitionName;
		Data->RuntimeClass = UCombatModifierRuntime::StaticClass();
		Data->Duration = 5.0f;
		Data->MaxStacks = 2;
		Data->bIsDebuff = true;
		return Data;
	}

	/** 创建立即可完成的 MoveToPoint 批次，专注验证 RPC 安全层。 */
	FCombatOrderBatchRequest MakeOrderBatch(const int32 RequestId, const FVector TargetPoint)
	{
		FCombatOrderBatchRequest Request;
		Request.RequestId = RequestId;
		FCombatOrderRequest& Order = Request.Orders.AddDefaulted_GetRef();
		Order.Type = ECombatOrderType::MoveToPoint;
		Order.TargetLocation = TargetPoint;
		Order.bHasTargetLocation = true;
		return Request;
	}
}

/** 验证玩家 Unit 使用 Mixed、纯 AI 使用 Minimal，并覆盖 Order RPC 四类攻击面。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCombatM7ReplicationAndRpcSecurityTest,
	"Combat.Network.M7.ReplicationAndRpcSecurity",
	CombatNetworkObservabilityTests::Flags)

bool FCombatM7ReplicationAndRpcSecurityTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FCombatAutomationWorldFixture Fixture(NM_DedicatedServer);
	UWorld* World = Fixture.GetWorld();
	if (!TestNotNull(TEXT("Dedicated fixture world exists"), World)) return false;
	ACombatUnitCharacter* Unit = CombatNetworkObservabilityTests::SpawnUnit(*World, TEXT("m7_network_unit"), FVector::ZeroVector);
	APlayerController* Owner = World->SpawnActor<APlayerController>();
	APlayerController* Attacker = World->SpawnActor<APlayerController>();
	if (!TestNotNull(TEXT("Unit exists"), Unit) || !TestNotNull(TEXT("Owner exists"), Owner) || !TestNotNull(TEXT("Attacker exists"), Attacker)) return false;

	TestEqual(TEXT("Pure AI defaults to Minimal"), Unit->GetEffectiveAscReplicationPolicy(), ECombatAscReplicationPolicy::Minimal);
	TestTrue(TEXT("Assigning an owning connection succeeds"), Unit->SetCommandingPlayerController(Owner));
	TestEqual(TEXT("Commanding player is the unit net owner"), Unit->GetNetOwner(), static_cast<const AActor*>(Owner));
	TestEqual(TEXT("Player-owned unit uses Mixed"), Unit->GetEffectiveAscReplicationPolicy(), ECombatAscReplicationPolicy::Mixed);

	FCombatOrderBatchRequest Valid = CombatNetworkObservabilityTests::MakeOrderBatch(1, Unit->GetActorLocation());
	const FCombatOrderBatchResult Accepted = Unit->ProcessOrderBatchForConnection(Owner, Valid);
	TestTrue(TEXT("Owned bounded request passes security"), Accepted.bAccepted);
	const FCombatOrderBatchResult Replay = Unit->ProcessOrderBatchForConnection(Owner, Valid);
	TestFalse(TEXT("Duplicate request id is rejected"), Replay.bAccepted);
	TestEqual(TEXT("Duplicate has stable failure tag"), Replay.FailureTag, CombatTags::Failure_Network_DuplicateRequest.GetTag());
	const FCombatOrderBatchResult Ownership = Unit->ProcessOrderBatchForConnection(Attacker,
		CombatNetworkObservabilityTests::MakeOrderBatch(2, Unit->GetActorLocation()));
	TestEqual(TEXT("Non-owner has stable failure tag"), Ownership.FailureTag, CombatTags::Failure_Network_Ownership.GetTag());

	FCombatOrderBatchRequest Oversized = CombatNetworkObservabilityTests::MakeOrderBatch(3, Unit->GetActorLocation());
	const FCombatOrderRequest OrderTemplate = Oversized.Orders[0];
	while (Oversized.Orders.Num() <= 8) Oversized.Orders.Add(OrderTemplate);
	const FCombatOrderBatchResult Payload = Unit->ProcessOrderBatchForConnection(Owner, Oversized);
	TestEqual(TEXT("Oversized batch has stable failure tag"), Payload.FailureTag, CombatTags::Failure_Network_PayloadTooLarge.GetTag());

	UCombatNetworkSecuritySubsystem* Security = World->GetSubsystem<UCombatNetworkSecuritySubsystem>();
	Security->BurstCapacity = 1.0f;
	Security->RequestsPerSecond = 1.0f;
	TestTrue(TEXT("First token-bucket request is accepted"), Unit->ProcessOrderBatchForConnection(Owner,
		CombatNetworkObservabilityTests::MakeOrderBatch(4, Unit->GetActorLocation())).bAccepted);
	const FCombatOrderBatchResult RateLimited = Unit->ProcessOrderBatchForConnection(Owner,
		CombatNetworkObservabilityTests::MakeOrderBatch(5, Unit->GetActorLocation()));
	TestEqual(TEXT("Burst overflow has stable failure tag"), RateLimited.FailureTag, CombatTags::Failure_Network_RateLimited.GetTag());
	TestTrue(TEXT("Security metrics include all rejection families"), Security->GetSecurityStats().RejectedRequests >= 4);
	return true;
}

/** 验证 Modifier Runtime 只投影 UI 所需的稳定身份、时间窗和白名单状态。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCombatM7UnitModifierViewTest,
	"Combat.Network.M7.UnitModifierView",
	CombatNetworkObservabilityTests::Flags)

bool FCombatM7UnitModifierViewTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FCombatAutomationWorldFixture Fixture(NM_DedicatedServer);
	UWorld* World = Fixture.GetWorld();
	ACombatUnitCharacter* Unit = World ? CombatNetworkObservabilityTests::SpawnUnit(*World, TEXT("m7_view_unit"), FVector::ZeroVector) : nullptr;
	if (!TestNotNull(TEXT("View unit exists"), Unit)) return false;
	UCombatUnitViewComponent* View = Unit->GetCombatUnitViewComponent();
	TestNotNull(TEXT("Unit owns replicated view component"), View);
	TestEqual(TEXT("Unit DefinitionId is projected"), View->GetUnitView().UnitDefinitionId, Unit->GetUnitDefinitionId());

	UCombatModifierData* Data = CombatNetworkObservabilityTests::MakeModifier(*Unit, TEXT("m7_visible_modifier"));
	Data->GrantedTags.AddTag(CombatTags::State_Stunned);
	FCombatModifierApplyRequest Apply;
	Apply.Source = Unit;
	Apply.ModifierData = Data;
	const FCombatModifierApplyResult First = Unit->GetCombatModifierComponent()->ApplyModifier(Apply);
	TestTrue(TEXT("Visible modifier applies"), First.bSuccess);
	TArray<FCombatModifierView> Entries = View->GetVisibleModifiers();
	TestEqual(TEXT("View contains one flat entry"), Entries.Num(), 1);
	if (!Entries.IsEmpty())
	{
		TestEqual(TEXT("View uses stable DefinitionId"), Entries[0].DefinitionId, Data->GetPrimaryAssetId());
		TestEqual(TEXT("Initial stack is one"), Entries[0].StackCount, 1);
		TestTrue(TEXT("Finite modifier exposes absolute server start time"), Entries[0].ServerStartTime >= 0.0);
		TestTrue(TEXT("Finite modifier exposes absolute server end time"), Entries[0].ServerEndTime > World->GetTimeSeconds());
		TestTrue(TEXT("Only supported control state is projected"), Entries[0].ControlTags.HasTagExact(CombatTags::State_Stunned));
		TestTrue(TEXT("Debuff flag is projected"), Entries[0].bIsDebuff);
	}
	TestTrue(TEXT("Unit view aggregates visible control state"),
		View->GetUnitView().VisibleStatusTags.HasTagExact(CombatTags::State_Stunned));
	TestTrue(TEXT("Refresh reuses runtime"), Unit->GetCombatModifierComponent()->ApplyModifier(Apply).bRefreshed);
	Entries = View->GetVisibleModifiers();
	TestEqual(TEXT("Refresh updates stack without a second view entry"), Entries.IsEmpty() ? 0 : Entries[0].StackCount, 2);
	TestTrue(TEXT("Modifier can be removed"), Unit->GetCombatModifierComponent()->RemoveModifier(First.Handle));
	TestTrue(TEXT("Removed runtime disappears from view"), View->GetVisibleModifiers().IsEmpty());
	TestFalse(TEXT("Removed control state disappears from unit view"),
		View->GetUnitView().VisibleStatusTags.HasTagExact(CombatTags::State_Stunned));
	return true;
}

/** 验证预测视觉只被服务器身份协调一次，并且不会生成第二 gameplay 弹体。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCombatM7ProjectilePresentationTest,
	"Combat.Network.M7.ProjectilePresentationReconcile",
	CombatNetworkObservabilityTests::Flags)

bool FCombatM7ProjectilePresentationTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FCombatAutomationWorldFixture Fixture;
	UWorld* World = Fixture.GetWorld();
	if (!TestNotNull(TEXT("World exists"), World)) return false;
	UCombatProjectilePresentationSubsystem* Presentation = World->GetSubsystem<UCombatProjectilePresentationSubsystem>();
	AActor* Predicted = World->SpawnActor<AActor>();
	ACombatProjectileActor* ServerActor = World->SpawnActor<ACombatProjectileActor>();
	TestTrue(TEXT("Predicted visual registers"), Presentation->RegisterPredictedVisual(77, Predicted));
	FCombatProjectileHandle Handle;
	Handle.Key.Id = 1;
	Handle.Key.Generation = 1;
	ServerActor->InitializeProjectile(Handle, FPrimaryAssetId(FPrimaryAssetType(TEXT("CombatProjectile")), TEXT("m7_projectile")), 20.0f, 77);
	Presentation->ReconcileServerProjectile(ServerActor);
	FCombatProjectilePresentationStats Stats = Presentation->GetPresentationStats();
	TestEqual(TEXT("Predicted visual is consumed"), Stats.ActivePredictedVisuals, 0);
	TestEqual(TEXT("One server visual remains"), Stats.ActiveServerVisuals, 1);
	TestEqual(TEXT("Reconcile occurs once"), Stats.ReconcileCount, static_cast<int64>(1));
	Presentation->ReconcileServerProjectile(ServerActor);
	Stats = Presentation->GetPresentationStats();
	TestEqual(TEXT("Duplicate identity does not add a visual"), Stats.ActiveServerVisuals, 1);
	TestEqual(TEXT("Duplicate identity is observable"), Stats.DuplicateServerIdentityCount, static_cast<int64>(1));
	return true;
}

/** 验证 schema、RootEvent 展开、资产迁移报告和冻结容量预算使用同一诊断基线。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCombatM7DiagnosticsValidationBudgetTest,
	"Combat.Observability.M7.DiagnosticsValidationBudget",
	CombatNetworkObservabilityTests::Flags)

bool FCombatM7DiagnosticsValidationBudgetTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FCombatAutomationWorldFixture Fixture;
	UWorld* World = Fixture.GetWorld();
	if (!TestNotNull(TEXT("World exists"), World)) return false;
	UCombatEventSubsystem* Events = World->GetSubsystem<UCombatEventSubsystem>();
	const FCombatEventContext Root = Events->CreateRootEvent();
	FCombatLogRecord RootRecord;
	RootRecord.SchemaVersion = 999;
	RootRecord.Context = Root;
	RootRecord.Diagnostic = TEXT("root");
	Events->Emit(RootRecord);
	FCombatLogRecord ChildRecord;
	ChildRecord.Context = Events->CreateChildEvent(Root);
	ChildRecord.Diagnostic = TEXT("child");
	Events->Emit(ChildRecord);
	const TArray<FCombatLogRecord> Expanded = Events->GetRecordsForRootEvent(Root.RootEventId);
	TestEqual(TEXT("Root expansion preserves two records"), Expanded.Num(), 2);
	TestEqual(TEXT("Emit overwrites unknown schema"), Expanded[0].SchemaVersion, UCombatEventSubsystem::CurrentSchemaVersion);
	UCombatDebugSubsystem* Debug = World->GetSubsystem<UCombatDebugSubsystem>();
	TestTrue(TEXT("Debug dump contains both records"), Debug->DumpRootEvent(Root.RootEventId).Contains(TEXT("Records=2")));

	UCombatUnitData* Definition = NewObject<UCombatUnitData>();
	Definition->DefinitionName = TEXT("m7_valid_definition");
	UCombatAssetValidationSettings* Settings = NewObject<UCombatAssetValidationSettings>();
	const FCombatAssetValidationReport ValidReport = FCombatAssetValidator::ValidateDefinitions({ Definition }, *Settings);
	TestTrue(TEXT("Valid in-memory definition passes project rules"), ValidReport.IsValid());
	UCombatUnitData* Duplicate = NewObject<UCombatUnitData>();
	Duplicate->DefinitionName = Definition->DefinitionName;
	const FCombatAssetValidationReport InvalidReport = FCombatAssetValidator::ValidateDefinitions({ Definition, Duplicate }, *Settings);
	TestFalse(TEXT("Duplicate stable DefinitionId blocks cook"), InvalidReport.IsValid());

	FCombatRuntimeMetrics Metrics;
	Metrics.Units = 64;
	Metrics.Modifiers = 256;
	Metrics.Projectiles = 128;
	Metrics.Thinkers = 32;
	Metrics.Auras = 16;
	Metrics.AuraChildren = 256;
	Metrics.SchedulerSlots = 256;
	Metrics.SchedulerCallbacks = 256;
	const FCombatPerformanceBudget Budget;
	TestTrue(TEXT("Frozen boundary is accepted"), FCombatPerformanceBudgetEvaluator::Evaluate(
		Metrics, Budget, Budget.MaxServerFrameP95Ms, Budget.MaxServerFrameP99Ms, Budget.MaxPerConnectionBandwidthKiBps).bPassed);
	Metrics.Units = 65;
	TestFalse(TEXT("Capacity overflow is rejected"), FCombatPerformanceBudgetEvaluator::Evaluate(Metrics, FCombatPerformanceBudget()).bPassed);
	return true;
}

/** 在真实 World 中达到 64 Unit / 256 Modifier 冻结边界并验证 teardown。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCombatM7CapacityLifecycleTest,
	"Combat.Performance.M7.CapacityLifecycle",
	CombatNetworkObservabilityTests::Flags)

bool FCombatM7CapacityLifecycleTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FCombatAutomationWorldFixture Fixture(NM_DedicatedServer);
	UWorld* World = Fixture.GetWorld();
	if (!TestNotNull(TEXT("Dedicated capacity world exists"), World)) return false;
	TArray<ACombatUnitCharacter*> Units;
	for (int32 UnitIndex = 0; UnitIndex < 64; ++UnitIndex)
	{
		ACombatUnitCharacter* Unit = CombatNetworkObservabilityTests::SpawnUnit(
			*World,
			FName(*FString::Printf(TEXT("m7_capacity_unit_%02d"), UnitIndex)),
			FVector((UnitIndex % 8) * 250.0, (UnitIndex / 8) * 250.0, 0.0));
		if (!Unit)
		{
			AddError(FString::Printf(TEXT("Capacity unit %d failed to spawn"), UnitIndex));
			return false;
		}
		Units.Add(Unit);
		for (int32 ModifierIndex = 0; ModifierIndex < 4; ++ModifierIndex)
		{
			UCombatModifierData* Data = CombatNetworkObservabilityTests::MakeModifier(
				*Unit,
				FName(*FString::Printf(TEXT("m7_capacity_modifier_%02d_%d"), UnitIndex, ModifierIndex)));
			Data->bIsDebuff = false;
			Data->Duration = 0.0f;
			FCombatModifierApplyRequest Apply;
			Apply.Source = Unit;
			Apply.ModifierData = Data;
			if (!Unit->GetCombatModifierComponent()->ApplyModifier(Apply).bSuccess)
			{
				AddError(FString::Printf(TEXT("Capacity modifier %d/%d failed to apply"), UnitIndex, ModifierIndex));
				return false;
			}
		}
	}

	UCombatDebugSubsystem* Debug = World->GetSubsystem<UCombatDebugSubsystem>();
	const FCombatRuntimeMetrics Metrics = Debug->CaptureMetrics();
	TestEqual(TEXT("Capacity world reaches 64 units"), Metrics.Units, 64);
	TestEqual(TEXT("Capacity world reaches 256 modifiers"), Metrics.Modifiers, 256);
	TestTrue(TEXT("Frozen capacity boundary remains within budget"),
		FCombatPerformanceBudgetEvaluator::Evaluate(Metrics, FCombatPerformanceBudget()).bPassed);
	for (ACombatUnitCharacter* Unit : Units)
	{
		TestEqual(TEXT("Capacity cleanup removes four runtimes per unit"),
			Unit->GetCombatModifierComponent()->Dispel(ECombatDispelStrength::Strong, false), 4);
	}
	for (const ACombatUnitCharacter* Unit : Units)
	{
		TestEqual(TEXT("Capacity cleanup leaves no modifier runtime"), Unit->GetCombatModifierComponent()->GetActiveModifierCount(), 0);
	}
	for (ACombatUnitCharacter* Unit : Units)
	{
		Unit->Destroy();
	}
	return true;
}

#endif
