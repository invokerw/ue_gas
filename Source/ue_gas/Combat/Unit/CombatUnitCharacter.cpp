#include "Combat/Unit/CombatUnitCharacter.h"

#include "Combat/Ability/CombatAbilitySystemComponent.h"
#include "Combat/Ability/CombatGameplayAbility.h"
#include "Combat/Attack/CombatAttackComponent.h"
#include "Combat/Attributes/CombatAttributeSet.h"
#include "Combat/Aura/CombatAuraSubsystem.h"
#include "Combat/Combat/CombatEffectUtilities.h"
#include "Combat/Core/CombatTags.h"
#include "Combat/Data/CombatDefinitionData.h"
#include "Combat/Log/CombatEventSubsystem.h"
#include "Combat/Modifiers/CombatModifierComponent.h"
#include "Combat/Motion/CombatMotionComponent.h"
#include "Combat/Order/CombatOrderComponent.h"
#include "Combat/Unit/CombatRegenerationComponent.h"
#include "Combat/Unit/CombatUnitLifecycleComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameplayEffectTypes.h"
#include "Net/UnrealNetwork.h"

ACombatUnitCharacter::ACombatUnitCharacter()
{
	bReplicates = true;
	SetReplicateMovement(true);

	CombatAbilitySystemComponent = CreateDefaultSubobject<UCombatAbilitySystemComponent>(TEXT("CombatAbilitySystem"));
	CombatAttributeSet = CreateDefaultSubobject<UCombatAttributeSet>(TEXT("CombatAttributes"));
	CombatAbilitySystemComponent->AddAttributeSetSubobject(CombatAttributeSet.Get());
	CombatModifierComponent = CreateDefaultSubobject<UCombatModifierComponent>(TEXT("CombatModifiers"));
	CombatLifecycleComponent = CreateDefaultSubobject<UCombatUnitLifecycleComponent>(TEXT("CombatLifecycle"));
	CombatRegenerationComponent = CreateDefaultSubobject<UCombatRegenerationComponent>(TEXT("CombatRegeneration"));
	CombatAttackComponent = CreateDefaultSubobject<UCombatAttackComponent>(TEXT("CombatAttack"));
	CombatOrderComponent = CreateDefaultSubobject<UCombatOrderComponent>(TEXT("CombatOrders"));
	CombatMotionComponent = CreateDefaultSubobject<UCombatMotionComponent>(TEXT("CombatMotion"));
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
	GetCapsuleComponent()->SetCollisionProfileName(TEXT("CombatUnit"));
	// Strategy 单位的 AI 路径使用输入加速度驱动，确保 PathFollowing 通过 PawnMovement 正式消费移动请求。
	GetCharacterMovement()->GetNavMovementProperties()->bUseAccelerationForPaths = true;
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
	const FCombatTeamId PreviousTeamId = TeamId;
	TeamId = NewTeamId;
	OnRep_TeamId(PreviousTeamId);
	ForceNetUpdate();
	return true;
}

bool ACombatUnitCharacter::InitializeFromUnitData(UCombatUnitData* InUnitData)
{
	if (!HasAuthority() || !InUnitData || !InUnitData->GetPrimaryAssetId().IsValid()
		|| !UCombatDefinitionData::IsValidDefinitionName(InUnitData->DefinitionName))
	{
		return false;
	}
	const FPrimaryAssetId RequestedId = InUnitData->GetPrimaryAssetId();
	if (InitializedUnitDefinitionId.IsValid())
	{
		return InitializedUnitDefinitionId == RequestedId;
	}
	FString StatsDiagnostic;
	if (!InUnitData->BaseStats.IsValid(&StatsDiagnostic) || !InUnitData->InitialTeamId.IsValid()
		|| !FMath::IsFinite(InUnitData->BaseAttackPoint) || InUnitData->BaseAttackPoint < 0.0f
		|| !FMath::IsFinite(InUnitData->AttackFacingToleranceDegrees)
		|| InUnitData->AttackFacingToleranceDegrees < 0.0f || InUnitData->AttackFacingToleranceDegrees > 180.0f
		|| !FMath::IsFinite(InUnitData->CriticalStrikeChance)
		|| InUnitData->CriticalStrikeChance < 0.0f || InUnitData->CriticalStrikeChance > 1.0f
		|| !FMath::IsFinite(InUnitData->CriticalStrikeMultiplier) || InUnitData->CriticalStrikeMultiplier < 1.0f)
	{
		return false;
	}

	// 先加载并完整校验 AbilitySet，避免属性已写入后才发现授予表非法。
	TArray<UCombatAbilitySet*> LoadedSets;
	TSet<UClass*> PendingAbilityClasses;
	TSet<FPrimaryAssetId> PendingAbilityDefinitions;
	for (const TSoftObjectPtr<UCombatAbilitySet>& SetReference : InUnitData->AbilitySets)
	{
		UCombatAbilitySet* AbilitySet = SetReference.LoadSynchronous();
		if (!AbilitySet)
		{
			return false;
		}
		LoadedSets.Add(AbilitySet);
		for (const FCombatAbilitySetEntry& Entry : AbilitySet->Abilities)
		{
			UClass* AbilityClass = Entry.AbilityClass.Get();
			const UCombatGameplayAbility* AbilityCdo = AbilityClass
				? Cast<UCombatGameplayAbility>(AbilityClass->GetDefaultObject()) : nullptr;
			const UCombatAbilityData* AbilityData = AbilityCdo ? AbilityCdo->GetAbilityData() : nullptr;
			FString AbilityDiagnostic;
			if (!AbilityData || !AbilityData->ValidateRuntime(AbilityDiagnostic)
				|| Entry.InitialLevel < 1 || Entry.InitialLevel > AbilityData->MaxLevel
				|| PendingAbilityClasses.Contains(AbilityClass)
				|| PendingAbilityDefinitions.Contains(AbilityData->GetPrimaryAssetId())
				|| CombatAbilitySystemComponent->FindCombatAbilitySpecByDefinitionId(AbilityData->GetPrimaryAssetId())
				|| (Entry.bAutoCastEnabled
					&& !AbilityData->BehaviorTags.HasTagExact(CombatTags::Ability_Behavior_AutoCast)))
			{
				return false;
			}
			PendingAbilityClasses.Add(AbilityClass);
			PendingAbilityDefinitions.Add(AbilityData->GetPrimaryAssetId());
		}
	}

	UnitData = InUnitData;
	if (TeamId != InUnitData->InitialTeamId)
	{
		SetCombatTeamId(InUnitData->InitialTeamId);
	}
	if (InUnitData->CapsuleRadiusOverride > 0.0f)
	{
		GetCapsuleComponent()->SetCapsuleRadius(InUnitData->CapsuleRadiusOverride);
	}
	const FCombatUnitBaseStats& Stats = InUnitData->BaseStats;
	// Max 属性排在 Current 属性之前，使同一 Instant 初始化 GE 按新上限完成 clamp。
	const TArray<TPair<FGameplayAttribute, float>> InitialAttributes = {
		{ UCombatAttributeSet::GetMaxHealthAttribute(), Stats.MaxHealth },
		{ UCombatAttributeSet::GetHealthAttribute(), Stats.MaxHealth },
		{ UCombatAttributeSet::GetMaxManaAttribute(), Stats.MaxMana },
		{ UCombatAttributeSet::GetManaAttribute(), Stats.MaxMana },
		{ UCombatAttributeSet::GetArmorAttribute(), Stats.Armor },
		{ UCombatAttributeSet::GetMagicResistAttribute(), Stats.MagicResist },
		{ UCombatAttributeSet::GetEvasionAttribute(), Stats.Evasion },
		{ UCombatAttributeSet::GetAttackDamageAttribute(), Stats.AttackDamage },
		{ UCombatAttributeSet::GetAttackSpeedAttribute(), Stats.AttackSpeed },
		{ UCombatAttributeSet::GetBaseAttackTimeAttribute(), Stats.BaseAttackTime },
		{ UCombatAttributeSet::GetAttackRangeAttribute(), Stats.AttackRange },
		{ UCombatAttributeSet::GetMoveSpeedAttribute(), Stats.MoveSpeed },
		{ UCombatAttributeSet::GetHealthRegenAttribute(), Stats.HealthRegen },
		{ UCombatAttributeSet::GetManaRegenAttribute(), Stats.ManaRegen },
		{ UCombatAttributeSet::GetLifestealPctAttribute(), Stats.LifestealPct },
		{ UCombatAttributeSet::GetSpellAmplifyPctAttribute(), Stats.SpellAmplifyPct },
		{ UCombatAttributeSet::GetCooldownReductionPctAttribute(), Stats.CooldownReductionPct },
		{ UCombatAttributeSet::GetCastRangeBonusAttribute(), Stats.CastRangeBonus },
		{ UCombatAttributeSet::GetStatusResistancePctAttribute(), Stats.StatusResistancePct },
		{ UCombatAttributeSet::GetHealAmplifyPctAttribute(), Stats.HealAmplifyPct },
		{ UCombatAttributeSet::GetHealReceivedPctAttribute(), Stats.HealReceivedPct }
	};
	if (!CombatEffectUtilities::ApplyAttributeOverrides(this, *CombatAbilitySystemComponent, InitialAttributes))
	{
		return false;
	}

	for (const UCombatAbilitySet* AbilitySet : LoadedSets)
	{
		for (const FCombatAbilitySetEntry& Entry : AbilitySet->Abilities)
		{
			FGameplayAbilitySpecHandle Handle;
			FGameplayTag FailureTag;
			if (!CombatAbilitySystemComponent->GrantCombatAbility(
				Entry.AbilityClass, Entry.InitialLevel, Entry.bAutoCastEnabled, Handle, FailureTag))
			{
				return false;
			}
		}
	}
	InitializedUnitDefinitionId = RequestedId;
	// 组件 BeginPlay 可能早于运行时 UnitData 注入；这里幂等确保恢复任务已按新属性建立。
	if (CombatRegenerationComponent)
	{
		CombatRegenerationComponent->HandleOwnerRespawn();
	}
	RefreshStatusResponse();
	return true;
}

bool ACombatUnitCharacter::IsMovementBlocked() const
{
	return LifeState != ECombatLifeState::Alive || !CombatAbilitySystemComponent
		|| CombatAbilitySystemComponent->HasMatchingGameplayTag(CombatTags::State_Stunned)
		|| CombatAbilitySystemComponent->HasMatchingGameplayTag(CombatTags::State_Rooted)
		|| CombatAbilitySystemComponent->HasMatchingGameplayTag(CombatTags::State_Hexed)
		|| CombatAbilitySystemComponent->HasMatchingGameplayTag(CombatTags::State_Frozen);
}

bool ACombatUnitCharacter::IsAttackBlocked() const
{
	return IsMovementBlocked() || CombatAbilitySystemComponent->HasMatchingGameplayTag(CombatTags::State_Disarmed);
}

bool ACombatUnitCharacter::IsAbilityBlocked() const
{
	return LifeState != ECombatLifeState::Alive || !CombatAbilitySystemComponent
		|| CombatAbilitySystemComponent->HasMatchingGameplayTag(CombatTags::State_Stunned)
		|| CombatAbilitySystemComponent->HasMatchingGameplayTag(CombatTags::State_Silenced)
		|| CombatAbilitySystemComponent->HasMatchingGameplayTag(CombatTags::State_Hexed)
		|| CombatAbilitySystemComponent->HasMatchingGameplayTag(CombatTags::State_Frozen);
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

void ACombatUnitCharacter::NotifyControllerChanged()
{
	Super::NotifyControllerChanged();
	if (CombatOrderComponent)
	{
		CombatOrderComponent->RefreshControllerBinding();
	}
}

void ACombatUnitCharacter::BeginPlay()
{
	Super::BeginPlay();
	RefreshAbilityActorInfo();
	RefreshLifeStateTag();
	if (CombatAbilitySystemComponent)
	{
		CombatAbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(UCombatAttributeSet::GetMoveSpeedAttribute())
			.AddUObject(this, &ACombatUnitCharacter::HandleMoveSpeedChanged);
	}
	if (HasAuthority() && UnitData)
	{
		InitializeFromUnitData(UnitData);
	}
	RefreshStatusResponse();
}

void ACombatUnitCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (HasAuthority())
	{
		if (UCombatAuraSubsystem* Auras = GetWorld() ? GetWorld()->GetSubsystem<UCombatAuraSubsystem>() : nullptr)
		{
			Auras->NotifyUnitEndPlay(this);
		}
	}
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

void ACombatUnitCharacter::OnRep_TeamId(const FCombatTeamId PreviousTeamId)
{
	if (!HasAuthority())
	{
		return;
	}
	if (UCombatAuraSubsystem* Auras = GetWorld() ? GetWorld()->GetSubsystem<UCombatAuraSubsystem>() : nullptr)
	{
		Auras->NotifyUnitChanged(this);
	}
	if (UCombatEventSubsystem* Events = GetWorld() ? GetWorld()->GetSubsystem<UCombatEventSubsystem>() : nullptr)
	{
		FCombatLogRecord Record;
		Record.Context = Events->CreateRootEvent();
		Record.EventType = CombatTags::Event_Combat_TeamChanged;
		Record.SourceActorId = GetUniqueID();
		Record.TargetActorId = GetUniqueID();
		Record.UnitLifeGeneration = LifeGeneration;
		Record.RequestedAmount = static_cast<float>(PreviousTeamId.Value);
		Record.AppliedAmount = static_cast<float>(TeamId.Value);
		Record.Diagnostic = FString::Printf(TEXT("Team %s -> %s"),
			*PreviousTeamId.ToString(), *TeamId.ToString());
		Events->Emit(Record);
	}
}

void ACombatUnitCharacter::OnRep_LifeState()
{
	RefreshLifeStateTag();
	RefreshStatusResponse();
	if (HasAuthority())
	{
		if (UCombatAuraSubsystem* Auras = GetWorld() ? GetWorld()->GetSubsystem<UCombatAuraSubsystem>() : nullptr)
		{
			Auras->NotifyUnitChanged(this);
		}
	}
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

void ACombatUnitCharacter::RefreshStatusResponse()
{
	if (!CombatAbilitySystemComponent)
	{
		return;
	}
	UCharacterMovementComponent* Movement = GetCharacterMovement();
	if (Movement)
	{
		Movement->MaxWalkSpeed = CombatAbilitySystemComponent->GetNumericAttribute(UCombatAttributeSet::GetMoveSpeedAttribute());
		if (IsMovementBlocked())
		{
			Movement->StopMovementImmediately();
			Movement->DisableMovement();
		}
		else if (Movement->MovementMode == MOVE_None)
		{
			Movement->SetMovementMode(MOVE_Walking);
		}
	}
	if (LifeState == ECombatLifeState::Dead || LifeState == ECombatLifeState::Dying)
	{
		GetCapsuleComponent()->SetCollisionProfileName(TEXT("CombatCorpse"));
	}
	else if (CombatAbilitySystemComponent->HasMatchingGameplayTag(CombatTags::State_NoUnitCollision))
	{
		GetCapsuleComponent()->SetCollisionProfileName(TEXT("CombatUnitNoCollision"));
	}
	else
	{
		GetCapsuleComponent()->SetCollisionProfileName(TEXT("CombatUnit"));
	}
	// Dying/Dead 的清理由 LifecycleComponent 以固定屏障顺序调用，避免复制投影重复提升 generation。
	if (LifeState == ECombatLifeState::Alive && CombatAttackComponent)
	{
		CombatAttackComponent->HandleOwnerStatusChanged();
	}
	if (LifeState == ECombatLifeState::Alive && CombatOrderComponent)
	{
		CombatOrderComponent->HandleOwnerStatusChanged();
	}
}

void ACombatUnitCharacter::HandleMoveSpeedChanged(const FOnAttributeChangeData& ChangeData)
{
	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->MaxWalkSpeed = FMath::Max(0.0f, ChangeData.NewValue);
	}
}

void ACombatUnitCharacter::SetLifeStateFromLifecycle(const ECombatLifeState NewState)
{
	LifeState = NewState;
	OnRep_LifeState();
}

void ACombatUnitCharacter::IncrementLifeGenerationFromLifecycle()
{
	++LifeGeneration;
}
