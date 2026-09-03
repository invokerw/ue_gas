#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"

#include "CombatAttributeSet.generated.h"

/** 为 AttributeSet 属性生成 GAS 标准 getter、setter 与 FGameplayAttribute 访问器。 */
#define COMBAT_ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)

/** UnitData 保存并在服务器初始化时写入 ASC 的基础战斗数值。 */
USTRUCT(BlueprintType)
struct UE_GAS_API FCombatUnitBaseStats
{
	GENERATED_BODY()

	/** 初始与最大生命值。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Attributes") float MaxHealth = 100.0f;
	/** 初始与最大法力值。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Attributes") float MaxMana = 100.0f;
	/** 物理护甲。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Attributes") float Armor = 0.0f;
	/** 魔法抗性比例。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Attributes") float MagicResist = 0.25f;
	/** 闪避概率。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Attributes") float Evasion = 0.0f;
	/** 基础攻击伤害。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Attributes") float AttackDamage = 50.0f;
	/** 攻击速度属性。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Attributes") float AttackSpeed = 100.0f;
	/** 基础攻击间隔，单位为秒。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Attributes") float BaseAttackTime = 1.7f;
	/** 攻击距离，单位为厘米。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Attributes") float AttackRange = 150.0f;
	/** 地面移动速度，单位为厘米/秒。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Attributes") float MoveSpeed = 300.0f;
	/** 每秒生命恢复。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Attributes") float HealthRegen = 0.0f;
	/** 每秒法力恢复。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Attributes") float ManaRegen = 0.0f;
	/** 按真实伤害计算的吸血比例。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Attributes") float LifestealPct = 0.0f;
	/** 技能伤害增幅比例。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Attributes") float SpellAmplifyPct = 0.0f;
	/** 冷却缩减比例。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Attributes") float CooldownReductionPct = 0.0f;
	/** 施法距离加成，单位为厘米。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Attributes") float CastRangeBonus = 0.0f;
	/** 可抵抗 Debuff 的持续时间缩减比例。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Attributes") float StatusResistancePct = 0.0f;
	/** 治疗来源提供的增幅比例。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Attributes") float HealAmplifyPct = 0.0f;
	/** 治疗目标接受的增幅比例。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Attributes") float HealReceivedPct = 0.0f;

	/** 检查全部基础数值是否有限并满足 Numeric Policy v1。 */
	bool IsValid(FString* OutDiagnostic = nullptr) const;
};

/**
 * Unit 战斗数值的唯一 GAS 属性集合，集中保存并复制资源、攻击、防御和施法相关聚合值。
 * Damage/Heal 只通过瞬时元属性进入；执行回调应用数值策略、写入真实 Health delta，并把结果同步回报给对应战斗事务。
 */
UCLASS()
class UE_GAS_API UCombatAttributeSet : public UAttributeSet
{
	GENERATED_BODY()

public:
	UCombatAttributeSet();

	/** 在聚合值即将变化时应用 Numeric Policy v1 的有限值与区间约束。 */
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
	/** 在 Instant GE 执行后消费 Damage/Heal 元属性并回报真实 Health delta。 */
	virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** 当前生命值，限制在 0..MaxHealth。 */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_Health, Category="Combat|Attributes") FGameplayAttributeData Health;
	COMBAT_ATTRIBUTE_ACCESSORS(UCombatAttributeSet, Health)
	/** 最大生命值，限制在 1..1e9。 */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_MaxHealth, Category="Combat|Attributes") FGameplayAttributeData MaxHealth;
	COMBAT_ATTRIBUTE_ACCESSORS(UCombatAttributeSet, MaxHealth)
	/** 当前法力值，限制在 0..MaxMana。 */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_Mana, Category="Combat|Attributes") FGameplayAttributeData Mana;
	COMBAT_ATTRIBUTE_ACCESSORS(UCombatAttributeSet, Mana)
	/** 最大法力值，限制在 1..1e9。 */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_MaxMana, Category="Combat|Attributes") FGameplayAttributeData MaxMana;
	COMBAT_ATTRIBUTE_ACCESSORS(UCombatAttributeSet, MaxMana)
	/** 物理伤害使用的护甲值。 */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_Armor, Category="Combat|Attributes") FGameplayAttributeData Armor;
	COMBAT_ATTRIBUTE_ACCESSORS(UCombatAttributeSet, Armor)
	/** 魔法伤害使用的抗性比例。 */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_MagicResist, Category="Combat|Attributes") FGameplayAttributeData MagicResist;
	COMBAT_ATTRIBUTE_ACCESSORS(UCombatAttributeSet, MagicResist)
	/** 普攻命中使用的闪避概率。 */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_Evasion, Category="Combat|Attributes") FGameplayAttributeData Evasion;
	COMBAT_ATTRIBUTE_ACCESSORS(UCombatAttributeSet, Evasion)
	/** 普攻基础伤害。 */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_AttackDamage, Category="Combat|Attributes") FGameplayAttributeData AttackDamage;
	COMBAT_ATTRIBUTE_ACCESSORS(UCombatAttributeSet, AttackDamage)
	/** 普攻攻速属性。 */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_AttackSpeed, Category="Combat|Attributes") FGameplayAttributeData AttackSpeed;
	COMBAT_ATTRIBUTE_ACCESSORS(UCombatAttributeSet, AttackSpeed)
	/** 普攻基础间隔。 */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_BaseAttackTime, Category="Combat|Attributes") FGameplayAttributeData BaseAttackTime;
	COMBAT_ATTRIBUTE_ACCESSORS(UCombatAttributeSet, BaseAttackTime)
	/** 普攻边缘距离。 */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_AttackRange, Category="Combat|Attributes") FGameplayAttributeData AttackRange;
	COMBAT_ATTRIBUTE_ACCESSORS(UCombatAttributeSet, AttackRange)
	/** CharacterMovement 投影使用的移动速度。 */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_MoveSpeed, Category="Combat|Attributes") FGameplayAttributeData MoveSpeed;
	COMBAT_ATTRIBUTE_ACCESSORS(UCombatAttributeSet, MoveSpeed)
	/** 每秒生命恢复。 */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_HealthRegen, Category="Combat|Attributes") FGameplayAttributeData HealthRegen;
	COMBAT_ATTRIBUTE_ACCESSORS(UCombatAttributeSet, HealthRegen)
	/** 每秒法力恢复。 */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_ManaRegen, Category="Combat|Attributes") FGameplayAttributeData ManaRegen;
	COMBAT_ATTRIBUTE_ACCESSORS(UCombatAttributeSet, ManaRegen)
	/** 按真实伤害触发的吸血比例。 */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_LifestealPct, Category="Combat|Attributes") FGameplayAttributeData LifestealPct;
	COMBAT_ATTRIBUTE_ACCESSORS(UCombatAttributeSet, LifestealPct)
	/** 技能伤害增幅比例。 */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_SpellAmplifyPct, Category="Combat|Attributes") FGameplayAttributeData SpellAmplifyPct;
	COMBAT_ATTRIBUTE_ACCESSORS(UCombatAttributeSet, SpellAmplifyPct)
	/** 冷却缩减比例。 */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_CooldownReductionPct, Category="Combat|Attributes") FGameplayAttributeData CooldownReductionPct;
	COMBAT_ATTRIBUTE_ACCESSORS(UCombatAttributeSet, CooldownReductionPct)
	/** 施法距离加成。 */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_CastRangeBonus, Category="Combat|Attributes") FGameplayAttributeData CastRangeBonus;
	COMBAT_ATTRIBUTE_ACCESSORS(UCombatAttributeSet, CastRangeBonus)
	/** 状态抗性比例。 */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_StatusResistancePct, Category="Combat|Attributes") FGameplayAttributeData StatusResistancePct;
	COMBAT_ATTRIBUTE_ACCESSORS(UCombatAttributeSet, StatusResistancePct)
	/** 治疗来源增幅比例。 */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_HealAmplifyPct, Category="Combat|Attributes") FGameplayAttributeData HealAmplifyPct;
	COMBAT_ATTRIBUTE_ACCESSORS(UCombatAttributeSet, HealAmplifyPct)
	/** 治疗目标接受增幅比例。 */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_HealReceivedPct, Category="Combat|Attributes") FGameplayAttributeData HealReceivedPct;
	COMBAT_ATTRIBUTE_ACCESSORS(UCombatAttributeSet, HealReceivedPct)

	/** DamageSubsystem 写入的瞬时元属性，不参与复制。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Attributes|Meta") FGameplayAttributeData IncomingDamage;
	COMBAT_ATTRIBUTE_ACCESSORS(UCombatAttributeSet, IncomingDamage)
	/** HealSubsystem 写入的瞬时元属性，不参与复制。 */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Attributes|Meta") FGameplayAttributeData IncomingHealing;
	COMBAT_ATTRIBUTE_ACCESSORS(UCombatAttributeSet, IncomingHealing)

protected:
	/** 各复制属性使用 GAS 标准 RepNotify 更新聚合器。 */
	UFUNCTION() void OnRep_Health(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_MaxHealth(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_Mana(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_MaxMana(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_Armor(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_MagicResist(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_Evasion(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_AttackDamage(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_AttackSpeed(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_BaseAttackTime(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_AttackRange(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_MoveSpeed(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_HealthRegen(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_ManaRegen(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_LifestealPct(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_SpellAmplifyPct(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_CooldownReductionPct(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_CastRangeBonus(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_StatusResistancePct(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_HealAmplifyPct(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_HealReceivedPct(const FGameplayAttributeData& OldValue);
};

#undef COMBAT_ATTRIBUTE_ACCESSORS
