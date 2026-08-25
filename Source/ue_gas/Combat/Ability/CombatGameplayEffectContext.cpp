#include "Combat/Ability/CombatGameplayEffectContext.h"

FGameplayEffectContext* FCombatGameplayEffectContext::Duplicate() const
{
	FCombatGameplayEffectContext* NewContext = new FCombatGameplayEffectContext();
	*NewContext = *this;
	if (const FHitResult* ExistingHit = GetHitResult())
	{
		NewContext->AddHitResult(*ExistingHit, true);
	}
	return NewContext;
}

UScriptStruct* FCombatGameplayEffectContext::GetScriptStruct() const
{
	return StaticStruct();
}

bool FCombatGameplayEffectContext::NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess)
{
	bool bBaseSuccess = false;
	const bool bBaseResult = FGameplayEffectContext::NetSerialize(Ar, Map, bBaseSuccess);

	// AttackHandle 绑定服务器运行时 AttackRecord，不能作为跨网络稳定身份，因此故意不序列化。
	Ar << EventId.Sequence;
	Ar << RootEventId.Sequence;

	uint8 DirectSourceType = static_cast<uint8>(Source.DirectSourceType);
	Ar.SerializeBits(&DirectSourceType, 3);
	if (Ar.IsLoading())
	{
		Source.DirectSourceType = static_cast<ECombatDirectSourceType>(DirectSourceType);
	}

	Ar << Source.AbilityDefinitionId;
	Ar << Source.ModifierDefinitionId;
	Ar << Source.ProjectileDefinitionId;

	bOutSuccess = bBaseSuccess && !Ar.IsError();
	return bBaseResult && bOutSuccess;
}
