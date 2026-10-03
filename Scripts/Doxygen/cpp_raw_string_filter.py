#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Doxygen input filter for C and C++ sources: raw string literals.

Doxygen 1.8 does not lex C++11 raw string literals. A quote inside one,
as in `R"(\"((?:[^\"\\]|\\.)*)\")"`, ends the string for Doxygen's
preprocessor, and everything after it is parsed as the wrong thing: a
namespace becomes anonymous, a class is lost, and its member definitions
in the .cpp file no longer match. This filter rewrites every raw string
literal into the ordinary string literal with the same value while
Doxygen reads the file, so the documentation shows the real initializer.
A raw string that spans lines is put on one line, followed by the line
breaks it had, so every line number stays the same.

Comments, ordinary strings and character literals are skipped, so text
that only looks like a raw string is left alone. The file on disk is not
changed.

Wired in the Doxyfile as FILTER_PATTERNS = *.hpp=<python> <this script>
(and *.h, *.cpp).

Usage (by Doxygen): cpp_raw_string_filter.py <source file>  -> filtered text
"""

import re
import sys

# The encoding prefix of a raw string literal, then R".
RAW_START = re.compile(r'(u8|u|U|L)?R"')
# A raw string delimiter: up to 16 characters other than space, parentheses,
# backslash and the control characters.
DELIMITER = re.compile(r"[^ ()\\\t\v\f\n]{0,16}")
IDENTIFIER_CHARACTER = re.compile(r"[A-Za-z0-9_]")


def escape(value):
    """The body of an ordinary string literal with this value."""
    return (
        value.replace("\\", "\\\\")
        .replace('"', '\\"')
        .replace("\n", "\\n")
        .replace("\r", "\\r")
        .replace("\t", "\\t")
    )


def raw_string_at(text, index):
    """(end, replacement) of a raw string literal that starts at index, or None."""
    match = RAW_START.match(text, index)
    if match is None:
        return None
    if index > 0 and IDENTIFIER_CHARACTER.match(text[index - 1]):
        return None  # part of an identifier, such as FOOR"..."
    opening = match.end()
    delimiter = DELIMITER.match(text, opening).group(0)
    body_start = opening + len(delimiter)
    if body_start >= len(text) or text[body_start] != "(":
        return None
    terminator = ")" + delimiter + '"'
    body_end = text.find(terminator, body_start + 1)
    if body_end < 0:
        return None
    value = text[body_start + 1 : body_end]
    prefix = match.group(1) or ""
    replacement = prefix + '"' + escape(value) + '"' + "\n" * value.count("\n")
    return body_end + len(terminator), replacement


def skip_quoted(text, index, quote):
    """The index after the ordinary string or character literal at index."""
    position = index + 1
    while position < len(text):
        character = text[position]
        if character == "\\":
            position += 2
            continue
        if character == quote or character == "\n":
            return position + 1
        position += 1
    return position


def filter_source(text):
    output = []
    index = 0
    length = len(text)
    while index < length:
        character = text[index]
        if text.startswith("//", index):
            end = text.find("\n", index)
            end = length if end < 0 else end
            output.append(text[index:end])
            index = end
            continue
        if text.startswith("/*", index):
            end = text.find("*/", index + 2)
            end = length if end < 0 else end + 2
            output.append(text[index:end])
            index = end
            continue
        if character in "uULR":
            raw = raw_string_at(text, index)
            if raw is not None:
                end, replacement = raw
                output.append(replacement)
                index = end
                continue
        if character in "\"'":
            end = skip_quoted(text, index, character)
            output.append(text[index:end])
            index = end
            continue
        output.append(character)
        index += 1
    return "".join(output)


def main(argv):
    if len(argv) != 2:
        sys.stderr.write("usage: cpp_raw_string_filter.py <source file>\n")
        return 2
    with open(
        argv[1], "r", encoding="utf-8", errors="surrogateescape", newline=""
    ) as source:
        text = source.read()
    sys.stdout.reconfigure(encoding="utf-8", errors="surrogateescape", newline="")
    sys.stdout.write(filter_source(text))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
