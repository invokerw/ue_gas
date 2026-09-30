# HUD-MINIMAP-001 HUD 左下角小地图设计

> Spec 版本：`0.2`
> 状态：`COMPLETED`
> Owner：Codex
> 创建日期：2026-09-27
> 关联进度台账：`Doc/CombatSystem/00-Project/00-01-Progress-Tracker.md` §12.12
> 风险等级：`L0`

## 0. Intake 与 Gate 记录

- 用户请求：在现有底部 HUD 左下角增加小地图，并在确认视觉方向后完善仓库文档。
- 附件解释：`无附件`；上一轮小地图 HUD 预览是本次设计参考，不是运行时资产或行为证据。
- 已读取入口：`agent.md`、`README.md`、`Doc/CombatSystem/README.md`、`Doc/CombatSystem/00-Project/00-01-Progress-Tracker.md`、`Doc/CombatSystem/00-Project/00-04-Decisions-Gaps.md`、`Doc/CombatSystem/00-Project/00-05-AI-Native-Development-Workflow.md`、`Doc/CombatSystem/10-Architecture/10-01-Scope-Architecture.md`、`Doc/CombatSystem/10-Architecture/10-09-Client-Server-Interaction.md`、`Doc/CombatSystem/10-Architecture/10-12-Bottom-HUD-Design.md`、`Doc/CombatSystem/10-Architecture/10-16-Dota-Camera-Movement.md`、`Skills/combat-task-router/SKILL.md`、`Skills/combat-feature-development/SKILL.md`。
- 主 Skill：`Skills/combat-feature-development/SKILL.md`（文档交付需要 Spec、事实核对、索引、台账和 Gate）。
- 备选 Skill 与排除理由：`combat-task-router` 负责路由与复盘，已作为入口使用；`combat-skill-development` 面向 GAS/Combat 技能，不适用于 HUD 设计文档。
- 路由置信度：`high`
- F0 结论：`GO`
- F0 依据：用户明确授权将已确认的视觉方案落入文档；本轮只修改 Markdown，不改变 C++、蓝图、DataAsset、地图、网络载荷、事件 schema 或发布契约。
- F1 结论：`APPROVED`
- F1 审查人：Codex（L0 文档计划自审）
- F1 审查版本：`0.2`
- F1 计划审查证据：2026-09-27，0.2 docs plan Gate 通过（0 error）。复审确认六份 Markdown 文件范围、AC-01–06、HUD/相机/Order/可见性事实、文档校验与回滚边界一致；根目录 `README.md` 当时明确“设计提案”状态。本 Spec 只交付设计输入，运行时实现与新证据转入 HUD-MINIMAP-002。
- Build 解锁：`已解锁`；运行同 Spec 的 0.2 docs Build Gate。本任务不进入运行时 BUILD。
- F1 重审条件：新增代码、资产或网络契约；改变可见性、Order、相机或生命周期边界；扩大为实际小地图实现；或修改测试矩阵/回滚策略时递增 Spec 版本并重新审查。
- F2 结论：`PASS`
- F2 审查人：Codex（L0 文档对抗复核）
- F2 证据：2026-09-27，0.2 复审后逐项核对 10-12、README、台账、ADR-068 与本 Spec；确认 Fog 仅为预留视觉、不声称战争迷雾保密，Ping 未获准复制，右键只进入公共 Move Order，旧 generation/EndPlay 清理边界完整。
- Push-Ready 结论：`READY`
- 验证：0.2 preflight/plan/build、文档校验和差异检查通过；0.2 delivery Gate 结果写入 Saved/HUDMinimap/v0.2-delivery.json。
- 未执行：UE 构建、Automation、PIE、Dedicated、cook/打包和性能测试；本任务没有运行时或资产修改。

## 1. 目标与范围

### 目标

将左下角小地图的布局、视觉语义、交互意图、数据/权威边界、生命周期和后续验收要求写入当前 HUD 专题，使后续实现可以直接按文档评审，不把预览中的示例值误当作已实现能力。

### 范围

- 在 `10-12-Bottom-HUD-Design.md` 增加小地图设计章节，记录标准/紧凑尺寸、底边对齐、安全区、图层和标记语义。
- 定义左键定位镜头、右键复用现有群体 Move Order、Alt+左键 Ping 的交互意图，并标明哪些入口仍需后续实现决策。
- 明确静态地图层、动态单位层、目标层、镜头视锥和可见性/战争迷雾的边界。
- 同步 `Doc/CombatSystem/README.md`、`00-01-Progress-Tracker.md` 和 `00-04-Decisions-Gaps.md`，保留可回读的 Spec、台账和 proposed ADR。
- 建立后续实现的 UMG/C++ 分工、生命周期清理、DPI/窄视口和验证矩阵。

### Non-Goals

- 不创建或修改 `WBP_CombatMinimap`、C++ Widget、DataAsset、纹理、地图、输入映射、GameplayTag、事件 schema、RPC 或发布契约。
- 不声称当前已经具备战争迷雾、敌方位置保密、队友 Ping 复制、地图投影或小地图运行时能力。
- 不新增服务器 gameplay 权威、单位移动旁路、相机 Pawn、Order 类型或独立计时器。
- 不把示意预览中的地形、坐标、单位、目标和颜色数值写入正式战斗定义。

## 2. 当前事实与依据

- `10-12` 已冻结底部 HUD 的底部居中布局、`ACombatPlayerHUD` 持有 Widget、`UCombatHUDWidget` 负责只读适配、Widget Blueprint 负责视觉，以及 `BindingRevision + LifeGeneration` 的旧回调隔离。
- `10-16` 已冻结本地 Edge Pan 与按住 Space 跟随 `CommandedUnit` 的相机语义；小地图左键定位必须复用本地相机表现链路，不写入 Unit Transform。
- `10-09`、`10-10` 和 `10-01` 要求客户端输入只是请求，服务器通过公共 Order/Targeting 入口复核；右键小地图移动不能直接修改单位位置。
- ADR-067 已明确当前没有战争迷雾能力，不能把公开查看或客户端 UI 描述为网络层的敌情保密；小地图设计必须保留这一限制。
- HUD 当前拥有者与公开查看分为不同投影；小地图标记应消费公开可见性/单位 View 事实，不能把技能 Spec、物品实例、冷却、AutoCast 或其他 owner-only 字段下发给非拥有者。
- 当前工作区没有 `.codegraph/` 索引；本任务使用现有文档、源码定位结果和 `rg`，不创建索引。

## 3. 行为与契约

### 主流程

1. 本地 `ACombatPlayerHUD` 创建主 HUD 时创建或持有小地图 Widget；专用服务器不创建。
2. 静态地图层从后续批准的地图边界/纹理配置加载一次；动态标记层消费已经复制到客户端的公开单位 View、拥有者主控状态和允许显示的目标/事件。
3. 小地图把世界 XY 投影为归一化坐标，UMG 只绘制标记和镜头视锥；展示刷新不能执行伤害、移动、资源修改或其他 gameplay。
4. 左键地图坐标只请求本地相机定位；右键地图坐标转换为现有选中组 Move Order 请求，由 Controller/服务器沿既有安全链路复核；Alt+左键 Ping 在实现前必须先确定是本地表现还是新增的可复制事件，文档不预授权网络事件。
5. 换 `CommandedUnit`、切换查看目标、绑定代次变化、Unit EndPlay 或 HUD 重建时清理旧标记/异步加载回调，并以新绑定重新投影。

### 状态转换

| 状态 | 进入 | 可观察结果 | 退出 |
| --- | --- | --- | --- |
| `Hidden` | 专用服务器、HUD 不可见、缺少地图配置 | 不创建或不绘制小地图，不发送 gameplay 请求 | 本地 HUD 就绪且配置有效 → `Visible` |
| `Visible` | 本地 HUD 创建完成且地图配置有效 | 显示静态地图、可见标记和当前镜头视锥 | HUD 隐藏/EndPlay/地图配置失效 → `Hidden` |
| `Visible + Rebinding` | `CommandedUnit`、查看目标或绑定代次变化 | 清掉旧主控/镜头标记，保留静态层，等待新投影 | 新绑定有效 → `Visible`；目标无效 → `Visible` 且无主控标记 |
| `Visible + PendingPing` | Alt+左键进入待定 Ping 交互 | 只显示本地候选反馈；不直接改战斗状态 | 取消、超时或 Ping 入口获批准后完成 |

### 输入、输出与数据约束

| 输入 | 设计语义 | 约束 |
| --- | --- | --- |
| 小地图左键 | 定位本地镜头到地图坐标 | 不发 Unit Order，不改变 `CommandedUnit`，不写服务器状态 |
| 小地图右键 | 沿既有公共 Move Order 给当前选中组下达移动 | 只提交意图；服务器复核 Owner、RequestId、载荷、生命状态和目标合法性 |
| `Alt +` 小地图左键 | 发送 Ping 的设计预留 | 在新增可复制 Ping 事件前不得宣称多人可见；本地预览可使用瞬态反馈 |
| 悬停/聚焦标记 | 显示名称、类别、可见性和时间等最小提示 | 不暴露 owner-only Spec、实例修订、冷却、AutoCast 或库存字段 |

### 权威边界与权限

- 静态地形和归一化投影是表现配置；它们不能成为移动、目标或可见性的第二事实源。
- 单位标记只使用客户端已拥有且允许展示的 View/事件事实。当前没有战争迷雾能力时，文档和 UI 不得承诺敌方位置保密；后续接入视野服务时必须单独建 Spec 并定义服务器输出。
- 右键移动复用现有 `ACombatPlayerController` 选中组入口和公共 Order；小地图不能调用 `SetActorLocation`、PathFollowing 或私有移动旁路。
- 小地图不复制完整 owner-only HUD 快照；非拥有者不因看到地图而获得技能、物品或经济私有字段。

### 失败、取消、过期、死亡、EndPlay 与重复请求

- 地图配置、单位 View、Owner 或绑定代次未就绪：静态层可保留，动态层显示空状态；不发送相机外的 gameplay 请求。
- 右键提交失败、权限变化、RequestId 重放或选中组在请求前失效：由现有 Order 失败回执处理；小地图清除本地候选反馈，不重试旧请求。
- Unit 死亡/重生/EndPlay：只更新允许公开的标记状态；不借小地图触发复活、追击、停止或镜头强制回中。
- HUD 重建、视口缩放和地图纹理异步加载必须校验 `BindingRevision + LifeGeneration`，旧回调不能写回新 Widget/新单位。
- 多次左键、右键或 Ping 事件按本地输入会话和现有 RequestId 规则幂等处理；不增加第二个终结入口。

### 兼容、版本与迁移

- 本 Spec 只记录设计，不修改 `combat_v4_economy_rc1`、GameplayTag、事件/展示 schema、DefinitionId、RPC 或资产版本。
- 后续实现优先复用现有 `ACombatPlayerHUD`/`UCombatHUDWidget` 生命周期和 `10-16` 相机入口，不新增 Camera Pawn 或独立小地图 Actor。
- 实现阶段若新增地图纹理、边界 DataAsset、UMG 控件或公开 View 字段，必须另建实现 Spec，重新审查网络/资产/生命周期 blast radius，并补迁移与回滚。

## 4. 实施计划

| 文件/资产 | 变更 | 原因 | blast radius |
| --- | --- | --- | --- |
| `Doc/CombatSystem/10-Architecture/10-12-Bottom-HUD-Design.md` | 增加小地图设计、交互、数据边界、生命周期和后续验证 | 维护 HUD 唯一设计入口 | L0 文档 |
| `README.md` | 设计阶段标明小地图为提案并链接 HUD 专题；实现阶段由 HUD-MINIMAP-002 更新为已接入状态 | 保留设计阶段边界，同时避免根入口与实现状态脱节 | L0 文档 |
| `Doc/CombatSystem/README.md` | 增加 HUD-MINIMAP-001 Spec 导航 | 让设计任务可发现 | L0 文档 |
| `Doc/CombatSystem/00-Project/00-01-Progress-Tracker.md` | 增加 §12.12 设计任务和更新日志 | 保持任务状态唯一来源 | L0 文档 |
| `Doc/CombatSystem/00-Project/00-04-Decisions-Gaps.md` | 增加 ADR-068 proposed 设计决策 | 固化视觉/交互/权威边界，区分后续实现 | L0 文档 |
| `Doc/CombatSystem/Specs/HUD-MINIMAP-001-minimap-design.spec.md` | 记录 F0/F1/F2、AC、证据和未执行项 | 可回读的交付契约 | 本任务 |

本轮顺序为：完成 Spec 与计划审查 → 修改六份 Markdown → 文档事实/链接/空白校验 → F2 对抗复核 → delivery Gate。不得修改 `Source/`、`Content/`、`Tools/` 或运行时配置。

## 5. 验收标准（AC）

- [x] AC-01：`10-12` 明确小地图 220×220 标准版、184×184 紧凑版、左/底安全边距、与底部 HUD 的对齐和窄视口策略。
- [x] AC-02：`10-12` 明确静态地形、道路/河道、友军/敌方/主控/目标/Ping/镜头视锥的图层和形状/颜色语义，并标注示意值不属于战斗定义。
- [x] AC-03：文档明确左键镜头、右键公共 Move Order、Alt+左键 Ping 的权限边界，且不声称已有战争迷雾或 Ping 复制；运行时实现现状与证据由 HUD-MINIMAP-002 单独记录。
- [x] AC-04：文档覆盖 HUD 创建/重建、目标切换、异步加载、EndPlay、请求失败/重复输入和旧 generation 隔离。
- [x] AC-05：`README`、Spec、进度台账和 ADR-068 互相可导航；不修改代码、资产或配置，不混入生成文件。
- [x] AC-06：`validate_docs.py`、`git diff --check`、docs preflight/plan/build/delivery Gate 的结果写回 Spec；UE 构建、Automation、PIE、Dedicated 和 cook 明确记为不适用/未执行。

## 6. Definition of Done

- [x] 文档中的当前事实、设计提案和未实现范围分开标记；不将预览或计划写成运行时通过证据。
- [x] `10-12`、`README`、`00-01`、`00-04` 和本 Spec 的路径/锚点互相可达。
- [x] `python -B Tools/validate_docs.py` 与 `git diff --check` 通过；共享工作区既有差异若影响 delivery，单独记录而不归因于本任务。
- [x] docs preflight、plan、build、delivery Gate 结果写回本 Spec；F2 与 Push-Ready 只反映文档范围。
- [x] 用户审核完成后将本设计 Spec 标记为 `COMPLETED`；运行时实现、PIE 和联机证据仍由独立 feature Spec 管理。

## 7. 测试矩阵与命令

| 层级 | 用例/命令 | 预期证据 | 结果 |
| --- | --- | --- | --- |
| 任务入口 | `python -B Tools/task_gate.py --mode preflight --spec Doc/CombatSystem/Specs/HUD-MINIMAP-001-minimap-design.spec.md --kind docs --report Saved/HUDMinimap/v0.2-preflight.json` | F0、路由、Spec 和状态有效 | PASS，0 error |
| 计划 | `python -B Tools/task_gate.py --mode plan --spec Doc/CombatSystem/Specs/HUD-MINIMAP-001-minimap-design.spec.md --kind docs --report Saved/HUDMinimap/v0.2-plan.json` | 计划章节完整、可审查 | PASS，0 error |
| 文档准入 | `python -B Tools/task_gate.py --mode build --spec Doc/CombatSystem/Specs/HUD-MINIMAP-001-minimap-design.spec.md --kind docs --report Saved/HUDMinimap/v0.2-build.json` | F1 与当前版本一致 | PASS，0 error |
| 文档/链接 | `python -B Tools/validate_docs.py --report Saved/HUDMinimap/docs.json` | Markdown、编号、路径和本地链接无错误 | PASS，90 Markdown / 491 本地链接 / 0 error |
| 差异 | `git diff --check`、工作区变更核对 | 无本任务新增空白/生成文件/越界文件 | PASS；仅报告 Git 的 LF/CRLF 提示 |
| 交付 | `python -B Tools/task_gate.py --mode delivery --spec Doc/CombatSystem/Specs/HUD-MINIMAP-001-minimap-design.spec.md --kind docs --report Saved/HUDMinimap/v0.2-delivery.json` | F2、Push-Ready、验证/未执行记录完整 | PASS，0 error / 6 changed files |
| UE 构建/Automation/蓝图/PIE/Dedicated/Soak | 本任务不改运行时、资产或配置 | 不把文档证据冒充运行证据 | N/A，未执行 |

## 8. 风险、回滚与升级

- 风险：把预览的 Fog、动态标记或 Ping 误读为已有网络能力；右键小地图可能绕过公共 Order；在 HUD 重建/目标切换时旧异步回调污染新 Widget；标准尺寸在窄视口遮挡底部 HUD。
- 回滚方式：只撤销本 Spec、`10-12` 小地图章节、README 索引、台账条目和 ADR-068；不触碰既有 HUD、相机、控制和物品文档结论。
- 触发升级的条件：需要新增可复制 Ping、战争迷雾/视野服务、地图 DataAsset/纹理、公开 View 字段、服务器投影、Input Action、UMG/C++ 类或真实 PIE/Dedicated 验收时，新建实现 Spec 并重新审查。
- 需要人决定的问题：后续地图边界/纹理来源、Ping 是否扩展为多人可见、敌方标记的权威可见性来源、地图更新频率和窄视口最小尺寸。

## 9. 交付证据

- 代码/资产 diff：无；本任务只修改 `README.md`、`Doc/CombatSystem/README.md`、`10-12-Bottom-HUD-Design.md`、`00-01-Progress-Tracker.md`、`00-04-Decisions-Gaps.md` 和本 Spec 六份 Markdown 文件。
- 构建结果：未执行；纯文档任务不适用。
- Automation/PIE/Dedicated 报告：未执行；纯文档任务不适用。
- 文档和 Gate 证据：0.2 preflight/plan/build、`validate_docs.py`、`git diff --check` 和 delivery Gate 结果已写回本节 §7；delivery 报告为 0 error / 6 changed files。
- 剩余风险：设计文档已审核；运行时体验仍按 HUD-MINIMAP-002 的真实窗口验收边界执行，后续地图纹理/联网能力需另建 Spec。

## 10. 变更记录

| 版本 | 日期 | 修改 | 原因 |
| --- | --- | --- | --- |
| 0.1 | 2026-09-27 | 建立小地图设计 Spec，冻结文档范围、交互意图和实现边界 | 用户确认视觉方向并要求完善文档 |
| 0.2 | 2026-09-27 | 同步 HUD 专题、根 README、台账和 ADR-068；纳入六份 Markdown 文件并完成 0.2 计划复审 | 修正文件范围记录，保持 F1 审查版本与实际 diff 一致 |
| 0.2 | 2026-09-29 | 用户审核设计并授权进入实现；设计 Spec 完成，运行时代码和验证转入 HUD-MINIMAP-002 | 保留设计与实现证据分离 |

## 11. 路由、自评与 Reflect

### 路由记录

- 原始需求摘要：在已有底部 HUD 左下角设计小地图，并把已确认设计落入仓库文档。
- 主 Skill：`combat-feature-development`
- 选择依据：本任务交付仓库内 HUD 专题、Spec、索引、台账和 ADR 文档，需要文档 Gate；不实现 GAS 技能或运行时行为。
- 备选 Skill 与排除理由：`combat-task-router` 负责路由与复盘；`combat-skill-development` 与本任务对象不符。
- 路由置信度：`high`

### 交付自评

| 维度 | 权重 | 分数（0–5） | 证据与扣分原因 |
| --- | ---: | ---: | --- |
| 需求与 AC | 20% | 5.0 | AC-01–06 全部完成；尺寸、图层、交互、边界和后续验收可回读 |
| 架构与权限 | 20% | 4.8 | 保持 HUD/相机/Order/公开 View 与服务器权威边界；Ping/战争迷雾明确留后续 |
| 实现与数据 | 20% | 4.5 | 给出 UMG/C++、投影、生命周期和升级入口；本轮没有运行时实现证据 |
| 验证证据 | 20% | 4.5 | 0.2 preflight/plan/build/delivery、90 Markdown/491 链接文档校验和 diff 检查通过；UE 运行验证不适用 |
| 文档与可观测性 | 10% | 5.0 | 10-12、README、台账、ADR-068、Spec 互相导航并区分事实/提案 |
| 交付卫生 | 10% | 4.8 | 变更限定六份 Markdown，未混入代码、资产或生成文件 |

- 计算总分：`4.7 / 5.0`
- 硬性封顶或未执行项：UE/Automation/PIE/Dedicated/Soak 因纯文档范围不适用；不得计为通过。
- 自评结论：`EVIDENCE_SUFFICIENT`
- 用户验收状态：`ACCEPTED`（2026-09-29；运行体验验收见 HUD-MINIMAP-002）

### Reflect 与调优

- 观察与证据：预览方案已经明确视觉语言，但若不补写“当前无战争迷雾/Ping 复制能力”，实现者容易把示意标记误读为网络事实；10-12、ADR-068 和本 Spec 已明确该边界。
- 根因类别：`单次文档实现`
- 调整文件与预期收益：同步 10-12、README、台账、ADR-068 和 Spec，让视觉、交互、权限和生命周期在一个可导航链路中闭合。
- 回归验证：preflight/plan/build/delivery 均通过；文档校验 90/491/0 与 `git diff --check` 通过。
- 需要用户决定的问题：实现阶段的地图数据来源、Ping 复制范围、可见性服务和最小尺寸。
