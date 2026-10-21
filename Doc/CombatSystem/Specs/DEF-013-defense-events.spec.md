# DEF-013 Phase 3D 防守事件节点与风险确认

> Spec 版本：`0.5`
> 状态：`已验收`
> Owner：Codex
> 创建日期：2026-10-19
> 关联进度台账：`00-01-Progress-Tracker.md` §12.13
> 风险等级：`L1`

## 0. Intake 与 Gate 记录

- 用户请求：开始实现“守卫雅典娜”尚未完成的内容；按已确认优先级先实现事件系统。
- 附件解释：无附件；用户此前的状态盘点是范围参考，`agent.md`、设计文档和既有 Phase 1–3 实现是工程约束与事实依据。
- 已读取入口：`agent.md`、`README.md`、`00-01-Progress-Tracker.md`、`00-03-Test-Plan.md`、`00-04-Decisions-Gaps.md`、`00-05-AI-Native-Development-Workflow.md`、`10-01-Scope-Architecture.md`、`10-09-Client-Server-Interaction.md`、`10-12-Bottom-HUD-Design.md`、`20-05-Roguelite-Defense-Game-Design.md`、`90-16-M8-Lifecycle-Audit.md`、`Skills/combat-task-router/SKILL.md`、`Skills/combat-feature-development/SKILL.md`、Defense 运行时/测试源码和现有 Defense 资产目录。
- 主 Skill：`Skills/combat-feature-development/SKILL.md`
- 备选 Skill 与排除理由：`combat-skill-development` 不作为主 Skill；本轮实现事件编排、风险事务和只读 UI，不新增玩家 Ability/Projectile/Modifier 技能包。
- 路由置信度：`high`
- F0 结论：`GO`
- F0 依据：事件是设计文档明确的下一项缺口；现有 Director、Scheduler、Economy、Damage/Heal、RunView 和 Defense HUD 已有公共入口，范围可回滚且不需要改变 Combat 发布契约。
- F1 结论：`APPROVED`
- F1 审查人：Codex（L1 本地计划审查）
- F1 审查版本：`0.5`
- F1 计划审查证据：0.1–0.3 计划见下文历史记录；0.4/0.5 preflight、plan、build 报告保存于 `Saved/DEF-013/targets-04/`。批准 0.5 的平台归档、日志身份补充、失败检测/清理及复验范围。
- Build 解锁：`已解锁`；`py -3 -B Tools/task_gate.py --mode build --spec Doc/CombatSystem/Specs/DEF-013-defense-events.spec.md --kind feature` 已通过
- F1 重审条件：增加事件外敌人/新 Boss、改变奖励所有权、增加公开网络 schema、修改发布契约或扩大为 12 波/完整事件池时递增 Spec 并重审。
- F2 结论：`PASS`
- Push-Ready 结论：`READY`
- Push-Ready 说明：PIE、正式资产冷回读、Game Archive 的既有证据保留；0.5 已使用平台正确的独立 Server/Client 归档完成两轮 Dedicated＋双客户端验证，关闭 FBufferReader 风险。
- 验证：0.5 Editor/Server/Client Development 构建通过；工具回归 59/59（其中新 Target 工具 10 项）；WindowsServer/WindowsClient Cook/Stage/Pak/Archive 成功；两次独立重启三端均 Pass，NetMode=1/3、同局 RunId、schema4、1940 金币和外来拥有者拒绝断言通过。
- 已完成：0.3 真实渲染 PIE 事件按钮、正式资产冷回读、146 个资产校验、Game 归档单机及 Editor 双客户端；0.5 额外关闭独立 Target cooked 读入风险。长时间 Soak/Perf 本轮未执行，原因是本轮仅修复启动编排和 smoke 诊断，不改变运行时玩法或性能路径。

## 1. 目标与范围

### 目标

在现有 5+1 原型防守循环中接入两个固定事件节点：第 2 波后“宝库探索”和第 4 波后“神殿誓约”。事件由服务器 Director 驱动，支持安全/风险选项、发起人确认、10 秒可取消窗口、一次性奖励和事件结束后恢复波次流程。

### 范围

- 新增 `EDefensePhase::EventChoice` 和事件数据/只读视图，事件发生在波次完成、强化选择收口之后、下一次准备阶段之前。
- 新增稳定 `UDefenseEventData` 与 `FDefenseRiskLedger`，事件/风险修订、选项、状态、奖励批次和截止时间由服务器持有。
- 宝库探索：安全选项奖励 240 金币 + 60 经验；风险选项进入 10 秒确认，成功后下一波结束奖励全队 300 金币，且后续 Boss 奖励增加 10%。
- 神殿誓约：安全选项立即修复核心最大生命 12%；风险选项提高下一波预算 12 点并提高精英概率，成功后全队 300 金币且 Boss 奖励增加 10%。本轮以现有波次预算和可观察风险修正表示，暂不新增精英/新敌人内容。
- 事件期间暂停下一波倒计时和新波出生，世界战斗模拟保持运行；事件完成、放弃、取消或超时后只恢复一次。
- Defense HUD 增加事件名称、选项、确认/取消状态和剩余时间的只读显示；若现有蓝图缺少事件控件，C++ 提供安全降级文本/状态，不允许 UI 直接修改核心、金币或阶段。
- 自动化覆盖选择权限、重复请求、旧修订、取消、超时、奖励 exactly-once、核心治疗、风险修正、死亡/断线/终局和 World teardown。

### Non-Goals

- 不扩展到 12 波、5 个事件节点或完整事件池；不实现新精英敌人、事件战斗副本或随机掉落。
- 不实现 Boss 三阶段技能、八件防守装备、局末统计或完整中途换英雄。
- 不改变 GameplayTag/Event/Formula/Release schema，不新增独立经济或伤害入口，不使用地图蓝图 Timer。
- 不支持客户端提交任意奖励、预算、核心生命或完整种子；不支持中途加入和跨局保存。

## 2. 当前事实与依据

以下为 0.1 开工基线；当前实现及 0.5 验证结论见 §0、§5–§9。

- 设计文档阶段流转要求 `BuildChoice -> Event -> Preparation`，事件节点位于第 2/4 波后；见 `Doc/CombatSystem/20-Content/20-05-Roguelite-Defense-Game-Design.md:59-69`、`:141-150`。
- 当前 `EDefensePhase` 只有 Preparation/WaveCombat/Intermission/BossPreparation/Victory/Defeat，Director 在普通波后直接进入 Intermission/BossPreparation；见 `Source/Combat/Combat/Defense/DefenseTypes.h`、`DefenseDirectorComponent.cpp`。
- 当前 `FDefenseRunView` 只有阶段、波次、倒计时、核心和终局字段，没有事件投影；`ADefenseGameState` 负责整体复制快照。
- 当前 Director 已有 PhaseGeneration、Scheduler、终局 exactly-once、玩家金币/经验入口、核心 Heal、团队账本和只读发布路径，可复用这些入口。
- 当前事件/风险账本、事件 DataAsset、事件 UI、EventChoice RNG 域均未实现；`GAP-029` 和设计文档仍将其列为后续内容。

## 3. 行为与契约

### 主流程

1. 普通波完成并结算波次奖励，收口个人强化选择。
2. 若波次为 2 或 4（0-based 波索引 1 或 3），进入 `Event`，冻结 `EventId + EventRevision`，发布事件视图；否则进入原有 Intermission/BossPreparation。
3. 玩家提交安全或风险意图。安全选项直接锁定；风险选项进入 10 秒 `PendingRisk` 确认窗口，发起人可取消，任一已登记玩家可在窗口内取消，超时自动采用安全选项。
4. 事件选项锁定后只执行一次：安全奖励立即结算；风险修正写入本局 RiskLedger，下一波清场成功后发放延迟奖励并更新 Boss 奖励倍率。
5. 事件终态写入 `RewardBatchId`，清理事件调度和只读视图，进入下一次 Preparation/Intermission；旧 EventId、旧 PhaseGeneration 和重复 RequestId 均无副作用。

### 状态转换

```text
Intermission/BossPreparation
  -> Event(Offer)
  -> Event(PendingRisk)  [仅风险选项]
  -> Event(ResolvedSafe | ResolvedRisk | Cancelled | Expired)
  -> Preparation/Intermission/BossPreparation
```

事件只能在服务器确定的窗口开启；核心死亡、对局终局、World teardown 或事件拥有者离线都会取消未结算事件并清理调度，不发奖励。

### 输入、输出与数据约束

- 请求包含 `RunId + PhaseGeneration + EventId + EventRevision + RequestId + OptionId`；客户端不得携带效果数值。
- `FDefenseEventView` 只复制事件 DefinitionId、显示信息、可选项、风险状态、发起人、截止服务器时间和修订。
- `FDefenseRiskLedger` 记录 EventId、RiskRevision、OptionId、状态、RewardBatchId、下一波修正和 Boss 奖励修正。
- 奖励使用现有 PlayerController Economy/Progression 入口；核心修复使用现有 `UCombatHealSubsystem`，不直接写 Health。
- 风险修正只作用于下一波，并在波次清场或失败时 exactly-once 结算/撤销；不能跨越终局或重复叠加。

### 权威边界与权限

- 服务器 Director 是事件阶段、选项合法性、RiskLedger、奖励、核心修复和阶段恢复的唯一写入者。
- 客户端只能提交意图；服务器复核所有玩家身份、同 World、当前阶段、RequestId、修订、选项、连接状态和频率。
- 事件 UI 只读消费 `FDefenseEventView`；不能直接调用 Economy、Heal、Director 或修改 GameState。

### 失败、取消、过期、死亡、EndPlay 与重复请求

- 同一 RequestId 同载荷返回原回执，换载荷拒绝；旧 EventRevision/PhaseGeneration 拒绝。
- 风险确认窗口内任一当前玩家可取消；发起人断线时事件取消并不自动采用风险选项。
- 10 秒无操作采用安全选项；死亡玩家不能提交，但不影响其他玩家取消/确认。
- 核心死亡、终局、Director EndPlay 或 World teardown 取消事件；所有 Scheduler Handle、委托和临时风险状态清理。
- 奖励凭证按 `RunId + EventId + RewardBatchId` 去重；金币、经验、核心治疗的任一失败均不重复重试，记录稳定 FailureTag 并以失败终态结束事件。

### 兼容、版本与迁移

- 继续使用现有 Defense RunView schema 3；事件字段加入同一快照时递增 Defense 独立 schema 至 4，并同步 Server/Client。
- 新增事件 DataAsset 类型纳入 AssetManager；旧 `DA_DefenseRun` 无事件表时保持原 6 波行为。
- 不修改 Combat Release/Tag/Event/Formula/Economy schema；旧资产和旧地图可删除事件配置后回滚。

## 4. 实施计划

| 文件/资产 | 变更 | 原因 | blast radius |
| --- | --- | --- | --- |
| `Source/Combat/Combat/Defense/DefenseTypes.*` | Event 阶段、事件状态、事件/风险 View/请求/回执 | 建立网络与状态契约 | Defense GameState/HUD |
| `Source/Combat/Combat/Defense/DefenseData.*` | `UDefenseEventData`、选项和事件表 | 稳定 DefinitionId 与可配置内容 | Defense AssetValidator/RunData |
| `Source/Combat/Combat/Defense/DefenseDirectorComponent.*` | 开启、提交、取消、超时、结算、清理事件 | 服务器权威与 exactly-once | Defense 波次/奖励/终局 |
| `Source/Combat/Combat/Defense/DefenseGameState.*` | 复制事件视图 | 客户端只读投影 | Defense HUD/网络 |
| `Source/Combat/Combat/Defense/DefensePlayerController.*` | 事件请求 RPC 与结果投影 | Owner 权限和限频 | Defense 输入 |
| `Source/Combat/Combat/Defense/DefenseHUD.*`、`DefenseBuildWidget.*` | 事件状态显示与按钮入口 | 可玩验证与可观测性 | Defense UI |
| `Source/Combat/Combat/Tests/CombatDefenseTests.cpp` | 事件状态/事务/失败路径自动化 | TDD 与回归 | Combat.Defense |
| `Content/Combat/Demo/Defense/DA_DefenseRun.uasset`、事件 DataAsset/Widget（如需） | 配置两个原型事件并回读 | 实际 L_Defense 可玩内容 | Defense Demo |
| `Doc/CombatSystem/20-Content/20-05-Roguelite-Defense-Game-Design.md`、`README.md`、`00-01`、`00-04` | 同步当前事实和开放边界 | 防止文档与代码漂移 | 文档/Gap |

## 5. 验收标准（AC）

- [x] AC-01：第 2/4 波完成后服务器进入 Event，事件只读 View 在客户端显示；其他波次保持原阶段流转。
- [x] AC-02：安全选项、风险选项、10 秒确认、任意队员取消、无操作安全超时均可通过 Automation 验证，重复/旧请求无副作用。
- [x] AC-03：安全奖励、风险延迟奖励、核心治疗、下一波修正和 Boss 奖励修正使用现有权威入口，并按 RewardBatch exactly-once 结算。
- [x] AC-04：核心死亡、玩家断线、事件终局、旧 generation 和 World teardown 均清理 Scheduler/委托/临时账本，不产生晚到副作用。
- [x] AC-05：真实渲染 L_Defense PIE 已验证安全事件、风险确认、风险取消和风险超时；相关资产编译、保存、冷回读通过。
- [x] AC-06：真实渲染 PIE 在导航就绪后完成安全金币、风险确认与取消/超时/恢复；三条新事件 PIE 和既有 ChoiceButtons/Guardian/Reinforcement 交互通过。
- [x] AC-07：冷进程回读两份正式事件资产、DA_DefenseRun、AssetManager ID/路径；资产校验 146 个资产、0 错误。
- [x] AC-08：Windows 归档生成并由归档 exe 成功载入 L_Defense，归档单机事件 smoke 通过。
- [x] AC-09：WindowsServer/WindowsClient 平台归档的真实独立 Target 两轮启动通过；服务端 NetMode=1、双客户端=3，同局 RunId/schema4，六波事件和奖励/权限断言全通过，无包摘要断言或 AssetRegistry 失败。
- [x] AC-10：工具拒绝 raw Binaries、错误/缺失平台内容、旧日志目录、早退、崩溃与错误身份；两轮结束后本轮三端进程均已退出。

## 6. Definition of Done

- [x] 事件 Automation、真实渲染 PIE 已有 Green；本轮编排工具先 Red 后 Green，并执行真实归档网络验证。
- [x] 实现通过直接测试，没有绕过公共权威入口。
- [x] C++/Slate 事件入口完成编译；正式事件资产已由 UE 保存并冷回读，本轮 0.5 不新增或修改二进制资产。
- [x] 按风险完成 Editor、Server/Client、PIE、Dedicated 与归档启动；长时间 Soak/Perf 未执行，原因见测试矩阵。
- [x] 设计文档、README、台账和 ADR-079 已同步。
- [x] `git diff --check` 和文档校验通过；Saved 报告未纳入行为差异。

## 7. 测试矩阵与命令

| 层级 | 用例/命令 | 预期证据 | 结果 |
| --- | --- | --- | --- |
| 文档/Gate | `Tools/validate_docs.py`、`Tools/task_gate.py --mode delivery`、`git diff --check` | `Saved/DEF-013/targets-04/docs-final.json`、`delivery-final.json` | 通过 |
| 工具回归 | `py -3 -B -m unittest discover -s Tools/Tests -v` | `Saved/DEF-013/targets-04/tools-regression.log` | 59/59，通过；新 Target 工具 10 项覆盖错误平台、缺失容器、早退、崩溃、旧Pass和身份校验 |
| Pure/World Automation（0.3 既有） | 事件状态/事务及 Defense 回归 | `Saved/DEF-013/defense-automation-3.log`、事件专项日志 | 74 项非 PIE 通过；0.5 未重跑，原因是无 gameplay 变化 |
| PIE / Blueprint（0.3 既有） | 导航就绪后的真实 Slate 交互 | `Saved/DEF-013/pie-final/index.json`、截图 | 事件 3/3，既有 ChoiceButtons/GuardianSelectionAndSkills/ReinforcementShop 通过；0.5 未重跑，无 UI/资产变化 |
| Types/Build（0.5） | Editor、源码 Server/Client Development | `Saved/DEF-013/targets-04/editor-build.log`、`server-build.log`、`client-build.log` | 三 Target 通过 |
| Asset validation / Cold Read（0.3 既有） | 定义资产校验与新进程正式资产回读 | `Saved/DEF-013/assets-final-current.json`、`Saved/DEF-013/event-cold-readback.json` | 146 个资产、0 错误；两份事件 DefinitionId/参数和 Run 引用通过；0.5 无资产变化 |
| Package（0.5） | `py -3 -B Tools/run_defense_targets.py --package --skip-build --archive Saved/DEF-013/targets-04/Archive --output Saved/DEF-013/targets-04/smoke-01 --port 7905 --timeout 240` | `Saved/DEF-013/targets-04/package-targets.log`、`package-environment.json` | WindowsServer/WindowsClient Cook/Stage/Pak/Archive 通过，实际载入平台 IoStore 和 AssetRegistry |
| Network / Dedicated（0.5） | 上述第一轮；清理后 `py -3 -B Tools/run_defense_targets.py --archive Saved/DEF-013/targets-04/Archive --output Saved/DEF-013/targets-04/smoke-02 --port 7906 --timeout 240` | 两轮 `report.json`、三端日志、`cleanup.json` | 2/2 轮通过；真实 Server=1、Client=3，奖励/权限/同局检查通过，进程清理通过 |
| Soak / Perf | 长时间容量和性能压力 | N/A | 未执行；本轮不改变 gameplay、网络载荷或性能路径，两轮独立重启覆盖本次启动风险 |

## 8. 风险、回滚与升级

- 风险：事件终态与波次终态竞争、多人取消重入、奖励重复、核心治疗跨 World、旧选择窗口回调写入新阶段。
- 回滚方式：移除事件表/事件配置即可保持旧 6 波逻辑；代码回滚按 DEF-013 文件组，不改 Phase 1–3 已验收资产。
- 触发升级的条件：需要新敌人/副本、12 波扩展、公开网络 schema 或改变奖励所有权时递增 Spec 并重审。
- 需要人决定的问题：无；本轮采用设计文档中的两个固定原型事件和安全超时默认。

## 9. 交付证据

- 0.5 变更：`Tools/package_defense_targets.ps1` 构建并分别 Cook/Stage 两个平台；`Tools/run_defense_targets.py` 校验完整平台容器、启动/验证/清理真实三端；对应工具测试；`CombatDefenseEventNetwork.cpp` 只补充末态 RunId 诊断；README、台账、ADR-079 和本 Spec 同步。
- 实际归档：`Saved/DEF-013/targets-04/Archive/WindowsServer` 与 `WindowsClient`；不用 raw Binaries、Game Target 或 Editor 作为独立 Target 证据。构建/打包日志和环境见 §7。
- 第一轮：`Saved/DEF-013/targets-04/smoke-01/report.json`，RunId=`4C622F8147A0D4F59C936B9736E5B260`；第二轮：`Saved/DEF-013/targets-04/smoke-02/report.json`，RunId=`0254DB1B459B8FE51206E5A8A26B0A08`。每轮三端身份一致，所有业务断言通过，两个 cleanup 均 `all_exited=true`。
- F2 对抗审查：单人本地审查，重新从工具输入和真实日志审查 raw 路径、错误平台、陈旧日志、伪 NetMode/RunId、外来拥有者、崩溃优先和异常清理；边界单测和实际失败拒绝/两轮运行支持 PASS。未添加生产自动操作或引擎断言旁路。
- Push-Ready 六层：Tests PASS（59 工具测试与两轮三端）；Types/Build PASS（三 Target）；No Regression PASS（0.5 仅编排/诊断，保留 0.3 已验证 gameplay）；Adversarial PASS；DDD/Constraints PASS（无发布/schema/权威入口变化）；Decisions PASS（ADR-079 记录正确平台 Stage 约束）。
- 历史 Red 保留：`Saved/DEF-013/target-network-final/server.log` 的 raw Target 启动失败。根因是未使用平台匹配的 Cooked 内容及 AssetRegistry；平台完整归档实际通过后该风险关闭，不再列为未闭合引擎风险。
- 未执行项：本轮未重跑完整 Combat/PIE/资产校验或长时间 Soak，原因见 §7；无已知本次范围内未闭合风险。用户已于 2026-10-21 确认验收完成；未提交、未推送。

## 10. 变更记录

| 版本 | 日期 | 修改 | 原因 |
| --- | --- | --- | --- |
| 0.1 | 2026-10-19 | 建立事件系统范围、契约、AC、测试和回滚计划 | 用户授权开始实现未完成内容 |
| 0.2 | 2026-10-20 | 导航就绪后真实 PIE、正式资产冷回读、Game 打包验证 | 用户要求关闭验证缺口 |
| 0.3 | 2026-10-20 | 修复 Cooked FName 大小写校验并跑通归档 | 实际归档初始化失败 |
| 0.4 | 2026-10-21 | 规划独立 Server/Client 平台归档和严格验证工具 | 用户要求解决额外风险 |
| 0.5 | 2026-10-21 | 补 RunId 诊断、构建/平台打包、两轮真实三端通过 | 关闭包摘要断言风险并建立可重复证据 |

## 11. 路由、自评与 Reflect

### 路由记录

- 原始需求摘要：开始实现守卫雅典娜未完成内容，优先实现事件系统。
- 主 Skill：`Skills/combat-feature-development/SKILL.md`
- 选择依据：涉及 C++、网络、状态机、DataAsset、UI、测试和文档同步，不是单一玩家技能。
- 备选 Skill 与排除理由：`combat-skill-development` 不覆盖本轮主要的事件编排。
- 路由置信度：`high`

### 交付自评

| 维度 | 权重 | 分数（0–5） | 证据与扣分原因 |
| --- | ---: | ---: | --- |
| 需求与 AC | 20% | 4.7 | AC-01 至 AC-10 全有实际证据，两个原型事件及本轮额外风险已闭环 |
| 架构与权限 | 20% | 4.5 | Director/Scheduler/Owner RPC/schema4 公共入口保持；本轮独立三端验证外来权限拒绝 |
| 实现与数据 | 20% | 4.5 | 正式事件资产、状态机和 UI 既有实现保留；新增可重复的正确平台打包入口 |
| 验证证据 | 20% | 4.7 | 既有 PIE/资产证据，新增三 Target 构建、59 工具测试、2 轮独立 Target；未做长时间 Soak |
| 文档与可观测性 | 10% | 4.5 | 当前事实、历史 Red、复验命令、实际 exe 摘要和同局 RunId 完整记录 |
| 交付卫生 | 10% | 4.8 | PID 清理、全新日志目录、文档/diff/交付 Gate；未提交生成文件 |

- 计算总分：`4.6`（加权 4.61，四舍五入一位）
- 未闭合风险：无已知本次范围内风险；长时间 Soak/Perf 未执行，不以两轮 smoke 宣称长期稳定性。
- 自评结论：`EVIDENCE_SUFFICIENT`
- 用户验收状态：`已验收`（2026-10-21；用户确认“验收完成”）

### Reflect 与调优

- 观察与证据：raw Target 缺少平台 Cooked 内容/AssetRegistry 后包摘要断言；平台正确归档连续两次新进程运行通过，实际 NetMode/同局/奖励/权限与清理可核验。
- 根因类别：`单次实现`（独立 Target 启动部署不完整）。
- 调整文件与预期收益：将平台打包/完整性检查、独立三端日志身份验证固化到 Tools 和 README，减少再次误用仓库裸程序或旧日志；无需改通用 Skill 或引擎源码。
- 回归验证：0.5 三 Target 构建、平台 Cook/Stage/Archive、工具 59/59、独立三端两轮通过；0.3 gameplay、PIE、冷回读证据保留且不冒充本轮重跑。
- 需要用户决定的问题：无；用户已验收，未授权后续阶段或自动提交/推送。


## 12. 0.2 验证闭环修正计划（2026-10-20）

- 用户请求：解决导航就绪后的真实 PIE 事件交互、正式事件 DataAsset 冷回读和最终打包。
- 已读取入口：继续复用 §0 的 agent.md、DDD、README、台账与主 Skill；额外读取 Defense PIE 夹具、导航命令、资产校验器、AssetManager 配置、事件 RPC/Director/Slate 及构建 Target。
- 主 Skill：`Skills/combat-feature-development/SKILL.md`；路由置信度 high，F0 GO。UE MCP 当前连接失败，降级为同机 Editor Python、命令行 Automation 和实际渲染 PIE，并保存日志/截图。
- 修正范围：测试启动先等待导航可投射且未被锁定；交互证据使用渲染 RHI，NullRHI 不能证明 Slate 几何与按钮交互；测试计时从命令实际执行开始。新增事件 PIE，走真实开始/安全/风险/取消按钮，公共 Damage 清场，不直接切导演阶段。仅瞬态测试副本缩短整备/出生间隔，不修改生产数值。
- 正式资产：新增两份 DefenseEvent DataAsset，接入 DA_DefenseRun；注册 AssetManager 和资产验证具体类，编译相关蓝图、保存后新进程只读回读，并通过资产 Automation 检查 ID/触发波次/参数和冻结副本。
- 集成修正：只修直接暴露的事件刷新、重复请求、玩家隔离、风险累计或阶段问题；保持 RunView schema 4 与既有公开契约。增加真实双客户端事件 RPC 取消/确认探针，禁止生产无开关自动操作。
- 打包：用安装版 UE 的 BuildCookRun 构建项目 Game Target，避免重编整套源码引擎；先正常 Editor/Game 构建，再 Cook/Stage/Pak/Archive，使用归档可执行文件启动 Defense 地图验证。Server/Client 用已配置源码引擎增量验证，日志如实区分 Target 与 Editor -server/-game。
- 文件定位：DefenseEventPIE/DefenseEventTests/DefensePIEHelpers、既有 Choice/Guardian/Shop PIE、DefenseBuildWidget、DefenseEventComponent/Director（必要时）、CombatAssetValidationCommandlet、Config/DefaultGame.ini、Tools/setup_defense_events.py、事件资产与 DA_DefenseRun、README/台账/ADR/本 Spec；测试编排和打包脚本限本次验证目录或 Tools。
- AC-06：真实渲染 PIE 在导航就绪后完成安全金币、风险确认与取消/超时/恢复；至少安全和取消有截图及金额/核心生命断言，既有三个失败 PIE 复验通过。
- AC-07：冷进程回读两份正式事件资产、DA_DefenseRun、AssetManager ID/路径；资产校验包含两份事件（总数至少 146），无错误。
- AC-08：Windows 归档生成并由归档 exe 成功载入 L_Defense，日志证明正式定义和 schema 4；未完成的归档不能记通过。
- 失败/取消/EndPlay：所有测试委托在 teardown 解绑、隐藏进程按具体 PID 清理、超时保留失败而不跳过断言；不改动用户编辑器。回滚按本轮文件组，资产由 UE 正规保存，配置可移除新增扫描项。
- 版本说明：0.1 的验证边界保留为历史证据；0.2 只在实际得到结果后更新 AC/Push-Ready，不沿用之前未经证明的宽泛通过描述。

- 0.2 F1 审查：APPROVED，审查人 Codex（L1 本地审查）；preflight/plan 通过。复核瞬态测试与生产资产隔离、导航锁/RHI 边界、正式定义扫描、请求身份、teardown、正常项目构建与归档启动，按上述计划解锁。

## 13. 0.3 Cooked 启动修正（2026-10-20）

- 实际 Red：正常 Game 构建及 Cook/Stage/Pak/Archive 成功，但归档启动报 `Shop categories require unique lower_snake_case ids and non-empty item lists`，经济与团队商店初始化失败；证据 `Saved/DEF-013/packaged-network/server.log`。
- 根因边界：UE `WITH_CASE_PRESERVING_NAME` 默认等于 `WITH_EDITORONLY_DATA`，Cooked 的 FName 不保留各调用处的显示大小写。身份比较本来不区分大小写，不能在非保留大小写构建中用 ToString 的偶然首见大小写拒绝合法分类。
- 文件扩展：`CombatDefinitionData.cpp` 的名字语法校验在 !WITH_CASE_PRESERVING_NAME 下先按小写验证，Editor 继续严格拒绝大写作者输入；`CombatShopData.cpp` 补齐无效分类 ID/计数诊断；现有定义测试增加按编译配置的大小写测试。不改变稳定 FName 身份或公开 schema。
- 测试扩展：继续 Editor 直接测试和正式资产校验；新归档可执行文件在显式测试开关下运行完整六波、安全取消/风险确认及奖励断言。测试Actor补 standalone 请求编排，不给生产 GameMode 新增自动选择。Game Target 的 -server 不等于独立 Server Target，归档启动证据明确记录实际 NetMode。
- 回滚：仅撤销新增的非保留大小写分支和诊断，不改资产 DefinitionName；若根因证据不支持则保留失败并重审，不以跳过商店校验消除错误。

- 0.3 F1 APPROVED：Codex L1 本地审查；preflight/plan 通过，核对 FName 比较身份保持、Editor 严格校验保持和归档实际 NetMode/金币验证，批准修正范围。



## 14. 0.4 独立 Server/Client 归档风险关闭（2026-10-21）

- 用户请求：解决上一轮独立 Server/Client Target 的 `FBufferReader` 额外风险；无新附件。
- 主 Skill：`Skills/combat-feature-development/SKILL.md`；备选 `combat-skill-development` 排除（没有技能行为变更）；路由 high，F0 GO。
- 已读取：agent.md、README、当前台账、10-01 DDD、00-03 测试计划、00-05 工作流、ADR-079、旧 Target 失败日志、当前Target receipts、UE Build.version、UE PackageFileSummary/AsyncLoading 以及 UAT 平台/目标选择源码。
- 已有差异：保留前序 DEF-013 的 42 项文件差异；本轮只扩展打包/验证工具、对应工具测试和文档，不改变事件服务器结算、RPC、schema、资产数值或引擎源码。
- 已有 Red：`Saved/DEF-013/target-network-final/server.log` 在读取预生成 AssetRegistry 失败后于包摘要读取断言；当时启动仓库 Binaries 下 raw Target，没有 WindowsServer/WindowsClient stage。Cooked 包摘要的编辑器字段裁剪与 raw Content 不同，首先检查平台正确的 Cook/Stage 产物，不绕过断言或扩大读取缓冲。
- 方案：为 WindowsServer 和 WindowsClient 分别 Cook/Stage/Pak/Archive；使用已配置源码引擎增量构建 Server/Client，Cooker 使用已验证事件资产的安装版 Editor（两者基线5.8.2、兼容CL一致）；记录工具/Target receipt 与实际引擎版本，若证实不兼容再改为同源码Editor Cook。禁止用 Game Target 或 Editor smoke 替代本轮独立 Target。
- 文件定位：新增 `Tools/run_defense_targets.py`（环境检查、构建/打包、staged 入口校验、真正 Dedicated+2Client 编排与失败清理），`Tools/Tests/test_run_defense_targets.py`（缺失/错误平台归档、旧Pass、早退/崩溃与结果身份检查），README/当前台账/ADR-079/本Spec。必要的临时诊断脚本/日志写 Saved/DEF-013/targets-04。
- AC-09：存在 WindowsServer/WindowsClient 独立归档，启动的是各自 staged exe；服务器实际 NetMode=1、两个客户端=3，使用相同 RunId、schema4，六波事件请求和最终奖励断言三端全通过，无FBufferReader/AssetRegistry失败。
- AC-10：工具在归档缺失、raw Binaries、平台错误、进程提前退出、Assertion/Fatal、超时或结果身份错误时立即失败；不读取旧日志当新通过；始终清理本轮具体PID，保留用户Editor。
- 验证矩阵：先对现有raw启动失败证据定位；工具边界单测先Red后Green；实际 WindowsServer/WindowsClient Cook/Stage + 正常双客户端事件 smoke；清理后第二次独立重启复验；文档/Gate/diff校验。仅编排/文档改变时原游戏回归沿用0.3历史证据，不声称重新跑完整Combat/PIE。
- 时序/失败/回滚：本地启动监听后再启动客户端，任何端退出优先失败；测试输出目录每次唯一，端口三端隔离；无网络发布、提交或推送。回滚新工具/文档即可，不改生产DataAsset或引擎。

- 0.4 F1：Codex 本地 L1 审查 APPROVED；preflight/plan 通过，核对平台产物隔离、真实Target/NetMode与RunId证据、日志新鲜度、PID清理、无引擎/Gameplay变更以及回滚范围。

### 0.5 证据身份补充（2026-10-21）

现有客户端摘要没有 RunId，为执行 AC-09 的同局检查，在 `Source/Combat/Combat/Tests/CombatDefenseEventNetwork.cpp` 的两条末态日志补记现有复制视图 RunId；只改变显式 smoke 的诊断文本，不改请求、复制载荷或 gameplay。随后重编 Server/Client、重新 Stage 其产物，所有结果必须来自同一批次。其余0.4计划保持。

- 0.5 F1 APPROVED，审查人 Codex；preflight/plan通过，确认日志补充无 gameplay/schema 副作用，更新产物后再测试。

## 15. 用户验收记录（2026-10-21）

用户确认“验收完成”。DEF-013 Phase 3D 事件原型与风险确认已验收。本次仅更新验收状态和证据台账，不新增运行验证结论；用户尚未授权后续阶段或自动提交/推送。
