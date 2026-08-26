# 06 普攻、法球、Projectile 与 Thinker

## 1. AttackRecord 所有权

```cpp
USTRUCT(BlueprintType)
struct FCombatAttackHandle
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    TWeakObjectPtr<AActor> Attacker;

    UPROPERTY(BlueprintReadOnly)
    int64 Sequence = 0;

    UPROPERTY(BlueprintReadOnly)
    uint32 LifeGeneration = 0;

    bool IsValid() const;
};

USTRUCT(BlueprintType)
struct FCombatAttackRecord
{
    GENERATED_BODY()

    FCombatAttackHandle Handle;
    TWeakObjectPtr<AActor> Attacker;
    TWeakObjectPtr<AActor> Target;
    float BaseDamage = 0.0f;
    float BonusDamage = 0.0f;
    ECombatDamageType DamageType = ECombatDamageType::Physical;
    ECombatAttackState State = ECombatAttackState::Pending;
    FCombatModifierHandle ClaimedOrb;
    TArray<FCombatOnHitAction> OnHitActions;
};
```

`UCombatAttackComponent` 的 `TMap<int64, FCombatAttackRecord>` 是唯一 registry，Sequence 单调递增。Handle 通过 Attacker 找组件，并同时校验组件当前 Unit life generation；Projectile、Cue 和异步回调只持有 Handle，不复制整份 Record。

Component EndPlay 把所有 Pending 原子终结为 Failed、解绑 Projectile 回调并清空 registry。Dying 清理 Attack registry/内部 generation，Respawn 在预检后提升 Unit LifeGeneration；旧 Handle 即使 Actor 和 Sequence 仍可解析也必须失效，防止旧回调命中新生命。

## 2. 普攻状态机

```text
Ready
  -> Windup/PendingRecord
    -> Launched (近战可立即进入 Finalize)
      -> Landed | Failed
  -> Recovery/AttackReadySchedule
  -> Ready
```

流程：

1. 校验目标、队伍、状态、距离、视线和 attack-ready。
2. 创建 Pending Record，快照 BaseDamage、目标和来源等级/属性。
3. 收集法球候选并仲裁。
4. 进入前摇，保存 AttackHandle、OrderHandle 和 ScheduleHandle。
5. Attack point 到达时广播一次 `AttackLaunched`。
6. 近战立即请求 Finalize；远程生成只持有 AttackHandle 的 Tracking Projectile。
7. 下一次 attack-ready 使用 ScheduleOnce；卡顿只切一次 Ready，不补发攻击。
8. 所有命中、闪避、目标失效、Projectile fizzle 和 EndPlay 都调用幂等 `FinalizeAttack`。
9. Finalize 只允许 Pending 原子转成 Landed/Failed；随后广播对应 Hook 和一次 RecordDestroy，再移除 registry。

前摇内 Stop/Disarm/Death 使 Record Failed。Attack point 后 Stop 只阻止后续攻击，不回滚已发射快照，除非明确支持 disjoint/cancel。

## 3. 攻速与前摇

AttackComponent 统一计算：

- Attack interval：由 BaseAttackTime 和聚合 AttackSpeed 计算并做最小/最大 clamp。
- Attack point：UnitData 配置基础前摇，可按 attack speed 缩放；具体缩放公式集中管理。
- Ready time 使用绝对服务器 game time，不累计帧 DeltaSeconds。
- 动画播放速率由同一计算结果投影，动画结束不作为权威 attack point。
- 转向、目标角度容差和移动中能否起手是 UnitData/Attack policy，不由 montage 临时决定。

M4 已按 `AttackTiming Policy v1` 落地公式、上限和转向策略并关闭 [GAP-010](12-Decisions-Gaps.md)；完整公式见 [20 §4](20-M4-Order-Attack-Decision.md#4-attacktiming-policy-v1关闭-gap-010)。

## 4. 法球仲裁

法球使用两阶段协议：

1. 按 `Priority -> ApplySequence` 收集候选，调用无副作用 `CanClaimAttack`。
2. 每个 exclusive orb group 选择 winner。
3. 对 winner 调 `OnAttackClaimed` 提交魔法/冷却，并把 bonus、DamageType、ProjectileData 和 OnHitActions 写入 Record 快照。
4. 提交失败继续尝试下一个候选；未胜出者不得扣资源或改变状态。`OnAttackClaimed=true` 代表提交已发生并锁定分组，后续输出即使需要净化也不再尝试同组候选，防止重复消费。

M4 已实现近战 AttackRecord、时序、RNG、Damage 与法球快照；本文件后续 Projectile/Thinker 部分仍属于 M5 范围。

命中时只执行 Record 中的 OnHitActions，不重新读取可能已升级/移除的 Ability 实例。是否允许多个非互斥 proc 叠加由 group/tag 规则声明。

## 5. 闪避、暴击与命中顺序

建议第一版顺序：

```text
Finalize request
  -> target still valid for impact
  -> accuracy/evasion roll
  -> crit/proc roll and immutable final attack spec
  -> DamageSubsystem
  -> on-hit actions that require landed attack
  -> Landed hooks
```

随机数必须来自 M0 冻结的 Combat RNG v1，上下文包含 RootEventId、DomainTag、AttackHandle/StableSubjectId 和 Ordinal。固定顺序是 impact 合法性、Evasion、Crit、再按 Modifier 稳定顺序执行 proc；未到达的阶段不创建 roll record。完整 keyed roll 和复现字段见 [14 M0 设计冻结](14-M0-Design-Freeze.md#52-combat-rng-v1)。Miss/evade 不调用 Damage，但仍发送 AttackFail/RecordDestroy。Break/SpellBlock 与普攻法球的交互作为具体 Modifier 规则，不隐含在 Projectile 中。

## 6. Projectile 数据和 Actor

`FCombatProjectileSpec` 在服务器 Spawn 时完成快照，至少包含：

- Source、Team、ProjectileData DefinitionId。
- 速度、宽度、距离、collision policy 和 AlreadyHit policy。
- Damage/GameplayEvent payload、RootEventId。
- 完整 `FCombatSourceContext`，保留 Ability/Modifier/Projectile 来源链。
- 可选 AttackHandle、Ability ActivationId。
- 目标/最后已知位置策略和超时时间。

Projectile 不保存裸 Ability instance。Subsystem 为 Handle 维护 Active/Finished；Hit、timeout、overlap、fizzle 和 EndPlay 最终进入幂等 `FinishProjectile`。

### Linear

- 保存上一帧位置，服务器每帧由 Previous 到 Current 做 Sphere/Capsule Sweep。
- 按最大步长 substep，避免高速穿透。
- `bDestroyOnFirstHit` 或穿透；AlreadyHit 保证每个 Victim 至多一次。
- 同一 sweep 多命中按沿路径距离排序，再用稳定 Actor identity 打破并列，避免物理回调顺序影响结果。

### Tracking

- 持有 Target weak pointer，每帧朝权威目标位置移动。
- 目标 Dead/OutOfGame/Untargetable 时按 TargetLostPolicy fizzle 或去最后位置。
- 不进入 Combat Scheduler。

### Attack Projectile

- 只保存 AttackHandle。
- 命中/结束请求 AttackComponent Finalize，不自行结算伤害。

M0 已冻结 `CombatUnit`、`CombatProjectile`、`CombatBlocker` Object Channel，`CombatTargeting` Trace Channel，以及对应 Profile。WorldStatic/WorldDynamic/CombatBlocker 的阻挡、友军命中、Source ignore、穿透和 first-hit 由集中 `FCombatProjectileHitPolicy` 决定；同一 sweep 按 path distance、Blocker-before-Unit、稳定 Net identity 排序。完整矩阵见 [14 M0 设计冻结](14-M0-Design-Freeze.md#6-dec-005碰撞los-和地图单位)。

## 7. AbilityTask 边界

```text
UAbilityTask_SpawnLinearProjectile
UAbilityTask_SpawnTrackingProjectile
UAbilityTask_WaitProjectileResult (optional)
```

Spawn Task 校验参数、请求 Subsystem 创建、返回 Handle，随后立即 EndTask。Projectile 按快照独立结算。

只有 Ability 必须保持 Active 到弹体结束时才使用 Wait Task，暴露 OnHit/OnFinished/OnFizzled。Ability 提前结束只解除订阅，默认不销毁 Projectile。显式 `bCancelWithSourceAbility` 时由 Subsystem 根据 ActivationId 统一取消。

## 8. 表现与网络 reconcile

- 服务器生成权威 Actor并结算命中。
- 客户端可预测纯表现 Projectile，不预测伤害。
- 权威 Actor 和预测表现使用同一 ProjectileId reconcile，不能显示两份。
- GameplayCue/Niagara/Sound 读取 ProjectileData；视觉资源不进入命中判定。
- Actor pooling 是性能优化，必须在幂等 EndPlay 和 Handle generation 稳定后再启用。

当前 `ATwinStickProjectile` 只作表现/碰撞参考；其 `NPC->ProjectileImpact` 必须替换为 Damage/Event 公共路径。

## 9. Thinker 与 AoE

`ACombatThinker`：

- 隐藏或只显示调试几何；不参与单位碰撞，无血条。
- 持有 Source、Ability DefinitionId、Team、Duration 和 RootEventId 快照。
- 可挂形状组件或执行权威查询。
- 默认关闭 Actor Tick；delay/pulse 由 Scheduler 驱动。
- 生命周期结束幂等取消 schedule、移除 child modifier/collision 并广播一次 Finished。

适用于延迟爆炸、Healing Ward、毒圈、持续 AoE 和 Fissure blocker。短生命周期效果也可在 Ability 内 `WaitDelay -> Query -> Apply -> EndTask`。

AoE 查询必须：

- 使用统一 Team/TargetFilter。
- 对返回 Actor 去重并使用稳定排序。
- 服务器重算半径/形状，不接受客户端目标列表。
- 明确快照或实时读取数值；默认生成时快照来源 Ability 等级和 special。

当前 `ATwinStickAoEAttack` 的 Actor Timer 和直接 `ProjectileImpact` 只作改造参考。

## 10. 最低验收

- AttackRecord 的所有完成路径 exactly once；过期/重复 Handle 不产生伤害。
- Respawn 后旧 LifeGeneration 的 AttackHandle/Projectile 回调不能查找或终结新生命的 Record。
- 远程弹体飞行不阻塞下一次 ready，Stop 不回滚已发射 Record。
- 法球候选无副作用，winner 提交失败可降级，on-hit 使用快照。
- Linear 高速/穿透不漏撞、不重复命中；Tracking 目标丢失策略明确。
- Ability 提前结束后 fire-and-forget 仍结算；cancel-with-source 可统一取消。
- 预测表现 reconcile 后不重复显示。
- Thinker 无 Actor Tick/Timer，查询目标去重且稳定。
