# 00 开发进度台账

> 最后更新：2026-08-27
> 当前里程碑：M6 已验收（复杂技能集）
> 总进度：68/82 Task 完成，7/9 里程碑由用户验收

本文件是项目执行状态的唯一来源。[10 实施路线图](10-Implementation-Roadmap.md)定义任务内容和依赖，本文件记录实际状态、验证证据和用户验收结论。

## 1. 状态定义

| 状态 | 含义 |
| --- | --- |
| 未开始 | 尚未产生本任务范围内的实现或资产修改 |
| 进行中 | 已开始工作，但未满足任务验收标准 |
| 已完成 | Task 验收标准和测试已满足，所属里程碑尚未提交用户验收 |
| 待验收 | 里程碑 Gate 已通过，已暂停并等待用户验收 |
| 需修正 | 用户验收提出修改，必须修正并重新执行 Gate |
| 已验收 | 用户明确确认当前里程碑通过 |
| 阻塞 | 存在无法在当前权限/环境/决策下继续的明确阻塞 |

状态流转：

```text
未开始 -> 进行中 -> 已完成
里程碑全部 Task 已完成 -> 待验收
待验收 -> 需修正 -> 进行中/待验收
待验收 -> 已验收
已验收后，收到用户“继续下一阶段” -> 下一里程碑进行中
```

只有用户可以把里程碑从“待验收”改为“已验收”。Task 的“已完成”不代表用户已经接受所属里程碑。

## 2. 里程碑总览

| 里程碑 | 名称 | Task 进度 | Gate | 用户验收 | 完成/验收日期 | 备注 |
| --- | --- | ---: | --- | --- | --- | --- |
| M0 | 设计冻结 | 7/7 | 通过 | 已验收 | 2026-08-24 / 2026-08-24 | M1 已授权 |
| M1 | GAS 基座 | 13/13 | 通过 | 已验收 | 2026-08-25 / 2026-08-25 | 源码 UE 5.8.0 Server/Client Target 构建通过；独立 Server/Game 进程连接 smoke 通过；M2 已授权并完成实现 |
| M2 | 战斗内核 | 15/15 | 通过 | 已验收 | 2026-08-25 / 2026-08-26 | G2 通过；Editor/Server/Client 构建、Automation 12/12 和独立联机 smoke 通过；M3 已授权；见 [17](17-M2-Acceptance.md) |
| M3 | 可施法切片 | 10/10 | 通过 | 已验收 | 2026-08-26 / 2026-08-26 | Editor/Server/Client 构建、Automation 17/17 和独立联机 smoke 通过；用户验收通过；见 [19](19-M3-Acceptance.md) |
| M4 | Order 与普攻 | 8/8 | 通过 | 已验收 | 2026-08-26 / 2026-08-26 | Editor/Server/Client 构建、Automation 23/23 和独立联机追击/连续近战 smoke 通过；用户验收通过；见 [21](21-M4-Acceptance.md) |
| M5 | Projectile、Thinker 与 Motion | 9/9 | 通过 | 已验收 | 2026-08-26 / 2026-08-26 | Editor/Server/Client 构建、Automation 27/27 和独立联机 Projectile spawn/hit/finish smoke 通过；用户验收通过；见 [23](23-M5-Acceptance.md) |
| M6 | 复杂技能集 | 6/6 | 通过 | 已验收 | 2026-08-26 / 2026-08-27 | Editor/Server/Client 构建、Automation 32/32、独立联机 M6 场景 smoke 通过；中文可见说明整改后用户验收通过；见 [26](26-M6-Acceptance.md) |
| M7 | 联机、UI、工具和性能 | 0/9 | 未开始 | 未开始 | — | — |
| M8 | 候选发布 | 0/5 | 未开始 | 未开始 | — | — |

## 3. M0：设计冻结

| Task | 需求名称 | 状态 | 完成证据/备注 |
| --- | --- | --- | --- |
| DEC-001 | 队伍与目标关系 | 已完成 | [14 §2](14-M0-Design-Freeze.md#2-dec-001队伍与目标关系)；ADR-017；关闭 GAP-001 |
| DEC-002 | Unit 生命状态 | 已完成 | [14 §3](14-M0-Design-Freeze.md#3-dec-002unit-生命状态)；ADR-018；关闭 GAP-002 |
| DEC-003 | Ability 授予与等级 | 已完成 | [14 §4](14-M0-Design-Freeze.md#4-dec-003ability-授予等级和-autocast)；ADR-019；关闭 GAP-003 |
| DEC-004 | 数值与 RNG | 已完成 | [14 §5](14-M0-Design-Freeze.md#5-dec-004数值与-rng)；ADR-020；关闭 GAP-006/GAP-007 |
| DEC-005 | 碰撞、LOS 与地图单位 | 已完成 | [14 §6](14-M0-Design-Freeze.md#6-dec-005碰撞los-和地图单位)；ADR-021；关闭 GAP-009 |
| DEC-006 | GameplayTag 与资产身份 | 已完成 | [14 §7](14-M0-Design-Freeze.md#7-dec-006gameplaytag-与资产身份)；ADR-022；关闭 GAP-004 |
| DEC-007 | P0 Gap 评审 | 已完成 | [14 §8](14-M0-Design-Freeze.md#8-dec-007p0-gap-总评审)；GAP-020 接受 M1/TST-003 到期方案 |

## 4. M1：GAS 基座

| Task | 需求名称 | 状态 | 完成证据/备注 |
| --- | --- | --- | --- |
| FND-001 | 启用 GAS 与 Combat 碰撞 | 已完成 | `ue_gas.uproject`、`ue_gas.Build.cs`、`DefaultEngine.ini`；Editor Target 构建成功；`TagsAndCollision` 通过 |
| FND-002 | Native Gameplay Tags | 已完成 | `CombatTags.h/.cpp`；自动化查询通过；MCP `ListTags(Combat)` 回读成功 |
| FND-003 | PrimaryAsset 基类 | 已完成 | `CombatDefinitionData.*`、AssetManager scan、redirect/唯一性校验；`CombatUnit:team_one/team_two` 冷启动发现通过 |
| FND-004 | 公共 Handle/Result/Numeric/RNG | 已完成 | `CombatTypes.*`、`CombatNumericPolicy.*`、`CombatRngSubsystem.*`；冻结向量与边界测试通过 |
| FND-005 | 自定义 EffectContext | 已完成 | `CombatGameplayEffectContext.*`、`CombatAbilitySystemGlobals.*`；实际分配、Duplicate、traits、NetSerialize round-trip 通过 |
| FND-006 | Combat Unit 与 ASC | 已完成 | `CombatUnitCharacter.*`、`CombatAbilitySystemComponent.*`、`CombatTeamSubsystem.*`；四 NetMode ActorInfo 用例与 PIE ASC 回读通过 |
| FND-007 | Combat Scheduler | 已完成 | `CombatSchedulerSubsystem.*`；稳定顺序、三 policy、三层 budget、generation、reentry、owner/world teardown 测试通过；关闭 GAP-025 |
| FND-008 | Deferred Operation 基件 | 已完成 | `CombatDeferredOperationQueue.*`；嵌套阶段、稳定快照和回调内新增延迟提交通过 |
| TST-001 | Automation 基架 | 已完成 | `CombatAutomationWorldFixture.*`、`CombatFoundationTests.cpp`；冷启动 `Combat.Foundation` 7/7 通过 |
| TST-002 | PIE 测试地图 | 已完成 | `/Game/Combat/Tests/L_CombatTest`、`CombatTestScenarioActor.*`；NavMesh 回读，PIE 自动生成 Team 1/2，停止后清理通过 |
| TST-003 | Dedicated Server/Client 构建目标 | 已完成 | 源码 UE 5.8.0（`D:\UE\UE`）Development Server/Client Target 构建成功；安装版 UE 5.8.1 独立 Server/Game 进程完成 127.0.0.1 连接，Server 记录 `Join succeeded`，Client 记录 `Welcomed by server`；场景日志确认 2 Unit、Team 1/2、ASC ActorInfo 与 `State.Alive`；见 [15](15-M1-Environment-Decision.md)，关闭 GAP-020 |
| OBS-001 | 事件与调试骨架 | 已完成 | `CombatEventSubsystem.*`、结构化 `LogCombat`、Event/Root/Depth、Handle `ToString`；失败原因日志测试通过 |
| MCP-001 | UE MCP Smoke | 已完成 | endpoint/toolset discovery、Editor/PIE World 区分、Tag/资产/地图/Actor/ASC 回读、MCP Automation 6/6 均通过 |

## 5. M2：战斗内核

| Task | 需求名称 | 状态 | 完成证据/备注 |
| --- | --- | --- | --- |
| ATR-001 | AttributeSet | 已完成 | `CombatAttributeSet.*`、`CombatRegenerationComponent.*`；21 项聚合/Meta Attribute、clamp、0.25 s Coalesce 恢复与死亡暂停测试通过；关闭 GAP-012 |
| ATR-002 | Unit 初始化 | 已完成 | `CombatUnitCharacter.*`、`CombatDefinitionData.*`、`CombatAbilitySystemComponent.*`；批量初始化 GE、AbilitySet/AutoCast、幂等与非法资产拒绝通过 |
| LIFE-001 | Unit 生命状态组件 | 已完成 | `CombatUnitLifecycleComponent.*`；同步 Death、旧生命调度取消、保留 Modifier、合法 Respawn 与 generation 递增通过；关闭 GAP-013 |
| CMB-001 | Combat 事务结果槽 | 已完成 | `CombatTransactionSubsystem.*`；EventId 槽位 Begin/Report/Consume exactly-once 与错误生命周期拒绝通过 |
| CMB-002 | Damage Calculator | 已完成 | `CombatDamageCalculator.*`；正负护甲、魔抗、Pure、SpellAmp/NoSpellAmplification 与非法数值测试通过 |
| CMB-003 | DamageSubsystem | 已完成 | `CombatDamageSubsystem.*`；权限/生命状态、免疫/绕过、HPLoss、Shield、致死、反伤与吸血闭环通过 |
| CMB-004 | HealSubsystem | 已完成 | `CombatHealSubsystem.*`；HealAmp/HealReceived、clamp、overheal、满血 Applied=0、Dead 不复活与非法输入通过 |
| MOD-001 | ModifierData 与 Runtime | 已完成 | `CombatDefinitionData.*`、`CombatModifierRuntime.*`；生命周期、Hook、实例状态和数据校验接口完成 |
| MOD-002 | ModifierComponent | 已完成 | `CombatModifierComponent.*`；一 Runtime 对应一 Active GE，Handle/ownership、叠层与刷新测试通过 |
| MOD-003 | 稳定排序与 Deferred Hook | 已完成 | Priority desc/ApplySequence asc 快照与阶段后 FIFO 操作完成；同优先级双 Shield 稳定顺序通过 |
| MOD-004 | 周期、刷新与驱散 | 已完成 | Scheduler Think/Expire、PreservePhase/ResetInterval、边界 tick、StatusResistance、Basic/Strong Dispel 通过；关闭 GAP-005 |
| MOD-005 | 状态响应 | 已完成 | Tag count 驱动移动、攻击、Ability 与碰撞响应；多来源 Stun 计数和 Slow Attribute 测试通过 |
| DEMO-201 | Magic Shield | 已完成 | `UCombatMagicShieldRuntime`；GE 魔抗同步、blocked/HPLoss 不耗盾、耗尽 deferred remove 和多盾顺序通过 |
| DEMO-202 | DOT、Slow 与 Stun | 已完成 | `UCombatPeriodicDamageRuntime` 与数据驱动 Attribute/Tag Modifier；到期边界、驱散、死亡/复活行为通过 |
| OBS-002 | Combat Result Log | 已完成 | Damage/Heal/Death/Respawn/Modifier 结构化日志，含 Schema/Formula、RootEvent、Source/Target、LifeGeneration、数值槽与 Flags；follow-up 链测试通过 |

## 6. M3：可施法切片

| Task | 需求名称 | 状态 | 完成证据/备注 |
| --- | --- | --- | --- |
| TGT-001 | Team 与 Target Filter | 已完成 | `CombatTargetingSubsystem.*`；关系/状态/edge range/Point/visibility/LOS/权威 AoE 自动化通过；关闭 GAP-008 |
| ABL-001 | AbilityData | 已完成 | 目标、时序、commit、special、Action schema 与运行时/Editor validator 完成；非法组合和 future Action 拒绝通过 |
| ABL-002 | Ability 基类 | 已完成 | `CombatGameplayAbility.*`；InstancedPerActor、Activation 快照、多 Unit 同类隔离和统一 cleanup 通过 |
| ABL-003 | Ability 生命周期与事件 | 已完成 | 分阶段原子提交、CDR 快照、固定事件顺序、状态中断和 ActorInfo 清理通过；关闭 GAP-011/GAP-024 |
| ABL-004 | WaitCombatInterval | 已完成 | Scheduler repeating/finish Handle、补帧与 duration 边界、正常/中断清理通过 |
| ABL-005 | DataDriven Actions | 已完成 | Damage/Heal/ApplyModifier/Event/服务器 AoE 公共执行器完成；M5 已启用 Linear/Tracking Projectile 与 Thinker Action |
| ABL-006 | 授予、等级与 AutoCast | 已完成 | DefinitionId 唯一、Spec.Level、remove、RPC、intrinsic reconcile 和 cooldown 清理通过 |
| DEMO-301 | 无目标治疗 | 已完成 | Self Heal 前摇、同 Stage cost/cooldown、CDR 与双 Unit 实例隔离自动化通过 |
| DEMO-302 | 单位目标伤害 | 已完成 | cast point 目标丢失、MagicImmune 与 Magical Damage 公共管线自动化通过 |
| DEMO-303 | 点目标 AoE | 已完成 | PointTarget 服务器 query、客户端命中列表拒绝、阵营过滤与稳定多目标结果通过 |

## 7. M4：Order 与普攻

| Task | 需求名称 | 状态 | 完成证据/备注 |
| --- | --- | --- | --- |
| ORD-001 | OrderComponent | 已完成 | `CombatOrderTypes.h`、`CombatOrderComponent.*`；FIFO、replace、Stop、generation、结果与过期回调自动化通过 |
| ORD-002 | Strategy 移动适配 | 已完成 | AI Move、可选 EQS、Request/Path/Handle/LifeGeneration 防护与结果分类完成；旧回调和成功 PartialPath 用例通过 |
| ORD-003 | 动态目标追击 | 已完成 | Scheduler 0.10 s 复核、50 cm 重发、最长时间与重试上限、移动结束距离重验完成；独立服务器实际 NavMesh 追击通过 |
| ORD-004 | Ability 与 Order 接入 | 已完成 | ASC `OrderReleased` 接口与 Cast Order 接入完成；正常/取消释放均不等待 backswing/cooldown，用例通过 |
| ATK-001 | Attack Registry | 已完成 | `CombatAttackComponent.*`、`CombatAttackTypes.h`；唯一 registry、幂等终结、EndPlay/Death/Respawn 与旧生命 Handle 用例通过 |
| ATK-002 | 攻击前摇与 Ready | 已完成 | `CombatAttackTimingPolicy.*`；Policy v1、绝对 ready、ScheduleOnce、前摇取消与 15° 朝向完成；关闭 GAP-010 |
| ATK-003 | 近战攻击循环 | 已完成 | `AttackTarget` 持续 Order、距离/LOS/状态重验、Damage 公共入口完成；自动化与独立联机连续两次 50 伤害通过 |
| ATK-004 | 法球仲裁 | 已完成 | Modifier 稳定两阶段 `CanClaim/OnAttackClaimed`、exclusive group、资源提交和 OnHit 快照完成；资源只扣一次与快照用例通过 |

## 8. M5：Projectile、Thinker 与 Motion

| Task | 需求名称 | 状态 | 完成证据/备注 |
| --- | --- | --- | --- |
| PRJ-001 | ProjectileData 与 Subsystem | 已完成 | `Combat/Projectile/*`：Handle registry、Spec/数值快照、Actor 复制、exactly-once Finish 与结构化日志完成；自动化与独立场景链路通过 |
| PRJ-002 | Linear Projectile | 已完成 | substep sphere sweep、稳定排序、AlreadyHit、穿透与 world block 完成；`LinearAndTrackingPolicies` 通过 |
| PRJ-003 | Tracking 与 Attack Projectile | 已完成 | Tracking 目标丢失策略、AttackHandle finalize 与旧生命隔离完成；`AttackRecordFinalize` 通过；ADR-034 已关闭 GAP-023 |
| PRJ-004 | Projectile Spawn/Wait Task | 已完成 | Linear/Tracking Spawn Task、Wait 的 OnHit/OnFizzled/OnFinished、fire-and-forget 与 cancel-with-source 完成并通过回归 |
| THK-001 | Thinker | 已完成 | 无 Tick Actor、Scheduler delay/pulse/duration、稳定 AoE 查询与 cleanup 完成；`SchedulerPreemptionAndHookCleanup` 通过 |
| MOT-001 | MotionComponent | 已完成 | H/V 通道、严格高优先级抢占、SafeMove、Nav 投影、Order 恢复与日志完成；抢占及 Hook cleanup 自动化通过 |
| ADP-001 | TwinStick Projectile/AoE 适配 | 已完成 | 模板 Actor 已移除 `ProjectileImpact` 直连和 AoE Actor Timer gameplay，降级为纯表现/Combat Scheduler；全量回归通过 |
| DEMO-501 | Dragon Slave | 已完成 | `UCombatDragonSlaveAbility` 与 DataDriven 穿透 Linear Projectile 完成；`DragonSlaveAndMeatHook` 通过 |
| DEMO-502 | Meat Hook | 已完成 | `UCombatMeatHookAbility`、首命中 Damage、Hook Modifier 与 Horizontal Motion 清理完成；成功/冲突清理均通过 |

## 9. M6：复杂技能集

| Task | 需求名称 | 状态 | 完成证据/备注 |
| --- | --- | --- | --- |
| DEMO-601 | Frost Arrows | 已完成 | `UCombatFrostArrowsRuntime` 完成 AutoCast/等级/Break/Mana 预检、winner 唯一提交和 Projectile/slow 参数 AttackRecord 快照；自动化通过 |
| DEMO-602 | Fissure A：伤害、控制与视觉 | 已完成 | `QueryUnitsAlongSegment` 稳定去重；Damage/Stun/Motion 公共入口和 visual-only Thinker 通过自动化 |
| DEMO-603 | Fissure B：阻挡与 Repath | 已完成 | 无 Tick `ACombatFissureBlocker`、Scheduler 生命周期、路径相交主动 repath 与 navigation attempt generation 旧回调淘汰通过 |
| EXT-601 | Aura 基础 | 已完成 | 每 World registry、Scheduler Coalesce、Targeting child reconcile、Break/换队/死亡/EndPlay 清理通过；ADR-036 关闭 GAP-014 |
| EXT-602 | 高级状态规则 | 已完成 | SpellBlock、Break、Debuff Immunity、Dispel Immunity 独立阶段矩阵通过；ADR-037 关闭 GAP-016 |
| TOOL-601 | 技能模板检查 | 已完成 | `FCombatSkillTemplateValidator`、旁路模式、Definition/schema/事件顺序自动化及 [25](25-M6-Skill-Template-Checklist.md) 完成 |

## 10. M7：联机、UI、工具和性能

| Task | 需求名称 | 状态 | 完成证据/备注 |
| --- | --- | --- | --- |
| NET-001 | ASC 复制矩阵 | 未开始 | — |
| NET-002 | Order RPC Hardening | 未开始 | 到期：GAP-021 |
| NET-003 | Modifier 与 Unit View | 未开始 | — |
| NET-004 | Projectile Reconcile | 未开始 | — |
| OBS-701 | Combat Log 与调试工具 | 未开始 | 到期：GAP-019 |
| MCP-701 | UE MCP 诊断配方 | 未开始 | — |
| DAT-701 | 资产验证与迁移 | 未开始 | — |
| PERF-701 | 容量与性能基线 | 未开始 | 到期：GAP-018 |
| TST-701 | Dedicated Soak | 未开始 | — |

## 11. M8：候选发布

| Task | 需求名称 | 状态 | 完成证据/备注 |
| --- | --- | --- | --- |
| REL-001 | 全量回归矩阵 | 未开始 | — |
| REL-002 | 生命周期审计 | 未开始 | — |
| REL-003 | 基于证据的性能优化 | 未开始 | — |
| REL-004 | 网络预测评估 | 未开始 | 到期：GAP-022（可明确延期） |
| REL-005 | 文档与样例冻结 | 未开始 | — |

## 12. 用户验收记录

| 里程碑 | 提交验收日期 | 用户结论 | 修正要求 | 最终验收日期 | 下一阶段授权 |
| --- | --- | --- | --- | --- | --- |
| M0 | 2026-08-24 | 已验收 | 无 | 2026-08-24 | 已授权 M1（2026-08-24） |
| M1 | 2026-08-25 | 已验收 | 中文注释规范与 M1 源码注释已补齐 | 2026-08-25 | 已授权 M2（2026-08-25） |
| M2 | 2026-08-25 | 已验收 | 无 | 2026-08-26 | 已授权 M3（2026-08-26） |
| M3 | 2026-08-26 | 已验收 | 无 | 2026-08-26 | 已授权 M4（2026-08-26） |
| M4 | 2026-08-26 | 已验收 | 无 | 2026-08-26 | 已授权 M5（2026-08-26） |
| M5 | 2026-08-26 | 已验收 | 无 | 2026-08-26 | 已授权 M6（2026-08-26） |
| M6 | 2026-08-26 | 已验收 | Native GameplayTag 中文说明与蓝图可见中文注释已补齐 | 2026-08-27 | 未授权 |
| M7 | — | 未提交 | — | — | 未授权 |
| M8 | — | 未提交 | — | — | 不适用 |

## 13. 更新日志

| 日期 | 更新内容 | 关联 Task/里程碑 |
| --- | --- | --- |
| 2026-08-24 | 创建进度台账；所有 Task 和里程碑初始化为未开始 | 全部 |
| 2026-08-24 | 根据最终文档评审补充 TST-003、EXT-602；总 Task 调整为 82，并标注 Gap 到期任务 | M1、M6、Gap 关联任务 |
| 2026-08-24 | 完成 M0 冻结包；关闭 GAP-001/002/003/004/006/007/009，G0 通过并提交用户验收 | DEC-001..007 / M0 |
| 2026-08-24 | 用户确认 M0 验收通过；保持 M1 未开始，等待单独授权 | M0 |
| 2026-08-24 | 用户授权开始 M1；FND-001 切换为进行中 | M1 / FND-001 |
| 2026-08-24 | 完成 M1 其余 12 项；Editor 构建、冷启动 Automation 7/7、MCP/PIE 回读通过；TST-003 因 Installed Engine 不支持 Server/Client Target 而阻塞 G1 | M1 / FND-001..008 / TST-001..003 / OBS-001 / MCP-001 |
| 2026-08-25 | 使用 `D:\UE\UE` 源码引擎完成 Development Server/Client Target 构建；持久化测试场景 Actor；独立 Server/Game 进程完成真实连接并确认双 Unit、双 Team、ASC ActorInfo、`State.Alive`；冷启动 Automation 7/7；关闭 GAP-020，G1 通过并提交用户验收 | M1 / TST-003 / GAP-020 / G1 |
| 2026-08-25 | 增加生成代码中文注释规范：项目自有的新建/实质修改代码必须注释类、结构、枚举、函数和关键字段，并纳入 Gate 与 Issue Definition of Done | 全部后续代码任务 |
| 2026-08-25 | 按新规范回填全部 M1 自有源码中文注释；Editor/Server/Client Target 编译通过，`Combat.Foundation` 回归 7/7 通过 | M1 / 中文注释整改 |
| 2026-08-25 | 用户确认 M1 验收通过并要求提交；保持 M2 未开始，等待单独授权 | M1 |
| 2026-08-25 | 用户授权开始 M2；ATR-001 切换为进行中 | M2 / ATR-001 |
| 2026-08-25 | 完成 M2 全部 15 项；关闭 GAP-005/012/013；Editor、源码 Server/Client Target 构建成功，`Combat.` 自动化 12/12、独立监听服务器联机 smoke 通过；G2 通过并提交用户验收 | M2 / ATR-001..OBS-002 / GAP-005/012/013 / G2 |
| 2026-08-26 | 用户确认 M2 验收通过并要求提交；保持 M3 未开始，等待单独授权 | M2 |
| 2026-08-26 | 用户授权开始 M3；冻结 Target visibility、Ability gameplay timing 与 commit snapshot 决策，TGT-001 切换为进行中 | M3 / TGT-001 / ADR-027..029 |
| 2026-08-26 | 完成 M3 全部 10 项；Editor、源码 Server/Client Target 构建成功，`Combat.` 自动化 17/17、独立监听服务器联机 smoke 通过；G3 通过并提交用户验收 | M3 / TGT-001 / ABL-001..006 / DEMO-301..303 / G3 |
| 2026-08-26 | 用户确认 M3 验收通过并要求提交；保持 M4 未开始，等待单独授权 | M3 |
| 2026-08-26 | 用户授权开始 M4；ORD-001 切换为进行中 | M4 / ORD-001 |
| 2026-08-26 | 完成 M4 全部 8 项；关闭 GAP-010；Editor、源码 Server/Client Target 构建成功，`Combat.` 自动化 23/23、独立监听服务器实际追击与连续近战 smoke 通过；G4 通过并提交用户验收 | M4 / ORD-001..004 / ATK-001..004 / GAP-010 / G4 |
| 2026-08-26 | 用户确认 M4 验收通过并要求提交；保持 M5 未开始，等待单独授权 | M4 |
| 2026-08-26 | 用户授权开始 M5；PRJ-001 切换为进行中 | M5 / PRJ-001 |
| 2026-08-26 | 完成 M5 九项源码、中文注释、L_CombatTest 场景 smoke 与 4 组专项自动化；Installed UE 5.8.1 `-NoLink` 编译全部修改成功；源码 UE 5.8.0 Development Server/Client 各 29/29 构建成功；运行中的 Editor 仍占用项目 DLL，正式 Editor 链接、Automation 与联机 smoke 待关闭后执行 | M5 / PRJ-001..DEMO-502 |
| 2026-08-26 | Editor 关闭后完成 Installed UE 5.8.1 正式链接；`Combat.` 冷启动自动化 27/27、独立监听服务器 Projectile spawn/hit/finish 与客户端握手 smoke 通过；G5 通过，M5 转为待验收 | M5 / PRJ-001..DEMO-502 / G5 |
| 2026-08-26 | 用户确认 M5 验收通过并要求提交；保持 M6 未开始，等待单独授权 | M5 |
| 2026-08-26 | 用户授权开始 M6；梳理 G6 六项任务与 GAP-014/GAP-016 到期约束，DEMO-601 切换为进行中 | M6 / DEMO-601 |
| 2026-08-26 | 完成 M6 全部 6 项；关闭 GAP-014/GAP-016；Editor、源码 Server/Client Target 构建成功，`Combat.` 自动化 32/32、独立监听服务器 M6 Aura 场景与客户端连接 smoke 通过；G6 通过并提交用户验收 | M6 / DEMO-601..TOOL-601 / GAP-014/GAP-016 / G6 |
| 2026-08-27 | 按验收反馈将 144 个 Native GameplayTag 定义统一为 `UE_DEFINE_GAMEPLAY_TAG_COMMENT` 并补齐中文说明；生产蓝图节点、事件、Pin 与 M6 配置字段补充中文 `DisplayName`/`ToolTip`；Editor 正式编译及 `Combat.` 自动化 32/32 通过 | M6 / 中文可见说明整改 |
| 2026-08-27 | 用户确认 M6 验收通过并要求提交；保持 M7 未开始，等待单独授权 | M6 |

## 14. 更新规则

- 开始任务时立即将其改为“进行中”，并更新“最后更新”和“当前里程碑”。
- Task 满足路线图验收标准后改为“已完成”，在证据列记录代码/资产路径、测试命令与结果、UE MCP 回读信息。
- 证据列标有“到期：GAP-xxx”的 Task，只有对应 Gap 在 [12](12-Decisions-Gaps.md) 中关闭或按规则明确延期后才能标为“已完成”。
- 每次状态变化同时更新里程碑计数和更新日志，不能只修改 Task 行。
- 里程碑 Gate 通过后，将里程碑改为“待验收”并暂停；不得预先把下一里程碑改为进行中。
- 用户要求修正时记录原话摘要和影响 Task，将状态改为“需修正”或“进行中”。
- 用户明确验收通过后才填写“已验收”和日期；只有用户另行要求继续，才把下一阶段改为进行中。
- 阻塞项必须写清原因、已尝试方法和解除条件，不能只写“阻塞”。
