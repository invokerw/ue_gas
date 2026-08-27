# 33 M8 验收记录：候选发布

## 1. 验收状态

- 里程碑：M8
- 候选发布：`combat_v1_rc1`
- Gate：G8 技术 Gate 通过
- Task：5/5 已完成
- 用户验收：已验收
- 提交验收日期：2026-08-27
- 最终验收日期：2026-08-27

## 2. 完成范围

| Task | 实现结果 |
| --- | --- |
| REL-001 | Editor/Server/Client 构建、最终修订 3 轮完整 Automation、资产校验、独立 Dedicated + 2 Client 容量场景、teardown 与静态 Gate 全部通过 |
| REL-002 | [31 生命周期审计](31-M8-Lifecycle-Audit.md)登记 Handle、Delegate、Schedule、Runtime、Actor 的 owner、退出与旧回调防护；真实 World 测试验证全部清零和 exactly-once |
| REL-003 | ADR-042 根据 M7 profiler 和本轮容量样本决定不引入无具体 owner 的 pooling/relevancy/批处理，保留稳定顺序与 exactly-once |
| REL-004 | ADR-041 冻结 v1 服务器权威与 Projectile 纯视觉 PredictionKey；完整 gameplay rollback、Cue reconcile 和 replay 明确延期 post-v1 |
| REL-005 | 发布契约、生命周期清单、[公共扩展与迁移指南](32-M8-Public-Extension-Guide.md)、事件序列和 DataAsset/蓝图扩展面自动化完成 |

完整发布边界见 [30 M8 候选发布决策](30-M8-Release-Candidate-Decision.md)。GAP-015/017/022 均已显式延期并进入机器可读发布契约，不再存在未知 v1 功能边界。

## 3. 代码与构建

新增 `FCombatReleaseContract` 与 `UCombatReleaseContractLibrary`，统一暴露 Contract/Content/GameplayTag/Formula/RNG/Event 版本，以及服务器权威、视觉预测和延期能力。蓝图字段与函数均有中文 `DisplayName`/`ToolTip`，参数使用中文 `UPARAM`。

| Engine | Target | Configuration | 结果 |
| --- | --- | --- | --- |
| Installed UE 5.8.1 | `ue_gasEditor` | Win64 Development | Succeeded；最终修订重新编译通过 |
| Source UE 5.8.0（`D:\UE\UE`） | `ue_gasServer` | Win64 Development | Succeeded |
| Source UE 5.8.0（`D:\UE\UE`） | `ue_gasClient` | Win64 Development | Succeeded |

源码 Server 初次编译发现测试在非 Editor 目标调用反射元数据 API；已将中文元数据断言限定为 `WITH_EDITOR`，Server/Client 仍验证公开函数符号，三个目标最终全部通过。

## 4. 自动化与无 flaky 证据

Installed UE 5.8.1 对最终 Tag schema 修订冷启动完整执行 `Combat.*` 三轮：

| 轮次 | Success | Fail | 进程退出码 |
| --- | ---: | ---: | ---: |
| R2 | 40 | 0 | 0 |
| R3 | 40 | 0 | 0 |
| Final | 40 | 0 | 0 |

M8 新增 3 项：

| Test | 覆盖 |
| --- | --- |
| `Combat.Release.M8.ContractAndDeferredBoundaries` | 版本常量一致、服务器权威与 post-v1 延期边界、schema 漂移负例 |
| `Combat.Release.M8.LifecycleOwnershipAndTeardown` | Schedule、Modifier、Thinker、Aura、Transaction 同 World 创建/退出、registry 清零、过期句柄和 exactly-once |
| `Combat.Release.M8.PublicExtensionSurface` | 公共 DataAsset 继承、Ability/Modifier 蓝图事件、发布契约函数与中文 Editor 元数据 |

最终日志为 `Saved/Logs/M8_Automation_Final_20260827.log`；重复轮次为 `M8_Automation_Full_R2/R3_20260827.log`。最终同一修订共完成 120 次测试、0 失败，没有发现 flaky。

## 5. 资产与 Dedicated 候选场景

资产 commandlet：

```text
CombatAssetValidation Version=1 Assets=2 Errors=0 Warnings=0
```

报告：`Saved/CombatValidation/CombatAssetReport.json`；日志：`Saved/Logs/M8_AssetValidation_20260827.log`。

独立进程运行 `/Game/Combat/Tests/L_CombatTest`，Server 使用 `-CombatM7CapacitySmoke`，两个 Client 使用 `-CombatM7ClientSmoke`：

```text
M7CapacityFixtureReady Units=64 Modifiers=256
M7ScenarioReady Players=2 Units=64 Mixed=2 Minimal=62 UnitViews=64 CapacityFixture=Ready CapacityBudget=Pass
M8ReleaseContract Valid=Pass Contract=1 Release=combat_v1_rc1 Content=1 Tags=1 Formula=1 RNG=1 Event=1 Authority=Server ProjectilePrediction=VisualOnly GameplayRollback=DeferredPostV1 Replay=DeferredPostV1 Summons=DeferredPostV1 Economy=DeferredPostV1
Client 1: M7OrderBatchResult Accepted=true Orders=1 Failure=None
Client 2: M7OrderBatchResult Accepted=true Orders=1 Failure=None
```

最终候选场景达到 Ready 时的 234 帧滚动快照：

| 指标 | 实测 | 预算 | 结果 |
| --- | ---: | ---: | --- |
| Server frame P95 | 9.132 ms | <= 33.34 ms | 通过 |
| Server frame P99 | 9.354 ms | <= 50 ms | 通过 |
| 单连接最大发送 | 5.760 KiB/s | <= 256 KiB/s | 通过 |
| Unit / Modifier | 64 / 256 | 64 / 256 | 通过 |

该快照用于确认最终发布契约与场景没有改变容量行为；REL-003 的长期性能依据仍是 M7 超过 90 秒、4096 帧滚动窗口的 16.281/22.392 ms 与 39.817 KiB/s 基线。M8 日志未出现 Critical error、Assertion failed、NetworkFailure、RPC rejection 或预算失败。验证后按准确 PID 停止 Server 和两个 Client，主进程残留为 0。

最终场景日志：

- `Saved/Logs/M8G8_Server_Candidate_20260827.log`
- `Saved/Logs/M8G8_Client1_Candidate_20260827.log`
- `Saved/Logs/M8G8_Client2_Candidate_20260827.log`

## 6. 静态 Gate

- `CombatTags.cpp` 普通 `UE_DEFINE_GAMEPLAY_TAG` 为 0，`UE_DEFINE_GAMEPLAY_TAG_COMMENT` 为 150；M8 未新增 Tag。
- 新增类、结构、函数、关键字段和测试分支均有中文源码注释。
- M8 蓝图可见发布字段、函数和参数全部有中文可见说明；Editor 自动化通过反射保护核心公共扩展面。
- 新代码未增加 Health、Transform、Timer gameplay 或客户端结算旁路；测试场景 Timer 只编排 smoke，不参与唯一 gameplay 结算。
- `git diff --check` 通过；验证结束后无 Unreal Editor/Server/Client 主进程残留。

## 7. G8 结论与人工验收建议

G8 的四项发布条件已满足：P0/P1 Gap 均关闭或版本化延期；无权威旁路、重复结算或 teardown 泄漏；Content/Formula/RNG/Event 有版本和迁移规则；新技能可只依赖公开 DataAsset、Ability/Modifier 基类与蓝图事件。

建议在 `L_CombatTest` 做一次最终人工查看：

1. 以 Dedicated Server、2 Players 运行，确认两名玩家单位可正常控制，AI 单位继续存在。
2. 查看 Output Log 中 `M8ReleaseContract Valid=Pass`，并确认完整延期字段符合当前产品边界。
3. 调用“获取战斗发布契约”蓝图节点，检查中文字段说明和冻结版本。
4. 重建/销毁测试场景一次，确认 Unit、Aura、Projectile 和容量对象无残留或重复事件。

M8 已通过用户验收，可以提交 `combat_v1_rc1` 的候选发布修改。
