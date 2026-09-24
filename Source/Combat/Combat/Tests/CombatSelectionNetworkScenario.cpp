#include "Combat/Tests/CombatSelectionNetworkScenario.h"
#include "CombatPlayerController.h"
#include "CombatCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Combat/Unit/CombatUnitCharacter.h"
#include "Combat/Unit/CombatUnitAIController.h"
#include "Combat/View/CombatUnitViewComponent.h"
#include "Combat/Network/CombatNetworkSecuritySubsystem.h"
#include "Combat/UI/CombatHUDWidget.h"
#include "Combat/UI/CombatPlayerHUD.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

ACombatSelectionNetworkScenario::ACombatSelectionNetworkScenario()
{
	PrimaryActorTick.bCanEverTick = true;
}

void ACombatSelectionNetworkScenario::Finish(const bool bPassed, const FString& Detail)
{
	UE_LOG(LogTemp, Display, TEXT("SelectionNetworkSmoke Role=%s Result=%s Input=Synthetic %s"),
		GetNetMode() == NM_DedicatedServer ? TEXT("Server") : GetNetMode() == NM_Standalone ? TEXT("PIE") : TEXT("Client"),
		bPassed ? TEXT("Pass") : TEXT("Fail"), *Detail);
	if (GetNetMode() == NM_Standalone)
		FFileHelper::SaveStringToFile(FString::Printf(TEXT("Result=%s Input=Synthetic %s"), bPassed ? TEXT("Pass") : TEXT("Fail"), *Detail),
			*FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("MultiControl/PIESmoke.txt")));
	SetActorTickEnabled(false);
}

void ACombatSelectionNetworkScenario::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Elapsed += DeltaSeconds;
	if (Elapsed > 65) { Finish(false, FString::Printf(TEXT("Timeout Phase=%d Rise=%.2f Stable=%.2f First=%s Second=%s Goal=%s"),
		Phase, MaxArrivalRise, ArrivalStableSeconds, First.IsValid() ? *First->GetActorLocation().ToString() : TEXT("None"),
		Second.IsValid() ? *Second->GetActorLocation().ToString() : TEXT("None"), *MoveGoal.ToString())); return; }
	if (GetNetMode() == NM_DedicatedServer)
	{
		if (Phase == 0)
		{
			int32 Players = 0;
			for (TActorIterator<ACombatPlayerController> It(GetWorld()); It; ++It) ++Players;
			if (Players < 2 || Elapsed < 2) return;
			int32 Team = 0;
			for (TActorIterator<ACombatPlayerController> It(GetWorld()); It; ++It)
			{
				++Team;
				for (TActorIterator<ACombatUnitCharacter> Unit(GetWorld()); Unit; ++Unit)
					if (Unit->GetCommandingPlayerController() == *It)
					{
						Unit->SetCombatTeamId(FCombatTeamId(Team));
						ServerStarts.Add(*Unit, Unit->GetActorLocation());
					}
			}
			Phase = 1;
		}
		for (const auto& Pair : ServerStarts)
		{
			if (!Pair.Key.IsValid()) continue;
			const FVector Position = Pair.Key->GetActorLocation();
			MaxArrivalRise = FMath::Max(MaxArrivalRise, Position.Z - Pair.Value.Z);
			if (Pair.Key->GetCharacterMovement()->IsFalling())
				MaxAirborneUpSpeed = FMath::Max(MaxAirborneUpSpeed, double(Pair.Key->GetVelocity().Z));
			if (const FVector* Previous = ServerPrevious.Find(Pair.Key))
				MaxServerStep = FMath::Max(MaxServerStep, FVector::Dist2D(Position, *Previous)
					- Pair.Key->GetCharacterMovement()->GetMaxSpeed() * DeltaSeconds);
			ServerPrevious.Add(Pair.Key, Position);
		}
		if (Elapsed < 10) return;
		const auto Stats = GetWorld()->GetSubsystem<UCombatNetworkSecuritySubsystem>()->GetSecurityStats();
		bool bValid = ServerStarts.Num() == 4 && Stats.OwnershipRejects >= 2;
		for (const auto& Pair : ServerStarts)
			bValid &= Pair.Key.IsValid() && Pair.Key->GetCommandingPlayerController()
				&& Cast<ACombatUnitAIController>(Pair.Key->GetController()) && Pair.Key->GetRemoteRole() == ROLE_SimulatedProxy
				&& FVector::Dist2D(Pair.Key->GetActorLocation(), Pair.Value) > 75;
		if (!bValid && Elapsed < 60) return;
		Finish(bValid && MaxAirborneUpSpeed < 5 && MaxServerStep < 5,
			FString::Printf(TEXT("Owned=%d OwnershipRejects=%lld ServerMovement=Checked MaxRise=%.2f AirborneUpSpeed=%.2f ExcessStep=%.2f"),
				ServerStarts.Num(), Stats.OwnershipRejects, MaxArrivalRise, MaxAirborneUpSpeed, MaxServerStep));
		return;
	}
	auto* Player = Cast<ACombatPlayerController>(GetWorld()->GetFirstPlayerController());
	if (!Player || !Player->IsLocalController() || Elapsed < 5) return;
	if (Phase == 0)
	{
		TArray<ACombatUnitCharacter*> Owned;
		for (TActorIterator<ACombatUnitCharacter> It(GetWorld()); It; ++It)
		{
			if (Player->CanControlUnit(*It)) Owned.Add(*It);
			else if (It->GetCombatUnitViewComponent()->GetHUDInspectionView().UnitDefinitionId.IsValid()) Foreign = *It;
		}
		if (Owned.Num() != 2 || !Foreign.IsValid() || !Player->CanOperateInspectedUnit()) return;
		First = Player->GetCommandedUnit();
		Second = Owned[0] == First.Get() ? Owned[1] : Owned[0];
		auto* Camera = Cast<ACombatCharacter>(Player->GetPawn());
		if (!Camera) return;
		const auto Public = Foreign->GetCombatUnitViewComponent()->GetHUDInspectionView();
		if (Public.Abilities.IsEmpty() && GetNetMode() != NM_Standalone) return;
		if ((!Public.Abilities.IsEmpty() && Public.Abilities[0].SpecHandle.IsValid()) || Public.InventoryRevision != 0
			|| (GetNetMode() != NM_Standalone && Foreign->GetCombatUnitViewComponent()->GetHUDOwnerView().LifeGeneration != 0))
		{ Finish(false, TEXT("Foreign private projection leaked")); return; }
		// 模拟玩家已移开的自由镜头，随后跨真实 RPC/复制检查选择全过程不回拉。
		CameraAnchor = Camera->GetActorLocation() + FVector(700, 500, 0);
		Camera->SetActorLocation(CameraAnchor);
		Player->SelectCombatUnit(Foreign.Get());
		Phase = 1;
		return;
	}
	if (!First.IsValid() || !Second.IsValid() || !Foreign.IsValid()) { Finish(false, TEXT("Unit disappeared")); return; }
	const auto* Camera = Cast<ACombatCharacter>(Player->GetPawn());
	if (!Camera || !Camera->GetActorLocation().Equals(CameraAnchor, 0.01))
	{ Finish(false, TEXT("Selection changed free camera anchor")); return; }
	if (Phase == 1)
	{
		auto* HUD = Cast<ACombatPlayerHUD>(Player->GetHUD());
		if (!HUD || !HUD->GetCombatWidget()) return;
		// NullRHI 不推进 Slate 的 NativeTick；显式绑定实际 Controller 查看目标来检查复制投影与权限。
		// 可见界面的自动换绑和框选几何由 PIE/人工验收覆盖，不计入本合成输入场景。
		HUD->GetCombatWidget()->InitializeForUnit(Player->GetInspectedUnit());
		if (HUD->GetCombatWidget()->GetObservedUnit() != Foreign.Get()) return;
		if (Player->CanOperateInspectedUnit() || HUD->GetCombatWidget()->CanOperateObservedUnit())
		{ Finish(false, TEXT("Inspection allowed input")); return; }
		Player->SelectCombatUnits({Second.Get(), First.Get()}, false);
		Phase = 2;
		return;
	}
	if (Phase == 2)
	{
		if (Player->GetCommandedUnit() != Second.Get() || !Player->CanOperateInspectedUnit()) return;
		FirstStart = First->GetActorLocation(); SecondStart = Second->GetActorLocation();
		FCombatOrderRequest Order;
		Order.Type = ECombatOrderType::MoveToPoint;
		Order.bHasTargetLocation = true;
		MoveGoal = FirstStart + FVector(350, 100, 0);
		Order.TargetLocation = MoveGoal;
		LastRequest = Player->NextCombatOrderRequestId;
		if (!Player->SubmitCombatOrder(Order)) { Finish(false, TEXT("Group move submission failed")); return; }
		Phase = 3;
		return;
	}
	if (Phase == 3 || Phase == 4)
	{
		MaxArrivalRise = FMath::Max(MaxArrivalRise, FMath::Max(First->GetActorLocation().Z - FirstStart.Z,
			Second->GetActorLocation().Z - SecondStart.Z));
		// Demo 含坡道；允许正常地形高度变化，但单位接触不能注入腾空向上速度。
		for (auto* Unit : {First.Get(), Second.Get()})
			if (Unit->GetCharacterMovement()->IsFalling())
				MaxAirborneUpSpeed = FMath::Max(MaxAirborneUpSpeed, double(Unit->GetVelocity().Z));
	}
	if (Phase == 3 || Phase == 5 || Phase == 6)
	{
		if (First->GetLastOrderBatchResult().RequestId != LastRequest || Second->GetLastOrderBatchResult().RequestId != LastRequest) return;
		if (First->GetLastOrderBatchResult().AcceptedOrderCount != 1 || Second->GetLastOrderBatchResult().AcceptedOrderCount != 1)
		{ Finish(false, FString::Printf(TEXT("Group receipt rejected Phase=%d"), Phase)); return; }
		if (Phase == 3) { Player->SelectCombatUnit(First.Get()); Phase = 4; return; }
		if (Phase == 5) { LastRequest = Player->NextCombatOrderRequestId; Player->OnStopCommand(); Phase = 6; return; }
		FCombatGroupOrderRequest Forged;
		Forged.RequestId = Player->AllocateCombatRequestId();
		Forged.Order.Type = ECombatOrderType::Stop;
		Forged.Units = {{First.Get(), int64(First->GetLifeGeneration())}, {Foreign.Get(), int64(Foreign->GetLifeGeneration())}};
		Player->ServerIssueGroupOrder(Forged);
		const ENetRole ExpectedRole = GetNetMode() == NM_Standalone ? ROLE_Authority : ROLE_SimulatedProxy;
		Finish(First->GetLocalRole() == ExpectedRole && Second->GetLocalRole() == ExpectedRole,
			FString::Printf(TEXT("PublicHUD=ReadOnly Focus=Preserved Camera=Preserved GroupMoveAttackStop=Accepted Movement=%s Forgery=Sent ArrivalLegs=%d ArrivalStable=%.2f MaxRise=%.2f AirborneUpSpeed=%.2f"),
				GetNetMode() == NM_Standalone ? TEXT("Authority") : TEXT("Replicated"), MoveLeg + 1, ArrivalStableSeconds, MaxArrivalRise, MaxAirborneUpSpeed));
		return;
	}
	if (Phase == 4)
	{
		const bool bArrived = FVector::Dist2D(First->GetActorLocation(), MoveGoal) < 150
			&& FVector::Dist2D(Second->GetActorLocation(), MoveGoal) < 150
			&& First->GetVelocity().Size() < 5 && Second->GetVelocity().Size() < 5;
		ArrivalStableSeconds = bArrived ? ArrivalStableSeconds + DeltaSeconds : 0;
		if (ArrivalStableSeconds < 3) return;
		if (MaxAirborneUpSpeed >= 5) { Finish(false, FString::Printf(TEXT("Arrival launched unit AirborneUpSpeed=%.2f"), MaxAirborneUpSpeed)); return; }
		if (Player->GetCommandedUnit() != First.Get() || !Player->CanOperateInspectedUnit()
			|| (MoveLeg == 0 && (FVector::Dist2D(First->GetActorLocation(), FirstStart) < 100
				|| FVector::Dist2D(Second->GetActorLocation(), SecondStart) < 100))) return;
		Player->SelectCombatUnits({First.Get(), Second.Get()}, false);
		if (MoveLeg < 2)
		{
			// 沿已走通路径折返后再次汇聚；不把任意偏移到不可导航区域当作到达回归。
			++MoveLeg;
			MoveGoal = FirstStart + (MoveLeg == 1 ? FVector::ZeroVector : FVector(350, 100, 0));
			FCombatOrderRequest Move;
			Move.Type = ECombatOrderType::MoveToPoint;
			Move.bHasTargetLocation = true;
			Move.TargetLocation = MoveGoal;
			LastRequest = Player->NextCombatOrderRequestId;
			if (!Player->SubmitCombatOrder(Move)) { Finish(false, TEXT("Arrival repeat submission failed")); return; }
			ArrivalStableSeconds = 0;
			Phase = 3;
			return;
		}
		LastRequest = Player->NextCombatOrderRequestId;
		if (!Player->IssueCombatAttackOrder(Foreign.Get())) return;
		Phase = 5;
	}
}
