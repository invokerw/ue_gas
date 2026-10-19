# DEF-011 Phase 3B 刺客进入真实波次

> Spec 版本：`0.2`
> 状态：`已验收`
> Owner：Codex
> 创建日期：2026-10-17
> 关联进度台账：`00-01-Progress-Tracker.md` §12.13
> 风险等级：`L1`

## 0. Intake 与 Gate 记录

- 用户请求：验收完毕，继续吧。将DEF-010记为已验收，继续设计中的刺客切片；本条没有新的提交/推送授权。
- 附件解释：无新附件；AGENTS.md和agent.md为工程约束，20-05为玩法需求。
- 已读取入口：agent.md全文、README、00-01/03/04/05、10-01/09/10、20-02/03/04/05、90-16；Skills/combat-task-router/SKILL.md与Skills/combat-feature-development/SKILL.md；AI角色实现、Director、数据、已有测试/资产脚本。无.codegraph，使用rg。
- 主 Skill：`Skills/combat-feature-development/SKILL.md`
- 备选 Skill 与排除理由：不是玩家技能，本步复用普攻和现有号角，不需要技能专项主流程。
- 路由置信度：`high`
- F0 结论：`GO`
- F0 依据：Lane角色树已支持巡线、交战、失去目标、超时归位；仅需默认关闭的生命评分和出生组角色接入。
- F1 结论：`APPROVED`
- F1 审查人：Codex（L1本地审查）
- F1 审查版本：`0.2`
- F1 计划审查证据：0.2 plan-v02通过；审查默认-100保持所有旧候选，过滤仅未完路线，不排除完点核心，新增过滤/回退测试；批准0.2。原0.1审查：preflight/plan均0错误；逐项审查确认低生命评分默认0兼容、仅当前合法观察读取、角色与导演互斥、配置冻结、限时职责与公共生命周期；冻结两波编成/数值/真实伤害验证，保留DEF-010差异，回滚可逆。按00-05 §4 L1授权批准0.1。
- Build 解锁：已解锁；批准后运行build Gate，报告Saved/DEF-011/build-gate-v02.json。
- F1 重审条件：增加新控制器、技能、公共网络契约，或范围、测试、回滚实质变化时先升版本重审。
- F2 结论：`PASS`
- Push-Ready 结论：`READY`
- 验证：三Target成功、完整230非PIE/4真实PIE通过、131定义零错误警告、双客户端4刺客自然打英雄和4攻城打核心、775包Cook、6资产幂等。Saved/DEF-011/verification-summary.json汇总。
- 未执行：四人容量、长时数值平衡、打包exe部署和Phase3其它内容不属于本步；无本步必需验证缺口。

## 1. 目标与范围

### 目标
第4/5波双路各出现一名可辨认刺客，感知英雄后临时脱线攻击，选择点优先生命比例较低的英雄，完成路线后没有可见英雄则攻击核心；复用公共AI/移动/普攻和号角15%加成。

### 范围
- 新defense_assassin定义、紫色瘦高锥体外形与血条；初始生命320、移速360、攻击28、攻击间隔1.2s、前摇0.2s、范围120、护甲1、无回血/击杀金币经验。
- Wave4两路各5普通改4普通+1刺客；Wave5各5普通+1攻城改4普通+1刺客+1攻城。总数10/12、奖励190/500与240/600保持，旧Wave1/2/3/6不改。
- AI Profile增加MissingHealthWeight，默认0；可见候选分数加入clamp(1-Health/MaxHealth,0,1)*weight。只对公共Targeting许可的当前观察读取ASC，不更新失去视野的记忆。配置有限0..10000，旧配置仍旧算法。
- SpawnGroup新增可选RoleProfile，默认空保持原导演Move→Attack队列；非空时仅角色树下令，Director设置职责并启动，不再提交人工队列。UnitData.AIProfile仍必须为空，避免早启动；拒绝Boss角色组/无感知/无编译树，初始化冻结Profile数值。
- 刺客Profile复用已编译ST_AI_Lane：英雄definition优先级10，其他0；missingHealthWeight100、距离/威胁权重0、半径1200、最大偏离1600、单次交战8s、失去目标后继续路线、超时归位。观察0.2s、闲置0.25s、记忆0.5s。已有持续攻击不主动切换目标，只有合法选择点择敌。
- 职责航点复用原路线，额外终点置于核心前安全位置（若原路线末点已在核心感知半径内则不增加）；确保无英雄时仍可发现核心。职责Home为实际安全出生点。

### Non-Goals
不代表整个Phase3完成；无第二英雄、远程兵、隐身/闪现/新技能、完整预算缩放、事件、装备和Boss阶段；不修DEF-009已记录缺口、不宣称真实构筑通关与数值平衡。

## 2. 当前事实与依据

- DefenseDirectorComponent.cpp:40冻结单位定义，:197生产出生后仅导演下令；DefenseData拒绝非空UnitData.AIProfile。
- CombatAIRoleBrain.cpp:112公共查询采样，现有Score仅威胁与距离；角色树已具有Leash/MaxEngagement/归位/路线恢复。
- 当前DEF-010未提交差异完整保留；223非PIE、3PIE、三Target、网络/冷读/Cook既有证据不能代替本次验证。
- 追猎号角已按稳定DefinitionId片段assassin匹配；本次增加真实定义生命差回归。

## 3. 行为与契约

### 主流程与状态转换
出生组→安全Spawn→登记LifeGeneration→设置Home/Route→启动冻结Lane Profile→巡线→公共范围/LOS感知→角色Prepare/Consume Attack→失敌续路或8秒/越界归位→重采样。核心作为无英雄时合法敌对候选。终局由原导演清理。

### 输入、输出与权限
服务器独占观察与下令；ASC是生命真值、Order/PathFollowing是移动真值；客户端只复制单位/UnitView。默认0字段为向后兼容可选策略，不改变发布契约、RPC或视图schema。低生命评分公式先记ADR再实现。

### 失败、取消、过期、死亡、EndPlay 与重复请求
非法Profile在初始化前拒绝；启动失败结束本局RoleStartupFailed，不同时退回人工队列。角色协议继续验证ControlEpoch/Life/Handle，目标死亡/丢失/复活使旧候选无效；手动接管/死亡/Destroy/World teardown取消已有感知Scheduler、事件委托和Order，不新增Timer或重复终结入口。

### 兼容、版本与迁移
旧Wave RoleProfile空，旧Profile权重0保持行为；新Profile资产独立引用旧编译树，冻结数值而不修改树。原生UE创建/编译/保存后独立冷读，新资产LFS。正常关闭已保存Editor构建，完成后恢复地图。

## 4. 实施计划

| 文件/资产 | 变更 | 原因与blast radius |
| --- | --- | --- |
| AI/CombatAIProfileData、CombatAIRoleBrain | 默认0生命评分及有界校验 | 通用可选策略，旧Profile兼容 |
| DefenseTypes/Data/DirectorComponent | 可选RoleProfile校验、冻结与独占启动 | 仅选择角色组受影响 |
| DefenseObjective、Assassin BP/Unit/Material/Profile、Wave4/5 | 纯表现外形与配置 | 真实内容接入 |
| Tools/setup_defense_assassin.py | 原生创建、幂等和只读回读 | 可逆资产维护 |
| AIRole/DefenseAssassin/TeamShop/PIE/Network测试 | 真选择、伤害、生命周期、复制 | 独立测试探针 |
| Spec、ADR、设计、AI指南、台账、README、90-16 | 边界、权威与证据 | 文档同步 |

## 5. 验收标准（AC）

- [x] AC-01：四/五波各两名刺客、血条和新外形；攻城编成/总数/奖励不变，定义与Profile合法。
- [x] AC-02：同优先级可见英雄中低生命比例优先；默认0旧评分、LOS许可/失去视野、死亡/旧Life、追击期限和取消清理受测。
- [x] AC-03：出生组Profile冻结且只有AI下令，旧组导演队列不变；无英雄仍威胁核心。
- [x] AC-04：号角对真实刺客实际伤害+15%且到期撤销；真实PIE出现两名刺客，走公共移动并自然伤害英雄，截图可见。
- [x] AC-05：三Target、完整非PIE分进程回归、必要PIE、双客户端Dedicated、资产冷读/幂等/校验/Cook、文档与交付门通过。

## 6. Definition of Done

- [x] 先跑真实缺失编成Red，再实现和Green，不以编译缺符号替代行为Red。
- [x] 全部AC有证据，公共入口/中文可编辑字段说明完整。
- [x] 生命周期、旧内容和真实网络检查通过，未执行项明确。
- [x] F2对抗审查、台账和设计事实同步，diff无生成文件，待用户验收且不自动提交。

## 7. 测试矩阵与命令

| 层级 | 用例/命令 | 预期证据 | 结果 |
| --- | --- | --- | --- |
| Gate/Docs | task_gate preflight/plan/build/delivery、validate_docs、diff --check | Saved/DEF-011 | preflight/plan/build通过，最终docs/delivery报告 |
| World | Combat.AI.Roles及Defense新用例、完整非PIE | 低血选择/LOS/冻结/取消/号角/旧内容 | combat-full.log 230/230 |
| PIE | 新AssassinHeroAttack+旧Siege/导航/商店按需复验 | 真实生产路线和英雄自然受击 | assassin-pie-final.log与pie-regression.log共4项通过 |
| Build | Editor/Server/Client Development | 三Target成功 | editor-final-build/server-build/client-build.log成功 |
| Assets | 原生脚本冷读/重复、CombatAssetValidation、地图Windows Cook | 编译保存/幂等/0错误 | 131定义、6资产字节幂等、775包Cook |
| Network | run_defense_smoke双客户端刺客选项 | 双端可见移动，服务端自然伤害英雄和完整胜利清理 | network-assassin-victory-2/report.json三端Pass |
| Perf/Soak | 不新增全量扫描或计时器，复用感知有界数组 | 长时/四人容量不属本切片 | 不适用 |

## 8. 风险、回滚与升级

- 风险：多AI写入者、优先英雄导致永久偏离、观察隐藏生命、客户端误启动、早清场掩盖自然攻击。分别以组互斥、8s/1600cm职责、公共许可后读ASC、服务器Brain与真实自然伤害探针约束。
- 回滚方式：只还原本任务RoleProfile/可选评分/新内容及Wave4/5刺客替换，不回退DEF-010或其他既有差异；旧字段默认使旧内容可继续加载。
- 升级：必须引入新RPC/控制器/替代寻路或改变既有公共契约时先升Spec。
- 需要人决定的问题：无；用户已授权继续可逆实现。

## 9. 交付证据

实现后按实际日志填入Saved/DEF-011；未运行的验证不计通过。已有GAP-027测试隔离、GAP-030动态GE警告与DEF-009构筑原型边界保持。

## 10. 变更记录

| 版本 | 日期 | 修改 | 原因 |
| --- | --- | --- | --- |
| 0.1 | 2026-10-17 | 初稿 | 用户验收攻城后继续刺客 |
| 0.2 | 2026-10-17 | 巡线最低候选优先级 | 事前修订避免过早攻核反复归位，plan/build重新通过 |

## 11. 路由、自评与 Reflect

主Skill combat-feature-development，high。完成后用实际AC、权限、实现、验证、文档、卫生六项加权评分；最终自评4.7/5，用户验收状态READY_FOR_REVIEW，见§14。不自动修改Skill。

## 12. 0.2 事前修订：巡线期间过滤低优先级目标

源码推演发现：核心比后排英雄先进入视野，原持续攻击不重选，8秒后回到前一航点又看到核心，可能重复中断原路线。新增RouteEngagementMinPriority（-100..100，默认-100）作为巡线期间候选优先级下限；有未完成航点时过滤低于门槛的候选，但仍可保存可见记忆。刺客设10，只响应已配置英雄；路线完成后取消此过滤，核心成为fallback。旧Profile默认-100不改变任何合法优先级候选。新增测试覆盖巡线时低优先级过滤、完点后核心回退与旧缺省行为。其它0.1范围/数值/验证矩阵不变，未增加网络载荷/计时器。修改对应行为前重新执行plan/build门。

## 13. 已执行证据与审查修正（持续回写）

- red.log：真实Wave4刺客数量0，期望2，断言失败后才实现。
- editor-build.log首轮因新PIE探针局部PC遮蔽导致C4456，移除重复声明；editor-build-v02.log成功。
- asset-migration.log首轮断言误写Wave4金币200，原资产实际190；未改奖励，修正Spec与断言。金币190/经验500保持。
- 新Profile初填不存在的defense_hero，核对真实DA_DefenseHero身份为defense_ranger后修复。生成/回读脚本改为从真实英雄资产取得身份；旧PIE/网络在运行中中止，不计通过，见aborted-runs.json。
- targeted-tests.log：13项AI角色/刺客专项通过；后续补不等MaxHealth比例比较、明确交战期限、资产真实英雄ID与奖励断言。
- assassin-pie-final.log：ProductionWave4，2刺客移动且自然普攻命中英雄；截图assassin-pie.png已目视核对紫色尖锥、游猎刺客320/320与英雄受击。前3波测试清场不代表正常构筑通关。
- network-assassin-victory-2/report.json：服务器及双客户端Pass。4刺客均自然命中英雄，4攻城均自然命中核心，客户端均观察4刺客/4攻城移动且不运行Brain；六波胜利核心740，奖励/复活一致。
- asset-validation.json：131定义0错误0警告；cold-readback.json原生冷读；idempotence-result.json重复生成6资产字节相同。
- cook.log：775总包，768cook/7平台跳过，0错误/1条既有MCP授权提示。
- server-build.log成功；Client和完整回归结果待最终写入，不能把当前中间状态视为交付。

## 14. 最终交付、自评与 Reflect

F0 GO、F1 0.2 APPROVED、F2 PASS、Push-Ready READY；仅DEF-010已获用户验收，本次DEF-011等待用户验收，均未提交/推送。实际命令与日志在§7/13，最终汇总verification-summary.json。

- 最终完整非PIE230/230成功，含新7项。强化后的比例测试用6000/20000对5000/10000，确认绝对血量更多但比例更低者被真实Attack追击；隐藏后的生命变化不更新记忆。明确的交战期限测试证实旧攻击取消、归位独占、旧移动回调拒绝及Destroy清理。
- 四项真实PIE通过：新刺客、旧导航、攻城、商店按钮；同进程仅运行PIE，不与非PIE混跑（GAP-027）。不等同于无测试清场辅助的玩家构筑通关。
- Server/Client已构建最终生产代码；随后仅加强测试断言，Editor最终重新编译并运行完整测试，没有在网络/PIE/Cook后再改变生产逻辑或资产。临时BuildConfiguration已逐字节恢复。
- F2反向审查的4项发现全部关闭，保留首轮编译错误、金币误写、错误英雄身份和中止运行证据；未降低自然命中/复制断言。刺客4个新资产及第4/5波使用LFS，生成报告均在忽略的Saved目录。
- 剩余边界：灰盒外形与初始平衡；英雄优先级目前引用真实defense_ranger，新增第二英雄时需同步配置。Phase3远程兵、第二英雄、事件/装备、阶段Boss和统计仍后续。DEF-009已记录原型不足与GAP-030未在本步解决。

| 自评维度 | 权重 | 分数 | 证据与扣分原因 |
| --- | ---: | ---: | --- |
| 需求与AC | 20% | 5 | 真实编成、低生命比例、路线/后备核心与号角闭合 |
| 架构与权限 | 20% | 5 | 公共Targeting/ASC/Order，角色独占写入、冻结与清理 |
| 实现与数据 | 20% | 4 | 可辨认灰盒及完整定义，正式美术/数值调优未做 |
| 验证证据 | 20% | 4.5 | 三Target、230/4项、双客户端/冷读/Cook；非四人和发布部署 |
| 文档与可观测性 | 10% | 5 | ADR077、中文字段、真实命中/移动计数、完整失败与修复证据 |
| 交付卫生 | 10% | 5 | 保留既有差异、LFS、构建配置恢复、不自动提交 |

加权4.7/5，EVIDENCE_SUFFICIENT，READY_FOR_REVIEW。Reflect：资源文件名不能替代稳定定义身份，奖励应回读原配置；生成器和回归已直接读取真实英雄资产，数值/比例与期限断言补强。属于本次实现与测试覆盖问题，未扩展修改通用Skill。无需用户额外决定实现细节。

最终交付门：docs-final.json为103篇Markdown/558本地链接零错误；delivery-final.json为39个工作区变更、零错误（包含保留的DEF-010）；git diff --check通过。editor-restored.json经8055端口MCP确认L_Defense、无PIE、刺客Blueprint无脏修改、All Saved。用户可在当前编辑器Alt+P，点击“锁定种子并开始”，第4/5波验收刺客。

2026-10-17：用户确认“验收完毕，继续吧”，DEF-011已验收；继续DEF-012神殿守卫，未要求新提交。
