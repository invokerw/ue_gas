# DEF-003 Phase 2A 种子与强化候选骨架

> Spec 版本：`0.3`
> 状态：`READY_FOR_REVIEW`
> Owner：Codex
> 创建日期：2026-10-15
> 关联进度台账：`00-01-Progress-Tracker.md` §12.13
> 风险等级：`L1`

## 0. Intake 与 Gate 记录

- 用户请求：DEF-002 验收完成并提交后继续下一步。
- 附件解释：无附件；DEF-001/20-05 Phase 2 是玩法边界，DEF-002 是已验收的服务器/地图基线；agent.md 是工程约束。
- 已读取入口：agent.md、README、00-01/03/04/05、10-01、10-08、20-02、20-03、20-05、90-16、DEF-002、任务路由/功能开发 Skill、CombatRng/Definition/Modifier/Director/Controller/Economy/Progression 代码。
- 主 Skill：`Skills/combat-feature-development/SKILL.md`
- 备选 Skill 与排除理由：combat-skill-development 暂不作为主 Skill；本轮是随机候选、RunState 和服务器事务骨架，不实现具体玩家技能动作。
- 路由置信度：`high`
- F0 结论：`GO`
- F0 依据：用户已确认 Phase 1 结果，设计文档把种子/强化候选列为下一阶段；本轮可在已验收的 Run/Player 记录上增量实现并可回滚。
- F1 结论：`APPROVED`
- F2 审查证据：反向检查固定种子跨玩家调用顺序、定义校验、候选过滤、权重上限、旧 Revision/非法 DefinitionId/重复 RequestId、空池兼容和 World/客户端权限；未发现阻塞。
- F1 审查人：Codex（L1 本地计划审查）
- F1 审查版本：`0.3`
- F1 计划审查证据：0.3 v0.3-plan.json 通过；按 §13 收敛服务器候选步进，批准本版本。此前历史：0.2 已核对第 12 节可玩范围、RPC 权限/计数/缓存、公共扣款、冻结/复现、死亡超时、Shipping 种子边界、三 Target/资产/PIE/双客户端矩阵与回滚；v0.2-plan.json 通过，批准本版本。0.1 历史：复核 Phase 2A 范围仅包含 RunSeed、Augment/Pool 定义、服务器三选一候选、ChoiceRevision/幂等和自动化；不引入团队商店、事件、正式 UI、换英雄或发布契约变化。确认使用稳定字符串域哈希不新增 GameplayTag schema，强化仍只能走公共 Modifier，结论 APPROVED。
- Build 解锁：`已解锁`；0.3 复核候选数据/冻结/身份/有界幂等/服务器权限/测试及回滚后批准。
- F1 重审条件：加入团队商店/事件/换英雄、改变网络载荷或发布契约、修改既有 RNG 算法/Tag/Event schema 时递增版本并重审。
- F2 结论：`PASS`
- Push-Ready 结论：`READY`
- 验证：Editor 构建、3 项 Augment Automation、DEF-002 防守回归、56 项资产校验和文档校验已通过；见 §7。
- 未执行：正式 UI/RPC/重抽扣款/PIE/Dedicated 选择流未接入，按 0.3 范围留 DEF-004；完整 Combat/三 Target 仅沿用 DEF-002 的已验收证据，不把它写成本轮新增结果。

## 1. 目标与范围

### 目标

在 DEF-002 的服务器 Director 上增加 Phase 2A 的可复现随机种子、强化定义/卡池、候选生成和 ChoiceRevision 骨架。候选生成固定为服务器权威、三张候选、DefinitionId 排序、合法过滤和重复请求幂等；先提供可测试的服务器 API，随后接入正式 Choice Window/UI。

### 范围

- `UDefenseAugmentData`：稳定 DefinitionId、家族、稀有度、最早波次、权重、适用英雄、互斥/前置、叠层上限、固定 Modifier/说明。
- `UDefenseAugmentPoolData`：服务器候选池和规则版本。
- `FDefenseSeedState`：RunSeed、RngAlgorithmVersion、各随机域计数；服务器持有，客户端只读短摘要预留。
- `FDefenseChoiceOffer`/`FDefensePlayerBuildState`：ChoiceRevision、候选 DefinitionId、过期波次、已持有卡、重抽次数/兼容状态。
- Director：开局冻结 RunSeed；使用通用稳定哈希/RNG 生成候选；按 RunId + PlayerId + ChoiceRevision 去重；波后生成 offer；提供选择/重发原结果的服务器函数，暂不添加最终 HUD 与团队商店。
- 自动化：固定种子复现、域计数隔离、合法过滤、DefinitionId 排序、选择 Revision/重复提交、旧阶段/非法候选拒绝。

### Non-Goals

团队即时商店、重抽付款事务、事件/RiskLedger、换英雄兼容休眠、复杂 Augment Runtime、四人容量、开发版可视化 Seed Panel、完整强化 HUD、随机地图和跨局存档不在本轮；本轮只保留这些后续接口。

## 2. 当前事实与依据

- DEF-002 Director 已有 RunId、PhaseGeneration、PlayerRecord、固定强化与每波奖励；Phase 2A 应把固定强化转成候选定义，不能另建第二套属性或玩家资源。
- `UCombatRngSubsystem` 的现有 Roll 需要 GameplayTag 域；为避免新增 Tag schema，本轮为通用稳定哈希增加字符串域/序号入口，继续使用 RNG v1 算法版本和服务器种子。
- `UCombatDefinitionData` 要求稳定 lower_snake_case DefinitionName；新定义类型必须进入 AssetManager/AssetValidator 扫描。
- Modifier Apply 仍是强化效果唯一入口；UI/客户端只读候选和 View，不提交数值或效果。

## 3. 行为与契约

### 主流程

Director 在初始化时冻结非零 RunSeed（开发测试可注入）；每次合法波次完成后为仍在局玩家创建一次 ChoiceRevision，并按域 `Run.AugmentOffer` 生成三张候选。候选先按 DefinitionId 排序，再按权重/门槛/互斥/前置/持有上限过滤；相同 RunSeed、RunId、PlayerId、WaveIndex、ChoiceRevision 的结果固定不变。Choice 请求只接受当前 Revision 和候选 DefinitionId，成功后记录 PlayerRunState/持有卡；重复 RequestId/旧 Revision/非法 DefinitionId 返回原结果或稳定拒绝。

### 状态与权限

服务器持有完整 Seed、候选、持有卡和计数；GameState/Owner View 只投影 ChoiceRevision、候选 DefinitionId、短哈希和状态。客户端不能提交 Seed、权重、效果、候选列表或奖励。Choice API 不直接激活 Ability/写 Health；选择后的具体 Modifier 绑定留在下一小步，先确保候选和幂等契约正确。

### 失败、取消、死亡、EndPlay

死亡玩家的 offer 保留到合法复活或本波窗口结束，核心失败作废；断线删除私有 Choice/PlayerRunState。旧 PhaseGeneration、旧 ChoiceRevision、重复 RequestId、缺失 DefinitionId、跨 World Player/Director 和不兼容英雄安全失败。World teardown 清理 Director 所持 offer 和候选，不保留静态 UObject 引用。

### 兼容与版本

RNG AlgorithmVersion 仍为 1；不增加 GameplayTag/Event schema 和发布契约字段。新 `DefenseAugment`/`DefenseAugmentPool` PrimaryAssetType 与 Choice View schema=1 独立。DEF-002 固定强化资产保留兼容，迁移器可把其 DefinitionId 当作初始持有卡；旧 Demo/地图不加载新模式池。

## 4. 实施计划

| 文件/资产 | 变更 | 原因 | blast radius |
| --- | --- | --- | --- |
| `Source/Combat/Combat/Defense/DefenseAugmentData.*` | 强化定义/卡池/候选/Seed/BuildState 结构 | 稳定内容与候选契约 | Defense 新类型 |
| `Source/Combat/Combat/Defense/DefenseDirectorComponent.*` | RunSeed、候选生成、ChoiceRevision、服务器选择 API | 唯一服务器权威入口 | Defense Director |
| `Source/Combat/Combat/Defense/DefenseGameState.*` | 防守 Seed/Choice 只读投影预留 | 后续 HUD 使用 | Defense View |
| `Source/Combat/Combat/Core/CombatRngSubsystem.*` | 字符串域稳定哈希入口，保留 RNG v1 | 不新增 GameplayTag schema | 通用 RNG |
| `Source/Combat/Combat/Validation/CombatAssetValidationCommandlet.cpp` | 扫描新定义类型 | 资产校验/cook | AssetValidator |
| `Source/Combat/Combat/Tests/CombatDefenseAugmentTests.cpp` | 固定种子/过滤/幂等/旧 revision | 直接语义验证 | Automation |
| `Config/DefaultGame.ini` | 新 PrimaryAssetType 扫描 | 资产发现 | Defense assets |
| `Doc/CombatSystem/20-Content/20-05...`, `00-01`, `00-04` | 当前 Phase 2A 事实与边界 | 文档同步 | 文档 |

## 5. 验收标准（AC）

- [x] AC-01：新 Augment/Pool 定义稳定、可校验、可被 AssetManager/AssetValidator 发现。
- [x] AC-02：固定 RunSeed 下，同一 Player/波次/ChoiceRevision 候选完全复现；不同随机域计数互不污染。
- [x] AC-03：候选过滤、DefinitionId 排序、三张候选、权重、波次门槛、互斥/前置/持有上限生效。
- [x] AC-04：选择/重复请求/旧 Revision/非法 DefinitionId/跨 World 安全失败，服务器不接受客户端效果数值。
- [x] AC-05：无既有 Demo 行为回归；DEF-002 旧固定强化和地图仍可运行。

## 6. Definition of Done

- [x] F0/F1/Build/Delivery Gate 与文档回写完成。
- [x] `Combat.Defense.Augment.*` 自动化通过，至少覆盖成功和拒绝路径。
- [x] Editor 构建、资产校验、相关 Defense 回归完成；PIE/Dedicated 留待正式 UI/RPC 步骤。
- [x] `git diff --check` 通过，无用户无关文件。

## 7. 测试矩阵与命令

| 层级 | 命令/用例 | 预期 |
| --- | --- | --- |
| Gate | `task_gate.py preflight/plan/build/delivery` | F0/F1/范围一致 |
| Pure/World | `Combat.Defense.Augment.DataValidation` | 定义/卡池拒绝非法输入 |
| World | `Combat.Defense.Augment.SeededOfferReplay` | 固定种子和域计数复现 |
| World | `Combat.Defense.Augment.ChoiceRevisionIdempotency` | 选择/重复/旧 Revision |
| World | `Combat.Defense.Augment.FilterAndOrdering` | 门槛/互斥/排序/持有上限 |
| 回归 | `Automation RunTests Combat.Defense.` + 直接相关 Combat | DEF-002 不回归 |
| 资产 | `CombatAssetValidation` | 新定义 0 error/0 warning |

## 8. 风险、回滚与升级

- 风险：权重与候选门槛尚未平衡；不在本轮伪装为完整随机构筑或 HUD。
- 回滚：删除新 Augment/Pool/Seed/Choice 文件及扫描条目，DEF-002 固定强化继续可用。
- 升级：加入正式 RPC/UI、重抽费用、团队商店、换英雄兼容、公开 View schema 时新建/升级 Spec。
- 需要人决定的问题：暂无阻塞决定；具体卡池权重和 UI 视觉留 Phase 2B。

## 9. 交付证据

- 代码/资产 diff：新增 DefenseAugmentData、RunSeed/候选 API、RNG 字符串域、AssetValidator 扫描和 3 项自动化；无新二进制资产。
- 构建/Automation/资产验证：Editor 增量构建成功；Augment 3/3、Defense 回归 12/12、AssetValidator 56/56 无错误。
- 未执行：正式 UI/RPC/重抽扣款、PIE/Dedicated 选择流、完整手工平衡；留 DEF-004。

## 10. 变更记录

| 版本 | 日期 | 修改 | 原因 |
| --- | --- | --- | --- |
| 0.1 | 2026-10-15 | 建立 Phase 2A 种子/候选骨架 Spec | DEF-002 验收后继续下一阶段 |

## 11. 路由、自评与 Reflect

### 路由记录

- 主 Skill：`combat-feature-development`
- 选择依据：涉及 Defense RunState、RNG、DataAsset、服务器候选和自动化；尚无具体玩家技能动作。
- 路由置信度：`high`

### 交付自评

| 维度 | 权重 | 分数 | 证据 |
| --- | ---: | ---: | --- |
| 需求与 AC | 20% | 4.3 | 0.3 服务器候选骨架完成，正式可玩 Choice Window 留后续 |  |
| 架构与权限 | 20% | 4.5 | RunSeed、稳定哈希、过滤和服务器权限已测 |  |
| 实现与数据 | 20% | 4.2 | 新定义/池/Offer/Choice API 完成，未接 Modifier 应用 |  |
| 验证证据 | 20% | 4.4 | Editor、Augment 3/3、Defense 回归、资产校验、文档通过 |  |
| 文档与可观测性 | 10% | 4.3 | Spec/ADR/测试报告写回 |  |
| 交付卫生 | 10% | 4.4 | 无资产噪声，未提交/推送 |  |

- 计算总分：`4.3/5.0`。
- 自评结论：`EVIDENCE_SUFFICIENT`
- 用户验收状态：`READY_FOR_REVIEW`

### Reflect 与调优

- 观察与证据：Phase 1 的固定强化已证明 Modifier/PlayerRecord 边界可复用，Phase 2 首先需要稳定候选和幂等。
- 根因类别：领域契约。
- 调整文件与预期收益：本 Spec 将种子域、ChoiceRevision、持有上限和服务器过滤冻结在进入代码前。
- 回归验证：待实现。

## 12. 0.2 计划修订：个人构筑可玩闭环

0.1 已按原批准范围写入服务器候选初稿，尚未编译验证；本修订先重审再接入 UI/RPC/资产，旧批准不覆盖以下扩展。当前范围以下文为准，交付时统一全文。

- 交付 Phase 2A 可玩个人构筑：初始准备及第 1–5 波结算后的三选一、重抽、15 秒选择窗口、超时默认首选、已选卡展示、开发服务器种子面板。团队商店、事件机制、复杂技能改造、换英雄界面属于后续步骤，不能宣称完整 Phase 2 已完成。
- 新 UDefenseAugmentData/Pool 使用独立 schema 1、稳定 ID、最多 64 定义、整数权重有上限、家族/基础/终局门槛、兼容英雄、前置/双向互斥、叠层上限。提供 12 张测试原型卡（攻击/技能/守护方向，各用公共属性 Modifier），不冒充设计 24 张最终技能改造卡。旧固定强化在配置新 Pool 时关闭，未配置池保持 Phase 1 行为。
- 采样按 DefinitionId 排序；初始只基础卡，前两波保证基础，终局需第 3 波后及同家族两张激活卡；优先原构筑/异家族/守护槽。先排除上次未选卡，不足才放宽。少于 3 张展示实际数，空池提供一次 80 金币补给。重抽排除当前全部候选，无替代卡禁止且不扣款。
- 复现输入为 Seed + 域文本 + 稳定加入席位 + Wave/ChoiceRevision + 槽位序号；RunId 只校验请求，不混入跨局复现随机键。域累计数只诊断，不把其他玩家的抽取次序混入本人的 RNG。原 CombatHash64V1 与公式/Tag/Event/Release 版本不变。
- Director 冻结池/卡/Modifier 数据并持有玩家构筑真值。开窗、选择、过期、死亡暂存与终局都由 Scheduler/Director 管理。手动选择要求当前英雄存活；死亡时窗口到期记录默认选择，效果复活后公共 ApplyModifier 落账。终局/断线清理，旧 RunId/Revision/生命/控制代次拒绝。
- 新 DefensePlayerController 子类持有复制的 ChoiceComponent，只向拥有者复制候选/已选卡/状态/响应；不改旧 PlayerController 默认类。RPC 载荷有正 RequestId、RunId、ChoiceRevision、Action、DefinitionId、LifeGeneration/BindingGeneration、EconomyRevision；共用现有连接限频/重放预算。32 条有界结果缓存，只有相同完整载荷返回原响应；换载荷复用 ID 拒绝。客户端绝不传 Seed/效果/价格。
- 重抽每局最多两次，60/100 金币。增加 Economy 服务器 TrySpendGold API（金额非负、修订匹配、余额足够、bMutating 拒绝），预先生成替代结果，扣款后一次替换候选。免费次数内容卡留后续，UI 显示实际价格/剩余次数。新增权限入口与本地网络 schema 均在 ADR-071 登记，不改旧经济 RPC。
- 新 HUD Widget 只读 Owner Choice View，点击三卡/重抽走 RPC，阻断鼠标穿透；构筑列表显示名称/家族/效果。服务器开发面板在开局前输入十进制/0x 种子或留空随机、锁定并开始；完整种子和复制按钮仅本地服务器可用，客户端仅见短摘要/域计数并等待开始。Shipping 自动随机开局；Dedicated 自动开局且支持开发命令行 Seed 参数。已有场景 smoke 开关自动开始，正常本地玩家看得见开始按钮。
- RunData 新字段默认兼容；新地图资产更新 Pool、Defense PC/HUD 与 25 秒整备（其中前 15 秒选择），不改原战斗 Demo。GameState 独立防守投影 schema 升为 2，增加等待开始/短摘要/域计数；没有旧跨版本网络兼容承诺，客户端/服务器用同一版本。
- 文件增加 DefenseChoiceComponent、DefensePlayerController、DefenseBuildWidget、DefenseOfferSampler，扩展 Director/GameMode/HUD/Data/GameState；Economy TrySpendGold、NetworkSecurity 通用组件意图验证；Tools/setup_defense_augments.py 生成/冷回读资产，Tests/CombatDefenseAugment*、网络/PIE探针、相关 Spec/台账/ADR/专题。保持单 Runtime Module。
- 验证矩阵：先写最小失败测试（固定种子跨玩家调用次序/重复扣费/无替代池/旧修订/空池），再实现；测试数据/非法值/兼容/叠层、seed解析锁定、选择/重抽去重、死亡过期/复活、终局/Owner EndPlay、客户端/跨World拒绝。Editor/Server/Client 构建，专项及风险相关回归，资产编译保存冷回读，真实 UI/PIE，独立 Dedicated 双客户端选择/重抽与构筑/余额复制验证。记录未执行项，不虚构 Red 或结果。
- 回滚：本地 d7c9809 为 Phase 1 基线；新 Pool 设空且关闭开发等待可恢复固定卡行为。回退新组件/API/扫描和新资产增量，保留用户文件；不自动提交 Phase 2A。

输入接线细化：将已有 `IsPointerOverCombatUI` 改为 virtual，Defense PC 在保留原 HUD 检测后只追加新面板的实际可见几何检测，避免选择卡片时命令穿透；不改变旧模式输入。


## 13. 0.3 范围收敛：先交付服务器候选骨架

0.2 的 UI/RPC/重抽扣款范围过大，当前小步收敛为可独立验证的服务器候选骨架；0.2 计划批准不覆盖本节之后的实现。0.3 交付：

- UDefenseAugmentData/Pool 稳定定义、RunSeed、字符串域 keyed hash、三张候选、权重/门槛/互斥/前置/DefinitionId 排序。
- Director 对每个开局玩家生成并缓存 ChoiceRevision，提供服务器 `GetOrCreateAugmentOffer` / `ChooseAugment` API；同一 RequestId 重发返回原结果，旧 Revision/非法候选拒绝。
- 候选选择只登记 OwnedAugments，不在本小步应用 Modifier；死亡/断线/UI/RPC/重抽扣款留 DEF-004，RunData 空池保持 DEF-002 固定强化行为。
- 新增 World Automation 覆盖固定种子重放、域计数不串、过滤/排序、旧 Revision/非法候选/重复 RequestId；Editor 构建和 AssetValidator 只验证新类型。PIE/Dedicated/UI 不适用，直到 DEF-004 接入可见入口。

0.3 审查细化：合法开窗仅 Preparation/Intermission/BossPreparation；Offer 带 RunId/PhaseGeneration/Deadline，手动选择复核英雄生命。结果缓存最多 32 条，只重放完全相同请求。RNG 使用本候选槽位序号，域累计数仅诊断，不受其他玩家调用先后影响。原型持有上限固定 1；不足三张返回实际数量，空池补给/超时自动选/效果提交留后续可玩接线。冻结 Pool/卡，新增编辑器校验；新配置资产保存在 Defense/Augments，暂不修改已验收 RunData 默认空池。


## 14. 后续接线事实（2026-10-16）

DEF-004 已在保留本任务未提交基底的前提下完善并验证可玩 UI/RPC/重抽/Modifier：Offer schema 2，卡/池严格校验、深冻结、期限、死亡默认与有界完整回执。当前行为以 [DEF-004](DEF-004-defense-choice-flow.spec.md) 为准，本 Spec 的 0.3 与 3 项测试数字为上一轮服务器骨架历史证据，不代表当前完整流程。
