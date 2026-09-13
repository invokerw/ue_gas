# 10-13 技能瞄准与范围指示器

> AIM-001 / ADR-053；工程验证以 [Spec](../Specs/AIM-001-skill-indicators.spec.md) 和 [台账](../00-Project/00-01-Progress-Tracker.md) 为准。

## 1. 玩家交互

主动目标技能默认标准施法：按 Q/W/E/R 进入瞄准，移动光标预览，左键确认。Escape 或右键取消本地瞄准；取消用的整个右键手势不发送移动，下一次右键恢复普通移动/普攻。A 切换为普攻选敌，S 清理手势并发送 Stop。无目标技能按下立即提交；卓尔 Q 保留服务器 AutoCast Toggle，W/E/R 保留空位。

Controller 的 `Input|Abilities / 技能施法方式` 可选标准、按下快施或松开快施。快施只改变确认时机；松开必须属于仍有效的同一会话。换技能、输入 Canceled、失焦或 UI 上松开不会使用旧落点。所有按键继续使用现有 InputAction 和 IMC 映射。

悬停技能槽显示已知范围，点击仍固定详情。底部 HUD、技能升级按钮、详情和日志入口/窗口阻断世界点击；日志拖动捕获期间也阻断。HUD 消费 Escape 时同步取消瞄准。

## 2. 三层视觉与状态

| 层 | 内容 | 边界 |
| --- | --- | --- |
| 施法圈 | 施法者脚下低亮度细圈 | CastRange + owner CastRangeBonus + 施法者胶囊半径；AutoCast 使用 AttackRange |
| 作用形状 | 目标圆形或由施法者发出的胶囊直线 | 显式主 Action 的半径/长度；无描述时不猜形状 |
| 目标标记 | 实际单位脚下锁定环或点目标准星 | 单位只认射线实际命中，无最近单位回退 |

绿色表示本地预检可提交，琥珀虚线表示超距但可以请求追近，红色表示目标不合法，灰色表示资源/控制阻断。原因显示在 HUD 活动行；服务器前摇/引导状态优先。数据未齐不显示虚构范围。

超距不截断点目标，原始点或单位仍提交原 Order。范围语义是 XY 胶囊边缘距离，公共 Targeting 的 5 cm 容差不额外扩大 HUD 数字。OutOfRange 在 LOS 检查之前返回，琥珀色不保证路线、视线或最终施法成功。

## 3. 数据配置

在 AbilityData 的“主预览动作索引”配置 Actions 中从 0 开始的索引，默认 -1。多个 Action 不自动选择第一条，避免多段技能误导。

| 动作 | 形状与参数 |
| --- | --- |
| 非弹体动作且 Target=UnitsInRadius | Circle，RadiusKey 当前等级值 |
| CreateThinker | Circle，RadiusKey；Caster 锚点显示在自己脚下，其余使用目标位置 |
| SpawnLinearProjectile | Line，RadiusKey 为半宽、ProjectileRangeKey 为飞行长度；负覆盖/空键按 ProjectileData 回退，0 为显式覆盖 |
| 无目标范围 | 与同一 Action 使用的施法者位置一致 |
| 未配置、单体或自身治疗 | 无额外作用形状；保留可靠的施法圈/目标标记 |

`ResolveIndicatorGeometry` 不保存第二套平衡数据，等级从当前 Spec/View 匹配。索引越界、不支持的主动作、非有限/负半径或无效直线长度经 AbilityData 的 runtime/editor validator 拒绝。

直线地面长度为当前三维弹道最大距离的 XY 投影；高低差会缩短投影，不把整个三维距离平铺到地面。它表示潜在轨迹，世界阻挡、提前命中与到期仍可能使实际弹体更早结束。

## 4. 实现与生命周期

Controller 解释输入并独占 `SubmitCombatOrder`。不复制的 `UCombatAbilityAimComponent` 保存弱 Unit、SpecHandle、SessionSerial、CommandBindingGeneration、LifeGeneration。确认时重新追踪、匹配授予身份并通过公共 Targeting 本地预检，再生成一次原有 Cast 请求。

Tick 只整理展示，不查询全场单位、不预测命中、不发送逐帧 RPC。换技能、控制转移、死亡/复活、撤销授予、Owner EndPlay、视口按键冲刷、应用失焦和 World teardown 使旧会话失效。旧 KeyUp 和旧 RequestId 回执无法确认新会话。

提交前关联原有 `OnOrderBatchResult`；匹配绑定/生命的首个回执仅显示“指令已接收”或“施法指令被拒绝”，不宣称技能完成，不自动重发。提示到期解绑。实际技能阶段继续取公共 UnitView。

`ACombatAbilityIndicatorActor` 由本地组件持有，复用三层 UDecalComponent 和动态材质，无复制、碰撞或 Tick。材质 `/Game/Combat/Shared/Materials/M_CombatAbilityIndicator` 使用世界 XY 有符号距离绘制圈、胶囊线和准星。只移动视觉组件，不移动战斗单位。Dedicated 禁用组件 Tick，不创建视觉 Actor。材质软引用由 CDO 持有供 cook 收集。

### 地面接收与落点（AIM-002 / ADR-054）

点目标使用 `CombatIndicatorGround` 射线；标准确认、按下快施、松开快施和逐帧预览共用 `TraceAimHit`。单位技能仍使用 Visibility 精确选敌。Hero/木桩不阻挡地面查询，点目标不会采用角色身体高度；无地面命中时清除作用形状，显示“请选择地面位置”，不提交旧点。

关卡作者应对地面、坡道和可站立平台的网格组件同时设置：

1. Collision Presets 从 Default 切换为组件的 Custom 设置，保留其他通道响应；仅把 `CombatIndicatorGround` 设为 Block（其他组件默认 Ignore）。仍继承 StaticMesh 默认碰撞时，重新加载会覆盖组件的临时响应。
2. Rendering 的 `Render CustomDepth Pass` 开启。
3. `CustomDepth Stencil Value` 加上 `128` 位，Write Mask 保留 Default/255；低七位保留给其他表现，不把 128 分配给角色或普通道具。
4. 项目 `Custom Depth-Stencil Pass` 使用 Enabled with Stencil（`r.CustomDepth=3`）。Landscape 地面同样需要配置查询与渲染标记。

指示器材质要求地面 stencil、CustomDepth 与当前 SceneDepth 一致、表面法线 Z >= 0.5（最大 60 度），只绘制可见的地面朝上部分。深度检查防止采到 Hero 后方的地面标记后把图案投到 Hero 上；法线检查排除已标记平台的侧壁。保留角色 ReceivesDecals，其他贴花不使用本指示器材质的过滤。投影高度随范围增加以容纳坡道，不以缩小高度代替接收过滤。

新地图未配置地面时，点目标不会退回任意 Visibility 命中；应先完成上述配置。查询通道和 stencil 是本地交互/视觉约定，不能作为服务器命中或权限依据。

HUDOwnerView 新增 CastRangeBonus、AttackRange，展示 schema 从 5 升至 6；客户端/服务器同版本部署。ASC 与 owner View 的范围字段跨流未匹配时暂停预览。核心 `combat_v1_rc1` 及核心 schema 不变。

客户端定义身份由同代的 Public UnitView 与 HUDOwnerView 核对。`ACombatUnitCharacter::GetUnitDefinitionId()` 返回未复制的服务器初始化缓存，不能作为客户端预览就绪条件；独立联机测试同时检查范围字段和本地视觉对象，避免只验证 Standalone。

## 5. 训练场与验证

打开 `/Game/Combat/Demo/Indicators/L_CombatIndicators` 后 Play：Q 定点打击（单位），W 范围冲击（点圆），E 直线冲击（点方向），R 自我恢复（无目标）。样例使用已有 Ability 子类和公共 Action，独立 UnitData/AbilitySet/GameMode，不替换原 Demo 英雄。

按 W 瞄准，将光标从圈内移到圈外，观察琥珀虚线；在可导航的远处地面确认，观察服务器追近。瞄准中右键取消，确认没有移动；尝试 Q 点空地、友方和敌方；瞄准时操作 HUD/日志验证不穿透。快施方式在 Controller 默认属性中切换。原 Demo 可检查 Q 悬停的普攻范围和 Toggle。

`Combat.Input.AbilityAim.*` 覆盖状态机、Action 参数、精确选敌、成本/冷却、追击、过期输入及 Owner 清理；既有 Input/HUD/Targeting、资产、三 Target、PIE 和 Dedicated 证据写入 Spec。

原生 PIE 场景 `-CombatAimPIESmoke` 额外验证实际 HUD 悬停、标准/快施、世界点击、取消和可导航远点的追近。自动化编排须等待编辑器导航构建完成再启动 PIE，避免在模板关卡首次生成 NavMesh 时复制空导航数据；测试 Actor 只临时创建，不保存到训练地图。

同时添加 `-CombatAimGroundSmoke` 时，原生场景继续检查鼠标指向 Hero 的地面落点，并捕获关闭/圆形/直线指示器的实际 GPU 像素。测试保留真实 Hero 网格与生产指示器材质，独立检查无标记道具、其他 stencil、坡道和高台；捕获组件注册后关闭其局部曝光、泛光与时域处理，避免把邻近亮地面导致的曝光变化误判为贴花覆盖。生产视口另存截图，生产后处理参数保持原值。最终 AIM-002 证据见 [修复 Spec](../Specs/AIM-002-ground-only-indicators.spec.md)。

扇形和向量二次指向属于后续 P1，本版不新增相应权威目标协议。2026-09-13，用户完成 AIM-001 与 AIM-002 的实机验收；复杂多层地形和其他渲染平台仍需后续专项验证。
