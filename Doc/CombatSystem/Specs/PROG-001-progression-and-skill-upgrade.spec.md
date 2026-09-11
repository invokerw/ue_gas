# PROG-001 等级经验与技能升级

> Spec 版本：`0.4`
> 状态：`已验收`
> Owner：Codex
> 创建日期：2026-09-11
> 关联进度台账：[00-01](../00-Project/00-01-Progress-Tracker.md)
> 风险等级：`L1`

## 0. Intake 与 Gate 记录（按 DOC-008 补录）

- 用户请求：参考 Dota2 经验等级，完善经验升级、技能加点和底部 HUD 等级/经验显示。
- 附件解释：用户提供的 HUD 截图属于视觉参考，影响技能图标上方的 `+` 按钮位置；不改变服务器权威和数值契约。
- 已读取入口：`agent.md`、进度台账、10-01/10-12、00-04、任务路由 Skill、功能开发 Skill。
- 主 Skill：`combat-feature-development`
- 备选 Skill 与排除理由：`combat-skill-development` 仅适用于新增/迁移具体 GAS 技能，本次没有新增技能定义。
- 路由置信度：`high`
- F0 结论：`GO`
- F0 依据：需求涉及运行时成长、服务器权限、复制 View 和 HUD，需要完整功能流程。
- F1 结论：`APPROVED`（按 DOC-008 补录；原实现前已冻结本 Spec 的范围与 AC）
- F2 结论：`PASS`（代码、蓝图、自动化、开发命令、资产校验与真实 PIE 加点链路均已复核；Dedicated/Client 受安装版引擎限制）
- Push-Ready 结论：`READY`（用户已完成 review）
- 验证：Editor 构建、`Combat.` 63/63、HUD 4/4、Progression 4/4（含开发命令）、资产 10/10、真实 PIE 等级/经验/技能点/按钮点击、文档校验和 `git diff --check`；完整命令与限制见第 7、9 节。
- 未执行：Dedicated Server/Client 和 Soak 尚未完成；安装版 UE 5.8 的 Server/Client Target 被 UBT 明确拒绝。
- 2026-09-11 收尾开工：继续使用仓库 `Skills/combat-feature-development/SKILL.md`（路由置信度 high），已回读 `agent.md`、两项路由/开发 Skill、10-12、原 Spec 与真实 UE 资产；附件仍仅为视觉参考。preflight 已通过。开发验证由 Codex 完成，不转交用户代跑。
- 收尾结果：升级条已完整放入技能槽可命中区域，保留 700×156 主面板及既有图标/资源条位置；主蓝图已提供经验数值与技能点。已完成实际 PIE 点击、蓝图保存回读和资产校验；源码版三 Target 因需全量引擎重编译未采用，安装版三 Target 受发行版限制。
- 2026-09-11 开发命令补充开工：用户要求增加快速加经验入口。继续使用 `Skills/combat-feature-development/SKILL.md`，已回读 `agent.md`、任务路由 Skill、主 Skill、README、进度台账、10-08/10-12 与本 Spec；附件仍是 HUD 视觉参考，不是命令语义约束。F0=GO，计划在现有 `CombatDebugSubsystem` 注册开发命令，只复用 `UCombatProgressionComponent::AddExperience`，默认当前玩家主控单位并支持对象名/UniqueID 指定目标；客户端不直接修改成长状态。

## 1. 目标与范围

### 目标

为 Combat Unit 增加服务器权威的 Dota 风格累计经验、英雄等级和技能点，并把等级、经验环、技能点与技能加点按钮接入底部 HUD。

### 范围

- `UCombatProgressionComponent` 保存累计经验、英雄等级、技能点，使用 1→2 为 200、之后每级递增 100 的累计阈值，默认上限 30 级。
- 单位定义配置初始等级、等级内经验和击杀经验奖励；致死伤害完成死亡转换后将奖励给实际击杀者。
- 每次跨等级增加一个技能点；技能升级检查服务器权限、存活、技能点、英雄等级和 Ability 最大等级。
- owner-only HUD 快照复制成长字段，技能槽在有可用加点时显示位于图标上方的 `+` 按钮；点击只发送升级请求。

### Non-Goals

物品/背包、经验范围共享、助攻分配、天赋树、技能等级的独立解锁等级和完整 Dota 平衡表不在本次范围。

## 2. 当前事实与依据

- 相关 DDD：[10-01](../10-Architecture/10-01-Scope-Architecture.md)、[10-12](../10-Architecture/10-12-Bottom-HUD-Design.md)、[00-04](../00-Project/00-04-Decisions-Gaps.md)。
- 代码事实：`CombatAbilitySystemComponent::SetCombatAbilityLevel` 已是服务器技能等级公共入口；`CombatDamageSubsystem::DealDamage` 在生命周期死亡转换后可得到唯一击杀来源；`CombatHUDWidget` 已有 `LevelText` 与 `ExperienceRing` 占位绑定。
- 当前测试/日志证据：全量 `Combat.` 自动化 62/62 通过（含 `Combat.Progression` 三项与 `Combat.UI.HUD` 四项）；Editor Development 构建通过。
- 已知限制：本轮未执行 Dedicated Server/Client 和 Soak；本机安装版 UE 5.8 不支持 Server/Client Target 构建。真实 PIE 已验证等级、经验、技能点显示和 `+` 点击升级；旧 HUD 蓝图中的可选新文字控件缺失时仍可使用等级数字与经验环，技能槽会动态补建 `+` 按钮。

## 3. 行为与契约

### 主流程

1. Unit 初始化完成技能授予后初始化成长状态。
2. 致死伤害请求成功完成 `RequestDeath` 后，读取目标 `ExperienceReward`，将奖励加入来源单位。
3. 累计经验跨过一个或多个阈值时提升英雄等级，并为每一级增加一个未使用技能点。
4. HUD 服务器采样成长状态和技能当前等级，仅向主控连接复制；客户端点击 `+` 发送 AbilitySpec 句柄，服务器调用 `SetCombatAbilityLevel`。

### 状态转换

`经验增加 ->（达到阈值）等级提升 -> 技能点 +1 -> HUD 快照更新`。死亡和复活不重置成长状态；满级后经验锁定在最高等级阈值，经验环固定 100%。

### 输入、输出与数据约束

累计阈值 `XP(n)=100*(n-1)*(n+2)/2`，因此 2/3/4 级分别为 200/500/900，30 级为 46400。经验奖励、初始等级和初始等级内经验必须为有限非负值；技能新等级不能超过英雄等级或技能定义的 `MaxLevel`。

### 权威边界与权限

成长字段只由服务器组件写入并复制；客户端只读取 owner-only View 或调用可靠 `ServerUpgradeAbility`。UI 不写 GAS 属性、不执行伤害、不推断击杀或等级。

### 失败、取消、过期、死亡、EndPlay 与重复请求

无技能点、英雄等级不足、技能未授予、技能达到上限、单位非存活或未初始化均返回稳定失败标签。重复点击由服务器逐次检查当前技能点和 AbilitySpec 等级；死亡/EndPlay 清理组件委托，复活保留等级与经验，重复死亡不会重复奖励。

### 兼容、版本与迁移

新增组件和 owner-only 字段属于 post-v1 兼容扩展；Combat 核心发布契约保持不变，展示 schema 从 4 升至 5。旧 UnitData 资产使用默认初始 1 级、0 经验、100 击杀奖励；旧 HUD Blueprint 的新增字段均为可选绑定。

## 4. 实施计划

| 文件/资产 | 变更 | 原因 | blast radius |
| --- | --- | --- | --- |
| `Source/ue_gas/Combat/Unit/CombatProgressionComponent.*` | 新增成长状态、曲线、技能点、RPC 与日志 | 建立单一服务器权威成长入口 | 所有 Combat Unit 的组件组合 |
| `CombatUnitCharacter.*`、`CombatDefinitionData.*` | 挂载组件并配置初始成长/击杀奖励 | 让单位初始化与奖励来源可配置 | UnitData 校验和初始化 |
| `CombatDamageSubsystem.cpp` | 死亡转换成功后发放击杀经验 | 保证 exactly-once 奖励 | 致死伤害结果 |
| `CombatHUDViewTypes.*`、`CombatUnitViewComponent.*` | 复制成长字段和技能可升级标志 | HUD 只消费 owner-only 快照 | 展示 schema 5 |
| `CombatHUDWidget.*`、`CombatHUDSlotWidget.*` | 显示等级/经验/技能点并提供 `+` 按钮 | 落实底部 HUD 与参考图交互 | 可选蓝图控件与运行时按钮 |
| `WBP_CombatHUD`、`WBP_CombatHUDSkill` | 技能槽内预留完整升级条，主布局补经验/技能点文字并调整父级命中范围 | 让蓝图视觉和真实鼠标点击均落地 | Demo HUD 资产 |
| `CombatDebugSubsystem.cpp`、`README.md` | 注册并说明 `combat.Debug.AddExperience <Amount> [ActorUniqueId\|Name]` 开发命令 | 为 Standalone/服务器调试提供可重复的经验升级入口 | 仅调用服务器权威成长组件，不改变正式玩法入口 |
| `CombatTags.*`、文档、测试 | 新增事件/失败标签、Spec 和决策记录 | 稳定诊断与交付追踪 | Tag schema 保持兼容新增 |

## 5. 验收标准（AC）

- [x] AC-01：累计经验曲线符合 2 级 200、3 级 500、30 级 46400，跨级每级增加一个技能点，满级经验环为 1。
- [x] AC-02：致死伤害只在死亡转换成功后发放目标定义中的击杀经验；重复死亡请求不重复发放。
- [x] AC-03：技能升级服务器校验技能点、英雄等级、技能上限和单位生命状态，并调用既有 AbilitySpec 等级入口。
- [x] AC-04：HUD owner-only 快照显示等级和经验进度，技能拥有可用技能点时在图标上方显示 `+`，按钮只提交升级请求。
- [x] AC-05：开发命令 `combat.Debug.AddExperience <Amount> [ActorUniqueId|Name]` 默认作用于当前玩家主控单位，支持按对象名或 UniqueID 指定目标，拒绝无效数量和客户端写入，并复用 `AddExperience` 的等级/技能点规则。

## 6. Definition of Done

- [x] 行为或可执行逻辑变化已用 `Combat.Progression` 自动化验证，并记录初次 Red（技能测试数据默认 `MaxLevel=1`）后修正为有效四级技能。
- [x] 实现通过直接测试，未绕过 Combat Damage、Lifecycle、AbilitySpec 或 View 公共入口。
- [x] 旧 HUD Blueprint 的可选绑定保持兼容；运行时按钮为旧技能槽蓝图提供降级布局。
- [ ] Dedicated Server/Client 和 Soak 仍待具备对应 Target 的环境复验；安装版 UE 5.8 的 Server/Client Target 构建被 UBT 明确拒绝。
- [x] 真实 Demo PIE 已冷启动验证等级 1 的 `0 / 200`、等级 2 的 `0 / 300` 与 `技能点 1`；点击技能上方 `+` 后技能点变为 0、Q 技能由等级 1 / 4 变为等级 2 / 4。
- [x] 相关 HUD、决策、README 与公开中文说明已同步。
- [x] 开发命令已在 README 记录，并通过 Editor Development 构建及相关 `Combat.Progression` 自动化验证。
- [x] `git diff --check` 通过，未混入用户已有修改；生成的 `Saved/` 与 `Intermediate/` 不纳入交付。

## 7. 测试矩阵与命令

| 层级 | 用例/命令 | 预期证据 | 结果 |
| --- | --- | --- | --- |
| 文档/本地工具 | `python3 -B Tools/validate_docs.py`、`git diff --check` | 文档校验与差异卫生 | 通过 |
| Pure/Unit | `Combat.Progression.ExperienceCurveAndPoints` | 阈值、跨级、技能点、满级边界 | 通过 |
| World Automation | `Combat.Progression.AbilityUpgradeAuthorization`、`KillExperienceReward` | 服务器加点权限、击杀奖励 | 通过 |
| Development console | `combat.Debug.AddExperience 200 [ActorUniqueId|Name]` | 默认主控单位与显式目标解析、无效参数/客户端权限边界 | 通过（`Combat.Progression.DebugAddExperienceCommand`，直接执行注册的控制台命令对象） |
| PIE / Blueprint | `Combat.UI.HUD.*` + Slate PIE | 真实 HUD 绑定、槽位按钮兼容、等级/经验/技能点显示与点击升级 | 通过（7/7 Widget/Progression Automation；Standalone PIE 点击后技能点 1→0、Q 1/4→2/4） |
| Network / Dedicated | Server/Client Target、Dedicated 双客户端 | owner-only 成长复制与 RPC | 未执行：安装版 UE 5.8 不支持 Server/Client Target，未启动联机场景 |
| Soak / Perf | 现有 64 Unit / 256 Modifier 场景 | 新组件无异常增长 | 未执行：本轮未重跑容量场景 |

## 8. 风险、回滚与升级

- 风险：经验共享、助攻和技能独立解锁规则尚未定义；现行奖励只归实际致死来源。
- 回滚方式：移除 UnitData 成长字段、Progression 组件和 HUD 兼容代码，旧 AbilitySpec 等级和物品占位不受影响。
- 触发升级的条件：需要经验共享/助攻、技能独立解锁等级或跨版本存档时，新增迁移和网络 schema 评审。
- 需要人决定的问题：用户验收时确认击杀经验是否需要范围共享，以及参考图按钮是否需要替换为项目美术资源。

## 9. 交付证据

- 代码/资产 diff：Progression 组件、UnitData、Damage、View、HUD、Tags、开发命令与测试文件。
- 构建结果：安装版 UE 5.8.2 `ue_gasEditor Win64 Development` 成功；`ue_gasServer` 与 `ue_gasClient` 已尝试，分别返回发行版不支持 Server/Client Target。已发现本机源码版 UE，但构建要求 3909 个引擎动作，在 23/3909 时取消；不将该尝试计为编译通过。
- Automation/PIE/Dedicated 报告：`Saved/Logs/ue_gas.log` 记录最终全量 `Combat.` 63 项通过（其中成长 4 项、HUD 4 项；另有 1 条既有 RHI 保留资源预算警告，命令测试的非法数量路径会按预期记录一条 Usage warning）；新增 `Combat.Progression.DebugAddExperienceCommand` 直接执行注册的命令对象，覆盖默认主控、UniqueID 和非法数量；`Saved/TaskGate/PROG-001-assets.json` 记录资产 10/10、0 错误/0 警告；Standalone PIE 通过 Slate 快照和点击回读验证升级链路。按钮验证只在 PIE 运行态写入测试夹具（等级 2、累计经验 200、技能点 1），未保存 UnitData；自然经验奖励与跨级由 `Combat.Progression` Automation 覆盖。
- 流程 Gate：`python -B Tools/task_gate.py --mode preflight --spec Doc/CombatSystem/Specs/PROG-001-progression-and-skill-upgrade.spec.md --kind feature`（PASS）；`python -B Tools/task_gate.py --mode delivery --spec Doc/CombatSystem/Specs/PROG-001-progression-and-skill-upgrade.spec.md --kind feature --report Saved/TaskGate/PROG-001-delivery.json`（PASS，36 个工作区变更文件）。工具单测 24/24、文档 57 份/320 本地链接、`git diff --check` 均通过。
- 未执行验证及原因：Dedicated/Client 与 Soak 未执行；安装版 UE 5.8 不支持对应 Target。PIE、蓝图保存回读、资产校验和文档校验均已完成。
- 开发命令验证边界：当前 UE MCP 未提供向 Editor 控制台 UI 注入任意命令的稳定接口；自动化已通过 `IConsoleManager` 查找并直接执行注册的 `IConsoleCommand`，因此覆盖命令回调和成长结果，但未进行控制台窗口键盘注入。
- 剩余风险：动态按钮依赖技能槽根节点为 Panel；若项目美术蓝图使用非 Panel 根节点，应在 Blueprint 中提供名为 `UpgradeButton` 的可选按钮。

## 10. 变更记录

| 版本 | 日期 | 修改 | 原因 |
| --- | --- | --- | --- |
| 0.1 | 2026-09-11 | 建立成长与技能升级实现及验证记录 | 用户需求 |
| 0.2 | 2026-09-11 | 补真实 HUD 蓝图、PIE 点击及网络验证；撤销过早的可验收结论 | 用户询问验收责任，回读发现旧证据缺口 |
| 0.3 | 2026-09-11 | 完成冷启动 PIE 加点点击验证；补充父槽位让出按钮命中区域的修正与发行版 Target 限制 | 交付前复核发现父级鼠标事件抢占升级按钮 |
| 0.4 | 2026-09-11 | 增加 `combat.Debug.AddExperience` 开发调试命令及使用说明 | 用户要求快速验证经验升级 |
| 0.5 | 2026-09-11 | 用户完成 review，确认 PROG-001 交付并更新为已验收 | 用户验收 |

## 11. 路由、自评与 Reflect

### 路由记录

- 原始需求摘要：参考 Dota2 经验等级，增加经验升级、技能加点、HUD 等级/经验与技能上方按钮。
- 主 Skill：`combat-feature-development`。
- 选择依据：同时修改运行时 C++、复制 View、HUD Widget 和自动化测试；没有新增单一技能行为。
- 备选 Skill 与排除理由：`combat-skill-development` 只适用于新增/迁移具体 GAS 技能，本次没有新技能定义。
- 路由置信度：`high`

### 交付自评

| 维度 | 权重 | 分数（0–5） | 证据与扣分原因 |
| --- | ---: | ---: | --- |
| 需求与 AC | 20% | 5 | 经验、等级、技能点、HUD 按钮均有 AC |
| 架构与权限 | 20% | 5 | 服务器组件、Lifecycle 后奖励、owner-only 快照与 RPC |
| 实现与数据 | 20% | 4 | 旧蓝图通过动态按钮兼容；未提供独立美术按钮资产 |
| 验证证据 | 20% | 4 | Editor 与全量 `Combat.` 62/62 Automation 通过；Dedicated 未执行，Server/Client Target 受安装版引擎限制 |
| 文档与可观测性 | 10% | 5 | Spec、ADR、Tag 日志、HUD 文档同步 |
| 交付卫生 | 10% | 5 | 差异检查与生成目录排除 |

- 计算总分：`4.6 / 5`
- 硬性封顶或未执行项：Dedicated、Server/Client Target 和 Soak 受本机安装版引擎限制或尚未重跑；不影响单机与自动化交付证据。
- 自评结论：`EVIDENCE_SUFFICIENT`
- 用户验收状态：`已验收`（2026-09-11，用户确认 review 完成）

### Reflect 与调优

- 观察与证据：原 HUD 已有占位字段和技能等级入口，最小扩展是组件 + owner-only 快照 + 槽位按钮；Automation 暴露默认技能 `MaxLevel=1` 的测试误设并已修正；冷启动 PIE 又暴露父槽位抢占按钮鼠标事件，已加入命中区域直通升级处理并回读技能点与技能等级变化。
- 根因类别：`领域契约`
- 调整文件与预期收益：新增 ADR-049 与本 Spec，明确 post-v1 成长边界和奖励归属，避免将经验共享误解为已实现。
- 回归验证：全量 `Combat.` 63/63、相关 8/8（含开发命令）、Editor 构建、资产 10/10、冷启动 PIE 点击和文档校验通过。
- 需要用户决定的问题：经验共享/助攻规则与最终按钮美术资源。
