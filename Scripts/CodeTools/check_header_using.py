#!/usr/bin/env python3
"""List the using-directives at file scope of the public LumexLib headers.

A public header is a `.hpp` or `.h` file under lumex/, or a module umbrella
(a file without an extension, such as lumex/xml/LumexXml), outside
lumex/tests, lumex/examples and any 3rdparty directory.

A `using namespace X;` written outside every namespace block of a header
puts the names of X into the global namespace of every translation unit
that includes the header, where they can clash with the consumer's own
names. Inside a namespace block of the library the directive affects only
lookup in that namespace, so it is not reported. Neither is a directive in
a function body or in an `extern "C"` block nested in a namespace; an
`extern "C"` block at file scope does not open a scope, so a directive in
it is reported.

The scanner counts braces. It skips comments, string and character
literals (raw strings too) and preprocessor lines, and it tells a
namespace block from any other block by the text that precedes its
opening brace.

Usage (from LumexLib/):
  python Scripts/CodeTools/check_header_using.py            # list, exit 0
  python Scripts/CodeTools/check_header_using.py --check    # exit 1 if any
  python Scripts/CodeTools/check_header_using.py --dir <copy>/lumex --check
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path
from typing import Iterator, List, Tuple

HEADER_SUFFIXES = {".hpp", ".h"}
SKIPPED_TOP_DIRS = {"tests", "examples"}
SKIPPED_DIR_NAMES = {"3rdparty"}

# Kinds of an open brace.
NAMESPACE = "namespace"
LINKAGE = "linkage"
OTHER = "other"

# The statement text before a brace that opens a namespace block: named,
# nested (C++17), inline or unnamed, with optional attributes.
NAMESPACE_HEAD = re.compile(r"(?:^|[^\w:])namespace\b[^;{}()=]*$")
# `extern "C" {`: the scanner blanks the text of a literal, not its quotes.
LINKAGE_HEAD = re.compile(r'(?:^|\W)extern\s*"[^"]*"\s*$')
USING_DIRECTIVE = re.compile(r"\busing\s+namespace\s+([\w:\s]+?)\s*;")
RAW_STRING = re.compile(r'(?:u8|u|U|L)?R"([^()\\\s]{0,16})\(')


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


def _blank(text: str) -> str:
    """Keep the newlines of text and turn every other character to a space."""
    return "".join(c if c == "\n" else " " for c in text)


def _is_word(c: str) -> bool:
    """Tell whether c can be part of an identifier."""
    return c.isalnum() or c == "_"


def _in_token(c: str) -> bool:
    """Tell whether c can be part of a pp-number such as 0x1'FF."""
    return _is_word(c) or c in "'."


def _in_number(code: str, quote: int) -> bool:
    """Tell whether the quote at index quote is a digit separator."""
    start = quote
    while start > 0 and _in_token(code[start - 1]):
        start -= 1
    return start < quote and code[start].isdigit()


def strip_code(text: str) -> str:
    """Return text with comments, literals and preprocessor lines blanked.

    Line breaks stay where they were, so a match in the result has the same
    line number as in text. A string or character literal keeps its quotes
    and loses its contents.
    """
    out: List[str] = []
    i = 0
    size = len(text)
    line_start = True
    while i < size:
        c = text[i]
        if line_start and c in " \t":
            out.append(c)
            i += 1
            continue
        if line_start and c == "#":
            end = i
            while end < size and text[end] != "\n":
                if text.startswith("\\\n", end):
                    end += 2
                    continue
                if text.startswith("/*", end):
                    close = text.find("*/", end + 2)
                    end = size if close < 0 else close + 2
                    continue
                end += 1
            out.append(_blank(text[i:end]))
            i = end
            continue
        line_start = c == "\n"
        if text.startswith("//", i):
            end = text.find("\n", i)
            end = size if end < 0 else end
            out.append(_blank(text[i:end]))
            i = end
            continue
        if text.startswith("/*", i):
            close = text.find("*/", i + 2)
            end = size if close < 0 else close + 2
            out.append(_blank(text[i:end]))
            i = end
            continue
        raw = RAW_STRING.match(text, i)
        if raw and (i == 0 or not _is_word(text[i - 1])):
            close = text.find(")" + raw.group(1) + '"', raw.end())
            end = size if close < 0 else close + len(raw.group(1)) + 2
            out.append('""' + _blank(text[i + 2 : end]))
            i = end
            continue
        if c == "'" and _in_number(text, i):
            out.append(c)
            i += 1
            continue
        if c in "\"'":
            end = i + 1
            while end < size and text[end] != c and text[end] != "\n":
                end += 2 if text[end] == "\\" else 1
            end = min(end + 1, size)
            out.append(c + _blank(text[i + 1 : end - 1]) + c)
            i = end
            continue
        out.append(c)
        i += 1
    return "".join(out)


def file_scope_directives(text: str) -> List[Tuple[int, str]]:
    """Return (line, namespace) of every using-directive at file scope."""
    code = strip_code(text)
    stack: List[str] = []
    found: List[Tuple[int, str]] = []
    statement_start = 0
    for index, c in enumerate(code):
        if c == "{":
            head = code[statement_start:index]
            if NAMESPACE_HEAD.search(head):
                stack.append(NAMESPACE)
            elif LINKAGE_HEAD.search(head):
                stack.append(LINKAGE)
            else:
                stack.append(OTHER)
            statement_start = index + 1
        elif c == "}":
            if stack:
                stack.pop()
            statement_start = index + 1
        elif c == ";":
            at_file_scope = all(kind == LINKAGE for kind in stack)
            statement = code[statement_start : index + 1]
            match = USING_DIRECTIVE.search(statement)
            if at_file_scope and match:
                line = code.count("\n", 0, statement_start + match.start()) + 1
                found.append((line, " ".join(match.group(1).split())))
            statement_start = index + 1
    return found


def find_problems(root: Path) -> List[str]:
    """Return one report line per file-scope directive under root."""
    problems = []
    for path in iter_public_headers(root):
        text = path.read_text(encoding="utf-8", errors="replace")
        shown = path.relative_to(root.parent).as_posix()
        for line, name in file_scope_directives(text):
            report = "{}:{}: using namespace {}; at file scope"
            problems.append(report.format(shown, line, name))
    return problems


def main() -> int:
    parser = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument(
        "--check",
        action="store_true",
        help="exit with status 1 when a header has a file-scope directive",
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
    print("{} using-directive(s) at file scope".format(len(problems)))
    return 1 if problems and args.check else 0


if __name__ == "__main__":
    sys.exit(main())
