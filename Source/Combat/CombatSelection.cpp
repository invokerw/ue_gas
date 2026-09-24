#include "CombatPlayerController.h"
#include "Combat/Ability/CombatAbilitySystemComponent.h"
#include "Combat/Core/CombatTags.h"
#include "Combat/Economy/CombatEconomyComponent.h"
#include "Combat/Network/CombatNetworkSecuritySubsystem.h"
#include "Combat/Order/CombatOrderComponent.h"
#include "Combat/Targeting/CombatTargetingSubsystem.h"
#include "Combat/Unit/CombatUnitCharacter.h"
#include "Engine/World.h"
#include "EngineUtils.h"

namespace
{
	/** 查看与框选共用公共目标规则；此处只作本地预筛，无法授予控制权。 */
	bool CanInspect(const ACombatPlayerController& Player, ACombatUnitCharacter* Unit, const bool bAllowDead)
	{
		if (!IsValid(Unit) || Unit->GetWorld() != Player.GetWorld() || Unit->IsHidden()) return false;
		auto* Targeting = Player.GetWorld()->GetSubsystem<UCombatTargetingSubsystem>();
		FCombatTargetingRules Rules;
		Rules.TargetTeamTag = CombatTags::TargetTeam_Both;
		Rules.bAllowSelf = Rules.bAllowInvulnerable = Rules.bAllowNeutralRelation = true;
		Rules.bAllowDead = bAllowDead;
		Rules.CastRange = TNumericLimits<float>::Max() * 0.5f;
		return Targeting && Targeting->ValidateUnitTarget(IsValid(Player.GetCommandedUnit())
			? Player.GetCommandedUnit() : Unit, Unit, Rules).bValid;
	}
}

bool ACombatPlayerController::CanControlUnit(const ACombatUnitCharacter* Unit) const
{
	return IsValid(Unit) && Unit->GetWorld() == GetWorld() && Unit->GetCommandingPlayerController() == this;
}

ACombatUnitCharacter* ACombatPlayerController::GetInspectedUnit() const
{
	return bSelectionInitialized ? InspectedUnit.Get() : GetCommandedUnit();
}

TArray<ACombatUnitCharacter*> ACombatPlayerController::GetSelectedUnits() const
{
	TArray<ACombatUnitCharacter*> Result;
	if (!bSelectionInitialized)
	{
		if (CanControlUnit(CommandedUnit)) Result.Add(CommandedUnit);
		return Result;
	}
	for (const auto& Unit : SelectedUnits)
		if (CanControlUnit(Unit.Get())) Result.Add(Unit.Get());
	return Result;
}

bool ACombatPlayerController::CanOperateInspectedUnit() const
{
	const auto Units = GetSelectedUnits();
	return CanControlUnit(CommandedUnit) && GetInspectedUnit() == CommandedUnit
		&& !Units.IsEmpty() && Units[0] == CommandedUnit && PendingPrimaryRequestId == 0;
}

bool ACombatPlayerController::GrantUnitControlAuthority(ACombatUnitCharacter* Unit)
{
	if (!HasAuthority() || !IsValid(Unit) || Unit->GetWorld() != GetWorld()) return false;
	if (CanControlUnit(Unit)) return true;
	if (auto* Previous = Cast<ACombatPlayerController>(Unit->GetCommandingPlayerController()))
		Previous->RevokeUnitControlAuthority(Unit);
	else if (auto* Orders = Unit->GetCombatOrderComponent())
		Orders->StopAllOrders(CombatTags::Order_Failure_Cancelled);
	return Unit->SetCommandingPlayerController(this);
}

bool ACombatPlayerController::RevokeUnitControlAuthority(ACombatUnitCharacter* Unit)
{
	if (!HasAuthority() || !CanControlUnit(Unit)) return false;
	if (auto* Orders = Unit->GetCombatOrderComponent()) Orders->StopAllOrders(CombatTags::Order_Failure_Cancelled);
	Unit->SetCommandingPlayerController(nullptr);
	HandleCommandedUnitEndPlay(Unit);
	RefreshLocalSelection();
	return true;
}

bool ACombatPlayerController::SetPrimaryUnitAuthority(ACombatUnitCharacter* Unit)
{
	if (!HasAuthority() || !CanControlUnit(Unit)) return false;
	if (CommandedUnit == Unit) return true;
	CommandedUnit = Unit;
	AdvanceCommandBindingGeneration();
	ForceNetUpdate();
	RefreshCommandBinding();
	if (CombatEconomyComponent) CombatEconomyComponent->RefreshInventoryProjection();
	return true;
}

void ACombatPlayerController::SelectCombatUnit(ACombatUnitCharacter* Unit, const bool bToggle)
{
	if (!IsLocalController() || !CanInspect(*this, Unit, true)) return;
	if (!CanControlUnit(Unit))
	{
		// Shift 不能把只读目标混入控制组，也不应破坏正在追加的己方选择。
		if (bToggle) return;
		SelectedUnits.Reset();
		InspectedUnit = Unit;
		bSelectionInitialized = true;
		PublishLocalSelection();
		return;
	}
	if (bToggle)
	{
		if (!bSelectionInitialized) for (ACombatUnitCharacter* Selected : GetSelectedUnits()) SelectedUnits.Add(Selected);
		const int32 Index = SelectedUnits.IndexOfByKey(TWeakObjectPtr<ACombatUnitCharacter>(Unit));
		if (Index != INDEX_NONE) SelectedUnits.RemoveAt(Index);
		else if (SelectedUnits.Num() < MaxSelectedUnits) SelectedUnits.Add(Unit);
	}
	else
	{
		SelectedUnits.Reset();
		SelectedUnits.Add(Unit);
	}
	bSelectionInitialized = true;
	InspectedUnit = SelectedUnits.IsEmpty() ? nullptr : SelectedUnits[0].Get();
	PublishLocalSelection();
}

void ACombatPlayerController::SelectCombatUnits(const TArray<ACombatUnitCharacter*>& Units, const bool bAppend)
{
	if (!IsLocalController()) return;
	TArray<TWeakObjectPtr<ACombatUnitCharacter>> Next;
	if (bAppend) for (ACombatUnitCharacter* Unit : GetSelectedUnits()) Next.Add(Unit);
	for (ACombatUnitCharacter* Unit : Units)
	{
		if (!CanControlUnit(Unit) || !CanInspect(*this, Unit, false)) continue;
		if (Next.Num() < MaxSelectedUnits) Next.AddUnique(Unit);
	}
	if (Next.IsEmpty()) return;
	SelectedUnits = MoveTemp(Next);
	bSelectionInitialized = true;
	InspectedUnit = SelectedUnits[0];
	PublishLocalSelection();
}

void ACombatPlayerController::PublishLocalSelection()
{
	CancelCombatTargeting();
	PendingPrimaryRequestId = 0;
	if (SelectedUnits.IsEmpty() || !CanControlUnit(SelectedUnits[0].Get())) return;
	ACombatUnitCharacter* Primary = SelectedUnits[0].Get();
	// 即使与尚未更新的复制值相同，也要发送最后一次选择。A→B→A 快点时，
	// 前一个 B 请求可能仍在途；省略 A 会让服务端最终停在 B，导致本地永久等待。
	if (Primary == CommandedUnit && (!LastRequestedPrimary.IsValid() || LastRequestedPrimary.Get() == Primary)) return;
	LastRequestedPrimary = Primary;
	PendingPrimaryRequestId = AllocateCombatRequestId();
	ServerSelectPrimaryUnit(Primary, PendingPrimaryRequestId);
}

void ACombatPlayerController::RefreshLocalSelection()
{
	if (!IsLocalController()) return;
	if (!bSelectionInitialized)
	{
		if (CanControlUnit(CommandedUnit))
		{
			SelectedUnits.Add(CommandedUnit);
			InspectedUnit = CommandedUnit;
			bSelectionInitialized = true;
		}
		return;
	}
	const int32 Removed = SelectedUnits.RemoveAll([this](const auto& Unit) { return !CanControlUnit(Unit.Get()); });
	if (Removed > 0)
	{
		InspectedUnit = SelectedUnits.IsEmpty() ? nullptr : SelectedUnits[0].Get();
		PublishLocalSelection();
	}
	if (Removed > 0 && !InspectedUnit.IsValid() && SelectedUnits.IsEmpty() && CanControlUnit(CommandedUnit))
	{
		SelectedUnits.Add(CommandedUnit);
		InspectedUnit = CommandedUnit;
	}
}

void ACombatPlayerController::ServerSelectPrimaryUnit_Implementation(ACombatUnitCharacter* Unit, const int32 RequestId)
{
	auto* Security = GetWorld()->GetSubsystem<UCombatNetworkSecuritySubsystem>();
	FGameplayTag Failure;
	FString Diagnostic;
	const bool bAccepted = Security && Security->ValidateAndConsumePrimarySelection(this, Unit, RequestId, Failure, Diagnostic)
		&& SetPrimaryUnitAuthority(Unit);
	ClientPrimarySelectionResult(RequestId, bAccepted);
}

void ACombatPlayerController::ClientPrimarySelectionResult_Implementation(const int32 RequestId, const bool bAccepted)
{
	if (PendingPrimaryRequestId != RequestId) return;
	PendingPrimaryRequestId = 0;
	if (!bAccepted)
	{
		SelectedUnits.Reset();
		// 保留查看目标，但撤去命令组；玩家重新点击即可重试，不会误操作旧主控。
		CancelCombatTargeting();
	}
}

void ACombatPlayerController::BeginSelectionGesture()
{
	if (IsPointerOverCombatUI() || !GetMousePosition(SelectionStart.X, SelectionStart.Y)) return;
	ResetDestinationInput();
	bSelectionGesture = true;
	bSelectionAdditive = bSelectionModifierDown;
	SelectionEnd = SelectionStart;
}

void ACombatPlayerController::UpdateSelectionGesture()
{
	if (!bSelectionGesture) return;
	if (IsPointerOverCombatUI() || !IsCameraViewportFocused()
		|| !GetMousePosition(SelectionEnd.X, SelectionEnd.Y)) CancelSelectionGesture();
}

bool ACombatPlayerController::GetSelectionRectangle(FVector2D& Start, FVector2D& End) const
{
	Start = SelectionStart;
	End = SelectionEnd;
	return bSelectionGesture && FVector2D::DistSquared(Start, End) >= 36.0f;
}

void ACombatPlayerController::FinishSelectionGesture()
{
	if (!bSelectionGesture) return;
	UpdateSelectionGesture();
	if (!bSelectionGesture) return;
	const bool bAppend = bSelectionAdditive;
	FVector2D Start, End;
	const bool bBox = GetSelectionRectangle(Start, End);
	CancelSelectionGesture();
	if (!bBox)
	{
		FHitResult Hit;
		if (GetHitResultUnderCursor(ECC_Visibility, true, Hit)) SelectCombatUnit(Cast<ACombatUnitCharacter>(Hit.GetActor()), bAppend);
		return;
	}
	const FBox2D Bounds(FVector2D(FMath::Min(Start.X, End.X), FMath::Min(Start.Y, End.Y)),
		FVector2D(FMath::Max(Start.X, End.X), FMath::Max(Start.Y, End.Y)));
	TArray<ACombatUnitCharacter*> Units;
	for (TActorIterator<ACombatUnitCharacter> It(GetWorld()); It; ++It)
	{
		FVector2D Position;
		if (CanControlUnit(*It) && ProjectWorldLocationToScreen(It->GetActorLocation(), Position)
			&& Bounds.IsInsideOrOn(Position)) Units.Add(*It);
	}
	Units.Sort([](const ACombatUnitCharacter& A, const ACombatUnitCharacter& B) { return A.GetFName().LexicalLess(B.GetFName()); });
	SelectCombatUnits(Units, bAppend);
}

void ACombatPlayerController::CancelSelectionGesture() { bSelectionGesture = false; }
void ACombatPlayerController::OnSelectionModifierStarted() { bSelectionModifierDown = true; }
void ACombatPlayerController::OnSelectionModifierReleased() { bSelectionModifierDown = false; }

bool ACombatPlayerController::SubmitSelectedGroupOrder(const FCombatOrderRequest& Order)
{
	FCombatGroupOrderRequest Request;
	Request.RequestId = AllocateCombatRequestId();
	Request.Order = Order;
	for (ACombatUnitCharacter* Unit : GetSelectedUnits())
	{
		if (Order.Type != ECombatOrderType::Stop && !CanInspect(*this, Unit, false)) continue;
		auto& Entry = Request.Units.AddDefaulted_GetRef();
		Entry.Unit = Unit;
		Entry.LifeGeneration = Unit->GetLifeGeneration();
	}
	if (Request.Units.IsEmpty()) return false;
	ServerIssueGroupOrder(MoveTemp(Request));
	return true;
}

FCombatOrderBatchResult ACombatPlayerController::ProcessGroupOrderRequest(const FCombatGroupOrderRequest& Request)
{
	FCombatOrderBatchResult Result;
	Result.RequestId = Request.RequestId;
	auto* Security = GetWorld() ? GetWorld()->GetSubsystem<UCombatNetworkSecuritySubsystem>() : nullptr;
	FString Diagnostic;
	if (!HasAuthority() || !Security || !Security->ValidateAndConsumeGroupRequest(this, Request, Result.FailureTag, Diagnostic)) return Result;
	Result.bAccepted = true;
	for (const auto& Entry : Request.Units)
	{
		if (!IsValid(Entry.Unit) || Entry.Unit->GetLifeGeneration() != Entry.LifeGeneration) continue;
		FCombatOrderBatchRequest Batch;
		Batch.RequestId = Request.RequestId;
		Batch.UnitLifeGeneration = Entry.LifeGeneration;
		Batch.CommandBindingGeneration = CommandBindingGeneration;
		Batch.Orders.Add(Request.Order);
		const auto UnitResult = Entry.Unit->ExecuteValidatedOrderBatch(this, Batch);
		Entry.Unit->ClientReceiveOrderBatchResult(UnitResult);
		Result.AcceptedOrderCount += UnitResult.AcceptedOrderCount;
		Result.OrderResults.Append(UnitResult.OrderResults);
	}
	return Result;
}

void ACombatPlayerController::ServerIssueGroupOrder_Implementation(FCombatGroupOrderRequest Request)
{
	ProcessGroupOrderRequest(Request);
}
