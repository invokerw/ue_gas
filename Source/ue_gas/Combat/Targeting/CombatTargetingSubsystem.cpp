#include "Combat/Targeting/CombatTargetingSubsystem.h"

#include "EngineUtils.h"
#include "Components/CapsuleComponent.h"

#include "Combat/Ability/CombatAbilitySystemComponent.h"
#include "Combat/Attributes/CombatAttributeSet.h"
#include "Combat/Core/CombatNumericPolicy.h"
#include "Combat/Core/CombatTags.h"
#include "Combat/Targeting/CombatTeamSubsystem.h"
#include "Combat/Unit/CombatUnitCharacter.h"

namespace CombatTargetingPrivate
{
	/** 构造包含原因标签和诊断文本的失败结果，位置保持默认值。 */
	FCombatTargetValidationResult Failure(const FGameplayTag& FailureTag, const TCHAR* Diagnostic)
	{
		FCombatTargetValidationResult Result;
		Result.FailureTag = FailureTag;
		Result.Diagnostic = Diagnostic;
		return Result;
	}

	/** 构造包含本次校验位置的成功结果；位置来自当前 World，是否权威取决于调用端。 */
	FCombatTargetValidationResult Success(const FVector& Location)
	{
		FCombatTargetValidationResult Result;
		Result.bValid = true;
		Result.AuthoritativeLocation = Location;
		return Result;
	}
}

FCombatTargetValidationResult UCombatTargetingSubsystem::ValidateAbilityTarget(
	ACombatUnitCharacter* Source,
	const FGameplayTagContainer& BehaviorTags,
	const FCombatTargetingRules& Rules,
	const FCombatAbilityTargetData& TargetData) const
{
	if (!Source || !TargetData.ClientClaimedHitActors.IsEmpty())
	{
		return CombatTargetingPrivate::Failure(CombatTags::Failure_Target_Invalid,
			TEXT("Source is invalid or client supplied an untrusted hit list"));
	}
	const bool bNoTarget = BehaviorTags.HasTagExact(CombatTags::Ability_Behavior_NoTarget);
	const bool bUnitTarget = BehaviorTags.HasTagExact(CombatTags::Ability_Behavior_UnitTarget);
	const bool bPointTarget = BehaviorTags.HasTagExact(CombatTags::Ability_Behavior_PointTarget);
	if (static_cast<int32>(bNoTarget) + static_cast<int32>(bUnitTarget) + static_cast<int32>(bPointTarget) != 1)
	{
		return CombatTargetingPrivate::Failure(CombatTags::Failure_Target_Invalid, TEXT("Ability has an invalid target mode"));
	}
	if (bNoTarget)
	{
		return !TargetData.TargetActor && !TargetData.bHasTargetLocation
			? CombatTargetingPrivate::Success(Source->GetActorLocation())
			: CombatTargetingPrivate::Failure(CombatTags::Failure_Target_Invalid, TEXT("NoTarget ability received TargetData"));
	}
	if (bUnitTarget)
	{
		return !TargetData.bHasTargetLocation
			? ValidateUnitTarget(Source, TargetData.TargetActor, Rules)
			: CombatTargetingPrivate::Failure(CombatTags::Failure_Target_Invalid, TEXT("UnitTarget ability received a point"));
	}
	return !TargetData.TargetActor && TargetData.bHasTargetLocation
		? ValidatePointTarget(Source, TargetData.TargetLocation, Rules)
		: CombatTargetingPrivate::Failure(CombatTags::Failure_Target_Invalid, TEXT("PointTarget ability requires exactly one point"));
}

FCombatTargetValidationResult UCombatTargetingSubsystem::ValidateUnitTarget(
	ACombatUnitCharacter* Source,
	ACombatUnitCharacter* Target,
	const FCombatTargetingRules& Rules) const
{
	return ValidateUnitTargetInternal(Source, Target, Rules, true);
}

FCombatTargetValidationResult UCombatTargetingSubsystem::ValidateUnitTargetInternal(
	ACombatUnitCharacter* Source,
	ACombatUnitCharacter* Target,
	const FCombatTargetingRules& Rules,
	const bool bCheckCastRange) const
{
	using namespace CombatTargetingPrivate;
	if (!Source || !Target || Source->GetWorld() != GetWorld() || Target->GetWorld() != GetWorld())
	{
		return Failure(CombatTags::Failure_Target_Invalid, TEXT("Target actor is invalid or belongs to another World"));
	}
	if (Rules.VisibilityPolicy != ECombatVisibilityPolicy::None)
	{
		return Failure(CombatTags::Failure_ActionUnsupported, TEXT("M3 has no authoritative visibility provider"));
	}
	if (Source == Target && !Rules.bAllowSelf)
	{
		return Failure(CombatTags::Failure_Target_SelfNotAllowed, TEXT("Self target is not allowed"));
	}
	switch (Target->GetLifeState())
	{
	case ECombatLifeState::Dying: return Failure(CombatTags::Failure_Target_Dying, TEXT("Target is Dying"));
	case ECombatLifeState::Dead:
		if (!Rules.bAllowDead) { return Failure(CombatTags::Failure_Target_Dead, TEXT("Target is Dead")); }
		break;
	case ECombatLifeState::Respawning: return Failure(CombatTags::Failure_Target_Respawning, TEXT("Target is Respawning"));
	default: break;
	}
	UCombatAbilitySystemComponent* TargetAsc = Target->GetCombatAbilitySystemComponent();
	if (!TargetAsc)
	{
		return Failure(CombatTags::Failure_Target_Invalid, TEXT("Target has no Combat ASC"));
	}
	if (TargetAsc->HasMatchingGameplayTag(CombatTags::State_OutOfGame))
	{
		return Failure(CombatTags::Failure_Target_OutOfGame, TEXT("Target is OutOfGame"));
	}
	if (!Rules.bAllowUntargetable && TargetAsc->HasMatchingGameplayTag(CombatTags::State_Untargetable))
	{
		return Failure(CombatTags::Failure_Target_Untargetable, TEXT("Target is Untargetable"));
	}
	if (!Rules.bAllowInvulnerable && TargetAsc->HasMatchingGameplayTag(CombatTags::State_Invulnerable))
	{
		return Failure(CombatTags::Failure_Target_Invulnerable, TEXT("Target is Invulnerable"));
	}
	if (!Rules.bAllowMagicImmune && TargetAsc->HasMatchingGameplayTag(CombatTags::State_MagicImmune))
	{
		return Failure(CombatTags::Failure_Target_MagicImmune, TEXT("Target is MagicImmune"));
	}

	UCombatTeamSubsystem* Teams = GetWorld()->GetSubsystem<UCombatTeamSubsystem>();
	if (!Teams || !Source->GetCombatTeamId().IsValid() || !Target->GetCombatTeamId().IsValid())
	{
		return Failure(CombatTags::Failure_Target_TeamInvalid, TEXT("Source or target team is invalid"));
	}
	const ECombatTeamRelation Relation = Teams->GetRelation(Source->GetCombatTeamId(), Target->GetCombatTeamId());
	if (Relation == ECombatTeamRelation::Neutral && !Rules.bAllowNeutralRelation)
	{
		return Failure(CombatTags::Failure_Target_NeutralNotAllowed, TEXT("Neutral relation is not allowed"));
	}
	if (!Teams->IsTargetTeamAllowed(Source->GetCombatTeamId(), Target->GetCombatTeamId(), Rules.TargetTeamTag)
		&& !(Relation == ECombatTeamRelation::Neutral && Rules.bAllowNeutralRelation))
	{
		return Relation == ECombatTeamRelation::Friendly
			? Failure(CombatTags::Failure_Target_FriendlyNotAllowed, TEXT("Friendly target is not allowed"))
			: Failure(CombatTags::Failure_Target_HostileNotAllowed, TEXT("Hostile target is not allowed"));
	}

	if (!FMath::IsFinite(Rules.CastRange) || Rules.CastRange < 0.0f)
	{
		return Failure(CombatTags::Failure_InvalidNumber, TEXT("CastRange is invalid"));
	}
	if (bCheckCastRange)
	{
		const UCombatAbilitySystemComponent* SourceAsc = Source->GetCombatAbilitySystemComponent();
		const float Bonus = SourceAsc
			? SourceAsc->GetNumericAttribute(UCombatAttributeSet::GetCastRangeBonusAttribute()) : 0.0f;
		const float SourceRadius = Source->GetCapsuleComponent()->GetScaledCapsuleRadius();
		const float TargetRadius = Target->GetCapsuleComponent()->GetScaledCapsuleRadius();
		const float EdgeDistance = FMath::Max(0.0f,
			FVector::Dist2D(Source->GetActorLocation(), Target->GetActorLocation()) - SourceRadius - TargetRadius);
		if (EdgeDistance > Rules.CastRange + Bonus + FCombatNumericPolicyV1::RangeToleranceCm)
		{
			return Failure(CombatTags::Failure_Target_OutOfRange, TEXT("Unit target is outside edge cast range"));
		}
	}
	if (Rules.bRequireLineOfSight && !HasLineOfSight(*Source, Target->GetActorLocation(), Target))
	{
		return Failure(CombatTags::Failure_Target_LineOfSightBlocked, TEXT("Unit target LOS is blocked"));
	}
	return Success(Target->GetActorLocation());
}

FCombatTargetValidationResult UCombatTargetingSubsystem::ValidatePointTarget(
	ACombatUnitCharacter* Source,
	const FVector TargetLocation,
	const FCombatTargetingRules& Rules) const
{
	using namespace CombatTargetingPrivate;
	if (!Source || Source->GetWorld() != GetWorld() || TargetLocation.ContainsNaN()
		|| !FMath::IsFinite(TargetLocation.X) || !FMath::IsFinite(TargetLocation.Y) || !FMath::IsFinite(TargetLocation.Z))
	{
		return Failure(CombatTags::Failure_Target_LocationInvalid, TEXT("Point target location is not finite"));
	}
	if (Rules.VisibilityPolicy != ECombatVisibilityPolicy::None)
	{
		return Failure(CombatTags::Failure_ActionUnsupported, TEXT("M3 has no authoritative visibility provider"));
	}
	if (!FMath::IsFinite(Rules.CastRange) || Rules.CastRange < 0.0f)
	{
		return Failure(CombatTags::Failure_InvalidNumber, TEXT("CastRange is invalid"));
	}
	const UCombatAbilitySystemComponent* SourceAsc = Source->GetCombatAbilitySystemComponent();
	const float Bonus = SourceAsc
		? SourceAsc->GetNumericAttribute(UCombatAttributeSet::GetCastRangeBonusAttribute()) : 0.0f;
	const float SourceRadius = Source->GetCapsuleComponent()->GetScaledCapsuleRadius();
	const float EdgeDistance = FMath::Max(0.0f, FVector::Dist2D(Source->GetActorLocation(), TargetLocation) - SourceRadius);
	if (EdgeDistance > Rules.CastRange + Bonus + FCombatNumericPolicyV1::RangeToleranceCm)
	{
		return Failure(CombatTags::Failure_Target_OutOfRange, TEXT("Point target is outside edge cast range"));
	}
	if (Rules.bRequireLineOfSight && !HasLineOfSight(*Source, TargetLocation, nullptr))
	{
		return Failure(CombatTags::Failure_Target_LineOfSightBlocked, TEXT("Point target LOS is blocked"));
	}
	return Success(TargetLocation);
}

TArray<ACombatUnitCharacter*> UCombatTargetingSubsystem::QueryUnitsInRadius(
	ACombatUnitCharacter* Source,
	const FVector Center,
	const float Radius,
	const FCombatTargetingRules& Rules) const
{
	TArray<ACombatUnitCharacter*> Result;
	if (!Source || Center.ContainsNaN() || !FMath::IsFinite(Radius) || Radius < 0.0f)
	{
		return Result;
	}
	TSet<TWeakObjectPtr<ACombatUnitCharacter>> Seen;
	for (TActorIterator<ACombatUnitCharacter> It(GetWorld()); It; ++It)
	{
		ACombatUnitCharacter* Candidate = *It;
		if (!Candidate || Seen.Contains(Candidate))
		{
			continue;
		}
		const float CandidateRadius = Candidate->GetCapsuleComponent()->GetScaledCapsuleRadius();
		if (FVector::Dist2D(Center, Candidate->GetActorLocation()) > Radius + CandidateRadius)
		{
			continue;
		}
		if (ValidateUnitTargetInternal(Source, Candidate, Rules, false).bValid)
		{
			Seen.Add(Candidate);
			Result.Add(Candidate);
		}
	}
	Result.Sort([](const ACombatUnitCharacter& Left, const ACombatUnitCharacter& Right)
	{
		return Left.GetUniqueID() < Right.GetUniqueID();
	});
	return Result;
}

TArray<ACombatUnitCharacter*> UCombatTargetingSubsystem::QueryUnitsAlongSegment(
	ACombatUnitCharacter* Source,
	const FVector Start,
	const FVector End,
	const float HalfWidth,
	const FCombatTargetingRules& Rules) const
{
	TArray<ACombatUnitCharacter*> Result;
	if (!Source || Start.ContainsNaN() || End.ContainsNaN()
		|| !FMath::IsFinite(HalfWidth) || HalfWidth < 0.0f)
	{
		return Result;
	}
	const FVector2D Start2D(Start.X, Start.Y);
	const FVector2D End2D(End.X, End.Y);
	const FVector2D Segment = End2D - Start2D;
	const double SegmentLengthSquared = Segment.SquaredLength();
	if (SegmentLengthSquared <= UE_DOUBLE_SMALL_NUMBER)
	{
		return QueryUnitsInRadius(Source, Start, HalfWidth, Rules);
	}

	struct FLineCandidate
	{
		ACombatUnitCharacter* Unit = nullptr;
		double Along = 0.0;
	};
	TArray<FLineCandidate> Candidates;
	TSet<TWeakObjectPtr<ACombatUnitCharacter>> Seen;
	for (TActorIterator<ACombatUnitCharacter> It(GetWorld()); It; ++It)
	{
		ACombatUnitCharacter* Candidate = *It;
		if (!Candidate || Seen.Contains(Candidate)
			|| !ValidateUnitTargetInternal(Source, Candidate, Rules, false).bValid)
		{
			continue;
		}
		const FVector Location = Candidate->GetActorLocation();
		const FVector2D ToCandidate(Location.X - Start.X, Location.Y - Start.Y);
		const double Along = FMath::Clamp(FVector2D::DotProduct(ToCandidate, Segment) / SegmentLengthSquared, 0.0, 1.0);
		const FVector2D Closest = Start2D + Segment * Along;
		const float CandidateRadius = Candidate->GetCapsuleComponent()->GetScaledCapsuleRadius();
		if (FVector2D::Distance(FVector2D(Location.X, Location.Y), Closest) > HalfWidth + CandidateRadius)
		{
			continue;
		}
		Seen.Add(Candidate);
		Candidates.Add({ Candidate, Along });
	}
	Candidates.Sort([](const FLineCandidate& Left, const FLineCandidate& Right)
	{
		return !FMath::IsNearlyEqual(Left.Along, Right.Along, 1.0e-6)
			? Left.Along < Right.Along
			: Left.Unit->GetUniqueID() < Right.Unit->GetUniqueID();
	});
	for (const FLineCandidate& Candidate : Candidates)
	{
		Result.Add(Candidate.Unit);
	}
	return Result;
}

bool UCombatTargetingSubsystem::HasLineOfSight(
	ACombatUnitCharacter& Source,
	const FVector& AimPoint,
	const AActor* TargetToIgnore) const
{
	FCollisionQueryParams Params(SCENE_QUERY_STAT(CombatTargetingLOS), false, &Source);
	if (TargetToIgnore)
	{
		Params.AddIgnoredActor(TargetToIgnore);
	}
	FHitResult Hit;
	// 使用发布契约固定的 CombatTargeting 通道；Foundation 自动化负责检测配置映射漂移。
	return !GetWorld()->LineTraceSingleByChannel(
		Hit, Source.GetActorLocation(), AimPoint, ECC_GameTraceChannel4, Params);
}
