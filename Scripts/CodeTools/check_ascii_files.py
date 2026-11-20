#!/usr/bin/env python3
"""Fail when a file that must be ASCII has a byte of 0x80 or more.

The units catalog writes every symbol as UTF-8 escapes (`\\xD0\\x9F`) and every
comment as ASCII (`U+041F`), so that a translation unit compiles the same with
and without `/utf-8`, whatever the code page: cl.exe reports C4819 for a byte
that the code page of the source cannot represent (an error under /WX; code
pages 936, 932, 949, 950, 1253 and 1257 do it for Cyrillic and for signs such
as U+22C5).

Usage (from LumexLib/):
  python Scripts/CodeTools/check_ascii_files.py --check <file-or-directory>...
A directory is scanned for `.hpp`, `.h` and files without an extension (the
module umbrellas). Without `--check` the offending lines are only listed.
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path
from typing import Iterator, List, Tuple

SUFFIXES = {".hpp", ".h"}


def files_of(path: Path) -> Iterator[Path]:
    if path.is_file():
        yield path
        return
    for item in sorted(path.rglob("*")):
        if item.is_file() and (item.suffix in SUFFIXES or item.suffix == ""):
            yield item


def offences(path: Path) -> List[Tuple[int, int]]:
    """(line, column) of the first byte >= 0x80 on every line that has one."""
    found: List[Tuple[int, int]] = []
    for number, line in enumerate(path.read_bytes().split(b"\n"), start=1):
        for column, byte in enumerate(line, start=1):
            if byte >= 0x80:
                found.append((number, column))
                break
    return found


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--check", action="store_true", help="exit 1 if any")
    parser.add_argument("paths", nargs="+", type=Path)
    args = parser.parse_args()
    total = 0
    for root in args.paths:
        if not root.exists():
            print("no such path: %s" % root, file=sys.stderr)
            return 2
        for path in files_of(root):
            for line, column in offences(path):
                total += 1
                print("%s:%d:%d: a byte of 0x80 or more" % (path, line, column))
    print("%d offending line(s)" % total)
    return 1 if (args.check and total) else 0


if __name__ == "__main__":
    sys.exit(main())
