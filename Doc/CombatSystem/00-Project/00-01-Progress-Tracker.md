# 00-01 开发进度台账

> 最后更新：2026-09-15
> 物品系统（2026-09-14）：ITEM-001 已通过用户游玩验收并获准本地提交。六装备/三背包、主动/被动、场景放下与走近拾取、HUD/日志及 v2 物品契约已落地；最终 Editor/Server/Client、Combat 82/82、资产 29/29、迁移器 3/3、冷启动 PIE、Dedicated 双客户端争用/控制互换及 64/256 容量通过。F0 GO、F1 APPROVED、F2 PASS、Push-Ready READY；未做 cook/打包、长时间浸泡和人工网络损伤，不推送。见 [Spec](../Specs/ITEM-001-item-system.spec.md)、[操作与配置](../10-Architecture/10-14-Item-System.md) 和 ADR-055。
> HUD 技能点击（2026-09-14）：HUD-ABILITY-CLICK-001 已完成技能槽左键施法接入；点击复用 Q/W/E/R 的无目标、目标瞄准和 AutoCast 链路，右键在未瞄准时固定详情，升级按钮保持优先。安装版 UE 5.8.2 Editor 构建、HUD 5/5、AbilityAim 4/4、全量 Combat 83/83、文档校验和交付 Gate 通过；真实交互 PIE 已由 HUD-ABILITY-CLICK-002 后续回归覆盖，用户已验收并授权本地提交，见 [Spec](../Specs/HUD-ABILITY-CLICK-001-skill-hud-click.spec.md) 与 ADR-056。
> 属性系统（2026-09-15）：ATTR-001 已完成 DOTA2 风格 Strength/Agility/Intelligence、Formula v2 派生属性和 owner-only HUD 全量快照；Editor 增量构建、核心与全量 Automation、资产校验、Dedicated smoke、文档校验和交付 Gate 已通过。Server/Client Target 受安装版引擎限制未执行，用户已验收实现结果并授权本地提交；各层验证的执行时点见 [Spec](../Specs/ATTR-001-attributes.spec.md) 与 ADR-058。
> HUD 技能取消后换槽（2026-09-14）：HUD-ABILITY-CLICK-002 已定位为 GameAndUI 焦点切换触发 `FlushPressedKeys` 清掉同一 MouseDown 新建的技能会话；HUD 请求现排到下一帧，右键取消后另一技能首击进入指示器。安装版 UE 5.8.2 构建、Editor 输入回归、全量 Automation、真实 PIE 首击、文档校验与交付 Gate 均通过；用户已验收并授权本地提交，见 [Spec](../Specs/HUD-ABILITY-CLICK-002-cancel-reclick.spec.md) 与 ADR-057。
> 指示器修正（2026-09-13）：[AIM-002](../Specs/AIM-002-ground-only-indicators.spec.md) 已通过用户实机验收并获准本地提交；地面接收过滤与点目标地面查询已修复，保留 Hero 其他贴花。Editor、Combat.Input. 7/7、资产 17/17、三地图 114 个地面组件独立重载与原生 PIE 41/41（含 Hero 遮挡、坡道/高台 GPU 像素对照）通过；本轮不改变 RPC/复制/服务器结算，未重跑 Dedicated。
> 技能指示器（2026-09-13）：AIM-001 连同 AIM-002 修正已通过用户验收；本地瞄准、范围预览与独立训练场已落地，首版 Editor/Server/Client 及最终增量构建、Combat 70/70、资产 17/17、PIE 22 项（含悬停和真实追近）与 Dedicated 双客户端通过，见 [Spec](../Specs/AIM-001-skill-indicators.spec.md)。
> 当前阶段：M8、SAM 与 DEMO-901 卓尔游侠 Demo 均已通过用户验收
> 历史 M0-M8：82/82 Task 完成，9/9 里程碑由用户验收
> SAM 进度：10/10 Task 完成；修正 Gate 和用户验收均已通过
> 成长专项（2026-09-11）：PROG-001 已完成，待用户验收；全量 `Combat.` 62/62 与 Editor 构建通过，Server/Client Target 受安装版 UE 5.8 限制
> 流程专项（2026-09-12）：DOC-009 已通过用户验收；DOC-008 与 TOOL-001 已完成，待用户验收；PLAN 审查和 BUILD 准入、Spec 当前批准版本、Skill 路由与交付证据已纳入可失败 Gate，UE 路径配置与 Dedicated 准入已纳入本地工具
> 最近工程验证（2026-09-08）：Demo 普攻输入已统一为 Enhanced Input Action；该轮常规 Editor 构建、蓝图编译保存回读、全量 Combat 53/53 和资产 7/7 通过。头顶 UI 蓝图拆分及此前三 Target/Dedicated 回归已完成；该轮未重跑联机矩阵
> HUD 专项（2026-09-09）：已实现并完成工程验证，待用户实机复验；三 Target、Combat 57/57、资产 7/7、双玩家 PIE 与 Dedicated 双客户端通过，见 [10-12](../10-Architecture/10-12-Bottom-HUD-Design.md) 与 ADR-047
> 战斗记录专项（2026-09-13）：HUD-LOG-001 已通过用户验收；左上角入口、可筛选历史和 owner-only 事件投影已接入，三 Target、Combat 67/67、资产 10/10、PIE 与 Dedicated 双客户端通过，见 [Spec](../Specs/HUD-LOG-001-combat-log.spec.md)
> 战斗记录交互（2026-09-13）：HUD-LOG-002 已通过用户验收；支持顶部栏拖动、视口约束和关闭重开保留位置，Editor、直接 Automation 5/5、资产 10/10 与实际 PIE 通过，见 [Spec](../Specs/HUD-LOG-002-window-drag.spec.md)

本文件是项目执行状态的唯一来源。[00-02 实施路线图](00-02-Implementation-Roadmap.md)定义任务内容和依赖，本文件记录实际状态、验证证据和用户验收结论。

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
| M2 | 战斗内核 | 15/15 | 通过 | 已验收 | 2026-08-25 / 2026-08-26 | G2 通过；Editor/Server/Client 构建、Automation 12/12 和独立联机 smoke 通过；M3 已授权；见 [90-04](../90-History/90-04-M2-Acceptance.md) |
| M3 | 可施法切片 | 10/10 | 通过 | 已验收 | 2026-08-26 / 2026-08-26 | Editor/Server/Client 构建、Automation 17/17 和独立联机 smoke 通过；用户验收通过；见 [90-06](../90-History/90-06-M3-Acceptance.md) |
| M4 | Order 与普攻 | 8/8 | 通过 | 已验收 | 2026-08-26 / 2026-08-26 | Editor/Server/Client 构建、Automation 23/23 和独立联机追击/连续近战 smoke 通过；用户验收通过；见 [90-08](../90-History/90-08-M4-Acceptance.md) |
| M5 | Projectile、Thinker 与 Motion | 9/9 | 通过 | 已验收 | 2026-08-26 / 2026-08-26 | Editor/Server/Client 构建、Automation 27/27 和独立联机 Projectile spawn/hit/finish smoke 通过；用户验收通过；见 [90-10](../90-History/90-10-M5-Acceptance.md) |
| M6 | 复杂技能集 | 6/6 | 通过 | 已验收 | 2026-08-26 / 2026-08-27 | Editor/Server/Client 构建、Automation 32/32、独立联机 M6 场景 smoke 通过；中文可见说明整改后用户验收通过；见 [90-12](../90-History/90-12-M6-Acceptance.md) |
| M7 | 联机、UI、工具和性能 | 9/9 | 通过 | 已验收 | 2026-08-27 / 2026-08-27 | Editor/Server/Client 构建、Automation 37/37、资产校验和 64 Unit/256 Modifier 双客户端 soak 通过；用户验收通过；见 [90-14](../90-History/90-14-M7-Acceptance.md) |
| M8 | 候选发布 | 5/5 | 通过 | 已验收 | 2026-08-27 / 2026-08-27 | Editor/Server/Client、最终修订 3×40 Automation、资产与 64/256 双客户端候选场景全绿；用户验收通过，见 [90-17](../90-History/90-17-M8-Acceptance.md) |
| SAM | 服务器权威单位移动改造 | 10/10 | 通过 | 已验收 | 2026-09-02 / 2026-09-09 | 已修复默认 Pawn 生成顺序导致的重复/孤立 AIController；单/双玩家 PIE 真实位移、Dedicated 双客户端、三 Target、`Combat.*` 44/44 与资产 7/7 全绿；用户已确认验收通过；见 [10-10](../10-Architecture/10-10-Server-Authoritative-Movement-Kickoff.md) |

## 3. M0：设计冻结

| Task | 需求名称 | 状态 | 完成证据/备注 |
| --- | --- | --- | --- |
| DEC-001 | 队伍与目标关系 | 已完成 | [90-01 §2](../90-History/90-01-M0-Design-Freeze.md#2-dec-001队伍与目标关系)；ADR-017；关闭 GAP-001 |
| DEC-002 | Unit 生命状态 | 已完成 | [90-01 §3](../90-History/90-01-M0-Design-Freeze.md#3-dec-002unit-生命状态)；ADR-018；关闭 GAP-002 |
| DEC-003 | Ability 授予与等级 | 已完成 | [90-01 §4](../90-History/90-01-M0-Design-Freeze.md#4-dec-003ability-授予等级和-autocast)；ADR-019；关闭 GAP-003 |
| DEC-004 | 数值与 RNG | 已完成 | [90-01 §5](../90-History/90-01-M0-Design-Freeze.md#5-dec-004数值与-rng)；ADR-020；关闭 GAP-006/GAP-007 |
| DEC-005 | 碰撞、LOS 与地图单位 | 已完成 | [90-01 §6](../90-History/90-01-M0-Design-Freeze.md#6-dec-005碰撞los-和地图单位)；ADR-021；关闭 GAP-009 |
| DEC-006 | GameplayTag 与资产身份 | 已完成 | [90-01 §7](../90-History/90-01-M0-Design-Freeze.md#7-dec-006gameplaytag-与资产身份)；ADR-022；关闭 GAP-004 |
| DEC-007 | P0 Gap 评审 | 已完成 | [90-01 §8](../90-History/90-01-M0-Design-Freeze.md#8-dec-007p0-gap-总评审)；GAP-020 接受 M1/TST-003 到期方案 |

## 4. M1：GAS 基座

| Task | 需求名称 | 状态 | 完成证据/备注 |
| --- | --- | --- | --- |
| FND-001 | 启用 GAS 与 Combat 碰撞 | 已完成 | `ue_gas.uproject`、`Source/Combat/Combat.Build.cs`、`DefaultEngine.ini`；Editor Target 构建成功；`TagsAndCollision` 通过 |
| FND-002 | Native Gameplay Tags | 已完成 | `CombatTags.h/.cpp`；自动化查询通过；MCP `ListTags(Combat)` 回读成功 |
| FND-003 | PrimaryAsset 基类 | 已完成 | `CombatDefinitionData.*`、AssetManager scan、redirect/唯一性校验；`CombatUnit:team_one/team_two` 冷启动发现通过 |
| FND-004 | 公共 Handle/Result/Numeric/RNG | 已完成 | `CombatTypes.*`、`CombatNumericPolicy.*`、`CombatRngSubsystem.*`；冻结向量与边界测试通过 |
| FND-005 | 自定义 EffectContext | 已完成 | `CombatGameplayEffectContext.*`、`CombatAbilitySystemGlobals.*`；实际分配、Duplicate、traits、NetSerialize round-trip 通过 |
| FND-006 | Combat Unit 与 ASC | 已完成 | `CombatUnitCharacter.*`、`CombatAbilitySystemComponent.*`、`CombatTeamSubsystem.*`；四 NetMode ActorInfo 用例与 PIE ASC 回读通过 |
| FND-007 | Combat Scheduler | 已完成 | `CombatSchedulerSubsystem.*`；稳定顺序、三 policy、三层 budget、generation、reentry、owner/world teardown 测试通过；关闭 GAP-025 |
| FND-008 | Deferred Operation 基件 | 已完成 | `CombatDeferredOperationQueue.*`；嵌套阶段、稳定快照和回调内新增延迟提交通过 |
| TST-001 | Automation 基架 | 已完成 | `CombatAutomationWorldFixture.*`、`CombatFoundationTests.cpp`；冷启动 `Combat.Foundation` 7/7 通过 |
| TST-002 | PIE 测试地图 | 已完成 | `/Game/Combat/Tests/L_CombatTest`、`CombatTestScenarioActor.*`；NavMesh 回读，PIE 自动生成 Team 1/2，停止后清理通过 |
| TST-003 | Dedicated Server/Client 构建目标 | 已完成 | 源码 UE 5.8.0（`D:\UE\UE`）Development Server/Client Target 构建成功；安装版 UE 5.8.1 独立 Server/Game 进程完成 127.0.0.1 连接，Server 记录 `Join succeeded`，Client 记录 `Welcomed by server`；场景日志确认 2 Unit、Team 1/2、ASC ActorInfo 与 `State.Alive`；见 [90-02](../90-History/90-02-M1-Environment-Decision.md)，关闭 GAP-020 |
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
| TOOL-601 | 技能模板检查 | 已完成 | `FCombatSkillTemplateValidator`、旁路模式、Definition/schema/事件顺序自动化及 [20-02](../20-Content/20-02-M6-Skill-Template-Checklist.md) 完成 |

## 10. M7：联机、UI、工具和性能

| Task | 需求名称 | 状态 | 完成证据/备注 |
| --- | --- | --- | --- |
| NET-001 | ASC 复制矩阵 | 已完成 | `Automatic` 在玩家 Owner Unit 选择 Mixed、AI Unit 选择 Minimal；PlayerController 网络 Owner 与 AIController 导航职责分离；Dedicated 2-client 矩阵为 Mixed=2/Minimal=62 |
| NET-002 | Order RPC Hardening | 已完成 | 批量 RPC 完成 owner、正数 RequestId、8 Order/4096 bytes、20/s + 32 burst、128 重放窗口和稳定失败 Tag；ADR-038 关闭 GAP-021 |
| NET-003 | Modifier 与 Unit View | 已完成 | `UCombatUnitViewComponent` 复制扁平 Unit View 与 FastArray Modifier View；owner/non-owner 共用投影，不复制 `UCombatModifierRuntime` |
| NET-004 | Projectile Reconcile | 已完成 | 权威 Projectile identity 与可选 PredictionKey 复制；客户端预测视觉被同键服务器视觉替换，重复 Handle 幂等去重且不能参与命中结算 |
| OBS-701 | Combat Log 与调试工具 | 已完成 | Event schema v1、RootEvent 展开、Unit dump、debug draw、metrics、滚动 p95/p99 与连接带宽统计完成；ADR-039 关闭 GAP-019 |
| MCP-701 | UE MCP 诊断配方 | 已完成 | [30-02](../30-Tooling/30-02-M7-MCP-Diagnostic-Recipe.md) 固化 World 身份、复制矩阵、安全 RPC、事件、Projectile 与性能查询顺序及命令行降级入口 |
| DAT-701 | 资产验证与迁移 | 已完成 | `CombatAssetValidation` commandlet、redirect/version 校验与 JSON 报告完成；验收扫描 2 个资产、0 Error/0 Warning |
| PERF-701 | 容量与性能基线 | 已完成 | 30 Hz 预算与指标采样实现；64 Unit/256 Modifier soak 的 p95 16.281 ms、p99 22.392 ms、单连接最大发送 39.817 KiB/s；ADR-040 关闭 GAP-018 |
| TST-701 | Dedicated Soak | 已完成 | `L_CombatTest -CombatM7CapacitySmoke` 独立 Server + 2 Client 运行超过 90 秒；双 RPC 成功，无拒绝、崩溃、网络失败或容量泄漏；World Automation 验证 64/256 teardown |

## 11. M8：候选发布

| Task | 需求名称 | 状态 | 完成证据/备注 |
| --- | --- | --- | --- |
| REL-001 | 全量回归矩阵 | 已完成 | Editor/Server/Client 构建通过；最终修订 `Combat.*` 冷启动 3 轮各 40/40；资产 2/2；Dedicated 2-client 64/256、双方 RPC、发布契约、预算与 teardown 全绿，见 [90-17](../90-History/90-17-M8-Acceptance.md) |
| REL-002 | 生命周期审计 | 已完成 | [90-16](../90-History/90-16-M8-Lifecycle-Audit.md) 冻结 Handle/Delegate/Schedule/Runtime/Actor owner 与全部退出路径；新增真实 World 清零/旧句柄负测试 |
| REL-003 | 基于证据的性能优化 | 已完成 | ADR-042：M7 profiler/容量样本无超预算具体 owner，保留稳定顺序与 exactly-once，不做推测性 pooling/relevancy/批处理 |
| REL-004 | 网络预测评估 | 已完成 | ADR-041：v1 仅 Projectile 纯视觉 PredictionKey；完整 rollback/Cue reconcile 明确延期 post-v1，GAP-022 已版本化延期 |
| REL-005 | 文档与样例冻结 | 已完成 | 发布契约、生命周期审计与[公共扩展/迁移指南](../20-Content/20-03-M8-Public-Extension-Guide.md)已落地；自动化保护 DataAsset/蓝图扩展面 |

## 12. Post-M8：服务器权威单位移动改造

> 该工作不改写 M0-M8 历史验收。架构、迁移记录与完整 Gate 见 [10-10 服务器权威单位移动改造与验收](../10-Architecture/10-10-Server-Authoritative-Movement-Kickoff.md)。

| Task | 需求名称 | 状态 | 完成证据/备注 |
| --- | --- | --- | --- |
| SAM-000 | 开工设计冻结 | 已完成 | ADR-043 与 [10-10](../10-Architecture/10-10-Server-Authoritative-Movement-Kickoff.md) 冻结拓扑、迁移、删除清单和 Gate |
| SAM-001 | 拓扑不变量与诊断 | 已完成 | Unit dump/一次性日志覆盖 Controller、Owner、Role、BindingGeneration、PathFollowing、Crowd、Collision 与 LifeGeneration；非法拓扑拒绝普通移动 |
| SAM-002 | 服务器 AI 导航器 | 已完成 | 新增 `ACombatUnitAIController` + `UCrowdFollowingComponent`；Order 只走服务器 `AAIController::MoveTo/StopMovement`，Move/Stop/追击回归全绿 |
| SAM-003 | Command Pawn 与控制绑定 | 已完成 | `ACombatGameMode` 在默认出生时独立生成 Combat Unit 与 Command Pawn，先完成 AI Possess/Owner 绑定，再只把 Command Pawn 返回给 PlayerController；Demo GameMode 已改为继承原生 GameMode，单玩家 PIE 保持 2 Unit/2 AIController，玩家 Unit 右键移动约 420 cm |
| SAM-004 | 输入与网络角色迁移 | 已完成 | 点击与 Q/W/E/R 全部改读 CommandedUnit；Dedicated 两端 RPC 成功，UnitLocalRole=1，Player Pawn 为 `CombatCharacter` |
| SAM-005 | 删除客户端路径分支 | 已完成 | 已删除 PC PathFollowing、`ClientFollowCombatOrderPath`、`ClientStopCombatOrderNavigation`、非 AI Move 分支与 `SetAutonomousProxy`；生产代码无遗留引用 |
| SAM-006 | 服务器碰撞与 Crowd | 已完成 | Capsule 硬阻挡、SimProxy `MaxDepenetrationWithPawnAsProxy=0`、CMC RVO 关闭、单一 Detour Crowd 及 NoCollision/Root/Motion/Dead/Respawn/UnPossess 状态测试通过；`BP_WoodenDummy` 四个装饰网格已关闭碰撞，只保留根 Capsule 参与 gameplay collision |
| SAM-007 | Demo 资产迁移 | 已完成 | 默认地图/GameMode 与 Crowd 配置已更新；`BP_CombatDemoGameMode` 已通过 UE MCP 改为继承 `ACombatGameMode`、编译并保存；全项目 Blueprint 编译 0 Error/0 Warning，资产 7/7、0 Error/0 Warning，Dedicated 回读真实 Demo GameMode/PC/Pawn |
| SAM-008 | Automation 与 Dedicated Gate | 已完成 | 默认出生链路新增 GameMode/蓝图父类、AIController 唯一性与孤立计数断言；Editor/Server/Client Target 构建通过，`Combat.*` 44/44、资产 7/7 通过；单/双玩家 PIE 真实位移通过，同版本 Dedicated 双客户端移动 324.508 cm、静止单位 0 cm |
| SAM-009 | 当前行为文档切换 | 已完成 | 10-01/10-07/10-09 已切换到服务器 AIController + Command Pawn + SimulatedProxy 当前语义；90-16 生命周期与本文证据同步更新 |

2026-09-07 移动手感调优：已完成。按用户要求关闭 Crowd `SlowdownAtGoal`，将普通移动原生默认加速度设为 `6000 cm/s²`。Editor Development 模块后缀构建成功，相关 Automation 10/10、0 失败/0 测试警告；新进程确认原生单位与 Demo 蓝图均继承新加速度。单玩家 PIE 在相同起点/方向的 600 cm 空旷直线路径上，以 Demo 实际 `500 cm/s` 移速对照：达到 90% 移速由 `227.3 ms` 降至 `83.3 ms`，两组均到达并停止。证据与命令见 `Saved/MovementTuning/Validation.md`、`EditorSuffixBuild.log`、`Automation/index.json`、`MovementSmoke.json`。当前打开的 Editor 仍需重启加载新参数；本次未重跑 Server/Client Target 或 Dedicated 联机 Gate，不代表 SAM 用户验收通过。

2026-09-07 转身速率统一专项首轮验证（起手容差调整前）：已完成。CastPoint/CastTarget 与普攻通过 `Facing` 等待移动组件按 `RotationRate.Yaw` 转身，当时施法在 1° 内才开始前摇，无目标技能保持朝向；保留允许尸体目标的配置。Editor Development 模块后缀构建成功，最终 `Combat.*` 48/48（新增 4 项）、0 失败/0 测试警告，资产校验 7/7、0 Error/0 Warning。独立单玩家 Demo PIE 回读新移动组件及 640°/s，背身 180° 后 0.283 s 进入前摇、0.534 s 释放 Order，停止转身和普通导航实测通过。命令、边界与日志见 `Saved/AbilityFacing/Validation.md`、`Automation/index.json`、`AssetReport.json`、`PieSmoke.json`。当前已打开的 Editor 需重启加载新模块；本次未运行 Server/Client Target 与 Dedicated 联机，不替代 SAM 用户验收。

2026-09-07 起手容差统一：已完成。按用户反馈，施法与普攻共用 UnitData 的起手容差，默认均为 15°；保留 `AttackFacingToleranceDegrees` 字段名与资产兼容，中文配置说明同步更新。Editor Development 模块后缀构建成功，`Combat.*` 48/48、0 失败/0 测试警告，覆盖容差外等待、进入 15° 后起手及跨 ±180° 最短转向；UE MCP 回读 Demo 玩家与木桩容差均为 15。证据见 `Saved/AbilityFacingTolerance/Validation.md`、`EditorBuild.log`、`Automation/index.json`。本轮未重跑 PIE、资产 commandlet 或 Server/Client/Dedicated；当前 Editor 需重启加载新模块。

## 12.1 Post-M8：头顶 UI 蓝图拆分

> 状态：已完成（2026-09-07）。设计、维护入口与迁移见 [10-11](../10-Architecture/10-11-Overhead-Blueprint-UI.md)，决策 ADR-045；核心 `combat_v1_rc1` 不变，独立展示接口 / View 投影为 v2。

| Task | 内容 | 状态 | 证据 |
| --- | --- | --- | --- |
| UI-001 | C++ 展示接口、状态时间窗与生命周期 | 已完成 | 3 项 `Combat.UI.Overhead.*`；客户端展示不依赖服务器内部事件序号 |
| UI-002 | 头顶与跳字 Widget Blueprint、Demo 配置迁移 | 已完成 | 2 个 WBP + 2 个角色蓝图编译，7 个修改/新增资产保存和冷回读；Demo 显示名解析正常 |
| UI-003 | Editor/蓝图/资产、Automation 与联机验证 | 已完成 | Editor `9140` / Server / Client 构建；Combat 53/53、资产定义 7/7、双端 PIE 及 Dedicated 两客户端 |
| UI-004 | 当前行为、公开接口和迁移文档同步 | 已完成 | README、索引、08/09/12/32/34/36 与本台账同步 |

证据：`Saved/OverheadBlueprint/Validation.md`；最终 Automation 为 `AutomationFinal2/index.json`（2026-09-07 09:46:07 UTC，53 成功、0 失败/警告/未运行），资产为 `AssetReportFinal.json`（7 个定义、0 Error/Warning）。`PieSmoke.json` 记录两端前摇/引导、投射物跳字、驱散和重生清理通过；`DedicatedServerFinal.log` 与两个 `DedicatedClient*Final.log` 记录最终模块 64 单位 / 256 Modifier、两连接、正式 RPC 与移动回归，Budget=Pass、静止目标水平位移 0。该轮验证已重启 Editor 到最终模块并打开头顶蓝图。

边界：未完整 cook / 打包；源码版 UE 5.8 用于 Server/Client Target 编译，安装版 UE 5.8.1 用于资产、Editor 和独立 `-server/-game` smoke。Dedicated 沿用基线的工具插件初始化和 Mixed ASC 动态 GE 定义告警，详见验证记录；不将其计为本次新增 UI 错误，也不宣称整份日志零错误。下节普攻专项的 52/53 保留为当时结果，其缺失头顶蓝图问题已由本节最终 53/53 关闭；SAM 用户验收仍待复验。

2026-09-08 启动收尾修正：上轮残留的隐藏 Editor 进程占用 `UnrealEditor-ue_gas-9140.dll`，导致 Rider 常规构建在清理热重载文件时失败。UE MCP 确认 All Saved、PIE 未运行后结束该旧进程；UE 5.8.1 常规 Editor 构建通过，UBT 自动清理后缀产物并把模块引用恢复为 `UnrealEditor-ue_gas.dll`。实际启动 Editor、加载 Demo 后 `Combat.UI.Overhead.*` 3/3 通过，测试实例自动退出（ExitCode=0）。本轮未修改源码或资产；启动与构建证据见 `Saved/EditorStartupFix/Validation.md`、`EditorBuild.log`、`EditorStartup.log` 和 `Automation/index.json`。

## 12.2 Post-M8：Demo 普攻输入

2026-09-08 输入配置修正：已完成。按用户反馈移除直接 Key 绑定，改为四个 Boolean Enhanced Input Action、Demo Controller 默认引用及 `IMC_Default` 映射（ADR-046）；默认 A/左键/Escape/S，Controller 只绑定 `Started`，可在映射资产中改键。右键/触摸/QWER 的六条原映射逐项回读保持一致，普攻与停止的服务器行为不变。

- 常规 UE 5.8.1 Editor Win64 Development 构建通过（52.66 秒），模块引用为 `UnrealEditor-ue_gas.dll`；Demo Controller 蓝图通过 UE MCP 编译、显式保存并回读四个 Action 引用。
- 独立冷启动 `Combat.*` 53/53，0 测试警告；输入测试验证真实 Demo 蓝图、Boolean Action、默认映射、Action 事件绑定及原有普攻/取消语义。`CombatAssetValidation` 扫描 7 个 Combat 定义资产，0 Error/0 Warning；Input Action 配置由前述输入测试覆盖。
- 证据：`Saved/AttackInputActions/Validation.md`、`EditorBuild.log`、`MCPReadback.json`、`Automation/index.json`、`CombatAssetReport.json`。本轮未执行真实鼠标/键盘 PIE、Server/Client Target、Dedicated 或 cook/打包；SAM 用户验收状态保持不变。下方保留首轮历史验证记录。

> 状态：已完成（2026-09-07）。兼容新增玩家输入：右键点敌人和 A 后左键确认提交已有 `AttackTarget`，S 提交 `Stop`；共用原 Order RPC、服务器追击与攻击生命周期，不改变结算契约或网络载荷。

- `ACombatPlayerController` 增加实际命中选敌、A/左键/Escape/S 绑定及统一单条批次提交；普攻手势的按住/松开不发送移动，技能/停止/控制绑定刷新清除旧拖动状态。A 模式点地面不自动找敌，也不执行 Attack Move。
- 新增 `Combat.Input.Attack.RightClickAndContinuousOrder`、`TargetSelectionAndCancellation` 两项，通过正式 RPC/Order/Attack 验证持续扣血、超距追击、选敌、取消与 Unit EndPlay；输入、OrderAttack、SAM、Network 合计 18/18 成功，0 测试警告。
- UE 5.8 Editor Development 模块后缀 `9076` 最终构建成功。完整 `Combat.*` 报告为 52 成功、1 失败、0 未运行：唯一失败 `Combat.UI.Overhead.BlueprintBindingLifecycle` 属于工作区既有 UI 改造，缺少 `/Game/Combat/Demo/UI/WBP_CombatOverhead`；没有删改该用例或资产，也不把全量 Gate 标成通过。
- UE MCP 回读 `IMC_Default` 右键/触摸/QWER 映射及玩家 DataAsset：AttackDamage=20、AttackRange=150 cm、BaseAttackTime=1.7 s、BaseAttackPoint=0.25 s、AttackProjectileData=None（当前 Demo 普攻为近战）。本次无二进制资产修改。
- 证据：`Saved/BasicAttackInput/Validation.md`、`EditorBuild.log`、`Automation/index.json`（2026-09-07 08:38:17 UTC）。当前打开的 Editor 需重启加载新模块；未执行真实鼠标 PIE、Server/Client Target 或 Dedicated 联机验证。SAM 用户验收状态保持不变。

## 12.3 Post-M8：底部居中 HUD

> 状态：工程实现与验证已完成，待用户实机复验（2026-09-09）。用户已验收布局设计并授权实现；维护入口见 [10-12](../10-Architecture/10-12-Bottom-HUD-Design.md)，接入决策见 ADR-047。HUD 初版使用展示投影 schema 3；DEMO-901 增加 AutoCast 槽位后当前为 schema 4。

| Task | 内容 | 状态 | 证据 |
| --- | --- | --- | --- |
| HUD-001 | 底部面板布局与占位规则 | 已完成 | 用户确认紧凑头像、属性覆盖、左下等级经验环、无资源标题、六格物品及三格背包；预览上下边距一致 |
| HUD-002 | C++ 展示适配与 Widget Blueprint 接入 | 已完成 | owner-only 属性 / 技能快照、4 技能槽、可见 Buff / Debuff、英雄属性详情、经验环与 6+3 常驻空槽；3 个 Widget Blueprint、HUD Actor 蓝图、示意头像，Demo GameMode 已接入 |
| HUD-003 | 工程、资产、生命周期及相应网络验证 | 已完成 | 常规 Editor 与源码 Server/Client 构建成功；Combat 57/57（新增 HUD 4 项）、0 测试警告；资产定义 7/7；真实双玩家 PIE 布局和详情交互；Dedicated 双客户端 owner-only 投影及移动回归通过 |

证据：`Saved/BottomHUD/Validation.md`、`EditorBuild.log`、`ServerBuildFinal.log`、`ClientBuild.log`、`AutomationFinal/index.json`（2026.09.09-10.16.49 UTC）、`AssetReport.json`、`PIE-Client.png`、`DedicatedSummary.txt`。HUD 网络专项服务端检查 2 个拥有者 / 2 个技能，两客户端各检查 1 个拥有者 / 1 个技能，其他可见单位快照均为空。64 Unit / 256 Modifier 容量样本 Budget=Pass，移动单位位移 316.617 cm，静止单位 0 cm。

范围（HUD-001–003 验收时）：等级 / 经验 / 物品 / 背包为视觉占位；等级与经验已由后续 PROG-001 接入服务器成长快照，物品与背包由后续 ITEM-001 接入真实服务器库存。Demo 的英雄技能仍只授予一个可切换 AutoCast 的被动技能，Q 显示霜冻之箭，W/E/R 为空；物品主动独立使用 1–6。头像为可替换的原创示意美术，技能 / Buff 无配置纹理时使用名称首字。当时未执行 cook / 打包；源码 UE 5.8.0 验证 Server/Client 编译，安装版 UE 5.8.1 验证资产和同版本独立 `-server/-game` 联机，沿用既有工具插件初始化和动态 GE 定义日志边界。后台测试进程已退出，不影响原有 SAM 用户验收状态。

## 12.4 Post-M8：卓尔游侠 Demo 流程

> 状态：用户已验收（2026-09-11）。任务规格见 [DEMO-901](../Specs/DEMO-901-drow-ranger-flow.spec.md)；风险 L1，F0=`GO`、F1=`APPROVED`、F2=`PASS`。

| Task | 内容 | 状态 | 证据 |
| --- | --- | --- | --- |
| DEMO-901 | `Characters` → `Heros`、`Player` → `DrowRanger`，配置默认远程普攻并将授予技能替换为四级霜冻之箭 | 已验收 | 旧包与 redirector 清零；Editor/Server/Client 构建、Combat 59/59、资产定义 10/10、PIE 远程法球 smoke、Dedicated 双客户端 HUD/移动及 64/256 容量边界通过；用户于 2026-09-11 确认验收完成 |

证据：`Saved/DrowRangerDemo/EditorBuild-Final3.log`、`ServerBuild-Final3.log`、`ClientBuild-Final3.log`、`Automation-Full-Final2.log`、`CombatAssetReport-Final2.json`、`PieSmoke.json`、`Dedicated-Final2.log`。PIE 实测等级 1 的 Mana 120→111、目标 500→468（20 基础 + 12 额外物理伤害）、Tracking Projectile、单层 1.5 秒减速及到期移除；Dedicated 的服务器与两个客户端 owner-only HUD 快照、移动和 64 Unit / 256 Modifier 预算均为 Pass。UE MCP 当前会话不可用，资产迁移、蓝图编译保存与冷回读由同版本 Unreal Python 命令行完成；未执行 cook / 打包。用户于 2026-09-11 明确确认 `DEMO-901` 验收完成，本次状态回写未新增工程验证。

## 12.5 Post-M8：等级经验与技能升级

| Task | 需求名称 | 状态 | 完成证据/备注 |
| --- | --- | --- | --- |
| PROG-001 | Dota 风格经验等级、技能点、技能升级与 HUD 加点按钮 | 已验收 | 全量 `Combat.` 63/63、相关 HUD/Progression 8/8、UE 5.8 Editor Development 构建和资产 10/10 通过；新增 `combat.Debug.AddExperience <Amount> [ActorUniqueId|Name]` 并由自动化直接执行注册命令，Standalone PIE 已验证等级/经验/技能点显示及 `+` 点击后技能点 1→0、Q 技能 1/4→2/4；用户已完成 review；Server/Client Target 受安装版引擎限制，Dedicated 与容量回归未执行，详见 [PROG-001 Spec](../Specs/PROG-001-progression-and-skill-upgrade.spec.md) 与 ADR-049 |

## 12.6 Post-M8：开发流程 Gate 强化

| Task | 需求名称 | 状态 | 完成证据/备注 |
| --- | --- | --- | --- |
| DOC-008 | 强制 Spec、Skill 路由、开工记录与交付验证证据 | 待验收 | 见 [DOC-008 Spec](../Specs/DOC-008-task-gate-enforcement.spec.md) |
| DOC-009 | PLAN 计划审查通过前禁止修改代码、测试和资产 | 已验收 | 2026-09-12 用户确认验收完成并授权提交；三个 Skill、流程和模板统一 F1/BUILD 边界；21 项 Gate 单测、13 个独立审查场景、Skill 格式、文档与 delivery 检查通过；见 [DOC-009 Spec](../Specs/DOC-009-plan-review-lock.spec.md) |

## 12.7 Post-M8：UE 环境配置与 Dedicated 准入

| Task | 需求名称 | 状态 | 完成证据/备注 |
| --- | --- | --- | --- |
| TOOL-001 | 不同机器的 UE 编辑器路径配置与 Dedicated 测试准入 | 待验收 | `.env.example`、`Tools/ue_environment.py`、`Tools/RunDedicated.ps1` 和工具单测；Dedicated 入口在缺少 `UE_SOURCE_EDITOR` 或源码 `Build.bat` 时明确拒绝，不启动 UE；见 [TOOL-001 Spec](../Specs/TOOL-001-ue-environment-config.spec.md) |

## 12.8 Post-M8：战斗记录窗口

| Task | 状态 | 内容与证据 |
| --- | --- | --- |
| HUD-LOG-001 | 已验收 | 完成左上角战斗记录入口、中文彩色历史、来源/目标/类别/时间筛选、512 条 owner-only FastArray 与 Blueprint 布局；最终 Editor/Server/Client 三 Target、Combat 67/67、资产 10/10、PIE 和 Dedicated 双客户端通过。2026-09-13 用户验收并授权提交。见 [Spec](../Specs/HUD-LOG-001-combat-log.spec.md)、ADR-052 与 `Saved/CombatLog/`。 |
| HUD-LOG-002 | 已验收 | 完成顶部栏左键拖动、DPI/视口约束、关闭重开保留位置和捕获清理；修复移动后祖先点击区域失配，连续拖动及原控件交互通过。Editor、Combat.UI.Log 5/5、资产 10/10、PIE 与交付 Gate 通过；2026-09-13 用户验收并授权提交。见 [Spec](../Specs/HUD-LOG-002-window-drag.spec.md) 和 `Saved/CombatLogDrag/`。 |

用户验收状态：`用户已验收`（2026-09-13）；F0 GO、F1 APPROVED、F2 PASS、Push-Ready READY，自评 4.7/5。用户明确确认“验收完成，提交吧”，授权本地 Git 提交。

HUD-LOG-002 追加交互状态：`用户已验收`（2026-09-13）；F0 GO、F1 APPROVED、F2 PASS、Push-Ready READY，自评 4.8/5。拖动增量未重跑 Server/Client、Dedicated 或 cook，原因是仅修改本地 UI；验收归档仅更新状态和检查提交内容。

HUD-ABILITY-CLICK-001/002 追加验收状态：`用户已验收`（2026-09-14）；F0 GO、F1 APPROVED、F2 PASS、Push-Ready READY。用户确认技能 HUD 点击、右键取消和换槽首击行为，授权本地 Git 提交。

## 12.9 Post-M8：三围与派生属性

> 状态：用户已验收并授权本地提交（2026-09-15）。[ATTR-001 Spec](../Specs/ATTR-001-attributes.spec.md) v0.3 的实现验收完成；F0=`GO`、F1=`APPROVED`、F2=`PASS`、Push-Ready=`READY`，各层验证的执行时点、Server/Client Target 与真实交互 PIE 的未执行原因已在 Spec 记录。

| Task | 内容 | 状态 | 证据 |
| --- | --- | --- | --- |
| ATTR-001 | DOTA2 风格 Strength/Agility/Intelligence、主属性、GAS 派生属性、全量 owner-only HUD 快照 | 已验收 | `CombatAttributeSet.*`、`CombatUnitCharacter.*`、`CombatHUDView.*` 与 `CombatHUDWidget.cpp` 已落地；三围公式、动态 GE、旧资产默认值、HUD 快照、全量 `Combat.` 85/85、资产 29/29 与 Dedicated Schema=8 Pass。2026-09-15 用户确认“验收完成，提交吧”，授权本地 Git 提交。 |

## 13. 用户验收记录

| 里程碑 | 提交验收日期 | 用户结论 | 修正要求 | 最终验收日期 | 下一阶段授权 |
| --- | --- | --- | --- | --- | --- |
| M0 | 2026-08-24 | 已验收 | 无 | 2026-08-24 | 已授权 M1（2026-08-24） |
| M1 | 2026-08-25 | 已验收 | 中文注释规范与 M1 源码注释已补齐 | 2026-08-25 | 已授权 M2（2026-08-25） |
| M2 | 2026-08-25 | 已验收 | 无 | 2026-08-26 | 已授权 M3（2026-08-26） |
| M3 | 2026-08-26 | 已验收 | 无 | 2026-08-26 | 已授权 M4（2026-08-26） |
| M4 | 2026-08-26 | 已验收 | 无 | 2026-08-26 | 已授权 M5（2026-08-26） |
| M5 | 2026-08-26 | 已验收 | 无 | 2026-08-26 | 已授权 M6（2026-08-26） |
| M6 | 2026-08-26 | 已验收 | Native GameplayTag 中文说明与蓝图可见中文注释已补齐 | 2026-08-27 | 已授权 M7（2026-08-27） |
| M7 | 2026-08-27 | 已验收 | 无 | 2026-08-27 | 已授权 M8（2026-08-27） |
| M8 | 2026-08-27 | 已验收 | 无 | 2026-08-27 | 不适用（最终里程碑） |
| SAM | 2026-09-02 | 已验收 | 用户反馈“启动 PIE，点击右键并不能移动”；默认出生拓扑已修复，并已补 AIController 唯一性与真实位移验证 | 2026-09-09 | 用户确认服务器权威单位移动验收通过 |
| DEMO-901 | 2026-09-10 | 已验收 | 无 | 2026-09-11 | 不适用（独立 post-M8 任务） |
| DOC-009 | 2026-09-12 | 已验收 | 无 | 2026-09-12 | 用户确认“验收完成，提交吧”，授权本地提交 |
| HUD-LOG-001 | 2026-09-12 | 已验收 | 追加顶部栏拖动，由 HUD-LOG-002 实现 | 2026-09-13 | 用户授权本地提交 |
| HUD-LOG-002 | 2026-09-12 | 已验收 | 无 | 2026-09-13 | 用户授权本地提交 |
| ATTR-001 | 2026-09-15 | 已验收 | 删除冗余初始化重算；只读盘点确认现有 5 个 UnitData 无需数据迁移 | 2026-09-15 | 用户确认“验收完成，提交吧”，授权本地提交 |

## 14. 更新日志

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
| 2026-08-27 | 用户授权开始 M7；读取 G7、网络复制、安全、View、诊断、资产与容量约束，NET-001 切换为进行中 | M7 / NET-001 |
| 2026-08-27 | 完成 M7 全部 9 项；ADR-038/039/040 关闭 GAP-021/019/018；Editor、源码 Server/Client 构建成功，`Combat.*` 自动化 37/37、资产校验 2/2、64 Unit/256 Modifier Dedicated 双客户端 soak 通过；G7 转为待用户验收 | M7 / NET-001..TST-701 / G7 |
| 2026-08-27 | 用户确认 M7 验收通过并要求提交；保持 M8 未开始，等待单独授权 | M7 |
| 2026-08-27 | 用户授权开始 M8；按 G8 进入全量回归、生命周期审计、证据驱动性能决策、预测边界与文档冻结，REL-001 切换为进行中 | M8 / REL-001 |
| 2026-08-27 | 冻结 `combat_v1_rc1` 发布契约；完成 REL-002/003/004/005，GAP-015/017/022 明确延期到 post-v1；开始执行候选发布全量 Gate | M8 / REL-002..005 |
| 2026-08-27 | 完成 M8 全部 5 项；Editor/源码 Server/Client 构建、最终修订 3 轮 `Combat.*` 40/40、资产校验 2/2、64 Unit/256 Modifier Dedicated 双客户端与发布契约日志通过；G8 转为待用户验收 | M8 / REL-001..005 / G8 |
| 2026-08-27 | 用户确认 M8 验收通过并要求提交；`combat_v1_rc1` 完成全部 9 个里程碑验收 | M8 / G8 |
| 2026-08-28 | M8 后新增远程攻击可玩 Demo，并将地图、角色、技能、输入和框架资产整理到 `/Game/Combat/Demo` 分层目录 | post-M8 Demo |
| 2026-09-01 | 增加纯 C++ 头顶资源、控制状态、施法/引导进度与伤害治疗跳字表现；核心 gameplay 仍只读取权威 View/Result | post-M8 UI |
| 2026-09-01 | 整理根 README、文档索引和当前工程基线；新增仓库级 Agent 开发规则，不改变 M0-M8 历史验收结论 | 文档维护 |
| 2026-09-01 | 移除 `Variant_Strategy`、`Variant_TwinStick` 的源码、资产、关卡和外部 Actor/Object 数据；清理模块搜索路径与模板引用。Win64 Development Game Target 构建通过，`Combat.*` 41/41、6 个 Combat Demo 蓝图编译、资产校验 7/7（0 Error/0 Warning）通过；Editor Target 正式链接因运行中 Live Coding 锁未执行 | post-M8 模板清理 |
| 2026-09-02 | 冻结服务器权威单位移动的 post-M8 开工设计：PlayerController 只指挥、AIController 在服务器 Possess/移动、所有客户端 Unit 为 SimulatedProxy；登记 ADR-043、GAP-026 和 SAM-000..009，生产实现保持未开始 | SAM-000 / ADR-043 / GAP-026 |
| 2026-09-02 | 用户授权完成 SAM 服务器权威单位移动改造；SAM-001 进入实现，开始建立拓扑不变量、诊断与测试保护 | SAM / SAM-001 |
| 2026-09-02 | 完成 SAM-001..009：服务器 Combat AIController/Detour Crowd、无碰撞 Command Pawn、owner-only 控制绑定与生命周期、输入/Order 收敛和客户端路径分支删除；三 Target、`Combat.*` 44/44、资产 7/7、蓝图 0 错误、三档 Dedicated 双客户端及 64/256 容量全绿；关闭 GAP-026，SAM 转为待用户验收 | SAM / SAM-001..009 / GAP-026 |
| 2026-09-02 | 用户验收反馈 PIE 右键无法移动；运行时确认 RPC 与寻路已接受但速度/位移为零，同一玩家 Unit 出现重复 AIController，SAM-003 与 SAM-008 转为进行中并补默认出生链路回归 | SAM / SAM-003 / SAM-008 |
| 2026-09-02 | 完成 PIE 右键不移动修正：GameMode 原子创建 Unit/Command Pawn 并建立 AI/Owner 绑定，Demo GameMode 改继承原生实现；单/双玩家 PIE、同版本 Dedicated 双客户端真实位移通过，三 Target、`Combat.*` 44/44、资产 7/7 全绿；SAM 重新转为待用户验收 | SAM / SAM-003 / SAM-007 / SAM-008 |
| 2026-09-02 | 修复 `BP_WoodenDummy` 装饰腿阻挡移动：蓝图模板及 `L_CombatDemo` 已放置实例的 `WoodenPost`、`WoodenCrossbarX/Y`、`WoodenTopCap` 统一为 `NoCollision`，只保留 `CombatUnit` Capsule；增加资产回归断言，并完成 Blueprint 编译保存、冷重载、PIE 运行时碰撞回读、资产与 Automation 复验 | SAM / SAM-006 / SAM-008 |
| 2026-09-03 | 移除 `/Game/TopDown` 模板蓝图、示例关卡及其 World Partition 外部数据；仍被 Combat 使用的输入、点击光标和环境材质迁入 `/Game/Combat` 并修复引用。`BP_CombatDemoPlayerController` 编译保存、双玩家 Demo PIE smoke、相关 Automation 3/3、资产校验 7/7（0 Error/0 Warning）均通过 | post-M8 模板清理 |
| 2026-09-04 | 完成 Combat 全目录注释审查：覆盖 25 个目录、126 个 C++ 文件，更新其中 75 个文件，改写或删除 723 处原注释，同步更新 112 处 ToolTip 及相关标签说明。补充周期时间线、参数覆盖、权限、失败与清理边界，纠正光环补建、技能回调顺序、治疗增幅、弹体快照等描述；未改变代码逻辑、公开签名、标签名称、数值或版本。逐文件去除说明文本后的代码 token 比较与 `git diff --check` 通过；UE 5.8 Win64 Development Editor 最终构建通过，`Combat.*` 44/44（含 `PublicExtensionSurface`）通过，0 失败/0 测试警告。证据：`Saved/CommentAudit/final-review.json`、`EditorBuild-Final.log`、`Automation/index.json`。本次未执行 PIE、独立联机或 Server/Client Target 验证，不替代 SAM 用户验收 | post-M8 注释维护 |
| 2026-09-07 | 关闭 Crowd `SlowdownAtGoal`，将普通移动默认 `MaxAcceleration` 提高到 `6000 cm/s²`。Editor 模块后缀构建、相关 Automation 10/10、原生/Demo 蓝图参数回读及单玩家 PIE 起步/停止对照通过；相同空旷路线达到 90% 移速由 227.3 ms 降至 83.3 ms。同步当前行为文档，SAM 用户验收状态保持待验收 | post-M8 移动手感调优 |
| 2026-09-07 | 按 ADR-044 实现施法/普攻按移动组件转速准备朝向；增加停止、状态、动态目标、生命周期和目标策略兼容测试。Editor 构建、最终 Combat 48/48、资产 7/7 与真实单玩家 Demo PIE 转身/前摇/停止/移动通过；命令和证据见 `Saved/AbilityFacing/Validation.md` | post-M8 转身速率统一 |
| 2026-09-07 | 按用户反馈将施法与普攻起手容差统一为 15°，共用已有 UnitData 配置；Editor 构建、Combat 48/48、Demo 玩家/木桩配置回读和文档检查通过 | post-M8 起手容差统一 / ADR-044 |
| 2026-09-07 | 补齐 Demo 右键普攻、A 后左键确认、S 停止与旧拖动清理；Editor 构建、相关 18/18 通过。完整 Combat 52/53，唯一失败为既有 UI 改造缺少头顶 Widget Blueprint；原样记录并保留该测试 | post-M8 普攻输入 |
| 2026-09-08 | 按用户反馈将 A/左键/Escape/S 改为四个 Input Action，接入 Demo Controller 与 IMC_Default；常规 Editor 构建、蓝图编译保存回读、冷启动 Combat 53/53 和资产 7/7 通过 | post-M8 普攻输入 / ADR-046 |
| 2026-09-09 | 结合 AI-Native 交付方法整理当前项目开发流程：新增 DDD/SDD/TDD 分层、九阶段流水线、F0/F1/F2 流程门、Push-Ready 六层和 Spec 模板；以 [DOC-001](../Specs/DOC-001-document-system-migration.spec.md) 记录本次迁移；不改变 `combat_v1_rc1`、M0–M8 或 SAM 验收状态 | 文档与流程维护 |
| 2026-09-09 | 新增 `Tools/validate_docs.py` 文档体系校验脚本，覆盖必需入口、目录迁移、旧路径、Markdown 本地链接、尾随空格和 Spec 格式；首次运行通过 | 文档与流程自动化 |
| 2026-09-09 | 按 DOC-002 增加本地校验脚本的 13 个正反例测试和 JSON 报告；本地 unittest 与文档校验通过，项目不依赖远端 CI | 文档与流程自动化 |
| 2026-09-09 | 用户确认 SAM 服务器权威单位移动验收通过；将 SAM 状态、用户验收日期和当前阶段同步更新 | SAM / 用户验收 |
| 2026-09-09 | 完成 DOC-003：38 份文档改为目录号与目录内序号，索引、正文引用和本地编号校验同步更新；不改变运行时与验收结论 | 文档编号整理 |
| 2026-09-09 | 新增 `Skills/combat-skill-development`，封装 Ability、DataAsset、Modifier、Projectile/Thinker、技能测试和专项验收；已接入项目入口并完成文档校验 | AI-Native 技能开发 |
| 2026-09-09 | 完成 DOC-005：为通用 Combat 功能任务补充可直接调用的 Skill，并补齐输入、前置条件、工具、输出、验证和升级边界；与专项技能 Skill 分层复用 | AI-Native 功能开发 |
| 2026-09-09 | 新增 DOC-006 任务路由与复盘 Skill：按动作目标选择主 Skill，交付后按六维证据自评，并按重复失败分类调优流程、模板、校验器或专项 Skill | AI-Native 路由与复盘 |
| 2026-09-09 | 用户确认底部居中 HUD 设计；记录头像、等级经验环、技能 / 资源和六格物品 + 三格背包的定稿比例与对齐规则，工程实现未开始 | post-M8 HUD / HUD-001 |
| 2026-09-09 | 用户授权后完成底部 HUD 接入；新增 owner-only 展示快照与 3 个 Widget Blueprint、HUD Actor / 头像配置；三 Target、Combat 57/57、资产 7/7、双玩家 PIE 和 Dedicated 双客户端通过 | post-M8 HUD / HUD-002..003 |
| 2026-09-10 | 合并远端 `d844f0e` 的 HUD 代码、资产和文档；HUD 专题纳入 `10-12`，保留本地文档体系、项目 Skill、配置与 SAM 已验收状态。运行时文件与远端一致，7 个 LFS 实体 hash/size 通过；本机文档校验和工具测试 16/16 通过，未重跑 UE Gate；详见 [DOC-007](../Specs/DOC-007-remote-hud-doc-merge.spec.md) | 远端集成 / 文档合并 |
| 2026-09-10 | 启动 DEMO-901：整理 Demo 英雄目录和 DrowRanger 命名，配置默认远程普攻与四级霜冻之箭；完成现状冷回读、F0/F1 与测试/迁移方案 | post-M8 Demo / DEMO-901 |
| 2026-09-10 | 完成 DEMO-901：迁移 DrowRanger/WoodenDummy 资产，接入四级霜冻之箭、625 cm Tracking 普攻、技能免疫边界及 Q 槽 AutoCast 原子切换；F2 修正 Dedicated 客户端断言与容量夹具后，三 Target、Combat 59/59、资产 10/10、PIE 与 Dedicated 双客户端全绿，转为待用户验收 | post-M8 Demo / DEMO-901 |
| 2026-09-11 | 用户确认 DEMO-901 验收完成；同步任务状态、最终验收日期与 Spec，不新增工程验证结论 | post-M8 Demo / DEMO-901 |
| 2026-09-11 | 完成 PROG-001：新增 Dota 风格累计经验、击杀经验奖励、等级技能点、服务器技能升级 RPC、HUD 等级/经验/技能点投影、技能槽上方 `+` 按钮和 `combat.Debug.AddExperience` 开发命令；Editor 构建、全量 `Combat.` 63/63 自动化通过；Server/Client Target 受安装版 UE 5.8 限制，转为待用户验收 | post-M8 成长 / PROG-001 / ADR-049 |
| 2026-09-11 | 用户完成 PROG-001 review，确认成长、HUD 加点按钮与开发命令交付完成，任务状态更新为已验收 | post-M8 成长 / PROG-001 |
| 2026-09-12 | 完成 REF-001：将 4 组模板 C++ h/cpp 文件及其反射类统一为 Combat 前缀，补充旧类名 CoreRedirect，并同步当前文档与默认配置段；Editor 构建、`Combat.*` 63/63、资产 10/10（0 error/0 warning）和文档校验通过 | post-M8 工程命名迁移 / REF-001 |
| 2026-09-12 | 完成 REF-002：Runtime Module 迁移为 `Combat`，保留 `ue_gas.uproject` 与 `ue_gasEditor/Server/Client` Target 名称；补充 `/Script/ue_gas` PackageRedirect 与旧 Asset Registry 类路径兼容。Editor 构建、`Combat.*` 63/63、资产 10/10（0 error/0 warning）和文档校验通过；Server/Client 受安装版 UE 限制未构建 | post-M8 工程命名迁移 / REF-002 |
| 2026-09-12 | 完成 TOOL-001：新增 `.env.example`、UE 环境解析/校验工具和配置驱动的 Dedicated Server + 两客户端入口；工具单测 32/32、文档校验 62 Markdown/325 本地链接、PowerShell 解析和空白检查通过；本机未配置 `.env`，Dedicated smoke 按预期拒绝启动并记为未执行 | post-M8 工具流程 / TOOL-001 |
| 2026-09-12 | 完成 DOC-009：三个项目 Skill、流程、模板和根入口统一 PLAN 审查先于代码/测试/资产修改；新增 plan/build Gate，BUILD 与交付校验当前 F1 批准、审查证据及版本，拒绝候选列表/历史批准。21 项单测、13 个独立场景、Skill 格式、文档、差异和 delivery 检查通过；不涉及 Runtime/资产，待用户验收 | post-M8 流程约束 / DOC-009 |
| 2026-09-12 | 用户确认 DOC-009 验收完成并授权本地提交；同步 Spec、当前状态和验收记录，不改变流程语义或其他任务验收状态 | DOC-009 / 用户验收 |
| 2026-09-12 | 完成 HUD-LOG-001：新增左上角战斗记录窗口、中文彩色服务器事件投影、来源/目标/类别/时间筛选和有界历史；三 Target、Combat 67/67、资产 10/10、PIE、Dedicated 双客户端与交付 Gate 通过，F2 发现固化回归，转为待用户验收 | post-M8 HUD / HUD-LOG-001 / ADR-052 |
| 2026-09-12 | 启动 HUD-LOG-002：用户追加顶部栏拖动窗口；完成 F0/F1 和 preflight，保留上一轮日志系统，验证拖动、DPI/边界、输入释放和原控件行为 | post-M8 HUD / HUD-LOG-002 |
| 2026-09-12 | 完成 HUD-LOG-002：标题栏左键拖动、入口固定、边界约束与本次 HUD 位置保留；修复移动后点击区域失配，Editor、直接 Automation 5/5、资产 10/10、真实 PIE 与交付 Gate 通过，转为待用户验收 | post-M8 HUD / HUD-LOG-002 |
| 2026-09-13 | 用户确认战斗记录系统及顶部栏拖动验收完成并授权本地提交；同步两个 Spec、任务状态和最终验收日期，复核文档、交付 Gate、差异及 LFS 资产，不新增运行时验证结论 | post-M8 HUD / HUD-LOG-001 / HUD-LOG-002 |
| 2026-09-15 | 完成 ATTR-001：新增三围与主属性、Formula v2 八项派生属性、GAS 动态重算、owner-only HUD 全量快照与回归测试；复核后删除零三围旧资产的冗余初始化重算，保留统一初始资源填充。Editor 增量构建、核心 1/1、全量 `Combat.` 85/85、资产 29/29、Dedicated Schema=8、文档校验和交付 Gate 通过；Server/Client Target 受安装版引擎限制未执行，转为待用户验收 | post-M8 属性系统 / ATTR-001 / ADR-058 |
| 2026-09-15 | 用户确认 ATTR-001 验收完成并授权本地提交；同步 Spec、任务状态和验收日期，明确最终专项复测与此前全量验证的证据边界，复核文档、差异与交付 Gate，不新增 UE 运行时验证结论 | post-M8 属性系统 / ATTR-001 / 用户验收 |

## 15. 更新规则

- 开始任务时立即将其改为“进行中”，并更新“最后更新”和“当前里程碑”。
- Task 满足路线图验收标准后改为“已完成”，在证据列记录代码/资产路径、测试命令与结果、UE MCP 回读信息。
- 证据列标有“到期：GAP-xxx”的 Task，只有对应 Gap 在 [00-04](00-04-Decisions-Gaps.md) 中关闭或按规则明确延期后才能标为“已完成”。
- 每次状态变化同时更新里程碑计数和更新日志，不能只修改 Task 行。
- 里程碑 Gate 通过后，将里程碑改为“待验收”并暂停；不得预先把下一里程碑改为进行中。
- 用户要求修正时记录原话摘要和影响 Task，将状态改为“需修正”或“进行中”。
- 用户明确验收通过后才填写“已验收”和日期；只有用户另行要求继续，才把下一阶段改为进行中。
- 阻塞项必须写清原因、已尝试方法和解除条件，不能只写“阻塞”。
