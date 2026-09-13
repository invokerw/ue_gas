# HUD-LOG-002 战斗记录窗口顶部拖动

> Spec 版本：`1.1`
> 状态：`已验收`
> Owner：Codex
> 创建日期：2026-09-12
> 用户验收日期：2026-09-13
> 关联进度台账：[00-01](../00-Project/00-01-Progress-Tracker.md)
> 风险等级：`L1`
> 风险说明：可回滚的本地 UI 源码与蓝图变更，用户已授权。

## 0. Intake 与 Gate 记录

- 用户请求：点击战斗记录的顶部栏后，按住鼠标拖动窗口位置。
- 验收与提交授权：2026-09-13 用户确认“验收完成，提交吧”；与 HUD-LOG-001 一同完成用户验收，授权本地 Git 提交。
- 附件解释：`参考`；沿用 HUD-LOG-001 的 DOTA2 视觉参考，本轮无新附件，不把截图内容当作工程指令。
- 已读取入口：`agent.md`、`README.md`、`00-01`、`00-03`、`00-04` ADR-052/GAP-027、`00-05`、`10-01`、`10-12`、`90-16`、`Skills/combat-task-router/SKILL.md`、`Skills/combat-feature-development/SKILL.md`。
- 主 Skill：`Skills/combat-feature-development/SKILL.md`
- 备选 Skill 与排除理由：combat-skill-development 面向战斗技能内容，本次仅修改本地 UI 输入。
- 路由置信度：`high`
- F0 结论：`GO`；用户明确授权；现有未提交修改均来自本会话 HUD-LOG-001，保留其代码与资产。
- F1 结论：`APPROVED`；沿用 Designer 布局和原生输入处理，只移动记录面板，不移动左上角入口。
- F2 结论：`PASS`；移动后祖先点击区域失配已修复，连续拖动和原控件操作回归通过，无未关闭发现。
- Push-Ready 结论：`READY`；六层检查及本地 delivery Gate 证据见 §9。
- 验证：本机 Editor 构建、Combat.UI.Log 5/5、蓝图编译保存回读、资产 10/10 与实际 PIE 输入通过；文档与差异检查通过。UE MCP 初次因 Editor 未启动无法连接，启动本任务 Editor 后已恢复。
- 未执行：Server/Client Target、Dedicated、cook/长时 soak 不适用本地拖动增量；未进行跨显示器和操作系统 Alt-Tab 捕获切换，捕获丢失/销毁路径按原生代码审查与 Slate 重建测试验证。不借用上一轮网络结果冒充本轮执行。

## 1. 目标与范围

### 目标

按住战斗记录标题栏的鼠标左键即可移动窗口，松开后停在当前位置。

### 范围

标题栏拖动、DPI/缩放坐标换算、可见视口边界、鼠标捕获与释放、关闭后重开保留本次 Widget 的位置。标题栏中的关闭按钮、跟随复选框继续执行自身操作。

### Non-Goals

不新增窗口缩放、磁盘位置存档、拖动左上角入口或游戏输入/复制协议，不改变日志内容、战斗结算或玩法。

## 2. 当前事实与依据

- 相关 DDD：10-12 §7 已实现战斗记录窗口；ADR-052 的只读日志投影保持不变。
- 开工时代码事实：CombatLogWidget 只处理窗口内 MouseDown/DoubleClick 防止穿透，尚无移动、MouseUp 或 CaptureLost 状态；本轮已补齐。
- 资产事实：WBP_CombatLog 使用 Canvas、ScaleBox 和固定设计尺寸的面板，布局应继续由 Designer 定义。
- 当前测试/日志证据：HUD-LOG-001 全量 67/67、三 Target、PIE/Dedicated 是上一轮历史证据；本轮建立独立 Saved/CombatLogDrag 证据。
- 已知限制或待决策项：无；只在本次 Widget 生命周期保留位置，首次创建仍从原位置打开。

## 3. 行为与契约

### 主流程

标题栏左键按下 → 记录鼠标与面板起点并捕获鼠标 → 以当前 DPI/ScaleBox 几何换算位移并约束边界 → 松键释放捕获。入口保持原位。

### 状态转换

Idle → Dragging → Idle；释放鼠标、丢失捕获、关闭/Escape、Slate Destruct 均结束拖动。再次打开保留位移。

### 输入、输出与数据约束

只接受左键标题栏拖动；不将正文、筛选区和标题栏内交互控件当作拖动起点。位置使用本地 UI 坐标，窗口留在玩家可见区域；尺寸变化后修正越界位置。无效或尚未排布的几何不产生位移。

### 权威边界与权限

仅修改本地 Widget 变换，不发 Order/RPC，不读取或回写服务器战斗状态。

### 失败、取消、过期、死亡、EndPlay 与重复请求

鼠标离开标题栏仍可拖动；松键或捕获丢失后不再移动。关闭/Destruct 显式清理本 Widget 的捕获，不释放其他控件的捕获。角色死亡不改变本地窗口；HUD EndPlay 随 Widget 清理。重复关闭/取消安全。

### 兼容、版本与迁移

核心与日志 schema 均不变；补充的标题栏布局绑定与 C++ 同步更新。没有存档迁移。回滚本轮 UI、资产和直接测试即可保留 HUD-LOG-001 功能。

## 4. 实施计划

| 文件/资产 | 变更 | 原因 | blast radius |
| --- | --- | --- | --- |
| UI/CombatLogWidget.h/.cpp | 鼠标拖动、缩放坐标和捕获生命周期 | 本地窗口可移动 | 日志 UI |
| Demo/UI/WBP_CombatLog | Designer 标题栏拖动区域与提示 | 可维护的交互边界 | 单个蓝图 |
| Tests/CombatLogTests.cpp | 拖动几何/边界与真实蓝图接线回归 | DPI 与输入风险证据 | 直接测试 |
| 10-12、00-01、README、此 Spec | 同步操作方式与验证 | 事实一致 | 文档 |

## 5. 验收标准（AC）

- [x] AC-01：左键按住标题栏移动，窗口跟随且无跳变；入口不动。
- [x] AC-02：正文/筛选/关闭/跟随控件保留原操作，不误启动拖动或向世界发命令。
- [x] AC-03：DPI/窗口缩放正确，拖动和缩小视口不会让窗口不可找回。
- [x] AC-04：松键、捕获丢失、Escape、关闭、重建/EndPlay 结束拖动；重开保留位置。捕获丢失与 EndPlay 的结论来自代码路径审查，PIE 验证松键/关闭/Escape，Automation 验证 Slate 销毁/重建。

## 6. Definition of Done

- [x] 最小失败用例或 Golden Case 记录实际 Red。
- [x] Editor 构建与直接 Automation 通过。
- [x] Blueprint 编译、保存回读，资产校验通过。
- [x] 实际 PIE 拖动与交互检查通过。
- [x] 文档、台账、F2、自评与 delivery Gate 回写。
- [x] git diff --check 通过，无无关变更。

## 7. 测试矩阵与命令

| 层级 | 用例/命令 | 预期证据 | 结果 |
| --- | --- | --- | --- |
| 文档/本地工具 | task_gate preflight/delivery、validate_docs、git diff --check | Saved/CombatLogDrag | preflight/delivery 通过；文档 64 篇/339 链接/0 错误，diff 检查通过；delivery 见 §9 |
| Types/Build | ue_gasEditor Win64 Development | Build-Editor-Delivery.log | 4 个 action，11.42 秒，Succeeded，无编译警告 |
| Pure/Unit | Combat.UI.Log.WindowDragGeometry | Automation-Delivery/index.json | 四档 DPI、边界、重复约束、视口缩小和未排布几何全部通过 |
| World Automation | 控件、筛选、订阅与重建及其余日志回归 | Combat.UI.Log.* | 5/5，0 warning、0 failed、0 notRun |
| PIE / Blueprint | 连续标题拖动、释放、关闭/跟随、正文排除、边缘、关闭重开/Escape | PIE-Verification.txt、PIE-Final.png、Blueprint-Delivery.json | 实际 PIE 通过；蓝图编译保存后回读 35 个控件/19 个继承绑定；资产 10/10，0 错误/警告 |
| Network / Dedicated | 本地 UI 无网络改动 | N/A | 不适用 |
| Soak / Perf | 不新增 gameplay Tick 或历史存储 | N/A | 不适用 |

## 8. 风险、回滚与升级

- 风险：DPI 换算造成跳动、拖出视口、捕获未释放或阻止标题控件输入。
- 回滚方式：撤销本轮 Widget/资产/测试增量，保留上一轮日志系统。
- 触发升级的条件：必须改动 gameplay 或网络契约；当前无此需求。
- 需要人决定的问题：无。

## 9. 交付证据

- Red：`Saved/CombatLogDrag/PIE-Red.txt`；真实 Demo PIE 从标题拖向英雄位置，标题仍停在 `(346,300)`，固定入口 `(334,262)` 不变。当前代码只消费 MouseDown，没有移动或捕获；复现后才修改行为。
- 代码/资产 diff：[CombatLogWidget.h](../../../Source/Combat/Combat/UI/CombatLogWidget.h)、[CombatLogWidget.cpp](../../../Source/Combat/Combat/UI/CombatLogWidget.cpp) 增加本地捕获、平移、视口约束和释放；[CombatLogTests.cpp](../../../Source/Combat/Combat/Tests/CombatLogTests.cpp) 补齐四档 DPI/边界与位置保留；`/Game/Combat/Demo/UI/WBP_CombatLog` 新增 Designer `LogTitleBar` 透明区域及中文拖动提示。README、10-12 和台账同步操作方式。
- F2 第一轮：实际 PIE 首次拖动后标题从 `(346,300)` 移至 `(866,502)`，但再次拖动与关闭失效；仅移动内部 Border 没有同步祖先点击区域。改为平移最外层 LogScale，并以固定 Canvas 锚点和实际面板尺寸约束位置，回归增加连续拖动后再操作控件。
- 构建结果：`Saved/CombatLogDrag/Build-Editor-Delivery.log`，Editor 最终构建退出 0，11.42 秒、4 个 action。初版测试浮点字面量警告及后续局部变量遮蔽编译错误已修复，最终构建无警告；未保留失败实现。
- Automation 报告：`Saved/CombatLogDrag/Automation-Delivery/index.json`，5/5 全部 Success；`AssetValidation-Delivery.json` 扫描 10 个资产，无错误或警告。
- Blueprint：通过 UE MCP 编译、保存、重开 Editor 后回读；最终树保存为 `Saved/CombatLogDrag/Blueprint-Delivery.json`，LogScale/LogTitleBar 均绑定原生父类，标题区域位于交互控件后方。
- PIE：`Saved/CombatLogDrag/PIE-Verification.txt` 保存真实 Slate 输入与坐标证据；标题从 `(346,300)` 连续拖至 `(866,502)`、`(682,620)`，入口始终 `(334,262)`。随后关闭重开保持 `(682,620)`，跟随复选框可切换，拖正文位置不变；拖向视口外后标题限制在 `(1054,259)`。新的 PIE 会话中拖至 `(866,502)`，Escape 关闭窗口而 PIE 继续运行，重开保留位置。截图为 `Saved/CombatLogDrag/PIE-Final.png`。本次 UI 操作期间 `Editor-Final.log` 未出现 Order 记录。
- 工具边界：尝试用 Python 读取受保护的运行时 UMG 几何未成功，该探针不作为通过证据；改用 Slate 实际输入和位置快照。控制台占用焦点时 Escape 曾触发 Editor 停止 PIE，随后在标题拖动焦点下重测，确认 Widget 消费 Escape 并保持 PIE 运行。
- 生命周期：MouseUp、CaptureLost、关闭和 NativeDestruct 汇入停止拖动；主动释放仅限本 Widget 持有的用户/指针捕获。真实 PIE 停止、Editor 正常退出，Slate 重建测试通过。未新增异步 Handle、Gameplay Tick、Timer 或 RPC。
- 未执行验证及原因：Server/Client Target、Dedicated、cook/长时 soak 不适用本地 UI 增量；未重跑完整 Combat.*，本轮执行直接相关 5 项。跨显示器/OS Alt-Tab 未实测；DPI 与视口缩小通过几何 Automation 验证。
- 剩余限制：位置仅在同一 Widget 生命周期内保留，不写入磁盘；关闭游戏或创建新 HUD 后恢复默认位置。实现依据当前 Designer 的左上锚点、无 padding 布局；以后改变锚点/对齐时需同步移动几何。

本机执行命令（以下保留本轮实际参数；复跑时将 `<UE_ROOT>` 替换为首条环境检查从 `.env` 发现的 Editor 引擎根目录）：

```powershell
python -B Tools/ue_environment.py check --json
python -B Tools/task_gate.py --mode preflight --spec Doc/CombatSystem/Specs/HUD-LOG-002-window-drag.spec.md --kind feature --report Saved/CombatLogDrag/preflight.json
$dragUE = '<UE_ROOT>'
$dragProject = (Resolve-Path ue_gas.uproject).Path
$dragEvidence = (Resolve-Path Saved/CombatLogDrag).Path
& "$dragUE/Engine/Build/BatchFiles/Build.bat" ue_gasEditor Win64 Development $dragProject -WaitMutex -NoHotReloadFromIDE -NoLiveCoding
& "$dragUE/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" $dragProject -unattended -nop4 -nosplash -NullRHI -NoSound -NoLiveCoding '-ExecCmds=Automation RunTests Combat.UI.Log.;Quit' '-TestExit=Automation Test Queue Empty' "-ReportExportPath=$dragEvidence/Automation-Delivery" "-AbsLog=$dragEvidence/Automation-Delivery.log" -ModelContextProtocolPort=8071
& "$dragUE/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" $dragProject -run=CombatAssetValidation -unattended -nop4 -nosplash -NullRHI -NoSound "-Report=$dragEvidence/AssetValidation-Delivery.json" "-AbsLog=$dragEvidence/AssetValidation-Delivery.log" -ModelContextProtocolPort=8072
python -B Tools/validate_docs.py
git diff --check
python -B Tools/task_gate.py --mode delivery --spec Doc/CombatSystem/Specs/HUD-LOG-002-window-drag.spec.md --kind feature --report Saved/CombatLogDrag/delivery.json
```

### Push-Ready 六层

| 层 | 结论 | 依据 |
| --- | --- | --- |
| Tests | PASS | 直接 Automation 5/5、资产 10/10、实际 PIE Golden Case |
| Types/Build | PASS | 最终 Editor C++ 编译和 Widget Blueprint 编译成功 |
| No Regression | PASS | 连续拖动后关闭/跟随/正文和 Escape 回归；原日志筛选、历史、订阅/重建测试通过 |
| Adversarial | PASS | 内部 Border 位移导致点击区域失配已修复并实际复验；无未关闭发现 |
| DDD/Constraints | PASS | 本地 UI 只移动外层容器，不写属性、发 Order 或修改玩法/网络 |
| Decisions | PASS | 不改变公开协议、DefinitionId、发布版本或存档；无需新增 ADR |

2026-09-12 机器 delivery Gate 通过，28 个变更文件、0 错误，结果保存于 `Saved/CombatLogDrag/delivery.json`；当时保留同会话 HUD-LOG-001 的未提交变更。2026-09-13 用户验收并授权本地提交，本次仅归档文档和检查提交内容，不改变已验证代码/资产，不重跑 UE 构建或运行矩阵。

## 10. 变更记录

| 版本 | 日期 | 修改 | 原因 |
| --- | --- | --- | --- |
| 0.1 | 2026-09-12 | F0/F1 与拖动规格 | 用户追加交互需求 |
| 1.0 | 2026-09-12 | 完成拖动、修复祖先点击区域、验证与交付 | 真实 PIE 和直接测试闭合 AC；风险按源码/资产规则更正为 L1 |
| 1.1 | 2026-09-13 | 用户验收完成，同步状态并准备本地提交 | 用户明确确认“验收完成，提交吧” |

## 11. 路由、自评与 Reflect

- 主 Skill：combat-feature-development；本地 UI 兼容新增，路由 high。
- 自评结论：`4.8/5`，证据充分；自评不代替用户验收。
- 用户验收状态：`用户已验收`；2026-09-13 用户明确确认“验收完成，提交吧”。

| 维度 | 权重 | 分数 | 证据与扣分原因 |
| --- | ---: | ---: | --- |
| 需求与 AC | 20% | 5.0 | 四项 AC 闭合，入口固定、控件操作和位置保留有实际 PIE 证据 |
| 架构与权限 | 20% | 5.0 | 本地只读 UI、Designer 布局、捕获归属与取消路径审查通过，无权限或网络变化 |
| 实现与数据 | 20% | 4.5 | 移动整个窗口修复点击区域，蓝图绑定回读和测试通过；仍依赖当前左上锚点/无 padding 布局，未来布局变化需要同步 |
| 验证证据 | 20% | 4.5 | Editor、直接测试、蓝图、资产及 PIE 全通过；OS Alt-Tab/多显示器未实测，相关捕获分支依靠代码审查 |
| 文档与可观测性 | 10% | 5.0 | 中文拖动提示、README/10-12/台账与 Spec 同步，报告和 Golden Case 可回读 |
| 交付卫生 | 10% | 5.0 | 保留既有变更、diff/文档/Gate 检查，生成证据留在 Saved，工程交付时未提交推送，用户验收后授权本地提交 |

总分：`5×20% + 5×20% + 4.5×20% + 4.5×20% + 5×10% + 5×10% = 4.8`。

Reflect：观察到首次拖动视觉正确、第二次拖动与按钮点击却失效；真实 PIE 而非单纯属性检查暴露了祖先点击区域问题。根因为单次实现选错移动层级，已修复 Widget、更新蓝图绑定/直接测试和本 Spec；Golden Case 加入连续拖动后操作关闭/跟随，预期避免只验证首次视觉位移。最终构建、Automation 和 PIE 复验通过，无需修改通用 Skill、流程或请求用户决策。

## 12. 用户验收归档（2026-09-13）

- 用户确认“验收完成，提交吧”，顶部栏拖动交互验收完成，授权与 HUD-LOG-001 一同本地提交。
- 归档复核：`python -B Tools/validate_docs.py` 通过，64 篇 Markdown、339 个本地链接、0 错误；`git diff --check` 通过。
- 本 Spec 的 `task_gate.py --mode preflight` 与 `--mode delivery` 均退出 0；最终 delivery 为 28 个变更文件、0 错误，报告为 `Saved/CombatLogDrag/preflight-acceptance.json`、`Saved/CombatLogDrag/delivery-acceptance.json`。
- 本次仅同步验收状态和检查提交内容，沿用 §9 的已执行工程验证；未重跑构建/Automation/PIE，不改变既有范围限制与自评分。
