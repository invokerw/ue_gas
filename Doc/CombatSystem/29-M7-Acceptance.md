# 29 M7 验收记录：联机、UI、工具和性能

## 1. 验收状态

- 里程碑：M7
- Gate：G7 通过
- Task：9/9 已完成
- 用户验收：已验收
- 提交验收日期：2026-08-27
- 最终验收日期：2026-08-27

## 2. 完成范围

| Task | 实现结果 |
| --- | --- |
| NET-001 | Unit 支持 Automatic/Mixed/Minimal/Full ASC 复制策略；Automatic 按 PlayerController Owner 选择 Mixed 或 Minimal。网络 Owner 与 AIController 导航职责分离，保留 `OwnerActor=AvatarActor=Unit` 的 GAS 身份链 |
| NET-002 | Unit 批量 Order Server RPC 增加 ownership、正数 RequestId、最多 8 Order/4096 bytes、每连接 20 request/s + 32 burst token bucket、最近 128 RequestId 重放窗口及安全统计/失败 Tag |
| NET-003 | `UCombatUnitViewComponent` 复制 `FCombatUnitView` 与 FastArray Modifier View；UI 只接收稳定身份、状态、数值、叠层和服务器时间，不复制 `UCombatModifierRuntime` |
| NET-004 | Projectile 复制权威 Handle、DefinitionId 与 PredictionKey；客户端表现子系统用服务器视觉替换同键预测视觉，并对重复 Handle 做幂等协调；客户端视觉不能触发命中或结算 |
| OBS-701 | Combat Event schema v1 校验、RootEvent 查询、Unit dump、debug draw、metrics 控制台命令、滚动 Server frame p95/p99 与每连接最大发送带宽完成 |
| MCP-701 | [28 M7 UE MCP 联机诊断配方](28-M7-MCP-Diagnostic-Recipe.md) 固化 World 身份、复制矩阵、安全 RPC、事件、Projectile、性能与资产诊断顺序，并提供 MCP 不可用时的命令行等价入口 |
| DAT-701 | `CombatAssetValidation` commandlet 校验 DefinitionId、版本和 redirect，输出版本化 JSON 报告；非法内容可使命令失败 |
| PERF-701 | 冻结 30 Hz Dedicated 的单位、Runtime、回调、Server frame 与发送带宽预算，并在运行时预算评估器中接入真实采样 |
| TST-701 | `L_CombatTest` 增加 Dedicated 2-client、确定性 Stop RPC 与 `-CombatM7CapacitySmoke`；实际生成 64 Unit/256 Modifier，提供周期性能日志和 teardown 验证 |

完整网络与容量边界见 [27 M7 联机、可观测性与容量决策](27-M7-Network-Observability-Decision.md)。ADR-038/039/040 已关闭 GAP-021/019/018。

## 3. 构建结果

| Engine | Target | Configuration | 结果 |
| --- | --- | --- | --- |
| Installed UE 5.8 | `ue_gasEditor` | Win64 Development | Succeeded；中文反射说明最终复核 17/17 actions |
| Source UE 5.8（`D:\UE\UE`） | `ue_gasServer` | Win64 Development | Succeeded；中文反射说明最终复核 16/16 actions |
| Source UE 5.8（`D:\UE\UE`） | `ue_gasClient` | Win64 Development | Succeeded；中文反射说明最终复核 17/17 actions |

Server/Client Target 均编译 Network、View、Debug、Performance、Validation 与容量场景源码，未引入 Editor-only 运行时依赖。

## 4. 自动化与资产校验

Installed UE 5.8 冷启动执行全部 `Combat.*` 自动化：

```text
Test Completed. Result={Success}: 37
Test Completed. Result={Fail}: 0
Process exit code: 0
```

M7 新增 5 项测试全部通过：

| Test | 覆盖重点 |
| --- | --- |
| `Combat.Network.M7.ReplicationAndRpcSecurity` | 复制策略、ownership、RequestId、载荷、限频与重放安全矩阵 |
| `Combat.Network.M7.UnitModifierView` | Unit/Modifier 投影、FastArray 身份与 Runtime 隔离 |
| `Combat.Network.M7.ProjectilePresentationReconcile` | PredictionKey 替换、Handle 去重与服务器表现生命周期 |
| `Combat.Observability.M7.DiagnosticsValidationBudget` | schema、RootEvent、调试输出、资产报告与预算判定 |
| `Combat.Performance.M7.CapacityLifecycle` | World 内实际 64 Unit/256 Modifier 创建、计数与 teardown |

最终日志：`Saved/Logs/M7_Automation_ReflectionFinal_20260827.log`。其余 32 个 M0–M6 回归测试同时通过。

资产命令：

```text
CombatAssetValidation Version=1 Assets=2 Errors=0 Warnings=0
```

报告为 `Saved/CombatValidation/CombatAssetReport.json`，日志为 `Saved/Logs/M7_AssetValidation_FinalGate_20260827.log`。

## 5. Dedicated Server + 2 Client 容量 soak

Installed UE 5.8 使用独立 Dedicated Server 与两个独立 Client 进程运行 `/Game/Combat/Tests/L_CombatTest`。Server 添加 `-CombatM7CapacitySmoke`，实际场景结果：

```text
M7CapacityFixtureReady Units=64 Modifiers=256
M7ScenarioReady Players=2 Units=64 Mixed=2 Minimal=62 UnitViews=64 CapacityFixture=Ready CapacityBudget=Pass
Client 1: M7OrderBatchResult Accepted=true Orders=1 Failure=None
Client 2: M7OrderBatchResult Accepted=true Orders=1 Failure=None
```

两客户端均向自己拥有的 Unit 提交确定性 Stop Order，Server 接收两次且两个 owning client 均收到成功结果。AI Unit 保持 AIController 导航，同时 `GetNetOwner/GetNetConnection/GetNetOwningPlayer` 按 PlayerController Actor Owner 建立 RPC owning connection。

容量进程运行超过 90 秒，滚动 4096 frame 样本的最终指标为：

| 指标 | 实测 | 预算 | 结果 |
| --- | ---: | ---: | --- |
| Server frame p95 | 16.281 ms | <= 33.34 ms | 通过 |
| Server frame p99 | 22.392 ms | <= 50 ms | 通过 |
| 单连接最大发送 | 39.817 KiB/s | <= 256 KiB/s | 通过 |
| Unit / Modifier | 64 / 256 | >= 64 / 256 | 通过 |

Installed Editor 的命令行 `-server` 默认平滑帧率会把约 22 Hz 的等待时间计入 wall frame；CSV profiler 同时显示稳态 GameThread WorldTick、TickActors 与 replication 工作远低于 30 Hz 预算。最终容量进程使用 `-ini:Engine:[ConsoleVariables]:t.MaxFPS=120` 移除这段人工等待，以原定 30 Hz 的 33.34 ms 阈值衡量吞吐余量，没有修改正确性或安全 Gate。

最终日志：

- `Saved/Logs/M7G7_Server_R15_Capacity_20260827.log`
- `Saved/Logs/M7G7_Client1_R15_Capacity_20260827.log`
- `Saved/Logs/M7G7_Client2_R15_Capacity_20260827.log`

日志未发现 `Critical error`、Fatal、Assertion、RPC Rejected、NetworkFailure 或容量预算失败；测试结束后按准确 PID 停止全部进程，残留进程为 0。

## 6. 静态与可见说明 Gate

- `CombatTags.cpp` 中普通 `UE_DEFINE_GAMEPLAY_TAG` 数量为 0，`UE_DEFINE_GAMEPLAY_TAG_COMMENT` 数量为 150；M7 网络失败与事件 Tag 均带中文说明。
- M7 新增或实质修改的类、结构、枚举、函数、关键字段与时序分支已补齐中文源码注释。
- 蓝图可见的网络策略、RPC 结果、Unit View、Projectile identity、性能结果、资产报告、场景入口与委托均提供中文 `DisplayName`/`ToolTip`；输入参数使用中文 `UPARAM(DisplayName=...)`。
- `git diff --check` 通过。

## 7. G7 结论与人工验收建议

- Dedicated 2-client 下 Mixed/Minimal 复制矩阵、统一 View 与 owning RPC 已被真实进程验证。
- 越权、无效、超载、过频和重放请求均由统一安全子系统在执行 Order 前拒绝；客户端没有服务器结算字段的写入口。
- Projectile 预测仅限可丢弃视觉，服务器 Actor 仍是命中与 Finish 的唯一权威。
- Event schema、调试命令、资产校验和容量预算都已有稳定入口；GAP-018/019/021 已关闭。
- 建议人工在 `L_CombatTest` 的 Dedicated 2-client PIE 中重点查看：两个玩家 Unit 的 Mixed、AI 的 Minimal、双方 Unit/Modifier View 一致、Stop RPC 成功回执，以及 `combat.Debug.Metrics`/`combat.Debug.Unit` 输出。

M7 已满足 G7 并通过用户验收，可以提交本次修改；尚未开始 M8，需等待用户单独授权。
