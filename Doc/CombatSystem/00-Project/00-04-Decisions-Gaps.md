# 00-04 决策与缺口登记

## 1. 用法

本文件区分三种状态：

- `已定`：第一版硬约束，修改需要决策记录和测试迁移。
- `已关闭`：原 Gap 已由已定结论、owner、迁移和测试落点关闭；重新打开必须新增 ADR。
- `建议基线`：为避免实现停摆给出的默认方案，必须在“最迟节点”前接受或替换。
- `开放`：缺少产品/技术结论，不能悄悄由某个子系统自行决定。

关闭 Gap 时必须写：结论、影响文档、代码 owner、测试和兼容/迁移。只有“已有想法”不算关闭。

## 2. 已定架构决策

| ID | 状态 | 决策 | 原因/影响 |
| --- | --- | --- | --- |
| ADR-001 | 已定 | ASC Attribute/ActiveGE 是最终属性唯一来源 | 防止 GE 与 Runtime 双聚合 |
| ADR-002 | 已定 | Damage/Heal Subsystem 是 Health 修改唯一编排入口 | 统一 Hook、实际 Result 和死亡 |
| ADR-003 | 已定 | Modifier 使用 ActiveGE + Runtime 双层 | GAS 表达属性/Tag，Runtime 表达有状态 Hook |
| ADR-004 | 已定 | Hook 按 Priority desc、ApplySequence asc | 同一输入产生稳定结果 |
| ADR-005 | 已定 | Hook 内结构修改 deferred | 防止重入和遍历失效 |
| ADR-006 | 已定 | 周期 gameplay 只走服务器 Scheduler | 防 Timer 漂移和不确定顺序 |
| ADR-007 | 已定 | 连续运动逐帧；不在 Motion/Projectile Tick 结算周期 Damage | 时序职责分离 |
| ADR-008 | 已定 | Attack/Order/Projectile/Schedule 使用稳定 Handle/generation | 过期异步回调失效 |
| ADR-009 | 已定 | Projectile 持快照，不依赖 AbilityTask 生命周期 | fire-and-forget 可安全结算 |
| ADR-010 | 已定 | Order 只等待 OrderReleased/ChannelEnded/失败，不等 cooldown/backswing | 保留 Dota 指令语义 |
| ADR-011 | 已定 | 服务器权威，第一版低预测 | 先稳定完整结算链 |
| ADR-012 | 已定 | 网络/日志引用 PrimaryAssetId，不复制 DataAsset UObject 指针 | 稳定身份和客户端本地解析 |
| ADR-013 | 已定 | ModifierRuntime 只在服务器权威运行 | 客户端通过 GE/Tag/Cue/View 投影 |
| ADR-014 | 已定 | Ability 默认 InstancedPerActor，同实例不重入 | 隔离蓝图/TargetData/提交状态 |
| ADR-015 | 已定 | UWorld game time 驱动 Combat Scheduler | pause/global dilation 一致作用于战斗 |
| ADR-016 | 已定 | Editor/Content/Blueprint/PIE 任务优先使用 UE MCP 读取、受控修改和回读 | 提高资产操作效率与准确性；编译/Automation/Dedicated 仍是最终 Gate |
| ADR-017 | 已定 | Combat 使用独立 FCombatTeamId + TeamSubsystem Relation API；控制权与队伍分离 | 关闭 GAP-001，避免 AI/Ability/Order/Projectile 散落阵营比较 |
| ADR-018 | 已定 | Unit 使用 Alive/Dying/Dead/Respawning；Respawn 递增 uint32 LifeGeneration | 关闭 GAP-002，Death exactly-once 且旧生命回调失效 |
| ADR-019 | 已定 | Ability Class 单向引用 Data，Spec.Level 权威，每 Unit/DefinitionId 一个 Spec | 关闭 GAP-003，消除 Class/Data/Spec 身份环和等级双来源 |
| ADR-020 | 已定 | Numeric Policy/Formula v1 集中边界且不中途取整；Combat RNG v1 使用 keyed roll | 关闭 GAP-006/GAP-007，可解释并重放数值与随机结果 |
| ADR-021 | 已定 | 运行时单位为 cm；固定 Combat Channel/Profile、XY edge range、5 cm tolerance 和统一 LOS | 关闭 GAP-009，Target/Order/Projectile 共享几何语义 |
| ADR-022 | 已定 | 核心语义使用 Native Tag v1；定义资产使用固定 Combat PrimaryAssetType + 显式 lower_snake DefinitionName | 关闭 GAP-004，身份不随路径漂移且可在 cook 阻止冲突 |
| ADR-023 | 已定 | M0 冻结包是 M1 公共字段输入，修改需显式 supersede/schema migration | 防止实现阶段重新引入 `TODO decide later` |
| ADR-024 | 已定 | Health/Mana regen 使用 0.25 s Scheduler Coalesce；Health 走 HealSubsystem，非 Alive 暂停且不补结 | 关闭 GAP-012，恢复量不随帧率漂移 |
| ADR-025 | 已定 | Death/Respawn 保留 AbilitySpec、cooldown、AutoCast 和非死亡移除 Modifier；Mana 复活至 Max；M2 只记录奖励归属事件 | 关闭 GAP-013，冻结最小生命状态机产品语义 |
| ADR-026 | 已定 | 状态抗性只缩短显式可抵抗 Debuff Duration；Think interval 不变；边界 tick 使用缩短后的 ExpireAt | 关闭 GAP-005，冻结周期与 Refresh 边界 |
| ADR-027 | 已定 | M3 VisibilityPolicy 固定为 None；LOS 可显式开启；客户端只提交 Actor/位置且命中列表始终由服务器重算 | 关闭 GAP-008，保留后续 VisionProvider 扩展点 |
| ADR-028 | 已定 | Cast/Channel gameplay 时间只由 Combat Scheduler 驱动；Montage/Notify 仅作可校准表现，不能触发唯一结算 | 关闭 GAP-011，统一中断和清理语义 |
| ADR-029 | 已定 | ManaCost 与 CDR 在各自 commit point 快照；已开始 cooldown 不因后续 CDR 改变而重排 | 关闭 GAP-024，冻结分阶段提交语义 |
| ADR-030 | 已定 | M4 AttackTiming v1 使用 IAS 20..700、BAT 等比缩放、0.20..10.00 s interval，并以 Scheduler 绝对时间决定 attack point/ready | 关闭 GAP-010；Montage/Notify 只投影表现 |
| ADR-031 | 已定 | Order 的 EQS/Move/Ability/Attack 回调必须同时匹配 OrderHandle、具体请求句柄与 Unit life generation | replace/Stop/Respawn 后旧异步结果不能推进新 Order |
| ADR-032 | 已定 | 法球按稳定 Modifier 顺序执行无副作用 CanClaim，再按 exclusive group 提交 winner 并快照 OnHit | 未胜出不扣资源，提交失败可降级且旧 Record 不被升级重解释 |
| ADR-033 | 已定 | Projectile 由 WorldSubsystem registry 持有，权威 Actor 只做连续 substep sweep 与位置复制，全部结束路径汇入幂等 Finish | fire-and-forget 不依赖 Ability 实例，穿透/高速/teardown 共用同一生命周期 |
| ADR-034 | 已定 | Tracking 对旧生命、Dead/Dying/Respawning、Untargetable、OutOfGame 按 Data 选择 Fizzle 或 LastKnown；Invulnerable 留到 impact 判定 | 关闭 GAP-023，避免已有弹体对目标状态各自猜测 |
| ADR-035 | 已定 | MotionComponent 分离 Horizontal/Vertical 独占通道，严格高优先级抢占；Thinker gameplay 时间只走 Scheduler | 保证中断 exactly-once、Order 恢复和 AoE 时序单一权威 |
| ADR-036 | 已定 | Aura 使用每 World registry + Scheduler Coalesce + Targeting 查询，对普通 child Modifier 做 `Target -> Handle/LifeGeneration` 幂等 reconcile | 关闭 GAP-014；统一进入/离开/换队/死亡/Break/EndPlay 清理，不引入 Actor Tick 或蓝图阵营旁路 |
| ADR-037 | 已定 | SpellBlock、Break、Debuff Immunity、Dispel Immunity 使用独立 Tag，并分别落在 Ability commit 后、Runtime Hook、Modifier Apply、Dispel 阶段 | 关闭 GAP-016；防止一个 MagicImmune Tag 混淆资源提交、既有状态与移除语义 |
| ADR-038 | 已定 | 玩家拥有 Unit 使用 Mixed、纯服务器 AI 使用 Minimal；Order 批量 RPC 由 Unit owning connection 承载，并执行 ownership、RequestId、载荷、token bucket 与重放窗口复核 | 关闭 GAP-021；AIController 继续负责导航，PlayerController Owner 只建立网络归属，客户端无 Damage/Modifier/Finish 写入口 |
| ADR-039 | 已定 | Combat Event schema v1 使用无 Runtime UObject 指针的稳定字段、World 环形缓冲与 RootEvent 展开；第一版不承诺录像或确定性 replay | 关闭 GAP-019；不兼容字段变更必须提升 schema 并提供离线迁移，Shipping 不默认保留完整高频 payload |
| ADR-040 | 已定 | M7 容量基线为 30 Hz Dedicated：64 Unit、256 Modifier、128 Projectile、32 Thinker、16 Aura/256 child，Server frame p95 <= 33.34 ms、p99 <= 50 ms，单连接发送 <= 256 KiB/s | 关闭 GAP-018；正确性与性能 Gate 分离，只有 profiler 证据指向具体 owner 后才引入 pooling、relevancy 或紧凑序列化 |
| ADR-041 | 已定 | `combat_v1_rc1` 使用机器可读发布契约冻结 Content/Formula/RNG/Event schema v1；gameplay 保持服务器权威，Projectile 只允许视觉预测；完整回滚、重放、召唤/幻象和物品经济明确延期到 post-v1 | 关闭 G8 的未知功能边界；契约漂移由 M8 Automation 和 Dedicated 日志阻止，完整边界见 [90-15](../90-History/90-15-M8-Release-Candidate-Decision.md) |
| ADR-042 | 已定 | M8 不做无 profiler owner 的推测性 pooling/relevancy/批处理优化 | M7 容量实测远低于预算；保留 Handle generation、稳定顺序和 exactly-once，未来超预算时按具体 owner 独立优化与回归 |
| ADR-043 | 已定 | 可玩 Combat Unit 统一由服务器 `ACombatUnitAIController` Possess 和移动；`PlayerController` 只 Possess Command Pawn、通过 Unit Owner 建立指挥连接；Combat Unit 在所有客户端均为 SimulatedProxy | 消除 owning client PathFollowing 与本地 Pawn 解穿透造成的非权威位移；保留 Order RPC、ASC Mixed、服务器 Capsule 硬阻挡，并只使用一种服务器 Crowd 避让；实施与 Gate 见 [10-10](../10-Architecture/10-10-Server-Authoritative-Movement-Kickoff.md) |
| ADR-044 | 已定 | Order 的施法与普攻朝向准备复用 `CharacterMovement.RotationRate.Yaw`，由移动组件逐帧旋转；两者共用 UnitData 起手容差，默认 15°，达到容差后才开始前摇，无目标施法不转身 | 修正 `Facing` 直接设置最终朝向的实现；连续旋转只在服务器执行，Scheduler 负责复核、超时与推进队列。按用户反馈统一起手容差，复用 `AttackFacingToleranceDegrees` 并保留字段名、既有 RPC、Ability/Attack 事件顺序及数据 schema，无新增转速/容差副本或资产迁移；规则和验证见 [10-07](../10-Architecture/10-07-Order-Movement.md) |
| ADR-045 | 已定 | 头顶 UI 使用 C++ 只读展示适配基类与 Widget Blueprint；布局、样式、血量拖影和跳字动画在蓝图，复制、状态归并、服务器时间与绑定生命周期在 C++ | 头顶展示接口及 View 投影进入 v2：显式区分前摇/引导，跳字携带目标生命代次，FastArray 完整接收后通知；保留伤害/治疗公开签名及核心 `combat_v1_rc1`。新增显示名称字段有安全空值，不改变 DefinitionId 或内容 schema；迁移与验证见 [10-11](../10-Architecture/10-11-Overhead-Blueprint-UI.md) |

### ADR-046：Demo 普攻输入统一使用 Enhanced Input（2026-09-08）

按用户反馈修正直接 `BindKey` 的实现：选敌、确认、取消与停止分别使用 Input Action，在 `IMC_Default` 中配置默认 A / 左键 / Escape / S。Controller 只绑定 Action 的 `Started` 事件，键位由映射资产管理，不保留硬编码 Key 兜底。

兼容新增四个可编辑 `UInputAction` 引用，空值表示禁用对应输入。迁移 `BP_CombatDemoPlayerController` 的默认引用及 Demo 输入映射；移动、触摸、Q/W/E/R 的既有引用和映射保持不变。该改动只调整本地输入配置，不改变公开蓝图函数、RPC 载荷、DefinitionId、战斗结算或 `combat_v1_rc1` 的版本。验证覆盖资产配置、Action 事件绑定、原有普攻/取消行为、蓝图编译保存回读及资产校验。

### ADR-047：底部 HUD 的拥有者投影与蓝图视觉（2026-09-09）

- 状态：已定；用户已确认 [10-12](../10-Architecture/10-12-Bottom-HUD-Design.md) 并授权开始实现。属于展示能力兼容新增，核心 `combat_v1_rc1`、战斗事件和内容版本不变。
- `UCombatUnitViewComponent` 的独立展示 schema 升至 v3，增加仅向拥有者复制的 HUD 快照：稳定技能定义与 Spec 句柄、等级、费用、已提交冷却的绝对结束时间和原始时长、英雄聚合属性。原 Unit View / Modifier FastArray 及头顶蓝图接口保持兼容；新网络字段要求同版本客户端和服务器。
- 服务器以 0.1 秒间隔采样只读 HUD 数据，仅有主控连接的单位构造快照，仅在数据变化时发布；不执行技能、资源修改或周期结算。冷却时间窗直接读取 ASC 已提交结果，不根据当前 CDR 反算旧冷却。
- HUD 由本地玩家的 HUD Actor 持有，以显式 CommandedUnit 为观察目标。Widget Blueprint 保存控件树与样式，C++ 负责快照、可选控件绑定、时间换算和清理；不修改现有头顶 UI 的事件接口。
- 等级、经验及六格物品 / 三格背包为明确占位，不新增成长、库存或经济权威。技能槽只提供信息与详情，不新增施法请求。
- 验证覆盖快照与属性来源、技能顺序及冻结冷却、初始复制 / 换单位 / 换生命 / teardown、蓝图绑定与几何、Editor / Server / Client 构建、资产校验和真实联机 HUD 投影。

### ADR-048：可切换 AutoCast 被动进入技能槽（2026-09-10）

- 状态：已定；为 DEMO-901 的霜冻之箭补齐可见和输入语义。核心 `combat_v1_rc1`、GameplayTag、内容与战斗事件 schema 保持不变；独立展示投影从 schema 3 升至 4，同版本服务器和客户端部署。
- Q/W/E/R 最多选择四个“可直接输入技能”：普通非被动技能按原规则创建 Cast Order，`Passive + AutoCast` 技能占槽但快捷键只调用 ASC 的可靠 Toggle RPC；纯被动继续隐藏。
- Toggle 请求不携带客户端推测的目标状态。服务器按当前 per-Spec AutoCast 值原子翻转，并复核网络所有权、Spec、行为标签和单位生命状态，避免连续按键依赖延迟到达的 HUD 数据。
- `FCombatHUDAbilityView` owner-only 投影新增“使用 AutoCast 切换输入”和“AutoCast 已开启”字段；HUD 显示“自动/关闭”。展示快照仍只读，不参与服务器法球仲裁或伤害结算。
- 本决策仅覆盖键盘技能槽；鼠标点击槽位仍只固定详情，不新增点击施法或切换。它 supersede ADR-047 中“技能槽只提供信息、不新增请求”对键盘槽输入的限制，其余 HUD 边界不变。
- 验证覆盖被动 AutoCast 槽位选择、连续 Q 翻转、无 Cast Order、副本投影更新、三 Target 和 Dedicated owner-only 回显。

### ADR-049：经验等级与技能点成长兼容扩展（2026-09-11）

- 状态：已定；由 `PROG-001` 落地，属于 post-v1 兼容扩展，不修改 `combat_v1_rc1` 的结算公式、网络发布契约或物品边界。
- 选择：Unit 新增服务器权威 `UCombatProgressionComponent`，使用累计阈值 `XP(n)=100*(n-1)*(n+2)/2`，默认上限 30 级；每次跨级增加一个技能点，死亡和复活保留成长状态。
- 奖励：致死伤害完成 Lifecycle `RequestDeath` 后，目标 `UCombatUnitData.ExperienceReward` 只发给实际致死来源；经验共享、助攻、范围衰减和天赋树留待后续决策。
- 加点：技能升级请求由 owning client 通过可靠 RPC 提交，服务器检查存活、技能点、英雄等级和 `AbilityData.MaxLevel`，并复用 `UCombatAbilitySystemComponent::SetCombatAbilityLevel`；HUD 的 `+` 按钮只是请求入口。
- 展示：`FCombatHUDOwnerView` 增加等级、累计经验、等级内经验、升级所需经验、经验进度、技能点和技能可升级标志，展示 schema 从 4 升至 5；旧 HUD 新增绑定均为可选，旧技能槽动态创建 `+` 按钮。
- 迁移：旧 UnitData 使用初始 1 级、0 等级内经验、100 击杀奖励默认值；旧资产无需脚本迁移。Dedicated/真实 PIE 与最终按钮美术仍需后续验收。

### ADR-050：模板 C++ 文件与原生类统一 Combat 前缀（2026-09-12；模块部分由 ADR-051 取代）

- 状态：已定；用户要求将 ue_gas 开头的 h/cpp 改为 Combat，实施与验证见 [REF-001](../Specs/REF-001-combat-source-prefix.spec.md)。
- 选择：4 组文件改为 `Combat`、`CombatCharacter`、`CombatGameMode`、`CombatPlayerController`；反射类采用 `ACombatCharacter`、`ACombatGameMode`、`ACombatPlayerController`。保留 `ue_gas` Module/Target、`UE_GAS_API` 和 `/Script/ue_gas` 包身份。
- 迁移 v1：`DefaultEngine.ini` 的 Core ClassRedirects 将三个旧 `/Script/ue_gas.ue_gas*` 类名分别映射到新类；原 TopDown 模板重定向直接指向新类，旧蓝图通过加载时重定向兼容。源码调用方同步更新 include 和类型；无需批量重存二进制资产。
- 日志：基础日志改为 `LogCombatGame`，避免与战斗事件已有的 `LogCombat` 重复定义。gameplay、公开函数参数、复制策略和事件 schema 不变，核心 `combat_v1_rc1` 及相关数据版本保持原值。
- 验证与回滚：Editor 构建、现有 Combat Automation 和资产校验检查引用与蓝图父类。回滚时成组恢复源码命名及配置，资产保持原存储格式。

### ADR-051：Runtime Module 迁移为 Combat（2026-09-12）

- 状态：已定；用户授权将 Runtime Module 从 `ue_gas` 改为 `Combat`，实施与验证见 [REF-002](../Specs/REF-002-combat-runtime-module.spec.md)。
- 选择：Module、Build.cs、源码根目录、`COMBAT_API`、`/Script/Combat` 和 Target 的 `ExtraModuleNames` 使用 Combat；`ue_gas.uproject` 文件名、四个 `ue_gas*Target.cs` 文件/类名以及 `ue_gasEditor`、`ue_gasServer`、`ue_gasClient` 构建命令保持不变。
- 迁移：`DefaultEngine.ini` 的 `[CoreRedirects]` 增加 `/Script/ue_gas` 到 `/Script/Combat` 的精确 `PackageRedirects`，三个既有原生类重定向直接指向新模块；配置类段、AssetManager 和 AbilitySystemGlobals 使用新包路径。资产校验同时识别新旧模块类路径，避免旧 Asset Registry 元数据使扫描静默变为 0；无需批量重存二进制资产。
- 影响：模块 DLL、UHT 生成包和 API 导出宏改变；Gameplay、网络、DefinitionId、GameplayTag、事件 schema 与 `combat_v1_rc1` 不变。Dedicated/Server/Client Target 若被安装版 SDK 阻塞，必须记录未执行原因。

### ADR-052：玩家战斗记录的独立只读投影（2026-09-12）

- 状态：已定；用户授权参考 DOTA2 战斗日志，实施规格见 [HUD-LOG-001](../Specs/HUD-LOG-001-combat-log.spec.md)。
- 核心 `FCombatLogRecord`/Event schema 1 与 `combat_v1_rc1` 不变。`Emit` 的原生可选展示上下文同步传递事务的真实生命前后值，不写入核心序列化，也不从事后 Health 反算。
- 新增独立 `CombatLogPresentation` schema 1。PlayerController 持有服务器订阅的日志组件，按网络相关性筛选后以 owner-only FastArray 保留最近 512 条；不复制 Runtime/DataAsset UObject 指针，不提供客户端写日志或写战斗的 RPC。
- Widget Blueprint 维护左上角入口、窗口和筛选布局；C++ 只整理文字、时间窗、定义解析、颜色标记及订阅生命周期。关闭窗口继续记录；历史跨死亡保留。
- 当前英雄判别仅指事件发生时有玩家指挥的单位。“非英雄”关闭时排除双方均非玩家单位的记录；物品选项置灰，仍不扩展物品/经济玩法。
- 新客户端和服务器同时更新，无存档迁移；回滚成组撤销日志组件、展示上下文、Widget 及 HUD 引用。验证覆盖真实事务、容量、分类过滤、重建/teardown、蓝图和 Dedicated 投影。

### ADR-053：技能瞄准会话与只读范围预览（2026-09-13）

- 状态：accepted；用户已确认设计并授权首版实现，范围和验证见 [AIM-001 Spec](../Specs/AIM-001-skill-indicators.spec.md)。
- 选择：本地 Controller 持有 AimComponent，通过现有 Enhanced Input 和唯一 Order 请求入口确认；纯视觉贴花只消费预览。标准施法默认，按下/松开快施可选。单位目标只取直接命中；超距保留原目标并允许服务器追击。
- 数据/API：AbilityData 兼容新增主预览 Action 索引，默认 -1 不推测作用形状；形状参数读取该 Action 的 SpecialValue 与 ProjectileData 回退。owner-only HUD 增加 CastRangeBonus 和 AttackRange 只读聚合值，PresentationSchemaVersion 由 5 升至 6；核心 combat_v1_rc1 和核心 schema 不变。
- 兼容与迁移：客户端/服务器同版本部署；旧 DataAsset 无需修改即可显示已知施法距离。原卓尔 Q AutoCast、空 WER、HUD 点击详情及 InputAction 引用保留。训练场使用独立资产，材质与样例由 UE 创建保存。未选用复制客户端命中或新目标协议，因为当前 Order/Targeting 已可表达 P0。
- 生命周期：SessionSerial、控制绑定与 LifeGeneration 隔离旧输入；取消、Owner 变更、死亡、撤销、失焦和 EndPlay 统一清理，初始 Accepted 不表示技能完成。
- 测试：真实 ASC/Order fixture 的 Red/Green、范围与 Action 一致性、取消/旧释放、HUD 不穿透、三 Target、全量 Automation、资产/PIE 和 Dedicated 双客户端；证据写入 Spec 后才交付。

### ADR-054：技能指示器地面接收与点目标采样（2026-09-13）

- 状态：已定；用户授权修复 Hero 接收地面指示器的问题，见 [AIM-002](../Specs/AIM-002-ground-only-indicators.spec.md)。
- GameTraceChannel5 为只读 `CombatIndicatorGround` 查询，默认 Ignore；地面、坡道和平台显式 Block。单位技能和普通移动保留 Visibility，服务器继续原 Targeting/Order 校验，网络与核心 schema 不变。
- CustomStencil 最高位 `128` 保留给指示器地面接收者，低七位可供其他表现；项目开启 CustomDepth with Stencil。只有地面组件写该位，角色与道具不写。
- 指示器材质同时检查 stencil、CustomDepth/SceneDepth 一致和向上法线（Z >= 0.5），防止地面标记透过角色染到其身体，或沿平台侧壁拉伸。其他材质和角色 ReceivesDecals 不改。
- 点目标的预览与三种确认方式共用地面查询，命中未配置组件/陡面/无地面时不提交、不重用旧点。Demo、训练与测试地图地面通过 Unreal API 迁移；配置步骤和验证边界见 [10-13](../10-Architecture/10-13-Skill-Indicators.md)。

### ADR-055：独立物品实例与 post-v1 物品契约（2026-09-13）

- 状态：设计已由用户审核并授权实现；执行与验证见 [ITEM-001 Spec](../Specs/ITEM-001-item-system.spec.md)。
- 物品由 World 注册表持有稳定实例与位置真值，单位库存提供六装备/三背包操作，地面 Actor 只引用实例。普通死亡保留物品；World/Unit EndPlay 有明确清理，转移不新建副本。
- 被动使用 Modifier/GE/Hook/Aura 公共入口并绑定物品来源；主动按物品实例授予 Spec，隔离英雄四槽和技能点。物品冷却独立于临时 Spec 且仅有注册表一份权威记录，ASC 委托查询/提交；装备 1 倍、背包/地面 0.5 倍，换入装备 6 秒休眠，均不因丢捡刷新。
- 拾取/放下沿用 Order 与服务器 PathFollowing，即时换格共用连接安全状态且不打断普通移动。所有激活与提交入口检查真实物品归属、版本、装备/休眠/充能/状态；最终结果不同于初始 Accepted。
- 当前发布身份迁为 `combat_v2_items_rc1`、ContractVersion 2，明确分离 ItemsEnabled=true 与 EconomyEnabled=false；保留旧合并字段为废弃兼容字段。标签 schema 2、事件 schema 2、展示 schema 7、日志展示 schema 2；Formula/RNG 与旧内容保持 v1，旧资产默认无物品。旧事件 JSON 迁移补空物品身份，未知新版本明确拒绝；联机双方同版本部署。
- SourceContext 增加物品定义与实例身份，后续 Modifier/Projectile 保留来源快照；HUD/日志只消费只读投影。原始 M8 v1 验收证据保留，不把物品或经济追认进历史发布。
- 回滚成组恢复源码、配置、明确修改的 Demo/UI 资产及新资产引用；需重跑三 Target、全量 Automation、资产/PIE、Dedicated 双客户端和容量/teardown，不以编译替代行为证据。

### ADR-056：HUD 技能槽左键复用技能输入（2026-09-14）

- 状态：已定；由 `HUD-ABILITY-CLICK-001` 修复并完成 Editor/Automation 验证，取代 ADR-047/048 中“技能槽鼠标点击只固定详情”的当前交互约束；真实 PIE 待用户复验。
- 选择：技能槽左键通过本地 HUD 委托映射到 Q/W/E/R 槽位，并调用既有 `ACombatPlayerController::ActivateCombatAbilitySlot` 入口；无目标技能、目标技能瞄准和 `Passive + AutoCast` 分别沿原键盘语义执行。
- 详情与取消：技能悬停仍展示详情；未瞄准时右键固定技能详情，瞄准期间右键继续取消会话。升级按钮优先于技能使用，空槽和非 Ability 槽不发使用请求。
- 权威边界：HUD 只传本地槽位索引，不提交 Ability 类、目标、数值或资源；Controller/Order/Targeting/ASC 的服务器权限、RPC、View schema 和发布契约均不变。
- 兼容与回滚：现有 Q/W/E/R 快捷键、物品点击、技能瞄准和 AutoCast RPC 保持兼容；回滚 HUD 委托、Controller 转发、测试和文档即可恢复原点击固定语义。
- 验证：新增 `Combat.UI.HUD.SkillClickRequest` 覆盖技能左键、右键详情和空槽边界，并回归既有 HUD/Input/AbilityAim；交付证据见 `HUD-ABILITY-CLICK-001` Spec。

### ADR-057：焦点切换期间 HUD 技能点击延后一帧（2026-09-14）

- 状态：已定；由 `HUD-ABILITY-CLICK-002` 的 PIE 复现与输入日志确认。
- 背景：`FInputModeGameAndUI` 从世界视口切到 HUD 时会调用 `ACombatPlayerController::FlushPressedKeys`。若 HUD 的左键请求在同一 MouseDown 中立即启动技能，随后冲刷会把刚建立的本地瞄准会话清掉，表现为右键取消后换技能需要第二次点击。
- 选择：HUD 槽位仍沿用 ADR-056 的统一 Controller 入口，但使用 `FTimerManager::SetTimerForNextTick` 排到焦点切换收尾后执行；`FlushPressedKeys` 的清理保护只覆盖该排队请求，显式右键、Escape、Stop、换绑和 EndPlay 仍可取消它。
- 权威边界：延迟只存在于本地输入调度，不新增 Order、RequestId 或网络协议；技能目标、资源、权限和服务器结算不变。
- 验证与回滚：输入测试覆盖 Flush 后下一帧启动，真实 PIE 覆盖“W → 右键 → E 单击”首击进入 Crosshairs；删除排队 Timer 和对应测试/文档即可回退。

### ADR-058：DOTA2 风格三围与派生属性（2026-09-14）

- 状态：accepted；用户已验收 `ATTR-001` Spec 并授权实现。
- 选择：在 `FCombatUnitBaseStats` 和 GAS `UCombatAttributeSet` 中增加 Strength、Agility、Intelligence 以及 Strength/Agility/Intelligence 主属性选择。力量每点提供 +22 最大生命和 +0.1 生命恢复；敏捷每点提供 +1 攻击速度和 +1/6 护甲；智力每点提供 +12 最大法力、+0.05 法力恢复和 +0.001 魔法抗性；主属性每点额外提供 +1 攻击力。
- 聚合：UnitData 的旧属性字段作为不含三围收益的基础种子；服务器在三围聚合变化后更新八项派生属性 BaseValue，现有装备/Modifier 聚合层保持不变。初始化时派生上限确定后填满当前资源，运行中提高上限不自动补充当前生命或法力。
- 兼容与版本：旧资产新增三围默认为 0，结果保持原值；Numeric Policy 与发布契约 FormulaVersion 升至 2，拥有者 HUD 的 `PresentationSchemaVersion` 由 7 升至 8。客户端只消费复制属性和 owner-only 快照，非拥有者不获得补充 HUD 字段。
- 测试与回滚：新增三围公式、非法值、动态 Modifier 叠加/移除和 HUD 全量快照测试；回滚时成组撤销 AttributeSet、UnitData、HUD 字段、版本声明及文档，旧资产无需迁移。

### ADR-059：单一金币、全局商店与储藏合成（2026-09-15）

- 状态：已定；`ECON-001` 已完成本地实现与交付 Gate，用户游玩验收进行中。
- 选择：在 `ACombatPlayerController` 上持有 `UCombatEconomyComponent`，金币、经济/储藏修订和六格储藏处是玩家级服务器权威；`ACombatGameMode` 为每局冻结 `EconomyData` 与唯一 `ShopData`。不创建场景商店 Actor，不检查位置、队伍或距离，死亡不扣金。
- 事务：`UCombatShopData` 是目录、递归配方和价格唯一来源；购买计划先消费当前合成域已有定义，再原子补购缺失叶子并交付储藏。库存与储藏是分离合成域，转移复用库存公共接口。出售按冻结的全额窗口/折扣值结算，所有请求检查 owning connection、RequestId、绑定代次、Revision、共享 token bucket 和重放窗口。
- UI 与版本：`UCombatShopWidget` 只观察 owner-only 快照并提交意图，包含搜索、基础/升级页、分类网格、配方关系和储藏操作；发布契约迁移为 `combat_v3_economy_rc1` / ContractVersion 3，物品与经济开关分别为 true，旧合并字段保持 false。
- 验证与回滚：Economy 8/8、Shop UI 1/1、全量 Combat 94/94、资产 31/31 无错误通过；Server/Client Target、Dedicated 双客户端、真实 PIE 和经济 soak 尚未执行。回滚必须成组恢复 Economy/Shop、Controller RPC、安全配置、Demo 资产、版本契约和文档，不能只关闭开关。

### ADR-060：商店直达当前物品栏与锁定合成（2026-09-16，2026-09-19 收敛）

- 状态：已定，用户已完成代码 review 并授权验证后本地提交；对应 `ECON-003`，用户要求下线储藏室运行时入口并将购买结果直接交付当前主控单位物品栏，同时允许删除兼容代码。
- 选择：`ACombatPlayerHUD` 不创建储藏 Widget；`UCombatShopWidget` 在右下角常驻金币按钮，按钮只负责本地开关商店。购买、库存出售和锁定/解锁均通过 `ACombatPlayerController` 的服务器经济意图进入 `UCombatEconomyComponent`，不新增场景商店 Actor 或第二套物品实例。库存是唯一交易与合成域。
- 合成与锁定：购买计划和库存自动合成只扫描未锁定实例；锁定状态随唯一实例、`FCombatItemView` 和 owner-only 经济投影复制。锁定只改变状态；解锁完成修订校验后立即检查当前库存，满足配方时在同一请求尾部按稳定优先级合成，无完整配方则只刷新状态。锁定与未锁定堆叠不合并，新的购买/合成结果默认解锁。
- 物品栏输入：右键菜单只保留使用、出售、锁定/解锁和反向整理；“放到地面”不再是菜单动作，也不保留脚下即时投递 API。地面丢弃只能由拖拽释放的屏幕落点提交，继续走服务器导航、距离、LOS、实例修订和所有权复核。
- 兼容与版本：不保留旧储藏字段、类、实例所有权或服务端动作；经济表现 schema 升为 3，`ContractVersion=4`、`ReleaseId=combat_v4_economy_rc1` 与物品/经济开关保持开启；客户端/服务器必须同版本。
- 验证与回滚：全量 Combat 96 项零失败、真实 NullRHI PIE、5 个蓝图编译回读、独立 Dedicated 双客户端各 300 秒/10 轮、64/256 容量、Windows cook 650 包和资产 31 项通过；三 Target 与最终 Gate 证据见 ECON-003 §7。人工视觉、打包部署、更长 soak 与网络损伤未覆盖。回滚需成组恢复上一版客户端/服务器发布，不留下半迁移入口。

### ADR-061：本地边缘滚屏与按住跟随（2026-09-20）

- 状态：accepted；用户已 review CAM-001 v0.2，并授权 CAM-002 实现。
  - 选择：复用 Command Pawn；普通视角仅靠鼠标贴视口边缘平移，Space 使用可配置 Enhanced Input Action 临时跟随就绪的 `CommandedUnit`。边缘滚屏接管后旧 Space 按住不恢复；松开/取消停留。已经贴边时新按 Space 可以进入跟随，原边缘等到移出后重新武装。默认阈值短边 2.5%、速度 1800 cm/s、立即停止、跟随 12/s、保持绑定高度、矩形边界默认关闭。
- 权限与生命周期：只在本地 Controller 更新，不发送相机 RPC、不写 Unit Transform；保留 `SetReplicateMovement(false)`。绑定按目标/代次去重，失焦/Owner 未就绪/teardown 清理；相机配置为本地蓝图属性，不进入战斗版本 schema。
- 迁移与回滚：新增 Demo Follow Action/Space 映射/Controller 引用；旧蓝图无引用只禁用 Space。撤销相机增量与这三个资产即可恢复旧跟随。
- 验证与边界：[CAM-002](../Specs/CAM-002-edge-pan-follow.spec.md) 覆盖自由锚点、边缘方向/帧率、跟随/释放、输入冲突、绑定与网络隔离；[10-16](../10-Architecture/10-16-Dota-Camera-Movement.md) 维护实际行为。Camera Grip、缩放和观战不在本轮范围。

### ADR-062：StateTree 通用 AI 决策架构（2026-09-20）

- 状态：accepted；2026-09-20 用户确认 review 完成并要求开始工作，接受 AI-001 v0.2。阶段 A 运行时授权与实施记录见 AI-002；后续阶段仍分别验证。
- 上下文：现有 Combat AIController 只承担导航/Crowd，Order 已统一移动、追击、持续普攻与施法；工程启用 StateTree 插件，但尚未建立通用自主决策层。
- 选择：以 Unit 上的服务器 `UStateTreeComponent` 派生 Brain 和自定义 Schema 运行唯一 StateTree；通过类型化知识 Context、Profile、Linked Asset、原生条件/效用评分/任务组合角色行为。C++ 提供数据服务和执行适配，不建立替代 StateTree 的行为状态机。
- 执行边界：每条活动路径只有一个命令写入任务；Order Bridge 处理语义去重、提交前回执监听、同步终态缓存、动作占用和按句柄取消。玩家/脚本合法接管先撤销 AI 资格，旧 Task 退出不得停止新控制者命令。
- 时序与数据：AI gameplay 轮询/等待使用 Combat Scheduler，StateTree 原生 Scheduled Tick 负责处理已发布信息；属性、命中、资源、移动和战斗结算继续沿用公共权威系统。知识只使用被允许观察的信息，完整迷雾与大规模查询另有接入门。
- 0.2 补充：事件仲裁与完成转移分阶段处理，普通重评不得跳过完成记账；PreparedIntent/完成凭证在类型化工作区跨状态交接；拟新增按 Order 句柄匹配的边界票据，为 StateTree 保留有界重评机会，仍由原 Attack/Scheduler 管理攻击时钟。Consideration 为实验性 API，阶段 C 通过版本与 Target/cook 准入后才启用 Utility Profile。
- 备选与未选择原因：不采用前次咨询提出的轻量状态机；用户已选定 StateTree。Behavior Tree/Blackboard 和另一套行为框架不与 StateTree 并行承载同一单位决策。
- 影响与迁移：本文不修改现有契约。后续实现另建 feature Spec，审查 `StateTreeModule`/`GameplayStateTreeModule` 依赖、Profile PrimaryAsset、Native Tag、Order 条件取消/边界交接和控制入口；旧单位无 Profile 默认不启用自主 AI。
- 验证与回滚：设计和未来测试矩阵见 [10-17](../10-Architecture/10-17-StateTree-AI-Decision-System.md)，文档证据见 [AI-001](../Specs/AI-001-statetree-ai-design.spec.md)。本次只回滚文档增量；运行时需要阶段性的 Automation、资产、Dedicated 与容量证据，尚未执行。

### ADR-063：StateTree 阶段 A 的兼容接入（2026-09-20）

- 状态：accepted；2026-09-21 用户明确确认 AI-002 阶段 A 验收成功，后续 B/C/D 尚未开始。
- 决策：按 [AI-002](../Specs/AI-002-statetree-runtime.spec.md) 实施 Brain/Schema、类型化工作区与 Order Bridge；增加按 Handle 取消和攻击边界票据。StateTree 是行为编排主体，战斗执行仍走 Order/ASC/Attack。
- 数据与版本：新增 `CombatAIProfile` 定义类型、本地 schema 1 和 UnitData 可选 Profile；空值保留既有行为。树 Wake 使用类型化本地事件载荷，不新增网络 GameplayTag/Event schema；保持 `combat_v4_economy_rc1`、Contract 4、Tag/Event 3。
- 控制与时序：服务器运行；合法手动命令经预检才撤销 AI，旧 epoch 只能清理自己完整 Handle；独立换槽事务维持原动作，不触发接管。所有等待和边界超时走 Combat Scheduler。完成凭证在普通重评前消费，死亡/撤权是同步清理屏障。取消前摘除旧当前项，允许同步回调提交新命令而不被旧清理覆盖。
- 迁移与回滚：新 Profile 类型纳入 AssetManager，编辑器树构建器按 WITH_EDITOR 隔离；旧资产无需迁移。移除 Profile 即禁用自主行为，代码/配置/新资产可增量回退，不覆盖既有用户修改。
- 验证：三 Target、完整 Automation、真实树资产编译保存回读、PIE 和 Dedicated 双客户端；状态和实测以 AI-002/台账为准，不借用历史通过结果。

## 3. 本轮查漏补缺摘要

原单体文档对 Damage、Modifier、Scheduler、AttackRecord 和网络权威已有较强约束；本轮新增或显式登记了以下遗漏：

- Unit 死亡/复活、跨生命 generation 和清理顺序。
- Team/阵营关系统一接口、视野/隐身/LOS 对目标合法性的影响。
- Ability 授予、等级、移除、Intrinsic Modifier 和 AutoCast 的权威来源。
- Native GameplayTag 治理、PrimaryAsset 身份/重命名/版本迁移。
- 数值有限值、取整、公式版本和可记录 RNG。
- 攻速、attack point、转向和动画只作表现的契约。
- Projectile collision matrix、同一 sweep 的稳定命中顺序。
- Aura 所有权与 child Modifier reconcile。
- 资源恢复、状态抗性、冷却缩减的动态语义。
- Combat View、结构化日志、调试命令、容量指标和 Dedicated soak。
- RPC 请求 id、限频/包上限和客户端缺失 Definition asset 的降级。
- 模板代码适配风险：EQS/MoveRequest 回调未绑定 generation，TwinStick 直接命中/Timer 不能复用为结算。

已给出不改变核心架构的建议基线；仍需产品选择的内容保留为 Gap。

## 4. P0：进入基础实现前关闭

| Gap | 状态 | 缺口 | 建议基线 | 最迟节点 |
| --- | --- | --- | --- | --- |
| GAP-001 | 已关闭 | TeamId、Neutral、召唤物继承关系和队伍变化 | ADR-017；完整值域/失败 Tag/换队规则见 [90-01](../90-History/90-01-M0-Design-Freeze.md#2-dec-001队伍与目标关系) | 2026-08-24 / DEC-001 |
| GAP-002 | 已关闭 | Alive/Dying/Dead/Respawning、尸体、复活和跨生命回调 | ADR-018；状态机/固定清理表/默认保留项见 [90-01](../90-History/90-01-M0-Design-Freeze.md#3-dec-002unit-生命状态) | 2026-08-24 / DEC-002 |
| GAP-003 | 已关闭 | Ability grant/level/remove/intrinsic/autocast 产品规则 | ADR-019；单一身份链/等级/移除规则见 [90-01](../90-History/90-01-M0-Design-Freeze.md#4-dec-003ability-授予等级和-autocast) | 2026-08-24 / DEC-003 |
| GAP-004 | 已关闭 | DefinitionId 命名、重命名、资产版本和 Tag 废弃 | ADR-022；Tag/PrimaryAsset/redirect 规则见 [90-01](../90-History/90-01-M0-Design-Freeze.md#7-dec-006gameplaytag-与资产身份) | 2026-08-24 / DEC-006 |
| GAP-006 | 已关闭 | 暴击/闪避/随机 proc 的随机源和复现 | ADR-020；keyed roll/记录/注入规则见 [90-01](../90-History/90-01-M0-Design-Freeze.md#52-combat-rng-v1) | 2026-08-24 / DEC-004 |
| GAP-007 | 已关闭 | 百分比 clamp、取整、超大值、NaN/Inf 和公式版本 | ADR-020；Numeric Policy v1 见 [90-01](../90-History/90-01-M0-Design-Freeze.md#51-numeric-policy-v1) | 2026-08-24 / DEC-004 |
| GAP-009 | 已关闭 | Pawn/Projectile/WorldStatic/友军/Source 的碰撞矩阵 | ADR-021；Channel/Profile/LOS/单位规则见 [90-01](../90-History/90-01-M0-Design-Freeze.md#6-dec-005碰撞los-和地图单位) | 2026-08-24 / DEC-005 |
| GAP-020 | 已关闭 | Automation/CI 是否包含 Dedicated Server/Client target | `ue_gasServer`/`ue_gasClient` Development Target 已纳入基线并由源码 UE 5.8.0 构建通过；安装版 UE 5.8.1 以独立 Server/Game 进程完成真实连接与场景状态 smoke；版本边界及复现命令见 [90-02](../90-History/90-02-M1-Environment-Decision.md) | 2026-08-25 / TST-003 |

这些字段会进入公共 Context、Handle、DataAsset 或 Collision Profile，晚改会波及几乎全部子系统。

## 5. P0/P1：进入对应功能前关闭

| Gap | 状态 | 缺口 | 建议基线 | 最迟节点 |
| --- | --- | --- | --- | --- |
| GAP-005 | 已关闭（ADR-026） | StatusResistance 是否改变 Duration、tick interval、总伤害 | 只缩短明确可缩短 Debuff Duration；Think interval 不变；边界 tick 按缩短后的 ExpireAt | M2/MOD-004 |
| GAP-008 | 已关闭（ADR-027） | Vision/Fog/Invisible/TrueSight 与施法、攻击、弹体目标合法性 | M3 固定 `VisibilityPolicy=None`；服务器重算 Actor/位置/AoE，API 保留可见性策略扩展点 | M3/TGT-001 |
| GAP-010 | 已关闭（ADR-030） | BAT/AttackSpeed/attack point、移动起手、转向角 | 公式、clamp、移动起手、15° 朝向与动画边界见 [90-07](../90-History/90-07-M4-Order-Attack-Decision.md#4-attacktiming-policy-v1关闭-gap-010) | 2026-08-26 / ATK-002 |
| GAP-011 | 已关闭（ADR-028） | Montage notify 与 gameplay 时间的关系、被打断时动画清理 | gameplay 只使用 Scheduler；notify 仅作表现校准；所有退出路径统一停止表现并清理 Task/Schedule | M3/ABL-003 |
| GAP-012 | 已关闭（ADR-024） | Health/Mana regen 的周期、暂停和死亡行为 | 0.25 s Scheduler Coalesce；Health 走 HealSubsystem；Mana 走 Instant GE；非 Alive 暂停 | M2/ATR-001 |
| GAP-013 | 已关闭（ADR-025） | Death 后 cooldown、Mana、非 RemoveOnDeath Modifier、奖励归属 | 保留 Spec/cooldown/AutoCast/非死亡移除 Modifier；Mana 复活至 Max；M2 仅记录归属 | M2/LIFE-001 |
| GAP-016 | 已关闭（ADR-037） | SpellBlock、Break、Debuff immunity、Dispel immunity 等高级状态 | 阶段矩阵与交互见 [90-11 §5](../90-History/90-11-M6-Content-Decision.md#5-高级状态矩阵关闭-gap-016-的基线)；四类独立 Tag/Failure/Event，不用 MagicImmune 包办 | 2026-08-26 / EXT-602 |
| GAP-023 | 已关闭（ADR-034） | Untargetable/Invulnerable/OutOfGame 对已有 Tracking Projectile 的影响 | Dead/生命代次变化/Untargetable/OutOfGame 按 Data 选择 Fizzle 或 LastKnown；Invulnerable 继续跟踪并在 impact 走统一目标/伤害判定；见 [90-09 §4](../90-History/90-09-M5-Projectile-Motion-Decision.md#4-tracking-目标丢失关闭-gap-023) | 2026-08-26 / PRJ-003 |
| GAP-024 | 已关闭（ADR-029） | CDR/耗蓝缩减在 cooldown 已开始后的动态变化 | Cost/CDR 在 commit point 快照；已开始 cooldown 不重排；同 Stage Cost/Cooldown 整体预检 | M3/ABL-003 |

## 6. P1：内容、网络与工具扩展

| Gap | 状态 | 缺口 | 建议基线 | 最迟节点 |
| --- | --- | --- | --- | --- |
| GAP-014 | 已关闭（ADR-036） | Aura 没有 owner/target 生命周期 | 每 World registry、Scheduler Coalesce、统一 Targeting 与普通 child Modifier reconcile；完整规则见 [90-11 §4](../90-History/90-11-M6-Content-Decision.md#4-aura关闭-gap-014-的基线) | 2026-08-26 / EXT-601 |
| GAP-015 | 明确延期（ADR-041） | Summon/illusion 的 Owner、Team、ASC、Order 权限 | 不属于 v1；发布契约固定 `bSummonsAndIllusions=false`。引入前新增独立 ADR，冻结独立 Unit/ASC、CommandingController 与 gameplay owner、Team 继承和 teardown Gate | post-v1 / 引入召唤物前 |
| GAP-017 | 部分解除（ADR-049、ADR-055、ADR-059、ADR-060、ADR-066） | 物品、背包、技能点、天赋和经济 | 经验与技能点按 ADR-049 接入成长组件；物品/背包按 ADR-055 进入 v2 基础；经济按 ADR-060/066 明确为玩家资源账本 + 英雄库存交易域。天赋仍延期；旧合并字段保持 false，物品与经济开关分别表达 | post-v1 / v4 |
| GAP-018 | 已关闭（ADR-040） | 目标容量/帧/带宽预算和池化触发阈值 | 预算、采样边界与优化触发规则见 [90-13 §7](../90-History/90-13-M7-Network-Observability-Decision.md#7-容量预算关闭-gap-018-的目标值)；64 Unit/256 Modifier 双客户端 soak 通过，验收证据见 [90-14](../90-History/90-14-M7-Acceptance.md) | 2026-08-27 / PERF-701 |
| GAP-019 | 已关闭（ADR-039） | Combat Event schema 版本、存档/回放边界 | schema v1、环形诊断与明确不支持的 replay 边界见 [90-13 §6](../90-History/90-13-M7-Network-Observability-Decision.md#6-事件调试和回放边界关闭-gap-019-的目标值) | 2026-08-27 / OBS-701 |
| GAP-021 | 已关闭（ADR-038） | RPC token bucket、批量命令上限和重复 request id 窗口 | ownership、20/s + 32 burst、8 Order/4096 bytes、128 RequestId 窗口及失败 Tag 见 [90-13 §3](../90-History/90-13-M7-Network-Observability-Decision.md#3-order-rpc-安全基线关闭-gap-021-的目标值) | 2026-08-27 / NET-002 |
| GAP-022 | 明确延期（ADR-041） | Ability/移动本地预测和回滚 | v1 只保留 Projectile 纯视觉 PredictionKey；完整 PredictionKey owner、rollback 与 Cue reconcile 需独立 ADR/schema/Gate，发布契约固定 `bGameplayRollback=false`；评估见 [90-15 §3](../90-History/90-15-M8-Release-Candidate-Decision.md#3-rel-004-预测评估) | 2026-08-27 / post-v1 |
| GAP-025 | 已关闭 | 暂停、global/custom time dilation 语义 | `UCombatSchedulerSubsystem` 使用 World game time；real-time UI 不进入 Scheduler；时序/catch-up/budget/teardown 自动化通过 | 2026-08-24 / FND-007 |
| GAP-026 | 已关闭 | 直接 Possess Demo 允许 owning client 参与单位移动，客户端 Pawn 解穿透可产生服务器未认可的视觉位移 | ADR-043 已落地：服务器 Combat AIController + Command Pawn + 全客户端 SimulatedProxy + 单一 Detour Crowd；三档 Dedicated 双客户端对撞、RPC、64/256 容量和 teardown Gate 见 [10-10](../10-Architecture/10-10-Server-Authoritative-Movement-Kickoff.md) | 2026-09-02 / SAM-008 |
| GAP-027 | 待处理 | 交互式 Editor 内运行既有完整 Automation 后切地图，部分技能测试的 CDO 持有临时 AbilityData，导致测试 World 无法 GC | HUD-LOG-001 验证时发现；引用链指向 Default__CombatSelfHealAbility、CombatMeatHookAbility 等既有测试配置，不含日志组件。当前以独立进程执行全量 Automation、干净 Editor 执行 PIE 隔离；后续为修改 CDO 的测试增加作用域恢复。证据 `Saved/CombatLog/Editor-UI.log` 的 World Memory Leaks 引用链 | 后续测试设施维护 |
| GAP-028 | 分阶段处理（ADR-062～065） | 阶段 A/B/C 已验收；阶段 C 的 v2 Utility、技能候选、EQS、World 预算、独立演示与 64/128/256 AI 容量证据见 AI-002～004。完整权威可见性、可选公共空间索引、长时间 AI soak、网络损伤和 D 阶段遭遇扩展仍未实现或验证 | [配置指南](../20-Content/20-04-StateTree-AI-Guide.md) 区分当前 API 与 [10-17](../10-Architecture/10-17-StateTree-AI-Decision-System.md) 的后续方案；范围/LOS 不等于迷雾。本轮容量未触发空间索引升级，真实复杂地图若超预算再独立评审公共 Targeting | A/B/C 证据独立归档；总缺口保留到可见性/D 阶段与长期容量边界另行决策 |
| GAP-029 | 分阶段处理 | DEF-002 Phase 1 已通过用户验收；DEF-004 已接入个人强化池/服务器种子/选择/重抽/效果；DEF-006 将团队原型商品接入原 Combat 商店与获得即用。换英雄保留、重连与完整机制内容仍待后续 | [DEF-001 设计](../20-Content/20-05-Roguelite-Defense-Game-Design.md)、[DEF-002 Spec](../Specs/DEF-002-defense-phase1-skeleton.spec.md)；Phase 1 通过后再为 Phase 2 建立种子/选择/商店契约 | Phase 2 肉鸽构筑前 |
| GAP-030 | 待处理 | 既有瞬态无限 GE 在 owning-client Mixed 中无法解析完整定义，DEF-002/004 网络日志出现 PostReplicatedAdd 诊断 | 本步保持公共属性/UnitView 结果，双客户端逐项核对；完整 GE 定义复制需独立内核 Spec，不通过切换玩家复制策略掩盖 | 后续公共 Modifier 网络改造前 |

### ADR-064：阶段 B 的事实快照与角色职责（2026-09-21）

- 状态：accepted（用户授权继续，AI-003 F1 0.1 本地批准；实现和验收结果见 Spec）。
- 选择：Brain 唯一持有 Scheduler 感知与有界知识记忆，范围/LOS 通过公共 Targeting；受击只为已被感知许可的来源增加威胁。服务器职责输入限定为锚点、有限路线及循环规则，StateTree 的有序条件和 Linked Asset 决定 Guard/Engage/Return/Route 分支。
- 接口：Profile 增量字段默认关闭，旧本地版本 1 可直接加载；新 SetAssignment 为本地服务器 API。PreparedIntent/Receipt 保存操作及取消原因，路线/归位提交以匹配回执和真实到达为准。不新增 RPC、Native Tag 或 Combat Event 字段，release/Contract/Content 版本不变。
- 接线细化：10-17 §15.4 的 Leash 强制归位在阶段 B 由 Execute 的事件 Tick 精确取消并保留原因，经过 Resolve 记录返回请求，再由条件进入 ReturnHome；仍可中断前摇，但使用统一完成出口，避免事件转移废弃同帧终态。C++ 不选择行为分支，也不建立替代 StateTree 的层级状态机。
- 兼容与回滚：旧显式目标树和阶段 A 场景保留；新角色资产位于 AI/Roles。移除新 Profile 即关闭自动感知；不得整体回滚仍未提交的 A 差异。
- 验证：AI-003 已完成隐藏信息/旧生命/确定排序/归位锁定/路线恢复/三次失败退避/接管与清理的专用覆盖；`Combat.AI` 20/20、完整 `Combat.` 121/121、37 资产 0/0、角色 PIE、Dedicated 双客户端、NavMesh 构建和 Windows cook 均通过。完整迷雾、空间索引与 AI 容量仍属于后续阶段，GAP-028 不关闭。

### ADR-065：阶段 C 的 v2 战术决策与 World 查询预算（2026-09-22）

- 状态：accepted（AI-004 F1 0.2 本地批准，F2 PASS、Push-Ready READY；2026-09-22 用户确认阶段 C 验收完成）。
- 选择：仅 `AIProfileVersion=2` 且显式启用战术的 Profile 使用最高效用选择。根树先按顺序处理 Blocked、Retry 和 Return，再进入 `TrySelectChildrenWithHighestUtility` 的 Cast/Reposition/Attack/Guard；硬职责不会被高分战术穿透。v1 Profile 绕过新增 World 预算，保持阶段 A/B 的同步首次感知与既有周期。
- 技能与执行边界：候选只枚举本单位已授予的主动 AbilitySpec，并调用 `PreflightAIOrder` 读取公共合法性；评分不激活、不扣费、不开始冷却，也不复制伤害/范围真值。保留的 `TacticalLocation` 目标策略隐藏且由校验拒绝，EQS 只服务 Reposition。持续 Attack 只在规则允许时申请既有边界票据，Ready 后重新核对候选、分差和 `InterruptPreference`；真正提交仍只有 Brain 的 `IssueAutonomousOrder`。
- 查询、权限与生命周期：Utility、感知、EQS 与 Order 提交只在服务器运行。每个战术查询同时持有 QueryId、QueryGeneration、运行/控制/生命、Scope、职责和目标生命身份；Scope 退出、换 Profile、接管、死亡、EndPlay/World teardown 先使身份失效，再精确 Abort，并以 World token exactly-once 结算。感知/EQS 在执行前申请 World 配额，生命周期清理、回执和攻击边界不受普通预算阻塞。
- 迁移与回滚：不修改 Release、Contract、Content、GameplayTag 或 Combat Event schema；旧 v1 与 A/B 资产不迁移。移除 `/Game/Combat/Demo/AI/Tactics` 的 v2 Profile/地图即可停用新路径。Consideration 仍为 UE 实验性 API，每次引擎升级重跑三 Target/cook 准入。
- 验证：AI-004 最终战术 21/21、AI 42/42、完整 Combat 143/143；Editor/Server/Client 三 Target、42 资产、冷回读、Tactics PIE、Dedicated 双客户端和 Windows cook 通过。64/128/256 档的感知与 EQS 请求/执行/延期、峰值在途、p95/p99 和命令数均已记录，当前未触发公共空间索引扩展；详见 [AI-004](../Specs/AI-004-statetree-tactics-capacity.spec.md)。

### ADR-066：玩家战略资源与英雄库存分域（2026-09-23）

- 状态：已实现并于 2026-09-24 通过用户验收；对应 [RES-001](../Specs/RES-001-player-resource-ownership.spec.md)，用户已明确“金币和其他战略资源归玩家，库存仍属于英雄”的边界，并确认纯资源余额变化不进入战斗 UI 日志。
- 选择：本局仍由 `ACombatPlayerController` 持有 `UCombatEconomyComponent` 作为玩家资源账本，不引入 `PlayerState` 或跨重连持久化。`ACombatUnitCharacter` 额外记录服务器侧稳定 `ResourceOwnerPlayerController`，与短暂的 `CommandingPlayerController` 分离；切换/解绑英雄不清除稳定 owner，Controller teardown 才清理。
- 库存边界：`UCombatInventoryComponent`、物品实例 `Holder`、槽位、库存修订、锁定、装备能力和自动合成继续属于英雄。商店事务只作用于当前主控英雄；经济组件的兼容 `FCombatEconomyView` 仍可读取，但新增 `FCombatPlayerResourceView` / `FCombatHeroInventoryView` 及分域变化委托，避免把聚合投影误读为所有权。
- 奖励与日志：击杀奖励、库存自动合成和库存投影回调通过稳定资源 owner 找到玩家账本。纯 `GoldChanged` 继续写服务器核心事件供诊断，但不再被 `CombatLogPresentation` 分类，因此不进入战斗 UI；购买/出售/合成和私有库存事件仍使用仅服务器的 owner recipient，公共战斗事件继续使用 Unit 相关性。核心 `FCombatLogRecord`、Event/Presentation wire schema 和 RPC 不变。
- 兼容、验证与回滚：旧经济请求、HUD 聚合读取和历史文档保持兼容；新增英雄切换、分域视图、资源事件 UI 排除与双玩家回归。最终证据统一记录在 RES-001 0.2。回滚按 RES-001 文件组撤销稳定 owner、展示 recipient、分域 API 与日志分类修订，同时保留既有资产修改；若未来需要断线重连、团队资源或跨局保存，必须新建 Spec/ADR。

### ADR-067：本地选择与多单位控制权限分离（2026-09-24）

- 状态：0.2 已于 2026-09-24 通过用户验收，并获授权创建本地提交；范围与验证见 [CTRL-001](../Specs/CTRL-001-multi-unit-selection.spec.md)，用户明确包含查看、切换、Shift 多选/框选和群体移动攻击。
- 本地观察目标、最多 8 个选中单位与服务器 CommandedUnit 主选分离。Owner 是控制许可，Team 不授予权限；选择已有权限英雄不取消旧 Order、不撤销旧 Owner。旧显式绑定 API 保留其转移语义，新增授权/撤销和受限主选请求。
- 群体 Move/Attack/Stop 使用有界 Controller RPC，共享原连接重放窗口/限频，服务器先复核所有权、唯一性、生命代次、载荷再逐个进入公共 Order；技能/物品/商店只操作主选。
- 展示 schema 8 升至 9：增加公开英雄查看快照，包含等级、属性、资源、技能定义/等级和物品定义/数量；清除技能/物品操作身份、修订、冷却、经验/技能点、锁定与 AutoCast 状态。旧 owner-only 投影保留；所有客户端/服务器须使用同版本。无战争迷雾能力，不声称实现迷雾保密。
- 查看无权单位时 HUD 只读；主选复制未就绪时阻止技能/物品请求，UI 消费区域和旧瞄准/拖放不得产生世界选择或旧英雄操作。新生命周期与验证结果随本任务记录，不改写历史验收。
- Demo 的额外英雄配置默认 0，仅演示蓝图开启 1；公式、GameplayTag/Event、资源归属和核心发布契约不变。
- 0.2 用户反馈修正：选中/框选只改变本地目标，不触发相机居中；每个 Command Pawn 首次就绪定位一次，Space 仍主动跟随。Combat Unit 不可作为另一 Combat Unit 的可行走地面，防止胶囊接触后的 UE 自动跳离；保留硬碰撞与单一 Crowd，不改变群体命令终点、权限、网络载荷或技能 Motion。

### ADR-068：HUD 左下角小地图的只读投影与交互边界（2026-09-27；已实现 2026-09-29）

- 状态：`accepted/implemented`；设计边界见 [HUD-MINIMAP-001](../Specs/HUD-MINIMAP-001-minimap-design.spec.md)，实现与验证见 [HUD-MINIMAP-002](../Specs/HUD-MINIMAP-002-minimap-implementation.spec.md) 和 [10-12 小地图实现](../10-Architecture/10-12-Bottom-HUD-Design.md)。原生暗色地表、真实原型几何特征和稳定地标是当前可运行基线，静态纹理与 Designer 外观保持可选。
- 选择：小地图固定在左下角，标准 `220 × 220`、窄视口 `184 × 184`，通过 viewport lower-left anchor 和 `12` 左/底安全边距与底部主 HUD 共用视口底边；静态地形、道路/河道、主控/友军/可见敌方/目标/Ping/镜头视锥分层绘制。主控用青色菱形，友军蓝点，公开可见敌方红点，目标与 Ping 使用琥珀色并配合形状表达。
- 交互：左键短点击定位本地镜头；左键按住移动超过阈值后按地图增量平移本地镜头，固定缩放的底图、网格和标记不移动；右键复用现有选中组 Move Order 和服务器安全校验；`Alt +` 左键短点击产生本地 Ping，未定义 Ping 事件和接收者范围前不得宣称多人可见。拖动不创建第二个 Camera Pawn，也不向世界或 Order 链路穿透。
- 权威与隐私：小地图只消费已经允许客户端观察的地图/单位 View 和表现事件，不复制完整 owner-only HUD 快照；当前没有战争迷雾能力，不能将“只显示可见敌方”描述为网络层敌情保密。需要视野服务、公开 View 字段或可复制 Ping 时，必须另建实现 Spec/ADR。
- 生命周期：由现有本地 `ACombatPlayerHUD` 持有 Widget；换主控、查看目标、绑定代次、Widget 重建和 Unit EndPlay 清理动态标记/异步回调，并继续使用 `BindingRevision + LifeGeneration` 隔离旧回调。静态图层和标记刷新只做表现，不新增 gameplay Timer 或服务器结算入口。
- 0.3 运行修复（2026-09-29）：UE MCP 真实 PIE 发现全屏 HUD 根几何被误当作避让面板，导致小地图折叠；实现改为读取 `HUDFrame`/详情面板的实际 Slate 几何，并由主 HUD 在折叠状态触发布局恢复。冷启动 PIE 回读为 `VISIBLE`，不改变地图、权限、Order 或网络契约。
- 0.4 交互/布局修订（2026-09-29）：横跨视口的 HUD 承载层不再抬高左下候选位置；小地图改用 lower-left anchor 处理 PIE/DPI 缩放，并加入 `ViewCenterUV` 边界夹紧和左键拖动视角。Minimap 4/4、Camera 6/6 与冷启动截图/`VISIBLE` 回读通过；物理拖动手感和四档 DPI留给用户复核。
- 0.5 悬停提示修订（2026-09-29）：移除小地图鼠标悬停时的操作提示行，保留短点击、拖动、右键和 Alt+左键交互；不改变地图、相机、Order、权限或网络契约。
- 0.6 拖动语义修订（2026-09-29）：左键拖动改为按地图增量平移本地相机，固定 `ViewCenterUV` 与地图内容；新增 `PanCameraFromMinimap` 本地入口和相机拖动回归，不改变单位、Order、权限或网络契约。
- 0.7 镜头轮廓修订（2026-09-29）：`PaintCamera` 不再逐角夹紧越界投影点，改为对四角做统一平移/缩放拟合，保持四点、非零面积和透视梯形；新增 `CameraTrapezoidStaysTrapezoid` 回归，不改变地图、相机、Order、权限或网络契约。
- 0.8 边界可见部分修订（2026-09-29）：取消越界轮廓的整体拟合，按地图表面矩形裁剪原始镜头投影；轮廓靠边时只显示地图内部分并继续跟随摄像机，新增 `CameraTrapezoidClipsAtBoundary` 回归，不改变地图、相机、Order、权限或网络契约。
- 0.9 地形与地标修订（2026-09-29）：依据 UE MCP 回读的 Demo/Test `Floor_0` 和 StaticMeshActor bounds 增加暗色地表、边界墙、障碍、中央平台、出生点和补给区向量层；无可靠纹理时收起白色 Designer `MapSurface`，并显式传递 Slate 画刷颜色。`TerrainFeatures` 回归、正常 Editor 构建、Minimap 6/6、Camera 6/6 和冷启动 PIE 截图通过，不改变地图资产、权限、Order 或网络契约。
- 1.0 文字与输入时序修订（2026-09-30）：低优先级墙体、掩体和中央十字只绘制形状，保留中心平台、出生点和补给区文字；普通左键按下立即调用本地镜头定位，后续拖动从按下点继续平移，释放不重复定位；新增低优先级标签和 `PressPositionsBeforeDrag` 回归，Minimap 7/7、Camera 6/6 和冷启动 PIE 截图通过，不改变地图、权限、Order 或网络契约。
- 1.1 全量无文字与重生成流程（2026-09-30）：小地图静态特征、动态单位、镜头梯形和 Ping 只保留颜色/形状，所有 `Label`/`MapStatus`/TextBlock 内容清空并隐藏，`PaintTerrain` 不再绘制文字；保留 `CombatMinimapWidget.cpp` 向量表作为配置缺失/非法时的回退。
- 1.2 编辑器生成入口（2026-09-30）：Level Editor 提供 `工具 → COMBAT 小地图 → 保存并生成小地图`、工具栏 `生成小地图` 和 `combat.Editor.GenerateMinimap` 命令。入口保存当前关卡后扫描可见静态网格（包含运行时 Mobility 为 Movable 的地面），按命名地面/最大水平几何确定范围，将无文字矩形特征写入 `Config/CombatMinimap.generated.json`；运行时按地图包加载，改图后重复按钮与 PIE 即可刷新，不改地图几何、碰撞、导航、Order 或网络契约。
- 1.3 用户验收（2026-09-30）：用户确认小地图生成按钮、JSON 配置和 PIE 显示验收完成，授权本地 Git 提交；保留完整 Combat D3D12 设备移除作为未执行边界，不推送远端。
- 兼容与回滚：不改变 `combat_v4_economy_rc1`、GameplayTag、事件/展示 schema、DefinitionId、RPC 和既有 HUD/相机语义。回滚时删除 HUD-MINIMAP-001 文档增量即可；实际实现需以独立 Spec 成组回退代码、蓝图、资产和网络字段。

### ADR-070：Phase 1 防守骨架的公共入口与实现边界（2026-10-15；DEF-002）

- 状态：`accepted/implemented`（用户授权实现，F1 0.2 本地计划审查批准）；DEF-002 本地矩阵完成，尚未用户验收。
- 选择：新增 DefenseRun/Wave 定义、Defense GameMode/GameState、Objective Combat Unit 代理和 Scheduler 驱动 Director；核心 Health 继续由 ASC/Combat Damage/Lifecycle 保存，敌人移动/攻击继续走公共 Order/AIController。
- 约束：Phase 1 只消费固定 5 个普通波次 + 1 个 Boss、两条路线、有限出生失败和 PhaseGeneration；不引入随机种子、随机强化选择、团队商店、事件、重连或新 GameplayTag/Event schema；固定 Modifier 强化属于 Phase 1。核心死亡优先，终局与旧回调 exactly-once。
- 兼容与回滚：旧 `ACombatGameMode`、Demo 地图和既有资产默认不变；删除 `Defense/`、DEF-002 测试和独立模式入口即可回滚。需要真实地图资产、公开 View 扩展、网络载荷或 Phase 2 机制时递增 Spec 并重新审查。
- DEF-002 0.2：新增独立 Run/Wave schema 1、GameState 投影 schema 1 和稳定 Unit/Modifier 定义；保留既有 Combat Content/Tag/Event/Release 版本。出生失败不得获胜，清场延后一轮判定以保证同帧核心死亡优先；加入独立灰盒地图、固定强化、复活、每玩家每波幂等奖励和单/双人网络验证。无旧资产迁移，回滚只撤新目录和扫描条目。
- 当前证据：DEF-002 三 Target、173 项分进程 Automation（含渲染 PIE）、56 资产、导航冷回读、单/双人胜利与单人失败独立网络 smoke 通过。原混合进程回归复现 GAP-027；3–4 人容量、长时 soak、cook/打包和用户完整游玩未覆盖。

### ADR-069：肉鸽成长型合作防守作为 post-M8 玩法方向（2026-10-14；用户方向更新 2026-10-15）

- 状态：`proposed`；2026-10-15 已记录用户方向，设计任务见 [DEF-001](../Specs/DEF-001-coop-defense-design.spec.md)，本设计任务当时未授权运行时修改；2026-10-15 用户另行授权 DEF-002 Phase 1 实现，发布契约仍不变。
- 选择：首版按 1–4 人合作、单人可通关、每局服务器种子、个人强化三选一、团队商店即时消耗品、波次回防、事件和 Boss 组织体验；完整版目标约 25 分钟；局外永久成长、账号存档和中途加入暂时关闭。
- 约束：随机结果按 Run/WaveSpawn、AugmentOffer、EventChoice 和 Loot 域隔离；商店目录固定，团队效果由 ShopRevision + RequestId + 购买账本幂等提交，高风险事件由 EventId + RiskRevision + RiskLedger 一次性结算；服务器拥有候选、购买、奖励、核心生命和阶段，客户端只提交请求并消费只读 View。强化和消耗品优先映射现有 Ability/Modifier/属性公共入口，不能由地图蓝图直接改 Health、金币或经验。
- 已确定：完整版允许换英雄且个人强化保留；不适用英雄的专属卡休眠且不计入激活家族协同，切回后恢复；高风险事件可以增加下一波预算/精英概率并提高成功后的 Boss 奖励，采用发起人确认、队友可取消的简单流程，不使用共享遗物式投票；开发版必须提供服务器专用自定义种子/复现面板；核心、神殿、英雄、敌人和剧情使用原创内容。
- 待调优：团队消耗品价格、限购效果持续时间、个人金币付款的公平性、换英雄时英雄专属卡的休眠提示、完整版 23–27 分钟节奏和高风险事件补偿数值。改变奖励所有权、网络载荷、公开 View 或发布版本时，必须递增 DEF-001/后续实现 Spec 并重新审查。
- 回滚：删除 DEF-001 设计增量和导航/台账记录即可；不影响已验收 Combat、AI、经济、HUD 或小地图。

## 7. 模板适配风险

| 风险 | 现状证据 | 处理任务 |
| --- | --- | --- |
| Strategy 移动旧回调污染 | `AStrategyUnit::OnMoveFinished` 无 RequestId/Result 过滤，EQS 回调无当前实例/generation 校验 | ORD-002 |
| Controller delegate 重复 | `NotifyControllerChanged` 添加 delegate，但未显示解绑/幂等保护 | ORD-002/FND-006 |
| Stop 不是请求取消闭环 | 当前 `StopMoving` 只 `StopMovementImmediately`，未绑定 AI Move/EQS 终态 | ORD-002 |
| TwinStick 弹体绕过 Combat | `NotifyHit` 直接调用 `NPC->ProjectileImpact` | ADP-001 |
| TwinStick AoE 自建 Timer | `ATwinStickAoEAttack` 用 TimerManager 控制 gameplay | ADP-001/THK-001 |
| 模板 Actor Tick 默认打开 | StrategyUnit、Projectile、AoE 构造中启用 Tick | 各适配任务按时序类别关闭或限制 |

这些不是要求立刻修改模板；只有在接入 Combat 时才改造，避免在 M1 之前扩大代码范围。

## 8. 风险登记

| 风险 | 可能结果 | 预防/探测 |
| --- | --- | --- |
| GE 与 Runtime 双聚合 | UI/服务器数值不同、重复增益 | ADR-001、Attribute 测试、蓝图 API 限制 |
| Timer/Tick 分散 | DOT 漂移、catch-up 爆发、顺序随机 | 单 Scheduler、budget 指标、代码评审 |
| Hook 重入 | 护盾/驱散容器损坏或不确定 | 强引用快照、deferred queue、嵌套测试 |
| Ability/Projectile 生命周期耦合 | Ability End 后弹体丢失或悬空 | Spec snapshot、Subsystem owner、Fire-and-forget 测试 |
| Order 多点 Pop | 跳过队列、旧回调执行新命令 | 单状态机、Handle/generation、Result 分类 |
| ASC owning connection 错误 | owner 看不到完整 GE 或 RPC 归属异常 | ActorInfo/Owner 矩阵和 Dedicated 测试 |
| NavMesh 动态阻挡依赖异步完成 | Fissure 穿越/单位卡死 | 第一版物理 blocker + 主动 repath |
| 蓝图旁路公共管线 | 伤害/权限/日志不一致 | 封装 API、资产模板、评审/自动检查 |
| 缺少容量目标 | 完成后才发现服务器不可承载 | M7 前定预算、全程暴露计数器 |
| DefinitionId 漂移 | UI/日志/存档无法解析 | 资产校验、redirect/version、cook gate |

## 9. 决策记录模板

```text
ADR/GAP ID:
状态: proposed | accepted | superseded | deferred
上下文:
选择:
备选与未选择原因:
影响的 API/资产/网络:
迁移策略:
新增/修改测试:
影响文档:
决定人/日期:
```

## 10. 关闭标准

一个 Gap 只有同时满足以下条件才可标为关闭：

1. 结论已写入最主要的功能文档，而不只留在本登记表。
2. 路线图 Task/Issue 已引用该结论。
3. 若进入公共 API/数据/网络，已有兼容和迁移说明。
4. 最少一个正向和一个失败/边界测试体现结论。
5. 与已有 ADR 冲突时，明确 supersede 哪条规则。


### ADR-071：防守个人构筑与私有选择事务（2026-10-15）

- 状态：该条为 DEF-003 0.2 原计划；DEF-003 0.3 实际只完成服务器候选基底，UI/RPC/扣款和效果接线现由 DEF-004/ADR-072 落地，验证见 DEF-004。
- 新增 Augment/Pool schema 1、owner-only Choice schema 1、Defense RunView schema 2；只在新 Defense PC/模式启用。旧 Combat Release/Tag/Event/Formula/RNG v1 不变。
- Director 保存构筑和冻结种子/池，ChoiceComponent 只负责拥有者 RPC/私有投影；公共 Economy TrySpendGold 处理修订/余额/重入。候选和扣费一次提交，有界回执缓存，旧局/修订/生命/控制身份拒绝。
- 复现键不包含随机 RunId/Actor 指针或其他玩家抽取计数；使用固定 Seed、域、加入席位、窗口修订和槽位。完整种子只在开发服务器面板/日志可读，客户端只有摘要。
- 死亡窗口超时选默认、效果暂存复活；核心失败作废。新池为空保留 Phase 1 固定卡，回滚到 d7c9809 的文件组不触碰其他用户文件。


### ADR-072：DEF-004 私有强化选择、重抽与生效（2026-10-16）

- 状态：accepted/implemented（DEF-004 0.1 已完成本地实现与验证，待用户验收）。
- Choice/RunView 独立防守 schema 2；新 Defense PC 的复制组件只向拥有者投影。请求包含局/修订/生命/控制/经济身份，复用共享连接预算与 32 条完整载荷缓存。旧 Combat Release、Event/Tag/Formula/RNG v1 不变。
- Director 冻结数据并控制 15 秒窗口、超时默认、死亡待生效和失败作废；选择只走公共 Modifier。新服务器 TrySpendGold 校验金额/修订/余额/重入，重抽 60/100 两次且无替代卡不扣款。
- 开局等待/种子输入仅开发本地服务器，Shipping 随机自动；Dedicated 开发固定种子仅命令行，不接收客户端种子。原型卡片最多持有一次，复杂机制卡/叠层内容/团队商店另一步。
- Pool 为空保留固定卡；旧 Demo PC 不改，新蓝图复制输入并继承 Defense PC。回滚只撤本步增量，保留 DEF-003 工作区。


### GAP-030：既有动态 GameplayEffect 定义的 owning-client 诊断

- 状态：保留后续公共 Modifier 网络改造；本步不得通过切换玩家 ASC 复制模式或旁路属性写入掩盖。
- 事实：`Saved/DEF-002/network-victory-2/client-1.log` 已出现 `PostReplicatedAdd Received ReplicatedGameplayEffect with no UGameplayEffect def`。DEF-004 使用同一公共 Modifier 后仍出现；`CombatModifierComponent::ApplyNewModifier` 构造瞬态无限 GE，而 owning-client Mixed 的完整 GE 定义引用不能解析。
- 验证边界：服务器 Modifier registry、UnitView 和 AttributeSet 复制为本步实际消费的权威结果，专用网络探针逐项对照 2 名英雄 × 12 属性；不把此诊断说成“完整 ActiveGE 复制通过”。后续新建独立 Spec 处理静态/稳定 GE 定义及原有内容兼容，不在强化界面任务中重构公共 GAS 内核。

DEF-004 验证结果：Editor/Server/Client、188 项分进程回归、81 定义资产、721 包 Windows cook、真实按钮/导航 PIE、双客户端重放/余额/效果及 24 属性对照通过；GAP-030 诊断保留，打包 exe/4 人容量/长期平衡不在本步通过范围。


### ADR-072：团队商店原型事务与内容边界（2026-10-16；DEF-005）

Phase 2 先交付六件独立属性/修复商品的团队限购、个人付款、共享账本、Owner RPC 和显示；正式 §6.3 护盾/条件增伤/援军效果属于 Phase 3。原型不复用正式六个 DefinitionId。目录 schema 2 通过 UE API 升级未交付的占位资产；每件属性商品引用独立、永久且无自定义 Hook 的 Modifier，由 Director 按下一波至最后覆盖波清场统一撤销。每项只限购一次，购买者掉线仍保留其对其他成员的效果，终局保留已购记录、撤销属性，World teardown 清理。稳定身份为目录 PrimaryAssetId 加 ItemId 键，网络不发送定义 UObject。具体准入与失败边界见 [DEF-005](../Specs/DEF-005-defense-team-shop.spec.md)。


### ADR-073：复用原商店与获得即使用（2026-10-16；DEF-006）

用户指出 DEF-005 应复用已有商店。六件防守消耗品迁为 UCombatItemData，由 CombatShopData 独占分类/价格，走既有 Economy RPC 和金币事务。物品新增默认关闭的 bUseOnAcquire 与同步获得效果配置；购买、拾取、奖励共用唯一物品登记表领取入口，不占槽位，成功即消耗、失败保留输入。团队行为只负责限购、公共战斗效果与逐波到期，删除第二套商店 UI/RPC/目录。

客户端只读可用性与请求上下文进入原经济投影；EconomyPresentationSchemaVersion=4、ContractVersion=5、ReleaseId=combat_v5_acquisition_rc1，旧物品内容版本仍为 2。三端必须一起更新；不新增 GameplayTag/Event 语义，不部署发布。详情、迁移与验证见 [DEF-006](../Specs/DEF-006-reuse-shop-use-on-acquire.spec.md)。ADR-072 的独立目录/组件选择被本决策替代，历史 DEF-005 验证记录保留。


### ADR-074：核心救援道具与护盾投影（2026-10-16；DEF-007）

正式神殿屏障/应急石分别使用新 CombatItem DefinitionId，替换防守目录中的铁甲/固定修复原型，旧资产留存不再售卖。屏障在整备阶段购买即生效，覆盖下一波至清场；以购买时核心最大生命8%为快照，不随上限变化增加。公开 DamageBlock Hook 在抗性后吸收物理/魔法/纯粹伤害，HPLoss与免疫沿用公共规则；应急石通过公开 Heal 请求15%最大生命修复。

Effect配置兼容新增，核心盾Runtime不修改现有魔法盾示例或战斗内核。Director持有核心精确句柄，耗尽/清场/终局移除，无新Timer；买家断线不撤盾。Defense RunView 独立 schema 2→3 增加只读护盾余量，三端同时更新；公共Release/Tag/Event/Formula/经济schema保持不变。原生资产迁移、验证和回滚见 [DEF-007](../Specs/DEF-007-defense-core-rescue.spec.md)，本地L1内容扩展，未部署。


DEF-007 0.2 追加：同步获得效果在应用 Modifier 前读取组件是否正在遍历 Hook；该兼容只读 C++ 查询不改变既有延迟队列。防守领取此时拒绝，避免已退款却排队生成效果；反伤嵌套反例保留回归。

### GAP-031：公共 Damage 跨 World 误用的事务归属（2026-10-16）

- 状态：待独立内核修复；本地可信调用者误用边界，不是客户端新入口。
- DEF-007 core-green1.log 的夹具主动以世界F的 DamageSubsystem 请求伤害世界G的核心：护盾被耗尽，后续G世界伤害未形成正确死亡凭证。入口目前没有显式校验 Source/Target 与 Subsystem 同 World，事件与属性回报归属可能不一致。
- 防守商品仍检查玩家/收件人同World，效果只操作导演自身核心；正式实现不会发送上述请求。本轮移除不合法夹具调用，保留正常死亡、跨World领取拒绝测试；后续独立Spec统一Damage/Heal/Modifier可信API跨World边界和全量回归。


### ADR-075：援军次数与特定复活生命绑定（2026-10-16；DEF-008）

正式援军印记使用新CombatItem身份，替换防守目录的魔法加固原型；原获得即用/经济RPC与每局限购不变。购买后按已登记英雄的真实死亡委托顺序分配两次救援，排除购买时已Dead/Dying的生命；重复或旧生命事件不重复消耗。预留绑定Hero和DeathLife，仅在DeathLife+1复活时领取20%最大生命、4秒护盾，已预留后断线不退团队次数。复活延迟按死亡事件时刻计算max(0,原等待−3)，仍由原Monitor执行安全复活。

共享UDefenseShieldRuntime承接已有吸收逻辑；UDefenseCoreShieldRuntime保留类身份作为子类，旧屏障资产无需重命名。援军状态复用原ShopOffer.Status、英雄护盾复用ModifierView，不新增网络字段/schema或伤害结算旁路。Director负责两类生命周期委托与精确句柄清理，公开调用后重查玩家记录，避免重入使引用失效。见 [DEF-008](../Specs/DEF-008-defense-reinforcement.spec.md)。


### ADR-076：Phase 2 正式团队效果、24卡池与英雄替换（2026-10-17；DEF-009）

Phase 2六件团队商品继续使用原CombatShop/获得即用；正式补齐追猎号角（攻城/刺客目标+15%）、灵巧回路（CooldownReductionPct+5%，受40%上限）和破城铭文（Boss目标+12%）。条件目标使用稳定Unit DefinitionId角色片段，未知目标不触发；Phase3敌人资产沿用相同定义规则接入，不新增Damage入口。

强化池由12张原型扩展为24张，新增四张终局卡；终局卡最早第3波候选，家族计数仍由服务器IsEligible判断，个人卡每张最多一次。Build HUD显示拥有卡的家族计数，客户端只消费CardView。服务器替换英雄保留PlayerRecord的OwnedAugments、ChosenCards、ChoiceResults、PendingCards和经济/团队账本，通用卡重新应用，不兼容卡休眠；Controller继续使用既有SetCommandedUnitAuthority与BindingGeneration。

本步不引入第二英雄、攻城/刺客生产内容、事件、装备或Boss多阶段技能；Phase2的三构筑Boss回归使用现有原型Boss胜负入口。实现和验证见 [DEF-009](../Specs/DEF-009-defense-phase2-completion.spec.md)。

### ADR-077：防守刺客复用角色树与可选低生命评分（2026-10-17；DEF-011）

采用兼容新增：AI Profile的MissingHealthWeight默认为0，启用时同定义优先级候选分数增加`clamp(1-Health/MaxHealth,0,1)*MissingHealthWeight`；仅在公共Targeting范围/LOS许可的当前观察中读ASC。失去观察的记忆不读取实时生命，持续攻击不强制换敌。默认0保持旧评分，AIProfileVersion仍1/2，不改变发布契约和网络schema。

Defense SpawnGroup增加可空RoleProfile，初始化复制冻结其配置。空值仍由导演提交原路线Move/Attack；非空则导演设置职责后只启动角色树，禁止UnitData同时配置AIProfile，也禁止Boss组使用本入口。新刺客Profile复用Lane树，当前英雄定义优先级高于核心，按低生命比例择敌，8秒或越界后归位。固定英雄身份优先级在以后新增英雄时须同步配置；未知友方定义作为普通敌对候选处理。新字段可逆、旧资产默认兼容，无新增权威计时、RPC或Damage入口。

DEF-011 0.2追加：RouteEngagementMinPriority默认-100保留旧候选；刺客巡线设10，仅英雄可中断航点。完点后允许优先级0核心，避免核心先入视野导致反复攻核归位而永远不能抵达后排。

### ADR-078：神殿守卫技能包与受限开局选择（2026-10-17；DEF-012）

守卫独立CombatUnit/AbilitySet与技能身份，不替换游侠定义。Q/E在公开Ability执行点中以公共Targeting许可后过滤扇形、公开Damage/Modifier结算；W沿用DataDriven ApplyModifier与有限护盾。可选shield_fraction/shield_cap_fraction默认0，旧shield_amount配置不变；有上限声明时，同族新盾吸收余量按当前ASC最大生命及已有盾余量限制。被动三次有效直接普攻受击武装，下次直接普攻给盾，按Root去重、4秒内置冷却，死亡重建Intrinsic。

Boss失衡采用数据驱动Modifier，Boss自持来源使多人同池叠层，首次一层起固定8秒窗口，4层触发2秒Stunned及总8秒失衡保护。没有新Tag/事件或攻击阶段API，不宣称Boss三阶段已实现。

开局选择为Defense独立owner-only组件schema1，只接收改用配置白名单守卫的意图，复用连接限频/重放；初始准备、空构筑/库存/经验及未购团队效果才允许成功一次。服务器安全出生并初始化，控制绑定成功后才提交记录、改委托和销毁旧角色；失败保留旧角色与金币。已有候选随身份修订且保留原截止。此入口不调用旧ReplacePlayerHero，不扩大为完整中途迁移；DEF-009相关Gap保持。发布契约、Combat内容/事件/公式和既有Choice/Run/HUD schema不变，新增身份与字段可逆兼容。


### ADR-079：DEF-013 防守事件原型与风险确认（2026-10-19）

事件接入现有 Defense Director/StateTree/Scheduler 边界，不增加地图蓝图 Timer、独立金币或核心生命旁路。第2/4波清场后进入 `EventChoice` 阶段；默认事件配置为空时由服务器创建两个固定原型，正式资产可通过 `UDefenseEventData` 覆盖。事件请求使用 `RunId + PhaseGeneration + EventId + EventRevision + RequestId + BindingGeneration + LifeGeneration`，服务器拥有安全/风险/取消/超时终态和 `FDefenseRiskLedger`，客户端只复制标题、说明、状态和截止时间。

风险选项先进入10秒可取消确认；任一当前玩家可取消，无操作采用安全选项。安全奖励使用既有 Economy/Progression/Heal 入口；风险成功只对下一波增加一名基础敌人、清场后发放延迟奖励并增加Boss奖励倍率，均按一次性账本结算。Defense RunView schema 从3升级为4，Combat Release/Tag/Event/Formula/Economy schema不变；旧RunData通过默认原型事件保持可启动，测试夹具可显式关闭原型事件。

DEF-013专项Automation、真实渲染PIE、正式资产冷回读和两轮WindowsServer/WindowsClient Dedicated双客户端验证均通过；长时间Soak/Perf未执行。用户已于2026-10-21确认DEF-013验收完成。

### ADR-079 追加：独立 Target 必须使用平台正确的 Stage（2026-10-21）

0.3 的独立 Target 风险定位为启动方式错误：直接运行仓库 Binaries 下的 Server/Client，却没有 WindowsServer/WindowsClient 的 Cooked 容器和预生成 AssetRegistry；非编辑器包摘要的裁剪布局不能直接读取 raw Content。没有修改 FBufferReader、放宽版本校验或增加读取缓冲。

0.5 使用源码5.8.2构建独立Server/Client，安装版兼容5.8.2 Cooker分别生成WindowsServer和WindowsClient并Stage/Pak/Archive。真实staged Server加载AssetRegistry后为NetMode1，两客户端为3；两轮六波事件同局RunId、取消/确认、1940金币和Owner拒绝均通过。由 `Tools/package_defense_targets.ps1` 与 `Tools/run_defense_targets.py` 固化构建/分发/启动顺序、新日志目录和进程清理。历史raw启动失败保留为Red证据，额外风险已关闭；不把Game Target的单机通过替代Dedicated证据。

### ADR-080：普通装备的同步领域购买策略（2026-10-21；DEF-014）

DEF-014 已新增默认空的 UCombatItemShopPolicy：QueryOffer 只读构造既有 ShopOffers，OnPurchased 只在原经济/库存互斥内记录成功购买配额，不写金币、实例或发布事件。仅可购买的普通无配方叶子允许配置；原商品无策略时行为不变。沿用现有领域上下文、拥有者修订与32条完整回执，不改变网络字段/发布schema。

防守适配要求同局登记英雄、存活核心、整备阶段、每种九格限一件、每玩家每PhaseGeneration整备窗口最多两次成功装备购买。死亡/出售不退配额，团队即用不占配额，注销/Shutdown释放记录。四件新物品身份为 CombatItem:item_iron_bow/item_swift_string/item_stone_guard/item_hunter_scope，效果通过原公开Modifier/ASC，其他四件与完整八件收口后续。回滚移除原Defense目录新分类及本步文件组，不修改已验收团队消耗品。成功回调使用稳定付款人PC和已接受Offer的阶段代次；同步库存观察者销毁英雄或推进阶段仍只记原窗口一次。

ADR-080 0.2：MaxOwnedQuantity 默认0保持旧内容不限量，普通授予/拾取与经济购买共用真实九格计数，锁定与背包仍计入，策略物品禁止作为配方组件；防守四件限额1。

DEF-014首批四件已通过248非PIE、3商店渲染PIE、154资产校验、三Target和WindowsServer/WindowsClient Cook/Stage/Archive及独立Dedicated双客户端。用户于2026-10-22确认验收成功并授权本地提交，不推送；长期平衡、四人容量及其余四件留后续，见 [DEF-014](../Specs/DEF-014-defense-equipment-foundation.spec.md)。

### ADR-081：条件冷却策略与本人核心修复归属（2026-10-22；DEF-015）

已以兼容可选内嵌CombatCooldownPolicy（自身契约1）扩展AbilityData，空值保持原ASC全局冷却式；防守英雄Q/E读取有效ActiveGE绑定Runtime的defense_qe_cooldown_reduction，与全局CDR相加后按内容配置上限40%，W不读取条件项。策略在唯一冷却提交点取值，已开始冷却不重排，物品主动保持既有路径。不修改原公共API签名、全局Formula/Release或网络schema。

修复透镜只通过公共PreDealHeal为本人对当前Run核心的修复增20%；团队应急石把已校验接收英雄作为直接治疗来源，明确安全事件把当次请求英雄作为来源；队友/取消/超时等自动修复不借其他英雄透镜。源身份调整保留SourceContext与公共增幅/真实Health上限，回调结束Run后不得重建事件账本。两件稳定身份为CombatItem:item_mana_sigil、item_repair_lens，价格240/200，共用原整备两件配额。回城与折扣在后续独立切片评审；本步先通过Spec0.1计划审查与Build Gate，再实施行为文件。

DEF-015已通过三Target、255非PIE/4原商店渲染PIE、158资产及WindowsServer/WindowsClient双平台Cook/Stage/Archive、独立Dedicated双客户端，待用户验收。新增装备回归与失败边界见 [DEF-015](../Specs/DEF-015-defense-utility-equipment.spec.md)，未自动提交；GAP-030动态GE定义复制诊断仍保留，拥有者冷却快照与核心Health/库存复制为实际验证结果。

### ADR-082：商贾印统一报价与独立拥有者价格投影（2026-10-23；DEF-016）

商贾印作为第七件普通装备，240金币，shop_discount_fraction=0.05；沿用BoundUnit/每款九格限1/每整备窗两件配额。所有购买（含获得即用、配方缺失叶子补购）及防守重抽由原Economy/Choice服务器入口在每笔费用总额上一次floor。有效参数相加封顶20%，以最近万分比量化并用整数商/余数避免浮点少扣一金和乘法溢出；无参数仍0折扣。原退款窗口返实付，窗口外出售继续原目录价及比例，本关卡原有50%出售比例低于最便宜80%购入价，不产生买卖套利；未来提高出售比例超过80%需重新审查。节俭卡资产与回城另立切片。

新增独立FCombatShopPriceView自身schema1（owner-only，非请求），保持冻结FCombatEconomyView/Release/Formula/Tag/Event版本；不承诺混版本可执行文件联机。当前英雄Modifier委托驱动价格投影，主控切换/EndPlay解绑；服务器每笔结算重新读取有效效果，客户端按ShopDefinitionId解析AssetManager目录及库存构造只读报价，不依赖服务器ShopData或Runtime。重抽已有发布比较覆盖价格和次数，金币不足时不提交候选或次数。计划0.2事前重审见 [DEF-016](../Specs/DEF-016-defense-merchant-seal.spec.md)。

DEF-016最终验证：三Target、9专项/264非PIE/5渲染PIE、59工具、160定义0错误/0警告、813包双平台Cook/Stage/Archive以及归档独立Dedicated双Client通过；RunId=A42EEEBA455EBEE6510C42857659AF80，逐端折扣/190报价/57与95重抽/实付582与重放一致，归档与源码exe摘要匹配。0.1先实现后测试的偏差保留，0.2真实Red/Green和F2结果见Spec；用户尚未验收，未提交或推送，GAP-030未关闭。

### ADR-083：防守回城与死亡参数快照（2026-10-23；DEF-017）

基础回城通过UCombatGameplayAbility/原Order RPC，以防守共享技能组追加到当前空槽（游侠W、守卫R），保留原技能顺序；零蓝耗，3秒引导/30秒CD，CD在开始引导时提交且内容策略固定不缩减，移动或真实扣血打断。信标150金币、普通占槽/绑定/限1/原两件整备配额，公共参数分别缩短引导1秒与复活等待2秒；引导启动冻结、最少1秒，死亡与援军减时相加，最少0并仍经原安全复活Monitor。

公共原生扩展各自身契约1：Ability增加可重写的引导时长与到期前校验，默认保持原定义/允许；Motion增加权威安全同步传送，检查生命/通道/胶囊阻挡并在日志回调后复核，经CharacterMovement更新，不在技能里直接改Transform；Lifecycle在Alive转Dying前只冻结有效Runtime参数并绑定生命，OnDied读取、复活清空，不能事后读已清理装备。冻结网络USTRUCT、Tag/Event/Formula/Release版本不变，无新RPC或客户端落点/数值输入，三端使用同版本。完整范围、反例、回滚和事前F1见 [DEF-017](../Specs/DEF-017-defense-return-beacon.spec.md)。

DEF-017交付补强：同步Motion开始日志/本地观察点之后重新检查Owner及完整业务资格；回城的ValidateBeforeCommit复核Run/Binding/Life/状态/原点，换控或导演Shutdown不再继续改位置。两个真实PIE反例均关闭；最终9专项/273非PIE、8渲染PIE、59工具、164定义0/0、三Target/818包双平台归档与Dedicated双Client通过，RunId=26B8A72143977E6ED60ACDA318BFE815、源码/归档exeSHA一致且全退出。用户已于2026-10-24验收完成，待本地提交，未推送；GAP-027/030未关闭，长期/3–4人容量后续。
