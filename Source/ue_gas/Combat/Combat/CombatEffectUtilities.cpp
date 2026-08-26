#include "Combat/Combat/CombatEffectUtilities.h"

#include "GameplayEffect.h"

#include "Combat/Ability/CombatAbilitySystemComponent.h"
#include "Combat/Ability/CombatGameplayEffectContext.h"

bool CombatEffectUtilities::ApplyMetaEffect(
	UObject* EffectOuter,
	UCombatAbilitySystemComponent& TargetAsc,
	UCombatAbilitySystemComponent& SourceAsc,
	const FGameplayAttribute& MetaAttribute,
	const FGameplayTag& SetByCallerTag,
	const float Amount,
	const FCombatEventContext& EventContext,
	const FCombatSourceContext& SourceContext)
{
	if (!EffectOuter || !MetaAttribute.IsValid() || !SetByCallerTag.IsValid() || !FMath::IsFinite(Amount))
	{
		return false;
	}

	UGameplayEffect* EffectDefinition = NewObject<UGameplayEffect>(EffectOuter);
	EffectDefinition->DurationPolicy = EGameplayEffectDurationType::Instant;
	FGameplayModifierInfo& Modifier = EffectDefinition->Modifiers.AddDefaulted_GetRef();
	Modifier.Attribute = MetaAttribute;
	Modifier.ModifierOp = EGameplayModOp::Additive;
	FSetByCallerFloat SetByCaller;
	SetByCaller.DataTag = SetByCallerTag;
	Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(SetByCaller);

	FGameplayEffectContextHandle ContextHandle = SourceAsc.MakeEffectContext();
	FGameplayEffectContext* RawContext = ContextHandle.Get();
	if (!RawContext || RawContext->GetScriptStruct() != FCombatGameplayEffectContext::StaticStruct())
	{
		return false;
	}
	FCombatGameplayEffectContext* CombatContext = static_cast<FCombatGameplayEffectContext*>(RawContext);
	CombatContext->EventId = EventContext.EventId;
	CombatContext->RootEventId = EventContext.RootEventId;
	CombatContext->Source = SourceContext;

	FGameplayEffectSpec EffectSpec(EffectDefinition, ContextHandle, 1.0f);
	EffectSpec.SetSetByCallerMagnitude(SetByCallerTag, Amount);
	return TargetAsc.ApplyGameplayEffectSpecToSelf(EffectSpec).WasSuccessfullyApplied();
}

bool CombatEffectUtilities::ApplyAttributeAdditive(
	UObject* EffectOuter,
	UCombatAbilitySystemComponent& TargetAsc,
	const FGameplayAttribute& Attribute,
	const float Magnitude)
{
	if (!EffectOuter || !Attribute.IsValid() || !FMath::IsFinite(Magnitude))
	{
		return false;
	}
	UGameplayEffect* EffectDefinition = NewObject<UGameplayEffect>(EffectOuter);
	EffectDefinition->DurationPolicy = EGameplayEffectDurationType::Instant;
	FGameplayModifierInfo& Modifier = EffectDefinition->Modifiers.AddDefaulted_GetRef();
	Modifier.Attribute = Attribute;
	Modifier.ModifierOp = EGameplayModOp::Additive;
	Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(Magnitude));
	FGameplayEffectSpec EffectSpec(EffectDefinition, TargetAsc.MakeEffectContext(), 1.0f);
	return TargetAsc.ApplyGameplayEffectSpecToSelf(EffectSpec).WasSuccessfullyApplied();
}

bool CombatEffectUtilities::ApplyAttributeOverrides(
	UObject* EffectOuter,
	UCombatAbilitySystemComponent& TargetAsc,
	const TArray<TPair<FGameplayAttribute, float>>& AttributeValues)
{
	if (!EffectOuter || AttributeValues.IsEmpty())
	{
		return false;
	}
	UGameplayEffect* EffectDefinition = NewObject<UGameplayEffect>(EffectOuter);
	EffectDefinition->DurationPolicy = EGameplayEffectDurationType::Instant;
	for (const TPair<FGameplayAttribute, float>& Pair : AttributeValues)
	{
		if (!Pair.Key.IsValid() || !FMath::IsFinite(Pair.Value))
		{
			return false;
		}
		FGameplayModifierInfo& Modifier = EffectDefinition->Modifiers.AddDefaulted_GetRef();
		Modifier.Attribute = Pair.Key;
		Modifier.ModifierOp = EGameplayModOp::Override;
		Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(Pair.Value));
	}
	FGameplayEffectSpec EffectSpec(EffectDefinition, TargetAsc.MakeEffectContext(), 1.0f);
	return TargetAsc.ApplyGameplayEffectSpecToSelf(EffectSpec).WasSuccessfullyApplied();
}
