#!/usr/bin/env python3
"""Build and run the mutants of `mutants.json`; print a table; exit 0 only if every verdict is as expected.

Use `run_mutants.sh` (a thin wrapper) or call this file directly; see README.md.

For each mutant: write its include overlay (`mutate.py`), build the test
executables its checks need in a build tree configured with
`-DCMAKE_CXX_FLAGS="-iquote <overlay>"`, then run every check. A check is a
gtest filter on one executable (and optional environment); each selected
test runs in its own process with a timeout, so a crash or a hang of one test
is a catch of that test and hides nothing else. A test that fails, crashes,
or times out catches the mutant; a test that is skipped caught nothing and a
check in which every test was skipped is invalid (a catch that silently did
not run must never read as "survived").
"""

from __future__ import print_function

import argparse
import fnmatch
import hashlib
import io
import json
import os
import re
import shlex
import subprocess
import sys
import time

sys.dont_write_bytecode = True
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import mutate  # noqa: E402  (the sibling module)

CAUGHT, UNCAUGHT, INVALID = "caught", "uncaught", "invalid"


def run(command, env=None, timeout=None, cwd=None):
    """Runs a command; returns (exit code, combined output). A timeout is exit code 124."""
    environment = dict(os.environ)
    environment["LC_ALL"] = "C"
    if env:
        environment.update(env)
    process = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, env=environment,
                               cwd=cwd, universal_newlines=True, errors="replace")
    try:
        output, _ = process.communicate(timeout=timeout)
    except subprocess.TimeoutExpired:
        process.kill()
        output, _ = process.communicate()
        return 124, (output or "") + "\n[run_mutants] TIMEOUT after %s s\n" % timeout
    return process.returncode, output or ""


class Harness(object):
    def __init__(self, args, data):
        self.args = args
        self.data = data
        self.executables = data["executables"]
        self.build_dir = os.path.abspath(args.build_dir)
        self.overlay = os.path.abspath(args.overlay)
        self.tree = os.path.abspath(args.tree)
        self.log_dir = os.path.abspath(args.log_dir)
        self.result_dir = None
        if not os.path.isdir(self.log_dir):
            os.makedirs(self.log_dir)
        self.test_lists = {}
        self.rebuilt = 0

    # ---- build -------------------------------------------------------

    def check_configuration(self):
        cache = os.path.join(self.build_dir, "CMakeCache.txt")
        if not os.path.exists(cache):
            return "the build directory %s is not configured" % self.build_dir
        with io.open(cache, "r", encoding="utf-8", errors="replace") as stream:
            text = stream.read()
        flags = re.search(r"^CMAKE_CXX_FLAGS:[A-Z]+=(.*)$", text, re.M)
        if flags is None or self.overlay not in flags.group(1) or "-iquote" not in flags.group(1):
            return "CMAKE_CXX_FLAGS of %s does not contain -iquote %s" % (self.build_dir, self.overlay)
        return None

    def configure(self):
        command = ["cmake", "-S", self.tree, "-B", self.build_dir, "-DCMAKE_BUILD_TYPE=Release",
                   "-DLUMEX_BUILD_TESTS=ON", "-DLUMEX_BUILD_DOCUMENTATION=OFF",
                   "-DCMAKE_CXX_FLAGS=-iquote " + self.overlay] + list(self.args.cmake_arg or [])
        code, output = run(command)
        sys.stderr.write(output[-2000:])
        return code

    def build(self, executables, log_path):
        targets = sorted(set(self.executables[name]["target"] for name in executables))
        command = ["cmake", "--build", self.build_dir, "--target"] + targets + ["--parallel", str(self.args.jobs)]
        code, output = run(command)
        with io.open(log_path, "w", encoding="utf-8") as stream:
            stream.write(output)
        self.rebuilt = len(re.findall(r"^\[\d+/\d+\] (?:Building|Linking) ", output, re.M))
        return code == 0

    def binary(self, name):
        return os.path.join(self.build_dir, self.executables[name]["binary"])

    # ---- tests -------------------------------------------------------

    def list_tests(self, executable, gtest_filter):
        key = (executable, gtest_filter)
        if key in self.test_lists:
            return self.test_lists[key]
        command = [self.binary(executable), "--gtest_list_tests"]
        if gtest_filter:
            command.append("--gtest_filter=" + gtest_filter)
        code, output = run(command, timeout=120)
        tests, suite = [], ""
        for line in output.splitlines():
            if line.startswith("Running main()"):
                continue
            if not line.startswith(" "):
                suite = line.split("#")[0].strip()
            elif line.strip():
                tests.append(suite + line.split("#")[0].strip())
        self.test_lists[key] = tests
        return tests

    def run_check(self, check, log):
        """Returns (outcome, first failing test, number run, number failed, oracle text)."""
        executable = check["exe"]
        tests = self.list_tests(executable, check.get("filter", ""))
        timeout = check.get("timeout", self.args.timeout)
        ran = failed = 0
        first = ""
        oracle = ""
        for test in tests:
            code, output = run([self.binary(executable), "--gtest_filter=" + test], env=check.get("env"),
                               timeout=timeout)
            skipped = code == 0 and "[  SKIPPED ]" in output and "[  PASSED  ]" not in output
            if skipped:
                continue
            ran += 1
            if code != 0:
                failed += 1
                if not first:
                    first = test
                    match = re.search(r"SIM-(?:FAIL|CRASH)[^\n]*", output)
                    oracle = match.group(0)[:160] if match else ("exit code %d" % code)
                    log.write("  first failing test: %s\n    %s\n" % (test, oracle))
                if check.get("stop_first", True):
                    break
        if ran == 0:
            log.write("  no test ran (%d selected, all skipped or none)\n" % len(tests))
            return INVALID, "", 0, 0, ""
        return (CAUGHT if failed else UNCAUGHT), first, ran, failed, oracle

    # ---- one mutant --------------------------------------------------

    def run_mutant(self, mutant, rebuild=True):
        log_path = os.path.join(self.log_dir, mutant["id"].replace("/", "_") + ".log")
        result = {"id": mutant["id"], "outcomes": [], "status": "ok"}
        with io.open(log_path, "w", encoding="utf-8") as log:
            log.write("mutant %s: %s\n" % (mutant["id"], mutant.get("description", "")))
            try:
                written, changed = mutate.build_overlay(self.tree, self.overlay, self.data, mutant["id"])
            except (KeyError, ValueError) as error:
                log.write("STALE: %s\n" % error.args[0])
                result["status"] = "stale"
                return result
            log.write("overlay: %d file(s) written, mutated: %s\n" % (written, ", ".join(changed)))
            if rebuild:
                needed = sorted(set(check["exe"] for check in mutant["checks"]))
                if not self.build(needed, log_path + ".build"):
                    log.write("BUILD ERROR (see %s.build)\n" % log_path)
                    result["status"] = "build-error"
                    return result
                if written and not self.rebuilt:
                    # The overlay changed but the build did nothing: the executables are not those of this mutant.
                    log.write("STALE BUILD: the overlay was written and nothing was rebuilt (see %s.build)\n" % log_path)
                    result["status"] = "stale-build"
                    return result
            for check in mutant["checks"]:
                started = time.time()
                outcome, first, ran, failed, oracle = self.run_check(check, log)
                log.write("check %s: %s (%d run, %d failed, %.0f s)\n"
                          % (check["label"], outcome, ran, failed, time.time() - started))
                result["outcomes"].append((check, outcome))
        return result

    def verdict(self, mutant, result):
        """Maps the outcomes to the class of the table: caught, soak only or uncaught."""
        if result["status"] != "ok":
            return result["status"]
        if any(outcome == INVALID for _, outcome in result["outcomes"]):
            return "invalid"
        quick = any(outcome == CAUGHT for check, outcome in result["outcomes"] if check.get("kind", "quick") == "quick")
        soak = any(outcome == CAUGHT for check, outcome in result["outcomes"] if check.get("kind", "quick") == "soak")
        return "caught" if quick else ("soak only" if soak else "uncaught")

    def matches(self, mutant, result, verdict):
        expected = {"caught": "caught", "soak only": "soak only", "survivor": "uncaught",
                    "equivalent": "uncaught"}[mutant["expected"]]
        if verdict != expected:
            return False
        for check, outcome in result["outcomes"]:
            want = check.get("expect")
            if want is not None and outcome != {"caught": CAUGHT, "uncaught": UNCAUGHT}[want]:
                return False
        return True


def select(mutants, args):
    chosen = []
    for mutant in mutants:
        if args.layer and mutant["layer"] not in args.layer:
            continue
        if args.only and not any(fnmatch.fnmatchcase(mutant["id"], pattern) for pattern in args.only):
            continue
        chosen.append(mutant)
    return chosen


def format_table(rows):
    header = ("id", "layer", "expected", "actual", "match", "checks")
    widths = [len(h) for h in header]
    for row in rows:
        for index, cell in enumerate(row):
            widths[index] = max(widths[index], len(cell))
    lines = []
    for row in [header] + rows:
        lines.append("  ".join(cell.ljust(widths[index]) for index, cell in enumerate(row)).rstrip())
    return "\n".join(lines)


def result_path(harness, name):
    return os.path.join(harness.result_dir, name.replace("/", "_") + ".json")


def write_result(harness, name, payload):
    if not os.path.isdir(harness.result_dir):
        os.makedirs(harness.result_dir)
    with io.open(result_path(harness, name), "w", encoding="utf-8") as stream:
        json.dump(payload, stream, indent=1, sort_keys=True)
        stream.write("\n")


def read_result(harness, name):
    try:
        with io.open(result_path(harness, name), "r", encoding="utf-8") as stream:
            return json.load(stream)
    except (IOError, OSError, ValueError):
        return None


def check_key(check):
    return json.dumps([check["exe"], check.get("filter", ""), check.get("env", {})], sort_keys=True)


def work_control(harness, mutants):
    """Builds the unmutated tree and runs every distinct check on it: none may catch anything."""
    mutate.build_overlay(harness.tree, harness.overlay, harness.data, "none")
    needed = sorted(set(check["exe"] for mutant in mutants for check in mutant["checks"]))
    previous = read_result(harness, "@control") if harness.args.resume else None
    verified = set(previous.get("keys", [])) if previous and previous.get("ok") else set()
    message = ""
    ok = harness.build(needed, os.path.join(harness.log_dir, "control.build"))
    if not ok:
        message = "the unmutated tree does not build (see %s/control.build)" % harness.log_dir
    elif not harness.args.skip_control:
        seen = set(verified)
        with io.open(os.path.join(harness.log_dir, "control.log"), "a" if verified else "w", encoding="utf-8") as log:
            for mutant in mutants:
                for check in mutant["checks"]:
                    key = check_key(check)
                    if key in seen:
                        continue
                    seen.add(key)
                    control = dict(check)
                    control["stop_first"] = True
                    outcome = harness.run_check(control, log)[0]
                    log.write("control %s: %s\n" % (check["label"], outcome))
                    if outcome != UNCAUGHT:
                        ok = False
                        message = ("control check '%s' of %s is %s on the unmutated tree (see %s/control.log)"
                                   % (check["label"], mutant["id"], outcome, harness.log_dir))
                        break
                if not ok:
                    break
        verified = seen
    write_result(harness, "@control", {"ok": ok, "message": message, "keys": sorted(verified)})


def fingerprint(mutant):
    """A short digest of what a result depends on: the edits and the checks (not the expected verdict)."""
    text = json.dumps([mutant.get("edits"), mutant.get("checks")], sort_keys=True)
    return hashlib.sha1(text.encode("utf-8")).hexdigest()[:12]


def work_mutant(harness, mutant):
    result = harness.run_mutant(mutant)
    write_result(harness, mutant["id"], {
        "id": mutant["id"], "fingerprint": fingerprint(mutant), "status": result["status"],
        "outcomes": [[check["label"], outcome] for check, outcome in result["outcomes"]]})


def evaluate(harness, mutant, stored):
    """The table row of @p mutant from a stored result: (actual, match, checks text)."""
    if stored is None:
        return "no-result", False, ""
    if stored.get("fingerprint") != fingerprint(mutant):
        return "stale-result", False, ""
    result = {"status": stored["status"], "outcomes": [(check, outcome) for check, (label, outcome)
                                                       in zip(mutant["checks"], stored["outcomes"])]}
    if stored["status"] == "ok" and len(stored["outcomes"]) != len(mutant["checks"]):
        return "stale-result", False, ""
    verdict = harness.verdict(mutant, result)
    marks = " ".join("%s:%s" % (check["label"], {CAUGHT: "C", UNCAUGHT: "-", INVALID: "?"}[outcome])
                     for check, outcome in result["outcomes"])
    return verdict, harness.matches(mutant, result, verdict), marks


def worker_command(args, name):
    command = [sys.executable, os.path.abspath(__file__), "--tree", args.tree, "--build-dir", args.build_dir,
               "--overlay", args.overlay, "--mutants", args.mutants, "--jobs", str(args.jobs),
               "--timeout", str(args.timeout), "--log-dir", args.log_dir, "--result-dir", args.result_dir,
               "--worker", name]
    if args.skip_control:
        command.append("--skip-control")
    if args.resume:
        command.append("--resume")
    for pattern in args.only or []:
        command += ["--only", pattern]
    for layer in args.layer or []:
        command += ["--layer", layer]
    return command


def main(argv):
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("--tree", default=os.path.abspath(os.path.join(HERE, "..", "..")), help="root of the LumexLib tree")
    parser.add_argument("--build-dir", help="a configured build tree (see README.md)")
    parser.add_argument("--overlay", help="the overlay directory the build tree was configured with")
    parser.add_argument("--mutants", default=os.path.join(HERE, "mutants.json"))
    parser.add_argument("--only", action="append", help="mutant id or glob (repeatable), e.g. 'V2/*'")
    parser.add_argument("--layer", action="append", help="only mutants of this layer (repeatable)")
    parser.add_argument("--jobs", type=int, default=2, help="parallel compile jobs of one build (default 2)")
    parser.add_argument("--timeout", type=int, default=600, help="seconds per test process (default 600)")
    parser.add_argument("--log-dir", default=None, help="per-mutant logs (default <build-dir>/mutation-logs)")
    parser.add_argument("--result-dir", default=None, help="per-mutant results (default <log-dir>/results)")
    parser.add_argument("--wrapper", default=None,
                        help="a command that every build-and-test step of one mutant runs under, e.g. a lock "
                             "script that makes jobs take turns; each mutant is then a separate process")
    parser.add_argument("--batch", type=int, default=1,
                        help="with --wrapper: how many mutants one wrapped process handles (default 1)")
    parser.add_argument("--report", action="store_true",
                        help="only print the table of the results already in the result directory")
    parser.add_argument("--resume", action="store_true",
                        help="keep the results already in the result directory and run only the missing mutants")
    parser.add_argument("--skip-control", action="store_true", help="do not run the control (unmutated) checks first")
    parser.add_argument("--check-patterns", action="store_true",
                        help="only apply every mutant in memory and report stale patterns; no build")
    parser.add_argument("--configure", action="store_true", help="configure the build tree first (cmake)")
    parser.add_argument("--cmake-arg", action="append", help="extra argument of the configure step (repeatable)")
    parser.add_argument("--list", action="store_true", help="list the mutants and exit")
    parser.add_argument("--worker", help=argparse.SUPPRESS)
    args = parser.parse_args(argv)

    data = mutate.load_mutants(args.mutants)
    mutants = select(data["mutants"], args)
    if args.list:
        for mutant in mutants:
            print("%-10s %-6s %-10s %s" % (mutant["id"], mutant["layer"], mutant["expected"], mutant.get("description", "")))
        return 0
    if args.check_patterns:
        stale = 0
        scratch = os.path.join(os.path.abspath(args.overlay or "."), "check-patterns")
        for mutant in mutants:
            try:
                mutate.build_overlay(os.path.abspath(args.tree), scratch, data, mutant["id"])
            except (KeyError, ValueError) as error:
                stale += 1
                print("STALE %s" % error.args[0])
        print("%d mutant(s) checked, %d stale" % (len(mutants), stale))
        return 2 if stale else 0
    if not args.build_dir or not args.overlay:
        parser.error("--build-dir and --overlay are required")
    args.tree = os.path.abspath(args.tree)
    args.build_dir = os.path.abspath(args.build_dir)
    args.overlay = os.path.abspath(args.overlay)
    args.mutants = os.path.abspath(args.mutants)
    if args.log_dir is None:
        args.log_dir = os.path.join(args.build_dir, "mutation-logs")
    args.log_dir = os.path.abspath(args.log_dir)
    if args.result_dir is None:
        args.result_dir = os.path.join(args.log_dir, "results")
    args.result_dir = os.path.abspath(args.result_dir)
    harness = Harness(args, data)
    harness.result_dir = args.result_dir

    if args.worker:
        for name in args.worker.split(","):
            if name == "@control":
                work_control(harness, mutants)
            else:
                work_mutant(harness, [m for m in data["mutants"] if m["id"] == name][0])
        return 0

    if args.report:
        return report(harness, mutants)
    if not os.path.isdir(harness.overlay):
        os.makedirs(harness.overlay)
    # The identity overlay must exist before the first build, so that the build records every copy as a dependency.
    mutate.build_overlay(harness.tree, harness.overlay, data, "none")
    if args.configure and harness.configure() != 0:
        return 3
    problem = harness.check_configuration()
    if problem:
        sys.stderr.write("run_mutants: %s\nConfigure it with: cmake -S %s -B %s -DCMAKE_BUILD_TYPE=Release "
                         "-DLUMEX_BUILD_TESTS=ON '-DCMAKE_CXX_FLAGS=-iquote %s'  (or pass --configure)\n"
                         % (problem, harness.tree, harness.build_dir, harness.overlay))
        return 3

    wrapper = shlex.split(args.wrapper) if args.wrapper else None

    def work(names, direct):
        """Runs the steps @p names (one process under the wrapper, or in this process)."""
        for name in names:
            stale = result_path(harness, name)
            if os.path.exists(stale) and not (name == "@control" and args.resume):
                os.remove(stale)
        if wrapper:
            code = subprocess.call(wrapper + worker_command(args, ",".join(names)))
            if code != 0:
                sys.stderr.write("run_mutants: the worker of %s ended with status %d\n" % (",".join(names), code))
        else:
            direct()

    control = read_result(harness, "@control") if args.resume else None
    wanted = set(check_key(check) for mutant in mutants for check in mutant["checks"])
    if control is None or not control["ok"] or not wanted <= set(control.get("keys", [])):
        work(["@control"], lambda: work_control(harness, mutants))
        control = read_result(harness, "@control")
    if control is None or not control["ok"]:
        sys.stderr.write("run_mutants: %s\n" % (control["message"] if control else "the control step produced no result"))
        return 3

    step = max(1, args.batch) if wrapper else 1
    for start in range(0, len(mutants), step):
        group = mutants[start:start + step]
        if args.resume:
            group = [m for m in group
                     if read_result(harness, m["id"]) is None
                     or read_result(harness, m["id"]).get("fingerprint") != fingerprint(m)]
            if not group:
                continue
        work([m["id"] for m in group], lambda g=group: [work_mutant(harness, m) for m in g])

    return report(harness, mutants)


def report(harness, mutants):
    rows, mismatches = [], []
    for mutant in mutants:
        actual, ok, marks = evaluate(harness, mutant, read_result(harness, mutant["id"]))
        rows.append([mutant["id"], mutant["layer"], mutant["expected"], actual, "ok" if ok else "MISMATCH", marks])
        if not ok:
            mismatches.append(mutant["id"])
    print(format_table(rows))
    print("")
    print("%d mutant(s), %d as expected, %d mismatch(es)%s"
          % (len(rows), len(rows) - len(mismatches), len(mismatches), (": " + " ".join(mismatches)) if mismatches else ""))
    return 1 if mismatches else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
