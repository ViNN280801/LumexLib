#!/usr/bin/env python3
"""Check (and optionally rewrite) LumexLib include-group order.

Groups (quality-gates.md section 7):
  1. lumex/LumexExport.hpp
  2. Standard C++ and C headers together
  3. Third-party / platform angle includes
  4. Project quoted includes ("lumex/...", same-dir children)

Usage (from LumexLib/):
  python Scripts/CodeTools/check_include_order.py --check
  python Scripts/CodeTools/check_include_order.py --fix
"""
from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

STD_CPP = {
    "algorithm",
    "any",
    "array",
    "atomic",
    "bitset",
    "cassert",
    "cctype",
    "cerrno",
    "cfenv",
    "cfloat",
    "charconv",
    "chrono",
    "cinttypes",
    "climits",
    "clocale",
    "cmath",
    "codecvt",
    "compare",
    "complex",
    "concepts",
    "condition_variable",
    "coroutine",
    "csetjmp",
    "csignal",
    "cstdarg",
    "cstddef",
    "cstdint",
    "cstdio",
    "cstdlib",
    "cstring",
    "ctime",
    "cuchar",
    "cwchar",
    "cwctype",
    "deque",
    "exception",
    "execution",
    "filesystem",
    "format",
    "forward_list",
    "fstream",
    "functional",
    "future",
    "initializer_list",
    "iomanip",
    "ios",
    "iosfwd",
    "iostream",
    "istream",
    "iterator",
    "limits",
    "list",
    "locale",
    "map",
    "memory",
    "memory_resource",
    "mutex",
    "new",
    "numeric",
    "optional",
    "ostream",
    "queue",
    "random",
    "ranges",
    "ratio",
    "regex",
    "scoped_allocator",
    "set",
    "shared_mutex",
    "source_location",
    "span",
    "sstream",
    "stack",
    "stdexcept",
    "stop_token",
    "streambuf",
    "string",
    "string_view",
    "strstream",
    "system_error",
    "thread",
    "tuple",
    "type_traits",
    "typeindex",
    "typeinfo",
    "unordered_map",
    "unordered_set",
    "utility",
    "valarray",
    "variant",
    "vector",
    "version",
}

STD_C = {
    "assert.h",
    "ctype.h",
    "errno.h",
    "fenv.h",
    "float.h",
    "inttypes.h",
    "iso646.h",
    "limits.h",
    "locale.h",
    "math.h",
    "setjmp.h",
    "signal.h",
    "stdalign.h",
    "stdarg.h",
    "stdbool.h",
    "stddef.h",
    "stdint.h",
    "stdio.h",
    "stdlib.h",
    "stdnoreturn.h",
    "string.h",
    "tgmath.h",
    "time.h",
    "uchar.h",
    "wchar.h",
    "wctype.h",
}

THIRD_PARTY_PREFIXES = (
    "gtest/",
    "gmock/",
    "nlohmann/",
    "sys/",
    "netinet/",
    "arpa/",
    "linux/",
    "asm/",
    "net/",
    "mach/",
)

THIRD_PARTY_EXACT = {
    "windows.h",
    "winsock2.h",
    "ws2tcpip.h",
    "winreg.h",
    "psapi.h",
    "iphlpapi.h",
    "tchar.h",
    "direct.h",
    "io.h",
    "process.h",
    "shlobj.h",
    "shlwapi.h",
    "dbghelp.h",
    "intrin.h",
    "immintrin.h",
    "x86intrin.h",
    "cpuid.h",
    "arm_neon.h",
    "unistd.h",
    "fcntl.h",
    "pthread.h",
    "dlfcn.h",
    "dirent.h",
    "poll.h",
    "fnmatch.h",
    "libgen.h",
    "pwd.h",
    "grp.h",
    "utime.h",
}

INCLUDE_RE = re.compile(
    r'^\s*#\s*include\s+(<([^>]+)>|"([^"]+)")(.*)$'
)

SKIP_DIR_NAMES = {"3rdparty", ".git", "build", "build-tests-asan", "build-notest", "build-examples"}


def classify(header: str, quoted: bool) -> int:
    if header.replace("\\", "/") == "lumex/LumexExport.hpp":
        return 1
    if quoted or header.startswith("lumex/"):
        return 4
    name = header.replace("\\", "/")
    if name in STD_CPP or name in STD_C:
        return 2
    if name in THIRD_PARTY_EXACT or name.startswith(THIRD_PARTY_PREFIXES):
        return 3
    # Unknown angle include: treat as third-party, not project.
    return 3


def iter_sources(root: Path) -> list[Path]:
    files: list[Path] = []
    for path in root.rglob("*"):
        if not path.is_file():
            continue
        if any(part in SKIP_DIR_NAMES for part in path.parts):
            continue
        if path.suffix.lower() not in {".c", ".cc", ".cpp", ".h", ".hpp"}:
            continue
        files.append(path)
    return sorted(files)


def extract_include_span(lines: list[str]) -> tuple[int, int] | None:
    """Return [start, end) of the first contiguous include region."""
    start = None
    i = 0
    n = len(lines)
    while i < n:
        stripped = lines[i].strip()
        if start is None:
            if INCLUDE_RE.match(lines[i]):
                start = i
                i += 1
                continue
            i += 1
            continue
        if (
            not stripped
            or stripped.startswith("//")
            or stripped.startswith("/*")
            or stripped.startswith("*")
            or stripped.startswith("#if")
            or stripped.startswith("#ifdef")
            or stripped.startswith("#ifndef")
            or stripped.startswith("#elif")
            or stripped.startswith("#else")
            or stripped.startswith("#endif")
            or INCLUDE_RE.match(lines[i])
        ):
            i += 1
            continue
        break
    if start is None:
        return None
    end = i
    while end > start and not lines[end - 1].strip():
        end -= 1
    return start, end


def includes_in_order(lines: list[str], start: int, end: int) -> list[tuple[int, int, str]]:
    found: list[tuple[int, int, str]] = []
    for idx in range(start, end):
        match = INCLUDE_RE.match(lines[idx])
        if not match:
            continue
        quoted = match.group(3) is not None
        header = match.group(3) if quoted else match.group(2)
        found.append((idx, classify(header, quoted), header))
    return found


def check_file(path: Path) -> list[str]:
    text = path.read_text(encoding="utf-8", errors="replace")
    lines = text.splitlines()
    span = extract_include_span(lines)
    if span is None:
        return []
    start, end = span
    found = includes_in_order(lines, start, end)
    errors: list[str] = []
    last_group = 0
    last_header = ""
    for _idx, group, header in found:
        if group < last_group:
            errors.append(
                f"{path}: include <{header}> is group {group} after group {last_group} ({last_header})"
            )
        last_group = group
        last_header = header
    return errors


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true", help="report violations")
    parser.add_argument("--fix", action="store_true", help="reserved; rewrite is manual")
    parser.add_argument(
        "--dir",
        default="lumex",
        help="root to scan (relative to cwd or absolute)",
    )
    args = parser.parse_args()
    if not args.check and not args.fix:
        parser.error("pass --check and/or --fix")

    root = Path(args.dir)
    if not root.is_absolute():
        root = Path.cwd() / root
    if not root.exists():
        print(f"directory not found: {root}", file=sys.stderr)
        return 2

    errors: list[str] = []
    for path in iter_sources(root):
        errors.extend(check_file(path))

    if args.fix:
        print("check_include_order.py --fix is not implemented; reorder by hand to the quality-gates groups.")

    if errors:
        for line in errors:
            print(line)
        print(f"{len(errors)} include-order violation(s)")
        return 1
    print("include order: 0 violations")
    return 0


if __name__ == "__main__":
    sys.exit(main())
