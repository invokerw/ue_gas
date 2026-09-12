# TOOL-001 UE 环境配置与 Dedicated 测试准入

> Spec 版本：`0.1`
> 状态：`READY_FOR_REVIEW`
> Owner：Codex
> 创建日期：2026-09-12
> 关联进度台账：工具/验证流程
> 风险等级：`L1`

## 0. Intake 与 Gate 记录

- 用户请求：不同机器的 UE 下载版编辑器与源码编译编辑器路径不同，希望由用户配置 env 文件，并依据配置决定是否允许 Dedicated Server 测试。
- 附件解释：`无附件`；实现依据仓库现有 Dedicated/Server/Client 验证命令和 M1 环境决策。
- 已读取入口：`agent.md`、`README.md`、`00-01-Progress-Tracker.md`、`00-03-Test-Plan.md`、`00-04-Decisions-Gaps.md`、`10-01-Scope-Architecture.md`、`90-02-M1-Environment-Decision.md`、`00-05-AI-Native-Development-Workflow.md`、`Skills/combat-task-router/SKILL.md`、`Skills/combat-feature-development/SKILL.md`
- 主 Skill：`combat-feature-development`
- 备选 Skill 与排除理由：`combat-task-router` 仅负责路由与复盘；`skill-creator` 不适用，本次不修改 Skill。
- 路由置信度：`high`
- F0 结论：`GO`
- F0 依据：这是仓库工具/流程能力，不改变运行时战斗、网络协议或资产；可以通过本地配置隔离机器差异。
- F1 结论：`APPROVED`
- F2 结论：`PASS`
- Push-Ready 结论：`READY`
- 验证：preflight 通过；工具单测 32/32；文档校验 62 Markdown、325 个本地链接；PowerShell 脚本解析通过；`compileall` 和 `git diff --check` 通过；缺少 `.env` 时 Dedicated 入口拒绝启动。
- 未执行：真实 UE Editor/Server/Client 构建、Automation、PIE 和 Dedicated smoke 未执行；本机没有用户 `.env`，Dedicated 入口检查返回 2 并在启动 UE 前拒绝，后续命令为 `Copy-Item .env.example .env` 后填写真实路径再运行 `Tools/RunDedicated.ps1`。

## 1. 目标与范围

### 目标

提供不提交机器绝对路径的 `.env` 配置入口，统一解析和校验 UE 下载版编辑器与源码编译编辑器，并让 Dedicated Server smoke 在源码编辑器未配置或路径无效时明确不可用。

### 范围

- 增加 `.env.example` 和 `.gitignore` 规则。
- 增加跨平台 Python 环境解析/校验工具。
- 增加使用已校验源码编辑器启动 Dedicated smoke 的 PowerShell 入口。
- 增加工具单元测试和 README 使用说明。

### Non-Goals

- 不修改 UE/Combat 运行时 C++、资产、网络协议或测试场景。
- 不自动发现或下载 UE，不覆盖用户已有 `.env`。
- 不把历史机器的绝对路径写入仓库。

## 2. 当前事实与依据

- 相关 DDD：`Doc/CombatSystem/90-History/90-02-M1-Environment-Decision.md`、`Doc/CombatSystem/00-Project/00-03-Test-Plan.md`。
- 代码事实（`file:line`）：`README.md:90-105` 只提供 `<UE_ROOT>` 占位命令；现有 `Saved/BottomHUD/RunDedicated.ps1` 使用硬编码安装版编辑器路径且位于被忽略的 `Saved/`。
- 当前测试/日志证据：M1 记录了安装版 UE 可运行独立 Server/Game，源码版 UE 可构建 Server/Client Target。
- 已知限制或待决策项：`.env` 是用户本地文件，Dedicated smoke 是否可运行由 `UE_SOURCE_EDITOR` 和其源码引擎 `Build.bat` 的有效性决定。

## 3. 行为与契约

### 主流程

1. 用户复制 `.env.example` 为 `.env`，填写 `UE_INSTALLED_EDITOR` 和 `UE_SOURCE_EDITOR` 的绝对路径。
2. `Tools/ue_environment.py` 读取 `.env`，展开引号并校验文件存在、可执行文件名和源码引擎 `Build.bat`。
3. 常规 Editor/Automation 使用下载版编辑器；Dedicated smoke 使用源码编译编辑器。
4. 校验命令以机器可读状态报告 `editor_ready`、`dedicated_ready` 和诊断列表；Dedicated 入口只在 `dedicated_ready` 时启动。

### 状态转换

`未配置 -> 配置不完整 -> 路径有效 -> Dedicated 可运行`；任何缺失、空值或路径失效均回到“不可运行”，不降级为使用仓库内默认路径。

### 输入、输出与数据约束

- 变量值为绝对文件路径；允许 PowerShell 单/双引号包裹。
- `UE_INSTALLED_EDITOR` 指向下载版 `UnrealEditor.exe`（或等价 Editor 可执行文件）。
- `UE_SOURCE_EDITOR` 指向源码编译的 `UnrealEditor.exe`；其上溯引擎根目录必须包含 `Engine/Build/BatchFiles/Build.bat`。
- `.env` 不提交；`.env.example` 只能包含占位符。

### 权威边界与权限

这是本地验证工具的准入检查，不改变 Combat 服务器权威逻辑；Dedicated smoke 进程仍由 UE/项目测试场景提供权威状态。

### 失败、取消、过期、死亡、EndPlay 与重复请求

路径检查失败时不启动 UE 进程；Dedicated 脚本在异常/超时 finally 中只清理本次启动的进程。重复运行不会修改 `.env` 或源码。

### 兼容、版本与迁移

保留现有 Target 名称和命令语义；旧的 Saved 脚本不作为仓库入口。已有用户可从 `.env.example` 迁移，未配置时 Dedicated 验证标记为未执行。

## 4. 实施计划

| 文件/资产 | 变更 | 原因 | blast radius |
| --- | --- | --- | --- |
| `.env.example` | 增加 UE 路径模板 | 给每台机器独立配置入口 | 仅本地工具 |
| `.gitignore` | 忽略 `.env`，保留示例 | 防止提交绝对路径 | Git 状态 |
| `Tools/ue_environment.py` | 解析、校验、状态输出 | 统一跨平台准入判断 | 工具调用 |
| `Tools/RunDedicated.ps1` | 从配置读取源码编辑器并启动 smoke | 移除硬编码路径 | Dedicated 流程 |
| `Tools/Tests/test_ue_environment.py` | 覆盖解析、校验和 Dedicated 准入 | 防止路径/状态回退 | 工具测试 |
| `README.md` | 记录配置、检查和运行命令 | 让用户可复现 | 文档 |

## 5. 验收标准（AC）

- [x] AC-01：仓库提供 `.env.example`，用户可以分别填写下载版和源码编译版编辑器路径，真实 `.env` 被忽略。
- [x] AC-02：环境检查能区分缺少配置、无效路径和 Dedicated 可运行，并且不使用任何历史绝对路径。
- [x] AC-03：Dedicated 入口只使用已校验的源码编辑器；配置不满足时不启动 UE，并给出可操作诊断。
- [x] AC-04：工具单测覆盖引号/注释解析、路径检查、Dedicated 准入和仓库路径未硬编码；文档包含实际命令和未配置时的结果含义。

## 6. Definition of Done

- [x] 行为或可执行逻辑变化已用工具单测验证；初始实现后修正路径规范化断言并增加历史路径扫描，最终 32/32 通过。
- [x] 实现通过直接测试，且没有绕过公共准入入口。
- [x] 相关 Editor/蓝图/资产已编译、保存并回读（不适用）。
- [x] 按风险完成 Editor、Server/Client、PIE、Dedicated 或 Soak 验证；本次工具变更不执行 UE 运行时层，Dedicated 未配置时已验证拒绝。
- [x] `10-01`–`10-12`、`00-04`、`90-16` 和公开中文说明已同步（仅工具文档适用）。
- [x] `git diff --check` 通过，未混入用户修改或生成文件。

## 7. 测试矩阵与命令

| 层级 | 用例/命令 | 预期证据 | 结果 |
| --- | --- | --- | --- |
| 文档/本地工具 | `$env:PYTHONUTF8='1'; python -B -m unittest discover -s Tools/Tests -v` | 全部工具测试通过 | 通过，32/32 |
| Pure/Unit | 同上 | 缺失/无效/有效配置状态正确 | 通过，新增用例 8/8 |
| World Automation | N/A | 不改 Combat Runtime | N/A |
| PIE / Blueprint | N/A | 不改资产或 PIE | N/A |
| Network / Dedicated | `& .\Tools\RunDedicated.ps1`；另执行 `python -B Tools/ue_environment.py --require dedicated --json` | 使用源码编辑器启动；未配置明确拒绝且不启动 UE | 未执行真实 smoke；无 `.env` 时检查返回 2，入口在 Start-Process 前拒绝 |
| Soak / Perf | N/A | 不改变运行时性能 | N/A |

## 8. 风险、回滚与升级

- 风险：用户填写的编辑器版本不匹配项目内容；脚本可能因本机 PowerShell 执行策略无法启动。
- 回滚方式：删除新增工具、`.env.example` 和文档变更，恢复 `.gitignore`。
- 触发升级的条件：需要支持非 Windows Dedicated 可执行入口、自动构建 Target 或修改网络测试协议。
- 需要人决定的问题：无；使用两个明确路径变量的默认方案。

## 9. 交付证据

- 代码/资产 diff：`.env.example`、`.gitignore`、`Tools/ue_environment.py`、`Tools/RunDedicated.ps1`、`Tools/Tests/test_ue_environment.py`、`README.md` 和本 Spec；未修改 C++ 或资产。
- 构建结果：不适用；本次没有 UE/C++ 变更。
- Automation/PIE/Dedicated 报告：`python -B Tools/ue_environment.py --require dedicated --json` 返回 2；`RunDedicated.ps1` 在 `Start-Process` 前拒绝，未创建 UE 进程。真实 Dedicated smoke 需用户填写 `.env` 后执行。
- 未执行验证及原因：真实 UE Editor/Server/Client 构建、Automation、PIE、Dedicated 和 Soak 未执行；本机没有用户 `.env`，且本任务只改本地准入工具。
- 剩余风险：不同 UE patch/content 版本仍需用户自行保持一致；PowerShell 执行策略或 UE 进程占用仍可能阻止实际 smoke。

## 10. 变更记录

| 版本 | 日期 | 修改 | 原因 |
| --- | --- | --- | --- |
| 0.1 | 2026-09-12 | 初稿 | 建立 UE 环境配置与 Dedicated 准入契约 |

## 11. 路由、自评与 Reflect

### 路由记录

- 原始需求摘要：通过 env 文件配置不同机器的 UE 下载版/源码版编辑器路径，并按设置决定 Dedicated Server 是否可测。
- 主 Skill：`combat-feature-development`
- 选择依据：需要新增工具、脚本、测试和 README 流程，属于工具/流程实现。
- 备选 Skill 与排除理由：`combat-task-router` 仅作路由；`skill-creator` 不适用。
- 路由置信度：`high`

### 交付自评

| 维度 | 权重 | 分数（0–5） | 证据与扣分原因 |
| --- | ---: | ---: | --- |
| 需求与 AC | 20% | 4.8 | 两个编辑器路径、Dedicated 条件、失败诊断和本地模板均闭合；真实 UE smoke 依赖用户配置 |
| 架构与权限 | 20% | 4.7 | 不触碰 Combat 权威边界，入口只在准入成功后启动 UE；未覆盖非 Windows Target |
| 实现与数据 | 20% | 4.6 | 解析、路径归一化、Build.bat 检查和 JSON schema 有单测；未实现自动发现/版本匹配 |
| 验证证据 | 20% | 4.0 | 工具 31/31、文档、PowerShell 解析和拒绝路径通过；真实 UE/Dedicated 因无 `.env` 未执行 |
| 文档与可观测性 | 10% | 4.8 | README、Spec、状态诊断、日志目录和进程摘要已说明 |
| 交付卫生 | 10% | 4.8 | `.env` 忽略、示例无历史路径、diff check 通过，未混入生成文件 |

- 计算总分：4.6 / 5.0
- 硬性封顶或未执行项：真实 UE/Dedicated 属于用户本机配置后的条件验证，本任务已验证未配置时安全拒绝；不把未执行的运行时验证记为通过。
- 自评结论：`EVIDENCE_SUFFICIENT`
- 用户验收状态：`READY_FOR_REVIEW`

### Reflect 与调优

- 观察与证据：README 原先要求手工替换 `<UE_ROOT>`，被忽略的 `Saved/BottomHUD/RunDedicated.ps1` 还包含固定安装路径；新增工具单测 32/32、缺失 `.env` 时 Dedicated 入口返回 2 并在启动前拒绝。
- 根因类别：`单次实现`
- 调整文件与预期收益：`.env.example`、`Tools/ue_environment.py`、`Tools/RunDedicated.ps1`、README 和 Tracker；让机器差异显式化，避免硬编码路径误测 Dedicated。
- 回归验证：文档校验 62 Markdown/325 本地链接、工具单测 32/32、PowerShell parse、Python compileall、`git diff --check` 和入口缺失配置拒绝均通过。
- 需要用户决定的问题：无。
