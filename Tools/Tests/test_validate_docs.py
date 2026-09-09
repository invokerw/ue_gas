"""用独立临时仓库验证文档门的正向行为和拒绝路径。"""

import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import validate_docs  # noqa: E402


SPEC = """# DOC-TEST 示例

> Spec 版本：`1.0`
> 状态：`BUILDING`
> 风险等级：`L1`

## 1. 目标与范围
校验文档。
## 2. 当前事实与依据
使用临时文件。
## 3. 验收标准（AC）
- [ ] 有效输入通过，断链失败。
## 4. 测试矩阵与命令
运行 unittest。
## 5. 交付证据
待执行，不声明通过。
"""


class DocsValidationTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        for path in validate_docs.REQUIRED_DOCUMENTS:
            self.write(path, "# 文档\n")
        for path in validate_docs.REQUIRED_DIRECTORIES:
            (self.root / path).mkdir(parents=True, exist_ok=True)
        self.write("Doc/CombatSystem/Specs/_template.spec.md", SPEC)
        self.write("Doc/CombatSystem/Specs/DOC-TEST.spec.md", SPEC)

    def write(self, name, text):
        path = self.root / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text, encoding="utf-8")
        return path

    def errors(self):
        return validate_docs.validate(self.root)["errors"]

    def codes(self):
        return {error["code"] for error in self.errors()}

    def test_valid_repository(self):
        self.assertEqual(self.errors(), [])

    def test_missing_entry_is_a_diagnostic(self):
        (self.root / "agent.md").unlink()
        self.assertIn("missing_entry", self.codes())

    def test_broken_link_reports_location(self):
        self.write("README.md", "# Root\n\n[断链](missing.md)\n")
        error = next(e for e in self.errors() if e["code"] == "broken_link")
        self.assertEqual((error["file"], error["line"]), ("README.md", 3))

    def test_code_examples_are_not_links(self):
        self.write("README.md", "# Root\n`[例](missing.md)`\n```md\n[例](missing.md)\n```\n~~~md\n[例](missing.md)\n~~~\n")
        self.assertEqual(self.errors(), [])

    def test_parentheses_spaces_images_and_titles(self):
        self.write("Doc/示例 (v1).md", "# Example\n")
        self.write("Doc/example(v2).md", "# Example\n")
        self.write("README.md", '[空间](<Doc/示例 (v1).md> "标题")\n![编码](Doc/示例%20%28v1%29.md#example)\n[括号](Doc/example(v2).md)\n')
        self.assertEqual(self.errors(), [])

    def test_reference_links_and_undefined_reference(self):
        self.write("README.md", '[正文][Guide]\n[Guide][]\n[Guide]\n\n[guide]: Doc/CombatSystem/README.md "索引"\n')
        self.assertEqual(self.errors(), [])
        self.write("README.md", "[正文][missing-reference]\n")
        self.assertIn("undefined_reference", self.codes())

    def test_external_schemes_are_not_local_files(self):
        self.write("README.md", "[web](https://example.com/a_(b))\n[email](mailto:test@example.com)\n[app](codex://review)\n[anchor](#title)\n")
        self.assertEqual(self.errors(), [])

    def test_legacy_file_and_link_are_rejected(self):
        self.write("Doc/CombatSystem/00-Old.md", "# Old\n")
        self.write("README.md", "[旧入口](Doc/CombatSystem/00-Old.md)\n")
        self.assertTrue({"legacy_file", "legacy_link"}.issubset(self.codes()))

    def test_tracker_can_advance_without_frozen_markers(self):
        self.write("Doc/CombatSystem/00-Project/00-01-Progress-Tracker.md", "# 台账\n\nSAM：已验收。测试数量由最新报告确定。\n")
        self.assertEqual(self.errors(), [])

    def test_directory_document_numbers_accept_ordered_files_and_templates(self):
        self.write("Doc/CombatSystem/10-Architecture/10-01-Scope.md", "# 10-01 Scope\n")
        self.write("Doc/CombatSystem/10-Architecture/10-02-Runtime.md", "# 10-02 Runtime\n")
        self.write("Doc/CombatSystem/10-Architecture/10-03-补充说明.md", "# 10-03 补充说明\n")
        self.write("Doc/CombatSystem/10-Architecture/README.md", "# Index\n")
        self.write("Doc/CombatSystem/10-Architecture/_template.md", "# Template\n")
        self.assertEqual(self.errors(), [])

    def test_old_mismatched_and_zero_document_numbers_are_rejected(self):
        for filename in ("01-Old.md", "20-01-WrongDirectory.md", "10-00-Zero.md", "10-MissingSequence.md", "10-04-.md"):
            with self.subTest(filename=filename):
                path = self.write(f"Doc/CombatSystem/10-Architecture/{filename}", "# Invalid\n")
                self.assertIn("invalid_doc_number", self.codes())
                path.unlink()

    def test_duplicate_document_number_is_rejected(self):
        self.write("Doc/CombatSystem/10-Architecture/10-01-First.md", "# First\n")
        self.write("Doc/CombatSystem/10-Architecture/10-01-Second.md", "# Second\n")
        self.assertIn("duplicate_doc_number", self.codes())

    def test_invalid_spec_is_rejected(self):
        self.write("Doc/CombatSystem/Specs/DOC-TEST.spec.md", SPEC.replace("> 风险等级：`L1`", "").replace("## 3. 验收标准（AC）", "## 3. 随笔"))
        self.assertIn("invalid_spec", self.codes())

    def test_encoding_and_whitespace_fail_cleanly(self):
        self.write("README.md", "# Root \n")
        (self.root / "agent.md").write_bytes(b"\xff")
        self.assertTrue({"read_error", "trailing_whitespace"}.issubset(self.codes()))

    def test_repository_escape_is_rejected(self):
        self.write("README.md", "[越界](../outside.md)\n")
        self.assertIn("outside_root", self.codes())

    def test_cli_failure_and_report_leave_inputs_unchanged(self):
        path = self.write("README.md", "[断链](missing.md)\n")
        original = path.read_bytes()
        report = self.root / "Saved/report.json"
        result = subprocess.run([sys.executable, "-B", str(Path(validate_docs.__file__)),
                                 "--root", str(self.root), "--report", str(report)],
                                capture_output=True, text=True)
        self.assertEqual(result.returncode, 1, result.stderr)
        data = json.loads(report.read_text())
        self.assertEqual(data["schema_version"], 1)
        self.assertFalse(data["passed"])
        self.assertEqual(path.read_bytes(), original)
        self.assertNotIn("Traceback", result.stdout + result.stderr)


if __name__ == "__main__":
    unittest.main()
