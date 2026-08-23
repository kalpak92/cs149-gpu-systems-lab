#!/usr/bin/env python3
"""Validate repository-local documentation links, skills, and web artifacts."""

from __future__ import annotations

import re
import sys
from html.parser import HTMLParser
from pathlib import Path
from urllib.parse import unquote, urlsplit
from xml.etree import ElementTree


PROJECT_ROOT = Path(__file__).resolve().parents[1]
DOCS_ROOT = PROJECT_ROOT / "docs"
EXCLUDED_PARTS = {".git", ".venv", "__pycache__", "node_modules"}
MARKDOWN_LINK = re.compile(r"!?\[[^\]]*\]\(([^)]+)\)")
SKILL_REFERENCE = re.compile(r"`((?:assets|references|scripts)/[^`\s]+)`")


class ArtifactLinkParser(HTMLParser):
    """Collect href and src targets from a standalone HTML artifact."""

    def __init__(self) -> None:
        super().__init__(convert_charrefs=True)
        self.targets: list[tuple[int, str]] = []

    def handle_starttag(
        self,
        tag: str,
        attrs: list[tuple[str, str | None]],
    ) -> None:
        del tag
        line, _ = self.getpos()
        for name, value in attrs:
            if name in {"href", "src"} and value:
                self.targets.append((line, value))

    handle_startendtag = handle_starttag


def is_excluded(path: Path) -> bool:
    return any(part in EXCLUDED_PARTS for part in path.relative_to(PROJECT_ROOT).parts)


def markdown_files() -> list[Path]:
    files = {PROJECT_ROOT / "README.md", PROJECT_ROOT / "AGENTS.md"}
    for directory in ("docs", "experiments", "results", "templates"):
        files.update(
            path
            for path in (PROJECT_ROOT / directory).rglob("*.md")
            if not is_excluded(path)
        )
    files.update((PROJECT_ROOT / ".agents" / "skills").glob("*/SKILL.md"))
    files.update((PROJECT_ROOT / ".agents" / "skills").glob("*/UPSTREAM.md"))
    return sorted(path for path in files if path.exists())


def link_target(raw_target: str) -> str:
    target = raw_target.strip()
    if target.startswith("<") and ">" in target:
        return target[1 : target.index(">")]
    return target.split(maxsplit=1)[0]


def resolve_local_target(source: Path, raw_target: str, absolute_root: Path) -> Path | None:
    target = link_target(raw_target)
    if not target or target.startswith("#") or target.startswith("//"):
        return None

    parsed = urlsplit(target)
    if parsed.scheme:
        return None

    decoded_path = unquote(parsed.path)
    if not decoded_path:
        return None
    if decoded_path.startswith("/"):
        return absolute_root / decoded_path.lstrip("/")
    return source.parent / decoded_path


def validate_markdown(errors: list[str]) -> int:
    files = markdown_files()
    for path in files:
        content = path.read_text(encoding="utf-8")
        for match in MARKDOWN_LINK.finditer(content):
            target = resolve_local_target(path, match.group(1), PROJECT_ROOT)
            if target is not None and not target.exists():
                line = content.count("\n", 0, match.start()) + 1
                errors.append(f"{path.relative_to(PROJECT_ROOT)}:{line}: missing {target}")
    return len(files)


def validate_skills(errors: list[str]) -> int:
    skill_files = sorted((PROJECT_ROOT / ".agents" / "skills").glob("*/SKILL.md"))
    for path in skill_files:
        content = path.read_text(encoding="utf-8")
        lines = content.splitlines()
        closing_marker = next(
            (index for index, line in enumerate(lines[1:], start=1) if line == "---"),
            None,
        )
        if not lines or lines[0] != "---" or closing_marker is None:
            errors.append(f"{path.relative_to(PROJECT_ROOT)}: missing YAML frontmatter")
            continue

        frontmatter = "\n".join(lines[1:closing_marker])
        if not re.search(r"^name:\s*\S+", frontmatter, re.MULTILINE):
            errors.append(f"{path.relative_to(PROJECT_ROOT)}: missing skill name")
        if not re.search(r"^description:\s*(?:[|>]|\S+)", frontmatter, re.MULTILINE):
            errors.append(f"{path.relative_to(PROJECT_ROOT)}: missing skill description")

        for match in SKILL_REFERENCE.finditer(content):
            target = path.parent / match.group(1)
            if not target.exists():
                line = content.count("\n", 0, match.start()) + 1
                errors.append(f"{path.relative_to(PROJECT_ROOT)}:{line}: missing {target}")
    return len(skill_files)


def validate_artifacts(errors: list[str]) -> tuple[int, int]:
    svg_files = sorted(DOCS_ROOT.rglob("*.svg"))
    for path in svg_files:
        try:
            root = ElementTree.parse(path).getroot()
        except ElementTree.ParseError as error:
            errors.append(f"{path.relative_to(PROJECT_ROOT)}: invalid XML: {error}")
            continue
        if root.tag.rsplit("}", maxsplit=1)[-1] != "svg":
            errors.append(f"{path.relative_to(PROJECT_ROOT)}: root element is not svg")

    html_files = sorted(DOCS_ROOT.rglob("*.html"))
    for path in html_files:
        parser = ArtifactLinkParser()
        try:
            parser.feed(path.read_text(encoding="utf-8"))
            parser.close()
        except Exception as error:  # HTMLParser reports malformed references as ValueError.
            errors.append(f"{path.relative_to(PROJECT_ROOT)}: invalid HTML: {error}")
            continue
        for line, raw_target in parser.targets:
            target = resolve_local_target(path, raw_target, DOCS_ROOT)
            if target is not None and not target.exists():
                errors.append(f"{path.relative_to(PROJECT_ROOT)}:{line}: missing {target}")

    return len(svg_files), len(html_files)


def main() -> int:
    errors: list[str] = []
    markdown_count = validate_markdown(errors)
    skill_count = validate_skills(errors)
    svg_count, html_count = validate_artifacts(errors)

    if errors:
        for error in errors:
            print(error, file=sys.stderr)
        return 1

    print(
        "Repository documentation valid: "
        f"{markdown_count} Markdown files, {skill_count} skills, "
        f"{svg_count} SVG files, {html_count} HTML files."
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
