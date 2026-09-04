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

/**
 * UnitData 中可编辑的出生属性模板；服务器创建单位时一次性写入 CombatAttributeSet。
 * MaxHealth/MaxMana 同时作为当前值初始化，之后的 Modifier、伤害和治疗都只改变 GAS 属性，不回写这份模板。
 * 名称带 Pct 的字段使用小数比例，例如 0.25 表示 25%。
 */
USTRUCT(BlueprintType)
struct UE_GAS_API FCombatUnitBaseStats
{
	GENERATED_BODY()

	/** 初始化 MaxHealth，并把当前 Health 同步设为该值。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Attributes", meta=(ClampMin="1", ClampMax="1000000000", DisplayName="最大生命值", ToolTip="单位初始化后的当前生命和最大生命，范围为 1 到 10 亿。")) float MaxHealth = 100.0f;
	/** 初始化 MaxMana，并把当前 Mana 同步设为该值。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Attributes", meta=(ClampMin="1", ClampMax="1000000000", DisplayName="最大法力值", ToolTip="单位初始化后的当前法力和最大法力，范围为 1 到 10 亿。")) float MaxMana = 100.0f;
	/** 物理护甲。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Attributes", meta=(ClampMin="-10000", ClampMax="10000", DisplayName="物理护甲", ToolTip="物理伤害公式使用的护甲值，允许负护甲；有效范围为 -10000 到 10000。")) float Armor = 0.0f;
	/** 魔法伤害减免比例；0.25 减免 25%，负值会放大伤害。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Attributes", meta=(ClampMin="-1", ClampMax="0.95", DisplayName="魔法抗性比例", ToolTip="魔法伤害结算使用的比例，范围为 -1 到 0.95；例如 0.25 表示减免 25%，-1 表示承受双倍伤害。")) float MagicResist = 0.25f;
	/** 普攻命中时使用的闪避概率，0 到 1；0.25 表示 25%。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Attributes", meta=(ClampMin="0", ClampMax="1", DisplayName="闪避概率", ToolTip="普攻命中时使用的闪避概率，0 到 1；0.25 表示 25%。")) float Evasion = 0.0f;
	/** 基础攻击伤害。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Attributes", meta=(ClampMin="0", DisplayName="基础攻击伤害", ToolTip="普通攻击记录快照的基础伤害，必须为非负值。")) float AttackDamage = 50.0f;
	/** 缩放普通攻击前摇与间隔的攻速值；100 表示使用基础时长。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Attributes", meta=(ClampMin="0", DisplayName="攻击速度", ToolTip="普通攻击前摇和间隔缩放使用的攻速属性；100 表示基础速率，必须为非负值。")) float AttackSpeed = 100.0f;
	/** 基础攻击间隔，单位为秒。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Attributes", meta=(Units="s", DisplayName="基础攻击间隔", ToolTip="AttackSpeed 为 100 时两次普通攻击之间的基础间隔，单位为秒，必须大于 0；最终由资产校验拒绝非法值。")) float BaseAttackTime = 1.7f;
	/** 普攻的基础边缘距离，单位为厘米；判定时还会计入双方碰撞半径。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Attributes", meta=(ClampMin="0", Units="cm", DisplayName="攻击距离", ToolTip="普通攻击的基础边缘距离，单位为厘米；运行时还会考虑双方碰撞半径。")) float AttackRange = 150.0f;
	/** 地面移动速度，单位为厘米/秒。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Attributes", meta=(ClampMin="0", Units="cm/s", DisplayName="地面移动速度", ToolTip="投影到 CharacterMovement 的基础最大地面速度，单位为厘米/秒。")) float MoveSpeed = 300.0f;
	/** 每秒生命恢复。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Attributes", meta=(ClampMin="0", DisplayName="每秒生命恢复", ToolTip="单位每秒恢复的基础生命值，必须为非负值。")) float HealthRegen = 0.0f;
	/** 每秒法力恢复。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Attributes", meta=(ClampMin="0", DisplayName="每秒法力恢复", ToolTip="单位每秒恢复的基础法力值，必须为非负值。")) float ManaRegen = 0.0f;
	/** 按真实伤害计算的吸血比例。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Attributes", meta=(ClampMin="0", ClampMax="10", DisplayName="吸血比例", ToolTip="按实际造成伤害计算的吸血比例，范围为 0 到 10；例如 0.2 表示 20%。")) float LifestealPct = 0.0f;
	/** 非物理伤害的增幅比例，0.25 表示增加 25%；生命移除或禁止技能增幅的伤害跳过该项。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Attributes", meta=(ClampMin="-1", ClampMax="10", DisplayName="技能伤害增幅比例", ToolTip="非物理伤害的增幅比例，0.25 表示增加 25%；生命移除或禁止技能增幅的伤害跳过该项。")) float SpellAmplifyPct = 0.0f;
	/** 技能冷却缩减比例，0.25 将基础 10 秒缩短为 7.5 秒；按数值策略限制上限。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Attributes", meta=(ClampMin="0", ClampMax="0.9", DisplayName="冷却缩减比例", ToolTip="技能冷却缩减比例，0.25 将基础 10 秒缩短为 7.5 秒；按数值策略限制上限。")) float CooldownReductionPct = 0.0f;
	/** 施法距离加成，单位为厘米。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Attributes", meta=(Units="cm", DisplayName="施法距离加成", ToolTip="运行时叠加到技能基础施法范围的距离，单位为厘米；负值会缩短施法范围。")) float CastRangeBonus = 0.0f;
	/** 只缩短明确标记为受状态抗性影响的 Debuff；0.25 会把 10 秒缩短到 7.5 秒。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Attributes", meta=(ClampMin="0", ClampMax="0.9", DisplayName="状态抗性比例", ToolTip="对标记为受状态抗性影响的 Debuff 缩短持续时间，范围为 0 到 0.9。")) float StatusResistancePct = 0.0f;
	/** 施加治疗时的增幅比例；与目标接受增幅相乘，例如双方各 20% 时，100 点治疗变为 144 点。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Attributes", meta=(ClampMin="-1", ClampMax="10", DisplayName="治疗来源增幅比例", ToolTip="施加治疗时的增幅比例；与目标接受增幅相乘，例如双方各 20% 时，100 点治疗变为 144 点。")) float HealAmplifyPct = 0.0f;
	/** 治疗目标接受的增幅比例。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Attributes", meta=(ClampMin="-1", ClampMax="10", DisplayName="受到治疗增幅比例", ToolTip="作为治疗目标时对治疗量应用的增幅比例，范围为 -1 到 10；例如 -0.2 表示减少 20%。")) float HealReceivedPct = 0.0f;

	/** 检查每个字段是否为有限数，并验证生命、比例、速度和时间等项目约束；失败时可返回首条诊断。 */
	bool IsValid(FString* OutDiagnostic = nullptr) const;
};

/**
 * 单位战斗数值的唯一来源，由 GAS 汇总效果后保存并复制生命、法力、攻击、防御和施法属性。
 * 伤害与治疗先写入不复制的临时属性 IncomingDamage/IncomingHealing；执行回调清空临时值、修改生命并向对应事件回报实际变化。
 */
UCLASS()
class UE_GAS_API UCombatAttributeSet : public UAttributeSet
{
	GENERATED_BODY()

public:
	UCombatAttributeSet();

	/** 在聚合值即将变化时应用 Numeric Policy v1 的有限值与区间约束。 */
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
	/** 立即生效的效果执行后，将临时伤害/治疗转换为实际生命变化并回报；临时数值随后清零，不会成为持续累积的属性。 */
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
	/** 普攻命中时读取的闪避概率，限制为 0 到 1；0.25 表示 25%。 */
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
	/** 普攻可达的胶囊边缘距离，单位为厘米；中心距离判定还计入双方胶囊半径。 */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_AttackRange, Category="Combat|Attributes") FGameplayAttributeData AttackRange;
	COMBAT_ATTRIBUTE_ACCESSORS(UCombatAttributeSet, AttackRange)
	/** 同步给 CharacterMovement 的普通移动速度，单位为厘米/秒；强制位移使用自己的请求速度。 */
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
	/** 非物理伤害的增幅比例，0.25 表示增加 25%；生命移除或禁止技能增幅的伤害跳过该项。 */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_SpellAmplifyPct, Category="Combat|Attributes") FGameplayAttributeData SpellAmplifyPct;
	COMBAT_ATTRIBUTE_ACCESSORS(UCombatAttributeSet, SpellAmplifyPct)
	/** 技能冷却缩减比例，0.25 将基础 10 秒缩短为 7.5 秒；按数值策略限制上限。 */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_CooldownReductionPct, Category="Combat|Attributes") FGameplayAttributeData CooldownReductionPct;
	COMBAT_ATTRIBUTE_ACCESSORS(UCombatAttributeSet, CooldownReductionPct)
	/** 施法距离加成。 */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_CastRangeBonus, Category="Combat|Attributes") FGameplayAttributeData CastRangeBonus;
	COMBAT_ATTRIBUTE_ACCESSORS(UCombatAttributeSet, CastRangeBonus)
	/** 状态抗性比例。 */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_StatusResistancePct, Category="Combat|Attributes") FGameplayAttributeData StatusResistancePct;
	COMBAT_ATTRIBUTE_ACCESSORS(UCombatAttributeSet, StatusResistancePct)
	/** 施加治疗时的增幅比例；与目标接受增幅相乘，例如双方各 20% 时，100 点治疗变为 144 点。 */
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
