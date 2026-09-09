#!/usr/bin/env python3
"""只读检查文档结构、本地链接和 Spec 格式；不判定业务完成度。"""

from __future__ import annotations

import argparse
import html
import json
import re
from pathlib import Path
from urllib.parse import unquote, urlsplit

DOC_ROOT = Path('Doc/CombatSystem')
REQUIRED_DIRECTORIES = tuple(str(DOC_ROOT / name) for name in (
    '00-Project', '10-Architecture', '20-Content', '30-Tooling', '90-History', 'Specs'
))
REQUIRED_DOCUMENTS = (
    'README.md', 'agent.md', 'AGENTS.md', str(DOC_ROOT / 'README.md'),
    str(DOC_ROOT / '00-Project/00-01-Progress-Tracker.md'),
    str(DOC_ROOT / '00-Project/00-05-AI-Native-Development-Workflow.md'),
    str(DOC_ROOT / '00-Project/_intake-template.md'),
    str(DOC_ROOT / '00-Project/_gate-checklist.md'),
    str(DOC_ROOT / '00-Project/_delivery-record-template.md'),
    str(DOC_ROOT / 'Specs/_template.spec.md'),
)
SPEC_SECTIONS = (
    ('目标与范围',), ('当前事实与依据',), ('验收标准', '验收标准（AC）'),
    ('测试矩阵与命令', '验证证据'), ('交付证据', '交付与后续'),
)


def mask_code(text: str) -> str:
    """掩掉代码示例并保留位置，使诊断行号仍指向原文。"""
    output = []
    fence = ''
    for line in text.splitlines(keepends=True):
        opening = re.match(r'^ {0,3}(`{3,}|~{3,})', line)
        if fence:
            closing = re.match(r'^ {0,3}(' + re.escape(fence[0]) + r'{'
                               + str(len(fence)) + r',})\s*$', line)
            output.append(re.sub(r'[^\n]', ' ', line))
            if closing:
                fence = ''
        elif opening:
            fence = opening.group(1)
            output.append(re.sub(r'[^\n]', ' ', line))
        else:
            output.append(line)
    # 行内代码不会创建实际链接。替换后的标签仍可与其链接目标匹配。
    return re.sub(r'(`+)(.*?)\1', lambda m: re.sub(r'[^\n]', ' ', m.group()),
                  ''.join(output), flags=re.DOTALL)


def destination_at(text: str, start: int) -> str:
    """提取常见 Markdown 目标，保留转义，支持括号和 <带空格路径>。"""
    while start < len(text) and text[start].isspace():
        start += 1
    if start == len(text):
        return ''
    if text[start] == '<':
        match = re.match(r'<((?:\\.|[^>])*)>', text[start:])
        return match.group(1) if match else ''
    end, depth = start, 0
    while end < len(text):
        char = text[end]
        if char == '\\' and end + 1 < len(text):
            end += 2
            continue
        if char.isspace() or (char == ')' and depth == 0):
            break
        if char == '(':
            depth += 1
        elif char == ')':
            depth -= 1
        end += 1
    return text[start:end]


def markdown_links(text: str):
    """返回链接位置和未定义的显式引用；代码和裸文本不创建链接。"""
    visible = mask_code(text)
    links, undefined, references = [], [], {}
    for match in re.finditer(r'(?<!\\)\]\(', visible):
        links.append((match.start(), destination_at(visible, match.end())))
    for match in re.finditer(r'^ {0,3}\[([^\]\n]+)\]:[ \t]*(.*)$', visible, re.MULTILINE):
        key = ' '.join(match.group(1).split()).casefold()
        if key.startswith('^'):
            continue
        references[key] = destination_at(match.group(2), 0)
        links.append((match.start(), references[key]))
    for match in re.finditer(r'(?<!\\)\[([^\]\n]+)\]\[([^\]\n]*)\]', visible):
        key = ' '.join((match.group(2) or match.group(1)).split()).casefold()
        if key not in references:
            undefined.append((match.start(), key))
    return links, undefined


def legacy_path(path: Path) -> bool:
    return (path.as_posix() == 'Doc/DotaLikeGASCombatSystemDesign.md'
            or (path.parent == DOC_ROOT and re.match(r'^\d{2}-.*\.md$', path.name) is not None))


def validate(root: Path) -> dict:
    """返回可序列化报告，不写文档、Git 索引或项目状态。"""
    root = root.resolve()
    errors = []

    def issue(code, path, message, line=0):
        errors.append({'code': code, 'file': str(path), 'line': line, 'message': message})

    for relative in REQUIRED_DOCUMENTS:
        if not (root / relative).is_file():
            issue('missing_entry', relative, '必需文档不存在或不是文件')
    for relative in REQUIRED_DIRECTORIES:
        if not (root / relative).is_dir():
            issue('missing_entry', relative, '必需目录不存在')
        # 目录号保证分类一致，目录内序号保证唯一；允许删除文档后的编号空档。
        directory = root / relative
        prefix = directory.name[:2]
        if not prefix.isdigit():
            continue
        numbers = {}
        for path in sorted(directory.glob('*.md')):
            if path.name == 'README.md' or path.name.startswith('_'):
                continue
            match = re.fullmatch(re.escape(prefix) + r'-(\d{2})-[^/]+\.md', path.name)
            if not match or match.group(1) == '00':
                issue('invalid_doc_number', path.relative_to(root),
                      f'文档应命名为 {prefix}-NN-主题.md，NN 从 01 开始')
                continue
            number = match.group(1)
            if number in numbers:
                issue('duplicate_doc_number', path.relative_to(root),
                      f'编号 {prefix}-{number} 与 {numbers[number]} 重复')
            numbers[number] = path.name
    for path in (root / DOC_ROOT).glob('[0-9][0-9]-*.md'):
        issue('legacy_file', path.relative_to(root), '平铺文档应按职责迁入子目录')
    old_index = root / 'Doc/DotaLikeGASCombatSystemDesign.md'
    if old_index.exists():
        issue('legacy_file', old_index.relative_to(root), '旧总索引已迁入 CombatSystem/README.md')

    files = {root / name for name in ('README.md', 'agent.md', 'AGENTS.md')}
    files.update((root / 'Doc').rglob('*.md'))
    checked = link_count = 0
    for path in sorted(files):
        relative = path.relative_to(root).as_posix()
        if not path.is_file():
            continue
        try:
            text = path.read_text(encoding='utf-8')
        except (OSError, UnicodeError) as error:
            issue('read_error', relative, str(error))
            continue
        checked += 1
        for number, line in enumerate(text.splitlines(), 1):
            if line.rstrip() != line:
                issue('trailing_whitespace', relative, '移除行末空白；换行使用空行或 <br>', number)
        links, undefined = markdown_links(text)
        for offset, key in undefined:
            issue('undefined_reference', relative, f'引用未定义：{key}', text.count('\n', 0, offset) + 1)
        for offset, target in links:
            number = text.count('\n', 0, offset) + 1
            target = html.unescape(re.sub(r'\\([\\`*{}\[\]()#+.!_<> ])', r'\1', target))
            if not target or target.startswith('#'):
                continue
            try:
                url = urlsplit(target)
                if url.scheme or url.netloc:
                    continue
                local = unquote(url.path)
            except ValueError as error:
                issue('broken_link', relative, f'{target}: {error}', number)
                continue
            if not local:
                continue
            link_count += 1
            destination = ((root / local.lstrip('/')) if local.startswith('/')
                           else (path.parent / local)).resolve()
            try:
                repo_path = destination.relative_to(root)
            except ValueError:
                issue('outside_root', relative, f'本地链接越出仓库：{target}', number)
                continue
            if legacy_path(repo_path):
                issue('legacy_link', relative, f'链接仍指向旧路径：{target}', number)
            if not destination.exists():
                issue('broken_link', relative, f'本地目标不存在：{target}', number)

        if path.parent == root / DOC_ROOT / 'Specs' and path.name.endswith('.spec.md'):
            headings = {re.sub(r'^\d+\.\s*', '', title).strip()
                        for title in re.findall(r'^##\s+(.+)$', mask_code(text), re.MULTILINE)}
            for aliases in SPEC_SECTIONS:
                if not any(title in headings for title in aliases):
                    issue('invalid_spec', relative, f'缺少章节：{aliases[0]}')
            for field in ('Spec 版本', '状态', '风险等级'):
                match = re.search(r'^>\s*' + re.escape(field) + r'[：:][ \t]*(.*)$', text, re.MULTILINE)
                value = match.group(1).strip().strip('`') if match else ''
                if not value:
                    issue('invalid_spec', relative, f'缺少字段或值：{field}')
                elif field == '风险等级' and path.name != '_template.spec.md' and value not in ('L0', 'L1', 'L2'):
                    issue('invalid_spec', relative, '风险等级应选择 L0、L1 或 L2')

    return {'schema_version': 1, 'passed': not errors, 'files_checked': checked,
            'local_links_checked': link_count, 'errors': errors}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, default=Path(__file__).resolve().parents[1],
                        help='待检查的仓库；默认使用脚本所在仓库')
    parser.add_argument('--report', type=Path, help='可选 JSON 报告路径，相对路径基于当前工作目录')
    args = parser.parse_args()
    if not args.root.is_dir():
        parser.error('仓库根目录不存在')
    result = validate(args.root)
    if args.report:
        try:
            args.report.parent.mkdir(parents=True, exist_ok=True)
            args.report.write_text(json.dumps(result, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
        except OSError as error:
            parser.error(f'不能写报告：{error}')
    outcome = 'passed' if result['passed'] else 'failed'
    print(f"Combat docs validation {outcome}: {result['files_checked']} Markdown files, "
          f"{result['local_links_checked']} local links, {len(result['errors'])} error(s)")
    for error in result['errors']:
        print(f"- {error['file']}:{error['line']} [{error['code']}] {error['message']}")
    return 0 if result['passed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
