# 00-05 AI-Native 开发流程与文档体系

> 本文把“Human Directs, AI Delivers”落到当前 `ue_gas` Combat 项目。它规定需求如何进入仓库、如何形成可执行规格、如何验证和交付；不替代 `00-01` 的实时状态、`10-01` 的架构约束或 `00-03` 的测试细则。

## 1. 适用范围与当前基线

本流程适用于 Combat 的 C++、DataAsset、蓝图、关卡、网络、测试和文档变更。当前工程基线如下：

- Unreal Engine 5.8，Combat 保持在 `Combat` 单 Runtime Module；项目文件和 Target 名称仍保留 `ue_gas`。
- 核心契约为 `combat_v1_rc1`；服务器负责战斗结算，客户端请求必须由服务器复核。
- M0–M8 已完成并通过用户验收；Post-M8 的 SAM（服务器权威单位移动）已完成用户验收。
- 最新完成度、测试数量和证据只读取 [00-01 开发进度台账](00-01-Progress-Tracker.md)，历史验收文档不用于推断当前状态。

流程的目标不是让 Agent 自主改变契约，而是让每次变更都能回答：依据是什么、改了什么、如何证明、哪些决定仍由人负责。

当前采用本地开发与用户验收流程：整理需求 → Spec → 实现 → 本地审查和验证 → 交付记录 → 用户验收。PR 和 GitHub Actions 不作为准入条件。下文的 Push-Ready 表示本地交付就绪；提交、推送和发布遵循 `agent.md` 中的授权规则。

## 2. 文档分层：DDD、SDD、代码事实

### 2.1 L1：领域判断层（DDD）

项目已有文档承担四类长期记忆，不再另建一套平行的 `PRODUCT.md` 等副本：

| 判断类型 | 当前入口 | 维护时机 |
| --- | --- | --- |
| 产品目标、范围、Non-Goals | `README.md`、`10-Architecture/10-01-Scope-Architecture.md` §1 | 范围或成功标准变化 |
| 技术架构与不可破坏约束 | `10-Architecture/10-01`–`10-08`、`90-History/90-16`、`10-Architecture/10-09`–`10-12`、`agent.md` | 架构、权限、生命周期或公开 API 变化 |
| 失败经验、开放决策与迁移 | `00-Project/00-04-Decisions-Gaps.md`、`90-History` 验收报告和 `00-Project/00-01` 更新日志 | Gate 失败、用户纠正、回归或延期 |
| 当前项目状态、依赖和证据 | `00-Project/00-01-Progress-Tracker.md` | Task、Gate、验收或证据变化 |

L1 记录判断及其来源。代码位置、日志、Automation 报告和 Golden Case 应使用 `file:line`、路径或明确报告名锚定；只贴链接而不写结论不算完成。

### 2.2 L2：交付规格层（SDD）

每个新功能、Bug 修复、兼容新增、资产迁移或契约变更都建立一份 `Doc/CombatSystem/Specs/<task-id>.spec.md`。开始前使用 [`Intake 模板`](_intake-template.md)，规格模板见 [`Specs/_template.spec.md`](../Specs/_template.spec.md)；交付时使用 [`Gate 检查表`](_gate-checklist.md) 和 [`交付记录模板`](_delivery-record-template.md)。

Intake、Gate 结论和交付证据可直接写入同一份 Spec，模板按需取用。小型链接、错字和说明修正可在台账记录范围与验证；功能、迁移或契约变更仍保留完整 Spec。

Spec 是人和 Agent 共同使用的交付契约，至少写明：

- 目标、范围和 Non-Goals；
- 用户流/业务流、状态转换和服务器/客户端边界；
- 输入、输出、DefinitionId、GameplayTag、事件和数据约束；
- 正常、取消、过期、死亡、EndPlay、重复请求和旧 generation 路径；
- 受影响的领域、文件、资产、依赖、迁移和回滚；
- Acceptance Criteria（AC）、Definition of Done（DoD）和可执行测试；
- 风险、权限、可观测性、需要人决定的选项和证据位置。

Spec 发生语义变化时先递增版本并写原因，再改代码。若只是修正文档事实，注明“不改变运行时语义”。

### 2.3 L3：代码事实层

代码事实只描述当前可验证实现，不承载产品判断。仓库存在 `.codegraph/` 时，定位复杂调用链优先使用它；当前工作区没有该索引，因此使用 `rg`、源码和测试直接定位。复杂变更必须在 Spec 或交付记录中列出关键 `file:line`、调用流和 blast radius；未来建立索引后再自动化生成。

## 3. 任务上下文装载

`agent.md` 是入口装配规则，任务只加载与风险相称的最小上下文：

| 任务 | 必读入口 | 追加材料 |
| --- | --- | --- |
| 文档/流程 | `README`、`00-Project/00-01`、本文 | 受影响专题 |
| Ability/技能/公开扩展 | `10-Architecture/10-01`、`10-03`、`10-05`、`20-Content/20-01`、`20-02`、`20-03` | 相关 DataAsset、测试与 `00-Project/00-04` |
| Scheduler/Modifier/Damage/生命周期 | `10-Architecture/10-01`、`10-02`、`10-04`、`10-05`、`90-History/90-16` | `00-Project/00-03` 和相关 ADR |
| Order/移动/控制/碰撞 | `10-Architecture/10-01`、`10-07`、`10-09`、`10-10` | `90-History/90-16`、`00-Project/00-03`、UE MCP 工作流 |
| 网络、复制、Projectile、UI | `10-Architecture/10-01`、`10-06`、`10-08`、`10-09`、`10-11`；底部 HUD 追加 `10-12` | `90-History/90-13`–`90-14`、`00-Project/00-03` |
| 资产、蓝图、关卡、PIE | `10-Architecture/10-01`、`10-08`、`30-Tooling/30-01` | 目标资产、`00-Project/00-03`、对应验收报告 |

加载完后先读 `00-01` 当前状态，再确认工作区 diff，保留用户已有修改。

## 4. 九阶段交付流水线

本文的流程门记作 **F0/F1/F2**，分别对应 framing、plan 和 adversarial review；现有路线图中的 **G0–G8** 仍是 Combat 里程碑 Gate，二者不能互相替代。一个变更要进入用户验收，既要通过适用的 F 门，也要满足所属 G 门和 `00-01` 的状态规则。

```text
EVALUATE → THINK → PLAN → BUILD → REVIEW → TEST → ADVERSARIAL → DELIVER → REFLECT
```

每次需要修改仓库的任务，在第一次行为文件修改前发送一条开工记录，明确：已读取的入口文件、用户请求、附件解释（需求/参考/工程约束）、主 Skill、备选 Skill、路由置信度、Spec 路径和 F0 结论。行为文件包括 `Source/`、`Content/`、`Tools/` 以及会改变运行结果的其他代码、脚本、蓝图和资产。随后运行可失败的机器 Gate：

```bash
python3 -B Tools/task_gate.py --mode preflight --spec Doc/CombatSystem/Specs/<task-id>.spec.md --kind <feature|docs|process>
```

进入计划审查时运行：

```bash
python3 -B Tools/task_gate.py --mode plan --spec Doc/CombatSystem/Specs/<task-id>.spec.md --kind <feature|docs|process>
```

`plan` 检查 F0、Spec 状态和计划章节；通过只表示可以开展计划审查。审查人逐项确认范围、AC、依赖、测试、回滚和风险后，在 Spec 中记录 F1 结论、审查人、审查版本和计划审查证据。只有结论为 `APPROVED` 才将状态切换为 `BUILDING`。在此之前可读取代码、运行已有检查、维护 Spec 和计划记录，不得修改代码、测试、工具脚本、蓝图、DataAsset、关卡或其他行为文件。

每个 `F0/F1/F2/Push-Ready 结论` 字段只写一个当前枚举值，原因放入独立证据字段；模板候选列表、重复字段和历史正文不能代替当前结论。继续旧任务时补齐当前审查人、版本和证据，不批量重写历史验收记录。

F1 通过后、第一次行为文件修改前运行：

```bash
python3 -B Tools/task_gate.py --mode build --spec Doc/CombatSystem/Specs/<task-id>.spec.md --kind <feature|docs|process>
```

`build` 必须看到 F0=`GO`、F1=`APPROVED`、可构建状态和实际审查记录，且 F1 审查版本必须等于当前 Spec 版本。范围、架构、权限、迁移、测试矩阵或回滚方案发生实质变化时，先递增 Spec 版本，将 F1 改为 `REVISE`、任务状态改为 `PLAN_REVIEW`，完成重审后才能继续对应实现。不得补写批准来追认未审查的代码变更。

这些检查核对当前记录，不能追溯证明修改先后，也不能自动判断 Spec 语义变化。开工时应记录已有差异，保留用户修改；已有差异和重审前已授权的实现不等于本任务违反计划顺序。F1 是计划审查结论，L0/L1 可由 Agent 在既有授权内审查并记录；用户明确要求人审或涉及 L2 时必须等待对应决定。

实现和验证完成后运行：

```bash
python3 -B Tools/task_gate.py --mode delivery --spec Doc/CombatSystem/Specs/<task-id>.spec.md --kind <feature|docs|process>
```

这些 Gate 都是只读检查（可选 `--report` 写 JSON），会验证 Spec 状态、路由和 F0、F1/F2、Push-Ready、实际验证/未执行记录，以及行为变更的测试和文档配套。失败时先修正 Spec、测试或证据，再继续；不能以口头说明代替失败结果。

### EVALUATE：F0，判断是否值得做

读取用户需求、L1、当前状态和依赖，确认问题边界、Non-Goals、风险等级和是否需要先补领域知识。输出 `GO`、`DEFER` 或需要人决定的选项。未通过不进入编码；`DEFER` 必须说明缺口和最小澄清动作。

### THINK：形成方案与风险探针

至少列出一个可行方案及其边界；对服务器权威、唯一数据源、Handle/generation、事件 exactly-once、迁移和回滚做反向提问。若需求能被现有公共入口表达，不为单个技能增加专用结算旁路。

### PLAN：F1，冻结可执行 Spec

补齐 Spec、AC、DoD、文件定位、依赖图、测试矩阵、回滚和证据位置。计划审查要回答：结构问题是否被误当成局部补丁、blast radius 是否可控、旧资产/旧回调如何失效、失败后如何恢复。F1 结果为 `APPROVED`、`REVISE` 或 `ESCALATE`。F1=`APPROVED` 是 BUILD 的硬解锁；没有该结论，任何代码、工具脚本、资产、蓝图或行为配置都不能修改。

计划批准只覆盖 Spec 中冻结的范围。若实现前发现范围、架构、权限、迁移、测试矩阵或回滚方案发生实质变化，先将 F1 改为 `REVISE`、任务状态改为 `PLAN_REVIEW`，更新 Spec 并重新审查。

### BUILD：TDD Red → Green → Verify

先从 AC 生成最小失败测试或 Golden Case，并确认失败原因；再写让测试通过的最小实现；随后做静态检查、回归和必要的 Editor/PIE/Dedicated 验证。测试通过不代表 Spec 合规，必须同时检查权限、日志、清理和公开说明。

纯文档修正用链接、格式和事实核对形成证据，无需为文字调整新建单元测试；校验工具等可执行逻辑变化应覆盖有效输入与拒绝路径。已有实现的补测应如实记录，不能补写未执行过的 Red 阶段。

### REVIEW 与 TEST：先局部，再全局

Review 检查 diff、约束、注释、资产引用和旁路模式。Test 按风险选择 `Pure/Unit → World Automation → PIE → Network/Dedicated → Soak/Perf`，不把 PIE 诊断当成 Dedicated 证据。所有时间敏感用例使用可控时间；flaky 用例按失败处理。

### ADVERSARIAL：F2，独立上下文攻击实现

审查者只看 Spec、diff、必要的仓库事实和测试结果，不依赖 Builder 的推理。至少检查契约合规、并发/状态机、生命周期、网络安全、性能、可观测性和集成影响。发现问题后最多进行三轮“修复 → 复验”；仍不收敛则输出 Gap Report 并升级。

### DELIVER：Push-Ready 六层门

逐层判断以下六项：适用项通过，不适用项标注 `N/A` 并写明原因，变更才可进入本地交付或用户验收。因环境阻塞而没有执行的必需项记为“未执行”，不能标成 `N/A`：

| 层 | 当前项目证据 |
| --- | --- |
| L1 Tests | 文档运行本地校验；工具运行对应测试；运行时运行直接相关 `Combat.*`、失败路径和 Golden Case |
| L2 Types/Build | C++ 变更构建 UE 5.8 Editor；若涉及网络则加 Server/Client Target；纯文档标为不适用 |
| L3 No Regression | 全量或风险相称的 `Combat.*`、资产和蓝图检查 |
| L4 Adversarial | F2 findings 已关闭或明确升级 |
| L5 DDD/Constraints | `10-01`、`agent.md`、专题文档、中文 ToolTip/注释和旁路扫描一致 |
| L6 Decisions | `00-04` 的 ADR/Gap、版本、迁移和延期记录齐全 |

交付记录必须列出实际执行的命令、结果、日志/报告路径、未执行项和剩余风险。没有执行的验证写“未执行”，不写“通过”。

文档类变更在交付前运行 `python3 -B Tools/validate_docs.py` 和 `git diff --check`。脚本检查必需入口、迁移后的目录、Markdown 本地目标路径、旧平铺路径、尾随空格和 Spec 格式；页内锚点、外部链接和文档事实需另行审查。报告可保存到 `Saved/DocValidation/report.json`，命令和脚本测试见 [根 README](../../../README.md#验证命令模板)。本地文档 Gate 的检查范围与限制见 [DOC-002](../Specs/DOC-002-local-doc-validation.spec.md)。

### REFLECT：把结果写回知识

更新 `00-01` 进度和证据；契约、公开 API、版本或迁移变化更新 `00-04` 及受影响专题；重复失败写入测试、Gate 或模板；用户纠正写入规则和 Golden Case。历史 M0–M8 验收结论只追加事实，不重写。

任务路由和交付自评由 [Combat 任务路由 Skill](../../../Skills/combat-task-router/SKILL.md) 统一编排。每份任务 Spec 使用模板中的“路由、自评与 Reflect”段落记录主 Skill、备选、置信度、六维 0–5 分证据和调优动作。自评分不能替代用户验收；必需验证未执行或 F2 高风险发现未关闭时，按路由 Skill 的硬性封顶规则处理。

调优遵循“证据先于规则”：单次实现问题只修当前任务；相同遗漏至少在两个独立任务出现后，才更新模板、Gate、校验器或流程；路由误选或专项遗漏再更新任务路由 Skill；领域契约变化必须先进入 `00-04` 和专题文档。每个任务最多一轮受控调优，调优后重新评分并记录回归证据，避免自我修改形成无界循环。

## 5. 状态、权限与升级

流水线状态建议使用 `DEFERRED → PLANNED → PLAN_REVIEW → APPROVED → BUILDING → VERIFYING → READY`，并允许 `REVISE`、`BLOCKED`、`ESCALATED`。状态变化都要引用 Spec、测试或日志证据；项目里程碑仍遵循 `00-01` 的“待验收/已验收”规则。

按风险分配自主程度：

- **L0**：读取、分析、测试和可回滚的文档变更，Agent 可直接执行。
- **L1**：源码、蓝图、DataAsset 和可回滚迁移；关键契约、范围和公开行为由人审阅。
- **L2**：发布、权限、生产数据、不可逆迁移或版本契约变更，必须暂停等待人决定。

出现同一失败重复、三轮仍不收敛、测试互相矛盾、blast radius 超过 Spec、或代码事实与 Spec 不一致时升级。升级包包含缺口、已尝试步骤、证据、建议选项和需要人决定的问题。

## 6. UE/GAS 专项规则

- Damage、Heal、Order、Attack Finalize、Projectile Hit、Death 只在服务器结算；客户端 TargetData、预测 Projectile 只能作为请求或可丢弃表现。
- 前摇、引导、DOT/HOT、Modifier Think/Expire、Aura reconcile、追击复核和 Thinker pulse 使用 `UCombatSchedulerSubsystem`。
- 每个异步对象写清创建者、持有者、结束入口、旧回调失效方式以及 Actor/World teardown 清理。
- DataAsset/蓝图任务遵循 [30-01 UE MCP 工作流](../30-Tooling/30-01-UE-MCP-Workflow.md) 的 Read → Plan → Mutate → Verify → Record；MCP 调用成功不等于资产验证通过。
- 新技能优先使用 DataDriven Action 和公共 Subsystem；派生自定义 Ability/Runtime 必须在 Spec 写明公共表达不足的原因。

## 7. Skill 设计与任务路由

Skill 是可复用的流程契约，不是把上下文切碎后互相转发的多个黑盒 Agent。每个 Skill 至少说明 `purpose`、`inputs`、`preconditions`、`procedure`、`tools`、`outputs`、`checks` 和 `escalation`；结果必须能写回 Spec 或 L1。当前项目可按需采用以下角色：

仓库已有一个任务路由 Skill：[Skills/combat-task-router/SKILL.md](../../../Skills/combat-task-router/SKILL.md)。它根据用户的动作目标选择一个主执行 Skill，记录路由依据和置信度，并在交付后执行证据化自评与受控调优。纯咨询不调用实现 Skill；实现技能本身优先走专项 Skill；Skill 维护走 `skill-creator`。这些 Skill 只保留在本仓库 `Skills/`，不安装到用户级目录。

仓库已有一个可直接执行的功能任务 Skill：[Skills/combat-feature-development/SKILL.md](../../../Skills/combat-feature-development/SKILL.md)。它负责把本文九阶段流程落到具体 Combat 任务；其他 Skill 名称仍是职责角色，不表示已经安装外部插件。

实现 GAS/Combat 技能时，在通用 Skill 之上使用 [Skills/combat-skill-development/SKILL.md](../../../Skills/combat-skill-development/SKILL.md)，由该 Skill 负责 Ability、DataAsset、Modifier、Projectile、Thinker、蓝图和技能专项验收。

路由与自评的完整规则、硬性封顶和调优分类以任务路由 Skill 为准；不要在具体功能 Skill 中复制另一套评分标准。

| Skill | 负责内容 | 产物 |
| --- | --- | --- |
| Task Router | 按动作目标选择主 Skill、记录置信度、交付自评和受控调优 | 路由记录 / 自评 / Reflect |
| Intake/Evaluate | 需求完整度、范围、F0 | intake report |
| Repo Understand | DDD、代码事实、依赖和 blast radius | repo map |
| Spec Author | AC、DoD、迁移和测试契约 | `*.spec.md` |
| Test First | Red 测试、fixture、Golden Case | test plan / failing test |
| Build/Verify | 最小实现、编译、Automation、PIE/Dedicated | diff / verification report |
| Adversarial/Deliver | F2、六层 Push-Ready 和升级 | findings / gate decision |
| Reflect | 决策、失败和用户纠正写回 | DDD、测试或模板更新 |

这些名称是本项目的流程角色，不表示安装了外部插件或用户级 Codex Skill；项目级说明只从仓库 `Skills/` 读取，具体执行工具以当前会话和 `agent.md` 为准。

## 8. 度量与反馈

以交付结果衡量 AI 协作。建议从 `00-01` 的证据开始记录：

- 交付指标：Intake 到验收的 P90 周期、每周可交付变更数、AI 自动完成比例、缺陷率和系统健康度。
- 流程指标：F0/F1/F2 失败率、首次通过率、平均收敛轮数、升级率、重复缺陷率、Golden Case 通过率和 DDD 新鲜度。
- Combat 专项：`Combat.*` 通过数、资产校验、蓝图编译、Dedicated 连接/容量、teardown 和性能 p95/p99；每项都记录报告路径和执行日期。

指标用于发现流程退化，不能替代 AC、用户验收或发布契约。

## 9. 分阶段落地

1. **Spec 纪律**：所有新任务使用模板，先落地 F0/F1；`00-01`、`00-04` 和 `00-03` 的证据位置在 Spec 中固定下来。
2. **自主试点**：为一个边界清晰、可回滚的 Post-M8 任务执行完整九阶段流程，加入 F2、三轮收敛上限和 Reflect。
3. **Eval 复利**：把稳定失败固化为 Automation、资产检查、Gate 或模板；再考虑 Golden Set、定时评估和 `.codegraph` 自动索引。

SAM 已完成用户验收；后续工作应创建新的 Post-M8 Spec/Task，不得把本流程的通过状态写回历史 M0–M8 验收。

## 10. 一个最小交付包

```text
需求 / Intake
  → F0 GO
  → Think + Plan
  → Spec + AC + DoD
  → F1 APPROVED
  → TDD Red
  → Build / Green
  → Verify（测试、构建、回归、资产）
  → F2 对抗审查
  → Push-Ready 六层
  → 按风险进行人审阅/验收
  → Reflect（00-01 / 00-04 / 专题 / 测试 / Golden Case）
```

判断标准是下一次同类工作能否更快、更准、少重复犯错，并且每个结论都有可回读的依据和证据。
