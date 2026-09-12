# DEMO-901 卓尔游侠 Demo 流程整理

> Spec 版本：`0.3`
> 状态：`用户已验收`
> Owner：Codex / 用户验收
> 创建日期：2026-09-10
> 关联进度台账：[00-01 §12.4](../00-Project/00-01-Progress-Tracker.md#124-post-m8卓尔游侠-demo-流程)
> 风险等级：`L1`

## 1. 目标与范围

### 目标

- 将 Demo 的单位内容从 `/Game/Combat/Demo/Characters` 整理到用户指定的 `/Game/Combat/Demo/Heros`。
- 将通用 `Player` 目录与资产名改为 `DrowRanger`，玩家展示名改为“卓尔游侠”。
- 为卓尔游侠配置默认追踪弹体普攻和 625 cm 攻击距离。
- 用可默认自动施放的“霜冻之箭”替换 Demo 当前授予的“远程攻击”：四级法力 9/10/11/12、物理额外伤害 12/18/24/30、减速 15%/25%/35%/45%、持续 1.5 秒、冷却 0、可基础驱散、不叠层、不爆炸且不作用于技能免疫目标。
- 让 `Passive + AutoCast` 技能进入 Q/W/E/R 槽位；Q 只请求服务器原子切换霜冻之箭并显示“自动/关闭”，不创建无目标施法 Order。

### 范围

- Demo Unit、AbilitySet、GameMode、地图和测试对角色资产路径的引用迁移。
- 新增 Frost Arrows Ability/Intrinsic Modifier/Slow Modifier DataAsset 与 Ability Blueprint；复用现有追踪弹体定义。
- 补充技能槽筛选、AutoCast owner-only 投影与输入 RPC；展示 schema 从 3 升至 4。
- 补充真实内容资产、技能免疫、HUD 输入和 Dedicated owner-only 行为回归，同步当前 Demo 文档。

### Non-Goals

- 不增加叠层、爆炸、生命回复降低或独立主动弹道；用户给出的这些数值均为 0。
- 不修改 `combat_v1_rc1` 的伤害、攻击、Projectile、Modifier、GameplayTag、内容或事件 schema；独立 HUD 展示投影按 ADR-048 从 schema 3 兼容升级到 4。
- DEMO-901 当时不删除旧 `CombatAbility:ranged_attack_bolt` 定义；它在该任务完成时作为未授予的兼容示例保留，后续已由 DEMO-902 清理，未影响仍在使用的 `CombatProjectile:ranged_attack_projectile`。
- 不重做卓尔游侠模型、动画、技能图标或冰箭专属弹体美术。
- 不增加技能槽鼠标施法或点击切换；点击仍只固定详情。

## 2. 当前事实与依据

- 相关 DDD：[10-03 Ability 与目标](../10-Architecture/10-03-Ability-Targeting-Blueprint.md)、[10-06 Attack/Projectile/Thinker](../10-Architecture/10-06-Attack-Projectile-Thinker.md)、[20-01 示例技能](../20-Content/20-01-Example-Skills.md)、[20-03 扩展指南](../20-Content/20-03-M8-Public-Extension-Guide.md)。
- 代码事实（`file:line`）：`UCombatFrostArrowsRuntime` 已从当前 AbilitySpec 读取 `mana_cost`、`bonus_damage`、`slow_duration`、`slow_pct`，法球胜出后把额外伤害、弹体和命中减速冻结到 AttackRecord；`UCombatUnitData::AttackProjectileData` 非空时普通攻击在前摇后生成追踪弹体。
- Red 基线：`Saved/DrowRangerDemo/pre-change.json` 回读玩家 `CombatUnit:ranged_combat_player` 的 AttackRange=150、AttackProjectileData=None，AbilitySet 授予 `BP_RangedAttackAbility`；AutoCast HUD/Input 两项新增自动化分别因槽位仅 1 个、Q 未关闭 AutoCast 而失败。
- 工具降级：UE MCP 自动审批服务不可用；资产修改改用同版本 Unreal Python 命令行，并已完成蓝图编译保存、冷回读、资产校验、PIE 和 Dedicated 复验。

## 3. 行为与契约

### 主流程

1. Demo GameMode 生成 `BP_DrowRanger`，服务器用 `DA_DrowRangerUnit` 初始化属性、追踪普攻弹体和 AbilitySet。
2. AbilitySet 以等级 1、AutoCast 开启授予 `BP_FrostArrowsAbility`；固有 Modifier 注册为 `Orb.Primary` 候选。
3. owner-only HUD 把该 `Passive + AutoCast` 技能放入 Q 槽并复制权威开关；按 Q 时客户端只发送 Toggle RPC，服务器读取当前值后原子翻转。
4. 对合法敌方目标开始普攻时，服务器检查 AutoCast、Silence、Break、技能免疫、当前等级参数与 Mana；候选胜出后仅扣一次 Mana，并冻结额外物理伤害、弹体和减速参数。
5. 前摇结束后发射追踪弹体；命中才由既有 Attack Finalize 结算主伤害与额外伤害，并施加 1.5 秒可驱散减速。

### 状态转换

`Ability granted + AutoCast on → HUD Q toggle（可选）→ intrinsic active → attack candidate → orb claimed/resource committed → AttackRecord snapshot → projectile launched → hit/fizzle → exactly-once attack finish`。

### 输入、输出与数据约束

- Ability ID：`CombatAbility:frost_arrows`；最大等级 4。
- Modifier ID：`CombatModifier:frost_arrows_intrinsic`、`CombatModifier:frost_arrow_slow`。
- 复用 Projectile ID：`CombatProjectile:ranged_attack_projectile`。
- `mana_cost=[9,10,11,12]`、`bonus_damage=[12,18,24,30]`、`slow_duration=[1.5]`、`slow_pct=[0.15,0.25,0.35,0.45]`、`cooldown=[0]`。
- 减速 Modifier 的 `MaxStacks=1`、`DispelRule=Basic`，只有一项 MoveSpeed 乘法修改，实际倍率由命中快照写成 `1-slow_pct`。
- “单位目标”由普通攻击目标承载；技能本身使用既有 NoTarget/Passive/Attack/AutoCast 法球标签，不建立第二套主动施法结算。
- HUD owner view 复制 `bUsesAutoCastToggleInput` 与 `bAutoCastEnabled`；客户端展示状态不是下一次 Toggle 的输入。

### 权威边界与权限

- 攻击声明、Mana 提交、Projectile 命中、Physical Damage 和 ApplyModifier 均沿用服务器公共入口。
- 客户端只提交 Attack Order 或无目标状态值的 AutoCast Toggle 请求，并消费复制后的技能/属性/弹体表现，不计算命中或写属性。

### 失败、取消、过期、死亡、EndPlay 与重复请求

- AutoCast 关闭、Silenced、Broken、Mana 不足或目标带 `State.MagicImmune` 时 Frost Arrows 不声明本次攻击，普通远程普攻仍可继续且不扣 Mana。
- 重复 Q 请求按服务器收到顺序逐次翻转；无效 Spec、非 AutoCast 技能或非 Alive 单位由已有 `SetAutoCastEnabled` 服务器校验拒绝，不产生 Cast Order。
- 弹体丢失目标或超时沿用 Projectile exactly-once 结束；未命中不施加减速。
- Slow 到期或基础驱散后移除；Debuff Immunity 拒绝新减速。死亡、复活、Ability 移除和 EndPlay 沿用既有 intrinsic/Attack/Projectile 清理。
- 重复施加同源 Slow 只刷新单层持续时间，不增加层数。

### 兼容、版本与迁移

- Unit、AbilitySet 和 Projectile 的稳定 DefinitionId 不随资产路径/文件名改变；地图、GameMode、HUD 与测试引用通过 UE AssetTools 迁移并修复 redirector。
- Frost Arrows 使用新 DefinitionId；在 DEMO-901 完成时旧 `ranged_attack_bolt` 资产已不再由 DrowRanger AbilitySet 授予，随后由 DEMO-902 删除 Ability Class/DataAsset；共享 Projectile 资产继续保留，因此无需修改已使用的 Projectile DefinitionId。
- `FCombatHUDAbilityView` 兼容新增两个布尔字段，展示 schema 从 3 升至 4；核心 `combat_v1_rc1` 及事件/内容 schema 不变，同版本客户端与服务器部署。

## 4. 实施计划

| 文件/资产 | 变更 | 原因 | blast radius |
| --- | --- | --- | --- |
| `Content/Combat/Demo/Characters` → `Heros` | 移动目录并将 Player 三个资产改名为 DrowRanger | 明确英雄内容与身份 | Demo 地图、GameMode、测试和文档引用 |
| `Content/Combat/Demo/Abilities/FrostArrows/*` | 新建 Ability、Intrinsic、Slow 和 Blueprint | 落地四级数据与法球装配 | AbilitySet、HUD 技能快照、资产扫描 |
| `DA_DrowRangerUnit` / `DA_DrowRangerAbilitySet` | 配置默认 Projectile、625 攻击距离和 Frost Arrows | 默认远程普攻与英雄技能 | 玩家出生与攻击表现 |
| `CombatDemoModifierRuntimes.*` | 技能免疫目标不声明 Frost Arrows 法球 | 实现“不无视技能免疫” | 仅 Frost Arrows 法球候选 |
| `CombatDefinitionData.*` / `CombatAbilitySystemComponent.*` / `CombatPlayerController.*` | 统一技能槽规则并增加服务器原子 Toggle RPC | 让 AutoCast 被动可直接输入且不误发 Cast Order | 玩家技能输入与 ASC AutoCast 状态 |
| `CombatHUDView*` / `CombatHUDSlotWidget.cpp` | owner-only 投影 AutoCast 输入语义与权威状态，显示“自动/关闭” | Q 槽与服务器状态一致 | 展示 schema 4，同版本联机 |
| `CombatFoundationTests.cpp` / `CombatContentExtensionTests.cpp` / `CombatHUDTests.cpp` / `CombatPlayerInputTests.cpp` | 增加真实资产、免疫、HUD 与输入断言 | 防止路径、数值、权限或槽位语义回退 | Editor Automation |
| `CombatTestScenarioActor.cpp` | Dedicated 使用真实 DrowRanger，核对 owner-only AutoCast 字段，并让容量夹具按既有固有 Modifier 补足 256 总量 | 验证真实联机投影且保持冻结容量边界 | Dedicated smoke 测试基础设施 |
| 当前文档 | 更新 Demo 路径、技能槽说明与证据 | 保持事实一致 | 文档导航和维护说明 |

## 5. 验收标准（AC）

- [x] AC-01：`/Game/Combat/Demo/Heros/DrowRanger` 和 `/Heros/WoodenDummy` 可加载，旧 `/Characters` 包不存在且无 redirector。
- [x] AC-02：Demo GameMode 默认英雄为 `BP_DrowRanger`；Unit 稳定 ID 保持 `ranged_combat_player`、显示名为“卓尔游侠”、AttackRange=625、默认 Projectile 为 `ranged_attack_projectile`。
- [x] AC-03：AbilitySet 只授予 `BP_FrostArrowsAbility`，初始等级 1、AutoCast=true；Ability/Intrinsic/Slow 的 ID、标签、四级数值、持续时间、属性倍率键、单层和可驱散配置全部正确。
- [x] AC-04：普通敌人受到对应等级的物理额外伤害和 1.5 秒减速；技能免疫目标只承受默认物理普攻，不扣 Frost Arrows Mana、不附加额外伤害或减速。
- [x] AC-05：相关 Blueprint 编译、保存和冷回读成功，`CombatAssetValidation`、专项与全量 `Combat.*` 通过，Demo PIE 可生成并执行远程普攻。
- [x] AC-06：Q 槽显示霜冻之箭和权威“自动/关闭”状态；连续 Q 只原子翻转 AutoCast，不生成 `CastNoTarget`，Dedicated owner-only 回显正确。
- [x] AC-07：Dedicated 容量夹具把英雄固有 Modifier 计入 256 总量，服务器预算、移动与双客户端 HUD 快照均通过。

## 6. Definition of Done

- [x] 行为或可执行逻辑变化已用相关测试/Golden Case 验证；TDD Red 与 Green 证据均已记录。
- [x] 实现通过直接测试，且旁路扫描为空；Damage、Modifier、Projectile、AutoCast 均保留公共服务器入口。
- [x] 相关 Editor/蓝图/资产已编译、保存并冷回读。
- [x] 已完成 Editor、Server/Client、PIE、Dedicated 与 64/256 容量边界验证。
- [x] `10-01`、`10-08`–`10-10`、`10-12`、`20-01`、`20-03`、`00-04` 和公开中文说明已同步；`90-16` 不适用，因为未新增异步所有权或生命周期类型。
- [x] `git diff --check` 通过；用户已有 `.codex/config.toml` 保留且未纳入本任务。

## 7. 测试矩阵与命令

| 层级 | 用例/命令 | 预期证据 | 结果 |
| --- | --- | --- | --- |
| 文档/本地工具 | `python3 -B Tools/validate_docs.py`（Windows `python3.exe` 为无效商店别名，等价改用 `python -B Tools/validate_docs.py`）、`git diff --check` | 文档与差异卫生 | 通过；55 个 Markdown、314 个本地链接、0 Error；diff check 无错误 |
| Pure/Unit | `Combat.Foundation.Content.DrowRangerDemo`、`Combat.Input.Ability.PassiveAutoCastToggle` | 路径、引用、Definition、配置和无 Cast Order 的原子切换 | 通过；各 1/1，最终包含于 59/59 |
| World Automation | `Combat.ContentExtension.FrostArrows.OrbProjectileSnapshot`、`Combat.UI.HUD.*`、全量 `Combat.*` | 免疫、资源、快照、HUD 状态与回归 | 通过；全量 59/59，0 失败 |
| PIE / Blueprint | Unreal Python 编译保存冷回读；Demo PIE | 可生成、可攻击、追踪弹体、伤害与减速 | 通过；Mana 120→111、Damage=32、Tracking、Slow=1.5 秒单层并到期 |
| Network / Dedicated | Server/Client Target；`Saved/BottomHUD/RunDedicated.ps1` | 权威 AutoCast 投影、移动与复制无回退 | 通过；三 Target 成功，服务器和两个客户端 HUD 快照均 Pass |
| Soak / Perf | Dedicated 64 Unit / 256 Modifier 边界样本 | 新固有 Modifier 不突破冻结容量 | 通过边界样本，P95=9.164 ms、P99=11.334 ms、MaxOut=6.565 KiB/s；未执行新的长时 soak（无新增热循环） |

## 8. 风险、回滚与升级

- 已关闭风险：F2 首轮发现 Dedicated 客户端夹具直接比较未复制的 ASC AutoCast 内部状态，已改为核对 owner-only HUD 投影；容量夹具原先在英雄既有固有 Modifier 之外再创建 256 个合成 Modifier，已改为补足到总计 256。最终三 Target、全量 Automation 与 Dedicated 复验均通过。
- 剩余风险：Frost Arrows 继续复用普通远程弹体美术，尚无冰箭专属视觉；本轮未执行 cook / 打包，因此不把编辑器、PIE 与独立进程证据外推为发行包验证。用户已接受当前 625 cm 射程、HUD 交互和整体 Demo 流程。
- 回滚方式：按本任务差异整体恢复 DrowRanger/FrostArrows 资产、GameMode/地图引用和对应源码；稳定 Unit、AbilitySet、Projectile DefinitionId 未变。DEMO-901 原先关于保留 `CombatAbility:ranged_attack_bolt` 的兼容边界已由 DEMO-902 superseded；如需恢复旧授予关系，还需一并恢复 DEMO-902 删除的两个 Ability 资产。
- 触发升级的条件：用户验收发现射程或交互契约需改变、后续 cook 暴露资产引用缺口、同版本联机之外需要兼容旧 schema 3 客户端，或同一高风险问题三轮仍不收敛。
- 需要人决定的问题：无；用户已于 2026-09-11 确认验收完成。

## 9. 交付证据

### Push-Ready 六层

| 层 | 结果 | 证据 |
| --- | --- | --- |
| L1 Tests | PASS | `Combat.Foundation.Content.DrowRangerDemo`、`Combat.ContentExtension.FrostArrows.OrbProjectileSnapshot`、`Combat.Input.Ability.PassiveAutoCastToggle` 与 HUD 专项通过；最终包含于 `Automation-Full-Final2.log` 的 59/59；文档校验 55 个 Markdown、314 个本地链接、0 Error |
| L2 Types/Build | PASS | UE 5.8 Development `ue_gasEditor`、`ue_gasServer`、`ue_gasClient` 均 `Result: Succeeded`，见 `EditorBuild-Final3.log`、`ServerBuild-Final3.log`、`ClientBuild-Final3.log` |
| L3 No Regression | PASS | 全量 `Combat.*` 59/59；资产扫描 10 个定义、0 Error/Warning；蓝图编译保存与冷回读、PIE 远程法球 smoke、Dedicated 双客户端和 64/256 容量边界通过 |
| L4 Adversarial | PASS | F2 发现并关闭“客户端读取未复制 AutoCast 内部状态”和“容量夹具额外叠加英雄固有 Modifier”两项；`Automation-Full-Final2.log` 与 `Dedicated-Final2.log` 复验通过，无未关闭高风险 finding |
| L5 DDD/Constraints | PASS | `10-01`、`10-08`–`10-10`、`10-12`、`20-01`、`20-03` 与公开中文说明同步；旧路径仅保留在迁移说明和负向测试，新增生产 diff 的 Health/Mana、Transform、Actor Timer 与自建队伍旁路扫描为空 |
| L6 Decisions | PASS | ADR-048 记录 AutoCast 槽位、服务器原子 Toggle、owner-only 投影与 schema 4；稳定 DefinitionId、`combat_v1_rc1`、战斗事件/内容 schema 和旧技能兼容边界均已记录 |

- 代码/资产 diff：DrowRanger/WoodenDummy 已迁至 `/Game/Combat/Demo/Heros`，新增 `/Game/Combat/Demo/Abilities/FrostArrows`，并同步 GameMode、地图、Ability/Modifier/HUD/Input、测试和文档；旧 `Content/Combat/Demo/Characters` 目录不存在。用户已有 `.codex/config.toml` 未修改、未纳入本任务结论。
- 构建结果：`Saved/DrowRangerDemo/EditorBuild-Final3.log`、`ServerBuild-Final3.log`、`ClientBuild-Final3.log` 全部成功。
- Automation 与资产：`Saved/DrowRangerDemo/Automation-Full-Final2.log` 为 59/59、0 失败；`CombatAssetReport-Final2.json` 扫描 10 个定义，0 Error/Warning。
- PIE：`Saved/DrowRangerDemo/PieSmoke.json` 为 Pass；等级 1 Mana 120→111、目标 500→468，20 基础 + 12 额外物理伤害，Tracking Projectile，单层 1.5 秒 Slow 并到期。
- Dedicated：`Saved/DrowRangerDemo/Dedicated-Final2.log` 中服务器和两个客户端 HUD 快照、服务器移动与容量预算均 Pass；64 Unit / 256 Modifier，P95=9.164 ms、P99=11.334 ms、MaxOut=6.565 KiB/s。
- 工具降级：UE MCP 自动审批不可用；资产改动由同版本 Unreal Python 命令行完成，并以蓝图编译保存、冷回读、资产校验、PIE 和 Dedicated 结果补齐证据。
- 未执行验证及原因：未执行 cook / 打包和新的长时 soak；本任务未改 cook 规则或新增周期热循环，当前 L1 交付以三 Target、资产、全量 Automation、PIE、Dedicated 与冻结容量样本为准。
- 用户验收：用户于 2026-09-11 明确确认 `DEMO-901` 验收完成；本次仅回写验收状态，未新增工程验证。
- Gate 结论：F0=`GO`、F1=`APPROVED`、F2=`PASS`，状态 `用户已验收`。

## 10. 变更记录

| 版本 | 日期 | 修改 | 原因 |
| --- | --- | --- | --- |
| 0.1 | 2026-09-10 | 初稿，冻结路径、数值、免疫与兼容边界 | 用户要求整理 Demo 并替换为卓尔游侠霜冻之箭 |
| 0.2 | 2026-09-10 | 完成资产迁移、霜冻之箭、AutoCast HUD/Input、分层验证、F2 修正与 Push-Ready 证据 | 工程 Gate 完成，转入用户验收 |
| 0.3 | 2026-09-11 | 回写用户验收结论，并关闭待确认的手感与整体流程验收项 | 用户明确确认 `DEMO-901` 验收完成 |
| 0.4 | 2026-09-11 | 记录 DEMO-902 对未授予 RangeAttack Ability Class/DataAsset 的后续清理；共享 Projectile 保留 | 保持当前资产事实与历史兼容说明一致 |

## 11. 路由、自评与 Reflect

### 路由记录

- 原始需求摘要：整理 Demo 英雄目录和命名，为 DrowRanger 配置默认远程普攻，并以给定四级参数替换为霜冻之箭。
- 主 Skill：`combat-skill-development`。
- 选择依据：同时涉及 Ability、Modifier、Projectile、AbilitySet、蓝图/DataAsset、地图引用、测试与文档。
- 备选 Skill 与排除理由：通用 `combat-feature-development` 由主 Skill 继承，无需作为并列主流程；非纯资产重命名。
- 路由置信度：`high`

### 交付自评

| 维度 | 权重 | 分数（0–5） | 证据与扣分原因 |
| --- | ---: | ---: | --- |
| 需求与 AC | 20% | 5.0 | 七项 AC 均有代码、资产或运行时证据；Non-Goals、失败路径和用户验收边界明确 |
| 架构与权限 | 20% | 5.0 | 法球、伤害、资源、Projectile 与 Modifier 均沿用服务器公共入口；Toggle 不携带客户端推测状态，核心发布契约未变 |
| 实现与数据 | 20% | 5.0 | 四级 special、稳定 ID、资产迁移、625 射程、Tracking 普攻、技能免疫和单层可驱散 Slow 均完成冷回读与直接测试 |
| 验证证据 | 20% | 4.5 | 三 Target、59/59、资产 10/10、PIE、Dedicated 双客户端与 64/256 边界齐全；扣分项为未执行 cook / 打包和新的长时 soak |
| 文档与可观测性 | 10% | 5.0 | Spec、台账、ADR、架构/技能/HUD 专题、日志路径和 schema 4 已同步 |
| 交付卫生 | 10% | 5.0 | F2 findings 已关闭，旧路径与生产旁路扫描符合预期，生成证据留在 `Saved/`，用户 `.codex/config.toml` 保持不动 |

- 计算总分：`4.9 / 5.0`。
- 硬性封顶或未执行项：无硬性封顶；cook / 打包与新的长时 soak 未执行，均已显式记录且不冒充通过。
- 自评结论：`EVIDENCE_SUFFICIENT`
- 用户验收状态：`已验收`（2026-09-11）

### Reflect 与调优

- 观察与证据：F2 暴露两项测试夹具假设：客户端不能读取未复制的 ASC AutoCast 内部状态；容量基线必须把真实英雄固有 Modifier 计入冻结的 256 总量。两项均非生产结算旁路，但会制造 Dedicated 假失败或错误容量样本。
- 根因类别：`单次实现`。
- 调整文件与预期收益：修正 `CombatTestScenarioActor.cpp`，客户端改验 owner-only HUD 可观察字段，容量夹具按现有 Modifier 数补足；使测试与真实复制边界及冻结预算一致。没有两个独立任务的重复证据，因此不扩大修改通用流程、模板或 Skill。
- 回归验证：修正后 Editor/Server/Client Target 成功，全量 `Combat.*` 59/59，Dedicated 服务器与两个客户端 HUD、移动和 64 Unit / 256 Modifier 容量预算全部 Pass。
- 需要用户决定的问题：无。
