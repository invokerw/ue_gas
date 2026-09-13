# DOC-009 PLAN 计划审查前禁止修改代码

> Spec 版本：`0.2`
> 状态：`已验收`
> Owner：Codex
> 创建日期：2026-09-12
> 关联进度台账：[开发进度台账](../00-Project/00-01-Progress-Tracker.md)
> 风险等级：`L1`

## 0. Intake 与 Gate 记录

- 用户请求：优化技能以及文档，在 PLAN 通过计划审查之前不得修改代码。
- 附件解释：无附件；用户请求定义本次流程约束，不涉及具体玩法。
- 已读取入口：`agent.md`、`README.md`、`00-01`、`00-05`、`10-01`、三个项目 Skill、`skill-creator`、Spec/Intake/Gate 模板、`Tools/task_gate.py` 和 DOC-008。
- 主 Skill：`skill-creator`
- 备选 Skill 与排除理由：`combat-feature-development` 提供项目流程，`combat-task-router` 提供路由与复盘；`combat-skill-development` 是待更新的项目入口，本任务不实现 GAS 技能。
- 路由置信度：`high`
- F0 结论：`GO`
- F0 依据：维护项目 Skill、文档、任务 Gate 及其测试；不改变 Runtime、资产或发布契约。
- F1 结论：`APPROVED`
- F1 审查人：Codex，本地计划审查，按 Spec 和 Gate 检查表复核。
- F1 审查版本：`0.2`
- F1 计划审查证据：0.1 在首次工具/测试代码修改前完成范围、AC、依赖、验证和回滚审查；0.2 在调整 Gate 实现前再次审查，确认计划检查不从 Git 差异推断时序，以当前结论及批准版本控制 BUILD，保留用户已有差异。两次审查均在对应代码修改前记录。
- Build 解锁：已解锁。本次先按原流程完成 F1；新增 build 命令在实现后用于功能复验，结果见 `Saved/TaskGate/DOC-009/build.json`，不作为此前执行顺序的追溯证明。
- F2 结论：`PASS`
- F2 证据：独立上下文只读审查并复验，两个 findings 均关闭；21 项单测、13 个独立临时 Spec 场景通过。审查记录见 `Saved/TaskGate/DOC-009/f2-review.md`。
- Push-Ready 结论：`READY`
- 验证：工具测试、三个 Skill 官方格式校验、文档校验、Build Gate、PLAN CLI 临时 fixture 和差异检查通过；最终 delivery 命令和报告见 §7、§9。
- 未执行：UE 构建、Combat Automation、PIE、Dedicated 和 Soak 均不适用，本任务没有 Runtime/资产/网络修改。首次实现未记录测试先于实现的 Red，不补写虚假 TDD 过程；后续空审查证据拒绝测试实际失败后完成修复并通过。

## 1. 目标与范围

### 目标

使所有项目开发入口明确执行：完成 PLAN、实际审查当前 Spec 并记录 F1 批准后，才能编写测试或修改实现。机器检查拒绝无当前批准、无审查证据或已过期的批准。

### 范围

- 三个项目 Skill、`agent.md`、README 和 AI-Native 流程。
- Spec、Intake、Gate、交付记录模板及进度台账。
- `Tools/task_gate.py` 的 plan/build 模式、当前结论和审查版本检查，及相应单测。

### Non-Goals

不修改 `Source/`、`Content/`、Combat Runtime、发布契约、用户验收权限或历史验收结论；不创建 PR 或推送；不新增文件系统写入拦截器或 Git 历史审计系统。用户于 2026-09-12 验收通过并明确授权本地提交。

## 2. 当前事实与依据

- [DOC-008](DOC-008-task-gate-enforcement.spec.md) 已建立 preflight/delivery，但没有独立的 PLAN 检查与 BUILD 准入。
- 通用功能 Skill 原本要求 F1 通过后再修改代码，专项技能步骤和其他入口未完全统一。
- 原 `conclusion()` 在整个 Spec 查找历史批准，候选列表也可能被匹配；空字段解析可能越过换行读取下一条记录。
- Git LFS 读取差异时受 `.git/lfs/tmp` 写权限限制；使用仅本次命令生效的临时 LFS 缓存后回读成功，差异仅包含本任务文件。没有改 Git 配置或资产。

## 3. 行为与契约

### 主流程与状态转换

`preflight → THINK/PLAN → plan 检查 → F1 实际审查 → APPROVED/BUILDING → build 检查 → 实现与验证 → F2 → delivery`

PLAN 和 F1 通过前，只读取、运行已有检查和维护 Spec/计划记录；不得修改代码、测试、工具脚本、蓝图、DataAsset、关卡或其他行为文件。

实质修改范围、架构、权限、迁移、测试矩阵或回滚方案时，先递增 Spec 版本，F1 设为 `REVISE`、状态退回 `PLAN_REVIEW`；重审批准当前版本后才可继续对应代码修改。不得用补写批准追认未审查的修改。

### 输入、输出与数据约束

- `preflight` 检查任务入口；`plan` 检查 F0 GO、计划状态和计划章节，不替代实际计划审查。
- `build` 要求 F0 GO、唯一当前 F1 APPROVED、可构建状态、审查人/证据和匹配的审查版本。
- `delivery` 同样拒绝缺失或过期的批准，保留原有测试/文档配套检查。
- 当前结论必须使用唯一的 `F0/F1/F2/Push-Ready 结论` 字段和单个精确枚举值；模板候选、重复结论和历史正文不能替代当前字段。
- 稳定错误码包括 `f0_not_go`、`missing_plan_section`、`invalid_plan_status`、`invalid_build_status`、`missing_f1_for_build`、`missing_plan_review`、`stale_plan_approval`。

### 权限、失败与兼容

- L0/L1 在既有授权内可由 Agent 审查；用户要求人审或 L2 操作仍等待对应决定。
- 检查记录不能证明历史操作顺序，也不能自动识别 Spec 语义变化；时序由工作记录和执行纪律保证。
- PLAN/BUILD 不将用户已有修改或重审前已授权的实现判为违规，不修改这些文件。
- 继续旧任务时补齐当前结论和审查记录；不批量重写已归档 Spec。
- 既有 delivery 仍检查全工作区，用户原有行为差异可能使 docs 任务失败；本次保留该边界，不顺带扩展任务隔离机制。

## 4. 实施计划

| 文件/资产 | 变更 | 原因 | blast radius |
| --- | --- | --- | --- |
| `Tools/task_gate.py` | plan/build 模式，批准版本和严格当前结论校验 | 明确 BUILD 准入、拒绝失效批准 | 本地流程工具 |
| `Tools/Tests/test_task_gate.py` | 增加 13 项拒绝/通过测试，总计 21 项 | 保护正常、重审和缺失证据路径 | 工具测试 |
| `Skills/*/SKILL.md` | 统一 PLAN 到 BUILD 的边界；专项方案选择在审查前、测试在审查后 | 避免入口顺序不一致 | 三个项目 Skill |
| `00-05`、`agent.md`、README | 流程、权限、四个 Gate 命令和能力边界 | 统一入口 | 当前流程文档 |
| Spec/Intake/Gate/交付模板 | 当前批准版本、审查证据、重审状态 | 为检查提供明确字段 | 新任务记录 |
| `00-01` | 记录本任务状态和证据 | 唯一实时状态来源 | 本任务条目 |

## 5. 验收标准（AC）

- [x] AC-01：三个项目 Skill 均要求当前 F1 批准和 Build Gate 通过后才允许修改代码、测试或资产。
- [x] AC-02：流程与模板包含审查人、审查版本、范围/AC/测试/回滚证据和重审状态。
- [x] AC-03：Gate 拒绝未批准、缺证据、版本过期、候选列表、重复结论和历史批准替代；完整当前批准可通过。
- [x] AC-04：21 项单测、独立审查复验、Skill 格式及文档/空白检查通过。
- [x] AC-05：差异不包含 `Source/`、`Content/` 或其他 Runtime 修改。

## 6. Definition of Done

- [x] 对应代码修改前完成 F1；新 Gate 自举验证顺序如实记录。
- [x] Skill、流程、模板、README 和 `agent.md` 规则一致。
- [x] 工具和文档验证包含实际结果及证据位置。
- [x] F2 两项发现修复后独立复验通过。
- [x] 未运行 UE 验证的原因、工具边界和用户验收状态已记录。

## 7. 测试矩阵与命令

| 层级 | 用例/命令 | 证据 | 结果 |
| --- | --- | --- | --- |
| Preflight | `python3 -B Tools/task_gate.py --mode preflight --spec Doc/CombatSystem/Specs/DOC-009-plan-review-lock.spec.md --kind process` | 创建 Spec 后、代码修改前的执行输出 | PASS |
| PLAN CLI | 临时目录复制 Spec，将状态设为 PLAN_REVIEW 后运行 `--mode plan --root <temp-root>` | `Saved/TaskGate/DOC-009/plan-cli-fixture.json` | PASS；功能验证，不追溯本次原始审查 |
| Build | `python3 -B Tools/task_gate.py --mode build --spec Doc/CombatSystem/Specs/DOC-009-plan-review-lock.spec.md --kind process --report Saved/TaskGate/DOC-009/build.json` | 对应 JSON | PASS |
| Unit | `python3 -B -m unittest discover -s Tools/Tests -p 'test_task_gate.py' -v` | `Saved/TaskGate/DOC-009/task-gate-tests.log` | 21/21 PASS |
| Skill | `quick_validate.py Skills/<skill-name>`，使用临时 venv 的 Python/PyYAML | `Saved/TaskGate/DOC-009/skill-validation.log` | 三个 Skill PASS |
| 文档 | `python3 -B Tools/validate_docs.py --report Saved/TaskGate/DOC-009/docs.json` | 对应 JSON | PASS |
| 差异 | `git -c lfs.storage=/private/tmp/ue_gas-lfs-status diff --check` | `Saved/TaskGate/DOC-009/diff-check.log` | PASS |
| Delivery | `python3 -B Tools/task_gate.py --mode delivery --spec Doc/CombatSystem/Specs/DOC-009-plan-review-lock.spec.md --kind process --report Saved/TaskGate/DOC-009/delivery.json` | 对应 JSON；Git 使用命令级临时 LFS 缓存 | PASS |
| Runtime | UE 构建、Combat Automation、PIE、Dedicated、Soak | 本任务不改 Runtime/资产/网络 | N/A，未执行 |

## 8. 风险、回滚与升级

- 风险：工具检查当前记录，不能作为文件系统强制锁或历史时序证明；delivery 全工作区范围限制见 §3。
- 回滚方式：仅回退本次流程、Skill、工具、单测和记录的差异，不触碰 Runtime、资产或用户修改。
- 触发升级：新需求超出批准范围、用户要求人审、L2 操作或必需验证无法完成。
- 需要人决定的问题：无；用户于 2026-09-12 明确验收完成并授权提交。

## 9. 交付证据

- F2 第一轮发现：P1 候选结论与历史正文可冒充批准；P2 重审步骤缺少退回 PLAN_REVIEW。修复后独立临时 Spec 的 13 个场景符合预期，两项 findings 关闭。
- 实际失败与修复：空审查证据拒绝测试失败，修复字段解析跨行后通过。其他新增测试随实现增加，没有伪造测试先行的 Red。
- 环境处理：系统 Python 缺少 PyYAML，使用 `/private/tmp/ue_gas-skill-validation` 临时 venv 运行官方校验器；没有修改项目或全局依赖。Git LFS 仅将此次检查缓存写入临时目录，未禁用过滤器。
- 代码/资产 diff：仅本任务工具、单测、Skill 和当前文档；无 Runtime/资产变更。初次交付未提交或推送；用户验收后授权本地提交。
- 构建结果：Python 工具测试通过；UE 构建和 Automation/PIE/Dedicated 不适用。
- 最终 delivery：PASS，0 error；报告见 `Saved/TaskGate/DOC-009/delivery.json`，仅列本任务工具、Skill、文档及 Spec 差异。

## 10. 变更记录

| 版本 | 日期 | 修改 | 原因 |
| --- | --- | --- | --- |
| 0.1 | 2026-09-12 | 初稿，经 F1 审查后实现 | 用户要求 PLAN 通过前禁止修改代码 |
| 0.2 | 2026-09-12 | 计划检查不从全工作区差异推断违规，BUILD/Delivery 校验当前批准版本；重新审查后实现 | 保留既有修改，明确工具能力边界 |
| 0.2 | 2026-09-12 | 回写验证、自举顺序和 F2 复验；不改变本版范围或运行语义 | 交付证据 |
| 0.2 | 2026-09-12 | 用户确认“验收完成，提交吧”，记录已验收和本地提交授权；不改变运行语义 | 用户验收 |

## 11. 路由、自评与 Reflect

- 原始需求摘要：优化 Skill/文档，PLAN 计划审查通过前禁止代码修改。
- 主 Skill：`skill-creator`
- 选择依据：任务维护开发 Skill 及其流程；复用通用功能流程和路由自评。
- 备选 Skill 与排除理由：专项 GAS 技能开发不适用；未新增项目外 Skill。
- 路由置信度：`high`

| 维度 | 权重 | 分数（0–5） | 证据与扣分原因 |
| --- | ---: | ---: | --- |
| 需求与 AC | 20% | 5 | 五项 AC 闭合 |
| 架构与权限 | 20% | 5 | F1 先于对应工具代码修改；不改变 Runtime 和用户权限 |
| 实现与数据 | 20% | 4 | 能拒绝当前无效批准，不能自动证明历史时序或识别语义变化 |
| 验证证据 | 20% | 5 | 21 项单测、13 个独立场景、Skill 与文档检查 |
| 文档与可观测性 | 10% | 5 | 三个 Skill、流程、模板和报告同步 |
| 交付卫生 | 10% | 5 | 无无关修改，生成报告保留在 Saved，不提交 |

- 计算总分：`4.8 / 5.0`
- 硬性封顶或未执行项：UE 验证不适用，无未关闭高风险 F2 finding。
- 自评结论：`EVIDENCE_SUFFICIENT`
- 用户验收状态：`已验收`（2026-09-12，用户确认“验收完成，提交吧”）
- 观察与证据：用户直接要求流程加强；原 Gate 的 F1 检查只在交付阶段，且结论匹配可能误读候选/历史记录。
- 根因类别：流程；本次是用户明确要求的维护，不依赖自动调优的重复遗漏门槛。
- 调整文件与预期收益：统一入口时序，增加当前批准和版本校验，降低先实现后补计划的风险。
- 回归验证：见 §7 和独立 F2 复验。
- 需要用户决定的问题：无。
