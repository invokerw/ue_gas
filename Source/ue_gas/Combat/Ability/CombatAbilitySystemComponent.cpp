#include "Combat/Ability/CombatAbilitySystemComponent.h"

UCombatAbilitySystemComponent::UCombatAbilitySystemComponent()
{
	SetIsReplicatedByDefault(true);
	SetReplicationMode(EGameplayEffectReplicationMode::Mixed);
}

void UCombatAbilitySystemComponent::InitializeCombatActorInfo(AActor* InOwnerActor, AActor* InAvatarActor)
{
	if (!IsValid(InOwnerActor) || !IsValid(InAvatarActor))
	{
		return;
	}
	// 相同 Owner/Avatar 的重复刷新直接返回，避免重复触发 ActorInfo 生命周期。
	if (GetOwnerActor() == InOwnerActor && GetAvatarActor() == InAvatarActor)
	{
		return;
	}
	InitAbilityActorInfo(InOwnerActor, InAvatarActor);
}

void UCombatAbilitySystemComponent::ClearCombatActorInfo()
{
	if (AbilityActorInfo.IsValid())
	{
		ClearActorInfo();
	}
}

bool UCombatAbilitySystemComponent::IsCombatActorInfoInitialized() const
{
	return AbilityActorInfo.IsValid() && AbilityActorInfo->OwnerActor.IsValid() && AbilityActorInfo->AvatarActor.IsValid();
}
