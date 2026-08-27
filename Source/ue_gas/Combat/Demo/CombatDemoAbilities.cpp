#include "Combat/Demo/CombatDemoAbilities.h"

#include "Combat/Combat/CombatDamageSubsystem.h"
#include "Combat/Core/CombatTags.h"
#include "Combat/Data/CombatDefinitionData.h"
#include "Combat/Demo/CombatFissureBlocker.h"
#include "Combat/Modifiers/CombatModifierComponent.h"
#include "Combat/Motion/CombatMotionComponent.h"
#include "Combat/Targeting/CombatTargetingSubsystem.h"
#include "Combat/Thinker/CombatThinkerSubsystem.h"
#include "Combat/Unit/CombatUnitCharacter.h"

UCombatFissureAbility::UCombatFissureAbility()
{
	BlockerClass = ACombatFissureBlocker::StaticClass();
}

void UCombatFissureAbility::OnGiveAbility(
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilitySpec& Spec)
{
	// InstancedPerActor 在测试动态 CDO 和内容热重载下也显式消费唯一 Class CDO 配置。
	if (const UCombatFissureAbility* AbilityCdo = GetClass()->GetDefaultObject<UCombatFissureAbility>();
		AbilityCdo && AbilityCdo != this)
	{
		StunModifierData = AbilityCdo->StunModifierData;
		BlockerClass = AbilityCdo->BlockerClass;
	}
	Super::OnGiveAbility(ActorInfo, Spec);
}

void UCombatFissureAbility::ReceiveSpellStart_Implementation(
	const FCombatAbilityActivationContext& Context)
{
	LastTargetCount = 0;
	LastMotionCount = 0;
	bVisualThinkerCreated = false;
	bBlockerCreated = false;
	ACombatUnitCharacter* Caster = Context.Caster;
	const UCombatAbilityData* Data = GetAbilityData();
	UWorld* World = Caster ? Caster->GetWorld() : nullptr;
	if (!Caster || !Data || !World)
	{
		return;
	}
	const float Length = FMath::Max(0.0f, GetSpecialValue(TEXT("fissure_length")));
	const float HalfWidth = FMath::Max(0.0f, GetSpecialValue(TEXT("fissure_half_width")));
	const float Damage = FMath::Max(0.0f, GetSpecialValue(TEXT("damage")));
	const float StunDuration = FMath::Max(0.0f, GetSpecialValue(TEXT("stun_duration")));
	const float KnockbackDistance = FMath::Max(0.0f, GetSpecialValue(TEXT("knockback_distance")));
	const float KnockbackSpeed = FMath::Max(0.0f, GetSpecialValue(TEXT("knockback_speed")));
	const float VisualDuration = FMath::Max(0.0f, GetSpecialValue(TEXT("visual_duration")));
	const float BlockerDuration = FMath::Max(0.0f, GetSpecialValue(TEXT("blocker_duration")));
	const float BlockerHeight = FMath::Max(1.0f, GetSpecialValue(TEXT("blocker_height")));
	FVector Direction = (Context.TargetLocation - Caster->GetActorLocation()).GetSafeNormal2D();
	if (Direction.IsNearlyZero())
	{
		Direction = Caster->GetActorForwardVector().GetSafeNormal2D();
	}
	const FVector Start = Caster->GetActorLocation();
	const FVector End = Start + Direction * Length;
	FCombatSourceContext SourceContext;
	SourceContext.DirectSourceType = ECombatDirectSourceType::Ability;
	SourceContext.AbilityDefinitionId = Data->GetPrimaryAssetId();

	UCombatTargetingSubsystem* Targeting = World->GetSubsystem<UCombatTargetingSubsystem>();
	const TArray<ACombatUnitCharacter*> Targets = Targeting
		? Targeting->QueryUnitsAlongSegment(Caster, Start, End, HalfWidth, Data->TargetingRules)
		: TArray<ACombatUnitCharacter*>();
	LastTargetCount = Targets.Num();
	const FVector2D SegmentStart(Start.X, Start.Y);
	const FVector2D SegmentEnd(End.X, End.Y);
	const FVector2D Segment = SegmentEnd - SegmentStart;
	const double SegmentLengthSquared = Segment.SizeSquared();
	for (ACombatUnitCharacter* Target : Targets)
	{
		if (!Target)
		{
			continue;
		}
		FCombatDamageRequest DamageRequest;
		DamageRequest.Source = Caster;
		DamageRequest.Target = Target;
		DamageRequest.Amount = Damage;
		DamageRequest.DamageType = ECombatDamageType::Magical;
		DamageRequest.ParentEvent = Context.EventContext;
		DamageRequest.SourceContext = SourceContext;
		World->GetSubsystem<UCombatDamageSubsystem>()->DealDamage(DamageRequest);
		if (Target->GetLifeState() != ECombatLifeState::Alive)
		{
			continue;
		}
		if (StunModifierData && StunDuration > 0.0f)
		{
			FCombatModifierApplyRequest StunRequest;
			StunRequest.Source = Caster;
			StunRequest.ModifierData = StunModifierData;
			StunRequest.DurationOverride = StunDuration;
			Target->GetCombatModifierComponent()->ApplyModifier(StunRequest);
		}
		if (KnockbackDistance > 0.0f && KnockbackSpeed > 0.0f && SegmentLengthSquared > UE_DOUBLE_SMALL_NUMBER)
		{
			const FVector TargetLocation = Target->GetActorLocation();
			const FVector2D ToTarget(TargetLocation.X - Start.X, TargetLocation.Y - Start.Y);
			const double Along = FMath::Clamp(FVector2D::DotProduct(ToTarget, Segment) / SegmentLengthSquared, 0.0, 1.0);
			const FVector2D Closest = SegmentStart + Segment * Along;
			FVector KnockbackDirection(TargetLocation.X - Closest.X, TargetLocation.Y - Closest.Y, 0.0);
			if (KnockbackDirection.IsNearlyZero())
			{
				KnockbackDirection = FVector(-Direction.Y, Direction.X, 0.0f);
			}
			FCombatMotionRequest MotionRequest;
			MotionRequest.Channel = ECombatMotionChannel::Horizontal;
			MotionRequest.Priority = 50;
			MotionRequest.TargetLocation = TargetLocation + KnockbackDirection.GetSafeNormal2D() * KnockbackDistance;
			MotionRequest.Source = Caster;
			MotionRequest.Speed = KnockbackSpeed;
			MotionRequest.ParentEvent = Context.EventContext;
			MotionRequest.SourceContext = SourceContext;
			LastMotionCount += Target->GetCombatMotionComponent()->TryAcquireMotion(MotionRequest).bSuccess ? 1 : 0;
		}
	}

	if (VisualDuration > 0.0f)
	{
		FCombatThinkerSpec ThinkerSpec;
		ThinkerSpec.Source = Caster;
		ThinkerSpec.Location = (Start + End) * 0.5f;
		ThinkerSpec.Radius = HalfWidth;
		ThinkerSpec.PulseInterval = VisualDuration + 1.0f;
		ThinkerSpec.Duration = VisualDuration;
		ThinkerSpec.bVisualOnly = true;
		ThinkerSpec.TargetingRules = Data->TargetingRules;
		ThinkerSpec.ParentEvent = Context.EventContext;
		ThinkerSpec.SourceContext = SourceContext;
		bVisualThinkerCreated = World->GetSubsystem<UCombatThinkerSubsystem>()->CreateThinker(ThinkerSpec).bSuccess;
	}

	if (BlockerDuration > 0.0f && BlockerClass)
	{
		FActorSpawnParameters Params;
		Params.Owner = Caster;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		// 在 SpawnActor 参数中一次性确定 blocker 几何变换，避免运行时直接旁路移动系统写 Transform。
		const FVector BlockerLocation = (Start + End) * 0.5f;
		const FRotator BlockerRotation = (End - Start).GetSafeNormal2D().Rotation();
		ACombatFissureBlocker* Blocker = World->SpawnActor<ACombatFissureBlocker>(
			BlockerClass, BlockerLocation, BlockerRotation, Params);
		bBlockerCreated = Blocker && Blocker->InitializeBlocker(
			Start, End, HalfWidth, BlockerHeight, BlockerDuration, Context.EventContext, SourceContext);
		if (Blocker && !bBlockerCreated)
		{
			Blocker->Destroy();
		}
	}
}

void UCombatChannelProbeAbility::ReceiveChannelTick_Implementation(
	const FCombatAbilityActivationContext& Context,
	const FCombatScheduledTickContext& TickContext)
{
	(void)Context;
	ObservedTickCount += TickContext.TickCount;
}

void UCombatChannelProbeAbility::ReceiveChannelFinish_Implementation(
	const FCombatAbilityActivationContext& Context,
	const bool bInterrupted)
{
	(void)Context;
	++ObservedFinishCount;
	bLastFinishInterrupted = bInterrupted;
}
