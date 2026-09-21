# AI-001 StateTree 通用 AI 决策系统设计

> Spec 版本：`0.2`
> 状态：`COMPLETED`
> 用户验收：2026-09-20 用户确认 review 完成；实施另见 AI-002。
> Owner：Codex
> 创建日期：2026-09-20
> 关联进度台账：`00-01-Progress-Tracker.md`
> 风险等级：`L0`

## 0. Intake 与 Gate 记录

- 用户请求：使用 StateTree 设计一套比较通用的 AI 决策系统，明确不采用此前建议的轻量层级状态机，并编写项目文档。
- 本轮请求：用户在文档 review 后要求“补齐吧”；修订三项实现契约，并补充完整野怪资产示例和 Utility 实验性边界，仍只交付文档。
- 附件解释：无附件；浏览器环境提供的 Epic StateTree 页面属于参考上下文，不代表已授权操作浏览器或编辑引擎。
- 已读取入口：`agent.md`、`README.md`、进度台账、`10-01` 架构、`10-07` Order、`00-05` 流程、`Skills/combat-task-router/SKILL.md`、`Skills/combat-feature-development/SKILL.md`；补读目标、调度、生命周期、公开扩展与客户端服务器交互专题。
- 主 Skill：`Skills/combat-feature-development/SKILL.md`
- 备选 Skill 与排除理由：`combat-skill-development` 面向可施放技能实现，本次是 AI 架构文档；不修改 Skill 或运行时代码。
- 路由置信度：`high`
- F0 结论：`GO`
- F0 依据：用户已授权修订文档；开工时已有 v0.1 的五份未提交 Markdown 变化，均保留并增量修订；不改变代码、资产、发布或权限。
- F1 结论：`APPROVED`
- F1 审查人：Codex（L0 文档计划自审，依据 `00-05` §4–5）。
- F1 审查版本：`0.2`
- F1 计划审查证据：2026-09-20，0.2 preflight/plan 均为 0 error；Codex 按 L0 授权自审范围、AC-07–11、源码依据、文档验证和增量回滚。只修订五份 Markdown，新增执行边界接口继续作为后续实现提案，不改变当前运行时。0.1 批准由本轮批准取代。
- Build 解锁：0.2 build Gate 已通过，0 error；在该检查之后修订正文。报告使用 `Saved/AI-001/v0.2/`，保留原报告。
- F1 重审条件：新增行为文件修改、扩大运行时契约变更或改变本次验证范围时，递增 Spec 版本并重新审查。
- F2 结论：`PASS`
- Push-Ready 结论：`READY`
- 验证：0.2 preflight/plan/build/delivery 全部通过；源码事实复核、文档校验 83 Markdown / 436 本地链接 / 0 error 和差异检查通过，delivery 为 0 error / 5 changed files。新增五项 review 补充已完成。0.1 报告保留于 `Saved/AI-001/`，本轮报告在 `Saved/AI-001/v0.2/`。
- 未执行：UE 构建、Automation、蓝图、PIE、Dedicated 与性能测试不适用本次文档交付；未来 AI 实现仍须执行，本文不宣称通过。

## 1. 目标与范围

### 目标

交付可供后续实现和内容编排使用的 StateTree AI 架构方案，明确复用边界、引擎语义、公共扩展面、异步生命周期和验证条件。

### 范围

- 新增 `10-17-StateTree-AI-Decision-System.md`，说明 StateTree 运行宿主、Schema、Context、Instance Data、事件、Linked Asset、条件/任务/评分、感知与记忆、命令适配和服务器预算。
- 给出小兵、野怪、英雄机器人、Boss 的组合方式和至少两条可追踪行为时序。
- 列出当前代码可复用的能力、必须新增的适配接口、实现分期与测试矩阵；设计中的类型、资产和 Tag 全部标注为拟新增。
- 同步文档索引、设计任务台账与 proposed ADR/开放缺口。
- 0.2 补充转移阶段与同帧仲裁表、PreparedIntent 的跨状态所有权、保护窗口与下一次攻击的交接、可照表编排的野怪示例、Utility 版本准入，并同步后续接口和验收矩阵。

### Non-Goals

不修改 C++、Build.cs、GameplayTag、配置、蓝图、DataAsset、地图或发布版本；不创建实际 StateTree；不执行游戏或性能验收；不承诺完整迷雾、团队战略或召唤物系统已实现。

## 2. 当前事实与依据

- 相关 DDD：`10-01`、`10-02`、`10-03`、`10-07`、`10-09`、`10-10`、`20-02`、`20-03`、`90-16`、`00-03`、`00-04`。
- 代码事实：`CombatUnitAIController.h:13` 限定导航/Crowd 职责；`CombatOrderComponent.h:41` 提供 IssueOrder，`:78` 提供最终完成委托；`CombatTargetingSubsystem.h:49` 提供范围候选查询。
- `ue_gas.uproject` 启用 StateTree/GameplayStateTree 插件，但 `Combat.Build.cs` 未声明对应运行时模块依赖；当前源码没有通用 StateTree AI 决策层。
- Order 替换取消当前项，AttackTarget 是持续命令，施法按 OrderReleased 释放；接收成功不等于执行完成，命令可能在 IssueOrder 返回前同步结束。
- 当前目标规则中的完整权威可见性提供者尚未接入；范围查询仍枚举 World 单位。设计必须明确视野和容量边界。
- 参考证据：Epic UE 5.8 StateTree 官方文档/API；可获取的本机引擎源码只用于核对接口，不修改。
- 当前测试/日志证据：0.1 文档检查已运行；0.2 重新验证并在 §7 登记。任何版本的文档 Gate 和既有项目验收数字均不作为 AI 运行证据。

## 3. 行为与契约

### 主流程与状态转换

本次仅生成设计文档：事实核查 → StateTree 原生组织方案 → 异步与权限契约 → 可复用示例 → 分期和验证。任务从 PLAN_REVIEW 经 F1/build 进入 BUILDING，完成检查后进入 READY_FOR_REVIEW。

### 输入、输出与数据约束

输入为用户选定的 StateTree 方向、仓库事实和官方引擎资料；输出为 Markdown 设计文档和关联记录。当前实现、建议设计、未验证假设和待决策参数必须分开标注。

### 权威边界与权限

方案遵守服务器决策、公共 Order/Targeting/ASC 入口和 Combat Scheduler 计时；StateTree 不生成第二套战斗状态、属性或结算链路。当前文档批准不代表后续运行时 API、Tag/schema 或资产迁移获准。

### 失败、取消、过期、死亡、EndPlay 与重复请求

文档必须覆盖任务退出取消自己的命令、同步回执、重选去重、抢占、目标失效、失去控制、同帧死亡/重生、旧查询结果、World teardown 和暂停恢复；列出当前 API 缺口及拟增加的最小适配。

### 兼容、版本与迁移

本次不改变运行时契约。ADR 标注设计提案，后续实现需另建 feature Spec 并决定新增定义类型、公开接口与版本迁移；既有单位默认无自主 AI。

## 4. 实施计划

| 文件/资产 | 变更 | 原因 | blast radius |
| --- | --- | --- | --- |
| `10-Architecture/10-17-StateTree-AI-Decision-System.md` | 新增 StateTree 通用 AI 方案 | 用户明确交付物 | 设计文档 |
| `Specs/AI-001-statetree-ai-design.spec.md` | 任务入口、审查和证据 | 记录设计交付边界 | 本任务 |
| `README.md`（CombatSystem 文档索引） | 增加专题/Spec 导航与阅读路径 | 便于发现文档 | 文档导航 |
| `00-Project/00-01-Progress-Tracker.md` | 增加 AI-001 文档待评审记录 | 保持状态唯一来源 | 当前文档任务 |
| `00-Project/00-04-Decisions-Gaps.md` | 追加 proposed ADR 与接入缺口 | 区分已选方向与未实现设计 | AI 后续决策 |

先核对 UE 5.8 原生任务完成策略、数据绑定、事件及 Linked Asset 机制；再写方案并逐条核对接口可用性。本文的实现范围只包含上表五个 Markdown 文件。

0.2 实施顺序：先修正事件/完成转移语义，定义普通重评让位于未消费完成结果；再增加类型化 PreparedIntent 和执行边界交接，避免兄弟任务数据悬空及低频采样饿死施法；最后给出无 Utility/EQS 依赖的最小野怪接线表、实验性 API 准入门，以及对应测试和 ADR/Gap。所有新增类型、节点和 Order 接口保持“拟新增”，不写代码或资产。

## 5. 验收标准（AC）

- [x] AC-01：StateTree 是唯一行为编排主体，没有自建层级状态机替代 StateTree；10-17 §1、3、4、6。
- [x] AC-02：明确宿主/Schema、数据所有权、原生树/节点/子树、事件与调度边界，能据此创建和配置不同 AI；10-17 §3–6、9、10、15。
- [x] AC-03：明确现有 Order 的持续/同步/异步语义，覆盖单命令写入者、任务重选、抢占、取消和旧回调防护；10-17 §8、12、13。
- [x] AC-04：覆盖感知与记忆、目标/动作选择、技能适配、多个单位类型和至少两条行为时序；10-17 §5–7、11–12。
- [x] AC-05：给出实现分期、能力缺口、性能预算方法、调试信息和具体测试矩阵，区分设计与实测；10-17 §14、16–18。
- [x] AC-06：索引/台账/决策可导航，官方资料和源码事实可回读，首轮文档校验与 diff 检查通过，工作区只含 §4 的五个 Markdown 文件。
- [x] AC-07：准确区分事件与完成转移处理阶段，明确同帧完成、普通重评、紧急事件及完成后记账的优先关系；10-17 §6.1。
- [x] AC-08：PreparedIntent 明确跨兄弟状态/Linked Asset 的生产消费、身份、有效期、转移保留和中断清理，不改写只读观察快照；§5.3、6.2、10。
- [x] AC-09：动作保护解除有可靠唤醒及下一次普攻起手的仲裁，涵盖撤销、超时、旧事件和原命令继续执行的路径；§8.5、9、12.3。
- [x] AC-10：最小野怪示例列出树、参数来源、各状态任务/输入/进入条件/成功失败出口/中断，具备有界失败恢复；§15.4。
- [x] AC-11：说明 Consideration 的实验性状态、版本隔离及阶段 C 准入，并把以上新增契约纳入后续验收矩阵；§6.3、16–18。

## 6. Definition of Done

- [x] 0.2 主文档与新增 AC 对齐，并完成事实和风险复核。
- [x] 0.2 preflight/plan/build/delivery Gate 及最终文档检查通过，实际结果写回。
- [x] 新增协议与现有接口的差异明确登记，未经运行验证的内容不标为已实现。
- [x] 修订交付状态为 READY_FOR_REVIEW；不自动标记用户已验收，不提交或推送。

## 7. 测试矩阵与命令

下表记录 0.2 检查；0.1 历史报告仍保留于 `Saved/AI-001/`。所有数字仅为文档验证证据。

| 层级 | 用例/命令 | 预期证据 | 结果 |
| --- | --- | --- | --- |
| 任务入口 | `python -X utf8 -B Tools/task_gate.py --mode preflight --spec Doc/CombatSystem/Specs/AI-001-statetree-ai-design.spec.md --kind docs --report Saved/AI-001/v0.2/preflight.json` | 入口有效 | PASS，0 error |
| 计划 | 相同命令的 `--mode plan`，report 为 `Saved/AI-001/v0.2/plan.json` | 计划可审查 | PASS，0 error |
| 文档实施准入 | 相同命令的 `--mode build`，report 为 `Saved/AI-001/v0.2/build.json` | 当前版本已审查 | PASS，0 error |
| 文档 | `python -X utf8 -B Tools/validate_docs.py --report Saved/AI-001/v0.2/docs.json` | 路径/编号/格式检查 | PASS，83 Markdown / 436 本地链接 / 0 error |
| 差异 | `git diff --check` 和文档事实人工复核 | 无空白错误/越界改动 | PASS；只包含五个 Markdown 文件；CRLF 提示不是校验错误 |
| 交付 | `python -X utf8 -B Tools/task_gate.py --mode delivery --spec Doc/CombatSystem/Specs/AI-001-statetree-ai-design.spec.md --kind docs --report Saved/AI-001/v0.2/delivery.json` | 文档交付 Gate | PASS，0 error / 5 changed files |
| UE 构建/Automation/PIE/Dedicated/Soak | 本次仅文档 | 不作为运行时能力证据 | N/A，未执行 |

## 8. 风险、回滚与升级

- 风险：StateTree 版本语义误用；兄弟状态数据丢失；边界票据与攻击起手竞态；Task 退出取消其他控制者命令；把角色模板写成不可扩展的大树；遗漏可见性、实验性 Utility 或性能限制；把拟新增接口误记为已存在。0.2 给出契约和测试要求，尚无运行证据。
- 回滚方式：只撤销本次五个 Markdown 文件的增量；保留其他工作区变化。
- 触发升级的条件：需要修改代码/资产或现有发布契约才能完成本次交付时重新定界；当前仅记录后续实现依赖。
- 需要人决定的问题：详细玩法参数与实施优先级留给设计评审；用户已选定 StateTree，不重复征询框架选型。

## 9. 交付证据

- 代码/资产 diff：零；`git status --short` 核对为 §4 五个 Markdown 文件。
- 文档 diff：10-17 保持 18 个章节；0.2 新增 §5.3 准备结果/完成凭证、§8.5 动作边界交接、§15.4 完整野怪接线，修订 §6.1 转移阶段、§6.3 Utility 准入，并更新节点/事件/分期/验收；索引、台账和 proposed ADR-062/GAP-028 同步。
- 引擎事实核对：`python -X utf8 -B Tools/ue_environment.py --json` 定位本机环境；安装版 `Build.version` 为 UE 5.8.2 / CL 56702186。只读核对 StateTreeComponent/Schema、TaskBase、Types、TasksStatus 和 ExecutionContext 的事件调度；没有启动或修改 Editor。
- 0.2 追加证据：ExecutionContext.cpp 的 TriggerTransitions 处理阶段及完成转移无统一优先级；EditorData/Compiler 的执行路径绑定限制；ConsiderationBase.h 与官方 API 的实验性声明；现有 Order 的 HandleAttackReady 直接 Pump，边界交接接口尚不存在。
- 构建结果：N/A，未执行；纯文档。
- Automation/PIE/Dedicated 报告：N/A，未执行；实现阶段测试矩阵写入主文档。
- 未执行验证及原因：不以文档评审替代实际引擎接入、资产编译、联机或性能测试。
- 剩余风险：设计待用户评审；具体引擎适配与玩法体验需后续实现证明。

### F2 文档对抗复核与 review 修订

审查人：Codex；方式：单 Agent 在正文完成后按 Spec、最终文件、现有 Order 源码与实际 UE 5.8.2 头文件另做逐项复核，不宣称独立代理审查或运行测试。审查范围是设计准确性与可实施边界。

| 检查/发现 | 处理与证据 | 结论 |
| --- | --- | --- |
| Combat 入口若只要求 CurrentTarget，首次选敌可能无法进入 | 根树明确先按合法候选进入，再经 AcquireTarget 到 Execute；10-17 §6.1 | 已关闭 |
| 先撤销运行代次可能同时阻断旧动作清理 | 增加只保留精确旧句柄的清理记录，撤权阻止提交但不阻止幂等取消；§8.4 | 已关闭 |
| 将排队攻击误描述为一定打断引导 | 区分替换型 Attack 与 FIFO 追加；现有追加会等待释放，AI 延后选择是防止过时决策；§12.3 | 已关闭 |
| 0.1 曾用统一 Priority 描述同帧完成/紧急事件，后续 review 证实不准确 | 0.2 按实际 TriggerTransitions 分阶段重写；普通重评受终态/凭证门控，紧急中断明确放弃行为记账；§6.1、17 | 0.2 文档修正；运行验证待实现 |
| 完成策略与事件唤醒不能依赖泛化教程 | 源码确认 Any/All、bConsideredForCompletion、事件 Tick 和 SendEvent 成功入队后 ScheduleNextTick；§4、9、18 | 已核对，运行验证明确留后续 |
| 台账更新日志新增行与原表隔开 | 移除多余空行，维持同一 Markdown 表 | 已关闭 |
| 权威旁路、过期查询、取消新命令、迷雾与容量误报 | §7–17 明确公共入口、身份、结果缓存、来源切换和未实现边界；现有接口缺口列入 GAP-028 | 无未关闭的文档阻断项 |
| 兄弟状态不能直接绑定前一准备任务；生产者退出会使数据交接含糊 | §5.3 定义 Scope/Preparation、ConsumerSlot、时效、成功退出保留和中断废弃；完成凭证在动作退出后继续存在 | 文档已补齐 |
| 普攻保护解除缺少可靠重评窗口 | §8.5 定义按句柄匹配的边界请求、Ready、Keep/切换/超时、所有起手入口统一检查；§12.3/17 更新 | 文档已补齐；接口尚未实现 |
| 新协议的迟到 Ready、调度失败和失败输入无凭证会再次形成漏洞 | 明确 Ready 失效复核、无法安排超时则不保持停打、直接拒绝也形成失败凭证；§5.3、8.5 | 本轮交叉复核后关闭文档缺口 |
| 策划仍需自行推导完整资产接线 | §15.4 给出参数、完整状态表、硬中断、归位请求写入、失败计数、Root 完成兜底和五条验证轨迹 | 文档已补齐；资产尚未创建 |
| Consideration 实验性与 A 阶段启动依赖不清楚 | §6.3/16 将评分适配隔离与版本准入列为 C 前置；最小 Profile 和基础边界协议前移至 A | 文档已补齐 |

### Push-Ready 六层

| 层 | 结论与证据 |
| --- | --- |
| Tests | 文档校验与 delivery 通过，结果见 §7 |
| Types/Build | N/A；无代码/配置/资产变更，未执行 UE 构建 |
| No Regression | 文档链接与 diff 检查；运行时回归 N/A |
| Adversarial | F2 文档复核通过，发现已修订；不替代后续实现验证 |
| DDD/Constraints | 服务器权威、单一执行链路、Scheduler、生命周期和单模块约束保持；拟新增接口单列 |
| Decisions | ADR-062 为 proposed，GAP-028 待处理；未将设计方向或文档完成写成运行时验收 |

## 10. 变更记录

| 版本 | 日期 | 修改 | 原因 |
| --- | --- | --- | --- |
| 0.1 | 2026-09-20 | 建立 StateTree AI 文档任务 | 用户明确采用 StateTree 并要求通用设计文档 |
| 0.2 | 2026-09-20 | 修订转移仲裁、准备结果交接和动作边界，补充野怪资产接线与 Utility 准入；重新审查和验证 | 用户要求补齐 review 发现；不扩大到运行时实现 |

## 11. 路由、自评与 Reflect

### 路由记录

本次为文档交付，使用 Combat 功能开发 Skill 的 docs 流程；置信度 high。与此前纯咨询区分，不把用户纠正的 StateTree 方向降级成轻量状态机。

### 交付自评

以下分数只评价 0.2 文档交付。0.1 曾自评 4.6，但随后 review 发现语义和协议缺口；本轮按修订后的文件与实际证据重评，不把运行时测试计划算成通过。

| 维度 | 权重 | 分数（0–5） | 证据与扣分原因 |
| --- | ---: | ---: | --- |
| 需求与 AC | 20% | 5.0 | 十一项 AC 有章节映射；五项 review 补充完成，保持 StateTree 原生编排 |
| 架构与权限 | 20% | 4.5 | 权威、任务并行、控制来源、回执和生命周期明确；公开接口仍是设计提案 |
| 实现与数据 | 20% | 4.5 | 配置/节点/子树/数据所有权和缺口齐全；尚无实际资产原型，已标明边界 |
| 验证证据 | 20% | 3.5 | 源码与官方资料、文档和差异检查；为单 Agent 复核，跨状态资产及交接时序尚无原型实证 |
| 文档与可观测性 | 10% | 5.0 | 索引、台账、ADR/Gap、调试指标和行为时序完整 |
| 交付卫生 | 10% | 5.0 | 保留 v0.1 未提交内容，变更仍限五份 Markdown，分版本报告，无代码/资产/提交/推送 |

- 计算总分：`4.5 / 5.0`。
- 硬性封顶或未执行项：无必需文档检查受阻；UE 构建和运行矩阵不属于本次文档范围，未计为通过。
- 自评结论：`EVIDENCE_SUFFICIENT`（文档范围）。
- 用户验收状态：`ACCEPTED`；补齐后的 0.2 已获用户确认“review完成，开始工作吧”。以上文档交付证据保留原时点；后续运行时不回填为 AI-001 文档测试，见 [AI-002](AI-002-statetree-runtime.spec.md)。

### Reflect 与调优

- 观察与证据：后续 review 发现 0.1 的 Priority 描述不准确，并缺少跨状态数据交接和保护窗口之后的推进协议；链接/Gate 通过不能证明设计语义闭合。
- 根因类别：单次文档实现问题与领域契约遗漏。
- 调整文件与预期收益：修订 10-17 的明确协议、最小资产接线与验证矩阵，同步 Spec/索引/台账/ADR；后续实施能够针对竞态和失败路径建立测试。
- 回归验证：文档校验、事实核对、差异检查和任务 Gate；最终结果见 §7。
- 调优动作：仅在本次设计中吸收用户纠正和复核发现；不调整项目 Skill 或历史文档规则。
- 后续决定：用户已接受方案并授权开工，阶段 A 由 AI-002 承接；不修改 Skill 或历史验收证据。
