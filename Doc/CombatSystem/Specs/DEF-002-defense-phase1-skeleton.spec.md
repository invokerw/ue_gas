# DEF-002 Phase 1 可玩的防守骨架

> Spec 版本：`0.2`
> 状态：`已验收`
> Owner：Codex
> 创建日期：2026-10-15
> 关联进度台账：`00-01-Progress-Tracker.md` §12.13
> 风险等级：`L1`

## 0. Intake 与 Gate 记录

- 用户请求：阅读守卫雅典娜设计文档，开始实现第一阶段防守骨架。
- 附件解释：无附件；用户的 AGENTS.md 与 agent.md 为工程约束，DEF-001/20-05 为玩法需求。
- 已读取入口：`agent.md`、README、00-01/03/04/05、10-01、10-09、10-10、20-02、20-03、20-05、30-01、90-16、DEF-001、任务模板、任务路由与功能开发 Skill；核对 GameMode、Unit/Lifecycle、Scheduler、Order、Damage、Modifier、Economy、Progression、AssetValidator、既有 AI 地图与网络测试源码。工作区初始干净，未发现 `.codegraph/`。
- 主 Skill：`Skills/combat-feature-development/SKILL.md`
- 备选 Skill 与排除理由：combat-skill-development 用于玩家技能内容，本次以模式编排为主，固定强化组合已有 Modifier，不新增技能内核。
- 路由置信度：`high`
- F0 结论：`GO`
- F0 依据：用户明确授权 Phase 1 实现；新模式及资产隔离，可回滚，公共战斗 API 足够表达。
- F1 结论：`APPROVED`
- F1 审查人：Codex（L1 本地计划审查）
- F1 审查版本：`0.2`
- F1 计划审查证据：0.1 preflight/plan/build 已在首个行为文件前通过。0.2 因恢复完整 Phase 1 可玩地图/固定强化/网络验收范围与修正出生失败规则，先将状态回 PLAN_REVIEW/F1 REVISE，再运行 `Saved/DEF-002/v0.2-plan.json`；复核独立地图、稳定定义、公共权威入口、失败优先、生命周期、网络、测试和回滚后 APPROVED，`v0.2-build.json` 通过后才修改扩展行为。当前文档统一 0.2 事实，不追认旧范围。
- Build 解锁：`已解锁`；0.2 plan/build Gate 通过。
- F1 重审条件：改变既有发布契约、Combat 公共 API、Tag/Event schema，加入随机选择/重抽/团队商店/事件/跨局或重连时升级 Spec 并重审。
- F2 结论：`PASS`
- 用户验收：2026-10-15，用户确认该版本验收完成并授权本地提交；进入下一阶段。
- Push-Ready 结论：`READY`
- 验证：Editor/Server/Client 三 Target、173 项分进程 Combat 回归、56 定义资产校验、导航、渲染 PIE、单/双人胜利与单人失败网络 smoke 均通过；文档和交付 Gate 见 §7。
- 未执行：cook/打包 exe、3–4 人容量、长时 soak/网络损伤、完整手工游玩和平衡；这些属于后续容量/发布/用户验收范围。初次混合进程回归复现 GAP-027，最终相同全部用例已分进程通过。

## 1. 目标与范围

### 目标

独立地图中单名现有远程英雄守护核心，两条路线、5 普通波和 1 简单 Boss；服务器可靠开始/推进/终结一局。为 Phase 2 保留稳定数据与玩家强化所有权接口。

### 范围

- DefenseRun/Wave 数据、GameMode/GameState、Director、Objective、可见敌人、顶部 HUD。
- 深拷贝计划、RunId/PhaseGeneration/敌人 LifeGeneration、有限出生重试、清场与失败优先。
- 每玩家每波金币/经验去重、死亡待领经验/强化、安全导航复活、断线清理；开局后拒绝中途加入。
- 两张固定强化（第 1 波 +5 护甲、第 3 波 +45 移速），使用公共 Modifier，死亡保留。
- 原生 UE API 生成并冷回读的独立灰盒地图、DataAsset/蓝图、导航构建与资产校验。
- 单/双人独立 Dedicated smoke、真实地图 PIE 与直接 Automation；Server/Client Target 构建按实际证据记录。

### Non-Goals

随机种子/种子面板、三选一/重抽、团队商店、第二英雄、事件区、Boss 多阶段技能、换英雄界面、跨局/账号/重连、3–4 人容量平衡、正式美术与发布。当前金币可读，防守 HUD 隐藏普通商店；Phase 3 配置经济内容。既有 Demo 默认模式不变，不提交或推送。

## 2. 当前事实与依据

- `20-05` §9 Phase 1 要求独立模式/地图、Objective/RunState/Director、两路、5+1 波、固定强化与单/双人 smoke。
- `Source/Combat/CombatGameMode.cpp` 保持 Unit + Command Pawn + 唯一服务器 AIController 拓扑；防守类继承它，额外英雄设 0。
- `Unit/CombatUnitLifecycleComponent.*` 的死亡广播 exactly-once；复活入口本身不做导航/避障，由 Defense 先复核。
- `Scheduling/CombatSchedulerSubsystem.*` 新建零延迟任务在下一 dispatch 执行；用于同批次死亡后裁决，测试同时验证 Boss 先死/核心后死。
- `Combat/CombatDamageSubsystem.cpp` 拒绝空伤害来源；测试使用真实有来源的 Damage 请求，不能直接改生命。
- 金币仍归 Controller，经验/库存归英雄；本阶段新增的奖励凭证和强化身份归 Controller 的局内记录。
- 本机安装版 UE 5.8.2 可用；源码引擎路径由环境工具发现。当前无 UE MCP 可调用工具，也无用户运行中的 Editor，因此按 30-01 降级为 UE Python/Commandlet，资产不通过文本工具写入。

## 3. 行为与契约

### 主流程与状态转换

Preparation（40 秒）→ WaveCombat → Intermission（8 秒）循环；第 5 波后 BossPreparation（8 秒）→ 第 6 波 → Victory。核心死亡/丢失、非法敌人生命、出生重试耗尽、波次超时等统一进入 Defeat。终局不可逆。

清场必须满足出生计划全部成功、登记敌人全被公共生命周期确认死亡；失败出生、直接 Destroy、复活换代不能充当击杀。死亡委托只标记和排队，下一 Scheduler 回合再次检查核心及敌人身份，才发奖励/判胜。同批次核心与 Boss 双死失败优先。

### 输入、输出与数据约束

`DefenseRun`/`DefenseWave` 为新 PrimaryAssetType，DefinitionName 稳定且 lower_snake_case；独立 DefenseSchemaVersion=1，继承 Combat 当前 Content schema=2，不改变旧资产版本。六波唯一 ID、两条唯一命名路线、有限世界坐标、单组 1–64/单波 ≤128、最后恰好一名 Boss、0–10 次重试、有限间隔/时限均校验。UnitData 不发最后一击金币/经验，避免与共享奖励重复。

Run 开局复制波次、UnitData、Modifier 数值定义，源资产变更不改本局。出生先导航投射/胶囊避障，再 Deferred Spawn + 公共单位初始化；经过最多 8 航点的 Move Order 后提交 AttackTarget。所有 Damage/生命、属性、金币/经验仍走原公共入口。

GameState 将 `FDefenseRunView`（独立 schema 1）整体复制：RunId、Phase、generation、波次、截止时间、待出生/存活数、核心引用/ASC 快照、终局原因。HUD 只读；无新客户端权威 RPC，无 DataAsset UObject 复制。

### 权威边界与权限

Director 仅服务器接受同 World 的有效核心/状态；跨 World、客户端、重复初始化拒绝。玩家沿原 Owner/CommandedUnit 提交命令。PreLogin 在 Preparation 之后拒绝新连接；现有玩家断线移除奖励/强化记录，不为新连接恢复存档。

### 失败、取消、过期、死亡、EndPlay 与重复请求

- 每阶段任务带 RunId + PhaseGeneration，旧任务即便晚到也不推进当前局。
- 敌人记录 Actor + LifeGeneration + Phase，重复死亡不减第二次计数；核心任何阶段消失/换生命失败。
- 出生每 0.5 秒重试，耗尽 SpawnFailed；不使用无碰撞强制出生掩盖阻塞。普通波 90 秒、Boss 180 秒上限后失败。
- 英雄默认 8 秒复活；最多 10 次安全落点重试，每次 0.5 秒。经验与强化在死亡期间暂存，金币属于玩家即时入账，失败则清待领记录。
- 每玩家每波奖励先登记凭证再提交；固定卡按 DefinitionId 去重。复活不重复加卡；断线撤销本局授予的精确 Modifier Handle。
- Finish 唯一封闭终局、取消 Director 调度、解绑核心、停令/清理所有本波敌人与专用 AIController；Shutdown/EndPlay 幂等清理玩家强化。核心由 GameMode 创建和销毁。

### 兼容、版本与迁移

新增类和资产都保持 Combat 单 Runtime Module。既有 Release/Contract/Tag/Event/Formula/RNG 不变，新增防守数据和 View 独立版本 1；AssetManager 新扫描目录、AssetValidator 显式识别新类。无旧二进制资产迁移。移除新模式文件及扫描条目即可停用；原 Demo 不改变。

## 4. 实施计划

| 文件/资产 | 变更 | 原因 | blast radius |
| --- | --- | --- | --- |
| `Source/Combat/Combat/Defense/` | 数据、导演、GameMode/GameState、Objective/Enemy、顶部 HUD | 服务器权威防守整局 | 新模式 |
| `Source/Combat/Combat/Tests/CombatDefense*` | 单元/World、真实 PIE、显式开关的网络探针 | 正常/失败/生命周期与实际导航网络证据 | 测试目录 |
| `Source/Combat/Combat/Validation/CombatAssetValidationCommandlet.cpp` | 识别两种新定义 | 防止漏扫 | 新类型扫描 |
| `Config/DefaultGame.ini` | DefenseRun/DefenseWave 扫描 | 稳定资产发现与 cook | 新目录 |
| `Tools/setup_defense.py` | 只创建缺失资产、蓝图编译保存回读 | 可复建独立内容 | Defense 新资产 |
| `Tools/run_defense_smoke.ps1` | 单/双人独立进程，唯一日志与精确进程清理 | 网络验证可重复 | 本地测试 |
| `Content/Combat/Demo/Defense` 与对应 ExternalActors/Objects | 独立地图、5+1 波、单位、强化、框架蓝图 | 可玩入口 | 新目录 |
| README、文档索引、20-05、00-01、00-04、本 Spec | 当前事实、入口、验证和剩余范围 | 文档同步 | 导航/台账 |

## 5. 验收标准（AC）

- [x] AC-01：独立可玩地图、单英雄、两路敌人、核心、5+1 波及只读阶段/核心 HUD。
- [x] AC-02：Scheduler 驱动并严格淘汰旧局/阶段/生命，核心死亡 exactly-once、同批次失败优先。
- [x] AC-03：出生失败/异常销毁不获胜，有限重试和超时，EndPlay 清理句柄/敌人/委托。
- [x] AC-04：固定强化、每玩家每波奖励幂等、死亡待领和安全复活可验证。
- [x] AC-05：Editor/直接 Automation、资产编译保存冷回读、导航/PIE、单/双人 Dedicated smoke 通过；三 Target 构建通过。
- [x] AC-06：文档/ADR/GAP/台账、中文公开说明与代码一致，保留后续阶段边界。

## 6. Definition of Done

- [x] AC 逐项有证据，不把测试注入当作真实导航/网络。
- [x] 无直接写 Health、Actor Timer、客户端结算或测试探针常态运行。
- [x] Editor、Automation、资产、PIE、网络与必要 Target 构建结果完整。
- [x] F2 发现关闭；文档校验、差异检查、delivery Gate 通过。
- [x] 未提交、未推送；待用户验收。

## 7. 测试矩阵与命令

报告均位于忽略目录 `Saved/DEF-002/`，不提交机器绝对引擎路径。

| 层级 | 命令/用例 | 当前证据 |
| --- | --- | --- |
| 入口/计划 | `python -B Tools/task_gate.py --mode preflight/plan/build --spec Doc/CombatSystem/Specs/DEF-002-defense-phase1-skeleton.spec.md --kind feature` | 0.1 preflight/plan/build 与 0.2 plan/build 均通过，JSON 在任务报告目录 |
| Red | `Automation RunTests Combat.Defense.DataValidationAndDefaultWavePlan` | `red.log` 真实失败：旧实现接受空 Boss；随后新增拒绝。0.1 是先写实现后补测，不虚构 TDD Red |
| Editor | 安装版 `Build.bat ue_gasEditor Win64 Development <project> -WaitMutex -NoHotReloadFromIDE -NoUBA` | `editor-build-console.log` 最终 Succeeded；原 `editor-build.log` 保留首轮证据 |
| World Automation | `Automation RunTests Combat.Defense.` | 直接 11 项在 `combat-isolated.log` 中全部 Success；真实 PIE 在 `pie-isolated.log` 成功，含位移和真实核心伤害 |
| 资产/蓝图 | `-ExecutePythonScript=Tools/setup_defense.py`；冷启动只回读 | `assets-readback.json` 六波/两路线/蓝图；生成期间复制资产的注册 ID 警告在冷启动 `assets3.log` 消失；`asset-validation.json` 扫描 56 定义，0 error/0 warning |
| 导航 | `-run=CombatNavigationBuild -Map=/Game/Combat/Demo/Defense/L_Defense` | `navigation.log` Projected=1 Saved=1，0 error |
| 独立单人 | `Tools/run_defense_smoke.ps1 -Players 1` | 最终 `network-victory-1/report.json` server/client Pass，六波、真实位移/核心伤害、复活、金币/经验、客户端 RPC 与终局复制；最终资产使用 BP_DefenseInvader/Boss |
| 独立双人/失败 | `Tools/run_defense_smoke.ps1 -Players 2`；`-Players 1 -Defeat` | `network-victory-2/report.json` 三进程 Pass；`network-defeat-1/report.json` 两进程 Pass。双人六波、两路实际移动/核心受击、复活、金币 1370/XP 2000、客户端 RPC 与终局复制已核对 |
| 源码 Target | `Build.bat ue_gasServer/ue_gasClient Win64 Development <project> -WaitMutex` | `server-build-final.log` 与 `client-build-console.log` 均 Succeeded；源码 Editor 误触发全引擎 3541 actions 已主动停止，改用安装版完成普通 Editor |
| 完整回归（分进程） | 全部已发现的 `Combat.*`：169 个非 PIE 精确名称 + 4 个 PIE | `combat-isolated.log` 169/169、`pie-isolated.log` 4/4，总计 173/173，全部 Success。首次混合运行在切地图时复现已有 GAP-027 的旧 CDO 引用泄漏，原 `combat-regression.log` 崩溃记录保留，未修改旧测试或隐藏失败 |
| 文档 | `python -B Tools/validate_docs.py --report Saved/DEF-002/docs.json`、`git diff --check` | 最终 `docs.json` 94 文件/510 链接 0 error，`git diff --check` 通过 |
| 交付 | `python -B Tools/task_gate.py --mode delivery --spec Doc/CombatSystem/Specs/DEF-002-defense-phase1-skeleton.spec.md --kind feature` | `delivery.json` 记录最终 Gate；本地交付完成，等待用户验收 |

本阶段未覆盖 3–4 人、长时 soak/网络损伤、打包发布、完整美术和手工游玩平衡。安装版独立 -server/-game 证据不声称是打包 exe。

## 8. 风险、回滚与升级

- 风险：灰盒只验证模式结构，当前波次数量/数值未做玩家平衡；敌人路径故障最迟以波次超时失败，暂无动态替换出生门；多人缩放与复杂 AI/内容留后续阶段。
- 回滚方式：删除本任务新目录/资产和新扫描条目，回退新类扫描/文档导航增量；不回退旧 Demo 或其他用户文件。
- 触发升级：新玩法阶段、网络权威/旧 schema 变化、跨局持久化、公开 RPC/发布契约变更。
- 需要人决定的问题：没有阻塞本次可回滚实现的未决项；后续英雄技能包、经济和内容调优另立 Spec。

## 9. 交付证据

- 代码/资产 diff：见 §4；只包含本次新增防守模式及必要注册/导航。
- 构建结果：安装版 Editor、源码 Server/Client 三 Target 均成功；最终日志见 §7。
- Automation/PIE/Dedicated 报告：见 §7；173 项分进程回归、56 定义校验、导航冷回读、真实渲染 PIE 截图与独立网络结果。
- F2 审查：PASS。本地反向检查 Spec、最终源码/资产引用、真实测试与日志，已修正空 Boss 获胜、同步死亡判胜、生成 ID 漂移、Blueprint 定义覆盖、Controller/尸体清理、重复卡与关闭导演残留强化、网络脚本误读旧 Pass。无第二位独立审查者。
- 未执行验证及原因：3–4 人容量、长时 soak/网络损伤、cook/打包 exe、手工完整游玩及平衡未执行，属于后续容量/发布/用户体验验收。混合进程全量曾失败，已按 GAP-027 的隔离方式完成相同全部 173 项，不宣称原混合运行通过。UE MCP 无暴露工具，使用 UE 原生 Python/Commandlet 降级并冷回读。
- 剩余风险：见 §8。

## 10. 变更记录

| 版本 | 日期 | 修改 | 原因 |
| --- | --- | --- | --- |
| 0.1 | 2026-10-15 | 初始 C++ 骨架计划及实现 | 用户授权 Phase 1 |
| 0.2 | 2026-10-15 | 重新审查后恢复可玩地图/固定强化/奖励/复活/网络范围，修正出生失败和同批次判定，统一最终文档 | 初稿范围过窄、错误清场规则需要修正 |

## 11. 路由、自评与 Reflect

### 路由记录

主执行 combat-feature-development，置信度 high；新增模式编排和资产验证，已有公共 Modifier 足够表达固定强化。

### 交付自评

| 维度 | 权重 | 分数（0–5） | 证据与扣分原因 |
| --- | ---: | ---: | --- |
| 需求与 AC | 20% | 4.5 | Phase 1 可玩整局与退出条件闭合，未人工平衡 |
| 架构与权限 | 20% | 4.6 | 公共入口、服务器权威、生命周期/代次回归通过 |
| 实现与数据 | 20% | 4.4 | 稳定资产、独立地图/固定强化/共享奖励，内容仍为灰盒 |
| 验证证据 | 20% | 4.5 | 三 Target、173 项分进程测试、资产/PIE/网络；混合进程受已有 GAP-027 限制 |
| 文档与可观测性 | 10% | 4.5 | Spec/ADR/台账、诊断原因与可回读报告 |
| 交付卫生 | 10% | 4.5 | 新资产隔离，未提交/推送，生成文件不纳入 |

- 计算总分：`4.5/5.0`（加权 4.5）。
- 硬性封顶或未执行项：适用最低矩阵已执行；GAP-027 采用分进程等价覆盖，cook/4 人/长时与手工平衡范围未执行。
- 自评结论：`EVIDENCE_SUFFICIENT`
- 用户验收状态：`已验收`

### Reflect 与调优

- 观察与证据：最初以空计划快速推进会造成假胜利，真实 Red 已证明；骨架必须先定义失败与正常清场的区别。
- 根因类别：单次实现。
- 调整文件与预期收益：只修本任务数据校验/延后终局/测试/Spec，不因一次失误修改通用 Skill 或流程。
- 回归验证：见 §7。
- 需要用户决定的问题：后续 Phase 2 与内容验收，不阻塞 Phase 1 的实现和本地验证。
