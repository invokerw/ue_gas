# 32 M8 公共技能扩展与迁移指南

## 1. v1 冻结扩展面

新技能应通过以下公开层组合，不修改 Damage/Heal、Scheduler、Targeting、Order、Attack 或 Projectile 的内部 registry：

| 需求 | 公共扩展点 | 约束 |
| --- | --- | --- |
| 稳定身份与配置 | `UCombatDefinitionData` 派生的 Unit/Ability/Modifier/Projectile/AbilitySet DataAsset | `lower_snake` DefinitionName；网络和日志只传 PrimaryAssetId |
| 施法阶段 | `UCombatGameplayAbility` 与 `ReceiveSpellStart/ReceiveChannelTick/ReceiveChannelFinish` | 目标由服务器重算；Cost/Cooldown 只走统一 commit |
| 有状态 Buff/Debuff/被动/法球 | `UCombatModifierRuntime` BlueprintNativeEvent Hook | 不直接写 Health；Hook 中结构修改走 deferred/public API |
| 技能动作 | Ability Data 的公共 Action、Projectile Impact Action、Thinker/Aura/Motion 公共请求 | 只使用服务器权威入口与不可变 Spec 快照 |
| 表现与 UI | GameplayCue、`UCombatUnitViewComponent`、Projectile Presentation | 只消费权威 View/事件，不反向驱动 gameplay |

`Combat.Release.M8.PublicExtensionSurface` 会通过反射检查 DataAsset 继承关系、核心 Ability/Modifier 蓝图事件和中文 `DisplayName/ToolTip`，防止重构误删扩展入口。

## 2. 推荐创建流程

1. 从对应 `UCombatDefinitionData` 派生类型创建 DataAsset，填写唯一 DefinitionName、版本化参数与目标规则。
2. 优先用 Ability Data 的公共 Action 表达 Damage、Heal、Modifier、Projectile、Thinker 或 Motion。
3. 只有需要自定义阶段控制时才派生 `UCombatGameplayAbility`；在公开蓝图事件中编排，生命周期交给基类。
4. 只有需要有状态 Hook 时才派生 `UCombatModifierRuntime`；在 `OnCreated/OnRefreshed/OnDestroyed/OnThink` 和伤害、治疗、技能、攻击 Hook 中实现最小逻辑。
5. 使用 `25-M6-Skill-Template-Checklist.md` 检查旁路、身份、时序、清理、中文说明和自动化。
6. 为技能添加至少一条成功事件序列和一条中断/失败序列；涉及异步对象时增加旧句柄与 teardown 用例。

## 3. 禁止旁路

- 蓝图或技能代码不能直接修改 Health，必须调用 Damage/Heal 公共入口。
- 不能用 Actor Timer 驱动 DOT、引导、攻击点或 Aura 协调，必须使用 Combat Scheduler。
- 不能直接 `SetActorLocation` 实现强制位移，必须使用 MotionComponent。
- 客户端预测视觉不能 sweep、Damage、ApplyModifier、Finalize Attack 或发送权威事件。
- 不能复制 `UCombatModifierRuntime` 或 DataAsset UObject 指针；UI 使用 Unit/Modifier View 与本地 DefinitionId 解析。
- 不得自行比较 TeamId、生命状态或距离来替代 Targeting/TeamSubsystem 公共规则。

## 4. 事件序列冻结

| 流程 | v1 稳定顺序 |
| --- | --- |
| Ability | CastStarted → SpellStarted/SpellBlocked → ChannelEnded（如有）→ OrderReleased → Interrupted/Ended |
| Damage | PreDeal → PreTake → resistance → Block/Shield → ActiveGE transaction → PostDeal/PostTake → Death（如归零） |
| Modifier | Apply/Refresh → OnCreated/OnRefreshed → Think（可选）→ OnDestroyed → Removed event |
| Attack | Request/record → attack point → Projectile 或近战 impact → public actions → Landed/Failed exactly-once → ready |
| Projectile/Thinker/Aura/Motion | registry Start → 0..N 更新/命中/协调 → 单一 Finish → delegate/log → Actor/child cleanup |

新增回调不能改变这些既有相对顺序；确需改变时必须提升对应契约版本并迁移自动化与消费者。

## 5. 版本与迁移

| 变更 | 必须执行 |
| --- | --- |
| DefinitionName 重命名 | 保留旧 ID，添加 `FCombatDefinitionRedirect`，递增内容版本并通过 cook validator |
| Native GameplayTag 改名或语义改变 | 递增 GameplayTagSchemaVersion，保留旧 Tag 的废弃/重定向迁移期并迁移资产与事件消费者 |
| DataAsset 字段兼容新增 | 提供安全默认值，保持内容版本；若旧资产语义改变则递增 |
| 公式、clamp、取整或随机算法改变 | 递增 FormulaVersion 或 RngAlgorithmVersion，保留旧版本读取/重放边界 |
| CombatLogRecord 不兼容改变 | 递增 EventSchemaVersion，提供离线迁移器，旧消费者必须明确拒绝 |
| 发布契约字段或延期边界改变 | 递增 ContractVersion，新增 ADR 与独立 Gate，不能只修改文档布尔值 |
| 蓝图公开函数改名或删除 | 提供 DeprecatedFunction/重定向和迁移期；更新 `PublicExtensionSurface` 测试 |

运行时可通过蓝图节点“获取战斗发布契约”和“校验战斗发布契约”读取并检查当前边界；候选发布若发生版本漂移会在 M8 自动化和 Dedicated `M8ReleaseContract` 日志中失败。
