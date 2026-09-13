---
name: combat-task-router
description: "根据用户需求为 ue_gas Combat 选择合适的开发 Skill，并在交付后进行证据化自评与受控流程调优。"
---

# Combat 任务路由与复盘

当用户提出一个可能涉及 Combat 开发、技能实现、文档流程或 Skill 维护的需求时使用本 Skill。它负责选择一个主执行 Skill、记录路由依据、收集交付证据、计算自评分数，并决定是否需要调整流程或 Skill；它不替代实际实现 Skill，也不把纯咨询强行变成代码任务。

本 Skill 只在当前仓库内生效，唯一来源为 `Skills/combat-task-router/`；不要复制或安装到用户级 Codex Skill 目录。若当前会话无法自动发现项目内 Skill，按该路径读取后继续执行。

## 输入、前置条件与输出

- 输入：用户原始需求、当前工作区、已有 Spec/台账、可用 Skill 和用户明确的范围或验收要求。
- 前置条件：确认工作区包含 `ue_gas.uproject`，完整阅读 `agent.md`，检查已有 diff；先判断是否允许修改文件或外部系统。
- 输出：路由结论、选择依据和置信度；被选 Skill 的完整交付结果；Spec 中的自评表、证据、Reflect 和调优结论。

路由完成后，开工消息必须先公开列出已读取入口、用户请求与附件解释、主 Skill、置信度和 Spec 路径，再进入实现。若任务需要修改仓库，必须先运行机器 Gate：

```bash
python3 -B Tools/task_gate.py --mode preflight --spec Doc/CombatSystem/Specs/<task-id>.spec.md --kind <feature|docs|process>
```

Gate 失败表示上下文或 Spec 不完整，应先修正记录；不能把“已经知道怎么做”当作 F0/F1 通过。

## 主 Skill 路由

按用户的**实际动作目标**判断，不按关键词单独判断。每次选择一个主执行 Skill，并把备选和排除理由写入 Spec；专项 Skill 可以在其内部复用通用流程。

| 需求目标 | 主 Skill | 判定信号 | 不应路由到 |
| --- | --- | --- | --- |
| 实现/修复/迁移 Combat 功能 | `$combat-feature-development` | C++、Bug、网络、Order、移动、UI、工具、文档或资产流程变更 | 只做技能内容时不单独调用专项之外的流程 |
| 实现或修改玩家可施放的 GAS/Combat 技能 | `$combat-skill-development` | Ability、技能 DataAsset、Modifier、Projectile、Thinker、Aura、Motion、伤害/治疗和技能验收 | 不要只调用通用 Skill 后遗漏技能专项检查 |
| 创建、修改或验证 Codex Skill 本身 | `$skill-creator` | 用户明确要求创建/更新/安装/验证 Skill 或其 UI 元数据 | 不把 Skill 维护误当 gameplay 实现 |
| 纯咨询、解释、评审或方案比较 | 不调用实现 Skill | 用户未要求修改仓库或交付实现 | 不为回答问题创建虚假 Spec |

### 冲突与边界

- 同时包含“实现技能”和“修改战斗内核”时，选择 `$combat-skill-development` 作为主 Skill，并在 Spec 标出内核扩展的 blast radius；它必须遵守通用功能流程。
- 同时包含“修改 Skill”和“用该 Skill 实现功能”时，先用 `$skill-creator` 完成并验证 Skill 变更，再为 gameplay 任务建立新的 Spec；不要让 Skill 在同一任务中递归修改自身并宣称已验证。
- 只有设计建议或代码解释时，直接完成咨询；若用户后来要求落地，再重新路由。
- 无法区分时选择覆盖范围更大的 `$combat-feature-development`，记录低置信度和需要澄清的玩法契约；如果选择会改变版本、权限或不可逆迁移，先升级给用户决定。

## 路由步骤

1. 提取动作（解释、实现、修复、迁移、创建 Skill）、对象（功能、技能、流程）和交付物。
2. 判断是否触及源码、资产、运行时契约、网络权限、生命周期或仅为文字说明。
3. 按上表选一个主 Skill，给出 `high / medium / low` 置信度和最多两个备选。
4. 将“原始需求、路由结论、依据、非目标、需要用户决定的问题”写入任务 Spec，加载主 Skill 完成 THINK/PLAN。F1 未批准当前 Spec 版本、Build Gate 未通过前，不得让执行 Skill 修改代码、测试、工具脚本、蓝图或资产；允许读取、运行已有检查和维护 Spec/计划记录。
5. 主 Skill 完成后检查它的 Gate、测试和未执行项，再进行下方自评与 Reflect。

计划审查运行 `python3 -B Tools/task_gate.py --mode plan --spec Doc/CombatSystem/Specs/<task-id>.spec.md --kind <feature|docs|process>`；F1 批准后、第一次行为文件修改前运行 `python3 -B Tools/task_gate.py --mode build --spec Doc/CombatSystem/Specs/<task-id>.spec.md --kind <feature|docs|process>`。交付前再次运行 `python3 -B Tools/task_gate.py --mode delivery --spec Doc/CombatSystem/Specs/<task-id>.spec.md --kind <feature|docs|process>`；只有机器 Gate 通过并且证据写回 Spec，才可给出 `READY_FOR_REVIEW`。

## 交付自评

只使用已经运行的测试、构建、回读、审查结果和文档证据评分；推测、计划和“应该可以”不得计分。每项 0–5 分，按权重计算：

| 维度 | 权重 | 评分依据 |
| --- | ---: | --- |
| 需求与 AC | 20% | 范围、Non-Goals、AC、失败路径和用户目标是否闭合 |
| 架构与权限 | 20% | 公共入口、服务器权威、唯一数据源、生命周期和版本边界 |
| 实现与数据 | 20% | 代码/资产行为、配置来源、兼容和回滚是否符合 Spec |
| 验证证据 | 20% | 直接测试、回归、资产/蓝图、PIE、Network/Dedicated 等适用层级 |
| 文档与可观测性 | 10% | DDD、Spec、事件/日志、ToolTip、报告路径和台账回写 |
| 交付卫生 | 10% | diff 范围、旁路扫描、空白、未执行项和用户验收状态 |

计算：`总分 = Σ(维度分 × 权重)`，结果保留一位小数。评分必须在 Spec 中列出每一项分数、证据和扣分原因，并单独写 `READY_FOR_REVIEW`、`BLOCKED` 或“用户已验收”；自评分不能替代用户验收。

硬性封顶：必需验证因环境阻塞而未执行时，“验证证据”最高 2 分；F2 有未关闭的高风险发现时总分最高 2.9；Spec 与代码事实不一致时不得判为 READY。没有用户明确确认时，不得把交付状态写成“已验收”。

建议解释区间：4.5–5.0 为证据充分，4.0–4.4 为可交用户复核，3.0–3.9 需修订，低于 3.0 或触发硬性封顶时升级。

## 受控调优与 Reflect

先把问题归类，再决定改哪里：

1. **单次实现问题**：只修当前任务、Spec 或测试；不因一次失败扩大通用规则。
2. **流程问题**：相同遗漏在至少两个独立任务出现，更新 `_template.spec.md`、`_gate-checklist.md`、`00-05` 或本地校验器，并增加可观察的验证项。
3. **路由问题**：出现误选主 Skill、触发范围重叠或专项检查长期遗漏，更新本 Skill 的路由表/描述，并用真实请求复盘路由结果。
4. **领域契约问题**：涉及 GameplayTag、DefinitionId、事件 schema、版本、权限或迁移时，先更新 `00-04-Decisions-Gaps.md` 和相关专题，再修改 Skill；不能用提示词掩盖架构缺口。

每次任务最多做一轮受控调优。Reflect 至少记录：观察、证据、根因类别、修改文件、预期收益、回归验证和是否需要用户决定。只修改低风险文档、模板、Skill 说明或校验规则；代码、二进制资产、版本契约和外部系统仍按原有权限和新 Spec 处理。调优后重新计算自评，不得为了提高分数修改评分证据。

## 停止与升级

出现需求无法归类、主 Skill 选择会改变玩法/版本契约、必需验证被阻塞、同一问题三轮仍不收敛、或自评触发硬性封顶时，停止自动扩展范围。向用户提供：路由结论、已完成工作、证据、缺口、建议选项和需要决定的问题。

## 交付格式

最终说明按以下顺序给出：主 Skill 与路由依据；变更和验证结果；自评分数与扣分证据；Reflect/调优动作；未执行项、剩余风险和用户验收状态。
