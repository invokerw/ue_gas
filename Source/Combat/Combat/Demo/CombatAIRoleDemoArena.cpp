#include "Combat/Demo/CombatAIRoleDemoArena.h"
#include "Combat/AI/CombatAIBrainComponent.h"
#include "Combat/AI/CombatAIProfileData.h"
#include "Combat/Scheduling/CombatSchedulerSubsystem.h"
#include "Combat/Unit/CombatUnitCharacter.h"
#include "GameFramework/SpringArmComponent.h"
#include "Components/TextRenderComponent.h"
#include "NavigationSystem.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"

ACombatAIRoleObserverPawn::ACombatAIRoleObserverPawn() { GetCameraBoom()->TargetArmLength = 2200; }

ACombatAIRoleDemoArena::ACombatAIRoleDemoArena()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	bAlwaysRelevant = true;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot")));
	const auto Label = [&](const TCHAR* Name, const TCHAR* Text, FVector Location, FColor Color)
	{
		auto* Component = CreateDefaultSubobject<UTextRenderComponent>(FName(Name));
		Component->SetupAttachment(GetRootComponent());
		Component->SetText(FText::FromString(Text));
		Component->SetRelativeLocation(Location);
		Component->SetRelativeRotation(FRotator(90, 0, 180));
		Component->SetWorldSize(48);
		Component->SetTextRenderColor(Color);
		Component->SetHorizontalAlignment(EHTA_Center);
	};
	Label(TEXT("GuardLabel"), TEXT("GUARD  >  ENGAGE  >  RETURN"), FVector(300, -850, -70), FColor(255, 205, 90));
	Label(TEXT("LaneLabel"), TEXT("LANE  >  ENGAGE  >  RESUME"), FVector(600, 850, -70), FColor(100, 220, 255));
	Label(TEXT("GuardTargetLabel"), TEXT("GUARD TARGET"), FVector(650, -750, -70), FColor::White);
	Label(TEXT("LaneTargetLabel"), TEXT("LANE TARGET"), FVector(1000, 750, -70), FColor::White);
}

void ACombatAIRoleDemoArena::BeginPlay()
{
	Super::BeginPlay();
	if (!HasAuthority() || GetNetMode() == NM_Client || !GuardProfile || !LaneProfile || !GuardClass || !LaneClass || !TargetClass) return;
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	const auto Spawn = [&](TSubclassOf<ACombatUnitCharacter> Class, FVector Offset, uint8 Team)
	{
		auto* Unit = GetWorld()->SpawnActor<ACombatUnitCharacter>(Class, GetActorLocation() + Offset, FRotator::ZeroRotator, Params);
		if (Unit) Unit->SetCombatTeamId(FCombatTeamId(Team));
		return Unit;
	};
	GuardAgent = Spawn(GuardClass, FVector(0, -600, 0), 1);
	LaneAgent = Spawn(LaneClass, FVector(0, 600, 0), 1);
	GuardTarget = Spawn(TargetClass, FVector(650, -600, 0), 2);
	LaneTarget = Spawn(TargetClass, FVector(1000, 600, 0), 2);
	StartupDeadline = GetWorld()->GetTimeSeconds() + 30;
	StartupSchedule = GetWorld()->GetSubsystem<UCombatSchedulerSubsystem>()->ScheduleRepeating(this, 0, 0.1, 0,
		ECombatCatchUpPolicy::Coalesce, FCombatScheduledDelegate::CreateUObject(this, &ThisClass::TryStart));
}

void ACombatAIRoleDemoArena::TryStart(const FCombatScheduledTickContext&)
{
	++StartupAttempts;
	auto* Scheduler = GetWorld()->GetSubsystem<UCombatSchedulerSubsystem>();
	if (GetWorld()->GetTimeSeconds() >= StartupDeadline || !IsValid(GuardAgent) || !IsValid(LaneAgent) || !IsValid(GuardTarget) || !IsValid(LaneTarget))
	{
		UE_LOG(LogTemp, Warning, TEXT("AIRoleDemoStartupUnavailable Arena=%s Attempts=%d Time=%.2f Guard=%d Lane=%d GuardTarget=%d LaneTarget=%d"),
			*GetName(), StartupAttempts, GetWorld()->GetTimeSeconds(), IsValid(GuardAgent) ? 1 : 0, IsValid(LaneAgent) ? 1 : 0,
			IsValid(GuardTarget) ? 1 : 0, IsValid(LaneTarget) ? 1 : 0);
		Scheduler->Cancel(StartupSchedule); StartupSchedule = {}; return;
	}
	auto* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	if (!Nav)
	{
		if (StartupAttempts == 1 || StartupAttempts % 10 == 0) UE_LOG(LogTemp, Warning, TEXT("AIRoleDemoStartupWait Arena=%s Reason=NoNavigationSystem Attempts=%d"), *GetName(), StartupAttempts);
		return;
	}
	const auto Project = [&](FVector Point, FNavLocation& Result)
	{
		return Nav->ProjectPointToNavigation(Point, Result, FVector(100, 100, 250), &GuardAgent->GetNavAgentPropertiesRef());
	};
	FNavLocation Home, Start, GuardGoal, LaneGoal, RouteGoal;
	const bool bHome = Project(GuardAgent->GetActorLocation(), Home);
	const bool bStart = Project(LaneAgent->GetActorLocation(), Start);
	const bool bGuardGoal = Project(GuardTarget->GetActorLocation(), GuardGoal);
	const bool bLaneGoal = Project(LaneTarget->GetActorLocation(), LaneGoal);
	const bool bRouteGoal = Project(GetActorLocation() + FVector(1500, 600, 0), RouteGoal);
	if (!bHome || !bStart || !bGuardGoal || !bLaneGoal || !bRouteGoal)
	{
		if (StartupAttempts == 1 || StartupAttempts % 10 == 0) UE_LOG(LogTemp, Warning,
			TEXT("AIRoleDemoStartupWait Arena=%s Reason=Project Home=%d Start=%d GuardGoal=%d LaneGoal=%d RouteGoal=%d GuardPos=%s LanePos=%s"),
			*GetName(), bHome ? 1 : 0, bStart ? 1 : 0, bGuardGoal ? 1 : 0, bLaneGoal ? 1 : 0, bRouteGoal ? 1 : 0,
			*GuardAgent->GetActorLocation().ToCompactString(), *LaneAgent->GetActorLocation().ToCompactString());
		return;
	}
	const auto* NavData = Nav->GetNavDataForProps(GuardAgent->GetNavAgentPropertiesRef(), Home.Location);
	if (!NavData)
	{
		if (StartupAttempts == 1 || StartupAttempts % 10 == 0) UE_LOG(LogTemp, Warning, TEXT("AIRoleDemoStartupWait Arena=%s Reason=NoNavData Home=%s"), *GetName(), *Home.Location.ToCompactString());
		return;
	}
	const auto Reachable = [&](FVector From, FVector To)
	{
		FPathFindingQuery Query(GuardAgent.Get(), *NavData, From, To);
		Query.SetAllowPartialPaths(false);
		return Nav->TestPathSync(Query);
	};
	const bool bGuardReachable = Reachable(Home.Location, GuardGoal.Location);
	const bool bLaneReachable = Reachable(Start.Location, LaneGoal.Location);
	const bool bRouteReachable = Reachable(Start.Location, RouteGoal.Location);
	if (!bGuardReachable || !bLaneReachable || !bRouteReachable)
	{
		if (StartupAttempts == 1 || StartupAttempts % 10 == 0) UE_LOG(LogTemp, Warning,
			TEXT("AIRoleDemoStartupWait Arena=%s Reason=Unreachable Guard=%d Lane=%d Route=%d Home=%s Start=%s GuardGoal=%s LaneGoal=%s RouteGoal=%s"),
			*GetName(), bGuardReachable ? 1 : 0, bLaneReachable ? 1 : 0, bRouteReachable ? 1 : 0,
			*Home.Location.ToCompactString(), *Start.Location.ToCompactString(), *GuardGoal.Location.ToCompactString(),
			*LaneGoal.Location.ToCompactString(), *RouteGoal.Location.ToCompactString());
		return;
	}
	Scheduler->Cancel(StartupSchedule); StartupSchedule = {};
	GuardHome = Home.Location;
	FCombatAIAssignment GuardDuty;
	GuardDuty.Home = Home.Location;
	GuardAgent->GetCombatAIBrainComponent()->SetAssignment(GuardDuty);
	GuardAgent->GetCombatAIBrainComponent()->ConfigureProfile(GuardProfile);
	FCombatAIAssignment LaneDuty;
	LaneDuty.Home = Start.Location;
	LaneDuty.Route = { RouteGoal.Location };
	LaneAgent->GetCombatAIBrainComponent()->SetAssignment(LaneDuty);
	LaneAgent->GetCombatAIBrainComponent()->ConfigureProfile(LaneProfile);
	UE_LOG(LogTemp, Display, TEXT("AIRoleDemoReady Guard=%s Lane=%s GuardTarget=%s LaneTarget=%s"),
		*GuardAgent->GetName(), *LaneAgent->GetName(), *GuardTarget->GetName(), *LaneTarget->GetName());
}

void ACombatAIRoleDemoArena::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ACombatAIRoleDemoArena, GuardAgent);
	DOREPLIFETIME(ACombatAIRoleDemoArena, LaneAgent);
	DOREPLIFETIME(ACombatAIRoleDemoArena, GuardTarget);
	DOREPLIFETIME(ACombatAIRoleDemoArena, LaneTarget);
}

void ACombatAIRoleDemoArena::EndPlay(const EEndPlayReason::Type Reason)
{
	if (auto* Scheduler = GetWorld() ? GetWorld()->GetSubsystem<UCombatSchedulerSubsystem>() : nullptr) Scheduler->CancelAllForOwner(this);
	if (HasAuthority() && Reason == EEndPlayReason::Destroyed)
		for (auto* Unit : { GuardAgent.Get(), LaneAgent.Get(), GuardTarget.Get(), LaneTarget.Get() }) if (IsValid(Unit)) Unit->Destroy();
	Super::EndPlay(Reason);
}
