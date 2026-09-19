#include "Combat/Tests/CombatEconomyNetworkScenario.h"

#include "Combat/Items/CombatItemTypes.h"
#include "Combat/Core/CombatTags.h"
#include "Combat/Unit/CombatUnitCharacter.h"
#include "Combat/View/CombatUnitViewComponent.h"
#include "CombatPlayerController.h"
#include "Combat/Economy/CombatEconomyComponent.h"
#include "Combat/Log/CombatEventSubsystem.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/Parse.h"

namespace CombatEconomySmoke
{
	static const FPrimaryAssetId WandId(FPrimaryAssetType(TEXT("CombatItem")), FName(TEXT("lightning_wand")));
	static const FPrimaryAssetId BootsId(FPrimaryAssetType(TEXT("CombatItem")), FName(TEXT("travel_boots")));
}

ACombatEconomyNetworkScenario::ACombatEconomyNetworkScenario()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.10f;
	// Dedicated 中必须在 SpawnActor 注册 NetDriver 前就标记复制，否则首个 PostLogin
	// 创建的场景可能只存在于服务器，客户端永远不会运行经济验证器。
	bReplicates = true;
	bAlwaysRelevant = true;
	bSoak = FParse::Param(FCommandLine::Get(), TEXT("CombatEconomySoak"));
}

void ACombatEconomyNetworkScenario::ReceiveEconomyResult(const FCombatEconomyResult Result)
{
	if (bFinished || AwaitingAction == 0 || Result.RequestId <= 0) return;
	++ResultCount;
	if (!Result.bSuccess)
	{
		if (Result.FailureTag == CombatTags::Failure_Economy_Stale && Result.GoldDelta == 0)
		{
			// 被动金币或另一份复制快照可能在请求提交前推进修订；重新从当前状态
			// 编排同一个动作，避免把这种正常竞争误判为经济事务失败。
			++StaleRetryCount;
			++TotalStaleRetries;
			if (StaleRetryCount > 8)
			{
				Fail(TEXT("Too many stale economy retries"));
				return;
			}
			switch (Step)
			{
			case 1: Step = 0; break;
			case 3: Step = 2; break;
			case 5: Step = 4; break;
			case 7: Step = 6; break;
			case 10: Step = 0; break;
			case 12: Step = 11; break;
			case 14: Step = 13; break;
			case 16: Step = 15; break;
			default: Fail(TEXT("Stale economy result arrived in an invalid step")); return;
			}
			AwaitingAction = 0;
			bHasResult = false;
			LastResult = Result;
			UE_LOG(LogCombat, Display, TEXT("EconomyNetworkRetry Reason=Stale Retry=%d Economy=%d Inventory=%d"),
				StaleRetryCount, Result.EconomyRevision, Result.InventoryRevision);
			return;
		}
		Fail(*FString::Printf(TEXT("Action=%d Failure=%s"), AwaitingAction, *Result.FailureTag.ToString()));
		return;
	}
	LastResult = Result;
	CycleGoldDelta += Result.GoldDelta;
	if (AwaitingAction == 1 && Result.GoldDelta < 0) bSawPurchase = true;
	if (AwaitingAction == 3 && Result.GoldDelta > 0) bSawSale = true;
	bHasResult = true;
}

const FCombatItemView* ACombatEconomyNetworkScenario::FindItem(const FCombatItemHandle& Handle) const
{
	if (!Controller.IsValid() || !Handle.IsValid()) return nullptr;
	const ACombatUnitCharacter* Unit = Controller->GetCommandedUnit();
	const UCombatUnitViewComponent* ViewComponent = Unit ? Unit->GetCombatUnitViewComponent() : nullptr;
	if (!ViewComponent) return nullptr;
	return OwnerSnapshot.Items.FindByPredicate(
		[&Handle](const FCombatItemView& Item) { return Item.Handle == Handle; });
}

const FCombatItemView* ACombatEconomyNetworkScenario::FindDefinition(
	const FName DefinitionName, const bool bExcludeInitialBlades) const
{
	if (!Controller.IsValid()) return nullptr;
	const ACombatUnitCharacter* Unit = Controller->GetCommandedUnit();
	const UCombatUnitViewComponent* ViewComponent = Unit ? Unit->GetCombatUnitViewComponent() : nullptr;
	if (!ViewComponent) return nullptr;
	return OwnerSnapshot.Items.FindByPredicate(
		[&DefinitionName, bExcludeInitialBlades, this](const FCombatItemView& Item)
		{
			return Item.Handle.IsValid() && Item.DefinitionId.PrimaryAssetName == DefinitionName
				&& (!bExcludeInitialBlades || !InitialBlades.Contains(Item.Handle));
		});
}

bool ACombatEconomyNetworkScenario::IsReady() const
{
	if (!Controller.IsValid() || !Controller->IsLocalController()) return false;
	const ACombatUnitCharacter* Unit = Controller->GetCommandedUnit();
	const UCombatEconomyComponent* Economy = Controller->GetCombatEconomyComponent();
	if (!Unit || !Unit->GetCombatUnitViewComponent() || !Economy || !Economy->IsInitialized()) return false;
	const FCombatEconomyView& EconomyView = Economy->GetEconomyView();
	return EconomyView.ShopDefinitionId.IsValid() && EconomyView.InventoryItems.Num() == CombatItems::TotalSlots;
}

static bool EconomyProjectionCaughtUp(const ACombatPlayerController* Controller, const FCombatEconomyResult& Result)
{
	return Controller && Controller->GetCombatEconomyComponent()
		&& Controller->GetCombatEconomyComponent()->GetEconomyView().EconomyRevision >= Result.EconomyRevision
		&& Controller->GetCombatEconomyComponent()->GetEconomyView().InventoryRevision >= Result.InventoryRevision;
}

void ACombatEconomyNetworkScenario::Fail(const TCHAR* Detail)
{
	if (bFinished) return;
	bFinished = true;
	++FailureCount;
	UE_LOG(LogCombat, Error, TEXT("EconomyNetworkSmoke Role=Client Result=Fail Cycles=%d Results=%d Detail=%s"),
		CompletedCycles, ResultCount, Detail);
	SaveReport();
}

void ACombatEconomyNetworkScenario::Finish()
{
	if (bFinished) return;
	bFinished = true;
	UE_LOG(LogCombat, Display, TEXT("EconomyNetworkSmoke Role=Client Result=Pass Cycles=%d Results=%d Seconds=%.1f Detail=%s"),
		CompletedCycles, ResultCount, Elapsed, bSoak ? TEXT("300 second economy soak completed") : TEXT("Purchase lock unlock-craft sell completed"));
	SaveReport();
}

void ACombatEconomyNetworkScenario::SaveReport() const
{
	const FString Directory = FPaths::ProjectSavedDir() / TEXT("ECON003Validation");
	IFileManager::Get().MakeDirectory(*Directory, true);
	const FString Report = FString::Printf(
		TEXT("{\"passed\":%s,\"cycles\":%d,\"results\":%d,\"failures\":%d,\"staleRetries\":%d,\"soak\":%s}\n"),
		bFinished && FailureCount == 0 && CompletedCycles > 0 ? TEXT("true") : TEXT("false"),
		CompletedCycles, ResultCount, FailureCount, TotalStaleRetries, bSoak ? TEXT("true") : TEXT("false"));
	FString ReportName = TEXT("EconomyNetworkSmoke");
	FParse::Value(FCommandLine::Get(), TEXT("CombatEconomyReport="), ReportName);
	FFileHelper::SaveStringToFile(Report, *(Directory / (FPaths::GetCleanFilename(ReportName) + TEXT(".json"))));
}

void ACombatEconomyNetworkScenario::SendPurchase(const FPrimaryAssetId& DefinitionId)
{
	AwaitingAction = 1;
	if (!Controller.IsValid() || !Controller->PurchaseShopItem(DefinitionId))
	{
		AwaitingAction = 0;
		Fail(TEXT("Purchase request was not accepted locally"));
		return;
	}
}

void ACombatEconomyNetworkScenario::SendToggle(const FCombatItemHandle& Handle)
{
	const FCombatItemView* Item = FindItem(Handle);
	AwaitingAction = 2;
	const bool bSent = Controller.IsValid() && Item && Controller->ToggleInventoryItemLock(*Item);
	if (!bSent)
	{
		AwaitingAction = 0;
		Fail(TEXT("Toggle request was not accepted locally"));
		return;
	}
}

void ACombatEconomyNetworkScenario::SendSell(const FCombatItemView& Item)
{
	AwaitingAction = 3;
	if (!Controller.IsValid() || !Controller->SellInventoryItem(Item))
	{
		AwaitingAction = 0;
		Fail(TEXT("Sell request was not accepted locally"));
		return;
	}
}

void ACombatEconomyNetworkScenario::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (bFinished || GetNetMode() == NM_DedicatedServer) return;
	Elapsed += DeltaSeconds;
	// 联机地图先完成既有 HUD 场景的主控单位接线，再采样经济循环的初始库存。
	if (GetNetMode() == NM_Client && Elapsed < 22.0f) return;
	if (Elapsed > (bSoak ? 330.0f : 90.0f))
	{
		Fail(TEXT("Timeout"));
		return;
	}
	if (!Controller.IsValid()) Controller = Cast<ACombatPlayerController>(GetWorld()->GetFirstPlayerController());
	if (!IsReady()) return;
	// HUD 访问器按值返回；持有本帧快照，避免向 RPC 传递临时数组中的悬空指针。
	OwnerSnapshot = Controller->GetCommandedUnit()->GetCombatUnitViewComponent()->GetHUDOwnerView();
	if (!bBoundResultDelegate)
	{
		Controller->OnEconomyResult.AddUniqueDynamic(this, &ACombatEconomyNetworkScenario::ReceiveEconomyResult);
		bBoundResultDelegate = true;
	}

	if (Step == 0)
	{
		if (!EconomyProjectionCaughtUp(Controller.Get(), LastResult)) return;
		if (!bCapturedInitialBlades)
		{
			for (const FCombatItemView& Item : OwnerSnapshot.Items)
				if (Item.DefinitionId.PrimaryAssetName == FName(TEXT("thunder_blade"))) InitialBlades.Add(Item.Handle);
			bCapturedInitialBlades = true;
		}
		const FCombatItemView* Candidate = OwnerSnapshot.Items.FindByPredicate(
			[](const FCombatItemView& Item)
			{
				return Item.Handle.IsValid()
					&& (Item.DefinitionId.PrimaryAssetName == FName(TEXT("lightning_wand"))
						|| Item.DefinitionId.PrimaryAssetName == FName(TEXT("travel_boots")))
					&& !Item.bLocked;
			});
		CycleStarted = Elapsed;
		CycleGoldDelta = 0;
		bSawPurchase = false;
		bSawSale = false;
		LockedComponent = Candidate ? Candidate->Handle : FCombatItemHandle();
		PurchasedComponent = FCombatItemHandle();
		SecondaryPurchasedComponent = FCombatItemHandle();
		CraftedBlade = FCombatItemHandle();
		if (Candidate)
		{
			PurchaseDefinitionId = Candidate->DefinitionId.PrimaryAssetName == FName(TEXT("lightning_wand"))
				? CombatEconomySmoke::BootsId : CombatEconomySmoke::WandId;
			SendToggle(LockedComponent);
			Step = 1;
		}
		else
		{
			PurchaseDefinitionId = CombatEconomySmoke::WandId;
			SendPurchase(PurchaseDefinitionId);
			Step = 10;
		}
		return;
	}

	if (bHasResult)
	{
		bHasResult = false;
		switch (Step)
		{
		case 1: Step = 2; break;
		case 3:
			PurchasedComponent = LastResult.ItemHandle;
			if (!PurchasedComponent.IsValid()) { Fail(TEXT("Purchase result has no item handle")); return; }
			Step = 4;
			break;
		case 5: Step = 6; break;
		case 7: Step = 8; break;
		case 10:
			PurchasedComponent = LastResult.ItemHandle;
			if (!PurchasedComponent.IsValid()) { Fail(TEXT("First purchase result has no item handle")); return; }
			Step = 11;
			break;
		case 12: Step = 13; break;
		case 14:
			SecondaryPurchasedComponent = LastResult.ItemHandle;
			if (!SecondaryPurchasedComponent.IsValid()) { Fail(TEXT("Second purchase result has no item handle")); return; }
			Step = 15;
			break;
		case 16: Step = 6; break;
		default: break;
		}
	}

	switch (Step)
	{
	case 2:
		if (EconomyProjectionCaughtUp(Controller.Get(), LastResult))
		if (const FCombatItemView* Item = FindItem(LockedComponent); Item && Item->bLocked)
		{
			SendPurchase(PurchaseDefinitionId);
			Step = 3;
		}
		break;
	case 4:
		if (EconomyProjectionCaughtUp(Controller.Get(), LastResult))
		if (const FCombatItemView* Item = FindItem(LockedComponent); Item && Item->bLocked
			&& FindItem(PurchasedComponent) && !FindItem(PurchasedComponent)->bLocked)
		{
			SendToggle(LockedComponent);
			Step = 5;
		}
		break;
	case 6:
		if (EconomyProjectionCaughtUp(Controller.Get(), LastResult))
		if (const FCombatItemView* Blade = FindDefinition(FName(TEXT("thunder_blade")), true))
		{
			CraftedBlade = Blade->Handle;
			SendSell(*Blade);
			Step = 7;
		}
		break;
	case 8:
		if (EconomyProjectionCaughtUp(Controller.Get(), LastResult) && !FindItem(CraftedBlade))
		{
			if (!bSawPurchase || !bSawSale) { Fail(TEXT("Purchase and sale gold deltas were not observed")); return; }
			if (FindItem(LockedComponent) || FindItem(PurchasedComponent) || FindItem(SecondaryPurchasedComponent))
			{
				Fail(TEXT("Crafted components remain in inventory"));
				return;
			}
			++CompletedCycles;
			StaleRetryCount = 0;
			UE_LOG(LogCombat, Display, TEXT("EconomyNetworkCycle Result=Pass Cycle=%d Elapsed=%.1f GoldDelta=%lld"),
				CompletedCycles, Elapsed - CycleStarted, CycleGoldDelta);
			AwaitingAction = 0;
			LockedComponent = FCombatItemHandle();
			PurchasedComponent = FCombatItemHandle();
			SecondaryPurchasedComponent = FCombatItemHandle();
			CraftedBlade = FCombatItemHandle();
			PurchaseDefinitionId = FPrimaryAssetId();
			Step = 17;
			NextCycleAt = Elapsed + (bSoak ? 30.0f : 1.0f);
		}
		break;
	case 11:
		if (EconomyProjectionCaughtUp(Controller.Get(), LastResult)
			&& FindItem(PurchasedComponent) && !FindItem(PurchasedComponent)->bLocked)
		{
			SendToggle(PurchasedComponent);
			Step = 12;
		}
		break;
	case 13:
		if (EconomyProjectionCaughtUp(Controller.Get(), LastResult)
			&& FindItem(PurchasedComponent) && FindItem(PurchasedComponent)->bLocked)
		{
			SendPurchase(CombatEconomySmoke::BootsId);
			Step = 14;
		}
		break;
	case 15:
		if (EconomyProjectionCaughtUp(Controller.Get(), LastResult)
			&& FindItem(PurchasedComponent) && FindItem(PurchasedComponent)->bLocked
			&& FindItem(SecondaryPurchasedComponent) && !FindItem(SecondaryPurchasedComponent)->bLocked)
		{
			SendToggle(PurchasedComponent);
			Step = 16;
		}
		break;
	case 17:
		if (bSoak && Elapsed >= 300.0f) { Finish(); return; }
		if (Elapsed >= NextCycleAt)
		{
			if (!bSoak) { Finish(); return; }
			Step = 0;
		}
		break;
	default:
		break;
	}
}

void ACombatEconomyNetworkScenario::EndPlay(const EEndPlayReason::Type Reason)
{
	if (Controller.IsValid()) Controller->OnEconomyResult.RemoveDynamic(this, &ACombatEconomyNetworkScenario::ReceiveEconomyResult);
	Super::EndPlay(Reason);
}
