# REF-002 Runtime Module 迁移为 Combat

> Spec 版本：`0.2`
> 状态：`READY_FOR_REVIEW`
> Owner：Codex
> 创建日期：2026-09-12
> 关联进度台账：[00-01](../00-Project/00-01-Progress-Tracker.md)
> 风险等级：`L2`

## 0. Intake 与 Gate 记录

- 用户请求：继续将 Runtime Module 名称从 `ue_gas` 改为 `Combat`。
- 附件解释：无附件；本轮承接 REF-001 的源码类名前缀迁移结果。
- 已读取入口：`agent.md`、README、00-01、00-03、00-04、00-05、10-01、10-08、10-09、10-10，以及任务路由和功能开发 Skill。
- 主 Skill：`combat-feature-development`（`Skills/combat-feature-development/SKILL.md`）。
- 备选 Skill 与排除理由：`combat-skill-development` 只处理玩家技能；本轮是模块身份迁移，不适用。
- 路由置信度：`high`
- F0 结论：`GO`
- F0 依据：用户明确保留项目文件和 Target 名称，只迁移 Runtime Module；影响面可列举，具备 Core Redirect、Editor 构建和 Automation 回归路径。
- F1 结论：`APPROVED`
- F2 结论：`PASS`（实现、差异、兼容性和验证证据已复核）。
- Push-Ready 结论：`READY`（不执行提交或推送，工作区保留给用户审阅）。
- 验证：preflight 与 delivery Gate 均通过（0 error）。
- 未执行：Dedicated/Soak 未执行；Server/Client Target 受安装版 UE“不支持 Server/Client targets”限制，命令已执行并记录失败原因。

## 1. 目标与范围

将 Runtime Module 身份由 `ue_gas` 迁移为 `Combat`，并保留：

- `ue_gas.uproject` 文件名；
- `ue_gas.Target.cs`、`ue_gasEditor.Target.cs`、`ue_gasServer.Target.cs`、`ue_gasClient.Target.cs` 文件和 Target 类名；
- 构建目标命令名 `ue_gasEditor`、`ue_gasServer`、`ue_gasClient`；
- Gameplay 行为、网络载荷、DefinitionId、GameplayTag、事件 schema 和 `combat_v1_rc1` 版本。

Module 根目录由 `Source/ue_gas` 迁移为 `Source/Combat`；模块实现文件和 Build.cs 使用 `Combat`；公共 API 宏由 `UE_GAS_API` 改为 `COMBAT_API`。

Non-Goals：不改项目名、uproject 文件名、Target 名称、Content 路径、玩法逻辑、网络协议或发布契约；不批量重存二进制资产，除非加载验证证明 Core Redirect 不足。

## 2. 当前事实与依据

- 当前 `ue_gas.uproject` 声明 Module `Combat`；四个 Target 的 `ExtraModuleNames` 都为 `Combat`，Target 文件和类名仍为 `ue_gas*`。
- `Source/Combat/Combat.Build.cs` 定义主模块；源码、Build.cs 和模块实现均使用 Combat，导出宏均为 `COMBAT_API`。
- 配置、DataAsset AssetManager、AbilitySystemGlobals、蓝图序列化路径和现有类重定向使用 `/Script/Combat`；旧 `/Script/ue_gas` 通过 PackageRedirect 兼容。
- UE 5.8 Core Redirects 的 `[CoreRedirects]` 支持 `PackageRedirects`；本轮验证了旧包路径资产与当前 Combat 类路径并存时的加载。
- 本机安装 UE 5.8.2 可执行 Editor 构建；Cmd 启动器此前受未安装的 LinuxArm64/VisionOS SDK 全平台预检影响，必要时使用同一 Editor fallback。

## 3. 行为与契约

### 主流程

UBT 仍通过三个原 Target 名称启动，但 Target 的 `ExtraModuleNames` 指向 `Combat`；UHT、链接、Editor 启动和资产加载使用 `/Script/Combat`。

### 兼容、版本与迁移

`DefaultEngine.ini` 的 `[CoreRedirects]` 增加：

```ini
+PackageRedirects=(OldName="/Script/ue_gas",NewName="/Script/Combat")
```

三个已重命名原生类的 `ClassRedirects` 目标同步改为 `/Script/Combat.Combat...`；旧 TopDown GameName Redirect 目标改为 `/Script/Combat`。当前配置和 DataAsset 类路径切换到 `/Script/Combat`。

## 4. 实施计划

| 文件/资产 | 变更 | 原因 | blast radius |
| --- | --- | --- | --- |
| `Source/ue_gas` | 整体迁移为 `Source/Combat` | 目录与模块身份一致 | 全部 Runtime C++、UHT、文档路径 |
| `Combat.Build.cs` | 模块类、公共 IncludePath、注释改为 Combat | UBT 发现新 Module | Editor/Server/Client Target |
| `ue_gas.uproject` | Module Name 改为 Combat | 工程加载新 Module | Editor 启动与资产包名 |
| 四个 `ue_gas*Target.cs` | 仅 `ExtraModuleNames` 改为 Combat | 保留 Target 名称并链接新 Module | 三 Target 构建 |
| `Source/**/*.h` | `UE_GAS_API` 改为 `COMBAT_API` | 导出宏必须匹配模块 | 全部公开 Runtime 类型 |
| `Config/*.ini` | `/Script/ue_gas` 改为 `/Script/Combat`，添加 PackageRedirect | 保持旧资产和类路径兼容 | AssetManager、蓝图、默认类 |
| 当前文档与 Spec | 更新当前路径和模块事实；历史证据保留旧身份说明 | 防止新路径失效 | 文档校验与维护入口 |

## 5. 验收标准（AC）

- [x] AC-01：uproject、Build.cs、Target ExtraModuleNames、主模块宏和 API 宏均使用 Combat；三个 Target 名称保持不变。
- [x] AC-02：旧 `/Script/ue_gas` 蓝图/资产通过 PackageRedirect 加载为 `/Script/Combat`，资产校验扫描 10 个定义且 0 Error/0 Warning。
- [x] AC-03：Editor 构建、现有 `Combat.*` Automation、CombatAssetValidation、文档校验和 delivery Gate 通过。
- [x] AC-04：`ue_gas.uproject`、Target 文件名/类名、Content 路径和 Gameplay 逻辑没有非目标变化；Server/Client 仅受引擎发行版限制未能构建。

## 6. Definition of Done

- [x] 完成目录和模块配置迁移，未遗留错误 `UE_GAS_API`、旧 Build.cs 模块名或 Target ExtraModuleNames。
- [x] Core Redirect 和当前配置均回读，Demo 蓝图、DataAsset、测试地图可加载。
- [x] Editor 构建和 Automation/资产校验已执行；Server/Client 按环境执行并记录发行版限制。
- [x] 当前文档、Spec、ADR、台账和差异卫生同步；生成目录不进入 diff。

## 7. 测试矩阵与命令

| 层级 | 用例/命令 | 预期证据 | 结果 |
| --- | --- | --- | --- |
| 开工 | `python -B Tools/task_gate.py --mode preflight --spec Doc/CombatSystem/Specs/REF-002-combat-runtime-module.spec.md --kind feature` | Gate 通过 | 通过（0 error） |
| Editor | `Build.bat ue_gasEditor Win64 Development ue_gas.uproject -WaitMutex` | 新 Combat Module UHT/链接成功 | 通过；Result: Succeeded，生成 `UnrealEditor-Combat.dll` |
| Target | `Build.bat ue_gasServer ...`、`Build.bat ue_gasClient ...` | Target 名称不变且链接 Combat | 未通过/受限；安装版 UE 报 `Server targets are not currently supported`、`Client targets are not currently supported` |
| World Automation | `UnrealEditor.exe ... -ExecCmds="Automation RunTests Combat.;Quit"` | 现有 Combat 测试及旧资产加载通过 | 通过；`Saved/Logs/ue_gas.log`，63/63 Success、0 Fail、Exit Code 0 |
| 资产 | `UnrealEditor-Cmd.exe ... -run=CombatAssetValidation -NoAssetRegistryCache -Report=Saved/CombatValidation/CombatAssetReport.json` | 资产定义和蓝图引用有效 | 通过；10 assets、0 Error、0 Warning |
| 文档 | `python -B Tools/validate_docs.py`、`$env:PYTHONUTF8='1'; python -B -m unittest discover -s Tools/Tests -p 'test_validate_docs.py'`、`git diff --check` | 文档、工具测试和空白有效 | 通过；61 Markdown、324 local links；工具单测 16/16；diff check 通过 |
| 交付 | `python -B Tools/task_gate.py --mode delivery --spec Doc/CombatSystem/Specs/REF-002-combat-runtime-module.spec.md --kind feature` | Gate 通过 | 通过（0 error，341 changed files；源码迁移按新增/删除计数） |
| Dedicated / Soak | Module identity affects process startup; run if installed/source UE supports Target | No old package load errors | 未执行；当前安装版不支持目标构建，留待源码版 UE/专用环境 |

本轮不新增镜像字符串单测；使用既有 Foundation、蓝图父类、AssetManager、HUD、SAM 和发布契约测试覆盖迁移后的模块加载。

## 8. 风险、回滚与升级

- 风险：旧 `/Script/ue_gas` 包引用未被重定向；API 宏遗漏造成链接错误；Target 名称保留但 ExtraModuleNames 未迁移；旧配置类段仍指向旧包。
- 回滚方式：恢复 `Source/ue_gas`、Module Name、API 宏和 `/Script/ue_gas` 配置；保留 Redirect 以便旧资产继续加载。
- 触发升级的条件：任一既有蓝图无法加载、Automation 发现模块身份不一致、或无法在当前 UE 环境完成目标构建。
- 需要人决定的问题：无；用户已明确 Target 和项目文件保留范围。

## 9. 交付证据

验证证据：Editor Development 构建成功并链接 `UnrealEditor-Combat.dll`；`Combat.*` Automation 为 63/63 Success、0 Fail；资产报告为 10 assets、0 Error、0 Warning；文档校验为 61 Markdown、324 local links；`git diff --check` 通过。Server/Client Target 命令已执行但被安装版 UE 拒绝，Dedicated/Soak 未执行。Core Redirect 加载路径已由旧 `/Script/ue_gas` 资产与新 `/Script/Combat` 类路径共同扫描验证。

## 10. 变更记录

| 版本 | 日期 | 修改 | 原因 |
| --- | --- | --- | --- |
| 0.1 | 2026-09-12 | 初稿 | 用户授权 Runtime Module 改为 Combat |
| 0.2 | 2026-09-12 | 回填实现、验证、Asset Registry 兼容和交付结论 | 完成 F2 与交付审查 |

## 11. 路由、自评与 Reflect

### 路由记录

- 原始需求摘要：模块名改为 Combat，项目文件和 Target 名称不变。
- 主 Skill：`combat-feature-development`
- 选择依据：涉及 C++ 模块、API 宏、构建目标、反射包名、资产兼容与多层验证。
- 备选 Skill 与排除理由：`combat-skill-development` 不涉及技能实现。
- 路由置信度：`high`

### 交付自评

六维评分：需求理解 5/5（保留项目文件与 Target 名称边界明确）；范围控制 5/5（仅迁移模块身份并补充必要资产扫描兼容）；实现质量 4/5（增量文件迁移、宏和重定向一致，增加旧 Asset Registry 类路径兜底）；验证充分性 4/5（Editor、63 项 Automation、10 项资产和文档通过，Server/Client/Dedicated 受环境限制）；文档与可追溯性 5/5（Spec、ADR、台账和证据同步）；交付卫生 4/5（未提交/推送，生成物留在既有 Saved/Intermediate 工作区）。综合 4.5/5。用户验收状态保持待用户审阅。

### Reflect 与调优

本轮首个验证问题是模块包名迁移后 Asset Registry 的基类递归过滤返回 0 个资产；已改为显式收集 Combat 定义类及旧 `/Script/ue_gas` 类路径，复验恢复为 10 个资产且 0 Error/0 Warning。PackageRedirect 使用旧包名到新包名的精确映射，验证旧资产仍可加载；不修改通用 Skill。
