# DOC-005 Combat 功能开发 Skill

> Spec 版本：`1.0`
> 状态：`READY_FOR_REVIEW`
> Owner：项目维护者
> 创建日期：2026-09-09
> 风险等级：`L1`
> 关联台账：[开发进度台账](../00-Project/00-01-Progress-Tracker.md)

## 1. 目标与范围

为 Combat 的 C++、DataAsset、蓝图、网络、测试和文档任务提供一个可直接调用的通用 AI-Native 功能开发 Skill，统一执行 F0/F1/F2、Spec、TDD、本地验证、Push-Ready 和 Reflect。

范围：`Skills/combat-feature-development/SKILL.md`、UI 元数据、项目入口和本 Spec。Non-Goals：实现具体 gameplay 功能、修改二进制资产、PR、GitHub Actions、自动提交或推送。

## 2. 当前事实与依据

- AI-Native 流程：[00-05 AI-Native 开发流程](../00-Project/00-05-AI-Native-Development-Workflow.md)。
- 项目硬约束：[agent.md](../../../agent.md)和当前台账。[00-01](../00-Project/00-01-Progress-Tracker.md)是唯一实时状态来源。
- 技能专项在通用流程之上使用：[DOC-004 Combat 技能开发 Skill](DOC-004-combat-skill-development.spec.md)。

F0：GO。F1：APPROVED。Skill 只编排当前项目流程和公共入口，不授权改变版本契约或越过项目权限边界。

## 3. 行为与契约

- 任务先读取需求、工作区和 DDD，输出 F0 范围、Non-Goals、风险和依赖。
- 功能、Bug、资产迁移和契约变更先建立 Spec，补齐 AC、DoD、状态转换、失败路径、测试、迁移、回滚和证据位置。
- 可执行行为采用 Red → Green → Verify；纯文档修正记录链接、格式和事实校验，不虚构未执行的测试。
- 按风险执行构建、Automation、资产、PIE、Network/Dedicated 和旁路扫描；未执行项必须如实记录。
- F2 最多三轮修复与复验；Push-Ready 六层逐层标明通过、N/A 或未执行；用户验收前不写“已验收”。

## 4. 验收标准（AC）

- [x] AC-01：Skill 有明确适用范围、输入、前置条件、步骤、工具、输出、检查和升级条件。
- [x] AC-02：Skill 引用当前台账、AI-Native 流程、Spec 模板、项目硬约束和本地验证命令。
- [x] AC-03：Skill 明确本地单人协作边界，不要求 PR 或 GitHub Actions，不自动提交或推送。
- [x] AC-04：UI 元数据可解析，根 README、文档索引、Agent 规则和仓库内 `Skills/` 路径可定位 Skill。

## 5. 测试矩阵与命令

| 检查 | 命令/方式 | 结果 |
| --- | --- | --- |
| Skill frontmatter/body | YAML 等价解析、名称/描述、输入/输出/升级段落和无 TODO 占位检查 | 通过 |
| UI YAML | Ruby YAML 解析 `Skills/combat-feature-development/agents/openai.yaml` | 通过 |
| 文档链接与编号 | `python3 -B Tools/validate_docs.py` | 通过 |
| 文档校验回归 | `python3 -B -m unittest discover -s Tools/Tests -p 'test_validate_docs.py'` | 通过 |
| 空白 | `git diff --check` 及新增文件尾随空格扫描 | 通过 |

## 6. 交付证据

通用 Skill 已创建并接入 README、文档索引、Agent 规则和 AI-Native 流程；项目内 `Skills/` 是唯一来源，不写入用户级 Codex Skill 目录。官方 `quick_validate.py` 因系统与 bundled Python 均缺少 PyYAML 未能启动；采用 Ruby YAML 解析和静态规则检查。尚未用该 Skill 实施具体 gameplay 任务；未运行 UE 编译、Automation、PIE 或 Dedicated，因为本任务只新增流程 Skill 和文档入口。

回滚：删除 `Skills/combat-feature-development/`、入口引用和本 Spec，不触碰专项技能 Skill、源码或资产。
