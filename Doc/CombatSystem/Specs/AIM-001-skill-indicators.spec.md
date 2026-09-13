# AIM-001 技能瞄准与范围指示器首版

> Spec 版本：`0.1`
> 状态：`已验收`
> Owner：Codex / 用户本地验收
> 创建日期：2026-09-13
> 关联进度台账：[00-01](../00-Project/00-01-Progress-Tracker.md)
> 风险等级：`L1`

## 0. Intake 与 Gate 记录

- 用户请求：参考 DOTA2 为当前 Combat 设计技能指示器；用户确认设计后说“可以，继续下一步吧”，授权按首版设计实现。
- 附件解释：无附件；上一轮交互原型和设计为已确认参考，示意数值不写入现有卓尔游侠平衡数据。扇形与向量属于后续 P1。
- 已读取入口：`agent.md`、`README.md`、`Skills/combat-task-router/SKILL.md`、`Skills/combat-feature-development/SKILL.md`、DDD `10-01`、`10-03`、`10-09`、`10-10`、`10-12`、`20-02`、`20-03`、`90-16`、`00-03`、`00-04`、`00-05` 与 UE MCP 工作流 `30-01`。
- 主 Skill：`combat-feature-development`（`Skills/combat-feature-development/SKILL.md`）。
- 备选 Skill 与排除理由：`combat-skill-development`；本任务是通用输入/表现设施，演示技能只组合现有公共 Action，不开发新结算语义。
- 路由置信度：`high`
- F0 结论：`GO`
- F0 依据：用户已确认三层指示器与首版边界；工作区干净，无 `.codegraph/`；UE 环境检查 Editor/Dedicated 均 ready，无运行中的 Editor。当前会话没有 UE MCP 工具，使用同版本 Unreal Python、命令行资产回读和自动化降级。
- F1 结论：`APPROVED`
- F1 审查人：Codex（L1；用户已确认设计及继续实现，按 00-05 §4 在现有授权内审查）。
- F1 审查版本：0.1
- F1 计划审查证据：`Saved/SkillIndicators/PreflightGate.json`、`PlanGate.json` 均为 0 errors；逐项核对三层预览、输入互斥、距离/Action 来源、只读范围复制、旧会话隔离、独立训练资产、三 Target 与回滚。无新增权威协议，已有 Q/HUD 意图不变，批准版本 0.1；审查时尚无行为文件修改。
- Build 解锁：`Saved/SkillIndicators/BuildGate.json` 通过（0 errors）；通过后才新增最小输入测试。
- F1 重审条件：范围、架构、权限、迁移、测试矩阵或回滚实质改变时递增版本并重新审查。
- F2 结论：`PASS`
- Push-Ready 结论：`READY`
- 验证：installed/source UE 均为 5.8.2；Editor、源码 Server/Client 完整构建及新增 PIE 检查后的两 Target 增量构建全部通过。直接 Input 6/6、最终全量 Combat 70/70、资产 17/17（0 warning）、真实 PIE 22 项、Dedicated 双客户端范围/视觉隔离与容量检查通过；汇总见 `Saved/SkillIndicators/EvidenceSummary.json`。
- 未执行：首版未执行成品 cook/打包、多显示器/DPI 组合及复杂地形专项；本轮开发构建和功能 smoke 的结果不代表这些环境均已验证。后续地面接收修复与画面验证见 [AIM-002](AIM-002-ground-only-indicators.spec.md)。
- 用户验收：2026-09-13，用户明确回复“验收完毕，提交吧”；AIM-001 连同 AIM-002 修正已通过实机验收，授权本地提交。

## 1. 目标与范围

### 目标

将主动技能从按键即提交升级为可持续瞄准、确认和取消；范围显示与同一技能的真实参数一致，服务器继续结算。

### 范围

- 单位、点、圆形、直线与自身范围预览；AutoCast 悬停显示普攻范围。
- 标准施法默认；Controller 可配置按下快施或松开快施，统一 Enhanced Input 绑定与唯一请求入口。
- 绿色可提交、琥珀色超距、红色目标错误、灰色资源/控制阻断及数据未就绪；初始回执只提示接收或拒绝。
- 本地 AimComponent、只读投影适配和可复用纯视觉贴花 Actor；HUD 悬停与点击阻断、焦点及 teardown 清理。
- 数据兼容新增显式主预览 Action 索引，-1 表示没有作用形状；已有资产仍可显示可靠施法圈或目标标记。
- 独立 Demo 指示器训练地图及样例定义，复用生产公共 Ability/Action，不修改原卓尔游侠 Q/空 WER 配置。

### Non-Goals

不实现扇形权威查询、向量协议、排队快捷键、多选、自动选敌、战争迷雾、命中预测、技能数值平衡或新服务器结算链。不创建提交或推送。

## 2. 当前事实与依据

以下保留修改前的开工基线；实现后的行为与验证见 §3、§7、§9。

- `Source/Combat/CombatPlayerController.cpp:426`：Started 即构造 Cast；`:520` 存在 175/1000 cm 的模糊单位回退。
- `Source/Combat/Combat/Targeting/CombatTargetingSubsystem.cpp:149`：XY 边缘距离 + 5 cm 容差；距离失败优先于 LOS，OutOfRange 不保证其他后续条件。
- `Source/Combat/Combat/View/CombatHUDView.cpp`：owner-only HUD 投影按授予顺序匹配至多四个技能；当前展示 schema 5。
- `Source/Combat/Combat/UI/CombatHUDSlotWidget.cpp:198`：悬停详情、点击固定；不允许施法点击穿透。
- 相关 DDD：`10-03` 目标/施法，`10-09` RPC/追击，`10-12` HUD，ADR-047/048/049。
- 当前测试/日志证据：台账最近 `Combat.*` 67/67 为历史证据，本轮须重跑。
- 开工限制：无当前 UE MCP 工具；本轮实际验证见 §7、§9，以上源码行号仅记录修改前定位。

## 3. 行为与契约

### 主流程

Controller 解析现有 Spec 槽位 → 本地准备会话 → 每帧光标射线与只读预览 → 确认时重新取命中、校验当前 Spec/Owner/生命 → 单次 `SubmitCombatOrder` → 既有服务器 Order/Targeting/Ability。

### 状态转换

Idle → Aiming → Submitted/Idle。无效确认保留 Aiming；标准模式 Esc/右键仅取消本地会话并消费当前手势。无目标技能与 AutoCast 按下立即沿原入口处理。按键释放只对同一仍有效会话生效；Cancelled 输入永不等价于 Completed。

### 输入、输出与数据约束

- 复用当前 QWER、确认、取消、A、S 和右键 Action，改键仍在 IMC_Default，不新增固定物理键兜底。
- 单位目标只认实际命中 Actor，无近邻或施法者附近替选。点目标原始位置不截断；无地面命中或 UI 下禁止确认。
- 形状参数使用明确主 Action 的 RadiusKey、ProjectileRangeKey 与 ProjectileData 回退；源/目标锚点遵守同一 Action 的 Caster/Target 语义。
- 拥有者快照兼容新增 CastRangeBonus 与 AttackRange 等必要只读范围信息；展示 schema 6，服务器/客户端同构更新。单位胶囊半径参与名义距离圈，容差不加大 HUD 数字。
- 原生输入适配可查询 ASC 的授予身份与公共 Targeting；渲染、UMG 只消费已整理的展示结构，不能反向驱动战斗。

### 权威边界与权限

客户端提示不承诺路径、LOS 后续条件或实际命中；超距允许发送原 Cast 请求进入服务器追击。沿用现有 ownership、正 RequestId、载荷、限频和重放校验，不新增 gameplay RPC、客户端命中列表或客户端 Ability 激活。已确认设计授权可回滚源码/资产及独立展示字段；不修改核心发布契约。

### 失败、取消、过期、死亡、EndPlay 与重复请求

局部 SessionSerial + CommandBindingGeneration + LifeGeneration 隔离旧输入；弱 Unit/Target 引用。换技能、控制转移、死亡/复活、撤销技能、失去 Owner、焦点丢失、EndPlay 统一取消。UI 焦点消费 Esc 时同步取消；鼠标松开位于 UI 时丢弃旧落点。初始回执按 RequestId 与绑定/生命核对，不自动重发、不把 Accepted 当施法完成。Tick 仅推进视觉，无周期 gameplay。

### 兼容、版本与迁移

先记录 ADR-053 与展示 schema 6，再修改公开字段；核心 `combat_v1_rc1`、Content/Formula/Tag/Event schema 不变。预览配置为安全可选字段。保留 HUD 点击详情、Q AutoCast 和现有 Input Action 引用；参数说明更新为共享确认/取消。材质与独立 Demo 资产由 Unreal Python 创建/编译/保存/回读，不用文本改二进制。

## 4. 实施计划

| 文件/资产 | 变更 | 原因 | blast radius |
| --- | --- | --- | --- |
| `Source/Combat/Combat/UI/CombatAbilityAim*`、`CombatAbilityIndicator*` | 本地会话、预览结构、贴花绘制 | 分离意图与视觉 | 本地 PC 生命周期 |
| `CombatPlayerController.*` | 标准/快施、确认/取消互斥、原始命中 | 接入既有请求入口 | 玩家输入 |
| `CombatDefinitionData.*`、`CombatHUDView*`、`CombatUnitViewComponent.h` | 显式主 Action、范围投影、schema | 参数单源与配置校验 | Ability Data / owner-only View |
| `CombatHUDWidget.*`、`CombatLogWidget.*`、`CombatPlayerHUD.*` | 悬停、UI 命中和 Escape 协同、短状态提示 | 不穿透 UI | 现有 HUD |
| `Source/Combat/Combat/Tests/CombatAbilityAimTests.cpp` 及相关测试 | 状态机、参数、权限、资产与回归 | 行为证据 | 验证基础设施 |
| `Content/Combat/Shared/Materials/M_CombatAbilityIndicator` | 参数化圈/线/准星贴花 | 实际世界表现 | 纯视觉 |
| `Content/Combat/Demo/Indicators`、独立训练地图 | 四种现有公共技能组合与原生配置 | 可直接试玩 | 不替换原 Demo |
| `Doc/CombatSystem/10-Architecture/10-13-Skill-Indicators.md`、README/台账/ADR/10-09/10-12 | 当前行为和配置导航 | 文档同步 | 当前文档 |

## 5. 验收标准（AC）

- [x] AC-01：标准模式按键不发目标 Cast，确认重新取命中且只发一次；无效地面不替选单位。
- [x] AC-02：超距提示与服务器 Chasing 一致；原目标点保持不变；缺蓝、冷却、沉默与非法目标可区分。
- [x] AC-03：按下/松开快施可配置；取消、换技能、右键、A、S、UI、焦点与旧释放不会误发。
- [x] AC-04：圈/线参数与等级、覆盖、胶囊半径一致；无描述不猜半径；AutoCast 保持服务器 Toggle。
- [x] AC-05：Owner、重生、EndPlay 清理；Dedicated 无视觉 Actor，客户端只消费投影与结果。
- [x] AC-06：HUD 悬停显示范围、点击固定详情，加点/日志拖动无穿透；训练地图可复现点、单位、直线、无目标技能。
- [x] AC-07：Editor/Server/Client、相关并全量 Automation、资产校验、PIE 与 Dedicated 完成并留真实证据。

## 6. Definition of Done

- [x] 最小 Red 复现缺少瞄准会话，随后用真实 Unit/ASC/Order fixture 验证 Green 与取消路径。
- [x] 代码中文说明、DataAsset/蓝图 metadata 与公共规则一致。
- [x] 资产真实编译保存回读，实际世界视觉与按键有证据。
- [x] 适用三 Target、Automation 与 Dedicated 通过；未执行项如实记录。
- [x] F2 问题关闭、文档校验和 diff 检查通过。

## 7. 测试矩阵与命令

| 层级 | 用例/命令 | 预期证据 | 结果 |
| --- | --- | --- | --- |
| 文档/流程 | `python -B Tools/task_gate.py --mode preflight/plan/build/delivery --spec Doc/CombatSystem/Specs/AIM-001-skill-indicators.spec.md --kind feature`；`python -B Tools/validate_docs.py`；`git diff --check` | `Saved/SkillIndicators/*Gate.json` | preflight/plan/build 通过；最终 delivery 结果见 `DeliveryGate.json` |
| Editor | 本机环境定位的 Build.bat，`ue_gasEditor Win64 Development` | `Saved/SkillIndicators/EditorBuild.log` | 通过 |
| World Automation | `Combat.Input.AbilityAim.*`、既有 Input/HUD/Targeting 与全量 `Combat.` | `InputGreen/index.json`、`FullFinal/index.json` | 直接 6/6、最终完整 70/70 通过；既有 DebugAddExperienceCommand 非法参数用例有 1 条 Usage warning |
| PIE / Blueprint | Unreal Python 创建与回读材质/训练地图；真实 Enhanced Input、视口鼠标射线、截图 | `AssetsCreated.json`、`AssetValidation.json`、`PIE.log`、`PIEResult.json`、`PIE-Circle.png`、`PIE-Line.png`、`PIE-OutOfRange.png` | 17 定义 0 errors/0 warnings；PIE 22 项通过、总请求 4、视觉复用 1 Actor；真实 HUD 悬停与超距后移动 256.900 cm 通过 |
| Network / Dedicated | `ue_gasServer`、`ue_gasClient`；独立 `UnrealEditor -server` 与两个 `-game` 客户端，复用既有 smoke 编排 | 两 Target 的 `*-Build-Final.log`、`*-Build-Verification.log`；`Dedicated/DedicatedSummary.txt` | 两 Target 完整及最终增量构建通过。Dedicated：Server Owners=2/Visuals=0；每个 Client Owners=1/Visuals=1，schema 6 范围字段一致，外部单位 owner 数据不可见 |
| Soak / Perf | 既有容量 smoke 回归；瞄准期间无逐帧 RPC、无全场查询 | 请求计数、对象上限与日志 | 64 Unit/256 Modifier 容量与性能 Budget=Pass；碰撞移动 381.703 cm、静止单位 0 cm；PIE 瞄准/取消请求为 0 |

## 8. 风险、回滚与升级

- 风险：UI/Enhanced Input 消费顺序、复制未齐、旧 KeyUp、贴花坡面与方向、参数来源歧义、跨引擎构建资产。
- 回滚方式：按本 Spec 成组撤销新增输入/表现源码、owner-only 字段/schema、可选配置与新增训练资产；原 Hero 资产与输入映射保持兼容，不回退用户差异。
- 触发升级的条件：需要改变权威目标协议、技能结算、不可逆迁移，或三轮仍不收敛；先修订 Spec，不扩到 P1。
- 需要人决定的问题：无；用户已完成本轮实机验收。

## 9. 交付证据

- 代码/资产 diff：本地 AimComponent、三层贴花 Actor、Controller/HUD 输入协同、主 Action 几何解析、schema 6 拥有者范围字段；独立四技能训练地图及其 World Partition 外部 Actor。原 Demo 资产不变。
- 构建结果：Editor、源码 Server/Client 完整构建全部 `Succeeded`；补充原生 PIE 检查后，Server 和 Client 各 3 个增量 action 均 `Succeeded`。构建命令统一为 `Build.bat ue_gasEditor/ue_gasServer/ue_gasClient Win64 Development <ue_gas.uproject> -WaitMutex -NoHotReloadFromIDE`，Editor 使用安装版 5.8.2、Server/Client 使用源码版 5.8.2。完整命令和实际输出保留在 `EditorBuild.log`、两份 `*-Build-Final.log` 及两份 `*-Build-Verification.log`。
- Automation/PIE/Dedicated 报告：统一在 `Saved/SkillIndicators/`；直接 6/6、最终全量 70/70、真实 PIE 22 项和 Dedicated 双客户端通过。PIE 确认了世界圈/线、实际按键、HUD 悬停、UI 几何以及远处确认后真实追近；测试 Actor 未保存到地图。网络 smoke 使用同版本已编译安装版 Editor 的真正独立 `-server`/`-game` 进程；本地脚本由 `Tools/RunDedicated.ps1` 复用编排，并额外验证 CombatLog 与 30 秒容量快照。
- 未执行验证及原因：首版未执行成品 cook/打包、多显示器/DPI 组合及复杂台阶/坡面专项；后续地面修复和坡道/高台验证见 AIM-002。本轮用户验收不代替未运行的环境矩阵。
- 剩余风险：见 §8；不将设计原型当作 Unreal 验证。

## 10. 变更记录

| 版本 | 日期 | 修改 | 原因 |
| --- | --- | --- | --- |
| 0.1 | 2026-09-13 | 首版实现计划 | 用户确认设计并授权继续 |

## 11. 路由、自评与 Reflect

### 路由记录

- 原始需求摘要：已确认 DOTA2 风格三层指示器，继续首版实现。
- 主 Skill：combat-feature-development。
- 选择依据：通用输入/表现设施，不改变技能结算。
- 备选 Skill 与排除理由：combat-skill-development；仅使用已有 Action 制作独立演示配置。
- 路由置信度：high。

### 交付自评

| 维度 | 权重 | 分数 | 证据与扣分原因 |
| --- | ---: | ---: | --- |
| 需求与 AC | 20% | 4.5 | AC-01～07 有自动化、实际输入和联机证据；首次交付评分时最终手感尚待用户验收 |
| 架构与权限 | 20% | 5.0 | 原 RPC/权威入口不变；复制身份一致性、生命周期和 owner-only 视觉有真实双客户端回归 |
| 实现与数据 | 20% | 4.5 | 17 定义校验、明确 Action 参数与 22 项 PIE 通过；复杂台阶/坡面的贴花表现尚未完整验收 |
| 验证证据 | 20% | 4.5 | 三 Target、直接 6/6、全量 70/70、PIE/Dedicated 通过；成品 cook 与多 DPI 组合未执行 |
| 文档与可观测性 | 10% | 5.0 | DDD、ADR、Spec、台账、中文 metadata、失败/修复日志及统一证据汇总齐全 |
| 交付卫生 | 10% | 5.0 | 原 Demo 资产未改；新增 84 个二进制全部 LFS；diff 检查通过，未执行范围与验收状态明确 |

首次交付总分：`4.7 / 5.0`。当前状态：`用户已验收`（2026-09-13，连同 AIM-002 修正）；保留原评分，不将验收自动换算为测试分数。

### Reflect 与调优

- 观察与证据：Standalone 与完整 Automation 可通过，而 Dedicated 首轮两客户端均无指示器；`DedicatedRed/` 的 Visuals=0 暴露了未复制初始化缓存被误用为客户端就绪条件。
- 根因类别：领域契约与单次实现。
- 调整文件与预期收益：AimComponent 改为核对复制快照；10-13 明确服务器缓存与客户端身份的边界；Dedicated smoke 保留范围字段与视觉计数断言，防止后续仅凭本地 World 结果交付。
- 回归验证：最终 Dedicated 两客户端 Visuals=1、Server Visuals=0；`FullFinal/index.json` 70/70。单次问题只固化到本任务和专题，不修改通用 Skill。
- 需要用户决定的问题：无，2026-09-13 已完成实机验收。

### F2 对抗审查

- 审查人/版本：Codex，0.1；单人本地流程，重新从实际调用和日志检查，未依赖“实现应该正确”的推论。
- 第一轮关闭：提前 BeginPlay 误停 Tick、UI 子控件消费右键、直线三维距离投影和快施失败提示；以完整 Automation、真实 PIE 复验。
- 第二轮关闭：Dedicated 暴露的服务器初始化缓存误用；保持原 owner/LifeGeneration/Spec 校验与 RPC 边界，以失败日志和实际双客户端 Green 复验。
- 权威与性能：新增 Tick 只查询当前命中和最多四个技能槽，只有确认进入现有 SubmitCombatOrder；无全场扫描、伤害、GE 写入或第二套 gameplay 时钟。视觉 Actor 本地复用，无复制和碰撞；Owner/World 退出显式解绑和清理。
- 无未关闭高风险发现。三 Target 及最终增量构建已有独立成功日志；复杂地形、DPI 与成品 cook 的未验证范围继续保留。

### 测试与审查记录

- Red：`EditorRedBuild.log` 编译通过；`Red/index.json` 实际失败，标准按键未进入十字光标，右键额外提交 Move（RequestId 从 1 到 2）。
- Green：`InputGreen/index.json` 6/6 通过，包含新增 ActionGeometry、StandardAndCancel、UnitResourcesAndOwnership；fixture 明确设置本地 Controller，不放宽生产本地权限检查。
- 资产：Unreal Python 编译、保存、回读材质、四技能及独立训练场，`AssetsCreated.json`；`AssetValidation.json` 17 定义、0 errors/0 warnings。关卡由 LevelEditor 的模板创建 API 复制外部 Actor，原地图未修改。
- 最终关卡回读：`TrainingReadBack.json` 确认独立 GameMode/英雄/4 个技能、MaxMana=300、ManaRegen=12，保存到地图的 PIE 测试 Actor 数量为 0。新增 84 个二进制文件（训练内容 14、材质 1、World Partition 外部 Actor 66/外部对象 3）全部匹配 LFS，未混入生成目录。
- 构建：`EditorBuild.log` 通过。PIE 单独进程运行，避免既有全量测试 CDO 的 GAP-027；DX11 首次着色器编译未完成，改回项目默认 D3D12 完成画面验证。测试 Actor 使用未保存的非 transient、非空间加载实例进入 PIE 副本，结束后删除；原始 transient 测试实例不会复制到 PIE，修正的是验证编排。
- 审查发现并修复：BeginPlay 早于 LocalPlayer 绑定不能永久禁用 Tick；HUD 子控件吞右键需在 PreviewMouseButtonDown 取消；直线三维方向必须投影到 XY；快施失败需短暂保留原因。修复后完整 Combat 70/70、PIE 16 项通过。
- Dedicated Red：`DedicatedRed/` 两客户端 Visuals=0/Fail，定位为 Aim 对比了未复制的 InitializedUnitDefinitionId。改为校验同代 Public/Owner View 定义身份，保持复制快照作为客户端单一来源。Green：`Dedicated/` 两客户端 Visuals=1/Pass、Server Visuals=0/Pass；随后完整 `FullFinal` 70/70 通过。该回归保护真实远端初始化路径，不能以 Standalone 成功替代。
- 原生 PIE 补充：Python 对 HUD 几何的回读未获得有效尺寸，因此不以该脚本输出判断悬停功能；新增原生检查直接读取实际 Slot 几何并移动光标，验证仅显示施法圈且不发请求。第一次远距导航检查暴露启动 PIE 时编辑器 NavMesh 仍在构建（`TrainingNav.json`）；`RunPIE.py` 改为等待 Navigation ready 后启动，并在原生测试中使用真实可达地面。最终 22/22、总请求 4，服务器移动 256.900 cm；生产逻辑和地图无须修改。新增检查仅扩展测试 Actor，不重复已通过且未受影响的 Automation/Dedicated。
- 联机背景日志：安装版工具集 Python 初始化及动态 GameplayEffect 定义的告警/错误在本轮仍出现；已与 `Saved/CombatLog/Dedicated-Final-Client1.log` 的既有同类日志核对。范围字段、技能投影、战斗记录、RPC、移动和容量断言均通过，本轮未修改这些外部工具集或既有 GE 复制实现，不声称整个日志零 warning/error。
