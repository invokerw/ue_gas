# DEF-006 复用既有商店与物品获得即使用

> Spec 版本：`0.2`
> 状态：`READY_FOR_REVIEW`
> Owner：Codex
> 创建日期：2026-10-16
> 关联进度台账：`00-01-Progress-Tracker.md` §12.13
> 风险等级：`L1`

## 0. Intake 与 Gate 记录

- 用户请求：指出已有商店应复用，建议物品增加获得时立即使用的功能；据此调整 DEF-005 的重复实现。
- 附件解释：无附件。用户本轮架构方向优先于 DEF-005 的独立商店方案。
- 已读取入口：agent.md、README、00-01/04/05、10-01/08/09/12/14/15、20-02/03/05、90-16、任务路由与功能开发 Skills，以及 Economy/Shop/Item/ASC/Defense/HUD/网络和测试代码。
- 主 Skill：`Skills/combat-feature-development/SKILL.md`
- 备选 Skill 与排除理由：combat-skill-development 不作为主 Skill，本步是物品交付公共能力与商店复用，不新增玩家施法技能。
- 路由置信度：`high`
- F0 结论：`GO`
- F0 依据：已有 UCombatShopWidget、UCombatShopData、PurchaseShopItem/RPC/Economy 与唯一物品登记表，缺少 slotless 获得即用入口；可兼容扩展，删除重复商店。
- F1 结论：`APPROVED`
- F1 审查人：Codex
- F1 审查版本：`0.2`
- F1 计划审查证据：复核已有 UCombatShopData/Widget/Economy/Inventory 入口，0.2 在修改前复核 §13：bUseOnAcquire 默认关闭、同步获得效果、保持 ActiveAbility 原语义、原子回滚、Run/Phase 限购与普通库存兼容；不增加第二套价格/金币/RPC，结论 APPROVED。
- Build 解锁：已解锁；F1 0.2 计划门通过后运行 Build Gate。
- F1 重审条件：改变现有主动物品/GAS 施法语义、跨局持久化、异步获得效果、配方即用合成或发布部署时升级 Spec。
- F2 结论：`PASS`
- Push-Ready 结论：`READY`
- 验证：最终三 Target Succeeded；199 非 PIE + 6 渲染 PIE 共 205 独立用例通过；93 资产校验和冷回读通过；原经济 RPC 防守双客户端六波、旧物品争抢/64 单位 256 效果、两端 300 秒经济 soak、733 包 cook 通过。
- 未执行：打包 exe 部署、4 人/网络损伤、超过 300 秒的 soak、异步获得效果、正式 §6.3 内容；无本步必需验证缺口。

## 1. 目标与范围

### 目标

防守团队消耗品改为普通 CombatItem，在已有商店中分类展示和右键购买。物品增加“获得即使用”交付方式；购买、拾取、服务器奖励统一调用同一物品入口，成功消耗而不进入九格库存。

### 范围

- `UCombatItemData::bUseOnAcquire` 默认为 false；启用时配置无异步、无施法目标输入的 `UCombatItemAcquireEffect` 实例。公共内置自体治疗行为作为通用示例；防守专用行为转交已有 Director 的全队效果/限购/逐波到期，仍使用公共 Heal/Modifier。
- 物品唯一登记表执行原子领取：标记本实例正在领取，成功后消费并释放实例和地面投影；失败保留原地面实例。GiveItem、TryPickup 和 Economy Purchase 共用。满库存也可使用，数量固定 1，不参与合成/出售/装备主动或被动。
- 复用 CombatShopData 的分类、价格和稳定 CombatItem DefinitionId；复用 Controller 经济 RPC、金币、资源修订和购买事件。原 DEF-005 左侧商店、专用 RPC/组件/数据目录撤除。
- 增加通用商店可用性投影（定义 ID、上下文/修订、剩余次数、购买者、到期/生效状态）；UI 不依赖 Defense 类型。获得效果在服务器提供可用性，客户端只消费快照。
- RPC 的即用购买保留最多 32 条完整请求回执和高水位，重发仍消耗共享限频；同号换载荷拒绝。普通购买/出售/合成规则保持原语义。
- 新六件 CombatItem + 五个独立 Modifier 沿用原型效果，放入防守专用 CombatShopData 目录；恢复 DefenseHUD 的既有金币按钮。普通 Demo 目录保留。

### Non-Goals

§6.3 正式护盾/条件增伤/援军、异步或需玩家选目标的自动施法、即用物品配方/多件批量、四人平衡、重连、部署发布不在本步。

## 2. 当前事实与依据

- PurchaseItem 固定经过九格容量/CommitInventoryResult；GiveItem 与 TryPickup 共用 AcceptItem；现有主动 ValidateActive 要求装备槽，不能简单模拟点击来满足满背包与不占格。
- CombatShopWidget 已有搜索、基础/升级分类、右键购买和金币入口。DEF-005 当前另建目录、组件/RPC/面板，用户明确要求收敛。
- 团队限购和逐波到期由 Director 保存，购买者金币归 Economy。获得行为只执行领域效果，不重复扣款或维护价格。
- 保留未提交 DEF-003/004/005 和用户其它修改，Phase 1 已提交 d7c9809。

## 3. 行为与契约

### 主流程与状态

购买从原商店 RPC 进入 Economy，服务器复核价格、余额、绑定、上下文/修订与领域可用性；创建瞬态物品实例，在事务锁内扣款并调用公共“获得即用”，成功后输出 ConsumedOnAcquire 与稳定物品来源（结果 Handle 为空），失败恢复余额/修订。普通购买继续走原库存计划。

拾取先通过既有 Targeting/共享/距离/修订检查，再进入相同领取入口。奖励 GiveItem 同样复用。领取前标记实例防重入，不先插入槽位；效果成功后移除唯一实例，失败不销毁原地面物品。只有声明可同步完成且正确报告结果的获得效果才能配置，不走主动物品冷却/前摇，不改变已有 Q/物品使用流程。

### 权威、失败与生命周期

全程服务器；客户端不能传金额/效果。团队购买任一已登记连接可发起，死亡仍可购买、复活在有效期内补领；全队一次、精确 Hero+Handle 移除、终局保留限购记录。购买者断线不撤销队友效果。新目录只定义一份价格，Director 只管理效果和限购，不扣金币。共用购买缓存绑定到当前 Controller/Run 上下文；跨 World/旧上下文/旧控制/重入/余额不足无二扣二效。失败、取消、Owner/World teardown 不留下瞬态实例或效果句柄。

### 兼容、版本与迁移

旧资产 bUseOnAcquire=false 保持语义，Content=2 不变。经济 RPC/只读投影新增即用字段，EconomyPresentationSchemaVersion 3→4，ContractVersion 4→5，ReleaseId=`combat_v5_acquisition_rc1`，要求三端同版本；Tag/Event/Formula 不改。先用 UE API 创建 CombatItem/Shop 并替换 GameMode/HUD/Run 引用、编译保存冷回读，再删除未发布的 DefenseTeamShopData 资产/扫描与废弃类、专用请求/视图。无文本改写二进制。

## 4. 实施计划

| 文件/资产 | 变更 | blast radius |
| --- | --- | --- |
| Items/CombatItemData、CombatItemAcquireEffect、ItemSubsystem、Inventory | 通用同步获得即用及领取互斥 | 公共物品，默认关闭 |
| Economy/Types/Component、Controller、NetworkSecurity | 原购买入口支持即用、完整回执、上下文/状态投影 | 经济/RPC |
| UI/CombatShopWidget | 原分类增加即用/限购提示，右键同一路径 | 商店 UI |
| DefenseTeamItemEffect、Director/TeamShop、GameMode | 领域效果适配，去掉扣款/独立 RPC/目录 | 防守 |
| DefenseBuildWidget/PlayerController、旧 TeamShopComponent/Data | 删除重复面板和组件 | 防守 |
| Defense 六件物品/CombatShopData、HUD/模式蓝图、RunData | 原生资产迁移及冷回读 | 可玩防守地图 |
| Tests、脚本、ReleaseContract、文档 | 回归、schema、生命周期和证据 | 版本配套 |

## 5. 验收标准（AC）

- [x] AC-01：防守只用原商店和金币按钮；六件 CombatItem 在分类中右键购买，无左侧重复商店。
- [x] AC-02：获得即用覆盖购买/拾取/奖励；满库存可用、无槽位/实例泄漏；失败保留金币/地面物品。
- [x] AC-03：团队限购/阶段/死亡补领/到期不变；两人争购只成功一次，完整重放不二扣，同号换载荷拒绝。
- [x] AC-04：既有装备、主动、库存合成/出售/锁定与普通 Demo 回归通过。
- [x] AC-05：三 Target、完整 Automation、资产冷回读、原商店真实 PIE、Dedicated 双客户端/容量、cook、文档与交付 Gate 完成。

## 6. Definition of Done

- [x] Red/Green 和 F2 反例通过。
- [x] 资产先迁移引用再移除废弃定义，中文配置完整。
- [x] 公共系统无 Defense 依赖，价格/扣款/购买 RPC 唯一。
- [x] 所有适用验证与未执行边界记录；不提交/不推送。

## 7. 测试矩阵与命令

| 层级 | 用例/命令 | 结果 |
| --- | --- | --- |
| Gate | task_gate preflight/plan/build/delivery + validate_docs + diff check | 0.1/0.2 流程门、最终 docs 与 delivery 报告 |
| World | 获得即用成功/失败/满库存/拾取/奖励/重入/GC/teardown；团队原十项经原入口 | acquire-final.log 30/30；f2-green.log 15/15 |
| 回归 | 完整 Combat 非 PIE 与渲染 PIE 分进程 | combat-delivery.log 199/199；pie-final.log 6/6 |
| Assets | 原生 API 保存编译、冷回读和 CombatAssetValidation | 93 定义 / 0 error / 0 warning；cold-read-final.log |
| Network | 原经济 RPC 双客户端争购、到期、属性对照；旧物品/原 M8 64/256 | network-victory-2；items-capacity 三端 Pass；economy-soak 两端 300 秒 Pass |
| Build/cook | Editor/Server/Client、Windows cook | 三 Target 成功；733 包 / 0 error |

## 8. 风险、回滚与升级

- 风险：即用效果不能承诺任意异步 Ability 的原子回滚；本步明确仅同步效果。保留 GAP-030 动态 GE 复制限制，用属性/视图验证。
- 回滚：按本任务局部 diff 还原，恢复六件目录引用和相应三端 schema；保留原有未提交修改，不 reset 仓库。
- 升级条件：扩大到主动技能自动施法、异步获取、配方即用或账号库存需新审查。
- 需要人决定的问题：无；这是用户已明确要求的本地架构调整，不涉及部署。

## 9. 交付证据

最终报告在 `Saved/DEF-006/verification-summary.json`；逐项证据见 §14–15。首个增量证据仅作历史保留，不替代最终获得成功/失败、资产和网络验证。

## 10. 路由、自评与 Reflect

主 Skill 为 combat-feature-development，置信度 high；旧单独商店重复了现有能力，本轮从原入口收敛。待实际验证后评分，不用旧版测试宣称通过。


## 12. 0.1 历史增量记录（2026-10-16；已由 0.2 替换）

已落地公共第一步：`UCombatItemData::bUseOnAcquire` 默认关闭；启用时校验无目标、零前摇、无引导、无配方、单数量和主动 Ability。`UCombatInventoryComponent` 增加同步 `AcquireOnReceive`，GiveItem/TryPickup 可进入同一入口，成功后实例不占槽并由 Ability End 统一清理；普通物品路径未改。`UCombatEconomyComponent::PurchaseItem` 对无配方即用商品预留原子扣款/失败退款分支。Editor 后缀构建和 `Combat.Economy.ItemAcquireUseValidation` 通过。

尚未把 DEF-005 六件商品迁回 `UCombatShopData`，也没有声称获得即用成功路径、现有主动物品触发、团队 Director 适配或网络证据完成；当时拟接无目标 Ability；0.2 的实际读取和 Red 证明该路径不能提供本任务的失败原子性，已按 §13 改为同步获得效果，以上仅为历史记录。


## 13. 0.2 同步领取边界修订

0.1 仅验证了开关校验与旧经济回归，尚未验证成功路径。继续检查发现它对地面实例直接拒绝、激活成功与执行成功混淆，以及为 slotless 物品放宽普通主动的槽位校验。当前撤回这些未完成分支，保留原有主动物品和 GAS 契约。

实施仍采用 §1 原定的 `UCombatItemAcquireEffect` 同步效果接口：`bUseOnAcquire=true` 时配置该实例，不能再配置 ActiveAbility/被动/光环/配方/充能，数量固定 1。内置自体治疗走 Heal，防守适配器走既有 Director 全队效果；它们与普通“施放物品”共用底层公共结算，不伪装一次法术激活。通用层不知道 Defense 类型。接口返回同步成功/失败且失败须无残留，不支持任意异步/多段部分提交效果。

唯一领取入口在 ItemSubsystem：持有实例领取锁；GiveItem/TryPickup/Economy 均使用，成功后同步释放实例，不依赖槽位或 AbilityEnded 的延迟任务。原 Tick/Order 不变化。收款在 Economy 的同一互斥范围内预扣，失败回滚余额和修订，成功写标准 ItemPurchased 事件和 ConsumedOnAcquire 回执。

通用经济投影新增 `ShopOffers`；每项携带 ContextId/Revision、可购/余量/购买者/到期。现有经济请求增加所选商品上下文，原 Controller API 从只读投影填入。仅即用购买启用 32 条完整载荷回执（普通经济重复仍保留原拒绝语义）；重复也使用连接预算，跨界旧请求失败。目录、价格和购买 RPC 仍各一份。

迁移顺序：先保留废弃 Data 类型用于原生迁移，创建六件 CombatItem/一个 CombatShopData，配置模式/HUD、清 Run 旧引用、编译保存并删除旧目录资产；随后删除旧类型/组件/RPC/面板与扫描，原十项团队测试改测公共入口。新通用例覆盖满库存购买/地面拾取/奖励、失败保留、领取竞争、重复、身份、teardown。


## 14. 已执行证据

- `red2.log`：旧 AcquireOnReceive 拒绝地面实例，三个真实断言失败（未领取/未治疗/实例残留）。
- `green.log`：改用公共同步领取后，物品、团队与经济 23 项通过。`acquire-final.log`：5 个获得场景、10 个团队场景、12 经济与 3 Release 场景共 30 项通过。
- `combat-full.log`：199 项完整非 PIE 通过；`pie-final.log`：3 个 AI、个人选择、真实导航、原商店右键购买共 6 个渲染 PIE 通过，总计 205 个独立测试。截图 `original-shop-pie.png` 确认原商店与金币按钮，已移除左侧第二面板。
- `f2-red.log` 检出治疗事件回调 Shutdown 后账本被重建。修正已提交治疗的结束边界以及发布接收者快照，`f2-green.log` 15/15 复验。
- `asset-migration2.log`/`asset-migration.json`：六个 CombatItem、统一 CombatShopData、Mode/HUD/PC 蓝图 BS_UP_TO_DATE；Run 断开旧引用并经 UE API 删除旧目录。迁移前 MCP 确认没有 PIE、没有未保存资产，再正常关闭编辑器释放文件锁；未强制终止用户编辑器。
- `asset-validation.json`：93 定义，0 error / 0 warning。旧扫描类型已删除，不漏扫新物品。
- `network-victory-2/report.json`：原经济 RPC 的 Dedicated 服务器 + 双客户端三端 Pass，六波胜利、两个团队效果正确领取/到期、无二扣、最终两英雄 × 12 属性一致。
- `economy-capacity-console.log`：原普通商店 300 秒双客户端 soak，两端各 10 轮通过；这个场景实际为 3 单位，不冒称 64/256 容量。64/256 由独立 Items smoke 另行记录。
- 最终构建：`item-probe-final-build.log`（安装版 Editor）、`server-delivery-build.log` 与 `client-delivery-build.log`（源码 Target）全部 Succeeded。项目临时构建工作集配置已恢复原文件。


## 15. 最终交付与 Reflect（2026-10-16）

- 冷回读：`cold-read-final.log`/`cold-readback.json` 确认 6 个普通 CombatItem、原 CombatShopWidget、统一目录引用、旧 Data/Component 类型与旧目录资产不存在。首次 Python 回读把已解析 soft-reference 对象误当路径字符串，修正脚本读取后通过；没有修改真实资产来迎合脚本。
- Cook：`cook.log` 733 总包（726 cooked、7 platform-skipped），0 error / 1 个既有 MCP 许可 warning；未运行打包 exe。
- 物品/容量：`items-capacity/` 保留三端最终日志；64 Unit / 256 Modifier 初始化基线和容量预算 Pass，普通药剂使用/背包/丢捡、争抢一胜一负、控制转移、旧 owner 隐私/绑定拒绝均 Pass。两轮首次 smoke 曾在一端拒绝本地使用，诊断证明 Operate=0、Spec=1、Data=DA_item_restore；旧容量夹具替换英雄后没有建立本地主选。测试改用已有 SelectCombatUnit 入口后通过，未修改生产选择/物品输入规则，也未绕过高层使用断言。
- 经济：`economy-soak/` 三端日志中两客户端各 10 轮、300 秒、0 失败；该场景为 3 Unit，独立于上述 64/256 夹具。
- 网络证据使用同版本安装版 Editor 的 `-server/-game` 独立进程；源码 Server/Client 构建单独通过，不将此称为打包 exe 验证。保留 GAP-027 的测试进程隔离和 GAP-030 动态 GE 完整定义复制限制；真实属性与只读快照已核对，未改 ASC 模式掩盖问题。
- F2：本地按最终 diff/AC 独立检查角度复核；关闭地面领取拒绝、激活/效果成功混淆、即用槽位旁路、完整回执、价格/定义冻结、死者补领、精确到期、治疗后 Shutdown 重建账本、跨观察者迭代、UI 遮挡和旧 smoke 主选前置条件。未使用子 Agent。
- Push-Ready 六层：Tests/Types-Build/No Regression/Adversarial/DDD-Constraints/Decisions 全部 PASS。版本为 Contract 5 / Economy presentation 4；客户端服务器同版本运行，普通旧物品默认关闭获得即用，Content 仍为 2。
- Reflect：这次采用既有商店/物品/经济入口，模式只实现领取效果和限购状态；没有再维护第二套价格、UI 或 RPC。失败被固化为有状态反例，未调整通用 Skill。后续正式团队机制沿这个效果扩展点实现。
- 用户验收状态：`READY_FOR_REVIEW`，未标已验收；未提交、未推送。下一步由用户验收原商店流程或继续正式内容，不再补建独立商店。

### 六维自评

| 维度 | 权重 | 分数 | 证据与扣分 |
| --- | ---: | ---: | --- |
| 需求与 AC | 20% | 4.5 | 原商店复用和获得即用三条来源闭合，正式机制明确后续 |
| 架构与权限 | 20% | 4.5 | 单一目录/金币/RPC，领域适配、上下文与限频，保留既有 GE 限制 |
| 实现与数据 | 20% | 4.5 | 6 物品迁移、旧类型删除、同步领取失败回滚；异步不支持 |
| 验证证据 | 20% | 4.6 | 205 用例、三 Target、93 资产、真实原 UI、双客户端、旧物品/容量与 300 秒 soak |
| 文档与可观测性 | 10% | 4.5 | 版本/公共 API/README/台账/ADR/生命周期与诊断同步 |
| 交付卫生 | 10% | 4.5 | 保存回读再删除、临时配置恢复、LFS/语法/差异检查、无提交推送 |

加权 4.52，显示 **4.5/5**；自评不代替用户验收。最终机器交付证据为 `Saved/DEF-006/delivery-final.json`。

最终卫生检查：98 Markdown/534 本地链接、Python AST、PowerShell 解析、git diff --check 与新资产 LFS 属性均通过。临时构建配置已按字节恢复。
