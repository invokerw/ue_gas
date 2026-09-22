#include "Combat/AI/CombatAIBrainComponent.h"

#include "Combat/AI/CombatAIProfileData.h"
#include "Combat/Ability/CombatAbilitySystemComponent.h"
#include "Combat/Attributes/CombatAttributeSet.h"
#include "Combat/Core/CombatTags.h"
#include "Combat/Data/CombatDefinitionData.h"
#include "Combat/Order/CombatOrderComponent.h"
#include "Combat/Unit/CombatUnitCharacter.h"
#include "Engine/World.h"

bool UCombatAIBrainComponent::EvaluateTacticalCandidates()
{
	TacticalSnapshot = {};
	TacticalSnapshot.Revision = ++NextTacticalRevision;
	TacticalSnapshot.EvaluatedAt = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	if (!Profile || !Profile->IsTacticsEnabled() || !CanSubmit(ControlEpoch) || !Context.Unit)
	{
		return false;
	}
	TacticalSnapshot.GuardUtility = FCombatAIUtilityScoring::ClampScore(Profile->GuardUtility);
	TacticalSnapshot.AttackUtility = CanEngageKnownTarget()
		? FCombatAIUtilityScoring::ClampScore(Profile->AttackUtility) : 0.0f;
	TacticalSnapshot.RepositionUtility = IsTacticalActionAvailable(ECombatAITacticalAction::Reposition)
		? FCombatAIUtilityScoring::ClampScore(Profile->RepositionUtility) : 0.0f;

	auto* AbilitySystem = Context.Unit->GetCombatAbilitySystemComponent();
	auto* Orders = Context.Unit->GetCombatOrderComponent();
	if (!AbilitySystem || !Orders)
	{
		return false;
	}

	const FCombatAIKnownTarget* KnownTarget = SelectKnownTarget();
	for (int32 RuleIndex = 0; RuleIndex < Profile->AbilityUsageRules.Num(); ++RuleIndex)
	{
		const FCombatAIAbilityUsageRule& Rule = Profile->AbilityUsageRules[RuleIndex];
		if (!Rule.Validate())
		{
			continue;
		}
		const FGameplayAbilitySpec* Spec = AbilitySystem->FindCombatAbilitySpecByDefinitionId(Rule.AbilityDefinitionId);
		const UCombatAbilityData* AbilityData = Spec ? AbilitySystem->GetCombatAbilityData(Spec->Handle) : nullptr;
		if (!Spec || !AbilityData
			|| AbilityData->BehaviorTags.HasTagExact(CombatTags::Ability_Behavior_Passive)
			|| AbilityData->BehaviorTags.HasTagExact(CombatTags::Ability_Behavior_AutoCast)
			|| AbilityData->BehaviorTags.HasTagExact(CombatTags::Ability_Behavior_Attack))
		{
			continue;
		}

		FCombatOrderRequest Request;
		Request.AbilitySpecHandle = Spec->Handle;
		int32 EffectiveTargets = 1;
		if (AbilityData->BehaviorTags.HasTagExact(CombatTags::Ability_Behavior_NoTarget))
		{
			if (Rule.TargetPolicy != ECombatAIAbilityTargetPolicy::Self)
			{
				continue;
			}
			Request.Type = ECombatOrderType::CastNoTarget;
		}
		else if (AbilityData->BehaviorTags.HasTagExact(CombatTags::Ability_Behavior_UnitTarget))
		{
			Request.Type = ECombatOrderType::CastTarget;
			if (Rule.TargetPolicy == ECombatAIAbilityTargetPolicy::Self)
			{
				Request.TargetUnit = Context.Unit;
			}
			else if (Rule.TargetPolicy == ECombatAIAbilityTargetPolicy::CurrentEnemy && KnownTarget
				&& KnownTarget->Unit.IsValid() && KnownTarget->Life == KnownTarget->Unit->GetLifeGeneration())
			{
				Request.TargetUnit = KnownTarget->Unit.Get();
			}
			else
			{
				continue;
			}
		}
		else if (AbilityData->BehaviorTags.HasTagExact(CombatTags::Ability_Behavior_PointTarget))
		{
			if (Rule.TargetPolicy != ECombatAIAbilityTargetPolicy::CurrentEnemyLocation || !KnownTarget
				|| !KnownTarget->Unit.IsValid() || !KnownTarget->bVisible
				|| KnownTarget->Life != KnownTarget->Unit->GetLifeGeneration())
			{
				continue;
			}
			Request.Type = ECombatOrderType::CastPoint;
			Request.TargetLocation = KnownTarget->LastSeenPosition;
			Request.bHasTargetLocation = true;
			EffectiveTargets = FMath::Max(1, Knowledge.Candidates.Num());
		}
		else
		{
			continue;
		}

		if (EffectiveTargets < Rule.MinimumEffectiveTargets)
		{
			continue;
		}
		const float CurrentMana = AbilitySystem->GetNumericAttribute(UCombatAttributeSet::GetManaAttribute());
		const float MaxMana = AbilitySystem->GetNumericAttribute(UCombatAttributeSet::GetMaxManaAttribute());
		const float ManaCost = AbilityData->GetSpecialValue(TEXT("mana_cost"), Spec->Level);
		if (!FMath::IsFinite(CurrentMana) || !FMath::IsFinite(MaxMana) || !FMath::IsFinite(ManaCost)
			|| MaxMana <= 0.0f || CurrentMana - ManaCost + KINDA_SMALL_NUMBER < MaxMana * Rule.ManaReserveRatio)
		{
			continue;
		}
		if (!Orders->PreflightAIOrder(Request).bSuccess)
		{
			continue;
		}

		float NeedUtility = 0.0f;
		if (Rule.IntentRole == ECombatAIAbilityIntentRole::Heal || Rule.IntentRole == ECombatAIAbilityIntentRole::Defense)
		{
			const float Health = AbilitySystem->GetNumericAttribute(UCombatAttributeSet::GetHealthAttribute());
			const float MaxHealth = AbilitySystem->GetNumericAttribute(UCombatAttributeSet::GetMaxHealthAttribute());
			if (FMath::IsFinite(Health) && FMath::IsFinite(MaxHealth) && MaxHealth > 0.0f)
			{
				NeedUtility = 1.0f - FMath::Clamp(Health / MaxHealth, 0.0f, 1.0f);
			}
			else if (Rule.IntentRole == ECombatAIAbilityIntentRole::Heal)
			{
				continue;
			}
			// 治疗必须存在真实生命缺口；防御用途仍可在满血时按基础效用主动使用。
			if (Rule.IntentRole == ECombatAIAbilityIntentRole::Heal && NeedUtility <= KINDA_SMALL_NUMBER)
			{
				continue;
			}
		}
		const float Score = FCombatAIUtilityScoring::ClampScore(
			Rule.BaseUtility + NeedUtility * (1.0f - Rule.BaseUtility));
		if (TacticalSnapshot.BestAbility.bValid && Score <= TacticalSnapshot.BestAbility.FinalUtility)
		{
			continue;
		}

		FCombatAIAbilityCandidate Candidate;
		Candidate.AbilityDefinitionId = Rule.AbilityDefinitionId;
		Candidate.SpecHandle = Spec->Handle;
		Candidate.RuleIndex = RuleIndex;
		Candidate.OrderType = Request.Type;
		Candidate.IntentRole = Rule.IntentRole;
		Candidate.InterruptPreference = Rule.InterruptPreference;
		Candidate.Target = Request.TargetUnit;
		Candidate.TargetLife = Request.TargetUnit ? Request.TargetUnit->GetLifeGeneration() : 0;
		Candidate.TargetLocation = Request.TargetLocation;
		Candidate.bHasTargetLocation = Request.bHasTargetLocation;
		Candidate.bValid = true;
		Candidate.BaseUtility = Rule.BaseUtility;
		Candidate.NeedUtility = NeedUtility;
		Candidate.FinalUtility = Score;
		TacticalSnapshot.BestAbility = MoveTemp(Candidate);
	}
	return TacticalSnapshot.BestAbility.bValid;
}

bool UCombatAIBrainComponent::IsTacticalActionAvailable(const ECombatAITacticalAction Action) const
{
	if (!Profile || !Profile->IsTacticsEnabled() || !CanSubmit(ControlEpoch)) return false;
	switch (Action)
	{
	case ECombatAITacticalAction::Guard: return true;
	case ECombatAITacticalAction::Attack: return CanEngageKnownTarget();
	case ECombatAITacticalAction::Cast: return TacticalSnapshot.BestAbility.bValid;
	case ECombatAITacticalAction::Reposition:
	{
		if (!bHasAssignment || !Profile->bEnablePerception || !Profile->TacticalLocationQuery
			|| Profile->RepositionTriggerDistance <= 0.0f || NeedsReturn() || !Context.Unit
			|| (RepositionFailureAssignmentRevision == Context.ObjectiveRevision
				&& RepositionFailureSnapshotRevision == Knowledge.Revision)) return false;
		const UCombatOrderComponent* Orders = Context.Unit->GetCombatOrderComponent();
		const FCombatAIKnownTarget* Target = SelectKnownTarget();
		return Orders && !Orders->MoveDestinationQuery && Target && Target->Unit.IsValid() && Target->bVisible
			&& Target->Life == Target->Unit->GetLifeGeneration()
			&& FVector::Dist2D(Context.Unit->GetActorLocation(), Target->LastSeenPosition)
				<= Profile->RepositionTriggerDistance;
	}
	}
	return false;
}

float UCombatAIBrainComponent::GetTacticalActionScore(const ECombatAITacticalAction Action) const
{
	return FCombatAIUtilityScoring::ClampScore(TacticalSnapshot.GetScore(Action));
}

bool UCombatAIBrainComponent::PrepareTacticalOrder(const ECombatAITacticalAction Action, const FName Slot,
	const uint64 Producer)
{
	if (Action == ECombatAITacticalAction::Attack)
	{
		return PrepareRoleOrder(ECombatAIRoleOperation::Attack, Slot, Producer);
	}
	if (Action != ECombatAITacticalAction::Cast || !TacticalSnapshot.BestAbility.bValid)
	{
		return false;
	}
	const FCombatAIAbilityCandidate Candidate = TacticalSnapshot.BestAbility;
	if (Candidate.Target.IsValid() && Candidate.Target->GetLifeGeneration() != Candidate.TargetLife)
	{
		return false;
	}
	return PrepareRequest(Scope, Slot, Producer, Candidate.MakeOrderRequest(), ECombatAIRoleOperation::Cast);
}

bool UCombatAIBrainComponent::BeginTacticalAction(const FCombatAIDecisionScope Expected,
	const FName ConsumerSlot, const uint64 Activation, const ECombatAITacticalAction Action)
{
	if (!BeginAction(Expected, ConsumerSlot, Activation))
	{
		return false;
	}
	// Execute 是 Cast/Attack 分支共用的单写入节点；以已冻结意图为准，不能依赖兄弟分支实例数据传值。
	ActiveTacticalAction = ActionIntent.Operation == ECombatAIRoleOperation::Attack
		? ECombatAITacticalAction::Attack
		: ActionIntent.Operation == ECombatAIRoleOperation::Cast ? ECombatAITacticalAction::Cast : Action;
	TacticalActionStartedAt = GetWorld()->GetTimeSeconds();
	return true;
}

EStateTreeRunStatus UCombatAIBrainComponent::PollTacticalAction(const uint64 Activation)
{
	if (ActiveTacticalAction != ECombatAITacticalAction::Attack)
	{
		return PollAction(Activation);
	}
	const EStateTreeRunStatus RoleStatus = PollRoleAction(Activation);
	if (RoleStatus != EStateTreeRunStatus::Running || !Profile)
	{
		return RoleStatus;
	}

	EvaluateTacticalCandidates();
	auto* Orders = Context.Unit->GetCombatOrderComponent();
	const bool bWantsCast = TacticalSnapshot.BestAbility.bValid
		&& TacticalSnapshot.BestAbility.InterruptPreference == ECombatAIInterruptPreference::AttackBoundary
		&& FCombatAIUtilityScoring::ShouldSwitch(Profile->AttackUtility,
			TacticalSnapshot.BestAbility.FinalUtility, GetWorld()->GetTimeSeconds() - TacticalActionStartedAt,
			Profile->ActionMinHoldSeconds, Profile->ActionSwitchMargin);
	if (!bWantsCast)
	{
		if (Boundary.IsValid()) Orders->ReleaseExecutionBoundary(Boundary);
		Boundary = {};
		return EStateTreeRunStatus::Running;
	}

	if (!Orders->IsExecutionBoundaryReady(Boundary))
	{
		Boundary = Orders->RequestExecutionBoundary(ActionOrder, Profile->BoundaryHoldSeconds);
		return EStateTreeRunStatus::Running;
	}
	// Ready 通知只是一次复核机会；候选在边界期间可能因冷却、资源或目标变化失效。
	EvaluateTacticalCandidates();
	if (!TacticalSnapshot.BestAbility.bValid
		|| TacticalSnapshot.BestAbility.InterruptPreference != ECombatAIInterruptPreference::AttackBoundary
		|| !FCombatAIUtilityScoring::ShouldSwitch(Profile->AttackUtility,
		TacticalSnapshot.BestAbility.FinalUtility, GetWorld()->GetTimeSeconds() - TacticalActionStartedAt,
		Profile->ActionMinHoldSeconds, Profile->ActionSwitchMargin))
	{
		Orders->ReleaseExecutionBoundary(Boundary);
		Boundary = {};
		return EStateTreeRunStatus::Running;
	}
	RoleStopReason = ECombatAIRoleStopReason::TacticalSwitch;
	Orders->CancelCurrentOrderIfMatches(ActionOrder, CombatTags::Order_Failure_Cancelled);
	return Receipt.bValid ? EStateTreeRunStatus::Succeeded : EStateTreeRunStatus::Running;
}

bool UCombatAIBrainComponent::ResolveTacticalReceipt()
{
	if (!Receipt.bValid)
	{
		return false;
	}
	if (Receipt.Operation == ECombatAIRoleOperation::Attack)
	{
		return ResolveRoleReceipt();
	}

	const FCombatAICompletionReceipt Completed = Receipt;
	bool bFailure = false;
	if ((Completed.Operation == ECombatAIRoleOperation::Cast
		|| Completed.Operation == ECombatAIRoleOperation::Reposition)
		&& Completed.ObjectiveRevision == Context.ObjectiveRevision)
	{
		bFailure = !Completed.Result.bSuccess;
		if (bFailure)
		{
			++Failures.FindOrAdd(Completed.Operation);
			RetryOperation = Completed.Operation;
			bRetryPending = true;
			if (Completed.Operation == ECombatAIRoleOperation::Reposition)
			{
				// 同一观察快照的选点或移动失败只计一次；退避结束后也要等新观察再重试。
				RepositionFailureAssignmentRevision = Context.ObjectiveRevision;
				RepositionFailureSnapshotRevision = Knowledge.Revision;
			}
		}
		else
		{
			Failures.Remove(Completed.Operation);
		}
	}
	const bool bResolved = ResolveReceipt(Scope);
	return bResolved && !bFailure;
}
