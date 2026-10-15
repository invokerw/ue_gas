# DEF-001 肉鸽成长型合作防守 RPG 设计文档

> Spec 版本：`0.5`
> 状态：`READY_FOR_REVIEW`
> Owner：Codex
> 创建日期：2026-10-14
> 关联进度台账：`00-01-Progress-Tracker.md` §12.13
> 风险等级：`L0`

## 0. Intake 与 Gate 记录

- 用户请求：在“类似守卫雅典娜”的玩法讨论后，用户要求“好的开始设计一个比较好的设计文档吧”；用户随后选择“肉鸽成长为主：每局随机技能或强化，强调不同构筑”。2026-10-15 用户进一步确认：移除共享遗物投票并改为限购即时团队商店物品；保留个人强化随换英雄；完整版约 25 分钟；允许高风险事件提高下一波难度并增加 Boss 奖励；需要自定义种子开发面板；使用原创题材。交付可用于产品评审和后续实现拆分的仓库设计文档。
- 附件解释：无附件；上轮玩法清单是设计参考，用户提供的 AGENTS.md 是工程约束；没有指定复刻版本、角色美术或外部 Page。
- 已读取入口：`agent.md`、根 `README.md`、`00-01` 台账、`00-03` 测试计划、`00-04` 决策、`00-05` 流程、`10-01` 架构、`10-15` 经济、`10-16` 相机、`10-17` AI、任务模板、`Skills/combat-task-router/SKILL.md`、`Skills/combat-feature-development/SKILL.md`；按需核对单位成长、生命周期、经济、GameMode 和 AI 源码。
- 主 Skill：`Skills/combat-feature-development/SKILL.md`
- 备选 Skill 与排除理由：`combat-skill-development` 适用于具体技能实现，本任务没有此类改动；`pages:write-page` 仅辅助写作质量，按既定仓库格式和位置保存，不创建外部 Page。
- 路由置信度：`high`
- F0 结论：`GO`
- F0 依据：用户已授权设计文档；开工时 `git status --short` 为空。范围为六份 Markdown，不涉及运行时契约变更、资产编辑或提交。
- F1 结论：`APPROVED`
- F1 计划复核摘要：0.5 计划已完成复核：团队商店即时购买、完整版 23–27 分钟排期、休眠卡协同排除、RiskLedger、无投票风险确认和种子摘要权限均有数据与权威边界。
- F1 审查人：Codex（L0 文档计划自审）。
- F1 审查版本：`0.5`
- F1 计划审查证据：`Saved/DEF-001/v0.5-preflight.json` 与 `Saved/DEF-001/v0.5-plan.json` 均通过；Codex 已复核完整版 23–27 分钟排期、休眠卡协同排除、RiskLedger、无投票风险确认和种子摘要权限，并回看团队商店六项即时消耗品、每局限购/幂等、换英雄保留、原创题材边界。文件范围仍为六份 Markdown，不改变运行时。
- Build 解锁：F1 0.5 `APPROVED`，允许执行 docs Build Gate。
- F1 重审条件：增加行为文件、外部写入、实际版本/权限迁移或实质扩大文件与验证范围时，递增 Spec 版本并重新进行计划审查。
- F2 结论：`PASS`
- F2 审查摘要：反向核对团队商店限购/幂等、购买效果到期、换英雄兼容性与休眠卡协同、RiskLedger 一次性结算、无投票风险确认、种子面板权限、完整版时长预算、原创题材、服务器权威、旧 generation、生命周期、链接和范围；未发现阻塞项。
- Push-Ready 结论：`READY`
- 验证：0.3/0.4 版本的既有文档证据保留；0.5 preflight/plan/build/delivery、文档链接校验、事实/计算检查和 `git diff --check` 均已通过。
- 未执行：UE 编译、Automation、蓝图、PIE、Dedicated、性能与玩家试玩；纯文档任务不适用，未来实现单独验收，不能把本任务文档结果当作运行时证据。

## 1. 目标与范围

### 目标

将守家、外出发育、随机强化构筑和 Boss 战组织为一局可说明、可测试、可分期实现的肉鸽合作防守游戏，提供产品取舍、完整首局样例和与现有 Combat 框架的接入边界。

### 范围

主文档覆盖定位、体验支柱、核心循环、随机种子与构筑、地图、英雄、敌人、六波原型、Boss、经济成长、团队商店即时消耗品、支线、胜负/死亡/断线、HUD、数据和权威架构、生命周期、内容清单、性能目标、完整版 23–27 分钟排期、休眠卡协同、高风险事件 RiskLedger、实施依赖、验收与用户已确定方向。同步导航、当前任务状态、proposed ADR 和 Gap。

### Non-Goals

不实现游戏、不创建蓝图/地图/资产、不改 C++/工具/配置/发布版本、不改旧验收结论、不承诺数值已平衡、不做账号级永久天赋或跨局存档、不提交或推送。没有用户确认的产品偏好使用可评审默认值；文档验收不自动授权后续实现。

## 2. 当前事实与依据

- 当前状态以 `00-01` 为准；根入口中的较旧发布数字不替代最新专项状态。当前项目已接入移动、技能、成长、物品、经济、AI A/B/C 和小地图，防守整局玩法仍需新增。
- `Source/Combat/CombatGameMode.h:17`：现有 GameMode 基于 `AGameModeBase`；`:29` 负责默认生成/控制拓扑，`:39` 的额外英雄配置可在独立模式设为 0。没有现成防守模式状态机。
- `Source/Combat/Combat/Data/CombatDefinitionData.h:222`、`:226`：现有经验、金币奖励字段面向实际击杀者；合作共享奖励不能直接沿用默认归属。
- `Source/Combat/Combat/Unit/CombatProgressionComponent.cpp:12`：累计等级阈值为 `100*(L-1)*(L+2)/2`；初始化不额外发技能点，升级增加技能点。
- `Source/Combat/Combat/Economy/CombatEconomyComponent.h:32`、`Unit/CombatProgressionComponent.h:64`：现有服务器 `AddGold` / `AddExperience` 可作为模式奖励的提交入口；幂等键需由模式持有，不能假设入口自带奖励凭证。
- `Source/Combat/Combat/Unit/CombatUnitLifecycleComponent.h:30`：`RespawnAtLocation` 不负责导航投射/落点避障；新模式须先验证位置，并保护旧生命回调。
- `10-15` / ADR-066：金币归 PlayerController，库存归英雄；当前不是跨连接存档服务。重连、跨局成长、账号体系不能列为现成能力。
- AI-004 容量证据和旧发布 Gate 仅说明对应场景；不代表新地图、特效、波次和四人完整局已经满足性能目标。

## 3. 行为与契约

### 主流程与状态转换

本任务：事实核查 → 计划审查 → 编写产品与系统方案 → 反例审查 → 文档检查 → 待用户评审。文档区分现有能力、拟新增模块、首版默认值和后续扩展。

### 输入、输出与数据约束

输入为用户目标和本仓库事实；输出为主设计及五份配套 Markdown。所有新增类型/资产/Tag 为提案。给出可计算波次时间、奖励预算、等级阈值、敌人倍率和资源上限，避免使用互相矛盾的表格；高风险事件、团队购买和换英雄必须有可去重的修订号。

### 权威边界与权限

保持 Combat 单 Runtime Module、服务器结算、ASC 单一属性源、公共 Order/Targeting/Motion/Heal/Damage、Scheduler 时钟和只读 UI。文档不修改现行 schema 或生产授权。

### 失败、取消、过期、死亡、EndPlay 与重复请求

方案必须明确：核心死亡/最终清场冲突、英雄死亡复活、敌人丢失/出生失败、旧局回调、队伍奖励去重、断线与禁止中途加入、资源拥塞和世界销毁。不把测试 Actor 当成生产导演。

### 兼容、版本与迁移

新增玩法通过独立 GameMode/地图/定义选择接入；旧 Demo 默认行为保持。首次落地需另建 feature Spec，审查新定义类型、Tag、View schema 和发布契约；本任务只登记 proposed 决策。

## 4. 实施计划

| 文件 | 变更 | 原因 | 影响范围 |
| --- | --- | --- | --- |
| `20-Content/20-05-Roguelite-Defense-Game-Design.md` | 新增肉鸽合作防守玩法与系统方案 | 用户请求的主要交付物 | 设计提案 |
| `Specs/DEF-001-coop-defense-design.spec.md` | 入口、Gate、AC、证据与复盘 | 可审查的任务记录 | 本任务 |
| 根 `README.md` | 增加设计入口，明确尚未实现 | 发现新玩法方向 | 导航 |
| `Doc/CombatSystem/README.md` | 增加主文档/Spec 和阅读路径 | 保持文档体系一致 | 导航 |
| `00-Project/00-01-Progress-Tracker.md` | 新增 DEF-001 状态、日期与日志 | 当前状态唯一来源 | 本文档任务 |
| `00-Project/00-04-Decisions-Gaps.md` | proposed ADR-069、GAP-029 | 记录产品默认与实现前待决事项 | 后续防守设计 |

先完成 preflight 和 plan，再按 L0 权限自审并运行 build；之后编写六份 Markdown。F2 以 Spec、最终文档差异、源码与校验结果逐项反向检查，不引入运行时行为。

## 5. 验收标准（AC）

- [x] AC-01：有清晰定位、设计支柱、首版/扩展/非目标、可调整的产品默认值。
- [x] AC-02：提供可按时间线复现的六波肉鸽原型、地图拓扑、两名英雄、四类兵种、Boss、强化池、八件装备、六件团队即时消耗品和开发版种子面板。
- [x] AC-03：波次、随机种子、三选一强化、构筑标签/协同、团队商店限购与即时生效、奖励、等级、多人缩放、换英雄保留、休眠卡不计入激活协同、高风险事件一次性结算、死亡/复活、胜负、断线规则闭合；防无限拖怪/重复奖励的边界明确。
- [x] AC-04：有现有能力/新增缺口映射、数据所有权、只读 UI、权威入口、状态机、异步生命周期和兼容方案。
- [x] AC-05：有实施依赖、分期进入/退出条件、功能与体验验证、性能采样目标及内容制作清单。
- [x] AC-06：主文档、导航、台账、ADR/Gap 一致，明确设计待评审和实现未开始，不改写旧验收。
- [x] AC-07：文档校验、差异检查和四阶段任务 Gate 通过；只改计划内六份 Markdown；证据如实登记。

## 6. Definition of Done

- [x] 六项文件范围完成，正文可独立阅读并有局内样例。
- [x] AC 对照与源码事实复核完成；新数值是调优起点，已实现与拟新增无混淆。
- [x] 反例审查覆盖权限、状态机、奖励、时序、生命周期、容量和接入影响。
- [x] `python -B Tools/validate_docs.py`、`git diff --check`、delivery Gate 通过。
- [x] 报告、未执行项、剩余风险、路由自评和待用户评审状态已填写。

## 7. 测试矩阵与命令

本机 `python` 指向 `C:/ProgramData/miniconda3/python.exe`；使用它运行仓库命令，与文档中的 `python3 -B` 等价。报告保存在忽略目录 `Saved/DEF-001/`。

| 层级 | 命令/检查 | 预期证据 | 结果 |
| --- | --- | --- | --- |
| 入口 | `python -B Tools/task_gate.py --mode preflight --spec Doc/CombatSystem/Specs/DEF-001-coop-defense-design.spec.md --kind docs --report Saved/DEF-001/v0.5-preflight.json` | 路由/F0 完整 | PASS：0 error(s)，0 changed file(s) |
| 计划 | `python -B Tools/task_gate.py --mode plan --spec Doc/CombatSystem/Specs/DEF-001-coop-defense-design.spec.md --kind docs --report Saved/DEF-001/v0.5-plan.json` | 计划章节与范围完整 | PASS：0 error(s)，0 changed file(s) |
| Build | `python -B Tools/task_gate.py --mode build --spec Doc/CombatSystem/Specs/DEF-001-coop-defense-design.spec.md --kind docs --report Saved/DEF-001/v0.5-build.json` | 当前版本 F1 批准 | PASS：0 error(s)，0 changed file(s) |
| 文档 | `python -B Tools/validate_docs.py --report Saved/DEF-001/v0.5-docs.json` | 93 Markdown、505 本地链接、0 error | PASS：93 Markdown、505 本地链接、0 error(s) |
| 事实与计算 | 对照 `CombatGameMode`、`CombatEconomyComponent`、`CombatProgressionComponent`、`CombatUnitLifecycleComponent` 和相关 DDD；复核波次点数、13 分钟原型/25 分钟完整版方向、团队购买、换英雄、等级/奖励/缩放、Mermaid 状态与表格 | 可回读检查记录 | PASS：源码入口与 24/6 卡表、780 秒原型、1460 秒完整版排期、团队购买幂等、RiskLedger、休眠卡协同、换英雄保留、风险事件、种子面板和原创题材检查通过 |
| 差异 | `git diff --check`、工作区状态与六份文件尾随空格检查 | 仅六份 Markdown、无空白错误 | PASS：`git diff --check` 无错误；无 Source/Content 资产/Tools 行为变更 |
| 交付 | `python -B Tools/task_gate.py --mode delivery --spec Doc/CombatSystem/Specs/DEF-001-coop-defense-design.spec.md --kind docs --report Saved/DEF-001/v0.5-delivery.json` | F1/F2/Push-Ready 与范围正确 | PASS：0 error(s)，6 changed file(s) |
| 运行时 | UE 构建、Automation、PIE、Dedicated、Soak、试玩 | 留给实施任务 | N/A：没有代码或资产变更 |

## 8. 风险、回滚与升级

- 风险：用户已确定团队商店、换英雄保留、完整版约 25 分钟、高风险事件、自定义种子开发面板和原创题材；价格、效果时长、个人金币付款的公平性、休眠卡反馈和 RiskLedger/API 仍是原型调优起点。联机房间、掉线恢复和商业化规模不会因文档出现而成为已授权实现。
- 回滚方式：仅撤销本任务新增文档与导航/台账/决策增量；不回退其他既有内容。
- 触发升级的条件：用户要求直接实施、增加外部服务或改变权限/发布契约时另立任务；文档事实/数字冲突先在当前文档修正。
- 需要人决定的问题：团队消耗品价格/持续时间、卡池权重、英雄专属卡休眠提示和是否加入账号级永久成长。当前文档采用肉鸽合作防守默认，不阻塞文档编写。

## 9. 交付证据

- 文件差异：`README.md`、`Doc/CombatSystem/README.md`、`Doc/CombatSystem/00-Project/00-01-Progress-Tracker.md`、`Doc/CombatSystem/00-Project/00-04-Decisions-Gaps.md`、`Doc/CombatSystem/20-Content/20-05-Roguelite-Defense-Game-Design.md`、本 Spec；未修改 Source/Content 资产/Tools。
- F2 审查：PASS；已反向核对团队商店限购/幂等、购买效果到期、换英雄兼容性与休眠卡协同、RiskLedger 一次性结算、无投票风险确认、种子面板权限、完整版时长预算、原创题材、范围、链接、服务器权威、旧 generation、生命周期和不适用运行时验证；本次为 L0 文档本地审查，不宣称有第二位独立审查者。
- Push-Ready 六层：READY；L1 文档校验、L2 Runtime 构建 N/A、L3 文档/链接回归、L4 F2、L5 agent.md/DDD、L6 ADR-069/GAP-029/Spec 均已复核；`Saved/DEF-001/v0.5-delivery.json` 通过。
- 构建与运行验证：UE 构建不适用；未执行且不能从本文档结果推断运行时正确性。
- 未执行验证及原因：UE 编译、Automation、蓝图/资产回读、PIE、Dedicated、Soak、实际玩家试玩均未执行，本任务没有行为文件变更。
- 剩余风险：团队消耗品对个人金币的公平性、限购效果的可读性、换英雄时专属卡休眠的理解成本、奖励分配的公平性、Boss 节奏、真实导航/容量与四人网络体验都需要原型实测。

## 10. 变更记录

| 版本 | 日期 | 修改 | 原因 |
| --- | --- | --- | --- |
| 0.1 | 2026-10-14 | 初稿与入口计划 | 用户要求形成正式设计文档 |
| 0.2 | 2026-10-14 | 按用户选择改为肉鸽成长主轴；完成正文、导航、台账、ADR/Gap 和文档验证记录 | 收敛随机强化、协作和实现边界 |
| 0.3 | 2026-10-14 | 补充 13 分钟首局节奏、24 张强化、八件装备、事件选项和奖励预算；重新执行 0.3 Gate | 让方案可以直接拆原型任务 |
| 0.4 | 2026-10-15 | 按用户决定移除共享遗物投票，改为六件限购即时团队消耗品；确定换英雄保留、完整版约 25 分钟、高风险事件、自定义种子开发面板和原创题材 | 收敛团队协作和实现边界 |
| 0.5 | 2026-10-15 | 自审补充完整版 23–27 分钟预算、休眠卡协同排除、RiskLedger、无投票风险确认和种子摘要权限；重新执行 0.5 Gate | 收紧可验算节奏、协同语义和调试权限 |

## 11. 路由、自评与 Reflect

### 路由记录

原始需求为游戏设计文档，主执行流程使用 `combat-feature-development` 的 docs 模式，置信度 high；不进入技能实现或资产编辑。

### 交付自评

| 维度 | 权重 | 分数（0–5） | 证据与扣分原因 |
| --- | ---: | ---: | --- |
| 需求与 AC | 20% | 4.5 | 主文档覆盖定位、六波原型、肉鸽构筑、合作奖励、非目标和待决策项；未含用户试玩结论 |
| 架构与权限 | 20% | 4.4 | 明确服务器种子、公共 Combat 入口、Scheduler、generation、RewardBatch、View 和生命周期；具体 API 仍待实现 Spec |
| 实现与数据 | 20% | 4.0 | DataAsset、RunState、Augment 和 Director 边界可落地；无运行时代码或资产验证 |
| 验证证据 | 20% | 4.0 | 文档校验、事实复核、Gate 和差异检查通过；UE/Automation/PIE/Dedicated 不适用且未执行 |
| 文档与可观测性 | 10% | 4.5 | 主文档、导航、台账、ADR-069、GAP-029、随机域和诊断字段一致 |
| 交付卫生 | 10% | 4.5 | 计划内六份 Markdown，0 本地链接错误，0 尾随空格，未混入行为文件 |

- 计算总分：`4.3/5.0`（按权重保留一位小数）。
- 硬性封顶或未执行项：没有因环境阻塞而遗漏适用的文档验证；运行时验证不适用于本任务，不能据此推断运行正确性。
- 自评结论：`EVIDENCE_SUFFICIENT`
- 用户验收状态：`READY_FOR_REVIEW`

### Reflect 与调优

- 观察与证据：用户将玩法主轴从经典防守改为肉鸽成长；源码核对确认现有金币、经验、经济和复活入口不能直接表达团队奖励与安全出生点。
- 根因类别：领域契约。
- 调整文件与预期收益：将随机种子、强化池、RewardBatch、团队商店购买账本、换英雄兼容性、死亡待领取和安全复活写进设计提案与 ADR/GAP，避免后续实现各自发明一套规则。
- 回归验证：0.3/0.4 证据保留；0.5 preflight/plan/build/delivery、链接校验、事实/计算复核和最终差异检查均已通过。
- 用户已决定：移除共享遗物投票；保留个人强化随换英雄；完整版约 25 分钟；允许高风险事件提高下一波难度并增加 Boss 奖励；需要自定义种子开发面板；使用原创题材。
