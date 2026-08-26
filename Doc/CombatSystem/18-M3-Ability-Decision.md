# 18 M3 Ability 与目标实现决策

> 状态：已冻结用于 M3 实现
> 日期：2026-08-26
> 适用任务：TGT-001、ABL-001..006、DEMO-301..303

本文关闭 M3 到期的 GAP-008、GAP-011 与 GAP-024，并将目标校验、分阶段提交和表现时间收敛为可执行规则。

## 1. ADR-027：服务器目标与可见性 v1

- M3 的 `VisibilityPolicy` 固定为 `None`，不读取客户端可见性，也不把屏幕可见当作服务器合法性。
- API 保留显式 visibility policy；请求第一版尚未实现的策略必须返回稳定 Unsupported 失败，不允许静默放行。
- 客户端只能提交 Unit Actor 或有限 Point；任何客户端命中 Actor 列表都直接拒绝。Point AoE 命中集合由服务器 query、去重并按稳定 Actor ID 排序。
- Unit 校验统一处理 Self、Team relation、LifeState、Untargetable、OutOfGame、Invulnerable、MagicImmune、XY 边缘距离与 5 cm tolerance。
- LOS 从来源 targeting origin 到单位/点 aim point 使用 `CombatTargeting` trace channel，并忽略来源和明确目标自身。
- UI、未来 Order 与 Ability 共用 `UCombatTargetingSubsystem`；UI 结果只作预览，Ability 在激活和 cast point 各由服务器复核一次。

## 2. ADR-028：Gameplay 时间与表现 v1

- Cast point、Channel interval 和 Channel duration 只由 `UCombatSchedulerSubsystem` 驱动；Ability 不使用 Actor Tick 或 Timer。
- Montage 与 AnimNotify 只作表现和校准，不能成为 Damage/Heal/Action 的唯一触发源。
- `UAbilityTask_WaitCombatInterval` 持有 repeating 与 finish Handle；Ability End、Cancel、移除 Spec、Avatar 变化或死亡都必须取消 Handle。
- 非引导事件顺序固定为 `CastStarted -> SpellStarted -> OrderReleased -> Ended`。
- 前摇中断固定为 `CastStarted -> Interrupted -> Ended`；引导中断固定为 `Interrupted -> ChannelEnded(true) -> OrderReleased -> Ended`。
- 每个生命周期事件在同一 ActivationId 最多一次；Target snapshot、提交标志与任务句柄只存在于 Instanced Ability 实例。

## 3. ADR-029：分阶段资源提交 v1

- `CanActivate` 对当前 Mana 和 cooldown 做无副作用预检；真正扣费/开始 cooldown 只发生在配置的 commit stage。
- Cost 与 Cooldown 同 Stage 时先整体预检，再构造并按固定顺序应用 Instant Cost GE 和 Duration Cooldown GE；预检失败时两者都不产生。
- ManaCost 在提交点读取当前 Ability Level；Cooldown duration 同时快照当前 CDR。已经开始的 cooldown 不因后续等级、Cost 或 CDR 改变而重排。
- CastStarted 已提交项不会在 SpellStarted 或 AbilityEnded 再检查、再提交；每项由 per-Activation 标志保证最多一次。
- 默认 Cost/Cooldown 均在 SpellStarted 提交，因此前摇目标失效或中断不扣资源。

## 4. M3 数据与授权边界

- 身份链固定为 `AbilitySet Entry -> UCombatGameplayAbility Class -> CDO.AbilityData -> DefinitionId`；AbilityData 不反向引用 Ability Class。
- 同一 Unit 每个 Ability DefinitionId 最多一个 Spec；`Spec.Level` 是唯一运行时等级，越界拒绝而非 clamp。
- AutoCast 是服务器 per-Spec 状态；RPC 只接受本 ASC 已授予、支持 AutoCast 且 Avatar Alive 的 Spec。
- Intrinsic Modifier 的 owner key 包含 AbilitySpecHandle；授予、ActorInfo 重建与 Respawn reconcile 幂等，移除 Spec 时同步清理。
- M3 Action 只启用 Damage、Heal、ApplyModifier、服务器 AoE Query 与 SendGameplayEvent；Projectile/Thinker Action 在资产校验和运行时都明确返回 Unsupported。

## 5. M3 自动化最低断言

- NoTarget、UnitTarget、PointTarget 合法组合及伪造 Actor/Point/命中列表拒绝。
- Friendly/Enemy/Self/Neutral、生命状态、状态 Tag、边缘范围、tolerance 与 LOS。
- 多 Unit 共用 Ability Class 时 ActivationId、Target snapshot 和 commit 标志互不污染。
- CastStarted/SpellStarted/ChannelEnded/OrderReleased/Interrupted/Ended 的固定顺序和 exactly-once。
- Cost/Cooldown 分阶段、同 Stage 原子预检、CDR snapshot、前摇/引导中断与全部 Schedule 清理。
- grant/remove/level/intrinsic/autocast 幂等且服务器权威。
- Self Heal、Unit Magical Damage、Point AoE 三条数据驱动纵向切片通过。
