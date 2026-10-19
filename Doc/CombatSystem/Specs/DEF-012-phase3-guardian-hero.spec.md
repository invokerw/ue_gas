# DEF-012 Phase 3C 神殿守卫英雄包与开局选择

> Spec 版本：`0.1`
> 状态：`已验收`
> Owner：Codex
> 创建日期：2026-10-17
> 关联进度台账：`00-01-Progress-Tracker.md` §12.13
> 风险等级：`L1`

## 0. Intake 与 Gate 记录

- 用户请求：验收完毕，继续吧。DEF-011刺客记为已验收，继续此前约定的第二英雄；本次没有提交/推送授权。
- 附件解释：无新附件；用户AGENTS.md及agent.md为工程约束，20-05 §4.1/5.4为英雄技能需求。
- 已读取入口：agent.md全文、README、00-01/03/04/05、10-01/03/05/06/09/10/12、20-02/03/05、90-16；Skills/combat-task-router/SKILL.md、Skills/combat-feature-development/SKILL.md、Skills/combat-skill-development/SKILL.md；真实MCP地图、英雄定义、Director/Choice/Order/Ability/Modifier/Targeting与测试。
- 主 Skill：`Skills/combat-skill-development/SKILL.md`
- 备选 Skill 与排除理由：包含完整玩家技能包，通用feature流程作为基础，不替代技能专项。
- 路由置信度：`high`
- F0 结论：`GO`
- F0 依据：公开Ability/Modifier/Targeting能表达盾击、护盾、范围控制及蓄盾；仅新增玩法适配与开局固定白名单选择，不扩展战斗内核。
- F1 结论：`APPROVED`
- F1 审查人：Codex（L1本地审查）
- F1 审查版本：`0.1`
- F1 计划审查证据：preflight/plan均0错误；按00-05 L1授权审查0.1，冻结技能数值/三目标验证；开局门排除已有构筑并独立于旧HeroSwap，RPC严格身份/回执/限频；主动复用公开阶段，W用DataDriven、被动/失衡用Modifier；默认参数兼容、生命与委托清理、资产回滚明确，批准。
- Build 解锁：已解锁；批准0.1后运行build Gate，Saved/DEF-012/build-gate.json。
- F1 重审条件：范围、公式/版本、权限、测试矩阵或回滚实质变化先递增Spec并重新plan/build。
- F2 结论：`PASS`
- Push-Ready 结论：`READY`
- 验证：开工MCP确认原地图/英雄；本步guardian-green7/7、combat-full237/237、真实守卫PIE与双客户端六波通过，144定义0错误/警告、24资产幂等、797包Cook通过。三Target与6项真实PIE均通过，最终结果见Saved/DEF-012/verification-summary.json。
- 未执行：四人容量、长时平衡、打包exe与完整中途迁移不属本步；本步必需验证均已完成。

## 1. 目标与范围

### 目标
独立可选第二英雄神殿守卫，Q/W/E与被动在真实防守关卡可用；三端继续使用同一公共权威战斗系统。

### 范围
- CombatUnit:defense_guardian：初始1400生命、240法力、85物理普攻、1.8秒间隔、0.3秒攻击前摇、160cm攻击距离、8护甲、500cm/s移速、1生命/3法力每秒恢复。无普攻弹体、无单体奖励、无自主AI。复用现有人形骨骼/动画与HUD，添加纯表现蓝色盾牌和武器识别。
- 三主动初始等级1、最高3，纯被动等级1不占R槽。Q盾击：点目标确定方向，400cm半径/120度扇形，物理180/240/300，耗蓝30，CD8秒，前摇0.25秒。公共Targeting先圈选，再在许可目标内进行角度过滤。
- W固守：无目标，公共ApplyModifier动作给自身当前最大生命20%的3秒盾；耗蓝35，CD12/11/10秒，前摇0.1秒。数据驱动动作复用UDefenseShieldRuntime，新增默认关闭的shield_fraction/shield_cap_fraction参数；固定shield_amount旧配置保持。已配置上限的盾存在时，同族盾新建余量不超过最大生命40%减去已有余量，核心旧盾不配置上限。
- E震地：自身450cm范围，物理80/120/160，普通敌人1秒眩晕，耗蓝45，CD14秒，前摇0.35秒。Boss以真实DA_DefenseBoss身份配置，单次施放每Boss仅1层失衡，不直接1秒硬控。
- 被动坚忍蓄盾：每承受3次有效直接普攻，武装一次下次有效直接普攻；命中后获得最大生命5%的3秒盾并进入4秒内置冷却。有效指Applied+Absorbed>0，反伤/技能/派生伤害不计数；按RootEvent去重并有界缓存，死亡移除并由Intrinsic在复活重建，Break期间Hook不工作。数值全部来自Modifier参数。
- Boss失衡最小公共Modifier组合：首次一层起固定8秒窗口，全队合计4层触发2秒Stunned，并有8秒保护（眩晕2秒加之后6秒免疫失衡）；不是刷新8秒滑动窗口。不同守卫来源合并为Boss自持状态，来源快照保留施法身份；层数/保护在现有Modifier View可见。Boss阶段攻击本步不做。
- 开局默认游侠；构筑面板提供“改用神殿守卫”。每玩家仅成功一次，服务器仅使用GameMode配置的GuardianClass，客户端不提交任意类/数值。只允许初始Preparation、活英雄、未选卡/未重抽/零经验/空库存/未购买团队效果；开局进入战斗即关闭。新入口不调用有已知回滚缺口的通用HeroSwap。
- 选择通过独立owning-client组件RPC与schema1只读视图；正RequestId、RunId、BindingGeneration、LifeGeneration、同World/组件owner、共享限频/重放、有界完整载荷回执全部校验。成功前先安全导航出生并初始化，绑定失败恢复旧角色且不扣钱；成功后记录和委托改绑，销毁旧角色及其AIController。种子/金币保留。已有未选择候选按新英雄修订一次、保留原截止时间，不重置两次重抽预算。
- 刺客目标优先级加入真实守卫ID，保持其余波次、商店、奖励和游侠资产不变。

### Non-Goals
不实现大厅/任意时刻来回换英雄、库存/经验迁移、游侠完整新三技能、24机制卡、Boss三阶段与预警、事件、装备或局末统计；不宣称正常构筑独立通关与最终美术平衡。DEF-009旧HeroSwap/机制卡缺口仍另行处理。

## 2. 当前事实与依据

- 防守英雄实际ID是defense_ranger，AbilitySet来自原DrowRanger；当前无第二英雄或选择入口。
- DefenseHeroSwap.cpp先移除旧效果再绑定，失败恢复旧句柄不完整；本步开局入口通过严格空构筑门与独立事务避免调用此旧路径。
- UCombatGameplayAbility支持统一Cost/Cooldown/Scheduler/取消；公共范围查询无扇形API，局部角度过滤不绕过关系/距离/LOS许可。
- UDefenseShieldRuntime已有吸收余量和到期，当前只读shield_amount；现有Modifier提供Intrinsic、Stack、Stunned、Break、死亡清理。
- DEF-010/011共39文件差异均保留未提交；本步不能用前序230/4项结果冒充新验证。

## 3. 行为与契约

### 主流程与状态转换
开局只读选英雄可用→客户端意图→服务器信封/业务校验→安全Spawn→初始化Unit/AbilitySet→绑定并提交Record→销毁旧Unit→私有确认；失败不清空旧Unit。随后原QWE输入→Order/ASC→统一前摇/提交→公开Damage/Modifier→UnitView。死亡移除护盾/被动状态，复活重建Intrinsic，终局按原Director清理。

### 输入、输出与权限
选择只带有限身份，不带Actor类和技能数值；技能命中由服务器重算，客户端不创建Runtime或结算。所有平衡在DataAsset Special/RuntimeParameters；新增Class/Data身份不重命名旧内容、不改变发布契约。新选择组件schema1独立，不修改现有Choice/Run/HUD schema。

### 失败、取消、过期、死亡、EndPlay、重复请求
旧绑定/旧Life/跨World/已购或已选/战斗期/恶意重放拒绝；同载荷成功重放返回原回执，不再出生。非法白名单、导航/碰撞或初始化失败保留旧角色。施法不足法力、冷却、目标非法、前摇取消和死亡仍由基类中断；伤害后重查目标Life再控制。被动只有有效直接普攻计数、同Root最多一次；冷却用World游戏时间比较，不新建Timer。Modifier到期/死亡/Owner EndPlay/World teardown由现有Scheduler和句柄代次清理。失衡触发先锁定再公共Apply/Remove，不能在Hook内假定排队返回有效句柄。

### 兼容、版本与迁移
默认不配GuardianClass则隐藏新入口；旧盾fraction/cap参数默认0，旧资产行为保持。新蓝图/Ability/Modifier/Set/Unit由原生UE创建保存并冷读；仅改DefenseGameMode的白名单引用和刺客Profile新增Hero优先级。新增独立schema1与身份先记ADR078；通用发布/事件/公式版本不变。

## 4. 实施计划

| 文件/资产 | 变更 | 原因与blast radius |
| --- | --- | --- |
| DefenseGuardian.h/.cpp | 纯表现英雄、扇形/范围技能、蓄盾和失衡Runtime | 使用公共扩展点 |
| DefenseCoreShieldRuntime | 可选生命比例与同族护盾上限 | 默认0兼容旧数据 |
| DefenseHeroSelectionComponent、Director/PlayerController/GameMode/BuildWidget | 开局受限选择、私有视图、UI及安全事务 | 仅Defense模式 |
| Defense/Guardian资产、GameMode、AssassinAI | 新英雄完整装配和AI优先级 | 不改原游侠和波次 |
| Tools/setup_defense_guardian.py | 原生幂等资产和冷读 | LFS维护 |
| Guardian专项、PIE/Network探针 | 真实施法/吸收/选择/复制/失败回归 | 显式测试开关 |
| ADR/Spec/设计/台账/README/生命周期 | 当前行为、边界和证据 | 文档 |

## 5. 验收标准（AC）

- [x] AC-01：守卫蓝图/定义/三主动一被动/中文说明合法，初始技能可用、原英雄保留，刺客对两英雄同优先级。
- [x] AC-02：Q扇形前后/友军/LOS、等级伤害、W实际吸收/到期/上限、E范围与普通眩晕/Boss固定窗口累计与保护有实际测试。
- [x] AC-03：被动三次直接受击→下一次普攻→5%盾/4秒内置冷却，派生/重复Root/死亡复活/Break/清理正确。
- [x] AC-04：实际UI开局选择可用；权限/限频/旧身份/重放/战斗期/已有构筑/出生失败安全，金币与候选截止保留，旧Actor和控制器无残留。
- [x] AC-05：完整非PIE回归、真实守卫PIE、双客户端选择和施法/复制、三Target、资产冷读/幂等/校验/Cook与docs/diff/delivery通过。

## 6. Definition of Done

- [x] 先运行缺失真实英雄内容Red；完成正向与失败技能Golden Case，不以编译缺符号冒充Red。
- [x] 设计与实现一致，全部数值配置、中文元数据和公共入口完整。
- [x] 实际验证分层闭合，无Actor Timer/客户端伤害/Health写入旁路。
- [x] F2发现关闭，记录未执行与剩余风险、恢复Editor，待用户验收且不自动提交。

## 7. 测试矩阵与命令

| 层级 | 命令/用例 | 预期 | 结果 |
| --- | --- | --- | --- |
| Gate/Docs | task_gate preflight/plan/build/delivery、validate_docs、diff check | 0错误 | preflight/plan/build通过；最终docs/delivery报告 |
| World | Guardian专项+完整非PIE Combat | 真实属性/技能/权限/生命周期/旧盾回归 | guardian-green.log7/7，combat-full.log237/237 |
| PIE | Defense Guardian真实选择按钮、QWE和敌人 | 正常UI/模型/技能/冷却与血条 | guardian-pie-spawn-fix.log加pie-regression.log共6/6 |
| Build | ue_gasEditor/Server/Client Development | 三Target成功 | spawn-fix-build/server-build/client-build.log均成功 |
| Assets | 原生脚本只读和重复、CombatAssetValidation、Windows地图Cook | 合法/幂等/0错误 | 144定义零错误/警告、24资产幂等、797包Cook |
| Network | run_defense_smoke双客户端Guardian开关 | 真RPC选择/技能与可见复制、六波奖励/复活 | network-guardian-victory-2/report.json三端Pass |
| Perf/Soak | 本步不改变容量预算 | 四人容量/长时平衡/打包exe另行 | 不适用 |

## 8. 风险、回滚与升级

- 风险：选择与开始/过期相撞、绑定重入、盾叠加、Boss层跨来源、被动重复事件。分别用服务器阶段/身份、事务重入锁/二次复核、统一余量、Boss自持堆叠与固定截止、Root去重和生命生命周期处理。
- 回滚：移除本步新资产/入口和可选参数、撤销GameMode/AI本步引用，保留DEF-010/011原差异；旧数据默认值继续兼容。
- 升级：如需新通用registry、任意时刻英雄迁移或发布契约变化先升Spec重审。
- 需要人决定的问题：无，原设计已确定职责，可逆数值在本Spec冻结。

## 9. 交付证据

Saved/DEF-012实际日志/截图/JSON；不得将测试清场说成玩家正常通关。GAP-027非PIE/PIE分进程，GAP-030动态GE既有告警和DEF-009已披露缺口保留。

## 10. 变更记录

| 版本 | 日期 | 修改 | 原因 |
| --- | --- | --- | --- |
| 0.1 | 2026-10-17 | 初稿 | 刺客验收后继续第二英雄 |

## 11. 路由、自评与 Reflect

主Skill combat-skill-development，high；最终按实际证据自评4.7/5，用户验收状态READY_FOR_REVIEW，详见§13；未修改Skills。

## 12. 实施过程证据（验证中）

- red.log：真实守卫资产缺失断言失败，之后才开始实现。
- 初轮编译发现复制宏要求OutLifetimeProps参数名，以及测试Actor局部Owner遮蔽成员；均修正，editor-green-build.log成功。
- asset-migration.log暴露Python临时SpecialValue不能用EditDefaultsOnly setter；改用原生ImportText，asset-migration-retry.log验证完整英雄/技能组/蓝图。
- SourceFileCache.bin曾使UBT漏发现新增测试；移除该生成缓存后guardian-discovery-build.log明确编译新测试，不把零测试当通过。
- guardian-tests.log首轮6/7通过，被动测试以0.15秒重复发起普攻，低于公共0.20秒最短间隔；改为0.25秒后guardian-green.log 7/7通过，未修改生产攻击节拍。
- guardian-pie.log在场景开始前引擎DDC的InheritedContext/HttpConnectionPool断言崩溃，不计通过。guardian-pie-retry/diagnostic.log进一步暴露真实开局选择HeroInitializationFailed，原因是Deferred Spawn尚未完成时ASC ActorInfo未就绪；调整为FinishSpawning后幂等初始化。失败期间原角色和金币未改。
- 骨骼使用材质标记已补入原生生成器，后续验证不得出现Default Material替代。完整三Target/PIE/网络/资产矩阵待最终结果，不以专项成功代替交付。


阶段验证补充：guardian-pie-spawn-fix.log通过真实Slate选角、保留Command Pawn/金币/输入种子、三主动槽与QWE实际效果、普攻被动盾；已目视核对guardian-pie.png蓝色人形、盾牌、血条和三技能CD。combat-full.log 237/237，network-guardian-victory-2/report.json三端Pass，服务器Skills=7与实际吸收、客户端W/被动/眩晕投影，六波核心4840且共享奖励/复活正确；技能使用显式训练靶，波次由原测试清场，不代表玩家自然通关。asset-validation.json为144定义0错误0警告，idempotence-result.json证明24相关资产字节不变，Cook为797总包（790cook/7平台跳过）、0错误/1既有MCP授权提示。F2具体检查见f2-review.json。


## 13. 最终交付与 Reflect

F0 GO、F1 0.1 APPROVED、F2 PASS、Push-Ready READY。DEF-012待用户验收，DEF-010/011既有差异保留；本次没有提交或推送。汇总报告：Saved/DEF-012/verification-summary.json。

- 237非PIE完整通过（含新增7项），真实守卫选择/技能PIE通过；选卡、商店、导航、攻城和刺客5项回归均通过，共6项。非PIE和PIE始终分进程（GAP-027），没有用通过退出码代替实际用例计数。
- 三Target成功，构建配置逐字节恢复。144定义零错误/警告；原生冷读与24相关资产字节幂等；Cook797包、0错误，保留1条既有MCP授权提示。
- 真Dedicated服务器和两客户端通过：第一客户端选守卫，另一客户端保留游侠；重复请求不重生第二个守卫，换载荷和跨Owner拒绝，开战后关闭选择；Q/W/E、实际吸收、被动盾和客户端只读投影通过。六波胜利/复活/金币经验账本一致。网络运行使用安装版Editor的-server/-game，独立Server/Client目标为编译证据，不声称已验证打包exe部署。
- 新英雄入口与旧HeroSwap分离，ASC注册顺序缺陷已由实际Slate按钮PIE固化；生命周期与权限F2审查记录见f2-review.json。原游侠、商店价格、波次编成和奖励不变，刺客加入守卫同优先级。
- 交付仍为灰盒与初始数值；三技能效果可玩，不含正式攻击动画/专属技能特效、Boss三阶段或玩家自然通关平衡证明。DEF-009原型限制与GAP-030仍保留。

| 自评维度 | 权重 | 分数 | 证据与扣分原因 |
| --- | ---: | ---: | --- |
| 需求与AC | 20% | 5 | 三主动一被动、受限选择、失衡、真实效果与旧内容闭合 |
| 架构与权限 | 20% | 5 | 公共Targeting/ASC/Order/Modifier，安全信封、事务提交与清理 |
| 实现与数据 | 20% | 4 | 原生完整英雄包，仍为灰盒美术与初始平衡 |
| 验证证据 | 20% | 4.5 | 三Target、237/6项、双客户端、144定义/冷读/Cook；非四人或打包部署 |
| 文档与可观测性 | 10% | 5 | ADR078、技能与入口边界、失败原因日志和完整修复证据 |
| 交付卫生 | 10% | 5 | 保留前序差异、LFS、配置恢复、不自动提交 |

加权4.7/5，EVIDENCE_SUFFICIENT，READY_FOR_REVIEW。Reflect：有技能的角色出生依赖ASC ActorInfo就绪，不能直接复制无技能敌人的Deferred初始化时序；纯World技能测试不能替代真实蓝图/UI出生验证。已用实际Slate选角、旧Actor/Controller清理、技能效果与种子保留检查固化。UBT生成缓存和材质使用标记问题保留证据并修正本任务生成器，不扩大修改通用Skill。无需额外用户决定。


资产身份：`/Game/Combat/Demo/Defense/Guardian`包含CombatUnit:defense_guardian、CombatAbilitySet:defense_guardian_ability_set，以及CombatAbility:guardian_shield_bash / guardian_fortify / guardian_ground_slam / guardian_resolve。Q/E使用公开Ability执行点，W使用DataDriven ApplyModifier，被动/失衡使用Modifier Runtime；旧游侠与发布schema兼容。

最终delivery-final.json为80个工作区变更、0错误（含保留的DEF-010/011）；docs-final.json为104篇Markdown/562本地链接、0错误。git diff --check及新增文本尾随空白检查通过。editor-restored.json经8055端口MCP确认L_Defense、无PIE、守卫Blueprint无脏修改、All Saved。可在当前编辑器Alt+P，先点“改用神殿守卫”，再锁定种子开始并选强化。


## 14. 用户验收与本地提交（2026-10-19）

用户确认“验收完成，提交吧”。DEF-012神殿守卫已验收，本次本地提交同时包含此前已验收但尚未提交的DEF-010攻城与DEF-011刺客。原实施日期和验证日志保持2026-10-17；本次仅回写验收状态、核对80文件范围及已验证资产哈希、运行文档/差异/交付检查，不重新声称执行UE运行验证。没有推送授权，后续事件、装备和Boss阶段等内容仍按原范围另行推进。
