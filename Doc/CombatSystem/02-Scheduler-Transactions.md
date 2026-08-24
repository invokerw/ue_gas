# 02 调度、事务与时序

## 1. 时序分类

战斗更新分三类：

| 类型 | 更新方式 | 适用对象 |
| --- | --- | --- |
| 事件驱动 | 状态变化时同步执行 | Damage/Heal、GE/Tag 变化、Projectile Hit、Order 完成 |
| 逻辑定时 | 服务器 Scheduler 按绝对 game time | Channel、Modifier Think、DOT/HOT、attack-ready、追击、Thinker pulse |
| 连续运动 | 每帧 DeltaSeconds + sweep/substep | Projectile、CharacterMovement、RootMotionSource/Motion |

禁止每个 Ability、Runtime 或 Thinker 各自开启 Tick/Timer。`UCombatSchedulerSubsystem` 第一版为每个 World 一个 `UTickableWorldSubsystem`；只在 Standalone、Listen Server、Dedicated Server 执行权威 gameplay callback，纯 Client 仅做表现插值。

确定性目标限于同一 World、同一到期批次的稳定顺序，不承诺跨平台 lockstep。

## 2. 公共接口

```cpp
UENUM()
enum class ECombatCatchUpPolicy : uint8
{
    ExecuteAllBounded,
    Coalesce,
    SkipExpired
};

USTRUCT(BlueprintType)
struct FCombatScheduleHandle
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    int64 Id = 0;

    UPROPERTY(BlueprintReadOnly)
    int32 Generation = 0;
};

USTRUCT(BlueprintType)
struct FCombatScheduledTickContext
{
    GENERATED_BODY()

    double ScheduledTime = 0.0;
    double ActualTime = 0.0;
    float Interval = 0.0f;
    int32 TickIndex = 0;
    int32 TickCount = 1;
};

FCombatScheduleHandle ScheduleOnce(
    UObject* Owner,
    double Delay,
    int32 Priority,
    FCombatScheduledDelegate Callback);

FCombatScheduleHandle ScheduleRepeating(
    UObject* Owner,
    double InitialDelay,
    double Interval,
    int32 Priority,
    ECombatCatchUpPolicy Policy,
    FCombatScheduledDelegate Callback);

FCombatScheduleHandle Reschedule(
    FCombatScheduleHandle Handle,
    double NewDelay,
    double NewInterval);

void Cancel(FCombatScheduleHandle Handle);
```

蓝图不把动态委托直接存入 Scheduler。AbilityTask、ModifierComponent、AttackComponent 等 C++ owner 注册 native delegate，再由 owner 调用 BlueprintNativeEvent。

## 3. 内部模型与稳定顺序

每个 slot 至少保存：

- Handle 和 generation。
- `TWeakObjectPtr<UObject> Owner`。
- `NextFireTime`、`Interval`。
- `Priority`、单调递增 `ApplySequence`。
- Catch-up policy、callback、逻辑 TickIndex。

最小堆节点只保存排序键和 Handle；取消或重排提升 slot generation，使旧堆节点自然失效。`Reschedule` 返回新 generation 的 Handle，保留 Id 和 ApplySequence。Repeating 的 Interval 必须大于 0。

稳定排序键：

```text
NextFireTime ascending -> Priority descending -> ApplySequence ascending
```

周期下一次时间使用 `NextFireTime += Interval`，不能使用 `Now + Interval`。默认读取 `UWorld` game time，所以暂停和 global time dilation 同时作用于战斗逻辑；UI real-time 任务不进入本 Scheduler。

## 4. 每帧执行

```text
Now = World Game Time
while Heap.Top.NextFireTime <= Now and budgets remain:
  Pop node
  Validate owner and handle generation
  Calculate DueCount and catch-up behavior
  Dispatch callback with TickContext
  Reinsert repeating slot only when owner/generation still valid
Flush deferred schedule/add/remove/reschedule operations
```

规则：

- 回调中 Cancel 立即提升 generation，使同 Handle 后续 callback 失效。
- Schedule、Reschedule 和堆结构变化在当前 callback 返回后提交。
- 回调中新建且已经到期的任务最早下一轮执行，禁止同步重入。
- Owner 销毁、World teardown 和 PIE EndPlay 自动取消关联任务。
- 设置全局 `MaxCallbacksPerFrame`、单 Owner budget 和 `MaxCatchUpCallbacksPerTask`。
- 达到预算时保留原 ScheduledTime 到下一帧，不改写为 `Now + Interval`。

## 5. Catch-up 规则

```text
DueCount = floor((Now - NextFireTime) / Interval) + 1
```

`TickIndex` 是本次 callback 覆盖的第一个逻辑 tick，`TickCount` 是覆盖数量。每次回调后按 `NextFireTime += Interval * TickCount` 推进。

| Policy | 行为 | 默认用途 |
| --- | --- | --- |
| ExecuteAllBounded | 每次 TickCount=1，本帧最多派发单任务上限；剩余 tick 留到后续帧 | DOT/HOT、Channel pulse、每 tick 独立 Hook |
| Coalesce | 只回调一次，TickCount 表示跨过的周期数 | attack-ready、Order 追击、Aura/Thinker 查询 |
| SkipExpired | 丢弃旧周期，最新周期至多回调一次且 TickCount=1 | 非权威提示、调试表现；禁止 Damage/Heal |

DOT/HOT/Channel 读取 TickContext，不读取当前帧 DeltaSeconds。Coalesce 的调用者必须明确是“只关心最新状态”还是按 TickCount 补偿；不能隐式猜测。

## 6. Duration、Refresh 与边界 tick

ModifierData 明确配置：

- `bTickOnExpire`：恰好位于 ExpireAt 的 tick 是否执行。
- Refresh 策略：`PreservePhase` 或 `ResetInterval`。
- Stack 增加默认不创建第二个 schedule。

ModifierComponent 保存绝对 `ExpireAt` 和最后结算 TickIndex。自然过期若先于 Scheduler 回调到达：

1. 结算所有早于 ExpireAt 的遗漏 tick。
2. 根据 `bTickOnExpire` 处理边界 tick。
3. 销毁 Runtime。

Purge、死亡移除和手动 Cancel 不 catch up。过期与 tick 的先后不能依赖 Timer 注册顺序。

## 7. 各系统接入

| 系统 | Scheduler 用法 |
| --- | --- |
| Channel | `UAbilityTask_WaitCombatInterval` 持有 repeating Handle；Ability End/Cancel 取消 |
| Modifier | Component 代表 Runtime 注册；Refresh/Destroyed 统一重排或取消 |
| DOT/HOT | Runtime Think 调 Damage/Heal Subsystem；Periodic GE 不直接改 Health |
| Attack | attack point、attack-ready 使用 ScheduleOnce；卡顿不补发攻击 |
| Order | 追击/距离复查用 Coalesce，目标位移阈值可提前唤醒 |
| Thinker | pulse 用 repeating，短延迟爆炸用 ScheduleOnce，Actor 默认无 Tick |
| Projectile | 不进入逻辑 Scheduler；服务器逐帧 sweep 后幂等 Finish |
| Motion | 不进入逻辑 Scheduler；由 Movement/RootMotionSource 更新 |

## 8. Combat 事务身份

所有可能嵌套的战斗操作分配唯一 `FCombatEventId`，同时保存：

- `EventId`：当前事务。
- `RootEventId`：事件链根。
- `Depth`：嵌套深度。
- 可选 `AttackHandle` 和统一来源身份。

反伤、吸血等 follow-up 创建新 EventId，继承 RootEventId。Subsystem 设置最大深度，并在创建 follow-up 前添加 `Reflection`、`NoLifesteal` 等防递归 flags。

统一来源结构允许同时保留因果链中的 Ability、Modifier 和 Projectile。例如 Ability 生成 Projectile，Projectile 施加 DOT Modifier 时，三种 DefinitionId 都可以存在；`DirectSourceType` 指明本次事务的直接来源，日志仍能回溯完整链。

```cpp
UENUM(BlueprintType)
enum class ECombatDirectSourceType : uint8
{
    Unit,
    Ability,
    Modifier,
    Projectile,
    Attack
};

USTRUCT(BlueprintType)
struct FCombatSourceContext
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    ECombatDirectSourceType DirectSourceType = ECombatDirectSourceType::Unit;

    UPROPERTY(BlueprintReadOnly)
    FPrimaryAssetId AbilityDefinitionId;

    UPROPERTY(BlueprintReadOnly)
    FPrimaryAssetId ModifierDefinitionId;

    UPROPERTY(BlueprintReadOnly)
    FPrimaryAssetId ProjectileDefinitionId;
};

USTRUCT()
struct FCombatGameplayEffectContext : public FGameplayEffectContext
{
    GENERATED_BODY()

    FCombatEventId EventId;
    FCombatEventId RootEventId;
    FCombatAttackHandle AttackHandle;
    FCombatSourceContext Source;

    virtual FGameplayEffectContext* Duplicate() const override;
    virtual UScriptStruct* GetScriptStruct() const override;
    virtual bool NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess) override;
};

template<>
struct TStructOpsTypeTraits<FCombatGameplayEffectContext>
    : TStructOpsTypeTraitsBase2<FCombatGameplayEffectContext>
{
    enum
    {
        WithCopy = true,
        WithNetSerializer = true
    };
};
```

项目使用 `UCombatAbilitySystemGlobals::AllocGameplayEffectContext` 分配该 Context，并通过 UE 5.8 Gameplay Abilities Developer Settings 或项目 config 把 `AbilitySystemGlobalsClassName` 指向该类。配置变更需要重启 Editor；G1 必须断言实际分配对象的 `GetScriptStruct()` 是 `FCombatGameplayEffectContext::StaticStruct()`。

进入网络的 EventId、RootEventId、Source 字段必须显式写入 `NetSerialize`；只服务服务器 exactly-once 的 AttackHandle 可以不复制。`TStructOpsTypeTraits` 的 `WithCopy`/`WithNetSerializer` 是结构复制和自定义网络序列化的一部分，不能只实现虚函数而省略 traits。客户端用 EventId 和 Source DefinitionId 关联日志/表现。

Context 不复制 DataAsset UObject 指针。DefinitionId 由客户端通过 AssetManager 解析；服务器内部若需要已加载对象，只能在 Subsystem/Component 的本地映射中保存，不能把 SourceObject 当作跨网络身份。

## 9. 同步结果槽

Damage/Heal 在 Apply GE 前按 EventId 创建同步事务槽。AttributeSet 的 `PostGameplayEffectExecute`：

1. 应用 Health clamp。
2. 把实际 delta 和死亡阈值回报到对应槽。
3. 不直接广播另一份 Damage/Death 事件。

Apply GE 返回后，Subsystem 读取槽，完成 Post Hook、follow-up、日志和唯一一次死亡广播，再关闭事务。未知 EventId、重复回报和超深事务必须记录诊断并安全失败。

## 10. Hook 重入与 deferred operation

每个 Hook 阶段开始时获取 Runtime 强引用快照，并按 `Priority -> ApplySequence` 排序。Hook 中的 Apply/Remove/Refresh/Purge 写入当前事务的 deferred queue，阶段结束后按提交顺序执行。

嵌套 Damage/Heal 可以同步创建子事务，但不得修改父阶段正在遍历的容器。每个事务应记录阶段名，便于日志定位“在哪个 Hook 创建了递归事件”。

## 11. 最低验收

- 可变帧率下 repeating task 不漂移，总逻辑次数正确。
- 同时到期任务稳定排序；回调中取消/重排不重复触发。
- Owner 销毁、World teardown、旧 generation 均能失效。
- 三种 catch-up policy 和三层 budget 可自动化验证。
- 纯 Client 不运行权威 callback。
- Modifier Duration/tick 同时到期的边界行为可重复。
- 反伤事件有新 EventId、相同 RootEventId，达到 Depth 上限安全终止。
- AbilitySystemGlobals 实际分配自定义 Context，traits 启用复制/NetSerialize，Source DefinitionId 完成网络 round-trip。
