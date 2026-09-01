# 01 范围、架构与硬约束

## 1. 目标与非目标

本设计参考 `invokerw/dota2_skill` 的战斗语义，面向当前 UE 5.8 工程实现 Dota-like 战斗内核。参考版本为 `085c1c5c2d1658a21f88cf9c01f6142df6830cc5`。

第一版目标：

- Ability：主动、被动、点目标、单位目标、无目标、引导、法球、自动施放、冷却、耗蓝、前摇和表现后摇。
- Modifier：Buff/Debuff、属性聚合、状态、周期 Think、事件 Hook、叠层、驱散和 motion controller。
- Projectile：直线、跟踪、普攻弹体、穿透/首个命中、命中回调和表现资源。
- Combat Pipeline：技能、普攻、DOT、反伤、护盾、魔免、抗性、吸血和治疗使用统一事务。
- Order/Movement：Move/Attack/Cast/Stop 队列，复用 UE NavMesh、AIController、EQS 和避让。
- 脚本层：用 Blueprintable C++ 基类和蓝图事件替代 Lua。
- 网络：服务器权威，第一阶段可低预测，但从第一天保持可复制的数据边界。

第一版明确不做：

- 完整物品、商店、经济、天赋、技能树和英雄选择系统。
- 大规模录像回放、观战、反作弊和高预测网络体验。
- 为临时阻挡高频重建 NavMesh。
- 逐帧完全确定性的跨平台 lockstep 模拟。

这些非目标可在核心链路稳定后进入扩展里程碑，不应阻塞第一条可玩的纵向切片。

## 2. 当前工程基线

截至 2026-09-01，仓库基线为：

- `ue_gas.uproject` 关联 UE 5.8，并启用 GameplayAbilities、StateTree 以及 Editor/MCP 辅助插件；StateTree 仅保留为可选引擎能力，不再被项目源码依赖。
- `Source/ue_gas/ue_gas.Build.cs` 已接入 GameplayAbilities、GameplayTags、GameplayTasks、导航、网络、Niagara 和 UMG/Slate 等运行时依赖。
- Combat 已在 `Source/ue_gas/Combat` 落地，包含 ASC、AttributeSet、Ability、Modifier、Damage/Heal、Order、Attack、Projectile、Thinker、Aura、Motion、网络 View、UI、调试、资产校验和 Automation。
- 当前仍保持单 Runtime Module；`ue_gasEditor`、`ue_gasServer`、`ue_gasClient` Target 均存在。Server/Client Target 的源码引擎要求见 [15 M1 环境决策](15-M1-Environment-Decision.md)。
- `/Game/Combat/Demo/Maps/L_CombatDemo` 提供远程攻击可玩 Demo；`/Game/Combat/Tests/L_CombatTest` 用于 PIE、Dedicated 和容量验证。
- `Variant_Strategy` 与 `Variant_TwinStick` 模板源码、资产和关卡已移除；可玩与验证入口统一位于 `/Game/Combat/Demo` 和 `/Game/Combat/Tests`。
- `.codex/config.toml` 配置本地 `unreal-mcp` endpoint，Editor/Content/PIE 操作遵循“读取—修改—回读—测试”闭环。
- M0-M8 已完成验收，核心发布契约为 `combat_v1_rc1`。最近一次完整发布证据见 [33 M8 验收记录](33-M8-Acceptance.md)；该历史记录不自动证明后续工作区修改已回归。

### 2.1 UE MCP 开发基线

涉及 Unreal Editor 实际状态的任务优先使用 UE MCP：检查插件/World/Actor/Component，创建或验证 DataAsset、GameplayEffect、蓝图和测试地图，驱动 PIE 并回读日志。这样可以避免只看 C++ 源码而遗漏 Content 资产、蓝图默认值和 Editor World 配置。

UE MCP 是效率与准确性工具，不是新的权威数据源：

- 核心 C++ 语义仍通过源码、编译和 Automation 验证。
- UE MCP 修改资产后必须回读目标、编译蓝图/保存资产并运行对应测试。
- 多人正确性以 Dedicated Server/Client 测试为准，Network PIE/MCP 用于快速构造和诊断。
- 每次会话先发现实际可用工具，不在设计或实现中硬编码假定的 MCP 工具名。
- 完整操作闭环见 [13 UE MCP 开发工作流](13-UE-MCP-Workflow.md)。

## 3. 参考语义到 UE/GAS 的映射

| 参考层 | 核心职责 | UE/GAS 映射 |
| --- | --- | --- |
| `core` | World、Unit、事件、指令队列、普攻、tick 时序 | `UWorld`、Character、Subsystem、Order/Attack Component |
| `ability` | Ability 生命周期、行为、数据、Lua 技能 | `UGameplayAbility`、PrimaryDataAsset、蓝图子类 |
| `modifier` | 属性、状态、事件 Hook、motion | ActiveGE + GameplayTag + Modifier Runtime |
| `projectile` | 直线/跟踪弹体、命中/结束 | Projectile Actor/Subsystem、AbilityTask、GameplayCue |
| `combat` | 伤害/治疗管线 | Combat Subsystem、Calculator、Instant GE、Result |
| `pathfinding` | A*、ShapeCast、动态圆碰撞 | NavMesh、AI MoveTo、RVO/Detour、EQS |
| `script` | Lua 绑定 | BlueprintNativeEvent/BlueprintImplementableEvent |
| `log/replay` | 事件记录与回放 | Combat Log、GameplayMessage、调试工具 |

保留的是语义，不照搬实现：完整施法阶段、Modifier 的属性/状态/Hook 三类职责、AttackRecord 与无副作用法球仲裁、投射物和结算分离、数据与差异逻辑分离。

## 4. 当前目录和模块依赖

```text
Source/ue_gas/Combat
  Ability/
  Attack/
  Attributes/
  Aura/
  Combat/
  Core/
  Data/
  Debug/
  Demo/
  Log/
  Modifiers/
  Motion/
  Network/
  Order/
  Performance/
  Projectile/
  Release/
  Scheduling/
  Targeting/
  Tests/
  Thinker/
  UI/
  Unit/
  Validation/
  View/
```

`ue_gas.Build.cs` 已包含 GAS 的三项基础依赖：

```csharp
"GameplayAbilities",
"GameplayTags",
"GameplayTasks"
```

`ue_gas.uproject` 已启用 GameplayAbilities；运行时同时使用 `AIModule`、`NavigationSystem`、`StateTree`、`GameplayStateTree`、`Niagara`、`UMG`、`Slate` 和 `SlateCore`。

第一版保持单 Runtime Module；只有当编译时间、依赖方向或独立自动化测试确实受阻时再拆 Combat Runtime/Developer 模块。

## 5. 运行时核心对象

| 类/组件 | 建议名称 | 单一职责 |
| --- | --- | --- |
| 战斗单位 | `ACombatUnitCharacter` | `IAbilitySystemInterface`、ASC、属性、队伍、攻击和指令组件 |
| ASC | `UCombatAbilitySystemComponent` | GAS 激活、标签查询、冷却/消耗查询和 ActorInfo 初始化 |
| 属性集 | `UCombatAttributeSet` | Health/Mana、战斗属性和 IncomingDamage/Healing Meta Attribute |
| 指令队列 | `UCombatOrderComponent` | Move/Attack/Cast/Stop 的当前项、FIFO 和 generation |
| 普攻组件 | `UCombatAttackComponent` | AttackRecord registry、前摇、冷却、近战/远程完成 |
| Modifier | `UCombatModifierComponent` | ActiveGE 与 Runtime 对应、Hook、驱散、周期调度 |
| 强制位移 | `UCombatMotionComponent` | 水平/垂直通道、抢占、碰撞、移动恢复 |
| 调度 | `UCombatSchedulerSubsystem` | 服务器逻辑时间、一次性/周期任务、稳定顺序和 catch-up |
| 投射物 | `UCombatProjectileSubsystem` | Handle、生成、命中/结束幂等、长期上下文 |
| 伤害/治疗 | `UCombatDamageSubsystem` / `UCombatHealSubsystem` | 权威事务编排、Hook、应用最终 GE、Result |
| 战斗事件 | `UCombatEventSubsystem` | 统一语义事件、日志投影和调试订阅 |
| 目标规则 | `UCombatTargetingSubsystem` / `UCombatTeamSubsystem` | 队伍关系、目标状态、距离、视线和位置校验 |

组件之间只通过明确 API、Handle、Result 和不可变快照协作；异步对象不得持有会随 Ability 结束而失效的裸上下文。

## 6. 数据层

| 定义 | 建议名称 | 主要内容 |
| --- | --- | --- |
| 单位 | `UCombatUnitData` | 初始属性、队伍、碰撞半径、普攻参数、AbilitySet |
| Ability | `UCombatAbilityData` | 行为标签、目标规则、阶段时长、消耗、距离、special、actions |
| Modifier | `UCombatModifierData` | GE/Runtime 类、优先级、叠层、驱散、状态、Think、motion |
| Projectile | `UCombatProjectileData` | 类型、速度、宽度、距离、碰撞、命中策略、Cue |
| AbilitySet | `UCombatAbilitySet` | Ability 类和初始等级列表，不保存运行时 SpecHandle |
| Damage/Heal | `FCombatDamageRequest` / `FCombatHealRequest` | 请求参数和来源上下文；只在服务器事务中使用 |
| 来源身份 | `FCombatSourceContext` | DirectSourceType，以及 Ability/Modifier/Projectile DefinitionId 因果链 |
| Ability Action | `FCombatAbilityAction` | Damage、Heal、ApplyModifier、Projectile、Thinker、Event |

`ability_special` 用 key 到等级数组表达：

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

规则：

- Ability 类是逻辑入口并单向引用 `UCombatAbilityData`；DataAsset 不反向引用 Ability 类。
- Unit/AbilitySet 保存 Ability 类列表，授予后以 `FGameplayAbilitySpec.Level` 作为权威等级。
- 会进入复制、日志或回放的定义继承 `UPrimaryDataAsset`，使用稳定 `FPrimaryAssetId DefinitionId`。
- 网络载荷传 DefinitionId，不复制 DataAsset UObject 指针；客户端通过 AssetManager/DataRegistry 解析显示数据。
- Damage/Heal/Modifier/Projectile 使用同一个 `FCombatSourceContext` 传递来源身份；不得为日志、Hook 和网络各维护一套来源字段。
- 编辑器校验必须拒绝重复/缺失 DefinitionId、空等级数组、非法范围、非法周期和硬引用环。

更完整的数据与版本规则见 [08](08-Data-Network-Observability.md)。

## 7. 第一版硬约束

M0 冻结的跨系统值域和默认值集中在 [14 M0 设计冻结](14-M0-Design-Freeze.md)：TeamId/关系、LifeState/LifeGeneration、Ability Class/Data/Spec 身份、Numeric/RNG v1、Collision/LOS/cm、Native Tag 和 PrimaryAsset identity。下列硬约束的公共字段必须遵循该契约；变更时按 ADR、schema/version 和迁移规则处理。

| 问题 | 唯一规则 |
| --- | --- |
| 属性最终值 | ASC Attribute/ActiveGE 聚合结果是唯一来源；Runtime 不维护第二套 Armor/MoveSpeed 等最终值。 |
| Health/Mana | Damage/Heal/Resource API 是编排入口；最终修改只通过 GE 或 AttributeSet Meta Attribute。 |
| 事件 Hook | Runtime 只做响应、实例状态和副作用，不绕过统一属性路径。 |
| 服务器入口 | Damage、Heal、ApplyModifier、Attack Finalize、Order 执行只允许服务器调用。 |
| Damage 载荷 | Final Amount 用 SetByCaller，类型/flags 用 DynamicAssetTags，身份链用自定义 EffectContext。 |
| 事件结果 | Post Hook、吸血、反伤、日志和死亡只读取实际 Result，不从请求值反推。 |
| Hook 顺序 | `Priority descending -> ApplySequence ascending`；结构修改延迟到当前阶段结束。 |
| 异步身份 | Attack、Order、Projectile、Schedule、EQS、AI Move 使用带 generation/sequence 的稳定 Handle。 |
| 周期逻辑 | Channel、Modifier Think、DOT/HOT、attack-ready、追击和 thinker pulse 只走 Combat Scheduler。 |
| 连续运动 | Projectile、CharacterMovement、RootMotionSource 可逐帧更新，但不得顺便结算周期 Damage/Heal。 |
| Order 释放 | 只等待 `OrderReleased`、`AbilityChannelEnded` 或失败/中断，不等待 cooldown 或纯表现 backswing。 |
| 蓝图边界 | 数值来自 DataAsset，公共校验/权限/生命周期在 C++，蓝图只实现技能差异。 |
| Editor/资产操作 | 优先通过 UE MCP 读取真实状态和执行受控修改；每次写入必须回读，并以编译/Automation/Gate 作为完成标准。 |
| 代码注释 | 项目自有的新建或实质修改代码必须按 §10 添加中文注释；缺少类、函数或关键语义注释时不得通过对应 Gate。 |

如果需要打破任一约束，先在 [12](12-Decisions-Gaps.md) 增加决策记录、替代路径和迁移影响。

## 8. 生命周期所有权

| 对象 | 权威所有者 | 创建 | 必须终结于 |
| --- | --- | --- | --- |
| Ability 激活 | ASC/Ability instance | 激活成功 | `EndAbility`，统一解绑 task/delegate |
| Modifier Runtime | ModifierComponent | 合法 ActiveGE 添加 | GE 移除/过期、Component EndPlay |
| AttackRecord | AttackComponent registry | 攻击前摇开始 | Landed/Failed 后一次性销毁 |
| Projectile | ProjectileSubsystem | 权威 spawn | Hit/timeout/fizzle/EndPlay 的幂等 Finish |
| Order | OrderComponent | IssueOrder | 完成、失败、Stop 或新 generation |
| Schedule | Scheduler slot | Schedule | Cancel、Owner 销毁或 World teardown |
| Combat Event | Damage/Heal/Event Subsystem | 事务入口 | Result、follow-up 入队、日志提交 |

任何路径都要能回答“谁创建、谁持有、谁取消、过期回调如何失效”。

## 9. 落地原则

1. 先做统一属性、Damage/Heal 和 Modifier，再做复杂技能。
2. 使用 GAS 擅长的属性、标签、持续时间、叠层、冷却、消耗和 GameplayCue。
3. 用自定义层补 AttackRecord、Modifier Hook、Order Queue、Scheduler 和 Projectile callback。
4. 示例玩法只通过 Combat 公共入口组合，不能保留第二套 gameplay 权威或结算旁路。
5. 先完成单机/服务器权威纵向切片，再扩展复制 UI 和预测。
6. 每个里程碑必须通过对应 Gate，不能用示例蓝图“看起来能工作”替代自动化验收。
7. 资产、蓝图、关卡和 PIE 操作使用 UE MCP 建立“读取—修改—回读—测试”闭环，减少手工配置漂移。
8. 生成项目代码时同步编写中文注释，不把注释补写留到里程碑验收阶段。

## 10. 生成代码中文注释规范

本规范适用于 `Source/` 下项目自有的新建代码，以及对既有项目代码的实质修改；不要求修改 Unreal Engine、第三方库或未触及的模板源码。注释使用 UTF-8，中文为主，类名、API 名、GameplayTag 和 Unreal/GAS 术语保留英文。

### 10.1 必须注释的对象

- 每个新建的类、结构体、枚举和委托：在声明前用中文说明职责、使用边界；涉及网络权威或生命周期时同时说明 owner、创建与终止条件。
- 每个函数：在声明处用中文说明行为；包括 `public`、`protected`、`private`、静态函数、回调和 `override`。参数、返回值、失败条件或副作用不直观时必须一并说明。
- 每个枚举值，以及影响复制、保存、时序、权限、Handle generation 或数值语义的成员字段：说明取值含义和约束。
- 对外暴露的 `UCLASS`、`USTRUCT`、`UFUNCTION`、`UPROPERTY`：注释必须能让 C++/蓝图调用者理解用途；Editor 中需要展示说明时补充中文 `ToolTip` metadata。
- 复杂分支、公式、稳定排序、deferred/reentry、网络降级和兼容逻辑：在实现处说明“为什么这样做”和关键不变量，不只复述代码表面动作。

### 10.2 写法与维护

- 声明处优先使用 `/** ... */` 文档注释；实现内部使用 `// ...` 解释局部决策。
- 函数声明和定义不机械重复同一段注释；公共契约写在头文件，`.cpp` 只补充实现原因和边界。
- 简单 getter、setter、构造函数和 Unreal 回调也需要简短中文职责说明，不以“名称已经清楚”为由省略。
- 修改行为时同步更新相邻注释；与实现不一致的旧注释视为缺陷，不能保留。
- 注释不得代替可执行测试，也不得使用 `TODO` 掩盖未完成的权限、生命周期或失败处理。

示例：

```cpp
/**
 * 战斗调度器负责按 World Game Time 执行服务器权威的离散战斗任务。
 * World teardown 时会使全部未完成 Handle 失效。
 */
UCLASS()
class UCombatSchedulerSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()

public:
    /**
     * 注册一次调度任务并返回带 generation 的稳定句柄。
     * @return 注册失败时返回无效句柄，不会执行回调。
     */
    FCombatScheduleHandle Schedule(const FCombatScheduleRequest& Request);
};
```
