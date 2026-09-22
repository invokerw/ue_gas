# AI-004 StateTree 阶段 C 战术与容量

> Spec 版本：`0.2`
> 状态：`COMPLETED`
> Owner：Codex
> 创建日期：2026-09-21
> 关联进度台账：`00-01-Progress-Tracker.md`
> 风险等级：`L1`

## 0. Intake 与 Gate 记录

- 用户请求：用户要求“开始 AI-003 StateTree 阶段 C”，即在已验收的阶段 A/B 基础上继续实现 `10-17` 定义的战术与容量阶段。AI-003 是已验收的阶段 B 归档，故本轮新建 AI-004，不改写历史 Spec。
- 验收与提交授权：2026-09-22 用户明确“AI-004 阶段 C 验收完毕，提交吧”，确认阶段 C 验收并授权创建本地 Git 提交；不推送远端。
- 附件解释：最初开工消息未附需求资产；本次续作附件是上一会话的历史上下文，只用于恢复 AI-004 的执行状态和证据，不新增范围。`agent.md`、已接受的 `10-17` 设计和阶段 A/B 现有实现仍是工程约束。
- 已读取入口：`agent.md`、`README.md`、`00-01`、`00-03`、`00-04`、`00-05`、`10-01`、`10-02`、`10-03`、`10-04`、`10-05`、`10-07`、`10-09`、`10-10`、`10-17`、`20-02`、`20-03`、`20-04`、`30-01`、`90-16`、Spec 模板、`Skills/combat-task-router/SKILL.md`、`Skills/combat-feature-development/SKILL.md`，以及当前 AI/Order/ASC/Targeting/Builder/测试源码和 UE 5.8.2 StateTree Consideration/EQS 头文件。
- 主 Skill：`Skills/combat-feature-development/SKILL.md`
- 备选 Skill 与排除理由：`combat-skill-development` 面向新增玩家技能及其结算；本轮只消费既有 Ability 定义与公共预检，不新增技能结算。`skill-creator` 不适用，因为不修改 Codex Skill。
- 路由置信度：`high`
- F0 结论：`GO`
- F0 依据：阶段 C 已由接受的 DDD 明确分期，属于本地兼容新增；不改变发布、网络、Tag、Event 或技能结算契约，风险为 L1。当前分支 `main` 相对远端 ahead 2 且工作区无未提交修改，阶段 A/B 提交必须保留。
- F1 结论：`APPROVED`
- F1 审查人：Codex，依据 `00-05` 的 L1 本地审查权限。
- F1 审查版本：`0.2`
- F1 计划审查证据：v0.1 的 F2 使旧批准失效；2026-09-21 重新运行 v0.2 `preflight` 与 `plan` Gate 均为 0 error（`Saved/AI-004/preflight-v0.2.json`、`plan-v0.2.json`）。逐项复核确认：HardSelect 复用阶段 B 的职责事实与 Role Task，不新增行为状态机；`TacticalLocation` 仅收紧未实现的公开配置而不改变枚举数值；v1 绕过新增预算以保持既有首次及周期感知；新增负向测试直接验证候选评估无激活、扣费、冷却或 Order。范围、权限、兼容、回滚与测试矩阵闭合，批准 0.2。
- Build 解锁：`已解锁`；2026-09-21 `python -X utf8 -B Tools/task_gate.py --mode build --spec Doc/CombatSystem/Specs/AI-004-statetree-tactics-capacity.spec.md --kind feature --report Saved/AI-004/build-v0.2.json` 为 0 error。
- F1 重审条件：范围、架构、权限、Profile 兼容策略、查询身份/预算、测试矩阵或回滚方案发生实质变化时先递增 Spec 版本，F1 回到 `REVISE`、状态回到 `PLAN_REVIEW`。
- F2 结论：`PASS`
- Push-Ready 结论：`READY`
- 验证：v0.2 `preflight`、`plan`、`build` Gate；Editor/Server/Client 三 Target；21/21 `Combat.AI.Tactics`、42/42 `Combat.AI`、143/143 `Combat.`；42 项资产校验、1/1 冷回读与 5 个蓝图 `BS_UP_TO_DATE`；Tactics PIE、Dedicated 双客户端、Windows cook 及 64/128/256 容量均通过。最终报告见 §7/§9。
- 未执行：人工视觉/手感检查、网络损伤、长时间 AI soak 和打包可执行文件启动；这些不属于 AI-004 AC，也不阻塞本地交付，仍保留为后续验证边界。

## 1. 目标与范围

### 目标

在阶段 B 的服务器 StateTree 角色框架上增加可解释的 Utility 战术选择、只读技能候选、可取消且可判旧的 EQS 站位查询，以及按 World 限流和错峰的 AI 查询预算；用英雄 Bot 和远程守卫两个示例闭合普攻切施法、站位、失败恢复与 64/128/256 AI 容量证据。

### 范围

1. 记录并验证安装版 UE 5.8.2 / CL 56702186 与源码版 UE 5.8.2 / CL 0（Compatible CL 55116800）的 `FStateTreeConsiderationBase`、`TrySelectChildrenWithHighestUtility`、三个 Target 与 cook 准入。
2. 增加无引擎执行上下文依赖的有界 Utility 纯函数和薄 StateTree Consideration；合法性仍由 Enter Condition/公共预检决定，同分按资产子状态顺序。
3. 为 Profile v2 增加技能用途规则、行为保持/切换阈值、战术站位和预算参数。v1 继续按阶段 A/B 语义加载，只有显式 v2 战术 Profile 启用新增路径。
4. 从本单位已授予的 AbilitySpec 枚举候选，以 DefinitionId 解析规则，构造 Cast Order 并调用 `PreflightAIOrder`；不以真实激活试探、不复制技能伤害/范围/冷却/费用真值、不把被动/法球/AutoCast 当主动施法。阶段 C 的 `TacticalLocation` 目标策略保留枚举序列化值但隐藏并拒绝配置，EQS 只用于 Reposition。
5. 增加 `QueryTacticalLocation` 原生 Task：单单位最多一个在途 EQS，保存运行/控制/生命/Scope/Preparation/职责/目标生命/查询代次，成功结果形成精确点 Move 意图；取消、失败、超时和迟到结果均有界且不可污染新决策。
6. 增加仅由 v2 战术 Profile 使用的 World 查询配额、稳定错峰和统计快照，覆盖感知扫描、EQS 发起与预算延期；v1 保留阶段 B 的同步首次感知与既有周期语义。受击威胁改为对已知来源做公共单目标复核，避免第二次全 World 枚举。
7. 新增 Hero Bot 与 Ranged Guard 的保存资产和独立演示地图；Hero 通过 Utility 从持续普攻在真实边界切换到主动施法，Ranged Guard 通过 EQS 选择战术站位后仍由 Move Order 执行。
8. 增加 Pure/World/StateTree/资产/PIE/Dedicated/容量测试和诊断，更新 `10-17` 当前落地状态、`20-04` 配置指南、`00-01`、`00-04` 与 README 入口（若适用）。

### Non-Goals

- 不实现完整战争迷雾、隐身/真视、Boss 阶段、团队战略、前瞻伤害模拟或动态 Linked Override。
- 不新增客户端决策、RPC、复制字段、GameplayTag/Event schema 或第二套行为状态机；客户端只观察既有战斗复制。
- 不修改 Ability 的真实效果、消耗、冷却、Targeting 与 Attack 时钟；AI 偏好不能绕过 ASC/Order 权威复核。
- 不预先承诺 Targeting 空间索引；先以 World 预算和 64/128/256 数据验证，只有证据显示当前枚举不满足预算时才另行扩展公共 Targeting。
- 实施交付时不自动提交或推送 Git；用户验收后已授权本地 Git 提交，推送不在本次范围内。

## 2. 当前事实与依据

- `AI/CombatAIBrainComponent.*`：服务器工作区现统一持有运行/控制/生命、Scope/PreparedIntent/Receipt、单活动 Order、攻击边界及战术候选/EQS 身份；停止、接管、死亡、换 Profile 和 EndPlay 共用精确清理。
- `AI/CombatAIRoleBrain.cpp`、`CombatAIWorldSubsystem.*`：v2 感知与 EQS 在执行前申请 World 配额并稳定错峰；受击威胁只对已知来源走公共单目标复核，不再触发第二次全 World 枚举。v1 绕过新增配额，保持阶段 B 时序。
- `AI/CombatAIProfileData.*`：运行时接受本地版本 1/2；只有显式 v2 且启用战术才开放 Utility/Ability/EQS，旧资产不批量迁移。
- `AI/CombatAIStateTreeSchema.cpp`：允许项目原生 Consideration，仍拒绝蓝图 Task、Global Task、Evaluator 和未批准节点。
- `Validation/CombatAIAssetBuilder.cpp`：使用真实 `FStateTreeCompiler` 构建/保存 HardSelect + 最高效用战术子树，并回读节点顺序、Consideration 与单写入约束。
- `Order/CombatOrderComponent.cpp`：`PreflightAIOrder` 已复用 ASC、Targeting、库存和权限规则；`RequestExecutionBoundary`/`ReleaseExecutionBoundary` 已覆盖前摇、Keep 和 Ready 后超时；普通 Cast 等待公共 `OrderReleased`。
- `Targeting/CombatTargetingSubsystem.cpp`：`QueryUnitsInRadius` 统一校验阵营、生命、范围、LOS 与可见性；当前仍使用 `TActorIterator`，但本轮 64/128/256 容量未触发空间索引升级条件。
- `10-17` §6.3/7.3/8.5/11/14/16/17：冻结 Utility 实验性准入、技能只读候选、EQS 过期身份、边界交接、预算与容量要求。
- 本机环境：安装版 UE 5.8.2 / CL 56702186 与源码版 UE 5.8.2 / CL 0（Compatible CL 55116800）均含 `FStateTreeConsiderationBase`、`UStateTreeState::AddConsideration` 和 `TrySelectChildrenWithHighestUtility`；三 Target 与 Windows cook 已通过。
- UE MCP 当前会话未暴露；资产工作按项目允许降级为原生 UE C++ commandlet 与 Python，并已完成保存、独立进程冷回读、蓝图状态回读及资产校验。

## 3. 行为与契约

### 主流程

Brain 的预算化感知先发布有限知识快照，再无副作用地枚举 Profile 规则和当前已授予 Spec，形成一个可解释的最佳技能候选及 Cast/Attack/Reposition/Wait 分数。StateTree 的最高效用父状态只在满足 Enter Condition 的子状态间比较 Consideration 分数；硬性归位、故障退避、死亡和控制权仍位于 Utility 之上。

Hero 在持续 Attack 中出现超过当前分数和切换阈值的技能候选时，仅申请匹配 Order 的边界票据。Ready 后重新执行只读预检：候选仍成立则精确结束旧 Attack，经 Receipt 进入 Utility 重选并由 Cast 分支准备/执行；不再成立则 Keep，释放票据并继续原 Order。超时由既有 Order/Scheduler 释放，旧票据不得恢复切换资格。

Ranged Guard 在目标过近且配置了 EQS 时由 Utility 选择 Reposition。查询 Task 取得 World 预算后启动一次 EQS；只有完整查询身份仍匹配时才发布点 Move 意图。Move 仍经 `PreflightAIOrder` 和导航执行，完成/失败统一进入 Receipt/Resolve，然后重新选择 Attack、Reposition 或 Wait。

### 状态转换

`WaitAssignment → HardSelect(Blocked / Retry / Return / TacticalSelect)`；前三项按资产顺序硬优先，`TacticalSelect` 必须是最后兜底并使用 `TrySelectChildrenWithHighestUtility`，候选为 `Cast / Reposition / Attack / Guard`。Return 与每个战术写命令分支保持 `Prepare(or Query) → Execute → Resolve`，Guard/Resolve 回到 HardSelect，活动路径仍只有一个写入者。普通战术重评经 Attack 边界完成退出；紧急归位继续沿阶段 B 的精确取消路径。

### 输入、输出与数据约束

- `FCombatAIAbilityUsageRule` 只保存 Ability DefinitionId、用途、目标策略、基础效用、最低有效目标数、法力保留与普通中断偏好；数值有限、有界、DefinitionId 唯一且类型必须为 `CombatAbility`。
- 候选保存 SpecHandle、规则索引、目标弱引用/生命、点快照、知识修订、分项与最终 `[0,1]` 分数；不保存 Ability UObject 作为配置身份。
- 被动、AutoCast 或攻击修饰技能不生成 Cast；目标模式由 AbilityData 行为标签决定，目标合法性、费用、冷却、沉默、物品修订与最终激活仍以公共预检/执行为准。`TacticalLocation` 目标策略在阶段 C 明确校验失败，避免把 Reposition 查询结果误当施法目标。
- 战术 EQS 只返回有限点；配置要求 Order 的 `MoveDestinationQuery` 为空，防止二次 EQS 改写落点。查询失败形成可确认失败凭证，移动失败沿既有有界退避。
- World 预算每帧分别限制感知扫描和 EQS 发起；拒绝只产生短期 Scheduler 延期，不丢失生命周期清理、完成回执或攻击边界处理。初始采样按稳定单位身份错峰。
- 统计至少包含请求、执行、延期、取消、陈旧结果、活动/峰值查询、查询耗时 p95/p99 和命令提交数；容量报告按 64/128/256 分档，不借用历史 M7 容量。

### 权威边界与权限

所有感知、Utility 输入、EQS、候选预检、StateTree 与 Order 提交仅在服务器执行。Consideration 和 Condition 无副作用；唯一写命令入口仍是 Brain → `IssueAutonomousOrder`。客户端不加载为决策所需的私有工作区，不新增 RPC 或权威状态。

### 失败、取消、过期、死亡、EndPlay 与重复请求

- 未授予、被动、冷却、资源不足、沉默、目标无效和库存修订变化均跳过或形成一次失败凭证；不扣费、不开始冷却、不实际激活来试探。
- 同单位只允许一个 EQS；重复 Tick 不重复发起。Task Exit、Scope 结束、Profile 更换、接管、死亡、EndPlay 与 World teardown 先增加/核对身份，再 Abort 精确 QueryId 并清理 inbox/延期 Schedule。
- EQS 回调必须匹配 QueryId、QueryGeneration、Run、ControlEpoch、SelfLife、Scope、职责修订、目标弱引用和目标生命；普通无关感知修订不使查询饥饿，目标/职责/查询参数变化使旧结果失效。
- Attack 边界 Ready 时若技能不再合法或分差不足，执行 Keep；Ready 后超时、重复通知和 Exit 只释放一次。Cast 被拒绝/中断或目标在准备后失效，统一确认 Receipt 并回到有界重评，不无限同帧重试。
- 预算延期不会补发积压的旧查询；下一次执行重新读取当前事实。生命周期清理和终态消费不受普通配额阻塞。

### 兼容、版本与迁移

`AIProfileVersion=1` 的阶段 A/B 资产继续加载并忽略战术字段，保持同步首次感知和原有周期行为；`AIProfileVersion=2` 才允许启用 Utility/Ability/EQS 和 World 预算错峰。新 Hero/Ranged Profile 显式为 v2。没有自动资产迁移，不改变 `CombatContentVersion`、Release、Contract、Tag 或 Event 版本。`TacticalLocation` 的枚举数值保留以兼容序列化，但验证明确拒绝，不需要内容迁移。每次 UE 升级重新运行 Utility 三 Target/cook 门；若实验性 API 不再兼容，则 v2 Utility Profile 拒绝启动或本阶段延期，不在 C++ 改用隐式决策器。

## 4. 实施计划

| 文件/资产 | 变更 | 原因 | blast radius |
| --- | --- | --- | --- |
| `AI/CombatAITacticalTypes.*`、`CombatAITacticalTasks.*` | 技能用途、纯评分、候选、EQS Task、Condition/Consideration；隐藏并拒绝未实现的技能 `TacticalLocation` | 将纯策略与引擎适配分离 | 新 v2 Profile |
| `AI/CombatAIWorldSubsystem.*` | World 感知/EQS 配额、稳定错峰、统计快照 | 防止同帧全量扫描与查询风暴 | 仅自主 AI 查询 |
| `AI/CombatAIProfileData.*`、`CombatAIBrainComponent.*`、`CombatAIRoleBrain.cpp` | v1/v2 校验、候选/查询工作区、边界重评与生命周期清理；预算仅作用于 v2 | 复用现有 Scope/Bridge | v1 默认路径保持 |
| `AI/CombatAIStateTreeSchema.*`、`Validation/CombatAIAssetBuilder.*` | 允许项目 Consideration，生成 HardSelect + Utility/Query/Cast 树，并对既有战术资产安全重建保存 | 真实实验性 API 准入与硬优先级 | Editor 构建 + v2 资产 |
| `AI/CombatAITacticalTargetContext.*` | 向 EQS 只暴露当前查询冻结目标 | 不从全局/黑板偷读目标 | EQS 查询实例 |
| `Demo/CombatAITacticalDemoArena.*`、`Tools/setup_ai_tactics.py`、`Tools/RunDedicated.ps1` | Hero/Ranged 场景、资产接线、PIE/Dedicated/容量入口 | 可玩与联机证据 | 新独立地图 |
| `Tests/CombatAITacticsTests.cpp`、PIE/Network 场景 | Red/Green：评分、技能预检、Utility、EQS、边界、预算、清理、64/128/256 | 直接覆盖 C 阶段 AC | 开发测试代码 |
| `/Game/Combat/Demo/AI/Tactics/*` | Utility StateTree、EQS、v2 Profile、单位/地图/蓝图 | 保存并 cook 的示例 | 新资产，无旧引用改写 |
| `10-17`、`20-04`、`00-01`、`00-04`、README | 实际接口、版本边界、配置、证据与剩余缺口 | 保持当前事实唯一来源 | 文档 |

## 5. 验收标准（AC）

- [x] AC-01：纯评分覆盖上下界、NaN/Inf、分差、保持和同分；真实 StateTree 以最高 Utility 选择合法子状态，非法高分候选被 Condition 排除；Blocked、Retry、Return 硬优先于 Utility，Guard/Resolve 回到 HardSelect；安装版/源码版 Editor、Server、Client 及 cook 全部通过。
- [x] AC-02：技能规则只解析已授予主动技能并经 `PreflightAIOrder`；未授予、被动/AutoCast/Attack、冷却、资源、沉默、目标失效、满血 Heal、重复规则和保留的 `TacticalLocation` 均有正反测试，候选评估不产生激活、扣费、冷却或 Order。
- [x] AC-03：英雄从持续普攻请求真实边界，覆盖发射后切 Cast、Ready 后候选失效 Keep、HoldTimeout、同步/异步失败恢复、重复/迟到通知和完成凭证 exactly-once；不重置攻击时钟、不误取消新命令。
- [x] AC-04：战术 EQS 覆盖成功、失败、预算延期、显式取消、目标/生命/职责/运行变化后的迟到结果和单单位在途上限；采纳点只形成一次精确 Move Order，不与 Order 自身 EQS 叠加。
- [x] AC-05：v2 World 预算在同帧限制感知和 EQS，初始采样稳定错峰；v1 保持阶段 B 同步首次感知与既有周期；受击威胁不再触发第二次全 World 枚举；死亡、接管、Profile 更换、EndPlay/teardown 后活动查询、Schedule、票据和注册计数归零。
- [x] AC-06：独立 Tactics 地图展示 Hero Bot 施法与 Ranged Guard EQS 站位；资产编译/保存/冷回读、CombatAssetValidation、PIE 和 Dedicated 双客户端证明服务器唯一决策与既有战斗复制。
- [x] AC-07：64/128/256 AI 分档报告单位数、查询执行/延期、峰值在途、p95/p99 与命令数，预算无越界且无泄漏；本轮数据未触发公共 Targeting 索引升级。
- [x] AC-08：相关专项与完整 `Combat.` 回归、文档校验、`git diff --check`、F2 和 delivery Gate 通过；所有未执行项与剩余风险显式记录。

## 6. Definition of Done

- [x] 先新增并实际运行最小失败测试，记录 Utility Schema/技能候选/EQS/预算缺口的 Red 原因，再做最小实现和 Green。
- [x] 只有 v2 显式启用战术与 World 预算错峰；旧 v1 Profile、AI-002 基本树和 AI-003 三类角色资产/同步首次感知行为保持通过。
- [x] StateTree、EQS、Profile、蓝图和地图资产由原生 UE API 创建/编译/保存并在新进程冷回读，`CombatAssetValidation` 通过。
- [x] Editor/Server/Client 三 Target、`Combat.AI`、完整 `Combat.`、Tactics PIE、Dedicated 双客户端、Windows cook、64/128/256 容量均有本轮证据。
- [x] F2 对抗审查覆盖服务器权威、单写入者、实验性 API、过期回调、边界 Keep/timeout、预算饥饿、版本迁移和回滚；高风险发现关闭。
- [x] `10-17`、`20-04`、`00-01`、`00-04` 与必要 README 入口同步；`git diff --check` 通过，不修改 A/B 验收历史，不混入生成目录。

## 7. 测试矩阵与命令

| 层级 | 用例/命令 | 预期证据 | 结果 |
| --- | --- | --- | --- |
| Gate | `python -X utf8 -B Tools/task_gate.py --mode preflight/plan/build/delivery --spec Doc/CombatSystem/Specs/AI-004-statetree-tactics-capacity.spec.md --kind feature` | `Saved/AI-004/*.json`，阶段顺序和结论一致 | preflight/plan/build/delivery 均 0 error；delivery 识别 66 个变更文件，报告 `Saved/AI-004/delivery.json` |
| Red/Pure | UBT 生成的 `CombatAITacticsTests.cpp.obj.rsp` + MSVC `/Zs`；随后 `Automation RunTests Combat.AI.Tactics` | 评分、Profile v1/v2、规则合法性、只读预检与预算边界先失败后通过 | Utility Red 为缺少 `CombatAITacticalTypes.h`；F2 又以旧回调、失败界限和边界偏好 Red 固化缺陷；最终 21/21，`Saved/AI-004/Reports/Tactics-F2-R5/index.json` |
| StateTree/World | `Automation RunTests Combat.AI` | HardSelect 优先级、Utility 选择、Cast/Attack/Reposition、EQS 身份、边界 Keep/timeout、v1 同步首次感知、生命周期及 A/B 回归 | 42/42（39 clean + 3 success-with-warnings，均为 PIE 启停时 RecastNavMesh/Crowd 初始化提示），`Saved/AI-004/Reports/AI-F2-R3/index.json` |
| 构建 | UE 5.8 安装版 Editor + 源码版 Server/Client Development | 三 Target `Result: Succeeded`，证明 Consideration API 准入 | Editor、Server、Client 各 23/23 actions，均 `Result: Succeeded` |
| 资产 | `CombatAIAssets -Tactics`、`setup_ai_tactics.py`、`CombatAssetValidation`、冷进程回读 | 已存在的战术根树也被安全重建；Utility/EQS/Profile/蓝图/地图编译保存且引用闭合 | 42 assets、0 error/0 warning；冷回读 1/1；5 个蓝图均 `BS_UP_TO_DATE`。见 `Saved/AI-004/asset-validation-final.json`、`Reports/Assets-ColdReadback-Final/index.json`、`tactics-demo-readback.json` |
| PIE | Tactics map NullRHI PIE | Hero 实际施法；Ranged Guard 实际站位/攻击；诊断与动作一致 | 1/1；HeroLaunches/Casts/Healed=1/1/1，RangedMoved/EQSSuccess=1/1。报告 `Saved/AI-004/Reports/PIE-Tactics-Final/index.json`；仅有 PIE 启停时 RecastNavMesh/Crowd 提示 |
| Network / Dedicated | `Tools/RunDedicated.ps1 -InstalledEditor -AITactics` | Server 有决策/动作；两个客户端 Brain 停止且观察相同移动/技能结果 | 通过；Server Brain running 且 `ActiveEQS=0`，两客户端 Brain stopped、Ranged 位置和受伤目标数一致。见 `Saved/UEEnvironment/Dedicated-Installed-AITactics/DedicatedSummary.txt` |
| Soak / Perf | `Combat.AI.Tactics.Capacity64/128/256` | 每档 p95/p99、执行/延期/峰值/命令/清理；预算不越界 | 64：Perception 92/92/0，EQS 78/64/14，Peak 6，P95/P99 50/50 ms，Commands 64；128：281/281/0，304/128/176，Peak 7，50/50 ms，128；256：1246/957/289，1361/256/1105，Peak 6，50/50 ms，256。格式均为 Requests/Granted/Deferred，报告同 Tactics F2 R5 |
| 回归 | `Automation RunTests Combat.`、Windows cook | 完整 Combat 0 failed；Tactics 地图 cook 成功 | 143/143（138 clean + 5 success-with-warnings，均为既有 PIE/Crowd 与调试拒绝路径日志），`Saved/AI-004/Reports/Combat-Final/index.json`；cook 680 total（673 cooked、7 platform-skipped），0 error/1 UE MCP EULA warning，`Saved/AI-004/cook-tactics-final.log` |
| 文档 | `python -X utf8 -B Tools/validate_docs.py`、`git diff --check` | 0 error，链接与格式有效 | 87 篇 Markdown、466 个本地链接、0 error；`git diff --check` 0 error（仅换行规范提示） |

## 8. 风险、回滚与升级

- 风险：实验性 Consideration API 在两套 UE 构建配置不同；HardSelect 的父子转移可能错误绕过职责条件；Utility 重选与攻击 Ready/timeout 形成竞态；EQS 回调可能跨 Scope/生命到达；World 配额可能造成饥饿或误改 v1 时序；大规模全 World 枚举可能仍超预算；测试动态修改 Ability CDO 可能污染后续 World。
- 回滚方式：撤下或删除 `/Game/Combat/Demo/AI/Tactics` v2 Profile/地图即可停用新路径；代码按 AI-004 文件和 v2 分支增量回退，v1 Profile 与阶段 A/B 资产保持可加载。不得 `git reset --hard` 或整体回退用户/既有提交。
- 触发升级的条件：任一 Target/cook 无法承载 Consideration；容量数据要求引入公共 Targeting 索引；必须修改网络/发布/Tag/Event/CombatContent 契约；三轮同类修复不收敛；或无法确保旧 EQS/边界回调不污染新运行。
- 需要人决定的问题：当前无。若容量证据触发公共空间索引或 Utility API 准入失败，将停止并回到 F1，向用户给出数据与选项。

## 9. 交付证据

- 代码/资产 diff：阶段 C 新增 Utility/技能候选/EQS/World 预算、Hero/Ranged 演示和直接测试；旧 A/B Profile 与资产不迁移。`TacticalLocation` 枚举值仅为序列化兼容保留，编辑器隐藏且校验拒绝。
- 构建结果：最初 Utility Red 由缺少 `CombatAITacticalTypes.h` 证明；最终安装版 Editor、源码版 Server/Client 均完成 23/23 actions 且 `Result: Succeeded`。
- Automation/PIE/Dedicated 报告：战术 21/21、AI 42/42、完整 Combat 143/143；Tactics PIE 和 Dedicated 双客户端通过。报告路径及容量明细见 §7。
- 资产/cook：原生 commandlet + Python 降级流程完成生成和重建；42 assets 0/0、冷回读 1/1、蓝图均 `BS_UP_TO_DATE`；Windows cook 680 total、0 error，仅 UE MCP EULA warning。
- F2 证据：逐项复核只有 `IssueAutonomousOrder` 写命令；战术代码无直接 Ability 激活、属性写入、Actor Timer 或 Transform 旁路；客户端/非 Authority 不启动 Brain。查询回调同时校验 QueryId + QueryGeneration 及运行/控制/生命/Scope/职责/目标身份，取消先失效工作区再 Abort 并 exactly-once 结算；边界 Ready 后重算候选、分差与 `InterruptPreference`。对应 Red/Green 和最终报告位于 `Saved/AI-004/Reports/`。
- 文档/Gate：`validate_docs.py` 为 87 篇 Markdown / 466 个本地链接 / 0 error；`git diff --check` 为 0 error（仅换行规范提示）；delivery Gate 为 0 error / 66 changed files，报告写入 `Saved/AI-004/delivery.json`。
- 验收归档：2026-09-22 用户确认阶段 C 验收完成并授权本地提交；文档校验为 87 篇 Markdown / 466 个本地链接 / 0 error，`git diff --check` 为 0 error，delivery Gate 为 0 error / 66 changed files，29 个新增 Unreal 二进制资产均命中 Git LFS 且无生成目录混入。报告见 `Saved/AI-004/acceptance-docs.json` 与 `acceptance-delivery.json`；本轮不据此新增 UE 构建或运行验证结论。
- 未执行验证及原因：人工视觉/手感、网络损伤、长时间 AI soak、打包可执行文件启动未执行；均不在本 Spec AC，不能据此声称已覆盖这些运行环境。
- 剩余风险：Consideration 仍是 UE 实验性 API，升级引擎必须重跑三 Target/cook 准入；范围/LOS 不是完整迷雾，当前 World 枚举虽满足本轮自动化容量门，真实复杂地图若触发预算仍需独立评审公共空间索引。当前无阻塞交付的高风险发现。

## 10. 变更记录

| 版本 | 日期 | 修改 | 原因 |
| --- | --- | --- | --- |
| 0.1 | 2026-09-21 | 初稿：冻结 Utility、技能候选、EQS、World 预算、Hero/Ranged Demo 与容量矩阵 | 用户授权开始阶段 C；AI-003 保持历史归档 |
| 0.2 | 2026-09-21～22 | 增加 HardSelect、v1 感知兼容、完整候选负向矩阵；保留但隐藏并拒绝技能 `TacticalLocation` 策略；关闭 F2 并写回最终分层证据 | F2 发现实现与原 Spec 在硬优先级、公开配置承诺和兼容边界上不一致，重回 F1 后修复并复验；交付回写不改变批准范围 |
| 0.2 | 2026-09-22 | 用户确认阶段 C 验收完成并授权本地 Git 提交；同步验收记录和最终交付检查 | 仅归档与提交已验证的 AI-004 变更，不重跑 UE 运行矩阵，不推送 |

## 11. 路由、自评与 Reflect

### 路由记录

- 原始需求摘要：继续 StateTree 阶段 C 的实现与验证。
- 主 Skill：`Skills/combat-feature-development/SKILL.md`
- 选择依据：任务同时修改 C++、StateTree/EQS/DataAsset、Demo、测试与文档，属于通用 Combat 功能交付；不新增具体技能结算。
- 备选 Skill 与排除理由：`combat-skill-development` 不适用，因为只读取既有 Ability 并走公共 Order/ASC；`skill-creator` 不适用。
- 路由置信度：`high`

### 交付自评

| 维度 | 权重 | 分数（0–5） | 证据与扣分原因 |
| --- | ---: | ---: | --- |
| 需求与 AC | 20% | 5.0 | 8 项 AC 均有直接实现与分层证据，Non-Goals 未扩张 |
| 架构与权限 | 20% | 5.0 | 服务器唯一决策、单 Order 写入者、公共预检、Scheduler 与完整异步身份均经 F2 复核 |
| 实现与数据 | 20% | 5.0 | v2 显式启用，v1 兼容；资产冷回读、蓝图状态和配置拒绝路径闭合 |
| 验证证据 | 20% | 4.8 | 三 Target、专项/全量、资产、PIE、Dedicated、cook 和三档容量全绿；未做非 AC 的长 soak/网络损伤/打包启动 |
| 文档与可观测性 | 10% | 5.0 | Spec、台账、ADR/Gap、设计和配置指南同步，容量与生命周期统计可回读 |
| 交付卫生 | 10% | 4.8 | F2 三轮内收敛、旁路扫描和最终 Gate 完成；交付时工作区未提交并等待用户 review |

- 计算总分：`4.9 / 5.0`
- 硬性封顶或未执行项：未触发硬性封顶；人工视觉/手感、网络损伤、长时间 soak 和打包启动不属于本轮必需验证。
- 自评结论：`EVIDENCE_SUFFICIENT`
- 用户验收状态：2026-09-22 用户确认阶段 C 验收完成并授权本地提交。

### Reflect 与调优

- 观察与证据：首轮 F2 发现根树 Utility 可能越过硬职责、v1 被新增预算改变时序、技能目标策略公开了未实现语义，以及 EQS 旧回调/失败重试和攻击边界偏好缺少直接断言；对应问题均以 Red/Green 和最终 21/21 战术专项关闭。
- 根因类别：`单次实现`；是本任务的边界与测试覆盖缺口，没有证据表明两个独立任务重复遗漏。
- 调整文件与预期收益：只修 AI-004 实现、资产、Spec 和专题文档，不修改通用 Skill/模板；HardSelect、v1 兼容、隐藏拒绝策略与完整异步身份成为可回归事实。
- 回归验证：42/42 `Combat.AI`、143/143 `Combat.`、资产/PIE/Dedicated/cook/容量及最终文档 Gate。
- 需要用户决定的问题：当前无；阶段 C 已由用户明确验收，后续 D 阶段或公共空间索引扩展需另行授权。
