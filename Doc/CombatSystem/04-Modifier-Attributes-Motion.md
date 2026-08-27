# 04 Modifier、属性与 Motion

## 1. 双层模型

GameplayEffect 适合表达持续时间、叠层、属性修改、Granted Tag 和 GameplayCue，但不适合 Dota Modifier 的大量有状态 Hook。第一版采用：

```text
Active GameplayEffect = 属性、Tag、Duration、Stack、Cue 的唯一 GAS 表达
Modifier Runtime      = Hook、实例状态、状态机和副作用
```

同一个最终属性不得同时由 GE 和 Runtime 提供。

## 2. GameplayEffect 层

用于：

- Instant/Duration/Infinite。
- Stack policy、持续时间刷新。
- Armor、MoveSpeed、AttackDamage、MagicResist 等属性修改。
- `State.Stunned`、`State.Silenced`、`State.Rooted` 等 Granted Tag。
- 非 Health/Mana 的简单周期属性效果。
- GameplayCue、UI 持续时间和层数来源。

需要进入战斗事件链的 DOT/HOT 不直接由 Periodic GE 修改 Health，而由 Runtime/Scheduler 调用统一 Damage/Heal Subsystem。

## 3. Runtime 层与 Hook

支持的语义 Hook：

```text
OnCreated / OnDestroyed / OnRefresh / OnStackChanged
OnIntervalThink
OnPreDealDamage / OnPreTakeDamage / OnDamageBlock
OnPostDealDamage / OnPostTakeDamage
OnPreTakeHeal / OnPostTakeHeal
OnAbilityExecuted
CanClaimAttack / OnAttackClaimed
OnAttack / OnAttackLanded / OnAttackFail / OnAttackRecordDestroy
GetAttackProjectileName
OnMotionTick / OnMotionInterrupted
```

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
    void OnIntervalThink(const FCombatScheduledTickContext& TickContext);

    UFUNCTION(BlueprintNativeEvent)
    void OnPreTakeDamage(UPARAM(ref) FCombatDamageEvent& Event);

    UFUNCTION(BlueprintNativeEvent)
    void OnDamageBlock(UPARAM(ref) FCombatDamageEvent& Event);

    UFUNCTION(BlueprintNativeEvent)
    bool CanClaimAttack(const FCombatAttackRecord& Record) const;
};
```

Runtime 可以保存护盾剩余值、法球认领状态和 motion 请求等实例状态，但不能：

- 维护第二套 MoveSpeed/Armor/AttackDamage 最终聚合。
- 绕过 ASC 直接修改 Health/Mana。
- 在客户端作为权威数据源。
- 自建 Tick/Timer。

## 4. ModifierComponent 所有权

`UCombatModifierComponent`：

- 提供服务器唯一 `ApplyModifier(ModifierData, Context)`。
- 把 Modifier DefinitionId 写入自定义 `FCombatSourceContext`；不得把 DataAsset UObject 作为复制身份写入 SourceObject。
- 服务器可在 Component 的本地 pending/application 映射中持有已加载 ModifierData，GE 添加回调以 Context DefinitionId 校验并关联；客户端只解析 DefinitionId。
- 监听合法 GE 添加并创建 Runtime；没有合法 DefinitionId/本地定义的普通 GE 不创建 Runtime。
- 维护 `FActiveGameplayEffectHandle -> Runtime` 的 `UPROPERTY` 映射。
- 在 GE 移除/过期时销毁 Runtime。
- 派发 Damage/Heal/Attack/Ability Hook。
- 处理叠层、刷新、驱散和 Scheduler Handle。

一个 ActiveGE Handle 对应一个 Runtime。GAS 在同一 Handle 增加 stack 时只调用该实例的 `OnStackChanged`/`OnRefresh`。需要每次施加独立状态的效果必须配置为独立 ActiveGE。

ThinkInterval 为 0 时不注册；大于 0 时由 Component 持有 ScheduleHandle。Refresh 根据 PreservePhase/ResetInterval 重排，Destroyed/EndPlay 必须取消。边界 tick 规则见 [02](02-Scheduler-Transactions.md)。

## 5. Hook 顺序与重入

ModifierData 提供整数 Priority，Component 分配单调递增 ApplySequence。每个阶段排序：

```text
Priority descending -> ApplySequence ascending
```

派发前取得 Runtime 强引用快照。Hook 内 Apply/Remove/Refresh/Purge 写入 deferred queue，当前阶段结束后按提交顺序执行。嵌套 Damage/Heal 分配新 EventId，继承 RootEventId，并遵守递归深度和 flags。

## 6. 属性与聚合

| 语义 | GAS Attribute |
| --- | --- |
| 生命/魔法 | `Health`、`MaxHealth`、`Mana`、`MaxMana` |
| 防御 | `Armor`、`MagicResist`、`Evasion` |
| 普攻 | `AttackDamage`、`AttackSpeed`、`BaseAttackTime`、`AttackRange` |
| 移动 | `MoveSpeed` |
| 恢复 | `HealthRegen`、`ManaRegen`、`LifestealPct` |
| 技能 | `SpellAmplifyPct`、`CooldownReductionPct`、`CastRangeBonus` |
| 控制 | `StatusResistancePct` |
| 治疗 | `HealAmplifyPct`、`HealReceivedPct`（补齐原设计） |

聚合基准：

```text
final = (base + additive) * (1 + additive_pct) * total_multiplier
```

GAS ModifierOp 覆盖常规聚合；Dota 特殊公式由纯 C++ Calculator 或 AttributeSet clamp 负责，不能两处重复。所有输入先验证有限值；百分比和速度上限必须由显式规则定义。

M0 已冻结 Numeric Policy v1：请求非法值拒绝，聚合 Attribute 在消费点按集中常量 clamp，中间不取整；Health/Mana、Armor、MagicResist、Evasion、增幅、吸血、CDR 和状态抗性的具体边界见 [14 M0 设计冻结](14-M0-Design-Freeze.md#51-numeric-policy-v1)。任何边界或公式变化都必须增加 FormulaVersion，不能在局部 Runtime 覆盖。

动态属性推荐：

1. 创建/刷新 GE 时把计算值写入 SetByCaller，由 GE Modifier 修改 Attribute。
2. 高频动态值由事件或 Scheduler 刷新专用 GE，不在每次查询时回调蓝图聚合。

## 7. 状态标签与响应

```text
State.Alive
State.Dying
State.Dead
State.Respawning
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

典型消费者：

- Ability：Stunned/Hexed/Silenced。
- 移动：Stunned/Rooted/Frozen。
- 攻击：Stunned/Disarmed/Hexed。
- 目标过滤：Dead/Untargetable/OutOfGame/Invulnerable/MagicImmune。
- 碰撞/UI：NoUnitCollision/NoHealthBar。

Character 状态响应器订阅 Tag count change，并根据聚合 count 是否大于 0 更新移动、碰撞和 UI。不能在某个 GE 移除时直接恢复默认状态。

状态抗性缺少最终产品语义，暂定只缩短可配置 Debuff Duration，不改变 Think 间隔和已经快照的总伤害；最终规则必须在 M2 前由 [GAP-005](12-Decisions-Gaps.md) 关闭。

## 8. 驱散

ModifierData 保存：

- `DispelRule`: NotDispellable、Basic、StrongOnly。
- `bIsDebuff`、`bRemoveOnDeath`。
- 默认整 Handle 移除，是否逐层驱散需显式配置。

驱散请求声明方向：Buff、Debuff 或 Both。Basic 移除 Basic；Strong 移除 Basic 和 StrongOnly。Component 在请求开始时快照候选，按 Priority/ApplySequence 排序，将 ActiveGE Handle 加入 deferred removal queue。NotDispellable 只由自身生命周期、死亡规则或明确管理接口移除。

## 9. Aura 补齐

M6 已按 [24 M6 复杂技能集决策](24-M6-Content-Decision.md#4-aura关闭-gap-014-的基线)补齐 Aura 所有权：

- `UCombatAuraSubsystem` 是每个 World 的唯一 registry；记录 Owner/LifeGeneration、Targeting 规则、调度和 child 映射。
- Scheduler 使用 Coalesce 低频查询目标；每个受影响目标持有普通 child Modifier，registry 保存 `Target -> ModifierHandle/LifeGeneration`，不用每帧重复 Apply。
- 进入、离开、死亡、队伍变化、Owner 销毁、Break/解除 Break 都走幂等 reconcile。
- child Modifier 的来源 Context 保存 Aura Owner 和 DefinitionId；驱散规则由 child 定义决定。
- Aura 查询必须使用统一 Targeting/Team 规则，不能在蓝图里自行比较队伍。

Aura 已由 M6/EXT-601 实现并关闭 GAP-014；自动化覆盖 add/remove/repair、换队、Break、Owner 死亡和取消清理。

## 10. Motion Controller

Runtime 不直接 Tick `SetActorLocation`。所有强制位移提交给 `UCombatMotionComponent`：

```text
TryAcquireMotion(Request) -> FCombatMotionHandle
UpdateMotion(Handle, DeltaTime)
ReleaseMotion(Handle, FinishReason)
```

MotionComponent 分别维护 Horizontal/Vertical 通道，并定义 Priority、抢占、碰撞和结束校正。同一通道一个 owner；高优先级可中断低优先级，被中断 Runtime 收到 `OnMotionInterrupted`。

开始时暂停 AI MoveTo，通过 RootMotionSource 或 CharacterMovement 受控移动。结束时先校正到 NavMesh，再通知 OrderComponent 重新判定队首。服务器权威，客户端使用 CharacterMovement/RootMotionSource 正常校正和表现。

`OnMotionTick` 是连续运动，只接收 DeltaSeconds，不进入 Scheduler，也不结算周期伤害。

## 11. 死亡时 Modifier 处理

M0 已关闭基础生命周期状态机和清理顺序，完整契约见 [14 M0 设计冻结](14-M0-Design-Freeze.md#3-dec-002unit-生命状态)：

- Death 事件开始后阻止新 Apply（标记允许作用于尸体的 Modifier 除外）。
- `bRemoveOnDeath=true` 的 ActiveGE 进入 deferred removal；`false` 的效果默认保留到复活或自然过期。
- Motion 一律释放；NoUnitCollision/NoHealthBar 等最终状态仍由聚合 Tag count 驱动。
- `OnDestroyed` 仍按稳定顺序执行，但禁止在死亡清理期间无条件重新施加自身。
- `bRemoveOnDeath=false` 的 Runtime/ActiveGE 是显式跨生命对象，保留原绝对到期时间，但非 Alive 时默认停用普通战斗 Hook，且不得持有上一 LifeGeneration 的 Order/Attack/Motion Handle。
- Respawn 在预检后递增 Unit LifeGeneration，重置基础属性、Order/Attack/Motion 和 life-bound Schedule；AbilitySpec/AutoCast/cooldown 默认保留，Intrinsic Modifier 幂等 reconcile。

尸体时长、奖励和按 Unit 类型覆盖保留/重置策略继续由 [GAP-013](12-Decisions-Gaps.md) 在 M2 收口；在此之前不得偏离 M0 默认值。

## 12. 最低验收

- ActiveGE 与 Runtime 一一对应，叠层不额外创建实例，移除后无悬空 Schedule。
- 相同 Priority 使用 ApplySequence 稳定；Hook 内自移除/新增不破坏遍历。
- GE 是属性最终来源，动态 Runtime 能通过 SetByCaller/专用 GE 更新。
- 多个相同状态 Tag 叠加时，移除一方不会提前解除状态。
- Basic/Strong 驱散、整 Handle/逐层策略和死亡移除可重复验证。
- Motion 抢占、中断、碰撞、NavMesh 校正和 Order 恢复完整闭环。
