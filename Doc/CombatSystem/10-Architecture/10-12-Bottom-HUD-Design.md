# 10-12 底部居中 HUD：设计与实现

## 多单位查看与操作（CTRL-001）

HUD 现在观察本地 `GetInspectedUnit()`，与服务器 `GetCommandedUnit()` 主选分开。普通世界左键点击可查看满足公共目标规则的单位；点击自己的单位切换主选，Shift 点击增删、拖框与 Shift 拖框形成最多 8 个单位的选中组。选择不会转移 Owner，也不会停止旧英雄命令。无权单位的 HUD 显示“仅查看 · 无控制权”，技能快捷键、加点、物品菜单/拖放和商店提交不能误作用于旧英雄。

展示 schema 9 新增 `GetHUDInspectionView()`：服务器对白名单字段投影等级、三围及战斗属性、生命/法力、技能定义/等级/费用和物品定义/数量。技能 Spec、物品实例与修订、经验/技能点、冷却、锁定与 AutoCast 状态不公开。拥有者仍使用原 owner-only 快照；无 Owner 的木桩/AI 也能显示公开属性。详情明确标注冷却和操作状态未公开。公共快照与拥有者快照均只消费 ASC、成长和库存事实，不新增 gameplay 权威。

多人必须使用同版本构建。主选 RPC 尚未确认或 Owner 尚未复制时显示切换提示并阻止操作；观察目标变化会清详情、拖放状态和异步定义加载。Controller 每帧只修剪本地弱选择；Unit/Controller EndPlay 继续显式释放服务器绑定。选择框与绿/蓝/橙轮廓由本地 HUD 绘制，不复制。群体 Move/Attack/Stop、单英雄技能/物品与授权边界见 [CTRL-001](../Specs/CTRL-001-multi-unit-selection.spec.md) 和 ADR-067。

技能指示器接入见 [10-13](10-13-Skill-Indicators.md)：悬停显示可靠范围，左键点击技能槽沿 Q/W/E/R 入口施法，右键可固定详情；加点、详情和日志的实际几何阻止世界输入。HUD 活动行显示瞄准原因，服务器施法/引导优先。拥有者范围与物品快照沿用 ADR-053、ADR-055；公开查看使当前展示 schema 升为 9（ADR-067）；物品操作与字段见 [10-14](10-14-Item-System.md)。

> 2026-09-09：用户确认设计并明确要求开始实现。底部 HUD 已接入 Demo，验证状态以 [进度台账](../00-Project/00-01-Progress-Tracker.md) 为准；DEMO-901 将展示投影升级到 schema 4，决策 ADR-047、ADR-048。

## 小地图实现（HUD-MINIMAP-001；设计与实现基线）

> 状态：设计已审核，原生小地图已接入 Demo；本节不代表工程已经具备战争迷雾、敌情保密或多人 Ping 复制。实现证据与未覆盖项见 [HUD-MINIMAP-002 Spec](../Specs/HUD-MINIMAP-002-minimap-implementation.spec.md)，设计边界仍见 [HUD-MINIMAP-001 Spec](../Specs/HUD-MINIMAP-001-minimap-design.spec.md)，决策记录见 ADR-068。

小地图是本地 HUD 的只读空间投影，固定在画面左下角，和底部居中主面板共用底边。它只把已经允许客户端观察的地图、单位和表现事件转换为紧凑标记；不维护第二套单位位置、目标、可见性或 Order 状态。静态地图层只负责识别地形和路线，动态标记层只负责当前可见状态。

### 布局基准

| 项目 | 标准方案 | 窄视口方案 |
| --- | --- | --- |
| 小地图外框 | `220 × 220` 设计单位，正方形 | 缩小到 `184 × 184`，不改变纵横比 |
| 左/底安全边距 | 各 `12`，通过 viewport lower-left anchor 与主 HUD 共用视口底边 | 先按 DPI 缩放，再保持安全边距 |
| 主 HUD 间距 | 小地图右边与底部主面板左边至少留 `12` | 若空间不足先缩小小地图，不遮挡主 HUD |
| 外框与底色 | 深蓝黑外框、低对比内描边、轻微阴影 | 保留同一色板，不增加高亮边框 |
| 辅助信息 | 右上角 `N` 指北；左下角显示当前可见状态 | 不增加标题栏、搜索；小地图拖动直接平移本地镜头，地图内容固定 |

小地图不使用独立标题栏，不参与底部 HUD 的横向排版。当前标准版优先保证真实障碍、中央平台、稳定地标和单位标记可读；紧凑版隐藏低优先级静态特征和网格，保留主控、友军、可见敌方、目标和 Ping 标记。颜色必须和形状同时表达语义，不能只依赖颜色区分；小地图不显示任何文字。地图内容使用固定缩放视口，拖动不改变地图视角中心。

### 图层和标记语义

| 图层 | 视觉语义 | 数据/权限边界 |
| --- | --- | --- |
| 基础地形 | 深绿地表、低对比网格和边界 | Demo/Test 的 `Floor_0` 和 StaticMeshActor bounds 已由 UE MCP 回读；原生层绘制四面边界墙、方向掩体、四角掩体和中央十字/高台，不伪造道路或河道；只读表现，不成为 gameplay 事实源 |
| 地标与障碍 | 青蓝障碍轮廓、琥珀色中央平台/出生点/补给区 | 所有地形文字已移除；范围来自已回读的关卡静态几何，不驱动 NavMesh、Order 或寻路，不携带私有玩法数据 |
| 主控单位 | 青色菱形，叠加淡青色镜头视锥 | 来源是本地已就绪的 `CommandedUnit`；不因地图点击转移主选 |
| 友军单位 | 蓝色小圆点 | 仅显示客户端已有且允许展示的单位 View；不公开 owner-only 技能/物品字段 |
| 可见敌方单位 | 红色小圆点 | 只能消费现有公开可见性事实；当前没有战争迷雾能力，不声称实现敌情保密 |
| 防御塔/中立目标 | 琥珀色菱形 | 只显示公开的目标/场景标识；不附带私有冷却、库存或实例修订 |
| Ping | 琥珀色环和短暂扩散反馈 | 仅为交互预留；多人可见 Ping 需要单独的事件/权限 Spec，不能由 UI 直接复制 |

标记尺寸以 `8–15` 设计单位为基准，主控标记优先级最高；小地图悬停不显示额外操作提示行，保持地图可读性。示意预览中的地形、坐标、英雄、目标和颜色数值均为设计示例，不写入正式战斗定义。

### 交互意图

| 输入 | 结果 | 约束 |
| --- | --- | --- |
| 小地图左键按下 | 先把本地镜头定位到按下位置 | 复用 [10-16 相机](10-16-Dota-Camera-Movement.md) 的本地表现入口；不改变 Unit Transform、`CommandedUnit` 或服务器状态，并记录按下点作为拖动基准 |
| 小地图左键拖动 | 从按下位置继续平移本地镜头视角 | 超过移动阈值后按地图增量移动本地相机，释放不重复定位；底图、网格、标记和 `ViewCenterUV` 保持固定，不发 Move Order；镜头未越界时保持透视梯形，靠边只显示地图内的可见部分并继续跟随镜头 |
| 小地图右键 | 给当前选中组提交移动意图 | 沿现有公共 Move Order、Owner、RequestId、载荷和生命状态复核；不能直接移动单位 |
| `Alt +` 小地图左键 | 发送 Ping 的设计预留 | 在 Ping 事件和接收者范围获批准前只允许本地瞬态反馈或禁用，不宣称多人可见 |
| 小地图悬停 | 不显示额外操作提示行 | 不因悬停加载或暴露技能 Spec、物品实例、冷却、AutoCast、经济或库存私有字段 |

左键按下后先记录指针位置；移动超过阈值才进入地图平移，短点击仍执行镜头定位，`Alt+左键` 短点击仍生成本地 Ping。拖动期间 Widget 捕获指针并消费移动/释放，不能穿透到 Edge Pan、技能瞄准、群体框选或世界 Order；右键失败由既有 Order 回执显示，不能在 Widget 内重试旧请求。

### 数据、生命周期与实现边界

当前实现由 `ACombatPlayerHUD` 持有原生 `UCombatMinimapWidget`，C++ 提供按地图包匹配的边界、归一化坐标、当前镜头轮廓和允许显示的动态标记；`NativePaint` 提供无纹理时的暗色地表、网格、方向标记以及由真实关卡 bounds 整理出的静态地形几何，所有文字绑定和 `PaintTerrain` 文字绘制路径均已清空/禁用，小地图悬停也不弹出操作提示行。没有可靠纹理时会收起可选的白色 `MapSurface` Image，避免覆盖原生地形层。小地图外框使用 viewport lower-left anchor 和 `12` 安全边距，横跨视口的 HUD 承载层不参与避让；全屏 `UUserWidget` 根只负责承载子面板，不能作为整块阻挡区。内容使用固定 `ViewZoom` 和 `ViewCenterUV`，标记、纹理 UV 与点击反投影共享同一变换；镜头轮廓保留四个视口射线投影点的原始位置，未越界时是透视梯形，越过地图边界时只按地图表面矩形裁剪可见部分，不整体缩放或夹住中心。普通左键按下先定位到按下点，后续超过阈值的拖动按地图增量只更新本地镜头，释放不重复定位，地图内容保持固定。小地图折叠时由仍可见的主 HUD 触发布局恢复，覆盖首帧 DPI、视口变化和短暂几何未就绪。`MapSurface` 仍可绑定纹理，但 `MapStatus` 和其他 TextBlock 会被强制清空并收起；不需要资产才能运行。小地图不复制完整 owner-only HUD 快照，也不创建独立 Camera Pawn 或小地图 Actor。

静态地形由 Level Editor 的 `工具 → COMBAT 小地图 → 保存并生成小地图`、工具栏 `生成小地图` 或 `combat.Editor.GenerateMinimap` 命令生成。入口先保存当前关卡，再扫描可见 `StaticMeshActor` 的网格 bounds，优先用 `Floor`/`Ground`/`Terrain` 命名的最大水平几何确定范围，输出无文字的矩形边界/障碍/地标到 `Config/CombatMinimap.generated.json`；运行时按地图包读取，文件缺失或非法时回退 `CombatMinimapWidget.cpp` 内置向量表。改关卡后重复“保存并生成小地图 → 启动 PIE”即可刷新，不需要手工改 C++ 或重建模块。该流程不生成 PNG/RenderTarget、不修改地图资产，也不把小地图几何写回碰撞或导航。

静态地图层在 Widget 创建后加载一次；动态标记层只消费 `UCombatUnitViewComponent`/公开查看投影以及本地 `CommandedUnit` 状态。实现阶段若需要新的地图纹理、边界 DataAsset、公开 View 字段或可复制 Ping，必须新建实现 Spec，重新审查网络、资产、生命周期和版本影响。当前工程没有战争迷雾服务，任何“Fog”外观只能作为未接入能力的视觉预留，不能作为网络保密承诺。

换主控、切换查看目标、`BindingRevision` 或 `LifeGeneration` 变化时清理旧标记、提示和异步定义加载；新绑定建立后再投影。Unit EndPlay、HUD Destruct、视口重建和专用服务器路径必须释放 Widget/委托，不留下旧目标回调。静态层可以保持不变，动态层在新快照到达或现有 HUD 表现刷新节拍更新；该刷新只做表现，不执行周期 gameplay。

### 后续实现验收基线

- 1280×720、1920×1080 和四档 DPI 下，小地图通过左下锚点保持底边安全距离，不遮挡战斗记录、技能瞄准和底部主 HUD；窄视口按 `220 → 184` 缩放，横纵比保持 1:1；没有静态纹理时仍显示暗色地表、真实原型几何特征和低对比网格，且不出现文字。
- 验证主控/友军/可见敌方/目标/Ping 的形状和颜色语义、缺少纹理的回退、死亡/重生/换主控、Widget 重建和 EndPlay 清理。
- 验证左键短点击只定位本地镜头、左键拖动只平移本地镜头且地图固定，右键只进入既有 Move Order 安全链路；失败、重放、失去 Owner 和 HUD 捕获不能产生旁路移动。
- 验证镜头四角越过地图边界时只绘制地图内的可见部分，轮廓继续随相机移动；未越界时保持四点、非零面积和透视梯形。
- 实现战争迷雾、多人 Ping、地图资产和网络投影前，分别建立实现 Spec、资产回读、Server/Client/Dedicated 与性能证据；本设计文档不替代这些验证。

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


## DEF-004 防守个人强化面板（2026-10-16）

新增 `UDefenseBuildWidget` 由 DefenseHUD 本地持有，独立于底部英雄/技能/物品布局。中部显示种子开始或个人三选一，右侧展示已选卡与金币；Slate 面板的实际可见几何用于输入阻挡。DefensePlayerController 继承 Combat 输入并追加面板几何判断，旧 Demo Controller 行为不变。

私有 ChoiceComponent 用 COND_OwnerOnly 复制候选/已选/期限/结果。Widget 不读取服务器 Director 产生候选或改属性；按钮只提交带身份的 RPC，倒计时只由服务器校准时间显示。开发本地服务器开始面板可锁种子，完整种子复制按钮不向客户端提供。Widget 释放 Slate 引用，Controller/Director 退出清理私有状态与 Modifier 句柄。详细流程和证据见 [DEF-004](../Specs/DEF-004-defense-choice-flow.spec.md)。


## DEF-005 团队商店面板（2026-10-16）

`DefenseTeamShopComponent` 由防守 Controller 创建，按 `COND_OwnerOnly` 发布共同商品账本和个人购买回执。左侧独立可滚动面板显示六件商品、由本人支付的价格、剩余 0/1 或 1/1、购买者、当前生效与最后覆盖波；实际按钮只发送意图。库存和原有装备商店不变。Server 的 ShopRevision 随阶段开放/购买/到期改变，组件另有 UI Revision，失败回执与金币变化也触发局部刷新。商店局部刷新不重建中部种子输入/卡片按钮，不丢失已输入文本。面板的实际可见几何纳入原有输入隔离；金币不足、Boss 整备限定、已购和战斗中禁用分别呈现。详见 [DEF-005](../Specs/DEF-005-defense-team-shop.spec.md)。


## DEF-006：团队商品复用原商店（2026-10-16）

本节替代 DEF-005 的独立团队商店面板方案：`DefenseBuildWidget` 只保留种子/个人选择和已选构筑。防守 HUD 恢复 `UCombatShopWidget` 的右下金币按钮，并把原商店置于构筑提示上方；购物时暂收起右侧构筑列表，关闭后恢复，输入和种子文本不受商店刷新影响。

原商店增加“团队补给”分类，商品仍是 `UCombatItemData`；左键查看、右键购买沿用原交互。`FCombatEconomyView::ShopOffers` 显示领域可用性、剩余次数、领取者与到期波次，UI 不引用 Defense 类型。普通配方区的布局和操作不变。真实按钮/几何/消费验证见 [DEF-006](../Specs/DEF-006-reuse-shop-use-on-acquire.spec.md)。


### DEF-007 防守核心屏障反馈

原商店金币入口、基础物品分类和右键购买沿用DEF-006。防守顶部核心HUD增加蓝色屏障条及“神殿屏障”余量，数据只读FDefenseRunView schema3，不在客户端查询或创建Runtime。导演通过既有监控节拍投影服务器核心屏障；购买、清场和终局同步刷新，伤害耗尽最迟下次监控刷新。底部原HUD与通用经济表现schema不改变。


### DEF-008 援军状态反馈

援军印记仍通过原商店右键购买；ShopOffer.Status复用现有复制显示剩余援军次数、等待复活人数和护盾保护人数。通用提示把限购计数明确标为“剩余购买”，与救援次数区分。英雄援军护盾通过既有ModifierView/Buff展示及4秒截止时间呈现，不新增HUD或经济schema。
