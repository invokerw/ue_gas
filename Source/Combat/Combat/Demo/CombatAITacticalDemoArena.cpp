#include "Combat/Demo/CombatAITacticalDemoArena.h"

#include "Combat/AI/CombatAIBrainComponent.h"
#include "Combat/AI/CombatAIProfileData.h"
#include "Combat/Attack/CombatAttackComponent.h"
#include "Combat/Attributes/CombatAttributeSet.h"
#include "Combat/Combat/CombatDamageSubsystem.h"
#include "Combat/Core/CombatTags.h"
#include "Combat/Order/CombatOrderComponent.h"
#include "Combat/Scheduling/CombatSchedulerSubsystem.h"
#include "Combat/Unit/CombatUnitCharacter.h"
#include "Components/TextRenderComponent.h"
#include "Engine/World.h"
#include "NavigationSystem.h"

ACombatAITacticalDemoArena::ACombatAITacticalDemoArena()
{
	PrimaryActorTick.bCanEverTick = false;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot")));
	const auto Label = [&](const TCHAR* Name, const TCHAR* Text, const FVector Location, const FColor Color)
	{
		auto* Component = CreateDefaultSubobject<UTextRenderComponent>(FName(Name));
		Component->SetupAttachment(GetRootComponent());
		Component->SetText(FText::FromString(Text));
		Component->SetRelativeLocation(Location);
		Component->SetRelativeRotation(FRotator(90, 0, 180));
		Component->SetWorldSize(48.0f);
		Component->SetTextRenderColor(Color);
		Component->SetHorizontalAlignment(EHTA_Center);
	};
	Label(TEXT("HeroLabel"), TEXT("HERO  ATTACK  >  BOUNDARY  >  HEAL"), FVector(300, -850, -70), FColor(120, 230, 130));
	Label(TEXT("RangedLabel"), TEXT("RANGED  EQS  >  MOVE ORDER  >  ATTACK"), FVector(400, 850, -70), FColor(100, 210, 255));
	Label(TEXT("HeroTargetLabel"), TEXT("HERO TARGET"), FVector(300, -500, -70), FColor::White);
	Label(TEXT("RangedTargetLabel"), TEXT("RANGED TARGET"), FVector(200, 500, -70), FColor::White);
}

void ACombatAITacticalDemoArena::BeginPlay()
{
	Super::BeginPlay();
	if (!HasAuthority() || GetNetMode() == NM_Client)
	{
		return;
	}
	if (!HeroProfile || !RangedProfile || !HeroClass || !RangedClass || !TargetClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("AITacticsDemoNotConfigured Arena=%s"), *GetName());
		return;
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	const auto Spawn = [&](const TSubclassOf<ACombatUnitCharacter> Class, const FVector Offset, const uint8 Team)
	{
		auto* Unit = GetWorld()->SpawnActor<ACombatUnitCharacter>(Class, GetActorLocation() + Offset, FRotator::ZeroRotator, Params);
		if (Unit)
		{
			Unit->SetCombatTeamId(FCombatTeamId(Team));
		}
		return Unit;
	};

	HeroAgent = Spawn(HeroClass, FVector(0, -500, 0), 1);
	RangedAgent = Spawn(RangedClass, FVector(0, 500, 0), 1);
	HeroTarget = Spawn(TargetClass, FVector(300, -500, 0), 2);
	RangedTarget = Spawn(TargetClass, FVector(200, 500, 0), 2);
	if (!IsValid(HeroAgent) || !IsValid(RangedAgent) || !IsValid(HeroTarget) || !IsValid(RangedTarget))
	{
		UE_LOG(LogTemp, Warning, TEXT("AITacticsDemoSpawnFailed Arena=%s"), *GetName());
		return;
	}

	HeroStart = HeroAgent->GetActorLocation();
	RangedStart = RangedAgent->GetActorLocation();
	HeroAttackBinding = HeroAgent->GetCombatAttackComponent()->OnAttackLaunched().AddUObject(
		this, &ThisClass::OnHeroAttackLaunched);
	HeroOrderBinding = HeroAgent->GetCombatOrderComponent()->OnOrderFinished().AddUObject(
		this, &ThisClass::OnHeroOrderFinished);
	StartupDeadline = GetWorld()->GetTimeSeconds() + 30.0;
	if (auto* Scheduler = GetWorld()->GetSubsystem<UCombatSchedulerSubsystem>())
	{
		StartupSchedule = Scheduler->ScheduleRepeating(this, 0.0, 0.1, 0,
			ECombatCatchUpPolicy::Coalesce, FCombatScheduledDelegate::CreateUObject(this, &ThisClass::TryStart));
	}
}

void ACombatAITacticalDemoArena::TryStart(const FCombatScheduledTickContext& Context)
{
	(void)Context;
	++StartupAttempts;
	auto* Scheduler = GetWorld()->GetSubsystem<UCombatSchedulerSubsystem>();
	if (GetWorld()->GetTimeSeconds() >= StartupDeadline || !IsValid(HeroAgent) || !IsValid(RangedAgent)
		|| !IsValid(HeroTarget) || !IsValid(RangedTarget))
	{
		UE_LOG(LogTemp, Warning, TEXT("AITacticsDemoStartupUnavailable Arena=%s Attempts=%d"), *GetName(), StartupAttempts);
		Scheduler->Cancel(StartupSchedule);
		StartupSchedule = {};
		return;
	}

	auto* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	if (!Navigation)
	{
		return;
	}
	const auto Project = [&](const ACombatUnitCharacter& Agent, const FVector Point, FNavLocation& Result)
	{
		return Navigation->ProjectPointToNavigation(Point, Result, FVector(100, 100, 250), &Agent.GetNavAgentPropertiesRef());
	};
	FNavLocation HeroHome;
	FNavLocation HeroGoal;
	FNavLocation RangedHome;
	FNavLocation RangedGoal;
	FNavLocation TacticalGoal;
	FVector Away = RangedTarget->GetActorLocation() - RangedAgent->GetActorLocation();
	Away.Z = 0.0f;
	if (!Away.Normalize())
	{
		Away = FVector::ForwardVector;
	}
	const FVector ExpectedTacticalGoal = RangedTarget->GetActorLocation() + Away * 600.0f;
	const bool bProjected = Project(*HeroAgent, HeroAgent->GetActorLocation(), HeroHome)
		&& Project(*HeroAgent, HeroTarget->GetActorLocation(), HeroGoal)
		&& Project(*RangedAgent, RangedAgent->GetActorLocation(), RangedHome)
		&& Project(*RangedAgent, RangedTarget->GetActorLocation(), RangedGoal)
		&& Project(*RangedAgent, ExpectedTacticalGoal, TacticalGoal);
	if (!bProjected)
	{
		if (StartupAttempts == 1 || StartupAttempts % 10 == 0)
		{
			UE_LOG(LogTemp, Warning, TEXT("AITacticsDemoStartupWait Arena=%s Reason=NavigationProjection Attempts=%d"),
				*GetName(), StartupAttempts);
		}
		return;
	}
	const auto* NavData = Navigation->GetNavDataForProps(HeroAgent->GetNavAgentPropertiesRef(), HeroHome.Location);
	if (!NavData)
	{
		return;
	}
	const auto Reachable = [&](const AActor& QueryOwner, const FVector From, const FVector To)
	{
		FPathFindingQuery Query(&QueryOwner, *NavData, From, To);
		Query.SetAllowPartialPaths(false);
		return Navigation->TestPathSync(Query);
	};
	if (!Reachable(*HeroAgent, HeroHome.Location, HeroGoal.Location)
		|| !Reachable(*RangedAgent, RangedHome.Location, TacticalGoal.Location))
	{
		if (StartupAttempts == 1 || StartupAttempts % 10 == 0)
		{
			UE_LOG(LogTemp, Warning, TEXT("AITacticsDemoStartupWait Arena=%s Reason=Unreachable Attempts=%d"),
				*GetName(), StartupAttempts);
		}
		return;
	}

	Scheduler->Cancel(StartupSchedule);
	StartupSchedule = {};
	HeroStart = HeroHome.Location;
	RangedStart = RangedHome.Location;
	FCombatAIAssignment HeroDuty;
	HeroDuty.Home = HeroHome.Location;
	FCombatAIAssignment RangedDuty;
	RangedDuty.Home = RangedHome.Location;
	HeroAgent->GetCombatAIBrainComponent()->SetAssignment(HeroDuty);
	RangedAgent->GetCombatAIBrainComponent()->SetAssignment(RangedDuty);
	HeroAgent->GetCombatAIBrainComponent()->ConfigureProfile(HeroProfile);
	RangedAgent->GetCombatAIBrainComponent()->ConfigureProfile(RangedProfile);
	UE_LOG(LogTemp, Display, TEXT("AITacticsDemoReady Hero=%s Ranged=%s HeroTarget=%s RangedTarget=%s"),
		*HeroAgent->GetName(), *RangedAgent->GetName(), *HeroTarget->GetName(), *RangedTarget->GetName());
}

void ACombatAITacticalDemoArena::OnHeroAttackLaunched(
	const FCombatAttackHandle AttackHandle, const FCombatOrderHandle OrderHandle)
{
	(void)AttackHandle;
	(void)OrderHandle;
	++HeroAttackLaunchCount;
	if (bHeroInjuryApplied || !IsValid(HeroAgent) || !IsValid(HeroTarget))
	{
		return;
	}
	const UCombatAttributeSet* Attributes = HeroAgent->GetCombatAttributeSet();
	auto* Damage = GetWorld() ? GetWorld()->GetSubsystem<UCombatDamageSubsystem>() : nullptr;
	if (!Attributes || !Damage)
	{
		return;
	}
	FCombatDamageRequest Request;
	Request.Source = HeroTarget;
	Request.Target = HeroAgent;
	Request.Amount = FMath::Max(1.0f, Attributes->GetMaxHealth() * 0.5f);
	Request.DamageType = ECombatDamageType::Pure;
	Request.Flags.AddTag(CombatTags::Damage_Flag_HPLoss);
	const FCombatDamageResult Result = Damage->DealDamage(Request);
	bHeroInjuryApplied = Result.bSuccess && Result.Event.AppliedAmount > 0.0f;
	HeroHealthAfterInjury = HeroAgent->GetCombatAttributeSet()->GetHealth();
	UE_LOG(LogTemp, Display, TEXT("AITacticsDemoHeroInjured Result=%s Applied=%.1f Health=%.1f Launches=%llu"),
		bHeroInjuryApplied ? TEXT("Pass") : TEXT("Fail"), Result.Event.AppliedAmount,
		HeroHealthAfterInjury, HeroAttackLaunchCount);
}

void ACombatAITacticalDemoArena::OnHeroOrderFinished(const FCombatOrderResult& Result)
{
	if (Result.bSuccess && Result.Type == ECombatOrderType::CastNoTarget)
	{
		++HeroCastCount;
		if (bHeroInjuryApplied)
		{
			++HeroCastAfterInjuryCount;
		}
		UE_LOG(LogTemp, Display, TEXT("AITacticsDemoHeroCast Result=Pass Count=%llu AfterInjury=%llu Health=%.1f"),
			HeroCastCount, HeroCastAfterInjuryCount,
			IsValid(HeroAgent) ? HeroAgent->GetCombatAttributeSet()->GetHealth() : 0.0f);
	}
}

void ACombatAITacticalDemoArena::ClearBindings()
{
	if (auto* Scheduler = GetWorld() ? GetWorld()->GetSubsystem<UCombatSchedulerSubsystem>() : nullptr)
	{
		Scheduler->Cancel(StartupSchedule);
	}
	StartupSchedule = {};
	if (IsValid(HeroAgent))
	{
		HeroAgent->GetCombatAttackComponent()->OnAttackLaunched().Remove(HeroAttackBinding);
		HeroAgent->GetCombatOrderComponent()->OnOrderFinished().Remove(HeroOrderBinding);
	}
	HeroAttackBinding.Reset();
	HeroOrderBinding.Reset();
}

void ACombatAITacticalDemoArena::EndPlay(const EEndPlayReason::Type Reason)
{
	ClearBindings();
	if (HasAuthority() && Reason == EEndPlayReason::Destroyed)
	{
		for (auto* Unit : { HeroAgent.Get(), RangedAgent.Get(), HeroTarget.Get(), RangedTarget.Get() })
		{
			if (IsValid(Unit))
			{
				Unit->Destroy();
			}
		}
	}
	Super::EndPlay(Reason);
}
