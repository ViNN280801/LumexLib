"""LLDB batch snapshot for hang_diag.sh — structured crash / hang report."""

from __future__ import annotations

import os
from datetime import datetime, timezone
from typing import Iterable, Optional

import lldb

BANNER_WIDTH = 72


def _banner(title: str) -> None:
    print("\n" + "=" * BANNER_WIDTH, flush=True)
    print(f" {title}", flush=True)
    print("=" * BANNER_WIDTH, flush=True)


def _subbanner(title: str) -> None:
    print(f"\n--- {title} ---", flush=True)


def _println(text: str = "") -> None:
    print(text, flush=True)


def _env_int(name: str, default: int = 0) -> int:
    raw = os.environ.get(name, "")
    if not raw:
        return default
    try:
        return int(raw)
    except ValueError:
        return default


def _env_str(name: str, default: str = "") -> str:
    return os.environ.get(name, default)


def _run(cmd: str) -> None:
    lldb.debugger.HandleCommand(cmd)


def _process_is_stopped(process: lldb.SBProcess) -> bool:
    if not process.IsValid():
        return False
    return process.GetState() == lldb.eStateStopped


def _configure_for_crash_debugging(*, run_mode: bool) -> None:
    """Run mode: pass SIGSEGV so sanitizers can print before exit; stop on SIGABRT."""
    if run_mode:
        _run("process handle SIGSEGV -s false -n true -p true")
        _run("process handle SIGABRT -s true -n true -p true")
    for sig in ("SIGBUS", "SIGILL", "SIGTRAP", "SIGFPE"):
        _run(f"process handle {sig} -s true -n true -p true")
    if not run_mode:
        _run("process handle SIGSEGV -s true -n true -p true")
        _run("process handle SIGABRT -s true -n true -p true")


def _inferior_io_paths() -> tuple[str, str]:
    log = _env_str("HANG_LOG", "/tmp/hang_diag.log")
    base = os.path.splitext(log)[0]
    return f"{base}.inferior.out", f"{base}.inferior.err"


def _setup_inferior_io_redirect() -> tuple[str, str]:
    out_path, err_path = _inferior_io_paths()
    for path in (out_path, err_path):
        with open(path, "w", encoding="utf-8"):
            pass
    _run(f'settings set target.output-path "{out_path}"')
    _run(f'settings set target.error-path "{err_path}"')
    return out_path, err_path


def _read_text_file(path: str) -> Optional[str]:
    try:
        with open(path, encoding="utf-8", errors="replace") as fh:
            return fh.read()
    except OSError as exc:
        return f"(unreadable: {exc})"


def _print_captured_io_files(out_path: str, err_path: str) -> None:
    _banner("inferior stdout/stderr (redirect files)")
    for label, path in (("stdout", out_path), ("stderr", err_path)):
        text = _read_text_file(path)
        _subbanner(label)
        if text is None:
            _println("(missing)")
        elif not text.strip():
            _println("(empty)")
        else:
            _println(text.rstrip())


def _capture_inferior_output(process: lldb.SBProcess) -> None:
    """Fallback when redirect files are not used (live attach)."""
    _banner("inferior stdout/stderr (process pipes)")
    if not process.IsValid():
        _println("(no valid process)")
        return
    for label, getter in (("stdout", process.GetSTDOUT), ("stderr", process.GetSTDERR)):
        try:
            data = getter(8 * 1024 * 1024)
        except Exception as exc:
            _println(f"{label}: unavailable ({exc})")
            continue
        _subbanner(label)
        if not data:
            _println("(empty)")
            continue
        text = data if isinstance(data, str) else data.decode("utf-8", "replace")
        _println(text.rstrip())


def _print_sanitizer_excerpt_from_text(text: str) -> None:
    if not text.strip():
        return
    keys = (
        "MemorySanitizer",
        "AddressSanitizer",
        "UndefinedBehaviorSanitizer",
        "ThreadSanitizer",
        "SUMMARY:",
        "ERROR:",
        "WARNING:",
        "DEADLYSIGNAL",
    )
    hits = [line for line in text.splitlines() if any(k in line for k in keys)]
    if not hits:
        return
    _banner("sanitizer report (extracted)")
    for line in hits[:300]:
        _println(line)


def _print_sanitizer_stack_from_stderr(err_text: str) -> None:
    """Print full MSan/ASan stderr block when present (primary signal for --run)."""
    if not err_text.strip():
        return
    if not any(
        token in err_text
        for token in (
            "MemorySanitizer",
            "AddressSanitizer",
            "UndefinedBehaviorSanitizer",
            "ThreadSanitizer",
        )
    ):
        return
    _banner("sanitizer report (full stderr)")
    _println(err_text.rstrip())


def _print_backtrace_api(process: lldb.SBProcess, depth: int, title: str) -> None:
    _banner(title)
    if not process.IsValid():
        print("(no valid process)")
        return
    if process.GetNumThreads() == 0:
        print("(no threads — process may have exited; see inferior stderr above)")
        return
    for thread in process:
        desc = thread.GetStopDescription(256) or ""
        print(f"\n* thread #{thread.GetIndexID()} ({thread.GetName() or 'unnamed'}) {desc}")
        n = thread.GetNumFrames()
        if n == 0:
            _run(f"thread select {thread.GetIndexID()}")
            _run(f"thread backtrace {depth}")
            continue
        for i in range(min(n, depth)):
            frame = thread.GetFrameAtIndex(i)
            if not frame.IsValid():
                continue
            pc = frame.GetPC()
            fn = frame.GetFunctionName() or frame.GetSymbol().GetName() or "??"
            line = frame.GetLineEntry()
            if line.IsValid():
                print(f"  frame #{i}: {pc:#x} {fn} at {line.GetFileName()}:{line.GetLine()}")
            else:
                print(f"  frame #{i}: {pc:#x} {fn}")


def _read_proc_file(pid: int, rel_path: str, binary: bool = False) -> Optional[str]:
    path = f"/proc/{pid}/{rel_path}"
    try:
        if rel_path == "cwd":
            return os.readlink(path)
        with open(path, "rb" if binary else "r", encoding=None if binary else "utf-8", errors="replace") as fh:
            data = fh.read()
        if binary:
            return data.replace(b"\0", b" ").decode("utf-8", "replace")
        return data
    except OSError as exc:
        print(f"unavailable ({path}): {exc}")
        return None


def _print_live_proc_info(pid: int) -> None:
    _banner("live process (/proc)")
    for label, rel, binary in (
        ("cmdline", "cmdline", True),
        ("cwd", "cwd", False),
        ("environ", "environ", True),
        ("status", "status", False),
        ("stat", "stat", False),
    ):
        _subbanner(f"proc {label}")
        text = _read_proc_file(pid, rel, binary=binary)
        if text is not None:
            print(text.rstrip())


def _print_target_summary(target: lldb.SBTarget, process: Optional[lldb.SBProcess]) -> None:
    _banner("target summary")
    print(f"Target triple     : {target.triple}")
    exe = target.executable
    print(f"Target executable : {exe.fullpath or exe.name}")
    if process and process.IsValid():
        print(f"Process pid       : {process.GetProcessID()}")
        print(f"Process state     : {lldb.SBDebugger.StateAsCString(process.GetState())}")
        thread = process.GetSelectedThread()
        if thread.IsValid():
            print(f"Selected thread   : #{thread.GetIndexID()} ({thread.GetName() or 'unnamed'})")
            print(f"Stop description  : {thread.GetStopDescription(256)}")


def _load_solib(target: lldb.SBTarget, solib: str, *, reload_modules: bool) -> None:
    _banner("shared library search path")
    if not solib:
        print("(none configured)")
        return
    print(f"append target.exec-search-paths: {solib}")
    escaped = solib.replace('"', '\\"')
    _run(f'settings append target.exec-search-paths "{escaped}"')
    if reload_modules:
        _run("target modules load --sync --flush-dcache")
    else:
        print("(module reload deferred until process is stopped)")


def _print_modules(target: lldb.SBTarget) -> None:
    _banner("loaded images")
    for i in range(target.GetNumModules()):
        module = target.GetModuleAtIndex(i)
        if not module.IsValid():
            continue
        path = module.file.fullpath or module.file.name
        sym = " [symbols OK]" if module.GetNumCompileUnits() > 0 else " [no symbols]"
        print(f"  [{i:3d}] {path}{sym}")


def _print_crash_context(process: lldb.SBProcess) -> None:
    _banner("crash context")
    thread = process.GetSelectedThread()
    if not thread.IsValid():
        print("(no selected thread)")
        return
    print(f"Stop reason : {thread.GetStopDescription(512)}")
    frame = thread.GetFrameAtIndex(0)
    if frame.IsValid():
        pc = frame.GetPC()
        print(f"Fault PC    : {pc:#x}")
        fn = frame.GetFunctionName() or frame.GetSymbol().GetName() or "??"
        line = frame.GetLineEntry()
        if line.IsValid():
            print(f"Fault at    : {fn} ({line.GetFileName()}:{line.GetLine()})")
        else:
            print(f"Fault in    : {fn}")


def _print_fault_frame_detail(process: lldb.SBProcess) -> None:
    if not _process_is_stopped(process):
        return
    thread = process.GetSelectedThread()
    if not thread.IsValid():
        return
    _banner("crash: faulting frame")
    process.SetSelectedThread(thread)
    _run("frame select 0")
    _run("frame info")
    _run("frame variable")

    frame = thread.GetFrameAtIndex(0)
    if not frame.IsValid():
        return

    _subbanner("disassembly at fault PC")
    _run("disassemble --frame")

    _subbanner("memory around fault PC")
    pc = frame.GetPC()
    _run(f"memory read --format hex --count 16 --size 8 {pc - 64:#x}")


def _print_registers(process: lldb.SBProcess, all_threads: bool = False) -> None:
    if not _process_is_stopped(process):
        return
    _banner("registers" + (" (all threads)" if all_threads else " (selected thread)"))
    if not all_threads:
        _run("register read")
        return
    for thread in process:
        _subbanner(f"thread #{thread.GetIndexID()} tid={thread.GetThreadID()}")
        process.SetSelectedThread(thread)
        _run("register read")


def _print_thread_backtraces(process: lldb.SBProcess, depth: int, full: bool) -> None:
    if not process.IsValid():
        return
    if not _process_is_stopped(process):
        _print_backtrace_api(process, depth, f"all threads backtrace (depth {depth}, API)")
        return
    if full:
        _banner("all threads backtrace (full)")
        _run("thread backtrace all -c")
    else:
        _banner(f"all threads backtrace (depth {depth})")
        _run(f"thread backtrace all {depth}")


def _print_verbose_threads(process: lldb.SBProcess) -> None:
    if not _process_is_stopped(process):
        return
    _banner("verbose: all threads (frames, args, locals)")
    for thread in process:
        _subbanner(f"thread #{thread.GetIndexID()} — backtrace")
        process.SetSelectedThread(thread)
        _run("thread backtrace -c")
        for frame_idx in range(min(thread.GetNumFrames(), 8)):
            frame = thread.GetFrameAtIndex(frame_idx)
            if not frame.IsValid():
                continue
            _subbanner(f"thread #{thread.GetIndexID()} frame #{frame_idx}")
            process.SetSelectedThread(thread)
            _run(f"frame select {frame_idx}")
            _run("frame info")
            _run("frame variable")


def _print_sanitizer_hints() -> None:
    _banner("sanitizer / stderr hints")
    _println(
        "Under --run, LLDB passes SIGSEGV to the inferior so MSan/ASan can flush reports to stderr.\n"
        "Read sections 'sanitizer report (full stderr)' and '*.inferior.err' beside HANG_LOG.\n"
        "Export MSAN_OPTIONS / ASAN_OPTIONS in the shell before hang_diag.sh (inherited by lldb).\n"
        "MSan stack-overflow may leave frame #0 empty in LLDB; trust the sanitizer stack in stderr."
    )


def _print_header() -> None:
    _banner("hang_diag snapshot")
    print(f"Timestamp (UTC) : {datetime.now(timezone.utc).isoformat()}")
    print(f"HANG_LOG        : {_env_str('HANG_LOG', '?')}")
    print(f"HANG_MODE       : {_env_str('HANG_MODE', '0')} (0=live, 1=core, 2=run)")
    print(f"HANG_VERBOSE    : {_env_str('HANG_VERBOSE', '0')}")
    print(f"HANG_BT_DEPTH   : {_env_str('HANG_BT_DEPTH', '20')}")
    print(f"HANG_SOLIB      : {_env_str('HANG_SOLIB', '') or '(none)'}")


def run_snapshot() -> None:
    hang_mode = _env_int("HANG_MODE", 0)
    verbose = _env_int("HANG_VERBOSE", 0) != 0
    bt_depth = _env_int("HANG_BT_DEPTH", 20)
    solib = _env_str("HANG_SOLIB", "")

    target = lldb.debugger.GetSelectedTarget()
    if not target or not target.IsValid():
        print("ERROR: no valid LLDB target")
        return

    process = target.GetProcess()
    has_process = process.IsValid()

    _print_header()

    _banner("lldb version")
    print(lldb.SBDebugger.GetVersionString())

    _print_target_summary(target, process if has_process else None)

    if hang_mode == 2:
        _configure_for_crash_debugging(run_mode=True)
        _load_solib(target, solib, reload_modules=False)
        out_path, err_path = _setup_inferior_io_redirect()

        _banner("run target")
        _run("run")
        process = target.GetProcess()
        has_process = process.IsValid()
        if has_process:
            _println(
                f"Process state after run: "
                f"{lldb.SBDebugger.StateAsCString(process.GetState())}"
            )
        else:
            _println("ERROR: run did not create a valid process (check binary, loader, sandbox).")
            _banner("DONE")
            return

        err_text = _read_text_file(err_path) or ""
        out_text = _read_text_file(out_path) or ""
        _print_captured_io_files(out_path, err_path)
        _print_sanitizer_stack_from_stderr(err_text)
        _print_sanitizer_excerpt_from_text(err_text + "\n" + out_text)

        if _process_is_stopped(process) and solib:
            _run("target modules load --sync --flush-dcache")
    else:
        _configure_for_crash_debugging(run_mode=False)
        _load_solib(target, solib, reload_modules=has_process and _process_is_stopped(process))

    if hang_mode != 2:
        _banner("process status")
        if has_process and _process_is_stopped(process):
            _run("process status")

        if hang_mode == 0 and has_process and _process_is_stopped(process):
            pid = process.GetProcessID()
            if pid:
                _print_live_proc_info(pid)

    if has_process and _process_is_stopped(process):
        _banner("memory regions (summary)")
        _run("memory region --all --verbose false")
    elif has_process:
        print(f"\n(skipping memory regions — process state is "
              f"{lldb.SBDebugger.StateAsCString(process.GetState())})")

    _print_modules(target)

    _banner("threads")
    if has_process and _process_is_stopped(process):
        _run("thread list")
    elif has_process:
        _print_backtrace_api(process, bt_depth, "threads at exit (API backtrace)")
    else:
        print("(no process)")

    if not has_process:
        _banner("DONE")
        return

    is_crash_like = hang_mode != 0
    stopped = _process_is_stopped(process)

    if is_crash_like and stopped:
        _print_crash_context(process)
        _print_fault_frame_detail(process)
    elif is_crash_like:
        _banner("crash context")
        _println(
            f"Process not stopped (state={lldb.SBDebugger.StateAsCString(process.GetState())}). "
            "For MSan/ASan under --run, the sanitizer report in stderr above is the primary signal."
        )

    if stopped:
        _print_registers(process, all_threads=False)

    if is_crash_like and stopped:
        _banner("crash: faulting thread backtrace (full)")
        _run("thread backtrace -c")
    elif is_crash_like:
        _print_backtrace_api(process, bt_depth, "crash thread backtrace (API)")

    _print_thread_backtraces(process, bt_depth, full=False)

    if verbose:
        if stopped:
            _print_thread_backtraces(process, bt_depth, full=True)
            _print_verbose_threads(process)
            _print_registers(process, all_threads=True)
        else:
            _print_backtrace_api(process, bt_depth, "verbose backtrace (API)")
    elif not is_crash_like and stopped:
        _print_thread_backtraces(process, bt_depth, full=True)

    if is_crash_like:
        _print_sanitizer_hints()

    if hang_mode == 0 and stopped:
        _banner("detach live process")
        _run("process detach")

    _banner("DONE")


def __lldb_init_module(debugger: lldb.SBDebugger, internal_dict) -> None:  # noqa: ANN001
    debugger.HandleCommand(
        "command script add -f hang_diag_helpers.run_snapshot hang_diag_snapshot"
    )


if __name__ == "__main__":
    run_snapshot()
