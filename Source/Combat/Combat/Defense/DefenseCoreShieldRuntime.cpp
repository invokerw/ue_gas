#include "Combat/Defense/DefenseCoreShieldRuntime.h"
#include "Combat/Core/CombatNumericPolicy.h"

void UDefenseCoreShieldRuntime::OnCreated_Implementation()
{
	const float Amount=GetRuntimeParameter(TEXT("shield_amount"));
	RemainingShield=FCombatNumericPolicyV1::IsValidNonNegativeRequest(Amount)?Amount:0.0f;
}
void UDefenseCoreShieldRuntime::OnDamageBlock_Implementation(FCombatDamageEvent& Event)
{
	if(Event.Amount<=0 || RemainingShield<=0) return;
	// 公共管线已完成免疫、生命流失分流和抗性计算；此处只消耗当前请求实际到达的伤害。
	const float Absorbed=FMath::Min(RemainingShield,Event.Amount);
	RemainingShield-=Absorbed; Event.Amount-=Absorbed; Event.AbsorbedAmount+=Absorbed;
	if(RemainingShield<=0) RequestRemoveSelf();
}
