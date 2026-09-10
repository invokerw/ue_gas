# Combat 项目 Agent 开发规则

本文是 `ue_gas` 仓库的长期开发约束，适用于整个仓库。开始任务前必须完整阅读本文；用户当前任务中的明确要求优先于本文。若确需突破规则，先说明影响，并在 `Doc/CombatSystem/00-Project/00-04-Decisions-Gaps.md` 增加或更新 ADR/Gap。

## 1. 项目事实

- 引擎基线是 Unreal Engine 5.8。
- Combat 位于 `Source/ue_gas/Combat`，当前保持在 `ue_gas` 单 Runtime Module 中。
- 核心发布契约是 `combat_v1_rc1`；发布边界由 `FCombatReleaseContract`、版本常量、自动化和文档共同保护。
- `Doc/CombatSystem/00-Project/00-01-Progress-Tracker.md` 是任务状态的唯一来源。`Doc/CombatSystem/00-Project/00-02-Implementation-Roadmap.md` 是历史 WBS 和 Gate 定义，不能用它判断当前完成度。
- `Content/Combat/Demo` 是可玩内容；`Content/Combat/Tests` 和 `Source/ue_gas/Combat/Tests` 是验证基础设施，不能把测试专用旁路带入生产玩法。

## 2. 开工前

1. 先按 [Combat 任务路由 Skill](Skills/combat-task-router/SKILL.md) 判断主 Skill；纯咨询不创建虚假 Spec。
2. 阅读 `README.md`、进度台账、架构硬约束和与任务直接相关的专题文档。
3. 修改技能或公开扩展面时，额外阅读 `Doc/CombatSystem/20-Content/20-03-M8-Public-Extension-Guide.md` 和 `Doc/CombatSystem/20-Content/20-02-M6-Skill-Template-Checklist.md`。
4. 修改单位控制、普通移动、PathFollowing、单位碰撞或服务器避让时，额外阅读 `Doc/CombatSystem/10-Architecture/10-10-Server-Authoritative-Movement-Kickoff.md`，并以 `Doc/CombatSystem/10-Architecture/10-09-Client-Server-Interaction.md` 核对当前端到端行为。
5. 修改异步对象、Handle、Delegate 或 teardown 时，额外阅读 `Doc/CombatSystem/90-History/90-16-M8-Lifecycle-Audit.md`。
6. 检查工作区状态，保留用户已有修改；不覆盖、不格式化、不回退无关文件。
7. 若仓库根目录存在 `.codegraph/`，理解或定位代码时先使用 CodeGraph；没有索引再使用 `rg` 和直接阅读源码。
8. 涉及蓝图、DataAsset、关卡或 PIE 时，优先通过 UE MCP 读取真实 Editor 状态；修改后回读、编译蓝图、保存资产并执行相应验证。MCP 不可用时记录降级方式。
9. 修改底部 HUD、拥有者展示快照或技能槽视觉时，额外阅读 `Doc/CombatSystem/10-Architecture/10-12-Bottom-HUD-Design.md` 和 `00-04` 的 ADR-047。

项目级 Skill 只允许存放并读取于仓库 `Skills/`；不得复制到 `/Users/admin/.codex/skills` 或其他用户级目录。Skill 的创建和维护使用 `skill-creator` 规则，但产物仍保留在本仓库。

## 3. 不可破坏的架构约束

### 3.1 权威与唯一数据源

- Damage、Heal、ApplyModifier、Order 执行、Attack Finalize、Projectile Hit 和死亡转换只在服务器结算。
- 客户端 TargetData 只是请求。Team、LifeState、距离、LOS、可见性、资源、冷却和命中必须由服务器复核。
- ASC Attribute/ActiveGE 聚合结果是战斗属性唯一来源；Runtime 不维护第二套最终 Armor、MoveSpeed、Health 或 Mana。
- Health/Mana 不允许从蓝图、Projectile、Modifier 或 UI 直接写入。伤害、治疗和资源变化必须走公共 API、GE 或 AttributeSet Meta Attribute。
- UI 和表现只消费 `UCombatUnitViewComponent`、FastArray View、Combat Event 或表现层数据，不能反向驱动 gameplay。

### 3.2 时序、事务与生命周期

- 前摇、引导、DOT/HOT、Modifier Think/Expire、Aura reconcile、Attack ready、追击复核和 Thinker pulse 使用 `UCombatSchedulerSubsystem`。不得新增 Actor Timer 作为周期 gameplay 权威来源。
- Projectile、CharacterMovement 和 Motion 可以逐帧推进连续运动，但 Tick 内不得顺便实现周期伤害或第二套计时语义。
- Hook 顺序保持 `Priority descending -> ApplySequence ascending`；Hook 阶段中的结构修改使用 deferred/public API。
- Attack、Order、Projectile、Thinker、Motion、Modifier 和 Schedule 必须使用稳定 Handle；异步回调校验 `Id + Generation`，涉及 Unit 生命周期时同时校验 `LifeGeneration`。
- 每个对象都必须能回答：谁创建、谁持有、谁结束、旧回调如何失效、Actor/World teardown 如何清理。
- Finish、Result、Death、OrderReleased 和对外广播保持 exactly-once。不得为了修复竞态新增第二个终结入口。
- follow-up 伤害/治疗继承 RootEventId，并遵守深度、递归 flag 和真实 AppliedAmount 规则。

### 3.3 数据、网络与扩展边界

- 可被网络、日志、UI、存档或迁移引用的定义使用 `UCombatDefinitionData` 派生 DataAsset 和稳定 `FPrimaryAssetId`。
- `DefinitionName` 使用 `lower_snake_case`，并在同一 `PrimaryAssetType` 内唯一；网络与日志不复制 DataAsset UObject 指针。
- 平衡数值优先进入 Ability/Modifier/Projectile DataAsset；公共校验、权限、提交和清理留在 C++。
- 普通技能优先组合 DataDriven Action；只有自定义施法阶段才派生 `UCombatGameplayAbility`，只有有状态 Hook 才派生 `UCombatModifierRuntime`。
- 不直接比较 TeamId、生命状态或距离来旁路 `CombatTeamSubsystem` / `CombatTargetingSubsystem`。
- 不直接 `SetActorLocation` 实现强制位移；使用 `UCombatMotionComponent`。
- 客户端 Projectile 预测只能产生可丢弃的视觉对象，不能 sweep、Damage、ApplyModifier、Finalize Attack 或生成权威事件。
- Order RPC 必须继续经过所有权、正 RequestId、批量大小、载荷、限频和重放窗口检查。

## 4. 变更流程

1. 先界定变更属于 Bug 修复、兼容新增还是契约变更，并列出受影响的权威入口、生命周期和测试。
2. 修改公式、GameplayTag 语义、DefinitionId、事件 schema、发布契约或公开蓝图 API 前，先更新 ADR/迁移方案和相应版本号。
3. 只在公共系统无法表达需求时扩展内核 registry；不得为单个技能增加专用结算旁路。
4. 项目 C++ 注释使用中文，重点说明“做什么、为什么、有什么约束”，避免逐行翻译代码：
   - 类、结构和枚举应有简介。简单类型可用一句话说明；核心类应进一步说明主要职责、职责边界、生命周期、网络权限及关键协作对象。
   - 项目自定义函数应至少简要说明用途。存在前置条件、副作用、失败情形、特殊返回语义、权限或时序要求时，必须一并说明。
   - 关键字段应说明其业务含义；单位、所有权、复制策略、生命周期或有效条件不直观时，需要明确标注。语义清晰的局部变量无需注释。
   - 注释应能让维护者不跳转到实现也理解基本行为，不能只翻译标识符或堆叠 `Think`、`ExpireAt`、`ActiveGE` 等内部术语。术语首次出现时说明其业务含义；策略类注释应写清生效场景、各选项的可观察结果以及不受该策略控制的边界。
   - 时间、叠层、覆盖优先级或数值运算容易误解时，补充最小时间线或配置示例。例如刷新周期应说明“何时再次触发”，参数覆盖应说明“有同名覆盖值时用哪个值”。
   - 构造函数、析构函数、简单访问器及其他惯例代码无需例行注释；只有存在特殊初始化、资源管理或非显然行为时才补充说明。
   - Unreal Engine 的常规生命周期回调、重写函数和接口实现无需重复解释引擎语义；只有承载项目特有职责、调用顺序或副作用时才需要注释。
   - 实现内部的注释应解释特殊分支、稳定排序、权限判断、兼容处理和清理顺序背后的原因，不要描述代码表面行为。
   - API 契约写在头文件声明处，实现原因写在 `.cpp` 对应逻辑附近，避免在声明和实现中重复同一段说明。
   - 修改行为时同步更新注释；失效、误导或与代码重复的注释应直接删除。
5. 蓝图可见的类、函数、字段和参数提供中文 `DisplayName`、`ToolTip` 或 `UPARAM(DisplayName=...)`。
   - 可在 UE Details 面板中编辑或查看的 DataAsset 字段，以及它们展开后的项目自有 `USTRUCT` 字段，必须显式同时提供中文 `DisplayName` 与 `ToolTip`，不能只依赖 C++ 注释。
   - `ToolTip` 应使用配置者可直接理解的语言说明选择后果；来源、单位、有效范围、空值、`0`、负数或保留值存在特殊语义时一并写明。适用时使用 `Units`、`ClampMin/ClampMax`、`EditCondition`、`TitleProperty` 等元数据降低误配风险。
6. 行为变化与代码同一次修改中更新文档；不要把已实现行为继续写成“建议实现”，也不要改写历史验收结果。
7. 除非编译依赖或测试隔离已有证据表明确实受阻，否则保持单 Runtime Module。

## 5. 资产规则

- `.uasset`、`.umap`、`.fbx`、音视频等二进制资产由 Git LFS 管理，不用文本工具或脚本直接改写。
- 资产移动/重命名后修复引用和 redirector，回读新旧路径，编译相关蓝图并保存。
- 新 Combat DataAsset 放入 AssetManager 已扫描的 `/Game/Combat/Definitions/...` 或明确的 Demo 目录，并通过 DefinitionId、schema、redirect 和 cook 校验。
- 测试地图中的 Timer 只能编排 smoke，不能成为生产 gameplay 的唯一结算时钟。
- 不提交 `Binaries/`、`DerivedDataCache/`、`Intermediate/`、`Saved/`、IDE 文件或本地绝对引擎路径。

## 6. 最低验证矩阵

| 变更类型 | 最低验证 |
| --- | --- |
| 仅 Markdown 文档 | `python3 -B Tools/validate_docs.py`、过时状态与事实核对、`git diff --check` |
| 文档校验脚本 | `python3 -B -m unittest discover -s Tools/Tests -p 'test_validate_docs.py' -v`、真实仓库文档校验、`git diff --check` |
| 普通 C++ 实现 | `ue_gasEditor` Development 构建 + 直接相关 `Combat.*` Automation |
| Damage/Heal/Modifier/Ability/时序语义 | Editor 构建 + 相关专项测试；公共顺序或契约变化时运行完整 `Combat.*` |
| DataAsset/蓝图/关卡 | Editor/蓝图编译与保存回读 + `CombatAssetValidation` + 相关 Automation/PIE |
| 复制、RPC、Owner、Projectile reconcile | Editor/Server/Client Target + Dedicated Server/Client smoke；PIE 不能替代最终网络证据 |
| 生命周期或 registry | 成功路径、取消路径、旧 Handle、Owner EndPlay、World teardown 和泄漏计数 |
| 发布契约或性能预算 | M8 全量矩阵：三 Target、完整 Automation、资产校验、Dedicated 双客户端和容量/teardown |

命令模板见 `README.md`。先发现本机 `<UE_ROOT>`；历史文档中的 `D:\UE\UE` 只是验收机器记录，不能成为新脚本的默认值。当前归档基线是 40 个 `Combat.*` 测试，新增能力应增加测试，不能无说明降低发现或通过数量。

如果受环境、权限或正在运行的 Editor 阻塞而无法执行某一层验证，必须如实写明“未执行”、原因和仍需运行的命令；不得把编译成功等同于 Automation、PIE 或 Dedicated 通过。

## 7. 文档职责

AI 协作任务遵循 [00-05-AI-Native-Development-Workflow.md](Doc/CombatSystem/00-Project/00-05-AI-Native-Development-Workflow.md)：先完成 F0 framing 和 Spec，再按 TDD、分层验证、F2 对抗审查和 Push-Ready Gate 交付。本文的 F0/F1/F2 是交付流程门；路线图的 G0-G8 仍是里程碑 Gate。新功能、Bug 修复、资产迁移和契约变更使用 `Doc/CombatSystem/Specs/_template.spec.md` 建立 Spec；`Doc/CombatSystem/00-Project/00-01-Progress-Tracker.md` 仍是唯一实时状态来源。
功能任务可直接使用 [Skills/combat-feature-development/SKILL.md](Skills/combat-feature-development/SKILL.md)，其步骤必须服从本文和当前台账，不替代项目硬约束。
实现 GAS/Combat 技能时使用 [Skills/combat-skill-development/SKILL.md](Skills/combat-skill-development/SKILL.md)，并按技能扩展指南、模板检查表和相关架构专题选择公共入口。
根据用户需求判断主 Skill、记录路由置信度并在交付后自评和受控调优时使用 [Skills/combat-task-router/SKILL.md](Skills/combat-task-router/SKILL.md)；Skill 本身的创建或维护按 `skill-creator` 规则执行。

- `README.md`：当前项目入口、快速上手和导航。
- `Doc/CombatSystem/README.md`：完整文档索引和阅读路径。
- `Doc/CombatSystem/00-Project/00-01-Progress-Tracker.md`：唯一实时状态与证据台账。
- `Doc/CombatSystem/10-Architecture/10-01-Scope-Architecture.md`：当前架构、范围和不可破坏约束。
- `Doc/CombatSystem/10-Architecture/10-02`–`10-08`：各运行时专题的 v1 语义契约；`Doc/CombatSystem/20-Content/20-01-Example-Skills.md` 保存示例技能。
- `Doc/CombatSystem/00-Project/00-02-Implementation-Roadmap.md`：M0-M8 历史 WBS/Gate，不代表当前状态。
- `Doc/CombatSystem/00-Project/00-03-Test-Plan.md`：验证分层和准入标准。
- `Doc/CombatSystem/00-Project/00-04-Decisions-Gaps.md`：ADR、Gap、延期与契约变更入口。
- `Doc/CombatSystem/00-Project/_intake-template.md`、`_gate-checklist.md`、`_delivery-record-template.md`：任务入口、流程门和交付证据模板。
- `Doc/CombatSystem/90-History/`：冻结决策与验收证据；除纠正事实错误外不重写历史结论。
- `Doc/CombatSystem/20-Content/20-03-M8-Public-Extension-Guide.md`：新技能和迁移的当前公开入口。
- `Doc/CombatSystem/10-Architecture/10-09-Client-Server-Interaction.md`：客户端 Order、服务器移动/施法/结算和客户端复制回显的当前端到端说明。
- `Doc/CombatSystem/10-Architecture/10-10-Server-Authoritative-Movement-Kickoff.md`：服务器权威单位移动的架构、迁移记录、生命周期与验收 Gate。
- `Doc/CombatSystem/10-Architecture/10-12-Bottom-HUD-Design.md`：底部 HUD 的定稿布局、拥有者投影、蓝图绑定、生命周期和验收边界。

## 8. 完成与交付

- 先检查差异范围，确认没有混入用户修改和生成文件。
- 执行与风险相称的构建、Automation、资产或 Dedicated 验证，并记录命令、结果和日志位置。
- 实际任务状态变化才更新进度台账；没有证据时不得写“已通过”。
- 检查文档、代码、DataAsset、GameplayTag、网络载荷和发布版本是否同步。
- 默认不创建提交、不推送、不重写历史，除非用户明确要求。
- 当前采用本地开发与用户验收流程，不要求 PR 或 GitHub Actions；适用的质量 Gate 在本地执行并记录证据。
- 交付说明必须列出改动、验证、未验证项和剩余风险。
