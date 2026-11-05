#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Measures the time and the memory of compiling a protocol::client<P>.

compile_time.cpp is compiled once per variant (BENCH_TRANSPORT_CLIENT):

  0  the headers only (the baseline)
  1  a client over the invented 7-row table, one method
  2  a client over layer device_m1 of the largest test table (device M, 20
     rows), one method instantiated per row of the layer
  3  as 2 over layer device_m2 (15 rows)
  4  as 2 with the synchronous form of each method (no completion handler)

in three modes: syntax only (-fsyntax-only), code generation as a Debug
build has it (-O0 -g -c) and as a Release build of this repository has it
(-O2 -g -DNDEBUG -c). Per compile the wall time is
taken from a monotonic clock and the peak resident memory of the compiler
process from the rusage that wait4 returns for that child alone (Linux).
The median of --repeats wall times and the largest peak are kept.

The budget is judged per called row of the largest table (variants 2 to 4):
the time over the headers-only unit divided by the rows (under 0.1 s at
syntax only, under 1 s with code generation) and the peak memory (under
1.5 GB); plot_results.py computes the verdicts.

A group of repeats whose load average (one minute, read after the group)
is above --max-load (default 3.0) is repeated after a pause, up to --retries
times, and then kept with a warning; the load of each row is in the CSV. The
average includes the compile itself (1.0) and the desktop session of the
machine (about 1.7 on the machine of the committed run), so a quiet machine
shows about 2.7 and the limit only catches another job.

Usage:
    python3 compile_time.py --compiler gcc=/opt/gcc-13.2.0/bin/g++ \
        --compiler clang=/opt/llvm-23.1.0/bin/clang++ --repeats 3

The include directories and defines the transport headers need come from
--asio-include (default: 3rdparty/asio/include of the checkout). Writes
--out-dir/compile_time.csv (default: results/ next to this script). Standard
library only; Linux only (fork, wait4).
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
VARIANTS = [(0, "headers"), (1, "table7_one_method"),
            (2, "device_m1_20_methods"), (3, "device_m2_15_methods"),
            (4, "device_m1_sync_20_methods")]
# The modes: syntax only; code generation as a Debug build has it (-O0 -g);
# as a Release build of this repository has it (-O2 -g -DNDEBUG).
MODES = [("syntax", ["-fsyntax-only"]), ("O0", ["-O0", "-g", "-c"]),
         ("O2", ["-O2", "-g", "-DNDEBUG", "-c"])]
DEFINES = ["-DASIO_STANDALONE", "-DASIO_NO_DEPRECATED",
           "-DLUMEX_TRANSPORT_WITH_TESTING=1",
           "-DLUMEX_TRANSPORT_WITH_PROTOCOL=1",
           "-DLUMEX_TRANSPORT_WITH_FRAMING=1"]


def die(message):
    sys.stderr.write("compile_time.py: error: %s\n" % message)
    sys.exit(1)


def require_linux():
    """The script forks and reads the rusage of the compiler (Linux)."""
    if not sys.platform.startswith("linux") or not hasattr(os, "wait4"):
        die("Linux only (os.fork, os.wait4); this is %s" % sys.platform)


def command_line(command, variant, mode_flags, standard, asio, output):
    source = os.path.join(HERE, "compile_time.cpp")
    line = shlex.split(command) + ["-std=c++" + standard, "-I" + REPO,
                                   "-isystem", asio] + DEFINES
    line += ["-DBENCH_TRANSPORT_CLIENT=%d" % variant] + mode_flags
    if "-c" in mode_flags:
        line += ["-o", output]
    return line + [source]


def run_measured(line):
    """Runs `line`, returns (seconds, peak_rss_mb, returncode, output)."""
    start = time.perf_counter()
    read_end, write_end = os.pipe()
    pid = os.fork()
    if pid == 0:
        try:
            os.close(read_end)
            os.dup2(write_end, 1)
            os.dup2(write_end, 2)
            os.execvp(line[0], line)
        finally:
            os._exit(127)
    os.close(write_end)
    chunks = []
    while True:
        data = os.read(read_end, 65536)
        if not data:
            break
        chunks.append(data)
    os.close(read_end)
    _, status, usage = os.wait4(pid, 0)
    seconds = time.perf_counter() - start
    code = os.WEXITSTATUS(status) if os.WIFEXITED(status) else 128
    return (seconds, usage.ru_maxrss / 1024.0, code,
            b"".join(chunks).decode("utf-8", "replace"))


def main():
    require_linux()
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--compiler", action="append", required=True,
                        help="name=command, repeatable")
    parser.add_argument("--repeats", type=int, default=3)
    parser.add_argument("--standard", default="11")
    parser.add_argument("--asio-include",
                        default=os.path.join(REPO, "3rdparty", "asio",
                                             "include"))
    parser.add_argument("--mode", action="append",
                        choices=[m for m, _ in MODES],
                        help="only these modes (default: all)")
    parser.add_argument("--out-dir", default=os.path.join(HERE, "results"))
    parser.add_argument("--scratch", default="/tmp",
                        help="where the object files go (deleted)")
    parser.add_argument("--max-load", type=float, default=3.0)
    parser.add_argument("--retries", type=int, default=2)
    parser.add_argument("--pause", type=int, default=60)
    args = parser.parse_args()

    rows = []
    for spec in args.compiler:
        if "=" not in spec:
            die("--compiler takes name=command, got %r" % spec)
        name, command = spec.split("=", 1)
        for mode, flags in MODES:
            if args.mode and mode not in args.mode:
                continue
            for variant, label in VARIANTS:
                for attempt in range(args.retries + 1):
                    times = []
                    peak = 0.0
                    output = os.path.join(args.scratch,
                                          "lumex_ct_%d.o" % os.getpid())
                    for _ in range(args.repeats):
                        line = command_line(command, variant, flags,
                                            args.standard, args.asio_include,
                                            output)
                        seconds, rss, code, text = run_measured(line)
                        if code != 0:
                            die("%s failed:\n%s" % (" ".join(line),
                                                    text[-4000:]))
                        times.append(seconds)
                        peak = max(peak, rss)
                    if os.path.exists(output):
                        os.remove(output)
                    load = os.getloadavg()[0]
                    if load <= args.max_load or attempt == args.retries:
                        if load > args.max_load:
                            sys.stderr.write(
                                "warning: load %.2f above %.1f for %s %s %s\n"
                                % (load, args.max_load, name, mode, label))
                        break
                    sys.stderr.write("load %.2f above %.1f: repeating %s %s "
                                     "%s in %d s\n"
                                     % (load, args.max_load, name, mode,
                                        label, args.pause))
                    time.sleep(args.pause)
                rows.append({"toolchain": name, "standard": args.standard,
                             "mode": mode, "variant": label,
                             "median_s": "%.2f" % statistics.median(times),
                             "min_s": "%.2f" % min(times),
                             "peak_rss_mb": "%.0f" % peak,
                             "repeats": args.repeats,
                             "loadavg": "%.2f" % load})
                sys.stderr.write("%s %s %s: %s s, %s MB\n"
                                 % (name, mode, label, rows[-1]["median_s"],
                                    rows[-1]["peak_rss_mb"]))
    os.makedirs(args.out_dir, exist_ok=True)
    path = os.path.join(args.out_dir, "compile_time.csv")
    with open(path, "w", newline="") as handle:
        for spec in args.compiler:
            name, command = spec.split("=", 1)
            version = subprocess.run(shlex.split(command) + ["--version"],
                                     stdout=subprocess.PIPE,
                                     universal_newlines=True).stdout
            handle.write("# compiler %s: %s\n"
                         % (name, version.splitlines()[0]))
        writer = csv.DictWriter(handle, fieldnames=list(rows[0].keys()),
                                lineterminator="\n")
        writer.writeheader()
        writer.writerows(rows)
    sys.stderr.write("wrote %s\n" % path)


if __name__ == "__main__":
    main()
