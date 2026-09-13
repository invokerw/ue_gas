---
name: combat-feature-development
description: "在 ue_gas Combat 仓库中执行功能、Bug 修复、兼容新增或契约变更的 AI-Native 交付流程，从 Intake 和 Spec 直到本地验证、交付记录与 Reflect。"
---

# Combat 功能开发任务

当用户请求新增或修改 Combat 功能时使用本 Skill。它适用于 C++、DataAsset、蓝图、关卡、网络、测试和文档变更；纯咨询、只读解释和不涉及本仓库的任务不使用它。

本 Skill 是仓库内的项目流程文件，唯一来源为当前工作区的 `Skills/combat-feature-development/`；不要复制或安装到用户级 Codex Skill 目录。

## 输入与前置条件

- 输入：用户需求、现有工作区、相关 DDD/专题文档，以及必要时用户明确的验收偏好。
- 前置条件：确认工作区根目录包含 `ue_gas.uproject`，完整阅读 `agent.md`，检查 `git status` 并保留已有修改。
- 输出：可回读的 Spec、实现 diff、验证证据、Gate 结论和用户验收状态；没有执行的检查必须标为“未执行”。

每次功能、Bug、工具、资产或流程变更都必须在**第一次行为文件修改前**建立任务 Spec，并把用户请求、附件解释、已读取入口、主 Skill 和路由置信度写入 Spec。纯咨询不创建虚假 Spec。行为文件包括 `Source/`、`Content/`、`Tools/` 及其他会改变运行结果的代码、脚本、蓝图和资产。开工前运行：

```bash
python3 -B Tools/task_gate.py --mode preflight --spec Doc/CombatSystem/Specs/<task-id>.spec.md --kind feature
```

Gate 失败时先补齐 Spec 或路由记录，不能绕过检查进入 BUILD。流程/工具变更使用 `--kind process`，纯文档变更使用 `--kind docs`。

## 工具边界

- 文档和代码定位使用 `rg`、直接阅读源码和现有测试；若存在 `.codegraph/`，优先使用它定位复杂调用链。
- 蓝图、DataAsset、关卡和 PIE 优先使用 UE MCP 的 Read → Plan → Mutate → Verify → Record；不可用时记录降级方式。
- 编译、Automation、PIE、Dedicated 和本地文档校验只按任务风险执行，命令、结果和报告路径写回 Spec 或交付记录。

## 不可替代的项目入口

开始前完整阅读仓库根目录 `agent.md`，再按任务风险读取：

- 当前状态唯一来源：`Doc/CombatSystem/00-Project/00-01-Progress-Tracker.md`
- AI-Native 流程：`Doc/CombatSystem/00-Project/00-05-AI-Native-Development-Workflow.md`
- 范围与架构：`Doc/CombatSystem/10-Architecture/10-01-Scope-Architecture.md`
- 测试准入：`Doc/CombatSystem/00-Project/00-03-Test-Plan.md`
- 决策、缺口和迁移：`Doc/CombatSystem/00-Project/00-04-Decisions-Gaps.md`

涉及技能、移动、生命周期、网络或资产时，按 `agent.md` 的任务表补读对应专题。修改蓝图、DataAsset、关卡或 PIE 时，优先使用 UE MCP 的 Read → Plan → Mutate → Verify → Record；MCP 不可用必须记录降级方式。

## 执行流程

### 1. EVALUATE / F0：框定任务

读取用户请求、当前台账、工作区状态和相关 DDD，明确目标、范围、Non-Goals、风险等级、依赖和需要用户决定的问题。输出 `GO`、`DEFER` 或 `ESCALATE`；没有 `GO` 不进入实现。

### 2. THINK：识别结构风险

至少提出一个实现方案和边界，检查服务器权威、唯一数据源、Handle/generation、exactly-once、权限、迁移、回滚和 blast radius。优先复用公共 Ability、Subsystem、Transaction、View 或 DataDriven Action；若公共入口无法表达，写明原因。

### 3. PLAN / F1：建立 Spec

从 `Doc/CombatSystem/Specs/_template.spec.md` 创建 `Doc/CombatSystem/Specs/<task-id>.spec.md`。补齐 AC、DoD、文件/资产定位、状态转换、正常/失败/取消/过期/死亡/EndPlay/重复请求路径、测试矩阵、迁移、回滚、可观测性和证据位置。小型文字修正可在台账记录范围与验证，但功能、Bug、资产迁移和契约变更必须保留 Spec。F1 结果写为 `APPROVED`、`REVISE` 或 `ESCALATE`。

完成 Spec 后先运行 `python3 -B Tools/task_gate.py --mode plan --spec Doc/CombatSystem/Specs/<task-id>.spec.md --kind <feature|docs|process>`。计划检查通过后逐项审查，并记录 F1 结论、审查人、审查版本和计划审查证据。只有 F1=`APPROVED` 且审查版本等于 Spec 版本，才将状态设为 `BUILDING`，运行 `python3 -B Tools/task_gate.py --mode build --spec Doc/CombatSystem/Specs/<task-id>.spec.md --kind <feature|docs|process>`。

F1 审查通过和 Build Gate 通过前，不得修改代码、测试、工具脚本、蓝图或资产；允许读取、运行已有检查和维护 Spec/计划记录。范围、架构、权限、迁移、测试矩阵或回滚实质变化时先递增 Spec 版本，F1 回到 `REVISE`、任务状态回到 `PLAN_REVIEW`，重新审查后才能继续实现。Gate 不追溯历史时序；不得用补写批准代替事前审查，且应保留已有用户改动。审批权限以 [00-05 §4](../../Doc/CombatSystem/00-Project/00-05-AI-Native-Development-Workflow.md#4-九阶段交付流水线) 为准。

### 4. BUILD：按 TDD 实现

对行为或可执行逻辑，先建立最小失败测试、fixture 或 Golden Case，记录实际 Red 原因；再做最小实现和 Green 验证。纯文档修正使用链接、格式和事实核对，不虚构未执行的 Red 阶段。遵守 `agent.md` 的服务器结算、Scheduler、生命周期、单 Runtime Module、中文注释和蓝图 ToolTip 约束。

### 5. REVIEW / TEST：分层验证

先检查 diff、旁路、公开说明和资产引用，再按风险执行：

- 文档：`python3 -B Tools/validate_docs.py`、过时状态/事实核对、`git diff --check`
- 校验工具：`python3 -B -m unittest discover -s Tools/Tests -p 'test_validate_docs.py' -v`，并运行真实仓库校验
- C++：UE 5.8 Editor 构建及直接相关 `Combat.*`
- 蓝图/资产：编译、保存、回读、`CombatAssetValidation` 和相关 Automation
- 网络/复制：Server/Client Target、Dedicated Server/Client smoke；PIE 不能替代 Dedicated 证据

命令、结果、日志路径和未执行原因写回 Spec 或交付记录。没有运行的检查写“未执行”，不能写“通过”。

### 6. ADVERSARIAL / F2：独立攻击审查

在不依赖 Builder 推理的上下文中检查 Spec 合规、状态机/并发、生命周期、网络安全、性能、可观测性和集成影响。修复与复验最多三轮；仍不收敛时写 Gap Report 并 `ESCALATE`。单人本地流程不要求 PR、GitHub Actions 或外部协作者。

### 7. DELIVER：本地交付门

逐项判断 Push-Ready 六层：Tests、Types/Build、No Regression、Adversarial、DDD/Constraints、Decisions。适用项通过；不适用项标 `N/A` 并说明原因；受阻的必需项标“未执行”。将结果和证据写入 Spec 或 `Doc/CombatSystem/00-Project/_delivery-record-template.md`，不自动提交或推送。

交付前运行与开工时相同任务的机器检查，并把输出写回 Spec：

```bash
python3 -B Tools/task_gate.py --mode delivery --spec Doc/CombatSystem/Specs/<task-id>.spec.md --kind feature
```

`delivery` 会检查 Spec 状态、F1/F2/Push-Ready 结论、验证与未执行记录，以及工作区行为变更是否同时带有测试和文档。没有通过就不能声称任务可交付。

### 8. REFLECT：回写知识

实际 Task、Gate 或验收状态变化才更新 `00-01`；契约、迁移、开放决策或延期更新 `00-04` 和受影响专题；稳定失败固化为测试、Gate、模板或本 Skill。历史 `90-History` 只追加事实，不改写当时结论。里程碑由用户明确验收后才标记为“已验收”。

## 停止和升级条件

遇到 L2 操作（生产、权限、不可逆迁移、发布或版本契约）、同一失败重复、三轮修复仍不收敛、测试互相矛盾、Spec 与代码事实不一致，停止自动推进并说明：缺口、已尝试步骤、证据、建议选项和需要用户决定的问题。保留用户已有修改，禁止覆盖、回退无关文件或写入生成目录。

## 交付输出

最终说明必须包含：变更文件/资产、行为变化、F0/F1/F2 和 Push-Ready 结果、实际验证命令与证据、未执行项及原因、剩余风险、用户验收状态和下一步。若任务只改变文档或流程，明确说明 UE 编译、Automation、PIE、Dedicated 是否不适用。
