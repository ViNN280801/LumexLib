#!/usr/bin/env python3
"""List constructors and destructors that spell `constexpr` by hand.

A constructor or destructor takes its constexpr from a macro of
lumex/core/utility/macros/LumexKeywords.hpp, because the standard decides per
member whether the specifier may stand there: a destructor is constexpr from
C++20, a defaulted constructor only from C++23 in the macros' terms, a virtual
destructor only from C++23. A raw `constexpr` on such a member is an error on
the standards below that; `LUMEX_CONSTEXPR` and `LUMEX_CONSTEXPR_FUNCTION`
(the function macros) on one are the same mistake in a macro.

Detection (a token scan, not a regular expression over lines):
  1. Comments, string literals, character literals and raw string literals are
     blanked (newlines kept). A preprocessor directive is a segment of its own,
     so the `constexpr` that ends `#define LUMEX_CONSTEXPR_CTOR constexpr`
     does not borrow the first token of the next line.
  2. For every token `constexpr`, `consteval`, `LUMEX_CONSTEXPR`,
     `LUMEX_CONSTEXPR_FUNCTION`, `LUMEX_CONSTEXPR_VIRTUAL_FUNCTION` (and
     `LUMEX_CONSTEXPR_CXX14` on a destructor only) the
     scanner skips what may stand between it and the declarator: `explicit`
     (with its condition), `inline`, `__forceinline`, `virtual`, `friend`,
     `[[...]]`, `__attribute__ ((...))`,
     `__declspec (...)`, and any macro written in capitals (with its argument
     list, if it has one).
  3. A constructor or destructor is what follows: an optional `~`, a qualified
     name (template arguments skipped) and straight away `(`; the name must be
     a class declared in the file (`struct`, `class`, `union`) or the second
     `X` of `X::X`, except in the body of a macro, where the class name is a
     parameter. `constexpr int (max) ()` (a name in parentheses) and
     `constexpr Type (*p) (int)` are declarators. A function has
     a return type before its name, a variable has a type, a conversion
     operator has `operator`, `if constexpr (` has `(` at once, `decltype (`
     and `sizeof (` are keywords: none of them reaches the test.
A raw `constexpr` on a function, a variable, a lambda or a conversion operator
is therefore never reported.

Not scanned: lumex/tests, lumex/examples and any 3rdparty directory. A source
tree that is not finished yet lists its directories with --skip (relative to
the --dir parent, for example `lumex/core/string`); the CMake test keeps that
list, one line per module, and an entry goes when the module is done.

Usage (from LumexLib/):
  python Scripts/CodeTools/check_constexpr_special_members.py
  python Scripts/CodeTools/check_constexpr_special_members.py --check \\
      --skip lumex/core/string --skip lumex/core/span
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path
from typing import Iterator, List, Optional, Tuple

SUFFIXES = {".hpp", ".h", ".inl", ".cpp"}
SKIPPED_TOP_DIRS = {"tests", "examples"}
SKIPPED_DIR_NAMES = {"3rdparty"}

HEX_DIGITS = frozenset("0123456789abcdefABCDEF")
RAW = "constexpr"
FUNCTION_MACROS = {
    "LUMEX_CONSTEXPR",
    "LUMEX_CONSTEXPR_FUNCTION",
    "LUMEX_CONSTEXPR_VIRTUAL_FUNCTION",
}
PLAIN_SPECIFIERS = {"explicit", "inline", "virtual", "friend", "__forceinline",
                    "__inline", "__inline__"}
# Spellings that mark a constructor or destructor constexpr: the raw keyword
# (`consteval` too), the macros of functions, and the macro of the C++14 body
# on a destructor (a destructor is constexpr from C++20 only).
TRIGGERS = {RAW, "consteval"} | FUNCTION_MACROS | {"LUMEX_CONSTEXPR_CXX14"}
NOT_A_NAME = {"operator", "decltype", "sizeof", "alignof", "alignas", "typeof",
              "int", "char", "short", "long", "unsigned", "signed", "float",
              "double", "bool", "void", "wchar_t", "char8_t", "char16_t",
              "char32_t", "auto"}
MACRO_NAME = re.compile(r"^[A-Z][A-Z0-9]*(_[A-Z0-9]+)+$")
IDENTIFIER = re.compile(r"[A-Za-z_]\w*")
TOKEN = re.compile(r"[A-Za-z_]\w*|::|\[\[|\]\]|\S")


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


def is_digit_separator(text: str, i: int) -> bool:
    """True for the quote in 1'000 or 0x1'F, not in u8'a' or x'a'."""
    if text[i + 1:i + 2] not in HEX_DIGITS or text[i - 1:i] not in HEX_DIGITS:
        return False
    start = i
    while start > 0 and (text[start - 1].isalnum() or text[start - 1] in "_'."):
        start -= 1
    return text[start].isdigit() or (text[start] == "." and start + 1 < i)


def blank_comments_and_literals(text: str) -> str:
    """Replace comments and literals by spaces, keeping every newline."""
    out: List[str] = []
    i = 0
    size = len(text)

    def blank(chunk: str) -> str:
        return "".join(c if c == "\n" else " " for c in chunk)

    while i < size:
        c = text[i]
        two = text[i:i + 2]
        if two == "//":
            j = i
            while j < size and text[j] != "\n":
                # a backslash before the newline continues the comment
                if text[j] == "\\" and text[j + 1:j + 2] == "\n":
                    j += 1
                elif text[j] == "\\" and text[j + 1:j + 3] == "\r\n":
                    j += 2
                j += 1
            out.append(blank(text[i:j]))
            i = j
        elif two == "/*":
            j = text.find("*/", i + 2)
            j = size if j < 0 else j + 2
            out.append(blank(text[i:j]))
            i = j
        elif c == '"' and i > 0 and text[i - 1] == "R":
            # raw string: R"delim( ... )delim" (the R is already in out)
            m = re.match(r'"([^()\ ]{0,16})\(', text[i:i + 20])
            if m:
                closing = ")" + m.group(1) + '"'
                j = text.find(closing, i + m.end())
                j = size if j < 0 else j + len(closing)
            else:
                j = i + 1
            out.append(blank(text[i:j]))
            i = j
        elif c == '"' or c == "'":
            if c == "'" and is_digit_separator(text, i):
                # a digit separator (1'000) is not a literal
                out.append(c)
                i += 1
                continue
            j = i + 1
            while j < size and text[j] != c and text[j] != "\n":
                j += 2 if text[j] == "\\" else 1
            j = min(j + 1, size)
            out.append(blank(text[i:j]))
            i = j
        else:
            out.append(c)
            i += 1
    return "".join(out)


Token = Tuple[str, int]


def segments(text: str) -> List[List[Token]]:
    """Tokens of the code, and one more list per preprocessor directive."""
    code: List[Token] = []
    result: List[List[Token]] = [code]
    lines = text.split("\n")
    number = 0
    while number < len(lines):
        line = lines[number]
        first = number + 1
        if line.lstrip().startswith("#"):
            directive = line
            while directive.rstrip().endswith("\\") and number + 1 < len(lines):
                number += 1
                directive += "\n" + lines[number]
            tokens = [(m.group(0), first + directive.count("\n", 0, m.start()))
                      for m in TOKEN.finditer(directive)]
            if tokens and tokens[0][0] == "#":
                tokens = tokens[1:]
            result.append(tokens)
        else:
            code.extend((m.group(0), first) for m in TOKEN.finditer(line))
        number += 1
    return result


def skip_balanced(tokens: List[Token], i: int, open_: str, close: str) -> int:
    """tokens[i] is `open_`: the index after its matching `close`."""
    depth = 0
    while i < len(tokens):
        word = tokens[i][0]
        if word == open_:
            depth += 1
        elif word == close:
            depth -= 1
            if depth == 0:
                return i + 1
        i += 1
    return i


def class_names(segment_lists: List[List[Token]]) -> set:
    """Names declared after `struct`, `class` or `union` in the file."""
    names = set()
    for tokens in segment_lists:
        n = len(tokens)
        for i, (word, _) in enumerate(tokens):
            if word not in ("struct", "class", "union"):
                continue
            j = i + 1
            while j < n:
                if tokens[j][0] == "[[":
                    while j < n and tokens[j][0] != "]]":
                        j += 1
                    j += 1
                elif (MACRO_NAME.match(tokens[j][0]) or tokens[j][0] in (
                        "alignas", "__attribute__", "__declspec")):
                    j += 1
                    if j < n and tokens[j][0] == "(":
                        j = skip_balanced(tokens, j, "(", ")")
                else:
                    break
            if j < n and IDENTIFIER.fullmatch(tokens[j][0]):
                names.add(tokens[j][0])
    return names


def special_member_after(tokens: List[Token], i: int,
                         names: Optional[set]) -> Optional[str]:
    """`constructor`, `destructor` or None for the declarator after tokens[i].

    `names` are the classes declared in the file: the name of a constructor
    is one of them, or the second `X` of `X::X`. None (the body of a macro,
    whose class name is a parameter) takes any name.
    """
    n = len(tokens)
    while i < n:
        word = tokens[i][0]
        if word in PLAIN_SPECIFIERS:
            i += 1
            if word == "explicit" and i < n and tokens[i][0] == "(":
                i = skip_balanced(tokens, i, "(", ")")
        elif word == "[[":
            while i < n and tokens[i][0] != "]]":
                i += 1
            i += 1
        elif word in ("__attribute__", "__declspec", "alignas"):
            i += 1
            if i < n and tokens[i][0] == "(":
                i = skip_balanced(tokens, i, "(", ")")
        elif MACRO_NAME.match(word) and word != RAW.upper():
            i += 1
            if i < n and tokens[i][0] == "(":
                i = skip_balanced(tokens, i, "(", ")")
        else:
            break
    destructor = i < n and tokens[i][0] == "~"
    if destructor:
        i += 1
    parts: List[str] = []
    while i < n:
        word = tokens[i][0]
        if not IDENTIFIER.fullmatch(word) or word in NOT_A_NAME:
            return None
        parts.append(word)
        i += 1
        if i < n and tokens[i][0] == "<":
            i = skip_balanced(tokens, i, "<", ">")
        if i < n and tokens[i][0] == "::":
            i += 1
            if i < n and tokens[i][0] == "~":
                destructor = True
                i += 1
            continue
        break
    if not parts or i >= n or tokens[i][0] != "(":
        return None
    if i + 1 < n and tokens[i + 1][0] in ("*", "&", "&&"):
        # `constexpr Type (*pointer) (int)`: a declarator, not a member
        return None
    if (i + 3 < n and IDENTIFIER.fullmatch(tokens[i + 1][0])
            and tokens[i + 2][0] == ")" and tokens[i + 3][0] == "("):
        # `constexpr int (max) ()`: a function name in parentheses
        return None
    known = (names is None or parts[-1] in names
             or (len(parts) > 1 and parts[-1] == parts[-2]))
    if not known:
        return None
    return "destructor" if destructor else "constructor"


def findings(text: str) -> List[Tuple[int, str, str]]:
    """(line, spelling, kind) of every hand-written constexpr special member."""
    found: List[Tuple[int, str, str]] = []
    lists = segments(blank_comments_and_literals(text))
    names = class_names(lists)
    for number, tokens in enumerate(lists):
        for index, (word, line) in enumerate(tokens):
            if word not in TRIGGERS:
                continue
            # the first list is the code; a directive body may name a class
            # by a macro parameter
            kind = special_member_after(tokens, index + 1,
                                        names if number == 0 else None)
            if not kind:
                continue
            if word == "LUMEX_CONSTEXPR_CXX14" and kind != "destructor":
                continue
            found.append((line, word, kind))
    return found


def skipped(relative: str, skip: List[str]) -> bool:
    return any(relative == s or relative.startswith(s.rstrip("/") + "/")
               for s in skip)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.split("\n", 1)[0])
    parser.add_argument("--dir", type=Path, default=default_root())
    parser.add_argument("--check", action="store_true")
    parser.add_argument("--skip", action="append", default=[],
                        help="directory not done yet, relative to the "
                             "parent of --dir (lumex/core/string)")
    args = parser.parse_args()

    total = 0
    for path in iter_sources(args.dir):
        relative = path.relative_to(args.dir.parent).as_posix()
        if skipped(relative, args.skip):
            continue
        text = path.read_text(encoding="utf-8", errors="replace")
        for line, word, kind in findings(text):
            total += 1
            if word in (RAW, "consteval"):
                advice = ("use LUMEX_CONSTEXPR_CTOR, LUMEX_CONSTEXPR_DTOR or "
                          "the DEFAULTED / VIRTUAL form")
            elif word == "LUMEX_CONSTEXPR_CXX14":
                advice = ("a destructor is constexpr from C++20: use "
                          "LUMEX_CONSTEXPR_DTOR")
            else:
                advice = ("%s is the macro of a function: use the macro of "
                          "the special member" % word)
            print("%s:%d: %s on a %s: %s" % (relative, line, word, kind,
                                             advice))
    print("%d hand-written constexpr special member(s)" % total)
    return 1 if args.check and total else 0


if __name__ == "__main__":
    sys.exit(main())
