# 30 M8 候选发布决策

## 1. 候选发布范围

M8 将 M0–M7 的服务器权威战斗系统冻结为 `combat_v1_rc1`。本阶段不扩展玩法范围，只完成全量回归、生命周期审计、基于 profiler 的性能决策、预测边界和公共扩展面冻结。

代码中的唯一机器可读入口为 `FCombatReleaseContract` / `UCombatReleaseContractLibrary`。蓝图、自动化、Dedicated 日志和发布工具都从该入口读取，不再分别维护版本号。

| 字段 | v1 冻结值 | 兼容要求 |
| --- | --- | --- |
| ContractVersion | 1 | 修改契约字段语义必须递增 |
| ReleaseId | `combat_v1_rc1` | 候选发布日志和验收报告统一使用 |
| CombatContentVersion | 1 | DefinitionId 重命名必须添加 redirect；不兼容内容变更递增 |
| GameplayTagSchemaVersion | 1 | Native Tag 改名或语义不兼容时递增，并提供废弃/重定向迁移期 |
| FormulaVersion | 1 | 数值公式、限制或取整语义改变时递增 |
| RngAlgorithmVersion | 1 | keyed RNG 哈希或输入编码改变时递增 |
| EventSchemaVersion | 1 | 日志字段不兼容时递增并提供离线迁移 |

`Combat.Release.M8.ContractAndDeferredBoundaries` 会比较发布契约与代码常量，版本漂移直接使 Gate 失败。

## 2. 功能边界

v1 承诺：

- Damage、Heal、Modifier、Ability、Attack、Projectile、Thinker、Aura、Motion 和 Order 只由服务器完成 gameplay 结算。
- 客户端只提交受 ownership、载荷、限频和重放窗口保护的 Order 请求。
- Projectile `PredictionKey` 只协调可丢弃视觉；服务器 Projectile Handle/Actor 仍是命中、伤害和 Finish 的唯一身份。
- 新技能可依赖公共 `UCombatDefinitionData` 派生 DataAsset、`UCombatGameplayAbility` 蓝图事件、`UCombatModifierRuntime` Hook 与公共动作入口。

v1 明确不承诺：

- Ability、移动、伤害或 Modifier 的客户端预测与回滚。
- 跨进程、跨版本的完整确定性录像重放。
- 召唤物、幻象的 Owner/ASC/CommandingController/Team 继承语义。
- 物品、背包、技能点、天赋、经验与经济系统。

上述延期项都进入发布契约布尔字段和自动化，不允许被单个技能或蓝图静默启用。

## 3. REL-004 预测评估

当前 Projectile 表现协调已经具备最小 `PredictionKey` 流程：客户端登记本地视觉，服务器 Actor 首次出现时用同键替换，本地对象不能 sweep、Damage、ApplyModifier 或 Finish。重复服务器身份由表现 registry 幂等拒绝。

完整 gameplay 预测至少还需要：

1. 为 Ability commit、移动请求和所有可回滚副作用定义统一 PredictionKey 所有权。
2. 保存客户端预测前状态，并定义 Cost/Cooldown、Modifier、Motion、Projectile 的逆操作顺序。
3. 处理服务器拒绝、乱序确认、Unit LifeGeneration 变化和旧预测回调。
4. 为 GameplayCue 定义预测生成、服务器确认、拒绝撤销和重复身份协调。
5. 建立独立网络 Gate，证明回滚不产生第二次伤害、第二个 Modifier 或第二次 Finish。

这些工作会同时改变公共事务与生命周期协议，不适合作为发布修复混入 v1。因此 GAP-022 明确延期到 post-v1；在新增独立 ADR、schema 和自动化前，`bGameplayRollback=false`。Projectile 继续只允许 `bProjectileVisualPrediction=true` 的表现路径。

## 4. REL-003 性能决策

M7 最终 64 Unit / 256 Modifier Dedicated 双客户端容量样本：

| 指标 | 实测 | 冻结预算 | 余量结论 |
| --- | ---: | ---: | --- |
| Server frame P95 | 16.281 ms | 33.34 ms | 通过，约 51% 预算 |
| Server frame P99 | 22.392 ms | 50 ms | 通过，约 45% 预算 |
| 单连接最大发送 | 39.817 KiB/s | 256 KiB/s | 通过，约 16% 预算 |
| Unit / Modifier | 64 / 256 | 64 / 256 | 达到冻结容量 |

CSV profiler 没有给出需要由 Combat pooling、relevancy 或批处理修复的具体 owner。按 ADR-040，M8 的结论是保留现有实现，不做推测性运行时优化：

- 不引入 Actor/Runtime pooling，避免复用 generation 和 EndPlay 语义变化。
- 不改变复制 relevancy，避免 owner/non-owner View 覆盖范围变化。
- 不合并 Scheduler 或 transaction 回调，避免 Priority/ApplySequence 和 exactly-once 变化。
- 后续只有在同一冻结场景超过预算，且 profiler 指向具体 owner 时，才单独立项并保留正确性 Gate。

## 5. REL-001 候选发布回归矩阵

| Gate | 覆盖 | 候选发布要求 |
| --- | --- | --- |
| Editor compile | UHT、蓝图元数据、全部运行时代码 | Installed UE 5.8 Development 通过 |
| Runtime targets | 无 Editor-only 依赖 | Source UE 5.8 Server/Client Development 通过 |
| Automation | 全部 `Combat.*`，包含 M8 契约/生命周期/扩展面 | 冷启动重复运行，无失败、无 flaky |
| Asset validation | DefinitionId、版本、redirect、资源引用 | Errors=0，Warnings=0 |
| Dedicated + 2 Client | Mixed/Minimal、View、RPC、容量、M8 契约日志 | 两端业务 RPC 成功，预算通过 |
| Teardown | Handle、Delegate、Schedule、Runtime、Actor registry | 全部清零，旧句柄不能重复结算 |
| 静态 Gate | Tag 注释、蓝图说明、旁路扫描、diff | 无普通 Tag 宏、无新增权威旁路 |

最终命令、计数和日志写入 [33 M8 验收记录](33-M8-Acceptance.md)。在用户验收前只形成候选发布，不提交 M8。

## 6. G8 判定

G8 只有同时满足以下条件才可交给用户验收：

- P0/P1 Gap 全部关闭，或像 GAP-015/017/022 一样在发布契约中显式延期到 post-v1。
- 没有客户端权威旁路、重复事务结算或 teardown 残留。
- DefinitionId、公式、RNG、Event schema 都有版本边界；DefinitionId 和 Event schema 有明确迁移规则。
- 新技能无需修改核心管线，可只依赖公共 DataAsset、Ability/Modifier 基类和蓝图事件。
