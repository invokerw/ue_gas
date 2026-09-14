#include "Combat/Items/CombatWorldItem.h"
#include "Combat/Items/CombatItemData.h"
#include "Combat/Items/CombatItemSubsystem.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "Combat/UI/CombatWorldItemLabelWidget.h"
#include "Engine/AssetManager.h"
#include "Engine/StaticMesh.h"
#include "Engine/StreamableManager.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

ACombatWorldItem::ACombatWorldItem()
{
	bReplicates = true;
	SetReplicateMovement(true);
	PickupShape = CreateDefaultSubobject<USphereComponent>(TEXT("PickupShape"));
	SetRootComponent(PickupShape);
	PickupShape->SetSphereRadius(42.0f);
	PickupShape->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	PickupShape->SetCollisionResponseToAllChannels(ECR_Ignore);
	PickupShape->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	PickupShape->SetCanEverAffectNavigation(false);
	ItemMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ItemMesh"));
	ItemMesh->SetupAttachment(PickupShape);
	ItemMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ItemMesh->SetCanEverAffectNavigation(false);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Mesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	ItemMesh->SetStaticMesh(Mesh.Object);
	ItemMesh->SetRelativeScale3D(FVector(0.42f, 0.42f, 0.24f));
	ItemMesh->SetRelativeRotation(FRotator(0.0f, 45.0f, 0.0f));
	ItemMesh->SetRenderCustomDepth(true);
	ItemMesh->SetCustomDepthStencilValue(1);
	GroundLabel = CreateDefaultSubobject<UWidgetComponent>(TEXT("GroundLabel"));
	GroundLabel->SetupAttachment(PickupShape);
	GroundLabel->SetRelativeLocation(FVector(0.0f, 0.0f, 46.0f));
	GroundLabel->SetWidgetClass(UCombatWorldItemLabelWidget::StaticClass());
	GroundLabel->SetWidgetSpace(EWidgetSpace::Screen);
	GroundLabel->SetDrawAtDesiredSize(true);
	GroundLabel->SetCanEverAffectNavigation(false);
	GroundLabel->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void ACombatWorldItem::BeginPlay()
{
	Super::BeginPlay();
	if (HasAuthority() && !ItemHandle.IsValid())
	{
		UCombatItemSubsystem* Items = GetWorld()->GetSubsystem<UCombatItemSubsystem>();
		if (!Items || !Items->RegisterPlacedItem(*this, ItemDefinition, InitialQuantity)) { Destroy(); return; }
	}
	RefreshPresentation();
}

void ACombatWorldItem::EndPlay(const EEndPlayReason::Type Reason)
{
	bEnding = true;
	if (DefinitionLoad) { DefinitionLoad->CancelHandle(); DefinitionLoad.Reset(); }
	if (MeshLoad) { MeshLoad->CancelHandle(); MeshLoad.Reset(); }
	if (HasAuthority())
	{
		if (UCombatItemSubsystem* Items = GetWorld()->GetSubsystem<UCombatItemSubsystem>()) Items->NotifyWorldActorEndPlay(*this);
	}
	Super::EndPlay(Reason);
}

void ACombatWorldItem::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	RefreshPresentation();
}

void ACombatWorldItem::InitializeProjection(FCombatItemHandle Handle, int32 Revision, UCombatItemData* Definition, int32 InQuantity)
{
	if (!HasAuthority() || !Definition) return;
	ItemHandle = Handle;
	ItemRevision = Revision;
	ItemDefinition = Definition;
	DefinitionId = Definition->GetPrimaryAssetId();
	Quantity = InQuantity;
	RefreshPresentation();
	ForceNetUpdate();
}

void ACombatWorldItem::RefreshPresentation()
{
	if (bEnding || GetNetMode() == NM_DedicatedServer) return;
	UCombatItemData* Data = ItemDefinition;
	if (!Data && DefinitionId.IsValid()) Data = Cast<UCombatItemData>(UAssetManager::Get().GetPrimaryAssetObject(DefinitionId));
	if (Data)
	{
		const FText Name = Data->DisplayNameText.IsEmpty() ? FText::FromName(Data->DefinitionName) : Data->DisplayNameText;
		if (!GroundLabel || !ItemMesh) return;
		GroundLabel->InitWidget();
		if (UCombatWorldItemLabelWidget* Label = Cast<UCombatWorldItemLabelWidget>(GroundLabel->GetUserWidgetObject()))
			Label->ShowItem(Name, ItemHandle.IsValid() ? Quantity : InitialQuantity, Data->Tint);
		if (UStaticMesh* Mesh = Data->WorldMesh.Get()) ItemMesh->SetStaticMesh(Mesh);
		else if (!Data->WorldMesh.IsNull() && !MeshLoad)
		{
			const FCombatItemHandle Expected = ItemHandle;
			MeshLoad = UAssetManager::GetStreamableManager().RequestAsyncLoad(Data->WorldMesh.ToSoftObjectPath(),
				FStreamableDelegate::CreateWeakLambda(this, [this, Expected]()
				{
					if (!bEnding && ItemHandle == Expected) RefreshPresentation();
				}));
		}
	}
	else
	{
		const FSoftObjectPath Path = UAssetManager::Get().GetPrimaryAssetPath(DefinitionId);
		if (Path.IsValid() && !DefinitionLoad)
		{
			const FCombatItemHandle Expected = ItemHandle;
			DefinitionLoad = UAssetManager::GetStreamableManager().RequestAsyncLoad(Path,
				FStreamableDelegate::CreateWeakLambda(this, [this, Expected]()
				{
					if (!bEnding && ItemHandle == Expected) RefreshPresentation();
				}));
		}
	}
}

void ACombatWorldItem::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ACombatWorldItem, ItemHandle);
	DOREPLIFETIME(ACombatWorldItem, ItemRevision);
	DOREPLIFETIME(ACombatWorldItem, DefinitionId);
	DOREPLIFETIME(ACombatWorldItem, Quantity);
}
