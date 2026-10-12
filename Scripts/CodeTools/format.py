#!/usr/bin/env python3
from __future__ import annotations

import argparse
import os
import re
import shutil
import subprocess
import sys
import threading
import time
from concurrent.futures import ThreadPoolExecutor, as_completed
from pathlib import Path

DEFAULT_EXTENSIONS = {".c", ".cc", ".h", ".hpp", ".cpp"}
DEFAULT_FORMAT_BY_EXT = "*.c,*.cc,*.h,*.hpp,*.cpp"
_PRINT_LOCK = threading.Lock()


def format_duration(seconds: float) -> str:
    if seconds < 60:
        return f"{seconds:.2f}s"
    total = int(seconds)
    frac = seconds - total
    hours, rem = divmod(total, 3600)
    minutes, secs = divmod(rem, 60)
    sec_str = f"{secs + frac:05.2f}s"
    if hours:
        return f"{hours}h {minutes:02d}m {sec_str}"
    return f"{minutes}m {sec_str}"


def split_alternatives(value: str) -> list[str]:
    return [part.strip() for part in re.split(r"[,|]", value) if part.strip()]


def parse_extensions(value: str | None) -> set[str]:
    if not value:
        return set(DEFAULT_EXTENSIONS)
    result: set[str] = set()
    for part in split_alternatives(value):
        token = part.lower()
        if token.startswith("*"):
            token = token[1:]
        if not token.startswith("."):
            token = f".{token}"
        result.add(token)
    return result or set(DEFAULT_EXTENSIONS)


def compile_exclude(value: str | None) -> re.Pattern[str] | None:
    if not value:
        return None
    pattern = "|".join(split_alternatives(value))
    return re.compile(pattern)


def resolve_config(config_arg: Path) -> Path:
    config_arg = config_arg.resolve()
    if config_arg.is_dir():
        path = config_arg / ".clang-format"
    else:
        path = config_arg
    if not path.is_file():
        raise FileNotFoundError(f"Config file not found: {path}")
    return path


def resolve_clang_format(binary_arg: str | None) -> str:
    """Return the clang-format executable to run.

    Without --with-clang-format the first clang-format found in PATH is used.
    With it, the value is a path to the executable or a command name that is
    looked up in PATH. The value is kept as text: Path("./clang-format") would
    collapse to "clang-format" and be searched in PATH instead of the cwd.
    """
    if binary_arg is None:
        found = shutil.which("clang-format")
        if found is None:
            raise FileNotFoundError(
                "clang-format not found in PATH.\n"
                "Install LLVM/clang-format and ensure it is on PATH, "
                "or pass --with-clang-format PATH."
            )
        return found

    candidate = os.path.expanduser(binary_arg.strip())
    if not candidate:
        raise FileNotFoundError("--with-clang-format needs a path to the clang-format executable.")
    if os.path.isdir(candidate):
        raise FileNotFoundError(
            f"--with-clang-format expects the clang-format executable, not a directory: {candidate}"
        )
    found = shutil.which(candidate)
    if found is None:
        raise FileNotFoundError(f"clang-format not found or not executable: {candidate}")
    return os.path.abspath(found)


def probe_clang_format(clang_format: str) -> str:
    """Run `clang-format --version` and return its text.

    Raises RuntimeError if the binary cannot be started or exits with an error,
    so a broken binary stops the run instead of failing every single file.
    """
    try:
        result = subprocess.run(
            [clang_format, "--version"],
            capture_output=True,
            text=True,
            encoding="utf-8",
            errors="replace",
            check=False,
        )
    except OSError as exc:
        raise RuntimeError(f"Cannot run {clang_format}: {exc}") from exc
    output = (result.stdout or result.stderr).strip()
    if result.returncode != 0:
        raise RuntimeError(f"{clang_format} --version exited with code {result.returncode}:\n{output}")
    return output


def collect_work_units(root: Path) -> list[tuple[Path, bool]]:
    units: list[tuple[Path, bool]] = [(root, False)]
    try:
        children = sorted(p for p in root.iterdir() if p.is_dir())
    except OSError:
        return units
    units.extend((child, True) for child in children)
    return units


def iter_source_files(
    directory: Path,
    recursive: bool,
    extensions: set[str],
) -> list[Path]:
    files: list[Path] = []
    if recursive:
        for path in directory.rglob("*"):
            if path.is_file() and path.suffix.lower() in extensions:
                files.append(path)
    else:
        try:
            for path in directory.iterdir():
                if path.is_file() and path.suffix.lower() in extensions:
                    files.append(path)
        except OSError:
            pass
    return files


def format_one(
    path: Path,
    config: Path,
    check: bool = False,
    clang_format: str = "clang-format",
) -> tuple[str, Path]:
    if check:
        argv = [clang_format, "--dry-run", "-Werror", f"--style=file:{config}", str(path)]
    else:
        argv = [clang_format, "-i", f"--style=file:{config}", str(path)]
    result = subprocess.run(
        argv,
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="replace",
    )
    if result.returncode == 0:
        return "ok", path
    return "failed", path


def process_unit(
    directory: Path,
    recursive: bool,
    config: Path,
    exclude: re.Pattern[str] | None,
    extensions: set[str],
    check: bool = False,
    clang_format: str = "clang-format",
) -> tuple[int, int, int]:
    ok = skipped = failed = 0
    for path in iter_source_files(directory, recursive, extensions):
        path_str = str(path)
        if exclude is not None and exclude.search(path_str):
            skipped += 1
            continue
        status, file_path = format_one(path, config, check, clang_format)
        with _PRINT_LOCK:
            if status == "ok":
                print(f"OK: {file_path}")
                ok += 1
            else:
                print(f"FAILED: {file_path}", file=sys.stderr)
                failed += 1
    return ok, skipped, failed


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Recursively format C/C++ sources with clang-format.",
    )
    parser.add_argument(
        "--dir",
        nargs="+",
        type=Path,
        default=[],
        metavar="DIR",
        help="One or more root directories to walk recursively",
    )
    parser.add_argument(
        "--files",
        nargs="+",
        type=Path,
        default=[],
        metavar="FILE",
        help="One or more individual files to format directly, regardless of extension",
    )
    parser.add_argument(
        "--config",
        required=True,
        type=Path,
        help="Path to .clang-format file, or a directory containing it",
    )
    parser.add_argument(
        "--with-clang-format",
        default=None,
        metavar="PATH",
        help=(
            "clang-format executable to run: a path, or a command name looked up in PATH "
            "(default: clang-format from PATH)"
        ),
    )
    parser.add_argument(
        "--exclude",
        default=None,
        help=r'Path patterns separated by "," or "|", e.g. "3rdparty|Temp" or "3rdparty,Temp"',
    )
    parser.add_argument(
        "--format-by-ext",
        default=DEFAULT_FORMAT_BY_EXT,
        help=f'Extensions separated by "," or "|", e.g. "*.c,*.cpp" (default: {DEFAULT_FORMAT_BY_EXT})',
    )
    parser.add_argument(
        "--jobs",
        "-j",
        type=int,
        default=0,
        help="Worker threads for subdirectories (0 = auto)",
    )
    parser.add_argument(
        "--check",
        action="store_true",
        help="Dry-run: report files that would be reformatted, change nothing",
    )
    return parser


def main() -> int:
    started = time.perf_counter()
    args = build_parser().parse_args()

    try:
        clang_format = resolve_clang_format(args.with_clang_format)
        version_text = probe_clang_format(clang_format)
    except (FileNotFoundError, RuntimeError) as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        return 1

    if not args.dir and not args.files:
        print("ERROR: Pass at least one of --dir or --files.", file=sys.stderr)
        return 1

    roots: list[Path] = []
    for dir_arg in args.dir:
        root = dir_arg.resolve()
        if not root.is_dir():
            print(f"ERROR: Directory not found: {root}", file=sys.stderr)
            return 1
        roots.append(root)

    files: list[Path] = []
    for file_arg in args.files:
        file_path = file_arg.resolve()
        if not file_path.is_file():
            print(f"ERROR: File not found: {file_path}", file=sys.stderr)
            return 1
        files.append(file_path)

    try:
        config = resolve_config(args.config)
    except FileNotFoundError as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        return 1

    extensions = parse_extensions(args.format_by_ext)
    try:
        exclude = compile_exclude(args.exclude)
    except re.error as exc:
        print(f"ERROR: Invalid --exclude pattern: {exc}", file=sys.stderr)
        return 1

    print("clang-format:")
    print(version_text)
    print(f"Directories ({len(roots)}):")
    for root in roots:
        print(f"  - {root}")
    print(f"Files ({len(files)}):")
    for file_path in files:
        print(f"  - {file_path}")
    print(f"Config:    {config}")
    print(f"Binary:    {clang_format}")
    print(f"Exts:      {', '.join(sorted(extensions))}")
    print(f"Exclude:   {args.exclude if args.exclude else '(none)'}")
    print()

    units: list[tuple[Path, bool]] = []
    for root in roots:
        units.extend(collect_work_units(root))
    jobs = args.jobs if args.jobs > 0 else min(32, max(1, (os.cpu_count() or 4) * 2))
    jobs = min(jobs, max(1, len(units) + len(files)))

    total_ok = total_skipped = total_failed = 0
    with ThreadPoolExecutor(max_workers=jobs) as pool:
        futures = [
            pool.submit(process_unit, directory, recursive, config, exclude, extensions, args.check, clang_format)
            for directory, recursive in units
        ]
        file_futures = {
            pool.submit(format_one, file_path, config, args.check, clang_format): file_path
            for file_path in files
            if exclude is None or not exclude.search(str(file_path))
        }
        total_skipped += sum(1 for file_path in files if exclude is not None and exclude.search(str(file_path)))
        for future in as_completed(futures):
            ok, skipped, failed = future.result()
            total_ok += ok
            total_skipped += skipped
            total_failed += failed
        for future in as_completed(file_futures):
            status, file_path = future.result()
            if status == "ok":
                print(f"OK: {file_path}")
                total_ok += 1
            else:
                print(f"FAILED: {file_path}", file=sys.stderr)
                total_failed += 1

    elapsed = time.perf_counter() - started
    print()
    if args.check:
        print(
            f"Done. Conforms: {total_ok}, skipped: {total_skipped}, "
            f"needs formatting: {total_failed}"
        )
    else:
        print(
            f"Done. Formatted: {total_ok}, skipped: {total_skipped}, "
            f"failed: {total_failed}"
        )
    print(f"Elapsed: {format_duration(elapsed)}")
    return 1 if total_failed else 0


if __name__ == "__main__":
    sys.exit(main())
