# DEF-016 商贾印

> Spec 版本：`0.2`
> 状态：`已验收`
> Owner：Codex
> 创建日期：2026-10-22
> 关联进度台账：`00-01-Progress-Tracker.md` §12.14
> 风险等级：`L1`

## 0. Intake 与 Gate 记录

- 用户请求：DEF-015 已完成实现并待验收后“继续做吧”；继续守卫雅典娜防守装备，处理剩余装备中的商贾印。
- 附件解释：无附件；AGENTS.md/agent.md 是工程约束，设计文档 §6.2 与现有商店/重抽行为是需求依据。
- 已读取入口：agent.md、README、当前进度台账、00-03测试计划、00-04 ADR/Gap、00-05流程、10-14物品、10-15经济、20-03公开扩展、20-05防守设计、90-16生命周期、DEF-015 Spec、购买/出售/重抽/商店 UI/Modifier/网络测试实现。
- 主 Skill：`Skills/combat-feature-development/SKILL.md`
- 备选 Skill 与排除理由：本步是普通装备与经济报价扩展，不新增玩家可施放技能，combat-skill-development不作为主 Skill。
- 路由置信度：`high`
- F0 结论：`GO`
- F0 依据：商贾印有明确数值和稳定物品身份；现有购买、获得即用、出售、重抽与 owner-only 商店视图已有权威入口，可以在同一价格策略上复用。DEF-015 的未提交变更属于前一切片，作为当前工作区前置状态保留，不改变本 Spec 的范围。
- F1 结论：`APPROVED`
- F1 审查人：Codex（既有 L1 继续开发授权内本地审查）
- F1 审查版本：`0.2`
- F1 计划审查证据：0.2 plan-02.json 通过；复核独立快照 schema1 不改变冻结经济布局，Modifier 委托唯一持有/换控解绑/EndPlay 清理，刷新受经济事务保护，客户端 AssetManager 只读目录，重抽去重比较价格和次数；新增反例先真实 Red。复核以 shop_discount_fraction 为唯一公共参数、由服务器从当前有效 Modifier 读取并在整笔 Plan 上一次 floor；购买/获得即用/重抽共用报价，owner-only 视图只做表现，PurchasePaidGold 保留折后实付；20% 封顶、背包/死亡/EndPlay 生命周期、重放与旧空参数兼容均有反例；不加入节俭卡资产、不改出售窗口外公式、不增加客户端输入。
- Build 解锁：0.2 F1 事前审查通过，build-gate-02.json通过后才编写新反例和修复；0.1历史证据保留。
- F1 重审条件：新增回城入口、改变团队商品领域条件、扩大到未落地的节俭强化卡、增加新的 RPC/客户端价格输入或改变出售公式时递增版本并重新审查。
- F2 结论：`PASS`
- F2 依据：已按权限、生命周期、取整、客户端只读目录与重放逐项攻击审查；发现均修复，最终独立三端RunId=A42EEEBA455EBEE6510C42857659AF80通过，见§9。
- Push-Ready 结论：`READY`
- Push-Ready 边界：实现、专项/回归/资产/PIE、三Target、双平台Cook/Stage/Archive和独立三端全部通过；用户于2026-10-23确认验收完成并授权本地提交，不推送。

## 1. 目标与范围

### 目标

在防守装备目录增加商贾印：240 金币、普通装备、每件限持一件、沿用每个整备阶段最多购买两件、绑定当前英雄、不可丢弃。装备生效后，当前英雄的所有商店购买报价和防守强化重抽费用按有效折扣计算：商贾印提供 5%，与同一价格参数的其他经济效果相加，最终折扣上限 20%，每次费用向下取整。

### 范围

价格折扣只在服务器重算并提交一次，覆盖普通装备配方补购、获得即用的团队商品和防守重抽；UI 的商品徽标、提示和重抽文字读取同一 owner-only 报价快照。商贾印本身在购买前不生效，装备/出售/死亡/复活/控制切换后随现有 Modifier 生命周期即时反映下一次报价。购买实例记录实际支付金额，保留现有全额退款窗口；窗口外出售继续使用既有完整目录价与出售比例，避免把出售当成新的购买折扣入口。

价格层使用公共 `shop_discount_fraction` 运行时参数，支持未来的 R1“节俭”或其他经济效果通过同一参数叠加；本切片不创建尚未落地的节俭强化卡资产，不把它伪装成已完成内容。总折扣固定封顶 20%，基础价格为正时折后价格仍为非负整数。

### Non-Goals

回城信标、回城引导/打断/复活点入口、节俭卡的候选池资产、Boss/远程兵/统计、长期平衡、3/4 人容量和新的客户端请求字段；不改原配方、出售比例、团队商品限购/受益人领域规则，也不让客户端提交价格或折扣。

## 2. 当前事实与依据

`UCombatShopData::BuildPurchasePlan` 已在服务器按当前未锁定库存计算缺失叶子总价；`UCombatEconomyComponent::PurchaseItem` 与 `PurchaseAcquireItem` 分别提交库存物品和获得即用商品，已有原子金币扣除、实际支付记录及回执。`SellInventoryItem` 在全额退款窗口内读取 `PurchasePaidGold`，窗口外按目录完整价格与冻结出售比例结算。

防守重抽在 `UDefenseDirectorComponent::ProcessChoice` 中有唯一 `TrySpendGold` 入口，视图由 `BuildChoiceView` 生成并复制给所属玩家；原商店 UI 当前读取目录基准价。DEF-015 已给 `UCombatModifierComponent` 增加当前有效 Runtime 参数只读汇总，可复用装备生命周期、背包禁用、死亡暂停和 EndPlay 清理。

现有内容目录仍有六件普通防守装备和六件团队商品；商贾印是第七件普通防守装备。设计文档已规定 `item_merchants_seal` 240 金币以及“商店与重抽价格 -5%；与节俭相加但总折扣上限 20%”。

## 3. 行为与契约

### 主流程与状态转换

原商店右键/团队领取请求进入原 Economy 事务；服务器先验证当前英雄、领域上下文、库存/限购和配方，再根据当前有效 `shop_discount_fraction` 对 `FCombatPurchasePlan::TotalCost` 做一次 floor 折算，随后以折后金额写入金币账本和 `PurchasePaidGold`。商贾印装备成功后刷新 owner-only 价格快照；卖出、移入背包、等待重新装备、死亡、复活或 EndPlay 由原 Inventory/Modifier 生命周期决定下一次报价是否包含效果。

防守重抽先完成候选抽取和当前请求/修订校验，再使用 `floor(BaseRerollPrice * (1 - Discount))` 调用唯一 `TrySpendGold`；`FDefenseChoiceView::RerollCost` 与 `bCanReroll` 使用相同报价。失败不消耗重抽次数、不写候选、不写金币。

### 输入、输出与数据约束

新增 `CombatItem:item_merchants_seal` 和 `CombatModifier:equipment_merchants_seal`。Modifier 使用公共 `UCombatModifierRuntime`，永久、随死亡清理并在装备有效时只提供 `shop_discount_fraction=0.05`；物品为可购买、可出售、不可丢弃、BoundUnit、MaxOwnedQuantity=1、无配方、普通 `DefenseEquipmentShopPolicy`。

新增独立 `FCombatShopPriceView`（自身 schema 1）仅复制给拥有者，保存 0..2000 的折扣万分比；不继续扩展已冻结的 FCombatEconomyView 布局。它不是客户端输入；经济初始化/主控绑定/Modifier 集合与死亡暂停恢复时重新构建，EndPlay 解绑观察者。客户端按原 ShopDefinitionId 加载只读目录，用拥有者库存构造配方补购报价。`UCombatEconomyComponent` 提供统一的折扣与当前物品报价方法，UI、测试和防守导演不得各自复制公式。参数先相加并封顶20%，再按最近万分比量化；报价使用整数商/余数对整笔总价一次向下取整，不能产生负数或溢出。例如 60/100 在5%时为57/95，在15%时为51/85，cap20%时为48/80。

### 权威边界与权限

价格、折扣来源、当前有效装备、配方消费和重抽费用全部由服务器决定；客户端只携带物品 ID、库存/经济修订、领域上下文和选择意图。服务器不接受客户端价格/折扣字段，不根据客户端徽标或文本决定交易。owner-only 价格快照仅用于表现和下一次本地重建，服务器购买/重抽仍重新读取活动 Modifier。

### 失败、取消、过期、死亡、EndPlay 与重复请求

任何状态、库存、金额、领域上下文或候选修订失败都保持金币、库存和重抽次数不变，按原机制保留拒绝回执。相同请求重放沿用原回执；同一 RequestId 换商品、动作或修订仍拒绝。价格/折扣不是客户端请求字段，服务器状态改变也不会使相同载荷重放再次付款。商贾印进入背包或等待重新装备时不计入有效折扣，死亡按 Modifier 现有暂停/清理语义处理，复活/重新装备后下一次报价恢复；出售/清空库存/EndPlay 后价格快照回到实际有效参数。

### 兼容、版本与迁移

空 `shop_discount_fraction` 保持 0 折扣，旧资产、旧购买/出售/重抽 RPC 和旧物品视图继续工作；报价以独立 owner-only 派生快照 schema1 扩展，不修改 FCombatEconomyView/Release/Formula/Tag/Event 版本，不增加客户端提交字段；本轮三端一起重构建，不承诺新旧可执行文件混连。商店目录幂等追加第七件，旧六件和团队六件保持不变。公共参数键和 20% 上限作为经济报价契约 1 写入文档与测试。

## 4. 实施计划

| 文件/资产 | 变更 | 原因 | blast radius |
| --- | --- | --- | --- |
| Economy/CombatEconomyTypes.*、CombatEconomyComponent.* | 独立 owner-only 折扣快照及绑定/解绑 Modifier 观察者、统一 floor 报价、购买/获得即用接线 | 服务端与 UI 共用一套价格 | Economy/Shop 全部购买事务 |
| Defense/DefenseChoices.*、DefenseChoiceComponent.* | 重抽费用复用经济报价，视图比较覆盖 RerollCost；同步原已存在 Monitor 发布 | 防止重抽显示和结算分叉 | 防守选择/重放 |
| UI/CombatShopWidget.* | 商品徽标/提示读取当前 owner 报价 | 玩家能看到实际可支付金额 | 原商店显示 |
| Defense/Equipment/DA_item_merchants_seal、DA_equipment_merchants_seal、原 UnifiedShop | 第七件正式装备和参数 | 内容落地、幂等迁移 | 防守商店/库存 |
| Tests/CombatDefenseMerchantSeal*、Economy/Defense choice 回归 | 折扣 floor/叠加/封顶、购买/团队商品/重抽/退款/装备生命周期/UI | 覆盖权威与边界 | 自动化/PIE/网络 |
| Tools/setup_defense_merchant_seal.py、smoke/target evidence | 幂等创建、冷读和独立三端可复验 | 资产与交付证据 | 仅显式调用 |
| Doc/CombatSystem/10-15、20-03、20-05、90-16、README、台账/ADR/Spec | 当前报价契约与延期项同步 | 防止把节俭/回城误标完成 | 文档 |

## 5. 验收标准（AC）

- [x] AC-01：`item_merchants_seal` 资产与 `equipment_merchants_seal` 参数正确，价格 240、限持一件、BoundUnit、不可丢弃、普通装备策略；目录为七件普通装备并保留六件团队商品，迁移可重复且冷读无漂移。
- [x] AC-02：无折扣、5%、10%+5%、超过上限四组输入在商品购买和重抽中均按一次 floor 结算；60/100 重抽分别得到 57/95，20% 上限得到 48/80；客户端不能伪造价格，金额不足/陈旧/重放无副作用。
- [x] AC-03：普通装备、获得即用团队商品和配方购买使用折后实际支付；购买实例保存折后 `PurchasePaidGold`，退款窗口返还实付金额；出售窗口外仍按现有目录价与比例；装备进入背包/死亡/EndPlay 后折扣快照正确恢复或清零。
- [x] AC-04：原商店徽标、提示和防守重抽按钮显示与服务器相同的折后报价；真实 PIE 买入商贾印后可观察下一笔价格变化，重抽文本同步。
- [x] AC-05：Editor/Server/Client 构建、直接相关自动化、全非 PIE Combat 回归、资产验证/冷读、真实商店 PIE、独立 Dedicated Server+双 Client 和正式 Cook/Stage 通过；未执行项按实际环境记录。

## 6. Definition of Done

- [x] 如实保留0.1实际先写实现后运行专项的流程偏差，不补写历史Red；0.2新增客户端目录/报价投影生命周期/重抽视图回归在修复前实际运行，失败后才修对应生产路径。
- [x] 中文注释/Details、经济/物品/防守扩展指南和生命周期说明同步。
- [x] 资产保存/冷读、编译、LFS 和幂等迁移通过。
- [x] F2 与 Push-Ready 六层、文档/交付 Gate 通过；不自动提交或推送，等待用户验收。

## 7. 测试矩阵与命令

| 层级 | 用例/命令 | 预期证据 | 结果 |
| --- | --- | --- | --- |
| Gate/Docs | `task_gate.py --mode preflight|plan|build|delivery`、`validate_docs.py`、`git diff --check` | `Saved/DEF-016/*.json` | preflight/plan-02/build-gate-02及文档检查通过；delivery见delivery.json |
| Build | Editor/Server/Client Development | `Saved/DEF-016/*build*.log` | 最终三Target通过；editor-delivery-final.log、ue_gasServer-delivery.log、ue_gasClient-delivery.log |
| Automation | `Combat.Defense.MerchantSeal.*`、全非 PIE Combat | `Saved/DEF-016/green-02`、`combat-non-pie-delivery` | 9专项通过，最终264非PIE通过（262成功+2预期调试警告） |
| PIE | 原商店真实徽标/提示、购买商贾印后报价与重抽同步 | `Saved/DEF-016/shops-pie-delivery`、两张截图 | 5真实渲染PIE通过，保留10条既有导航启动警告 |
| Assets | UE 原生迁移/重复/只读冷读、`CombatAssetValidation` | `Saved/DEF-016/assets-final.json`、`asset-idempotency.json`、`asset-contract-cold-read.log` | 160定义0错误/0警告；重复迁移/冷读摘要一致，旧两迁移器兼容七件 |
| Network/Cook | 显式 MerchantSeal smoke，双客户端角色/金币/折扣/重抽一致；WindowsServer/WindowsClient Cook/Stage/Archive | `Saved/DEF-016/targets-final/*` | 813包双平台Cook/Stage/Archive与最终独立三端通过，摘要匹配 |
| Soak/Perf | 长期经济平衡/4 人容量 | 后续切片 | 未执行：不属于本切片 |

## 8. 风险、回滚与升级

- 风险：UI读基准价而服务器读折后价；购买与获得即用其中一路遗漏；配方总价逐叶折算造成舍入套利；背包/死亡/EndPlay后旧快照残留；重抽视图与扣款不一致。
- 回滚方式：移除第七件资产和目录引用，撤销统一折扣接线/owner-only派生字段/重抽调用；空参数保留旧行为，前六件装备与团队商品不变。
- 触发升级的条件：真正加入节俭卡资产、修改出售公式、增加客户端价格输入、改冻结的网络schema或新RPC、加入回城入口时递增 Spec 版本并重新 F1。
- 需要人决定的问题：无；节俭卡资产和回城信标另立切片。

## 9. 交付证据

实际最终证据以Saved/DEF-016为根；源码/资产、原生Editor回读和真实进程报告相互核对。UE MCP端点不可用，降级为UE原生Python、Automation、渲染PIE和Cook/Stage；未将降级称为MCP成功。

### 实际命令与复验入口

- preflight.json、plan-02.json、build-gate-02.json均通过；0.2先计划重审后修改，delivery.json为最终机器交付门。
- Editor使用本机安装版UE5.8 Build.bat构建ue_gasEditor Win64 Development；Server/Client使用已配置源码引擎。最终Editor/Server/Client成功日志分别为editor-delivery-final.log、ue_gasServer-delivery.log、ue_gasClient-delivery.log；Target receipt分别为Editor/Server/Client，日志均Result: Succeeded。安装版不支持独立Server是环境限制，不作为最终Target证据。
- `Automation RunTests Combat.Defense.MerchantSeal.`：green-02及最终merchant-tests-delivery九项全部成功；覆盖正式资产、5%/15%/20%、整笔配方20→19、int64上界、退款190/窗口外100、即用商品180→171、真实装备/死亡/复活/换控/EndPlay、重抽不足/陈旧/同号换载荷和原回执重放。
- `combat-non-pie-delivery/index.json`：264/264通过、0失败/0未运行。262成功，2个旧Debug命令拒绝边界按预期产生警告；测试完整名单见test-manifest-final.json，未减少此前255基线。
- `shops-pie-delivery/index.json`：5/5通过，包含EquipmentShop、ReinforcementShop、TeamShopButtons、UtilityEquipment、MerchantSeal；各两条旧RecastNavMesh启动/退出警告，不能称零警告。商贾印用原商店实际几何右键连续买入两实物，Tooltip/Quote190、余额820；点原Start/Reroll按钮后扣57至763并显示第二次95。截图merchant-seal-pie.png、merchant-reroll-pie.png已目视核对。
- 原生`-run=pythonscript -script=Tools/setup_defense_merchant_seal.py`重复及`-DefenseMerchantSealReadOnly`新进程冷读；旧equipment/utility只读脚本均通过。asset-idempotency.json保存Defense资产前后SHA256，重复执行无变化；严格冷读还核对绑定、购买/出售、无配方、精确被动及死亡/Break/驱散配置。
- `-run=CombatAssetValidation -Report=.../assets-final.json`：160定义，0错误/0警告；LFS两新增资产filter=lfs，未用文本改写二进制。
- `python -B -m unittest discover -s Tools/Tests -v`：59通过；validate_docs.py与git diff --check通过，最终交付前复验。
- `Tools/run_defense_smoke.ps1 -Players 2 -MerchantSeal -EvidenceFolder DEF-016-Network02 -Port 7921 -TimeoutSeconds 120`：安装版Editor的-server/-game三进程通过，RunId=1C3ECFE3438A42DE9640FA9CB8304BE8；这是阶段网络证据，不代替独立Target。
- WindowsServer/WindowsClient Cook/Stage/Archive成功：813包（805 Cooked、8按平台跳过）；首次日志package-targets-initial.log与最终归档更新package-targets.log均ExitCode=0。最终重新三Target构建后，原package_defense_targets.ps1 -SkipBuild更新双平台容器和归档，再从Archive内真实ue_gasServer.exe与ue_gasClient.exe运行。

最终独立网络：targets-final/merchant-seal-smoke-final/report.json，RunId=A42EEEBA455EBEE6510C42857659AF80，22.45秒。Server NetMode1与两Client NetMode3各唯一完成；每名客户端5个请求及5次同载荷重发，商贾印240、透镜190、两次重抽57/95，合计582、余额668；拥有者库存两实物、当前折扣5%，客户端Runtime数0。原生客户端目录解析报价190已断言；三端RunId完全一致。server SHA256=0c2f87b7bbac29144b46b430bc510d8d34aefb0684ef549536d90c17cb850a33，client=2a2d724c5cad56678f9d2e85ea4c705c67fc2e8999c201e64fa97ef3fac539c2，均与本轮Binaries构建一致，cleanup.json确认三端全退出。validation-summary.json汇总各层实证。

### 真实失败与修复

0.1先写生产实现再运行3专项，不符合Red在先的约束。本记录承认时序偏差，不把后来补测写成此前Red。实际曾用浮点floor把60/100/200在5%算成56/94/189，已改万分比整数商/余数；旧失败报告被重跑覆盖，保留这一事实，最终Green能复验但不冒充旧Red文件。

0.2在生产修复前运行red-02/index.json，旧3专项成功，新3项失败：客户端无ShopData无法报价；背包/重新装备/外部效果的复制字段滞后；重抽价格与次数变化被去重忽略。随后删除对冻结FCombatEconomyView的临时加字段，改独立schema1价格快照，按AssetManager解析客户端目录，绑定当前Modifier集合并在换控/EndPlay解绑，补去重字段。green-02九项通过。对抗检查再运行red-break/index.json，确实复现只改变ASC Break标签时价格仍0.10；修复原RefreshShopOffers入口的重建，最终264回归中的同一用例已通过，无新增计时器。

首次Editor网络探针失败证据保留在network-merchant-seal-victory-2：服务器在买入前断言折扣、客户端在经济/库存复制到达前提交下一步造成Economy.Stale。现改为等待真实购买/重抽完成再核验，按回执修订与金币分别等待跨组件复制，不放宽生产请求校验。测试夹具不再const_cast冻结RunData，而在Initialize之前构造合法池及整备时长。首轮测试编译曾引用不存在的CombatItemInstance.h，已使用实际声明所在CombatItemSubsystem.h并构建通过。

源码Editor构建会展开大量未就绪引擎动作，已终止该备用尝试；Editor最终使用安装版。源码UBT的Git工作集查询有超时，末轮仅对子进程设置UnrealBuildTool_SourceFileWorkingSet__Provider=None避免等待，没有修改用户或引擎全局配置。网络探针完成字段使用实际5个请求/5次同载荷重发，未用旧固定RPCSteps计数作为验收证据。

### F2 对抗审查

按Spec重新逐项对照权威入口与失败状态：购买两分支只在整笔费用上折算一次；实际支付进入原实例退款链；价格投影无客户端写入；旧冻结经济聚合及Release/Formula/Tag/Event版本无差异；回执等待不绕过所有权/绑定/修订校验；委托不持有旧英雄或新增周期账本。以上生命周期/客户端目录/去重/Break/探针和夹具发现已修复并有回归；最终独立Target三端亦通过，F2=PASS，没有未关闭的本切片高风险发现。

### Push-Ready 六层

| 层 | 结论 | 证据 |
| --- | --- | --- |
| Tests | 通过 | 9专项/264非PIE/5渲染PIE/59工具，资产冷读与160定义 |
| Types/Build | 通过 | 最终Editor/Server/Client均Succeeded，独立Target receipt与归档exe摘要匹配 |
| No Regression | 通过 | 全非PIE与前四个商店PIE，旧迁移脚本及冻结发布测试 |
| Adversarial | 通过 | 上述真实失败均修复，归档独立三端同RunId与逐角色断言通过 |
| DDD/Constraints | 通过 | ADR-082及物品/经济/扩展/设计/生命周期/README同步 |
| Decisions | 通过 | 独立快照schema1；节俭卡/回城/长期平衡明确后续，保留GAP-030 |

### 未执行与剩余风险

长期平衡、3/4人容量和长时间Soak未执行，不属于本切片；节俭卡资产、回城信标、远程敌人、Boss阶段和统计未实现。GAP-030动态GameplayEffect定义复制诊断仍存在，拥有者界面按独立投影验证；Editor插件启动Python报错与导航警告保留，不伪称完整日志零错误。当前仅限同版本三端，用户尚未验收，未提交/推送。

## 10. 变更记录

| 版本 | 日期 | 修改 | 原因 |
| --- | --- | --- | --- |
| 0.2 | 2026-10-23 | 独立价格快照schema1、客户端目录解析与Modifier生命周期观察，补强真实复制/退款/团队商品/配方测试 | 网络与静态审查发现客户端空ShopData、快照滞后及重抽视图去重遗漏；不改变已批准折扣数值和交易权限 |
| 0.1 | 2026-10-22 | 创建商贾印切片，冻结统一商店/重抽报价与节俭参数契约 | DEF-015 完成后继续剩余装备，回城另立切片 |

## 11. 路由、自评与 Reflect

主 Skill `combat-feature-development`，high；用户于2026-10-23确认验收完成并授权本地提交，不推送。

| 维度 | 权重 | 分数 | 证据/扣分 |
| --- | ---: | ---: | --- |
| 需求与AC | 20% | 5.0 | 七件目录、统一报价、完整交易与可玩入口，AC全部有实证 |
| 架构与权限 | 20% | 5.0 | 原服务器事务/原RPC；独立owner-only投影，冻结布局无变化 |
| 实现与数据 | 20% | 4.5 | 九专项、重复迁移/严格冷读和原商店；经济参数目前仅在本切片内容落地 |
| 验证证据 | 20% | 4.5 | 三Target/264非PIE/5渲染PIE/59工具/160定义/813包及真实双Client；长期平衡/容量不在本轮 |
| 文档与可观测性 | 10% | 4.5 | ADR/专题/Spec/台账与逐角色回执，既有GAP-030和导航/插件诊断保留 |
| 交付卫生 | 10% | 3.5 | 0.1曾先实现后测，0.2真实Red/Green修正并保留失败，不以补写历史消除偏差 |

加权自评4.6/5，状态READY_FOR_REVIEW；自评不代替用户验收，未提交/推送。

Reflect：仅检查权威Getter会漏掉客户端目录与跨组件复制到达顺序，这是本切片实现/夹具问题。以独立价格投影、实际owner字段反例、真实原UI和逐端归档报告固化；收益由red-02/red-break与最终Green、同RunId/摘要验证证明。没有跨两个独立任务的重复流程证据，本轮不修改通用Skill或评分标准；后续仍先写真实Red。需要用户决定的仅本轮验收，回城与节俭卡另立切片。

### 0.2 计划重审记录

0.1已实现并有3专项/258非PIE/1PIE/160资产和三Target阶段证据，但尚未交付。实际未在首次生产修改前取得Red，这一时序偏差保留，不以补写记录追认。0.2修改前重新计划审查：独立快照避免改已冻结经济布局；订阅仅当前英雄的Modifier委托，无新计时器，换主控/EndPlay解绑；客户端不持有运行时Modifier，按AssetManager目录与服务器价格快照展示；配方按缺失叶子总额一次floor；原重抽投影去重补价格/次数字段。网络探针修复异步等待、初始状态与每个真实回执独立核验。

0.2 F1（2026-10-23，Codex）：APPROVED。逐项检查 AC、权限、整笔整数报价、复制到达顺序、重入、死亡保留效果、换控与销毁、退款/团队/配方边界及目标打包矩阵；该增量无新客户端输入、无新权威账本和新调度器。现有L1继续授权覆盖；0.1先实现后测的偏差仍如实保留。

0.2 对抗检查：额外确认仅改变 ASC Break 标签时不一定发生 Modifier 集合变更；在既有 ProjectionLifecycle 增加原 Monitor 刷新入口反例，先运行再修复，无新 Timer/观察对象。

- 用户验收状态：`已验收`（2026-10-23；用户确认“验收完成，提交吧”）。
