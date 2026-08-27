# 24 M6 复杂技能集决策

## 1. 冻结范围

本文件冻结 M6 的 Frost Arrows 快照、Fissure 线段结算与 blocker、Aura child reconcile，以及 SpellBlock、Break、Debuff Immunity、Dispel Immunity 的阶段语义。它补充 [04 Modifier、属性与 Motion](04-Modifier-Attributes-Motion.md)、[07 Order 与移动](07-Order-Movement.md) 和 [09 示例技能](09-Example-Skills.md)。

## 2. Frost Arrows

- Frost Arrows 是 `Passive + Attack + AutoCast` Ability，其 Intrinsic Modifier 只负责参与 `Orb.Primary` 两阶段仲裁。
- `CanClaimAttack` 无副作用检查 per-Spec AutoCast、Silenced/Break、Mana 和等级 special；`OnAttackClaimed` 再次检查后唯一扣除 Mana。
- winner 把 bonus damage、slow duration、slow magnitude、on-hit Modifier 和可选 ProjectileData 完整复制到 AttackRecord。
- 已发射 Projectile 只持 AttackHandle；Ability 升级、AutoCast 切换或移除不回读和修改既有 Record。
- miss、fizzle 和 stale Record 不执行 landed-only slow。

## 3. Fissure

- 服务器以 `QueryUnitsAlongSegment` 对有限线段做一次去重和稳定排序，Damage、Stun、Knockback 分别进入 DamageSubsystem、ModifierComponent 与 MotionComponent。
- 目标沿线的 knockback 方向取目标到最近线段点的法向；退化重合时使用稳定的线段右法向。
- 视觉占位由 `bVisualOnly` Thinker 承担，仍由 Combat Scheduler 管理生命周期，不拥有伤害或控制权威。
- `ACombatFissureBlocker` 使用 `CombatBlocker` Profile、关闭 Tick，并由 Scheduler 到期销毁。创建和销毁都会通知相交 Move/Chase 主动取消旧 MoveRequest 并 Pump 当前 Order。
- OrderHandle 不因 repath 改变；每次导航尝试额外递增 attempt generation，旧 MoveFinished/测试回调不能推进新尝试。

## 4. Aura（关闭 GAP-014 的基线）

- `UCombatAuraSubsystem` 是每个 World 唯一 Aura registry；每条记录保存 Owner、Owner LifeGeneration、查询规则、child Modifier 和 `Target -> child handle/life`。
- reconcile 使用 Combat Scheduler `Coalesce` 和 TargetingSubsystem，不使用 Actor Tick/Timer/蓝图队伍比较。
- 新进入目标只 Apply 一次普通 child Modifier；离开、换队、死亡、Owner 死亡、显式取消与 EndPlay 均幂等移除 child。
- child 的 Source 固定为 Aura Owner，DefinitionId 使用普通 ModifierData 身份；child 的驱散和死亡规则仍由自身定义决定。
- Break 可按 Aura Spec 显式禁用 owner Aura；禁用期间移除全部 child，解除后由下一次/主动 reconcile 恢复。

## 5. 高级状态矩阵（关闭 GAP-016 的基线）

| 规则 | 阶段 | 第一版语义 |
| --- | --- | --- |
| SpellBlock | UnitTarget Ability 的 SpellStarted commit 之后、Action 之前 | 仅带 `Ability.Behavior.SpellBlockable` 的技能可触发；消耗一个最高优先级 SpellBlock Runtime；Cost/Cooldown 已提交，但不执行 Ability Action |
| Break | Modifier Hook/Orb/Aura reconcile | 只停用显式 `bDisabledByBreak` 的 Runtime 行为，不删除 Modifier、AbilitySpec 或已经发射的 AttackRecord/Projectile |
| Debuff Immunity | Modifier Apply | 拒绝新的 `bIsDebuff` Modifier；既有 Debuff 不被追溯移除，自然过期和显式清理照常 |
| Dispel Immunity | Dispel | 拒绝本次 Basic/Strong Dispel；自然过期、死亡清理、owner cleanup 和 `RequestRemoveSelf` 不受影响 |

四条规则分别使用独立 Native GameplayTag 与 Failure/Event，不由 `State.MagicImmune` 代替。

## 6. 模板 Gate

- 公共 validator 检查 Ability Class/Data 单向身份、Runtime schema、示例所需 special/引用与定义唯一性。
- 静态旁路扫描覆盖项目自有技能代码中的直接 Health 写入、`SetActorLocation`、`TimerManager` 和模板 `ProjectileImpact`。
- 每个 M6 示例必须有 Automation、PIE 操作说明和预期 Combat Event 序列后，才可复制为新技能模板。
