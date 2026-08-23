# UE GAS Dota-Like Combat System Design

## 1. 背景与目标

本文参考 `invokerw/dota2_skill` 仓库 `src` 目录的实现方式，设计一套适合本 UE 5.8 工程的 Dota-like 战斗系统。参考仓库当前阅读版本为 `085c1c5c2d1658a21f88cf9c01f6142df6830cc5`。

本项目当前是 UE 模板工程，已有两条可复用基础：

- `Source/ue_gas/Variant_Strategy`：多单位选择、AIController、EQS、NavMesh 移动、到达交互。
- `Source/ue_gas/Variant_TwinStick`：简单 Actor 弹体、AoE Actor、NPC 命中销毁、RVO 避让。

目标是在此基础上使用 Gameplay Ability System 实现以下要素：

- Ability：主动、被动、点目标、单位目标、无目标、引导、法球、自动施放、冷却、耗蓝、施法前摇、施法后摇。
- Modifier：Buff/Debuff、属性聚合、状态标记、周期 Think、伤害/治疗/普攻/技能事件 Hook、叠层、驱散、motion controller。
- Projectile：直线弹体、跟踪弹体、普攻弹体、命中回调、穿透/首个命中销毁、投射物表现资源。
- Damage/Heal Pipeline：统一处理技能、普攻、反伤、护盾、魔免、抗性、吸血、治疗增幅。
- Order/Movement：Dota 风格走/打/施法统一指令队列，移动使用 UE NavMesh、AIController、EQS，而不照搬参考仓库的网格寻路。
- Lua 替代：参考仓库的 Lua Ability/Modifier 在 UE 中用 Blueprintable C++ 基类 + 蓝图事件替代。

## 2. 参考仓库 `src` 结构摘要

参考项目是一个数据驱动的 Dota 战斗模拟内核，主要分层如下：

| 目录 | 核心职责 | UE/GAS 映射 |
| --- | --- | --- |
| `core` | `World`、`Unit`、事件、指令队列、普攻、tick 时序 | `UWorld`、Character/Pawn、Combat Subsystem、OrderComponent |
| `ability` | `Ability` 生命周期、行为标志、YAML 数据、Lua 技能 | `UGameplayAbility` 基类、Ability DataAsset、蓝图能力 |
| `modifier` | Buff/Debuff、属性聚合、状态、事件 Hook、motion controller | `GameplayEffect` + `GameplayTag` + 自定义 Modifier Runtime |
| `projectile` | 直线/跟踪投射物、ProjectileManager、命中/结束事件 | `ACombatProjectile`、AbilityTask、GameplayCue |
| `combat` | 统一伤害/治疗管线 | Combat Subsystem、纯数值 Calculator、最终应用 GameplayEffect、DamageSpec/Result |
| `pathfinding` | A* 网格、ShapeCast、WallTracer、动态圆碰撞 | UE NavMesh、AI MoveTo、RVO/Detour Crowd/EQS |
| `script` | sol2 Lua 绑定 | BlueprintNativeEvent/BlueprintImplementableEvent |
| `log/replay` | 战斗事件记录与回放 | Combat Log、GameplayMessage、调试工具 |

参考项目最值得保留的不是具体代码，而是战斗语义：

- 一个单位持有 AbilityManager 和 ModifierManager。
- Ability 有完整施法阶段：Ready、Casting、Backswing、Channelling、OnCooldown。
- Modifier 同时提供属性、状态、事件 Hook。
- 普攻有 AttackRecord，法球通过无副作用的 CanClaimAttack 参与仲裁，胜出后在 OnAttackClaimed 提交资源并写入命中快照。
- 投射物只负责移动和命中事件，真正的伤害/Modifier 仍走统一战斗管线。
- 所有技能数值来自数据层，复杂逻辑由脚本层实现。

## 3. GAS 总体架构

建议新增独立战斗目录：

```text
Source/ue_gas/Combat
  Ability/
  Attribute/
  Damage/
  Modifier/
  Projectile/
  Order/
  Targeting/
  Scheduling/
  Data/
  Log/
  Blueprint/
```

模块依赖需要在 `Source/ue_gas/ue_gas.Build.cs` 增加：

```csharp
"GameplayAbilities",
"GameplayTags",
"GameplayTasks"
```

`ue_gas.uproject` 建议启用 GameplayAbilities 插件。当前工程已有 `AIModule`、`NavigationSystem`、`StateTree`、`GameplayStateTree`、`Niagara`、`UMG`，可直接用于移动、AI、特效和 UI。

### 3.1 运行时核心对象

| 类/组件 | 建议名称 | 职责 |
| --- | --- | --- |
| 战斗单位 | `ACombatUnitCharacter` | 继承 `ACharacter`，实现 `IAbilitySystemInterface`，承载 ASC、属性、队伍、攻击组件 |
| ASC | `UCombatAbilitySystemComponent` | 封装 GAS 激活、标签查询、事件派发、冷却/消耗查询 |
| 属性集 | `UCombatAttributeSet` | 生命、魔法、护甲、魔抗、攻击力、攻速、移动速度等 |
| 指令队列 | `UCombatOrderComponent` | Dota 风格 Move/Attack/Cast/Stop FIFO |
| 普攻组件 | `UCombatAttackComponent` | 攻击冷却、AttackRecord、近战/远程命中 |
| Modifier Runtime | `UCombatModifierComponent` | 补齐 GAS 原生 GE 不擅长的事件 Hook 和蓝图 Modifier 实例 |
| 强制位移 | `UCombatMotionComponent` | 仲裁水平/垂直 motion、RootMotionSource、碰撞和移动恢复 |
| 战斗调度 | `UCombatSchedulerSubsystem` | 服务器逻辑时间、一次性/周期任务、catch-up 和稳定执行顺序 |
| 投射物管理 | `UCombatProjectileSubsystem` 或 Actor 管理 | 创建线性/跟踪投射物，转发命中事件 |
| 战斗事件 | `UCombatEventSubsystem` | 统一广播伤害、治疗、技能、投射物、Modifier、AttackRecord 事件 |

### 3.2 数据层

建议用 DataAsset 替代 YAML，用 Blueprint Class 替代 Lua：

| 数据资产 | 建议名称 | 内容 |
| --- | --- | --- |
| 单位定义 | `UCombatUnitData` | 初始属性、队伍、碰撞半径、普攻参数、默认 Ability 类 |
| 技能定义 | `UCombatAbilityData` | 行为标签、目标阵营、前摇/后摇/引导、冷却、耗蓝、施法距离、special、intrinsic modifier |
| Modifier 定义 | `UCombatModifierData` | GE 类、蓝图 runtime 类、Priority、显示/驱散/死亡移除/状态/属性/ThinkInterval/motion 配置 |
| 投射物定义 | `UCombatProjectileData` | 直线/跟踪、速度、宽度、距离、命中策略、Niagara/Sound |
| 伤害定义 | `FCombatDamageSpec` | 伤害类型、数值、flags、来源 ability、是否触发吸血/反伤 |
| 技能动作 | `FCombatAbilityAction` | Damage、Heal、ApplyModifier、SpawnProjectile、CreateThinker、GameplayEvent |

`ability_special` 建议实现为：

```cpp
USTRUCT(BlueprintType)
struct FCombatSpecialValue
{
    GENERATED_BODY()

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
    TArray<float> Values;

    float GetValueAtLevel(int32 Level) const;
};
```

技能和 Modifier 蓝图都只通过 key 读取当前等级数值，避免逻辑蓝图里硬编码数值。

Ability 类是技能实现入口，并单向引用 `UCombatAbilityData`。`UCombatAbilityData` 不再反向引用 Ability 类；单位和 AbilitySet 直接保存 Ability 类列表。这样可以避免“DataAsset -> Ability Class -> DataAsset”的硬引用环，也保证技能类只有一个数据定义入口。

需要通过网络 View、日志或回放引用的 Combat 定义资产统一继承 `UPrimaryDataAsset`，使用稳定 `FPrimaryAssetId DefinitionId`。运行时和复制数据传 DefinitionId，不复制 DataAsset UObject 指针；客户端通过 AssetManager/DataRegistry 解析本地名称、图标和数值定义。

### 3.3 权威边界与唯一数据源

为了避免 GAS 原生层和自定义 Dota-like Runtime 层各算一套，第一版必须先确定下面几条硬规则：

| 问题 | 约束 |
| --- | --- |
| 属性最终值 | 以 ASC Attribute/Active GameplayEffect 聚合结果为唯一来源。ModifierRuntime 不直接维护第二套 MoveSpeed、Armor、SpellAmp 等最终属性。 |
| 动态属性 | 蓝图 Modifier 如果需要动态数值，必须把结果写成 GameplayEffect Modifier、SetByCaller、Attribute 或刷新已有 GE；不要让战斗查询直接调用蓝图函数实时聚合。 |
| 扣血/回血 | `UCombatDamageSubsystem` / `UCombatHealSubsystem` 是唯一编排入口；Health/Mana 的最终修改只允许通过统一 GameplayEffect 或 AttributeSet Meta Attribute 完成。 |
| 事件 Hook | ModifierRuntime 只负责事件响应、状态机、副作用和需要实例状态的逻辑，例如护盾剩余值、法球认领、motion controller。 |
| 复制 | 服务器拥有权威 Runtime；客户端 UI 和表现通过 Attribute、ActiveGE、GameplayTag、GameplayCue、GameplayMessage/CombatLog 投影，不依赖复制 UObject Runtime 实例。 |
| 伤害载荷 | Damage 数值使用 SetByCaller；伤害类型和 flags 映射为 GameplayEffectSpec DynamicAssetTags；来源 Ability、AttackHandle、EventId 放入自定义 GameplayEffectContext。 |
| 事件结果 | Damage/Heal 入口返回实际结算 Result。Post Hook、吸血、反伤、日志和死亡只读取 Result，不根据请求 Amount 再推算。 |
| 事件顺序 | 同一阶段的 Runtime 按 `Priority -> ApplySequence` 稳定排序；派发期间的添加、移除和刷新延迟到当前阶段结束后执行。 |
| 异步身份 | Attack、Order、Projectile、EQS、AI Move 都使用带 generation 的稳定 Handle；过期回调不得改变当前状态。 |
| 周期逻辑 | Channel、Modifier Think、DOT/HOT、attack-ready、Order 追击和 Thinker pulse 只通过服务器 Combat Scheduler 调度；禁止各 Runtime 自建 Tick/Timer。 |
| 指令队列 | Order 只等待 `OrderReleased`、`AbilityChannelEnded` 或失败/中断事件，不等待 cooldown 或纯表现 backswing。 |
| 服务器入口 | Damage、Heal、ApplyModifier、Attack Finalize 和 Order 执行只允许服务器调用；客户端只能提交经过归属、目标和频率校验的 Order 请求。 |

这几条约束比具体类名更重要。后续实现如果需要打破其中一条，必须先把替代路径写清楚，否则很容易出现双重扣血、客户端状态缺失、属性显示和服务器判定不一致等问题。

### 3.4 运行时事务与上下文

所有可能产生嵌套事件的战斗操作都分配唯一 `FCombatEventId`，并保留 `RootEventId` 和 `Depth`。例如反伤会创建新的 EventId，但沿用原始 RootEventId。Subsystem 必须限制最大嵌套深度，并用 flags 阻止反伤、吸血等不允许递归的效果。

Damage GameplayEffect 使用自定义上下文：

```cpp
USTRUCT()
struct FCombatGameplayEffectContext : public FGameplayEffectContext
{
    GENERATED_BODY()

    FCombatEventId EventId;
    FCombatEventId RootEventId;
    FCombatAttackHandle AttackHandle;
    FPrimaryAssetId AbilityDefinitionId;

    virtual FGameplayEffectContext* Duplicate() const override;
    virtual UScriptStruct* GetScriptStruct() const override;
    virtual bool NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess) override;
};

template<>
struct TStructOpsTypeTraits<FCombatGameplayEffectContext>
    : TStructOpsTypeTraitsBase2<FCombatGameplayEffectContext>
{
    enum { WithCopy = true, WithNetSerializer = true };
};
```

实现时还需要通过项目的 `UAbilitySystemGlobals` 子类分配该 Context。DefinitionId、EventId 等可复制字段必须显式进入 `NetSerialize`；AttackHandle 若只服务于服务器 exactly-once 结算则不复制，客户端日志使用 EventId/DefinitionId。

`FCombatDamageSpec` 到 GameplayEffectSpec 的映射固定如下：

| Combat 数据 | GAS 载体 |
| --- | --- |
| Final Amount | SetByCaller `Data.Damage.Final`；原始 Amount 只存在服务器事务中 |
| DamageType | 唯一一个 DynamicAssetTag：`Damage.Type.Physical/Magical/Pure` |
| Flags | DynamicAssetTags：`Damage.Flag.*` |
| Attacker/Victim | EffectContext Instigator/EffectCauser 与目标 ASC |
| Ability、Attack、事件链 | `FCombatGameplayEffectContext` |

`UCombatDamageSubsystem::DealDamage` 在调用 Apply GE 前创建一个同步事务槽。`UCombatAttributeSet::PostGameplayEffectExecute` 根据 Context EventId 回报实际 Health delta、clamp、死亡阈值等结果。Apply GE 返回后，Subsystem 取回结果并完成 Post Hook、吸血、反伤、日志和唯一一次死亡广播。AttributeSet 不直接广播第二份 Damage/Death 事件。

### 3.5 Combat Scheduler 与 Tick Policy

战斗时序分成三类，不能让每个 Ability、Modifier 和 Thinker 各自开启 UObject/Actor Tick：

| 类型 | 更新方式 | 适用对象 |
| --- | --- | --- |
| 事件驱动 | 状态变化时同步执行，不 Tick | Damage/Heal、GE/Tag 变化、Projectile Hit、Order 完成 |
| 逻辑定时 | `UCombatSchedulerSubsystem` 按绝对服务器时间调度 | Channel、Modifier Think、DOT/HOT、attack-ready、Order 追击、Thinker pulse |
| 连续运动 | 每帧 DeltaSeconds + sweep/substep | Projectile、CharacterMovement、RootMotionSource/Motion |

Scheduler 第一版实现为 `UTickableWorldSubsystem`，每个 World 只保留一个统一 Tick。它在 Standalone、Listen Server 和 Dedicated Server 执行权威 gameplay callback；纯 Client 只做表现插值，不运行 Damage、Heal、Modifier Think 或 attack-ready。Scheduler 的确定性范围是同一个 World、同一次调度批次；注册发生在本轮 Scheduler Tick 之后的零延迟任务统一到下一轮执行，禁止同步重入。

公共类型和接口：

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

    UPROPERTY(BlueprintReadOnly)
    double ScheduledTime = 0.0;

    UPROPERTY(BlueprintReadOnly)
    double ActualTime = 0.0;

    UPROPERTY(BlueprintReadOnly)
    float Interval = 0.0f;

    UPROPERTY(BlueprintReadOnly)
    int32 TickIndex = 0;

    UPROPERTY(BlueprintReadOnly)
    int32 TickCount = 1;
};

DECLARE_DELEGATE_OneParam(
    FCombatScheduledDelegate,
    const FCombatScheduledTickContext&);

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

蓝图不直接把动态委托存进 Scheduler。`AbilityTask`、`ModifierComponent`、`AttackComponent` 等 C++ owner 注册 native delegate，再由 owner 调用 BlueprintNativeEvent。内部任务至少保存 `Handle`、`TWeakObjectPtr<UObject> Owner`、`NextFireTime`、`Interval`、`Priority`、`ApplySequence`、`Policy` 和 callback。Handle 指向 slot，heap node 只保存排序键与 Handle；Cancel/Reschedule 提升 slot generation，使 heap 中尚未弹出的旧节点自然失效。Reschedule 返回更新后的 Handle，并保留原 Id 与 ApplySequence。Repeating 的 Interval 必须大于零，非法输入直接拒绝并返回无效 Handle。

内部使用最小堆，稳定排序键固定为：

```text
NextFireTime ascending -> Priority descending -> ApplySequence ascending
```

周期任务下一次时间使用 `NextFireTime += Interval`，不能使用 `Now + Interval`，否则帧率波动会让周期持续漂移。默认使用 `UWorld` game time，因此暂停和 global time dilation 会同时暂停/缩放战斗逻辑；需要 real time 的纯 UI 任务不能进入 Combat Scheduler。

每帧执行流程：

```text
Now = World Game Time
while Heap.Top.NextFireTime <= Now and CallbackBudget remains:
  Pop item
  Validate Owner weak pointer and Handle generation
  Calculate due TickCount according to CatchUpPolicy
  Dispatch callback with FCombatScheduledTickContext
  Reinsert repeating item only if Owner and generation are still valid
Flush deferred schedule/add/remove operations
```

回调期间 Cancel 立即提升 generation，使同一 Handle 的后续 callback 失效；Schedule、reschedule 和 heap 结构修改在当前 callback 返回后统一提交。回调中新建且已经到期的任务也不能在本轮继续执行。Owner 销毁、World teardown、PIE EndPlay 时自动取消关联任务。

对于 repeating task，先计算 `DueCount = floor((Now - NextFireTime) / Interval) + 1`。`TickIndex` 表示本次 callback 覆盖的第一个逻辑 tick 序号，`TickCount` 表示本次覆盖的逻辑 tick 数；每次 callback 后都按 `NextFireTime += Interval * TickCount` 推进。具体规则如下：

Catch-up 规则固定如下：

| Policy | 行为 | 默认用途 |
| --- | --- | --- |
| ExecuteAllBounded | 本帧最多派发 `MaxCatchUpCallbacksPerTask` 次，每次 TickCount=1；剩余 tick 保持 overdue，进入下一帧的 deferred-due 队列，不能在本帧再次占用预算 | DOT/HOT、Channel pulse、每次 tick 都有独立 Hook 的逻辑 |
| Coalesce | 无论遗漏几次只回调一次，TickCount 表示跨过的周期数 | attack-ready、Order 追击、Aura/Thinker 查询 |
| SkipExpired | 丢弃 `DueCount - 1` 个历史周期，最新周期最多回调一次且 TickCount=1，不补偿历史总量 | 非权威提示、调试表现；不能用于 Damage/Heal |

Scheduler 还需要全局 `MaxCallbacksPerFrame` 和单 Owner budget，防止服务器卡顿后在一帧内执行无限补偿。达到预算时保留原 ScheduledTime 到下一帧继续处理，不能改成 `Now + Interval`。

周期任务与 Duration 同时到期时，先按 ModifierData 的显式 `bTickOnExpire` 决定是否执行末次 tick，再处理过期；不能依赖 Timer 注册顺序。ModifierComponent 保存绝对 `ExpireAt` 和最后结算的 TickIndex：自然过期回调若先于 Scheduler 到达，Component 先结算所有早于 `ExpireAt` 的遗漏 tick，再根据 `bTickOnExpire` 决定是否结算恰好落在边界上的 tick，最后销毁 Runtime；Purge、死亡移除和手动 Cancel 不做 catch-up。Refresh 还要配置 `PreservePhase` 或 `ResetInterval`，Stack 增加默认不创建第二个 schedule。

各系统接入规则：

| 系统 | Scheduler 用法 |
| --- | --- |
| Channel | `UAbilityTask_WaitCombatInterval` 持有 repeating handle；Ability End/Cancel 时取消 |
| Modifier | Component 代表 Runtime 注册 handle；OnRefresh/OnDestroyed 统一 reschedule/cancel |
| DOT/HOT | `OnIntervalThink` 调用统一 Damage/Heal Subsystem；Periodic GE 不直接修改 Health |
| Attack | attack point 和 attack-ready 使用 ScheduleOnce；卡顿后只进入一次 Ready，不补发多次攻击 |
| Order | 追击/施法距离复查使用 Coalesce，也可由目标位移阈值提前唤醒 |
| Thinker | pulse 使用 repeating handle；短延迟爆炸使用 ScheduleOnce，Actor 本身关闭 Tick |
| Projectile | 不进入逻辑 Scheduler；服务器逐帧 sweep，命中后进入幂等 FinishProjectile |
| Motion | 不进入逻辑 Scheduler；由 CharacterMovement/RootMotionSource 的帧更新处理 |

只有连续运动使用帧 `DeltaSeconds`。周期伤害和 Channel 不读取本帧 DeltaSeconds，而使用 TickContext 的 Interval/TickCount，保证不同服务器帧率下总次数与总量一致。网络层不复制每次 scheduler tick，只复制最终 Attribute/Tag/GE/CombatResult 和必要的开始、结束服务器时间。

## 4. Ability 设计

### 4.1 行为标志

参考项目的 `BehaviorFlag` 直接使用 GameplayTag 表达，不再同时维护一份可漂移的枚举/位掩码。编辑器若需要复选框，可提供只写 GameplayTagContainer 的定制面板：

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

目标阵营：

```text
TargetTeam.None
TargetTeam.Enemy
TargetTeam.Friendly
TargetTeam.Both
```

### 4.2 基类

建议实现：

```cpp
UCLASS(Abstract, Blueprintable)
class UCombatGameplayAbility : public UGameplayAbility
{
    GENERATED_BODY()

public:
    UCombatGameplayAbility();

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

基类默认使用 `InstancedPerActor`，并禁止同一个 Ability 在未结束时重入；这样蓝图事件、目标快照、delegate 和一次性提交标记都保存在单位自己的实例上。确实需要同一单位并发执行多份实例的技能，必须显式改为 `InstancedPerExecution`，并重新评估复制策略。`NonInstanced` 只用于完全无状态、无蓝图、无 latent task 的高频能力。

目标通过 `FGameplayAbilityTargetDataHandle` 进入 Ability。服务器在 cast point 完成时重新校验目标、阵营、距离和状态；不得把客户端传入的 Actor 指针或位置直接当成权威结果。

AbilityData 明确保存以下生命周期策略，禁止蓝图临时决定：

| 配置 | 默认值 | 说明 |
| --- | --- | --- |
| `CostCommitPoint` | SpellStarted | CastStarted、SpellStarted；默认 cast point 内被打断不扣资源 |
| `CooldownCommitPoint` | SpellStarted | CastStarted、SpellStarted、AbilityEnded |
| `ChannelInterruptOrderPolicy` | Continue | Continue 或 ClearQueuedOrders |
| `bCancelProjectilesWithAbility` | false | 默认 fire-and-forget Projectile 脱离 Ability 生命周期 |

### 4.3 施法阶段

参考项目的 Ability 流程应在 GAS 中显式保留：

1. `CanActivateAbility`
   - 检查死亡、眩晕、妖术、沉默、冷却、魔法、目标阵营、魔免、不可选中、距离。
2. `ActivateAbility`
   - 在当前 Ability 实例缓存 TargetData 和激活上下文。
   - 根据配置提交 cast-start 阶段的 cost/cooldown；每项只能提交一次。
   - 广播 `AbilityCastStarted`。
   - 播放 cast point montage 或启动 `UAbilityTask_WaitDelay`。
3. Cast Point 完成
   - 服务器再次验证目标、阵营、距离、状态和资源。
   - 先同时通过 `CheckCost` 和 cooldown 检查，再根据配置分别调用 `CommitAbilityCost` / `CommitAbilityCooldown`；失败则中断，不能只提交其中一半。
   - 调用 `ReceiveSpellStart` 或执行 DataDriven Actions，并广播 `AbilitySpellStarted`。
   - 向 owner ModifierRuntime 派发语义事件 `OnAbilityExecuted`。
4. Channel
   - 用 `UAbilityTask_WaitCombatInterval` 向 Combat Scheduler 注册 repeating handle，调用 `ReceiveChannelTick`。
   - 结算读取 TickContext.Interval/TickCount，不读取帧 DeltaSeconds；Ability End/Cancel 时取消 handle。
   - 被眩晕/沉默/死亡/目标丢失时中断。
   - 结束时广播一次 `AbilityChannelEnded(bInterrupted)`。
5. Backswing
   - Dota 语义里后摇不应阻止新施法；UE 可只作为动画/表现，不用锁住 ASC。
   - 不建议让 OrderComponent 等待 backswing 结束才继续下一条指令。
6. Cooldown
   - 使用 Cooldown GameplayEffect。
   - 默认在 `AbilitySpellStarted` 提交；cast point 内被打断不耗蓝、不进冷却，引导开始后被打断保留已提交结果。
   - 特殊技能可配置 `CastStarted` 或 `AbilityEnded` 提交点，但基类用 `bCostCommitted` / `bCooldownCommitted` 保证幂等。
7. Order 释放
   - 非引导技能在 `AbilitySpellStarted` 后广播 `OrderReleased`。
   - 引导技能在 `AbilityChannelEnded` 后广播 `OrderReleased`。
8. 完整结束
   - 所有路径最终调用 `EndAbility`；在 override 中统一解绑 delegate、结束 task、清理上下文并广播 `AbilityEnded(bWasCancelled)`。

建议 Ability 基类明确广播以下事件，避免动画、结算、队列状态混在一起：

| 事件 | 触发时机 | 用途 |
| --- | --- | --- |
| `AbilityCastStarted` | 通过校验并进入 cast point | UI、动画、打断窗口、日志 |
| `AbilitySpellStarted` | cast point 完成并开始技能实际效果 | Modifier `OnAbilityExecuted`、触发被动、日志 |
| `AbilityChannelEnded` | 引导正常结束或中断 | 引导清理、持续效果收尾 |
| `OrderReleased` | 非引导技能生效后，或引导结束后 | Order 队列继续 |
| `AbilityInterrupted` | 生效前被打断，或引导被中断 | Order 失败策略、UI、日志 |
| `AbilityEnded` | Ability 的 task 和 delegate 全部清理完成 | 生命周期断言、表现回收 |

所有事件每次激活最多广播一次。`OnAbilityExecuted` 的统一语义是“技能已经生效”，因此引导技能也在引导开始时触发，而不是延迟到引导结束。Order 是否继续只看 `OrderReleased`，不复用 `OnAbilityExecuted`。

引导被中断时的固定事件顺序是 `AbilityInterrupted -> AbilityChannelEnded(true) -> OrderReleased/清队列 -> AbilityEnded(true)`；cast point 内被中断时没有 ChannelEnded，只走 `AbilityInterrupted -> Order 失败策略 -> AbilityEnded(true)`。

### 4.4 DataDriven Ability

参考项目 `DataDrivenAbility` 只支持 Damage、Heal、ApplyModifier。UE 第一阶段也建议保持克制：

```text
Action.Damage
Action.Heal
Action.ApplyModifier
Action.SpawnLinearProjectile
Action.SpawnTrackingProjectile
Action.CreateThinker
Action.SendGameplayEvent
```

复杂技能使用蓝图子类：

- `BP_Ability_LinaDragonSlave`
- `BP_Ability_PudgeMeatHook`
- `BP_Ability_EarthshakerFissure`
- `BP_Ability_JuggernautOmnislash`

## 5. Modifier 设计

GAS 的 GameplayEffect 很适合表达持续时间、叠层、属性修改、GameplayTag 状态，但不适合直接表达 Dota Modifier 的大量事件 Hook。因此建议采用双层结构：

核心原则：数值属性统一落到 GAS，事件语义统一落到 ModifierRuntime。不要让 GE 和 Runtime 同时提供同一个最终属性值。

### 5.1 GameplayEffect 层

用于：

- Duration/Infinite/Instant。
- Stack policy。
- 属性修改：Armor、MoveSpeed、AttackDamage、MagicResist 等。
- Granted Tags：`State.Stunned`、`State.Silenced`、`State.Rooted`、`State.Hexed`、`State.MagicImmune`。
- 非 Health/Mana 的简单周期属性效果；需要进入战斗事件链的 DOT/HOT 不在 GE 内直接结算。
- GameplayCue：显示特效、音效、UI。

### 5.2 Modifier Runtime 层

用于参考项目中的 Hook：

```text
OnCreated
OnDestroyed
OnRefresh
OnStackChanged
OnIntervalThink
OnPreDealDamage
OnPreTakeDamage
OnDamageBlock
OnPostDealDamage
OnPostTakeDamage
OnPreTakeHeal
OnPostTakeHeal
OnAbilityExecuted
CanClaimAttack
OnAttackClaimed
OnAttack
OnAttackLanded
OnAttackFail
OnAttackRecordDestroy
GetAttackProjectileName
OnMotionTick
```

建议基类：

```cpp
UCLASS(Abstract, Blueprintable, EditInlineNew)
class UCombatModifierRuntime : public UObject
{
    GENERATED_BODY()

public:
    UPROPERTY(BlueprintReadOnly)
    FActiveGameplayEffectHandle ActiveGEHandle;

    UPROPERTY(BlueprintReadOnly)
    TObjectPtr<UCombatModifierData> ModifierData;

    UFUNCTION(BlueprintNativeEvent)
    void OnCreated(const FCombatModifierContext& Context);

    UFUNCTION(BlueprintNativeEvent)
    void OnIntervalThink(const FCombatScheduledTickContext& TickContext);

    UFUNCTION(BlueprintNativeEvent)
    void OnPreTakeDamage(UPARAM(ref) FCombatDamageEvent& Event);

    UFUNCTION(BlueprintNativeEvent)
    void OnDamageBlock(UPARAM(ref) FCombatDamageEvent& Event);

    UFUNCTION(BlueprintNativeEvent)
    bool CanClaimAttack(const FCombatAttackRecord& Record) const;

    UFUNCTION(BlueprintNativeEvent)
    void OnAttackClaimed(UPARAM(ref) FCombatAttackRecord& Record);

    UFUNCTION(BlueprintNativeEvent)
    void OnAttack(UPARAM(ref) FCombatAttackRecord& Record);

    UFUNCTION(BlueprintNativeEvent)
    void OnAttackLanded(const FCombatAttackRecord& Record);
};
```

`UCombatModifierComponent` 负责：

- 提供服务器唯一入口 `ApplyModifier(ModifierData, Context)`，并把 ModifierData 写入 GameplayEffectContext SourceObject。
- 在 GE 添加时从 EffectContext 解析 ModifierData 并创建 Runtime 实例；没有合法 ModifierData 的 GE 不创建 Runtime。
- 在 GE 移除/过期时销毁 Runtime。
- 维护 `FActiveGameplayEffectHandle -> Runtime` 映射。
- 派发伤害、治疗、普攻、技能事件。
- 处理驱散、刷新、叠层，并代表 Runtime 向 Combat Scheduler 注册/取消 ThinkInterval handle。

映射中的 Runtime 必须由 `UPROPERTY` 持有，防止 GC。一个 ActiveGE handle 对应一个 Runtime；GAS 在同一 handle 上增加 stack 时只调用该 Runtime 的 `OnStackChanged` / `OnRefresh`，不能为每层 stack 创建第二个实例。如果某种 Modifier 需要每次施加各自保存状态，它必须配置为独立 ActiveGE，而不是聚合 stack。

ThinkInterval 为 0 时不注册任务；大于 0 时由 Component 持有 `FCombatScheduleHandle`。OnRefresh 根据 ModifierData 的 PreservePhase/ResetInterval 策略重排，OnDestroyed 和 Component EndPlay 必须取消。DOT/HOT 的 OnIntervalThink 只能调用统一 Damage/Heal Subsystem，不能直接修改 Attribute。

Runtime 不负责：

- 直接覆盖 `MoveSpeed()`、`Armor()`、`AttackDamage()` 这类最终属性查询。
- 在客户端作为权威数据源。
- 绕过 ASC 直接修改 Health/Mana。

动态属性推荐两种实现方式：

1. 创建/刷新 GE 时，把蓝图计算出的数值写入 SetByCaller，然后让 GE Modifier 修改 Attribute。
2. 对需要经常变化的数值，Runtime 通过 Scheduler 按固定频率或由事件触发刷新一个专用 GE，而不是每次属性查询时调用蓝图函数。

### 5.3 Hook 顺序与重入

每个 ModifierData 提供整数 `Priority`。Component 为 Runtime 分配单调递增的 `ApplySequence`，每个 Hook 阶段都按下面的稳定顺序派发：

```text
Priority descending -> ApplySequence ascending
```

派发开始时先取得强引用快照。Hook 内产生的 Apply、Remove、Refresh、Purge 进入 deferred operation queue，在当前阶段完成后按提交顺序执行。这样 Shield 在 `OnDamageBlock` 内耗尽并移除自身时，不会修改正在遍历的容器或重入 `OnDestroyed`。

嵌套 Damage/Heal 使用新的 EventId 并继承 RootEventId。Subsystem 对 Depth 设置硬上限；`Reflection`、`NoLifesteal` 等 flags 在进入新事务前统一添加，不能依赖各蓝图自行阻止递归。

### 5.4 状态标签

建议状态用 GameplayTag：

```text
State.Stunned
State.Silenced
State.Rooted
State.Disarmed
State.Hexed
State.Invisible
State.Invulnerable
State.OutOfGame
State.MagicImmune
State.Untargetable
State.NoUnitCollision
State.NoHealthBar
State.Frozen
```

这些标签用于：

- Ability 激活阻止：Stunned、Hexed、Silenced。
- 移动阻止：Stunned、Rooted、Frozen。
- 攻击阻止：Stunned、Disarmed、Hexed。
- 目标过滤：Untargetable、OutOfGame、Invulnerable、MagicImmune。
- 碰撞/显示：NoUnitCollision、NoHealthBar。

Character 上的状态响应器订阅 GameplayTag 的 count change，并根据“当前聚合 count 是否大于 0”更新移动、碰撞和 UI。不能在某个 GE 移除时直接恢复默认值，否则两个 Modifier 同时授予 NoUnitCollision/Stunned 时，先移除的一方会错误解除仍然有效的状态。

### 5.5 Motion Controller

ModifierRuntime 不直接 Tick `SetActorLocation`。所有强制位移统一提交给 `UCombatMotionComponent`：

```text
TryAcquireMotion(Request) -> FCombatMotionHandle
UpdateMotion(Handle, DeltaTime)
ReleaseMotion(Handle, FinishReason)
```

MotionComponent 分别维护 Horizontal 和 Vertical 通道，并定义 Priority、是否可被抢占、碰撞策略和结束位置校正。同一通道只能有一个 owner；高优先级请求可中断低优先级请求，被中断的 Modifier 收到 `OnMotionInterrupted`。

位移开始时由 MotionComponent 暂停 AI MoveTo，并通过 RootMotionSource 或 CharacterMovement 的受控接口移动，禁止多个 Runtime 同时写 Actor Transform。结束后先做 NavMesh 可达位置校正，再通知 OrderComponent 用当前队首重新判定目标和距离。服务器执行权威位移，客户端只接收 CharacterMovement/RootMotionSource 的正常校正与表现。

`OnMotionTick` 属于连续运动帧更新，接收 CharacterMovement 的 DeltaSeconds；它不走 Combat Scheduler，也不能在其中直接结算周期 Damage/Heal。

### 5.6 驱散

ModifierData 使用明确的 `DispelRule`：`NotDispellable`、`Basic`、`StrongOnly`，并保存 `bIsDebuff`、`bRemoveOnDeath`。驱散请求还必须声明目标方向：移除 Buff、移除 Debuff 或两者；不能仅凭 GrantedTag 猜测正负面效果。

Basic dispel 只移除 `DispelRule.Basic`，Strong dispel 同时移除 Basic 和 StrongOnly。Component 在请求开始时取得候选快照，按 Priority/ApplySequence 排序，将对应 ActiveGE handle 加入 deferred removal queue；默认一次驱散移除整个 ActiveGE 及其全部 stack，需要逐层移除的 Modifier 必须显式配置。`NotDispellable` 只能由自身生命周期、死亡规则或显式管理接口移除。

## 6. 属性与聚合

参考项目的 `ModifierProperty` 建议转成 GAS Attribute：

| 属性 | GAS Attribute |
| --- | --- |
| MaxHealth / Health | `MaxHealth` / `Health` |
| MaxMana / Mana | `MaxMana` / `Mana` |
| Armor | `Armor` |
| MagicResist | `MagicResist` |
| AttackDamage | `AttackDamage` |
| AttackSpeed | `AttackSpeed` |
| BaseAttackTime | `BaseAttackTime` |
| MoveSpeed | `MoveSpeed` |
| AttackRange | `AttackRange` |
| Evasion | `Evasion` |
| LifestealPct | `LifestealPct` |
| HealthRegen / ManaRegen | `HealthRegen` / `ManaRegen` |
| SpellAmplifyPct | `SpellAmplifyPct` |
| StatusResistancePct | `StatusResistancePct` |
| CooldownReductionPct | `CooldownReductionPct` |
| CastRangeBonus | `CastRangeBonus` |

聚合顺序建议：

```text
final = (base + additive) * (1 + additive_pct) * total_multiplier
```

GAS 默认 ModifierOp 可以覆盖大多数持续属性聚合；`additive_pct` 映射 MultiplyAdditive，独立乘区映射 MultiplyCompound。护甲、攻速、闪避上限等 Dota 特殊公式由对应的纯 C++ Calculator 或 AttributeSet clamp 负责，不能同时在 GE 和 Calculator 计算两次。

## 7. Damage/Heal Pipeline

建议所有技能、普攻、反伤、DOT 都走同一条 `FCombatDamageSpec`：

```cpp
UENUM(BlueprintType)
enum class ECombatDamageType : uint8
{
    Physical,
    Magical,
    Pure
};

USTRUCT(BlueprintType)
struct FCombatDamageSpec
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite)
    TObjectPtr<AActor> Attacker;

    UPROPERTY(BlueprintReadWrite)
    TObjectPtr<AActor> Victim;

    UPROPERTY(BlueprintReadWrite)
    ECombatDamageType Type = ECombatDamageType::Physical;

    UPROPERTY(BlueprintReadWrite)
    float Amount = 0.0f;

    UPROPERTY(BlueprintReadWrite)
    FGameplayTagContainer Flags;

    UPROPERTY(BlueprintReadWrite)
    FPrimaryAssetId SourceAbilityDefinitionId;

    UPROPERTY(BlueprintReadWrite)
    FCombatAttackHandle AttackHandle;
};

USTRUCT(BlueprintType)
struct FCombatDamageResult
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    FCombatEventId EventId;

    UPROPERTY(BlueprintReadOnly)
    float RequestedAmount = 0.0f;

    UPROPERTY(BlueprintReadOnly)
    float MitigatedAmount = 0.0f;

    UPROPERTY(BlueprintReadOnly)
    float AbsorbedAmount = 0.0f;

    UPROPERTY(BlueprintReadOnly)
    float AppliedDamage = 0.0f;

    UPROPERTY(BlueprintReadOnly)
    bool bKilled = false;

    UPROPERTY(BlueprintReadOnly)
    FGameplayTag BlockReason;
};
```

Damage flags：

```text
Damage.Flag.BypassMagicImmune
Damage.Flag.HPLoss
Damage.Flag.NoSpellAmplification
Damage.Flag.Reflection
Damage.Flag.NoLifesteal
```

管线顺序：

1. 只在服务器创建事务，校验 Attacker/Victim、Amount、EventDepth 和目标存活/OutOfGame/Invulnerable 状态。
2. 规范化 DamageType/Flags。`HPLoss` 进入独立直扣分支：跳过增伤、抗性、护盾、反伤和吸血，只保留 Health clamp、死亡与日志。
3. 魔法免疫检查。Magical 且没有 `BypassMagicImmune` 时立即返回 blocked Result，尚未执行任何护盾或有状态副作用的 Hook。
4. 攻击者 `OnPreDealDamage` 和输出增伤。
5. 魔法/纯粹伤害的 SpellAmplify。
6. 受害者 `OnPreTakeDamage` 和承受增伤。
7. 类型抗性：
   - 物理：护甲公式。
   - 魔法：`Amount * (1 - MagicResist)`。
   - 纯粹：不减免。
8. `OnDamageBlock`，用于护盾和固定值吸收；记录 AbsorbedAmount。Hook 内移除 Modifier 会延迟到本阶段结束。
9. 用 Instant GE 把最终数值写入 IncomingDamage Meta Attribute；AttributeSet 应用 Health clamp，并按 EventId 回报正数的实际 AppliedDamage 和 bKilled。
10. 广播一次 DamageApplied；按稳定顺序调用目标 `OnPostTakeDamage(Result)` 和来源 `OnPostDealDamage(Result)`。
11. 将反伤、吸血等 follow-up 事务加入队列。它们使用新的 EventId、继承 RootEventId，并带上 `Reflection` / `NoLifesteal` 等防递归 flags。

物理护甲公式可沿用参考项目：

```text
reduction = (0.06 * armor) / (1 + 0.06 * abs(armor))
armor >= 0: multiplier = 1 - reduction
armor < 0:  multiplier = 2 - pow(0.94, -armor)
```

Heal Pipeline：

1. 服务器校验来源、目标、Amount 和 EventDepth。
2. `OnPreTakeHeal`，然后计算来源 HealAmp 与目标 HealReceivedAmp。
3. 用统一 Instant GE 写入 IncomingHealing Meta Attribute。
4. AttributeSet clamp 到 MaxHealth，并按 EventId 回报正数的实际 AppliedHealing。
5. 广播一次 HealApplied，并把 `FCombatHealResult` 传给 `OnPostTakeHeal`。

`FCombatHealResult` 与 DamageResult 一样区分 RequestedAmount 和实际 AppliedHealing；满血目标的 overheal 不能被后续 Hook 当成真实治疗量。Heal 事务同样遵守稳定 Hook 顺序、deferred mutation 和递归深度限制。

GAS 实现建议：

- `UCombatDamageSubsystem::DealDamage` 是唯一公开入口，负责权限检查、事务上下文、Hook、纯数值计算、调用 GE、Post Hook 和广播事件，并返回 `FCombatDamageResult`。
- `FCombatDamageCalculator` 是无状态 C++ 计算器，读取 ASC 当前聚合属性，完成 SpellAmp、incoming amp、护甲和魔抗公式。它不修改 Attribute、不广播事件，也不调用蓝图。
- `GE_CombatDamageApply` 只把 SetByCaller `Data.Damage.Final` 写入 IncomingDamage Meta Attribute，不再重复计算增伤或抗性。
- `UCombatAttributeSet` 只负责应用 Meta Attribute 到 Health、clamp，并通过 EventId 回报实际 delta 和死亡阈值，不直接广播 Damage/Death。
- ModifierRuntime 的 Damage/Heal Pre、Block、Post Hook 必须在对应 Subsystem 的同一条服务器权威事务内执行。

禁止路径：

- 技能蓝图直接调用 `SetHealth`。
- Projectile 直接修改目标生命。
- GE/AttributeSet 内部再次计算抗性或派发 `OnPostTakeDamage`。
- 反伤在没有 `Damage.Flag.Reflection` 的情况下递归触发自身。

推荐调用栈：

```text
Ability/Attack/Projectile
  -> UCombatDamageSubsystem::DealDamage(Spec)
    -> Validate authority / target / event depth / immunity
    -> Source OnPreDealDamage / Target OnPreTakeDamage
    -> FCombatDamageCalculator computes amplification and resistance
    -> Target OnDamageBlock
    -> Apply GE_CombatDamageApply with final SetByCaller
      -> AttributeSet applies Health delta and reports transaction result
    -> Broadcast one DamageApplied / Death if needed
    -> Target OnPostTakeDamage / Source OnPostDealDamage
    -> Queue Lifesteal / Reflection follow-up transactions with flags
```

## 8. 普攻与法球

参考项目 `AttackRecord` 是实现 Dota 法球的关键，UE 中建议保留：

```cpp
USTRUCT(BlueprintType)
struct FCombatAttackHandle
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    TWeakObjectPtr<AActor> Attacker;

    UPROPERTY(BlueprintReadOnly)
    int64 Sequence = 0;

    bool IsValid() const;
};

USTRUCT(BlueprintType)
struct FCombatAttackRecord
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    FCombatAttackHandle Handle;

    UPROPERTY(BlueprintReadOnly)
    TWeakObjectPtr<AActor> Attacker;

    UPROPERTY(BlueprintReadOnly)
    TWeakObjectPtr<AActor> Target;

    UPROPERTY(BlueprintReadWrite)
    float BaseDamage = 0.0f;

    UPROPERTY(BlueprintReadWrite)
    float BonusDamage = 0.0f;

    UPROPERTY(BlueprintReadWrite)
    ECombatDamageType DamageType = ECombatDamageType::Physical;

    UPROPERTY(BlueprintReadOnly)
    ECombatAttackState State = ECombatAttackState::Pending;

    UPROPERTY(BlueprintReadOnly)
    FCombatModifierHandle ClaimedOrb;

    UPROPERTY(BlueprintReadOnly)
    TArray<FCombatOnHitAction> OnHitActions;
};
```

`UCombatAttackComponent` 持有 `TMap<int64, FCombatAttackRecord>` 和单调递增 Sequence，是 AttackRecord 的唯一权威所有者。Handle 通过 Attacker 查找该组件；Projectile、GameplayCue 和异步回调只保存 `FCombatAttackHandle`，不得复制整份 Record。Component EndPlay 时把所有 Pending record 终结为 Failed，并解除 Projectile 回调，不能把悬空记录留给下一局或对象复用。

普攻流程：

1. 攻击组件检查目标、距离、攻击冷却、状态标签。
2. 在 AttackComponent registry 创建 Pending AttackRecord，锁定 BaseDamage 和目标快照。
3. 按 Modifier Priority/ApplySequence 收集法球候选，调用无副作用的 `CanClaimAttack`；同一 exclusive orb group 只选出一个 winner。
4. winner 确认后调用 `OnAttackClaimed` 提交魔法/冷却，并把 bonus、damage type、ProjectileData 和 OnHitActions 快照写入 Record。提交失败则继续尝试下一个候选，未胜出的法球不能扣资源。
5. 近战立即完成攻击。
6. 远程生成只持有 AttackHandle 的 TrackingProjectile；目标死亡/不可选中或 Projectile 销毁时请求 fail。
7. 所有完成路径调用 `FinalizeAttack(Handle, Outcome)`。该函数只允许服务器把 Pending 原子地转换为 Landed 或 Failed；重复、过期或未知 Handle 直接忽略。
8. Finalize 内处理闪避、伤害和 OnHitActions，然后广播一次 OnAttackLanded/Fail 和 OnAttackRecordDestroy，最后从 registry 移除 Record。

AttackTarget Order 与 AttackRecord 是两条生命周期：

- 攻击前摇开始时创建 Record；前摇内被 Stop/Disarm 打断则 Finalize 为 Failed。
- 到达 attack point 后广播 `AttackLaunched(Handle)`。Order 不等待远程 Projectile 命中；已发射 Record 按快照继续存在。
- AttackComponent 根据 AttackSpeed/BaseAttackTime 计算下一次 attack-ready 时间，并注册 ScheduleOnce。回调只把状态切为 Ready，不因服务器卡顿补发多次攻击；持续的 AttackTarget Order 随后重新校验距离和目标并开始下一轮。
- AttackTarget 是持续 Order，直到目标失效、单位被下达替代命令或策略明确结束；它不会在每次 OnAttackLanded 时 pop。
- Stop 发生在 attack point 之后时只停止后续攻击，不回滚已经发射的 Projectile/Record，除非该攻击显式带有可 disjoint/cancel 配置。

示例：Drow Frost Arrows

- 技能 DataAsset：`Passive + Attack + AutoCast`，intrinsic modifier 为 `modifier_frost_arrows`。
- `modifier_frost_arrows::CanClaimAttack / OnAttackClaimed`
  - 检查 ability autocast。
  - 检查 owner 未沉默。
  - 在 `CanClaimAttack` 阶段声明候选，不修改资源。
  - 成为 winner 后提交资源并把 bonus damage、slow action 快照写入 record。
- `OnAttackLanded`
  - 执行 record 中的 slow action，slow_pct 使用发射时的 ability special 快照。

## 9. Projectile 设计

参考项目只有两类投射物：LinearProjectile 和 TrackingProjectile。UE 侧建议实现 Actor 基类，同时提供 AbilityTask 封装。

### 9.1 Actor 层

```cpp
UCLASS(Abstract)
class ACombatProjectile : public AActor
{
    GENERATED_BODY()

public:
    UPROPERTY(BlueprintReadOnly)
    FCombatProjectileSpec Spec;

    UPROPERTY(BlueprintReadOnly)
    FCombatProjectileHandle Handle;

    UFUNCTION(BlueprintNativeEvent)
    void OnProjectileHit(AActor* Victim, const FHitResult& Hit);

    UFUNCTION(BlueprintNativeEvent)
    void OnProjectileFinished();
};
```

`FCombatProjectileSpec` 在服务器 Spawn 时完成快照，至少包含 Source、Team、ProjectileData、Damage/GameplayEvent payload、RootEventId，以及可选 AttackHandle。Projectile 不保存裸 Ability 实例指针；即使 Ability 已结束或被移除，Projectile 仍能用快照上下文完成权威结算。

ProjectileSubsystem 为每个 Handle 维护 Active/Finished 状态。Hit、timeout、overlap 和 Actor EndPlay 最终都进入幂等 `FinishProjectile`；未知或已 Finished 的 Handle 不再广播回调。穿透 Projectile 可以产生多个带独立 EventId 的 OnHit，但每个 Victim 只命中一次，且 OnFinished 仍只广播一次。

派生：

- `ACombatLinearProjectile`
  - 记录上一帧位置。
  - 服务器每帧从 Previous 到 Current 做 Sphere/Capsule Sweep，并按最大步长 substep。
  - 支持 `bDestroyOnFirstHit` 和 AlreadyHit 集合。
- `ACombatTrackingProjectile`
  - 持有 Target weak pointer。
  - 在服务器帧 Tick 中朝目标当前位置移动，不进入 Combat Scheduler。
  - 目标死亡、OutOfGame、Untargetable 时 fizzle。
- `ACombatAttackProjectile`
  - 只包含 `FCombatAttackHandle`，不复制 AttackRecord。
  - 命中/结束时调用 AttackComponent 的幂等 `FinalizeAttack`。

### 9.2 AbilityTask 层

复杂蓝图技能不应直接 SpawnActor 到处写重复逻辑，但需要区分短生命周期 Spawn Task 和可选的 Wait Task：

```text
UAbilityTask_SpawnLinearProjectile
UAbilityTask_SpawnTrackingProjectile
UAbilityTask_WaitProjectileResult (optional)
```

Spawn Task 只负责校验参数、向服务器 ProjectileSubsystem 请求创建、返回 `FCombatProjectileHandle`，随后立即 `EndTask`。Projectile 的命中、超时和 fizzle 由 Actor/Subsystem 根据快照 Context 结算，不依赖 Spawn Task 或 Ability 继续存活。

只有技能语义明确要求“Ability 保持 Active 直到弹体结束”时，才使用 `WaitProjectileResult`：

```text
OnHit(Victim, HitLocation)
OnFinished()
OnFizzled()
```

Ability 提前结束时 Wait Task 只解除订阅，不能销毁或取消已经脱离 Ability 生命周期的权威 Projectile。需要随 Ability 取消的 Projectile 必须在 Spec 中显式配置 `bCancelWithSourceAbility`，由 ProjectileSubsystem 根据 Ability ActivationId 统一取消。

表现：

- 用 GameplayCue 或 Niagara Component 表现弹体。
- 投射物逻辑由服务器权威处理。
- 客户端可以预测生成纯表现弹体，但命中以服务器为准。
- 权威 Actor 和预测表现使用同一个 ProjectileId 做 reconcile，避免服务器 Actor 与本地 GameplayCue 重复显示。

当前 `ATwinStickProjectile` 可以作为第一版视觉/碰撞参考，但需要把 `NPC->ProjectileImpact` 改成发送 GameplayEvent 或调用 `UCombatDamageSubsystem::DealDamage`。

## 10. Thinker 与 AoE

参考项目的 thinker 是隐藏、中立、不可选、无碰撞、无血条的临时单位，用于周期 Think 或区域效果。UE 中建议两种实现：

### 10.1 Actor Thinker

`ACombatThinker`：

- Hidden in game 或只有调试表现。
- 不参与单位碰撞。
- 持有 Source、Ability、Team、Duration。
- 可带 Sphere/Box/Capsule 碰撞或纯定时查询。
- 可挂一个 ModifierRuntime，由 ModifierComponent/Scheduler 驱动 OnIntervalThink；Thinker Actor 默认关闭 Tick。

适用于：

- Lina Light Strike Array 延迟爆炸。
- Earthshaker Fissure 临时阻挡/区域。
- Healing Ward 周期治疗。
- 毒圈、火圈、持续 AoE。

### 10.2 GameplayAbility 内部 Task

对于短生命周期 AoE，可用 AbilityTask：

```text
WaitDelay -> SphereOverlap -> ApplyDamage/Modifier -> EndTask
```

当前 `ATwinStickAoEAttack` 可以升级为 `ACombatThinker` 的一个蓝图子类。

## 11. Order 与 NavMesh 移动

参考项目自带网格 A*、WallTracer、动态圆避障。本项目已经有 UE NavMesh/EQS/AIController，建议不重写寻路。

### 11.1 指令类型

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

    UPROPERTY(BlueprintReadOnly)
    int64 Generation = 0;
};
```

`UCombatOrderComponent` 持有 FIFO：

- `IssueOrder(Order, bQueue)`
- `ClearOrders`
- `PumpCurrentOrder`
- `OnMoveFinished(OrderHandle, MoveRequestId, Result)`
- `OnAbilityOrderReleased(OrderHandle, Result)`
- `OnAttackLaunched(OrderHandle, AttackHandle)`
- `OnAttackCycleReady(OrderHandle)`

`bQueue=false` 表示中断当前可中断行为、清空 pending queue，并以新的 generation 立即执行该 Order；`bQueue=true` 只追加。Stop 永远提升 generation、取消当前 EQS/Move/Ability/Attack，并清空全部 pending order。每次 Pump 都先验证回调携带的 OrderHandle 等于当前 handle，旧 generation 的任何异步结果都直接丢弃。

### 11.2 移动执行

复用 `AStrategyUnit::MoveToLocation` 思路：

- Point 移动：`AAIController::MoveTo` + `FAIMoveRequest`。
- 多单位移动：仍可用 EQS 为每个单位找分散位置。
- CastTarget/AttackTarget：
  - 距离不足时 MoveTo 目标附近，通过 Scheduler Coalesce 任务低频复查，目标位移超过阈值时可提前唤醒。
  - 有效距离必须同时考虑双方碰撞半径、AttackRange/CastRangeBonus 和技能要求的视线规则。
  - 到达距离后停止移动，重新校验目标与距离，转向目标，再激活技能/普攻。
- Rooted：取消当前 MoveRequest 但不清队列；Root tag 归零时重新 Pump 当前 Order。
- Stunned/Hexed：中断当前可中断 Ability/attack windup，暂停 Pump；默认保留 pending queue，状态 Tag 归零后按当前 Order 的失败策略继续或清空。
- MotionController：暂停普通 MoveTo，位移结束后恢复/重判队首。

当前 `AStrategyUnit` 只能作为导航逻辑参考，不能直接把 `OnMoveCompleted` 接入 Order 队列。适配时必须：

- 保存当前 `UEnvQueryInstanceBlueprintWrapper` 和 `FAIRequestID`，并绑定当前 OrderHandle。
- 新 Order/Stop 时取消旧 EQS，调用 `AIController->StopMovement()`，使旧请求进入可识别的 Aborted 终态。
- `OnEQSFinished` 先比较 QueryInstance 与当前实例；`OnRequestFinished` 同时比较 RequestID 和 OrderHandle。
- 分别处理 Success、AlreadyAtGoal、Blocked、Aborted、Invalid 和 PartialPath，只有匹配当前请求的成功结果才能推进队列。

OrderComponent 与 Ability 的衔接建议：

- `CastNoTarget/CastPoint/CastTarget` 一旦成功进入 cast point，标记为 dispatched。
- 非引导技能在 `OrderReleased` 后 pop 当前 cast order，而不是等 `AbilityEnded` 或 cooldown。
- 引导技能在 `AbilityChannelEnded` 后由 Ability 广播 `OrderReleased`；中断时按 AbilityData 的明确策略继续或清空队列。
- 施法距离不足时，OrderComponent 只派生 MoveTo，不直接激活 Ability。
- 目标在追击或 cast point 中死亡/不可选中时，当前 order fail 并进入下一条，或按配置清队列。

### 11.3 碰撞与避让

UE 侧建议：

- CharacterMovement `bUseRVOAvoidance = true` 或 Detour Crowd。
- Capsule 半径对应 Dota hull radius。
- `State.NoUnitCollision` 时调整 collision response 或 movement avoidance。
- 阻挡型地形技能：
  - 简单版：生成 Blocking Volume / NavModifierVolume，并调用 NavigationSystem 更新。
  - 高级版：自定义 NavArea 和动态导航代价。

动态 NavMesh 阻挡需要分阶段处理。UE 的 Runtime NavMesh 更新可能异步且有成本，AI 当前 PathFollowing 不一定立刻重算，因此不要在第一版把 Fissure 这类技能的正确性建立在动态 NavMesh 之上。

推荐阶段：

1. 第一版：生成物理阻挡体，设置 Pawn/Projectile collision；对正在 MoveTo 的单位调用 StopMovement + 重新 `PumpCurrentOrder`，让 AI 自行 repath。
2. 第二版：启用 Runtime Navigation Generation，使用 `NavModifierVolume` / `UNavModifierComponent` 标记临时不可通行区域，并在创建/销毁时主动请求相关单位重新寻路。
3. 第三版：对高频或大量临时阻挡，改为自定义局部避障/代价系统，避免频繁重建 NavMesh。

## 12. 蓝图替代 Lua

参考项目 Lua Ability 只暴露少量 self API，Modifier 暴露较多 Hook。UE 里建议定义两类蓝图基类：

### 12.1 Ability 蓝图事件

```text
ReceiveSpellStart(Context)
ReceiveChannelTick(Context, TickContext)
ReceiveChannelFinish(Context, bInterrupted)
GetSpecialValue(Key)
GetCaster()
GetTargetActor()
GetTargetLocation()
```

### 12.2 Modifier 蓝图事件

```text
OnCreated(Context)
OnDestroyed(Context)
OnRefresh(Context)
OnIntervalThink(TickContext)
OnPreDealDamage(ref DamageEvent)
OnPreTakeDamage(ref DamageEvent)
OnDamageBlock(ref DamageEvent)
OnPostDealDamage(DamageResult)
OnPostTakeDamage(DamageResult)
OnPreTakeHeal(ref HealEvent)
OnPostTakeHeal(HealResult)
OnAbilityExecuted(AbilityEvent)
CanClaimAttack(AttackRecord)
OnAttackClaimed(ref AttackRecord)
OnAttack(ref AttackRecord)
OnAttackLanded(AttackRecord)
OnAttackFail(AttackRecord)
OnAttackRecordDestroy(AttackRecord)
OnMotionTick(MotionHandle, DeltaTime)
OnMotionInterrupted(MotionHandle)
GetAttackProjectileName()
```

关键约束：

- 数值必须优先来自 DataAsset special，不在蓝图中硬编码。
- Modifier Runtime 可以在自身保存实例状态，例如护盾剩余值、法球已认领 record id。
- 蓝图只写行为，冷却、耗蓝、状态校验、目标过滤、事件派发由 C++ 基类统一处理。

## 13. 网络同步策略

GAS 默认适合网络同步，但 Dota-like 技能会有很多服务器权威逻辑：

本项目是一个 PlayerController 间接控制多个 AI possessed Character 的结构，因此 ASC 放在 Unit Character 上，`OwnerActor = AvatarActor = Unit`。玩家拥有的 Unit 还必须在服务器设置 `Unit->SetOwner(CommandingPlayerController)`，让 Mixed replication 能找到 owning connection；AIController 继续负责寻路，不作为网络 OwnerActor。

复制策略按单位类型固定，而不是运行时随数量模糊切换：

| 单位类型 | ASC Replication Mode | 完整 ActiveGE 可见范围 | UI 数据来源 |
| --- | --- | --- | --- |
| 玩家拥有的英雄/单位 | Mixed | owning client；其他客户端仅 minimal | owner 读 ActiveGE，其他客户端读 replicated combat view model |
| 中立或纯服务器 AI | Minimal | 无 owning client | replicated combat view model + Tags/Attributes |
| 小规模调试/自动化地图 | Full | 全部客户端 | 仅用于调试，不作为正式配置 |

服务器在 Unit 完成 Owner/Controller 设置后调用 `InitAbilityActorInfo(Unit, Unit)`；客户端在 BeginPlay 以及 `OnRep_Owner` / `OnRep_Controller` 后刷新 ActorInfo。初始化函数必须幂等，并在 Avatar/Owner 改变时解绑旧 delegate。由于一个玩家拥有多个独立 ASC，本设计不把这些 Unit 的 ASC 放到单一 PlayerState。

- Attribute、GE、GameplayTag 由 ASC 复制。
- Ability 激活可做客户端预测，但复杂目标/投射物命中以服务器为准。
- Projectile Actor：
  - 服务器生成权威 Actor。
  - 客户端生成 GameplayCue/Niagara 表现。
  - 命中只在服务器结算。
- CombatEvent/Log：
  - 服务器记录完整事件。
  - UI 只订阅本地需要的简化事件。
- Order：
  - PlayerController 将 Order RPC 到服务器。
  - 服务器执行 MoveTo/Attack/Cast。
  - 服务器校验 Unit 是否属于调用者、AbilitySpec 是否已授予、目标阵营/状态、位置是否为有限值且可接受，并限制 RPC 频率和单包 Order 数量。
  - 客户端永远不能提交 Damage Amount、ModifierData、资源结果或 Attack Finalize。
- ModifierRuntime：
  - 只在服务器作为权威实例运行。
  - 客户端不要依赖 Runtime UObject 复制。
  - UI 所需的名称、图标、层数、剩余时间、是否 debuff，通过 Active GameplayEffect 或独立 replicated view model 暴露。
  - 纯表现通过 GameplayCue；战斗日志通过 GameplayMessage/CombatLog 复制或本地投影。

敌方 Buff/Debuff 面板不能假设 Mixed/Minimal 模式下存在完整 ActiveGE。`FCombatModifierView` 至少复制稳定 DefinitionId、StackCount、ServerEndTime、Debuff/Dispellable flags；名称、图标和本地化文本由客户端通过 DefinitionId 查 DataAsset。纯服务器 Runtime 状态不进入该 View。

第一阶段如果只做单机/PIE，可以先不做预测，只保证服务器权威路径干净。

多人阶段建议先采用“低预测”策略：

- 指令、施法、投射物命中都由服务器确认。
- 客户端只预测本地输入反馈、施法指示器、非命中特效。
- 等 Damage/Modifier/Projectile 全链路稳定后，再考虑本地预测瞬发技能或普通移动。

## 14. 示例技能落地

### 14.1 Lina Dragon Slave

数据：

- Behavior：PointTarget、AoE。
- CastPoint：0.45。
- Cooldown/ManaCost/Special：damage、radius、range。

蓝图逻辑：

1. `ReceiveSpellStart`
2. 读取 caster location 和 target point。
3. 计算方向和范围终点。
4. 做 capsule/box/sphere sweep 或 `FindEnemiesInLine`。
5. 对每个敌人调用 `DealDamage(Magical, damage)`。
6. 触发 GameplayCue。

### 14.2 Pudge Meat Hook

数据：

- Behavior：PointTarget。
- Special：damage、length、width、missile_speed。

蓝图逻辑：

1. `ReceiveSpellStart`
2. Spawn LinearProjectile，`bDestroyOnFirstHit = true`。
3. OnHit：
   - DealDamage Magical。
   - 给目标挂 `modifier_hook_drag`。
4. `modifier_hook_drag`：
   - State：Stunned、NoUnitCollision。
   - 通过 CombatMotionComponent 请求高优先级 Horizontal motion；获取失败时立即结束 Modifier。
   - MotionComponent 使用 RootMotionSource/CharacterMovement 朝 caster 移动，并统一处理碰撞。
   - 结束或中断时释放 MotionHandle、校正到 NavMesh，再恢复 Order。

### 14.3 Drow Frost Arrows

数据：

- Behavior：Passive、Attack、AutoCast。
- IntrinsicModifier：`modifier_frost_arrows`。
- Special：bonus_damage、slow_duration、slow_pct。

Modifier：

- `GetAttackProjectileName` 返回冰箭 GameplayCue/ProjectileData。
- `CanClaimAttack` 只声明法球候选。
- 成为 winner 后提交蓝量/冷却，并把 slow on-hit action 快照到 AttackRecord。
- `OnAttackLanded` 执行快照 action，不能重新读取一个可能已经升级或移除的 Ability 实例。

### 14.4 Magic Shield

Modifier：

- GE 提供 `MagicResist +0.10`。
- Runtime 保存 `_remaining = 200`。
- `OnDamageBlock`：
  - 只处理 Magical。
  - 吸收 `min(remaining, amount)`。
  - 修改 DamageEvent.Amount，并累加 DamageEvent.AbsorbedAmount；Subsystem 最终复制到 DamageResult。
  - remaining 归零时请求移除自身 GE；实际移除延迟到当前 Hook 阶段完成。

魔免和 HPLoss 分支发生在 `OnDamageBlock` 之前，因此不会错误消耗 Shield remaining。

### 14.5 Earthshaker Fissure

技能：

- PointTarget。
- 线性范围伤害 + stun + knockback。
- 中点生成 `ACombatThinker_FissureBlocker`。

阻挡实现分两阶段：

- 第一阶段：只做视觉 thinker，不阻挡导航。
- 第二阶段：生成阻挡碰撞体并强制相关单位 repath。
- 第三阶段：生成动态 NavModifierVolume，持续时间结束后移除并刷新 NavMesh。

## 15. 与当前工程的集成步骤

### 阶段 0：工程准备

1. Build.cs 添加 GAS 依赖。
2. uproject 启用 GameplayAbilities。
3. 新增 `Combat` 目录。
4. 定义 GameplayTags：State、Ability、Damage.Type、Damage.Flag、Data、Event、Cue。
5. 定义 ScheduleHandle、ScheduledTickContext、CatchUpPolicy，并实现服务器 `UCombatSchedulerSubsystem`。
6. 定义 EventId、ModifierHandle、AttackHandle、OrderHandle、ProjectileHandle 和 Damage/Heal Result。
7. 实现 `FCombatGameplayEffectContext`、NetSerialize 和项目 AbilitySystemGlobals。
8. 建立 `ACombatUnitCharacter`，明确 ASC Owner/Avatar、InitActorInfo 和 replication mode。
9. 建立统一 authority guard、Hook 稳定排序和 deferred operation queue。

### 阶段 1：属性与伤害

1. 实现 `UCombatAttributeSet`。
2. 实现初始化 GE/DataAsset。
3. 实现 `UCombatDamageSubsystem` / `UCombatHealSubsystem` 和事务结果槽。
4. 实现纯 C++ Damage/Heal Calculator 与只应用最终 Meta Attribute 的 Instant GE。
5. 做自动化测试：物理护甲、魔抗、纯粹伤害、魔免、HPLoss、实际 delta、死亡唯一广播、治疗增幅。

### 阶段 2：Modifier

1. 实现 `UCombatModifierData`。
2. 实现 `UCombatModifierRuntime`。
3. 实现 `UCombatModifierComponent::ApplyModifier`、EffectContext 关联和 Handle -> Runtime 映射。
4. 支持稳定 Priority、deferred mutation，并由 Scheduler 驱动 OnCreated/Destroyed/IntervalThink/Damage/Heal Hook。
5. 做示例：Stun、Slow、Shield、DOT。

### 阶段 3：Ability

1. 实现 `UCombatAbilityData`。
2. 实现 `UCombatGameplayAbility` 的 InstancingPolicy、TargetData 校验、`UAbilityTask_WaitCombatInterval` 和完整施法生命周期。
3. 实现 DataDriven Actions。
4. 支持幂等 Cost/Cooldown commit point、CanCast、TargetFilter、OrderReleased。
5. 做示例：无目标治疗、单位目标伤害、点目标 AoE。

### 阶段 4：Order 与普攻

1. 实现带 generation 的 `UCombatOrderComponent`。
2. 为当前 StrategyUnit 增加 EQS/FAIRequestID 关联与取消适配层，再接入 `DoMoveUnitsCommand`。
3. 实现动态目标追击、AttackTarget/CastTarget 和状态恢复。
4. 实现 `UCombatAttackComponent` registry、AttackHandle、Scheduler attack point/ready 和幂等 FinalizeAttack。
5. 支持近战、远程、闪避、exclusive orb 仲裁和 on-hit 快照。

### 阶段 5：Projectile 与 Thinker

1. 实现 Linear/Tracking Projectile。
2. 实现 ProjectileHandle、快照 Context、短生命周期 Spawn Task 和可选 Wait Task。
3. 实现默认关闭 Tick、由 Scheduler 驱动 pulse/delay 的 `ACombatThinker`。
4. 实现 `UCombatMotionComponent` 的通道、优先级、中断、碰撞和 Order 恢复。
5. 改造 `ATwinStickProjectile` 和 `ATwinStickAoEAttack`，让它们走 Combat Damage/GameplayEvent。

### 阶段 6：复杂技能与工具

1. 实现 Pudge Hook、Drow Frost Arrows、Lina Dragon Slave、Earthshaker Fissure。
2. 增加 CombatModifierView、战斗日志 UI 和 Mixed/Minimal 多客户端验证。
3. 增加 DataAsset 编辑校验，禁止双向硬引用和缺失 DefinitionId。
4. 增加 PIE 测试地图。
5. 后续再做网络预测、录像回放、技能编辑器。

## 16. 测试建议

参考仓库测试覆盖很细，本项目也应优先建立自动化测试：

- Attribute 初始化和 GE 修改。
- Combat Scheduler：
  - 可变帧率下 repeating task 不漂移，执行次数与绝对时间一致。
  - 同一时间任务按 Priority/ApplySequence 稳定排序。
  - callback 内 Cancel/Reschedule、Owner 销毁和 generation 失效不会重复回调。
  - ExecuteAllBounded、Coalesce、SkipExpired 和全局 budget 符合策略。
  - Duration 同时到期时 bTickOnExpire、PreservePhase/ResetInterval 行为明确。
  - 纯 Client 不执行权威 scheduler callback。
- Damage pipeline：
  - 护甲正负值。
  - 魔法/纯粹/物理。
  - 魔免、BypassMagicImmune。
  - HPLoss 跳过护盾和抗性。
  - 魔免/HPLoss 不消耗护盾，多个护盾按固定 Priority 消耗。
  - Health clamp 后的 AppliedDamage 和死亡只报告一次。
  - 反伤带 RootEventId 且不递归，超过 Depth 上限安全终止。
  - 吸血只读取实际 DamageResult，并遵守类型/flags。
- Modifier lifecycle：
  - OnCreated/OnDestroyed。
  - Duration 过期。
  - Stack 改变。
  - Purge strong/normal。
  - Hook 内移除自身或添加新 Modifier 不破坏当前遍历。
  - 相同 Priority 时按 ApplySequence 确定执行顺序。
- Ability lifecycle：
  - cast point 完成。
  - 被沉默/眩晕中断。
  - channel tick 和 finish。
  - Cost/Cooldown 每次激活最多提交一次，cast point 前后中断符合配置。
  - AbilitySpellStarted、OrderReleased、AbilityEnded 各自只广播一次。
  - 不同单位使用同一 Ability 类时目标快照互不污染。
- Projectile：
  - linear 首个命中销毁。
  - linear 穿透不重复命中。
  - tracking 目标死亡 fizzle。
  - 远程普攻 AttackRecord 只结算一次。
  - attack point 后的 Projectile 飞行不阻塞下一次 attack-ready，Stop 不回滚已发射 Record。
  - Ability 提前结束后，fire-and-forget Projectile 仍能结算。
  - 预测表现与服务器 ProjectileId reconcile 后不重复显示。
- Order：
  - Move/Cast/Attack FIFO。
  - queue=true 追加。
  - Stop 清空。
  - 距离不足自动追击。
  - 旧 EQS/MoveRequest 回调不会推进新 generation 的 Order。
  - Root 解除和 Motion 结束后只恢复当前有效队首。
- Network：
  - 未拥有 Unit 的客户端 Order RPC 被拒绝。
  - 客户端不能提交 Damage/Modifier/Attack 结果。
  - Mixed owner 与非 owner、Minimal AI 的 Attribute/Tag/View 数据符合矩阵。
- Motion：
  - 同通道优先级抢占与 OnMotionInterrupted。
  - 强制位移结束后位置在 NavMesh 上，并正确恢复 Order。

## 17. 关键风险与决策

- GAS 原生 GameplayEffect 不能完整表达 Dota Modifier Hook，所以必须有 ModifierRuntime 层。
- 逻辑周期统一走 Combat Scheduler；每个 Runtime/Thinker 自建 Tick 或 Timer 会造成漂移、不可控 catch-up 和非确定顺序。
- Runtime Hook 必须稳定排序并延迟结构修改，否则护盾、反伤和驱散会出现非确定结果或重入问题。
- Backswing 在 Dota 中不阻止新施法，UE 动画后摇不要简单用 Blocking Ability Tag 锁死所有输入。
- 默认在 AbilitySpellStarted 提交冷却/耗蓝；特殊技能只能通过显式 commit point 配置改变，并保持幂等。
- 投射物命中必须服务器权威，否则法球、吸血、反伤和多段命中会出现不一致。
- Projectile 结算不能依赖已经结束的 AbilityTask；长期上下文由 Projectile/Subsystem 持有。
- Attack、Order、Projectile 和移动请求必须使用稳定 Handle，任何未知或过期回调都不能推进状态。
- 多单位 AI possessed 架构必须显式设置 ASC owning connection，并用 CombatModifierView 补齐非 owner UI。
- NavMesh 动态阻挡技能要谨慎，第一版可只做视觉/碰撞，后续再引入动态 NavModifier。
- 蓝图能力要有强约束：数值来自 DataAsset，公共校验在 C++，蓝图只写技能差异逻辑。

## 18. 推荐落地原则

1. 先做统一 Damage/Modifier/Attribute，再做复杂技能。
2. 先保留 Dota 语义，再决定表现形式。
3. 能用 GAS 原生的用 GAS：属性、标签、持续时间、叠层、冷却、消耗、GameplayCue。
4. GAS 不擅长的补自定义层：AttackRecord、Modifier Hook、Order Queue、Projectile callbacks。
5. 当前 Strategy 的 NavMesh 移动和 TwinStick 的 Actor 弹体都可以作为第一版入口，但战斗结算必须收敛到 Combat Subsystem。
6. 帧 Tick 只服务连续运动；所有周期 gameplay 结算使用绝对时间 Scheduler 和稳定 Handle。
