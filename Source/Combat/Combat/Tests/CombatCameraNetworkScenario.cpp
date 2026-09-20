#include "Combat/Tests/CombatCameraNetworkScenario.h"
#include "Combat/Unit/CombatUnitAIController.h"
#include "Combat/Unit/CombatUnitCharacter.h"
#include "CombatCharacter.h"
#include "CombatPlayerController.h"
#include "Engine/World.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"

ACombatCameraNetworkScenario::ACombatCameraNetworkScenario()
{
	PrimaryActorTick.bCanEverTick = true;
}

void ACombatCameraNetworkScenario::Finish(const bool bPassed, const FString& Detail)
{
	const FString Result = FString::Printf(TEXT("CameraNetworkSmoke Result=%s NetMode=%d Input=Synthetic %s"),
		bPassed ? TEXT("Pass") : TEXT("Fail"), int32(GetNetMode()), *Detail);
	UE_LOG(LogTemp, Display, TEXT("%s"), *Result);
	FString ReportName;
	if (FParse::Value(FCommandLine::Get(), TEXT("CombatCameraReport="), ReportName)
		&& !ReportName.Contains(TEXT("/")) && !ReportName.Contains(TEXT("\\")) && !ReportName.Contains(TEXT("..")))
	{
		const FString Directory = FPaths::ProjectSavedDir() / TEXT("CameraValidation");
		IFileManager::Get().MakeDirectory(*Directory, true);
		FFileHelper::SaveStringToFile(Result, *(Directory / (ReportName + TEXT(".txt"))));
	}
	SetActorTickEnabled(false);
}

void ACombatCameraNetworkScenario::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Elapsed += DeltaSeconds;
	if (Elapsed > 60) { Finish(false, TEXT("Timeout waiting for real controller/unit binding")); return; }
	if (Elapsed < 5) return;
	if (GetNetMode() == NM_DedicatedServer)
	{
		if (Elapsed < 20) return;
		int32 Count = 0;
		bool bPassed = true;
		for (auto It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
		{
			auto* PC = Cast<ACombatPlayerController>(It->Get());
			auto* Camera = PC ? Cast<ACombatCharacter>(PC->GetPawn()) : nullptr;
			auto* Unit = PC ? PC->GetCommandedUnit() : nullptr;
			if (!Camera || !Unit) continue;
			++Count;
			const FVector Before = Camera->GetActorLocation();
			Camera->UpdateCamera(1, FVector2D(1, 1));
			bPassed &= !PC->IsLocalController() && !Camera->IsReplicatingMovement()
				&& Camera->GetActorLocation().Equals(Before) && Camera->BeginCameraFollow() == 0
				&& Cast<ACombatUnitAIController>(Unit->GetController()) != nullptr;
		}
		if (Count >= 2) Finish(bPassed, FString::Printf(TEXT("RemoteControllers=%d CameraWrites=Rejected UnitAI=Preserved"), Count));
		return;
	}
	auto* PC = Cast<ACombatPlayerController>(GetWorld()->GetFirstPlayerController());
	auto* Camera = PC ? Cast<ACombatCharacter>(PC->GetPawn()) : nullptr;
	auto* Unit = PC ? PC->GetCommandedUnit() : nullptr;
	if (!PC || !PC->IsLocalController() || !Camera || !Unit || Unit->GetCommandingPlayerController() != PC) return;
	Camera->SetFollowTarget(Unit, PC->GetCommandBindingGeneration());
	Camera->ResetCameraInput();
	const FVector Anchor = Camera->GetActorLocation(), UnitBefore = Unit->GetActorLocation();
	int32 ClientIndex = 1;
	FParse::Value(FCommandLine::Get(), TEXT("CombatCameraClientIndex="), ClientIndex);
	const FVector2D Size(1920, 1080);
	const FVector2D Edge = Camera->GetEdgePanInput(FVector2D(ClientIndex == 2 ? 0 : 1920, 540), Size);
	Camera->UpdateCamera(0.5f, Edge);
	const FVector Panned = Camera->GetActorLocation();
	bool bPassed = FVector::Dist2D(Panned, Anchor) > 100 && Panned.Z == Anchor.Z;
	Camera->UpdateCamera(0.1f, FVector2D::ZeroVector);
	bPassed &= Camera->GetActorLocation().Equals(Panned) && Camera->GetCameraMode() == ECombatCameraMode::Free;
	const uint64 Press = Camera->BeginCameraFollow();
	Camera->UpdateCamera(0.1f, FVector2D::ZeroVector);
	bPassed &= Press > 0 && FVector::Dist2D(Camera->GetActorLocation(), UnitBefore) < FVector::Dist2D(Panned, UnitBefore);
	Camera->EndCameraFollow(Press);
	const FVector Released = Camera->GetActorLocation();
	Camera->UpdateCamera(0.1f, FVector2D::ZeroVector);
	bPassed &= Camera->GetActorLocation().Equals(Released) && Unit->GetActorLocation().Equals(UnitBefore)
		&& !Camera->IsReplicatingMovement();
	PC->FlushPressedKeys();
	bPassed &= Camera->GetCameraMode() == ECombatCameraMode::Free;
	Finish(bPassed, FString::Printf(TEXT("Client=%d Anchor=(%s) Panned=(%s) Released=(%s) Unit=(%s)"),
		ClientIndex, *Anchor.ToCompactString(), *Panned.ToCompactString(), *Released.ToCompactString(), *UnitBefore.ToCompactString()));
}
