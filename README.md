# ue_gas Combat

这是一个面向 Unreal Engine 5.8 的 Dota-like 战斗框架。项目以 Gameplay Ability System（GAS）承载 Attribute、GameplayTag、GameplayEffect 和 GameplayAbility，并用自定义运行时补齐 Order、AttackRecord、Modifier Hook、Combat Scheduler、Projectile、Thinker、Aura、Motion、统一伤害/治疗事务和多人可观测性。

Combat 当前位于 `Combat` 单 Runtime Module 中，不是独立插件或独立 Module；项目文件和 Target 名称仍保留 `ue_gas`。

## 当前基线

- 当前发布契约：`combat_v5_acquisition_rc1`，Contract 为 5，物品与经济开关分别开启，GameplayTag/Event 为 3，经济目录 schema 为 1、表现 schema 为 4；Content/Formula/RNG 保持当前冻结值。物品与经济验证状态见 ITEM-001/ECON-003 台账。
- 权威模型：服务器结算；客户端 TargetData 仅作为请求，目标、资源和结果由服务器复核。
- 资源所有权：金币及未来战略资源归本局玩家的 `ACombatPlayerController` 经济账本；英雄切换不会清空资源。六格装备、三格背包、物品实例 `Holder`、槽位和库存修订仍归各自英雄，商店只把结果交付当前主控英雄。纯资源余额变化保留服务器诊断事件，但不进入战斗 UI 日志；分域读取与展示边界见 [RES-001](Doc/CombatSystem/Specs/RES-001-player-resource-ownership.spec.md)。
- M0-M8 共 82 个 Task 已完成并通过用户验收；最近一次发布 Gate 记录为 `Combat.*` 40/40、Editor/Server/Client 构建、资产校验和 Dedicated 双客户端容量场景通过。
- M8 之后增加了卓尔游侠远程攻击 Demo、头顶资源/状态/施法条、伤害治疗跳字，以及底部居中的英雄、技能、Buff HUD。C++ 提供只读数据，Widget Blueprint 维护布局和视觉；等级经验与技能加点已接入服务器权威成长组件，六格装备与三格背包已接入物品主动、被动、场景交互和权威投影。
- 完整 gameplay 预测回滚、跨进程确定性 Replay、召唤物/幻象仍未接入；ECON-003 单一库存交易、锁定合成和出售已通过用户代码 review，NullRHI PIE、独立 Dedicated 双客户端 300 秒经济 soak、Windows cook 与资产校验通过，验证边界见 ECON-003 Spec。
- Post-M8 新增肉鸽成长型合作防守设计提案：每局服务器种子、个人强化、团队商店即时消耗品、波次回防、事件和 Boss；Phase 1 已验收，个人强化选择/重抽/种子面板已由 DEF-004 接入；独立灰盒地图与防守循环见 [DEF-002](Doc/CombatSystem/Specs/DEF-002-defense-phase1-skeleton.spec.md)，当前验证/验收状态以台账为准，见 [DEF-001 设计文档](Doc/CombatSystem/20-Content/20-05-Roguelite-Defense-Game-Design.md) 与 [Spec](Doc/CombatSystem/Specs/DEF-001-coop-defense-design.spec.md)。

以上测试数字是已归档的最近验收证据，不自动代表任意工作区修改已经重新验证。实时任务状态以 [开发进度台账](Doc/CombatSystem/00-Project/00-01-Progress-Tracker.md) 为准。

## 快速入口

1. 安装 UE 5.8，并确保 Git LFS 已拉取 `.uasset`、`.umap` 等二进制资产。
2. **一般情况下请使用下载版 UE 启动项目并打开 `ue_gas.uproject`。** 可玩 Demo 地图位于 `/Game/Combat/Demo/Maps/L_CombatDemo`。只有需要测试 Dedicated Server 时，才切换到源码编译版 UE。
3. 自动化测试地图位于 `/Game/Combat/Tests/L_CombatTest`。
4. Combat C++ 入口位于 `Source/Combat/Combat`；新增技能先阅读 [公共技能扩展与迁移指南](Doc/CombatSystem/20-Content/20-03-M8-Public-Extension-Guide.md)。
5. AI 协作开发先阅读 [AI-Native 开发流程与文档体系](Doc/CombatSystem/00-Project/00-05-AI-Native-Development-Workflow.md)，新需求或修复使用 [Spec 模板](Doc/CombatSystem/Specs/_template.spec.md)。
   进入 BUILD 前必须先完成 PLAN 计划审查并记录 F1=`APPROVED`；按顺序运行 `task_gate.py --mode preflight`、`--mode plan`、`--mode build`。F1 通过前不得修改代码、工具脚本、蓝图或资产。
6. 任务开始用 [Intake](Doc/CombatSystem/00-Project/_intake-template.md) 整理边界，交付前按 [Gate 检查表](Doc/CombatSystem/00-Project/_gate-checklist.md) 记录验证结果；这些内容可以合并到任务 Spec，独立记录时使用 [交付记录模板](Doc/CombatSystem/00-Project/_delivery-record-template.md)。
7. 让 Agent 判断应该调用哪个 Skill 时使用 [Combat 任务路由 Skill](Skills/combat-task-router/SKILL.md)，它会在交付后按证据自评并执行受控调优。
8. 开发功能任务时使用 [Combat 功能开发 Skill](Skills/combat-feature-development/SKILL.md)，它会按 AI-Native 流程推进 Spec、Gate、验证和 Reflect。
9. 实现 GAS/Combat 技能时使用 [Combat 技能开发 Skill](Skills/combat-skill-development/SKILL.md)，它会在通用流程上补齐 Ability、DataAsset、Modifier、Projectile、蓝图和技能验收。

项目 Skill 全部位于仓库内的 `Skills/`，按项目路径读取，不安装到用户级 Codex Skill 目录，也不影响其他项目。

Demo 操作：左键点击英雄在本地 HUD 查看信息；有控制权时同时选中它。按住 **Shift** 点击增删组成员，左键拖动框选自己的单位，Shift 拖框追加，最多选择 8 个。绿色圈为主选、蓝色圈为其他选中单位，橙色圈为只读查看目标。右键点地面让选中组移动，点敌人让选中组持续普攻并自动追击；**A** 后左键指定群体攻击，**S** 停止选中组，**Escape** 取消瞄准/拖框。Q/W/E/R 和物品仅操作主选英雄，点击切换英雄不会停止旧英雄或释放控制权；查看无权单位时不会给旧英雄下令。Demo 初次出生提供两名独立英雄。详见 [多操规格](Doc/CombatSystem/Specs/CTRL-001-multi-unit-selection.spec.md)。卓尔 **Q** 切换“霜冻之箭”，默认开启，W/E/R 为空；A 模式点地面不执行 Attack Move。

技能指示器训练场：打开 `/Game/Combat/Demo/Indicators/L_CombatIndicators`，Q/W/E/R 分别体验单位、圆形、直线和自身技能。目标技能默认按键瞄准、左键确认，Escape/右键取消；琥珀虚线表示超距，确认后由服务器追近。Controller 可切换按下/松开快施。配置见 [10-13 技能指示器](Doc/CombatSystem/10-Architecture/10-13-Skill-Indicators.md)。

防守原型：打开 `/Game/Combat/Demo/Defense/L_Defense`，点击 UE 顶部 Play（Alt+P），在中央点击 **锁定种子并开始**；可留空随机或输入十进制/0x 种子。准备 40 秒后两路敌人进攻核心，完成五个普通波与 Boss 获胜，核心摧毁则失败。波间整备 25 秒，开局与第 1–5 波后有 15 秒个人三选一；每局可重抽两次，费用 60/100 金币。阵亡后约 8 秒安全复活。

Phase 3 首项攻城兵已接入第 **3、5 波**，每路一名橙色车体、前方带冲槌的敌人；总出生数仍为8/12。攻城兵650生命、移速140、每3秒80攻击，沿原路线攻击核心，追猎号角对它增伤15%。数值仍在灰盒调优，见 [DEF-010](Doc/CombatSystem/Specs/DEF-010-phase3-siege-enemy.spec.md)。

购物复用原商店：点击右下角 **金币按钮**，在 **基础物品 → 团队补给 · 获得即用** 中 **右键购买**。六件商品由本人支付、全队受益、每局全队限购一次，成功立即消耗且不占库存；悬停查看剩余次数、领取者与到期波次。准备/波间/Boss 整备开放团队领取；普通装备仍按原商店规则购买。原左侧独立商店及专用购买 RPC 已撤除。新物品的“获得即使用”配置也支持满库存拾取和奖励，失败保留金币或原地面物品。团队目录现包含六件正式团队道具：**神殿屏障**（220 金币，核心获得当前最大生命8%的护盾，至下一波清场）和 **应急石**（180 金币，修复核心当前最大生命15%）。默认5000生命核心对应400护盾/750修复；顶部核心条显示剩余屏障。**援军印记**（200金币）提供接下来两次新死亡的救援：复活等待各减少3秒，复活后获得最大生命20%的4秒护盾；购买前已阵亡者不追溯。悬停商品可看剩余援军、待复活和护盾保护人数。个人强化池现有24张卡，包含四家族终局卡；替换英雄时通用强化保留，不兼容卡休眠。Phase 2完成后，神殿守卫已作为Phase 3开局英雄接入；事件原型现已接入第2/4波后的事件阶段，支持安全选项、风险确认、10秒取消窗口、超时采用安全选项和下一波压力/奖励修正。防守装备已接入首批四件：铁制弓、迅弦、石卫徽记、猎手瞄准镜；其余四件、远程敌人、多阶段Boss与局末统计仍留后续。见 [DEF-009](Doc/CombatSystem/Specs/DEF-009-defense-phase2-completion.spec.md) 与 [DEF-013](Doc/CombatSystem/Specs/DEF-013-defense-events.spec.md)。见 [DEF-006](Doc/CombatSystem/Specs/DEF-006-reuse-shop-use-on-acquire.spec.md) 与 [DEF-004](Doc/CombatSystem/Specs/DEF-004-defense-choice-flow.spec.md)。

StateTree AI 演示：打开 `/Game/Combat/Demo/AI/L_CombatAI`，服务器自主单位在导航就绪后追近并攻击木桩。PIE 中选中演示场实例可点击 **AI 返回起点 / AI 攻击靶子**。阶段 A 提供显式 Move/Attack/Cast、单命令写入者、攻击边界切换和玩家接管；Profile、树接线与后续阶段边界见 [20-04 配置指南](Doc/CombatSystem/20-Content/20-04-StateTree-AI-Guide.md)。

阶段 B 角色演示：`/Game/Combat/Demo/AI/Roles/L_CombatAI_Roles` 提供宽视野入口和清晰目标标识，野怪自动感知、追击后归位，小兵交战后继续路线；复用 Guard/Engage/Return/Lane/Patrol StateTree 节点与子树。当前证据与验收状态见 [AI-003](Doc/CombatSystem/Specs/AI-003-statetree-roles.spec.md)。

阶段 C 战术演示：`/Game/Combat/Demo/AI/Tactics/L_CombatAI_Tactics` 使用显式 v2 Profile 展示最高效用战术选择。Hero Bot 会在合法攻击边界从持续普攻切换到主动治疗，Ranged Guard 通过服务器 EQS 选择站位后仍由公共 Move Order 移动；World 预算限制并错峰感知/EQS，旧 v1 Profile 保持阶段 A/B 时序。AI-004 已完成三 Target、专项/全量自动化、资产冷回读、PIE、Dedicated 双客户端、cook 和 64/128/256 容量验证，并于 2026-09-22 通过用户验收；配置和边界见 [20-04](Doc/CombatSystem/20-Content/20-04-StateTree-AI-Guide.md) 与 [AI-004 Spec](Doc/CombatSystem/Specs/AI-004-statetree-tactics-capacity.spec.md)。

视角操作：鼠标贴近游戏视口边缘自动平移，无需抓取键；按住 **Space** 跟随主控单位，松开停留。首次进入定位一次，点击或框选切换英雄保持当前镜头位置。跟随中重新贴边会由滚屏接管，需松开重按 Space 才恢复跟随。UI、瞄准和失焦会阻止误滚屏。参数与验证边界见 [10-16 视角移动](Doc/CombatSystem/10-Architecture/10-16-Dota-Camera-Movement.md)。

以上按键统一通过 `/Game/Combat/Demo/Input/IMC_Default` 映射到 Input Action；可在该资产中改键。普攻选敌、选择/确认、Shift 追加、取消和停止的 Action 引用配置在 `BP_CombatDemoPlayerController` 的 `Input|Combat` 默认属性中，Space 跟随引用配置在 `Input|Camera`；边缘滚屏直接检测视口位置，不需要 Action。

底部 HUD 随本地玩家的指挥单位切换。悬停技能、Buff 或头像上的属性可查看详情；技能左键点击复用 Q/W/E/R 施法，目标技能再左键点世界确认，未瞄准时右键技能固定详情，关闭按钮或 Escape 取消固定/瞄准。HUD 阻止鼠标穿透，物品槽通过显式使用或拖放操作提交请求。英雄等级和经验环读取服务器成长快照；有技能点且满足英雄等级时，技能图标上方显示“+”按钮，点击请求服务器加点。Q 槽显示“霜冻之箭”及服务器权威的“自动/关闭”状态，W/E/R 保留空位。界面配置入口见 [10-12 底部 HUD](Doc/CombatSystem/10-Architecture/10-12-Bottom-HUD-Design.md)；左下角小地图已接入原生绘制，按地图包匹配边界，使用暗色地表和真实原型几何轮廓，不显示任何文字；镜头未越界时显示透视梯形，靠边时只显示地图内的可见部分并继续跟随摄像机；普通左键按下先定位视角，再跟随拖动，地图固定，支持选中组移动和本地 Ping。改动关卡后在 UE Editor 使用 `工具 → COMBAT 小地图 → 保存并生成小地图` 或工具栏 `生成小地图`，再启动 PIE 即可刷新 `Config/CombatMinimap.generated.json`，详见同文档的 HUD-MINIMAP-001 章节与 [HUD-MINIMAP-002 Spec](Doc/CombatSystem/Specs/HUD-MINIMAP-002-minimap-implementation.spec.md)。

点击 HUD 左上角的 **战斗记录** 查看服务器确认的伤害、治疗、技能、状态与死亡/复活事件。记录包含毫秒时间戳、彩色名称、实际数值及生命前后值；攻击者、目标、类别与时间范围可以组合筛选，默认最近 30 秒，最多保留 512 条。上滚暂停跟随，勾选“跟随最新”回到底部；按住顶部栏左键拖动窗口，松开停留，关闭再打开保留当前位置；关闭窗口仍记录，Escape 收起窗口。物品选项筛选拾取、丢弃、换槽和消耗；物品造成的伤害与治疗仍归对应类别。布局与颜色在 `WBP_CombatLog` 调整，详见 [战斗记录接入](Doc/CombatSystem/10-Architecture/10-12-Bottom-HUD-Design.md#7-战斗记录窗口hud-log-001)。

开发调试时可在 Standalone 或服务器控制台执行 `combat.Debug.AddExperience 200`，给当前 World 的首个玩家主控单位增加 200 点经验；也可追加单位对象名或 `ActorUniqueId` 精确指定目标，例如 `combat.Debug.AddExperience 200 BP_DrowRanger_C_0`。命令仅在非 Shipping 构建注册，并复用服务器权威成长组件；客户端执行不会直接修改等级、经验或技能点。可先用 `combat.Debug.Unit <ActorUniqueId|Name>` 查询单位名称和 ID。

物品操作：右键场景物品走近拾取；左键装备栏主动物品或按 **1–6** 使用，目标类物品沿用技能瞄准。右键库存槽打开菜单，拖到另一格交换，拖到场景放下。背包禁用主动和被动，换回装备等待 6 秒；背包与地面冷却半速。Demo 初始携带指环、鞋、三份药剂和法杖，场景另有光环和普攻触发物品。详见 [物品系统指南](Doc/CombatSystem/10-Architecture/10-14-Item-System.md)。

## 运行时主链路

```text
客户端输入 -> Order RPC 安全检查
服务器 StateTree -> Brain 控制代次与单写入者检查
  -> OrderComponent（Move / Attack / Cast / Stop / Pickup / Drop；Swap 即时事务）
  -> Targeting 服务器复核
  -> Ability / AttackRecord
  -> Combat Scheduler 驱动前摇、引导、周期与过期
  -> Damage / Heal / Modifier / Projectile / Thinker / Motion 公共入口
  -> GAS Attribute / ActiveGE 真实落账
  -> Combat Event、Unit View、Projectile Presentation、Overhead UI / Bottom HUD
```

关键原则是“一个事实只有一个权威来源”：最终属性来自 ASC 聚合，Health/Mana 通过统一资源与事务入口修改，异步实体由稳定 Handle 和 generation 管理，结束和广播保持 exactly-once。

## 目录导航

| 路径 | 内容 |
| --- | --- |
| `Source/Combat/Combat/Ability` | ASC、GameplayAbility 基类、AbilityTask 与 EffectContext |
| `Source/Combat/Combat/AI` | 服务器 StateTree 宿主、Profile/Schema、跨状态工作区与 Order Bridge |
| `Source/Combat/Combat/Combat` | Damage、Heal、Transaction 和 Effect 工具 |
| `Source/Combat/Combat/Modifiers` | ActiveGE/Runtime 映射、Hook、叠层、周期和驱散 |
| `Source/Combat/Combat/Order`、`Attack` | 指令状态机、追击、AttackRecord、法球和普攻时序 |
| `Source/Combat/Combat/Projectile`、`Thinker`、`Aura`、`Motion` | 异步空间实体与强制位移 |
| `Source/Combat/Combat/Items` | 物品定义、服务器实例、装备背包、拾取投影与主动被动接入 |
| `Source/Combat/Combat/Data` | Unit/Ability/Modifier/Projectile/AbilitySet PrimaryDataAsset |
| `Source/Combat/Combat/Network`、`View`、`UI` | RPC 防护、公共/拥有者 View、头顶表现与底部 HUD |
| `Source/Combat/Combat/Tests` | `Combat.*` Automation 测试 |
| `Content/Combat/Demo` | 可玩 Demo 地图、`Heros/DrowRanger`、木桩、霜冻之箭、远程攻击和输入资产 |
| `Content/Combat/Tests` | PIE、Dedicated 与容量测试地图 |
| `Doc/CombatSystem/00-Project` | 当前状态、AI 开发流程、路线图、测试计划和决策入口 |
| `Doc/CombatSystem/10-Architecture` | 当前运行时、联机、移动和 UI 架构契约 |
| `Doc/CombatSystem/20-Content` | 示例技能、技能模板和公共扩展指南 |
| `Doc/CombatSystem/30-Tooling` | UE MCP 操作与诊断配方 |
| `Doc/CombatSystem/90-History` | M0–M8 冻结决策和验收证据 |

## 文档阅读顺序

- 初次了解：本文 → [文档总索引](Doc/CombatSystem/README.md) → [范围、架构与硬约束](Doc/CombatSystem/10-Architecture/10-01-Scope-Architecture.md)。
- 理解联机交互：[客户端与服务器交互流程](Doc/CombatSystem/10-Architecture/10-09-Client-Server-Interaction.md) → [Order 与移动](Doc/CombatSystem/10-Architecture/10-07-Order-Movement.md) → [Ability 与目标](Doc/CombatSystem/10-Architecture/10-03-Ability-Targeting-Blueprint.md) → [网络与 UI](Doc/CombatSystem/10-Architecture/10-08-Data-Network-Observability.md)。
- 理解服务器权威单位移动：[服务器权威单位移动改造与验收](Doc/CombatSystem/10-Architecture/10-10-Server-Authoritative-Movement-Kickoff.md)；当前端到端链路以 10-09 为准。
- 调整头顶 UI：[C++ 与蓝图边界、事件接口和资产配置](Doc/CombatSystem/10-Architecture/10-11-Overhead-Blueprint-UI.md)。
- 调整底部 HUD：[定稿布局、Widget Blueprint、拥有者快照和物品接线](Doc/CombatSystem/10-Architecture/10-12-Bottom-HUD-Design.md)。
- 开发技能：[Ability、目标与蓝图接口](Doc/CombatSystem/10-Architecture/10-03-Ability-Targeting-Blueprint.md) → [Damage/Heal](Doc/CombatSystem/10-Architecture/10-05-Damage-Heal.md) → [示例技能](Doc/CombatSystem/20-Content/20-01-Example-Skills.md) → [技能模板检查表](Doc/CombatSystem/20-Content/20-02-M6-Skill-Template-Checklist.md)。
- 修改内核：先读对应 10-02–10-08 专题，再检查 [决策与缺口登记](Doc/CombatSystem/00-Project/00-04-Decisions-Gaps.md) 和 [生命周期审计](Doc/CombatSystem/90-History/90-16-M8-Lifecycle-Audit.md)。
- 验证发布边界：[候选发布决策](Doc/CombatSystem/90-History/90-15-M8-Release-Candidate-Decision.md) → [M8 验收记录](Doc/CombatSystem/90-History/90-17-M8-Acceptance.md)。
- Agent 或自动化开发：先读根目录 [agent.md](agent.md)。
- AI 开发流程与 Spec：先读 [00-05 AI-Native 开发流程](Doc/CombatSystem/00-Project/00-05-AI-Native-Development-Workflow.md)，状态和证据仍以 [00-01 开发进度台账](Doc/CombatSystem/00-Project/00-01-Progress-Tracker.md) 为准。

## 验证命令模板

不同机器的 UE 安装路径不相同，先复制环境模板并填写本机路径。`.env` 只保存在本地并已加入 Git 忽略；不要把真实机器路径提交到仓库。

```powershell
Copy-Item .env.example .env
notepad .env
```

配置两个编辑器入口：

- `UE_INSTALLED_EDITOR`：Launcher/下载版 `UnrealEditor.exe`。这是**默认入口**，用于打开项目、日常 Demo、普通 Editor、Automation 和资产校验。
- `UE_SOURCE_EDITOR`：从源码编译的 `UnrealEditor.exe`。这是 **Dedicated Server 测试专用入口**，仅在构建/运行 Server/Client Target 或 Dedicated smoke 时使用；工具会从该路径上溯找到源码引擎根目录，并检查 `Engine/Build/BatchFiles/Build.bat`。

检查配置状态：

```powershell
python Tools/ue_environment.py check
python Tools/ue_environment.py check --require editor
python Tools/ue_environment.py check --require dedicated --json
```

`--require dedicated` 返回 0 表示 Dedicated 可以运行，返回 2 表示配置缺失或路径无效；后者应把 Dedicated 记为“未执行”，不能记为通过。满足检查后运行 Dedicated Server + 两客户端 smoke：

```powershell
& .\Tools\RunDedicated.ps1
```

日志和进程摘要写入 `Saved/UEEnvironment/Dedicated/`。脚本只清理本次启动的 UE 进程。

ECON-003 的经济闭环与物品争抢需要分别运行：

```powershell
& .\Tools\RunDedicated.ps1 -InstalledEditor -Economy
& .\Tools\RunDedicated.ps1 -InstalledEditor -Items -TimeoutSeconds 120
```

DEF-013 独立打包 Target 验证使用专用入口，构建源码 Server/Client，分别 Cook/Stage 为 WindowsServer/WindowsClient 后再启动：

```powershell
py -3 -B Tools/run_defense_targets.py --package
# 归档已准备好时，清理后以新日志目录重新运行独立 Dedicated + 双客户端。
py -3 -B Tools/run_defense_targets.py
```

归档位于 `Saved/DEF-013/targets-04/Archive`，每轮报告使用新的 `smoke-*` 目录。不要直接启动仓库 `Binaries/Win64` 下的 Server/Client exe 读取 raw Content；它们需要各自平台的 Cooked 容器和 AssetRegistry。工具校验真实 NetMode、三端相同 RunId、schema 4、事件请求/奖励结果，并在早退、断言或超时时失败。

`-InstalledEditor` 明确选择安装版 Editor，以独立 `-server`/`-game` 进程运行网络场景；它不代表已运行打包可执行文件。`-Economy` 让每端约每 30 秒执行买入、锁定、解锁合成和出售，持续 300 秒，默认超时 390 秒，日志位于 `Saved/UEEnvironment/Dedicated-Installed-Economy/`。`-Items` 同时验证 64 Unit/256 Modifier 容量，日志位于 `Saved/UEEnvironment/Dedicated-Installed/`；因物品争抢会切换主控单位，两种场景不能同时启用。省略 `-InstalledEditor` 时使用源码 Editor。

需要构建 Server/Client Target 时，使用源码版 UE：

```powershell
& "<UE_SOURCE_ROOT>\Engine\Build\BatchFiles\Build.bat" ue_gasEditor Win64 Development "<REPO>\ue_gas.uproject" -WaitMutex
& "<UE_SOURCE_ROOT>\Engine\Build\BatchFiles\Build.bat" ue_gasServer Win64 Development "<REPO>\ue_gas.uproject" -WaitMutex
& "<UE_SOURCE_ROOT>\Engine\Build\BatchFiles\Build.bat" ue_gasClient Win64 Development "<REPO>\ue_gas.uproject" -WaitMutex
```

普通 Editor、Automation 和资产校验使用下载版 UE：

```powershell

& "<UE_INSTALLED_ROOT>\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "<REPO>\ue_gas.uproject" `
  -unattended -nop4 -nosplash -NullRHI -NoSound `
  -ExecCmds="Automation RunTests Combat.;Quit" `
  -TestExit="Automation Test Queue Empty"

& "<UE_INSTALLED_ROOT>\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "<REPO>\ue_gas.uproject" `
  -run=CombatAssetValidation -Unattended -NoP4 `
  -Report="<REPO>\Saved\CombatValidation\CombatAssetReport.json"
```

上面的源码引擎 Build 命令用于源码 Editor 或 Server/Client Target 的测试准备；日常打开项目不需要源码引擎。Dedicated Server + 两客户端 smoke 通过 `Tools/RunDedicated.ps1` 选择对应 Editor。

上面的 `<UE_SOURCE_ROOT>` 和 `<UE_INSTALLED_ROOT>` 只是命令中的说明占位符；实际路径以 `.env` 中的两个编辑器文件位置为准。Dedicated Server/Client Target 需要支持该 Target 的源码引擎。详细环境边界见 [M1 环境决策](Doc/CombatSystem/90-History/90-02-M1-Environment-Decision.md)，完整测试分层见 [测试计划](Doc/CombatSystem/00-Project/00-03-Test-Plan.md)。

文档体系校验可在仓库根目录运行：

```bash
python3 -B Tools/validate_docs.py
git diff --check
```

任务开工和交付使用可失败的流程 Gate；`<task-id>` 必须对应本次任务的 Spec。`feature` 覆盖功能/Bug/资产变更，`process` 覆盖工具和流程变更，`docs` 覆盖纯文档变更：

```bash
python3 -B Tools/task_gate.py --mode preflight --spec Doc/CombatSystem/Specs/<task-id>.spec.md --kind <feature|docs|process>
python3 -B Tools/task_gate.py --mode plan --spec Doc/CombatSystem/Specs/<task-id>.spec.md --kind <feature|docs|process>
```

完成计划审查，在 Spec 写入 F1=`APPROVED`、审查版本和证据后，再进入实现与交付：

```bash
python3 -B Tools/task_gate.py --mode build --spec Doc/CombatSystem/Specs/<task-id>.spec.md --kind <feature|docs|process>
python3 -B Tools/task_gate.py --mode delivery --spec Doc/CombatSystem/Specs/<task-id>.spec.md --kind <feature|docs|process>
```

Gate 会检查 Spec、Skill 路由、F0/F1/F2、计划审查版本和证据、Push-Ready、验证/未执行记录，以及行为变更是否配套测试和文档；失败结果必须先修复并回写 Spec。Spec 实质变更后先升级版本并重新审查，旧批准不能用于新范围；检查工具不追溯证明修改先后。

文档检查覆盖必需入口、目录迁移、Markdown 本地目标路径、尾随空格和 Spec 格式；页内锚点、外部链接和文档语义需要另行审查。当前采用本地开发与用户验收流程，交付或提交前运行上述命令。它们不能替代 UE 编译、Automation、PIE 或 Dedicated 验证。

需要保存文档校验结果时，添加 `--report Saved/DocValidation/report.json`。修改校验脚本时，再运行其正反例测试：

```bash
python3 -B -m unittest discover -s Tools/Tests -p 'test_validate_docs.py' -v
```

若 IDE 构建报 `Unable to delete hot-reload file`，先检查日志中的 DLL 是否仍被同工程的 UE 进程占用；后台没有可见窗口的进程也可能持有模块。保存资产并完全退出占用进程后，运行上面的常规 Editor 构建，让 UBT 清理热重载记录并恢复 `UnrealEditor-Combat.dll`。使用 `-ModuleWithSuffix` 做临时验证后，交付前应完成这一步，并退出仅用于启动验证的 Editor 实例，避免影响下一次 IDE 构建。

防守Phase3继续接入游猎刺客：第4/5波双路紫色尖锥，优先威胁低生命比例英雄，原追猎号角生效；攻城已验收。当前实现/验证进度见 [DEF-011](Doc/CombatSystem/Specs/DEF-011-phase3-assassin-enemy.spec.md)。

第二英雄神殿守卫已接入开局选择（2026-10-19已验收）：Play后先点“改用神殿守卫”，再锁定种子并开始、选择强化。Q指向盾击、W固守、E震地，被动三次普攻受击后下次普攻蓄盾；仅开局可选择一次，见 [DEF-012](Doc/CombatSystem/Specs/DEF-012-phase3-guardian-hero.spec.md)。事件阶段在第2/4波清场后显示，安全/风险按钮位于防守构筑面板。

防守装备首批四件在原商店 **基础物品 → 防守装备** 右键购买：铁制弓180金币（直接普攻+12%）、迅弦220金币（普攻间隔−8%）、石卫徽记160金币（生命上限+120，不补血）、猎手瞄准镜260金币（对精英/攻城/Boss伤害+12%）。每款九格最多一件，每次整备最多成功购买两件，出售不返还本窗次数；战斗/事件/终局关闭该分类购买，背包与重新装备等待沿用原规则。想选神殿守卫须先选角再购物。其余四件仍后续，见 [DEF-014](Doc/CombatSystem/Specs/DEF-014-defense-equipment-foundation.spec.md)。
