# AI-003 StateTree 阶段 B 感知与角色复用

> Spec 版本：`0.1`
> 状态：`COMPLETED`
> Owner：Codex
> 创建日期：2026-09-21
> 关联进度台账：`00-01-Progress-Tracker.md`
> 风险等级：`L1`

## 0. Intake 与 Gate 记录

- 用户请求：阶段 A 验收成功后，用户回复“继续吧”，授权按上一轮建议实施阶段 B：感知/记忆、候选排序、野怪守点追击归位、小兵推进交战恢复，以及清晰的演示入口。
- 验收与提交授权：2026-09-21 用户明确“验收完成，提交吧”，确认 B 阶段验收并授权将本次 AI 设计及 A/B 实现创建本地 Git 提交；不推送远端。
- 附件解释：无新附件；既有 Epic 页面为参考，仓库 agent.md 与已审阅的 10-17 是工程约束。
- 已读取入口：`agent.md`、`README.md`、`00-01`、`00-03`、`00-04`、`00-05`、`10-01`、`10-02`、`10-07`、`10-09`、`10-10`、`10-17`、`20-02`、`20-03`、`90-16`、`Skills/combat-task-router/SKILL.md`、`Skills/combat-feature-development/SKILL.md`；沿用阶段 A 的移动/生命周期专题上下文并复核当前源码。
- 主 Skill：`Skills/combat-feature-development/SKILL.md`
- 备选 Skill 与排除理由：技能开发 Skill 面向具体 Ability；本次不新增技能结算。
- 路由置信度：`high`
- F0 结论：`GO`
- F0 依据：用户授权的兼容新增；StateTree 为唯一行为编排，阶段 A 的未提交代码和 81 个新增资产保留。
- F1 结论：`APPROVED`
- F1 审查人：Codex，依据 00-05 的 L1 本地审查权限。
- F1 审查版本：`0.1`
- F1 计划审查证据：2026-09-21 preflight/plan 均为 0 error（Saved/AI-003/preflight.json、plan.json）。独立按七项 AC 核对：行为选择仅在资产；观察/协议数据不构成第二行为机；取消与记账保留终态优先；所有新增异步工作沿用 epoch/生命/完整 Schedule 身份；旧 Profile 默认关闭；两种角色与失败路线覆盖真实树；三 Target/PIE/Dedicated/cook 不用单元测试替代；回滚精确保留 A。无需改变发布或网络版本，批准 0.1。
- Build 解锁：F1 批准后、首次行为修改前已通过 build Gate，证据为 `Saved/AI-003/build.json`。
- F1 重审条件：范围、架构、权限、迁移、测试矩阵或回滚实质变化时升版并重新审查。
- F2 结论：`PASS`
- Push-Ready 结论：`READY`
- 验证：阶段 B 已完成实现和分层验证；最终 Editor/Server/Client 三 Target 构建通过，`Combat.AI` 20/20、完整 `Combat.` 121/121，Combat 资产校验 37 项 `0 error/0 warning`，角色 PIE、Dedicated 双客户端、导航构建与角色地图 Windows cook 均通过。证据集中在 `Saved/AI-003/` 与 `Saved/UEEnvironment/Dedicated-Installed-AIRoles/`。
- 未执行：Utility/EQS、完整权威迷雾、AI 64/128/256 容量、网络损伤、长 soak 和打包后 exe 启动仍属 C/D 或另行发布验证；本轮不宣称通过。

## 1. 目标与范围

### 目标

无需手工给攻击目标，服务器 StateTree 根据合法知识执行职责；同一套准备、Order 执行和结果确认节点支撑守卫/野怪、巡线和巡逻。

### 范围

1. Profile 增加可选范围/LOS 感知、有限记忆、候选上限、定义优先级/威胁/距离排序、保持门槛、追击边界、重试与到达参数。
2. Brain 持有只读知识快照和服务器职责输入（锚点、路线、循环策略、游标）；Scheduler 唯一驱动采样和退避。受击只增加当前感知许可来源的威胁，不暴露隐藏攻击者。
3. StateTree 原生进入条件与原子任务，组合 Guard / Engage / ReturnHome / LaneAdvance / Patrol Linked Asset；野怪与小兵两个根树；执行仍用阶段 A 的单写入桥接。
4. 独立角色演示地图、可见角色/目标标识、初始相机位置；阶段 A 原地图与 Blueprint 行为保持兼容。
5. 直接自动化、实际编译/保存资产、真实 PIE、Dedicated 双客户端角色行为与 A 回归、三 Target、完整 Combat、资产校验与角色地图 cook。

### Non-Goals

不做 Utility、EQS、自动施法/Boss/队伍战略、权威迷雾、AI 空间索引或容量承诺；不改变 RPC、Combat Event/Tag/Release schema、玩家输入、技能和属性数值。实施交付时未提交；用户验收后已授权本地 Git 提交，推送不在本次范围内。

## 2. 当前事实与依据

- `AI/CombatAIBrainComponent.cpp`：已有 Scope、一次准备、终态优先、Order 精确取消和控制 epoch；新角色数据必须沿用这些身份。
- `AI/CombatAIStateTreeTasks.cpp`：事件 Tick + Scheduler 等待，链接任务可消费父 Scope；无 C++ 行为状态机。
- `Targeting/CombatTargetingSubsystem.cpp`：公共圆形查询已复核阵营/生命/LOS，按 Actor ID 稳定排序；当前实现枚举 World，规模优化留 C。
- `Validation/CombatAIAssetBuilder.cpp`：真实 StateTree Compiler 与活动路径单写入校验；阶段 B 继续采用完成转移，强制归位由执行 Task 先记录取消原因再走完成凭证，避免事件转移跳过回执。
- 2026-09-21 未发现运行中的 UnrealEditor，MCP initialize 连接 127.0.0.1:8000 被拒绝；降级为原生 UE API/commandlet + 独立 Editor 验证，之后可用时再回读 MCP。
- 当前 release `combat_v4_economy_rc1`、Contract 4 / Tag 与 Event 3 / Content 2 不变；AI Profile 本地版本 1 做默认关闭的增量字段，不重新解释已有数据。

## 3. 行为与契约

### 主流程与状态转换

根 Scope → 等待有效职责 → 有序选择（故障等待/退避 → Return → Engage → RouteDuty → Guard）。选择与转移全部在 StateTree 资产。子树 Prepare → Execute → Resolve；结果提交后才能普通重选。Guard 等快照/职责修订；Lane 与 Patrol 共享到点提交节点，区别仅在路线是否循环。

野怪：Guard → 发现敌人 → Attack；目标终结/失感知/离营或时间上限 → 记录返回请求 → MoveHome → 匹配成功凭证及 XY 到达复核 → 清返回请求和可选记忆 → Guard。归位不被新敌人/受击打断，职责更换、死亡和撤权例外。最小野怪不做交战中普通换敌；候选排序与保持在合法选择点生效。

小兵：移动当前航点 → 获得敌人后精确取消路线命令 → Attack → 结束后继续原游标；只有匹配成功凭证和实际到达复核才推进游标。最后一点非循环路线进入等待；巡逻到最后一点循环。已完成路线不因普通感知刷新再次下原地 Move。

### 输入、输出与数据约束

- 服务器 `SetAssignment` 只接收有限锚点/有限路线（上限 64）和循环标志，校验成功后增加职责修订；显式重发合法职责可重试，普通 Sense 不修改职责修订。
- 感知使用 Enemy + 公共 Targeting；VisibilityPolicy 要求完整视野时明确拒绝。记忆只在当前可感知集合更新位置/生命/定义；丢失后只保留 LastSeen，不继续读取未知敌人状态。
- 目标弱引用 + LifeGeneration；新生命只有重新观测后成为新候选，不能复用旧攻击身份。稳定优先级、有限威胁、距离和 Actor ID 同分规则；上限裁剪发生在排序后。未知定义优先级为 0。
- PreparedIntent/Receipt 增加操作类型、路线索引与本地取消原因；不是 Patrol/Combat 行为枚举。角色动作不会调用 SetObjective 来伪造外部职责变更。
- 采样默认交战 0.2 秒 / 空闲 0.8 秒；追击/失感知检测存在最多一个采样间隔及调度延期，明确不等于迷雾零延迟。时间全部使用 World Game Time。
- 同职责同操作最多 3 次尝试（含首次），失败由一次 Receipt 确认累计；0.5 秒退避期间普通观察不跳过。超限仅有效新职责/显式恢复解除；正常目标终结/策略取消不消耗重试。

### 权威边界与权限

客户端不启动感知、订阅、树和 Scheduler；只观察既有单位/弹体/属性复制与 Demo 标识。没有新 RPC。采样只发布事实，回调只写知识/唤醒；无回调直接下 Order、强行移动或修改 Health。

### 失败、取消、过期、死亡、EndPlay 与重复请求

终态优先于新快照；成功到达只有一次游标/归位提交。旧 Scope/生命/职责/目标/有效期拒绝消费。停止前增加 epoch，清掉自身 Schedule/Combat 订阅/弱记忆，再精确取消旧命令。人工接管保持阶段 A 的 Manual 语义；重生只恢复职责配置，清空旧知识/执行/故障记录。重复事件不重复提交，退避超限不重新启动树。

### 兼容、版本与迁移

旧 Profile 新字段默认 bEnablePerception=false；旧显式 Objective 树与资产继续通过 A 全套回归。新增职责 API 为服务器本地 API，不改网络/保存格式。新增 Profile 定义 ai_neutral_camp / ai_lane / ai_patrol，独立 `/Game/Combat/Demo/AI/Roles`，通过已有 CombatAIProfile 扫描。ADR-064 记录接口和 §15.4 完成式归位的等价编排选择。

## 4. 实施计划

| 文件/资产 | 变更 | 原因 | blast radius |
| --- | --- | --- | --- |
| AI/CombatAITypes、CombatAIProfileData | 感知/职责/排序与协议元数据 | 参数与观察分离 | 默认关闭兼容 A |
| AI/CombatAIRoleTypes、CombatAIRoleTasks、CombatAIRoleBrain、Brain 角色扩展 | 采样、纯排序、原子准备/取消/提交/条件 | 树唯一编排，复用 Bridge | 服务器 Brain |
| Validation/CombatAIAssetBuilder | 编排共享子树、三个根配置、活动路径验证 | 可编辑且可 cook | 仅 Editor 构建依赖 |
| Demo/CombatAIRoleDemoArena、Tools/setup_ai_roles.py、Roles 资产 | 演示与标识、出生视野 | 避免同名木桩与离屏误解 | 新地图 |
| Tests/CombatAIRoleTests、Role PIE/Network fixture、RunDedicated | 正反轨迹与网络回归 | 不引入生产测试旁路 | 验证设施 |
| 10-17、20-04、00-01、00-04、README | 当前 API 与边界/证据 | 区分 A/B 已实现与 C/D | 文档 |

## 5. 验收标准（AC）

- [x] AC-01：范围/LOS/关系合法性、隐藏位置不更新、记忆过期/上限、生命变化、受击信息限制与确定排序有正反例。
- [x] AC-02：真实树自动发现→持续攻击，普通刷新不重下单；失目标/越界→返回，到家确认一次，途中见敌/受击不退出。
- [x] AC-03：Lane 战斗打断不前进游标，成功到点才前进，战后恢复原路线；Patrol 循环与终点等待共享节点。
- [x] AC-04：路径失败三次后只等合法新职责，普通快照/受击不解锁，退避使用 Scheduler；到达证据和旧凭证不得误记成功。
- [x] AC-05：死亡/重生、玩家接管、Profile 更换、EndPlay/World teardown 清空订阅和调度；客户端无决策。
- [x] AC-06：角色资产编译保存回读、非法配置/多写入拒绝、独立地图在初始视野展示角色和明确靶子；PIE 与双客户端证明实际位移/伤害复制。
- [x] AC-07：三 Target、完整 Combat（由 A 的 112 项基线扩展至 121 项）、资产校验、角色地图 cook、文档与 delivery Gate 通过；未执行项已列出。

## 6. Definition of Done

- [x] 最小失败测试先执行，记录实际 Red，再实现与 Green。
- [x] F2 独立按 AC/竞态/旧回调/资产接线审查，必需发现关闭。
- [x] Tests / Types / No Regression / Adversarial / DDD / Decisions 六层证据齐全。
- [x] 保留阶段 A 的用户验收结论，不混入生成文件；阶段 B 已由用户验收，本地提交已获明确授权，不推送远端。

## 7. 测试矩阵与命令

| 层级 | 用例/命令 | 预期证据 | 结果 |
| --- | --- | --- | --- |
| 开工 | `python -X utf8 -B Tools/task_gate.py --mode preflight/plan/build --spec Doc/CombatSystem/Specs/AI-003-statetree-roles.spec.md --kind feature` | `Saved/AI-003/preflight.json`、`plan.json`、`build.json` 均 0 error | 通过 |
| 配置/World | `Automation RunTests Combat.AI`；角色知识、权限、生命代次、目标预算、归位/路线/退避用例 | `Saved/AI-003/ai-final.log`；20/20 success，含 `AIRolesPIE GuardMoved=1 LaneMoved=1 GuardHit=1 LaneHit=1 HomeCommits=1 RouteCommits=1` | 通过 |
| 构建 | `ue_gasEditor` / `ue_gasServer` / `ue_gasClient` Development | `Saved/AI-003/editor-final-build.log`、`server-final-build.log`、`client-final-build.log` 均 `Result: Succeeded` | 通过 |
| 回归 | `Automation RunTests Combat.` | `Saved/AI-003/combat-final.log`；121/121 success，0 fail | 通过 |
| 资产/PIE | `CombatAIAssets -Roles`、`setup_ai_roles.py`、`CombatAssetValidation`、Role PIE | `assets-validation-final.log`：37 assets/0/0；`demo-readback-final.log`；`roles-pie.png`；角色 PIE 通过 | 通过 |
| Network | `powershell -File Tools/RunDedicated.ps1 -InstalledEditor -AIRoles -TimeoutSeconds 90` | `Saved/UEEnvironment/Dedicated-Installed-AIRoles/`；服务器 `GuardMoved=1 LaneMoved=1 HomeCommits=1 RouteCommits=1`，两个客户端 Brain=Stopped 且伤害/血量复制通过 | 通过 |
| Cook | `Cook -TargetPlatform=Windows -Map=/Game/Combat/Demo/AI/Roles/L_CombatAI_Roles` | `Saved/AI-003/cook-roles-final.log`；`Success - 0 error(s), 1 warning(s)` | 通过 |
| 文档/交付 | `validate_docs.py`、`git diff --check`、`task_gate delivery` | 86 篇 Markdown、459 个本地链接、0 error；差异检查通过；`Saved/AI-003/delivery.json` 为 passed，173 changed files（含阶段 A 未提交内容） | 通过 |
| 附加工具单测 | `Tools/Tests/test_ue_environment.py` | 8 项中 1 项因 Windows 子进程输出解码 `UnicodeDecodeError` 出错；未修改该工具，不计入 AI 功能通过证据 | 未通过，已记录环境问题 |
| Soak/Perf | AI 64/128/256、网络损伤、打包后 exe | 属 C 或另行发布验证 | 未执行，不宣称通过 |

## 8. 风险、回滚与升级

- 风险：感知枚举 World 的规模成本、异步生命变化、任务瞬时循环、Route 到达与取消同帧、同队/隐藏来源信息、编辑器和源码引擎版本差异。
- 回滚方式：新角色撤下 Profile 或切回原 AI_Basic；仅回退 AI-003 新增字段/节点/资产及精确增量。阶段 A 未提交内容不得整体 git reset 或删除。
- 触发升级的条件：发现必须变更网络契约、引入完整迷雾、破坏既有公共 Order 行为、三轮同项失败不收敛时升级，不扩大到 C/D。
- 需要人决定的问题：当前无；用户已完成阶段 B 验收并授权本地提交，C/D 未开始。

## 9. 交付证据

- 代码/资产 diff：`Source/Combat/Combat/AI`、`Source/Combat/Combat/Demo/CombatAIRoleDemoArena.*`、`Source/Combat/Combat/Validation/CombatAIAssetBuilder.*`、`Source/Combat/Combat/Validation/CombatNavigationBuildCommandlet.*`、`Source/Combat/Combat/Tests/CombatAIRole*`、`Tools/setup_ai_roles.py`、`Tools/RunDedicated.ps1` 及 `/Game/Combat/Demo/AI/Roles` 角色资产。
- 构建与测试：`Saved/AI-003/` 保存三 Target 构建、121/121 全量回归、20/20 AI 专项、资产、PIE、导航和 cook 证据；Dedicated 双客户端证据在 `Saved/UEEnvironment/Dedicated-Installed-AIRoles/`。
- 验收归档：2026-09-21 用户确认后仅同步验收文档和准备本地提交；文档校验 86 篇/459 个本地链接、差异检查及 `Saved/AI-003/acceptance-delivery.json` 均通过。保留原 UE 构建与运行证据，不据本轮文档更新新增测试结论。
- F2 关闭记录：初轮资产缺失 Red、隐藏目标/当前目标预算、生命代次、归位锁定和路线游标等边界发现已修复；AI 专项测试最终 20/20。新 World Partition 地图的空 NavMesh 通过 `CombatNavigationBuild` 同步构建、保存并回读解决，之后 Dedicated 与 cook 均通过。
- 剩余风险：范围/LOS 精度不等于迷雾；感知仍枚举 World，现阶段不提供 AI 容量或长 soak 结论。

## 10. 变更记录

| 版本 | 日期 | 修改 | 原因 |
| --- | --- | --- | --- |
| 0.1 | 2026-09-21 | 阶段 B 初稿 | 用户继续授权 |
| 0.1 | 2026-09-21 | 完成阶段 B 角色实现、资产接线、独立地图、三 Target 构建、20/20 AI 专项、121/121 全量回归、PIE、Dedicated、导航构建与 cook 验证 | 用户继续授权后的实现与交付 |
| 0.1 | 2026-09-21 | 用户确认 B 阶段验收完成，授权本地 Git 提交；同步验收记录和最终交付检查 | 仅归档与提交已验证的 AI 变更，不重跑 UE 运行矩阵，不推送 |

## 11. 路由、自评与 Reflect

- 路由记录：通用 Combat 功能实现；主 Skill 与排除理由见 Intake。
- 自评分数：保留实施交付时分数：需求 4.8/5、架构 4.8/5、实现 4.7/5、验证 4.7/5、文档 4.7/5、卫生 4.6/5，按 20/20/20/20/10/10 加权后四舍五入为 4.7/5。扣分来自 World 枚举感知、未覆盖 AI 容量/长 soak/打包 exe，以及交付时尚待用户验收；本次验收不新增运行验证结论。
- 用户验收状态：2026-09-21 用户确认阶段 B 验收完成；阶段 A 的验收结论不变。
- Reflect：最有价值的修正是把角色演示地图的 NavMesh 构建变成可重复的 `CombatNavigationBuild` commandlet，并把客户端无 Brain 与真实伤害复制纳入 Dedicated smoke；不修改通用 Skill 规则。
