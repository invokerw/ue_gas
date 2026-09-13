#include "Combat/Tests/CombatAbilityAimScenarioActor.h"
#include "CombatPlayerController.h"
#include "Combat/UI/CombatAbilityAimComponent.h"
#include "Combat/UI/CombatAbilityIndicatorActor.h"
#include "Combat/UI/CombatAbilityIndicatorGround.h"
#include "Combat/UI/CombatHUDWidget.h"
#include "Combat/UI/CombatLogWidget.h"
#include "Combat/UI/CombatPlayerHUD.h"
#include "Combat/Unit/CombatUnitCharacter.h"
#include "Combat/View/CombatUnitViewComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/DecalComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "InputKeyEventArgs.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "ImageUtils.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "TextureResource.h"
#include "NavigationPath.h"
#include "NavigationSystem.h"
#include "UnrealClient.h"
#include "Framework/Application/SlateApplication.h"

ACombatAbilityAimScenarioActor::ACombatAbilityAimScenarioActor()
{
	PrimaryActorTick.bCanEverTick = true;
	SetActorEnableCollision(false);
	bReplicates = false;
}

void ACombatAbilityAimScenarioActor::BeginPlay()
{
	Super::BeginPlay();
	if (!FParse::Param(FCommandLine::Get(), TEXT("CombatAimPIESmoke")) || GetNetMode() == NM_DedicatedServer) Destroy();
}

void ACombatAbilityAimScenarioActor::Key(const FKey Input, const EInputEvent Event)
{
	if (Player.IsValid()) Player->InputKey(FInputKeyEventArgs::CreateSimulated(Input, Event, Event == IE_Pressed ? 1.0f : 0.0f));
}

void ACombatAbilityAimScenarioActor::PointAt(const FVector Location)
{
	FVector2D Screen;
	if (Player.IsValid() && Player->ProjectWorldLocationToScreen(Location, Screen)) Player->SetMouseLocation(FMath::RoundToInt(Screen.X), FMath::RoundToInt(Screen.Y));
}

void ACombatAbilityAimScenarioActor::Check(const bool bCondition, const TCHAR* Description)
{
	bPassed &= bCondition;
	UE_LOG(LogTemp, Display, TEXT("AIMPIE Step=%d Check=%s Result=%s"), Step, Description, bCondition ? TEXT("Pass") : TEXT("Fail"));
}

void ACombatAbilityAimScenarioActor::Screenshot(const TCHAR* Name)
{
	const TCHAR* Folder = FParse::Param(FCommandLine::Get(), TEXT("CombatAimGroundSmoke")) ? TEXT("SkillIndicatorGround") : TEXT("SkillIndicators");
	FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir() / Folder / Name, true, false, false, FIntRect(), true);
}

void ACombatAbilityAimScenarioActor::PrepareGroundVisual(ACombatUnitCharacter* Unit)
{
	CaptureOrigin = Unit->GetActorLocation() - FVector(0, 0, Unit->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
	CapturedUnit = Unit;
	bSavedPauseAnims = Unit->GetMesh()->bPauseAnims;
	Unit->GetMesh()->bPauseAnims = true;
	HeroSample = Unit->GetMesh()->GetSocketLocation(TEXT("head")) - CaptureOrigin;
	GroundTarget = NewObject<UTextureRenderTarget2D>(this);
	GroundTarget->RenderTargetFormat = RTF_RGBA8;
	GroundTarget->InitAutoFormat(512, 512);
	GroundTarget->UpdateResourceImmediate();
	GroundCapture = NewObject<USceneCaptureComponent2D>(this);
	GroundCapture->bCaptureEveryFrame = false;
	GroundCapture->bCaptureOnMovement = false;
	GroundCapture->TextureTarget = GroundTarget;
	GroundCapture->CaptureSource = SCS_FinalColorLDR;
	GroundCapture->ProjectionType = ECameraProjectionMode::Orthographic;
	GroundCapture->OrthoWidth = 1200;
	GroundCapture->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
	// OnRegister 会从 Archetype 重置 ShowFlags，测试隔离设置必须在注册之后应用。
	GroundCapture->RegisterComponent();
	GroundCapture->ShowFlags.SetAntiAliasing(false);
	GroundCapture->ShowFlags.SetTemporalAA(false);
	GroundCapture->ShowFlags.SetEyeAdaptation(false);
	// 排除局部曝光和泛光；亮地面引起邻近物体的明暗变化不等于贴花覆盖。
	GroundCapture->ShowFlags.SetLocalExposure(false);
	GroundCapture->ShowFlags.SetBloom(false);
	GroundCapture->ShowFlags.SetMotionBlur(false);
	GroundCapture->ShowFlags.SetDynamicShadows(false);
	GroundCapture->ShowFlags.SetAmbientOcclusion(false);
	GroundCapture->ShowFlags.SetGlobalIllumination(false);
	GroundCapture->ShowFlags.SetFog(false);
	Check(!GroundCapture->ShowFlags.Bloom && !GroundCapture->ShowFlags.EyeAdaptation && !GroundCapture->ShowFlags.LocalExposure
		&& !GroundCapture->ShowFlags.TemporalAA, TEXT("CaptureExcludesLensAndTemporalEffects"));
	GroundCapture->SetWorldLocationAndRotation(CaptureOrigin + FVector(0, 0, 1200), FRotator(-90, 0, 0));
	GroundCapture->ShowOnlyActorComponents(Unit);
	UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	const auto Surface = [this, Cube](const FVector Offset, const FVector Scale, const FRotator Rotation, const int32 Stencil)
	{
		AStaticMeshActor* Actor = GetWorld()->SpawnActor<AStaticMeshActor>(CaptureOrigin + Offset, Rotation);
		VisualFixtures.Add(Actor);
		UStaticMeshComponent* Mesh = Actor->GetStaticMeshComponent();
		Mesh->SetMobility(EComponentMobility::Movable);
		Mesh->SetStaticMesh(Cube);
		Mesh->SetWorldScale3D(Scale);
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Mesh->SetRenderCustomDepth(Stencil != 0);
		Mesh->SetCustomDepthStencilValue(Stencil);
		GroundCapture->ShowOnlyActorComponents(Actor);
	};
	Surface(FVector(0, 0, -5), FVector(12, 12, 0.1), FRotator::ZeroRotator, 128);
	Surface(FVector(-260, 0, 90), FVector(1.5, 1.5, 1.8), FRotator::ZeroRotator, 0);
	Surface(FVector(260, 0, 90), FVector(1.5, 1.5, 1.8), FRotator::ZeroRotator, 19);
	Surface(FVector(260, 260, 100), FVector(1.7, 1.7, 0.15), FRotator(30, 0, 0), 128);
	Surface(FVector(-260, 260, 180), FVector(1.7, 1.7, 0.2), FRotator::ZeroRotator, 128 | 19);
	GroundIndicator = GetWorld()->SpawnActor<ACombatAbilityIndicatorActor>();
	VisualFixtures.Add(GroundIndicator);
	GroundIndicator->SetActorHiddenInGame(true);
}

bool ACombatAbilityAimScenarioActor::ReadGroundFrame(const TCHAR* Name, TArray<FColor>& OutPixels)
{
	if (!GroundCapture || !GroundTarget) return false;
	GroundCapture->CaptureScene();
	FTextureRenderTargetResource* Resource = GroundTarget->GameThread_GetRenderTargetResource();
	if (!Resource || !Resource->ReadPixels(OutPixels) || OutPixels.Num() != 512 * 512) return false;
	TArray64<uint8> Png;
	FImageUtils::PNGCompressImageArray(512, 512, OutPixels, Png);
	return FFileHelper::SaveArrayToFile(Png, *(FPaths::ProjectSavedDir() / TEXT("SkillIndicatorGround") / Name));
}

void ACombatAbilityAimScenarioActor::CheckGroundPixels(const TArray<FColor>& Pixels, const bool bLine)
{
	if (Pixels.Num() != 512 * 512 || GroundBaseline.Num() != Pixels.Num()) { Check(false, TEXT("GroundPixelBuffersValid")); return; }
	const auto Difference = [&Pixels, this](const FVector Offset, const int32 Radius)
	{
		// 相机沿 -Z 正交俯视，屏幕右为世界 +Y，屏幕上为世界 +X。
		const int32 X = FMath::RoundToInt(256 + Offset.Y * 512 / 1200);
		const int32 Y = FMath::RoundToInt(256 - Offset.X * 512 / 1200);
		double Sum = 0;
		int32 Count = 0;
		for (int32 V = Y - Radius; V <= Y + Radius; ++V)
		{
			for (int32 U = X - Radius; U <= X + Radius; ++U)
			{
				if (U < 0 || V < 0 || U >= 512 || V >= 512) continue;
				const FColor A = Pixels[V * 512 + U], B = GroundBaseline[V * 512 + U];
				Sum += FMath::Max3(FMath::Abs(int32(A.R) - B.R), FMath::Abs(int32(A.G) - B.G), FMath::Abs(int32(A.B) - B.B));
				++Count;
			}
		}
		return Count ? Sum / Count : 255.0;
	};
	const double Floor = Difference(FVector(120, 0, 0), 6);
	const double Hero = Difference(HeroSample, 2);
	const double Prop = Difference(FVector(-260, 0, 0), 12);
	const double OtherStencil = Difference(FVector(260, 0, 0), 12);
	const double Ramp = Difference(FVector(260, 260, 0), 8);
	const double Platform = Difference(FVector(-260, 260, 0), 8);
	UE_LOG(LogTemp, Display, TEXT("AIMGroundHeroSample Offset=%s Pixel=(%.1f,%.1f)"), *HeroSample.ToString(),
		256 + HeroSample.Y * 512 / 1200, 256 - HeroSample.X * 512 / 1200);
	UE_LOG(LogTemp, Display, TEXT("AIMGroundPixels Shape=%s Floor=%.3f Hero=%.3f Prop=%.3f OtherStencil=%.3f Ramp=%.3f Platform=%.3f"),
		bLine ? TEXT("Line") : TEXT("Circle"), Floor, Hero, Prop, OtherStencil, Ramp, Platform);
	Check(Floor > 8, TEXT("IndicatorChangesGroundPixels"));
	Check(Hero <= 2, TEXT("IndicatorLeavesHeroPixelsUnchanged"));
	Check(Prop <= 2, TEXT("GroundBehindPropDoesNotColorProp"));
	Check(OtherStencil <= 2, TEXT("OtherStencilDoesNotReceiveIndicator"));
	if (!bLine)
	{
		Check(Ramp > 8, TEXT("MarkedRampReceivesIndicator"));
		Check(Platform > 8, TEXT("RaisedPlatformReceivesIndicator"));
	}
}

void ACombatAbilityAimScenarioActor::CleanupGroundVisual()
{
	if (CapturedUnit.IsValid()) CapturedUnit->GetMesh()->bPauseAnims = bSavedPauseAnims;
	CapturedUnit.Reset();
	if (GroundCapture) GroundCapture->DestroyComponent();
	GroundCapture = nullptr;
	GroundTarget = nullptr;
	for (TWeakObjectPtr<AActor> Actor : VisualFixtures) if (Actor.IsValid()) Actor->Destroy();
	VisualFixtures.Reset();
	GroundIndicator = nullptr;
}

void ACombatAbilityAimScenarioActor::EndPlay(const EEndPlayReason::Type Reason)
{
	CleanupGroundVisual();
	Super::EndPlay(Reason);
}

void ACombatAbilityAimScenarioActor::Tick(const float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (bFinished) return;
	Elapsed += DeltaTime;
	if (!Player.IsValid())
	{
		Player = Cast<ACombatPlayerController>(GetWorld()->GetFirstPlayerController());
		if (!Player.IsValid()) return;
	}
	ACombatPlayerController* PC = Player.Get();
	ACombatUnitCharacter* Unit = PC->GetCommandedUnit();
	if (!Unit || !PC->GetHUD() || Unit->GetCombatUnitViewComponent()->GetHUDOwnerView().Abilities.Num() != 4) return;
	if (Elapsed < (Step == 0 ? 3.0f : 0.8f)) return;
	Elapsed = 0;
	UCombatAbilityAimComponent* Aim = PC->GetAbilityAimComponent();
	switch (Step)
	{
	case 0:
		NearPoint = Unit->GetActorLocation() + FVector(0, 230, -90);
		PointAt(NearPoint);
		BaselineRequest = ExpectedRequest = PC->NextCombatOrderRequestId;
		Key(EKeys::W, IE_Pressed);
		break;
	case 1:
		Check(Aim->IsAiming() && Aim->GetPreview().Geometry.Shape == ECombatIndicatorShape::Circle, TEXT("W_CircleAim"));
		Check(PC->NextCombatOrderRequestId == ExpectedRequest, TEXT("NoRequestWhileAiming"));
		Screenshot(TEXT("PIE-Circle.png"));
		Key(EKeys::W, IE_Released);
		break;
	case 2:
		Check(Aim->IsAiming(), TEXT("StandardReleaseKeepsAim"));
		Key(EKeys::RightMouseButton, IE_Pressed);
		break;
	case 3:
		Check(!Aim->IsAiming() && PC->NextCombatOrderRequestId == ExpectedRequest, TEXT("RightCancelsWithoutMove"));
		Key(EKeys::RightMouseButton, IE_Released);
		Key(EKeys::E, IE_Pressed);
		break;
	case 4:
		Check(Aim->IsAiming() && Aim->GetPreview().Geometry.Shape == ECombatIndicatorShape::Line, TEXT("E_LineAim"));
		Screenshot(TEXT("PIE-Line.png"));
		Key(EKeys::E, IE_Released);
		Key(EKeys::Escape, IE_Pressed);
		break;
	case 5:
		Check(!Aim->IsAiming(), TEXT("EscapeCancels"));
		Key(EKeys::Escape, IE_Released);
		PC->AbilityCastMode = ECombatAbilityCastMode::QuickPress;
		PointAt(NearPoint);
		Key(EKeys::W, IE_Pressed);
		++ExpectedRequest;
		break;
	case 6:
		Check(!Aim->IsAiming() && PC->NextCombatOrderRequestId == ExpectedRequest, TEXT("QuickPressSingleCast"));
		Key(EKeys::W, IE_Released);
		PC->AbilityCastMode = ECombatAbilityCastMode::QuickRelease;
		Key(EKeys::E, IE_Pressed);
		break;
	case 7:
		Check(Aim->IsAiming() && PC->NextCombatOrderRequestId == ExpectedRequest, TEXT("QuickReleaseWaits"));
		Key(EKeys::E, IE_Released);
		++ExpectedRequest;
		break;
	case 8:
		Check(!Aim->IsAiming() && PC->NextCombatOrderRequestId == ExpectedRequest, TEXT("QuickReleaseSingleCast"));
		PC->AbilityCastMode = ECombatAbilityCastMode::Standard;
		Key(EKeys::Q, IE_Pressed);
		break;
	case 9:
		Check(Aim->IsAiming() && Aim->GetPreview().Status == ECombatAbilityAimStatus::InvalidTarget, TEXT("UnitSkillGroundInvalid"));
		Key(EKeys::Q, IE_Released);
		Key(EKeys::LeftMouseButton, IE_Pressed);
		break;
	case 10:
		Check(Aim->IsAiming() && PC->NextCombatOrderRequestId == ExpectedRequest, TEXT("NoNearestUnitFallback"));
		Key(EKeys::LeftMouseButton, IE_Released);
		Key(EKeys::Escape, IE_Pressed);
		break;
	case 11:
		Key(EKeys::Escape, IE_Released);
		Key(EKeys::W, IE_Pressed);
		break;
	case 12:
		Key(EKeys::W, IE_Released);
		if (ACombatPlayerHUD* HUD = Cast<ACombatPlayerHUD>(PC->GetHUD()); HUD && HUD->GetCombatWidget())
		{
			const FGeometry Geometry = HUD->GetCombatWidget()->GetCachedGeometry();
			const FVector2D Point = Geometry.LocalToAbsolute(FVector2D(Geometry.GetLocalSize().X * 0.5f, Geometry.GetLocalSize().Y - 36.0f));
			FSlateApplication::Get().SetCursorPos(Point);
		}
		break;
	case 13:
		Check(PC->IsPointerOverCombatUI(), TEXT("HUDGeometryBlocksWorld"));
		Key(EKeys::LeftMouseButton, IE_Pressed);
		break;
	case 14:
		Check(PC->NextCombatOrderRequestId == ExpectedRequest, TEXT("HUDClickDoesNotCast"));
		Key(EKeys::LeftMouseButton, IE_Released);
		PC->FlushPressedKeys();
		Check(!Aim->IsAiming(), TEXT("ViewportFocusFlushCancels"));
		PointAt(NearPoint);
		Key(EKeys::R, IE_Pressed);
		++ExpectedRequest;
		break;
	case 15:
		Check(!Aim->IsAiming() && PC->NextCombatOrderRequestId == ExpectedRequest, TEXT("NoTargetImmediateCast"));
		Key(EKeys::R, IE_Released);
		{
			int32 VisualCount = 0;
			for (TActorIterator<ACombatAbilityIndicatorActor> It(GetWorld()); It; ++It) ++VisualCount;
			Check(VisualCount == 1, TEXT("ReuseOneVisualActor"));
		}
		PC->AbilityCastMode = ECombatAbilityCastMode::Standard;
		PC->CancelCombatTargeting();
		break;
	case 16:
		if (ACombatPlayerHUD* HUD = Cast<ACombatPlayerHUD>(PC->GetHUD()); HUD && HUD->GetCombatWidget())
		{
			const UWidget* Slot = HUD->GetCombatWidget()->GetWidgetFromName(TEXT("SkillW"));
			const FGeometry Geometry = Slot ? Slot->GetCachedGeometry() : FGeometry();
			Check(Geometry.GetLocalSize().X > 0 && Geometry.GetLocalSize().Y > 0, TEXT("HoveredSlotHasRealLayout"));
			FSlateApplication::Get().SetCursorPos(Geometry.LocalToAbsolute(Geometry.GetLocalSize() * 0.5f));
		}
		else Check(false, TEXT("HoveredSlotHasRealLayout"));
		break;
	case 17:
		Check(Aim->GetPreview().bVisible && !Aim->IsAiming() && !Aim->GetPreview().bHasTarget
			&& Aim->GetPreview().CastRadius > 0, TEXT("HoverShowsRangeWithoutPointAoE"));
		Check(PC->NextCombatOrderRequestId == ExpectedRequest, TEXT("HoverSendsNoRequest"));
		Screenshot(TEXT("PIE-Hover.png"));
		{
			// 选择实际可见的远处地面，保留视口射线；墙体挡住的投影点不能充当超距输入。
			int32 Width = 0, Height = 0;
			PC->GetViewportSize(Width, Height);
			bool bFound = false;
			NearPoint = Unit->GetActorLocation();
			const double GroundZ = NearPoint.Z - Unit->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
			for (int32 Y = 60; Y < Height - 180 && !bFound; Y += 40)
			{
				for (int32 X = 80; X < Width - 80; X += 40)
				{
					FHitResult Hit;
					if (!PC->GetHitResultAtScreenPosition(FVector2D(X, Y), ECC_Visibility, true, Hit)) continue;
					const double Distance = FVector::Dist2D(NearPoint, Hit.Location);
					if (Distance > 840 && Distance < 1200 && FMath::Abs(Hit.Location.Z - GroundZ) < 30)
					{
						const UNavigationPath* Path = UNavigationSystemV1::FindPathToLocationSynchronously(GetWorld(), NearPoint, Hit.Location, Unit);
						if (!Path || !Path->IsValid() || Path->IsPartial()) continue;
						PC->SetMouseLocation(X, Y);
						bFound = true;
						UE_LOG(LogTemp, Display, TEXT("AIMPIE ChaseStart=%s Target=%s"), *NearPoint.ToString(), *Hit.Location.ToString());
						break;
					}
				}
			}
			Check(bFound, TEXT("VisibleDistantFloorFound"));
		}
		Key(EKeys::W, IE_Pressed);
		break;
	case 18:
		Check(Aim->IsAiming() && Aim->GetPreview().Status == ECombatAbilityAimStatus::OutOfRange
			&& PC->NextCombatOrderRequestId == ExpectedRequest, TEXT("FarAimShowsOutOfRangeWithoutRequest"));
		Screenshot(TEXT("PIE-OutOfRange.png"));
		Key(EKeys::W, IE_Released);
		Key(EKeys::LeftMouseButton, IE_Pressed);
		++ExpectedRequest;
		break;
	case 19:
		Key(EKeys::LeftMouseButton, IE_Released);
		UE_LOG(LogTemp, Display, TEXT("AIMPIE ChaseDistance=%.3f"), FVector::Dist2D(NearPoint, Unit->GetActorLocation()));
		Check(PC->NextCombatOrderRequestId == ExpectedRequest && FVector::Dist2D(NearPoint, Unit->GetActorLocation()) > 10,
			TEXT("FarConfirmSendsOneRequestAndServerChases"));
		PC->CancelCombatTargeting();
		if (FParse::Param(FCommandLine::Get(), TEXT("CombatAimGroundSmoke")))
		{
			Key(EKeys::S, IE_Pressed);
			Key(EKeys::S, IE_Released);
			++ExpectedRequest;
			break;
		}
		bFinished = true;
		UE_LOG(LogTemp, Display, TEXT("AIMPIEComplete Requests=%d Result=%s"), PC->NextCombatOrderRequestId - BaselineRequest, bPassed ? TEXT("Pass") : TEXT("Fail"));
		break;
	case 20:
		PointAt(Unit->GetActorLocation() + FVector(0, 0, 25));
		Key(EKeys::W, IE_Pressed);
		break;
	case 21:
		{
			FHitResult Body, Ground;
			PC->GetHitResultUnderCursor(ECC_Visibility, true, Body);
			Check(Body.GetActor() == Unit, TEXT("CursorReallyHitsHeroBody"));
			Check(Aim->TraceAimHit(Ground) && CombatAbilityIndicatorGround::IsGroundHit(Ground)
				&& Ground.GetActor() != Unit, TEXT("PointSkillFindsGroundThroughHero"));
			FCombatOrderRequest Order;
			Check(Aim->BuildConfirmedOrder(Aim->GetSessionSerial(), Ground, true, Order)
				&& Order.TargetLocation.Equals(Ground.Location, 0.1)
				&& Aim->GetPreview().TargetLocation.Equals(Ground.Location, 0.1), TEXT("PreviewAndConfirmShareGroundHeight"));
			Check(Unit->GetMesh()->bReceivesDecals, TEXT("HeroCanStillReceiveOtherDecals"));
			Screenshot(TEXT("PIE-HeroGround.png"));
		}
		Key(EKeys::W, IE_Released);
		break;
	case 22:
		PC->CancelCombatTargeting();
		PrepareGroundVisual(Unit);
		break;
	case 23:
		Check(ReadGroundFrame(TEXT("GroundVisual-Off.png"), GroundBaseline), TEXT("ReadActualBaselinePixels"));
		break;
	case 24:
	case 25:
		{
			const bool bLine = Step == 25;
			FCombatAbilityAimPreview Visual;
			Visual.bVisible = Visual.bHasTarget = true;
			Visual.Status = ECombatAbilityAimStatus::Ready;
			Visual.CasterLocation = CaptureOrigin - FVector(500, 0, 0);
			Visual.TargetLocation = CaptureOrigin;
			Visual.Direction = FVector::ForwardVector;
			Visual.Geometry.Shape = bLine ? ECombatIndicatorShape::Line : ECombatIndicatorShape::Circle;
			Visual.Geometry.Radius = bLine ? 65 : 550;
			Visual.PlanarLineLength = 1000;
			GroundIndicator->ShowPreview(Visual);
			// 增强填充便于像素断言；仍使用生产材质的同一接收过滤和深度/法线逻辑。
			TArray<UDecalComponent*> Decals;
			GroundIndicator->GetComponents(Decals);
			for (UDecalComponent* Decal : Decals)
				if (UMaterialInstanceDynamic* Mat = Cast<UMaterialInstanceDynamic>(Decal->GetDecalMaterial())) Mat->SetScalarParameterValue(TEXT("Fill"), 0.75f);
			TArray<FColor> Pixels;
			Check(ReadGroundFrame(bLine ? TEXT("GroundVisual-Line.png") : TEXT("GroundVisual-Circle.png"), Pixels), TEXT("ReadActualIndicatorPixels"));
			CheckGroundPixels(Pixels, bLine);
		}
		break;
	case 26:
		CleanupGroundVisual();
		Check(PC->NextCombatOrderRequestId == ExpectedRequest, TEXT("VisualChecksSendNoGameplayRequests"));
		bFinished = true;
		UE_LOG(LogTemp, Display, TEXT("AIMPIEComplete Requests=%d Result=%s"), PC->NextCombatOrderRequestId - BaselineRequest, bPassed ? TEXT("Pass") : TEXT("Fail"));
		break;
	}
	++Step;
}
