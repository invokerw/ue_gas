#!/usr/bin/env python3
"""读取本地 UE 配置并判断各类验证入口是否具备运行条件。"""

from __future__ import annotations

import argparse
import json
import re
import sys
from dataclasses import dataclass, field
from pathlib import Path
from typing import Mapping


ENV_FILE_NAME = ".env"
INSTALLED_EDITOR_KEY = "UE_INSTALLED_EDITOR"
SOURCE_EDITOR_KEY = "UE_SOURCE_EDITOR"
EDITOR_NAMES = {"unrealeditor.exe", "unrealeditor-cmd.exe", "unrealeditor"}
_ENV_KEY = re.compile(r"^[A-Za-z_][A-Za-z0-9_]*$")


class EnvFileError(ValueError):
    """表示 `.env` 中存在无法安全解释的行。"""


def _parse_value(raw_value: str, line_number: int) -> str:
    """解析 dotenv 值，保留 Windows 路径中的反斜杠和空格。"""
    value = raw_value.strip()
    if not value:
        return ""

    if value[0] in {"'", '"'}:
        quote = value[0]
        closing = value.find(quote, 1)
        if closing < 0:
            raise EnvFileError(f"第 {line_number} 行的值缺少结束引号")
        trailing = value[closing + 1 :].strip()
        if trailing and not trailing.startswith("#"):
            raise EnvFileError(f"第 {line_number} 行的引号后存在无法解析的内容")
        return value[1:closing]

    # 只把空白后的井号当作注释，避免破坏路径或其他合法值。
    return re.split(r"\s+#", value, maxsplit=1)[0].rstrip()


def parse_env(text: str) -> dict[str, str]:
    """解析简单 dotenv 文件；重复键采用最后一次定义。"""
    values: dict[str, str] = {}
    for line_number, line in enumerate(text.splitlines(), start=1):
        stripped = line.strip()
        if not stripped or stripped.startswith("#"):
            continue
        if stripped.startswith("export "):
            stripped = stripped[7:].lstrip()
        if "=" not in stripped:
            raise EnvFileError(f"第 {line_number} 行缺少 '='")
        key, raw_value = stripped.split("=", 1)
        key = key.strip()
        if not _ENV_KEY.fullmatch(key):
            raise EnvFileError(f"第 {line_number} 行的变量名无效：{key!r}")
        values[key] = _parse_value(raw_value, line_number)
    return values


def load_env_file(env_file: Path) -> dict[str, str]:
    """读取并解析指定 `.env` 文件。"""
    try:
        return parse_env(env_file.read_text(encoding="utf-8"))
    except FileNotFoundError:
        raise EnvFileError(f"未找到配置文件：{env_file}") from None
    except OSError as exc:
        raise EnvFileError(f"无法读取配置文件 {env_file}：{exc}") from exc
    except UnicodeError as exc:
        raise EnvFileError(f"配置文件不是 UTF-8：{env_file}：{exc}") from exc


def _absolute_path(raw_value: str) -> Path | None:
    """将配置值转换为绝对路径；相对路径不参与自动补全。"""
    if not raw_value.strip():
        return None
    path = Path(raw_value.strip()).expanduser()
    if not path.is_absolute():
        return None
    return path.resolve(strict=False)


def _engine_root(editor_path: Path) -> Path | None:
    """从 `.../Engine/Binaries/<Platform>/UnrealEditor*` 找到引擎根目录。"""
    for parent in (editor_path.parent, *editor_path.parents):
        if parent.name.casefold() == "engine":
            return parent.parent
    return None


def _validate_editor(
    values: Mapping[str, str], key: str, label: str, diagnostics: list[str]
) -> Path | None:
    raw_value = values.get(key, "").strip()
    if not raw_value:
        diagnostics.append(f"{label}未配置：{key}")
        return None
    path = _absolute_path(raw_value)
    if path is None:
        diagnostics.append(f"{label}必须是绝对路径：{key}")
        return None
    if not path.is_file():
        diagnostics.append(f"{label}文件不存在：{path}")
        return None
    if path.name.casefold() not in EDITOR_NAMES:
        diagnostics.append(
            f"{label}应指向 UnrealEditor.exe 或 UnrealEditor-Cmd.exe：{path}"
        )
        return None
    return path


@dataclass
class EnvironmentReport:
    """记录 UE 环境检查结果，供人和脚本共同消费。"""

    env_file: Path
    installed_editor: Path | None = None
    source_editor: Path | None = None
    source_root: Path | None = None
    build_script: Path | None = None
    diagnostics: list[str] = field(default_factory=list)

    @property
    def editor_ready(self) -> bool:
        """下载版编辑器是否可以执行普通 Editor/Automation。"""
        return self.installed_editor is not None

    @property
    def dedicated_ready(self) -> bool:
        """源码版编辑器及其 Build.bat 是否满足 Dedicated 准入。"""
        return self.source_editor is not None and self.build_script is not None and self.build_script.is_file()

    def to_dict(self) -> dict[str, object]:
        """输出稳定的 JSON schema，不暴露 dotenv 中的无关变量。"""
        return {
            "env_file": str(self.env_file),
            "installed_editor": str(self.installed_editor) if self.installed_editor else None,
            "source_editor": str(self.source_editor) if self.source_editor else None,
            "source_root": str(self.source_root) if self.source_root else None,
            "build_script": str(self.build_script) if self.build_script else None,
            "editor_ready": self.editor_ready,
            "dedicated_ready": self.dedicated_ready,
            "diagnostics": list(self.diagnostics),
        }


def check_environment(repo_root: Path, env_file: Path | None = None) -> EnvironmentReport:
    """检查本地配置，不写入配置文件或修改工作区。"""
    root = repo_root.resolve()
    config_path = env_file or (root / ENV_FILE_NAME)
    if not config_path.is_absolute():
        config_path = root / config_path
    config_path = config_path.resolve(strict=False)
    report = EnvironmentReport(env_file=config_path)
    try:
        values = load_env_file(config_path)
    except EnvFileError as exc:
        report.diagnostics.append(str(exc))
        return report

    report.installed_editor = _validate_editor(
        values, INSTALLED_EDITOR_KEY, "下载版编辑器", report.diagnostics
    )
    report.source_editor = _validate_editor(
        values, SOURCE_EDITOR_KEY, "源码编译编辑器", report.diagnostics
    )

    if report.source_editor is not None:
        report.source_root = _engine_root(report.source_editor)
        if report.source_root is None:
            report.diagnostics.append(
                "源码编译编辑器路径必须位于包含 Engine 目录的源码引擎中"
            )
        else:
            report.build_script = (
                report.source_root / "Engine" / "Build" / "BatchFiles" / "Build.bat"
            )
            if not report.build_script.is_file():
                report.diagnostics.append(
                    f"源码引擎缺少 Build.bat：{report.build_script}"
                )
    return report


def _requirement_ready(report: EnvironmentReport, requirement: str) -> bool:
    if requirement == "editor":
        return report.editor_ready
    if requirement == "dedicated":
        return report.dedicated_ready
    if requirement == "all":
        return report.editor_ready and report.dedicated_ready
    return True


def _print_human(report: EnvironmentReport, requirement: str) -> None:
    print(f"UE 环境配置：{report.env_file}")
    print(f"下载版编辑器：{'READY' if report.editor_ready else 'UNAVAILABLE'}")
    print(f"Dedicated Server：{'READY' if report.dedicated_ready else 'UNAVAILABLE'}")
    if requirement != "none":
        state = "READY" if _requirement_ready(report, requirement) else "BLOCKED"
        print(f"要求 {requirement}：{state}")
    for diagnostic in report.diagnostics:
        print(f"- {diagnostic}")


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("command", nargs="?", choices=("check",), default="check")
    parser.add_argument("--repo-root", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--env-file", type=Path, help="默认读取仓库根目录的 .env")
    parser.add_argument(
        "--require",
        choices=("none", "editor", "dedicated", "all"),
        default="none",
        help="要求某类入口可用；不满足时返回 2",
    )
    parser.add_argument("--json", action="store_true", help="输出机器可读 JSON")
    args = parser.parse_args(argv)

    report = check_environment(args.repo_root, args.env_file)
    if args.json:
        print(json.dumps(report.to_dict(), ensure_ascii=False, indent=2))
    else:
        _print_human(report, args.require)
    return 0 if _requirement_ready(report, args.require) else 2


if __name__ == "__main__":
    sys.exit(main())
