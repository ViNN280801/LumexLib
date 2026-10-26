#!/usr/bin/env python3
"""List the includes of standard headers newer than C++11 without a standard guard.

The MSVC STL warns when a header of a newer standard is included (STL4038,
"The contents of <compare> are available only with C++20 or later"). The
message comes from `#pragma message`, so /WX does not stop it, and a build at
an older standard prints it once per translation unit. `__has_include` is no
guard: the header exists at every standard.

The check looks at every `.hpp`, `.h`, `.inl` and `.cpp` file under lumex/
outside lumex/tests, lumex/examples and any 3rdparty directory. An include of
a C++17, C++20 or C++23 header is accepted when an enclosing `#if`, `#elif` or
`#ifdef` (not its `#else` branch) requires the standard: `__cplusplus` or
`_MSVC_LANG` compared with `>=` or `>` to a value that reaches it (`>= 201703L`
or `> 201402L` for C++17, `>= 202002L` or `> 201703L` for C++20, `>= 202302L`
or `> 202002L` for C++23), or a feature-test macro (`__cpp_lib_...`) or a
`LUMEX_HAS_STD_...` macro, which are set only from the standard library.

Usage (from LumexLib/):
  python Scripts/CodeTools/check_std_header_guards.py            # list, exit 0
  python Scripts/CodeTools/check_std_header_guards.py --check    # exit 1 if any
  python Scripts/CodeTools/check_std_header_guards.py --dir <copy>/lumex --check
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

# The headers that the MSVC STL fills only from a later standard.
HEADER_STANDARD = {
    "any": 17, "charconv": 17, "execution": 17, "filesystem": 17,
    "memory_resource": 17, "optional": 17, "string_view": 17, "variant": 17,
    "barrier": 20, "bit": 20, "compare": 20, "concepts": 20, "coroutine": 20,
    "format": 20, "latch": 20, "numbers": 20, "ranges": 20, "semaphore": 20,
    "source_location": 20, "span": 20, "stop_token": 20, "syncstream": 20,
    "expected": 23, "flat_map": 23, "flat_set": 23, "generator": 23,
    "mdspan": 23, "print": 23, "spanstream": 23, "stacktrace": 23,
    "stdfloat": 23,
}
# __cplusplus of each standard and of the one before it.
STANDARD_VALUE = {14: 201402, 17: 201703, 20: 202002, 23: 202302}
PREVIOUS = {17: 14, 20: 17, 23: 20}

INCLUDE = re.compile(r"^\s*#\s*include\s*<(\w+)>")
IF = re.compile(r"^\s*#\s*if\b(.*)")
IFDEF = re.compile(r"^\s*#\s*ifdef\b\s*(\w+)")
IFNDEF = re.compile(r"^\s*#\s*ifndef\b\s*(\w+)")
ELIF = re.compile(r"^\s*#\s*elif\b(.*)")
ELSE = re.compile(r"^\s*#\s*else\b")
ENDIF = re.compile(r"^\s*#\s*endif\b")
LANGUAGE = re.compile(r"\b(?:__cplusplus|_MSVC_LANG)\s*(>=|>)\s*(\d+)L?\b")
LIBRARY_MACRO = re.compile(r"\b(?:__cpp_lib_\w+|LUMEX_HAS_STD_\w+)\b")


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


def requires_standard(condition: str, standard: int) -> bool:
    """True for a condition that holds only from the given standard on."""
    if "||" in condition:
        return False
    if LIBRARY_MACRO.search(condition):
        return True
    for operator, value in LANGUAGE.findall(condition):
        number = int(value)
        if operator == ">=" and number >= STANDARD_VALUE[standard]:
            return True
        if operator == ">" and number >= STANDARD_VALUE[PREVIOUS[standard]]:
            return True
    return False


def joined_lines(text: str) -> List[Tuple[int, str]]:
    """Lines with their continuations (a trailing backslash) joined."""
    result: List[Tuple[int, str]] = []
    pending = ""
    start = 0
    for number, line in enumerate(text.split("\n"), start=1):
        if not pending:
            start = number
        if line.rstrip().endswith("\\"):
            pending += line.rstrip()[:-1] + " "
            continue
        result.append((start, pending + line))
        pending = ""
    if pending:
        result.append((start, pending))
    return result


def unguarded_includes(text: str) -> List[Tuple[int, str, int]]:
    """Return (line number, header, standard) of every include without a guard."""
    found: List[Tuple[int, str, int]] = []
    # One entry per open #if: [condition text, in the #else branch].
    stack: List[List[object]] = []
    for number, line in joined_lines(text):
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
        match = INCLUDE.match(line)
        if match and match.group(1) in HEADER_STANDARD:
            standard = HEADER_STANDARD[match.group(1)]
            guarded = any(
                requires_standard(str(entry[0]), standard) and not entry[1]
                for entry in stack
            )
            if not guarded:
                found.append((number, match.group(1), standard))
    return found


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.split("\n", 1)[0])
    parser.add_argument("--dir", type=Path, default=default_root())
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()

    total = 0
    for path in iter_sources(args.dir):
        text = path.read_text(encoding="utf-8", errors="replace")
        for number, header, standard in unguarded_includes(text):
            total += 1
            print("%s:%d: <%s> (C++%d) is not under a C++%d guard" % (
                path.relative_to(args.dir.parent).as_posix(), number, header,
                standard, standard))
    print("%d unguarded standard header include(s)" % total)
    return 1 if args.check and total else 0


if __name__ == "__main__":
    sys.exit(main())
