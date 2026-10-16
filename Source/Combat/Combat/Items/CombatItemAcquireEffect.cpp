#include "Combat/Items/CombatItemAcquireEffect.h"
#include "Combat/Items/CombatItemData.h"
#include "Combat/Unit/CombatUnitCharacter.h"
#include "Combat/Combat/CombatHealSubsystem.h"
#include "Combat/Core/CombatTags.h"
#include "Engine/World.h"

bool FCombatShopOfferView::operator==(const FCombatShopOfferView& O) const
{
	return DefinitionId==O.DefinitionId && ContextId==O.ContextId && Revision==O.Revision && bCanPurchase==O.bCanPurchase
		&& RemainingPurchases==O.RemainingPurchases && bEffectActive==O.bEffectActive && ExpireWave==O.ExpireWave && RecipientName==O.RecipientName && Status.EqualTo(O.Status);
}
bool UCombatItemAcquireEffect::ValidateDefinition(FString& Error) const { Error=TEXT("必须配置具体的同步获得效果"); return false; }
FCombatShopOfferView UCombatItemAcquireEffect::QueryOffer(ACombatUnitCharacter* Unit,const UCombatItemData* Definition) const
{
	FCombatShopOfferView V; if(Definition) V.DefinitionId=Definition->GetPrimaryAssetId();
	V.bCanPurchase=IsValid(Unit)&&Unit->HasAuthority()&&Unit->GetLifeState()==ECombatLifeState::Alive;
	V.Status=FText::FromString(V.bCanPurchase?TEXT("获得即使用 · 不占库存"):TEXT("需要存活的接收单位")); return V;
}
bool UCombatItemAcquireEffect::CanUse(const FCombatItemAcquireContext& C,FGameplayTag& Failure) const
{
	Failure={}; if(!QueryOffer(C.Recipient,C.Definition).bCanPurchase) { Failure=CombatTags::Failure_ActionUnsupported; return false; } return true;
}
bool UCombatItemAcquireEffect::Use(const FCombatItemAcquireContext&,FGameplayTag& Failure) const { Failure=CombatTags::Failure_ActionUnsupported; return false; }
bool UCombatHealOnAcquireEffect::ValidateDefinition(FString& Error) const
{
	Error=TEXT("获得治疗量必须在 1 到 100000 之间"); if(!FMath::IsFinite(Amount)||Amount<1||Amount>100000) return false; Error.Reset(); return true;
}
bool UCombatHealOnAcquireEffect::Use(const FCombatItemAcquireContext& C,FGameplayTag& Failure) const
{
	if(!CanUse(C,Failure)) return false;
	FCombatHealRequest R; R.Source=C.Recipient; R.Target=C.Recipient; R.Amount=Amount; R.SourceContext=C.Source;
	const auto Result=C.Recipient->GetWorld()->GetSubsystem<UCombatHealSubsystem>()->Heal(R); Failure=Result.FailureTag; return Result.bSuccess;
}
