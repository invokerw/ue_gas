#include "Combat/Demo/CombatDemoModifierRuntimes.h"

#include "Combat/Combat/CombatDamageSubsystem.h"
#include "Combat/Core/CombatTags.h"
#include "Combat/Data/CombatDefinitionData.h"
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
