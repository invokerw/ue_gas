# 经济、商店与物品合成：运行时入口与配置

ECON-003 在 ECON-001/002 的唯一物品实例和商店目录之上，将购买与出售收敛到当前主控单位物品栏。当前实现与验收证据以 [ECON-003 Spec](../Specs/ECON-003-direct-inventory-shop-lock.spec.md) 和 [进度台账](../00-Project/00-01-Progress-Tracker.md) 为准；运行时只有库存交易域。

## 1. 权威边界

- `ACombatGameMode` 选择 `UCombatEconomyData` 与唯一 `UCombatShopData`；服务器初始化时校验并冻结本局规则。
- `ACombatPlayerController` 持有 `UCombatEconomyComponent`。金币、经济修订和当前主控单位物品栏投影只在服务器侧有权威来源，客户端只接收 owner-only `FCombatEconomyView`。
- `UCombatShopData` 是目录、页面、分类、价格和递归配方的唯一来源。客户端搜索和展示不能改变服务器购买计划。
- 英雄六装备/三背包组成唯一活动交易和合成域；购买直接消费未锁定库存组件并把结果写回库存。锁定实例不参与购买抵扣或自动合成。

## 2. 事务与生命周期

购买请求携带 `RequestId`、绑定代次、经济/库存修订和稳定 DefinitionId。服务器先做权限、限频、重放、修订、存活主控单位和空间检查，再生成购买计划；缺失叶子按递归价格收费，消费、生成最终实例、余额和修订在同一经济变更中提交。失败不会扣金币或吞组件。

出售和锁定使用精确实例 Handle/Revision；解锁通过相同的服务器修订校验后，立即检查当前库存并复用稳定合成，只有完整配方才消费组件。配置的全额退款窗口和其后的折扣值由 `EconomyData` 冻结。死亡不扣金币，复活不重置余额，控制单位切换不迁移经济状态。World teardown 先关闭入口，再停止被动收入 Scheduler 并注销组件。

被动金币按 Scheduler 提供的绝对 World Game Time 计算累计值，避免 Tick 间隔或临时测试时间源造成重复/漏发。击杀奖励、调试设置金币、购买和出售都通过同一余额修订与结构化事件入口。

## 3. 商店 UI 与 Demo 资产

`ACombatPlayerHUD` 当前只创建 `UCombatShopWidget`（以及底部 HUD/战斗记录）。商店根节点右下角常驻金币按钮，商店面板初始关闭；按钮在关闭/打开状态都可点击并开关面板，命中区域阻止世界点击穿透。商店只观察 owner-only Economy View，其中拥有数来自 `InventoryItems`。

商店打开后显示搜索、基础/升级页、两列分类目录和下方固定配方区。目录、结果和直接组件都使用 48×34 横向节点；优先显示物品 Icon，缺失时回退 Glyph/名称。左键选择节点并查看其配方，右键只向 owning Controller 提交稳定 DefinitionId 的购买意图；成功结果直接进入当前库存并在顶层事务结束后自动合成。无配方时清除旧节点并留白。配方区不构造滚动容器，只保留结果节点、箭头、直接组件和连接符；Escape 只关闭商店并恢复游戏焦点。Slate 重建保持当前开关状态，Widget 重建/销毁时释放 Brush 引用并解绑经济委托。

ECON-002 v0.2 以 1920×1080 为布局基准，不从参考截图读取绝对像素：商店内容宽由 v0.1 的 760 按 60% 缩为 456（屏宽 23.75%），右上锚定，顶部间距为屏高 5% 的 54，内容高按屏高 78% 四舍五入为 842。物品区固定为屏高 55% 的 594并保留滚动，合成区固定为屏高 14% 四舍五入后的 151；v0.3 将合成区改为无滚动的固定裁切区域，两区仍不会互相挤压，右边距保持 28。

Demo 资产仍位于 `/Game/Combat/Demo/Economy` 和 `/Game/Combat/Demo/UI`，由 `BP_CombatDemoGameMode` 引用唯一规则和目录。`Tools/setup_economy_demo.py` 用于重复生成/回读示例配置；ECON-002 没有新增或修改资产，也没有场景商店 Actor、客户端价格判断或第二套余额。

## 4. 验证入口

| 检查 | 当前证据 |
| --- | --- |
| UE 5.8 构建 | 安装版 Editor、源码 Server/Client Development 三 Target 全部通过；日志和命令见 Spec §7 |
| 全量 Combat 回归 | `Saved/ECON003Validation/Automation/index.json`：94 success + 2 success with warnings、0 failed、0 not run，96 项；包含经济、安全、输入、物品、商店和 Release 用例 |
| PIE / 蓝图 | 安装版 NullRHI PIE 真实执行 1 轮经济闭环、5 回执、0 失败；5 个蓝图编译为 `BS_UP_TO_DATE`，规则/目录/HUD/商店类引用正确。`Saved/ECON003Validation/PIEReport.json`、`BlueprintReport.json` |
| Dedicated / 300 秒 soak | `Tools/RunDedicated.ps1 -InstalledEditor -Economy`：独立服务端和双客户端，两端各 10 轮、48 回执、0 失败/重试；M7 预算全部通过，循环后 Modifier/调度槽计数稳定。日志位于 `Saved/UEEnvironment/Dedicated-Installed-Economy/` |
| 物品与容量 | 单独运行 `Tools/RunDedicated.ps1 -InstalledEditor -Items`：争抢一胜一负、控制互换、旧 owner 隐私与旧绑定拒绝通过；64 Unit/256 Modifier 容量预算通过 |
| Windows cook / 资产 | 650/650 包 cook 成功、0 错误、退出 0；`CombatAssetValidation` 31 项、0 错误/警告、退出 0。`Saved/ECON003Validation/Cook.log`、`AssetReport.json` |

上述 PIE 使用 NullRHI；Dedicated 为安装版 Editor 的独立 `-server`/`-game` 进程。人工渲染几何/鼠标输入、打包可执行文件部署、超过 300 秒的 soak 和人工网络损伤未执行，不能用本轮结果代替。详细命令、首次失败及复验、既有日志问题见 Spec §7、§9。

## 5. 版本与回滚

发布契约为 `ContractVersion=4`、`ReleaseId=combat_v4_economy_rc1`、物品与经济开关分别为 true，旧合并字段保持 false；`EconomyPresentationSchemaVersion=3` 表示金币按钮与库存投影契约。回滚必须成组恢复 Economy/Shop 源码、Item 扩展、Controller RPC、配置和文档，并同时回退客户端/服务器版本；不得只关闭开关而留下可写经济入口。
