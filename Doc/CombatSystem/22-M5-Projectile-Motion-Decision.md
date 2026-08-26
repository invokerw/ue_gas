# 22 M5 Projectile、Thinker 与 Motion 决策

## 1. 冻结范围

本文件冻结 M5 的 Projectile registry、连续 sweep、Tracking 目标丢失、Thinker 调度和 Motion 抢占协议。它补充 [04 Modifier、属性与 Motion](04-Modifier-Attributes-Motion.md)、[06 普攻、法球、Projectile 与 Thinker](06-Attack-Projectile-Thinker.md) 与 [09 示例技能](09-Example-Skills.md)。

## 2. Projectile 所有权与时序

- `UCombatProjectileSubsystem` 是服务器权威 Handle registry；`ACombatProjectileActor` 只驱动连续运动、复制位置并把结果回报 registry。
- Spawn 时完整快照 Source、Team、DefinitionId、RootEvent、Ability Activation、可选 AttackHandle、运动与 Impact Action；不保存裸 Ability 实例。
- Linear 与 Tracking 使用 Actor Tick 做连续运动；每帧按 `MaxSimulationStep` 分段执行三维 Sphere Sweep，不进入 Combat Scheduler。
- 命中、world block、最大距离、timeout、target lost、显式取消、Actor EndPlay 与 World teardown 最终都进入同一个幂等 `FinishProjectile`。
- Ability 默认 fire-and-forget；只有显式 `bCancelWithSourceAbility` 的记录才可按 Source + ActivationId 批量取消。

## 3. 稳定碰撞顺序

- Sweep 使用冻结的 `CombatProjectile` Profile，并始终忽略 Source Actor。
- 同一 substep 先按沿路径距离升序；`0.1 cm` 并列内按 Blocker、Unit、其他顺序，再按稳定 Actor identity 排序。
- `AlreadyHit` 在 registry 记录中持有；穿透弹体对同一 Unit 最多执行一次 Impact Action。
- Friendly/Enemy/Self、first-hit、pierce 与 world-stop 都来自快照 HitPolicy，不通过临时修改全局 Collision Response 表达。

## 4. Tracking 目标丢失（关闭 GAP-023）

- `Fizzle`：目标不再是同一 LifeGeneration、进入 Dead/Dying/Respawning、Untargetable 或 OutOfGame 时立即结束，不执行命中动作。
- `UseLastKnownPoint`：发生上述失效时停止读取 Actor，沿最后一次合法位置继续；到达该点后 fizzle，不自动换目标。
- Invulnerable 不属于“丢失”：弹体继续跟踪，impact 时由统一 Target/Damage 规则决定是否无效或 Applied=0。
- Attack Projectile 只保存 AttackHandle；只有命中原 Record 目标才请求 `FinalizeAttackFromProjectile`，其余 Finish 路径使 Record exactly-once Failed。

## 5. Thinker

- `UCombatThinkerSubsystem` 持有 registry，`ACombatThinker` 默认关闭 Tick 和碰撞。
- delay、pulse、duration 全部由 Combat Scheduler 使用绝对 World Game Time 驱动。
- 每次 pulse 通过 `UCombatTargetingSubsystem` 做服务器半径查询、去重与稳定排序，再走 Damage/Modifier 公共入口。
- Finish、EndPlay 与 World teardown 幂等取消全部 Schedule；Ability 结束默认不影响已创建 Thinker。

## 6. Motion

- 每个 Combat Unit 持有一个 `UCombatMotionComponent`，分别维护 Horizontal 与 Vertical 通道；同一通道只有一个 owner。
- 新请求 Priority 必须严格高于现有 owner 才能抢占；旧 owner只收到一次 `Interrupted`。
- Motion 每帧通过 CharacterMovement 的受控移动和 sweep 推进，不在 Tick 内结算周期 Damage。
- 开始 Motion 时取消当前 AI Move 并暂停 Order；完成、阻挡、中断、Death 或 EndPlay 后释放句柄，按配置投影 NavMesh，再只 Pump 当前有效 Order generation。
- Meat Hook 第一版使用 Horizontal 通道，目标点和速度在命中时快照；不持续跟随施法者。

## 7. TwinStick 适配边界

- 旧 `ATwinStickProjectile` 与 `ATwinStickAoEAttack` 降级为表现参考，不再直接调用 `ProjectileImpact`。
- AoE 的时序改用 Combat Scheduler，不保留 Actor Timer gameplay。
- 新 Combat 内容只从 Projectile/Thinker 公共入口创建；模板 Actor 不成为第二套伤害或时序权威。

## 8. 验收关注

- Linear 高速、穿透、AlreadyHit、同 sweep 稳定顺序与全部 Finish exactly once。
- Tracking 两种 target-lost policy、远程 AttackRecord 及跨生命旧回调。
- Ability fire-and-forget、cancel-with-source 与 Wait Task 解绑。
- Thinker 无 Tick/Timer、pulse 稳定目标集与 teardown。
- Motion 抢占、碰撞、死亡、NavMesh 校正和 Order 恢复。
- Dragon Slave 多目标穿透与 Meat Hook 首命中拖拽均只走公共入口。
