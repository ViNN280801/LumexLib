#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Doxygen input filter for Markdown files.

A README keeps repository-relative image links, for example
`![Chart](../../../benchmarks/fmt/results/format_benchmark.svg)`, so it
renders in the repository web view. Doxygen copies an image into the HTML
output only when the link is a bare file name found in IMAGE_PATH, so this
filter rewrites every relative image link to its file name while Doxygen
reads the file. Absolute URLs (http, https, data) are left alone.

Wired in the Doxyfile as FILTER_PATTERNS = *.md=<python> <this script>.

Usage (by Doxygen): markdown_image_filter.py <file.md>  -> filtered text
"""

import re
import sys

IMAGE_LINK = re.compile(r"(!\[[^\]]*\]\()([^)\s]+)((?:\s+\"[^\"]*\")?\))")


def rewrite(match):
    target = match.group(2)
    is_url = re.match(r"^[a-zA-Z][a-zA-Z0-9+.-]*:", target) is not None
    if is_url or target.startswith("/"):
        return match.group(0)
    name = target.replace("\\", "/").rsplit("/", 1)[-1]
    return match.group(1) + name + match.group(3)


def main(argv):
    if len(argv) != 2:
        sys.stderr.write(__doc__)
        return 2
    with open(argv[1], encoding="utf-8") as stream:
        text = stream.read()
    output = getattr(sys.stdout, "buffer", None)
    filtered = IMAGE_LINK.sub(rewrite, text)
    if output is not None:
        output.write(filtered.encode("utf-8"))
    else:
        sys.stdout.write(filtered)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
