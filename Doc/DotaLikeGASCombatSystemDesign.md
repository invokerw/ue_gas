# UE GAS Dota-Like Combat 文档索引

> 当前状态：`combat_v1_rc1` 核心发布契约已冻结，M0-M8 共 82/82 Task、9/9 里程碑已通过用户验收。
> 当前工程：UE 5.8；GameplayAbilities/GameplayTags/GameplayTasks 已接入，Combat 实现位于 `Source/ue_gas/Combat`。
> 状态权威：[00 开发进度台账](CombatSystem/00-Progress-Tracker.md)。

原单体设计文档已按“当前架构、运行时语义、实施与测试、冻结决策、验收证据”拆分。本文只维护导航和文档职责；项目概览与启动方式见根目录 [README](../README.md)。

## 文档导航

| 文档 | 解决的问题 | 推荐读者 |
| --- | --- | --- |
| [00 开发进度台账](CombatSystem/00-Progress-Tracker.md) | M0-M8、Task、Gate、用户验收状态和证据 | 全员；每次任务完成必须更新 |
| [01 范围、架构与硬约束](CombatSystem/01-Scope-Architecture.md) | 系统边界、核心对象、数据层、权威来源 | 全员 |
| [02 调度、事务与时序](CombatSystem/02-Scheduler-Transactions.md) | Scheduler、EventId、重入、catch-up | 核心战斗开发 |
| [03 Ability、目标与蓝图接口](CombatSystem/03-Ability-Targeting-Blueprint.md) | 施法生命周期、目标校验、数据驱动 Ability | 技能开发 |
| [04 Modifier、属性与 Motion](CombatSystem/04-Modifier-Attributes-Motion.md) | GE/Runtime 分工、Hook、状态、驱散、强制位移 | 核心战斗开发 |
| [05 Damage 与 Heal 管线](CombatSystem/05-Damage-Heal.md) | 统一伤害/治疗事务、公式、结果回报 | 核心战斗开发 |
| [06 普攻、法球、Projectile 与 Thinker](CombatSystem/06-Attack-Projectile-Thinker.md) | AttackRecord、法球仲裁、投射物、AoE | 技能与战斗开发 |
| [07 Order 与 NavMesh 移动](CombatSystem/07-Order-Movement.md) | 指令队列、追击、异步回调、避让 | 单位控制开发 |
| [08 数据、网络、UI 与可观测性](CombatSystem/08-Data-Network-Observability.md) | PrimaryAsset、复制矩阵、RPC 安全、日志 | 网络与工具开发 |
| [09 示例技能](CombatSystem/09-Example-Skills.md) | 七个纵向切片与可玩远程攻击 Demo 的落地方式 | 技能开发与验收 |
| [10 实施路线图与关键节点](CombatSystem/10-Implementation-Roadmap.md) | M0-M8 历史 WBS、依赖、Gate 和验收标准 | 负责人、维护者 |
| [11 测试计划](CombatSystem/11-Test-Plan.md) | 自动化测试分层、关键用例、里程碑准入 | 开发与测试 |
| [12 决策与缺口登记](CombatSystem/12-Decisions-Gaps.md) | 已定原则、遗漏项、开放决策和进入节点 | 负责人、架构评审 |
| [13 UE MCP 开发工作流](CombatSystem/13-UE-MCP-Workflow.md) | Editor/资产/蓝图/PIE 的 MCP 操作与验证闭环 | 全体开发者与自动化 Agent |
| [34 客户端与服务器交互流程](CombatSystem/34-Client-Server-Interaction.md) | 从客户端 Order 到服务器移动、施法、伤害和复制回显的完整时序 | 联机、单位控制、技能与 UI 开发 |
| [35 服务器权威单位移动改造与验收](CombatSystem/35-Server-Authoritative-Movement-Kickoff.md) | PlayerController 指挥、AIController 服务器移动、Command Pawn、Crowd 与 Dedicated Gate 的当前实现和证据 | 单位控制、网络、AI、测试与维护者 |

冻结与发布文档：

| 范围 | 文档 | 用途 |
| --- | --- | --- |
| M0-M7 | `14`-`29` 决策与验收记录 | 保存当时的决策、环境、Gate 命令和用户验收证据 |
| M8 | [30 候选发布决策](CombatSystem/30-M8-Release-Candidate-Decision.md) | 冻结 v1 发布边界、预测和性能策略 |
| M8 | [31 生命周期审计](CombatSystem/31-M8-Lifecycle-Audit.md) | Handle、Delegate、Schedule、Runtime 和 Actor 清理契约 |
| 扩展 | [32 公共扩展与迁移指南](CombatSystem/32-M8-Public-Extension-Guide.md) | 新技能、DataAsset、蓝图事件和版本迁移入口 |
| M8 | [33 候选发布验收](CombatSystem/33-M8-Acceptance.md) | 最近一次完整发布 Gate 证据 |

## 建议阅读路径

- 初次了解：根 README → 00 → 01 → 32。
- 实现技能：03 → 05 → 06 → 09 → 25 → 32。
- 修改战斗内核：01 → 对应 02-08 专题 → 12 → 31 → 11。
- 理解当前单位控制与联机：34 → 07 → 08 → 27 → 28 → 11。
- 维护服务器权威移动：35 → 34 → 07 → 01 → 31 → 12 → 11。
- 核对发布状态：00 → 30 → 33。

## 原章节迁移

| 原章节 | 新位置 |
| --- | --- |
| 1-3 背景、参考结构、总体架构 | 01、02、08 |
| 4 Ability、12 蓝图替代 Lua | 03 |
| 5 Modifier、6 属性 | 04 |
| 7 Damage/Heal | 05 |
| 8 普攻、9 Projectile、10 Thinker | 06 |
| 11 Order/NavMesh | 07 |
| 13 网络同步 | 08 |
| 14 示例技能 | 09 |
| 15 集成步骤 | 10 |
| 16 测试 | 11 |
| 17 风险、18 落地原则 | 12、01 |

## 文档约定

- “必须/禁止”表示 v1 不可破坏的系统约束；“建议”表示默认实现，可通过设计决策记录变更。
- `10` 中“任务默认未开始”描述的是 2026-08-24 的历史计划基线；当前完成度只读取 `00`。
- `14`-`31`、`33` 保存冻结决策和验收时点，不因后续实现自然演进而改写历史证据。
- 开放问题统一登记在 12，并写明最迟决策节点；不在功能文档中悄悄引入第二套语义。
- 代码、DataAsset、GameplayTag 和网络载荷的命名发生变化时，先更新 01/08，再更新对应子系统文档与测试矩阵。
- 涉及 Unreal Editor、Content 资产、蓝图和 PIE 的任务优先使用 UE MCP 获取当前状态、执行受控操作并回读验证；最终完成状态仍以源码 diff、编译、Automation 和对应 Gate 为准。
- M0-M8 已完成。post-v1 工作必须建立新的 Task/ADR/验收边界，不能继续复用已关闭里程碑伪装完成度。
