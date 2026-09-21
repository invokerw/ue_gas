#include "Combat/Demo/CombatAIDemoArena.h"
#include "Combat/AI/CombatAIBrainComponent.h"
#include "Combat/AI/CombatAIProfileData.h"
#include "Combat/Unit/CombatUnitCharacter.h"
#include "Combat/Scheduling/CombatSchedulerSubsystem.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
#include "NavigationSystem.h"

ACombatAIDemoArena::ACombatAIDemoArena()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	bAlwaysRelevant = true;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot")));
}

void ACombatAIDemoArena::BeginPlay()
{
	Super::BeginPlay();
	if (!HasAuthority() || GetNetMode() == NM_Client) return;
	if (!Profile || !AgentClass || !TargetClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("AIDemoNotConfigured Arena=%s"), *GetName());
		return;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	Agent = GetWorld()->SpawnActor<ACombatUnitCharacter>(AgentClass, GetActorLocation(), GetActorRotation(), Params);
	Target = GetWorld()->SpawnActor<ACombatUnitCharacter>(TargetClass, GetActorLocation() + TargetOffset, FRotator::ZeroRotator, Params);
	if (!Agent || !Target || !Agent->GetUnitDefinitionId().IsValid() || !Target->GetUnitDefinitionId().IsValid()) return;
	ReturnLocation = Agent->GetActorLocation();
	Agent->SetCombatTeamId(FCombatTeamId(1));
	Target->SetCombatTeamId(FCombatTeamId(2));
	Agent->GetCombatAIBrainComponent()->ConfigureProfile(Profile);
	StartupDeadline = GetWorld()->GetTimeSeconds() + 30.0;
	if (auto* Scheduler = GetWorld()->GetSubsystem<UCombatSchedulerSubsystem>())
	{
		StartupSchedule = Scheduler->ScheduleRepeating(this, 0.0, 0.1, 0, ECombatCatchUpPolicy::Coalesce,
			FCombatScheduledDelegate::CreateUObject(this, &ThisClass::TryStartAfterNavigation));
	}
}

void ACombatAIDemoArena::TryStartAfterNavigation(const FCombatScheduledTickContext& Context)
{
	if (!IsValid(Agent) || !IsValid(Target) || GetWorld()->GetTimeSeconds() >= StartupDeadline)
	{
		UE_LOG(LogTemp, Warning, TEXT("AIDemoStartupAborted Arena=%s NavigationOrUnitUnavailable"), *GetName());
		CancelStartupWait();
		return;
	}
	auto* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	if (!Navigation) return;
	FNavLocation Start, Goal;
	const auto& Properties = Agent->GetNavAgentPropertiesRef();
	if (Navigation->ProjectPointToNavigation(Agent->GetActorLocation(), Start, FVector(100, 100, 250), &Properties)
		&& Navigation->ProjectPointToNavigation(Target->GetActorLocation(), Goal, FVector(100, 100, 250), &Properties))
	{
		// 动态地图其他区域可以继续生成；只要求演示当前路线已有完整可达路径。
		if (const auto* NavData = Navigation->GetNavDataForProps(Properties, Start.Location))
		{
			FPathFindingQuery Query(Agent.Get(), *NavData, Start.Location, Goal.Location);
			Query.SetAllowPartialPaths(false);
			if (Navigation->TestPathSync(Query))
			{
				ReturnLocation = Start.Location;
				RequestAttack();
			}
		}
	}
}

void ACombatAIDemoArena::CancelStartupWait()
{
	if (auto* Scheduler = GetWorld() ? GetWorld()->GetSubsystem<UCombatSchedulerSubsystem>() : nullptr)
		Scheduler->Cancel(StartupSchedule);
	StartupSchedule = {};
}

void ACombatAIDemoArena::RequestReturn()
{
	if (!HasAuthority() || !GetWorld()->IsGameWorld() || !IsValid(Agent)) return;
	CancelStartupWait();
	FCombatOrderRequest Request;
	Request.Type = ECombatOrderType::MoveToPoint;
	Request.TargetLocation = ReturnLocation;
	Request.bHasTargetLocation = true;
	Agent->GetCombatAIBrainComponent()->SetObjective(Request);
}

void ACombatAIDemoArena::RequestAttack()
{
	if (!HasAuthority() || !GetWorld()->IsGameWorld() || !IsValid(Agent) || !IsValid(Target)) return;
	CancelStartupWait();
	auto* Brain = Agent->GetCombatAIBrainComponent();
	FCombatOrderRequest Request;
	Request.Type = ECombatOrderType::AttackTarget;
	Request.TargetUnit = Target;
	Brain->SetObjective(Request);
	if (!Brain->IsRunning()) Brain->ResumeAutonomous();
}

void ACombatAIDemoArena::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ACombatAIDemoArena, Agent);
	DOREPLIFETIME(ACombatAIDemoArena, Target);
}

void ACombatAIDemoArena::EndPlay(const EEndPlayReason::Type Reason)
{
	CancelStartupWait();
	if (HasAuthority() && Reason == EEndPlayReason::Destroyed)
	{
		if (IsValid(Agent)) Agent->Destroy();
		if (IsValid(Target)) Target->Destroy();
	}
	Super::EndPlay(Reason);
}
