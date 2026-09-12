# DEMO-902 删除未使用的 Demo 远程攻击技能

> Spec 版本：`0.2`
> 状态：`READY_FOR_REVIEW`
> Owner：Codex
> 创建日期：2026-09-11
> 关联进度台账：`00-01` Post-M8 Demo
> 风险等级：`L1`

## 0. Intake 与 Gate 记录

- 用户请求：确认 Demo 中的 RangeAttack 技能是否已无用；若无用则删除。
- 附件解释：无附件（用户请求本身是范围和删除条件，不提供额外工程约束）
- 已读取入口：`agent.md`、`README.md`、`00-01-Progress-Tracker.md`、`00-03-Test-Plan.md`、`00-04-Decisions-Gaps.md`、`00-05-AI-Native-Development-Workflow.md`、`10-01-Scope-Architecture.md`、`10-03-Ability-Targeting-Blueprint.md`、`10-05-Damage-Heal.md`、`10-06-Attack-Projectile-Thinker.md`、`20-01-Example-Skills.md`、`20-02-M6-Skill-Template-Checklist.md`、`20-03-M8-Public-Extension-Guide.md`、`Skills/combat-task-router/SKILL.md`、`Skills/combat-feature-development/SKILL.md`、`Skills/combat-skill-development/SKILL.md`。
- 主 Skill：`combat-skill-development`
- 备选 Skill 与排除理由：`combat-feature-development` 作为通用流程被主 Skill 继承；`skill-creator` 不适用，因为不修改 Skill 本身。
- 路由置信度：`high`
- F0 结论：`GO`。
- F0 依据：请求涉及 Demo Ability/DataAsset 资产清理；UE MCP 引用盘点确认待删 AbilityData/Class 无外部资产引用，复用的 Projectile 仍有三处资产引用，删除范围可控且可回滚。
- F1 结论：`APPROVED`。
- F2 结论：`PASS`。
- Push-Ready 结论：`READY`。
- 验证：开工前通过 UE MCP AssetTools 回读 `get_referencers`/`get_dependencies` 与对象属性；删除后再次回读资产存在性和共享 Projectile 引用，并完成构建、Automation、资产校验、文档校验与差异检查。
- 未执行：Demo PIE、Dedicated/Network、Cook/Packaging 和长时 Soak/Perf 未执行；本任务仅删除未授予 Ability Class/DataAsset，不改运行时行为、网络契约或运行时循环，相关风险已记录。

## 1. 目标与范围

> 历史说明：本 Spec 记录 DEMO-902 完成交付时的资源状态。后续 DEMO-903 已将当时保留的共享 Projectile 资源迁移到 Drow Ranger 英雄目录，并为 Frost Arrows 建立专用 Projectile 表现；当前状态以 DEMO-903 Spec 为准。

### 目标

删除 Demo 中已被 Frost Arrows 和当前 AbilitySet 替代、且没有外部引用的 RangeAttack Ability Class/DataAsset，保留仍被默认普攻和 Frost Arrows 使用的追踪 Projectile 及其表现资产。

### 范围

- 删除 `/Game/Combat/Demo/Abilities/RangedAttack/BP_RangedAttackAbility`。
- 删除 `/Game/Combat/Demo/Abilities/RangedAttack/DA_RangedAttackAbility`（DefinitionId `CombatAbility:ranged_attack_bolt`）。
- 在 `Combat.Foundation.Content.DrowRangerDemo` 增加旧 Ability 包不存在的负向断言。
- 在当前任务 Spec 和相关事实说明中记录删除原因、保留的 Projectile 依赖与验证结果。

### Non-Goals

- 不删除、移动或重命名 `DA_RangedAttackProjectile`、`BP_RangedAttackProjectileActor`、`MI_RangedAttackProjectile`；在 DEMO-902 交付时它们仍由 `DA_FrostArrows` 和 `DA_DrowRangerUnit` 使用。
- 不改变 Frost Arrows、默认远程普攻、Projectile DefinitionId、伤害/攻击/网络契约或 `combat_v1_rc1`。
- 不修改已验收 DEMO-901 的运行时流程；本任务只清理未授予且未被外部资产引用的兼容示例。

## 2. 当前事实与依据

- 相关 DDD：`10-03` Ability 授予与 DefinitionId、`10-06` Projectile 复用与生命周期、`20-03` 资产引用与迁移约束。
- UE MCP AssetTools 回读：删除前 `DA_RangedAttackAbility` 的唯一引用者是 `BP_RangedAttackAbility`；`BP_RangedAttackAbility` 无引用；`DA_RangedAttackProjectile` 被 `DA_FrostArrows`、`DA_DrowRangerUnit`、`DA_RangedAttackAbility` 引用；`BP_RangedAttackProjectileActor` 被 `DA_RangedAttackProjectile` 引用。删除后旧 Ability 两项均不存在，`DA_RangedAttackProjectile` 的现存引用者仅为 `DA_FrostArrows` 和 `DA_DrowRangerUnit`。
- UE MCP 对象回读：`DA_DrowRangerAbilitySet.abilities` 只有 `BP_FrostArrowsAbility`；`DA_FrostArrows.attackOrbProjectileData` 与 `DA_DrowRangerUnit.attackProjectileData` 均指向 `DA_RangedAttackProjectile`。
- 代码事实：`Source/Combat/Combat/Tests/CombatFoundationTests.cpp` 当前只加载并验证共享 Projectile；没有加载 `BP_RangedAttackAbility` 或 `DA_RangedAttackAbility`。
- 已知限制：二进制资产删除需通过 UE AssetTools；若 Editor/资产注册表状态不可用，改用同版本 Unreal Python 命令行并在交付证据中记录降级原因。

## 3. 行为与契约

### 主流程

1. 通过 UE AssetTools 删除未被外部资产引用的 `BP_RangedAttackAbility` 和 `DA_RangedAttackAbility`。
2. 保留共享 Projectile 资产，确保 Frost Arrows 法球和 Drow 默认普攻仍能解析 `CombatProjectile:ranged_attack_projectile`。
3. 运行内容回归断言，确认旧 Ability 包不存在、共享 Projectile 可加载、AbilitySet 仍只授予 Frost Arrows。

### 状态转换

`PLANNED → BUILDING（删除两项资产并更新测试）→ VERIFYING（资产回读/构建/Automation/校验）→ READY_FOR_REVIEW`。

### 输入、输出与数据约束

- 删除对象：`CombatAbility:ranged_attack_bolt` 对应的 AbilityData 与 Ability Class。
- 保留对象：`CombatProjectile:ranged_attack_projectile` 及其 Actor/Material 依赖。
- 不新增 DefinitionId、GameplayTag、事件或网络字段。

### 权威边界与权限

本任务不改运行时结算；技能授予、攻击、Projectile 命中继续使用既有服务器公共入口。删除动作只影响编辑器资产注册和内容加载。

### 失败、取消、过期、死亡、EndPlay 与重复请求

任务不新增运行时异步对象或请求。若删除前发现新的外部引用，停止删除并调整范围；若验证发现共享 Projectile 缺失，按 Git/资产回滚恢复两项 Ability 资产。

### 兼容、版本与迁移

`ranged_attack_bolt` 仅为未授予的 Demo 兼容示例，当前无外部资产或运行时加载者；删除不改变已使用的 Unit、AbilitySet、Frost Arrows 或 Projectile DefinitionId。若后续发现存档/网络消费者，必须升级为独立迁移任务，不在本任务旁路兼容。

## 4. 实施计划

| 文件/资产 | 变更 | 原因 | blast radius |
| --- | --- | --- | --- |
| `Content/Combat/Demo/Abilities/RangedAttack/BP_RangedAttackAbility.uasset` | 删除 | 无外部引用且不在 AbilitySet 中授予 | 仅旧 Ability Class 内容 |
| `Content/Combat/Demo/Abilities/RangedAttack/DA_RangedAttackAbility.uasset` | 删除 | 唯一引用者是待删 Ability Class | 仅旧 AbilityData/DefinitionId |
| `Source/Combat/Combat/Tests/CombatFoundationTests.cpp` | 增加两个包不存在断言 | 防止旧技能资产回流 | Demo 内容回归测试 |
| `Doc/CombatSystem/Specs/DEMO-902-remove-unused-ranged-attack.spec.md` | 记录路由、范围、证据和交付状态 | 可回读的任务契约 | 无运行时影响 |

## 5. 验收标准（AC）

- [x] AC-01：`BP_RangedAttackAbility` 和 `DA_RangedAttackAbility` 包从 Demo 内容中删除，资产注册表中不存在。
- [x] AC-02：`DA_RangedAttackProjectile`、`BP_RangedAttackProjectileActor`、`MI_RangedAttackProjectile` 保留且引用关系完整；Frost Arrows 与 Drow 默认普攻仍指向共享 Projectile。
- [x] AC-03：`Combat.Foundation.Content.DrowRangerDemo` 通过，确认旧 Ability 包不存在、共享 Projectile 可加载、AbilitySet 仍只授予 Frost Arrows。
- [x] AC-04：资产校验、相关 Editor 构建/Automation、`git diff --check` 通过；未发现 redirector 或无意的地图/运行时变更。

## 6. Definition of Done

- [x] 删除范围与 UE MCP 引用盘点一致，没有删除共享 Projectile。
- [x] 负向资产断言通过，且 Drow/Frost Arrows 内容回归通过。
- [x] 相关 Editor 构建、Automation、资产校验和差异检查按风险完成并记录。
- [x] Spec 写回 F1/F2、Push-Ready、验证命令、未执行项和回滚方式。
- [x] `git diff --check` 通过，未混入用户修改或生成文件。

## 7. 测试矩阵与命令

| 层级 | 用例/命令 | 预期证据 | 结果 |
| --- | --- | --- | --- |
| 文档/本地工具 | `python -B Tools/validate_docs.py`、`git diff --check` | Spec/链接/空白检查 | 通过：58 Markdown、320 本地链接、0 error；diff check 通过 |
| Pure/Unit | `Combat.Foundation.Content.DrowRangerDemo`（全量报告包含） | 旧 Ability 不存在；共享 Projectile、Drow Unit、AbilitySet、Frost Arrows 可加载 | 通过：全量 63 个 Combat 测试中该用例成功 |
| World Automation | `Automation RunTests Combat.` | 内容引用无回归 | 通过：63/63 成功，62 无警告，1 个既有调试命令用例带预期用法警告，失败 0 |
| PIE / Blueprint | Demo PIE 或蓝图编译/保存回读 | Frost Arrows 和远程普攻仍可生成追踪 Projectile | 未执行：无蓝图行为修改；由 Foundation 内容测试和资产校验覆盖加载/引用回归 |
| Network / Dedicated | 不改变网络/复制契约 | N/A：仅删除未授予内容；若资产校验要求则记录 | 不适用 |
| Soak / Perf | 不新增运行时循环 | N/A：仅删除资产 | 不适用 |

## 8. 风险、回滚与升级

- 风险：隐藏的软引用或历史工具可能仍按旧 DefinitionId 加载；删除前后通过资产注册表引用查询和内容回归探测。
- 回滚方式：恢复本任务删除的两个 `.uasset` 文件，并回退测试断言；共享 Projectile 不变。
- 触发升级的条件：发现外部资产/存档/网络消费者，或删除导致 Frost Arrows/默认普攻无法加载；此时停止交付并建立迁移方案。
- 需要人决定的问题：无；用户已授权“无用就删除”，外部引用盘点已给出可执行范围。

## 9. 交付证据

- 代码/资产 diff：删除 `BP_RangedAttackAbility.uasset` 与 `DA_RangedAttackAbility.uasset`；新增两个包不存在负向断言；保留 `DA_RangedAttackProjectile`、`BP_RangedAttackProjectileActor`、`MI_RangedAttackProjectile`。DEMO-901 Spec 补充了本次清理的历史兼容边界说明。
- 构建结果：`Build.bat ue_gasEditor Win64 Development ue_gas.uproject -WaitMutex` 通过，UBT `Result: Succeeded`，6 actions，57.53 秒。
- Automation 报告：`Saved/CombatValidation/DEMO-902-Automation/index.json`；冷启动 `Automation RunTests Combat.` 通过 63 个测试，`failed=0`，`notRun=0`。
- 资产校验报告：`Saved/CombatValidation/DEMO-902-AssetReport.json`；扫描 9 个资产，`errorCount=0`、`warningCount=0`、issues 为空。首次编辑器占用端口的运行退出码为 1，关闭无脏资产的 Editor 后复跑通过，最终报告不受该环境冲突影响。
- 文档与差异：`python -B Tools/validate_docs.py` 通过（58 Markdown、320 本地链接、0 error）；`git diff --check` 通过。
- 未执行验证及原因：Demo PIE、Dedicated/Network、Cook/Packaging、长时 Soak/Perf 未执行；本次删除无运行时行为、复制契约、地图或性能循环变更，且相关内容加载/引用已由 Foundation、全量 Automation 和资产校验覆盖。
- 剩余风险：仓库外存档或网络消费者若仍硬编码 `CombatAbility:ranged_attack_bolt`，不在当前资产引用图内；若发现此类消费者，按 Spec 回滚两项资产并建立迁移任务。共享 Projectile 的现存引用已复核为 `DA_FrostArrows` 与 `DA_DrowRangerUnit`。

## 10. 变更记录

| 版本 | 日期 | 修改 | 原因 |
| --- | --- | --- | --- |
| 0.1 | 2026-09-11 | 建立 Spec，冻结删除范围和保留 Projectile 依赖 | 用户请求清理未使用 Demo 技能 |
| 0.2 | 2026-09-12 | 完成资产删除、测试断言、构建与内容/资产验证，更新交付状态 | 形成可审查的删除证据 |

## 11. 路由、自评与 Reflect

### 路由记录

- 原始需求摘要：确认 Demo RangeAttack 技能无外部使用后删除。
- 主 Skill：`combat-skill-development`。
- 选择依据：对象是 Ability Class/DataAsset，且删除会影响 Demo 内容资产与技能回归；需执行技能专项资产引用检查。
- 备选 Skill 与排除理由：`combat-feature-development` 仅作为通用流程；`skill-creator` 不涉及。
- 路由置信度：`high`。

### 交付自评

| 维度 | 权重 | 分数（0–5） | 证据与扣分原因 |
| --- | ---: | ---: | --- |
| 需求与 AC | 20% | 5.0 | 两项未使用 Ability 资产按范围删除，四项 AC 均有证据 |
| 架构与权限 | 20% | 5.0 | 未改运行时权威入口、网络契约或共享 Projectile |
| 实现与数据 | 20% | 5.0 | UE MCP 删除与删除后引用回读闭环，负向断言已加入 |
| 验证证据 | 20% | 4.0 | 构建、全量 Automation、资产校验通过；PIE/Cook 未执行且已记录 |
| 文档与可观测性 | 10% | 4.5 | Spec、DEMO-901 历史边界和测试证据已写回 |
| 交付卫生 | 10% | 5.0 | diff check 通过，无生成文件或用户修改混入 |

- 计算总分：`4.8 / 5.0`（按权重计算为 4.75，四舍五入到一位小数）。
- 硬性封顶或未执行项：无硬性封顶；PIE、Dedicated/Network、Cook/Packaging、长时 Soak/Perf 因本任务不改运行时行为/复制契约/地图而未执行，已在测试矩阵与交付证据中说明。
- 自评结论：`EVIDENCE_SUFFICIENT`。
- 用户验收状态：`READY_FOR_REVIEW`。

### Reflect 与调优

- 观察与证据：UE MCP 引用回读显示 Ability Class/DataAsset 无外部使用，而 Projectile 是共享内容；删除后旧包不存在且共享 Projectile 仍有 Frost Arrows/Drow 两个引用者。
- 根因类别：`单次实现`。
- 调整文件与预期收益：本 Spec 和内容测试增加删除边界与负向断言，避免误删共享 Projectile。
- 回归验证：Editor 构建通过；冷启动全量 63 个 Combat Automation 测试无失败；资产校验 9 个资产 0 error/0 warning；文档和差异检查通过。
- 需要用户决定的问题：无。
