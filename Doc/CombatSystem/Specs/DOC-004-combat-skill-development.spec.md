# DOC-004 Combat 技能开发 Skill

> Spec 版本：`1.0`
> 状态：`READY_FOR_REVIEW`
> Owner：项目维护者
> 创建日期：2026-09-09
> 风险等级：`L1`
> 关联台账：[开发进度台账](../00-Project/00-01-Progress-Tracker.md)

## 1. 目标与范围

为 GAS/Combat 技能创建一个可复用的项目级 Skill，让 Agent 按 AI-Native 流程实际落地 Ability、DataAsset、Modifier、Projectile、Thinker、蓝图、测试和文档。

范围：`Skills/combat-skill-development/SKILL.md`、UI 元数据、项目入口和本 Spec。Non-Goals：实现某个具体游戏技能、修改 C++/蓝图/二进制资产、PR、GitHub Actions 或自动提交。

## 2. 当前事实与依据

- 通用流程 Skill：[combat-feature-development](../../../Skills/combat-feature-development/SKILL.md)。
- 技能扩展边界：[20-03 公共扩展与迁移指南](../20-Content/20-03-M8-Public-Extension-Guide.md)和 [20-02 模板检查表](../20-Content/20-02-M6-Skill-Template-Checklist.md)。
- 技能运行时依据：`10-03` Ability/目标、`10-05` Damage/Heal、`10-06` Attack/Projectile/Thinker。

F0：GO。F1：APPROVED。Skill 只编排已存在的公共入口，不替代项目硬约束。

## 3. 行为与契约

- 技能任务先建立 Spec，判断 DataDriven Action、Ability、Modifier Runtime、Projectile、Thinker、Aura 或 Motion 的最小组合。
- 平衡值进入 DataAsset special；目标、资源、冷却、生命周期和服务器结算遵循现有公共 API。
- 技能专项包含成功、失败/边界、取消、死亡/复活、重复请求、快照、旧 Handle 和 Dedicated（适用时）验证。
- 输出包含资产/DefinitionId、行为边界、修改文件、Gate、测试证据、未执行项、迁移和用户验收状态。

## 4. 验收标准（AC）

- [x] AC-01：Skill 目录包含有效 `SKILL.md`，有明确适用范围、输入、步骤、检查和升级条件。
- [x] AC-02：Skill 引用当前目录编号和技能专项文档，覆盖 Ability、DataAsset、Modifier、Projectile/Thinker、服务器权限和测试。
- [x] AC-03：Skill 明确复用通用 AI-Native 流程、Spec、F0/F1/F2 和本地验证，不要求 PR 或远端 CI。
- [x] AC-04：UI 元数据可解析，仓库 README、索引和 Agent 入口可定位 Skill。

## 5. 测试矩阵与命令

| 检查 | 命令/方式 | 结果 |
| --- | --- | --- |
| Skill frontmatter/body | YAML 解析、名称/描述/字段与无 TODO 占位检查 | 通过 |
| UI YAML | Ruby YAML 解析 `Skills/combat-skill-development/agents/openai.yaml` | 通过 |
| 文档链接与编号 | `python3 -B Tools/validate_docs.py` | 通过 |
| 文档校验回归 | `python3 -B -m unittest discover -s Tools/Tests -p 'test_validate_docs.py'` | 通过 |
| 空白 | `git diff --check` | 通过 |

## 6. 交付证据

Skill 已创建并接入 README、文档索引、Agent 规则和 AI-Native 流程；项目内 `Skills/` 是唯一来源，不写入用户级 Codex Skill 目录。官方 `quick_validate.py` 因系统与 bundled Python 均缺少 PyYAML 未能启动；采用 Ruby YAML 解析并按脚本规则检查名称、描述、允许字段和占位符。尚未用该 Skill 实施真实技能任务；未运行 UE 编译、Automation、PIE 或 Dedicated，因为本任务只新增流程 Skill 和文档入口。

回滚：删除 `Skills/combat-skill-development/`、入口引用和本 Spec，不触碰通用功能 Skill、源码或资产。
