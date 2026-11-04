#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Runs the transport benchmark executables of one or more build trees and
writes one CSV per benchmark and toolchain, then the Markdown and the charts.

    python3 run_benchmark.py --build-dir gcc=build-bench-gcc \
        --build-dir clang=build-bench-clang [--out-dir results] [--quick]

Each build tree must hold bin/LumexTransportBench{Codec,Protocol,Logging,
Dialogue,Devices}. The executables run one after another under `timeout` (and
`prlimit --as` where it exists). The timings are only worth anything on an
idle machine: the load average (the loadavg column of every row) is read after
each case, and a run in which a row shows more than --max-load is repeated up
to --retries times after a pause. Standard library only.
"""

import argparse
import csv
import os
import shutil
import subprocess
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
BENCHMARKS = [("codec", "LumexTransportBenchCodec"),
              ("protocol", "LumexTransportBenchProtocol"),
              ("logging", "LumexTransportBenchLogging"),
              ("dialogue", "LumexTransportBenchDialogue"),
              ("devices", "LumexTransportBenchDevices")]


def die(message):
    sys.stderr.write("run_benchmark.py: error: %s\n" % message)
    sys.exit(1)


def worst_load(path):
    worst = 0.0
    with open(path) as handle:
        lines = [line for line in handle if not line.startswith("#")]
    for row in csv.DictReader(lines):
        worst = max(worst, float(row["loadavg"]))
    return worst


def run_one(executable, csv_path, args):
    command = [executable, csv_path]
    if args.quick:
        command.append("--quick")
    if shutil.which("prlimit"):
        command = ["prlimit", "--as=4000000000"] + command
    command = ["timeout", str(args.timeout)] + command
    for attempt in range(args.retries + 1):
        result = subprocess.run(command, stdout=subprocess.PIPE,
                                stderr=subprocess.STDOUT,
                                universal_newlines=True)
        sys.stdout.write(result.stdout)
        if result.returncode != 0:
            die("%s exited with %d" % (executable, result.returncode))
        load = worst_load(csv_path)
        if load <= args.max_load or attempt == args.retries:
            if load > args.max_load:
                sys.stderr.write("warning: load %.2f above %.1f in %s\n"
                                 % (load, args.max_load, csv_path))
            return
        sys.stderr.write("load %.2f above %.1f: repeating in %d s\n"
                         % (load, args.max_load, args.pause))
        time.sleep(args.pause)


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--build-dir", action="append", required=True,
                        help="name=directory, repeatable")
    parser.add_argument("--out-dir", default=os.path.join(HERE, "results"))
    parser.add_argument("--quick", action="store_true")
    parser.add_argument("--timeout", type=int, default=1800)
    parser.add_argument("--max-load", type=float, default=2.0)
    parser.add_argument("--retries", type=int, default=2)
    parser.add_argument("--pause", type=int, default=120)
    parser.add_argument("--no-plot", action="store_true")
    args = parser.parse_args()

    os.makedirs(args.out_dir, exist_ok=True)
    for spec in args.build_dir:
        if "=" not in spec:
            die("--build-dir takes name=directory, got %r" % spec)
        name, directory = spec.split("=", 1)
        for key, target in BENCHMARKS:
            executable = os.path.join(directory, "bin", target)
            if not os.path.exists(executable):
                die("%s is not built" % executable)
            run_one(executable,
                    os.path.join(args.out_dir, "%s_%s.csv" % (key, name)),
                    args)
    if not args.no_plot:
        subprocess.check_call([sys.executable,
                               os.path.join(HERE, "plot_results.py"),
                               args.out_dir])


if __name__ == "__main__":
    main()
