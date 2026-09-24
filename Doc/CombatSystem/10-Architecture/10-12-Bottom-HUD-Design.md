# 10-12 底部居中 HUD：设计与实现

## 多单位查看与操作（CTRL-001）

HUD 现在观察本地 `GetInspectedUnit()`，与服务器 `GetCommandedUnit()` 主选分开。普通世界左键点击可查看满足公共目标规则的单位；点击自己的单位切换主选，Shift 点击增删、拖框与 Shift 拖框形成最多 8 个单位的选中组。选择不会转移 Owner，也不会停止旧英雄命令。无权单位的 HUD 显示“仅查看 · 无控制权”，技能快捷键、加点、物品菜单/拖放和商店提交不能误作用于旧英雄。

展示 schema 9 新增 `GetHUDInspectionView()`：服务器对白名单字段投影等级、三围及战斗属性、生命/法力、技能定义/等级/费用和物品定义/数量。技能 Spec、物品实例与修订、经验/技能点、冷却、锁定与 AutoCast 状态不公开。拥有者仍使用原 owner-only 快照；无 Owner 的木桩/AI 也能显示公开属性。详情明确标注冷却和操作状态未公开。公共快照与拥有者快照均只消费 ASC、成长和库存事实，不新增 gameplay 权威。

多人必须使用同版本构建。主选 RPC 尚未确认或 Owner 尚未复制时显示切换提示并阻止操作；观察目标变化会清详情、拖放状态和异步定义加载。Controller 每帧只修剪本地弱选择；Unit/Controller EndPlay 继续显式释放服务器绑定。选择框与绿/蓝/橙轮廓由本地 HUD 绘制，不复制。群体 Move/Attack/Stop、单英雄技能/物品与授权边界见 [CTRL-001](../Specs/CTRL-001-multi-unit-selection.spec.md) 和 ADR-067。

技能指示器接入见 [10-13](10-13-Skill-Indicators.md)：悬停显示可靠范围，左键点击技能槽沿 Q/W/E/R 入口施法，右键可固定详情；加点、详情和日志的实际几何阻止世界输入。HUD 活动行显示瞄准原因，服务器施法/引导优先。拥有者范围与物品快照沿用 ADR-053、ADR-055；公开查看使当前展示 schema 升为 9（ADR-067）；物品操作与字段见 [10-14](10-14-Item-System.md)。

> 2026-09-09：用户确认设计并明确要求开始实现。底部 HUD 已接入 Demo，验证状态以 [进度台账](../00-Project/00-01-Progress-Tracker.md) 为准；DEMO-901 将展示投影升级到 schema 4，决策 ADR-047、ADR-048。

## 1. 已确认的布局

HUD 固定在游戏画面底部居中，采用参考图中的紧凑横向布局。左侧为英雄头像与叠加信息，中间为技能和资源条，右侧为常驻物品与背包槽；Buff/Debuff 位于主面板上方。

| 区域 | 定稿要求 |
| --- | --- |
| 英雄头像 | 充满左侧区域，尺寸保持紧凑；属性信息叠加在头像右侧，英雄名称位于左上方 |
| 等级与经验 | 等级数字压在头像左下角；数字外侧的圆形进度环顺时针表示当前等级经验百分比，不另设经验条 |
| 技能 | 中间单行 Q/W/E/R 四槽，显示图标、快捷键、技能等级、法力消耗和冷却状态 |
| 生命与法力 | 位于技能下方；只显示当前值 / 最大值以及右侧恢复速率，不显示“生命”“法力”标题 |
| 主物品栏 | 常驻六格，三列两行；每格为约 3:2 的横向长方形，不显示“物品”标题 |
| 背包栏 | 位于六格物品下方，常驻三个横向小格；与上方三列对齐，格子更扁、底色更暗 |
| Buff / Debuff | 主面板上方的紧凑图标行；显示剩余时间、无限持续标记和叠层，增益与减益分组 |

技能与物品的顶边对齐。主物品栏和背包栏共同占满右侧高度，背包底边与法力条底边对齐，避免右下方出现无用途的大块留白。

技能、物品到面板上边缘的内间距，必须与法力条到面板下边缘的内间距一致。前摇、引导和阻断提示放在面板上方，不挤占这一内间距。

## 2. 比例基准

下表记录已确认预览在常规宽度下的布局数值，作为 UMG 布局换算参考。它们是设计尺寸，不要求在不同 DPI 下保持相同物理像素数。

| 项目 | 设计基准 |
| --- | --- |
| 主面板 | 约 700 宽、155 高，底部居中 |
| 头像区域 | 约 144 宽、153 高，贴满左侧内边界 |
| 技能图标 | 约 68 × 68，四槽横排；槽间距 9 |
| 主物品格 | 约 67 × 44，三列两行；行列间距 5 |
| 背包小格 | 约 67 × 28，三格横排；列间距 5 |
| 主物品与背包间距 | 8 |
| 主面板上下内间距 | 均为 12，不含边框 |
| 生命 / 法力条 | 各高 21，两条间距 4 |
| 等级经验徽章 | 直径 36，距头像左边与下边各 8 |

字体、图标和倒计时需在目标游戏分辨率及 DPI 下保持清晰。缩放不得把横向物品格拉成竖向格，也不得放大头像来填补高度。

## 3. 显示与交互范围

- 技能、Buff 和英雄属性提供悬停详情；技能左键点击执行对应槽位输入，技能右键、Buff/属性点击可固定详情，关闭按钮或 Escape 取消固定。
- 冷却以遮罩与剩余秒数显示；法力不足、沉默、阵亡等状态使用明确的阻断表现。界面显示就绪不代表服务器一定接受施法。
- 技能快捷键与 HUD 点击共用现有输入槽及映射。目标技能点击槽位后进入本地瞄准，世界左键确认；无目标技能立即提交。
- 可切换 AutoCast 的被动技能占用普通技能槽；按对应快捷键或左键点击请求服务器原子翻转开关。瞄准期间右键继续取消本地会话，未瞄准时技能右键固定详情。
- HUD 技能左键沿统一 Controller 入口排到下一帧，避开从世界视口切换到 GameAndUI 控件时的 `FlushPressedKeys`；因此右键取消指示器后，换另一个技能单击即可进入新瞄准，不需要用第二次点击唤醒输入。排队期间右键、Escape 或其他显式取消仍会清除请求。
- 等级与经验已接入服务器权威成长快照；六格物品和三个背包格绑定服务器库存快照，金币与当前库存投影通过 owner-only 经济快照接入，规则和事务见 [10-15](10-15-Economy-Shop-Crafting.md) 与 ECON-003 Spec。
- 预览中的英雄、图标、等级、经验和技能数值均为示例，不写入正式战斗定义或当作平衡配置。
- 物品与背包空槽始终保留；ITEM-001 已接入使用、菜单、拖放换槽和场景丢弃；经济扩展提供右下金币入口、全局商店、直接库存交付、自动合成、出售和锁定。真实 WBP 几何与输入仍需 PIE 验收。

## 4. 当前工程入口

沿用 [头顶 UI 的分工](10-11-Overhead-Blueprint-UI.md)：C++ 负责只读数据适配与生命周期，Widget Blueprint 负责布局和视觉。HUD 观察 `ACombatPlayerController::GetInspectedUnit()` 指定的本地查看单位；被占有的 Command Pawn 是相机载体，不能据此推断英雄。HUD 使用可选 `BindWidget` 接线，与头顶 UI 的事件接口并存。

| 资产 | 作用 |
| --- | --- |
| `/Game/Combat/Demo/UI/WBP_CombatHUD` | 主布局，继承 `UCombatHUDWidget`；宽 700、高 156 的主面板，外部留出 Buff、施法及详情区域 |
| `/Game/Combat/Demo/UI/WBP_CombatHUDSkill` | 四个技能槽共用，继承 `UCombatHUDSlotWidget` |
| `/Game/Combat/Demo/UI/WBP_CombatHUDBuff` | 动态 Buff / Debuff 图标，继承 `UCombatHUDSlotWidget` |
| `/Game/Combat/Demo/UI/BP_CombatPlayerHUD` | 继承 `ACombatPlayerHUD`，默认 `WidgetClass` 指向主 HUD |
| `/Game/Combat/Demo/Framework/BP_CombatDemoGameMode` | `HUDClass` 指向上述 HUD Actor |
| `/Game/Combat/Demo/UI/Art/T_HUDRangerPortrait` | 游侠示意头像；源图及生成提示词见 [美术说明](../../../SourceArt/HUD/README.md) |

Designer 中使用底边锚定的 ScaleBox，按 UE DPI 规则显示，狭窄区域仅缩小。物品格实际为 `67 × 44.5`，背包格 `67 × 28`；技能、物品上边以及法力、背包下边距外边缘均为 13（12 内边距 + 1 边框）。左侧头像为 `143 × 154`，等级环为 `36 × 36`。

可直接在主蓝图的“定义图标”映射中按 `FPrimaryAssetId` 指定 Unit / Ability / Modifier 纹理；缺少纹理时用名称首字占位。当前只配置 `CombatUnit:ranged_combat_player` 的示意头像，技能与效果保留首字回退。Demo 当前只授予一个可切换 AutoCast 的被动技能，Q 显示“霜冻之箭”及“自动/关闭”状态，W/E/R 为空；界面不额外授予技能。快捷键文案对应当前默认 Q/W/E/R 映射，修改输入映射时需同步 HUD 的显示文案。

保留下列控件名即可使用原生绑定；省略可选控件不会报错，重命名后须同步 C++ 绑定名。

| 蓝图区域 | 绑定名 |
| --- | --- |
| 主布局与头像 | `HUDPanel`、`HUDFrame`、`HeroPortrait`、`HeroSymbol`、`HeroNameText`、`StatsText` |
| 资源 | `HealthBar`、`ManaBar`、`HealthText`、`ManaText`、`HealthRegenText`、`ManaRegenText` |
| 等级与经验 | `LevelText`、`ExperienceRing`；可选 `ExperienceText`、`AbilityPointsText`，内容来自服务器成长快照 |
| 技能与效果 | `SkillQ/W/E/R`、`BuffPanel`、`BuffOverflowText`、`ActivityText` |
| 详情 | `DetailPanel`、`DetailText`、`CloseDetailButton` |
| 技能 / Buff 子控件 | `IconImage`、`SymbolText`、`HotkeyText`、`CostText`、`CountText`、`StackText`、`RankText`、`CooldownShade`、`BlockedShade`、`DurationRing`；技能槽可选 `UpgradeButton`，缺失时 C++ 在 Panel 根节点动态创建；两种槽按需要提供其中部分 |

## 5. 数据与生命周期

`UCombatUnitViewComponent` 的公共 View 继续提供生命、法力、生命代次、可见状态、施法阶段及 Modifier FastArray。`FCombatHUDOwnerView` 以 `COND_OwnerOnly` 复制英雄等级、累计经验、当前等级经验、升级所需经验、经验进度、未使用技能点，以及 Strength/Agility/Intelligence、主属性、资源、攻击、防御、恢复、增幅、抗性和距离等全量战斗属性；同时复制最多四个直接输入技能的 Spec 句柄、稳定定义 ID、等级、费用、已提交冷却结束时间、冻结时长、AutoCast 切换语义与可升级标志。纯被动技能隐藏，`Passive + AutoCast` 与主动技能按 AbilitySpec 授予顺序占槽。属性快照扩展使展示 schema 从 7 升至 8。

服务器每 0.1 秒采样展示数据，仅在内容改变时更新快照；该 Tick 不执行 gameplay。客户端先核对单位定义与生命代次，再按本地输入使用的 Spec 顺序匹配技能，复制未齐时留空。冷却使用校准服务器时间推进本地遮罩，不重算旧冷却，也不因 UI 倒计时归零而移除 Buff。失去拥有权时公共读取入口屏蔽旧缓存。

`ACombatPlayerHUD` 只在本地创建主 Widget，专用服务器跳过创建；鼠标输入由 GameAndUI 模式先交给界面，未处理的输入继续交给原 Enhanced Input。换单位显式移除委托，换生命清除固定详情与 Buff 子控件；Widget 重建恢复一次订阅，Unit EndPlay 清空观察目标，HUD EndPlay 移除视口控件。定义异步加载同时校验绑定版本与 `LifeGeneration`，旧回调不能写回新英雄。

2026-09-09 的 HUD 生命周期审计作为当前扩展记录保存在本节：

| 创建者 | 持有关系 | 正常刷新 | 终止与切换清理 | 旧回调隔离 |
| --- | --- | --- | --- | --- |
| 本地 `ACombatPlayerHUD` | 强持有主 Widget；Widget 弱观察 `InspectedUnit` / View，强持有效 Buff 子控件 | View 变化或本地显示刷新 | 换单位解绑；换生命清空详情与子控件；Widget Destruct / Unit EndPlay 取消加载并移除委托；HUD EndPlay 移除视口控件 | `BindingRevision + LifeGeneration`；专用服务器不创建 Widget |

`UCombatProgressionComponent` 保存服务器权威等级、经验和技能点。累计经验阈值采用 `XP(n)=100*(n-1)*(n+2)/2`，默认上限 30 级；单位定义可配置初始等级、等级内经验和击杀经验奖励，致死伤害完成死亡转换后把奖励发给实际击杀者。技能加点通过 owning client 的可靠请求进入服务器，服务器检查技能点、英雄等级、技能上限和生命状态。物品、背包和库存由 ITEM-001 接入，经济由 `UCombatEconomyComponent` 持有并单独复制金币/当前库存快照。成长字段在 schema 5 引入，范围字段在 schema 6 引入，物品与经济展示使用当前 v4/schema 9 约束，经济表现 schema 为 3，要求服务器和客户端使用同版本。

## 6. 确认与验证

设计预览已检查常规宽度与窄宽度布局：六个主物品格及三个背包格均为横向长方形，无水平溢出；常规宽度下技能、物品顶部以及法力条、背包底部到面板外缘均为 13（12 内间距 + 1 边框）。用户已确认该版视觉方案。

工程新增四项 `Combat.UI.HUD.*` 自动化，覆盖拥有者属性与冷却冻结、异常进度、真实 Widget Blueprint 接线及重建 / 换单位 / 致死伤害 / 重生 / EndPlay、缺蓝 / 沉默 / 无限与过期效果。完整 Combat 57/57、资产定义校验 7/7 已通过。

此前实际双玩家 Demo PIE 检查了常规与小窗口布局、客户端真实资源、头像叠加信息、常驻物品 / 背包，以及英雄和技能详情的悬停、点击固定、移出后保留、关闭按钮和 Escape。技能左键施法与技能右键详情由 `HUD-ABILITY-CLICK-001` 增量接入，直接 HUD/Input Automation 已覆盖；本轮真实 PIE 待用户复验。截图为 `Saved/BottomHUD/PIE-Client.png`。三 Target、Dedicated 与最终资产回读结果统一记录在进度台账和 `Saved/BottomHUD/Validation.md`；不将 PIE 视觉检查当作 Dedicated 网络证据。

2026-09-10 的 DEMO-901 回归将 Q 槽切换为霜冻之箭 AutoCast，并把展示投影升级到 schema 4。自动化覆盖槽位筛选、服务器原子翻转、无 `CastNoTarget`、权威开关投影和“自动/关闭”文案；最终 `Combat.*` 59/59。Dedicated 服务器与两个客户端分别验证 2/1/1 个 owner-only 技能快照，均为 Pass；容量夹具把英雄固有 Modifier 计入冻结总量后保持 64 Unit / 256 Modifier，预算为 Pass。证据见 `Saved/DrowRangerDemo/Automation-Full-Final2.log` 与 `Dedicated-Final2.log`。

## 7. 战斗记录窗口（HUD-LOG-001）

`/Game/Combat/Demo/UI/WBP_CombatLog` 继承 `UCombatLogWidget`，由 `BP_CombatPlayerHUD.LogWidgetClass` 创建在本地屏幕层 20。左上角入口设计坐标为 `(16,16)`、大小 `138×36`；展开窗口为 `840×540`，从入口下方开始，ScaleBox 按可用区域缩小并为底部 HUD 留空。入口与窗口的几何、文字、复选框、下拉框和滑条由 Designer 维护；原生只生成复用的富文本记录行，字体和主要颜色可在蓝图默认值编辑。

记录显示毫秒时间、蓝色来源、浅色目标、青色技能/伤害、绿色生命变化和紫色状态。名称来自稳定定义 ID，本地定义未加载时回退 ID，异步加载完成后刷新；富文本特殊字符转义。同名单位的下拉选项附带服务器实例编号；与“全部”或其他标签冲突时继续区分，确保选项不会覆盖彼此。已离场但正在筛选的实例保留选项。

攻击者、目标、伤害/治疗/技能/状态、非英雄和时间条件按 AND 组合。时间滑条吸附 `30/60/120/300 秒/全部`，全部仍受 512 条历史上限约束。物品复选框筛选拾取、丢弃、换槽与消耗；物品伤害和治疗仍属于对应分类。默认跟随最新；手动向上滚动暂停跟随，回到底部或勾选跟随恢复。关闭继续保存记录，打开时回到最新；右上角 × 或 Escape 关闭。窗口和入口处理鼠标事件，避免点击穿透至世界发出移动/攻击。

按住标题栏的鼠标左键可拖动记录窗口，入口位置固定；标题栏中的关闭按钮与“跟随最新”仍执行各自操作。`LogTitleBar` 是 Designer 中高度 48、横向拉伸的透明拖动区域，位于标题文字和按钮后方，悬停显示移动光标与中文提示。原生平移最外层 `LogScale`，使面板及其祖先的鼠标点击区域一起移动；锚点和设计尺寸不变。位移转换到玩家视口坐标，再以未移动的 Canvas 锚点和面板实际尺寸约束边界，兼容 DPI/ScaleBox 缩放并避免逐帧反馈。窗口关闭再打开保留本次 Widget 的位置；首次创建使用 Designer 原位，不写磁盘。视口缩小时重新约束位置。

拖动期间由窗口捕获对应用户的鼠标指针；松开左键、捕获丢失、Escape/关闭或 Destruct 结束拖动。主动取消只释放本 Widget 持有的那个指针，不干扰其他控件或玩家的捕获。正文和筛选区不能启动拖动，拖动过程不向世界发送 Order。

下列绑定为必需控件，重命名应同步原生声明并编译蓝图：

| 功能 | 控件名 |
| --- | --- |
| 入口与窗口 | `LogEntryButton`、`LogScale`、`LogPanel`、`LogTitleBar`、`CloseLogButton`、`LogScrollBox` |
| 单位筛选 | `AttackerCombo`、`TargetCombo` |
| 分类与跟随 | `DamageCheck`、`HealingCheck`、`AbilityCheck`、`ItemCheck`、`StatusCheck`、`NonHeroCheck`、`FollowLatestCheck` |
| 时间与提示 | `TimeRangeSlider`、`TimeRangeText`、`RecordCountText`、`EmptyStateText` |

生命周期审计（2026-09-12）：

| 创建者 | 持有关系 | 终止与重建 | 旧回调隔离 |
| --- | --- | --- | --- |
| PlayerController 构造 | 强持有日志组件；组件弱缓存世界单位、弱观察事件子系统 | 组件 EndPlay 移除事件委托、清空历史/单位缓存并通知观察者后清空委托 | 无 gameplay 计时器；只复制服务器追加的历史 |
| 本地 HUD BeginPlay | 强持有窗口；窗口弱观察日志组件、强持文字池与瞬态样式表 | 窗口 Destruct 解绑所有委托、取消加载并释放行；重建保留弱历史源并恢复一次订阅；HUD EndPlay 显式置空历史源并移除窗口 | 弱异步回调与 `BindingRevision`；过期请求不能写回新绑定；专用服务器不创建窗口 |

相关测试为 `Combat.UI.Log.*` 的筛选与格式、容量边界、真实事务与组件 EndPlay、真实蓝图控件与重建，以及四档 DPI、视口边缘和缩小后的位置恢复。工程和联网证据记录在 [HUD-LOG-001 Spec](../Specs/HUD-LOG-001-combat-log.spec.md)，本地拖动增量证据见 [HUD-LOG-002 Spec](../Specs/HUD-LOG-002-window-drag.spec.md)；不改写此前 HUD 验收结论。
