# DEMO-903 迁移英雄默认弹体并区分霜冻之箭表现

> Spec 版本：`0.4`
> 状态：`COMPLETED`
> Owner：Codex
> 创建日期：2026-09-12
> 关联进度台账：`00-01` Post-M8 Demo
> 风险等级：`L1`

## 0. Intake 与 Gate 记录

- 用户请求：将 RangedAttack 资源放到英雄目录作为英雄默认攻击资源；为 Frost Arrows 增加独立 Projectile Actor，以区分开启和关闭 Frost Arrows 时的弹体效果。
- 附件解释：无附件（用户请求本身提供了目录迁移与视觉区分目标，不提供额外工程约束）
- 已读取入口：`agent.md`、`README.md`、`00-01-Progress-Tracker.md`、`00-03-Test-Plan.md`、`00-04-Decisions-Gaps.md`、`00-05-AI-Native-Development-Workflow.md`、`10-01-Scope-Architecture.md`、`10-03-Ability-Targeting-Blueprint.md`、`10-05-Damage-Heal.md`、`10-06-Attack-Projectile-Thinker.md`、`20-01-Example-Skills.md`、`20-02-M6-Skill-Template-Checklist.md`、`20-03-M8-Public-Extension-Guide.md`、`Skills/combat-task-router/SKILL.md`、`Skills/combat-feature-development/SKILL.md`、`Skills/combat-skill-development/SKILL.md`。
- 主 Skill：`combat-skill-development`
- 备选 Skill 与排除理由：`combat-feature-development` 作为通用流程被主 Skill 继承；`skill-creator` 不适用，因为不修改 Skill 本身。
- 路由置信度：`high`
- F0 结论：`GO`。
- F0 依据：需求涉及 Projectile DataAsset/Blueprint 资产迁移与技能法球表现隔离；已有 ProjectileData 的 ActorClass 字段和 AssetTools 移动/复制能力可表达目标，无需新增运行时结算旁路或修改发布契约。
- F1 结论：`APPROVED`。
- F2 结论：`PASS`。
- Push-Ready 结论：`READY`。
- 验证：通过 UE MCP AssetTools/ObjectTools/ActorTools 回读新旧路径、DataAsset 引用、Actor 组件材质/灯光、Blueprint 编译结果和保存状态；旧路径三项均不存在。跟进后默认 MI 的 Parent 为 `BasicShapeMaterial`、Color 为白色，默认 Light 为 `bVisible=false` 且 `intensity=0`。
- 跟进变更：用户要求将 Drow Ranger 英雄默认弹体改为白色并关闭默认弹体灯光；该变更只调整表现属性，不改变 Frost Arrows 专用蓝色弹体。
- 未执行：Dedicated/Network、Cook/Packaging、长时 Soak/Perf 未执行；本任务没有复制/RPC或运行时循环变更，相关风险已记录。

## 1. 目标与范围

### 目标

把 Drow Ranger 默认远程攻击使用的 Projectile DataAsset、Actor Blueprint 和 Material Instance 归档到英雄目录；为 Frost Arrows 创建独立的 Projectile DataAsset/Actor/Material 表现，使 AutoCast 开启时使用 Frost Arrows 弹体，关闭或不满足法球条件时仍使用英雄默认弹体。

### 范围

- 将 `DA_RangedAttackProjectile`、`BP_RangedAttackProjectileActor`、`MI_RangedAttackProjectile` 从 `Abilities/RangedAttack` 移到 `Heros/DrowRanger`。
- 将 `DA_DrowRangerUnit.attackProjectileData` 更新为英雄目录下的默认 Projectile 定义，稳定 DefinitionId 仍为 `CombatProjectile:ranged_attack_projectile`。
- 在 `Abilities/FrostArrows` 新建 `DA_FrostArrowsProjectile`、`BP_FrostArrowsProjectileActor`、`MI_FrostArrowsProjectile`；新定义使用 `frost_arrows_projectile`，新 Actor 使用独立蓝色材质和灯光颜色。
- 将 `DA_FrostArrows.attackOrbProjectileData` 更新为 Frost Arrows 专用 ProjectileData，保留攻击记录快照和公共 Projectile 子系统。
- 更新内容回归测试、示例技能文档和本 Spec，记录旧路径不存在、稳定 ID 与视觉引用关系。

### Non-Goals

- 不改变 Projectile 运动、碰撞、命中、伤害、Mana、Slow、AutoCast 仲裁、服务器权威或网络复制契约。
- 不删除默认攻击或 Frost Arrows 的 Projectile；只移动默认资源并复制一套 Frost Arrows 表现资源。
- 不新增 GameplayTag、事件 schema、公式版本、DefinitionId redirect 或新的 Projectile registry。
- 不修改技能图标、英雄模型、攻击数值或地图布局。

## 2. 当前事实与依据

- `DA_RangedAttackProjectile` 当前配置为 Tracking、速度 1200、半径 18、最大距离 1200、寿命 3 秒、`CombatProjectile` 碰撞 Profile，ActorClass 为 `BP_RangedAttackProjectileActor`。
- 当前资产引用者：`DA_FrostArrows` 与 `DA_DrowRangerUnit` 均引用 `DA_RangedAttackProjectile`；Actor Blueprint 的 `ProjectileMesh` 使用 Sphere 和 `MI_RangedAttackProjectile`，`ProjectileLight` 为青色灯光。
- `DA_FrostArrows` 的 `AttackOrbProjectileData` 在法球胜出时写入 `AttackRecord.ProjectileDataOverride`；为空或未胜出时攻击组件使用单位默认 `AttackProjectileData`。
- `UCombatProjectileSubsystem` 从 ProjectileData 的 `ProjectileActorClass` 选择服务器生成的 Actor；Actor 只负责连续运动和表现，命中仍由服务器 Projectile 子系统完成。
- 已有 `Combat.Foundation.Content.DrowRangerDemo` 只验证共享 Projectile 加载与 Drow/Frost 引用，本任务将它扩展为验证两个 Projectile 定义与 ActorClass 不同。

## 3. 行为与契约

### 主流程

1. 移动默认远程攻击三项表现/定义资源到 `Heros/DrowRanger`，AssetTools 自动修复 Drow Unit 与 Frost Arrows 的引用。
2. 复制默认 Projectile DataAsset、Actor Blueprint 与 Material Instance 到 `Abilities/FrostArrows`，给 Frost DataAsset 设置唯一 DefinitionName，并给新 Actor 设置 Frost 材质与灯光颜色。
3. 让 Drow Unit 指向英雄目录的默认 Projectile，Frost Arrows 指向 Frost 专用 Projectile；法球胜出时快照 Frost Projectile，未胜出时继续快照/使用默认 Projectile。
4. 编译保存 Blueprint，回读 DataAsset、Actor 组件与引用者，并运行内容测试、资产校验和全量 Combat Automation。

### 状态转换

`攻击开始 → Frost Arrows 合法且 AutoCast 开启 → AttackRecord 使用 Frost Projectile → 生成 Frost Actor；`
`攻击开始 → Frost Arrows 关闭/失效或未胜出 → AttackRecord 使用 Drow 默认 Projectile → 生成默认 Actor`。

### 输入、输出与数据约束

- 默认 Projectile DefinitionId：`CombatProjectile:ranged_attack_projectile`，资源路径迁移不改变身份。
- Frost Projectile DefinitionId：`CombatProjectile:frost_arrows_projectile`，只由 Frost Arrows 法球使用。
- 两个 Projectile 保持相同 Tracking/命中/速度/半径/距离/寿命配置，差异只在 ActorClass 和表现材质/灯光颜色。
- Frost Actor 的 `ProjectileMesh.overrideMaterials` 只引用 `MI_FrostArrowsProjectile`；默认 Actor 只引用英雄目录下的 `MI_RangedAttackProjectile`。

### 权威边界与权限

Projectile 选择沿用服务器 AttackRecord 快照和 `UCombatProjectileSubsystem`；客户端只通过复制的 Projectile DefinitionId/Actor 进行表现 reconcile，不接收客户端命中或伤害输入。

### 失败、取消、过期、死亡、EndPlay 与重复请求

本任务不新增异步对象或结束入口。AssetTools 移动/复制失败、Blueprint 编译失败、DefinitionId 冲突或外部引用不符合预期时停止交付；运行时取消、目标丢失、死亡和 EndPlay 继续使用既有 Projectile exactly-once 清理。

### 兼容、版本与迁移

默认 Projectile 的稳定 DefinitionId 不变；Frost 专用 Projectile 是新增内容定义，不改变 `combat_v1_rc1` 或事件/网络 schema。旧 `/Game/Combat/Demo/Abilities/RangedAttack` 目录中的默认资源迁移后不保留旧路径；如仓库外消费者硬编码旧路径，需按独立内容迁移任务处理。

## 4. 实施计划

| 文件/资产 | 变更 | 原因 | blast radius |
| --- | --- | --- | --- |
| `Content/Combat/Demo/Abilities/RangedAttack/DA_RangedAttackProjectile.uasset` | 移动到 `Content/Combat/Demo/Heros/DrowRanger/` | 归档为英雄默认攻击资源 | Drow Unit、Frost Arrows 引用修复 |
| `Content/Combat/Demo/Abilities/RangedAttack/BP_RangedAttackProjectileActor.uasset` | 移动到 `Content/Combat/Demo/Heros/DrowRanger/` | 默认攻击表现归属英雄 | 默认 Projectile Actor |
| `Content/Combat/Demo/Abilities/RangedAttack/MI_RangedAttackProjectile.uasset` | 移动到 `Content/Combat/Demo/Heros/DrowRanger/` | 默认攻击材质归属英雄 | 默认 Actor 材质引用 |
| `Content/Combat/Demo/Abilities/FrostArrows/DA_FrostArrowsProjectile.uasset` | 新建（复制默认定义） | Frost 法球需要独立 ActorClass 与稳定定义 | Frost 法球快照 |
| `Content/Combat/Demo/Abilities/FrostArrows/BP_FrostArrowsProjectileActor.uasset` | 新建（复制默认 Actor） | 区分 Frost 开启时的表现 | Frost 弹体表现 |
| `Content/Combat/Demo/Abilities/FrostArrows/MI_FrostArrowsProjectile.uasset` | 新建（复制默认材质） | 使用 Frost 专用颜色 | Frost Actor 材质 |
| `Source/ue_gas/Combat/Tests/CombatFoundationTests.cpp` | 更新路径并增加双 Projectile/Actor 断言 | 防止迁移遗漏和表现资源混用 | Demo 内容回归 |
| `Doc/CombatSystem/20-Content/20-01-Example-Skills.md` | 更新默认/Frost Projectile 目录事实 | 保持公开文档与资产一致 | 文档导航 |

## 5. 验收标准（AC）

- [x] AC-01：默认远程攻击三项资源位于 `Heros/DrowRanger`，旧 `Abilities/RangedAttack` 默认资源路径不存在，Drow Unit 引用新路径且 DefinitionId 仍为 `ranged_attack_projectile`。
- [x] AC-02：Frost Arrows 三项专用资源存在，`DA_FrostArrows` 引用 `DA_FrostArrowsProjectile`，其 DefinitionId 为 `frost_arrows_projectile`，ActorClass 与默认 Projectile 不同。
- [x] AC-03：Frost Actor 的 Mesh 使用专用 Material、Light 使用专用颜色；默认 Actor 使用白色材质且关闭灯光，开启与关闭 Frost Arrows 的表现可区分。
- [x] AC-04：`Combat.Foundation.Content.DrowRangerDemo`、`Combat.ContentExtension.*` 与全量 `Combat.*` 通过，验证 Drow 默认 Projectile、Frost Projectile 和 AbilitySet 引用关系。
- [x] AC-05：Editor Development 构建、Blueprint 编译保存回读、`CombatAssetValidation`、文档校验和 `git diff --check` 通过；未产生无意 redirector 或运行时旁路。

## 6. Definition of Done

- [x] 默认资源迁移和 Frost 专用复制通过 UE MCP AssetTools 完成，并回读新旧路径与引用者。
- [x] DataAsset 的稳定 DefinitionId、ActorClass、Material、灯光颜色和 Drow/Frost 引用均通过对象回读。
- [x] 相关 Blueprint 编译保存，Editor 构建、Automation、资产校验与差异检查完成并记录。
- [x] Spec 写回 F1/F2、Push-Ready、验证命令、未执行项、兼容边界和回滚方式。
- [x] 未混入用户修改、生成文件或不相关运行时变更。

## 7. 测试矩阵与命令

| 层级 | 用例/命令 | 预期证据 | 结果 |
| --- | --- | --- | --- |
| 文档/本地工具 | `python -B Tools/validate_docs.py`、`git diff --check` | 文档/空白检查 | 通过：59 Markdown、320 links；diff check 无错误（仅 CRLF 提示） |
| Pure/Unit | `Combat.Foundation.Content.DrowRangerDemo` | 新旧路径、稳定 ID、Drow/Frost Projectile 与 ActorClass 关系 | 通过（全量报告中的 DrowRangerDemo Success） |
| World Automation | `Automation RunTests Combat.` | Frost/默认弹体引用和既有 Combat 回归 | 通过：62 succeeded、1 succeededWithWarnings、0 failed、0 notRun |
| PIE / Blueprint | Blueprint 编译保存回读；Demo PIE | Frost 开启时生成 Frost Actor，关闭时生成默认 Actor | Blueprint 编译通过；PIE 日志记录 AutoCast=0 后 `ranged_attack_projectile`，AutoCast=1 后 `frost_arrows_projectile`，启动/停止成功 |
| Network / Dedicated | 不改复制/RPC 契约 | N/A：只改变 ProjectileData/Actor 表现引用 | 不适用 |
| Soak / Perf | 不新增运行时循环 | N/A：只替换表现 ActorClass | 不适用 |

## 8. 风险、回滚与升级

- 风险：AssetTools 移动后旧软路径残留，或 Frost DataAsset DefinitionId 与默认定义冲突；通过新旧路径存在性、引用者、对象属性和资产校验确认。
- 风险：只改变表现 ActorClass，若攻击快照仍指向默认 DataAsset，会导致视觉不区分；通过 Frost/Drow DataAsset 指针和 ActorClass 不同断言确认。
- 回滚方式：按 Git 恢复三项默认资源到旧目录，删除 Frost 三项新资源，并恢复 Drow/Frost 两个 DataAsset 的旧引用；不回滚运行时 C++ 核心。
- 触发升级的条件：发现仓库外硬编码旧路径、Frost Actor 需要新的命中/伤害语义，或必须兼容旧客户端的 Projectile DefinitionId；此时建立迁移/版本方案。
- 需要人决定的问题：无；默认使用当前青色材质保留普通攻击表现，Frost 专用弹体采用蓝色材质和灯光以满足开启/关闭区分。

## 9. 交付证据

- 代码/资产 diff：迁移默认 Projectile 三项到 `Content/Combat/Demo/Heros/DrowRanger/`；新增 Frost 三项到 `Content/Combat/Demo/Abilities/FrostArrows/`；默认 MI 改用 `BasicShapeMaterial` 白色参数并关闭默认 Light；更新 Drow/Frost DataAsset 指针、内容回归断言和示例文档。旧默认包由 UE MCP 删除；因孤立旧 DataAsset 仍被已移走 Actor 依赖，使用 UE 5.8 UnrealEditor-Cmd Python 回退脚本清理残留包并回读文件不存在。
- 构建结果：`Build.bat ue_gasEditor Win64 Development ... -WaitMutex` 通过。
- Automation/PIE/Dedicated 报告：全量 Combat Automation 通过；最新 `CombatAssetValidation` 报告为 10 assets、0 errors、0 warnings（命令行进程因已有编辑器占用 MCP 端口返回 1，但报告已写入）；Demo PIE 启动/停止 smoke 通过；Dedicated 对本任务不适用。
- 未执行验证及原因：Cook/Packaging、Dedicated/Network、长时 Soak/Perf 未执行；本任务仅调整内容引用和表现 Actor，不改变运行时结算或复制契约。
- 剩余风险：仓库外若有硬编码旧 `/Game/Combat/Demo/Abilities/RangedAttack` 默认资源路径，需要独立迁移；F2 审查前不视为用户验收。

## 10. 变更记录

| 版本 | 日期 | 修改 | 原因 |
| --- | --- | --- | --- |
| 0.1 | 2026-09-12 | 建立 Spec，冻结英雄默认 Projectile 迁移和 Frost 专用 Actor 范围 | 用户要求区分 Frost Arrows 开关时的弹体表现 |
| 0.2 | 2026-09-12 | 完成资产迁移/复制、引用更新、Blueprint/代码/文档验证并记录回退清理 | 交付前写回实际证据 |
| 0.3 | 2026-09-12 | 将 Drow Ranger 默认弹体材质改为白色并关闭默认灯光 | 用户要求普通攻击不发光 |
| 0.4 | 2026-09-12 | 切换默认 MI 到非发光 `BasicShapeMaterial` 并完成回读、PIE smoke 与资产报告 | 确保白色默认弹体不产生发光效果 |

## 11. 路由、自评与 Reflect

### 路由记录

- 原始需求摘要：迁移英雄默认远程攻击资源，并为 Frost Arrows 增加独立 Projectile Actor 表现。
- 主 Skill：`combat-skill-development`。
- 选择依据：同时涉及 Projectile DataAsset、Blueprint Actor、Material、Ability 法球引用和技能回归。
- 备选 Skill 与排除理由：`combat-feature-development` 作为通用流程；`skill-creator` 不涉及。
- 路由置信度：`high`。

### 交付自评

| 维度 | 权重 | 分数（0–5） | 证据与扣分原因 |
| --- | ---: | ---: | --- |
| 需求与 AC | 20% | 5.0 | 默认/Frost 资源归属、引用和开关路径均已落地 |
| 架构与权限 | 20% | 5.0 | 沿用 AttackRecord、公共 Projectile 子系统和服务器权威边界 |
| 实现与数据 | 20% | 5.0 | 两套 DataAsset/Actor/Material 属性与引用已回读 |
| 验证证据 | 20% | 4.5 | 构建、全量 Automation、资产校验、PIE smoke 通过；Cook/Soak 未执行 |
| 文档与可观测性 | 10% | 5.0 | 示例技能文档、测试断言和 Spec 已同步 |
| 交付卫生 | 10% | 4.5 | 仅保留用户既有改动及本任务文件；无意 redirector 未发现 |

- 计算总分：`4.85 / 5.0`。
- 硬性封顶或未执行项：Cook/Packaging、Dedicated/Network、长时 Soak/Perf 未执行；无运行时结算或网络改动，因此不阻塞内容交付。
- 自评结论：`EVIDENCE_SUFFICIENT`。
- 用户验收状态：`已验收`（2026-09-12，用户确认验收完成）。

### Reflect 与调优

- 观察与证据：当前同一 ProjectileData 同时承担 Drow 默认攻击和 Frost Arrows 法球，ActorClass/材质无法表达开关区分。
- 根因类别：`单次实现`。
- 调整文件与预期收益：迁移默认资源、复制 Frost 专用资源、更新内容测试和示例文档，使表现资源归属和技能快照边界可回读。
- 回归验证：内容测试断言 Drow 默认与 Frost 专用 Projectile 的稳定 ID/ActorClass；UE MCP 回读材质/灯光；全量 Automation、资产校验和 PIE smoke 通过。
- 需要用户决定的问题：无。
