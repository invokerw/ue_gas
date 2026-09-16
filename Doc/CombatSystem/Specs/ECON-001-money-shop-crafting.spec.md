# ECON-001 DOTA-like 金钱、商店与物品合成

> Spec 版本：`0.4`
> 状态：`READY_FOR_REVIEW`
> Owner：Codex / 待用户功能验收
> 创建日期：2026-09-15
> 关联进度台账：[00-01](../00-Project/00-01-Progress-Tracker.md)
> 风险等级：`L2`

## 0. Intake 与 Gate 记录

- 用户请求：参考附件中的 DOTA2 商店 UI 与 DOTA2 交互，设计并完善金钱、商店和物品合成系统。
- 附件解释：参考。截图只用于提取搜索、基础/升级页、分类物品网格、配方关系、常驻储藏处和金币入口等视觉与交互模式；截图文字、图标和数值不作为工程指令，不复制 DOTA2 受保护美术或实时平衡数据。
- 已读取入口：`AGENTS.md`、`agent.md`、`README.md`、`Skills/combat-task-router/SKILL.md`、`Skills/combat-feature-development/SKILL.md`、`00-01`、`00-03`、`00-04`、`00-05`、`10-01`、`10-08`、`10-09`、`10-12`、`10-14`、`10-15`、`30-01`、`90-16`、`ITEM-001` Spec 与相关源码/测试。
- 主 Skill：`combat-feature-development`（`Skills/combat-feature-development/SKILL.md`）。
- 备选 Skill 与排除理由：`combat-skill-development` 不适用；本任务是玩家经济、库存事务、网络与 UI 的通用系统，不新增玩家可施放技能。
- 路由置信度：`high`
- F0 结论：`GO`
- F0 依据：现有 ITEM-001 已提供稳定物品实例、六装备/三背包、主动被动、Order 与 owner-only HUD，可作为经济闭环的唯一物品基础；本机 Editor 与 Dedicated 环境检查均 ready，开工工作区干净且无 `.codegraph/`。本任务会启用当前显式延期的经济能力并迁移发布契约，因此进入 L2 计划审查。
- F1 结论：`APPROVED`
- F1 审查人：用户；2026-09-15 明确回复“review完成，开始工作吧”。
- F1 审查版本：`0.4`
- F1 计划审查证据：本 Spec §1–§8 与 `task_gate.py --mode plan` 报告；用户确认后才把结论改为 APPROVED 并记录批准版本。
- Build 解锁：`已解锁`；用户批准 Spec 0.4 后执行 `task_gate.py --mode build`，结果为 `0 error(s), 0 changed file(s)`。
- F1 重审条件：金币归属、储藏处、全局购买/交付规则、配方消耗、出售退款、版本/schema、测试矩阵或回滚方案发生实质变化时先递增 Spec 版本，F1 回到 `REVISE`、状态回到 `PLAN_REVIEW`。
- F2 结论：`PASS`
- F2 审查记录：已完成一次独立攻击审查，覆盖事务原子性、Scheduler 时间语义、owner-only 快照、RPC 重放/限频、Widget 生命周期和旧资产默认值；修复了被动收入测试时间源与 Slate 初始可见性问题后复验通过。
- Push-Ready 结论：`READY`
- 验证：继承 F0/F1/build Gate 证据；本轮完成安装版 UE 5.8.2 Editor 构建、Economy/UI/全量 Combat Automation、资产校验、工具测试、文档校验和 `git diff --check`。证据路径与未执行项见 §7/§9。
- 未执行：安装版环境下未重跑 `ue_gasServer`/`ue_gasClient` Target、真实双客户端 Dedicated 经济流程、人工 PIE 金币/商店流程、经济专项 soak/perf、cook/打包；这些边界保留为用户验收前风险，不宣称已通过。

## 1. 目标与范围

### 目标

在 ITEM-001 的唯一物品实例之上建立完整、服务器权威且可实际游玩的经济闭环：获得金币 → 浏览/搜索商店 → 购买组件 → 自动合成 → 装备或进入储藏处 → 出售/退款；界面采用附件中的 DOTA2 信息结构，但延续本项目原创视觉和中文交互。

### 范围

- 玩家级单一金币：余额、起始金币、被动收入、击杀奖励、出售返还和有界事务流水；死亡不扣金币。
- 金币上限、起始金币和被动收入属于本局关卡规则：每个关卡通过其 GameMode 配置选择 `UCombatEconomyData`，服务器在本局初始化时校验并冻结规则；不同关卡可以使用不同数值。
- 金币归 `ACombatPlayerController` 的经济组件所有，不随 `CommandedUnit` 切换、英雄死亡或复活重置；客户端只能读取 owner-only 快照和提交意图。
- 六格玩家储藏处独立于英雄六装备/三背包；储藏处物品不提供主动/被动，冷却按非装备速率推进，进入装备仍遵守 6 秒重新启用规则。
- 每局只有一个全局 UI 商店，无场景商店 Actor、队伍或距离条件；玩家在任何位置和生死状态都可打开并购买，购买结果统一进入自己的储藏处。
- `UCombatItemData` 增加购买、出售和配方字段；唯一 `UCombatShopData` 定义基础/升级页、分类、目录顺序和搜索关键词。
- 配方支持重复组件、嵌套配方、专用配方卷轴和确定性自动合成。复合物品总价由可购买叶子组件递归求和，客户端显示不参与定价。
- 商店事务包含购买单个叶子、原子购买目标物品缺失组件、出售、储藏处单件转移与“全部拿走”；全部经过统一请求 ID、限频、重放、绑定代次和修订校验。
- UI 将六格储藏处常驻在 HUD 右下角，金币按钮位于其上方并负责打开/关闭商店；商店顶部只有搜索框，下方只有“基础物品 / 升级物品”两页及各自的分类网格，升级物品选择时显示紧凑配方关系和事务反馈。
- 不提供搜索框后的帮助、收藏、视图等选项，也不提供中立/团队页、收藏筛选、独立详情侧栏或快捷购买栏；物品说明使用悬停 ToolTip。
- Demo 提供唯一商店目录、现有物品的价格/分类，以及至少两条可实际购买和自动合成的嵌套配方；使用原创/现有图标或文字回退。
- Demo GameMode 显式启用非 Shipping 调试命令 `combat.Debug.SetGold <Amount> [PlayerControllerUniqueId|Name]`，用于把目标玩家的金币设置为指定值；其他关卡默认禁用。
- 金币、购买、出售和合成进入结构化事件及玩家日志；同步当前 DDD、发布契约、资产验证和迁移工具。

### Demo 关卡配置示例

- 以下数值只属于 Demo 关卡引用的 EconomyData，不是项目硬编码默认值：金币上限 99,999、起始金币 600、被动收入每分钟 100 金币。
- 被动收入按服务器 World Game Time 和绝对累计值结算；切换到引用其他 EconomyData 的关卡后使用该关卡配置。
- 所有收入和消费只读写同一余额；英雄击杀奖励由被击杀单位 DataAsset 配置为一个非负整数。
- 死亡不扣除金币，也不重置余额；本局经济参数只读取服务器冻结的关卡 EconomyData 快照。
- 购买后 10 秒内、物品未使用且完整购买链仍可追溯时按实际支付额全额退款；否则按定义总价的 50% 出售并向下取整。

### Non-Goals

- 不复制 DOTA2 图标、音效、文字资源或当前线上平衡表。
- 首版不做信使、团队共享物品、中立物品掉落、库存交易/赠送、有限库存与补货、助攻/补刀分配、连杀奖金、买活、多货币、持久化收藏、存档或跨进程 Replay。
- 不改变六装备/三背包的主动被动规则，不新增第二套属性或物品实例权威，不让 UI/蓝图计算价格、金币或合成结果。
- 不自动提交、推送或发布；用户验收和 Git 操作需另行明确授权。

## 2. 当前事实与依据

- `Source/Combat/Combat/Release/CombatReleaseContract.h:20` 和 `:24` 固定 ContractVersion 3 / `combat_v3_economy_rc1`，经济开关为 true；`CombatReleaseContract.cpp` 校验物品与经济独立开启、旧合并字段保持 false。
- `Source/Combat/Combat/Items/CombatItemTypes.h:11-17` 只定义六装备和总计九槽；没有玩家级储藏处位置。
- `Source/Combat/Combat/Items/CombatItemData.h:33-67` 已定义表现、堆叠、主动、被动、冷却和共享规则，但没有价格、商店目录或配方。
- `Source/Combat/Combat/Items/CombatItemSubsystem.h:68-96` 是物品实例唯一注册表；`CreateItem`/`DestroyItem` 为受限事务入口，合成必须复用而不能另建物品副本。
- `Source/Combat/Combat/Items/CombatInventoryComponent.h:24-60` 已提供授予、拾取、放下、换位、消耗和九槽 View；需要增加仅供经济事务使用的预检/提交接口，不能绕过其锁、Spec 和效果清理。
- `Source/Combat/Combat/UI/CombatPlayerHUD.h:17-26` 当前只持有底部 HUD 与战斗记录；金币按钮和常驻储藏处应并入底部 HUD，商店作为第三个本地 Widget 由 HUD 生命周期统一创建/销毁。
- `Source/Combat/CombatGameMode.h` 当前只配置玩家 Controller/Unit 出生，没有关卡级经济或商店规则引用；本任务由各关卡使用的 GameMode 蓝图显式选择 EconomyData 与唯一 ShopData。
- `Source/Combat/Combat/Data/CombatDefinitionData.h:207-216` 已有等级/击杀经验，尚无金币奖励；新增字段默认 0 才能保持旧 UnitData 行为。
- `Config/DefaultGame.ini:26` 已扫描 `CombatItem`；新增 `CombatEconomy` / `CombatShop` PrimaryAssetType 需要显式扫描并纳入 commandlet。
- `ITEM-001` Spec §1/Non-Goals 明确排除商店、金币、购买/出售、配方、信使和储藏处；本任务必须新增 ADR 并只解除其中经用户批准的边界。
- `00-01` 当前历史基线为全量 `Combat.*` 85/85、资产 29/29；这些是此前证据，不能代替本次回归。
- 开工盘点：`git status --short --branch` 仅显示 `main...origin/main`；仓库无 `.codegraph/`；`python -B Tools/ue_environment.py check --json` 返回 `editor_ready=true`、`dedicated_ready=true`。当前会话未暴露 UE MCP 工具，资产阶段按 `30-01` 记录命令行 Unreal Python 降级并执行冷回读。

## 3. 行为与契约

### 主流程

```text
金币按钮
  -> 唯一商店目录、搜索、基础/升级页、分类网格与紧凑配方关系
  -> PlayerController 可靠 Economy RPC（意图 + RequestId + 相关修订）
  -> NetworkSecurity 共享 connection token/replay window
  -> EconomyComponent + ShopSubsystem 同步预检
  -> 原子扣款/储藏处变更/配方消费与生成
  -> Economy owner-only 快照 + Inventory owner-only 快照
  -> 结构化 Event / 玩家日志 / 明确结果 RPC
```

商店打开和选择物品完全本地，不暂停游戏、不打断 Move/Attack/Cast；商店没有世界 Actor、阵营或距离条件。购买、出售、储藏处转移和合成是服务器同步事务。客户端目录缺失时显示稳定 ID 占位并禁止提交，不影响服务器。

服务器在 `ACombatGameMode::InitGame` 阶段解析当前关卡 GameMode 配置的 EconomyData 与 ShopData，完成校验后生成整局不可变规则快照；玩家初始加入、晚加入和复活均读取同一快照，不在运行中重新读取可变资产。未配置或校验失败时经济事务 fail-closed，并记录明确错误。

### 金钱模型

- `UCombatEconomyComponent` 是 PlayerController 默认子对象，保存单一 `Gold`、`EconomyRevision`、六格储藏处和最近有界事务摘要。
- `UCombatEconomyData` 是关卡/对局规则资产；当前关卡 GameMode 是唯一选择入口，EconomyComponent 只消费服务器冻结的规则快照，不持有私自覆盖值。
- 所有加减使用 `int64` 中间值并限制到当前关卡 EconomyData 的非负上限；拒绝负数、溢出、未初始化和客户端直接写入。
- 起始、被动、击杀、购买和出售只改变同一余额，但事务仍记录来源与原因；死亡明确不产生金币事务。
- 被动收入只在服务器由 `UCombatSchedulerSubsystem` 驱动；按绝对 World Game Time 与分钟速率累计余数，卡帧不重复发放，暂停不增长，Controller/World teardown 取消唯一 Schedule。
- `RequestDeath` 成功后只向实际击杀者发放一次配置奖励，不改变受害者金币；自杀、无指挥玩家或奖励为 0 时不凭空创建金币。

### Demo 金币调试命令

- 沿用 `CombatDebugSubsystem.cpp` 现有开发命令模式，注册 `combat.Debug.SetGold <Amount> [PlayerControllerUniqueId|Name]`；省略目标时使用当前 World 的首个玩家 Controller。
- 命令只在 `!UE_BUILD_SHIPPING` 编译，并使用 `ECVF_Cheat`；当前权威 GameMode 还必须显式设置 `bEnableEconomyDebugCommands=true`，该开关默认 false，仅 Demo GameMode 开启。
- 命令只允许服务器或 Standalone World 执行；客户端调用、找不到目标、参数不是整数、数值小于 0 或超过当前关卡金币上限时，记录 Usage/失败原因且不改变状态，不做静默截断。
- 成功路径调用 EconomyComponent 的受限服务器调试入口，不直接写复制字段；它把余额设为目标值，递增一次 EconomyRevision，写入来源为 `DebugCommand` 的事务/事件并触发 owner-only 快照更新。
- 示例：`combat.Debug.SetGold 5000` 修改首个玩家；`combat.Debug.SetGold 2500 BP_CombatDemoPlayerController_C_1` 修改显式目标。

### 商店与交付

- `UCombatShopSubsystem` 解析每局唯一 `UCombatShopData`，不创建或注册 `ACombatShopActor`，也不检查位置、队伍、英雄存活或商店半径。
- 玩家任何时候都能打开商店并提交购买；购买成功的物品统一进入该 PlayerController 的储藏处。储藏处容量不足时整个购买事务失败，不扣金币、不创建遗留实例。
- 储藏处物品由 EconomyComponent 持有稳定 Handle，物品实例记录弱 Controller 所有者和槽位；它们不授予 Ability、Modifier 或 Aura。存在有效且存活的 `CommandedUnit` 时，可随时拖动单件或使用“全部拿走”转入英雄库存。
- “全部拿走”按储藏槽顺序移动尽可能多的完整实例；成功返回移动数量，剩余物品不丢失。单件转移、出售和购买保持全有或全无。

### 配方与确定性合成

- `FCombatItemRecipeIngredient` 使用软物品引用和正整数数量；组件可以是普通叶子或专用配方卷轴。复合物品自身不直接定价，总价递归求和所有可购买叶子。
- Validator 拒绝自引用、环、缺失组件、零/负数量、不可购买叶子、深度/展开数量超限、重复 DefinitionId 以及同优先级歧义配方。
- 英雄装备+背包是一个合成域，玩家储藏处是另一个合成域；不跨域偷偷消耗。商店购买只在储藏处域内使用已有组件并原子补齐缺失叶子。
- 每次成功购买、转移或库存变更后，服务器按 `CraftPriority desc -> DefinitionId asc` 反复合成到稳定态。重复组件按槽位升序和实例 Handle 排序选择，结果不依赖 TMap/AssetRegistry 枚举顺序。
- 合成先生成完整计划并验证容器空间、绑定、活动施法和所有修订；提交时消费精确数量/实例，创建一个新 Handle。失败不扣金币、不消耗组件、不改变修订。
- 结果继承组件中最大的剩余冷却和重新启用时间；绑定必须兼容；任一组件已使用、免费获得或不在同一退款链时，结果失去全额退款资格。合成不刷新冷却、能量或绑定。

### 购买、出售与退款

- 购买请求只携带目标 DefinitionId 与期望 Economy/Stash 修订；服务器解析唯一目录、价格、缺失组件、储藏容量和当前金币，客户端不能选择绕过储藏处的交付位置。
- 基础页物品购买一个叶子；升级页目标会在储藏处域内原子补齐缺失叶子并完成所有可达嵌套合成。客户端不维护快捷购买队列。
- 出售请求携带实例 Handle/Revision。活动物品、不可出售定义、旧持有者和已变化实例均拒绝；出售不要求商店位置或英雄存活。
- 原购买链在 10 秒内且未使用/拆分/跨玩家时退还实际支付额；否则返还定义递归总价的 50%（向下取整）。返还进入同一金币余额，实例和其授予/效果在同一事务中清理。

### UI 与输入

- 主商店使用暗色半透明面板与金色价格/青色可用反馈。顶部只有一个搜索框；其下只有“基础物品 / 升级物品”两个页签和当前页的分类网格。
- 基础分类默认“消耗品、属性、装备、其他”，升级分类默认“辅助、法器、防具、兵刃、军备”；分类和顺序来自 ShopData，不写死英文或 DOTA 图标。
- 左键选择物品；悬停 ToolTip 显示说明、价格和属性，右键购买；不可购买项显示具体原因。选择升级物品时在商店下部显示紧凑配方关系，节点只显示拥有数/需要数和静态依赖。
- 六格储藏处独立于商店面板，始终显示在 HUD 右下角；单一金币余额按钮位于储藏处上方，点击它开关商店，Escape 优先关闭商店。
- 储藏处支持拖到英雄库存、右键出售和“全部拿走”；商店关闭后储藏处和金币按钮继续显示。
- 搜索框右侧不放帮助、收藏、筛选、视图或其他按钮；不提供中立/团队页、独立详情侧栏或快捷购买栏。
- 商店、配方关系和储藏处的实际 Slate 几何阻止世界点击；关闭商店恢复原输入，不清理服务器 Order。

### 输入、输出与数据约束

- 新定义使用 `CombatEconomy:lower_snake_case` 与 `CombatShop:lower_snake_case`；现有 `CombatItem` ID 不改名。
- 目录和配方只引用稳定 DefinitionId；网络、日志和快照不复制 DataAsset UObject 指针。
- 价格、奖励、数量、层数、修订和请求 ID 均为整数且有明确上限；客户端不能提交价格、退款率、奖励、配方展开结果或最终余额。
- 新 FailureTag 至少覆盖未初始化、金币不足、旧快照、目录不可用、非法目录、配方损坏、储藏容量不足、物品忙和不可出售；新 EventTag 覆盖金币变化、购买、出售与合成。
- 经济事务结果包含 RequestId、CommandBindingGeneration、成功/失败、FailureTag、目标 DefinitionId、结果 Handle 和服务器修订；Reliable RPC 与 owner-only 快照互相独立。

### 权威边界与权限

- 金币、目录、配方、容器容量、出售资格和最终物品变更只在服务器结算；UI 过滤、预计价格和“可购买”状态均为提示。
- 经济 RPC 与 Order 共用同一 PlayerController 连接级 token bucket 和重放窗口，避免通过两个入口翻倍额度；Controller 使用一个单调正 RequestId 序列。
- 事务同时锁定 Economy、目标 Inventory/Stash 和 ItemSubsystem；预检后只存在一个提交入口，事件、Revision 与回执各一次。
- 最终属性仍只来自 ASC/GE；合成结果通过 Inventory 原有效果协调入口生效，不直接写属性、Health/Mana 或 AbilitySpec。

### 失败、取消、过期、死亡、EndPlay 与重复请求

- 同 RequestId 重放、旧 Economy/Inventory/Stash Revision、旧控制绑定、旧生命、物品旧 Handle/Revision 均无副作用失败。
- UI 关闭、切换选择或异步图标加载完成不会取消已发送的同步事务；旧 Widget/旧绑定回调由 `BindingRevision + CommandBindingGeneration` 淘汰。
- 死亡不扣金币；死亡中仍可打开商店、购买到储藏处和出售，但没有有效存活 `CommandedUnit` 时不能把储藏处物品转入英雄库存。复活不重置余额、退款时间或储藏处。
- Controller EndPlay 销毁其储藏处实例、取消被动收入 Schedule、解绑事件并清空快照；Unit EndPlay 仍按 ITEM-001 清英雄库存，不误删 Controller 储藏处。
- World Deinitialize 先使交易入口失效，再清关卡经济/商店规则快照、Economy 组件和 Item 注册表；重复结束安全失败且不二次返还金币。进入新关卡时重新解析新 GameMode 的规则并按其起始金币创建新对局，不跨关卡继承余额。

### 兼容、版本与迁移

- 推荐发布身份升级为 `combat_v3_economy_rc1`、ContractVersion 3、`bItemsEnabled=true`、`bEconomyEnabled=true`；旧 `bItemsAndEconomy` 继续固定 false。
- 推荐 ContentVersion 2、GameplayTag schema 3、Combat Event schema 3、玩家日志投影 schema 3；FormulaVersion 2、RNG 1、现有 Unit HUD schema 8 保持。新增独立 EconomyPresentation schema 1 与 ShopCatalog schema 1。
- 旧 ItemData 的价格、配方和可购买标志默认 0/空/false，仍可拾取和使用但不会自动进入商店；旧 UnitData 的单一金币奖励默认 0；地图不需要 Shop Actor。旧关卡 GameMode 未配置 EconomyData 或唯一 ShopData 时经济系统 fail-closed、商店显示空状态且购买失败，不静默套用 Demo 数值。
- 现有物品实例/事件没有存档承诺；离线事件迁移由 v1/v2 补空金币事务字段到 v3，拒绝未知未来版本和覆盖输出。
- 同版本客户端与服务器部署；历史 `combat_v1_rc1` / `combat_v2_items_rc1` 验收记录不改写。

## 4. 实施计划

| 文件/资产 | 变更 | 原因 | blast radius |
| --- | --- | --- | --- |
| `Combat/Economy/CombatEconomyTypes.*`、`CombatEconomyData.*`、`CombatEconomyComponent.*` | 关卡规则快照、单一金币、储藏处、Revision、被动收入、事务流水与 owner-only 复制 | 玩家级经济唯一来源 | 新领域、Controller 生命周期 |
| `Combat/Economy/CombatShopData.*`、`CombatShopSubsystem.*` | 唯一目录、递归配方图、购买/出售/合成事务 | 数据驱动 UI 商店和唯一提交入口 | ItemSubsystem、GameMode 配置 |
| `CombatGameMode.*`、Demo GameMode 蓝图 | 为各关卡选择 EconomyData/ShopData，在服务器 `InitGame` 校验、冻结本局规则，并由 Demo 独立开启经济调试命令 | 数值随关卡变化且对局中不可漂移 | 出生初始化、旧关卡兼容 |
| `Combat/Items/CombatItemData.*`、`CombatItemSubsystem.*`、`CombatInventoryComponent.*` | 价格/配方字段、储藏所有者、原子预检/消费/生成、冷却与退款来源继承 | 复用唯一实例和效果生命周期 | 全部物品回归 |
| `CombatPlayerController.*`、`CombatNetworkTypes.*`、`CombatNetworkSecuritySubsystem.*` | 经济 RPC、共享请求额度/重放、结果回执与金币按钮入口 | 客户端只能提交有界意图 | 网络安全与既有 Order |
| `CombatUnitCharacter.*`、`CombatDefinitionData.*`、`CombatDamageSubsystem.*` | 单一击杀金币与 Controller 归属，死亡不扣金 | 正确奖励实际玩家且 exactly-once | 致死事务/成长回归 |
| `Combat/UI/CombatShopWidget.*`、`CombatShopItemWidget.*`、`CombatRecipeTreeWidget.*`、`CombatHUDWidget.*`、`CombatPlayerHUD.*` | 顶部仅搜索、基础/升级分类网格、紧凑配方、金币按钮和常驻储藏处生命周期 | 落地简化后的附件信息架构 | 本地输入/HUD |
| `Combat/Log/*`、`Combat/Core/CombatTags.*`、`Combat/Release/*` | 经济事件、失败标签、日志分类和 v3 契约 | 可观测且版本诚实 | 序列化/同版本部署 |
| `Combat/Debug/CombatDebugSubsystem.cpp` | 注册非 Shipping、服务器权威的 `combat.Debug.SetGold` | Demo 快速验证购买、余额和 HUD | 调试命令/日志 |
| `Config/DefaultGame.ini`、`CombatAssetValidationCommandlet.cpp` | 扫描/校验 Economy、Shop 和配方图 | cook 前阻止坏数据 | 资产发现与内容版本 |
| `Content/Combat/Demo/Economy`、`Content/Combat/Demo/UI`、Demo Framework | Demo 关卡经济规则、唯一目录、原创物品/配方、简化 WBP 及 GameMode 引用 | 可玩纵向切片 | 蓝图/LFS |
| `Combat/Tests/CombatEconomyTests.cpp`、HUD/Release/Network 场景 | Red/Green、失败、生命周期、UI 和 Dedicated 覆盖 | 交付证据 | Automation 数量增加 |
| README、`10-01/08/09/12/14`、新增经济专题、`00-01/04`、`90-16` | 当前行为、ADR、迁移、生命周期与操作说明 | DDD/代码一致 | 文档入口 |

依赖顺序：契约测试 Red → 数据/配方纯函数 → Economy 生命周期 → Item 原子容器接口 → Shop 事务 → RPC/安全 → View/UI → Demo 资产 → PIE/Dedicated。任何范围、权限、版本、测试矩阵或回滚实质改变都先触发 F1 重审。

## 5. 验收标准（AC）

- [ ] AC-01：Economy 组件规则、单一余额、被动/击杀/购买/出售、死亡不扣金及边界写入由 `Combat.Economy.*` 8/8 覆盖；GameMode 冻结快照和真实入局流程仍未做实机验证。
- [ ] AC-02：商店目录、储藏交付、空间不足原子失败与购买计划由 Economy/Shop 自动化覆盖；真实任意位置/生死状态交互仍列为未执行项。
- [x] AC-03：重复/嵌套配方、环、缺资源、旧修订和空间失败由配方图与购买计划测试覆盖。
- [x] AC-04：库存域与储藏域的实例转移、Handle/Revision 和自动合成边界由 Economy 自动化覆盖；长流程 teardown 仍列为未执行项。
- [ ] AC-05：新鲜购买全额退款和重复请求已有自动化；超出 10 秒窗口的 50% 退款、不可出售定义和实机出售流程尚未执行。
- [ ] AC-06：Native Widget 结构、初始化/销毁和商店开关已通过 `Combat.UI.Shop.NativeStructureAndLifecycle`；真实 HUD 几何、输入阻止和 Demo WBP 人工回归未执行。
- [ ] AC-07：Controller/World 生命周期、被动调度和 UI 重建已有自动化边界；经济跨 World teardown 与 Dedicated teardown 仍列为未执行项。
- [x] AC-08：v3 发布契约、迁移工具、资产校验和全量 `Combat.` 回归通过；中文 DDD 已同步当前入口。
- [ ] AC-09：Editor、直接/全量 Combat 和资产校验有证据；Server/Client Target、真实 PIE、Dedicated 双客户端、容量/teardown 尚未在本轮执行。
- [x] AC-10：`combat.Debug.SetGold` 默认/显式目标、边界和拒绝路径由 `DebugSetGoldCommandBoundaries` 覆盖。

## 6. Definition of Done

- [x] 已先建立 `CombatEconomyTests.cpp` 的 Economy/Recipe/Shop 最小 Red；首次 Editor 构建按预期因尚不存在 `Combat/Economy/CombatEconomyComponent.h` 失败，随后才进入实现。
- [x] 经济、商店、物品、网络和 UI 只有文中一个权威入口，没有蓝图价格或第二套实例/余额。
- [x] 新增公开类型、函数、字段和 DataAsset 展开字段已补中文注释、DisplayName/ToolTip；资产校验 31 个资产 0 error/0 warning。
- [x] Demo/DataAsset 按 Read → Mutate → Verify → Record 修改，安装版 Editor 构建、资产校验和冷启动命令行回读证据已保留；不新增场景商店 Actor。
- [ ] Editor 与直接 Automation 通过；网络/复制的 Server/Client Target、Dedicated 双客户端和人工 PIE 尚未执行，自动化权限/重放用例已通过。
- [x] F2 问题已关闭，delivery Gate、文档校验和 `git diff --check` 通过；未混入生成文件或用户修改。

## 7. 测试矩阵与命令

| 层级 | 用例/命令 | 预期证据 | 结果 |
| --- | --- | --- | --- |
| 流程/文档 | `python -B Tools/task_gate.py --mode preflight/plan/build/delivery --spec Doc/CombatSystem/Specs/ECON-001-money-shop-crafting.spec.md --kind feature`；`python -B Tools/validate_docs.py`；`git diff --check` | 四阶段报告、文档链接与差异卫生 | 本轮 delivery、docs、diff check 通过；命令与结果见 §9 |
| Pure/Unit | `Combat.Economy.*` | 余额、Scheduler 绝对时间、配方图、购买计划、全额/半价/不可出售退款边界 | 8/8 Success，`Saved/CombatEconomy/Automation-Economy-Final3.log` |
| World Automation | Economy lifecycle tests | 规则快照、被动收入、击杀/死亡、购买/合成、储藏转移、调试命令与 RPC 安全 | 8/8 Success；真实长流程 Dedicated 未执行 |
| UI / Blueprint | `Combat.UI.Shop.NativeStructureAndLifecycle` | Widget 结构、初始化、NativeConstruct/Destruct、绑定和商店开关 | 1/1 Success，`Saved/CombatEconomy/Automation-UI4.log`；真实 WBP/几何未执行 |
| Development console | `DebugSetGoldCommandBoundaries` | 默认/显式目标、0/上限、非法参数和拒绝路径 | 已包含在 Economy 8/8 |
| Build / Regression | UE 5.8.2 `ue_gasEditor` Development；`Automation RunTests Combat.` | 编译、直接专项和全量回归 | Editor build 成功；`Combat.` 94/94，`Saved/CombatEconomy/Automation-All-Final3.log` |
| Asset / PIE | `CombatAssetValidation` | DataAsset/蓝图发现和配方图校验 | 31 assets, 0 errors, 0 warnings，`AssetValidation-Current.json`；人工 PIE 未执行 |
| Network / Dedicated | RPC 安全自动化；`ue_gasServer`/`ue_gasClient`、Dedicated | ownership、重放、共享限频和 owner-only | 自动化 RPC 用例通过；Server/Client Target 与 Dedicated 经济场景未执行 |
| Soak / Perf | 经济事务循环 + teardown | 预算、无实例/Schedule/委托泄漏 | 未执行，需后续独立容量 Gate |
| 迁移 | v1/v2 事件到 v3 正反例单测 | 旧事件兼容、未知版本拒绝 | 迁移工具单测已执行；结果写入 §9 |

## 8. 风险、回滚与升级

- 风险：关卡 GameMode 漏配或客户端/服务器规则漂移；调试命令误在非 Demo 或 Shipping 生效；跨 Inventory/Stash 的多对象原子性；共享组件产生歧义合成；递归配方环或膨胀；全局购买/出售重放导致复制金币；组件消耗刷新冷却/绑定；Controller 与 Unit 不同生命周期；UI 预测价格与服务器目录漂移；大量目录图标异步加载；版本不同步。
- 回滚方式：成组恢复 Economy/Shop 源码、Item 扩展、网络/schema、Demo/UI 资产和文档；删除新 PrimaryAsset 扫描与引用；联机双方一同回滚到 `combat_v2_items_rc1`。不得保留 `bEconomyEnabled=true` 而撤销实际系统，也不得引入第二套余额状态。
- 触发升级的条件：需要改变已批准的金币归属、引入持久存档/交易/信使/有限库存、无法让跨容器事务保持原子、三轮修复仍不收敛、必需 Dedicated/资产验证受阻，或出现不可逆内容迁移。
- 需要人决定的问题：无；用户已批准 Spec 0.4 的 L2 契约。若后续需要改变关卡规则归属、金币语义、交付域、配方或版本/schema，先递增 Spec 并重新进入 F1。

## 9. 交付证据

- 代码/资产 diff：已落地 Economy/Shop 类型、组件、Controller RPC、安全校验、HUD/Shop Widget、Demo Economy/Shop/UI 资产、v3 发布契约、迁移工具和自动化测试；保留工作区原有修改，不提交、不推送。
- 计划门禁：2026-09-15 F0/F1 后的 preflight/plan Gate 通过；build Gate 已通过（均 `0 error(s), 0 changed file(s)`）。
- 构建结果：安装版 UE 5.8.2 `ue_gasEditor Win64 Development` 成功；构建命令使用 `Engine/Build/BatchFiles/Build.bat`，未重跑源码引擎的 Server/Client Target。
- Economy/UI Automation：`Automation-Economy-Final3.log` 显示 `Combat.Economy.` 8/8 Success（含 10 秒窗口外半价退款和不可出售拒绝）；`Automation-UI4.log` 显示 `Combat.UI.Shop.` 1/1 Success。
- 全量回归：`Automation-All-Final3.log` 发现 94 个 `Combat.` 测试，退出码 0，未见测试失败或 CriticalError。
- 资产校验：`AssetValidation-Current.json` 为 `scannedAssets=31, errorCount=0, warningCount=0`；命令行日志确认 `CombatAssetValidation Version=2` 已保存报告。
- 工具与文档：Tools 全量 unittest 49/49、`python -B Tools/validate_docs.py`（76 Markdown、388 链接）和 `git diff --check` 均通过；delivery Gate 在本 Spec 回写后通过。
- 未执行验证及原因：Server/Client Target、真实双客户端 Dedicated 经济场景、人工 PIE 几何/输入、经济专项 soak/perf、cook/打包未执行；当前安装版环境与时间窗口不足以把这些结果诚实标为通过，后续需在用户验收或独立网络 Gate 中完成。
- 剩余风险：未执行项覆盖真实联机 owner-only 复制、跨 World teardown 和 UI 几何；Demo 数值只作为当前资产配置，平衡调整仍应走关卡 EconomyData；用户验收仍为 `IN_PROGRESS`。

## 10. 变更记录

| 版本 | 日期 | 修改 | 原因 |
| --- | --- | --- | --- |
| 0.4 | 2026-09-15 | 增加 Demo-only `combat.Debug.SetGold` 命令、服务器权限/边界规则与自动化验收项 | 用户要求在 Demo 场景快速修改金币 |
| 0.4-build | 2026-09-15 | 完成 Economy/Shop 实现、Demo 资产、v3 契约、自动化与资产校验；修复被动收入绝对时间和 Shop Widget 初始可见性测试夹具问题 | 继续上一会话未完成的实现和验证 |
| 0.3 | 2026-09-15 | 将金币上限、起始金币、被动收入和唯一商店目录改为关卡 GameMode 选择的对局规则；Demo 数值仅作 Demo 关卡配置 | 用户确认经济参数应与关卡相关 |
| 0.2 | 2026-09-15 | 改为单一金币且死亡不扣金；移除实体/范围商店与 Home/Secret；商店简化为仅搜索、基础/升级分类，并将储藏处固定在 HUD 右下角 | 用户明确修订玩法与 UI 范围 |
| 0.1 | 2026-09-15 | 冻结 DOTA-like 金币、商店、储藏处、配方与 UI 的实施候选 | 用户请求进入 L2 设计审查 |

## 11. 路由、自评与 Reflect

### 路由记录

- 原始需求摘要：参考 DOTA2 商店 UI 和设计，实现项目的金钱、商店和物品合成闭环。
- 主 Skill：`combat-feature-development`
- 选择依据：跨 C++、网络、物品事务、UI、DataAsset、测试与发布契约的通用功能变更。
- 备选 Skill 与排除理由：`combat-skill-development` 仅在新增具体主动/被动技能时适用，本任务不需要新施法内核。
- 路由置信度：`high`

### 交付自评

| 维度 | 权重 | 分数（0–5） | 证据与扣分原因 |
| --- | ---: | ---: | --- |
| 需求与 AC | 20% | 4.0 | AC-01/03/05/08/10 有自动化证据；AC-06/09 的真实 UI/网络场景仍未执行 |
| 架构与权限 | 20% | 4.0 | Controller/Economy/Shop 单一权威入口、owner-only 快照、共享 RPC 安全和 v3 契约已落地 |
| 实现与数据 | 20% | 4.0 | 代码、测试、Demo DataAsset 和 UI Native 结构已构建并通过资产校验 |
| 验证证据 | 20% | 2.0 | Editor、94 项全量和资产校验通过；Server/Client、Dedicated、人工 PIE、soak 未执行，按门槛封顶 |
| 文档与可观测性 | 10% | 3.5 | Spec、进度台账、ADR、HUD/Item/经济专题已同步；未执行项显式记录 |
| 交付卫生 | 10% | 3.5 | 文档校验、diff check、delivery Gate 通过；工作区保留用户已有修改且未提交 |

- 计算总分：`3.6 / 5`（按权重四舍五入）。
- 硬性封顶或未执行项：真实 Server/Client、Dedicated、人工 PIE 和 soak 未执行，因此不宣称完整网络/实机验收。
- 自评结论：`READY_FOR_REVIEW`
- 用户验收状态：`IN_PROGRESS`（实现与本地交付 Gate 完成，等待用户游玩验收）

### Reflect 与调优

- 观察与证据：上一会话已完成主要 Economy/Shop 实现，但 Spec 和 DDD 仍停留在 BUILD 前状态；本轮先读取旧 Automation/资产日志，再修复 Editor 构建与测试夹具差异，最后完成全量回归。
- 根因类别：`实现问题` + `测试夹具`
- 调整文件与预期收益：被动收入回调使用 Scheduler 提供的绝对时间，避免直接 `RunDueTasks(Now)` 与 World time 脱节；Shop Widget 显式初始化可见性，测试使用无 LocalPlayer 的 Native Widget fixture，避免把测试环境限制误判为产品失败。
- 回归验证：Economy 8/8、UI 1/1、Combat 94/94、资产 31/0/0、文档与 delivery Gate 均通过；未执行网络/实机项已列于 §7/§9。
- 需要用户决定的问题：无需重新审查已批准的 v0.4 范围；请在本地游玩后确认是否接受当前 Demo 数值与 UI 纵向切片，并决定是否开启 Dedicated/PIE 专项 Gate。
