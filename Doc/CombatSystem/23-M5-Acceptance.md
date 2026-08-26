# 23 M5 验收记录：Projectile、Thinker 与 Motion

## 1. 验收状态

- 里程碑：M5
- Gate：G5 通过
- Task：9/9 已完成
- 用户验收：已验收
- 验收日期：2026-08-26

## 2. 完成范围

| 范围 | 实现结果 |
| --- | --- |
| Projectile | `UCombatProjectileSubsystem` 统一注册 Linear/Tracking Projectile；生成时快照策略与数值；连续分段 Sphere Sweep；稳定命中排序、AlreadyHit、穿透、world block 与全部结束原因 exactly once |
| Tracking/Attack | 支持 `Fizzle` 与 `UseLastKnownPoint`；远程普攻通过 `AttackHandle` 回到原 Attack Record，命中或失败均唯一终结；旧 LifeGeneration 回调无效 |
| Ability Task | 提供 Linear/Tracking Spawn Task 与 Wait Task；Wait 公开 Hit/Fizzled/Finished/Failed；默认 fire-and-forget，显式配置时随 Ability 取消 |
| Thinker | Actor 无 Tick/碰撞；delay、pulse、duration 统一进入 Combat Scheduler；每次 pulse 使用服务器 Targeting 查询并走 Damage/Modifier 公共入口 |
| Motion | Unit 统一 Horizontal/Vertical 通道；只允许严格更高优先级抢占；通过 CharacterMovement 安全移动；结束后按需 Nav 投影并恢复有效 Order generation |
| TwinStick 适配 | 旧 Projectile/AoE Actor 降级为纯表现，不再直接结算伤害或持有独立 Timer gameplay |
| Dragon Slave | 数据驱动 Linear Projectile，支持高速、多目标穿透和伤害/radius/range/speed special 快照 |
| Meat Hook | 首命中伤害并应用 Hook Modifier；Modifier 持有 Horizontal Motion，成功、冲突、终止时均对称清理 |

实现决策见 [22 M5 Projectile、Thinker 与 Motion 决策](22-M5-Projectile-Motion-Decision.md)。

## 3. 自动化结果

Installed UE 5.8.1 冷启动执行全部 `Combat.*` 自动化：

```text
Test Completed. Result={Success}: 27
**** TEST COMPLETE. EXIT CODE: 0 ****
```

M5 新增 4 组测试全部通过：

| Test | 覆盖重点 |
| --- | --- |
| `Combat.ProjectileMotion.Projectile.LinearAndTrackingPolicies` | Linear/Tracking、穿透、友军过滤、AlreadyHit、target-lost、timeout/cancel/stale 与 exactly once |
| `Combat.ProjectileMotion.Projectile.AttackRecordFinalize` | 远程普攻 Record 命中/失败收口与旧回调隔离 |
| `Combat.ProjectileMotion.ThinkerMotion.SchedulerPreemptionAndHookCleanup` | Thinker pulse、稳定过滤、Motion 优先级、Hook 成功/冲突清理 |
| `Combat.ProjectileMotion.Demo.DragonSlaveAndMeatHook` | 两个示例技能仅走公共 Projectile/Modifier/Motion 入口 |

其余 23 个 M0–M4 回归测试同时通过。验收日志：`Saved/Logs/M5AutomationAcceptance.log`。

## 4. 构建结果

| Engine | Target | Configuration | 结果 |
| --- | --- | --- | --- |
| Installed UE 5.8.1 | `ue_gasEditor` | Win64 Development | 正式编译、链接成功 |
| Source UE 5.8.0（`D:\UE\UE`） | `ue_gasServer` | Win64 Development | Succeeded；最终增量 4/4 actions |
| Source UE 5.8.0（`D:\UE\UE`） | `ue_gasClient` | Win64 Development | Succeeded；最终增量 4/4 actions |

三个 Target 均编译 M5 Projectile、Thinker、Motion、Ability Task、Demo 与自动化源码，没有 Editor-only 依赖泄漏到 Server/Client。

## 5. 独立联机 smoke

使用 Installed UE 5.8.1 启动两个隐藏的独立 OS 进程：监听服务器加载 `/Game/Combat/Tests/L_CombatTest?listen` 并监听 `17806`，客户端连接 `127.0.0.1:17806`。关键证据：

```text
IpNetDriver listening on port 17806
M5ScenarioReady ProjectileRuntime=Ready ThinkerRuntime=Ready MotionRuntime=Ready ProjectileSpawned=Yes
Event.Combat.ProjectileSpawned
Event.Combat.ProjectileHit ... Applied=3.750
Event.Combat.ProjectileFinished ... Hits=1
Welcomed by server (Level: /Game/Combat/Tests/L_CombatTest, ...)
Join succeeded
```

验收日志：

- `Saved/Logs/M5ListenServer.log`
- `Saved/Logs/M5ListenClient.log`

自动化、服务端和客户端日志均未发现项目 `LogCombat: Error`、Fatal、Assertion、ensure、NetworkFailure 或失败用例。源码 UE 5.8.0 独立运行不能读取由 Installed UE 5.8.1 生成的预制 Asset Registry，故源码引擎仅承担 Server/Client Target 构建，运行 smoke 继续由与内容版本一致的 Installed UE 5.8.1 执行。

## 6. G5 结论

- Projectile 的 hit、pierce、fizzle、timeout、cancel、stale 与 teardown 都收敛到唯一结束路径，并有自动化覆盖。
- Attack Projectile 只终结原始 Attack Record，跨生命或过期 Handle 不产生重复结算。
- Thinker 的周期权威统一由 Combat Scheduler 承担；Motion 不包含临时周期伤害逻辑。
- Dragon Slave 与 Meat Hook 均由公共数据驱动入口组合完成，TwinStick 模板不再保留第二套 gameplay 权威。
- Editor、Server、Client 构建、27/27 自动化和独立联机 smoke 均通过，M5 可以提交用户验收。
