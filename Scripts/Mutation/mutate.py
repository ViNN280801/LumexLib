#!/usr/bin/env python3
"""Write the include overlay of one mutant (the tree is never edited).

A mutant is a list of text edits (file, old text, new text, how many times
the old text occurs) in `mutants.json`. This script copies the headers it
may mutate into an overlay directory that the compiler searches first
(`-iquote <overlay>`, so `#include "lumex/..."` finds the copy), applies the
edits of the chosen mutant to the copies and leaves every other copy equal to
the tree. The tree itself is not touched, so an interrupted run leaves
nothing to restore.

Usage:
  mutate.py --tree <repo root> --overlay <dir> [--mutants mutants.json] <id | none>

`none` writes the overlay of the unmutated tree (identical copies). Files whose
content is already right are not rewritten, so a build system that watches the
modification times rebuilds only what a change of mutant really changed.

Every directory that any mutant of `mutants.json` edits is mirrored in full
(all of its headers), so that the relative includes between sibling headers
stay inside the overlay and the set of watched files is the same for every
mutant (a build made with `none` records all of them as dependencies).

Exit status: 0 done; 2 a pattern was not found the stated number of times
(the mutant is stale against the tree) or the id is unknown.
"""

from __future__ import print_function

import argparse
import io
import json
import os
import sys

HEADER_SUFFIXES = (".hpp", ".h", ".inl")


def as_text(value):
    """A pattern is a string or a list of lines (joined with a newline)."""
    if isinstance(value, list):
        return "\n".join(value)
    return value


def read_text(path):
    with io.open(path, "r", encoding="utf-8", newline="") as stream:
        return stream.read()


def write_if_different(path, text):
    """Writes @p text unless the file already has it; returns True if written."""
    if os.path.exists(path) and read_text(path) == text:
        return False
    directory = os.path.dirname(path)
    if not os.path.isdir(directory):
        os.makedirs(directory)
    with io.open(path, "w", encoding="utf-8", newline="") as stream:
        stream.write(text)
    return True


def load_mutants(path):
    with io.open(path, "r", encoding="utf-8") as stream:
        return json.load(stream)


def mirrored_directories(data):
    """The directories (relative to the repo root) of every edited file."""
    result = set()
    for mutant in data["mutants"]:
        for edit in mutant.get("edits", []):
            result.add(os.path.dirname(edit["file"]))
    return sorted(result)


def apply_edits(mutant, texts):
    """Applies the edits of @p mutant to @p texts (dict file -> text)."""
    for number, edit in enumerate(mutant.get("edits", []), 1):
        old = as_text(edit["old"])
        new = as_text(edit["new"])
        expected = edit.get("count", 1)
        name = edit["file"]
        if name not in texts:
            raise KeyError("%s: edit %d: file %s is not mirrored" % (mutant["id"], number, name))
        found = texts[name].count(old)
        if found != expected:
            raise ValueError(
                "%s: edit %d: the pattern occurs %d times in %s, expected %d: %r"
                % (mutant["id"], number, found, name, expected, old[:90]))
        texts[name] = texts[name].replace(old, new)


def build_overlay(tree, overlay, data, mutant_id):
    texts = {}
    for directory in mirrored_directories(data):
        for entry in sorted(os.listdir(os.path.join(tree, directory))):
            if entry.endswith(HEADER_SUFFIXES):
                relative = directory + "/" + entry
                texts[relative] = read_text(os.path.join(tree, relative))
    originals = dict(texts)
    if mutant_id != "none":
        chosen = [m for m in data["mutants"] if m["id"] == mutant_id]
        if not chosen:
            raise KeyError("unknown mutant id %r" % mutant_id)
        apply_edits(chosen[0], texts)
    written = 0
    for relative in sorted(texts):
        if write_if_different(os.path.join(overlay, relative), texts[relative]):
            written += 1
    changed = sorted(name for name in texts if texts[name] != originals[name])
    return written, changed


def main(argv):
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("--tree", required=True, help="root of the LumexLib tree")
    parser.add_argument("--overlay", required=True, help="the overlay directory")
    parser.add_argument("--mutants", default=os.path.join(os.path.dirname(os.path.abspath(__file__)), "mutants.json"))
    parser.add_argument("mutant", help="a mutant id from mutants.json, or none")
    args = parser.parse_args(argv)
    data = load_mutants(args.mutants)
    try:
        written, changed = build_overlay(args.tree, args.overlay, data, args.mutant)
    except (KeyError, ValueError) as error:
        print("mutate.py: %s" % error.args[0], file=sys.stderr)
        return 2
    print("overlay %s: %d file(s) written, mutated: %s" % (args.mutant, written, ", ".join(changed) or "-"))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
