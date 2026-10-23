#include "Combat/Ability/CombatCooldownPolicy.h"
#include "Combat/Ability/CombatAbilitySystemComponent.h"
#include "Combat/Attributes/CombatAttributeSet.h"
#include "Combat/Modifiers/CombatModifierComponent.h"
#include "Combat/Unit/CombatUnitCharacter.h"
#include "Combat/Core/CombatNumericPolicy.h"

bool UCombatCooldownPolicy::ValidateDefinition(FString& Error) const
{
    if(!FMath::IsFinite(MaxReduction)||MaxReduction<0||MaxReduction>FCombatNumericPolicyV1::MaxReduction)
    {Error=TEXT("冷却缩减上限必须是0到公共上限之间的有限比例");return false;}
    Error.Reset();return true;
}
float UCombatCooldownPolicy::CalculateDuration(UCombatAbilitySystemComponent* Asc,const float BaseDuration) const
{
    FString Error; auto* Unit=Asc?Cast<ACombatUnitCharacter>(Asc->GetAvatarActor()):nullptr;
    if(!IsValid(Unit)||!Unit->HasAuthority()||!FMath::IsFinite(BaseDuration)||BaseDuration<0||!ValidateDefinition(Error))return -1;
    const float Global=FCombatNumericPolicyV1::ClampReduction(Asc->GetNumericAttribute(UCombatAttributeSet::GetCooldownReductionPctAttribute()));
    const float Bonus=BonusParameterKey.IsNone()?0:Unit->GetCombatModifierComponent()->GetActiveRuntimeParameterSum(BonusParameterKey);
    return BaseDuration*(1.f-FMath::Clamp(Global+Bonus,0.f,MaxReduction));
}
