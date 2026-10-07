#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Runs the span benchmark executables of one or more build trees and merges
their results.

Every build tree is one toolchain (a compiler with its standard library) and
holds one executable per implementation and C++ standard, named
LumexSpanBench_<impl>_cxx<std> (see CMakeLists.txt). This script

  1. finds those executables in <build-dir>/bin,
  2. runs each of them --passes times, one pass over all executables after the
     other (so a slow moment of the machine hits every variant alike), pinned
     to one CPU when taskset exists,
  3. keeps, for a scenario, the lowest median of any pass (the best pass),
     the highest median of any pass (to show how much the processes differ),
     the smallest minimum and the largest maximum over the passes,
  4. checks that every implementation computed the same checksum for every
     scenario (a scenario with different checksums is an error: the numbers
     would compare different work),
  5. optionally measures the compile time of one translation unit per variant
     (--compile-time), and
  6. writes span_benchmark.csv (and compile_time.csv) and calls
     plot_results.py for the Markdown table and the SVG charts.

Why the best pass: the loops of the smallest scenarios (a fraction of a
nanosecond, a few instructions) run twice as fast or twice as slow in two
processes of the same executable, depending on where the stack and the code
land (address space randomization), so the median over processes would report
a random mix of the two. The lowest pass median is the speed the code can
reach, and the highest is printed next to it in the table when the processes
disagree by more than 25 %.

Standard library only.

Usage:
    python run_benchmark.py --build-dir gcc=build-bench-gcc \\
                            --build-dir clang=build-bench-clang
    python run_benchmark.py --build-dir this=build-bench --quick
    python run_benchmark.py --merge gcc/span_benchmark.csv \\
                            clang/span_benchmark.csv --out-dir results
    python run_benchmark.py --build-dir gcc=build-bench-gcc \\
        --compile-time --compiler "gcc=/opt/gcc-13.2.0/bin/g++" \\
        --compiler "clang=/opt/llvm-23.1.0/bin/clang++ -stdlib=libc++" \\
        --boost-include /opt/boost-1.92.0/include

Writes into --out-dir (default: results/ next to this script).
"""

import argparse
import csv
import glob
import os
import platform
import re
import shlex
import shutil
import statistics
import subprocess
import sys
import tempfile
import time

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, "..", ".."))
# Order of the variants in tables and charts.
VARIANT_ORDER = ["lumex_cxx11", "lumex_cxx20", "std_cxx20", "boost_cxx11",
                 "boost_cxx20"]
DEFAULT_BOOST = "/opt/boost-1.92.0/include"
IMPL_IDS = {"lumex": 1, "std": 2, "boost": 3}


def log(message):
    sys.stderr.write("run_benchmark.py: %s\n" % message)
    sys.stderr.flush()


def die(message):
    log("error: " + message)
    sys.exit(1)


def variant_of(path):
    match = re.match(r"LumexSpanBench_(\w+?)_cxx(\d+)$",
                     os.path.basename(path))
    if not match:
        return None
    return "%s_cxx%s" % (match.group(1), match.group(2))


def find_executables(build_dirs):
    """Returns [(toolchain, variant, path)] in a stable order."""
    found = []
    for toolchain, build_dir in build_dirs:
        paths = glob.glob(os.path.join(build_dir, "bin",
                                       "LumexSpanBench_*"))
        for path in paths:
            if not os.access(path, os.X_OK) or path.endswith(".csv"):
                continue
            variant = variant_of(path)
            if variant:
                found.append((toolchain, variant, path))
    names = [name for name, _ in build_dirs]

    def key(item):
        order = (VARIANT_ORDER.index(item[1]) if item[1] in VARIANT_ORDER
                 else len(VARIANT_ORDER))
        return (names.index(item[0]), order, item[1])
    found.sort(key=key)
    if not found:
        die("no LumexSpanBench_* executable in the build directories")
    return found


def parse_run(path):
    """Reads one CSV the executable wrote: (meta dict, [row dict])."""
    meta = {}
    rows = []
    with open(path, newline="") as handle:
        lines = handle.readlines()
    body = []
    for line in lines:
        if line.startswith("#"):
            for part in line[1:].split():
                if "=" in part:
                    key, value = part.split("=", 1)
                    meta[key] = value
            # the compiler name has a space ("GCC 13.2.0"): keep it whole
            compiler = re.search(r"compiler=(.*?) library=", line)
            if compiler:
                meta["compiler"] = compiler.group(1)
        else:
            body.append(line)
    for row in csv.DictReader(body):
        rows.append(row)
    return meta, rows


def run_executable(path, cpu, quick, scratch):
    out_csv = os.path.join(scratch, os.path.basename(path) + ".csv")
    command = [path, out_csv]
    if quick:
        command.append("--quick")
    if cpu is not None and shutil.which("taskset"):
        command = ["taskset", "-c", str(cpu)] + command
    result = subprocess.run(command, stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT,
                            universal_newlines=True)
    if result.returncode != 0:
        die("%s failed (%d):\n%s" % (path, result.returncode, result.stdout))
    return parse_run(out_csv)


def merge_passes(passes):
    """passes: list of row lists of the same executable -> merged rows."""
    merged = {}
    for rows in passes:
        for row in rows:
            key = (row["scenario"], row["size"])
            merged.setdefault(key, []).append(row)
    result = []
    for key in sorted(merged, key=lambda item: (
            [r["scenario"] for r in passes[0]].index(item[0]), int(item[1]))):
        samples = merged[key]
        first = samples[0]
        medians = [float(s["median_ns"]) for s in samples]
        result.append({
            "scenario": first["scenario"],
            "description": first["description"],
            "size": first["size"],
            "median_ns": min(medians),
            "worst_median_ns": max(medians),
            "min_ns": min(float(s["min_ns"]) for s in samples),
            "max_ns": max(float(s["max_ns"]) for s in samples),
            "iterations": first["iterations"],
            "repetitions": first["repetitions"],
            "checksum": first["checksum"],
        })
    return result


def check_checksums(table):
    """Same scenario, same checksum, in every variant of every toolchain."""
    by_scenario = {}
    for (toolchain, variant), rows in table.items():
        for row in rows:
            key = (row["scenario"], row["size"])
            by_scenario.setdefault(key, set()).add(row["checksum"])
    bad = [key for key, sums in by_scenario.items() if len(sums) != 1]
    if bad:
        die("different checksums (different work) in: %s"
            % ", ".join("%s/%s" % key for key in bad))


def cpu_model():
    try:
        with open("/proc/cpuinfo") as handle:
            for line in handle:
                if line.startswith("model name"):
                    return line.split(":", 1)[1].strip()
    except OSError:
        pass
    return platform.processor() or "unknown"


def machine_comment():
    return ("machine=\"%s\" os=\"%s %s\" logical_cpus=%d python=%s"
            % (cpu_model(), platform.system(), platform.release(),
               os.cpu_count() or 0, platform.python_version()))


def compiler_version(command):
    try:
        out = subprocess.run(shlex.split(command) + ["--version"],
                             stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                             universal_newlines=True).stdout
        return out.splitlines()[0].strip()
    except OSError:
        return "unknown"


def compile_times(compilers, boost_include, repeats, cpu):
    """Wall time (ms) of -fsyntax-only for compile_time.cpp per variant.

    The row `none` includes the same headers without any span: the cost of
    the translation unit itself."""
    rows = []
    source = os.path.join(HERE, "compile_time.cpp")
    for toolchain, command in compilers:
        base = shlex.split(command)
        variants = [("none", 0, "11"), ("none", 0, "20")]
        for impl in ("lumex", "std", "boost"):
            for std in ("11", "20"):
                if impl == "std" and std != "20":
                    continue
                variants.append((impl, IMPL_IDS[impl], std))
        for impl, impl_id, std in variants:
            flags = ["-std=c++" + std, "-fsyntax-only", "-O0",
                     "-DBENCH_SPAN_IMPL=%d" % impl_id, "-I" + REPO]
            if impl == "boost":
                if not os.path.exists(os.path.join(
                        boost_include, "boost", "core", "span.hpp")):
                    continue
                flags += ["-isystem", boost_include]
            command_line = base + flags + [source]
            if cpu is not None and shutil.which("taskset"):
                command_line = ["taskset", "-c", str(cpu)] + command_line
            # std::span needs <span>: a failed probe means "not available".
            samples = []
            failed = False
            for _ in range(repeats):
                start = time.perf_counter()
                result = subprocess.run(command_line, stdout=subprocess.PIPE,
                                        stderr=subprocess.STDOUT,
                                        universal_newlines=True)
                elapsed = (time.perf_counter() - start) * 1000.0
                if result.returncode != 0:
                    failed = True
                    break
                samples.append(elapsed)
            if failed:
                continue
            rows.append({
                "toolchain": toolchain,
                "variant": "none_cxx" + std if impl == "none"
                else "%s_cxx%s" % (impl, std),
                "median_ms": "%.1f" % statistics.median(samples),
                "min_ms": "%.1f" % min(samples),
                "repeats": repeats,
            })
    return rows


def merge_csvs(paths, out_path):
    """Combines the CSV files of several runs (one per toolchain) into one."""
    comments = []
    rows = []
    header = None
    for path in paths:
        with open(path, newline="") as handle:
            lines = handle.readlines()
        body = []
        for line in lines:
            if line.startswith("#"):
                comments.append(line[1:].strip())
            else:
                body.append(line)
        reader = csv.reader(body)
        file_header = next(reader)
        if header is None:
            header = file_header
        elif header != file_header:
            die("%s has other columns than the first file" % path)
        rows.extend(list(reader))
    with open(out_path, "w", newline="") as handle:
        seen = set()
        for comment in comments:
            if comment not in seen:
                seen.add(comment)
                handle.write("# %s\n" % comment)
        writer = csv.writer(handle, lineterminator="\n")
        writer.writerow(header)
        writer.writerows(rows)


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("--build-dir", action="append", default=[],
                        metavar="NAME=DIR",
                        help="a build tree (one toolchain) with bin/"
                             "LumexSpanBench_*; repeatable")
    parser.add_argument("--out-dir", default=os.path.join(HERE, "results"))
    parser.add_argument("--passes", type=int, default=5,
                        help="runs of every executable (default 5)")
    parser.add_argument("--quick", action="store_true",
                        help="smoke run: one pass, few repetitions")
    parser.add_argument("--cpu", type=int, default=None,
                        help="pin to this CPU with taskset when available")
    parser.add_argument("--compile-time", action="store_true",
                        help="also measure the compile time of one "
                             "translation unit per variant")
    parser.add_argument("--compiler", action="append", default=[],
                        metavar="NAME=COMMAND",
                        help="a compiler for --compile-time, with its "
                             "flags; repeatable")
    parser.add_argument("--boost-include", default=DEFAULT_BOOST,
                        help="directory with boost/core/span.hpp "
                             "(default %s)" % DEFAULT_BOOST)
    parser.add_argument("--compile-repeats", type=int, default=5)
    parser.add_argument("--no-plot", action="store_true")
    parser.add_argument("--merge", nargs="+", metavar="CSV", default=[],
                        help="do not run anything: combine the CSV files of "
                             "earlier runs (one per toolchain) into "
                             "--out-dir/span_benchmark.csv and plot")
    parser.add_argument("--compile-time-only", action="store_true",
                        help="only measure the compile time (needs "
                             "--compiler) and plot with the existing "
                             "span_benchmark.csv of --out-dir")
    args = parser.parse_args()

    csv_path = os.path.join(args.out_dir, "span_benchmark.csv")
    compile_path = os.path.join(args.out_dir, "compile_time.csv")
    if args.merge:
        os.makedirs(args.out_dir, exist_ok=True)
        merge_csvs(args.merge, csv_path)
        log("wrote " + csv_path)
        finish(args, csv_path, compile_path)
        return
    if args.compile_time_only:
        args.compile_time = True
        measure_compile_time(args, compile_path)
        finish(args, csv_path, compile_path)
        return
    if not args.build_dir:
        die("pass at least one --build-dir NAME=DIR")
    build_dirs = []
    for item in args.build_dir:
        if "=" not in item:
            die("--build-dir needs NAME=DIR, got %r" % item)
        name, directory = item.split("=", 1)
        build_dirs.append((name, os.path.abspath(directory)))

    executables = find_executables(build_dirs)
    passes = 1 if args.quick else args.passes
    os.makedirs(args.out_dir, exist_ok=True)
    log("%d executable(s), %d pass(es)" % (len(executables), passes))

    runs = {}
    meta = {}
    with tempfile.TemporaryDirectory() as scratch:
        for number in range(passes):
            for toolchain, variant, path in executables:
                log("pass %d/%d: %s %s" % (number + 1, passes, toolchain,
                                           variant))
                run_meta, rows = run_executable(path, args.cpu, args.quick,
                                                scratch)
                runs.setdefault((toolchain, variant), []).append(rows)
                meta[(toolchain, variant)] = run_meta
                if run_meta.get("build") != "release":
                    die("%s is not a release build (build=%s)"
                        % (path, run_meta.get("build")))

    table = {key: merge_passes(value) for key, value in runs.items()}
    check_checksums(table)

    with open(csv_path, "w", newline="") as handle:
        handle.write("# %s\n" % machine_comment())
        handle.write("# passes=%d cpu=%s\n"
                     % (passes, "any" if args.cpu is None else args.cpu))
        for toolchain, variant, path in executables:
            run_meta = meta[(toolchain, variant)]
            handle.write("# variant %s %s: compiler=\"%s\" library=\"%s\" "
                         "cplusplus=%s\n"
                         % (toolchain, variant, run_meta.get("compiler", "?"),
                            run_meta.get("library", "?"),
                            run_meta.get("cplusplus", "?")))
        writer = csv.writer(handle, lineterminator="\n")
        writer.writerow(["toolchain", "variant", "scenario", "description",
                         "size", "median_ns", "worst_median_ns", "min_ns",
                         "max_ns", "iterations", "repetitions", "checksum"])
        for toolchain, variant, path in executables:
            for row in table[(toolchain, variant)]:
                writer.writerow([toolchain, variant, row["scenario"],
                                 row["description"], row["size"],
                                 "%.4f" % row["median_ns"],
                                 "%.4f" % row["worst_median_ns"],
                                 "%.4f" % row["min_ns"],
                                 "%.4f" % row["max_ns"], row["iterations"],
                                 row["repetitions"], row["checksum"]])
    log("wrote " + csv_path)

    if args.compile_time:
        measure_compile_time(args, compile_path)
    finish(args, csv_path, compile_path)


def measure_compile_time(args, compile_path):
    compilers = []
    for item in args.compiler:
        if "=" not in item:
            die("--compiler needs NAME=COMMAND, got %r" % item)
        compilers.append(tuple(item.split("=", 1)))
    if not compilers:
        die("--compile-time needs at least one --compiler NAME=COMMAND")
    os.makedirs(args.out_dir, exist_ok=True)
    rows = compile_times(compilers, args.boost_include, args.compile_repeats,
                         args.cpu)
    with open(compile_path, "w", newline="") as handle:
        for name, command in compilers:
            handle.write("# compiler %s: %s\n"
                         % (name, compiler_version(command)))
        writer = csv.DictWriter(handle, lineterminator="\n", fieldnames=[
            "toolchain", "variant", "median_ms", "min_ms", "repeats"])
        writer.writeheader()
        writer.writerows(rows)
    log("wrote " + compile_path)


def finish(args, csv_path, compile_path):
    if args.no_plot:
        return
    plot = os.path.join(HERE, "plot_results.py")
    command = [sys.executable, plot, csv_path]
    if os.path.exists(compile_path):
        command.append(compile_path)
    subprocess.check_call(command)


if __name__ == "__main__":
    main()
