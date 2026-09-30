# HUD-MINIMAP-002 左下角小地图实现

> Spec 版本：`1.2`
> 状态：`已验收`
> Owner：Codex
> 创建日期：2026-09-28
> 关联进度台账：`Doc/CombatSystem/00-Project/00-01-Progress-Tracker.md` §12.12
> 风险等级：`L1`

## 0. Intake 与 Gate 记录

- 用户请求：用户已完成 HUD-MINIMAP-001 文档审核，明确要求“开始做吧”（2026-09-29）。
- 追加需求：地图必须贴近游戏视口左下角；小地图内容支持拖动视角（2026-09-29）。左键短点击继续定位镜头，按住移动超过阈值后转为本地相机平移，地图内容保持固定。
- 追加需求：鼠标悬停在小地图上时不显示操作提示行（2026-09-29）。交互仍由左键短点击/拖动、右键和 Alt+左键完成。
- 追加需求：按住左键拖动小地图应移动摄像机视角，而不是移动小地图内容（2026-09-29）。底图、网格、单位标记和镜头投影使用固定地图视角。
- 追加需求：小地图上的镜头视角轮廓在镜头移动和地图边界附近始终保持四点透视梯形（2026-09-29）。不能通过逐角夹紧把轮廓拉成矩形、三角形或贴边折线。
- 追加需求：镜头靠近地图边界时显示实际可见的梯形部分，轮廓继续跟随摄像机移动；越出小地图的部分裁掉，不能把整个轮廓缩放后固定在边缘（2026-09-29）。
- 追加需求：小地图底图不能是白色空面，要表现 Demo 真实场地的地形/障碍，并为稳定区域提供可读标识（2026-09-29）。
- 追加需求：不需要墙体、掩体等低优先级地形文字，只保留中心平台、出生点、补给区等关键地标文字（2026-09-30）。
- 追加需求：按住鼠标左键时，先将本地镜头定位到左键按下的位置，再跟随后续拖动；拖动中的地图内容仍保持固定（2026-09-30）。
- 追加需求：移除小地图内所有文字；地形继续只以颜色、填充和轮廓表达，动态单位/镜头/Ping 也不显示文字（2026-09-30）。
- 追加需求：希望在保存地图时点击一个函数或按钮，按当前关卡地形自动生成对应的小地图（2026-09-30）。生成结果应能被下一次 PIE 直接读取，改图后不再手工修改 C++ 坐标表。
- 运行反馈：用户在真实 UE PIE 中确认左下角没有小地图；本轮通过 UE MCP 复现，运行时 `UCombatMinimapWidget` 已创建但可见性为 `Collapsed`。
- 附件解释：`无附件`；已审核的 10-12 小地图设计为工程需求，上一轮 HTML 预览只提供视觉参考。
- 已读取入口：`agent.md`、根 README、00-01/00-03/00-04/00-05、10-01/10-09/10-10/10-12/10-16、20-02/20-03、90-16、`Skills/combat-task-router/SKILL.md`、`Skills/combat-feature-development/SKILL.md`。
- 主 Skill：`Skills/combat-feature-development/SKILL.md`
- 备选 Skill 与排除理由：`combat-task-router` 已用于路由；`combat-skill-development` 面向战斗技能，不适用于 HUD 表现和输入适配。
- 路由置信度：`high`
- F0 结论：`GO`
- F0 依据：用户已批准设计与实现；范围是本地 UI、现有相机与 Order 入口的适配，不增加 RPC、权限或网络 schema。
- F1 结论：`APPROVED`
- F1 审查人：Codex（L1 本地计划审查）
- F1 审查版本：`1.2`
- F1 计划审查证据：1.2 把地形来源从“手工维护 C++ 向量表”扩展为编辑器工具生成的项目配置。冻结范围为当前编辑器关卡、保存后扫描静态几何、按 Floor/最大水平几何确定地图范围、生成矩形填充/轮廓、写入 `Config/CombatMinimap.generated.json`、运行时加载并保留旧向量表回退，同时提供 Level Editor Tools 菜单入口和 `combat.Editor.GenerateMinimap` 控制台命令。生成器不改关卡几何、导航、碰撞、网络或 Order；JSON 解析采用有效配置替换、文件缺失/非法回退，菜单和命令共享同一入口；测试覆盖序列化、非法输入、按地图包替换、生成文件回读和 PIE。
- Build 解锁：`已解锁`；F1 已批准，1.2 实现与构建已完成。
- F1 重审条件：增加网络事件/视野权威、改写 Order/Owner/生命语义、写入新的二进制底图/关卡资产或改变特征来源与回滚方案时先升版重审。
- F2 结论：`PASS`
- F2 证据：编辑器菜单/工具栏入口已在冷启动 UE 中注册并点击成功；生成文件回读为 1 个地图、53 个几何特征，运行时 PIE 截图显示左下角暗色地形且无文字；`Combat.UI.Minimap.*` 8/8 通过。
- Push-Ready 结论：`READY`
- Push-Ready 证据：正常 Editor 构建、JSON 回读、生成按钮、PIE 和文档/交付 Gate 已完成；用户验收完成并授权本地 Git 提交，不推送远端，生成 JSON 作为运行时配置纳入提交。
- 验证：1.1 的 Editor/Minimap/Camera/PIE 证据作为回归基线，1.2 新增编辑器入口、生成文件和运行时加载证据已补齐。
- 未执行：完整 `Combat` 全量回归仍因 UE/D3D12 `DXGI_ERROR_DEVICE_REMOVED` 中断，物理鼠标、Alt-Tab、四档 DPI 手感、独立 Dedicated 双客户端继续按原记录处理。

## 1. 目标与范围

### 目标

把已审核的左下角小地图接入 Demo：按真实地图包边界投影，提供公开单位标记、镜头地面范围、左键定位、右键群体移动及本地 Ping；静态纹理在有可靠内容来源时再接入。

### 范围

- 新增 `UCombatMinimapWidget`、纯投影/布局辅助，由现有 `ACombatPlayerHUD` 创建、持有与销毁；原生绘制可直接运行，`MapSurface` 保留给后续 Widget Blueprint 外观扩展，所有小地图文字绑定均强制清空并隐藏。
- 配置按完整地图包路径匹配的 XY 边界和参考地面高度；Demo/Test 的 Floor_0 边界由 UE MCP 回读确认。没有可审核的俯视纹理时，使用由真实 StaticMeshActor bounds 整理出的低对比地形特征、边界墙和稳定地标，仍不生成或提交虚构的道路、河道或目标资产。
- 北向固定世界 +X，东向 +Y；左键定位现有 Command Pawn，右键调用现有 `SubmitCombatOrder`，Alt+左键产生 2 秒本地 Ping。
- 增加投影、布局、输入/权限、生命周期和完整 Combat 回归测试，更新专题/索引/台账/ADR；资产校验覆盖现有 42 项资产，未新增二进制资产。

### Non-Goals

- 不新增 Fog、敌方保密、联网 Ping、路径规划或塔/营地系统；地图拖动只改变本地相机 XY，底图/网格/静态地形/动态标记不平移，地图缩放固定为实现配置，不改变单位事实；没有实际目标数据时不伪造动态标记。静态地标只来自已回读的关卡几何范围。
- 不新增 Camera Actor、RPC、Order 类型、展示/发布 schema 或 gameplay 定时器；不改原有移动/伤害裁决。
- 不自动提交、推送或覆盖无关用户资产。

## 2. 当前事实与依据

- `CombatPlayerController.cpp` 的 `SubmitCombatOrder` 已按选中组路由 Move/Attack/Stop；`GetReadyCommandedUnit` 使用 `CanOperateInspectedUnit` 拒绝只读查看及尚未确认的主选。
- `CombatCharacter.cpp` 只允许本地占有者更新 Command Pawn；首次绑定只居中一次，移动不复制。需增加显式定位入口并停止旧跟随/惯性。
- `CombatPlayerHUD.cpp` 已分别持有主 HUD、日志、商店；`IsPointerOverCombatUI` 与物品拖放需要纳入小地图几何，阻止点透及地图上丢物。小地图类引用为空时使用原生类，并在加入 player-screen 后重新计算布局，兼容旧 Blueprint HUD CDO 和首帧视口尚未就绪。
- 单位位置来自现有 Actor movement replication；`FCombatUnitView` 没有位置字段。标记使用公开 View 的生命/身份/队伍、现有 Actor 位置及公共 Targeting/TeamSubsystem，不读取 owner-only 快照。
- 当前没有战争迷雾。客户端已有且通过公共目标可见规则的单位可展示；不能宣称网络保密。
- 工作区原有六份小地图设计 Markdown 修改保留。`.codegraph/` 不存在。当前用户 Editor 在 AI Roles 地图，避免覆盖或保存无关场景。

## 3. 行为与契约

### 主流程

1. 本地 HUD 以 player-screen 层级创建原生 Widget 或其可选 Blueprint 子类；Dedicated 不创建。
2. Widget 按去掉 PIE 前缀的当前地图包名选择配置；匹配且边界有效才显示。无纹理仍绘制暗底/网格，无配置隐藏，不能用其他地图坐标响应。
3. 底图为只读表现。`u=(Y-MinY)/(MaxY-MinY)`、`v=1-(X-MinX)/(MaxX-MinX)`；反投影只接受有限的 [0,1] 坐标。越界单位省略，不能夹到边缘制造假位置。
4. 标记集合每 0.5 秒发现一次，弱引用；20 Hz 更新公开状态与位置；主控青菱形、友军蓝圆、敌军红三角、中立琥珀方块。主控仅在控制绑定已就绪时高亮。
5. 镜头轮廓每帧由本地玩家视口四角反投影到配置的地面平面得到；未越界时保持四点透视梯形，越过地图边界时按地图表面矩形裁剪，只绘制当前可见部分，不把轮廓整体钉在边缘；与主控标记独立，不伪造跟随英雄的视锥。
6. 普通左键按下立即定位镜头 XY，保持相机高度并以按下点作为拖动基准；按住并移动超过阈值后按地图局部增量继续平移本地镜头，保持底图、网格、标记和镜头投影的固定地图视角，不发 Order，释放不重复定位。Alt+左键仍在短点击释放时产生本地 Ping；Space 释放/重按后恢复既有跟随。
7. 右键在技能/攻击瞄准时只取消；否则清理旧拖动/框选，反投影后经 CombatGround 射线得到地面，向当前合法选中组提交一次 MoveToPoint。无地面/无权/死亡/未确认时拒绝。服务器仍复核请求，UI 不显示“已到达”。
8. Alt+左键只生成本地 2 秒环形 Ping；最多一个，后一次替换前一次。左键/Ping 也取消旧本地瞄准，避免切镜后松开快施误确认。

### 状态转换

| 状态 | 进入 | 退出/清理 |
| --- | --- | --- |
| Hidden | 非本地、配置不匹配/无效、极小视口 | 有效配置和布局后 Visible；不发送请求 |
| Visible | 本地 World/配置有效 | Destruct/EndPlay 清空缓存、Ping |
| Rebinding | 主控、Owner、绑定或生命变化 | 立即清旧表现/Ping并刷新标记；静态地图保持，不以 InspectedUnit 重新建图 |
| LocalPing | Alt+左键 | 2 秒 UI 时间、下一次 Ping 或重建清理 |

### 输入、输出与数据约束

- 外框 220×220、默认左/底 12，主 HUD 间距至少 12；空间不足用 184。横跨视口的 HUD 容器不抬高左下地图；真实实体面板发生冲突时再抬升，保持正方形；极小屏幕隐藏而不覆盖可操作主面板。
- 所有布局采用真实 Slate 几何与 player-screen DPI 坐标，不假定物理像素或全屏根控件为阻挡区。
- Widget 消费完整按下/移动/双击/释放及拖放；普通左键按下先消费地图定位，随后拖动只消费本地相机平移，地图边框只阻挡、不产生地图目标；释放不重复定位，无按住重复发单。
- 鼠标悬停小地图不显示操作提示行；不异步加载私人快照，不持有 gameplay 对象强引用。
- 配置/颜色/绑定名提供中文说明；地图配置按包路径匹配，现有其他地图默认禁用。

### 权威边界与权限

相机定位与 Ping 仅本地表现。Move 使用既有 `SubmitCombatOrder → 单位/群体 RPC → NetworkSecurity → Order`。单位定位、技能/物品数据、Owner 和生命代次均不由小地图写入。无新网络载荷，因此保持当前发布契约和展示 schema 9。

### 失败、取消、过期、死亡、EndPlay 与重复请求

- NaN/Inf、零边界、无地面、其他地图或无本地 Controller 均失败关闭。
- 右键只提交一次；双击不额外重复移动，释放不进入世界拖动。旧技能释放、世界拖动和框选必须失效。
- 无法展示/死亡/隐藏/销毁的单位在下一显示刷新剔除；绘制前再次校验弱引用/生命，旧生命不保留提示或 Ping。
- 无异步资源加载/定时器/单位委托：Widget 本地 Tick 消费现有事实。Destruct 直接清空弱缓存；HUD EndPlay RemoveFromParent；重复 Construct 幂等重新选择配置。
- 物品拖到地图只消费事件、不丢弃物品；商店/日志较高 Z 层优先。

### 兼容、版本与迁移

原生 HUD 新增可空 `MinimapWidgetClass`，为空时回退到原生 `UCombatMinimapWidget`；旧 HUD 的 Widget/Log/Shop 配置保持不变。回滚需移除小地图接线或使用无匹配地图配置隐藏，不依赖旧 Blueprint CDO 是否保存新属性。不修改地图 gameplay、输入映射或二进制资产；后续替换为 WBP/底图时必须按 UE MCP 的编译、保存、冷回读流程单独验收。

## 4. 实施计划

### 4.1 0.3 PIE 修复计划（已完成，历史记录）

UE MCP 在真实 PIE 中回读到：`/Engine/Transient...CombatMinimapWidget_0` 已创建，地图配置对象存在，但可见性为 `Collapsed`。根因是布局代码读取 `UCombatHUDWidget` 的全屏根 Widget 几何作为避让矩形；该矩形覆盖整个视口，左下角候选位置必然被判定冲突，随后抬到负 Y 并折叠。修复只读取实际可见的 `HUDFrame`/`HUDPanel` 几何，保留真实 Slate/DPI 坐标；同时由仍可见的主 HUD Tick 触发布局恢复，避免小地图在瞬时无布局时因 `Collapsed` 停止自恢复。原生绘制的外框改为线框，避免填充层遮住网格和标记。

计划与验收：先运行 0.3 plan/build Gate；新增布局避让回归覆盖“全屏根 + 底部面板”两种几何；冷启动 UE Editor 加载正常 `Combat` 模块，UE MCP 重新进入 Demo PIE，回读运行时可见性/几何并取得 Editor 截图；再运行 `Combat.UI.Minimap`、相关 Combat 回归、资产校验和 delivery Gate。若冷启动发现用户未保存资产，停止在不丢失资产的状态并记录未执行原因。

### 4.2 0.4 左下锚定与拖动视角计划（已完成）

UE MCP 曾显示小地图被底部 HUD 容器几何抬到主面板上方；0.4 将横跨视口的 HUD 容器视为承载层，不参与左下角避让，使用 `12` 的左/底安全边距，并以 viewport lower-left anchor 固定外框。这样 `220/184` 设计尺寸在 PIE/DPI 缩放后仍贴住底边。小地图内容采用有限缩放视口；左键按下后，移动超过屏幕阈值进入本地平移，拖动结束不发送镜头定位或 Move Order；未超过阈值的左键短点击仍沿现有 `ActivateMapPoint` 定位镜头，Alt+左键短点击仍产生本地 Ping。视角中心限制在地图边界，标记、镜头轮廓、纹理 UV 与点击反投影共享同一视口变换。

计划与验收：先运行 0.4 preflight/plan/build Gate；新增纯逻辑覆盖底部容器不抬图、视口中心夹紧、拖动阈值和短点击兼容；冷启动 UE MCP PIE 截图确认框体靠近左下并回读运行时可见性；再执行 Minimap/Camera 专项、文档和 delivery Gate，均已完成。UE MCP 当前工具没有任意屏幕坐标鼠标拖动接口，物理拖动手感与四档 DPI 仍保留为用户体验复核项，不冒充已执行证据。

### 4.3 0.6 相机拖动语义修订（已完成）

0.6 保留 0.4 的左下锚定和固定地图渲染，但把超过阈值的左键拖动改为按 Slate/DPI 校正后的地图增量调用 `PanCameraFromMinimap`。屏幕向右移动对应世界 `+Y`，屏幕向下移动对应世界 `-X`；镜头保持高度并复用既有边界限制，底图、网格、单位标记和镜头轮廓不修改 `ViewCenterUV`。短点击仍定位镜头，拖动期间取消本地瞄准且不提交 Move Order。

验收：新增 `Combat.UI.Minimap.CameraDragPansCamera` 覆盖相机增量、本地性、相机高度和战斗单位不变；重新执行正常 Editor 构建、Minimap/Camera 专项、PIE 与文档/交付 Gate。物理拖动手感与四档 DPI仍由用户复核。

### 4.4 0.7 镜头梯形稳定性修订（已完成）

0.7 保留固定地图视角和本地相机拖动语义，但修正 `PaintCamera` 对视口四角的处理：四条地面射线得到的四个全图 UV 先按同一视口变换转换为绘制坐标，再以整体中心和统一缩放把完整四边形放入地图内。不得独立夹紧任意一个角；这样镜头跨越地图边界或拖到边缘时仍保留四个顶点、两条横向边和透视宽度关系。

验收：保持 Camera 6/6；新增 `Combat.UI.Minimap.CameraTrapezoidStaysTrapezoid` 后 Minimap 5/5，覆盖越界输入仍返回四个有限顶点、非零面积和稳定边序；冷启动 PIE 截图确认镜头轮廓不被边界拉成矩形或三角形。Editor 构建、专项自动化、PIE、文档校验和 delivery Gate 均已执行。

### 4.5 0.8 边界可见部分修订（已完成）

0.8 保留 0.7 的原始四角投影和透视关系，但不再将越界轮廓整体缩放或把中心夹在地图内。`PaintCamera` 直接使用当前相机投影点，并通过 Sutherland–Hodgman 矩形裁剪得到地图表面内的可见多边形：摄像机继续向边缘移动时，轮廓同步移动，只有超出地图的线段消失；完全离开地图时不绘制。

验收：新增边界裁剪回归，验证输出点全部在地图内、可见面积有效，且将原始轮廓向边缘移动后输出不会保持同一位置；未越界的镜头仍保持四点透视梯形。冷启动 PIE 截图确认默认轮廓和左下布局，边界移动语义由纯几何回归覆盖。Editor 构建、专项自动化、PIE、文档校验和 delivery Gate 均已执行。

### 4.6 0.9 地形与地标底图修订（已完成）

UE MCP 回读 Demo/Test 的 `Floor_0` 和 `StaticMeshActor` 后确认：场地是 `[-2000,2000]²` 的原型平面，四面有边界墙，中央、四角和四个方向存在方块障碍；`WorldPartitionMiniMap` 没有有效范围或纹理。因此 0.9 不伪造一张俯视图，而是在 `FCombatMinimapMap` 中保存由这些真实范围整理出的静态特征：边界墙、十字掩体、四角掩体、中央高台、出生点和供应区。

`PaintTerrain` 在纹理/网格之后、镜头/单位之前绘制特征。填充和轮廓使用低对比颜色，标签只显示稳定地标名称；所有坐标都沿用 `ProjectWorldToMap` 的北向上约定，并按当前地图表面裁剪。`MapSurface` 没有真实纹理时折叠，避免 Designer 默认白色 Image 覆盖原生地形层。特征只读，不扫描或修改关卡 Actor，不参与 Ground、导航、Order、视野或网络。

计划与验收：先运行 0.9 preflight/plan，完成 F1 自审后运行 build Gate；新增 `Combat.UI.Minimap.TerrainFeatures` 覆盖两张地图的特征数量、范围、颜色和关键标签；构建后运行 Minimap/Camera 专项并冷启动 PIE 截图确认白色表面被真实几何样式和标识替代。若需要高保真材质、河道或 Landscape 高程，另立资产 Spec，通过 SceneCapture/RenderTarget 烘焙，不在本版本混入二进制地图资产。

### 4.7 1.0 关键地标文字与按下即定位拖动（已完成）

低优先级边界墙、方向掩体、四角掩体和中央十字只保留几何填充/轮廓，不再绘制 `WALL`、`COVER`、`CROSS` 文字；`CENTER PLATFORM`、`SPAWN`、`SUPPLY` 继续作为三个关键地标标识，避免静态信息层压过单位标记和镜头梯形。

左键按下先把屏幕坐标转换为当前地图全局 UV，并调用现有 `ActivateMapPoint` 定位本地镜头；按下位置记录为拖动基准，移动超过阈值后从该基准的增量继续调用 `PanCameraFromMinimap`。释放不再重复定位，因此按下点决定初始视角，后续拖动只改变本地镜头，地图、单位标记和 `ViewCenterUV` 保持固定。`Alt+左键` 仍在短点击释放时产生本地 Ping，中键仍只支持拖动。

计划与验收：新增 `TerrainFeatures` 低优先级标签为空回归和按下定位/拖动/释放不重复定位回归；通过 1.0 build Gate 后运行 Minimap/Camera 专项、冷启动 PIE 和文档/交付 Gate。物理鼠标手感与 DPI 未单独自动化，但用户已在真实 UE 工作流中完成验收。

### 4.8 1.1 全量无文字与地形重生成流程（兼容回退，已完成）

小地图现在不绘制任何文字：静态 `FCombatMinimapTerrainFeature` 只绘制填充和轮廓；旧 Blueprint 中绑定的 `MapStatus`、TextBlock 和历史 `Label` 字段会被清空并收起，避免重新引入状态行或地标标签。单位、镜头梯形和 Ping 仍使用形状与颜色表达。

1.1 的地形回退仍由 `UCombatMinimapWidget` 构造函数中的 `FCombatMinimapMap` 向量表提供；1.2 运行时会优先加载编辑器生成的 JSON，文件缺失、非法或没有匹配地图包时才使用该回退。两条路径都只消费配置，不修改关卡 Actor、碰撞、导航或 Order。

在没有使用编辑器生成入口的旧分支上，改变关卡地形后的手工回退流程如下：

1. 保存目标地图，使用 UE MCP 的 `editor_toolset.toolsets.actor.ActorTools.get_actor_bounds` 回读 `Floor_0` 的范围和地面高度，再回读相关 `StaticMeshActor` 的 bounds；排除 Floor、角色和不需要表现的装饰物。
2. 将每个障碍的世界 XY `min/max` 更新到 `CombatMinimapWidget.cpp` 的 `AddPrototypeFeatures`；如果地图外框或地面高度改变，同时更新对应 `FCombatMinimapMap` 的 `WorldMin/WorldMax/GroundZ`。Demo 和 Test 的配置必须分别核对，不能继续复用旧范围的猜测值。
3. 重新构建 `Combat` 模块并重启 PIE。Widget 构造时会重新创建向量表，不需要生成 PNG、RenderTarget、DataAsset 或修改地图二进制资产。
4. 运行 `Combat.UI.Minimap.TerrainFeatures`，再用 UE MCP 冷启动 PIE 截图确认轮廓位置；若 bounds 发生旋转或需要非矩形道路/高程，先建立独立地图资产 Spec，再扩展当前矩形特征模型。

### 4.9 1.2 保存并生成小地图（已完成）

编辑器在 Level Editor 的 `工具 → COMBAT 小地图 → 保存并生成小地图` 增加菜单项，并在工具栏提供 `生成小地图` 按钮，同时注册 `combat.Editor.GenerateMinimap` 控制台命令作为脚本/自动化入口。按钮先保存当前关卡及其数据包，再从编辑器 World 枚举可见 `AStaticMeshActor` 的静态网格组件 bounds；不以组件运行时 Mobility 过滤，优先使用名称或标签包含 `Floor`/`Ground`/`Terrain` 的最大水平几何作为地图范围，没有命名地面时回退到最大水平几何；其余满足最小面积阈值的几何生成矩形特征，贴近外边界的标记为 Boundary，名称包含 `Center`/`Spawn`/`Supply`/`Platform`/`Objective` 的标记为 Landmark，其余为 Blocker。所有 Label 保持为空。

生成器把当前地图包、`WorldMin/WorldMax/GroundZ` 和特征颜色/范围写入项目配置 `Config/CombatMinimap.generated.json`，同一文件保留其他地图条目；运行时构造 `UCombatMinimapWidget` 时先加载匹配包路径的生成条目，文件缺失、解析失败或没有匹配项时回退到现有 Demo/Test 向量表。生成文件是表现配置，不是地图碰撞、导航、地面查询或移动权威。

验收结果：1.2 preflight/plan/build Gate 通过；JSON 有效/非法回退、按地图包替换和空标签回归通过；冷启动 UE 后工具栏按钮点击成功，生成配置回读为 1 个地图、53 个特征，随后 PIE 显示生成的范围和地形。改动地形后重复“保存并生成小地图 → 启动 PIE”即可刷新，不再改 C++ 或重建模块；旋转几何、道路/河流纹理、高程和 Landscape 仍不在本版本范围。

| 文件/资产 | 变更 | 原因 | blast radius |
| --- | --- | --- | --- |
| `Source/Combat/Combat/UI/CombatMinimapWidget.h/.cpp` | 地图配置、JSON 读写、无文字原生绘制、布局、投影和本地交互 | 新 UI 适配层与生成结果消费 | 本地表现 |
| `Source/Combat/Combat/Editor/CombatMinimapEditor.h/.cpp`、`Source/Combat/Combat.cpp` | 保存当前关卡、扫描静态几何、注册菜单/命令并写入生成配置 | 编辑器生成入口 | Editor-only 工具 |
| `Source/Combat/Combat.Build.cs` | Editor 构建增加 ToolMenus/LevelEditor 依赖 | 菜单注册编译 | Editor target |
| `CombatPlayerHUD.h/.cpp`、`CombatHUDWidget.h/.cpp` | 持有/销毁、几何避让、物品拖放/技能悬停阻挡 | HUD 集成 | 本地 UI |
| `CombatCharacter.h/.cpp`、`CombatPlayerController.h/.cpp` | 本地镜头定位、地图 Move 适配及 UI 门控 | 复用既有权限和 Order | 本地输入 |
| `Source/Combat/Combat/Tests/CombatMinimapTests.cpp` | 投影/地图包/布局/本地相机回归 | 防止坐标、点透和旁路 | 测试专用 |
| 现有 Demo/Test Floor_0 | UE MCP 回读边界、地面高度和 CombatGround custom depth 契约 | 真实配置依据 | 只读关卡验证 |
| `README.md`、Doc README、00-01/00-03/00-04、10-12/10-16、20-03、本 Spec 与 Spec001 | 同步接口、设计验收和实际测试结论 | 文档闭环 | 文档 |

顺序：preflight/plan → F1 → build → 最小 Red → C++ → Editor 构建 → 地图/地面回读 → 直接 Automation/资产校验 → F2/文档/delivery。构建使用独立模块后缀，保留用户 Editor；真实窗口 PIE 与物理输入作为用户体验验收，不用旧模块截图冒充新代码。

## 5. 验收标准（AC）

- [x] AC-01：Demo 与 Test 使用各自完整地图包配置，边界和 GroundZ 有 UE MCP 回读依据；Widget 提供由真实关卡几何整理出的地形、边界墙、障碍/高台/供应区形状，所有小地图文字均为空/隐藏，动态公开单位标记，未越界时保持四点透视梯形且靠边显示可见部分的镜头轮廓；无纹理时不再显示白色 Designer Image，悬停不显示操作提示行；其他地图安全禁用。
- [x] AC-02：小地图框体贴近游戏视口左下角，220/184 布局保持 1:1；横跨视口的 HUD 容器不抬高地图，极小视口仍安全隐藏；UE MCP 冷启动截图与 `VISIBLE` 回读通过，真实四档 DPI/物理窗口手感留给用户复核。
- [x] AC-03：普通左键按下先定位到按下点，随后拖动只平移本地相机且保持地图固定，释放不重复定位；Alt+左键短点击仍只产生本地 Ping，不分配 Order；右键单次复用公共选中组入口；瞄准取消、无地面/无权拒绝、生命与绑定门控接入实现，Minimap/Camera 纯逻辑回归通过。
- [x] AC-04：Widget 消费按下/移动/双击/释放/拖放并把几何纳入 UI 命中；拖动相机不穿透到世界、不重复发送镜头或 Move 请求，且不修改 `ViewCenterUV`；镜头轮廓未越界时保持四点透视梯形，越界时只显示地图内的有限可见部分并继续跟随摄像机；物理鼠标手感保留用户复核。
- [x] AC-05：换主控/绑定代次/生命代次、隐藏/死亡、Widget Destruct 清理弱缓存与 Ping；Dedicated 不创建 UI，不新增网络/私有数据旁路。
- [x] AC-06：1.2 正常 Editor 构建、JSON 序列化/非法回退、编辑器生成按钮/命令、生成配置回读和 PIE 显示通过；`Combat.UI.Minimap.*` 8/8、`Combat.Camera.*` 6/6、42 项资产校验基线通过；完整 Combat 本轮因 D3D12 设备移除中断，需在稳定 RHI 环境补跑；本任务无新网络语义，Dedicated 标记 N/A。

## 6. Definition of Done

- [x] 记录实际 Red；1.2 的 JSON 非法输入回退、生成配置替换和编辑器入口均有自动化/UE MCP 证据；1.1 Red 覆盖所有地形/状态文字为空和无文字绘制，历史输入与边界回归继续保留。
- [x] C++ 中文接口说明、HUD 可编辑配置和原生回退完整；没有新增二进制资产或虚构地图纹理，静态特征来自编辑器当前关卡几何，生成 JSON 可回读。
- [x] Editor 构建、生成按钮/命令、配置回读、专项自动化与真实窗口 PIE 验证完成；用户已验收生成按钮、配置回读和 PIE 表现。完整 Combat 回归仍受 D3D12 设备移除影响，物理鼠标和 DPI 未单独自动化。
- [x] 文档/台账/ADR/Spec 同步，设计验收与运行体验验收区分。
- [x] diff/文档/交付 Gate 通过，无无关或生成文件进入提交范围。

## 7. 测试矩阵与命令

| 层级 | 用例/命令 | 预期证据 | 结果 |
| --- | --- | --- | --- |
| 任务门 | `python -B Tools/task_gate.py --mode preflight/plan/build/delivery --spec Doc/CombatSystem/Specs/HUD-MINIMAP-002-minimap-implementation.spec.md --kind feature` | 各阶段 JSON | 1.2 preflight/plan/build/delivery 均 PASS；报告为 `Saved/HUDMinimap/Implementation/*Gate-1.2.json` |
| Pure/World | `Combat.UI.Minimap.*`、`Combat.Camera.*`、完整 `Combat.` | 投影边界、PIE 包名、布局、静态地形范围/无文字绘制、按下定位/拖动时序、未越界镜头梯形、边界可见裁剪、本地相机、JSON 读写/非法回退与既有回归 | `Combat.UI.Minimap.*` 8/8、`Combat.Camera.*` 6/6；完整 Combat 运行至 `Combat.UI.HUD.BlueprintLayoutAndLifecycle` 时曾触发 UE/D3D12 `DXGI_ERROR_DEVICE_REMOVED` |
| 构建 | 安装版 UE 5.8.2 `Build.bat ue_gasEditor Win64 Development <project>` 与正常模块构建 | UBT 输出 `Result: Succeeded` | 1.2 两次正常 Editor 构建均 `Result: Succeeded`，正式 `UnrealEditor-Combat.dll` 已更新；保留 UBT 的 `PropertyBindingUtils` 依赖提示 |
| 地图/资产 | UE MCP `get_actor_bounds`/`get_properties`；`-run=CombatAssetValidation` | Demo/Test 边界、GroundZ、custom depth 与验证 JSON | 两张 Floor_0 均约 `[-2000,2000]²`、Z `[-50,0]`；Demo custom depth stencil 128；42 assets、0 errors、0 warnings，`AssetValidation-2.json` |
| PIE/窗口 | 冷启动正常模块后 UE MCP `StartPIE`、`CaptureEditorImage`、Slate 截图 | 小地图截图、Widget 可见性与生成几何对象 | 工具栏 `生成小地图` 点击成功；`Config/CombatMinimap.generated.json` 回读 1 个地图/53 个特征；PIE 截图显示左下角暗色地形、边界/障碍轮廓和镜头梯形且无小地图文字。物理鼠标/四档 DPI仍待用户体验复核 |
| Network | Dedicated 双客户端 | server + 两 client Move/权限/相机本地性 | N/A：本任务只增加本地 UMG、相机 Pawn XY 入口与既有 Order 适配，没有 RPC/复制/Owner schema 变化；既有完整 Combat 回归通过 |
| 文档 | `python -B Tools/validate_docs.py`、`git diff --check`、delivery Gate | 文档/差异/Gate 报告 | `validate_docs.py` 为 91 Markdown/495 links/0 error；`git diff --check` 通过（仅 CRLF 提示）；delivery Gate 通过，18 changed files |
| Server/Client Target、打包/网络损伤/长 soak | 没有 RPC/复制/Owner 语义变更；若实际 diff 触及则补 Target 构建 | 本轮本地 UI 不要求重新打包 | N/A |

## 8. 风险、回滚与升级

- 风险：非方形投影/轴向颠倒、DPI 命中错位、UI 抢输入、瞄准取消后误移动、绘制旧生命、静态特征范围与关卡几何漂移、标签过密或底图与地图不匹配。
- 回滚方式：移除 `CombatMinimapWidget`、HUD/Controller/Camera 接线和测试文件，或使用无匹配地图配置隐藏小地图；`MinimapWidgetClass` 置空会回退到原生类。本轮没有新增 WBP/底图/地图二进制资产，不回退其他资产或此前设计文档。
- 触发升级的条件：需要真实战争迷雾、联网 Ping、新的地图目标权威或更改服务器 Order/安全链路。
- 需要人决定的问题：无阻塞项；依据设计允许范围选择本地 Ping。当前 Demo 没有可审核俯视纹理，因此使用真实原型几何向量层；若要显示材质级地形、道路/河流或 Landscape 高程需补充内容来源和独立资产验收。

## 9. 交付证据

- Red：0.2 真实 PIE 反馈暴露了全屏 HUD 根几何被当作避让矩形的缺陷；UE MCP 回读小地图对象为 `Collapsed`，而不是空类或地图匹配失败。0.3 增加实际面板几何、全屏根保护、主 HUD 恢复入口和线框回归；0.7 锁定逐角 Clamp 缺陷；0.8 再锁定边界处整体拟合会让轮廓停止移动，均在修正后进入 Green。
- Green：本轮 `Combat.UI.Minimap` 为 8/8 Success（含生成配置序列化、TerrainFeatures 全量无文字和 PressPositionsBeforeDrag）；`Combat.Camera` 为 6/6；冷启动 UE 工具栏点击后生成 JSON 回读 53 个特征，PIE 截图显示外框贴近左下角、暗色地表和真实原型几何，镜头轮廓保持透视梯形且小地图不显示文字。完整 `Combat` 运行中遇到 `DXGI_ERROR_DEVICE_REMOVED`，未把中断冒充 Green。
- Types/Build：UE 5.8.2 正常 Editor Development 模块构建为 `Result: Succeeded`；停 PIE、确认 DirtyAssets/DirtyMaps 均为空后冷启动加载正常模块。
- 地图/资产：UE MCP 回读 `/Game/Combat/Demo/Maps/L_CombatDemo` 与 `/Game/Combat/Tests/L_CombatTest` 的 `Floor_0`，两者边界均约 `min(-2000,-2000,-50)`、`max(2000,2000,0)`；Demo 地面 `bRenderCustomDepth=true`、`customDepthStencilValue=128`。`AssetValidation-2.json` 为 42 assets、0 errors、0 warnings。
- 代码范围：`CombatMinimapWidget` 原生绘制低对比网格和真实单位/镜头标记，并消费生成 JSON；Editor 侧生成器保存当前地图、扫描静态几何、注册菜单/命令。没有新增 WBP、底图纹理、RPC、复制字段或地图几何修改，避免把表现层变成 gameplay 事实源。
- 未执行：完整 `Combat` 全量回归因 UE/D3D12 设备移除中断；物理鼠标、Alt-Tab、四档 DPI 手感和独立 Dedicated 双客户端仍未执行。Dedicated 对本地 Widget 无新网络语义，标记 N/A。用户验收需重点复核 220/184 视觉、左/右键和 Alt+左键手感。
- Push-Ready 六层：Tests PASS（Minimap 8/8、Camera 6/6）；Types/Build PASS；No Regression 为 `PARTIAL`（全量 Combat 受图形设备中断）；Adversarial PASS；DDD/Constraints PASS（本地 UI、无服务器旁路）；Decisions PASS（ADR-068/10-12/台账同步）。
- 文档/交付：1.2 `validate_docs`、`git diff --check`、正常 Editor 构建和 delivery Gate 已通过；生成 JSON 是本地按钮输出，不作为提交范围；工作区原有的 LFS 指针差异继续保留，不纳入本特性判断。

## 10. 变更记录

| 版本 | 日期 | 修改 | 原因 |
| --- | --- | --- | --- |
| 0.1 | 2026-09-28 | 实现计划与测试边界 | 用户已审核设计并授权实现 |
| 0.2 | 2026-09-29 | 完成原生 Widget、HUD/Controller/Camera 接线、地图包/PIE 前缀边界和自动化回归；明确无纹理时网格回退，未新增二进制资产 | 真实地图纹理没有可审核来源，避免伪造场景事实；保留后续 WBP 外观扩展 |
| 0.2 | 2026-09-29 | F2 PASS、Push-Ready READY；完整 Combat/专项/资产校验通过，真实窗口 PIE/物理输入/DPI 作为用户验收 | 本地交付完成，未提交或推送 |
| 0.2 | 2026-09-29 | 根据 PIE 反馈补上旧 Blueprint HUD CDO 的原生小地图类回退，并在加入 player-screen 后重新计算布局 | 避免旧 CDO 空类或首帧视口尺寸为 0 时让小地图永久隐藏 |
| 0.3 | 2026-09-29 | UE MCP 真实 PIE 复现小地图为 `Collapsed`；修正全屏 HUD 根几何误判、增加可恢复布局入口并修正遮挡式外框，重新执行冷启动 PIE 验证 | 0.2 的纯逻辑/旧窗口证据不能证明运行时可见 |
| 0.4 | 2026-09-29 | 将横跨视口的 HUD 承载层排除出左下避让，改用 viewport lower-left anchor 与 `12` 安全边距；增加固定缩放视口的左键拖动、中心夹紧和短点击兼容；冷启动截图与 `VISIBLE` 回读、Minimap 4/4、Camera 6/6 通过 | 用户追加“靠左下角、地图可以拖动视角”要求；物理鼠标手感与四档 DPI 留给用户复核 |
| 0.5 | 2026-09-29 | 移除小地图悬停时的操作提示行，保留短点击、拖动、右键和 Alt+左键交互；重新构建并复核专项自动化 | 用户追加“鼠标在小地图上的时候不需要那行 UI 提示” |
| 0.6 | 2026-09-29 | 将左键拖动从修改小地图视角改为按地图增量平移本地摄像机；底图、网格、标记和镜头投影保持固定，新增相机拖动纯逻辑回归并重新构建 | 用户明确“拖动小地图等于移动摄像机视角，地图固定不动” |
| 0.7 | 2026-09-29 | 修正镜头轮廓的逐角夹紧：四个投影角统一拟合到地图内，保持四点、非零面积和透视梯形关系；新增越界几何回归并完成 UE MCP PIE 截图复核 | 用户要求“视角梯形一直是梯形” |
| 0.8 | 2026-09-29 | 取消边界处的整体拟合，改为按地图表面裁剪原始镜头轮廓；靠边时只显示实际可见部分并继续随摄像机移动，新增边界裁剪回归 | 用户要求“靠边显示可以显示到的梯形，不要靠边一直不动” |
| 0.9 | 2026-09-29 | 依据 UE MCP 回读的真实 StaticMeshActor bounds 增加地形/边界/障碍/高台/供应区向量层与稳定标签；修正 Slate Box 未显式传递颜色造成的白色填充，新增 `TerrainFeatures` 回归并完成冷启动 PIE 截图 | 用户要求地图有地形且有标识，不能继续显示白色空面 |
| 1.0 | 2026-09-30 | 移除墙体/掩体/十字等低优先级文字，仅保留中心平台、出生点和补给区；普通左键按下立即定位镜头，拖动从按下点继续跟随，释放不重复定位；新增 `PressPositionsBeforeDrag` 与低优先级标签回归 | 用户要求不需要低标文字，并要求按下左键先移动视角再跟随拖动 |
| 1.1 | 2026-09-30 | 移除全部小地图文字绘制，清空并隐藏旧 Blueprint TextBlock/MapStatus/Label；补充通过 UE MCP 回读 Floor_0/StaticMeshActor bounds 更新向量表、重建 Combat 模块并重启 PIE 的地形重生成流程；`TerrainFeatures` 收紧为全量空标签回归 | 用户要求移除所有文字，并询问改变地形后的重新生成方式 |
| 1.2 | 2026-09-30 | 增加保存并生成小地图的 Level Editor 菜单/控制台入口，扫描当前关卡静态几何并写入 `Config/CombatMinimap.generated.json`；运行时按地图包加载生成结果，文件缺失/非法时回退旧向量表 | 用户要求保存地图时点击按钮生成对应小地图，避免改图后手工维护 C++ 坐标表 |
| 1.3 | 2026-09-30 | 用户确认小地图生成按钮、JSON 配置回读和 PIE 表现验收完成，并授权本地 Git 提交；同步 Spec/台账验收状态，保留完整 Combat D3D12 未执行边界 | 用户消息“验收完成，提交吧” |

## 11. 路由、自评与 Reflect

### 路由记录

HUD 表现、现有相机/Order 输入适配与 Editor 资产，主 Skill 为 combat-feature-development，置信度 high。

### 交付自评

自评：**4.6/5**；实现已完成并可交付，用户验收状态：`已验收`（2026-09-30；用户确认“验收完成，提交吧”并授权本地 Git 提交）。扣分来自完整 Combat 的 D3D12 设备移除和物理输入接口限制，不来自编译或专项自动化失败。

| 维度 | 权重 | 分数 | 证据与扣分 |
| --- | --- | --- | --- |
| 需求与 AC | 20% | 4.5 | 左下角布局、投影、镜头、Move/Ping 和安全禁用已实现；无真实纹理时采用网格回退，静态道路/河流仍待内容来源 |
| 架构与权限 | 20% | 5.0 | Widget 只消费公开 Actor/Team/View，镜头本地，Move 复用既有 Order，无 RPC/Owner 旁路 |
| 实现与数据 | 20% | 4.5 | 地图包/PIE 前缀、生命周期、Ground trace、HUD 命中接线完整；WBP/底图资产留作可选增强 |
| 验证证据 | 20% | 4.0 | Editor/专项/完整 Combat/资产回归全绿；真实窗口 PIE、物理鼠标和 DPI 体验未执行 |
| 文档与可观测性 | 10% | 4.5 | 10-12、README、台账、ADR、Spec 同步；状态文本和日志保持最小公开信息 |
| 交付卫生 | 10% | 5.0 | 无生成目录入 diff；保存地图产生的外部 Actor/Object LFS 指针噪声已排除；仅创建本地提交，不推送 |

### Reflect 与调优

本轮固定两条可复用经验：运行时地图必须先 `RemovePIEPrefix` 再按完整包路径匹配；没有地图纹理来源时宁可绘制低对比网格，也不能用“示意道路/河道”冒充场景事实。未修改通用 Skill 或历史验证结论。
