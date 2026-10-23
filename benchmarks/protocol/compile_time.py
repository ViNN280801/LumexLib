#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Measures the compile time of one translation unit per size of a protocol
table.

compile_time.cpp is compiled syntax-only (-fsyntax-only -O0 for GCC and
Clang, /Zs for cl.exe and clang-cl) with BENCH_PROTOCOL_TABLE 0 (the module
headers only), 1 (a layout of 40 scaled fields), 2 (the layout and a protocol
of 100 rows) and 3 (as 2, plus the run-time entries and three client
methods), at C++11 (C++14 for the cl drivers, their lowest) and C++20. The
wall time of every compile is measured --repeats times; the median and the
minimum are kept. The cost of a table is its row minus row 0.

Usage:
    python compile_time.py --compiler "cl=cl" \
        --compiler "clang-cl=E:/local/LLVM/bin/clang-cl.exe" --repeats 7
    python compile_time.py --compiler "gcc=/opt/gcc-13.2.0/bin/g++"

Writes --out-dir/compile_time.csv (default: results/ next to this script).
Standard library only.
"""

import argparse
import csv
import os
import shlex
import statistics
import subprocess
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, "..", ".."))
TABLES = [0, 1, 2, 3]
STANDARDS = ["11", "20"]


def die(message):
    sys.stderr.write("compile_time.py: error: %s\n" % message)
    sys.exit(1)


def is_cl_driver(command):
    name = os.path.basename(shlex.split(command)[0]).lower()
    return name in ("cl", "cl.exe", "clang-cl", "clang-cl.exe")


def command_line(command, table, standard):
    source = os.path.join(HERE, "compile_time.cpp")
    base = shlex.split(command)
    if is_cl_driver(command):
        std = "c++14" if standard == "11" else "c++" + standard
        return base + ["/nologo", "/Zs", "/EHsc", "/Zc:__cplusplus",
                       "/std:" + std, "/I" + REPO,
                       "/DBENCH_PROTOCOL_TABLE=%d" % table, source]
    return base + ["-std=c++" + standard, "-fsyntax-only", "-O0",
                   "-I" + REPO, "-DBENCH_PROTOCOL_TABLE=%d" % table, source]


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--compiler", action="append", required=True,
                        help="name=command, repeatable")
    parser.add_argument("--repeats", type=int, default=5)
    parser.add_argument("--out-dir", default=os.path.join(HERE, "results"))
    args = parser.parse_args()

    rows = []
    for spec in args.compiler:
        if "=" not in spec:
            die("--compiler takes name=command, got %r" % spec)
        name, command = spec.split("=", 1)
        for standard in STANDARDS:
            for table in TABLES:
                line = command_line(command, table, standard)
                samples = []
                for _ in range(args.repeats):
                    start = time.perf_counter()
                    result = subprocess.run(line, stdout=subprocess.PIPE,
                                            stderr=subprocess.STDOUT,
                                            universal_newlines=True)
                    samples.append((time.perf_counter() - start) * 1000.0)
                    if result.returncode != 0:
                        die("%s failed:\n%s" % (" ".join(line),
                                                result.stdout))
                rows.append({"toolchain": name, "standard": standard,
                             "table": table,
                             "median_ms": "%.0f" % statistics.median(samples),
                             "min_ms": "%.0f" % min(samples),
                             "repeats": args.repeats})
                sys.stderr.write("%s c++%s table %d: %s ms\n"
                                 % (name, standard, table,
                                    rows[-1]["median_ms"]))
    os.makedirs(args.out_dir, exist_ok=True)
    path = os.path.join(args.out_dir, "compile_time.csv")
    with open(path, "w", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=list(rows[0].keys()),
                                lineterminator="\n")
        writer.writeheader()
        writer.writerows(rows)
    sys.stderr.write("wrote %s\n" % path)


if __name__ == "__main__":
    main()
