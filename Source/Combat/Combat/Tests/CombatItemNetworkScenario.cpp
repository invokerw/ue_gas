#include "Combat/Tests/CombatItemNetworkScenario.h"
#include "Combat/Ability/CombatAbilitySystemComponent.h"
#include "Combat/Items/CombatWorldItem.h"
#include "Combat/Items/CombatInventoryComponent.h"
#include "Combat/Items/CombatItemSubsystem.h"
#include "Combat/Items/CombatItemData.h"
#include "Combat/Core/CombatTags.h"
#include "Combat/Log/CombatEventSubsystem.h"
#include "Combat/Unit/CombatUnitCharacter.h"
#include "Combat/View/CombatUnitViewComponent.h"
#include "CombatPlayerController.h"
#include "Engine/World.h"
#include "EngineUtils.h"

ACombatItemNetworkScenario::ACombatItemNetworkScenario()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.1f;
}
void ACombatItemNetworkScenario::Submit(ACombatPlayerController& Player, FCombatOrderRequest Request)
{
	ACombatUnitCharacter* Unit = Player.GetCommandedUnit();
	FCombatOrderBatchRequest Batch;
	Batch.RequestId = RequestId++;
	Batch.UnitLifeGeneration = Unit->GetLifeGeneration();
	Batch.CommandBindingGeneration = Player.GetCommandBindingGeneration();
	Batch.Orders.Add(Request);
	Unit->ServerIssueOrderBatch(Batch);
}
void ACombatItemNetworkScenario::ReceiveFinal(FCombatOrderResult Result)
{
	if (Result.ItemHandle == ContestedItem && Result.RequestId == ContestedRequestId)
	{
		bContestResolved = true;
		bContestWon = Result.bSuccess;
		return;
	}
	if (Result.ItemHandle != Item || Result.RequestId < 800001) return;
	++FinalCount;
	if (!Result.bSuccess) Finish(false, *Result.FailureTag.ToString());
}
void ACombatItemNetworkScenario::TickServer()
{
	TArray<ACombatPlayerController*> Players;
	for (TActorIterator<ACombatPlayerController> It(GetWorld()); It; ++It)
		if (It->GetCommandedUnit()) Players.Add(*It);
	if (Players.Num() != 2) return;
	Players.Sort([](const ACombatPlayerController& A, const ACombatPlayerController& B) { return A.GetUniqueID() < B.GetUniqueID(); });
	ACombatUnitCharacter* First = Players[0]->GetCommandedUnit();
	ACombatUnitCharacter* Second = Players[1]->GetCommandedUnit();
	if (Step == 0)
	{
		for (ACombatUnitCharacter* Unit : {First, Second})
		{
			const auto View = Unit->GetCombatUnitViewComponent()->GetHUDOwnerView();
			if (!View.Items.IsValidIndex(2) || View.Items[2].Quantity != 2 || View.Items[2].EnabledAt <= 0
				|| View.Items[2].DefinitionId.PrimaryAssetName != TEXT("healing_potion")) return;
		}
		// 给两端足够时间收到上一阶段回执；竞争目标由两端先保存同一版本再延时请求。
		if (Elapsed < 27) return;
		auto* Definition = LoadObject<UCombatItemData>(nullptr, TEXT("/Game/Combat/Demo/Items/DA_lightning_wand.DA_lightning_wand"));
		const FVector Location = (First->GetNavAgentLocation() + Second->GetNavAgentLocation()) * 0.5 + FVector(0, -180, 20);
		ContestedItem = GetWorld()->GetSubsystem<UCombatItemSubsystem>()->SpawnItem(Definition, 1, Location);
		if (!ContestedItem.IsValid()) { Finish(false, TEXT("Contention fixture could not spawn")); return; }
		PhaseStarted = Elapsed;
		Step = 1;
	}
	else if (Step == 1)
	{
		const auto* Instance = GetWorld()->GetSubsystem<UCombatItemSubsystem>()->FindItem(ContestedItem);
		if (!Instance || !Instance->GetHolder() || Elapsed - PhaseStarted < 6) return;
		int32 Copies = 0;
		for (ACombatUnitCharacter* Unit : {First, Second})
			for (int32 Slot = 0; Slot < CombatItems::TotalSlots; ++Slot)
				Copies += Unit->GetCombatInventoryComponent()->GetItemAt(Slot) == ContestedItem ? 1 : 0;
		if (Copies != 1 || Instance->GetWorldActor()) { Finish(false, TEXT("Contention duplicated a world instance")); return; }
		const auto FirstPotion = First->GetCombatInventoryComponent()->GetItemAt(2);
		const auto SecondPotion = Second->GetCombatInventoryComponent()->GetItemAt(2);
		const bool bTransferred = Players[0]->SetCommandedUnitAuthority(Second) && Players[1]->SetCommandedUnitAuthority(First);
		const bool bRetained = First->GetCombatInventoryComponent()->GetItemAt(2) == FirstPotion
			&& Second->GetCombatInventoryComponent()->GetItemAt(2) == SecondPotion;
		Finish(bTransferred && bRetained, TEXT("Both item loops, one contention winner, and control transfer retaining unit inventories checked"));
	}
}
void ACombatItemNetworkScenario::Finish(bool bSuccess, const TCHAR* Detail)
{
	if (bFinished) return;
	bFinished = true;
	UE_LOG(LogCombat, Display, TEXT("ItemNetworkSmoke Role=%s Step=%d FinalCount=%d Result=%s Detail=%s"),
		GetNetMode() == NM_DedicatedServer ? TEXT("Server") : TEXT("Client"), Step, FinalCount, bSuccess ? TEXT("Pass") : TEXT("Fail"), Detail);
}
void ACombatItemNetworkScenario::Tick(float Delta)
{
	Super::Tick(Delta);
	if (bFinished) return;
	Elapsed += Delta;
	if (Elapsed > 70) { Finish(false, TEXT("Timeout")); return; }
	if (Elapsed < 20) return;
	if (GetNetMode() == NM_DedicatedServer)
	{
		TickServer();
		return;
	}
	ACombatPlayerController* Player = Cast<ACombatPlayerController>(GetWorld()->GetFirstPlayerController());
	ACombatUnitCharacter* Unit = Player ? Player->GetCommandedUnit() : nullptr;
	if (!Unit || Unit->GetCommandingPlayerController() != Player) return;
	const auto View = Unit->GetCombatUnitViewComponent()->GetHUDOwnerView();
	if (View.Items.Num() != CombatItems::TotalSlots) return;
	if (Step == 0)
	{
		if (View.Items[2].Quantity != 3 || !View.Items[2].AbilityHandle.IsValid()) return;
		BoundUnit = Unit;
		InitialBindingGeneration = Player->GetCommandBindingGeneration();
		Item = View.Items[2].Handle;
		Unit->OnOrderFinalResult.AddUniqueDynamic(this, &ACombatItemNetworkScenario::ReceiveFinal);
		if (!Player->UseInventoryItem(2, View.Items[2])) { Finish(false, TEXT("Use rejected locally")); return; }
		Step = 1;
	}
	else if (Step == 1)
	{
		if (View.Items[2].Quantity != 2 || View.Items[2].CooldownRemaining <= 0) return;
		const auto* Spec = Unit->GetCombatAbilitySystemComponent()->FindAbilitySpecFromHandle(View.Items[2].AbilityHandle);
		if (Spec && Spec->IsActive()) return;
		Player->SwapInventoryItems(2, 6, View);
		Step = 2;
	}
	else if (Step == 2)
	{
		if (View.Items[6].Handle != Item) return;
		FCombatOrderRequest Drop;
		Drop.Type = ECombatOrderType::DropItem;
		Drop.ItemHandle = Item;
		Drop.ItemRevision = View.Items[6].Revision;
		Drop.bHasTargetLocation = true;
		// 容量场景的两位英雄靠得很近；脚下放置避免把固定偏移点放进另一单位的阻挡胶囊。
		Drop.TargetLocation = Unit->GetNavAgentLocation();
		Submit(*Player, Drop);
		Step = 3;
	}
	else if (Step == 3)
	{
		if (View.Items.ContainsByPredicate([this](const auto& Entry) { return Entry.Handle == Item; })) return;
		for (TActorIterator<ACombatWorldItem> It(GetWorld()); It; ++It)
		{
			if (It->GetItemHandle() != Item) continue;
			FCombatOrderRequest Pickup;
			Pickup.Type = ECombatOrderType::PickupItem;
			Pickup.ItemHandle = Item;
			Pickup.ItemRevision = It->GetItemRevision();
			Submit(*Player, Pickup);
			Step = 4;
			break;
		}
	}
	else if (Step == 4)
	{
		if (View.Items[2].Handle != Item || FinalCount < 2) return;
		bool bForeignHidden = true;
		for (TActorIterator<ACombatUnitCharacter> It(GetWorld()); It; ++It)
			if (*It != Unit && GetNetMode() == NM_Client) bForeignHidden &= It->GetCombatUnitViewComponent()->GetHUDOwnerView().Items.IsEmpty();
		const bool bLoopPassed = View.Items[2].Quantity == 2 && View.Items[2].EnabledAt > Unit->GetCombatUnitViewComponent()->GetEstimatedServerTimeSeconds()
			&& View.Items[2].GetRemaining(Unit->GetCombatUnitViewComponent()->GetEstimatedServerTimeSeconds()) > 0
			&& View.Items[2].AbilityHandle.IsValid() && bForeignHidden && FinalCount == 2;
		if (!bLoopPassed) { Finish(false, TEXT("Item round trip projection mismatch")); return; }
		Step = 5;
	}
	else if (Step == 5)
	{
		for (TActorIterator<ACombatWorldItem> It(GetWorld()); It; ++It)
		{
			if (It->GetItemDefinitionId().PrimaryAssetName != TEXT("lightning_wand")) continue;
			ContestedItem = It->GetItemHandle();
			ContestedRevision = It->GetItemRevision();
			if (!ContestedItem.IsValid() || ContestedRevision <= 0) continue;
			PhaseStarted = Elapsed;
			Step = 6;
			break;
		}
	}
	else if (Step == 6 && Elapsed - PhaseStarted >= 2)
	{
		FCombatOrderRequest Pickup;
		Pickup.Type = ECombatOrderType::PickupItem;
		Pickup.ItemHandle = ContestedItem;
		Pickup.ItemRevision = ContestedRevision;
		ContestedRequestId = RequestId;
		Submit(*Player, Pickup);
		Step = 7;
	}
	else if (Step == 7)
	{
		const auto& Batch = Unit->GetLastOrderBatchResult();
		if (Batch.RequestId == ContestedRequestId && Batch.bAccepted && Batch.AcceptedOrderCount == 0)
		{
			bContestResolved = true;
			bContestWon = false;
		}
		if (!bContestResolved) return;
		const bool bHasItem = View.Items.ContainsByPredicate([this](const auto& Entry) { return Entry.Handle == ContestedItem; });
		if (bContestWon && !bHasItem) return;
		if (!bContestWon && bHasItem) { Finish(false, TEXT("Losing contender received the instance")); return; }
		UE_LOG(LogCombat, Display, TEXT("ItemNetworkContention Role=Client Outcome=%s Result=Pass"), bContestWon ? TEXT("Won") : TEXT("Lost"));
		Step = 8;
	}
	else if (Step == 8)
	{
		if (Unit == BoundUnit.Get() || Player->GetCommandBindingGeneration() == InitialBindingGeneration) return;
		if (!View.Items[2].Handle.IsValid() || View.Items[2].Handle == Item || !View.Items[2].AbilityHandle.IsValid()) return;
		if (BoundUnit.IsValid() && !BoundUnit->GetCombatUnitViewComponent()->GetHUDOwnerView().Items.IsEmpty()) return;
		FCombatOrderRequest Swap;
		Swap.Type = ECombatOrderType::SwapItems;
		Swap.ItemHandle = View.Items[2].Handle;
		Swap.ItemRevision = View.Items[2].Revision;
		Swap.FromItemSlot = 2;
		Swap.ToItemSlot = 6;
		Swap.OtherItemHandle = View.Items[6].Handle;
		Swap.InventoryRevision = View.InventoryRevision;
		FCombatOrderBatchRequest Stale;
		StaleRequestId = Stale.RequestId = RequestId++;
		Stale.UnitLifeGeneration = Unit->GetLifeGeneration();
		Stale.CommandBindingGeneration = InitialBindingGeneration;
		Stale.Orders.Add(Swap);
		Unit->ServerIssueOrderBatch(Stale);
		Step = 9;
	}
	else if (Step == 9)
	{
		const auto& Batch = Unit->GetLastOrderBatchResult();
		if (Batch.RequestId != StaleRequestId) return;
		const bool bStaleRejected = Batch.bAccepted && Batch.AcceptedOrderCount == 0 && Batch.OrderResults.Num() == 1
			&& Batch.OrderResults[0].FailureTag == CombatTags::Failure_Item_Stale;
		Finish(bStaleRejected && View.Items[2].Quantity == 2 && !View.Items[6].Handle.IsValid(),
			TEXT("Round trip, contention receipt, new owner inventory, old owner privacy and stale binding RPC rejection checked"));
	}
}
void ACombatItemNetworkScenario::EndPlay(const EEndPlayReason::Type Reason)
{
	if (BoundUnit.IsValid()) BoundUnit->OnOrderFinalResult.RemoveDynamic(this, &ACombatItemNetworkScenario::ReceiveFinal);
	Super::EndPlay(Reason);
}
