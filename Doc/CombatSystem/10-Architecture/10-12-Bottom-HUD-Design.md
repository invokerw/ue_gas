# 10-12 底部居中 HUD：设计与实现

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

- 技能、Buff 和英雄属性提供悬停详情；点击可固定详情，关闭按钮或 Escape 取消固定。
- 冷却以遮罩与剩余秒数显示；法力不足、沉默、阵亡等状态使用明确的阻断表现。界面显示就绪不代表服务器一定接受施法。
- 技能快捷键应与现有输入槽及映射一致。本次确认的是信息展示，不新增鼠标点击技能施法流程。
- 可切换 AutoCast 的被动技能占用普通技能槽；按对应快捷键请求服务器原子翻转开关。鼠标点击槽位仍只固定详情，不施放或切换技能。
- 等级与经验已接入服务器权威成长快照；六格物品和三个背包格仍显示占位，库存与经济玩法后续接入。
- 预览中的英雄、图标、等级、经验和技能数值均为示例，不写入正式战斗定义或当作平衡配置。
- 物品与背包空槽始终保留；本阶段不设计拖放、使用物品、交换槽位、商店或购买交互。

## 4. 当前工程入口

沿用 [头顶 UI 的分工](10-11-Overhead-Blueprint-UI.md)：C++ 负责只读数据适配与生命周期，Widget Blueprint 负责布局和视觉。HUD 观察 `Aue_gasPlayerController::GetCommandedUnit()` 指定的单位；被占有的 Command Pawn 是相机载体，不能据此推断英雄。HUD 使用可选 `BindWidget` 接线，与头顶 UI 的事件接口并存。

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

`UCombatUnitViewComponent` 的公共 View 继续提供生命、法力、生命代次、可见状态、施法阶段及 Modifier FastArray。`FCombatHUDOwnerView` 以 `COND_OwnerOnly` 复制英雄等级、累计经验、当前等级经验、升级所需经验、经验进度、未使用技能点，以及攻击力、护甲、魔抗、移速、恢复速率和最多四个直接输入技能的 Spec 句柄、稳定定义 ID、等级、费用、已提交冷却结束时间、冻结时长、AutoCast 切换语义与可升级标志。纯被动技能隐藏，`Passive + AutoCast` 与主动技能按 AbilitySpec 授予顺序占槽。

服务器每 0.1 秒采样展示数据，仅在内容改变时更新快照；该 Tick 不执行 gameplay。客户端先核对单位定义与生命代次，再按本地输入使用的 Spec 顺序匹配技能，复制未齐时留空。冷却使用校准服务器时间推进本地遮罩，不重算旧冷却，也不因 UI 倒计时归零而移除 Buff。失去拥有权时公共读取入口屏蔽旧缓存。

`ACombatPlayerHUD` 只在本地创建主 Widget，专用服务器跳过创建；鼠标输入由 GameAndUI 模式先交给界面，未处理的输入继续交给原 Enhanced Input。换单位显式移除委托，换生命清除固定详情与 Buff 子控件；Widget 重建恢复一次订阅，Unit EndPlay 清空观察目标，HUD EndPlay 移除视口控件。定义异步加载同时校验绑定版本与 `LifeGeneration`，旧回调不能写回新英雄。

2026-09-09 的 HUD 生命周期审计作为当前扩展记录保存在本节：

| 创建者 | 持有关系 | 正常刷新 | 终止与切换清理 | 旧回调隔离 |
| --- | --- | --- | --- | --- |
| 本地 `ACombatPlayerHUD` | 强持有主 Widget；Widget 弱观察 `CommandedUnit` / View，强持有效 Buff 子控件 | View 变化或本地显示刷新 | 换单位解绑；换生命清空详情与子控件；Widget Destruct / Unit EndPlay 取消加载并移除委托；HUD EndPlay 移除视口控件 | `BindingRevision + LifeGeneration`；专用服务器不创建 Widget |

`UCombatProgressionComponent` 保存服务器权威等级、经验和技能点。累计经验阈值采用 `XP(n)=100*(n-1)*(n+2)/2`，默认上限 30 级；单位定义可配置初始等级、等级内经验和击杀经验奖励，致死伤害完成死亡转换后把奖励发给实际击杀者。技能加点通过 owning client 的可靠请求进入服务器，服务器检查技能点、英雄等级、技能上限和生命状态。物品、背包、库存与经济仍为展示占位；核心 `combat_v1_rc1` 保持不变，展示 schema 5 要求服务器和客户端使用同版本。

## 6. 确认与验证

设计预览已检查常规宽度与窄宽度布局：六个主物品格及三个背包格均为横向长方形，无水平溢出；常规宽度下技能、物品顶部以及法力条、背包底部到面板外缘均为 13（12 内间距 + 1 边框）。用户已确认该版视觉方案。

工程新增四项 `Combat.UI.HUD.*` 自动化，覆盖拥有者属性与冷却冻结、异常进度、真实 Widget Blueprint 接线及重建 / 换单位 / 致死伤害 / 重生 / EndPlay、缺蓝 / 沉默 / 无限与过期效果。完整 Combat 57/57、资产定义校验 7/7 已通过。

实际双玩家 Demo PIE 检查了常规与小窗口布局、客户端真实资源、头像叠加信息、常驻物品 / 背包，以及英雄和技能详情的悬停、点击固定、移出后保留、关闭按钮和 Escape。截图为 `Saved/BottomHUD/PIE-Client.png`。三 Target、Dedicated 与最终资产回读结果统一记录在进度台账和 `Saved/BottomHUD/Validation.md`；不将 PIE 视觉检查当作 Dedicated 网络证据。

2026-09-10 的 DEMO-901 回归将 Q 槽切换为霜冻之箭 AutoCast，并把展示投影升级到 schema 4。自动化覆盖槽位筛选、服务器原子翻转、无 `CastNoTarget`、权威开关投影和“自动/关闭”文案；最终 `Combat.*` 59/59。Dedicated 服务器与两个客户端分别验证 2/1/1 个 owner-only 技能快照，均为 Pass；容量夹具把英雄固有 Modifier 计入冻结总量后保持 64 Unit / 256 Modifier，预算为 Pass。证据见 `Saved/DrowRangerDemo/Automation-Full-Final2.log` 与 `Dedicated-Final2.log`。
