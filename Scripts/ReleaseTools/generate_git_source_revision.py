#!/usr/bin/env python3
"""Write pe_git_source_revision.h and/or a *-source-revision.rev sidecar.

Superproject HEAD plus every `git submodule status --recursive` entry go into
PrivateBuild (short) and Comments (compact path=hash list). The sidecar carries
the full multiline listing for Linux packages (no VERSIONINFO).

Usable from VS PreBuild, CMake (via EmbedGitSourceRevision.cmake), and by hand
on Windows and Linux - same entry point, no .bat/.sh split.

With --binaries-dir the sidecar also lists the binaries of a Distr package: size,
SHA-256 and the keys of each one. Windows: the symbol store key of each .exe/.dll
(PE time stamp and image size) and the .pdb it was linked with (name, GUID, age).
Linux: the Build ID of each ELF file (a symbolic link is listed through its target
only) and the .debug file it names in .gnu_debuglink. And whether --symbols-dir
holds that very .pdb (GUID and age) or .debug (Build ID). A crash dump records
the same keys for every module, so this listing says which build a dump comes
from and which symbols read it, wherever those symbols are kept. Nothing in
that part raises: a missing folder, a missing symbol file or an unreadable file is
reported in the listing and the sidecar is still written.

Examples:
  python generate_git_source_revision.py --start-dir . --header-out pe_git_source_revision.h
  python generate_git_source_revision.py --start-dir . --sidecar-out <engine>-source-revision.rev
  python generate_git_source_revision.py --start-dir . --sidecar-out Distr/<engine>-source-revision.rev \\
      --binaries-dir Distr --symbols-dir Distr_Symbols
"""

from __future__ import annotations

import argparse
import datetime
import hashlib
import re
import struct
import subprocess
import sys
import uuid
from pathlib import Path
from typing import Dict, List, Optional


_SUBMODULE_LINE = re.compile(
    r"^[ +\-U]?([0-9a-fA-F]+)[ \t]+([^ \t]+)"
)


def _as_dir(path: Path) -> Path:
    """Normalize a directory path from VS PreBuild / cmd quoting.

    ``$(ProjectDir)`` ends with a backslash. Quoting it as ``"$(ProjectDir)"``
    makes cmd treat the closing quote as escaped, so Python receives a path
    that ends with a literal ``"`` and ``is_dir()`` fails. Strip leftover
    quotes and accept a trailing ``\\.`` from the safe ``"$(ProjectDir)."`` form.
    """
    text = str(path).strip().strip('"').strip("'")
    return Path(text)


def _run_git(start: Path, *args: str) -> str:
    try:
        completed = subprocess.run(
            ["git", "-C", str(start), *args],
            check=False,
            capture_output=True,
            text=True,
            encoding="utf-8",
            errors="replace",
        )
    except OSError:
        return ""
    if completed.returncode != 0:
        return ""
    return (completed.stdout or "").strip()


def _c_escape(value: str) -> str:
    return (
        value.replace("\\", "\\\\")
        .replace('"', '\\"')
        .replace("\n", " ")
        .replace("\r", "")
    )


def _collect(start_dir: Path) -> tuple[str, str, str, str]:
    """Return (super_hash_or_unavailable, dirty_suffix, comments, submodule_block)."""
    toplevel = _run_git(start_dir, "rev-parse", "--show-toplevel")
    if not toplevel:
        return "unavailable", "", ".=unavailable", ""

    top = Path(toplevel)
    super_hash = _run_git(top, "rev-parse", "HEAD") or "unavailable"
    dirty = ""
    if super_hash != "unavailable":
        porcelain = _run_git(top, "status", "--porcelain")
        if porcelain:
            dirty = "-dirty"

    sub_raw = _run_git(top, "submodule", "status", "--recursive")
    comments = f".={super_hash}{dirty}"
    if sub_raw:
        for line in sub_raw.replace("\r\n", "\n").replace("\r", "\n").split("\n"):
            line = line.strip()
            if not line:
                continue
            match = _SUBMODULE_LINE.match(line)
            if match:
                comments += f"; {match.group(2)}={match.group(1)}"

    return super_hash, dirty, comments, sub_raw


def _cap(text: str, limit: int) -> str:
    if len(text) <= limit:
        return text
    if limit <= 3:
        return text[:limit]
    return text[: limit - 3] + "..."


_PDB_MSF_MAGIC = b"Microsoft C/C++ MSF 7.00\r\n\x1a\x44\x53\x00\x00\x00"
_RSDS = 0x53445352
_IMAGE_DEBUG_TYPE_CODEVIEW = 2


def read_pe_identity(data: bytes) -> Optional[Dict[str, object]]:
    """Time stamp, image size and CodeView record (.pdb name, GUID, age) of a PE image.

    Returns None when ``data`` is not a PE image; ``guid`` is None for an image without a
    CodeView record (a binary linked without /DEBUG, or a third-party DLL).
    """
    if len(data) < 0x40 or data[:2] != b"MZ":
        return None
    pe = struct.unpack_from("<i", data, 0x3C)[0]
    if pe < 0 or pe + 24 > len(data) or struct.unpack_from("<I", data, pe)[0] != 0x00004550:
        return None
    section_count, time_stamp = struct.unpack_from("<H", data, pe + 6)[0], struct.unpack_from("<I", data, pe + 8)[0]
    optional_size = struct.unpack_from("<H", data, pe + 20)[0]
    opt = pe + 24
    if opt + 60 > len(data):
        return None
    image_size = struct.unpack_from("<I", data, opt + 56)[0]
    data_dirs = opt + (112 if struct.unpack_from("<H", data, opt)[0] == 0x20B else 96)

    sections = []
    for index in range(section_count):
        base = opt + optional_size + 40 * index
        if base + 40 > len(data):
            return None
        virtual_size, virtual_address, raw_size, raw_pointer = struct.unpack_from("<IIII", data, base + 8)
        sections.append((virtual_address, max(virtual_size, raw_size), raw_pointer))

    def offset_of(rva: int) -> int:
        for address, size, raw in sections:
            if address <= rva < address + size:
                return rva - address + raw
        return -1

    identity: Dict[str, object] = {"time_stamp": time_stamp, "image_size": image_size, "guid": None}
    if data_dirs + 6 * 8 + 8 > len(data):
        return identity
    debug_rva, debug_size = struct.unpack_from("<II", data, data_dirs + 6 * 8)
    entries = offset_of(debug_rva) if debug_rva else -1
    for index in range(debug_size // 28 if entries >= 0 else 0):
        entry = entries + 28 * index
        if entry + 28 > len(data):
            break
        kind, size, _rva, raw = struct.unpack_from("<IIII", data, entry + 12)
        if kind != _IMAGE_DEBUG_TYPE_CODEVIEW or size < 25 or raw + size > len(data):
            continue
        if struct.unpack_from("<I", data, raw)[0] != _RSDS:
            continue
        identity["guid"] = uuid.UUID(bytes_le=bytes(data[raw + 4 : raw + 20]))
        identity["age"] = struct.unpack_from("<I", data, raw + 20)[0]
        name = bytes(data[raw + 24 : raw + size]).split(b"\x00", 1)[0].decode("utf-8", "replace")
        identity["pdb"] = name.replace("\\", "/").rsplit("/", 1)[-1]
        break
    return identity


def read_pdb_identity(path: Path) -> Optional[Dict[str, object]]:
    """GUID and age of a PDB (MSF 7.00): the first blocks of its stream 1, the PDB info stream.

    Returns None when the file is not a PDB of that format or is cut short.
    """
    with open(str(path), "rb") as handle:
        head = handle.read(56)
        if len(head) < 56 or head[:32] != _PDB_MSF_MAGIC:
            return None
        block_size, _free, block_count, directory_size, _unused, map_block = struct.unpack_from("<6I", head, 32)
        if block_size not in (512, 1024, 2048, 4096) or directory_size == 0 or directory_size > 1 << 26:
            return None

        def read_block(index: int) -> bytes:
            if index >= block_count:
                raise ValueError("block out of range")
            handle.seek(index * block_size)
            return handle.read(block_size)

        directory_blocks = -(-directory_size // block_size)
        block_map = read_block(map_block)
        if directory_blocks * 4 > len(block_map):
            return None
        directory = b"".join(read_block(i) for i in struct.unpack_from("<%dI" % directory_blocks, block_map))
        stream_count = struct.unpack_from("<I", directory, 0)[0]
        if stream_count < 2 or 4 + 4 * stream_count > len(directory):
            return None
        sizes = struct.unpack_from("<%dI" % stream_count, directory, 4)
        # after the sizes come the block lists of the streams in order: skip the list of stream 0
        nil = 0xFFFFFFFF
        stream0_blocks = 0 if sizes[0] == nil else -(-sizes[0] // block_size)
        info_size = sizes[1]
        if info_size < 28 or info_size == nil:
            return None
        first_block = struct.unpack_from("<I", directory, 4 + 4 * stream_count + 4 * stream0_blocks)[0]
        info = read_block(first_block)
        if len(info) < 28:
            return None
        age = struct.unpack_from("<I", info, 8)[0]
        return {"guid": uuid.UUID(bytes_le=bytes(info[12:28])), "age": age}


_ELF_MAGIC = b"\x7fELF"
_SHT_NOBITS = 8


def read_elf_identity(data: bytes) -> Optional[Dict[str, object]]:
    """Build ID, .gnu_debuglink name and whether .debug_info exists, of a little-endian ELF file.

    The ELF counterpart of ``read_pe_identity``: a Release binary built with debug symbols carries
    a Build ID (``.note.gnu.build-id``) and names its split-off debug file in ``.gnu_debuglink``;
    that file carries the same Build ID. Returns None when ``data`` is not such an ELF file; the
    values are empty for a section the file does not have (a stripped binary).
    """
    if len(data) < 64 or data[:4] != _ELF_MAGIC or data[5] != 1:
        return None
    if data[4] not in (1, 2):
        return None
    wide = data[4] == 2
    if wide:
        sh_offset, sh_entry, sh_count, sh_names = (
            struct.unpack_from("<Q", data, 0x28)[0],
            struct.unpack_from("<H", data, 0x3A)[0],
            struct.unpack_from("<H", data, 0x3C)[0],
            struct.unpack_from("<H", data, 0x3E)[0],
        )
    else:
        sh_offset, sh_entry, sh_count, sh_names = (
            struct.unpack_from("<I", data, 0x20)[0],
            struct.unpack_from("<H", data, 0x2E)[0],
            struct.unpack_from("<H", data, 0x30)[0],
            struct.unpack_from("<H", data, 0x32)[0],
        )
    identity: Dict[str, object] = {"build_id": "", "debuglink": "", "has_debug_info": False}
    minimum = 40 if wide else 28
    if sh_offset == 0 or sh_count == 0 or sh_entry < minimum or sh_offset + sh_entry * sh_count > len(data):
        return identity

    def section(index: int):
        base = sh_offset + sh_entry * index
        name, kind = struct.unpack_from("<II", data, base)
        if wide:
            offset, size = struct.unpack_from("<QQ", data, base + 24)
        else:
            offset, size = struct.unpack_from("<II", data, base + 16)
        return name, kind, offset, size

    if sh_names >= sh_count:
        return identity
    _n, _k, names_offset, names_size = section(sh_names)
    if names_offset + names_size > len(data):
        return identity
    names = data[names_offset : names_offset + names_size]
    for index in range(sh_count):
        name_at, kind, offset, size = section(index)
        end = names.find(b"\x00", name_at)
        name = names[name_at : end if end >= 0 else len(names)].decode("ascii", "replace")
        if name == ".debug_info":
            identity["has_debug_info"] = True
        if kind == _SHT_NOBITS or offset + size > len(data):
            continue
        body = data[offset : offset + size]
        if name == ".note.gnu.build-id" and len(body) >= 16:
            name_size, desc_size, note_kind = struct.unpack_from("<III", body, 0)
            start = 12 + ((name_size + 3) & ~3)
            if note_kind == 3 and body[12 : 12 + name_size].rstrip(b"\x00") == b"GNU":
                identity["build_id"] = body[start : start + desc_size].hex()
        elif name == ".gnu_debuglink" and body:
            identity["debuglink"] = body.split(b"\x00", 1)[0].decode("utf-8", "replace")
    return identity


def _symbol_key(guid: uuid.UUID, age: int) -> str:
    return "%s%x" % (guid.hex.upper(), age)


def _is_elf_file(path: Path) -> bool:
    try:
        with open(str(path), "rb") as handle:
            return handle.read(4) == _ELF_MAGIC
    except OSError:
        return False


def describe_binaries(binaries_dir: Path, symbols_dir: Optional[Path]) -> List[str]:
    """Lines for the sidecar: the identity of every binary of ``binaries_dir`` and of its symbols.

    Windows: every .exe/.dll and the .pdb it was linked with. Linux: every ELF file that is not a
    symbolic link (the soname chain of a library is listed once) and the .debug file it names.
    Never raises; what cannot be read is said in the line of the file.
    """
    stamp = datetime.datetime.now(datetime.timezone.utc).strftime("%Y-%m-%d %H:%M:%S")
    lines = ["binaries (symbol keys; a dump records the same keys)"]
    lines.append(f"  listed {stamp} UTC")
    symbols_note = "no symbols folder given"
    if symbols_dir is not None:
        symbols_note = f"symbols folder {symbols_dir.name}" + ("" if symbols_dir.is_dir() else " (missing)")
    lines.append(f"  {symbols_note}")
    try:
        files = sorted(
            (p for p in binaries_dir.iterdir() if _is_listed_binary(p)),
            key=lambda p: p.name.lower(),
        )
    except OSError as error:
        lines.append(f"  no listing: cannot read {binaries_dir.name}: {error}")
        return lines
    if not files:
        lines.append(f"  none: no binaries in {binaries_dir.name}")
        return lines
    for path in files:
        lines.append(f"{path.name}")
        try:
            lines.extend(_describe_one(path, symbols_dir))
        except Exception as error:  # one odd file must not hide the others
            lines.append(f"  unreadable: {error}")
    return lines


def _is_listed_binary(path: Path) -> bool:
    if path.is_symlink() or not path.is_file():
        return False
    if path.suffix.lower() in (".exe", ".dll"):
        return True
    return path.suffix != ".debug" and _is_elf_file(path)


def _describe_one(path: Path, symbols_dir: Optional[Path]) -> List[str]:
    data = path.read_bytes()
    lines = [f"  size {len(data)} bytes, sha256 {hashlib.sha256(data).hexdigest()}"]
    if data[:4] == _ELF_MAGIC:
        return lines + _describe_elf(data, symbols_dir)
    identity = read_pe_identity(data)
    if identity is None:
        return lines + ["  not a PE image"]
    lines.append("  binary {0}/{1:08X}{2:x}".format(path.name, identity["time_stamp"], identity["image_size"]))
    guid = identity["guid"]
    if guid is None:
        return lines + ["  pdb none recorded in the file"]
    name, age = identity["pdb"], identity["age"]
    status = _symbols_status(symbols_dir, name, guid, age)
    lines.append(f"  pdb {name}/{_symbol_key(guid, age)} (GUID {str(guid).upper()}, age {age}) - {status}")
    return lines


def _symbols_status(symbols_dir: Optional[Path], name: object, guid: uuid.UUID, age: int) -> str:
    if symbols_dir is None or not symbols_dir.is_dir():
        return "kept elsewhere"
    candidate = symbols_dir / str(name)
    if not candidate.is_file():
        return "not in the symbols folder"
    try:
        found = read_pdb_identity(candidate)
    except (OSError, ValueError, struct.error):
        found = None
    if found is None:
        return "in the symbols folder, not verified"
    if found["guid"] == guid and found["age"] == age:
        return "in the symbols folder, GUID and age match"
    return "in the symbols folder but it is another build: {0}/{1}".format(str(found["guid"]).upper(), found["age"])


def _describe_elf(data: bytes, symbols_dir: Optional[Path]) -> List[str]:
    identity = read_elf_identity(data)
    if identity is None:
        return ["  not a little-endian ELF file"]
    build_id, link = str(identity["build_id"]), str(identity["debuglink"])
    lines = ["  build-id " + build_id if build_id else "  build-id none recorded in the file"]
    if not link:
        return lines + ["  debug none named in the file (.gnu_debuglink)"]
    key = ".build-id/%s/%s.debug" % (build_id[:2], build_id[2:]) if build_id else "no key without a Build ID"
    lines.append(f"  debug {link} ({key}) - {_elf_symbols_status(symbols_dir, link, build_id)}")
    return lines


def _elf_symbols_status(symbols_dir: Optional[Path], link: str, build_id: str) -> str:
    if symbols_dir is None or not symbols_dir.is_dir():
        return "kept elsewhere"
    candidate = symbols_dir / link
    if not candidate.is_file():
        return "not in the symbols folder"
    try:
        found = read_elf_identity(candidate.read_bytes())
    except OSError:
        found = None
    if found is None or not found["build_id"] or not build_id:
        return "in the symbols folder, not verified"
    if found["build_id"] == build_id:
        return "in the symbols folder, Build ID matches"
    return "in the symbols folder but it is another build: " + str(found["build_id"])


def write_header(path: Path, private: str, comments: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    body = (
        "// Generated by generate_git_source_revision.py - do not edit.\n"
        "#pragma once\n"
        f'#define PE_GIT_PRIVATE_BUILD "{_c_escape(private)}"\n'
        f'#define PE_GIT_COMMENTS "{_c_escape(comments)}"\n'
    )
    # Python 3.8 Path.write_text has no newline=; keep LF on all hosts.
    path.write_bytes(body.encode("utf-8"))
    print(f"generate_git_source_revision: header -> {path}", flush=True)


def write_sidecar(
    path: Path,
    super_hash: str,
    dirty: str,
    toplevel: str,
    sub_raw: str,
    binaries: Optional[List[str]] = None,
) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    lines = [
        "source-revision",
        f"superproject {super_hash}{dirty}",
    ]
    if toplevel:
        lines.append(f"toplevel {toplevel}")
    if sub_raw:
        lines.append("submodules")
        lines.append(sub_raw)
    else:
        lines.append("submodules (none or unavailable)")
    if binaries:
        lines.extend(binaries)
    path.write_bytes(("\n".join(lines) + "\n").encode("utf-8"))
    print(f"generate_git_source_revision: sidecar -> {path}", flush=True)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--start-dir",
        type=Path,
        default=None,
        help="Any directory inside the git work tree",
    )
    parser.add_argument("--header-out", type=Path, default=None)
    parser.add_argument("--sidecar-out", type=Path, default=None)
    parser.add_argument("--max-comments-chars", type=int, default=1400)
    parser.add_argument(
        "--binaries-dir",
        type=Path,
        default=None,
        help="Distr folder: list the identity of its .exe/.dll files in the sidecar",
    )
    parser.add_argument(
        "--symbols-dir",
        type=Path,
        default=None,
        help="Folder of the .pdb files, to say which listed binary has its symbols there",
    )
    # Positional project dir for VS PreBuild: python script.py "$(ProjectDir)"
    parser.add_argument(
        "project_dir",
        nargs="?",
        type=Path,
        default=None,
        help="Optional project dir; implies --start-dir and --header-out there",
    )
    args = parser.parse_args(argv)

    start = args.start_dir
    header_out = args.header_out
    sidecar_out = args.sidecar_out

    if args.project_dir is not None:
        project = _as_dir(args.project_dir)
        if start is None:
            start = project
        if header_out is None:
            header_out = project / "pe_git_source_revision.h"

    if start is None:
        print("ERROR: pass --start-dir or a project_dir", file=sys.stderr)
        return 1
    start = _as_dir(start)
    if not start.is_dir():
        print(f"ERROR: start dir does not exist: {start}", file=sys.stderr)
        return 1
    if header_out is None and sidecar_out is None:
        print("ERROR: pass --header-out and/or --sidecar-out (or a project_dir)", file=sys.stderr)
        return 1

    super_hash, dirty, comments, sub_raw = _collect(start.resolve())
    comments = _cap(comments, args.max_comments_chars)
    private = f"{super_hash}{dirty}"
    toplevel = _run_git(start.resolve(), "rev-parse", "--show-toplevel")

    if header_out is not None:
        write_header(header_out, private, comments)
    if sidecar_out is not None:
        binaries = None
        if args.binaries_dir is not None:
            symbols = _as_dir(args.symbols_dir) if args.symbols_dir is not None else None
            try:
                binaries = describe_binaries(_as_dir(args.binaries_dir), symbols)
            except Exception as error:  # the listing is extra: the revision lines must still be written
                binaries = [f"binaries not listed: {error}"]
        write_sidecar(sidecar_out, super_hash, dirty, toplevel, sub_raw, binaries)
    return 0


if __name__ == "__main__":
    sys.exit(main())
