# ATTR-001 DOTA2 三围与派生战斗属性

> Spec 版本：`0.3`
> 状态：`已验收`
> Owner：Codex
> 创建日期：2026-09-14
> 关联进度台账：`Doc/CombatSystem/00-Project/00-01-Progress-Tracker.md`
> 风险等级：`L1`

## 0. Intake 与 Gate 记录

- 用户请求：参考 DOTA2 完善属性系统，添加力量、智力、敏捷三大基础属性，并完善其他属性。
- 附件解释：`参考`；无用户附件。DOTA2 资料只用于确定三围的派生数值，Combat 的服务器权威、GAS 唯一属性源和既有网络边界保持不变。
- 已读取入口：`agent.md`、`README.md`、`00-01-Progress-Tracker.md`、`00-03-Test-Plan.md`、`00-04-Decisions-Gaps.md`、`00-05-AI-Native-Development-Workflow.md`、`10-01-Scope-Architecture.md`、`10-04-Modifier-Attributes-Motion.md`、`10-05-Damage-Heal.md`、`10-08-Data-Network-Observability.md`、`combat-task-router`、`combat-feature-development`。
- 主 Skill：`Skills/combat-feature-development/SKILL.md`
- 备选 Skill 与排除理由：`combat-skill-development` 不适用，本次不创建或修改可施放 Ability/DataDriven Action；`spreadsheets` 不适用。
- 路由置信度：`high`
- F0 结论：`GO`
- F0 依据：现有 `UCombatAttributeSet` 和 `FCombatUnitBaseStats` 已是唯一属性入口，缺少三围与派生关系；需求边界可在现有 GAS/UnitData/View 公共入口中闭合，不需新增旁路。
- F1 结论：`APPROVED`
- F1 审查人：用户（本轮对话明确验收）
- F1 审查版本：`0.3`
- F1 计划审查证据：用户已验收冻结三围与派生公式、初始化顺序、动态 GE 修改、复制/HUD 快照、兼容默认值、回滚和测试矩阵；不改变 Damage/Heal/Order/RPC 权威入口。用户复核时要求优先迁移实际旧数据；本处核实为冗余初始化重算，删除该调用不改变已验收的公式或数据语义。
- Build 解锁：`已解锁`；进入实现与验证阶段。
- F1 重审条件：若新增通用属性、Universal 英雄、天赋成长、属性网络载荷以外的契约，或改变现有公式/版本/迁移策略，先递增 Spec 版本并重新审查。
- F2 结论：`PASS`
- Push-Ready 结论：`READY`
- 验证：删除冗余初始化重算后，Editor 增量构建、属性核心测试、HUD 属性测试与文档校验均通过；全量 Combat Automation、资产校验、Dedicated 安装版双客户端 smoke 沿用此前通过的证据，未在删除冗余调用后重跑。
- 用户验收：2026-09-15 用户明确确认“验收完成，提交吧”，验收实现结果并授权本地 Git 提交。
- 未执行：安装版 UE 不支持 `ue_gasServer/ue_gasClient` Target，源码引擎 Target 全量重建在 3871 actions 时中止；真实交互 PIE 未单独执行，本任务未修改二进制 UI 资产。

## 1. 目标与范围

### 目标

1. 在 UnitData 和 GAS AttributeSet 中增加 Strength、Agility、Intelligence 三个基础属性。
2. 按 DOTA2 当前常用换算，将三围稳定地聚合到最大生命、生命恢复、护甲、攻击速度、最大法力、法力恢复、魔法抗性和主属性攻击力。
3. 让三围通过既有 ActiveGE/ASC 体系动态生效，并向拥有者 HUD 提供完整只读属性快照。
4. 保留已有 UnitData 属性字段语义；旧资产三围默认为 0，旧战斗数值不因迁移发生变化。

### 范围

- `FCombatUnitBaseStats` 增加三围和主属性选择，提供合法性校验与中文配置元数据。
- `FCombatNumericPolicyV1` 增加三围换算常量，公式版本升级为 2。
- `UCombatAttributeSet` 增加三围 GAS 属性、复制和动态派生重算；派生结果仍是 ASC 聚合属性唯一来源。
- `ACombatUnitCharacter::InitializeFromUnitData` 按安全顺序配置种子值、三围和派生属性；派生上限确定后按统一规则填满初始生命/法力，不保留旧资产专用分支。
- `FCombatHUDOwnerView` 增加三围、主属性以及现有 AttributeSet 全量战斗属性只读字段，展示 schema 升级。
- 增加属性正向、边界、动态 Modifier 和旧默认值回归测试；同步架构/决策/使用文档。

### Non-Goals

- 不加入 DOTA2 Universal 第四属性、天赋树、属性成长或等级自动加点。
- 不改变 Damage/Heal、Armor 伤害公式、攻击时序、资源事务、RPC、GameplayTag 和物品契约。
- 现有 5 个 UnitData 的零三围是合法配置，本轮不变更其战斗数值；后续确有旧数据变更时优先迁移资产，不新增旧版本运行时分支。

## 2. 当前事实与依据

- 相关 DDD：`10-04` 当前列出基础战斗属性但没有三围依赖；`10-08` owner-only HUD 当前只采样八个属性；ADR-020 冻结 Formula v1；ADR-047/055 冻结展示与发布边界。
- 代码事实：`CombatAttributeSet.h/cpp` 维护 24 个 GAS 属性（含三围与 Meta）和 clamp/RepNotify；`CombatUnitCharacter.cpp` 批量初始化 UnitData 属性并在派生上限确定后填充资源；`CombatHUDViewTypes.h`/`CombatHUDView.cpp` 定义 owner-only 全量快照和采样；`CombatNumericPolicy.h` 固定 FormulaVersion=2。
- 当前测试/日志证据：`Combat.Core.Attributes.InitializationRegenAndLifecycle`、`Combat.Core.Transactions.DamageAndHealPipeline`、`Combat.UI.HUD.*` 已覆盖旧属性初始化、clamp、伤害治疗和 HUD 来源。
- 外部参考：DOTA2 当前资料给出 Strength 每点 +22 最大生命、+0.1 生命恢复；Agility 每点 +1 攻击速度、+0.167 护甲；Intelligence 每点 +12 最大法力、+0.05 法力恢复、+0.1% 魔法抗性；主属性每点 +1 攻击伤害。

## 3. 行为与契约

### 主流程

服务器初始化 UnitData 时先缓存非三围种子值，再以三围 GAS 属性写入 Strength/Agility/Intelligence；AttributeSet 在聚合变化后以缓存种子和当前三围重算派生属性的 GAS base value。派生属性仍可叠加已有 Modifier GE。主属性只影响 AttackDamage，不影响其他三围收益。

换算公式（Formula v2）：

- `MaxHealth = BaseMaxHealth + Strength * 22`
- `HealthRegen = BaseHealthRegen + Strength * 0.1`
- `Armor = BaseArmor + Agility / 6`（约 0.166667）
- `AttackSpeed = BaseAttackSpeed + Agility`
- `MaxMana = BaseMaxMana + Intelligence * 12`
- `ManaRegen = BaseManaRegen + Intelligence * 0.05`
- `MagicResist = BaseMagicResist + Intelligence * 0.001`
- `AttackDamage = BaseAttackDamage + PrimaryAttributeValue`

所有结果继续经 `PreAttributeChange` 的 Numeric Policy 限幅；当前 Health/Mana 不因上限提高而自动补满，降低上限时沿既有逻辑裁剪。

### 状态转换

属性无独立生命周期；Unit Dead/Respawning 期间保留三围和 ActiveGE，Respawn 只按既有生命入口恢复当前资源。Modifier 移除后 GAS 聚合回退，三围派生值随聚合回调恢复。

### 输入、输出与数据约束

三围必须是有限且非负值，受 `MaxAbsoluteValue` 限制；主属性必须是 Strength/Agility/Intelligence。旧 UnitData 的新增字段采用零三围、Strength 主属性默认值。派生属性不提供客户端写入口。

### 权威边界与权限

UnitData 仅是服务器初始化模板；ASC Attribute/ActiveGE 是最终三围与派生属性来源。服务器处理所有初始写入和 GE 变化；客户端只消费复制属性或 owner-only HUD 快照。HUD/Widget 不直接读取或写入 ASC。

### 失败、取消、过期、死亡、EndPlay 与重复请求

非法三围或主属性使 UnitData 校验/初始化失败，不产生部分有效初始化。属性 GE 失败沿既有 GAS 失败语义，不新增重试或旁路。三围没有异步 Handle；Unit EndPlay 清理 ASC 与 Modifier 的既有顺序不变；同一 DefinitionId 初始化保持幂等。

### 兼容、版本与迁移

公式版本从 1 升为 2，发布契约与 Numeric Policy 同步；现有 UnitData 三围为 0 时，八项派生值就是原基础字段，不需要旧资产重算兜底。新单位统一在派生属性确定后填满当前资源。展示 schema 从 7 升为 8，客户端与服务器需同版本。回滚时成组撤销新增字段/派生重算/HUD 字段和 Formula v2 版本声明。确需改变旧资产内容时直接迁移并保存资产，不在运行时保留旧版本分支。

## 4. 实施计划

| 文件/资产 | 变更 | 原因 | blast radius |
| --- | --- | --- | --- |
| `Source/Combat/Combat/Core/CombatTypes.h` | 增加 `ECombatPrimaryAttribute` | UnitData/HUD 共用主属性枚举 | Unit 配置与展示编译依赖 |
| `Source/Combat/Combat/Core/CombatNumericPolicy.h` | 增加三围常量，FormulaVersion=2 | 集中公式与数值边界 | 发布契约、测试期望 |
| `Source/Combat/Combat/Attributes/CombatAttributeSet.h/.cpp` | 增加三围 GAS 属性、RepNotify、种子配置和派生重算 | 唯一属性来源与动态 GE 支持 | 初始化、Modifier、复制 |
| `Source/Combat/Combat/Unit/CombatUnitCharacter.cpp` | 初始化种子与三围，统一填满初始资源；删除冗余初始化重算 | 避免中间值覆盖和无必要的旧资产兜底 | 所有 Unit 初始化 |
| `Source/Combat/Combat/View/CombatHUDViewTypes.h/.cpp` | owner-only 快照增加三围和全量属性 | 让 UI 能完整展示“其他属性” | 展示 schema/网络兼容 |
| `Source/Combat/Combat/View/CombatUnitViewComponent.h` | PresentationSchemaVersion 7→8 | 复制结构变更 | 客户端/服务器版本契约 |
| `Source/Combat/Combat/UI/CombatHUDWidget.cpp` | 属性详情显示三围和完整关键属性 | 用户可见的属性完善 | HUD 文本表现 |
| `Source/Combat/Combat/Tests/CombatCoreTests.cpp` | 三围公式、边界、动态变化测试 | 防止公式和聚合回归 | Combat.Core 自动化 |
| `Doc/CombatSystem/10-Architecture/10-04-Modifier-Attributes-Motion.md` | 增加三围和派生公式 | 同步行为文档 | 文档校验 |
| `Doc/CombatSystem/10-Architecture/10-08-Data-Network-Observability.md` | 更新 owner-only 字段/版本 | 同步网络展示契约 | 客户端协议说明 |
| `Doc/CombatSystem/00-Project/00-04-Decisions-Gaps.md` | 增加 ADR-058 | 记录公式、迁移和版本决策 | 领域契约 |
| `Doc/CombatSystem/00-Project/00-01-Progress-Tracker.md` | 追加 ATTR-001 实时状态 | 交付台账唯一来源 | 流程证据 |

## 5. 验收标准（AC）

- [x] AC-01：UnitData 可配置 Strength/Agility/Intelligence 与主属性；非法值被拒绝，旧资产默认值保持原属性结果。
- [x] AC-02：服务器 GAS AttributeSet 按 Formula v2 计算八项派生属性；三围通过 ActiveGE 动态变化时派生结果同步变化，Modifier 叠加和移除不残留。
- [x] AC-03：三围与全量属性复制使用既有 RepNotify/ASC 入口；owner-only HUD 快照可读取新字段，详情文本显示关键三围/派生属性，非拥有者无隐私泄漏。
- [x] AC-04：直接相关 Combat Automation、Editor 构建、文档校验和 delivery Gate 通过；未执行层级如实记录。

## 6. Definition of Done

- [x] 行为或可执行逻辑变化已用相关测试/Golden Case 验证；核心属性测试 1/1、全量 Combat 85/85 为 Green。
- [x] 实现通过直接测试，且没有绕过公共权威入口。
- [x] 删除冗余初始化重算后，零三围与非零三围的初始化资源及派生值回归通过。
- [x] 按风险完成 Editor、Automation、Dedicated 与容量验证；Server/Client Target 和真实交互 PIE 的未执行原因已记录。
- [x] 10-04、10-08、00-04 与公开中文说明已同步。
- [x] `git diff --check` 通过，未混入用户修改或生成文件。

## 7. 测试矩阵与命令

| 层级 | 用例/命令 | 预期证据 | 结果 |
| --- | --- | --- | --- |
| 文档/本地工具 | `python Tools/validate_docs.py`；`PYTHONUTF8=1 python -m unittest discover -s Tools/Tests -p 'test_validate_docs.py' -v`；`git diff --check` | 74 Markdown、379 本地链接通过；工具单测 16/16；差异空白检查通过 | 通过 |
| Pure/Unit | `Combat.Core.Attributes.PrimaryAndDerivedAttributes` | 三围公式、边界、动态 GE、主属性切换 | 通过（1/1） |
| UI Projection | `Combat.UI.HUD.PrimaryAndFullAttributeProjection` | owner-only 三围、资源、攻防、恢复、增幅和距离快照 | 通过（1/1） |
| World Automation | `Automation RunTests Combat.` | 新旧属性、Modifier 动态回退、全量 UI/网络回归 | 通过（85/85） |
| PIE / Blueprint | 全量 Automation 中 `Combat.UI.HUD.BlueprintLayoutAndLifecycle`；未单独启动 PIE | HUD Blueprint 加载、布局与生命周期断言 | 通过（Automation）；真实交互 PIE 未执行 |
| 资产校验 | `UnrealEditor.exe -run=CombatAssetValidation` | 29 assets、0 error、0 warning | 通过 |
| Network / Dedicated | `Tools/RunDedicated.ps1 -InstalledEditor -TimeoutSeconds 120 -Port 7863` | Schema=8；Server/Client owner-only 快照、64 Unit/256 Modifier、移动与预算 | 通过 |
| 资产盘点 | 编辑器只读加载 5 个 UnitData | 全部三围为 0、主属性为 Strength；当前配置无需内容迁移 | 通过；`Saved/ATTR001/asset-attrs.json` |
| 初始化收口回归 | 删除冗余重算后重跑 Editor、属性核心和 HUD 测试 | 零三围与非零三围均完整初始化，动态 GE 行为不变 | 通过（核心 1/1、HUD 1/1） |
| Server/Client Target | 安装版 `Build.bat ue_gasServer/ue_gasClient` | 安装版引擎明确返回不支持；源码引擎全量重建已中止 | 未执行（环境限制） |
| Soak / Perf | 本次无周期/实体结构新增；沿用既有容量预算 | N/A（说明无新增压测语义） | N/A |

## 8. 风险、回滚与升级

- 风险：派生属性重算可能与瞬时 GE 顺序交错；通过初始化顺序、重算保护和动态 Modifier 测试约束。Formula v2 会要求同版本客户端/服务器。
- 回滚方式：成组恢复 `CombatAttributeSet`、UnitData、HUD schema、FormulaVersion 和文档/测试；旧资产无需改写。
- 触发升级的条件：发现 DOTA2 数值版本不一致、需要 Universal/属性成长，或派生属性无法在 GAS base/ActiveGE 中保持唯一来源。
- 需要人决定的问题：无；本 Spec 采用 DOTA2 当前公开换算，用户可在验收时调整数值平衡。

## 9. 交付证据

- 代码/资产 diff：`Source/Combat/Combat/Attributes/CombatAttributeSet.*`、`Core/CombatTypes.h`、`Core/CombatNumericPolicy.h`、`Unit/CombatUnitCharacter.cpp`、`View/CombatHUDView*`、`UI/CombatHUDWidget.cpp`、`Release/CombatReleaseContract.h`、`Core/CombatRngSubsystem.h` 及对应测试/文档；未修改二进制资产。
- 构建结果：安装版 UE 5.8 Editor Target 首次 18/18 成功，测试场景修正后增量 4/4 成功；日志见 `Saved/ATTR001/`。
- Automation/PIE/Dedicated 报告：删除冗余初始化重算后，核心与 HUD 用例各 1/1，通过报告见 `Saved/ATTR001/Automation-Core-Editor-Final2/` 与 `Saved/ATTR001/Automation-HUD-Editor-Final2/`。此前全量 `Combat.` 85/85、资产校验 29/29、Dedicated smoke Server/Client/移动/预算 Pass；报告和日志见 `Saved/ATTR001/` 与 `Saved/UEEnvironment/Dedicated-Installed/`，删除冗余调用后未重跑这些层级。
- 用户验收与提交授权：2026-09-15 用户确认“验收完成，提交吧”；本轮仅回写验收状态并复核文档、差异与交付 Gate，不新增 UE 运行时验证结论。
- 未执行验证及原因：安装版 UE 不支持独立 `ue_gasServer`/`ue_gasClient` Target，源码引擎首次全量 3871 actions 重建在 64 actions 后中止。真实交互 PIE 未单独执行，因为本次未修改二进制 UI 资产，相关 Blueprint 生命周期已由 Automation 覆盖。
- 剩余风险：源码 Server/Client Target 尚未完成编译；三围新增复制字段需在源码目标或后续 cook/打包流程中再次确认跨版本部署。

## 10. 变更记录

| 版本 | 日期 | 修改 | 原因 |
| --- | --- | --- | --- |
| 0.1 | 2026-09-14 | 初稿；F0 GO | 建立三围与派生属性实现边界 |
| 0.2 | 2026-09-14 | 撤回自动 F1 和 Build 解锁，等待用户验收 Spec | 遵循用户要求的 Spec-first 流程 |
| 0.3 | 2026-09-14 | 用户验收 Spec，批准进入实现 | 按批准范围开始开发 |
| 0.3 | 2026-09-15 | 用户验收实现结果并授权本地提交；补充最终复测与此前验证的证据边界 | 验收归档，不改变已批准范围 |

## 11. 路由、自评与 Reflect

### 路由记录

- 原始需求摘要：参考 DOTA2 完善属性系统，添加力智敏三大基础属性，完善其他属性。
- 主 Skill：`combat-feature-development`
- 选择依据：目标是修改 Combat C++/DataAsset/View/测试的可执行功能；不涉及 Ability 内容专项。
- 备选 Skill 与排除理由：`combat-skill-development` 不适用；`spreadsheets` 不适用。
- 路由置信度：`high`

### 交付自评

| 维度 | 权重 | 分数（0–5） | 证据与扣分原因 |
| --- | ---: | ---: | --- |
| 需求与 AC | 20% | 4.8 | AC-01–04 已覆盖；Server/Client Target 与真实交互 PIE 留有环境边界 |
| 架构与权限 | 20% | 4.6 | GAS/服务器权威与 owner-only 投影复用既有入口 |
| 实现与数据 | 20% | 4.7 | 三围、八项派生属性、动态 GE 与旧资产默认值均有代码和测试证据 |
| 验证证据 | 20% | 4.2 | Editor/Automation/资产/Dedicated 全绿；Server/Client Target、真实交互 PIE 未执行 |
| 文档与可观测性 | 10% | 4.8 | Formula v2、网络快照 schema 8、ADR 与进度台账已同步 |
| 交付卫生 | 10% | 4.8 | 文档、差异、Gate 通过；未修改二进制资产，用户已授权本地提交 |

- 计算总分：`4.6/5.0`
- 硬性封顶或未执行项：Server/Client Target 与真实交互 PIE 未执行，原因已在第 7、9 节记录；不影响已完成的 Editor/Automation/Dedicated 安装版证据。
- 自评结论：`READY_FOR_REVIEW`
- 用户验收状态：`已验收`（2026-09-15）；用户确认“验收完成，提交吧”，授权本地 Git 提交。

### Reflect 与调优

- 观察与证据：初始属性集合已覆盖大量战斗数值，但缺少三围依赖和统一派生重算入口；复核后确认零三围资产不需要额外兼容重算。
- 根因类别：`领域契约`
- 调整文件与预期收益：本任务新增 ADR-058 与 Formula v2 文档，避免局部公式漂移。
- 回归验证：删除冗余初始化重算后核心 1/1、HUD 1/1 通过；此前全量 `Combat.` 85/85、资产校验 29/29、Dedicated smoke Schema=8 Pass，未在最后一次修改后重跑。
- 需要用户决定的问题：无。
