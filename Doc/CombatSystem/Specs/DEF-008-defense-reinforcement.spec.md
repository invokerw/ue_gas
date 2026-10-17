# DEF-008 援军印记与复活保护

> Spec 版本：`0.1`
> 状态：`已验收`
> Owner：Codex
> 创建日期：2026-10-16
> 关联进度台账：`00-01-Progress-Tracker.md` §12.13
> 风险等级：`L1`

## 0. Intake 与 Gate 记录

- 用户请求：提交当前版本，并进行下一步。已将DEF-003/004/006/007有效实现提交为 ff418a2，未推送。
- 附件解释：无新附件；AGENTS.md/agent.md是工程约束，设计20-05为需求依据。
- 已读取入口：agent.md全文、README、00-01/03/04/05、10-01/03/05/06/12、20-02/03/05、90-16；Skills/combat-task-router/SKILL.md、Skills/combat-feature-development/SKILL.md、Skills/combat-skill-development/SKILL.md；Director/TeamShop/Shield/Lifecycle/Scheduler/View及测试。无.codegraph索引。
- 主 Skill：`Skills/combat-skill-development/SKILL.md`
- 备选 Skill 与排除理由：功能开发作为通用流程，护盾与生命周期组合使用技能专项。
- 路由置信度：`high`
- F0 结论：`GO`
- F0 依据：已有获得即用、公开死亡/复活委托、有限护盾与Scheduler，新增模式内容可复用现有入口，无需新商店或公共结算旁路。
- F1 结论：`APPROVED`
- F1 审查人：Codex（L1本地审查）
- F1 审查版本：`0.1`
- F1 计划审查证据：preflight/plan均0错误；复核死亡先计次、购买前已死排除、DeathLife+1待领、4秒公共Scheduler、先取凭证后Apply、委托解绑和回调后重查；目录单项替换和旧核心类身份保留，三端/分进程/两种网络矩阵及回滚明确，批准0.1。
- Build 解锁：已解锁；批准0.1后运行build Gate，证据Saved/DEF-008/build-gate.json
- F1 重审条件：范围、架构、权限、迁移、测试矩阵或回滚变化时先递增Spec版本并重审。
- F2 结论：`PASS`
- Push-Ready 结论：`READY`
- 验证：三Target成功；215非PIE+7渲染PIE共222独立用例；98资产、738包Cook、独立冷读、两组双客户端六波通过，详见Saved/DEF-008/verification-summary.json。
- 未执行：4人/网络损伤/长期平衡与打包exe部署不属本步；其它三件正式道具留后续。GAP-030/031保留，未改变公共边界；无本步必需验证缺口。

## 1. 目标与范围

### 目标
原商店新增正式援军印记，消费后提供两次团队复活救援，实际死亡/复活、短盾和共享次数都可验证。

### 范围
- CombatItem:team_reinforcement_mark，200金币，每局全队一次，获得即用不占库存；替换目录team_fortify，旧资产保留，仍六件商品。
- 购买后接下来两次已登记英雄死亡，按死亡委托实际顺序各消耗一次；复活等待max(0,原等待−3秒)，默认8→5秒，由既有Monitor执行安全复活。
- 获得救援的该生命复活后，按复活时MaxHealth的20%生成护盾，4秒由公共Modifier Scheduler到期，耗尽/再次死亡也移除。两次可以来自同一个英雄；第三次死亡恢复普通等待且无保护。
- 购买前已Dead/Dying的生命不触发，包含UnitDeath日志回调里购买发生于OnDied之前的边界；已发生死亡不追溯。援军未使用次数跨波保留，终局取消。
- 买家离开不取消队友未使用次数；已消耗后断线不退次数，取消该英雄待领/短盾。核心失败、Shutdown、Owner/World teardown清理委托、待领与精确句柄。
- 原ShopOffer.Status显示剩余援军/待复活/护盾保护人数；原ModifierView显示英雄4秒Buff。不新增RPC、经济或RunView字段/schema。

### Non-Goals
其余正式道具、英雄切换/重连、战斗中购买、额外伤害公式、世界护盾特效或公共跨World内核修复不在本步。

## 2. 当前事实与依据

- 设计20-05 §6.3定义200金币、两次死亡、缩短3秒、20%最大生命4秒护盾。
- DEF-007提供核心护盾Hook，默认8秒复活由Director.ServicePlayers低频检测；Lifecycle已提供OnDied/OnRespawned与LifeGeneration。
- 原商店Status包含在复制/equality与悬停提示；UnitView已有Modifier剩余时间，无需新表现协议。
- 前序工作已提交ff418a2，开工工作区干净。GAP-030/031保持独立边界。

## 3. 行为与契约

### 主流程与状态转换
购买提交团队账本RemainingReinforcements=2，并快照当前已死生命排除表。RegisterPlayer绑定既有死亡/复活委托；OnDied先锁DeadLife与默认复活截止，再为合法新死亡预留救援、递减次数并缩短截止。重复委托/旧生命不重复预留。

OnRespawned与ServicePlayers共用幂等待领入口，只接受预留DeathLife+1的当前Alive英雄。应用前取走待领凭证，公开ApplyModifier成功后登记精确Hero+Handle；Hook内暂不能同步施加时保留待领，由下一Monitor重试。效果余量复用护盾实现，4秒由Modifier.Duration配置与Scheduler执行。死亡计数和护盾生命周期分别记账，次数耗尽且无待领/活跃盾时标记效果结束。

### 数据约束
UDefenseTeamItemEffect兼容新增ReinforcementShieldData、ReinforcementDeaths、RespawnReduction、RespawnShieldFraction；默认空/0关闭，四字段必须完整且与其它效果互斥，DurationWaves=0。次数1–8、缩短(0,60]秒、比例(0,1]、护盾Duration(0,30]秒，拒绝非有限。护盾定义单层/不可驱散/死亡移除/无属性标签Think或自定义参数，只允许本步复用Runtime。目录只允许一件援军类型，定义ID与其它效果独立，开局深拷贝。

### 权威与生命周期
所有次数、等待和护盾在服务器。复活走原Lifecycle与安全点，不直接设置Health或Transform；无新Actor Timer。提取UDefenseShieldRuntime为共享吸收实现，现有UDefenseCoreShieldRuntime保留原类身份并继承该实现，核心旧资产和行为保持兼容。援军盾使用共享类，不复写一套吸收逻辑。

所有委托在RemovePlayer、Finish/Shutdown/EndPlay解绑；外部回调可能删除Players，ServicePlayers改为快照键并在公开调用后重新查找，避免保存失效引用。新回调逐次检查World/Run/Terminal/Hero/LifeGeneration；暂时找不到安全复活点仍沿用原最多10次重试，不二次扣援军次数。断线后剩余团队次数不退、不清他人的盾。默认普通复活截止改为死亡事件时间+8秒（此前Monitor首次发现时间+8秒），安全执行仍在Monitor。

### 失败与兼容
购买验证失败保留金币；重复/争购复用原经济完整回执。短盾Apply失败若当前生命仍合法则终局报ReinforcementEffectFailed，已终局/断线/新死亡则撤销本次精确句柄且不重建账本。旧内容新增字段默认0/空；保持Release、Event、Formula、RNG、Content、经济与RunView版本。新DefinitionId不改旧ID。

## 4. 实施计划

| 文件/资产 | 变更 | blast radius |
| --- | --- | --- |
| DefenseTeamItemEffect | 援军配置与互斥校验 | 模式内容 |
| DefenseCoreShieldRuntime | 提取共享护盾基类，旧核心类保留 | 防守护盾 |
| Director、TeamShop、新DefenseReinforcement.cpp | 死亡计次、复活短盾、委托/待领/句柄/状态清理 | 防守生命周期 |
| CombatDefenseTeamShopTests/PIE/NetworkScenario | 正反例、真实购买、5/5/8秒及复制探针 | 测试 |
| 两个DataAsset、统一目录、setup_defense_reinforcement.py | 原生创建/替换/保存/冷读 | 防守目录 |
| smoke脚本、Spec/设计/ADR/台账/生命周期/README | 新显式验证开关与证据 | 配套 |

## 5. 验收标准（AC）

- [x] AC-01：原商店右键200金币获得即用、六项目录、全队一次、重放不二扣；剩余次数提示正确。
- [x] AC-02：两个新死亡各扣一次，按实际事件顺序；购买前已死、重发/旧生命不扣；第三次恢复8秒。
- [x] AC-03：两次复活各20%当前最大生命盾，抗性后吸收、4秒Scheduler到期、耗尽/再死清理、不重复补发。
- [x] AC-04：买家离开保留团队权益；受益人断线、核心失败、Apply重入、旧生命/GC/World teardown无残留。
- [x] AC-05：Editor/Server/Client、完整分进程回归、资产冷读/校验、真实商店PIE、双客户端5/5/8秒与Buff复制通过。

## 6. Definition of Done

- [x] 实际Red/Green、新失败与生命周期路径有证据。
- [x] 只用公共生命周期/Modifier/Scheduler，中文配置和注释齐全。
- [x] 三Target、分进程Automation/PIE、资产/网络按矩阵完成。
- [x] F2/Push-Ready、docs/diff/delivery门通过，未执行项如实记录。
- [x] 用户已于2026-10-17完成验收并授权本地提交；本步不推送远端。

## 7. 测试矩阵与命令

| 层级 | 用例/命令 | 预期证据 | 结果 |
| --- | --- | --- | --- |
| Gate | task_gate preflight/plan/build/delivery、validate_docs、diff check | Saved/DEF-008 | 全部通过，docs-final/delivery-final.json |
| Automation | 现有207非PIE+8援军用例 | 次数/延迟/旧生命/快照/到期/断线/取消/重入/GC/委托清理 | combat-full.log 215/215 |
| PIE | Combat.AI.PIE + Combat.Defense.PIE | 原商店真实购买与老屏障/选择/导航回归 | pie.log 7/7，截图已目视核对 |
| Build | Editor/Server/Client Development | 三Target成功 | editor-final-build/server-build/client-build.log Succeeded |
| Assets | 原生脚本与冷读/CombatAssetValidation/Windows地图Cook | 新2定义、三蓝图、目录和旧盾兼容 | 98定义0错误/警告，三蓝图BS_UP_TO_DATE；738包Cook |
| Dedicated | run_defense_smoke -Players 2 -Reinforcement -Port 7895；独立-TeamShop -Port 7896 | 连续三次死亡5/5/8秒，两盾各4秒和复制，终局清理 | 两组report.json均三端Pass |

## 8. 风险、回滚与升级

- 风险：同步生命周期回调重入使记录失效；在公共调用后重新按弱键和生命复核，先消费待领后应用，精确清理。
- 回滚：只回滚ff418a2后的本任务diff与目录一项引用；保留旧fortify资产，恢复核心类原实现；不触碰其它工作。
- 升级：新增RPC/共享协议字段、改复活公用契约、其它道具机制需重审。
- 需要人决定的问题：无，200/2/3秒/20%/4秒均来自已授权设计。

## 9. 交付证据

本步实际证据见§11，不复用DEF-007的通过数字。UE MCP已确认L_Defense、无PIE、All Saved、原商店无脏状态；原生新类加载需正常关闭已保存Editor后通过UE Python命令执行，完成后恢复地图。

## 10. 路由、自评与 Reflect

主Skill为combat-skill-development，high；以既有公共入口组合而非复制商店/护盾/复活系统。已完成单人F2审查，自评见§12；未修改Skills。

## 11. 已执行证据与F2

- red.log：初始援军用例真实失败，旧目录校验拒绝新效果。green.log 19/19，扩展 aid-tests.log 24/24。
- f2-red.log：旧生命重新登记后迟到死亡事件错误消费次数（期望2，实际1）。排除表改为保留整局；最终combat-full.log完整非PIE215/215，包含8项援军测试，验证重放/争购、5/5/8等待、购买前阵亡、死亡日志重入、四秒/耗尽/再死、断线、终局、Apply重入、GC和Hook内复活延后领取。
- pie.log：7/7，包括原商店实际几何右键购买援军200金币、两次状态与零库存占用、旧核心屏障/选择/导航及AI场景。reinforcement-shop-pie.png已目视确认原商店“复”商品、购买成功提示和金币50。
- cold-readback.json：正式ID/200/2/3秒/20%/4秒，三蓝图BS_UP_TO_DATE，旧核心Runtime身份保留，分类中文及六项引用正确。asset-validation.json：98资产0错误0警告。
- cook.log：Windows L_Defense总738包（731已cook、7平台跳过），0错误/1既有MCP授权警告。不是打包exe部署。
- network-reinforcement-victory-2/report.json：真实安装版Editor -server + 双-game客户端三端Pass，复活等待5.094/5.114/8.041秒，前两次有盾、第三次无盾，两次四秒过期；两客户端均Two=1/One=1/Zero=1/ClientShields=2/Expired=1，六波胜利与200金币一次消费。
- network-team-shop-victory-2/report.json：独立双客户端旧团队商店回归三端Pass，核心盾/法力补给/限购/到期/24属性对照与六波胜利。
- 单人F2反向审查见Saved/DEF-008/f2-review.json。无新生产Timer、直接生命/位移写入或第二套经济入口。
- docs-pre-delivery.json：100 Markdown、546链接、0错误；Python AST、PowerShell解析、LFS和git diff --check通过。最终三Target均Succeeded；临时构建配置逐字节恢复，编辑器MCP确认已恢复L_Defense、无PIE、商店无脏修改。

## 12. 最终交付、自评与 Reflect

F0 GO、F1 0.1 APPROVED、F2 PASS、Push-Ready READY。前序版本已按用户指令提交ff418a2，未推送；用户于2026-10-17确认DEF-008验收完成并授权本地提交，本次仍不推送远端。适用Tests/Build/NoRegression/Adversarial/DDD/Decisions六层均有本步证据。

| 维度 | 分数 | 证据与扣分 |
| --- | ---: | --- |
| 需求与AC | 5 | 正式200/2次/3秒/20%/4秒、原商店与全队一次闭合 |
| 架构与权限 | 4.5 | 公共Lifecycle/Modifier/Scheduler，精确生命/句柄与清理；公共GAP-030/031仍存在 |
| 实现与数据 | 4.5 | 共享护盾实现、旧核心身份保留、配置冻结；灰盒UI，无世界特效 |
| 验证证据 | 4.5 | 三Target、222用例、两组真实双客户端、资产/Cook；未做四人长期平衡和exe部署 |
| 文档与可观测性 | 5 | ADR-075、生命周期、设计/README、台账、剩余次数与复活/盾日志 |
| 交付卫生 | 5 | 先提交前序版本，新增范围清楚；LFS/语法/diff/文档门与配置恢复 |

加权自评4.7/5。Reflect：购买时的旧生命排除是局内资格凭证，不能随玩家登记项一同删除。真实Red已固化为OldDeathAfterReregister回归；Hook内复活延后交付也有独立反例。问题属于具体生命周期集成，本轮未修改通用Skill。新道具替换原型目录的一项并保留旧资产，回滚无需重命名定义或改协议。

## 13. 用户验收记录

2026-10-17，用户明确回复“验收完成，提交吧”。DEF-008 援军印记与复活保护验收完成，授权本地提交；未授权推送远端。
