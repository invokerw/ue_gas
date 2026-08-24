# 13 UE MCP 开发工作流

## 1. 定位

UE MCP 是本项目操作 Unreal Editor、检查资产/蓝图、构造测试场景和运行 PIE 验证的标准辅助通道。它的价值是让 Agent 直接读取 Editor 的真实状态并执行结构化操作，减少手工点击、资产路径猜测和只靠源码推断蓝图状态造成的误差。

UE MCP 不替代：

- C++ 源码和 Git diff 评审。
- UnrealBuildTool/编译器。
- Automation Spec、Functional Test 和 Dedicated Server 测试。
- Combat 系统的服务器权威规则。

正确关系是：

```text
源码/设计定义语义
  -> UE MCP 操作和读取 Editor/Content/PIE
  -> 编译与 Automation 验证
  -> UE MCP 回读资产、日志和场景结果
  -> Gate 验收
```

## 2. 当前项目接入状态

仓库已经包含：

- `ue_gas.uproject`：启用 `ModelContextProtocol`、`MCPClientToolset`、`AllToolsets`。
- `.codex/config.toml`：配置 MCP server `unreal-mcp`，端点为 `http://127.0.0.1:8000/mcp`。

这表示项目具备接入配置，不等于每次会话都已经可调用。开始 Editor 任务前仍需确认：

1. 当前打开的是本仓库对应的 Unreal 项目。
2. Unreal Editor 和本地 MCP endpoint 正常运行。
3. 当前会话实际暴露了所需工具及其参数 schema。
4. PIE、编译、资产保存等前置状态允许执行目标操作。

禁止根据历史经验虚构工具名或参数；先发现当前工具，再调用。

## 3. 何时优先使用 UE MCP

| 工作 | UE MCP 用途 | 仍需的最终证据 |
| --- | --- | --- |
| 工程/插件检查 | 读取项目、插件、Editor 状态 | uproject/Build.cs diff、启动/编译成功 |
| GameplayTag | 检查 Editor 可见 Tag、引用和配置结果 | Native Tag 源码、启动注册测试 |
| DataAsset/GE | 创建或检查类型、字段、引用、默认值 | 资产校验、Automation、cook 检查 |
| Blueprint | 检查父类、组件、变量、事件和编译状态 | 蓝图编译无错误、公共 API 测试 |
| 测试地图 | 放置 Unit/NavMesh/碰撞体、检查 World 设置 | Functional/PIE 测试可重复运行 |
| PIE | 启停、执行场景、读取 Output Log/对象状态 | 自动化断言、结构化 Combat Event |
| 网络调试 | 检查 PIE 配置和客户端可见状态 | Dedicated Server/Client 测试矩阵 |
| 问题定位 | 查询 Actor/Component/资产和运行时属性 | 最小复现测试和源码修复 |

纯 C++ 类型、模板、算法、Build.cs 和测试源码仍直接在仓库中编辑；不要为了“全部走 MCP”而绕开更准确的源码工具。

## 4. 标准操作闭环

每个 Editor/Content 任务遵守：

### 4.1 Read

- 查询当前 Editor/PIE 状态。
- 读取目标资产的真实路径、类、父类、字段和引用。
- 记录变更前值；不要根据文件名猜测资产内容。

### 4.2 Plan

- 明确只读检查还是会修改资产/场景。
- 列出精确目标路径和期望字段。
- 确认操作不会覆盖用户未提交的蓝图/关卡改动。

### 4.3 Mutate

- 使用当前会话实际提供的最小范围工具。
- 一次只完成一个可验证目标，例如创建一个 DataAsset 或修改一个字段组。
- 不批量修改整个 Content 根目录，不通过模糊名称匹配写入多个资产。

### 4.4 Verify

- 立即回读资产/Actor/字段，确认实际值和引用。
- 编译蓝图、保存明确目标资产。
- 运行对应 Automation/PIE 场景并读取错误、警告和 Combat Event。
- 检查源码和资产 diff；UE MCP 的“调用成功”不等于任务完成。

### 4.5 Record

- 在任务交付中列出 UE MCP 修改的资产路径。
- 记录编译/测试结果、关键截图或日志标识。
- 若工具不可用，说明采用的替代流程，不把未验证资产标为完成。

## 5. 各里程碑的 UE MCP 用法

| 里程碑 | 优先用途 |
| --- | --- |
| M0 | 检查现有 Collision Profile、GameplayTag、蓝图/资产命名，避免设计与工程现状冲突 |
| M1 | 验证插件/endpoint、Editor 启动、Native Tag 可见性、ASC 测试 Actor 和基础 PIE 地图 |
| M2 | 创建/检查初始化 GE、Damage/Heal GE、ModifierData、Shield/DOT/Stun 测试资产 |
| M3 | 创建 AbilityData/测试 Ability 蓝图，检查父类、事件、TargetData 和 compile status |
| M4 | 构建 Move/Cast/Attack 测试场景，观察 EQS、AI Move、OrderHandle 和 AttackRecord 状态 |
| M5 | 配置 ProjectileData、碰撞、Niagara/Cue，构建 Hook/Dragon Slave 场景并检查命中序列 |
| M6 | 批量前先验证技能模板，检查蓝图是否绕过公共 Damage/Modifier/Timer API |
| M7 | 驱动多客户端 PIE 诊断、View/UI 检查、日志/性能采样；最终仍跑 Dedicated 测试 |
| M8 | 复核资产编译、引用、地图和样例内容，配合全量 Automation/cook/release Gate |

## 6. GAS 专项检查清单

通过 UE MCP 创建或检查 GAS 内容时，至少验证：

- Ability/Modifier/Projectile DataAsset 的 DefinitionId 唯一且字段完整。
- GameplayAbility 蓝图父类是预期的 `UCombatGameplayAbility` 子类。
- GameplayEffect 的 Duration/Stack/GrantedTags/Modifiers 与 ModifierData 一致。
- Damage/Heal GE 只写 Incoming Meta Attribute，没有重复计算抗性或直接写 Health。
- 蓝图没有直接 SetHealth、SetActorLocation、自建 gameplay Timer 或绕过 ProjectileSubsystem。
- GameplayCue、Niagara、Collision Profile 和目标资产引用可解析。
- 蓝图编译无 error；warning 必须分类处理，不能只保存资产。

## 7. 测试与诊断工作流

推荐顺序：

1. 运行 C++ Automation，先验证纯语义和生命周期。
2. 用 UE MCP 打开/构造精确测试地图并确认对象配置。
3. 启动 PIE，触发单一测试场景。
4. 读取 Output Log、Combat Event、Actor/ASC/Tag/Attribute 状态。
5. 停止 PIE，确认 World teardown 没有残留 Schedule/Runtime/Delegate。
6. 对网络节点运行 Dedicated Server/Client 自动化；Network PIE 只用于快速诊断。

发现问题后先保存可复现输入和 EventId，再修改源码；不要在 Editor 中反复手调资产直到“看起来正常”而没有测试。

## 8. 安全与准确性边界

- 修改前解析并回读精确资产路径；禁止依赖搜索结果顺序选择目标。
- 对关卡、蓝图和 DataAsset 的批量写入先用一个测试资产验证 schema。
- 不删除、重命名或覆盖未知用户资产；此类操作需要明确目标和迁移计划。
- PIE 运行时区分 Editor World、PIE Server World 和 Client World，读取状态时注明 World/NetMode。
- 客户端看到的 Attribute/Tag/View 不能作为服务器战斗正确性的唯一证据。
- UE MCP 返回的文本/对象状态是诊断证据之一，不能替代 exactly-once 自动化断言。
- 工具调用失败、超时或 endpoint 不可用时安全停止 Editor 写操作，改用源码/命令行检查或等待环境恢复。

## 9. 任务交付模板补充

涉及 Editor/Content 的 Issue 在通用模板外增加：

```text
UE MCP 是否使用:
读取的 Editor/World/PIE 状态:
修改的资产绝对路径:
变更前后字段:
蓝图/资产编译结果:
PIE/Automation/Dedicated 结果:
回读验证:
未能通过 MCP 验证的部分:
```

## 10. 最低准入

- M1 必须完成 UE MCP endpoint/tool discovery 的 smoke check。
- 任何 UE MCP 资产写入都能列出目标并回读验证。
- 蓝图/资产任务同时有编译结果和对应测试，不以工具调用成功作为完成依据。
- Gate 评审能区分源码结果、Editor 资产结果、PIE 诊断和 Dedicated 权威测试。
