# M1 环境决策：Dedicated Server/Client Target

> 日期：2026-08-25
> 关联：TST-003、GAP-020、G1
> 结论：组合证据满足 G1；TST-003 完成，GAP-020 关闭

## 1. 环境边界

本机同时存在两个 UE 5.8 环境：

- Launcher Installed Engine：UE 5.8.1，CL 56057345，路径 `C:\Program Files\Epic Games\UE_5.8`。
- Source Engine：UE 5.8.0，路径 `D:\UE\UE`。

Installed Engine 能构建并运行 `ue_gasEditor`，但 UBT 明确拒绝 Server/Client Target：

```text
Server targets are not currently supported from this engine distribution.
Client targets are not currently supported from this engine distribution.
```

这属于 Engine distribution 限制，不是 Combat 模块编译错误。源码引擎解除该限制，但两个引擎存在 5.8.0/5.8.1 版本偏差，因此构建能力与运行连接分别取证。

## 2. Target 构建证据

以下目标均由 `D:\UE\UE\Engine\Build\BatchFiles\Build.bat` 完成 Development 构建：

```text
ue_gasServer Win64 Development -> Binaries\Win64\ue_gasServer.exe -> Succeeded
ue_gasClient Win64 Development -> Binaries\Win64\ue_gasClient.exe -> Succeeded
```

首次构建覆盖完整依赖；最终代码修改后的增量构建分别为 3/3 actions，均返回 `Result: Succeeded`。因此 UBT 可发现目标，项目模块也能分别在 Server 与 Client 编译环境下通过。

## 3. 独立连接 smoke

连接 smoke 使用 Installed Engine 5.8.1 启动两个相互独立的 OS 进程，不使用 PIE：

- Server：`UnrealEditor.exe <uproject> /Game/Combat/Tests/L_CombatTest?listen -server -port=17777 ...`
- Client：`UnrealEditor.exe <uproject> 127.0.0.1:17777 -game ...`

验收日志：

- `Saved/Logs/CombatM1AcceptanceServer.log`
- `Saved/Logs/CombatM1AcceptanceClient.log`

关键证据：

```text
IpNetDriver listening on port 17777
M1ScenarioReady Units=2 Team1=1 Team2=1 ASCActorInfo=Ready State.Alive=Present
Join succeeded
Welcomed by server (Level: /Game/Combat/Tests/L_CombatTest, ...)
Bringing World /Game/Combat/Tests/L_CombatTest.L_CombatTest up for play
```

Server/Client 均无 Fatal、Assertion、NetworkFailure、`LogCombat: Error`、`LogGameplayTags: Error` 或 `LogAssetManager: Error`。Installed Engine 的 Experimental Toolsets 在 `-server`/`-game` 模式会输出缺少 `AgentSkill`/`ToolsetDefinition` 的 Python 启动错误；它来自引擎实验插件，不影响项目模块、GameplayTag、AssetManager 或网络握手，本次不计为 Combat Gate 失败。

## 4. 自动化与场景持久化

- `/Game/Combat/Tests/L_CombatTest` 已持久化一个非 spatially loaded 的 `CombatTestScenarioActor`；关卡二次冷加载确认 `Existing=1`。
- 场景在 Authority 侧生成 Team 1/2 各一名 Unit，并通过结构化日志检查 ASC ActorInfo 与 `State.Alive`。
- 冷启动 `Combat.Foundation` 自动化最终结果为 7/7 Success、0 Failed：`Saved/Logs/CombatM1AcceptanceAutomation.log`。

## 5. 版本偏差与后续复现

源码 5.8.0 Target 可执行文件不能直接消费由 Installed 5.8.1 Editor 保存的未 Cook 内容；直接启动会在 premade asset registry/BufferReader 阶段失败。构建匹配的源码 Editor 并重新 Cook 需要额外完整 Editor toolchain 构建，不属于项目源码缺陷。

因此本轮采用两条独立且互补的证据链：

1. 源码 UE 5.8.0 证明 Development Server/Client Target 与项目模块可构建。
2. Installed UE 5.8.1 的独立 Server/Game 进程证明真实 socket 连接、地图加载与 M1 场景状态。

CI 固化时应统一 Engine patch 版本，并由同一源码 Engine 完成 Build、Cook、Run，届时可直接以 `ue_gasServer.exe`/`ue_gasClient.exe` 重放 smoke，消除该环境偏差。

## 6. G1 结论

- TST-003：已完成。
- GAP-020：已关闭。
- G1：通过，M1 提交用户验收。
- M2：在用户明确验收并另行授权前不开始。
