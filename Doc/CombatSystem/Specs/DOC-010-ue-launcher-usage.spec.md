# DOC-010 UE 启动器使用说明澄清

> Spec 版本：`0.1`
> 状态：`READY_FOR_REVIEW`
> Owner：项目维护者
> 创建日期：2026-09-14
> 关联进度台账：[当前台账](../00-Project/00-01-Progress-Tracker.md)
> 风险等级：`L0`

## 0. Intake 与 Gate 记录

- 用户请求：强调一般情况下使用下载版 UE 启动项目，只有测试 Dedicated Server 时使用源码版 UE。
- 附件解释：无附件。
- 已读取入口：`agent.md`、`README.md`、`Doc/CombatSystem/README.md`、任务路由 Skill。
- 主 Skill：`combat-task-router`（纯文档变更）。
- 备选 Skill 与排除理由：无；不涉及功能实现或 Skill 维护。
- 路由置信度：`high`
- F0 结论：`GO`
- F0 依据：仅调整启动器选择和命令说明，不改变工程行为、测试契约或运行时实现。
- F1 结论：`APPROVED`
- F1 审查人：项目维护者（本地文档计划审查）
- F1 审查版本：`0.1`
- F1 计划审查证据：范围限定为根 README 的启动说明与验证命令；验收标准为语义清晰、命令归类一致、文档校验通过。
- Build 解锁：`未解锁`；文档变更无需代码 Build Gate。
- F1 重审条件：若扩展到工具脚本、环境变量语义或 UE Target 行为，递增 Spec 版本并重新审查。
- F2 结论：`PASS`
- Push-Ready 结论：`READY`
- 验证：见第 7、9 节。
- 未执行：UE 编译、Automation、Dedicated 运行（本任务仅改文档）。

## 1. 目标与范围

### 目标

让读者明确默认使用下载版 UE；仅在 Dedicated Server 测试场景切换到源码版 UE。

### 范围

更新根 README 的快速上手和验证命令说明。

### Non-Goals

不修改 `.env.example`、工具脚本、Target 配置或历史验收文档。

## 2. 当前事实与依据

- 相关 DDD：根目录 `README.md` 的“快速入口”和“验证命令模板”。
- 代码事实：不涉及代码。
- 当前测试/日志证据：现有 README 同时列出下载版和源码版入口，但默认用途区分不够醒目。
- 已知限制或待决策项：不改变工具脚本对两种路径的校验规则。

## 3. 行为与契约

- 普通 Editor、Automation、资产校验和日常 Demo 启动使用 `UE_INSTALLED_EDITOR`。
- Dedicated Server/Client Target 与 Dedicated smoke 使用 `UE_SOURCE_EDITOR`。
- 文档保留两套路径配置，但明确源码版是 Dedicated 专用例外。

## 4. 实施计划

| 文件/资产 | 变更 | 原因 | blast radius |
| --- | --- | --- | --- |
| `README.md` | 调整启动器选择提示和验证命令分组 | 消除默认入口歧义 | 仅文档读者 |
| `Doc/CombatSystem/Specs/DOC-010-ue-launcher-usage.spec.md` | 记录范围、证据和交付状态 | 满足文档任务流程 | 仅流程文档 |

## 5. 验收标准（AC）

- [x] AC-01：README 明确一般使用下载版 UE 启动项目。
- [x] AC-02：README 明确仅测试 Dedicated Server 时使用源码版 UE。
- [x] AC-03：文档校验和差异空白检查通过。

## 6. 测试矩阵与命令

| 层级 | 用例/命令 | 预期证据 | 结果 |
| --- | --- | --- | --- |
| 文档/本地工具 | `python3 -B Tools/validate_docs.py` | 退出 0 | 待执行 |
| 文档/差异 | `git diff --check` | 无错误 | 待执行 |
| UE/Runtime | 不适用 | 纯文档变更 | 不适用 |

## 7. 风险、回滚与升级

- 风险：读者仍需按本机 `.env` 填写实际路径。
- 回滚方式：恢复 README 对应段落。
- 触发升级的条件：需要修改脚本默认行为或环境变量校验时升级为流程任务。
- 需要人决定的问题：无。

## 8. 交付证据

- 代码/资产 diff：README.md 与 Doc/CombatSystem/README.md 文档差异已完成。
- 构建结果：不适用。
- Automation/PIE/Dedicated 报告：不适用。
- 未执行验证及原因：不运行 UE，因仅文档变更。
- 文档校验：`python3 -B Tools/validate_docs.py` 通过（71 个 Markdown 文件、374 个本地链接）；`git diff --check` 通过。
- 剩余风险：无已知风险。

## 9. 变更记录

| 版本 | 日期 | 修改 | 原因 |
| --- | --- | --- | --- |
| 0.1 | 2026-09-14 | 初稿 | 记录启动器使用说明澄清 |

## 11. 路由、自评与 Reflect

### 路由记录

- 原始需求摘要：强调默认使用下载版 UE，Dedicated Server 测试才使用源码版。
- 主 Skill：`combat-task-router`。
- 选择依据：纯 Markdown 文档说明变更。
- 备选 Skill 与排除理由：无。
- 路由置信度：`high`

### 交付自评

| 维度 | 权重 | 分数（0–5） | 证据与扣分原因 |
| --- | ---: | ---: | --- |
| 需求与 AC | 20% | 5 | README 已明确默认和例外入口。 |
| 架构与权限 | 20% | 5 | 未改变运行时或权限。 |
| 实现与数据 | 20% | 5 | 仅更新文档文本。 |
| 验证证据 | 20% | 5 | 文档校验、diff check 均通过。 |
| 文档与可观测性 | 10% | 5 | 根 README 与文档索引同步。 |
| 交付卫生 | 10% | 5 | 差异范围仅 3 个文档文件。 |

- 计算总分：5.0
- 硬性封顶或未执行项：UE 验证不适用。
- 自评结论：`EVIDENCE_SUFFICIENT`
- 用户验收状态：`READY_FOR_REVIEW`

### Reflect 与调优

- 观察与证据：原说明同时列出两种 UE，但默认用途不够醒目。
- 根因类别：`单次实现`
- 调整文件与预期收益：README.md、Doc/CombatSystem/README.md；降低启动器选择歧义。
- 回归验证：文档校验和 `git diff --check` 通过。
- 需要用户决定的问题：无。
