# UE GAS Dota-Like 战斗系统设计文档索引

> 状态：设计拆分完成，代码尚未开始落地。
> 工程基线：UE 5.8 模板工程；截至 2026-08-24，项目未启用 GameplayAbilities，`Build.cs` 未加入 GAS 依赖，`Source/ue_gas/Combat` 尚不存在。

原单体设计文档已按“架构、运行时系统、实施路线、测试、决策”拆分。本文只保留总入口和阅读顺序，避免设计说明与任务状态混为一谈。

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
| [09 示例技能](CombatSystem/09-Example-Skills.md) | 五个纵向切片技能的落地方式 | 技能开发与验收 |
| [10 实施路线图与关键节点](CombatSystem/10-Implementation-Roadmap.md) | WBS、依赖、里程碑、Gate、验收标准 | 负责人、执行者 |
| [11 测试计划](CombatSystem/11-Test-Plan.md) | 自动化测试分层、关键用例、里程碑准入 | 开发与测试 |
| [12 决策与缺口登记](CombatSystem/12-Decisions-Gaps.md) | 已定原则、遗漏项、开放决策和进入节点 | 负责人、架构评审 |
| [13 UE MCP 开发工作流](CombatSystem/13-UE-MCP-Workflow.md) | Editor/资产/蓝图/PIE 的 MCP 操作与验证闭环 | 全体开发者与自动化 Agent |

## 建议阅读路径

- 查看当前状态：00；开始排任务：01 → 10 → 12 → 13 → 11。
- 实现战斗内核：01 → 02 → 04 → 05。
- 实现技能：03 → 05 → 06 → 09。
- 实现单位控制和联机：07 → 08 → 11。

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

- “必须/禁止”表示第一版不可破坏的系统约束；“建议”表示默认实现，可通过设计决策记录变更。
- 所有路线图任务默认状态均为 `未开始`；模板中的 Strategy/TwinStick 代码标记为“可参考”，不标记为战斗能力已完成。
- 开放问题统一登记在 12，并写明最迟决策节点；不在功能文档中悄悄引入第二套语义。
- 代码、DataAsset、GameplayTag 和网络载荷的命名发生变化时，先更新 01/08，再更新对应子系统文档与测试矩阵。
- 涉及 Unreal Editor、Content 资产、蓝图和 PIE 的任务优先使用 UE MCP 获取当前状态、执行受控操作并回读验证；最终完成状态仍以源码 diff、编译、Automation 和对应 Gate 为准。
- 每个 M0-M8 里程碑完成后必须暂停并等待用户验收；未收到用户明确的“继续下一阶段”指令，不得开始下一里程碑的实现或资产修改。
