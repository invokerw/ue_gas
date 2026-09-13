# ue_gas Combat

这是一个面向 Unreal Engine 5.8 的 Dota-like 战斗框架。项目以 Gameplay Ability System（GAS）承载 Attribute、GameplayTag、GameplayEffect 和 GameplayAbility，并用自定义运行时补齐 Order、AttackRecord、Modifier Hook、Combat Scheduler、Projectile、Thinker、Aura、Motion、统一伤害/治疗事务和多人可观测性。

Combat 当前位于 `Combat` 单 Runtime Module 中，不是独立插件或独立 Module；项目文件和 Target 名称仍保留 `ue_gas`。

## 当前基线

- 核心发布契约：`combat_v1_rc1`，Contract/Content/GameplayTag/Formula/RNG/Event schema 均为 v1。
- 权威模型：服务器结算；客户端 TargetData 仅作为请求，目标、资源和结果由服务器复核。
- M0-M8 共 82 个 Task 已完成并通过用户验收；最近一次发布 Gate 记录为 `Combat.*` 40/40、Editor/Server/Client 构建、资产校验和 Dedicated 双客户端容量场景通过。
- M8 之后增加了卓尔游侠远程攻击 Demo、头顶资源/状态/施法条、伤害治疗跳字，以及底部居中的英雄、技能、Buff HUD。C++ 提供只读数据，Widget Blueprint 维护布局和视觉；等级经验与技能加点已接入服务器权威成长组件，六格物品及三格背包仍显示占位。
- 完整 gameplay 预测回滚、跨进程确定性 Replay、召唤物/幻象、物品与经济不属于当前 v1 范围。

以上测试数字是已归档的最近验收证据，不自动代表任意工作区修改已经重新验证。实时任务状态以 [开发进度台账](Doc/CombatSystem/00-Project/00-01-Progress-Tracker.md) 为准。

## 快速入口

1. 安装 UE 5.8，并确保 Git LFS 已拉取 `.uasset`、`.umap` 等二进制资产。
2. 打开 `ue_gas.uproject`。可玩 Demo 地图位于 `/Game/Combat/Demo/Maps/L_CombatDemo`。
3. 自动化测试地图位于 `/Game/Combat/Tests/L_CombatTest`。
4. Combat C++ 入口位于 `Source/Combat/Combat`；新增技能先阅读 [公共技能扩展与迁移指南](Doc/CombatSystem/20-Content/20-03-M8-Public-Extension-Guide.md)。
5. AI 协作开发先阅读 [AI-Native 开发流程与文档体系](Doc/CombatSystem/00-Project/00-05-AI-Native-Development-Workflow.md)，新需求或修复使用 [Spec 模板](Doc/CombatSystem/Specs/_template.spec.md)。
   进入 BUILD 前必须先完成 PLAN 计划审查并记录 F1=`APPROVED`；按顺序运行 `task_gate.py --mode preflight`、`--mode plan`、`--mode build`。F1 通过前不得修改代码、工具脚本、蓝图或资产。
6. 任务开始用 [Intake](Doc/CombatSystem/00-Project/_intake-template.md) 整理边界，交付前按 [Gate 检查表](Doc/CombatSystem/00-Project/_gate-checklist.md) 记录验证结果；这些内容可以合并到任务 Spec，独立记录时使用 [交付记录模板](Doc/CombatSystem/00-Project/_delivery-record-template.md)。
7. 让 Agent 判断应该调用哪个 Skill 时使用 [Combat 任务路由 Skill](Skills/combat-task-router/SKILL.md)，它会在交付后按证据自评并执行受控调优。
8. 开发功能任务时使用 [Combat 功能开发 Skill](Skills/combat-feature-development/SKILL.md)，它会按 AI-Native 流程推进 Spec、Gate、验证和 Reflect。
9. 实现 GAS/Combat 技能时使用 [Combat 技能开发 Skill](Skills/combat-skill-development/SKILL.md)，它会在通用流程上补齐 Ability、DataAsset、Modifier、Projectile、蓝图和技能验收。

项目 Skill 全部位于仓库内的 `Skills/`，按项目路径读取，不安装到用户级 Codex Skill 目录，也不影响其他项目。

Demo 操作：右键点敌方单位持续普攻，超出范围时自动追击；右键点地面移动。按 **A** 进入选敌模式，再左键点敌人确认普攻；**S** 停止，**Escape** 取消选敌。卓尔游侠的 **Q** 在“霜冻之箭”开启/关闭间切换，默认开启；W/E/R 当前为空。A 模式点地面不会自动找敌或执行攻击移动（Attack Move）。

以上按键统一通过 `/Game/Combat/Demo/Input/IMC_Default` 映射到 Input Action；可在该资产中改键。普攻选敌、确认、取消和停止的 Action 引用配置在 `BP_CombatDemoPlayerController` 的 `Input|Combat` 默认属性中。

底部 HUD 随本地玩家的指挥单位切换。悬停技能、Buff 或头像上的属性可查看详情，点击固定，关闭按钮或 Escape 取消固定；点击 HUD 不发出移动或施法请求。英雄等级和经验环读取服务器成长快照；有技能点且满足英雄等级时，技能图标上方显示“+”按钮，点击请求服务器加点。Q 槽显示“霜冻之箭”及服务器权威的“自动/关闭”状态，W/E/R 保留空位。界面配置入口见 [10-12 底部 HUD](Doc/CombatSystem/10-Architecture/10-12-Bottom-HUD-Design.md)。

点击 HUD 左上角的 **战斗记录** 查看服务器确认的伤害、治疗、技能、状态与死亡/复活事件。记录包含毫秒时间戳、彩色名称、实际数值及生命前后值；攻击者、目标、类别与时间范围可以组合筛选，默认最近 30 秒，最多保留 512 条。上滚暂停跟随，勾选“跟随最新”回到底部；按住顶部栏左键拖动窗口，松开停留，关闭再打开保留当前位置；关闭窗口仍记录，Escape 收起窗口。物品选项因物品系统尚未接入而置灰。布局与颜色在 `WBP_CombatLog` 调整，详见 [战斗记录接入](Doc/CombatSystem/10-Architecture/10-12-Bottom-HUD-Design.md#7-战斗记录窗口hud-log-001)。

开发调试时可在 Standalone 或服务器控制台执行 `combat.Debug.AddExperience 200`，给当前 World 的首个玩家主控单位增加 200 点经验；也可追加单位对象名或 `ActorUniqueId` 精确指定目标，例如 `combat.Debug.AddExperience 200 BP_DrowRanger_C_0`。命令仅在非 Shipping 构建注册，并复用服务器权威成长组件；客户端执行不会直接修改等级、经验或技能点。可先用 `combat.Debug.Unit <ActorUniqueId|Name>` 查询单位名称和 ID。

## 运行时主链路

```text
客户端输入 / AI 意图
  -> Order RPC 安全检查
  -> OrderComponent（Move / Attack / Cast / Stop）
  -> Targeting 服务器复核
  -> Ability / AttackRecord
  -> Combat Scheduler 驱动前摇、引导、周期与过期
  -> Damage / Heal / Modifier / Projectile / Thinker / Motion 公共入口
  -> GAS Attribute / ActiveGE 真实落账
  -> Combat Event、Unit View、Projectile Presentation、Overhead UI / Bottom HUD
```

关键原则是“一个事实只有一个权威来源”：最终属性来自 ASC 聚合，Health/Mana 通过统一资源与事务入口修改，异步实体由稳定 Handle 和 generation 管理，结束和广播保持 exactly-once。

## 目录导航

| 路径 | 内容 |
| --- | --- |
| `Source/Combat/Combat/Ability` | ASC、GameplayAbility 基类、AbilityTask 与 EffectContext |
| `Source/Combat/Combat/Combat` | Damage、Heal、Transaction 和 Effect 工具 |
| `Source/Combat/Combat/Modifiers` | ActiveGE/Runtime 映射、Hook、叠层、周期和驱散 |
| `Source/Combat/Combat/Order`、`Attack` | 指令状态机、追击、AttackRecord、法球和普攻时序 |
| `Source/Combat/Combat/Projectile`、`Thinker`、`Aura`、`Motion` | 异步空间实体与强制位移 |
| `Source/Combat/Combat/Data` | Unit/Ability/Modifier/Projectile/AbilitySet PrimaryDataAsset |
| `Source/Combat/Combat/Network`、`View`、`UI` | RPC 防护、公共/拥有者 View、头顶表现与底部 HUD |
| `Source/Combat/Combat/Tests` | `Combat.*` Automation 测试 |
| `Content/Combat/Demo` | 可玩 Demo 地图、`Heros/DrowRanger`、木桩、霜冻之箭、远程攻击和输入资产 |
| `Content/Combat/Tests` | PIE、Dedicated 与容量测试地图 |
| `Doc/CombatSystem/00-Project` | 当前状态、AI 开发流程、路线图、测试计划和决策入口 |
| `Doc/CombatSystem/10-Architecture` | 当前运行时、联机、移动和 UI 架构契约 |
| `Doc/CombatSystem/20-Content` | 示例技能、技能模板和公共扩展指南 |
| `Doc/CombatSystem/30-Tooling` | UE MCP 操作与诊断配方 |
| `Doc/CombatSystem/90-History` | M0–M8 冻结决策和验收证据 |

## 文档阅读顺序

- 初次了解：本文 → [文档总索引](Doc/CombatSystem/README.md) → [范围、架构与硬约束](Doc/CombatSystem/10-Architecture/10-01-Scope-Architecture.md)。
- 理解联机交互：[客户端与服务器交互流程](Doc/CombatSystem/10-Architecture/10-09-Client-Server-Interaction.md) → [Order 与移动](Doc/CombatSystem/10-Architecture/10-07-Order-Movement.md) → [Ability 与目标](Doc/CombatSystem/10-Architecture/10-03-Ability-Targeting-Blueprint.md) → [网络与 UI](Doc/CombatSystem/10-Architecture/10-08-Data-Network-Observability.md)。
- 理解服务器权威单位移动：[服务器权威单位移动改造与验收](Doc/CombatSystem/10-Architecture/10-10-Server-Authoritative-Movement-Kickoff.md)；当前端到端链路以 10-09 为准。
- 调整头顶 UI：[C++ 与蓝图边界、事件接口和资产配置](Doc/CombatSystem/10-Architecture/10-11-Overhead-Blueprint-UI.md)。
- 调整底部 HUD：[定稿布局、Widget Blueprint、拥有者快照和占位边界](Doc/CombatSystem/10-Architecture/10-12-Bottom-HUD-Design.md)。
- 开发技能：[Ability、目标与蓝图接口](Doc/CombatSystem/10-Architecture/10-03-Ability-Targeting-Blueprint.md) → [Damage/Heal](Doc/CombatSystem/10-Architecture/10-05-Damage-Heal.md) → [示例技能](Doc/CombatSystem/20-Content/20-01-Example-Skills.md) → [技能模板检查表](Doc/CombatSystem/20-Content/20-02-M6-Skill-Template-Checklist.md)。
- 修改内核：先读对应 10-02–10-08 专题，再检查 [决策与缺口登记](Doc/CombatSystem/00-Project/00-04-Decisions-Gaps.md) 和 [生命周期审计](Doc/CombatSystem/90-History/90-16-M8-Lifecycle-Audit.md)。
- 验证发布边界：[候选发布决策](Doc/CombatSystem/90-History/90-15-M8-Release-Candidate-Decision.md) → [M8 验收记录](Doc/CombatSystem/90-History/90-17-M8-Acceptance.md)。
- Agent 或自动化开发：先读根目录 [agent.md](agent.md)。
- AI 开发流程与 Spec：先读 [00-05 AI-Native 开发流程](Doc/CombatSystem/00-Project/00-05-AI-Native-Development-Workflow.md)，状态和证据仍以 [00-01 开发进度台账](Doc/CombatSystem/00-Project/00-01-Progress-Tracker.md) 为准。

## 验证命令模板

不同机器的 UE 安装路径不相同，先复制环境模板并填写本机路径。`.env` 只保存在本地并已加入 Git 忽略；不要把真实机器路径提交到仓库。

```powershell
Copy-Item .env.example .env
notepad .env
```

配置两个编辑器入口：

- `UE_INSTALLED_EDITOR`：Launcher/下载版 `UnrealEditor.exe`，用于普通 Editor、Automation 和资产校验。
- `UE_SOURCE_EDITOR`：从源码编译的 `UnrealEditor.exe`，用于 Server/Client Target 和 Dedicated smoke。工具会从该路径上溯找到源码引擎根目录，并检查 `Engine/Build/BatchFiles/Build.bat`。

检查配置状态：

```powershell
python Tools/ue_environment.py check
python Tools/ue_environment.py check --require editor
python Tools/ue_environment.py check --require dedicated --json
```

`--require dedicated` 返回 0 表示 Dedicated 可以运行，返回 2 表示配置缺失或路径无效；后者应把 Dedicated 记为“未执行”，不能记为通过。满足检查后运行 Dedicated Server + 两客户端 smoke：

```powershell
& .\Tools\RunDedicated.ps1
```

日志和进程摘要写入 `Saved/UEEnvironment/Dedicated/`。脚本只清理本次启动的 UE 进程。

```powershell
& "<UE_SOURCE_ROOT>\Engine\Build\BatchFiles\Build.bat" ue_gasEditor Win64 Development "<REPO>\ue_gas.uproject" -WaitMutex

& "<UE_INSTALLED_ROOT>\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "<REPO>\ue_gas.uproject" `
  -unattended -nop4 -nosplash -NullRHI -NoSound `
  -ExecCmds="Automation RunTests Combat.;Quit" `
  -TestExit="Automation Test Queue Empty"

& "<UE_INSTALLED_ROOT>\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "<REPO>\ue_gas.uproject" `
  -run=CombatAssetValidation -Unattended -NoP4 `
  -Report="<REPO>\Saved\CombatValidation\CombatAssetReport.json"
```

上面的 `<UE_SOURCE_ROOT>` 和 `<UE_INSTALLED_ROOT>` 只是命令中的说明占位符；实际路径以 `.env` 中的两个编辑器文件位置为准。Dedicated Server/Client Target 需要支持该 Target 的源码引擎。详细环境边界见 [M1 环境决策](Doc/CombatSystem/90-History/90-02-M1-Environment-Decision.md)，完整测试分层见 [测试计划](Doc/CombatSystem/00-Project/00-03-Test-Plan.md)。

文档体系校验可在仓库根目录运行：

```bash
python3 -B Tools/validate_docs.py
git diff --check
```

任务开工和交付使用可失败的流程 Gate；`<task-id>` 必须对应本次任务的 Spec。`feature` 覆盖功能/Bug/资产变更，`process` 覆盖工具和流程变更，`docs` 覆盖纯文档变更：

```bash
python3 -B Tools/task_gate.py --mode preflight --spec Doc/CombatSystem/Specs/<task-id>.spec.md --kind <feature|docs|process>
python3 -B Tools/task_gate.py --mode plan --spec Doc/CombatSystem/Specs/<task-id>.spec.md --kind <feature|docs|process>
```

完成计划审查，在 Spec 写入 F1=`APPROVED`、审查版本和证据后，再进入实现与交付：

```bash
python3 -B Tools/task_gate.py --mode build --spec Doc/CombatSystem/Specs/<task-id>.spec.md --kind <feature|docs|process>
python3 -B Tools/task_gate.py --mode delivery --spec Doc/CombatSystem/Specs/<task-id>.spec.md --kind <feature|docs|process>
```

Gate 会检查 Spec、Skill 路由、F0/F1/F2、计划审查版本和证据、Push-Ready、验证/未执行记录，以及行为变更是否配套测试和文档；失败结果必须先修复并回写 Spec。Spec 实质变更后先升级版本并重新审查，旧批准不能用于新范围；检查工具不追溯证明修改先后。

文档检查覆盖必需入口、目录迁移、Markdown 本地目标路径、尾随空格和 Spec 格式；页内锚点、外部链接和文档语义需要另行审查。当前采用本地开发与用户验收流程，交付或提交前运行上述命令。它们不能替代 UE 编译、Automation、PIE 或 Dedicated 验证。

需要保存文档校验结果时，添加 `--report Saved/DocValidation/report.json`。修改校验脚本时，再运行其正反例测试：

```bash
python3 -B -m unittest discover -s Tools/Tests -p 'test_validate_docs.py' -v
```

若 IDE 构建报 `Unable to delete hot-reload file`，先检查日志中的 DLL 是否仍被同工程的 UE 进程占用；后台没有可见窗口的进程也可能持有模块。保存资产并完全退出占用进程后，运行上面的常规 Editor 构建，让 UBT 清理热重载记录并恢复 `UnrealEditor-Combat.dll`。使用 `-ModuleWithSuffix` 做临时验证后，交付前应完成这一步，并退出仅用于启动验证的 Editor 实例，避免影响下一次 IDE 构建。
