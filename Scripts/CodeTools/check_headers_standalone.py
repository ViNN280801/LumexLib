#!/usr/bin/env python3
"""Check that every public LumexLib header compiles on its own.

A public header is a `.hpp` or `.h` file under lumex/, or a module umbrella
(a file without an extension, such as lumex/xml/LumexXml), outside
lumex/tests, lumex/examples and any 3rdparty directory.

For each header the script writes a translation unit with the single line
`#include "lumex/<path>"` and compiles it with `-fsyntax-only` (`/Zs` for
cl and clang-cl) at one language standard. A header that fails alone relies
on a neighbour to have included something first (`std::size_t` without
<cstddef>, a class that another header declared), so a consumer that
includes it first breaks. All compiles run on a configurable number of
parallel jobs. Only the exit status of the compiler counts; warnings are not
reported.

Some headers refuse to compile until the build selects a configuration
macro. They are compiled with the macro listed in NEEDS_MACRO instead of
alone (today: the logger config format header, whose `#error` is
deliberate).

Include directories are the parent of the scanned lumex/ directory (so
`lumex/...` resolves) and its 3rdparty/ directory (`<nlohmann/json.hpp>`).

Usage (from LumexLib/):
  python Scripts/CodeTools/check_headers_standalone.py                 # list
  python Scripts/CodeTools/check_headers_standalone.py --check         # exit 1 if any
  python Scripts/CodeTools/check_headers_standalone.py --check --std 11 \\
      --compiler /usr/bin/g++-8 --jobs 4
  python Scripts/CodeTools/check_headers_standalone.py --dir <copy>/lumex --check

Exit status: 0 (all compile, or no --check), 1 (--check and a header fails),
2 (bad arguments or the compiler cannot compile an empty file), 77 (cl or
clang-cl without the INCLUDE variable of a developer shell: CTest "skipped").
"""

from __future__ import annotations

import argparse
import os
import shlex
import subprocess
import sys
import tempfile
import time
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
from typing import Dict, Iterator, List, Optional, Sequence, Tuple

HEADER_SUFFIXES = {".hpp", ".h"}
SKIPPED_TOP_DIRS = {"tests", "examples"}
SKIPPED_DIR_NAMES = {"3rdparty"}

# Headers that need a configuration macro, with the macros to compile them
# with (path relative to lumex/, forward slashes).
NEEDS_MACRO: Dict[str, Tuple[str, ...]] = {
    "applied/logger/config/LumexLoggerConfigFormat.hpp": (
        "LUMEX_LOGGER_CONFIG_FORMAT_PLAIN_TEXT",
    ),
}

# The options that select a standard, newest spelling first: a compiler that
# predates the final name takes the draft name (GCC 8 knows -std=c++2a, not
# -std=c++20). cl and clang-cl have no C++11 mode (C++14 is their lowest and
# their default) and spell the newest standard /std:c++latest.
GNU_SPELLINGS = {
    "11": ("c++11",),
    "14": ("c++14",),
    "17": ("c++17",),
    "20": ("c++20", "c++2a"),
    "23": ("c++23", "c++2b"),
    "26": ("c++26", "c++2c"),
}
MSVC_SPELLINGS = {
    "11": (),
    "14": ("c++14",),
    "17": ("c++17",),
    "20": ("c++20", "c++latest"),
    "23": ("c++latest",),
    "26": ("c++latest",),
}

COMPILE_TIMEOUT_SECONDS = 300
DEFAULT_JOBS = 4
SKIP_EXIT_CODE = 77
SKIP_MESSAGE = (
    "skipped: no MSVC environment (INCLUDE is empty); "
    "run it from a Visual Studio developer shell"
)


def default_root() -> Path:
    """Return the lumex/ directory of the checkout that holds this script."""
    return Path(__file__).resolve().parents[2] / "lumex"


def is_public_header(relative: Path) -> bool:
    """Tell whether a path relative to lumex/ names a public header."""
    if relative.parts[0] in SKIPPED_TOP_DIRS:
        return False
    if any(part in SKIPPED_DIR_NAMES for part in relative.parts[:-1]):
        return False
    name = relative.name
    if name.startswith("."):
        return False
    return relative.suffix in HEADER_SUFFIXES or "." not in name


def iter_public_headers(root: Path) -> Iterator[Path]:
    """Yield every public header under root, sorted by path."""
    for path in sorted(root.rglob("*")):
        if path.is_file() and is_public_header(path.relative_to(root)):
            yield path


def is_msvc_style(compiler: str) -> bool:
    """Tell whether the compiler takes cl.exe style options."""
    name = os.path.basename(compiler).lower()
    return name in ("cl", "cl.exe", "clang-cl", "clang-cl.exe")


def normalize_standard(value: str) -> str:
    """Return the two digit year of a standard given as 11, c++11, gnu++11."""
    digits = value.lower().replace("gnu++", "").replace("c++", "")
    digits = digits.replace("-std=", "").replace("/std:", "")
    if digits not in GNU_SPELLINGS:
        known = ", ".join(sorted(GNU_SPELLINGS))
        raise ValueError(
            "unknown standard '{}' (use one of {})".format(value, known)
        )
    return digits


def run_compiler(
    command: Sequence[str], timeout: int = COMPILE_TIMEOUT_SECONDS
) -> Tuple[int, str]:
    """Run a compiler command; return its exit status and its output."""
    env = dict(os.environ)
    env["LC_ALL"] = "C"
    env["LANGUAGE"] = "C"
    try:
        done = subprocess.run(
            list(command),
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            universal_newlines=True,
            errors="replace",
            timeout=timeout,
            env=env,
        )
    except subprocess.TimeoutExpired:
        return 124, "timeout after {} s".format(timeout)
    except OSError as error:
        return 127, str(error)
    return done.returncode, done.stdout


def standard_options(compiler: str, year: str, flags: Sequence[str],
                     scratch: Path) -> Optional[List[str]]:
    """Return the options that select the standard, or None if none works.

    The compiler is probed on an empty file with each spelling of the
    standard, so one script run needs no list of compiler versions.
    """
    empty = scratch / "probe.cpp"
    empty.write_text("\n", encoding="utf-8")
    msvc = is_msvc_style(compiler)
    spellings = (MSVC_SPELLINGS if msvc else GNU_SPELLINGS)[year]
    candidates: List[List[str]] = [
        [("/std:" if msvc else "-std=") + spelling] for spelling in spellings
    ]
    if not candidates:
        candidates = [[]]
    for options in candidates:
        probe = build_command(compiler, options, flags, [], [], empty)
        status, _ = run_compiler(probe)
        if status == 0:
            return options
    return None


def build_command(compiler: str, options: Sequence[str],
                  flags: Sequence[str], includes: Sequence[Path],
                  defines: Sequence[str], source: Path) -> List[str]:
    """Return the command that compiles one translation unit."""
    command = [compiler]
    if is_msvc_style(compiler):
        command += ["/nologo", "/Zs", "/EHsc", "/Zc:__cplusplus",
                    "/permissive-"]
        command += ["/I" + str(path) for path in includes]
        command += ["/D" + define for define in defines]
    else:
        command += ["-fsyntax-only"]
        command += ["-I" + str(path) for path in includes]
        command += ["-D" + define for define in defines]
    command += list(options) + list(flags) + [str(source)]
    return command


def first_errors(output: str, limit: int = 3) -> List[str]:
    """Return up to limit lines of compiler output that name an error."""
    lines = [
        line.strip()
        for line in output.splitlines()
        if "error" in line.lower() or "fatal" in line.lower()
    ]
    return lines[:limit] if lines else output.strip().splitlines()[:limit]


def check_header(
    root: Path,
    header: Path,
    compiler: str,
    options: Sequence[str],
    flags: Sequence[str],
    includes: Sequence[Path],
    scratch: Path,
    serial: int,
) -> Tuple[Path, int, List[str]]:
    """Compile one header alone; return (header, status, error lines)."""
    relative = header.relative_to(root).as_posix()
    source = scratch / "tu_{}.cpp".format(serial)
    source.write_text(
        '#include "lumex/{}"\n'.format(relative), encoding="utf-8"
    )
    defines = NEEDS_MACRO.get(relative, ())
    command = build_command(
        compiler, options, flags, includes, defines, source
    )
    status, output = run_compiler(command)
    return header, status, ([] if status == 0 else first_errors(output))


def find_problems(
    root: Path,
    compiler: str,
    year: str,
    flags: Sequence[str],
    jobs: int,
) -> Tuple[List[str], int]:
    """Compile every public header under root; return (reports, count).

    A report is one line per failing header followed by its first errors.
    """
    headers = list(iter_public_headers(root))
    includes = [root.parent]
    if (root.parent / "3rdparty").is_dir():
        includes.append(root.parent / "3rdparty")
    with tempfile.TemporaryDirectory(prefix="lumex_headers_") as name:
        scratch = Path(name)
        options = standard_options(compiler, year, flags, scratch)
        if options is None:
            raise RuntimeError(
                "{} cannot compile an empty file at C++{}".format(
                    compiler, year
                )
            )
        with ThreadPoolExecutor(max_workers=max(1, jobs)) as pool:
            futures = [
                pool.submit(
                    check_header, root, header, compiler, options, flags,
                    includes, scratch, serial,
                )
                for serial, header in enumerate(headers)
            ]
            results = [future.result() for future in futures]
    problems: List[str] = []
    for header, status, errors in results:
        if status == 0:
            continue
        shown = header.relative_to(root.parent).as_posix()
        problems.append("{}: does not compile alone".format(shown))
        problems.extend("    " + line for line in errors)
    return problems, len(headers)


def main() -> int:
    parser = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument(
        "--check",
        action="store_true",
        help="exit with status 1 when a header does not compile alone",
    )
    parser.add_argument(
        "--dir",
        type=Path,
        default=default_root(),
        help="lumex/ directory to scan (default: the one of this checkout)",
    )
    parser.add_argument(
        "--compiler",
        default=os.environ.get("CXX") or "c++",
        help="C++ compiler (default: $CXX, else c++)",
    )
    parser.add_argument(
        "--std",
        default="11",
        help="language standard: 11, 14, 17, 20, 23 or 26 (default: 11)",
    )
    parser.add_argument(
        "--cxx-flags",
        default="",
        help="extra compiler options as one string, split like a shell "
        "would (written --cxx-flags=..., for example --cxx-flags=-stdlib=libc++)",
    )
    parser.add_argument(
        "--jobs",
        "-j",
        type=int,
        default=DEFAULT_JOBS,
        help="compiles in parallel (default: {})".format(DEFAULT_JOBS),
    )
    args = parser.parse_args()

    root = args.dir.resolve()
    if not root.is_dir():
        print("directory not found: {}".format(root), file=sys.stderr)
        return 2
    try:
        year = normalize_standard(args.std)
    except ValueError as error:
        print(error, file=sys.stderr)
        return 2
    flags = shlex.split(args.cxx_flags)
    if is_msvc_style(args.compiler) and not os.environ.get("INCLUDE"):
        # cl and clang-cl find the standard headers through INCLUDE, which a
        # Visual Studio developer shell sets; CTest reports 77 as skipped.
        print(SKIP_MESSAGE)
        return SKIP_EXIT_CODE

    started = time.time()
    try:
        problems, count = find_problems(
            root, args.compiler, year, flags, args.jobs
        )
    except RuntimeError as error:
        print(error, file=sys.stderr)
        return 2
    for line in problems:
        print(line)
    failed = sum(1 for line in problems if not line.startswith(" "))
    print(
        "{} of {} public header(s) do not compile alone "
        "(C++{}, {}, {} job(s), {:.1f} s)".format(
            failed, count, year, args.compiler, max(1, args.jobs),
            time.time() - started,
        )
    )
    return 1 if failed and args.check else 0


if __name__ == "__main__":
    sys.exit(main())
