# 12 决策与缺口登记

## 1. 用法

本文件区分三种状态：

- `已定`：第一版硬约束，修改需要决策记录和测试迁移。
- `已关闭`：原 Gap 已由已定结论、owner、迁移和测试落点关闭；重新打开必须新增 ADR。
- `建议基线`：为避免实现停摆给出的默认方案，必须在“最迟节点”前接受或替换。
- `开放`：缺少产品/技术结论，不能悄悄由某个子系统自行决定。

关闭 Gap 时必须写：结论、影响文档、代码 owner、测试和兼容/迁移。只有“已有想法”不算关闭。

## 2. 已定架构决策

| ID | 状态 | 决策 | 原因/影响 |
| --- | --- | --- | --- |
| ADR-001 | 已定 | ASC Attribute/ActiveGE 是最终属性唯一来源 | 防止 GE 与 Runtime 双聚合 |
| ADR-002 | 已定 | Damage/Heal Subsystem 是 Health 修改唯一编排入口 | 统一 Hook、实际 Result 和死亡 |
| ADR-003 | 已定 | Modifier 使用 ActiveGE + Runtime 双层 | GAS 表达属性/Tag，Runtime 表达有状态 Hook |
| ADR-004 | 已定 | Hook 按 Priority desc、ApplySequence asc | 同一输入产生稳定结果 |
| ADR-005 | 已定 | Hook 内结构修改 deferred | 防止重入和遍历失效 |
| ADR-006 | 已定 | 周期 gameplay 只走服务器 Scheduler | 防 Timer 漂移和不确定顺序 |
| ADR-007 | 已定 | 连续运动逐帧；不在 Motion/Projectile Tick 结算周期 Damage | 时序职责分离 |
| ADR-008 | 已定 | Attack/Order/Projectile/Schedule 使用稳定 Handle/generation | 过期异步回调失效 |
| ADR-009 | 已定 | Projectile 持快照，不依赖 AbilityTask 生命周期 | fire-and-forget 可安全结算 |
| ADR-010 | 已定 | Order 只等待 OrderReleased/ChannelEnded/失败，不等 cooldown/backswing | 保留 Dota 指令语义 |
| ADR-011 | 已定 | 服务器权威，第一版低预测 | 先稳定完整结算链 |
| ADR-012 | 已定 | 网络/日志引用 PrimaryAssetId，不复制 DataAsset UObject 指针 | 稳定身份和客户端本地解析 |
| ADR-013 | 已定 | ModifierRuntime 只在服务器权威运行 | 客户端通过 GE/Tag/Cue/View 投影 |
| ADR-014 | 已定 | Ability 默认 InstancedPerActor，同实例不重入 | 隔离蓝图/TargetData/提交状态 |
| ADR-015 | 已定 | UWorld game time 驱动 Combat Scheduler | pause/global dilation 一致作用于战斗 |
| ADR-016 | 已定 | Editor/Content/Blueprint/PIE 任务优先使用 UE MCP 读取、受控修改和回读 | 提高资产操作效率与准确性；编译/Automation/Dedicated 仍是最终 Gate |
| ADR-017 | 已定 | Combat 使用独立 FCombatTeamId + TeamSubsystem Relation API；控制权与队伍分离 | 关闭 GAP-001，避免 AI/Ability/Order/Projectile 散落阵营比较 |
| ADR-018 | 已定 | Unit 使用 Alive/Dying/Dead/Respawning；Respawn 递增 uint32 LifeGeneration | 关闭 GAP-002，Death exactly-once 且旧生命回调失效 |
| ADR-019 | 已定 | Ability Class 单向引用 Data，Spec.Level 权威，每 Unit/DefinitionId 一个 Spec | 关闭 GAP-003，消除 Class/Data/Spec 身份环和等级双来源 |
| ADR-020 | 已定 | Numeric Policy/Formula v1 集中边界且不中途取整；Combat RNG v1 使用 keyed roll | 关闭 GAP-006/GAP-007，可解释并重放数值与随机结果 |
| ADR-021 | 已定 | 运行时单位为 cm；固定 Combat Channel/Profile、XY edge range、5 cm tolerance 和统一 LOS | 关闭 GAP-009，Target/Order/Projectile 共享几何语义 |
| ADR-022 | 已定 | 核心语义使用 Native Tag v1；定义资产使用固定 Combat PrimaryAssetType + 显式 lower_snake DefinitionName | 关闭 GAP-004，身份不随路径漂移且可在 cook 阻止冲突 |
| ADR-023 | 已定 | M0 冻结包是 M1 公共字段输入，修改需显式 supersede/schema migration | 防止实现阶段重新引入 `TODO decide later` |
| ADR-024 | 已定 | Health/Mana regen 使用 0.25 s Scheduler Coalesce；Health 走 HealSubsystem，非 Alive 暂停且不补结 | 关闭 GAP-012，恢复量不随帧率漂移 |
| ADR-025 | 已定 | Death/Respawn 保留 AbilitySpec、cooldown、AutoCast 和非死亡移除 Modifier；Mana 复活至 Max；M2 只记录奖励归属事件 | 关闭 GAP-013，冻结最小生命状态机产品语义 |
| ADR-026 | 已定 | 状态抗性只缩短显式可抵抗 Debuff Duration；Think interval 不变；边界 tick 使用缩短后的 ExpireAt | 关闭 GAP-005，冻结周期与 Refresh 边界 |
| ADR-027 | 已定 | M3 VisibilityPolicy 固定为 None；LOS 可显式开启；客户端只提交 Actor/位置且命中列表始终由服务器重算 | 关闭 GAP-008，保留后续 VisionProvider 扩展点 |
| ADR-028 | 已定 | Cast/Channel gameplay 时间只由 Combat Scheduler 驱动；Montage/Notify 仅作可校准表现，不能触发唯一结算 | 关闭 GAP-011，统一中断和清理语义 |
| ADR-029 | 已定 | ManaCost 与 CDR 在各自 commit point 快照；已开始 cooldown 不因后续 CDR 改变而重排 | 关闭 GAP-024，冻结分阶段提交语义 |
| ADR-030 | 已定 | M4 AttackTiming v1 使用 IAS 20..700、BAT 等比缩放、0.20..10.00 s interval，并以 Scheduler 绝对时间决定 attack point/ready | 关闭 GAP-010；Montage/Notify 只投影表现 |
| ADR-031 | 已定 | Order 的 EQS/Move/Ability/Attack 回调必须同时匹配 OrderHandle、具体请求句柄与 Unit life generation | replace/Stop/Respawn 后旧异步结果不能推进新 Order |
| ADR-032 | 已定 | 法球按稳定 Modifier 顺序执行无副作用 CanClaim，再按 exclusive group 提交 winner 并快照 OnHit | 未胜出不扣资源，提交失败可降级且旧 Record 不被升级重解释 |
| ADR-033 | 已定 | Projectile 由 WorldSubsystem registry 持有，权威 Actor 只做连续 substep sweep 与位置复制，全部结束路径汇入幂等 Finish | fire-and-forget 不依赖 Ability 实例，穿透/高速/teardown 共用同一生命周期 |
| ADR-034 | 已定 | Tracking 对旧生命、Dead/Dying/Respawning、Untargetable、OutOfGame 按 Data 选择 Fizzle 或 LastKnown；Invulnerable 留到 impact 判定 | 关闭 GAP-023，避免已有弹体对目标状态各自猜测 |
| ADR-035 | 已定 | MotionComponent 分离 Horizontal/Vertical 独占通道，严格高优先级抢占；Thinker gameplay 时间只走 Scheduler | 保证中断 exactly-once、Order 恢复和 AoE 时序单一权威 |
| ADR-036 | 已定 | Aura 使用每 World registry + Scheduler Coalesce + Targeting 查询，对普通 child Modifier 做 `Target -> Handle/LifeGeneration` 幂等 reconcile | 关闭 GAP-014；统一进入/离开/换队/死亡/Break/EndPlay 清理，不引入 Actor Tick 或蓝图阵营旁路 |
| ADR-037 | 已定 | SpellBlock、Break、Debuff Immunity、Dispel Immunity 使用独立 Tag，并分别落在 Ability commit 后、Runtime Hook、Modifier Apply、Dispel 阶段 | 关闭 GAP-016；防止一个 MagicImmune Tag 混淆资源提交、既有状态与移除语义 |
| ADR-038 | 已定 | 玩家拥有 Unit 使用 Mixed、纯服务器 AI 使用 Minimal；Order 批量 RPC 由 Unit owning connection 承载，并执行 ownership、RequestId、载荷、token bucket 与重放窗口复核 | 关闭 GAP-021；AIController 继续负责导航，PlayerController Owner 只建立网络归属，客户端无 Damage/Modifier/Finish 写入口 |
| ADR-039 | 已定 | Combat Event schema v1 使用无 Runtime UObject 指针的稳定字段、World 环形缓冲与 RootEvent 展开；第一版不承诺录像或确定性 replay | 关闭 GAP-019；不兼容字段变更必须提升 schema 并提供离线迁移，Shipping 不默认保留完整高频 payload |
| ADR-040 | 已定 | M7 容量基线为 30 Hz Dedicated：64 Unit、256 Modifier、128 Projectile、32 Thinker、16 Aura/256 child，Server frame p95 <= 33.34 ms、p99 <= 50 ms，单连接发送 <= 256 KiB/s | 关闭 GAP-018；正确性与性能 Gate 分离，只有 profiler 证据指向具体 owner 后才引入 pooling、relevancy 或紧凑序列化 |

## 3. 本轮查漏补缺摘要

原单体文档对 Damage、Modifier、Scheduler、AttackRecord 和网络权威已有较强约束；本轮新增或显式登记了以下遗漏：

- Unit 死亡/复活、跨生命 generation 和清理顺序。
- Team/阵营关系统一接口、视野/隐身/LOS 对目标合法性的影响。
- Ability 授予、等级、移除、Intrinsic Modifier 和 AutoCast 的权威来源。
- Native GameplayTag 治理、PrimaryAsset 身份/重命名/版本迁移。
- 数值有限值、取整、公式版本和可记录 RNG。
- 攻速、attack point、转向和动画只作表现的契约。
- Projectile collision matrix、同一 sweep 的稳定命中顺序。
- Aura 所有权与 child Modifier reconcile。
- 资源恢复、状态抗性、冷却缩减的动态语义。
- Combat View、结构化日志、调试命令、容量指标和 Dedicated soak。
- RPC 请求 id、限频/包上限和客户端缺失 Definition asset 的降级。
- 模板代码适配风险：EQS/MoveRequest 回调未绑定 generation，TwinStick 直接命中/Timer 不能复用为结算。

已给出不改变核心架构的建议基线；仍需产品选择的内容保留为 Gap。

## 4. P0：进入基础实现前关闭

| Gap | 状态 | 缺口 | 建议基线 | 最迟节点 |
| --- | --- | --- | --- | --- |
| GAP-001 | 已关闭 | TeamId、Neutral、召唤物继承关系和队伍变化 | ADR-017；完整值域/失败 Tag/换队规则见 [14](14-M0-Design-Freeze.md#2-dec-001队伍与目标关系) | 2026-08-24 / DEC-001 |
| GAP-002 | 已关闭 | Alive/Dying/Dead/Respawning、尸体、复活和跨生命回调 | ADR-018；状态机/固定清理表/默认保留项见 [14](14-M0-Design-Freeze.md#3-dec-002unit-生命状态) | 2026-08-24 / DEC-002 |
| GAP-003 | 已关闭 | Ability grant/level/remove/intrinsic/autocast 产品规则 | ADR-019；单一身份链/等级/移除规则见 [14](14-M0-Design-Freeze.md#4-dec-003ability-授予等级和-autocast) | 2026-08-24 / DEC-003 |
| GAP-004 | 已关闭 | DefinitionId 命名、重命名、资产版本和 Tag 废弃 | ADR-022；Tag/PrimaryAsset/redirect 规则见 [14](14-M0-Design-Freeze.md#7-dec-006gameplaytag-与资产身份) | 2026-08-24 / DEC-006 |
| GAP-006 | 已关闭 | 暴击/闪避/随机 proc 的随机源和复现 | ADR-020；keyed roll/记录/注入规则见 [14](14-M0-Design-Freeze.md#52-combat-rng-v1) | 2026-08-24 / DEC-004 |
| GAP-007 | 已关闭 | 百分比 clamp、取整、超大值、NaN/Inf 和公式版本 | ADR-020；Numeric Policy v1 见 [14](14-M0-Design-Freeze.md#51-numeric-policy-v1) | 2026-08-24 / DEC-004 |
| GAP-009 | 已关闭 | Pawn/Projectile/WorldStatic/友军/Source 的碰撞矩阵 | ADR-021；Channel/Profile/LOS/单位规则见 [14](14-M0-Design-Freeze.md#6-dec-005碰撞los-和地图单位) | 2026-08-24 / DEC-005 |
| GAP-020 | 已关闭 | Automation/CI 是否包含 Dedicated Server/Client target | `ue_gasServer`/`ue_gasClient` Development Target 已纳入基线并由源码 UE 5.8.0 构建通过；安装版 UE 5.8.1 以独立 Server/Game 进程完成真实连接与场景状态 smoke；版本边界及复现命令见 [15](15-M1-Environment-Decision.md) | 2026-08-25 / TST-003 |

这些字段会进入公共 Context、Handle、DataAsset 或 Collision Profile，晚改会波及几乎全部子系统。

## 5. P0/P1：进入对应功能前关闭

| Gap | 状态 | 缺口 | 建议基线 | 最迟节点 |
| --- | --- | --- | --- | --- |
| GAP-005 | 已关闭（ADR-026） | StatusResistance 是否改变 Duration、tick interval、总伤害 | 只缩短明确可缩短 Debuff Duration；Think interval 不变；边界 tick 按缩短后的 ExpireAt | M2/MOD-004 |
| GAP-008 | 已关闭（ADR-027） | Vision/Fog/Invisible/TrueSight 与施法、攻击、弹体目标合法性 | M3 固定 `VisibilityPolicy=None`；服务器重算 Actor/位置/AoE，API 保留可见性策略扩展点 | M3/TGT-001 |
| GAP-010 | 已关闭（ADR-030） | BAT/AttackSpeed/attack point、移动起手、转向角 | 公式、clamp、移动起手、15° 朝向与动画边界见 [20](20-M4-Order-Attack-Decision.md#4-attacktiming-policy-v1关闭-gap-010) | 2026-08-26 / ATK-002 |
| GAP-011 | 已关闭（ADR-028） | Montage notify 与 gameplay 时间的关系、被打断时动画清理 | gameplay 只使用 Scheduler；notify 仅作表现校准；所有退出路径统一停止表现并清理 Task/Schedule | M3/ABL-003 |
| GAP-012 | 已关闭（ADR-024） | Health/Mana regen 的周期、暂停和死亡行为 | 0.25 s Scheduler Coalesce；Health 走 HealSubsystem；Mana 走 Instant GE；非 Alive 暂停 | M2/ATR-001 |
| GAP-013 | 已关闭（ADR-025） | Death 后 cooldown、Mana、非 RemoveOnDeath Modifier、奖励归属 | 保留 Spec/cooldown/AutoCast/非死亡移除 Modifier；Mana 复活至 Max；M2 仅记录归属 | M2/LIFE-001 |
| GAP-016 | 已关闭（ADR-037） | SpellBlock、Break、Debuff immunity、Dispel immunity 等高级状态 | 阶段矩阵与交互见 [24 §5](24-M6-Content-Decision.md#5-高级状态矩阵关闭-gap-016-的基线)；四类独立 Tag/Failure/Event，不用 MagicImmune 包办 | 2026-08-26 / EXT-602 |
| GAP-023 | 已关闭（ADR-034） | Untargetable/Invulnerable/OutOfGame 对已有 Tracking Projectile 的影响 | Dead/生命代次变化/Untargetable/OutOfGame 按 Data 选择 Fizzle 或 LastKnown；Invulnerable 继续跟踪并在 impact 走统一目标/伤害判定；见 [22 §4](22-M5-Projectile-Motion-Decision.md#4-tracking-目标丢失关闭-gap-023) | 2026-08-26 / PRJ-003 |
| GAP-024 | 已关闭（ADR-029） | CDR/耗蓝缩减在 cooldown 已开始后的动态变化 | Cost/CDR 在 commit point 快照；已开始 cooldown 不重排；同 Stage Cost/Cooldown 整体预检 | M3/ABL-003 |

## 6. P1：内容、网络与工具扩展

| Gap | 状态 | 缺口 | 建议基线 | 最迟节点 |
| --- | --- | --- | --- | --- |
| GAP-014 | 已关闭（ADR-036） | Aura 没有 owner/target 生命周期 | 每 World registry、Scheduler Coalesce、统一 Targeting 与普通 child Modifier reconcile；完整规则见 [24 §4](24-M6-Content-Decision.md#4-aura关闭-gap-014-的基线) | 2026-08-26 / EXT-601 |
| GAP-015 | 开放 | Summon/illusion 的 Owner、Team、ASC、Order 权限 | 独立 Unit + 独立 ASC；CommandingController 与 gameplay owner 分离 | 引入召唤物前 |
| GAP-017 | 明确延期 | 物品、背包、技能点、天赋、经验和经济 | 不属于第一版；通过 AbilitySet/Modifier 公共 API 留扩展口 | M8 后 |
| GAP-018 | 已关闭（ADR-040） | 目标容量/帧/带宽预算和池化触发阈值 | 预算、采样边界与优化触发规则见 [27 §7](27-M7-Network-Observability-Decision.md#7-容量预算关闭-gap-018-的目标值)；64 Unit/256 Modifier 双客户端 soak 通过，验收证据见 [29](29-M7-Acceptance.md) | 2026-08-27 / PERF-701 |
| GAP-019 | 已关闭（ADR-039） | Combat Event schema 版本、存档/回放边界 | schema v1、环形诊断与明确不支持的 replay 边界见 [27 §6](27-M7-Network-Observability-Decision.md#6-事件调试和回放边界关闭-gap-019-的目标值) | 2026-08-27 / OBS-701 |
| GAP-021 | 已关闭（ADR-038） | RPC token bucket、批量命令上限和重复 request id 窗口 | ownership、20/s + 32 burst、8 Order/4096 bytes、128 RequestId 窗口及失败 Tag 见 [27 §3](27-M7-Network-Observability-Decision.md#3-order-rpc-安全基线关闭-gap-021-的目标值) | 2026-08-27 / NET-002 |
| GAP-022 | 明确延期 | Ability/移动本地预测和回滚 | G7 后单独设计 PredictionKey/rollback/Cue reconcile | M8/REL-004 |
| GAP-025 | 已关闭 | 暂停、global/custom time dilation 语义 | `UCombatSchedulerSubsystem` 使用 World game time；real-time UI 不进入 Scheduler；时序/catch-up/budget/teardown 自动化通过 | 2026-08-24 / FND-007 |

## 7. 模板适配风险

| 风险 | 现状证据 | 处理任务 |
| --- | --- | --- |
| Strategy 移动旧回调污染 | `AStrategyUnit::OnMoveFinished` 无 RequestId/Result 过滤，EQS 回调无当前实例/generation 校验 | ORD-002 |
| Controller delegate 重复 | `NotifyControllerChanged` 添加 delegate，但未显示解绑/幂等保护 | ORD-002/FND-006 |
| Stop 不是请求取消闭环 | 当前 `StopMoving` 只 `StopMovementImmediately`，未绑定 AI Move/EQS 终态 | ORD-002 |
| TwinStick 弹体绕过 Combat | `NotifyHit` 直接调用 `NPC->ProjectileImpact` | ADP-001 |
| TwinStick AoE 自建 Timer | `ATwinStickAoEAttack` 用 TimerManager 控制 gameplay | ADP-001/THK-001 |
| 模板 Actor Tick 默认打开 | StrategyUnit、Projectile、AoE 构造中启用 Tick | 各适配任务按时序类别关闭或限制 |

这些不是要求立刻修改模板；只有在接入 Combat 时才改造，避免在 M1 之前扩大代码范围。

## 8. 风险登记

| 风险 | 可能结果 | 预防/探测 |
| --- | --- | --- |
| GE 与 Runtime 双聚合 | UI/服务器数值不同、重复增益 | ADR-001、Attribute 测试、蓝图 API 限制 |
| Timer/Tick 分散 | DOT 漂移、catch-up 爆发、顺序随机 | 单 Scheduler、budget 指标、代码评审 |
| Hook 重入 | 护盾/驱散容器损坏或不确定 | 强引用快照、deferred queue、嵌套测试 |
| Ability/Projectile 生命周期耦合 | Ability End 后弹体丢失或悬空 | Spec snapshot、Subsystem owner、Fire-and-forget 测试 |
| Order 多点 Pop | 跳过队列、旧回调执行新命令 | 单状态机、Handle/generation、Result 分类 |
| ASC owning connection 错误 | owner 看不到完整 GE 或 RPC 归属异常 | ActorInfo/Owner 矩阵和 Dedicated 测试 |
| NavMesh 动态阻挡依赖异步完成 | Fissure 穿越/单位卡死 | 第一版物理 blocker + 主动 repath |
| 蓝图旁路公共管线 | 伤害/权限/日志不一致 | 封装 API、资产模板、评审/自动检查 |
| 缺少容量目标 | 完成后才发现服务器不可承载 | M7 前定预算、全程暴露计数器 |
| DefinitionId 漂移 | UI/日志/存档无法解析 | 资产校验、redirect/version、cook gate |

## 9. 决策记录模板

```text
ADR/GAP ID:
状态: proposed | accepted | superseded | deferred
上下文:
选择:
备选与未选择原因:
影响的 API/资产/网络:
迁移策略:
新增/修改测试:
影响文档:
决定人/日期:
```

## 10. 关闭标准

一个 Gap 只有同时满足以下条件才可标为关闭：

1. 结论已写入最主要的功能文档，而不只留在本登记表。
2. 路线图 Task/Issue 已引用该结论。
3. 若进入公共 API/数据/网络，已有兼容和迁移说明。
4. 最少一个正向和一个失败/边界测试体现结论。
5. 与已有 ADR 冲突时，明确 supersede 哪条规则。
