# AI-002 StateTree 阶段 A 运行时接入

> Spec 版本：`0.1`
> 状态：`COMPLETED`
> Owner：Codex
> 创建日期：2026-09-20
> 关联进度台账：`00-01-Progress-Tracker.md`
> 风险等级：`L1`

## 0. Intake 与 Gate 记录

- 用户请求：用户确认“review完成，开始工作吧”；接受 AI-001 v0.2 设计，开始按阶段 A 实现。
- 用户验收：2026-09-21 用户明确“这个阶段验收成功了”；阶段 A 已验收。本轮仅回写验收与诊断记录、说明后续分期，不改变已验证实现，不启动 B/C/D。
- 附件解释：无附件；Epic 页面为参考，当前仓库和 agent.md 是工程约束。
- 已读取入口：`agent.md`、`README.md`、`00-01` 台账、`00-03` 测试计划、`00-04` 决策、`00-05` 流程、`10-01` 架构、`10-07` Order、`10-09` 客户端服务器交互、`10-10` SAM、`10-17` AI v0.2、`20-02` 技能检查表、`20-03` 公开扩展、`90-16` 生命周期审计、`Skills/combat-task-router/SKILL.md`、`Skills/combat-feature-development/SKILL.md`。
- 主 Skill：`Skills/combat-feature-development/SKILL.md`
- 备选 Skill 与排除理由：技能开发 Skill 面向单技能，本次是决策宿主和通用运行时集成。
- 路由置信度：`high`
- F0 结论：`GO`
- F0 依据：用户已接受架构并授权实现；可逆兼容新增，旧单位无 Profile 时行为不变。保留 AI-001 五份未提交文档。
- F1 结论：`APPROVED`
- F1 审查人：Codex（依据 00-05 的 L1 本地审查权限，在用户已批准的架构范围内实施）。
- F1 审查版本：`0.1`
- F1 计划审查证据：2026-09-20 preflight/plan 均通过，0 error。Codex 按用户已审 AI-001 的阶段 A 核对七项 AC、单命令写入者、Scope/Receipt 生命周期、攻击边界协议、可选 Profile 兼容、三 Target/Automation/资产/联机矩阵及增量回滚；不改变网络发布契约，L1 本地批准 0.1。
- Build 解锁：`Saved/AI-002/build.json` 通过，0 error；在该检查之后才修改行为文件。
- F1 重审条件：阶段范围、控制权限、网络载荷、迁移、测试矩阵或回滚实质变化时先升版重审。
- F2 结论：`PASS`
- Push-Ready 结论：`READY`
- 验证：最终 Editor/Server/Client 构建、完整 Combat 112 项（109 success + 3 success with warnings，0 failed / not run）、其中 AI 11 项、真实 PIE、资产 32 项、AI Dedicated 双客户端及独立的原 64/256 容量回归通过；AI 地图 Windows cook 656 包、0 error。证据和本地交付门见 §7、§9。
- 未执行：Codex 的完整画面/操作手感专项自动化、打包后 Server/Client 可执行文件启动、网络损伤、长时间 AI soak 和 AI 容量；用户已完成阶段 A 验收，但不据此补写上述测试通过。本阶段工程矩阵使用 NullRHI、独立 Editor server/game 进程及 Windows cook。感知、角色模板和 Utility 等 B/C/D 内容另行实施。

## 1. 目标与范围

### 目标

交付可运行、可配置的最小通用 StateTree 决策链路。StateTree 负责行为编排，Order/ASC/Attack 保持唯一执行和结算权威。

### 范围

实现 Brain、Schema、最小 Profile、类型化 Context/Workspace、DecisionScope、Prepare/Execute/Resolve/Wait 节点与 Order Bridge；支持显式目标、位置、AbilitySpec 的 Move/Attack/Cast 和 Scheduler Wait。扩展按完整 Handle 取消、攻击边界票据；接入初始化、死亡、重生、控制者变化、玩家有效命令接管、EndPlay。新增实际编译的树资产、可玩演示入口与自动化/联机验证。

### Non-Goals

不实现感知记忆、自动技能评分、EQS、Utility、Boss 模板、迷雾或 AI 容量结论；不改网络 RPC schema、发布契约、既有技能数值或玩家命令 UI；不提交推送。

## 2. 当前事实与依据

- 设计依据：[10-17](../10-Architecture/10-17-StateTree-AI-Decision-System.md) v0.2 的阶段 A。
- 开工时 `CombatOrderComponent.cpp` 的 IssueOrder 可在返回前完成；替换会提升 generation 并取消队列；AttackReady 直接 Pump，尚无边界保护。本次已新增匹配取消与边界协议。
- 开工时 `CombatUnitCharacter` 已拥有 ASC、Order、Lifecycle 和导航 AIController；本次新增可选 Profile 驱动的 StateTree Brain。
- UE 5.8.2 StateTree 支持原生 Any/All、类型化外部数据、事件 Tick、Scheduled Tick；有效结构载荷可以发送无 GameplayTag 的本地事件。
- 当前 release 为 `combat_v4_economy_rc1`，Contract 4 / Tag 与 Event 3；本任务保持这些网络契约。历史 agent.md 的 v2 和 40 测试不是当前基线。
- 开工时 MCP initialize 连接 `127.0.0.1:8000/mcp` 实测拒绝，工具列表无 UE MCP。降级为 Unreal 原生编辑器 API/commandlet 生成、编译、保存和回读资产，使用 Editor Automation 与独立 Dedicated 进程实测。

## 3. 行为与契约

### 主流程与状态转换

Unit Ready + Authority + Profile + Autonomous → Start StateTree → Scope → Prepare 冻结意图 → Execute 只消费一次并提交 Order → 保存 CompletionReceipt → Resolve 确认 → 下一轮。C++ 只提供协议和原子执行节点，不建立替代 StateTree 的决策状态机。

### 输入、输出与数据约束

观察输入为显式 Objective（目标/位置/AbilitySpec）和 revision；Scope 带 run/life/control epoch/serial，Prepare 带 serial、consumer slot 和有效期。生产者成功退出保留意图，Abort/离开 Scope 清理；消费时复核目标生命、ObjectiveRevision、slot 和时效。回执在动作退出后仍有效，确认一次后才允许普通重评。Linked Asset 继承同一 Brain/Scope，不绑定前一个兄弟 Task 的实例。

Bridge 在提交前绑定委托，缓存同步完成；同一激活和语义动作只提交一次。一个 Brain 同时只有一个命令写入者，不给 AI 排队。回调只记事实并发送类型化本地 Wake 载荷，不在广播中递归推进树。

### 权威边界与权限

客户端 Brain 不启动；玩家/脚本有效请求完成安全和业务预检后撤销自主运行，再通过 Order 原入口执行。非法请求不撤销 AI。首次追加请求取消 AI 当前动作后保留玩家 FIFO，Stop 保持 Manual；自主恢复必须显式调用。独立换槽事务保持原“不替换动作”契约，不触发接管。网络 Owner 不等同于行为来源。AI 通过受 Brain epoch 校验的专用 C++ 提交入口，不能伪造玩家网络权限。

### 失败、取消、过期、死亡、EndPlay 与重复请求

按完整 Order Handle 取消当前项，不调用 StopAllOrders 清空新队列。撤权先递增 epoch，清理仍可精确取消旧句柄。同步终态、提交拒绝、准备失败均有唯一回执。死亡/控制者丢失/EndPlay 停树并清理 Scope、Delegate、Scheduler、票据；重生只启动新代次，旧回调不得改变新动作。

持续 Attack 普通重评申请边界；前摇中等待 Launch，Ready 时阻止下一次起手，AttackReady 时钟继续。Keep 释放票据继续原 Order，Switch 精确取消；Ready 起才安排 Scheduler 超时，超时/调度失败不无限停打。所有起手经过统一检查；晚到通知重新核对票据和 windup。Cast 完成依旧依赖 OrderReleased。

### 兼容、版本与迁移

ADR-062 接受设计，ADR-063 记录阶段 A：新增 `CombatAIProfile` PrimaryAssetType / 本地 schema 1、可选 UnitData Profile 和 C++ API；旧数据空值不启动，现有二进制定义无需改写。Wake 使用有效 USTRUCT payload，不新增全局 GameplayTag/EventSchema，不改变网络版本。Editor-only 树编译/资产创建依赖受 Target.bBuildEditor 和 WITH_EDITOR 隔离。

## 4. 实施计划

| 文件/资产 | 变更 | 原因 | blast radius |
| --- | --- | --- | --- |
| `Source/Combat/Combat/AI/` | Brain、Schema、Profile、Workspace/Bridge、原生节点 | 阶段 A 运行时 | 仅启用 Profile 的单位 |
| `Combat/Order/` | 匹配取消、攻击边界、来源仲裁与只读预检 | 防止误取消/前摇饥饿 | 公共 Order；完整回归 |
| `Combat/Unit/`、`Combat/Data/` | 可选 Profile 与生命周期接入 | 单宿主与清理 | 默认关闭 |
| `Combat.Build.cs`、`Config/DefaultGame.ini` | StateTree 依赖与资产扫描 | 编译/cook | 单 Runtime Module |
| `Combat/Tests/`、`Combat/Validation/` | World/StateTree 测试、资产构建验证、联机演示 | 可回读证据 | 编辑器构建器隔离 |
| `Content/Combat/Demo/AI/` | 最小 StateTree/Profile/演示内容 | 可玩验证 | 新增资产；不改旧单位 |
| `Tools/RunDedicated.ps1`、可选 AI 资产脚本 | AI 联机开关和可重复生成入口 | 专项网络回归 | 默认参数保持原行为 |
| `README.md`、DDD/Spec/台账/ADR/索引 | 配置说明、契约与实测 | 同步交付 | 文档 |

顺序：Order 失败测试 → 最小扩展 → Brain/Workspace 与真实 StateTree 测试 → 生命周期/接管 → 资产和演示 → 分层验证 → F2 和文档回写。

## 5. 验收标准（AC）

- [x] AC-01：原生 StateTree 编译运行，Scope/准备/执行/完成跨兄弟状态传递，至少验证链接子树；没有自建行为层级状态机。
- [x] AC-02：Move/Attack/Cast/Wait 通过现有权威入口完成；同步终态、拒绝、重复唤醒、条件退出均无丢失/重复回执。
- [x] AC-03：匹配取消不触及其他 Handle/玩家 FIFO；攻击 Ready 交接、Keep、Switch、超时和晚到通知正确，不重置攻击时钟。
- [x] AC-04：合法手动命令接管、非法命令不撤权；死亡、重生、控制者更换、Owner/World teardown 无旧回调和资源泄漏。
- [x] AC-05：Profile/树资产编译保存回读和校验通过；提供显式目标的可玩演示与配置步骤。
- [x] AC-06：Editor/Server/Client 构建、完整 Combat Automation、PIE/实际树运行及 Dedicated 双客户端通过；客户端不运行权威 Brain。
- [x] AC-07：文档/Gate/diff 通过；阶段 B/C/D 和性能未验证范围清楚。

## 6. Definition of Done

- [x] 保存真实 Red 与 Green 证据；测试覆盖正常/失败/取消/旧 Handle/Owner EndPlay/World teardown。
- [x] 满足 AC，编辑器依赖未进入 Server/Client；中文 API 注释和 Details 元数据完整。
- [x] 相关资产真实编译、保存、回读及 AssetValidation 通过。
- [x] 三 Target、完整 Automation、PIE、Dedicated 完成；必要层受阻则如实报告，不声明 READY。
- [x] F2 单代理按最终文件攻击审查、文档和六层 Push-Ready 完成；保留原修改，不提交推送。

## 7. 测试矩阵与命令

| 层级 | 用例/命令 | 预期证据 | 结果 |
| --- | --- | --- | --- |
| 流程 | `python -X utf8 -B Tools/task_gate.py --mode preflight --spec Doc/CombatSystem/Specs/AI-002-statetree-runtime.spec.md --kind feature --report Saved/AI-002/preflight.json`；plan/build/delivery 同参数，各自报告名 | 当前版本 Gate | 四项均通过、0 error；最终 delivery 覆盖 123 个变更文件，报告见 §9 |
| TDD/Order | `Combat.AI.Order.CancelPreservesQueue`、`AttackBoundary`、`CancelReentry` | Red/Green 日志 | 通过；覆盖攻击取消回调与终态日志观察者重入 |
| World/StateTree | `Combat.AI.StateTree.*`、`Lifecycle.*`、`Workspace.*`、`Assets.*`；真实编译树/Linked Asset | 自动化报告 | 通过；全部 AI 共 11 项，含 PIE |
| 构建 | 环境探测返回的 Build.bat：`ue_gasEditor/ue_gasServer/ue_gasClient Win64 Development <项目> -WaitMutex -NoHotReloadFromIDE` | 三 Target 成功 | 最终三项均 Succeeded |
| 完整回归 | UnrealEditor.exe `-unattended -NullRHI -ExecCmds="Automation RunTests Combat." -TestExit="Automation Test Queue Empty" -ReportExportPath=<报告目录>` | 不少于既有 101 测试 | 112 项：109 success + 3 success with warnings；0 failed / not run |
| 资产/PIE | `-run=CombatAIAssets`、`-run=pythonscript -script=Tools/setup_ai_demo.py`、`-run=CombatAssetValidation`；`Combat.AI.PIE.PlayableArena` | 资产报告和行为轨迹 | 32 资产零错误/警告；Blueprint 已编译，地图/树/Profile 独立回读；PIE 实际移动、命中和返回 |
| Network/Dedicated | `Tools/RunDedicated.ps1 -InstalledEditor -AI -TimeoutSeconds 120`；另跑去掉 `-AI` 的同一命令 | Server Brain 运行、Client Brain 停止；原容量与 HUD/SAM 回归 | 两轮各自通过；64 Unit/256 Modifier 预算未放宽 |
| Cook | UnrealEditor.exe `-run=Cook -TargetPlatform=Windows -Map=/Game/Combat/Demo/AI/L_CombatAI -unattended -NullRHI -NoSound -NoP4` | 地图和所有 AI 核心资产进入 cook 闭包 | 656 包、0 error；两棵树在 cook 中编译成功；1 条引擎 MCP 提示 |
| 文档/差异 | `python -X utf8 -B Tools/validate_docs.py`；`git -c core.safecrlf=false diff --check` | 零错误 | 85 篇 Markdown / 454 个本地链接；diff 通过 |
| Soak/Perf | 阶段 A 不承诺 AI 容量，保留现有联机容量回归 | AI 容量属于阶段 C | N/A |

## 8. 风险、回滚与升级

- 风险：StateTree completion/event 顺序、同步回调、攻击时钟、取消重入、编辑器模块混入 Runtime、失效目标和未保存资产。
- 回滚方式：清空新 Unit Profile 可即时禁用 AI；本次新增资产/代码和配置增量可单独回退，保持 AI-001 已有文档与所有其他用户改动；不删除已有内容。
- 触发升级的条件：需要改变网络契约或公共战斗结算权威、需要新决策框架替代 StateTree、必须侵入未授权阶段时重审 Spec。
- 需要人决定的问题：无当前实施阻塞；参数采用显式可配置的保守默认值。

## 9. 交付证据

以下报告均位于本机 `Saved/AI-002/`，不纳入 Git；本节保留可复现命令和结果摘要。安装版 UE 5.8.2（CL 56702186）承担 Editor、运行与 cook；源码 UE 5.8.2（CL 0，Compatible CL 55116800）承担 Server/Client Target。三 Target 的成功不代替运行检查，Editor 进程返回 0 也不代替 Automation JSON 的失败计数。

### 最终结果

| 证据 | 实测结果 |
| --- | --- |
| `editor-build-final.log`、`server-build-final.log`、`client-build-final.log` | 全部 Succeeded；均包含最终 Order 修正；Server/Client 未引入编辑器依赖 |
| `combat-full-final/index.json`、`combat-full-final.log` | 112 项全部执行，109 success + 3 success with warnings，0 failed / not run；AI 11 项全通过 |
| 同一全量报告中的 `Combat.AI.PIE.PlayableArena` | 加载已保存地图、等待导航并启动真实 PIE；实际向外移动 718.7 cm 后命中，返回后 Submitted=2 / Resolved=2、Move 成功且无活动 Order；使用真实引擎帧循环 |
| `assets-report-final.json`、`assets-readback.log` | Content schema 2；32 项、0 error / warning；Root/Linked Tree/Profile 重新加载后 `AIAssetsReadback Result=Pass` |
| `demo-create-2.log`、`demo-readback-final.log`、`demo-readback.json` | Blueprint `BS_UP_TO_DATE`，Profile/Root 引用正确，地图回读恰有一个演示场 |
| `dedicated-final.log`、`dedicated-final/` | 独立 Server + 两个 Client：ServerBrain=Running，AttackLandedAndMoved=1，Submitted=2 / Resolved=2 / MoveSuccess=1；两个客户端均 Brain=Stopped，复制位置一致；HUD 三端、SAM 通过 |
| `dedicated-baseline-2.log`、`dedicated-baseline-final/` | 单独执行原样本：64 Units / 256 Modifiers，容量与性能预算 Pass；HUD 三端和 SAM 通过 |
| `cook.log`、`cook-default-references.txt` | 默认 Windows cook 654 包、0 error；Profile/两棵树经资产扫描进入闭包 |
| `cook-ai-map.log`、`cook-ai-map-references.txt` | 显式 AI 地图 cook 656 包、0 error；引用集包含地图、Blueprint、Profile、Root/Action 两棵树；World Partition 流式生成和树重新编译成功 |
| `preflight.json`、`plan.json`、`build.json`、`delivery.json` | 当前 Spec 0.1 的四项 Gate 均通过、0 error；preflight/plan/build 在行为修改前执行，最终 delivery 覆盖 123 个变更文件 |

Automation 的三个带警告测试分别为 AI PIE 的 CrowdManager 初始化/清理提示、Economy 开发命令的预期拒绝/用法提示、Progression 开发命令的预期用法提示。两轮 cook 各一条警告来自引擎 MCP 插件的许可提示，没有树编译或资产错误。Dedicated 使用 `UnrealEditor.exe -server` 和两个 `-game` 独立进程，未声称运行过打包后的 Server/Client exe。

### Red、F2 发现与关闭依据

| 发现 | Red / 核对证据 | 修正与 Green |
| --- | --- | --- |
| 原 Order 无精确取消/边界 API | `red-order-build.log` | 新增完整 Handle 取消与边界票据；CancelPreservesQueue、AttackBoundary 通过 |
| 取消前摇和终态日志都可同步重入，旧项可能重复完成或吞掉新命令 | `ai-tests-5.log`、`reentry-red.log` | 先摘除旧项，以旧快照清理和记录日志，清理期间禁止 Pump；CancelReentry 覆盖两种回调来源，最终全量通过 |
| Tick 内消耗事件后残留合并标记，后续 Wake 可能被吞掉 | 专项时序失败及真实 PIE 轨迹核对 | Tick 前后清理合并标记；同步完成/重入、Wait 和真实 PIE/Dedicated 通过 |
| 导航投影与业务距离不同，AlreadyAtGoal 路径可能挂起 | `ai-tests-6.log` | 公共 Order 有界重试；演示返回实际出生点的导航投影；最终 PIE 与联机真实移动/返回通过 |
| 禁用单个 Scope Task 会绕过作者结构约束 | 最终树资产审查及 SingleWriterValidation 用例 | 禁止单独禁用协议 Task；允许禁用整个分支；校验回归通过 |
| 旧 Ability 测试把临时 World 留在 CDO 上，新 PIE 切图暴露泄漏 | `combat-full.log` | 五处 fixture 使用作用域恢复原 AbilityData；最终 112 项含切图/退出通过 |
| 额外 AI NPC 污染固定 64/256 样本 | `dedicated-capacity-overlap/` | `-AI` 与默认容量场景独立运行，均通过，未放宽原预算 |
| 高负载并发启动使旧容量场景在客户端连接前采样 HUD | `dedicated-baseline-early-snapshot/` | 不将失败轮计为通过；串行重跑 `dedicated-baseline-2.log`，三端 HUD 与原预算同时通过 |

最终 F2 按已保存代码/资产重新核对 Scope、控制 epoch、目标生命代次、同步回执、精确取消、边界计时、客户端准入和 Runtime/Editor 隔离；发现均已关闭。单次故障固定在本任务测试和文档，不修改通用 Gate 掩盖失败。

### Push-Ready 六层

| 层 | 结论 | 依据 |
| --- | --- | --- |
| Tests | PASS | 112 项完整回归、11 项 AI、保存资产的真实 PIE、双客户端实际运行 |
| Types/Build | PASS | 最终 Editor/Server/Client 构建、32 项资产校验、AI 地图 cook |
| No Regression | PASS | 完整 Combat、原 64/256 容量、三端 HUD/SAM；原 Demo/单位二进制资产无修改 |
| Adversarial | PASS | 上表发现关闭；取消/日志重入和禁用 Scope 均有回归 |
| DDD/Constraints | PASS | 单 Runtime Module、服务器决策、Scheduler、公共 Order/ASC/Targeting、中文 API、可选 Profile；无新行为 HSM |
| Decisions | PASS | ADR-062/063、阶段 A 指南/台账同步；不变更发布契约，GAP-028 保留 B/C/D 范围 |

代码/资产增量包含 `Combat/AI`、Order/Unit 接入、Editor 构建器、测试、`Tools/setup_ai_demo.py` 和独立 AI Demo。World Partition 外部资产随新地图纳入 LFS；保留开工时 AI-001 的五份文档修改，不提交或推送。

### 2026-09-21 诊断与用户验收

用户反馈“战斗日志有，但眼前木桩不掉血”后，通过当前 Editor 的 UE MCP 重新运行 PIE 并读取场景对象与 ASC：原木桩位于 `(1340, 550)`，两次采样均为 500 Health；AI 演示另行生成的目标位于约 `(454, -454)`，Health 从 468 降到 340。样本保存在 `Saved/AI-002/visual-diagnosis-samples.json`。日志与眼前木桩对应不同对象，现有证据指向演示位置/目标辨识问题，不据此判断为复制故障；本轮没有调整场景资产。

用户随后明确确认阶段 A 验收成功。仅同步 Spec、台账、设计状态及演示说明；原三 Target、Automation、资产、Dedicated 和 cook 证据保留原执行时点，本次不重跑运行时矩阵。文档/diff 与 acceptance delivery 检查结果见 `Saved/AI-002/acceptance-delivery.json`。后续 B/C/D 尚未开始。

## 10. 变更记录

| 版本 | 日期 | 修改 | 原因 |
| --- | --- | --- | --- |
| 0.1 | 2026-09-20 | 基于已审设计建立阶段 A 实施范围和矩阵 | 用户确认 review 完成并要求开始工作 |
| 0.1 | 2026-09-20 | 完成约定实现、Red/Green、F2 修正及三 Target/全量/资产/PIE/Dedicated/cook；补齐交付证据 | 变更均在原批准范围内，转为待用户验收 |
| 0.1 | 2026-09-21 | 用户确认阶段 A 验收成功，状态转 COMPLETED；补录双木桩诊断与配置说明 | 仅验收文档回写，保留测试边界；后续阶段未开始 |

## 11. 路由、自评与 Reflect

- 原始需求摘要：将已审查 StateTree 方案转为运行时实现。
- 主 Skill：Combat 功能开发；路由置信度 high。
- 自评结论：交付时加权 4.6/5；下表保留 2026-09-20 的证据和扣分，当前状态为用户已验收。
- 用户验收状态：AI-001 与 AI-002 阶段 A 均已验收；2026-09-21 的阶段 A 验收确认当时不包含后续实施或 Git 授权。随后用户另行授权阶段 B，并在 B 验收完成后明确要求本地提交，最新状态见 [AI-003](AI-003-statetree-roles.spec.md)。

| 维度 | 权重 | 分数 | 证据与扣分原因 |
| --- | ---: | ---: | --- |
| 需求与 AC | 20% | 4.5 | 七项 AC 有证据闭合；当前是明确目标驱动的阶段 A，完整通用角色行为仍按 B/C/D 实施 |
| 架构与权限 | 20% | 4.5 | 权威/单写入者/代次/生命周期实测通过；复杂多角色与动态子树尚未开放 |
| 实现与数据 | 20% | 4.5 | 可选 Profile、两棵真实树、Demo 和回滚路径完整；当前作者协议有意限制节点/转移组合 |
| 验证证据 | 20% | 4.5 | 三 Target、完整回归、资产、PIE、独立联机和 cook 全部完成；未做人工画面、打包启动或 AI 容量/长 soak |
| 文档与可观测性 | 10% | 5.0 | DDD/配置指南/ADR/Spec/台账一致；回执、计数器与逐层报告可回读；本维度无已知缺口 |
| 交付卫生 | 10% | 5.0 | 原资产/用户修改保留，diff/Gate 检查，范围和未执行项明确；本维度无已知缺口 |

Reflect：实际 StateTree/导航/事件观察者的执行时序暴露了纯编译检查无法发现的问题。它们属于本次实现和 fixture 问题，已修改 `CombatOrderComponent`、Brain、AI 测试、Ability 测试、演示场和 Dedicated 脚本，收益是阻止旧回调覆盖新动作并验证真实导航返回。回归以最终 112 项、保存地图 PIE 和双客户端轨迹为准；容量失败保留原报告并隔离样本，不提高预算。后续诊断说明演示初始视野与目标辨识也应单独检查，已补充指南中的实际位置；不把未修改的布局写成已修复。未发现需要扩大通用流程/Skill 的重复遗漏，因此本轮不调整它们。阶段 A 已由用户验收，后续阶段不自动标记为已开始。
