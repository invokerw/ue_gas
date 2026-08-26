# 14 M0 设计冻结

> 冻结日期：2026-08-24
> 适用版本：Combat schema v1 / Formula v1 / RNG v1
> 状态：G0 已通过，用户已于 2026-08-24 验收

本文是 M0 的集中决策包。功能文档仍是各领域的主要契约；本文固定跨领域字段、默认值、失败语义、迁移和未来测试，以便 M1 不把未决项带入公共 API。

## 1. 工程核对基线

2026-08-24 通过源码、Config、Asset Registry 和 UE MCP 只读检查确认：

- Editor 可连接，检查时当前 Level 为 `/Game/TopDown/Lvl_TopDown`，未运行 PIE。
- GameplayTag Manager 中没有 `Combat.*`、`State.*`、`Ability.Behavior.*`、`TargetTeam.*`、`Damage.*`、`Order.Failure.*` 或 `Cue.Combat.*` 项目战斗 Tag；当前可见项均来自模板或引擎插件。
- `/Game` 中没有 Combat DataAsset、GameplayEffect 或战斗蓝图；只有 TopDown、Strategy、TwinStick 和公共模板资产。
- `DefaultEngine.ini` 没有自定义 Combat Object/Trace Channel 或 Collision Profile。
- AssetManager 只配置 Map、PrimaryAssetLabel 和 GameFeatureData，没有 Combat PrimaryAsset 类型。
- GAS 插件和模块依赖尚未启用，`Source/ue_gas/Combat` 不存在。

因此 M0 没有既有 Combat 内容迁移；本文件中的 redirect、schema/version 和兼容规则从 v1 起生效。模板碰撞、Tag 和资产名不获得 Combat 语义，后续只能通过明确适配接入。

## 2. DEC-001：队伍与目标关系

### 2.1 身份和关系

玩法层使用独立、可复制的 `FCombatTeamId`，而不是把 `FGenericTeamId` 直接暴露到公共 Combat API：

```cpp
USTRUCT(BlueprintType)
struct FCombatTeamId
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    uint8 Value = 255;
};

UENUM(BlueprintType)
enum class ECombatTeamRelation : uint8
{
    Friendly,
    Hostile,
    Neutral,
    Invalid
};
```

固定值域：

- `0`：Neutral camp，是有效队伍；默认与所有不同队伍 Hostile。
- `1..254`：普通有效队伍。
- `255`：NoTeam/Invalid，不能通过需要阵营的目标校验。
- 相同有效 TeamId 为 Friendly；不同有效 TeamId 默认为 Hostile。
- 服务器可在单一 diplomacy table 中覆盖某一有序 TeamId 对为 Friendly、Hostile 或 Neutral；禁止 Unit、Ability、Order、Projectile 自行比较数值或维护局部特例。
- `Self` 不是队伍关系。是否允许自身目标由目标规则的 `bAllowSelf` 单独控制。

`UCombatTeamSubsystem` 是关系唯一入口，并提供 `GetTeamId`、`GetRelation`、`IsTargetTeamAllowed` 和服务器 `SetTeamId`。v1 diplomacy table 在 World 初始化时从同一 match rule 加载，运行中不可变；客户端副本只用于 UI 预览，服务器结果始终权威。Unit 可把 CombatTeamId 派生映射给 `IGenericTeamAgentInterface` 供 AI 感知使用；该映射不是玩法关系权威来源。

### 2.2 TargetTeam 语义

```text
TargetTeam.None     不接受单位目标
TargetTeam.Friendly 接受 Friendly；Self 还要求 bAllowSelf
TargetTeam.Enemy    接受 Hostile，包括默认 Neutral camp
TargetTeam.Both     接受 Friendly 或 Hostile；Self 仍要求 bAllowSelf
```

显式 diplomacy `Neutral` 不被 Friendly/Enemy/Both 接受；只有目标规则 `bAllowNeutralRelation=true` 时接受。NoTeam 返回 `Combat.Failure.Target.TeamInvalid`。

共享目标失败 Tag 至少包括：

```text
Combat.Failure.Target.Invalid
Combat.Failure.Target.TeamInvalid
Combat.Failure.Target.SelfNotAllowed
Combat.Failure.Target.FriendlyNotAllowed
Combat.Failure.Target.HostileNotAllowed
Combat.Failure.Target.NeutralNotAllowed
```

Order 和 Ability 保留 Targeting 返回的原始 FailureTag，不翻译为另一套阵营错误。Projectile/Thinker 在生成时快照 SourceTeamId；命中时把快照来源队伍与目标当前 TeamId 交给同一 Relation API。

### 2.3 队伍变化和召唤物

- TeamId 只允许服务器修改。修改产生一次 `Event.Combat.TeamChanged`，并触发当前 Target/Order、Aura child 和持续追踪对象按各自 policy 重新校验。
- 召唤物默认在服务器 spawn 时复制召唤者当时的 TeamId；UnitData 可显式覆盖。召唤者之后换队不自动传播。
- gameplay owner、CommandingController 和 TeamId 是三种独立身份；能下达 Order 不隐含 Friendly，Friendly 也不隐含 RPC 所有权。
- illusion、可共享控制和动态联盟的产品扩展继续登记在 GAP-015，但只能扩展 TeamSubsystem/diplomacy table，不能新增关系旁路。

### 2.4 兼容和测试

M1/M3 至少覆盖：相同队、不同队、Neutral camp、NoTeam、Self 开关、显式 Neutral relation、TeamChanged 后重校验，以及召唤物快照后召唤者换队。失败用例必须断言稳定 FailureTag。

## 3. DEC-002：Unit 生命状态

### 3.1 状态机和 generation

```text
Initialise -> Alive(generation=1)
Alive -> Dying -> Dead -> Respawning(generation++) -> Alive
```

- `ECombatLifeState` 只有 `Alive`、`Dying`、`Dead`、`Respawning` 四个公开运行状态；未完成初始数据应用的 Actor 不对 Combat API 注册为 Unit。
- `LifeGeneration` 使用 `uint32`，`0` 永远无效；首次进入 Alive 为 `1`。
- 只有服务器 `RequestDeath(EventId)` 能执行 `Alive -> Dying`。转换使用 compare-and-set 语义；同一帧多次 lethal result 只有首个成功，其余返回 `Combat.Failure.Life.NotAlive`，不再广播 Death。
- Dying 是同步清理阶段，不依赖动画、Timer 或 Scheduler；清理完成后进入 Dead 并广播一次 `Event.Combat.UnitDeath`。
- 只有服务器 `RequestRespawn(RespawnSpec)` 能执行 `Dead -> Respawning`。所有输入预检成功后立即递增 generation；资源初始化、位置和状态恢复完成后进入 Alive 并广播一次 `Event.Combat.UnitRespawned`。
- generation 溢出到 `0` 视为不可恢复诊断错误，拒绝本次 respawn；不允许绕回后复用旧 Handle。

所有 Unit 相关异步 Handle/请求都携带 LifeGeneration。回调有效性至少同时检查 Actor、组件、generation 和当前状态；旧生命回调在新生命中只能记录 invalid reason，不能改变状态。

### 3.2 Death 清理顺序

成功进入 Dying 后按固定顺序执行：

1. 设置 LifeState=Dying，应用互斥状态 Tag `State.Dying`，关闭新的 Order、Attack、Ability 激活、Heal 和普通 Modifier Apply。
2. 提升 Order/Attack 内部 generation；取消当前和排队 Order、EQS、AI Move、attack windup/ready schedule。
3. 取消所有活动 Ability；已脱离 Ability 的 Projectile/AttackRecord 按不可变快照和自身 target-lost policy 继续或 fizzle。
4. 释放 Horizontal/Vertical Motion，停止 CharacterMovement 的 gameplay 请求。
5. 按 `Priority desc -> ApplySequence asc` 快照 Modifier；deferred 移除 `bRemoveOnDeath=true` 项。保留项不得在非 Alive 状态执行默认 Damage/Heal/Order Hook，除非定义显式 `bAffectsDeadUnits`。
6. 将碰撞切到 corpse policy：不再参与 CombatUnit 阻挡和目标候选；保留 Actor/表现，不自动 Destroy。
7. 完成 deferred cleanup，设置互斥状态 Tag `State.Dead`，广播一次 UnitDeath 和结构化日志。

Death 不销毁 Unit Actor，不通过 Health 写入重复触发，不任意清除已发射 Projectile。

### 3.3 Respawn 清理顺序

`RequestRespawn` 仅接受 Dead；位置必须有限、在允许地图范围内，并按 UnitData policy 完成 NavMesh 投影。接受后：

1. generation 加一，进入 Respawning/`State.Respawning`；再次保证 Order、Attack、Ability、Motion 和 life-bound Schedule 为空。
2. 通过 UnitData 初始化 GE 恢复基础属性；默认 Health=MaxHealth、Mana=MaxMana，禁止用 Heal 模拟复活。
3. AbilitySpec 保留；活动实例保持已取消；AutoCast 开关和 cooldown 默认保留。Intrinsic Modifier 以 `(AbilitySpecHandle, DefinitionId)` 幂等 reconcile。
4. `bRemoveOnDeath=false` 的 Modifier/ActiveGE 和 cooldown 保留原 Handle、层数与绝对到期时间；它们是显式跨生命对象，不得持有上一 LifeGeneration 的 Order/Attack/Motion Handle。其普通战斗 Hook 在重新 Alive 后恢复。
5. 恢复正常 CombatUnit 碰撞和可见状态，进入 Alive/`State.Alive`，广播一次 UnitRespawned。

尸体时长、奖励归属、按 Unit 类型覆盖 Mana/cooldown/保留 Modifier 的配置在 GAP-013/M2 收口；在此之前上面的默认值是可实现基线，不能由技能蓝图改写。

### 3.4 兼容和测试

M2 至少覆盖：两个同帧 lethal Event、重复 RequestDeath、Dying/Dead 拒绝 Order/Heal/Modifier、旧 generation 回调、保留/移除 Modifier、已发射 Projectile、无效 respawn 位置、重复 respawn 和 generation `0` 防护。

## 4. DEC-003：Ability 授予、等级和 AutoCast

### 4.1 单一身份链

```text
AbilitySet entry -> Ability Class -> class CDO.AbilityData -> CombatAbility DefinitionId
                                      |
                                      +-> granted FGameplayAbilitySpec
                                          Spec.Level = runtime authority
```

- `UCombatAbilityData` 不引用 Ability Class。
- 每个 `UCombatGameplayAbility` 类 CDO 必须引用一个 AbilityData；每个 CombatAbility DefinitionId 恰好映射一个 Ability Class。
- `UCombatAbilitySet` 只保存授予配方：Ability Class、InitialLevel、初始 AutoCast 状态和 grant flags；不保存 DataAsset 的反向 Class 引用，不保存运行时 SpecHandle。
- UnitData 通过 AbilitySet DefinitionId 列表声明初始技能。服务器解析并授予；客户端不能提交 Class、DefinitionId 或 Level。
- 同一 Unit 第一版每个 Ability DefinitionId 最多一个 Spec。资产内重复条目是校验错误；同一 grant source 的重复初始化返回既有 Spec，其他冲突来源返回 `Combat.Failure.Ability.DuplicateDefinition`。

### 4.2 等级规则

- `FGameplayAbilitySpec.Level` 是唯一运行时等级，合法范围 `1..AbilityData.MaxLevel`；`0` 表示不授予，不创建 level-0 Spec。
- `InitialLevel`、升级和降级若越界均拒绝，不做静默 clamp。
- `GetSpecialValue` 使用 Spec.Level，special 数组长度必须为 `1`（所有等级共用）或 `MaxLevel`；空数组和其他长度阻止资产通过验证。
- 升/降级是服务器原子 API。成功产生一次 `Event.Combat.AbilityLevelChanged`；失败不修改 Spec、Intrinsic 或已创建快照。

技能点、经验、等级上限的产品来源不属于第一版；外部系统未来只能调用上述服务器 API。

### 4.3 移除、Intrinsic 和 AutoCast

- 移除是服务器 API，默认先禁止新激活，再取消该 Spec 的全部活动实例，移除其 Intrinsic Modifier，关闭 AutoCast，最后清除 Spec，并产生一次 `Event.Combat.AbilityRemoved`。
- 已经 fire-and-forget 的 Projectile、Thinker 和已 Launched AttackRecord 使用创建时快照继续；移除不能回滚它们。
- Intrinsic Modifier 的 owner key 为 AbilitySpecHandle + Ability DefinitionId；授予、ActorInfo 重建和 respawn reconcile 必须幂等，移除 Spec 后不得残留。
- AutoCast 是服务器保存的 per-Spec 状态。AbilitySet 可给初值；客户端/Order 只能请求切换，服务器验证 ownership、Spec、Ability Behavior 和 Unit life state。成功变化产生 `Event.Combat.AutoCastChanged`。
- AbilitySpec、等级和 AutoCast 默认跨 Death/Respawn 保留；活动实例在 Dying 被取消。

### 4.4 兼容和测试

M1/M3 至少覆盖：循环引用拒绝、一个 DefinitionId 对多个 Class、重复 AbilitySet、重复初始化、等级 0/越界、移除活动 Ability、Intrinsic 幂等、AutoCast 越权，以及移除后已发射快照不变化。

## 5. DEC-004：数值与 RNG

### 5.1 Numeric Policy v1

所有常量集中在 `FCombatNumericPolicyV1`；业务代码不能散落不同 clamp。权威规则：

| 类别 | v1 规则 |
| --- | --- |
| Damage/Heal/Shield 请求绝对值 | 必须有限且位于 `[0, 1.0e9]`；负值、NaN、Inf、超上限拒绝，不 clamp |
| Health/MaxHealth/Mana/MaxMana | Max 位于 `[1, 1.0e9]`；当前值在最终写入时 clamp 到 `[0, Max]` |
| Armor | 消费时 clamp 到 `[-10000, 10000]` |
| MagicResistPct | 消费时 clamp 到 `[-1.0, 0.95]` |
| Evasion/Crit chance | 消费时 clamp 到 `[0, 1]` |
| Spell/Heal/Received amplify | 消费时 clamp 到 `[-1.0, 10.0]` |
| LifestealPct | 消费时 clamp 到 `[0, 10.0]` |
| CooldownReduction/StatusResistance | 消费时 clamp 到 `[0, 0.90]` |
| 时间/距离/速度配置 | 必须有限且非负；各 DataAsset 另有字段上限，超界阻止资产验证 |

- 请求/资产输入非法时拒绝；合法聚合 Attribute 超出消费区间时 clamp，并在调试日志记录 Raw、Clamped 和 Policy 字段。
- 中间阶段不取整、不量化；与 GAS 一致使用 `float`。调度绝对时间使用 `double`。UI 可自行格式化，但显示取整不得回写 gameplay。
- Health 最终 clamp 后，以 `PreviousHealth > 0 && NewHealth == 0` 判定 lethal；不使用 UI 精度或任意 epsilon。
- 每个事务保留 Requested、各阶段 Raw/Clamped、Applied；改变常量或公式必须增加 FormulaVersion，而不是覆盖 v1 测试基线。
- 物理护甲使用 [05](05-Damage-Heal.md) 的公式；FormulaVersion v1 固定为 `1`。

### 5.2 Combat RNG v1

服务器唯一 `UCombatRngSubsystem` 提供可注入、可记录的无状态 keyed roll；禁止 gameplay 使用 `FMath::FRand`、全局随机流或蓝图 Random 节点。

```text
RollKey = MatchSeed + RootEventId + DomainTag + StableSubjectId + Ordinal
RollBits = CombatHash64V1(RollKey)
Roll01 = top 24 bits / 2^24       // [0, 1)
Success = Roll01 < Clamp01(Chance)
```

`CombatHash64V1` 不使用 `FName` index 或平台相关内存布局。它按下列固定步骤计算：DomainTag 的规范完整字符串用 UTF-8 做 FNV-1a-64（offset `0xcbf29ce484222325`，prime `0x100000001b3`）；`StableSubjectId` 是两个显式 `uint64`；MatchSeed、RootEventId、DomainHash、SubjectHigh、SubjectLow、Ordinal 依次以 `State = SplitMix64(State ^ Field)` 混合，初始 State 为固定 v1 salt `0xC0B47A11D06A5EED`，最后一次 State 即 RawBits。整数按数值参与，不序列化为本地字节或文本。`SplitMix64(x)` 固定为：`z=x+0x9e3779b97f4a7c15; z=(z^(z>>30))*0xbf58476d1ce4e5b9; z=(z^(z>>27))*0x94d049bb133111eb; return z^(z>>31)`，所有运算按 `uint64` 溢出。

- MatchSeed 在服务器 match/world 初始化时生成或由测试注入；结构化日志记录其公开 replay id，完整 seed 只写开发/回放流。
- DomainTag 必须是 Native Tag，例如 `Combat.RNG.Attack.Evasion`、`Combat.RNG.Attack.Crit`、`Combat.RNG.Modifier.Proc`。
- StableSubjectId 优先使用 AttackHandle；否则使用 Source/Target Net identity 与当前 LifeGeneration 的稳定组合。
- Ordinal 是同一 RootEvent/Domain/Subject 内的显式从 0 开始序号。插入其他 Domain 的 roll 不改变现有结果。
- 普攻固定顺序为 impact 合法性检查、Evasion、Crit、随后按 `Priority desc -> ApplySequence asc` 的 proc。未到达某阶段不生成该 roll record；不是通过消耗全局流“跳过”。

每个 roll 记录：FormulaVersion、RngAlgorithmVersion、RootEventId、DomainTag、StableSubjectId、Ordinal、DerivedSeed/RawBits、ChanceRaw、ChanceClamped、Roll01 和 outcome。测试可以注入 MatchSeed 或直接注入 roll provider，并可用记录逐项 replay。

### 5.3 兼容和测试

v1 记录不可用新算法重解释；算法变化增加 `RngAlgorithmVersion`。M1/M2/M4 至少覆盖所有有限值边界、拒绝与 clamp 区别、无中间取整、同 key 重放、不同 Domain 隔离、固定攻击 roll 顺序和注入值。

## 6. DEC-005：碰撞、LOS 和地图单位

### 6.1 单位制和范围

- Combat 运行时沿用 UE 单位：距离为厘米 `cm`、速度为 `cm/s`、时间为秒；DataAsset 字段和日志使用相同单位。
- 不存在第二套“玩法单位”。外部 Dota 数据若需要缩放，只能在导入器中显式转换并写入最终 cm，运行时不再转换。
- Cast/Attack/Order 默认使用 XY 平面边缘距离：`max(0, Dist2D(CenterA, CenterB) - RadiusA - RadiusB)`。
- Projectile sweep 使用三维路径距离。显式需要 3D cast 的技能通过 TargetPolicy 选择 3D，不能自行换公式。
- `CombatRangeToleranceCm = 5.0`，只用于最终 `EffectiveDistance <= Range + Tolerance`；UI 可使用同一 helper 预览，服务器仍重算。
- Unit radius 来自 Capsule 或 UnitData 显式 override，必须有限且非负。位置必须有限并位于 UE world bounds；需要导航的位置还必须通过允许的 NavMesh projection。

### 6.2 冻结的 Channel/Profile 名称

M1 配置以下固定名称；编号只是 DefaultEngine.ini 的落点，代码使用名称/集中常量：

| 类型 | 名称 | 默认职责 |
| --- | --- | --- |
| Object Channel | `CombatUnit` | 活体/可查询战斗单位 |
| Object Channel | `CombatProjectile` | 权威弹体查询体 |
| Object Channel | `CombatBlocker` | Fissure/门等显式 gameplay blocker |
| Trace Channel | `CombatTargeting` | 目标 LOS |
| Profile | `CombatUnit` | 阻挡 WorldStatic/WorldDynamic/CombatBlocker/CombatUnit；被 Projectile 查询 |
| Profile | `CombatUnitNoCollision` | 只忽略 CombatUnit；仍可被 Targeting/Projectile 查询并阻挡世界 |
| Profile | `CombatProjectile` | QueryOnly；候选 CombatUnit，阻挡 WorldStatic/WorldDynamic/CombatBlocker |
| Profile | `CombatBlocker` | QueryAndPhysics；阻挡 CombatUnit、CombatProjectile 和 CombatTargeting |
| Profile | `CombatCorpse` | 不阻挡/不作为 CombatUnit 候选；按表现需要保留 Visibility query |

核心 response 矩阵（未列出的 gameplay channel 默认 Ignore）：

| Profile | WorldStatic | WorldDynamic | CombatUnit | CombatProjectile | CombatBlocker | CombatTargeting |
| --- | --- | --- | --- | --- | --- | --- |
| CombatUnit | Block | Block | Block | Overlap | Block | Ignore |
| CombatUnitNoCollision | Block | Block | Ignore | Overlap | Block | Ignore |
| CombatProjectile | Block | Block | Overlap | Ignore | Block | Ignore |
| CombatBlocker | Block | Block | Block | Block | Block | Block |
| CombatCorpse | Ignore | Ignore | Ignore | Ignore | Ignore | Ignore |

`CombatTargeting` 对 WorldStatic、默认遮挡型 WorldDynamic 和 CombatBlocker 为 Block，对 CombatUnit/CombatProjectile 为 Ignore；Trigger、Volume 等非遮挡 WorldDynamic profile 必须显式 Ignore CombatTargeting。Visibility 是否命中尸体只服务选取/表现，不参与 CombatTargeting。

`State.NoUnitCollision` 只在 `CombatUnit` 与 `CombatUnitNoCollision` 间切换，不能使单位免疫 Projectile 或目标查询。ProjectileData 的 Friendly/Enemy、Source ignore、pierce、first-hit 和 world-stop 是 `FCombatProjectileHitPolicy` 数据，不通过临时改全局 Collision Response 表达。

同一 sweep 结果按 path distance 升序；`CombatHitTieToleranceCm = 0.1` 内先 Blocker、后 Unit，再以稳定 Actor Net identity 排序。物理回调返回顺序不是 gameplay 顺序。

### 6.3 LOS

- `bRequireLineOfSight=false` 时明确跳过；不能用客户端可见性替代。
- 开启时从 Source 的 `GetCombatTargetingOrigin()` 到 Target 的 `GetCombatTargetingAimPoint()` 使用 `CombatTargeting` 单线 trace；忽略 Source 和目标 Actor 自身。
- WorldStatic、会遮挡的 WorldDynamic 和 CombatBlocker 阻挡；CombatUnit、CombatProjectile、表现粒子和 Trigger 默认忽略。
- 任一端点非法、World 不同或 trace 无法执行返回 `Combat.Failure.Target.LocationInvalid`；命中阻挡返回 `Combat.Failure.Target.LineOfSightBlocked`。
- 5 cm 范围容差不适用于 LOS，不能穿过薄墙。特殊弧线/多点 LOS 必须作为显式 TargetPolicy，仍复用统一结果结构。

### 6.4 兼容和测试

当前工程没有同名 Channel/Profile，M1 可直接新增。M3/M5 至少覆盖边缘距离、半径、4.9/5.1 cm 容差、2D/3D policy、薄墙、动态 blocker、NoUnitCollision、Source ignore、同距 blocker/unit 和稳定命中排序。

## 7. DEC-006：GameplayTag 与资产身份

### 7.1 Native Tag 清单 v1

以下被 C++ 分支、序列化或测试直接引用的 Tag 在 M1 集中用 Native Gameplay Tags 声明；内容不得用同义名称另建一套：

```text
State.Alive / State.Dying / State.Dead / State.Respawning
State.Stunned / State.Silenced / State.Rooted / State.Disarmed / State.Hexed
State.Invisible / State.Invulnerable / State.OutOfGame / State.MagicImmune
State.Untargetable / State.NoUnitCollision / State.NoHealthBar / State.Frozen

Ability.Behavior.NoTarget / UnitTarget / PointTarget / Passive / Channelled
Ability.Behavior.AoE / Attack / AutoCast / IgnoreSilence
Ability.Behavior.IgnoreMagicImmune / IgnoreUntargetable

TargetTeam.None / TargetTeam.Enemy / TargetTeam.Friendly / TargetTeam.Both
Damage.Type.Physical / Damage.Type.Magical / Damage.Type.Pure
Damage.Flag.BypassMagicImmune / HPLoss / NoSpellAmplification
Damage.Flag.Reflection / NoLifesteal / NoCrit
Data.Damage.Final / Data.Heal.Final

Event.Combat.DamageApplied / HealApplied / ModifierApplied / ModifierRemoved / UnitDeath / UnitRespawned / TeamChanged
Event.Combat.AbilityGranted / AbilityRemoved / AbilityLevelChanged / AutoCastChanged
Event.Combat.AbilityCastStarted / AbilitySpellStarted / AbilityChannelEnded
Event.Combat.AbilityOrderReleased / AbilityInterrupted / AbilityEnded / AbilityActionFailed

Combat.Failure.Authority / InvalidNumber / ActionUnsupported
Combat.Failure.Target.Invalid / TeamInvalid / SelfNotAllowed
Combat.Failure.Target.FriendlyNotAllowed / HostileNotAllowed / NeutralNotAllowed
Combat.Failure.Target.Dying / Dead / Respawning / Untargetable / OutOfGame
Combat.Failure.Target.Invulnerable / MagicImmune / OutOfRange
Combat.Failure.Target.LocationInvalid / LineOfSightBlocked
Combat.Failure.Life.NotAlive / InvalidTransition
Combat.Failure.Ability.DuplicateDefinition / InvalidLevel / NotGranted / InvalidTargetData
Combat.Failure.Ability.Cost / Cooldown / CommitFailed / AlreadyActive / UnitStateBlocked

Order.Failure.Cancelled / QueueFull / PathFailed / Blocked
Order.Failure.AbilityRejected / UnitStateBlocked
Combat.RNG.Attack.Evasion / Combat.RNG.Attack.Crit / Combat.RNG.Modifier.Proc
```

`Cue.Combat.*` 的具体叶节点随内容增加，但根域保留。Damage Spec 必须恰好一个 `Damage.Type.*`。Order 若失败于共享 Targeting，直接携带 `Combat.Failure.Target.*`；`Order.Failure.*` 只表达 Order 自身状态机/路径失败。

核心 Tag 重命名需在同一变更加入 GameplayTag redirect，旧名在一个 ContentSchemaVersion 内只读兼容且禁止新资产写入；删除前 validator 必须确认无引用。逻辑代码只能引用 Native Tag 符号，不能散落字符串请求。

### 7.2 PrimaryAsset 身份

固定 PrimaryAssetType：

```text
CombatUnit
CombatAbility
CombatModifier
CombatProjectile
CombatAbilitySet
```

每个定义资产保存显式 `FName DefinitionName` 和 `int32 SchemaVersion=1`。类决定 PrimaryAssetType，`GetPrimaryAssetId()` 返回 `(FixedType, DefinitionName)`：

- DefinitionName 必须是 lower_snake_case，匹配 `^[a-z][a-z0-9_]*$`。
- DefinitionId 不从显示名、包路径或 UObject 名推导；移动/重命名 `.uasset` 不改变身份。
- 同一 Type 内 DefinitionName 全局唯一；空值、重复、类型不符或不支持的 SchemaVersion 是 cook error。
- 推荐路径 `/Game/Combat/Definitions/{Units,Abilities,Modifiers,Projectiles,AbilitySets}`，路径只用于组织。
- 网络、日志、View 和来源链只传 FPrimaryAssetId；客户端缺失时显示稳定 placeholder 并诊断，服务器加载失败则拒绝创建权威对象。

`CombatContentVersion` 从 `1` 开始。DefinitionId 改名必须在同一提交加入 `FCombatDefinitionRedirect(OldId, NewId, IntroducedInVersion)`；redirect 必须唯一、无环、目标存在，只允许一次规范化结果，禁止依赖长期 redirect chain。删除而无替代时登记 tombstone，旧存档解析为明确 MissingDefinition，不能静默绑定同名新资产。

Ability Class/Data 一一对应、AssetManager scan 和全项目唯一性由 Editor validator、启动开发检查和 cook gate 共同验证。

### 7.3 兼容和测试

M1 至少覆盖 Native Tag 可查、DamageType 0/1/2 个、空/非法/重复 DefinitionName、资产路径重命名不改 Id、合法 redirect、chain/cycle/缺失目标拒绝、客户端缺资产 placeholder，以及 Ability Class/Data 映射冲突。

## 8. DEC-007：P0 Gap 总评审

| Gap | M0 结论 | 代码 owner | 主要文档 | 测试落点 |
| --- | --- | --- | --- | --- |
| GAP-001 | 按 DEC-001 关闭 | TeamSubsystem / Targeting | [03](03-Ability-Targeting-Blueprint.md)、[07](07-Order-Movement.md) | M1 pure + M3 targeting |
| GAP-002 | 按 DEC-002 关闭 | UnitLifecycleComponent | [04](04-Modifier-Attributes-Motion.md)、[05](05-Damage-Heal.md) | M2 lifecycle |
| GAP-003 | 按 DEC-003 关闭 | Ability/AbilitySet | [03](03-Ability-Targeting-Blueprint.md) | M1 asset + M3 ability |
| GAP-004 | 按 DEC-006 关闭 | Tags/Data/Asset validation | [08](08-Data-Network-Observability.md) | M1 tags/assets |
| GAP-006 | 按 DEC-004 关闭 | CombatRngSubsystem | [05](05-Damage-Heal.md)、[06](06-Attack-Projectile-Thinker.md) | M1 pure + M4 attack |
| GAP-007 | 按 DEC-004 关闭 | NumericPolicy/Calculator | [04](04-Modifier-Attributes-Motion.md)、[05](05-Damage-Heal.md) | M1 pure + M2 calculator |
| GAP-009 | 按 DEC-005 关闭 | Targeting/Projectile/Config | [06](06-Attack-Projectile-Thinker.md)、[07](07-Order-Movement.md) | M3 LOS + M5 projectile |
| GAP-020 | 接受 M1 到期方案，M0 不提前实现 | TST-003 | [10](10-Implementation-Roadmap.md)、[11](11-Test-Plan.md) | M1 UBT/build/connect smoke |

其余 Gap 尚未到最迟节点，不阻止 G0；它们不得覆盖本文件已冻结的字段和权威边界。若后续结论与本文件冲突，必须新增 ADR、说明 supersede、迁移版本和测试变化。

## 9. G0 结论

- DEC-001 至 DEC-007 已有单一书面结论、owner、兼容策略和正反测试落点。
- M1 所需的 TeamId、LifeState/LifeGeneration、Ability identity/level、Numeric/RNG、Collision/LOS、Native Tag 和 PrimaryAsset 字段可以直接定义，不含 `TODO decide later`。
- M0 只修改文档；未启用 GAS、未创建 Combat 代码/Config/Content，也未执行编译或 Automation。
- G0 已由用户验收。M1 仍需用户单独授权后才能开始。
