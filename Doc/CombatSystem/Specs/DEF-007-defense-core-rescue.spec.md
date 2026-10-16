# DEF-007 正式团队道具第一批：核心屏障与应急修复

> Spec 版本：`0.2`
> 状态：`已验收`
> Owner：Codex
> 创建日期：2026-10-16
> 关联进度台账：`00-01-Progress-Tracker.md` §12.13
> 风险等级：`L1`

## 0. Intake 与 Gate 记录

- 用户请求：继续实现守卫雅典娜设计；沿用用户明确要求的原商店与获得即用方向。
- 附件解释：无附件；用户提供 AGENTS.md 为工程约束，已完整读取根 agent.md。
- 已读取入口：agent.md、README、00-01/03/04/05、10-01/03/05/06/12、20-02/03/05、90-16；Skills/combat-task-router/SKILL.md、Skills/combat-feature-development/SKILL.md、Skills/combat-skill-development/SKILL.md；Director/TeamItem/Modifier/Heal/Shop/测试。
- 主 Skill：`Skills/combat-skill-development/SKILL.md`
- 备选 Skill 与排除理由：功能开发作为通用流程；本步引入有状态护盾 Hook，主用技能专项。
- 路由置信度：`high`
- F0 结论：`GO`
- F0 依据：DEF-006 已完成原商店交付，设计 §6.3 前两件救援道具可复用公开 Modifier/Heal；不扩展公共交易或施法内核。
- F1 结论：`APPROVED`
- F1 审查人：Codex（L1 本地审查）
- F1 审查版本：`0.2`
- F1 计划审查证据：0.2 plan-v02.json通过；基于反伤嵌套实际Red，审查只读IsDeferringOperations查询、全目标提前校验和拒绝而不排队语义；无Hook顺序/权限/网络改变，新增完整非PIE回归与局部回滚，批准0.2。
- Build 解锁：已解锁；0.2计划门通过并批准，随后运行build-v02.json。
- F1 重审条件：范围、架构、权限、迁移、测试矩阵或回滚实质改变时递增版本并重审。
- F2 结论：`PASS`
- Push-Ready 结论：`READY`
- 验证：三Target成功；207非PIE+6渲染PIE共213独立测试；96资产0错误/0警告；双客户端六波、独立冷回读、736包Cook与文档/交付Gate通过，汇总 Saved/DEF-007/verification-summary.json。
- 未执行：打包exe部署、4人/网络损伤/长时间平衡soak，均不属本步；正式其余四件、事件与英雄切换留后续。GAP-030/031保持独立记录，无本步必需验证缺口。

## 1. 目标与范围

### 目标
原商店提供两件有正式设计身份和行为的核心救援道具，购买结果可从顶部核心条看清楚。

### 范围
- 神殿屏障 `CombatItem:team_shrine_barrier`，220 金币，立即按核心购买时 ASC MaxHealth 的 8% 生成全伤害护盾，至下一波清场；每局全队一次，不叠加。准备阶段没有“当前战斗波”，故覆盖下一波；Boss 整备同样覆盖 Boss。
- 应急石 `CombatItem:team_emergency_stone`，180 金币，按购买时 MaxHealth 的 15% 请求公共 Heal，受既有治疗管线与生命上限约束；满血仍允许购买并消费。
- 两件替换防守目录中的 team_guard/team_repair，旧资产保留但不在目录，新 DefinitionId 不偷换旧原型身份。其它四件原型不变，目录仍六件。
- UDefenseTeamItemEffect 兼容增加核心护盾定义和百分比修复配置，属性/固定治疗/比例治疗/护盾四种互斥。护盾专用 Runtime 保存余量，公共 DamageBlock 吸收；不更改原 MagicShield 示例或核心公式。
- Director 保存核心精确 Handle，吸收耗尽后共享 offer 标记失效，不为复活英雄补发核心屏障；买家掉线保留屏障，终局/Shutdown 撤销。
- FDefenseRunView 独立 schema 2→3，增加只读 ObjectiveShield；HUD 顶部核心条显示蓝色护盾量。只从服务器 Runtime 投影，不在 UI 维护第二份值。

### Non-Goals
其余四件正式道具、护盾世界材质/特效、普通技能/装备泛化护盾 API、额外商店/RPC、发布契约重构、部署发布、跨局存档均不在本步。

## 2. 当前事实与依据

- 设计 20-05 §6.3 明确屏障 8%、修复15%；商店仅整备阶段开放。当前核心5000生命，默认屏障400、修复请求750。
- DefenseTeamShop.cpp 目前只支持英雄属性及固定核心修复，账本已有精确句柄与回滚；公共 ModifierRuntime 已提供 DamageBlock/deferred remove。
- CombatDemoModifierRuntimes.cpp 的 MagicShield 只吸收魔法，不能改它以免改变旧内容。
- 核心 RunView 为独立模式 schema，不属 FCombatReleaseContract；沿用 ADR-071/072 的独立版本边界，公共发布契约不变。
- DEF-003–006 工作区均保留；本任务不提交，不把用户“继续”当成验收。

## 3. 行为与契约

### 主流程、状态转换
原经济 RPC → 领取锁 → Director 预检 → 核心 ApplyModifier 或 Heal → 成功记每局限购 → 原经济消费。护盾 Apply 时快照 MaxHealth，不随之后上限变化补充。吸收按公共优先级/施加顺序，抗性后削减 Amount 并累加 AbsorbedAmount；归零 deferred remove。0伤害、魔免 blocked 和 HPLoss 不消耗屏障。

### 输入、输出与数据约束
核心护盾定义限定本步 Runtime、无属性/标签/额外参数、无限时长、不可驱散、死亡清理、单层；比例来自 Effect，有限且 (0,1]；持续波数必须1。与英雄属性和其它治疗配置互斥。旧配置默认新字段0/null保持原行为。Director 冻结定义和效果参数，与个人/团队效果 ID 保持独立。

### 权威边界与权限
服务器计算金额及护盾/治疗快照；客户端只发既有购买请求。仍复核同 World、Run/Phase/生命和购买者绑定。获得效果不扣款、不写生命；Runtime 仅在公共伤害 Hook 修改事件。

### 失败、取消、过期、死亡、EndPlay 与重复请求
应用失败、重入终局/Shutdown 时精确撤销盾并回滚交易；已提交治疗沿用 DEF-006 成功消费规则。购买重复缓存、两人争购、旧阶段请求沿用原入口。买家死亡/断线不移除核心效果；英雄复活不补发核心盾。耗尽/波次到期/核心死亡/终局/World teardown 使用公共移除且不复活旧句柄；无新 Timer/委托。

### 兼容、版本与迁移
先记录 ADR-074，Defense RunView schema 3 三端同时重建；公共 Release/Tag/Event/公式/经济 schema 不变。使用 UE Python/原生 API 创建新2物品+1盾定义、替换统一目录2个引用、编译保存相关蓝图并冷回读。旧原型资产保留可回滚。脚本幂等，不覆盖已有普通分类及4件原型。UE MCP先确认实际Editor状态，若需要重载DLL正常关闭已保存Editor，完成后恢复地图。

## 4. 实施计划

| 文件/资产 | 变更 | 原因 / blast radius |
| --- | --- | --- |
| DefenseTeamItemEffect、DefenseCoreShieldRuntime | 核心效果配置与专用护盾Hook | 防守内容，不修改内核 |
| DefenseTeamShop、Director、Types/HUD | 核心句柄、耗尽/到期投影、schema3/核心显示 | 防守模式 |
| 原商店+两件物品+盾DataAsset、Tools/setup_defense_core_rescue.py | 原生迁移、回读 | 防守目录 |
| TeamShopTests/PIE/NetworkScenario、smoke脚本 | 实际购买/吸收/到期/重复/网络证据 | 测试 |
| Spec/设计/ADR/台账/README/生命周期 | 同步当前可玩事实 | 文档 |

## 5. 验收标准（AC）

- [x] AC-01：两件正式商品身份/价格/说明正确，原右键购买、六项目录、无库存占用。
- [x] AC-02：屏障按最大生命快照、吸收三种伤害、HPLoss绕过、耗尽不补充；英雄不获得核心盾。
- [x] AC-03：比例修复不写死750，满血/上限/失败与已提交治疗取消边界正确。
- [x] AC-04：争购/重放/战斗禁购/买家断线/到期/终局/Shutdown 无二扣或残留；旧属性商品回归。
- [x] AC-05：Editor/Server/Client、直接专项、真实商店PIE、双客户端核心护盾投影、资产保存/冷回读/验证和文档门通过。

## 6. Definition of Done

- [x] 记录新增失败测试Red与Green。
- [x] 公共Damage/Heal/Modifier入口与中文字段/注释齐全。
- [x] 原生资产保存回读、蓝图编译、validation与PIE完成。
- [x] 三Target与双客户端证据、生命周期和F2反例完成。
- [x] 文档、台账、diff卫生和交付Gate完成；未验证边界如实记录。

## 7. 测试矩阵与命令

| 层级 | 用例/命令 | 预期证据 | 结果 |
| --- | --- | --- | --- |
| Gate | task_gate preflight/plan/build/delivery；validate_docs；git diff --check | Saved/DEF-007 | 通过；docs-final/delivery-final报告 |
| World | 完整非PIE Combat | 盾三类型/HPLoss/耗尽/快照/比例/断线/到期/终局/回滚/GC/旧柄 | combat-full.log 207/207 |
| Build | ue_gasEditor/Server/Client Development | 三Target Succeeded | editor-final-build/server-build/client-build.log 全部成功 |
| Assets | UE原生脚本、独立冷读、CombatAssetValidation | 新3定义+旧资产无错 | 96定义0错误/警告，三蓝图BS_UP_TO_DATE |
| PIE | 原金币按钮与右键购买屏障、顶部核心盾截图 | 渲染PIE实际几何 | pie.log 6/6；截图已目视核对 |
| Network | run_defense_smoke -Players 2 -TeamShop -Port 7894 | 一次220购买、核心盾快照/吸收/耗尽、旧属性到期、胜利 | network-victory-2/report.json 三端Pass |

## 8. 风险、回滚与升级

- 风险：防守复制字段三端需同版本；GAP-030动态GE定义诊断仍保留，以RunView/ASC复制实际结果验证。
- 回滚：只还原DEF-007局部diff及目录两项引用，保留旧原型资产和前序未提交工作；不整体reset。
- 升级：新增通用属性、正式其余4道具或更改公共结算契约需重审。
- 需要人决定的问题：无，本地可逆实现已由“继续”授权。

## 9. 交付证据

证据统一在 Saved/DEF-007，待实际验证补充，不复用前序通过数字。

## 10. 路由、自评与 Reflect

主Skill为combat-skill-development，high；通用流程由combat-feature-development承接。本步已完成单人F2反向审查，评分见§13；不自动修改Skills，等待用户验收。

## 11. 0.2 同步领取重入边界

首轮7项中5项通过。core-green1.log 真实反伤→嵌套DamageApplied→领取屏障反例显示：购买失败退款后，Modifier延迟队列仍施加一个没有账本的盾（预期1个反伤Runtime，实际2个）。扩大范围仅增加 UCombatModifierComponent 的只读 C++ IsDeferringOperations()，不更改现有 Apply/Remove/Hook 排序或蓝图schema；团队同步领取对核心和所有活英雄在施加前检查，拒绝Hook内提交。重入反例与完整非PIE Combat回归纳入矩阵；公共扩展文档同步这个前置条件。回滚包含这一只读方法与调用者，其他计划不变。

另一个失败来自测试主动把G核心交给F世界的DamageSubsystem，破坏结算归属；不是本功能路径。移除这条不合法的夹具调用，记录独立GAP-031，不扩大本步修改Damage/Heal内核。正常核心死亡继续保留验证。

## 12. F2 与分层证据

- 实际Red：red.log 两项失败，旧实现不吸收纯粹伤害且比例治疗配置被拒绝；core-green1.log 反伤嵌套领取检出延迟队列孤儿屏障（1→2）。
- 修复后 core-green2.log 17/17；最终 combat-full.log 207/207，包含新增8项核心救援用例、原团队/物品/经济/Modifier/伤害/生命周期等完整非PIE回归。
- pie.log 6/6，原商店真实几何右键、220金币扣款、400核心屏障、0库存占用、种子文本保留及战斗禁购；core-rescue-pie.png 已目视核对顶部蓝色护盾数字和既有商店。
- asset-migration.json 与 cold-readback.json 验证新两物品价格/比例/原目录六项、原型不再上架、三蓝图BS_UP_TO_DATE；asset-validation.json 96定义0错误0警告。
- cook.log Windows地图Cook：总736包，729已cook、7平台跳过，0错误/1既有MCP授权警告；这不是打包exe部署。
- 单人F2按Spec逆向检查失败原子性/旧柄/GC/死亡/OwnerEndPlay/Shutdown；除已修反伤重入外补齐核心EndPlay后的可用性检查和Shutdown投影清零，最终8项均绿。记录 Saved/DEF-007/f2-review.json，无新生产Timer、生命直写或第二套购买入口。
- docs-pre-delivery.json：99 Markdown/540本地链接/0错误；Python AST、PowerShell解析、git diff --check、新资产LFS检查通过。
- editor-final-build.log、server-build.log、client-build.log 全部Succeeded。network-victory-2三端Pass，两客户端均Full=1/Partial=1/Depleted=1/Schema=3，最终总金币2×1370−420，六波胜利/复活/24属性对照。测试使用安装版Editor的真实-server/-game连接，另行验证源码Server/Client Target构建；不声称已部署打包exe。

## 13. 最终交付与 Reflect

F0 GO、F1 0.2 APPROVED、F2 PASS、Push-Ready READY；本地待用户验收，未提交/推送。当前实现两件正式道具，不代表六件正式内容或整个Phase2/3完成。

| 自评维度 | 分数 | 证据与扣分 |
| --- | ---: | --- |
| 需求与AC | 5 | 两件道具、原商店、核心反馈与AC全部闭合 |
| 架构与权限 | 4.5 | 公共入口、冻结/句柄/回滚通过；GAP-030与独立跨World API缺口仍保留 |
| 实现与数据 | 4.5 | 原生3资产/六项目录与默认兼容；HUD仍为灰盒表现，无世界特效 |
| 验证证据 | 4.5 | 三Target/213测试/双客户端/96资产/Cook；未做4人长期平衡和exe部署 |
| 文档与可观测性 | 5 | ADR-074、生命周期/设计/README/台账、盾吸收及网络日志 |
| 交付卫生 | 5 | 局部diff复核、LFS/语法/文档门；保留前序工作，临时构建配置逐字节恢复 |

加权自评4.7/5。Reflect：同步交易不能把公共API的“已排队”视为“已生效”；本次真实反伤反例固化为回归，公开扩展文档写清只读时序预检。属于具体集成问题，不调整通用Skill。GAP-031作为独立内核边界登记，不用防守内容任务扩大修复公共事务。

编辑器已正常恢复 `/Game/Combat/Demo/Defense/L_Defense`，MCP确认未运行PIE、商店资产无脏修改。可按Alt+P，在原金币按钮的团队补给中右键购买；默认核心5000生命对应400屏障或750修复。


## 14. 用户确认与提交授权

2026-10-16，用户回复“提交，并进行下一步”。当前集成版本已获确认，授权本地提交DEF-003至DEF-007的有效实现；DEF-005仅保留历史记录。上文“未提交/待验收”为当时交付状态，本记录更新当前状态，不改变历史验证结果。
