#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Runs LumexAtomicBenchmark many times and turns the runs into results.

One run of the benchmark program is one repetition: every implementation,
uncontended and at every thread count, each block next to its own
std::atomic<std::uint64_t> compare-exchange baseline. This script runs the
repetitions with a pause in between, checks that each implementation is the
one it claims to be, takes the medians across the repetitions and calls
plot_results.py. Standard library only.

The method is the author's, from the libc++ benchmark of llvm-project pull
request 194215: ratios to the baseline of the same process, same thread
count and same moment, never raw nanoseconds across processes; the median
of many repetitions; the implementations taking turns rather than running
as blocks; pauses between the runs so the processor cools down.

Usage:
    python run_benchmark.py --exe build-bench/bin/LumexAtomicBenchmark
    python run_benchmark.py --exe ... --quick
    python run_benchmark.py --exe ... --repetitions 100 --settle-seconds 30
    python run_benchmark.py --compile-time-only --cpu 3 \\
        --compiler "gcc=/opt/gcc-13.2.0/bin/g++" \\
        --compiler "clang=/opt/llvm-23.1.0/bin/clang++ -stdlib=libc++"

Writes into --out-dir (default: results/ next to this script):
    atomic_benchmark.csv   medians and quartiles per implementation, mode,
                           thread count and operation, with the machine
                           and the build in # comment lines
and, through plot_results.py, the Markdown table and the SVG charts.

--compile-time-only measures, instead of running the benchmark, the wall time
of -fsyntax-only -O0 of compile_time.cpp (the header of one atomic shared
pointer, included and used) per compiler and variant, writes compile_time.csv
next to the results and calls plot_results.py to draw
atomic_compile_time.svg.
"""

import argparse
import csv
import datetime
import os
import platform
import shlex
import shutil
import statistics
import subprocess
import sys
import tempfile
import time

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, "..", ".."))
DEFAULT_BOOST = "/opt/boost-1.92.0/include"
# The variants of the compile-time unit: (name, BENCH_ATOMIC_IMPL, standard).
# `none` is a std::shared_ptr behind a std::mutex, the cost of the shared
# headers.
COMPILE_VARIANTS = [("none_cxx11", 0, "11"), ("none_cxx20", 0, "20"),
                    ("lumex_cxx11", 1, "11"), ("lumex_cxx20", 1, "20"),
                    ("lumex_lock_based_cxx11", 2, "11"),
                    ("lumex_lock_based_cxx20", 2, "20"),
                    ("lumex_lock_free_cxx11", 3, "11"),
                    ("lumex_lock_free_cxx20", 3, "20"),
                    ("lumex_std_backed_cxx20", 4, "20"),
                    ("std_cxx20", 5, "20"), ("boost_cxx11", 6, "11"),
                    ("boost_cxx20", 6, "20")]
COMPILE_MODES = [("include", 0), ("use", 1)]
# What each series must have measured: the engine (the class template) and
# the way of sleeping (the inline namespace) are in the "path" of --list.
LOCK_BASED_PATHS = ("lock_based_std_wait", "lock_based_table_wait")
LOCK_FREE_PATHS = ("lock_free_std_wait", "lock_free_table_wait")
COMMON_PATHS = ("common_std_wait", "common_table_wait")
OPERATIONS = ("uint64_cas", "load", "store", "exchange",
              "compare_exchange_strong", "load_one_writer",
              "one_writer_stores", "load_store", "weak_load",
              "weak_load_lock", "destroy_median", "destroy_p99",
              "destroy_max", "destroy_backlog", "store_wait_timer",
              "store_wait_median", "store_wait_p90", "store_wait_p99",
              "store_wait_p999", "store_wait_p9999", "store_wait_max",
              "store_wait_over_1ms", "store_wait_switches",
              "store_wait_over_1ms_switched")
SCENARIOS = ("shared", "weak", "destroy", "store_wait")
# The split-count series (bench_lumex_family*.cpp): the key and what its
# "path" of --list must start with.
SPLIT_COUNT_PATHS = {
    "lumex_split_count_cxx11": "split_count_table_wait",
    "lumex_split_count": "split_count_std_wait",
    "lumex_split_count_r4": "split_count_r4_std_wait",
    "lumex_split_count_nocasres": "split_count_nocasres_std_wait",
    "lumex_lock_based_lumex_cxx11": "lock_based_lumex_table_wait",
    "lumex_lock_based_lumex": "lock_based_lumex_std_wait",
    "lumex_split_count_weak": "split_count_std_wait",
    "lumex_lock_based_lumex_weak": "lock_based_lumex_std_wait",
}


def log(message):
    sys.stderr.write("run_benchmark.py: %s\n" % message)
    sys.stderr.flush()


def die(message):
    log("error: " + message)
    sys.exit(1)


def list_implementations(exe, scenario="shared"):
    """Returns [(key, label, path, standard, lock_free)] from --list."""
    output = subprocess.run([exe, "--list", "--scenario", scenario],
                            check=True, stdout=subprocess.PIPE,
                            universal_newlines=True).stdout
    implementations = []
    for line in output.splitlines():
        if line.strip():
            key, label, path, standard, lock_free = line.split("|")[:5]
            implementations.append((key, label, path, standard, lock_free))
    return implementations


def check_split_count(implementations, series):
    """The split-count and own-family series must be the engines they name.

    `series` is the list of keys the run selects (empty: every series)."""
    by_key = {item[0]: item for item in implementations}
    for key, path in SPLIT_COUNT_PATHS.items():
        if series and key not in series:
            continue
        if key not in by_key:
            if key in series:
                die("the benchmark has no %s implementation" % key)
            continue
        if by_key[key][2] != path:
            die("%s measured %s, not %s" % (key, by_key[key][2], path))
        wants_lock_free = key.startswith("lumex_split_count")
        if (by_key[key][4] == "1") != wants_lock_free:
            die("%s reports is_lock_free () %s" % (key, by_key[key][4]))


def check_implementations(implementations):
    """The build must have measured what each key promises."""
    by_key = {item[0]: item for item in implementations}
    for key in ("lumex_lock_based_cxx11", "lumex_lock_based",
                "lumex_default"):
        if key not in by_key:
            die("the benchmark has no %s implementation" % key)
    if by_key["lumex_lock_based_cxx11"][2] != "lock_based_table_wait":
        die("lumex_lock_based_cxx11 measured %s, not lock_based_table_wait"
            % by_key["lumex_lock_based_cxx11"][2])
    if by_key["lumex_lock_based"][2] not in LOCK_BASED_PATHS:
        die("lumex_lock_based measured %s" % by_key["lumex_lock_based"][2])
    # The lock-free series exist only in a build that has the engine; each
    # one must be the engine it names, and the common name must be one of
    # the two engines (never the wrapper of the standard library's type).
    free = [key for key in by_key if key.startswith("lumex_lock_free")]
    for key in free:
        if not by_key[key][2].startswith("lock_free"):
            die("%s measured %s" % (key, by_key[key][2]))
        if by_key[key][4] != "1":
            die("%s reports is_lock_free () false" % key)
    if "lumex_lock_free_cxx11" in by_key and \
            by_key["lumex_lock_free_cxx11"][2] != "lock_free_table_wait":
        die("lumex_lock_free_cxx11 measured %s, not lock_free_table_wait"
            % by_key["lumex_lock_free_cxx11"][2])
    default_path = by_key["lumex_default"][2]
    if default_path not in COMMON_PATHS:
        die("lumex_default measured %s" % default_path)
    if free and by_key["lumex_default"][4] != "1":
        die("the build has the lock-free engine but the common name is not "
            "lock-free")
    if not free:
        log("the build has no lock-free engine; those series are left out")
    if "lumex_std_backed" not in by_key:
        log("no std::atomic<std::shared_ptr<T>> here; the std_backed series "
            "is left out")
    if "std" not in by_key:
        log("the standard library has no std::atomic<std::shared_ptr<T>>; "
            "the std series is left out")
    if "boost" not in by_key:
        log("Boost was not found at build time; the boost series is left out")


def read_file(path):
    try:
        with open(path, encoding="utf-8") as stream:
            return stream.read().strip()
    except OSError:
        return ""


def machine_info():
    """CPU, governor, kernel and load, as far as the platform tells."""
    info = {}
    cpu = ""
    for line in read_file("/proc/cpuinfo").splitlines():
        if line.startswith("model name"):
            cpu = line.split(":", 1)[1].strip()
            break
    info["cpu"] = cpu or platform.processor() or "unknown"
    info["logical_cpus"] = str(os.cpu_count() or 0)
    governors = set()
    for index in range(os.cpu_count() or 0):
        governor = read_file("/sys/devices/system/cpu/cpu%d/cpufreq/"
                             "scaling_governor" % index)
        if governor:
            governors.add(governor)
    info["governor"] = ",".join(sorted(governors)) or "unknown"
    info["kernel"] = platform.release()
    info["os"] = platform.platform()
    return info


def load_average():
    try:
        return "%.2f %.2f %.2f" % os.getloadavg()
    except (AttributeError, OSError):
        return "unknown"


def read_run(path):
    """Returns (header dict, rows) of one benchmark run."""
    header = {}
    lines = []
    with open(path, newline="", encoding="utf-8") as stream:
        for line in stream:
            if line.startswith("#"):
                key, _, value = line[1:].strip().partition("=")
                if key == "implementation":
                    header.setdefault("implementations", []).append(value)
                else:
                    header[key] = value
            else:
                lines.append(line)
    return header, list(csv.DictReader(lines))


def percentile(values, fraction):
    ordered = sorted(values)
    if len(ordered) == 1:
        return ordered[0]
    position = fraction * (len(ordered) - 1)
    low = int(position)
    high = min(low + 1, len(ordered) - 1)
    return ordered[low] + (ordered[high] - ordered[low]) * (position - low)


def aggregate(rows):
    """Medians and quartiles per (mode, threads, implementation, operation)."""
    groups = {}
    for row in rows:
        key = (row["mode"], int(row["threads"]), row["implementation"],
               row["operation"])
        groups.setdefault(key, []).append(row)
    result = []
    for key in sorted(groups, key=lambda k: (k[0] != "uncontended", k[1],
                                             k[2], OPERATIONS.index(k[3]))):
        group = groups[key]
        ratios = [float(r["ratio"]) for r in group]
        ns = [float(r["ns_per_op"]) for r in group]
        operations = [float(r["operations"]) for r in group]
        successes = [float(r["successes"]) for r in group]
        success_rate = (sum(successes) / sum(operations)
                        if sum(operations) else 0.0)
        extra = {}
        if "cycles_per_op" in group[0]:
            cycles = [float(r["cycles_per_op"]) for r in group]
            instructions = [float(r["instructions_per_op"]) for r in group]
            extra = {"cycles_median": statistics.median(cycles),
                     "instructions_median": statistics.median(instructions)}
        result.append({
            **extra,
            "mode": key[0], "threads": key[1], "implementation": key[2],
            "operation": key[3],
            "ratio_median": statistics.median(ratios),
            "ratio_p25": percentile(ratios, 0.25),
            "ratio_p75": percentile(ratios, 0.75),
            "ns_median": statistics.median(ns),
            "ns_p25": percentile(ns, 0.25),
            "ns_p75": percentile(ns, 0.75),
            "ns_min": min(ns), "ns_max": max(ns),
            "success_rate": success_rate,
            "operations_sum": int(sum(operations)),
            "successes_sum": int(sum(successes)),
            "repetitions": len(group),
            "valid": int(all(r["valid"] == "1" for r in group)),
        })
    return result


def write_results(path, header, rows):
    columns = ["mode", "threads", "implementation", "operation",
               "ratio_median", "ratio_p25", "ratio_p75", "ns_median",
               "ns_p25", "ns_p75", "ns_min", "ns_max", "success_rate",
               "repetitions", "valid"]
    if rows and "cycles_median" in rows[0]:
        columns += ["cycles_median", "instructions_median"]
    # The store-wait scenario sums the timed stores and the slow ones over
    # the repetitions; the other scenarios keep their columns as they were.
    if ("scenario", "store_wait") in header:
        columns += ["operations_sum", "successes_sum"]
    with open(path, "w", newline="", encoding="utf-8") as stream:
        for key, value in header:
            stream.write("# %s=%s\n" % (key, value))
        writer = csv.DictWriter(stream, fieldnames=columns,
                                lineterminator="\n", extrasaction="ignore")
        writer.writeheader()
        for row in rows:
            out = dict(row)
            for name in ("ratio_median", "ratio_p25", "ratio_p75"):
                out[name] = "%.4f" % row[name]
            for name in ("ns_median", "ns_p25", "ns_p75", "ns_min", "ns_max"):
                out[name] = "%.3f" % row[name]
            out["success_rate"] = "%.4f" % row["success_rate"]
            for name in ("cycles_median", "instructions_median"):
                if name in row:
                    out[name] = "%.1f" % row[name]
            writer.writerow(out)


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

    Two units per variant: `include` (the header is included and one object
    is declared) and `use` (the operations the benchmark measures). A variant
    the compiler cannot build (no std::atomic<std::shared_ptr> in a standard
    library, no Boost) is left out with the compiler's message."""
    rows = []
    source = os.path.join(HERE, "compile_time.cpp")
    for toolchain, command in compilers:
        base = shlex.split(command)
        for variant, impl_id, std in COMPILE_VARIANTS:
            for mode_name, mode in COMPILE_MODES:
                flags = ["-std=c++" + std, "-fsyntax-only", "-O0",
                         "-DBENCH_ATOMIC_IMPL=%d" % impl_id,
                         "-DBENCH_COMPILE_MODE=%d" % mode, "-I" + REPO]
                if 1 <= impl_id <= 4:
                    # what the CMake target lumex::atomic defines when the
                    # hazard pointer library is built: the lock-free engine
                    flags += ["-DLUMEX_ATOMIC_HAS_HAZARD_POINTER=1"]
                if impl_id == 6:
                    if not os.path.exists(os.path.join(
                            boost_include, "boost", "smart_ptr",
                            "atomic_shared_ptr.hpp")):
                        continue
                    flags += ["-isystem", boost_include]
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
                        log("%s %s %s does not compile:\n%s"
                            % (toolchain, variant, mode_name,
                               result.stdout[-600:]))
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


def measure_compile_time(args):
    compilers = []
    for item in args.compiler:
        if "=" not in item:
            die("--compiler needs NAME=COMMAND, got %r" % item)
        compilers.append(tuple(item.split("=", 1)))
    if not compilers:
        die("--compile-time-only needs at least one --compiler NAME=COMMAND")
    os.makedirs(args.out_dir, exist_ok=True)
    rows = compile_times(compilers, args.boost_include, args.compile_repeats,
                         args.cpu)
    path = os.path.join(args.out_dir, "compile_time.csv")
    with open(path, "w", newline="", encoding="utf-8") as stream:
        for name, command in compilers:
            stream.write("# compiler %s: %s\n"
                         % (name, compiler_version(command)))
        writer = csv.DictWriter(stream, lineterminator="\n", fieldnames=[
            "toolchain", "variant", "mode", "median_ms", "min_ms", "repeats"])
        writer.writeheader()
        writer.writerows(rows)
    log("written %s" % path)
    results = os.path.join(args.out_dir, "atomic_benchmark.csv")
    if not args.no_plot and os.path.exists(results):
        subprocess.run([sys.executable, os.path.join(HERE, "plot_results.py"),
                        results], check=True)


def parse_args(argv):
    parser = argparse.ArgumentParser(
        description="Runs LumexAtomicBenchmark and aggregates the runs.")
    parser.add_argument("--exe", default=None,
                        help="the LumexAtomicBenchmark executable")
    parser.add_argument("--out-dir", default=os.path.join(HERE, "results"),
                        help="where atomic_benchmark.csv and the charts go")
    parser.add_argument("--name", default="atomic_benchmark",
                        help="stem of the result files (default "
                             "atomic_benchmark)")
    parser.add_argument("--scenario", default="shared", choices=SCENARIOS,
                        help="shared: the operations on the atomic shared "
                             "pointers; weak: load and lock () on the "
                             "atomic weak pointers; destroy: the latency "
                             "from a replacing store to the destructor; "
                             "store_wait: the time of every single store "
                             "of one writer under a read load "
                             "(default shared)")
    parser.add_argument("--objects", type=int, default=20000,
                        help="scenario destroy: replacements per "
                             "measurement (default 20000)")
    parser.add_argument("--stores", type=int, default=100000,
                        help="scenario store_wait: timed stores per "
                             "measurement (default 100000)")
    parser.add_argument("--pin", action="store_true",
                        help="scenario store_wait: pin every thread to a "
                             "CPU of its own")
    parser.add_argument("--raw-csv", default=None,
                        help="also keep every row of every run here")
    parser.add_argument("--repetitions", type=int, default=100,
                        help="runs of the program (default 100)")
    parser.add_argument("--settle-seconds", type=float, default=30.0,
                        help="pause before every run but the first "
                             "(default 30)")
    parser.add_argument("--duration-ms", type=int, default=100,
                        help="timed window of one measurement (default 100)")
    parser.add_argument("--warmup-ms", type=int, default=300,
                        help="all-thread spin before each run (default 300)")
    parser.add_argument("--threads", default=None,
                        help="thread counts, e.g. 1,2,4,8 (default: 1 and "
                             "every even count up to the logical CPUs)")
    parser.add_argument("--series", default=None,
                        help="only these series (comma separated keys of "
                             "--list), e.g. lumex_lock_free,"
                             "lumex_lock_free_deferred")
    parser.add_argument("--quick", action="store_true",
                        help="3 repetitions, no pause, 20 ms windows: a "
                             "smoke run, not results")
    parser.add_argument("--no-plot", action="store_true",
                        help="skip plot_results.py")
    parser.add_argument("--compile-time-only", action="store_true",
                        help="do not run the benchmark: measure the compile "
                             "time of compile_time.cpp (needs --compiler)")
    parser.add_argument("--compiler", action="append", default=[],
                        metavar="NAME=COMMAND",
                        help="a compiler for --compile-time-only, with its "
                             "flags; repeatable")
    parser.add_argument("--boost-include", default=DEFAULT_BOOST,
                        help="directory with boost/smart_ptr/"
                             "atomic_shared_ptr.hpp (default %s)"
                             % DEFAULT_BOOST)
    parser.add_argument("--compile-repeats", type=int, default=5,
                        help="compiles per cell (default 5)")
    parser.add_argument("--cpu", type=int, default=None,
                        help="pin the compiles to this CPU with taskset")
    args = parser.parse_args(argv)
    if not args.compile_time_only and not args.exe:
        parser.error("--exe is required unless --compile-time-only is given")
    if args.quick:
        args.repetitions = 3
        args.settle_seconds = 0.0
        args.duration_ms = 20
        args.warmup_ms = 50
    if args.repetitions < 1 or args.settle_seconds < 0 or args.duration_ms < 1:
        parser.error("repetitions and duration must be positive, settle "
                     "not negative")
    return args


def main(argv):
    args = parse_args(argv)
    if args.compile_time_only:
        measure_compile_time(args)
        return 0
    exe = os.path.abspath(args.exe)
    if not os.path.isfile(exe):
        die("no executable %s" % exe)
    implementations = list_implementations(exe, args.scenario)
    series = args.series.split(",") if args.series else []
    if args.scenario == "weak":
        check_split_count(implementations, series)
    elif args.scenario == "store_wait":
        check_implementations(list_implementations(exe, "shared"))
        check_split_count(list_implementations(exe, "shared"), series)
        if not implementations:
            die("no series for the store_wait scenario")
    else:
        implementations_all = list_implementations(exe, "shared")
        check_implementations(implementations_all)
        check_split_count(implementations_all, series)
    os.makedirs(args.out_dir, exist_ok=True)
    machine = machine_info()
    load_start = load_average()
    started = datetime.datetime.now()
    log("%d repetitions, %.0f s pauses, %d ms windows; %s, %s logical CPUs, "
        "governor %s" % (args.repetitions, args.settle_seconds,
                         args.duration_ms, machine["cpu"],
                         machine["logical_cpus"], machine["governor"]))

    rows = []
    run_header = {}
    clock = time.monotonic()
    with tempfile.TemporaryDirectory(prefix="lumex_atomic_bench_") as scratch:
        for repetition in range(args.repetitions):
            if repetition > 0 and args.settle_seconds > 0:
                time.sleep(args.settle_seconds)
            path = os.path.join(scratch, "run_%d.csv" % repetition)
            command = [exe, "--csv", path, "--repetition", str(repetition),
                       "--duration-ms", str(args.duration_ms),
                       "--warmup-ms", str(args.warmup_ms),
                       "--scenario", args.scenario,
                       "--objects", str(args.objects)]
            if args.scenario == "store_wait":
                command += ["--stores", str(args.stores)]
                if args.pin:
                    command += ["--pin"]
            if args.threads:
                command += ["--threads", args.threads]
            if args.series:
                command += ["--series", args.series]
            completed = subprocess.run(command, stderr=subprocess.DEVNULL)
            if completed.returncode == 3:
                die("repetition %d: a measured object left a reference "
                    "behind (see %s)" % (repetition, path))
            if completed.returncode != 0:
                die("repetition %d: the benchmark exited with %d"
                    % (repetition, completed.returncode))
            run_header, run_rows = read_run(path)
            rows.extend(run_rows)
            elapsed = time.monotonic() - clock
            remaining = elapsed / (repetition + 1) * (args.repetitions
                                                      - repetition - 1)
            log("repetition %d/%d done, %.0f min left"
                % (repetition + 1, args.repetitions, remaining / 60))

    if args.raw_csv:
        with open(args.raw_csv, "w", newline="", encoding="utf-8") as stream:
            writer = csv.DictWriter(stream, fieldnames=list(rows[0].keys()),
                                    lineterminator="\n")
            writer.writeheader()
            writer.writerows(rows)

    header = [("tool", "LumexAtomicBenchmark"),
              ("scenario", args.scenario),
              ("objects", str(args.objects)),
              ("date", started.strftime("%Y-%m-%d")),
              ("cpu", machine["cpu"]),
              ("logical_cpus", machine["logical_cpus"]),
              ("governor", machine["governor"]),
              ("kernel", machine["kernel"]),
              ("os", machine["os"]),
              ("compiler", run_header.get("compiler", "unknown")),
              ("library", run_header.get("library", "unknown")),
              ("build", run_header.get("build", "unknown")),
              ("repetitions", str(args.repetitions)),
              ("settle_seconds", "%g" % args.settle_seconds),
              ("duration_ms", str(args.duration_ms)),
              ("warmup_ms", str(args.warmup_ms)),
              ("load_average_start", load_start),
              ("load_average_end", load_average()),
              ("minutes", "%.1f" % ((time.monotonic() - clock) / 60))]
    if args.scenario == "store_wait":
        header += [("stores", str(args.stores)),
                   ("pinned", "1" if args.pin else "0"),
                   ("clock", "std::chrono::steady_clock")]
    header += [("implementation", value)
               for value in run_header.get("implementations", [])]
    results = os.path.join(args.out_dir, args.name + ".csv")
    write_results(results, header, aggregate(rows))
    log("written %s" % results)
    if not args.no_plot:
        subprocess.run([sys.executable, os.path.join(HERE, "plot_results.py"),
                        results], check=True)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
