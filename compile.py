#!/usr/bin/env python3
"""
Cross-platform CMake build system wrapper with colored logging.

This script provides a clean interface to build CMake projects with various
configuration options while handling errors gracefully.
"""

from os import X_OK as os_X_OK
from os import walk as os_walk
from os import chdir as os_chdir
from os import access as os_access
from os import execve as os_execve
from os import remove as os_remove
from os import listdir as os_listdir
from os import scandir as os_scandir
from os import environ as os_environ
from os import makedirs as os_makedirs
from os.path import join as os_path_join
from os.path import pathsep as os_pathsep
from os.path import isdir as os_path_isdir
from os.path import isfile as os_path_isfile
from os.path import exists as os_path_exists
from os.path import abspath as os_path_abspath
from os.path import dirname as os_path_dirname
from os.path import basename as os_path_basename
from os.path import expanduser as os_path_expanduser

from json import load as json_load
from json import dump as json_dump
from json import loads as json_loads
from json import dumps as json_dumps

from re import sub as re_sub
from re import search as re_search
from re import escape as re_escape
from re import compile as re_compile
from re import findall as re_findall
from re import DOTALL as re_DOTALL
from re import MULTILINE as re_MULTILINE
from re import IGNORECASE as re_IGNORECASE

import sys
from sys import exit as sys_exit
from sys import argv as sys_argv
from sys import executable as sys_executable

from shutil import copy2 as shutil_copy2
from shutil import rmtree as shutil_rmtree

from signal import signal as signal_signal
from signal import SIGINT as signal_SIGINT

from logging import INFO as logging_INFO
from argparse import ArgumentParser as argparse_ArgumentParser

from platform import system as platform_system
from platform import machine as platform_machine
from platform import release as platform_release
from platform import processor as platform_processor

from tempfile import mkdtemp as tempfile_mkdtemp
from tempfile import NamedTemporaryFile as tempfile_NamedTemporaryFile

from html import escape as html_escape

import importlib.util

from time import perf_counter as time_perf_counter
from typing import TYPE_CHECKING, Any, Callable, Dict, List, Optional, Tuple

# Metadata backend for "installed packages" (Python 3.7+ support: prefer importlib.metadata, fallback to pkg_resources).
_metadata_distributions_fn = None  # type: Optional[Any]
try:
    from importlib.metadata import distributions as _metadata_distributions_fn
except ImportError:
    pass
# Use the module (not working_set): we create a fresh WorkingSet() on each check so that
# packages installed by pip in the same process are visible (global working_set is stale).
_pkg_resources_module = None  # type: Optional[Any]
if _metadata_distributions_fn is None:
    try:
        import pkg_resources as _pkg_resources_module
    except ImportError:
        pass
_HAS_METADATA_BACKEND = (
    _metadata_distributions_fn is not None or _pkg_resources_module is not None
)

# Required packages for this script (no external requirements.txt).
REQUIRED_PACKAGES = [
    "colorlog",
    "matplotlib",
    "numpy",
    "psutil",
    "PyQt5",
    "PyQt5_sip",
]


def _normalize_pkg_name(name: str) -> str:
    """PEP 503: lowercase, replace underscore with hyphen."""
    if not name:
        return ""
    return name.strip().lower().replace("_", "-")


def _iter_installed_distributions():
    """Yield (normalized_name, display_name) for each installed distribution. No yield if no backend."""
    if _metadata_distributions_fn is not None:
        try:
            for dist in _metadata_distributions_fn():
                name = dist.metadata.get("Name")
                if name:
                    norm = _normalize_pkg_name(name)
                    if norm:
                        yield norm, name
        except Exception:
            pass
        return
    if _pkg_resources_module is not None:
        try:
            # Fresh WorkingSet() rescans sys.path; global working_set is stale after pip install in same process.
            ws = _pkg_resources_module.WorkingSet()
            for dist in ws:
                try:
                    name = getattr(dist, "project_name", None) or getattr(
                        dist, "key", None
                    )
                    if name:
                        norm = _normalize_pkg_name(name)
                        if norm:
                            yield norm, name
                except Exception:
                    pass
        except Exception:
            pass


class _BootstrapDependencyChecker:
    """Checks required packages against current Python (stdlib only, no colorlog/psutil)."""

    def __init__(self, script_dir: str, required_list: List[str]) -> None:
        self._script_dir = script_dir
        self._required_list = list(required_list)

    def _get_installed_normalized_map(self) -> dict:
        out: dict = {}
        if not _HAS_METADATA_BACKEND:
            return out
        try:
            for norm, name in _iter_installed_distributions():
                out[norm] = name
        except Exception:
            pass
        return out

    def check(self) -> Tuple[List[str], List[str], List[str]]:
        required = self._required_list
        if not required:
            return required, [], []
        installed_map = self._get_installed_normalized_map()
        installed_list: List[str] = []
        missing_list: List[str] = []
        for pkg in required:
            norm = _normalize_pkg_name(pkg)
            if norm and norm in installed_map:
                installed_list.append(pkg)
            else:
                missing_list.append(pkg)
        return required, installed_list, missing_list


def _bootstrap_run_pip_install(
    script_dir: str,
    python_exe: str,
    args: List[str],
    description: str,
) -> bool:
    from subprocess import run as _subprocess_run
    from subprocess import TimeoutExpired as _TimeoutExpired

    cmd = [python_exe, "-m", "pip", "install"] + args
    try:
        r = _subprocess_run(
            cmd,
            cwd=script_dir,
            timeout=300,
        )
        if r.returncode != 0:
            print(
                "ERROR pip install ({}): exited with code {}".format(
                    description, r.returncode
                )
            )
            return False
        return True
    except _TimeoutExpired:
        print("ERROR pip install timed out: {}".format(description))
        return False
    except FileNotFoundError:
        print("ERROR pip not found for {}. Ensure pip is installed.".format(python_exe))
        return False
    except Exception as e:
        print("ERROR pip install ({}): {}".format(description, e))
        return False


def _bootstrap_dependency_console(
    script_dir: str, run_command: Optional[str] = None
) -> None:
    """Interactive dependency helper (stdlib only). Runs until user exits."""
    if run_command is None:
        run_command = "{} {}".format(sys_executable, " ".join(sys_argv))
    checker = _BootstrapDependencyChecker(script_dir, REQUIRED_PACKAGES)
    compile_script = os_path_join(script_dir, "compile.py")

    def print_lists() -> None:
        required, installed, missing = checker.check()
        print("Required for this program:", required if required else "(none)")
        print("Installed (from required):", installed if installed else "(none)")
        print("Missing:", missing if missing else "(none)")

    def install_system() -> bool:
        required, _, missing = checker.check()
        missing_pip = [m for m in missing if " (Python 3.8+ required)" not in m]
        if not missing and not missing_pip:
            print("All required packages are already installed.")
            return True
        if missing and not missing_pip:
            print(
                "Missing: {} (upgrade to Python 3.8+ or install backport: pip install importlib_metadata).".format(
                    ", ".join(m for m in missing if " (Python 3.8+ required)" in m)
                )
            )
            return True
        if not missing_pip:
            return True
        print("Installing missing packages into system Python:", sys_executable)
        return _bootstrap_run_pip_install(
            script_dir, sys_executable, list(missing_pip), "install (system)"
        )

    def create_venv() -> bool:
        required, _, missing = checker.check()
        if not missing and required:
            print("All required packages are already installed.")
            return True
        venv_dir = os_path_join(script_dir, ".venv")
        if os_path_exists(venv_dir):
            print(".venv already exists:", venv_dir)
            try:
                inp = (
                    input("Use existing .venv and install packages? [y/N]: ")
                    .strip()
                    .lower()
                )
            except (EOFError, KeyboardInterrupt):
                return False
            if inp not in ("y", "yes"):
                return False
        else:
            from subprocess import TimeoutExpired as _TimeoutExpired
            from subprocess import run as _subprocess_run

            try:
                r = _subprocess_run(
                    [sys_executable, "-m", "venv", venv_dir],
                    cwd=script_dir,
                    timeout=60,
                )
                if r.returncode != 0:
                    print("ERROR venv creation: exited with code", r.returncode)
                    return False
            except FileNotFoundError:
                print("ERROR venv module not found. Use Python 3.3+ with venv.")
                return False
            except _TimeoutExpired:
                print("ERROR venv creation timed out.")
                return False
            except Exception as e:
                print("ERROR venv creation:", e)
                return False

        if platform_system() == "Windows":
            pip_exe = os_path_join(venv_dir, "Scripts", "python.exe")
        else:
            pip_exe = os_path_join(venv_dir, "bin", "python")
        if not os_path_exists(pip_exe):
            print("ERROR venv Python not found:", pip_exe)
            return False
        _, _, missing = checker.check()
        to_install = [
            m
            for m in (list(missing) if missing else list(REQUIRED_PACKAGES))
            if " (Python 3.8+ required)" not in m
        ]
        if not to_install:
            return True
        return _bootstrap_run_pip_install(
            script_dir, pip_exe, to_install, "install into .venv"
        )

    def upgrade_pip() -> bool:
        return _bootstrap_run_pip_install(
            script_dir, sys_executable, ["--upgrade", "pip"], "upgrade pip"
        )

    def run_script() -> None:
        if not os_path_exists(compile_script):
            print("ERROR script not found:", compile_script)
            return
        from subprocess import run as _subprocess_run

        cmd = [sys_executable, compile_script] + sys_argv[1:]
        try:
            _subprocess_run(cmd, cwd=script_dir)
        except KeyboardInterrupt:
            print("Run interrupted by user.")
        except Exception as e:
            print("Run failed:", e)

    def menu() -> None:
        print()
        print("--- Dependency helper (compile.py) ---")
        print("  1 - Check dependencies (required / installed / missing)")
        print("  2 - Install missing packages into system Python")
        print("  3 - Create .venv next to script and install packages")
        print("  4 - Upgrade pip")
        print('  5 - Run "{}"'.format(run_command))
        print("  0 - Exit")
        print()

    interrupted = False

    def sigint_handler(signum: int, frame: Any) -> None:
        nonlocal interrupted
        interrupted = True
        print("\nInterrupted (Ctrl+C). Use 0 to exit.")

    try:
        signal_signal(signal_SIGINT, sigint_handler)
    except (ValueError, OSError):
        pass

    print_lists()
    while True:
        if interrupted:
            interrupted = False
        menu()
        try:
            choice = input("Choice [0-5]: ").strip()
        except (EOFError, KeyboardInterrupt):
            print("Exiting.")
            break
        if not choice:
            choice = "0"
        if choice == "0":
            print("Exiting.")
            break
        if choice == "1":
            print_lists()
        elif choice == "2":
            install_system()
        elif choice == "3":
            create_venv()
        elif choice == "4":
            upgrade_pip()
        elif choice == "5":
            run_script()
        else:
            print("Unknown choice. Enter 0-5.")


# Run dependency check when script is executed; if something is missing, start interactive helper and exit.
# Skip entirely when frozen (PyInstaller): all deps are bundled; interactive helper uses input() which fails without stdin (--windowed).
if __name__ == "__main__":
    if getattr(sys, "frozen", False):
        pass
    else:
        _script_dir = os_path_dirname(os_path_abspath(__file__))
        _checker = _BootstrapDependencyChecker(_script_dir, REQUIRED_PACKAGES)
        _required, _installed, _missing = _checker.check()
        # Only block on missing deps when we have a metadata backend (importlib.metadata or pkg_resources).
        # On Python 3.7 without setuptools we proceed and let ImportError occur if a package is missing.
        if _HAS_METADATA_BACKEND and _missing:
            if os_environ.get("CI") or os_environ.get("GITHUB_ACTIONS"):
                print(
                    "ERROR: Missing required Python packages for compile.py:",
                    ", ".join(_missing),
                )
                print(
                    "Install with: python -m pip install -r compile.py-requirements-ci.txt"
                )
                sys_exit(1)
            _run_cmd = "{} {}".format(sys_executable, " ".join(sys_argv))
            _bootstrap_dependency_console(_script_dir, run_command=_run_cmd)
            sys_exit(0)

from colorlog import getLogger as colorlog_getLogger
from colorlog import StreamHandler as colorlog_StreamHandler
from colorlog import ColoredFormatter as colorlog_ColoredFormatter

from psutil import Process as psutil_Process
from psutil import AccessDenied as psutil_AccessDenied
from psutil import NoSuchProcess as psutil_NoSuchProcess
from psutil import cpu_count as psutil_cpu_count
from psutil import virtual_memory as psutil_virtual_memory

from subprocess import run as subprocess_run
from subprocess import Popen as subprocess_Popen
from subprocess import PIPE as subprocess_PIPE
from subprocess import STDOUT as subprocess_STDOUT
from subprocess import list2cmdline as subprocess_list2cmdline
from subprocess import TimeoutExpired as subprocess_TimeoutExpired
from subprocess import CalledProcessError as subprocess_CalledProcessError

# Hide subprocess console on Windows when running as frozen GUI (PyInstaller --windowed)
_SUBPROCESS_NO_WINDOW: dict = {}
if platform_system() == "Windows" and getattr(sys, "frozen", False):
    _cf = getattr(
        __import__("subprocess", fromlist=["CREATE_NO_WINDOW"]),
        "CREATE_NO_WINDOW",
        0x08000000,
    )
    _SUBPROCESS_NO_WINDOW = {"creationflags": _cf}

import logging as logging_module


def _log_level_for_build_line(line: str) -> int:
    """Return logging level (ERROR/WARNING/INFO) for a build output line from its content.
    Used so GUI shows correct prefix (ERROR:/WARNING:/INFO:) and colors, not INFO for everything.
    """
    if not line or not line.strip():
        return logging_module.INFO
    lower = line.lower()
    # Error patterns (aligned with GUI _level_color)
    if (
        "): error " in lower
        or " error: " in lower
        or "error c" in lower
        or "failed:" in lower
        or "fatal error" in lower
        or lower.startswith("error:")
    ):
        return logging_module.ERROR
    # Warning patterns
    if ": warning " in lower or " warning: " in lower or "warning c" in lower:
        return logging_module.WARNING
    return logging_module.INFO


from shutil import which as shutil_which
from concurrent.futures import ThreadPoolExecutor, as_completed

# Optional PyQt5 for GUI (--ui). All UI code is in this file when available.
# TYPE_CHECKING imports bind names for static analysis when PyQt5 may not be installed.
if TYPE_CHECKING:
    from PyQt5.QtCore import Qt, QObject, pyqtSignal, QThread, QTimer
    from PyQt5.QtGui import QFont
    from PyQt5.QtWidgets import (
        QAbstractItemView,
        QAction,
        QApplication,
        QCheckBox,
        QComboBox,
        QDockWidget,
        QFormLayout,
        QGroupBox,
        QHeaderView,
        QLabel,
        QLineEdit,
        QMainWindow,
        QMessageBox,
        QPlainTextEdit,
        QProgressBar,
        QPushButton,
        QScrollArea,
        QTabWidget,
        QTableWidget,
        QTableWidgetItem,
        QToolBar,
        QVBoxLayout,
        QHBoxLayout,
        QWidget,
    )

# When running as PyInstaller-frozen binary, default to --ui so double-click opens the GUI.
if getattr(sys, "frozen", False) and "--ui" not in sys_argv:
    sys_argv.append("--ui")

# On Linux with --ui, re-exec so the process starts with LD_LIBRARY_PATH set;
# the dynamic linker reads it only at process start, so in-process changes don't
# fix the xcb plugin loading system libQt5XcbQpa instead of PyQt5's bundled Qt.
if platform_system() == "Linux" and "--ui" in sys_argv:
    _pyqt5_spec = importlib.util.find_spec("PyQt5")
    if _pyqt5_spec is not None and _pyqt5_spec.origin is not None:
        _pyqt5_qt5_lib = os_path_abspath(
            os_path_join(os_path_dirname(_pyqt5_spec.origin), "Qt5", "lib")
        )
        if os_path_isdir(_pyqt5_qt5_lib):
            _ld_path = os_environ.get("LD_LIBRARY_PATH", "")
            _first = (_ld_path.split(os_pathsep)[:1] or [""])[0]
            if _first.rstrip() != _pyqt5_qt5_lib:
                _new_env = dict(os_environ)
                _new_env["LD_LIBRARY_PATH"] = (
                    _pyqt5_qt5_lib + os_pathsep + _ld_path
                    if _ld_path
                    else _pyqt5_qt5_lib
                )
                os_execve(
                    sys_executable,
                    [sys_executable] + sys_argv,
                    _new_env,
                )
                sys_exit(1)

_ui_available = False
try:
    from PyQt5.QtWidgets import (  # noqa: F811
        QAbstractItemView,
        QAction,
        QApplication,
        QCheckBox,
        QComboBox,
        QDialog,
        QDialogButtonBox,
        QDockWidget,
        QFileDialog,
        QFileSystemModel,
        QFormLayout,
        QGroupBox,
        QHeaderView,
        QKeySequenceEdit,
        QLabel,
        QLineEdit,
        QMainWindow,
        QMessageBox,
        QPlainTextEdit,
        QProgressBar,
        QPushButton,
        QScrollArea,
        QShortcut,
        QSizePolicy,
        QSpinBox,
        QTabWidget,
        QTextEdit,
        QTableWidget,
        QTableWidgetItem,
        QToolBar,
        QTreeView,
        QVBoxLayout,
        QHBoxLayout,
        QWidget,
    )
    from PyQt5.QtCore import (  # noqa: F811
        Qt,
        QEvent,
        QObject,
        QProcess,
        QRegExp,
        pyqtSignal,
        QThread,
        QTimer,
        QSettings,
    )
    from PyQt5.QtGui import (  # noqa: F811
        QColor,
        QFont,
        QKeySequence,
        QStandardItemModel,
        QStandardItem,
        QTextCharFormat,
        QTextCursor,
        QTextDocument,
    )
    import matplotlib

    matplotlib.use("Qt5Agg")
    from matplotlib.backends.backend_qt5agg import FigureCanvasQTAgg  # type: ignore
    from matplotlib.figure import Figure

    _ui_available = True
except ImportError:
    pass

# Standard base paths to search for compilers (fast scan; regex filters names).
# Searched first; then extra per-drive paths (LLVM, msys64, etc.) are added for parallel search.
_COMPILER_BASE_PATHS_WIN = (
    os_path_join(
        os_environ.get("ProgramFiles", "C:\\Program Files"), "Microsoft Visual Studio"
    ),
    os_path_join(
        os_environ.get("ProgramFiles(x86)", "C:\\Program Files (x86)"),
        "Microsoft Visual Studio",
    ),
    os_path_join(
        os_environ.get("ProgramFiles(x86)", "C:\\Program Files (x86)"),
        "Microsoft Visual Studio\\2022",
    ),
    os_path_join(
        os_environ.get("ProgramFiles(x86)", "C:\\Program Files (x86)"),
        "Microsoft Visual Studio\\2019",
    ),
    os_path_join(os_environ.get("ProgramFiles", "C:\\Program Files"), "LLVM\\bin"),
    os_path_join(
        os_environ.get("ProgramFiles(x86)", "C:\\Program Files (x86)"), "LLVM\\bin"
    ),
    "C:\\MinGW\\bin",
    "C:\\MinGW64\\bin",
    "C:\\msys64\\mingw64\\bin",
    "C:\\msys64\\ucrt64\\bin",
    "C:\\msys64\\clang64\\bin",
    "C:\\TDM-GCC-64\\bin",
    "C:\\TDM-GCC\\bin",
    os_path_join(
        os_environ.get("ProgramFiles(x86)", "C:\\Program Files (x86)"), "Intel\\oneAPI"
    ),
    os_path_join(
        os_environ.get("ProgramFiles(x86)", "C:\\Program Files (x86)"),
        "Intel\\Compiler",
    ),
    os_path_join(
        os_environ.get("ProgramFiles(x86)", "C:\\Program Files (x86)"), "Embarcadero"
    ),
    os_path_join(
        os_environ.get("ProgramFiles(x86)", "C:\\Program Files (x86)"), "Borland"
    ),
)


def _get_windows_drive_letters() -> List[str]:
    """Return list of existing Windows drive roots (e.g. ['C:\\', 'D:\\']) for extra compiler search."""
    drives: List[str] = []
    for letter in "ABCDEFGHIJKLMNOPQRSTUVWXYZ":
        root = letter + ":\\"
        if os_path_isdir(root):
            drives.append(root)
    return drives


def _compiler_extra_base_paths_win() -> List[str]:
    """
    Extra base paths for compiler search: non-standard LLVM, per-drive locations.
    E.g. D:\\local\\LLVM\\bin, D:\\LLVM\\bin, D:\\msys64\\mingw64\\bin.
    Standard paths are searched first; these are added for parallel search on other drives.
    """
    extra: List[str] = []
    for root in _get_windows_drive_letters():
        for sub in (
            "local\\LLVM\\bin",
            "LLVM\\bin",
            "msys64\\mingw64\\bin",
            "msys64\\ucrt64\\bin",
            "msys64\\clang64\\bin",
            "MinGW\\bin",
            "MinGW64\\bin",
            "TDM-GCC-64\\bin",
            "TDM-GCC\\bin",
        ):
            extra.append(os_path_join(root, sub))
    return extra


def _compiler_extra_base_paths_linux() -> List[str]:
    """
    Extra base paths for compiler search on Linux: NVIDIA CUDA/HPC, additional
    /opt and /usr locations. Standard paths are searched first; these are added
    for parallel search. One thread per path for speed (like Windows).
    """
    extra: List[str] = [
        "/usr/local/cuda/bin",
        "/opt/nvidia",
        "/opt/cuda/bin",
        "/opt/gcc/bin",
        "/opt/rh/devtoolset-10/root/usr/bin",
        "/opt/rh/devtoolset-11/root/usr/bin",
        "/opt/rh/gcc-toolset-10/root/usr/bin",
        "/opt/rh/gcc-toolset-11/root/usr/bin",
        "/opt/rh/gcc-toolset-12/root/usr/bin",
    ]
    return extra


_COMPILER_BASE_PATHS_UNIX = (
    "/usr/bin",
    "/usr/local/bin",
    "/opt/local/bin",
    "/opt/llvm/bin",
    "/opt/intel/oneapi/compiler/latest/linux/bin",
    "/opt/intel/bin",
)
# Regex for compiler executables: C compilers (and dual C/C++) and C++ compilers.
_COMPILER_NAMES_C_WIN = re_compile(
    r"^(cl\.exe|gcc\.exe|clang\.exe|clang-cl\.exe|icc\.exe|bcc32\.exe|bcc32c\.exe)$",
    re_IGNORECASE,
)
_COMPILER_NAMES_CPP_WIN = re_compile(
    r"^(cl\.exe|g\+\+\.exe|clang\+\+\.exe|clang-cl\.exe|icpc\.exe|bcc32\.exe|bcc32c\.exe)$",
    re_IGNORECASE,
)
# Linux/Unix: gcc, gcc-12, x86_64-linux-gnu-gcc, c89-gcc, clang, clang-14, nvc, nvcc, etc.
_COMPILER_NAMES_C_UNIX = re_compile(
    r"^(gcc(-\d+)?|c89-gcc|c99-gcc|[a-z0-9_.-]+-gcc|"
    r"clang(-\d+)?|[a-z0-9_.-]+-clang|"
    r"icc(-\d+)?|cc|nvcc?)$"
)
_COMPILER_NAMES_CPP_UNIX = re_compile(
    r"^(g\+\+(-\d+)?|[a-z0-9_.-]+-g\+\+|"
    r"clang\+\+(-\d+)?|[a-z0-9_.-]+-clang\+\+|"
    r"icpc(-\d+)?|c\+\+|nvc\+\+)$"
)
_COMPILER_SEARCH_MAX_DEPTH = (
    12  # Limit recursion (e.g. VS/2022/Community/VC/Tools/MSVC/14.xx/bin/Hostx64/x64)
)
_COMPILER_DISCOVERY_MAX_WORKERS = (
    8  # Cap parallel search threads (one per base path group)
)


def _discover_compilers_in_base(
    base: str,
    is_win: bool,
    name_c: Any,
    name_cpp: Any,
) -> Tuple[List[str], List[str]]:
    """
    Search a single base path for C/C++ compilers; return (c_paths, cpp_paths).
    Safe to call from a thread; no shared mutable state.
    """
    c_paths: List[str] = []
    cpp_paths: List[str] = []

    def is_executable(path: str) -> bool:
        if not os_path_exists(path):
            return False
        try:
            if is_win:
                return True
            return os_access(path, os_X_OK)
        except OSError:
            return False

    base = os_path_expanduser(base)
    if not os_path_isdir(base):
        return (c_paths, cpp_paths)
    try:
        base_abs = os_path_abspath(base)
        for root, dirs, files in os_walk(base):
            root_abs = os_path_abspath(root)
            if not root_abs.startswith(base_abs):
                continue
            rel = root_abs[len(base_abs) :].replace("\\", "/").strip("/")  # NOQA: E203
            depth = len([p for p in rel.split("/") if p]) if rel else 0
            if depth >= _COMPILER_SEARCH_MAX_DEPTH:
                dirs[:] = []
                continue
            for f in files:
                if name_c.search(f):
                    absp = os_path_abspath(os_path_join(root, f))
                    if is_executable(absp):
                        c_paths.append(absp)
                if name_cpp.search(f):
                    absp = os_path_abspath(os_path_join(root, f))
                    if is_executable(absp):
                        cpp_paths.append(absp)
    except OSError:
        pass
    return (c_paths, cpp_paths)


def _compiler_option_value(combo: Any, default_label: str) -> Optional[str]:
    """
    Return None when (default) is selected (nothing passed to CMake), else the current path.
    """
    text = combo.currentText().strip()
    if combo.currentData() is None and (not text or text == default_label):
        return None
    return text or None


def _discover_compilers() -> Tuple[List[str], List[str]]:
    """
    Search standard and extra (per-drive LLVM, etc.) paths for C and C++ compilers;
    return (c_paths, cpp_paths) as absolute paths. Uses parallel search (one thread per
    base path) for speed. Safe to call from a background thread.
    """
    c_paths: List[str] = []
    cpp_paths: List[str] = []
    seen_c: set = set()
    seen_cpp: set = set()
    is_win = platform_system() == "Windows"

    def add_c(p: str) -> None:
        if p not in seen_c:
            seen_c.add(p)
            c_paths.append(p)

    def add_cpp(p: str) -> None:
        if p not in seen_cpp:
            seen_cpp.add(p)
            cpp_paths.append(p)

    if is_win:
        name_c = _COMPILER_NAMES_C_WIN
        name_cpp = _COMPILER_NAMES_CPP_WIN
        # Standard paths first, then extra per-drive (LLVM, msys64, etc.)
        all_bases = list(_COMPILER_BASE_PATHS_WIN) + _compiler_extra_base_paths_win()
    else:
        name_c = _COMPILER_NAMES_C_UNIX
        name_cpp = _COMPILER_NAMES_CPP_UNIX
        # Standard paths first, then extra (NVIDIA CUDA/HPC, devtoolset, etc.) for parallel search
        all_bases = list(_COMPILER_BASE_PATHS_UNIX) + _compiler_extra_base_paths_linux()

    # Filter to existing dirs only
    bases_to_search = []
    for base in all_bases:
        base = os_path_expanduser(base)
        if os_path_isdir(base):
            bases_to_search.append(base)

    # Parallel search: one thread per base path
    max_workers = min(
        len(bases_to_search) or 1,
        _COMPILER_DISCOVERY_MAX_WORKERS,
        (psutil_cpu_count(logical=True) or 4),
    )
    with ThreadPoolExecutor(max_workers=max_workers) as executor:
        futures = {
            executor.submit(
                _discover_compilers_in_base, base, is_win, name_c, name_cpp
            ): base
            for base in bases_to_search
        }
        for future in as_completed(futures):
            try:
                c_list, cpp_list = future.result()
                for p in c_list:
                    add_c(p)
                for p in cpp_list:
                    add_cpp(p)
            except Exception:
                continue

    # Also add whatever shutil.which finds (user PATH)
    which_names_c = (
        ("gcc", "cc", "clang", "gcc-12", "gcc-13", "clang-14", "nvcc", "nvc")
        if not is_win
        else ("gcc.exe", "cl.exe", "clang.exe", "clang-cl.exe")
    )
    for name in which_names_c:
        w = shutil_which(name)
        if w:
            w = os_path_abspath(w)
            if w not in seen_c:
                add_c(w)
    which_names_cpp = (
        ("g++", "c++", "clang++", "g++-12", "g++-13", "clang++-14", "nvc++")
        if not is_win
        else ("g++.exe", "clang++.exe", "clang-cl.exe")
    )
    for name in which_names_cpp:
        w = shutil_which(name)
        if w:
            w = os_path_abspath(w)
            if w not in seen_cpp:
                add_cpp(w)

    # On Windows, put MSVC (cl.exe) paths first so VS is the preferred default
    if is_win:

        def _is_msvc_path(p: str) -> bool:
            return "MSVC" in p and (
                p.lower().endswith("cl.exe")
                or p.lower().replace("\\", "/").endswith("cl.exe")
            )

        c_paths = [p for p in c_paths if _is_msvc_path(p)] + [
            p for p in c_paths if not _is_msvc_path(p)
        ]
        cpp_paths = [p for p in cpp_paths if _is_msvc_path(p)] + [
            p for p in cpp_paths if not _is_msvc_path(p)
        ]

    return (c_paths, cpp_paths)


def _split_extra_cmake_args(raw: Optional[str]) -> List[str]:
    """Split --cmake-args value into tokens; strip stray quotes from PowerShell."""
    if not raw:
        return []
    normalized = raw.replace(";", os_pathsep).replace(":", os_pathsep)
    return [
        token.strip().strip('"').strip("'")
        for token in normalized.split(os_pathsep)
        if token.strip().strip('"').strip("'")
    ]


class ArchitectureDetector:
    """Detects the architecture of the system."""

    @staticmethod
    def detect() -> str:
        """
        Static method to detect the architecture of the system.

        Returns:
            str: "x64" for 64-bit systems, "x86" for 32-bit systems.
        Raises:
            ValueError: If the architecture is not supported.
        """
        try:
            machine = platform_machine().lower()
            if machine in ("amd64", "x86_64", "x64"):
                return "x64"
            elif machine in ("i386", "i686"):
                return "x86"
            else:
                raise ValueError("Unsupported architecture: {}".format(machine))
        except Exception as e:
            raise RuntimeError("Failed to detect architecture: {}".format(e))


def get_project_name(project_root: str) -> str:
    """
    Read project name from the root CMakeLists.txt (first argument of project(...)).

    Returns the first word after project(, or "Project" if not found.
    """
    cmake_list_path = os_path_join(project_root, "CMakeLists.txt")
    if not os_path_exists(cmake_list_path):
        return "Project"
    try:
        with open(cmake_list_path, "r", encoding="utf-8") as f:
            content = f.read()
        match = re_search(r"project\s*\(\s*([A-Za-z_][A-Za-z0-9_]*)", content)
        if match:
            return match.group(1).strip()
    except (OSError, IOError, Exception):
        pass
    return "Project"


def _parse_cmake_string_choices(project_root: str) -> Dict[str, List[str]]:
    """
    Parse `set_property(CACHE <NAME> PROPERTY STRINGS ...)` entries from
    CMakeLists.txt to build predefined choices for STRING cache variables.
    """
    result: Dict[str, List[str]] = {}
    cmake_list_path = os_path_join(project_root, "CMakeLists.txt")
    if not os_path_exists(cmake_list_path):
        return result
    try:
        with open(cmake_list_path, "r", encoding="utf-8") as f:
            content = f.read()
        pattern = re_compile(
            r"set_property\s*\(\s*CACHE\s+([A-Za-z_][A-Za-z0-9_]*)\s+PROPERTY\s+STRINGS\s+([^)]+)\)",
            re_IGNORECASE | re_DOTALL,
        )
        for match in pattern.finditer(content):
            name = match.group(1).strip()
            raw_values = match.group(2).strip()
            tokens = re_findall(r'"([^"]*)"|([^\s"]+)', raw_values)
            values: List[str] = []
            for quoted, plain in tokens:
                value = quoted if quoted else plain
                value = value.strip()
                if value:
                    values.append(value)
            # Preserve order while removing duplicates.
            unique_values: List[str] = []
            for value in values:
                if value not in unique_values:
                    unique_values.append(value)
            result[name] = unique_values
    except (OSError, IOError, Exception):
        pass
    return result


def _format_cmake_options_probe_error(
    stage: str, cmd: List[str], exc: Exception
) -> str:
    """Build a human-readable error for CMake options probing."""
    cmd_text = " ".join(cmd)
    if isinstance(exc, subprocess_CalledProcessError):
        stderr_text = (exc.stderr or "").strip()
        stdout_text = (exc.stdout or "").strip()
        details = stderr_text or stdout_text or "No stderr/stdout captured."
        return (
            "CMake options probe failed at stage '{}'. "
            "Command: {}. Exit code: {}. Output: {}".format(
                stage,
                cmd_text,
                exc.returncode,
                details,
            )
        )
    if isinstance(exc, subprocess_TimeoutExpired):
        return (
            "CMake options probe timed out at stage '{}'. "
            "Command: {}. Timeout: {}s.".format(stage, cmd_text, exc.timeout)
        )
    if isinstance(exc, FileNotFoundError):
        return "CMake executable not found in PATH while probing options."
    return "CMake options probe error at stage '{}': {}".format(stage, exc)


def get_cmake_options_detailed(
    project_root: str, extra_cmake_args: Optional[List[str]] = None
) -> Tuple[List[Tuple[str, str, str, str, List[str]]], Optional[str]]:
    """
    Get CMake cache options via cmake -LH.

    Runs a minimal configure then cmake -LH to list cache variables with help.
    Returns:
      (options, error_message)
    where options is a list of tuples:
      (name, description, var_type, value, choices)
    where var_type is BOOL or STRING.
    """

    result: List[Tuple[str, str, str, str, List[str]]] = []
    error_message: Optional[str] = None
    hidden_option_names = {
        "CMAKE_CACHEFILE_DIR",
        "CMAKE_COMMAND",
        "CMAKE_CPACK_COMMAND",
        "CMAKE_CTEST_COMMAND",
        "CMAKE_GENERATOR",
        "CMAKE_GENERATOR_INSTANCE",
        "CMAKE_GENERATOR_PLATFORM",
        "CMAKE_GENERATOR_TOOLSET",
        "CMAKE_HOME_DIRECTORY",
        "CMAKE_PROJECT_NAME",
        "CMAKE_ROOT",
        "CMAKE_CONFIGURATION_TYPES",
    }
    string_choices = _parse_cmake_string_choices(project_root)
    cmake_list_path = os_path_join(project_root, "CMakeLists.txt")
    if not os_path_exists(cmake_list_path):
        return result, None
    temp_build_dir = None
    current_stage = "configure"
    current_cmd: List[str] = ["cmake", project_root, "-B", "<temp-build-dir>"]
    try:
        temp_build_dir = tempfile_mkdtemp(prefix="cmake_options_")
        configure_cmd = ["cmake", project_root, "-B", temp_build_dir]
        if extra_cmake_args:
            configure_cmd.extend(extra_cmake_args)
        current_stage = "configure"
        current_cmd = configure_cmd

        # On Windows, Ninja + no compiler in args => configure would fail (No CMAKE_CXX_COMPILER).
        # Run configure via vcvarsall batch so the probe gets MSVC in PATH.
        vcvarsall_path = None
        if platform_system() == "Windows":
            extra = extra_cmake_args or []
            has_ninja = "Ninja" in extra or (
                "-G" in extra
                and extra.index("-G") + 1 < len(extra)
                and extra[extra.index("-G") + 1] == "Ninja"
            )
            has_cxx = any("CMAKE_CXX_COMPILER" in str(a) for a in configure_cmd)
            if has_ninja and not has_cxx:
                try:
                    builder = CMakeBuilder(project_root)
                    vcvarsall_path = builder._find_vcvarsall()
                except Exception:
                    pass

        if vcvarsall_path:
            batch_path = None
            try:
                with tempfile_NamedTemporaryFile(
                    mode="w", suffix=".bat", delete=False, encoding="utf-8"
                ) as bat:
                    bat.write("@echo off\n")
                    bat.write('cd /d "{}"\n'.format(project_root))
                    bat.write('call "{}" x64 >nul 2>&1\n'.format(vcvarsall_path))
                    bat.write("{}\n".format(subprocess_list2cmdline(configure_cmd)))
                    bat.write("exit /b %errorlevel%\n")
                    batch_path = bat.name
                subprocess_run(
                    ["cmd", "/d", "/c", batch_path],
                    cwd=project_root,
                    check=True,
                    text=True,
                    capture_output=True,
                    timeout=120,
                    **_SUBPROCESS_NO_WINDOW,
                )
            finally:
                if batch_path and os_path_exists(batch_path):
                    try:
                        os_remove(batch_path)
                    except OSError:
                        pass
        else:
            subprocess_run(
                configure_cmd,
                check=True,
                text=True,
                capture_output=True,
                timeout=120,
                **_SUBPROCESS_NO_WINDOW,
            )
        dump_cmd = ["cmake", "-LH", temp_build_dir]
        current_stage = "dump"
        current_cmd = dump_cmd
        dump_result = subprocess_run(
            dump_cmd,
            check=True,
            text=True,
            capture_output=True,
            timeout=30,
            **_SUBPROCESS_NO_WINDOW,
        )
        stdout = (dump_result.stdout or "").strip()
        current_help = ""
        for line in stdout.splitlines():
            line = line.rstrip()
            if line.startswith("//"):
                current_help = line[2:].strip()
                continue
            if ":" in line and "=" in line:
                part = line.split("=", 1)
                if len(part) != 2:
                    current_help = ""
                    continue
                left, value = part[0].strip(), part[1].strip()
                if ":" not in left:
                    current_help = ""
                    continue
                name, var_type = left.split(":", 1)
                name, var_type = name.strip(), var_type.strip()
                if name in hidden_option_names:
                    current_help = ""
                    continue
                var_type_normalized = var_type.upper()
                if var_type_normalized not in ("BOOL", "STRING"):
                    current_help = ""
                    continue
                value_text = value
                if var_type_normalized == "BOOL":
                    value_text = "ON" if value.upper() == "ON" else "OFF"
                choices = string_choices.get(name, [])
                if var_type_normalized == "STRING" and not choices and value_text:
                    choices = [value_text]
                result.append(
                    (name, current_help, var_type_normalized, value_text, choices)
                )
                current_help = ""
    except (subprocess_CalledProcessError, FileNotFoundError, OSError) as exc:
        error_message = _format_cmake_options_probe_error(
            current_stage, current_cmd, exc
        )
    except subprocess_TimeoutExpired as exc:
        error_message = _format_cmake_options_probe_error(
            current_stage, current_cmd, exc
        )
    finally:
        if temp_build_dir and os_path_exists(temp_build_dir):
            try:
                shutil_rmtree(temp_build_dir)
            except OSError:
                pass
    if not result and error_message is None:
        error_message = (
            "CMake completed but returned no BOOL/STRING cache options from cmake -LH."
        )
    return result, error_message


def get_cmake_options(project_root: str) -> List[Tuple[str, str, str, str, List[str]]]:
    """
    Backward-compatible wrapper for callers that only need option entries.
    """
    options, _error = get_cmake_options_detailed(project_root)
    return options


def detect_windows_version() -> Optional[str]:
    """
    Detect the current Windows version and map it to a WINDOWS_VERSION string
    (XP, VISTA, SEVEN, EIGHT, EIGHTDOTONE, TENELEVEN) for CMake WIN32_WINNT.
    Returns None on non-Windows or if detection fails.
    """
    if platform_system() != "Windows":
        return None
    try:
        from sys import getwindowsversion as sys_getwindowsversion

        # sys.getwindowsversion() is available on Windows (Python 3)
        ver = sys_getwindowsversion()
        major, minor = ver.major, ver.minor
        if major == 5 and minor in (1, 2):
            return "XP"
        if major == 6 and minor == 0:
            return "VISTA"
        if major == 6 and minor == 1:
            return "SEVEN"
        if major == 6 and minor == 2:
            return "EIGHT"
        if major == 6 and minor == 3:
            return "EIGHTDOTONE"
        if major >= 10:
            return "TENELEVEN"
        # Fallback for unknown future versions: use TENELEVEN
        return "TENELEVEN"
    except (AttributeError, OSError):
        return None


def detect_msvc_toolset() -> Optional[str]:
    """
    Detect the newest MSVC toolset installed (v120, v140, v141, v142, v143, v144, v145)
    by scanning VC\\Tools\\MSVC under Visual Studio installs. Returns None on non-Windows
    or if no toolset dirs are found.
    """
    if platform_system() != "Windows":
        return None
    program_files = os_environ.get("ProgramFiles", "C:\\Program Files")
    program_files_x86 = os_environ.get("ProgramFiles(x86)", "C:\\Program Files (x86)")
    vs_roots = [
        os_path_join(program_files, "Microsoft Visual Studio"),
        os_path_join(program_files_x86, "Microsoft Visual Studio"),
    ]
    best_toolset: Optional[str] = None
    best_major, best_minor = 0, 0

    def version_to_toolset(major: int, minor: int) -> Optional[str]:
        if major == 12:
            return "v120"
        if major == 14:
            if minor == 0:
                return "v140"
            if minor < 20:
                return "v141"
            if minor < 30:
                return "v142"
            if minor < 40:
                return "v143"
            if minor < 50:
                return "v144"
            return "v145"
        return None

    for vs_root in vs_roots:
        if not os_path_isdir(vs_root):
            continue
        try:
            for year_entry in os_listdir(vs_root):
                yearp = os_path_join(vs_root, year_entry)
                if not os_path_isdir(yearp):
                    continue
                for edition in (
                    "Community",
                    "Professional",
                    "Enterprise",
                    "BuildTools",
                ):
                    vcp = os_path_join(yearp, edition)
                    if not os_path_isdir(vcp):
                        continue
                    msvc_base = os_path_join(vcp, "VC", "Tools", "MSVC")
                    if not os_path_isdir(msvc_base):
                        continue
                    for ver_dir in os_listdir(msvc_base):
                        vp = os_path_join(msvc_base, ver_dir)
                        if not os_path_isdir(vp):
                            continue
                        parts = ver_dir.split(".")
                        if len(parts) >= 2:
                            try:
                                maj, min_ = int(parts[0]), int(parts[1])
                                toolset = version_to_toolset(maj, min_)
                                if toolset and (
                                    maj > best_major
                                    or (maj == best_major and min_ > best_minor)
                                ):
                                    best_major, best_minor = maj, min_
                                    best_toolset = toolset
                            except ValueError:
                                continue
        except OSError:
            continue

    return best_toolset


# MSVC toolset id -> display label (for UI). Order: oldest to newest.
_MSVC_TOOLSET_LABELS = [
    ("v120", "v120 (VS 2013)"),
    ("v140", "v140 (VS 2015)"),
    ("v141", "v141 (VS 2017)"),
    ("v142", "v142 (VS 2019)"),
    ("v143", "v143 (VS 2022)"),
    ("v144", "v144 (VS 2025)"),
    ("v145", "v145 (VS 2026+)"),
]


def detect_installed_msvc_toolsets() -> List[Tuple[str, str]]:
    """
    Return list of (toolset_id, display_label) for MSVC toolsets that are actually
    installed on this machine. Only includes toolsets found under VC\\Tools\\MSVC.
    Returns empty list on non-Windows.
    """
    if platform_system() != "Windows":
        return []
    program_files = os_environ.get("ProgramFiles", "C:\\Program Files")
    program_files_x86 = os_environ.get("ProgramFiles(x86)", "C:\\Program Files (x86)")
    vs_roots = [
        os_path_join(program_files, "Microsoft Visual Studio"),
        os_path_join(program_files_x86, "Microsoft Visual Studio"),
    ]

    def version_to_toolset(major: int, minor: int) -> Optional[str]:
        if major == 12:
            return "v120"
        if major == 14:
            if minor == 0:
                return "v140"
            if minor < 20:
                return "v141"
            if minor < 30:
                return "v142"
            if minor < 40:
                return "v143"
            if minor < 50:
                return "v144"
            return "v145"
        return None

    found_ids: set = set()
    for vs_root in vs_roots:
        if not os_path_isdir(vs_root):
            continue
        try:
            for year_entry in os_listdir(vs_root):
                yearp = os_path_join(vs_root, year_entry)
                if not os_path_isdir(yearp):
                    continue
                for edition in (
                    "Community",
                    "Professional",
                    "Enterprise",
                    "BuildTools",
                ):
                    vcp = os_path_join(yearp, edition)
                    if not os_path_isdir(vcp):
                        continue
                    msvc_base = os_path_join(vcp, "VC", "Tools", "MSVC")
                    if not os_path_isdir(msvc_base):
                        continue
                    for ver_dir in os_listdir(msvc_base):
                        vp = os_path_join(msvc_base, ver_dir)
                        if not os_path_isdir(vp):
                            continue
                        parts = ver_dir.split(".")
                        if len(parts) >= 2:
                            try:
                                maj, min_ = int(parts[0]), int(parts[1])
                                toolset = version_to_toolset(maj, min_)
                                if toolset:
                                    found_ids.add(toolset)
                            except ValueError:
                                continue
        except OSError:
            continue

    return [(tid, label) for tid, label in _MSVC_TOOLSET_LABELS if tid in found_ids]


# All standards we offer in the UI (order matters for combo).
ALL_C_STANDARDS_UI = (89, 99, 11, 17, 23)
ALL_CPP_STANDARDS_UI = (98, 11, 14, 17, 20, 23, 26)

# Compiler support for C/C++ standards. Source of truth (updated over time):
#   https://en.cppreference.com/w/cpp/compiler_support
#   (C++: compiler_support/11, 14, 17, 20, 23, 26; C: see C compiler support links there.)
# Compiler family -> (version_rows, c_standards_per_row, cpp_standards_per_row). Same length lists.
# For version (major, minor) we use the row i with largest version_rows[i] <= (major, minor).
# Thresholds below are aligned with cppreference.com; re-check that URL when adding new standards.
_COMPILER_STANDARDS_TABLE: List[
    Tuple[str, List[Tuple[int, int]], List[Tuple[int, ...]], List[Tuple[int, ...]]]
] = [
    # GCC: C11 4.7+, C17 8+, C23 14+. C++14 5+, C++17 7+, C++20 10+, C++23 13+, C++26 14+ (partial).
    (
        "gcc",
        [(0, 0), (4, 7), (8, 0), (10, 0), (14, 0)],
        [
            (89, 99),
            (89, 99, 11),
            (89, 99, 11, 17),
            (89, 99, 11, 17),
            (89, 99, 11, 17, 23),
        ],
        [
            (98,),
            (98, 11, 14),
            (98, 11, 14, 17),
            (98, 11, 14, 17, 20),
            (98, 11, 14, 17, 20, 23, 26),
        ],
    ),
    (
        "mingw",
        [(0, 0), (4, 7), (8, 0), (10, 0), (14, 0)],
        [
            (89, 99),
            (89, 99, 11),
            (89, 99, 11, 17),
            (89, 99, 11, 17),
            (89, 99, 11, 17, 23),
        ],
        [
            (98,),
            (98, 11, 14),
            (98, 11, 14, 17),
            (98, 11, 14, 17, 20),
            (98, 11, 14, 17, 20, 23, 26),
        ],
    ),
    # Clang: C11 3.1+, C17 5+, C23 17+. C++11 3.1+, C++14 3.4+, C++17 5+, C++20 10+, C++23 17+.
    (
        "clang",
        [(0, 0), (3, 1), (5, 0), (10, 0), (17, 0)],
        [
            (89, 99),
            (89, 99, 11),
            (89, 99, 11, 17),
            (89, 99, 11, 17),
            (89, 99, 11, 17, 23),
        ],
        [(98,), (98, 11), (98, 11, 14), (98, 11, 14, 17), (98, 11, 14, 17, 20, 23)],
    ),
    # clang-cl: follows Clang version; C++14+ typically (MSVC-compat). C++20 10+, C++23 17+.
    (
        "clang-cl",
        [(0, 0), (10, 0), (17, 0)],
        [(89, 99, 11), (89, 99, 11, 17), (89, 99, 11, 17, 23)],
        [(98, 11, 14), (98, 11, 14, 17, 20), (98, 11, 14, 17, 20, 23)],
    ),
    # MSVC: version from path (14.xx = VS 2015+). C++17 19.10+, C++20 19.29+, C++23 19.34+ (see cppreference).
    (
        "msvc",
        [(14, 0), (14, 16), (14, 28), (14, 34)],
        [(89, 99), (89, 99), (89, 99, 11, 17), (89, 99, 11, 17, 23)],
        [
            (98, 11, 14),
            (98, 11, 14, 17),
            (98, 11, 14, 17, 20),
            (98, 11, 14, 17, 20, 23),
        ],
    ),
]


def _compiler_version_from_path(path: str) -> Tuple[str, Optional[int], Optional[int]]:
    """
    Detect compiler family and version from executable path.
    Returns (family, major, minor). family is one of gcc, mingw, clang, clang-cl, msvc, unknown.
    For MSVC, version is parsed from path (e.g. MSVC\\14.29\\ -> 14, 29). For others, returns (family, None, None) unless we run --version (done separately).
    """
    if not path or not path.strip():
        return ("unknown", None, None)
    name = os_path_basename(path).lower()
    path_lower = path.lower()
    if "clang-cl" in name:
        return ("clang-cl", None, None)
    if "clang" in name or "clang++" in name:
        return ("clang", None, None)
    if "gcc" in name or "g++" in name:
        return ("gcc", None, None)
    if "mingw" in name:
        return ("mingw", None, None)
    if "cl.exe" in name or (name == "cl" and "cl.exe" in path_lower):
        m = re_search(r"MSVC[\\/](\d+)\.(\d+)", path, re_IGNORECASE)
        if m:
            return ("msvc", int(m.group(1)), int(m.group(2)))
        return ("msvc", 14, 0)
    return ("unknown", None, None)


def _compiler_version_from_version_output(
    version_output: str, family: str
) -> Tuple[int, int]:
    """Parse major.minor from compiler --version output. Returns (0, 0) if parse fails."""
    m = re_search(r"(\d+)\.(\d+)", version_output)
    if m:
        return (int(m.group(1)), int(m.group(2)))
    return (0, 0)


def get_supported_c_cpp_standards_for_compiler(
    compiler_path: Optional[str], is_cpp: bool
) -> Tuple[Tuple[int, ...], Tuple[int, ...]]:
    """
    Return (supported_c_standards, supported_cpp_standards) for the given compiler path.
    If path is None/empty or compiler is unknown, returns (ALL_C_STANDARDS_UI, ALL_CPP_STANDARDS_UI).
    For GCC/Clang/Mingw we run compiler --version to get version; for MSVC we use path.
    """
    if not compiler_path or not compiler_path.strip():
        return (ALL_C_STANDARDS_UI, ALL_CPP_STANDARDS_UI)
    family, major, minor = _compiler_version_from_path(compiler_path)
    if family == "unknown":
        return (ALL_C_STANDARDS_UI, ALL_CPP_STANDARDS_UI)
    if family == "msvc" and major is not None and minor is not None:
        pass
    elif family in ("gcc", "mingw", "clang", "clang-cl"):
        try:
            r = subprocess_run(
                [compiler_path, "--version"],
                capture_output=True,
                timeout=5,
                **_SUBPROCESS_NO_WINDOW,
            )
            if r.returncode == 0 and r.stdout:
                text = (r.stdout.decode("utf-8", errors="ignore") or "").strip()
                major, minor = _compiler_version_from_version_output(text, family)
        except Exception:
            major, minor = 0, 0
    else:
        return (ALL_C_STANDARDS_UI, ALL_CPP_STANDARDS_UI)

    for row_family, version_rows, c_rows, cpp_rows in _COMPILER_STANDARDS_TABLE:
        if row_family != family:
            continue
        best_c, best_cpp = c_rows[0], cpp_rows[0]
        if major is None or minor is None:
            best_c = c_rows[-1]
            best_cpp = cpp_rows[-1]
        else:
            for i, (vmaj, vmin) in enumerate(version_rows):
                if (vmaj, vmin) <= (major, minor):
                    best_c = c_rows[i] if i < len(c_rows) else c_rows[-1]
                    best_cpp = cpp_rows[i] if i < len(cpp_rows) else cpp_rows[-1]
        return (best_c, best_cpp)

    return (ALL_C_STANDARDS_UI, ALL_CPP_STANDARDS_UI)


class CMakeBuilder:
    # C standards support
    kdefault_min_c_standard = 89
    kdefault_max_c_standard = 23

    # C++ standards support
    kdefault_min_cpp_standard = 98
    kdefault_max_cpp_standard = 26

    def __init__(self, project_root: str):
        """
        Initialize the builder with project root directory.

        Args:
            project_root: Path to the project root directory containing CMakeLists.txt
        """
        self.os_prefix = "win" if platform_system() == "Windows" else "linux"
        self.architecture = ArchitectureDetector.detect()
        self.project_root = os_path_abspath(project_root)
        self.version = self._get_project_version()
        self.c_version = self.kdefault_min_c_standard
        self.cpp_version = self.kdefault_min_cpp_standard
        self.build_dir = os_path_join(self.project_root, "build")
        self.cmake_args = []
        self.compiler_id = "unknown"
        self.compiler_version_tag = ""
        self.build_type = "Release"  # Default build type
        self.custom_c_compiler = None
        self.custom_cpp_compiler = None
        self.compiler_specific_flags = {}
        self.use_ninja = False  # Flag to use Ninja build system
        self.ninja_executable = (
            None  # Resolved path for CMAKE_MAKE_PROGRAM (avoids shim/wrapper issues)
        )
        self.debug_trycompile = (
            False  # When True, add --debug-trycompile to preserve TryCompile dirs
        )
        self.generator_override: Optional[str] = (
            None  # e.g. "NMake Makefiles" for compile_commands on Windows
        )
        self.parallel_jobs = (
            None  # If set, used for cmake --build --parallel N; else auto
        )

        # Configure colorlog
        self.logger = colorlog_getLogger("CMakeBuilder")
        self.logger.setLevel(logging_INFO)

        handler = colorlog_StreamHandler()
        handler.setFormatter(
            colorlog_ColoredFormatter(
                "%(log_color)s%(levelname)-8s%(reset)s %(message)s",
                log_colors={
                    "DEBUG": "cyan",
                    "INFO": "green",
                    "WARNING": "yellow",
                    "ERROR": "red",
                    "CRITICAL": "red,bg_white",
                },
            )
        )
        self.logger.addHandler(handler)

    def _check_ninja_available(self) -> bool:
        """
        Check if Ninja build system is available in PATH.
        Stores the resolved path in self.ninja_executable so CMake can be given
        -DCMAKE_MAKE_PROGRAM=... and avoid shim/wrapper issues (Scoop, .bat, etc.).

        Returns:
            bool: True if Ninja is available, False otherwise
        """
        ninja_cmd = shutil_which("ninja")
        if ninja_cmd:
            self.ninja_executable = os_path_abspath(ninja_cmd)
            self.logger.info(
                "Ninja build system found: {}".format(self.ninja_executable)
            )
            path_lower = (ninja_cmd or "").lower()
            if (
                "shims" in path_lower
                or path_lower.endswith(".bat")
                or path_lower.endswith(".cmd")
            ):
                self.logger.warning(
                    "Ninja appears to be a shim or script (path contains 'shims' or is .bat/.cmd). "
                    "If you see 'rules.ninja' not found, use a real ninja.exe from "
                    "https://github.com/ninja-build/ninja/releases and set CMAKE_MAKE_PROGRAM or PATH."
                )
            return True
        self.ninja_executable = None
        return False

    def _show_ninja_installation_instructions(self) -> None:
        """
        Display OS-specific instructions for installing Ninja build system.
        Provides detailed guidance for Windows, Linux (multiple distros), and macOS.
        """
        current_os = platform_system()
        self.logger.error("Ninja build system not found in PATH!")
        self.logger.info("")
        self.logger.info("=== Ninja Installation Instructions ===")
        self.logger.info("")

        if current_os == "Windows":
            self.logger.info("Windows Installation Options:")
            self.logger.info("")
            self.logger.info("1. Using Chocolatey (Recommended):")
            self.logger.info("   choco install ninja")
            self.logger.info("")
            self.logger.info("2. Using Scoop:")
            self.logger.info("   scoop install ninja")
            self.logger.info("")
            self.logger.info("3. Using winget (Windows Package Manager):")
            self.logger.info("   winget install Ninja-build.Ninja")
            self.logger.info("")
            self.logger.info("4. Manual Installation:")
            self.logger.info(
                "   a) Download from: https://github.com/ninja-build/ninja/releases"
            )
            self.logger.info(
                "   b) Extract ninja.exe to a directory (e.g., C:\\Tools\\ninja)"
            )
            self.logger.info("   c) Add directory to PATH:")
            self.logger.info("      - Open System Properties -> Environment Variables")
            self.logger.info("      - Edit PATH and add C:\\Tools\\ninja")
            self.logger.info("      - Restart terminal/IDE")
            self.logger.info("")
            self.logger.info("5. With Visual Studio:")
            self.logger.info("   - Install 'C++ CMake tools for Windows' component")
            self.logger.info("   - Ninja is included in Visual Studio 2019+")
            self.logger.info("")

        elif current_os == "Linux":
            self.logger.info("Linux Installation Instructions:")
            self.logger.info("")
            self.logger.info("Debian/Ubuntu/Linux Mint:")
            self.logger.info("   sudo apt update")
            self.logger.info("   sudo apt install ninja-build")
            self.logger.info("")
            self.logger.info("Fedora/RHEL/CentOS/Rocky Linux/AlmaLinux:")
            self.logger.info("   sudo dnf install ninja-build")
            self.logger.info("   # Or on older systems:")
            self.logger.info("   sudo yum install ninja-build")
            self.logger.info("")
            self.logger.info("openSUSE/SUSE Linux Enterprise:")
            self.logger.info("   sudo zypper install ninja")
            self.logger.info("")
            self.logger.info("Arch Linux/Manjaro:")
            self.logger.info("   sudo pacman -S ninja")
            self.logger.info("")
            self.logger.info("Gentoo:")
            self.logger.info("   sudo emerge dev-util/ninja")
            self.logger.info("")
            self.logger.info("Alpine Linux:")
            self.logger.info("   sudo apk add ninja")
            self.logger.info("")
            self.logger.info("Void Linux:")
            self.logger.info("   sudo xbps-install -S ninja")
            self.logger.info("")
            self.logger.info("From Source (any distro):")
            self.logger.info("   git clone https://github.com/ninja-build/ninja.git")
            self.logger.info("   cd ninja && ./configure.py --bootstrap")
            self.logger.info("   sudo cp ninja /usr/local/bin/")
            self.logger.info("")

        elif current_os == "Darwin":  # macOS
            self.logger.info("macOS Installation Options:")
            self.logger.info("")
            self.logger.info("1. Using Homebrew (Recommended):")
            self.logger.info("   brew install ninja")
            self.logger.info("")
            self.logger.info("2. Using MacPorts:")
            self.logger.info("   sudo port install ninja")
            self.logger.info("")
            self.logger.info("3. From Source:")
            self.logger.info("   git clone https://github.com/ninja-build/ninja.git")
            self.logger.info("   cd ninja && ./configure.py --bootstrap")
            self.logger.info("   sudo cp ninja /usr/local/bin/")
            self.logger.info("")

        else:
            self.logger.info("For your operating system, please visit:")
            self.logger.info("   https://github.com/ninja-build/ninja/releases")
            self.logger.info("")

        self.logger.info("After installation, verify with: ninja --version")
        self.logger.info("Then retry your build command with --use-ninja flag")
        self.logger.info("")

    def _find_clang_compiler(self) -> Optional[str]:
        """
        Find Clang compiler (clang/clang++) on the system.

        Returns:
            Optional[str]: Path to clang++ if found, None otherwise
        """
        # Try to find clang++ in PATH
        clang_path = shutil_which("clang++")
        if clang_path:
            return clang_path

        # Try clang as fallback
        clang_path = shutil_which("clang")
        if clang_path:
            return clang_path

        return None

    def _find_clang_cl_compiler(self) -> Optional[str]:
        """
        Find Clang-CL compiler (clang-cl.exe) on Windows.

        Returns:
            Optional[str]: Path to clang-cl.exe if found, None otherwise
        """
        if platform_system() != "Windows":
            return None

        # Try to find clang-cl.exe in PATH
        clang_cl_path = shutil_which("clang-cl")
        if clang_cl_path:
            return clang_cl_path

        # Search in common LLVM installation paths
        import os

        program_files = os.environ.get("ProgramFiles", "C:\\Program Files")
        program_files_x86 = os.environ.get(
            "ProgramFiles(x86)", "C:\\Program Files (x86)"
        )

        llvm_paths = [
            os_path_join(program_files, "LLVM", "bin", "clang-cl.exe"),
            os_path_join(program_files_x86, "LLVM", "bin", "clang-cl.exe"),
            os_path_join(
                program_files,
                "Microsoft Visual Studio",
                "2022",
                "Community",
                "VC",
                "Tools",
                "Llvm",
                "bin",
                "clang-cl.exe",
            ),
            os_path_join(
                program_files,
                "Microsoft Visual Studio",
                "2022",
                "Professional",
                "VC",
                "Tools",
                "Llvm",
                "bin",
                "clang-cl.exe",
            ),
            os_path_join(
                program_files,
                "Microsoft Visual Studio",
                "2022",
                "Enterprise",
                "VC",
                "Tools",
                "Llvm",
                "bin",
                "clang-cl.exe",
            ),
        ]

        for llvm_path in llvm_paths:
            if os_path_exists(llvm_path):
                return llvm_path

        return None

    def _find_gcc_compiler(self) -> Optional[str]:
        """
        Find GCC compiler (gcc/g++) on the system.

        Returns:
            Optional[str]: Path to g++ if found, None otherwise
        """
        # Try to find g++ in PATH
        gpp_path = shutil_which("g++")
        if gpp_path:
            return gpp_path

        # Try gcc as fallback
        gcc_path = shutil_which("gcc")
        if gcc_path:
            return gcc_path

        # On Windows, try MinGW paths
        if platform_system() == "Windows":
            import os

            program_files = os.environ.get("ProgramFiles", "C:\\Program Files")
            program_files_x86 = os.environ.get(
                "ProgramFiles(x86)", "C:\\Program Files (x86)"
            )

            mingw_paths = [
                os_path_join(program_files, "mingw64", "bin", "g++.exe"),
                os_path_join(program_files, "mingw-w64", "bin", "g++.exe"),
                os_path_join(program_files_x86, "mingw64", "bin", "g++.exe"),
                os_path_join(program_files_x86, "mingw-w64", "bin", "g++.exe"),
            ]

            for mingw_path in mingw_paths:
                if os_path_exists(mingw_path):
                    return mingw_path

        return None

    def _find_msvc_compiler(self, architecture: Optional[str] = None) -> Optional[str]:
        """
        Find MSVC compiler (cl.exe) on Windows.
        Searches in standard Visual Studio installation paths.

        Args:
            architecture: Target architecture ('x64' or 'x86'). If None, defaults to x64.

        Returns:
            Optional[str]: Path to cl.exe if found, None otherwise
        """
        if platform_system() != "Windows":
            return None

        # Default to x64 if architecture not specified
        if architecture is None:
            architecture = "x64"

        # CRITICAL: Do NOT use cl.exe from PATH for x64 architecture
        # PATH typically contains x86 compiler by default (from Developer Command Prompt)
        # This causes architecture mismatch: x86 compiler cannot link x64 libraries
        # Only search PATH if architecture is x86 (which matches default PATH)
        if architecture == "x86":
            # For x86, PATH might have correct compiler
            cl_path = shutil_which("cl")
            if cl_path:
                # Verify it's actually x86 (basic check: path contains x86 or no x64)
                if "x64" not in cl_path.lower() and (
                    "x86" in cl_path.lower() or "x64" not in cl_path
                ):
                    return cl_path
                # If PATH has x64 compiler but we need x86, continue searching
        # For x64 architecture: skip PATH entirely, search explicit paths only

        # Search in standard Visual Studio installation paths
        import os

        program_files = os.environ.get("ProgramFiles", "C:\\Program Files")
        program_files_x86 = os.environ.get(
            "ProgramFiles(x86)", "C:\\Program Files (x86)"
        )

        # Common Visual Studio installation paths (18 = VS 2026+, 2022, 2019)
        editions = ("Community", "Professional", "Enterprise", "BuildTools")
        pf_versions = ("18", "2022", "2019")
        pf_x86_versions = ("2019", "2017")
        vs_paths = []
        for version in pf_versions:
            for edition in editions:
                vs_paths.append(
                    os_path_join(
                        program_files,
                        "Microsoft Visual Studio",
                        version,
                        edition,
                        "VC",
                        "Tools",
                        "MSVC",
                    )
                )
        for version in pf_x86_versions:
            for edition in ("Community", "Professional", "Enterprise"):
                vs_paths.append(
                    os_path_join(
                        program_files_x86,
                        "Microsoft Visual Studio",
                        version,
                        edition,
                        "VC",
                        "Tools",
                        "MSVC",
                    )
                )

        # Determine target architecture directory
        target_arch = architecture.lower()
        if target_arch not in ["x64", "x86"]:
            target_arch = "x64"  # Default to x64

        # Search for cl.exe in MSVC toolset directories
        for vs_base_path in vs_paths:
            if not os_path_exists(vs_base_path):
                continue

            # List all MSVC version directories (e.g., 14.29.30133)
            try:
                import glob

                version_dirs = glob.glob(os_path_join(vs_base_path, "*"))
                # Sort by version (newest first)
                version_dirs.sort(reverse=True)

                for version_dir in version_dirs:
                    if not os_path_isdir(version_dir):
                        continue

                    # Try different host/target combinations
                    # Priority: Hostx64 for x64 target, Hostx86 for x86 target
                    # NOTE: Hostx86 cannot compile x64 code (only Hostx64 can compile x64)
                    if target_arch == "x64":
                        # For x64 target: only use Hostx64 (x86 host cannot compile x64)
                        host_targets = [
                            ("Hostx64", "x64"),  # Preferred: x64 host for x64 target
                            ("HostX64", "x64"),  # Case variation
                        ]
                    elif target_arch == "x86":
                        # For x86 target: prefer Hostx86, but Hostx64 can cross-compile
                        host_targets = [
                            ("Hostx86", "x86"),  # Preferred: x86 host for x86 target
                            ("HostX86", "x86"),  # Case variation
                            (
                                "Hostx64",
                                "x86",
                            ),  # Fallback: x64 host cross-compiling x86
                            ("HostX64", "x86"),  # Case variation
                        ]
                    else:
                        # Default fallback (shouldn't happen)
                        host_targets = [
                            ("Hostx64", target_arch),
                            ("HostX64", target_arch),
                        ]

                    for host, target in host_targets:
                        cl_exe_path = os_path_join(
                            version_dir, "bin", host, target, "cl.exe"
                        )
                        if os_path_exists(cl_exe_path):
                            # Architecture is already verified by host_targets list
                            # For x64: only Hostx64\x64 paths are searched
                            # For x86: Hostx86\x86 and Hostx64\x86 paths are searched
                            return cl_exe_path

                        # Also try with .EXE extension (case-insensitive filesystem)
                        cl_exe_path_upper = os_path_join(
                            version_dir, "bin", host, target, "cl.EXE"
                        )
                        if os_path_exists(cl_exe_path_upper):
                            # Architecture is already verified by host_targets list
                            return cl_exe_path_upper
            except Exception:
                continue

        return None

    def _find_vcvarsall(self) -> Optional[str]:
        """
        Find vcvarsall.bat script for setting up MSVC environment.

        Returns:
            Optional[str]: Path to vcvarsall.bat if found, None otherwise
        """
        if platform_system() != "Windows":
            return None

        import os

        program_files = os.environ.get("ProgramFiles", "C:\\Program Files")
        program_files_x86 = os.environ.get(
            "ProgramFiles(x86)", "C:\\Program Files (x86)"
        )

        editions = ("Community", "Professional", "Enterprise", "BuildTools")
        # VS install folder names: 18 (VS 2026+), 2022, 2019, 2017
        pf_versions = ("18", "2022", "2019")
        pf_x86_versions = ("18", "2022", "2019", "2017")

        vs_paths = []
        for version in pf_versions:
            for edition in editions:
                vs_paths.append(
                    os_path_join(
                        program_files,
                        "Microsoft Visual Studio",
                        version,
                        edition,
                        "VC",
                        "Auxiliary",
                        "Build",
                        "vcvarsall.bat",
                    )
                )
        for version in pf_x86_versions:
            for edition in editions:
                vs_paths.append(
                    os_path_join(
                        program_files_x86,
                        "Microsoft Visual Studio",
                        version,
                        edition,
                        "VC",
                        "Auxiliary",
                        "Build",
                        "vcvarsall.bat",
                    )
                )

        for vcvarsall_path in vs_paths:
            if os_path_exists(vcvarsall_path):
                return vcvarsall_path

        vswhere_path = os_path_join(
            program_files_x86,
            "Microsoft Visual Studio",
            "Installer",
            "vswhere.exe",
        )
        if os_path_exists(vswhere_path):
            try:
                result = subprocess_run(
                    [
                        vswhere_path,
                        "-latest",
                        "-products",
                        "*",
                        "-requires",
                        "Microsoft.VisualStudio.Component.VC.Tools.x86.x64",
                        "-property",
                        "installationPath",
                    ],
                    capture_output=True,
                    text=True,
                    encoding="utf-8",
                    errors="ignore",
                    **_SUBPROCESS_NO_WINDOW,
                )
                if result.returncode == 0:
                    install_path = result.stdout.strip()
                    if install_path:
                        candidate = os_path_join(
                            install_path,
                            "VC",
                            "Auxiliary",
                            "Build",
                            "vcvarsall.bat",
                        )
                        if os_path_exists(candidate):
                            return candidate
            except Exception:
                pass

        return None

    def _setup_msvc_environment(self) -> bool:
        """
        Set up MSVC environment by finding and using vcvarsall.bat.
        Required for MSVC/clang-cl with the Ninja generator (Windows SDK, INCLUDE/LIB/PATH).

        Returns:
            bool: True if environment was set up successfully, False otherwise
        """
        if platform_system() != "Windows":
            return False

        # Check if we're using an MSVC-compatible compiler (cl.exe or clang-cl.exe)
        if not self.custom_cpp_compiler:
            return False

        compiler_name = os_path_basename(self.custom_cpp_compiler).lower()
        uses_msvc_toolchain = (
            compiler_name in ("cl.exe", "cl") or "clang-cl" in compiler_name
        )
        if not uses_msvc_toolchain:
            return False

        # Find vcvarsall.bat
        vcvarsall_path = self._find_vcvarsall()
        if not vcvarsall_path:
            self.logger.warning(
                "⚠️  vcvarsall.bat not found. MSVC environment may not be properly configured."
            )
            return False

        # Determine architecture for vcvarsall
        arch_arg = "x64" if self.architecture == "x64" else "x86"

        # Use vcvarsall.bat to set up environment via temp .bat + os.environ
        # Also set up environment variables by running vcvarsall and capturing env vars
        try:
            # Create a temporary batch file to run vcvarsall and capture environment variables
            # This avoids complex quote escaping issues with cmd.exe
            with tempfile_NamedTemporaryFile(
                mode="w", suffix=".bat", delete=False, encoding="utf-8"
            ) as temp_bat:
                temp_bat.write("@echo off\n")
                temp_bat.write(
                    'call "{}" {} >nul 2>&1\n'.format(vcvarsall_path, arch_arg)
                )
                temp_bat.write("set\n")
                temp_bat_path = temp_bat.name

            try:
                result = subprocess_run(
                    [temp_bat_path],
                    shell=True,
                    capture_output=True,
                    text=True,
                    encoding="utf-8" if platform_system() == "Windows" else "cp1251",
                    errors="ignore",
                    **_SUBPROCESS_NO_WINDOW,
                )
            finally:
                # Clean up temporary batch file
                try:
                    os_remove(temp_bat_path)
                except OSError:
                    pass

            if result.returncode == 0:
                # Parse ALL environment variables from vcvarsall output
                env_vars = {}
                for line in result.stdout.splitlines():
                    if "=" in line and not line.strip().startswith("_"):
                        key, value = line.split("=", 1)
                        key = key.strip()
                        value = value.strip()
                        if key:
                            env_vars[key.upper()] = value

                # Apply ALL variables; CMake 4.x + Ninja need WindowsSDKVersion,
                # UCRTVersion, VCToolsVersion, VSINSTALLDIR, Platform, etc.
                for key, value in env_vars.items():
                    # Use vcvarsall result as authoritative snapshot.
                    # This avoids unbounded PATH/INCLUDE/LIB growth in long-lived GUI process.
                    os_environ[key] = value

                self.logger.info(
                    "🔧 Configured MSVC environment ({} vars) using: {}".format(
                        len(env_vars), vcvarsall_path
                    )
                )
                return True
            else:
                self.logger.warning(
                    "⚠️  Failed to set up MSVC environment. Error: {}".format(
                        result.stderr[:200] if result.stderr else "Unknown error"
                    )
                )
                # Still return True; env setup may have partially succeeded
                return True
        except Exception as e:
            self.logger.warning(
                "⚠️  Error setting up MSVC environment: {}. Continuing anyway...".format(
                    e
                )
            )
            # Still return True; env setup may have partially succeeded
            return True

    def _get_project_version(self) -> str:
        default_version = "0.0.0"  # Default version if not found

        cmake_list_path = os_path_join(self.project_root, "CMakeLists.txt")
        if not os_path_exists(cmake_list_path):
            return default_version

        with open(cmake_list_path, "r") as f:
            content = f.read()

        match = re_search(r"project\(.*?VERSION\s+([0-9.]+)", content, re_DOTALL)
        if match:
            return match.group(1)
        return default_version

    def run_command(
        self,
        cmd_list: List[str],
        cwd: Optional[str] = None,
        capture_output: bool = False,
        timeout: Optional[int] = None,
        cancel_check: Optional[Callable[[], bool]] = None,
    ) -> bool:
        """
        Execute a shell command with error handling.

        When capture_output is False, stdout/stderr are streamed line-by-line to
        the logger so that GUI log panels show cmake/build output in real time.

        Args:
            cmd_list: List of command and arguments
            cwd: Working directory for command execution
            capture_output: Whether to capture command output (for dependency checks)
            timeout: Timeout in seconds for command execution
            cancel_check: Optional callable returning True to abort (used by GUI Stop).

        Returns:
            bool: True if command succeeded, False otherwise
        """
        if not capture_output:
            self.logger.info("Running command: {}".format(" ".join(cmd_list)))
        proc = None
        try:
            if capture_output:
                subprocess_run(
                    cmd_list,
                    cwd=cwd,
                    check=True,
                    text=True,
                    capture_output=True,
                    timeout=timeout,
                    **_SUBPROCESS_NO_WINDOW,
                )
                return True
            # Stream stdout/stderr to logger in real time
            # encoding/errors: compiler may emit non-UTF-8 (locale, paths); avoid decode errors
            proc = subprocess_Popen(
                cmd_list,
                cwd=cwd or self.project_root,
                stdout=subprocess_PIPE,
                stderr=subprocess_STDOUT,
                text=True,
                encoding="utf-8",
                errors="replace",
                bufsize=1,
                **_SUBPROCESS_NO_WINDOW,
            )
            if proc.stdout is not None:
                for line in iter(proc.stdout.readline, ""):
                    if cancel_check and cancel_check():
                        try:
                            proc.terminate()
                            proc.wait(timeout=5)
                        except Exception:
                            pass
                        self.logger.info("Command cancelled by user.")
                        return False
                    line_clean = line.rstrip("\n\r")
                    level = _log_level_for_build_line(line_clean)
                    self.logger.log(level, line_clean)
            proc.wait(timeout=timeout)
            if proc.returncode != 0:
                self.logger.error(
                    "Command failed with error code {}: {}".format(
                        proc.returncode, " ".join(cmd_list)
                    )
                )
                return False
            self.logger.info("Command finished successfully.")
            return True
        except subprocess_TimeoutExpired:
            if proc is not None:
                try:
                    proc.terminate()
                    proc.wait(timeout=5)
                except Exception:
                    pass
            self.logger.error("Command timed out after {} seconds.".format(timeout))
            return False
        except subprocess_CalledProcessError as e:
            self.logger.error(
                "Command failed with error code {}: {}".format(
                    e.returncode, " ".join(cmd_list)
                )
            )
            return False
        except FileNotFoundError:
            self.logger.error(
                "Command not found: {}. Is it installed and in PATH?".format(
                    cmd_list[0]
                )
            )
            return False
        except Exception as e:
            self.logger.critical("An unexpected error occurred: {}".format(e))
            return False

    def _check_makeindex_available(self) -> bool:
        """
        Check if makeindex is available by running it with a timeout.
        makeindex waits for stdin input, so we need to interrupt it quickly.

        Returns:
            bool: True if makeindex is available, False otherwise
        """
        try:
            # Run makeindex with a short timeout - if it starts, it's available
            subprocess_run(
                ["makeindex"],
                check=True,
                text=True,
                capture_output=True,
                timeout=1,  # 1 second timeout
                input="",  # Send empty input to avoid hanging
                **_SUBPROCESS_NO_WINDOW,
            )
            return True
        except subprocess_TimeoutExpired:
            # Timeout is expected - makeindex started and is waiting for input
            return True
        except (subprocess_CalledProcessError, FileNotFoundError):
            # Command failed or not found
            return False
        except Exception:
            # Any other error
            return False

    def clean_build_dir(self) -> bool:
        """
        Description:
            Remove existing build directory.

        Returns:
            bool: True if cleaning succeeded or wasn't needed
        """
        if os_path_exists(self.build_dir):
            self.logger.info("Cleaning directory: {}".format(self.build_dir))
            try:
                shutil_rmtree(self.build_dir)
                self.logger.info("Cleaned successfully.")
                return True
            except OSError as e:
                self.logger.error(
                    "Error cleaning directory {}: {}".format(self.build_dir, e)
                )
                return False
        self.logger.info(
            "Directory {} does not exist, no need to clean.".format(self.build_dir)
        )
        return True

    def _read_cmake_cache_value(self, key: str) -> Optional[str]:
        """Read a single INTERNAL cache entry from CMakeCache.txt."""
        cache_path = os_path_join(self.build_dir, "CMakeCache.txt")
        if not os_path_exists(cache_path):
            return None
        try:
            with open(cache_path, "r", encoding="utf-8", errors="replace") as cache_file:
                content = cache_file.read()
        except OSError:
            return None
        pattern = r"{}:INTERNAL=(.*?)\n".format(re_escape(key))
        match = re_search(pattern, content)
        return match.group(1).strip() if match else None

    def _needs_generator_cache_clean(self) -> bool:
        """
        Return True when an existing CMake cache was produced with a different
        generator/platform than the current configure request (e.g. Ninja vs VS -A x64).
        """
        cached_generator = self._read_cmake_cache_value("CMAKE_GENERATOR")
        if not cached_generator:
            return False

        gen_override = getattr(self, "generator_override", None)
        want_ninja = bool(self.use_ninja or gen_override == "Ninja")
        cached_ninja = "Ninja" in cached_generator

        if want_ninja != cached_ninja:
            return True

        if gen_override and gen_override != cached_generator:
            return True

        if platform_system() == "Windows" and not want_ninja and not gen_override:
            cached_platform = self._read_cmake_cache_value("CMAKE_GENERATOR_PLATFORM") or ""
            want_platform = "Win32" if self.architecture == "x86" else "x64"
            if "Visual Studio" in cached_generator and cached_platform != want_platform:
                return True

        return False

    # MSVC platform toolsets: v120 (VS 2013), v140 (VS 2015), v141 (VS 2017), v142 (VS 2019),
    # v143 (VS 2022), v144 (VS 2025), v145 (VS 2026 and later).
    MSVC_TOOLSETS = (
        "v120",
        "v140",
        "v141",
        "v142",
        "v143",
        "v144",
        "v145",
    )

    def add_toolset(self, toolset: str) -> None:
        """
        Description:
            Adds a MSVC toolset to the CMake arguments.
            Supported toolsets: v120 (VS 2013), v140 (VS 2015), v141 (VS 2017),
            v142 (VS 2019), v143 (VS 2022), v144 (VS 2025), v145 (VS 2026+).
            For UNIX systems, the toolset is not supported, so this function does nothing.

        Args:
            toolset (str): The MSVC toolset to add.
        """

        if platform_system() == "Windows":
            if toolset not in self.MSVC_TOOLSETS:
                self.logger.warning("Unsupported MSVC toolset: {}".format(toolset))
                return

            self.cmake_args.append("-T")
            self.cmake_args.append("{},host={}".format(toolset, self.architecture))
            self.logger.info("🔍 Using MSVC toolset: {}".format(toolset))

    # Windows version compatibility for WIN32_WINNT: XP, VISTA, SEVEN, EIGHT, EIGHTDOTONE, TENELEVEN.
    WINDOWS_VERSIONS = ("XP", "VISTA", "SEVEN", "EIGHT", "EIGHTDOTONE", "TENELEVEN")

    def add_windows_version(self, windows_version: str) -> None:
        """
        Description:
            Sets Windows version compatibility (WIN32_WINNT) via CMake cache variable
            WINDOWS_VERSION. Supported values: XP, VISTA, SEVEN, EIGHT, EIGHTDOTONE, TENELEVEN.
            For UNIX systems, this function does nothing.

        Args:
            windows_version (str): The Windows version compatibility target.
        """

        if platform_system() == "Windows":
            if windows_version not in self.WINDOWS_VERSIONS:
                self.logger.warning(
                    "Unsupported Windows version: {}".format(windows_version)
                )
                return

            self.cmake_args.append("-DWINDOWS_VERSION={}".format(windows_version))
            self.logger.info(
                "🔍 Using Windows compatibility: {}".format(windows_version)
            )

    def add_architecture(self, architecture: str) -> None:
        """
        Description:
            Adds an architecture to the CMake arguments.
            Supported architectures: x86, x64.
        """
        if architecture not in ["x86", "x64"]:
            self.logger.warning("Unsupported architecture: {}".format(architecture))
            return
        self.architecture = architecture
        self.cmake_args.append("-DARCHITECTURE={}".format(architecture))
        self.logger.info("🔍 Using architecture: {}".format(architecture))

    def add_c_standard(self, c_standard: int, override: bool = False) -> None:
        """
        Description:
            Adds a C standard to the CMake arguments.
            Supported standards: 89, 99, 11, 17, 23.
            C89/C90 = C89 (ANSI C).
            C99 = C99 (ISO C99).
            C11 = C11 (ISO C11).
            C17 = C17 (ISO C17).
            C23 = C23 (ISO C23).
        """
        # Validate C standard - only allow specific supported standards
        supported_c_standards = [89, 99, 11, 17, 23]
        if c_standard not in supported_c_standards:
            self.logger.warning("Unsupported C standard: {}".format(c_standard))
            self.logger.warning(
                "Supported C standards: {}".format(
                    ", ".join(map(str, supported_c_standards))
                )
            )
            return

        current_c_standard_arg = None
        for arg in self.cmake_args:
            if arg.startswith("-DCMAKE_C_STANDARD="):
                current_c_standard_arg = arg
                break

        if current_c_standard_arg is None:
            self.logger.info("Setting C standard to C{}".format(c_standard))
            self.cmake_args.append("-DCMAKE_C_STANDARD={}".format(c_standard))
            self.c_version = int(c_standard)
        else:
            # Extract the current standard value from the argument string
            current_set_standard = current_c_standard_arg.split("=")[1]
            self.logger.warning(
                "C standard already set to C{}".format(current_set_standard)
            )
            self.c_version = int(current_set_standard)
            if override:
                self.logger.warning("Overriding C standard to C{}".format(c_standard))
                self.cmake_args.remove(current_c_standard_arg)
                self.cmake_args.append("-DCMAKE_C_STANDARD={}".format(c_standard))
                self.c_version = int(c_standard)

    def add_cpp_standard(self, cpp_standard: int, override: bool = False) -> None:
        """
        Description:
            Adds a C++ standard to the CMake arguments.
            Supported standards: 98, 03, 11, 14, 17, 20, 23, 26.
            C++98/C++03 = C++98/C++03 (ISO C++98/C++03).
            C++11 = C++11 (ISO C++11).
            C++14 = C++14 (ISO C++14).
            C++17 = C++17 (ISO C++17).
            C++20 = C++20 (ISO C++20).
            C++23 = C++23 (ISO C++23).
            C++26 = C++26 (ISO C++26).
        """
        # Validate C++ standard - only allow specific supported standards
        supported_cpp_standards = [98, 11, 14, 17, 20, 23, 26]
        if cpp_standard not in supported_cpp_standards:
            self.logger.warning("Unsupported C++ standard: {}".format(cpp_standard))
            self.logger.warning(
                "Supported C++ standards: {}".format(
                    ", ".join(map(str, supported_cpp_standards))
                )
            )
            return

        current_cpp_standard_arg = None
        for arg in self.cmake_args:
            if arg.startswith("-DCMAKE_CXX_STANDARD="):
                current_cpp_standard_arg = arg
                break

        if current_cpp_standard_arg is None:
            self.logger.info("Setting C++ standard to C++{}".format(cpp_standard))
            self.cmake_args.append("-DCMAKE_CXX_STANDARD={}".format(cpp_standard))
            self.cpp_version = int(cpp_standard)
        else:
            # Extract the current standard value from the argument string
            current_set_standard = current_cpp_standard_arg.split("=")[1]
            self.logger.warning(
                "C++ standard already set to C++{}".format(current_set_standard)
            )
            self.cpp_version = int(current_set_standard)
            if override:
                self.logger.warning(
                    "Overriding C++ standard to C++{}".format(cpp_standard)
                )
                self.cmake_args.remove(current_cpp_standard_arg)
                self.cmake_args.append("-DCMAKE_CXX_STANDARD={}".format(cpp_standard))
                self.cpp_version = int(cpp_standard)

    def validate_standard_compatibility(self) -> bool:
        """
        Validate that the selected C and C++ standards are compatible.

        Returns:
            bool: True if standards are compatible, False otherwise
        """
        # C23 requires C++23 or later
        if self.c_version == 23 and self.cpp_version < 23:
            self.logger.warning(
                "C23 standard typically requires C++23 or later for full compatibility"
            )
            return False

        # C17 requires C++17 or later for best compatibility
        if self.c_version == 17 and self.cpp_version < 17:
            self.logger.warning("C17 standard works best with C++17 or later")

        # C11 requires C++11 or later
        if self.c_version == 11 and self.cpp_version < 11:
            self.logger.warning("C11 standard requires C++11 or later")
            return False

        # C99 works with C++98/03 but warns
        if self.c_version == 99 and self.cpp_version < 11:
            self.logger.warning("C99 standard works best with C++11 or later")

        return True

    def show_supported_standards(self) -> None:
        """
        Display all supported C and C++ standards with descriptions.
        """
        self.logger.info("=== Supported C Standards ===")
        c_standards = {
            89: "C89/C90 (ANSI C) - Original C standard",
            99: "C99 (ISO C99) - Added inline, restrict, VLA, compound literals",
            11: "C11 (ISO C11) - Added _Generic, _Static_assert, atomics, threads",
            17: "C17 (ISO C17) - Bug fixes, no new features",
            23: "C23 (ISO C23) - Added nullptr, typeof, _BitInt, attributes",
        }

        for std, desc in c_standards.items():
            self.logger.info("  C{:2d}: {}".format(std, desc))

        self.logger.info("\n=== Supported C++ Standards ===")
        cpp_standards = {
            98: "C++98/C++03 (ISO C++98/C++03) - Original C++ standard",
            11: "C++11 (ISO C++11) - Added auto, lambda, move semantics, nullptr",
            14: "C++14 (ISO C++14) - Added auto return, generic lambdas, relaxed constexpr",
            17: "C++17 (ISO C++17) - Added if constexpr, structured bindings, filesystem",
            20: "C++20 (ISO C++20) - Added concepts, modules, coroutines, ranges",
            23: "C++23 (ISO C++23) - Added deducing this, multidimensional subscript",
            26: "C++26 (ISO C++26) - Added pattern matching, reflection, contracts",
        }

        for std, desc in cpp_standards.items():
            self.logger.info("  C++{:2d}: {}".format(std, desc))

        self.logger.info("\n=== Standard Compatibility Notes ===")
        self.logger.info("  > C23 requires C++23+ for full compatibility")
        self.logger.info("  > C17 works best with C++17+")
        self.logger.info("  > C11 requires C++11+")
        self.logger.info("  > C99 works with C++98/03 but C++11+ recommended")
        self.logger.info("  > C89 works with any C++ standard")

    def show_compiler_info(self) -> None:
        """
        Display comprehensive compiler detection and usage information.
        """
        self.logger.info("=== Supported Compilers ===")

        compilers = {
            "gcc": {
                "name": "GNU Compiler Collection (GCC)",
                "platforms": ["Linux", "macOS", "Windows (MinGW)", "Unix"],
                "standards": "C89-C23, C++98-C++26",
                "features": "LTO, PGO, sanitizers, vectorization",
                "flags": "-O2 -flto -march=native -Wall -Wextra",
            },
            "mingw": {
                "name": "MinGW-w64 (GCC for Windows)",
                "platforms": ["Windows"],
                "standards": "C89-C23, C++98-C++26",
                "features": "Native Windows binaries, GCC compatibility",
                "flags": "-O2 -flto -march=native -Wall -Wextra",
            },
            "clang": {
                "name": "Clang/LLVM",
                "platforms": ["Linux", "macOS", "Windows", "Unix"],
                "standards": "C89-C23, C++98-C++26",
                "features": "Fast compilation, excellent diagnostics, sanitizers",
                "flags": "-O2 -flto=thin -march=native -Wall -Wextra",
            },
            "clang-cl": {
                "name": "Clang-CL (Clang with MSVC compatibility)",
                "platforms": ["Windows"],
                "standards": "C89-C23, C++98-C++26",
                "features": "MSVC ABI compatibility, Clang diagnostics",
                "flags": "/O2 /GL /arch:AVX2 /W4",
            },
            "msvc": {
                "name": "Microsoft Visual C++ (MSVC)",
                "platforms": ["Windows"],
                "standards": "C89-C23, C++98-C++26",
                "features": "Windows integration, PGO, static analysis",
                "flags": "/O2 /GL /arch:AVX2 /W4 /GS",
            },
            "icc": {
                "name": "Intel C++ Compiler (ICC)",
                "platforms": ["Linux", "Windows", "macOS"],
                "standards": "C89-C23, C++98-C++26",
                "features": "Intel CPU optimization, auto-parallelization",
                "flags": "-O3 -xHost -ipo -qopt-report=5",
            },
            "borland": {
                "name": "Borland C++ Builder",
                "platforms": ["Windows"],
                "standards": "C89-C17, C++98-C++17",
                "features": "RAD development, Windows-specific",
                "flags": "-O2 -w- -tW",
            },
            "pgi": {
                "name": "PGI Compiler (NVIDIA HPC)",
                "platforms": ["Linux", "Windows"],
                "standards": "C89-C17, C++98-C++17",
                "features": "HPC optimization, GPU acceleration",
                "flags": "-O3 -fast -Mipa=fast",
            },
            "xlc": {
                "name": "IBM XL C/C++",
                "platforms": ["AIX", "Linux (Power)", "z/OS"],
                "standards": "C89-C17, C++98-C++17",
                "features": "Power architecture optimization",
                "flags": "-O3 -qhot -qarch=auto",
            },
            "aocc": {
                "name": "AMD Optimizing C/C++ Compiler (AOCC)",
                "platforms": ["Linux"],
                "standards": "C89-C23, C++98-C++26",
                "features": "AMD CPU optimization, LLVM-based",
                "flags": "-O3 -march=native -flto",
            },
            "armclang": {
                "name": "ARM Compiler (armclang)",
                "platforms": ["Linux (ARM)", "Windows (ARM)"],
                "standards": "C89-C23, C++98-C++26",
                "features": "ARM architecture optimization",
                "flags": "-O3 -mcpu=native -flto",
            },
            "nvcc": {
                "name": "NVIDIA CUDA Compiler (NVCC)",
                "platforms": ["Linux", "Windows", "macOS"],
                "standards": "C89-C17, C++98-C++17",
                "features": "GPU programming, CUDA kernels",
                "flags": "-O3 -arch=sm_XX -Xcompiler -O3",
            },
        }

        for compiler_id, info in compilers.items():
            self.logger.info("\n🔧 {} ({})".format(info["name"], compiler_id.upper()))
            self.logger.info("   Platforms: {}".format(", ".join(info["platforms"])))
            self.logger.info("   Standards: {}".format(info["standards"]))
            self.logger.info("   Features: {}".format(info["features"]))
            self.logger.info("   Flags: {}".format(info["flags"]))

        self.logger.info("\n=== Compiler Detection ===")
        self.logger.info("  > Automatic detection from CMakeCache.txt")
        self.logger.info("  > Manual specification with --compiler-c/--compiler-cpp")
        self.logger.info("  > Path-based detection for custom installations")
        self.logger.info("  > Version detection and compatibility checking")

        self.logger.info("\n=== Usage Examples ===")
        self.logger.info("  # Use system default compiler")
        self.logger.info("  python compile.py Release")
        self.logger.info("")
        self.logger.info("  # Specify custom GCC")
        self.logger.info(
            "  python compile.py Release --compiler-cpp /opt/gcc-14/bin/g++"
        )
        self.logger.info("")
        self.logger.info("  # Use Intel compiler")
        self.logger.info(
            "  python compile.py Release --compiler-cpp /opt/intel/bin/icpc"
        )
        self.logger.info("")
        self.logger.info("  # Use Clang-CL on Windows")
        self.logger.info("  python compile.py Release --compiler-cpp clang-cl")
        self.logger.info("")
        self.logger.info("  # Use MinGW on Windows")
        self.logger.info(
            "  python compile.py Release --compiler-cpp C:\\mingw64\\bin\\g++.exe"
        )

    def set_custom_c_compiler(self, compiler_path: str) -> None:
        """
        Set custom C compiler path.

        Args:
            compiler_path: Path to the C compiler executable
        """
        if not compiler_path:
            return

        # Validate compiler path exists
        if not os_path_exists(compiler_path):
            self.logger.warning(
                "C compiler path does not exist: {}".format(compiler_path)
            )
            return

        self.custom_c_compiler = os_path_abspath(compiler_path)
        self.cmake_args.append("-DCMAKE_C_COMPILER={}".format(self.custom_c_compiler))
        self.logger.info(
            "🔧 Using custom C compiler: {}".format(self.custom_c_compiler)
        )

    def set_custom_cpp_compiler(self, compiler_path: str) -> None:
        """
        Set custom C++ compiler path.

        Args:
            compiler_path: Path to the C++ compiler executable
        """
        if not compiler_path:
            return

        # Validate compiler path exists
        if not os_path_exists(compiler_path):
            self.logger.warning(
                "C++ compiler path does not exist: {}".format(compiler_path)
            )
            return

        self.custom_cpp_compiler = os_path_abspath(compiler_path)
        self.cmake_args.append(
            "-DCMAKE_CXX_COMPILER={}".format(self.custom_cpp_compiler)
        )
        self.logger.info(
            "🔧 Using custom C++ compiler: {}".format(self.custom_cpp_compiler)
        )

    def _detect_compiler_from_custom_path(self, compiler_path: str) -> str:
        """
        Detect compiler type from custom compiler path.

        Args:
            compiler_path: Path to the compiler executable

        Returns:
            str: Detected compiler type (gcc, clang, msvc, mingw, clang-cl, icc, borland, unknown)
        """
        if not compiler_path:
            return "unknown"

        compiler_name = os_path_basename(compiler_path).lower()

        # Clang family (check BEFORE GCC to avoid "g++" matching "clang++")
        if "clang-cl" in compiler_name:
            return "clang-cl"
        elif "clang" in compiler_name or "clang++" in compiler_name:
            return "clang"

        # GCC family
        elif "gcc" in compiler_name or "g++" in compiler_name:
            return "gcc"
        elif (
            "mingw" in compiler_name
            or "mingw32" in compiler_name
            or "mingw64" in compiler_name
        ):
            return "mingw"

        # Microsoft family
        elif "cl.exe" in compiler_name or "cl" in compiler_name:
            return "msvc"

        # Intel family
        elif (
            "icc" in compiler_name or "icpc" in compiler_name or "icl" in compiler_name
        ):
            return "icc"

        # Borland family
        elif (
            "bcc" in compiler_name
            or "bcc32" in compiler_name
            or "bcc64" in compiler_name
        ):
            return "borland"
        elif "bccx" in compiler_name:
            return "borland"

        # Other compilers
        elif "pgcc" in compiler_name or "pgc++" in compiler_name:
            return "pgi"
        elif "xlc" in compiler_name or "xlC" in compiler_name:
            return "xlc"
        elif (
            "aocc" in compiler_name
            or "clang" in compiler_name
            and "amd" in compiler_path.lower()
        ):
            return "aocc"
        elif "armclang" in compiler_name:
            return "armclang"
        elif "nvcc" in compiler_name:
            return "nvcc"
        else:
            return "unknown"

    def _detect_compiler_id_from_cache(self, cache_content: str) -> str:
        """
        Attempts to detect the C++ compiler ID and version from CMakeCache.txt content.
        Prioritizes CMAKE_CXX_COMPILER_ID, then CMAKE_GENERATOR, then CMAKE_CXX_COMPILER path.
        """
        detected_compiler = "unknown"
        detected_version_tag = ""

        # 1. Try to get CMAKE_CXX_COMPILER_ID directly
        match_id = re_search(r"CMAKE_CXX_COMPILER_ID:[^\n=]*=(.*?)\n", cache_content)
        if match_id:
            detected_compiler = match_id.group(1).strip().lower()
            self.logger.debug(
                "DEBUG: Found CMAKE_CXX_COMPILER_ID: {}".format(detected_compiler)
            )
        else:
            self.logger.debug("DEBUG: CMAKE_CXX_COMPILER_ID not found or regex failed.")

        # 2. Try to infer from CMAKE_GENERATOR (especially for MSVC)
        match_generator = re_search(r"CMAKE_GENERATOR:INTERNAL=(.*?)\n", cache_content)
        if match_generator:
            generator_name = match_generator.group(1)
            self.logger.debug("DEBUG: Found CMAKE_GENERATOR: {}".format(generator_name))
            if "Visual Studio" in generator_name:
                detected_compiler = "msvc"
                version_match = re_search(r"Visual Studio \d+ (\d{4})", generator_name)
                if version_match:
                    detected_version_tag = version_match.group(1)
                else:
                    match_instance = re_search(
                        r"CMAKE_GENERATOR_INSTANCE:INTERNAL=.*?\\(\d{4})\\",
                        cache_content,
                    )
                    if match_instance:
                        detected_version_tag = match_instance.group(1)
                self.logger.debug(
                    "DEBUG: Inferred MSVC compiler: {}, version: {}".format(
                        detected_compiler, detected_version_tag
                    )
                )
        else:
            self.logger.debug("DEBUG: CMAKE_GENERATOR not found.")

        # 3. If still 'unknown' or 'msvc' without a specific version, try to infer from CMAKE_CXX_COMPILER path
        if detected_compiler == "unknown" or (
            detected_compiler == "msvc" and not detected_version_tag
        ):
            match_path = re_search(
                r"CMAKE_CXX_COMPILER:FILEPATH=(.*?)\n", cache_content
            )
            if match_path:
                compiler_path = match_path.group(1)
                self.logger.debug(
                    "DEBUG: Found CMAKE_CXX_COMPILER path: {}".format(compiler_path)
                )
                compiler_exe = os_path_basename(compiler_path).lower()

                # Check Clang BEFORE GCC to avoid "g++" matching "clang++"
                if "clang-cl" in compiler_exe:
                    detected_compiler = "clang-cl"
                elif (
                    "clang++" in compiler_exe
                    or "clang.exe" in compiler_exe
                    or "clang" in compiler_exe
                ):
                    detected_compiler = "clang"
                elif "cl.exe" in compiler_exe:
                    detected_compiler = "msvc"
                    msvc_version_path_match = re_search(
                        r"MSVC\\(\d+\.\d+)\.\d+\\", compiler_path
                    )
                    if msvc_version_path_match:
                        major_minor = msvc_version_path_match.group(1).split(".")
                        if major_minor[0] == "14":
                            if (
                                major_minor[1] == "29"
                                or major_minor[1] == "30"
                                or major_minor[1] == "31"
                            ):
                                detected_version_tag = "2022"
                            elif major_minor[1] == "16":
                                detected_version_tag = "2017"
                            elif major_minor[1] == "14":
                                detected_version_tag = "2015"
                elif (
                    "g++" in compiler_exe
                    or "gcc.exe" in compiler_exe
                    or compiler_exe == "c++"
                ):
                    detected_compiler = "gcc"
                elif (
                    "mingw" in compiler_exe
                    or "mingw32" in compiler_exe
                    or "mingw64" in compiler_exe
                ):
                    detected_compiler = "mingw"
                elif (
                    "icc" in compiler_exe
                    or "icpc" in compiler_exe
                    or "icl" in compiler_exe
                ):
                    detected_compiler = "icc"
                elif (
                    "bcc" in compiler_exe
                    or "bcc32" in compiler_exe
                    or "bcc64" in compiler_exe
                    or "bccx" in compiler_exe
                ):
                    detected_compiler = "borland"
                elif "pgcc" in compiler_exe or "pgc++" in compiler_exe:
                    detected_compiler = "pgi"
                elif "xlc" in compiler_exe or "xlc++" in compiler_exe:
                    detected_compiler = "xlc"
                elif "aocc" in compiler_exe:
                    detected_compiler = "aocc"
                elif "armclang" in compiler_exe:
                    detected_compiler = "armclang"
                elif "nvcc" in compiler_exe:
                    detected_compiler = "nvcc"
                self.logger.debug(
                    "DEBUG: Inferred compiler from path: {}, version: {}".format(
                        detected_compiler, detected_version_tag
                    )
                )
            else:
                self.logger.debug("DEBUG: CMAKE_CXX_COMPILER path not found.")

        # Save found version to self.compiler_version_tag
        self.compiler_version_tag = detected_version_tag
        self.logger.debug(
            "DEBUG: Final detected compiler: {}, version tag: {}".format(
                detected_compiler, self.compiler_version_tag
            )
        )

        return detected_compiler

    def configure(
        self, build_type: str, cancel_check: Optional[Callable[[], bool]] = None
    ) -> bool:
        """
        Configure CMake project.

        Args:
            build_type: Build type (Debug/Release)
            cancel_check: Optional callable returning True to abort (GUI Stop).

        Returns:
            bool: True if configuration succeeded
        """
        self.build_type = build_type  # Store build type for later use
        os_makedirs(self.build_dir, exist_ok=True)

        if self._needs_generator_cache_clean():
            cached_generator = self._read_cmake_cache_value("CMAKE_GENERATOR") or "unknown"
            cached_platform = self._read_cmake_cache_value("CMAKE_GENERATOR_PLATFORM") or ""
            requested = "Ninja" if self.use_ninja else "Visual Studio (-A {})".format(
                "Win32" if self.architecture == "x86" else "x64"
            )
            self.logger.warning(
                "Existing CMake cache uses generator '{}' (platform='{}'), but configure "
                "requested '{}'. Cleaning build directory to avoid generator/platform mismatch.".format(
                    cached_generator, cached_platform, requested
                )
            )
            if not self.clean_build_dir():
                return False

        cmake_configure_cmd = [
            "cmake",
            self.project_root,
            "-DCMAKE_BUILD_TYPE={}".format(build_type),
            "-B",
            self.build_dir,
        ]

        # Generator: override (e.g. NMake for compile_commands on Windows) or Ninja
        gen_override = getattr(self, "generator_override", None)
        if gen_override:
            cmake_configure_cmd.extend(["-G", gen_override])
            self.logger.info(
                "Using generator (override for compile_commands): {}".format(
                    gen_override
                )
            )
        elif self.use_ninja:
            if not self.ninja_executable:
                self._check_ninja_available()
            cmake_configure_cmd.extend(["-G", "Ninja"])
            if self.ninja_executable:
                # Pin CMake to this exact Ninja binary. Avoids Scoop/shim/.bat wrapper issues
                # that can cause "rules.ninja not found" during try_compile (CMake 4.x + MSVC).
                ninja_for_cmake = self.ninja_executable.replace("\\", "/")
                cmake_configure_cmd.append(
                    "-DCMAKE_MAKE_PROGRAM={}".format(ninja_for_cmake)
                )
            self.logger.info("Using Ninja build system generator")

        if getattr(self, "debug_trycompile", False):
            cmake_configure_cmd.append("--debug-trycompile")
            cmake_configure_cmd.append("--debug-output")
            self.logger.info(
                "Preserving TryCompile scratch dirs (--debug-trycompile) and verbose CMake log (--debug-output)"
            )

        # Add architecture/platform specific flags for Visual Studio generators (not for Ninja/NMake)
        if platform_system() == "Windows" and not self.use_ninja and not gen_override:
            if self.architecture == "x86":
                cmake_configure_cmd.extend(["-A", "Win32"])
                self.logger.info("Setting Visual Studio Platform: Win32 (for x86)")
            elif self.architecture == "x64":
                cmake_configure_cmd.extend(["-A", "x64"])
                self.logger.info("Setting Visual Studio Platform: x64")

        if self.cmake_args:
            filtered_cmake_args = [
                arg
                for arg in self.cmake_args
                if not arg.startswith("-DCMAKE_BUILD_TYPE")
                and not arg.startswith(
                    "-A"
                )  # Avoid duplicating -A if it was manually added
            ]
            cmake_configure_cmd.extend(filtered_cmake_args)

        # For MSVC + Ninja on Windows, run CMake from a vcvarsall-backed cmd session.
        # This avoids CMake 4.x try_compile/scratch issues where child ninja invocations
        # may fail with missing CMakeFiles/rules.ninja. We also pass CMAKE_MAKE_PROGRAM
        # (when ninja_executable is set) so CMake uses the real ninja.exe, not a shim/.bat.
        try:
            if self._should_run_cmake_via_msvc_batch():
                if not self._run_cmake_via_msvc_batch(
                    cmake_configure_cmd, cancel_check=cancel_check
                ):
                    return False
            elif not self.run_command(cmake_configure_cmd, cancel_check=cancel_check):
                return False

            # After successful configuration, try to determine the compiler ID and version
            # If custom compilers were specified, detect from their paths first
            if self.custom_cpp_compiler:
                detected_from_path = self._detect_compiler_from_custom_path(
                    self.custom_cpp_compiler
                )
                if detected_from_path != "unknown":
                    self.compiler_id = detected_from_path
                    self.logger.info(
                        "Detected compiler from custom path: {}".format(
                            detected_from_path.upper()
                        )
                    )

            cache_file_path = os_path_join(self.build_dir, "CMakeCache.txt")
            if os_path_exists(cache_file_path):
                with open(cache_file_path, "r") as f:
                    content = f.read()

                # Call updated method to determine ID and version
                detected_from_cache = self._detect_compiler_id_from_cache(content)

                # Use cache detection if no custom compiler was detected
                if not self.custom_cpp_compiler or self.compiler_id == "unknown":
                    self.compiler_id = detected_from_cache

                compiler_display_name = self.compiler_id.upper()
                if self.compiler_version_tag:
                    compiler_display_name += self.compiler_version_tag

                if self.compiler_id != "unknown":
                    self.logger.info(
                        "Detected compiler: {}".format(compiler_display_name)
                    )
                else:
                    self.logger.warning(
                        "Could not detect C++ compiler ID from CMakeCache.txt. Defaulting to 'unknown'."
                    )
            else:
                self.logger.warning("CMakeCache.txt not found after configuration.")
        except Exception as e:
            self.logger.error("Error detecting compiler ID: {}".format(e))
            self.compiler_id = "unknown"
            self.compiler_version_tag = ""  # Reset version tag on error
        finally:
            self.debug_trycompile = False
            self.generator_override = None

        return True

    def _should_run_cmake_via_msvc_batch(self) -> bool:
        """Return True when CMake should run from a vcvarsall-backed cmd session."""
        if platform_system() != "Windows":
            return False
        if getattr(self, "generator_override", None) == "NMake Makefiles":
            return True
        if not self.use_ninja:
            return False
        compiler_path = (self.custom_cpp_compiler or "").lower()
        compiler_name = os_path_basename(compiler_path)
        return (
            compiler_name in ("cl.exe", "cl")
            or "clang-cl" in compiler_name
        )

    def _run_cmake_via_msvc_batch(
        self, cmake_cmd: List[str], cancel_check: Optional[Callable[[], bool]] = None
    ) -> bool:
        """
        Run a CMake command through cmd.exe after calling vcvarsall.bat.
        Ensures CMake and all child processes share a complete MSVC environment.
        """
        vcvarsall_path = self._find_vcvarsall()
        if not vcvarsall_path:
            self.logger.warning(
                "vcvarsall.bat not found. Falling back to direct CMake execution."
            )
            return self.run_command(cmake_cmd, cancel_check=cancel_check)

        arch_arg = "x64" if self.architecture == "x64" else "x86"
        temp_bat_path = None

        try:
            with tempfile_NamedTemporaryFile(
                mode="w", suffix=".bat", delete=False, encoding="utf-8"
            ) as temp_bat:
                temp_bat.write("@echo off\n")
                temp_bat.write('cd /d "{}"\n'.format(self.project_root))
                temp_bat.write(
                    'call "{}" {} >nul 2>&1\n'.format(vcvarsall_path, arch_arg)
                )
                temp_bat.write("if errorlevel 1 exit /b %errorlevel%\n")
                temp_bat.write("{}\n".format(subprocess_list2cmdline(cmake_cmd)))
                temp_bat.write("exit /b %errorlevel%\n")
                temp_bat_path = temp_bat.name

            self.logger.info(
                "Running CMake via MSVC batch wrapper: {}".format(vcvarsall_path)
            )
            return self.run_command(
                ["cmd", "/d", "/c", temp_bat_path],
                cwd=self.project_root,
                cancel_check=cancel_check,
            )
        finally:
            if temp_bat_path and os_path_exists(temp_bat_path):
                try:
                    os_remove(temp_bat_path)
                except OSError:
                    pass

    def add_cmake_prefix_path(self, prefix_paths: List[str]) -> None:
        """
        Description:
            Adds prefix paths to the CMake arguments.
            Multiple paths will be joined with the platform-specific path separator.

        Args:
            prefix_paths: List of prefix paths to add.

        Examples:
            ["/opt/Qt5.15.17", "F:\\local\\Qt\\6.5.3\\msvc2019_64\\lib\\cmake\\Qt6"]
        """
        if not prefix_paths:
            return

        new_prefix_string = os_pathsep.join(prefix_paths)

        found = False
        for i, arg in enumerate(self.cmake_args):
            if arg.startswith("-DCMAKE_PREFIX_PATH="):
                # If CMAKE_PREFIX_PATH already exists, append to it
                existing_paths = arg.split("=", 1)[
                    1
                ]  # Use 1 to split only on the first '='
                self.cmake_args[i] = "-DCMAKE_PREFIX_PATH={}{}{}".format(
                    existing_paths, os_pathsep, new_prefix_string
                )
                self.logger.info(
                    "Updated CMAKE_PREFIX_PATH to: {}".format(self.cmake_args[i])
                )
                found = True
                break

        if not found:
            self.cmake_args.append("-DCMAKE_PREFIX_PATH={}".format(new_prefix_string))
            self.logger.info("Added CMAKE_PREFIX_PATH: {}".format(new_prefix_string))

    def add_cmake_args(self, cmake_args: List[str]) -> None:
        """
        Description:
            Adds additional CMake arguments to the builder.

        Args:
            cmake_args: List of additional CMake arguments to add.

        Examples:
            ["-DCMAKE_PREFIX_PATH=/path/to/qt", "-DCMAKE_INSTALL_PREFIX=/path/to/install"]
        """
        self.cmake_args.extend(cmake_args)

    def activate_shared_libs(self) -> None:
        """
        Description:
            Activates shared libraries.
            Adds -DBUILD_SHARED_LIBS=ON to CMake arguments.
        """
        self.cmake_args.append("-DBUILD_SHARED_LIBS=ON")
        self.logger.info("Activated shared libraries by adding -DBUILD_SHARED_LIBS=ON.")

    def enable_ninja(self, compiler_type: Optional[str] = None) -> bool:
        """
        Enable Ninja build system for CMake.
        Optionally auto-detect and use a specific compiler type.

        Args:
            compiler_type: Type of compiler to auto-detect and use.
                          Options: 'msvc', 'clang', 'clang-cl', 'gcc', or None (auto-detect MSVC on Windows).

        Returns:
            bool: True if Ninja is available and enabled, False if not available
        """
        if not self._check_ninja_available():
            self._show_ninja_installation_instructions()
            return False

        self.use_ninja = True
        self.logger.info("Ninja build system enabled")

        # If custom compiler is already set, don't auto-detect
        if self.custom_cpp_compiler:
            return True

        # Auto-detect compiler based on type or default behavior
        compiler_path = None
        compiler_name = None

        if compiler_type:
            compiler_type_lower = compiler_type.lower()
            if compiler_type_lower == "msvc":
                # Use current architecture to find the correct compiler
                compiler_path = self._find_msvc_compiler(self.architecture)
                compiler_name = "MSVC"
            elif compiler_type_lower == "clang":
                compiler_path = self._find_clang_compiler()
                compiler_name = "Clang"
            elif compiler_type_lower == "clang-cl":
                compiler_path = self._find_clang_cl_compiler()
                compiler_name = "Clang-CL"
            elif compiler_type_lower == "gcc":
                compiler_path = self._find_gcc_compiler()
                compiler_name = "GCC"
            else:
                self.logger.warning(
                    "⚠️  Unknown compiler type: {}. Available: msvc, clang, clang-cl, gcc".format(
                        compiler_type
                    )
                )
        elif platform_system() == "Windows":
            # Default behavior: try MSVC on Windows
            # Use current architecture to find the correct compiler
            compiler_path = self._find_msvc_compiler(self.architecture)
            compiler_name = "MSVC"

        if compiler_path:
            # Verify architecture matches for x64 (critical for correct linking)
            if compiler_name == "MSVC" and self.architecture == "x64":
                path_lower = compiler_path.lower()
                # For x64, path must contain x64 and should NOT use Hostx86\x86
                if "hostx86" in path_lower and "x86" in path_lower:
                    self.logger.warning(
                        "⚠️  WARNING: Found x86 compiler ({}) for x64 architecture! "
                        "This will cause linking errors. Searching for correct x64 compiler...".format(
                            compiler_path
                        )
                    )
                    # Continue searching - don't use this compiler
                    compiler_path = None
                elif "x64" not in path_lower or (
                    "hostx86" in path_lower
                    and "x64" not in path_lower.replace("hostx64", "")
                ):
                    self.logger.warning(
                        "⚠️  WARNING: Compiler path ({}) may not match x64 architecture. "
                        "Expected path should contain Hostx64\\x64\\cl.exe".format(
                            compiler_path
                        )
                    )

        if compiler_path:
            # Set compiler for both C and C++
            self.set_custom_cpp_compiler(compiler_path)

            # Determine compiler type for C compiler detection
            detected_type = (
                compiler_type.lower()
                if compiler_type
                else "msvc" if compiler_name == "MSVC" else None
            )

            # For MSVC and Clang-CL, try to find C compiler in the same directory
            if detected_type in ["msvc", "clang-cl"]:
                compiler_dir = os_path_dirname(compiler_path)
                if detected_type == "msvc":
                    cl_c_path = os_path_join(compiler_dir, "cl.exe")
                else:  # clang-cl
                    cl_c_path = os_path_join(compiler_dir, "clang-cl.exe")

                if os_path_exists(cl_c_path):
                    self.set_custom_c_compiler(cl_c_path)
            elif detected_type in ["clang", "gcc"]:
                # For Clang and GCC, try to find C compiler (clang/gcc instead of clang++/g++)
                compiler_dir = os_path_dirname(compiler_path)
                compiler_exe = os_path_basename(compiler_path)
                if "++" in compiler_exe:
                    c_compiler_exe = compiler_exe.replace("++", "")
                    c_compiler_path = os_path_join(compiler_dir, c_compiler_exe)
                    if os_path_exists(c_compiler_path):
                        self.set_custom_c_compiler(c_compiler_path)
            elif not compiler_type and compiler_name == "MSVC":
                # Default MSVC case (when compiler_type is None but we found MSVC)
                compiler_dir = os_path_dirname(compiler_path)
                cl_c_path = os_path_join(compiler_dir, "cl.exe")
                if os_path_exists(cl_c_path):
                    self.set_custom_c_compiler(cl_c_path)

            self.logger.info(
                "🔧 Automatically using {} compiler with Ninja: {}".format(
                    compiler_name, compiler_path
                )
            )
        elif compiler_type:
            self.logger.warning(
                "⚠️  {} compiler not found. CMake will use default compiler.".format(
                    compiler_name
                )
            )
            self.logger.warning(
                "   To use a specific compiler, specify it explicitly: --compiler-cpp <path>"
            )
        elif platform_system() == "Windows":
            self.logger.warning(
                "⚠️  MSVC compiler not found. CMake will use default compiler (may be MinGW)."
            )
            self.logger.warning(
                "   To use a specific compiler, use: --use-ninja <compiler_type>"
            )
            self.logger.warning(
                "   Available compiler types: msvc, clang, clang-cl, gcc"
            )

        return True

    def build(
        self, build_type: str, cancel_check: Optional[Callable[[], bool]] = None
    ) -> bool:
        """
        Build the configured project with parallel job count from self.parallel_jobs
        or auto-calculated from CPU and memory.

        Args:
            build_type: Build type (Debug/Release)
            cancel_check: Optional callable returning True to abort (GUI Stop).

        Returns:
            bool: True if build succeeded
        """
        max_threads = psutil_cpu_count(logical=True) or 1

        jobs_override = getattr(self, "parallel_jobs", None)
        if jobs_override is not None:
            parallel_jobs = max(1, min(int(jobs_override), max_threads))
        else:
            # Get available CPU cores and free memory
            cpu_cores = psutil_cpu_count(logical=False) or 1
            free_memory = psutil_virtual_memory().available / (
                1024**3
            )  # Free memory in GB

            if free_memory < 2.0:  # Less than 2GB free memory
                parallel_jobs = 1
            elif free_memory < 4.0:  # Less than 4GB free memory
                parallel_jobs = max(1, cpu_cores // 2)
            else:
                parallel_jobs = min(cpu_cores, max_threads)

        cmake_build_cmd = [
            "cmake",
            "--build",
            self.build_dir,
            "--config",
            build_type,
            "--parallel",
            str(parallel_jobs),
        ]
        return self.run_command(cmake_build_cmd, cancel_check=cancel_check)

    def _check_documentation_dependencies(self, doc_format: str) -> bool:
        """
        Check if required dependencies for documentation generation are available.

        Args:
            doc_format: Documentation format ('html', 'pdf', or 'all')

        Returns:
            bool: True if all required dependencies are available, False otherwise
        """
        missing_deps = []

        # Check for doxygen (required for all formats)
        if not self.run_command(["doxygen", "--version"], capture_output=True):
            missing_deps.append("doxygen")

        # Check for PDF-specific dependencies
        if doc_format in ["pdf", "all"]:
            # Check for pdflatex
            if not self.run_command(["pdflatex", "--version"], capture_output=True):
                missing_deps.append("pdflatex")

            # Check for makeindex (MiKTeX version doesn't support --version, so check differently)
            if not self._check_makeindex_available():
                missing_deps.append("makeindex")

        if missing_deps:
            self.logger.error(
                "Missing required dependencies: {}".format(", ".join(missing_deps))
            )
            self.logger.error("Please install the missing tools and try again")
            return False

        self.logger.info("All required documentation dependencies found")
        return True

    def _check_doxyfile_exists(self) -> bool:
        """
        Check if Doxyfile exists in the project root.

        Returns:
            bool: True if Doxyfile exists, False otherwise
        """
        doxyfile_path = os_path_join(self.project_root, "Doxyfile")
        if not os_path_exists(doxyfile_path):
            self.logger.error("Doxyfile not found at: {}".format(doxyfile_path))
            self.logger.error("Please ensure Doxyfile exists in the project root")
            return False

        self.logger.info("Doxyfile found at: {}".format(doxyfile_path))
        return True

    def generate_html_documentation(self) -> bool:
        """
        Generate HTML documentation using Doxygen.

        Returns:
            bool: True if HTML generation succeeded, False otherwise
        """
        self.logger.info("Generating HTML documentation...")

        doxygen_cmd = ["doxygen", "Doxyfile"]
        if not self.run_command(doxygen_cmd, cwd=self.project_root):
            self.logger.error("HTML documentation generation failed")
            return False

        # Check if HTML documentation was generated
        html_dir = os_path_join(self.project_root, "docs", "html")
        index_file = os_path_join(html_dir, "index.html")

        if os_path_exists(index_file):
            self.logger.info(
                "HTML documentation generated successfully: {}".format(index_file)
            )
            return True
        else:
            self.logger.error(
                "HTML documentation generation completed but index.html not found"
            )
            return False

    def generate_pdf_documentation(self) -> bool:
        """
        Generate PDF documentation using Doxygen + LaTeX.

        Returns:
            bool: True if PDF generation succeeded, False otherwise
        """
        self.logger.info("Generating PDF documentation...")

        # First, ensure LaTeX files are generated
        latex_dir = os_path_join(self.project_root, "docs", "latex")
        if not os_path_exists(latex_dir):
            self.logger.info("LaTeX files not found, generating them first...")
            if not self.generate_html_documentation():
                self.logger.error("Failed to generate LaTeX files")
                return False

        # Change to LaTeX directory for PDF generation
        original_cwd = os_path_abspath(".")
        try:
            # Change to LaTeX directory
            import os

            os.chdir(latex_dir)
            self.logger.info("Changed to LaTeX directory: {}".format(latex_dir))

            # Generate PDF using pdflatex (multiple passes for proper cross-references)
            self.logger.info("Running pdflatex (first pass)...")
            if not self.run_command(
                ["pdflatex", "-interaction=nonstopmode", "refman.tex"]
            ):
                self.logger.error("LaTeX first pass failed")
                return False

            self.logger.info("Running makeindex...")
            if not self.run_command(["makeindex", "refman.idx"]):
                self.logger.error("Index generation failed")
                return False

            self.logger.info("Running pdflatex (second pass)...")
            if not self.run_command(
                ["pdflatex", "-interaction=nonstopmode", "refman.tex"]
            ):
                self.logger.error("LaTeX second pass failed")
                return False

            self.logger.info("Running pdflatex (final pass)...")
            if not self.run_command(
                ["pdflatex", "-interaction=nonstopmode", "refman.tex"]
            ):
                self.logger.error("LaTeX final pass failed")
                return False

            # Check if PDF was generated
            pdf_file = "refman.pdf"
            if os_path_exists(pdf_file):
                # Get file size for information
                try:
                    pdf_size = os.path.getsize(pdf_file) / (1024 * 1024)  # Size in MB
                    self.logger.info(
                        "PDF documentation generated successfully: {}".format(
                            os_path_join(latex_dir, pdf_file)
                        )
                    )
                    self.logger.info("PDF file size: {:.2f} MB".format(pdf_size))
                except OSError:
                    self.logger.info(
                        "PDF documentation generated successfully: {}".format(
                            os_path_join(latex_dir, pdf_file)
                        )
                    )
                return True
            else:
                self.logger.error("PDF generation completed but refman.pdf not found")
                return False

        except Exception as e:
            self.logger.error("Unexpected error during PDF generation: {}".format(e))
            return False
        finally:
            # Return to original directory
            try:
                os_chdir(original_cwd)
            except OSError:
                pass  # Ignore directory change errors

    def generate_documentation(self, doc_format: str) -> bool:
        """
        Generate documentation in the specified format.

        Args:
            doc_format: Documentation format ('html', 'pdf', or 'all')

        Returns:
            bool: True if documentation generation succeeded, False otherwise
        """
        self.logger.info(
            "Starting documentation generation in format: {}".format(doc_format)
        )

        # Check if Doxyfile exists
        if not self._check_doxyfile_exists():
            return False

        # Check dependencies
        if not self._check_documentation_dependencies(doc_format):
            return False

        # Generate documentation based on format
        success = True

        if doc_format in ["html", "all"]:
            if not self.generate_html_documentation():
                success = False

        if doc_format in ["pdf", "all"]:
            if not self.generate_pdf_documentation():
                success = False

        if success:
            self.logger.info("Documentation generation completed successfully")

            # Display results
            self.logger.info("=== Documentation Generation Results ===")
            if doc_format in ["html", "all"]:
                html_dir = os_path_join(self.project_root, "docs", "html")
                self.logger.info(
                    "HTML documentation: {}".format(
                        os_path_join(html_dir, "index.html")
                    )
                )

            if doc_format in ["pdf", "all"]:
                latex_dir = os_path_join(self.project_root, "docs", "latex")
                self.logger.info(
                    "PDF documentation: {}".format(
                        os_path_join(latex_dir, "refman.pdf")
                    )
                )
        else:
            self.logger.error("Documentation generation failed")

        return success

    def _check_packaging_configuration(self) -> bool:
        """
        Check if CMakeLists.txt contains packaging configuration.

        Uses regex patterns to detect CPack-related configuration in CMakeLists.txt.
        Returns True if any packaging configuration is found, False otherwise.

        Returns:
            bool: True if packaging configuration is found, False otherwise
        """
        cmake_list_path = os_path_join(self.project_root, "CMakeLists.txt")

        if not os_path_exists(cmake_list_path):
            self.logger.warning("CMakeLists.txt not found in project root")
            return False

        try:
            with open(cmake_list_path, "r", encoding="utf-8") as f:
                content: str = f.read()

            # Compile regex patterns for better performance
            packaging_patterns: List[Optional[object]] = [
                re_search(r"include\(CPack\)", content, re_DOTALL),
                re_search(r"set\(CPACK_", content, re_DOTALL),
                re_search(r"CPACK_PACKAGE", content, re_DOTALL),
                re_search(r"CPACK_GENERATOR", content, re_DOTALL),
                re_search(r"CPACK_COMPONENTS", content, re_DOTALL),
            ]

            # Check if any pattern matches
            if any(pattern is not None for pattern in packaging_patterns):
                self.logger.info("Packaging configuration found in CMakeLists.txt")
                return True

            self.logger.warning("No packaging configuration found in CMakeLists.txt")
            return False

        except (OSError, IOError) as e:
            self.logger.error("Error reading CMakeLists.txt: {}".format(e))
            return False
        except Exception as e:
            self.logger.error(
                "Unexpected error checking packaging configuration: {}".format(e)
            )
            return False

    def cpack(self, build_type: str) -> bool:
        """
        Description:
            Generates an installer package using CPack with detailed naming.

        Args:
            build_type: Build type (Debug/Release/RelWithDebInfo)

        Returns:
            bool: True if installer generation succeeded, False otherwise
        """
        # Project name from root CMakeLists.txt (project(...))
        project_name = get_project_name(self.project_root)

        # Create detailed installer name
        compiler_tag_part = self.compiler_id
        if self.compiler_version_tag:
            compiler_tag_part += self.compiler_version_tag
        elif self.compiler_id == "unknown":
            compiler_tag_part = "unknown"

        # Generate detailed package name
        package_name = "{}-{}-{}_{}_".format(
            project_name, self.version, self.os_prefix, self.architecture
        ) + "{}_cpp{}".format(compiler_tag_part, self.cpp_version)

        # Determine appropriate packaging generator based on platform
        if platform_system() == "Windows":
            primary_generator = "NSIS"
        elif platform_system() == "Darwin":  # macOS
            primary_generator = "DragNDrop"
        else:  # Linux/Unix
            primary_generator = "TGZ"

        # Set CPack variables for detailed naming
        cpack_cmd = [
            "cpack",
            "-G",
            primary_generator,
            "-C",
            build_type,
            "-D",
            "CPACK_PACKAGE_NAME={}".format(project_name),
            "-D",
            "CPACK_PACKAGE_FILE_NAME={}".format(package_name),
            "-D",
            "CPACK_PACKAGE_VERSION={}".format(self.version),
            "-D",
            "CPACK_PACKAGE_DESCRIPTION_SUMMARY={} {} ({}, C++{})".format(
                project_name, self.version, compiler_tag_part, self.cpp_version
            ),
        ]

        # Determine file extension based on generator
        if primary_generator == "NSIS":
            file_ext = ".exe"
        elif primary_generator == "TGZ":
            file_ext = ".tar.gz"
        elif primary_generator == "DEB":
            file_ext = ".deb"
        elif primary_generator == "RPM":
            file_ext = ".rpm"
        elif primary_generator == "ZIP":
            file_ext = ".zip"
        elif primary_generator == "DragNDrop":
            file_ext = ".dmg"
        else:
            file_ext = ""

        self.logger.info("📦 Generating package: {}{}".format(package_name, file_ext))
        return self.run_command(cpack_cmd, cwd=self.build_dir)

    def install(self, install_prefix: str) -> bool:
        """
        Description:
            Installs the built project to the specified prefix.

        Args:
            install_prefix: Path to install the project to

        Returns:
            bool: True if installation succeeded, False otherwise
        """
        compiler_tag_part = self.compiler_id
        if self.compiler_version_tag:
            compiler_tag_part += self.compiler_version_tag
        elif self.compiler_id == "unknown":
            compiler_tag_part = "unknown"

        lib_prefix = (
            self.version
            + "_"
            + self.os_prefix
            + "_"
            + self.architecture
            + "_"
            + compiler_tag_part
            + "_c"
            + str(self.c_version)
            + "_cpp"
            + str(self.cpp_version)
        )

        full_install_path = install_prefix + "/" + lib_prefix

        self.logger.info("📁 Installing to: {}".format(full_install_path))
        self.logger.info("   Version: {}".format(self.version))
        self.logger.info("   Platform: {}_{}".format(self.os_prefix, self.architecture))
        self.logger.info("   Compiler: {}".format(compiler_tag_part))
        self.logger.info(
            "   Standards: C{}, C++{}".format(self.c_version, self.cpp_version)
        )

        install_cmd = [
            "cmake",
            "--install",
            self.build_dir,
            "--prefix",
            full_install_path,
            "--config",
            self.build_type,
        ]
        return self.run_command(install_cmd)

    def dump_cmake_variables(self) -> bool:
        """
        Description:
            Dumps all CMake constants that can be passed to CMakeLists.txt.

        Returns:
            bool: True if dumping succeeded, False otherwise
        """
        from glob import glob as glob_glob
        from tempfile import mkdtemp as tempfile_mkdtemp

        temp_build_dir = None
        generated_files = []

        try:
            # Record existing files before CMake generation
            existing_files = set()
            for pattern in ["*.cmake", "CMakeCache.txt", "CMakeFiles"]:
                existing_files.update(
                    glob_glob(os_path_join(self.project_root, pattern))
                )

            # Create a temporary build directory
            temp_build_dir = tempfile_mkdtemp(prefix="cmake_dump_")

            # Configure the project to populate cache
            configure_cmd = ["cmake", self.project_root, "-B", temp_build_dir]
            configure_result = subprocess_run(
                configure_cmd,
                check=True,
                text=True,
                capture_output=True,
                **_SUBPROCESS_NO_WINDOW,
            )

            if not configure_result.returncode == 0:
                self.logger.error(
                    "Error configuring the project: {}".format(configure_result.stderr)
                )
                return False

            # Now dump the cache variables
            dump_cmd = ["cmake", "-LA", temp_build_dir]
            dump_result = subprocess_run(
                dump_cmd,
                check=True,
                text=True,
                capture_output=False,
                **_SUBPROCESS_NO_WINDOW,
            )

            if not dump_result.returncode == 0:
                self.logger.error(
                    "Error dumping CMake constants: {}".format(dump_result.stderr)
                )
                return False

            # Record files that were generated after CMake run
            current_files = set()
            for pattern in ["*.cmake", "CMakeCache.txt", "CMakeFiles"]:
                current_files.update(
                    glob_glob(os_path_join(self.project_root, pattern))
                )

            generated_files = list(current_files - existing_files)

            self.logger.info("CMake constants dumped successfully.")
            return True

        except subprocess_CalledProcessError as e:
            self.logger.error(
                "Error dumping CMake constants: {}".format(
                    e.stderr if e.stderr else e.stdout
                )
            )
            return False
        except FileNotFoundError:
            self.logger.error("CMake not found. Is it installed and in PATH?")
            return False
        except Exception as e:
            self.logger.critical(
                "An unexpected error occurred during dumping: {}".format(e)
            )
            return False
        finally:
            # Clean up temporary directory
            if temp_build_dir:
                try:
                    shutil_rmtree(temp_build_dir)
                except OSError:
                    pass  # Ignore cleanup errors

            # Clean up generated CMake files in project root
            for file_path in generated_files:
                try:
                    if os_path_exists(file_path):
                        if os_path_isdir(file_path):
                            shutil_rmtree(file_path)
                            self.logger.info(
                                "Removed generated directory: {}".format(file_path)
                            )
                        else:
                            os_remove(file_path)
                            self.logger.info(
                                "Removed generated file: {}".format(file_path)
                            )
                except OSError as e:
                    self.logger.warning("Failed to remove {}: {}".format(file_path, e))


class CMakeBuilderCLI:
    def __init__(self):
        self.logger = colorlog_getLogger("CMakeBuilderCLI")
        self.logger.setLevel(logging_INFO)

        handler = colorlog_StreamHandler()
        handler.setFormatter(
            colorlog_ColoredFormatter(
                "%(log_color)s%(levelname)-8s%(reset)s %(message)s",
                log_colors={
                    "DEBUG": "cyan",
                    "INFO": "green",
                    "WARNING": "yellow",
                    "ERROR": "red",
                    "CRITICAL": "red,bg_white",
                },
            )
        )
        self.logger.addHandler(handler)

        script_dir = os_path_dirname(os_path_abspath(__file__))

        self.parser = argparse_ArgumentParser(
            description="Cross-platform CMake build system wrapper."
        )
        self.parser.add_argument(
            "--path",
            default=".",
            metavar="DIR",
            help=(
                "Path to directory containing CMakeLists.txt (source directory). "
                "Default: current directory (directory of this script). "
                "Supports ~ for home (e.g. --path ~/develop/myproject)."
            ),
        )
        self.parser.add_argument(
            "--ui",
            action="store_true",
            help=(
                "Launch the graphical UI. Incompatible with other flags; "
                "only --path may be used together with --ui to set the project directory."
            ),
        )
        self.parser.add_argument(
            "--show-standards",
            action="store_true",
            help="Show all supported C and C++ standards with descriptions.",
        )
        self.parser.add_argument(
            "--show-compilers",
            action="store_true",
            help="Show compiler detection and usage information.",
        )
        self.parser.add_argument(
            "--show-constants",
            action="store_true",
            help="Show all CMake constants that can be passed to CMakeLists.txt.",
        )
        self.parser.add_argument(
            "build_type",
            nargs="?",
            choices=["Debug", "Release", "RelWithDebInfo"],
            help="Build type (Debug, Release, or RelWithDebInfo). Not required when using --documentation.",
        )
        self.parser.add_argument(
            "-m",
            "--arch",
            choices=["x86", "x64"],
            help=(
                "Architecture to build for (x86 or x64).\n"
                "If not specified, the script will detect the architecture automatically."
            ),
        )
        self.parser.add_argument(
            "--stdc",
            type=int,
            default=17,
            help="C standard to use (89, 99, 11, 17, 23). Defaults to 17.",
        )
        self.parser.add_argument(
            "--stdcxx",
            type=int,
            default=20,
            help="C++ standard to use (98, 03, 11, 14, 17, 20, 23, 26). Defaults to 20.",
        )
        self.parser.add_argument(
            "-r",
            "--clean",
            action="store_true",
            help="Clean the build directory before building.",
        )
        self.parser.add_argument(
            "-i",
            "--setup-installer",
            action="store_true",
            help=(
                "Build in Release mode and generate installer package "
                "(requires CPack configuration in CMakeLists.txt)"
            ),
        )
        self.parser.add_argument(
            "-s",
            "--shared-libs",
            action="store_true",
            help="Build shared libraries. Adds -DBUILD_SHARED_LIBS=ON to CMake arguments.",
        )
        self.parser.add_argument(
            "--cmake-prefix-path",
            help=(
                "Add prefix paths to the CMake arguments. Multiple paths should be "
                "separated by the platform-specific path separator (e.g., ';' on Windows, ':' on Unix). "
                'Example: --cmake-prefix-path "/path/to/qt;/path/to/another_lib"'
            ),
        )
        self.parser.add_argument(
            "--cmake-args",
            help=(
                "Add additional CMake arguments. Multiple arguments should be "
                "separated by the platform-specific path separator (e.g., ';' on Windows, ':' on Unix). "
                'Example: --cmake-args="-DVAR1=VALUE1;-DVAR2=VALUE2;-DVAR3_CONSTANT"'
            ),
        )
        self.parser.add_argument(
            "--tests",
            nargs="?",  # Allows 0 or 1 argument
            help="Enable building tests. Optionally provide a custom CMake variable name",
        )
        self.parser.add_argument(
            "--examples",
            nargs="?",
            help="Enable building examples. Optionally provide a custom CMake variable name",
        )
        self.parser.add_argument(
            "--documentation",
            nargs="?",
            const="html",
            choices=["html", "pdf", "all"],
            help="Generate project documentation. Options: html (default), pdf, or all (both HTML and PDF)",
        )
        self.parser.add_argument(
            "--install-prefix",
            help="Install prefix for the build. Default is /usr/local or C:/Program Files/Lumex.",
        )
        self.parser.add_argument(
            "--compiler-c",
            help="Specify custom C compiler path (e.g., /opt/gcc-14/gcc). Uses system default if not specified.",
        )
        self.parser.add_argument(
            "--compiler-cpp",
            help="Specify custom C++ compiler path (e.g., /opt/gcc-14/g++). Uses system default if not specified.",
        )
        self.parser.add_argument(
            "--use-ninja",
            nargs="?",
            const="auto",
            choices=["auto", "msvc", "clang", "clang-cl", "gcc"],
            help=(
                "Use Ninja build system instead of default generator (Make/MSBuild). "
                "Optionally specify compiler type to auto-detect: msvc, clang, clang-cl, gcc. "
                "If not specified, defaults to 'auto' (MSVC on Windows, system default on Unix). "
                "Examples: --use-ninja, --use-ninja msvc, --use-ninja clang"
            ),
        )
        self.parser.add_argument(
            "--compile-commands",
            action="store_true",
            help=(
                "Generate compile_commands.json file and copy it to project root. "
                "Requires --use-ninja flag. Only performs CMake configuration, does not build the project."
            ),
        )
        self.parser.add_argument(
            "--debug-trycompile",
            action="store_true",
            help=(
                "Preserve CMake TryCompile scratch dirs and add --debug-output when generating "
                "compile_commands or when used with --ui (for the Generate compile_commands button). "
                "Use to debug 'rules.ninja not found' and similar try_compile issues."
            ),
        )

        # Windows-only options
        if platform_system() == "Windows":
            self.parser.add_argument(
                "--windows-version",
                choices=list(CMakeBuilder.WINDOWS_VERSIONS),
                help=(
                    "Windows version compatibility (WINDOWS_VERSION / WIN32_WINNT). "
                    "XP, VISTA, SEVEN, EIGHT, EIGHTDOTONE, TENELEVEN."
                ),
            )
            self.parser.add_argument(
                "--toolset",
                choices=list(CMakeBuilder.MSVC_TOOLSETS),
                help=(
                    "Select MSVC toolset version. "
                    "v120 (VS 2013), v140 (VS 2015), v141 (VS 2017), "
                    "v142 (VS 2019), v143 (VS 2022), v144 (VS 2025), v145 (VS 2026+)."
                ),
            )

        self.args = self.parser.parse_args()

        # Resolve project root from --path (default: directory of this script)
        if self.args.path == ".":
            self.project_root = script_dir
        else:
            self.project_root = os_path_abspath(os_path_expanduser(self.args.path))

        # Require CMakeLists.txt only when not launching UI (UI allows opening project from File → Open project)
        if not self.args.ui:
            cmake_lists_path = os_path_join(self.project_root, "CMakeLists.txt")
            if not os_path_exists(cmake_lists_path):
                self.logger.error(
                    "CMakeLists.txt not found in project root: {}".format(
                        self.project_root
                    )
                )
                self.logger.error(
                    "Use --path to specify directory containing CMakeLists.txt (e.g. --path ~/develop/myproject)"
                )
                sys_exit(1)

        # When --ui is set, only --path is allowed; no other operation flags
        if self.args.ui:
            conflicting = []
            if self.args.build_type is not None:
                conflicting.append("build_type")
            if self.args.documentation is not None:
                conflicting.append("--documentation")
            if self.args.compile_commands:
                conflicting.append("--compile-commands")
            if self.args.clean:
                conflicting.append("--clean")
            if self.args.setup_installer:
                conflicting.append("--setup-installer")
            if self.args.tests is not None:
                conflicting.append("--tests")
            if self.args.examples is not None:
                conflicting.append("--examples")
            if self.args.show_standards:
                conflicting.append("--show-standards")
            if self.args.show_compilers:
                conflicting.append("--show-compilers")
            if self.args.show_constants:
                conflicting.append("--show-constants")
            if conflicting:
                self.logger.error(
                    "--ui is incompatible with other options. Use only --path with --ui."
                )
                self.logger.error("Conflicting: %s", ", ".join(conflicting))
                sys_exit(1)

        # Launch UI when --ui is set (incompatible with other operations)
        if self.args.ui:
            if not _ui_available:
                raise RuntimeError(
                    "Failed to launch UI: PyQt5 is required. Install with: pip install PyQt5"
                )
            # When no path was given, open last used project dir if valid; else script dir
            if self.project_root == script_dir:
                self.project_root = _get_last_or_default_project_root(script_dir)
            run_compile_ui(
                self.project_root,
                debug_trycompile=getattr(self.args, "debug_trycompile", False),
            )
            sys_exit(0)

        # Handle compile_commands.json generation early (requires --use-ninja)
        if self.args.compile_commands:
            if self.args.use_ninja is None:
                self.logger.error(
                    "ERROR: --compile-commands requires --use-ninja flag to be specified"
                )
                self.logger.error(
                    "Usage: python compile.py --compile-commands --use-ninja [build_type]"
                )
                sys_exit(1)

        # Handle documentation generation early (no build type required)
        if self.args.documentation is not None:
            self.builder = CMakeBuilder(self.project_root)
            if not self.builder.generate_documentation(self.args.documentation):
                self.logger.error("ERROR: Documentation generation failed")
                sys_exit(1)
            # Exit after documentation generation (no need to build)
            sys_exit(0)

        # Validate build_type is provided for non-documentation operations
        if self.args.build_type is None and not (
            self.args.show_standards
            or self.args.show_compilers
            or self.args.show_constants
            or self.args.compile_commands
        ):
            self.logger.error("ERROR: build_type is required for build operations")
            self.logger.error("Use --help for usage information")
            sys_exit(1)

        # For compile_commands generation, use Release as default build_type if not specified
        if self.args.compile_commands and self.args.build_type is None:
            self.args.build_type = "Release"

        if self.args.setup_installer:
            self.args.build_type = "Release"

        # Detect architecture automatically
        try:
            self.auto_detected_arch = ArchitectureDetector.detect()
        except RuntimeError as e:
            self.logger.error(str(e))
            sys_exit(1)

        self.builder = CMakeBuilder(self.project_root)

    def run(self) -> None:
        if self.args.show_standards:
            self.builder.show_supported_standards()
            sys_exit(0)

        if self.args.show_compilers:
            self.builder.show_compiler_info()
            sys_exit(0)

        if self.args.show_constants:
            if self.builder.dump_cmake_variables():
                sys_exit(0)
            else:
                self.logger.error("ERROR: Failed to dump CMake variables.")
                sys_exit(1)

        if self.args.clean and not self.builder.clean_build_dir():
            self.logger.error("ERROR: Failed to clean build directory")
            sys_exit(1)

        # Apply architecture FIRST (before enabling Ninja, so compiler selection uses correct architecture)
        if self.args.arch:
            self.builder.add_architecture(self.args.arch)
        else:
            self.builder.add_architecture(self.auto_detected_arch)

        # Apply custom compilers if specified (before enabling Ninja, so Ninja knows not to auto-detect)
        if self.args.compiler_c:
            self.builder.set_custom_c_compiler(self.args.compiler_c)

        if self.args.compiler_cpp:
            self.builder.set_custom_cpp_compiler(self.args.compiler_cpp)

        # Enable Ninja build system if requested (after architecture and custom compilers)
        if self.args.use_ninja is not None:
            # Determine compiler type: None means auto-detect, "auto" means default behavior
            compiler_type = (
                None if self.args.use_ninja == "auto" else self.args.use_ninja
            )
            if not self.builder.enable_ninja(compiler_type):
                self.logger.error("ERROR: Cannot enable Ninja build system")
                sys_exit(1)

        # Apply C standard
        self.builder.add_c_standard(self.args.stdc, override=True)
        self.logger.info("⚙️ Using C Standard: C{}".format(self.args.stdc))

        # Apply C++ standard
        self.builder.add_cpp_standard(self.args.stdcxx, override=True)
        self.logger.info("⚙️ Using C++ Standard: C++{}".format(self.args.stdcxx))

        # Validate standard compatibility
        if not self.builder.validate_standard_compatibility():
            self.logger.warning("Standard compatibility warning issued")

        # Apply Windows version compatibility: explicit or auto-detect when (default)
        if platform_system() == "Windows" and hasattr(self.args, "windows_version"):
            windows_ver = self.args.windows_version
            if windows_ver:
                self.builder.add_windows_version(windows_ver)
            else:
                detected = detect_windows_version()
                if detected:
                    self.builder.add_windows_version(detected)

        # Apply MSVC toolset if requested
        if (
            platform_system() == "Windows"
            and hasattr(self.args, "toolset")
            and self.args.toolset
        ):
            self.builder.add_toolset(self.args.toolset)

        if self.args.shared_libs:
            self.logger.info(
                "🔍 Activating shared libraries by adding -DBUILD_SHARED_LIBS=ON."
            )
            self.builder.activate_shared_libs()

        if self.args.tests is not None:
            self.builder.add_cmake_args(["-D{}={}".format(self.args.tests, "ON")])

        if self.args.examples is not None:
            self.builder.add_cmake_args(["-D{}={}".format(self.args.examples, "ON")])

        if self.args.cmake_prefix_path:
            # Split the string of paths by the platform-specific separator
            prefix_paths = self.args.cmake_prefix_path.split(os_pathsep)
            self.builder.add_cmake_prefix_path(prefix_paths)

        if self.args.cmake_args:
            self.builder.add_cmake_args(_split_extra_cmake_args(self.args.cmake_args))

        # Set up MSVC environment if using MSVC with Ninja (or NMake override)
        if self.builder.use_ninja and self.builder.custom_cpp_compiler:
            self.builder._setup_msvc_environment()

        compile_commands_temp_build_dir = None
        if self.args.compile_commands:
            self.builder.add_cmake_args(["-DCMAKE_EXPORT_COMPILE_COMMANDS=ON"])
            self.logger.info("📋 Enabled compile_commands.json generation")
            # On Windows, use NMake and skip try_compile to avoid Ninja rules.ninja / NMake U1052 issues.
            if platform_system() == "Windows":
                self.builder.generator_override = "NMake Makefiles"
                self.builder.add_cmake_args(
                    [
                        "-DCMAKE_CXX_COMPILER_WORKS=ON",
                        "-DCMAKE_C_COMPILER_WORKS=ON",
                    ]
                )
                self.logger.info(
                    "Using NMake Makefiles for compile_commands (skip try_compile to avoid NMake U1052)"
                )
            # Remove previous compile_commands build dirs (from earlier failed runs) to avoid clutter.
            import glob as glob_module

            for old_path in glob_module.glob(
                os_path_join(self.project_root, "build_compile_commands_*")
            ):
                if os_path_isdir(old_path):
                    try:
                        shutil_rmtree(old_path)
                        self.logger.info(
                            "Removed previous compile_commands build dir: {}".format(
                                old_path
                            )
                        )
                    except OSError:
                        pass
            # Isolate compile_commands generation from a normal build directory.
            compile_commands_temp_build_dir = tempfile_mkdtemp(
                prefix="build_compile_commands_", dir=self.project_root
            )
            self.builder.build_dir = compile_commands_temp_build_dir
            if getattr(self.args, "debug_trycompile", False):
                self.builder.debug_trycompile = True
            self.logger.info(
                "Using temporary build directory for compile_commands: {}".format(
                    compile_commands_temp_build_dir
                )
            )

        if not self.builder.configure(self.args.build_type):
            if compile_commands_temp_build_dir and os_path_exists(
                compile_commands_temp_build_dir
            ):
                if self.args.compile_commands and getattr(
                    self.args, "debug_trycompile", False
                ):
                    self.logger.info(
                        "Build dir preserved for TryCompile inspection: {}".format(
                            compile_commands_temp_build_dir
                        )
                    )
                else:
                    try:
                        shutil_rmtree(compile_commands_temp_build_dir)
                    except OSError:
                        pass
            self.logger.error("ERROR: CMake configuration failed")
            sys_exit(1)

        # Handle compile_commands.json generation
        if self.args.compile_commands:
            compile_commands_src = os_path_join(
                self.builder.build_dir, "compile_commands.json"
            )
            compile_commands_dst = os_path_join(
                self.project_root, "compile_commands.json"
            )

            if not os_path_exists(compile_commands_src):
                if compile_commands_temp_build_dir and os_path_exists(
                    compile_commands_temp_build_dir
                ):
                    try:
                        shutil_rmtree(compile_commands_temp_build_dir)
                    except OSError:
                        pass
                self.logger.error(
                    "ERROR: compile_commands.json not found in build directory: {}".format(
                        compile_commands_src
                    )
                )
                self.logger.error("Possible causes:")
                self.logger.error(
                    "  1. No compilable (.cpp) files in project (only headers)"
                )
                self.logger.error(
                    "  2. CMake configuration failed before Ninja could generate compile_commands.json"
                )
                self.logger.error(
                    "  3. Generator does not support compile_commands.json (use --use-ninja)"
                )
                self.logger.error(
                    "Solution: Ensure at least one .cpp file exists in src/ for CMake to process"
                )
                sys_exit(1)

            try:
                shutil_copy2(compile_commands_src, compile_commands_dst)
                self.logger.info(
                    "✅ Successfully copied compile_commands.json to project root: {}".format(
                        compile_commands_dst
                    )
                )

                # Fix MSVC external include flags for clangd/clang-cl:
                # CMake (esp. Ninja+MSVC) may emit `-external:*` spellings into compile_commands.json.
                # MSVC understands `/external:*`, but clangd treats `-external:*` as an unknown argument.
                _normalize_compile_commands_for_clangd(
                    compile_commands_dst, self.logger
                )

                # If there are local (gitignored) headers, clangd often falls back to `.clangd` flags for them
                # (because they are not part of the compilation database and may not be reachable from any TU).
                # We add explicit entries for such headers so clangd uses the same flags as the project.
                _add_compile_commands_entries_for_local_headers(
                    compile_commands_dst, self.project_root, self.logger
                )
            except Exception as e:
                if compile_commands_temp_build_dir and os_path_exists(
                    compile_commands_temp_build_dir
                ):
                    try:
                        shutil_rmtree(compile_commands_temp_build_dir)
                    except OSError:
                        pass
                self.logger.error(
                    "ERROR: Failed to copy compile_commands.json: {}".format(e)
                )
                sys_exit(1)

            if compile_commands_temp_build_dir and os_path_exists(
                compile_commands_temp_build_dir
            ):
                try:
                    shutil_rmtree(compile_commands_temp_build_dir)
                    self.logger.info(
                        "Removed temporary compile_commands build directory: {}".format(
                            compile_commands_temp_build_dir
                        )
                    )
                except OSError as e:
                    self.logger.warning(
                        "Could not remove temporary compile_commands build directory {}: {}".format(
                            compile_commands_temp_build_dir, e
                        )
                    )

            # Exit after generating compile_commands.json (no need to build)
            sys_exit(0)

        if not self.builder.build(self.args.build_type):
            self.logger.error("ERROR: Build failed")
            sys_exit(1)

        if self.args.setup_installer:
            packaging_found = self.builder._check_packaging_configuration()
            if not packaging_found:
                self.logger.warning(
                    "Skipping installer generation: No packaging configuration found"
                )
                self.logger.info(
                    "To enable installer generation, add CPack configuration to CMakeLists.txt"
                )
                self.logger.info(
                    'Example: include(CPack) and set(CPACK_PACKAGE_NAME "YourPackage")'
                )
            else:
                self.logger.info(
                    "Packaging configuration detected, proceeding with installer generation"
                )
                if not self.builder.cpack(self.args.build_type):
                    self.logger.error("ERROR: Installer generation failed")
                    sys_exit(1)

        if self.args.install_prefix:
            if not self.builder.install(self.args.install_prefix):
                self.logger.error("ERROR: Installation failed")
                sys_exit(1)


def _normalize_compile_commands_for_clangd(compile_commands_path: str, logger) -> bool:
    """
    Normalize compile_commands.json to be parseable by clangd on Windows.

    Specifically, rewrite MSVC external include spellings:
    - `-external:I...` -> `/external:I...`
    - `-external:W0`   -> `/external:W0`
    (and same for `/external:*` already correct; GCC/Clang builds are untouched).
    """
    try:
        if platform_system() != "Windows":
            return True

        if not os_path_exists(compile_commands_path):
            return False

        with open(compile_commands_path, "r", encoding="utf-8") as f:
            database = json_load(f)

        if not isinstance(database, list):
            return False

        updated_entries = 0

        for entry in database:
            if not isinstance(entry, dict):
                continue

            changed = False

            # Newer generators sometimes emit structured arguments.
            if "arguments" in entry and isinstance(entry["arguments"], list):
                new_args = []
                for arg in entry["arguments"]:
                    if isinstance(arg, str) and arg.startswith("-external:"):
                        new_args.append("/" + arg[1:])
                        changed = True
                    else:
                        new_args.append(arg)

                if changed:
                    entry["arguments"] = new_args

            # Common case: single `command` string (what you have now).
            elif "command" in entry and isinstance(entry["command"], str):
                original = entry["command"]
                # Replace only token-start occurrences (beginning or whitespace).
                fixed = re_sub(r"(^|\s)-external:", r"\1/external:", original)
                if fixed != original:
                    entry["command"] = fixed
                    changed = True

            if changed:
                updated_entries += 1

        if updated_entries > 0:
            with open(compile_commands_path, "w", encoding="utf-8", newline="\n") as f:
                json_dump(database, f, indent=2, ensure_ascii=False)
                f.write("\n")

            logger.info(
                "🛠️ Normalized compile_commands.json for clangd "
                "(MSVC -external:* -> /external:*), updated_entries={}".format(
                    updated_entries
                )
            )

        return True
    except Exception as e:
        logger.error(
            "ERROR: Failed to normalize compile_commands.json for clangd: {}".format(e)
        )
        return False


def _add_compile_commands_entries_for_local_headers(
    compile_commands_path: str, project_root: str, logger
) -> bool:
    """
    Add compile_commands.json entries for local (gitignored) headers.

    Problem this solves:
    - clangd applies flags from compile_commands.json only for files that have an entry,
      or for headers it can "associate" with some translation unit.
    - For local headers (e.g. `src/not_to_distr/**`) that are NOT part of the build and may not be included
      by any compiled .cpp, clangd falls back to `.clangd` defaults, which often misses external includes
      like Boost.

    Approach:
    - If `src/not_to_distr` exists, enumerate headers there and append "synthetic" entries
      based on the first real compile command in the database (same flags, only `-c <file>` replaced).
    """
    try:
        not_to_distr_dir = os_path_join(project_root, "src", "not_to_distr")
        if not os_path_exists(not_to_distr_dir) or not os_path_isdir(not_to_distr_dir):
            return True

        if not os_path_exists(compile_commands_path):
            return False

        with open(compile_commands_path, "r", encoding="utf-8") as f:
            database = json_load(f)

        if not isinstance(database, list) or not database:
            return False

        # Pick a template command (first entry with a string `command`).
        template_entry = None
        for entry in database:
            if isinstance(entry, dict) and isinstance(entry.get("command"), str):
                template_entry = entry
                break

        if template_entry is None:
            return True

        template_command = template_entry["command"]
        template_directory = template_entry.get("directory", "")

        existing_files = set()
        for entry in database:
            if isinstance(entry, dict) and isinstance(entry.get("file"), str):
                existing_files.add(entry["file"].replace("\\", "/"))

        header_extensions = {".h", ".hpp", ".hh", ".hxx", ".ipp", ".inl"}
        added_entries = 0

        boost_root = os_environ.get("BOOST_ROOT", "")
        if not boost_root:
            boost_root = _try_get_boost_include_root_from_cmake_cache(project_root)

        boost_root_norm = boost_root.replace("\\", "/") if boost_root else ""

        for root, _, files in os_walk(not_to_distr_dir):
            for name in files:
                lower = name.lower()
                dot = lower.rfind(".")
                ext = lower[dot:] if dot >= 0 else ""
                if ext not in header_extensions:
                    continue

                header_path = os_path_join(root, name)
                header_file_json = header_path.replace("\\", "/")
                if header_file_json in existing_files:
                    continue

                new_entry = dict(template_entry)
                new_entry["directory"] = template_directory
                new_entry["file"] = header_file_json

                # Replace the last `-c <file>` in the command (common for both MSVC and GCC/Clang).
                replacement = header_path
                if " " in replacement:
                    replacement = '"' + replacement + '"'
                new_command = re_sub(
                    r"(\s-c\s+)(\"[^\"]+\"|\S+)\s*$",
                    lambda match: match.group(1) + replacement,
                    template_command,
                )

                # Ensure Boost headers are discoverable for local headers.
                # This is needed when the compilation database has no TU that carries Boost include dirs
                # (e.g. the main project is currently header-only and only dependencies are compiled).
                if (
                    boost_root
                    and (boost_root not in new_command)
                    and (boost_root_norm not in new_command)
                ):
                    include_flag = "-I{}".format(boost_root)
                    new_command = re_sub(
                        r"(\s)-c(\s+)",
                        lambda match: match.group(1)
                        + include_flag
                        + " -c"
                        + match.group(2),
                        new_command,
                        1,
                    )

                # If we couldn't match `-c <file>` (unexpected), keep the original command;
                # clangd still uses `file` for mapping, and most toolchains accept the command line anyway.
                new_entry["command"] = new_command

                database.append(new_entry)
                existing_files.add(header_file_json)
                added_entries += 1

        if added_entries > 0:
            with open(compile_commands_path, "w", encoding="utf-8", newline="\n") as f:
                json_dump(database, f, indent=2, ensure_ascii=False)
                f.write("\n")

            logger.info(
                "🧩 Added compile_commands.json entries for local headers in src/not_to_distr: {}".format(
                    added_entries
                )
            )

        return True
    except Exception as e:
        logger.error(
            "ERROR: Failed to extend compile_commands.json for local headers: {}".format(
                e
            )
        )
        return False


def _try_get_boost_include_root_from_cmake_cache(project_root: str) -> str:
    """
    Best-effort extraction of Boost include root from CMakeCache.txt.

    This is a fallback for environments where BOOST_ROOT is not present for the python process,
    but CMake still discovered Boost (e.g. via cache/presets/toolchain).
    """
    try:
        cache_path = os_path_join(project_root, "build", "CMakeCache.txt")
        if not os_path_exists(cache_path):
            return ""

        with open(cache_path, "r", encoding="utf-8", errors="ignore") as f:
            content = f.read()

        # Prefer explicit BOOST_ROOT (matches the folder containing `boost/`).
        match = re_search(r"^BOOST_ROOT:PATH=(.+)$", content, re_DOTALL | re_MULTILINE)
        if match:
            return match.group(1).strip()

        # Common FindBoost variables.
        match = re_search(
            r"^Boost_INCLUDE_DIR:PATH=(.+)$", content, re_DOTALL | re_MULTILINE
        )
        if match:
            return match.group(1).strip()

        match = re_search(
            r"^Boost_INCLUDE_DIRS:PATH=(.+)$", content, re_DOTALL | re_MULTILINE
        )
        if match:
            return match.group(1).strip()

        match = re_search(
            r"^Boost_INCLUDE_DIRS:STRING=(.+)$", content, re_DOTALL | re_MULTILINE
        )
        if match:
            # May contain a list separated by ';' - pick the first entry.
            return match.group(1).split(";")[0].strip()

        return ""
    except Exception:
        return ""


# ============== GUI (--ui): all UI code in this file ==============
NINJA_RELEASES_URL = "https://github.com/ninja-build/ninja/releases"

# Official download pages for CMake and C/C++ compilers (by platform).
# Used when CMake or compiler is not found to show the user where to get them.
BUILD_TOOLS_DOWNLOAD_LINKS = {
    "Windows": {
        "cmake": "https://cmake.org/download/",
        "compilers": [
            (
                "Visual Studio Build Tools (MSVC)",
                "https://visualstudio.microsoft.com/visual-cpp-build-tools/",
            ),
            ("MSYS2 (MinGW-w64, GCC/Clang)", "https://www.msys2.org/"),
            (
                "LLVM/Clang for Windows",
                "https://github.com/llvm/llvm-project/releases",
            ),
            ("WinLibs (MinGW-w64 standalone)", "https://winlibs.com/"),
        ],
    },
    "Linux": {
        "cmake": "https://cmake.org/download/",
        "compilers": [
            (
                "CMake and build-essential (apt: sudo apt install cmake build-essential)",
                "https://cmake.org/download/",
            ),
            (
                "Fedora/RHEL: sudo dnf install cmake gcc-c++",
                "https://cmake.org/download/",
            ),
        ],
    },
    "Darwin": {
        "cmake": "https://cmake.org/download/",
        "compilers": [
            (
                "Xcode Command Line Tools (run: xcode-select --install)",
                "https://developer.apple.com/xcode/",
            ),
            ("Homebrew: brew install cmake gcc", "https://cmake.org/download/"),
        ],
    },
}


def _check_build_tools_ui() -> Tuple[bool, bool]:
    """
    Check for CMake and at least one C/C++ compiler.
    Returns (has_cmake, has_compiler). Show missing-tools dialog only when either is missing.
    """
    has_cmake = bool(shutil_which("cmake"))
    if not has_cmake and platform_system() == "Windows":
        try:
            r = subprocess_run(
                ["cmake", "--version"],
                capture_output=True,
                timeout=5,
                **_SUBPROCESS_NO_WINDOW,
            )
            if r.returncode == 0:
                has_cmake = True
        except Exception:
            pass
    is_win = platform_system() == "Windows"
    if is_win:
        compiler_names = (
            "gcc.exe",
            "g++.exe",
            "cl.exe",
            "clang.exe",
            "clang-cl.exe",
            "clang++.exe",
        )
    else:
        compiler_names = ("gcc", "g++", "clang", "clang++")
    has_compiler = any(shutil_which(n) for n in compiler_names)
    return has_cmake, has_compiler


def _build_about_message_html(
    tr: dict,
    lang: str,
    app_version: str,
    app_description_en: str,
    app_description_ru: str,
) -> str:
    """Build About/Help message as HTML with clickable download links."""
    version_label = tr.get("about_version_label", "Version:")
    description = app_description_en if lang == "en" else app_description_ru
    requirements = tr.get(
        "help_requirements",
        "Requirements: CMake and at least one C/C++ compiler (e.g. GCC, Clang, or MSVC) must be installed and available in PATH to configure and build projects.",
    )
    plat = platform_system()
    links = BUILD_TOOLS_DOWNLOAD_LINKS.get(plat, BUILD_TOOLS_DOWNLOAD_LINKS["Linux"])
    cmake_link = '<a href="{0}">{1}</a>'.format(
        links["cmake"], tr.get("build_tools_download_cmake", "Download CMake")
    )
    compiler_parts = [
        '<a href="{0}">{1}</a>'.format(url, html_escape(name))
        for name, url in links["compilers"]
    ]
    compiler_links_html = "<br>".join(compiler_parts)
    return (
        "<p><b>{} {}</b></p>"
        "<p>{}</p>"
        "<p>{}</p>"
        "<p><b>Downloads:</b><br>CMake: {}<br>Compilers:<br>{}</p>"
    ).format(
        html_escape(version_label),
        html_escape(app_version),
        html_escape(description),
        html_escape(requirements),
        cmake_link,
        compiler_links_html,
    )


def _show_missing_build_tools_dialog(
    parent: "QWidget",
    missing_cmake: bool,
    missing_compiler: bool,
    lang: str = "en",
) -> None:
    """Show a dialog with a short message and clickable download links when CMake or compiler is missing."""
    plat = platform_system()
    links = BUILD_TOOLS_DOWNLOAD_LINKS.get(plat, BUILD_TOOLS_DOWNLOAD_LINKS["Linux"])
    tr_en = _UI_STRINGS.get("en", {})
    tr_ru = _UI_STRINGS.get("ru", {})
    tr = tr_ru if lang == "ru" else tr_en
    title = tr.get(
        "build_tools_missing_title",
        "Missing build tools",
    )
    parts = []
    if missing_cmake:
        parts.append(
            tr.get(
                "build_tools_missing_cmake",
                "CMake is not installed or not in PATH. It is required to configure and build the project.",
            )
        )
        parts.append(
            '<br><a href="{0}">{1}</a>'.format(
                links["cmake"],
                tr.get("build_tools_download_cmake", "Download CMake"),
            )
        )
    if missing_compiler:
        if parts:
            parts.append("<br><br>")
        parts.append(
            tr.get(
                "build_tools_missing_compiler",
                "No C/C++ compiler was found in PATH (e.g. GCC, Clang, or MSVC). A compiler is required to build.",
            )
        )
        for name, url in links["compilers"]:
            parts.append('<br>• <a href="{0}">{1}</a>'.format(url, name))
    body = "".join(parts)
    if not body:
        return
    dlg = QDialog(parent)  # type: ignore
    dlg.setWindowTitle(title)
    layout = QVBoxLayout(dlg)
    label = QLabel(dlg)
    label.setOpenExternalLinks(True)
    label.setWordWrap(True)
    label.setTextFormat(getattr(Qt, "RichText", 2))  # type: ignore
    label.setText(body)
    label.setMinimumWidth(480)
    layout.addWidget(label)
    btn = QPushButton(tr.get("build_tools_ok", "OK"), dlg)
    btn.clicked.connect(dlg.accept)
    layout.addWidget(btn)
    dlg.exec_()


def _maybe_show_missing_build_tools(win: "QMainWindow") -> None:
    """If CMake or compiler is missing, show a one-time dialog with download links."""
    if not _ui_available:
        return
    has_cmake, has_compiler = _check_build_tools_ui()
    if has_cmake and has_compiler:
        return
    _ = getattr(win, "_lang", "en")  # RIP: here was variable `lang`
    # _show_missing_build_tools_dialog(win, not has_cmake, not has_compiler, lang=lang)


def _get_last_or_default_project_root(script_dir: str) -> str:
    """Return last opened project directory from QSettings if valid; else script_dir.
    Used when user starts UI without --path so the last opened folder opens by default.
    """
    try:
        _s = QSettings("CMakeBuildGUI", "compile")  # type: ignore
        last = _s.value("last_project_directory")
        if not last or not isinstance(last, str) or not last.strip():
            return script_dir
        path = os_path_abspath(os_path_expanduser(last.strip()))
        if not os_path_isdir(path):
            return script_dir
        if not os_path_exists(os_path_join(path, "CMakeLists.txt")):
            return script_dir
        return path
    except Exception:
        return script_dir


def run_compile_ui(project_root: str, debug_trycompile: bool = False) -> None:
    """Entry point for --ui: create application and main window, run event loop."""
    if not _ui_available:
        raise RuntimeError(
            "PyQt5 is required for the UI. Install with: pip install PyQt5"
        )
    app = QApplication(sys_argv)
    win = _CompileMainWindow(project_root, debug_trycompile=debug_trycompile)
    win.show()
    # One-time check: if CMake or compiler is missing, show a dialog with download links
    QTimer.singleShot(300, lambda: _maybe_show_missing_build_tools(win))
    sys_exit(app.exec_())


if _ui_available:

    class _LogBridge(QObject):
        log_text = pyqtSignal(str)

    class _LogHandler(logging_module.Handler):
        def __init__(self, bridge: _LogBridge) -> None:
            super().__init__()
            self._bridge = bridge

        def emit(self, record: logging_module.LogRecord) -> None:
            try:
                msg = self.format(record)
                self._bridge.log_text.emit(msg)
            except Exception:
                self.handleError(record)

    class _BuildWorker(QObject):
        finished = pyqtSignal(bool)

        def __init__(
            self,
            project_root: str,
            task: str,
            build_type: str,
            options: dict,
            option_values: List[Tuple[str, str]],
            log_bridge: _LogBridge,
        ) -> None:
            super().__init__()
            self._project_root = project_root
            self._task = task
            self._build_type = build_type
            self._options = options
            self._option_values = option_values
            self._log_bridge = log_bridge
            self._cancel_requested = False

        def run(self) -> None:
            success = False
            self._cancel_requested = False
            try:
                builder = CMakeBuilder(self._project_root)
                # Redirect all output to dock: remove console and previous GUI
                # handlers so we don't duplicate (logger "CMakeBuilder" is shared).
                for h in list(builder.logger.handlers):
                    if isinstance(h, (logging_module.StreamHandler, _LogHandler)):
                        builder.logger.removeHandler(h)
                handler = _LogHandler(self._log_bridge)
                handler.setFormatter(
                    logging_module.Formatter("%(levelname)s: %(message)s")
                )
                builder.logger.addHandler(handler)
                builder.logger.setLevel(logging_INFO)
                self._apply_options(builder)
                # Set up MSVC environment when using Ninja with MSVC (required for cl.exe + Ninja)
                if (
                    self._task == "configure"
                    and builder.use_ninja
                    and builder.custom_cpp_compiler
                ):
                    builder._setup_msvc_environment()
                cancel_check: Callable[[], bool] = lambda: self._cancel_requested
                if self._task == "configure":
                    success = builder.configure(
                        self._build_type, cancel_check=cancel_check
                    )
                else:
                    success = builder.build(self._build_type, cancel_check=cancel_check)
            except Exception as e:
                self._log_bridge.log_text.emit("ERROR: {}".format(e))
            self.finished.emit(success)

        def _apply_options(self, builder: CMakeBuilder) -> None:
            opts = self._options
            builder.build_type = self._build_type
            arch = opts.get("arch")
            if arch:
                builder.add_architecture(arch)
            else:
                try:
                    builder.add_architecture(ArchitectureDetector.detect())
                except RuntimeError:
                    builder.add_architecture("x64")
            if opts.get("stdc") is not None:
                builder.add_c_standard(opts["stdc"], override=True)
            if opts.get("stdcxx") is not None:
                builder.add_cpp_standard(opts["stdcxx"], override=True)
            if opts.get("compiler_c"):
                builder.set_custom_c_compiler(opts["compiler_c"])
            if opts.get("compiler_cpp"):
                builder.set_custom_cpp_compiler(opts["compiler_cpp"])
            if opts.get("use_ninja"):
                if not builder.enable_ninja(None):
                    raise RuntimeError("Cannot enable Ninja build system")
            if opts.get("shared_libs"):
                builder.add_cmake_args(["-DBUILD_SHARED_LIBS=ON"])
            if opts.get("cmake_prefix_path"):
                builder.add_cmake_prefix_path(
                    opts["cmake_prefix_path"].split(os_pathsep)
                )
            if opts.get("cmake_args"):
                builder.add_cmake_args(_split_extra_cmake_args(opts["cmake_args"]))
            if opts.get("install_prefix"):
                builder.add_cmake_args(
                    ["-DCMAKE_INSTALL_PREFIX={}".format(opts["install_prefix"])]
                )
            windows_ver = opts.get("windows_version")
            if windows_ver:
                builder.add_windows_version(windows_ver)
            elif platform_system() == "Windows":
                detected = detect_windows_version()
                if detected:
                    builder.add_windows_version(detected)
            if opts.get("toolset"):
                builder.add_toolset(opts["toolset"])
            if opts.get("parallel_jobs") is not None:
                builder.parallel_jobs = opts["parallel_jobs"]
            for name, value in self._option_values:
                builder.add_cmake_args(["-D{}={}".format(name, value)])

    class _CompileCommandsWorker(QObject):
        """Runs compile_commands.json generation in-process (same process as GUI) so MSVC batch wrapper works."""

        finished = pyqtSignal(bool)

        def __init__(
            self,
            project_root: str,
            build_type: str,
            options: dict,
            log_bridge: "_LogBridge",
        ) -> None:
            super().__init__()
            self._project_root = project_root
            self._build_type = build_type
            self._options = options
            self._log_bridge = log_bridge

        def run(self) -> None:
            success = False
            try:
                builder = CMakeBuilder(self._project_root)
                for h in list(builder.logger.handlers):
                    if isinstance(h, (logging_module.StreamHandler, _LogHandler)):
                        builder.logger.removeHandler(h)
                handler = _LogHandler(self._log_bridge)
                handler.setFormatter(
                    logging_module.Formatter("%(levelname)s: %(message)s")
                )
                builder.logger.addHandler(handler)
                builder.logger.setLevel(logging_INFO)
                self._apply_options(builder)
                builder.add_cmake_args(["-DCMAKE_EXPORT_COMPILE_COMMANDS=ON"])
                if platform_system() == "Windows":
                    builder.generator_override = "NMake Makefiles"
                    builder.add_cmake_args(
                        [
                            "-DCMAKE_CXX_COMPILER_WORKS=ON",
                            "-DCMAKE_C_COMPILER_WORKS=ON",
                        ]
                    )
                    builder.logger.info(
                        "Using NMake Makefiles for compile_commands (skip try_compile to avoid NMake U1052)"
                    )
                # Avoid mutating process-wide environment in long-lived GUI process.
                # configure() already runs CMake via vcvarsall batch wrapper for MSVC+Ninja.
                compile_commands_temp_build_dir = tempfile_mkdtemp(
                    prefix="build_compile_commands_", dir=self._project_root
                )
                builder.build_dir = compile_commands_temp_build_dir
                builder.logger.info(
                    "Using temporary build directory for compile_commands: %s",
                    compile_commands_temp_build_dir,
                )
                if not builder.configure(self._build_type):
                    if os_path_exists(compile_commands_temp_build_dir):
                        try:
                            shutil_rmtree(compile_commands_temp_build_dir)
                        except OSError:
                            pass
                    self._log_bridge.log_text.emit("ERROR: CMake configuration failed")
                    self.finished.emit(False)
                    return
                compile_commands_src = os_path_join(
                    builder.build_dir, "compile_commands.json"
                )
                compile_commands_dst = os_path_join(
                    self._project_root, "compile_commands.json"
                )
                if not os_path_exists(compile_commands_src):
                    if os_path_exists(compile_commands_temp_build_dir):
                        try:
                            shutil_rmtree(compile_commands_temp_build_dir)
                        except OSError:
                            pass
                    self._log_bridge.log_text.emit(
                        "ERROR: compile_commands.json not found in build directory"
                    )
                    self.finished.emit(False)
                    return
                try:
                    shutil_copy2(compile_commands_src, compile_commands_dst)
                    _normalize_compile_commands_for_clangd(
                        compile_commands_dst, builder.logger
                    )
                    _add_compile_commands_entries_for_local_headers(
                        compile_commands_dst, self._project_root, builder.logger
                    )
                except Exception as e:
                    if os_path_exists(compile_commands_temp_build_dir):
                        try:
                            shutil_rmtree(compile_commands_temp_build_dir)
                        except OSError:
                            pass
                    self._log_bridge.log_text.emit(
                        "ERROR: Failed to copy compile_commands.json: {}".format(e)
                    )
                    self.finished.emit(False)
                    return
                if os_path_exists(compile_commands_temp_build_dir):
                    try:
                        shutil_rmtree(compile_commands_temp_build_dir)
                    except OSError:
                        pass
                success = True
            except Exception as e:
                self._log_bridge.log_text.emit("ERROR: {}".format(e))
            self.finished.emit(success)

        def _apply_options(self, builder: CMakeBuilder) -> None:
            opts = self._options
            builder.build_type = self._build_type
            arch = opts.get("arch")
            if arch:
                builder.add_architecture(arch)
            else:
                try:
                    builder.add_architecture(ArchitectureDetector.detect())
                except RuntimeError:
                    builder.add_architecture("x64")
            if opts.get("compiler_c"):
                builder.set_custom_c_compiler(opts["compiler_c"])
            if opts.get("compiler_cpp"):
                builder.set_custom_cpp_compiler(opts["compiler_cpp"])
            if opts.get("use_ninja"):
                if not builder.enable_ninja(None):
                    raise RuntimeError("Cannot enable Ninja build system")
            if opts.get("stdc") is not None:
                builder.add_c_standard(opts["stdc"], override=True)
            if opts.get("stdcxx") is not None:
                builder.add_cpp_standard(opts["stdcxx"], override=True)
            if opts.get("shared_libs"):
                builder.add_cmake_args(["-DBUILD_SHARED_LIBS=ON"])
            if opts.get("cmake_prefix_path"):
                builder.add_cmake_prefix_path(
                    opts["cmake_prefix_path"].split(os_pathsep)
                )
            if opts.get("cmake_args"):
                builder.add_cmake_args(_split_extra_cmake_args(opts["cmake_args"]))
            if opts.get("install_prefix"):
                builder.add_cmake_args(
                    ["-DCMAKE_INSTALL_PREFIX={}".format(opts["install_prefix"])]
                )
            windows_ver = opts.get("windows_version")
            if windows_ver:
                builder.add_windows_version(windows_ver)
            elif platform_system() == "Windows":
                detected = detect_windows_version()
                if detected:
                    builder.add_windows_version(detected)
            if opts.get("toolset"):
                builder.add_toolset(opts["toolset"])
            if opts.get("parallel_jobs") is not None:
                builder.parallel_jobs = opts["parallel_jobs"]

    class _FlagsLoaderWorker(QObject):
        """Runs get_cmake_options in a background thread so the main thread can paint the loading animation."""

        options_ready = pyqtSignal(list, str)

        def __init__(
            self, project_root: str, extra_cmake_args: Optional[List[str]] = None
        ) -> None:
            super().__init__()
            self._project_root = project_root
            self._extra_cmake_args = list(extra_cmake_args or [])

        def run(self) -> None:
            options: List[Tuple[str, str, str, str, List[str]]] = []
            error_message = ""
            try:
                options, maybe_error = get_cmake_options_detailed(
                    self._project_root, self._extra_cmake_args
                )
                error_message = maybe_error or ""
            except Exception:
                pass
            self.options_ready.emit(options, error_message)

    class _CompilerDiscoveryWorker(QObject):
        """Runs _discover_compilers in a background thread to fill C/C++ compiler dropdowns."""

        compilers_ready = pyqtSignal(list, list)

        def run(self) -> None:
            c_list: List[str] = []
            cpp_list: List[str] = []
            try:
                c_list, cpp_list = _discover_compilers()
            except Exception:
                pass
            self.compilers_ready.emit(c_list, cpp_list)

    class _FlagsTab(QWidget):
        refresh_requested = pyqtSignal()
        _OPTIONS_HIDDEN_FROM_TABLE = ("_BUILD_TYPE", "_C_STANDARD", "_CXX_STANDARD")

        def __init__(
            self,
            proot: str,
            options: Optional[List[Tuple[str, str, str, str, List[str]]]] = None,
            probe_error: str = "",
        ) -> None:
            super().__init__()
            self._project_root = proot
            self._options: List[Tuple[str, str, str, str, List[str]]] = []
            self._value_widgets: List[Any] = []
            self._last_probe_error = ""
            self._tr = _UI_STRINGS.get("en", {})
            layout = QVBoxLayout(self)
            self._table = QTableWidget(0, 3)
            self._table.setSelectionBehavior(QAbstractItemView.SelectRows)
            self._table.setHorizontalHeaderLabels(["Option", "Description", "Value"])
            self._sort_column = 0
            self._sort_order_asc = True
            self._default_width_0 = 220
            self._default_width_2 = 160
            self._saved_width_0 = self._default_width_0
            self._saved_width_2 = self._default_width_2
            hdr = self._table.horizontalHeader()
            if hdr is not None:
                hdr.setSectionsMovable(False)
                hdr.setSectionResizeMode(0, QHeaderView.Interactive)
                hdr.setSectionResizeMode(1, QHeaderView.Stretch)
                hdr.setSectionResizeMode(2, QHeaderView.Interactive)
                hdr.setMinimumSectionSize(60)
                hdr.resizeSection(0, self._default_width_0)
                hdr.resizeSection(2, self._default_width_2)
                self._load_table_state_from_settings()
                self._apply_table_state()

                def _on_header_clicked(section: int) -> None:
                    if section == 2:
                        return
                    if self._sort_column == section:
                        self._sort_order_asc = not self._sort_order_asc
                    else:
                        self._sort_column = section
                        self._sort_order_asc = True
                    order = (
                        Qt.AscendingOrder  # type: ignore[attr-defined]
                        if self._sort_order_asc
                        else Qt.DescendingOrder  # type: ignore[attr-defined]
                    )
                    self._table.setSortingEnabled(True)
                    self._table.sortByColumn(section, order)
                    self._table.setSortingEnabled(False)
                    hdr.setSortIndicator(section, order)
                    self._save_table_state_to_settings()

                hdr.sectionClicked.connect(_on_header_clicked)
                hdr.sectionResized.connect(self._on_section_resized)
            refresh_row = QHBoxLayout()
            refresh_row.addStretch()
            self._refresh_btn = QPushButton("Refresh")
            self._refresh_btn.setToolTip(
                "Reload CMake options from CMakeLists.txt (e.g. after adding new options)."
            )
            self._refresh_btn.clicked.connect(self.refresh_requested.emit)
            refresh_row.addWidget(self._refresh_btn)
            layout.addLayout(refresh_row)
            layout.addWidget(self._table)
            if options is not None:
                self._set_options(options, probe_error)
            else:
                self._load_options()

        def _set_options(
            self,
            options: List[Tuple[str, str, str, str, List[str]]],
            probe_error: str = "",
        ) -> None:
            filtered = [
                opt
                for opt in options
                if not any(
                    opt[0].endswith(suffix)
                    for suffix in self._OPTIONS_HIDDEN_FROM_TABLE
                )
            ]
            self._options = filtered
            self._last_probe_error = probe_error.strip()
            self._value_widgets.clear()
            self._table.setRowCount(len(self._options))
            for row, (name, desc, var_type, value_text, choices) in enumerate(
                self._options
            ):
                opt_item = QTableWidgetItem(name)
                opt_item.setToolTip(name)
                self._table.setItem(row, 0, opt_item)
                desc_item = QTableWidgetItem(desc)
                desc_item.setToolTip(desc)
                self._table.setItem(row, 1, desc_item)
                if var_type == "BOOL":
                    cb = QCheckBox()
                    cb.setChecked(value_text.upper() == "ON")
                    cb.setToolTip(value_text or "ON / OFF")
                    self._value_widgets.append(cb)
                    self._table.setCellWidget(row, 2, cb)
                else:
                    combo = _NoWheelComboBox()
                    combo.setEditable(True)
                    combo.setProperty("disableWheelChange", True)
                    for choice in choices:
                        combo.addItem(choice)
                    if value_text and combo.findText(value_text) < 0:
                        combo.addItem(value_text)
                    combo.setCurrentText(value_text)
                    combo.setToolTip(value_text or "")
                    self._value_widgets.append(combo)
                    self._table.setCellWidget(row, 2, combo)
            if not self._options:
                self._table.setRowCount(1)
                no_opts_msg = self._build_no_options_message(self._tr)
                no_opts_item = QTableWidgetItem(no_opts_msg)
                no_opts_item.setToolTip(no_opts_msg)
                self._table.setItem(0, 0, no_opts_item)
            else:
                self._apply_table_state()

        def _flags_table_settings_prefix(self) -> str:
            return "flags_table/"

        def _load_table_state_from_settings(self) -> None:
            try:
                _s = QSettings("CMakeBuildGUI", "compile")  # type: ignore
                p = self._flags_table_settings_prefix()
                sc = _s.value(p + "sort_column")
                so = _s.value(p + "sort_order_asc")
                w0 = _s.value(p + "width_0")
                w2 = _s.value(p + "width_2")
                if sc is not None:
                    try:
                        col = int(sc)
                        if 0 <= col <= 1:
                            self._sort_column = col
                    except (TypeError, ValueError):
                        pass
                if so is not None and so in (True, "true", "1"):
                    self._sort_order_asc = True
                elif so is not None and so in (False, "false", "0"):
                    self._sort_order_asc = False
                if w0 is not None:
                    try:
                        self._saved_width_0 = int(w0)
                    except (TypeError, ValueError):
                        pass
                if w2 is not None:
                    try:
                        self._saved_width_2 = int(w2)
                    except (TypeError, ValueError):
                        pass
            except Exception:
                pass

        def _apply_table_state(self) -> None:
            item0 = self._table.item(0, 0) if self._table.rowCount() == 1 else None
            if self._table.rowCount() == 0 or (
                item0 is not None and item0.text().strip().startswith("(")
            ):
                return
            hdr = self._table.horizontalHeader()
            if hdr is None:
                return
            order = (
                Qt.AscendingOrder  # type: ignore[attr-defined]
                if self._sort_order_asc
                else Qt.DescendingOrder  # type: ignore[attr-defined]
            )
            self._table.setSortingEnabled(True)
            self._table.sortByColumn(self._sort_column, order)
            self._table.setSortingEnabled(False)
            hdr.setSortIndicator(self._sort_column, order)
            w0 = getattr(self, "_saved_width_0", self._default_width_0)
            w2 = getattr(self, "_saved_width_2", self._default_width_2)
            hdr.resizeSection(0, max(60, w0))
            hdr.resizeSection(2, max(60, w2))

        def _save_table_state_to_settings(self, settings: Optional[Any] = None) -> None:
            try:
                _s = settings if settings is not None else QSettings("CMakeBuildGUI", "compile")  # type: ignore
                p = self._flags_table_settings_prefix()
                _s.setValue(p + "sort_column", self._sort_column)
                _s.setValue(p + "sort_order_asc", self._sort_order_asc)
                hdr = self._table.horizontalHeader()
                if hdr is not None:
                    _s.setValue(p + "width_0", hdr.sectionSize(0))
                    _s.setValue(p + "width_2", hdr.sectionSize(2))
            except Exception:
                pass

        def _on_section_resized(self, logical_index: int, old_size: int, new_size: int) -> None:
            if getattr(self, "_section_resize_timer", None) is not None:
                try:
                    self._section_resize_timer.stop()
                except Exception:
                    pass
            self._section_resize_timer = QTimer()
            self._section_resize_timer.setSingleShot(True)
            self._section_resize_timer.timeout.connect(
                lambda: self._save_table_state_to_settings()
            )
            self._section_resize_timer.start(400)

        def reset_table_to_defaults(self) -> None:
            self._sort_column = 0
            self._sort_order_asc = True
            self._saved_width_0 = self._default_width_0
            self._saved_width_2 = self._default_width_2
            hdr = self._table.horizontalHeader()
            if hdr is not None:
                hdr.resizeSection(0, self._default_width_0)
                hdr.resizeSection(2, self._default_width_2)
            self._apply_table_state()
            self._save_table_state_to_settings()

        def save_state(self, settings: Any) -> None:
            self._save_table_state_to_settings(settings)

        def _load_options(self) -> None:
            options, probe_error = get_cmake_options_detailed(self._project_root)
            self._set_options(options, probe_error or "")

        def _build_no_options_message(self, strings: dict) -> str:
            base_text = strings.get(
                "no_cache_options", "(No cache options from cmake -LH)"
            )
            if not self._last_probe_error:
                return base_text
            prefix = strings.get("cmake_options_error_prefix", "Error details")
            details = self._last_probe_error.replace("\n", " ").strip()
            if len(details) > 800:
                details = details[:800] + "..."
            return "{}\n{}: {}".format(base_text, prefix, details)

        def apply_ui_strings(self, strings: dict) -> None:
            self._tr = strings
            self._table.setHorizontalHeaderLabels(
                [
                    strings.get("option", "Option"),
                    strings.get("description", "Description"),
                    strings.get("value", "Value"),
                ]
            )
            self._refresh_btn.setText(strings.get("refresh_cmake_options", "Refresh"))
            self._refresh_btn.setToolTip(
                strings.get(
                    "tooltip_refresh_cmake_options",
                    "Reload CMake options from CMakeLists.txt (e.g. after adding new options).",
                )
            )
            if self._table.rowCount() == 1:
                it = self._table.item(0, 0)
                if it is not None and "cmake" in it.text().lower():
                    it.setText(self._build_no_options_message(strings))

        def get_option_values(self) -> List[Tuple[str, str]]:
            result: List[Tuple[str, str]] = []
            for row in range(self._table.rowCount()):
                name_item = self._table.item(row, 0)
                if name_item is None:
                    continue
                name = name_item.text().strip()
                if not name or name.startswith("("):
                    continue
                widget = self._table.cellWidget(row, 2)
                if widget is None:
                    continue
                if isinstance(widget, QCheckBox):
                    result.append((name, "ON" if widget.isChecked() else "OFF"))
                elif isinstance(widget, QComboBox):
                    result.append((name, widget.currentText().strip()))
            return result

    class _ClangdTab(QWidget):
        def __init__(
            self,
            proot: str,
            output_slot: Any,
            debug_trycompile: bool = False,
        ) -> None:
            super().__init__()
            self._project_root = proot
            self._output_slot = output_slot
            self._debug_trycompile = debug_trycompile
            self._compile_commands_process = None
            layout = QVBoxLayout(self)
            self._hint_label = QLabel(
                "Generate compile_commands.json for Clangd/IntelliSense. "
                "Runs configure with Ninja and exports compile commands."
            )
            layout.addWidget(self._hint_label)
            self._btn = QPushButton("Generate compile_commands.json")
            self._btn.clicked.connect(self._on_generate)
            layout.addWidget(self._btn)
            layout.addStretch()

        def apply_ui_strings(self, strings: dict) -> None:
            self._hint_label.setText(
                strings.get("clangd_hint", self._hint_label.text())
            )
            self._btn.setText(
                strings.get(
                    "generate_compile_commands", "Generate compile_commands.json"
                )
            )

        def _on_generate(self) -> None:
            if not shutil_which("ninja"):
                QMessageBox.warning(
                    self,
                    "Ninja not found",
                    "Ninja build system is required but was not found in PATH.\n\n"
                    "Download from:\n{}".format(NINJA_RELEASES_URL),
                )
                return
            script_path = os_path_abspath(__file__).replace(".pyc", ".py")
            script_dir = os_path_dirname(script_path)
            cmd = [
                sys_executable,
                script_path,
                "--path",
                self._project_root,
                "--use-ninja",
                "--compile-commands",
                "Release",
            ]
            if self._debug_trycompile:
                cmd.append("--debug-trycompile")
            self._output_slot("Running: {}".format(" ".join(cmd)))
            self._btn.setEnabled(False)
            proc = QProcess()  # type: ignore[name-defined]
            self._compile_commands_process = proc
            proc.setWorkingDirectory(script_dir)
            merged = getattr(QProcess, "MergedChannels", 2)  # type: ignore[name-defined]
            proc.setProcessChannelMode(merged)  # type: ignore[arg-type]

            def _on_ready_read() -> None:
                p = self._compile_commands_process
                not_running = getattr(QProcess, "NotRunning", 0)  # type: ignore[name-defined]
                if p and p.state() != not_running:
                    data = p.readAllStandardOutput().data()
                    try:
                        text = data.decode("utf-8", errors="replace")
                    except Exception:
                        text = data.decode("cp1251", errors="replace")
                    for line in text.splitlines():
                        self._output_slot(line.rstrip())

            def _on_finished(exit_code: int, status: "QProcess.ExitStatus") -> None:
                _ = status
                self._btn.setEnabled(True)
                self._output_slot(
                    "Finished successfully."
                    if exit_code == 0
                    else "Exited with code {}.".format(exit_code)
                )
                self._compile_commands_process = None

            proc.readyReadStandardOutput.connect(_on_ready_read)
            proc.finished.connect(_on_finished)
            proc.start(cmd[0], cmd[1:])

        def set_project_root(self, proot: str) -> None:
            self._project_root = proot

    # UI themes (name -> stylesheet or None for default); 10 themes for View menu
    _GUI_THEMES = [
        ("Default", None),
        ("Dark", "dark"),
        ("Green", "green"),
        ("Blue", "blue"),
        ("Ocean", "ocean"),
        ("Amber", "amber"),
        ("Purple", "purple"),
        ("High Contrast", "high_contrast"),
        ("Forest", "forest"),
        ("Midnight", "midnight"),
    ]

    _GUI_THEME_STYLESHEETS = {
        "dark": """
            QWidget { background: #2d2d2d; color: #e0e0e0; }
            QDockWidget::title { background: #3d3d3d; color: #fff; padding: 6px 8px; font-weight: bold; }
            QGroupBox { font-weight: bold; }
            QLineEdit, QComboBox, QSpinBox { background: #1e1e1e; color: #e0e0e0; selection-background-color: #0d47a1; }
            QPushButton { background: #404040; color: #e0e0e0; }
            QPushButton:hover { background: #505050; }
            QTabWidget::pane { border: 1px solid #555; background: #2d2d2d; top: -1px; }
            QTabBar::tab { background: #3d3d3d; color: #e0e0e0; padding: 6px 12px; margin-right: 2px; border: 1px solid #555; border-bottom: none; }
            QTabBar::tab:selected { background: #2d2d2d; color: #fff; }
            QHeaderView::section { background: #3d3d3d; color: #e0e0e0; padding: 4px 8px; border: 1px solid #555; }
            QTableWidget { background: #2d2d2d; color: #e0e0e0; gridline-color: #555; }
            QTreeView { background: #2d2d2d; color: #e0e0e0; }
        """,
        "green": """
            QWidget { background: #f1f8e9; color: #1b5e20; }
            QDockWidget::title { background: #81c784; color: #1b5e20; padding: 6px 8px; font-weight: bold; }
            QPushButton { background: #a5d6a7; color: #1b5e20; }
            QPushButton:hover { background: #c8e6c9; }
            QHeaderView::section { background: #c8e6c9; color: #1b5e20; padding: 4px 8px; border: 1px solid #81c784; }
            QTabWidget::pane { border: 1px solid #81c784; background: #f1f8e9; top: -1px; }
            QTabBar::tab { background: #c8e6c9; color: #1b5e20; padding: 6px 12px; margin-right: 2px; border: 1px solid #81c784; border-bottom: none; }
            QTabBar::tab:selected { background: #f1f8e9; }
            QTableWidget, QTreeView { background: #f1f8e9; color: #1b5e20; gridline-color: #81c784; }
        """,
        "blue": """
            QWidget { background: #e3f2fd; color: #0d47a1; }
            QDockWidget::title { background: #64b5f6; color: #0d47a1; padding: 6px 8px; font-weight: bold; }
            QPushButton { background: #90caf9; color: #0d47a1; }
            QPushButton:hover { background: #bbdefb; }
            QHeaderView::section { background: #bbdefb; color: #0d47a1; padding: 4px 8px; border: 1px solid #64b5f6; }
            QTabWidget::pane { border: 1px solid #64b5f6; background: #e3f2fd; top: -1px; }
            QTabBar::tab { background: #bbdefb; color: #0d47a1; padding: 6px 12px; margin-right: 2px; border: 1px solid #64b5f6; border-bottom: none; }
            QTabBar::tab:selected { background: #e3f2fd; }
            QTableWidget, QTreeView { background: #e3f2fd; color: #0d47a1; gridline-color: #64b5f6; }
        """,
        "ocean": """
            QWidget { background: #e0f7fa; color: #006064; }
            QDockWidget::title { background: #4dd0e1; color: #006064; padding: 6px 8px; font-weight: bold; }
            QPushButton { background: #80deea; color: #006064; }
            QPushButton:hover { background: #b2ebf2; }
            QHeaderView::section { background: #b2ebf2; color: #006064; padding: 4px 8px; border: 1px solid #4dd0e1; }
            QTabWidget::pane { border: 1px solid #4dd0e1; background: #e0f7fa; top: -1px; }
            QTabBar::tab { background: #b2ebf2; color: #006064; padding: 6px 12px; margin-right: 2px; border: 1px solid #4dd0e1; border-bottom: none; }
            QTabBar::tab:selected { background: #e0f7fa; }
            QTableWidget, QTreeView { background: #e0f7fa; color: #006064; gridline-color: #4dd0e1; }
        """,
        "amber": """
            QWidget { background: #fff8e1; color: #e65100; }
            QDockWidget::title { background: #ffb74d; color: #e65100; padding: 6px 8px; font-weight: bold; }
            QPushButton { background: #ffcc80; color: #e65100; }
            QPushButton:hover { background: #ffe0b2; }
            QHeaderView::section { background: #ffe0b2; color: #e65100; padding: 4px 8px; border: 1px solid #ffb74d; }
            QTabWidget::pane { border: 1px solid #ffb74d; background: #fff8e1; top: -1px; }
            QTabBar::tab { background: #ffe0b2; color: #e65100; padding: 6px 12px; margin-right: 2px; border: 1px solid #ffb74d; border-bottom: none; }
            QTabBar::tab:selected { background: #fff8e1; }
            QTableWidget, QTreeView { background: #fff8e1; color: #e65100; gridline-color: #ffb74d; }
        """,
        "purple": """
            QWidget { background: #f3e5f5; color: #4a148c; }
            QDockWidget::title { background: #ce93d8; color: #4a148c; padding: 6px 8px; font-weight: bold; }
            QPushButton { background: #e1bee7; color: #4a148c; }
            QPushButton:hover { background: #f8bbd0; }
            QHeaderView::section { background: #e1bee7; color: #4a148c; padding: 4px 8px; border: 1px solid #ce93d8; }
            QTabWidget::pane { border: 1px solid #ce93d8; background: #f3e5f5; top: -1px; }
            QTabBar::tab { background: #e1bee7; color: #4a148c; padding: 6px 12px; margin-right: 2px; border: 1px solid #ce93d8; border-bottom: none; }
            QTabBar::tab:selected { background: #f3e5f5; }
            QTableWidget, QTreeView { background: #f3e5f5; color: #4a148c; gridline-color: #ce93d8; }
        """,
        "high_contrast": """
            QWidget { background: #fff; color: #000; }
            QDockWidget::title { background: #000; color: #fff; padding: 6px 8px; font-weight: bold; }
            QPushButton { background: #000; color: #fff; }
            QPushButton:hover { background: #333; }
            QHeaderView::section { background: #ddd; color: #000; padding: 4px 8px; border: 1px solid #000; }
            QTabWidget::pane { border: 1px solid #000; background: #fff; top: -1px; }
            QTabBar::tab { background: #ddd; color: #000; padding: 6px 12px; margin-right: 2px; border: 1px solid #000; border-bottom: none; }
            QTabBar::tab:selected { background: #fff; }
            QTableWidget, QTreeView { background: #fff; color: #000; gridline-color: #000; }
        """,
        "forest": """
            QWidget { background: #e8f5e9; color: #1b5e20; }
            QDockWidget::title { background: #2e7d32; color: #fff; padding: 6px 8px; font-weight: bold; }
            QPushButton { background: #388e3c; color: #fff; }
            QPushButton:hover { background: #43a047; }
            QHeaderView::section { background: #66bb6a; color: #fff; padding: 4px 8px; border: 1px solid #2e7d32; }
            QTabWidget::pane { border: 1px solid #2e7d32; background: #e8f5e9; top: -1px; }
            QTabBar::tab { background: #66bb6a; color: #fff; padding: 6px 12px; margin-right: 2px; border: 1px solid #2e7d32; border-bottom: none; }
            QTabBar::tab:selected { background: #e8f5e9; color: #1b5e20; }
            QTableWidget, QTreeView { background: #e8f5e9; color: #1b5e20; gridline-color: #2e7d32; }
        """,
        "midnight": """
            QWidget { background: #0d1b2a; color: #e0e1dd; }
            QDockWidget::title { background: #1b263b; color: #778da9; padding: 6px 8px; font-weight: bold; }
            QPushButton { background: #415a77; color: #e0e1dd; }
            QPushButton:hover { background: #778da9; }
            QHeaderView::section { background: #1b263b; color: #e0e1dd; padding: 4px 8px; border: 1px solid #415a77; }
            QTabWidget::pane { border: 1px solid #415a77; background: #0d1b2a; top: -1px; }
            QTabBar::tab { background: #1b263b; color: #e0e1dd; padding: 6px 12px; margin-right: 2px; border: 1px solid #415a77; border-bottom: none; }
            QTabBar::tab:selected { background: #0d1b2a; color: #778da9; }
            QTableWidget, QTreeView { background: #0d1b2a; color: #e0e1dd; gridline-color: #415a77; }
        """,
    }

    # Application version and description (constants, translated via _UI_STRINGS keys)
    APP_VERSION = "1.0.0"
    APP_DESCRIPTION_EN = (
        "Cross-platform graphical application for configuring and building projects "
        "that use the CMake build system.\n\n"
        "Supports single-configuration generators (e.g. Unix Makefiles, Ninja), "
        "custom C/C++ compilers, C/C++ standard selection, install prefix, CMAKE_PREFIX_PATH, "
        "and optional CMake cache variables (BOOL + STRING options from cmake -LH). "
        "Build and configure output is displayed in an integrated log panel with ANSI color support.\n\n"
        "Intended for developers and build engineers who require a unified GUI for CMake-based "
        "workflows without relying on the command line."
    )
    APP_DESCRIPTION_RU = (
        "Кроссплатформенное графическое приложение для настройки (configure) и сборки проектов, "
        "использующих систему сборки CMake.\n\n"
        "Поддерживаются одноконфигурационные генераторы (Unix Makefiles, Ninja), "
        "задание компиляторов C/C++, выбор стандартов языка, префикс установки, CMAKE_PREFIX_PATH "
        "и дополнительные переменные кэша CMake (BOOL + STRING опции из cmake -LH). "
        "Вывод configure и сборки отображается в интегрированной панели логов с поддержкой ANSI-цветов.\n\n"
        "Предназначено для разработчиков и инженеров сборки, которым требуется единый графический "
        "интерфейс для работы с проектами на CMake без использования командной строки."
    )

    # i18n: English and Russian for all UI strings
    _UI_STRINGS = {
        "en": {
            "window_title": "CMake Build GUI",
            "output": "Output",
            "settings": "Settings",
            "performance": "Performance",
            "sysinfo": "System:",
            "sysinfo_user": "User:",
            "sysinfo_os": "OS:",
            "sysinfo_cpu": "CPU:",
            "sysinfo_ram": "RAM:",
            "sysinfo_gpu": "GPU:",
            "tooltip_sysinfo": (
                "System information (User, OS, CPU, RAM, GPU) for support and diagnostics."
            ),
            "path": "Path:",
            "tooltip_path": "Project root directory containing CMakeLists.txt.",
            "build_type": "Build type:",
            "tooltip_build_type": "CMake build type: Debug, Release, RelWithDebInfo, or MinSizeRel.",
            "arch": "Architecture:",
            "tooltip_arch": "Target architecture. (auto) uses system default (e.g. x64).",
            "c_standard": "C standard:",
            "tooltip_c_standard": "C language standard (e.g. C17) passed to CMake.",
            "cpp_standard": "C++ standard:",
            "tooltip_cpp_standard": "C++ language standard (e.g. C++20) passed to CMake.",
            "use_ninja": "Use Ninja:",
            "tooltip_use_ninja": "Use Ninja generator for faster incremental builds. Cannot be changed after Configure.",
            "parallel_jobs": "Parallel jobs:",
            "tooltip_parallel_jobs": (
                "Number of parallel build jobs (cmake --build --parallel N). "
                "Range: 1 to {max} (logical CPUs on this machine)."
            ),
            "tooltip_shared_libs": "Build shared libraries (ON) or static libraries (OFF).",
            "cmake_prefix_path": "CMAKE_PREFIX_PATH:",
            "tooltip_cmake_prefix_path": "Semicolon-separated paths for CMake to find packages (e.g. Qt).",
            "extra_cmake_args": "Extra CMake args:",
            "tooltip_extra_cmake_args": 'Additional -DVAR=value arguments passed to CMake configure, e.g. "-DMY_OPTION_1=OFF -DMY_OPTION_2=ON"',
            "install_prefix": "Install prefix:",
            "tooltip_install_prefix": "Installation root (e.g. /usr/local or C:/Program Files/MyApp).",
            "compiler_c": "C compiler:",
            "tooltip_compiler_c": "Absolute path to C compiler. Type or pick from list (found recursively on this machine).",
            "compiler_cpp": "C++ compiler:",
            "tooltip_compiler_cpp": "Absolute path to C++ compiler. Type or pick from list (found recursively on this machine).",
            "tooltip_compiler_default_hint": (
                "(default) = system compiler; nothing is passed to CMake (CMake chooses). "
                "Windows: if Visual Studio is installed, system compiler is cl.exe "
                "(e.g. C:\\Program Files\\Microsoft Visual Studio\\2022\\Community\\VC\\Tools\\MSVC\\14.xx\\bin\\Hostx64\\x64\\cl.exe). "
                "Often there is no system compiler. Linux: gcc (e.g. /usr/bin/gcc)."
            ),
            "windows_version": "Windows compatibility:",
            "tooltip_windows_version": (
                "When (default) is selected, the program detects the current "
                "Windows version and passes it to CMake (WINDOWS_VERSION / WIN32_WINNT). "
                "No dependency on project CMakeLists.txt."
            ),
            "msvc_toolset": "MSVC toolset:",
            "tooltip_msvc_toolset": (
                "When (default) is selected, CMake uses the default toolset "
                "of the selected Visual Studio generator (e.g. v143 for VS 2022)."
            ),
            "hint_detected": "Detected: {}.",
            "clean": "Clean",
            "configure": "Configure",
            "build": "Build",
            "stop": "Stop",
            "flags": "Flags",
            "intellisense": "IntelliSense",
            "loading_options": "Loading options...",
            "auto": "(auto)",
            "default": "(default)",
            "reset": "Reset",
            "reset_tooltip": "Restore the entire UI (window layout, positions, and all settings) to default state.",
            "view": "View",
            "lang": "Lang",
            "theme_default": "Default",
            "theme_dark": "Dark",
            "theme_green": "Green",
            "theme_blue": "Blue",
            "theme_ocean": "Ocean",
            "theme_amber": "Amber",
            "theme_purple": "Purple",
            "theme_high_contrast": "High Contrast",
            "theme_forest": "Forest",
            "theme_midnight": "Midnight",
            "placeholder_path": "Project root (directory of CMakeLists.txt)",
            "placeholder_cmake_prefix": "e.g. /path/to/qt",
            "placeholder_cmake_args": "-DVAR=value",
            "placeholder_install": ("e.g. /usr/local or C:/Program Files/Lumex"),
            "placeholder_compiler_c": "e.g. /usr/bin/gcc (empty = default)",
            "placeholder_compiler_cpp": ("e.g. /usr/bin/g++ (empty = default)"),
            "generate_compile_commands": "Generate compile_commands.json",
            "clangd_hint": (
                "Generate compile_commands.json for Clangd/IntelliSense. "
                "Runs configure with Ninja and exports compile commands."
            ),
            "option": "Option",
            "description": "Description",
            "value": "Value",
            "no_cache_options": "(No cache options from cmake -LH)",
            "cmake_options_error_prefix": "Error details",
            "refresh_cmake_options": "Refresh",
            "tooltip_refresh_cmake_options": (
                "Reload CMake options from CMakeLists.txt (e.g. after adding new options)."
            ),
            "help": "Help",
            "shortcuts_menu": "Shortcuts…",
            "shortcuts_dialog_title": "Keyboard Shortcuts",
            "shortcut_close": "Close window",
            "shortcut_quit": "Quit application",
            "shortcut_open_project": "Open project",
            "shortcut_find": "Focus search",
            "shortcut_toggle_project": "Toggle Project panel",
            "shortcut_toggle_output": "Toggle Output panel",
            "shortcut_key": "Shortcut",
            "about": "About",
            "about_title": "About CMake Build GUI",
            "about_version_label": "Version:",
            "loading_cmake_options": "Loading CMake options…",
            "loading_may_take_seconds": "This may take a few seconds.",
            "flags_no_project_title": "No project opened",
            "flags_no_project_hint": (
                "Open a project (File → Open project) to load CMake options."
            ),
            "exit_confirm_title": "Exit",
            "exit_confirm_message": "Do you really want to exit?",
            "exit_confirm_with_task": (
                "A configuration or build is currently running. It will be "
                "terminated.\n\nDo you really want to exit?"
            ),
            "project": "Project",
            "open": "Open",
            "project_tree": "Project",
            "go_to_parent": "Go to parent directory",
            "show_all_files": "Show all files",
            "open_folder_title": "Open project folder",
            "open_folder_prompt": "Select a folder containing CMakeLists.txt",
            "no_cmake_in_folder": "The selected folder does not contain CMakeLists.txt.",
            "clear_logs": "Clear logs",
            "find": "Find:",
            "regex": "Regex",
            "case_sensitive": "Case sensitive",
            "whole_word": "Whole word",
            "search_placeholder": "Search (Ctrl+F)...",
            "theme_submenu": "Theme",
            "theme_configure": "Configure",
            "default_theme_dialog_title": "Default theme",
            "save": "Save",
            "generator_mismatch_title": "Generator mismatch",
            "generator_mismatch_message": (
                'The project was configured with "{configured}", but you have '
                "selected a different generator (Use Ninja: {ninja}).\n\n"
                "Build is disabled until you run Configure again with the desired "
                "options, or change the option back to match the current configuration."
            ),
            "use_ninja_locked_tooltip": (
                "Cannot change generator after configuration. Project was configured "
                'with "{generator}". Run Configure again to use another generator.'
            ),
            "hint_clean": "Remove build directory. After Clean, run Configure before Build.",
            "hint_clean_busy": "Remove build directory. (Disabled while a task is running.)",
            "hint_configure": "Run CMake configure. Required before Build.",
            "hint_configure_busy": "Run CMake configure. (Disabled while a task is running.)",
            "hint_build": "Build the project. Available only after Configure.",
            "hint_build_not_configured": "Build the project. (Run Configure first; after Clean, run Configure again.)",
            "hint_build_busy": "Build the project. (Disabled while a task is running.)",
            "hint_stop": "Stop the current configure or build.",
            "hint_stop_idle": "Stop the current task. (No task is running.)",
            "build_tools_missing_title": "Missing build tools",
            "build_tools_missing_cmake": (
                "CMake is not installed or not in PATH. It is required to configure and build the project."
            ),
            "build_tools_missing_compiler": (
                "No C/C++ compiler was found in PATH (e.g. GCC, Clang, or MSVC). A compiler is required to build."
            ),
            "build_tools_download_cmake": "Download CMake",
            "build_tools_ok": "OK",
            "monitor_cpu": "Build CPU %",
            "monitor_ram": "Build RAM (MB)",
            "axis_cpu": "CPU, %",
            "axis_ram": "RAM, MB",
            "axis_time": "Time, s",
            "help_requirements": (
                "Requirements: CMake and at least one C/C++ compiler (e.g. GCC, Clang, or MSVC) must be installed and available in PATH to configure and build projects."
            ),
        },
        "ru": {
            "window_title": "CMake Build GUI",
            "output": "Вывод",
            "settings": "Настройки",
            "performance": "Производительность",
            "sysinfo": "Система:",
            "sysinfo_user": "Пользователь:",
            "sysinfo_os": "ОС:",
            "sysinfo_cpu": "ЦПУ:",
            "sysinfo_ram": "ОЗУ:",
            "sysinfo_gpu": "ГПУ:",
            "tooltip_sysinfo": (
                "Сведения о системе (пользователь, ОС, ЦПУ, ОЗУ, ГПУ) для поддержки и диагностики."
            ),
            "path": "Путь:",
            "tooltip_path": "Корневой каталог проекта с CMakeLists.txt.",
            "build_type": "Тип сборки:",
            "tooltip_build_type": "Тип сборки CMake: Debug, Release, RelWithDebInfo или MinSizeRel.",
            "arch": "Архитектура:",
            "tooltip_arch": "Целевая архитектура. (авто) - по умолчанию системы (например x64).",
            "c_standard": "Стандарт C:",
            "tooltip_c_standard": "Стандарт языка C (например C17), передается в CMake.",
            "cpp_standard": "Стандарт C++:",
            "tooltip_cpp_standard": "Стандарт языка C++ (например C++20), передается в CMake.",
            "use_ninja": "Использовать Ninja:",
            "parallel_jobs": "Параллельные задачи:",
            "tooltip_parallel_jobs": (
                "Число параллельных задач сборки (cmake --build --parallel N). "
                "Диапазон: 1 до {max} (логических ядер на этой машине)."
            ),
            "tooltip_shared_libs": "Собирать динамические библиотеки (ON) или статические (OFF).",
            "tooltip_use_ninja": "Использовать генератор Ninja для ускорения инкрементальной сборки. Нельзя изменить после Настроить.",
            "cmake_prefix_path": "CMAKE_PREFIX_PATH:",
            "tooltip_cmake_prefix_path": "Пути через точку с запятой для поиска пакетов CMake (например Qt).",
            "extra_cmake_args": "Доп. аргументы CMake:",
            "tooltip_extra_cmake_args": 'Дополнительные аргументы -DVAR=value для CMake configure, например "-DMY_OPTION_1=OFF -DMY_OPTION_2=ON"',
            "install_prefix": "Каталог установки:",
            "tooltip_install_prefix": "Корень установки (например /usr/local или C:/Program Files/MyApp).",
            "compiler_c": "Компилятор C:",
            "tooltip_compiler_c": "Абсолютный путь к компилятору C. Ввод или выбор из списка (найдены рекурсивно на машине).",
            "compiler_cpp": "Компилятор C++:",
            "tooltip_compiler_cpp": "Абсолютный путь к компилятору C++. Ввод или выбор из списка (найдены рекурсивно на машине).",
            "tooltip_compiler_default_hint": (
                "(по умолчанию) = системный компилятор; в CMake ничего не передается (выбор CMake). "
                "Windows: при установленной Visual Studio системный компилятор — cl.exe "
                "(напр. C:\\Program Files\\Microsoft Visual Studio\\2022\\Community\\VC\\Tools\\MSVC\\14.xx\\bin\\Hostx64\\x64\\cl.exe). "
                "Часто системного компилятора нет. Linux: gcc (напр. /usr/bin/gcc)."
            ),
            "windows_version": "Совместимость Windows:",
            "tooltip_windows_version": (
                "При выборе (по умолчанию) программа определяет версию текущей "
                "Windows и передает ее в CMake (WINDOWS_VERSION / WIN32_WINNT). "
                "Не зависит от CMakeLists.txt проекта."
            ),
            "msvc_toolset": "Набор MSVC:",
            "tooltip_msvc_toolset": (
                "При выборе (по умолчанию) CMake использует набор инструментов "
                "по умолчанию выбранного генератора Visual Studio (например v143 для VS 2022)."
            ),
            "hint_detected": "Определено: {}.",
            "clean": "Очистить",
            "configure": "Настроить",
            "build": "Собрать",
            "stop": "Стоп",
            "flags": "Флаги",
            "intellisense": "IntelliSense",
            "loading_options": "Загрузка опций...",
            "auto": "(авто)",
            "default": "(по умолчанию)",
            "reset": "Сброс",
            "reset_tooltip": "Вернуть весь интерфейс (расположение окна, панелей и все настройки) в состояние по умолчанию.",
            "view": "Вид",
            "lang": "Язык",
            "theme_default": "По умолчанию",
            "theme_dark": "Темная",
            "theme_green": "Зеленая",
            "theme_blue": "Синяя",
            "theme_ocean": "Океан",
            "theme_amber": "Янтарная",
            "theme_purple": "Фиолетовая",
            "theme_high_contrast": "Высокая контрастность",
            "theme_forest": "Лес",
            "theme_midnight": "Полночь",
            "placeholder_path": "Корень проекта (каталог CMakeLists.txt)",
            "placeholder_cmake_prefix": "например /path/to/qt",
            "placeholder_cmake_args": "-DVAR=value",
            "placeholder_install": ("например /usr/local или C:/Program Files/Lumex"),
            "placeholder_compiler_c": ("например /usr/bin/gcc (пусто = по умолчанию)"),
            "placeholder_compiler_cpp": (
                "например /usr/bin/g++ (пусто = по умолчанию)"
            ),
            "generate_compile_commands": "Создать compile_commands.json",
            "clangd_hint": (
                "Создать compile_commands.json для Clangd/IntelliSense. "
                "Запускает configure с Ninja и экспортирует команды компиляции."
            ),
            "option": "Опция",
            "description": "Описание",
            "value": "Значение",
            "no_cache_options": "(Нет cache-опций из cmake -LH)",
            "cmake_options_error_prefix": "Детали ошибки",
            "refresh_cmake_options": "Обновить",
            "tooltip_refresh_cmake_options": (
                "Перезагрузить опции CMake из CMakeLists.txt (например, после добавления новых опций)."
            ),
            "help": "Справка",
            "shortcuts_menu": "Горячие клавиши…",
            "shortcuts_dialog_title": "Горячие клавиши",
            "shortcut_close": "Закрыть окно",
            "shortcut_quit": "Выход из приложения",
            "shortcut_open_project": "Открыть проект",
            "shortcut_find": "Фокус в поиск",
            "shortcut_toggle_project": "Панель проекта",
            "shortcut_toggle_output": "Панель вывода",
            "shortcut_key": "Сочетание",
            "about": "О программе",
            "about_title": "О программе CMake Build GUI",
            "about_version_label": "Версия:",
            "loading_cmake_options": "Загрузка опций CMake…",
            "loading_may_take_seconds": "Это может занять несколько секунд.",
            "flags_no_project_title": "Проект не открыт",
            "flags_no_project_hint": (
                "Откройте проект (Файл → Открыть проект), чтобы загрузить опции CMake."
            ),
            "exit_confirm_title": "Выход",
            "exit_confirm_message": "Действительно выйти из приложения?",
            "exit_confirm_with_task": (
                "Сейчас выполняется настройка или сборка. Она будет "
                "прервана.\n\nДействительно выйти?"
            ),
            "project": "Проект",
            "open": "Открыть",
            "project_tree": "Проект",
            "go_to_parent": "Перейти в родительский каталог",
            "show_all_files": "Показывать все файлы",
            "open_folder_title": "Открыть папку проекта",
            "open_folder_prompt": "Выберите папку с CMakeLists.txt",
            "no_cmake_in_folder": "В выбранной папке нет CMakeLists.txt.",
            "clear_logs": "Очистить логи",
            "find": "Найти:",
            "regex": "Regex",
            "case_sensitive": "Учет регистра",
            "whole_word": "Целое слово",
            "search_placeholder": "Поиск (Ctrl+F)...",
            "theme_submenu": "Тема",
            "theme_configure": "Настроить",
            "default_theme_dialog_title": "Тема по умолчанию",
            "save": "Сохранить",
            "generator_mismatch_title": "Несовпадение генератора",
            "generator_mismatch_message": (
                'Проект был настроен с генератором "{configured}", но вы выбрали '
                "другой (Использовать Ninja: {ninja}).\n\n"
                "Сборка отключена, пока вы снова не запустите Настроить с нужными "
                "опциями или не вернете опцию в соответствие с текущей конфигурацией."
            ),
            "use_ninja_locked_tooltip": (
                "После конфигурации менять генератор нельзя. Проект был настроен "
                'с генератором "{generator}". Запустите Настроить снова, чтобы '
                "использовать другой генератор."
            ),
            "hint_clean": "Удалить каталог сборки. После Очистить нужно снова запустить Настроить перед Сборкой.",
            "hint_clean_busy": "Удалить каталог сборки. (Недоступно во время выполнения задачи.)",
            "hint_configure": "Запустить CMake configure. Необходимо перед Сборкой.",
            "hint_configure_busy": "Запустить CMake configure. (Недоступно во время выполнения задачи.)",
            "hint_build": "Собрать проект. Доступно только после Настроить.",
            "hint_build_not_configured": "Собрать проект. (Сначала запустите Настроить; после Очистить - снова Настроить.)",
            "hint_build_busy": "Собрать проект. (Недоступно во время выполнения задачи.)",
            "hint_stop": "Остановить текущую настройку или сборку.",
            "hint_stop_idle": "Остановить текущую задачу. (Нет выполняемой задачи.)",
            "build_tools_missing_title": "Отсутствуют инструменты сборки",
            "build_tools_missing_cmake": (
                "CMake не установлен или отсутствует в PATH. Он нужен для настройки и сборки проекта."
            ),
            "build_tools_missing_compiler": (
                "Компилятор C/C++ не найден в PATH (например GCC, Clang или MSVC). Компилятор нужен для сборки."
            ),
            "build_tools_download_cmake": "Скачать CMake",
            "build_tools_ok": "OK",
            "monitor_cpu": "ЦПУ сборки %",
            "monitor_ram": "RAM сборки (МБ)",
            "axis_cpu": "ЦПУ, %",
            "axis_ram": "RAM, МБ",
            "axis_time": "Время, с",
            "help_requirements": (
                "Требования: для настройки и сборки проектов необходимы CMake и хотя бы один компилятор C/C++ (например GCC, Clang или MSVC), установленные и доступные в PATH."
            ),
        },
    }

    def _make_flags_loading_widget(tr: dict) -> QWidget:
        """Build a lightweight loading widget with indeterminate progress for the Flags tab."""
        widget = QWidget()
        layout = QVBoxLayout(widget)
        layout.addStretch()
        align_center = getattr(Qt, "AlignCenter", 0x84)
        label_main = QLabel(tr.get("loading_cmake_options", "Loading CMake options…"))
        label_main.setAlignment(align_center)  # type: ignore[arg-type]
        label_main.setStyleSheet("font-size: 13px; color: #555;")
        layout.addWidget(label_main)
        progress = QProgressBar()
        progress.setRange(0, 0)
        progress.setMinimumWidth(240)
        progress.setMaximumWidth(320)
        progress.setFixedHeight(8)
        progress.setStyleSheet(
            "QProgressBar { border: 1px solid #ddd; border-radius: 4px; background: #f5f5f5; } "
            "QProgressBar::chunk { background: qlineargradient(x1:0, y1:0, x2:1, y2:0, "
            "stop:0 #6eb6ff, stop:1 #4a9eff); border-radius: 3px; }"
        )
        layout.addWidget(progress, 0, align_center)  # type: ignore[arg-type]
        label_hint = QLabel(
            tr.get("loading_may_take_seconds", "This may take a few seconds.")
        )
        label_hint.setAlignment(align_center)  # type: ignore[arg-type]
        label_hint.setStyleSheet("font-size: 11px; color: #888;")
        layout.addWidget(label_hint)
        layout.addStretch()
        return widget

    def _make_flags_no_project_widget(tr: dict) -> QWidget:
        """Build a placeholder widget for the Flags tab when no CMake project is opened."""
        widget = QWidget()
        layout = QVBoxLayout(widget)
        layout.addStretch()
        align_center = getattr(Qt, "AlignCenter", 0x84)
        label_title = QLabel(tr.get("flags_no_project_title", "No project opened"))
        label_title.setAlignment(align_center)  # type: ignore[arg-type]
        label_title.setStyleSheet("font-size: 14px; font-weight: bold; color: #555;")
        layout.addWidget(label_title)
        label_hint = QLabel(
            tr.get(
                "flags_no_project_hint",
                "Open a project (File → Open project) to load CMake options.",
            )
        )
        label_hint.setAlignment(align_center)  # type: ignore[arg-type]
        label_hint.setWordWrap(True)
        label_hint.setStyleSheet("font-size: 12px; color: #888;")
        layout.addWidget(label_hint)
        layout.addStretch()
        return widget

    def _get_system_info_text(tr: dict) -> str:
        """Build a short system info string: User, OS, CPU (cores), RAM, GPU."""
        user = os_environ.get("USER") or os_environ.get("USERNAME") or "?"
        os_name = "{} {}".format(platform_system(), platform_release()).strip()
        try:
            phys = psutil_cpu_count(logical=False) or 0
            log = psutil_cpu_count(logical=True) or 0
            cpu_cores = "{} ({} phys, {} log)".format(
                platform_processor() or "N/A", phys, log
            )
        except Exception:
            cpu_cores = platform_processor() or "N/A"
        try:
            total_bytes = psutil_virtual_memory().total
            ram_gb = total_bytes / (1024**3)
            ram_str = "{:.1f} GB".format(ram_gb)
        except Exception:
            ram_str = "N/A"
        gpu_str = "N/A"
        try:
            if platform_system() == "Windows":
                r = subprocess_run(
                    ["wmic", "path", "win32_VideoController", "get", "Name"],
                    capture_output=True,
                    timeout=3,
                    **_SUBPROCESS_NO_WINDOW,
                )
                if r.returncode == 0 and r.stdout:
                    lines = [
                        x.strip()
                        for x in r.stdout.decode("utf-8", errors="ignore").splitlines()
                        if x.strip() and x.strip() != "Name"
                    ]
                    _virtual_keywords = (
                        "microsoft remote display",
                        "microsoft basic",
                        "standard vga",
                        "basic display",
                        "virtual",
                        "hyper-v",
                        "vmware",
                        "virtualbox",
                        "remote display adapter",
                    )
                    for name in lines:
                        name_lower = name.lower()
                        if (
                            not any(kw in name_lower for kw in _virtual_keywords)
                            and len(name_lower) > 3
                        ):
                            gpu_str = name[:80]
                            break
                    if gpu_str == "N/A" and lines:
                        gpu_str = lines[0][:80]
            if gpu_str == "N/A":
                r = subprocess_run(
                    ["nvidia-smi", "--query-gpu=name", "--format=csv,noheader"],
                    capture_output=True,
                    timeout=2,
                    **_SUBPROCESS_NO_WINDOW,
                )
                if r.returncode == 0 and r.stdout:
                    gpu_str = r.stdout.decode("utf-8", errors="ignore").strip()[:80]
        except Exception:
            pass
        lbl_user = tr.get("sysinfo_user", "User:")
        lbl_os = tr.get("sysinfo_os", "OS:")
        lbl_cpu = tr.get("sysinfo_cpu", "CPU:")
        lbl_ram = tr.get("sysinfo_ram", "RAM:")
        lbl_gpu = tr.get("sysinfo_gpu", "GPU:")
        return "\n".join(
            [
                "{} {}".format(lbl_user, user),
                "{} {}".format(lbl_os, os_name),
                "{} {}".format(lbl_cpu, cpu_cores),
                "{} {}".format(lbl_ram, ram_str),
                "{} {}".format(lbl_gpu, gpu_str),
            ]
        )

    class _BuildMonitorWidget(QWidget):
        """Real-time CPU and RAM usage graphs for build processes."""

        _kHistorySize = 180
        _kUpdateIntervalMs = 120
        _kSmoothPoints = 4

        def __init__(
            self,
            is_building_fn: Callable[[], bool],
            tr_dict: dict,
        ) -> None:
            super().__init__()
            self._is_building = is_building_fn
            self._tr = tr_dict
            self._cpu_history: List[float] = []
            self._ram_history: List[float] = []
            self._last_cpu_times: Dict[int, float] = {}
            self._last_sample_time: Optional[float] = None
            layout = QVBoxLayout(self)
            layout.setContentsMargins(4, 4, 4, 4)
            self._figure = Figure(figsize=(5, 3.2), dpi=110)  # type: ignore
            self._figure.patch.set_facecolor("#f5f5f5")
            self._figure.subplots_adjust(
                hspace=0.4, left=0.12, right=0.96, top=0.94, bottom=0.12
            )
            self._ax_cpu = self._figure.add_subplot(211)
            self._ax_ram = self._figure.add_subplot(212)
            self._sec_per_sample = self._kUpdateIntervalMs / 1000.0
            self._max_sec = self._kHistorySize * self._sec_per_sample
            for ax in (self._ax_cpu, self._ax_ram):
                ax.set_facecolor("#fafafa")
                ax.tick_params(axis="both", which="major", labelsize=9)
                ax.minorticks_on()
                ax.grid(True, which="major", alpha=0.25, linestyle="-")
                ax.grid(True, which="minor", alpha=0.12, linestyle="-", linewidth=0.5)
                ax.spines["top"].set_visible(False)
                ax.spines["right"].set_visible(False)
            self._ax_cpu.set_ylabel(
                tr_dict.get("axis_cpu", "CPU, %"),
                fontsize=9,
                fontweight="bold",
            )
            self._ax_ram.set_ylabel(
                tr_dict.get("axis_ram", "RAM, MB"),
                fontsize=9,
                fontweight="bold",
            )
            for ax in (self._ax_cpu, self._ax_ram):
                ax.set_xlabel(
                    tr_dict.get("axis_time", "Time, s"),
                    fontsize=9,
                )
            self._ax_cpu.set_ylim(0, 100)
            self._ax_cpu.set_xlim(0, self._max_sec)
            self._ax_ram.set_xlim(0, self._max_sec)
            (self._line_cpu,) = self._ax_cpu.plot(
                [], [], color="#2563eb", linewidth=2.0, antialiased=True
            )
            (self._line_ram,) = self._ax_ram.plot(
                [], [], color="#059669", linewidth=2.0, antialiased=True
            )
            self._figure.tight_layout()
            self._canvas = FigureCanvasQTAgg(self._figure)  # type: ignore
            self._canvas.setMinimumSize(280, 240)
            self._canvas.setSizePolicy(
                QSizePolicy.Expanding,  # type: ignore[possibly-unbound]
                QSizePolicy.Expanding,  # type: ignore[possibly-unbound]
            )
            layout.addWidget(self._canvas, 1)
            self._timer = QTimer(self)
            self._timer.timeout.connect(self._on_tick)
            self._timer.start(self._kUpdateIntervalMs)

        def _smooth_curve(self, y: List[float]) -> Tuple[List[float], List[float]]:
            if len(y) < 2:
                return (list(range(len(y))), list(y))
            try:
                import numpy as np

                n = len(y)
                x_orig = np.arange(n, dtype=float)
                y_arr = np.array(y, dtype=float)
                n_smooth = max(n * self._kSmoothPoints, 2)
                x_smooth = np.linspace(0, n - 1, n_smooth)
                y_smooth = np.interp(x_smooth, x_orig, y_arr)
                return (x_smooth.tolist(), y_smooth.tolist())
            except ImportError:
                return (list(range(len(y))), list(y))

        def _collect_cpu_ram(self) -> Tuple[float, float]:
            """Run in worker thread: collect CPU % and RAM MB for build child processes."""
            ram_mb = 0.0
            cpu_pct = 0.0
            try:
                proc = psutil_Process()
                children = proc.children(recursive=True)
                now = time_perf_counter()
                cpu_delta = 0.0
                current_times: Dict[int, float] = {}
                for p in children:
                    try:
                        times = p.cpu_times()
                        cpu_sec = times.user + times.system
                        current_times[p.pid] = cpu_sec
                        prev = self._last_cpu_times.get(p.pid)
                        if prev is not None:
                            cpu_delta += cpu_sec - prev
                        ram_mb += p.memory_info().rss / (1024 * 1024)
                    except (psutil_NoSuchProcess, psutil_AccessDenied):
                        pass
                self._last_cpu_times = current_times
                if self._last_sample_time is not None:
                    elapsed = now - self._last_sample_time
                    if elapsed >= 0.05:
                        num_cores = psutil_cpu_count(logical=True) or 1
                        cpu_pct = (cpu_delta / elapsed) * 100.0 / num_cores
                self._last_sample_time = now
            except Exception:
                pass
            return (cpu_pct, ram_mb)

        def _on_tick(self) -> None:
            if not self._is_building():
                self._last_cpu_times.clear()
                self._last_sample_time = None
                if self._cpu_history or self._ram_history:
                    self._cpu_history.append(0.0)
                    self._ram_history.append(0.0)
                    self._trim_history()
                    self._redraw()
                return
            try:
                cpu_pct, ram_mb = self._collect_cpu_ram()
                self._cpu_history.append(cpu_pct)
                self._ram_history.append(ram_mb)
                self._trim_history()
                self._redraw()
            except Exception:
                pass

        def _trim_history(self) -> None:
            while len(self._cpu_history) > self._kHistorySize:
                self._cpu_history.pop(0)
            while len(self._ram_history) > self._kHistorySize:
                self._ram_history.pop(0)

        def _index_to_sec(self, x_idx: List[Any]) -> List[float]:
            return [float(xi) * self._sec_per_sample for xi in x_idx]

        def _fill_gradient(
            self, ax: Any, x: List[float], y: List[float], color: str, y_max: float
        ) -> None:
            """Fill under curve with uniform shaded area."""
            if not x or not y or len(x) != len(y):
                return
            ax.fill_between(x, y, alpha=0.2, color=color)

        def _redraw(self) -> None:
            n = len(self._cpu_history)
            if n < 2:
                x_idx = list(range(n))
                y_cpu = list(self._cpu_history)
                y_ram = list(self._ram_history)
            else:
                x_idx, y_cpu = self._smooth_curve(self._cpu_history)
                _, y_ram = self._smooth_curve(self._ram_history)
            x_sec = self._index_to_sec(x_idx)
            for coll in list(self._ax_cpu.collections):
                coll.remove()
            for coll in list(self._ax_ram.collections):
                coll.remove()
            for img in list(self._ax_cpu.images):
                img.remove()
            for img in list(self._ax_ram.images):
                img.remove()
            self._line_cpu.set_data(x_sec, y_cpu)
            self._line_ram.set_data(x_sec, y_ram)
            if n > 1:
                cpu_ymax = max(max(y_cpu), 1)
                self._fill_gradient(self._ax_cpu, x_sec, y_cpu, "#2563eb", cpu_ymax)
                ram_ymax = max(max(y_ram), 0.001)
                self._fill_gradient(self._ax_ram, x_sec, y_ram, "#059669", ram_ymax)
            if self._ram_history:
                ymax = max(self._ram_history) * 1.15 or 1
                self._ax_ram.set_ylim(0, ymax)
            self._canvas.draw_idle()

    class _ScrollChildWheelFilter(QObject):
        """Event filter to redirect wheel events from QComboBox/QSpinBox to scroll area."""

        def __init__(self, scroll_area: QScrollArea) -> None:
            super().__init__(scroll_area)
            self._scroll = scroll_area

        def eventFilter(self, obj: QObject, event: QEvent) -> bool:  # type: ignore
            if event.type() == QEvent.Wheel and isinstance(  # type: ignore
                obj, (QComboBox, QSpinBox)  # type: ignore
            ):
                if isinstance(obj, QComboBox) and bool(
                    obj.property("disableWheelChange")
                ):
                    return True
                if self._scroll.viewport() is not None:
                    QApplication.sendEvent(self._scroll.viewport(), event)  # type: ignore
                return True
            return False

    class _NoWheelComboBox(QComboBox):
        """QComboBox that ignores mouse-wheel changes when popup is closed."""

        def wheelEvent(self, event: QEvent) -> None:  # type: ignore[override]
            popup_view = self.view()
            if popup_view is None or not popup_view.isVisible():
                event.ignore()  # type: ignore[call-arg]
                return
            super().wheelEvent(event)  # type: ignore[misc]

    class _SettingsPanel(QWidget):
        def __init__(self, tr_dict: Optional[dict] = None) -> None:
            super().__init__()
            self._tr = tr_dict or _UI_STRINGS["en"]
            layout = QFormLayout(self)
            self._sysinfo_label = QLabel(_get_system_info_text(self._tr))
            self._sysinfo_label.setWordWrap(True)
            self._sysinfo_label.setStyleSheet("font-size: 11px; color: #555;")
            self._sysinfo_label.setToolTip(
                self._tr.get(
                    "tooltip_sysinfo",
                    "System information (User, OS, CPU, RAM, GPU) for support and diagnostics.",
                )
            )
            layout.addRow(self._tr.get("sysinfo", "System:"), self._sysinfo_label)
            self._path = QLabel()
            self._path.setToolTip(
                self._tr.get(
                    "tooltip_path", "Project root directory containing CMakeLists.txt."
                )
            )
            self._path.setWordWrap(True)
            _sel = getattr(Qt, "TextSelectableByMouse", None)
            if _sel is not None:
                self._path.setTextInteractionFlags(_sel)  # type: ignore[arg-type]
            layout.addRow(self._tr.get("path", "Path:"), self._path)
            self._form_layout = layout
            self._field_keys = [("sysinfo", self._sysinfo_label), ("path", self._path)]
            self._build_type = QComboBox()
            self._build_type.setToolTip(
                self._tr.get(
                    "tooltip_build_type",
                    "CMake build type: Debug, Release, RelWithDebInfo, or MinSizeRel.",
                )
            )
            self._build_type.addItems(
                [
                    "Debug",
                    "Release",
                    "RelWithDebInfo",
                    "MinSizeRel",
                ]
            )
            self._build_type.setCurrentText("Release")
            layout.addRow(self._tr.get("build_type", "Build type:"), self._build_type)
            self._field_keys.append(("build_type", self._build_type))
            self._arch = QComboBox()
            self._arch.addItem(self._tr.get("auto", "(auto)"), None)
            self._arch.addItem("x86", "x86")
            self._arch.addItem("x64", "x64")
            self._arch.setToolTip(
                self._tr.get(
                    "tooltip_arch",
                    "Target architecture. (auto) uses system default (e.g. x64).",
                )
            )
            layout.addRow(self._tr.get("arch", "Architecture:"), self._arch)
            self._field_keys.append(("arch", self._arch))
            self._stdc = QComboBox()
            self._stdc.setToolTip(
                self._tr.get(
                    "tooltip_c_standard",
                    "C language standard (e.g. C17) passed to CMake.",
                )
            )
            self._stdc = QComboBox()
            self._stdc.setToolTip(
                self._tr.get(
                    "tooltip_c_standard",
                    "C language standard (e.g. C17) passed to CMake.",
                )
            )
            for std in ALL_C_STANDARDS_UI:
                self._stdc.addItem("C{}".format(std), std)
            self._stdc.setCurrentIndex(self._stdc.findData(17))
            layout.addRow(self._tr.get("c_standard", "C standard:"), self._stdc)
            self._field_keys.append(("c_standard", self._stdc))
            self._stdcxx = QComboBox()
            self._stdcxx.setToolTip(
                self._tr.get(
                    "tooltip_cpp_standard",
                    "C++ language standard (e.g. C++20) passed to CMake.",
                )
            )
            for std in ALL_CPP_STANDARDS_UI:
                self._stdcxx.addItem("C++{}".format(std), std)
            self._stdcxx.setCurrentIndex(self._stdcxx.findData(20))
            layout.addRow(self._tr.get("cpp_standard", "C++ standard:"), self._stdcxx)
            self._field_keys.append(("cpp_standard", self._stdcxx))
            self._shared_libs = QCheckBox()
            self._shared_libs.setToolTip(
                self._tr.get(
                    "tooltip_shared_libs",
                    "Build shared libraries (ON) or static libraries (OFF).",
                )
            )
            self._field_keys.append(("shared_libs", self._shared_libs))
            self._use_ninja = QCheckBox()
            self._use_ninja.setToolTip(
                self._tr.get(
                    "tooltip_use_ninja",
                    "Use Ninja generator for faster incremental builds. Cannot be changed after Configure.",
                )
            )
            layout.addRow(self._tr.get("use_ninja", "Use Ninja:"), self._use_ninja)
            self._field_keys.append(("use_ninja", self._use_ninja))
            _max_parallel = psutil_cpu_count(logical=True) or 1
            _default_parallel = max(
                1,
                min(psutil_cpu_count(logical=False) or 1, _max_parallel),
            )
            self._parallel_jobs = QSpinBox()  # type: ignore
            self._parallel_jobs.setRange(1, _max_parallel)
            self._parallel_jobs.setValue(_default_parallel)
            self._parallel_jobs.setToolTip(
                self._tr.get(
                    "tooltip_parallel_jobs",
                    "Number of parallel build jobs (cmake --build --parallel N). "
                    "Range: 1 to {max} (logical CPUs on this machine).",
                ).format(max=_max_parallel)
            )
            layout.addRow(
                self._tr.get("parallel_jobs", "Parallel jobs:"),
                self._parallel_jobs,
            )
            self._field_keys.append(("parallel_jobs", self._parallel_jobs))
            self._cmake_prefix_path = QLineEdit()
            self._cmake_prefix_path.setPlaceholderText(
                self._tr.get("placeholder_cmake_prefix", "e.g. /path/to/qt")
            )
            self._cmake_prefix_path.setToolTip(
                self._tr.get(
                    "tooltip_cmake_prefix_path",
                    "Semicolon-separated paths for CMake to find packages (e.g. Qt).",
                )
            )
            layout.addRow(
                self._tr.get("cmake_prefix_path", "CMAKE_PREFIX_PATH:"),
                self._cmake_prefix_path,
            )
            self._field_keys.append(("cmake_prefix_path", self._cmake_prefix_path))
            self._cmake_args = QLineEdit()
            self._cmake_args.setPlaceholderText(
                self._tr.get("placeholder_cmake_args", "-DVAR=value")
            )
            self._cmake_args.setToolTip(
                self._tr.get(
                    "tooltip_extra_cmake_args",
                    "Additional -DVAR=value arguments passed to CMake configure.",
                )
            )
            layout.addRow(
                self._tr.get("extra_cmake_args", "Extra CMake args:"), self._cmake_args
            )
            self._field_keys.append(("extra_cmake_args", self._cmake_args))
            self._install_prefix = QLineEdit()
            self._install_prefix.setPlaceholderText(
                self._tr.get(
                    "placeholder_install", "e.g. /usr/local or C:/Program Files/Lumex"
                )
            )
            self._install_prefix.setToolTip(
                self._tr.get(
                    "tooltip_install_prefix",
                    "Installation root (e.g. /usr/local or C:/Program Files/MyApp).",
                )
            )
            layout.addRow(
                self._tr.get("install_prefix", "Install prefix:"), self._install_prefix
            )
            self._field_keys.append(("install_prefix", self._install_prefix))
            self._compiler_c = QComboBox()
            self._compiler_c.setEditable(True)
            self._compiler_c.addItem(self._tr.get("default", "(default)"), None)
            self._compiler_c.setCurrentIndex(0)
            le_c = self._compiler_c.lineEdit()
            if le_c is not None:
                le_c.setPlaceholderText(
                    self._tr.get(
                        "placeholder_compiler_c", "e.g. /usr/bin/gcc (empty = default)"
                    )
                )
            _tip_c = self._tr.get(
                "tooltip_compiler_c",
                "Absolute path to C compiler. Type or pick from list (found recursively on this machine).",
            )
            _tip_c += "\n\n" + self._tr.get(
                "tooltip_compiler_default_hint",
                "(default) = system compiler; nothing is passed to CMake (CMake chooses). "
                "Windows: if Visual Studio is installed, system compiler is cl.exe (e.g. C:\\...\\MSVC\\14.xx\\bin\\Hostx64\\x64\\cl.exe). "
                "Linux: gcc (e.g. /usr/bin/gcc).",
            )
            self._compiler_c.setToolTip(_tip_c)
            self._compiler_c_default_tip = _tip_c
            layout.addRow(self._tr.get("compiler_c", "C compiler:"), self._compiler_c)
            self._field_keys.append(("compiler_c", self._compiler_c))
            self._compiler_cpp = QComboBox()
            self._compiler_cpp.setEditable(True)
            self._compiler_cpp.addItem(self._tr.get("default", "(default)"), None)
            self._compiler_cpp.setCurrentIndex(0)
            le_cpp = self._compiler_cpp.lineEdit()
            if le_cpp is not None:
                le_cpp.setPlaceholderText(
                    self._tr.get(
                        "placeholder_compiler_cpp",
                        "e.g. /usr/bin/g++ (empty = default)",
                    )
                )
            _tip_cpp = self._tr.get(
                "tooltip_compiler_cpp",
                "Absolute path to C++ compiler. Type or pick from list (found recursively on this machine).",
            )
            _tip_cpp += "\n\n" + self._tr.get(
                "tooltip_compiler_default_hint",
                "(default) = system compiler; nothing is passed to CMake (CMake chooses). "
                "Windows: if Visual Studio is installed, system compiler is cl.exe (e.g. C:\\...\\MSVC\\14.xx\\bin\\Hostx64\\x64\\cl.exe). "
                "Linux: gcc (e.g. /usr/bin/gcc).",
            )
            self._compiler_cpp.setToolTip(_tip_cpp)
            self._compiler_cpp_default_tip = _tip_cpp
            layout.addRow(
                self._tr.get("compiler_cpp", "C++ compiler:"), self._compiler_cpp
            )
            self._field_keys.append(("compiler_cpp", self._compiler_cpp))
            self._compiler_c.currentIndexChanged.connect(
                self._update_standards_for_compiler
            )
            self._compiler_cpp.currentIndexChanged.connect(
                self._update_standards_for_compiler
            )
            self._compiler_c.currentIndexChanged.connect(
                lambda: self._update_compiler_combo_tooltip(True)
            )
            self._compiler_c.currentTextChanged.connect(
                lambda t: self._update_compiler_combo_tooltip(True)
            )
            self._compiler_cpp.currentIndexChanged.connect(
                lambda: self._update_compiler_combo_tooltip(False)
            )
            self._compiler_cpp.currentTextChanged.connect(
                lambda t: self._update_compiler_combo_tooltip(False)
            )
            le_c_edit = self._compiler_c.lineEdit()
            if le_c_edit is not None:
                le_c_edit.textEdited.connect(self._update_standards_for_compiler)
            le_cpp_edit = self._compiler_cpp.lineEdit()
            if le_cpp_edit is not None:
                le_cpp_edit.textEdited.connect(self._update_standards_for_compiler)
            self._windows_version = QComboBox()
            self._windows_version.addItem(self._tr.get("default", "(default)"), None)
            for ver in ("XP", "VISTA", "SEVEN", "EIGHT", "EIGHTDOTONE", "TENELEVEN"):
                self._windows_version.addItem(ver, ver)
            _tip_wv = self._tr.get(
                "tooltip_windows_version",
                "When (default) is selected, the program detects the current "
                "Windows version and passes it to CMake (WINDOWS_VERSION / WIN32_WINNT).",
            )
            if platform_system() == "Windows":
                _dw = detect_windows_version()
                if _dw:
                    _tip_wv += "\n" + self._tr.get(
                        "hint_detected", "Detected: {}."
                    ).format(_dw)
            self._windows_version.setToolTip(_tip_wv)
            layout.addRow(
                self._tr.get("windows_version", "Windows compatibility:"),
                self._windows_version,
            )
            self._field_keys.append(("windows_version", self._windows_version))
            self._toolset = QComboBox()
            self._toolset.addItem(self._tr.get("default", "(default)"), None)
            if platform_system() == "Windows":
                for toolset_id, label in detect_installed_msvc_toolsets():
                    self._toolset.addItem(label, toolset_id)
            _tip_ts = self._tr.get(
                "tooltip_msvc_toolset",
                "When (default) is selected, CMake uses the default toolset "
                "of the selected Visual Studio generator (e.g. v143 for VS 2022).",
            )
            if platform_system() == "Windows":
                _dt = detect_msvc_toolset()
                if _dt:
                    _tip_ts += "\n" + self._tr.get(
                        "hint_detected", "Detected: {}."
                    ).format(_dt)
            self._toolset.setToolTip(_tip_ts)
            layout.addRow(self._tr.get("msvc_toolset", "MSVC toolset:"), self._toolset)
            self._field_keys.append(("msvc_toolset", self._toolset))
            if platform_system() != "Windows":
                self._windows_version.setVisible(False)
                lbl_wv = layout.labelForField(self._windows_version)
                if lbl_wv:
                    lbl_wv.setVisible(False)
                self._toolset.setVisible(False)
                lbl = layout.labelForField(self._toolset)
                if lbl:
                    lbl.setVisible(False)

        def apply_ui_strings(self, strings: dict) -> None:
            """Update all form labels, placeholders and tooltips when language changes."""
            self._tr = strings
            for key, widget in self._field_keys:
                lbl = self._form_layout.labelForField(widget)
                if lbl is not None and key in strings:
                    lbl.setText(strings[key])
            self._sysinfo_label.setText(_get_system_info_text(strings))
            self._sysinfo_label.setToolTip(
                strings.get(
                    "tooltip_sysinfo",
                    "System information (User, OS, CPU, RAM, GPU) for support and diagnostics.",
                )
            )
            self._path.setToolTip(strings.get("tooltip_path", ""))
            self._build_type.setToolTip(strings.get("tooltip_build_type", ""))
            self._arch.setToolTip(strings.get("tooltip_arch", ""))
            self._stdc.setToolTip(strings.get("tooltip_c_standard", ""))
            self._stdcxx.setToolTip(strings.get("tooltip_cpp_standard", ""))
            self._shared_libs.setToolTip(strings.get("tooltip_shared_libs", ""))
            self._use_ninja.setToolTip(strings.get("tooltip_use_ninja", ""))
            self._cmake_prefix_path.setPlaceholderText(
                strings.get("placeholder_cmake_prefix", "")
            )
            self._cmake_prefix_path.setToolTip(
                strings.get("tooltip_cmake_prefix_path", "")
            )
            self._cmake_args.setPlaceholderText(
                strings.get("placeholder_cmake_args", "")
            )
            self._cmake_args.setToolTip(strings.get("tooltip_extra_cmake_args", ""))
            self._install_prefix.setPlaceholderText(
                strings.get("placeholder_install", "")
            )
            self._install_prefix.setToolTip(strings.get("tooltip_install_prefix", ""))
            idx_c_def = self._compiler_c.findData(None)
            if idx_c_def >= 0:
                self._compiler_c.setItemText(
                    idx_c_def, strings.get("default", "(default)")
                )
            _tip_c = strings.get(
                "tooltip_compiler_c",
                "Absolute path to C compiler. Type or pick from list (found recursively on this machine).",
            )
            _tip_c += "\n\n" + strings.get(
                "tooltip_compiler_default_hint",
                "(default) = system compiler; nothing is passed to CMake (CMake chooses). "
                "Windows: if Visual Studio is installed, system compiler is cl.exe (e.g. C:\\...\\MSVC\\14.xx\\bin\\Hostx64\\x64\\cl.exe). "
                "Linux: gcc (e.g. /usr/bin/gcc).",
            )
            le_c = self._compiler_c.lineEdit()
            if le_c is not None:
                le_c.setPlaceholderText(strings.get("placeholder_compiler_c", ""))
            self._compiler_c.setToolTip(_tip_c)
            idx_cpp_def = self._compiler_cpp.findData(None)
            if idx_cpp_def >= 0:
                self._compiler_cpp.setItemText(
                    idx_cpp_def, strings.get("default", "(default)")
                )
            _tip_cpp = strings.get(
                "tooltip_compiler_cpp",
                "Absolute path to C++ compiler. Type or pick from list (found recursively on this machine).",
            )
            _tip_cpp += "\n\n" + strings.get(
                "tooltip_compiler_default_hint",
                "(default) = system compiler; nothing is passed to CMake (CMake chooses). "
                "Windows: if Visual Studio is installed, system compiler is cl.exe (e.g. C:\\...\\MSVC\\14.xx\\bin\\Hostx64\\x64\\cl.exe). "
                "Linux: gcc (e.g. /usr/bin/gcc).",
            )
            le_cpp = self._compiler_cpp.lineEdit()
            if le_cpp is not None:
                le_cpp.setPlaceholderText(strings.get("placeholder_compiler_cpp", ""))
            self._compiler_cpp.setToolTip(_tip_cpp)
            idx = self._arch.findData(None)
            if idx >= 0:
                self._arch.setItemText(idx, strings.get("auto", "(auto)"))
            idx = self._windows_version.findData(None)
            if idx >= 0:
                self._windows_version.setItemText(
                    idx, strings.get("default", "(default)")
                )
            _tip_wv = strings.get(
                "tooltip_windows_version",
                "When (default) is selected, the program detects the current "
                "Windows version and passes it to CMake (WINDOWS_VERSION / WIN32_WINNT).",
            )
            if platform_system() == "Windows":
                _dw = detect_windows_version()
                if _dw:
                    _tip_wv += "\n" + strings.get(
                        "hint_detected", "Detected: {}."
                    ).format(_dw)
            self._windows_version.setToolTip(_tip_wv)
            idx = self._toolset.findData(None)
            if idx >= 0:
                self._toolset.setItemText(idx, strings.get("default", "(default)"))
            _tip_ts = strings.get(
                "tooltip_msvc_toolset",
                "When (default) is selected, CMake uses the default toolset "
                "of the selected Visual Studio generator (e.g. v143 for VS 2022).",
            )
            if platform_system() == "Windows":
                _dt = detect_msvc_toolset()
                if _dt:
                    _tip_ts += "\n" + strings.get(
                        "hint_detected", "Detected: {}."
                    ).format(_dt)
            self._toolset.setToolTip(_tip_ts)
            _max_p = self._parallel_jobs.maximum()
            self._parallel_jobs.setToolTip(
                strings.get(
                    "tooltip_parallel_jobs",
                    "Number of parallel build jobs (cmake --build --parallel N). "
                    "Range: 1 to {max} (logical CPUs on this machine).",
                ).format(max=_max_p)
            )

        def set_path(self, path: str) -> None:
            self._path.setText(path)

        def get_path(self) -> str:
            return self._path.text().strip() or "."

        def get_options(self) -> dict:
            arch_item = self._arch.currentData()
            windows_version_item = self._windows_version.currentData()
            toolset_item = self._toolset.currentData()
            return {
                "arch": arch_item if arch_item else None,
                "stdc": self._stdc.currentData(),
                "stdcxx": self._stdcxx.currentData(),
                "shared_libs": self._shared_libs.isChecked(),
                "use_ninja": self._use_ninja.isChecked(),
                "parallel_jobs": self._parallel_jobs.value(),
                "cmake_prefix_path": self._cmake_prefix_path.text().strip() or None,
                "cmake_args": self._cmake_args.text().strip() or None,
                "install_prefix": self._install_prefix.text().strip() or None,
                "compiler_c": _compiler_option_value(
                    self._compiler_c, self._tr.get("default", "(default)")
                ),
                "compiler_cpp": _compiler_option_value(
                    self._compiler_cpp, self._tr.get("default", "(default)")
                ),
                "windows_version": (
                    windows_version_item if windows_version_item else None
                ),
                "toolset": toolset_item if toolset_item else None,
            }

        def get_build_type(self) -> str:
            return self._build_type.currentText()

        def save_to_settings(self, settings: "QSettings") -> None:
            """Persist all panel field values to QSettings (e.g. for next run). Path is not stored; it always reflects the current project."""
            prefix = "settings/"
            settings.setValue(prefix + "build_type", self._build_type.currentText())
            arch_data = self._arch.currentData()
            settings.setValue(
                prefix + "arch",
                "auto" if arch_data is None else str(arch_data),
            )
            settings.setValue(prefix + "c_standard", self._stdc.currentData())
            settings.setValue(prefix + "cpp_standard", self._stdcxx.currentData())
            settings.setValue(prefix + "shared_libs", self._shared_libs.isChecked())
            settings.setValue(prefix + "use_ninja", self._use_ninja.isChecked())
            settings.setValue(prefix + "parallel_jobs", self._parallel_jobs.value())
            settings.setValue(
                prefix + "cmake_prefix_path", self._cmake_prefix_path.text().strip()
            )
            settings.setValue(prefix + "cmake_args", self._cmake_args.text().strip())
            settings.setValue(
                prefix + "install_prefix", self._install_prefix.text().strip()
            )
            _def = self._tr.get("default", "(default)")
            _c_text = self._compiler_c.currentText().strip()
            _cpp_text = self._compiler_cpp.currentText().strip()
            settings.setValue(
                prefix + "compiler_c",
                _c_text if _c_text and _c_text != _def else "",
            )
            settings.setValue(
                prefix + "compiler_cpp",
                _cpp_text if _cpp_text and _cpp_text != _def else "",
            )
            wv = self._windows_version.currentData()
            settings.setValue(prefix + "windows_version", "" if wv is None else str(wv))
            ts = self._toolset.currentData()
            settings.setValue(prefix + "msvc_toolset", "" if ts is None else str(ts))

        def load_from_settings(self, settings: "QSettings", project_root: str) -> None:
            """Load panel field values from QSettings; use defaults if key missing. Path is not loaded; it is set from current project_root."""
            prefix = "settings/"
            self.set_path(project_root)
            build_type = settings.value(prefix + "build_type")
            if build_type is not None and isinstance(build_type, str):
                idx = self._build_type.findText(build_type)
                if idx >= 0:
                    self._build_type.setCurrentIndex(idx)
            arch_val = settings.value(prefix + "arch")
            if arch_val is not None and isinstance(arch_val, str):
                if arch_val == "auto":
                    self._arch.setCurrentIndex(self._arch.findData(None))
                else:
                    idx = self._arch.findData(arch_val)
                    if idx >= 0:
                        self._arch.setCurrentIndex(idx)
            c_std = settings.value(prefix + "c_standard")
            if c_std is not None:
                try:
                    i = int(c_std)
                    idx = self._stdc.findData(i)
                    if idx >= 0:
                        self._stdc.setCurrentIndex(idx)
                except (TypeError, ValueError):
                    pass
            cpp_std = settings.value(prefix + "cpp_standard")
            if cpp_std is not None:
                try:
                    i = int(cpp_std)
                    idx = self._stdcxx.findData(i)
                    if idx >= 0:
                        self._stdcxx.setCurrentIndex(idx)
                except (TypeError, ValueError):
                    pass
            shared = settings.value(prefix + "shared_libs")
            if shared is not None:
                self._shared_libs.setChecked(
                    shared is True
                    or (isinstance(shared, str) and shared.lower() == "true")
                )
            ninja = settings.value(prefix + "use_ninja")
            if ninja is not None:
                self._use_ninja.setChecked(
                    ninja is True
                    or (isinstance(ninja, str) and ninja.lower() == "true")
                )
            pj = settings.value(prefix + "parallel_jobs")
            if pj is not None:
                try:
                    v = int(pj)
                    self._parallel_jobs.setValue(
                        max(1, min(v, self._parallel_jobs.maximum()))
                    )
                except (TypeError, ValueError):
                    pass
            for key, line_edit in (
                (prefix + "cmake_prefix_path", self._cmake_prefix_path),
                (prefix + "cmake_args", self._cmake_args),
                (prefix + "install_prefix", self._install_prefix),
            ):
                val = settings.value(key)
                if val is not None and isinstance(val, str):
                    line_edit.setText(val)
            comp_c = settings.value(prefix + "compiler_c")
            if comp_c is not None and isinstance(comp_c, str) and comp_c.strip():
                idx = self._compiler_c.findText(comp_c.strip())
                if idx >= 0:
                    self._compiler_c.setCurrentIndex(idx)
                else:
                    self._compiler_c.setCurrentIndex(0)
                    le = self._compiler_c.lineEdit()
                    if le is not None:
                        le.setText(comp_c.strip())
            else:
                self._compiler_c.setCurrentIndex(0)
                le = self._compiler_c.lineEdit()
                if le is not None:
                    le.clear()
            comp_cpp = settings.value(prefix + "compiler_cpp")
            if comp_cpp is not None and isinstance(comp_cpp, str) and comp_cpp.strip():
                idx = self._compiler_cpp.findText(comp_cpp.strip())
                if idx >= 0:
                    self._compiler_cpp.setCurrentIndex(idx)
                else:
                    self._compiler_cpp.setCurrentIndex(0)
                    le = self._compiler_cpp.lineEdit()
                    if le is not None:
                        le.setText(comp_cpp.strip())
            else:
                self._compiler_cpp.setCurrentIndex(0)
                le = self._compiler_cpp.lineEdit()
                if le is not None:
                    le.clear()
            wv = settings.value(prefix + "windows_version")
            if wv is not None and isinstance(wv, str) and wv.strip():
                idx = self._windows_version.findData(wv.strip())
                if idx >= 0:
                    self._windows_version.setCurrentIndex(idx)
            ts = settings.value(prefix + "msvc_toolset")
            if ts is not None and isinstance(ts, str) and ts.strip():
                idx = self._toolset.findData(ts.strip())
                if idx >= 0:
                    self._toolset.setCurrentIndex(idx)

        def reset_to_defaults(self, project_root: str) -> None:
            """Set all panel fields to their default values (for Reset button)."""
            self.set_path(project_root)
            self._build_type.setCurrentText("Release")
            self._arch.setCurrentIndex(self._arch.findData(None))
            self._stdc.setCurrentIndex(self._stdc.findData(17))
            self._stdcxx.setCurrentIndex(self._stdcxx.findData(20))
            self._shared_libs.setChecked(False)
            self._use_ninja.setChecked(False)
            _max_parallel = self._parallel_jobs.maximum()
            _default_parallel = max(
                1,
                min(psutil_cpu_count(logical=False) or 1, _max_parallel),
            )
            self._parallel_jobs.setValue(_default_parallel)
            self._cmake_prefix_path.clear()
            self._cmake_args.clear()
            self._install_prefix.clear()
            self._compiler_c.setCurrentIndex(0)
            le_c = self._compiler_c.lineEdit()
            if le_c is not None:
                le_c.clear()
            self._compiler_cpp.setCurrentIndex(0)
            le_cpp = self._compiler_cpp.lineEdit()
            if le_cpp is not None:
                le_cpp.clear()
            self._windows_version.setCurrentIndex(self._windows_version.findData(None))
            self._toolset.setCurrentIndex(self._toolset.findData(None))

        def _update_standards_for_compiler(self) -> None:
            """Refill C/C++ standard dropdowns based on the selected compiler (path)."""
            default_lbl = self._tr.get("default", "(default)")
            path_cpp = _compiler_option_value(self._compiler_cpp, default_lbl)
            path_c = _compiler_option_value(self._compiler_c, default_lbl)
            path = path_cpp or path_c
            c_list, cpp_list = get_supported_c_cpp_standards_for_compiler(path, True)
            cur_c = self._stdc.currentData()
            cur_cpp = self._stdcxx.currentData()
            self._stdc.blockSignals(True)
            self._stdcxx.blockSignals(True)
            try:
                self._stdc.clear()
                for std in c_list:
                    self._stdc.addItem("C{}".format(std), std)
                idx_c = self._stdc.findData(cur_c)
                self._stdc.setCurrentIndex(idx_c if idx_c >= 0 else 0)
                self._stdcxx.clear()
                for std in cpp_list:
                    self._stdcxx.addItem("C++{}".format(std), std)
                idx_cpp = self._stdcxx.findData(cur_cpp)
                self._stdcxx.setCurrentIndex(idx_cpp if idx_cpp >= 0 else 0)
            finally:
                self._stdc.blockSignals(False)
                self._stdcxx.blockSignals(False)

        def _update_compiler_combo_tooltip(self, is_c: bool) -> None:
            """Set combo tooltip to full path when a compiler path is selected; else default tip."""
            combo = self._compiler_c if is_c else self._compiler_cpp
            default_tip = (
                self._compiler_c_default_tip if is_c else self._compiler_cpp_default_tip
            )
            default_lbl = self._tr.get("default", "(default)")
            text = (combo.currentText() or "").strip()
            if not text or (combo.currentIndex() == 0 and text == default_lbl):
                combo.setToolTip(default_tip)
            else:
                combo.setToolTip(text)

        def set_compiler_paths(self, c_paths: List[str], cpp_paths: List[str]) -> None:
            """Fill C/C++ compiler dropdowns with discovered paths (called from discovery thread signal).
            Each item gets a tooltip with the full path so truncated entries show full path on hover.
            """
            for path in c_paths:
                if self._compiler_c.findText(path) < 0:
                    self._compiler_c.addItem(path)
                    self._compiler_c.setItemData(
                        self._compiler_c.count() - 1, path, Qt.ToolTipRole  # type: ignore
                    )
            for path in cpp_paths:
                if self._compiler_cpp.findText(path) < 0:
                    self._compiler_cpp.addItem(path)
                    self._compiler_cpp.setItemData(
                        self._compiler_cpp.count() - 1, path, Qt.ToolTipRole  # type: ignore
                    )

    # Minimal dock title bar style so the drag handle is always visible (even with Default theme)
    _DOCK_TITLE_STYLE = (
        "QDockWidget::title { background: #e0e0e0; color: #333; padding: 6px 8px; "
        "font-weight: bold; min-height: 14px; }"
        " QHeaderView::section { background: #e8e8e8; color: #333; padding: 4px 8px; border: 1px solid #ccc; }"
        " QTabWidget::pane { border: 1px solid #ccc; background: #f5f5f5; top: -1px; }"
        " QTabBar::tab { background: #e8e8e8; color: #333; padding: 6px 12px; margin-right: 2px; border: 1px solid #ccc; border-bottom: none; }"
        " QTabBar::tab:selected { background: #f5f5f5; }"
        " QTableWidget { background: #fff; color: #333; gridline-color: #ddd; }"
        " QTreeView { background: #fff; color: #333; }"
    )

    _CMAKE_DIR_CACHE: Dict[str, bool] = {}
    _CMAKE_CACHE_LOADED: List[bool] = [False]
    _CMAKE_CACHE_MAX_ENTRIES = 10000
    _CMAKE_CACHE_SETTINGS_KEY = "cmake_dir_cache"

    def _load_cmake_cache_from_settings() -> None:
        """Load CMake dir cache from QSettings (persisted across runs)."""
        try:
            _s = QSettings("CMakeBuildGUI", "compile")  # type: ignore[name-defined]
            data = _s.value(_CMAKE_CACHE_SETTINGS_KEY)
            if isinstance(data, dict):
                for k, v in data.items():
                    if isinstance(k, str) and isinstance(v, bool):
                        _CMAKE_DIR_CACHE[k] = v
            elif isinstance(data, str) and data:
                try:
                    loaded = json_loads(data)
                    if isinstance(loaded, dict):
                        for k, v in loaded.items():
                            if isinstance(k, str) and isinstance(v, bool):
                                _CMAKE_DIR_CACHE[k] = v
                except (ValueError, TypeError):
                    pass
        except Exception:
            pass

    def _save_cmake_cache_to_settings() -> None:
        """Persist CMake dir cache to QSettings (for fast startup on next run)."""
        try:
            _s = QSettings("CMakeBuildGUI", "compile")  # type: ignore[name-defined]
            keys_to_save = list(_CMAKE_DIR_CACHE.keys())[-_CMAKE_CACHE_MAX_ENTRIES:]
            data = {
                k: _CMAKE_DIR_CACHE[k] for k in keys_to_save if k in _CMAKE_DIR_CACHE
            }
            _s.setValue(_CMAKE_CACHE_SETTINGS_KEY, json_dumps(data))
            _s.sync()
        except Exception:
            pass

    def _scan_subtree_for_cmake(path: str) -> Dict[str, bool]:
        """Scan directory subtree; return path->bool for each dir (contains CMakeLists at any depth). Uses os.scandir for speed."""
        path = os_path_abspath(path)
        result: Dict[str, bool] = {}
        cmake_path = os_path_join(path, "CMakeLists.txt")
        if os_path_exists(cmake_path):
            result[path] = True
            return result
        subdirs: List[str] = []
        try:
            with os_scandir(path) as it:
                for entry in it:
                    if entry.is_dir(follow_symlinks=False):
                        subdirs.append(os_path_abspath(entry.path))
        except OSError:
            result[path] = False
            return result
        subdirs.sort()
        for sub in subdirs:
            sub_res = _scan_subtree_for_cmake(sub)
            result.update(sub_res)
        result[path] = any(result.get(sub, False) for sub in subdirs)
        return result

    def _populate_cmake_cache(project_root: str) -> None:
        """Populate _CMAKE_DIR_CACHE by scanning subdirs in parallel. Skips if already cached."""
        if not _CMAKE_CACHE_LOADED[0]:
            _load_cmake_cache_from_settings()
            _CMAKE_CACHE_LOADED[0] = True
        project_root = os_path_abspath(project_root)
        if not os_path_isdir(project_root):
            return
        subdirs: List[str] = []
        try:
            with os_scandir(project_root) as it:
                for entry in it:
                    if entry.is_dir(follow_symlinks=False):
                        subdirs.append(os_path_abspath(entry.path))
        except OSError:
            return
        subdirs.sort()
        if project_root in _CMAKE_DIR_CACHE and all(
            d in _CMAKE_DIR_CACHE for d in subdirs
        ):
            return
        if os_path_exists(os_path_join(project_root, "CMakeLists.txt")):
            _CMAKE_DIR_CACHE[project_root] = True
        max_workers = min(max(len(subdirs), 1), 64)
        with ThreadPoolExecutor(max_workers=max_workers) as executor:
            futures = {executor.submit(_scan_subtree_for_cmake, d): d for d in subdirs}
            for future in as_completed(futures):
                try:
                    sub_result = future.result()
                    for k, v in sub_result.items():
                        _CMAKE_DIR_CACHE[k] = v
                except Exception:
                    pass
        if project_root not in _CMAKE_DIR_CACHE:
            _CMAKE_DIR_CACHE[project_root] = any(
                _CMAKE_DIR_CACHE.get(d, False) for d in subdirs
            )
        excess = len(_CMAKE_DIR_CACHE) - _CMAKE_CACHE_MAX_ENTRIES
        if excess > 0:
            keys_to_drop = list(_CMAKE_DIR_CACHE.keys())[: min(excess, 500)]
            for k in keys_to_drop:
                _CMAKE_DIR_CACHE.pop(k, None)
        _save_cmake_cache_to_settings()

    def _dir_contains_cmake(path: str) -> bool:
        """Check if directory contains CMakeLists.txt at any depth (uses cache)."""
        key = os_path_abspath(path)
        if key in _CMAKE_DIR_CACHE:
            return _CMAKE_DIR_CACHE[key]
        direct = os_path_exists(os_path_join(path, "CMakeLists.txt"))
        if direct:
            _CMAKE_DIR_CACHE[key] = True
            return True
        try:
            for name in os_listdir(path):
                full = os_path_join(path, name)
                if os_path_isdir(full) and _dir_contains_cmake(full):
                    _CMAKE_DIR_CACHE[key] = True
                    return True
        except OSError:
            pass
        _CMAKE_DIR_CACHE[key] = False
        return False

    def _build_cmake_only_tree_model(project_root: str) -> "QStandardItemModel":
        """Build a tree model showing only directories that contain CMakeLists.txt at any depth."""
        project_root = os_path_abspath(project_root)
        _populate_cmake_cache(project_root)
        user_role = getattr(Qt, "UserRole", 32)
        model = QStandardItemModel()  # pyright: ignore[reportPossiblyUnboundVariable]
        root_name = os_path_basename(project_root) or project_root
        root_item = QStandardItem(  # type: ignore
            root_name
        )  # pyright: ignore[reportPossiblyUnboundVariable]
        root_item.setData(project_root, user_role)
        model.appendRow(root_item)

        def add_children(
            parent_path: str,
            parent_item: QStandardItem,  # pyright: ignore[reportPossiblyUnboundVariable]
        ) -> None:
            try:
                names = sorted(os_listdir(parent_path))
            except OSError:
                return
            for name in names:
                full = os_path_join(parent_path, name)
                if os_path_isdir(full) and _dir_contains_cmake(full):
                    dir_item = QStandardItem(  # type: ignore
                        name
                    )  # pyright: ignore[reportPossiblyUnboundVariable]
                    dir_item.setData(full, user_role)
                    parent_item.appendRow(  # type: ignore
                        dir_item
                    )  # pyright: ignore[reportPossiblyUnboundVariable]
                    cmake_path = os_path_join(full, "CMakeLists.txt")
                    if os_path_exists(cmake_path):
                        cmake_item = QStandardItem(  # type: ignore
                            "CMakeLists.txt"
                        )  # pyright: ignore[reportPossiblyUnboundVariable]
                        cmake_item.setData(cmake_path, user_role)
                        dir_item.appendRow(cmake_item)
                    add_children(full, dir_item)

        add_children(project_root, root_item)
        return model

    class _ProjectTreeWidget(QWidget):
        """Left dock widget: tree of project (CMakeLists-only by default, or full files via checkbox)."""

        def __init__(
            self,
            project_root: str,
            tr_dict: dict,
            on_open_project: Optional[Callable[[str], None]] = None,
        ) -> None:
            super().__init__()
            self._project_root = os_path_abspath(project_root or ".")
            self._tr = tr_dict
            self._on_open_project = on_open_project
            layout = QVBoxLayout(self)
            top_row = QHBoxLayout()
            self._parent_btn = QPushButton("..")
            self._parent_btn.setFixedSize(24, 24)
            self._parent_btn.setToolTip(
                tr_dict.get("go_to_parent", "Go to parent directory")
            )
            self._parent_btn.clicked.connect(self._on_parent_clicked)
            top_row.addWidget(self._parent_btn)
            self._show_all_cb = QCheckBox(
                tr_dict.get("show_all_files", "Show all files")
            )
            self._show_all_cb.setChecked(False)
            self._show_all_cb.stateChanged.connect(self._on_show_all_changed)
            top_row.addWidget(self._show_all_cb)
            top_row.addStretch()
            layout.addLayout(top_row)
            self._tree = QTreeView()  # pyright: ignore[reportPossiblyUnboundVariable]
            self._tree.setHeaderHidden(True)
            self._tree.setHorizontalScrollMode(  # pyright: ignore[reportPossiblyUnboundVariable]
                QTreeView.ScrollPerPixel  # pyright: ignore[reportPossiblyUnboundVariable]
            )
            self._tree.setMinimumWidth(220)
            # Do not call setSectionResizeMode while header is hidden: Qt can crash
            # (QHeaderViewPrivate::createSectionItems). Resize mode is set when
            # "Show all files" is enabled in _on_show_all_changed.
            self._cmake_model = _build_cmake_only_tree_model(self._project_root)
            self._fs_model = (
                QFileSystemModel()  # type: ignore
            )  # pyright: ignore[reportPossiblyUnboundVariable]
            self._fs_model.setRootPath(self._project_root)
            self._tree.setModel(self._cmake_model)
            self._tree.resizeColumnToContents(0)
            self._tree.expandToDepth(1)
            self._tree.doubleClicked.connect(self._on_item_double_clicked)
            layout.addWidget(self._tree)

        def _on_parent_clicked(self) -> None:
            if self._on_open_project is None:
                return
            parent = os_path_dirname(self._project_root)
            if parent and parent != self._project_root:
                self._on_open_project(parent)

        def _on_item_double_clicked(self, index: Any) -> None:
            if self._on_open_project is None:
                return
            user_role = getattr(Qt, "UserRole", 32)
            model = self._tree.model()
            if model is None:
                return
            if self._show_all_cb.isChecked():
                path = self._fs_model.filePath(index)
            else:
                item = self._cmake_model.itemFromIndex(index)
                if item is None:
                    return
                path = item.data(user_role)
            if not path or not isinstance(path, str):
                return
            path = os_path_abspath(path)
            if os_path_basename(path) == "CMakeLists.txt" and os_path_isfile(path):
                proot = os_path_dirname(path)
                if os_path_exists(os_path_join(proot, "CMakeLists.txt")):
                    self._on_open_project(proot)
            elif os_path_isdir(path):
                self._on_open_project(path)

        def _on_show_all_changed(self, state: int) -> None:
            checked = state == getattr(Qt, "Checked", 2)
            if checked:
                self._tree.setHeaderHidden(False)
                self._tree.setModel(self._fs_model)
                self._tree.setRootIndex(self._fs_model.index(self._project_root))
                self._tree.expandToDepth(0)
                hdr = self._tree.header()
                if hdr is not None:
                    hdr.setSectionResizeMode(0, QHeaderView.ResizeToContents)
                self._tree.resizeColumnToContents(0)
            else:
                self._tree.setHeaderHidden(True)
                self._tree.setModel(self._cmake_model)
                self._tree.resizeColumnToContents(0)
                self._tree.expandToDepth(1)

        def set_root(self, project_root: str) -> None:
            self._project_root = os_path_abspath(project_root or ".")
            self._cmake_model = _build_cmake_only_tree_model(self._project_root)
            self._fs_model.setRootPath(self._project_root)
            if self._show_all_cb.isChecked():
                self._tree.setModel(self._fs_model)
                self._tree.setRootIndex(self._fs_model.index(self._project_root))
                hdr = self._tree.header()
                if hdr is not None:
                    hdr.setSectionResizeMode(0, QHeaderView.ResizeToContents)
                self._tree.resizeColumnToContents(0)
            else:
                self._tree.setModel(self._cmake_model)
                self._tree.resizeColumnToContents(0)
            self._tree.expandToDepth(1)

        def apply_ui_strings(self, tr_dict: dict) -> None:
            self._tr = tr_dict
            self._show_all_cb.setText(tr_dict.get("show_all_files", "Show all files"))
            self._parent_btn.setToolTip(
                tr_dict.get("go_to_parent", "Go to parent directory")
            )

    class _CompileMainWindow(QMainWindow):
        def __init__(self, project_root: str, debug_trycompile: bool = False) -> None:
            super().__init__()
            self._debug_trycompile = debug_trycompile
            self._project_root = os_path_abspath(
                os_path_expanduser(project_root) if project_root else "."
            )
            self._log_bridge = _LogBridge()
            self._worker = None
            self._current_worker = None
            self._flags_loader_thread = None
            self._flags_loader_worker = None
            self._compiler_discovery_thread = None
            self._compiler_discovery_worker = None
            self._lang = "en"
            _pre_settings = QSettings("CMakeBuildGUI", "compile")  # type: ignore
            _saved_lang = _pre_settings.value("lang")
            if _saved_lang in _UI_STRINGS:
                self._lang = _saved_lang
            self._current_theme_key = None
            self._initial_state = None
            self._initial_geometry = None
            _tr = _UI_STRINGS[self._lang]
            self.setWindowTitle(
                "{} - {}".format(
                    _tr.get("window_title", "CMake Build GUI"), self._project_root
                )
            )
            self.setMinimumSize(800, 600)
            # Menu bar: Project, View (themes), Help (about)
            menubar = self.menuBar()
            if menubar is not None:
                self._project_menu = menubar.addMenu(_tr.get("project", "Project"))
                self._open_act = QAction(_tr.get("open", "Open"), self)
                self._open_act.triggered.connect(self._on_project_open)
                if self._project_menu is not None:
                    self._project_menu.addAction(self._open_act)
                self._view_menu = menubar.addMenu(_tr.get("view", "View"))
                self._theme_submenu = (
                    self._view_menu.addMenu(_tr.get("theme_submenu", "Theme"))
                    if self._view_menu is not None
                    else None
                )
                self._help_menu = menubar.addMenu(_tr.get("help", "Help"))
                help_menu = self._help_menu
            else:
                self._project_menu = None
                self._open_act = None
                self._view_menu = None
                self._theme_submenu = None
                help_menu = None
                self._help_menu = None
            if help_menu is not None:
                self._about_act = QAction(_tr.get("about", "About"), self)
                self._about_act.triggered.connect(self._on_about)
                help_menu.addAction(self._about_act)
                help_menu.addSeparator()
                self._shortcuts_act = QAction(
                    _tr.get("shortcuts_menu", "Shortcuts…"), self
                )
                self._shortcuts_act.triggered.connect(self._on_shortcuts_dialog)
                help_menu.addAction(self._shortcuts_act)
            else:
                self._about_act = None
                self._shortcuts_act = None
            self._theme_actions = []
            _theme_key_map = {
                "Default": "theme_default",
                "Dark": "theme_dark",
                "Green": "theme_green",
                "Blue": "theme_blue",
                "Ocean": "theme_ocean",
                "Amber": "theme_amber",
                "Purple": "theme_purple",
                "High Contrast": "theme_high_contrast",
                "Forest": "theme_forest",
                "Midnight": "theme_midnight",
            }
            for theme_name, theme_id in _GUI_THEMES:
                key = _theme_key_map.get(theme_name) or theme_name.lower()
                label = _tr.get(key, theme_name)
                act = QAction(label, self)
                act.triggered.connect(
                    lambda checked=False, t=theme_id: self._apply_theme(t)
                )
                if self._theme_submenu is not None:
                    self._theme_submenu.addAction(act)
                self._theme_actions.append((theme_name, act))
            # Toolbar: Reset + Lang
            toolbar = QToolBar()
            toolbar.setObjectName("MainToolbar")
            self._reset_act = QAction(_tr.get("reset", "Reset"), self)
            self._reset_act.setToolTip(_tr.get("reset_tooltip", ""))
            self._reset_act.triggered.connect(self._on_reset_layout)
            toolbar.addAction(self._reset_act)
            toolbar.addSeparator()
            self._lang_label = QLabel(_tr.get("lang", "Lang") + ": ")
            toolbar.addWidget(self._lang_label)
            self._lang_combo = QComboBox()
            self._lang_combo.addItem("English", "en")
            self._lang_combo.addItem("Русский", "ru")
            self._lang_combo.setCurrentIndex(self._lang_combo.findData(self._lang))
            self._lang_combo.currentIndexChanged.connect(self._on_lang_changed)
            toolbar.addWidget(self._lang_combo)
            self.addToolBar(toolbar)
            central = QWidget()
            main_layout = QVBoxLayout(central)
            self._tabs = QTabWidget()
            _has_cmake = os_path_exists(
                os_path_join(self._project_root, "CMakeLists.txt")
            )
            if _has_cmake:
                self._flags_loading_widget = _make_flags_loading_widget(_tr)
                self._tabs.addTab(self._flags_loading_widget, _tr.get("flags", "Flags"))
                QTimer.singleShot(0, self._start_flags_loader)
            else:
                self._flags_loading_widget = None
                self._tabs.addTab(
                    _make_flags_no_project_widget(_tr),
                    _tr.get("flags", "Flags"),
                )
            self._clangd_tab = _ClangdTab(
                self._project_root,
                self._append_output,
                debug_trycompile=self._debug_trycompile,
            )
            self._tabs.addTab(self._clangd_tab, _tr.get("intellisense", "IntelliSense"))
            self._flags_tab = None
            QTimer.singleShot(200, self._start_compiler_discovery)
            main_layout.addWidget(self._tabs)
            self._settings_group = QGroupBox("")
            settings_layout = QVBoxLayout(self._settings_group)
            settings_layout.setContentsMargins(6, 10, 6, 6)
            self._settings = _SettingsPanel(_UI_STRINGS[self._lang])
            self._settings.set_path(self._project_root)
            self._settings._use_ninja.toggled.connect(self._on_use_ninja_toggled)
            scroll = QScrollArea()
            scroll.setWidget(self._settings)
            scroll.setWidgetResizable(True)
            _wheel_filter = _ScrollChildWheelFilter(scroll)
            for _w in list(self._settings.findChildren(QComboBox)) + list(
                self._settings.findChildren(QSpinBox)  # type: ignore
            ):
                _w.installEventFilter(_wheel_filter)
            settings_layout.addWidget(scroll)
            self._settings_group.setMinimumWidth(220)
            self._settings_group.setMinimumHeight(180)
            self._performance_group = QGroupBox("")
            perf_layout = QVBoxLayout(self._performance_group)
            perf_layout.setContentsMargins(6, 10, 6, 6)
            self._build_monitor = _BuildMonitorWidget(
                lambda: self._current_worker is not None
                and self._worker is not None
                and self._worker.isRunning(),
                _UI_STRINGS[self._lang],
            )
            perf_layout.addWidget(self._build_monitor)
            self._performance_group.setMinimumWidth(200)
            self._performance_group.setMinimumHeight(180)
            btn_layout = QHBoxLayout()
            self._clean_btn = QPushButton(_tr.get("clean", "Clean"))
            self._clean_btn.clicked.connect(self._on_clean)
            self._configure_btn = QPushButton(_tr.get("configure", "Configure"))
            self._configure_btn.clicked.connect(self._on_configure)
            self._build_btn = QPushButton(_tr.get("build", "Build"))
            self._build_btn.setEnabled(False)  # Enabled only after Configure
            self._build_btn.clicked.connect(self._on_build)
            self._stop_btn = QPushButton(_tr.get("stop", "Stop"))
            self._stop_btn.clicked.connect(self._on_stop)
            self._stop_btn.setEnabled(False)
            btn_layout.addWidget(self._clean_btn)
            btn_layout.addWidget(self._configure_btn)
            btn_layout.addWidget(self._build_btn)
            btn_layout.addWidget(self._stop_btn)
            btn_layout.addStretch()
            main_layout.addLayout(btn_layout)
            self.setCentralWidget(central)
            self._output_plain = QPlainTextEdit()
            self._output_plain.setReadOnly(True)
            out_font = QFont()
            if hasattr(out_font, "setFamilies"):
                out_font.setFamilies(
                    [
                        "DejaVu Sans Mono",
                        "Liberation Mono",
                        "Noto Color Emoji",
                        "Symbola",
                        "monospace",
                    ]
                )
            else:
                out_font.setFamily("DejaVu Sans Mono")
            out_font.setPointSize(9)
            self._output_plain.setFont(out_font)
            self._output_plain.setMinimumHeight(120)
            self._output_dock = QDockWidget(_tr.get("output", "Output"), self)
            self._output_dock.setObjectName("OutputDock")
            output_container = QWidget()
            output_container_layout = QVBoxLayout(output_container)
            output_container_layout.setContentsMargins(6, 10, 6, 6)
            output_toolbar_layout = QHBoxLayout()
            self._clear_logs_btn = QPushButton(_tr.get("clear_logs", "Clear logs"))
            self._clear_logs_btn.clicked.connect(self._on_clear_logs)
            output_toolbar_layout.addWidget(self._clear_logs_btn)
            output_toolbar_layout.addStretch()
            output_container_layout.addLayout(output_toolbar_layout)
            search_layout = QHBoxLayout()
            self._search_line_edit = QLineEdit()
            self._search_line_edit.setPlaceholderText(
                _tr.get("search_placeholder", "Search (Ctrl+F)...")
            )
            self._search_regex_checkbox = QCheckBox(_tr.get("regex", "Regex"))
            self._search_case_checkbox = QCheckBox(
                _tr.get("case_sensitive", "Case sensitive")
            )
            self._search_whole_word_checkbox = QCheckBox(
                _tr.get("whole_word", "Whole word")
            )
            self._search_count_label = QLabel("0/0")
            self._search_count_label.setMinimumWidth(36)
            self._search_match_positions: List[int] = []
            self._search_current_index = 0
            self._search_label = QLabel(_tr.get("find", "Find:"))
            search_layout.addWidget(self._search_label)
            search_layout.addWidget(self._search_line_edit)
            search_layout.addWidget(self._search_regex_checkbox)
            search_layout.addWidget(self._search_case_checkbox)
            search_layout.addWidget(self._search_whole_word_checkbox)
            search_layout.addWidget(self._search_count_label)
            self._search_line_edit.returnPressed.connect(self._on_search_enter)
            output_container_layout.addLayout(search_layout)
            output_container_layout.addWidget(self._output_plain)
            self._output_dock.setWidget(output_container)
            self._append_busy = False
            self._append_pending: List[str] = []
            self._flush_scheduled = False
            self._configured_generator: Optional[str] = None
            self._build_disabled_by_generator_mismatch = False
            _dock_features = (
                QDockWidget.DockWidgetClosable
                | QDockWidget.DockWidgetMovable
                | QDockWidget.DockWidgetFloatable
            )
            _all_areas = getattr(Qt, "AllDockWidgetAreas", 15)
            self._output_dock.setFeatures(_dock_features)
            self._output_dock.setAllowedAreas(_all_areas)  # type: ignore[arg-type]
            bottom_area = getattr(Qt, "BottomDockWidgetArea", 8)
            self.addDockWidget(bottom_area, self._output_dock)  # type: ignore[arg-type]
            left_area = getattr(Qt, "LeftDockWidgetArea", 1)
            right_area = getattr(Qt, "RightDockWidgetArea", 2)
            self._project_tree_widget = _ProjectTreeWidget(
                self._project_root,
                _tr,
                on_open_project=self._set_project_root,
            )
            self._project_dock = QDockWidget(_tr.get("project_tree", "Project"), self)
            self._project_dock.setObjectName("ProjectDock")
            self._project_dock.setWidget(self._project_tree_widget)
            self._project_dock.setFeatures(_dock_features)
            self._project_dock.setAllowedAreas(_all_areas)  # type: ignore[arg-type]
            self.addDockWidget(left_area, self._project_dock)  # type: ignore[arg-type]
            self._project_dock.setMinimumWidth(260)
            self._settings_dock = QDockWidget(_tr.get("settings", "Settings"), self)
            self._settings_dock.setObjectName("SettingsDock")
            self._settings_dock.setWidget(self._settings_group)
            self._settings_dock.setFeatures(_dock_features)
            self._settings_dock.setAllowedAreas(_all_areas)  # type: ignore[arg-type]
            self.addDockWidget(right_area, self._settings_dock)  # type: ignore[arg-type]
            self._performance_dock = QDockWidget(
                _tr.get("performance", "Performance"), self
            )
            self._performance_dock.setObjectName("PerformanceDock")
            self._performance_dock.setWidget(self._performance_group)
            self._performance_dock.setFeatures(_dock_features)
            self._performance_dock.setAllowedAreas(_all_areas)  # type: ignore[arg-type]
            self.addDockWidget(right_area, self._performance_dock)  # type: ignore[arg-type]
            self.splitDockWidget(
                self._settings_dock, self._performance_dock, Qt.Vertical  # type: ignore[attr-defined]
            )
            self._log_bridge.log_text.connect(self._on_log_text)
            _settings_obj = QSettings(  # type: ignore
                "CMakeBuildGUI", "compile"
            )  # pyright: ignore[reportPossiblyUnboundVariable]
            _saved_theme = _settings_obj.value("theme")
            if (
                _saved_theme is not None
                and isinstance(_saved_theme, str)
                and _saved_theme.strip()
            ):
                self._apply_theme(_saved_theme.strip())
            else:
                self._apply_theme(None)
            _geom = _settings_obj.value("main_window/geometry")
            _state = _settings_obj.value("main_window/state")
            if _geom is not None and _state is not None:
                self.restoreGeometry(_geom)
                self.restoreState(_state)
                self.show()
            else:
                screen = QApplication.primaryScreen()
                if screen:
                    g = screen.availableGeometry()
                    self.resize(g.width(), g.height())
                    self.showMaximized()
                else:
                    self.show()
            self._settings.load_from_settings(_settings_obj, self._project_root)
            _show_all = _settings_obj.value("project_tree/show_all_files")
            if _show_all is not None and _show_all in (True, "true", "1"):
                self._project_tree_widget._show_all_cb.setChecked(True)
                _state = getattr(Qt, "Checked", 2)
                self._project_tree_widget._on_show_all_changed(_state)
            QTimer.singleShot(100, self._save_initial_layout)
            self._shortcuts: List[Any] = []
            self._setup_shortcuts()
            self._search_line_edit.textChanged.connect(self._apply_search_highlights)
            self._search_regex_checkbox.toggled.connect(self._apply_search_highlights)
            self._search_case_checkbox.toggled.connect(self._apply_search_highlights)
            self._search_whole_word_checkbox.toggled.connect(
                self._apply_search_highlights
            )

        def _start_flags_loader(self) -> None:
            """Start loading CMake options in a background thread so the loading animation stays visible."""
            self._flags_loader_thread = QThread()
            self._flags_loader_worker = _FlagsLoaderWorker(
                self._project_root, self._build_flags_refresh_cmake_args()
            )
            self._flags_loader_worker.moveToThread(self._flags_loader_thread)
            self._flags_loader_thread.started.connect(self._flags_loader_worker.run)
            self._flags_loader_worker.options_ready.connect(self._on_flags_loaded)
            self._flags_loader_thread.start()

        def _build_flags_refresh_cmake_args(self) -> List[str]:
            """
            Build extra CMake args for Flags refresh.
            Uses currently selected compilers to make cmake -LH refresh consistent
            with UI toolchain selection.
            """
            args: List[str] = []
            opts = self._settings.get_options()
            compiler_c = opts.get("compiler_c")
            compiler_cpp = opts.get("compiler_cpp")
            if compiler_c:
                args.append("-DCMAKE_C_COMPILER={}".format(compiler_c))
            if compiler_cpp:
                args.append("-DCMAKE_CXX_COMPILER={}".format(compiler_cpp))
            if opts.get("use_ninja"):
                args.extend(["-G", "Ninja"])
            return args

        def _start_compiler_discovery(self) -> None:
            """Start discovering C/C++ compilers in a background thread and fill dropdowns when done."""
            if self._compiler_discovery_thread is not None:
                return
            self._compiler_discovery_thread = QThread()
            self._compiler_discovery_worker = _CompilerDiscoveryWorker()
            self._compiler_discovery_worker.moveToThread(
                self._compiler_discovery_thread
            )
            self._compiler_discovery_thread.started.connect(
                self._compiler_discovery_worker.run
            )
            self._compiler_discovery_worker.compilers_ready.connect(
                self._on_compilers_discovered
            )
            self._compiler_discovery_thread.start()

        def _on_compilers_discovered(
            self, c_paths: List[str], cpp_paths: List[str]
        ) -> None:
            """Fill C/C++ compiler dropdowns with discovered paths (runs in main thread)."""
            if self._compiler_discovery_thread is not None:
                self._compiler_discovery_thread.quit()
                self._compiler_discovery_thread.wait(2000)
                self._compiler_discovery_thread = None
            self._compiler_discovery_worker = None
            self._settings.set_compiler_paths(c_paths, cpp_paths)

        def _on_flags_loaded(self, options: list, probe_error: str) -> None:
            """Replace loading widget with Flags tab when options are ready."""
            if (
                self._flags_loader_thread is not None
                and self._flags_loader_thread.isRunning()
            ):
                self._flags_loader_thread.quit()
                self._flags_loader_thread.wait(2000)
            self._flags_tab = _FlagsTab(self._project_root, options, probe_error)
            self._flags_tab.refresh_requested.connect(self._reload_flags_tab)
            self._tabs.removeTab(0)
            _tr = _UI_STRINGS[self._lang]
            self._tabs.insertTab(0, self._flags_tab, _tr.get("flags", "Flags"))
            self._tabs.setCurrentIndex(0)
            self._flags_tab.apply_ui_strings(_tr)

        def closeEvent(self, a0: Any) -> None:
            """Confirm exit; if configure/build is running, warn and terminate it."""
            event = a0
            _tr = _UI_STRINGS[self._lang]
            title = _tr.get("exit_confirm_title", "Exit")
            msg = _tr.get("exit_confirm_message", "Do you really want to exit?")
            if self._worker is not None and self._worker.isRunning():
                msg = _tr.get(
                    "exit_confirm_with_task",
                    "A configuration or build is currently running. It will be "
                    "terminated.\n\nDo you really want to exit?",
                )
            reply = QMessageBox.question(
                self,
                title,
                msg,
                QMessageBox.Yes | QMessageBox.No,
                QMessageBox.No,
            )
            if reply == QMessageBox.Yes:
                if self._worker is not None and self._worker.isRunning():
                    self._worker.terminate()
                    if not self._worker.wait(3000):
                        self._worker.terminate()
                        self._worker.wait(1000)
                if (
                    self._compiler_discovery_thread is not None
                    and self._compiler_discovery_thread.isRunning()
                ):
                    self._compiler_discovery_thread.quit()
                    self._compiler_discovery_thread.wait(1000)
                _s = QSettings("CMakeBuildGUI", "compile")  # type: ignore
                _s.setValue("lang", self._lang)
                _s.setValue("main_window/geometry", self.saveGeometry())
                _s.setValue("main_window/state", self.saveState())
                _s.setValue("last_project_directory", self._project_root)
                _s.setValue(
                    "project_tree/show_all_files",
                    self._project_tree_widget._show_all_cb.isChecked(),
                )
                self._settings.save_to_settings(_s)
                if self._flags_tab is not None:
                    self._flags_tab.save_state(_s)
                _save_cmake_cache_to_settings()
                event.accept()
            else:
                event.ignore()

        def showEvent(self, event: Any) -> None:  # type: ignore
            """Ensure output dock gets 1/3 of window height on first show."""
            super().showEvent(event)  # type: ignore[misc]
            QTimer.singleShot(50, self._set_output_dock_height)
            QTimer.singleShot(50, self._update_build_button_state)

        def _set_output_dock_height(self) -> None:
            """Set output dock height to 1/3 of main window (only affects current session)."""
            if not self._output_dock or not self._output_dock.isVisible():
                return
            h = self.height()
            if h <= 0:
                return
            dock_height = max(120, h // 3)
            self.resizeDocks(  # type: ignore[attr-defined]
                [self._output_dock], [dock_height], Qt.Vertical  # type: ignore[attr-defined]
            )

        def _save_initial_layout(self) -> None:
            """Persist default layout for Reset; set _initial_state/_initial_geometry."""
            _s = QSettings("CMakeBuildGUI", "compile")  # type: ignore
            if _s.contains("main_window/default_geometry"):
                self._initial_geometry = _s.value("main_window/default_geometry")
                self._initial_state = _s.value("main_window/default_state")
            else:
                self._initial_geometry = self.saveGeometry()
                self._initial_state = self.saveState()
                _s.setValue("main_window/default_geometry", self._initial_geometry)
                _s.setValue("main_window/default_state", self._initial_state)

        def _apply_theme(self, theme_id: Optional[str]) -> None:
            app = QApplication.instance()
            if app is None or not hasattr(app, "setStyleSheet"):
                return
            self._current_theme_key = theme_id
            if theme_id is None:
                app.setStyleSheet(_DOCK_TITLE_STYLE)  # type: ignore[union-attr]
            else:
                sheet = _GUI_THEME_STYLESHEETS.get(theme_id)
                if sheet:
                    app.setStyleSheet(sheet)  # type: ignore[union-attr]
                else:
                    app.setStyleSheet(_DOCK_TITLE_STYLE)  # type: ignore[union-attr]
            _settings_obj = QSettings(  # type: ignore
                "CMakeBuildGUI", "compile"
            )  # pyright: ignore[reportPossiblyUnboundVariable]
            _settings_obj.setValue("theme", theme_id if theme_id else "")

        def _on_project_open(self) -> None:
            _tr = _UI_STRINGS[self._lang]
            current = self._settings.get_path() or self._project_root
            dir_path = QFileDialog.getExistingDirectory(  # pyright: ignore[reportPossiblyUnboundVariable]
                self,
                _tr.get("open_folder_title", "Open project folder"),
                current,
                QFileDialog.ShowDirsOnly  # pyright: ignore[reportPossiblyUnboundVariable]
                | QFileDialog.DontResolveSymlinks,  # pyright: ignore[reportPossiblyUnboundVariable]
            )
            if not dir_path:
                return
            dir_path = os_path_abspath(dir_path)
            if not os_path_exists(os_path_join(dir_path, "CMakeLists.txt")):
                QMessageBox.warning(
                    self,
                    _tr.get("open_folder_title", "Open project folder"),
                    _tr.get(
                        "no_cmake_in_folder",
                        "The selected folder does not contain CMakeLists.txt.",
                    ),
                )
                return
            self._set_project_root(dir_path)

        def _set_project_root(self, path: str) -> None:
            path = os_path_abspath(path)
            self._project_root = path
            self._settings.set_path(path)
            try:
                _s = QSettings("CMakeBuildGUI", "compile")  # type: ignore
                _s.setValue("last_project_directory", path)
            except Exception:
                pass
            _tr = _UI_STRINGS[self._lang]
            self.setWindowTitle(
                "{} - {}".format(_tr.get("window_title", "CMake Build GUI"), path)
            )
            self._project_tree_widget.set_root(path)
            self._clangd_tab.set_project_root(path)
            self._reload_flags_tab()
            self._update_build_button_state()

        def _reload_flags_tab(self) -> None:
            """Replace Flags tab with loading widget and start loader for current _project_root."""
            _tr = _UI_STRINGS[self._lang]
            self._tabs.removeTab(0)
            self._flags_loading_widget = _make_flags_loading_widget(_tr)
            self._tabs.insertTab(
                0, self._flags_loading_widget, _tr.get("flags", "Flags")
            )
            self._tabs.setCurrentIndex(0)
            self._flags_tab = None
            if (
                self._flags_loader_thread is not None
                and self._flags_loader_thread.isRunning()
            ):
                self._flags_loader_thread.quit()
                self._flags_loader_thread.wait(2000)
            self._start_flags_loader()

        def _on_reset_layout(self) -> None:
            """Restore window layout and reset all settings to defaults."""
            if self._initial_state is not None and self._initial_geometry is not None:
                self.restoreGeometry(self._initial_geometry)
                self.restoreState(self._initial_state)
            self._settings.reset_to_defaults(self._project_root)
            if self._flags_tab is not None:
                self._flags_tab.reset_table_to_defaults()

        def _on_about(self) -> None:
            _tr = _UI_STRINGS[self._lang]
            title = _tr.get("about_title", "About CMake Build GUI")
            html = _build_about_message_html(
                _tr,
                self._lang,
                APP_VERSION,
                APP_DESCRIPTION_EN,
                APP_DESCRIPTION_RU,
            )
            dlg = QDialog(self)  # type: ignore
            dlg.setWindowTitle(title)
            layout = QVBoxLayout(dlg)
            label = QLabel(dlg)
            label.setOpenExternalLinks(True)
            label.setWordWrap(True)
            label.setTextFormat(getattr(Qt, "RichText", 2))  # type: ignore
            label.setText(html)
            label.setMinimumWidth(480)
            layout.addWidget(label)
            btn = QPushButton(_tr.get("build_tools_ok", "OK"), dlg)
            btn.clicked.connect(dlg.accept)
            layout.addWidget(btn)
            dlg.exec_()

        def _on_lang_changed(self, index: int) -> None:
            lang = self._lang_combo.currentData()
            if lang not in _UI_STRINGS:
                return
            self._lang = lang
            self._lang_combo.blockSignals(True)
            try:
                QTimer.singleShot(0, self._retranslate_ui)
            finally:
                self._lang_combo.blockSignals(False)

        def _retranslate_ui(self) -> None:
            _tr = _UI_STRINGS[self._lang]
            self.setWindowTitle(_tr.get("window_title", "CMake Build GUI"))
            self._output_dock.setWindowTitle(_tr.get("output", "Output"))
            self._project_dock.setWindowTitle(_tr.get("project_tree", "Project"))
            self._settings_dock.setWindowTitle(_tr.get("settings", "Settings"))
            self._performance_dock.setWindowTitle(_tr.get("performance", "Performance"))
            self._project_tree_widget.apply_ui_strings(_tr)
            self._tabs.setTabText(0, _tr.get("flags", "Flags"))
            self._tabs.setTabText(1, _tr.get("intellisense", "IntelliSense"))
            self._settings_group.setTitle("")
            self._performance_group.setTitle("")
            self._search_case_checkbox.setText(
                _tr.get("case_sensitive", "Case sensitive")
            )
            self._search_whole_word_checkbox.setText(
                _tr.get("whole_word", "Whole word")
            )
            self._clean_btn.setText(_tr.get("clean", "Clean"))
            self._configure_btn.setText(_tr.get("configure", "Configure"))
            self._build_btn.setText(_tr.get("build", "Build"))
            self._stop_btn.setText(_tr.get("stop", "Stop"))
            self._clear_logs_btn.setText(_tr.get("clear_logs", "Clear logs"))
            self._search_label.setText(_tr.get("find", "Find:"))
            self._search_line_edit.setPlaceholderText(
                _tr.get("search_placeholder", "Search (Ctrl+F)...")
            )
            self._search_regex_checkbox.setText(_tr.get("regex", "Regex"))
            self._settings.apply_ui_strings(_tr)
            if self._flags_tab is not None:
                self._flags_tab.apply_ui_strings(_tr)
            self._clangd_tab.apply_ui_strings(_tr)
            self._reset_act.setText(_tr.get("reset", "Reset"))
            self._reset_act.setToolTip(_tr.get("reset_tooltip", ""))
            self._lang_label.setText(_tr.get("lang", "Lang") + ": ")
            if self._project_menu is not None:
                self._project_menu.setTitle(_tr.get("project", "Project"))
            if self._open_act is not None:
                self._open_act.setText(_tr.get("open", "Open"))
            if self._view_menu is not None:
                self._view_menu.setTitle(_tr.get("view", "View"))
            if self._theme_submenu is not None:
                self._theme_submenu.setTitle(_tr.get("theme_submenu", "Theme"))
            if self._help_menu is not None:
                self._help_menu.setTitle(_tr.get("help", "Help"))
            if self._about_act is not None:
                self._about_act.setText(_tr.get("about", "About"))
            if self._shortcuts_act is not None:
                self._shortcuts_act.setText(_tr.get("shortcuts_menu", "Shortcuts…"))
            for theme_name, act in self._theme_actions:
                key_map = {
                    "Default": "theme_default",
                    "Dark": "theme_dark",
                    "Green": "theme_green",
                    "Blue": "theme_blue",
                    "Ocean": "theme_ocean",
                    "Amber": "theme_amber",
                    "Purple": "theme_purple",
                    "High Contrast": "theme_high_contrast",
                    "Forest": "theme_forest",
                    "Midnight": "theme_midnight",
                }
                key = key_map.get(theme_name) or theme_name.lower()
                act.setText(_tr.get(key, theme_name))

        def _on_log_text(self, text: str) -> None:
            """Queue log line from worker; schedule flush so GUI updates in real time."""
            if not text:
                return
            if not text.endswith("\n"):
                text = text + "\n"
            self._append_pending.append(text)
            self._schedule_flush()

        def _schedule_flush(self) -> None:
            """Schedule a single flush so the event loop can repaint between batches."""
            if self._flush_scheduled:
                return
            self._flush_scheduled = True
            QTimer.singleShot(0, self._do_flush)

        def _do_flush(self) -> None:
            """Flush a batch of pending log lines so output updates visibly.
            Scroll like VS: if user was at bottom, follow new content; else keep current scroll.
            """
            self._flush_scheduled = False
            if not self._output_plain or not self._append_pending:
                return
            vbar = self._output_plain.verticalScrollBar()
            at_bottom = False
            saved_scroll = 0
            if vbar is not None:
                at_bottom = vbar.value() >= vbar.maximum() - 20
                saved_scroll = vbar.value()
            self._output_plain.setUpdatesEnabled(False)
            try:
                batch_size = 25
                for _ in range(batch_size):
                    if not self._append_pending:
                        break
                    chunk = self._append_pending.pop(0)
                    self._append_plain_or_ansi(chunk)
            finally:
                self._output_plain.setUpdatesEnabled(True)
            if vbar is not None:
                if at_bottom:
                    vbar.setValue(vbar.maximum())
                else:
                    vbar.setValue(min(saved_scroll, vbar.maximum()))
            self._apply_search_highlights()
            if self._append_pending:
                self._schedule_flush()

        def _append_output(self, text: str) -> None:
            """Append log text to output widget (direct call, e.g. Clean or Clangd tab)."""
            if not self._output_plain:
                return
            if text and not text.endswith("\n"):
                text = text + "\n"
            self._append_pending.append(text)
            self._schedule_flush()

        def _on_clear_logs(self) -> None:
            """Clear the output log panel."""
            if self._output_plain is not None:
                self._output_plain.clear()
                self._apply_search_highlights()

        def _focus_search_bar(self) -> None:
            """Focus the search line edit (Ctrl+F)."""
            if self._search_line_edit is not None:
                self._search_line_edit.setFocus()
                self._search_line_edit.selectAll()

        _SHORTCUT_KEYS = (
            "close",
            "quit",
            "open_project",
            "find",
            "toggle_project",
            "toggle_output",
        )
        _SHORTCUT_DEFAULTS = {
            "close": "Ctrl+W",
            "quit": "Ctrl+Q",
            "open_project": "Ctrl+O",
            "find": "Ctrl+F",
            "toggle_project": "Ctrl+Shift+P",
            "toggle_output": "Ctrl+Shift+O",
        }

        def _setup_shortcuts(self) -> None:
            """Create keyboard shortcuts from QSettings (or defaults). Recreate after user changes in Shortcuts dialog."""
            for s in self._shortcuts:
                s.deleteLater()
            self._shortcuts.clear()
            _s = QSettings("CMakeBuildGUI", "compile")  # type: ignore
            _callbacks = {
                "close": self.close,
                "quit": self.close,
                "open_project": self._on_project_open,
                "find": self._focus_search_bar,
                "toggle_project": lambda: self._project_dock.setVisible(
                    not self._project_dock.isVisible()
                ),
                "toggle_output": lambda: self._output_dock.setVisible(
                    not self._output_dock.isVisible()
                ),
            }
            for key in self._SHORTCUT_KEYS:
                seq_str = _s.value(
                    "shortcuts/" + key, self._SHORTCUT_DEFAULTS.get(key, "")
                )
                if not seq_str or not isinstance(seq_str, str):
                    seq_str = self._SHORTCUT_DEFAULTS.get(key, "")
                seq = QKeySequence(seq_str) if seq_str else None  # type: ignore
                if key == "open_project":
                    if (
                        self._open_act is not None
                        and seq is not None
                        and not seq.isEmpty()
                    ):
                        self._open_act.setShortcut(seq)
                    continue
                if seq is not None and not seq.isEmpty():
                    shortcut = QShortcut(seq, self, _callbacks[key])  # type: ignore
                    self._shortcuts.append(shortcut)

        def _on_shortcuts_dialog(self) -> None:
            """Open dialog to view and edit keyboard shortcuts; save to QSettings and reapply."""
            _tr = _UI_STRINGS[self._lang]
            dlg = QDialog(self)  # type: ignore
            dlg.setWindowTitle(_tr.get("shortcuts_dialog_title", "Keyboard Shortcuts"))
            dlg.setMinimumWidth(520)
            layout = QVBoxLayout(dlg)
            table = QTableWidget(len(self._SHORTCUT_KEYS), 2)
            table.setHorizontalHeaderLabels(
                [
                    _tr.get("option", "Action"),
                    _tr.get("shortcut_key", "Shortcut"),
                ]
            )
            hdr = table.horizontalHeader()
            if hdr is not None:
                hdr.setSectionResizeMode(0, QHeaderView.ResizeToContents)
                hdr.setSectionResizeMode(1, QHeaderView.Stretch)
            _labels = {
                "close": _tr.get("shortcut_close", "Close window"),
                "quit": _tr.get("shortcut_quit", "Quit application"),
                "open_project": _tr.get("shortcut_open_project", "Open project"),
                "find": _tr.get("shortcut_find", "Focus search"),
                "toggle_project": _tr.get(
                    "shortcut_toggle_project", "Toggle Project panel"
                ),
                "toggle_output": _tr.get(
                    "shortcut_toggle_output", "Toggle Output panel"
                ),
            }
            _s = QSettings("CMakeBuildGUI", "compile")  # type: ignore
            for row, key in enumerate(self._SHORTCUT_KEYS):
                table.setItem(row, 0, QTableWidgetItem(_labels.get(key, key)))
                seq_str = _s.value(
                    "shortcuts/" + key, self._SHORTCUT_DEFAULTS.get(key, "")
                )
                if not seq_str or not isinstance(seq_str, str):
                    seq_str = self._SHORTCUT_DEFAULTS.get(key, "")
                seq_edit = QKeySequenceEdit(QKeySequence(seq_str))  # type: ignore
                seq_edit.setObjectName("shortcut_" + key)
                table.setCellWidget(row, 1, seq_edit)
            layout.addWidget(table)

            def _apply_reset() -> None:
                for row, key in enumerate(self._SHORTCUT_KEYS):
                    seq_edit = table.cellWidget(row, 1)
                    if seq_edit is not None and hasattr(seq_edit, "setKeySequence"):
                        default_str = self._SHORTCUT_DEFAULTS.get(key, "")
                        seq_edit.setKeySequence(QKeySequence(default_str))  # type: ignore
                _s2 = QSettings("CMakeBuildGUI", "compile")  # type: ignore
                for key in self._SHORTCUT_KEYS:
                    _s2.setValue(
                        "shortcuts/" + key, self._SHORTCUT_DEFAULTS.get(key, "")
                    )
                self._setup_shortcuts()

            bbox = QDialogButtonBox(QDialogButtonBox.Save | QDialogButtonBox.Cancel)  # type: ignore
            reset_btn = bbox.addButton(
                _tr.get("reset", "Reset"), QDialogButtonBox.ResetRole  # type: ignore
            )
            if reset_btn is not None:
                reset_btn.clicked.connect(_apply_reset)  # type: ignore
            bbox.accepted.connect(dlg.accept)
            bbox.rejected.connect(dlg.reject)
            save_btn = bbox.button(QDialogButtonBox.Save)  # type: ignore
            if save_btn is not None:
                save_btn.setText(_tr.get("save", "Save"))
            layout.addWidget(bbox)
            if dlg.exec_() == QDialog.Accepted:  # type: ignore
                _s = QSettings("CMakeBuildGUI", "compile")  # type: ignore
                for row, key in enumerate(self._SHORTCUT_KEYS):
                    seq_edit = table.cellWidget(row, 1)
                    if seq_edit is not None and hasattr(seq_edit, "keySequence"):
                        seq = seq_edit.keySequence()
                        seq_str = (
                            seq.toString()
                            if seq is not None and not seq.isEmpty()
                            else ""
                        )
                        _s.setValue("shortcuts/" + key, seq_str)
                self._setup_shortcuts()

        def _get_search_highlight_color(self) -> QColor:  # type: ignore
            """Return theme-visible background color for search/regex highlights."""
            if self._is_dark_theme():
                return QColor(180, 120, 0)  # type: ignore
            return QColor(255, 255, 120)  # type: ignore

        def _apply_search_highlights(self) -> None:
            """Highlight all matches using QTextCursor.find() for correct document positions."""
            if not self._output_plain or not self._search_line_edit:
                return
            search_text = self._search_line_edit.text().strip()
            if not search_text:
                self._output_plain.setExtraSelections([])
                self._search_match_positions = []
                self._search_current_index = 0
                self._update_search_count_label()
                return
            doc = self._output_plain.document()
            if doc is None:
                return
            use_regex = self._search_regex_checkbox.isChecked()
            case_sensitive = self._search_case_checkbox.isChecked()
            whole_word = self._search_whole_word_checkbox.isChecked()
            find_flags = QTextDocument.FindFlags()  # type: ignore[possibly-unbound]
            if case_sensitive:
                find_flags |= QTextDocument.FindCaseSensitively  # type: ignore[attr-defined]
            if use_regex:
                pat = search_text
                if whole_word:
                    pat = r"\b(?:" + pat + r")\b"
                try:
                    rx = QRegExp(pat)  # type: ignore[possibly-unbound]
                except Exception:
                    return
                rx.setPatternSyntax(QRegExp.RegExp)  # type: ignore[possibly-unbound]
            else:
                rx = QRegExp(search_text)  # type: ignore[possibly-unbound]
                rx.setPatternSyntax(QRegExp.FixedString)  # type: ignore[possibly-unbound]
                if whole_word:
                    find_flags |= QTextDocument.FindWholeWords  # type: ignore[attr-defined]
            rx.setCaseSensitivity(
                Qt.CaseSensitive if case_sensitive else Qt.CaseInsensitive  # type: ignore[attr-defined]
            )
            extra: List[Any] = []
            highlight_fmt = QTextCharFormat()  # type: ignore
            highlight_fmt.setBackground(self._get_search_highlight_color())
            cursor = QTextCursor(doc)  # type: ignore
            cursor.movePosition(QTextCursor.Start)  # type: ignore
            found = doc.find(rx, cursor, find_flags)
            while not found.isNull():
                sel = QTextEdit.ExtraSelection()  # type: ignore
                sel.cursor = QTextCursor(found)  # type: ignore
                sel.format = highlight_fmt
                extra.append(sel)
                cursor.setPosition(found.selectionEnd())
                found = doc.find(rx, cursor, find_flags)
            self._output_plain.setExtraSelections(extra)
            self._search_match_positions = [sel.cursor.selectionStart() for sel in extra]
            self._search_current_index = 0
            self._update_search_count_label()

        def _update_search_count_label(self) -> None:
            total = len(self._search_match_positions)
            if total == 0:
                self._search_count_label.setText("0/0")
            else:
                cur = self._search_current_index + 1
                self._search_count_label.setText("{}/{}".format(cur, total))

        def _on_search_enter(self) -> None:
            if not self._search_match_positions or not self._output_plain:
                return
            doc = self._output_plain.document()
            if doc is None:
                return
            pos = self._search_match_positions[self._search_current_index]
            cursor = QTextCursor(doc)  # type: ignore
            cursor.setPosition(pos)
            self._output_plain.setTextCursor(cursor)
            self._output_plain.ensureCursorVisible()
            self._update_search_count_label()
            self._search_current_index = (
                (self._search_current_index + 1) % len(self._search_match_positions)
            )

        def _is_dark_theme(self) -> bool:
            """True if current theme is dark (INFO should be white)."""
            key = getattr(self, "_current_theme_key", None)
            return key in ("dark", "midnight")

        def _get_log_palette(self) -> dict:
            """Return log colors for current theme (light or dark)."""
            if self._is_dark_theme():
                return {
                    "info": QColor(230, 230, 230),  # type: ignore
                    "warning": QColor(255, 180, 80),  # type: ignore
                    "error": QColor(255, 110, 110),  # type: ignore
                    "critical": QColor(255, 80, 80),  # type: ignore
                    "ansi_31": QColor(255, 110, 110),  # type: ignore
                    "ansi_32": QColor(140, 255, 140),  # type: ignore
                    "ansi_33": QColor(255, 220, 100),  # type: ignore
                    "ansi_34": QColor(120, 160, 255),  # type: ignore
                    "ansi_35": QColor(255, 130, 255),  # type: ignore
                    "ansi_36": QColor(100, 255, 255),  # type: ignore
                }
            return {
                "info": QColor(0, 0, 0),  # type: ignore
                "warning": QColor(180, 100, 0),  # type: ignore
                "error": QColor(200, 0, 0),  # type: ignore
                "critical": QColor(150, 0, 0),  # type: ignore
                "ansi_31": QColor(200, 0, 0),  # type: ignore
                "ansi_32": QColor(0, 140, 0),  # type: ignore
                "ansi_33": QColor(180, 120, 0),  # type: ignore
                "ansi_34": QColor(0, 80, 200),  # type: ignore
                "ansi_35": QColor(180, 0, 180),  # type: ignore
                "ansi_36": QColor(0, 140, 140),  # type: ignore
            }

        def _level_color(self, line: str) -> Optional[QColor]:  # type: ignore
            """Return base color for line from content and level prefix (theme-aware).
            Content (compiler error/warning) is checked first so e.g. 'INFO: ... error C1083 ...'
            is colored red, not info.
            """
            pal = self._get_log_palette()
            line_lower = line.lower()
            # Content first: compiler/log patterns override any INFO: prefix
            # Error patterns (avoid identifiers like IDS_MB_FOLDER_ERROR: match real errors)
            if (
                line_lower.startswith("error:")
                or "): error " in line_lower  # MSVC: file(line,col): error Cxxxx
                or " error: " in line_lower  # GCC/Clang
                or "error c" in line_lower  # MSVC error code (e.g. error C1083)
                or "failed:" in line_lower
                or "fatal error" in line_lower
            ):
                return pal["error"]
            # Warning patterns
            if (
                ": warning " in line_lower  # MSVC
                or " warning: " in line_lower  # GCC/Clang
                or "warning c" in line_lower  # MSVC warning code
            ):
                return pal["warning"]
            # Then prefix-based (for messages logged with explicit level)
            if line.startswith("WARNING:"):
                return pal["warning"]
            if line.startswith("CRITICAL:"):
                return pal["critical"]
            if line.startswith("ERROR:"):
                return pal["error"]
            if line.startswith("INFO:"):
                return pal["info"]
            return None

        def _append_plain_or_ansi(self, text: str) -> None:
            """Append text with level-based and ANSI coloring in the output widget.
            Each line is colored independently (ERROR:, error Cxxxx, failed:, etc. → red).
            """
            if not self._output_plain:
                return
            lines = text.splitlines()
            if not lines:
                return
            cursor = self._output_plain.textCursor()
            cursor.movePosition(QTextCursor.End)  # type: ignore
            _CSI = "\033["
            for line in lines:
                base_color = self._level_color(line)
                default_fmt = QTextCharFormat()  # type: ignore
                if base_color is not None:
                    default_fmt.setForeground(base_color)
                current_fmt = QTextCharFormat(default_fmt)  # type: ignore
                i = 0
                n = len(line)
                while i < n:
                    if i + 1 < n and line[i : i + 2] == _CSI:  # NOQA: E203
                        j = i + 2
                        while j < n and line[j] != "m" and line[j] in "0123456789;":
                            j += 1
                        if j < n and line[j] == "m":
                            # SGR sequence: parse semicolon-separated codes
                            seq = line[i + 2 : j]  # NOQA: E203
                            i = j + 1
                            for code in seq.split(";"):
                                code = code.strip()
                                if not code:
                                    current_fmt = QTextCharFormat(default_fmt)  # type: ignore
                                    continue
                                try:
                                    c = int(code)
                                except ValueError:
                                    continue
                                pal = self._get_log_palette()
                                if c == 0:
                                    current_fmt = QTextCharFormat(default_fmt)  # type: ignore
                                elif c == 1:
                                    current_fmt.setFontWeight(700)
                                elif c == 31:
                                    current_fmt.setForeground(pal["ansi_31"])
                                elif c == 32:
                                    current_fmt.setForeground(pal["ansi_32"])
                                elif c == 33:
                                    current_fmt.setForeground(pal["ansi_33"])
                                elif c == 34:
                                    current_fmt.setForeground(pal["ansi_34"])
                                elif c == 35:
                                    current_fmt.setForeground(pal["ansi_35"])
                                elif c == 36:
                                    current_fmt.setForeground(pal["ansi_36"])
                        else:
                            # Other CSI (e.g. [K erase to EOL): skip full sequence
                            i = j + 1 if j < n else i + 1
                    else:
                        k = line.find(_CSI, i)
                        if k == -1:
                            k = n
                        segment = line[i:k]
                        if segment:
                            cursor.insertText(segment, current_fmt)
                        i = k
                cursor.insertText("\n", current_fmt)
            self._output_plain.setTextCursor(cursor)

        def _on_clean(self) -> None:
            """Clean build directory for current project path."""
            project_root = self._settings.get_path() or self._project_root
            project_root = os_path_abspath(os_path_expanduser(project_root))
            if not os_path_exists(os_path_join(project_root, "CMakeLists.txt")):
                self._append_output(
                    "ERROR: CMakeLists.txt not found in {}".format(project_root)
                )
                return
            try:
                builder = CMakeBuilder(project_root)
                for h in list(builder.logger.handlers):
                    if isinstance(h, (logging_module.StreamHandler, _LogHandler)):
                        builder.logger.removeHandler(h)
                handler = _LogHandler(self._log_bridge)
                handler.setFormatter(
                    logging_module.Formatter("%(levelname)s: %(message)s")
                )
                builder.logger.addHandler(handler)
                if builder.clean_build_dir():
                    self._append_output("Build directory cleaned.")
                    self._configured_generator = None
                    self._build_disabled_by_generator_mismatch = False
                    self._update_build_button_state()
                else:
                    self._append_output("Clean failed or directory missing.")
            except Exception as e:
                self._append_output("Clean failed: {}".format(e))

        def _on_configure(self) -> None:
            self._run_task("configure")

        def _on_build(self) -> None:
            self._run_task("build")

        def _on_stop(self) -> None:
            """Request cancellation of the running configure or build."""
            if self._current_worker is not None:
                self._current_worker._cancel_requested = True
                self._append_output("Stopping... (cancellation requested)")

        def _run_task(self, task: str) -> None:
            if self._worker and self._worker.isRunning():
                self._append_output(
                    "Already running. Wait for the current task to finish."
                )
                return
            project_root = self._settings.get_path() or self._project_root
            project_root = os_path_abspath(os_path_expanduser(project_root))
            if not os_path_exists(os_path_join(project_root, "CMakeLists.txt")):
                self._append_output(
                    "ERROR: CMakeLists.txt not found in {}".format(project_root)
                )
                return
            build_type = self._settings.get_build_type()
            options = self._settings.get_options()
            option_values = (
                self._flags_tab.get_option_values() if self._flags_tab else []
            )
            self._worker = QThread()
            worker_obj = _BuildWorker(
                project_root, task, build_type, options, option_values, self._log_bridge
            )
            worker_obj.moveToThread(self._worker)
            self._worker.started.connect(worker_obj.run)
            worker_obj.finished.connect(self._on_worker_finished)
            worker_obj.finished.connect(self._worker.quit)
            self._worker.start()
            self._current_worker = worker_obj
            self._clean_btn.setEnabled(False)
            self._configure_btn.setEnabled(False)
            self._build_btn.setEnabled(False)
            self._stop_btn.setEnabled(True)
            if hasattr(self._clangd_tab, "_btn") and self._clangd_tab._btn is not None:
                self._clangd_tab._btn.setEnabled(False)
            self._append_output("--- {} started ---".format(task.capitalize()))
            self._update_button_tooltips(busy=True)

        def _get_effective_build_dir(self) -> str:
            """Return build directory for current project (project_root/build)."""
            project_root = self._settings.get_path() or self._project_root
            project_root = os_path_abspath(os_path_expanduser(project_root))
            return os_path_join(project_root, "build")

        def _read_cmake_cache_generator(self, build_dir: str) -> Optional[str]:
            """Read CMAKE_GENERATOR from CMakeCache.txt. Returns None if missing or unreadable."""
            cache_path = os_path_join(build_dir, "CMakeCache.txt")
            if not os_path_exists(cache_path):
                return None
            try:
                with open(cache_path, "r", encoding="utf-8", errors="replace") as f:
                    content = f.read()
                match = re_search(r"CMAKE_GENERATOR:INTERNAL=(.*?)\n", content)
                return match.group(1).strip() if match else None
            except Exception:
                return None

        def _update_use_ninja_availability(self) -> None:
            """When project is configured, lock Use Ninja checkbox and set tooltip."""
            _tr = _UI_STRINGS[self._lang]
            build_dir = self._get_effective_build_dir()
            configured = os_path_exists(os_path_join(build_dir, "CMakeCache.txt"))
            if not configured:
                self._settings._use_ninja.setEnabled(True)
                self._settings._use_ninja.setToolTip("")
                return
            configured_gen = (
                self._configured_generator
                or self._read_cmake_cache_generator(build_dir)
            )
            if not configured_gen:
                self._settings._use_ninja.setEnabled(True)
                self._settings._use_ninja.setToolTip("")
                return
            self._settings._use_ninja.setEnabled(False)
            self._settings._use_ninja.setToolTip(
                _tr.get(
                    "use_ninja_locked_tooltip",
                    "Cannot change generator after configuration. Project was configured "
                    'with "{generator}". Run Configure again to use another generator.',
                ).format(generator=configured_gen)
            )
            configured_is_ninja = "Ninja" in configured_gen
            if self._settings._use_ninja.isChecked() != configured_is_ninja:
                self._settings._use_ninja.blockSignals(True)
                self._settings._use_ninja.setChecked(configured_is_ninja)
                self._settings._use_ninja.blockSignals(False)

        def _update_button_tooltips(self, busy: bool = False) -> None:
            """Set tooltips for Clean, Configure, Build, Stop based on current state."""
            _tr = _UI_STRINGS[self._lang]
            if busy:
                self._clean_btn.setToolTip(_tr.get("hint_clean_busy", ""))
                self._configure_btn.setToolTip(_tr.get("hint_configure_busy", ""))
                self._build_btn.setToolTip(_tr.get("hint_build_busy", ""))
                self._stop_btn.setToolTip(_tr.get("hint_stop", ""))
                return
            self._clean_btn.setToolTip(_tr.get("hint_clean", ""))
            self._configure_btn.setToolTip(_tr.get("hint_configure", ""))
            build_dir = self._get_effective_build_dir()
            configured = os_path_exists(os_path_join(build_dir, "CMakeCache.txt"))
            if configured and not self._build_disabled_by_generator_mismatch:
                self._build_btn.setToolTip(_tr.get("hint_build", ""))
            else:
                self._build_btn.setToolTip(_tr.get("hint_build_not_configured", ""))
            self._stop_btn.setToolTip(_tr.get("hint_stop_idle", ""))

        def _update_build_button_state(self) -> None:
            """Enable Build only when project is configured (and generator matches). After Clean, project is not configured."""
            if self._current_worker is not None:
                return
            self._clean_btn.setEnabled(True)
            self._configure_btn.setEnabled(True)
            build_dir = self._get_effective_build_dir()
            configured = os_path_exists(os_path_join(build_dir, "CMakeCache.txt"))
            if not configured:
                self._build_btn.setEnabled(False)
            elif self._build_disabled_by_generator_mismatch:
                self._build_btn.setEnabled(False)
            else:
                self._build_btn.setEnabled(True)
            self._stop_btn.setEnabled(False)
            self._update_use_ninja_availability()
            self._update_button_tooltips(busy=False)

        def _on_use_ninja_toggled(self) -> None:
            """Update Build button state when Use Ninja is toggled (only when checkbox is enabled)."""
            self._update_build_button_state()

        def _on_worker_finished(self, success: bool) -> None:
            task = (
                getattr(self._current_worker, "_task", None)
                if self._current_worker
                else None
            )
            self._current_worker = None
            self._clean_btn.setEnabled(True)
            self._configure_btn.setEnabled(True)
            if task == "configure" and success:
                build_dir = self._get_effective_build_dir()
                self._configured_generator = self._read_cmake_cache_generator(build_dir)
                self._build_disabled_by_generator_mismatch = False
            elif task == "clean" and success:
                self._configured_generator = None
                self._build_disabled_by_generator_mismatch = False
            self._update_build_button_state()
            if hasattr(self._clangd_tab, "_btn") and self._clangd_tab._btn is not None:
                self._clangd_tab._btn.setEnabled(True)
            self._append_output(
                "--- Finished: {} ---".format("OK" if success else "FAILED")
            )


def main():
    cli = CMakeBuilderCLI()
    cli.run()


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        colorlog_getLogger().info("Build process interrupted by user.")
        sys_exit(1)
    except Exception as e:
        colorlog_getLogger().critical("An unexpected error occurred: {}".format(e))
        sys_exit(1)
