---
name: combat-skill-development
description: "在 ue_gas Combat 中落地一个可验收的 GAS 技能，处理 Ability、DataAsset、Modifier、Projectile、Thinker、蓝图、测试和文档的完整交付。"
---

# Combat 技能开发

当用户请求新增、迁移或修改一个 Combat/GAS 技能时使用本 Skill。它负责实际实现和验收，不只输出技能设计建议；纯规则咨询、只读代码解释或非 Combat 功能不使用它。

本 Skill 是 [combat-feature-development](../combat-feature-development/SKILL.md) 的领域专用执行层。先遵守通用功能流程的 F0/F1/F2、Spec、权限和交付规则，再执行以下技能步骤。

本 Skill 只在当前仓库内生效，唯一来源为 `Skills/combat-skill-development/`；不要复制或安装到用户级 Codex Skill 目录。

## 输入、前置条件与输出

- 输入：技能需求、目标/队伍关系、施法和效果规则、现有技能/资产、验收场景及用户明确的玩法选择。
- 前置条件：确认当前工作区包含 `ue_gas.uproject`，完整阅读 `agent.md` 和下列必读上下文；先检查已有 diff，不覆盖用户修改。
- 输出：技能 DefinitionId/资产路径、公共入口选择、行为与服务器权威边界、修改文件、AC/Gate 结果、测试报告、未执行项、迁移/兼容说明、剩余风险和用户验收状态。

## 工具与证据

- 代码和文档定位使用 `rg`、源码、DataAsset 定义和现有 `Combat.*` 测试；复杂调用链按通用 Skill 的 CodeGraph 规则处理。
- 蓝图、DataAsset、关卡和 PIE 优先使用 UE MCP 的 Read → Plan → Mutate → Verify → Record；不可用时记录降级方式和仍需补跑的验证。
- 运行时验证按风险选择 C++ 构建、Automation、资产校验、PIE、Server/Client Target 和 Dedicated；所有实际命令、结果与报告路径写回技能 Spec。

下列项目路径均相对当前工作区根目录。先用 `ue_gas.uproject` 和 `agent.md` 确认目标仓库；在其他项目中调用时先核实目标，不自动修改另一份 checkout。

## 必读上下文

开始前完整阅读仓库根目录 `agent.md`，再读取：

- 当前状态：`Doc/CombatSystem/00-Project/00-01-Progress-Tracker.md`
- AI-Native 流程：`Doc/CombatSystem/00-Project/00-05-AI-Native-Development-Workflow.md`
- Ability 与目标：`Doc/CombatSystem/10-Architecture/10-03-Ability-Targeting-Blueprint.md`
- Damage/Heal：`Doc/CombatSystem/10-Architecture/10-05-Damage-Heal.md`
- Attack/Projectile/Thinker：`Doc/CombatSystem/10-Architecture/10-06-Attack-Projectile-Thinker.md`
- 技能扩展契约：`Doc/CombatSystem/20-Content/20-03-M8-Public-Extension-Guide.md`
- 技能模板检查：`Doc/CombatSystem/20-Content/20-02-M6-Skill-Template-Checklist.md`
- 测试准入和决策：`Doc/CombatSystem/00-Project/00-03-Test-Plan.md`、`Doc/CombatSystem/00-Project/00-04-Decisions-Gaps.md`

使用现有示例作为纵向切片参考：`Doc/CombatSystem/20-Content/20-01-Example-Skills.md`。修改蓝图、DataAsset、地图或 PIE 时，优先走 UE MCP 的 Read → Plan → Mutate → Verify → Record；MCP 不可用时按降级流程记录证据。

## 实现步骤

### 1. 收集技能契约并建立 Spec

从用户请求提取技能名称、目标类型、队伍关系、施法阶段、消耗/冷却提交点、等级、special、伤害/治疗/状态、范围、持续时间、Projectile/Thinker/Aura/Motion 需求、表现、失败路径和验收场景。确认 `DefinitionName` 使用唯一 `lower_snake_case`，并确认是否影响 `combat_v1_rc1` 的 Tag、事件、公式或版本契约。

从 `Doc/CombatSystem/Specs/_template.spec.md` 创建任务 Spec，写清主流程、状态转换、服务器/客户端边界、快照点、正常/取消/过期/死亡/EndPlay/重复请求、文件/资产、AC、DoD、测试、回滚和需要用户决定的问题。可继承现有资产和公开默认配置，并在 Spec 记录依据；只有无法从上下文确定、会改变玩法契约的选择才向用户澄清，同时继续不依赖该选择的工作。

### 2. 选择最小公共实现

按以下顺序选择实现方式，并把选择写入 Spec：

1. **DataDriven Action**：Damage、Heal、ApplyModifier、Spawn Projectile、Thinker、Aura、Motion 等可由 Ability Data 公共 Action 表达时优先使用它。
2. **自定义 Ability**：只有现有 Action 无法表达自定义施法阶段时，才派生 `UCombatGameplayAbility`；使用 `ReceiveSpellStart`、`ReceiveChannelTick`、`ReceiveChannelFinish` 等公开回调，目标复核和生命周期仍由公共层管理。
3. **Modifier Runtime**：普通属性、叠层、周期和驱散优先用现有 DataAsset 配置；只有需要自定义有状态 Hook 或法球逻辑时，才派生 `UCombatModifierRuntime`，结构修改使用 deferred/public API。
4. **Projectile/Thinker/Aura/Motion**：使用现有 Subsystem/Component 和稳定 Handle；不为单个技能增加 registry、Actor Timer 或 Transform 旁路。

若公共入口无法表达需求，记录具体缺口、替代方案、blast radius、迁移和版本影响；按通用功能流程判断是否需要用户决定，已授权的兼容扩展可继续执行。

### 3. 先建立失败证据

从 Spec 的 AC 生成至少一条成功路径和一条失败/边界路径。先建立或定位对应的技能行为测试，检查可观察事件、稳定 FailureTag、属性实际变化和清理结果；先运行新增测试并记录实际 Red 原因。使用已有测试调整数值或补测已存在实现时，如实记录验证顺序。

### 4. 实现数据、行为和表现

- 先创建或修改 Ability/Modifier/Projectile/AbilitySet DataAsset，保持 Class → Data 单向身份、PrimaryAssetId 唯一、special 分级长度合法、中文 `DisplayName`/`ToolTip`、单位和范围元数据完整。
- 平衡数值进入 DataAsset special；代码只实现公共规则和校验，不硬编码伤害、距离、持续时间或资源消耗。
- Ability 的 Cost/Cooldown 通过统一 commit 阶段提交；同一 ActivationId 至多提交一次。目标在服务器激活前和关键阶段重新校验，客户端只提交请求。
- Damage/Heal 只调用公共 Subsystem；不要直接写 Health、模拟负治疗、从客户端接收 Amount/命中列表或在蓝图重算抗性/阵营。
- DOT/HOT、Channel、Think、Expire、Aura reconcile 和延迟效果使用 Combat Scheduler；连续 Projectile/CharacterMovement 可逐帧推进，但不承载周期 gameplay 计时。
- 按公共管线既定的 SpellStarted、AttackRecord/attack point 或 Projectile Spawn 快照点保存来源、目标策略、等级 special 和 OnHitActions。普攻弹体通过 AttackHandle 请求 `FinalizeAttack`，技能弹体使用公共 Impact Action；结束保持 exactly-once。客户端预测 Projectile 只负责可丢弃视觉。
- 强制位移使用 `UCombatMotionComponent`；表现/UI 只消费 View、Combat Event、GameplayCue 和 Presentation 数据。
- 源码新增或实质修改的注释使用中文，说明业务约束、时序、权限、失败和清理；公开蓝图字段、函数和参数提供中文说明。

### 5. 验证技能与资产

实现后运行直接测试确认 Green，再按风险运行：

- C++：UE 5.8 Editor Development 构建和直接相关 `Combat.*`。
- DataAsset/蓝图：编译、保存、回读、`CombatAssetValidation` 和 `Combat.ContentExtension.*`。
- PIE：技能施放、目标无效、资源不足、取消、死亡/复活和重复请求。
- Network/Dedicated：涉及复制、RPC、Owner、Projectile reconcile 或跨端行为变化时运行 Server/Client Target 和 Dedicated 双客户端；纯数值配置按 `agent.md` 的资产/行为矩阵验证，PIE 不替代必需的 Dedicated 证据。
- 旁路扫描：`SetHealth`、`SetActorLocation`、Actor Timer、客户端命中/伤害结算和自建队伍判断。

所有命令、结果、日志/报告路径和未执行原因写入 Spec 的交付证据。校验工具或文档也变更时运行 `python3 -B Tools/validate_docs.py`；修改校验脚本时再运行 `python3 -B -m unittest discover -s Tools/Tests -p 'test_validate_docs.py' -v`。

### 6. 对抗审查、交付和回写

用独立于实现推理的上下文检查：技能身份、目标和权限、commit 时序、快照边界、事件 exactly-once、旧 Handle/LifeGeneration、Death/EndPlay、客户端权限、性能和公开配置。最多三轮修复/复验；不收敛则写 Gap Report 并升级。

完成适用的 Push-Ready 六层后，将技能 Spec、验证证据、当前台账、相关专题和 `00-04` 决策同步。用户验收前状态为 `READY_FOR_REVIEW` 或台账允许的对应状态；只有用户明确确认后才写“已验收”。不自动提交、推送或创建 PR。

## 技能交付输出

最终输出必须说明：技能 DefinitionId/资产路径、Ability/Modifier/Projectile/Thinker 选择、行为与服务器权威边界、修改文件、AC 与 Gate 结果、实际测试和报告、未执行层级及原因、兼容/迁移、剩余风险和用户验收状态。
