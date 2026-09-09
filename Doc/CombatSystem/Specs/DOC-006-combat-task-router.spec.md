# DOC-006 Combat 任务路由与复盘 Skill

> Spec 版本：`1.0`
> 状态：`READY_FOR_REVIEW`
> Owner：项目维护者
> 创建日期：2026-09-09
> 风险等级：`L1`
> 关联台账：[开发进度台账](../00-Project/00-01-Progress-Tracker.md)

## 1. 目标与范围

为 Combat 建立一个统一任务入口，根据用户的实际动作目标选择通用功能 Skill、专项技能 Skill、Skill 维护流程或直接咨询；交付后使用可回读证据自评，并按重复失败受控调优流程或 Skill。

范围：`Skills/combat-task-router/SKILL.md`、UI 元数据、Intake/Spec/交付记录模板、AI-Native 流程、项目入口和本 Spec。Non-Goals：实现 gameplay 功能、自动递归调用 Skill、替代用户验收、修改生产代码/二进制资产或创建 PR/GitHub Actions。

## 2. 当前事实与依据

- 通用功能流程：[DOC-005 Combat 功能开发 Skill](DOC-005-combat-feature-development.spec.md)。
- 技能专项流程：[DOC-004 Combat 技能开发 Skill](DOC-004-combat-skill-development.spec.md)。
- Skill 创建规范：`skill-creator`；项目任务流程：[00-05 AI-Native 开发流程](../00-Project/00-05-AI-Native-Development-Workflow.md)。

F0：GO。F1：APPROVED。路由层只选择主执行 Skill 和复盘动作，不授予额外权限。

## 3. 行为与契约

- 以用户动作目标为主判断：实现/修复/迁移走通用流程，实际 GAS 技能走专项流程，Skill 本身的创建/更新走 `skill-creator`，纯咨询不调用实现 Skill。
- 冲突需求只选择一个主 Skill；技能实现同时涉及内核时由专项 Skill 承担主流程并在 Spec 记录 blast radius。
- Spec 记录原始需求、主 Skill、备选、排除理由和路由置信度。
- 完成后按需求与 AC、架构与权限、实现与数据、验证证据、文档与可观测性、交付卫生六个维度以 0–5 分自评并给出证据。
- 必需验证未执行、F2 高风险发现未关闭或 Spec 与代码事实不一致时触发封顶/升级；自评不能替代用户验收。
- 单次问题只修任务；相同遗漏至少出现于两个独立任务后才调整模板、Gate、校验器或流程；路由误选才调整路由 Skill；契约问题先进入 ADR/Gap。
- 每个任务最多一轮受控调优，调优后重新评分并记录回归证据，避免无界自我修改。

## 4. 验收标准（AC）

- [x] AC-01：Skill 覆盖四类路由、冲突边界、置信度和路由记录格式。
- [x] AC-02：Intake、Spec 和交付记录模板包含路由或自评/Reflect 记录区，六维评分权重和硬性封顶规则明确。
- [x] AC-03：流程文档、README、Agent 入口和 UI 元数据可定位该 Skill；明确本地单人流程，不要求 PR 或远端 CI。
- [x] AC-04：调优规则区分单次实现、流程、路由和领域契约问题，并限制为一轮且需要回归证据。

## 5. 测试矩阵与命令

| 检查 | 命令/方式 | 结果 |
| --- | --- | --- |
| Skill frontmatter/body | YAML 等价解析、路由/自评/调优段落和无 TODO 占位检查 | 通过 |
| UI YAML | Ruby YAML 解析 `Skills/combat-task-router/agents/openai.yaml` | 通过 |
| Spec/模板与链接 | `python3 -B Tools/validate_docs.py` | 通过 |
| 文档校验回归 | `python3 -B -m unittest discover -s Tools/Tests -p 'test_validate_docs.py'` | 通过 |
| 空白 | `git diff --check` 及新增文件尾随空格扫描 | 通过 |

## 6. 交付证据

任务路由 Skill 已创建并接入 Spec 模板、AI-Native 流程、README、文档索引和 Agent 规则；项目内 `Skills/` 是唯一来源，不写入用户级 Codex Skill 目录。官方 `quick_validate.py` 因系统与 bundled Python 均缺少 PyYAML 未能启动；采用 Ruby YAML 解析和静态规则检查。尚未用该 Skill 执行真实 gameplay 任务或调优一轮；未运行 UE 编译、Automation、PIE 或 Dedicated，因为本任务只新增路由/复盘流程。

回滚：删除 `Skills/combat-task-router/`、入口引用、Intake/Spec/交付记录模板新增段落和本 Spec，不触碰两个执行 Skill、源码或资产。
