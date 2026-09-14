#include "Combat/Items/CombatItemSubsystem.h"
#include "Combat/Items/CombatItemData.h"
#include "Combat/Items/CombatWorldItem.h"
#include "Combat/Items/CombatInventoryComponent.h"
#include "Combat/Unit/CombatUnitCharacter.h"
#include "Engine/World.h"

float UCombatItemInstance::GetCooldownRemaining(double Now) const
{
	return FMath::Max(0.0, CooldownRemaining - FMath::Max(0.0, Now - CooldownCheckpoint) * CooldownRate);
}

FCombatSourceContext UCombatItemInstance::MakeSource() const
{
	FCombatSourceContext Source;
	Source.DirectSourceType = ECombatDirectSourceType::Item;
	Source.ItemDefinitionId = Definition ? Definition->GetPrimaryAssetId() : FPrimaryAssetId();
	Source.ItemHandle = Handle;
	return Source;
}

bool UCombatItemSubsystem::DoesSupportWorldType(EWorldType::Type Type) const
{
	return Type == EWorldType::Game || Type == EWorldType::PIE || Type == EWorldType::GamePreview;
}

void UCombatItemSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	// 多个 PIE/测试 World 共存时也不给旧 World 的请求机会匹配新 World 的第一件物品。
	static uint32 NextWorldGeneration = 1;
	Generation = NextWorldGeneration++;
	if (Generation == 0) Generation = NextWorldGeneration++;
}

UCombatItemInstance* UCombatItemSubsystem::FindMutable(FCombatItemHandle Handle) const
{
	const TObjectPtr<UCombatItemInstance>* Found = Instances.Find(Handle.Key.Id);
	return !bShuttingDown && Handle.IsValid() && Found && *Found && (*Found)->Handle == Handle ? Found->Get() : nullptr;
}

const UCombatItemInstance* UCombatItemSubsystem::FindItem(FCombatItemHandle Handle) const { return FindMutable(Handle); }

UCombatItemInstance* UCombatItemSubsystem::CreateItem(UCombatItemData* Definition, int32 Quantity)
{
	FString Error;
	if (bShuttingDown || !GetWorld() || GetWorld()->GetNetMode() == NM_Client || !Definition
		|| !Definition->ValidateRuntime(Error) || Quantity < 1 || Quantity > Definition->MaxStack || NextId == MAX_uint64) return nullptr;
	UCombatItemInstance* Item = NewObject<UCombatItemInstance>(this);
	Item->Handle.Key.Id = NextId++;
	Item->Handle.Key.Generation = Generation;
	Item->Definition = Definition;
	Item->Quantity = Quantity;
	Item->Charges = Definition->InitialCharges;
	Item->CooldownCheckpoint = GetWorld()->GetTimeSeconds();
	Instances.Add(Item->Handle.Key.Id, Item);
	return Item;
}

ACombatWorldItem* UCombatItemSubsystem::CreateWorldActor(UCombatItemInstance& Item, const FVector& Location)
{
	if (bShuttingDown || GetWorld()->GetNetMode() == NM_Client || Location.ContainsNaN()) return nullptr;
	const FTransform Transform(FRotator::ZeroRotator, Location);
	ACombatWorldItem* Actor = GetWorld()->SpawnActorDeferred<ACombatWorldItem>(ACombatWorldItem::StaticClass(), Transform,
		nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Actor) return nullptr;
	Actor->InitializeProjection(Item.Handle, Item.Revision, Item.Definition, Item.Quantity);
	Actor->FinishSpawning(Transform);
	return IsValid(Actor) && !Actor->IsActorBeingDestroyed() ? Actor : nullptr;
}

FCombatItemHandle UCombatItemSubsystem::SpawnItem(UCombatItemData* Definition, int32 Quantity, const FVector& Location)
{
	UCombatItemInstance* Item = CreateItem(Definition, Quantity);
	if (!Item) return {};
	Item->WorldActor = CreateWorldActor(*Item, Location);
	if (!Item->WorldActor.IsValid()) { DestroyItem(Item->Handle); return {}; }
	return Item->Handle;
}

bool UCombatItemSubsystem::RegisterPlacedItem(ACombatWorldItem& Actor, UCombatItemData* Definition, int32 Quantity)
{
	if (!Actor.HasAuthority() || Actor.GetWorld() != GetWorld() || Actor.GetItemHandle().IsValid()) return false;
	UCombatItemInstance* Item = CreateItem(Definition, Quantity);
	if (!Item) return false;
	Item->WorldActor = &Actor;
	Actor.InitializeProjection(Item->Handle, Item->Revision, Definition, Quantity);
	return true;
}

void UCombatItemSubsystem::DestroyItem(FCombatItemHandle Handle)
{
	if (FindMutable(Handle)) Instances.Remove(Handle.Key.Id);
}

void UCombatItemSubsystem::NotifyWorldActorEndPlay(ACombatWorldItem& Actor)
{
	UCombatItemInstance* Item = FindMutable(Actor.GetItemHandle());
	if (Item && Item->WorldActor.Get() == &Actor && !Item->Holder.IsValid()) DestroyItem(Item->Handle);
}

void UCombatItemSubsystem::Deinitialize()
{
	// Actor EndPlay 通常已完成；显式清理仍在世界登记表内的持有者，支持自动化主动 teardown。
	TSet<TWeakObjectPtr<ACombatUnitCharacter>> Holders;
	for (const auto& Pair : Instances) if (Pair.Value->Holder.IsValid()) Holders.Add(Pair.Value->Holder);
	for (const auto& Holder : Holders) if (Holder.IsValid()) Holder->GetCombatInventoryComponent()->ClearInventory();
	bShuttingDown = true;
	TArray<TObjectPtr<UCombatItemInstance>> Remaining;
	Instances.GenerateValueArray(Remaining);
	Instances.Reset();
	for (UCombatItemInstance* Item : Remaining) if (Item->WorldActor.IsValid()) Item->WorldActor->Destroy();
	++Generation;
	Super::Deinitialize();
}
