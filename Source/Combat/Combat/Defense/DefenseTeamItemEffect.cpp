#include "Combat/Defense/DefenseTeamItemEffect.h"
#include "Combat/Defense/DefenseDirectorComponent.h"
#include "Combat/Defense/DefenseGameMode.h"
#include "Combat/Defense/DefenseCoreShieldRuntime.h"
#include "Combat/Defense/DefenseConditionalDamageRuntime.h"
#include "Combat/Items/CombatItemData.h"
#include "Combat/Modifiers/CombatModifierRuntime.h"
#include "Combat/Attributes/CombatAttributeSet.h"
#include "Combat/Unit/CombatUnitCharacter.h"
#include "Combat/Core/CombatTags.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "CombatPlayerController.h"

namespace DefenseTeamItem
{
	/** 正常地图从 GameMode 定位；独立宿主也可使用公开的 Director 组件，但必须同 World 且已初始化。 */
	UDefenseDirectorComponent* Director(ACombatUnitCharacter* Unit)
	{
		if(!IsValid(Unit)||!Unit->GetWorld()||Unit->GetNetMode()==NM_Client) return nullptr;
		if(auto* Mode=Unit->GetWorld()->GetAuthGameMode<ADefenseGameMode>()) return Mode->GetDefenseDirector();
		for(TActorIterator<AActor> It(Unit->GetWorld());It;++It)
			if(auto* D=It->FindComponentByClass<UDefenseDirectorComponent>();D&&D->GetFrozenRunData()) return D;
		return nullptr;
	}
}
bool UDefenseTeamItemEffect::ValidateDefinition(FString& Error) const
{
	Error=TEXT("团队效果必须且只能配置英雄属性、固定修复、比例修复或核心屏障或援军之一");
	if(!FMath::IsFinite(ObjectiveShieldFraction)||ObjectiveShieldFraction<0||ObjectiveShieldFraction>1
		||!FMath::IsFinite(ObjectiveHealFraction)||ObjectiveHealFraction<0||ObjectiveHealFraction>1) return false;
	const int32 Modes=int32(ModifierData!=nullptr)+int32(ObjectiveHeal>0)+int32(ObjectiveHealFraction>0)+int32(CoreShieldData!=nullptr)+int32(ReinforcementShieldData!=nullptr);
	if(!FMath::IsFinite(RespawnReduction)||RespawnReduction<0||RespawnReduction>60
		||!FMath::IsFinite(RespawnShieldFraction)||RespawnShieldFraction<0||RespawnShieldFraction>1
		||ReinforcementDeaths<0||ReinforcementDeaths>8) return false;
	if(ReinforcementShieldData)
	{
		if(ReinforcementDeaths<1||RespawnReduction<=0||RespawnShieldFraction<=0) return false;
	}
	else if(ReinforcementDeaths||RespawnReduction!=0||RespawnShieldFraction!=0) return false;
	if(Modes!=1 || (CoreShieldData!=nullptr)!=(ObjectiveShieldFraction>0)) return false;
	if(AllowedPhases.IsEmpty()||AllowedPhases.Num()>3||DurationWaves<0||DurationWaves>6||ObjectiveHeal<0||ObjectiveHeal>100000) return false;
	TSet<EDefensePhase> Seen;
	for(auto P:AllowedPhases)
	{
		if((P!=EDefensePhase::Preparation&&P!=EDefensePhase::Intermission&&P!=EDefensePhase::BossPreparation)||Seen.Contains(P)) return false;
		Seen.Add(P);
	}
	if(ReinforcementShieldData)
	{
		const auto* M=ReinforcementShieldData.Get();
		if(DurationWaves!=0||!M->GetPrimaryAssetId().IsValid()||M->SchemaVersion!=FCombatDefinitionRegistry::CombatContentVersion
			||M->RuntimeClass!=UDefenseShieldRuntime::StaticClass()||M->bIsDebuff||!M->bRemoveOnDeath||M->bDisabledByBreak
			||!FMath::IsFinite(M->Duration)||M->Duration<=0||M->Duration>30||M->ThinkInterval!=0||M->MaxStacks!=1
			||M->DispelRule!=ECombatModifierDispelRule::NotDispellable||!M->GrantedTags.IsEmpty()||!M->RuntimeParameters.IsEmpty()||!M->AttributeChanges.IsEmpty()) return false;
	}
	else if(CoreShieldData)
	{
		const auto* M=CoreShieldData.Get();
		if(DurationWaves!=1||!M->GetPrimaryAssetId().IsValid()||M->SchemaVersion!=FCombatDefinitionRegistry::CombatContentVersion
			||M->RuntimeClass!=UDefenseCoreShieldRuntime::StaticClass()||M->bIsDebuff||!M->bRemoveOnDeath||M->bDisabledByBreak
			||M->Duration!=0||M->ThinkInterval!=0||M->MaxStacks!=1||M->DispelRule!=ECombatModifierDispelRule::NotDispellable
			||!M->GrantedTags.IsEmpty()||!M->RuntimeParameters.IsEmpty()||!M->AttributeChanges.IsEmpty()) return false;
	}
	else if(ModifierData)
	{
		const auto* M=ModifierData.Get();
		if(DurationWaves<1||ObjectiveHeal||!M->GetPrimaryAssetId().IsValid()||M->SchemaVersion!=FCombatDefinitionRegistry::CombatContentVersion||M->bIsDebuff||M->bRemoveOnDeath
			||M->Duration!=0||M->ThinkInterval!=0||M->MaxStacks!=1||M->DispelRule!=ECombatModifierDispelRule::NotDispellable
			||!M->GrantedTags.IsEmpty()) return false;
		const bool bConditional=ConditionalTargetRole!=EDefenseTeamTargetRole::None;
		if(bConditional)
		{
			if(ConditionalDamageFraction<=0||ConditionalDamageFraction>1||M->RuntimeClass!=UDefenseConditionalDamageRuntime::StaticClass()
			||M->AttributeChanges.Num()!=0||M->RuntimeParameters.Num()!=2) return false;
		}
		else if((M->RuntimeClass&&M->RuntimeClass!=UCombatModifierRuntime::StaticClass())||!M->RuntimeParameters.IsEmpty()
			||M->AttributeChanges.IsEmpty()||M->AttributeChanges.Num()>8) return false;
		for(const auto& C:M->AttributeChanges)
		{
			const bool Allowed=C.Attribute==UCombatAttributeSet::GetArmorAttribute()||C.Attribute==UCombatAttributeSet::GetAttackDamageAttribute()
				||C.Attribute==UCombatAttributeSet::GetAttackSpeedAttribute()||C.Attribute==UCombatAttributeSet::GetManaRegenAttribute()
				||C.Attribute==UCombatAttributeSet::GetHealthRegenAttribute()||C.Attribute==UCombatAttributeSet::GetMagicResistAttribute()||C.Attribute==UCombatAttributeSet::GetCooldownReductionPctAttribute();
			if(!Allowed||!FMath::IsFinite(C.Magnitude)||C.Magnitude<=0||C.Magnitude>10000||C.ModifierOp!=EGameplayModOp::Additive||!C.MagnitudeParameterKey.IsNone()) return false;
		}
	}
	else if(DurationWaves!=0) return false;
	Error.Reset(); return true;
}
FCombatShopOfferView UDefenseTeamItemEffect::QueryOffer(ACombatUnitCharacter* Unit,const UCombatItemData* Item) const
{
	if(auto* D=DefenseTeamItem::Director(Unit);D&&Item) return D->BuildTeamItemOffer(Cast<ACombatPlayerController>(Unit->GetResourceOwnerPlayerController()),Item->GetPrimaryAssetId());
	FCombatShopOfferView V; if(Item) V.DefinitionId=Item->GetPrimaryAssetId(); V.RemainingPurchases=0; V.Status=FText::FromString(TEXT("仅防守模式可用")); return V;
}
bool UDefenseTeamItemEffect::CanUse(const FCombatItemAcquireContext& C,FGameplayTag& Failure) const
{
	if(auto* D=DefenseTeamItem::Director(C.Recipient)) return D->CanReceiveTeamItem(C,Failure);
	Failure=CombatTags::Failure_ActionUnsupported; return false;
}
bool UDefenseTeamItemEffect::Use(const FCombatItemAcquireContext& C,FGameplayTag& Failure) const
{
	if(auto* D=DefenseTeamItem::Director(C.Recipient)) return D->ReceiveTeamItem(C,Failure);
	Failure=CombatTags::Failure_ActionUnsupported; return false;
}
