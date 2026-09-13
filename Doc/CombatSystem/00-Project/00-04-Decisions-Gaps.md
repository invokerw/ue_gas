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
| GAP-017 | 明确延期（ADR-041；成长部分由 ADR-049 兼容扩展） | 物品、背包、技能点、天赋、经验和经济 | 物品、背包、天赋和经济仍不属于 v1；经验与技能点按 ADR-049 接入 post-v1 成长组件，发布契约仍固定 `bItemsAndEconomy=false` | post-v1 |
| GAP-018 | 已关闭（ADR-040） | 目标容量/帧/带宽预算和池化触发阈值 | 预算、采样边界与优化触发规则见 [90-13 §7](../90-History/90-13-M7-Network-Observability-Decision.md#7-容量预算关闭-gap-018-的目标值)；64 Unit/256 Modifier 双客户端 soak 通过，验收证据见 [90-14](../90-History/90-14-M7-Acceptance.md) | 2026-08-27 / PERF-701 |
| GAP-019 | 已关闭（ADR-039） | Combat Event schema 版本、存档/回放边界 | schema v1、环形诊断与明确不支持的 replay 边界见 [90-13 §6](../90-History/90-13-M7-Network-Observability-Decision.md#6-事件调试和回放边界关闭-gap-019-的目标值) | 2026-08-27 / OBS-701 |
| GAP-021 | 已关闭（ADR-038） | RPC token bucket、批量命令上限和重复 request id 窗口 | ownership、20/s + 32 burst、8 Order/4096 bytes、128 RequestId 窗口及失败 Tag 见 [90-13 §3](../90-History/90-13-M7-Network-Observability-Decision.md#3-order-rpc-安全基线关闭-gap-021-的目标值) | 2026-08-27 / NET-002 |
| GAP-022 | 明确延期（ADR-041） | Ability/移动本地预测和回滚 | v1 只保留 Projectile 纯视觉 PredictionKey；完整 PredictionKey owner、rollback 与 Cue reconcile 需独立 ADR/schema/Gate，发布契约固定 `bGameplayRollback=false`；评估见 [90-15 §3](../90-History/90-15-M8-Release-Candidate-Decision.md#3-rel-004-预测评估) | 2026-08-27 / post-v1 |
| GAP-025 | 已关闭 | 暂停、global/custom time dilation 语义 | `UCombatSchedulerSubsystem` 使用 World game time；real-time UI 不进入 Scheduler；时序/catch-up/budget/teardown 自动化通过 | 2026-08-24 / FND-007 |
| GAP-026 | 已关闭 | 直接 Possess Demo 允许 owning client 参与单位移动，客户端 Pawn 解穿透可产生服务器未认可的视觉位移 | ADR-043 已落地：服务器 Combat AIController + Command Pawn + 全客户端 SimulatedProxy + 单一 Detour Crowd；三档 Dedicated 双客户端对撞、RPC、64/256 容量和 teardown Gate 见 [10-10](../10-Architecture/10-10-Server-Authoritative-Movement-Kickoff.md) | 2026-09-02 / SAM-008 |
| GAP-027 | 待处理 | 交互式 Editor 内运行既有完整 Automation 后切地图，部分技能测试的 CDO 持有临时 AbilityData，导致测试 World 无法 GC | HUD-LOG-001 验证时发现；引用链指向 Default__CombatSelfHealAbility、CombatMeatHookAbility 等既有测试配置，不含日志组件。当前以独立进程执行全量 Automation、干净 Editor 执行 PIE 隔离；后续为修改 CDO 的测试增加作用域恢复。证据 `Saved/CombatLog/Editor-UI.log` 的 World Memory Leaks 引用链 | 后续测试设施维护 |

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
