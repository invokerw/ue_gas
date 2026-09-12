#include "Combat/Unit/CombatProgressionComponent.h"

#include "Net/UnrealNetwork.h"

#include "Combat/Ability/CombatAbilitySystemComponent.h"
#include "Combat/Core/CombatNumericPolicy.h"
#include "Combat/Core/CombatTags.h"
#include "Combat/Data/CombatDefinitionData.h"
#include "Combat/Log/CombatEventSubsystem.h"
#include "Combat/Unit/CombatUnitCharacter.h"
#include "GameFramework/PlayerController.h"

UCombatProgressionComponent::UCombatProgressionComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

int64 UCombatProgressionComponent::GetExperienceForLevel(const int32 TargetLevel) const
{
	const int32 SafeMaxLevel = FMath::Max(1, MaxLevel);
	const int32 SafeLevel = FMath::Clamp(TargetLevel, 1, SafeMaxLevel);
	// 1->2 需要 200，之后每级多 100；累计公式为 100 * (n - 1) * (n + 2) / 2。
	return 100LL * (SafeLevel - 1) * (SafeLevel + 2) / 2;
}

int64 UCombatProgressionComponent::GetExperienceIntoLevel() const
{
	return FMath::Max<int64>(0, Experience - GetExperienceForLevel(Level));
}

int64 UCombatProgressionComponent::GetExperienceToNextLevel() const
{
	if (Level >= FMath::Max(1, MaxLevel))
	{
		return 0;
	}
	return FMath::Max<int64>(0, GetExperienceForLevel(Level + 1) - Experience);
}

float UCombatProgressionComponent::GetExperienceProgress() const
{
	if (Level >= FMath::Max(1, MaxLevel))
	{
		return 1.0f;
	}
	const int64 LevelStart = GetExperienceForLevel(Level);
	const int64 LevelEnd = GetExperienceForLevel(Level + 1);
	const int64 Span = LevelEnd - LevelStart;
	return Span > 0 ? FMath::Clamp(static_cast<float>(Experience - LevelStart) / static_cast<float>(Span), 0.0f, 1.0f) : 0.0f;
}

bool UCombatProgressionComponent::InitializeProgression(
	const int32 InitialLevel,
	const int64 InitialExperienceIntoLevel)
{
	ACombatUnitCharacter* Unit = GetOwnerUnit();
	if (bInitialized || !Unit || !Unit->HasAuthority() || MaxLevel < 1 || InitialLevel < 1 || InitialLevel > MaxLevel
		|| InitialExperienceIntoLevel < 0)
	{
		return false;
	}

	Level = InitialLevel;
	Experience = GetExperienceForLevel(Level) + InitialExperienceIntoLevel;
	// 若配置的等级内经验已经跨过下一级，按实际累计经验归一化并补发技能点。
	const int64 MaxExperience = GetExperienceForLevel(MaxLevel);
	Experience = FMath::Clamp<int64>(Experience, 0, MaxExperience);
	while (Level < MaxLevel && Experience >= GetExperienceForLevel(Level + 1))
	{
		++Level;
	}
	UnspentAbilityPoints = 0;
	bInitialized = true;
	ProgressionChangedDelegate.Broadcast();
	Unit->ForceNetUpdate();
	return true;
}

bool UCombatProgressionComponent::AddExperience(const int32 Amount)
{
	ACombatUnitCharacter* Unit = GetOwnerUnit();
	if (!Unit || !Unit->HasAuthority() || !bInitialized || Amount <= 0
		|| !FCombatNumericPolicyV1::IsValidNonNegativeRequest(static_cast<float>(Amount)))
	{
		return false;
	}

	const int32 OldLevel = Level;
	const int64 OldExperience = Experience;
	const int64 MaxExperience = GetExperienceForLevel(MaxLevel);
	Experience = FMath::Min<int64>(MaxExperience, Experience + static_cast<int64>(Amount));
	while (Level < MaxLevel && Experience >= GetExperienceForLevel(Level + 1))
	{
		++Level;
	}
	const int32 LevelUps = FMath::Max(0, Level - OldLevel);
	UnspentAbilityPoints += LevelUps;

	if (Experience == OldExperience)
	{
		return false;
	}
	ProgressionChangedDelegate.Broadcast();
	EmitProgressionEvent(CombatTags::Event_Combat_ExperienceGained, static_cast<float>(Amount),
		static_cast<float>(Experience - OldExperience), FString::Printf(TEXT("Level=%d Experience=%lld"), Level, static_cast<long long>(Experience)));
	if (LevelUps > 0)
	{
		EmitProgressionEvent(CombatTags::Event_Combat_LevelUp, static_cast<float>(OldLevel), static_cast<float>(Level),
			FString::Printf(TEXT("Level %d -> %d AbilityPoints=%d"), OldLevel, Level, UnspentAbilityPoints));
	}
	Unit->ForceNetUpdate();
	return true;
}

bool UCombatProgressionComponent::UpgradeAbility(
	const FGameplayAbilitySpecHandle AbilityHandle,
	FGameplayTag& OutFailureTag)
{
	OutFailureTag = FGameplayTag();
	ACombatUnitCharacter* Unit = GetOwnerUnit();
	if (!Unit || !Unit->HasAuthority())
	{
		OutFailureTag = CombatTags::Failure_Authority;
		return false;
	}
	if (!bInitialized)
	{
		OutFailureTag = CombatTags::Failure_Progression_NotInitialized;
		return false;
	}
	if (Unit->GetLifeState() != ECombatLifeState::Alive)
	{
		OutFailureTag = CombatTags::Failure_Life_NotAlive;
		return false;
	}
	if (UnspentAbilityPoints <= 0)
	{
		OutFailureTag = CombatTags::Failure_Progression_NoAbilityPoints;
		return false;
	}

	UCombatAbilitySystemComponent* Asc = Unit->GetCombatAbilitySystemComponent();
	FGameplayAbilitySpec* Spec = Asc ? Asc->FindAbilitySpecFromHandle(AbilityHandle) : nullptr;
	const UCombatAbilityData* AbilityData = Asc ? Asc->GetCombatAbilityData(AbilityHandle) : nullptr;
	if (!Spec || !AbilityData)
	{
		OutFailureTag = CombatTags::Failure_Ability_NotGranted;
		return false;
	}
	if (Spec->Level >= AbilityData->MaxLevel)
	{
		OutFailureTag = CombatTags::Failure_Ability_InvalidLevel;
		return false;
	}
	if (Spec->Level >= Level)
	{
		OutFailureTag = CombatTags::Failure_Progression_HeroLevelRequired;
		return false;
	}

	const int32 PreviousLevel = Spec->Level;
	if (!Asc->SetCombatAbilityLevel(AbilityHandle, PreviousLevel + 1, OutFailureTag))
	{
		return false;
	}
	--UnspentAbilityPoints;
	ProgressionChangedDelegate.Broadcast();
	EmitProgressionEvent(CombatTags::Event_Combat_AbilityPointSpent, static_cast<float>(PreviousLevel),
		static_cast<float>(PreviousLevel + 1), FString::Printf(TEXT("Ability=%s Points=%d"),
			*AbilityData->GetPrimaryAssetId().ToString(), UnspentAbilityPoints));
	Unit->ForceNetUpdate();
	return true;
}

bool UCombatProgressionComponent::RequestAbilityUpgrade(const FGameplayAbilitySpecHandle AbilityHandle)
{
	if (ACombatUnitCharacter* Unit = GetOwnerUnit())
	{
		if (Unit->HasAuthority())
		{
			FGameplayTag Failure;
			return UpgradeAbility(AbilityHandle, Failure);
		}
		if (Unit->GetCommandingPlayerController() && Unit->GetCommandingPlayerController()->IsLocalController())
		{
			ServerUpgradeAbility(AbilityHandle);
			return true;
		}
	}
	return false;
}

void UCombatProgressionComponent::ServerUpgradeAbility_Implementation(const FGameplayAbilitySpecHandle AbilityHandle)
{
	FGameplayTag Failure;
	UpgradeAbility(AbilityHandle, Failure);
}

void UCombatProgressionComponent::OnRep_Progression()
{
	ProgressionChangedDelegate.Broadcast();
}

void UCombatProgressionComponent::BeginPlay()
{
	Super::BeginPlay();
	MaxLevel = FMath::Max(1, MaxLevel);
}

void UCombatProgressionComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ProgressionChangedDelegate.Clear();
	Super::EndPlay(EndPlayReason);
}

void UCombatProgressionComponent::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UCombatProgressionComponent, Level);
	DOREPLIFETIME(UCombatProgressionComponent, Experience);
	DOREPLIFETIME(UCombatProgressionComponent, UnspentAbilityPoints);
}

ACombatUnitCharacter* UCombatProgressionComponent::GetOwnerUnit() const
{
	return Cast<ACombatUnitCharacter>(GetOwner());
}

void UCombatProgressionComponent::EmitProgressionEvent(
	const FGameplayTag& EventType,
	const float RequestedAmount,
	const float AppliedAmount,
	const FString& Diagnostic) const
{
	ACombatUnitCharacter* Unit = GetOwnerUnit();
	UCombatEventSubsystem* Events = Unit && Unit->GetWorld()
		? Unit->GetWorld()->GetSubsystem<UCombatEventSubsystem>() : nullptr;
	if (!Events || !Unit || !EventType.IsValid())
	{
		return;
	}
	const FCombatEventContext Context = Events->CreateRootEvent();
	if (!Context.IsValid())
	{
		return;
	}
	FCombatLogRecord Record;
	Record.Context = Context;
	Record.EventType = EventType;
	Record.SourceActorId = Unit->GetUniqueID();
	Record.TargetActorId = Unit->GetUniqueID();
	Record.UnitLifeGeneration = Unit->GetLifeGeneration();
	Record.RequestedAmount = RequestedAmount;
	Record.AppliedAmount = AppliedAmount;
	Record.Diagnostic = Diagnostic;
	Events->Emit(Record);
}
