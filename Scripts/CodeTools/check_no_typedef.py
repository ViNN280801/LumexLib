#!/usr/bin/env python3
"""List (and optionally convert) the C-style `typedef` declarations of LumexLib.

The convention (hub CLAUDE.md, "Type aliases", 28.10.2026): C++11 alias
declarations only, `using name = type;` and alias templates. A `typedef`
does not belong to any file of lumex/ or benchmarks/: not to sources, tests,
examples, DSL macros (a `typedef` inside the continuation lines of a
`#define` counts), Windows-only branches (the scanner does not evaluate the
preprocessor) or the fenced code blocks of Markdown files. Not scanned:
comments, string and character literals (raw strings too), any `3rdparty`
directory, the checker's own fixtures (`fixtures/no_typedef`) and the files
of the C-exception list below (code that must compile as C).

A file is scanned when it is a C or C++ source or header (`.hpp .h .hh .hxx
.cpp .cc .cxx .ipp .inl .tpp .c`), a header without an extension (a module
umbrella such as lumex/xml/LumexXml) or a Markdown file (`.md`, code blocks
only).

`--fix` rewrites the declarations in place:
  typedef T name;                  ->  using name = T;
  typedef T name[N];               ->  using name = T[N];
  typedef R (*name) (A);           ->  using name = R (*) (A);
  typedef T a, *b;                 ->  using a = T; using b = T *;
  typedef struct { ... } name;     ->  struct name { ... };
  typedef struct tag { ... } name; ->  struct tag { ... }; using name = tag;
It leaves a declaration it cannot convert with certainty (and says so on
stderr); the exit status is 1 when any such declaration remains.

Usage (from LumexLib/):
  python Scripts/CodeTools/check_no_typedef.py                   # list
  python Scripts/CodeTools/check_no_typedef.py --check           # exit 1 if any
  python Scripts/CodeTools/check_no_typedef.py --fix             # convert
  python Scripts/CodeTools/check_no_typedef.py --dir lumex --dir benchmarks
  python Scripts/CodeTools/check_no_typedef.py --check --dir <dir> \
      --c-exception <path shown in the report>
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path
from typing import Iterator, List, Optional, Sequence, Set, Tuple

SOURCE_SUFFIXES = {
    ".hpp", ".h", ".hh", ".hxx", ".cpp", ".cc", ".cxx", ".ipp", ".inl",
    ".tpp", ".c",
}
MARKDOWN_SUFFIXES = {".md"}
SKIPPED_DIR_NAMES = {"3rdparty", ".git"}
# `fixtures/no_typedef` holds the files this checker is tested with.
FIXTURE_PARTS = ("fixtures", "no_typedef")

# Files that must compile as C, as paths relative to the directory that holds
# lumex/ and benchmarks/. Empty: every header of the library is C++ (the
# C-facing functions sit in `extern "C"` blocks of C++ headers).
C_EXCEPTIONS: Set[str] = set()

# Words that may stand between a typedef's specifiers and its name without
# being the name.
CV_WORDS = {"const", "volatile", "noexcept", "throw", "restrict", "__restrict"}
ATTRIBUTE_WORDS = {"__attribute__", "__declspec", "alignas"}
CLASS_KEYS = {"struct", "class", "union", "enum"}
KEYWORDS = CLASS_KEYS | CV_WORDS | {
    "typedef", "typename", "template", "unsigned", "signed", "int", "char",
    "short", "long", "float", "double", "void", "bool", "auto", "decltype",
    "operator",
}

RAW_STRING = re.compile(r'(?:u8|u|U|L)?R"([^()\\\s]{0,16})\(')
TYPEDEF_WORD = re.compile(r"\btypedef\b")
TOKEN = re.compile(r"\w+(?:\s*##\s*\w+)*|::|\.\.\.|->|\S")
FENCE = re.compile(r"^([ \t]*)(`{3,}|~{3,})[ \t]*([\w+#.-]*)[^\n]*$", re.M)
CODE_LANGUAGES = {"", "cpp", "c++", "cxx", "hpp", "h", "c", "cc"}


class Unconvertible(Exception):
    """A typedef that --fix does not touch."""


# ---------------------------------------------------------------------------
# Scanning
# ---------------------------------------------------------------------------


def _blank(text: str) -> str:
    """Keep the newlines of text and turn every other character to a space."""
    return "".join(c if c == "\n" else " " for c in text)


def _is_word(c: str) -> bool:
    return c.isalnum() or c == "_"


def _in_number(code: str, quote: int) -> bool:
    """Tell whether the quote at index quote is a digit separator."""
    start = quote
    while start > 0 and (_is_word(code[start - 1]) or code[start - 1] in "'."):
        start -= 1
    return start < quote and code[start].isdigit()


def strip_code(text: str) -> str:
    """Return text with comments and literals blanked, same length and lines.

    A string or character literal keeps its quotes and loses its contents;
    preprocessor lines (macro bodies included) stay as they are.
    """
    out: List[str] = []
    i = 0
    size = len(text)
    while i < size:
        c = text[i]
        if text.startswith("//", i):
            end = i
            while end < size and text[end] != "\n":
                end += 2 if text.startswith("\\\n", end) else 1
            end = min(end, size)
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


def mask_markdown(text: str) -> str:
    """Blank everything of a Markdown text except its fenced C++ blocks."""
    out: List[str] = []
    pos = 0
    opening = None
    keep_from = 0
    for match in FENCE.finditer(text):
        if opening is None:
            opening = match
            keep_from = match.end()
            language = match.group(3).lower()
            if language not in CODE_LANGUAGES:
                opening = ("skip", match)
            continue
        is_skip = isinstance(opening, tuple)
        fence = opening[1] if is_skip else opening
        if (
            match.group(2)[0] == fence.group(2)[0]
            and len(match.group(2)) >= len(fence.group(2))
            and not match.group(3)
        ):
            out.append(_blank(text[pos:keep_from]))
            if is_skip:
                out.append(_blank(text[keep_from : match.start()]))
            else:
                out.append(text[keep_from : match.start()])
            pos = match.start()
            opening = None
    out.append(_blank(text[pos:]))
    return "".join(out)


def find_typedefs(code: str) -> List[int]:
    """Return the offsets of every `typedef` token of stripped code."""
    return [m.start() for m in TYPEDEF_WORD.finditer(code)]


def line_of(text: str, offset: int) -> int:
    return text.count("\n", 0, offset) + 1


def is_scanned(path: Path) -> bool:
    name = path.name
    if name.startswith("."):
        return False
    return (
        path.suffix in SOURCE_SUFFIXES
        or path.suffix in MARKDOWN_SUFFIXES
        or "." not in name
    )


def iter_files(root: Path) -> Iterator[Path]:
    for path in sorted(root.rglob("*")):
        if not path.is_file():
            continue
        relative = path.relative_to(root)
        parts = relative.parts
        if any(part in SKIPPED_DIR_NAMES for part in parts[:-1]):
            continue
        pair = list(zip(parts, parts[1:]))
        if FIXTURE_PARTS in pair:
            continue
        if is_scanned(path):
            yield path


def code_of(path: Path, text: str) -> str:
    """Return the stripped, scannable code of a file's text."""
    if path.suffix in MARKDOWN_SUFFIXES:
        return strip_code(mask_markdown(text))
    return strip_code(text)


# ---------------------------------------------------------------------------
# Conversion
# ---------------------------------------------------------------------------


class Tok:
    __slots__ = ("text", "start", "end")

    def __init__(self, text: str, start: int, end: int) -> None:
        self.text = text
        self.start = start
        self.end = end

    def is_ident(self) -> bool:
        return (
            self.text[0].isalpha() or self.text[0] == "_"
        ) and self.text not in KEYWORDS

    def __repr__(self) -> str:
        return "Tok({!r})".format(self.text)


def tokenize(code: str, begin: int, end: int) -> List[Tok]:
    return [
        Tok(m.group(0), m.start(), m.end())
        for m in TOKEN.finditer(code, begin, end)
        if m.group(0) != "\\"
    ]


OPEN = {"(": ")", "[": "]", "{": "}"}
CLOSE = {v: k for k, v in OPEN.items()}


def match_close(toks: Sequence[Tok], index: int) -> int:
    """Return the index of the bracket closing the one at toks[index]."""
    depth = 0
    for j in range(index, len(toks)):
        if toks[j].text in OPEN:
            depth += 1
        elif toks[j].text in CLOSE:
            depth -= 1
            if depth == 0:
                return j
    raise Unconvertible("unbalanced brackets")


def match_open(toks: Sequence[Tok], index: int) -> int:
    """Return the index of the bracket opening the one at toks[index]."""
    depth = 0
    for j in range(index, -1, -1):
        if toks[j].text in CLOSE:
            depth += 1
        elif toks[j].text in OPEN:
            depth -= 1
            if depth == 0:
                return j
    raise Unconvertible("unbalanced brackets")


def split_top_level(toks: List[Tok]) -> List[List[Tok]]:
    """Split a token list at the commas outside brackets and template <>."""
    parts: List[List[Tok]] = [[]]
    depth = 0
    angle = 0
    for k, tok in enumerate(toks):
        t = tok.text
        if t in OPEN or t in CLOSE:
            depth += 1 if t in OPEN else -1
        elif depth == 0 and t == "<" and k > 0 and (
            toks[k - 1].is_ident() or toks[k - 1].text in ("template", ">")
            or toks[k - 1].text == "::"
        ):
            angle += 1
        elif depth == 0 and t == ">" and angle > 0:
            angle -= 1
        elif depth == 0 and t == ">" and k + 1 < len(toks) and False:
            pass
        if t == "," and depth == 0 and angle == 0:
            parts.append([])
            continue
        parts[-1].append(tok)
    return parts


def declarator_name(toks: List[Tok]) -> Tuple[int, bool]:
    """Return (index of the declared name, name sits in a (*name) group)."""
    # Function pointer, reference or member pointer: `R (*name) (A)`.
    for i, tok in enumerate(toks):
        if tok.text != "(":
            continue
        close = match_close(toks, i)
        if close + 1 >= len(toks) or toks[close + 1].text not in ("(", "["):
            continue
        inner = toks[i + 1 : close]
        if not inner:
            continue
        has_ptr = any(t.text in ("*", "&", "&&") for t in inner)
        if not has_ptr:
            continue
        idents = [k for k, t in enumerate(inner) if t.is_ident()]
        last = inner[-1]
        if last.is_ident() and last.text not in KEYWORDS:
            return i + 1 + len(inner) - 1, True
        if idents:
            # `(*name[3])`, `(* name) const`: take the last identifier that is
            # not followed only by closing brackets.
            return i + 1 + idents[-1], True
        raise Unconvertible("abstract function pointer (no name)")
    # Ordinary declarator: skip trailing suffix groups, then take the name.
    i = len(toks) - 1
    while i >= 0:
        t = toks[i].text
        if t in (")", "]"):
            i = match_open(toks, i) - 1
            if i >= 0 and toks[i].text in ATTRIBUTE_WORDS:
                i -= 1
            continue
        if t in CV_WORDS or t in ("&", "&&"):
            i -= 1
            continue
        break
    if i < 0 or not toks[i].is_ident():
        raise Unconvertible("no declared name found")
    return i, False


def squeeze(text: str) -> str:
    """Collapse runs of blanks to one space and keep the line splices."""
    text = re.sub(r"[ \t]+", " ", text)
    text = re.sub(r" ?\\\n ?", " \\\n", text)
    return text.strip(" ")


def without_name(src: str, toks: List[Tok], name_index: int) -> str:
    """Return the source text of toks with the name token removed."""
    name = toks[name_index]
    return src[toks[0].start : name.start] + src[name.end : toks[-1].end]


def pointer_run_start(toks: List[Tok], name_index: int) -> int:
    """Index where the declarator (pointer operators) starts before a name."""
    run_start = name_index
    j = name_index - 1
    while j >= 0 and toks[j].text in ("*", "&", "&&", "const", "volatile",
                                      "__restrict", "restrict"):
        run_start = j
        j -= 1
    # cv words at the start of the run belong to the type.
    while run_start < name_index and toks[run_start].text in (
            "const", "volatile", "__restrict", "restrict"):
        run_start += 1
    return run_start


def convert_declaration(src: str, code: str, begin: int, end: int,
                        macro: bool, indent: str, terminator: str) -> str:
    """Return the replacement text for `typedef <src[begin:end]>;`."""
    toks = tokenize(code, begin, end)
    if not toks:
        raise Unconvertible("empty typedef")
    brace = None
    depth = 0
    for k, t in enumerate(toks):
        if t.text in "([":
            depth += 1
        elif t.text in ")]":
            depth -= 1
        elif t.text == "{" and depth == 0:
            brace = k
            break
    if brace is not None:
        return convert_with_body(src, toks, brace, macro, indent, terminator)
    parts = split_top_level(toks)
    if any(not part for part in parts):
        raise Unconvertible("empty declarator")
    first = parts[0]
    f_name, grouped = declarator_name(first)
    aliases: List[Tuple[str, str]] = []
    if grouped:
        if len(parts) > 1:
            raise Unconvertible("function pointer in a list of declarators")
        aliases.append(
            (first[f_name].text, squeeze(without_name(src, first, f_name))))
    else:
        run = pointer_run_start(first, f_name)
        if run == 0:
            raise Unconvertible("no type before the name")
        base = squeeze(src[first[0].start : first[run].start])
        for index, part in enumerate(parts):
            if index == 0:
                declarator = first[run:]
                name_i = f_name - run
            else:
                declarator = part
                name_i, again = declarator_name(part)
                if again:
                    raise Unconvertible("function pointer in a list")
            text = squeeze(base + " " + without_name(src, declarator, name_i))
            aliases.append((declarator[name_i].text, text))
    if not macro:
        aliases = [(n, re.sub(r"\s*\n\s*", " ", t).strip())
                   for n, t in aliases]
    return join_aliases(
        ["using {} = {}".format(n, t) for n, t in aliases], macro, indent,
        terminator)


def join_aliases(statements: List[str], macro: bool, indent: str,
                 terminator: str) -> str:
    """Join alias statements; the last one takes the original terminator."""
    glue = ";" + (" \\\n" + indent if macro else "\n" + indent)
    return glue.join(statements) + terminator


def convert_with_body(src: str, toks: List[Tok], brace: int, macro: bool,
                      indent: str, terminator: str) -> str:
    close = match_close(toks, brace)
    head = toks[:brace]
    names = split_top_level(toks[close + 1 :])
    if not head or head[0].text not in CLASS_KEYS:
        raise Unconvertible("typedef with a body that is not a class or enum")
    if len(names) != 1 or len(names[0]) != 1 or not names[0][0].is_ident():
        raise Unconvertible("class typedef with pointer or several names")
    alias = names[0][0].text
    body = src[toks[brace].start : toks[close].end]
    head_text = squeeze(src[head[0].start : head[-1].end])
    words = [t.text for t in head]
    colon = words.index(":") if ":" in words else len(words)
    before_colon = head[:colon]
    first_name = 2 if (head[0].text == "enum" and len(before_colon) > 1
                       and before_colon[1].text in ("class", "struct")) else 1
    tags = [t for t in before_colon[first_name:] if t.is_ident()]
    if tags:
        tag = tags[-1].text
        if tag == alias:
            return head_text + " " + body + terminator
        glue = ";" + (" \\\n" + indent if macro else "\n" + indent)
        return "{} {}{}using {} = {}{}".format(
            head_text, body, glue, alias, tag, terminator)
    insert_at = before_colon[first_name - 1].end - head[0].start
    new_head = head_text[:insert_at] + " " + alias + head_text[insert_at:]
    return new_head + " " + body + terminator


def statement_end(code: str, start: int, macro: bool) -> Tuple[int, str]:
    """Return (offset of the end, terminator) of the typedef at start.

    The end is the offset of the `;` (the terminator is ";") or, in a macro
    whose logical line ends first, the offset of the line end (terminator "").
    """
    depth = 0
    i = start
    size = len(code)
    while i < size:
        c = code[i]
        if c in "([{":
            depth += 1
        elif c in ")]}":
            depth -= 1
            if depth < 0:
                raise Unconvertible("statement not terminated")
        elif c == ";" and depth == 0:
            return i, ";"
        elif c == "\n" and macro and depth == 0 and code[i - 1] != "\\":
            return i, ""
        i += 1
    raise Unconvertible("statement not terminated")


def logical_line_start(text: str, offset: int) -> int:
    pos = text.rfind("\n", 0, offset)
    while pos > 0 and text[pos - 1] == "\\":
        pos = text.rfind("\n", 0, pos)
    return pos + 1


DEFINE_LINE = re.compile(r"[ \t]*#[ \t]*define\b")
LEADING_BLANKS = re.compile(r"[ \t]*")


def fix_text(text: str, code: str, path: str,
             problems: List[str]) -> Tuple[str, int]:
    """Convert every typedef of text; return (new text, number converted)."""
    done = 0
    for start in reversed(find_typedefs(code)):
        macro = bool(DEFINE_LINE.match(code, logical_line_start(code, start)))
        line_start = text.rfind("\n", 0, start) + 1
        indent = LEADING_BLANKS.match(text, line_start).group(0)
        if text[line_start:start].strip():
            indent += "    " if macro else ""
        try:
            end, term = statement_end(code, start + len("typedef"), macro)
            replacement = convert_declaration(
                text, code, start + len("typedef"), end, macro, indent, term)
        except Unconvertible as error:
            problems.append("{}:{}: not converted: {}".format(
                path, line_of(text, start), error))
            continue
        stop = end + 1 if term == ";" else end
        text = text[:start] + replacement + text[stop:]
        code = code[:start] + _blank(replacement) + code[stop:]
        done += 1
    return text, done


# ---------------------------------------------------------------------------
# Command line
# ---------------------------------------------------------------------------


def default_dirs() -> List[Path]:
    top = Path(__file__).resolve().parents[2]
    return [top / "lumex", top / "benchmarks"]


def read_text(path: Path) -> str:
    """Read a file without translating its line endings."""
    with open(str(path), encoding="utf-8", errors="surrogateescape",
              newline="") as handle:
        return handle.read()


def scan_dir(root: Path, exceptions: Set[str]) -> List[Tuple[Path, str, str]]:
    """Return (path, shown, text) of each scanned file with a typedef."""
    found = []
    for path in iter_files(root):
        shown = path.relative_to(root.parent).as_posix()
        if shown in exceptions:
            continue
        text = read_text(path)
        if "typedef" not in text:
            continue
        found.append((path, shown, text))
    return found


def main() -> int:
    parser = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument("--check", action="store_true",
                        help="exit with status 1 when a typedef is found")
    parser.add_argument("--fix", action="store_true",
                        help="convert the typedefs in place")
    parser.add_argument("--dir", type=Path, action="append", default=[],
                        help="directory to scan (repeatable; default: lumex "
                        "and benchmarks of this checkout)")
    parser.add_argument("--c-exception", action="append", default=[],
                        help="a reported path to skip, besides the built-in "
                        "list (code that must compile as C)")
    args = parser.parse_args()

    dirs = [d.resolve() for d in (args.dir or default_dirs())]
    for d in dirs:
        if not d.is_dir():
            print("directory not found: {}".format(d), file=sys.stderr)
            return 2
    exceptions = set(C_EXCEPTIONS) | set(args.c_exception)

    problems: List[str] = []
    count = 0
    converted = 0
    for root in dirs:
        for path, shown, text in scan_dir(root, exceptions):
            code = code_of(path, text)
            hits = find_typedefs(code)
            if not hits:
                continue
            if args.fix:
                new_text, done = fix_text(text, code, shown, problems)
                converted += done
                if new_text != text:
                    with open(str(path), "w", encoding="utf-8",
                              errors="surrogateescape", newline="") as handle:
                        handle.write(new_text)
                new_code = code_of(path, new_text)
                for offset in find_typedefs(new_code):
                    count += 1
                    print("{}:{}: typedef".format(
                        shown, line_of(new_text, offset)))
            else:
                for offset in hits:
                    count += 1
                    print("{}:{}: typedef".format(shown, line_of(text, offset)))
    for line in problems:
        print(line, file=sys.stderr)
    if args.fix:
        print("{} typedef(s) converted, {} left".format(converted, count))
    else:
        print("{} typedef(s)".format(count))
    return 1 if count and (args.check or args.fix) else 0


if __name__ == "__main__":
    sys.exit(main())
