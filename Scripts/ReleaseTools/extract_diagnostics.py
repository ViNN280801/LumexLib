#!/usr/bin/env python3
"""Split the output of a compiler build into warnings and errors.

create_release.sh runs it after every build and keeps the result in
<output-dir>/warns/, so every diagnostic of every compiler and C++ standard
can be read at once.

usage: extract_diagnostics.py <build log> --warn-out FILE --err-out FILE
                              [--failed]

A diagnostic is the line "file:line:col: warning|error: text" together with
the lines that explain it (the "In file included from" chain before it, the
source line, the caret and the notes after it). A warning that -Werror turned
into an error ("[-Werror=...]" for GCC, "[-Werror,-W...]" for Clang) is still a
warning here: the build failed because of it, but it is the warning to fix.
The same diagnostic from a header included by many translation units is kept
once.

The files are written only when they have something in them. Standard output
carries one line, "warnings=<n> errors=<n>". A build that failed only because
-Werror turned warnings into errors has errors=0: the warnings are the news.
With --failed (the build did not succeed), no error and no promoted warning
found, the tail of the log is written to the error file, so a failure of the
build system itself is not lost.
"""

import argparse
import re
import sys

# file:line[:col]: kind: text
# The place is file:line[:col], or a pseudo file such as <built-in> or
# <command-line> (a redeclaration of a compiler builtin is reported there).
DIAGNOSTIC = re.compile(
    r"^(?P<where><[^>\s]+>|(?:[A-Za-z]:)?[^\s:<][^:]*:\d+(?::\d+)?):\s+"
    r"(?P<kind>warning|error|fatal error|note):\s+(?P<text>.*)$"
)
# collect2: error: ld returned 1 exit status / g++-8: error: unrecognized ...
TOOL_ERROR = re.compile(r"^(?P<where>[\w+.\-]+):\s+(?P<kind>fatal error|error):\s+(?P<text>.*)$")
# the linker's own message
LINK_ERROR = re.compile(r"(undefined reference to|multiple definition of|cannot find -l)")
PREFIX = re.compile(r"^(In file included from |\s+from |[^\s:][^:]*: In .*:$)")
# The progress and bookkeeping lines of Ninja ("[3/90]", "FAILED:", "ninja:") and of
# make ("[ 20%] Building", "make[2]: *** ... Error 1", "Scanning dependencies").
STATUS = re.compile(
    r"^(\[\d+/\d+\]|\[\s*\d+%\]|FAILED:|ninja:|-- |g?make(?:\[\d+\])?: "
    r"|Scanning dependencies of target |Consolidate compiler generated dependencies of target )"
)
# End of a block, nothing to keep: GCC's note that -Werror is why a build stopped,
# Clang's "1 warning generated." summaries.
NOISE = re.compile(
    r"^(?:[\w+.\-]+: all warnings being treated as errors"
    r"|\d+ (?:warnings?|errors?)(?: and \d+ (?:warnings?|errors?))? generated\.?)$"
)
PROMOTED = re.compile(r"\[-Werror(?:=|,)[^\]]*\]\s*$")


def classify(kind, text):
    """Return "warn", "promoted" (a warning -Werror made an error), "err" or
    "note" for a diagnostic."""
    if kind == "note":
        return "note"
    if kind == "warning":
        return "warn"
    return "promoted" if PROMOTED.search(text) else "err"


def parse(lines):
    """Yield (class, key, text) for every diagnostic block of the log."""
    prefix = []
    block = None  # [class, key, [lines]]
    for raw in lines:
        line = raw.rstrip("\n")
        if STATUS.match(line):
            if block:
                yield tuple(block)
                block = None
            prefix = []
            continue
        if NOISE.match(line):
            if block:
                yield tuple(block)
                block = None
            prefix = []
            continue
        match = DIAGNOSTIC.match(line)
        tool = None if match else TOOL_ERROR.match(line)
        if match and match.group("kind") != "note":
            if block:
                yield tuple(block)
            kind = classify(match.group("kind"), match.group("text"))
            block = [kind, (match.group("where"), match.group("text")), "\n".join(prefix + [line])]
            prefix = []
            continue
        if match:  # a note belongs to the diagnostic before it
            if block:
                block[2] += "\n" + line
            continue
        if tool:
            if block:
                yield tuple(block)
            block = ["err", (tool.group("where"), tool.group("text")), line]
            prefix = []
            continue
        if LINK_ERROR.search(line):
            if block:
                yield tuple(block)
                block = None
            yield ("err", ("link", line.strip()), line)
            continue
        if PREFIX.match(line):
            if block:
                yield tuple(block)
                block = None
            prefix.append(line)
            continue
        if block is not None:
            if line.strip() == "":
                yield tuple(block)
                block = None
            else:
                block[2] += "\n" + line
        else:
            prefix = []
    if block:
        yield tuple(block)


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n", 1)[0])
    parser.add_argument("log")
    parser.add_argument("--warn-out", required=True)
    parser.add_argument("--err-out", required=True)
    parser.add_argument("--failed", action="store_true")
    args = parser.parse_args()

    with open(args.log, encoding="utf-8", errors="replace") as handle:
        lines = handle.readlines()

    seen = set()
    warnings, errors = [], []
    has_promoted = False
    for cls, key, text in parse(lines):
        if (cls, key) in seen:
            continue
        seen.add((cls, key))
        has_promoted = has_promoted or cls == "promoted"
        (errors if cls == "err" else warnings).append(text)

    if args.failed and not errors and not has_promoted:
        errors.append("The build failed without a compiler diagnostic; the end of its log:\n" + "".join(lines[-40:]).rstrip("\n"))

    for path, items in ((args.warn_out, warnings), (args.err_out, errors)):
        if items:
            with open(path, "w", encoding="utf-8") as out:
                out.write("\n\n".join(items) + "\n")

    print("warnings=%d errors=%d" % (len(warnings), len(errors)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
