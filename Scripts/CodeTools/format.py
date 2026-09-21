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


def format_one(path: Path, config: Path, check: bool = False) -> tuple[str, Path]:
    if check:
        argv = ["clang-format", "--dry-run", "-Werror", f"--style=file:{config}", str(path)]
    else:
        argv = ["clang-format", "-i", f"--style=file:{config}", str(path)]
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
) -> tuple[int, int, int]:
    ok = skipped = failed = 0
    for path in iter_source_files(directory, recursive, extensions):
        path_str = str(path)
        if exclude is not None and exclude.search(path_str):
            skipped += 1
            continue
        status, file_path = format_one(path, config, check)
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

    if shutil.which("clang-format") is None:
        print("ERROR: clang-format not found in PATH.", file=sys.stderr)
        print("Install LLVM/clang-format and ensure it is on PATH.", file=sys.stderr)
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

    version = subprocess.run(
        ["clang-format", "--version"],
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="replace",
        check=False,
    )
    print("clang-format:")
    print((version.stdout or version.stderr).strip())
    print(f"Directories ({len(roots)}):")
    for root in roots:
        print(f"  - {root}")
    print(f"Files ({len(files)}):")
    for file_path in files:
        print(f"  - {file_path}")
    print(f"Config:    {config}")
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
            pool.submit(process_unit, directory, recursive, config, exclude, extensions, args.check)
            for directory, recursive in units
        ]
        file_futures = {
            pool.submit(format_one, file_path, config, args.check): file_path
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
