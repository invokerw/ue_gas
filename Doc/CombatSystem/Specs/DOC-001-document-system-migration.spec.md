# DOC-001 文档体系迁移

> Spec 版本：`1.0`
> 状态：`READY_FOR_REVIEW`
> Owner：项目维护者
> 创建日期：2026-09-09
> 关联进度台账：[`00-01-Progress-Tracker.md`](../00-Project/00-01-Progress-Tracker.md)
> 风险等级：`L0`

## 1. 目标与范围

### 目标

将 Combat 的平铺文档按开发职责分层，使当前规则、任务状态、内容扩展、工具流程和历史证据有明确入口，并让 AI-Native 流程可以直接执行。

### 范围

- 重组 `Doc/CombatSystem` 下的 Markdown 文档目录。
- 修复根 README、`agent.md`、索引、当前文档和历史文档中的相对链接。
- 增加 Intake、Spec、Gate 和交付记录模板。
- 将新流程接入当前项目的 README、文档索引和 Agent 规则。

### Non-Goals

- 不修改 C++、蓝图、DataAsset、GameplayTag、网络载荷或运行时语义。
- 不重写 M0–M8 历史决策和验收结论。
- 不改变 `combat_v1_rc1` 或 SAM 的用户验收状态。

## 2. 当前事实与依据

- 当前入口：[`Combat 文档索引`](../README.md)、[`README.md`](../../../README.md)、[`agent.md`](../../../agent.md)。
- 当前状态（建档时）：M0–M8 已验收；SAM 技术 Gate 已通过，仍待用户复验。
- 迁移前文档：`Doc/CombatSystem/*.md` 平铺结构。

## 3. 目标结构

| 目录 | 职责 |
| --- | --- |
| `00-Project` | 状态、流程、路线图、测试、ADR/Gap |
| `10-Architecture` | 当前架构和运行时语义 |
| `20-Content` | 示例技能和公开扩展 |
| `30-Tooling` | UE MCP 工作流与诊断 |
| `90-History` | M0–M8 冻结决策与验收 |
| `Specs` | 任务规格和模板 |

## 4. 验收标准

- [x] 所有原平铺文档已移动到目标目录，迁移阶段保留原编号；后续由 DOC-003 按目录重新编号。
- [x] `Doc/CombatSystem/README.md` 成为 Combat 文档总入口，包含目录职责和阅读路径。
- [x] README、`agent.md`、当前文档、历史文档的 Markdown 本地链接全部可解析。
- [x] 新增 Intake、Spec、Gate、交付记录模板，并加入索引。
- [x] 文档迁移不引入源码或资产变更。
- [x] 明确当前文档与历史文档的维护边界。

## 5. 验证证据

- 本地 Markdown 链接检查：46 个文件，0 个断链。
- Markdown 尾随空格检查：0 处。
- `git diff --check`：通过。
- `python3 Tools/validate_docs.py`：通过；必需入口、目录结构、旧路径、链接和状态标记均通过。
- UE 编译、Automation、PIE、Dedicated：未执行，原因是本任务仅修改文档。

## 6. 交付与后续

- 交付状态：文档迁移已完成，等待用户复核目录和阅读路径。
- 后续新任务从本目录复制 Spec 模板，并按 [`00-05-AI-Native-Development-Workflow.md`](../00-Project/00-05-AI-Native-Development-Workflow.md) 执行 F0/F1/F2 和 Push-Ready 检查。
- 如需恢复旧路径，使用 Git 重命名记录或在索引中添加兼容说明，不在当前文档目录重新建立平铺副本。
