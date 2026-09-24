#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Combat/Tests/CombatAutomationWorldFixture.h"
#include "Combat/Unit/CombatUnitCharacter.h"
#include "Combat/Data/CombatDefinitionData.h"

/** 在真实平地碰撞中持续汇聚；直接输入仅隔离 CMC，完整 Crowd/Order 到达另由 Dedicated 验证。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatUnitConvergingCollisionTest,
	"Combat.SAM.ConvergingUnitsStayGrounded", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCombatUnitConvergingCollisionTest::RunTest(const FString& Parameters)
{
	for (const bool bCrossing : {false, true})
	{
		FCombatAutomationWorldFixture Fixture;
		auto* World = Fixture.GetWorld();
		auto* Floor = World->SpawnActor<AActor>();
		auto* Box = NewObject<UBoxComponent>(Floor);
		Floor->SetRootComponent(Box);
		Box->SetBoxExtent(FVector(2000, 2000, 20));
		Box->SetCollisionProfileName(TEXT("BlockAll"));
		Box->RegisterComponent();
		Floor->SetActorLocation(FVector(0, 0, -20));
		TArray<ACombatUnitCharacter*> Units;
		FActorSpawnParameters Spawn;
		Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		for (int32 Index = 0; Index < 2; ++Index)
		{
			const FVector Location(bCrossing && Index == 1 ? 400 : -400, Index == 0 ? -50 : 50, 100);
			auto* Unit = World->SpawnActor<ACombatUnitCharacter>(Location, FRotator::ZeroRotator, Spawn);
			auto* Data = NewObject<UCombatUnitData>(Unit);
			Data->DefinitionName = TEXT("collision_regression");
			if (!Unit->InitializeFromUnitData(Data)) return false;
			Unit->SpawnDefaultController();
			Units.Add(Unit);
		}
		constexpr float Dt = 1.0f / 60;
		// 同步 Automation 内 GFrameCounter 不推进，显式执行连续移动，避免只测到一帧静止结果。
		for (int32 I = 0; I < 60; ++I)
			for (auto* Unit : Units) Unit->GetCharacterMovement()->TickComponent(Dt, LEVELTICK_All, nullptr);
		const double GroundZ = Units[0]->GetActorLocation().Z;
		double MaxRise = 0, MaxStep = 0, MaxVerticalSpeed = 0;
		for (int32 Frame = 0; Frame < 600; ++Frame)
		{
			FVector Before[2];
			for (int32 I = 0; I < 2; ++I)
			{
				Before[I] = Units[I]->GetActorLocation();
				if (Before[I].Size2D() > 25)
					Units[I]->AddMovementInput((-Before[I]).GetSafeNormal2D());
			}
			for (auto* Unit : Units) Unit->GetCharacterMovement()->TickComponent(Dt, LEVELTICK_All, nullptr);
			for (int32 I = 0; I < 2; ++I)
			{
				MaxRise = FMath::Max(MaxRise, Units[I]->GetActorLocation().Z - GroundZ);
				MaxStep = FMath::Max(MaxStep, FVector::Dist2D(Before[I], Units[I]->GetActorLocation()));
				MaxVerticalSpeed = FMath::Max(MaxVerticalSpeed, double(Units[I]->GetVelocity().Z));
			}
		}
		AddInfo(FString::Printf(TEXT("Crossing=%d GroundZ=%.2f Rise=%.2f Step=%.2f VerticalSpeed=%.2f FinalDistance=%.2f"),
			bCrossing, GroundZ, MaxRise, MaxStep, MaxVerticalSpeed, FVector::Dist2D(Units[0]->GetActorLocation(), Units[1]->GetActorLocation())));
		TestTrue(TEXT("Units do not launch above flat ground"), MaxRise < 5 && MaxVerticalSpeed < 5);
		TestTrue(TEXT("Unit contacts cannot eject faster than normal movement"), MaxStep <= Units[0]->GetCharacterMovement()->GetMaxSpeed() * Dt + 2);
		TestTrue(TEXT("Both units reached the contested destination area"), Units[0]->GetActorLocation().Size2D() < 100 && Units[1]->GetActorLocation().Size2D() < 100);
	}
	return true;
}

/** 胶囊顶部接触不能被判为地面，也不能触发 Character 的自动跳离冲量。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatUnitLandingContactTest,
	"Combat.SAM.UnitContactDoesNotBounce", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCombatUnitLandingContactTest::RunTest(const FString& Parameters)
{
	FCombatAutomationWorldFixture Fixture;
	auto* World = Fixture.GetWorld();
	auto* Floor = World->SpawnActor<AActor>();
	auto* Box = NewObject<UBoxComponent>(Floor);
	Floor->SetRootComponent(Box);
	Box->SetBoxExtent(FVector(2000, 2000, 20));
	Box->SetCollisionProfileName(TEXT("BlockAll"));
	Box->RegisterComponent();
	Floor->SetActorLocation(FVector(0, 0, -20));
	FActorSpawnParameters Spawn;
	Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	auto* Lower = World->SpawnActor<ACombatUnitCharacter>(FVector(0, 0, 90.15), FRotator::ZeroRotator, Spawn);
	auto* Upper = World->SpawnActor<ACombatUnitCharacter>(FVector(20, 0, 285), FRotator::ZeroRotator, Spawn);
	for (auto* Unit : {Lower, Upper})
	{
		auto* Data = NewObject<UCombatUnitData>(Unit);
		Data->DefinitionName = TEXT("contact_regression");
		if (!Unit->InitializeFromUnitData(Data)) return false;
		Unit->SpawnDefaultController();
	}
	auto* Movement = Upper->GetCharacterMovement();
	FHitResult UnitHit(Lower, Lower->GetCapsuleComponent(), FVector(0, 0, 178), FVector::UpVector);
	UnitHit.bBlockingHit = true;
	FHitResult FloorHit(Floor, Box, FVector::ZeroVector, FVector::UpVector);
	FloorHit.bBlockingHit = true;
	TestTrue(TEXT("Normal terrain remains walkable"), Movement->IsWalkable(FloorHit));
	TestFalse(TEXT("Unit capsule is not a walkable floor"), Movement->IsWalkable(UnitHit));
	Movement->SetMovementMode(MOVE_Falling);
	double MaxUpwardSpeed = 0;
	for (int32 Frame = 0; Frame < 300; ++Frame)
	{
		Movement->TickComponent(1.0f / 60, LEVELTICK_All, nullptr);
		MaxUpwardSpeed = FMath::Max(MaxUpwardSpeed, double(Movement->Velocity.Z));
	}
	AddInfo(FString::Printf(TEXT("Unit contact MaxUpwardSpeed=%.2f FinalPosition=%s"), MaxUpwardSpeed, *Upper->GetActorLocation().ToString()));
	TestTrue(TEXT("Landing against another unit never injects an upward impulse"), MaxUpwardSpeed < 1);
	return true;
}
#endif
