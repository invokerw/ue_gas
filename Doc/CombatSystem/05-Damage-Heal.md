# 05 Damage 与 Heal 管线

## 1. 统一入口

技能、普攻、DOT、反伤都创建 `FCombatDamageSpec` 并调用 `UCombatDamageSubsystem::DealDamage`。治疗统一调用 `UCombatHealSubsystem::Heal`。两个入口只在服务器执行并返回实际结算 Result。

```cpp
UENUM(BlueprintType)
enum class ECombatDamageType : uint8
{
    Physical,
    Magical,
    Pure
};

USTRUCT(BlueprintType)
struct FCombatDamageSpec
{
    GENERATED_BODY()

    TObjectPtr<AActor> Attacker;
    TObjectPtr<AActor> Victim;
    ECombatDamageType Type = ECombatDamageType::Physical;
    float Amount = 0.0f;
    FGameplayTagContainer Flags;
    FCombatSourceContext Source;
    FCombatAttackHandle AttackHandle;
};

USTRUCT(BlueprintType)
struct FCombatDamageResult
{
    GENERATED_BODY()

    FCombatEventId EventId;
    float RequestedAmount = 0.0f;
    float MitigatedAmount = 0.0f;
    float AbsorbedAmount = 0.0f;
    float AppliedDamage = 0.0f;
    bool bKilled = false;
    FGameplayTag BlockReason;
};
```

HealSpec/HealResult 使用同样结构，区分 Requested、Amplified、Applied 和 Overheal。

## 2. 数据与 GAS 载体

| Combat 数据 | GAS 载体 |
| --- | --- |
| 最终 Damage/Heal Amount | SetByCaller `Data.Damage.Final` / `Data.Heal.Final` |
| DamageType | 唯一 DynamicAssetTag：`Damage.Type.Physical/Magical/Pure` |
| Flags | DynamicAssetTags：`Damage.Flag.*` |
| Attacker/Victim | EffectContext Instigator/EffectCauser、目标 ASC |
| Ability/Modifier/Projectile 来源链、Attack、事件链 | `FCombatGameplayEffectContext` |

原始 Amount、Pre Hook 的可变过程值和内部计算细节只存在服务器事务中；网络/日志按需要投影 Result。

所有入口都填充同一个 `FCombatSourceContext`：Ability 直接伤害以 Ability 为 direct source；Projectile 命中保留 Ability/Projectile DefinitionId 并以 Projectile 为 direct source；DOT tick 保留 Ability/Modifier DefinitionId 并以 Modifier 为 direct source；反伤 follow-up 继承原来源链并把直接来源切换为反伤 Modifier。未知或无法解析的 DefinitionId 不得退化为复制 DataAsset 指针。

## 3. Damage flags

```text
Damage.Flag.BypassMagicImmune
Damage.Flag.HPLoss
Damage.Flag.NoSpellAmplification
Damage.Flag.Reflection
Damage.Flag.NoLifesteal
Damage.Flag.NoCrit
```

Flags 的组合和互斥必须由 C++ 规范化。客户端不能提交 flags 或 Amount。

## 4. Damage 管线顺序

1. 创建服务器事务，校验 authority、Attacker/Victim、有限非负 Amount、Depth 和目标生命状态。
2. 规范化 DamageType/Flags。
3. `HPLoss` 进入独立直扣分支：跳过增伤、抗性、护盾、反伤和吸血，只保留 Health clamp、死亡和日志。
4. 魔免检查：Magical 且无 BypassMagicImmune 时返回 blocked Result，尚不执行有副作用 Hook。
5. 攻击者 `OnPreDealDamage` 和输出增伤。
6. 非 NoSpellAmplification 的技能魔法/纯粹伤害应用 SpellAmplify。
7. 目标 `OnPreTakeDamage` 和承受增伤。
8. 类型抗性：物理护甲、MagicResist、Pure 不减免。
9. `OnDamageBlock` 处理护盾/固定吸收，累加 AbsorbedAmount；结构修改延迟。
10. Instant GE 把 Final Amount 写入 IncomingDamage Meta Attribute。
11. AttributeSet clamp Health，并按 EventId 回报真实 AppliedDamage/bKilled。
12. 广播一次 DamageApplied；调用目标 PostTake 和来源 PostDeal。
13. 将反伤、吸血等 follow-up 加入事务队列；使用新 EventId、相同 RootEventId 和防递归 flags。
14. 如果 bKilled，从唯一死亡入口推进 Unit 生命周期；AttributeSet 不另发 Death。

推荐调用栈：

```text
Ability/Attack/Projectile/ModifierThink
  -> DamageSubsystem::DealDamage(Spec)
    -> Validate / normalize / immunity
    -> Source PreDeal / Target PreTake
    -> pure C++ amplification and resistance
    -> Target DamageBlock
    -> GE_CombatDamageApply(SetByCaller Final)
      -> AttributeSet applies delta and reports transaction slot
    -> DamageApplied / Post hooks / one Death transition
    -> queued lifesteal/reflection follow-up
```

## 5. 物理护甲和数值规则

基准公式：

```text
reduction = (0.06 * armor) / (1 + 0.06 * abs(armor))
armor >= 0: multiplier = 1 - reduction
armor < 0:  multiplier = 2 - pow(0.94, -armor)
```

数值安全：

- 拒绝 NaN、Inf 和负请求 Amount；0 返回零 Result，不运行有副作用 Hook。
- Damage/Heal/Shield 请求必须位于 `[0, 1e9]`；超限与非法值拒绝，不静默 clamp。
- MagicResist、Evasion 等百分比使用显式 clamp，具体上下限由 `FCombatNumericPolicyV1` 集中定义。
- 内部使用 `float` 与 GAS 保持一致；日志保留原始 float，UI 只负责显示取整。
- 不在中间阶段逐步取整；最终 Health 写入遵守 AttributeSet 的单位精度规则。
- 公式版本写入 CombatLog/测试基线，改变公式视为数据兼容变更。

Numeric Policy v1 的完整边界见 [14 M0 设计冻结](14-M0-Design-Freeze.md#51-numeric-policy-v1)。权威值不做小数位量化；Health clamp 后只在 `PreviousHealth > 0 && NewHealth == 0` 时判定 lethal。

随机暴击、闪避等不直接调用全局随机数；使用服务器 `UCombatRngSubsystem` 的 keyed roll：`MatchSeed + RootEventId + DomainTag + StableSubjectId + Ordinal` 唯一决定一次 roll。算法、固定攻击顺序、记录字段和版本兼容见 [14 M0 设计冻结](14-M0-Design-Freeze.md#52-combat-rng-v1)。

## 6. Heal 管线

1. 服务器校验来源、目标、Amount、Depth、目标是否允许被治疗。
2. 来源 HealAmplify 和目标 HealReceived；执行 `OnPreTakeHeal`。
3. Instant GE 写入 IncomingHealing Meta Attribute。
4. AttributeSet clamp 到 MaxHealth，回报实际 AppliedHealing。
5. 计算 Overheal，但只有 AppliedHealing 进入触发条件。
6. 广播一次 HealApplied，并把 HealResult 传给 `OnPostTakeHeal`。

满血目标的请求值不能被后续 Hook 当成真实治疗量。是否允许治疗 Dead/Dying、负治疗和复活属于明确产品规则；第一版全部拒绝，复活使用独立 Unit Lifecycle API。

## 7. HPLoss、护盾、吸血和反伤

### HPLoss

- 跳过增减伤、抗性、护盾、反伤、吸血和 SpellAmp。
- 仍做 Health clamp、Death 和日志。
- 不能用负 Heal 模拟。

### 护盾

- 在 `OnDamageBlock` 按 Priority/ApplySequence 消耗。
- Runtime 保存剩余值，修改事件 Amount 并累加 AbsorbedAmount。
- 归零时请求 deferred remove；魔免 blocked 和 HPLoss 不消耗护盾。
- 是否吸收特定 DamageType 由 Modifier 定义明确声明。

### 吸血

- 只读取 AppliedDamage，不读取 Requested/Mitigated。
- 根据 DamageType/Source 类型和 NoLifesteal 判断。
- 创建新的 Heal 事务，继承 RootEventId。

### 反伤

- 创建新 Damage 事务并带 Reflection，继承 RootEventId。
- Reflection 默认隐含 NoLifesteal，并阻止再次触发同类反伤。
- 达到 Depth 上限时返回 blocked Result 和诊断日志，不崩溃。

## 8. 死亡唯一入口

DamageSubsystem 读取 AttributeSet 回报的 bKilled，只调用一次 `UCombatUnitLifecycleComponent::RequestDeath(EventId)`。建议状态机：

```text
Alive -> Dying -> Dead -> Respawning -> Alive
```

第一版最低语义：

- Alive 进入 Dying 的原子转换只成功一次。
- Dying 是同步清理阶段；完成后进入 Dead 并只广播一次 UnitDeath。
- Dying 立即阻止新 Order/Attack/Ability，取消当前可中断行为，清理 motion 和按规则移除 Modifier。
- 已到 attack point 的 Projectile/AttackRecord 依其快照继续或 fizzle，不由 Death 回调任意销毁。
- Death 事件、奖励和日志只由成功状态转换产生一次。
- Respawn 不通过 Heal 模拟；仅 `Dead -> Respawning` 的服务器 API 可在预检后递增 `uint32 LifeGeneration`，并按 UnitData 初始化资源。

Death/Respawn 的固定清理表、generation 规则和默认保留项见 [14 M0 设计冻结](14-M0-Design-Freeze.md#3-dec-002unit-生命状态)。尸体时长、奖励和按 Unit 类型覆盖策略见 [GAP-013](12-Decisions-Gaps.md)。

## 9. 实现分工

- `UCombatDamageSubsystem`：权限、事务、Hook、Calculator、应用 GE、Result、follow-up 和 Death。
- `FCombatDamageCalculator`：无状态、纯 C++，读取聚合 Attribute，不修改状态、不广播、不调用蓝图。
- `GE_CombatDamageApply`：只把 Final SetByCaller 写入 IncomingDamage。
- `UCombatAttributeSet`：Meta Attribute 到 Health、clamp、按 EventId 回报，不另算抗性或广播。
- ModifierRuntime：仅在 Subsystem 同一权威事务内执行 Pre/Block/Post。

禁止：

- 蓝图、Projectile 或 Modifier 直接 SetHealth。
- GE/AttributeSet 再算一遍抗性、增伤或 Post Hook。
- 未带 Reflection 的递归反伤。
- 用请求 Amount 推算吸血、击杀或战斗日志。
- 客户端提交 Amount、Result、ModifierData 或 FinalizeAttack。

## 10. 最低验收

- 正负护甲、魔抗、Pure、魔免/绕过和 HPLoss 符合公式与顺序。
- 多护盾稳定消耗，blocked/HPLoss 不消耗护盾。
- Health clamp 后 AppliedDamage/Healing 正确，overkill/overheal 不进入后续触发。
- Damage、Heal、Death 每个 EventId 至多广播一次。
- 反伤/吸血 EventId 链、flags 和深度限制正确。
- Ability、Projectile、DOT Modifier 和反伤的 DirectSourceType/DefinitionId 能在 Result/CombatLog 中完整回溯。
- NaN/Inf/负值/无效 Actor 安全失败并产生诊断。
