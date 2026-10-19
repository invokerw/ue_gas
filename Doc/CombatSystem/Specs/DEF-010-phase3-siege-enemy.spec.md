# DEF-010 Phase 3A 攻城兵进入真实波次

> Spec 版本：`0.1`
> 状态：`已验收`
> Owner：Codex
> 创建日期：2026-10-17
> 关联进度台账：`00-01-Progress-Tracker.md` §12.13
> 风险等级：`L1`

## 0. Intake 与 Gate 记录

- 用户请求：验收完成，提交并开始Phase3。前序DEF-009已提交bd41b78，未推送。
- 附件解释：无新附件；用户提供AGENTS.md/根agent.md为工程约束，20-05设计为需求。
- 已读取入口：agent.md全文、README、00-01/03/04/05、10-01/09/10、20-03/05、90-16；Skills/combat-task-router/SKILL.md、Skills/combat-feature-development/SKILL.md；DefenseData/Objective/Director/条件增伤/网络和PIE测试。无.codegraph，使用rg读取。
- 主 Skill：`Skills/combat-feature-development/SKILL.md`
- 备选 Skill 与排除理由：本步不新建技能/Modifier，现有条件增伤直接复用，技能Skill不是主执行层。
- 路由置信度：`high`
- F0 结论：`GO`
- F0 依据：防守导演已有按路线Move并Attack核心的完整公共入口，攻城内容只需新单位定义/外形/出生组。没有新增AI控制系统、RPC或权威Tick。
- F1 结论：`APPROVED`
- F1 审查人：Codex（L1本地审查）
- F1 审查版本：`0.1`
- F1 计划审查证据：preflight/plan报告均通过；冻结两波各替换两名、零单体奖励和配置数值；外形类不承载行为，公共Spawn/Move/Attack不改，实际伤害与导航/网络探针覆盖，资产回滚与失败路径明确，批准0.1。
- Build 解锁：已解锁；批准0.1后执行build Gate，Saved/DEF-010/build-gate.json。
- F1 重审条件：新增自主选敌/StateTree、修改奖励或跨系统schema、测试矩阵/回滚改变时先重审版本。
- F2 结论：`PASS`
- Push-Ready 结论：`READY`
- 验证：三Target最终成功；223非PIE通过、3攻城专项最终复验通过、3渲染PIE通过；129定义0错误/警告，冷读/771包Cook，双客户端4名攻城真实攻击核心通过。汇总Saved/DEF-010/verification-summary.json。
- 未执行：刺客/第二英雄/事件/装备/Boss阶段技能、4人容量、长期数值平衡、打包exe部署不属本切片；无本步必需验证缺口。

## 1. 目标与范围

### 目标
L_Defense第3和第5波真实出现可辨认的攻城兵，沿路线慢速推进、攻击核心；追猎号角对真实攻城定义生效。

### 范围
- 新CombatUnit:defense_siege、BP_DefenseSiege和仅外形职责的ADefenseSiegeEnemy子类。橙色短宽车体与前方冲槌，不增加碰撞/导航障碍或Tick。
- 初始配置生命650、移速140 cm/s、物理攻击80、基础间隔3秒、前摇0.65秒、攻击距离220cm、护甲4、无回血/击杀金币/击杀经验；其余沿普通兵合法默认数据。数值在UnitData而非C++。
- 第3波原两路各4人改成各3普通+1攻城，第5波各6人改成各5普通+1攻城。总数仍8/12，奖励仍160/400和240/600；第1/2/4/6波不改。
- 复用Director按路线公共Move→Attack核心、公共CombatAIController/PathFollowing、Attack/Damage/Lifecycle/Scheduler；受到英雄攻击不脱离核心任务。没有其它建筑目标、独立StateTree或新选敌语义。
- 原商店追猎号角下一波对defense_siege真实伤害+15%；用高血量目标前后实际Health delta对照，普通敌人无加成。不更改现有DefinitionId匹配实现。
- 真实PIE推进到第三波并等待攻城移动和自然普攻核心；明确测试前两波清场由测试编排完成，不把它当构筑通关证据。

### Non-Goals
本步不是整个Phase3完成，不修DEF-009的机制卡、冷却40%、铭文早购与HeroSwap事务缺口。刺客/新英雄/完整预算缩放/事件/装备/阶段Boss/世界受击特效均后续独立实施。

## 2. 当前事实与依据

- MCP当前8055端口可用，L_Defense、无PIE/无打开资产；真实第3波两组各4普通兵，第5波各6普通兵，均1.2秒间隔，两路线现有导航有效。
- DefenseDirector::SpawnEnemy已冻结定义，出生安全检查后提交路线Move和Attack核心；DefenseData拒绝非空AIProfile，保持单写入者。
- 条件Runtime目前以Unit DefinitionId片段匹配siege/assassin，defense_siege可直接消费；正式追猎道具价格260，每局一次，下一波到期。
- DEF-009验收的是当前实现；真实机制卡、三构筑正常战斗与HeroSwap失败/休眠等不足已在提交前附注，不以既有通过数字推断。

## 3. 行为与契约

### 主流程与状态转换
原第三/第五波出生计划→普通兵→攻城兵→沿两条原路线→持续攻击核心；死亡走同一EnemyRecord/LifeGeneration清场。敌人若异常销毁仍原流程报错，不能算击杀。

### 输入、输出与权威
新UnitData固定稳定身份、敌队、零单体奖励，三端使用同一蓝图。Class仅纯表现，所有数值取ASC/UnitData，客户端只看ReplicatedMovement/UnitView/Combat事件。新商品/新RPC均无。

### 生命周期与失败
继承现有Unit/Enemy生命周期；生成失败仍原重试上限。死亡/Finish/Shutdown先解绑再清敌人与AIController，旧回调/旧句柄不复活敌人；没有额外Scheduler任务或Timer。单位自然伤害和号角伤害测试必须验证实际AppliedAmount/Health delta与已购账本，不能只看Event.Amount或直接致死宣称通关。

### 兼容与迁移
保持发布契约和schema不变。UE原生创建新UnitData/Blueprint/Material并替换第3/5波引用，旧普通兵定义不变。脚本幂等，保存每个修改资产后独立进程冷读；先确认Editor All Saved再正常关闭释放DLL，完成后恢复L_Defense。新资产LFS。

## 4. 实施计划

| 文件/资产 | 变更 | blast radius |
| --- | --- | --- |
| DefenseObjective.h/.cpp | 新攻城外形类与冲槌组件 | 纯表现、不改普通敌人 |
| DA_DefenseSiege/BP_DefenseSiege/M_DefenseSiege、Wave3/5 | 数值/材质/出生编成 | 内容 |
| Tools/setup_defense_siege.py | 幂等原生创建与只读模式 | 资产维护 |
| Tests/CombatDefenseSiegeTests.cpp | 数据/真实伤害/号角/回滚边界与清理 | 测试 |
| Tests/CombatDefenseSiegePIE.cpp、NetworkScenario | 实际移动、目标核心、自然伤害/双方客户端可见 | 显式测试开关 |
| run_defense_smoke.ps1 | Siege开关与独立报告目录，避免覆盖前序日志 | 测试工具 |
| Spec/设计/README/台账/生命周期 | 已做与未做边界、验证 | 文档 |

## 5. 验收标准（AC）

- [x] AC-01：新攻城身份/数值/外形合法，第三/第五波各两名，两路线、总数/奖励不变，旧波未改。
- [x] AC-02：追猎通过原经济购买后对真实攻城AppliedAmount提高15%，普通目标不加成，到期撤销；无单体重复奖励。
- [x] AC-03：真实PIE攻城沿路线移动、靠近核心并产生公共Attack/Damage，截图可区分；未绕过生产SpawnEnemy。
- [x] AC-04：Dedicated双客户端看见第三/第五波四名攻城，服务器记录自然攻击核心，六波清理和奖励无重复。
- [x] AC-05：三Target、专项/防守回归、冷读/蓝图/资产校验/Cook、文档/diff/交付门通过。

## 6. Definition of Done

- [x] 实际Red/Green，公共入口复用、中文说明完整。
- [x] 源码和原生资产配套，生效值/冷读/截图核对。
- [x] 生命周期/损伤/网络矩阵和F2有证据，无测试旁路进入生产。
- [x] 交付待用户验收，Phase3首项不代表Phase3完成，新工作不自动提交。

## 7. 测试矩阵与命令

| 层级 | 用例/命令 | 预期证据 | 结果 |
| --- | --- | --- | --- |
| Gate | task_gate preflight/plan/build/delivery + validate_docs + diff check | Saved/DEF-010 | preflight/plan/build已通过；最终docs/delivery报告 |
| World | 完整非PIE Combat及攻城最终复验 | 单位、两波、号角15%真实伤害、到期/清理/旧内容 | combat-full.log 223/223；siege-delivery-tests.log 3/3 |
| PIE | Combat.Defense.PIE.SiegeRouteAndCoreAttack及原导航/商店回归 | 实际生产出生移动/伤害与截图 | 3独立用例通过，最终攻城截图见siege-pie-delivery.log与siege-pie.png |
| Build | Editor/Server/Client Development | 三Target Succeeded | editor-delivery-build/server-delivery-build/client-delivery-build.log全部成功 |
| Assets | setup_defense_siege.py只读、CombatAssetValidation、Windows地图Cook | 原生冷读/零错误 | cold-delivery.log、asset-delivery.json 129/0错误、cook-delivery.log 771包 |
| Network | run_defense_smoke -Players 2 -Siege -Port 7910 | 四攻城自然攻击/两客户端位置投影与六波奖励 | network-siege-victory-2/report.json三端Pass |

## 8. 风险、回滚与升级

- 风险：移速较慢使旧网络探针在攻城到达前杀完本波；测试只在攻城已攻击核心后允许清场，不修改生产波次速度或超时。
- 风险：新外形碰撞不能扩大阻塞；模型NoCollision，胶囊继承已有配置并实测导航。
- 回滚：只还原bd41b78后的本任务diff与Wave3/5两组引用，旧资产保留，不reset其它文件。
- 升级条件：需要自主选敌/修改公共路径/新RPC则先升Spec重审。
- 需要人决定的问题：无，先交付设计已明确的攻城职责与可逆调优参数。

## 9. 交付证据

本步证据在Saved/DEF-010/verification-summary.json；日志、JSON与截图来源均为本步执行，详见§11–12。

## 10. 路由、自评与 Reflect

主Skill为combat-feature-development，high；本地单人F2审查完成，未启动子agent或修改Skills，评分和结论见§12。

## 11. 实施与证据

- 原有交付已按授权提交bd41b78；本步工作独立且未提交。
- red.log：AuthoredWaveComposition读取生产Wave3/5时失败，均0名攻城（期望各2）；不是以缺少C++编译当作行为Red。
- asset-migration.json/cold-readback.json：新defense_siege、650/140/80/3/220/4配置，BP_DefenseSiege为BS_UP_TO_DATE，两路线出生总量/奖励保持原值。
- siege-green.log 3/3；combat-full.log完整非PIE223/223。号角实际生命差验证20→23，非攻城不加成，到期恢复20，击杀无额外金币经验。
- asset-validation.json 129定义0错误0警告；cook.log共771包（764cook/7平台跳过），0错误/1既有MCP授权警告。
- network-siege-victory-2/report.json服务器与2客户端Pass；服务器Spawned=4/Moved=4/CoreAttackers=4，两客户端Spawned=4/Moved=4，六波终局核心1800生命。测试等自然攻城伤害后才以公共Damage清场，未宣称正常玩家构筑通关或难度平衡。
- siege-pie.log旧导航/商店2项成功，新攻城项在首波因Editor导航尚未就绪复制到PIE而SpawnFailed。已补编辑器导航前置等待，复验另存siege-pie-final.log；不得把此首轮失败覆盖为通过。
- F2反向审查见f2-review.json；无新生产计时、伤害/移动入口或奖励真值。

## 12. 最终交付与 Reflect

F0 GO、F1 0.1 APPROVED、F2 PASS、Push-Ready READY。DEF-009已按用户验收提交bd41b78，未推送；DEF-010新增攻城切片待验收、未提交，不代表Phase3完成。

- 最终视觉复验：siege-pie-delivery.log成功。第一张镜头只拍到英雄，调整测试的本地相机后看到两名橙色冲槌；随后发现新Blueprint未配置头顶组件，已复用原WBP_CombatOverhead，新增冷读断言并确认最终截图显示“攻城冲槌 650/650”。未修改战斗单位位置或放宽移动/核心自然受击断言。
- 三Target最终日志均Succeeded。最后仅补充表现资产/测试，3项攻城专项再次通过；223项非PIE原回归通过证据保留。原导航/商店PIE2项通过，加最终攻城PIE共3项。
- 最终asset-delivery.json 129定义0错误0警告；cold-readback.json包含头顶Widget类；cook-delivery.log 771总包（764已cook/7平台跳过）0错误，1条既有MCP授权警告。idempotence-final.log证明脚本重复执行不改变五个资产字节。
- 网络测试使用安装版Editor实际-server/-game服务器+双客户端连接；4名攻城均自然打到核心，客户端观察4名移动，正常六波胜利、复活和共享奖励通过；不把测试编排的击杀当作真实构筑强度证明。
- F2检查纯表现组件的碰撞/导航边界、资源身份、冻结计划/奖励、精确委托解绑、真实伤害delta、脚本幂等和旧内容回归，见f2-review.json。
- 编辑器已恢复L_Defense，8055端口MCP确认无PIE、新攻城蓝图无脏修改；构建配置逐字节恢复。生成日志/截图/构建文件均留Saved，不进入提交。

| 自评维度 | 分数 | 证据与扣分 |
| --- | ---: | --- |
| 需求与AC | 5 | 两波双路攻城、真实核心攻击和号角15%闭合 |
| 架构与权限 | 5 | 无新权威入口，复用Spawn/Order/Attack/Lifecycle，奖励不重复 |
| 实现与数据 | 4 | 可辨认灰盒外形和血条齐全，动画/美术与长期平衡未完成 |
| 验证证据 | 4.5 | 三Target、223非PIE/3PIE、实际双客户端/冷读/Cook，非四人或发布验证 |
| 文档与可观测性 | 5 | 真实攻击者计数、来源日志、设计/台账/Spec和原型边界 |
| 交付卫生 | 5 | 用户指定前序提交已完成，新工作独立；LFS/diff/文档门、临时配置恢复 |

加权自评4.7/5。Reflect：内容继承要同时核对玩法定义和表现组件，截图只覆盖HUD不足以证明新单位外形/血条可见。本次通过相机对准核心、复用已有头顶Widget及CDO测试固化；属具体资产接入问题，未扩大为通用Skill修改。

2026-10-17 用户确认“验收完毕，继续吧”，DEF-010已验收；历史验证结论保留。此次未要求提交，继续DEF-011。
