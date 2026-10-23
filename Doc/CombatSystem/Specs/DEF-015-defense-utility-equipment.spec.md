# DEF-015 回能符与修复透镜

> Spec 版本：`0.1`
> 状态：`已验收`
> Owner：Codex
> 创建日期：2026-10-22
> 关联进度台账：`00-01-Progress-Tracker.md` §12.13
> 风险等级：`L1`

## 0. Intake 与 Gate 记录

- 用户请求：DEF-014验收提交63e45a7后“继续进行下一步吧”；继续防守装备，先闭合回能符与修复透镜。
- 附件解释：无附件；AGENTS.md/agent.md为工程约束，设计§6.2为已确定需求。
- 已读取入口：agent.md、README、当前台账、00-03测试计划、00-04 ADR/Gap、00-05流程、10-01架构、10-14物品、10-15经济、10-12 HUD、20-02技能检查表、20-03公开扩展、20-05防守设计、90-16生命周期、Skills/combat-task-router/SKILL.md、Skills/combat-feature-development/SKILL.md；ASC冷却提交、Modifier排序/Hook、真实装备事务、团队修复/事件及现有测试。
- 主 Skill：`Skills/combat-feature-development/SKILL.md`
- 备选 Skill 与排除理由：本步是普通装备及内核兼容扩展，不新增玩家可施放技能，combat-skill-development不作为主Skill。
- 路由置信度：`high`
- F0 结论：`GO`
- F0 依据：当前工作区干净；设计有明确数值，复用原Inventory/ShopPolicy与公共Heal/ASC。回城入口、报价折扣各有独立依赖，另作后续切片。
- F1 结论：`APPROVED`
- F1 审查人：Codex（既有L1继续开发授权内本地审查）
- F1 审查版本：`0.1`
- F1 计划审查证据：preflight/plan均0错误；复核旧空策略/物品主动保留原路径、普通英雄唯一提交点与40%内容上限、只读ActiveGE条件查询、两个物品共用原事务及配额、明确本人/队友/自动修复归属、已提交Heal与终结回调、全非PIE/三Target/真实PIE/独立双客户端和可逆资产迁移。批准0.1两件切片，不包含回城/折扣或网络版本变更。
- Build 解锁：`已解锁`；见Saved/DEF-015/build-gate.json。
- F1 重审条件：扩大到回城、商贾印、增加网络字段或改变既有公共公式/schema时递增版本重审。
- F2 结论：`PASS`
- F2 依据：按独立攻击清单重新读取原始diff与原始日志，检查空策略/物品边界、先预检再冻结、本人/队友/取消/自动归属、出售/死亡/重放、同步Shutdown及真实三端。修复已提交Heal后的事件/回执复活问题；网络夹具只读复制身份、回传领域修订、经过原LOS确认合法瞄准，不放宽生产规则。
- Push-Ready 结论：`READY`
- Push-Ready 边界：本地交付就绪；用户于2026-10-23确认验收完成并授权本地提交，不推送。
- 验证：三Target、255/255非PIE（含7新专项）、4/4渲染PIE、59工具测试、158资产0错误0警告、幂等迁移/冷读、811包WindowsServer/WindowsClient Cook/Stage/Archive与独立双客户端通过。MCP8000/8055拒绝连接，使用原生UE Python/Automation/渲染PIE降级。
- 未执行：全部15项PIE的全量复跑、长期Soak/装备平衡、3/4人容量和真实网络损伤；本步运行4项直接相关商店PIE及准备阶段独立三端，未把它当作正常六波构筑通关。回城信标/商贾印留后续。GAP-030动态GE定义复制诊断未关闭，已验证的冷却为原拥有者HUD权威快照。

## 1. 目标与范围

### 目标

在原防守装备分类增加回能符240金币与修复透镜200金币；沿用每款九格限1、整备窗共计最多2件和普通库存/背包/死亡复活规则。

### 范围

回能符：只为配置了对应条件参数的英雄Q/E冷却增加8个百分点缩减，与ASC全局冷却缩减相加后上限40%；不改变W/R、物品主动、普攻、已开始的冷却。防守主动技能配置40%上限，旧能力空策略保持原通用数值策略。

修复透镜：仅装备者自己发起的本局核心修复+20%。本人购买应急石或本人明确选择神殿誓约安全选项时归属本人；队友购买/选择、取消、超时自动安全、核心自身恢复不受其他英雄的透镜影响。正常治疗继续公共Heal增幅与生命上限，满血不产生额外奖励。

### Non-Goals

回城信标/商贾印、回城入口、价格报价/节俭、八件全部完成、Boss阶段/统计/远程兵、长期平衡与3/4人容量。最初按四件调研后，在任何行为修改前按依赖明确拆成两件切片；不把未实现效果写入完成范围。

## 2. 当前事实与依据

ASC在CommitCombatAbilityStage按全局CooldownReductionPct冻结技能冷却，空策略保持此路径；Modifier运行时一一绑定ActiveGE，MakeSortedSnapshot排除死亡暂停/失效/被Break禁用效果。原Inventory已负责六装备三背包、回装6秒与精确撤销。

团队应急石和神殿誓约目前均把核心作为Heal.Source，因此英雄来源Hook不能识别本人参与。事件ResolveEvent有Proposer参数可识别明确安全选择；自动/取消仍以核心为来源。现有规格未提供回城及折扣交易入口。

## 3. 行为与契约

### 主流程与状态转换

原商店右键→原服务器经济/库存事务→正式装备实例→精确被动。回能符被动参数只在ASC提交新冷却时读取；透镜只在公共PreDealHeal调整当前事件，禁止新增治疗请求。

### 输入、输出与数据约束

新增普通英雄AbilityData可选内嵌CombatCooldownPolicy（契约1）：MaxReduction合法0..公共上限，BonusParameterKey空表示不加条件项；空策略保留旧公式。Modifier新增只读参数和查询，仅汇总当前有效Runtime，不保存第二份最终CDR。ASC在唯一冷却提交处应用策略；物品主动不读取英雄策略。Q/E正式资产选择defense_qe_cooldown_reduction，W选择无条件项上限40%。纯被动无冷却无需策略。

新增两Item/两Modifier稳定ID：CombatItem:item_mana_sigil、item_repair_lens；参数分别0.08与0.20，沿原DefenseEquipmentShopPolicy、MaxOwnedQuantity=1、BoundUnit、可售/不可丢弃/无配方。迁移扩展已有分类到六件，保留首四件与团队六件；旧迁移校验兼容六件。

### 权威边界与权限

服务器根据本人现有装备/Modifier与ASC读取效果，不接受客户端冷却/治疗量。透镜要求来源就是装备英雄、同World正式GameMode的核心且英雄登记在当前Run。应急石改用已验证且存活的Recipient为来源；阵亡购买按旧规则回退核心自身来源。明确安全事件用当前登记存活Proposer英雄，否则核心自身来源。SourceContext仍沿用原物品身份，事件不冒充物品；不新增RPC/复制字段。

### 失败、取消、过期、死亡、EndPlay 与重复请求

先走原状态/法力/冷却整体预检；非法策略不提交费用或冷却。冷却提交后移背包/卖出/死亡不重排旧GE，下一次按新效果取值。库存撤销/复活/EndPlay沿原精确句柄，无新Timer/异步持有。本人参与只以本次服务器意图归属，不追溯队友或自动事件。公共Heal已成功但观察者终结对局时保持已消费结果，不重建Event/团队账本。

### 兼容、版本与迁移

兼容新增的可选本地数据策略，不修改原函数签名、Tag、FormulaVersion、Release或网络schema；新策略自身契约1，旧资产空值保持原式。防守内容主动技能与新增Item先登记ADR-081。公开Heal的Source身份按明确参与者修正，因此原无装备/队友/自动路径需反例回归；无不可逆迁移。

## 4. 实施计划

| 文件/资产 | 变更 | 原因 | blast radius |
| --- | --- | --- | --- |
| Ability/CombatCooldownPolicy.*、Data/CombatDefinitionData.*、ASC | 可选冷却策略与唯一提交接线/校验 | 表达条件CDR且旧默认不变 | 普通英雄冷却，物品保持原式 |
| Modifiers/CombatModifierComponent.* | 当前有效Runtime参数只读求和 | 复用ActiveGE生命周期 | 无新可变账本 |
| Defense/DefenseEquipment.*、Director/TeamShop | 透镜Hook、登记查询、本人参与归属和终结复核 | 真实核心修复 | 防守模式 |
| Equipment两Item/两Modifier、Guardian技能Data与原目录 | 正式内容与Q/E条件策略 | 原商店可玩 | UE原生保存、冷读、LFS |
| Tests/CombatDefenseUtilityEquipment*、既有装备/网络/PIE | 实际冷却/修复/负例/事务/三端 | 风险验证 | 显式smoke开关 |
| Tools/setup_defense_utility_equipment.py及旧迁移兼容、smoke/target工具 | 幂等创建/复验/正式三端编排 | 可重复证据 | 仅显式调用 |
| Spec/台账/ADR/README/相关专题 | 当前事实与延期 | 文档同步 | 不改历史结果 |

## 5. 验收标准（AC）

- [x] AC-01：两个正式物品价格/ID/被动正确，原目录六件/团队六件保留，幂等迁移与冷读通过。
- [x] AC-02：真实ASC提交/施法的Q/E减8个百分点、与全局CDR40%封顶；W/R/物品/旧空策略反例、冷却快照不重排，背包/等待/死亡复活/出售/EndPlay通过。
- [x] AC-03：本人应急石及本人明确安全事件实际核心Health增量+20%；队友/自动/取消/未知目标/过量/死亡拒绝负例，回调终结不复活账本。
- [x] AC-04：原RPC买两新装备/重复请求/九格限持有/共用两件配额通过；原商店真实渲染几何与效果回读截图。
- [x] AC-05：三Target、专项与全部非PIECombat回归、资产校验/冷读、独立Dedicated双客户端及正确平台Cook/Stage通过；未执行项明确。

## 6. Definition of Done

- [x] 最小Red在生产代码之前实际运行，Green与负例有日志。
- [x] 中文注释/Details及公开扩展/生命周期说明同步。
- [x] 相关资产保存/冷读、编译及LFS通过。
- [x] F2与Push-Ready六层、文档/交付Gate通过，未自动提交推送。

## 7. 测试矩阵与命令

| 层级 | 用例/命令 | 预期证据 | 结果 |
| --- | --- | --- | --- |
| Gate/Docs | task_gate preflight/plan/build/delivery，validate_docs，diff | Saved/DEF-015 | 前置Gate通过，最终文档/交付报告见§9 |
| Build | Editor/Server/Client Development | editor-build-4.log、editor-network-los-build-2.log、targets-final/*-build.log | 三Target成功，网络探针增量成功 |
| Automation | Combat.Defense.UtilityEquipment +全非PIECombat分进程 | utility-tests-2、combat-non-pie、test-manifest.json | 7/7新专项，255/255非PIE，0失败/0未运行 |
| PIE | 原商店几何买新两件，守卫实际冷却与本人修复 | shops-pie-1/index.json、utility-shop-pie.png | 新两件及原装备/屏障/援军4/4成功，截图人工回看 |
| Assets | UE迁移/重复/只读冷读、CombatAssetValidation | assets.json、asset-idempotency.json、asset-cold-read.log、asset-legacy-read.log | 158扫描0错误0警告，重复无漂移、旧四件兼容，8份变更资产均LFS |
| Network/Cook | 显式UtilityEquipment smoke，独立Server双Client，Cook/Stage | targets-final/package-targets.log、utility-equipment-smoke-final/report.json | 811包双平台成功；三端同局/角色正确、两实物/精确金币/900修复/Q7.36 E12.88/客户端无权威 |
| Soak/Perf | 长期装备平衡/4人容量 | 后续切片 | 未执行；非本步声明 |

## 8. 风险、回滚与升级

- 风险：条件CDR误影响物品/W、40%重复相乘、背包失效未撤；治疗来源改变的全局增幅与日志归因；同步观察者结束Run后容器失效。
- 回滚方式：撤原目录新增两项及正式技能策略，再撤对应源码/资产/测试；默认空策略兼容旧内容，原首四件不变。
- 触发升级的条件：加入回城或折扣、新RPC/网络字段、改全局Formula/Release时重审。
- 需要人决定的问题：无；两件效果遵循已确定设计；其余两件明示后续。

## 9. 交付证据

日志位于Saved/DEF-015，Editor网络分层证据另见Saved/DEF-015-NetworkLOS。初始工作区干净、HEAD63e45a7。

- 先编译最小真实内容测试，equipment-red.log实际失败（两资产与CooldownPolicy字段缺失），随后才修改生产代码。
- asset-migration.log首次失败于UE结构体EditDefaultsOnly保护；改用原生ImportText重建原分类，未改二进制原始字节，asset-migration-2.log成功；重复与只读冷读、旧四件迁移兼容验证均通过。
- utility-tests-1.log中前五项通过，Shutdown观察者反例发现事件入口重建一个回执；补充外层生命周期复核后utility-tests-2.log七项全过（含购买重放/配额/出售/EndPlay）。
- combat-non-pie/index.json：255/255非PIE成功，253无警告、2调试命令负例按预期输出用法警告，0失败0未运行；test-manifest.json完整保留270发现（255非PIE+15PIE），不混入PIE造成既有CDO隔离问题。
- shops-pie-1/index.json：原装备、核心屏障、援军与新两件4/4渲染PIE成功；新例真实选守卫、原目录滚动几何右键三次、两实物440金币+应急石180、核心修复900、真实Order施放Q7.36秒/E12.88秒。utility-shop-pie.png已人工回看，核心3900/5000、两装备、金币1130。
- assets.json扫描158项/0错误0警告；asset-idempotency.json重复前后全部装备及守卫相关资产摘要一致，asset-cold-read.log和asset-legacy-read.log成功。
- network-utility-equipment-victory-2保留首轮失败：夹具错误读取服务器本地InitializedUnitDefinitionId，客户端应改读已复制HUD身份；团队Offer修订不是阶段代次，须按商品原样回传。只修探针，生产网络载荷未改。
- 第二轮Editor网络在DEF-015-NetworkFinal继续发现固定向前Q点被相邻队友挡住LOS；没有关闭碰撞/LOS或放宽权限，改为用原Targeting确认合法自身落点，再走原Order施法。DEF-015-NetworkLOS三端同局2535FB994DCF26A0922D10BFCA057850通过。
- 最终三Target与双平台Cook/Stage/Archive完成，811包/0错误，Cook唯一警告为现有MCP插件EULA提示。独立归档Server与双Client在同局39BC1E1A4FEEEACD2432C299E8BD65FE通过，NetMode1/3/3；5/4步RPC各重复发送，同款/配额拒绝、本人修复900、每人两实物、精确金币1130/1310、拥有者Q7.36/E12.88快照及客户端无Runtime权威正确。归档exe摘要与最终源码Target一致，cleanup.json显示所有本轮进程退出。
- targets-final/utility-equipment-smoke保留本地编排器首轮失败：复制旧DEF-014验证器时残留PublicAttributes字段断言，当前契约为Q/E拥有者冷却字段；修正验证器并使用全新utility-equipment-smoke-final目录复跑通过，没有修改运行代码或放宽Q/E/Core/金币/角色断言。
- 既有GAP-030的动态GE定义复制警告仍存在，不宣称完整ActiveGE定义复制；权威AttributeSet/原拥有者HUD冷却与库存投影为本步实际消费和三端验证的数据。



### 实际命令与复验入口

- 前置Gate：`python -B Tools/task_gate.py --mode preflight|plan|build --spec Doc/CombatSystem/Specs/DEF-015-defense-utility-equipment.spec.md --kind feature --report <Saved/DEF-015/报告.json>`，对应preflight.json/plan.json/build-gate.json均在行为修改前通过。
- Editor：`<UE_INSTALLED>/Engine/Build/BatchFiles/Build.bat ue_gasEditor Win64 Development <PROJECT> -WaitMutex -NoHotReloadFromIDE`；初次及后续实际日志为editor-red-build.log、editor-build-1/2/3/4.log（2为测试字段/访问器编译修正）、editor-network-fix-build.log与editor-network-los-build-2.log。末次只修改显式网络探针，生产代码与完整回归时一致。
- 全非PIE：Cmd以`-ExecCmds="Automation RunTests <test-manifest.json中255个精确名称以+连接>;Quit" -TestExit="Automation Test Queue Empty" -ReportExportPath=<Saved/DEF-015/combat-non-pie>`独立进程运行；不混合既有CDO fixture与真实PIE。
- 渲染PIE：Editor `-unattended -nop4 -nosplash -NoSound -windowed -ResX=1600 -ResY=900 -d3d11 -ExecCmds="Automation RunTests ^Combat.Defense.PIE.UtilityEquipment$+^Combat.Defense.PIE.EquipmentShop$+^Combat.Defense.PIE.TeamShopButtons$+^Combat.Defense.PIE.ReinforcementShop$;Quit" -TestExit="Automation Test Queue Empty" -ReportExportPath=<Saved/DEF-015/shops-pie-1>`，4/4（带引擎导航teardown警告、0失败）。
- 资产迁移：Cmd `-run=pythonscript -script=<REPO>/Tools/setup_defense_utility_equipment.py`；重复执行无漂移，再加`-DefenseUtilityReadOnly`新进程冷读。旧脚本加`-DefenseEquipmentReadOnly`验证首四件；`-run=CombatAssetValidation -Report=<Saved/DEF-015/assets.json>`扫描158、0/0。蓝图行为未改，相关类/引用加载、真实PIE与双平台Cook均正常，无无关BP二进制保存。
- 工具：`python -B -m unittest discover -s Tools/Tests -v`，59/59，tool-tests.log；两个迁移脚本通过Python AST，smoke脚本通过原生PowerShell AST。
- Editor网络：`Tools/run_defense_smoke.ps1 -Players 2 -UtilityEquipment -EvidenceFolder DEF-015-NetworkLOS -Port 7919 -TimeoutSeconds 100`，本机原始三端日志与report.json均保留。
- 独立Target：`Tools/package_defense_targets.ps1 -ArchiveRoot <REPO>/Saved/DEF-015/targets-final/Archive`重新构建源码Server/Client并由同版本安装版Cooker生成WindowsServer/WindowsClient Cook/Stage/Pak/Archive。随后本机`python -B Saved/DEF-015/verify_targets_utility_equipment.py`，从对应归档启动端口7920的Server及Seat1/2两Client，全部带`-DefenseSmoke -DefenseUtilityEquipmentSmoke -DefenseSmokePlayers=2 -DefenseSeed=831907 -NullRHI -NoSound`。精确exe/参数/PID见utility-equipment-smoke-final/processes.json；report.json保存同Run、NetMode、实际exe摘要，cleanup.json确认全退出。该回合验证准备期交易和实际施法，不是正常构筑六波胜利或长期平衡证据。
- 卫生：4新DataAsset+原目录+3守卫技能Data共8份uasset均LFS，lfs-check.json；无新增/修改蓝图或地图，无生成目录进入diff。`git diff --check`、最终`python -B Tools/validate_docs.py --report Saved/DEF-015/docs-final.json`与`python -B Tools/task_gate.py --mode delivery --spec Doc/CombatSystem/Specs/DEF-015-defense-utility-equipment.spec.md --kind feature --report Saved/DEF-015/delivery.json`结果随最终收尾回读。

### Push-Ready 六层

| 层 | 结论 | 证据 |
| --- | --- | --- |
| Tests | PASS | 255非PIE含7专项、4真实商店PIE、59工具测试、实际Red/Green与失败记录 |
| Types/Build | PASS | 最终Editor/Server/Client、811包双平台Cook/Stage/Archive |
| No Regression | PASS | 全非PIECombat、原装备/核心屏障/援军几何购买、158定义与旧脚本兼容 |
| Adversarial | PASS | 旧空策略/物品主动、40%叠加封顶、本人/队友/自动/取消/背包、死亡复活/出售/EndPlay、重复请求/配额/提交Shutdown、独立三端 |
| DDD/Constraints | PASS | 原ASC/GE/Heal与库存权威，原回执/字段/时钟，条件和不缓存最终属性；中文元数据与生命周期同步 |
| Decisions | PASS | ADR-081及设计/台账/README/物品/经济/扩展说明同步，回城/折扣与长期边界明确 |

## 10. 变更记录

| 版本 | 日期 | 修改 | 原因 |
| --- | --- | --- | --- |
| 0.1 | 2026-10-22 | 两件条件装备与可选冷却策略/修复归属计划 | 继续下一步授权，按依赖切片 |

## 11. 路由、自评与 Reflect

主Skill combat-feature-development，high；用户于2026-10-23确认验收完成并授权本地提交，不推送。本次不修改Skill。

| 维度 | 权重 | 分数 | 证据/扣分 |
| --- | ---: | ---: | --- |
| 需求 | 20% | 4.5 | 两件闭合，明确未交付回城/折扣，当前有效Q/E为守卫 |
| 权限 | 20% | 5.0 | 原服务器事务/ASC/Heal；无新RPC或版本契约改动 |
| 实现 | 20% | 4.5 | 可选兼容策略、有效Runtime条件和、本人修复与Shutdown清理 |
| 验证 | 20% | 4.5 | 255+4+59、三Target/158资产、双平台归档和独立三端；全PIE/长测未声明 |
| 文档 | 10% | 4.5 | Spec/ADR/台账/专题及全部失败、边界和命令记录 |
| 卫生 | 10% | 5.0 | 原生资产、8份LFS，无生成物、未自动提交推送 |

加权4.7/5（保留一位小数），EVIDENCE_SUFFICIENT。Reflect：服务器缓存不是客户端可用身份，网络探针须消费现有复制View；不同商品领域不能共用同一种Revision解释；多人实际LOS会挡住固定测试瞄准点，必须用原Targeting验证；已提交公开Heal后的每层调用者都要检查teardown，避免重新创建已清空回执。本轮根因均固化到专项断言/夹具，未为单次问题修改全局Skill。

- 用户验收状态：`已验收`（2026-10-23；用户确认“验收完成，提交吧”）。
