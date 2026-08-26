#include "Combat/Combat/CombatHealSubsystem.h"

#include "Combat/Ability/CombatAbilitySystemComponent.h"
#include "Combat/Attributes/CombatAttributeSet.h"
#include "Combat/Combat/CombatEffectUtilities.h"
#include "Combat/Combat/CombatTransactionSubsystem.h"
#include "Combat/Core/CombatNumericPolicy.h"
#include "Combat/Core/CombatTags.h"
#include "Combat/Log/CombatEventSubsystem.h"
#include "Combat/Modifiers/CombatModifierComponent.h"
#include "Combat/Unit/CombatUnitCharacter.h"

FCombatHealResult UCombatHealSubsystem::Heal(const FCombatHealRequest& Request)
{
	FCombatHealResult Result;
	Result.Event.Source = Request.Source;
	Result.Event.Target = Request.Target;
	Result.Event.RequestedAmount = Request.Amount;
	Result.Event.Amount = Request.Amount;
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

	SourceModifiers->ExecutePreDealHeal(Result.Event);
	if (!FCombatNumericPolicyV1::IsValidNonNegativeRequest(Result.Event.Amount))
	{
		Result.FailureTag = CombatTags::Failure_InvalidNumber;
		return Result;
	}
	const float SourceAmplify = FCombatNumericPolicyV1::ClampAmplification(
		SourceAsc->GetNumericAttribute(UCombatAttributeSet::GetHealAmplifyPctAttribute()));
	const float TargetAmplify = FCombatNumericPolicyV1::ClampAmplification(
		TargetAsc->GetNumericAttribute(UCombatAttributeSet::GetHealReceivedPctAttribute()));
	Result.Event.Amount *= (1.0f + SourceAmplify) * (1.0f + TargetAmplify);
	TargetModifiers->ExecutePreTakeHeal(Result.Event);
	if (!FCombatNumericPolicyV1::IsValidNonNegativeRequest(Result.Event.Amount))
	{
		Result.FailureTag = CombatTags::Failure_InvalidNumber;
		return Result;
	}

	UCombatTransactionSubsystem* Transactions = GetWorld()->GetSubsystem<UCombatTransactionSubsystem>();
	if (!Transactions || !Transactions->BeginSlot(Result.Event.Context, ECombatTransactionKind::Heal, Request.Target))
	{
		Result.FailureTag = CombatTags::Failure_ActionUnsupported;
		return Result;
	}
	const bool bApplied = CombatEffectUtilities::ApplyMetaEffect(
		this, *TargetAsc, *SourceAsc, UCombatAttributeSet::GetIncomingHealingAttribute(),
		CombatTags::Data_Heal_Final, Result.Event.Amount, Result.Event.Context, Request.SourceContext);
	FCombatTransactionDelta Delta;
	if (!bApplied || !Transactions->ConsumeSlot(Result.Event.Context.EventId, ECombatTransactionKind::Heal, Delta))
	{
		Transactions->CancelSlot(Result.Event.Context.EventId);
		Result.FailureTag = CombatTags::Failure_ActionUnsupported;
		return Result;
	}

	Result.Event.AppliedAmount = Delta.AppliedAmount;
	Result.Event.OverhealAmount = FMath::Max(0.0f, Result.Event.Amount - Delta.AppliedAmount);
	Result.bSuccess = true;
	SourceModifiers->ExecutePostDealHeal(Result.Event);
	TargetModifiers->ExecutePostTakeHeal(Result.Event);
	EmitResultLog(Result);
	return Result;
}

FCombatEventContext UCombatHealSubsystem::CreateEventContext(const FCombatEventContext& Parent) const
{
	UCombatEventSubsystem* Events = GetWorld() ? GetWorld()->GetSubsystem<UCombatEventSubsystem>() : nullptr;
	if (!Events)
	{
		return FCombatEventContext();
	}
	return Parent.IsValid() ? Events->CreateChildEvent(Parent) : Events->CreateRootEvent();
}

void UCombatHealSubsystem::EmitResultLog(const FCombatHealResult& Result) const
{
	UCombatEventSubsystem* Events = GetWorld() ? GetWorld()->GetSubsystem<UCombatEventSubsystem>() : nullptr;
	if (!Events || !Result.Event.Context.IsValid())
	{
		return;
	}
	FCombatLogRecord Record;
	Record.Context = Result.Event.Context;
	Record.EventType = CombatTags::Event_Combat_HealApplied;
	Record.FailureTag = Result.FailureTag;
	Record.Source = Result.Event.SourceContext;
	Record.SourceActorId = Result.Event.Source ? Result.Event.Source->GetUniqueID() : 0;
	Record.TargetActorId = Result.Event.Target ? Result.Event.Target->GetUniqueID() : 0;
	Record.UnitLifeGeneration = Result.Event.Target ? Result.Event.Target->GetLifeGeneration() : 0;
	Record.RequestedAmount = Result.Event.RequestedAmount;
	Record.AppliedAmount = Result.Event.AppliedAmount;
	Record.Diagnostic = FString::Printf(TEXT("HealResult Overheal=%.3f"), Result.Event.OverhealAmount);
	Events->Emit(Record);
}
