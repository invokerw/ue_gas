# 25 M6 技能模板检查表

## 1. 使用范围

复制 Frost Arrows、Fissure 或其他 M6 示例创建新技能前，必须完成本表。公共检查器是 `FCombatSkillTemplateValidator`，人工检查用于资产表现、PIE 与源码旁路边界。

## 2. Definition 与 Class/Data 身份

- [ ] Ability、Modifier、Projectile 等 `DefinitionName` 均为唯一 `lower_snake`，没有空值或重复 `PrimaryAssetId`。
- [ ] Ability Class CDO 的 `AbilityData` 与待检查 DataAsset 是同一对象，保持 Class -> Data 单向引用。
- [ ] `ValidateRuntime` 通过；TargetMode、Behavior、CommitPolicy、等级数组与 Action schema 没有冲突。
- [ ] 所有平衡值来自 `SpecialValues`，数组覆盖 `MaxLevel`；代码没有写死可配置伤害、距离、持续时间或资源消耗。
- [ ] 被动/法球模板配置 `IntrinsicModifier`；公共 Action 模板至少包含一个受支持的 DataDriven Action。
- [ ] DataAsset 及展开的项目自有结构字段在 Details 面板中具有中文显示名和说明；单位、范围、空值及 `0`、负数、保留值等特殊语义可直接从提示中确认。

## 3. 公共管线与时序

- [ ] Damage、Heal、Modifier、Projectile、Thinker、Motion、Aura 分别使用对应公共 Subsystem/Component。
- [ ] gameplay 周期、延迟和生命周期使用 Combat Scheduler；Actor Tick/Timer 只允许纯表现且需注明。
- [ ] 强制位移使用 `UCombatMotionComponent`；技能代码不直接写 Unit Transform。
- [ ] 服务端重新执行 Targeting；客户端不提交命中列表、伤害值、等级或阵营结论。
- [ ] 事件序列与模板完全一致，失败分支也只产生一次 Finish/Interrupted/OrderReleased。
- [ ] Snapshot 边界明确：发射、attack point 或 SpellStarted 后不回读会改变既有事务含义的 Data/Spec。

## 4. M6 专项

- [ ] Frost Arrows：`CanClaimAttack` 无副作用；winner 才提交 Mana；Projectile、bonus、slow Modifier、duration 与 magnitude 已写入 AttackRecord。
- [ ] Fissure：线段查询稳定去重；Damage/Stun/Motion 各走公共入口；视觉 Thinker 不结算 gameplay；blocker 创建/移除会主动 repath。
- [ ] Aura：Owner/LifeGeneration、Targeting、child handle 与调度全部由 Aura registry 持有；Break、换队、死亡和 EndPlay 无 child 残留。
- [ ] SpellBlock 只处理显式 `Ability.Behavior.SpellBlockable`，发生在 commit 后、Action 前；高级免疫没有复用 MagicImmune。

## 5. 自动化与旁路扫描

运行：

```text
UnrealEditor.exe ue_gas.uproject -unattended -nop4 -nosplash -NullRHI -NoSound -NoAssetRegistryCache -ExecCmds="Automation RunTests Combat.ContentExtension.;Quit" -TestExit="Automation Test Queue Empty"
```

标准环境也可使用 `UnrealEditor-Cmd.exe`。若 Installed UE 5.8 的 Cmd 启动器因未安装的 LinuxArm64/VisionOS SDK 在全平台预检阶段退出，可使用上面的 `UnrealEditor.exe -unattended -NullRHI`；仍由同一 Editor 模块和 Automation Controller 执行测试。

源码扫描范围为项目自有生产技能目录，排除 `Combat/Tests` 和 validator 自身的模式字面量：

```text
rg -n "SetHealth\s*\(|SetActorLocation\s*\(|GetTimerManager\s*\(|SetTimer\s*\(|ProjectileImpact\s*\(" Source/ue_gas/Combat/Demo Source/ue_gas/Combat/Aura
```

- [ ] `Combat.ContentExtension.*` 全部通过。
- [ ] 扫描结果为空；若是 Actor 生成时的初始 Transform，改为通过 `SpawnActor` Transform 参数表达。
- [ ] 新增至少一个正向、一个失败/边界自动化，并断言稳定 FailureTag 或事件顺序。

## 6. PIE / Dedicated 验收

1. 打开 `/Game/Combat/Tests/L_CombatTest`，启动两玩家 PIE 或独立监听服务器。
2. 确认服务器日志包含：

```text
M6ScenarioReady FrostArrows=Ready Fissure=Ready AuraRuntime=Ready AuraStarted=Yes AuraChildren=1 AdvancedStatus=Ready TemplateValidator=Ready
```

3. 确认存在 `Event.Combat.AuraStarted`、`Event.Combat.AuraReconciled`；停止场景后 Aura/Modifier/Thinker/Blocker registry 无残留。
4. Dedicated Client 必须记录连接成功；客户端只观察复制结果，不拥有 Modifier Runtime 或 gameplay 结算权威。

## 7. 模板放行

- [ ] 资产保存、编译且无 validation error。
- [ ] Automation、PIE、Dedicated（涉及网络时）证据已记录。
- [ ] 中文注释覆盖项目自有新增/实质修改的类、结构、枚举、函数、关键字段和非直观时序。
- [ ] 新增或修改的 DataAsset 配置字段显式提供中文 `DisplayName` 与 `ToolTip`，并按语义补充单位、范围、条件显示或数组标题元数据。
- [ ] 对应功能文档、测试计划和进度台账已同步。

全部勾选后，示例才可作为新技能模板；检查器通过不能替代 PIE、网络和人工旁路审计。
