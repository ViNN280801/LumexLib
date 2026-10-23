#!/usr/bin/env python3
"""Check (and optionally extend) the platform macro list of the protocol
tables (lumex/core/protocol/dsl/LumexProtocolPlatformMacros.hpp).

A row identifier of a protocol table becomes a method name, so a macro of
a platform header with the same name breaks the table in the translation
units that include that header. The guard of the DSL checks every row
identifier against a list of such names on every platform. This script
keeps the Windows part of the list complete: it dumps the macros the
Windows SDK defines after <windows.h>, <winioctl.h>, <setupapi.h> and
<cfgmgr32.h> (clang-cl -E -dM, in a Visual Studio developer environment
for the SDK include path) and keeps the names a row identifier can be:

  - every name without an underscore (ERROR, DELETE, FAILED, the mixed-case
    A/W aliases such as GetMessage, min, max);
  - every function-like macro whose name does not start with an
    underscore (a function-like macro is not caught by the check of the
    expanded identifier, which sees only object-like macros).

Names that start with an underscore are reserved and never row
identifiers. The POSIX names already in the list stay. The SDK data is not
committed: the script reads the SDK of the machine it runs on.

Usage (from LumexLib/, in a developer shell):
  python Scripts/CodeTools/protocol_platform_macros.py --check \
      --compiler E:/local/LLVM/bin/clang-cl.exe
  python Scripts/CodeTools/protocol_platform_macros.py --write \
      --compiler E:/local/LLVM/bin/clang-cl.exe

--check exits 1 and prints the missing names when the header lacks one;
--write adds them (sorted by byte value, as the binary search needs);
format the header afterwards with Scripts/CodeTools/format.py.
"""
from __future__ import annotations

import argparse
import os
import re
import subprocess
import sys
import tempfile
from pathlib import Path

HEADERS = ("windows.h", "winioctl.h", "setupapi.h", "cfgmgr32.h")
HEADER = Path("lumex/core/protocol/dsl/LumexProtocolPlatformMacros.hpp")
DEFINE = re.compile(r"^#define ([A-Za-z_][A-Za-z0-9_]*)(\()?")
TABLE_START = "struct platform_macro_table"
NAME = re.compile(r'"([^"]*)"')


def dump_macros(compiler: str) -> list[tuple[str, bool]]:
    """(name, function-like) of every macro the SDK headers define."""
    with tempfile.TemporaryDirectory() as work:
        source = Path(work) / "sdk_macros.cpp"
        source.write_text("".join("#include <%s>\n" % h for h in HEADERS),
                          encoding="ascii")
        result = subprocess.run(
            [compiler, "/nologo", "/E", "-Xclang", "-dM", str(source)],
            stdout=subprocess.PIPE, stderr=subprocess.PIPE,
            universal_newlines=True, check=False)
    if result.returncode != 0:
        sys.stderr.write(result.stderr)
        sys.exit("protocol_platform_macros.py: the compiler failed (run it "
                 "in a Visual Studio developer shell)")
    macros = []
    for line in result.stdout.splitlines():
        match = DEFINE.match(line)
        if match:
            macros.append((match.group(1), match.group(2) is not None))
    if not macros:
        sys.exit("protocol_platform_macros.py: no macro dumped")
    return macros


def candidates(macros: list[tuple[str, bool]]) -> set[str]:
    names = set()
    for name, function_like in macros:
        if name.startswith("_"):
            continue
        if "_" not in name or function_like:
            names.add(name)
    return names


def table_span(text: str) -> tuple[int, int]:
    """The offsets of the `{ ... }` of the platform macro array."""
    start = text.index(TABLE_START)
    open_brace = text.index("= {", start) + 2
    close_brace = text.index("};", open_brace)
    return open_brace, close_brace + 1


def listed_names(text: str) -> list[str]:
    first, last = table_span(text)
    return NAME.findall(text[first:last])


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--check", action="store_true")
    mode.add_argument("--write", action="store_true")
    parser.add_argument("--compiler", default=os.environ.get(
        "LUMEX_CLANG_CL", "clang-cl"))
    parser.add_argument("--header", default=str(HEADER))
    args = parser.parse_args()

    header = Path(args.header)
    text = header.read_text(encoding="utf-8")
    listed = listed_names(text)
    wanted = candidates(dump_macros(args.compiler))
    missing = sorted(wanted - set(listed), key=lambda n: n.encode())
    print("listed %d, from the SDK %d, missing %d"
          % (len(listed), len(wanted), len(missing)))
    if args.check:
        for name in missing:
            print("  " + name)
        return 1 if missing else 0
    merged = sorted(set(listed) | wanted, key=lambda n: n.encode())
    first, last = table_span(text)
    body = "{ " + ",\n          ".join('"%s"' % n for n in merged) + " }"
    header.write_text(text[:first] + body + text[last:], encoding="utf-8",
                      newline="")
    print("wrote %d names to %s" % (len(merged), header))
    return 0


if __name__ == "__main__":
    sys.exit(main())
