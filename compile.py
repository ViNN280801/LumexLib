#!/usr/bin/env python3
"""
Cross-platform CMake build system wrapper with colored logging.

This script provides a clean interface to build CMake projects with various
configuration options while handling errors gracefully.
"""

from os import chdir as os_chdir
from os import remove as os_remove
from os import makedirs as os_makedirs
from os.path import join as os_path_join
from os.path import pathsep as os_pathsep
from os.path import isdir as os_path_isdir
from os.path import exists as os_path_exists
from os.path import abspath as os_path_abspath
from os.path import dirname as os_path_dirname
from os.path import basename as os_path_basename

from re import search as re_search
from re import DOTALL as re_DOTALL

from sys import exit as sys_exit
from shutil import rmtree as shutil_rmtree
from psutil import cpu_count as psutil_cpu_count
from psutil import virtual_memory as psutil_virtual_memory

from logging import INFO as logging_INFO
from argparse import ArgumentParser as argparse_ArgumentParser

from platform import system as platform_system
from platform import machine as platform_machine

from colorlog import getLogger as colorlog_getLogger
from colorlog import StreamHandler as colorlog_StreamHandler
from colorlog import ColoredFormatter as colorlog_ColoredFormatter

from subprocess import run as subprocess_run
from subprocess import TimeoutExpired as subprocess_TimeoutExpired
from subprocess import CalledProcessError as subprocess_CalledProcessError

from typing import List, Optional
from shutil import which as shutil_which


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

        Returns:
            bool: True if Ninja is available, False otherwise
        """
        ninja_cmd = shutil_which("ninja")
        if ninja_cmd:
            self.logger.info(f"Ninja build system found: {ninja_cmd}")
            return True
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
            self.logger.info("   a) Download from: https://github.com/ninja-build/ninja/releases")
            self.logger.info("   b) Extract ninja.exe to a directory (e.g., C:\\Tools\\ninja)")
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
    ) -> bool:
        """
        Execute a shell command with error handling.

        Args:
            cmd_list: List of command and arguments
            cwd: Working directory for command execution
            capture_output: Whether to capture command output (for dependency checks)
            timeout: Timeout in seconds for command execution

        Returns:
            bool: True if command succeeded, False otherwise
        """
        if not capture_output:
            self.logger.info("Running command: {}".format(" ".join(cmd_list)))
        try:
            subprocess_run(
                cmd_list,
                cwd=cwd,
                check=True,
                text=True,
                capture_output=capture_output,
                timeout=timeout,
            )
            if not capture_output:
                self.logger.info("Command finished successfully.")
            return True
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

    def add_toolset(self, toolset: str) -> None:
        """
        Description:
            Adds a MSVC toolset to the CMake arguments.
            Supported toolsets: v141, v142, v143.
            v141 = Visual Studio 2017 (oldest supported version).
            v142 = Visual Studio 2019 (better compatibility with old Windows).
            v143 = Visual Studio 2022 (default).
            For UNIX systems, the toolset is not supported, so this function does nothing.

        Args:
            toolset (str): The MSVC toolset to add.
        """

        if platform_system() == "Windows":
            if toolset not in ["v141", "v142", "v143"]:
                self.logger.warning("Unsupported MSVC toolset: {}".format(toolset))
                return

            self.cmake_args.append("-T")
            if toolset == "v141":
                self.cmake_args.append("v141,host={}".format(self.architecture))
            elif toolset == "v142":
                self.cmake_args.append("v142,host={}".format(self.architecture))
            else:
                self.cmake_args.append("v143,host={}".format(self.architecture))
            self.logger.info("🔍 Using MSVC toolset: {}".format(toolset))

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
            self.logger.info(f"  C{std:2d}: {desc}")

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
            self.logger.info(f"  C++{std:2d}: {desc}")

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
            self.logger.info(f"\n🔧 {info['name']} ({compiler_id.upper()})")
            self.logger.info(f"   Platforms: {', '.join(info['platforms'])}")
            self.logger.info(f"   Standards: {info['standards']}")
            self.logger.info(f"   Features: {info['features']}")
            self.logger.info(f"   Flags: {info['flags']}")

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
            self.logger.warning(f"C compiler path does not exist: {compiler_path}")
            return

        self.custom_c_compiler = os_path_abspath(compiler_path)
        self.cmake_args.append(f"-DCMAKE_C_COMPILER={self.custom_c_compiler}")
        self.logger.info(f"🔧 Using custom C compiler: {self.custom_c_compiler}")

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
            self.logger.warning(f"C++ compiler path does not exist: {compiler_path}")
            return

        self.custom_cpp_compiler = os_path_abspath(compiler_path)
        self.cmake_args.append(f"-DCMAKE_CXX_COMPILER={self.custom_cpp_compiler}")
        self.logger.info(f"🔧 Using custom C++ compiler: {self.custom_cpp_compiler}")

    def apply_compiler_specific_optimizations(self, compiler_id: str) -> None:
        """
        Apply compiler-specific optimization flags based on detected compiler.

        Args:
            compiler_id: Detected compiler identifier
        """
        if compiler_id == "gcc":
            self.cmake_args.extend(
                [
                    "-DCMAKE_CXX_FLAGS_RELEASE=-O3 -DNDEBUG -march=native -mtune=native",
                    "-DCMAKE_C_FLAGS_RELEASE=-O3 -DNDEBUG -march=native -mtune=native",
                    "-DCMAKE_CXX_FLAGS_DEBUG=-g -O0 -Wall -Wextra -Wpedantic",
                    "-DCMAKE_C_FLAGS_DEBUG=-g -O0 -Wall -Wextra -Wpedantic",
                ]
            )
        elif compiler_id == "mingw":
            self.cmake_args.extend(
                [
                    "-DCMAKE_CXX_FLAGS_RELEASE=-O3 -DNDEBUG -march=native -mtune=native",
                    "-DCMAKE_C_FLAGS_RELEASE=-O3 -DNDEBUG -march=native -mtune=native",
                    "-DCMAKE_CXX_FLAGS_DEBUG=-g -O0 -Wall -Wextra -Wpedantic",
                    "-DCMAKE_C_FLAGS_DEBUG=-g -O0 -Wall -Wextra -Wpedantic",
                ]
            )
        elif compiler_id == "clang":
            self.cmake_args.extend(
                [
                    "-DCMAKE_CXX_FLAGS_RELEASE=-O3 -DNDEBUG -march=native",
                    "-DCMAKE_C_FLAGS_RELEASE=-O3 -DNDEBUG -march=native",
                    "-DCMAKE_CXX_FLAGS_DEBUG=-g -O0 -Wall -Wextra -Wpedantic",
                    "-DCMAKE_C_FLAGS_DEBUG=-g -O0 -Wall -Wextra -Wpedantic",
                ]
            )
        elif compiler_id == "clang-cl":
            self.cmake_args.extend(
                [
                    "-DCMAKE_CXX_FLAGS_RELEASE=/O2 /Oi /Ot /GL /arch:AVX2",
                    "-DCMAKE_C_FLAGS_RELEASE=/O2 /Oi /Ot /GL /arch:AVX2",
                    "-DCMAKE_CXX_FLAGS_DEBUG=/Od /Zi /W4",
                    "-DCMAKE_C_FLAGS_DEBUG=/Od /Zi /W4",
                ]
            )
        elif compiler_id == "msvc":
            self.cmake_args.extend(
                [
                    "-DCMAKE_CXX_FLAGS_RELEASE=/O2 /Oi /Ot /GL /arch:AVX2 /GS",
                    "-DCMAKE_C_FLAGS_RELEASE=/O2 /Oi /Ot /GL /arch:AVX2 /GS",
                    "-DCMAKE_CXX_FLAGS_DEBUG=/Od /Zi /W4 /GS",
                    "-DCMAKE_C_FLAGS_DEBUG=/Od /Zi /W4 /GS",
                ]
            )
        elif compiler_id == "icc":
            self.cmake_args.extend(
                [
                    "-DCMAKE_CXX_FLAGS_RELEASE=-O3 -xHost -ipo",
                    "-DCMAKE_C_FLAGS_RELEASE=-O3 -xHost -ipo",
                    "-DCMAKE_CXX_FLAGS_DEBUG=-g -O0 -Wall",
                    "-DCMAKE_C_FLAGS_DEBUG=-g -O0 -Wall",
                ]
            )
        elif compiler_id == "borland":
            self.cmake_args.extend(
                [
                    "-DCMAKE_CXX_FLAGS_RELEASE=-O2 -w- -tW",
                    "-DCMAKE_C_FLAGS_RELEASE=-O2 -w- -tW",
                    "-DCMAKE_CXX_FLAGS_DEBUG=-g -O0 -w-",
                    "-DCMAKE_C_FLAGS_DEBUG=-g -O0 -w-",
                ]
            )
        elif compiler_id == "pgi":
            self.cmake_args.extend(
                [
                    "-DCMAKE_CXX_FLAGS_RELEASE=-O3 -fast -Mipa=fast",
                    "-DCMAKE_C_FLAGS_RELEASE=-O3 -fast -Mipa=fast",
                    "-DCMAKE_CXX_FLAGS_DEBUG=-g -O0",
                    "-DCMAKE_C_FLAGS_DEBUG=-g -O0",
                ]
            )
        elif compiler_id == "xlc":
            self.cmake_args.extend(
                [
                    "-DCMAKE_CXX_FLAGS_RELEASE=-O3 -qhot -qarch=auto",
                    "-DCMAKE_C_FLAGS_RELEASE=-O3 -qhot -qarch=auto",
                    "-DCMAKE_CXX_FLAGS_DEBUG=-g -O0",
                    "-DCMAKE_C_FLAGS_DEBUG=-g -O0",
                ]
            )
        elif compiler_id == "aocc":
            self.cmake_args.extend(
                [
                    "-DCMAKE_CXX_FLAGS_RELEASE=-O3 -march=native -flto",
                    "-DCMAKE_C_FLAGS_RELEASE=-O3 -march=native -flto",
                    "-DCMAKE_CXX_FLAGS_DEBUG=-g -O0 -Wall -Wextra",
                    "-DCMAKE_C_FLAGS_DEBUG=-g -O0 -Wall -Wextra",
                ]
            )
        elif compiler_id == "armclang":
            self.cmake_args.extend(
                [
                    "-DCMAKE_CXX_FLAGS_RELEASE=-O3 -mcpu=native -flto",
                    "-DCMAKE_C_FLAGS_RELEASE=-O3 -mcpu=native -flto",
                    "-DCMAKE_CXX_FLAGS_DEBUG=-g -O0 -Wall -Wextra",
                    "-DCMAKE_C_FLAGS_DEBUG=-g -O0 -Wall -Wextra",
                ]
            )
        elif compiler_id == "nvcc":
            self.cmake_args.extend(
                [
                    "-DCMAKE_CXX_FLAGS_RELEASE=-O3 -arch=sm_XX -Xcompiler -O3",
                    "-DCMAKE_C_FLAGS_RELEASE=-O3 -arch=sm_XX -Xcompiler -O3",
                    "-DCMAKE_CXX_FLAGS_DEBUG=-g -O0 -Xcompiler -g",
                    "-DCMAKE_C_FLAGS_DEBUG=-g -O0 -Xcompiler -g",
                ]
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
                f"DEBUG: Found CMAKE_CXX_COMPILER_ID: {detected_compiler}"
            )
        else:
            self.logger.debug("DEBUG: CMAKE_CXX_COMPILER_ID not found or regex failed.")

        # 2. Try to infer from CMAKE_GENERATOR (especially for MSVC)
        match_generator = re_search(r"CMAKE_GENERATOR:INTERNAL=(.*?)\n", cache_content)
        if match_generator:
            generator_name = match_generator.group(1)
            self.logger.debug(f"DEBUG: Found CMAKE_GENERATOR: {generator_name}")
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
                    f"DEBUG: Inferred MSVC compiler: {detected_compiler}, version: {detected_version_tag}"
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
                    f"DEBUG: Found CMAKE_CXX_COMPILER path: {compiler_path}"
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
                    f"DEBUG: Inferred compiler from path: {detected_compiler}, version: {detected_version_tag}"
                )
            else:
                self.logger.debug("DEBUG: CMAKE_CXX_COMPILER path not found.")

        # Save found version to self.compiler_version_tag
        self.compiler_version_tag = detected_version_tag
        self.logger.debug(
            f"DEBUG: Final detected compiler: {detected_compiler}, version tag: {self.compiler_version_tag}"
        )

        return detected_compiler

    def configure(self, build_type: str) -> bool:
        """
        Configure CMake project.

        Args:
            build_type: Build type (Debug/Release)

        Returns:
            bool: True if configuration succeeded
        """
        self.build_type = build_type  # Store build type for later use
        os_makedirs(self.build_dir, exist_ok=True)

        cmake_configure_cmd = [
            "cmake",
            self.project_root,
            "-DCMAKE_BUILD_TYPE={}".format(build_type),
            "-B",
            self.build_dir,
        ]

        # Add Ninja generator if enabled
        if self.use_ninja:
            cmake_configure_cmd.extend(["-G", "Ninja"])
            self.logger.info("Using Ninja build system generator")

        # Add architecture/platform specific flags for Visual Studio generators (not for Ninja)
        if platform_system() == "Windows" and not self.use_ninja:
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

        if not self.run_command(cmake_configure_cmd):
            return False

        # After successful configuration, try to determine the compiler ID and version
        try:
            # If custom compilers were specified, detect from their paths first
            if self.custom_cpp_compiler:
                detected_from_path = self._detect_compiler_from_custom_path(
                    self.custom_cpp_compiler
                )
                if detected_from_path != "unknown":
                    self.compiler_id = detected_from_path
                    self.logger.info(
                        f"Detected compiler from custom path: {detected_from_path.upper()}"
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
                    self.logger.info(f"Detected compiler: {compiler_display_name}")
                    # Apply compiler-specific optimizations
                    self.apply_compiler_specific_optimizations(self.compiler_id)
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

        return True

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

    def enable_ninja(self) -> bool:
        """
        Enable Ninja build system for CMake.

        Returns:
            bool: True if Ninja is available and enabled, False if not available
        """
        if not self._check_ninja_available():
            self._show_ninja_installation_instructions()
            return False

        self.use_ninja = True
        self.logger.info("Ninja build system enabled")
        return True

    def build(self, build_type: str) -> bool:
        """
        Build the configured project with dynamic parallel job count based on system resources.

        Args:
            build_type: Build type (Debug/Release)

        Returns:
            bool: True if build succeeded
        """
        # Get available CPU cores and free memory
        cpu_cores = psutil_cpu_count(logical=False)  # Physical cores
        free_memory = psutil_virtual_memory().available / (1024**3)  # Free memory in GB
        
        if not cpu_cores:
            self.logger.warning("No CPU cores detected, using 1 parallel job.")
            cpu_cores = 1

        # Calculate safe parallel jobs
        if free_memory < 2.0:  # Less than 2GB free memory
            parallel_jobs = 1
        elif free_memory < 4.0:  # Less than 4GB free memory
            parallel_jobs = max(1, cpu_cores // 2)
        else:
            parallel_jobs = cpu_cores

        cmake_build_cmd = [
            "cmake",
            "--build",
            self.build_dir,
            "--config",
            build_type,
            "--parallel",
            str(parallel_jobs),
        ]
        return self.run_command(cmake_build_cmd)

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
                f"Missing required dependencies: {', '.join(missing_deps)}"
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
            self.logger.error(f"Doxyfile not found at: {doxyfile_path}")
            self.logger.error("Please ensure Doxyfile exists in the project root")
            return False

        self.logger.info(f"Doxyfile found at: {doxyfile_path}")
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
            self.logger.info(f"HTML documentation generated successfully: {index_file}")
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
            self.logger.info(f"Changed to LaTeX directory: {latex_dir}")

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
                        f"PDF documentation generated successfully: {os_path_join(latex_dir, pdf_file)}"
                    )
                    self.logger.info(f"PDF file size: {pdf_size:.2f} MB")
                except OSError:
                    self.logger.info(
                        f"PDF documentation generated successfully: {os_path_join(latex_dir, pdf_file)}"
                    )
                return True
            else:
                self.logger.error("PDF generation completed but refman.pdf not found")
                return False

        except Exception as e:
            self.logger.error(f"Unexpected error during PDF generation: {e}")
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
        self.logger.info(f"Starting documentation generation in format: {doc_format}")

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
                    f"HTML documentation: {os_path_join(html_dir, 'index.html')}"
                )

            if doc_format in ["pdf", "all"]:
                latex_dir = os_path_join(self.project_root, "docs", "latex")
                self.logger.info(
                    f"PDF documentation: {os_path_join(latex_dir, 'refman.pdf')}"
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
            self.logger.error(f"Error reading CMakeLists.txt: {e}")
            return False
        except Exception as e:
            self.logger.error(f"Unexpected error checking packaging configuration: {e}")
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
        # Create detailed installer name
        compiler_tag_part = self.compiler_id
        if self.compiler_version_tag:
            compiler_tag_part += self.compiler_version_tag
        elif self.compiler_id == "unknown":
            compiler_tag_part = "unknown"

        # Generate detailed package name
        package_name = (
            f"DChannel-{self.version}-{self.os_prefix}_{self.architecture}_"
            f"{compiler_tag_part}_cpp{self.cpp_version}"
        )

        # Determine appropriate packaging generator based on platform
        if platform_system() == "Windows":
            generators = ["NSIS", "ZIP"]
            primary_generator = "NSIS"
        elif platform_system() == "Darwin":  # macOS
            generators = ["DragNDrop", "TGZ"]
            primary_generator = "DragNDrop"
        else:  # Linux/Unix
            generators = ["TGZ", "DEB", "RPM"]
            primary_generator = "TGZ"

        # Set CPack variables for detailed naming
        cpack_cmd = [
            "cpack",
            "-G",
            primary_generator,
            "-C",
            build_type,
            "-D",
            f"CPACK_PACKAGE_NAME=DChannel",
            "-D",
            f"CPACK_PACKAGE_FILE_NAME={package_name}",
            "-D",
            f"CPACK_PACKAGE_VERSION={self.version}",
            "-D",
            f"CPACK_PACKAGE_DESCRIPTION_SUMMARY=DChannel {self.version} ({compiler_tag_part}, C++{self.cpp_version})",
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

        self.logger.info(f"📦 Generating package: {package_name}{file_ext}")
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

        self.logger.info(f"📁 Installing to: {full_install_path}")
        self.logger.info(f"   Version: {self.version}")
        self.logger.info(f"   Platform: {self.os_prefix}_{self.architecture}")
        self.logger.info(f"   Compiler: {compiler_tag_part}")
        self.logger.info(f"   Standards: C{self.c_version}, C++{self.cpp_version}")

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
                configure_cmd, check=True, text=True, capture_output=True
            )

            if not configure_result.returncode == 0:
                self.logger.error(
                    "Error configuring the project: {}".format(configure_result.stderr)
                )
                return False

            # Now dump the cache variables
            dump_cmd = ["cmake", "-LA", temp_build_dir]
            dump_result = subprocess_run(
                dump_cmd, check=True, text=True, capture_output=False
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
    def __init__(self, project_root: str):
        self.project_root = project_root
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

        self.parser = argparse_ArgumentParser(
            description="Cross-platform CMake build system wrapper."
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
            default=11,
            help="C standard to use (89, 99, 11, 17, 23). Defaults to 11.",
        )
        self.parser.add_argument(
            "--stdcxx",
            type=int,
            default=11,
            help="C++ standard to use (98, 03, 11, 14, 17, 20, 23, 26). Defaults to 11.",
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
            help="Build in Release mode and generate installer package (requires CPack configuration in CMakeLists.txt)",
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
            action="store_true",
            help="Use Ninja build system instead of default generator (Make/MSBuild). Provides faster builds with better parallelism.",
        )

        # MSVC toolset selection (Windows only)
        if platform_system() == "Windows":
            self.parser.add_argument(
                "--toolset",
                choices=["v141", "v142", "v143"],
                help=(
                    "Select MSVC toolset version.\n"
                    "v141 = Visual Studio 2017 (oldest supported version).\n"
                    "v142 = Visual Studio 2019 (better compatibility with old Windows).\n"
                    "v143 = Visual Studio 2022 (default)."
                ),
            )

        self.args = self.parser.parse_args()

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
        ):
            self.logger.error("ERROR: build_type is required for build operations")
            self.logger.error("Use --help for usage information")
            sys_exit(1)

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

        # Enable Ninja build system if requested
        if self.args.use_ninja:
            if not self.builder.enable_ninja():
                self.logger.error("ERROR: Cannot enable Ninja build system")
                sys_exit(1)

        # Apply custom compilers if specified
        if self.args.compiler_c:
            self.builder.set_custom_c_compiler(self.args.compiler_c)

        if self.args.compiler_cpp:
            self.builder.set_custom_cpp_compiler(self.args.compiler_cpp)

        # Apply C standard
        self.builder.add_c_standard(self.args.stdc, override=True)
        self.logger.info("⚙️ Using C Standard: C{}".format(self.args.stdc))

        # Apply C++ standard
        self.builder.add_cpp_standard(self.args.stdcxx, override=True)
        self.logger.info("⚙️ Using C++ Standard: C++{}".format(self.args.stdcxx))

        # Validate standard compatibility
        if not self.builder.validate_standard_compatibility():
            self.logger.warning("Standard compatibility warning issued")

        # Apply architecture
        if self.args.arch:
            self.builder.add_architecture(self.args.arch)
        else:
            self.builder.add_architecture(self.auto_detected_arch)

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
            # Windows uses ; as separator, Unix uses :
            raw = self.args.cmake_args.replace(";", os_pathsep)
            raw = raw.replace(":", os_pathsep)
            self.builder.add_cmake_args(raw.split(os_pathsep))

        if not self.builder.configure(self.args.build_type):
            self.logger.error("ERROR: CMake configuration failed")
            sys_exit(1)

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


def main():
    cli = CMakeBuilderCLI(os_path_dirname(os_path_abspath(__file__)))
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
