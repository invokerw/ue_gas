# HUD-ABILITY-CLICK-002 右键取消后技能首击被视口捕获

> Spec 版本：`0.2`
> 状态：`已验收`
> Owner：Codex
> 创建日期：2026-09-14
> 关联进度台账：`Doc/CombatSystem/00-Project/00-01-Progress-Tracker.md`
> 风险等级：`L1`

## 0. Intake 与 Gate 记录

- 用户请求：技能 HUD 进入指示器后右键取消，再点击另一个技能需要两次；定位并修复首击失效。
- 附件解释：`无附件`；依据用户描述和本地 PIE 行为复现定义验收路径。
- 已读取入口：`agent.md`、`00-01`、`00-03`、`00-04`、`00-05`、`10-01`、`10-09`、`10-12`、`10-13`、`90-16`、任务路由 Skill、主执行 Skill。
- 主 Skill：`Skills/combat-feature-development/SKILL.md`
- 备选 Skill 与排除理由：`combat-skill-development` 不存在；本任务是现有 HUD/输入 Bug 修复，不是新技能定义。
- 路由置信度：`high`
- F0 结论：`GO`
- F0 依据：问题可在本地 PIE 确定复现，改动限于本地 HUD 输入调度与回归测试，不改变服务器结算契约。
- F1 结论：`APPROVED`
- F1 审查人：Codex
- F1 审查版本：`0.2`
- F1 计划审查证据：范围已重审为 HUD 点击下一帧调度与焦点冲刷保护；AC、测试和回滚见第 3–8 节，计划重审 Gate 已通过后解锁 Build。
- Build 解锁：`已解锁`；计划重审 Gate `Saved/HUDAbilityClick/plan-002-rereview.json` 通过
- F1 重审条件：范围、架构、权限、迁移、测试矩阵或回滚方案发生实质变化时递增 Spec 版本并重新审查。
- F2 结论：`PASS`
- F2 审查证据：实现逐项满足 AC-01 至 AC-03；HUD 请求仍调用统一 Controller Ability 入口，`PendingHUDAbilityTimer` 只延迟本地启动，`bFlushingPressedKeys` 仅保护焦点冲刷期间的排队请求，显式取消仍清理队列；未新增 RPC、Order 或资源旁路。
- Push-Ready 结论：`READY`
- 用户验收：`2026-09-14` 已完成 PIE 复验并确认首击换槽行为，授权本地 Git 提交。
- 验证：已通过 UE MCP 在 PIE 复现基线：点击技能图标 → 右键世界取消 → 另一技能第一次点击光标仍为 `Default`，第二次为 `Crosshairs`；日志确认首击已到 HUD 后被 `FlushPressedKeys` 清理。修复后的首击验证与 Automation 见第 7、9 节。
- 未执行：Dedicated/Soak 增量不适用；训练场视觉与目标确认手感已由用户完成验收。

## 1. 目标与范围

### 目标

右键取消技能瞄准后立即点击任意其他 HUD 技能槽，第一次左键就应进入该技能的瞄准会话。

### 范围

修复 `ACombatPlayerController` 在 GameAndUI 焦点切换时冲刷掉 HUD 新技能会话的问题；HUD 点击请求延后一帧执行，增加输入回归断言，补写相关 Spec/决策和验证证据。

### Non-Goals

不改变技能槽映射、Ability 激活、目标校验、服务器 Order/RPC、技能结算、物品拖放或普通移动语义。

## 2. 当前事实与依据

- 相关 DDD：`10-09` 客户端输入与服务器 Order、`10-12` HUD 交互、`10-13` 技能指示器、`00-03` 测试准入。
- 代码事实（`file:line`）：`Source/Combat/CombatPlayerController.cpp` 的 `BeginDestinationInput` 在右键开始时调用 `AbilityAimComponent->ResetLocalState()`；`Source/Combat/Combat/UI/CombatHUDSlotWidget.cpp` 左键在 MouseDown 广播技能请求；`Source/Combat/Combat/UI/CombatHUDWidget.cpp` 转发到统一 `ActivateCombatAbilitySlotFromHUD`。
- 当前测试/日志证据：UE MCP PIE 序列中首击日志为 `HUD ability click: slot=2`，紧接 `Combat input flush: aiming=1 slot=2`，说明 HUD 请求已启动后被焦点切换冲刷；第二次点击不再触发该冲刷。
- 已知限制或待决策项：修复只改变本地 HUD 输入调度与 Flush 保护，不改变 Slate/网络协议；真实用户可继续在训练场复验视觉与目标确认。

## 3. 行为与契约

### 主流程

1. HUD 技能槽左键请求沿现有公共入口开始本地瞄准。
2. 右键世界输入进入 `BeginDestinationInput`，识别技能瞄准并取消本地会话。
3. HUD 左键请求排到下一帧，等待焦点切换和按键冲刷完成。
4. 后续任一 HUD 技能槽第一次左键进入新的瞄准会话。

### 状态转换

`Idle → Aiming(Q/W/E/R) → Idle(CancelledByRightClick)`；HUD 请求经过 `PendingHUDAbilityTimer` 延后一帧，`FlushPressedKeys` 不得取消该排队请求，随后 `Idle → Aiming(other slot)` 不需要额外点击。

### 输入、输出与数据约束

仅调整本地 HUD 输入调度和 Aim 本地状态清理；不新增请求 ID、不复用旧 SessionSerial、不提交额外移动/技能 Order。

### 权威边界与权限

客户端只取消本地意图和 Slate 输入捕获；技能激活仍经 Controller 现有公共 HUD 入口，目标与权限仍由既有 Ability/Aim/服务器链路裁决。

### 失败、取消、过期、死亡、EndPlay 与重复请求

右键取消幂等；无技能瞄准时保持既有右键移动逻辑；显式右键、Stop、换绑、死亡和 EndPlay 清理排队请求；焦点切换的 `FlushPressedKeys` 只清理旧输入，不取消同一切换中已排队的 HUD 点击；重复释放不产生请求。

### 兼容、版本与迁移

无资产、网络协议或存档迁移；兼容 UE 5.8.2 当前 Slate API。

## 4. 实施计划

| 文件/资产 | 变更 | 原因 | blast radius |
| --- | --- | --- | --- |
| `Source/Combat/CombatPlayerController.cpp` | HUD 技能入口使用下一帧 Timer；`FlushPressedKeys` 保护排队请求；显式右键先清理 Timer | 避免 GameAndUI 焦点切换在 HUD MouseDown 后冲刷新技能会话 | 所有本地 HUD 技能点击与焦点切换 |
| `Source/Combat/Combat/Tests/CombatPlayerInputTests.cpp` | 增加取消后换槽首击回归断言 | 防止捕获清理回归 | Editor 输入测试 |
| `Doc/CombatSystem/Specs/HUD-ABILITY-CLICK-002-cancel-reclick.spec.md` | 记录 AC、根因和验证证据 | 可回读交付契约 | 文档 |
| `Doc/CombatSystem/00-Project/00-04-Decisions-Gaps.md` | 追加取消后 Slate 捕获清理决策 | 固化输入边界 | 文档 |
| `Doc/CombatSystem/10-Architecture/10-09-Client-Server-Interaction.md` | 记录本地捕获清理不改变 Order 权威链 | 保持客户端/服务器边界说明一致 | 文档 |
| `Doc/CombatSystem/10-Architecture/10-12-Bottom-HUD-Design.md` | 补充取消后首击行为 | HUD 交互契约 | 文档 |
| `Doc/CombatSystem/10-Architecture/10-13-Skill-Indicators.md` | 补充取消后可立即换槽 | 指示器生命周期契约 | 文档 |

## 5. 验收标准（AC）

- [x] AC-01：标准技能瞄准右键取消后，任一其他 HUD 技能槽第一次左键即进入 `Crosshairs` 瞄准状态。
- [x] AC-02：右键取消不产生移动或技能 Order；第二次点击不被要求用于“唤醒”输入路由。
- [x] AC-03：无瞄准状态的右键移动、Escape/失焦/死亡清理和物品 HUD 行为保持现状。

## 6. Definition of Done

- [x] 行为或可执行逻辑变化已用输入回归测试/PIE 复现验证，并记录实际 Red/Green 原因。
- [x] 实现通过直接测试，且没有绕过公共技能入口或服务器权威链。
- [x] 相关 Editor/蓝图/资产已编译、保存并回读（本 Bug 不改资产）。
- [x] 按风险完成 Editor、PIE 验证；Dedicated/Soak 对本地调度修复不适用并明确记录。
- [x] `10-09`、`10-12`、`10-13`、`00-04` 和公开中文说明已同步。
- [x] `git diff --check` 通过，未混入用户修改或生成文件。

## 7. 测试矩阵与命令

| 层级 | 用例/命令 | 预期证据 | 结果 |
| --- | --- | --- | --- |
| 文档/本地工具 | `python -B Tools/validate_docs.py`；`git diff --check` | 链接与空白校验 | PASS，72 Markdown / 376 links |
| Pure/Unit | `Combat.Input.AbilityAim.StandardAndCancel` 扩展 HUD 队列经过 Flush 后下一帧启动 | 输入测试日志 | PASS，7/7 `Combat.Input.*` |
| World Automation | `Automation RunTests Combat.` | 无回归 | PASS，82 succeeded + 1 existing warning / 0 failed |
| PIE / Blueprint | UE MCP Slate：技能 → 右键 → 另一技能单击 | 首击 `Crosshairs`，无第二次点击 | PASS，E1 在 100 ms/500 ms 均为 `Crosshairs` |
| Network / Dedicated | N/A：只清理本地 Slate 捕获，不改变网络协议 | 记录 N/A | N/A |
| Soak / Perf | N/A：无持续运行时代码或资源变化 | 记录 N/A | N/A |

## 8. 风险、回滚与升级

- 风险：HUD 点击延后一帧；队列仅保留最后一次点击，显式取消会清理队列。
- 回滚方式：回退 `PendingHUDAbilityTimer`、`bFlushingPressedKeys` 保护和对应测试/文档，恢复同步 HUD 激活。
- 触发升级的条件：PIE 首击仍失败、右键产生意外移动/Order、或发现其他输入手势依赖该捕获时升级审查。
- 需要人决定的问题：无。

## 9. 交付证据

- 代码/资产 diff：`CombatPlayerController.cpp/.h` 与 `CombatPlayerInputTests.cpp`；未修改 `.uasset`、网络载荷或发布契约。
- 构建结果：安装版 UE 5.8.2 Editor Development PASS；最终增量构建 4 actions。
- Automation/PIE/Dedicated 报告：`Saved/HUDAbilityClick/Automation-Input-005/index.json`（7/7）、`Automation-Full-003/index.json`（82+1 warning/0 fail）；真实 PIE 末次序列 W → 右键 → E 单击首击 `Crosshairs`。
- 文档/流程报告：`Saved/HUDAbilityClick/preflight.json`、`plan-002-rereview.json`、`build-002-final.json`、`delivery-002-final.json` 均通过；`validate_docs.py` 72/376，`git diff --check` PASS。
- 未执行验证及原因：Dedicated/Soak 增量不适用（无网络、Tick 或资源变化）；用户已完成真实 PIE 视觉与目标确认验收。
- 剩余风险：HUD 请求引入一帧本地调度延迟，若同帧连续点击只保留最后一次。

## 10. 变更记录

| 版本 | 日期 | 修改 | 原因 |
| --- | --- | --- | --- |
| 0.1 | 2026-09-14 | 初稿 | 记录右键取消后技能首击失效 Bug |
| 0.2 | 2026-09-14 | 重审根因并改为 HUD 下一帧调度，完成构建、Automation 与真实 PIE 首击验证 | `FlushPressedKeys` 在焦点切换后清掉新技能会话 |

## 11. 路由、自评与 Reflect

### 路由记录

- 原始需求摘要：右键取消技能指示器后点击另一个技能需要两次。
- 主 Skill：`combat-feature-development`
- 选择依据：现有 Combat HUD/输入行为 Bug 修复，需要 Spec、代码、测试和 PIE 验证。
- 备选 Skill 与排除理由：无其他适用 Skill。
- 路由置信度：`high`

### 交付自评

| 维度 | 权重 | 分数（0–5） | 证据与扣分原因 |
| --- | ---: | ---: | --- |
| 需求与 AC | 20% | 5.0 | AC-01～03 由定向 Automation 与真实 PIE 首击验证覆盖 |
| 架构与权限 | 20% | 5.0 | 只调度本地 HUD 请求，复用统一 Controller 入口，无 RPC/结算旁路 |
| 实现与数据 | 20% | 4.5 | Timer 与 Flush 保护已编译；不改资产，保留一帧本地延迟 |
| 验证证据 | 20% | 4.5 | UE 5.8.2 构建、输入 7/7、全量 82+1 warning/0 fail、PIE 首击通过 |
| 文档与可观测性 | 10% | 5.0 | Spec、ADR-057、10-09/10-12/10-13、README/台账已同步 |
| 交付卫生 | 10% | 5.0 | plan/build/delivery Gate、文档校验与 diff check 通过 |

- 计算总分：4.8 / 5.0
- 硬性封顶或未执行项：Dedicated/Soak 增量不适用；真实 PIE 已完成用户验收。
- 自评结论：`READY_FOR_REVIEW`
- 用户验收状态：`用户已验收`（2026-09-14），并授权本地 Git 提交。

### Reflect 与调优

- 观察与证据：PIE 日志显示首击先记录 HUD 槽位，再由 `FlushPressedKeys` 清理 `aiming=1 slot=2`；修复后 E 首击在 100 ms 与 500 ms 均为 `Crosshairs`。
- 根因类别：`输入焦点生命周期`
- 调整文件与预期收益：Controller HUD 请求下一帧执行并保护 Flush 阶段的排队 Timer，避免焦点切换撤销新会话。
- 回归验证：`Combat.Input.*` 7/7、全量 82+1 warning/0 fail、UE MCP PIE 首击通过；文档 72/376 通过。
- 需要用户决定的问题：无
