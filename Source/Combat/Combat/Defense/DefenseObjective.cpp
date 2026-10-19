#include "Combat/Defense/DefenseObjective.h"
#include "Components/StaticMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "UObject/ConstructorHelpers.h"
ADefenseEnemy::ADefenseEnemy(const FObjectInitializer& Init) : Super(Init)
{
	Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DefenseBody"));
	Body->SetupAttachment(GetRootComponent());
	Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Body->SetCanEverAffectNavigation(false);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> ShapeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	Body->SetStaticMesh(ShapeMesh.Object);
	Body->SetRelativeScale3D(FVector(0.65,0.65,1.3));
	bAlwaysRelevant = true;
}
ADefenseObjective::ADefenseObjective(const FObjectInitializer& Init) : Super(Init)
{
	static ConstructorHelpers::FObjectFinder<UStaticMesh> ShapeMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	Body->SetStaticMesh(ShapeMesh.Object);
	Body->SetRelativeScale3D(FVector(1.8,1.8,1.8));
	GetCapsuleComponent()->InitCapsuleSize(92,96);
}

ADefenseSiegeEnemy::ADefenseSiegeEnemy(const FObjectInitializer& Init) : Super(Init)
{
	Body->SetRelativeScale3D(FVector(1.05,0.75,0.85));
	Body->SetRelativeLocation(FVector(0,0,-25));
	Ram=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SiegeRam"));
	Ram->SetupAttachment(GetRootComponent());
	Ram->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Ram->SetCanEverAffectNavigation(false);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> ShapeMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	Ram->SetStaticMesh(ShapeMesh.Object);
	Ram->SetRelativeScale3D(FVector(0.22,0.22,1.4));
	Ram->SetRelativeRotation(FRotator(90,0,0));
	Ram->SetRelativeLocation(FVector(50,0,15));
}

ADefenseAssassinEnemy::ADefenseAssassinEnemy(const FObjectInitializer& Init) : Super(Init)
{
	static ConstructorHelpers::FObjectFinder<UStaticMesh> ShapeMesh(TEXT("/Engine/BasicShapes/Cone.Cone"));
	Body->SetStaticMesh(ShapeMesh.Object);
	Body->SetRelativeScale3D(FVector(0.55,0.55,1.65));
}
