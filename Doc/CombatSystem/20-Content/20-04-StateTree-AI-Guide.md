# 20-04 StateTree AI 配置与接入

> 阶段 A、阶段 B 均于 2026-09-21 通过用户验收，阶段 C 于 2026-09-22 通过用户验收。阶段 B 证据见 [AI-003 Spec](../Specs/AI-003-statetree-roles.spec.md)，阶段 C 的 Utility/EQS/容量证据见 [AI-004 Spec](../Specs/AI-004-statetree-tactics-capacity.spec.md)；D 阶段仍未开始。完整设计见 [10-17](../10-Architecture/10-17-StateTree-AI-Decision-System.md)。

## 1. 本阶段提供什么

单位已有 `UCombatAIBrainComponent`，它在服务器运行原生 StateTree，通过公共 Order 执行显式 Move、Attack、Cast。树决定准备、执行、完成记账和等待的顺序；Brain 提供跨状态数据和生命周期管理，没有另建行为层级状态机。已有 `ACombatUnitAIController` 继续负责导航与 Crowd。

阶段 B 新增可选范围/LOS 感知、有限记忆、野怪归位和小兵/巡逻路线职责，见 §7。阶段 C 为显式 v2 Profile 增加 HardSelect + Utility、只读主动技能候选、EQS 战术站位和 World 查询预算，见 §8。阶段 A/B 的 v1 Profile 绕过新增预算并保持原时序；无 Profile 的旧单位维持原行为。

## 2. 打开可玩演示

1. 使用 UE 5.8 打开 `/Game/Combat/Demo/AI/L_CombatAI` 并进入 PIE。
2. 演示场生成一名自主卓尔游侠和一个敌方木桩；当前路线导航可达后开始追近并持续攻击。启动等待最长 30 秒，使用 Combat Scheduler；导航缺失时记录 `AIDemoStartupAborted`，不无限重试。
3. 在 PIE 的 World Outliner 选择 `StateTree AI 演示：攻击 / 返回` 的运行实例，Details 中点击 **AI 返回起点**。若正在普攻前摇，先完成本轮发射，再返回生成位置在导航上的投影点。
4. 点击 **AI 攻击靶子** 可重新提供目标并显式恢复自主权。停止 PIE 后实例和调度一起清理。多人 PIE 应操作服务器 World 的实例。

演示地图是独立内容，未修改原 `L_CombatDemo`。生成时碰撞可能调整出生位置；返回点使用调整后位置的导航投影，避免追向不可达的场景原点。`靶子偏移` 默认 `(900, 0, 0)` 厘米。玩家原有单位仍由原输入控制。

当前地图保留了出生点附近的原木桩（XY 约 `1340, 550`），AI 攻击的是演示场另外生成的木桩（XY 约 `454, -454`）。这对 AI 单位不在默认初始视野内，需要平移镜头观察；可通过演示场的“自主单位 / 当前靶子”引用确认对象。出生点的木桩不受这次 AI 攻击影响，不能用它的血条判断 AI 是否命中或复制是否正常。

| 资产 | 用途 |
| --- | --- |
| `ST_CombatAI_Root` | 等目标 → 准备 → 链接执行 → 确认回执 → Scheduler 等待 |
| `ST_CombatAI_Action` | 一个通用 Execute 节点，复用 Move/Attack/Cast 的 Order Bridge |
| `DA_CombatAI_Basic` | 根树、准备有效期和边界持有时长；ID 为 `CombatAIProfile:ai_basic` |
| `BP_CombatAIDemoArena` | Profile、自主单位类型、靶子类型和偏移 |
| `L_CombatAI` | 导航、环境和演示场实例 |

以上资产均位于 `/Game/Combat/Demo/AI`；World Partition 的外部 Actor/Object 数据与地图一起纳入 LFS。

## 3. 给单位启用 AI

在 `UCombatUnitData` 的 **AI 配置** 中选择 `UCombatAIProfileData`，或在服务器初始化单位后调用 Brain 的 **设置 AI 配置**。满足存活、ASC 初始化、单位 DefinitionId、导航 Controller 和有效 Profile 后，且未被玩家接管，Brain 才启动。客户端即使本地生成一个 Authority Actor 也不能启动 Brain。

| Profile 字段 | 默认值 | 配置含义 |
| --- | --- | --- |
| `DefinitionName` | 由作者填写 | 同类型唯一的 `lower_snake_case`；不要复制既有 ID |
| `AIProfileVersion` | 1 | AI 配置结构版本；独立于全局 Content schema |
| `RootTree` | 必填 | 已编译、使用 Combat AI Schema 的 StateTree |
| `IntentLifetime` | 1 秒 | 从 Prepare 到 Execute 消费的期限；不是动作超时 |
| `BoundaryHoldSeconds` | 0.25 秒 | Attack 边界 Ready 后允许交接的最长时间；范围 0.01–5 秒 |
| `bEnableTactics` | false | 仅版本 2 可启用；版本 1 忽略战术字段 |
| `Guard/Attack/RepositionUtility` | 0.1 / 0.35 / 0.45 | 合法战术分支的基础效用；最终分数限制在 `[0,1]` |
| `ActionMinHoldSeconds` / `ActionSwitchMargin` | 0.2 秒 / 0.05 | 普通重评的最短保持和严格超越分差；不阻塞死亡、接管等硬失效 |
| `AbilityUsageRules` | 空 | 只解析本单位已授予的主动技能；用途、目标策略、法力保留和边界偏好不改变 Ability 真值 |
| `TacticalLocationQuery` / `RepositionTriggerDistance` | 空 / 0 | 可选服务器 EQS 与过近触发距离；结果只生成精确 Move 意图 |
| `PerceptionBudgetRetrySeconds` / `TacticalQueryRetrySeconds` | 0.05 秒 / 0.05 秒 | World 配额不足后的有界稳定错峰；不阻塞清理、回执或攻击边界 |

新 Profile 放在 `/Game/Combat/Definitions/AI` 或 `/Game/Combat/Demo/AI`，均已加入 AssetManager 扫描及 AlwaysCook。Profile 继承 `UCombatDefinitionData`，必须保留项目当前 Content schema。根树和 Linked Asset 通过资产硬引用进入 cook 闭包。

显式输入通过服务器 Brain 的 `SetObjective(FCombatOrderRequest)` / 蓝图 **设置 AI 目标命令**：

| 意图 | 必需字段 | 结束依据 |
| --- | --- | --- |
| MoveToPoint | `bHasTargetLocation=true`、有限世界坐标 | 原 Order 的导航完成回执 |
| MoveToUnit | 有效单位目标 | 原 Order 到达目标附近 |
| AttackTarget | 有效敌方单位目标 | 持续执行，直到切换、取消或目标失效 |
| CastNoTarget / CastPoint / CastTarget | 本单位已授予的 AbilitySpecHandle，以及匹配类型的目标载荷 | 原 Ability 的 OrderReleased |

例如在服务器已初始化单位后提供位置：

```cpp
FCombatOrderRequest Objective;
Objective.Type = ECombatOrderType::MoveToPoint;
Objective.bHasTargetLocation = true;
Objective.TargetLocation = Destination;
Unit->GetCombatAIBrainComponent()->SetObjective(Objective);
```

AbilitySpecHandle 来自当前单位的真实授予结果，不能使用类默认对象或另一单位的句柄。Targeting/ASC 复核阵营、目标、资源、冷却等条件；超距可交给原 Order 追近。AI 不使用 Order FIFO，不提交 Stop、拾取、丢弃或换槽动作。主动道具的自动选择/库存修订策略尚未提供。

每次设置目标会生成观察版本。正在执行时重复发布完全相同的动作，只更新版本并保持原 Order；已经完成或失败后再次设置目标，代表明确的一次新尝试。失败会记录回执并等待新输入，当前模板不会每帧重试失败命令。

## 4. StateTree 的接线契约

```text
根
├─ 等待显式目标：WaitObjective
├─ 决策作用域：DecisionScope（服务，不参与完成）
│  ├─ 准备命令：PrepareOrder
│  ├─ 链接执行：ST_CombatAI_Action / ExecuteOrder
│  └─ 确认完成凭证：ResolveReceipt
└─ 有界决策间隔：Scheduler Wait（默认 0.1 秒）
```

Prepare 成功进入链接执行，准备失败直接进入 Resolve；执行结束也进入 Resolve。Resolve 后退出整个 Scope，再等待 0.1 秒回到 WaitObjective。任务停止逐帧 Tick，通过本地类型化事件处理新目标、Order 完成和 Scheduler 到期；游戏时间不使用 Actor Timer 或 StateTree 延迟转移。

Prepare/Execute 的 `ConsumerSlot` 必须相同，默认 `Action`。不要把前一个兄弟 Task 的实例作为下一个 Task 的输入。冻结意图保存在 Brain 工作区中，Linked Asset 继承父 Scope；Scope 包含运行、控制、生命和序号，意图还包含准备序号、目标生命、相关版本和有效期。Prepare 成功退出保留数据；条件中断、Scope 退出、死亡和接管使旧数据失效。

Execute 消费准备结果一次。回执独立于动作实例，在动作退出后仍由 Resolve 确认一次；同帧目标变化不能跳过旧动作完成记账。同步完成也有回执，委托回调只记录事实并唤醒，不递归推进树。

内容校验刻意限制可编排结构：

- Schema 仅允许 Combat 原生 AI Tasks、获准的通用条件，以及阶段 C 的项目原生 Consideration；不允许蓝图 Task、Global Task 或 Evaluator。
- 任一活动父子路径至多一个 DecisionScope，Prepare/Execute/Resolve 合计至多一个，命令阶段必须有 Scope。
- 协议 Task 必须启用；临时停用内容应禁用整个分支，不能单独禁用 Scope 破坏交接。
- 复用使用 Linked Asset；当前不开放树内 Linked/Subtree 路由或动态 Linked Override。
- 只使用完成转移；不开放 OnTick、OnEvent 条件跳转或延迟转移。事件用于唤醒节点，由节点读取持久事实。
- Linked Asset 引用不能成环，深度上限 16。运行时 Bridge 仍有单写入者检查，防止不合规节点重复下单。

阶段 B 角色任务和阶段 C 战术任务都继承这些准备/执行/确认协议；不要用蓝图直接调用 MoveTo、TryActivateAbility 或伤害接口来绕过执行链路。

## 5. 切换、接管与生命周期

普通目标变化的处理：Move 可以精确取消；Cast 等待公共 OrderReleased；持续 Attack 申请边界票据。前摇中等 Launch，Ready 后暂停下一次起手，AttackReady 时钟继续。切换精确取消旧 Order；Keep 释放票据并保持原句柄；持有超时自动释放，避免永久停打。晚到通知再次核对票据和完整 Order 身份。

有效玩家/脚本动作通过安全与业务预检后撤销自主权，再走原 Order 入口。第一次追加命令也先接管，后续继续原玩家 FIFO。无效命令不撤销 AI。`SwapItems` 是独立库存事务，沿用“不替换当前动作”的既有契约，不触发接管。外部 Stop 进入 Manual。

Manual 保持到显式 **恢复自主决策**，该调用停止手动队列并创建新运行。修改 Objective 本身不恢复自主权；移除网络 Owner 也不自动恢复。死亡、控制器丢失、Profile 更换和 EndPlay 停树并清理旧资源；仍为 Autonomous 的单位在复活/重新具备条件时启动新运行。Manual 单位复活后仍为 Manual。

`CancelCurrentOrderIfMatches` 只终结完整句柄匹配的当前项，不清空后继 FIFO、不提升整条队列的 generation。取消旧前摇/技能前先摘除旧当前项，禁止旧清理吞掉同步回调新提交的命令。旧 Scope、生命、控制 epoch 或激活回调不得写回新动作。

## 6. 诊断与验证入口

通过 Brain C++ 只读访问器查看 `RunSerial`、`ControlEpoch`、`DecisionScope`、`SubmittedCount`、`ResolvedCount`、`LastReceipt` 与活动 Wait 数。启动失败记录 `AIStartRejected`；用 `-LogCmds="LogTemp Verbose"` 查看 `AIReceipt` 的运行、Scope、Preparation 和实际 Order 诊断。客户端这些执行计数应保持初始值。

首次创建示例树使用 Editor `-run=CombatAIAssets`；然后使用 `-run=pythonscript -script="<仓库>/Tools/setup_ai_demo.py"` 创建演示蓝图和地图。重复执行均回读现有内容，不覆盖作者修改；已有资产编辑后仍须在编辑器编译保存。

验证命令使用 [README 的引擎发现方式](../../../README.md#验证命令模板)，不固定机器安装路径：

```text
ue_gasEditor / ue_gasServer / ue_gasClient Win64 Development
UnrealEditor <项目> -ExecCmds="Automation RunTests Combat.AI" -TestExit="Automation Test Queue Empty"
UnrealEditor <项目> -run=CombatAssetValidation -Unattended -NoP4
Tools/RunDedicated.ps1 -InstalledEditor -AI
UnrealEditor <项目> -run=Cook -TargetPlatform=Windows -Map=/Game/Combat/Demo/AI/L_CombatAI -unattended -NullRHI
```

演示地图须明确加入 cook 的 `-Map` 参数或项目打包地图列表；Profile 的 AlwaysCook 只保证其引用的树，不会反向包含演示地图和场景蓝图。

`Combat.AI.PIE.PlayableArena` 使用保存后的阶段 A 地图启动真实 PIE；阶段 B 为 `Combat.AI.PIE.RolesArena`，阶段 C 为 `Combat.AI.PIE.TacticsArena`。同步 World 测试用于边界时序，不能替代真实帧调度。`-AI`、`-AIRoles` 和 `-AITactics` 不叠加固定的旧 64/256 容量样本；不带这些开关另跑原容量回归。网络损伤、长时间 AI soak 和 D 不在阶段 C 的通过结论内。

## 7. 阶段 B：自动感知与通用角色

新演示入口为 **`/Game/Combat/Demo/AI/Roles/L_CombatAI_Roles`**。使用独立宽视野 Command Pawn，相机仍支持边缘滚屏与 Space 跟随。场景只有两只运行时木桩，没有模板中的旧木桩或物品：

- **野怪 AI / GUARD TARGET**：初始自动发现木桩，追近攻击，目标结束后回到营地；地面 `GUARD > ENGAGE > RETURN` 标识这组区域。
- **小兵 AI / LANE TARGET**：先推进路线，接近敌人后交战，目标结束后继续原航点；地面 `LANE > ENGAGE > RESUME` 标识这组区域。

这是独立示例数值：新 Guard/Lane Unit DataAsset 复用卓尔游侠模型与已有普攻弹体，缩短射程以便清楚观察追近；不修改原卓尔游侠或木桩定义。原阶段 A 场景保持可用。

| 资产 | 组合与作用 |
| --- | --- |
| `DA_AI_NeutralCamp` / `ST_AI_NeutralCamp` | 有序选择 ReturnHome、Engage、Guard；交战后归位 |
| `DA_AI_Lane` / `ST_AI_Lane` | 同一 ReturnHome/Engage，加 LaneAdvance；交战结束继续原游标 |
| `DA_AI_Patrol` / `ST_AI_PatrolRoot` | Patrol 链接 LaneAdvance；服务器职责设置循环路线 |
| `ST_AI_Engage` | PrepareRole(Attack) → ExecuteRole → ResolveRole |
| `ST_AI_ReturnHome` | PrepareRole(Home) → ExecuteRole → ResolveRole，复核到家 |
| `ST_AI_LaneAdvance` / `ST_AI_Patrol` | 共享航点执行与成功记账，Scheduler 到点停留 |
| `ST_AI_Guard` | 等待知识或职责修订，不重复注册感知 |

各根树始终持有一个 Scope，先等合法职责，再按“故障超限 → 故障退避 → 归位 → 交战 → 路线 → 守点”选择。强制离营/超时/失感知由 ExecuteRole 的事件 Tick 精确取消旧命令并保留原因，统一经过 ResolveRole；没有另一条事件转移跳过成功到达凭证。

服务器接入示例：

```cpp
FCombatAIAssignment Duty;
Duty.Home = SpawnNavigationPoint;
Duty.Route = LaneWaypoints; // 空数组为守点；最多 64 点。
Duty.bLoopRoute = false;   // 巡逻设 true。
auto* Brain = Unit->GetCombatAIBrainComponent();
Brain->SetAssignment(Duty);
Brain->ConfigureProfile(RoleProfile);
```

`SetAssignment` 校验失败无副作用；成功重发是显式重试，重置游标和职责故障记录。它不会解除 Manual，手动接管后仍需显式 `ResumeAutonomous()`。开启感知的角色使用职责 API，`SetObjective` 不会覆盖角色职责或解除故障等待。

新增 Profile 配置：感知默认关闭；角色开启后默认半径 800 cm，交战/空闲采样 0.2/0.8 秒，记忆 3 秒，候选 16 / 记忆 32。候选经公共 Targeting 后按定义优先级、`Threat × ThreatWeight − DistanceCm × DistanceWeight`、Actor ID 排序；未配置定义优先级为 0。最短保持/换目标分差用于合法选择点，最小野怪不在持续攻击中普通换敌。受击威胁只从当时仍可见的已知来源更新，未知/隐藏来源不会变成全知追踪目标。

敌人丢失后不更新其 LastSeenPosition、生命或属性；当前 Actor 跟踪命令在下一次采样事件中取消。检测受采样间隔与 Scheduler 延期限制，**范围/LOS 不等于战争迷雾**，要求 `RequireVisible` 的 Profile 会被拒绝。查询目前仍复用 World 枚举；阶段 C 已增加执行前配额和 64/128/256 容量证据，本轮未触发公共空间索引，真实复杂地图若超预算需另立任务。

默认职责边界 1200 cm、交战上限 20 秒、到达复核容差 80 cm。Home 途中普通索敌和受击不换单；只有匹配成功回执且实际 XY 到达才清返回请求。Route 中断不前进游标，终点非循环路线等待，循环路线保留最近成功航点为职责锚点。停留至少 0.05 秒，重合航点不会在一次 Tick 内无限循环。

同职责同操作默认最多 3 次尝试（含首次），失败后 Scheduler 等待 0.5 秒；普通观察不重置计数或跳过等待。超限保持可诊断等待，直到有效新职责或显式恢复。`AIRoleReceipt` 输出操作、取消原因、职责版本、失败次数、游标和归位请求；C++ 只读接口还有 `GetKnowledge()`、`GetRouteCursor()`、`GetHomeCommitCount()`、`GetRouteCommitCount()` 和感知资源状态。

首次创建/回读角色内容与验证：

```text
UnrealEditor <项目> -run=CombatAIAssets -Roles -Unattended -NullRHI
UnrealEditor <项目> -run=pythonscript -script="<仓库>/Tools/setup_ai_roles.py" -Unattended -NullRHI
UnrealEditor <项目> -ExecCmds="Automation RunTests Combat.AI;Quit" -TestExit="Automation Test Queue Empty"
Tools/RunDedicated.ps1 -InstalledEditor -AIRoles -TimeoutSeconds 120
UnrealEditor <项目> -run=CombatNavigationBuild -Map=/Game/Combat/Demo/AI/Roles/L_CombatAI_Roles -unattended -NullRHI
UnrealEditor <项目> -run=Cook -TargetPlatform=Windows -Map=/Game/Combat/Demo/AI/Roles/L_CombatAI_Roles -unattended -NullRHI
```

角色地图是从 World Partition 模板生成的；在首次 Dedicated/cook 前必须运行 `CombatNavigationBuild`，它会解除编辑器载入锁、同步生成并保存 NavMesh，再用原生投影回读确认不是空网格。角色专用 Dedicated 使用新地图，服务器验证自动命中、归位和航点完成，两个客户端独立验证 Brain 未运行、位移和木桩 Health 复制。`-AIRoles` 与其他专项开关分开执行；当前证据和未执行项以 AI-003 为准。

## 8. 阶段 C：Utility、技能与战术站位

新演示入口为 **`/Game/Combat/Demo/AI/Tactics/L_CombatAI_Tactics`**：

- **Hero Bot** 持续普攻后在合法 Attack Boundary 重新检查已授予的自我治疗候选、效用分差与中断偏好，再切换到公共 Cast Order。候选评估本身不会激活、扣费或开始冷却。
- **Ranged Guard** 在目标过近时申请服务器 EQS，选定点后通过公共 Move Order 改变站位；技能目标不消费该点。
- 客户端只观察既有移动、ASC 和 View 复制；Brain、Utility、知识与查询预算只在服务器运行。

| 资产 | 组合与作用 |
| --- | --- |
| `ST_AI_Tactical` | `WaitAssignment → HardSelect(Blocked / Retry / Return / TacticalSelect)`；战术子层以最高 Utility 选择 Cast/Reposition/Attack/Guard |
| `DA_AI_HeroTactics` / `DA_AI_HeroAbilitySet` | v2 Hero Profile 与已授予治疗技能；规则通过 DefinitionId 匹配当前 AbilitySpec |
| `DA_AI_RangedGuard` / `EQS_AI_TargetOutside` | v2 远程守卫 Profile 与战术位置查询；站位点只用于 Reposition |
| `BP_AI_HeroUnit` / `BP_AI_RangedUnit` | 独立示例单位，不改写已有卓尔游侠和角色 Demo |
| `BP_AI_TacticsArena` / `BP_AI_TacticsGameMode` / `BP_AI_TacticsPlayerController` / `L_CombatAI_Tactics` | 演示生成、宽视野入口、联机 smoke 与地图 |

技能规则只允许 `Self`、`CurrentEnemy` 和 `CurrentEnemyLocation` 三种目标策略，并必须匹配 Ability 的真实目标模式。`TacticalLocation` 枚举值为旧序列化兼容保留，但在编辑器隐藏且配置校验拒绝；战术 EQS 只服务 Reposition。冷却、资源、沉默、满血治疗、目标生命和库存修订仍由 `PreflightAIOrder`/ASC/Order 复核，AI 不缓存第二套真值。

每只单位最多一个战术查询。工作区同时核对 QueryId、QueryGeneration、运行/控制/自身生命、Scope、职责修订、查询模板和目标生命；退出、换 Profile、接管、死亡与 EndPlay 先使身份失效，再 Abort 查询并 exactly-once 结算 World token。预算拒绝只通过 Combat Scheduler 延后下一次尝试，不补发过期工作；v1 Profile 不进入这条预算路径。

首次创建/重建和验证：

```text
UnrealEditor <项目> -run=CombatAIAssets -Tactics -Unattended -NullRHI
UnrealEditor <项目> -run=pythonscript -script="<仓库>/Tools/setup_ai_tactics.py" -Unattended -NullRHI
UnrealEditor <项目> -run=CombatNavigationBuild -Map=/Game/Combat/Demo/AI/Tactics/L_CombatAI_Tactics -Unattended -NullRHI
UnrealEditor <项目> -ExecCmds="Automation RunTests Combat.AI.Tactics;Quit" -TestExit="Automation Test Queue Empty"
Tools/RunDedicated.ps1 -InstalledEditor -AITactics -TimeoutSeconds 120
UnrealEditor <项目> -run=Cook -TargetPlatform=Windows -Map=/Game/Combat/Demo/AI/Tactics/L_CombatAI_Tactics -Unattended -NullRHI
```

AI-004 最终证据为战术 21/21、AI 42/42、完整 Combat 143/143、42 资产 0 error/0 warning、冷回读 1/1、Tactics PIE 1/1、Dedicated 双客户端和 Windows cook 通过。容量档位如下，格式均为 Requests/Granted/Deferred：

| 单位数 | 感知 | EQS | 峰值在途 | EQS P95/P99 | 命令数 |
| ---: | --- | --- | ---: | ---: | ---: |
| 64 | 92/92/0 | 78/64/14 | 6 | 50/50 ms | 64 |
| 128 | 281/281/0 | 304/128/176 | 7 | 50/50 ms | 128 |
| 256 | 1246/957/289 | 1361/256/1105 | 6 | 50/50 ms | 256 |

这些是当前自动化场景的准入证据，不是所有地图的性能承诺。AI-004 已于 2026-09-22 通过用户验收；人工视觉/手感、网络损伤、长时间 soak 和打包可执行文件启动仍未执行。
