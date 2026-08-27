# 31 M8 生命周期与所有权审计

## 1. 审计规则

每个异步或有状态对象必须回答五个问题：谁创建、谁持有、谁结束、如何拒绝旧回调、World/Actor teardown 如何清理。Handle 必须校验完整 `Id + Generation + LifeGeneration`（不适用生命代次的类型至少校验 `Id + Generation`）；结束广播只能发生一次。

## 2. 所有权清单

| 对象 | 创建者 / 运行时 owner | 活动引用或委托 | 正常退出 | 中断、死亡与 teardown | 旧回调防护 |
| --- | --- | --- | --- | --- | --- |
| Unit Actor | World / 测试场景或关卡 | World Actor registry | Destroy/关卡切换 | `ACombatUnitCharacter::EndPlay` 通知 Aura，清 ASC ActorInfo | LifeGeneration 隔离复活前记录 |
| ASC、Attribute、生命周期、恢复、Order、Attack、Modifier、Motion、View | Unit 构造的默认子对象 | Unit 强持有 | 随 Unit 结束 | 各组件 EndPlay 取消自身任务/委托；Unit 最后清 ActorInfo | 组件只接受当前 Unit/生命状态 |
| GameplayAbility 实例 | GAS AbilitySpec | ASC/Spec 与 AbilityTask | `EndAbility` | 取消 Schedule、Task 和显式绑定的 Projectile/Thinker；停止表现 | ActivationId、SpecHandle、ActorInfo 复核 |
| AbilityTask Interval/Projectile | Ability 实例 | AbilityTask + Scheduler/Projectile finish delegate | 完成后 `EndTask` | `OnDestroy` 取消 Schedule、解绑 delegate；可选取消来源弹体 | Schedule/Projectile Handle + ActivationId |
| Modifier Runtime + ActiveGE | ModifierComponent | `ActiveModifiers`、ActiveGE 一一映射 | Remove/Dispel/Expire | Death 规则、Unit EndPlay 和组件 EndPlay 汇入移除；清 Think/Expire Schedule | ModifierHandle、ApplySequence、当前 ActiveGE |
| Scheduler slot | CombatSchedulerSubsystem | slot 持弱 Owner、堆持 generation 快照 | 一次回调完成或显式 Cancel | `CancelAllForOwner`；Subsystem Deinitialize 清 slot/heap/callback | Id + Generation，陈旧堆节点丢弃 |
| Order record | Unit OrderComponent | 当前 + FIFO 队列 | Completed/Failed | Replace/Stop/Death/EndPlay 取消 EQS、Path、Move、Ability、Attack delegate | OrderHandle + 请求句柄 + LifeGeneration |
| EQS / Path / AI Move | OrderComponent 发起 | 引擎异步请求和显式 delegate handle | 回调推进当前 Order | Cancel generation、Abort/Remove delegate | OrderHandle、EQS instance、Path/MoveRequestId 三重匹配 |
| AttackRecord | Unit AttackComponent | `ActiveRecords` | Landed/Failed/Cancelled Finalize | Death/Stop/EndPlay 取消 attack point/ready 与 Projectile delegate | AttackHandle + LifeGeneration；Finalize 幂等 |
| Projectile record + Actor | ProjectileSubsystem | registry 持 Spec/Actor；World 持 Actor | Hit/Blocked/Distance/Timeout/TargetLost | Ability 可选取消、Source/World EndPlay 汇入 `FinishProjectile` | ProjectileHandle + Source LifeGeneration + AlreadyHit |
| Thinker record + Actor | ThinkerSubsystem | registry 持 Spec/Actor/Schedule | pulse/duration 完成 | Cancel、Ability 可选取消、Actor/World EndPlay 汇入 `FinishThinker` | ThinkerHandle + Source LifeGeneration |
| Aura record + child Modifier | AuraSubsystem | registry 持 Schedule 和 Target→Handle/LifeGeneration | 显式 Cancel | Owner 死亡/换生命/EndPlay，Target 离开/死亡/EndPlay，World Deinitialize | AuraHandle；child 同时校验 Target LifeGeneration |
| Motion record | Unit MotionComponent | Horizontal/Vertical 通道记录 | Completed | 高优先级抢占、Blocked、Death、EndPlay | MotionHandle + channel generation，Finish exactly-once |
| Unit/Modifier View 委托 | UnitViewComponent | Attribute/Modifier 原生 delegate handles | Actor 持续期间更新 | EndPlay 显式 Remove，View 不反向驱动 gameplay | 只读当前 Unit 与稳定 DefinitionId |
| Damage/Heal transaction slot | CombatTransactionSubsystem | EventId → 同步 slot | AttributeSet 回报后 Consume | GE 失败 Cancel；World 销毁释放 map | EventId 唯一、Kind/Target/Reported 复核 |
| Combat event 与诊断 | CombatEventSubsystem | World 内 512 条环形缓冲和同步 delegate | 超限淘汰最旧记录 | World 生命周期结束整体释放 | 单调 EventId/Sequence，schema v1 |
| RPC 安全状态 | CombatNetworkSecuritySubsystem | 每连接 token/replay window | 连接请求持续更新 | World/连接结束释放；Unit EndPlay 不留下 gameplay 入口 | ownership + RequestId + bounded replay window |
| Projectile 预测视觉 | ProjectilePresentationSubsystem | 弱 predicted/server Actor map | 服务器身份 reconcile 或 Actor 结束 | World Deinitialize 释放；视觉 Actor 无 gameplay API | PredictionKey 只消费一次，服务器 Handle 去重 |
| 测试场景 Actor | `L_CombatTest` | 强引用瞬态 Data、Unit 数组与 TimerHandle | DestroyScenario/RespawnScenario | EndPlay 清 Timer、Aura、Projectile、Unit 和容量 Data | 有效 Actor/Handle 检查，重复清理安全失败 |

## 3. 审计结论

- 没有发现未知 runtime owner；World registry、Unit component、GAS Spec/ActiveGE 和 AbilityTask 的边界明确。
- 所有跨帧 gameplay 时间都由 Combat Scheduler 或连续运动 Tick 承担，Actor Timer 只存在于测试场景编排，不参与唯一结算。
- Finish/Remove/Cancel 都先从活动 registry 取走或验证完整句柄，再广播与销毁；重复使用旧句柄安全失败。
- Unit Death/Respawn 的 LifeGeneration 会淘汰旧 Attack、Projectile、Order、Aura child 和异步回调。
- View、Projectile 表现和调试缓冲只消费权威结果，不会反向修改 gameplay。

`Combat.Release.M8.LifecycleOwnershipAndTeardown` 在真实 Dedicated 测试 World 中同时建立 Schedule、Modifier Runtime、Thinker、Aura 和 Transaction slot，逐个退出后断言 registry 全部为 0、广播 exactly-once、旧句柄全部失败。前序 M2–M7 自动化继续覆盖 Death、Respawn、Order/EQS/Move、Attack、Projectile、Motion、Aura 与 64/256 容量 teardown。

## 4. 后续新增对象的检查表

新增 Runtime、Actor、Delegate、Schedule 或异步请求时，必须：

1. 在本表增加唯一 owner 和持有方式。
2. 列出 Completed、Cancelled、Death/Respawn、Owner EndPlay、World Deinitialize 五类出口。
3. 复用稳定 Handle/generation；与 Unit 生命相关时加入 LifeGeneration。
4. 将所有出口汇入一个幂等 Finish/Remove 函数，并为重复调用写负测试。
5. 明确委托解绑和 Schedule 取消位置，不能依赖弱引用“以后自然失效”代替退出协议。
