# 07 Order 与 NavMesh 移动

## 1. 范围

Order 把玩家输入、AI 意图和战斗执行统一为服务器权威状态机。寻路复用 UE NavMesh、AIController、EQS 和 RVO/Detour，不重写参考项目的网格 A*。

```cpp
UENUM(BlueprintType)
enum class ECombatOrderType : uint8
{
    MoveToPoint,
    MoveToUnit,
    AttackTarget,
    CastNoTarget,
    CastPoint,
    CastTarget,
    Stop
};

USTRUCT(BlueprintType)
struct FCombatOrderHandle
{
    GENERATED_BODY()

    int64 Sequence = 0;
    int32 Generation = 0;
};
```

Order payload 保存 Unit life generation、目标 Net identity 或有限位置、AbilitySpecHandle、行为 flags 和来源 PlayerController，不保存可由服务器计算的 Damage/Modifier/命中列表。

## 2. 队列语义

`UCombatOrderComponent` 提供：

```text
IssueOrder(Order, bQueue)
ClearOrders(Reason)
PumpCurrentOrder()
OnEQSFinished(OrderHandle, QueryInstance, Result)
OnMoveFinished(OrderHandle, MoveRequestId, Result)
OnAbilityOrderReleased(OrderHandle, Result)
OnAttackLaunched(OrderHandle, AttackHandle)
OnAttackCycleReady(OrderHandle)
```

- `bQueue=false`：中断当前可中断行为、清空 pending queue、提升 generation 并执行新 Order。
- `bQueue=true`：只追加，达到队列上限则拒绝并返回 FailureTag。
- Stop：永远提升 generation，取消当前 EQS/Move/Ability/Attack，清空 pending。
- 每个异步回调同时比较 OrderHandle、具体请求 Handle 和 Unit life generation。
- 旧 generation、未知请求或已完成 Order 的回调只记录调试信息，不改变状态。

Order 结果使用结构化 `FCombatOrderResult`：Success、Cancelled、TargetInvalid、OutOfRange、Blocked、PathFailed、AbilityRejected、UnitStateBlocked 等稳定 FailureTag。

## 3. 当前 Order 状态机

```text
Queued
  -> Validating
  -> Moving/Chasing
  -> Facing
  -> DispatchingAbility | StartingAttack
  -> WaitingOrderRelease | WaitingAttackReady
  -> Completed | Failed | Cancelled
```

每次状态转换记录 OrderHandle 和原因。Pump 只能由当前有效状态触发，禁止在多个 delegate 中直接 Pop 队列。

### Cast Order

- 距离不足时进入 Moving，不直接激活 Ability。
- 到达后停止移动、权威复核目标/距离/LOS、转向，再激活 Ability。
- 成功进入 cast point 标记 dispatched。
- 非引导在 `OrderReleased` 后 pop；引导在 `AbilityChannelEnded -> OrderReleased` 后处理。
- 前摇/引导中断按 AbilityData policy 继续或清队列。
- 不等待 AbilityEnded、backswing 或 cooldown。

### AttackTarget

- 是持续 Order，直到目标失效、新非排队 Order、Stop、Unit 状态或显式策略终止。
- 每次 AttackLaunched 后等待 attack-ready，再重新验证距离并开始下一轮。
- 不在 OnAttackLanded 时 pop，远程弹体可与后续攻击周期并存。

## 4. 移动执行

- Point：`AAIController::MoveTo` + `FAIMoveRequest`。
- 多单位目标分散：复用 EQS 选择候选位置。
- Unit/Attack/Cast 追击：MoveTo 保存的服务器目标位置，Scheduler Coalesce 低频复查；目标位移超过阈值时重发请求。成功的 PartialPath 也必须用当前 gameplay 距离重验。
- 有效距离统一考虑双方碰撞半径、AttackRange/CastRangeBonus、技能距离和容差。
- 到达后重新验证，不以客户端距离或旧 EQS 结果作为权威。

状态响应：

| 状态 | 当前行为 | 队列 |
| --- | --- | --- |
| Rooted | 取消 MoveRequest，暂停移动 | 保留；Root count 归零重新 Pump |
| Stunned/Hexed | 中断可中断 Ability/attack windup，暂停 Pump | 默认保留，按当前项失败策略恢复/清空 |
| Dead/OutOfGame | 取消全部当前行为 | 清空并提升 generation |
| Motion active | 暂停普通 MoveTo | 位移结束后重判当前队首 |

## 5. Strategy 模板适配要求

当前 `AStrategyUnit` 有可复用的 EQS/MoveTo 思路，但不能直接把 `OnMoveCompleted` 接入队列。现状缺口包括：没有保存/比较 MoveRequestId、OnMoveFinished 不检查 Result、EQS 回调不验证当前实例、StopMovement 与旧回调没有 generation 绑定。

适配必须：

- 保存当前 `UEnvQueryInstanceBlueprintWrapper`、`FAIRequestID` 和 OrderHandle。
- 新 Order/Stop 时取消旧 EQS，调用 AIController StopMovement，并让旧请求进入可识别 Aborted 终态。
- `OnEQSFinished` 比较 QueryInstance、QueryStatus、OrderHandle。
- `OnRequestFinished` 同时比较 RequestId、OrderHandle 和 result code。
- 分别处理 Success、AlreadyAtGoal、Blocked、Aborted、Invalid、PartialPath。
- 只有匹配当前请求的成功结果能推进队列。
- 在 NotifyControllerChanged/EndPlay 解绑旧 PathFollowing delegate，初始化必须幂等。

M4 已按上述约束实现 `UCombatOrderComponent`。CharacterMovement 使用 acceleration-driven PathFollowing 输入；Move 回调出现一帧残余速度时进入同一有界重试，避免把正常到达误判成永久移动阻止。实际监听服务器 NavMesh 追击、到达、转向和连续近战已通过 smoke。

## 6. 碰撞、避让和临时阻挡

M0 已冻结 Combat 几何单位和 Profile：所有距离为 cm、速度为 cm/s；Cast/Attack/Order 默认使用双方半径扣除后的 XY 边缘距离并共享 5 cm 容差。`CombatUnit`、`CombatUnitNoCollision`、`CombatProjectile`、`CombatBlocker`、`CombatCorpse` 和 `CombatTargeting` 的职责及响应矩阵见 [14 M0 设计冻结](14-M0-Design-Freeze.md#6-dec-005碰撞los-和地图单位)。现有 Pawn/WorldDynamic 模板碰撞不直接获得 Combat 语义。

- CharacterMovement 使用 RVO 或 Detour Crowd；选型要在同一地图统一，避免双重避让。
- Capsule 半径映射 hull radius。
- `State.NoUnitCollision` 通过聚合 Tag 响应器切换 collision/avoidance。
- Projectile 使用独立 collision profile，不能继承 Pawn 阻挡配置。

Fissure 等临时阻挡分三阶段：

1. 第一版：物理阻挡体 + 对相关 MoveTo 调 StopMovement/Pump 触发 repath。
2. 第二版：Runtime Navigation Generation + NavModifier，创建/销毁时主动请求相关单位重寻路。
3. 第三版：高频临时阻挡使用局部避障/代价系统，避免频繁重建 NavMesh。

第一版正确性不能依赖异步 NavMesh 已经完成重建。

## 7. 队伍控制和 RPC

M0 固定 TeamId、关系和控制权是三种不同概念：`FCombatTeamId` 决定 Friendly/Hostile/Neutral，`UCombatTeamSubsystem` 是唯一关系入口，CommandingPlayerController 只决定谁能发 RPC。召唤物默认快照 spawn 时的队伍，召唤者之后换队不自动传播；细则见 [14 M0 设计冻结](14-M0-Design-Freeze.md#2-dec-001队伍与目标关系)。Order 不得因为 Controller 相同就推断 Friendly，也不得因为 Friendly 就授予控制权。

PlayerController 将批量 Order RPC 到服务器。服务器验证：

- 调用者是否拥有每个 Unit，Unit 是否仍是同一 life generation。
- Order 数量、队列长度、RPC 频率和位置数值。
- AbilitySpec 已授予且属于该 Unit。
- Target Net identity、队伍/状态和可选可见性。
- 批量命令中重复 Unit 去重，拒绝越权 Unit，不因一个失败执行不相关越权动作。

服务端为每个 Unit 独立生成 OrderHandle。客户端可立即显示指示器，但不将预测 Order 状态作为 Damage/Ability 激活依据。

## 8. 失败与恢复策略

每种 OrderData/AbilityData 明确：

- 目标失效：FailAndContinue、ClearQueue、UseLastKnownPoint。
- 无路径/PartialPath：RetryBounded、FailAndContinue、ClearQueue。
- 状态阻止：PauseUntilTagRemoved 或立即 Fail。
- 引导中断：Continue 或 ClearQueuedOrders。
- 最大追击时间/距离：防止永久挂起和无限 Scheduler 任务。

Retry 必须有次数/时间上限并保留原 OrderHandle；如果生成新 Handle，旧异步回调必须失效。

## 9. 最低验收

- Move/Cast/Attack FIFO、queue true、替换和 Stop 语义正确。
- 旧 EQS/Move/Ability/Attack 回调不会推进新 generation。
- 距离不足自动追击，到达后复核目标/LOS 再执行。
- Root 解除、Motion 结束只恢复当前有效队首。
- AttackTarget 不因单次 Landed pop，Cast 不等待 cooldown/backswing。
- 越权、过频、超长队列、NaN 位置和未授予 Ability 请求被服务器拒绝。
