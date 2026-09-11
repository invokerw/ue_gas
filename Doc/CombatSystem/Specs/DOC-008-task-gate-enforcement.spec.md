# DOC-008 任务流程 Gate 强化

> Spec 版本：`0.1`
> 状态：`READY_FOR_REVIEW`
> Owner：Codex
> 创建日期：2026-09-11
> 风险等级：`L1`

## 1. 目标与范围

### 目标

把 `agent.md`、项目 Skill、Spec、进度台账和验证证据串成可执行的 preflight/delivery 检查，避免实现先于 Spec、遗漏 Skill 路由或把未执行验证写成通过。

### 范围

- 新增 `Tools/task_gate.py`，提供 `preflight` 和 `delivery` 两种检查模式。
- 扩展 Spec 模板、功能 Skill、`agent.md` 和 AI-Native 流程，明确开工记录、用户请求与附件解释、Spec 时序和交付证据。
- 同步 Intake/Gate 清单和 README 命令模板，让人工入口与机器检查使用同一套字段和命令。
- 为 Gate 增加独立正向/拒绝路径单测，并将本任务写入进度台账。

### Non-Goals

不改变 Combat 运行时、发布契约、用户验收权限或既有历史验收结论；不强制使用 GitHub Actions、远端服务或外部插件。

## 2. 当前事实与依据

- `agent.md` 已要求先读文档、路由 Skill、建立 Spec 和执行验证，但缺少机器可失败的任务级检查。
- `Skills/combat-feature-development/SKILL.md` 已描述 F0/F1/F2，但没有统一的首条开工记录和命令化 Gate。
- `Tools/validate_docs.py` 只校验文档结构、链接和 Spec 基本章节，不读取 Git 变更范围或验证证据。
- 本次用户反馈确认：上一任务虽然实际读取了规则，但没有显式汇报，且 Spec 建立晚于实现。

### F0 结论

- 用户请求：修复此前开发流程未显式展示 `agent.md`、dev Skill 和 Spec 证据的问题，并把规则落成可执行检查。
- 附件解释：无附件；用户此前附带的 HUD 截图属于上一项功能参考，不属于本流程修复的输入。
- 主 Skill：`skill-creator`（维护仓库 Skill）+ `combat-feature-development`（流程功能变更）
- 路由置信度：`high`
- F0 结论：`GO`
- F0 依据：本任务只修改流程文档、Skill、检查脚本和单测，不改变运行时或发布契约。

## 3. 行为与契约

### Preflight

`task_gate.py --mode preflight --spec <path>` 必须确认 Spec 位于仓库 `Doc/CombatSystem/Specs/`、具备目标/依据/验收/测试/交付章节、记录主 Skill/路由置信度/F0 结论，并且状态处于可开工状态。脚本只读 Git 状态，不修改文件。

### Delivery

`task_gate.py --mode delivery --spec <path>` 必须确认 Spec 已记录 F0/F1/F2、Push-Ready、实际验证和未执行项；若检测到源码/资产/运行时代码变更，则要求 Spec 非模板且存在测试计划、交付证据和对应路径。脚本输出可机器读取的 JSON 报告并以非零状态拒绝不完整交付。

### 失败与例外

缺 Spec、状态错误、缺 Skill 路由、缺验收或缺验证记录均返回稳定错误码。纯文档小修可通过明确任务类型跳过运行时测试，但仍需文档校验和差异检查；环境阻塞只能写为“未执行”，不能转换为通过。

## 4. 验收标准（AC）

- [x] AC-01：有效的功能 Spec 能通过 preflight；缺 Spec、缺路由或缺 F0 记录时被拒绝。
- [x] AC-02：delivery 能识别源码/资产变更，并拒绝没有测试/证据/未执行项的交付。
- [x] AC-03：纯 Markdown 任务可在声明类型后通过适用 Gate，不要求 UE 构建。
- [x] AC-04：脚本报告包含稳定 schema、错误码、文件和行号，并且不修改工作区。
- [x] AC-05：后续开工消息模板明确列出已读取的 `agent.md`、Skill、DDD、Spec 和附件解释。

## 5. Definition of Done

- [x] `Tools/task_gate.py`、单测、Spec/Intake/Gate 模板、Skill、`agent.md`、AI-Native 流程和 README 已同步。
- [x] `python -B Tools/validate_docs.py`、Gate 单测和 `git diff --check` 通过。
- [x] 以当前任务运行一次 preflight 和 delivery，记录结果与任何有意的例外。
- [x] 未改变 Combat 运行时代码、历史验收记录或用户验收权限。

## 6. 测试矩阵与命令

| 层级 | 命令/操作 | 预期 |
| --- | --- | --- |
| Unit | `python -B -m unittest discover -s Tools/Tests -p 'test_task_gate.py' -v` | 有效/拒绝/只读路径通过 |
| 文档 | `python -B Tools/validate_docs.py` | 本地文档与 Spec 结构通过 |
| 差异 | `git diff --check` | 无空白错误 |
| Runtime | UE 构建、Combat Automation、PIE、Dedicated | N/A：本任务不修改运行时；不执行并记录原因 |

## 7. 交付证据

- F1 结论：`APPROVED`。Spec 已冻结范围、输入输出、拒绝路径、测试和回滚方式。
- F2 结论：`PASS`。独立检查覆盖缺 Spec、缺路由/F0、缺测试/文档、纯文档类型和只读行为。
- Push-Ready 结论：`READY`。六层门中流程/文档/工具适用项有证据，UE Runtime 项明确 N/A。
- 验证：`D:\tools\Python313\python.exe -B -m unittest discover -s Tools/Tests -p 'test_task_gate.py' -v`（8/8 PASS）；`D:\tools\Python313\python.exe -B -m unittest discover -s Tools/Tests -p 'test_validate_docs.py' -v`（16/16 PASS）；`python -B Tools/validate_docs.py`（PASS）；`git diff --check`（PASS）；`python -B Tools/task_gate.py --mode preflight --spec Doc/CombatSystem/Specs/DOC-008-task-gate-enforcement.spec.md --kind process`（PASS）；`python -B Tools/task_gate.py --mode delivery --spec Doc/CombatSystem/Specs/DOC-008-task-gate-enforcement.spec.md --kind process --report Saved/TaskGate/DOC-008-delivery.json`（PASS，34 个变更文件）。
- 未执行：UE 5.8 Editor 构建、Combat Automation、PIE、Dedicated 和 Soak 未执行，原因是本任务不修改 Combat Runtime、资产或网络行为；后续运行时任务仍按风险矩阵执行。
- 代码/文档证据：`Tools/task_gate.py`、`Tools/Tests/test_task_gate.py`、本 Spec、Spec/Intake/Gate 模板、`agent.md`、任务 Skill、路由 Skill、AI-Native 流程和 `README.md`。

## 8. 兼容、回滚与 Reflect

- 兼容：脚本默认只读取工作区，现有文档和运行时 API 不变；旧 Spec 可通过补充流程记录后交付。
- 回滚：移除 `task_gate.py`、新增单测和模板/规则增量即可恢复原流程。
- 若 Gate 与现有小型文档修正规则冲突，记录例外而不扩大脚本范围。

## 9. 路由、自评与 Reflect

- 原始需求摘要：让每次开发显式遵循 `agent.md`、dev Skill 和 Spec，并在开工/交付前有可失败检查。
- 主 Skill：`skill-creator` + `combat-feature-development`；备选 `combat-task-router` 负责路由记录，未单独作为实现 Skill。
- 路由置信度：`high`；任务修改仓库 Skill、流程文档和校验工具，不涉及运行时玩法。

| 维度 | 权重 | 分数（0–5） | 证据与扣分原因 |
| --- | ---: | ---: | --- |
| 需求与 AC | 20% | 5 | 5 项 AC 均完成并由单测/命令验证 |
| 架构与权限 | 20% | 5 | 不改变 Runtime、发布契约或用户验收权限 |
| 实现与数据 | 20% | 4 | Gate 覆盖 Spec、变更范围、测试和文档；未接入 CI 属于明确 Non-Goal |
| 验证证据 | 20% | 5 | 8 个 Gate 单测、16 个文档单测、文档校验、preflight/delivery 和 diff check 均通过 |
| 文档与可观测性 | 10% | 5 | 模板、Intake、Gate 清单、Skill、规则和 README 已同步 |
| 交付卫生 | 10% | 4 | 当前工作区还包含上一任务的未提交变更，delivery 已完整列出 34 个文件并通过配套检查 |

- 计算总分：`4.8 / 5.0`
- 硬性封顶或未执行项：Runtime 构建/Automation/PIE/Dedicated/Soak 对本流程任务不适用；已按规则记录未执行原因。
- 自评结论：`EVIDENCE_SUFFICIENT`
- 用户验收状态：`READY_FOR_REVIEW`

### Reflect

- 观察与证据：上一任务规则虽被读取，但 Spec 建立晚于实现，且开工消息没有公开读取入口；本次用 `task_gate.py` 将遗漏变成稳定错误码。
- 根因类别：`流程`
- 调整文件与预期收益：模板、Intake、Gate 清单、`agent.md`、两个 Skill、AI-Native 流程、README 和 Gate 单测；后续任务必须先通过 preflight，交付必须带验证证据和配套测试/文档。
- 回归验证：`Tools/Tests/test_task_gate.py` 8/8、`test_validate_docs.py` 16/16、文档校验和 DOC-008 preflight/delivery 均通过。
- 需要用户决定的问题：无。
