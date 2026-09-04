#include "Combat/Combat/CombatDamageSubsystem.h"

#include "Combat/Ability/CombatAbilitySystemComponent.h"
#include "Combat/Attributes/CombatAttributeSet.h"
#include "Combat/Combat/CombatDamageCalculator.h"
#include "Combat/Combat/CombatEffectUtilities.h"
#include "Combat/Combat/CombatHealSubsystem.h"
#include "Combat/Combat/CombatTransactionSubsystem.h"
#include "Combat/Core/CombatNumericPolicy.h"
#include "Combat/Core/CombatTags.h"
#include "Combat/Log/CombatEventSubsystem.h"
#include "Combat/Modifiers/CombatModifierComponent.h"
#include "Combat/Unit/CombatUnitCharacter.h"
#include "Combat/Unit/CombatUnitLifecycleComponent.h"
#include "Combat/UI/CombatOverheadWidgetComponent.h"

FCombatDamageResult UCombatDamageSubsystem::DealDamage(const FCombatDamageRequest& Request)
{
	FCombatDamageResult Result;
	Result.Event.Source = Request.Source;
	Result.Event.Target = Request.Target;
	Result.Event.RequestedAmount = Request.Amount;
	Result.Event.Amount = Request.Amount;
	Result.Event.DamageType = Request.DamageType;
	Result.Event.Flags = Request.Flags;
	Result.Event.SourceContext = Request.SourceContext;

	if (!IsValid(Request.Source) || !IsValid(Request.Target) || !Request.Target->HasAuthority())
	{
		Result.FailureTag = CombatTags::Failure_Authority;
		return Result;
	}
	if (!FCombatNumericPolicyV1::IsValidNonNegativeRequest(Request.Amount))
	{
		Result.FailureTag = CombatTags::Failure_InvalidNumber;
		return Result;
	}
	if (Request.Source->GetLifeState() != ECombatLifeState::Alive
		|| Request.Target->GetLifeState() != ECombatLifeState::Alive)
	{
		Result.FailureTag = CombatTags::Failure_Life_NotAlive;
		return Result;
	}

	Result.Event.Context = CreateEventContext(Request.ParentEvent);
	if (!Result.Event.Context.IsValid())
	{
		Result.FailureTag = CombatTags::Failure_ActionUnsupported;
		return Result;
	}

	UCombatAbilitySystemComponent* SourceAsc = Request.Source->GetCombatAbilitySystemComponent();
	UCombatAbilitySystemComponent* TargetAsc = Request.Target->GetCombatAbilitySystemComponent();
	UCombatModifierComponent* SourceModifiers = Request.Source->GetCombatModifierComponent();
	UCombatModifierComponent* TargetModifiers = Request.Target->GetCombatModifierComponent();
	if (!SourceAsc || !TargetAsc || !SourceModifiers || !TargetModifiers)
	{
		Result.FailureTag = CombatTags::Failure_Target_Invalid;
		return Result;
	}
	if (SourceAsc->HasMatchingGameplayTag(CombatTags::State_OutOfGame)
		|| TargetAsc->HasMatchingGameplayTag(CombatTags::State_OutOfGame))
	{
		Result.FailureTag = CombatTags::Failure_Target_OutOfGame;
		return Result;
	}

	const bool bHpLoss = Request.Flags.HasTagExact(CombatTags::Damage_Flag_HPLoss);
	const bool bInvulnerable = TargetAsc->HasMatchingGameplayTag(CombatTags::State_Invulnerable);
	const bool bMagicImmune = Request.DamageType == ECombatDamageType::Magical
		&& TargetAsc->HasMatchingGameplayTag(CombatTags::State_MagicImmune)
		&& !Request.Flags.HasTagExact(CombatTags::Damage_Flag_BypassMagicImmune);
	if (!bHpLoss && (bInvulnerable || bMagicImmune))
	{
		Result.bSuccess = true;
		Result.bBlocked = true;
		Result.Event.MitigatedAmount = Request.Amount;
		Result.Event.Amount = 0.0f;
		EmitResultLog(Result);
		return Result;
	}

	if (!bHpLoss)
	{
		SourceModifiers->ExecutePreDealDamage(Result.Event);
		if (!FCombatNumericPolicyV1::IsValidNonNegativeRequest(Result.Event.Amount))
		{
			Result.FailureTag = CombatTags::Failure_InvalidNumber;
			return Result;
		}

		const bool bSpellDamage = Request.DamageType != ECombatDamageType::Physical;
		if (bSpellDamage && !Request.Flags.HasTagExact(CombatTags::Damage_Flag_NoSpellAmplification))
		{
			const float Amplify = FCombatNumericPolicyV1::ClampAmplification(
				SourceAsc->GetNumericAttribute(UCombatAttributeSet::GetSpellAmplifyPctAttribute()));
			Result.Event.Amount *= 1.0f + Amplify;
		}
		TargetModifiers->ExecutePreTakeDamage(Result.Event);
		if (!FCombatNumericPolicyV1::IsValidNonNegativeRequest(Result.Event.Amount))
		{
			Result.FailureTag = CombatTags::Failure_InvalidNumber;
			return Result;
		}

		const float BeforeResistance = Result.Event.Amount;
		Result.Event.Amount = FCombatDamageCalculator::CalculateAfterResistance(
			BeforeResistance,
			Request.DamageType,
			TargetAsc->GetNumericAttribute(UCombatAttributeSet::GetArmorAttribute()),
			TargetAsc->GetNumericAttribute(UCombatAttributeSet::GetMagicResistAttribute()));
		Result.Event.MitigatedAmount += BeforeResistance - Result.Event.Amount;

		TargetModifiers->ExecuteDamageBlock(Result.Event);
		if (!FCombatNumericPolicyV1::IsValidNonNegativeRequest(Result.Event.Amount)
			|| !FCombatNumericPolicyV1::IsValidNonNegativeRequest(Result.Event.AbsorbedAmount))
		{
			Result.FailureTag = CombatTags::Failure_InvalidNumber;
			return Result;
		}
	}

	UCombatTransactionSubsystem* Transactions = GetWorld()->GetSubsystem<UCombatTransactionSubsystem>();
	if (!Transactions || !Transactions->BeginSlot(Result.Event.Context, ECombatTransactionKind::Damage, Request.Target))
	{
		Result.FailureTag = CombatTags::Failure_ActionUnsupported;
		return Result;
	}
	const bool bApplied = CombatEffectUtilities::ApplyMetaEffect(
		this, *TargetAsc, *SourceAsc, UCombatAttributeSet::GetIncomingDamageAttribute(),
		CombatTags::Data_Damage_Final, Result.Event.Amount, Result.Event.Context, Request.SourceContext);
	FCombatTransactionDelta Delta;
	if (!bApplied || !Transactions->ConsumeSlot(Result.Event.Context.EventId, ECombatTransactionKind::Damage, Delta))
	{
		Transactions->CancelSlot(Result.Event.Context.EventId);
		Result.FailureTag = CombatTags::Failure_ActionUnsupported;
		return Result;
	}

	Result.Event.AppliedAmount = Delta.AppliedAmount;
	Result.bSuccess = true;
	if (Delta.AppliedAmount > KINDA_SMALL_NUMBER)
	{
		if (UCombatOverheadWidgetComponent* Overhead = Request.Target->GetCombatOverheadWidgetComponent())
		{
			Overhead->ShowDamageNumber(Delta.AppliedAmount, Request.DamageType);
		}
	}
	if (!bHpLoss)
	{
		SourceModifiers->ExecutePostDealDamage(Result.Event);
		TargetModifiers->ExecutePostTakeDamage(Result.Event);

		// 吸血按实际扣除的生命计算，避免把护盾吸收和超出剩余生命的伤害计入；治疗作为子事件保留这次伤害的来源链。
		if (Delta.AppliedAmount > 0.0f && Request.Source != Request.Target
			&& !Request.Flags.HasTagExact(CombatTags::Damage_Flag_NoLifesteal))
		{
			const float Lifesteal = FCombatNumericPolicyV1::ClampLifesteal(
				SourceAsc->GetNumericAttribute(UCombatAttributeSet::GetLifestealPctAttribute()));
			if (Lifesteal > 0.0f)
			{
				FCombatHealRequest HealRequest;
				HealRequest.Source = Request.Source;
				HealRequest.Target = Request.Source;
				HealRequest.Amount = Delta.AppliedAmount * Lifesteal;
				HealRequest.SourceContext = Request.SourceContext;
				HealRequest.ParentEvent = Result.Event.Context;
				GetWorld()->GetSubsystem<UCombatHealSubsystem>()->Heal(HealRequest);
			}
		}
	}

	EmitResultLog(Result);
	if (Delta.bLethal)
	{
		if (UCombatUnitLifecycleComponent* Lifecycle = Request.Target->GetCombatLifecycleComponent())
		{
			Lifecycle->RequestDeath(Result.Event.Context, Request.Source);
		}
	}
	return Result;
}

FCombatEventContext UCombatDamageSubsystem::CreateEventContext(const FCombatEventContext& Parent) const
{
	UCombatEventSubsystem* Events = GetWorld() ? GetWorld()->GetSubsystem<UCombatEventSubsystem>() : nullptr;
	if (!Events)
	{
		return FCombatEventContext();
	}
	return Parent.IsValid() ? Events->CreateChildEvent(Parent) : Events->CreateRootEvent();
}

void UCombatDamageSubsystem::EmitResultLog(const FCombatDamageResult& Result) const
{
	UCombatEventSubsystem* Events = GetWorld() ? GetWorld()->GetSubsystem<UCombatEventSubsystem>() : nullptr;
	if (!Events || !Result.Event.Context.IsValid())
	{
		return;
	}
	FCombatLogRecord Record;
	Record.Context = Result.Event.Context;
	Record.EventType = CombatTags::Event_Combat_DamageApplied;
	Record.FailureTag = Result.FailureTag;
	Record.Source = Result.Event.SourceContext;
	Record.SourceActorId = Result.Event.Source ? Result.Event.Source->GetUniqueID() : 0;
	Record.TargetActorId = Result.Event.Target ? Result.Event.Target->GetUniqueID() : 0;
	Record.UnitLifeGeneration = Result.Event.Target ? Result.Event.Target->GetLifeGeneration() : 0;
	Record.RequestedAmount = Result.Event.RequestedAmount;
	Record.MitigatedAmount = Result.Event.MitigatedAmount;
	Record.AbsorbedAmount = Result.Event.AbsorbedAmount;
	Record.AppliedAmount = Result.Event.AppliedAmount;
	Record.Flags = Result.Event.Flags;
	Record.Diagnostic = Result.bBlocked ? TEXT("Blocked") : TEXT("DamageResult");
	Events->Emit(Record);
}
