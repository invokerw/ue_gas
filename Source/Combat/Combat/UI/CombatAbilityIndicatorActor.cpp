#include "Combat/UI/CombatAbilityIndicatorActor.h"
#include "Combat/UI/CombatAbilityAimComponent.h"
#include "Components/DecalComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

ACombatAbilityIndicatorActor::ACombatAbilityIndicatorActor()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;
	SetActorEnableCollision(false);
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("IndicatorRoot")));
	for (const FName Name : { FName(TEXT("CastRange")), FName(TEXT("EffectShape")), FName(TEXT("TargetMarker")) })
	{
		UDecalComponent* Decal = CreateDefaultSubobject<UDecalComponent>(Name);
		Decal->SetupAttachment(GetRootComponent());
		Decal->SetRelativeRotation(FRotator(-90, 0, 0));
		Decal->SetVisibility(false);
		Decal->FadeScreenSize = 0.0f;
		Decal->SortOrder = 50 + IndicatorLayers.Num();
		IndicatorLayers.Add(Decal);
	}
}

void ACombatAbilityIndicatorActor::ShowLayer(const int32 Index, const FVector Center, const FVector Direction,
	const float Radius, const float Length, const float Shape, const FLinearColor Color,
	const float Stroke, const float Fill, const bool bDashed)
{
	UDecalComponent* Decal = IndicatorLayers[Index];
	UMaterialInstanceDynamic* Material = Materials[Index];
	// 材质按地面 stencil、场景深度和法线筛接收者；投影高度覆盖坡道，不能靠压薄体积排除角色。
	const float Extent = FMath::Max(12.0f, Length * 0.5f + Radius + Stroke * 2.0f);
	Decal->DecalSize = FVector(FMath::Max(180.0f, Extent * 2.0f), Extent, Extent);
	Decal->SetWorldLocation(Center + FVector(0, 0, 30.0f));
	Decal->SetVisibility(true);
	Material->SetVectorParameterValue(TEXT("Center"), FLinearColor(Center));
	Material->SetVectorParameterValue(TEXT("Direction"), FLinearColor(Direction));
	Material->SetVectorParameterValue(TEXT("Tint"), Color);
	Material->SetScalarParameterValue(TEXT("Radius"), Radius);
	Material->SetScalarParameterValue(TEXT("Length"), Length);
	Material->SetScalarParameterValue(TEXT("Shape"), Shape);
	Material->SetScalarParameterValue(TEXT("Stroke"), Stroke);
	Material->SetScalarParameterValue(TEXT("Fill"), Fill);
	Material->SetScalarParameterValue(TEXT("Dashed"), bDashed ? 1.0f : 0.0f);
}

void ACombatAbilityIndicatorActor::ShowPreview(const FCombatAbilityAimPreview& Preview)
{
	SetActorHiddenInGame(!Preview.bVisible || GetNetMode() == NM_DedicatedServer);
	for (UDecalComponent* Layer : IndicatorLayers) Layer->SetVisibility(false);
	if (!Preview.bVisible || GetNetMode() == NM_DedicatedServer) return;
	if (Materials.IsEmpty())
	{
		// 单次同步加载已打包的小材质，无异步回调或逐帧动态材质分配。
		UMaterialInterface* Base = IndicatorMaterial.LoadSynchronous();
		if (!Base) { SetActorHiddenInGame(true); return; }
		for (UDecalComponent* Layer : IndicatorLayers)
		{
			UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(Base, this);
			Materials.Add(Material);
			Layer->SetDecalMaterial(Material);
		}
	}
	FLinearColor Color(0.22f, 0.95f, 0.42f);
	switch (Preview.Status)
	{
	case ECombatAbilityAimStatus::OutOfRange: Color = FLinearColor(1.0f, 0.55f, 0.10f); break;
	case ECombatAbilityAimStatus::InvalidTarget: Color = FLinearColor(1.0f, 0.16f, 0.12f); break;
	case ECombatAbilityAimStatus::Blocked:
	case ECombatAbilityAimStatus::Unavailable: Color = FLinearColor(0.42f, 0.46f, 0.50f); break;
	default: break;
	}
	const bool bDashed = Preview.Status == ECombatAbilityAimStatus::OutOfRange;
	if (Preview.CastRadius > 0.0f) ShowLayer(0, Preview.CasterLocation, Preview.Direction, Preview.CastRadius, 0, 0,
		Color * 0.48f, 2.5f, 0.0f, bDashed);
	if (Preview.bHasTarget)
	{
		const FCombatAbilityIndicatorGeometry& Geometry = Preview.Geometry;
		if (Geometry.Shape == ECombatIndicatorShape::Circle && Geometry.Radius > 0.0f)
		{
			ShowLayer(1, Geometry.bCenterOnCaster ? Preview.CasterLocation : Preview.TargetLocation, Preview.Direction,
				Geometry.Radius, 0, 0, Color, 3.0f, 0.055f, bDashed);
		}
		else if (Geometry.Shape == ECombatIndicatorShape::Line)
		{
			ShowLayer(1, Preview.CasterLocation + Preview.Direction * Preview.PlanarLineLength * 0.5f, Preview.Direction,
				Geometry.Radius, Preview.PlanarLineLength, 1, Color, 3.0f, 0.055f, bDashed);
		}
		if (Preview.bAiming) ShowLayer(2, Preview.TargetLocation, Preview.Direction,
			FMath::Max(18.0f, Preview.TargetRadius + 5.0f), 0, 2, Color, 3.0f, 0, false);
	}
}
