#!/usr/bin/env python3
"""List the MSVC-only pragmas that are not guarded by _MSC_VER.

`#pragma warning` and `#pragma comment` exist in MSVC and clang-cl, which
both define _MSC_VER. GCC and Clang with the GNU driver (MinGW included)
report every one of them as an unknown pragma, so a build with -Werror stops
on the first. A guard on _WIN32 is not enough: MinGW defines _WIN32 too.

The check looks at every `.hpp`, `.h`, `.inl` and `.cpp` file under lumex/
outside lumex/tests, lumex/examples and any 3rdparty directory. A pragma is
accepted when an enclosing `#if` or `#ifdef` requires _MSC_VER (written as
`defined(_MSC_VER)`, `_MSC_VER` or `defined(_MSC_VER) && ...`) and the
pragma is not in its `#else` branch. A condition with `||` does not count.

Usage (from LumexLib/):
  python Scripts/CodeTools/check_msvc_pragmas.py            # list, exit 0
  python Scripts/CodeTools/check_msvc_pragmas.py --check    # exit 1 if any
  python Scripts/CodeTools/check_msvc_pragmas.py --dir <copy>/lumex --check
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path
from typing import Iterator, List, Tuple

SUFFIXES = {".hpp", ".h", ".inl", ".cpp"}
SKIPPED_TOP_DIRS = {"tests", "examples"}
SKIPPED_DIR_NAMES = {"3rdparty"}

PRAGMA = re.compile(r"^\s*#\s*pragma\s+(warning|comment)\b")
IF = re.compile(r"^\s*#\s*if\b(.*)")
IFDEF = re.compile(r"^\s*#\s*ifdef\b\s*(\w+)")
IFNDEF = re.compile(r"^\s*#\s*ifndef\b\s*(\w+)")
ELIF = re.compile(r"^\s*#\s*elif\b(.*)")
ELSE = re.compile(r"^\s*#\s*else\b")
ENDIF = re.compile(r"^\s*#\s*endif\b")
MSC_VER = re.compile(r"\b_MSC_VER\b")


def default_root() -> Path:
    return Path(__file__).resolve().parents[2] / "lumex"


def iter_sources(root: Path) -> Iterator[Path]:
    for path in sorted(root.rglob("*")):
        if not path.is_file() or path.suffix not in SUFFIXES:
            continue
        relative = path.relative_to(root)
        if relative.parts and relative.parts[0] in SKIPPED_TOP_DIRS:
            continue
        if SKIPPED_DIR_NAMES.intersection(relative.parts):
            continue
        yield path


def requires_msvc(condition: str) -> bool:
    """True for a condition that holds only when _MSC_VER is defined."""
    if not MSC_VER.search(condition) or "||" in condition:
        return False
    return not re.search(r"!\s*(defined\s*\(\s*)?_MSC_VER", condition)


def unguarded_pragmas(text: str) -> List[Tuple[int, str]]:
    """Return (line number, pragma name) of every pragma outside _MSC_VER."""
    found: List[Tuple[int, str]] = []
    # One entry per open #if: [condition text, negated by #else].
    stack: List[List[object]] = []
    joined = text.split("\n")
    for number, line in enumerate(joined, start=1):
        match = IF.match(line)
        if match:
            stack.append([match.group(1), False])
            continue
        match = IFDEF.match(line)
        if match:
            stack.append(["defined(%s)" % match.group(1), False])
            continue
        match = IFNDEF.match(line)
        if match:
            stack.append(["defined(%s)" % match.group(1), True])
            continue
        match = ELIF.match(line)
        if match and stack:
            stack[-1] = [match.group(1), False]
            continue
        if ELSE.match(line) and stack:
            stack[-1][1] = not stack[-1][1]
            continue
        if ENDIF.match(line):
            if stack:
                stack.pop()
            continue
        match = PRAGMA.match(line)
        if match:
            guarded = any(
                requires_msvc(str(entry[0])) and not entry[1] for entry in stack
            )
            if not guarded:
                found.append((number, match.group(1)))
    return found


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.split("\n", 1)[0])
    parser.add_argument("--dir", type=Path, default=default_root())
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()

    total = 0
    for path in iter_sources(args.dir):
        text = path.read_text(encoding="utf-8", errors="replace")
        for number, name in unguarded_pragmas(text):
            total += 1
            print("%s:%d: #pragma %s is not under _MSC_VER" % (
                path.relative_to(args.dir.parent).as_posix(), number, name))
    print("%d unguarded MSVC pragma(s)" % total)
    return 1 if args.check and total else 0


if __name__ == "__main__":
    sys.exit(main())
