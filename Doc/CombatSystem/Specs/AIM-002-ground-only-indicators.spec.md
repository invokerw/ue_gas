# AIM-002 技能指示器仅投射地面

> Spec 版本：`0.1`
> 状态：`已验收`
> Owner：Codex
> 创建日期：2026-09-13
> 关联进度台账：[00-01](../00-Project/00-01-Progress-Tracker.md)
> 风险等级：`L1`

## 0. Intake 与 Gate 记录

- 用户请求：修复 AIM-001 指示器投到 Hero/场景物体，以及点目标命中 Hero 后落点被抬高的问题。
- 附件解释：无新增附件；上一轮只读代码、Unreal 蓝图回读和 PIE-Circle.png 是缺陷证据，DOTA2 是交互参考。
- 已读取入口：agent.md、README、00-01、00-03、00-04、00-05、10-01、10-09、10-13、AIM-001、Skills/combat-task-router/SKILL.md、Skills/combat-feature-development/SKILL.md。
- 主 Skill：combat-feature-development。
- 备选 Skill 与排除理由：combat-skill-development；本轮修正通用输入/表现设施，不新增技能或结算规则。
- 路由置信度：high
- F0 结论：`GO`
- F0 依据：用户明确授权修复；已有 AIM-001 未提交差异全部保留，无 CodeGraph 索引。当前无可调用 UE MCP，使用 UE 5.8.2 Python/命令行和原生 PIE 降级。现有用户 Editor 进程不强制关闭或丢弃其未保存内容。
- F1 结论：`APPROVED`
- F1 审查人：Codex，按 00-05 §4 在用户既有 L1 修复授权内审查。
- F1 审查版本：0.1
- F1 计划审查证据：Saved/SkillIndicatorGround/PreflightGate.json 与 PlanGate.json 均通过。审查确认专用查询与 stencil 位当前未占用；深度比较解决被遮挡的地面 mask 投到角色的问题；法线检查排除已标记平台的侧面；原单位/攻击 Visibility 和服务器复核保持原入口。资产迁移限地面，普通单位贴花不改；Editor/直接测试/真实 PIE 覆盖接收像素、两类查询及取消/确认回归。仅有本 Spec 和 Gate 报告新增，行为文件尚未修改。
- Build 解锁：Saved/SkillIndicatorGround/BuildGate.json 已通过；行为文件修改在 F1 和 Build Gate 之后开始。
- F1 重审条件：接收策略、通道/stencil 分配、资产迁移或测试范围实质变化时升级 Spec 并重审。
- F2 结论：`PASS`
- Push-Ready 结论：`READY`
- 验证：Editor 构建、Combat.Input. 7/7、资产 17/17、独立进程三地图 114 个地面接收组件回读、原生 PIE 41/41（含真实 GPU 像素对照）通过；文档 68 Markdown / 356 local links、diff 检查和 delivery Gate 均通过。
- 未执行：成品 cook/打包、多平台/DPI、GPU 性能基准；没有网络/权威变更，本轮 Server/Client/Dedicated 不适用，不引用 AIM-001 的历史通过代替新验证。
- 用户验收：2026-09-13，用户明确回复“验收完毕，提交吧”；本轮修复连同 AIM-001 已通过实机验收，授权本地提交，不包含远程推送。

## 1. 目标与范围

### 目标

技能圈、直线和目标标记仅显示在明确配置的地面、坡道和平台上；Hero、木桩、墙壁和未标记物体不染色。点目标在鼠标下查询真实地面高度。

### 范围

- 保留三层贴花和世界 XY SDF。使用专用地面接收标记、场景深度一致性及向上表面过滤；保留单位接收其他贴花的能力。
- 分配未使用的 GameTraceChannel5（CombatIndicatorGround，默认 Ignore）及 CustomStencil 最高位 128；项目开启 CustomDepth with Stencil。
- 显式配置 Demo/训练场/测试地图内的地面、坡道和平台：地面查询 Block，CustomDepth 开启、Stencil 位 128。非地面不迁移。
- 统一点目标的预览、标准确认和两种快施查询；单位技能维持 Visibility 精确选敌。无地面/陡面命中无目标，不回退 Hero 坐标或旧点。
- 渲染投影高度允许覆盖坡道，不以压薄投影体积充当过滤。

### Non-Goals

不改变普通移动、单位碰撞避让、攻击选敌、服务器 Targeting/Order 协议、技能数值、RPC/schema 或核心发布契约。不全局关闭角色 ReceivesDecals，不运行全场逐帧扫描。实现阶段不提交或推送；后续本地提交按用户验收后的明确授权执行。

## 2. 当前事实与依据

- 10-13 约定地面作用形状，但 AIM-001 未检查接收网格；前一轮自动化通过不能证明接收过滤正确。
- CombatAbilityIndicatorActor.cpp:33 投影半深度 180 cm、中心抬高 30 cm；材质只计算 XY，无接收过滤。
- BP_DrowRanger/BP_IndicatorHero 的 CharacterMesh0 均允许接收贴花；胶囊阻挡 Visibility。
- CombatAbilityAimComponent.cpp:208/249 直接把 Hit.Location 用于点目标预览和提交；Controller 三个确认/快施入口及 Tick 均使用 Visibility。
- Config 当前无 CustomDepth/Stencil 分配，无 GameTraceChannel5 使用；新增的是本地查询通道，不修改已有通道语义。

## 3. 行为与契约

### 主流程

技能会话选择点目标地面查询或单位 Visibility 查询 → 验证最新命中 → 生成统一 Preview → 确认时复查会话与最新命中 → 既有 Cast 请求 → 服务器原 Targeting/Order 复核。

### 状态转换

沿用 Idle/Aiming/Submitted；无地面命中进入 InvalidTarget 并清除目标形状，标准施法保留瞄准，快施失败清理会话并显示原因。

### 输入、输出与数据约束

接收组件必须显式加入地面查询与 stencil 标记；向上法线 Z >= 0.5（不超过 60 度）才是可预览表面。材质同时检查 stencil 位及 CustomDepth 与 SceneDepth 一致，防止地面标记透过 Hero 投到其身体上。下方地面被遮挡时，不绘制在遮挡者上。地图作者新增地形必须采用相同设置。

### 权威边界与权限

仅改变本地目标位置采样和视觉；服务器不信任这些查询或 stencil，继续公共距离、LOS、资源校验。点目标请求使用经地面验证的本次落点，不钳制超距位置；单位请求仍是实际命中单位。

### 失败、取消、过期、死亡、EndPlay 与重复请求

保持现有会话代次/Owner/生命/弱引用和 exactly-once 提交；查询失败输出空命中，不保留旧点。无新异步/Timer/委托/Actor 所有权。地面设置保存于资产，运行时不修改角色网格或碰撞。

### 兼容、版本与迁移

ADR 记录保留位与新地面 authoring 约定；核心和网络 schema 不变。原 Demo 与训练场地面资产通过 Unreal API 更新并回读，测试地图如有地面同样迁移。CustomStencil 低 7 位保留给其他表现；不能把 128 位分配给 Hero。材质仅此指示器使用过滤，其他贴花不受该材质影响。

## 4. 实施计划

| 文件/资产 | 变更 | 原因 | blast radius |
| --- | --- | --- | --- |
| CombatAbilityAimComponent.* / CombatPlayerController.cpp | 地面查询入口和命中验证 | 避免点目标采到角色身体 | 本地技能输入 |
| UI 地面查询辅助 / CombatAbilityIndicatorActor.cpp | 共用地面通道、接收约定与坡道投影高度 | 保持采样/呈现一致 | 本地表现 |
| Config/DefaultEngine.ini | 专用通道与 CustomDepth stencil | 接收地形白名单 | 渲染配置/额外只读查询 |
| M_CombatAbilityIndicator / 地面外部 Actor | 深度/stencil/法线过滤及标记 | 仅渲染地面 | 该材质与地面 authoring |
| CombatPlayerInputTests / CombatAbilityAimTests / PIE 场景 | Hero 命中拒绝、实际地面射线、坡面与画面对照 | 覆盖遗漏的接收对象 | 验证设施 |
| 10-13 / ADR / 台账 | 接收配置、行为、证据 | 当前文档一致 | 文档 |

## 5. 验收标准（AC）

- [x] AC-01：圈/线穿过 Hero、木桩、墙或未标记物体时，图形不出现在它们的表面，普通 ReceivesDecals 设置保留。
- [x] AC-02：点目标在 Hero 下方获取真实地面；预览和确认相同，标准/按下/松开快施使用共同入口；单位技能保持精确选敌。
- [x] AC-03：无地面、未标记物体或陡面不成为点目标，失败不重用旧点，超距地面仍可提交追近。
- [x] AC-04：平地、坡道和抬高平台的贴花跟随可见表面，深度遮挡不串到角色；配置回读证明地面 whitelist 一致。
- [x] AC-05：Editor、直接 Automation、资产校验、原生 PIE 输入回归及视觉对照通过；差异、文档与实际证据同步。

## 6. Definition of Done

- [x] 保留缺陷 Red/Golden Case，新增真实命中与接收过滤回归。
- [x] 未增加权威旁路、逐帧扫描或新生产生命周期责任。
- [x] 材质保存回读和地面资产回读通过；未关闭 Hero 其他贴花。
- [x] Editor / 直接 Automation / PIE / 资产校验完成。
- [x] F2 / 文档校验 / diff 检查 / delivery Gate 通过。

## 7. 测试矩阵与命令

| 层级 | 用例/命令 | 预期证据 | 结果 |
| --- | --- | --- | --- |
| 流程/文档 | task_gate.py preflight/plan/build/delivery、validate_docs.py、git diff --check | Saved/SkillIndicatorGround/DeliveryGate.json / Docs-Final.json | 四个 Gate 通过；68 Markdown / 356 local links，0 errors；diff 检查通过 |
| Editor | UE 5.8.2 Build.bat ue_gasEditor Win64 Development，WaitMutex / NoHotReloadFromIDE | EditorBuild.log / EditorBuild-LocalExposure.log | 首次 16 actions 及最终增量 4 actions 均 Succeeded |
| Automation | Automation RunTests Combat.Input.；TestExit=Automation Test Queue Empty | Automation/index.json | 7/7，0 failed / 0 notRun |
| 资产 | Unreal Python 材质/地面保存回读、独立进程重载；run=CombatAssetValidation | AssetReadBack.json / FreshAssetVerification.json / AssetValidation-Final.json | 3 × 38 接收组件、13 个材质输入连接；17 定义，0 errors / 0 warnings |
| PIE | 训练场运行 CombatAimPIESmoke + CombatAimGroundSmoke；RunPIE.py 等待导航后启动原生场景 | PIE-LocalExposure.log / PIEResult.json / GroundVisual-*.png / PIE-*.png | 41/41，5 请求；原 22 项输入与追近、4 项 Hero 地面检查、15 项渲染/请求检查全部通过 |
| Network / Dedicated | N/A，无复制/RPC/Owner/结算变化 | AIM-001 历史网络证据不充当本轮新验证 | 不适用 |
| Soak / Perf | 静态检查每帧查询次数、无全场遍历；复用三层材质 | F2 diff 审查 | 静态审查通过；没有长稳或 GPU 基准，不能据此声称性能无开销 |

## 8. 风险、回滚与升级

- 风险：CustomDepth 地面在 Hero 后仍可采样，必须比较实际 SceneDepth；Substrate 下法线读取/深度支持需画面验证；Stencil 位与新地面配置需要明确文档。
- 回滚方式：按本轮保存的差异/资产备份撤销辅助、查询入口、材质和配置/地图标记；保留 AIM-001 和用户差异，不整体 git reset。
- 触发升级的条件：过滤需要修改引擎/权威协议、影响其他 stencil 归属，或三轮缺陷仍未收敛；先修订计划。
- 需要人决定的问题：无新增产品决定；若正在运行的用户 Editor 阻止编译，应先让用户保存关闭，不强制丢弃内容。

## 9. 交付证据

- 代码/资产 diff：本轮对照 Saved/SkillIndicatorGround/Baseline 审查；只新增地面辅助、统一技能查询、扩大坡道投影及对应测试。118 个资产包差异由 114 个地面外部 Actor、3 张地图和 1 个指示器材质组成，全部匹配 Git LFS；保留 AIM-001 和用户原有差异，未修改 Hero 蓝图/网格/技能定义。
- 构建结果：用户保存并关闭原 Editor 后完成 UE 5.8.2 Editor 构建；最新 EditorBuild-LocalExposure.log 为 Succeeded。
- Automation/PIE 报告：Saved/SkillIndicatorGround/VerificationSummary.json 汇总实际检查、像素指标和资产路径；Automation/index.json 7/7，PIE-LocalExposure.log 41/41，AssetValidation-Final.json 17/17。
- 可见效果：PIE-Circle.png、PIE-Line.png 中 Hero、木桩与平台侧壁没有指示器图案；GroundVisual-Off/Circle/Line.png 是同一静止场景的实际 GPU 输出，未以合成图代替验证。
- 未执行验证及原因：成品 cook/打包、多平台/DPI 与性能基准不属于本次缺陷修复验证。
- 剩余风险：新增地面必须按 authoring 约定配置；CustomDepth 增加地面深度绘制，尚未量测 GPU 开销。复杂多层地形与其他渲染平台仍需专项验证。用户已完成本轮实机验收并授权本地提交，未授权远程推送。

## 10. 变更记录

| 版本 | 日期 | 修改 | 原因 |
| --- | --- | --- | --- |
| 0.1 | 2026-09-13 | 冻结地面 whitelist、独立查询与遮挡过滤方案 | 用户授权修复 |

## 11. 路由、自评与 Reflect

### 路由记录

- 原始需求摘要：处理已确认的指示器接收对象与点目标高度问题。
- 主 Skill：combat-feature-development。
- 选择依据：通用本地输入与渲染 Bug。
- 备选 Skill 与排除理由：combat-skill-development；技能结算不变。
- 路由置信度：high

### 交付自评

| 维度 | 权重 | 分数 | 证据与扣分原因 |
| --- | ---: | ---: | --- |
| 需求与 AC | 20% | 4.5 | 原生 Hero 落点与地形像素检查闭合缺陷；首次交付评分时最终手感待用户验收 |
| 架构与权限 | 20% | 5.0 | Controller 三处技能入口与 Tick 共用查询，原服务器 Targeting/Order、普通输入和权限不变 |
| 实现与数据 | 20% | 4.5 | 三地图 114 个组件持久化回读、保留 Hero 其他贴花；新地形仍需显式配置 |
| 验证证据 | 20% | 4.5 | Editor、7/7 Automation、17 定义、41/41 PIE 及 GPU 对照通过；其他平台、cook 和 GPU 基准未执行 |
| 文档与可观测性 | 10% | 5.0 | ADR-054、10-13、Spec 与台账同步，保留真实失败记录和最终汇总 |
| 交付卫生 | 10% | 5.0 | 基于本轮备份隔离差异，118 个二进制均 LFS，无 Hero 资产变化，无提交/推送 |

首次交付总分：`4.7 / 5.0`。当前状态：`用户已验收`（2026-09-13）；保留原评分，不将验收自动换算为测试分数。

### Reflect 与调优

- 观察与证据：此前输入、参数和网络测试未覆盖实际接收像素；本轮独立加载发现 StaticMesh 默认碰撞继承会覆盖临时通道响应，GPU 对照发现局部曝光会因亮地面而改变 Hero/道具亮度。
- 根因类别：单次实现与验证遗漏。
- 调整文件与预期收益：本任务的接收/射线测试、画面对照和 10-13 固化地面持久化配置与真实像素验证；受控测试相机排除局部曝光/泛光，生产渲染设置保持原值。不扩大修改通用 Skill。
- 回归验证：FreshAssetVerification.json 的三地图持久化检查通过；最终圆/线地面平均 RGB 最大通道差约 155.6，Hero/道具差值 <= 0.16（阈值 2），标记坡道/平台差值 >= 143.9（阈值 8）。
- 需要用户决定的问题：无，2026-09-13 已完成实机验收。

### F2 对抗审查

- 审查人/版本：Codex，0.1；以本轮备份 diff、真实调用入口、引擎实现、资产独立重载及 GPU 输出重新审查。
- 接收边界：仅 stencil 位不足以阻止角色被染色，因此实际材质同时检查 CustomDepth 与 SceneDepth 的一致性。法线阈值排除被标记平台的侧壁；另一个 stencil 值 19 的遮挡物通过像素负例。保留低七位及角色原 ReceivesDecals。
- 输入边界：所有点目标查询共用入口，空命中、陡面、未标记组件和 Pawn 命中失败并清空旧目标；单位技能和攻击仍按真实 Visibility 命中。点预览与确认使用同一地面高度；超距地面通过原服务器追近路径。
- 持久化修复：第一次保存后内存回读为 Block，但独立重载变为 Ignore。已先退出 StaticMesh 默认碰撞继承再设置组件通道，逐个断言原有通道响应、碰撞启用模式和对象类型不变，最终新进程确认 114/114。
- 验证隔离修复：SceneCapture 注册会重置 ShowFlags，因此测试在注册后设置；局部曝光导致 Hero/道具出现整体变暗，关闭测试相机 LocalExposure 后差值从 6～20 降至 <= 0.16，未放宽断言阈值、未关闭生产后处理。
- 生命周期/性能：生产没有新 Timer、异步回调、扫描或复制对象。临时 GPU 场景只由显式测试参数启用，由测试 Actor 持有 RT/捕获组件和临时 Actor 引用；正常结束与 EndPlay 共用幂等清理并恢复英雄动画。每帧仍是一次技能射线和三个复用贴花层；新增 CustomDepth 开销未做 GPU 基准。
- 无未关闭高风险发现；测试输出和截图支持本轮修复，用户已完成实机验收，未执行的 cook/跨平台/性能边界继续保留。

### Red / Green 记录

- Red：GroundBaseline.json / Red.json 记录三地图 Floor 未开启 CustomDepth、stencil 为 0；上一轮 Saved/SkillIndicators/PIE-Circle.png 显示 Hero 与墙被绘制。这是修改前的真实 Golden Case，不宣称当时已运行新增 Automation。
- Green：真实组件射线 Automation 7/7；独立保存重载后 114/114 地面生效；17 定义校验通过；最终原生 PIE 41/41，原 22 项输入行为没有下降。
- 画面断言：分别捕获关闭、圆形和直线指示器，使用实际生产材质与 Hero 网格；测试仅增强 Fill 便于检测。未标记道具及其他 stencil 遮挡物像素不变，坡道/高台像素变色；原始游戏视口另外检查木桩和平台侧壁。
