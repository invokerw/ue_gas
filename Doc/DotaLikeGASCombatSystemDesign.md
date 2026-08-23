# UE GAS Dota-Like Combat System Design

## 1. 背景与目标

本文参考 `invokerw/dota2_skill` 仓库 `src` 目录的实现方式，设计一套适合本 UE 5.8 工程的 Dota-like 战斗系统。参考仓库当前阅读版本为 `085c1c5c2d1658a21f88cf9c01f6142df6830cc5`。

本项目当前是 UE 模板工程，已有两条可复用基础：

- `Source/ue_gas/Variant_Strategy`：多单位选择、AIController、EQS、NavMesh 移动、到达交互。
- `Source/ue_gas/Variant_TwinStick`：简单 Actor 弹体、AoE Actor、NPC 命中销毁、RVO 避让。

目标是在此基础上使用 Gameplay Ability System 实现以下要素：

- Ability：主动、被动、点目标、单位目标、无目标、引导、法球、自动施放、冷却、耗蓝、施法前摇、施法后摇。
- Modifier：Buff/Debuff、属性聚合、状态标记、周期 Think、伤害/治疗/普攻/技能事件 Hook、叠层、驱散、motion controller。
- Projectile：直线弹体、跟踪弹体、普攻弹体、命中回调、穿透/首个命中销毁、投射物表现资源。
- Damage/Heal Pipeline：统一处理技能、普攻、反伤、护盾、魔免、抗性、吸血、治疗增幅。
- Order/Movement：Dota 风格走/打/施法统一指令队列，移动使用 UE NavMesh、AIController、EQS，而不照搬参考仓库的网格寻路。
- Lua 替代：参考仓库的 Lua Ability/Modifier 在 UE 中用 Blueprintable C++ 基类 + 蓝图事件替代。

## 2. 参考仓库 `src` 结构摘要

参考项目是一个数据驱动的 Dota 战斗模拟内核，主要分层如下：

| 目录 | 核心职责 | UE/GAS 映射 |
| --- | --- | --- |
| `core` | `World`、`Unit`、事件、指令队列、普攻、tick 时序 | `UWorld`、Character/Pawn、Combat Subsystem、OrderComponent |
| `ability` | `Ability` 生命周期、行为标志、YAML 数据、Lua 技能 | `UGameplayAbility` 基类、Ability DataAsset、蓝图能力 |
| `modifier` | Buff/Debuff、属性聚合、状态、事件 Hook、motion controller | `GameplayEffect` + `GameplayTag` + 自定义 Modifier Runtime |
| `projectile` | 直线/跟踪投射物、ProjectileManager、命中/结束事件 | `ACombatProjectile`、AbilityTask、GameplayCue |
| `combat` | 统一伤害/治疗管线 | `GameplayEffectExecutionCalculation`、DamageSpec |
| `pathfinding` | A* 网格、ShapeCast、WallTracer、动态圆碰撞 | UE NavMesh、AI MoveTo、RVO/Detour Crowd/EQS |
| `script` | sol2 Lua 绑定 | BlueprintNativeEvent/BlueprintImplementableEvent |
| `log/replay` | 战斗事件记录与回放 | Combat Log、GameplayMessage、调试工具 |

参考项目最值得保留的不是具体代码，而是战斗语义：

- 一个单位持有 AbilityManager 和 ModifierManager。
- Ability 有完整施法阶段：Ready、Casting、Backswing、Channelling、OnCooldown。
- Modifier 同时提供属性、状态、事件 Hook。
- 普攻有 AttackRecord，法球通过 OnAttack 认领本次攻击，在 OnAttackLanded/Fail/Destroy 中继续处理。
- 投射物只负责移动和命中事件，真正的伤害/Modifier 仍走统一战斗管线。
- 所有技能数值来自数据层，复杂逻辑由脚本层实现。

## 3. GAS 总体架构

建议新增独立战斗目录：

```text
Source/ue_gas/Combat
  Ability/
  Attribute/
  Damage/
  Modifier/
  Projectile/
  Order/
  Targeting/
  Data/
  Log/
  Blueprint/
```

模块依赖需要在 `Source/ue_gas/ue_gas.Build.cs` 增加：

```csharp
"GameplayAbilities",
"GameplayTags",
"GameplayTasks"
```

`ue_gas.uproject` 建议启用 GameplayAbilities 插件。当前工程已有 `AIModule`、`NavigationSystem`、`StateTree`、`GameplayStateTree`、`Niagara`、`UMG`，可直接用于移动、AI、特效和 UI。

### 3.1 运行时核心对象

| 类/组件 | 建议名称 | 职责 |
| --- | --- | --- |
| 战斗单位 | `ACombatUnitCharacter` | 继承 `ACharacter`，实现 `IAbilitySystemInterface`，承载 ASC、属性、队伍、攻击组件 |
| ASC | `UCombatAbilitySystemComponent` | 封装 GAS 激活、标签查询、事件派发、冷却/消耗查询 |
| 属性集 | `UCombatAttributeSet` | 生命、魔法、护甲、魔抗、攻击力、攻速、移动速度等 |
| 指令队列 | `UCombatOrderComponent` | Dota 风格 Move/Attack/Cast/Stop FIFO |
| 普攻组件 | `UCombatAttackComponent` | 攻击冷却、AttackRecord、近战/远程命中 |
| Modifier Runtime | `UCombatModifierComponent` | 补齐 GAS 原生 GE 不擅长的事件 Hook 和蓝图 Modifier 实例 |
| 投射物管理 | `UCombatProjectileSubsystem` 或 Actor 管理 | 创建线性/跟踪投射物，转发命中事件 |
| 战斗事件 | `UCombatEventSubsystem` | 统一广播伤害、治疗、技能、投射物、Modifier、AttackRecord 事件 |

### 3.2 数据层

建议用 DataAsset 替代 YAML，用 Blueprint Class 替代 Lua：

| 数据资产 | 建议名称 | 内容 |
| --- | --- | --- |
| 单位定义 | `UCombatUnitData` | 初始属性、队伍、碰撞半径、普攻参数、默认技能 |
| 技能定义 | `UCombatAbilityData` | Ability 类、行为标志、目标阵营、前摇/后摇/引导、冷却、耗蓝、施法距离、special、intrinsic modifier |
| Modifier 定义 | `UCombatModifierData` | GE 类、蓝图 runtime 类、显示/驱散/死亡移除/状态/属性/ThinkInterval |
| 投射物定义 | `UCombatProjectileData` | 直线/跟踪、速度、宽度、距离、命中策略、Niagara/Sound |
| 伤害定义 | `FCombatDamageSpec` | 伤害类型、数值、flags、来源 ability、是否触发吸血/反伤 |
| 技能动作 | `FCombatAbilityAction` | Damage、Heal、ApplyModifier、SpawnProjectile、CreateThinker、GameplayEvent |

`ability_special` 建议实现为：

```cpp
USTRUCT(BlueprintType)
struct FCombatSpecialValue
{
    GENERATED_BODY()

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
    TArray<float> Values;

    float GetValueAtLevel(int32 Level) const;
};
```

技能和 Modifier 蓝图都只通过 key 读取当前等级数值，避免逻辑蓝图里硬编码数值。

### 3.3 权威边界与唯一数据源

为了避免 GAS 原生层和自定义 Dota-like Runtime 层各算一套，第一版必须先确定下面几条硬规则：

| 问题 | 约束 |
| --- | --- |
| 属性最终值 | 以 ASC Attribute/Active GameplayEffect 聚合结果为唯一来源。ModifierRuntime 不直接维护第二套 MoveSpeed、Armor、SpellAmp 等最终属性。 |
| 动态属性 | 蓝图 Modifier 如果需要动态数值，必须把结果写成 GameplayEffect Modifier、SetByCaller、Attribute 或刷新已有 GE；不要让战斗查询直接调用蓝图函数实时聚合。 |
| 扣血/回血 | `UCombatDamageSubsystem` / `UCombatHealSubsystem` 是唯一编排入口；Health/Mana 的最终修改只允许通过统一 GameplayEffect 或 AttributeSet Meta Attribute 完成。 |
| 事件 Hook | ModifierRuntime 只负责事件响应、状态机、副作用和需要实例状态的逻辑，例如护盾剩余值、法球认领、motion controller。 |
| 复制 | 服务器拥有权威 Runtime；客户端 UI 和表现通过 Attribute、ActiveGE、GameplayTag、GameplayCue、GameplayMessage/CombatLog 投影，不依赖复制 UObject Runtime 实例。 |
| 指令队列 | Order 只等待“技能已执行/引导结束/被中断”等战斗事件，不等待冷却结束；backswing 默认是动画表现，不阻塞队列。 |

这几条约束比具体类名更重要。后续实现如果需要打破其中一条，必须先把替代路径写清楚，否则很容易出现双重扣血、客户端状态缺失、属性显示和服务器判定不一致等问题。

## 4. Ability 设计

### 4.1 行为标志

参考项目的 `BehaviorFlag` 应转成 UE 可编辑枚举/位掩码，并同步到 GameplayTag：

```text
Ability.Behavior.NoTarget
Ability.Behavior.UnitTarget
Ability.Behavior.PointTarget
Ability.Behavior.Passive
Ability.Behavior.Channelled
Ability.Behavior.AoE
Ability.Behavior.Attack
Ability.Behavior.AutoCast
Ability.Behavior.IgnoreSilence
Ability.Behavior.IgnoreMagicImmune
Ability.Behavior.IgnoreUntargetable
```

目标阵营：

```text
TargetTeam.None
TargetTeam.Enemy
TargetTeam.Friendly
TargetTeam.Both
```

### 4.2 基类

建议实现：

```cpp
UCLASS(Abstract, Blueprintable)
class UCombatGameplayAbility : public UGameplayAbility
{
    GENERATED_BODY()

public:
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
    TObjectPtr<UCombatAbilityData> AbilityData;

    UFUNCTION(BlueprintCallable)
    float GetSpecialValue(FName Key) const;

protected:
    virtual bool CanActivateAbility(...) const override;
    virtual void ActivateAbility(...) override;
    virtual void CancelAbility(...) override;

    UFUNCTION(BlueprintNativeEvent)
    void ReceiveSpellStart(const FCombatAbilityContext& Context);

    UFUNCTION(BlueprintNativeEvent)
    void ReceiveChannelTick(const FCombatAbilityContext& Context, float DeltaTime);

    UFUNCTION(BlueprintNativeEvent)
    void ReceiveChannelFinish(const FCombatAbilityContext& Context, bool bInterrupted);
};
```

### 4.3 施法阶段

参考项目的 Ability 流程应在 GAS 中显式保留：

1. `CanActivateAbility`
   - 检查死亡、眩晕、妖术、沉默、冷却、魔法、目标阵营、魔免、不可选中、距离。
2. `ActivateAbility`
   - 缓存目标快照。
   - 先扣资源或在 cast point 完成时 Commit，具体按设计开关决定。
   - 广播 `AbilityCastStarted`。
   - 播放 cast point montage 或启动 `UAbilityTask_WaitDelay`。
3. Cast Point 完成
   - 再次验证目标仍存在。
   - 调用 `ReceiveSpellStart` 或执行 DataDriven Actions。
4. Channel
   - 用 Tick Task 或 Periodic Task 调用 `ReceiveChannelTick`。
   - 被眩晕/沉默/死亡/目标丢失时中断。
5. Backswing
   - Dota 语义里后摇不应阻止新施法；UE 可只作为动画/表现，不用锁住 ASC。
   - 不建议让 OrderComponent 等待 backswing 结束才继续下一条指令。队列衔接应以 `AbilityExecuted` / `ChannelFinished` / `AbilityInterrupted` 为准。
6. Cooldown
   - 使用 Cooldown GameplayEffect。
   - 若要求“被打断也进冷却”，在 Cancel 时也 Apply Cooldown。
7. 完整释放完成
   - 广播 `AbilityCastFinished`。
   - 向 owner 的 ModifierRuntime 派发 `OnAbilityExecuted`。

建议 Ability 基类明确广播三个不同事件，避免动画、结算、队列状态混在一起：

| 事件 | 触发时机 | 用途 |
| --- | --- | --- |
| `AbilityCastStarted` | 通过校验并进入 cast point | UI、动画、打断窗口、日志 |
| `AbilityExecuted` | cast point 完成并调用 `ReceiveSpellStart`，或 channel 正常结束 | Modifier `OnAbilityExecuted`、Order 队列继续、触发被动 |
| `AbilityEnded` | ability task 全部结束，包括 backswing/清理 | 生命周期清理、表现回收 |

如果某个技能是引导型，`AbilityExecuted` 应在引导正常结束时触发；引导被中断时触发 `AbilityInterrupted`，并按技能配置决定是否进入冷却。

### 4.4 DataDriven Ability

参考项目 `DataDrivenAbility` 只支持 Damage、Heal、ApplyModifier。UE 第一阶段也建议保持克制：

```text
Action.Damage
Action.Heal
Action.ApplyModifier
Action.SpawnLinearProjectile
Action.SpawnTrackingProjectile
Action.CreateThinker
Action.SendGameplayEvent
```

复杂技能使用蓝图子类：

- `BP_Ability_LinaDragonSlave`
- `BP_Ability_PudgeMeatHook`
- `BP_Ability_EarthshakerFissure`
- `BP_Ability_JuggernautOmnislash`

## 5. Modifier 设计

GAS 的 GameplayEffect 很适合表达持续时间、叠层、属性修改、GameplayTag 状态，但不适合直接表达 Dota Modifier 的大量事件 Hook。因此建议采用双层结构：

核心原则：数值属性统一落到 GAS，事件语义统一落到 ModifierRuntime。不要让 GE 和 Runtime 同时提供同一个最终属性值。

### 5.1 GameplayEffect 层

用于：

- Duration/Infinite/Instant。
- Stack policy。
- 属性修改：Armor、MoveSpeed、AttackDamage、MagicResist 等。
- Granted Tags：`State.Stunned`、`State.Silenced`、`State.Rooted`、`State.Hexed`、`State.MagicImmune`。
- Periodic execution：简单 DOT/HOT。
- GameplayCue：显示特效、音效、UI。

### 5.2 Modifier Runtime 层

用于参考项目中的 Hook：

```text
OnCreated
OnDestroyed
OnRefresh
OnStackChanged
OnIntervalThink
OnPreTakeDamage
OnPostTakeDamage
OnPreTakeHeal
OnPostTakeHeal
OnAbilityExecuted
OnAttack
OnAttackLanded
OnAttackFail
OnAttackRecordDestroy
GetAttackProjectileName
OnMotionTick
```

建议基类：

```cpp
UCLASS(Abstract, Blueprintable, EditInlineNew)
class UCombatModifierRuntime : public UObject
{
    GENERATED_BODY()

public:
    UPROPERTY(BlueprintReadOnly)
    FActiveGameplayEffectHandle ActiveGEHandle;

    UPROPERTY(BlueprintReadOnly)
    TObjectPtr<UCombatModifierData> ModifierData;

    UFUNCTION(BlueprintNativeEvent)
    void OnCreated(const FCombatModifierContext& Context);

    UFUNCTION(BlueprintNativeEvent)
    void OnPreTakeDamage(UPARAM(ref) FCombatDamageEvent& Event);

    UFUNCTION(BlueprintNativeEvent)
    void OnAttack(UPARAM(ref) FCombatAttackRecord& Record);

    UFUNCTION(BlueprintNativeEvent)
    void OnAttackLanded(const FCombatAttackRecord& Record);
};
```

`UCombatModifierComponent` 负责：

- 在 GE 添加时创建 Runtime 实例。
- 在 GE 移除/过期时销毁 Runtime。
- 维护 `FActiveGameplayEffectHandle -> Runtime` 映射。
- 派发伤害、治疗、普攻、技能事件。
- 处理驱散、刷新、叠层、ThinkInterval。

Runtime 不负责：

- 直接覆盖 `MoveSpeed()`、`Armor()`、`AttackDamage()` 这类最终属性查询。
- 在客户端作为权威数据源。
- 绕过 ASC 直接修改 Health/Mana。

动态属性推荐两种实现方式：

1. 创建/刷新 GE 时，把蓝图计算出的数值写入 SetByCaller，然后让 GE Modifier 修改 Attribute。
2. 对需要经常变化的数值，Runtime 按固定频率或事件触发刷新一个专用 GE，而不是每次属性查询时调用蓝图函数。

### 5.3 状态标签

建议状态用 GameplayTag：

```text
State.Stunned
State.Silenced
State.Rooted
State.Disarmed
State.Hexed
State.Invisible
State.Invulnerable
State.OutOfGame
State.MagicImmune
State.Untargetable
State.NoUnitCollision
State.NoHealthBar
State.Frozen
```

这些标签用于：

- Ability 激活阻止：Stunned、Hexed、Silenced。
- 移动阻止：Stunned、Rooted、Frozen。
- 攻击阻止：Stunned、Disarmed、Hexed。
- 目标过滤：Untargetable、OutOfGame、Invulnerable、MagicImmune。
- 碰撞/显示：NoUnitCollision、NoHealthBar。

## 6. 属性与聚合

参考项目的 `ModifierProperty` 建议转成 GAS Attribute：

| 属性 | GAS Attribute |
| --- | --- |
| MaxHealth / Health | `MaxHealth` / `Health` |
| MaxMana / Mana | `MaxMana` / `Mana` |
| Armor | `Armor` |
| MagicResist | `MagicResist` |
| AttackDamage | `AttackDamage` |
| AttackSpeed | `AttackSpeed` |
| BaseAttackTime | `BaseAttackTime` |
| MoveSpeed | `MoveSpeed` |
| AttackRange | `AttackRange` |
| Evasion | `Evasion` |
| LifestealPct | `LifestealPct` |
| HealthRegen / ManaRegen | `HealthRegen` / `ManaRegen` |
| SpellAmplifyPct | `SpellAmplifyPct` |
| StatusResistancePct | `StatusResistancePct` |
| CooldownReductionPct | `CooldownReductionPct` |
| CastRangeBonus | `CastRangeBonus` |

聚合顺序建议：

```text
final = (base + additive) * (1 + additive_pct) * total_multiplier
```

GAS 默认 ModifierOp 可以覆盖大多数场景；对护甲、攻速、闪避上限等 Dota 特殊公式，使用 ExecutionCalculation 或 AttributeSet 的 PostGameplayEffectExecute/PostAttributeChange。

## 7. Damage/Heal Pipeline

建议所有技能、普攻、反伤、DOT 都走同一条 `FCombatDamageSpec`：

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

    UPROPERTY(BlueprintReadWrite)
    TObjectPtr<AActor> Attacker;

    UPROPERTY(BlueprintReadWrite)
    TObjectPtr<AActor> Victim;

    UPROPERTY(BlueprintReadWrite)
    ECombatDamageType Type = ECombatDamageType::Physical;

    UPROPERTY(BlueprintReadWrite)
    float Amount = 0.0f;

    UPROPERTY(BlueprintReadWrite)
    FGameplayTagContainer Flags;
};
```

Damage flags：

```text
DamageFlag.BypassMagicImmune
DamageFlag.HPLoss
DamageFlag.NoSpellAmplification
DamageFlag.Reflection
DamageFlag.NoLifesteal
```

管线顺序：

1. 攻击者输出增伤。
2. 魔法/纯粹伤害的 SpellAmplify。
3. 受害者承受增伤。
4. `OnPreTakeDamage`，用于护盾、吸收、减伤前拦截。
5. 魔法免疫短路。
6. 类型抗性：
   - 物理：护甲公式。
   - 魔法：`Amount * (1 - MagicResist)`。
   - 纯粹：不减免。
7. 扣血。
8. 广播 DamageApplied。
9. `OnPostTakeDamage`，用于反伤、触发效果。
10. 物理吸血。

物理护甲公式可沿用参考项目：

```text
reduction = (0.06 * armor) / (1 + 0.06 * abs(armor))
armor >= 0: multiplier = 1 - reduction
armor < 0:  multiplier = 2 - pow(0.94, -armor)
```

Heal Pipeline：

1. `OnPreTakeHeal`。
2. HealAmpPct。
3. 应用生命回复并 clamp 到 MaxHealth。
4. 广播 HealApplied。
5. `OnPostTakeHeal`。

GAS 实现建议：

- `UCombatDamageSubsystem::DealDamage` 是唯一公开入口，负责创建上下文、执行 Pre Hook、调用 GE、执行 Post Hook、广播事件。
- `UGameplayEffectExecutionCalculation_Damage` 只做数值计算：读取 SetByCaller Damage、DamageType、Flags、Source/Target 属性，输出最终伤害到 Health Meta Attribute。
- `UCombatAttributeSet` 只负责应用 Meta Attribute 到 Health，并处理 clamp、死亡阈值等最底层规则。
- ModifierRuntime 的 `OnPreTakeDamage` / `OnPostTakeDamage` 必须在 Subsystem 的同一条服务器权威调用栈内执行。

禁止路径：

- 技能蓝图直接调用 `SetHealth`。
- Projectile 直接修改目标生命。
- `ExecutionCalculation` 内部再次派发 `OnPostTakeDamage`。
- 反伤在没有 `DamageFlag.Reflection` 的情况下递归触发自身。

推荐调用栈：

```text
Ability/Attack/Projectile
  -> UCombatDamageSubsystem::DealDamage(Spec)
    -> Build DamageEvent
    -> Source/Target ModifierRuntime OnPreTakeDamage
    -> Apply Damage GameplayEffect
      -> Damage ExecutionCalculation
      -> AttributeSet applies Health delta
    -> Broadcast DamageApplied / Death if needed
    -> Source/Target ModifierRuntime OnPostTakeDamage
    -> Lifesteal / Reflection follow-up damage with flags
```

## 8. 普攻与法球

参考项目 `AttackRecord` 是实现 Dota 法球的关键，UE 中建议保留：

```cpp
USTRUCT(BlueprintType)
struct FCombatAttackRecord
{
    GENERATED_BODY()

    int32 RecordId = INDEX_NONE;
    TWeakObjectPtr<AActor> Attacker;
    TWeakObjectPtr<AActor> Target;
    float BaseDamage = 0.0f;
    float BonusDamage = 0.0f;
    ECombatDamageType DamageType = ECombatDamageType::Physical;
    bool bMissed = false;
    bool bProcessed = false;
    TArray<TWeakObjectPtr<UCombatModifierRuntime>> OrbListeners;
};
```

普攻流程：

1. 攻击组件检查目标、距离、攻击冷却、状态标签。
2. 创建 AttackRecord，锁定 BaseDamage。
3. 派发 `OnAttack` 给攻击者所有 Modifier。
4. 法球 Modifier 可检查 autocast、沉默、冷却、魔法；成功后扣资源、进入冷却、修改 BonusDamage/DamageType，并把自己加入 OrbListeners。
5. 近战立即完成攻击。
6. 远程生成 TrackingProjectile，命中后完成攻击；目标死亡/不可选中则 fail。
7. 完成时处理闪避、伤害、事件、OnAttackLanded/Fail/Destroy。

示例：Drow Frost Arrows

- 技能 DataAsset：`Passive + Attack + AutoCast`，intrinsic modifier 为 `modifier_frost_arrows`。
- `modifier_frost_arrows::OnAttack`
  - 检查 ability autocast。
  - 检查 owner 未沉默。
  - 调用 ability 资源扣除。
  - 增加 bonus damage。
  - claim record。
- `OnAttackLanded`
  - 给目标 Apply GE `modifier_frost_arrows_slow`，slow_pct 从当前 ability special 快照。

## 9. Projectile 设计

参考项目只有两类投射物：LinearProjectile 和 TrackingProjectile。UE 侧建议实现 Actor 基类，同时提供 AbilityTask 封装。

### 9.1 Actor 层

```cpp
UCLASS(Abstract)
class ACombatProjectile : public AActor
{
    GENERATED_BODY()

public:
    UPROPERTY(BlueprintReadOnly)
    FCombatProjectileSpec Spec;

    UFUNCTION(BlueprintNativeEvent)
    void OnProjectileHit(AActor* Victim, const FHitResult& Hit);

    UFUNCTION(BlueprintNativeEvent)
    void OnProjectileFinished();
};
```

派生：

- `ACombatLinearProjectile`
  - 记录上一帧位置。
  - 每 tick 从 Previous 到 Current 做 Sphere/Capsule Sweep。
  - 支持 `bDestroyOnFirstHit` 和 AlreadyHit 集合。
- `ACombatTrackingProjectile`
  - 持有 Target weak pointer。
  - 逐帧朝目标当前位置移动。
  - 目标死亡、OutOfGame、Untargetable 时 fizzle。
- `ACombatAttackProjectile`
  - 包含 `FCombatAttackRecord` 或 RecordId。
  - 命中/结束时回调 AttackComponent 完成普攻。

### 9.2 AbilityTask 层

复杂蓝图技能不应直接 SpawnActor 到处写重复逻辑，建议封装：

```text
UAbilityTask_SpawnLinearProjectile
UAbilityTask_SpawnTrackingProjectile
UAbilityTask_WaitProjectileHit
```

输出：

```text
OnHit(Victim, HitLocation)
OnFinished()
OnFizzled()
```

表现：

- 用 GameplayCue 或 Niagara Component 表现弹体。
- 投射物逻辑由服务器权威处理。
- 客户端可以预测生成纯表现弹体，但命中以服务器为准。

当前 `ATwinStickProjectile` 可以作为第一版视觉/碰撞参考，但需要把 `NPC->ProjectileImpact` 改成发送 GameplayEvent 或调用 `UCombatDamageSubsystem::DealDamage`。

## 10. Thinker 与 AoE

参考项目的 thinker 是隐藏、中立、不可选、无碰撞、无血条的临时单位，用于周期 Think 或区域效果。UE 中建议两种实现：

### 10.1 Actor Thinker

`ACombatThinker`：

- Hidden in game 或只有调试表现。
- 不参与单位碰撞。
- 持有 Source、Ability、Team、Duration。
- 可带 Sphere/Box/Capsule 碰撞或纯定时查询。
- 可挂一个 ModifierRuntime，提供 OnIntervalThink。

适用于：

- Lina Light Strike Array 延迟爆炸。
- Earthshaker Fissure 临时阻挡/区域。
- Healing Ward 周期治疗。
- 毒圈、火圈、持续 AoE。

### 10.2 GameplayAbility 内部 Task

对于短生命周期 AoE，可用 AbilityTask：

```text
WaitDelay -> SphereOverlap -> ApplyDamage/Modifier -> EndTask
```

当前 `ATwinStickAoEAttack` 可以升级为 `ACombatThinker` 的一个蓝图子类。

## 11. Order 与 NavMesh 移动

参考项目自带网格 A*、WallTracer、动态圆避障。本项目已经有 UE NavMesh/EQS/AIController，建议不重写寻路。

### 11.1 指令类型

```cpp
UENUM(BlueprintType)
enum class ECombatOrderType : uint8
{
    MoveToPoint,
    MoveToUnit,
    AttackTarget,
    CastNoTarget,
    CastPoint,
    CastTarget,
    Stop
};
```

`UCombatOrderComponent` 持有 FIFO：

- `IssueOrder(Order, bQueue)`
- `ClearOrders`
- `PumpCurrentOrder`
- `OnMoveFinished`
- `OnAbilityFinished`
- `OnAttackFinished`

### 11.2 移动执行

复用 `AStrategyUnit::MoveToLocation` 思路：

- Point 移动：`AAIController::MoveTo` + `FAIMoveRequest`。
- 多单位移动：仍可用 EQS 为每个单位找分散位置。
- CastTarget/AttackTarget：
  - 距离不足时 MoveTo 目标附近。
  - 到达距离后停止移动，转向目标，激活技能/普攻。
- Rooted：停止 MoveTo，但不清队列。
- Stunned/Hexed：停止当前行为，可按 Dota 语义清队列。
- MotionController：暂停普通 MoveTo，位移结束后恢复/重判队首。

OrderComponent 与 Ability 的衔接建议：

- `CastNoTarget/CastPoint/CastTarget` 一旦成功进入 cast point，标记为 dispatched。
- 非引导技能在 `AbilityExecuted` 后 pop 当前 cast order，而不是等 `AbilityEnded` 或 cooldown。
- 引导技能在 `ChannelFinished` 后 pop；`ChannelInterrupted` 时按 Dota 语义清队列或继续队列，由技能配置决定。
- 施法距离不足时，OrderComponent 只派生 MoveTo，不直接激活 Ability。
- 目标在追击或 cast point 中死亡/不可选中时，当前 order fail 并进入下一条，或按配置清队列。

### 11.3 碰撞与避让

UE 侧建议：

- CharacterMovement `bUseRVOAvoidance = true` 或 Detour Crowd。
- Capsule 半径对应 Dota hull radius。
- `State.NoUnitCollision` 时调整 collision response 或 movement avoidance。
- 阻挡型地形技能：
  - 简单版：生成 Blocking Volume / NavModifierVolume，并调用 NavigationSystem 更新。
  - 高级版：自定义 NavArea 和动态导航代价。

动态 NavMesh 阻挡需要分阶段处理。UE 的 Runtime NavMesh 更新可能异步且有成本，AI 当前 PathFollowing 不一定立刻重算，因此不要在第一版把 Fissure 这类技能的正确性建立在动态 NavMesh 之上。

推荐阶段：

1. 第一版：生成物理阻挡体，设置 Pawn/Projectile collision；对正在 MoveTo 的单位调用 StopMovement + 重新 `PumpCurrentOrder`，让 AI 自行 repath。
2. 第二版：启用 Runtime Navigation Generation，使用 `NavModifierVolume` / `UNavModifierComponent` 标记临时不可通行区域，并在创建/销毁时主动请求相关单位重新寻路。
3. 第三版：对高频或大量临时阻挡，改为自定义局部避障/代价系统，避免频繁重建 NavMesh。

## 12. 蓝图替代 Lua

参考项目 Lua Ability 只暴露少量 self API，Modifier 暴露较多 Hook。UE 里建议定义两类蓝图基类：

### 12.1 Ability 蓝图事件

```text
ReceiveSpellStart(Context)
ReceiveChannelTick(Context, DeltaTime)
ReceiveChannelFinish(Context, bInterrupted)
GetSpecialValue(Key)
GetCaster()
GetTargetActor()
GetTargetLocation()
```

### 12.2 Modifier 蓝图事件

```text
OnCreated(Context)
OnDestroyed(Context)
OnRefresh(Context)
OnIntervalThink(Context)
OnPreTakeDamage(ref DamageEvent)
OnPostTakeDamage(DamageEvent)
OnPreTakeHeal(ref HealEvent)
OnPostTakeHeal(HealEvent)
OnAbilityExecuted(AbilityEvent)
OnAttack(ref AttackRecord)
OnAttackLanded(AttackRecord)
OnAttackFail(AttackRecord)
OnAttackRecordDestroy(AttackRecord)
OnMotionTick(DeltaTime)
GetAttackProjectileName()
```

关键约束：

- 数值必须优先来自 DataAsset special，不在蓝图中硬编码。
- Modifier Runtime 可以在自身保存实例状态，例如护盾剩余值、法球已认领 record id。
- 蓝图只写行为，冷却、耗蓝、状态校验、目标过滤、事件派发由 C++ 基类统一处理。

## 13. 网络同步策略

GAS 默认适合网络同步，但 Dota-like 技能会有很多服务器权威逻辑：

- ASC 使用 Mixed 或 Full replication，视玩家/AI 数量决定。
- Attribute、GE、GameplayTag 由 ASC 复制。
- Ability 激活可做客户端预测，但复杂目标/投射物命中以服务器为准。
- Projectile Actor：
  - 服务器生成权威 Actor。
  - 客户端生成 GameplayCue/Niagara 表现。
  - 命中只在服务器结算。
- CombatEvent/Log：
  - 服务器记录完整事件。
  - UI 只订阅本地需要的简化事件。
- Order：
  - PlayerController 将 Order RPC 到服务器。
  - 服务器执行 MoveTo/Attack/Cast。
- ModifierRuntime：
  - 只在服务器作为权威实例运行。
  - 客户端不要依赖 Runtime UObject 复制。
  - UI 所需的名称、图标、层数、剩余时间、是否 debuff，通过 Active GameplayEffect 或独立 replicated view model 暴露。
  - 纯表现通过 GameplayCue；战斗日志通过 GameplayMessage/CombatLog 复制或本地投影。

第一阶段如果只做单机/PIE，可以先不做预测，只保证服务器权威路径干净。

多人阶段建议先采用“低预测”策略：

- 指令、施法、投射物命中都由服务器确认。
- 客户端只预测本地输入反馈、施法指示器、非命中特效。
- 等 Damage/Modifier/Projectile 全链路稳定后，再考虑本地预测瞬发技能或普通移动。

## 14. 示例技能落地

### 14.1 Lina Dragon Slave

数据：

- Behavior：PointTarget、AoE。
- CastPoint：0.45。
- Cooldown/ManaCost/Special：damage、radius、range。

蓝图逻辑：

1. `ReceiveSpellStart`
2. 读取 caster location 和 target point。
3. 计算方向和范围终点。
4. 做 capsule/box/sphere sweep 或 `FindEnemiesInLine`。
5. 对每个敌人调用 `DealDamage(Magical, damage)`。
6. 触发 GameplayCue。

### 14.2 Pudge Meat Hook

数据：

- Behavior：PointTarget。
- Special：damage、length、width、missile_speed。

蓝图逻辑：

1. `ReceiveSpellStart`
2. Spawn LinearProjectile，`bDestroyOnFirstHit = true`。
3. OnHit：
   - DealDamage Magical。
   - 给目标挂 `modifier_hook_drag`。
4. `modifier_hook_drag`：
   - State：Stunned、NoUnitCollision。
   - MotionController：每 tick 朝 caster 当前位置移动或使用 RootMotionSource。
   - Destroy 时恢复碰撞并做位置校正。

### 14.3 Drow Frost Arrows

数据：

- Behavior：Passive、Attack、AutoCast。
- IntrinsicModifier：`modifier_frost_arrows`。
- Special：bonus_damage、slow_duration、slow_pct。

Modifier：

- `GetAttackProjectileName` 返回冰箭 GameplayCue/ProjectileData。
- `OnAttack` 扣蓝/冷却，成功后 claim record。
- `OnAttackLanded` 给目标 Apply slow GE。

### 14.4 Magic Shield

Modifier：

- GE 提供 `MagicResist +0.10`。
- Runtime 保存 `_remaining = 200`。
- `OnPreTakeDamage`：
  - 只处理 Magical。
  - 吸收 `min(remaining, amount)`。
  - 修改 DamageEvent.Amount。
  - remaining 归零时移除自身 GE。

### 14.5 Earthshaker Fissure

技能：

- PointTarget。
- 线性范围伤害 + stun + knockback。
- 中点生成 `ACombatThinker_FissureBlocker`。

阻挡实现分两阶段：

- 第一阶段：只做视觉 thinker，不阻挡导航。
- 第二阶段：生成阻挡碰撞体并强制相关单位 repath。
- 第三阶段：生成动态 NavModifierVolume，持续时间结束后移除并刷新 NavMesh。

## 15. 与当前工程的集成步骤

### 阶段 0：工程准备

1. Build.cs 添加 GAS 依赖。
2. uproject 启用 GameplayAbilities。
3. 新增 `Combat` 目录。
4. 定义 GameplayTags：State、Ability、Damage、Event、Cue。
5. 建立 `ACombatUnitCharacter`，可先由 `AStrategyUnit` 或新类继承。

### 阶段 1：属性与伤害

1. 实现 `UCombatAttributeSet`。
2. 实现初始化 GE/DataAsset。
3. 实现 `UCombatDamageSubsystem`。
4. 实现 Damage ExecutionCalculation。
5. 做自动化测试：物理护甲、魔抗、纯粹伤害、魔免、护盾吸收、治疗增幅。

### 阶段 2：Modifier

1. 实现 `UCombatModifierData`。
2. 实现 `UCombatModifierRuntime`。
3. 实现 `UCombatModifierComponent`。
4. 支持 OnCreated/Destroyed/IntervalThink/PreDamage/PostDamage。
5. 做示例：Stun、Slow、Shield、DOT。

### 阶段 3：Ability

1. 实现 `UCombatAbilityData`。
2. 实现 `UCombatGameplayAbility` 施法生命周期。
3. 实现 DataDriven Actions。
4. 支持 Cooldown/Mana/CanCast/TargetFilter。
5. 做示例：无目标治疗、单位目标伤害、点目标 AoE。

### 阶段 4：Order 与普攻

1. 实现 `UCombatOrderComponent`。
2. 接入 `AStrategyPlayerController::DoMoveUnitsCommand`。
3. 实现 AttackTarget 指令。
4. 实现 `UCombatAttackComponent` 和 AttackRecord。
5. 支持近战、远程、闪避、法球。

### 阶段 5：Projectile 与 Thinker

1. 实现 Linear/Tracking Projectile。
2. 封装 SpawnProjectile AbilityTask。
3. 实现 `ACombatThinker`。
4. 改造 `ATwinStickProjectile` 和 `ATwinStickAoEAttack`，让它们走 Combat Damage/GameplayEvent。

### 阶段 6：复杂技能与工具

1. 实现 Pudge Hook、Drow Frost Arrows、Lina Dragon Slave、Earthshaker Fissure。
2. 增加战斗日志 UI。
3. 增加 DataAsset 编辑规范。
4. 增加 PIE 测试地图。
5. 后续再做网络预测、录像回放、技能编辑器。

## 16. 测试建议

参考仓库测试覆盖很细，本项目也应优先建立自动化测试：

- Attribute 初始化和 GE 修改。
- Damage pipeline：
  - 护甲正负值。
  - 魔法/纯粹/物理。
  - 魔免、BypassMagicImmune。
  - HPLoss 跳过护盾和抗性。
  - 护盾吸收。
  - 反伤不递归。
  - 吸血只在物理伤害触发。
- Modifier lifecycle：
  - OnCreated/OnDestroyed。
  - Duration 过期。
  - Stack 改变。
  - Purge strong/normal。
- Ability lifecycle：
  - cast point 完成。
  - 被沉默/眩晕中断。
  - channel tick 和 finish。
  - cooldown/mana 扣除时机。
- Projectile：
  - linear 首个命中销毁。
  - linear 穿透不重复命中。
  - tracking 目标死亡 fizzle。
  - 远程普攻 AttackRecord 只结算一次。
- Order：
  - Move/Cast/Attack FIFO。
  - queue=true 追加。
  - Stop 清空。
  - 距离不足自动追击。

## 17. 关键风险与决策

- GAS 原生 GameplayEffect 不能完整表达 Dota Modifier Hook，所以必须有 ModifierRuntime 层。
- Backswing 在 Dota 中不阻止新施法，UE 动画后摇不要简单用 Blocking Ability Tag 锁死所有输入。
- 冷却/耗蓝扣除时机要产品化决定：Dota 风格通常施法开始扣资源，被打断也可能进冷却；某些游戏是生效瞬间 Commit。
- 投射物命中必须服务器权威，否则法球、吸血、反伤和多段命中会出现不一致。
- NavMesh 动态阻挡技能要谨慎，第一版可只做视觉/碰撞，后续再引入动态 NavModifier。
- 蓝图能力要有强约束：数值来自 DataAsset，公共校验在 C++，蓝图只写技能差异逻辑。

## 18. 推荐落地原则

1. 先做统一 Damage/Modifier/Attribute，再做复杂技能。
2. 先保留 Dota 语义，再决定表现形式。
3. 能用 GAS 原生的用 GAS：属性、标签、持续时间、叠层、冷却、消耗、GameplayCue。
4. GAS 不擅长的补自定义层：AttackRecord、Modifier Hook、Order Queue、Projectile callbacks。
5. 当前 Strategy 的 NavMesh 移动和 TwinStick 的 Actor 弹体都可以作为第一版入口，但战斗结算必须收敛到 Combat Subsystem。
