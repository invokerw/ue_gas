# 物品系统：配置、操作与权威边界

ITEM-001 在 Combat 单 Runtime Module 内增加物品实例、库存、地面拾取物和 HUD。当前物品能力作为 `combat_v3_economy_rc1` 的基础；物品自身规则仍见 [Spec](../Specs/ITEM-001-item-system.spec.md) 与 [进度台账](../00-Project/00-01-Progress-Tracker.md)，经济扩展见 [10-15](10-15-Economy-Shop-Crafting.md) 和 ADR-059。

## 1. Demo 操作

打开 `/Game/Combat/Demo/Maps/L_CombatDemo`。卓尔初始携带护甲指环、旅行靴、三份恢复药剂和雷击法杖；出生点附近预放六种样例物品。

| 操作 | 结果 |
| --- | --- |
| 右键地面物品 | 发出拾取指令，距离不足时走近，服务器到达后再次校验 |
| 左键装备栏主动物品，或按 1–6 | 无目标物品立即请求使用；目标物品进入既有技能瞄准流程 |
| 右键库存槽 | 打开使用、放到地面、移入背包/装备栏菜单 |
| 拖到另一格 | 原子交换两格；不会替换正在执行的移动指令 |
| 拖到场景，或菜单“放到地面…”后左键 | 请求到指定地面位置放下；超距时走近 |
| S、替换指令、死亡或控制转移 | 取消尚未完成的场景交互；旧导航回调不能完成旧事务 |
| Escape / 右键 | 取消瞄准或放置模式，消费本次鼠标手势 |

六个装备槽按三列两行显示，下面三个扁槽是背包。悬停显示说明、数量/能量、法力费用和操作提示；槽内显示冷却、重新启用等待、禁用或缺蓝原因。快捷键来自 `IMC_Default` 的 `IA_ItemSlot_1`–`6`，可改键；按下/松开快施复用 Controller 的施法模式。

## 2. 实例与位置

`UCombatItemData` 只保存定义。`UCombatItemSubsystem` 在服务器登记唯一 `UCombatItemInstance`；`UCombatInventoryComponent` 保存九个槽的句柄，`ACombatWorldItem` 复制地面投影。客户端不复制实例 UObject。

`FCombatItemHandle` 使用 64 位 Id 与 Generation，具有显式网络序列化；实例修订号用于比较并交换。世界结束使旧句柄失效；合并、末件消耗与销毁都不复用旧身份。库存修订号与来源/目标槽快照一起保护拖放。

| 位置 | 主动和被动 | 冷却推进 |
| --- | --- | --- |
| 装备槽 0–5 | 生效；从背包换入后等待 6 秒 | 1 倍 |
| 背包槽 6–8 | 禁用 | 0.5 倍 |
| 地面 | 禁用 | 0.5 倍 |

位置变更先结算之前一段时间，再切换速率。冷却、数量、能量、绑定和重新装备等待随同一实例保留；经过地面中转也不能规避背包等待。普通死亡保留库存与冷却，结束活动施法并撤销持有效果；复活只恢复有效装备的效果。配置死亡掉落时投放到脚下。

同定义堆叠采用整堆合并：数量不超过上限，双方无冷却/重新装备等待且绑定兼容才合并；无法完整合并时使用空格，库存满时保留地面物品。数量和能量独立，带能量的物品最大堆叠为 1。

## 3. 配置一个物品

在 AssetManager 扫描的 `/Game/Combat/Definitions/Items` 或 `/Game/Combat/Demo/Items` 创建 `CombatItemData`，使用唯一 `lower_snake_case` 的 DefinitionName，稳定身份为 `CombatItem:<name>`。字段提供中文 Details 名称与提示，Editor 校验和服务器授予共用定义校验。

| 配置 | 约束与用途 |
| --- | --- |
| DisplayName / Description / Glyph / Tint / Icon / WorldMesh | 名称、说明和表现；没有纹理时使用简称和颜色 |
| MaxStack / QuantityPerUse | 堆叠 1–99；每次消耗数量为 0 表示保留物品 |
| InitialCharges / ChargesPerUse | 初始能量与每次能量消耗；不提供隐式自动充能 |
| bDestroyWhenChargesEmpty | 默认保留空能量物品与被动；开启后本次施法安全结束才移除 |
| ActiveAbility | Ability Blueprint 的 CDO 指向 AbilityData；每实例独立 Spec，不占 QWER，不使用英雄技能点 |
| Passives | 无限、不可驱散的 Modifier；属性走 GE，Hook 使用 Modifier Runtime |
| UniqueGroup | 空名称可叠加；同组最靠前的有效装备槽提供该被动 |
| AuraModifier / Radius / Targeting | 独立光环子定义，使用既有 Aura 与 Team/Targeting 入口 |
| SharedCooldownGroup | 非空同名组在提交时同步携带物品冷却下限；不消耗其他实例的能量 |
| bMovementActive | 移动类主动额外受缠绕限制；位移仍通过 Motion |
| Sharing | 公开、绑定首次持有单位、首次持有者的友军；丢弃不清除绑定，原单位销毁也不解除绑定 |
| bCanDrop / bCanEnterBackpack / bDropOnDeath | 主动丢弃、进入背包与死亡掉落分别配置 |

单位定义 `InitialItems` 支持引用物品定义和初始数量，默认空数组。服务器动态授予使用 `Inventory.GiveItem`；投放到世界使用 `ItemSubsystem.SpawnItem`。关卡可直接放置 `ACombatWorldItem` 并设置物品定义与初始数量，不需要另写拾取蓝图。

样例均位于 `/Game/Combat/Demo/Items`：指环护甲 +5；鞋移动速度 +45、同组不叠加；药剂治疗 180、冷却 12 秒；法杖魔法伤害 160、法力 40、冷却 16 秒；守护雕像友军光环护甲 +3、半径 600 cm；雷纹刃普攻实际造成伤害后追加 25 魔法伤害。数值在 DataAsset 中配置。

## 4. 结算与生命周期

主动复用 Ability / Action / Targeting / Scheduler。`State.Silenced` 不阻止物品主动，`State.Muted` 阻止物品主动；眩晕、妖术、冻结、移出游戏和死亡仍阻断。被动不因普通物品禁用消失。库存位置、等待、数量/能量、法力、冷却和目标在服务器激活与提交时复核。

同款主动按实例授予独立 Spec，客户端普通技能 RPC 不能绕过物品 Order。数量和能量只在费用提交点扣除，冷却在配置的冷却提交点记录。最后一件不会在当前 Ability 调用栈中被销毁；结束后由 Scheduler 再验证实例并撤销 Spec。

同一阶段的物品数量、共享冷却和提交标记在法力 GE 广播属性变化前就绪；回调导致死亡时不会掉出尚未消耗的副本，也不会继续执行本次效果。放下时先交接地面归属再撤销被动，避免移除回调观察到半完成的槽位。

被动按实例保存精确 Modifier/Aura Handle，移出装备只撤销自己的效果。GE 聚合是最终属性唯一来源；提高 MaxHealth/MaxMana 不补充当前资源，降低上限会钳制当前值，反复换装不会补血回蓝。Hook 追加伤害使用 Item SourceContext 并继承 RootEventId，雷纹刃不会递归触发自身。

瞬时 GE 的基础生命/法力也遵守资源上限。满蓝自然回复不会积累界面不可见的基础法力，因此长时间停留后使用物品仍实际扣蓝。

Owner EndPlay 清理全部持有实例、Spec、被动、光环与等待任务；地面 Actor EndPlay 只删除仍由它持有的实例。拾取先完成权威所有权转移再销毁地面投影。世界 teardown 清空注册表与组件槽位。

## 5. 网络、导航与 HUD

客户端请求只携带完整物品身份、修订号、槽位或原始目标点。既有 owning Unit RPC 继续检查所有权、正 RequestId、批量大小、载荷、限频与重放窗口；客户端不能提交费用、数量、定义或命中结果。

Pickup / Drop 进入 Order 状态机，复用 AI 导航、路径跟随、Scheduler 和统一 Targeting；到达后比较物品修订号、归属、共享、距离、LOS 与地面合法性。拾取不预留物品，同步事务先到先得。Swap 是即时事务。活动施法的实例不能离开装备栏。

拥有者快照只复制给当前 owning client，包含九槽、独立 Spec、数量/能量、冷却检查点/速率、等待结束时间与修订号。HUD 使用估计服务器时间绘制倒计时，时间到零不会反向触发 gameplay。异步加载、鼠标按下、菜单和拖放都校验单位、LifeGeneration 与控制绑定代次。

批量接收回执和最终交互结果分开：接收成功仅表示订单已接受；导航结束后恰好发送一次最终结果。最终回执显式复制生命代次，防止旧生命或旧控制绑定的结果覆盖新 HUD 状态。

`WBP_CombatHUDItem` 继承 `UCombatHUDItemSlotWidget`，主 HUD 绑定 `EquipItem0`–`5` 和 `BackpackItem0`–`2`。布局保持 [10-12](10-12-Bottom-HUD-Design.md) 的 700 × 155 紧凑主面板；地面名称使用支持中文字体回退的屏幕空间 Widget。

## 6. 版本与迁移

| 契约 | 当前值 |
| --- | --- |
| ReleaseId / ContractVersion | combat_v3_economy_rc1 / 3 |
| GameplayTag / Combat Event | 3 / 3 |
| HUD View / 玩家日志投影 | 8 / 3 |
| Content / Formula / RNG | 1 / 1 / 1 |
| 能力开关 | bItemsEnabled=true，bEconomyEnabled=true；旧 `bItemsAndEconomy=false` |

旧 `bItemsAndEconomy` 是废弃兼容字段，固定 false。服务器和客户端要求同版本，不承诺 v1/v2/v3 混合连接；已有定义保持原 ID，旧 UnitData 的空库存继续有效。

`SourceContext` 与日志保留物品定义/实例以及操作后的数量/能量，实例销毁后仍可追溯。旧 JSON 事件使用离线迁移器：`python Tools/migrate_item_events.py old-events.json item-events-v2.json`，保留旧因果字段并补空物品来源；拒绝未知版本和覆盖现有输出。

## 7. 验证入口

直接 Automation 前缀为 `Combat.Items.`，覆盖实例/网络身份、冷却与重新装备、同款主动/最终消耗、竞争/满包/旧导航、死亡复活/光环/teardown、唯一组/资源上限、绑定死亡掉落和能量/共享冷却。测试地图的 `-CombatItemsSmoke` 通过真实客户端 RPC 执行使用、换背包、丢弃、拾取和最终回执验证，并让两端对同一地面快照竞争；随后服务器交换主控单位，客户端检查库存归属、旧拥有者展示清空与旧控制代次请求被拒绝。测试编排不进入产品权威入口。

本机配置 `.env` 后执行 `Tools/RunDedicated.ps1 -Items`，启动独立服务器与两个客户端，同时验证物品 RPC、owner-only 快照及既有 64 单位/256 Modifier 容量场景。三 Target、完整 Combat、资产、真实 PIE 与 Dedicated/容量的实际结果以 ITEM-001 Spec 记录为准。

源码 Editor 的启动入口不可用时，可用 `-InstalledEditor` 选择安装版 Editor 的独立 `-server`/`-game` 进程，日志单独写入 `Saved/UEEnvironment/Dedicated-Installed`。该运行证据不替代源码 Server/Client Target 构建，也不代表已验证打包后的可执行文件。
