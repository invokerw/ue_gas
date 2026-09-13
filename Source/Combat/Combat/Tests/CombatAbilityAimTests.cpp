#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Combat/Ability/CombatAbilityIndicatorGeometry.h"
#include "Combat/Core/CombatTags.h"
#include "Combat/Data/CombatDefinitionData.h"
#include "Combat/Tests/CombatAutomationWorldFixture.h"
#include "Combat/UI/CombatAbilityIndicatorGround.h"
#include "Combat/Unit/CombatUnitCharacter.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"

/** 验证预览按显式 Action/等级/覆盖读取真实参数；旧资产及损坏配置不产生虚构范围。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatAbilityIndicatorGeometryTest,
	"Combat.Input.AbilityAim.ActionGeometry", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCombatAbilityIndicatorGeometryTest::RunTest(const FString& Parameters)
{
	UCombatAbilityData* Data = NewObject<UCombatAbilityData>();
	Data->DefinitionName = TEXT("indicator_geometry");
	Data->MaxLevel = 2;
	Data->BehaviorTags.AddTag(CombatTags::Ability_Behavior_PointTarget);
	Data->TargetingRules.TargetTeamTag = CombatTags::TargetTeam_Enemy;
	Data->SpecialValues.FindOrAdd(TEXT("damage")).Values = { 40.0f, 80.0f };
	Data->SpecialValues.FindOrAdd(TEXT("radius")).Values = { 150.0f, 250.0f };
	FCombatAbilityAction Circle;
	Circle.Type = ECombatAbilityActionType::Damage;
	Circle.Target = ECombatAbilityActionTarget::UnitsInRadius;
	Circle.MagnitudeKey = TEXT("damage");
	Circle.RadiusKey = TEXT("radius");
	Data->Actions = { Circle };
	FCombatAbilityIndicatorGeometry Geometry;
	TestTrue(TEXT("Old asset remains valid"), Data->ResolveIndicatorGeometry(1, Geometry));
	TestEqual(TEXT("No configured action never guesses AoE"), Geometry.Shape, ECombatIndicatorShape::None);
	Data->IndicatorActionIndex = 0;
	TestTrue(TEXT("Explicit AoE resolves"), Data->ResolveIndicatorGeometry(2, Geometry));
	TestEqual(TEXT("Level two radius"), Geometry.Radius, 250.0f);
	TestFalse(TEXT("Point AoE follows cursor"), Geometry.bCenterOnCaster);
	Data->IndicatorActionIndex = 9;
	FString Diagnostic;
	TestFalse(TEXT("Bad index is rejected by runtime asset validator"), Data->ValidateRuntime(Diagnostic));
	TestFalse(TEXT("Bad index has no guessed geometry"), Data->ResolveIndicatorGeometry(1, Geometry));
	TestEqual(TEXT("Failure resets old shape"), Geometry.Shape, ECombatIndicatorShape::None);
	Data->IndicatorActionIndex = 0;
	UCombatProjectileData* Projectile = NewObject<UCombatProjectileData>(Data);
	Projectile->Radius = 45.0f;
	Projectile->MaxDistance = 1200.0f;
	Data->Actions[0].Type = ECombatAbilityActionType::SpawnLinearProjectile;
	Data->Actions[0].ProjectileData = Projectile;
	Data->Actions[0].RadiusKey = NAME_None;
	TestTrue(TEXT("Linear defaults resolve"), Data->ResolveIndicatorGeometry(1, Geometry));
	TestEqual(TEXT("Projectile base half width"), Geometry.Radius, 45.0f);
	TestEqual(TEXT("Projectile base travel length"), Geometry.Length, 1200.0f);
	Data->Actions[0].RadiusKey = TEXT("radius");
	Data->Actions[0].ProjectileRangeKey = TEXT("range");
	Data->SpecialValues.FindOrAdd(TEXT("range")).Values = { -1.0f, 1800.0f };
	TestTrue(TEXT("Negative override uses projectile fallback"), Data->ResolveIndicatorGeometry(1, Geometry));
	TestEqual(TEXT("Fallback matches projectile pipeline"), Geometry.Length, 1200.0f);
	TestTrue(TEXT("Level two overrides resolve"), Data->ResolveIndicatorGeometry(2, Geometry));
	TestEqual(TEXT("Override length"), Geometry.Length, 1800.0f);
	TestEqual(TEXT("Override half width"), Geometry.Radius, 250.0f);
	TestTrue(TEXT("Line starts at caster"), Geometry.bCenterOnCaster);
	TestEqual(TEXT("Ground footprint projects actual 3D travel"), Geometry.GetPlanarLineLength(FVector(0,0,400), FVector(300,0,0), FVector::ForwardVector), 1080.0f);
	TestEqual(TEXT("Zero direction follows caster forward fallback"), Geometry.GetPlanarLineLength(FVector::ZeroVector, FVector::ZeroVector, FVector::ForwardVector), 1800.0f);
	Data->SpecialValues[TEXT("range")].Values[1] = 0.0f;
	TestFalse(TEXT("Zero projectile range fails instead of displaying a fake line"), Data->ValidateRuntime(Diagnostic));
	Data->Actions[0] = Circle;
	Data->BehaviorTags.RemoveTag(CombatTags::Ability_Behavior_PointTarget);
	Data->BehaviorTags.AddTag(CombatTags::Ability_Behavior_NoTarget);
	Data->TargetingRules.TargetTeamTag = CombatTags::TargetTeam_None;
	TestTrue(TEXT("No target AoE resolves caster anchor"), Data->ResolveIndicatorGeometry(1, Geometry));
	TestTrue(TEXT("No target action uses caster location"), Geometry.bCenterOnCaster);
	return true;
}

/** 使用真实碰撞场景验证地面射线穿过单位/道具，并保留斜坡和高台的命中高度。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatAbilityIndicatorGroundTest,
	"Combat.Input.AbilityAim.GroundReceivers", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCombatAbilityIndicatorGroundTest::RunTest(const FString& Parameters)
{
	using namespace CombatAbilityIndicatorGround;
	FCombatAutomationWorldFixture Fixture;
	if (!Fixture.IsValid()) return false;
	UWorld& World = *Fixture.GetWorld();
	UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (!TestNotNull(TEXT("Collision mesh exists"), Cube)) return false;
	const auto MakeSurface = [&World, Cube](const FVector Location, const FVector Scale, const FRotator Rotation, const bool bGround)
	{
		AStaticMeshActor* Actor = World.SpawnActor<AStaticMeshActor>(Location, Rotation);
		UStaticMeshComponent* Mesh = Actor->GetStaticMeshComponent();
		Mesh->SetMobility(EComponentMobility::Movable);
		Mesh->SetStaticMesh(Cube);
		Mesh->SetWorldScale3D(Scale);
		Mesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		Mesh->SetCollisionResponseToAllChannels(ECR_Ignore);
		Mesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
		Mesh->SetCollisionResponseToChannel(TraceChannel, bGround ? ECR_Block : ECR_Ignore);
		Mesh->SetRenderCustomDepth(bGround);
		Mesh->SetCustomDepthStencilValue(bGround ? StencilBit | 19 : 19);
		return Actor;
	};
	AStaticMeshActor* Floor = MakeSurface(FVector(0, 0, -10), FVector(30, 30, 0.2), FRotator::ZeroRotator, true);
	ACombatUnitCharacter* Hero = World.SpawnActor<ACombatUnitCharacter>(FVector(-300, 0, 100), FRotator::ZeroRotator);
	MakeSurface(FVector(300, 0, 100), FVector(1, 1, 2), FRotator::ZeroRotator, false);
	AStaticMeshActor* Ramp = MakeSurface(FVector(700, 0, 150), FVector(3, 3, 0.2), FRotator(30, 0, 0), true);
	AStaticMeshActor* Platform = MakeSurface(FVector(1100, 0, 250), FVector(3, 3, 0.2), FRotator::ZeroRotator, true);
	FCollisionQueryParams Query(SCENE_QUERY_STAT(CombatIndicatorGroundTest), true);
	FHitResult Hit;
	const auto Trace = [&World, &Hit, &Query](const float X, const ECollisionChannel Channel)
	{
		return World.LineTraceSingleByChannel(Hit, FVector(X, 0, 600), FVector(X, 0, -100), Channel, Query);
	};
	TestTrue(TEXT("Visibility still finds hero for unit skills"), Trace(-300, ECC_Visibility) && Hit.GetActor() == Hero);
	TestFalse(TEXT("Hero hit is not a valid point location"), IsGroundHit(Hit));
	TestTrue(TEXT("Ground trace passes through hero to floor"), Trace(-300, TraceChannel) && Hit.GetActor() == Floor && IsGroundHit(Hit));
	TestTrue(TEXT("Hero does not raise floor height"), FMath::IsNearlyZero(Hit.Location.Z, 0.1));
	TestTrue(TEXT("Ground trace passes through unmarked prop"), Trace(300, TraceChannel) && Hit.GetActor() == Floor && IsGroundHit(Hit));
	TestTrue(TEXT("Ramp retains actual sloped surface"), Trace(700, TraceChannel) && Hit.GetActor() == Ramp && IsGroundHit(Hit));
	TestTrue(TEXT("Ramp height is above base floor"), Hit.Location.Z > 140 && Hit.ImpactNormal.Z > 0.8 && Hit.ImpactNormal.Z < 0.9);
	TestTrue(TEXT("Raised platform is ground"), Trace(1100, TraceChannel) && Hit.GetActor() == Platform && IsGroundHit(Hit));
	TestTrue(TEXT("Platform height retained"), FMath::IsNearlyEqual(Hit.Location.Z, 260.0, 0.1));
	TestTrue(TEXT("Other stencil bits remain supported"), (Platform->GetStaticMeshComponent()->CustomDepthStencilValue & 127) == 19);
	Floor->GetStaticMeshComponent()->SetCustomDepthStencilValue(19);
	TestTrue(TEXT("Collision can still hit an incorrectly configured receiver"), Trace(300, TraceChannel));
	TestFalse(TEXT("Missing ground stencil fails closed"), IsGroundHit(Hit));
	TestFalse(TEXT("Sky or map edge has no fallback target"), Trace(4000, TraceChannel));
	TestFalse(TEXT("Empty hit is rejected"), IsGroundHit(Hit));
	return true;
}
#endif
