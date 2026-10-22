# DEF-014 Phase 3E 防守装备首批四件

> Spec 版本：`0.2`
> 状态：`已验收`
> Owner：Codex
> 创建日期：2026-10-21
> 关联进度台账：`00-01-Progress-Tracker.md` §12.13
> 风险等级：`L1`

## 0. Intake 与 Gate 记录

- 用户请求：DEF-013 验收和本地提交后“继续下一步吧”；按已说明顺序推进防守装备。
- 附件解释：无新附件；agent.md 为工程约束，设计 §6.2 为需求依据。
- 已读取入口：`agent.md`、`README.md`、`Doc/CombatSystem/00-Project/00-01-Progress-Tracker.md`、`00-03-Test-Plan.md`、`00-04-Decisions-Gaps.md`、`00-05-AI-Native-Development-Workflow.md`、`10-01-Scope-Architecture.md`、`10-14-Item-System.md`、`10-15-Economy-Shop-Crafting.md`、`20-02-M6-Skill-Template-Checklist.md`、`20-03-M8-Public-Extension-Guide.md`、`20-05-Roguelite-Defense-Game-Design.md`、`90-16-M8-Lifecycle-Audit.md`、`Skills/combat-task-router/SKILL.md`、`Skills/combat-feature-development/SKILL.md`；Economy/Inventory/Shop/Defense/Attack 源码与专项测试。
- 主 Skill：`Skills/combat-feature-development/SKILL.md`
- 备选 Skill 与排除理由：`combat-skill-development` 不作为主 Skill，本步无玩家主动技能；常驻效果使用公开 Modifier Hook。
- 路由置信度：`high`
- F0 结论：`GO`
- F0 依据：工作区干净；装备在已授权的 Phase3 方向内，四项机制可通过既有 GAS/Modifier/Inventory 表达；剩余四件需要独立专项契约，不纳入本切片。
- F1 结论：`APPROVED`
- F1 审查人：Codex（既有L1授权内本地计划审查）
- F1 审查版本：`0.2`
- F1 计划审查证据：preflight/plan均0错误；复核默认空策略不影响旧资产、仅普通叶子允许策略、已有网络格式和有界回执复用、真实库存限持有、配额在经济互斥内记录且以已接受Offer的PhaseGeneration冻结、GAS唯一属性/Hook唯一伤害入口、失败原子性/teardown/资产回滚与三Target/PIE/双客户端矩阵。0.2 preflight/plan再次通过；追加审查默认0兼容、三入口统一只读限额与策略配方禁用，批准0.2四件切片，其他四件不纳入。
- Build 解锁：`已解锁`；0.1 与 0.2 的 preflight/plan/build 均通过，报告见 Saved/DEF-014/*gate*.json 与 *-v2.json。
- F1 重审条件：扩大至其他四件装备、回城/专项冷却/折扣、新网络字段或发布契约时递增 Spec 并重审。
- F2 结论：`PASS`
- F2 依据：按 AC、原始差异和失败路径重新审查，修复提交后英雄结束的稳定付款人、旧窗口归属，以及旧商店 PIE 的可视区交互。见 §9。
- Push-Ready 结论：`READY`
- Push-Ready 边界：本地交付门已通过；用户于 2026-10-22 确认验收成功并授权本地提交，不推送。
- 验证：三 Target Development 构建、248/248 非 PIE、3/3 渲染商店 PIE、59/59 工具测试、154 资产0错误0警告、幂等迁移/冷读、WindowsServer/WindowsClient Cook/Stage/Archive 和独立双客户端通过。UE MCP 8000/8055 拒绝连接，降级使用同版本原生 UE Python/Automation/渲染 PIE。
- 未执行：全部14项 PIE 的全量复跑、长期 Soak/装备平衡、3/4人容量和剩余四件机制；本步执行与商店直接相关的3项渲染 PIE，长期与扩展内容留后续。

## 1. 目标与范围

### 目标

在既有 L_Defense 的原 Combat 商店加入四件真正占库存、随装备位置生效的防守装备，并保护购买窗口、限持有和幂等事务。

### 范围

| 物品 ID | 名称 | 价格 | 机制 |
| --- | --- | ---: | --- |
| item_iron_bow | 铁制弓 | 180 | 公共 PreDealDamage 对直接普攻 +12%，不放大技能/物品派生伤害 |
| item_swift_string | 迅弦 | 220 | GAS BaseAttackTime 乘 0.92，公共普攻间隔缩短8%，技能冷却不变 |
| item_stone_guard | 石卫徽记 | 160 | GAS MaxHealth +120，提高上限不补当前生命，不生成盾 |
| item_hunter_scope | 猎手瞄准镜 | 260 | 对稳定定义名称含 elite/siege/boss 的目标伤害 +12%；普通兵/刺客无加成 |

每件最多持有一件（九格含锁定库存），无合成、无随机掉落；允许出售和进入背包，禁止主动丢弃，首次持有者绑定。每名玩家每个整备窗口（以服务器 PhaseGeneration 区分）最多成功购买本批防守装备两件；失败和退款不返还已用购买次数。团队即用商品保持原有规则，不占装备购买配额。

### Non-Goals

回能符、修复透镜、回城信标、商贾印、八件全部完成、远程兵、阶段 Boss、统计、完整换英雄、24机制卡、12波和3/4人容量不在本步。不将测试清场当正常构筑通关证明。

## 2. 当前事实与依据

本节保留开工时基线；实施后的行为与验证见 §3、§9。

- Economy PurchaseItem 在服务器同一经济/库存锁内提交物品，Inventory 精确管理被动句柄；默认物品无领域购买限制。
- 开工时 `CombatItemData.h` 只有获得即用 AcquireEffect；`CombatPlayerController.cpp:681` 仅为即用商品允许领域上下文及完整回执缓存。
- `CombatEconomyComponent.cpp:925` 只投影 FrozenAcquireItems；`DefenseTeamShop.cpp:63` 提供整备阶段门及阶段变化刷新。
- `DefenseDirectorComponent.cpp:137` 冻结 PhaseGeneration/WaveIndex；PlayerRecord 已有连接归属和统一注销/Shutdown。
- `CombatAttackTimingPolicy.cpp` 读取最终 BaseAttackTime；Damage Hook 可检查 DirectSourceType，不需要改 Attack registry。
- MCP 无服务，未开启用户 Editor；使用 UE 原生 API，禁止直接编辑二进制。

## 3. 行为与契约

### 主流程与状态转换

原商店右键 → 原拥有者经济 RPC → 权限/限频/重放/绑定/修订 → 同步商品策略预检 → 唯一库存交易 → 在互斥内记录购买配额 → GAS 装备效果 → 原投影/回执。背包、死亡、出售和 EndPlay 使用既有库存效果撤销/恢复。

### 输入、输出与数据约束

新增默认0不限量的 MaxOwnedQuantity 和库存只读持有限额检查，同时覆盖可信授予/拾取/购买，九格和锁定不绕过，负值/超过99拒绝。新增默认空的可内嵌 `UCombatItemShopPolicy`。其 QueryOffer 只读，OnPurchased 只记录已成功事务的领域配额，不扣钱、生成实例或广播。只允许无配方、非即用的可购买叶子配置策略，策略物品不得作为其他配方组件。经济组件深拷贝带策略的定义/被动，默认旧物品不变；沿用既有 ShopOffers、RunId/条件修订及最多32条完整回执，不新增线格式。

防守策略返回 Director 构造的只读条件，要求当前英雄是登记英雄、同 World、当前 Run、核心有效、存活且整备开放。限持有按真实库存实例检查，不以客户端拥有数为准。配额按玩家记录的 PhaseGeneration 购买窗口和次数保存，死亡/出售/换控制不重置；注销/Shutdown随 PlayerRecord 清理。

### 权威边界与权限

策略仅由服务器经济事务调用；UI只消费 Offer。Damage Runtime 只调整自己的公共 PreDealDamage 事件，不新建伤害、不保存最终属性；参数来自 ModifierData。未知目标不触发猎手，铁弓要求真实 Attack 直接来源且事件来源为装备英雄。

### 失败、取消、过期、死亡、EndPlay 与重复请求

旧局/旧阶段/旧绑定/旧经济库存修订拒绝；同载荷缓存重放返回原句柄、金币和修订，换载荷拒绝。满包/缺钱/不合法定义/配额耗尽/重复持有不扣钱、不记配额。Commit 后立即在锁内记配额，观察者重入不能双购；不创建新异步任务或权威时钟。终局关闭 Offer；Death撤被动、Respawn由Inventory恢复，Actor/World结束清精确句柄与冻结定义。

### 兼容、版本与迁移

默认空策略兼容旧内容；复用现有RPC和ShopOffers结构，Combat Release/Tag/Event/Formula/Economy schema保持原值。新增扩展点及资产稳定ID先登记 ADR-080。仅 Defense 商店增加新分类，旧 Demo 目录与团队六件保持；回滚移除分类和本步代码文件组。

## 4. 实施计划

| 文件/资产 | 变更 | 原因 | blast radius |
| --- | --- | --- | --- |
| Items/CombatItemShopPolicy.*、CombatItemData.*、CombatInventoryComponent.* | 同步只读策略/成功提交通知、默认不限量的九格持有限额，校验组合 | 公共系统缺少普通装备领域购买准入 | 默认空兼容 |
| Economy/CombatEconomyComponent.*、CombatPlayerController.cpp | 冻结策略定义、条件预检/提交配额、完整回执、Offer | 复用既有库存事务和网络身份 | 原普通商品/即用回归 |
| Defense/DefenseEquipment.*、DefenseDirectorComponent.h | 四件效果/购买策略和每玩家窗口账本 | 独立玩法适配 | Defense |
| UI/CombatShopWidget.cpp | 普通领域商品显示服务器条件 | 可见限购原因 | 保持原商店布局 |
| Content/Combat/Demo/Defense/Equipment、DA_DefenseUnifiedShop | 四件Item/四件Modifier/原目录新分类 | 真实可玩内容 | UE保存、冷读、LFS |
| Tests/CombatDefenseEquipment*、NetworkScenario*、Tools | 资产/效果/事务/PIE/网络验证和幂等迁移 | 可重复证据 | 显式测试开关 |
| Spec/台账/ADR/设计/README/生命周期/10-14 | 当前事实和边界 | 文档同步 | 不重写历史 |

## 5. 验收标准（AC）

- [x] AC-01：四件正式资产/价格/说明/稳定ID/原商店分类冷读正确，团队目录保留，迁移重复无漂移。
- [x] AC-02：真实Damage与AttackTiming证明四效果；普通目标/派生/未知身份反例；背包、等待、出售、死亡复活、EndPlay清理正确。
- [x] AC-03：RPC拥有者/限频/完整重放、旧上下文、九格重复、锁定、每窗两件、跨窗重开、失败原子性、提交重入通过。
- [x] AC-04：真实渲染PIE走原商店几何右键购买，库存/金币/效果及限制可回读并截图。
- [x] AC-05：Editor/Server/Client、专项及风险相称Combat分进程回归、资产校验/冷读、Dedicated双客户端购买与复制和Cook通过；明确实际执行边界。

## 6. Definition of Done

- [x] 记录最小 Red 和 Green；无新的权威旁路。
- [x] 公开元数据与中文注释同步，旧默认物品/团队商品回归通过。
- [x] UE资产保存/编译/冷读，LFS与差异范围可核查。
- [x] F2与六层交付门完成；未提交、未推送，待用户验收。

## 7. 测试矩阵与命令

| 层级 | 用例/命令 | 预期证据 | 结果 |
| --- | --- | --- | --- |
| 文档/Gate | preflight/plan/build/delivery、validate_docs、diff | Saved/DEF-014 | preflight/plan/build 0错误；最终文档与delivery报告见 §9 |
| Types/Build | 安装版Editor、源码Server/Client Development | *-build-final.log；editor-ui-fixture-build.log | 三Target成功；仅Editor测试夹具随后增量成功 |
| Automation | 全部已发现非PIE Combat.*；工具单测 | combat-non-pie/index.json；tool-tests.log | 248/248（含装备7项）、59/59工具测试；0失败 |
| PIE | L_Defense原商店装备/核心屏障/援军交互 | shop-scroll-final/index.json；equipment-shop-pie.png | 3/3；实际可视几何右键、扣费/属性/配额/原商品正确 |
| Asset | UE Python迁移/重复/只读冷读，CombatAssetValidation | asset-idempotency.json；equipment-final-cold-read.log；assets-final.json | 重复9资产摘要一致；冷读成功；154扫描0错误0警告 |
| Dedicated/Cook | 显式Equipment smoke，独立Server与双Client；正确平台Cook/Stage/Archive | targets-final/equipment-smoke/report.json；targets-final/package-targets.log | 三端同局、4实物/7步/精确金币/公开属性复制；WindowsServer/WindowsClient双平台Cook通过，807包，0错误 |
| Soak/Perf | 长期经济平衡与四人容量 | 后续Phase4 | 未执行；本步不宣称平衡或容量 |

## 8. 风险、回滚与升级

- 风险：配额提交与Inventory通知重入、阶段/Offer不同Actor复制时序、同款锁定/背包绕过、属性上限换装回血。
- 回滚方式：先撤Defense目录分类，再撤本步策略/适配/测试与资产；旧内容默认空策略，无不可逆迁移。
- 触发升级的条件：新增网络字段/发布版本、改变其他模式商品或扩展其余四件时递增Spec重审。
- 需要人决定的问题：无；按已确定设计交付四件独立切片，其余四件明确保留后续。

## 9. 交付证据

证据均保存在本机 `Saved/DEF-014`，生成日志、截图与归档不进入 Git。初始工作区干净；用户于 2026-10-22 确认本批首四件验收成功并授权本地提交，不推送。

### Red → Green 与集成修正

- `editor-red-build.log` + `equipment-red.log`：先编译并运行正式资产测试，四件资产/ShopPolicy 缺失，实际失败；随后才实施生产代码。
- `equipment-tests-1.log`：真实公共攻击时序抓到迅弦初版资产操作为 AddBase，BAT误加0.92。修正 UE5.8 序列化操作为 MultiplyCompound，资产校验同时断言操作名；`equipment-tests-2.log` 六项通过。随后新增的第七项提交观察者生命周期测试也通过完整248回归。
- F2 检查库存提交广播可以结束英雄或推进阶段：成功回调使用稳定付款人PC及预检AcceptedOffer记录旧窗口，`CommitObserverLifecycle` 证明旧购买不占新窗口、英雄EndPlay仍记一次；未增加第二库存提交或伤害入口。
- `equipment-pie-1.log`：手工发送金币按钮 Down/Up 不能打开商店的夹具失败；改用既有公开 ToggleShop，正式购买仍用真实条目几何右键。`equipment-pie-2.log` 与最终装备 PIE 通过。
- `equipment-final-ui.log` 和 `reinforcement-isolated.log` 保留旧援军超时。新增分类后援军位于滚动区外，日志没有购买事务；夹具现在沿真实 Slate 父路径滚动目标入可视区，再等待一帧使用新几何右键。`shop-scroll-final.log` 三用例全过，生产商店布局、购买入口和团队六件资产未改。
- F2 同步恢复原即用商品的“已用”角标，仅普通装备显示“限额”；原团队商品渲染回归验证通过。

### 实际命令与结果

- Gate：`python -B Tools/task_gate.py --mode preflight|plan|build --spec Doc/CombatSystem/Specs/DEF-014-defense-equipment-foundation.spec.md --kind feature --report <Saved/DEF-014/对应报告.json>`；0.1和0.2均事前通过，0.2报告为 `preflight-v2.json`、`plan-v2.json`、`build-gate-v2.json`。
- 构建：安装版 `<UE_INSTALLED>/Engine/Build/BatchFiles/Build.bat ue_gasEditor Win64 Development <PROJECT> -WaitMutex -NoHotReloadFromIDE`；源码同命令分别指定 `ue_gasServer`/`ue_gasClient`。最终三份 `*-build-final.log` 成功；滚动夹具仅在 WITH_EDITOR 下，随后 Editor 增量见 `editor-ui-fixture-build.log`。
- 完整非PIE：静态发现262个Combat测试，按 `test-manifest.json` 保留248非PIE与14PIE完整名单；Cmd独立进程 `-ExecCmds="Automation RunTests <248个精确名称以+连接>;Quit" -TestExit="Automation Test Queue Empty" -ReportExportPath=<Saved/DEF-014/combat-non-pie>`，248/248、0未运行、0失败。246无警告，2项调试控制台负例按预期输出用法/拒绝警告；不隐瞒发现数，也不将PIE混入纯World回归。
- 渲染PIE：真实 `UnrealEditor.exe <PROJECT> -unattended -nop4 -nosplash -NoSound -windowed -ResX=1600 -ResY=900 -d3d11 -ModelContextProtocolPort=8121 -ExecCmds="Automation RunTests ^Combat.Defense.PIE.EquipmentShop$+^Combat.Defense.PIE.TeamShopButtons$+^Combat.Defense.PIE.ReinforcementShop$;Quit" -TestExit="Automation Test Queue Empty" -ReportExportPath=<Saved/DEF-014/shop-scroll-final>`，3/3通过。保留引擎导航teardown警告；无Automation错误。装备截图已人工回看：防守分类四件、两实物、金币910、生命900/1020；两件扣340，第三件被拒。
- 资产：UE Cmd `-run=pythonscript -script=<REPO>/Tools/setup_defense_equipment.py` 创建/保存；重复运行 `equipment-repeat.log` 和 `asset-idempotency.json` 证明8新资产+原目录摘要一致。加 `-DefenseEquipmentReadOnly` 冷回读，最终 `equipment-final-cold-read.log` 成功。`-run=CombatAssetValidation -Report=<Saved/DEF-014/assets-final.json>` 扫描154项、0错误0警告。未产生必要蓝图行为修改，撤回本任务初次无差别编译保存造成的三份蓝图二进制噪声；原GameMode目录引用冷读、双平台Cook与独立启动均正常。
- 工具：`python -B -m unittest discover -s Tools/Tests -v` 59/59，见 `tool-tests.log`；修改的PowerShell脚本通过原生AST解析。
- 原Editor Dedicated原型：`Tools/run_defense_smoke.ps1 -Players 2 -Equipment -EvidenceFolder DEF-014 -Port 7914 -TimeoutSeconds 280`，`network-equipment-victory-2` 三端通过；该轮早于最终生命周期修正，仅作为分层证据。
- **最终独立Target**：`Tools/package_defense_targets.ps1 -ArchiveRoot <REPO>/Saved/DEF-014/targets-final/Archive -SkipBuild`，WindowsServer与WindowsClient Cook/Stage/Pak/Archive全部成功，807包、0错误；唯一Cook警告是已有MCP插件的EULA提示。按 `targets-final/equipment-smoke/processes.json` 的真实命令分别运行归档 Server 与两个Client（`-DefenseSmoke -DefenseEquipmentSmoke -DefenseSmokePlayers=2 -DefenseSeed=831907 -NullRHI -NoSound`，Server端口7916，Client连接并带Seat1/2）；本机编排命令为 `python -B Saved/DEF-014/verify_targets_equipment.py`。三端同局 `9D41AE4A4FD2647A42A253858FA4AC2D`，NetMode1/3/3、每客户端七步购买/重放/重复持有拒绝/配额拒绝/战斗拒绝/跨窗购买，最后各四实物、精确金币及两英雄三项公开ASC属性复制。`report.json` 证明实际归档exe摘要与最终源码Target一致，`cleanup.json` 证明本轮全部进程退出。显式测试赠金/加速/清场仅编排协议验证，不宣称正常构筑通关。
- 卫生：9份变更资产均匹配LFS（`lfs-check.json`），只交付8个新DataAsset+原商店目录；无生成目录、无用户既有改动混入。最终文档106篇/571本地链接/0错误（`docs-final.json`），交付门0错误（`delivery.json`），`git diff --check`通过；执行命令分别是 `python -B Tools/validate_docs.py --report Saved/DEF-014/docs-final.json`、`python -B Tools/task_gate.py --mode delivery --spec Doc/CombatSystem/Specs/DEF-014-defense-equipment-foundation.spec.md --kind feature --report Saved/DEF-014/delivery.json`，以及 `git diff --check`。

### Push-Ready 六层

| 层 | 结论 | 证据 |
| --- | --- | --- |
| Tests | PASS | 248非PIE含7装备专项、3渲染PIE、59工具测试、Red/Green与失败记录 |
| Types/Build | PASS | Editor/Server/Client Development及测试夹具增量、双平台Cook/Stage |
| No Regression | PASS | 所有非PIECombat，原团队屏障/援军几何购买，154资产与原目录引用 |
| Adversarial | PASS | 背包/锁定/授予上限、缺钱/满包/外来Owner、重放/变载荷/旧阶段/终局、提交重入/阶段变化/英雄EndPlay；独立双客户端 |
| DDD/Constraints | PASS | GAS/公共Hook唯一结算、原库存/经济锁与回执、无新增时钟/RPC/schema、中文Details和生命周期说明 |
| Decisions | PASS | ADR-080、台账、设计、README、Item/Economy及生命周期同步；限四件、其余机制与长测明确延期 |

## 10. 变更记录

| 版本 | 日期 | 修改 | 原因 |
| --- | --- | --- | --- |
| 0.1 | 2026-10-21 | 首批四件装备、领域购买策略与验证计划 | 用户授权继续下一步 |
| 0.2 | 2026-10-21 | 增补通用持有限额保护授予/拾取/购买和策略组件禁用 | 源码审查确认仅限购不能兑现设计限持有；尚未修改生产代码 |

## 11. 路由、自评与 Reflect

主Skill `combat-feature-development`，路由high；本次不修改Skill。

| 维度 | 权重 | 0–5分 | 依据 |
| --- | ---: | ---: | --- |
| 需求 | 20% | 4.5 | 首批四件完整可玩；没有将整个八件或Phase3写完成 |
| 权限 | 20% | 5.0 | 服务器策略与原RPC/事务，默认兼容，无提交/推送 |
| 实现 | 20% | 4.5 | 公共GAS/Hook、真实库存、冻结定义、窗口账本和精确清理 |
| 验证 | 20% | 4.5 | 248+3+59、三Target、资产、双平台Cook与独立三端；未宣称全PIE/长期平衡 |
| 文档 | 10% | 4.5 | Spec/ADR/台账及相关专题同步，失败与实际边界保留 |
| 卫生 | 10% | 5.0 | 原生资产/LFS，撤回无关BP噪声，不提交生成物 |

加权4.65/5。Reflect：真实AttackTiming比只看属性导出更能发现GE操作错误；增加目录内容后，渲染夹具必须先滚动目标入可视区；库存提交广播后的领域记账必须绑定冻结上下文和稳定付款人。这些已固化为专项断言/夹具，不因单次任务修改全局Skill。
