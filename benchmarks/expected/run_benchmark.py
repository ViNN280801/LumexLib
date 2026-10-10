#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Runs the expected benchmark executables of one or more build trees and
merges their results.

Every build tree is one toolchain (a compiler with its standard library) and
holds one executable per implementation and C++ standard, named
LumexExpectedBench_<impl>_cxx<std> (see CMakeLists.txt). This script

  1. finds those executables in <build-dir>/bin,
  2. runs each of them --passes times, one pass over all executables after the
     other (so a slow moment of the machine hits every variant alike), pinned
     to one CPU when taskset exists,
  3. keeps, for a scenario, the MEDIAN of the pass medians (each pass median
     is itself the median of 15 repetitions), the smallest and the largest
     pass median (the spread between processes), and the smallest minimum and
     the largest maximum of any repetition,
  4. checks that every implementation computed the same checksum for every
     scenario (a scenario with different checksums is an error: the numbers
     would compare different work),
  5. collects the sizes and properties of the types (sizes.csv),
  6. optionally measures the compile time of one translation unit per variant
     (--compile-time), and
  7. writes expected_benchmark.csv (sizes.csv, compile_time.csv) and calls
     plot_results.py for the Markdown tables and the SVG charts.

Standard library only (Python 3.7 or later).

Usage:
    python run_benchmark.py --build-dir gcc=build-bench
    python run_benchmark.py --build-dir this=build-bench --quick
    python run_benchmark.py --build-dir gcc=build-bench --cpu 3 \\
        --compile-time --compiler "gcc=/opt/gcc-13.2.0/bin/g++" \\
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
VARIANT_ORDER = ["std_cxx23", "lumex_cxx23", "lumex_cxx20", "lumex_cxx17",
                 "lumex_cxx14", "lumex_cxx11", "outcome_cxx17"]
DEFAULT_BOOST = "/opt/boost-1.92.0/include"
IMPL_IDS = {"none": 0, "lumex": 1, "std": 2, "outcome": 3}


def log(message):
    sys.stderr.write("run_benchmark.py: %s\n" % message)
    sys.stderr.flush()


def die(message):
    log("error: " + message)
    sys.exit(1)


def variant_of(path):
    match = re.match(r"LumexExpectedBench_(\w+?)_cxx(\d+)$",
                     os.path.basename(path))
    if not match:
        return None
    return "%s_cxx%s" % (match.group(1), match.group(2))


def find_executables(build_dirs):
    """Returns [(toolchain, variant, path)] in a stable order."""
    found = []
    for toolchain, build_dir in build_dirs:
        for path in glob.glob(os.path.join(build_dir, "bin",
                                           "LumexExpectedBench_*")):
            if not os.access(path, os.X_OK) or "." in os.path.basename(path):
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
        die("no LumexExpectedBench_* executable in the build directories")
    return found


def parse_run(path):
    """Reads one CSV the executable wrote: (meta dict, [row dict])."""
    meta = {}
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
    return meta, list(csv.DictReader(body))


def read_sizes(path):
    with open(path, newline="") as handle:
        return list(csv.DictReader(handle))


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
    meta, rows = parse_run(out_csv)
    sizes = read_sizes(out_csv[:-4] + ".sizes.csv")
    return meta, rows, sizes


def merge_passes(passes):
    """passes: list of row lists of the same executable -> merged rows."""
    merged = {}
    order = []
    for rows in passes:
        for row in rows:
            if row["scenario"] not in merged:
                order.append(row["scenario"])
            merged.setdefault(row["scenario"], []).append(row)
    result = []
    for name in order:
        samples = merged[name]
        first = samples[0]
        medians = [float(s["median_ns"]) for s in samples]
        result.append({
            "scenario": name,
            "group": first["group"],
            "description": first["description"],
            "median_ns": statistics.median(medians),
            "pass_min_ns": min(medians),
            "pass_max_ns": max(medians),
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
            by_scenario.setdefault(row["scenario"], {}).setdefault(
                row["checksum"], []).append("%s/%s" % (toolchain, variant))
    bad = [name for name, sums in by_scenario.items() if len(sums) != 1]
    if bad:
        details = []
        for name in bad:
            details.append("%s: %s" % (name, "; ".join(
                "%s -> %s" % (checksum, ",".join(who))
                for checksum, who in by_scenario[name].items())))
        die("different checksums (different work) in:\n  "
            + "\n  ".join(details))


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


# Variants of the compile-time unit: (impl, standard). `none` includes the
# standard headers only.
COMPILE_VARIANTS = [("none", "11"), ("none", "23"), ("lumex", "11"),
                    ("lumex", "14"), ("lumex", "17"), ("lumex", "20"),
                    ("lumex", "23"), ("std", "23"), ("outcome", "17")]
# (mode name, flags)
COMPILE_MODES = [("syntax_O0", ["-fsyntax-only", "-O0"]),
                 ("compile_O2", ["-c", "-O2", "-DNDEBUG", "-o", os.devnull])]


def compile_times(compilers, boost_include, repeats, cpu):
    """Wall time (ms) of compile_time.cpp per variant and mode."""
    rows = []
    source = os.path.join(HERE, "compile_time.cpp")
    for toolchain, command in compilers:
        base = shlex.split(command)
        for impl, std in COMPILE_VARIANTS:
            if impl == "outcome" and not os.path.exists(os.path.join(
                    boost_include, "boost", "outcome", "result.hpp")):
                continue
            for mode, mode_flags in COMPILE_MODES:
                flags = ["-std=c++" + std,
                         "-DBENCH_EXPECTED_IMPL=%d" % IMPL_IDS[impl],
                         "-I" + REPO, "-I" + HERE] + mode_flags
                if impl == "outcome":
                    flags += ["-isystem", boost_include]
                command_line = base + flags + [source]
                if cpu is not None and shutil.which("taskset"):
                    command_line = ["taskset", "-c", str(cpu)] + command_line
                samples = []
                failed = False
                for _ in range(repeats):
                    start = time.perf_counter()
                    result = subprocess.run(
                        command_line, stdout=subprocess.PIPE,
                        stderr=subprocess.STDOUT, universal_newlines=True)
                    elapsed = (time.perf_counter() - start) * 1000.0
                    if result.returncode != 0:
                        log("compile failed: %s\n%s" % (
                            " ".join(command_line), result.stdout[-2000:]))
                        failed = True
                        break
                    samples.append(elapsed)
                if failed:
                    continue
                rows.append({
                    "toolchain": toolchain,
                    "variant": "%s_cxx%s" % (impl, std),
                    "mode": mode,
                    "median_ms": "%.1f" % statistics.median(samples),
                    "min_ms": "%.1f" % min(samples),
                    "max_ms": "%.1f" % max(samples),
                    "repeats": repeats,
                })
    return rows


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
            "toolchain", "variant", "mode", "median_ms", "min_ms", "max_ms",
            "repeats"])
        writer.writeheader()
        writer.writerows(rows)
    log("wrote " + compile_path)


def finish(args, csv_path, sizes_path, compile_path):
    if args.no_plot:
        return
    plot = os.path.join(HERE, "plot_results.py")
    command = [sys.executable, plot, csv_path, sizes_path]
    if os.path.exists(compile_path):
        command.append(compile_path)
    subprocess.check_call(command)


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("--build-dir", action="append", default=[],
                        metavar="NAME=DIR",
                        help="a build tree (one toolchain) with bin/"
                             "LumexExpectedBench_*; repeatable")
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
                        help="directory with boost/outcome/result.hpp "
                             "(default %s)" % DEFAULT_BOOST)
    parser.add_argument("--compile-repeats", type=int, default=7)
    parser.add_argument("--no-plot", action="store_true")
    parser.add_argument("--compile-time-only", action="store_true",
                        help="only measure the compile time (needs "
                             "--compiler) and plot with the existing "
                             "CSV files of --out-dir")
    args = parser.parse_args()

    csv_path = os.path.join(args.out_dir, "expected_benchmark.csv")
    sizes_path = os.path.join(args.out_dir, "sizes.csv")
    compile_path = os.path.join(args.out_dir, "compile_time.csv")
    if args.compile_time_only:
        measure_compile_time(args, compile_path)
        finish(args, csv_path, sizes_path, compile_path)
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
    sizes = {}
    with tempfile.TemporaryDirectory() as scratch:
        for number in range(passes):
            for toolchain, variant, path in executables:
                log("pass %d/%d: %s %s" % (number + 1, passes, toolchain,
                                           variant))
                run_meta, rows, size_rows = run_executable(
                    path, args.cpu, args.quick, scratch)
                runs.setdefault((toolchain, variant), []).append(rows)
                meta[(toolchain, variant)] = run_meta
                sizes[(toolchain, variant)] = size_rows
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
        writer.writerow(["toolchain", "variant", "scenario", "group",
                         "description", "median_ns", "pass_min_ns",
                         "pass_max_ns", "min_ns", "max_ns", "iterations",
                         "repetitions", "checksum"])
        for toolchain, variant, path in executables:
            for row in table[(toolchain, variant)]:
                writer.writerow([toolchain, variant, row["scenario"],
                                 row["group"], row["description"],
                                 "%.4f" % row["median_ns"],
                                 "%.4f" % row["pass_min_ns"],
                                 "%.4f" % row["pass_max_ns"],
                                 "%.4f" % row["min_ns"],
                                 "%.4f" % row["max_ns"], row["iterations"],
                                 row["repetitions"], row["checksum"]])
    log("wrote " + csv_path)

    with open(sizes_path, "w", newline="") as handle:
        writer = csv.writer(handle, lineterminator="\n")
        writer.writerow(["toolchain", "variant", "type", "sizeof", "alignof",
                         "trivially_copyable", "trivially_destructible",
                         "standard_layout", "nothrow_move_constructible"])
        for toolchain, variant, path in executables:
            for row in sizes[(toolchain, variant)]:
                writer.writerow([toolchain, variant, row["type"],
                                 row["sizeof"], row["alignof"],
                                 row["trivially_copyable"],
                                 row["trivially_destructible"],
                                 row["standard_layout"],
                                 row["nothrow_move_constructible"]])
    log("wrote " + sizes_path)

    if args.compile_time:
        measure_compile_time(args, compile_path)
    finish(args, csv_path, sizes_path, compile_path)


if __name__ == "__main__":
    main()
