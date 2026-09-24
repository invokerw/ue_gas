#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/World.h"
#include "EnhancedInputComponent.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Camera/CameraComponent.h"
#include "Misc/AutomationTest.h"
#include "Combat/Tests/CombatAutomationWorldFixture.h"
#include "Combat/Unit/CombatUnitCharacter.h"
#include "Combat/Unit/CombatUnitAIController.h"
#include "CombatCharacter.h"
#include "CombatPlayerController.h"

/** 验证绑定只做一次居中，未按 Space 时单位运动不能拉动自由镜头。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatCameraFreeAnchorTest,
    "Combat.Camera.FreeAnchorDoesNotFollow", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCombatCameraFreeAnchorTest::RunTest(const FString& Parameters)
{
    FCombatAutomationWorldFixture Fixture;
    if (!Fixture.IsValid()) return false;
    UWorld* World = Fixture.GetWorld();
    ACombatPlayerController* PC = World->SpawnActor<ACombatPlayerController>();
    ACombatCharacter* Camera = World->SpawnActor<ACombatCharacter>();
    ACombatUnitCharacter* Unit = World->SpawnActor<ACombatUnitCharacter>();
    if (!PC || !Camera || !Unit) return false;
    PC->SetAsLocalPlayerController();
    PC->SetCommandedUnitAuthority(Unit);
    PC->Possess(Camera);
    TestTrue(TEXT("Fixture is a local controller"), PC->IsLocalController());
    const FVector Initial = Camera->GetActorLocation();
    Unit->SetActorLocation(Initial + FVector(1000, 0, 0));
    Camera->Tick(0.1f);
    TestEqual(TEXT("Free camera stays at initial anchor"), Camera->GetActorLocation(), Initial);
    return true;
}

namespace CombatCameraTests
{
	/** 真实 World/Controller/Pawn 夹具；使用引擎的本地标记，不生成额外视口或修改 CDO。 */
	struct FCameraFixture
	{
		FCombatAutomationWorldFixture World;
		ACombatPlayerController* PC;
		ACombatCharacter* Camera;
		ACombatUnitCharacter* Unit;
		explicit FCameraFixture(bool bLocal = true, ENetMode Mode = NM_Standalone) : World(Mode)
		{
			PC = World.GetWorld()->SpawnActor<ACombatPlayerController>();
			Camera = World.GetWorld()->SpawnActor<ACombatCharacter>();
			Unit = World.GetWorld()->SpawnActor<ACombatUnitCharacter>();
			if (bLocal) PC->SetAsLocalPlayerController();
			PC->SetCommandedUnitAuthority(Unit);
			PC->Possess(Camera);
		}
	};
}

/** 四边/角落、视口有效性、帧率、固定高度、可选减速/边界与 UI 禁止路径。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatCameraMovementTest,
	"Combat.Camera.EdgePanGeometryAndTiming", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCombatCameraMovementTest::RunTest(const FString& Parameters)
{
	using namespace CombatCameraTests;
	FCameraFixture F;
	auto* C = F.Camera;
	const FVector2D Size(1920, 1080);
	TestEqual(TEXT("Center is idle"), C->GetEdgePanInput(Size * 0.5, Size), FVector2D::ZeroVector);
	TestEqual(TEXT("Right"), C->GetEdgePanInput(FVector2D(1920, 540), Size), FVector2D(1, 0));
	TestEqual(TEXT("Left"), C->GetEdgePanInput(FVector2D(0, 540), Size), FVector2D(-1, 0));
	TestEqual(TEXT("Top"), C->GetEdgePanInput(FVector2D(960, 0), Size), FVector2D(0, 1));
	TestEqual(TEXT("Bottom"), C->GetEdgePanInput(FVector2D(960, 1080), Size), FVector2D(0, -1));
	TestEqual(TEXT("Outside stops"), C->GetEdgePanInput(FVector2D(-1, 540), Size), FVector2D::ZeroVector);
	TestEqual(TEXT("Invalid size stops"), C->GetEdgePanInput(FVector2D::ZeroVector, FVector2D::ZeroVector), FVector2D::ZeroVector);
	TestTrue(TEXT("Half band gives half strength"), C->GetEdgePanInput(FVector2D(1906.5, 540), Size).Equals(FVector2D(0.5, 0), 0.00001));
	TestTrue(TEXT("Resolution proportional"), C->GetEdgePanInput(FVector2D(3813, 1080), Size * 2).Equals(FVector2D(0.5, 0), 0.00001));
	const FVector UnitBefore = F.Unit->GetActorLocation();
	C->SetActorLocation(FVector(0, 0, 123));
	C->UpdateCamera(0.5f, FVector2D(1, 0));
	TestTrue(TEXT("Right moves along camera right"), C->GetActorLocation().Equals(FVector(0, 900, 123), 0.01));
	const FVector Stopped = C->GetActorLocation();
	C->UpdateCamera(0.5f, FVector2D::ZeroVector);
	TestEqual(TEXT("Default stops immediately"), C->GetActorLocation(), Stopped);
	TestEqual(TEXT("Idle state"), C->GetCameraMode(), ECombatCameraMode::Free);
	C->SetActorLocation(FVector(0, 0, 123));
	C->UpdateCamera(0.5f, FVector2D(1, 1));
	TestTrue(TEXT("Diagonal speed is normalized"), FMath::IsNearlyEqual(C->GetActorLocation().Size2D(), 900.0, 0.01));
	TestEqual(TEXT("Height fixed"), C->GetActorLocation().Z, 123.0);
	C->SetActorLocation(FVector::ZeroVector);
	for (int32 I = 0; I < 60; ++I) C->UpdateCamera(1.0f / 60, FVector2D(0, 1));
	const FVector SixtyFPS = C->GetActorLocation();
	C->SetActorLocation(FVector::ZeroVector);
	for (int32 I = 0; I < 30; ++I) C->UpdateCamera(1.0f / 30, FVector2D(0, 1));
	TestTrue(TEXT("Frame rate independent"), C->GetActorLocation().Equals(SixtyFPS, 0.01));
	C->EdgePanDeceleration = 10;
	const FVector BeforeBlocked = C->GetActorLocation();
	C->UpdateCamera(0.1f, FVector2D(1, 0), false);
	TestEqual(TEXT("UI block cancels velocity immediately"), C->GetActorLocation(), BeforeBlocked);
	C->UpdateCamera(0.1f, FVector2D::ZeroVector);
	TestEqual(TEXT("Blocked inertia cannot resume"), C->GetActorLocation(), BeforeBlocked);
	C->bClampCameraBounds = true;
	C->CameraBoundsMin = FVector2D(-100, -200);
	C->CameraBoundsMax = FVector2D(100, 200);
	C->SetActorLocation(FVector(0, 0, 123));
	C->UpdateCamera(1, FVector2D(1, 1));
	TestEqual(TEXT("Bounds clamp only anchor XY"), C->GetActorLocation(), FVector(100, 200, 123));
	TestEqual(TEXT("No Unit movement"), F.Unit->GetActorLocation(), UnitBefore);
	TestFalse(TEXT("No movement replication"), C->IsReplicatingMovement());
	return true;
}

/** 按住/释放、接管、重复绑定、旧释放、Owner 丢失与销毁只清理本地镜头。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatCameraLifecycleTest,
	"Combat.Camera.FollowAndBindingLifecycle", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCombatCameraLifecycleTest::RunTest(const FString& Parameters)
{
	using namespace CombatCameraTests;
	FCameraFixture F;
	auto* C = F.Camera;
	const uint64 First = C->BeginCameraFollow();
	TestTrue(TEXT("Ready target starts follow"), First > 0);
	F.Unit->SetActorLocation(FVector(1000, 0, 500));
	C->UpdateCamera(0.1f, FVector2D::ZeroVector);
	TestTrue(TEXT("Held follows smoothly"), C->GetActorLocation().X > 0 && C->GetActorLocation().X < 1000);
	TestEqual(TEXT("Follow holds original Z"), C->GetActorLocation().Z, 0.0);
	C->EndCameraFollow(First);
	const FVector Released = C->GetActorLocation();
	C->UpdateCamera(1, FVector2D::ZeroVector);
	TestEqual(TEXT("Release freezes"), C->GetActorLocation(), Released);
	C->SetFollowTarget(F.Unit, F.PC->GetCommandBindingGeneration());
	TestEqual(TEXT("Same binding cannot recenter"), C->GetActorLocation(), Released);
	const uint64 Second = C->BeginCameraFollow();
	C->EndCameraFollow(First);
	TestEqual(TEXT("Old release cannot end new follow"), C->GetCameraMode(), ECombatCameraMode::FollowHeld);
	C->UpdateCamera(0.1f, FVector2D(1, 0));
	TestEqual(TEXT("Edge takes over"), C->GetCameraMode(), ECombatCameraMode::EdgePan);
	TestEqual(TEXT("Held old press cannot restart"), C->BeginCameraFollow(), uint64(0));
	C->UpdateCamera(0.1f, FVector2D::ZeroVector);
	TestEqual(TEXT("Leave edge remains free"), C->GetCameraMode(), ECombatCameraMode::Free);
	C->EndCameraFollow(Second);
	TestTrue(TEXT("Release then press follows again"), C->BeginCameraFollow() > Second);
	F.PC->FlushPressedKeys();
	TestEqual(TEXT("Flush cancels follow"), C->GetCameraMode(), ECombatCameraMode::Free);
	C->UpdateCamera(0.1f, FVector2D(1, 0));
	const uint64 FromEdge = C->BeginCameraFollow();
	C->UpdateCamera(0.1f, FVector2D(1, 0));
	TestTrue(TEXT("Space can take over an existing edge pan"), FromEdge > 0 && C->GetCameraMode() == ECombatCameraMode::FollowHeld);
	C->UpdateCamera(0.1f, FVector2D::ZeroVector);
	C->UpdateCamera(0.1f, FVector2D(1, 0));
	TestEqual(TEXT("Reentering edge takes over new follow"), C->GetCameraMode(), ECombatCameraMode::EdgePan);
	C->EndCameraFollow(FromEdge);
	const uint64 BeforeRebind = C->BeginCameraFollow();
	auto* NewUnit = F.World.GetWorld()->SpawnActor<ACombatUnitCharacter>();
	NewUnit->SetActorLocation(FVector(50, 60, 70));
	const FVector BeforeRebindAnchor = C->GetActorLocation();
	F.PC->SetCommandedUnitAuthority(NewUnit);
	TestEqual(TEXT("Rebinding keeps initialized anchor"), C->GetActorLocation(), BeforeRebindAnchor);
	const uint64 AfterRebind = C->BeginCameraFollow();
	C->EndCameraFollow(BeforeRebind);
	TestEqual(TEXT("Old binding release ignored"), C->GetCameraMode(), ECombatCameraMode::FollowHeld);
	TestTrue(TEXT("Serial never reused"), AfterRebind > BeforeRebind);
	NewUnit->SetCommandingPlayerController(nullptr);
	C->UpdateCamera(0.1f, FVector2D::ZeroVector);
	TestEqual(TEXT("Owner loss stops follow"), C->GetCameraMode(), ECombatCameraMode::Free);
	TestEqual(TEXT("Owner loss rejects press"), C->BeginCameraFollow(), uint64(0));
	F.PC->SetCommandedUnitAuthority(NewUnit);
	C->BeginCameraFollow();
	if (!NewUnit->HasActorBegunPlay()) NewUnit->DispatchBeginPlay();
	NewUnit->Destroy();
	C->UpdateCamera(0.1f, FVector2D::ZeroVector);
	TestEqual(TEXT("Destroyed target stops follow"), C->GetCameraMode(), ECombatCameraMode::Free);
	F.PC->UnPossess();
	const FVector Unpossessed = C->GetActorLocation();
	C->UpdateCamera(1, FVector2D(1, 1));
	TestEqual(TEXT("Unpossessed camera cannot move"), C->GetActorLocation(), Unpossessed);
	return true;
}

/** Dedicated/远端 Controller 即使被直接调用也不能驱动镜头。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatCameraAuthorityTest,
	"Combat.Camera.LocalPresentationOnly", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCombatCameraAuthorityTest::RunTest(const FString& Parameters)
{
	using namespace CombatCameraTests;
	for (const ENetMode Mode : {NM_Standalone, NM_DedicatedServer})
	{
		FCameraFixture F(false, Mode);
		const FVector Initial = F.Camera->GetActorLocation();
		F.Camera->UpdateCamera(1, FVector2D(1, 0));
		TestEqual(TEXT("Nonlocal camera stays still"), F.Camera->GetActorLocation(), Initial);
		TestEqual(TEXT("Nonlocal follow rejected"), F.Camera->BeginCameraFollow(), uint64(0));
		TestTrue(TEXT("Unit AI possession preserved"), Cast<ACombatUnitAIController>(F.Unit->GetController()) != nullptr);
	}
	return true;
}

/** 检查真实 Demo 映射和实际绑定的 Completed/Canceled，确保没有抓取键依赖。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatCameraInputTest,
	"Combat.Camera.DemoInputContract", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCombatCameraInputTest::RunTest(const FString& Parameters)
{
	using namespace CombatCameraTests;
	UClass* Demo = LoadClass<ACombatPlayerController>(nullptr, TEXT("/Game/Combat/Demo/Framework/BP_CombatDemoPlayerController.BP_CombatDemoPlayerController_C"));
	if (!TestNotNull(TEXT("Demo class"), Demo)) return false;
	const auto* Defaults = Demo->GetDefaultObject<ACombatPlayerController>();
	const UInputAction* Action = Defaults->CameraFollowAction;
	if (!TestNotNull(TEXT("Configured Follow Action"), Action)) return false;
	TestEqual(TEXT("Boolean action"), Action->ValueType, EInputActionValueType::Boolean);
	if (!TestNotNull(TEXT("Demo mapping context"), Defaults->DefaultMappingContext.Get())) return false;
	bool bSpace = false;
	for (const auto& Mapping : Defaults->DefaultMappingContext->GetMappings())
		if (Mapping.Action == Action && Mapping.Key == EKeys::SpaceBar) bSpace = true;
	TestTrue(TEXT("Space mapping present"), bSpace);
	FCameraFixture F;
	F.PC->CameraFollowAction = const_cast<UInputAction*>(Action);
	auto* Input = NewObject<UEnhancedInputComponent>(F.PC);
	F.PC->BindCameraActions(*Input);
	TestEqual(TEXT("Started, Completed and Canceled bindings"), Input->GetActionEventBindings().Num(), 3);
	for (const ETriggerEvent Event : {ETriggerEvent::Completed, ETriggerEvent::Canceled})
	{
		F.PC->CameraFollowPressSerial = F.Camera->BeginCameraFollow();
		for (const auto& Binding : Input->GetActionEventBindings())
			if (Binding->GetTriggerEvent() == Event) Binding->Execute(FInputActionInstance(Action));
		TestEqual(TEXT("Actual bound release exits follow"), F.Camera->GetCameraMode(), ECombatCameraMode::Free);
	}
	TestEqual(TEXT("Camera did not allocate any Order RPC id"), F.PC->NextCombatOrderRequestId, 1);
	F.Camera->BeginCameraFollow();
	F.PC->UpdateLocalCamera(0.1f);
	TestEqual(TEXT("Missing viewport cancels follow"), F.Camera->GetCameraMode(), ECombatCameraMode::Free);
	F.PC->OnCameraFollowStarted();
	TestEqual(TEXT("Missing viewport rejects follow input"), F.PC->CameraFollowPressSerial, uint64(0));
	return true;
}

/** 框选、只读切回和网络确认空窗只换观察目标，不应隐式夺回自由镜头。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatCameraSelectionAnchorTest,
	"Combat.Camera.SelectionPreservesAnchor", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCombatCameraSelectionAnchorTest::RunTest(const FString& Parameters)
{
	FCombatAutomationWorldFixture World;
	auto* PC = World.GetWorld()->SpawnActor<ACombatPlayerController>();
	auto* Camera = World.GetWorld()->SpawnActor<ACombatCharacter>();
	auto* A = World.GetWorld()->SpawnActor<ACombatUnitCharacter>();
	auto* B = World.GetWorld()->SpawnActor<ACombatUnitCharacter>();
	A->SetActorLocation(FVector(400, 500, 90));
	B->SetActorLocation(FVector(-400, -500, 90));
	PC->SetAsLocalPlayerController();
	PC->SetCommandedUnitAuthority(A);
	PC->GrantUnitControlAuthority(B);
	PC->Possess(Camera);
	TestEqual(TEXT("First ready target initializes camera"), Camera->GetActorLocation(), A->GetActorLocation());
	const FVector Anchor(1300, 1400, 90);
	Camera->SetActorLocation(Anchor);
	PC->SelectCombatUnits({B, A}, false);
	Camera->SetFollowTarget(PC->GetCommandedUnit(), PC->GetCommandBindingGeneration());
	TestEqual(TEXT("Box selection changing primary keeps anchor"), Camera->GetActorLocation(), Anchor);
	Camera->SetFollowTarget(nullptr, PC->GetCommandBindingGeneration());
	Camera->SetFollowTarget(B, PC->GetCommandBindingGeneration());
	TestEqual(TEXT("Read-only or pending-ack gap cannot recenter"), Camera->GetActorLocation(), Anchor);
	PC->SelectCombatUnits({A, B}, false);
	Camera->SetFollowTarget(A, PC->GetCommandBindingGeneration());
	TestEqual(TEXT("Reselecting another group keeps anchor"), Camera->GetActorLocation(), Anchor);
	const auto Press = Camera->BeginCameraFollow();
	Camera->UpdateCamera(0.1f, FVector2D::ZeroVector);
	TestTrue(TEXT("Explicit follow still moves toward new primary"), Press > 0
		&& FVector::Dist2D(Camera->GetActorLocation(), A->GetActorLocation()) < FVector::Dist2D(Anchor, A->GetActorLocation()));
	Camera->EndCameraFollow(Press);
	return true;
}

#endif
