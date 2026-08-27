# 09 示例技能

示例技能不是演示列表，而是里程碑验收用的纵向切片。每个示例必须使用公共 Damage/Modifier/Ability/Projectile/Order API，禁止为过关增加技能专用旁路。

## 1. 覆盖矩阵

| 示例 | 核心覆盖 | 最早进入节点 |
| --- | --- | --- |
| 无目标治疗（测试技能） | Ability 生命周期、Heal、Target self、Cost/Cooldown | M3 |
| 单位目标伤害（测试技能） | TargetData、追击、Damage、魔免、失败结果 | M3/M4 |
| Magic Shield | ActiveGE/Runtime、Block、deferred removal、稳定顺序 | M2 |
| Lina Dragon Slave | Point target、线性查询/Projectile、多目标 Damage、快照 | M5 |
| Pudge Meat Hook | Linear Projectile、首命中、Modifier、Horizontal Motion、Order 恢复 | M5 |
| Drow Frost Arrows | Intrinsic Modifier、AutoCast、法球仲裁、资源提交、on-hit/Projectile 快照 | M6 |
| Earthshaker Fissure | 线伤、stun、knockback、Thinker、阻挡/repath | M6 |

先实现前三个测试技能建立最小纵向切片，再做具有复杂表现的英雄技能。

## 2. 无目标治疗

数据：

- `Ability.Behavior.NoTarget`。
- ManaCost、Cooldown、CastPoint、special `heal_amount`。
- `Action.Heal`，Target=self。

验收：

- 前摇内中断不扣资源；SpellStarted 提交一次。
- 满血时 AppliedHealing=0，OnPostTakeHeal 不把请求值当真实治疗。
- Order 在 SpellStarted 后释放，不等待 backswing/cooldown。
- 客户端不能伪造 Heal Amount。

## 3. 单位目标伤害

数据：

- `Ability.Behavior.UnitTarget`，`TargetTeam.Enemy`。
- CastRange、LOS、ManaCost、Cooldown、special `damage`。
- Magical Damage Action。

验收：

- 距离不足由 Order 追击，到达后服务器复核。
- Friendly、Untargetable、OutOfGame 和普通 MagicImmune 目标被正确拒绝/阻挡。
- 目标在前摇中失效时按 TargetLostPolicy 失败。
- DamageResult、Ability 事件和 Order 释放各 exactly once。

## 4. Magic Shield

Modifier：

- GE 提供 `MagicResist +0.10`，Runtime `_remaining = shield_amount`。
- `OnDamageBlock` 只处理 Magical，吸收 `min(remaining, event.amount)`。
- 修改 Event.Amount，累加 Event.AbsorbedAmount。
- remaining 归零请求移除自身 GE，实际移除延迟到当前 Hook 阶段结束。

验收：

- MagicImmune blocked 和 HPLoss 在 Block 前返回，不消耗 shield。
- 两个 Shield 按 Priority/ApplySequence 稳定消耗。
- Runtime 移除后 GE 的 MagicResist 同时消失，无第二套属性残留。
- DamageResult 中 Absorbed/Applied 与真实 Health delta 一致。

## 5. Lina Dragon Slave

数据：

- PointTarget、AoE；CastPoint 0.45。
- Cooldown、ManaCost、special：damage、radius、range、projectile_speed。

实现可选择 LinearProjectile 或权威线性 sweep；第一版优先复用 Projectile 基础设施：

1. SpellStarted 读取权威 target point，计算规范化方向。
2. Spawn LinearProjectile，快照 range/radius/damage/team/RootEventId。
3. 穿透多个敌人，每个 Victim 至多一次。
4. 每个命中使用新 EventId 调 Magical Damage。
5. Cue 通过相同 ProjectileId 表现/reconcile。

验收：

- 多目标沿路径稳定排序，不受 overlap 返回顺序影响。
- Ability 结束后 Projectile 继续结算。
- 服务器重算目标，不接受客户端命中列表。

## 6. Pudge Meat Hook

数据：

- PointTarget。
- special：damage、length、width、missile_speed、drag_speed。
- LinearProjectile `bDestroyOnFirstHit=true`。

命中：

1. Magical Damage。
2. Apply `modifier_hook_drag`。
3. Modifier 授予 Stunned/NoUnitCollision。
4. Runtime 向 MotionComponent 请求高优先级 Horizontal motion；获取失败立即结束 Modifier。
5. Motion 朝施法者快照位置或明确跟随点移动，统一处理 sweep/collision。
6. 结束/中断释放 Handle，校正 NavMesh，通知 Order 重判。

验收：

- 首命中 exactly once，WorldStatic/友军/目标丢失遵守 ProjectileData。
- 高优先级 Motion 可抢占，旧 Motion 收到一次 Interrupted。
- 目标死亡、施法者死亡、Motion 获取失败和 Actor EndPlay 都能清理。
- 拖拽结束不恢复旧的无效 MoveRequest，只 Pump 当前 Order generation。

## 7. Drow Frost Arrows

数据：

- Passive、Attack、AutoCast。
- Intrinsic Modifier `modifier_frost_arrows`。
- special：bonus_damage、slow_duration、slow_pct、mana_cost。

流程：

1. `UCombatFrostArrowsRuntime::CanClaimAttack` 从 `AbilityOwnerHandle` 查找当前 Spec，检查 AutoCast、Silenced、Broken、等级 special、Mana 和 `Orb.Primary`，只声明候选。
2. winner 的 `OnAttackClaimed` 原子提交 Mana，并把 bonus、ProjectileData、slow Modifier/duration/`slow_pct` 参数覆盖快照写入 Record。
3. 远程 Projectile 只持 AttackHandle。
4. Landed 后使用 Record 的 runtime parameter override 施加 slow，不重新读 Ability 当前等级或 DataAsset 当前值。

验收：

- 未胜出候选不扣资源；winner 提交失败可尝试下一候选。
- 发射后升级/移除 Ability 不改变本 Record 的 slow/damage。
- Miss/target fizzle 不执行 landed-only slow，并完整销毁 Record。

## 8. Earthshaker Fissure

技能：

- PointTarget，线性范围伤害 + stun + knockback。
- 中线生成 visual-only `ACombatThinker` 与 `ACombatFissureBlocker`。

迭代：

1. M6a（已实现）：`QueryUnitsAlongSegment` 稳定去重，伤害、stun、knockback 分别进入公共 Damage/Modifier/Motion 管线；视觉 Thinker 不拥有 gameplay 结算。
2. M6b（已实现）：物理 blocker 创建/移除时通知路径穿过 bounds 的 Move/Chase，保持 OrderHandle 并递增 navigation attempt generation 后 repath。
3. 扩展：Runtime NavModifier，持续时间结束安全移除并刷新导航。

验收：

- 目标查询去重、稳定排序；Damage/Stun/Knockback 各自使用明确 Context。
- Thinker 默认关闭 Tick，生命周期由 Scheduler 驱动。
- 阻挡创建/销毁不会让旧 MoveFinished 回调推进新 Order。
- 第一版不以异步 NavMesh 更新完成作为 gameplay 正确性的唯一条件。

## 9. 示例资产统一要求

- 每个 DefinitionId 唯一，Ability Class/Data 单向引用。
- 所有平衡数值来自 DataAsset special。
- 所有 Damage/Heal/Modifier/Projectile/Thinker 从公共服务器入口创建。
- 每个示例附 Automation Spec、PIE 操作说明和预期 Combat Event 序列。
- DataAsset、GameplayEffect、蓝图和测试地图优先通过 UE MCP 创建/检查，并记录精确资产路径、编译结果和回读值。
- 示例通过对应 Gate 后才可作为后续技能模板。
- M6 示例复制前还必须通过 [25 M6 技能模板检查表](25-M6-Skill-Template-Checklist.md)。
