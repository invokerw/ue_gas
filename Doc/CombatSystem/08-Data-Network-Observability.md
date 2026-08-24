# 08 数据、网络、UI 与可观测性

## 1. 数据资产身份

需要被网络、日志、回放或 UI 引用的定义使用 `UPrimaryDataAsset` 和稳定 `FPrimaryAssetId DefinitionId`：

```text
Unit / Ability / Modifier / Projectile / AbilitySet
```

运行时载荷传 DefinitionId，不复制 UObject 指针。AssetManager 负责客户端解析名称、图标、本地化、Cue 和静态数值。

规则：

- DefinitionId 在类型内唯一，不由显示名或磁盘临时路径推导。
- 重命名/迁移通过 redirect/version table 处理，不能静默生成新身份。
- Ability Class 单向引用 AbilityData；DataAsset 不反向引用 Class。
- Cook 前运行完整资产校验，错误阻止打包，警告进入报告。
- 运行时缺失资产用稳定占位显示并记诊断，服务器权威结算不得依赖客户端加载成功。

## 2. GameplayTag 治理

Tag 分域：

```text
State.*
Ability.Behavior.*
TargetTeam.*
Damage.Type.*
Damage.Flag.*
Data.Damage.* / Data.Heal.*
Event.Combat.*
Cue.Combat.*
Order.Failure.*
Combat.Failure.*
```

核心 C++ Tag 使用 Native Gameplay Tags 集中声明；内容扩展可来自配置，但禁止同义 Tag 并存。Tag 新增/废弃需要说明消费者和兼容策略。DamageType 在一个 Spec 中必须恰好一个。

## 3. ASC 所有权和复制矩阵

本项目由 PlayerController 间接控制多个 AI possessed Character。ASC 放在 Unit Character：

```text
OwnerActor = AvatarActor = Unit
```

玩家拥有的 Unit 由服务器 `Unit->SetOwner(CommandingPlayerController)`，让 Mixed replication 找到 owning connection；AIController 只负责导航，不作为 ASC OwnerActor。

| 单位类型 | ASC Mode | 完整 ActiveGE | UI 来源 |
| --- | --- | --- | --- |
| 玩家拥有英雄/单位 | Mixed | owning client | owner 读 ActiveGE；其他读 Combat View |
| 中立/纯服务器 AI | Minimal | 无 owning client | Combat View + Tags/Attributes |
| 调试/自动化小图 | Full | 全客户端 | 仅调试，不作正式配置 |

服务器在 Owner/Controller 设置后 `InitAbilityActorInfo(Unit, Unit)`；客户端在 BeginPlay、OnRep_Owner、OnRep_Controller 后刷新。初始化幂等，Owner/Avatar 变化时解绑旧 delegate。

## 4. 权威边界

- Attribute、必要 GE/Tag 由 ASC 复制。
- Order 由 PlayerController RPC 到服务器执行。
- Damage、Heal、ApplyModifier、Attack Finalize、Projectile Hit 只在服务器。
- 客户端不能提交 Amount、flags、ModifierData、资源结果、Attack/Projectile Finish。
- 客户端 TargetData 只作请求，服务器重算队伍、状态、范围、LOS 和位置。
- 复杂目标/投射物低预测；第一版只预测输入反馈、指示器和非命中特效。

RPC 防护：

- 所有权、AbilitySpec、目标身份和 Unit life generation 校验。
- 每 connection token bucket/时间窗限频。
- 单包 Unit/Order/TargetData 数量和序列化大小上限。
- FVector 有限值、世界范围和可选 NavMesh 投影。
- 重复 request id 幂等拒绝或返回已有结果，防止重放产生双 Order。

## 5. Modifier 和 Unit View

客户端不复制权威 ModifierRuntime UObject。敌方或 Minimal Unit UI 使用扁平 View：

```cpp
struct FCombatModifierView
{
    FPrimaryAssetId DefinitionId;
    int32 StackCount;
    double ServerEndTime;
    bool bIsDebuff;
    bool bDispellable;
};
```

可用 FastArray 增量复制。名称/图标/文本由 DefinitionId 本地解析；护盾剩余值等秘密 Runtime 状态只在产品明确需要展示时增加量化字段。

建议 `FCombatUnitView` 至少投影：

- Unit DefinitionId、TeamId、life generation、Alive/Dead 状态。
- Health/MaxHealth、Mana/MaxMana 或 ASC 直接可见 Attribute。
- 当前 cast/channel DefinitionId、ServerStart/EndTime。
- 可见 ModifierView。

View 只服务 UI/表现，不可反向成为服务器战斗判定来源。

## 6. Projectile 表现复制

- 服务器权威 Projectile Actor 或紧凑 replicated projectile state。
- 客户端可本地创建预测视觉，使用 ProjectileId 和服务器实体 reconcile。
- 只复制开始参数、必要修正和结束原因，不复制每次 Scheduler tick。
- 命中 Result 由 CombatEvent/属性变化投影。
- relevancy、cull distance 和 dormancy 在多单位压力测试后定，不在第一版猜测。

## 7. Combat Event 与日志

服务器记录结构化事件，而不是拼接字符串：

```text
EventId / RootEventId / Depth
ServerTime / Sequence
EventType
SourceNetId / TargetNetId / UnitLifeGeneration
Ability/Modifier/Projectile DefinitionId
Attack/Order/Projectile Handle（仅调试或服务器）
Requested/Mitigated/Absorbed/Applied 数值
FailureTag / Flags
```

使用方式：

- 自动化测试订阅完整服务器流做顺序和 exactly-once 断言。
- UI 订阅本地相关的精简 GameplayMessage/replicated event。
- 调试控制台按 RootEventId 展开一条 Damage/Heal/反伤链。
- 日志级别和采样可配置，Shipping 不默认记录高频完整 payload。

第一版不承诺录像回放，但事件 schema 要带版本并避免 UObject 指针，为后续 replay 留入口。

## 8. 调试与可观测性

最低工具：

- `combat.Debug.Unit <id>`：ASC 初始化、当前 Attribute/Tag、Order、Attack、Modifier 和 Schedule 摘要。
- `combat.Debug.Event <EventId>`：事务阶段、Hook 顺序、Result 和 follow-up。
- `combat.Debug.DrawTargeting/Projectile/Motion/Order`：服务器/客户端使用不同颜色。
- Scheduler 统计：active slots、callbacks/frame、overdue、budget drops、owner top-N。
- Combat 统计：Damage/Heal TPS、active modifiers/projectiles/thinkers、rejected RPC。

开发期为每个 Handle 提供 `ToString()` 和 invalid reason；Shipping 避免暴露敏感对象路径。

### 8.1 UE MCP 诊断入口

UE MCP 是上述调试信息进入 Unreal Editor/PIE 工作流的首选桥梁：

- 读取明确 World/NetMode 下的 Unit、ASC、Attribute、GameplayTag 和组件状态。
- 检查 DataAsset、GameplayEffect、蓝图父类/默认值/引用和编译状态。
- 构造测试地图对象、启动 PIE、触发单一场景并收集 Output Log/Combat Event。
- 将 Editor 中观察到的 Actor/Handle/EventId 与服务器结构化日志关联。

调试接口应输出结构化、稳定、可查询的字段，避免 UE MCP 只能解析人类日志文本。MCP 回读是诊断证据，最终网络结论仍以 Dedicated Server/Client 自动化为准。完整流程见 [13](13-UE-MCP-Workflow.md)。

## 9. 性能预算与压测入口

原设计只有防无限 callback 的 budget，没有系统级容量目标。M7 前必须给目标平台定义场景预算：

- 同屏/服务器最大 Unit、ActiveGE/Runtime、Projectile、Thinker、Aura target 数。
- Scheduler callbacks/frame 和 catch-up 延迟上限。
- Order RPC/sec/connection、Combat events/sec。
- Server frame、网络带宽、Actor/channel 数和内存。

在数字未确定前，代码至少暴露计数器、限额和失败策略。对象池、FastArray 压缩和 relevancy 优化由 profiling 证据触发，不提前改变语义。

## 10. 低预测阶段

第一阶段：

- 指令、施法接受、Projectile 命中、Damage/Modifier 由服务器确认。
- 客户端只预测点击反馈、施法指示器、选中和非命中特效。
- UI 使用服务器时间显示 cast/channel/cooldown，允许平滑但不改变权威状态。

Damage/Modifier/Projectile 全链路通过 Dedicated Server 测试后，再单独评估瞬发技能和普通移动预测。预测设计需要明确 PredictionKey、回滚对象和 Cue reconcile，不作为“打开 GAS 预测开关”处理。

## 11. 最低验收

- Mixed owner、非 owner 和 Minimal AI 的 Attribute/Tag/View 符合矩阵。
- 未拥有 Unit、未授予 Ability、过频/超大/重放 Order RPC 被拒绝。
- 客户端缺失 Definition asset 时 UI 安全降级，服务器不受影响。
- ModifierView 增删/层数/结束时间无重复、无泄露。
- EventId/RootEventId 能串起一次伤害及其 follow-up，Death 只出现一次。
- 预测 Projectile 与服务器实体 reconcile 后只保留一个视觉。
