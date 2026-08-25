# 00 开发进度台账

> 最后更新：2026-08-25
> 当前里程碑：M1 已验收（等待 M2 单独授权）
> 总进度：20/82 Task 完成，2/9 里程碑由用户验收

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
| M1 | GAS 基座 | 13/13 | 通过 | 已验收 | 2026-08-25 / 2026-08-25 | 源码 UE 5.8.0 Server/Client Target 构建通过；独立 Server/Game 进程连接 smoke 通过；M2 未授权 |
| M2 | 战斗内核 | 0/15 | 未开始 | 未开始 | — | — |
| M3 | 可施法切片 | 0/10 | 未开始 | 未开始 | — | — |
| M4 | Order 与普攻 | 0/8 | 未开始 | 未开始 | — | — |
| M5 | Projectile、Thinker 与 Motion | 0/9 | 未开始 | 未开始 | — | — |
| M6 | 复杂技能集 | 0/6 | 未开始 | 未开始 | — | — |
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
| ATR-001 | AttributeSet | 未开始 | 到期：GAP-012 |
| ATR-002 | Unit 初始化 | 未开始 | — |
| LIFE-001 | Unit 生命状态组件 | 未开始 | 到期：GAP-013 |
| CMB-001 | Combat 事务结果槽 | 未开始 | — |
| CMB-002 | Damage Calculator | 未开始 | — |
| CMB-003 | DamageSubsystem | 未开始 | — |
| CMB-004 | HealSubsystem | 未开始 | — |
| MOD-001 | ModifierData 与 Runtime | 未开始 | — |
| MOD-002 | ModifierComponent | 未开始 | — |
| MOD-003 | 稳定排序与 Deferred Hook | 未开始 | — |
| MOD-004 | 周期、刷新与驱散 | 未开始 | 到期：GAP-005 |
| MOD-005 | 状态响应 | 未开始 | — |
| DEMO-201 | Magic Shield | 未开始 | — |
| DEMO-202 | DOT、Slow 与 Stun | 未开始 | — |
| OBS-002 | Combat Result Log | 未开始 | — |

## 6. M3：可施法切片

| Task | 需求名称 | 状态 | 完成证据/备注 |
| --- | --- | --- | --- |
| TGT-001 | Team 与 Target Filter | 未开始 | 到期：GAP-008 |
| ABL-001 | AbilityData | 未开始 | — |
| ABL-002 | Ability 基类 | 未开始 | — |
| ABL-003 | Ability 生命周期与事件 | 未开始 | 到期：GAP-011、GAP-024 |
| ABL-004 | WaitCombatInterval | 未开始 | — |
| ABL-005 | DataDriven Actions | 未开始 | — |
| ABL-006 | 授予、等级与 AutoCast | 未开始 | — |
| DEMO-301 | 无目标治疗 | 未开始 | — |
| DEMO-302 | 单位目标伤害 | 未开始 | — |
| DEMO-303 | 点目标 AoE | 未开始 | — |

## 7. M4：Order 与普攻

| Task | 需求名称 | 状态 | 完成证据/备注 |
| --- | --- | --- | --- |
| ORD-001 | OrderComponent | 未开始 | — |
| ORD-002 | Strategy 移动适配 | 未开始 | — |
| ORD-003 | 动态目标追击 | 未开始 | — |
| ORD-004 | Ability 与 Order 接入 | 未开始 | — |
| ATK-001 | Attack Registry | 未开始 | — |
| ATK-002 | 攻击前摇与 Ready | 未开始 | 到期：GAP-010 |
| ATK-003 | 近战攻击循环 | 未开始 | — |
| ATK-004 | 法球仲裁 | 未开始 | — |

## 8. M5：Projectile、Thinker 与 Motion

| Task | 需求名称 | 状态 | 完成证据/备注 |
| --- | --- | --- | --- |
| PRJ-001 | ProjectileData 与 Subsystem | 未开始 | — |
| PRJ-002 | Linear Projectile | 未开始 | — |
| PRJ-003 | Tracking 与 Attack Projectile | 未开始 | 到期：GAP-023 |
| PRJ-004 | Projectile Spawn/Wait Task | 未开始 | — |
| THK-001 | Thinker | 未开始 | — |
| MOT-001 | MotionComponent | 未开始 | — |
| ADP-001 | TwinStick Projectile/AoE 适配 | 未开始 | — |
| DEMO-501 | Dragon Slave | 未开始 | — |
| DEMO-502 | Meat Hook | 未开始 | — |

## 9. M6：复杂技能集

| Task | 需求名称 | 状态 | 完成证据/备注 |
| --- | --- | --- | --- |
| DEMO-601 | Frost Arrows | 未开始 | — |
| DEMO-602 | Fissure A：伤害、控制与视觉 | 未开始 | — |
| DEMO-603 | Fissure B：阻挡与 Repath | 未开始 | — |
| EXT-601 | Aura 基础 | 未开始 | 到期：GAP-014 |
| EXT-602 | 高级状态规则 | 未开始 | 到期：GAP-016 |
| TOOL-601 | 技能模板检查 | 未开始 | — |

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
| M1 | 2026-08-25 | 已验收 | 中文注释规范与 M1 源码注释已补齐 | 2026-08-25 | 未授权 |
| M2 | — | 未提交 | — | — | 未授权 |
| M3 | — | 未提交 | — | — | 未授权 |
| M4 | — | 未提交 | — | — | 未授权 |
| M5 | — | 未提交 | — | — | 未授权 |
| M6 | — | 未提交 | — | — | 未授权 |
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

## 14. 更新规则

- 开始任务时立即将其改为“进行中”，并更新“最后更新”和“当前里程碑”。
- Task 满足路线图验收标准后改为“已完成”，在证据列记录代码/资产路径、测试命令与结果、UE MCP 回读信息。
- 证据列标有“到期：GAP-xxx”的 Task，只有对应 Gap 在 [12](12-Decisions-Gaps.md) 中关闭或按规则明确延期后才能标为“已完成”。
- 每次状态变化同时更新里程碑计数和更新日志，不能只修改 Task 行。
- 里程碑 Gate 通过后，将里程碑改为“待验收”并暂停；不得预先把下一里程碑改为进行中。
- 用户要求修正时记录原话摘要和影响 Task，将状态改为“需修正”或“进行中”。
- 用户明确验收通过后才填写“已验收”和日期；只有用户另行要求继续，才把下一阶段改为进行中。
- 阻塞项必须写清原因、已尝试方法和解除条件，不能只写“阻塞”。
