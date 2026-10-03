#!/usr/bin/env python3
"""List the public LumexLib headers that have no Doxygen @file block.

A public header is a `.hpp` or `.h` file under lumex/, or a module umbrella
(a file without an extension, such as lumex/core/atomic/LumexAtomic), outside
lumex/tests, lumex/examples and any 3rdparty directory. Without an @file
block Doxygen documents none of the header's free functions, macros,
variables or typedefs, and the file list shows it without a description.

A header passes when a Doxygen comment (`/**`, `/*!`, `///` or `//!`) holds
`@file` or `\\file`, either without a name or with the header's own name.
An @file that names another file documents that file, not this one, so it
is reported too.

Usage (from LumexLib/):
  python Scripts/CodeTools/check_file_blocks.py            # list, exit 0
  python Scripts/CodeTools/check_file_blocks.py --check    # exit 1 if any
  python Scripts/CodeTools/check_file_blocks.py --dir <copy>/lumex --check
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path
from typing import Iterator, List, Optional

HEADER_SUFFIXES = {".hpp", ".h"}
SKIPPED_TOP_DIRS = {"tests", "examples"}
SKIPPED_DIR_NAMES = {"3rdparty"}

# Block comments that open with /** or /*! (but not the empty /**/), and runs
# of /// or //! line comments. Text in other comments is not documentation.
DOC_BLOCK = re.compile(r"/\*[*!](?!/)(.*?)\*/", re.DOTALL)
DOC_LINE = re.compile(r"^[ \t]*//[/!](.*)$", re.MULTILINE)
FILE_COMMAND = re.compile(r"[@\\]file(?![\w-])[ \t]*([^\s*]*)")


def default_root() -> Path:
    """Return the lumex/ directory of the checkout that holds this script."""
    return Path(__file__).resolve().parents[2] / "lumex"


def is_public_header(relative: Path) -> bool:
    """Tell whether a path relative to lumex/ names a public header."""
    if relative.parts[0] in SKIPPED_TOP_DIRS:
        return False
    if any(part in SKIPPED_DIR_NAMES for part in relative.parts[:-1]):
        return False
    name = relative.name
    if name.startswith("."):
        return False
    return relative.suffix in HEADER_SUFFIXES or "." not in name


def iter_public_headers(root: Path) -> Iterator[Path]:
    """Yield every public header under root, sorted by path."""
    for path in sorted(root.rglob("*")):
        if path.is_file() and is_public_header(path.relative_to(root)):
            yield path


def file_block_problem(path: Path) -> Optional[str]:
    """Return why the header has no @file block of its own, or None."""
    text = path.read_text(encoding="utf-8", errors="replace")
    comments = DOC_BLOCK.findall(text) + DOC_LINE.findall(text)
    names = [m.group(1) for c in comments for m in FILE_COMMAND.finditer(c)]
    if not names:
        return "no @file block"
    posix = path.as_posix()
    for name in names:
        if not name or name == path.name or posix.endswith("/" + name):
            return None
    return "@file names " + ", ".join(sorted(set(names)))


def find_problems(root: Path) -> List[str]:
    """Return one report line per public header under root that fails."""
    problems = []
    for path in iter_public_headers(root):
        problem = file_block_problem(path)
        if problem is not None:
            shown = path.relative_to(root.parent).as_posix()
            problems.append("{}: {}".format(shown, problem))
    return problems


def main() -> int:
    parser = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument(
        "--check",
        action="store_true",
        help="exit with status 1 when a public header has no @file block",
    )
    parser.add_argument(
        "--dir",
        type=Path,
        default=default_root(),
        help="lumex/ directory to scan (default: the one of this checkout)",
    )
    args = parser.parse_args()

    root = args.dir.resolve()
    if not root.is_dir():
        print("directory not found: {}".format(root), file=sys.stderr)
        return 2

    problems = find_problems(root)
    for line in problems:
        print(line)
    print("{} public header(s) without an @file block".format(len(problems)))
    return 1 if problems and args.check else 0


if __name__ == "__main__":
    sys.exit(main())
