# 90-16 M8 生命周期与所有权审计

> 2026-09-02：补充 SAM 服务器权威移动的 CommandedUnit、AIController 与 Crowd 生命周期。

## 1. 审计规则

每个异步或有状态对象必须回答五个问题：谁创建、谁持有、谁结束、如何拒绝旧回调、World/Actor teardown 如何清理。Handle 必须校验完整 `Id + Generation + LifeGeneration`（不适用生命代次的类型至少校验 `Id + Generation`）；结束广播只能发生一次。

## 2. 所有权清单

| 对象 | 创建者 / 运行时 owner | 活动引用或委托 | 正常退出 | 中断、死亡与 teardown | 旧回调防护 |
| --- | --- | --- | --- | --- | --- |
| Unit Actor | World / 测试场景或关卡 | World Actor registry | Destroy/关卡切换 | `ACombatUnitCharacter::EndPlay` 通知指挥 Controller/Aura，清 owning 反向引用与 ASC ActorInfo | LifeGeneration 隔离复活前记录 |
| Command Pawn / CommandedUnit 绑定 | GameMode 默认出生编排 + PlayerController 服务器绑定事务 | GameMode 独立生成 Unit/Command Pawn，PC 强引用 owner-only CommandedUnit；Unit 以服务器弱引用锚定指挥 PC | 幂等重绑、切换 Unit 或 Command Pawn 重生 | 出生任一步失败清理本次新建 Actor；PC EndPlay、Unit EndPlay、断线或控制转移先取消旧 Order、清 Owner，并提升 BindingGeneration | CommandBindingGeneration；输入还要复核 Unit Owner 已复制为本地 PC；禁止嵌套 `OnPossess` 迁移 Unit |
| Combat AIController / Crowd agent | Unit `SpawnDefaultController` / 服务器 | AIController Possess Unit；CrowdFollowing 由 Controller 默认子对象持有 | Alive 普通移动为 Enabled | Root/Stun/Motion 为 ObstacleOnly；NoCollision/Dead/UnPossess 为 Disabled；Controller 变化时 Order delegate 重绑 | 普通 Move 只接受专用 AIController；OrderHandle + MoveRequestId + attempt generation 淘汰旧回调 |
| ASC、Attribute、生命周期、恢复、Order、Attack、Modifier、Motion、View | Unit 构造的默认子对象 | Unit 强持有 | 随 Unit 结束 | 各组件 EndPlay 取消自身任务/委托；Unit 最后清 ActorInfo | 组件只接受当前 Unit/生命状态 |
| GameplayAbility 实例 | GAS AbilitySpec | ASC/Spec 与 AbilityTask | `EndAbility` | 取消 Schedule、Task 和显式绑定的 Projectile/Thinker；停止表现 | ActivationId、SpecHandle、ActorInfo 复核 |
| AbilityTask Interval/Projectile | Ability 实例 | AbilityTask + Scheduler/Projectile finish delegate | 完成后 `EndTask` | `OnDestroy` 取消 Schedule、解绑 delegate；可选取消来源弹体 | Schedule/Projectile Handle + ActivationId |
| Modifier Runtime + ActiveGE | ModifierComponent | `ActiveModifiers`、ActiveGE 一一映射 | Remove/Dispel/Expire | Death 规则、Unit EndPlay 和组件 EndPlay 汇入移除；清 Think/Expire Schedule | ModifierHandle、ApplySequence、当前 ActiveGE |
| Scheduler slot | CombatSchedulerSubsystem | slot 持弱 Owner、堆持 generation 快照 | 一次回调完成或显式 Cancel | `CancelAllForOwner`；Subsystem Deinitialize 清 slot/heap/callback | Id + Generation，陈旧堆节点丢弃 |
| Order record | Unit OrderComponent | 当前 + FIFO 队列 | Completed/Failed | Replace/Stop/Death/EndPlay 取消 EQS、Path、Move、Ability、Attack delegate | OrderHandle + 请求句柄 + LifeGeneration |
| Order 转身准备（ADR-044） | OrderComponent 持有命令与 FacingSchedule；Unit 默认子对象 CombatCharacterMovement 执行旋转 | Scheduler 弱 Owner 回调；移动组件每帧只读当前命令，不复制目标或保存独立转向 Handle | 对准后取消复核，重走公共校验再激活技能/攻击 | 替换/Stop/Death/EndPlay 取消，控制状态/Motion 暂停，重新追击或超时退出；World teardown 清 Scheduler | Schedule generation + OrderHandle + UnitLifeGeneration；移动组件还检查服务器权限、状态和当前 Facing 阶段 |
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
- Command Pawn 不含 Combat 组件或 gameplay collision；其本地 Tick 只移动相机载体，不回写 Unit transform。
- Unit 保存的服务器弱指挥引用只用于抵抗 Pawn 销毁/Controller 重建时 Owner 被引擎临时覆盖；网络 Owner 仍是 RPC 与 ASC owning connection 的唯一来源。
- Demo GameMode 必须继承 `Aue_gasGameMode`，让默认出生直接返回 Command Pawn；Automation 同时校验该资产父类、唯一 Combat AIController 和零孤立 AIController，避免重现“Unit BeginPlay 生成 AI 后又被 PlayerController 迁移”的顺序缺陷。

`Combat.Release.M8.LifecycleOwnershipAndTeardown` 在真实 Dedicated 测试 World 中同时建立 Schedule、Modifier Runtime、Thinker、Aura 和 Transaction slot，逐个退出后断言 registry 全部为 0、广播 exactly-once、旧句柄全部失败。`Combat.SAM.CommandBindingLifecycle` 与 `Combat.SAM.CrowdStateLifecycle` 继续覆盖控制转移、Unit EndPlay、死亡/复活、Motion 与 Controller teardown；前序 M2–M7 自动化保留 Order/EQS/Move、Attack、Projectile、Aura 与 64/256 容量 teardown。

## 4. 后续新增对象的检查表

新增 Runtime、Actor、Delegate、Schedule 或异步请求时，必须：

1. 在本表增加唯一 owner 和持有方式。
2. 列出 Completed、Cancelled、Death/Respawn、Owner EndPlay、World Deinitialize 五类出口。
3. 复用稳定 Handle/generation；与 Unit 生命相关时加入 LifeGeneration。
4. 将所有出口汇入一个幂等 Finish/Remove 函数，并为重复调用写负测试。
5. 明确委托解绑和 Schedule 取消位置，不能依赖弱引用“以后自然失效”代替退出协议。

## 5. DEF-002 防守模式增量审计（2026-10-15）

本节追加 post-M8 新对象，不改变上文 M8 历史验收结论；当前验证见 [DEF-002](../Specs/DEF-002-defense-phase1-skeleton.spec.md)。

| 对象 | 创建/持有 | 正常/失败/取消出口 | 旧回调与 teardown |
| --- | --- | --- | --- |
| Defense Director | Defense GameMode 默认组件 | 唯一 Finish 冻结 Victory/Defeat；Shutdown/EndPlay 幂等结束 | Scheduler Owner 为组件；RunId + PhaseGeneration 校验；终局 CancelAllForOwner，解绑核心 |
| Core Objective | GameMode 创建，World 持有 | 公共 Damage/Lifecycle 死亡触发失败，GameMode EndPlay 销毁 | CoreLife 校验，丢失或换生命拒绝胜利；清理专用 AIController |
| 波次敌人 | Director Deferred Spawn，World 持有 | 已确认死亡后排队清场；异常销毁/换生命/超时失败 | Actor + LifeGeneration + Phase；先解绑再 StopOrder、Destroy Unit/Controller；旧死亡不重复减计数 |
| 玩家对局记录 | Director 按 Controller 弱引用持有 | 每波凭证去重；经验/强化死亡暂存，安全复活再交付 | Logout 移除记录和精确 Modifier Handle；Shutdown 撤销强化，终局清待领取项 |
| Run/Wave 冻结快照 | Director 深拷贝并强持有 | 随组件释放，不回写源资产 | 无跨局静态容器；DefinitionId 稳定，Actor 实例身份独立 |
| Defense RunView / HUD | GameState 复制值快照，HUD 本地绘制 | 随 GameState/HUD 生命周期结束 | 只读 ASC 投影和服务器截止时间，不持有 gameplay 委托或调度 |

验证覆盖成功、失败、重复死亡、旧局/阶段/生命、Owner 销毁、World teardown、奖励幂等和断线；真实网络探针仅显式 `DefenseSmoke` 开关启用，平时不 Tick。


## DEF-004 选择窗口增量（2026-10-16）

追加新对象审计，不修改历史 M8/DEF-002 结论：Director 按 RunId/PhaseGeneration/ChoiceRevision 和英雄生命/控制身份维护每玩家窗口，开窗不延长公共阶段，重抽不延长期限。15 秒到期或开始下一波时统一收口；重复请求只读最多 32 条完整载荷回执。选择通过公共 Modifier 生效，死亡时保留待生效 ID，复活后恰好一次；核心失败清待领，断线/Shutdown 撤销精确已授予句柄。

ChoiceComponent 随 Defense PC 创建，只复制拥有者视图、不保存第二套构筑真值；EndPlay 清视图，UI NativeTick 仅显示快照。Seed/Pool/Card/Modifier 在开局冻结且不复制对象指针，World teardown 不留下静态引用。新的测试覆盖同号换载荷、旧生命/修订/局、终局取消、死亡超时/复活、Shutdown 和有界缓存；见 [DEF-004](../Specs/DEF-004-defense-choice-flow.spec.md)。


## DEF-005 团队商品增量（2026-10-16）

本节追加当前实现，不改写历史验收：Director 强持有冻结目录及独立效果定义，按 RunId/目录项键保存每局限购记录。活跃临时效果绑定精确 Hero 弱引用 + Modifier Handle；准备期迟加入和阵亡时未领效果在有效期内补领。购买者离开不会撤销其他成员效果。最后覆盖波清场、终局与 Shutdown 都先取走句柄再调用公共移除；终局保留购买记录，Shutdown 清空账本及回执，重复清理无副作用。复用已有阶段/监控 Scheduler，不新建 Actor Timer。RPC 组件拥有者复制，EndPlay 清视图；Widget 释放商店 Slate 引用。独立效果、死亡补领、旧请求、GC、Owner teardown 和提交中取消有专项测试，见 [DEF-005](../Specs/DEF-005-defense-team-shop.spec.md)。


## DEF-006 同步领取增量（2026-10-16）

本节记录新实现并替代上文 DEF-005 独立组件的当前职责，不改历史验收：ItemSubsystem 仍是唯一实例登记表；获得即用在一次同步领取锁内调用配置效果，成功后立即销毁实例与地面投影。没有额外 Actor Timer、延迟 AbilityEnded 或隐藏库存槽。原经济组件持有即用定义快照、有界购买回执及只读 ShopOffers；EndPlay 释放这些引用。

Defense Director 仅持有领域效果快照、每局限领账本与精确 Hero+Modifier Handle。复制状态由原经济组件投影，不再有 TeamShop Component/RPC。阶段改变、到期、死亡补领与终局仍复用既有 Scheduler。已提交的即时治疗若在日志回调中引发 Shutdown，物品消费仍成功，导演不再重建已清空账本；属性效果执行中取消会撤销已授予句柄并使经济事务回滚。状态发布先快照接收者集合，避免观察者结束对局时迭代失效。新 Red/Green 与回归见 [DEF-006](../Specs/DEF-006-reuse-shop-use-on-acquire.spec.md)。


## DEF-007 核心屏障增量（2026-10-16）

核心 ModifierComponent 唯一持有屏障Runtime与ActiveGE；Director账本只保存核心弱引用和精确Handle。创建时以核心最大生命快照参数初始化余量；公共伤害Hook扣减、耗尽请求deferred remove，不新增Timer或委托。现有Monitor发现耗尽后清活跃状态，下一波清场/终局/Shutdown撤销精确句柄。买家离开不撤核心效果，英雄复活补领仅处理属性商品。

同步领取在任何目标正执行Hook时提前拒绝；不能将无句柄的延迟Apply当作失败后留给队列执行。比例治疗仍沿用“已提交治疗被观察者终结时消费成功、不得重建账本”的边界。测试与证据见 [DEF-007](../Specs/DEF-007-defense-core-rescue.spec.md)。
