#!/usr/bin/env python3
from __future__ import annotations

import argparse
import locale
import os
import re
import sys
import threading
import time
from concurrent.futures import ThreadPoolExecutor, as_completed
from pathlib import Path

DEFAULT_EXTENSIONS = {".c", ".cc", ".h", ".hpp", ".cpp"}
DEFAULT_FORMAT_BY_EXT = "*.c,*.cc,*.h,*.hpp,*.cpp"
_PRINT_LOCK = threading.Lock()
_DESTRUCTOR_TILDE = re.compile(
    r"(virtual\s+)[\u203e~]([A-Za-z_]\w*)|(::)[\u203e~]([A-Za-z_]\w*)"
)


def restore_destructor_tildes(text: str) -> str:
    def repl(match: re.Match[str]) -> str:
        if match.group(1) is not None:
            return f"{match.group(1)}~{match.group(2)}"
        return f"{match.group(3)}~{match.group(4)}"

    return _DESTRUCTOR_TILDE.sub(repl, text)


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


def candidate_encodings() -> list[str]:
    encodings = [
        "utf-8-sig",
        "utf-8",
        "utf-16",
        "utf-16-le",
        "utf-16-be",
        "cp1251",
        "cp866",
        "cp1252",
        "latin-1",
    ]
    preferred = locale.getpreferredencoding(False) or ""
    if preferred and preferred.lower() not in {e.lower() for e in encodings}:
        encodings.insert(2, preferred)
    return encodings


def detect_and_decode(data: bytes) -> tuple[str, str]:
    if not data:
        return "", "utf-8"

    if data.startswith(b"\xef\xbb\xbf"):
        return data.decode("utf-8-sig"), "utf-8-sig"
    if data.startswith(b"\xff\xfe") or data.startswith(b"\xfe\xff"):
        return data.decode("utf-16"), "utf-16"

    try:
        from charset_normalizer import from_bytes

        best = from_bytes(data).best()
        if best is not None and best.encoding:
            text = str(best)
            return text, best.encoding
    except Exception:
        pass

    try:
        import chardet

        guess = chardet.detect(data)
        enc = (guess.get("encoding") or "").strip()
        if enc:
            try:
                return data.decode(enc), enc
            except (LookupError, UnicodeDecodeError):
                pass
    except Exception:
        pass

    for enc in candidate_encodings():
        try:
            return data.decode(enc), enc
        except UnicodeDecodeError:
            continue

    return data.decode("utf-8", errors="replace"), "utf-8-replace"


def convert_one(path: Path) -> tuple[str, Path, str]:
    try:
        data = path.read_bytes()
    except OSError as exc:
        return "failed", path, str(exc)

    try:
        text, detected = detect_and_decode(data)
    except Exception as exc:
        return "failed", path, str(exc)

    text = restore_destructor_tildes(text)
    out = text.encode("utf-8")

    if data == out:
        return "unchanged", path, detected

    if data.startswith(b"\xef\xbb\xbf") and out == data[len(b"\xef\xbb\xbf") :]:
        detail = "utf-8-sig->utf-8"
    else:
        detail = detected

    try:
        path.write_bytes(out)
    except OSError as exc:
        return "failed", path, str(exc)
    return "converted", path, detail


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


def process_unit(
    directory: Path,
    recursive: bool,
    exclude: re.Pattern[str] | None,
    extensions: set[str],
) -> tuple[int, int, int, int]:
    converted = unchanged = skipped = failed = 0
    for path in iter_source_files(directory, recursive, extensions):
        path_str = str(path)
        if exclude is not None and exclude.search(path_str):
            skipped += 1
            continue
        status, file_path, detail = convert_one(path)
        with _PRINT_LOCK:
            if status == "converted":
                print(f"UTF-8: {file_path}  ({detail})")
                converted += 1
            elif status == "unchanged":
                print(f"OK: {file_path}  (already {detail})")
                unchanged += 1
            else:
                print(f"FAILED: {file_path}  ({detail})", file=sys.stderr)
                failed += 1
    return converted, unchanged, skipped, failed


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Re-save source files as UTF-8 (detect native encoding).",
    )
    parser.add_argument(
        "--dir",
        required=True,
        type=Path,
        help="Root directory to walk recursively",
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
    return parser


def main() -> int:
    started = time.perf_counter()
    args = build_parser().parse_args()

    root = args.dir.resolve()
    if not root.is_dir():
        print(f"ERROR: Directory not found: {root}", file=sys.stderr)
        return 1

    extensions = parse_extensions(args.format_by_ext)
    try:
        exclude = compile_exclude(args.exclude)
    except re.error as exc:
        print(f"ERROR: Invalid --exclude pattern: {exc}", file=sys.stderr)
        return 1

    print(f"Directory: {root}")
    print(f"Exts:      {', '.join(sorted(extensions))}")
    print(f"Exclude:   {args.exclude if args.exclude else '(none)'}")
    print()

    units = collect_work_units(root)
    jobs = args.jobs if args.jobs > 0 else min(32, max(1, (os.cpu_count() or 4) * 2))
    jobs = min(jobs, max(1, len(units)))

    total_converted = total_unchanged = total_skipped = total_failed = 0
    with ThreadPoolExecutor(max_workers=jobs) as pool:
        futures = [
            pool.submit(process_unit, directory, recursive, exclude, extensions)
            for directory, recursive in units
        ]
        for future in as_completed(futures):
            converted, unchanged, skipped, failed = future.result()
            total_converted += converted
            total_unchanged += unchanged
            total_skipped += skipped
            total_failed += failed

    elapsed = time.perf_counter() - started
    print()
    print(
        f"Done. Converted: {total_converted}, unchanged: {total_unchanged}, "
        f"skipped: {total_skipped}, failed: {total_failed}"
    )
    print(f"Elapsed: {format_duration(elapsed)}")
    return 1 if total_failed else 0


if __name__ == "__main__":
    sys.exit(main())
