# 07 Order 与 NavMesh 移动

> 本文描述 SAM Gate 通过后的当前实现；迁移决策、测试矩阵和验收证据见 [35 服务器权威单位移动改造](35-Server-Authoritative-Movement-Kickoff.md)。

## 1. 范围

Order 把玩家输入、AI 意图和战斗执行统一为服务器权威状态机。寻路复用 UE NavMesh、`ACombatUnitAIController` 上的 `UCrowdFollowingComponent` 和 EQS，不重写网格 A*。所有 Combat Unit 都由服务器 AIController Possess；PlayerController 只 Possess 无碰撞 Command Pawn，并通过显式 `CommandedUnit` 提交 Order。

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

- Point：统一构造 `FAIMoveRequest`，只允许 `ACombatUnitAIController::MoveTo` 创建服务器 PathFollowing 请求。客户端不接收路径点、不运行 Unit PathFollowing，也不发送 Combat Unit `ServerMove`。
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

## 5. Order 导航适配要求

`Variant_Strategy` 模板已经移除；`UCombatOrderComponent` 直接持有并校验 EQS/MoveTo 异步状态。后续导航扩展不能把裸 `OnMoveCompleted` 回调直接接入队列。

适配必须：

- 保存当前 `UEnvQueryInstanceBlueprintWrapper`、`FAIRequestID` 和 OrderHandle。
- 新 Order/Stop 时取消旧 EQS，调用当前 `ACombatUnitAIController::StopMovement`，并让旧请求进入可识别 Aborted 终态。
- `OnEQSFinished` 比较 QueryInstance、QueryStatus、OrderHandle。
- `OnRequestFinished` 同时比较 RequestId、OrderHandle 和 result code。
- 分别处理 Success、AlreadyAtGoal、Blocked、Aborted、Invalid、PartialPath。
- 只有匹配当前请求的成功结果能推进队列。
- 在 NotifyControllerChanged/EndPlay 解绑旧 PathFollowing delegate；Controller 改变时只重新解析 `ACombatUnitAIController` 上的组件，初始化必须幂等。

`UCombatOrderComponent` 已收敛为单一服务器导航器。CharacterMovement 使用 acceleration-driven PathFollowing 输入；Move 回调出现一帧残余速度时进入同一有界重试，避免把正常到达误判成永久移动阻止。RequestId、OrderHandle、NavigationAttemptGeneration 和 LifeGeneration 共同淘汰陈旧回调。

玩家拥有 Unit 仍通过 `Unit.Owner` 建立 Order RPC 与 ASC Mixed replication，但移动网络角色与所有权分离：owning client 和其他客户端看到的 Unit 都是 `ROLE_SimulatedProxy`。服务器 `UCrowdFollowingComponent` 产生局部 steering，服务器 Capsule sweep 产生最终几何结果，再由 ReplicatedMovement 向所有客户端收敛。

### 5.1 当前 Demo 点击移动

`Aue_gasPlayerController` 的输入处理不再直接调用 `AddMovementInput` 或 `SimpleMoveToLocation`：

- `BP_CombatDemoGameMode` 继承 `Aue_gasGameMode`；默认出生由原生 GameMode 独立生成 Unit 与 Command Pawn，先完成 Unit 的 AIController/Owner 绑定，再只把 Command Pawn 交给 PlayerController Possess。禁止在 `PlayerController::OnPossess` 中嵌套迁移 Unit。

- 鼠标右键（当前 `IMC_Default` 映射）/触摸按下命中地面后，立即构造替换型 `MoveToPoint` 批次并调用 `ServerIssueOrderBatch`。
- 按住拖动时，只在距离上一目标至少 25 cm 且距离上一请求至少 0.20 秒时重发，避免 Reliable RPC 按帧发送。
- 松开时若最终落点明显变化，则补发一次最终目标；光标特效仍是客户端可丢弃反馈。
- `bAppendToExistingQueue=false`，因此新的点击移动通过服务器 Order generation 取消并替换旧行为，不在客户端直接 `StopMovement`。
- `GetReadyCommandedUnit()` 会等待 `CommandedUnit` 与 Unit Owner 都复制就绪才允许发 RPC；客户端仅播放光标/路径预览，不启动 Unit PathFollowing。

## 6. 碰撞、避让和临时阻挡

M0 已冻结 Combat 几何单位和 Profile：所有距离为 cm、速度为 cm/s；Cast/Attack/Order 默认使用双方半径扣除后的 XY 边缘距离并共享 5 cm 容差。`CombatUnit`、`CombatUnitNoCollision`、`CombatProjectile`、`CombatBlocker`、`CombatCorpse` 和 `CombatTargeting` 的职责及响应矩阵见 [14 M0 设计冻结](14-M0-Design-Freeze.md#6-dec-005碰撞los-和地图单位)。现有 Pawn/WorldDynamic 模板碰撞不直接获得 Combat 语义。

- 普通移动只使用服务器 Detour Crowd；`UCrowdFollowingComponent` 参数集中在 `ACombatUnitAIController`，CharacterMovement RVO 固定关闭。
- Capsule 半径映射 hull radius。
- `State.NoUnitCollision` 退出 Crowd；Root/Stun/Motion 使用 ObstacleOnly；Dead/UnPossess 使用 Disabled；恢复 Alive 后重新加入。
- `MaxDepenetrationWithPawnAsProxy=0` 禁止客户端 SimulatedProxy 因本地 Pawn 重叠获得非权威水平位移；服务器 Authority 解穿透和硬阻挡不受影响。
- Projectile 使用独立 collision profile，不能继承 Pawn 阻挡配置。

Fissure 等临时阻挡分三阶段：

1. 第一版（M6 已实现）：`ACombatFissureBlocker` 物理阻挡体 + 对路径穿过 blocker bounds 的 Move/Chase 主动取消当前 MoveRequest，并 Pump 同一 OrderHandle 触发 repath；每次尝试递增 attempt generation，旧回调不能推进新尝试。
2. 第二版：Runtime Navigation Generation + NavModifier，创建/销毁时主动请求相关单位重寻路。
3. 第三版：高频临时阻挡使用局部避障/代价系统，避免频繁重建 NavMesh。

第一版正确性不依赖异步 NavMesh 已经完成重建；blocker 创建与 Scheduler 到期移除都会通知受影响 Order。

## 7. 队伍控制和 RPC

从 owning client 的 Command Pawn 输入、显式 CommandedUnit RPC、服务器 AIController 求路移动到全客户端 SimulatedProxy 收敛的完整时序见 [34 客户端与服务器交互流程](34-Client-Server-Interaction.md)。

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
- 玩家拥有 Unit 在 owning client 仍为 SimulatedProxy；客户端无 PathFollowing/RequestMove/路径 RPC 生产分支。
- Dedicated 双客户端在基础 RTT、约 80 ms 和约 150 ms + 2% 丢包下对撞收敛，静止 Unit 不产生服务器未认可的水平位移。
