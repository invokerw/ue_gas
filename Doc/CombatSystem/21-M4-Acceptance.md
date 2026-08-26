# M4 Order 与普攻验收报告

> 日期：2026-08-26
> 用户验收：2026-08-26
> 关联：ORD-001..004、ATK-001..004、GAP-010、G4
> 结论：8/8 Task 完成，G4 通过，用户验收通过

## 1. 交付结果

M4 已形成服务器权威的 Order、NavMesh 追击和近战普攻纵向闭环：

- `UCombatOrderComponent` 统一 FIFO、非排队替换、Stop、generation、结构化结果和状态机；EQS、AI Move、Ability、Attack、Scheduler 回调均以 Order/Request/LifeGeneration 联合校验。
- Move/Cast/Attack 共用 AI Move 执行和到达后服务器重验；动态目标每 0.10 s 复核，位移超过 50 cm 重发请求，并具有重试次数和追击时长上限。
- AI Move 成功、失败、取消及 PartialPath 被分别处理；成功 PartialPath 仍按当前 gameplay 距离重验，移动结束的一帧残余速度只进入有界重试。
- ASC 和 `UCombatGameplayAbility` 提供 `OrderReleased`，Cast Order 在释放或激活失败时推进，不等待 AbilityEnded、backswing 或 cooldown。
- `UCombatAttackComponent` 是 AttackRecord 唯一 registry，Handle 绑定组件 generation 与 Unit life generation；完成、取消、死亡、复活和 EndPlay 都进入同一幂等终结入口。
- `AttackTiming Policy v1` 统一 AttackSpeed、BAT、前摇、Recovery、绝对 ready 时间和动画速率；Scheduler `ScheduleOnce` 不因卡顿补发攻击。
- `AttackTarget` 是持续 Order：每轮重验目标、距离、LOS、朝向、移动和状态，近战命中通过 keyed RNG、DamageSubsystem 与快照 OnHit 公共链路结算，单次 Landed 不出队。
- Modifier 法球采用稳定两阶段仲裁；未胜出候选无副作用，提交成功只扣一次资源，bonus、DamageType 和 OnHit action 在 AttackRecord 创建时快照。

实现决策见 [20 M4 Order 与普攻决策](20-M4-Order-Attack-Decision.md)，已通过 ADR-030..032 关闭 GAP-010。

## 2. 核心源码位置

| 范围 | 主要源码 |
| --- | --- |
| Order 数据与状态机 | `Source/ue_gas/Combat/Order/CombatOrderTypes.h`、`CombatOrderComponent.*` |
| Attack Record 与时序 | `Source/ue_gas/Combat/Attack/CombatAttackTypes.h`、`CombatAttackTimingPolicy.*`、`CombatAttackComponent.*` |
| Ability Order 释放 | `Source/ue_gas/Combat/Ability/CombatAbilitySystemComponent.*`、`CombatGameplayAbility.*` |
| 法球协议 | `Source/ue_gas/Combat/Modifiers/CombatModifierRuntime.*`、`CombatModifierComponent.*` |
| Demo 法球 | `Source/ue_gas/Combat/Demo/CombatDemoModifierRuntimes.*` |
| Unit 与测试场景 | `Source/ue_gas/Combat/Unit/CombatUnitCharacter.*`、`Combat/Tests/CombatTestScenarioActor.*` |
| 自动化 | `Source/ue_gas/Combat/Tests/CombatOrderAttackTests.cpp` |

所有本次新建或实质修改的项目自有类、结构、枚举、函数、关键字段和非显然逻辑均补充中文注释。

## 3. 自动化结果

最终冷启动命令：

```text
UnrealEditor-Cmd.exe ue_gas.uproject -unattended -nop4 -nosplash -nullrhi -nosound -ExecCmds="Automation RunTests Combat.;Quit" -TestExit="Automation Test Queue Empty"
```

验收日志：`Saved/Logs/M4AutomationAcceptance.log`。

- `Combat.OrderAttack`：6/6 Success。
- 既有 `Combat.Ability`、`Combat.Core`、`Combat.Foundation`：17/17 Success。
- 合计：23/23 Success，0 Failed，`TEST COMPLETE. EXIT CODE: 0`。

M4 专项用例覆盖：

1. AttackTiming v1 的攻速边界、前摇、Recovery 与动画速率。
2. FIFO、非排队替换、Stop、旧 Move 回调、成功 PartialPath 到达重验。
3. Ability 正常与取消时 `OrderReleased` exactly once。
4. Attack registry 幂等终结、旧生命 generation 失效和 Scheduler ready。
5. 近战 `AttackTarget` 连续循环、状态/移动阻止及 Damage 结算。
6. 法球两阶段预检、exclusive group、资源只提交一次和 OnHit 快照。

## 4. 构建矩阵

| Engine | Target | 配置 | 结果 |
| --- | --- | --- | --- |
| Installed UE 5.8.1 | `ue_gasEditor` | Win64 Development | Succeeded |
| Source UE 5.8.0（`D:\UE\UE`） | `ue_gasServer` | Win64 Development | Succeeded；20/20 actions |
| Source UE 5.8.0（`D:\UE\UE`） | `ue_gasClient` | Win64 Development | Succeeded；19/19 actions |

三个 Target 均编译了 M4 Order、Attack、Modifier、Ability 和自动化源码，没有 Editor-only 依赖泄漏到 Server/Client。

## 5. 独立联机 smoke

使用 Installed UE 5.8.1 启动两个隐藏的独立 OS 进程：监听服务器加载 `/Game/Combat/Tests/L_CombatTest?listen`，客户端连接 `127.0.0.1:17803`。验收日志：

- `Saved/Logs/M4ListenServer.log`
- `Saved/Logs/M4ListenClient.log`

服务器实际完成 NavMesh 追击、到达重验和连续攻击，关键证据：

```text
M4ScenarioReady ... OrderRuntime=Ready AttackRuntime=Ready AttackOrderAccepted=Yes
AttackLaunched ...
AttackLanded ... Applied=50
AttackLaunched ...
AttackLanded ... Applied=50
Join succeeded
Welcomed by server (Level: /Game/Combat/Tests/L_CombatTest, ...)
```

最终日志没有追击重试耗尽、项目 `LogCombat: Error`、Fatal、Assertion、ensure 或 NetworkFailure。Installed Engine 启动阶段仍会输出实验性 Toolset Python 模块缺失和非 Windows SDK 探测信息，属于引擎环境噪声，不影响项目构建、自动化和联机结果。

## 6. G4 结论

- Move、Cast、Attack、Stop 已通过统一服务器 Order 执行；队列、替换、取消和旧回调隔离均有自动化覆盖。
- 近战持续攻击、前摇、绝对 ready、状态/移动阻止、转向、RNG 和 Damage 形成完整闭环。
- EQS、AI Move、Ability、Attack、Scheduler 的异步边界均有 Handle/generation 防护。
- 法球两阶段协议通过资源唯一提交、稳定分组和 OnHit 快照测试。
- G4：通过。用户已于 2026-08-26 确认 M4 验收通过；M5 尚未授权，不会提前开始。

## 7. 建议人工验收

1. 打开 `/Game/Combat/Tests/L_CombatTest` 并以 Listen Server PIE 运行。
2. 观察 Team 1 单位自动追击 Team 2，进入近战距离后停止移动并转向目标。
3. 观察至少两次 `AttackLaunched` / `AttackLanded`，每次基础伤害 50；单次命中后 Attack Order 不应退出。
4. 可在攻击前摇期间触发 Stop、Stun 或 Death，确认本轮取消且之后没有旧回调补伤害。
5. 可发出排队 Move/Cast/Attack，再用非排队 Order 或 Stop 替换，确认旧 Move/Ability/Attack 回调不推进新队列。
