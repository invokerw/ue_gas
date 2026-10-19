#include "Combat/Defense/DefenseCoreShieldRuntime.h"
#include "Combat/Core/CombatNumericPolicy.h"
#include "Combat/Unit/CombatUnitCharacter.h"
#include "Combat/Modifiers/CombatModifierComponent.h"
#include "Combat/Attributes/CombatAttributeSet.h"

void UDefenseShieldRuntime::OnCreated_Implementation()
{
	float Amount=GetRuntimeParameter(TEXT("shield_amount"));
	auto* Unit=GetTargetUnit(); const float Fraction=GetRuntimeParameter(TEXT("shield_fraction"));
	if(Unit&&FMath::IsFinite(Fraction)&&Fraction>0&&Fraction<=1) Amount=Unit->GetCombatAttributeSet()->GetMaxHealth()*Fraction;
	float Cap=GetRuntimeParameter(TEXT("shield_cap_fraction")),Existing=0;
	if(Unit)
	{
		TArray<FCombatModifierView> Views; Unit->GetCombatModifierComponent()->BuildModifierViews(Views);
		for(const auto& V:Views) if(!(V.Handle==GetHandle()))
			if(const auto* Shield=Cast<UDefenseShieldRuntime>(Unit->GetCombatModifierComponent()->FindRuntime(V.Handle)))
			{
				Existing+=Shield->GetRemainingShield(); const float Other=Shield->GetRuntimeParameter(TEXT("shield_cap_fraction"));
				if(Other>0&&Other<=1) Cap=Cap>0?FMath::Min(Cap,Other):Other;
			}
		// 旧固定盾不声明上限；新英雄盾的声明约束同一单位后续新建的同族盾，核心旧资产不受影响。
		if(FMath::IsFinite(Cap)&&Cap>0&&Cap<=1) Amount=FMath::Min(Amount,FMath::Max(0.f,Unit->GetCombatAttributeSet()->GetMaxHealth()*Cap-Existing));
	}
	RemainingShield=FCombatNumericPolicyV1::IsValidNonNegativeRequest(Amount)?Amount:0.0f;
}
void UDefenseShieldRuntime::OnDamageBlock_Implementation(FCombatDamageEvent& Event)
{
	if(Event.Amount<=0 || RemainingShield<=0) return;
	// 公共管线已完成免疫、生命流失分流和抗性计算；此处只消耗当前请求实际到达的伤害。
	const float Absorbed=FMath::Min(RemainingShield,Event.Amount);
	RemainingShield-=Absorbed; Event.Amount-=Absorbed; Event.AbsorbedAmount+=Absorbed;
	if(RemainingShield<=0) RequestRemoveSelf();
}
