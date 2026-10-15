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
