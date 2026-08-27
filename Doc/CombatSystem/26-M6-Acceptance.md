# 26 M6 验收记录：复杂技能集

## 1. 验收状态

- 里程碑：M6
- Gate：G6 通过
- Task：6/6 已完成
- 用户验收：已验收
- 提交验收日期：2026-08-26
- 最终验收日期：2026-08-27

## 2. 完成范围

| 范围 | 实现结果 |
| --- | --- |
| Frost Arrows | Intrinsic Runtime 从 AbilitySpec 读取等级与 AutoCast；`Orb.Primary` winner 才提交 Mana；bonus、ProjectileData、slow Modifier/duration/magnitude override 冻结到 AttackRecord，已发射事务不受升级或移除影响 |
| Fissure A | 服务器有限线段稳定查询；Damage、Stun、Knockback 分别进入 DamageSubsystem、ModifierComponent、MotionComponent；visual-only Thinker 仅负责 Scheduler 表现生命周期 |
| Fissure B | `ACombatFissureBlocker` 使用 `CombatBlocker`、关闭 Tick、由 Scheduler 到期；创建/移除主动通知路径相交 Order repath，attempt generation 淘汰同 OrderHandle 的旧移动回调 |
| Aura | `UCombatAuraSubsystem` 提供每 World registry；保存 Owner/LifeGeneration、Targeting 与 child 映射，使用 Scheduler Coalesce 做 add/remove/repair；换队、Break、死亡、取消和 teardown 清理幂等 |
| 高级状态 | SpellBlock 位于 SpellStarted commit 后、Action 前；Break 只暂停显式标记行为；Debuff Immunity 只拒绝新 Debuff；Dispel Immunity 只拒绝 Dispel；四类规则使用独立 Tag/Failure/Event |
| 技能模板 Gate | `FCombatSkillTemplateValidator` 校验 Class/Data 身份、schema、必需 behavior/special、Definition 唯一与事件顺序；提供稳定旁路模式和 [25 检查表](25-M6-Skill-Template-Checklist.md) |

实现语义见 [24 M6 复杂技能集决策](24-M6-Content-Decision.md)。ADR-036/037 已关闭 GAP-014/GAP-016。

## 3. 自动化结果

Installed UE 5.8.1 以 `UnrealEditor.exe -unattended -NullRHI` 冷启动执行全部 `Combat.*` 自动化：

```text
Test Completed. Result={Success}: 32
Test Completed. Result={Fail}: 0
RequestExitWithStatus: 0
```

M6 新增 5 组测试全部通过：

| Test | 覆盖重点 |
| --- | --- |
| `Combat.ContentExtension.FrostArrows.OrbProjectileSnapshot` | winner 扣蓝、等级 special、Projectile/slow 快照、Ability 升级/移除后 landed-only OnHit |
| `Combat.ContentExtension.Fissure.LineControlBlockerRepath` | 线段去重、Damage/Stun/Motion、visual Thinker、blocker 与旧 attempt 淘汰 |
| `Combat.ContentExtension.Aura.OwnerChildReconcile` | 进入/离开、重复 reconcile、换队、Break、Owner 死亡和 stale cancel |
| `Combat.ContentExtension.Status.AdvancedInteractionMatrix` | SpellBlock commit 边界、非可阻挡技能、Break、Debuff/Dispel Immunity |
| `Combat.ContentExtension.Tool.SkillTemplateValidator` | 模板身份、schema、special/behavior、Definition、事件序列和旁路模式 |

其余 27 个 M0–M5 回归测试同时通过。验收日志：`Saved/Logs/M6FullAutomation.log`。

Installed UE 5.8.1 的 `UnrealEditor-Cmd.exe` 启动器在本机额外执行全平台 SDK 预检，并因未安装 LinuxArm64/VisionOS SDK 提前退出；改用同版本 `UnrealEditor.exe -unattended -NullRHI` 后正常加载相同 Editor 模块并完成全部测试。该预检失败不属于项目编译或 Automation 失败。

## 4. 构建结果

| Engine | Target | Configuration | 结果 |
| --- | --- | --- | --- |
| Installed UE 5.8.1 | `ue_gasEditor` | Win64 Development | 正式编译、链接成功；最终 8/8 actions |
| Source UE 5.8.0（`D:\UE\UE`） | `ue_gasServer` | Win64 Development | Succeeded；最终 13/13 actions |
| Source UE 5.8.0（`D:\UE\UE`） | `ue_gasClient` | Win64 Development | Succeeded；最终 13/13 actions |

三个 Target 均编译 Aura、Fissure blocker、Frost Arrows、高级状态、模板 validator 与自动化源码，没有 Editor-only 依赖泄漏到 Server/Client。

## 5. 独立联机 smoke

使用 Installed UE 5.8.1 启动两个隐藏的独立 OS 进程：监听服务器加载 `/Game/Combat/Tests/L_CombatTest?listen` 并监听 `17807`，客户端连接 `127.0.0.1:17807`。关键证据：

```text
IpNetDriver listening on port 17807
M6ScenarioReady FrostArrows=Ready Fissure=Ready AuraRuntime=Ready AuraStarted=Yes AuraChildren=1 AdvancedStatus=Ready TemplateValidator=Ready
Event.Combat.AuraStarted
Event.Combat.AuraReconciled ... Applied=1.000
Welcomed by server (Level: /Game/Combat/Tests/L_CombatTest, ...)
Join succeeded
```

验收日志：

- `Saved/Logs/M6ListenServer.log`
- `Saved/Logs/M6ListenClient.log`

最终自动化与 smoke 日志未发现项目 `LogCombat: Error`、Fatal、Assertion、ensure、NetworkFailure 或失败用例。Installed UE 自带 Experimental Toolsets 仍会输出与 M5 相同的 Python API 导入噪声，不影响 Combat 模块、场景或联机结果。

## 6. 静态 Gate

生产技能目录 `Combat/Demo` 与 `Combat/Aura` 的旁路扫描为空：

```text
SetHealth( | SetActorLocation( | GetTimerManager( | SetTimer( | ProjectileImpact(
```

Fissure blocker 的初始位置与朝向通过权威 `SpawnActor` Transform 参数冻结；单位强制位移只走 MotionComponent。M6 新增或实质修改的项目源码已补齐中文类、结构、枚举、函数、关键字段和时序注释。

验收反馈整改后，`CombatTags.cpp` 中 144 个 Native GameplayTag 定义全部使用 `UE_DEFINE_GAMEPLAY_TAG_COMMENT` 并提供中文语义说明，普通 `UE_DEFINE_GAMEPLAY_TAG` 数量为 0。现有生产 `BlueprintCallable`、`BlueprintPure`、`BlueprintNativeEvent` 节点统一补充中文 `DisplayName` 与 `ToolTip`，有输入参数的节点补充中文 `UPARAM(DisplayName=...)`；M6 新增的 Aura、法球快照、Fissure、Break 与 visual-only 配置字段补充属性面板可见说明。整改后 Installed UE 5.8.1 Editor 正式编译成功，`Combat.*` 再次 32/32 通过；复核日志为 `Saved/Logs/CombatCommentsAutomation.log`。

## 7. G6 结论

- Frost Arrows 覆盖法球 winner 资源提交与远程普攻快照边界。
- Fissure 组合公共 Damage/Modifier/Motion/Thinker/Order 管线，并证明 blocker repath 不接受旧导航回调。
- Aura 和四类高级状态的所有权、阶段与生命周期已冻结，GAP-014/GAP-016 关闭。
- 模板检查器、复制检查表、自动化、PIE/独立场景入口和预期事件均已提供。
- Editor、Server、Client 构建、32/32 自动化和独立联机 smoke 均通过；中文可见说明整改复核通过，M6 已通过用户验收，可以提交。
