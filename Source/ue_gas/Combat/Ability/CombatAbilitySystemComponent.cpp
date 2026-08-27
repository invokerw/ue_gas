#include "Combat/Ability/CombatAbilitySystemComponent.h"

#include "GameplayEffect.h"

#include "Combat/Ability/CombatGameplayAbility.h"
#include "Combat/Attributes/CombatAttributeSet.h"
#include "Combat/Aura/CombatAuraSubsystem.h"
#include "Combat/Combat/CombatEffectUtilities.h"
#include "Combat/Core/CombatNumericPolicy.h"
#include "Combat/Core/CombatTags.h"
#include "Combat/Data/CombatDefinitionData.h"
#include "Combat/Log/CombatEventSubsystem.h"
#include "Combat/Modifiers/CombatModifierComponent.h"
#include "Combat/Modifiers/CombatModifierRuntime.h"
#include "Combat/Targeting/CombatTargetingSubsystem.h"
#include "Combat/Unit/CombatUnitCharacter.h"

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
	// 相同 Owner/Avatar 仍执行 Intrinsic reconcile，覆盖 Respawn 和 ActorInfo 重建后的幂等恢复。
	if (GetOwnerActor() != InOwnerActor || GetAvatarActor() != InAvatarActor)
	{
		if (AbilityActorInfo.IsValid())
		{
			CancelAllAbilities();
			PendingTargetData.Reset();
		}
		InitAbilityActorInfo(InOwnerActor, InAvatarActor);
	}
	ReconcileIntrinsicModifiers();
}

void UCombatAbilitySystemComponent::ClearCombatActorInfo()
{
	CancelAllAbilities();
	PendingTargetData.Reset();
	if (AbilityActorInfo.IsValid())
	{
		ClearActorInfo();
	}
}

bool UCombatAbilitySystemComponent::IsCombatActorInfoInitialized() const
{
	return AbilityActorInfo.IsValid() && AbilityActorInfo->OwnerActor.IsValid() && AbilityActorInfo->AvatarActor.IsValid();
}

bool UCombatAbilitySystemComponent::SetInitialAutoCastState(
	const FGameplayAbilitySpecHandle Handle,
	const bool bEnabled)
{
	const UCombatAbilityData* Data = GetCombatAbilityData(Handle);
	if (!Handle.IsValid() || !FindAbilitySpecFromHandle(Handle) || !Data
		|| (bEnabled && !Data->BehaviorTags.HasTagExact(CombatTags::Ability_Behavior_AutoCast)))
	{
		return false;
	}
	AutoCastStates.FindOrAdd(Handle) = bEnabled;
	return true;
}

bool UCombatAbilitySystemComponent::IsAutoCastEnabled(const FGameplayAbilitySpecHandle Handle) const
{
	if (const bool* State = AutoCastStates.Find(Handle))
	{
		return *State;
	}
	return false;
}

bool UCombatAbilitySystemComponent::GrantCombatAbility(
	const TSubclassOf<UCombatGameplayAbility> AbilityClass,
	const int32 InitialLevel,
	const bool bInitialAutoCast,
	FGameplayAbilitySpecHandle& OutHandle,
	FGameplayTag& OutFailureTag)
{
	OutHandle = FGameplayAbilitySpecHandle();
	OutFailureTag = FGameplayTag();
	if (!GetOwnerActor() || !GetOwnerActor()->HasAuthority())
	{
		OutFailureTag = CombatTags::Failure_Authority;
		return false;
	}
	const UCombatGameplayAbility* AbilityCdo = AbilityClass
		? Cast<UCombatGameplayAbility>(AbilityClass->GetDefaultObject()) : nullptr;
	const UCombatAbilityData* Data = AbilityCdo ? AbilityCdo->GetAbilityData() : nullptr;
	FString Diagnostic;
	if (!Data || !Data->ValidateRuntime(Diagnostic))
	{
		OutFailureTag = CombatTags::Failure_ActionUnsupported;
		return false;
	}
	if (InitialLevel < 1 || InitialLevel > Data->MaxLevel)
	{
		OutFailureTag = CombatTags::Failure_Ability_InvalidLevel;
		return false;
	}
	if (FindCombatAbilitySpecByDefinitionId(Data->GetPrimaryAssetId()))
	{
		OutFailureTag = CombatTags::Failure_Ability_DuplicateDefinition;
		return false;
	}
	if (bInitialAutoCast && !Data->BehaviorTags.HasTagExact(CombatTags::Ability_Behavior_AutoCast))
	{
		OutFailureTag = CombatTags::Failure_ActionUnsupported;
		return false;
	}

	OutHandle = GiveAbility(FGameplayAbilitySpec(AbilityClass, InitialLevel));
	if (!OutHandle.IsValid() || !SetInitialAutoCastState(OutHandle, bInitialAutoCast))
	{
		if (OutHandle.IsValid()) { ClearAbility(OutHandle); }
		OutHandle = FGameplayAbilitySpecHandle();
		OutFailureTag = CombatTags::Failure_ActionUnsupported;
		return false;
	}
	if (!ReconcileIntrinsicModifier(OutHandle))
	{
		AutoCastStates.Remove(OutHandle);
		ClearAbility(OutHandle);
		OutHandle = FGameplayAbilitySpecHandle();
		OutFailureTag = CombatTags::Failure_ActionUnsupported;
		return false;
	}
	EmitAbilitySpecLog(OutHandle, CombatTags::Event_Combat_AbilityGranted,
		FString::Printf(TEXT("Granted Level=%d AutoCast=%d"), InitialLevel, bInitialAutoCast));
	return true;
}

bool UCombatAbilitySystemComponent::SetCombatAbilityLevel(
	const FGameplayAbilitySpecHandle Handle,
	const int32 NewLevel,
	FGameplayTag& OutFailureTag)
{
	OutFailureTag = FGameplayTag();
	if (!GetOwnerActor() || !GetOwnerActor()->HasAuthority())
	{
		OutFailureTag = CombatTags::Failure_Authority;
		return false;
	}
	FGameplayAbilitySpec* Spec = FindAbilitySpecFromHandle(Handle);
	const UCombatAbilityData* Data = GetCombatAbilityData(Handle);
	if (!Spec || !Data)
	{
		OutFailureTag = CombatTags::Failure_Ability_NotGranted;
		return false;
	}
	if (NewLevel < 1 || NewLevel > Data->MaxLevel)
	{
		OutFailureTag = CombatTags::Failure_Ability_InvalidLevel;
		return false;
	}
	if (Spec->Level == NewLevel)
	{
		return true;
	}
	const int32 OldLevel = Spec->Level;
	Spec->Level = NewLevel;
	MarkAbilitySpecDirty(*Spec);
	EmitAbilitySpecLog(Handle, CombatTags::Event_Combat_AbilityLevelChanged,
		FString::Printf(TEXT("Level %d -> %d"), OldLevel, NewLevel));
	return true;
}

bool UCombatAbilitySystemComponent::RemoveCombatAbility(
	const FGameplayAbilitySpecHandle Handle,
	FGameplayTag& OutFailureTag)
{
	OutFailureTag = FGameplayTag();
	if (!GetOwnerActor() || !GetOwnerActor()->HasAuthority())
	{
		OutFailureTag = CombatTags::Failure_Authority;
		return false;
	}
	if (!FindAbilitySpecFromHandle(Handle))
	{
		OutFailureTag = CombatTags::Failure_Ability_NotGranted;
		return false;
	}
	EmitAbilitySpecLog(Handle, CombatTags::Event_Combat_AbilityRemoved, TEXT("Removed"));
	CancelAbilityHandle(Handle);
	if (const FCombatModifierHandle* ModifierHandle = IntrinsicModifierHandles.Find(Handle))
	{
		if (ACombatUnitCharacter* Unit = GetCombatAvatar())
		{
			Unit->GetCombatModifierComponent()->RemoveModifier(*ModifierHandle);
		}
	}
	IntrinsicModifierHandles.Remove(Handle);
	if (const FActiveGameplayEffectHandle* CooldownHandle = CooldownEffectHandles.Find(Handle))
	{
		RemoveActiveGameplayEffect(*CooldownHandle);
	}
	CooldownEffectHandles.Remove(Handle);
	CooldownEffectDefinitions.Remove(Handle);
	CooldownEndTimes.Remove(Handle);
	PendingTargetData.Remove(Handle);
	AutoCastStates.Remove(Handle);
	ClearAbility(Handle);
	return true;
}

bool UCombatAbilitySystemComponent::SetAutoCastEnabled(
	const FGameplayAbilitySpecHandle Handle,
	const bool bEnabled,
	FGameplayTag& OutFailureTag)
{
	OutFailureTag = FGameplayTag();
	ACombatUnitCharacter* Unit = GetCombatAvatar();
	const UCombatAbilityData* Data = GetCombatAbilityData(Handle);
	if (!GetOwnerActor() || !GetOwnerActor()->HasAuthority())
	{
		OutFailureTag = CombatTags::Failure_Authority;
		return false;
	}
	if (!Unit || Unit->GetLifeState() != ECombatLifeState::Alive)
	{
		OutFailureTag = CombatTags::Failure_Life_NotAlive;
		return false;
	}
	if (!Data || !Data->BehaviorTags.HasTagExact(CombatTags::Ability_Behavior_AutoCast))
	{
		OutFailureTag = Data ? CombatTags::Failure_ActionUnsupported : CombatTags::Failure_Ability_NotGranted;
		return false;
	}
	bool& Current = AutoCastStates.FindOrAdd(Handle);
	if (Current == bEnabled)
	{
		return true;
	}
	Current = bEnabled;
	EmitAbilitySpecLog(Handle, CombatTags::Event_Combat_AutoCastChanged,
		FString::Printf(TEXT("AutoCast=%d"), bEnabled));
	return true;
}

FGameplayAbilitySpec* UCombatAbilitySystemComponent::FindCombatAbilitySpecByDefinitionId(
	const FPrimaryAssetId& DefinitionId)
{
	if (!DefinitionId.IsValid())
	{
		return nullptr;
	}
	for (FGameplayAbilitySpec& Spec : GetActivatableAbilities())
	{
		const UCombatGameplayAbility* Ability = Cast<UCombatGameplayAbility>(Spec.Ability.Get());
		if (Ability && Ability->GetAbilityData()
			&& Ability->GetAbilityData()->GetPrimaryAssetId() == DefinitionId)
		{
			return &Spec;
		}
	}
	return nullptr;
}

const UCombatAbilityData* UCombatAbilitySystemComponent::GetCombatAbilityData(
	const FGameplayAbilitySpecHandle Handle) const
{
	const FGameplayAbilitySpec* Spec = FindAbilitySpecFromHandle(Handle);
	const UCombatGameplayAbility* Ability = Spec ? Cast<UCombatGameplayAbility>(Spec->Ability.Get()) : nullptr;
	return Ability ? Ability->GetAbilityData() : nullptr;
}

bool UCombatAbilitySystemComponent::TryActivateCombatAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FCombatAbilityTargetData& TargetData,
	FGameplayTag& OutFailureTag)
{
	OutFailureTag = FGameplayTag();
	ACombatUnitCharacter* Unit = GetCombatAvatar();
	FGameplayAbilitySpec* Spec = FindAbilitySpecFromHandle(Handle);
	const UCombatAbilityData* Data = GetCombatAbilityData(Handle);
	if (!GetOwnerActor() || !GetOwnerActor()->HasAuthority())
	{
		OutFailureTag = CombatTags::Failure_Authority;
		return false;
	}
	if (!Unit || Unit->GetLifeState() != ECombatLifeState::Alive)
	{
		OutFailureTag = CombatTags::Failure_Life_NotAlive;
		return false;
	}
	if (!Spec || !Data)
	{
		OutFailureTag = CombatTags::Failure_Ability_NotGranted;
		return false;
	}
	if (Spec->IsActive() || PendingTargetData.Contains(Handle))
	{
		OutFailureTag = CombatTags::Failure_Ability_AlreadyActive;
		return false;
	}
	const bool bHardStateBlocked = HasMatchingGameplayTag(CombatTags::State_Stunned)
		|| HasMatchingGameplayTag(CombatTags::State_Hexed)
		|| HasMatchingGameplayTag(CombatTags::State_Frozen);
	const bool bSilenceBlocked = HasMatchingGameplayTag(CombatTags::State_Silenced)
		&& !Data->BehaviorTags.HasTagExact(CombatTags::Ability_Behavior_IgnoreSilence);
	if (bHardStateBlocked || bSilenceBlocked)
	{
		OutFailureTag = CombatTags::Failure_Ability_UnitStateBlocked;
		return false;
	}
	UCombatTargetingSubsystem* Targeting = GetWorld() ? GetWorld()->GetSubsystem<UCombatTargetingSubsystem>() : nullptr;
	const FCombatTargetValidationResult TargetResult = Targeting
		? Targeting->ValidateAbilityTarget(Unit, Data->BehaviorTags, Data->TargetingRules, TargetData)
		: FCombatTargetValidationResult();
	if (!TargetResult.bValid)
	{
		OutFailureTag = TargetResult.FailureTag.IsValid()
			? TargetResult.FailureTag : CombatTags::Failure_Ability_InvalidTargetData.GetTag();
		return false;
	}
	if (!PreflightCombatAbility(Handle, *Data, Spec->Level, OutFailureTag))
	{
		return false;
	}
	PendingTargetData.Add(Handle, TargetData);
	// 在进入 GAS 内部激活前显式收集 CanActivate 标签，使 RPC/Order 获得稳定失败原因。
	const UGameplayAbility* AbilitySource = Spec->GetPrimaryInstance()
		? Spec->GetPrimaryInstance() : Spec->Ability.Get();
	FGameplayTagContainer ActivationFailureTags;
	if (!AbilitySource || !AbilitySource->CanActivateAbility(
		Handle, AbilityActorInfo.Get(), nullptr, nullptr, &ActivationFailureTags))
	{
		PendingTargetData.Remove(Handle);
		const FGameplayTag CombatFailureRoot = FGameplayTag::RequestGameplayTag(TEXT("Combat.Failure"), false);
		for (const FGameplayTag& FailureTag : ActivationFailureTags)
		{
			if (CombatFailureRoot.IsValid() && FailureTag.MatchesTag(CombatFailureRoot))
			{
				OutFailureTag = FailureTag;
				break;
			}
		}
		if (!OutFailureTag.IsValid()) { OutFailureTag = CombatTags::Failure_ActionUnsupported; }
		UE_LOG(LogCombat, Verbose, TEXT("Combat Ability CanActivate rejected Ability=%s Tags=%s InstanceData=%s"),
			*Data->GetPrimaryAssetId().ToString(), *ActivationFailureTags.ToStringSimple(),
			*GetNameSafe(Cast<UCombatGameplayAbility>(AbilitySource)
				? Cast<UCombatGameplayAbility>(AbilitySource)->GetAbilityData() : nullptr));
		return false;
	}
	if (!TryActivateAbility(Handle, false))
	{
		PendingTargetData.Remove(Handle);
		if (!OutFailureTag.IsValid()) { OutFailureTag = CombatTags::Failure_ActionUnsupported; }
		return false;
	}
	return true;
}

void UCombatAbilitySystemComponent::ServerTryActivateCombatAbility_Implementation(
	const FGameplayAbilitySpecHandle Handle,
	const FCombatAbilityTargetData TargetData)
{
	FGameplayTag FailureTag;
	TryActivateCombatAbility(Handle, TargetData, FailureTag);
}

void UCombatAbilitySystemComponent::ServerSetAutoCastEnabled_Implementation(
	const FGameplayAbilitySpecHandle Handle,
	const bool bEnabled)
{
	FGameplayTag FailureTag;
	SetAutoCastEnabled(Handle, bEnabled, FailureTag);
}

bool UCombatAbilitySystemComponent::PeekPendingTargetData(
	const FGameplayAbilitySpecHandle Handle,
	FCombatAbilityTargetData& OutTargetData) const
{
	if (const FCombatAbilityTargetData* Found = PendingTargetData.Find(Handle))
	{
		OutTargetData = *Found;
		return true;
	}
	return false;
}

bool UCombatAbilitySystemComponent::ConsumePendingTargetData(
	const FGameplayAbilitySpecHandle Handle,
	FCombatAbilityTargetData& OutTargetData)
{
	if (!PeekPendingTargetData(Handle, OutTargetData))
	{
		return false;
	}
	PendingTargetData.Remove(Handle);
	return true;
}

bool UCombatAbilitySystemComponent::PreflightCombatAbility(
	const FGameplayAbilitySpecHandle Handle,
	const UCombatAbilityData& AbilityData,
	const int32 AbilityLevel,
	FGameplayTag& OutFailureTag) const
{
	OutFailureTag = FGameplayTag();
	if (!FindAbilitySpecFromHandle(Handle) || AbilityLevel < 1 || AbilityLevel > AbilityData.MaxLevel)
	{
		OutFailureTag = CombatTags::Failure_Ability_InvalidLevel;
		return false;
	}
	const float ManaCost = AbilityData.GetSpecialValue(TEXT("mana_cost"), AbilityLevel);
	if (!FMath::IsFinite(ManaCost) || ManaCost < 0.0f
		|| GetNumericAttribute(UCombatAttributeSet::GetManaAttribute()) + KINDA_SMALL_NUMBER < ManaCost)
	{
		OutFailureTag = CombatTags::Failure_Ability_Cost;
		return false;
	}
	if (GetCombatAbilityCooldownRemaining(Handle) > 0.0f)
	{
		OutFailureTag = CombatTags::Failure_Ability_Cooldown;
		return false;
	}
	return true;
}

bool UCombatAbilitySystemComponent::CommitCombatAbilityStage(
	const FGameplayAbilitySpecHandle Handle,
	const UCombatAbilityData& AbilityData,
	const int32 AbilityLevel,
	const ECombatAbilityCommitStage Stage,
	bool& bCostCommitted,
	bool& bCooldownCommitted,
	FGameplayTag& OutFailureTag)
{
	OutFailureTag = FGameplayTag();
	const bool bNeedCost = !bCostCommitted && AbilityData.CostCommitPoint == Stage;
	const bool bNeedCooldown = !bCooldownCommitted && AbilityData.CooldownCommitPoint == Stage;
	if (!bNeedCost && !bNeedCooldown)
	{
		return true;
	}
	const float ManaCost = AbilityData.GetSpecialValue(TEXT("mana_cost"), AbilityLevel);
	const float BaseCooldown = AbilityData.GetSpecialValue(TEXT("cooldown"), AbilityLevel);
	const float CurrentMana = GetNumericAttribute(UCombatAttributeSet::GetManaAttribute());
	if ((bNeedCost && (!FMath::IsFinite(ManaCost) || ManaCost < 0.0f
		|| CurrentMana + KINDA_SMALL_NUMBER < ManaCost))
		|| (bNeedCooldown && (!FMath::IsFinite(BaseCooldown) || BaseCooldown < 0.0f
			|| GetCombatAbilityCooldownRemaining(Handle) > 0.0f)))
	{
		OutFailureTag = bNeedCost && CurrentMana < ManaCost
			? CombatTags::Failure_Ability_Cost : CombatTags::Failure_Ability_Cooldown;
		return false;
	}

	bool bAppliedCooldown = false;
	if (bNeedCooldown)
	{
		const float Cdr = FCombatNumericPolicyV1::ClampReduction(
			GetNumericAttribute(UCombatAttributeSet::GetCooldownReductionPctAttribute()));
		if (!ApplyCombatCooldown(Handle, BaseCooldown * (1.0f - Cdr)))
		{
			OutFailureTag = CombatTags::Failure_Ability_CommitFailed;
			return false;
		}
		bAppliedCooldown = true;
	}
	if (bNeedCost && ManaCost > 0.0f
		&& !CombatEffectUtilities::ApplyAttributeAdditive(
			this, *this, UCombatAttributeSet::GetManaAttribute(), -ManaCost))
	{
		// 同 Stage 的第二项失败时回滚刚应用的 cooldown，防止出现半提交。
		if (bAppliedCooldown)
		{
			if (const FActiveGameplayEffectHandle* CooldownHandle = CooldownEffectHandles.Find(Handle))
			{
				RemoveActiveGameplayEffect(*CooldownHandle);
			}
			CooldownEffectHandles.Remove(Handle);
			CooldownEffectDefinitions.Remove(Handle);
			CooldownEndTimes.Remove(Handle);
		}
		OutFailureTag = CombatTags::Failure_Ability_CommitFailed;
		return false;
	}
	if (bNeedCost) { bCostCommitted = true; }
	if (bNeedCooldown) { bCooldownCommitted = true; }
	return true;
}

float UCombatAbilitySystemComponent::GetCombatAbilityCooldownRemaining(
	const FGameplayAbilitySpecHandle Handle) const
{
	const double* EndTime = CooldownEndTimes.Find(Handle);
	return EndTime && GetWorld()
		? static_cast<float>(FMath::Max(0.0, *EndTime - GetWorld()->GetTimeSeconds())) : 0.0f;
}

void UCombatAbilitySystemComponent::ReconcileIntrinsicModifiers()
{
	if (!GetOwnerActor() || !GetOwnerActor()->HasAuthority())
	{
		return;
	}
	TArray<FGameplayAbilitySpecHandle> Handles;
	for (const FGameplayAbilitySpec& Spec : GetActivatableAbilities())
	{
		Handles.Add(Spec.Handle);
	}
	for (const FGameplayAbilitySpecHandle Handle : Handles)
	{
		ReconcileIntrinsicModifier(Handle);
	}
}

void UCombatAbilitySystemComponent::CancelCombatAbilitiesBlockedByStatus(const FGameplayTag StatusTag)
{
	if (StatusTag != CombatTags::State_Stunned && StatusTag != CombatTags::State_Hexed
		&& StatusTag != CombatTags::State_Frozen && StatusTag != CombatTags::State_Silenced)
	{
		return;
	}
	TArray<FGameplayAbilitySpecHandle> HandlesToCancel;
	for (const FGameplayAbilitySpec& Spec : GetActivatableAbilities())
	{
		if (!Spec.IsActive())
		{
			continue;
		}
		const UCombatAbilityData* Data = GetCombatAbilityData(Spec.Handle);
		const bool bIgnoreSilence = Data
			&& Data->BehaviorTags.HasTagExact(CombatTags::Ability_Behavior_IgnoreSilence);
		if (StatusTag != CombatTags::State_Silenced || !bIgnoreSilence)
		{
			HandlesToCancel.Add(Spec.Handle);
		}
	}
	for (const FGameplayAbilitySpecHandle Handle : HandlesToCancel)
	{
		CancelAbilityHandle(Handle);
	}
}

void UCombatAbilitySystemComponent::NotifyCombatAbilityOrderReleased(
	const FGameplayAbilitySpecHandle Handle,
	const bool bSuccess,
	const FGameplayTag FailureTag,
	const ECombatChannelInterruptOrderPolicy InterruptPolicy)
{
	AbilityOrderReleasedDelegate.Broadcast(Handle, bSuccess, FailureTag, InterruptPolicy);
}

void UCombatAbilitySystemComponent::OnTagUpdated(const FGameplayTag& Tag, const bool bTagExists)
{
	Super::OnTagUpdated(Tag, bTagExists);
	if (bTagExists)
	{
		CancelCombatAbilitiesBlockedByStatus(Tag);
	}
	if (ACombatUnitCharacter* Unit = GetCombatAvatar())
	{
		Unit->RefreshStatusResponse();
		if (Tag == CombatTags::State_Broken)
		{
			if (UCombatAuraSubsystem* Auras = GetWorld() ? GetWorld()->GetSubsystem<UCombatAuraSubsystem>() : nullptr)
			{
				Auras->NotifyUnitChanged(Unit);
			}
		}
	}
}

ACombatUnitCharacter* UCombatAbilitySystemComponent::GetCombatAvatar() const
{
	return Cast<ACombatUnitCharacter>(GetAvatarActor());
}

void UCombatAbilitySystemComponent::EmitAbilitySpecLog(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayTag& EventType,
	const FString& Diagnostic) const
{
	UCombatEventSubsystem* Events = GetWorld() ? GetWorld()->GetSubsystem<UCombatEventSubsystem>() : nullptr;
	ACombatUnitCharacter* Unit = GetCombatAvatar();
	const UCombatAbilityData* Data = GetCombatAbilityData(Handle);
	if (!Events || !Unit || !Data)
	{
		return;
	}
	FCombatLogRecord Record;
	Record.Context = Events->CreateRootEvent();
	Record.EventType = EventType;
	Record.Source.DirectSourceType = ECombatDirectSourceType::Ability;
	Record.Source.AbilityDefinitionId = Data->GetPrimaryAssetId();
	Record.SourceActorId = Unit->GetUniqueID();
	Record.TargetActorId = Unit->GetUniqueID();
	Record.UnitLifeGeneration = Unit->GetLifeGeneration();
	Record.Diagnostic = FString::Printf(TEXT("Ability=%s Spec=%s %s"),
		*Data->GetPrimaryAssetId().ToString(), *Handle.ToString(), *Diagnostic);
	Events->Emit(Record);
}

bool UCombatAbilitySystemComponent::ReconcileIntrinsicModifier(const FGameplayAbilitySpecHandle Handle)
{
	ACombatUnitCharacter* Unit = GetCombatAvatar();
	const UCombatAbilityData* Data = GetCombatAbilityData(Handle);
	if (!Unit || !Data)
	{
		return false;
	}
	if (!Data->IntrinsicModifier)
	{
		return true;
	}
	if (const FCombatModifierHandle* ExistingHandle = IntrinsicModifierHandles.Find(Handle))
	{
		if (UCombatModifierRuntime* Runtime = Unit->GetCombatModifierComponent()->FindRuntime(*ExistingHandle);
			Runtime && Runtime->GetAbilityOwnerHandle() == Handle)
		{
			return true;
		}
		IntrinsicModifierHandles.Remove(Handle);
	}
	if (Unit->GetLifeState() != ECombatLifeState::Alive)
	{
		return true;
	}
	FCombatModifierApplyRequest Request;
	Request.Source = Unit;
	Request.ModifierData = Data->IntrinsicModifier;
	Request.AbilityOwnerHandle = Handle;
	const FCombatModifierApplyResult Result = Unit->GetCombatModifierComponent()->ApplyModifier(Request);
	if (!Result.bSuccess || !Result.Handle.IsValid())
	{
		return false;
	}
	IntrinsicModifierHandles.Add(Handle, Result.Handle);
	return true;
}

bool UCombatAbilitySystemComponent::ApplyCombatCooldown(
	const FGameplayAbilitySpecHandle Handle,
	const float Duration)
{
	if (!FMath::IsFinite(Duration) || Duration < 0.0f || !GetWorld())
	{
		return false;
	}
	CooldownEndTimes.Add(Handle, GetWorld()->GetTimeSeconds() + Duration);
	if (Duration <= 0.0f)
	{
		CooldownEffectDefinitions.Remove(Handle);
		return true;
	}
	UGameplayEffect* Effect = NewObject<UGameplayEffect>(this);
	Effect->DurationPolicy = EGameplayEffectDurationType::HasDuration;
	Effect->DurationMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(Duration));
	FGameplayEffectSpec Spec(Effect, MakeEffectContext(), 1.0f);
	const FActiveGameplayEffectHandle ActiveHandle = ApplyGameplayEffectSpecToSelf(Spec);
	if (!ActiveHandle.WasSuccessfullyApplied())
	{
		CooldownEndTimes.Remove(Handle);
		return false;
	}
	CooldownEffectDefinitions.Add(Handle, Effect);
	CooldownEffectHandles.Add(Handle, ActiveHandle);
	return true;
}
