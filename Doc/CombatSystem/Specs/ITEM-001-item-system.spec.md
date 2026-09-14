# ITEM-001 服务器权威物品、场景交互与 HUD

> Spec 版本：`0.1`
> 状态：`已验收`
> Owner：Codex / 用户本地验收
> 创建日期：2026-09-13
> 关联进度台账：[00-01](../00-Project/00-01-Progress-Tracker.md)
> 风险等级：`L2`

## 0. Intake 与 Gate 记录

- 用户请求：参考 DOTA2，为现有 Combat 实现物品主动/被动、场景掉落、右键走近拾取和 HUD；用户审阅上一轮完整设计后明确回复“审核完毕，开始做吧”，授权实现该设计及已说明的物品发布契约/schema 迁移。
- 附件解释：无上传附件；上一轮会话设计和交互示意为已批准需求参考。示意使用的颜色/数值不替换既有英雄技能；首版样例数值进入独立 DataAsset。
- 已读取入口：`agent.md`、`README.md`、`Skills/combat-task-router/SKILL.md`、`Skills/combat-feature-development/SKILL.md`、`Skills/combat-skill-development/SKILL.md`；DDD `10-01`、`10-03`、`10-04`、`10-05`、`10-06`、`10-09`、`10-10`、`10-12`、`10-13`，`20-01`、`20-02`、`20-03`、`90-16`、`00-03`、`00-04`、`00-05` 与 `30-01`。
- 主 Skill：`combat-feature-development`（`Skills/combat-feature-development/SKILL.md`）。
- 备选 Skill 与排除理由：`combat-skill-development` 作为样例 Ability/Modifier 专项复用；主任务是库存领域与通用 Order/ASC/UI 集成，普通主动优先复用 DataDriven Action。
- 路由置信度：`high`
- F0 结论：`GO`
- F0 依据：用户已确认完整设计并授权开始，涵盖新增契约；开工工作区干净、无 `.codegraph/`。`python -B Tools/ue_environment.py check --json` 返回 editor_ready/dedicated_ready=true，无运行中的 Editor。当前会话没有 UE MCP 工具，HTTP initialize 端点拒绝连接；资产阶段启动 Editor 后重新发现，必要时降级为同版本 Unreal Python 和命令行回读。
- F1 结论：`APPROVED`
- F1 审查人：用户（设计与契约授权）及 Codex（实施计划逐项复核）。
- F1 审查版本：0.1
- F1 计划审查证据：`Saved/Items/PreflightGate.json` 和 `PlanGate.json` 均通过（0 errors）。用户已逐项审阅上一轮设计并明确授权开始，包含发布/schema 迁移；Codex 将该范围映射为版本 0.1 的 AC、文件依赖、生命周期、权限、验证矩阵与成组回滚并完成复核。普通主动复用 Action，Item 冷却唯一归注册表，同款 Spec 使用实例来源，网络和资产必须真实验证；未增加经济或未授权范围。审查时尚无行为文件修改，批准当前 0.1。
- Build 解锁：F1 已批准；第一次行为修改前执行 build Gate，结果记录 `Saved/Items/BuildGate.json`。
- F1 重审条件：范围、架构、权限、迁移、测试矩阵或回滚实质变化时先递增 Spec 版本，F1 回到 REVISE、状态回到 PLAN_REVIEW；用户现有授权内可逆实施细节由 Codex 复核，超出已审设计才请求新增决定。
- F2 结论：`PASS`
- Push-Ready 结论：`READY`
- 用户验收：2026-09-14，用户明确回复“验收完毕，提交吧”，确认 ITEM-001 通过并授权本地提交；不包含推送授权。
- 验证：Editor/Server/Client Development 及最终联机编排器增量构建、全量 Combat 82/82（81 无警告成功、1 预期用法警告成功）、资产 29/29、迁移器 3/3 和真实冷启动 PIE 已通过。Dedicated 双客户端完成使用/转移循环、同物品争用、控制权互换、旧请求拒绝和 64/256 容量预算检查。文档 70 篇 / 373 个本地链接、diff 检查和 delivery Gate 通过；实际日志见 §7。
- 未执行：cook/打包后的 Server/Client 可执行文件、长时间浸泡、人工丢包/高延迟；本次最低矩阵使用同版本 Editor 的真实独立服务器与双客户端，Server/Client Target 由源码引擎单独构建。2026-09-14 验收收尾只执行用户授权的本地提交，不推送。

## 1. 目标与范围

### 目标

让 Demo 中的英雄能携带、交换、使用、放下并重新拾取同一件物品；服务器结算效果，HUD 只展示权威投影。

### 范围

- 六格装备、三格背包；装备生效，背包休眠，背包换入装备休眠 6 秒；物品实例跨放下/拾取/死亡保留冷却、数量与充能。
- Item DataAsset、稳定 Item Handle、World 注册表、Inventory 组件与地面 Actor；关卡预放和运行时掉落使用相同入口。
- 被动属性/唯一组/Hook/Aura 使用既有公共入口；主动技能按物品实例授予，复用 Cast、Targeting、Aim 和统一提交。
- 拾取/放下接入 Order、AI 导航和最终回执；换格即时事务不打断移动。已有 RPC 安全策略继续生效。
- 物品 HUD、悬停详情、使用、右键操作、拖动换格/丢地面、六个可重映射快捷键、禁用原因、冷却/休眠和数量。
- 演示内容：护甲指环、移速鞋、治疗消耗品、雷击法杖、持有光环和攻击触发物品；属性和技能平衡值均由 DataAsset 提供。
- 物品来源贯穿 Combat 事件与玩家日志；发布、标签、事件和展示的必要版本迁移与自动化保护。

### Non-Goals

商店、金币、购买/出售、配方合成、信使、仓库/专用槽、多单位编队、存档和跨进程 Replay；不推送或对外发布。本地提交已由用户在 2026-09-14 验收后另行授权。

## 2. 当前事实与依据

以下是第一次行为修改前的定位快照，行号对应当时版本；最终实现与当前契约见 [10-14](../10-Architecture/10-14-Item-System.md)。

- `CombatAbilitySystemComponent.cpp:81/109` 的 GrantCombatAbility 按 Ability DefinitionId 唯一；`:176/207` 的 RemoveCombatAbility 会清理冷却，不能直接用于持久物品身份。
- `CombatHUDView.cpp:36` 按 AbilitySpec 顺序取四槽，需排除物品来源；服务器成长入口也要拒绝物品升级。
- `CombatOrderTypes.h:72` 仅含 Unit/Point/Spec 请求，需为物品交互增加明确且互斥的载荷。
- `CombatTypes.h:291` 的 SourceContext 缺少物品来源；`CombatReleaseContract.cpp:34` 显式禁止 v1 物品能力；当前展示 schema 6。
- 当前 `WBP_CombatHUD` 六格物品与三格背包为占位；当前输入统一 Enhanced Input，右键瞄准取消必须消费整个手势。
- 台账最近全量 Combat 70/70、资产 17/17 是历史证据；本次修改必须重新验证。

## 3. 行为与契约

### 主流程

客户端 UI/输入 → owning Unit 安全 RPC → Order 或即时库存事务 → 服务器 ItemSubsystem → Inventory 效果绑定 / WorldItem → 只读 View / 最终结果 / Combat Log。

主动：物品实例 → 独立授予 Spec → 现有 Cast/追近/前摇/引导/Action；激活、提交和旧 RPC 均复核当前物品归属与装备状态。物品技能不进入 QWER、不使用英雄技能点。

### 状态转换

World ↔ Equipped ↔ Backpack；从 Backpack 移出开始/保留 6 秒休眠，不能通过先丢再捡规避；装备格内换位不重置休眠或冷却。Consumed/Destroyed 为终态，旧 Handle 无效。普通死亡保留实例并结束活动施法/持有光环；复活只恢复缺失的有效被动。

物品冷却由 ItemSubsystem 维护唯一记录，装备按 1 倍、背包/地面按 0.5 倍推进；切换位置先结算上一时间段。已提交冷却缩减冻结，休眠与冷却并行。共享冷却可配置组，转移不能刷新已提交实例冷却。数量与充能分开；最后一份消耗提交后延迟到本次激活安全结束再撤销 Spec，不能同步销毁正在执行的 Ability。

### 输入、输出与数据约束

- `CombatItem:lower_snake_case` 定义，Class → AbilityData 单向引用；DataAsset 展开字段显式中文 DisplayName/ToolTip、范围与空值语义。
- 请求只含物品 Handle、版本、格位或原始目标，不接受客户端数值/类/等级/命中列表。物品归属修改递增版本，显示倒计时不改版本。
- 拾取先尝试完整兼容堆叠，再装备空位，最后背包空位；容量不足不删除地面物品。堆叠只合并相同定义、绑定规则及兼容实例状态，保留较晚休眠，不能合并刷新冷却。
- 主动/被动冷却、堆叠唯一组、死亡掉落、是否可丢弃/进入背包为数据策略。默认普通地面物品公开拾取，绑定可限制原持有者/友方，关系使用 TeamSubsystem。
- 地面网格仅查询碰撞、不阻挡导航；点击实际命中后分流，不寻找最近物品、不穿透遮挡。
- 非法位置、无路径、满包、物品丢失、旧版本、正在施法、缺蓝/充能、休眠/冷却/禁用分别返回稳定 FailureTag。

### 权威边界与权限

注册表统一持有实例与位置真值，Inventory 只提供所属单位操作和绑定，不维护最终属性。属性只来自 ASC/GE；资源走公共 GE/事务。持有被动移除不撤销已经施加到目标的普通效果/已发射弹体。物品来源复制为不可变身份；后续命中不重新从当前背包猜来源。

沿用所有权、正 RequestId、20 requests/s、突发 32、128 重放窗口和有界载荷；即时库存事务复用同一连接安全状态。普通 Move/Cast 的历史规则保持，物品拾取/放下使用公共 Targeting 点/交互校验及同一服务器 PathFollowing；不添加客户端导航或 Actor gameplay Timer。

### 失败、取消、过期、死亡、EndPlay 与重复请求

服务器同步事务先校验/准备，再提交位置与效果，提交前失败保持原位置；同步事件重入受事务锁保护，不向观察者暴露可重复拾取中间态。两人竞争同一物品只成功一次，不在远距离追近时长期占用。

拾取/放下的异步行为校验 Item Handle/Revision、Order Handle、LifeGeneration 与控制绑定；Stop、替换、死亡或目标销毁均取消。最终结果独立于初始 Accepted，成功/失败只完成一次。活动物品禁止移出装备栏；同栏换位允许。Unit EndPlay 清理该单位持有实例、Spec、Modifier、Aura、Schedule 和 View；World teardown 注销所有记录，地面 Actor EndPlay 幂等回收。控制转移只更换请求权，不复制或重建物品。

### 兼容、版本与迁移

新增 ADR-055，当前发布身份进入 `combat_v2_items_rc1`、ContractVersion=2，新增 ItemsEnabled=true/EconomyEnabled=false 的独立能力字段；保留旧合并字段为废弃兼容字段，不据此宣称已实现经济。Formula/RNG 和旧定义内容保持 v1。GameplayTag schema 升至 2，旧标签不改名；SourceContext/事件 schema 升至 2，新字段为空时表达旧事件，离线旧 JSON 明确补零/空来源；未知未来版本拒绝。HUD 展示 schema 升至 7，日志投影 schema 升至 2，同版本客户端与服务器一起部署。旧 Unit/DataAsset 默认空库存，已有技能行为不变；新增资产纳入 AssetManager 扫描。历史 M8 证据不改写。

## 4. 实施计划

| 文件/资产 | 变更 | 原因 | blast radius |
| --- | --- | --- | --- |
| `Combat/Items/*`、Data/定义注册与验证 | 实例/数据/事务/地面 Actor/库存绑定 | 物品唯一身份与生命周期 | 新领域 |
| ASC、GameplayAbility、Modifier/Aura、Core 来源与标签 | 物品授予、统一提交/冷却查询、因果链与被动来源 | 同款物品、旧入口防护与公共结算 | 战斗扩展面 |
| Order、Network、Targeting、Unit | 拾取/放下/即时换格、严格载荷、最终回执、死亡/teardown | 权威场景交互 | 全部指令需回归 |
| PlayerController、Aim、HUD/View、Log | 六槽输入、目标预览、拖放、快照/日志 | 可玩交互与只读显示 | 本地 UI + owner-only 复制 |
| Release/Version/序列化及相关 Automation | v2 物品发布边界与旧数据迁移 | 契约诚实可验证 | 全量发布矩阵 |
| `Content/Combat/Definitions/Items`、`Content/Combat/Demo`、输入/UI 资产 | 物品定义、Ability Blueprint、网格/图标、Demo 掉落和九格接线 | 真实可玩内容 | 蓝图编译/回读 |
| `Combat/Tests/CombatItem*`、网络场景 | 正反例、实例/效果/输入/资产/teardown 与网络 smoke | 行为证据 | 验证基础设施 |
| README、10-14、10-01/03/08/09/12、20-03、台账/ADR | 已实现行为与配置说明 | 文档一致 | 当前文档 |

依赖顺序：定义/注册表 → 容器事务/被动 → 物品主动 → Order/输入 → View/HUD/资产 → 全量集成验证。每阶段保持可编译，问题通过直接测试定位；不为了方便测试引入生产旁路。

## 5. 验收标准（AC）

- [x] AC-01：唯一物品实例在 World/装备/背包之间转移；重复/旧版本、满包、禁用和无路径安全失败。
- [x] AC-02：两件同款物品可并存，被动仅撤销所属实例；唯一组/光环无重复，复活/重绑幂等。
- [x] AC-03：背包与 6 秒休眠关闭主动被动，冷却半速且换位/丢捡不刷新；时间全部服务器权威。
- [x] AC-04：物品主动复用公共 Ability/Action，资源和数量只提交一次；最后一件、安全取消、旧 Spec、缺蓝、目标与状态检查正确。
- [x] AC-05：右键物品走近拾取；拖到地面走近放下；S/替换/死亡/控制转移淘汰旧回调，不重复完成。
- [x] AC-06：六装备三背包真实接线，使用/菜单/拖放/详情/快捷键/休眠/冷却/数量可见，UI 不穿透；QWER 与技能加点不包含物品。
- [x] AC-07：Demo 样例资产能真实游玩、编译保存回读；物品来源正确进入日志，旧事件迁移可验证。
- [x] AC-08：Editor/Server/Client、直接与全量 Combat、资产、PIE、Dedicated 双客户端、容量与 teardown 有实际证据。

## 6. Definition of Done

- [x] 先记录真实 Red，再实现 Green；不将编译当成行为通过。
- [x] 服务器唯一来源、公共 API、中文说明、资产反射和全部失败路径闭合。
- [x] 全量及网络/资产验证完成，或如实记录未执行原因与命令。
- [x] F2 问题关闭、六层 Push-Ready、文档与 diff 检查通过。
- [x] 代码/资产/文档/schema 同步；未混入用户修改，不提交生成文件。

## 7. 测试矩阵与命令

| 层级 | 用例/命令 | 预期证据 | 结果 |
| --- | --- | --- | --- |
| 文档/流程 | `python -B Tools/task_gate.py --mode preflight/plan/build/delivery --spec Doc/CombatSystem/Specs/ITEM-001-item-system.spec.md --kind feature`；`python -B Tools/validate_docs.py`；`git -c core.safecrlf=false diff --check` | `Saved/Items/*Gate.json`、`ReleaseDocs.log`、`ReleaseDiffCheck.log` | 四个阶段分别执行，均 0 errors；最终文档 70 篇 / 373 个本地链接 / 0 errors，diff 检查通过 |
| Pure/World Automation | `Automation RunTests Combat.Items.`；冷却、转移、激活、竞争、旧身份、提交观察者死亡和资源基础值上限 | `Saved/Items/ReleaseTests/index.json`、`ReleaseTests.log` | 10/10，通过全量运行执行 |
| 全量回归 | `Automation RunTests Combat.` | 同上；82 项含既有技能、输入、HUD、日志、成长、网络与生命周期 | 82/82，0 failed、0 notRun；唯一警告是 DebugAddExperienceCommand 故意传入非法参数后的用法提示 |
| Build | `.env` 对应 Build.bat 的 ue_gasEditor/Server/Client Win64 Development，追加 `-WaitMutex -NoHotReloadFromIDE -NoUBA` | `Saved/Items/ReleaseEditorBuild.log`、`Release-ue_gasServer-Build.log`、`Release-ue_gasClient-Build.log`；最终 `NetworkBoundaryEditorBuild.log`、`NetworkBoundary-ue_gasServer-Build.log`、`NetworkBoundary-ue_gasClient-Build.log` | 三 Target 与最后的测试编排器增量均 Succeeded；Editor 使用安装版 UE 5.8.2，Server/Client 使用源码 UE 5.8.2 |
| Blueprint/资产 | UE MCP Read/Mutate/Verify 与 Unreal Python 创建/编译/保存/冷重载；`-run=CombatAssetValidation` | `Saved/Items/VerifiedAssetValidation.json`、`VerifiedAssetValidation.log` | 29 扫描、0 error、0 warning；蓝图保存后冷启动实机通过 |
| PIE | 真实 Demo 鼠标/键盘输入及只读状态回读；生命周期另由 Automation 执行 | `Saved/Items/ReleasePIE-Wand.json`、`ReleasePIE-Mana.json`、`ReleasePIE-DropState.json`、`ReleasePIE-PickupState.json`、`ReleasePIE.png` | 满蓝等待后法杖伤害 160、费用 40、冷却 16；指环拖出/拾回护甲 10→5→10、地面数量 6→7→6、同实例修订 4；早期 PIE 已检查药剂 3→2→1、热键、菜单、背包 6 秒休眠 |
| Dedicated | `Tools/RunDedicated.ps1 -Items -InstalledEditor -Port 7879 -TimeoutSeconds 110`；真实 RPC、拥有者快照、争用和控制转移 | `Saved/UEEnvironment/Dedicated-Installed/DedicatedSummary.txt` 与三端日志 | 退出码 0；三端 HUD schema 7 和 ItemNetworkSmoke 均 Pass；两个客户端各 2 个循环最终回执，争用恰好 1 Won / 1 Lost；控制互换后旧代次 RPC 拒绝、旧拥有者快照隐藏 |
| Capacity/teardown | 64 Unit/256 Modifier 容量回归；物品 Owner EndPlay、World teardown、Aura 与组件槽清零 | Dedicated 性能日志、`Combat.Items.DeathRespawnAuraAndWorldCleanup`、`EquipmentCooldownAndReequip` | 64/256、Mixed 2 / Minimal 62，CapacityBudget 与 Performance Budget 均 Pass；P95 9.111 ms、P99 10.077 ms、最大连接输出 11.526 KiB/s。生命周期计数清零通过；这不是长时间浸泡结果 |
| JSON 迁移 | `python -B -m unittest discover -s Tools/Tests -p 'test_migrate_item_events.py' -v` | 旧来源保留、补空物品字段、未来版本和重复输出拒绝 | 3/3 |

Automation 使用安装版同版本 UE 的 `UnrealEditor-Cmd.exe ue_gas.uproject -unattended -NoP4 -NoSplash -NullRHI -NoSound -ExecCmds="Automation RunTests Combat." -TestExit="Automation Test Queue Empty" -ReportExportPath=<报告目录> -AbsLog=<日志>`，退出码为 0。资产命令使用 `-run=CombatAssetValidation -Report=<JSON>`。本机真实引擎入口由 `.env` 与 `Tools/ue_environment.py check --json` 解析，不把本机绝对引擎路径写入可移植脚本。

## 8. 风险、回滚与升级

- 风险：重复定义 Spec/冷却绑定、移除最后一件时同步重入、延迟导航结果、库存/Spec/View 跨流乱序、资源最大值换装、同款唯一被动、资产/版本部署不一致。
- 回滚方式：成组恢复本任务源码、配置和明确修改的 Demo/UI 资产；新增物品定义与引用一起撤销；联机双方一同回滚。不开启双权威模式，不修改历史验收结论。
- 触发升级的条件：必须改变已审玩法、引入经济或额外权威来源、不可逆存档迁移，或三轮相同问题仍无法收敛。
- 需要人决定的问题：当前无；用户已明确完成设计审核并授权开始，后续实施细节在该范围内推进。

## 9. 交付证据

- 代码/资产 diff：新增 `Combat/Items`、物品 HUD/地面标签、直接和联机测试；接入 ASC/Ability、Order/RPC、Modifier/Aura、SourceContext、View/日志、单位出生与生命周期。新增六种物品、主动/被动定义、两种 Ability Blueprint、六个输入动作和物品槽蓝图；修改 Demo 地图及六个 World Partition 地面 Actor、卓尔初始库存与 HUD。配置、版本、迁移器及 DDD 同步。
- TDD：`Saved/Items/RedBuildInstalled.log`、`RedAutomation.log` 保留缺少物品基础类时的真实 Red；最终 Green 见 §7。实机另捕获 `PIE-ManaOverflow-Red.json`（基础法力 451、显示 120），修正后 `ReleasePIE-Mana.json` 为基础/显示同时 80.5（已扣 40 后发生一次自然回复）。
- F2 第一轮：费用 GE 同步回调可在消耗/冷却前触发死亡；现先准备物品提交状态，再广播 GE，失败回滚并复核死亡。`CommitObserverDeathIsAtomic` 通过。放下时先交接地面归属再撤销被动，消除同步观察者看到半完成库存的窗口。
- F2 第二轮：满资源自然回复累积隐藏 BaseValue；`PreAttributeBaseChange` 应用既有 NumericPolicy 的资源上限，没有改变公式版本。`ManaCostAfterRegenerationAtCap` 与冷启动法杖实机通过。
- 测试基础设施：固定 X 偏移放下在容量场景中可能落进另一英雄的胶囊，服务器正确拒绝 LOS；保存 `Dedicated-FixtureLOS-Red.txt`，改为当前单位脚下的可校验落点。抢拾仍使用场景中同一件物品与两端真实 RPC，不绕过导航/权限检查；smoke 仅在显式命令行开关下生成。
- 最终联机复验：两端先保存同一地面物品及修订，再各自发送拾取 RPC，观察到一胜一负且服务器仅有一个所属实例。服务器经公共控制绑定 API 交换主控单位，库存保持在原单位；两客户端收到新拥有者库存，旧单位公开查询返回空库存，并通过真实 RPC 确认旧控制代次的换格请求没有执行。脚本只采集本轮新日志，旧报告不能冒充新运行成功。
- 工具降级：Editor 可运行后使用真实 HTTP UE MCP 操作资产与 Slate UI；未暴露的反射属性使用同一 Editor 的 Unreal Python。源码 Editor 启动在初始化日志前超时，使用安装版 UE 5.8.2 的独立 `-server` 与两条 `-game` 进程完成运行验证，源码 Server/Client Target 构建单独留证。
- 未执行验证及原因：未做 cook/打包、长时间浸泡和人工网络损伤矩阵；它们超出首版本地最低验证，不能从三 Target 编译或短时容量数据推断已通过。复现当前网络层使用 §7 命令；后续打包需按目标平台完成 cook 后启动一服两客户端，再追加延迟/丢包和长期运行。
- 剩余风险：样例数字与简称/灰模用于功能演示，未完成物品经济和平衡测试；打包部署与恶劣网络仍需专项验证。插件在非 Editor 模式的 Python 初始化噪声已单独核对，不等于 Combat 检查失败。用户于 2026-09-14 验收通过并授权本地提交，未授权推送。
- 验收收尾：本轮只更新验收记录并提交此前已验证的 123 个任务文件；重新执行文档、delivery Gate、暂存差异和 Git LFS 检查，沿用 §7 已通过的运行时证据，不重复构建或运行 UE。

### Push-Ready 六层

| 层级 | 结论 | 已执行证据 |
| --- | --- | --- |
| Tests | PASS | 82/82 Combat、迁移器 3/3、资产 29/29、真实 PIE 与 Dedicated 三端结果 |
| Types/Build | PASS | 最终 Editor / Server / Client Development，六个构建日志见 §7 |
| No Regression | PASS | QWER、成长、日志、SAM、拥有者 HUD、64/256 容量及 teardown 纳入本次验证 |
| Adversarial | PASS | 单人本地 F2 两轮发现均修复；`Saved/Items/F2Review.md`，没有宣称外部独立审查 |
| DDD/Constraints | PASS | 单 Runtime Module、GE 属性唯一来源、公共 Targeting/Order、Scheduler、中文配置元数据与当前文档同步 |
| Decisions | PASS | 用户批准的 Spec 0.1、ADR-055、schema 与离线迁移闭合；经济仍关闭，无新增待决定项 |

交付机器命令：`python -B Tools/task_gate.py --mode delivery --spec Doc/CombatSystem/Specs/ITEM-001-item-system.spec.md --kind feature --report Saved/Items/DeliveryGate.json`。2026-09-14 用户已完成 Demo 验收；验收收尾的机器报告另存 `Saved/Items/AcceptedDeliveryGate.json`，保留原交付报告。

## 10. 变更记录

| 版本 | 日期 | 修改 | 原因 |
| --- | --- | --- | --- |
| 0.1 | 2026-09-13 | 将用户已审核的完整物品设计冻结为实施计划 | 用户明确授权开始实现 |
| 0.1 验证记录 | 2026-09-13 | 实现原范围，关闭提交重入与资源基础值问题，补全资产、实机、真实双客户端争用及控制互换证据 | 实施和验证回写，不改变已批准的范围、架构、权限、迁移与回滚 |
| 0.1 用户验收 | 2026-09-14 | 状态改为已验收，记录本地提交授权 | 用户明确回复“验收完毕，提交吧”；不改变玩法或运行时实现 |

## 11. 路由、自评与 Reflect

### 路由记录

通用功能开发为主，技能专项复用；本次从纯设计切换为实现，建立真实 Spec。路由 high，未扩展商店/经济。

### 交付自评

根据最终构建、行为验证与交付记录评分；用户于 2026-09-14 独立确认验收通过，自评不替代该确认。

| 维度 | 权重 | 分数 | 证据与扣分原因 |
| --- | ---: | ---: | --- |
| 需求与 AC | 20% | 5 | 六装备/三背包、主动被动、场景与 HUD 闭环；成功/失败、争用、旧身份和死亡清理有直接证据 |
| 架构与权限 | 20% | 5 | 注册表唯一实例、服务器 GE/公共结算、Scheduler、owner-only View；真实两端旧控制代次被拒绝 |
| 实现与数据 | 20% | 4 | 六种可玩定义与蓝图保存冷重载，29 个资产校验；美术仍为简称/灰模，数值仅用于功能演示 |
| 验证证据 | 20% | 4 | 82 项 Combat、三 Target、真实 PIE、Dedicated 双客户端及容量/teardown；未覆盖打包、长时和网络损伤矩阵 |
| 文档与可观测性 | 10% | 5 | ADR-055、10-14、现有专题、事件/日志 schema、迁移器与可复现报告同步 |
| 交付卫生 | 10% | 5 | 初始工作区干净，修改限本任务，无生成文件进入 diff，文档/机器 Gate 和用户验收边界明确 |

加权自评：`4.6 / 5`。交付状态以页首当前状态和机器 Gate 为准。

### Reflect 与调优

- 观察与证据：Automation 验证了费用事务，但满蓝闲置后的实机检查发现基础法力与显示值分离；原生 Slate 鼠标路径也确认了真正的 HUD/地面交互。Dedicated 增加同快照争用与控制权互换后，完整网络边界有实际成功/失败回执。
- 根因类别：单次实现与测试场景问题。费用 GE 的同步观察者、GAS 基础值与聚合当前值、以及固定测试落点与邻居胶囊碰撞需要分别建模，不能依赖短时正常路径。
- 修改与收益：Inventory 提交阶段、AttributeSet 基础资源限幅及两条直接回归；联机编排器保存同一争用版本、校验双方回执并验证旧代次拒绝；smoke 工具只读取本次运行日志。
- 回归：82/82 Combat、冷启动法杖扣蓝、真实争用 1 Won / 1 Lost、控制互换和容量通过。具体最终构建/机器 Gate 见 §7。
- 调优与决策：不修改 Skill、通用模板或流程校验器；当前观察不足以证明跨任务流程缺陷。用户于 2026-09-14 完成游玩验收并授权本地提交，无新增待决定项。
