# CAM-001 Dota 风格视角移动调研与设计

> Spec 版本：`0.2`
> 状态：`COMPLETED`
> Owner：Codex
> 创建日期：2026-09-17
> 关联进度台账：`Doc/CombatSystem/00-Project/00-01-Progress-Tracker.md` §12.10
> 风险等级：`L0`

## 0. Intake 与 Gate 记录

- 用户请求：重新核对 DOTA2 的视角移动设计；普通视角移动采用鼠标贴屏幕边缘自动平移，不增加抓取键平移镜头，并用空格锁定当前控制的主要目标单位。
- 附件解释：`无附件`；Dota 2 是用户指定的参考产品，不是待迁移的工程资产或代码约束。
- 已读取入口：`agent.md`、`README.md`、`Doc/CombatSystem/README.md`、`Doc/CombatSystem/00-Project/00-01-Progress-Tracker.md`、`Doc/CombatSystem/00-Project/00-05-AI-Native-Development-Workflow.md`、`Doc/CombatSystem/10-Architecture/10-01-Scope-Architecture.md`、`10-07-Order-Movement.md`、`10-09-Client-Server-Interaction.md`、`10-10-Server-Authoritative-Movement-Kickoff.md`、任务路由 Skill、功能开发 Skill。
- 主 Skill：`Skills/combat-feature-development/SKILL.md`（文档交付需要仓库索引、Spec、验证和交付证据）。
- 备选 Skill 与排除理由：`Skills/combat-task-router/SKILL.md` 负责路由与复盘，已作为入口使用；未使用技能开发 Skill，因为本任务不实现 GAS Ability。
- 路由置信度：`high`
- F0 结论：`GO`
- F0 依据：用户明确要求调研并形成仓库文档；范围是 L0 文档设计，不改变运行时代码、资产、网络载荷或发布契约。
- F1 结论：`APPROVED`
- F1 审查人：Codex（本地计划审查）
- F1 审查版本：`0.2`
- F1 计划审查证据：已运行 `python -B Tools/task_gate.py --mode plan --spec Doc/CombatSystem/Specs/CAM-001-dota-camera-research.spec.md --kind docs` 并通过。版本 `0.2` 将普通视角移动收敛为边缘滚屏，Camera Grip 仅保留为 Dota 外部参考/未来扩展，不进入本次输入资产、状态机或验收范围；AC、Non-Goals、回滚和未执行的 UE 验证与范围一致，准许进入文档修订。
- Build 解锁：`已解锁`；已运行 `python -B Tools/task_gate.py --mode build --spec Doc/CombatSystem/Specs/CAM-001-dota-camera-research.spec.md --kind docs` 并通过；本任务仍为纯文档变更，不进入 UE 行为 BUILD。
- F1 重审条件：范围、架构、权限、迁移、测试矩阵或回滚方案发生实质变化时先递增 Spec 版本，F1 回到 `REVISE`、任务状态回到 `PLAN_REVIEW`；重审通过前不得继续对应代码修改。
- F2 结论：`PASS`
- F2 证据：独立检查确认普通视角移动仅保留 Edge Pan，Camera Grip 已从本任务状态机、输入资产和验收项移除。
- Push-Ready 结论：`READY`
- Push-Ready 证据：文档事实、索引、台账和 Spec v0.2 已同步。
- 验证：`python -B Tools/validate_docs.py` 通过（80 Markdown files、405 local links、0 error）；`python -B Tools/task_gate.py --mode plan ... --kind docs` 通过；`python -B Tools/task_gate.py --mode build ... --kind docs` 通过；`git diff --check` 未发现本任务新增空白错误，但报告了用户既有 `Source/Combat/Combat/Economy/CombatEconomyComponent.cpp:834` 的 EOF 空行。标准 delivery CLI 仍因共享工作区既有 37 个差异触发 `kind_mismatch`，不将其归因于 CAM-001；以本任务 4 个 Markdown 文件调用同一 `evaluate(..., changed=[...])` 入口通过，作为范围化 delivery 证据。
- 未执行：UE 编译、Automation、PIE、Dedicated 和 cook/打包；本任务没有运行时行为或资产变更，不适用。

## 1. 目标与范围

### 目标

形成一份可供实现评审使用的 Dota 风格相机设计，回答：Dota 2 的参考交互是什么、当前 Combat 相机能复用什么、边缘滚屏和空格跟随如何组成不冲突的状态机、网络/生命周期/输入优先级如何落地，以及实现前需要确认哪些参数。

### 范围

- 调研 Dota 2 的边缘滚屏、Camera Grip（仅作外部可选设置参考）、Hold Select Hero to Follow、相机速度/减速和相关快捷键语义。
- 对照当前 `ACombatCharacter` Command Pawn、`ACombatPlayerController`、`CommandedUnit` 和 Enhanced Input 事实。
- 提出边缘滚屏、按住空格跟随、松开冻结、重绑定和失效处理的建议基线；不为本任务增加抓取键平移镜头。
- 列出实现文件、输入资产、配置项、自动化/PIE/联机验收用例和回滚方式。

### Non-Goals

- 本任务不修改 C++、蓝图、DataAsset、Input Mapping、关卡或发布版本。
- 本任务不实现边缘滚屏、滚轮缩放、小地图跳转、镜头位置编组、旋转相机、触摸/手柄适配、镜头震动或 Camera Grip 拖拽；本文只冻结设计。
- 本任务不改变 Combat Unit 的服务器移动、Order、Owner、ASC、碰撞或网络权威语义。

## 2. 当前事实与依据

- 相关 DDD：`10-01` 规定一个事实只有一个权威来源；`10-07`/`10-09` 规定输入从显式 `CommandedUnit` 提交服务器 Order；`10-10` 规定 PlayerController 只 Possess 无碰撞 Command Pawn，镜头是客户端表现且不能回写 Unit Transform。
- 代码事实：`Source/Combat/CombatCharacter.h:15-58` 定义无碰撞 Command Pawn、固定 SpringArm/Camera、`FollowTarget` 和 `CameraFollowSpeed`；`Source/Combat/CombatCharacter.cpp:41-65` 当前每帧以 `FollowTarget` 的位置插值设置 Command Pawn 位置，`SetFollowTarget` 会立即对齐目标。
- 代码事实：`Source/Combat/CombatPlayerController.h:27-225` 定义 owner-only `CommandedUnit`、`CommandBindingGeneration`、`RefreshCommandBinding` 和 Enhanced Input 入口；`Source/Combat/CombatPlayerController.cpp:175-214` 在绑定复制/控制切换时幂等刷新相机目标，并以 `GetReadyCommandedUnit()` 防止 Owner 尚未就绪时发 RPC。
- 外部依据：Valve 的 Spring Cleaning 2016 说明新增 Immediate Camera Grip 和 Camera Control Group Hotkeys；Liquipedia 的 Game Settings 记录 Camera Grip、Hold Select Hero to Follow、Left-Click Activates Camera Grip、Camera Speed、Camera Deceleration 等设置；相关链接收录在设计文档的“资料来源”。
- 已知限制或待决策项：本任务采用用户确认的“按住空格跟随，松开停在当前位置”，并确认普通视角移动使用无需按键的屏幕边缘滚屏；实现时仍需决定边缘阈值/速度、相机边界、平面高度策略和跟随插值。Camera Grip 不属于本次实现范围。

## 3. 行为与契约

### 主流程

1. 本地 owning client 的 Command Pawn 读取视口指针位置；指针进入未被 UI 捕获的屏幕边缘时进入 EdgePan，离开边缘后停止平移。
2. EdgePan 只按边缘方向和本地速度/减速参数改变 Command Pawn 的相机锚点 XY；不调用 Unit 移动接口、不发 Order RPC。
3. 按下 Space 后进入 FollowHeld：目标取当前已就绪的 owner-only `CommandedUnit`，相机平滑跟随目标；不改变 Unit 的位置、朝向、Order 或生命状态。
4. 松开或取消 Space 后退出 FollowHeld，保留当时的相机锚点；不会自动跳回单位，也不会发 Stop 或移动命令。
5. EdgePan 与 FollowHeld 同时发生时，按本项目互斥规则由边缘滚屏接管本地镜头并结束本次 FollowHeld；要再次跟随需重新按下 Space。该规则避免手动平移与跟随插值在同一帧竞争。
6. `CommandedUnit` 重绑定、EndPlay 或 `CommandBindingGeneration` 变化时，清理边缘滚屏/跟随状态；若有新有效目标则将锚点一次性置于新目标，否则保持最后锚点并进入 Free。

### 状态转换

| 状态 | 进入 | 每帧行为 | 退出 |
| --- | --- | --- | --- |
| `Free` | 初始、离开边缘、松开 Space、绑定失效 | 保持当前锚点；指针到边缘时进入 `EdgePan` | Space Started → `FollowHeld`；边缘进入 → `EdgePan` |
| `EdgePan` | 指针进入未被 UI 捕获的屏幕边缘 | 按边缘方向平移锚点；不跟随 Unit | 指针离开边缘 → `Free`；Space Started → `FollowHeld`；绑定刷新 → `Free` |
| `FollowHeld` | Space Started 且 `GetReadyCommandedUnit()` 有效 | 按 `FollowInterpSpeed` 跟随目标；目标无效时降级 `Free` | Space Completed/Canceled → `Free`；边缘进入 → `EdgePan` 并使本次 Space 失效 |

### 输入、输出与数据约束

建议的本地 Input Action：

| Action | 默认映射 | 类型/事件 | 说明 |
| --- | --- | --- | --- |
| `IA_CameraEdgePan` | 无 | 鼠标位置检测屏幕边缘 | 唯一的默认视角移动，不提交 gameplay Order |
| `IA_CameraFollow` | Space | Boolean Started/Completed/Canceled | 用户确认的按住跟随；松开冻结当前位置 |

建议配置项属于 Command Pawn/Controller 的本地表现配置：`bEnableEdgePan`、`EdgePanScreenThreshold`、`EdgePanSpeed`、`EdgePanDeceleration`、`CameraFollowInterpSpeed`、`CameraAnchorZMode`、`CameraBounds`。它们不进入 Combat DefinitionId、RPC 载荷、服务器属性或战斗事件。

### 权威边界与权限

- 相机状态、锚点、输入会话和插值均为 owning client 本地状态，不复制、不保存为 Combat gameplay 状态。
- `CommandedUnit` 仍是镜头唯一目标来源；不能从 `GetPawn()`、队伍、最近单位或客户端命中结果推断主控单位。
- 相机可读取 Unit 的复制位置，但不得 `SetActorLocation`、`AddMovementInput` 或调用 PathFollowing 改变 Unit；服务器仍是单位移动唯一权威。
- `CommandBindingGeneration` 和弱引用用于淘汰旧目标/旧手势；不以相机本地状态绕过服务器 Owner/Order 校验。

### 失败、取消、过期、死亡、EndPlay 与重复请求

- `CommandedUnit` 未复制、Owner 尚未就绪或已失效：Space 不进入跟随，边缘滚屏仍可自由移动；不发送 RPC。
- Space 被 `Canceled`（窗口失焦、输入冲刷）：与 Completed 相同，冻结当前位置并清理手势。
- Unit `Dead/Dying` 但 Actor 仍有效：默认继续跟随其复制位置；Unit EndPlay/绑定代次变化立即退出跟随。是否在死亡时自动回中心不属于本任务。
- 重复 Space Started/Completed、旧边缘状态或旧绑定回调必须通过本地会话号/绑定代次幂等忽略；不能让旧会话覆盖新绑定。
- 鼠标位于 HUD、战斗记录或技能瞄准 UI：边缘滚屏不启动，左键技能/右键移动的现有消费规则保持不变。

### 兼容、版本与迁移

- 这是设计文档，不改变 `combat_v4_economy_rc1`、GameplayTag、事件 schema、DefinitionId 或网络协议。
- 实现阶段优先在 `ACombatCharacter`/`ACombatPlayerController` 增加本地相机状态和 Input Action 引用；保留现有 Command Pawn 和 `SetFollowTarget` 的生命周期入口，避免新增第二个 Camera Pawn。
- 现有始终跟随逻辑迁移为“初始/重绑定时对齐 + FollowHeld 时跟随”；旧资产缺少新 Action 时应安全降级为当前相机行为或禁用对应能力，并在 Demo Input Mapping 中显式配置。

## 4. 实施计划

| 文件/资产 | 变更 | 原因 | blast radius |
| --- | --- | --- | --- |
| `Source/Combat/CombatCharacter.h/.cpp` | 增加本地相机状态、边缘滚屏、边界/插值和跟随会话；停止每帧无条件跟随 | Command Pawn 是当前相机唯一承载体 | L1；只影响本地镜头表现 |
| `Source/Combat/CombatPlayerController.h/.cpp` | 增加 Edge Pan/Follow 处理、UI 门控、绑定刷新时清理和会话代次 | Controller 已统一管理 Command Pawn 输入与 `CommandedUnit` | L1；与现有右键/技能输入共享焦点规则 |
| `/Game/Combat/Demo/Input/IMC_Default` | 配置 Space；边缘滚屏读取视口位置，不增加抓取键映射 | 让跟随键可编辑，普通平移不依赖按键 | L1；仅 Demo 输入资产 |
| `BP_CombatDemoPlayerController` / `BP_CombatCharacter` | 配置 Action 引用和本地参数，编译保存回读 | 保持蓝图可调而不复制 gameplay 权威 | L1；仅 Demo 资产 |
| `Doc/CombatSystem/10-Architecture/10-09-Client-Server-Interaction.md` | 实现后补充当前输入与本地镜头时序 | 维护端到端事实 | L0 文档 |

## 5. 验收标准（AC）

- [x] AC-01：调研文档明确区分 Dota 2 参考行为、当前 Combat 事实和本项目建议选择，并包含可访问的资料来源链接。
- [x] AC-02：文档冻结“边缘滚屏移动镜头、按住 Space 跟随当前主控 Unit、松开停在当前位置”，并定义边缘滚屏/跟随互斥、UI 消费、绑定代次和失效路径。
- [x] AC-03：文档给出不改变服务器移动权威的 UE 实施落点、输入资产、配置参数、测试矩阵和回滚方案。
- [x] AC-04：新增文档被 `Doc/CombatSystem/README.md` 和当前进度台账索引；纯文档验证通过，未声称 UE/PIE/Dedicated 已执行。

## 6. Definition of Done

- [x] 行为或可执行逻辑变化已用相关测试/Golden Case 验证；本任务纯文档，使用链接、格式和事实核对。
- [x] 实现通过直接测试，且没有绕过公共权威入口；本任务无实现。
- [x] 相关 Editor/蓝图/资产已编译、保存并回读；本任务不修改资产，标记 N/A。
- [x] 按风险完成 Editor、Server/Client、PIE、Dedicated 或 Soak 验证；本任务不改变运行时，标记 N/A。
- [x] 受影响的当前架构入口和公开中文说明已同步；已更新索引、台账和本设计文档。
- [x] `git diff --check` 通过，未混入用户修改或生成文件。

## 7. 测试矩阵与命令

| 层级 | 用例/命令 | 预期证据 | 结果 |
| --- | --- | --- | --- |
| 文档/本地工具 | `python -B Tools/validate_docs.py`（本机 `python3` 为 Microsoft Store alias，无输出退出 1，改用 `python`） | 80 Markdown、405 本地链接、0 error | 通过 |
| 文档/本地工具 | `git diff --check` | 无空白错误；仅 LF→CRLF 提示 | 通过 |
| World Automation | 不适用；本任务不改运行时 | 不新增 Automation | N/A |
| PIE / Blueprint | 不适用；未修改资产/蓝图 | 不新增 PIE | N/A |
| Network / Dedicated | 不适用；相机状态本地且未实现 | 不新增联机测试 | N/A |
| Soak / Perf | 不适用；未改变 Tick/运行时 | 不新增性能证据 | N/A |

## 8. 风险、回滚与升级

- 风险：当前相机每帧跟随和新自由锚点之间存在迁移边界；若实现只新增边缘滚屏而不拆分跟随状态，会出现松开 Space 后被下一帧拉回单位的问题。视口边缘若未排除 HUD/技能瞄准区域，可能误启动镜头平移。
- 回滚方式：文档层回滚新增设计文档及索引/台账条目即可；实现阶段若出现问题，移除 Edge Pan/Follow 配置并恢复 `ACombatCharacter::Tick` 的旧跟随逻辑，不触碰 Unit/Order/网络代码。
- 触发升级的条件：需要 Camera Grip/中键拖拽、滚轮缩放、镜头旋转、服务器/观战者镜头、多人共享镜头、录像回放、可存档镜头位置或改变 `CommandedUnit` 生命周期时，新增独立 Spec/ADR 并重新审查。
- 需要人决定的问题：实现阶段确认边缘阈值/速度/减速、相机边界来源、跟随插值/高度策略、边缘滚屏是否在 Space 期间直接接管，以及死亡时是否继续跟随。

## 9. 交付证据

- 代码/资产 diff：无；本任务仅新增/更新 Markdown 文档。工作区已有 ECON/物品代码差异保留未改。
- 构建结果：未执行；纯文档任务不适用。
- Automation/PIE/Dedicated 报告：未执行；纯文档任务不适用。
- 未执行验证及原因：UE 编译、Automation、PIE、Dedicated、cook/打包均未执行，原因是没有运行时或资产修改；共享工作区还存在用户既有 ECON/物品行为差异，未将其冒充为本任务验证。
- 共享工作区 delivery 说明：直接运行 `python -B Tools/task_gate.py --mode delivery --spec Doc/CombatSystem/Specs/CAM-001-dota-camera-research.spec.md --kind docs` 的状态/F2/Push-Ready检查已满足，但整体仍因共享工作区既有 37 个代码/文档差异触发 `kind_mismatch`；这些文件未由 CAM-001 修改，不能据此声称全工作区 delivery 通过。
- 剩余风险：相机功能尚未实现，边缘阈值/速度/减速、边界、高度和 Space 跟随接管规则仍需在实现评审中确认。

## 10. 变更记录

| 版本 | 日期 | 修改 | 原因 |
| --- | --- | --- | --- |
| 0.1 | 2026-09-17 | 创建调研与设计 Spec，冻结按住 Space 跟随/松开冻结的用户选择；完成设计文档、索引、台账、文档校验和 F2 审查 | 用户要求继续完成文档 |
| 0.2 | 2026-09-18 | 根据用户复核，将普通视角移动明确为屏幕边缘自动平移，移除 Camera Grip/拖拽键作为本任务的实现入口和验收项 | 修正 Dota 2 交互基线 |

## 11. 路由、自评与 Reflect

### 路由记录

- 原始需求摘要：调研 Dota 2 视角移动，形成鼠标拖动相机与 Space 锁定主控单位的项目文档。
- 主 Skill：`combat-feature-development`
- 选择依据：交付物进入仓库文档体系，需要 Spec、事实核对、索引、台账和 Gate；没有代码或 GAS 技能实现。
- 备选 Skill 与排除理由：`combat-task-router` 作为路由/复盘入口；`combat-skill-development` 与本任务对象不符。
- 路由置信度：`high`

### 交付自评

| 维度 | 权重 | 分数（0–5） | 证据与扣分原因 |
| --- | ---: | ---: | --- |
| 需求与 AC | 20% | 4.8 | AC-01–04 已闭合；用户确认 Space 语义，未包含实现验收 |
| 架构与权限 | 20% | 4.8 | 明确 Command Pawn、本地相机和服务器 Unit 移动边界 |
| 实现与数据 | 20% | 4.5 | 给出类/资产/参数落点；尚无代码实现证据 |
| 验证证据 | 20% | 4.0 | 文档校验和 diff 通过；UE/PIE/Dedicated 按范围 N/A |
| 文档与可观测性 | 10% | 4.7 | 资料来源、状态机、日志建议和索引齐全 |
| 交付卫生 | 10% | 4.5 | 保留共享工作区用户差异；标准 delivery CLI 受既有行为 diff 影响 |

- 计算总分：4.6/5.0
- 硬性封顶或未执行项：无；UE/Automation/PIE/Dedicated 未执行，因纯文档任务不适用。共享工作区标准 `--kind docs` delivery 检查会看到用户既有 Source/Tests 差异，需按下方说明解释。
- 自评结论：`EVIDENCE_SUFFICIENT`
- 用户验收状态：`已验收`；2026-09-20 用户确认“review 完成，开始干吧”，授权 [CAM-002 实现](CAM-002-edge-pan-follow.spec.md)。本结论只验收设计，不代表运行时已验证。

### Reflect 与调优

- 观察与证据：本轮复核确认 Dota 的普通镜头移动是 Edge Pan；原文虽标注边缘滚屏默认，却仍把 Camera Grip 写入本项目状态机和实现计划，容易误导实现。
- 根因类别：`单次实现`
- 调整文件与预期收益：更新 CAM-001 Spec、10-16、文档索引和进度台账，将 Camera Grip 降为外部参考/未来扩展，避免实现阶段新增不必要的抓取输入。
- 回归验证：本轮修改后重新运行 `python -B Tools/validate_docs.py`、`git diff --check`、plan/build/delivery Gate。
- 需要用户决定的问题：实现阶段的边缘阈值/速度/减速、相机边界、高度和 Space 跟随接管规则。
