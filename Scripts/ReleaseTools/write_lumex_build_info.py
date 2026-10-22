#!/usr/bin/env python3
"""Write LumexLib-build-info.json: one build report for all the libraries.

CMakeRoutines' GenerateBuildInfo.py writes one report per target. For the
libraries of one LumexLib build almost all of it is the same (compiler, flags,
CMake, git, host, tools): of 126 fields only the target and the binary differ.
This script runs GenerateBuildInfo.build_report once for the common part and
adds a "libraries" list with, per library, the "target" section of its own
configure context and the "binary" section of its file (size, SHA-256,
SHA-512, format, modification time).

  python write_lumex_build_info.py --utils-dir <CMakeRoutines/utils>
      --source-dir <LumexLib> --binary-dir <build tree> --output <file>
      <library files...>

A library file maps to its target by name (LumexCore_utility.dll,
libLumexCore_utility.so.2.0.3 -> LumexCore_utility) and needs the context
<binary dir>/<target>-build-info-context.json the configure step wrote;
other files are skipped. Symbolic links are skipped (the soname chain of a
library is listed once). Exit status: 0 written, 1 nothing to write or a
missing input, 2 bad arguments.
"""

from __future__ import annotations

import argparse
import importlib.util
import json
import re
import sys
from pathlib import Path
from typing import Any, Dict, List, Optional, Tuple

_LIBRARY = re.compile(r"^(?:lib)?(Lumex[A-Za-z0-9_]+)\.(?:dll|so|dylib)")


def load_generator(utils_dir: Path):
    """Import CMakeRoutines' GenerateBuildInfo.py from utils_dir."""
    path = utils_dir / "GenerateBuildInfo.py"
    spec = importlib.util.spec_from_file_location("lumex_generate_build_info", path)
    if spec is None or spec.loader is None:
        raise ImportError(f"cannot import {path}")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def library_target(path: Path) -> Optional[str]:
    match = _LIBRARY.match(path.name)
    return match.group(1) if match else None


def target_section(context: Dict[str, Any]) -> Dict[str, Any]:
    """The "target" section of GenerateBuildInfo.build_report, from one context."""
    return {
        "name": context.get("target_name", ""),
        "type": context.get("target_type", ""),
        "output_name": context.get("target_output_name", ""),
        "output_dir": context.get("target_output_dir", ""),
        "compile_options": context.get("target_compile_options", []),
        "link_options": context.get("target_link_options", []),
        "compile_definitions": context.get("target_compile_definitions", []),
        "include_dirs": context.get("target_include_dirs", []),
        "link_libraries": context.get("target_link_libraries", []),
        "position_independent": context.get("target_pic", ""),
        "cxx_standard": context.get("target_cxx_standard", ""),
    }


def collect(binary_dir: Path, files: List[Path]) -> List[Tuple[Path, Dict[str, Any]]]:
    """(library file, its configure context) for every library of this build."""
    found = []
    for path in sorted(files, key=lambda p: p.name.lower()):
        if path.is_symlink() or not path.is_file():
            continue
        target = library_target(path)
        if target is None:
            continue
        context_file = binary_dir / f"{target}-build-info-context.json"
        if not context_file.is_file():
            continue
        with open(context_file, encoding="utf-8") as handle:
            context = json.load(handle)
        context["_context_file"] = str(context_file)
        found.append((path, context))
    return found


def build(generator, source_dir: Path, libraries) -> Dict[str, Any]:
    first_path, first_context = libraries[0]
    report = generator.build_report(first_context, str(source_dir), None)
    report.pop("target", None)
    report.pop("binary", None)
    report["meta"].pop("context_file", None)
    report["meta"]["format"] = "json"
    report["meta"]["writer"] = "write_lumex_build_info.py"
    report["libraries"] = [
        {
            "target": target_section(context),
            "binary": generator.collect_binary_info(str(path)),
        }
        for path, context in libraries
    ]
    return report


def main(argv: Optional[List[str]] = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--utils-dir", type=Path, required=True)
    parser.add_argument("--source-dir", type=Path, required=True)
    parser.add_argument("--binary-dir", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("files", type=Path, nargs="*")
    args = parser.parse_args(argv)

    libraries = collect(args.binary_dir, args.files)
    if not libraries:
        print("write_lumex_build_info: no library of this build among the files",
              file=sys.stderr)
        return 1
    try:
        generator = load_generator(args.utils_dir)
    except (ImportError, OSError) as error:
        print(f"write_lumex_build_info: {error}", file=sys.stderr)
        return 1
    report = build(generator, args.source_dir, libraries)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with open(args.output, "w", encoding="utf-8") as handle:
        json.dump(report, handle, indent=2, ensure_ascii=False, default=str)
        handle.write("\n")
    print(f"write_lumex_build_info: {len(libraries)} libraries -> {args.output}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
