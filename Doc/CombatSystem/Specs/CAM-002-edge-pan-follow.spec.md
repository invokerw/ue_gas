# CAM-002 边缘滚屏与 Space 临时跟随实现

> Spec 版本：`0.1`
> 状态：`COMPLETED`
> Owner：Codex
> 创建日期：2026-09-20
> 关联进度台账：`Doc/CombatSystem/00-Project/00-01-Progress-Tracker.md` §12.10
> 风险等级：`L1`

## 0. Intake 与 Gate 记录

- 用户请求：2026-09-20 确认 CAM-001 v0.2 review 完成并要求开始实现。普通镜头只采用边缘滚屏，不需要抓取键；Space 按住跟随、松开停留。同日实现交付后用户确认“review完成，提交吧”，接受 CAM-002 并授权本地 Git 提交。
- 附件解释：无附件；CAM-001/10-16 为已验收的设计输入。
- 已读取入口：`agent.md`、`README.md`、进度台账、`00-03` 测试计划、`00-04` ADR、`00-05` 工作流、DDD `10-01`、`10-09`、`10-10`、`10-16`、`90-16` 生命周期审计、`30-01` UE MCP 工作流、`Skills/combat-task-router/SKILL.md`、`Skills/combat-feature-development/SKILL.md`。
- 主 Skill：`Skills/combat-feature-development/SKILL.md`
- 备选 Skill 与排除理由：任务路由 Skill 用于路由；本任务不实现 GAS 技能，无需技能开发 Skill。
- 路由置信度：`high`
- F0 结论：`GO`
- F0 依据：用户已批准交互设计和实施；兼容新增本地相机逻辑，不改变 gameplay 权威或发布协议。
- F1 结论：`APPROVED`
- F1 审查人：Codex（依据用户已审阅设计进行 L1 实施计划核对）
- F1 审查版本：`0.1`
- F1 计划审查证据：2026-09-20 preflight/plan Gate 通过；逐项核对已验收的 EdgePan/FollowHeld 设计、配置初值、UI/Owner/失焦清理、Demo 三资产迁移、Red/Green/PIE/网络矩阵与回滚，范围仍为用户授权的本地相机实现，不改变服务器权威或发布协议。
- Build 解锁：F1 已批准，进入 build Gate。
- F1 重审条件：范围、权限、迁移、测试矩阵或回滚发生实质变化时递增 Spec 并重新审查。
- F2 结论：`PASS`
- F2 证据：单人本地对抗复核以 Spec、最终 diff、引擎 API 和测试结果为依据；检查绑定去重/Owner 乱序、按住号、输入失焦、网络旁路、帧率/方向与资产集成。发现“已贴边时按 Space 下一帧被旧边缘状态撤销”，增加离边重武装与回归断言后关闭；没有使用独立子 agent，不声称外部审查。
- Push-Ready 结论：`READY`
- Push-Ready 证据：本地实现、测试、构建、资产、文档与对抗复核齐备；最终 delivery 输出见 §9。用户已 review 完成并授权本地提交，不推送。
- 验证：2026-09-20 安装版 Editor 构建、完整 Combat 101 项（99 success + 2 success with warnings，0 failed）、相机 5 项、PIE、独立服务器/双客户端、Demo 蓝图编译保存冷回读、31 项资产校验通过；日志位于 `Saved/CameraValidation/`。
- 未执行：物理鼠标/Alt-Tab/不同 DPI 可见窗口手感验收（自动场景注入边缘样本，交用户体验复核）；本次未重建源码 Server/Client Target、cook 或打包（本地相机无网络 schema/新 gameplay 平台依赖，采用安装版真实独立 -server/-game 连接覆盖运行时隔离）。UE MCP endpoint 拒绝连接，资产通过 Unreal Python Editor API 完成。

## 1. 目标与范围

### 目标

将已验收的边缘滚屏与 Space 跟随接入 Demo，停止当前无条件跟随。

### 范围

Command Pawn 相机模式、Controller 视口门控、Space Enhanced Input、Demo 输入资产、回归测试与当前行为文档。

### Non-Goals

不做 Camera Grip、缩放、旋转、小地图、相机存档、观战或手柄；不改变 Unit 移动、Order RPC、Owner、生命代次与发布契约。

## 2. 当前事实与依据

本节保留开工时的事实快照；最终行为见 §3 与 §9。

- `CombatCharacter.cpp::Tick` 当前无条件追随 `FollowTarget`；`SetFollowTarget` 每次立即对齐。
- `CombatPlayerController.cpp::RefreshCommandBinding` 在指针/代次刷新时设置相机目标；输入绑定在 `SetupInputComponent`。
- `GetReadyCommandedUnit` 同时校验指针和 Unit Owner；相机继续使用该显式来源。
- 开工工作区只有 CAM-001 的四个文档差异；无代码/资产差异，没有 `.codegraph/`。
- 当前 callable 工具未暴露 UE MCP；先探测本地 endpoint，若不可用，用同一 UE Editor 的 Python/Automation 做读取、修改、编译、保存和冷回读；不直接改写二进制。

## 3. 行为与契约

### 主流程与状态转换

默认 `Free`，首次有效绑定对齐一次。有效视口边缘输入进入 `EdgePan`，向屏幕对应方向持续平移，角落归一化避免斜向增速，移出触发带默认立即停止。Space Started 且主控就绪进入 `FollowHeld`；Completed/Canceled 冻结当前位置。跟随中新进入边缘时滚屏接管，仍按住的旧 Space 手势不重新跟随，需松开重按。已经贴边时按 Space 可以接管到跟随，原有边缘输入等到鼠标离开后才重新武装；否则 EdgePan → FollowHeld 会在下一帧被原有边缘输入立即撤销。

### 输入、输出与数据约束

边缘输入读取本地视口绝对指针位置，无 Input Action，无抓取键。只新增 `IA_CameraFollow` Boolean，Demo IMC 映射 Space。配置中文 DisplayName/ToolTip：边缘开关 true、阈值短边 2.5%、速度 1800 cm/s、停止减速默认 0（立即停止，可调）、跟随插值 12/s、可选 XY 矩形边界默认关闭。锚点 Z 固定为绑定时高度，跟随只追踪 XY，不保留观察偏移；死亡 Actor 尚有效时继续跟随。

### 权威边界与权限

只在本地 Controller 驱动 Command Pawn；Dedicated 和远端 Controller 不更新镜头。Pawn 保持 `SetReplicateMovement(false)`；不新增 RPC、Scheduler 或 Unit 写入。单帧只执行一种相机模式。

### 失败、取消、过期、死亡、EndPlay 与重复请求

窗口失焦、视口无效、指针出界、UI 捕获/拖放、HUD 命中和技能/攻击瞄准时禁止边缘滚屏并清除平移惯性；失焦取消跟随。输入释放只结束匹配的会话号。绑定指针/代次改变、Pawn 更换、Unit Owner 延迟/丢失、Unit/Controller EndPlay 清理旧状态；幂等刷新不重复居中。每帧重新检查绑定，兼容客户端 Pawn/Unit/Owner 乱序到达。

### 兼容、版本与迁移

保留现有 Pawn、Controller、FollowTarget 入口；新增 C++ 本地相机 API/属性记 ADR-061，不改变网络 schema。旧蓝图无 Follow Action 时安全禁用 Space，边缘仍可用；Demo 明确配置新资产，不在 C++ 写死 Space。回滚移除相机增量及新资产引用并恢复旧跟随；不回滚 Unit/Owner 系统。

## 4. 实施计划

| 文件/资产 | 变更 | 原因 | blast radius |
| --- | --- | --- | --- |
| `Source/Combat/CombatCharacter.h/.cpp` | 本地相机状态、平面速度/边界、按住跟随与绑定去重 | 复用唯一 Command Pawn | L1 本地表现 |
| `Source/Combat/CombatPlayerController.h/.cpp` | PlayerTick 视口门控、Follow Action、失焦/绑定清理 | 输入与相机时序一致 | L1 输入 |
| `Source/Combat/Combat/Tests/CombatCameraTests.cpp` | Red 自由镜头、方向/速度/互斥/代次/输入资产/权限回归 | 可执行验收 | 测试 |
| `Source/Combat/Combat/Tests/` | 按需补充 PIE/网络相机 smoke | 本地逻辑在真实连接中隔离 | 测试 |
| `Tools/setup_camera_input.py` | 精确读取/配置/编译/保存 Demo 输入资产 | 无 MCP 时用 Editor API | 工具 |
| `Tools/RunDedicated.ps1` | 新增可选 `-Camera`，复用既有独立双客户端启动与清理 | 可重复执行相机隔离 smoke | 测试工具；默认行为保持 |
| `/Game/Combat/Demo/Input/IA_CameraFollow`、`IMC_Default`、`/Game/Combat/Demo/Framework/BP_CombatDemoPlayerController` | Boolean/Space 映射及属性引用 | Demo 可直接操作 | 三个输入资产 |
| `10-16`、`10-09`、`00-04`、README/索引/台账/Spec | 同步实际行为、验收与证据 | 单一状态入口 | 文档 |

## 5. 验收标准（AC）

- [x] AC-01：无按键边缘平移，四边/角落方向、阈值、帧率、Z 与可选边界正确，移出立即停止；自动样本通过，用户已 review 完成。
- [x] AC-02：默认不追随；Space 按住跟随就绪主控、松开/取消停止，边缘接管后旧按住不复活。
- [x] AC-03：门控接入既有真实 UI/瞄准与视口状态；禁止平移/清除惯性、无视口取消、出界、重绑/失效/旧释放、重复绑定自动检查通过。真实 UI 鼠标命中和 Alt-Tab 交用户复核。
- [x] AC-04：实际 Demo Action 映射与 Controller 引用保存、编译并冷回读；右键/技能输入回归通过。
- [x] AC-05：相机不写 Unit、不发 Order，独立双客户端相机可不同，服务器单位移动权威保持。

## 6. Definition of Done

- [x] 记录 Red/Green；Editor 编译及相机/输入/移动回归通过。
- [x] 资产回读/编译/校验、PIE/网络证据与未覆盖边界齐备。
- [x] 过时设计改为当前行为；文档与 diff 校验通过，delivery 结果见 §9。
- [x] F2 和自评完成；实现交付不等于用户体验验收。

## 7. 测试矩阵与命令

本机 UE 路径由 `python Tools/ue_environment.py check --json` 解析。

| 层级 | 用例/命令 | 预期证据 | 结果 |
| --- | --- | --- | --- |
| Gate | `python -B Tools/task_gate.py --mode preflight --spec Doc/CombatSystem/Specs/CAM-002-edge-pan-follow.spec.md --kind feature`；plan/build/delivery 同 Spec | 开工 plan 为会话输出；交付 `PreflightGate.json`、`BuildGate.json`、`DeliveryGate.json` | 开工 preflight/plan/build PASS；最终 delivery PASS |
| 构建 | 安装版 `Build.bat ue_gasEditor Win64 Development <project> -WaitMutex` | `Saved/CameraValidation/Build.log` | PASS，最终代码重新构建成功 |
| World Automation | `-ExecCmds="Automation RunTests Combat.;Quit" -TestExit="Automation Test Queue Empty"` | `Saved/CameraValidation/Final/index.json`、`Final.log` | 101/101，含相机 5/5；2 个原有调试命令边界测试带预期 warning |
| 资产 | `-run=pythonscript -script=<repo>/Tools/setup_camera_input.py`，冷回读追加 `-CameraReadOnly`；`-run=CombatAssetValidation -Report=<repo>/Saved/CameraValidation/Assets.json` | `InputBefore.json`、`InputMigration.json`、`InputReadback.json`、`Assets.json` | 蓝图 UpToDate、Follow Boolean，16→17 映射且原 16 条保留；31 资产 0 error/0 warning |
| PIE | `python Saved/CameraValidation/run_pie_host.py`，Editor `-CombatCameraSmoke -CombatCameraReport=PIECamera -ExecutePythonScript=<repo>/Saved/CameraValidation/RunPIE.py` | `PIEReport.json`、`PIECamera.txt`、`PIE.log` | PASS；实际 PIE 开始/结束，边缘样本注入 |
| 网络 | `Tools/RunDedicated.ps1 -InstalledEditor -Camera -Port 7871 -TimeoutSeconds 100` | `Saved/UEEnvironment/Dedicated-Installed-Camera/DedicatedSummary.txt`、各端日志 | 服务器和双客户端相机 PASS；HUD、SAM 碰撞、64 Unit/256 Modifier 容量均 PASS |
| 文档 | `python -B Tools/validate_docs.py`；`git diff --check` | §9 最终计数 | PASS；换行转换提示不是 diff 错误 |
| Soak/Perf | 本地相机无 gameplay 周期/网络载荷；不扩大到长期 soak | N/A | 不适用 |

## 8. 风险、回滚与升级

- 风险：重复 OnRep 居中、Space 释放误结束新会话、UI 捕获丢鼠标位置、斜向超速、Dedicated 意外运行本地逻辑。
- 回滚方式：撤销本任务相机逻辑与输入资产差异，恢复无条件跟随；保留此前 ECON 等提交。
- 触发升级的条件：需要移动权威/复制契约、全屏 UI 改造或额外相机交互。
- 需要人决定的问题：无阻塞项；已在授权设计范围内给出可编辑初值，最终手感由用户实机复核。

## 9. 交付证据

- Red：`Saved/CameraValidation/Red.log` 与 `Red/index.json` 记录旧代码在单位移动 1000 cm 后镜头也移动 1000 cm，自由锚点断言失败。早期夹具使用未初始化 LocalPlayer 同时产生 ensure；已改用引擎 `SetAsLocalPlayerController()`，最终夹具没有该 ensure。Red 记录不冒充干净单一失败。
- Green：最终 `Final/index.json` 为 99 success + 2 success with warnings，0 failed/0 notRun。两个 warning 测试为 `Combat.Economy.DebugSetGoldCommandBoundaries` 与 `Combat.Progression.DebugAddExperienceCommand` 的既有非法输入诊断；相机五项全部无 warning。
- 实施资产：新增 `Content/Combat/Demo/Input/IA_CameraFollow.uasset`；修改 `IMC_Default.uasset` 和 `BP_CombatDemoPlayerController.uasset`。使用 DataAssetFactory 创建 InputAction，DefaultKeyMappings 读取映射，编译/保存后在独立 Editor 进程冷回读。
- 网络证据：两个客户端分别使用左右边缘样本，镜头平移方向相反，独立返回跟随与释放结果；服务器检查 2 个远端 Controller，相机写入被拒绝，Unit AI possession 保持。日志 `Input=Synthetic` 明示覆盖范围。
- Push-Ready 六层：Tests PASS；Types/Build PASS（Editor）；No Regression PASS（完整 Combat、HUD/SAM/容量）；Adversarial PASS（本地对抗复核与已关闭缺陷）；DDD/Constraints PASS；Decisions PASS（ADR-061、实施默认值与回滚齐备）。源码 Server/Client 重建、cook/发布与长期 soak 本次不适用；未覆盖的物理输入验收保留为用户体验验收项。
- 最终文档/diff/delivery：`python -B Tools/validate_docs.py` 为 81 Markdown、411 local links、0 error；`git diff --check` 无错误；`python -B Tools/task_gate.py --mode delivery --spec Doc/CombatSystem/Specs/CAM-002-edge-pan-follow.spec.md --kind feature --report Saved/CameraValidation/DeliveryGate.json` PASS（21 changed files）。交付阶段误重跑 plan 因 READY_FOR_REVIEW 不属于计划阶段被正确拒绝，记录在 `PlanGateOutOfPhase.json`，不覆盖开工 F1 审批，也未倒退任务状态绕过阶段约束。

## 10. 变更记录

用户 review 后的提交检查：只更新验收文档，未修改已验证的代码/资产；重新执行文档校验（81 文件、411 链接、0 error）、diff 检查和 delivery Gate 通过，报告为 `Saved/CameraValidation/CommitDeliveryGate.json`。运行时矩阵沿用 §7 的同版本结果。

| 版本 | 日期 | 修改 | 原因 |
| --- | --- | --- | --- |
| 0.1 | 2026-09-20 | 建立实施计划，接受 CAM-001 v0.2 设计审阅 | 用户要求开始实现 |
| 0.1 | 2026-09-20 | 实现与验证；复用 RunDedicated 增加相机模式，澄清已贴边时 Space 接管的时序 | 落实既有 EdgePan → FollowHeld 契约，无新增交互或网络权限 |
| 0.1 | 2026-09-20 | 用户确认 review 完成，任务转 COMPLETED 并授权本地 Git 提交 | 仅更新验收记录，运行时代码和资产保持已验证版本；不推送 |

## 11. 路由、自评与 Reflect

- 路由：功能开发，high；输入/相机兼容新增。
- 自评：交付时 **4.6/5**；当前状态为**用户已验收**。评分保留当时验证边界，不因 review 确认而补写未执行的物理输入测试。

| 维度 | 权重 | 分数 | 证据与扣分 |
| --- | --- | --- | --- |
| 需求与 AC | 20% | 4.5 | 已实现授权交互；最终速度和触发宽度待用户体验 |
| 架构与权限 | 20% | 5.0 | 唯一本地 Pawn、Unit 只读、无 RPC，独立双客户端验证 |
| 实现与数据 | 20% | 4.5 | 状态机/生命周期与三资产齐备；真实桌面手势组合待复核 |
| 验证证据 | 20% | 4.0 | 101 Automation、PIE、真实连接、31 资产；未以合成输入替代物理输入验收 |
| 文档与可观测性 | 10% | 5.0 | 10-16/10-09、ADR、Spec、台账、中文 ToolTip 和 smoke 证据 |
| 交付卫生 | 10% | 5.0 | 精确资产、保留原 16 映射、不提交、无无关变更 |

- Reflect：UE 5.8 的旧 `Mappings` 字段能读到空数组，容易误判没有既有输入；通过新版 DefaultKeyMappings 回读和原映射保持断言固定处理方式，记录在 10-16 与迁移脚本。本轮低风险文档调优仅补充该版本注意事项，不改 Skill 或评分规则。
- 用户验收状态：设计与实现均已验收；2026-09-20 用户明确“review完成，提交吧”，授权本地 Git 提交。未额外执行物理鼠标/Alt-Tab/DPI 自动验证，§7 的运行证据与范围保持原记录。
