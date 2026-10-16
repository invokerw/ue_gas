#include "Combat/Defense/DefenseGameMode.h"
#include "Combat/Defense/DefenseData.h"
#include "Combat/Defense/DefenseDirectorComponent.h"
#include "Combat/Defense/DefenseGameState.h"
#include "Combat/Defense/DefenseObjective.h"
#include "Combat/Log/CombatEventSubsystem.h"
#include "CombatPlayerController.h"
#include "EngineUtils.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Engine/World.h"
ADefenseGameMode::ADefenseGameMode()
{
	GameStateClass = ADefenseGameState::StaticClass();
	Director = CreateDefaultSubobject<UDefenseDirectorComponent>(TEXT("DefenseDirector"));
	AdditionalControlledUnitCount = 0;
}
void ADefenseGameMode::BeginPlay()
{
	Super::BeginPlay();
	FString Error;
	if (!RunData || !RunData->ValidateRuntime(Error))
	{ UE_LOG(LogCombat, Error, TEXT("DefenseConfigurationRejected %s"), *Error); return; }
	FTransform Transform(FRotator::ZeroRotator, RunData->ObjectiveLocation);
	Objective = GetWorld()->SpawnActorDeferred<ADefenseObjective>(ADefenseObjective::StaticClass(), Transform,
		nullptr, nullptr, ESpawnActorCollisionHandlingMethod::DontSpawnIfColliding);
	if (!Objective || !Objective->InitializeFromUnitData(RunData->ObjectiveData))
	{ if (Objective) Objective->Destroy(); UE_LOG(LogCombat, Error, TEXT("DefenseCoreSpawnFailed")); return; }
	Objective->FinishSpawning(Transform);
	if (!Director->InitializeForRun(RunData, Objective, GetGameState<ADefenseGameState>(),
		bDevelopmentWaitForStart && GetNetMode()!=NM_DedicatedServer && !FParse::Param(FCommandLine::Get(),TEXT("DefenseSmoke"))))
	{ Objective->Destroy(); UE_LOG(LogCombat, Error, TEXT("DefenseInitializationFailed")); return; }
	if(!Director->InitializeTeamItems(GetShopData())) { UE_LOG(LogCombat,Error,TEXT("DefenseTeamItemsConfigurationFailed")); return; }
	for (TActorIterator<ACombatPlayerController> It(GetWorld()); It; ++It) RegisterHero(*It);
}
APawn* ADefenseGameMode::SpawnDefaultPawnAtTransform_Implementation(AController* Player, const FTransform& Transform)
{
	APawn* Pawn = Super::SpawnDefaultPawnAtTransform_Implementation(Player, Transform);
	RegisterHero(Player);
	return Pawn;
}
void ADefenseGameMode::RegisterHero(AController* Player)
{
	if (auto* PC = Cast<ACombatPlayerController>(Player)) Director->RegisterPlayer(PC, PC->GetCommandedUnit());
}
void ADefenseGameMode::PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& Id, FString& Error)
{
	Super::PreLogin(Options, Address, Id, Error);
	if (Error.IsEmpty() && Director->GetPhase() != EDefensePhase::Preparation) Error = TEXT("DefenseRunAlreadyStarted");
}
void ADefenseGameMode::Logout(AController* Exiting)
{
	if (auto* PC = Cast<ACombatPlayerController>(Exiting)) Director->RemovePlayer(PC);
	Super::Logout(Exiting);
}
void ADefenseGameMode::EndPlay(const EEndPlayReason::Type Reason)
{
	Director->Shutdown();
	if (IsValid(Objective))
	{
		auto* Controller = Objective->GetController();
		Objective->Destroy(); if (IsValid(Controller)) Controller->Destroy();
	}
	Objective = nullptr;
	Super::EndPlay(Reason);
}
