# 经济、商店与物品合成：运行时入口与配置

ECON-001 在 ITEM-001 的唯一物品实例之上启用 `combat_v3_economy_rc1`。当前实现已经通过本地 Editor、Automation 和资产校验，用户游玩验收仍以 [Spec](../Specs/ECON-001-money-shop-crafting.spec.md) 和 [进度台账](../00-Project/00-01-Progress-Tracker.md) 为准。

## 1. 权威边界

- `ACombatGameMode` 选择 `UCombatEconomyData` 与唯一 `UCombatShopData`；服务器初始化时校验并冻结本局规则。
- `ACombatPlayerController` 持有 `UCombatEconomyComponent`。金币、经济修订、储藏修订和六格储藏处只在该组件中有权威状态，客户端只接收 owner-only `FCombatEconomyView`。
- `UCombatShopData` 是目录、页面、分类、价格和递归配方的唯一来源。客户端搜索和展示不能改变服务器购买计划。
- 英雄六装备/三背包与玩家六格储藏处是两个合成域；购买只在储藏处域内消费和自动合成，转入英雄库存必须经过库存公共接口。

## 2. 事务与生命周期

购买请求携带 `RequestId`、绑定代次、经济/储藏修订和稳定 DefinitionId。服务器先做权限、限频、重放、修订和空间检查，再生成购买计划；缺失叶子按递归价格收费，消费、生成最终实例、余额和修订在同一经济变更中提交。失败不会扣金币或吞组件。

出售使用精确实例 Handle/Revision；配置的全额退款窗口和其后的折扣值由 `EconomyData` 冻结。死亡不扣金币，复活不重置余额，控制单位切换不迁移经济状态。World teardown 先关闭入口，再停止被动收入 Scheduler、清空储藏和注销组件。

被动金币按 Scheduler 提供的绝对 World Game Time 计算累计值，避免 Tick 间隔或临时测试时间源造成重复/漏发。击杀奖励、调试设置金币、购买和出售都通过同一余额修订与结构化事件入口。

## 3. 商店 UI 与 Demo 资产

`ACombatPlayerHUD` 分别创建 `UCombatShopWidget` 和 `UCombatStashWidget`。储藏室常驻右下角：第一行是左侧“全部拿走”和右侧金币入口，两个入口固定为 112×34；第二行左对齐排列六个 42×32 储藏格。金币入口开关独立商店，储藏格左键取出、右键出售。两个 Widget 只观察同一份 owner-only Economy View，储藏室以弱引用连接商店，HUD EndPlay 先断开互联再移除两者。

商店初始关闭，打开后显示搜索、基础/升级页、两列分类目录和下方固定配方区。目录、结果和直接组件都使用 48×34 横向节点；优先显示物品 Icon，缺失时回退 Glyph/名称。左键选择节点并查看其配方，右键只向 owning Controller 提交稳定 DefinitionId 的购买意图；无配方时清除旧节点并留白。ECON-002 v0.3 的配方区不构造滚动容器，只保留结果节点、箭头、直接组件和连接符，不显示标题、价格、提示、操作说明或交易结果文字。商店、储藏室及其配方区域都会阻止世界点击，Escape 只关闭商店并恢复游戏焦点。Slate 重建保持当前开关状态，两个 Widget 重建/销毁时释放 Brush 引用并解绑经济委托。

ECON-002 v0.2 以 1920×1080 为布局基准，不从参考截图读取绝对像素：商店内容宽由 v0.1 的 760 按 60% 缩为 456（屏宽 23.75%），右上锚定，顶部间距为屏高 5% 的 54，内容高按屏高 78% 四舍五入为 842。物品区固定为屏高 55% 的 594并保留滚动，合成区固定为屏高 14% 四舍五入后的 151；v0.3 将合成区改为无滚动的固定裁切区域，两区仍不会互相挤压，右边距保持 28。

Demo 资产仍位于 `/Game/Combat/Demo/Economy` 和 `/Game/Combat/Demo/UI`，由 `BP_CombatDemoGameMode` 引用唯一规则和目录。`Tools/setup_economy_demo.py` 用于重复生成/回读示例配置；ECON-002 没有新增或修改资产，也没有场景商店 Actor、客户端价格判断或第二套余额。

## 4. 验证入口

| 检查 | 当前证据 |
| --- | --- |
| UE 5.8 Editor Development | 最终增量构建 10/10 actions，`Result: Succeeded` |
| Economy 逻辑、安全和调试命令 | `ECON002-Automation-Economy-Delivery/index.json`：`Combat.Economy.` 8/8，0 失败 |
| Shop / Stash Native Widget | `ECON002-Automation-UI-Delivery/index.json`：`Combat.UI.Shop.` 1/1，覆盖双根、固定尺寸、配方节点、重建状态与解绑 |
| 全量回归 | `ECON002-Automation-All-Delivery/index.json`：`Combat.` 94/94，0 失败 |
| 资产/DataAsset | `ECON002-AssetValidation-PostFix.json`：31 assets、0 errors、0 warnings |

真实 Demo PIE 几何/鼠标输入未在 ECON-002 命令行会话执行，仍需用户可视验收；Server/Client Target、Dedicated 双客户端、cook/打包和经济 soak/perf 未重跑。本次不改网络载荷、资产、结算或周期逻辑，Automation 结果不能替代这些未执行项。

## 5. 版本与回滚

发布契约固定 `ContractVersion=3`、`ReleaseId=combat_v3_economy_rc1`、物品与经济开关分别为 true，旧合并字段保持 false。回滚必须成组恢复 Economy/Shop 源码、Item 扩展、Controller RPC、Demo 资产、配置和文档，并同时回退客户端/服务器版本；不得只关闭开关而留下可写经济入口。
