# HUD-LOG-001 战斗记录窗口

> Spec 版本：`0.3`
> 状态：`已验收`
> Owner：Codex
> 创建日期：2026-09-12
> 用户验收日期：2026-09-13
> 关联进度台账：[00-01](../00-Project/00-01-Progress-Tracker.md)
> 风险等级：`L1`

## 0. Intake 与 Gate 记录

- 用户请求：参考 DOTA2 截图制作战斗日志记录系统，在 HUD 左上角添加入口按钮。
- 验收与提交授权：2026-09-13 用户确认“验收完成，提交吧”；战斗记录系统及 HUD-LOG-002 拖动交互通过用户验收，授权合并为一次本地 Git 提交。
- 附件解释：`参考`；图片定义时间戳、彩色文字、可滚动历史、攻击者/目标选择、伤害/治疗/技能/物品/状态/非英雄筛选和时间范围。图片中的英雄、技能、数值不是本项目配置或执行指令。
- 已读取入口：`agent.md`、`README.md`、`00-01`、`00-03`、`00-04`、`00-05`、`10-01`、`10-08`、`10-12`、`90-16`、`Skills/combat-task-router/SKILL.md`、`Skills/combat-feature-development/SKILL.md`。
- 主 Skill：`Skills/combat-feature-development/SKILL.md`。
- 备选 Skill 与排除理由：combat-skill-development 负责技能内容，本任务是只读 UI 与事件投影；不适用。
- 路由置信度：`high`
- F0 结论：`GO`。
- F0 依据：用户已授权实现；工作区干净；已有权威 Combat Event、UMG HUD、FastArray 模式和可用 UE MCP。
- F1 结论：`APPROVED`；采用现有事件的独立显示投影，核心事件 schema 与结算保持兼容。
- F2 结论：`PASS`；最终接口、事件时序、生命周期、只读网络边界和 Blueprint 集成已复核；保留标签重名的最后发现已修复并通过完整回归，见 §9。
- Push-Ready 结论：`READY`；Editor、源码 Client 和最终 Server 构建及全部适用运行验证均已通过，六层证据见 §9。
- 验证：开工 Gate 已通过（Saved/CombatLog/preflight.json）；本机 UE 环境检查 Editor/Dedicated 均 READY。2026-09-12 TDD Red：新增 CombatLogTests.cpp 后 Editor 构建报 C1083，缺少计划新增的 Combat/Log/CombatLogTypes.h；随后进入实现。最终三 Target 构建、冷启动全量 Combat 67/67、资产 10/10、PIE 和 Dedicated 双客户端已通过；delivery 机器检查 0 错误、27 个变更文件，见 §9。
- 未执行：cook/打包与长时 soak 不属于本次增量范围；无受阻的必需验证项。

## 1. 目标与范围

### 目标

玩家从左上角“战斗记录”按钮打开可筛选的实时中文战斗历史，在单机和联机客户端均消费服务器结果。

### 范围

记录伤害、治疗、成功施法/中断/自动施法切换、Modifier 施加/移除、死亡/复活。显示毫秒时间戳、来源/目标/效果名称、真实变化量；Damage/Heal 显示事务实际生命前后值。攻击者/目标按独立实例过滤，类别与时间窗可组合，窗口关闭时继续保留记录。

### Non-Goals

不实现物品/经济、磁盘永久存档、录像回放、日志反向施法或战斗数据修改。物品选项置灰并解释当前项目未接入。当前“英雄”沿用玩家指挥单位身份；非英雄开关控制双方均非玩家单位的记录，不建立新玩法分类。

## 2. 当前事实与依据

- 相关 DDD：10-08 §7、10-12、ADR-039/047；新增 ADR-052 记录显示投影契约。
- 代码事实：`UCombatEventSubsystem::Emit` 维护 512 条服务器结构化事件，未复制给客户端；`FCombatTransactionDelta` 已有 PreviousHealth/NewHealth；`ACombatPlayerHUD` 创建底部 HUD。
- 当前测试/日志证据：最近 REF-002 台账全量 Combat 63/63 是历史证据，本次已扩展为 67 项。
- 已知限制或待决策项：本地示例没有独立英雄分类或物品系统；不猜测图片里的玩法配置。

## 3. 行为与契约

### 主流程

服务器 Emit → 临时只读资源变化附加信息 → PlayerController 的日志组件筛选网络相关单位并追加有界 FastArray → owning client 接收 → Widget 按条件生成彩色行。

### 状态转换

入口按钮：关闭 ↔ 打开；关闭/重建不删除组件历史。Escape 或关闭按钮收起窗口。打开时定位最新记录；手动上滚暂停自动跟随，勾选跟随恢复。

### 输入、输出与数据约束

每连接最多保留 512 条，保持服务器提交 Sequence；记录只含稳定定义 ID、服务器不透明实例 ID、时间、类别、实际数值与显示标志，无 Runtime/DataAsset UObject 指针。默认 30 秒，可选 60/120/300 秒及全部保留记录。全类别勾选；0 治疗不进入玩家日志，诊断事件仍照常存在。名称中的富文本字符转义；缺资产回退稳定 ID。同名实例可区分。

### 权威边界与权限

无客户端写 gameplay 或上传日志 RPC。组件只在服务器订阅事件；只向自身 owning connection 复制来源和目标均网络相关的事件。专用服务器不创建 Widget。

### 失败、取消、过期、死亡、EndPlay 与重复请求

重复构建/绑定先解绑；FastArray 完整接收后通知；历史记录跨死亡保留且生命前后值冻结。无效数值丢弃显示。组件 EndPlay 解绑事件并清理；Widget Destruct 解绑组件并取消异步加载，旧加载回调校验修订号。历史超限淘汰最旧条目；同序号不重复。没有日志时显示空态，不生成模拟战斗。

### 兼容、版本与迁移

新增独立 CombatLogPresentation schema 1；核心 combat_v1_rc1、FCombatLogRecord schema 1、现有 View schema 保持不变。Emit 新增可省略的原生展示上下文，只在同步回调中使用，不进入核心序列化。PC 添加原生日志组件；新 WBP_CombatLog 由 HUD 配置创建。新客户端/服务器需同时更新。撤销时成组移除投影组件和 Widget 引用即可，无存档迁移。

## 4. 实施计划

| 文件/资产 | 变更 | 原因 | blast radius |
| --- | --- | --- | --- |
| Log/CombatEventSubsystem、CombatDamage/HealSubsystem | 同步转交实际资源变化给显示观察者 | 精确前后生命，保留核心 schema | 原生只读事件扩展 |
| Log/CombatLogTypes、CombatLogComponent | 有界 owner-only 日志投影、分类/筛选 | 客户端可回看真实历史 | PC 新增组件与复制 |
| UI/CombatLogWidget、CombatPlayerHUD | 控件绑定、行展示、开关和清理 | 左上角入口与筛选窗口 | 本地 UMG |
| Demo/UI/WBP_CombatLog、BP_CombatPlayerHUD | Designer 布局与配置 | 视觉继续由蓝图维护 | Demo 资产 |
| Config/DefaultGame.ini | 将 Demo Modifier/Projectile 纳入现有 PrimaryAsset 扫描 | 客户端由稳定 ID 解析已有中文效果名 | 资产发现与 cook 规则补齐，无定义或数值变更 |
| Tests/CombatLogTests | 分类/筛选/事务/重复/生命周期/Widget | 直接可执行证据 | 验证设施 |
| 10-08、10-12、00-04、00-01、README | 行为和证据同步 | 文档事实一致 | 文档 |

## 5. 验收标准（AC）

- [x] AC-01：左上角按钮可开关窗口，Escape 关闭；窗口内点击不发移动或攻击。
- [x] AC-02：真实 Damage/Heal/Ability/Modifier/死亡事件产生中文彩色时间戳行；数值来自服务器事务。
- [x] AC-03：来源、目标、类别、非英雄与时间窗组合筛选；滚动与跟随最新可用，关闭仍记录。
- [x] AC-04：有界缓存、同名实例区分、缺资产、异常数值、重建/EndPlay 安全。
- [x] AC-05：Blueprint 编译保存回读、Editor 构建、相关 Automation、资产与联机验证有实际证据。

## 6. Definition of Done

- [x] 最小失败用例/Golden Case 与实际 Red 原因已记录。
- [x] 直接测试通过，未绕过公共权威入口。
- [x] Editor/蓝图/资产编译、保存并回读。
- [x] 适用 Editor、Server/Client、PIE、Dedicated 验证完成。
- [x] DDD、ADR、生命周期与中文说明同步。
- [x] git diff --check 通过，未混入用户修改或生成文件。

## 7. 测试矩阵与命令

| 层级 | 用例/命令 | 预期证据 | 结果 |
| --- | --- | --- | --- |
| 文档/本地工具 | task_gate preflight/delivery、validate_docs、git diff --check | 本地 Gate 输出 | preflight/delivery、63 Markdown/332 本地链接和空白检查通过；delivery 0 错误、27 个变更文件 |
| Pure/Unit | Combat.UI.Log 分类、筛选、富文本转义、时间与容量 | Automation 报告 | 通过，最终全量 67/67 中含新增 4 项 |
| World Automation | 真实伤害治疗、出生 Modifier、组件订阅与 teardown | Automation 报告 | 通过，真实 25 伤害/25 有效治疗，0 治疗隐藏，蓝图重建恢复历史 |
| PIE / Blueprint | WBP 编译保存、开关与筛选、实际显示截图 | Saved/CombatLog | 通过，PIE-Final.png / PIE-Filtered.png / PIE-UI-Snapshot.txt |
| Network / Dedicated | 三 Target + Dedicated 双客户端日志投影 | Saved/CombatLog | Editor、源码 Client、最终 Server 构建与 Dedicated 双客户端全部通过 |
| Soak / Perf | 512 条有界淘汰断言，64 Unit/256 Modifier 容量 | World Automation / Dedicated | 容量与淘汰通过；未执行长时 soak |

本轮实际入口（工程路径为仓库根目录）：

```powershell
& 'C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat' ue_gasEditor Win64 Development "$PWD\ue_gas.uproject" -WaitMutex -NoHotReloadFromIDE -NoLiveCoding
& 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' "$PWD\ue_gas.uproject" -unattended -nop4 -nosplash -NullRHI -NoSound -NoLiveCoding '-ExecCmds=Automation RunTests Combat.;Quit' '-TestExit=Automation Test Queue Empty' "-ReportExportPath=$PWD\Saved\CombatLog\Automation-Delivery" "-AbsLog=$PWD\Saved\CombatLog\Automation-Delivery.log" -ModelContextProtocolPort=8071
& 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' "$PWD\ue_gas.uproject" -run=CombatAssetValidation -unattended -nop4 -nosplash -NullRHI -NoSound "-Report=$PWD\Saved\CombatLog\AssetValidation-Complete.json" "-AbsLog=$PWD\Saved\CombatLog\AssetValidation-Complete.log" -ModelContextProtocolPort=8072
powershell -NoProfile -ExecutionPolicy Bypass -File Saved/CombatLog/Run-LogDedicated-Final.ps1
& 'D:\UE\UnrealEngine\Engine\Build\BatchFiles\Build.bat' ue_gasServer Win64 Development "$PWD\ue_gas.uproject" -WaitMutex -NoHotReloadFromIDE -NoLiveCoding
& 'D:\UE\UnrealEngine\Engine\Build\BatchFiles\Build.bat' ue_gasClient Win64 Development "$PWD\ue_gas.uproject" -WaitMutex -NoHotReloadFromIDE -NoLiveCoding
python -B Tools/task_gate.py --mode delivery --spec Doc/CombatSystem/Specs/HUD-LOG-001-combat-log.spec.md --kind feature --report Saved/CombatLog/delivery.json
python -B Tools/validate_docs.py
git diff --check
```

Blueprint 的创建、Designer 属性、父类与 HUD 引用均通过 UE MCP 的读取、设置、编译、保存、回读执行。交互验证使用 SlateInspector 的聚焦/键盘确认、下拉选择、滑条与 Escape；工具合成点击在部分控件上只取得焦点，因此同时检查实际控件状态和截图，不将工具返回 true 当作完成证据。

## 8. 风险、回滚与升级

- 风险：日志增长/复制负载、富文本注入、跨流资源值被错误反算、UI 输入穿透。
- 回滚方式：成组撤销本任务 C++、资产和引用；无数据迁移。
- 触发升级的条件：必需验证无法执行或出现核心结算变更需求；先完成仍可独立验证的已授权工作。
- 需要人决定的问题：暂无。

## 9. 交付证据

- 机器交付检查：`python -B Tools/task_gate.py --mode delivery --spec Doc/CombatSystem/Specs/HUD-LOG-001-combat-log.spec.md --kind feature --report Saved/CombatLog/delivery.json` 实际退出 0；输出 `Combat task gate delivery passed: 0 error(s), 27 changed file(s)`，完整结果见 `Saved/CombatLog/delivery.json`。
- 代码/资产 diff：新增 CombatLogTypes/Component/Widget/Tests；扩展 Emit 的同步显示上下文、Damage/Heal 端点、Controller 默认组件及 HUD 生命周期；新增 WBP_CombatLog 并配置 BP_CombatPlayerHUD，补齐 Demo 效果的 PrimaryAsset 扫描。
- Push-Ready 六层：Tests 通过（67/67、资产、PIE、Dedicated）；Types/Build 通过（Editor、源码 Client 与最终 Server）；No Regression 通过（全量 Combat、原 HUD 与移动断言）；Adversarial 通过（下述 F2）；DDD/Constraints 通过（ADR-052 与专题同步、核心 schema 仍为 1）；Decisions 通过（物品/存档/英雄定义边界已冻结，无待决策项）。
- 构建结果：安装版 UE 5.8.2 Editor 通过（`Saved/CombatLog/Build-Editor-Complete.log`，4 个步骤、16.09 秒）。源码 UE 5.8.2 Server 首轮全量通过（`Build-Server.log`，1017 个步骤、5777.98 秒）；源码 Client 全量通过（`Build-Client.log`，1038 个步骤、6766.93 秒）；覆盖最终名称、反射与测试修正的 Server 增量通过（`Build-Server-Final.log`，11 个步骤、279.37 秒）。三者进程退出码均为 0，输出二进制已生成。源码引擎的首次完整编译耗时单独保留，不计作功能运行延迟。
- Automation：`Saved/CombatLog/Automation-Delivery/index.json`：67/67（66 Success、1 SuccessWithWarning、0 Fail）；唯一 warning 是既有调试命令用例的预期 Usage 提示。最终源码覆盖出生名称、Demo 效果 PrimaryAsset 发现、选项重名、富文本与生命周期回归。
- 资产：`Saved/CombatLog/AssetValidation-Complete.json`：10 个定义，0 error/0 warning。两份 HUD 蓝图由 UE MCP 编译、保存并回读父类与引用。
- Dedicated：`Saved/CombatLog/Dedicated-Final-Summary.txt`；UE 5.8.2 CL56702186 安装版独立 `-server/-game` 三进程实际联网。Server 有 2 份、两个 Client 各 1 份日志，时间/序号/真实端点断言全为 Pass；HUD 2/1/1、64 Unit/256 Modifier、移动对撞和冻结发布契约也为 Pass。容量采样 FrameP95 11.673 ms / P99 16.863 ms / MaxOut 8.554 KiB/s。源码 Target 构建与此联网证据分别记录。
- PIE：真实 Demo 通过公共 AttackTarget Order 攻击木桩，产生 32 点实际伤害、生命 500→468→436→404→372→340，以及中文减速施加/到期；窗口关闭期间仍保留 16 条。来源卓尔游侠/目标木桩并关闭状态后为 5 条伤害，改为目标卓尔游侠为 0 条；Escape 收起窗口但 PIE 保持运行。面板右键前后 Order 事件均为 17 条。`Saved/CombatLog/PIE-Final.png` 为完整记录，`PIE-Filtered.png` 为伤害筛选，`PIE-UI-Snapshot.txt` 为控件状态。
- 环境记录：`GAP-027` 为既有测试 CDO 持有临时数据导致同 Editor 切图 GC 错误，引用链见 `Editor-UI.log`；完整 Automation 改为独立进程，PIE 使用干净 Editor。Dedicated 中 Experimental Toolsets 的 Python 初始化错误和动态测试 GE 无网络定义日志属于既有工具/夹具边界，实际 Combat 检查均通过。
- 剩余风险：暂不提供永久存档、物品事件或战争迷雾；按当前网络相关性显示，最多 512 条。“英雄”身份沿用当前玩家指挥单位，未引入新的玩法分类。

### F2 差异审查记录

以最终接口、Spec 和可执行证据重新审查：无客户端结算或写入 RPC，无新增 gameplay Tick/Timer；只读同步上下文不改变核心 schema/数值公式；FastArray 自身不被显示排序重排；单位弱缓存与加载修订号保护 teardown。发现并关闭：环形数组引用被重入 Emit 改写、持续事件清空正在打开的下拉列表、Slate 重建丢失显式历史源、出生固有效果早于完成标记导致无单位名、Demo 效果未进入 PrimaryAsset 扫描导致中文名不可解析。窗口布局标签宽度已在实际 DPI 下调整，最终资产发现断言、中文显示和三进程联网均已复验。

最后边界复核发现：显示名“全部”可覆盖默认筛选；作者直接填写“木桩 · #22”也可能碰撞自动生成的同名实例标签。先提取原有选项命名规则并执行回归，`Automation-Label-Red/index.json` 为 3 成功/1 失败，恰好触发上述两个身份冲突断言。随后先保留“全部”和已离场的当前选择，再为冲突名称持续追加实例后缀直到标签唯一；`Build-Editor-Complete.log` 构建通过，`Automation-Delivery/index.json` 全量 67/67 成功。该修正仅影响本地下拉文字，不改变已验证的复制内容、资产或普通名称画面。

## 10. 变更记录

| 版本 | 日期 | 修改 | 原因 |
| --- | --- | --- | --- |
| 0.1 | 2026-09-12 | 初稿与 F0/F1 | 用户授权战斗日志系统 |
| 0.2 | 2026-09-12 | 完成实现、F2、三 Target 与运行验证，转待用户验收 | 实际代码、资产、联机及画面证据齐备 |
| 0.3 | 2026-09-13 | 用户验收完成，同步状态并准备本地提交 | 用户明确确认“验收完成，提交吧” |

## 11. 路由、自评与 Reflect

### 路由记录

- 原始需求摘要：DOTA2 风格战斗记录窗口。
- 主 Skill：combat-feature-development。
- 选择依据：功能和 UI/网络显示新增。
- 备选 Skill 与排除理由：不是技能内容制作。
- 路由置信度：`high`

### 交付自评

| 维度 | 权重 | 分数（0–5） | 证据与扣分原因 |
| --- | ---: | ---: | --- |
| 需求与 AC | 20% | 4.5 | AC-01..05 均有可执行证据；物品系统尚不存在，入口按约定置灰说明 |
| 架构与权限 | 20% | 5 | 服务器权威、owner-only 只读投影、实际事务端点；核心 schema 与结算保持兼容 |
| 实现与数据 | 20% | 4.5 | 512 条有界历史、稳定实例筛选、弱引用与加载取消、重建回归；当前英雄身份与内存历史仍受既有范围限制 |
| 验证证据 | 20% | 4.5 | 三 Target、67/67 Automation、10/10 资产、PIE 与 Dedicated 双客户端通过；未执行 cook/打包或长时 soak |
| 文档与可观测性 | 10% | 5 | ADR-052、GAP-027、生命周期与 HUD/网络专题、台账和实际命令同步 |
| 交付卫生 | 10% | 5 | 蓝图经 UE 编译保存回读；生成证据留在忽略目录，无无关用户文件改动，空白检查通过 |

- 计算总分：`4.7/5`（4.5×20% + 5×20% + 4.5×20% + 4.5×20% + 5×10% + 5×10%）。
- 硬性封顶或未执行项：无必需验证缺口；cook/打包与长时 soak 在本次增量范围外，尚未执行。
- 自评结论：`SUFFICIENT`。
- 用户验收状态：`用户已验收`；2026-09-13 用户明确确认“验收完成，提交吧”。本次验收归档仅更新文档状态和检查提交内容，不改变已验证代码/资产，不重跑 UE 构建或运行矩阵。

### Reflect 与调优

- 观察与证据：全量 Automation 发现 Slate 重建订阅遗漏；实际 PIE 暴露出生被动名称与 Demo 效果资产发现缺口。修正后通过最终 67/67、中文截图与 Dedicated 双客户端验证。
- 根因类别：`单次实现`。
- 调整文件与预期收益：缺口已固化到 CombatLogTests 与现有 PrimaryAsset 配置；同一任务内的实现问题不升级为通用 Skill 规则。既有测试 CDO 泄漏另记 GAP-027，本次使用独立测试进程隔离。
- 回归验证：最终完整 Automation、资产、PIE、Dedicated 和 Editor/Server/Client 三 Target 全部通过。
- 需要用户决定的问题：暂无。

## 12. 用户验收归档（2026-09-13）

- 用户确认“验收完成，提交吧”，同意战斗记录系统及标题拖动交互，授权本地 Git 提交。
- 归档复核：`python -B Tools/validate_docs.py` 通过，64 篇 Markdown、339 个本地链接、0 错误；`git diff --check` 通过。
- 本 Spec 的 `task_gate.py --mode preflight` 与 `--mode delivery` 均退出 0；最终 delivery 为 28 个变更文件、0 错误，报告为 `Saved/CombatLog/preflight-acceptance.json`、`Saved/CombatLog/delivery-acceptance.json`。
- 本次仅同步验收状态和检查提交内容，沿用 §9 的已执行工程验证；未重跑构建/Automation/PIE/Dedicated，不改变既有范围限制与自评分。
