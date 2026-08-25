#include "Combat/Unit/CombatUnitCharacter.h"

#include "Combat/Ability/CombatAbilitySystemComponent.h"
#include "Combat/Core/CombatTags.h"
#include "Components/CapsuleComponent.h"
#include "Net/UnrealNetwork.h"

ACombatUnitCharacter::ACombatUnitCharacter()
{
	bReplicates = true;
	SetReplicateMovement(true);

	CombatAbilitySystemComponent = CreateDefaultSubobject<UCombatAbilitySystemComponent>(TEXT("CombatAbilitySystem"));
	GetCapsuleComponent()->SetCollisionProfileName(TEXT("CombatUnit"));
}

UAbilitySystemComponent* ACombatUnitCharacter::GetAbilitySystemComponent() const
{
	return CombatAbilitySystemComponent;
}

bool ACombatUnitCharacter::SetCombatTeamId(const FCombatTeamId NewTeamId)
{
	if (!HasAuthority() || !NewTeamId.IsValid() || TeamId == NewTeamId)
	{
		return false;
	}
	TeamId = NewTeamId;
	OnRep_TeamId();
	ForceNetUpdate();
	return true;
}

void ACombatUnitCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ACombatUnitCharacter, TeamId);
	DOREPLIFETIME(ACombatUnitCharacter, LifeState);
	DOREPLIFETIME(ACombatUnitCharacter, LifeGeneration);
}

void ACombatUnitCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	RefreshAbilityActorInfo();
}

void ACombatUnitCharacter::UnPossessed()
{
	Super::UnPossessed();
	RefreshAbilityActorInfo();
}

void ACombatUnitCharacter::BeginPlay()
{
	Super::BeginPlay();
	RefreshAbilityActorInfo();
	RefreshLifeStateTag();
}

void ACombatUnitCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (CombatAbilitySystemComponent)
	{
		CombatAbilitySystemComponent->ClearCombatActorInfo();
	}
	Super::EndPlay(EndPlayReason);
}

void ACombatUnitCharacter::OnRep_Owner()
{
	Super::OnRep_Owner();
	RefreshAbilityActorInfo();
}

void ACombatUnitCharacter::OnRep_Controller()
{
	Super::OnRep_Controller();
	RefreshAbilityActorInfo();
}

void ACombatUnitCharacter::OnRep_TeamId()
{
	// Team-dependent systems subscribe here in M3. The replicated value is already authoritative.
}

void ACombatUnitCharacter::OnRep_LifeState()
{
	RefreshLifeStateTag();
}

void ACombatUnitCharacter::RefreshAbilityActorInfo()
{
	// Character 自持 ASC，Owner 与 Avatar 都使用 Unit，可在四种 NetMode 下保持同一复制契约。
	if (CombatAbilitySystemComponent)
	{
		CombatAbilitySystemComponent->InitializeCombatActorInfo(this, this);
	}
}

void ACombatUnitCharacter::RefreshLifeStateTag()
{
	if (!CombatAbilitySystemComponent)
	{
		return;
	}

	// 先移除完整互斥集合，再添加当前状态，避免复制回调留下多个生命标签。
	CombatAbilitySystemComponent->RemoveLooseGameplayTag(CombatTags::State_Alive);
	CombatAbilitySystemComponent->RemoveLooseGameplayTag(CombatTags::State_Dying);
	CombatAbilitySystemComponent->RemoveLooseGameplayTag(CombatTags::State_Dead);
	CombatAbilitySystemComponent->RemoveLooseGameplayTag(CombatTags::State_Respawning);

	switch (LifeState)
	{
	case ECombatLifeState::Alive: CombatAbilitySystemComponent->AddLooseGameplayTag(CombatTags::State_Alive); break;
	case ECombatLifeState::Dying: CombatAbilitySystemComponent->AddLooseGameplayTag(CombatTags::State_Dying); break;
	case ECombatLifeState::Dead: CombatAbilitySystemComponent->AddLooseGameplayTag(CombatTags::State_Dead); break;
	case ECombatLifeState::Respawning: CombatAbilitySystemComponent->AddLooseGameplayTag(CombatTags::State_Respawning); break;
	default: break;
	}
}
