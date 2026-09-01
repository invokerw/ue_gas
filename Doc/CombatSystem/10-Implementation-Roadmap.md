# 10 实施路线图与关键节点

## 1. 历史计划基线

> 文档性质：本文件冻结 M0-M8 的原始 WBS、依赖和 Gate。M0-M8 当前均已完成验收；实际状态只读取 [00 开发进度台账](00-Progress-Tracker.md)。下段保留的是 2026-08-24 开始实施前的历史基线，不代表当前代码状态。

截至 2026-08-24，Combat 代码尚未开始：GAS 插件/模块依赖未启用，Combat 目录和测试基架不存在。UE MCP 相关插件和本地 `unreal-mcp` endpoint 配置已经存在，但尚未作为 Combat Gate 验证。下列任务默认状态均为 `未开始`。Strategy/TwinStick 只计为参考资产，直到完成适配和 Gate 验收后才记为完成。

本路线图不估算人日。进入迭代排期时，每个 Task ID 可以直接转为 Issue；若单个 Task 无法在一次可评审变更中完成，应继续拆成实现、测试、内容三个子 Issue，但不得拆掉同一个原子语义的 exactly-once 验收。

实际执行状态统一记录在 [00 开发进度台账](00-Progress-Tracker.md)。本文件只定义计划、依赖和验收标准；不得通过修改本文件中的描述暗示任务已经完成。每个 Task 完成、Gate 状态变化、用户提出修正或验收通过时，都必须同步更新进度台账。

## 2. 里程碑总览

| 里程碑 | 可交付结果 | 关键 Gate |
| --- | --- | --- |
| M0 设计冻结 | P0 规则闭合，数据/身份/生命周期不再含糊 | G0 架构准入 |
| M1 GAS 基座 | 工程启用 GAS，ASC/Context/Handle/Scheduler、Server/Client Target 和测试基架可运行 | G1 基础设施 |
| M2 战斗内核 | Attribute + Damage/Heal + Modifier + Shield/DOT 纵向闭环 | G2 数值与 Modifier |
| M3 可施法切片 | 目标校验、Ability 生命周期、无目标治疗/单位伤害/AoE | G3 Ability |
| M4 可控普攻循环 | Order、追击、近/远程普攻、法球基础闭环 | G4 Order/Attack |
| M5 弹体与位移 | Projectile、Thinker、Motion、Hook/Dragon Slave 切片 | G5 Projectile/Motion |
| M6 复杂技能集 | Frost Arrows、Fissure 等公共能力验证 | G6 内容扩展 |
| M7 联机与工具 | Mixed/Minimal、多客户端 UI、日志、资产校验、压测 | G7 网络准入 |
| M8 候选发布 | 预测/性能/兼容按证据加固，文档与测试基线完整 | G8 发布候选 |

主关键路径：

```text
M0
 -> M1 Scheduler/Context/ASC
 -> M2 Attribute -> Damage/Heal -> Modifier Hook
 -> M3 Targeting -> Ability lifecycle
 -> M4 Order -> AttackRecord
 -> M5 Projectile -> Motion/Thinker
 -> M6 reference skills
 -> M7 dedicated-server matrix/performance
 -> M8 release gate
```

并行路径：

```text
M1 Data identity/Tags -------------> M3/M6 content
M1 Test harness -> 每个里程碑测试 --> M8 regression
M1 Network ownership skeleton ------> M7 replication hardening
M2 Combat Event schema ------------> M7 UI/log/tools
```

## 3. Gate 规则

每个 Gate 必须同时满足：

- 代码：Editor/Development 构建通过；涉及网络的节点还需 Dedicated Server/Client target 构建通过。
- 自动化：本节点 P0 用例全绿，无跳过；失败用例有归属，不用手工演示替代。
- 生命周期：正常、取消、Owner EndPlay、World teardown、过期 Handle 都有测试。
- 观测：关键 Handle/EventId/FailureTag 可从日志或调试命令定位。
- UE MCP：涉及 Editor/Content/Blueprint/PIE 的任务先读取真实状态，写入后回读目标并记录编译/测试结果；“MCP 调用成功”不算 Gate 证据。
- 中文注释：新建或实质修改的项目代码已按 [01 §10](01-Scope-Architecture.md#10-生成代码中文注释规范) 注释类、结构、枚举、函数和关键字段；注释与最终行为一致。
- 文档：实际命名/行为回写对应设计文档；开放偏差进入 [12](12-Decisions-Gaps.md)。
- 评审：没有绕过统一 Damage/Modifier/Order/Projectile 入口的蓝图或临时代码。

Gate 未通过时，只能继续当前里程碑内不依赖失败项的并行任务；不得开始、创建或修改下一里程碑的代码与资产。

Gap 表中的“最迟节点/Task”自动成为对应 Task 的验收条件。到期 Gap 仍为“开放”时，Task 不得标记为已完成，Gate 也不得通过；关闭证据必须写入 [00 开发进度台账](00-Progress-Tracker.md)。

### 3.1 用户验收暂停点

每个里程碑 M0-M8 都是强制人工验收边界：

1. 完成本里程碑范围内的代码、资产、测试和文档。
2. 更新 [00 开发进度台账](00-Progress-Tracker.md)，执行对应 Gate，并整理验收材料。
3. 停止继续开发，向用户提交本阶段结果。
4. 用户可以要求修正；修正后重新执行 Gate 并再次提交验收。
5. 只有收到用户明确的“验收通过，继续 Mx”或等价指令后，才允许开始下一里程碑。

不得因为下一阶段任务无依赖、环境仍处于打开状态或可以顺手完成，就提前创建/修改下一阶段的代码和资产。调查性只读检查也应限制在解释当前阶段结果所必需的范围内。

每次验收提交至少包含：

```text
完成的 Task ID:
代码文件和资产路径:
关键行为变化:
编译结果:
Automation/PIE/Dedicated 结果:
UE MCP 读取/修改/回读记录:
已知限制和未关闭 Gap:
建议用户重点检查的场景:
下一阶段名称（仅说明，不执行）:
```

用户验收关注“是否符合预期”，不只关注测试是否通过。用户未确认的设计偏差不能通过自动化 Gate 自动视为接受。

## 4. M0：设计冻结

目标：在写基础类型前关闭会造成大面积返工的 P0 缺口。

实际冻结结论、owner、迁移和测试映射记录在 [14 M0 设计冻结](14-M0-Design-Freeze.md)；执行状态和用户验收仍只记录在 [00](00-Progress-Tracker.md)。

| ID | 任务 | 交付物 | 依赖 | 验收 |
| --- | --- | --- | --- | --- |
| DEC-001 | 队伍与目标关系 | TeamId/关系 API、Friendly/Enemy 规则、目标失败 Tag | 无 | Unit/Ability/Order/Projectile 共用一套规则；关闭 GAP-001 |
| DEC-002 | Unit 生命状态 | Alive/Dying/Dead/Respawning 状态机、死亡/复活清理表 | 无 | exactly-once Death 和跨生命 generation 定义清楚；关闭 GAP-002 |
| DEC-003 | Ability 授予与等级 | AbilitySet、SpecLevel、移除/intrinsic/autocast 规则 | 无 | Class/Data/Spec 身份无环且单一；关闭 GAP-003 |
| DEC-004 | 数值与 RNG | clamp/取整、暴击/闪避随机流、日志复现字段 | 无 | 同一记录可解释每次 roll 和公式版本；关闭 GAP-006、GAP-007 |
| DEC-005 | 碰撞/LOS/地图单位 | Collision Profile、cm 到玩法单位、距离/LOS 容差 | DEC-001 | Target/Order/Projectile 不各算一套；关闭 GAP-009 |
| DEC-006 | Tag 与资产身份 | Native Tag 表、PrimaryAsset 类型、DefinitionId 规则 | 无 | 重复/缺失身份能被校验阻止；关闭 GAP-004 |
| DEC-007 | P0 Gap 评审 | 关闭或接受 [12](12-Decisions-Gaps.md) 中 M1 前项 | DEC-001..006 | 每项有 owner、结论和变更位置 |

### G0 架构准入

- DEC-001 至 DEC-007 全部有书面结论。
- 关键结构字段可以据此冻结，不再以 `TODO decide later` 进入公共 API。
- 文档中的开放建议与已定规则有清晰标记。

## 5. M1：GAS 基座

### 5.1 工程与数据

| ID | 任务 | 交付物 | 依赖 | 验收 |
| --- | --- | --- | --- | --- |
| FND-001 | 启用 GAS 与 Combat 碰撞 | uproject 插件、Build.cs 模块、Combat 目录骨架、M0 Collision Channel/Profile | G0 | Editor 编译/启动无模块缺失；Profile 名称和 response 矩阵可查询 |
| FND-002 | Native Gameplay Tags | State/Ability/Target/Damage/Data/Event/Cue/Failure Tag | DEC-006,FND-001 | 启动注册无重复，自动化可查 |
| FND-003 | PrimaryAsset 基类 | Unit/Ability/Modifier/Projectile/AbilitySet skeleton | DEC-003,DEC-006 | AssetManager 可发现，DefinitionId 校验可运行 |
| FND-004 | 公共 Handle/Result/Numeric/RNG | Event/Modifier/Attack/Order/Projectile/Schedule Handle，Result/FailureTag，Numeric Policy 与 keyed RNG v1 | DEC-002,DEC-004 | Handle 默认无效/比较/ToString/life generation；数值边界和 RNG replay/injection 测试通过 |

### 5.2 GAS 与运行时

| ID | 任务 | 交付物 | 依赖 | 验收 |
| --- | --- | --- | --- | --- |
| FND-005 | 自定义 EffectContext | SourceContext、Duplicate、GetScriptStruct、NetSerialize、TStructOps traits、AbilitySystemGlobals 配置 | FND-001,FND-004 | 实际分配自定义 Struct；Ability/Modifier/Projectile DefinitionId 网络 round-trip 正确 |
| FND-006 | Combat Unit/ASC | Character、ASC、ActorInfo 幂等初始化、Owner/Avatar | DEC-001,DEC-002,FND-001 | Standalone/Listen/Dedicated 初始化矩阵通过 |
| FND-007 | Scheduler | 最小堆、稳定顺序、generation、3 policy、budget、teardown | FND-004,FND-006 | [02](02-Scheduler-Transactions.md) P0 用例全绿；关闭 GAP-025 |
| FND-008 | deferred operation 基件 | 稳定快照、提交队列、阶段/Depth 上下文 | FND-004 | 回调内新增/移除不重入、不破坏遍历 |

### 5.3 测试与日志

| ID | 任务 | 交付物 | 依赖 | 验收 |
| --- | --- | --- | --- | --- |
| TST-001 | Automation 基架 | Combat 测试目录、World fixture、Server/Client helper | FND-001 | 本地/CI 可发现并运行空 fixture |
| TST-002 | PIE 测试地图 | 两队 Unit、NavMesh、生成/销毁/重生入口 | FND-006 | 一键启动可复现场景，不依赖手摆状态 |
| TST-003 | Dedicated 构建目标 | `ue_gasServer.Target.cs`、`ue_gasClient.Target.cs`、构建/启动 smoke 和环境前置说明 | FND-001,TST-001 | UBT 发现 Development Server/Client；Server + Client smoke 通过；关闭 GAP-020 |
| OBS-001 | 事件/调试骨架 | EventId sequence、结构化 log、Handle ToString | FND-004 | 失败测试能打印 RootEventId 和 invalid reason |
| MCP-001 | UE MCP smoke | endpoint/tool discovery、项目/Editor/World 只读检查、受控测试资产回读 | FND-001 | 能区分 Editor/PIE World；工具不可用时安全降级并留记录 |

### G1 基础设施

- Scheduler 的时序、catch-up、预算、teardown 自动化通过。
- M0 Collision Profile response、Numeric Policy v1 和 keyed RNG v1 自动化通过。
- ASC 在 Standalone、Listen owner/non-owner、Dedicated Server/Client 正确初始化。
- Development Server/Client Target 可构建并完成最小连接 smoke；若当前 Engine 安装不支持目标构建，G1 保持阻塞并提交环境决策，不得用 Network PIE 冒充通过。
- 自定义 EffectContext 字段复制与回收安全。
- 可创建两个具有 TeamId/life generation 的空战斗单位，并能从调试输出确认状态。
- UE MCP smoke 通过；测试地图/资产的任何 MCP 写入均已回读、编译和保存，具体流程符合 [13](13-UE-MCP-Workflow.md)。

## 6. M2：战斗内核

### 6.1 Attribute 与生命

| ID | 任务 | 交付物 | 依赖 | 验收 |
| --- | --- | --- | --- | --- |
| ATR-001 | AttributeSet | Health/Mana、攻防/移动/恢复、Incoming Meta | FND-006,DEC-004 | 初始化、GE 聚合、clamp、恢复时序测试通过；关闭 GAP-012 |
| ATR-002 | Unit 初始化 | UnitData -> 初始化 GE、AbilitySet 初始授予接口 | FND-003,ATR-001 | 重复初始化幂等，非法资产拒绝 |
| LIFE-001 | 生命状态组件 | Alive->Dying->Dead->Respawning、life generation | DEC-002,FND-006,ATR-001 | Death/Respawn exactly once，旧 Handle 失效；关闭 GAP-013 |

### 6.2 Damage/Heal

| ID | 任务 | 交付物 | 依赖 | 验收 |
| --- | --- | --- | --- | --- |
| CMB-001 | 事务结果槽 | EventId -> AttributeSet actual delta 回报 | FND-005,FND-008,ATR-001 | Apply GE 后同步得到真实 delta |
| CMB-002 | Damage Calculator | SpellAmp、护甲、魔抗、Pure、flags、数值安全 | DEC-004,ATR-001 | 正负护甲/边界/NaN 自动化 |
| CMB-003 | DamageSubsystem | 权限、阶段、GE apply、Result、Death、follow-up queue | CMB-001,CMB-002,LIFE-001 | 一次 DamageApplied/Death，blocked 路径无副作用 |
| CMB-004 | HealSubsystem | Heal amp、clamp、overheal、Result | CMB-001,LIFE-001 | 满血 Applied=0，不用 Heal 复活 |

### 6.3 Modifier

| ID | 任务 | 交付物 | 依赖 | 验收 |
| --- | --- | --- | --- | --- |
| MOD-001 | ModifierData/Runtime | 生命周期、Hook API、实例状态边界 | FND-003,FND-008 | C++/蓝图 Runtime 可创建销毁 |
| MOD-002 | ModifierComponent | Apply、GE Context 关联、Handle->Runtime、UPROPERTY ownership | MOD-001,ATR-001 | 一 GE 一 Runtime、叠层不重复实例 |
| MOD-003 | 排序与 deferred Hook | Priority/ApplySequence、阶段快照、Damage/Heal Hook | MOD-002,CMB-003,CMB-004 | 自移除/新增、相同优先级稳定 |
| MOD-004 | 周期/刷新/驱散 | Scheduler、Expire 边界、stack、Basic/Strong purge、StatusResistance | MOD-002,FND-007 | tick-on-expire、refresh、purge 和抗性后边界用例通过；关闭 GAP-005 |
| MOD-005 | 状态响应 | Tag count -> 移动/攻击/Ability/碰撞/UI | MOD-002,FND-002 | 多来源 Tag 移除不提前恢复 |

### 6.4 纵向验证

| ID | 任务 | 交付物 | 依赖 | 验收 |
| --- | --- | --- | --- | --- |
| DEMO-201 | Magic Shield | GE 魔抗 + Runtime shield | MOD-003,CMB-003 | blocked/HPLoss 不耗盾，多盾顺序稳定 |
| DEMO-202 | DOT/Slow/Stun | Scheduler Damage、属性 GE、状态 Tag | MOD-004,MOD-005 | 帧率变化、驱散、死亡清理正确 |
| OBS-002 | Combat Result log | Damage/Heal/Modifier 结构化事件 | CMB-003,CMB-004,OBS-001 | RootEventId 可展开 follow-up 链 |

### G2 数值与 Modifier

- [05](05-Damage-Heal.md) 与 [04](04-Modifier-Attributes-Motion.md) P0 测试全绿。
- Magic Shield、DOT、Slow、Stun 没有直接写 Health/Transform 或自建 Timer。
- 多 Modifier 稳定顺序、驱散、死亡、EndPlay 无 Runtime/Schedule 泄漏。

## 7. M3：可施法切片

| ID | 任务 | 交付物 | 依赖 | 验收 |
| --- | --- | --- | --- | --- |
| TGT-001 | Team/Target filter | relation、状态、范围、LOS、visibility policy、点位置和 FailureTag | DEC-001,DEC-005,FND-002 | UI/Order/Ability 调同一规则，服务器重算；关闭 GAP-008 |
| ABL-001 | AbilityData | behavior、target、special、commit policy、actions | FND-003,TGT-001 | 资产校验拒绝非法组合 |
| ABL-002 | Ability 基类 | instancing、ActivationId、目标快照、统一 cleanup | ABL-001,FND-006 | 多 Unit 同类实例不污染 |
| ABL-003 | 生命周期/事件 | cast/channel/interrupt/order release/end、分阶段原子 commit、Montage 表现契约 | ABL-002,FND-007 | 事件顺序/次数、已提交项不重查、同 Stage 不半提交；关闭 GAP-011、GAP-024 |
| ABL-004 | WaitCombatInterval | AbilityTask -> Scheduler，结束清理 | ABL-002,FND-007 | 可变帧率 channel tick 正确 |
| ABL-005 | DataDriven Actions | Damage/Heal/ApplyModifier/AoE/Event 基础 action | ABL-003,CMB-003,CMB-004,MOD-002 | 无蓝图旁路，失败 Result 可追踪 |
| ABL-006 | 授予/等级/autocast | AbilitySet、SpecLevel、intrinsic、toggle RPC | DEC-003,ABL-002 | grant/remove/level 变化幂等且权威 |
| DEMO-301 | 无目标治疗 | DataDriven self heal | ABL-005 | commit、overheal、OrderReleased 语义正确 |
| DEMO-302 | 单位目标伤害 | target validation + Magical damage | ABL-005 | 前摇目标失效、魔免和距离失败正确 |
| DEMO-303 | 点目标 AoE | server query + multi-target result | ABL-005 | 客户端目标列表不被信任，去重稳定 |

### G3 Ability

- 三个基础技能覆盖 NoTarget/UnitTarget/PointTarget。
- Cost/Cooldown 每次激活最多一次，所有中断路径清理 Task/Delegate/Schedule。
- Server 拒绝伪造 TargetData、AbilitySpec 和 Unit ownership。

## 8. M4：Order 与普攻

| ID | 任务 | 交付物 | 依赖 | 验收 |
| --- | --- | --- | --- | --- |
| ORD-001 | OrderComponent | FIFO、generation、状态机、Result/FailureTag | FND-004,TGT-001,LIFE-001 | queue/replace/Stop/旧回调用例通过 |
| ORD-002 | Strategy 移动适配 | EQS instance、MoveRequestId、取消、结果分类 | ORD-001 | 旧 EQS/Move 不推进新 Order |
| ORD-003 | 动态追击 | Cast/Attack 距离复查、位移阈值唤醒、retry 上限 | ORD-002,FND-007 | 目标移动/无路/PartialPath 可恢复或失败 |
| ORD-004 | Ability 接入 | dispatched、OrderReleased、channel interrupt policy | ORD-001,ABL-003 | Cast 不等 backswing/cooldown |
| ATK-001 | Attack registry | Handle/Record/state/finalize/EndPlay/life generation | FND-004,CMB-003,TGT-001 | 所有 outcome exactly once；旧生命 Handle 无法 Finalize |
| ATK-002 | 前摇与 ready | attack point、attack speed、ScheduleOnce、动画投影 | ATK-001,FND-007 | 卡顿不补攻击、Stop 边界正确；关闭 GAP-010 |
| ATK-003 | 近战循环 | AttackTarget 持续 Order、距离/转向、Damage | ORD-003,ATK-002 | 单次 Landed 不 pop，状态阻止正确 |
| ATK-004 | 法球仲裁 | CanClaim/Claim、exclusive group、on-hit snapshot | ATK-001,MOD-003,ABL-006 | 未胜出不扣资源，提交失败可降级 |

### G4 Order/Attack

- 多单位 Move/Cast/Attack/Stop 可通过统一 Order 执行。
- 近战持续攻击、前摇打断、ready 和 Damage 完整闭环。
- 所有 EQS/AI Move/Ability/Attack 异步回调都有 Handle/generation 防护。
- 基础法球两阶段协议通过资源和快照测试。

## 9. M5：Projectile、Thinker 与 Motion

| ID | 任务 | 交付物 | 依赖 | 验收 |
| --- | --- | --- | --- | --- |
| PRJ-001 | ProjectileData/Subsystem | Handle registry、Spec snapshot、Finish idempotency | FND-003,FND-004,TGT-001 | hit/timeout/fizzle/EndPlay 一次结束 |
| PRJ-002 | Linear Projectile | sweep/substep、穿透、AlreadyHit、稳定命中顺序 | PRJ-001 | 高速不漏撞，多命中不重复 |
| PRJ-003 | Tracking/Attack Projectile | target lost policy、AttackHandle/life generation finalize | PRJ-001,ATK-001 | 远程 Record exactly once；旧生命回调失效；关闭 GAP-023 |
| PRJ-004 | Spawn/Wait Task | fire-and-forget、可选等待、cancel-with-source | PRJ-001,ABL-002 | Ability 结束默认不销毁弹体 |
| THK-001 | Thinker | 无 Tick、Scheduler delay/pulse、形状查询、cleanup | PRJ-001,FND-007 | 去重/稳定/EndPlay 清理 |
| MOT-001 | MotionComponent | H/V 通道、优先级、抢占、collision、Nav 校正 | MOD-003,ORD-002 | Interrupted once、结束恢复当前 Order |
| ADP-001 | TwinStick 适配 | Projectile/AoE 改走 Combat 公共入口 | PRJ-002,THK-001 | 无直接 ProjectileImpact/Actor Timer gameplay |
| DEMO-501 | Dragon Slave | 穿透 Linear + 多目标 Damage | PRJ-002,ABL-003 | 结束后弹体仍结算，reconcile 不重影 |
| DEMO-502 | Meat Hook | 首命中 + Damage + drag Motion | PRJ-002,MOT-001 | 全部中断/死亡/EndPlay 清理正确 |

### G5 Projectile/Motion

- Linear/Tracking/Attack Projectile 覆盖命中、穿透、fizzle、超时和过期回调。
- Dragon Slave/Hook 只使用公共 API。
- 强制位移不直接 SetActorLocation，不在 MotionTick 中结算周期 Damage。

## 10. M6：复杂技能集

| ID | 任务 | 交付物 | 依赖 | 验收 |
| --- | --- | --- | --- | --- |
| DEMO-601 | Frost Arrows | intrinsic/autocast/orb/snapshot/远程攻击 | ATK-004,PRJ-003 | 升级/移除不改变已发射 Record |
| DEMO-602 | Fissure A | 线伤、stun、knockback、视觉 Thinker | THK-001,MOT-001 | 目标去重、状态/位移清理正确 |
| DEMO-603 | Fissure B | 物理 blocker、MoveTo repath | DEMO-602,ORD-002 | 阻挡销毁无旧回调污染 |
| EXT-601 | Aura 基础 | owner reconcile、child Modifier、队伍变化 | MOD-004,TGT-001,THK-001 | 进入/离开/死亡/EndPlay 无残留；关闭 GAP-014 |
| EXT-602 | 高级状态规则 | SpellBlock、Break、Debuff/Dispel Immunity 的阶段和交互矩阵 | MOD-004,ABL-003,TGT-001 | 不由单一 MagicImmune Tag 包办，正反用例通过；关闭 GAP-016 |
| TOOL-601 | 技能模板检查 | 示例资产校验、事件序列、蓝图旁路扫描清单 | DEMO-601,DEMO-603 | 新技能可按统一模板创建 |

### G6 内容扩展

- 示例矩阵覆盖引导以外的主要 Ability/Attack/Projectile/Motion/Thinker 语义。
- 每个示例有自动化和 PIE 操作说明。
- 不存在技能专用直接 Health/Transform/Timer 旁路。

## 11. M7：联机、UI、工具和性能

| ID | 任务 | 交付物 | 依赖 | 验收 |
| --- | --- | --- | --- | --- |
| NET-001 | ASC 复制矩阵 | Mixed owner/non-owner、Minimal AI | G6,FND-006 | Dedicated 2-client 矩阵通过 |
| NET-002 | Order RPC hardening | ownership、限频、包上限、request id | ORD-001 | 恶意/重复/无效输入安全拒绝；关闭 GAP-021 |
| NET-003 | Modifier/Unit View | FastArray、DefinitionId、ServerEndTime | MOD-002,LIFE-001 | owner/non-owner UI 一致且不复制 Runtime |
| NET-004 | Projectile reconcile | ProjectileId、预测视觉、服务器结束 | PRJ-001 | 无双视觉、服务器命中唯一 |
| OBS-701 | Combat log/tools | RootEvent 展开、Unit dump、debug draw、metrics、Event schema 版本 | OBS-002,G6 | 可定位失败 Hook/Handle/Order 状态；关闭 GAP-019 |
| MCP-701 | UE MCP 诊断配方 | Unit/ASC/Tag/Attribute、Order/Attack、Projectile/Motion、PIE 日志的可重复查询步骤 | MCP-001,OBS-701 | 新会话可按配方复现定位；不依赖隐含 Editor 状态 |
| DAT-701 | 资产验证/迁移 | cook validator、redirect/version、报告 | FND-003,G6 | 非法内容阻止打包，迁移可追踪 |
| PERF-701 | 容量基线 | 目标场景、Server frame/带宽/对象/回调计数 | G6,OBS-701 | 达到 [08](08-Data-Network-Observability.md) 决定的预算；关闭 GAP-018 |
| TST-701 | Dedicated soak | 多 Unit/Projectile/Modifier/Order 长时运行 | NET-001..004,PERF-701 | 无增长泄漏、重复结算、崩溃 |

### G7 网络准入

- Dedicated Server + 2 Client 自动化/soak 通过。
- Mixed/Minimal 下 UI 不依赖 Runtime UObject 或完整敌方 ActiveGE。
- RPC fuzz、过频和重放请求不能改变越权 Unit。
- 容量场景达到已记录预算；未达标项有 profiling 证据和 owner。
- UE MCP 能高效复现 Editor/Network PIE 诊断场景；最终结论与 Dedicated 测试及结构化日志一致。

## 12. M8：候选发布

| ID | 任务 | 交付物 | 依赖 | 验收 |
| --- | --- | --- | --- | --- |
| REL-001 | 回归矩阵 | 全 Gate 自动化、PIE、Dedicated、teardown | G7 | P0/P1 全绿，无 flaky 用例 |
| REL-002 | 生命周期审计 | Handle/Delegate/Schedule/Runtime/Actor ownership 清单 | G7 | 无未知 owner、无未覆盖退出路径 |
| REL-003 | 性能优化 | 基于 profiler 的 pooling/relevancy/批处理 | PERF-701 | 不改变顺序/exactly-once 语义 |
| REL-004 | 预测评估 | PredictionKey、回滚和 Cue reconcile 方案或明确延期 | NET-004 | 有独立 Gate，不混入权威逻辑修复；关闭或明确延期 GAP-022 |
| REL-005 | 文档与样例冻结 | 命名、API、事件序列、资产模板、迁移说明 | REL-001..004 | 文档与代码/测试一致 |

### G8 发布候选

- 所有 P0/P1 Gap 已关闭、延期或有明确版本边界。
- 无绕过权威入口、无可复现重复结算、无 teardown 泄漏。
- 公式、Tag、DefinitionId、Event schema 有版本/迁移说明。
- 新技能开发可只依赖公开基类、DataAsset 和蓝图事件完成。

## 13. 任务依赖和并行建议

可并行但需协调接口：

- FND-007 Scheduler 与 FND-003 DataAsset 可并行，统一依赖 FND-004 Handle conventions。
- Damage Calculator 与 AttributeSet 可并行，CMB-001 负责约定 Meta/Result 槽。
- ModifierData/Runtime API 可与 Damage Calculator 并行，Hook 接入必须等 CMB-003。
- Targeting 与 AbilityData 可先并行建 schema，Ability lifecycle 必须等 Targeting Result 稳定。
- Order state machine 可在 Ability 完成前搭骨架，但 ORD-004 只能在 ABL-003 后验收。
- Projectile 基类可与 Attack 前摇并行，AttackProjectile 要等 Attack registry。
- 网络复制骨架从 M1 就测试；完整 UI/View 和安全压测在 M7 收口。
- UE MCP 从 M1 完成 smoke 后贯穿每个 Editor/资产任务；不同里程碑的推荐用法见 [13](13-UE-MCP-Workflow.md)。

不能安全并行的关键接缝：

- AttributeSet 实际 delta 回报与 DamageSubsystem：必须共同冻结 EventId/Result 语义。
- Modifier Hook 与 Damage 阶段顺序：不得分别实现两条 Damage 计算。
- AttackRecord 与 AttackProjectile：Record registry 必须先成为唯一 owner。
- Order/Ability 释放：只允许一个 `OrderReleased` 语义，不能各自 Pop。

## 14. Issue 模板

每个任务转 Issue 时至少包含：

```text
Task ID:
目标/不做什么:
依赖:
公共 API/数据变更:
正常路径:
取消/失败/EndPlay 路径:
权限与复制:
自动化用例:
调试输出:
中文注释检查（类/函数/关键字段）:
UE MCP 读取/修改/回读记录（涉及 Editor/Content 时）:
对应文档:
Definition of Done:
```

完成定义必须包括代码、中文注释、测试、文档和清理路径；只有蓝图演示或手工 PIE 成功不算完成。
完成当前里程碑的 Definition of Done 后仍必须进入用户验收暂停；不得自动开始下一里程碑。
