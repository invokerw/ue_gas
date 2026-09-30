#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Combat/Tests/CombatAutomationWorldFixture.h"
#include "Combat/UI/CombatMinimapWidget.h"
#include "CombatCharacter.h"
#include "CombatPlayerController.h"
#include "Combat/Unit/CombatUnitCharacter.h"
#include "SlateCore.h"

/** 地图必须按完整包路径配置，不能让 Demo 和测试场共用一个猜测矩形。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatMinimapMapConfigurationTest,
	"Combat.UI.Minimap.PerWorldConfiguration", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCombatMinimapMapConfigurationTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	TArray<FCombatMinimapMap> Configs;
	FCombatMinimapMap Demo;
	Demo.MapPackage = TEXT("/Game/Combat/Demo/Maps/L_CombatDemo");
	Demo.WorldMin = FVector2D(-2000, -2000);
	Demo.WorldMax = FVector2D(2000, 2000);
	Configs.Add(Demo);
	TestEqual(TEXT("Exact Demo package resolves"), UCombatMinimapWidget::FindMap(Configs, Demo.MapPackage.ToString()), 0);
	TestEqual(TEXT("PIE package prefix resolves to the saved map"),
		UCombatMinimapWidget::FindMap(Configs, TEXT("/Game/Combat/Demo/Maps/UEDPIE_0_L_CombatDemo")), 0);
	TestEqual(TEXT("Other map does not reuse Demo bounds"), UCombatMinimapWidget::FindMap(Configs, TEXT("/Game/Combat/Tests/L_CombatTest")), INDEX_NONE);
	const FCombatMinimapLayout Layout = UCombatMinimapWidget::ResolveLayout(FVector2D(1280, 720), FSlateRect(300, 560, 1000, 720), true);
	TestTrue(TEXT("Standard layout fits left of bottom HUD"), Layout.bVisible && FMath::IsNearlyEqual(Layout.Size, 220.0f));
	const FCombatMinimapLayout FullScreenRootLayout = UCombatMinimapWidget::ResolveLayout(
		FVector2D(1280, 720), FSlateRect(0, 0, 1280, 720), true);
	TestTrue(TEXT("Full-screen HUD root does not hide the map"),
		FullScreenRootLayout.bVisible && FMath::IsNearlyEqual(FullScreenRootLayout.Size, 220.0f));
	const FCombatMinimapLayout BottomContainerLayout = UCombatMinimapWidget::ResolveLayout(
		FVector2D(1280, 720), FSlateRect(0, 560, 1280, 720), true, 220.0f, 184.0f, 12.0f, 12.0f);
	TestTrue(TEXT("Viewport-wide bottom HUD container does not lift the map"),
		BottomContainerLayout.bVisible && FMath::IsNearlyEqual(BottomContainerLayout.Position.X, 12.0f)
			&& FMath::IsNearlyEqual(BottomContainerLayout.Position.Y, 488.0f));
	const FCombatMinimapLayout CompactLayout = UCombatMinimapWidget::ResolveLayout(FVector2D(360, 240), FSlateRect(0, 0, 360, 240), true);
	TestTrue(TEXT("Narrow viewport either moves compact map above HUD or hides safely"),
		!CompactLayout.bVisible || FMath::IsNearlyEqual(CompactLayout.Size, 184.0f));
	return true;
}

/** 验证 Demo/Test 使用的是来自原型关卡几何的稳定地形与地标，而不是白色 Image 回退。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatMinimapTerrainTest,
	"Combat.UI.Minimap.TerrainFeatures", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCombatMinimapTerrainTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	UCombatMinimapWidget* Widget = NewObject<UCombatMinimapWidget>();
	if (!TestNotNull(TEXT("Minimap widget owns map configurations"), Widget)) return false;
	TestEqual(TEXT("Demo and test maps have independent feature tables"), Widget->Maps.Num(), 2);
	for (const FCombatMinimapMap& Map : Widget->Maps)
	{
		TestTrue(TEXT("Configured map remains valid"), Map.IsValid());
		TestTrue(TEXT("Configured map has static terrain features"), Map.TerrainFeatures.Num() >= 20);
		for (const FCombatMinimapTerrainFeature& Feature : Map.TerrainFeatures)
		{
			TestTrue(TEXT("Terrain feature has a finite range and color"), Feature.IsValid());
			TestTrue(TEXT("Terrain feature stays inside the configured map"),
				Feature.WorldMin.X >= Map.WorldMin.X - 0.1f && Feature.WorldMin.Y >= Map.WorldMin.Y - 0.1f
				&& Feature.WorldMax.X <= Map.WorldMax.X + 0.1f && Feature.WorldMax.Y <= Map.WorldMax.Y + 0.1f);
		}
	}
	const FCombatMinimapMap& Demo = Widget->Maps[0];
	int32 LabelCount = 0;
	for (const FCombatMinimapTerrainFeature& Feature : Demo.TerrainFeatures)
	{
		if (!Feature.Label.IsEmpty()) ++LabelCount;
		TestTrue(TEXT("Every terrain feature has no label"), Feature.Label.IsEmpty());
	}
	TestEqual(TEXT("Terrain has no labels"), LabelCount, 0);
	return true;
}

/** 验证编辑器生成配置可往返解析，非法文件不会污染运行时内置回退。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatMinimapGeneratedConfigTest,
	"Combat.UI.Minimap.GeneratedConfigSerialization", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCombatMinimapGeneratedConfigTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	UCombatMinimapWidget* Widget = NewObject<UCombatMinimapWidget>();
	if (!TestNotNull(TEXT("Minimap widget provides a fallback map"), Widget)) return false;
	const TArray<FCombatMinimapMap>& WidgetMaps = Widget->GetMapConfigurations();
	if (WidgetMaps.Num() == 0) return false;
	TArray<FCombatMinimapMap> SourceMaps;
	SourceMaps.Add(WidgetMaps[0]);
	FString Json;
	FString Error;
	TestTrue(TEXT("Generated map serializes to JSON"), CombatMinimapConfig::Serialize(SourceMaps, Json, &Error));
	TArray<FCombatMinimapMap> ParsedMaps;
	TestTrue(TEXT("Generated JSON deserializes"), CombatMinimapConfig::Deserialize(Json, ParsedMaps, &Error));
	TestEqual(TEXT("Round trip preserves one map"), ParsedMaps.Num(), 1);
	if (ParsedMaps.Num() == 1)
	{
		TestEqual(TEXT("Round trip preserves map package"), ParsedMaps[0].MapPackage, SourceMaps[0].MapPackage);
		TestEqual(TEXT("Round trip preserves terrain count"), ParsedMaps[0].TerrainFeatures.Num(), SourceMaps[0].TerrainFeatures.Num());
		bool bLabelsEmpty = true;
		for (const FCombatMinimapTerrainFeature& Feature : ParsedMaps[0].TerrainFeatures)
			bLabelsEmpty &= Feature.Label.IsEmpty();
		TestTrue(TEXT("Round trip keeps all generated labels empty"), bLabelsEmpty);
	}
	TArray<FCombatMinimapMap> InvalidMaps;
	TestFalse(TEXT("Invalid JSON is rejected"), CombatMinimapConfig::Deserialize(TEXT("{\"schemaVersion\":99}"), InvalidMaps, &Error));
	TestEqual(TEXT("Invalid JSON leaves no partial maps"), InvalidMaps.Num(), 0);
	return true;
}

/** 验证小地图拖动平移的是本地相机，底图视角中心保持固定且不移动战斗单位。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatMinimapCameraDragTest,
	"Combat.UI.Minimap.CameraDragPansCamera", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCombatMinimapCameraDragTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	const FVector2D Clamped = UCombatMinimapWidget::ClampViewCenter(FVector2D(-2.0f, 3.0f), 1.25f);
	TestTrue(TEXT("Fixed map view center remains bounded"), Clamped.Equals(FVector2D(0.4f, 0.6f), 0.0001f));
	const FVector2D FullMap = UCombatMinimapWidget::ClampViewCenter(FVector2D(0.2f, 0.8f), 1.0f);
	TestTrue(TEXT("Full-map view remains centered"), FullMap.Equals(FVector2D(0.5f, 0.5f), 0.0001f));

	FCombatAutomationWorldFixture Fixture;
	if (!Fixture.IsValid()) return false;
	UWorld* World = Fixture.GetWorld();
	ACombatPlayerController* PC = World->SpawnActor<ACombatPlayerController>();
	ACombatCharacter* Camera = World->SpawnActor<ACombatCharacter>();
	ACombatUnitCharacter* Unit = World->SpawnActor<ACombatUnitCharacter>();
	if (!TestNotNull(TEXT("Player controller"), PC) || !TestNotNull(TEXT("Camera pawn"), Camera)
		|| !TestNotNull(TEXT("Combat unit"), Unit)) return false;
	PC->SetAsLocalPlayerController();
	PC->SetCommandedUnitAuthority(Unit);
	PC->Possess(Camera);
	Camera->SetActorLocation(FVector(400.0f, -300.0f, 777.0f));
	const FVector UnitBefore = Unit->GetActorLocation();
	TestTrue(TEXT("Minimap drag pans the local camera"), PC->PanCameraFromMinimap(FVector2D(-150.0f, 220.0f)));
	TestTrue(TEXT("Camera moves by the drag world delta"), Camera->GetActorLocation().Equals(FVector(250.0f, -80.0f, 777.0f), 0.01f));
	TestEqual(TEXT("Camera drag leaves camera free"), Camera->GetCameraMode(), ECombatCameraMode::Free);
	TestEqual(TEXT("Minimap drag never moves Combat Unit"), Unit->GetActorLocation(), UnitBefore);
	return true;
}

/** 验证普通左键按下先定位，Alt+左键和中键不抢占各自的输入语义。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatMinimapInputTest,
	"Combat.UI.Minimap.PressPositionsBeforeDrag", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCombatMinimapInputTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	TestTrue(TEXT("Normal left press positions the camera"),
		UCombatMinimapWidget::ShouldRelocateOnPress(EKeys::LeftMouseButton, false));
	TestFalse(TEXT("Alt left press remains a local Ping gesture"),
		UCombatMinimapWidget::ShouldRelocateOnPress(EKeys::LeftMouseButton, true));
	TestFalse(TEXT("Middle press remains drag-only"),
		UCombatMinimapWidget::ShouldRelocateOnPress(EKeys::MiddleMouseButton, false));
	return true;
}

/** 镜头越过地图边界时只裁掉不可见线段，验证可见轮廓仍随相机移动。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatMinimapCameraTrapezoidTest,
	"Combat.UI.Minimap.CameraTrapezoidClipsAtBoundary", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCombatMinimapCameraTrapezoidTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	const FVector2D MapSize(210.0f, 210.0f);
	TArray<FVector2D> InBounds = {
		FVector2D(24.0f, 36.0f), FVector2D(174.0f, 36.0f),
		FVector2D(152.0f, 170.0f), FVector2D(42.0f, 170.0f) };
	TestTrue(TEXT("In-bounds camera outline remains drawable"),
		UCombatMinimapWidget::ClipCameraTrapezoidToMap(InBounds, MapSize));
	TestEqual(TEXT("In-bounds outline keeps four corners"), InBounds.Num(), 4);
	const float InBoundsTopWidth = FVector2D::Distance(InBounds[0], InBounds[1]);
	const float InBoundsBottomWidth = FVector2D::Distance(InBounds[3], InBounds[2]);
	TestTrue(TEXT("In-bounds perspective width is preserved"), !FMath::IsNearlyEqual(InBoundsTopWidth, InBoundsBottomWidth, 0.01f));

	TArray<FVector2D> Shifted = InBounds;
	for (FVector2D& Point : Shifted) Point.X += 70.0f;
	TestTrue(TEXT("Boundary clipping keeps the visible portion"),
		UCombatMinimapWidget::ClipCameraTrapezoidToMap(Shifted, MapSize));
	TestTrue(TEXT("Clipped outline keeps at least three visible corners"), Shifted.Num() >= 3);
	float InBoundsCenterX = 0.0f;
	for (const FVector2D& Point : InBounds) InBoundsCenterX += Point.X;
	InBoundsCenterX /= InBounds.Num();
	float ShiftedCenterX = 0.0f;
	for (const FVector2D& Point : Shifted) ShiftedCenterX += Point.X;
	ShiftedCenterX /= Shifted.Num();
	TestTrue(TEXT("Visible outline continues moving toward the edge"), ShiftedCenterX > InBoundsCenterX + 0.5f);
	for (const FVector2D& Point : Shifted)
	{
		TestTrue(TEXT("Camera corner is finite"), !Point.ContainsNaN() && FMath::IsFinite(Point.X) && FMath::IsFinite(Point.Y));
		TestTrue(TEXT("Camera corner stays inside map surface"), Point.X >= 0.0f && Point.X <= 210.0f
			&& Point.Y >= 0.0f && Point.Y <= 210.0f);
	}
	double AreaTwice = 0.0;
	for (int32 Index = 0; Index < Shifted.Num(); ++Index)
	{
		const FVector2D& Current = Shifted[Index];
		const FVector2D& Next = Shifted[(Index + 1) % Shifted.Num()];
		AreaTwice += static_cast<double>(Current.X) * Next.Y - static_cast<double>(Next.X) * Current.Y;
	}
	const float Area = FMath::Abs(static_cast<float>(AreaTwice * 0.5));
	TestTrue(TEXT("Camera outline keeps non-zero area"), Area > 1.0f);
	return true;
}

/** 验证北向投影、边界拒绝与反投影不会把世界轴或地图命中方向写反。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatMinimapProjectionTest,
	"Combat.UI.Minimap.ProjectionRoundTrip", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCombatMinimapProjectionTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	const FVector2D Min(-1000.0f, -500.0f);
	const FVector2D Max(1000.0f, 500.0f);
	FVector2D Normalized;
	TestTrue(TEXT("World center projects to map center"),
		UCombatMinimapWidget::ProjectWorldToMap(FVector2D::ZeroVector, Min, Max, Normalized));
	TestTrue(TEXT("Center is 0.5/0.5"), Normalized.Equals(FVector2D(0.5f, 0.5f), 0.0001f));
	TestTrue(TEXT("North/east corner projects top right"),
		UCombatMinimapWidget::ProjectWorldToMap(FVector2D(1000.0f, 500.0f), Min, Max, Normalized));
	TestTrue(TEXT("North/east is top right"), Normalized.Equals(FVector2D(1.0f, 0.0f), 0.0001f));
	FVector2D World;
	TestTrue(TEXT("Inverse accepts normalized point"),
		UCombatMinimapWidget::UnprojectMapToWorld(FVector2D(0.25f, 0.75f), Min, Max, World));
	TestTrue(TEXT("Inverse round trip"), World.Equals(FVector2D(-500.0f, -250.0f), 0.0001f));
	TestFalse(TEXT("Outside world is rejected"),
		UCombatMinimapWidget::ProjectWorldToMap(FVector2D(1001.0f, 0.0f), Min, Max, Normalized));
	TestFalse(TEXT("Outside map click is rejected"),
		UCombatMinimapWidget::UnprojectMapToWorld(FVector2D(1.01f, 0.5f), Min, Max, World));
	TestFalse(TEXT("Degenerate bounds are rejected"),
		UCombatMinimapWidget::ProjectWorldToMap(FVector2D::ZeroVector, Min, FVector2D(-1000.0f, 500.0f), Normalized));
	return true;
}

/** 验证小地图定位只改本地 Command Pawn 的 XY，保留高度、清理跟随且不改 Combat Unit。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatMinimapCameraRelocationTest,
	"Combat.UI.Minimap.CameraRelocationIsLocal", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCombatMinimapCameraRelocationTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FCombatAutomationWorldFixture Fixture;
	if (!Fixture.IsValid()) return false;
	UWorld* World = Fixture.GetWorld();
	ACombatPlayerController* PC = World->SpawnActor<ACombatPlayerController>();
	ACombatCharacter* Camera = World->SpawnActor<ACombatCharacter>();
	ACombatUnitCharacter* Unit = World->SpawnActor<ACombatUnitCharacter>();
	if (!TestNotNull(TEXT("Player controller"), PC) || !TestNotNull(TEXT("Camera pawn"), Camera)
		|| !TestNotNull(TEXT("Combat unit"), Unit)) return false;
	PC->SetAsLocalPlayerController();
	Unit->SetActorLocation(FVector(100.0f, 200.0f, 30.0f));
	PC->SetCommandedUnitAuthority(Unit);
	PC->Possess(Camera);
	Camera->SetActorLocation(FVector(400.0f, -300.0f, 777.0f));
	const FVector UnitBefore = Unit->GetActorLocation();
	TestTrue(TEXT("Local minimap relocation succeeds"), PC->RelocateCameraFromMinimap(FVector2D(-800.0f, 600.0f)));
	TestTrue(TEXT("Camera XY follows map click"), Camera->GetActorLocation().Equals(FVector(-800.0f, 600.0f, 777.0f), 0.01f));
	TestEqual(TEXT("Relocation leaves camera free"), Camera->GetCameraMode(), ECombatCameraMode::Free);
	TestEqual(TEXT("Minimap never moves Combat Unit"), Unit->GetActorLocation(), UnitBefore);
	return true;
}

#endif
