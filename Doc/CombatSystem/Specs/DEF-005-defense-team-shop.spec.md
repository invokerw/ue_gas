# DEF-005 团队即时消耗品商店

> Spec 版本：`0.2`
> 状态：`READY_FOR_REVIEW`
> Owner：Codex
> 创建日期：2026-10-16
> 关联进度台账：`00-01-Progress-Tracker.md` §12.13
> 风险等级：`L1`

## 0. Intake 与 Gate 记录

- 用户请求：在 DEF-004 强化选择完成后继续下一步。
- 附件解释：无新附件；20-05 §6.3/§7.1 的团队即时消耗品是需求，agent.md 与现有 Economy/Modifier 约束是工程边界。
- 已读取入口：agent.md、README、00-01/03/04/05、10-01/08/09/12/15、20-05 §6.3/§7.1/§7.4/§9、90-16、DEF-002/003/004、CombatEconomy/Network/Modifier/Director/HUD 代码。
- 主 Skill：`Skills/combat-feature-development/SKILL.md`
- 备选 Skill 与排除理由：combat-skill-development 不作为主 Skill；六件消耗品的效果组合既有 Modifier/属性入口，不新建玩家技能。
- 路由置信度：`high`
- F0 结论：`GO`
- F0 依据：Phase 2 下一项是团队即时消耗品；已有 PlayerController 金币、NetworkSecurity、Modifier、Scheduler 和 Defense Director 可复用。
- F1 结论：`APPROVED`
- F1 审查人：Codex（L1 本地计划审查）
- F1 审查版本：`0.2`
- F1 计划审查证据：0.1 已建立初始流程；0.2 修改前先计划检查，再按 §11 审查独立原型资产/schema 2、完整请求回执、购买互斥、精确 Hero+Handle、下一波清场到期、死亡补领、私有结果刷新、既有系统兼容、测试矩阵与回滚，Codex 本地审查 APPROVED，Build Gate 通过后实施。
- Build 解锁：0.2 PLAN 通过；Codex 本地复核 §11 六件独立原型、schema/迁移、权限、缓存、过期、死亡、UI 和网络测试后 APPROVED，继续前运行 Build Gate。
- F1 重审条件：加入事件/RiskLedger、改变团队资源所有权、公开网络 schema、换英雄或发布契约时升级 Spec。
- F2 结论：`PASS`
- Push-Ready 结论：`READY`
- 验证：最终 Editor/Server/Client 构建通过；10 项专项纳入 193 非 PIE 回归，4 项 AI/导航 PIE + 2 项真实渲染按钮，共 199 项独立测试；87 资产、冷回读、双客户端六波争购/到期/属性对照与 727 包 Windows cook 均通过。
- 未执行：4 人容量/长期经济平衡、网络损伤、打包 exe 部署及 §6.3 正式机制内容不在本步；无本步必需验证遗漏。最终网络复验三端通过。

## 1. 目标与范围

### 目标

在 Preparation、Intermission 和 BossPreparation 开放团队商店；任一仍连接玩家用个人金币购买一次性团队消耗品。服务器以 RunId + ShopRevision + RequestId + DefinitionId 幂等扣款并立即应用效果，所有客户端显示相同剩余次数、购买者和到期波次。

### 范围

- `UDefenseTeamShopData` / `FDefenseTeamShopItem`：六件原型消耗品、价格、开放阶段、每局一次、效果 Modifier/属性/团队状态。
- Director 的 TeamRunState/TeamPurchaseLedger/ShopRevision、购买/重发/失败结果、效果代次/到期波次和 World teardown。
- 复用 `TrySpendGold` 与 NetworkSecurity 组件入口；购买不进入英雄库存，不可出售或交易。
- 服务器权威、Controller 拥有者复制的 TeamShopView，原生防守 HUD 商店区；不改变旧 Combat Shop/Inventory Widget。
- 自动化覆盖限购/重复、价格/修订、阶段拒绝、断线不撤销、终局/到期、旧 RunId/跨 World/客户端请求。

### Non-Goals

事件/RiskLedger、个人装备商店改造、换英雄、§6.3 正式护盾/条件增伤/援军机制及六件最终平衡、4 人容量和经济长期平衡、客户端预测、生产发布契约不在本步。

## 2. 当前事实与依据

- 已有 `UCombatEconomyComponent::TrySpendGold`、`UCombatNetworkSecuritySubsystem` 和 `UCombatModifierComponent` 可复用；不新增第二套玩家资源。
- DEF-002 Director 负责阶段/终局/teardown，DEF-004 已有私有 Choice/Builder UI；团队商店显示放在 Defense 专用面板。
- 团队效果由服务器保存目录 PrimaryAssetId + ItemId、精确英雄/句柄和到期波次，客户端只读。

## 3. 行为与契约

### 主流程与状态

Director 开局冻结商店目录，ShopRevision 从 1 开始。开放阶段发布 View；WaveCombat 关闭购买。购买成功先校验当前阶段、DefinitionId、剩余次数、RequestId/修订、付款人玩家/金币修订，再扣款并应用团队效果，递增 ShopRevision/账本，广播统一结果。每项每局一次，不存在重新激活代次；精确 Hero + Modifier Handle（含 generation）和 1-based ExpireWave 保存归属。既有 Scheduler 的阶段转换在最后覆盖波清场时移除效果。终局撤销属性并保留购买记录，Shutdown 才清账本。

### 权威/权限

服务器持有目录、价格、次数、购买者、效果和到期；客户端只提交意图。每种定义每局最多一次；同 RequestId 同完整载荷返回原结果，换 DefinitionId/RunId/ShopRevision 的重放拒绝。购买者掉线不撤销已生效团队效果；团队继续运行。

### 失败与生命周期

未开放、无余额/修订、无效定义、客户端/跨 World、旧阶段、终局和购买中重入均无扣款/效果。World teardown 取消 Director 的既有 Scheduler、撤销精确团队句柄并清空账本；不修改个人库存、个人强化或金币以外的旧 Economy 语义。

## 4. 实施计划

| 文件 | 变更 | blast radius |
| --- | --- | --- |
| `Source/Combat/Combat/Defense/DefenseTeamShopData.*` | 新稳定定义/六件原型目录/校验 | Defense 新类型 |
| `Source/Combat/Combat/Defense/DefenseTeamShop.*` | Ledger/View/结果和团队效果运行时 | Defense Director |
| `DefenseDirectorComponent.*` | 冻结目录、ShopRevision、购买/到期/清理 | Defense only |
| `DefenseTeamShopComponent.*`/`DefensePlayerController.*`/`DefenseBuildWidget.*` | owner-only TeamShopView、RPC 与原生 Slate 商店 | Defense UI |
| `CombatEconomyComponent.*`/`NetworkSecuritySubsystem.*` | 复用/补充安全扣款公共入口 | Economy/Network |
| `CombatDefenseTeamShopTests.cpp`/NetworkScenario | 限购、幂等、阶段、到期、teardown | Automation |
| `Config/DefaultGame.ini`、`Tools/setup_defense_shop.py` | 新类型扫描/六件资产/蓝图保存回读 | Defense assets |
| Spec/设计/台账/ADR | 事实和证据同步 | 文档 |

## 5. 验收标准（AC）

- [x] AC-01：六件定义稳定、目录校验通过，资产编译保存冷回读。
- [x] AC-02：开放阶段购买成功立即生效，WaveCombat/终局拒绝；个人金币扣款正确。
- [x] AC-03：每种每局一次，重复/改载荷/旧修订/旧 RunId/跨 World/客户端安全失败，不二扣/二效。
- [x] AC-04：全客户端只读视图一致；购买者掉线效果继续，到期/终局清理 exactly-once。
- [x] AC-05：Editor/Server/Client、相关 Automation、PIE/双客户端 smoke、资产校验和文档 Gate 通过。

## 6. Definition of Done

- [x] F0/F1/Build/Delivery Gate 和文档回写完成。
- [x] TeamShop 专项自动化覆盖成功/拒绝/重复/到期/teardown。
- [x] Editor/Server/Client、资产校验、PIE/Dedicated 按适用范围完成。
- [x] `git diff --check` 通过，不混入生成文件。

## 7. 测试矩阵与命令

| 层级 | 用例/命令 | 证据 |
| --- | --- | --- |
| Gate | preflight/plan/build/delivery | 0.1/0.2 计划门通过；最终交付见 delivery-final.json |
| Pure/World | `Combat.Defense.TeamShop.*` | green-final2.log，10/10 |
| 回归 | 完整非 PIE Combat 名称清单 + 6 项 PIE | combat-final.log 193/193；pie-final.log 4 个 AI/导航通过；pie-rendered-final.log 两个按钮通过 |
| Assets | `CombatAssetValidation`、UE Python 冷回读 | 87 资产 / 0 错误 / 0 警告；cold-read-final.log 与 cold-readback.json |
| PIE/Dedicated | 真实商店按钮、双客户端购买/同步/到期 | pie-rendered-final.log；run_defense_smoke.ps1 -TeamShop -Players 2；network-victory-2 三端记录 |

## 8. 风险、回滚与升级

- 风险：团队效果与个人金币公平性未平衡；六件原型效果只用于流程和可读性验证。
- 回滚：删除 TeamShop 文件/资产/扫描/Director 增量，DEF-004 个人强化流程和 DEF-002 固定波次保留。
- 升级：事件/RiskLedger、团队资源或公开 View schema 改变时单独升级。
- 需要人决定的问题：暂无阻塞；价格/持续时间是原型调优值。

## 9. 交付证据

- 代码/资产 diff：新增 TeamShopData/TeamShopComponent/DefenseTeamShop 事务与测试、原生商店面板，六件目录/五个独立效果，通过 RunData 引用和 AssetManager 扫描；复用金币、公共 Modifier/Heal、连接预算。保留 DEF-003/004 未提交基底。
- 构建：`Saved/DEF-005/editor-final-build2.log`、`server-validated-build.log`、`client-validated-build.log` 均 Succeeded；安装版 Editor 与环境中源码 Server/Client 分别执行。
- 全套证据见 §11；未执行边界见顶部，无提交推送。

## 10. 交付自评与 Reflect

- 自评：见下方六维证据评分。
- Reflect：原来的三个正例不足以发现共享效果、重入和逐波时序；先扩大反例再修复，使限购记录与属性生命周期分离。问题属于本功能实现和测试夹具，未修改通用 Skill。
- 用户验收状态：`READY_FOR_REVIEW`，尚未用户验收。

## 11. 0.2 收敛计划（2026-10-16）

0.1 的 Editor/Server/Client 构建、30 项防守回归、PIE 商店显示已执行；这不是最终验证。F2 发现占位商品共享效果、阶段不触发 UI 刷新、目录未被校验器扫描、临时效果过期边界及回执测试覆盖不足。按以下范围重新审查后继续，不追认旧实现完整。

- 本步为 Phase 2 团队商店事务与表现原型。保留六个原型商品键：team_guard（120 金，全队护甲 +5，一波）、team_repair（150 金，核心修复 750）、team_rally（180 金，攻击 +15，一波）、team_supply（200 金，回蓝 +2，两波）、team_fortify（240 金，魔抗 +0.12，两波）、team_last_stand（300 金，攻速 +20，Boss 整备限定一波）。每项单独 Modifier，修复无 Modifier；数值由目录/独立效果资产提供。
- §6.3 最终护盾、条件增伤和援军机制属于 Phase 3 内容，本步不冒用正式 DefinitionId，不把六个原型效果写成最终六件实现。新增目录 schema 2；原 0.1 未发布临时资产通过 UE API 原地升级。
- 目录 DefinitionId 为 DefenseTeamShop:...，ItemId 是该冻结目录内的稳定项键；RunId + ShopRevision + 项键限定请求。快照包含 RunId、独立 UI Revision、逐项可购/剩余次数/购买者/生效状态和到期波数；最近个人错误不会被周期发布覆盖。
- 按下一波计算持续期：Preparation 购买一波效果到第 1 波清场撤销；第 N 波后购买一波效果到第 N+1 波清场撤销。过期在 Scheduler 阶段转换内处理，战斗期间不撤销；终局撤销但保留已购账本，Shutdown 才清账本。
- 冻结目录和全部效果并强引用；只允许无 Hook/标签的非驱散属性效果，与个人卡 DefinitionId 不重叠；死亡购买/准备期迟加入在复活/登记后补到当前未过期效果。购买者掉线不撤销其他成员效果。
- RPC 共用连接预算，完整载荷有界回执 32 条并有高水位；身份/终局在重放前校验，阶段/经济陈旧拒绝不扣款。购买重入拒绝；失败应用撤销精确 Hero+Handle 并退款，不记成功账本。使用独立 View 修订跟踪失败提示、开放状态和余额，不更换种子文本框。
- 扩充真实测试：同载荷重发/换载荷、两个玩家竞争、跨 World/旧 Run/旧绑定、32 条缓存、死亡补领、精确到期不误删个人卡、购买者断线、终局/Owner/World teardown、非法效果及冻结目录。资产校验扫描新类型并要求实际六件有效定义；PIE 点击和双客户端购买/同步/过期作为必需证据。
- 保留 DEF-004 GAP-030 动态 GE 复制限制，网络校验用实际属性和只读视图；不扩大内核与发布契约。

### 收敛验证事实

- Red：`Saved/DEF-005/red.log` 七项中四项失败，实际检出重入二扣、非法定义放行、死亡漏领和到期/终局错误；不是以夹具启动失败替代行为 Red。
- Green：`green-final2.log` 10/10；增加跨组件同编号 Handle 隔离、Apply 回调内 Shutdown 回滚、32 回执缓存、GC 保活和 RPC 连接预算。`green-final.log` 曾因测试直接对未 BeginPlay 的组件调用 EndPlay 触发断言，已修正测试生命周期入口，保留失败日志。
- 非 PIE 回归：`combat-final.log` 193/193，通过上一版 183 项加 10 项本次测试的完整名称清单 `nonpie-test-list.txt` 执行。
- 真实渲染 PIE：`pie-shop.log` 的 TeamShopButtons 通过，截图 `team-shop-pie.png`；验证 6 按钮、实际扣款/Armor、种子输入不丢失、Boss 专项按钮禁用和战斗关店。
- 网络：`network-victory-2/report.json` 及三端日志通过。两连接争购团队铁甲和法力补给并各重发相同载荷；服务器恰好两次购买，总金币为 `2*1370-320`；两端观察到两种效果的生效/到期、战斗请求拒绝、六波胜利与两英雄 × 12 属性最终一致。
- 资产：新目录+5 独立 Modifier 通过 UE Python 创建/迁移，两个蓝图编译保存；`asset-validation-final.json` 87 项 / 0 Error / 0 Warning（旧报告 81 项漏扫新类型，不能作为本次最终证据）。`cold-readback.json` 回读 6 商品、5 独立效果及 Run 引用。
- NullRHI 的混合 PIE 批次中两个依赖实际 Slate 几何的按钮探针需要渲染环境，按实际渲染进程复测；AI 与导航测试保持原标准，不删减用例。


### F2 本地审查与最终证据边界

按 AC 重新核对最终源码和反例，不依赖“编译通过”的判断，未使用子 Agent：目录及效果深拷贝/UPROPERTY GC 保活；pure Additive 白名单；独立效果身份；RunId/绑定/所有权/阶段/余额/预算；购买与个人选择互斥；32 条完整回执和高水位；应用回调中 Shutdown 的退款/精确撤销；死亡补领；买家离开；终局保留记录；UI 逐项禁用与种子输入保留均已核验。暂未发现本步未关闭的高风险问题。

- 最终构建是源码 Server/Client Target 与安装版 Editor 的实际 Succeeded 日志；独立网络用同安装版 Editor 的 `-server` + 两个 `-game` 进程，NetMode=DedicatedServer/Client。不把这一证据写成已运行打包 Server/Client exe。
- `verification-summary.json` 合并独立测试名得到 199/199。NullRHI 中两个按钮探针超时日志原样保留，以实际渲染的 `pie-rendered-final.log` 验证；未删测试或放宽断言。先前冷回读并行运行发生 MCP 8000 端口占用，已在单进程 `cold-read-final.log` 重跑，0 错误，只有既有 MCP 许可提示。
- Windows cook：`cook-final.log` 727 总包，720 cooked、7 platform-skipped，0 error / 1 MCP 许可 warning。未部署或运行打包 exe。
- 网络日志仍含编辑器工具插件在 `-game` 模式下缺失 AgentSkill 的启动诊断；玩法探针完整通过。保留既有 GAP-030 动态 GE 定义复制限制，检查的是服务器效果和客户端 AttributeSet/只读视图，不声称 ActiveGE 对象复制完整。
- 卫生：git diff --check、文档校验、PowerShell 解析、Python 语法与 LFS 属性检查；所有新运行证据在忽略的 Saved 目录；不提交、不推送。

### 六维自评

| 维度 | 权重 | 分数 | 依据与扣分 |
| --- | ---: | ---: | --- |
| 需求与 AC | 20% | 4.3 | 六件原型事务闭环；正式机制仍需后续内容阶段 |
| 架构与权限 | 20% | 4.4 | 服务器唯一真值、公共 API、完整身份、缓存与 reentry；保留 GAP-030 |
| 实现与数据 | 20% | 4.4 | 六件目录/五个独立效果、GC 冻结、阶段/死亡/终局、可玩 UI |
| 验证证据 | 20% | 4.5 | 三 Target、199 独立测试、87 资产、真实按钮、双客户端、cook；长期与网络损伤未做 |
| 文档与可观测性 | 10% | 4.5 | 中文配置、购买/到期诊断、Spec/ADR/台账和运行入口同步 |
| 交付卫生 | 10% | 4.5 | 原改动保留、二进制由原生 API 操作、证据边界明示、无提交推送 |

加权总分 4.42，显示 **4.4/5**；自评不能替代用户验收。下一步是最终团队机制/敌人内容与事件实现，或按用户验收反馈调整当前原型。


### 最终交付记录（2026-10-16）

最终双客户端复验的 RunId 为 `9B1FF3324F2F513F3DF1DDAC12A0A5A2`。服务器与两个客户端均为 `DefenseSmoke Result=Pass`；两个客户端的 `DefenseTeamShopNetwork` 为 ActiveSeen=1、SupplySeen=1、ExpiredSeen=1、CombatRejected=1、Match=1，服务器为 ServerEffect=1、Match=1。汇总保存在 `Saved/DEF-005/verification-summary.json`。

Push-Ready：Tests PASS（199 独立用例）；Types/Build PASS（三 Target）；No Regression PASS（完整非 PIE + AI/导航 + 两个渲染 UI）；Adversarial PASS（十项专项和最终源码检查）；DDD/Constraints PASS（公共入口、ADR-072、中文配置、生命周期）；Decisions PASS（原型边界与已知 GAP 明示）。97 Markdown/524 本地链接、git diff --check、Python AST、PowerShell 解析和新资产 LFS 属性检查通过。最终 delivery Gate 证据为 `Saved/DEF-005/delivery-final.json`。

未创建新提交或推送；用户只验收了已提交的 Phase 1，本步骤与 DEF-003/004 保持工作区供验收。
