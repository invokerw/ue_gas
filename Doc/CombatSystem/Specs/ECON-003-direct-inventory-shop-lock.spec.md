# ECON-003 商店直达物品栏与锁定合成

> Spec 版本：`0.8`
> 状态：`COMPLETED`
> Owner：Codex
> 创建日期：2026-09-16
> 关联进度台账：[00-01](../00-Project/00-01-Progress-Tracker.md)
> 风险等级：`L2`

## 0. Intake 与 Gate 记录

- 用户请求：继续梳理物品出售流程；兼容代码可以不要；修改要完整。结合前一轮需求，储藏室运行时和兼容链路全部移除，商店购买直接进入物品栏，物品栏右键出售与锁定/解锁，进入物品栏自动合成且锁定物品不参与合成；右键菜单移除“移入背包”和“放到地面”，仅保留拖拽到地面的落点；解锁时检测当前库存是否已满足合成条件并立即处理可合成配方。
- 附件解释：`pasted-text.txt` 是前一轮物品出售流程与修改讨论，作为需求上下文；当前消息明确覆盖兼容代码、右键入口和解锁合成行为。没有额外视觉稿、数值或外部协议。
- 已读取入口：`agent.md`、`README.md`、`00-01`、`00-03`、`00-04`、`00-05`、`10-01`、`10-09`、`10-12`、`10-14`、`10-15`、`90-16`、`combat-task-router`、`combat-feature-development`、现有 ECON/ITEM Spec，以及 Economy、Inventory、Item、Controller、NetworkSecurity、HUD、Shop 和 Automation 源码。
- 主 Skill：`combat-feature-development`（`Skills/combat-feature-development/SKILL.md`）。
- 备选 Skill 与排除理由：`combat-skill-development` 不适用，本任务不新增玩家可施放 Ability；`skill-creator` 不适用，本任务不修改 Skill；`visualize` 不适用，用户要求落地游戏功能而非交互说明图。
- 路由置信度：`high`
- F0 结论：`GO`
- F0 依据：上一轮已将购买、出售、锁定和自动合成切到当前主控单位库存，但保留了储藏字段、实例所有权、PlayerController 入口、Native Widget 和清理函数。用户明确允许删除兼容代码；本轮只收敛为单一库存出售/购买事务，并同步公开 RPC、复制 schema、发布契约和文档。
- F1 结论：`APPROVED`
- F1 审查人：Codex（本地计划审查）
- F1 审查版本：`0.8`
- F1 计划审查证据：v0.8 用户已完成 review 并授权执行 PIE、Dedicated、cook、soak 后本地提交；扩展实际运行测试矩阵，新增仅显式启用的经济验证编排器、PIE 驱动和报告。此计划只增加开发/验证入口，不改变运行时协议、资产或价格；前版审查：v0.7 将右键菜单的“放到地面”入口和脚下即时投递路径一并删除，只保留拖拽释放到屏幕地面的路径；同步收窄 Controller/UI/API、常量、输入 Automation、文档、风险与回滚范围。`preflight`、`plan` 均通过，准许进入实现。
- Build 解锁：F1 已审查通过；首次代码/测试修改前运行机器 Build Gate。
- F1 重审条件：右键菜单动作、脚下落点计算、拖拽任意落点保留、库存/经济修订字段、RPC 动作、测试矩阵或回滚方式发生实质变化时递增 Spec 版本并重新审查。
- F2 结论：`PASS`
- F2 对抗审查证据：菜单与 Controller 不再暴露脚下即时投递入口，拖拽仍经 `DropInventoryItemAtScreenPosition` 和服务器复核；v0.8 全量 Combat、PIE、独立 Dedicated 双客户端经济闭环、物品争抢/控制互换与容量均通过。验证编排器只在显式命令行开关下启用，通过公开请求观察投影，不改变生产事务；旧修订仅在零金币变化时等待复制并有限重试。报告区分初次失败与最终复验，最终双客户端均为 0 失败、0 重试。
- Push-Ready 结论：`READY`
- 验证：v0.8 已完成安装版 Editor 与源码 Server/Client 三 Target 构建、完整 Combat 96 项、NullRHI PIE、5 个蓝图编译回读、Dedicated 双客户端 300 秒经济 soak、64 Unit/256 Modifier 容量与物品争抢、Windows cook 650 包、资产校验 31 项。具体命令、报告和边界见 §7、§9。
- 未执行：渲染窗口中的菜单几何/人工鼠标拖拽、打包可执行文件部署、超过 300 秒的 soak 和人工网络损伤；PIE 使用 NullRHI，Dedicated 使用安装版 Editor 的独立 `-server`/`-game` 进程，不能替代这些额外验证。

## 1. 目标与范围

### 目标

将当前“商店 → 六格储藏室 → 手动转入英雄”的流程收敛为“金币按钮 → 商店 → 直接进入当前主控单位物品栏”，并让物品栏右键完成出售与锁定管理。服务器继续是价格、金币、物品实例、修订和合成的唯一权威。

### 范围

- 从 `ACombatPlayerHUD` 的运行时创建链中移除储藏室；`UCombatShopWidget` 常驻并在右下角显示金币按钮，按钮打开/关闭商店面板。
- 购买事务以当前存活 `CommandedUnit` 的九格库存为交付域；先按稳定配方计划消费未锁定库存组件，再补购缺失叶子、扣金币、生成结果并经库存效果入口提交。
- 购买、拾取、剧情/奖励授予等物品进入库存的顶层事务结束后自动稳定合成；锁定实例从合成候选和购买计划中排除，合成结果默认解锁。
- 在 `FCombatItemView`、物品实例和 owner-only HUD 快照中增加锁定状态；物品栏右键菜单提供“出售”和“锁定/解锁”。出售沿用全额退款窗口与折扣规则，并清理装备能力/被动/光环。
- 为购买、库存出售、锁定切换增加服务器 RPC 意图、连接级限频/重放/绑定代次/修订校验和结构化结果；新增库存修订字段，经济 RPC 不再暴露旧储藏动作。
- 删除储藏兼容类、字段、RPC 入口、实例所有权和清理代码；库存出售成为唯一出售路径。
- 更新 ECON 专题、ADR/Gap、README、进度台账和相关 Automation；经济表现 schema 递增到 3，ContractVersion/ReleaseId 递增到 v4 候选身份。

### Non-Goals

- 不保留旧 `UCombatStashWidget` 类、旧储藏槽成员、旧储藏实例所有权或历史 PlayerController 入口；已验收的 ECON-001/002 文档只作为历史记录，不继续承担运行时兼容。
- 不改变价格、退款窗口、出售折扣、配方定义、六装备/三背包容量、技能/属性公式或金币收入规则。
- 不允许客户端直接写金币、物品定义、库存槽、锁定状态或合成结果；UI 的价格/拥有数只作提示。
- 不新增场景商店 Actor、距离/队伍条件、持久化、交易/赠送、信使或第二套物品实例注册表。
- 不在本任务中修改二进制 DataAsset/蓝图布局；若真实 Editor 不可用，记录降级方式并保留用户现有资产。

## 2. 当前事实与依据

- `Source/Combat/Combat/Economy/CombatEconomyComponent.cpp` 已只保留库存购买、出售、锁定和库存自动合成；储藏字段、事务和合成入口已删除。
- `Source/Combat/Combat/Economy/CombatEconomyComponent.cpp` 的库存自动合成已经完成锁定过滤、购买接线、解锁检查和合成后的经济投影刷新，保持唯一合成入口。
- `Source/Combat/Combat/Items/CombatInventoryComponent.cpp` 的 `GiveItem`/`TryPickup` 在顶层接管成功后调用 `StabilizeInventoryCrafting`；这是“进入物品栏自动合成”的公共入口。
- `Source/Combat/Combat/Items/CombatItemSubsystem.h` 是唯一物品实例登记表；实例持有锁定状态和购买退款链。
- `Source/Combat/Combat/Items/CombatItemTypes.h` 的 `FCombatItemView` 是 owner-only 物品投影；`CombatHUDView.cpp` 从库存构建含锁定状态的九槽 HUD 快照。
- `Source/Combat/CombatPlayerController.cpp` 和 `CombatNetworkSecuritySubsystem.cpp` 为购买、库存出售、锁定动作统一检查 RequestId、绑定代次、修订、限频和重放窗口。
- `Source/Combat/Combat/UI/CombatPlayerHUD.cpp` 已不创建储藏 Widget；`CombatShopWidget.cpp` 的 Slate 根提供右下常驻金币按钮和库存拥有数。
- `CombatHUDItemSlotWidget.cpp:124-168` 的右键菜单已有使用、出售、锁定和反向整理闭包；本次移除“放到地面”菜单项，与上一轮已移除的“移入背包”一起不再出现在右键菜单，拖拽释放路径仍由 `DropInventoryItemAtScreenPosition` 处理。
- 当前 README/ADR-060/10-15 已统一为单一库存路径和 v4/schema 3；历史 ECON-001/002 结论仍只作为历史 Spec/台账，不承担运行时兼容。

## 3. 行为与契约

### 主流程

```text
右下金币按钮
  -> 商店面板开/关（本地 Slate；不暂停游戏）
  -> 右键商品提交 DefinitionId + Economy/Inventory revision
  -> PlayerController 可靠 Economy RPC
  -> NetworkSecurity 所有权/RequestId/绑定代次/限频/重放/载荷校验
  -> EconomyComponent 在当前库存域构建计划（锁定组件不可用）
  -> 原子消费未锁定组件、补购缺失叶子、生成结果、扣金币
  -> Inventory 自动稳定合成（锁定实例持续保留）并 ReconcileEffects
  -> owner-only HUD/经济投影与结果回执更新
```

物品栏右键出售沿用同一 RPC 安全链；锁定只改变实例锁定位、实例 Revision 和 InventoryRevision，解锁在同一服务器事务完成状态切换后立即检查当前库存，若已满足配方则复用 `StabilizeInventoryCrafting` 完成自动合成，否则只刷新解锁状态。右键菜单不再生成 `DropItem` 请求；拖拽释放仍通过 `DropInventoryItemAtScreenPosition` 提交屏幕地面落点，并由服务器复核实例修订、导航、距离和 LOS。

### 状态转换

- 购买：`ShopIntent -> Validate -> Plan -> CommitInventory -> Stabilize -> Refresh`；任何预检失败都回到原库存/金币/实例状态。
- 出售：`InventorySnapshot -> ValidateHandle/Revision/Sellable -> RemoveEffects/Ability -> AddGold -> Refresh`；活动施法、旧句柄、非出售定义和正在结束的组件失败且无副作用。
- 锁定：`Unlocked -> Locked` 只切换状态；`Locked -> Unlocked` 在实例修订和库存修订递增后检查当前库存，若存在可合成配方则在同一服务器请求尾部立即稳定合成；两种方向都只允许实例当前由该玩家主控单位持有且修订匹配。
- 进入库存：`World/Grant/Purchase/Transfer -> AcceptItem -> StabilizeInventoryCrafting -> ReconcileEffects`；合成按 `CraftPriority desc, DefinitionId asc`，实例槽位按稳定顺序选择。

### 输入、输出与数据约束

- 新经济请求增加 `ExpectedInventoryRevision`；购买/出售/锁定只接受稳定 DefinitionId 或精确 Handle+ItemRevision，不接受价格、金币、锁定结果或配方展开数组。
- `FCombatItemView` 增加 `bLocked`；`FCombatEconomyView` 只保留库存修订/库存物品投影，不再包含 `StashItems`/`StashRevision`；结果只返回库存修订和必要的锁定结果信息。
- 锁定实例不能作为 `BuildPurchasePlan` 的 OwnedDefinitions，也不能被 `TryCraftInventoryRecipe` 选作 ingredient；锁定与未锁定堆叠不合并，防止通过合并绕过锁定。
- 购买结果、自动合成结果默认 `bLocked=false`；出售整件实例沿用已有退款链和配置的出售折扣，返还受金币上限限制。

### 权威边界与权限

- 服务器 `UCombatEconomyComponent` 读取当前 `CommandedUnit` 和 `UCombatInventoryComponent`，在 Economy/Inventory/ItemSubsystem 事务锁内提交；客户端只显示投影和提交意图。
- UI 不直接调用 `CreateItem`、`DestroyItem`、`GrantItemAbility` 或写库存数组；购买必须复用库存效果/能力清理和 `NotifyChanged`，锁定必须由 Economy 服务器入口完成。
- 经济快照仍 owner-only；不存在旧储藏动作或旁路，所有购买、出售和锁定请求都进入库存修订校验。

### 失败、取消、过期、死亡、EndPlay 与重复请求

- 旧 Economy/Inventory/Item Revision、旧 `CommandBindingGeneration`、重复 RequestId、非 owning connection、载荷不匹配、金币不足、库存满、锁定组件不足、活动施法或组件结束均 fail-closed 且不扣金/不吞实例。
- 当前无存活主控单位时购买直接失败 `Failure_Life_NotAlive`；储藏室不再作为死亡期间的后备交付域。死亡后已有库存可按现有物品生命周期处理，出售/锁定仍需实例和组件未结束。
- Shop/HUD EndPlay 解绑经济与单位 View；Controller/Unit/World teardown 取消调度并清理库存能力/效果，旧回调由弱引用、绑定代次和 LifeGeneration 淘汰。
- 同一锁定/出售请求重放只消费一次 token/replay window；客户端收到旧绑定结果时丢弃，不二次广播。

### 版本与迁移

- 递增为 `ContractVersion=4`、`ReleaseId=combat_v4_economy_rc1`、`EconomyPresentationSchemaVersion=3`；物品/经济开关保持开启。
- 经济 RPC 只包含购买、库存出售和锁定切换；删除旧储藏动作字段、实例所有权、View 字段和 Widget 源文件，不承诺旧客户端在线互操作。
- 不跨版本迁移在线玩家状态；客户端与服务器必须使用同一 v4 协议。回滚必须整组恢复上一版发布，不保留可写半迁移入口。

## 4. 实施计划

| 文件/资产 | 变更 | 原因 | blast radius |
| --- | --- | --- | --- |
| `CombatItemTypes.h/.cpp`、`CombatItemSubsystem.h` | 增加锁定投影和实例状态、比较字段 | 让锁定状态可由服务器持有并复制到 HUD | 物品 View、复制、测试 |
| `CombatInventoryComponent.h/.cpp` | 锁定友好合并规则、锁定过滤所需友元/刷新入口 | 保持公共库存接管、自动合成和效果清理唯一 | Inventory/Item/UnitView |
| `CombatEconomyTypes.h`、`CombatEconomyComponent.h/.cpp` | 库存修订/投影、新购买/出售/锁定事务；删除全部储藏字段和函数 | 将经济权威收敛到 Inventory | 金币、合成、RPC 回执 |
| `CombatPlayerController.h/.cpp`、`CombatNetworkSecuritySubsystem.cpp` | 新动作、请求字段和服务器分派；本地提交库存修订 | 维持所有权、限频、重放和绑定代次 | 网络安全、客户端 HUD |
| `CombatShopWidget.h/.cpp`、`CombatPlayerHUD.h/.cpp`、`CombatHUDWidget.cpp` | 常驻右下金币按钮、库存拥有数投影和无储藏 HUD 接线 | 让 UI 只暴露单一库存交易域 | Slate 命中、焦点、输入 |
| `CombatHUDItemSlotWidget.cpp` | 右键出售、锁定/解锁、锁定提示 | 物品栏直接操作入口 | HUD 菜单 |
| `CombatHUDItemSlotWidget.cpp`、`CombatHUDWidget.cpp` | 移除装备物品右键“移入背包”和所有物品右键“放到地面”项，保留背包物品“移入装备栏”；拖拽释放继续进入屏幕地面落点 | 右键菜单只保留操作类动作，同时保留反向整理和拖拽丢弃能力 | HUD 菜单、状态栏、拖拽 |
| `CombatItemInput.cpp`、`CombatPlayerController.*` | 删除右键丢弃的脚下目标计算、提交入口及相关常量，保留 `DropInventoryItemAtScreenPosition` 的拖拽路径 | 防止右键绕过玩家选择落点，同时保持任意合法拖拽落点 | Controller、Order、HUD 输入 |
| `CombatEconomyTests.cpp` 及相关测试 | 覆盖直达库存、库存出售、锁定、解锁后可合成、无完整配方解锁、RPC 载荷和 Widget 结构；删除储藏假设 | 防止旧储藏旁路回归并验证解锁事务 | Automation |
| `CombatPlayerInputTests.cpp`、相关 HUD/物品测试 | 覆盖右键菜单不生成放地面请求、拖拽屏幕落点入口仍可用和菜单动作结构 | 锁定新的输入语义，避免恢复右键丢弃或删除拖拽入口 | Automation |
| `CombatReleaseContract.*`、`10-15`、`10-14`、`00-04`、README、`00-01` | 更新 schema、ADR-060、运行时说明和证据 | 让发布契约与实现事实一致 | 文档/Gate |

### v0.8 运行验证计划

- 新增 `CombatEconomyNetworkScenario` 测试 Actor，通过 `-CombatEconomySmoke` 启用；只编排公共客户端购买/锁定/出售请求，不另建事务或改写价格。检查真实 owning RPC、owner-only 投影、购买/出售金币方向、解锁合成、材料与成品移除；精确退款数值、非法请求与 teardown 由专项 Automation 覆盖。旧经济修订拒绝必须没有金币变化，等待复制后每轮最多重试 8 次并记录次数。
- 在 Demo PIE 中启动同一经济循环，编译并回读现有 HUD/Shop/GameMode 蓝图；检查 Slate 菜单动作及拖拽地面路径。UE MCP 当前会话未提供工具，降级为安装版 Editor Python 启停 PIE 与 C++ 结构化日志。
- 使用源码引擎构建 Server/Client Target；Dedicated 双客户端每 30 秒执行一个经济闭环、持续 300 秒，记录每端完成数、失败和修订重试次数，使用 M7 指标观察 Modifier/调度槽/帧时；容量与物品争抢场景单独运行，避免控制互换干扰经济场景。
- 执行 Windows cook 与 CombatAssetValidation；任何错误先保存失败证据，再最小修复并复验。既有生产事务若无失败证据不改写。
- 本地脚本和原始日志写入 `Saved/ECON003Validation/`；测试编排代码和可复用启动参数纳入本地提交。回滚按测试 Actor/入口与原 v0.7 功能文件组移除；不修改资产数值或协议版本。
- 所有适用项结束后更新 Spec、台账与 F2/delivery Gate，提交只包含 ECON-003 及其验证变更，保留 CAM-001 独立文档差异。

## 5. 验收标准（AC）

- [x] AC-01：运行时 HUD 不创建、不显示储藏室；储藏 Widget 源文件和 HUD 接线已删除；右下常驻金币按钮在商店关闭/打开时均可见，点击可幂等开关商店并阻止按钮区域世界点击穿透（Native UI Automation；真实 PIE 几何仍未执行）。
- [x] AC-02：商店购买成功后物品直接进入当前主控单位九格库存；库存满、无存活主控单位或修订过期时失败且金币/已有物品不丢失（直接库存/事务与 RPC 回归；容量边界保留旧库存测试）。
- [x] AC-03：购买/拾取/奖励进入库存后自动按确定性配方规则合成；自动合成复用现有 Inventory 接管、能力授予、效果协调和生命周期清理。
- [x] AC-04：物品栏右键菜单对可出售物品显示“出售”，服务器按既有全额退款窗口/折扣规则结算并正确清理装备能力、被动和光环；不可出售物品拒绝。
- [x] AC-05：物品栏右键显示“锁定/解锁”；锁定状态出现在 `FCombatItemView`/HUD，锁定物品既不被自动合成也不作为商店购买计划组件；解锁通过服务器修订校验，并在解锁后检测当前库存，存在可合成配方时立即自动合成。
- [x] AC-06：锁定与未锁定同定义实例不会通过堆叠合并绕过保护；新购买/合成结果默认解锁。
- [x] AC-07：所有新经济动作经过所有权、正 RequestId、绑定代次、Economy/Inventory/Item Revision、限频和重放校验；重复/旧请求 exactly-once 无副作用。
- [x] AC-08：现有战斗/物品/经济回归通过，Automation 覆盖直达库存、库存出售、锁定过滤、金币按钮结构和无储藏代码/字段；文档与 v4 发布表现 schema 同步。
- [x] AC-09：装备物品右键菜单不再出现“移入背包”，任何物品右键菜单不再出现“放到地面”，背包物品仍可右键“移入装备栏”；拖拽到地面的任意屏幕落点继续通过 `DropInventoryItemAtScreenPosition` 提交并接受服务器复核（输入 Automation、源码扫描；真实 PIE 几何未执行）。

## 6. Definition of Done

- [x] 行为代码先有最小 Red 测试并记录实际失败原因，再实现 Green；测试不通过不得宣称完成。
- [x] 购买、出售、锁定和合成均复用服务器权威公共入口，没有 UI/蓝图旁路。
- [x] Native Widget Automation 和真实 Editor 蓝图编译回读通过：GameMode、PlayerController、PlayerHUD、HUD、HUDItem 共 5 项均 `BS_UP_TO_DATE`，规则/目录/商店类引用正确；无资产修改，因此不重新保存二进制资产。人工视觉命中仍未执行。
- [x] 按 L2 风险完成三 Target、全量 Combat、PIE、Dedicated、cook/资产和 300 秒 soak；未覆盖项如实记录。
- [x] `10-14`、`10-15`、`00-04`、README、`00-01` 和生命周期/网络约束说明已同步。
- [x] `git diff --check`、文档校验和 delivery Gate 通过；生成的 `Saved/` 报告不纳入行为 diff。
- [x] 解锁后的可合成检测有 Red→Green 回归：锁定组件阻止合成，解锁后同一请求尾部立即消费可用组件并生成配方结果；无完整配方时只改变锁定状态。
- [x] 右键菜单不再包含“放到地面”，脚下即时投递路径和近脚常量已删除；拖拽落点入口保留并完成 Red→Green 菜单/输入回归。

## 7. 测试矩阵与命令

| 层级 | 用例/命令 | 预期证据 | 结果 |
| --- | --- | --- | --- |
| Spec/Gate | `python -B Tools/task_gate.py --mode preflight/plan/build/delivery --spec ... --kind feature` | 各阶段 Gate 记录 F0/F1/F2/交付状态 | preflight、plan、build、delivery 均通过 |
| Pure/Unit | `Combat.Economy.*` 直达库存、卖出、锁定、解锁后可合成、锁定组件不足/过期、锁定堆叠 | 新增测试 Red→Green 报告 | 经济报告：9 项（8 success + 1 warning）、0 failed；解锁 Red：`Saved/ItemUnlockCraft/Red/index.json`，最终 Green：`Saved/ItemUnlockCraft/Green2/index.json` |
| World Automation | 现有 `Combat.Items.*`、`Combat.Network.*`、全量 `Combat.` | 无旧物品/权限/生命周期回归 | 最终 `Saved/ECON003Validation/Automation/index.json`：94 success + 2 success with warnings、0 failed、0 not run、96 total；进程退出 0。两项警告来自非法调试命令边界用例 |
| UI Automation | `Combat.UI.Shop.` | 金币按钮结构、开关、无储藏 HUD 接线 | `Saved/CombatEconomy/ECON004-UI-Green/index.json`：1 success、0 failed |
| Input/Item Automation | `Combat.Input.*`、拖拽与菜单相关 `Combat.Items.*` | 右键无放地面入口/请求、拖拽屏幕落点入口保留、菜单动作结构 | Red：`Saved/ItemDropMenu/Red/index.json`（1 failed test / 4 assertions）；Green：`Saved/ItemDropMenu/Green/index.json`（1/1）；回归：`Saved/ItemDropMenu/InputItems/index.json`（8/8）与 `Saved/ItemDropMenu/Items/index.json`（10/10） |
| Release Automation | `Combat.Release.` | v4 发布契约、生命周期和公开扩展面 | `Saved/CombatEconomy/ECON004-Release-Green/index.json`：3 success、0 failed |
| Build | 安装版 `ue_gasEditor Win64 Development`；源码版 `ue_gasServer/ue_gasClient Win64 Development` | 三 Target 编译链接成功 | Editor 最终增量 6 actions、Server/Client 各 5 actions，均 `Result: Succeeded`；源码日志 `Saved/ECON003Validation/BuildServer.log`、`BuildClient.log` |
| PIE / Blueprint | Editor Python 启停 `L_CombatTest` 中的 Demo GameMode；`-CombatEconomySmoke`；相关蓝图编译并回读默认引用 | 真实 PIE 与蓝图结果；MCP 不可用则标降级 | 安装版 NullRHI PIE 通过：1 轮、5 回执、0 失败；5 个蓝图 `BS_UP_TO_DATE`，引用正确。`Saved/ECON003Validation/PIEReport.json`、`EconomyNetworkSmoke.json`、`BlueprintReport.json`；人工鼠标几何未执行 |
| Network / Dedicated | `pwsh -NoProfile -File Tools/RunDedicated.ps1 -InstalledEditor -Economy -TimeoutSeconds 390`；另跑 `-Items -TimeoutSeconds 120` | 双客户端成功闭环；争抢、控制互换、旧 owner 隐私与旧绑定拒绝 | 两次退出 0；经济两端各 10 轮、48 回执、0 失败/重试；物品争抢恰好一胜一负，三端 smoke Pass。`Saved/UEEnvironment/Dedicated-Installed-Economy/`、`Dedicated-Installed/` |
| Soak / Perf | 上述 `-Economy` 每 30 秒重复操作并持续 300 秒；`-Items` 单独启用 64 Unit/256 Modifier 容量 | 重复闭环后活跃资源计数不增长，M7 预算通过 | 两端各运行 300.0 秒，循环后 Modifiers=3、SchedulerSlots=6 保持稳定；P95 最大 9.185 ms、P99 最大 9.298 ms、最大发送 20.912 KiB/s，Budget 全部 Pass。容量场景 P95 9.080 ms、P99 9.230 ms，Budget Pass；不能据此宣称长时间无泄漏 |
| Cook / Assets | `UnrealEditor-Cmd <PROJECT> -run=Cook -TargetPlatform=Windows -Unattended -NoP4 -NullRHI -NoSound`；`-run=CombatAssetValidation -ModelContextProtocolPort=8060 -Report=<REPORT>` | 全量 cook 和资产规则无错误 | Windows cook 650/650 包、退出 0、0 错误；资产报告 31 项、0 错误/警告、退出 0。进程日志各有一条 UE MCP 授权提示警告；`Saved/ECON003Validation/Cook.log`、`AssetReport.json`、`AssetValidation.log` |
| 文档 | `python -B Tools/validate_docs.py`、`python -B -m unittest discover -s Tools/Tests -p 'test_validate_docs.py' -v`、`git diff --check` | 文档链接/空白无错误 | `validate_docs.py`：80 Markdown / 405 links / 0 errors；单测 16/16；`git diff --check`：0 error |

## 8. 风险、回滚与升级

- 风险：删除公开储藏 API 和复制字段后，旧客户端/旧蓝图不能互操作；库存出售仍需保证能力、被动和光环 exactly-once 清理。解锁后自动合成会在一次锁定请求内消费当前可用组件，必须保持库存/实例修订、效果清理和 HUD 投影一致；若无完整配方则不能产生副作用。删除右键放地面入口后，拖拽路径仍需由服务器复核屏幕落点的导航、距离和 LOS；任何 UI 菜单回归都不能重新暴露脚下即时投递旁路。反向移入装备栏继续依赖库存修订校验。
- 回滚方式：以本 Spec 的文件组为单位回退 Economy/Controller/NetworkSecurity/Item/View/UI/Tests/Docs 和 schema 变更，整体恢复上一版发布；不单独关闭经济开关或留下可写半迁移 RPC。
- 触发升级的条件：发现必须删除旧网络字段、需要持久化迁移、需要修改价格/配方/能力语义，或安装版 Editor 无法完成最低编译/Automation 且没有可接受降级时停止并向用户说明。
- 需要人决定的问题：无；用户已明确兼容代码可以删除，本轮按完整 v4 契约收敛。

## 9. 交付证据

- 代码/资产 diff：已完成 C++、Spec/DDD 文档和测试变更；`Source`、`Tools`、`Config`、`Content` 生产扫描未发现 stash/储藏字段、API 或类名；无二进制资产变更，`Saved/` 报告未纳入行为 diff。
- 构建结果：安装版 UE 5.8 `ue_gasEditor Win64 Development` 最终增量 6 actions 成功；源码版 `ue_gasServer/ue_gasClient Win64 Development` 各 5 actions 成功。
- Automation 报告：旧路径 Red/Green、输入和物品专项仍保留于 `Saved/ItemDropMenu/`；最终 `Saved/ECON003Validation/Automation/index.json` 为 94 success + 2 success with warnings、0 failed、0 not run、96 total。经济/商店 UI/Release/解锁专项均由最终全量回归覆盖。
- PIE/蓝图：`PIE.log`、`PIEReport.json`、`EconomyNetworkSmoke.json` 记录真实 NullRHI PIE 买入/锁定/解锁合成/出售闭环；`BlueprintReport.json` 与 `BlueprintValidation.log` 记录 5 项编译回读和 Demo 规则/目录/HUD/原生商店类引用。全部位于 `Saved/ECON003Validation/`，进程退出 0。
- Dedicated/soak：`Saved/UEEnvironment/Dedicated-Installed-Economy/DedicatedSummary.txt` 及两客户端 JSON 记录两端各 300 秒、10 轮、48 回执、0 失败、0 修订重试。资源计数和帧/带宽预算见 §7；`Saved/UEEnvironment/Dedicated-Installed/DedicatedSummary.txt` 单独记录物品争抢、控制互换、隐私/旧绑定拒绝和 64/256 容量。
- Cook/资产：Windows 全量 cook 650 包、0 错误、退出 0；资产校验 31 项、0 错误/警告、退出 0。资产首次进程因与 cook 争用 MCP 8000 端口退出 1，原始日志保留为 `AssetValidation-PortCollision.log`；改用 8060 后复验成功，未更改资产。
- 失败与修复证据：早期 soak 的正常旧经济修订拒绝被测试编排器误判为失败，报告保留于 `Saved/ECON003Validation/StaleFailure/`；仅修复验证器等待复制及有界重试，生产修订拒绝规则保持原样。最终完整重跑无失败、无重试。Dedicated 启动仍含已有动态 GameplayEffect 缺少 Definition 和实验 Python 工具集日志，未将整份日志声明为零错误。
- 文档/Gate 证据：文档工具单测 16/16、UE 环境工具单测 8/8 通过；最终文档校验 80 Markdown / 405 links / 0 errors，`git diff --check` 和 delivery Gate 均通过（0 error）。
- 未执行验证及原因：当前采用 NullRHI 无界面运行，因此未验证渲染分辨率、焦点与人工鼠标命中；未做打包可执行文件部署、超过 300 秒的浸泡或人工网络损伤。源码 Server/Client 编译与安装版独立进程 Dedicated 是两层证据，不等于运行打包后的源码二进制。
- 剩余风险：旧客户端/旧蓝图必须同步升级 v4；渲染几何、部署和更长时间压力仍有上述验证边界。用户已完成本轮代码 review，并明确授权测试完成后本地提交；不推送。

## 10. 变更记录

| 版本 | 日期 | 修改 | 原因 |
| --- | --- | --- | --- |
| 0.1 | 2026-09-16 | 建立 F0/路由、范围、兼容边界、AC、测试与回滚计划 | 用户提出储藏室下线、商店直达库存、出售和锁定合成需求 |
| 0.1 | 2026-09-16 | 完成实现与 F2：商店直达库存、出售/锁定 RPC、锁定过滤自动合成、金币按钮和表现 schema 2；补充 Red→Green、UI、Release、全量 Combat 证据 | 实现阶段发现并修复自动合成后经济投影修订未刷新的边界，保持事务回执与库存快照一致 |
| 0.3 | 2026-09-18 | 删除储藏兼容类、字段、所有权、RPC、清理和公开表现；经济事务只走当前主控单位物品栏，发布契约升级到 v4/schema 3；补跑 24-action 构建、经济/UI/Release/全量 Combat 和文档校验 | 用户明确允许删除兼容代码，要求物品出售流程收敛完整 |
| 0.4 | 2026-09-18 | 右键菜单移除槽位移动项；“放到地面”改为直接投递到当前主控角色脚下附近，删除待选落点状态并保留拖拽任意落点；补充输入、菜单和物品回归计划 | 用户要求优化物品右键并让放地面一步完成 |
| 0.5 | 2026-09-18 | 明确只删除装备物品右键“移入背包”，保留背包物品“移入装备栏”；脚下立即落点和拖拽任意落点语义不变 | 按用户原话收敛菜单变更范围，避免删除未要求的反向整理入口 |
| 0.6 | 2026-09-19 | 解锁后在服务器事务尾部检测当前库存是否满足配方；可合成时立即复用库存稳定合成，无完整配方时只刷新解锁状态；补充效果、投影、旧修订和金币不变回归 | 用户要求解锁时检测是否可以合成 |
| 0.7 | 2026-09-19 | 删除所有物品右键菜单“放到地面”入口及脚下即时投递代码/常量，保留拖拽到地面的屏幕落点提交与服务器复核 | 用户要求去掉右键菜单放到地面，只保留拖动丢到地面 |
| 0.8 | 2026-09-19 | 用户 review 完成；补齐真实 PIE、Dedicated 双客户端 300 秒经济 soak、Server/Client 构建、Windows cook 和资产验证；授权验证后本地提交 | 收口运行验证与提交 |

## 11. 路由、自评与 Reflect

### 路由记录

- 原始需求摘要：下线储藏室运行时入口；金币按钮开关商店；购买直达库存；库存右键出售/锁定；进入库存自动合成且锁定不参与。
- 主 Skill：`combat-feature-development`
- 选择依据：涉及 C++ 经济事务、库存实例、RPC 安全、Native Slate、HUD owner-only View、Automation 和 DDD 同步，是通用 Combat 功能变更。
- 备选 Skill 与排除理由：`combat-skill-development`、`skill-creator`、`visualize` 均不覆盖本次动作目标。
- 路由置信度：`high`

### 交付自评

| 维度 | 权重 | 分数（0–5） | 证据与扣分原因 |
| --- | ---: | ---: | --- |
| 需求与 AC | 20% | 4.5 | AC-01~09 有实现、自动化与运行证据；人工渲染几何未执行。 |
| 架构与权限 | 20% | 4.5 | 服务器权威、修订/绑定/重放/限频和兼容入口均复核并通过回归。 |
| 实现与数据 | 20% | 4.5 | 直接库存事务、锁定投影、自动合成、出售清理和 v4/schema 3 已实现并编译。 |
| 验证证据 | 20% | 4.5 | 全量 Automation、真实 NullRHI PIE、独立 Dedicated 双客户端、cook/资产和 300 秒 soak 通过；人工视觉、打包部署和更长 soak 不在本次证据内。 |
| 文档与可观测性 | 10% | 4.5 | ECON/ITEM/HUD/ADR/README/台账/Release contract 已同步，报告路径可追溯。 |
| 交付卫生 | 10% | 4.5 | 无二进制资产 diff，`Saved/` 报告隔离，Spec、Gate 和回滚边界齐全。 |

- 计算总分：`4.5 / 5（代码 review 与运行矩阵已收口，保留人工视觉和打包部署边界）`
- 硬性封顶或未执行项：NullRHI 不覆盖人工视觉命中；300 秒 soak 不代表长时间稳定性；本地 cook 不代表打包部署验收。
- 自评结论：`COMPLETED`
- 用户验收状态：用户已完成代码 review 并授权测试后本地提交；运行验证已完成，实际结果与未覆盖边界见 §7、§9。

### Reflect 与调优

- 观察与证据：上一轮 ECON-002 将金币入口和交易操作绑定在储藏室；本轮删除了储藏实例、字段、RPC、Widget 和清理链路，库存成为唯一交易域。
- 根因类别：`领域契约`（并暴露出投影生命周期边界）
- 调整文件与预期收益：新增 ADR-060 与经济表现 schema 3，明确单一库存交易域和锁定影响购买计划/堆叠的边界；实现中将购买、出售、锁定统一接到库存修订与 owner-only 投影，并删除所有储藏兼容入口。
- 回归验证：Red→Green 经济用例、UI/Release 用例、全量 `Combat.`、DebugGame 构建、文档校验和 F2 对抗审查；期间发现自动合成在 guard 作用域内推进 `InventoryRevision` 后经济投影仍为旧值，已在 `StabilizeInventoryCrafting` 释放 guard 后刷新投影并复跑出售/锁定回归。
- 需要用户决定的问题：无；旧客户端/旧蓝图不再作为 v4 兼容目标，需按同版本客户端/服务器部署。
