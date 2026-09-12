import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path


TOOLS_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(TOOLS_ROOT))

from ue_environment import EnvFileError, check_environment, parse_env  # noqa: E402


class EnvParserTests(unittest.TestCase):
    def test_supports_quotes_comments_and_export(self):
        values = parse_env(
            """
            # comment
            export UE_INSTALLED_EDITOR='C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe' # trailing
            UE_SOURCE_EDITOR="D:/UE/UE/Engine/Binaries/Win64/UnrealEditor.exe"
            """
        )
        self.assertEqual(
            values["UE_INSTALLED_EDITOR"],
            "C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe",
        )
        self.assertEqual(
            values["UE_SOURCE_EDITOR"],
            "D:/UE/UE/Engine/Binaries/Win64/UnrealEditor.exe",
        )

    def test_rejects_unparseable_lines(self):
        with self.assertRaises(EnvFileError):
            parse_env("UE_SOURCE_EDITOR\n")
        with self.assertRaises(EnvFileError):
            parse_env("UE-SOURCE=/tmp/editor\n")


class EnvironmentCheckTests(unittest.TestCase):
    def make_editor_tree(self, root: Path, source: bool) -> Path:
        engine_root = root / ("source-engine" if source else "installed-engine")
        editor = engine_root / "Engine" / "Binaries" / "Win64" / "UnrealEditor.exe"
        editor.parent.mkdir(parents=True)
        editor.write_text("fake editor", encoding="utf-8")
        if source:
            build = engine_root / "Engine" / "Build" / "BatchFiles" / "Build.bat"
            build.parent.mkdir(parents=True)
            build.write_text("@echo off\n", encoding="utf-8")
        return editor

    def write_env(self, root: Path, installed: Path | None, source: Path | None):
        values = []
        if installed:
            values.append(f'UE_INSTALLED_EDITOR="{installed.as_posix()}"')
        if source:
            values.append(f'UE_SOURCE_EDITOR="{source.as_posix()}"')
        (root / ".env").write_text("\n".join(values) + "\n", encoding="utf-8")

    def test_missing_env_blocks_dedicated(self):
        with tempfile.TemporaryDirectory() as temp:
            report = check_environment(Path(temp))
            self.assertFalse(report.editor_ready)
            self.assertFalse(report.dedicated_ready)
            self.assertIn("未找到配置文件", report.diagnostics[0])

    def test_downloaded_editor_alone_does_not_enable_dedicated(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            installed = self.make_editor_tree(root, source=False)
            self.write_env(root, installed, None)
            report = check_environment(root)
            self.assertTrue(report.editor_ready)
            self.assertFalse(report.dedicated_ready)
            self.assertTrue(any("源码编译编辑器未配置" in item for item in report.diagnostics))

    def test_source_editor_requires_build_script(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            source = self.make_editor_tree(root, source=True)
            source_root = source.parents[3]
            (source_root / "Engine" / "Build" / "BatchFiles" / "Build.bat").unlink()
            self.write_env(root, None, source)
            report = check_environment(root)
            self.assertFalse(report.dedicated_ready)
            self.assertTrue(any("缺少 Build.bat" in item for item in report.diagnostics))

    def test_both_valid_editors_enable_dedicated(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            installed = self.make_editor_tree(root, source=False)
            source = self.make_editor_tree(root, source=True)
            self.write_env(root, installed, source)
            report = check_environment(root)
            self.assertTrue(report.editor_ready)
            self.assertTrue(report.dedicated_ready)
            self.assertEqual(report.source_root, source.parents[3].resolve())

    def test_cli_json_reports_blocked_requirement(self):
        with tempfile.TemporaryDirectory() as temp:
            result = subprocess.run(
                [
                    sys.executable,
                    str(TOOLS_ROOT / "ue_environment.py"),
                    "--repo-root",
                    temp,
                    "--require",
                    "dedicated",
                    "--json",
                ],
                capture_output=True,
                text=True,
                check=False,
            )
            self.assertEqual(result.returncode, 2)
            report = json.loads(result.stdout)
            self.assertFalse(report["dedicated_ready"])
            self.assertTrue(report["diagnostics"])

    def test_new_tooling_does_not_embed_historical_engine_paths(self):
        forbidden = (
            r"C:\Program Files\Epic Games\UE_5.8",
            r"D:\UE\UE",
        )
        for relative_path in ("Tools/ue_environment.py", "Tools/RunDedicated.ps1"):
            content = (TOOLS_ROOT.parent / relative_path).read_text(encoding="utf-8")
            for path in forbidden:
                self.assertNotIn(path, content)


if __name__ == "__main__":
    unittest.main()
