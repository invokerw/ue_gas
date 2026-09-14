#include "Combat/Items/CombatItemProcRuntime.h"
#include "Combat/Combat/CombatDamageSubsystem.h"
#include "Combat/Core/CombatTags.h"
#include "Combat/Data/CombatDefinitionData.h"
#include "Combat/Unit/CombatUnitCharacter.h"
#include "Engine/World.h"

void UCombatItemProcRuntime::OnPostDealDamage_Implementation(const FCombatDamageEvent& Event)
{
	ACombatUnitCharacter* Owner = GetTargetUnit();
	if (!IsActive() || !Owner || Event.Source != Owner || !Event.Target || Event.AppliedAmount <= 0
		|| Event.SourceContext.DirectSourceType != ECombatDirectSourceType::Attack || !Event.Context.IsValid()) return;
	const float Bonus = GetRuntimeParameter(TEXT("bonus_damage"));
	if (!FMath::IsFinite(Bonus) || Bonus <= 0) return;
	FCombatDamageRequest Request;
	Request.Source = Owner;
	Request.Target = Event.Target;
	Request.Amount = Bonus;
	Request.DamageType = ECombatDamageType::Magical;
	Request.ParentEvent = Event.Context;
	Request.SourceContext = GetSourceContext();
	Request.SourceContext.DirectSourceType = ECombatDirectSourceType::Item;
	Request.SourceContext.ModifierDefinitionId = GetModifierData()->GetPrimaryAssetId();
	Request.Flags.AddTag(CombatTags::Damage_Flag_NoLifesteal);
	Owner->GetWorld()->GetSubsystem<UCombatDamageSubsystem>()->DealDamage(Request);
}
