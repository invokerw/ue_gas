# 11 测试计划

## 1. 测试目标

测试优先保证三件事：

1. 同一输入的战斗阶段和 Hook 顺序稳定。
2. 正常、取消、过期回调、EndPlay 和网络重放都只能结算一次。
3. 客户端请求永远不能绕过服务器权威边界。

测试不是在最后集中补齐。每个路线图 Task 的实现变更必须同时增加本层最低用例，Gate 只聚合已有测试。

## 2. 测试分层

| 层级 | 运行环境 | 适合内容 |
| --- | --- | --- |
| Pure/Unit | 无 World 或最小对象 | Calculator、公式、Handle、排序、数据校验、RNG |
| World Automation | 临时 UWorld | Scheduler、ASC、GE、Modifier、Ability、Projectile、Order |
| PIE Functional | 测试地图 | NavMesh、AI Move、碰撞、动画事件、视觉 reconcile |
| Network PIE | Listen/Dedicated + clients | ActorInfo、Mixed/Minimal、RPC、安全和 UI View |
| Soak/Perf | Dedicated 长时场景 | 泄漏、budget、带宽、峰值对象和稳定帧率 |

所有时间敏感测试使用可控 world time/测试驱动器，避免依赖真实 Sleep。

涉及 Editor、Content、蓝图、关卡和 PIE 的测试优先通过 UE MCP 构造/检查环境，并遵守 [13 UE MCP 开发工作流](13-UE-MCP-Workflow.md)。UE MCP 用于提高场景准备和诊断效率，断言仍落在 Automation、Functional Test、结构化 Combat Event 或 Dedicated 测试中。

## 3. 通用断言

每类异步对象都复用以下矩阵：

| 场景 | 期望 |
| --- | --- |
| 正常完成 | 一次完成事件、资源清理 |
| 主动取消 | 一次取消，无后续 callback |
| 重复取消/完成 | 幂等，无第二次副作用 |
| Owner EndPlay | Handle 失效，delegate/schedule/registry 清理 |
| World teardown | 无跨 PIE 残留或下一局回调 |
| 旧 generation | 被忽略，不改变当前状态 |
| 无效/未知 Handle | 安全失败并可诊断 |
| Callback 内结构修改 | 延迟提交，不破坏当前遍历 |

网络对象再增加 owner/non-owner、丢失 asset、重复 RPC 和越权请求矩阵。

### 3.1 UE MCP 辅助验证矩阵

| 阶段 | UE MCP 操作 | 必须留下的验证 |
| --- | --- | --- |
| 测试前 | 读取 Editor/PIE/World 状态、目标资产和默认值 | 精确项目、World、NetMode、资产路径 |
| 构造 | 创建/更新测试 DataAsset、GE、蓝图或地图对象 | 变更目标清单和字段回读 |
| 执行 | 启动 PIE、触发场景、查询 Actor/ASC/Tag/Attribute | Automation/Functional 断言和 EventId |
| 诊断 | 读取 Output Log、组件和运行时状态 | 最小复现输入，不用手调结果代替测试 |
| 收尾 | 停止 PIE、检查 teardown、保存明确资产 | 无残留 Schedule/Runtime/Delegate，源码/资产 diff 可审查 |

任何 MCP 调用都先使用当前会话暴露的工具 schema；endpoint 或工具不可用时，测试应明确跳转到命令行/手工 Editor 备用流程，不能把未执行的验证记为通过。

## 4. Scheduler

P0：

- 可变帧率下 repeating 不漂移，执行次数与绝对时间一致。
- 同时到期按 `NextFireTime -> Priority -> ApplySequence` 稳定。
- callback 内 Cancel/Reschedule，新零延迟任务不在当前轮重入。
- Owner 销毁、generation 失效、World teardown 不回调。
- ExecuteAllBounded、Coalesce、SkipExpired 行为和 TickCount 正确。
- 全局、单 Owner、单任务 catch-up budget 生效；overdue 保留原 ScheduledTime。
- Duration 同时到期的 bTickOnExpire、PreservePhase、ResetInterval 明确。
- 纯 Client 不执行权威 callback。

P1：

- 大量同一时间任务的排序和 budget 性能。
- time dilation、pause、PIE restart 行为。
- Heap 中大量 stale node 的回收策略不会长期增长。

## 5. Attribute、Damage 与 Heal

P0：

- Attribute 初始化、Add/Multiply/Compound 聚合和 clamp。
- 正负护甲、MagicResist、Pure、0/极大数值。
- 魔免、BypassMagicImmune、HPLoss 顺序。
- HPLoss/blocked 不消耗 Shield；多 Shield 按稳定顺序。
- Health clamp 后 AppliedDamage、overkill 和一次 Death。
- Heal amp、HealReceived、AppliedHealing、overheal；Heal 不复活 Dead。
- SpellAmp/NoSpellAmplification、NoLifesteal。
- 反伤新 EventId/相同 RootEventId、不递归、Depth 上限安全终止。
- 吸血只按实际 AppliedDamage。
- Ability、Projectile、DOT Modifier 和反伤的 DirectSourceType/DefinitionId 来源链正确进入 Context 与 CombatLog。
- NaN/Inf/负请求、null/Dead/OutOfGame 目标安全失败。
- AttributeSet 不广播第二份 Damage/Death。

P1：

- 公式版本/边界常量数据回归。
- RNG 注入后暴击/闪避可按记录重放。
- 同一 RootEvent 多 follow-up 的稳定入队顺序。

## 6. Modifier 与状态

P0：

- OnCreated/Destroyed/Refresh/StackChanged 各自次数和顺序。
- 一个 ActiveGE Handle 一个 Runtime，stack 不创建第二实例。
- Duration、自然过期、Purge、死亡移除、Component EndPlay。
- Basic/Strong/NotDispellable 和整 Handle/逐层策略。
- Hook 内自移除、添加、刷新不破坏当前阶段。
- 相同 Priority 由 ApplySequence 决定。
- Think 使用 Scheduler，刷新 phase 和 expire 边界正确。
- 多来源 Stunned/NoUnitCollision 的 Tag count 不提前解除。
- DOT/HOT 只走 Damage/Heal Subsystem。

P1：

- Aura 进入/离开/队伍变化/Owner 死亡的 child reconcile。
- StatusResistance 的 Duration/Think 规则。
- 大量 Runtime 的调度和 deferred queue 性能。

## 7. Ability 与目标

P0：

- NoTarget、UnitTarget、PointTarget 类型和非法组合。
- Friendly/Enemy、Dead/Untargetable/OutOfGame/Invulnerable/MagicImmune。
- 客户端伪造 Actor、位置、命中列表、AbilitySpec 和等级被拒绝。
- cast point 完成；前摇/引导中被状态、死亡、目标丢失中断。
- Channel tick/finish 使用 Scheduler，不随帧率漂移。
- Cost/Cooldown 每 Activation 最多一次，commit point 符合配置。
- CastStarted 已提交 Cost/Cooldown 后，后续 Stage 不重新 Check；同一 Stage 两项提交不会只成功一项。
- CastStarted、SpellStarted、ChannelEnded、OrderReleased、Interrupted、Ended 的顺序和次数。
- 多 Unit 同 Ability Class 的目标快照互不污染。
- EndAbility、移除 Ability、Avatar 变化时 task/delegate/schedule 清理。
- grant/remove/level/intrinsic/autocast 幂等和服务器权威。
- M3 使用 Projectile/Thinker Action 时，资产校验和运行时返回稳定 Unsupported；M5 启用后同一 schema 正常执行。

P1：

- LOS、地图边界、碰撞半径和距离容差。
- Ability Class/DataAsset DefinitionId 一一对应校验。
- DataDriven Action 失败后的事务/Order 策略。

## 8. Order 与移动

P0：

- Move/Cast/Attack FIFO、queue=true、替换、Stop。
- 旧 EQS instance、FAIRequestID、OrderHandle、life generation 不推进新 Order。
- Success/AlreadyAtGoal/Blocked/Aborted/Invalid/PartialPath 分类。
- 距离不足追击，到达后重新校验目标/LOS。
- Root 解除、Stun 恢复、Motion 结束只 Pump 当前队首。
- Cast 非引导/引导的 OrderReleased，不等待 backswing/cooldown。
- AttackTarget 为持续项，不因一次 Landed pop。
- retry、最大追击时间、无路和目标失效的策略。
- Order 队列上限、NaN 位置和重复 Unit 输入。

P1：

- 多单位 EQS 分散、拥堵、RVO/Detour 行为。
- 临时物理 blocker 后 repath；销毁 blocker 后恢复。
- AIController/PathFollowing delegate 多次初始化和切换。

## 9. Attack 与法球

P0：

- 前摇开始创建 Record，前摇中断 Finalize Failed。
- attack point 后 Stop 不回滚已发射 Record。
- attack-ready 卡顿不补发多次攻击。
- 近战/远程、目标死亡、Miss/Evasion 和 EndPlay exactly once。
- 旧/重复/未知 AttackHandle 不结算。
- Death/Respawn 后旧 LifeGeneration 的 AttackHandle/Projectile 回调不能命中新生命 Record。
- CanClaim 无副作用；exclusive group winner；提交失败尝试下一候选。
- on-hit 使用发射快照，升级/移除 Ability 不改变 Record。
- RecordDestroy 总在 Landed/Failed 后一次发生。

P1：

- AttackSpeed/BAT/attack point 边界和动画投影。
- 暴击、闪避、多个非互斥 proc 的稳定 RNG/顺序。
- Sequence/Generation 接近边界时 Handle 比较仍不会跨生命误匹配。

## 10. Projectile、Thinker 与 Motion

P0：

- Linear 首个命中销毁、穿透不重复命中、高速 substep 不漏撞。
- 同 sweep 多目标按路径距离/稳定 identity 排序。
- Tracking 目标丢失的 fizzle/last-known policy。
- Hit/timeout/fizzle/overlap/EndPlay 只 Finish 一次。
- Ability 提前结束后 fire-and-forget 仍结算；cancel-with-source 正确取消。
- 远程 AttackRecord 只结算一次。
- 预测视觉和权威 ProjectileId reconcile 不重复。
- Thinker 无 Actor Timer/Tick，delay/pulse、目标去重和结束清理。
- Motion 同通道优先级抢占、Interrupted、碰撞、NavMesh 校正和 Order 恢复。

P1：

- WorldStatic/友军/Source/NoUnitCollision 等 Collision Profile 矩阵。
- Projectile/Thinker/Motion 大量对象性能和可选池化复用。
- Motion owner/target/caster 在运动中销毁的每条路径。

## 11. 网络与安全

P0：

- Unit Owner/Controller 设置前后 ActorInfo 幂等初始化。
- 自定义 EffectContext 的 TStructOps traits 生效，AbilitySystemGlobals 实际分配自定义 Struct，Source DefinitionId 网络 round-trip 正确。
- Development Server/Client Target 可由 UBT 发现、构建并完成最小连接 smoke。
- Mixed owner/non-owner、Minimal AI 的 Attribute/Tag/ModifierView/UnitView。
- 未拥有 Unit 的 Order RPC 拒绝。
- 未授予 Ability、伪造 TargetData、Amount、Modifier、Attack/Projectile 结果拒绝。
- RPC 限频、包数量/大小、重复 request id 和非法 float。
- ModifierRuntime 不复制，客户端缺 Definition asset 安全降级。
- Dedicated Server 2 Client 下 Damage/Death/Projectile/Order 各 exactly once。

P1：

- 延迟、抖动、丢包下 UI server-time 显示和 Projectile reconcile。
- relevancy/dormancy 切换后 View 恢复。
- reconnect/late join 的 Unit/Modifier/cast 状态。

## 12. 资产与蓝图检查

自动校验：

- DefinitionId 缺失/重复、PrimaryAsset 类型错误。
- AbilityData 与 Ability Class 双向硬引用或不匹配。
- special 空数组、等级越界、NaN/Inf、非法范围/Duration/Interval。
- DamageType 缺失/多重，Tag 废弃/拼写错误。
- Modifier GE/Runtime 配对和 Dispel/Death 配置冲突。
- Projectile collision profile/资源缺失。
- 蓝图公共接口只能使用允许的封装；人工评审检查直接 SetHealth、SetActorLocation、Timer、SpawnActor 旁路。
- 使用 UE MCP 回读父类、变量、GameplayEffect 配置、编译状态和引用，避免只根据资产名判断内容。

## 13. 示例技能验收

每个 [09](09-Example-Skills.md) 示例提供：

- 一个 Automation Spec：纯语义和事件序列。
- 一个 PIE 场景：导航、碰撞、动画和 Cue。
- 一个失败场景：取消、死亡、目标失效或 EndPlay。
- 如涉及网络，一个 Dedicated 2 Client 场景。
- 预期 Combat Event 序列和关键 Result 值。

示例资产只有通过本矩阵后才能复制为新技能模板。

## 14. Gate 映射

| Gate | 必须完成的测试章节 |
| --- | --- |
| G1 | Scheduler、公共 Handle、ASC ActorInfo、自定义 Context traits/Globals/NetSerialize、Server/Client Target smoke、UE MCP smoke |
| G2 | Attribute/Damage/Heal、Modifier、状态、Magic Shield/DOT |
| G3 | Ability/Target、三个基础技能 |
| G4 | Order/Movement、Attack/Orb |
| G5 | Projectile/Thinker/Motion、Dragon Slave/Hook |
| G6 | 示例技能、Aura/阻挡扩展 |
| G7 | Network/Security、资产校验、Soak/Perf |
| G8 | 全量回归、teardown、文档/事件 schema 兼容 |

## 15. Flaky 与失败处理

- 不用扩大 Sleep/容差掩盖时序问题；优先使用可控时间、稳定排序和事件等待。
- 任何 flaky 用例视为失败，记录随机种子、EventId、world time、Handle generation 和日志尾部。
- 修复线上/PIE bug 时先增加最小复现测试，再改语义。
- 性能测试与正确性测试分开阈值；性能波动不能放宽 exactly-once 或顺序断言。
- UE MCP 可用于快速读取现场和重复触发，但修复完成仍需稳定自动化用例，不能只保留一次性的 Editor 操作记录。
- Gate 检查同时读取 [12](12-Decisions-Gaps.md)；到期 Gap 未关闭时，即使功能测试通过也不得把关联 Task 或 Gate 标为完成。
