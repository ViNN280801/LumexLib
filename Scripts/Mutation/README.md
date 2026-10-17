# Mutation harness of the split-count engine

This directory holds the tools that put a deliberate defect (a *mutant*) into the split-count engine, build the affected test executables, run the checks that are supposed to notice it and compare the outcome with the recorded verdict. The results they reproduce are described in `lumex/core/atomic/VERIFICATION.md` (sections "The simulator", "The ABA scenarios", "The broad suites", "The mutations in one view"). Python 3.7 or newer (standard library only) and a POSIX shell.

| File | What it is |
| --- | --- |
| `mutants.json` | The data: the executables, and for every mutant its id, layer, description, the text edits, the checks and the expected verdict. |
| `mutate.py` | Writes the include overlay of one mutant. It never edits the tree. |
| `run_mutants.py`, `run_mutants.sh` | Builds the executables for each mutant, runs its checks, prints a table, exits 0 only if every verdict is as expected. `run_mutants.sh` is the entry point (it only fixes `LC_ALL=C` and calls the Python file). |

## Why overlays

A mutant must never change a file of the working tree: an interrupted run would leave a defect behind, a build tree could not be shared with the unmutated build, and two mutants could not be compared with the same objects. So `mutate.py` writes **copies** of the headers it may change into a directory (the overlay) that the compiler searches first. The build tree is configured once with

```sh
cmake -G Ninja -S <tree> -B <build> -DCMAKE_BUILD_TYPE=Release -DLUMEX_BUILD_TESTS=ON \
      -DLUMEX_BUILD_DOCUMENTATION=OFF "-DCMAKE_CXX_FLAGS=-iquote <overlay>"
```

and `#include "lumex/..."` then finds the copy (`-iquote` is searched before `-I`). The overlay mirrors the whole directory of every header that any mutant edits, so the relative includes between sibling headers stay inside it; a file whose content is already right is not rewritten, so the build system rebuilds exactly what a change of mutant changed (the overlay must exist, equal to the tree, before the first build: `run_mutants.py` makes it). `--configure` runs the `cmake` line above for you (extra arguments: `--cmake-arg`). The tree is never touched, so there is nothing to restore after an interrupted run.

The first condition of a trustworthy catch is a clean control. Before the mutants the harness builds the unmutated tree and runs every distinct check on it: a check that fails there (or that runs no test, or whose tests are all skipped) stops the run with status 3, so a catch always means "the mutation made this fail".

## Running it

```sh
Scripts/Mutation/run_mutants.sh --build-dir <build> --overlay <overlay> --configure   # all mutants
Scripts/Mutation/run_mutants.sh --build-dir <build> --overlay <overlay> --only 'V2/*' --only V1/M1
Scripts/Mutation/run_mutants.sh --check-patterns --overlay <scratch>                  # no build: are all patterns still found?
Scripts/Mutation/run_mutants.sh --list [--layer v1]
```

`--layer` takes `v1` (the pre-registered list M1 to M38 and R4; a mutation with several readings has one id per reading, for example `V1/M5` and `V1/M5w`), `v2` (the ABA campaign), `v3` (the five mutants of the review of the family suites, run against the engine with the reserve of the compare-exchange), `x` (the mutants of the reserve of the compare-exchange; `X/X1` to `X/X9` are the rows of the table in `VERIFICATION.md`, the others carry the names of the review's script) or `o3` (two mutants of the engine against the differential suite of the pointer family). Options: `--jobs N` (parallel compile jobs of one build, default 2), `--timeout S` (seconds per test process, default 600), `--log-dir`, `--result-dir`, `--skip-control`.

Every mutant leaves a result file (the outcome of each of its checks) in the result directory (default `<log-dir>/results`); the table is computed from those files and the current `mutants.json`. So a run can be interrupted and continued with `--resume` (finished mutants are kept, the others are run), `--report` prints the table of the files without building anything, and a change of an `expected` value needs no new run; a result whose edits or checks changed since it was made is shown as `stale-result` and is run again by `--resume`.

A full run builds the dedicated and the family executable once per mutant (about 3 minutes each at `-j2` with GCC 13.2) and runs the checks, so the whole set takes hours. To let other jobs of the machine take their turns, give `--wrapper "<lock command>"`: then the control step and every group of `--batch N` mutants (default 1) run as a separate process under that command, e.g. `--wrapper /path/to/lumexlock.sh --batch 12`.

The output is one table, in the order of `mutants.json`, for example

```
id      layer  expected  actual  match  checks
V1/M1   v1     caught    caught  ok     S01@R8@1:C unit:C
V2/M10  v2     caught    caught  ok     v2:- soak:RX@R8@3:C v1:C
```

`checks` lists each check of the mutant with `C` (caught: at least one test of it failed, crashed or timed out), `-` (every selected test passed) or `?` (no test ran: invalid, always a mismatch). The table has no times and no test names, so two runs on the same tree print the same text (a stress test that catches a mutant only now and then would show up as a difference, and is a finding). The per-mutant logs in the log directory name the first failing test of each check, with the `SIM-FAIL oracles=...` line of the simulator, and keep the build log of a mutant that does not compile.

Exit status: 0 every verdict is as expected; 1 at least one mismatch (the line below the table lists them); 2 with `--check-patterns`, a pattern no longer matches the tree; 3 the setup is wrong (build tree not configured with the overlay, the unmutated tree does not build, a control check fails).

## What a mutant is, and what the verdict means

```json
{
 "id": "V1/M22a",
 "layer": "v1",
 "description": "pre-transfer off by one (L + 1)",
 "form": "the edit as registered with the mutation",
 "edits": [{"file": "lumex/core/.../LumexSplitCountCell.hpp",
            "old": "std::uint32_t moved = word::ticks_of (seen) + Policy::reserve ();",
            "new": "std::uint32_t moved = word::ticks_of (seen) + 1u + Policy::reserve ();"}],
 "checks": [{"label": "S01@R8@1", "exe": "dedicated", "filter": "LumexSplitCountSimTest.GivenScenarioS01R8_*",
             "env": {"LUMEX_SIM_ONLY": "S01@R8@1"}, "expect": "caught"},
            {"label": "unit", "exe": "dedicated", "filter": "-LumexSplitCountSim*", "expect": "caught"}],
 "expected": "caught"
}
```

- `edits`: each edit replaces the text `old` by `new` in `file` (a path from the root of the tree) and must find `old` exactly `count` times (default 1). `old` and `new` are a string or a list of lines. A pattern that is not found the stated number of times means the mutant is *stale* against the tree (the code moved): the table shows `stale` and the run fails, which is how the harness notices a refactoring that made a mutant meaningless.
- `form`: present where the one-line description of the mutant is ambiguous, or where the code changed since the mutant was first written; it says which reading was chosen (the literal one). `note` gives the reason of a verdict that is not "caught".
- `checks`: each check is a gtest filter on one executable (`executables` at the top of the file maps a name to a CMake target and its binary), optionally with environment variables (`LUMEX_SIM_ONLY=<scenario>@<policy>@<bound>` narrows the simulator to one scenario at exactly that preemption bound, which is how the pre-registered mutants name their catcher). Every test selected runs in its own process with the timeout; a failing exit, a crash or a timeout is a catch of that test. A skipped test caught nothing. `kind` is `quick` (default) or `soak`; `expect` (`caught` or `uncaught`) pins the outcome of that check, which is how the V1 and V2 columns of the tables are kept apart. `stop_first` (default true) stops a check at its first catch.
- `expected`: `caught` (a quick check catches it), `soak only` (only a soak check does), `survivor` (nothing catches it, with the reason in `note`) or `equivalent` (nothing can: the mutant behaves like the engine). The harness cannot tell a survivor from an equivalent mutant; the word records the argument.

A mutant that does not compile is a mismatch (`build-error`), not a catch; so is a mutant whose overlay was written but whose build rebuilt nothing (`stale-build`: the executables would not be those of the mutant).

## Adding a mutant

1. Add an object to `mutants.json` (the order is the order of the table). Take `old` from the current file, with its indentation; run `run_mutants.sh --check-patterns --overlay <scratch>` to see that it applies exactly as often as you wrote `count`.
2. Choose the checks that should notice it and say what you expect from each. Start without `expect` on the checks you are unsure about and `expected: "caught"`, run `--only <id>`, read the table and the log, then write down what happened. A result that surprises you is the finding: do not adjust `expected` until you know why.
3. Run the mutant with the control (a check that also fails on the unmutated tree is rejected) and run it twice: the two tables must be the same.

To add a test executable of another module, add it to `executables` with its CMake target and binary and put the headers it mutates in `edits`; the harness itself knows nothing about the split-count engine.

## What is not here

- The Relacy mutants (single memory orders and the reserve of the compare-exchange as logic mutants of the models, V4) are in `lumex/tests/core/atomic/smart_ptr/split_count/relacy/CMakeLists.txt` (label `relacy_mutant`) and are not duplicated. The memory-order mutants of the list (M8, M9, M10, M25 and M33) appear here as survivors of the x86-64 test suites, with the pointer to the model that catches them.
- The mutants R5 to R7 of the review of the simulator are not here: their definitions were in a scratch script that was not kept, and nothing in the repository describes them beyond their scenario and oracle.
- The sanitizer matrix and the benchmarks check no mutant.
