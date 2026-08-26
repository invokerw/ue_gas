#include "Combat/Unit/CombatUnitLifecycleComponent.h"

#include "GameFramework/CharacterMovementComponent.h"

#include "Combat/Ability/CombatAbilitySystemComponent.h"
#include "Combat/Attack/CombatAttackComponent.h"
#include "Combat/Attributes/CombatAttributeSet.h"
#include "Combat/Core/CombatTags.h"
#include "Combat/Modifiers/CombatModifierComponent.h"
#include "Combat/Order/CombatOrderComponent.h"
#include "Combat/Scheduling/CombatSchedulerSubsystem.h"
#include "Combat/Unit/CombatRegenerationComponent.h"
#include "Combat/Unit/CombatUnitCharacter.h"
#include "Components/CapsuleComponent.h"

UCombatUnitLifecycleComponent::UCombatUnitLifecycleComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

bool UCombatUnitLifecycleComponent::RequestDeath(
	const FCombatEventContext& CauseEvent,
	ACombatUnitCharacter* Killer)
{
	ACombatUnitCharacter* Unit = GetOwnerUnit();
	if (!Unit || !Unit->HasAuthority() || Unit->GetLifeState() != ECombatLifeState::Alive)
	{
		return false;
	}
	const FCombatEventContext Context = CauseEvent.IsValid() ? CauseEvent : CreateRootEvent();
	if (!Context.IsValid())
	{
		return false;
	}

	// Dying 是同步清理屏障：先阻止新玩法，再取消所有本生命代次的活动行为。
	Unit->SetLifeStateFromLifecycle(ECombatLifeState::Dying);
	// Order 先提升 generation 并清空 Ability/Attack 观察状态，使随后取消产生的回调只能成为旧回调。
	if (UCombatOrderComponent* Orders = Unit->GetCombatOrderComponent())
	{
		Orders->HandleOwnerDeath();
	}
	if (UCombatAbilitySystemComponent* Asc = Unit->GetCombatAbilitySystemComponent())
	{
		Asc->CancelAllAbilities();
	}
	if (UCombatAttackComponent* Attacks = Unit->GetCombatAttackComponent())
	{
		Attacks->HandleOwnerDeath();
	}
	if (UCharacterMovementComponent* Movement = Unit->GetCharacterMovement())
	{
		Movement->StopMovementImmediately();
		Movement->DisableMovement();
	}
	if (UCombatSchedulerSubsystem* Scheduler = GetWorld()->GetSubsystem<UCombatSchedulerSubsystem>())
	{
		Scheduler->CancelAllForOwner(Unit);
	}
	if (UCombatRegenerationComponent* Regen = Unit->GetCombatRegenerationComponent())
	{
		Regen->HandleOwnerDeath();
	}
	if (UCombatModifierComponent* Modifiers = Unit->GetCombatModifierComponent())
	{
		Modifiers->HandleOwnerDeath();
	}
	Unit->GetCapsuleComponent()->SetCollisionProfileName(TEXT("CombatCorpse"));

	Unit->SetLifeStateFromLifecycle(ECombatLifeState::Dead);
	EmitLifecycleLog(Context, false, Killer);
	DiedDelegate.Broadcast(Unit, Context);
	Unit->ForceNetUpdate();
	return true;
}

bool UCombatUnitLifecycleComponent::RespawnAtLocation(const FVector NewLocation)
{
	ACombatUnitCharacter* Unit = GetOwnerUnit();
	if (!Unit || !Unit->HasAuthority() || Unit->GetLifeState() != ECombatLifeState::Dead
		|| NewLocation.ContainsNaN() || Unit->GetLifeGeneration() == MAX_uint32)
	{
		return false;
	}
	const FCombatEventContext Context = CreateRootEvent();
	if (!Context.IsValid())
	{
		return false;
	}

	Unit->SetLifeStateFromLifecycle(ECombatLifeState::Respawning);
	if (!Unit->SetActorLocation(NewLocation, false, nullptr, ETeleportType::TeleportPhysics))
	{
		Unit->SetLifeStateFromLifecycle(ECombatLifeState::Dead);
		return false;
	}
	Unit->IncrementLifeGenerationFromLifecycle();
	if (UCombatAbilitySystemComponent* Asc = Unit->GetCombatAbilitySystemComponent())
	{
		const float MaxHealth = Asc->GetNumericAttribute(UCombatAttributeSet::GetMaxHealthAttribute());
		const float MaxMana = Asc->GetNumericAttribute(UCombatAttributeSet::GetMaxManaAttribute());
		Asc->SetNumericAttributeBase(UCombatAttributeSet::GetHealthAttribute(), MaxHealth);
		Asc->SetNumericAttributeBase(UCombatAttributeSet::GetManaAttribute(), MaxMana);
	}
	Unit->GetCapsuleComponent()->SetCollisionProfileName(TEXT("CombatUnit"));
	if (UCharacterMovementComponent* Movement = Unit->GetCharacterMovement())
	{
		Movement->SetMovementMode(MOVE_Walking);
	}
	if (UCombatModifierComponent* Modifiers = Unit->GetCombatModifierComponent())
	{
		Modifiers->HandleOwnerRespawn();
	}
	if (UCombatAttackComponent* Attacks = Unit->GetCombatAttackComponent())
	{
		Attacks->HandleOwnerRespawn();
	}
	if (UCombatOrderComponent* Orders = Unit->GetCombatOrderComponent())
	{
		Orders->HandleOwnerRespawn();
	}
	Unit->SetLifeStateFromLifecycle(ECombatLifeState::Alive);
	if (UCombatAbilitySystemComponent* Asc = Unit->GetCombatAbilitySystemComponent())
	{
		Asc->ReconcileIntrinsicModifiers();
	}
	if (UCombatRegenerationComponent* Regen = Unit->GetCombatRegenerationComponent())
	{
		Regen->HandleOwnerRespawn();
	}

	EmitLifecycleLog(Context, true, nullptr);
	RespawnedDelegate.Broadcast(Unit, Context);
	Unit->ForceNetUpdate();
	return true;
}

ACombatUnitCharacter* UCombatUnitLifecycleComponent::GetOwnerUnit() const
{
	return Cast<ACombatUnitCharacter>(GetOwner());
}

FCombatEventContext UCombatUnitLifecycleComponent::CreateRootEvent() const
{
	if (UCombatEventSubsystem* Events = GetWorld() ? GetWorld()->GetSubsystem<UCombatEventSubsystem>() : nullptr)
	{
		return Events->CreateRootEvent();
	}
	return FCombatEventContext();
}

void UCombatUnitLifecycleComponent::EmitLifecycleLog(
	const FCombatEventContext& Context,
	const bool bRespawn,
	ACombatUnitCharacter* OtherUnit) const
{
	UCombatEventSubsystem* Events = GetWorld() ? GetWorld()->GetSubsystem<UCombatEventSubsystem>() : nullptr;
	ACombatUnitCharacter* Unit = GetOwnerUnit();
	if (!Events || !Unit)
	{
		return;
	}
	FCombatLogRecord Record;
	Record.Context = Context;
	Record.EventType = bRespawn ? CombatTags::Event_Combat_UnitRespawned : CombatTags::Event_Combat_UnitDeath;
	Record.SourceActorId = OtherUnit ? OtherUnit->GetUniqueID() : Unit->GetUniqueID();
	Record.TargetActorId = Unit->GetUniqueID();
	Record.UnitLifeGeneration = Unit->GetLifeGeneration();
	Record.Diagnostic = bRespawn ? TEXT("Respawned") : TEXT("Died");
	Events->Emit(Record);
}
