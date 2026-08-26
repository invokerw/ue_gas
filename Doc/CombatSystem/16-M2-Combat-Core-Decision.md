# 16 M2 战斗内核实现决策

> 状态：已冻结用于 M2 实现
> 日期：2026-08-25
> 适用任务：ATR-001、ATR-002、LIFE-001、CMB-001..004、MOD-001..005、DEMO-201..202、OBS-002

本文关闭 M2 到期的 GAP-005、GAP-012 与 GAP-013，并把既有设计中的建议基线收敛为可测试规则。若后续产品需求改变这些规则，必须新增 superseding ADR，并同步迁移测试与内容 schema。

## 1. ADR-024：恢复节拍与死亡行为

- `HealthRegen` 与 `ManaRegen` 表示每秒恢复量；服务器使用 `UCombatSchedulerSubsystem` 的固定 `0.25 s` repeating task，Catch-up 策略为 `Coalesce`。
- 单次结算量为 `Rate * Interval * TickCount`，因此卡顿不会损失恢复量，也不依赖帧率。
- Health 恢复调用 `UCombatHealSubsystem`，继续经过 Heal Hook、同步结果槽与结构化日志；Mana 恢复通过 Instant GameplayEffect 写入并由 AttributeSet clamp。
- `Dying`、`Dead`、`Respawning` 暂停恢复；重新进入 `Alive` 后从新相位开始，不补结死亡期间的 tick。
- 所有恢复调度由组件集中持有，禁止 Actor Tick、Timer 或直接写 Attribute。

该决策关闭 GAP-012。

## 2. ADR-025：死亡、复活与保留策略 v1

- `Alive -> Dying -> Dead` 在一次服务器同步调用中完成；`PreviousHealth > 0 && NewHealth == 0` 才能触发致死请求。
- 进入 `Dying` 后取消活动 Ability、移动和该生命代次调度；按稳定顺序移除 `bRemoveOnDeath=true` 的 Modifier。
- AbilitySpec、Ability 等级、AutoCast 与已经开始的 cooldown 跨死亡保留。M2 不产生金币、经验等 gameplay 奖励，只记录死亡来源、目标生命代次和事件链。
- `bRemoveOnDeath=false` 的 Modifier 保留原绝对 `ExpireAt`，死亡期间暂停 Hook/Think；若在死亡期间已经到期，则复活时直接移除且不 catch up。
- `Dead -> Respawning -> Alive` 先验证有限位置和 generation 溢出，再递增 `uint32 LifeGeneration`。Health、Mana 默认恢复到新生命的 MaxHealth、MaxMana；恢复碰撞和状态响应后广播一次 Respawn 事件。

该决策关闭 GAP-013。

## 3. ADR-026：状态抗性与周期边界 v1

- 只有同时标记为 Debuff 且 `bDurationAffectedByStatusResistance=true` 的 Modifier 才受状态抗性影响。
- 生效持续时间为 `BaseDuration * (1 - Clamp(StatusResistancePct, 0, 0.9))`；Think interval 不变，后续状态抗性变化不追溯重排已生效 Modifier。
- 自然过期结算所有严格早于 `ExpireAt` 的 tick；恰好位于边界的 tick 仅在 `bTickOnExpire=true` 时执行。
- Refresh 的 `PreservePhase` 保留原 Think 相位，`ResetInterval` 从刷新时刻重新建立相位；两者都从刷新时刻重算 `ExpireAt`。
- Purge、死亡移除和显式 Remove 不 catch up。Basic Dispel 移除 Basic，Strong Dispel 移除 Basic 与 StrongOnly；NotDispellable 永不由 Dispel 移除。

该决策关闭 GAP-005。

## 4. M2 公共实现边界

- Health 的最终变化只由 Damage/Heal Instant GameplayEffect 写入 `IncomingDamage` / `IncomingHealing`，AttributeSet clamp 后通过 EventId 同步结果槽回报真实 delta。
- Damage/Heal Subsystem 是唯一公开结算入口；Modifier Runtime 只能通过 Hook 修改事件，不能直接写 Health。
- 一个 Modifier Runtime 必须对应一个 ActiveGameplayEffectHandle。属性、Tag 与 Runtime 同时创建、刷新和销毁，不维护第二套残留属性。
- 每个 Hook 阶段按 `Priority desc -> ApplySequence asc` 创建强引用快照；Apply、Remove、Refresh、Dispel 在阶段结束后按 FIFO 提交。
- Damage、Heal、Death 与 Respawn 只各发出一条结构化结果日志；反伤使用新 EventId、继承 RootEventId，并自动附加 `Reflection` 与 `NoLifesteal` 防递归标志。
- M2 为 OBS-002 增补 `Event.Combat.ModifierApplied` 与 `Event.Combat.ModifierRemoved` 两个 Native Tag；Apply、Refresh、Remove 均记录 DefinitionId、层数、Handle、Source/Target 与 LifeGeneration。

## 5. M2 自动化最低断言

- Attribute 默认值、初始化幂等、数值 clamp、恢复节拍与死亡暂停。
- 物理正/负护甲、魔抗、纯粹、HPLoss、魔免、无敌、过量治疗与致死 exactly-once。
- 两个护盾的稳定消耗顺序、耗尽后的 deferred removal，以及 MagicResist 随 GE 同步消失。
- DOT 边界 tick、两种 Refresh、Basic/Strong Dispel、状态抗性缩时和多个 Stun 来源的 Tag count。
- Slow 的 Attribute 变化、Stun 状态响应、反伤 RootEventId/Depth 和吸血按真实 AppliedDamage 计算。
- Combat Result Log 的 Requested、Mitigated、Absorbed、Applied、Source/Target、LifeGeneration 与 Flags 可断言。
