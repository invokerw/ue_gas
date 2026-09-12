#include "Combat/Unit/CombatRegenerationComponent.h"

#include "Combat/Ability/CombatAbilitySystemComponent.h"
#include "Combat/Attributes/CombatAttributeSet.h"
#include "Combat/Combat/CombatEffectUtilities.h"
#include "Combat/Combat/CombatHealSubsystem.h"
#include "Combat/Scheduling/CombatSchedulerSubsystem.h"
#include "Combat/Unit/CombatUnitCharacter.h"

UCombatRegenerationComponent::UCombatRegenerationComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UCombatRegenerationComponent::BeginPlay()
{
	Super::BeginPlay();
	StartSchedule();
}

void UCombatRegenerationComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopSchedule();
	Super::EndPlay(EndPlayReason);
}

void UCombatRegenerationComponent::HandleOwnerDeath()
{
	StopSchedule();
}

void UCombatRegenerationComponent::HandleOwnerRespawn()
{
	StartSchedule();
}

void UCombatRegenerationComponent::StartSchedule()
{
	ACombatUnitCharacter* Unit = Cast<ACombatUnitCharacter>(GetOwner());
	UCombatSchedulerSubsystem* Scheduler = GetWorld() ? GetWorld()->GetSubsystem<UCombatSchedulerSubsystem>() : nullptr;
	if (!Unit || !Unit->HasAuthority() || !Scheduler || Scheduler->IsHandleActive(RegenSchedule))
	{
		return;
	}
	RegenSchedule = Scheduler->ScheduleRepeating(
		this, RegenIntervalSeconds, RegenIntervalSeconds, 0, ECombatCatchUpPolicy::Coalesce,
		FCombatScheduledDelegate::CreateUObject(this, &UCombatRegenerationComponent::HandleRegenTick));
}

void UCombatRegenerationComponent::StopSchedule()
{
	if (UCombatSchedulerSubsystem* Scheduler = GetWorld() ? GetWorld()->GetSubsystem<UCombatSchedulerSubsystem>() : nullptr)
	{
		Scheduler->Cancel(RegenSchedule);
	}
	RegenSchedule = FCombatScheduleHandle();
}

void UCombatRegenerationComponent::HandleRegenTick(const FCombatScheduledTickContext& TickContext)
{
	ACombatUnitCharacter* Unit = Cast<ACombatUnitCharacter>(GetOwner());
	UCombatAbilitySystemComponent* Asc = Unit ? Unit->GetCombatAbilitySystemComponent() : nullptr;
	if (!Unit || !Asc || Unit->GetLifeState() != ECombatLifeState::Alive)
	{
		return;
	}
	const float Elapsed = RegenIntervalSeconds * static_cast<float>(TickContext.TickCount);
	const float HealthAmount = FMath::Max(0.0f,
		Asc->GetNumericAttribute(UCombatAttributeSet::GetHealthRegenAttribute()) * Elapsed);
	if (HealthAmount > 0.0f)
	{
		FCombatHealRequest Request;
		Request.Source = Unit;
		Request.Target = Unit;
		Request.Amount = HealthAmount;
		Request.SourceContext.DirectSourceType = ECombatDirectSourceType::Unit;
		GetWorld()->GetSubsystem<UCombatHealSubsystem>()->Heal(Request);
	}

	const float ManaAmount = FMath::Max(0.0f,
		Asc->GetNumericAttribute(UCombatAttributeSet::GetManaRegenAttribute()) * Elapsed);
	if (ManaAmount > 0.0f)
	{
		CombatEffectUtilities::ApplyAttributeAdditive(
			this, *Asc, UCombatAttributeSet::GetManaAttribute(), ManaAmount);
	}
}
