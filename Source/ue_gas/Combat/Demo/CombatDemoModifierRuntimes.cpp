#include "Combat/Demo/CombatDemoModifierRuntimes.h"

#include "Combat/Ability/CombatAbilitySystemComponent.h"
#include "Combat/Attributes/CombatAttributeSet.h"
#include "Combat/Combat/CombatDamageSubsystem.h"
#include "Combat/Combat/CombatEffectUtilities.h"
#include "Combat/Core/CombatTags.h"
#include "Combat/Data/CombatDefinitionData.h"
#include "Combat/Motion/CombatMotionComponent.h"
#include "Combat/Unit/CombatUnitCharacter.h"

void UCombatMagicShieldRuntime::OnCreated_Implementation()
{
	RemainingShield = FMath::Max(0.0f, GetRuntimeParameter(TEXT("shield_amount")));
}

void UCombatMagicShieldRuntime::OnDamageBlock_Implementation(FCombatDamageEvent& Event)
{
	if (Event.DamageType != ECombatDamageType::Magical || Event.Amount <= 0.0f || RemainingShield <= 0.0f)
	{
		return;
	}
	const float Absorbed = FMath::Min(RemainingShield, Event.Amount);
	RemainingShield -= Absorbed;
	Event.Amount -= Absorbed;
	Event.AbsorbedAmount += Absorbed;
	if (RemainingShield <= KINDA_SMALL_NUMBER)
	{
		RemainingShield = 0.0f;
		RequestRemoveSelf();
	}
}

void UCombatPeriodicDamageRuntime::OnThink_Implementation(const FCombatScheduledTickContext& TickContext)
{
	ACombatUnitCharacter* Source = GetSourceUnit();
	ACombatUnitCharacter* Target = GetTargetUnit();
	if (!Source || !Target || !Target->GetWorld())
	{
		return;
	}
	FCombatDamageRequest Request;
	Request.Source = Source;
	Request.Target = Target;
	Request.Amount = FMath::Max(0.0f, GetRuntimeParameter(TEXT("damage_per_tick"))) * TickContext.TickCount;
	Request.DamageType = static_cast<ECombatDamageType>(FMath::Clamp(
		FMath::RoundToInt(GetRuntimeParameter(TEXT("damage_type"), 1.0f)), 0, 2));
	Request.SourceContext.DirectSourceType = ECombatDirectSourceType::Modifier;
	if (const UCombatModifierData* Data = GetModifierData())
	{
		Request.SourceContext.ModifierDefinitionId = Data->GetPrimaryAssetId();
	}
	Target->GetWorld()->GetSubsystem<UCombatDamageSubsystem>()->DealDamage(Request);
}

void UCombatDamageReflectionRuntime::OnPostTakeDamage_Implementation(const FCombatDamageEvent& Event)
{
	ACombatUnitCharacter* Reflector = GetTargetUnit();
	if (!Reflector || !Event.Source || Event.Source == Reflector || Event.AppliedAmount <= 0.0f
		|| Event.Flags.HasTagExact(CombatTags::Damage_Flag_Reflection) || !Reflector->GetWorld())
	{
		return;
	}
	const float ReflectionPct = FMath::Max(0.0f, GetRuntimeParameter(TEXT("reflection_pct")));
	if (ReflectionPct <= 0.0f)
	{
		return;
	}
	FCombatDamageRequest Request;
	Request.Source = Reflector;
	Request.Target = Event.Source;
	Request.Amount = Event.AppliedAmount * ReflectionPct;
	Request.DamageType = Event.DamageType;
	Request.Flags.AddTag(CombatTags::Damage_Flag_Reflection);
	Request.Flags.AddTag(CombatTags::Damage_Flag_NoLifesteal);
	Request.ParentEvent = Event.Context;
	Request.SourceContext.DirectSourceType = ECombatDirectSourceType::Modifier;
	if (const UCombatModifierData* Data = GetModifierData())
	{
		Request.SourceContext.ModifierDefinitionId = Data->GetPrimaryAssetId();
	}
	Reflector->GetWorld()->GetSubsystem<UCombatDamageSubsystem>()->DealDamage(Request);
}

FName UCombatDemoOrbRuntime::GetAttackOrbExclusiveGroup_Implementation() const
{
	return TEXT("Orb.Primary");
}

bool UCombatDemoOrbRuntime::CanClaimAttack_Implementation(const FCombatAttackCandidateContext& Context) const
{
	const ACombatUnitCharacter* Source = GetTargetUnit();
	const UCombatAbilitySystemComponent* Asc = Source ? Source->GetCombatAbilitySystemComponent() : nullptr;
	const float Enabled = GetRuntimeParameter(TEXT("orb_enabled"), 1.0f);
	const float ManaCost = GetRuntimeParameter(TEXT("mana_cost"));
	const float BonusDamage = GetRuntimeParameter(TEXT("bonus_damage"));
	const float OnHitDamage = GetRuntimeParameter(TEXT("on_hit_damage"));
	return Context.Attacker == Source && Asc && Enabled > 0.0f
		&& FMath::IsFinite(ManaCost) && ManaCost >= 0.0f
		&& FMath::IsFinite(BonusDamage) && BonusDamage >= 0.0f
		&& FMath::IsFinite(OnHitDamage) && OnHitDamage >= 0.0f
		&& Asc->GetNumericAttribute(UCombatAttributeSet::GetManaAttribute()) + KINDA_SMALL_NUMBER >= ManaCost;
}

bool UCombatDemoOrbRuntime::OnAttackClaimed_Implementation(
	const FCombatAttackCandidateContext& Context,
	FCombatOrbSnapshot& OutSnapshot)
{
	ACombatUnitCharacter* Source = GetTargetUnit();
	UCombatAbilitySystemComponent* Asc = Source ? Source->GetCombatAbilitySystemComponent() : nullptr;
	if (!CanClaimAttack(Context) || !Asc)
	{
		return false;
	}
	// 测试/内容可显式模拟 winner 提交失败；失败发生在任何 Mana 或 Runtime 状态写入之前。
	if (GetRuntimeParameter(TEXT("commit_succeeds"), 1.0f) <= 0.0f)
	{
		return false;
	}

	FCombatOrbSnapshot Snapshot;
	Snapshot.ExclusiveGroup = TEXT("Orb.Primary");
	Snapshot.BonusDamage = GetRuntimeParameter(TEXT("bonus_damage"));
	const int32 DamageTypeValue = FMath::RoundToInt(GetRuntimeParameter(TEXT("damage_type"), -1.0f));
	if (DamageTypeValue >= 0 && DamageTypeValue <= 2)
	{
		Snapshot.bOverrideDamageType = true;
		Snapshot.DamageType = static_cast<ECombatDamageType>(DamageTypeValue);
	}
	const float OnHitDamage = GetRuntimeParameter(TEXT("on_hit_damage"));
	if (OnHitDamage > 0.0f)
	{
		FCombatOnHitAction& Action = Snapshot.OnHitActions.AddDefaulted_GetRef();
		Action.Type = ECombatOnHitActionType::Damage;
		Action.Magnitude = OnHitDamage;
		Action.DamageType = static_cast<ECombatDamageType>(FMath::Clamp(
			FMath::RoundToInt(GetRuntimeParameter(TEXT("on_hit_damage_type"), 1.0f)), 0, 2));
	}

	const float ManaCost = GetRuntimeParameter(TEXT("mana_cost"));
	if (ManaCost > 0.0f && !CombatEffectUtilities::ApplyAttributeAdditive(
		Source, *Asc, UCombatAttributeSet::GetManaAttribute(), -ManaCost))
	{
		return false;
	}
	++SuccessfulClaimCount;
	OutSnapshot = MoveTemp(Snapshot);
	return true;
}

FName UCombatFrostArrowsRuntime::GetAttackOrbExclusiveGroup_Implementation() const
{
	return TEXT("Orb.Primary");
}

bool UCombatFrostArrowsRuntime::CanClaimAttack_Implementation(
	const FCombatAttackCandidateContext& Context) const
{
	const ACombatUnitCharacter* Source = GetTargetUnit();
	const UCombatAbilitySystemComponent* Asc = Source ? Source->GetCombatAbilitySystemComponent() : nullptr;
	const FGameplayAbilitySpecHandle AbilityHandle = GetAbilityOwnerHandle();
	const FGameplayAbilitySpec* Spec = Asc && AbilityHandle.IsValid()
		? Asc->FindAbilitySpecFromHandle(AbilityHandle) : nullptr;
	const UCombatAbilityData* AbilityData = Asc ? Asc->GetCombatAbilityData(AbilityHandle) : nullptr;
	if (Context.Attacker != Source || !Asc || !Spec || !AbilityData
		|| !Asc->IsAutoCastEnabled(AbilityHandle)
		|| Asc->HasMatchingGameplayTag(CombatTags::State_Silenced)
		|| Asc->HasMatchingGameplayTag(CombatTags::State_Broken))
	{
		return false;
	}
	const float ManaCost = AbilityData->GetSpecialValue(TEXT("mana_cost"), Spec->Level);
	const float BonusDamage = AbilityData->GetSpecialValue(TEXT("bonus_damage"), Spec->Level);
	const float SlowDuration = AbilityData->GetSpecialValue(TEXT("slow_duration"), Spec->Level);
	const float SlowPct = AbilityData->GetSpecialValue(TEXT("slow_pct"), Spec->Level);
	return FMath::IsFinite(ManaCost) && ManaCost >= 0.0f
		&& FMath::IsFinite(BonusDamage) && BonusDamage >= 0.0f
		&& FMath::IsFinite(SlowDuration) && SlowDuration >= 0.0f
		&& FMath::IsFinite(SlowPct) && SlowPct >= 0.0f
		&& Asc->GetNumericAttribute(UCombatAttributeSet::GetManaAttribute()) + KINDA_SMALL_NUMBER >= ManaCost;
}

bool UCombatFrostArrowsRuntime::OnAttackClaimed_Implementation(
	const FCombatAttackCandidateContext& Context,
	FCombatOrbSnapshot& OutSnapshot)
{
	ACombatUnitCharacter* Source = GetTargetUnit();
	UCombatAbilitySystemComponent* Asc = Source ? Source->GetCombatAbilitySystemComponent() : nullptr;
	const FGameplayAbilitySpecHandle AbilityHandle = GetAbilityOwnerHandle();
	const FGameplayAbilitySpec* Spec = Asc && AbilityHandle.IsValid()
		? Asc->FindAbilitySpecFromHandle(AbilityHandle) : nullptr;
	const UCombatAbilityData* AbilityData = Asc ? Asc->GetCombatAbilityData(AbilityHandle) : nullptr;
	if (!CanClaimAttack(Context) || !Asc || !Spec || !AbilityData)
	{
		return false;
	}

	FCombatOrbSnapshot Snapshot;
	Snapshot.ExclusiveGroup = TEXT("Orb.Primary");
	Snapshot.BonusDamage = AbilityData->GetSpecialValue(TEXT("bonus_damage"), Spec->Level);
	Snapshot.ProjectileDataOverride = AbilityData->AttackOrbProjectileData;
	const float SlowDuration = AbilityData->GetSpecialValue(TEXT("slow_duration"), Spec->Level);
	const float SlowPct = AbilityData->GetSpecialValue(TEXT("slow_pct"), Spec->Level);
	if (AbilityData->AttackOrbOnHitModifierData && SlowDuration > 0.0f)
	{
		FCombatOnHitAction& SlowAction = Snapshot.OnHitActions.AddDefaulted_GetRef();
		SlowAction.Type = ECombatOnHitActionType::ApplyModifier;
		SlowAction.ModifierData = AbilityData->AttackOrbOnHitModifierData;
		SlowAction.DurationOverride = SlowDuration;
		// special 使用正数表达减速比例，GAS Multiplicitive magnitude 使用剩余倍率。
		SlowAction.RuntimeParameterOverrides.Add(TEXT("slow_pct"), FMath::Clamp(1.0f - SlowPct, 0.0f, 1.0f));
	}

	const float ManaCost = AbilityData->GetSpecialValue(TEXT("mana_cost"), Spec->Level);
	if (ManaCost > 0.0f && !CombatEffectUtilities::ApplyAttributeAdditive(
		Source, *Asc, UCombatAttributeSet::GetManaAttribute(), -ManaCost))
	{
		return false;
	}
	++SuccessfulClaimCount;
	OutSnapshot = MoveTemp(Snapshot);
	return true;
}

bool UCombatSpellBlockRuntime::TryBlockAbility_Implementation(
	const FPrimaryAssetId& AbilityDefinitionId,
	ACombatUnitCharacter* Caster,
	const FCombatEventContext& Context)
{
	(void)AbilityDefinitionId;
	(void)Context;
	if (!Caster || !GetTargetUnit() || Caster == GetTargetUnit())
	{
		return false;
	}
	RequestRemoveSelf();
	return true;
}

void UCombatHookDragRuntime::OnCreated_Implementation()
{
	ACombatUnitCharacter* Target = GetTargetUnit();
	UCombatMotionComponent* Motion = Target ? Target->GetCombatMotionComponent() : nullptr;
	if (!Motion || !HasInitialMotionRequest())
	{
		RequestRemoveSelf();
		return;
	}
	Motion->OnMotionFinished().AddUObject(this, &UCombatHookDragRuntime::HandleMotionFinished);
	const FCombatMotionResult Result = Motion->TryAcquireMotion(GetInitialMotionRequest());
	if (!Result.bSuccess)
	{
		Motion->OnMotionFinished().RemoveAll(this);
		RequestRemoveSelf();
		return;
	}
	MotionHandle = Result.Handle;
}

void UCombatHookDragRuntime::OnDestroyed_Implementation()
{
	ACombatUnitCharacter* Target = GetTargetUnit();
	if (UCombatMotionComponent* Motion = Target ? Target->GetCombatMotionComponent() : nullptr)
	{
		Motion->OnMotionFinished().RemoveAll(this);
		if (Motion->IsMotionActive(MotionHandle))
		{
			Motion->ReleaseMotion(MotionHandle, ECombatMotionFinishReason::Cancelled);
		}
	}
	MotionHandle = FCombatMotionHandle();
}

void UCombatHookDragRuntime::HandleMotionFinished(const FCombatMotionResult& Result)
{
	if (MotionHandle.IsValid() && Result.Handle == MotionHandle)
	{
		MotionHandle = FCombatMotionHandle();
		if (ACombatUnitCharacter* Target = GetTargetUnit())
		{
			Target->GetCombatMotionComponent()->OnMotionFinished().RemoveAll(this);
		}
		RequestRemoveSelf();
	}
}
