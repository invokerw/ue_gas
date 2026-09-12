#include "Combat/Demo/CombatFissureBlocker.h"

#include "Components/BoxComponent.h"
#include "EngineUtils.h"

#include "Combat/Core/CombatTags.h"
#include "Combat/Order/CombatOrderComponent.h"
#include "Combat/Unit/CombatUnitCharacter.h"

ACombatFissureBlocker::ACombatFissureBlocker()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicateMovement(true);
	BlockerCollision = CreateDefaultSubobject<UBoxComponent>(TEXT("CombatBlocker"));
	SetRootComponent(BlockerCollision);
	BlockerCollision->SetCollisionProfileName(TEXT("CombatBlocker"));
	BlockerCollision->SetGenerateOverlapEvents(false);
}

bool ACombatFissureBlocker::InitializeBlocker(
	const FVector Start,
	const FVector End,
	const float HalfWidth,
	const float Height,
	const float Duration,
	const FCombatEventContext& ParentEvent,
	const FCombatSourceContext& SourceContext)
{
	if (!HasAuthority() || bInitialized || Start.ContainsNaN() || End.ContainsNaN()
		|| !FMath::IsFinite(HalfWidth) || HalfWidth <= 0.0f
		|| !FMath::IsFinite(Height) || Height <= 0.0f
		|| !FMath::IsFinite(Duration) || Duration <= 0.0f)
	{
		return false;
	}
	const FVector Direction = (End - Start).GetSafeNormal2D();
	const float Length = FVector::Dist2D(Start, End);
	if (Direction.IsNearlyZero() || Length <= KINDA_SMALL_NUMBER)
	{
		return false;
	}
	// 位置和朝向已由权威 SpawnActor 参数冻结；初始化阶段只设置碰撞体尺寸与生命周期。
	BlockerCollision->SetBoxExtent(FVector(Length * 0.5f, HalfWidth, Height * 0.5f), true);
	BlockerEvent = ParentEvent;
	BlockerSource = SourceContext;
	bInitialized = true;
	if (UCombatSchedulerSubsystem* Scheduler = GetWorld()->GetSubsystem<UCombatSchedulerSubsystem>())
	{
		LifetimeSchedule = Scheduler->ScheduleOnce(
			this, Duration, 0,
			FCombatScheduledDelegate::CreateWeakLambda(this,
				[this](const FCombatScheduledTickContext& Context) { HandleLifetimeExpired(Context); }));
	}
	if (!LifetimeSchedule.IsValid())
	{
		bInitialized = false;
		return false;
	}
	EmitBlockerLog(true);
	NotifyAffectedOrders();
	ForceNetUpdate();
	return true;
}

FBox ACombatFissureBlocker::GetBlockerBounds() const
{
	return BlockerCollision ? BlockerCollision->Bounds.GetBox() : FBox(ForceInit);
}

void ACombatFissureBlocker::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UCombatSchedulerSubsystem* Scheduler = GetWorld() ? GetWorld()->GetSubsystem<UCombatSchedulerSubsystem>() : nullptr)
	{
		Scheduler->Cancel(LifetimeSchedule);
	}
	LifetimeSchedule = FCombatScheduleHandle();
	if (bInitialized && HasAuthority())
	{
		NotifyAffectedOrders();
		EmitBlockerLog(false);
		bInitialized = false;
	}
	Super::EndPlay(EndPlayReason);
}

void ACombatFissureBlocker::HandleLifetimeExpired(const FCombatScheduledTickContext& TickContext)
{
	(void)TickContext;
	LifetimeSchedule = FCombatScheduleHandle();
	Destroy();
}

void ACombatFissureBlocker::NotifyAffectedOrders() const
{
	if (!GetWorld())
	{
		return;
	}
	const FBox Bounds = GetBlockerBounds();
	for (TActorIterator<ACombatUnitCharacter> It(GetWorld()); It; ++It)
	{
		if (ACombatUnitCharacter* Unit = *It)
		{
			Unit->GetCombatOrderComponent()->HandleGameplayBlockerChanged(Bounds);
		}
	}
}

void ACombatFissureBlocker::EmitBlockerLog(const bool bCreated) const
{
	UCombatEventSubsystem* Events = GetWorld() ? GetWorld()->GetSubsystem<UCombatEventSubsystem>() : nullptr;
	if (!Events)
	{
		return;
	}
	FCombatLogRecord Record;
	Record.Context = BlockerEvent.IsValid() ? BlockerEvent : Events->CreateRootEvent();
	Record.EventType = CombatTags::Event_Combat_BlockerChanged;
	Record.Source = BlockerSource;
	Record.SourceActorId = GetOwner() ? GetOwner()->GetUniqueID() : 0;
	Record.TargetActorId = GetUniqueID();
	Record.AppliedAmount = bCreated ? 1.0f : 0.0f;
	Record.Diagnostic = FString::Printf(TEXT("FissureBlocker=%s State=%s Bounds=%s"),
		*GetName(), bCreated ? TEXT("Created") : TEXT("Removed"), *GetBlockerBounds().ToString());
	Events->Emit(Record);
}
