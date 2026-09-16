# ECON-002 紧凑商店与独立储藏室 UI

> Spec 版本：`0.3`
> 状态：`已验收`
> Owner：Codex
> 创建日期：2026-09-16
> 关联进度台账：[00-01](../00-Project/00-01-Progress-Tracker.md)
> 风险等级：`L1`

## 0. Intake 与 Gate 记录

- 用户请求：在已完成的 ECON-002 v0.2 基础上继续精简合成区域：去掉滚轮和多余文字，只显示配方；此前 1920×1080 百分比几何、双 UI、左右键交互和储藏室要求保持不变。
- 附件解释：参考。2026-09-16 新截图中的外层红框只用于确定商店整体位置与尺寸，上方内框表示固定物品区域，下方内框表示固定合成区域；截图中的 UI 文字、场景、美术和红框本身都不是实现指令。此前三张截图和可视化继续只作为信息结构、密度与交互参考，不复制第三方图标、美术、文字或平衡数值。
- 已读取入口：`AGENTS.md`、`agent.md`、`README.md`、`Skills/combat-task-router/SKILL.md`、`Skills/combat-feature-development/SKILL.md`、`00-01`、`00-03`、`00-05`、`10-01`、`10-12`、`10-15`、`ECON-001` 及现有 Shop/HUD/测试源码。
- 主 Skill：`combat-feature-development`（`Skills/combat-feature-development/SKILL.md`）。
- 备选 Skill 与排除理由：`combat-skill-development` 不适用；本任务只调整经济 UI 组合、Slate 几何与本地输入，不新增 Ability、Modifier 或结算入口。
- 路由置信度：`high`
- F0 结论：`GO`
- F0 依据：ECON-001 已提供服务器权威 Economy、目录、配方和事务 API；本次只重组本地只读界面并复用原有 RPC，范围可回滚且不改变网络载荷、价格、配方或物品实例权威。
- F1 结论：`APPROVED`
- F1 审查人：Codex（L1，本地合成区展示精简）
- F1 审查版本：`0.3`
- F1 计划审查证据：v0.3 preflight 与 plan Gate 均通过（0 error(s)）。逐项审查确认 151 固定高度、无滚动容器、0/1 配方槽、辅助文字删除、结果 delegate 生命周期、TDD 与回滚范围已冻结；节点输入和服务器 Economy/RPC/配方语义不变，超宽配方裁切保留为视觉验收风险。
- Build 解锁：`已解锁`；v0.3 build Gate 通过（0 error(s)），允许进入 TDD。
- F1 重审条件：若需要改变 Economy RPC、配方语义、储藏容量、资产 schema、发布契约、拖放事务或测试矩阵，先递增 Spec 版本并重审。
- F2 结论：`PASS`
- F2 说明：静态复审确认合成区直接持有 `RecipeBox`、不构造 `SScrollBox`，151 高外框显式 `ClipToBounds`；辅助文本与仅服务该文本的 `OnEconomyResult` 订阅已成组移除。配方节点仍复用既有命中与购买入口，无新 Tick、Timer、状态或权限路径，无未关闭发现。
- Push-Ready 结论：`READY`
- Push-Ready 说明：v0.3 的 Tests、Types/Build、No Regression、Adversarial、DDD/Constraints 与 Decisions 六层均有实际证据；delivery Gate 已通过，用户已完成视觉验收。
- 验证：v0.3 DebugGame Editor 构建通过；Development 源码编译与带后缀模块链接通过（当前打开的 Editor 占用基础 DLL，未替换正在运行的模块）；Recipe-only Red 记录 4 个结构差异，Green `Combat.UI.Shop.*` 1/1；完整 `Combat.*` 94/94（92 success + 2 expected-warning）；文档 77 个 Markdown/390 个链接、0 错误，`git diff --check` 退出码 0；delivery Gate 0 错误。
- 用户验收：2026-09-16 用户明确确认“验收成功，提交吧”，验收 ECON-002 v0.3 视觉结果并授权本地 Git 提交；不包含推送授权。
- 未执行：真实 1920×1080 Demo PIE 几何/鼠标交互未执行（当前会话无 UE Editor MCP，且用户当前打开的 Editor 仍运行旧模块）；无后缀 Development DLL 替换因该 Editor 文件锁未执行，已由同配置 `-ModuleWithSuffix` 编译链接和 DebugGame Editor Automation 覆盖；v0.3 未改资产，未重复资产校验。Server/Client Target、Dedicated、cook/打包和 soak/perf 未执行，因为本次不改网络载荷、资产、结算或周期玩法。

## 1. 目标与范围

### 目标

在不改变 ECON-001 权威经济链路的前提下，把现有单体 Native Widget 重构为独立储藏室与独立商店两个本地 UI，并将用户确认的紧凑几何和合成树交互落实到游戏。

### 范围

- 新增独立 `UCombatStashWidget`，常驻 HUD 右下角，只负责金币入口、全部拿走、六格储藏物品、左键取出和右键出售。
- `ACombatPlayerHUD` 分别创建、持有和销毁商店与储藏室，储藏室金币按钮只调用商店的公开开关接口。
- 储藏室第一行按“全部拿走 → 金币”排列；两者固定紧凑宽度并左对齐。六格固定为同尺寸小型横向矩形，不随整行拉伸。
- `UCombatShopWidget` 只保留搜索、基础/升级页、分类网格与纯配方图，不再构造金币/储藏格、“常驻物品”区域或可见交易反馈。
- 商店物品使用统一小型横向矩形按钮；图标优先读取 `UCombatItemData::Icon`，缺失时使用 `Glyph`/名称首字回退，名称、价格和说明继续由 ToolTip 展示。
- 左键选择目录物品并刷新配方图；无配方时留白，有配方时按“结果 → 直接组件”构造紧凑树，节点角标继续显示拥有数/需要数。
- 合成树的结果和组件节点与目录按钮使用相同外框尺寸；其右键命中也复用现有购买意图，左键允许继续选择该节点查看嵌套配方。
- 商店、储藏室和合成区域继续阻止世界点击；Escape 只关闭商店，不移除常驻储藏室，也不取消服务器 Order。
- 以 1920×1080 为布局基准：原 760 宽商店按 60% 缩为 456（屏宽 23.75%），顶部间距按屏高 5% 固定为 54；整体高度按屏高 78% 取 842，物品区域按屏高 55% 固定为 594，合成区域按屏高 14% 固定为 151。参考截图只提供上下区域关系，不直接取其像素。
- 合成区域保持 151 固定高度，但移除 `SScrollBox` 和可见滚动条；删除“合成方式”标题、选中物品/价格、选择提示、无配方提示、操作说明和交易结果文字，只呈现结果节点、箭头、直接组件及组件连接符。未选择物品或物品无配方时区域为空白。
- 更新 Native Widget Automation、ECON-001 专题说明和进度台账。

### Non-Goals

- 不改变金币、购买、出售、退款、自动合成、储藏容量、物品实例或 RPC 安全语义。
- 不新增蓝图价格计算、客户端可购买权威判断或第二套 Economy View。
- 不制作或导入新的第三方图标；沿用现有资产图标、Glyph 和颜色。
- 不实现新的库存拖放协议、快捷购买、收藏、中立/团队页、详情侧栏或常驻物品栏。
- 不改发布契约、GameplayTag/Event schema、DefinitionId 或 Demo 平衡数值。
- 不改变配方展开层级、节点尺寸、节点左右键语义或服务器交易结果；只是停止在 Shop Widget 中订阅并显示交易反馈。

## 2. 当前事实与依据

- `Source/Combat/Combat/UI/CombatShopWidget.cpp:35-166` 当前在一个 `SOverlay` 中同时构造 650×530 商店、金币按钮和 3×2 的 92×48 储藏格，违反已确认的独立 UI 和紧凑单行规则。
- `CombatShopWidget.cpp:295-359` 当前目录按钮为 116×58 文本块，分类纵向堆叠；需要改为两列分类和固定小型横向图标。
- `CombatShopWidget.cpp:384-410` 当前配方仅为一段文字，没有与目录等大的结果/组件节点。
- `CombatShopWidget.cpp:441-485` 已通过真实 Slate 几何处理目录右键购买、储藏右键出售和 UI 命中；拆分后应保留相同语义并分别归属两个 Widget。
- `Source/Combat/Combat/UI/CombatPlayerHUD.cpp:7-47` 当前只创建一个商店 Widget；需要新增储藏室生命周期与连接。
- `Source/Combat/Combat/Tests/CombatEconomyTests.cpp:511-552` 当前结构测试断言六格储藏处仍在 Shop Widget 内，将作为首个 Red 修改点。
- 当前工作区已包含 ECON-001 的未提交代码、资产与文档；本任务只叠加相关 UI/测试/专题文档，不回退或格式化其他现有修改。
- 当前仓库没有 `.codegraph/`；按项目规则使用 `rg` 和源码直接定位。当前会话未暴露 UE MCP，若需资产/PIE 检查则记录命令行或人工 Editor 降级。
- v0.2 修改前的 v0.1 基线使用 760×470 面板、距屏幕底部 178 的右下锚定；目录槽为弹性高度、配方槽为内容自适应。该基线现已替换为 1920×1080 百分比基准下的右上锚定和两个固定高度区域。
- v0.2 合成固定区当前仍包裹一个纵向 `SScrollBox`，并构造标题、选中物品/价格、空态、操作说明和交易结果等文本；这些是 v0.3 的明确删除目标。

## 3. 行为与契约

### 主流程

```text
常驻储藏室金币按钮
  -> UCombatShopWidget::SetShopOpen
  -> 搜索 / 页签 / 分类目录
  -> 左键选择并构造只读合成树
  -> 目录或合成节点右键
  -> ACombatPlayerController::PurchaseShopItem
  -> 原 ECON-001 服务器事务与 owner-only Economy View
  -> 商店和储藏室各自刷新只读表现
```

### 状态转换

- 储藏室在本地 HUD 生命周期内常驻；商店初始关闭，金币按钮在开/关之间切换。
- 商店关闭不清空搜索、当前页或已选物品；重新打开时用最新 Economy View 刷新价格颜色、拥有数和反馈。
- 未选择物品或选择无配方物品时合成区保持空白且不保留上一物品节点；选择有配方物品时只构造一棵配方图，不再构造辅助文本或滚动容器。
- Widget 重建、Controller EndPlay 或 HUD EndPlay 都解绑各自 Economy delegate；两个 Widget 不互相持有强引用。

### 输入、输出与数据约束

- 目录与合成节点只提交稳定 `FPrimaryAssetId`；不提交价格、配方展开或拥有数量。
- 储藏室仍按 `FCombatItemView` 的 Handle/Revision 调用现有转移和出售入口。
- 统一几何常量：目录与配方节点 48×34，储藏格 42×32；仅作为本地表现，不进入网络或资产 schema。
- 面板基准几何由 `1920×1080` 与显式比例换算：宽度 `760×60%=456`，顶部 `1080×5%=54`，整体高度 `1080×78%≈842`，物品区 `1080×55%=594`，合成区 `1080×14%≈151`；这些是 1920×1080 下的固定布局值，不从附件截图像素反推。
- 图标资源缺失时使用一至两个字的回退，不因表现缺失禁用合法购买。

### 权威边界与权限

- UI 仅消费 `UCombatEconomyComponent::GetEconomyView()`、`UCombatShopData` 和 `UCombatItemData`；金币、价格、配方、空间和购买结果仍由服务器复核。
- 新 Widget 不直接写 Economy、Inventory、ItemSubsystem、ASC 或 Health/Mana。
- 商店与合成节点的右键都只调用现有 Controller 意图接口，共享 ECON-001 请求 ID、限频和重放保护。

### 失败、取消、过期、死亡、EndPlay 与重复请求

- 目录/图标/配方节点缺失时安全显示空态或回退，不发送无效请求。
- 交易失败继续由 `FCombatEconomyResult` 显示 FailureTag；重复右键由服务器既有重放/修订检查处理。
- 英雄死亡不隐藏储藏室或商店；没有可用英雄时取出失败，但购买/出售仍按 ECON-001 规则处理。
- Escape 关闭商店并恢复游戏焦点；不清空储藏、余额、服务器 Order 或已发送交易。
- HUD/Controller EndPlay 先解除 Widget 互联，再移除父级和 delegate；重复销毁无副作用。

### 兼容、版本与迁移

- 纯本地 UI 类拆分；不改变复制字段、RPC、PrimaryAsset、事件或发布契约，`combat_v3_economy_rc1` 保持。
- `ACombatPlayerHUD::GetShopWidget()` 保留；新增 `GetStashWidget()`。默认原生类在构造函数配置，现有 HUD 蓝图无需新增资产引用即可获得储藏室。
- 回滚时成组恢复 Shop/Stash/HUD/结构测试与专题文档即可，不需要资产或存档迁移。

## 4. 实施计划

| 文件/资产 | 变更 | 原因 | blast radius |
| --- | --- | --- | --- |
| `UI/CombatStashWidget.h/.cpp` | 新增独立金币、全部拿走与六格储藏室，绑定 Economy View 并连接 Shop | 两个 UI 和紧凑常驻区域 | 本地 HUD、Economy delegate |
| `UI/CombatShopWidget.h/.cpp` | 保留 v0.2 几何与节点交互；移除合成区滚动容器、辅助文本、反馈控件和结果 delegate 订阅，只保留配方图 | 落地 v0.3 极简合成区 | 本地 Slate/input、结果反馈表现 |
| `UI/CombatPlayerHUD.h/.cpp` | 分别创建/连接/销毁两个 Widget | 明确生命周期与弱互联 | 本地 HUD 初始化 |
| `Tests/CombatEconomyTests.cpp` | 先把旧单体结构断言改为双 Widget、尺寸/节点/解绑 Red，再实现到 Green | 防止 UI 再合并或尺寸回退 | Editor Automation |
| `10-15`、`00-01`、本 Spec | 同步当前 UI 行为、验证与未执行项 | DDD 与代码一致 | 文档 |

## 5. 验收标准（AC）

- [x] AC-01：HUD 创建两个不同的原生 Widget；商店初始关闭，储藏室常驻，EndPlay 后都移除且互联失效。
- [x] AC-02：储藏室按“全部拿走、金币”顺序显示，两者固定 112×34；六格固定为 42×32 横向矩形并左对齐；左键取出、右键出售和金币开关商店继续复用既有入口。
- [x] AC-03：Shop Widget 不再包含储藏/金币控件；目录按钮统一为 48×34 横向矩形，并使用 Icon 或 Glyph/名称回退。
- [x] AC-04：左键选择有配方物品后显示等尺寸的结果/直接组件节点和拥有数/需要数；无配方清除旧节点，v0.3 的留白表现由 AC-10 固化。
- [x] AC-05：目录与合成节点右键均调用现有购买意图；所有命中区域继续阻止世界输入，Escape 只关闭商店。
- [x] AC-06：相关 Automation、UE 5.8 Editor 构建、文档校验和差异检查通过；真实 PIE 几何/鼠标交互因当前会话无 UE Editor MCP 而明确记录为未执行。
- [x] AC-07：1920×1080 基准下，商店宽 456（原 760 的 60%、屏宽 23.75%）、高 842（屏高约 78%），右上锚定且顶部间距 54（屏高 5%）；右边距保持 28。
- [x] AC-08：物品区域固定高 594（屏高 55%），合成区域固定高 151（屏高约 14%）；物品区保留滚动，合成区固定裁切且不互相挤压，Automation 固化全部基准值。
- [x] AC-09：合成区域不构造 `SScrollBox` 或滚动条，151 固定高度及既有节点命中区域保持不变。
- [x] AC-10：合成区域只显示配方图；标题、选中物品/价格、选择/无配方提示、操作说明和交易反馈均不再构造。未选择或无配方时为 0 个内容槽，有配方时仅 1 个配方图槽。

## 6. Definition of Done

- [x] 结构测试先因配方节点未解析出临时目录资产而得到真实 Red，再完成双 Widget、尺寸、配方节点、重建状态和解绑断言的 Green。
- [x] 实现只复用现有 Economy/Controller 公共入口，没有客户端结算或第二套状态。
- [x] 新类型、函数和关键字段具备中文注释；蓝图可见字段有中文 `DisplayName`/`ToolTip`。
- [x] 直接 UI/Economy Automation 与 UE 5.8 Editor 构建通过；真实 PIE 未执行，资产验证按实际结果记录。
- [x] `10-15`、`00-01` 和本 Spec 与实现、验证事实一致。
- [x] `git diff --check` 通过，差异未回退或混入无关用户修改。
- [x] v0.2 先以布局 Getter 断言取得真实 Red，再实现固定百分比基准几何并完成直接 UI、完整 Combat、Editor 构建、文档与 delivery Gate 复验。
- [x] v0.3 先以滚动容器存在和配方槽数量断言取得真实 Red，再移除辅助 UI/结果订阅并完成 UI、完整 Combat、Editor 构建、文档与 delivery Gate 复验。

## 7. 测试矩阵与命令

| 层级 | 用例/命令 | 预期证据 | 结果 |
| --- | --- | --- | --- |
| 流程/文档 | `task_gate.py` preflight/plan/build/delivery；`validate_docs.py`；`git diff --check` | 四门、链接与空白卫生 | v0.2 preflight/plan/build、文档与 diff 已通过；delivery 最终结果见 §9 |
| Pure/Unit | 不新增结算算法；运行 `Combat.Economy.*` | 原交易、合成与储藏事务不回归 | `ECON002-Automation-Economy-Delivery/index.json`：8/8 成功，0 失败；边界命令用例按设计产生 5 条 warning |
| World Automation | `Combat.UI.Shop.*` | 双 Widget 结构、尺寸、节点、开关与 delegate 生命周期 | `ECON002-Automation-UI-Delivery/index.json`：1/1 成功，0 warning/error |
| v0.2 Layout Red/Green | `Combat.UI.Shop.*` 新增 1920×1080 基准比例、面板和区域固定几何断言 | 先证实旧 760×470/底部锚定为 Red，再验证 456×842、顶部 54、物品 594、合成 151 | `ECON002-v02-Automation-UI-Red/index.json` 1/1 失败且 6 个目标差异；`ECON002-v02-Automation-UI-Green/index.json` 1/1 成功 |
| v0.3 Recipe-only Red/Green | `Combat.UI.Shop.*` 断言无合成滚动容器、空态 0 槽、有配方 1 槽、无配方 0 槽 | 先记录 v0.2 滚动容器与辅助文字造成的槽数差异，再验证极简配方区 | `ECON002-v03-Automation-UI-Red/index.json` 1/1 失败且 4 个结构差异；`ECON002-v03-Automation-UI-Delivery/index.json` 1/1 成功 |
| Build | UE 5.8 `ue_gasEditor Win64 Development` | 新 UCLASS/UHT/Slate 编译成功 | 通过；最终增量构建 10/10 actions，`Result: Succeeded` |
| PIE / Blueprint | Demo 中查看右下储藏室、商店、左/右键和 Escape | 几何与真实输入符合预览 | 未执行：当前会话没有 UE Editor MCP；未改 Widget Blueprint 或其他资产 |
| Network / Dedicated | 本次不改网络载荷；风险相称时复用 Economy RPC 自动化 | 现有 owner-only/RPC 语义无回归 | 未单独运行 Target/Dedicated；完整 94/94 包含既有 RPC 重放、限频、所有权与载荷回归 |
| Soak / Perf | 小规模固定节点，无新 Tick/Schedule | 无独立性能 Gate | 不适用：纯事件驱动本地 UI |

## 8. 风险、回滚与升级

- 风险：Slate Brush 生命周期导致悬空资源；两个 Widget 重复绑定或销毁顺序错误；配方节点命中与目录节点重复；1920×1080 固定比例换算在其他 DPI/分辨率下产生裁切；固定物品/合成区内容溢出；搜索重建后旧命中引用残留。
- v0.3 追加风险：无滚动时超宽配方可能被固定区域裁切；删除反馈订阅时必须同步解除绑定代码，避免残留无用途 delegate。
- 回滚方式：恢复原 `UCombatShopWidget` 单体布局，删除 Stash Widget 和 HUD 接线，并同步还原测试/文档；经济与资产数据无需回滚。
- 触发升级的条件：需要新 RPC/复制字段、跨容器拖放语义、资产 schema、发布契约或无法通过三轮构建/测试收敛。
- 需要人决定的问题：无；用户已确认预览、按钮顺序、固定尺寸、左对齐和合成图结构。

## 9. 交付证据

- v0.3 Red / Green：`Saved/CombatEconomy/ECON002-v03-Automation-UI-Red/index.json` 为真实 Red，v0.2 实际仍有滚动容器，初始/有配方/无配方内容槽分别为 1/3/2，而目标为无滚动与 0/1/0；实现后 `ECON002-v03-Automation-UI-Delivery/index.json` 为 1/1 成功、0 失败。
- v0.3 实现：合成固定区直接持有 `RecipeBox` 并 `ClipToBounds`，删除标题、选中物品/价格、空态、帮助、交易反馈及相应 `OnEconomyResult` 订阅；配方节点、箭头、组件连接符和左右键交互保持。
- v0.3 构建/回归：DebugGame Editor 完整构建通过，最终增量 4/4 actions；Development 先因当前打开的 Editor 锁定基础 DLL 而链接失败，随后使用 `-ModuleWithSuffix=Combat,90301` 完成同配置 10/10 actions 编译链接。`Saved/CombatEconomy/ECON002-v03-Automation-All-Delivery/index.json` 为 94/94 成功、0 失败（92 success + 2 expected-warning）。
- v0.3 资产：未修改二进制资产，因此未重复运行资产校验；沿用 v0.2 已记录的 31 assets、0 errors、0 warnings 作为未变基线，不将其宣称为本次新执行。
- v0.3 流程/文档：preflight、plan、build 与 delivery Gate 均通过；`validate_docs.py` 通过（77 个 Markdown、390 个本地链接、0 错误），`git diff --check` 退出码 0；delivery 为 69 个工作区变更文件、0 错误，报告位于 `Saved/CombatEconomy/ECON002-v03-delivery.json`。
- v0.2 Red / Green：`Saved/CombatEconomy/ECON002-v02-Automation-UI-Red/index.json` 为真实 Red，旧布局实际返回宽度比例 1.0、面板 760×470、等效顶距 432、固定区域 0/0，与期望 0.6、456×842、54、594/151 形成 6 个明确差异；实现后 `ECON002-v02-Automation-UI-Green/index.json` 为 1/1 成功、0 失败。
- v0.2 实现：以 `1920×1080` 和显式比例常量换算布局，商店从右下锚定改为右上锚定；物品区和合成区分别放入 594/151 高的 `SBox`，内部各自使用 `SScrollBox` 处理溢出。未修改经济数据、RPC、资产或储藏室。
- v0.2 构建/回归：UE 5.8.2 Editor Development 增量构建通过（4/4 actions）；`Saved/CombatEconomy/ECON002-v02-Automation-All-Delivery/index.json` 为 94/94 成功、0 失败（92 success + 2 expected-warning）。
- v0.2 资产校验：`Saved/CombatEconomy/ECON002-v02-AssetValidation.json` 为 31 assets、0 errors、0 warnings；命令行退出码 0。
- 代码/资产 diff：新增 `CombatStashWidget.h/.cpp`，重构 `CombatShopWidget.h/.cpp`，接入 `CombatPlayerHUD.h/.cpp`、`CombatPlayerController.cpp`、`CombatHUDWidget.cpp`，并更新 `CombatEconomyTests.cpp`；ECON-002 未新增或修改二进制资产。
- Red / Green：`Saved/CombatEconomy/ECON002-Automation-UI/index.json` 为真实 Red，`Recipe contains result and direct component nodes` 期望 2、实际 0；原因是 Automation 的瞬态 ShopData 条目未注册到 AssetManager。实现先复用当前目录中的对象再回退 AssetManager，随后 `ECON002-Automation-UI-Green/index.json` 1/1 通过。F2 后又增加打开状态重建与 112×34 操作入口断言，最终 `ECON002-Automation-UI-Delivery/index.json` 1/1 通过。
- 构建结果：安装版 UE 5.8.2 执行 `Build.bat ue_gasEditor Win64 Development <project> -WaitMutex -NoHotReloadFromIDE`，最终 10/10 actions，`Result: Succeeded`。
- Automation：`Saved/CombatEconomy/ECON002-Automation-Economy-Delivery/index.json` 为 8/8 成功、0 失败（7 success + 1 expected-warning）；`Saved/CombatEconomy/ECON002-Automation-All-Delivery/index.json` 为 94/94 成功、0 失败（92 success + 2 expected-warning）。warning 仅来自既有无效调试命令边界用例。
- 资产校验：`Saved/CombatEconomy/ECON002-AssetValidation-PostFix.json` 为 31 assets、0 errors、0 warnings；命令行退出码 0。
- 静态/F2：检查双 Widget、HUD/Controller 接线和输入命中；UI 只读取 `FCombatEconomyView`/DataAsset 并调用既有请求入口，没有直接写金币/库存、没有新 Timer/Tick、没有强引用环。发现并修复打开状态 Slate 重建被强制折叠的问题，新增回归后构建和完整 Automation 复验通过。
- v0.2 流程/文档：preflight、plan、build 与 delivery Gate 均通过；`validate_docs.py` 通过（77 个 Markdown、390 个本地链接、0 错误），`git diff --check` 退出码 0；delivery 为 69 个工作区变更文件、0 错误，报告位于 `Saved/CombatEconomy/ECON002-v02-delivery.json`。工作区仍包含同一批既有 ECON-001/ITEM 未提交修改，本任务未回退或重写它们。
- 未执行验证及原因：真实 Demo PIE 几何、点击、右键与 Escape 未执行，因为当前会话没有可用 UE Editor MCP，命令行 NullRHI Automation 不能代替屏幕级验收；Server/Client Target、Dedicated、cook/打包和长时 soak/perf 未执行，因为本任务不改变网络载荷、资产、结算或周期逻辑。
- 剩余风险：用户已验收目标视觉结果；当前自动化覆盖结构、固定尺寸、状态重建、配方清理和事务回归，但 Codex 未取得可回读的真实 PIE 日志，不同 DPI/分辨率下的观感仍属于后续兼容风险。

### Push-Ready 六层

| 层 | 结论 | 依据 |
| --- | --- | --- |
| Tests | PASS | v0.3 UI 1/1、完整 Combat 94/94，0 失败；未改资产 |
| Types/Build | PASS | UE 5.8.2 DebugGame Editor 及 Development 后缀模块均成功编译链接；基础 Development DLL 因打开的 Editor 文件锁未替换 |
| No Regression | PASS | 完整 Combat 回归覆盖现有战斗、物品、经济、输入和 HUD；两个 warning 用例均为既有预期边界 |
| Adversarial | PASS | 合成区无滚动且显式裁切，辅助 UI/结果 delegate 成组移除；权限、生命周期、Brush 持有和节点命中无未关闭发现 |
| DDD/Constraints | PASS | 双 Widget 仅做本地只读投影，复用服务器权威事务；10-15、台账与 Spec 已同步 |
| Decisions | PASS | 不改 RPC/schema/DefinitionId/发布契约或资产，无需新增 ADR |

本机执行命令：

```powershell
python -B Tools/task_gate.py --mode preflight --spec Doc/CombatSystem/Specs/ECON-002-compact-shop-stash-ui.spec.md --kind feature
python -B Tools/task_gate.py --mode build --spec Doc/CombatSystem/Specs/ECON-002-compact-shop-stash-ui.spec.md --kind feature
& '<UE_ROOT>\Engine\Build\BatchFiles\Build.bat' ue_gasEditor Win64 Development '<PROJECT>\ue_gas.uproject' -WaitMutex -NoHotReloadFromIDE
& '<UE_ROOT>\Engine\Build\BatchFiles\Build.bat' ue_gasEditor Win64 DebugGame '<PROJECT>\ue_gas.uproject' -WaitMutex -NoHotReloadFromIDE
& '<UE_ROOT>\Engine\Build\BatchFiles\Build.bat' ue_gasEditor Win64 Development '<PROJECT>\ue_gas.uproject' -WaitMutex -NoHotReloadFromIDE '-ModuleWithSuffix=Combat,90301'
& '<UE_ROOT>\Engine\Binaries\Win64\UnrealEditor-Win64-DebugGame-Cmd.exe' '<PROJECT>\ue_gas.uproject' -unattended -nop4 -nosplash -NullRHI -NoSound '-ExecCmds=Automation RunTests Combat.UI.Shop.;Quit' '-TestExit=Automation Test Queue Empty' '-ReportExportPath=<PROJECT>\Saved\CombatEconomy\ECON002-v03-Automation-UI-Delivery'
& '<UE_ROOT>\Engine\Binaries\Win64\UnrealEditor-Win64-DebugGame-Cmd.exe' '<PROJECT>\ue_gas.uproject' -unattended -nop4 -nosplash -NullRHI -NoSound '-ExecCmds=Automation RunTests Combat.;Quit' '-TestExit=Automation Test Queue Empty' '-ReportExportPath=<PROJECT>\Saved\CombatEconomy\ECON002-v03-Automation-All-Delivery'
python -B Tools/validate_docs.py
git diff --check
python -B Tools/task_gate.py --mode delivery --spec Doc/CombatSystem/Specs/ECON-002-compact-shop-stash-ui.spec.md --kind feature --report Saved/CombatEconomy/ECON002-v03-delivery.json
```

## 10. 变更记录

| 版本 | 日期 | 修改 | 原因 |
| --- | --- | --- | --- |
| 0.1 | 2026-09-16 | 冻结并完成双 Widget、紧凑固定尺寸、等尺寸合成节点和左右键交互 | 用户确认可视化并授权开始修改；实现未改变已批准范围 |
| 0.2 | 2026-09-16 | 以 1920×1080 为基准重新冻结百分比布局：宽 456、顶部 54、整体高 842、物品区 594、合成区 151 | 用户要求商店缩至原宽 60%、靠近顶部并固定上下区域；附件只作关系参考 |
| 0.3 | 2026-09-16 | 合成固定区移除滚轮和所有辅助文字，只显示配方图；空选择/无配方时留白 | 用户明确要求合成方式没有滚轮并去除多余文字 |

## 11. 路由、自评与 Reflect

### 路由记录

- 原始需求摘要：在既有 1920×1080 紧凑商店上移除合成区滚轮和多余文字，只显示配方图。
- 主 Skill：`combat-feature-development`
- 选择依据：涉及 C++ Native Widget、HUD 生命周期、Automation 与 DDD 的通用 UI 功能变更。
- 备选 Skill 与排除理由：`combat-skill-development` 不适用，本任务不修改玩家可施放技能。
- 路由置信度：`high`

### 交付自评

| 维度 | 权重 | 分数（0–5） | 证据与扣分原因 |
| --- | ---: | ---: | --- |
| 需求与 AC | 20% | 4.5 | 十项 AC 均落实并有结构/回归证据，用户已确认目标视觉结果；Codex 无可回读 PIE 日志 |
| 架构与权限 | 20% | 5.0 | 只读 View、既有 Controller 意图、服务器事务和弱互联边界均保持 |
| 实现与数据 | 20% | 4.5 | 配方区只保留节点图并移除滚动/辅助文字/结果订阅；超宽配方在固定区内的实际观感未实机调优 |
| 验证证据 | 20% | 4.0 | DebugGame Editor、Development 后缀模块及直接/全量 Automation 通过；v0.3 未改资产故未重复资产校验，真实 PIE、Dedicated/cook 未执行 |
| 文档与可观测性 | 10% | 5.0 | DDD、台账、Spec、Red/Green 和可回读报告路径同步 |
| 交付卫生 | 10% | 4.5 | 保留既有 dirty worktree、无生成物入 Git、执行差异/Gate 检查；用户已授权本地提交且未授权推送 |

- 计算总分：`4.5×20% + 5.0×20% + 4.5×20% + 4.0×20% + 5.0×10% + 4.5×10% = 4.6 / 5`
- 硬性封顶或未执行项：不触发硬性封顶；真实 PIE 是视觉验收缺口，运行中的 Editor 需重启加载新模块；Network/Dedicated/cook/soak 对本次纯 UI 增量未执行。
- 自评结论：`EVIDENCE_SUFFICIENT`
- 用户验收状态：`用户已验收`（2026-09-16）；用户明确确认“验收成功，提交吧”，授权本地 Git 提交，不包含推送授权。

### Reflect 与调优

- 观察与证据：v0.3 Red 精确显示滚动容器仍存在，初始/有配方/无配方为 1/3/2 个槽；同时 Development 基础 DLL 被用户正在运行的 Editor 锁定。
- 根因类别：`单次实现`
- 调整文件与预期收益：`CombatShopWidget.cpp` 让合成固定区直接持有配方图并裁切，删除辅助文本与结果 delegate；`CombatEconomyTests.cpp` 固化无滚动和 0/1/0 槽结构。构建改用 DebugGame 与 Development 后缀模块绕过文件锁，不中断用户 Editor。
- 回归验证：v0.3 DebugGame Editor、Development 后缀模块、UI 1/1 和完整 Combat 94/94 均通过；文档、diff 与 delivery 结果见 §9。
- 需要用户决定的问题：无。
