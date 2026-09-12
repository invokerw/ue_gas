# REF-001 模板 C++ 文件改用 Combat 前缀

> Spec 版本：`0.1`
> 状态：`READY_FOR_REVIEW`
> Owner：Codex
> 创建日期：2026-09-12
> 关联进度台账：[00-01](../00-Project/00-01-Progress-Tracker.md)
> 风险等级：`L1`

## 0. Intake 与 Gate 记录

- 用户请求：修改 ue_gas 相关开头的几个 h、cpp 文件，替换为 Combat。
- 附件解释：无附件；用户提供的 AGENTS.md 是整个仓库的工程约束。
- 已读取入口：根目录 `agent.md`、`README.md`、00-01 台账、00-03 测试计划、00-04 决策、00-05 开发流程、10-01 架构 DDD、10-09 联机交互、10-10 移动约束、20-02/20-03 公开扩展指南，以及 `Skills/combat-task-router/SKILL.md` 与 `Skills/combat-feature-development/SKILL.md`。
- 主 Skill：`combat-feature-development`（`Skills/combat-feature-development/SKILL.md`）。
- 备选 Skill 与排除理由：`combat-skill-development` 面向玩家可施放技能；本次只有工程命名迁移，不适用。
- 路由置信度：`high`
- F0 结论：`GO`
- F0 依据：4 组源文件命名范围明确，工作区初始干净；可用配置重定向保留旧资产引用。
- F1 结论：`APPROVED`
- F2 结论：`PASS`；审查发现的 `[CoreRedirects]` 配置节遗漏已修复并复验。
- Push-Ready 结论：`READY`
- 验证：preflight 通过（0 error）；UE 5.8.2 Editor Development 构建通过（18 actions，`Result: Succeeded`）；`UnrealEditor.exe -unattended -NullRHI -ExecCmds="Automation RunTests Combat.;Quit"` 通过，日志 `Saved/Logs/ue_gas-backup-2026.09.12-06.37.43.log`，63/63 Success、0 Fail、Exit Code 0；`CombatAssetValidation` 通过，`Saved/CombatValidation/CombatAssetReport.json` 为 10 assets、0 errors、0 warnings；`validate_docs.py` 通过（60 Markdown、322 local links）；`git diff --check` 通过。
- 未执行：`UnrealEditor-Cmd.exe` 因安装版 UE 对未安装的 LinuxArm64/VisionOS SDK 做全平台预检而退出（见 `Saved/Logs/AutoSDKInfo.txt`）；按项目技能说明改用同一 Editor 的 `UnrealEditor.exe` 完成 Automation。Dedicated/Server/Client、PIE 和 Soak 不适用：本次不改网络、复制或 gameplay 行为。

## 1. 目标与范围

将 `Source/Combat/` 根部 `ue_gas`、`CombatCharacter`、`CombatGameMode`、`CombatPlayerController` 的 4 对 `.h/.cpp` 改为 `Combat` 前缀。同步 C++ 类型、构造函数、生成头、include、日志引用、现有测试及当前文档。

Non-Goals：不改项目目录、uproject、Module/Target/Build.cs、`UE_GAS_API` 或 `/Script/ue_gas` 包名；不改变输入、移动、RPC、HUD 或战斗行为；不重写二进制资产和历史验收结论。

## 2. 当前事实与依据

- 代码事实：原模板三个反射类分别为 `ACombatCharacter`、`ACombatGameMode`、`ACombatPlayerController`；HUD、单位和 Automation 引用这些类型。
- `Config/DefaultEngine.ini` 有 TopDown 模板到旧类名的 ActiveClassRedirects，必须让目标直达新类名。
- Combat Event 已定义 `LogCombat`，基础项目日志选择 `LogCombatGame` 避免重复定义。
- 本机 Launcher 记录 UE 5.8.2 安装位置；开始时没有运行的 UnrealEditor，UE MCP 工具未暴露，资产加载验证降级到命令行。
- DDD：10-01/10-07/10-09 的当前类名与源码导航随本次改名更新；10-10 的历史验收段落保留当时名称。

## 3. 行为与契约

主流程、状态转换、参数、返回值、服务器权威、Owner/Handle generation、失败、取消、过期、死亡、EndPlay 与重复请求处理均保持原实现。

兼容与迁移按 ADR-050：三个反射类使用完整 `/Script/ue_gas` 路径的 Core ClassRedirects。旧蓝图父类和序列化类引用在加载时映射到新类型；不要求重存资产。C++ 调用方必须同步使用新头文件和类型名。

版本：这是类名兼容迁移 v1；发布契约 `combat_v1_rc1`、GameplayTag、DefinitionId、公式、网络事件及内容 schema 均无变更，不提升无关版本。

## 4. 实施计划

| 文件/资产 | 变更 | 原因 | blast radius |
| --- | --- | --- | --- |
| `Source/Combat/` 根部 8 个 h/cpp | 重命名为 Combat 前缀，更新符号与生成头 | 统一项目命名 | Module 初始化、Command Pawn、GameMode、PlayerController |
| `Combat/UI`、`Combat/Unit`、`Combat/Tests` 中的引用 | 替换头文件和类名 | 保持编译与测试覆盖 | 现有输入、HUD、控制绑定、蓝图加载测试 |
| `Config/DefaultEngine.ini`、`Config/DefaultGame.ini` | 新增旧原生类到新类的重定向，更新模板重定向终点和类默认配置段 | 保持资产可加载 | Demo 蓝图父类与旧序列化引用 |
| 当前 DDD、ADR、Spec、台账 | 更新当前命名与迁移证据 | 保持文档可定位 | 不改变历史日志事实 |

## 5. 验收标准（AC）

- [x] AC-01：4 组文件及对应 C++ 符号改名完成；生产代码无旧头文件或旧类名引用。
- [x] AC-02：Module 注册和 `/Script/ue_gas` 保留，三个旧类名配置到新类；Demo 蓝图可加载。
- [x] AC-03：Editor 构建、现有直接相关 Automation、资产校验与文档/Gate 通过，记录未执行项。

## 6. Definition of Done

- [x] 同步所有生产与测试引用，无 gameplay 逻辑改动。
- [x] 旧蓝图在新编译模块中加载并验证父类；资产不必重存。
- [x] 本地验证、差异审查与 F2 有实测记录。
- [x] 文档、ADR、台账、Spec 及工作区卫生核对完成。

## 7. 测试矩阵与命令

| 层级 | 用例/命令 | 预期证据 | 结果 |
| --- | --- | --- | --- |
| 开工 | `python -B Tools/task_gate.py --mode preflight --spec Doc/CombatSystem/Specs/REF-001-combat-source-prefix.spec.md --kind feature` | Gate 通过 | 通过（0 error） |
| Editor | UE 5.8.2 `Build.bat ue_gasEditor Win64 Development ue_gas.uproject -WaitMutex` | UHT、编译、链接成功 | 通过（18 actions） |
| World Automation | `UnrealEditor.exe ... -ExecCmds="Automation RunTests Combat.;Quit"` | 现有全量测试及蓝图父类加载通过 | 通过（63/63，日志见上） |
| 资产 | `UnrealEditor.exe ... -run=CombatAssetValidation -Report=Saved/CombatValidation/CombatAssetReport.json` | 既有资产有效 | 通过（10/10，0/0） |
| 文档 | `python -B Tools/validate_docs.py`、`git diff --check` | 链接与空白有效 | 通过 |
| 交付 | `python -B Tools/task_gate.py --mode delivery --spec Doc/CombatSystem/Specs/REF-001-combat-source-prefix.spec.md --kind feature` | Gate 通过 | 通过（0 error，35 changed files） |
| Network / Dedicated / Soak | 无协议、复制策略或 gameplay 行为变化 | 本次命名迁移不适用；不重复历史专项 | N/A |

不新增仅镜像字符串替换的单元测试；使用现有蓝图父类、输入、SAM 生命周期、HUD 与发布契约用例验证集成。无新 gameplay 逻辑，因此不虚构 TDD Red；旧名称清单是迁移前事实。

## 8. 风险、回滚与升级

- 风险：遗漏 C++ 引用导致编译失败；缺少 ClassRedirects 导致旧蓝图父类加载失败；简单替换日志名会与已有 LogCombat 重复。
- 回滚方式：成组恢复文件名、C++ 引用与重定向配置。由于资产未重写，原资产可由旧代码直接加载。
- 触发升级的条件：编译或蓝图加载在本次范围内无法修复；如实记录必需验证的环境阻塞。
- 需要人决定的问题：无；不创建提交或推送。

## 9. 交付证据

Editor、Automation、资产、文档、F2 和 delivery Gate 证据已在本 Spec §0、§7 记录。

## 10. 变更记录

| 版本 | 日期 | 修改 | 原因 |
| --- | --- | --- | --- |
| 0.1 | 2026-09-12 | 初稿 | 用户要求统一模板文件为 Combat 前缀 |

## 11. 路由、自评与 Reflect

主 Skill 为 `combat-feature-development`，路由置信度 high；对象是源文件命名迁移，不是 GAS 技能内容。自评：需求与 AC 5/5（范围和兼容目标闭合）；架构与权限 5/5（保留 Module、包身份和日志类别边界）；实现与数据 5/5（8 文件重命名、引用和配置迁移通过构建）；验证证据 4/5（Editor/63 Automation/资产/文档通过，Cmd 全平台预检受 SDK 限制但 Editor fallback 通过）；文档与可观测性 5/5（ADR、台账、当前 DDD、Spec 和报告齐全）；交付卫生 4/5（无生成文件进入 diff，历史文档保留，文件重命名待 Git 记录）。计算总分：`4.7/5.0`。用户尚未验收，状态为 `READY_FOR_REVIEW`。

Reflect：F2 首次检查发现 ClassRedirect 写入了错误的配置节；根据 UE 5.8 Core Redirect 文档补充 `[CoreRedirects]`，随后 Automation 63/63 与资产 10/10 通过。根因是配置迁移的语法边界遗漏，属于单次实现问题；未调整通用 Skill 或校验器。剩余风险仅是用户对新类名和旧 Demo 蓝图的实机复核。
