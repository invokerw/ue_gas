# 10-08 数据、网络、UI 与可观测性

## 1. 数据资产身份

需要被网络、日志、回放或 UI 引用的定义使用 `UPrimaryDataAsset` 和稳定 `FPrimaryAssetId DefinitionId`：

```text
Unit / Ability / Modifier / Projectile / AbilitySet
```

运行时载荷传 DefinitionId，不复制 UObject 指针。AssetManager 负责客户端解析名称、图标、本地化、Cue 和静态数值。

规则：

- PrimaryAssetType 固定为 `CombatUnit`、`CombatAbility`、`CombatModifier`、`CombatProjectile`、`CombatAbilitySet`。
- 每个定义保存显式 lower_snake_case `DefinitionName` 和 `SchemaVersion=1`；Class 决定固定 Type，`GetPrimaryAssetId()` 返回 `(Type, DefinitionName)`。
- DefinitionId 在类型内唯一，不由显示名、UObject 名或磁盘路径推导；移动/重命名 `.uasset` 不改变身份。
- DefinitionId 重命名/删除通过唯一、无环、目标存在的 redirect 或 tombstone 处理，不能静默生成新身份或依赖长期 redirect chain。
- Ability Class 单向引用 AbilityData；DataAsset 不反向引用 Class。
- Cook 前运行完整资产校验，错误阻止打包，警告进入报告。
- 运行时缺失资产用稳定占位显示并记诊断，服务器权威结算不得依赖客户端加载成功。

完整命名正则、ContentVersion 和迁移规则见 [90-01 M0 设计冻结](../90-History/90-01-M0-Design-Freeze.md#72-primaryasset-身份)。当前工程没有 Combat PrimaryAsset，v1 不需要迁移既有内容。

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
Combat.RNG.*
```

核心 C++ Tag 使用 Native Gameplay Tags 集中声明；内容扩展可来自配置，但禁止同义 Tag 并存。Tag 新增/废弃需要说明消费者和兼容策略。DamageType 在一个 Spec 中必须恰好一个。

M0 已冻结 C++ 直接引用的 v1 叶节点，包括四个生命状态、Ability Behavior、TargetTeam、Damage Type/Flag、Data SetByCaller、Combat Event/Failure、Order Failure 和 RNG Domain；清单见 [90-01 M0 设计冻结](../90-History/90-01-M0-Design-Freeze.md#71-native-tag-清单-v1)。核心 Tag 重命名必须同提交添加 redirect，旧名只读兼容一个 ContentSchemaVersion，并由 validator 禁止新资产继续写入。

## 3. ASC 所有权和复制矩阵

所有可玩 Combat Unit 统一采用服务器 AI 控制拓扑：PlayerController 只 Possess 无碰撞 Command Pawn，并通过 owner-only `CommandedUnit` 指挥目标 Unit；`ACombatUnitAIController` 负责 Possess/导航。Unit 的 ASC 仍放在 Unit Character：

```text
OwnerActor = AvatarActor = Unit
```

玩家指挥的 Unit 由服务器 `Unit->SetOwner(CommandingPlayerController)`，让 Mixed replication 和 Unit RPC 找到 owning connection；PlayerController 的 Possession 只属于 Command Pawn。AIController 或 PlayerController 只承载导航与网络控制关系，都不替代 ASC 的 OwnerActor。

| 单位类型 | ASC Mode | 完整 ActiveGE | UI 来源 |
| --- | --- | --- | --- |
| 玩家拥有英雄/单位 | Mixed | owning client | owner 读 ActiveGE；其他读 Combat View |
| 中立/纯服务器 AI | Minimal | 无 owning client | Combat View + Tags/Attributes |
| 调试/自动化小图 | Full | 全客户端 | 仅调试，不作正式配置 |

服务器在 Owner/Controller 设置后 `InitAbilityActorInfo(Unit, Unit)`；客户端在 BeginPlay、OnRep_Owner、OnRep_Controller 后刷新。初始化幂等，Owner/Avatar 变化时解绑旧 delegate。

## 4. 权威边界

移动与 Cast Order 从 owning client RPC、服务器状态机、GAS/Projectile/Damage 结算到客户端 View/表现复制的端到端时序见 [10-09 客户端与服务器交互流程](10-09-Client-Server-Interaction.md)。

- Attribute、必要 GE/Tag 由 ASC 复制。
- Order 由 PlayerController RPC 到服务器执行。
- Combat Unit 只由服务器 AIController 和 CharacterMovement 移动；所有客户端（包括 owning client）都以 SimulatedProxy 消费移动复制，不运行 Unit PathFollowing，也不发送 Combat Unit `ServerMove`。
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

客户端不复制权威 ModifierRuntime UObject。Owner 与非 Owner UI 统一使用 `UCombatUnitViewComponent` 提供的扁平 View：

```cpp
struct FCombatModifierView
{
    FCombatModifierHandle Handle;
    FPrimaryAssetId DefinitionId;
    int32 StackCount;
    double ServerStartTime;
    double ServerEndTime;
    FGameplayTagContainer ControlTags;
    bool bIsDebuff;
    bool bDispellable;
};
```

Modifier View 当前使用 FastArray 增量复制，在 `PostReplicatedReceive` 完整应用本次增删改后通知 UI，避免移除前回调仍读到旧条目。名称/图标/文本由 DefinitionId 本地解析；护盾剩余值等秘密 Runtime 状态只在产品明确需要展示时增加量化字段。

当前 `FCombatUnitView` 投影：

- Unit DefinitionId、TeamId、life generation、完整 LifeState 和 UI 可见状态标签。
- Health/MaxHealth、Mana/MaxMana。
- 当前 cast/channel DefinitionId、ActivationId、阶段时间窗和明确的 `Casting/Channeling` 阶段；保留旧 `bChanneling` 作为“配置为引导技能”的兼容标记。
- 独立 FastArray 中的可见 ModifierView。

`UCombatOverheadWidgetComponent` 负责创建、挂载和转发服务器结果，专用服务器跳过 Widget 创建。`UCombatOverheadWidget` 绑定 View，整理安全展示数据与校准服务器时间进度，通过事件交给 `WBP_CombatOverhead`；控件树、样式、血条缓降和跳字动画由 Widget 蓝图实现。

伤害/治疗跳字只读取服务器 Result，通过携带 `LifeGeneration` 的不可靠多播发送给相关客户端；旧生命载荷直接丢弃。当前独立展示 / View 投影版本为 4（ADR-048），需要服务器/客户端同版本部署；头顶 UI 的事件签名、冻结的核心事件与发布契约保持不变。名称使用本地定义上的 `DisplayNameText`，空值回退稳定 ID。关系颜色通过本地指挥单位的 View 和 TeamSubsystem 计算。具体接线与边界见 [10-11 头顶 UI](10-11-Overhead-Blueprint-UI.md)；View 和 Widget 不可反向成为服务器战斗判定来源。

底部 HUD 使用 `FCombatHUDOwnerView`，在同一 Unit View 组件上以 `COND_OwnerOnly` 复制。服务器每 0.1 秒采样 ASC 的攻击、护甲、魔抗、移速、恢复属性和最多四个直接输入技能（主动技能或可切换 AutoCast 的被动技能），同时投影 AutoCast 开关状态，仅在内容改变时更新快照。该 Tick 只产生展示数据，不推进技能或伤害。冷却保存已提交的结束时间与冻结时长，后续 CDR 变化不重算旧时间窗；客户端按校准服务器时间绘制遮罩和倒计时。客户端请求切换 AutoCast 时由服务器读取当前状态并原子翻转，展示快照不作为 gameplay 输入。

HUD 观察显式 `CommandedUnit`，以单位定义及 `LifeGeneration` 匹配公共 View 与拥有者快照，再按本地 AbilitySpec 句柄匹配 Q/W/E/R 顺序。初始复制未齐时留空，失去拥有权后展示入口屏蔽旧快照；Buff 继续使用公共 FastArray。`ACombatPlayerHUD` 只在本地客户端创建 `WBP_CombatHUD`，专用服务器不创建 UMG。配置与生命周期见 [10-12](10-12-Bottom-HUD-Design.md)。

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

### 7.1 玩家战斗记录投影

`UCombatLogComponent` 作为 PlayerController 的原生默认子对象，只在 Authority 订阅 `UCombatEventSubsystem::OnPresentationRecord`。它将 DamageApplied、HealApplied、技能成功开始/中断/格挡/AutoCast 切换、Modifier 施加/移除及死亡/复活转换成独立 schema 1 的 `FCombatLogEntry`；不展示内部攻击/弹体阶段或零治疗。原有 `FCombatLogRecord` schema 1 和发布契约保持不变（ADR-052）。

Damage/Heal 在真实事务落账后，随 Emit 同步转交 `FCombatLogResourceChange` 中的生命前后值。这个原生临时上下文不参与核心事件序列化，也不反算或采样之后的生命值。已有记录的数值不会被后续恢复、反伤或死亡改写。

出生授予的固有效果可能早于单位“初始化完成”标志，此时显示投影从已赋值、已校验的 UnitData 读取稳定 ID，保证首次效果名称正确；不改变单位初始化或技能授予顺序。

`DefaultGame.ini` 的 CombatModifier/CombatProjectile 扫描同时覆盖 `/Game/Combat/Definitions/...` 和 `/Game/Combat/Demo`，因此本地加载能由记录 ID 找到已有的中文效果名称；缺失内容仍回退稳定 ID。

每连接历史独立保存最多 512 条，超限淘汰最早提交的记录，拒绝重复或倒退 Sequence。`FCombatLogArray` 以 `COND_OwnerOnly` 增量复制；在网络模式下，来源和目标必须同时对该连接网络相关。载荷包含服务器不透明实例 ID、稳定定义 ID、时间、类别、真实 AppliedAmount、可选生命端点及事件时身份标志，不包含 Actor/Runtime/DataAsset 指针。当前“英雄”指玩家指挥的单位；关闭“非英雄”只隐藏双方均非玩家单位的记录。网络相关性不是另行实现战争迷雾，后续若加入迷雾需扩展展示权限契约。

`PostReplicatedReceive` 完整应用增删后通知 UI。显示用有序副本按服务器 Sequence 排序，不能重排 FastArray 底层数组而破坏复制索引。关闭窗口仍接收，Widget 重建继续观察现有历史；PC EndPlay 解绑事件并清空历史，不持久化到磁盘。UI 的名称加载、筛选和 0.1 秒本地显示刷新不推进 gameplay。布局和生命周期见 [10-12 §7](10-12-Bottom-HUD-Design.md#7-战斗记录窗口hud-log-001)。

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

调试接口应输出结构化、稳定、可查询的字段，避免 UE MCP 只能解析人类日志文本。MCP 回读是诊断证据，最终网络结论仍以 Dedicated Server/Client 自动化为准。完整流程见 [30-01](../30-Tooling/30-01-UE-MCP-Workflow.md)。

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
- 客户端只预测点击反馈、路径预览、施法指示器、选中和非命中特效；Combat Unit 的实际路径跟随与位移完全在服务器执行。
- UI 使用服务器时间显示 cast/channel/cooldown，允许平滑但不改变权威状态。

Damage/Modifier/Projectile 全链路通过 Dedicated Server 测试后，再单独评估瞬发技能和普通移动预测。预测设计需要明确 PredictionKey、回滚对象和 Cue reconcile，不作为“打开 GAS 预测开关”处理。

## 11. 最低验收

- Mixed owner、非 owner 和 Minimal AI 的 Attribute/Tag/View 符合矩阵。
- 未拥有 Unit、未授予 Ability、过频/超大/重放 Order RPC 被拒绝。
- 客户端缺失 Definition asset 时 UI 安全降级，服务器不受影响。
- ModifierView 增删/层数/结束时间无重复、无泄露。
- EventId/RootEventId 能串起一次伤害及其 follow-up，Death 只出现一次。
- 预测 Projectile 与服务器实体 reconcile 后只保留一个视觉。
