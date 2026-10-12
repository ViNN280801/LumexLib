#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Check that every public source file of lumex/ starts with the MIT notice.

A public file is a `.hpp`, `.h` or `.cpp` file, or an extensionless module
umbrella, under `lumex/`, outside `lumex/tests/` and `lumex/examples/`. Its
first comment must be a plain C comment (`/*`, not the Doxygen `/**` or
`/*!`, which would copy the license onto every page of the documentation)
that carries the copyright line and `SPDX-License-Identifier: MIT`.

Usage: check_license_headers.py [--dir lumex] [--check]
Without --check the script only reports; with it, it exits 1 when a file
fails. It exits 2 when the directory does not exist.
"""

import argparse
import os
import re
import sys

SOURCE_SUFFIXES = (".hpp", ".h", ".cpp")
SKIPPED_TOP_DIRECTORIES = ("tests", "examples")
FIRST_COMMENT = re.compile(r"\A\s*(/\*.*?\*/)", re.DOTALL)


def is_public_file(relative_path):
    """True for the files the rule covers, given a path relative to lumex/."""
    parts = relative_path.split(os.sep)
    if parts[0] in SKIPPED_TOP_DIRECTORIES:
        return False
    name = parts[-1]
    if name.startswith(".") or name == "CMakeLists.txt":
        return False
    if name.endswith(SOURCE_SUFFIXES):
        return True
    return "." not in name


def problem_of(text):
    """None when the file starts with a valid notice, otherwise the reason."""
    match = FIRST_COMMENT.match(text)
    if match is None:
        return "does not start with a comment"
    comment = match.group(1)
    if comment.startswith("/**") or comment.startswith("/*!"):
        return "the license comment is a Doxygen comment (/** or /*!)"
    if "SPDX-License-Identifier: MIT" not in comment:
        return "the first comment has no SPDX-License-Identifier: MIT"
    if "Copyright (c)" not in comment:
        return "the first comment has no Copyright (c) line"
    return None


def find_problems(root):
    problems = []
    for directory, subdirectories, files in os.walk(root):
        subdirectories.sort()
        for name in sorted(files):
            path = os.path.join(directory, name)
            relative = os.path.relpath(path, root)
            if not is_public_file(relative):
                continue
            with open(path, "r", encoding="utf-8", errors="replace") as source:
                reason = problem_of(source.read())
            if reason is not None:
                problems.append((relative, reason))
    return problems


def main(argv):
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--dir", default="lumex", help="the lumex/ directory")
    parser.add_argument("--check", action="store_true", help="exit 1 when a file fails")
    arguments = parser.parse_args(argv)
    if not os.path.isdir(arguments.dir):
        sys.stderr.write("no such directory: {}\n".format(arguments.dir))
        return 2
    problems = find_problems(arguments.dir)
    for relative, reason in problems:
        print("{}: {}".format(os.path.join(arguments.dir, relative), reason))
    print("{} public file(s) without a valid license notice".format(len(problems)))
    return 1 if problems and arguments.check else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
