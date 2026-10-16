# DEF-004 强化三选一、重抽与生效闭环

> Spec 版本：`0.1`
> 状态：`READY_FOR_REVIEW`
> Owner：Codex
> 创建日期：2026-10-16
> 关联进度台账：`00-01-Progress-Tracker.md` §12.13
> 风险等级：`L1`

## 0. Intake 与 Gate 记录

- 用户请求：继续 DEF-003 之后的正式 UI、RPC、重抽扣款和 Modifier 应用。
- 附件解释：无新附件；原设计 20-05 为需求，AGENTS.md/agent.md 为工程约束。DEF-003 工作区为上一轮未提交实现，保留并在其基础修正；已验收 Phase 1 提交 d7c9809 不重写。
- 已读取入口：agent.md、README、00-01/03/04/05、10-01/08/09/10/12、20-02/03/05、90-16、30-01、任务路由与功能开发 Skill、DEF-003 Spec；核对 Director/RunData/候选/RNG/经济/安全/Controller/HUD/Modifier/测试及资产生成工具。无 .codegraph 索引。
- 主 Skill：`Skills/combat-feature-development/SKILL.md`
- 备选 Skill 与排除理由：combat-skill-development 针对新技能动作；本次组合现有 Modifier 属性，不扩展技能内核。
- 路由置信度：`high`
- F0 结论：`GO`
- F0 依据：用户授权继续，个人选择流程是上一轮明确的下一步。全部变更可回滚，服务器公共入口足够。
- F1 结论：`APPROVED`
- F1 审查人：Codex（用户继续授权内 L1 自审）
- F1 审查版本：`0.1`
- F1 计划审查证据：preflight.json/plan.json 通过，逐项复核可玩范围、私有网络与有界缓存、交易重入、随机键、冻结、死亡/过期/终局、资产隔离、三Target/PIE/网络测试矩阵与回滚；0.1 APPROVED。
- Build 解锁：F1 已批准，执行 Build Gate 后开始行为修改。
- F1 重审条件：加入团队商店、事件、换英雄界面、破坏 Combat 旧网络/公式/发布契约时先升级 Spec 重审。
- F2 结论：`PASS`
- Push-Ready 结论：`READY`
- 最终 Gate：`delivery.json` 通过；用户验收状态仍待用户确认。
- 验证：已完成 183 非 PIE + 5 PIE、81 定义校验、资产冷回读和双客户端选择/重抽/属性对照；三 Target 与最后边界增量均通过，日志见 §9。
- 未执行：4 人容量、长时间浸泡/网络损伤、打包 exe、完整手工游玩平衡未执行；当前 Dedicated 使用安装版独立 -server/-game。GAP-030 的完整 ActiveGE 定义复制仍是已有缺口，属性/私有视图复制已对照。

## 1. 目标与范围

### 目标

打开 L_Defense、进入 Play 后能看到个人三选一卡片，点击后实际获得 Modifier；初始与普通波后有选择，重抽真实扣款、拒绝重复提交，并在多人下只向拥有者显示自己的候选。

### 范围

新增 Defense 专用 PlayerController/ChoiceComponent、私有选择投影与 RPC、原生 UMG/Slate 强化面板。Director 开局冻结池/卡/Modifier，控制选择窗口、候选/已选 ID、共享种子、重抽次数。现有核心/战斗/物品系统继续运行。

初始和第 1–5 波后开窗，默认 15 秒，整备 25 秒；超时默认首张，空池补给 80 金币一次。死亡期间手动操作拒绝，超时选择保存为待生效、复活后落账。两次重抽 60/100 金币，排除当前候选；无其他卡不可重抽且不扣款。

新建 12 张数值原型卡和 Modifier，覆盖普攻/技能/防守方向；说明这些是可玩流程测试内容，不冒充最终 24 张机制卡。持有上限当前固定 1。原型效果只允许有限幅度的 Additive 属性项，无 GrantedTags/自定义 Runtime/周期/资源或伤害治疗 Meta Attribute；避免把内容配置变为选择事务副作用。显示已选卡、家族、效果/待生效、剩余时间、金币与错误原因。选卡不暂停世界。

开发服务器可在开局前配置十进制/0x种子并锁定开始；本地防守模式提供开始/种子面板，客户端只看等待、摘要。Dedicated 自动开始并支持开发命令行种子，Shipping 忽略固定种子/关闭面板。

### Non-Goals

团队即时商店、风险事件、重连、换英雄界面、最终技能改造卡、叠层内容、4 人容量与平衡不在本步；不提交或推送本步。

## 2. 当前事实与依据

- 当前 DEF-003 工作区只有服务器 API，无网络身份、期限或效果生效，结果缓存无界、池未深复制；本步必须修正后再对客户端开放。
- 现有 Modifier Apply/Remove、Economy 账本、NetworkSecurity 连接预算可复用；新增服务器 TrySpendGold（修订/余额/重入检查）避免负 AddGold 或调试入口扣款。
- RNG 文本域是新增候选哈希，不改原 CombatHash64V1 冻结向量。抽取输入为 Seed/席位/波次/修订/槽位，RunId 只验证请求，域计数仅诊断，玩家调用次序不影响结果。
- 核心与英雄状态仍读取 ASC/Lifecycle，UI 不改属性；私有视图只复制 ID/文本/状态，不复制定义 UObject。

## 3. 行为与契约

### 主流程与状态转换

Preparation/Intermission/BossPreparation 开窗 → Pending → Chosen/TimedOut/Cancelled。每窗 Revision 单调增加；重抽在同一期限内替换候选并递增修订，不延长计时。15 秒到期由 Scheduler/Director 结算，下一波开始强制关闭旧窗；Boss 胜利不发新候选。核心失败取消待生效卡。

### 输入、输出与数据约束

新 Choice schema 2：RunId/PhaseGeneration/ChoiceRevision/Wave/Deadline/候选/已选/剩余重抽/状态。RPC 固定载荷：正 RequestId、RunId、Revision、Action、DefinitionId、LifeGeneration、BindingGeneration、EconomyRevision。服务器校验连接组件所有权、当前登记玩家/英雄、窗口/期限/修订、候选、资源；客户端不能传效果/价格/种子。

候选按稳定 ID 排序，支持波次、英雄、前置/双向互斥、已持有过滤；初始只基础，前两波优先至少一基础，终局第 3 波后且同家族至少两张；优先同构筑/不同家族/防守槽。上一轮未选项先排除、不足再放宽；重抽严格排除当前候选，允许不足三张。

### 权威边界与权限

Director 保存唯一构筑真值，ChoiceComponent 只负责 RPC/owner-only 投影和最多 32 条完整载荷回执缓存。缓存命中仍受连接限频；同 ID 换载荷拒绝，缓存淘汰后重放由共享安全窗口/单调请求号拒绝。死亡/控制权改变/跨局请求无副作用。旧 ChooseAugment C++ 入口保留兼容但走同一业务提交，不成为 RPC 旁路。

### 失败、取消、死亡、EndPlay 与重复请求

新效果统一 ApplyModifier，已生效句柄在 Controller 记录中按 ID 持有，永久/死亡保留；死亡待生效队列仅在恢复 Alive 后应用一次。断线和 Shutdown 撤销精确句柄/解绑/清空组件私有视图；终局取消所有选择、清待领，世界退出不留任务或静态 UObject。未知卡/缺资产、无余额/旧经济修订、过期/旧请求不扣费/不加效果。重抽先准备候选，再在重入保护下扣款并替换，失败保持原结果。

### 兼容、版本与迁移

Defense Offer 独立 schema 2，RunView schema 2，Augment 定义/Pool schema 1；首个未发布 DEF-003 Offer 完善为可网络化结构。旧 Combat Tag/Event/Formula/RNG/Release 不变。RunData Pool 为空保留 DEF-002 固定强化；新池启用时关闭固定卡避免双重奖励。Defense PC 蓝图从旧 Demo PC 复制输入配置后重设新父类；旧 Demo PC 不改。公开新 API/版本记 ADR-072。

## 4. 实施计划

| 文件/资产 | 改动 | 原因/影响 |
| --- | --- | --- |
| DefenseAugmentData/DefenseOfferSampler | 严格校验、纯候选采样、私有视图/请求 | 服务器内容/复现 |
| DefenseDirectorComponent + DefenseChoices.cpp | 冻结、开窗、选择/重抽/超时/效果、清理 | Defense 唯一权威 |
| DefenseChoiceComponent、DefensePlayerController | owner-only RPC/回执/投影 | 新 Defense PC |
| DefenseBuildWidget、DefenseHUD | 卡片/重抽/构筑与开发种子开始面板 | 只读 UI + 请求 |
| DefenseGameMode/Data/GameState/Types | 开局锁种子、公开摘要与默认时长 | 新模式 |
| Combat.Build.cs | 添加 ApplicationCore 依赖，仅用于开发种子面板复制按钮 | 本地表现 |
| CombatPlayerController.h | IsPointerOverCombatUI virtual，新 PC 追加实际面板几何检测 | 旧输入行为不变 |
| CombatEconomyComponent / CombatNetworkSecuritySubsystem | 服务器扣款与通用组件意图安全检查 | 公共 API，专项回归 |
| Tools/setup_defense_augments.py + Defense/Augments 新资产/框架蓝图/RunData | 12 原型卡、25 秒整备、新 PC | UE 原生 API 创建/编译/冷回读 |
| Tests/CombatDefenseChoice*、网络/PIE探针及 smoke 工具 | 自动化/真实输入/联机 | 隔离验证 |
| Spec/README/设计/专题/台账/ADR | 当前事实/证据 | 不改历史结论 |

## 5. 验收标准（AC）

- [x] AC-01：Play 后可见开始/强化三选一，点击真实应用公共 Modifier，原玩法仍可玩。
- [x] AC-02：固定种子在加入席位一致时跨请求顺序复现，冻结池/候选过滤/空池行为正确。
- [x] AC-03：每局最多两次 60/100 金币重抽；重复、换载荷、旧局/修订/生命/控制、无余额/替代卡、客户端/跨World被拒绝。
- [x] AC-04：15 秒超时默认、死亡暂存/复活生效、终局/断线/World teardown无重复效果或泄漏。
- [x] AC-05：新定义资产校验/编译保存冷回读、Editor/Server/Client、专项/相关回归、PIE/双客户端 smoke通过。

## 6. Definition of Done

- [x] Red/Green 或补测真实证据，行为与文档一致。
- [x] 中文可读注释/ToolTip、公共入口、旧 Demo 兼容。
- [x] AC 测试和运行证据齐全；未覆盖项注明。
- [x] F2/Push-Ready/delivery/docs/diff通过，无提交推送。

## 7. 测试矩阵与命令

报告在 Saved/DEF-004。先 `task_gate.py --mode preflight/plan/build --spec Doc/CombatSystem/Specs/DEF-004-defense-choice-flow.spec.md --kind feature`。Editor 普通构建用安装版，Server/Client 用环境工具发现的源码版。

- 自动化：严格定义/冻结/RNG隔离、选择生效/重复/无替代重抽/金额修订/错误请求/客户端/死亡过期/旧生命/终局/清理。
- Editor + 相关 Defense/Economy/Network/Foundation/UI/Input 回归；公共契约必要时全 Combat（GAP-027 按已有约定分 PIE/非 PIE进程）。
- UE 资产校验、蓝图编译/冷回读、真实 PIE 点击/界面截屏、双客户端私有候选/选择/重抽/余额/效果/死亡复活 smoke。
- 文档、差异、最终 delivery Gate。
- 本步矩阵结果见 §9；手工长期平衡/4人/网络损伤/打包发布留后续。

## 8. 风险、回滚与升级

- 风险：这是原型卡片流程，非最终 24 张机制内容；UI 与异步身份跨网络要单独实测。
- 回滚：保留 d7c9809 与未提交 DEF-003 原差异；本步修改按新文件和局部 diff 回退，新 Pool 设空恢复固定卡。用户文件不覆盖。
- 升级：新增团队/账号资源、换英雄生产流、破坏旧 Combat 契约时重审。
- 需要人决定的问题：暂无阻塞决定；用户已授权继续具体下一步。

## 9. 交付证据

- 变更：新增个人选择组件、专用 Controller、三选一/重抽/构筑/种子面板；Director 冻结和事务，公共 TrySpendGold/组件预算；12 卡/12 效果/1 卡池与专用 PC 蓝图，目标 RunData 和模式蓝图接线。旧 Demo 默认 Controller 不变，保留未提交 DEF-003 基底。
- Red：`red.log` 真实失败两个断言：选择后护甲仍为 0；同号换卡错误返回成功。`green.log` 修正后通过。后续专项扩展到全部同号/计费/冻结/死亡/过期/终局矩阵。
- 编译：`editor-build-final.log` 安装版 UE 5.8.2 Editor 成功；`server-final-build.log`/`client-final-build.log` 源码 Server/Client 最终增量均 Succeeded。编译/验证不同引擎入口由 Tools/ue_environment.py 发现，真实本机路径不写脚本默认值。
- Windows cook：`cook.log` 对 L_Defense 处理 721 包（714 cooked，7 platform-skipped），0 error；唯一汇总 warning 为 MCP 插件许可提示。未运行打包 exe 或部署。
- 全回归：`combat-validated.log` 183/183 非 PIE Success，`pie-final.log` 5/5 PIE Success，共 188；通过 discovery.log 发现清单并追加本次最后两项用例，未降低发现范围。继续按 GAP-027 分进程隔离旧技能 CDO 遗留，不把该历史泄漏声明为已修复。最终“开局前核心死亡关闭开始面板”和 Meta Attribute 校验增量已在 `choice-final.log` 11/11 复验。
- 真实 UI：`pie-final.log` ChoiceButtons 用真实 Slate 按钮 MouseDown 测试开始、60 金币重抽、选卡和 Modifier 生效；不是物理鼠标自动化。`start-panel.png`/`choice-panel.png` 渲染截图已检查，小视口增加独立缩放保证正文可读。旧 RealMapNavigation 与三项 AI PIE 均通过。
- 网络：`network-victory-2/report.json` Server + 2 Client Pass，固定种子 831907；每名玩家初始重复发送两次重抽信封，实际只付 60+100，六波后余额 1210、经验 2000、已选 6 卡/效果存在；两个客户端各自确认 2 英雄 × 12 属性与服务器一致（`DefenseChoiceAttributeReplication ... Match=1`），客户端没有自主 Brain/权威行为。
- 资产：`asset-validation-final.json` 扫描 81 定义，0 error/0 warning；`asset-readback.json`/`asset-cold-readback.log` 冷回读 12 卡/效果引用、卡池、新 PC、15/25 秒设置、蓝图 BS_UP_TO_DATE。使用 UE 原生 API，不用文本编辑二进制；起始 UE MCP 连接被拒绝且无运行 Editor，按 30-01 安全降级。
- F2：本地独立检查视角复核最终 diff、Spec 和证据，未使用子 Agent。关闭同号换载荷、无界缓存、未生效卡、可变源池、非法 Meta 属性、死亡过期/终局、UI 穿透与小视口显示等发现。原型效果只允许永久无 Hook 的 Additive 项，后续机制卡需另审。
- 已知边界：GAP-030（DEF-002 日志已存在的动态 GE 定义 PostReplicatedAdd 诊断）仍有，完整 ActiveGE 对象未宣称网络正确。本步依赖的服务器效果、UnitView/AttributeSet 和私有选择结果已实测。不通过改 ASC 复制模式掩盖该公共内核缺口。
- 未执行：见顶部与 §7；长期玩法数值、4 人压力、完整团队商店/事件/换英雄/24 张机制卡留后续。
- 交付：文档（96 Markdown/517 本地链接，0 error）、差异和 delivery Gate 已执行；`delivery.json` 保存最终结果，报告保存在 Saved/DEF-004。所有改动留工作区，未创建新提交/未推送。

## 10. 变更记录

| 版本 | 日期 | 修改 | 原因 |
| --- | --- | --- | --- |
| 0.1 | 2026-10-16 | 冻结可玩个人强化流程 | 用户继续授权 |

## 11. 路由、自评与 Reflect

主 Skill 为功能开发，置信度 high；服务器候选基底在本任务被接入可玩流程，不要求用户重复批准已授权动作。

| 维度 | 权重 | 分数（0–5） | 证据及扣分 |
| --- | ---: | ---: | --- |
| 需求与 AC | 20% | 4.5 | 可见选择/重抽/效果/种子/默认选择闭环；内容仍为属性原型 |
| 架构与权限 | 20% | 4.5 | 公共入口、owner-only、身份/限频/冻结/有界缓存；保留既有 GAP-030 |
| 实现与数据 | 20% | 4.4 | 新资产与专用 Controller 接线，重复运行工具保留卡片作者修改 |
| 验证证据 | 20% | 4.5 | 三 Target 按实际日志、188 分进程回归、资产/PIE/双客户端；未做长时及打包 |
| 文档与可观测性 | 10% | 4.5 | Spec/ADR/台账/配置说明/诊断/报告齐全 |
| 交付卫生 | 10% | 4.5 | 保留 DEF-003 基底，无生成目录/无提交推送 |

- 计算总分：`4.5/5.0`（加权 4.48）。
- 自评结论：`EVIDENCE_SUFFICIENT`，不用自评代替用户验收。
- 用户验收状态：`READY_FOR_REVIEW`，尚未用户验收。
- Reflect：先用反例证明旧骨架没有效果提交及完整载荷幂等，再将开窗、截止与生效收敛到 Director；不把 UI/测试探针当作权威来源。改动局限本功能和必要公共接口，不改通用 Skill。下一步团队商店/复杂卡另立任务。
