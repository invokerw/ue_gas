# M2 战斗内核验收报告

> 日期：2026-08-25
> 用户验收：2026-08-26
> 关联：ATR-001..002、LIFE-001、CMB-001..004、MOD-001..005、DEMO-201..202、OBS-002、G2
> 结论：15/15 Task 完成，G2 通过，用户验收通过

## 1. 交付结果

M2 已形成服务器权威的 Attribute、Damage/Heal、Modifier 与生命状态纵向闭环：

- `CombatAttributeSet` 提供 Health/Mana、攻防、移动、恢复、增幅/吸血和 Incoming Meta Attribute；普通属性复制并统一执行 Numeric Policy v1 clamp。
- `CombatUnitCharacter` 通过批量 Instant GE 从 UnitData 初始化基础属性，幂等授予 AbilitySet，并保存每个 AbilitySpec 的初始 AutoCast 状态。
- `CombatUnitLifecycleComponent` 同步推进 `Alive -> Dying -> Dead` 与 `Dead -> Respawning -> Alive`，取消旧生命调度并维护 `LifeGeneration`。
- `CombatDamageSubsystem` 与 `CombatHealSubsystem` 是 Health 结算的公开入口；AttributeSet 通过 EventId 同步结果槽回报 clamp 后的真实变化量。
- `CombatModifierComponent` 维护一 Runtime 对一 Active GE，支持稳定 Hook 顺序、阶段后 FIFO 结构修改、叠层、刷新、周期、状态抗性、驱散和死亡策略。
- Demo Runtime 覆盖 Magic Shield、周期伤害和反伤；Slow/Stun 使用数据驱动 Attribute/Granted Tag Modifier。
- CombatLog 覆盖 Damage、Heal、Death、Respawn、Modifier Apply/Remove，并输出 `Schema=1`、`Formula=1`、RootEvent、Source/Target、LifeGeneration、Requested/Mitigated/Absorbed/Applied 与 Flags。

M2 规则冻结见 [16 M2 战斗内核实现决策](16-M2-Combat-Core-Decision.md)，已关闭 GAP-005、GAP-012 和 GAP-013。

## 2. 核心源码位置

| 范围 | 主要源码 |
| --- | --- |
| Attribute 与恢复 | `Source/ue_gas/Combat/Attributes/CombatAttributeSet.*`、`Source/ue_gas/Combat/Unit/CombatRegenerationComponent.*` |
| 初始化与生命周期 | `Source/ue_gas/Combat/Unit/CombatUnitCharacter.*`、`CombatUnitLifecycleComponent.*` |
| Damage/Heal 与结果槽 | `Source/ue_gas/Combat/Combat/CombatTransaction*`、`CombatDamage*`、`CombatHealSubsystem.*`、`CombatEffectUtilities.*` |
| Modifier | `Source/ue_gas/Combat/Modifiers/CombatModifierRuntime.*`、`CombatModifierComponent.*` |
| 示例 Runtime | `Source/ue_gas/Combat/Demo/CombatDemoModifierRuntimes.*` |
| 自动化 | `Source/ue_gas/Combat/Tests/CombatCoreTests.cpp` |

所有本次新建或实质修改的项目自有类、结构、枚举、函数、关键字段和非显然逻辑均补充中文注释。

## 3. 自动化结果

最终冷启动命令：

```text
UnrealEditor-Cmd.exe ue_gas.uproject -unattended -nop4 -nosplash -nullrhi -nosound -ExecCmds="Automation RunTests Combat.;Quit" -TestExit="Automation Test Queue Empty"
```

验收日志：`Saved/Logs/M2AutomationAcceptance.log`。

- `Combat.Core`：5/5 Success。
- `Combat.Foundation` 回归：7/7 Success。
- 合计：12/12 Success，0 Failed，无 Fatal、Assertion、ensure 或 Automation Error。

M2 用例覆盖：

1. Attribute 初始化、AbilitySet/AutoCast 幂等、恢复节拍、死亡与复活。
2. 正负护甲、魔抗、Pure、SpellAmp、免疫、HPLoss、Heal amp、overheal、Dead 不复活和非法数值/Actor。
3. EventId 同步结果槽 exactly-once。
4. 多 Shield 稳定顺序、GE/Runtime 同步销毁、叠层/刷新、状态抗性、Slow/Stun 与驱散。
5. DOT 到期边界、反伤/吸血、RootEventId/Depth、结构化日志数值槽与 Flags。

## 4. 构建矩阵

| Engine | Target | 配置 | 结果 |
| --- | --- | --- | --- |
| Installed UE 5.8.1 | `ue_gasEditor` | Win64 Development | Succeeded；最终增量 4/4 actions |
| Source UE 5.8.0（`D:\UE\UE`） | `ue_gasServer` | Win64 Development | Succeeded；最终增量 3/3 actions |
| Source UE 5.8.0（`D:\UE\UE`） | `ue_gasClient` | Win64 Development | Succeeded；最终增量 3/3 actions |

Server/Client 的完整 M2 重编译也分别以 21/21 actions 成功；最终 3/3 用于确认新增 P0 测试源码同样通过两个 Target。

## 5. 独立联机 smoke

使用 Installed UE 5.8.1 启动两个隐藏的独立 OS 进程：监听服务器加载 `/Game/Combat/Tests/L_CombatTest?listen`，客户端连接 `127.0.0.1:17778`。验收日志：

- `Saved/Logs/M2ListenServerJoin.log`
- `Saved/Logs/M2ListenClientJoin.log`

关键证据：

```text
M2ScenarioReady Units=2 Team1=1 Team2=1 ASCActorInfo=Ready State.Alive=Present CoreComponents=Ready
Join succeeded
Welcomed by server (Level: /Game/Combat/Tests/L_CombatTest, ...)
```

两端均无 `LogCombat: Error`、`LogNet: Error`、Fatal、Assertion、ensure 或 NetworkFailure，测试进程已关闭。Installed Engine 的 Experimental Toolsets 仍会输出缺少 `AgentSkill`/`ToolsetDefinition` 的 Python 启动错误；该引擎插件问题与 M1 一致，不影响项目模块、地图或网络握手，不计为 Combat Gate 失败。

## 6. G2 结论

- Attribute、Damage/Heal、Modifier、状态、Magic Shield/DOT 的 P0 自动化全部通过。
- Magic Shield、DOT、Slow、Stun 不直接写 Health/Transform，不自建 Actor Tick 或 Timer；周期行为统一进入 Combat Scheduler。
- Damage/Heal 的真实 Health delta 只由 Meta Attribute、Instant GE 和同步结果槽产生；Death 不由 AttributeSet 重复广播。
- G2：通过。用户已于 2026-08-26 确认 M2 验收通过；M3 尚未授权。
