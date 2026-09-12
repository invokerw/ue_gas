#include "Combat/Attributes/CombatAttributeSet.h"

#include "GameplayEffectExtension.h"
#include "Net/UnrealNetwork.h"

#include "Combat/Ability/CombatGameplayEffectContext.h"
#include "Combat/Combat/CombatTransactionSubsystem.h"
#include "Combat/Core/CombatNumericPolicy.h"
#include "Combat/Unit/CombatUnitCharacter.h"

bool FCombatUnitBaseStats::IsValid(FString* OutDiagnostic) const
{
	const float Values[] = {
		MaxHealth, MaxMana, Armor, MagicResist, Evasion, AttackDamage, AttackSpeed, BaseAttackTime,
		AttackRange, MoveSpeed, HealthRegen, ManaRegen, LifestealPct, SpellAmplifyPct,
		CooldownReductionPct, CastRangeBonus, StatusResistancePct, HealAmplifyPct, HealReceivedPct
	};
	for (const float Value : Values)
	{
		if (!FMath::IsFinite(Value) || FMath::Abs(Value) > FCombatNumericPolicyV1::MaxAbsoluteValue)
		{
			if (OutDiagnostic) { *OutDiagnostic = TEXT("Base stats contain a non-finite or over-limit value"); }
			return false;
		}
	}
	const bool bValid = MaxHealth >= 1.0f && MaxHealth <= FCombatNumericPolicyV1::MaxAbsoluteValue
		&& MaxMana >= 1.0f && MaxMana <= FCombatNumericPolicyV1::MaxAbsoluteValue
		&& Armor >= FCombatNumericPolicyV1::MinArmor && Armor <= FCombatNumericPolicyV1::MaxArmor
		&& MagicResist >= FCombatNumericPolicyV1::MinMagicResistance
		&& MagicResist <= FCombatNumericPolicyV1::MaxMagicResistance
		&& Evasion >= 0.0f && Evasion <= 1.0f
		&& AttackDamage >= 0.0f && AttackSpeed >= 0.0f && BaseAttackTime > 0.0f
		&& AttackRange >= 0.0f && MoveSpeed >= 0.0f && HealthRegen >= 0.0f && ManaRegen >= 0.0f
		&& LifestealPct >= 0.0f && LifestealPct <= FCombatNumericPolicyV1::MaxLifesteal
		&& SpellAmplifyPct >= FCombatNumericPolicyV1::MinAmplification
		&& SpellAmplifyPct <= FCombatNumericPolicyV1::MaxAmplification
		&& CooldownReductionPct >= 0.0f && CooldownReductionPct <= FCombatNumericPolicyV1::MaxReduction
		&& StatusResistancePct >= 0.0f && StatusResistancePct <= FCombatNumericPolicyV1::MaxReduction
		&& HealAmplifyPct >= FCombatNumericPolicyV1::MinAmplification
		&& HealAmplifyPct <= FCombatNumericPolicyV1::MaxAmplification
		&& HealReceivedPct >= FCombatNumericPolicyV1::MinAmplification
		&& HealReceivedPct <= FCombatNumericPolicyV1::MaxAmplification;
	if (!bValid && OutDiagnostic)
	{
		*OutDiagnostic = TEXT("Base stats violate positive max/resource/range/timing constraints");
	}
	return bValid;
}

UCombatAttributeSet::UCombatAttributeSet()
{
	InitHealth(100.0f);
	InitMaxHealth(100.0f);
	InitMana(100.0f);
	InitMaxMana(100.0f);
	InitMagicResist(0.25f);
	InitAttackDamage(50.0f);
	InitAttackSpeed(100.0f);
	InitBaseAttackTime(1.7f);
	InitAttackRange(150.0f);
	InitMoveSpeed(300.0f);
}

void UCombatAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);
	// 先把非有限值转成 0，再按属性范围限制，避免异常数值传播到生命、移动或战斗公式。
	if (!FMath::IsFinite(NewValue))
	{
		NewValue = 0.0f;
	}

	if (Attribute == GetHealthAttribute()) { NewValue = FCombatNumericPolicyV1::ClampHealth(NewValue, GetMaxHealth()); }
	else if (Attribute == GetMaxHealthAttribute()) { NewValue = FMath::Clamp(NewValue, 1.0f, FCombatNumericPolicyV1::MaxAbsoluteValue); }
	else if (Attribute == GetManaAttribute()) { NewValue = FCombatNumericPolicyV1::ClampHealth(NewValue, GetMaxMana()); }
	else if (Attribute == GetMaxManaAttribute()) { NewValue = FMath::Clamp(NewValue, 1.0f, FCombatNumericPolicyV1::MaxAbsoluteValue); }
	else if (Attribute == GetArmorAttribute()) { NewValue = FCombatNumericPolicyV1::ClampArmor(NewValue); }
	else if (Attribute == GetMagicResistAttribute()) { NewValue = FCombatNumericPolicyV1::ClampMagicResistance(NewValue); }
	else if (Attribute == GetEvasionAttribute()) { NewValue = FCombatNumericPolicyV1::ClampChance(NewValue); }
	else if (Attribute == GetSpellAmplifyPctAttribute() || Attribute == GetHealAmplifyPctAttribute()
		|| Attribute == GetHealReceivedPctAttribute()) { NewValue = FCombatNumericPolicyV1::ClampAmplification(NewValue); }
	else if (Attribute == GetLifestealPctAttribute()) { NewValue = FCombatNumericPolicyV1::ClampLifesteal(NewValue); }
	else if (Attribute == GetCooldownReductionPctAttribute() || Attribute == GetStatusResistancePctAttribute())
	{
		NewValue = FCombatNumericPolicyV1::ClampReduction(NewValue);
	}
	else if (Attribute == GetAttackDamageAttribute() || Attribute == GetAttackSpeedAttribute()
		|| Attribute == GetAttackRangeAttribute() || Attribute == GetMoveSpeedAttribute()
		|| Attribute == GetHealthRegenAttribute() || Attribute == GetManaRegenAttribute()
		|| Attribute == GetBaseAttackTimeAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.0f, FCombatNumericPolicyV1::MaxAbsoluteValue);
	}
	else
	{
		NewValue = FMath::Clamp(NewValue, -FCombatNumericPolicyV1::MaxAbsoluteValue, FCombatNumericPolicyV1::MaxAbsoluteValue);
	}
}

void UCombatAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);
	const FGameplayAttribute ChangedAttribute = Data.EvaluatedData.Attribute;
	if (ChangedAttribute != GetIncomingDamageAttribute() && ChangedAttribute != GetIncomingHealingAttribute())
	{
		return;
	}

	const FGameplayEffectContext* RawContext = Data.EffectSpec.GetContext().Get();
	if (!RawContext || RawContext->GetScriptStruct() != FCombatGameplayEffectContext::StaticStruct())
	{
		SetIncomingDamage(0.0f);
		SetIncomingHealing(0.0f);
		return;
	}
	const FCombatGameplayEffectContext* CombatContext = static_cast<const FCombatGameplayEffectContext*>(RawContext);
	UAbilitySystemComponent* OwningAsc = GetOwningAbilitySystemComponent();
	ACombatUnitCharacter* Unit = OwningAsc ? Cast<ACombatUnitCharacter>(OwningAsc->GetAvatarActor()) : nullptr;
	UCombatTransactionSubsystem* Transactions = Unit && Unit->GetWorld()
		? Unit->GetWorld()->GetSubsystem<UCombatTransactionSubsystem>() : nullptr;
	if (!Transactions)
	{
		SetIncomingDamage(0.0f);
		SetIncomingHealing(0.0f);
		return;
	}

	FCombatTransactionDelta Delta;
	Delta.PreviousHealth = GetHealth();
	if (ChangedAttribute == GetIncomingDamageAttribute())
	{
		const float RequestedDelta = FMath::Max(0.0f, GetIncomingDamage());
		SetIncomingDamage(0.0f);
		Delta.NewHealth = FCombatNumericPolicyV1::ClampHealth(Delta.PreviousHealth - RequestedDelta, GetMaxHealth());
		SetHealth(Delta.NewHealth);
		Delta.AppliedAmount = Delta.PreviousHealth - Delta.NewHealth;
		Delta.bLethal = Delta.PreviousHealth > 0.0f && Delta.NewHealth == 0.0f;
		Transactions->ReportDelta(CombatContext->EventId, ECombatTransactionKind::Damage, Delta);
	}
	else
	{
		const float RequestedDelta = FMath::Max(0.0f, GetIncomingHealing());
		SetIncomingHealing(0.0f);
		Delta.NewHealth = FCombatNumericPolicyV1::ClampHealth(Delta.PreviousHealth + RequestedDelta, GetMaxHealth());
		SetHealth(Delta.NewHealth);
		Delta.AppliedAmount = Delta.NewHealth - Delta.PreviousHealth;
		Transactions->ReportDelta(CombatContext->EventId, ECombatTransactionKind::Heal, Delta);
	}
}

void UCombatAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
#define COMBAT_REPLICATE_ATTRIBUTE(PropertyName) \
	DOREPLIFETIME_CONDITION_NOTIFY(UCombatAttributeSet, PropertyName, COND_None, REPNOTIFY_Always)
	COMBAT_REPLICATE_ATTRIBUTE(Health);
	COMBAT_REPLICATE_ATTRIBUTE(MaxHealth);
	COMBAT_REPLICATE_ATTRIBUTE(Mana);
	COMBAT_REPLICATE_ATTRIBUTE(MaxMana);
	COMBAT_REPLICATE_ATTRIBUTE(Armor);
	COMBAT_REPLICATE_ATTRIBUTE(MagicResist);
	COMBAT_REPLICATE_ATTRIBUTE(Evasion);
	COMBAT_REPLICATE_ATTRIBUTE(AttackDamage);
	COMBAT_REPLICATE_ATTRIBUTE(AttackSpeed);
	COMBAT_REPLICATE_ATTRIBUTE(BaseAttackTime);
	COMBAT_REPLICATE_ATTRIBUTE(AttackRange);
	COMBAT_REPLICATE_ATTRIBUTE(MoveSpeed);
	COMBAT_REPLICATE_ATTRIBUTE(HealthRegen);
	COMBAT_REPLICATE_ATTRIBUTE(ManaRegen);
	COMBAT_REPLICATE_ATTRIBUTE(LifestealPct);
	COMBAT_REPLICATE_ATTRIBUTE(SpellAmplifyPct);
	COMBAT_REPLICATE_ATTRIBUTE(CooldownReductionPct);
	COMBAT_REPLICATE_ATTRIBUTE(CastRangeBonus);
	COMBAT_REPLICATE_ATTRIBUTE(StatusResistancePct);
	COMBAT_REPLICATE_ATTRIBUTE(HealAmplifyPct);
	COMBAT_REPLICATE_ATTRIBUTE(HealReceivedPct);
#undef COMBAT_REPLICATE_ATTRIBUTE
}

// 所有 RepNotify 都使用同一 GAS 宏，把复制值写回对应属性聚合器。
#define COMBAT_DEFINE_ATTRIBUTE_REP_NOTIFY(PropertyName) \
	void UCombatAttributeSet::OnRep_##PropertyName(const FGameplayAttributeData& OldValue) \
	{ \
		GAMEPLAYATTRIBUTE_REPNOTIFY(UCombatAttributeSet, PropertyName, OldValue); \
	}

COMBAT_DEFINE_ATTRIBUTE_REP_NOTIFY(Health)
COMBAT_DEFINE_ATTRIBUTE_REP_NOTIFY(MaxHealth)
COMBAT_DEFINE_ATTRIBUTE_REP_NOTIFY(Mana)
COMBAT_DEFINE_ATTRIBUTE_REP_NOTIFY(MaxMana)
COMBAT_DEFINE_ATTRIBUTE_REP_NOTIFY(Armor)
COMBAT_DEFINE_ATTRIBUTE_REP_NOTIFY(MagicResist)
COMBAT_DEFINE_ATTRIBUTE_REP_NOTIFY(Evasion)
COMBAT_DEFINE_ATTRIBUTE_REP_NOTIFY(AttackDamage)
COMBAT_DEFINE_ATTRIBUTE_REP_NOTIFY(AttackSpeed)
COMBAT_DEFINE_ATTRIBUTE_REP_NOTIFY(BaseAttackTime)
COMBAT_DEFINE_ATTRIBUTE_REP_NOTIFY(AttackRange)
COMBAT_DEFINE_ATTRIBUTE_REP_NOTIFY(MoveSpeed)
COMBAT_DEFINE_ATTRIBUTE_REP_NOTIFY(HealthRegen)
COMBAT_DEFINE_ATTRIBUTE_REP_NOTIFY(ManaRegen)
COMBAT_DEFINE_ATTRIBUTE_REP_NOTIFY(LifestealPct)
COMBAT_DEFINE_ATTRIBUTE_REP_NOTIFY(SpellAmplifyPct)
COMBAT_DEFINE_ATTRIBUTE_REP_NOTIFY(CooldownReductionPct)
COMBAT_DEFINE_ATTRIBUTE_REP_NOTIFY(CastRangeBonus)
COMBAT_DEFINE_ATTRIBUTE_REP_NOTIFY(StatusResistancePct)
COMBAT_DEFINE_ATTRIBUTE_REP_NOTIFY(HealAmplifyPct)
COMBAT_DEFINE_ATTRIBUTE_REP_NOTIFY(HealReceivedPct)

#undef COMBAT_DEFINE_ATTRIBUTE_REP_NOTIFY
