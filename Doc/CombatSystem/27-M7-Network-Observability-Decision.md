# 27 M7 联机、可观测性与容量决策

## 1. 目标与非目标

M7 将 M0–M6 的服务器权威战斗链路收口为可在 Dedicated Server、两个客户端和长期运行场景中验证的产品边界。第一版不引入伤害、Modifier、移动或命中的客户端预测，也不承诺录像回放；客户端预测只允许创建可丢弃的输入反馈与弹体视觉。

## 2. ASC 复制矩阵

| Unit | Actor Owner | GameplayEffect ReplicationMode | UI 数据源 |
| --- | --- | --- | --- |
| 玩家拥有单位 | 对应 `APlayerController` | Mixed | 统一读取 Combat Unit View；Owner 可额外诊断完整 ActiveGE |
| 中立/纯服务器 AI | 无玩家 Owner | Minimal | Combat Unit View、必要 Attribute 与 Tag |
| 自动化调试单位 | 显式 Full | Full | 只用于测试，不允许作为产品默认值 |

`OwnerActor=AvatarActor=Unit` 保持不变。`Actor::Owner` 只建立 owning connection，不替代 GAS ActorInfo。Automatic 策略在服务器根据是否存在 PlayerController Owner 选择 Mixed 或 Minimal；显式 Full 只允许测试资产配置。

## 3. Order RPC 安全基线（关闭 GAP-021 的目标值）

- RPC 挂在被 owning connection 拥有的 Unit 上，服务端再次验证 `Unit.Owner == PlayerController`。
- 每个请求包含正数、单调语义无关的 `RequestId` 和最多 8 个同 Unit Order；估算载荷上限 4096 bytes。
- 每连接 token bucket：持续 20 request/s，突发容量 32；拒绝请求不执行任何 Order。
- 每连接保存最近 128 个 RequestId；合法格式的首次请求在执行前进入窗口，重复请求返回稳定失败且不得产生第二个 Order。
- FVector、Target、AbilitySpec、Unit life generation 和行为范围继续由 Order/Targeting/ASC 公共入口复核。
- 拒绝原因使用独立 Native GameplayTag，并进入安全统计；客户端提交 Amount、ModifierData、Damage flags 或 Finish 结果仍无入口。

## 4. Unit 与 Modifier View

- `UCombatModifierRuntime` 永不复制。
- `FCombatModifierView` 只包含稳定句柄、DefinitionId、StackCount、ServerEndTime、Debuff 和 Dispellable 标记，并通过 FastArray 增量复制。
- `FCombatUnitView` 投影 Unit DefinitionId、TeamId、LifeGeneration、LifeState、Health/Mana、当前 Ability DefinitionId 与服务器起止时间。
- Owner 与非 Owner 使用相同 View API；View 只服务 UI/表现，不允许战斗系统反向读取。
- 缺失 Definition 由 `MissingDefinition[...]` 占位文本安全降级，不影响服务器结算。

## 5. Projectile 视觉协调

- 服务器 Projectile Actor 继续持有唯一 Handle、DefinitionId、movement 与结束权威。
- 可选 `PredictionKey` 只关联客户端本地视觉；服务器 Actor 首次复制时销毁同键预测视觉并注册为唯一服务器视觉。
- 服务器 Actor 结束时客户端移除协调记录；预测视觉永不触发 sweep、Damage、Modifier 或 Finish。

## 6. 事件、调试和回放边界（关闭 GAP-019 的目标值）

- Combat Event Schema v1 保持 `EventId/RootEventId/Depth/Sequence/ServerTime/DefinitionId/数值槽/FailureTag`，不保存 Runtime UObject 指针。
- 当前版本提供 World 内环形诊断缓冲、按 RootEvent 展开、Unit dump、debug draw 开关与运行时 metrics。
- Shipping 默认不保留完整高频 payload；M7 不实现存档录像或确定性 replay。未来 schema 不兼容时必须提升版本并提供离线迁移器。

## 7. 容量预算（关闭 GAP-018 的目标值）

目标平台为 30 Hz Dedicated Strategy Server，NullRHI 基准场景：

| 指标 | M7 预算 |
| --- | ---: |
| 同时存活 Unit | 64 |
| Active Modifier Runtime | 256 |
| Active Projectile | 128 |
| Active Thinker | 32 |
| Active Aura / child | 16 / 256 |
| Scheduler callbacks/frame | 256 全局、64/Owner |
| 稳态 overdue/budget deferral | 0 |
| Combat Event 环形缓冲 | 512 |
| Server frame | p95 <= 33.34 ms（30 Hz 精确帧长），p99 <= 50 ms |
| 单客户端战斗相关发送带宽 | <= 256 KiB/s |
| Order RPC | 20/s 持续、32 burst、8 Order/包 |

正确性 Gate 与性能 Gate 分离；超预算不能通过放宽 exactly-once、稳定顺序或安全校验解决。Pooling、relevancy 和更紧凑序列化只在 profiler 证据表明具体 owner 后引入。

## 8. G7 验证入口

- Editor Automation：复制策略、安全拒绝矩阵、FastArray View、Projectile reconcile、事件展开、资产报告和容量/teardown。
- Source UE Server/Client Target：证明运行时类型没有 Editor-only 依赖。
- Dedicated Server + 2 Client：玩家 Unit Mixed、AI Minimal、两客户端 View、RPC ownership/replay/rate limit 与 Projectile 唯一视觉。
- World Automation 实际创建 64 个 Unit、挂载 256 个 Modifier，并验证销毁后的 registry/Runtime 清理。
- 独立 Dedicated Server + 2 Client 容量 soak 在 64 Unit/256 Modifier 下运行至少 60 秒；使用提高进程帧率上限的方式移除 Installed Editor `-server` 的等待时间，预算仍固定为 30 Hz 的 p95 33.34 ms / p99 50 ms，不降低正确性与安全 Gate。
