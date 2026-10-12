#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Measures the compile time of one translation unit per way of formatting.

compile_time.cpp is compiled with -fsyntax-only -O0 for LumexFormat (at C++11
and at C++20), std::format, std::ostringstream, std::snprintf and a stub that
has <string> and <iterator> only (the cost of the shared headers), in two
units: `include` (the header and one call) and `use` (what the scenarios of
bench_format.cpp format). The wall time of every compile is measured
--compile-repeats times and the median is kept.

Usage:
    python compile_time.py --compiler "gcc=/opt/gcc-13.2.0/bin/g++" \\
        --compiler "clang=/opt/llvm-23.1.0/bin/clang++ -stdlib=libc++" \\
        --cpu 3 --compile-repeats 7

Writes into --out-dir (default: results/ next to this script):
    compile_time.csv   one row per compiler, variant and unit
and calls plot_results.py for the chart and the table when
format_benchmark.csv is in --out-dir (--no-plot skips that).

Standard library only.
"""

import argparse
import csv
import os
import shlex
import shutil
import statistics
import subprocess
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, "..", ".."))
# (variant, implementation id of compile_time.cpp, standard)
VARIANTS = [("none_cxx20", 0, "20"), ("lumex_cxx11", 1, "11"),
            ("lumex_cxx20", 1, "20"), ("std_cxx20", 2, "20"),
            ("ostringstream_cxx20", 3, "20"), ("snprintf_cxx20", 4, "20")]
MODES = [("include", 0), ("use", 1)]


def log(message):
    sys.stderr.write("compile_time.py: %s\n" % message)
    sys.stderr.flush()


def die(message):
    log("error: " + message)
    sys.exit(1)


def compiler_version(command):
    try:
        out = subprocess.run(shlex.split(command) + ["--version"],
                             stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                             universal_newlines=True).stdout
        return out.splitlines()[0].strip()
    except OSError:
        return "unknown"


def compile_times(compilers, repeats, cpu):
    """Wall time (ms) of -fsyntax-only for compile_time.cpp per variant."""
    rows = []
    source = os.path.join(HERE, "compile_time.cpp")
    for toolchain, command in compilers:
        base = shlex.split(command)
        for variant, impl_id, std in VARIANTS:
            for mode_name, mode in MODES:
                flags = ["-std=c++" + std, "-fsyntax-only", "-O0",
                         "-DBENCH_FMT_IMPL=%d" % impl_id,
                         "-DBENCH_COMPILE_MODE=%d" % mode, "-I" + REPO]
                command_line = base + flags + [source]
                if cpu is not None and shutil.which("taskset"):
                    command_line = ["taskset", "-c", str(cpu)] + command_line
                samples = []
                failed = False
                for _ in range(repeats):
                    start = time.perf_counter()
                    result = subprocess.run(command_line,
                                            stdout=subprocess.PIPE,
                                            stderr=subprocess.STDOUT,
                                            universal_newlines=True)
                    elapsed = (time.perf_counter() - start) * 1000.0
                    if result.returncode != 0:
                        failed = True
                        sys.stderr.write(result.stdout)
                        break
                    samples.append(elapsed)
                if failed:
                    continue
                rows.append({
                    "toolchain": toolchain,
                    "variant": variant,
                    "mode": mode_name,
                    "median_ms": "%.1f" % statistics.median(samples),
                    "min_ms": "%.1f" % min(samples),
                    "repeats": repeats,
                })
    return rows


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("--compiler", action="append", default=[],
                        metavar="NAME=COMMAND",
                        help="a compiler, with its flags; repeatable")
    parser.add_argument("--out-dir", default=os.path.join(HERE, "results"))
    parser.add_argument("--cpu", type=int, default=None,
                        help="pin to this CPU with taskset when available")
    parser.add_argument("--compile-repeats", type=int, default=5)
    parser.add_argument("--no-plot", action="store_true")
    args = parser.parse_args()

    compilers = []
    for item in args.compiler:
        if "=" not in item:
            die("--compiler needs NAME=COMMAND, got %r" % item)
        compilers.append(tuple(item.split("=", 1)))
    if not compilers:
        die("pass at least one --compiler NAME=COMMAND")
    os.makedirs(args.out_dir, exist_ok=True)
    rows = compile_times(compilers, args.compile_repeats, args.cpu)
    compile_path = os.path.join(args.out_dir, "compile_time.csv")
    with open(compile_path, "w", newline="") as handle:
        for name, command in compilers:
            handle.write("# compiler %s: %s\n"
                         % (name, compiler_version(command)))
        writer = csv.DictWriter(handle, lineterminator="\n", fieldnames=[
            "toolchain", "variant", "mode", "median_ms", "min_ms", "repeats"])
        writer.writeheader()
        writer.writerows(rows)
    log("wrote " + compile_path)
    csv_path = os.path.join(args.out_dir, "format_benchmark.csv")
    if not args.no_plot and os.path.exists(csv_path):
        subprocess.check_call([sys.executable,
                               os.path.join(HERE, "plot_results.py"),
                               csv_path])


if __name__ == "__main__":
    main()
