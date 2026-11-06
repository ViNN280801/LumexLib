#!/usr/bin/env python3
"""Runs _split_extra_cmake_args of compile.py without importing the module
(compile.py needs colorlog and psutil at import). usage: <compile.py> <os.pathsep>"""
import ast
import re
import sys
from typing import List, Optional

path, pathsep = sys.argv[1], sys.argv[2]
tree = ast.parse(open(path, encoding="utf-8").read())
func = [n for n in tree.body if isinstance(n, ast.FunctionDef) and n.name == "_split_extra_cmake_args"][0]
namespace = {"List": List, "Optional": Optional, "re_sub": re.sub, "os_pathsep": pathsep}
exec(compile(ast.Module(body=[func], type_ignores=[]), path, "exec"), namespace)
split = namespace["_split_extra_cmake_args"]

CASES = [
    (None, []),
    ("", []),
    ("-DA=1;-DB=2", ["-DA=1", "-DB=2"]),
    ("-DA=1:-DB=2", ["-DA=1", "-DB=2"]),
    ("'-DA=1';\"-DB=2\"", ["-DA=1", "-DB=2"]),
    # a drive path survives, with '/' and with '\'
    ("-DLUMEX_DOCUMENTATION_PREBUILT_DIR=D:/work/doc", ["-DLUMEX_DOCUMENTATION_PREBUILT_DIR=D:/work/doc"]),
    ("-DX=C:\\work\\doc;-DY=1", ["-DX=C:\\work\\doc", "-DY=1"]),
    ("-DX=D:/a;-DY=E:/b:-DZ=3", ["-DX=D:/a", "-DY=E:/b", "-DZ=3"]),
    ("-DP=C:/a;D:/b", ["-DP=C:/a", "D:/b"]),
    ("D:/dir", ["D:/dir"]),
    # a colon that is not a drive letter still separates
    ("-DA=/opt/a:/opt/b", ["-DA=/opt/a", "/opt/b"]),
    ("-DA=ab:/x", ["-DA=ab", "/x"]),
    ("-DA=1:-DB=C:/x", ["-DA=1", "-DB=C:/x"]),
]
failed = 0
for raw, want in CASES:
    got = split(raw)
    if got != want:
        failed += 1
        print("FAIL %r: got %r, want %r" % (raw, got, want))
print("%d cases, %d failed" % (len(CASES), failed))
sys.exit(1 if failed else 0)
