# CTRL-001 英雄查看与多单位操控

> Spec 版本：`0.2`
> 状态：`COMPLETED`
> Owner：Codex
> 创建日期：2026-09-24
> 关联进度台账：[00-01](../00-Project/00-01-Progress-Tracker.md)
> 风险等级：`L1`

## 0. Intake 与 Gate 记录

- 用户请求：参考 DOTA2，点击其他英雄在本地 HUD 查看信息，有控制权的英雄可以操控；用户进一步明确包含点击查看、切换操控、Shift 多选/框选、群体移动攻击。
- 验收与提交授权：2026-09-24 用户明确“验收完毕，提交吧”，确认 CTRL-001 0.2 验收完成并授权创建本地 Git 提交；不推送远端。
- 附件解释：无附件；DOTA2 是交互参考，不引入外部代码或资源。
- 已读取入口：`agent.md`、`README.md`、`00-01`、`00-03`、`00-04` ADR-047/066、`00-05`、DDD `10-01`、`10-09`、`10-10`、`10-12`、`90-16`、`20-02`、`20-03`，`Skills/combat-task-router/SKILL.md`、`Skills/combat-feature-development/SKILL.md`。
- 主 Skill：`combat-feature-development`。
- 备选 Skill 与排除理由：`combat-skill-development` 不适用，本次不新增技能结算。
- 路由置信度：`high`
- F0 结论：`GO`
- F0 依据：用户明确授权交互及多操功能；复用现有 Unit Owner、Order、Targeting、HUD View 和相机，保持服务器导航。
- F1 结论：`APPROVED`
- F1 审查人：Codex（用户已确认范围内的本地计划审查）
- F1 审查版本：`0.2`
- F1 计划审查证据：0.2 preflight/plan 均 0 error；本地审查 AC-07/08 与原权限边界，修正仅限用户反馈和可复现根因。相机初始化不依赖网络临时空目标；单位硬碰撞与服务器 Crowd 保留，先观察到达全过程再修复。三 Target、碰撞/相机回归与 Dedicated 同点到达为准入，既有资产不改，无需新增用户决定。0.1 审查与证据保留在历史交付记录。
- Build 解锁：`已解锁`；第一次 0.2 行为修改前运行 build Gate。
- F1 重审条件：范围、架构、权限、迁移、测试矩阵或回滚实质变化时递增版本并重审。
- F2 结论：`PASS`
- F2 审查证据：0.2 按最终 diff 核对相机初始化、复制空窗、地面判定与 Motion/权限边界；实际碰撞 Red/Green、全量回归与最终 PIE/Dedicated 通过，详见第 9 节。
- Push-Ready 结论：`READY`
- 验证：0.2 Editor/Server/Client 构建、全量 Automation 153 项、真实 Demo PIE 和 Dedicated 双客户端三段到达/折返/静置均通过。0.1 资产和容量证据保留于第 9 节。
- 未执行：Codex 未执行物理鼠标/真实屏幕框选及人工视觉检查、打包 exe、长 soak 和网络损伤；自动化/PIE/Dedicated 使用合成输入。用户已于 2026-09-24 确认验收完成，具体手动步骤未另行记录。

## 1. 目标与范围

### 0.2 用户反馈修正（2026-09-24）

- 原始反馈：框选不应触发一次视角锁定；两单位移动到同一点时存在被挤开弹飞。
- 用户进一步确认：弹飞指单位被顶到空中再落下，重点回归单位接触不得产生向上速度。
- 开工记录：重新读取 agent、路由/功能 Skill、README、台账、DDD 10-01/10-09/10-10、10-16、测试与流程约束；主 Skill 与 high/F0 GO 不变。当前没有 CodeGraph 索引。UE MCP 确认用户 Editor 无 PIE、无打开资产；不修改或关闭该实例。
- 代码依据：`ACombatCharacter::SetFollowTarget` 在每次目标变化时居中，选择确认暂时返回空目标也会触发再次居中；`ProcessGroupOrderRequest` 为所有单位发送完全相同的移动终点。服务器仍使用 UE 默认 Pawn 解穿透/地面判定，需以实际碰撞复现区分拥挤挤出与落在角色上的弹跳，不能仅凭客户端表现调参数。
- 修正边界：Command Pawn 仅在首次有效目标就绪时定位，之后选择、框选、只读切回及复制重绑保持锚点；Space 主动跟随、首次定位和新 Pawn 初始化保持有效。群体移动继续保留硬碰撞、单一 Crowd、服务器公共 Order；按实测根因修正地面/解穿透或同终点拥挤，不引入客户端位移、不关闭单位硬碰撞、不修改技能 Motion。
- 实施与测试落点：CombatCharacter 与 Camera/Selection 测试；Selection 群体终点与 CombatCharacterMovementComponent/UnitAIController（仅对复现确定的碰撞根因修改）；新增实际移动碰撞回归，扩展 SelectionNetworkScenario 等待到达并静置、记录高度/位移/到达结果。必要的间隔终点由服务器计算、走现有导航与失败回执，不接收客户端权限或新增 RPC 字段。
- 验证矩阵：先运行新增回归取得 Red，再最小修复；Editor、完整 Combat Automation、源码 Server/Client 与 Dedicated 双客户端同点到达/静置；相机覆盖初始、框选换主选、只读返回、确认空窗、Space。沿用已验证资产，当前不计划二进制迁移；若实际资产覆盖导致必须迁移，先重审。
- 回滚：只撤销本轮相机/碰撞及测试增量，保留已有多操实现和用户工作区；不改发布版本/展示 schema/权限。风险为坡道台阶或 Motion 回归、群体终点投影与网络延迟；要求相关旧测试一起通过。物理鼠标与手感仍由用户验收。
- 历史证据：第 7/9/11 节现有结果为 0.1 已执行记录，不作为本轮修正已通过的证明。本轮结果追加记录。
- Red 证据：`Automation-Feedback-Red.log` 的相机 3 个位置断言失败；`Automation-Collision-Red.log` 的实际双胶囊接触向上速度为 193.67 cm/s，单位顶部被 IsWalkable 当成地面、BaseChange→JumpOff 注入速度。修正同步 World 的 GFrameCounter 夹具后，平地同侧/对向汇聚 Rise=0、Step=5，本轮不再扩展群体队形或修改解穿透参数。最终修复限定为首个镜头锚点标记、CMC 对 CombatUnit 拒绝地面判定。UE MCP 回读实际 Drow 胶囊 ECB_No、34/88 cm，证明仅禁止 StepUp 不能阻止落地弹跳。

### 目标

将本地观察、选中组、主选操作对象与服务器授予的控制权分离，支持多个英雄持续执行各自命令。

### 范围

- 世界左键查看单位；有控制权时单选，Shift 点击增删组成员，拖动框选自己的单位，Shift 框选追加。
- 群体右键移动/攻击、A 左键指定攻击、S 停止；每组最多 8 个单位，首个为主选。技能/物品/购买只作用于主选英雄。
- 公共查看快照展示英雄等级、属性、资源、技能定义/等级和公开物品；技能句柄、物品身份/修订、经验/技能点、冷却及 AutoCast 状态仍只给拥有者。
- 切换主选不取消旧命令、不释放旧单位 Owner；显式授权/撤销与本地选择分离。
- Demo 可配置额外初始英雄，默认配置保持旧关卡行为；Demo 蓝图配置一个额外英雄以便体验。

### Non-Goals

不实现控制编队快捷键、同队自动授权、多玩家共享同一单位、召唤/幻象生成、攻击移动、战争迷雾、群体施法或客户端移动预测。

## 2. 当前事实与依据

- 相关 DDD：`10-09` 的 Owner/RPC 链、`10-10` 的 AI Possess 拓扑、`10-12` 的只读 HUD、ADR-066 玩家资源与英雄库存分域。
- 代码事实：`Source/Combat/CombatPlayerController.cpp:61` 的旧绑定会取消并撤销旧英雄；`:511` 普通左键当前无动作；`:577` 单单位 Order RPC；`Combat/UI/CombatHUDWidget.cpp:177` HUD 固定观察 CommandedUnit；`Combat/View/CombatHUDView.cpp:84` 只构造拥有者快照。
- 0.1 开工基线：当时仅有 `Content/Combat/Demo/AI/ST_CombatAI_Root.uasset` 用户修改，已保留；历史验证不是本轮反馈修正通过证据。
- 0.1 环境记录：没有 `.codegraph/`，使用 rg/源码；当时未直接暴露 UE MCP 工具，通过本地 HTTP endpoint 完成 discovery，读取打开的 `BP_AI_RoleArena`，确认三个目标资产均无未保存修改。独立保存遭遇 Win32 错误 32 后，通过 MCP Slate 控制台的 `fully_load_packages` 释放三个干净目标包文件锁，再完成资产编译、保存、冷回读，未关闭用户 Editor。0.2 已可直接调用 UE MCP，本轮只读回实际状态与胶囊配置，不保存资产。

## 3. 行为与契约

### 主流程

1. 左键在 UI 外且无瞄准会话时开始选择手势，释放时区分点击与框选；技能/攻击确认保持优先，不能同时改变选择。
2. 点击敌方/无权单位进入本地只读观察，清空命令选中组；右键/技能/物品不得意外作用于旧英雄。点击自己的单位恢复操控。
3. 多选只包含当前 Owner 为本 Controller 的单位。空白点击/空框保留现有选择；Shift 点击切换成员。框选结果排序稳定并去重、有界。
4. 主选改变经 Controller 可靠 RPC 请求服务器更新 CommandedUnit，服务器再次复核 Owner；确认前技能/物品入口关闭。记录最近发出的主选意图，确保 A→B→只读目标→A 在旧复制尚未到达时仍发送最后的 A 请求。Camera 刷新跟随目标但保持已初始化锚点；只有本 Pawn 首次有效绑定自动定位，Space 主动跟随新主选。
5. 群体命令通过一次有界 Controller RPC 发送，连接限频/重放只消费一次；服务器校验全组所有权、生命代次、唯一性、类型/载荷，随后逐个进入现有 OrderComponent 和 Targeting。组内业务失败可独立返回，不承诺全部执行成功。

### 状态转换

无选择 → 初始主控单选；单选 ↔ 多选；有权选择 → 无权只读观察；失焦/取消 → 放弃框选与旧瞄准；被撤权/销毁成员 → 移除且重选有效主选。查看和选择不发 Stop。

### 输入、输出与数据约束

沿用 Enhanced Input 的确认 Action，补充 Triggered/Completed/Canceled；Shift 使用新增可配置 Boolean Action 与映射。选择框和单位标记由本地 HUD 绘制，不写游戏状态。公共查看投影复用只读结构但清除全部操作身份与私人字段；展示 schema 8 → 9，联机两端必须同版本。

### 权威边界与权限

Unit Owner 是唯一控制许可，Team 不授予控制。新增服务器授权/撤销入口不允许客户端调用；选择 RPC 无法取得控制权。原单单位绑定入口保留其显式转移契约；新主选切换入口只选已有权限单位。群体命令不绕过正 RequestId、8 单位上限、估算载荷、共享限频/重放、生命代次、公共 Order 与目标校验。

### 失败、取消、过期、死亡、EndPlay 与重复请求

- 无权、重复成员、超载荷、过期生命、非法动作/坐标、重放和过频请求拒绝，拒绝发生在执行前。
- 死亡可继续查看；群体选取只发送可操作成员，服务器最终复核生命。复活仍沿原 LifeGeneration。
- 失焦、UI 捕获、右键、Escape 和瞄准开始取消旧选择手势；过期 MouseUp 不得重新选中。
- Controller EndPlay 撤销其全部控制单位、取消对应 Order，清资源锚点；Unit EndPlay 清主控，本地弱选择每帧修剪。
- 换查看目标清 HUD 详情/拖放和异步绑定；本地交互必须匹配被查看单位、当前主选与 Owner，避免相同定义/生命代次碰撞。

### 兼容、版本与迁移

保留旧蓝图入口；新增公开 API、schema 9 与信息公开范围先登记 ADR-067。不改战斗公式、GameplayTag/Event、物品事务或资源归属。旧 GameMode 额外英雄数为 0；仅 Demo 设置 1。移除新增配置和代码可恢复旧行为，保存前后回读资产，不手工改二进制。

## 4. 实施计划

| 文件/资产 | 变更 | 原因 | blast radius |
| --- | --- | --- | --- |
| CombatPlayerController、Input/Selection 新文件 | 本地选择、权限 API、主选 RPC、群体请求 | 分离查看与控制 | 输入/相机/生命周期 |
| NetworkTypes/Security、UnitCharacter | 有界群体安全层与共享 Order 执行 | 保持权威和每连接预算 | 联网/回执 |
| View、HUDWidget/Slot/Item、PlayerHUD | 公开只读快照、操作门禁、选择框/标记 | 正确查看其他英雄 | 展示与交互 |
| CombatGameMode、Demo Controller/IMC/GameMode | 额外英雄和 Shift Action | 可玩的多操演示 | 默认安全的资产配置 |
| Tests、测试网络场景、验证脚本 | 输入/权限/查看/联机回归 | 验证完整用户路径 | 测试基础设施 |
| README、10-09/10-10/10-12、00-01/00-04 | 当前行为、ADR 与证据 | 文档同改 | 文档 |

## 5. 验收标准（AC）

- [x] AC-01：普通左键查看其他英雄并正确显示公开信息，不泄漏操作身份/私人字段，不发旧英雄技能/物品/命令。
- [x] AC-02：同一玩家可同时拥有两名英雄，点击切换保持旧英雄正在执行的 Order 与 Owner。
- [x] AC-03：Shift 点击增删与框选/追加稳定、去重且有界，UI/瞄准/失焦不误选；有可见选中标记与框。
- [x] AC-04：移动、右键/A 攻击、S 停止覆盖全组；技能/物品只给主选；与相机和 HUD 旧操作兼容。
- [x] AC-05：伪造权限、旧生命、重复/超限/非法组命令、限频/重放被服务器拒绝；撤权、死亡/复活和 EndPlay 无残留。
- [x] AC-06：真实 Demo 输入资产可回读，Editor/Server/Client、相关及全量 Automation、资产验证、独立 Dedicated 双客户端证据完整。
- [x] AC-07：框选/切主选与确认空窗均不改变已初始化镜头锚点；首次定位和 Space 主动跟随有效。
- [x] AC-08：两单位从同侧和交叉方向移动到同一点，真实碰撞/导航与复制路径均无非预期腾空或弹飞；到达静置后的硬碰撞和已有 Motion 保持正确。

AC 勾选表示实现与工程检查完成；AC-03 的物理鼠标、选择框/圈视觉与操作手感仍待用户实机验收，不自动标为已验收。

## 6. Definition of Done

- [x] 最小失败测试/Golden Case 记录实际 Red，再完成实现与 Green。
- [x] 通过直接与全量回归，无 gameplay 旁路，F2 已完成。
- [x] 资产编译保存并冷回读；对应 UE 与网络验证完成。
- [x] 文档、中文公开说明、ADR、台账、delivery Gate 和 git diff --check 同步。

## 7. 测试矩阵与命令

以下为 0.1 已执行矩阵。0.2 新增相机锚点与真实碰撞回归、全量 153 项及三段到达场景，当前结果见第 9 节的 0.2 证据；不把历史资产与容量检查写成本轮重跑。

| 层级 | 用例/命令 | 预期证据 | 结果 |
| --- | --- | --- | --- |
| 文档/本地工具 | task_gate preflight/plan/build/delivery、validate_docs、git diff --check | Saved/MultiControl | preflight / plan / build / delivery 全通过；文档 89 文件、480 链接，0 错误 |
| Pure/Unit | 选择框阈值/取消、增删/过滤、组请求限制、快照脱敏 | Combat.Input.Selection / Network / UI.HUD.PublicInspection | 新增 5 项通过 |
| World Automation | 多 Owner、切主选保留命令、HUD 只读、权限/过期/teardown、全量 Combat. | Automation-Delivery/index.json | 145 success + 5 success with warnings，0 failed / 0 not run |
| PIE / Blueprint | Demo 资产编译保存冷回读、输入绑定与双英雄 | AssetReadback.json、PIEReport.json | 通过；真实 UEDPIE_0_L_CombatDemo、合成输入，不覆盖物理鼠标/Slate 几何 |
| Network / Dedicated | 三 Target、独立 server + 两 client，拥有双英雄/查看对方/批量操作/拒绝越权 | Dedicated-Selection-Final.log | 三 Target 通过；三端 Pass；4 个受控英雄，2 次所有权拒绝 |
| Soak / Perf | 原有容量回归；本次无长 soak 或网络损伤新增要求 | Dedicated-Capacity.log | 64 Unit / 256 Modifier，预算与 SAM 位移、HUD 三端均 Pass；不等于长 soak |

## 8. 风险、回滚与升级

- 风险：跨 Actor Owner/主选复制乱序；公开投影误携带操作句柄；群体 RPC 放大；UI 焦点冲刷。
- 回滚方式：按本任务文本 diff 撤销；资产保存前保留原始版本，仅还原本任务相关资产，不触碰用户 StateTree 修改。
- 触发升级的条件：需要共享同一单位、完整迷雾、释放版本契约、改变资源归属或未收敛验证。
- 需要人决定的问题：无；具体操作范围已获用户确认，人工视觉验收保留到实现交付。

## 9. 交付证据

报告位于 `Saved/MultiControl/`；生成日志不提交。以下至 Push-Ready 表为 **0.1 历史交付**，0.2 当前结果见本节末。

- 代码/资产：本地选择、权限 API、主选 RPC、共享安全预算的群体请求、公开 HUD 白名单与只读交互已实现；Demo 的 `BP_CombatDemoPlayerController`、`BP_CombatDemoGameMode`、`IMC_Default` 及新增 `IA_AddToSelection` 完成配置。19 个键位包含原有 17 个映射及左右 Shift；默认额外英雄数 1，原生 GameMode 默认仍为 0。
- 用户状态：原有 `ST_CombatAI_Root.uasset` SHA256 始终为 `510ACA8117F0B9F96B9BF30A7BCC2F19F44C5D86A73C7BFB4071742AA1031C5E`；未关闭用户 Editor，未提交或推送。
- 构建：安装版 Editor 使用 `Build.bat ue_gasEditor Win64 Development <project> -WaitMutex -NoHotReloadFromIDE -ModuleWithSuffix=Combat,92401`，`Build-Delivery.log` 成功；用户运行中的 Live Coding 阻止常规模块覆盖，因此独立验证进程加载新后缀模块。源码 `Build.bat ue_gasServer/ue_gasClient Win64 Development <project> -WaitMutex` 已通过 Server 和 Client（`Build-Server-Final.log`、`Build-Client.log`）。Server 最终增量还纳入快速主选修正；Client 构建后仅调整了测试报告的 Standalone 位移文案与注释，未改客户端玩法。源码构建命令临时指定工作集为项目仓库以避免扫描整个引擎的 git status 超时，不修改持久用户配置。
- TDD：`Automation-Red.log` 实际失败于缺少选择 API；随后修正输入绑定计数断言与本地 Player/UMG 测试夹具。`Automation-Delivery/index.json` 最终 150 项全部完成（145 success、5 success with warnings、0 failed）。命令为 `-ExecCmds="Automation RunTests Combat.;Quit" -TestExit="Automation Test Queue Empty" -ReportExportPath=<report>`。
- 资产：独立 Editor `-run=pythonscript -script=Tools/setup_selection_input.py` 编译两个蓝图并仅保存四个目标资产；新进程加 `-SelectionReadOnly` 冷回读 `configured=true`。`-run=CombatAssetValidation` 得到 42 Assets、0 Errors、0 Warnings，见 `AssetMigration-Final.log`、`AssetReadback.json`、`AssetValidation.log`。
- PIE：独立 Editor `-ExecutePythonScript=Saved/MultiControl/RunPIE.py -NullRHI` 在真实 `UEDPIE_0_L_CombatDemo` 用暂存测试 Actor 检查双英雄、公开查看、切主选、群体移动/攻击/停止并正常 EndPIE，未保存关卡。`PIEReport.json` 为 Pass。Standalone 的位移是本进程服务器权威结果，复制证据来自下项 Dedicated。
- 网络：`Tools/RunDedicated.ps1 -Selection -InstalledEditor -Port 7893 -TimeoutSeconds 150` 启动独立 dedicated server 与两个 game client；三端 `Result=Pass`，4 个受控英雄、2 次伪造 Owner 请求被拒绝，客户端核对 SimulatedProxy、复制位移、两个单位的 Move/Attack/Stop 回执和只读公开字段。日志为 `Dedicated-Selection-Final.log` 及 `Saved/UEEnvironment/Dedicated-Installed-Selection/`。首轮 NullRHI 不推进 UMG NativeTick 导致 HUD 阶段等待；最终夹具显式绑定实际观察目标来校验 HUD 数据和操作权限，不能把它当物理界面输入证据。
- 原回归：`Tools/RunDedicated.ps1 -InstalledEditor -Port 7894 -TimeoutSeconds 150` 的 `Dedicated-Capacity.log` 记录 64 Unit / 256 Modifier、`CapacityBudget=Pass`、`M7Performance Budget=Pass`、SAM 位移与三个 schema 9 HUD 快照 Pass；本次采样 MaxOutKiBps=22.923。
- 日志限制：Dedicated 中仍有已有的动态 GameplayEffect 缺定义诊断；2026-09-13 的旧 `DedicatedClient1` 日志已有相同报错，本次不宣称引擎日志零错误，也未修改该既有 GAS 问题。
- F2 审查：按本轮 diff 独立核对服务器入口、公开字段、跨 Actor 复制、撤权/销毁、拖放、共享重放窗口与组成员执行时复核。发现快速 B→只读→A 可能丢失最后主选请求，已保留 `LastRequestedPrimary` 并增加回归；另修正转移非主选单位不得清空旧玩家其他主选。5 项直接测试及最终全量回归通过，无未关闭高风险发现。
- 未执行：物理鼠标点击/屏幕框选的完整自动交互、人工视觉验收、打包 exe、网络损伤和长 soak。现有用户 Editor 仍加载旧模块，需重启后体验新配置。
- 剩余风险：主选确认受网络往返影响；最多 8 单位，群体技能/物品与共享控制不在范围。视觉和手感待用户验收。

- 交付 Gate：`python -B Tools/task_gate.py --mode delivery --spec Doc/CombatSystem/Specs/CTRL-001-multi-unit-selection.spec.md --kind feature` 通过（0 error，40 changed files，含保留的用户 StateTree 差异）；`python -B Tools/validate_docs.py` 通过（89 Markdown、480 local links、0 error），`git diff --check` 通过。

### Push-Ready 六层

| 层级 | 结论 | 证据 |
| --- | --- | --- |
| Tests | PASS | 最终 Automation 150 项、42 资产、PIE 与 Dedicated |
| Types/Build | PASS | Editor / Server / Client Development 构建 |
| No Regression | PASS | 原全量与 64/256 容量、SAM / HUD 回归 |
| Adversarial | PASS | F2 发现已修正；权限、重放、生命周期、主选时序回归 |
| DDD/Constraints | PASS | 服务器 Order、ASC 唯一来源、Owner / 观察分离、公开投影白名单 |
| Decisions | PASS | ADR-067、schema 9、兼容 API、Demo 迁移和回滚记录 |

### 0.2 反馈修正证据

- 修改文件：`CombatCharacter.cpp/.h` 的镜头初始化与绑定语义；`CombatCharacterMovementComponent.cpp/.h` 的单位地面判定；`CombatCameraTests.cpp`、新增 `CombatUnitCollisionTests.cpp` 与 `CombatSelectionNetworkScenario.cpp/.h`；同步 README、10-16、10-10、ADR-067、台账和 Spec。本轮不改任何资产、群体 RPC 或移动终点。
- TDD：相机测试先复现三个跳镜断言失败。碰撞夹具在平地实际运行 CMC，双胶囊顶部接触产生 `MaxUpwardSpeed=193.67`；修复后同例 `0.00`，自然落回地面。实际同侧/对向汇聚最高抬升 0、每帧位移 5 cm、最终胶囊间距 68.17 cm，保持硬阻挡。初版同步夹具未推进组件 Tick 被到达断言拦住，已参照现有 Facing 夹具显式推进 CMC，未把静止样本算通过。
- 自动化：`Automation-Feedback-Green.log`、`Feedback-Green/index.json`，`-ExecCmds="Automation RunTests Combat.;Quit" -TestExit="Automation Test Queue Empty"`，148 success + 5 success with warnings、0 failed / 0 not run（153 项）。包括相机 6 项、SAM 接触与汇聚、原 Motion/状态/权限/生命周期。此后仅扩展联机场景的编排与诊断，无运行时实现改动。
- 场景审查：0.1 只检查移动 100 cm 后立即攻击/停止，未覆盖到达。扩展为同点到达→静置 3 秒→沿已走通路径折返→再汇聚，每段检查静置。首轮 Demo 的正常爬坡高差 191.63 cm 被绝对高度断言误判；日志明确导航终点地面从 Z=10 上升到 Z=189.69，故网络/PIE 改测 Falling 时的向上速度，平地 World 仍保留严格高度断言。首轮任意负向偏移落在第二客户端不可导航区，服务端正确 PathFailed；折返改用真实初始可达点，没有放宽寻路或生产代码。
- 构建：安装版 `Build.bat ue_gasEditor Win64 Development <project> -WaitMutex -NoHotReloadFromIDE -ModuleWithSuffix=Combat,92402` 成功，见 `Build-Feedback-Final.log`。源码 `Build.bat ue_gasServer/ue_gasClient Win64 Development <project> -WaitMutex` 均成功，见 `Build-Feedback-Server.log`（210.45 s）、`Build-Feedback-Client.log`（330.30 s）；临时 Git 工作集范围仍指定项目目录。三 Target 都包含最终场景代码。用户 Editor 保持加载原 `UnrealEditor-Combat.dll`，独立验证使用 `UnrealEditor-Combat-92402.dll`；重启用户 Editor 后才体验本轮修正。
- PIE：`RunPIEFeedback.py` 通过独立 Editor 在实际 `UEDPIE_0_L_CombatDemo` 执行合成选择与三段移动；`PIEFeedbackReport.json` passed=true，Camera=Preserved、ArrivalLegs=3、每段静置 3 秒、AirborneUpSpeed=0。正常坡道产生 192.94 cm 高差，未产生腾空冲量；正常 EndPIE，未保存地图，详见 `PIE-Feedback.log`。
- Dedicated：`Tools/RunDedicated.ps1 -Selection -InstalledEditor -Port 7893 -TimeoutSeconds 150` 最终三端 Pass，见 `Dedicated-Feedback-Final.log` 与 `Saved/UEEnvironment/Dedicated-Installed-Selection/`。两个客户端都完成 3 段真实复制移动和静置、镜头锚点保持，服务器 4 个英雄、2 次越权拒绝、AirborneUpSpeed=0、最大单帧额外平移 1.00 cm。进程模式为安装版 Editor 的 -server/-game，不冒充打包 Server/Client exe。
- F2：从最终变更核对 IsWalkable 只排除 CombatUnit 地面，不改其他地形、平台、碰撞响应、服务器移动入口和技能 Motion；初始相机标记不因 Owner/主选复制空窗重置，首次定位/Space/失焦/旧释放保持受测。扩展场景检查到达全过程并区分正常坡道；没有未关闭的代码审查发现。修正限于本次实现与测试，不修改通用 Skill。
- 工作区：本轮未修改二进制资产。用户的 StateTree 在本轮开工与交付检查中均无 Git 差异；本轮回读 SHA256 为 `6F9D2A6A661FD5B30E33F2A854AF635894BA7C880B657F3DA3E33EF91D34689D`，与 0.1 历史值分开记录。未关闭用户 Editor，未提交或推送。
- 未执行：本轮没有资产修改，未重复 42 项资产校验；未重复 64/256 容量、长 soak、网络损伤、打包 exe；物理鼠标与人工视觉仍待用户复验。使用独立验证进程，保留用户 Editor，不提交或推送。

0.2 Push-Ready：Tests PASS（153 项及 PIE/Dedicated）、Types/Build PASS（三 Target）、No Regression PASS（原 Camera/SAM/Motion/权限/生命周期）、Adversarial PASS（上文 F2）、DDD/Constraints PASS（本地相机、服务器移动与硬碰撞不变）、Decisions PASS（ADR-067 与 10-16/10-10 同步，无迁移）。最终 `python -B Tools/task_gate.py --mode delivery --spec Doc/CombatSystem/Specs/CTRL-001-multi-unit-selection.spec.md --kind feature` 通过（0 error、46 changed files，包含 0.1 尚未提交的多操工作区）；`python -B Tools/validate_docs.py` 通过（89 Markdown、481 local links、0 error），`git diff --check` 通过。

### 0.2 用户验收与提交（2026-09-24）

- 用户确认“验收完毕，提交吧”，本任务状态转为 COMPLETED；验收包含英雄查看与多操，以及框选保持镜头和单位接触防顶飞修正。
- 提交范围为本 Spec 的源码、测试、工具、文档和四个 Demo 输入/框架资产，共 46 个文件；生成日志、构建产物不提交，用户 StateTree 资产无差异。
- 本轮仅同步验收状态与提交记录，不修改运行时实现或资产，不重复执行 UE 构建、Automation、PIE、Dedicated；保留上述已运行证据及合成输入的覆盖边界，不新增运行验证结论。
- 提交前验证：`python -B Tools/validate_docs.py` 通过（89 Markdown、481 local links、0 error）；`python -B Tools/task_gate.py --mode delivery --spec Doc/CombatSystem/Specs/CTRL-001-multi-unit-selection.spec.md --kind feature` 通过（0 error、46 changed files）；`git diff --check` 通过，四个二进制资产均由 Git LFS 管理。与验收后快照相比仅本轮三份验收文档发生变化，运行时实现、测试及资产保持已验收版本。

## 10. 变更记录

| 版本 | 日期 | 修改 | 原因 |
| --- | --- | --- | --- |
| 0.1 | 2026-09-24 | 初稿 | 用户要求并确认多选/框选与群体操作 |
| 0.2 | 2026-09-24 | 重新 PLAN；相机锚点与同点移动碰撞修正 | 用户实机反馈跳镜和单位弹飞，补全到达阶段证据 |
| 0.2 | 2026-09-24 | 用户验收完成，同步状态并授权本地提交 | 用户明确“验收完毕，提交吧”；实现范围不变 |

## 11. 路由、自评与 Reflect

### 路由记录

- 原始需求摘要：DOTA2 风格查看与多英雄操控。
- 主 Skill：`combat-feature-development`。
- 选择依据：涉及输入、网络所有权、HUD 与生命周期，非新技能。
- 备选 Skill 与排除理由：技能开发不适用。
- 路由置信度：`high`

### 交付自评

以下保留 0.2 工程交付时的评分和证据边界；0.1 历史自评为 4.7。用户后续确认验收完成，不将该确认改写为 Codex 执行了物理输入测试。

| 维度 | 权重 | 分数（0–5） | 证据与扣分原因 |
| --- | ---: | ---: | --- |
| 需求与 AC | 20% | 4.5 | 两项用户反馈均有修复与回归；物理框选和手感仍需实机复核 |
| 架构与权限 | 20% | 5 | 相机只做本地表现；保留服务器 CMC/Crowd/硬碰撞与既有 Order 权限 |
| 实现与数据 | 20% | 5 | 镜头只首次定位；单位不充当地面；无需数据、资产或网络 schema 迁移 |
| 验证证据 | 20% | 4.5 | 153 Automation、实际 Demo 坡道 PIE 和 Dedicated 三段移动通过；无物理鼠标/人工视觉证据 |
| 文档与可观测性 | 10% | 5 | ADR-067、10-16/10-10 与 Red/Green、三端高度/腾空速度/静置日志 |
| 交付卫生 | 10% | 5 | 保留本轮既有工作区和用户 Editor，生成文件未入 diff；无提交/推送 |

- 计算总分：4.8；工程验证完成，可供用户复核，评分不替代人工验收。
- 硬性封顶或未执行项：物理输入、人工视觉、打包与长 soak 未执行；不把合成输入检查视为人工验收。
- 自评结论：`READY_FOR_REVIEW`
- 用户验收状态：`用户已验收`（2026-09-24）；用户明确要求创建本地提交，不推送远端。

### Reflect 与调优

- 观察与证据：0.1 的绑定重用引入跳镜；移动 100 cm 后就结束的测试遗漏到达拥挤阶段。胶囊顶部接触实际产生 193.67 cm/s 自动跳离，修复后为 0。
- 根因类别：本次实现遗漏与场景覆盖不足；不扩大到通用 Skill 变更。
- 调整文件与预期收益：相机和移动组件最小修正，增加稳定接触回归与完整到达/静置/折返；地形爬坡与腾空分别测量。
- 回归验证：最终全量 153 项、实际 Demo PIE 与 Dedicated 三端通过；三 Target 与交付 Gate 见第 9 节。
- 需要用户决定的问题：无新增。
