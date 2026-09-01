# ue_gas Combat

这是一个面向 Unreal Engine 5.8 的 Dota-like 战斗框架。项目以 Gameplay Ability System（GAS）承载 Attribute、GameplayTag、GameplayEffect 和 GameplayAbility，并用自定义运行时补齐 Order、AttackRecord、Modifier Hook、Combat Scheduler、Projectile、Thinker、Aura、Motion、统一伤害/治疗事务和多人可观测性。

Combat 当前仍位于 `ue_gas` 单 Runtime Module 中，不是独立插件或独立 Module。

## 当前基线

- 核心发布契约：`combat_v1_rc1`，Contract/Content/GameplayTag/Formula/RNG/Event schema 均为 v1。
- 权威模型：服务器结算；客户端 TargetData 仅作为请求，目标、资源和结果由服务器复核。
- M0-M8 共 82 个 Task 已完成并通过用户验收；最近一次发布 Gate 记录为 `Combat.*` 40/40、Editor/Server/Client 构建、资产校验和 Dedicated 双客户端容量场景通过。
- M8 之后仓库继续增加了可玩的远程攻击 Demo、整理后的 Demo 资产结构，以及纯 C++ 的单位头顶资源/状态/施法条和伤害治疗跳字。
- 完整 gameplay 预测回滚、跨进程确定性 Replay、召唤物/幻象、物品与经济不属于当前 v1 范围。

以上测试数字是已归档的最近验收证据，不自动代表任意工作区修改已经重新验证。实时任务状态以 [开发进度台账](Doc/CombatSystem/00-Progress-Tracker.md) 为准。

## 快速入口

1. 安装 UE 5.8，并确保 Git LFS 已拉取 `.uasset`、`.umap` 等二进制资产。
2. 打开 `ue_gas.uproject`。可玩 Demo 地图位于 `/Game/Combat/Demo/Maps/L_CombatDemo`。
3. 自动化测试地图位于 `/Game/Combat/Tests/L_CombatTest`。
4. Combat C++ 入口位于 `Source/ue_gas/Combat`；新增技能先阅读 [公共技能扩展与迁移指南](Doc/CombatSystem/32-M8-Public-Extension-Guide.md)。

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
  -> Combat Event、Unit View、Projectile Presentation、Overhead UI
```

关键原则是“一个事实只有一个权威来源”：最终属性来自 ASC 聚合，Health/Mana 通过统一资源与事务入口修改，异步实体由稳定 Handle 和 generation 管理，结束和广播保持 exactly-once。

## 目录导航

| 路径 | 内容 |
| --- | --- |
| `Source/ue_gas/Combat/Ability` | ASC、GameplayAbility 基类、AbilityTask 与 EffectContext |
| `Source/ue_gas/Combat/Combat` | Damage、Heal、Transaction 和 Effect 工具 |
| `Source/ue_gas/Combat/Modifiers` | ActiveGE/Runtime 映射、Hook、叠层、周期和驱散 |
| `Source/ue_gas/Combat/Order`、`Attack` | 指令状态机、追击、AttackRecord、法球和普攻时序 |
| `Source/ue_gas/Combat/Projectile`、`Thinker`、`Aura`、`Motion` | 异步空间实体与强制位移 |
| `Source/ue_gas/Combat/Data` | Unit/Ability/Modifier/Projectile/AbilitySet PrimaryDataAsset |
| `Source/ue_gas/Combat/Network`、`View`、`UI` | RPC 防护、扁平复制 View 和头顶表现 |
| `Source/ue_gas/Combat/Tests` | `Combat.*` Automation 测试 |
| `Content/Combat/Demo` | 可玩 Demo 地图、角色、远程攻击和输入资产 |
| `Content/Combat/Tests` | PIE、Dedicated 与容量测试地图 |
| `Doc/CombatSystem` | 架构、专题契约、决策、测试和验收证据 |

## 文档阅读顺序

- 初次了解：本文 → [文档总索引](Doc/DotaLikeGASCombatSystemDesign.md) → [范围、架构与硬约束](Doc/CombatSystem/01-Scope-Architecture.md)。
- 理解联机交互：[客户端与服务器交互流程](Doc/CombatSystem/34-Client-Server-Interaction.md) → [Order 与移动](Doc/CombatSystem/07-Order-Movement.md) → [Ability 与目标](Doc/CombatSystem/03-Ability-Targeting-Blueprint.md) → [网络与 UI](Doc/CombatSystem/08-Data-Network-Observability.md)。
- 开发技能：[Ability、目标与蓝图接口](Doc/CombatSystem/03-Ability-Targeting-Blueprint.md) → [Damage/Heal](Doc/CombatSystem/05-Damage-Heal.md) → [示例技能](Doc/CombatSystem/09-Example-Skills.md) → [技能模板检查表](Doc/CombatSystem/25-M6-Skill-Template-Checklist.md)。
- 修改内核：先读对应 02-08 专题，再检查 [决策与缺口登记](Doc/CombatSystem/12-Decisions-Gaps.md) 和 [生命周期审计](Doc/CombatSystem/31-M8-Lifecycle-Audit.md)。
- 验证发布边界：[候选发布决策](Doc/CombatSystem/30-M8-Release-Candidate-Decision.md) → [M8 验收记录](Doc/CombatSystem/33-M8-Acceptance.md)。
- Agent 或自动化开发：先读根目录 [agent.md](agent.md)。

## 验证命令模板

先根据本机 UE 安装确定 `<UE_ROOT>`；不要把历史验收机器的绝对路径写入新脚本。

```powershell
& "<UE_ROOT>\Engine\Build\BatchFiles\Build.bat" ue_gasEditor Win64 Development "<REPO>\ue_gas.uproject" -WaitMutex

& "<UE_ROOT>\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "<REPO>\ue_gas.uproject" `
  -unattended -nop4 -nosplash -NullRHI -NoSound `
  -ExecCmds="Automation RunTests Combat.;Quit" `
  -TestExit="Automation Test Queue Empty"

& "<UE_ROOT>\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "<REPO>\ue_gas.uproject" `
  -run=CombatAssetValidation -Unattended -NoP4 `
  -Report="<REPO>\Saved\CombatValidation\CombatAssetReport.json"
```

Dedicated Server/Client Target 需要支持该 Target 的源码引擎。详细环境边界见 [M1 环境决策](Doc/CombatSystem/15-M1-Environment-Decision.md)，完整测试分层见 [测试计划](Doc/CombatSystem/11-Test-Plan.md)。
