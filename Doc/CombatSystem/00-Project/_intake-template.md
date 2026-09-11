# Intake 模板

> 用于进入 F0 前整理需求。完成后由负责人给出 `GO`、`DEFER` 或 `ESCALATE`，不要直接把聊天记录当作交付规格。

## 1. 请求

- 请求标题：
- 提出人/日期：
- 用户原始请求（保持原意）：
- 附件解释：`无附件 / 需求 / 参考 / 工程约束`；说明附件是否改变范围或验收。
- 触发背景：
- 用户或项目价值：
- 期望完成时间（如有）：

## 2. 边界

- 目标：
- 明确范围：
- Non-Goals：
- 是否涉及 `combat_v1_rc1`、GameplayTag、DefinitionId、事件 schema、网络载荷或公开蓝图 API：

## 3. 当前事实

- 进度台账位置和状态：
- 相关 DDD 文档：
- 代码/资产事实（`file:line` 或资产路径）：
- 现有测试、日志或 Golden Case：
- 依赖和 blast radius：

## 4. 风险与权限

- 风险等级：`L0 / L1 / L2`
- 服务器权威或客户端表现影响：
- 生命周期、旧回调、迁移或回滚风险：
- 需要人决定的问题：

## 5. F0 结论

- 结论：`GO / DEFER / ESCALATE`
- 依据：
- 缺失信息：
- 下一步最小动作：
- 建议主 Skill：
- 备选 Skill 与排除理由：
- 路由置信度：`high / medium / low`
- 决策人和日期：

F0 Spec 建立后运行 `python3 -B Tools/task_gate.py --mode preflight --spec Doc/CombatSystem/Specs/<task-id>.spec.md --kind <feature|docs|process>`；失败时先补齐输入，不进入实现。
