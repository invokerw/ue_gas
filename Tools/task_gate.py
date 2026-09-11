#!/usr/bin/env python3
"""检查 Combat 任务的 Spec、Skill 路由、交付证据和工作区变更。"""

from __future__ import annotations

import argparse
import json
import re
import subprocess
import sys
from pathlib import Path
from typing import Iterable


SPEC_ROOT = Path("Doc/CombatSystem/Specs")
PRE_BUILD_STATUSES = {"PLANNED", "APPROVED", "BUILDING", "IN_PROGRESS", "READY_FOR_REVIEW"}
DELIVERY_STATUSES = {"READY", "READY_FOR_REVIEW", "COMPLETED", "PASS", "待验收", "已完成", "已验收"}
RISK_LEVELS = {"L0", "L1", "L2"}
REQUIRED_SECTIONS = (
    ("目标与范围",),
    ("当前事实与依据",),
    ("验收标准", "验收标准（AC）"),
    ("测试矩阵与命令", "验证证据"),
    ("交付证据", "交付与后续"),
)
BEHAVIOR_ROOTS = ("Source/", "Content/", "Tools/")


def relative(root: Path, path: Path) -> str:
    """将路径统一成报告中的仓库相对 POSIX 路径。"""
    return path.resolve().relative_to(root.resolve()).as_posix()


def field(text: str, name: str) -> str:
    """读取 Spec 顶部 blockquote 字段，兼容中英文冒号和反引号值。"""
    match = re.search(r"^>\s*" + re.escape(name) + r"[：:]\s*(.*?)\s*$", text, re.MULTILINE)
    return match.group(1).strip().strip("`") if match else ""


def has_section(text: str, aliases: Iterable[str]) -> bool:
    headings = re.findall(r"^#{1,6}\s+(.+?)\s*$", text, re.MULTILINE)
    return any(any(alias in heading for alias in aliases) for heading in headings)


def has_marker(text: str, *markers: str) -> bool:
    return all(marker in text for marker in markers)


def labeled_value(text: str, label: str) -> str:
    """读取流程记录中的字段值，避免只有字段名而没有实际结论。"""
    match = re.search(
        r"(?im)^[ \t>*-]*" + re.escape(label) + r"\s*[：:]\s*([^\n]+)", text
    )
    if not match:
        return ""
    value = match.group(1).strip().strip("`")
    if value.startswith("<") or value in {"...", "—", "-"} or value.upper() in {
        "TODO", "TBD", "待填写", "待定", "待补充", "未填写", "PLACEHOLDER"
    }:
        return ""
    return value


def conclusion(text: str, gate: str, expected: str) -> bool:
    """检查 F0/F1/F2/Push-Ready 是否写入明确结论。"""
    pattern = rf"(?is)\b{re.escape(gate)}(?:\s+结论)?\s*[：:]?\s*`?{re.escape(expected)}\b"
    return re.search(pattern, text) is not None


def issue(code: str, path: str, message: str, line: int = 0) -> dict:
    return {"code": code, "file": path, "line": line, "message": message}


def read_spec(root: Path, spec_arg: str) -> tuple[Path | None, str, list[dict]]:
    """读取并检查 Spec 路径，返回规范化路径、文本和路径类错误。"""
    candidate = (root / spec_arg).resolve() if not Path(spec_arg).is_absolute() else Path(spec_arg).resolve()
    errors: list[dict] = []
    try:
        spec_relative = relative(root, candidate)
    except ValueError:
        return None, "", [issue("spec_outside_root", spec_arg, "Spec 必须位于仓库内")]
    if not spec_relative.startswith(SPEC_ROOT.as_posix() + "/") or not spec_relative.endswith(".spec.md"):
        errors.append(issue("invalid_spec_path", spec_relative, "Spec 必须位于 Doc/CombatSystem/Specs/ 且以 .spec.md 结尾"))
    if candidate.name == "_template.spec.md":
        errors.append(issue("template_not_task", spec_relative, "任务 Gate 不能直接使用 Spec 模板"))
    if not candidate.is_file():
        errors.append(issue("missing_spec", spec_relative, "任务 Spec 不存在"))
        return candidate, "", errors
    try:
        return candidate, candidate.read_text(encoding="utf-8"), errors
    except (OSError, UnicodeError) as exc:
        errors.append(issue("spec_read_error", spec_relative, str(exc)))
        return candidate, "", errors


def collect_changed_files(root: Path) -> tuple[list[str], list[dict]]:
    """读取 HEAD 以来的已跟踪和未跟踪文件，不修改 Git 或工作区。"""
    errors: list[dict] = []
    names: set[str] = set()
    commands = (
        ["git", "diff", "--name-only", "HEAD", "--"],
        ["git", "ls-files", "--others", "--exclude-standard"],
    )
    for command in commands:
        try:
            result = subprocess.run(command, cwd=root, capture_output=True, text=True,
                                    encoding="utf-8", errors="replace", check=False)
        except OSError as exc:
            errors.append(issue("git_unavailable", ".", f"无法读取工作区变更：{exc}"))
            return [], errors
        if result.returncode != 0:
            errors.append(issue("git_status_failed", ".", result.stderr.strip() or "Git 变更读取失败"))
            return [], errors
        names.update(line.strip().replace("\\", "/") for line in result.stdout.splitlines() if line.strip())
    return sorted(names), errors


def behavior_files(changed: Iterable[str]) -> list[str]:
    return [path for path in changed if path.startswith(BEHAVIOR_ROOTS)]


def test_files(changed: Iterable[str]) -> list[str]:
    return [path for path in changed if (
        "/Tests/" in f"/{path}/" or path.startswith("Tools/Tests/")
        or path.endswith("_test.py") or path.startswith("test_")
    )]


def documentation_files(changed: Iterable[str]) -> list[str]:
    return [path for path in changed if path == "README.md" or path.startswith("Doc/")]


def evaluate(root: Path, spec_arg: str, mode: str, kind: str = "feature",
             changed: list[str] | None = None) -> dict:
    """执行一个无副作用的任务 Gate；可传入 changed 供单测隔离 Git 状态。"""
    root = root.resolve()
    spec_path, text, errors = read_spec(root, spec_arg)
    spec_relative = relative(root, spec_path) if spec_path and spec_path.exists() else spec_arg.replace("\\", "/")

    if text:
        status = field(text, "状态")
        risk = field(text, "风险等级")
        if not field(text, "Spec 版本"):
            errors.append(issue("missing_spec_version", spec_relative, "缺少 Spec 版本"))
        if not status:
            errors.append(issue("missing_status", spec_relative, "缺少 Spec 状态"))
        if risk not in RISK_LEVELS:
            errors.append(issue("invalid_risk", spec_relative, "风险等级必须是 L0、L1 或 L2"))
        for aliases in REQUIRED_SECTIONS:
            if not has_section(text, aliases):
                errors.append(issue("missing_section", spec_relative, f"缺少章节：{aliases[0]}"))
        route = labeled_value(text, "主 Skill")
        confidence = labeled_value(text, "路由置信度").lower()
        attachment = labeled_value(text, "附件解释")
        if not route or confidence not in {"high", "medium", "low"}:
            errors.append(issue("missing_skill_route", spec_relative, "必须记录主 Skill 和路由置信度"))
        if not conclusion(text, "F0", "GO") and not conclusion(text, "F0", "DEFER") and not conclusion(text, "F0", "ESCALATE"):
            errors.append(issue("missing_f0", spec_relative, "必须记录 F0 结论 GO/DEFER/ESCALATE"))
        if not labeled_value(text, "用户请求") or not attachment:
            errors.append(issue("missing_request_attachment_split", spec_relative, "必须区分用户请求与附件解释"))
        if mode == "preflight" and status not in PRE_BUILD_STATUSES:
            errors.append(issue("invalid_preflight_status", spec_relative, f"状态 {status or '<empty>'} 不允许进入开工 Gate"))
        if mode == "delivery":
            if status not in DELIVERY_STATUSES:
                errors.append(issue("invalid_delivery_status", spec_relative, "交付状态必须是 READY/READY_FOR_REVIEW/COMPLETED 或项目中文状态"))
            if not conclusion(text, "F1", "APPROVED"):
                errors.append(issue("missing_f1", spec_relative, "交付前必须记录 F1 APPROVED"))
            if not conclusion(text, "F2", "PASS"):
                errors.append(issue("missing_f2", spec_relative, "交付前必须记录 F2 PASS"))
            if not conclusion(text, "Push-Ready", "READY"):
                errors.append(issue("missing_push_ready", spec_relative, "交付前必须记录 Push-Ready 结论"))
            if not has_marker(text, "未执行"):
                errors.append(issue("missing_unexecuted_record", spec_relative, "必须明确列出未执行项或写 N/A 及原因"))
            if not has_marker(text, "验证"):
                errors.append(issue("missing_verification_record", spec_relative, "必须记录实际验证命令和结果"))

    changed_files = list(changed) if changed is not None else []
    git_errors: list[dict] = []
    if mode == "delivery" and changed is None:
        changed_files, git_errors = collect_changed_files(root)
        errors.extend(git_errors)
    if mode == "delivery" and spec_relative not in changed_files:
        errors.append(issue("spec_not_changed", spec_relative, "交付时必须让本次任务 Spec 与实现/验证证据一起进入 diff"))
    behavior = behavior_files(changed_files)
    tests = test_files(changed_files)
    docs = documentation_files(changed_files)
    if mode == "delivery" and behavior:
        if kind == "docs":
            errors.append(issue("kind_mismatch", ".", "任务声明为 docs，但工作区包含代码、工具或资产变更"))
        if not tests:
            errors.append(issue("missing_tests", ".", "检测到代码/工具/资产变更，但没有对应测试文件变更"))
        if not docs:
            errors.append(issue("missing_docs", ".", "检测到行为变更，但没有 Spec、DDD、README 或其他文档变更"))
    return {
        "schema_version": 1,
        "passed": not errors,
        "mode": mode,
        "kind": kind,
        "spec": spec_relative,
        "changed_files": changed_files,
        "behavior_files": behavior,
        "test_files": tests,
        "documentation_files": docs,
        "errors": errors,
    }


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mode", choices=("preflight", "delivery"), required=True,
                        help="preflight 在实现前运行；delivery 在交付前运行")
    parser.add_argument("--spec", required=True, help="仓库内任务 Spec 路径")
    parser.add_argument("--kind", choices=("feature", "docs", "process"), default="feature",
                        help="任务类型；docs 只允许文档变更，process 用于流程/工具变更")
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--report", type=Path, help="可选 JSON 报告路径")
    args = parser.parse_args(argv)
    root = args.root.resolve()
    if not root.is_dir():
        parser.error("仓库根目录不存在")
    result = evaluate(root, args.spec, args.mode, args.kind)
    if args.report:
        report = args.report if args.report.is_absolute() else Path.cwd() / args.report
        report.parent.mkdir(parents=True, exist_ok=True)
        report.write_text(json.dumps(result, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    outcome = "passed" if result["passed"] else "failed"
    print(f"Combat task gate {args.mode} {outcome}: {len(result['errors'])} error(s), "
          f"{len(result['changed_files'])} changed file(s)")
    for item in result["errors"]:
        print(f"- {item['file']}:{item['line']} [{item['code']}] {item['message']}")
    return 0 if result["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
