# Gate 检查表

> 本表使用 F0/F1/F2 表示单项交付流程门；项目原有 G0–G8 里程碑 Gate 仍按路线图执行。
> 可将结论和证据直接写入任务 Spec。每项填写通过、不适用（附原因）或未执行；必需检查受阻时填写未执行并说明缺口。

## F0 Framing

- [ ] 目标、用户价值、范围和 Non-Goals 已写清。
- [ ] 已读取 `00-01-Progress-Tracker.md` 和相关 DDD。
- [ ] 已判断是否触及服务器权威、公开契约、生命周期、网络或资产迁移。
- [ ] 已给出 `GO`、`DEFER` 或 `ESCALATE`，并记录依据。

结论：`GO / DEFER / ESCALATE`
证据：

## F1 Plan

- [ ] Spec 已有版本、文件定位、依赖、AC、DoD、测试和回滚。
- [ ] 已覆盖正常、取消、过期、死亡、EndPlay、重复请求和旧 generation 路径（适用时）。
- [ ] 已确认使用现有公共入口，或记录新增内核扩展的必要性。
- [ ] 已确定 Editor/Automation/PIE/Network/Dedicated/Soak 验证层级。
- [ ] 已检查迁移、兼容、版本和 `00-04-Decisions-Gaps.md` 影响。

结论：`APPROVED / REVISE / ESCALATE`
证据：

## F2 Adversarial

- [ ] 审查者使用独立上下文，只读取 Spec、diff、仓库事实和测试证据。
- [ ] 已检查契约、状态机、并发、生命周期、网络安全、性能、可观测性和集成。
- [ ] 所有发现都有严重度、修复文件位置和复验结果。
- [ ] 修复/复验不超过三轮；未收敛项已形成 Gap Report 并升级。

结论：`PASS / REVISE / ESCALATE`
证据：

## Push-Ready 六层

- [ ] L1 Tests：直接测试、失败路径和 Golden Case。
- [ ] L2 Types/Build：适用的 Editor、Server/Client Target 构建。
- [ ] L3 No Regression：回归、资产校验、蓝图编译和必要的 Dedicated/Soak。
- [ ] L4 Adversarial：F2 findings 已关闭或升级。
- [ ] L5 DDD/Constraints：架构、注释、ToolTip、旁路扫描与文档一致。
- [ ] L6 Decisions：ADR/Gap、版本、迁移、延期和进度台账齐全。
- [ ] 文档类变更已运行 `python3 -B Tools/validate_docs.py` 和 `git diff --check`，并核对过时状态与事实。
- [ ] 修改文档校验脚本时，已运行 `python3 -B -m unittest discover -s Tools/Tests -p 'test_validate_docs.py' -v`。

结论：`READY / BLOCKED / ESCALATED`
证据：
