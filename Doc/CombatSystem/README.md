# UE GAS Dota-Like Combat 文档索引

> 当前契约：ECON-001 在 ITEM-001 之上实现 `combat_v3_economy_rc1` 经济扩展；本地验证已完成，用户验收仍进行中。M0-M8 的 v1 共 82/82 Task、9/9 里程碑已通过用户验收，历史证据保持冻结。
> 当前工程：UE 5.8；GameplayAbilities/GameplayTags/GameplayTasks 已接入，Combat 实现位于 `Source/Combat/Combat`。
> 当前 Post-M8：SAM 服务器权威单位移动与 DEMO-901 卓尔游侠 Demo 均已通过用户验收。
> 底部 HUD：工程验证已完成，待用户实机复验；状态与远端验证记录见台账 §12.3。
> 状态权威：[00-01 开发进度台账](00-Project/00-01-Progress-Tracker.md)。

原单体设计文档已按“当前架构、运行时语义、实施与测试、冻结决策、验收证据”拆分。本文只维护导航和文档职责；项目概览与启动方式见根目录 [README](../../README.md)。一般使用下载版 UE 启动项目；只有测试 Dedicated Server 时才使用源码版 UE。

编号使用“目录号－目录内序号”：`00-Project` 为 `00-NN`，`10-Architecture` 为 `10-NN`，`20-Content` 为 `20-NN`，`30-Tooling` 为 `30-NN`，`90-History` 为 `90-NN`。下划线模板和 `Specs` 任务文件保留专用命名；历史决策和验收只进入 `90-History`，不要在历史文件中追加当前实现说明。

## 目录结构

| 目录 | 定位 | 是否作为当前依据 |
| --- | --- | --- |
| [`00-Project`](00-Project/) | 状态台账、AI 开发流程、路线图、测试计划、ADR/Gap | 是；状态以 `00-01-Progress-Tracker.md` 为准 |
| [`10-Architecture`](10-Architecture/) | 当前架构和运行时语义，包括联机、移动、UI | 是；按变更类型读取 |
| [`20-Content`](20-Content/) | 示例技能、技能模板、公开扩展与迁移 | 是；新增内容的公开入口 |
| [`30-Tooling`](30-Tooling/) | UE MCP 操作与诊断工作流 | 是；Editor/资产任务优先遵循 |
| [`90-History`](90-History/) | M0–M8 冻结决策、验收记录和历史环境证据 | 仅用于历史追溯，不推断当前状态 |
| [`Specs`](Specs/) | 每个新任务的可执行规格 | 按任务新增，模板为 `_template.spec.md` |

项目模板：[`Intake`](00-Project/_intake-template.md)、[`Gate 检查表`](00-Project/_gate-checklist.md)、[`交付记录`](00-Project/_delivery-record-template.md)、[`Spec`](Specs/_template.spec.md)。需求路由和交付复盘使用 [Combat 任务路由 Skill](../../Skills/combat-task-router/SKILL.md)；功能开发任务使用 [Combat 功能开发 Skill](../../Skills/combat-feature-development/SKILL.md)；实现 GAS 技能时使用 [Combat 技能开发 Skill](../../Skills/combat-skill-development/SKILL.md)。这些 Skill 只保留在本仓库 `Skills/`，不安装到用户级目录。

## 文档导航

### 00-Project：项目状态与流程

| 文档 | 解决的问题 | 推荐读者 |
| --- | --- | --- |
| [00-01 开发进度台账](00-Project/00-01-Progress-Tracker.md) | M0-M8、Task、Gate、用户验收状态和证据 | 全员；每次任务完成必须更新 |
| [00-02 实施路线图与关键节点](00-Project/00-02-Implementation-Roadmap.md) | M0-M8 历史 WBS、依赖、Gate 和验收标准 | 负责人、维护者 |
| [00-03 测试计划](00-Project/00-03-Test-Plan.md) | 自动化测试分层、关键用例、里程碑准入 | 开发与测试 |
| [00-04 决策与缺口登记](00-Project/00-04-Decisions-Gaps.md) | 已定原则、遗漏项、开放决策和进入节点 | 负责人、架构评审 |
| [00-05 AI-Native 开发流程与文档体系](00-Project/00-05-AI-Native-Development-Workflow.md) | 将 DDD/SDD/TDD、Gate、权限和 Reflect 落到当前 Combat 仓库 | 全体开发者与自动化 Agent |

### 10-Architecture：运行时架构

| 文档 | 解决的问题 | 推荐读者 |
| --- | --- | --- |
| [10-01 范围、架构与硬约束](10-Architecture/10-01-Scope-Architecture.md) | 系统边界、核心对象、数据层、权威来源 | 全员 |
| [10-02 调度、事务与时序](10-Architecture/10-02-Scheduler-Transactions.md) | Scheduler、EventId、重入、catch-up | 核心战斗开发 |
| [10-03 Ability、目标与蓝图接口](10-Architecture/10-03-Ability-Targeting-Blueprint.md) | 施法生命周期、目标校验、数据驱动 Ability | 技能开发 |
| [10-04 Modifier、属性与 Motion](10-Architecture/10-04-Modifier-Attributes-Motion.md) | GE/Runtime 分工、Hook、状态、驱散、强制位移 | 核心战斗开发 |
| [10-05 Damage 与 Heal 管线](10-Architecture/10-05-Damage-Heal.md) | 统一伤害/治疗事务、公式、结果回报 | 核心战斗开发 |
| [10-06 普攻、法球、Projectile 与 Thinker](10-Architecture/10-06-Attack-Projectile-Thinker.md) | AttackRecord、法球仲裁、投射物、AoE | 技能与战斗开发 |
| [10-07 Order 与 NavMesh 移动](10-Architecture/10-07-Order-Movement.md) | 指令队列、追击、异步回调、避让 | 单位控制开发 |
| [10-08 数据、网络、UI 与可观测性](10-Architecture/10-08-Data-Network-Observability.md) | PrimaryAsset、复制矩阵、RPC 安全、日志 | 网络与工具开发 |
| [10-09 客户端与服务器交互流程](10-Architecture/10-09-Client-Server-Interaction.md) | 从客户端 Order 到服务器移动、施法、伤害和复制回显的完整时序 | 联机、单位控制、技能与 UI 开发 |
| [10-10 服务器权威单位移动改造与验收](10-Architecture/10-10-Server-Authoritative-Movement-Kickoff.md) | PlayerController 指挥、AIController 服务器移动、Command Pawn、Crowd 与 Dedicated Gate 的当前实现和证据 | 单位控制、网络、AI、测试与维护者 |
| [10-11 头顶 UI：C++ 与蓝图边界](10-Architecture/10-11-Overhead-Blueprint-UI.md) | 展示快照、生命周期、UMG 蓝图维护入口与迁移 | UI、美术、战斗与网络开发 |
| [10-14 物品系统](10-Architecture/10-14-Item-System.md) | 装备/背包、主动被动、场景拾取、HUD、版本与迁移 | 玩法、内容、网络与 UI 开发 |
| [10-15 经济、商店与合成](10-Architecture/10-15-Economy-Shop-Crafting.md) | 单一金币、全局商店、储藏处、配方事务、RPC 安全与 Demo 配置 | 玩法、内容、网络与 UI 开发 |
| [10-12 底部居中 HUD：设计与实现](10-Architecture/10-12-Bottom-HUD-Design.md) | 定稿布局、拥有者快照、Widget Blueprint 与物品接线 | UI、美术、网络开发与验收 |
| [10-13 技能瞄准与范围指示器](10-Architecture/10-13-Skill-Indicators.md) | 输入会话、三层贴花、Action 参数与训练场 | 输入、技能、UI 与验收 |

### 20-Content：技能与扩展

| 文档 | 解决的问题 | 推荐读者 |
| --- | --- | --- |
| [20-01 示例技能](20-Content/20-01-Example-Skills.md) | 七个纵向切片与可玩卓尔游侠霜冻之箭 Demo 的落地方式 | 技能开发与验收 |
| [20-02 M6 技能模板检查表](20-Content/20-02-M6-Skill-Template-Checklist.md) | 技能旁路、身份、时序、清理、中文说明和自动化检查 | 技能开发 |
| [20-03 公共扩展与迁移指南](20-Content/20-03-M8-Public-Extension-Guide.md) | 新技能、DataAsset、蓝图事件和版本迁移入口 | 内容开发与维护者 |

### 30-Tooling：工具与诊断

| 文档 | 解决的问题 | 推荐读者 |
| --- | --- | --- |
| [30-01 UE MCP 开发工作流](30-Tooling/30-01-UE-MCP-Workflow.md) | Editor/资产/蓝图/PIE 的 MCP 操作与验证闭环 | 全体开发者与自动化 Agent |
| [30-02 M7 MCP 诊断配方](30-Tooling/30-02-M7-MCP-Diagnostic-Recipe.md) | 联机、复制、安全 RPC、事件、性能与资产诊断顺序 | 网络、测试与工具开发 |

### Specs：单项交付规格

| 文档 | 解决的问题 | 推荐读者 |
| --- | --- | --- |
| [Intake 模板](00-Project/_intake-template.md) | F0 前整理目标、边界、事实和风险 | 需求提出者、负责人 |
| [Gate 检查表](00-Project/_gate-checklist.md) | F0/F1/F2 与 Push-Ready 六层的逐项检查 | 实现者、审查者 |
| [交付记录模板](00-Project/_delivery-record-template.md) | 固化命令、结果、证据、未执行项和 Reflect | 交付者、维护者 |
| [Spec 模板](Specs/_template.spec.md) | 新功能、修复、迁移和契约变更的可执行规格模板 | 需求提出者、实现者、审查者 |
| [DOC-001 文档体系迁移](Specs/DOC-001-document-system-migration.spec.md) | 本次目录迁移的实际 Spec 与验证边界示例 | 维护者、审查者 |
| [DOC-002 本地文档校验](Specs/DOC-002-local-doc-validation.spec.md) | 文档检查命令、正反例测试、JSON 报告和支持边界 | 维护者、自动化 Agent |
| [DOC-003 按目录统一文档编号](Specs/DOC-003-directory-document-numbering.spec.md) | 目录号、目录内序号、引用同步和编号校验规则 | 维护者、自动化 Agent |
| [DOC-004 Combat 技能开发 Skill](Specs/DOC-004-combat-skill-development.spec.md) | GAS/Combat 技能的专项实现、测试和验收流程 | 技能开发者、自动化 Agent |
| [DOC-005 Combat 功能开发 Skill](Specs/DOC-005-combat-feature-development.spec.md) | 通用 Combat 功能的 F0/F1/F2、Spec、验证和交付流程 | 功能开发者、自动化 Agent |
| [DOC-006 Combat 任务路由与复盘 Skill](Specs/DOC-006-combat-task-router.spec.md) | 主 Skill 选择、证据化自评和受控流程调优 | 需求提出者、开发者、自动化 Agent |
| [DOC-007 远端 HUD 与文档体系合并](Specs/DOC-007-remote-hud-doc-merge.spec.md) | HUD 提交的集成、目录冲突处理与验证来源 | 维护者、审查者 |
| [DEMO-901 卓尔游侠 Demo 流程](Specs/DEMO-901-drow-ranger-flow.spec.md) | 英雄资产迁移、远程普攻、霜冻之箭、AutoCast HUD 与完整验证 | 内容、技能、UI 与验收人员 |

冻结与发布文档位于 `90-History`，只保存当时的决策和验收证据：

| 范围 | 文档 | 用途 |
| --- | --- | --- |
| M0-M7 | [`90-History`](90-History/) 中的 `90-01`–`90-14` 文件 | 保存当时的决策、环境、Gate 命令和用户验收证据 |
| M8 | [90-15 候选发布决策](90-History/90-15-M8-Release-Candidate-Decision.md) | 冻结 v1 发布边界、预测和性能策略 |
| M8 | [90-16 生命周期审计](90-History/90-16-M8-Lifecycle-Audit.md) | Handle、Delegate、Schedule、Runtime 和 Actor 清理契约 |
| 扩展 | [20-03 公共扩展与迁移指南](20-Content/20-03-M8-Public-Extension-Guide.md) | 新技能、DataAsset、蓝图事件和版本迁移入口 |
| M8 | [90-17 候选发布验收](90-History/90-17-M8-Acceptance.md) | 最近一次完整发布 Gate 证据 |

## 建议阅读路径

- 初次了解：根 README → `00-Project/00-01-Progress-Tracker.md` → `10-Architecture/10-01-Scope-Architecture.md` → `20-Content/20-03-M8-Public-Extension-Guide.md`。
- 使用 AI 协作开发：`00-Project/00-05-AI-Native-Development-Workflow.md` → `Specs/_template.spec.md` → `00-Project/00-01-Progress-Tracker.md` → 相关专题 → `00-Project/00-03-Test-Plan.md` → `00-Project/00-04-Decisions-Gaps.md`。
- 实现技能：`10-Architecture/10-03-Ability-Targeting-Blueprint.md` → `10-Architecture/10-05-Damage-Heal.md` → `10-Architecture/10-06-Attack-Projectile-Thinker.md` → `20-Content/20-01-Example-Skills.md` → `20-Content/20-02-M6-Skill-Template-Checklist.md` → `20-Content/20-03-M8-Public-Extension-Guide.md`。
- 修改战斗内核：`10-Architecture/10-01-Scope-Architecture.md` → 对应 `10-Architecture/10-02`–`10-08` → `00-Project/00-04-Decisions-Gaps.md` → `90-History/90-16-M8-Lifecycle-Audit.md` → `00-Project/00-03-Test-Plan.md`。
- 理解当前单位控制与联机：`10-Architecture/10-09-Client-Server-Interaction.md` → `10-Architecture/10-07-Order-Movement.md` → `10-Architecture/10-08-Data-Network-Observability.md` → `30-Tooling/30-02-M7-MCP-Diagnostic-Recipe.md` → `00-Project/00-03-Test-Plan.md`。
- 维护服务器权威移动：`10-Architecture/10-10-Server-Authoritative-Movement-Kickoff.md` → `10-Architecture/10-09-Client-Server-Interaction.md` → `10-Architecture/10-07-Order-Movement.md` → `10-Architecture/10-01-Scope-Architecture.md` → `90-History/90-16-M8-Lifecycle-Audit.md` → `00-Project/00-04-Decisions-Gaps.md` → `00-Project/00-03-Test-Plan.md`。
- 维护头顶界面：`10-Architecture/10-11-Overhead-Blueprint-UI.md` → `10-Architecture/10-08-Data-Network-Observability.md` → `20-Content/20-03-M8-Public-Extension-Guide.md` → `90-History/90-16-M8-Lifecycle-Audit.md`。
- 维护底部 HUD：`10-Architecture/10-12-Bottom-HUD-Design.md` → `10-Architecture/10-14-Item-System.md` → `10-Architecture/10-15-Economy-Shop-Crafting.md` → `10-Architecture/10-08-Data-Network-Observability.md` → `00-Project/00-04-Decisions-Gaps.md` 的 ADR-047。
- 核对发布状态：`00-Project/00-01-Progress-Tracker.md` → `90-History/90-15-M8-Release-Candidate-Decision.md` → `90-History/90-17-M8-Acceptance.md`。

## 原章节迁移

| 原章节 | 新位置 |
| --- | --- |
| 1-3 背景、参考结构、总体架构 | 10-01、10-02、10-08 |
| 4 Ability、12 蓝图替代 Lua | 10-03 |
| 5 Modifier、6 属性 | 10-04 |
| 7 Damage/Heal | 10-05 |
| 8 普攻、9 Projectile、10 Thinker | 10-06 |
| 11 Order/NavMesh | 10-07 |
| 13 网络同步 | 10-08 |
| 14 示例技能 | 20-01 |
| 15 集成步骤 | 00-02 |
| 16 测试 | 00-03 |
| 17 风险、18 落地原则 | 00-04、10-01 |

## 文档约定

- “必须/禁止”表示 v1 不可破坏的系统约束；“建议”表示默认实现，可通过设计决策记录变更。
- `00-Project/00-02-Implementation-Roadmap.md` 中“任务默认未开始”描述的是 2026-08-24 的历史计划基线；当前完成度只读取 `00-Project/00-01-Progress-Tracker.md`。
- `90-History/90-01`–`90-16`、`90-17` 保存冻结决策和验收时点，不因后续实现自然演进而改写历史证据。
- 开放问题统一登记在 00-04，并写明最迟决策节点；不在功能文档中悄悄引入第二套语义。
- 代码、DataAsset、GameplayTag 和网络载荷的命名发生变化时，先更新 `10-Architecture/10-01`、`10-Architecture/10-08`，再更新对应子系统文档与测试矩阵。
- 涉及 Unreal Editor、Content 资产、蓝图和 PIE 的任务优先使用 `30-Tooling/30-01-UE-MCP-Workflow.md` 获取当前状态、执行受控操作并回读验证；最终完成状态仍以源码 diff、编译、Automation 和对应 Gate 为准。
- M0-M8 已完成。post-v1 工作必须建立新的 Task/ADR/验收边界，不能继续复用已关闭里程碑伪装完成度。
