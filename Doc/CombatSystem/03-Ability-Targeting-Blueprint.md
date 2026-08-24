# 03 Ability、目标与蓝图接口

## 1. 行为标签

Ability 行为只使用 GameplayTag，不再维护可漂移的枚举/位掩码副本：

```text
Ability.Behavior.NoTarget
Ability.Behavior.UnitTarget
Ability.Behavior.PointTarget
Ability.Behavior.Passive
Ability.Behavior.Channelled
Ability.Behavior.AoE
Ability.Behavior.Attack
Ability.Behavior.AutoCast
Ability.Behavior.IgnoreSilence
Ability.Behavior.IgnoreMagicImmune
Ability.Behavior.IgnoreUntargetable
```

目标阵营标签：

```text
TargetTeam.None
TargetTeam.Enemy
TargetTeam.Friendly
TargetTeam.Both
```

编辑器复选 UI 最终写入同一个 TagContainer，不保存第二份配置。

## 2. 目标校验契约

M0 已冻结队伍身份和关系：玩法层使用 `FCombatTeamId`（`0` Neutral camp、`1..254` 普通队、`255` NoTeam/Invalid）以及 `UCombatTeamSubsystem` 的单一 Relation API。相同有效 TeamId 为 Friendly，不同有效 TeamId 默认 Hostile，Neutral camp 默认也与其他队 Hostile；显式 diplomacy table 可以覆盖为 Friendly/Hostile/Neutral。`Self` 独立于队伍关系，由 `bAllowSelf` 控制。完整值域、召唤物快照和换队规则见 [14 M0 设计冻结](14-M0-Design-Freeze.md#2-dec-001队伍与目标关系)。

`TargetTeam.Friendly` 接受 Friendly，`TargetTeam.Enemy` 接受 Hostile，`TargetTeam.Both` 接受二者；显式 Neutral relation 只有 `bAllowNeutralRelation=true` 才接受。NoTeam、Self 禁止和关系不匹配分别返回稳定 `Combat.Failure.Target.*`，Order/Ability/Projectile 不得翻译或另建阵营错误。

目标校验由 C++ 公共层统一实现，蓝图只声明规则。每次校验至少考虑：

- 调用者是否拥有 Unit 和 AbilitySpec，AbilitySpec 是否有效且已授予。
- 目标类型：无目标、单位、点、方向或组合。
- 队伍关系：Friendly/Enemy/Both，不能直接比较枚举后散落特判。
- 目标状态：Alive/Dead、Untargetable、OutOfGame、Invulnerable、MagicImmune。
- 距离：施法距离 + CastRangeBonus + 双方碰撞半径；使用统一单位和容差。
- 世界位置：有限值、可选 NavMesh 投影、最大请求范围和地图边界。
- 可见性与视线：第一版可配置关闭，但 API 必须保留明确结果，不能默认客户端可见即服务器可见。
- Point/AoE 形状：服务器重算，不接收客户端传入的命中 Actor 列表。

范围/LOS 使用 M0 单一几何规则：运行时单位为 cm，Cast/Attack/Order 默认用 XY 边缘距离，固定 `CombatRangeToleranceCm=5`；LOS 从 Combat targeting origin 到 aim point 走 `CombatTargeting` trace，忽略 Source/Target 自身并由 WorldStatic、有效 WorldDynamic 和 CombatBlocker 阻挡。完整 Profile 和失败 Tag 见 [14 M0 设计冻结](14-M0-Design-Freeze.md#6-dec-005碰撞los-和地图单位)。

建议返回结构化 `FCombatTargetValidationResult`，包含 `bValid`、稳定 `FailureTag` 和可选修正位置。Order、Ability 激活和 UI 预览共享规则；UI 结果只作提示，服务器结果才权威。

目标数据使用 `FGameplayAbilityTargetDataHandle`。客户端可提交 Actor NetGUID 或位置，服务器在进入 cast point 前校验一次，在 cast point 完成时再次校验并生成权威快照。

## 3. Ability 基类

```cpp
UCLASS(Abstract, Blueprintable)
class UCombatGameplayAbility : public UGameplayAbility
{
    GENERATED_BODY()

public:
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
    TObjectPtr<UCombatAbilityData> AbilityData;

    UFUNCTION(BlueprintCallable)
    float GetSpecialValue(FName Key) const;

protected:
    virtual bool CanActivateAbility(...) const override;
    virtual void ActivateAbility(...) override;
    virtual void EndAbility(..., bool bWasCancelled) override;

    UFUNCTION(BlueprintNativeEvent)
    void ReceiveSpellStart(const FCombatAbilityContext& Context);

    UFUNCTION(BlueprintNativeEvent)
    void ReceiveChannelTick(
        const FCombatAbilityContext& Context,
        const FCombatScheduledTickContext& TickContext);

    UFUNCTION(BlueprintNativeEvent)
    void ReceiveChannelFinish(const FCombatAbilityContext& Context, bool bInterrupted);
};
```

默认 `InstancedPerActor`，同一 Ability 未结束时禁止重入。需要同单位并发多份执行的能力显式改为 `InstancedPerExecution` 并重新评估复制；`NonInstanced` 只用于完全无状态、无蓝图、无 latent task 的能力。

每次激活缓存：ActivationId、权威 TargetData、目标快照、阶段、资源提交标记、OrderHandle 和所有 Task/Delegate Handle。所有退出路径最终走 `EndAbility` 清理。

## 4. AbilityData 生命周期策略

| 配置 | 默认值 | 说明 |
| --- | --- | --- |
| `CostCommitPoint` | SpellStarted | CastStarted/SpellStarted；默认前摇中断不扣资源 |
| `CooldownCommitPoint` | SpellStarted | CastStarted/SpellStarted/AbilityEnded |
| `ChannelInterruptOrderPolicy` | Continue | Continue 或 ClearQueuedOrders |
| `bCancelProjectilesWithAbility` | false | 默认 fire-and-forget Projectile 脱离 Ability 生命周期 |
| `bRequireLineOfSight` | 按技能 | 服务器 cast point 时复核 |
| `TargetLostPolicy` | Fail | Fail、UseLastKnownPoint 或技能特定策略 |

蓝图不能在执行中临时决定这些策略。`bCostCommitted` 和 `bCooldownCommitted` 保证每次 ActivationId 最多提交一次。

### 4.1 分阶段提交协议

基类提供统一 `PreflightAndCommitStage(Stage)`，禁止蓝图直接组合 `CheckCost`、`CheckCooldown` 和分项 Commit：

1. 激活前的 `CanActivateAbility` 检查当前 Cost/Cooldown，确认可以进入 Ability；此时不产生副作用。
2. 到达某个 commit point 时，只处理“配置为当前 Stage 且尚未提交”的项目。
3. Cost 和 Cooldown 都在当前 Stage 时，先用一次无副作用 preflight 同时检查，再走 GAS 的完整 `CommitAbility`/项目级原子 helper；任何检查失败都不得提交其中一项。
4. 只有 Cost 在当前 Stage 时，检查并调用一次 `CommitAbilityCost`；只有 Cooldown 在当前 Stage 时，检查并调用一次 `CommitAbilityCooldown`。
5. 已在 CastStarted 提交的 Cost/Cooldown，在 SpellStarted 或 AbilityEnded 不再检查，也不再提交；不能因为蓝量已经扣除或 cooldown tag 已经存在而错误中断。
6. 每个成功提交立即设置对应幂等标记并记录 Stage/EventId；失败返回结构化 FailureTag。

如果项目级 Cost/Cooldown GameplayEffect 的应用可能触发同步副作用，原子 helper 必须预先构造并验证两份 Spec，再按固定顺序应用；若无法保证不可失败，则禁止把两个提交点配置到同一 Stage 以外的自定义组合。

## 5. 施法生命周期

1. `CanActivateAbility`
   - 检查死亡、眩晕、妖术、沉默、冷却、魔法、目标规则和并发策略。
2. `ActivateAbility`
   - 缓存 TargetData/ActivationId/OrderHandle。
   - 根据配置提交 cast-start 阶段资源。
   - 广播一次 `AbilityCastStarted`。
   - 播放 cast point montage 或启动受管理的等待 Task。
3. Cast Point 完成
   - 服务器复核目标、阵营、距离、状态、LOS 和资源。
   - 调用 `PreflightAndCommitStage(SpellStarted)`；只检查/提交当前阶段尚未提交的项目，不能重新检查 CastStarted 已提交项。
   - 调 `ReceiveSpellStart` 或 DataDriven Actions。
   - 广播一次 `AbilitySpellStarted`，向 owner Modifier 派发 `OnAbilityExecuted`。
4. Channel
   - `UAbilityTask_WaitCombatInterval` 向 Scheduler 注册 repeating Handle。
   - Tick 使用 TickContext；被状态、死亡、目标丢失或 Order 中断时结束。
   - 广播一次 `AbilityChannelEnded(bInterrupted)`。
5. Backswing
   - 只作为动画/表现，不锁 ASC，不阻止 Order 继续。
6. Cooldown
   - 使用 Cooldown GE；默认 SpellStarted 提交。
7. Order 释放
   - 非引导在 SpellStarted 后广播 `OrderReleased`。
   - 引导在 ChannelEnded 后广播。
8. 完整结束
   - 统一取消 Task/Schedule、解绑 Delegate、清上下文并广播 `AbilityEnded`。

事件语义：

| 事件 | 时机 | 主要消费者 |
| --- | --- | --- |
| `AbilityCastStarted` | 进入 cast point | UI、动画、打断、日志 |
| `AbilitySpellStarted` | 技能实际生效 | Modifier 被动、技能日志 |
| `AbilityChannelEnded` | 引导正常结束/中断 | 清理持续效果 |
| `OrderReleased` | 当前施法 Order 可继续 | OrderComponent |
| `AbilityInterrupted` | 生效前中断或引导中断 | Order 失败策略、UI |
| `AbilityEnded` | Task/Delegate 已全部清理 | 生命周期断言、表现回收 |

固定顺序：

- 引导中断：`AbilityInterrupted -> AbilityChannelEnded(true) -> OrderReleased/清队列 -> AbilityEnded(true)`。
- 前摇中断：`AbilityInterrupted -> Order 失败策略 -> AbilityEnded(true)`，不发送 ChannelEnded。

所有事件每次 ActivationId 最多一次。`OnAbilityExecuted` 统一表示“技能已经生效”，引导技能在引导开始时触发。

## 6. Ability 授予、等级和自动施放

M0 已关闭授予身份和产品默认值，完整契约见 [14 M0 设计冻结](14-M0-Design-Freeze.md#4-dec-003ability-授予等级和-autocast)。第一版规则：

- 服务器从 UnitData/AbilitySet 授予 Ability；客户端不得指定 Ability 类、DefinitionId 或等级。
- `FGameplayAbilitySpec.Level` 是运行时权威等级，DataAsset special 按该等级读取并做边界校验。
- Ability Class 与 AbilityData DefinitionId 必须一一对应；重复 DefinitionId 在编辑器校验和启动检查中报错。
- AbilityData 不反向引用 Class；AbilitySet 只保存 Class、InitialLevel、初始 AutoCast 和 grant flags，不保存运行时 SpecHandle。
- 同一 Unit 每个 Ability DefinitionId 最多一个 Spec；同一 grant source 重复初始化幂等，其他冲突来源明确失败。
- 等级合法范围为 `1..MaxLevel`，越界拒绝而非 clamp；special 数组长度只能为 1 或 MaxLevel。
- 升级、降级、移除均为服务器 API，并产生 CombatLog 事件。
- 移除 Ability 时，默认取消该 Ability 的活动实例；已脱离 Ability 生命周期的 Projectile/AttackRecord 按快照继续。
- AutoCast 是 per-Spec 服务器状态，Order RPC 只能请求切换，服务器复核可切换性、归属和生命状态。
- Intrinsic Modifier 以 AbilitySpecHandle + DefinitionId 作为 owner key；授予、ActorInfo 重建和 respawn reconcile 幂等，移除 Spec 后不得残留。
- AbilitySpec、等级、AutoCast 和 cooldown 默认跨 Death/Respawn 保留；活动实例在 Dying 被取消。

技能点、经验和物品临时授予不属于第一版；未来只能调用相同服务器 API，不能成为第二套等级权威来源。

## 7. DataDriven Actions

DataDriven Action schema v1 定义：

```text
Action.Damage
Action.Heal
Action.ApplyModifier
Action.SpawnLinearProjectile
Action.SpawnTrackingProjectile
Action.CreateThinker
Action.SendGameplayEvent
```

可执行范围按里程碑开放：

- M3：`Damage`、`Heal`、`ApplyModifier`、服务器 AoE Query 和 `SendGameplayEvent`。
- M5：在 ProjectileSubsystem/Thinker 完成后启用 `SpawnLinearProjectile`、`SpawnTrackingProjectile` 和 `CreateThinker`。
- M5 前可以冻结并序列化完整 schema，但资产校验必须拒绝在当前运行能力中使用尚未启用的 Action，执行器也返回稳定 `Combat.Failure.ActionUnsupported`，不能静默 no-op 或临时直连 SpawnActor。

Action 负责声明目标选择、参数 key 和输出，不允许直接写 Health、Transform 或 Runtime 容器。复杂技能使用蓝图子类，例如 Dragon Slave、Meat Hook、Fissure；公共权限、校验、资源和生命周期仍走基类。

## 8. 蓝图 API

Ability 事件/查询：

```text
ReceiveSpellStart(Context)
ReceiveChannelTick(Context, TickContext)
ReceiveChannelFinish(Context, bInterrupted)
GetSpecialValue(Key)
GetCaster()
GetTargetActor()
GetTargetLocation()
DealDamage/Heal（封装统一 Subsystem）
ApplyModifier（封装服务器入口）
SpawnProjectile/CreateThinker（返回稳定 Handle）
```

蓝图约束：

- 数值来自 DataAsset special，不硬编码平衡数值。
- 不直接 `SetHealth`、`ApplyGameplayEffectToTarget` 或 `SpawnActor` 绕过公共封装。
- 不保存裸 Ability/Runtime 指针给长期 Projectile、Thinker 或异步回调。
- 不自建 Tick/Timer；使用 AbilityTask/Scheduler。
- BlueprintNativeEvent 的输入优先使用快照或只读结构；可变事件只在明确 Hook 中出现。

## 9. 最低验收

- 不同单位共享同一 Ability 类时，目标和提交标记互不污染。
- 前摇前后中断符合 commit point，Cost/Cooldown 最多提交一次。
- CastStarted 已提交项不会在 SpellStarted/AbilityEnded 被再次检查；同一 Stage 的 Cost/Cooldown 不会只提交一半。
- TargetData 在服务器复核；伪造归属、距离、位置和 Actor 列表被拒绝。
- Channel tick 不随帧率漂移，所有退出路径取消 Scheduler Handle。
- 六类生命周期事件的数量和顺序可自动化断言。
- Ability 移除能清理 intrinsic modifier 和活动实例，不回滚已发射快照对象。
- M3 对尚未启用的 Projectile/Thinker Action 在资产校验和运行时都明确拒绝，M5 启用后使用同一 schema。
