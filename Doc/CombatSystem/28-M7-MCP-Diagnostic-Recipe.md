# 28 M7 UE MCP 联机诊断配方

## 1. 用途

本配方用于复现“客户端 UI 不一致、Order RPC 被拒绝、Projectile 出现双视觉、事件因果链断裂”四类 M7 问题。UE MCP 负责读取 Editor/PIE 的真实对象和日志；最终结论仍以 Automation、资产校验命令和 Dedicated Server + 2 Client 为准。

## 2. 前置条件

1. 打开本仓库的 `ue_gas.uproject`，确认 Editor 没有待保存的未知用户资产。
2. 发现当前会话真实暴露的 UE MCP 工具和参数 schema；禁止凭历史名称直接调用。
3. 打开 `/Game/Combat/Tests/L_CombatTest`，确认 World 中只有一个 `CombatTestScenarioActor`。
4. Network PIE 使用 Dedicated Server、2 Clients；运行前清空 Output Log。
5. 若 endpoint 或目标工具不可用，跳到第 7 节命令行替代流程，不执行资产写入。

## 3. 只读诊断顺序

### 3.1 建立 World 身份

- 列出 Editor World、PIE Server World 和两个 PIE Client World。
- 对每个 World 记录 `WorldName / NetMode / PIEInstance / LocalPlayerCount`。
- 后续每条对象读数必须标明来自哪个 World，禁止把 Editor CDO 或 Client 副本当成服务器权威对象。

### 3.2 检查复制矩阵

- 在 Server World 查询全部 `ACombatUnitCharacter`。
- 记录 Actor 名、Owner、Team、LifeGeneration、`EffectiveAscReplicationPolicy` 和 `CombatUnitViewComponent`。
- 期望至少两个玩家 Owner Unit 为 `Mixed`，至少一个无 PlayerController Owner 的 AI Unit 为 `Minimal`。
- 在两个 Client World 回读相同 Actor 的 `FCombatUnitView` 和 `FCombatModifierViewArray`；Owner 与非 Owner 的扁平字段应一致，不读取 `UCombatModifierRuntime`。

### 3.3 检查安全 RPC

- 正常客户端只向自己拥有的 Unit 提交一个正数、未使用的 RequestId 和一个合法 Stop Order；验收 smoke 使用 Stop 以排除 NavMesh 与目标位置对 RPC ownership 证据的干扰。
- Server 日志应出现接受结果；Owning Client 应出现 `M7OrderBatchResult ... Accepted=true`。
- Automation 再覆盖非 Owner、重复 ID、超载荷和 token bucket；不要用 PIE 手工洪泛代替确定性用例。
- 拒绝时保存 `Failure.Network.*`、RequestId、Controller ActorId、Unit ActorId 和对应 RootEventId。

### 3.4 展开事件与单位状态

- 在 Server 控制台执行 `combat.Debug.Metrics`。
- 使用 `combat.Debug.Unit <ActorUniqueId|Name>` 保存目标单位单行快照。
- 使用 `combat.Debug.Event <RootSequence>` 展开因果链；确认全部记录 `Schema=1` 且 Depth 连续。
- 必要时执行 `combat.Debug.Draw 1`，检查 Order 目标线、Motion 方向和 Projectile 唯一视觉；采样后恢复为 `0`。

### 3.5 Projectile 双视觉诊断

- 在 Client World 记录预测键、服务器 Projectile Handle 和 Actor 数量。
- 同一 PredictionKey 到达服务器 Actor 后，预测视觉必须销毁；同一 Handle 重复通知只能增加去重计数，不能增加视觉 Actor。
- Damage、Impact 和 Finish 只在 Server World 的 `UCombatProjectileSubsystem` 中判定；Client 视觉不得作为命中证据。

## 4. 性能采样

1. 固定地图、单位数量、客户端数量和运行时长，禁止边采样边编辑资产。
2. 先保存 `combat.Debug.Metrics` 的起始快照。
3. 运行至少 60 秒；采集 Server frame p95/p99、每连接战斗相关发送带宽、Scheduler deferral/overdue 和各 registry 数量。Installed Editor 的命令行 `-server` 默认可能受平滑帧率等待影响，容量验收进程可添加 `-ini:Engine:[ConsoleVariables]:t.MaxFPS=120` 移除人工等待；比较阈值仍使用 30 Hz 的 33.34 ms，不把 120 Hz 当作新的产品帧率承诺。
4. 停止 PIE 前保存结束快照；停止后确认新 World 不继承旧 Schedule、Modifier、Projectile、Thinker 或 Aura。
5. 与 [27 §7](27-M7-Network-Observability-Decision.md#7-容量预算关闭-gap-018-的目标值) 比较；不得通过放宽正确性或安全规则换取通过。

## 5. 资产与迁移检查

执行项目级校验命令：

```powershell
UnrealEditor-Cmd.exe ue_gas.uproject -run=CombatAssetValidation -Unattended -NoP4 -Report=Saved/CombatValidation/CombatAssetReport.json
```

报告必须满足 `errorCount=0`。修改 DefinitionName 前先添加显式 `FCombatDefinitionRedirect`，其 `IntroducedInVersion` 不得大于当前内容版本；禁止重定向链、环和缺失目标。

## 6. 记录模板

```text
时间/引擎版本:
地图与 PIE 配置:
Server/Client World 身份:
Unit ActorId/Owner/复制策略:
UnitView/ModifierView 差异:
RequestId/FailureTag:
RootEventId 与事件展开:
Projectile PredictionKey/Handle/视觉数:
起止 Metrics:
Server frame p95/p99:
每连接 KiB/s:
资产报告路径与错误数:
结论/最小复现:
```

## 7. MCP 不可用时的等价入口

- Editor/PIE 状态：用 Output Log 的 `M7ScenarioReady`、`M7ClientRpcSmoke`、`M7OrderBatchResult`。
- Unit/Event/Metrics：使用 `combat.Debug.Unit`、`combat.Debug.Event`、`combat.Debug.Metrics` 控制台命令。
- 语义回归：`Automation RunTests Combat.`。
- 数据资产：运行 `CombatAssetValidation` commandlet 并检查 JSON。
- 最终网络证据：独立 Dedicated Server + 2 Client 日志；MCP/PIE 结果不替代它。
