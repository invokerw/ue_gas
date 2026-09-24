# RES-001 玩家战略资源与英雄库存边界

> Spec 版本：`0.2`
> 状态：`COMPLETED`
> Owner：Codex
> 创建日期：2026-09-23
> 关联进度台账：[00-01](../00-Project/00-01-Progress-Tracker.md)
> 风险等级：`L1`

## 0. Intake 与 Gate 记录

- 用户请求：金币和其他战略资源归玩家、库存仍属于英雄；后续明确“资源的添加没有必要显示在战斗 UI 日志里面”。本版保留资源权威事件与账本，但从玩家战斗记录投影中排除纯资源余额变化。
- 验收与提交授权：2026-09-24 用户明确“验收完毕，提交吧”，确认 RES-001 0.2 验收完成并授权创建本地 Git 提交；不推送远端。
- 附件解释：`无附件`；实现依据当前仓库源码、经济专题和用户在本轮明确的所有权边界。
- 已读取入口：`agent.md`、`README.md`、`00-01`、`00-03`、`00-04`、`00-05`、`10-01`、`10-08`、`10-14`、`10-15`、`HUD-LOG-001`、现有 ECON/ITEM Spec、`Skills/combat-task-router/SKILL.md`、`Skills/combat-feature-development/SKILL.md`，以及 Economy、Inventory、Unit、PlayerController、Damage、Log 和 Automation 源码。
- 主 Skill：`combat-feature-development`（`Skills/combat-feature-development/SKILL.md`）。
- 备选 Skill 与排除理由：`combat-skill-development` 不适用，本任务不新增玩家可施放 Ability；`skill-creator` 不适用，本任务不修改 Skill；`visualize` 不适用，本任务是运行时所有权和日志路由修正。
- 路由置信度：`high`
- F0 结论：`GO`
- F0 依据：0.1 已完成玩家资源/英雄库存分域；当前 `Event.Combat.GoldChanged` 仍被战斗日志分类为物品事件并进入 owner-only 历史，与用户最新展示边界不符。可只收窄 `UCombatLogComponent` 的表现分类，不修改服务器资源账本、奖励结算、核心事件、RPC 或复制 schema。
- F1 结论：`APPROVED`
- F1 审查人：Codex（本地计划审查）
- F1 审查版本：`0.2`
- F1 计划审查证据：已确认仅让 `CombatLogPresentation::Classify(Event.Combat.GoldChanged)` 返回 false；集成测试同时断言 AddGold 仍精确落账且核心 `UCombatEventSubsystem` 仍保存该事件。购买/出售/合成、物品变化与公共战斗事件维持既有分类、owner-only 路由和 schema。无需资产、RPC、复制或版本迁移，准许进入 BUILD。
- Build 解锁：`已解锁`；0.2 `preflight`、`plan`、`build` Gate 均已通过。
- F1 重审条件：若改为 `PlayerState` 持久化、增加跨重连迁移，或修改核心 `FCombatLogRecord`/客户端 wire schema，必须递增 Spec 版本并重新审查；本版明确不做这些扩展。
- F2 结论：`PASS`
- Push-Ready 结论：`READY`
- 验证：0.2 已完成行为 Red→Green、安装版 UE 5.8.2 增量构建、Economy 11/11、UI Log 5/5、88 篇 Markdown/471 条本地链接与差异检查；delivery Gate 见第 7/9 节。0.1 的 Items 10/10 证据继续保留。
- 未执行/降级：未修改二进制资产；未重复跑 Dedicated/cook/长 soak，因为本轮不改 RPC/wire schema、不新增 Tick/调度循环，且已有 ECON-003/ITEM-001 运行矩阵作为基线。

## 1. 目标与范围

### 目标

1. 把本局玩家的战略资源边界显式化：金币及未来同类资源由玩家级经济宿主持有，英雄只持有自己的战斗属性和物品库存。
2. 让英雄切换不迁移、不清空玩家资源；旧英雄产生的延迟击杀奖励仍记入其稳定玩家所有者。
3. 服务器继续记录玩家资源事件用于诊断，但纯资源余额变化不进入战斗 UI 日志；购买、出售、合成和英雄库存事件仍按玩家所有者投影。
4. 提供分离的玩家资源视图和英雄库存视图 API，同时保留当前 owner-only 经济快照的兼容读取入口，避免无必要的客户端 wire schema 迁移。

### 范围

- 在 `ACombatUnitCharacter` 上增加与瞬时 `CommandingPlayerController` 分离的、服务器侧稳定玩家资源所有者引用；绑定英雄时建立，玩家断开时清理，英雄切换时不清理旧英雄所有者。
- 击杀金币和库存自动合成/投影回调使用稳定资源所有者；库存实例、`Holder`、库存修订和英雄级能力效果继续留在 `UCombatInventoryComponent`/英雄上。
- 增加 `FCombatPlayerResourceView`、`FCombatHeroInventoryView` 及 `UCombatEconomyComponent` 的分域读取/变化通知 API；`FCombatEconomyView` 保留为兼容的 owner-only 聚合 envelope，不再被解释为资源归英雄。
- 扩展仅用于同步展示路由的 `FCombatLogResourceChange`，记录玩家 owner id；购买/出售/合成及玩家拥有英雄的库存变化只进入对应玩家日志，纯 `GoldChanged` 不进入战斗 UI，公共战斗相关性日志维持原规则。
- 将原“无主控英雄金币日志可见”回归改为“金币增加不进入任何玩家战斗日志”，并覆盖分类器与双玩家投影；同步经济架构、ADR/Gap、README/台账中受影响的当前事实。

### Non-Goals

- 不新增 `PlayerState`、跨重连持久化、交易/赠送、团队共享资源或跨局存档；本版以当前 `PlayerController` 作为本局玩家资源宿主，后续迁移需独立 Spec。
- 不把 HP/Mana、经验、等级、技能点或其他英雄战斗属性搬到玩家；这些继续由英雄 ASC/Progression 持有。
- 不把物品实例、库存槽、`Holder`、库存修订、装备能力或合成结果搬到玩家；商店事务仍把物品交付到当前主控英雄库存。
- 不改变价格、金币上限、被动收入、奖励数值、库存容量、网络 RPC 载荷或核心 `FCombatLogRecord` schema；owner id 只存在于同步展示上下文。
- 不修改二进制蓝图/DataAsset；若资产只读回归需要 Editor，记录降级方式。

## 2. 当前事实与依据

- 相关 DDD：`10-15-Economy-Shop-Crafting.md`、`10-14-Item-System.md`、`10-08-Data-Network-Observability.md`、`00-04-Decisions-Gaps.md`、`HUD-LOG-001-combat-log.spec.md`。
- `Source/Combat/Combat/Economy/CombatEconomyComponent.h:16-18,45-76`、`CombatEconomyTypes.h:61-101`：金币账本已经在 PlayerController 经济组件；新增资源/英雄库存分域 view/delegate，旧聚合 envelope 保持兼容。
- `Source/Combat/CombatPlayerController.cpp:60-137`、`Source/Combat/Combat/Unit/CombatUnitCharacter.cpp:72-151`：`CommandingPlayerController` 继续承担网络 owner/输入绑定，新增独立 `ResourceOwnerPlayerController`；切换只清除前者，Controller teardown 扫描清理后者。
- `Source/Combat/Combat/Combat/CombatDamageSubsystem.cpp:192-200`：击杀金币现在从稳定资源 owner 解析玩家，旧英雄延迟奖励不再因切换丢失。
- `Source/Combat/Combat/Economy/CombatEconomyComponent.cpp:809-868`：资源事件不填当前英雄 source/target，并继续写入核心服务器事件；库存经济事件保留英雄归因与玩家 recipient。
- 当前待修事实：`CombatLogPresentation::Classify` 仍把 `Event.Combat.GoldChanged` 归入物品类，`UCombatLogComponent` 因此会把资源增加写入战斗 UI 历史；0.2 将仅从表现分类中排除该事件。
- `Source/Combat/Combat/Items/CombatInventoryComponent.cpp:39-67,146-179,591-627`：实例 Holder、库存修订和槽位变化仍属于英雄；自动合成、投影刷新和私有库存展示改查稳定资源 owner。
- 0.1 Automation 基线：`Combat.Economy.` 11/11、`Combat.UI.Log.` 5/5、`Combat.Items.` 10/10 通过；0.2 展示边界尚未实现或验证。
- 已知限制：`ACombatPlayerController` 是本局资源宿主，不保证 Controller 重建后的跨连接恢复；这属于 Non-Goal，需未来 `PlayerState` 迁移 Spec。

## 3. 行为与契约

### 主流程

```text
玩家绑定英雄
  -> 建立 Unit.ResourceOwner（稳定玩家关联）
  -> CommandingPlayerController 只负责当前输入/网络绑定

战斗奖励或玩家资源变化
  -> 读取 ResourceOwner
  -> 修改 PC.EconomyComponent 的玩家资源账本
  -> 继续写服务器权威 Combat Event 用于诊断
  -> 纯 GoldChanged 不投影到战斗 UI 日志

英雄库存事务
  -> 读取当前/指定英雄的 UCombatInventoryComponent
  -> Holder、槽位、修订、能力和合成仍在英雄域
  -> 需要玩家经济回调时使用该英雄的 ResourceOwner
```

### 状态转换

- `Unowned Unit -> PlayerOwned Unit`：服务器绑定指挥英雄时设置稳定资源所有者；重复绑定同一玩家幂等。
- `PlayerOwned Unit(Commanded) -> PlayerOwned Unit(Uncommanded)`：清除瞬时指挥/网络 owner，但保留资源所有者和英雄库存。
- `PlayerOwned Unit A -> PlayerOwned Unit B`：玩家资源账本不变；A 保留其资源所有者，B 建立同一玩家资源所有者；当前库存投影切到 B。
- `PlayerOwned Unit -> Destroyed/Player EndPlay`：Unit 销毁自然结束引用；Controller EndPlay 显式清理其拥有英雄的稳定 owner，避免悬空玩家引用。
- `GoldChanged -> Combat log projection`：表现分类直接拒绝，不进入任何玩家的战斗记录 FastArray；核心服务器事件仍保留。
- `Inventory/economy transaction -> Log projection`：有 owner id 的购买、出售、合成和私有库存事件绕过 Unit 相关性筛选，仅接受匹配玩家；公共战斗事件继续按原 Unit 相关性筛选。

### 输入、输出与数据约束

- `FCombatPlayerResourceView` 只包含玩家战略资源字段（当前为 Gold、Cap、被动收入和 ResourceRevision）；`FCombatHeroInventoryView` 只包含库存修订和 `FCombatItemView` 数组。
- 资源视图和库存视图均为服务器权威 owner-only 读取；资源 revision 与库存 revision 独立递增。
- `FCombatLogResourceChange::OwningPlayerId` 是服务器进程内展示路由 id，0 表示没有 owner-only recipient；不进入 `FCombatLogRecord` 核心 schema、不作为客户端请求输入。
- 纯金币事件不伪装成英雄来源，其 Unit source/target 可为 0；该事件只保留在服务器核心事件/诊断中，不生成 `FCombatLogEntry`。经济购买/出售/合成事件可保留当前英雄作为库存归因，recipient 仍为玩家。

### 权威边界与权限

- 只有服务器 `UCombatEconomyComponent` 修改玩家资源；客户端只读取分域视图并提交既有经济意图。
- 只有服务器 `UCombatInventoryComponent` 修改英雄库存；客户端不能通过新的视图 API 写资源或库存。
- 稳定资源所有者引用只允许服务器设置；不能由客户端 RPC 或复制字段注入。
- 日志 recipient 由服务器根据实际 PC/Unit 关系生成，客户端不能选择可见玩家。

### 失败、取消、过期、死亡、EndPlay 与重复请求

- 没有稳定资源所有者的 AI/中立单位不产生玩家金币奖励；实现不回退到瞬时指挥 owner，所有合法玩家绑定入口都会先建立稳定 owner。
- 英雄死亡不清除玩家资源 owner；死亡期间库存仍属于该英雄，玩家资源账本不扣除。
- 英雄切换/旧绑定 RPC 仍按原 `CommandBindingGeneration` 拒绝；这不影响旧英雄已经提交的服务器奖励。
- PC EndPlay、Unit EndPlay 和组件 EndPlay 清理弱引用/调度；日志订阅解绑，不保留跨 World 指针。
- 资源事务继续使用现有经济 guard、修订和 exactly-once 入口；本任务不新增可重放写入口。

### 兼容、版本与迁移

- 保留 `GetEconomyView()`、`GetEconomyRevision()` 和既有经济 RPC 字段，新增分域视图/通知作为明确 API；现有 UI 可渐进迁移。
- 不修改核心 EventSchemaVersion、EconomyPresentationSchemaVersion 或线上 wire payload；展示上下文新增字段只影响同版本服务器日志投影。
- 文档把旧的“经济/库存混合 view”解释修正为兼容 envelope；历史 Spec/History 不改写，只在当前 ADR/专题追加事实。
- 回滚可按文件组移除稳定 owner、展示 recipient 和分域 API，恢复旧的指挥 owner 奖励/日志路径；不需要资产迁移。

## 4. 实施计划

| 文件/资产 | 变更 | 原因 | blast radius |
| --- | --- | --- | --- |
| `CombatEconomyTypes.h` | 增加玩家资源视图、英雄库存视图及比较/注释 | 公开所有权分域，保留旧聚合 envelope | Economy/UI/Automation 编译接口 |
| `CombatEconomyComponent.h/.cpp` | 增加分域 getter、变化 delegate；按资源/库存域刷新通知；经济事件写 owner recipient，纯金币不绑定当前英雄 | 资源归 PC 玩家、库存归当前英雄且 revision/视图可区分 | 经济事务、HUD、日志 |
| `CombatUnitCharacter.h/.cpp` | 增加服务器稳定资源 owner 引用和 setter/getter；与瞬时 Commanding owner 分离 | 英雄切换后延迟奖励仍找到玩家，EndPlay 不留悬空引用 | Unit/Network/Lifecycle |
| `CombatPlayerController.cpp` | 绑定时建立资源 owner，EndPlay 时清理；保持库存投影跟随 CommandedUnit | 明确玩家资源生命周期 | Command binding/teardown |
| `CombatDamageSubsystem.cpp` | 击杀奖励从稳定资源 owner 解析玩家 | 修复英雄切换后的奖励丢失 | Death/reward |
| `CombatInventoryComponent.cpp` | 自动合成、库存投影和私有库存展示事件使用稳定 owner；实例 Holder/槽位不变 | 保持库存英雄归属，同时支持旧英雄事务 | Items/Economy/Log |
| `CombatLogTypes.cpp` | `Classify` 排除 `Event.Combat.GoldChanged`；保留核心事件与其他经济/库存分类 | 资源增加不属于战斗 UI 记录 | Log/UI presentation only |
| `CombatEconomyTests.cpp`、`CombatLogTests.cpp` | 把无主控金币日志用例改为双玩家均无投影，并直接断言分类器拒绝纯金币事件 | 固化用户最新展示边界 | Automation |
| `10-15`、`10-14`、`00-04`、README、`00-01` | 同步当前所有权决策、限制和验证证据 | 避免 DDD 与实现漂移 | 文档/Gate |

## 5. 验收标准（AC）

- [x] AC-01：金币及当前支持的玩家战略资源只由玩家级经济宿主持有；切换/死亡英雄不会清空或复制资源，英雄 ASC/Progression 与物品库存仍保持英雄级。
- [x] AC-02：玩家绑定英雄后建立稳定资源 owner；切换到新英雄后旧英雄产生的合法延迟击杀奖励仍记入同一玩家，未绑定 AI/中立单位不凭空获得玩家金币。
- [x] AC-03：分域 API 能分别读取玩家资源视图和英雄库存视图，资源 revision 与库存 revision 独立；现有经济请求和 owner-only HUD 不回归。
- [x] AC-04：无论是否存在主控英雄，纯资源增加/余额变化都不进入任何玩家的战斗 UI 日志；核心服务器事件仍生成，购买/出售/合成、库存和公共战斗事件维持既有投影与隐私边界。
- [x] AC-05：英雄库存实例 `Holder`、槽位、库存修订、装备能力、锁定和自动合成仍属于英雄；切换投影只改变当前显示，不搬迁实例。
- [x] AC-06：PC/Unit/组件 EndPlay 清理稳定 owner、调度和日志订阅；旧 command binding、重复请求和无效 owner fail-closed。
- [x] AC-07：相关 Automation、UE 构建、文档校验、F2 对抗检查和 delivery Gate 通过；未执行项如实记录，用户于 2026-09-24 明确验收完成。

## 6. Definition of Done

- [x] 0.2 行为先有最小 Red 测试并记录实际失败原因，再实现 Green。
- [x] 资源奖励、经济事务、库存事务和日志投影均复用服务器权威公共入口，无客户端旁路。
- [x] 0.2 直接 `Combat.Economy.*` 与 `Combat.UI.Log.*` 回归通过；F2 确认生产变更仅为日志分类，Items 不需重复。
- [x] 安装版 UE 5.8 Editor 增量构建通过；网络/Dedicated 不适用理由已记录。
- [x] `10-15`、`10-08`、`00-04`、README、`00-01` 和当前 Spec 同步；历史文档不改写。
- [x] `python -B Tools/validate_docs.py`、`git diff --check`、delivery Gate 通过，且未混入用户既有 `.uasset` 修改或生成日志。

## 7. 测试矩阵与命令

| 层级 | 用例/命令 | 预期证据 | 结果 |
| --- | --- | --- | --- |
| Spec/Gate | `python -B Tools/task_gate.py --mode preflight/plan/build/delivery --spec Doc/CombatSystem/Specs/RES-001-player-resource-ownership.spec.md --kind feature` | 0.2 F0/F1/Build/F2/交付机器 Gate 输出 | preflight/plan/build/delivery 全部通过；delivery 0 error、23 changed files |
| Pure/Unit | `Combat.Economy.PlayerResourceChangeIsNotProjectedToCombatLog`、`Combat.UI.Log.FilterAndFormatting` | AddGold 后 owner/other 战斗日志均为 0；分类器拒绝 `GoldChanged`，但资源账本仍精确增加 | Red：owner UI 1/期望 0；Green：余额 625、核心事件 delta 25、owner/other UI 0/0；物品事务分类保持 Item |
| World Automation | `Combat.Economy.`、`Combat.UI.Log.` | 经济账本与战斗日志回归 | 最终头版本 Economy 11/11、UI Log 5/5，0 failed |
| Network / Dedicated | 本轮未重复 `CombatEconomySmoke`/`CombatItemsSmoke`；引用 ECON-003/ITEM-001 已有矩阵 | owner-only 经济投影和旧请求回归；不新增 wire payload | 降级：无 RPC/wire/Tick 变化，沿用既有通过证据 |
| 文档/本地工具 | `python -B Tools/validate_docs.py`、`git diff --check` | 文档链接/格式/空白检查 | 88 篇 Markdown、471 条本地链接、0 error；diff check 通过 |
| PIE / Blueprint | 不修改二进制资产；仅做源码/UI Automation | 资产无变更证据 | N/A；用户既有 `.uasset` 修改从本任务差异中排除 |
| Soak / Perf | 不新增 Tick/调度槽；不做长 soak，除非回归发现资源调度受影响 | 说明不新增运行时循环 | N/A |

## 8. 风险、回滚与升级

- 风险：稳定 owner 只在本局 Unit/Controller 生命周期内有效；若未来需要断线重连，必须迁移到 PlayerState 并处理状态恢复。Transient recipient id 只能用于服务器展示投影，不能用于跨进程 replay。
- 回滚方式：按 Spec 第 4 节文件组回退；保留用户已有 `ST_CombatAI_Root.uasset` 修改，不回退无关资产。
- 触发升级的条件：需要 PlayerState/跨局持久化、核心 EventSchemaVersion 变化、多人交易/团队资源或第三种资源的客户端 wire 复制时停止并新建 Spec。
- 需要人决定的问题：无；用户已明确本轮所有权边界。若产品要求断线重连仍保留资源，需要另行确认。

## 9. 交付证据

- 代码/资产 diff：源码、当前文档和 Spec 有变更；未修改二进制资产，保留既有 `ST_CombatAI_Root.uasset` 工作区修改。
- 构建结果：安装版 UE 5.8.2 Development 增量模块构建通过（0.2 最终模块后缀 `Combat,61013`；4 actions，Result Succeeded）。
- Automation/PIE/Dedicated 报告：0.2 最终头版本日志为 `Saved/RES001v02EconomyFinal.log`（Economy 11/11）与 `Saved/RES001v02LogFinal2.log`（UI Log 5/5）；0.1 库存边界日志 `Saved/RES001ItemsFinal.log`（Items 10/10）继续有效。0.2 生产变更只删除 `GoldChanged` 的战斗日志分类，未重复 Items。Saved 日志均为本地证据，不纳入提交。
- Red 证据：`Saved/RES001v02Red.log` 中 AddGold 已把余额改为 625 且核心事件存在，但 owner 战斗 UI 为 1 条（期望 0）；`Saved/RES001v02Green.log` 在同一场景验证余额与核心事件保留、owner/other UI 均为 0。0.1 的延迟奖励 Red→Green 证据继续保留。
- 交付检查：`Tools/validate_docs.py` 验证 88 篇 Markdown、471 条本地链接、0 error；`git diff --check` 通过；delivery Gate 为 0 error、23 changed files。
- 未执行验证及原因：未重复 Dedicated/cook/长 soak；本轮不修改 RPC/wire schema、不新增 Tick/调度循环，且现有 ECON-003/ITEM-001 运行矩阵已覆盖相关网络/资产边界。PIE/Blueprint 为 N/A（无二进制资产变更）。
- 剩余风险：资源 owner 是本局 Unit/Controller 生命周期锚点，不支持断线重连/跨局持久化；团队资源和 PlayerState 迁移需另立 Spec。
- 用户验收与提交授权：2026-09-24 用户确认验收完毕并要求提交；授权范围为本 Spec 的源码、测试与文档，不包含既有 `ST_CombatAI_Root.uasset` 差异，不推送远端。

## 10. 变更记录

| 版本 | 日期 | 修改 | 原因 |
| --- | --- | --- | --- |
| 0.1 | 2026-09-23 | 建立玩家战略资源/英雄库存边界、稳定 owner、日志 recipient 和分域视图计划 | 用户明确所有权边界 |
| 0.2 | 2026-09-24 | 纯资源余额变化退出战斗 UI 日志，保留服务器核心事件与经济/库存事务投影 | 用户明确资源添加无需显示在战斗 UI 日志 |

## 11. 路由、自评与 Reflect

### 路由记录

- 原始需求摘要：金币和其他战略资源属于玩家，库存属于英雄；追加要求纯资源添加不显示在战斗 UI 日志。
- 主 Skill：`combat-feature-development`
- 选择依据：涉及 C++ 运行时所有权、网络 owner-only 投影、生命周期、日志和 Automation，不是单一 Ability。
- 备选 Skill 与排除理由：见第 0 节。
- 路由置信度：`high`

### 交付自评

| 维度 | 权重 | 分数（0–5） | 证据与扣分原因 |
| --- | ---: | ---: | --- |
| 需求与 AC | 20% | 5.0 | 资源归属、英雄库存和“资源增加不进战斗 UI”均有直接 AC/Automation |
| 架构与权限 | 20% | 5.0 | 只收窄只读表现分类；服务器账本、核心事件、RPC、复制与 schema 均不变 |
| 实现与数据 | 20% | 4.5 | 单点分类修正，余额和诊断事件保持；当前实际战略资源仍只有金币 |
| 验证证据 | 20% | 4.5 | 0.2 Red→Green、最终 Editor、Economy 11/11、UI Log 5/5；未重复不相关网络/资产矩阵 |
| 文档与可观测性 | 10% | 5.0 | 10-08/10-15/ADR/README/台账/Spec 明确区分服务器诊断与战斗 UI |
| 交付卫生 | 10% | 4.5 | 未改二进制资产并保留用户既有 uasset；Saved 日志不纳入提交 |

- 计算总分：`4.8 / 5.0`。
- 硬性封顶或未执行项：无硬性封顶；Dedicated/cook/soak 未重复执行，按无 wire/资产/循环变化的风险边界降级记录。
- 自评结论：`EVIDENCE_SUFFICIENT`
- 用户验收状态：`用户已验收`（2026-09-24）；用户明确要求创建本地提交，不推送远端。

### Reflect 与调优

- 观察与证据：0.1 把资源事件路由到正确玩家后，用户进一步明确资源增加本身不属于战斗 UI；0.2 Red 证明账本与诊断正确但 UI 多出 1 条。
- 根因类别：`领域契约`
- 调整文件与预期收益：核心 `GoldChanged` 保持可观测，表现分类器显式拒绝它；避免战斗记录被被动金币、击杀金币或调试改金刷屏，同时不影响购买/出售/合成和物品日志。
- 回归验证：安装版 UE 5.8.2 最终构建成功；Economy 11/11、UI Log 5/5，专项验证余额/核心事件保留、双玩家 UI 均无资源行；Items 10/10 沿用 0.1 同一工作区基线。
- 需要用户决定的问题：无；跨重连资源持久化另行立项。
