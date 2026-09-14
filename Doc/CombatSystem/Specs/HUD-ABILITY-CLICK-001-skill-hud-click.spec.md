# HUD-ABILITY-CLICK-001 HUD 技能槽点击施法

> Spec 版本：`0.1`
> 状态：`已验收`
> Owner：Codex
> 创建日期：2026-09-14
> 关联进度台账：[00-01](../00-Project/00-01-Progress-Tracker.md)
> 风险等级：`L1`

## 0. Intake 与 Gate 记录

- 用户请求：物品可以在 HUD 点击使用，但技能不行；检查并修改为技能也可通过 HUD 点击使用。
- 附件解释：无附件；需求仅来自用户文字，不引入视觉参考或外部工程约束。
- 已读取入口：`agent.md`、`README.md`、`00-01`、`00-03`、`00-04`、`00-05`、`10-01`、`10-03`、`10-09`、`10-12`、`90-16`、`Skills/combat-task-router/SKILL.md`、`Skills/combat-feature-development/SKILL.md`、`Skills/combat-skill-development/SKILL.md`、`20-02`、`20-03`。
- 主 Skill：`Skills/combat-feature-development/SKILL.md`
- 备选 Skill 与排除理由：`combat-skill-development`；本轮不新增/迁移 Ability、DataAsset、Modifier、Projectile 或技能结算逻辑，只接入已有 HUD 输入。
- 路由置信度：`high`
- F0 结论：`GO`
- F0 依据：用户明确要求修复可观察的 HUD 技能点击行为；现有物品点击已提供可复用入口，改动局部且可回滚，不改变服务器 Order/Ability 契约。
- F1 结论：`APPROVED`
- F1 审查人：Codex
- F1 审查版本：0.1
- F1 计划审查证据：已通过 plan Gate；逐项审查了技能槽点击、升级按钮优先级、目标技能瞄准/取消、Owner/LifeGeneration 校验、唯一 SubmitCombatOrder 入口、无网络/结算旁路、回滚和测试矩阵。批准范围为 Spec 0.1，行为文件尚未修改。
- Build 解锁：`已解锁`；F1 批准后运行 build Gate，结果写入 `Saved/HUDAbilityClick/build.json`。
- F1 重审条件：范围、架构、权限、迁移、测试矩阵或回滚方案发生实质变化时先递增版本，F1 回到 `REVISE`、任务状态回到 `PLAN_REVIEW`；重审通过前不得继续对应代码修改。
- F2 结论：`PASS`
- F2 审查证据：实现逐项复核了 AC-01 至 AC-05；技能槽只发本地槽位意图，Controller 复用既有技能激活/瞄准链路，未增加结算或网络旁路；升级按钮优先级、空槽、右键详情/取消和委托清理均有代码复核，点击请求与空槽/右键边界由自动化覆盖。
- Push-Ready 结论：`READY`
- 验证：已完成安装版 UE 5.8.2 Editor Development 构建、HUD/Input 定向 Automation、全量 `Combat.` Automation、文档校验和 diff 检查；结果见第 7 节和 `Saved/HUDAbilityClick/`。
- 未执行：Network/Dedicated 增量验证不适用；真实交互 PIE 已由 HUD-ABILITY-CLICK-002 的后续回归完成，用户已确认技能点击、右键取消和换槽首击行为。未新增 RPC、复制字段或结算逻辑。

## 1. 目标与范围

### 目标

让底部 HUD 的主动技能槽支持左键点击，行为与对应 Q/W/E/R 技能输入一致：无目标技能立即提交，目标技能进入已有本地瞄准并由世界确认，AutoCast 技能沿已有服务器切换入口处理。

### 范围

- 技能槽识别自身为 Ability 展示项，并发出一次带槽位来源的本地使用请求。
- HUD 将技能槽请求映射到当前拥有者的技能索引，调用 Controller 现有 `ActivateCombatAbilitySlot` 逻辑。
- 保留升级按钮优先级、悬停详情和右键取消/输入消费；技能点击不得向世界穿透产生 Move/Attack。
- 增加直接自动化覆盖技能槽点击请求与 Controller 入口映射。
- 同步 HUD 设计、README、技能槽交互说明和进度台账事实。

### Non-Goals

不改变 Ability DataAsset、GameplayTag、Targeting、Cost/Cooldown、服务器 RPC、Order 状态机、技能数值、物品逻辑、技能快捷键映射、蓝图布局资产或发布契约。

## 2. 当前事实与依据

- `Source/Combat/Combat/UI/CombatHUDSlotWidget.cpp:210-229`：技能/Buff 槽左键只广播详情固定，未向 Controller 发出技能使用请求；右键消费输入。
- `Source/Combat/Combat/UI/CombatHUDItemSlotWidget.cpp:78-101`：物品槽左键记录快照并在 MouseUp 调用 `ACombatPlayerController::UseInventoryItem`，作为可复用的 HUD 本地请求模式。
- `Source/Combat/Combat/UI/CombatHUDWidget.cpp:52-74, 440-474`：主 HUD 绑定技能槽详情/升级委托，父级 Preview 处理 UI 右键取消。
- `Source/Combat/CombatPlayerController.cpp:497-578`：`ActivateCombatAbilitySlot` 已完整处理无目标、AutoCast、目标技能瞄准、QuickPress 与既有提交入口，但当前仅由 Enhanced Input 调用。
- `Doc/CombatSystem/10-Architecture/10-12-Bottom-HUD-Design.md:45-48`：当前文档明确技能点击只固定详情，与用户新需求冲突，需同步为点击施法。
- 当前测试：`Combat.UI.HUD.*` 覆盖显示/升级/生命周期及技能点击，`Combat.Input.AbilityAim.*` 覆盖键盘技能输入与瞄准取消；新增 `Combat.UI.HUD.SkillClickRequest` 覆盖 HUD 点击边界。

## 3. 行为与契约

### 主流程

HUD 技能槽左键按下 → 槽位确认是已显示 Ability 且不是升级按钮 → 发出 `OnAbilityUseRequested(Source)` → 主 HUD 根据固定 Q/W/E/R 数组解析槽位 → 本地 Controller 调用 `ActivateCombatAbilitySlot(Index)` → 复用现有无目标提交或 `UCombatAbilityAimComponent::BeginAim` → 目标技能仍由世界左键确认，现有 `ConfirmAbilityTarget` 单次提交。

### 状态转换

`Idle → (HUD click) Idle`（无目标/AutoCast直接沿现有流程）或 `Idle → Aiming → Submitted/Idle`（目标技能）；升级按钮点击仍为升级请求；空槽点击保持 UI 消费且不发送请求。

### 输入、输出与数据约束

点击只携带本地四槽索引，不提交 Ability 类、DefinitionId、等级、目标 Actor、伤害或资源数值。Controller 再从当前主控 Unit/ASC 解析授予的 Spec；切换主控、死亡、撤销技能、焦点丢失和旧绑定均由现有 Aim/Owner/LifeGeneration 校验淘汰。技能槽左键不再固定详情；悬停仍显示详情，右键在未瞄准时固定详情，瞄准期间右键继续取消会话。

### 权威边界与权限

HUD 和 Controller 仅产生本地意图；所有 Cost、Cooldown、Targeting、Ability 激活和结算继续由服务器公共 Order/ASC/Ability 管线复核。客户端不直接写 ASC/属性，不增加 RPC。

### 失败、取消、过期、死亡、EndPlay 与重复请求

空槽、无 Owner、非本地 Controller、非存活单位或升级按钮命中时不产生技能使用请求。目标技能点击后沿现有会话处理，Esc/右键取消；重复点击由现有 `CancelAim`/SessionSerial 和提交入口约束，不重放旧目标。HUD/Unit EndPlay 清理委托，旧槽位回调不能写入新 HUD。

### 兼容、版本与迁移

不改变 GameplayTag、DefinitionId、Event/Content/Formula、HUD View schema 或网络协议；仅改变 HUD 鼠标交互语义。原有 Q/W/E/R 快捷键和物品点击保持兼容。回滚本任务 UI/Controller/测试/文档增量即可恢复点击固定详情行为。

## 4. 实施计划

| 文件/资产 | 变更 | 原因 | blast radius |
| --- | --- | --- | --- |
| `Source/Combat/Combat/UI/CombatHUDSlotWidget.h/.cpp` | 增加 Ability 使用委托与 Ability/Modifier 展示标记；技能左键发使用请求，右键可固定技能详情 | 让技能槽拥有与物品槽等价的本地点击入口 | HUD 技能/Buff 槽 |
| `Source/Combat/Combat/UI/CombatHUDWidget.h/.cpp` | 绑定/解绑技能使用委托，按四槽来源转发给 Controller；调整右键 Preview 让未瞄准技能槽可处理详情 | 复用主 HUD 的槽位索引和输入生命周期 | 底部 HUD 输入 |
| `Source/Combat/CombatPlayerController.h/.cpp` | 增加受控的 HUD 技能槽入口，调用现有 Ability 激活函数 | 保持唯一 Ability 输入实现与权限边界 | 本地 Controller |
| `Source/Combat/Combat/Tests/CombatHUDTests.cpp` | 新增技能槽左键委托、升级按钮优先级和右键详情回归 | 证明点击行为及边界 | HUD 自动化 |
| `Doc/CombatSystem/10-Architecture/10-12-Bottom-HUD-Design.md`、`README.md`、`Doc/CombatSystem/00-Project/00-01-Progress-Tracker.md` | 同步技能点击操作说明和验证状态 | 避免文档与实现矛盾 | 项目文档 |
| 本 Spec | 记录 Gate、测试、F2、交付证据 | 流程与可回读依据 | 任务记录 |

## 5. 验收标准（AC）

- [x] AC-01：对有 Ability 的技能槽左键点击会触发现有技能激活入口；空槽不发送技能请求。
- [x] AC-02：无目标技能/AutoCast/目标技能分别保持现有快捷键语义；目标技能仍需世界目标确认，点击槽本身不产生 Move/Attack。
- [x] AC-03：升级按钮命中只请求升级，不同时使用技能；悬停详情可用，未瞄准时右键可固定详情，瞄准时右键可取消。
- [x] AC-04：切换主控、死亡、撤销技能、HUD 重建/EndPlay 后旧槽位委托不产生请求；不引入新的网络/结算旁路。
- [x] AC-05：Editor 构建、相关 HUD/Input Automation、文档校验和 diff 检查通过；适用的 PIE/网络验证如实记录。

## 6. Definition of Done

- [x] 行为或可执行逻辑变化已用相关测试/Golden Case 验证，并记录实际 Red/Green。
- [x] 实现通过直接测试，且没有绕过公共权威入口。
- [x] 相关蓝图/资产无需改动；自动化加载并实例化真实 `WBP_CombatHUDSkill` 类完成回归。
- [x] 按风险完成 Editor、PIE、Network/Dedicated 或明确记录不适用/未执行原因。
- [x] 10-12、README、00-01 与公开中文说明已同步。
- [x] `git diff --check` 通过，未混入用户修改或生成文件。

## 7. 测试矩阵与命令

| 层级 | 用例/命令 | 预期证据 | 结果 |
| --- | --- | --- | --- |
| 文档/流程 | `task_gate.py preflight/plan/build/delivery`、`validate_docs.py`、`git diff --check` | `Saved/HUDAbilityClick/*Gate.json` | preflight/plan/build/delivery PASS；文档校验 71 文件/375 链接 PASS；diff check PASS（仅 CRLF 提示） |
| Types/Build | 安装版 UE 5.8.2 `ue_gasEditor Win64 Development` | `Saved/HUDAbilityClick/EditorBuild-Installed.log` | PASS，12 actions；最终增量构建 4 actions，退出码 0 |
| Pure/Unit | `Combat.UI.HUD.SkillClickRequest` | 点击委托、空槽/升级按钮/右键边界 | PASS，HUD 定向 5/5；新增用例通过 |
| World Automation | `Combat.Input.AbilityAim.*` 与全量 `Combat.` | 快捷键与 HUD 点击回归 | AbilityAim 4/4；全量 Combat 83/83（1 个既有开发命令警告） |
| PIE / Blueprint | Demo HUD 技能槽点击、目标确认、取消、物品/加点回归 | 实际输入日志/截图 | PASS；后续 HUD-ABILITY-CLICK-002 真实 PIE 覆盖 W → 右键 → E 首击进入 `Crosshairs`，自动化加载真实技能槽 Widget Blueprint |
| Network / Dedicated | 不新增 RPC；若环境可用运行现有 smoke | 服务器仍复核请求 | 不适用增量验证；本轮未改复制、RPC、Schema 或结算链路 |
| Soak / Perf | 不新增 gameplay Tick/Timer；无需专项 soak | 代码审查 | 不适用 |

## 8. 风险、回滚与升级

- 风险：技能点击改变原有详情固定语义；UI 事件顺序可能让同一鼠标手势穿透世界；升级按钮命中可能重复触发。
- 回滚方式：撤销本 Spec 记录的 UI/Controller/测试/文档增量，恢复技能槽左键详情固定。
- 触发升级的条件：需要改动 Ability/Order/RPC/网络协议、HUD View schema 或新增资产时，递增 Spec 版本并重新 F1 审查。
- 需要人决定的问题：无；用户需求已授权可回滚的本地 HUD 输入修复。

## 9. 交付证据

- 代码/资产 diff：13 个 C++/Markdown 文件与本 Spec；未修改 `.uasset`、网络载荷或发布契约。
- 构建结果：安装版 UE 5.8.2 Editor Development PASS，日志为 `Saved/HUDAbilityClick/EditorBuild-Installed.log`；源码 UE 构建因引擎变更检测触发全量引擎动作后中止，不影响安装版构建结果。
- Automation 报告：`Saved/HUDAbilityClick/Automation-HUD-Final/index.json`（5/5）、`Automation-Input/index.json`（4/4）、`Automation-Full/index.json`（83/83，1 个既有警告）。
- 文档/流程报告：`Saved/HUDAbilityClick/preflight.json`、`plan.json`、`build.json`、`delivery-001-final.json` 均通过；`validate_docs.py` 通过 72 个 Markdown/376 个本地链接；`git diff --check` 通过。
- 未执行验证及原因：Network/Dedicated 增量验证不适用；真实交互 PIE 已由 HUD-ABILITY-CLICK-002 的后续回归完成，用户已确认点击后的视觉反馈、目标技能确认和右键取消行为。
- 剩余风险：目标技能 HUD 请求保留一帧本地调度延迟，具体焦点切换约束与回归证据见 HUD-ABILITY-CLICK-002；不改变网络或结算链路。

## 10. 变更记录

| 版本 | 日期 | 修改 | 原因 |
| --- | --- | --- | --- |
| 0.1 | 2026-09-14 | 建立 F0/PLAN 与点击施法范围 | 用户 Bug 请求 |
| 0.1 | 2026-09-14 | 完成 HUD/Controller 接入、自动化与文档同步，记录 F2/Push-Ready | 实现与交付验证 |
| 0.2 | 2026-09-14 | 用户完成 HUD 技能点击与取消换槽验收并授权本地提交 | 后续首击回归由 HUD-ABILITY-CLICK-002 覆盖 |

## 11. 路由、自评与 Reflect

### 路由记录

- 原始需求摘要：物品可以在 HUD 点击使用，技能无法点击使用，要求检查并修改。
- 主 Skill：combat-feature-development。
- 选择依据：问题位于 HUD 槽位鼠标事件与既有 Controller 技能入口之间，不涉及新技能内容。
- 备选 Skill 与排除理由：combat-skill-development；没有 Ability/DataAsset/结算语义变更。
- 路由置信度：high。

### 交付自评

| 维度 | 权重 | 分数（0–5） | 证据与扣分原因 |
| --- | ---: | ---: | --- |
| 需求与 AC | 20% | 4.5 | AC-01～05 已由实现审查和 Editor/Automation 覆盖；真实 PIE 尚待用户确认 |
| 架构与权限 | 20% | 5.0 | HUD 只转发本地槽位索引，复用唯一 Controller Ability 入口，无 RPC/结算旁路 |
| 实现与数据 | 20% | 4.5 | 技能/空槽/升级/右键边界已实现并编译；未改资产 |
| 验证证据 | 20% | 4.5 | Editor、定向及全量 Automation 通过；后续真实交互 PIE 已覆盖技能点击、取消与换槽首击 |
| 文档与可观测性 | 10% | 5.0 | README、10-09、10-12、10-13、ADR-056、进度台账与 Spec 已同步，文档校验通过 |
| 交付卫生 | 10% | 5.0 | delivery Gate、diff check 通过；无提交/推送，未混入生成文件 |

- 计算总分：4.7 / 5.0。
- 硬性封顶或未执行项：Network/Dedicated 增量不适用；真实交互 PIE 已完成并由用户确认。
- 自评结论：`READY_FOR_REVIEW`
- 用户验收状态：`用户已验收`（2026-09-14），并授权本地 Git 提交。

### Reflect 与调优

- 观察与证据：开工基线显示技能槽左键只固定详情，物品槽通过 Controller 使用接口；后续 PIE 发现焦点切换会冲刷首个 HUD 技能会话。
- 根因类别：`输入焦点生命周期`
- 调整文件与预期收益：技能槽沿统一 Controller 入口接入；后续由 HUD-ABILITY-CLICK-002 增加下一帧调度和 Flush 保护，避免取消后换槽首击失效。
- 回归验证：HUD 5/5、AbilityAim 4/4、全量 Combat 82+1 warning/0 fail、真实 PIE 首击通过；文档 72 文件/376 链接通过。
- 需要用户决定的问题：无。
