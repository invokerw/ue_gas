# M3 可施法切片验收报告

> 日期：2026-08-26
> 用户验收：2026-08-26
> 关联：TGT-001、ABL-001..006、DEMO-301..303、G3
> 结论：10/10 Task 完成，G3 通过，用户验收通过

## 1. 交付结果

M3 已形成服务器权威的目标校验、Ability 生命周期和数据驱动动作纵向闭环：

- `UCombatTargetingSubsystem` 统一处理 NoTarget、UnitTarget、PointTarget、阵营关系、生命/状态、XY 边缘距离、5 cm 容差和 `CombatTargeting` LOS；客户端 Actor 命中列表始终拒绝，AoE 目标由服务器去重并稳定排序。
- `UCombatAbilityData` 保存目标规则、等级 special、cast/channel 时序、Cost/Cooldown commit point、目标丢失策略、动作和 Intrinsic Modifier，并在运行时与编辑器统一拒绝非法组合及 M5 前尚未启用的 Projectile/Thinker Action。
- `UCombatGameplayAbility` 默认 `InstancedPerActor + ServerOnly`，保存每次激活的 Event/Target/LifeGeneration/Level 快照，统一执行 `CastStarted -> SpellStarted -> Channel/OrderReleased -> Ended` 和全部中断清理。
- `UAbilityTask_WaitCombatInterval` 只使用 Combat Scheduler；补帧批次也会淘汰恰好位于 duration 的边界 tick，正常结束、中断、移除 Spec、死亡和 ActorInfo 清理均不残留 Handle。
- Cost 与 Cooldown 在配置阶段整体预检并最多各提交一次；Mana、等级和 CDR 在 commit point 快照，开始后的 cooldown 不重排。
- DataDriven Action v1 已接通 Damage、Heal、ApplyModifier、SendGameplayEvent 和服务器半径查询；公共入口保留 Combat source identity 与 RootEvent 链。
- 授予、等级、移除、AutoCast RPC 和 Intrinsic reconcile 均由服务器 ASC 管理；同 Unit/DefinitionId 唯一，移除活动实例时同步清理 Task、cooldown 和 Intrinsic。
- Demo 类与自动化纵向切片覆盖无目标自我治疗、敌方单位魔法伤害和点目标 AoE。

M3 规则冻结见 [18 M3 Ability 与目标实现决策](18-M3-Ability-Decision.md)，已关闭 GAP-008、GAP-011 和 GAP-024。

## 2. 核心源码位置

| 范围 | 主要源码 |
| --- | --- |
| 目标规则 | `Source/ue_gas/Combat/Targeting/CombatTargetingTypes.h`、`CombatTargetingSubsystem.*` |
| Ability 数据与动作结构 | `Source/ue_gas/Combat/Data/CombatDefinitionData.*`、`Combat/Ability/CombatAbilityTypes.h` |
| Ability 生命周期与 ASC | `Source/ue_gas/Combat/Ability/CombatGameplayAbility.*`、`CombatAbilitySystemComponent.*` |
| Channel Task | `Source/ue_gas/Combat/Ability/AbilityTask_WaitCombatInterval.*` |
| Demo Ability | `Source/ue_gas/Combat/Demo/CombatDemoAbilities.*` |
| 自动化 | `Source/ue_gas/Combat/Tests/CombatAbilityTests.cpp` |

所有本次新建或实质修改的项目自有类、结构、枚举、函数、关键字段和非显然逻辑均补充中文注释。

## 3. 自动化结果

最终冷启动命令：

```text
UnrealEditor-Cmd.exe ue_gas.uproject -unattended -nop4 -nosplash -nullrhi -nosound -ExecCmds="Automation RunTests Combat.;Quit" -TestExit="Automation Test Queue Empty"
```

验收日志：`Saved/Logs/M3AutomationAcceptance.log`。

- `Combat.Ability`：5/5 Success。
- `Combat.Core` 回归：5/5 Success。
- `Combat.Foundation` 回归：7/7 Success。
- 合计：17/17 Success，0 Failed，`TEST COMPLETE. EXIT CODE: 0`。

M3 专项用例覆盖：

1. Friendly/Enemy/Self/Neutral、状态 Tag、边缘范围、有限 Point、visibility Unsupported、LOS 和服务器 AoE 查询。
2. AbilityData schema、future Action 拒绝、grant/remove/level/autocast/intrinsic 幂等与活动实例清理。
3. 多 Unit 共享 Ability Class 的实例隔离、前摇提交、Mana/CDR 快照、同 Stage 原子预检和 cooldown 拒绝。
4. 前摇目标丢失、MagicImmune 阻挡、Magical/Pure Damage、客户端 AoE 命中列表拒绝和多目标结果。
5. Channel 补帧、duration 边界、Stun 中断固定事件顺序、正常结束和全部 Scheduler Handle 清理。

## 4. 构建矩阵

| Engine | Target | 配置 | 结果 |
| --- | --- | --- | --- |
| Installed UE 5.8.1 | `ue_gasEditor` | Win64 Development | Succeeded；最终完整增量 14/14 actions |
| Source UE 5.8.0（`D:\UE\UE`） | `ue_gasServer` | Win64 Development | Succeeded；18/18 actions |
| Source UE 5.8.0（`D:\UE\UE`） | `ue_gasClient` | Win64 Development | Succeeded；18/18 actions |

三个 Target 均编译了 M3 Ability、Targeting、Demo 与自动化源码，没有 Editor-only 依赖泄漏到 Server/Client。

## 5. 独立联机 smoke

使用 Installed UE 5.8.1 启动两个隐藏的独立 OS 进程：监听服务器加载 `/Game/Combat/Tests/L_CombatTest?listen`，客户端连接 `127.0.0.1:17779`。验收日志：

- `Saved/Logs/M3ListenServer.log`
- `Saved/Logs/M3ListenClient.log`

关键证据：

```text
M3ScenarioReady Units=2 Team1=1 Team2=1 ASCActorInfo=Ready State.Alive=Present CoreComponents=Ready AbilityRuntime=Ready
Join succeeded
Welcomed by server (Level: /Game/Combat/Tests/L_CombatTest, ...)
```

两端均无 `LogCombat: Error`、`LogNet: Error`、Fatal、Assertion、ensure 或 NetworkFailure，测试进程已关闭。

## 6. G3 结论

- NoTarget、UnitTarget、PointTarget 三条基础技能纵向切片均通过公共 DataDriven Action 与服务器目标规则。
- Cost/Cooldown 每次激活最多一次；前摇、Channel、状态中断、移除 Spec、死亡和 ActorInfo 变化路径均进入统一清理。
- 伪造 TargetData、未授予 Spec、重复定义、非法等级、资源不足、cooldown 和阻断状态均由服务器拒绝并返回稳定 FailureTag。
- G3：通过。用户已于 2026-08-26 确认 M3 验收通过；M4 未授权且未开始。
