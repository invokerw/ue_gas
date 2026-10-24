# DEF-017 回城与回城信标

> Spec 版本：`0.1`
> 状态：`COMPLETED`
> Owner：Codex
> 创建日期：2026-10-23
> 关联进度台账：`00-01-Progress-Tracker.md` §12.15
> 风险等级：`L1`

## 0. Intake 与 Gate 记录

- 用户请求：DEF-015/016验收、提交39ca21a并推送后“继续”；接着实现八件装备中剩余回城信标及其依赖的基础回城能力。
- 附件解释：无新附件；用户给出的AGENTS.md/agent.md为工程约束，防守设计§3/§6.2为需求，已有完整网络/PIE证据为基线而不是本轮新验证。
- 已读取入口：agent.md、README、00-01、00-03、00-04、00-05、10-01/03/05/06/09/10/12/14、20-01/02/03/05、90-16；Skills/combat-task-router/SKILL.md、Skills/combat-feature-development/SKILL.md、Skills/combat-skill-development/SKILL.md；原GAS引导/Order/Modifier/死亡/复活/援军/Motion与资产迁移代码。
- 主 Skill：`Skills/combat-skill-development/SKILL.md`
- 备选 Skill 与排除理由：combat-feature-development作为通用流程复用；有自定义回城引导，主执行选择技能专项。无图片附件，不新增外部美术资源。
- 路由置信度：`high`
- F0 结论：`GO`
- F0 依据：设计明确3秒回城/30秒CD、受伤或移动打断；信标150金币、引导-1秒/复活等待-2秒。工作区干净且main与origin/main为39ca21a。原GAS/Order/Inventory/安全复活可复用，只需兼容新增原生扩展入口。
- F1 结论：`APPROVED`
- F1 审查人：Codex（L1既有继续授权内本地审查）
- F1 审查版本：`0.1`
- F1 计划审查证据：preflight/plan均通过；2026-10-23逐项复核AC、原GAS唯一引导/CD/结束、无新RPC、死亡清理前快照与生命隔离、Motion不泵送当前Cast、同步日志重入、导航碰撞、两英雄原槽/共享技能组及全矩阵回滚。ADR-083已记录，批准0.1，生产/测试/资产尚未修改。
- Build 解锁：F1通过后运行Saved/DEF-017/build-gate.json，通过后进入Red。
- F1 重审条件：变更回城提交/CD、传送权限、死亡快照、输入方案或测试矩阵时先递增版本并重审。
- F2 结论：`PASS`
- Push-Ready 结论：`READY`
- 验证：三Target、273非PIE、8渲染PIE、59工具、164定义0错误/0警告、资产幂等/冷读、818包双平台Cook/Stage/Archive和最终归档Dedicated双Client通过，见§9。
- 未执行：长期平衡、3–4人容量、网络损伤和人工连续完整局体验均不在本切片；用户已于2026-10-24验收完成并授权本地提交；未授权推送。

## 1. 目标与范围

### 目标

补齐原防守八件装备：item_return_beacon回城信标150金币，占普通装备槽，每款九格限1，每整备窗口共两件、BoundUnit不可丢弃。有效装备提供defense_recall_reduction_seconds=1和defense_respawn_reduction_seconds=2。

基础回城对防守两英雄可用，CombatAbility:defense_recall：无目标、零蓝耗、零前摇、基础引导3秒、30秒CD。使用原技能组附加到已有空技能槽，保留原技能授予顺序（游侠W/守卫R），可点击HUD或原热键；不新建RPC或第二套计时器/UI账本。

### 范围

UDefenseRecallAbility继承CombatGameplayAbility，只控制自定义引导与合法回城目的地。按启动时有效参数冻结本次引导，最少1秒；移走/获得装备不重排在途引导。CD在SpellStarted进入引导时由ASC提交，内容选原可选冷却策略MaxReduction=0，固定30秒；中断不返还，死亡后CD仍保留。受伤仅指公共DamageApplied的真实AppliedAmount>0（含HPLoss），完全吸收/免疫/0伤害不打断。Stop/新Move/其他替换Order、眩晕/沉默/妖术/冻结/死亡沿原取消路径；实际位置相对起点超过资产容差也中断。终局/换控/断线/旧生命禁止到期传送。

### Non-Goals

专用B键、独立回城按钮、事件放弃自动回城、技能/物品重映射、传送任意客户端落点、免费治疗/回蓝、跳过死亡回调、完整12波/远程敌人/Boss阶段/局末统计、美术特效与长期平衡；不改冻结Release/Formula/Tag/Event/View布局。

## 2. 当前事实与依据

- 设计20-05第177行规定基础回城3秒、30秒CD、移动或受伤打断；§6.2规定信标150/引导-1/复活-2。
- CombatGameplayAbility.cpp已有Scheduler引导任务与exactly-once结束，当前引导时长直接取AbilityData，需兼容的原生virtual时长解析及完成前校验（默认保持旧行为）。
- MotionComponent只支持连续移动，缺少安全同步传送入口；不能在Ability里SetActorLocation。新增原生同步Motion入口契约1，在组件内检查权限/生命/冲突/目标胶囊阻挡，经CharacterMovement传送；不伪造超高速跨帧移动。
- Lifecycle在OnDied前已移除有效装备/暂停Modifier，不能在死亡通知后读取折扣。新增通用只读死亡参数快照（自身契约1），在Alive→Dying前冻结当前有效Runtime参数，绑定当前LifeGeneration；复活清空，不复制或缓存最终属性。
- Director原OnHeroDied锁定RespawnAt，援军最多一次减3秒，Monitor安全复活。信标与援军减时相加，8秒普通变6、援军变5、同时变3，最低0，始终等Monitor安全复活。已死才登记的英雄不追溯信标或援军。
- UE MCP当前8000/8055超时，Saved/DEF-017/mcp-availability.json；本轮降级为UE原生Python/Automation/渲染PIE/Cook/Stage，不能记MCP成功。

## 3. 行为与契约

### 主流程与状态转换

原CastNoTarget Order→服务器拥有权/请求/生命/绑定校验→ASC激活回城→冻结引导3/2秒→原CD提交30→原UnitView施法条→完成点再次检查当前Run/登记英雄/生命/绑定/原点位置/状态→Director按冻结RunData.RespawnLocation使用原FindSafeLocation找可导航无阻挡点→Motion组件同步传送→原ChannelEnded/OrderReleased/AbilityEnded各一次。无合法落点则中断，保持原位置；CD已提交不返还。

### 输入、输出与数据约束

新增正式AbilityData/BP/AbilitySet和Equipment两DataAsset。Ability Class CDO单向指向Data，shared RecallAbilitySet追加到防守游侠/守卫UnitData，不改普通Demo；旧主技能集合内容与排序保持。特殊值配置mana_cost=0、cooldown=30、movement_tolerance=1cm、minimum_channel=1s；ChannelDuration=3/ChannelInterval=0.05。信标被动普通Runtime、永久、死亡移除、Break不禁用，不新增自定义Hook。

### 权威边界与权限

客户端只发原SpecHandle/NoTarget意图，不能提交落点/引导时长/复活时间；额外载荷沿原网络严格校验。回城只允许当前Run的登记主控英雄，活核心、未终局，执行点复核当前Owner/BindingGeneration/LifeGeneration和同RunId。服务器使用冻结RunData落点；所有客户端只显示既有UnitView/位置复制，无客户端Motion/Runtime或Timer。

### 失败、取消、过期、死亡、EndPlay 与重复请求

Ability每次激活绑定的Damage日志与根组件TransformUpdated委托由本实例持有，结束/取消/死亡/移除/OwnerEndPlay统一解绑；原ChannelTask独占到期Schedule，旧Activation/Life不得传送。完成前解绑移动观察者防自身传送重入；同步Motion开始日志观察者若销毁/死亡/发新Motion须复核，且结束日志/通知一次。原Order重放拒绝不重新激活或重开CD。

死亡快照只保存本生命实际有效参数，背包/6秒回装等待/已出售/Break过滤结果不贡献；真实死亡和显式RequestDeath均覆盖。信标效果清理仍按原Inventory，OnDied/OnRespawned与援军次数/护盾不绕过、不错配；复活后重建一次有效装备。Snapshot在Alive不对外返回旧值，重复死亡不覆盖，复活/World结束无旧生命数据可用。

### 兼容、版本与迁移

原channel virtual默认数据值/允许完成，旧Ability行为不变，新增入口只供新回城使用；旧连续Motion不改变。新增原生C++ API自身契约1，不增加蓝图事件、不修改冻结USTRUCT或网络字段。死亡快照不是属性真值，现有无参数的英雄复活语义不变。新增稳定DefinitionId无改名或redirect。原七件资产/团队六件保持，迁移幂等补齐第八件和共享回城组；旧迁移脚本接受精确已知八件。三端一起构建，不支持新旧二进制混连。

## 4. 实施计划

| 文件/资产 | 变更 | 原因 | blast radius |
| --- | --- | --- | --- |
| Ability/CombatGameplayAbility.* | 原生可重写引导时长/到期校验，冻结时长；扩展回调后检查活动状态 | 只新技能自定义引导，基类仍独占任务 | 所有旧引导默认保持 |
| Defense/DefenseRecall.* | GAS回城、事件解绑、完成前验证；Director回城资格/落点接口 | 玩法限定在当前防守Run | 防守回城 |
| Motion/CombatMotionComponent.* | 服务器安全同步传送入口与exactly-once日志/复核 | 统一位移权限，无Transform旁路 | 新原生入口，不改旧请求 |
| Modifiers/CombatModifierComponent.*、Unit/CombatUnitLifecycleComponent.* | 构造有效Runtime参数死亡快照，生命隔离/复活清空 | 装备清理前冻结减时 | 公共死亡入口 |
| Defense/DefenseReinforcement.cpp | 使用快照减时并与援军相加 | 保留死亡/救援/Monitor流程 | 防守复活 |
| Content/Defense/Recall、Equipment、两英雄UnitData、UnifiedShop | 正式回城技能组与第八装备 | 可在原HUD/商店操作 | 防守内容 |
| Tests/CombatDefenseReturnBeacon*、既有迁移脚本/显式smoke | 先Red后修复、真实UI及双Client | 三层实证与兼容 | 显式测试入口 |
| README/台账/ADR/10-03/14/20-03/05/90-16 | 当前玩法/兼容/生命周期/延期 | 文档与代码同轮 | 文档 |

## 5. 验收标准（AC）

- [x] AC-01：八件正式装备与原六团队目录，信标150/限一/绑定/无丢弃，两个参数1/2正确，迁移重复与冷读一致；共享回城技能组在防守两英雄正常授予。
- [x] AC-02：真实回城引导3秒、装备2秒、CD30；施法条/原槽热键与HUD点击能启动，在导航安全点一次传送，不恢复生命/蓝、不改变LifeGeneration。
- [x] AC-03：真实伤害/HPLoss/移动/Stop/状态/死亡/旧绑定/终局/OwnerEndPlay中断不传送；全吸收/零伤不误中断；落点被阻挡/并发Motion/客户端调用拒绝；旧引导和Motion回归通过。
- [x] AC-04：普通8秒、信标6、援军5、同时3；背包/回装等待/出售后不减时，显式死亡也有效；死亡/复活通知、Life和护盾/次数各一次，复活后装备效果重建，旧生命/重复/teardown不泄漏。
- [x] AC-05：Editor/Server/Client、全非PIE Combat（基线264不降低）、专项、原相关商店/回城真实PIE、资产验证/冷读、正式WindowsServer/Client Cook/Stage/Archive及归档独立Dedicated双Client通过；日志/报告真实保存。

## 6. Definition of Done

- [x] 在首次生产代码/资产修改之前实际运行最小失败测试；保存Red报告，不伪造或覆盖历史失败。
- [x] AC成功/失败/取消/旧生命/OwnerEndPlay/Worldteardown、exactly-once与委托/调度清理有断言。
- [x] 原生资产编译/保存/冷读/目录闭包/LFS与三端发布边界核对。
- [x] 中文注释/元数据、DDD/ADR/台账同步，F2/Push-Ready六层与文档/交付门通过；不自动提交推送。

## 7. 测试矩阵与命令

| 层级 | 实际入口 | 结果/证据 |
| --- | --- | --- |
| Gate/Docs/Tools | task_gate.py preflight/plan/build/delivery、validate_docs.py、Tools/Tests、diff --check | 开工Gate/59工具/docs/diff通过，delivery报告见§9 |
| Build | 安装版Editor、源码Server/Client Development | 三Target通过，*build*.log及targets-release-final |
| Automation | 精确非PIE清单、Combat.Defense.ReturnBeacon.* | 273通过（271成功+2既有警告）、0失败；9新增含exactly-once/取消/旧生命/EndPlay |
| Assets | UE原生Python重复/冷读、CombatAssetValidation、BP编译保存 | 164定义0错误/0警告；141防守资产SHA不变；8变更新资产LFS属性正确 |
| PIE | 原HUD/商店，三回城边界和五旧商店/援军用例 | 8渲染PIE通过，导航清理警告；报告/截图已回读 |
| Network/Cook | 正式WindowsServer/Client Cook/Stage/Archive、归档独立Dedicated双Client | 818包、三端通过、同RunId、源码/归档exeSHA一致且全退出 |
| Soak/Perf | 长期平衡/3–4人容量/网络损伤 | 未执行：本切片外 |

## 8. 风险、回滚与升级

- 风险：引导被卖装备重排；到期与伤害/死亡重入；瞬移泵送旧Cast；死亡后才读已被清空参数；客户端伪造落点；回装等待/援军凭证重复；完整原GAS/事件/视图回归。
- 回滚方式：成组撤销回城资产/新增技能组引用、信标与目录追加、新入口和接线，保留七件装备和原援军；不只删能力而留下损坏软引用。
- 触发升级的条件：需要改冻结网络载荷/事件schema/发布版本或引导提交语义；任一实质设计变化先递增Spec重新F1。
- 需要人决定的问题：无；在已批准设计和继续授权内实施，新的平衡与独立热键另立需求。

## 9. 交付证据

以下路径均在Saved/DEF-017，全部为本地实际执行，未用MCP不可达替代成功。

- 开工：preflight.json、plan.json、build-gate.json；0.1 F1/Build Gate在测试/行为/资产修改之前。
- TDD：red/index.json三失败，原因为正式信标/回城资产与英雄授予不存在；首轮Green三通过。扩展测试出售请求曾混带购买上下文/使用旧投影，修正夹具后9专项通过；回装等待模式按真实六秒解禁规则断言，未改生产语义。StartEventCancellationLeavesNoTask首次即Green（目录名start-reentry-red），没有相应生产修复，不冒称Red。
- 两个真实提交反例：位移夹具仍移动的初次PIE失败不作为业务Red；隔离移动后实测Motion开始换控仍传送，修正Owner复核。pie-terminal-commit-red实测导演Shutdown仍传送，增加开始日志/本地观察点之后同步只读ValidateBeforeCommit，完整复核Run/Binding/Life/位置/状态。delivery-return-pie三项均通过。
- 最终Editor：editor-start-regression-build-02.log成功链接，editor-start-regression-build.log完成最后新增回归编译；最终9专项见delivery-new-tests-final/index.json。源码两端targets-release-final/ue_gasServer-build.log、ue_gasClient-build.log均Succeeded；package-targets.log完成818包WindowsClient/WindowsServer Cook/Stage/Archive。
- 非PIE：delivery-non-pie-a132成功，b130成功+2调试命令边界警告，delivery-new-tests-final9成功，共273且0失败；test-manifest-final.json和automation-delivery-summary.json严格比较完整名称，未降低264基线。第一次Combat混跑误含PIE后复现GAP-027临时CDO引用旧World崩溃，combat-non-pie-final.log保存；最终精确过滤/独立进程全部通过。额外混合NullRHI Defense运行已停止本任务具体进程，未算通过；该进程曾锁DLL，停止后重新链接成功。
- 渲染PIE：delivery-return-pie3项，delivery-existing-pie5项（EquipmentShop、MerchantSeal、ReinforcementShop、TeamShopButtons、UtilityEquipment）全部通过，每项有既有导航启动/teardown警告。return-beacon-channel-pie.png与arrival-pie.png实际视觉回读；确认原R槽/2秒引导/冷却与商店实扣150。
- 资产：asset-validation.json为164定义、0错误/0警告；asset-idempotency.json对141个防守uasset SHA比较一致，旧三迁移与新增脚本重复执行通过；asset-readonly.log冷读确认8装备/6团队、150价格、参数1/2和两英雄共享组。新BP原生编译保存，lfs-check.json确认8变更新资产均LFS。首次迁移因枚举拼写失败，修正后同路径重跑asset-migration.log，不将失败记为通过。
- 最终独立三端：targets-release-final/return-beacon-smoke-final/report.json，RunId=26B8A72143977E6ED60ACDA318BFE815。实际NetMode1/3/3、同Run，两英雄基础3/装备2秒、CD30、真实伤害中断、安全回城、6秒复活、Life+1、装备重建；每客户端3请求/3重放、Runtime=0/Authority=0。源码与归档exe SHA一致，cleanup.json确认全部退出。此前targets-final-02一轮通过为历史补充，不混作最终修订。
- F2：f2-review.json独立清单复核权威、时序、exactly-once、生命周期、重入、数据、兼容、范围，两个实质发现均真实Red后收敛，PASS。Push-Ready六层：逻辑契约273、网络归档三端、编辑器8PIE、资产164/幂等/冷读/LFS、构建三Target/818包、文档工具59/docs/diff，READY。delivery-gate.json记录最终机器Gate。
- 风险与未执行：GAP-027旧CDO测试World引用、GAP-030动态GE定义复制诊断仍保留；后者不影响本轮实际位置/View/库存/CD/Life断言，未称其已修复。长期平衡、3–4人容量、网络损伤、人工完整局未执行。MCP8000/8055超时，全部采用原生UE降级。

## 10. 变更记录

| 版本 | 日期 | 修改 | 原因 |
| --- | --- | --- | --- |
| 0.1 | 2026-10-23 | 基础回城、信标、死亡参数快照及安全Motion兼容入口计划 | 用户继续授权，补齐第八装备 |

## 11. 路由、自评与 Reflect

主Skill combat-skill-development，high；自定义阶段使用UDefenseRecallAbility，原GAS/Order/Inventory/普通Modifier与公共Motion负责权威和生命周期，通用feature流程作依赖，未修改Skill。

| 维度 | 分数 | 证据与扣分 |
| --- | --- | --- |
| 需求与AC（20%） | 5 | 两英雄/8装备/6团队、五项AC范围内矩阵完成 |
| 架构权限（20%） | 4.5 | 原权威与冻结schema；新原生同步入口仍需后续复用观察 |
| 实现数据（20%） | 4.5 | 164定义、141资产幂等、死亡和重入实证；灰盒平衡另列 |
| 验证证据（20%） | 4.5 | 273非PIE/8PIE/三Target/独立三端；GAP-027/030与警告保留 |
| 文档可观测性（10%） | 5 | DDD/ADR/台账/Spec及真实证据同步 |
| 交付卫生（10%） | 5 | diff/LFS/生成目录/旁路检查，无自动提交推送 |

加权自评4.7/5，COMPLETED；用户于2026-10-24验收完成，授权本地提交，未授权推送。

Reflect：同步事件广播可能在最终完成点换控/终局，日志之前的一次检查不足。证据是两个实际PIE反例及最终修订Green；根因属于本次实现的提交边界，修改公共Motion同步复核与回城调用，原Skill/模板无须扩大。预期收益是所有开始观察者返回后资格仍有效才移动，回归覆盖Owner/终局/死亡/取消。受控调优本轮不修改通用流程；无需用户新决策。验证的CDO/World混跑、MCP降级和动态GE诊断边界如实保留。
