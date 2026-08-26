# 20 M4 Order 与普攻决策

## 1. 冻结范围

本文件冻结 M4 的 Order generation、移动回调、普攻时序和法球提交协议。它补充 [06 普攻、法球、Projectile 与 Thinker](06-Attack-Projectile-Thinker.md) 与 [07 Order 与 NavMesh 移动](07-Order-Movement.md)，不提前实现 M5 的远程弹体。

## 2. Order 权威状态机

- 每个单位只有一个 `UCombatOrderComponent`，服务器持有 FIFO 队列和唯一当前 Order。
- 非排队请求与 Stop 都先提升 Order generation，再取消当前 EQS、Move、Ability、Attack 和追击 Schedule；旧回调只记录，不推进队列。
- 每个异步回调必须同时匹配 `OrderHandle`、请求句柄和 Unit `LifeGeneration`。
- Cast 在 `OrderReleased` 或激活失败后结束，不等待 AbilityEnded、backswing 或 cooldown。
- `AttackTarget` 是持续 Order；单次 Landed/Failed 不出队，只有 Stop、替换、目标永久失效、状态策略或追击耗尽才结束。
- Root/普通临时移动阻止保留当前队首并暂停；Stun/Hex/Dead 会取消当前前摇，Dead 同时清空队列。

## 3. 移动与动态追击

- 第一版统一使用 `AAIController::MoveTo`，可选 EQS 只负责解析目的点；EQS wrapper、`FAIRequestID` 和 Path 实例都与当前 OrderHandle 绑定。动态目标追击保存服务器位置快照，Scheduler 发现目标位移超过阈值时重发请求。
- Success/AlreadyAtGoal 才可推进；成功的 PartialPath 仍必须按当前 gameplay 边缘距离重验，未到达才按失败分类；Blocked/Invalid/未到达的 PartialPath 进入有界重试，Aborted 只有仍匹配当前请求时才分类处理。
- 追击每 `0.10 s` 由 Combat Scheduler `Coalesce` 复核；目标位移超过 `50 cm` 时重发 Move，请求最多重试 3 次，默认最长追击 10 秒。PathFollowing 通过 acceleration-driven PawnMovement 驱动 CharacterMovement；Move 完成后一帧的残余速度只触发同一有界重试，不会永久暂停攻击。
- 到达只代表重新 Pump；Cast/Attack 必须再次执行服务器 Target、距离与 LOS 校验。

## 4. AttackTiming Policy v1（关闭 GAP-010）

输入全部为有限值：

```text
EffectiveAttackSpeed = clamp(AttackSpeed, 20, 700)
SpeedScale           = 100 / EffectiveAttackSpeed
AttackInterval       = clamp(BaseAttackTime * SpeedScale, 0.20 s, 10.00 s)
AttackPoint          = clamp(BaseAttackPoint * SpeedScale, 0, AttackInterval)
Recovery             = AttackInterval - AttackPoint
AnimationRate        = clamp(EffectiveAttackSpeed / 100, 0.20, 7.00)
```

- `AttackSpeed=100` 保持 UnitData 的 BAT 和基础前摇；公式中途不取整。
- 权威 ready 时间从本轮前摇开始的绝对 World Game Time 计算；attack point 到达后只调度剩余 Recovery。
- Scheduler 使用 `ScheduleOnce`，卡顿只把本轮切到 Ready 一次，不补发攻击。
- 默认移动中不能起手；Order 在起手前停止 Move 并朝向目标。
- `AttackFacingToleranceDegrees` 默认 15°。第一版 Order 服务器即时设置 XY 朝向后复核；Montage、Notify 与动画完成不参与 gameplay 判定。
- M4 只实现近战命中；远程 Record 在 M5 由 Attack Projectile finalize。

## 5. AttackRecord 与随机顺序

- `UCombatAttackComponent` 是本单位 AttackRecord 的唯一 registry；Handle 同时包含记录 ID、组件 generation 和 Unit life generation。
- 前摇取消、目标失效、闪避、伤害失败、Death 与 EndPlay 都通过同一个幂等 finalize/abort 入口结束一次。
- 命中固定顺序为 impact 合法性、Evasion roll、Crit roll、DamageSubsystem、快照 OnHit、Landed hook。
- RNG 使用 `RootEventId + Combat.RNG.Attack.* + AttackHandle` 稳定主体；未到达的阶段不创建后续 roll。
- Stop 在 attack point 前终止 Pending Record；attack point 后近战已立即 finalize，未来远程已发射 Record 不回滚。

## 6. 法球两阶段协议

- Modifier 按 `Priority desc / ApplySequence asc` 生成稳定候选；`CanClaimAttack` 必须无副作用。
- 候选按 `ExclusiveGroup` 仲裁；每组按稳定顺序调用提交，winner 提交失败才尝试下一候选。
- 未胜出、预检失败或提交失败的候选不得扣 Mana、写 cooldown 或修改 Runtime 状态。
- `OnAttackClaimed=true` 表示资源已经提交并锁定该 exclusive group；提交后的非法 bonus/action 会被净化，但不会退回同组下一候选，避免一次攻击重复扣资源。
- winner 的 bonus damage、DamageType override 与 OnHit action 复制进 AttackRecord；之后 Ability/Modifier 升级、移除或关闭 AutoCast 都不能重解释已创建记录。
- M4 的基础 OnHit action 支持快照额外 Damage 与 ApplyModifier；执行仍走 DamageSubsystem/ModifierComponent 公共入口。

## 7. 验收关注

- FIFO、replace、Stop、过期 EQS/Move/Ability/Attack 回调。
- Cast 距离不足追击、到达重验、`OrderReleased` 推进。
- Attack point 前取消、ready 不补发、连续近战不因 Landed 出队。
- Death/Respawn 后旧 Attack/Order handle 失效。
- 法球无副作用预检、提交失败降级、winner 资源只扣一次与 OnHit 快照。
