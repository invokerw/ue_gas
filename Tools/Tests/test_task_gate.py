"""验证任务 Gate 的正向、拒绝和只读行为。"""

import json
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import task_gate  # noqa: E402


SPEC_BODY = """# DOC-TEST 任务 Gate

> Spec 版本：`0.1`
> 状态：`{status}`
> 风险等级：`L1`

## 0. Intake 与 Gate 记录

- 用户请求：修复一个可验证的流程问题
- 附件解释：无附件
- 主 Skill：combat-feature-development
- 路由置信度：high
- F0 结论：`GO`
- F1 结论：`{f1}`
- F1 审查版本：`0.1`
- F1 审查人：测试审查者
- F1 计划审查证据：范围、AC、依赖、测试和回滚已逐项审查
- F2 结论：`{f2}`
- Push-Ready 结论：`{push}`
- 验证：{verification}
- 未执行：{unexecuted}

## 1. 目标与范围
验证任务 Gate。

## 2. 当前事实与依据
使用临时 Spec。

## 3. 行为与契约
行为契约。

## 4. 实施计划
| 文件 | 变更 |
| --- | --- |
| Tools/task_gate.py | 检查流程 |

## 5. 验收标准（AC）
- [ ] AC-01：有效输入通过。

## 6. Definition of Done
- [ ] 有测试和证据。

## 7. 测试矩阵与命令
| 层级 | 命令 | 结果 |
| --- | --- | --- |
| Unit | unittest | PASS |

## 8. 风险、回滚与升级
低风险，可回滚。

## 9. 交付证据
包含测试与文档路径。

## 10. 变更记录
初稿。

## 11. 路由、自评与 Reflect
已记录路由和反思。
"""


class TaskGateTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.spec = "Doc/CombatSystem/Specs/DOC-TEST.spec.md"
        self.write_spec(status="PLANNED", f1="APPROVED", f2="PASS", push="READY",
                        verification="待执行", unexecuted="N/A：流程任务不涉及 UE")

    def write_spec(self, **values):
        path = self.root / self.spec
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(SPEC_BODY.format(**values), encoding="utf-8")

    def test_preflight_accepts_complete_spec(self):
        result = task_gate.evaluate(self.root, self.spec, "preflight", changed=[])
        self.assertTrue(result["passed"], result["errors"])

    def test_preflight_rejects_missing_route_and_f0(self):
        path = self.root / self.spec
        path.write_text(path.read_text(encoding="utf-8").replace("- 主 Skill：combat-feature-development", "- 主 Skill：")
                        .replace("- 路由置信度：high", "- 路由置信度：待填写")
                        .replace("- F0 结论：`GO`", "- F0 依据：尚未决定"), encoding="utf-8")
        result = task_gate.evaluate(self.root, self.spec, "preflight", changed=[])
        self.assertFalse(result["passed"])
        self.assertTrue({"missing_skill_route", "missing_f0"}.issubset({e["code"] for e in result["errors"]}))

    def test_plan_accepts_clean_plan_before_behavior_changes(self):
        self.write_spec(status="PLAN_REVIEW", f1="REVISE", f2="FINDINGS", push="BLOCKED",
                        verification="计划检查待执行", unexecuted="N/A：尚未进入实现")
        result = task_gate.evaluate(self.root, self.spec, "plan", changed=[self.spec])
        self.assertTrue(result["passed"], result["errors"])

    def test_plan_preserves_preexisting_behavior_changes(self):
        self.write_spec(status="PLAN_REVIEW", f1="REVISE", f2="FINDINGS", push="BLOCKED",
                        verification="计划检查待执行", unexecuted="N/A：尚未进入实现")
        result = task_gate.evaluate(self.root, self.spec, "plan",
                                    changed=[self.spec, "Source/ue_gas/Combat/Example.cpp"])
        self.assertTrue(result["passed"], result["errors"])

    def test_plan_rejects_missing_implementation_plan(self):
        path = self.root / self.spec
        path.write_text(path.read_text(encoding="utf-8").replace("## 4. 实施计划", "## 4. 草稿"), encoding="utf-8")
        result = task_gate.evaluate(self.root, self.spec, "plan", changed=[])
        self.assertIn("missing_plan_section", {e["code"] for e in result["errors"]})

    def test_build_requires_f1_approval(self):
        self.write_spec(status="BUILDING", f1="REVISE", f2="FINDINGS", push="BLOCKED",
                        verification="计划检查通过", unexecuted="N/A：构建前")
        result = task_gate.evaluate(self.root, self.spec, "build", changed=[self.spec])
        self.assertFalse(result["passed"])
        self.assertIn("missing_f1_for_build", {e["code"] for e in result["errors"]})

    def test_build_accepts_approved_plan(self):
        self.write_spec(status="BUILDING", f1="APPROVED", f2="FINDINGS", push="BLOCKED",
                        verification="计划检查通过", unexecuted="N/A：构建前")
        result = task_gate.evaluate(self.root, self.spec, "build", changed=[self.spec])
        self.assertTrue(result["passed"], result["errors"])

    def test_build_and_delivery_reject_stale_plan_approval(self):
        path = self.root / self.spec
        for mode, status in (("build", "BUILDING"), ("delivery", "READY_FOR_REVIEW")):
            with self.subTest(mode=mode):
                self.write_spec(status=status, f1="APPROVED", f2="PASS", push="READY",
                                verification="unit PASS", unexecuted="N/A")
                path.write_text(path.read_text(encoding="utf-8").replace("> Spec 版本：`0.1`", "> Spec 版本：`0.2`"), encoding="utf-8")
                result = task_gate.evaluate(self.root, self.spec, mode, changed=[self.spec])
                self.assertIn("stale_plan_approval", {e["code"] for e in result["errors"]})

    def test_build_rejects_approval_without_review_evidence(self):
        self.write_spec(status="BUILDING", f1="APPROVED", f2="PASS", push="READY",
                        verification="unit PASS", unexecuted="N/A")
        path = self.root / self.spec
        path.write_text(path.read_text(encoding="utf-8").replace(
            "- F1 计划审查证据：范围、AC、依赖、测试和回滚已逐项审查", "- F1 计划审查证据："), encoding="utf-8")
        result = task_gate.evaluate(self.root, self.spec, "build", changed=[])
        self.assertIn("missing_plan_review", {e["code"] for e in result["errors"]})

    def test_current_revise_is_not_overridden_by_historical_approval(self):
        self.write_spec(status="BUILDING", f1="REVISE", f2="PASS", push="READY",
                        verification="unit PASS", unexecuted="N/A")
        path = self.root / self.spec
        with path.open("a", encoding="utf-8") as output:
            output.write("\n历史记录：F1 APPROVED，仅覆盖旧范围。\n")
        result = task_gate.evaluate(self.root, self.spec, "build", changed=[])
        self.assertIn("missing_f1_for_build", {e["code"] for e in result["errors"]})

    def test_build_rejects_f1_template_candidates(self):
        self.write_spec(status="BUILDING", f1="APPROVED / REVISE / ESCALATE", f2="PASS", push="READY",
                        verification="unit PASS", unexecuted="N/A")
        result = task_gate.evaluate(self.root, self.spec, "build", changed=[])
        self.assertIn("missing_f1_for_build", {e["code"] for e in result["errors"]})

    def test_plan_rejects_f0_template_candidates(self):
        path = self.root / self.spec
        path.write_text(path.read_text(encoding="utf-8").replace(
            "- F0 结论：`GO`", "- F0 结论：`GO / DEFER / ESCALATE`"), encoding="utf-8")
        result = task_gate.evaluate(self.root, self.spec, "plan", changed=[])
        self.assertIn("f0_not_go", {e["code"] for e in result["errors"]})

    def test_history_cannot_supply_missing_current_f1(self):
        self.write_spec(status="BUILDING", f1="APPROVED", f2="PASS", push="READY",
                        verification="unit PASS", unexecuted="N/A")
        path = self.root / self.spec
        path.write_text(path.read_text(encoding="utf-8").replace("- F1 结论：`APPROVED`", "")
                        + "\n历史记录：F1 APPROVED，仅覆盖旧范围。\n", encoding="utf-8")
        result = task_gate.evaluate(self.root, self.spec, "build", changed=[])
        self.assertIn("missing_f1_for_build", {e["code"] for e in result["errors"]})

    def test_conflicting_current_f1_is_rejected(self):
        self.write_spec(status="BUILDING", f1="APPROVED", f2="PASS", push="READY",
                        verification="unit PASS", unexecuted="N/A")
        path = self.root / self.spec
        with path.open("a", encoding="utf-8") as output:
            output.write("\n- F1 结论：`REVISE`\n")
        result = task_gate.evaluate(self.root, self.spec, "build", changed=[])
        self.assertIn("missing_f1_for_build", {e["code"] for e in result["errors"]})

    def test_deferred_f0_cannot_enter_plan_or_build(self):
        path = self.root / self.spec
        path.write_text(path.read_text(encoding="utf-8").replace("- F0 结论：`GO`", "- F0 结论：`DEFER`"), encoding="utf-8")
        for mode in ("plan", "build"):
            with self.subTest(mode=mode):
                result = task_gate.evaluate(self.root, self.spec, mode, changed=[])
                self.assertIn("f0_not_go", {e["code"] for e in result["errors"]})

    def test_delivery_requires_tests_and_documentation_for_behavior_changes(self):
        result = task_gate.evaluate(self.root, self.spec, "delivery", kind="process",
                                    changed=[self.spec, "Tools/task_gate.py"])
        self.assertFalse(result["passed"])
        codes = {e["code"] for e in result["errors"]}
        self.assertIn("missing_tests", codes)

    def test_delivery_rejects_unfinished_gate_conclusions(self):
        self.write_spec(status="READY_FOR_REVIEW", f1="REVISE", f2="FINDINGS", push="BLOCKED",
                        verification="unit 未执行", unexecuted="环境阻塞：待补跑")
        result = task_gate.evaluate(self.root, self.spec, "delivery", kind="docs",
                                    changed=[self.spec, "README.md"])
        self.assertFalse(result["passed"])
        codes = {e["code"] for e in result["errors"]}
        self.assertTrue({"missing_f1", "missing_f2", "missing_push_ready"}.issubset(codes))

    def test_delivery_accepts_behavior_with_test_and_doc(self):
        self.write_spec(status="READY_FOR_REVIEW", f1="APPROVED", f2="PASS", push="READY",
                        verification="unit PASS", unexecuted="N/A：不适用")
        changed = [self.spec, "Tools/task_gate.py", "Tools/Tests/test_task_gate.py", "README.md"]
        result = task_gate.evaluate(self.root, self.spec, "delivery", kind="process", changed=changed)
        self.assertTrue(result["passed"], result["errors"])
        self.assertEqual(result["test_files"], ["Tools/Tests/test_task_gate.py"])

    def test_docs_kind_accepts_markdown_only_without_ue_build(self):
        self.write_spec(status="READY_FOR_REVIEW", f1="APPROVED", f2="PASS", push="READY",
                        verification="validate_docs PASS", unexecuted="UE 构建未执行：纯文档任务")
        changed = [self.spec, "Doc/CombatSystem/README.md", "README.md"]
        result = task_gate.evaluate(self.root, self.spec, "delivery", kind="docs", changed=changed)
        self.assertTrue(result["passed"], result["errors"])
        self.assertEqual(result["behavior_files"], [])

    def test_delivery_rejects_spec_not_in_diff(self):
        result = task_gate.evaluate(self.root, self.spec, "delivery", kind="process",
                                    changed=["Tools/task_gate.py", "Tools/Tests/test_task_gate.py", "README.md"])
        self.assertFalse(result["passed"])
        self.assertIn("spec_not_changed", {e["code"] for e in result["errors"]})

    def test_evaluate_does_not_modify_spec(self):
        path = self.root / self.spec
        before = path.read_bytes()
        result = task_gate.evaluate(self.root, self.spec, "preflight", changed=[])
        self.assertTrue(result["passed"])
        self.assertEqual(path.read_bytes(), before)


if __name__ == "__main__":
    unittest.main()
