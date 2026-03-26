#!/usr/bin/env python3

"""Normalize quoted relative includes in tracked C/C++ files."""

from __future__ import annotations

import argparse
import os
import re
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path

SOURCE_EXTENSIONS = {
    ".cpp",
    ".h",
}

INCLUDE_RE = re.compile(
    r'^(?P<prefix>\s*#\s*include\s*")(?P<path>[^"]+)(?P<suffix>".*)$'
)


@dataclass(frozen=True)
class IncludeChange:
    file_path: Path
    line_no: int
    before: str
    after: str


def tracked_source_files(repo_root: Path) -> list[Path]:
    output = subprocess.check_output(
        ["git", "-C", str(repo_root), "ls-files"],
        text=True,
    )
    files: list[Path] = []
    for rel in output.splitlines():
        path = repo_root / rel
        if path.suffix.lower() not in SOURCE_EXTENSIONS:
            continue
        if path.is_file():
            files.append(path)
    return files


def canonical_include_path(
    include_path: str, file_path: Path, line_no: int
) -> str | None:
    if not (include_path.startswith("../") or include_path.startswith("./")):
        return None

    file_dir = file_path.parent
    target = Path(os.path.normpath(str(file_dir / include_path)))
    if not target.is_file():
        raise FileNotFoundError(
            f"{file_path}:{line_no}: include target does not exist: "
            f"{include_path} -> {target}"
        )

    normalized = os.path.relpath(target, start=file_dir)
    normalized = Path(normalized).as_posix()
    if normalized == include_path:
        return None

    return normalized


def split_eol(line: str) -> tuple[str, str]:
    if line.endswith("\r\n"):
        return line[:-2], "\r\n"
    if line.endswith("\n"):
        return line[:-1], "\n"
    return line, ""


def process_file(file_path: Path, write: bool) -> list[IncludeChange]:
    original_lines = file_path.read_text(encoding="utf-8").splitlines(keepends=True)
    updated_lines = list(original_lines)
    changes: list[IncludeChange] = []

    for index, line in enumerate(original_lines):
        content, eol = split_eol(line)
        match = INCLUDE_RE.match(content)
        if not match:
            continue

        include_path = match.group("path")
        canonical = canonical_include_path(include_path, file_path, index + 1)
        if canonical is None:
            continue

        updated_lines[index] = (
            f"{match.group('prefix')}{canonical}{match.group('suffix')}{eol}"
        )
        changes.append(
            IncludeChange(
                file_path=file_path,
                line_no=index + 1,
                before=include_path,
                after=canonical,
            )
        )

    if write and changes:
        file_path.write_text("".join(updated_lines), encoding="utf-8")

    return changes


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Normalize quoted relative include paths in tracked C/C++ files."
    )
    parser.add_argument(
        "--write",
        action="store_true",
        help="Write updated include paths back to files.",
    )
    parser.add_argument(
        "--repo",
        type=Path,
        default=Path(__file__).resolve().parents[1],
        help="Repository root (defaults to script parent parent).",
    )
    args = parser.parse_args()

    repo_root = args.repo.resolve()
    files = tracked_source_files(repo_root)

    all_changes: list[IncludeChange] = []
    try:
        for file_path in files:
            all_changes.extend(process_file(file_path, write=args.write))
    except FileNotFoundError as exc:
        print(str(exc), file=sys.stderr)
        return 2

    mode = "write" if args.write else "check"
    print(f"Mode: {mode}")
    print(f"Files scanned: {len(files)}")
    print(f"Includes to normalize: {len(all_changes)}")

    for change in all_changes:
        rel_path = change.file_path.relative_to(repo_root).as_posix()
        print(
            f"{rel_path}:{change.line_no}: {change.before} -> {change.after}"
        )

    if not args.write and all_changes:
        return 1

    return 0


if __name__ == "__main__":
    sys.exit(main())
