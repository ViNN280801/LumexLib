#!/usr/bin/env bash
# create_release.sh - build LumexLib with every given compiler and ISA and
# package each build as
#   LumexLib-<version>_linux_<ISA>_<compiler>_glibc<glibc>_cxx<std>.<deb|rpm|tar.xz|tar.gz>
# or, for a MinGW cross compiler,
#   LumexLib-<version>_win_x64_<compiler>_cxx<std>.zip
# The C++ standard is in the name because it is part of what a consumer must
# match (the ABI of the standard library types changes with it).
#
# Run with --help for the options. Every check (tools, compilers, the
# 32-bit toolchain) runs before the first build, so a missing piece stops
# the script before any work is done. The warnings and errors of every build
# are kept in <output-dir>/warns/.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="${SCRIPT_DIR}"

usage() {
  cat <<'EOF'
Usage: ./create_release.sh --compilers <path>[,<path>...] [options]

Builds LumexLib (Release, shared libraries, no tests) once per compiler and
ISA, installs it under /opt/LumexLib/<version>_<compiler>, and packages it.
A MinGW-w64 cross compiler (x86_64-w64-mingw32-g++-posix) builds the Windows
DLLs and packs them, with the MinGW runtime DLLs, into a zip.

Required:
  --compilers LIST   Comma-separated compiler paths. Each entry may name the
                     C or the C++ compiler; the other one must lie next to it
                     (gcc-13 <-> g++-13, clang <-> clang++, cc <-> c++,
                     x86_64-w64-mingw32-g++-posix <-> ...-gcc-posix). A MinGW
                     compiler must use the posix thread model: the library
                     uses std::thread.
                     Example (the four compilers of the release machine):
                     /usr/bin/gcc-8,/opt/gcc-13.2.0/bin/gcc,/opt/llvm-23.1.0/bin/clang++,/usr/bin/x86_64-w64-mingw32-g++-posix

Options:
  --arch LIST        x86-64 (default), x86. Comma-separated, for example
                     --arch x86,x86-64. x86 builds with -m32 and needs the
                     32-bit glibc and C++ library of every listed compiler.
  --formats LIST     deb,rpm,tar.xz,tar.gz (default: all four). rpm needs
                     rpmbuild (package "rpm"), deb needs dpkg-deb and fakeroot.
                     A MinGW compiler always gives a zip (needs "zip");
                     --formats does not apply to it.
  --std LIST         C++ standards to build, comma-separated (11, 14, 17, 20,
                     23), for example --std 11,14,17,20: every compiler is
                     built once per standard. Default: 20 where the compiler
                     supports it (probe: -std=c++20 gives __cplusplus >=
                     202002L), otherwise the compiler's default standard.
                     GCC 8 and MinGW 8.3 have no
                     C++20 (only the draft -std=c++2a, __cplusplus 201709L):
                     --std 20 builds that draft there and says so. The
                     standard of the build ends the name of the package:
                     ..._cxx17.tar.gz (cxx2a for the draft, and the compiler's
                     own default when no standard was set).
  --no-werror        Build without -Werror. By default the script passes
                     -DLUMEX_WERROR=ON, so a warning fails the build. A --tag
                     older than 2.0.0.0 has no such option and builds as it
                     was released.
  --use-ninja        Configure with -G Ninja (needs ninja in PATH). Without it
                     CMake picks its own generator: $CMAKE_GENERATOR if set,
                     else Unix Makefiles (needs make). The packages are the
                     same; after a failure Ninja has built more (make leaves
                     out the libraries that depend on a failed one), so one
                     failed run lists more diagnostics.
  --dry-run          Show what would run, run nothing: no build tree, no log,
                     no package, no worktree. The compiler checks that decide
                     the plan (do they run, link, support the standard) still
                     run; they write only to a temporary directory.
  --quiet            Do not show the output of the commands, only the commands
                     and the result of every build. The output is in the logs
                     under <output-dir>/.work/<build>/ either way.
  --color WHEN       auto (default), always or never. auto colours a terminal
                     and honours NO_COLOR. Every command the script runs is
                     shown in cyan after a "$"; its output keeps the colour
                     of the terminal.
  --no-package       Build only: no install, no packages, no packaging tools
                     needed. The warnings of the build are still collected;
                     with --std 11,14,17,20 one run checks all four standards.
  --output-dir DIR   Where the packages go (default: <repo>/release). The
                     diagnostics of every build go to <output-dir>/warns/:
                     <compiler>_c++<std>_warn.log and _err.log (x86 builds:
                     <compiler>_x86_c++<std>...), written only when not empty,
                     so an empty folder means a clean run. A warning that
                     -Werror turned into an error is listed in _warn.log.
  --version V        Override the version read from project(LumexLib VERSION).
  --tag TAG          Build the sources of a git tag instead of the working tree.
                     A detached worktree of the tag is created under
                     <output-dir>/.work/src-<TAG>, its CMakeRoutines submodule is
                     initialized from this checkout's copy (no network), and the
                     packages are named after the tag's project(LumexLib VERSION).
                     This script drives the build; compile.py and the CMakeLists
                     come from the tag. The worktree is removed at the end (kept
                     with --keep-work; on an early stop remove it with:
                     git worktree remove --force <path>; git worktree prune).
  -j N, --jobs N, --parallel N
                     Parallel build jobs, passed to cmake --build --parallel
                     (default: nproc). -jN, --jobs=N and --parallel=N work too.
  --keep-work        Keep the build, staging and packaging trees under
                     <output-dir>/.work (removed by default).
  -h, --help         Show this help.

Notes:
  * A build that fails does not stop the run: the other builds go on, the
    errors are in the _err.log of the build, and the exit status is 1.
  * The compiler's own C++ runtime (libstdc++ / libgcc_s, or libc++ /
    libc++abi / libunwind) goes into lib/ of the package when it is not the
    system one (/lib, /usr/lib), selected by cmake/PublishDistr.cmake; the
    libraries get RUNPATH $ORIGIN.
  * <repo>/x64 is removed after every package, so that published build
    output never lands in a later archive.
  * dev/ carries the .debug files of every library, split out next to the
    release binaries by the CMakeRoutines linker launcher.
  * --tag builds a tag's sources in a worktree under <output-dir>/.work and
    removes it when the run ends; the tag's CMakeLists sets the version.
EOF
}

# Colours: empty until setup_colors, so every message is plain before the
# options are read and when the output is not a terminal.
C_RESET=""; C_BOLD=""; C_DIM=""; C_CYAN=""; C_GREEN=""; C_YELLOW=""; C_RED=""
E_RESET=""; E_RED=""; E_YELLOW=""

# setup_colors <auto|always|never>: stdout and stderr are decided separately.
setup_colors() {
  local out=0 err=0
  case "$1" in
    always) out=1; err=1 ;;
    never) ;;
    *)
      if [[ -z "${NO_COLOR:-}" && "${TERM:-dumb}" != dumb ]]; then
        [[ -t 1 ]] && out=1
        [[ -t 2 ]] && err=1
      fi
      ;;
  esac
  if [[ "$out" -eq 1 ]]; then
    C_RESET=$'\033[0m'; C_BOLD=$'\033[1m'; C_DIM=$'\033[2m'; C_CYAN=$'\033[36m'
    C_GREEN=$'\033[32m'; C_YELLOW=$'\033[33m'; C_RED=$'\033[31m'
  fi
  if [[ "$err" -eq 1 ]]; then
    E_RESET=$'\033[0m'; E_RED=$'\033[1;31m'; E_YELLOW=$'\033[33m'
  fi
  return 0
}

die() {
  echo "${E_RED}Error:${E_RESET} $*" >&2
  exit 1
}

warn() {
  echo "${E_YELLOW}Warning:${E_RESET} $*" >&2
}

# quote_arg <argument>: the argument as one could type it; a <placeholder> stays readable.
quote_arg() {
  if [[ "$1" == \<*\> ]]; then printf '%s' "$1"; else printf '%q' "$1"; fi
}

# show_cmd <command...>: the command line as one could type it, in cyan after a
# "$"; a long one gets one argument per line.
show_cmd() {
  local -a args=("$@")
  local i n=${#args[@]} line="" part sep
  for ((i = 0; i < n; i++)); do
    line+="$(quote_arg "${args[$i]}") "
  done
  if [[ ${#line} -gt 110 ]]; then
    line=""
    sep=""
    # The command and its subcommand (fakeroot dpkg-deb) stay on the first line.
    for ((i = 0; i < n; i++)); do
      [[ $i -eq 0 || ( "${args[$i]}" != -* && "${args[$i]}" != *=* && "${args[$i]}" != */* ) ]] || break
      line+="${line:+ }$(quote_arg "${args[$i]}")"
      sep=$' \\\n    '
    done
    for ((; i < n; i++)); do
      part="$(quote_arg "${args[$i]}")"
      # An option and its value stay on one line: -G Ninja, --parallel 4,
      # tar's -czf <file>. A group of flags (zip's -qrX) takes no value.
      if [[ "${args[$i]}" == -* && "${args[$i]}" != *=* && $((i + 1)) -lt $n \
        && "${args[$((i + 1))]}" != -* \
        && ( "${args[$i]}" == --* || ${#args[$i]} -eq 2 || "${args[$i]}" == -*f ) ]]; then
        part+=" $(quote_arg "${args[$((i + 1))]}")"
        i=$((i + 1))
      fi
      line+="${sep}${part}"
      sep=$' \\\n    '
    done
  fi
  echo "${C_CYAN}\$ ${line% }${C_RESET}"
}

# run_logged <log> <command...>: show the command, run it, keep its output in
# <log> and, unless --quiet, show it in the terminal's own colour. Returns the
# status of the command. With --dry-run it only shows the command.
run_logged() {
  local log="$1"
  shift
  show_cmd "$@"
  [[ "${DRY_RUN}" -eq 1 ]] && return 0
  if [[ "${QUIET}" -eq 1 ]]; then
    "$@" >"${log}" 2>&1
  else
    "$@" 2>&1 | tee "${log}"
  fi
}

# run_plain <command...>: show the command and run it, its output as it is.
run_plain() {
  show_cmd "$@"
  [[ "${DRY_RUN}" -eq 1 ]] && return 0
  "$@"
}

# note <text>: a dim line for what a dry run would do without a command to show.
note() {
  echo "${C_DIM}# $*${C_RESET}"
}

# ---------------------------------------------------------------------------
# Arguments
# ---------------------------------------------------------------------------

COMPILERS_ARG=""
ARCH_ARG="x86-64"
FORMATS_ARG="deb,rpm,tar.xz,tar.gz"
STD_ARG=""
OUTPUT_DIR="${REPO_ROOT}/release"
VERSION_ARG=""
TAG_ARG=""
JOBS="$(nproc 2>/dev/null || echo 4)"
KEEP_WORK=0
WERROR=1
NO_PACKAGE=0
USE_NINJA=0
DRY_RUN=0
QUIET=0
COLOR_ARG="auto"

need_value() {
  [[ $# -ge 2 && -n "$2" && "$2" != --* ]] || die "$1 needs a value (see --help)"
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    -h | --help) usage; exit 0 ;;
    --compilers) need_value "$@"; COMPILERS_ARG="$2"; shift 2 ;;
    --compilers=*) COMPILERS_ARG="${1#*=}"; shift ;;
    --arch) need_value "$@"; ARCH_ARG="$2"; shift 2 ;;
    --arch=*) ARCH_ARG="${1#*=}"; shift ;;
    --formats) need_value "$@"; FORMATS_ARG="$2"; shift 2 ;;
    --formats=*) FORMATS_ARG="${1#*=}"; shift ;;
    --std) need_value "$@"; STD_ARG="$2"; shift 2 ;;
    --std=*) STD_ARG="${1#*=}"; shift ;;
    --output-dir) need_value "$@"; OUTPUT_DIR="$2"; shift 2 ;;
    --output-dir=*) OUTPUT_DIR="${1#*=}"; shift ;;
    --version) need_value "$@"; VERSION_ARG="$2"; shift 2 ;;
    --version=*) VERSION_ARG="${1#*=}"; shift ;;
    --tag) need_value "$@"; TAG_ARG="$2"; shift 2 ;;
    --tag=*) TAG_ARG="${1#*=}"; shift ;;
    -j | --jobs | --parallel) need_value "$@"; JOBS="$2"; shift 2 ;;
    --jobs=* | --parallel=*) JOBS="${1#*=}"; shift ;;
    -j[0-9]*) JOBS="${1#-j}"; shift ;;
    --keep-work) KEEP_WORK=1; shift ;;
    --dry-run) DRY_RUN=1; shift ;;
    --quiet) QUIET=1; shift ;;
    --color) need_value "$@"; COLOR_ARG="$2"; shift 2 ;;
    --color=*) COLOR_ARG="${1#*=}"; shift ;;
    --no-werror) WERROR=0; shift ;;
    --use-ninja) USE_NINJA=1; shift ;;
    --no-package) NO_PACKAGE=1; shift ;;
    *) die "unknown parameter: $1 (see --help)" ;;
  esac
done

[[ "${COLOR_ARG}" =~ ^(auto|always|never)$ ]] || die "--color must be auto, always or never, got '${COLOR_ARG}'"
setup_colors "${COLOR_ARG}"
[[ -n "${COMPILERS_ARG}" ]] || { usage >&2; die "--compilers is required"; }
[[ "${JOBS}" =~ ^[1-9][0-9]*$ ]] || die "-j / --jobs / --parallel must be a positive number, got '${JOBS}'"

split_list() { # split_list <comma list> -> one item per line, no empties
  tr ',' '\n' <<<"$1" | sed 's/^[[:space:]]*//; s/[[:space:]]*$//' | sed '/^$/d'
}

mapfile -t ARCHES < <(split_list "${ARCH_ARG}")
mapfile -t FORMATS < <(split_list "${FORMATS_ARG}")
mapfile -t COMPILER_PATHS < <(split_list "${COMPILERS_ARG}")
# --std is a list too; no --std gives one empty item: the default standard.
mapfile -t STD_ITEMS < <(split_list "${STD_ARG}" | awk '!seen[$0]++')
for s in "${STD_ITEMS[@]}"; do
  [[ "$s" =~ ^(11|14|17|20|23)$ ]] || die "--std must be a list of 11, 14, 17, 20, 23, got '${STD_ARG}'"
done
[[ ${#STD_ITEMS[@]} -gt 0 ]] || STD_ITEMS=("")
[[ ${#ARCHES[@]} -gt 0 ]] || die "--arch is empty"
[[ ${#FORMATS[@]} -gt 0 ]] || die "--formats is empty"
[[ ${#COMPILER_PATHS[@]} -gt 0 ]] || die "--compilers is empty"

for a in "${ARCHES[@]}"; do
  [[ "$a" == "x86-64" || "$a" == "x86" ]] || die "unknown ISA '$a' (use x86-64 or x86)"
done
for f in "${FORMATS[@]}"; do
  [[ "$f" =~ ^(deb|rpm|tar\.xz|tar\.gz)$ ]] || die "unknown format '$f' (use deb, rpm, tar.xz, tar.gz)"
done

has_format() {
  local f
  for f in "${FORMATS[@]}"; do [[ "$f" == "$1" ]] && return 0; done
  return 1
}

# ---------------------------------------------------------------------------
# Tools, version, glibc
# ---------------------------------------------------------------------------

for tool in cmake python3; do
  command -v "$tool" >/dev/null || die "'$tool' is not in PATH"
done
if [[ "${USE_NINJA}" -eq 1 ]]; then
  command -v ninja >/dev/null || die "--use-ninja needs 'ninja' in PATH"
elif [[ -z "${CMAKE_GENERATOR:-}" ]]; then
  command -v make >/dev/null || die "'make' is not in PATH (or pass --use-ninja)"
fi
EXTRACTOR="${REPO_ROOT}/Scripts/ReleaseTools/extract_diagnostics.py"
[[ -f "${EXTRACTOR}" ]] || die "missing ${EXTRACTOR}"
if [[ "${NO_PACKAGE}" -eq 0 ]]; then
  for tool in tar readelf; do
    command -v "$tool" >/dev/null || die "'$tool' is not in PATH"
  done
  if has_format tar.xz; then command -v xz >/dev/null || die "tar.xz needs 'xz'"; fi
  if has_format tar.gz; then command -v gzip >/dev/null || die "tar.gz needs 'gzip'"; fi
  if has_format deb; then
    command -v dpkg-deb >/dev/null || die "deb needs 'dpkg-deb'"
    command -v fakeroot >/dev/null || die "deb needs 'fakeroot' (package fakeroot)"
  fi
  if has_format rpm; then
    command -v rpmbuild >/dev/null \
      || die "rpm needs 'rpmbuild': install the package 'rpm' (sudo apt install rpm) or drop rpm from --formats"
  fi
fi

# ---------------------------------------------------------------------------
# Tag mode: build the sources of a tag in a detached worktree
# ---------------------------------------------------------------------------

BUILD_ROOT="${REPO_ROOT}"
TAG_WORKTREE=""
if [[ -n "${TAG_ARG}" ]]; then
  git -C "${REPO_ROOT}" rev-parse --verify --quiet "refs/tags/${TAG_ARG}^{commit}" >/dev/null \
    || die "tag '${TAG_ARG}' is not in this repository; fetch it first (git fetch --tags)"
  if [[ "${DRY_RUN}" -eq 0 ]]; then
    mkdir -p "${OUTPUT_DIR}"
  fi
  OUTPUT_DIR="$(realpath -m "${OUTPUT_DIR}")"
  if [[ "${DRY_RUN}" -eq 0 ]]; then
    mkdir -p "${OUTPUT_DIR}/.work"
  fi
  TAG_WORKTREE="${OUTPUT_DIR}/.work/src-$(printf '%s' "${TAG_ARG}" | tr -c 'A-Za-z0-9._-' '_')"
  [[ ! -e "${TAG_WORKTREE}" ]] \
    || die "the tag worktree path already exists: ${TAG_WORKTREE} (remove it with: git worktree remove --force <path>)"
  # An interrupted earlier run can leave a worktree registration whose
  # directory is gone; git refuses to add one on top of it.
  run_plain git -C "${REPO_ROOT}" worktree prune
  # This script drives the build; the sources, compile.py and the CMakeLists
  # come from the tag, so the packages follow the tag's project(LumexLib
  # VERSION).
  tag_log="${OUTPUT_DIR}/.work/worktree.log"
  run_logged "${tag_log}" git -C "${REPO_ROOT}" worktree add --detach "${TAG_WORKTREE}" "refs/tags/${TAG_ARG}" \
    || die "git worktree add failed for tag '${TAG_ARG}', see ${tag_log}"
  # The submodule comes from this checkout's copy, so a tag build needs no
  # network: its pinned commit is already here. The file transport needs the
  # explicit allow since Git 2.38.1.
  run_logged "${OUTPUT_DIR}/.work/worktree-submodule.log" git -C "${TAG_WORKTREE}" -c protocol.file.allow=always \
    -c "submodule.CMakeRoutines.url=${REPO_ROOT}/CMakeRoutines" \
    submodule update --init --recursive \
    || die "git submodule update failed in the tag worktree, see ${OUTPUT_DIR}/.work/worktree-submodule.log"
  BUILD_ROOT="${TAG_WORKTREE}"
  echo "${C_BOLD}Tag:${C_RESET} ${TAG_ARG} (build tree: ${BUILD_ROOT}; removed at the end unless --keep-work)"
fi

# The text of a file of the tree that is built. A dry run has no tag worktree,
# so it asks git for the file of the tag.
tree_file_text() {
  if [[ "${DRY_RUN}" -eq 1 && -n "${TAG_ARG}" ]]; then
    git -C "${REPO_ROOT}" show "refs/tags/${TAG_ARG}:$1"
  else
    cat "${BUILD_ROOT}/$1"
  fi
}

# project(LumexLib VERSION x.y.z.w) may span several lines.
CMAKE_VERSION_FOUND="$(tree_file_text CMakeLists.txt | tr '\n' ' ' \
  | grep -oE 'project\([[:space:]]*LumexLib[^)]*VERSION[[:space:]]+[0-9]+(\.[0-9]+){1,3}' \
  | grep -oE '[0-9]+(\.[0-9]+){1,3}$' | head -1 || true)"
[[ -n "${CMAKE_VERSION_FOUND}" ]] || die "cannot read project(LumexLib VERSION ...) from CMakeLists.txt"
VERSION="${VERSION_ARG:-${CMAKE_VERSION_FOUND}}"
if [[ -n "${VERSION_ARG}" && "${VERSION_ARG}" != "${CMAKE_VERSION_FOUND}" ]]; then
  warn "--version ${VERSION_ARG} differs from CMakeLists.txt (${CMAKE_VERSION_FOUND}); the libraries keep ${CMAKE_VERSION_FOUND}"
fi

GLIBC_VERSION="$(getconf GNU_LIBC_VERSION 2>/dev/null | awk '{print $2}')"
[[ -n "${GLIBC_VERSION}" ]] || die "cannot read the glibc version (getconf GNU_LIBC_VERSION)"

# ---------------------------------------------------------------------------
# Compilers
# ---------------------------------------------------------------------------

# pair_for <path> -> prints "<c compiler> <c++ compiler>"; the partner is
# looked up in the directory of the given path under the matching name.
pair_for() {
  local path="$1" dir name c cxx
  dir="$(dirname "$path")"
  name="$(basename "$path")"
  case "$name" in
    *clang++*) cxx="$path"; c="${dir}/${name/clang++/clang}" ;;
    *g++*) cxx="$path"; c="${dir}/${name/g++/gcc}" ;;
    c++) cxx="$path"; c="${dir}/cc" ;;
    *clang*) c="$path"; cxx="${dir}/${name/clang/clang++}" ;;
    *gcc*) c="$path"; cxx="${dir}/${name/gcc/g++}" ;;
    cc) c="$path"; cxx="${dir}/c++" ;;
    *) die "cannot tell whether '$path' is a C or a C++ compiler (expected gcc/g++/clang/clang++/cc/c++ in the name)" ;;
  esac
  [[ -x "$c" ]] || die "C compiler for '$path' not found: expected '$c'"
  [[ -x "$cxx" ]] || die "C++ compiler for '$path' not found: expected '$cxx'"
  echo "$c $cxx"
}

# is_mingw <compiler> -> success when the compiler targets Windows (MinGW-w64)
is_mingw() { [[ "$("$1" -dumpmachine 2>/dev/null)" == *mingw32* ]]; }

# compiler_label <compiler> -> gcc13.2.0 / clang23.1.0 / mingw8.3.0 (from the
# predefined macros and the target)
compiler_label() {
  local macros
  macros="$("$1" -dM -E -x c /dev/null 2>/dev/null)" || die "'$1' does not run"
  macro() { awk -v m="$1" '$1 == "#define" && $2 == m {print $3}' <<<"$macros"; }
  if [[ -n "$(macro __clang__)" ]]; then
    is_mingw "$1" && die "'$1' is Clang for MinGW; only GCC is supported as a MinGW compiler"
    echo "clang$(macro __clang_major__).$(macro __clang_minor__).$(macro __clang_patchlevel__)"
  elif [[ -n "$(macro __GNUC__)" ]]; then
    local family="gcc"
    is_mingw "$1" && family="mingw"
    echo "${family}$(macro __GNUC__).$(macro __GNUC_MINOR__).$(macro __GNUC_PATCHLEVEL__)"
  else
    die "'$1' is neither GCC nor Clang"
  fi
}

# check_mingw <c++ compiler> <isa> -> the checks that only a MinGW job needs
check_mingw() {
  local cxx="$1" isa="$2" model
  [[ "$isa" == "x86-64" ]] || die "'$cxx' builds 64-bit Windows code only; drop x86 from --arch for it"
  model="$("$cxx" -v 2>&1 | sed -n 's/^Thread model: //p' | head -1)"
  [[ "$model" == "posix" ]] \
    || die "'$cxx' uses the '${model:-unknown}' thread model; the library needs std::thread, so pass the posix compiler (x86_64-w64-mingw32-g++-posix)"
  if [[ "${NO_PACKAGE}" -eq 0 ]]; then
    command -v zip >/dev/null || die "a MinGW build is packed with 'zip' (package zip)"
  fi
}

# mingw_tool <c++ compiler> <suffix> -> a binutils tool of the same triple
# (windres, objdump): next to the compiler, else in PATH.
mingw_tool() {
  local cxx="$1" suffix="$2" triple
  triple="$("$cxx" -dumpmachine)"
  if [[ -x "$(dirname "$cxx")/${triple}-${suffix}" ]]; then
    echo "$(dirname "$cxx")/${triple}-${suffix}"
  else
    command -v "${triple}-${suffix}" || true
  fi
}

# write_mingw_toolchain <file> <c> <c++> -> a CMake toolchain file for the cross build
write_mingw_toolchain() {
  local file="$1" c="$2" cxx="$3" triple windres sysroot
  triple="$("$cxx" -dumpmachine)"
  windres="$(mingw_tool "$cxx" windres)"
  [[ -n "$windres" ]] || die "no ${triple}-windres next to '$cxx' (package binutils-mingw-w64); the version resources need it"
  sysroot=""
  [[ -d "/usr/${triple}" ]] && sysroot="/usr/${triple}"
  cat >"$file" <<TOOLCHAIN
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)
set(CMAKE_C_COMPILER "${c}")
set(CMAKE_CXX_COMPILER "${cxx}")
set(CMAKE_RC_COMPILER "${windres}")
set(CMAKE_FIND_ROOT_PATH "${sysroot}")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
TOOLCHAIN
}

arch_flags() { [[ "$1" == "x86" ]] && echo "-m32" || true; }

# probe_link <c++ compiler> <isa> -> links and runs a small C++ program
probe_link() {
  local cxx="$1" isa="$2" dir flags
  dir="$(mktemp -d)"
  flags="$(arch_flags "$isa")"
  printf '#include <string>\n#include <iostream>\nint main () { std::string s ("ok"); std::cout << s.size () << "\\n"; }\n' >"${dir}/probe.cpp"
  # shellcheck disable=SC2086
  if ! "$cxx" $flags "${dir}/probe.cpp" -o "${dir}/probe" >"${dir}/log" 2>&1; then
    local reason
    reason="$(grep -m1 -E 'error|cannot find|No such' "${dir}/log" || true)"
    rm -rf "$dir"
    if [[ "$isa" == "x86" ]]; then
      die "'$cxx' cannot build 32-bit (x86) programs: ${reason:-link failed}. Install its 32-bit glibc and C++ library (multilib) or drop x86 from --arch"
    fi
    die "'$cxx' cannot build a C++ program: ${reason:-link failed}"
  fi
  rm -rf "$dir"
}

# std_for <c++ compiler> <isa> <std from --std or empty> -> the
# -DCMAKE_CXX_STANDARD value, or empty
std_for() {
  local cxx="$1" isa="$2" asked="$3" flags
  if [[ -n "${asked}" ]]; then echo "${asked}"; return; fi
  flags="$(arch_flags "$isa")"
  # shellcheck disable=SC2086
  if printf '#if __cplusplus < 202002L\n#error no C++20\n#endif\n' \
    | "$cxx" $flags -std=c++20 -x c++ -fsyntax-only - >/dev/null 2>&1; then
    echo 20
  fi
}

# std_note <c++ compiler> <isa> <std> -> a note when the standard asked for is
# only a draft there (GCC 8: -std=c++20 does not exist, -std=c++2a does)
std_note() {
  local cxx="$1" isa="$2" std="$3" flags draft macro
  case "$std" in 20) draft=c++2a ;; 23) draft=c++2b ;; *) return 0 ;; esac
  flags="$(arch_flags "$isa")"
  # shellcheck disable=SC2086
  if "$cxx" $flags -std="c++${std}" -x c++ -fsyntax-only /dev/null >/dev/null 2>&1; then
    return 0
  fi
  # shellcheck disable=SC2086
  macro="$("$cxx" $flags -std="${draft}" -x c++ -dM -E /dev/null 2>/dev/null | awk '$2 == "__cplusplus" {print $3}')"
  [[ -n "$macro" ]] || die "'$cxx' knows neither -std=c++${std} nor -std=${draft}"
  echo "C++${std} is -std=${draft} here (__cplusplus ${macro}); the library sees the older standard"
}

# std_tag_for <c++ compiler> <isa> <std> -> the standard of the build as it goes
# into the package name: 11, 14, 17, 20, 23; 2a or 2b where only the draft
# exists; with no standard given, the one the compiler uses by default.
std_tag_for() {
  local cxx="$1" isa="$2" std="$3" flags macro
  flags="$(arch_flags "$isa")"
  if [[ -n "$std" ]]; then
    if [[ "$std" == 20 || "$std" == 23 ]]; then
      # shellcheck disable=SC2086
      if ! "$cxx" $flags -std="c++${std}" -x c++ -fsyntax-only /dev/null >/dev/null 2>&1; then
        [[ "$std" == 20 ]] && echo 2a || echo 2b
        return
      fi
    fi
    echo "$std"
    return
  fi
  # shellcheck disable=SC2086
  macro="$("$cxx" $flags -x c++ -dM -E /dev/null 2>/dev/null | awk '$2 == "__cplusplus" {print $3}')"
  case "$macro" in
    199711L) echo 98 ;;
    201103L) echo 11 ;;
    201402L) echo 14 ;;
    201703L) echo 17 ;;
    202002L) echo 20 ;;
    202302L) echo 23 ;;
    *) die "cannot tell the default C++ standard of '$cxx' (__cplusplus ${macro:-unknown}); pass --std" ;;
  esac
}

declare -a JOB_C JOB_CXX JOB_LABEL JOB_ISA JOB_STD JOB_KIND JOB_NOTE JOB_STDTAG
for path in "${COMPILER_PATHS[@]}"; do
  [[ -x "$path" ]] || die "compiler '$path' does not exist or is not executable"
  pair="$(pair_for "$path")"
  c="${pair% *}"; cxx="${pair#* }"
  label="$(compiler_label "$cxx")"
  c_label="$(compiler_label "$c")"
  [[ "$label" == "$c_label" ]] || die "'$c' ($c_label) and '$cxx' ($label) are different compilers"
  for isa in "${ARCHES[@]}"; do
    if [[ "$isa" == "x86-64" ]]; then
      predefined="$("$cxx" -dM -E -x c++ /dev/null)"
      [[ "$predefined" == *__x86_64__* ]] || die "'$cxx' does not target x86-64"
    fi
    kind="linux"
    if is_mingw "$cxx"; then
      kind="windows"
      check_mingw "$cxx" "$isa"
    fi
    probe_link "$cxx" "$isa"
    for std_item in "${STD_ITEMS[@]}"; do
      JOB_C+=("$c"); JOB_CXX+=("$cxx"); JOB_LABEL+=("$label"); JOB_ISA+=("$isa")
      JOB_KIND+=("$kind")
      job_std="$(std_for "$cxx" "$isa" "$std_item")"
      JOB_STD+=("${job_std}")
      JOB_STDTAG+=("$(std_tag_for "$cxx" "$isa" "${job_std}")")
      JOB_NOTE+=("$([[ -n "${std_item}" ]] && std_note "$cxx" "$isa" "${std_item}" || true)")
    done
  done
done

# ---------------------------------------------------------------------------
# Build and package
# ---------------------------------------------------------------------------

if [[ "${DRY_RUN}" -eq 0 ]]; then
  mkdir -p "${OUTPUT_DIR}"
fi
OUTPUT_DIR="$(realpath -m "${OUTPUT_DIR}")"
WORK_ROOT="${OUTPUT_DIR}/.work"
WARN_DIR="${OUTPUT_DIR}/warns"
if [[ "${DRY_RUN}" -eq 0 ]]; then
  mkdir -p "${WARN_DIR}"
fi
SECONDS=0
ARTIFACTS=()
SUMMARY=()
FAILED_JOBS=()

# The compilers' messages are parsed for the logs of warns/: keep them in English.
export LC_ALL=C

echo "${C_BOLD}LumexLib ${VERSION}${C_RESET}, glibc ${GLIBC_VERSION}, formats: ${FORMATS[*]}$([[ "${NO_PACKAGE}" -eq 1 ]] && echo ', no packages')$([[ "${WERROR}" -eq 1 ]] && echo ', -Werror')"
if [[ "${DRY_RUN}" -eq 1 ]]; then
  echo "${C_YELLOW}Dry run:${C_RESET} the commands are shown, nothing is built, installed or written."
fi
for i in "${!JOB_CXX[@]}"; do
  printf '  %s%-12s%s %-7s C++%-8s %s%s / %s%s\n' "${C_BOLD}" "${JOB_LABEL[$i]}" "${C_RESET}" "${JOB_ISA[$i]}" "${JOB_STD[$i]:-default}" \
    "${C_DIM}" "${JOB_C[$i]}" "${JOB_CXX[$i]}" "${C_RESET}"
  [[ -z "${JOB_NOTE[$i]}" ]] || echo "    ${C_YELLOW}note:${C_RESET} ${JOB_NOTE[$i]}"
done

remove_x64() {
  # Build output published into the checkout must not reach a later package.
  [[ "${DRY_RUN}" -eq 1 ]] || rm -rf "${BUILD_ROOT}/x64"
}

# cleanup_work <dir>: the scratch directory of one build goes away unless
# --keep-work (a dry run never made it).
cleanup_work() {
  [[ "${DRY_RUN}" -eq 1 || "${KEEP_WORK}" -eq 1 ]] || rm -rf "$1"
}

# run_in <dir> <command...>: run_plain in another directory.
run_in() {
  local dir="$1"
  shift
  note "in ${dir}"
  show_cmd "$@"
  [[ "${DRY_RUN}" -eq 1 ]] && return 0
  (cd "${dir}" && "$@")
}

# result_line <package>: one line per package made (or, in a dry run, to be made).
result_line() {
  echo "  ${C_GREEN}->${C_RESET} $(basename "$1")$([[ "${DRY_RUN}" -eq 1 ]] && echo " ${C_DIM}(would be created)${C_RESET}")"
}

# diag_color <warnings> <errors>: green when clean, red with errors, yellow otherwise.
diag_color() {
  if [[ "$2" -gt 0 ]]; then
    echo "${C_RED}"
  elif [[ "$1" -gt 0 ]]; then
    echo "${C_YELLOW}"
  else
    echo "${C_GREEN}"
  fi
}

# stage_runtime <build dir> <c++ compiler> <isa> <lib dir>
# Runs cmake/PublishDistr.cmake (CopyRuntimeDependencies + the compiler's own
# runtime files) into a scratch directory and keeps the compiler runtime that
# does not come from the system library directories.
stage_runtime() {
  local build="$1" cxx="$2" isa="$3" libdir="$4" distr flags file name origin
  if [[ "${DRY_RUN}" -eq 1 ]]; then
    distr="/tmp/tmp.XXXXXXXXXX"
  else
    distr="$(mktemp -d)"
  fi
  flags="$(arch_flags "$isa")"
  run_logged "${build}/publish-runtime.log" cmake -Dbin_dir="${build}/bin" -Ddistr_dir="${distr}" \
    -Dcopy_runtime_script="${BUILD_ROOT}/CMakeRoutines/deployment/CopyRuntimeDependencies.cmake" \
    -Dcxx_compiler="${cxx}" -P "${BUILD_ROOT}/cmake/PublishDistr.cmake" \
    || die "cmake/PublishDistr.cmake failed, see ${build}/publish-runtime.log"
  if [[ "${DRY_RUN}" -eq 1 ]]; then
    note "then the compiler's runtime libraries that are not system ones (libstdc++, libgcc_s, libc++) are copied to the lib directory of the package"
    return 0
  fi
  for file in "${distr}"/*; do
    [[ -e "$file" ]] || continue
    name="$(basename "$file")"
    case "$name" in libLumex* | *.json | *.txt) continue ;; esac
    # shellcheck disable=SC2086
    origin="$(readlink -f "$("$cxx" $flags -print-file-name="$name")" 2>/dev/null || true)"
    # A name the compiler does not know was resolved through the system
    # loader paths: that is the system runtime.
    [[ "$origin" == /* && -e "$origin" ]] || continue
    case "$origin" in
      /lib/* | /usr/lib/* | /lib32/* | /lib64/* | /usr/lib32/* | /usr/lib64/*) continue ;;
    esac
    cp -a "$file" "${libdir}/"
    echo "    ${C_DIM}runtime: ${name}${origin:+ (from ${origin})}${C_RESET}"
  done
  rm -rf "${distr}"
}

# stage_mingw_runtime <c++ compiler> <bin dir>
# Copies the MinGW runtime DLLs the Lumex DLLs need (libstdc++-6, libgcc_s_seh-1,
# libwinpthread-1, ...) next to them. Every DLL is read with objdump and every
# lib*.dll it names that is not in the directory is fetched from the compiler
# (-print-file-name) until nothing is missing. Windows system DLLs have no
# "lib" prefix, so they are left alone.
stage_mingw_runtime() {
  local cxx="$1" bindir="$2" objdump pass dll dep found settled
  if [[ "${DRY_RUN}" -eq 1 ]]; then
    note "then the MinGW runtime DLLs the Lumex DLLs need (libstdc++, libgcc_s, libwinpthread) are copied next to them"
    return 0
  fi
  objdump="$(mingw_tool "$cxx" objdump)"
  [[ -n "$objdump" ]] || die "no objdump of the MinGW triple next to '$cxx' (package binutils-mingw-w64)"
  for pass in 1 2 3 4 5 6; do
    settled=1
    for dll in "${bindir}"/*.dll; do
      while read -r dep; do
        case "$dep" in lib*.dll) ;; *) continue ;; esac
        [[ -e "${bindir}/${dep}" ]] && continue
        found="$("$cxx" -print-file-name="${dep}")"
        [[ -f "$found" ]] || die "cannot find ${dep} (needed by $(basename "$dll")) for '$cxx'"
        cp "$found" "${bindir}/"
        echo "    ${C_DIM}runtime: ${dep} (from ${found})${C_RESET}"
        settled=0
      done < <("$objdump" -p "$dll" | sed -n 's/^[[:space:]]*DLL Name: //p')
    done
    [[ "$settled" -eq 1 ]] && return 0
  done
  die "the MinGW runtime of '$cxx' does not settle after 6 passes"
}

write_deb() { # write_deb <stage root> <package name> <isa> <out file>
  local stage="$1" pkg="$2" isa="$3" out="$4" debarch size
  if [[ "${DRY_RUN}" -eq 1 ]]; then
    show_cmd fakeroot dpkg-deb --build "${stage}" "${out}"
    return 0
  fi
  debarch="amd64"; [[ "$isa" == "x86" ]] && debarch="i386"
  size="$(du -sk "${stage}" | awk '{print $1}')"
  mkdir -p "${stage}/DEBIAN"
  cat >"${stage}/DEBIAN/control" <<EOF
Package: ${pkg}
Version: ${VERSION}
Architecture: ${debarch}
Maintainer: Vladislav Semykin <vladislav.semykin@gmail.com>
Installed-Size: ${size}
Depends: libc6 (>= ${GLIBC_VERSION})
Section: libdevel
Priority: optional
Description: LumexLib C++ utility library (${pkg#lumexlib-})
 Shared libraries, headers and CMake package files of LumexLib ${VERSION},
 built with ${pkg#lumexlib-}, installed under ${INSTALL_PREFIX}.
EOF
  show_cmd fakeroot dpkg-deb --build "${stage}" "${out}"
  fakeroot dpkg-deb --build "${stage}" "${out}" >/dev/null
  rm -rf "${stage}/DEBIAN"
}

write_rpm() { # write_rpm <stage root> <package name> <isa> <out file> <work dir>
  local stage="$1" pkg="$2" isa="$3" out="$4" work="$5" rpmarch built
  rpmarch="x86_64"; [[ "$isa" == "x86" ]] && rpmarch="i686"
  if [[ "${DRY_RUN}" -eq 1 ]]; then
    show_cmd rpmbuild -bb --target "${rpmarch}" --define "_topdir ${work}/rpm" "${work}/rpm/SPECS/${pkg}.spec"
    return 0
  fi
  mkdir -p "${work}/rpm/SPECS" "${work}/rpm/RPMS"
  cat >"${work}/rpm/SPECS/${pkg}.spec" <<EOF
Name: ${pkg}
Version: ${VERSION}
Release: 1
Summary: LumexLib C++ utility library (${pkg#lumexlib-})
License: MIT
AutoReqProv: no
Requires: glibc >= ${GLIBC_VERSION}
%define debug_package %{nil}
%define __os_install_post %{nil}
%define _build_id_links none

%description
Shared libraries, headers and CMake package files of LumexLib ${VERSION},
built with ${pkg#lumexlib-}, installed under ${INSTALL_PREFIX}.

%install
mkdir -p %{buildroot}
cp -a "${stage}/." %{buildroot}/

%files
%defattr(-,root,root,-)
${INSTALL_PREFIX}
EOF
  run_logged "${work}/rpmbuild.log" rpmbuild -bb --target "${rpmarch}" --define "_topdir ${work}/rpm" \
    "${work}/rpm/SPECS/${pkg}.spec" \
    || die "rpmbuild failed, see ${work}/rpmbuild.log"
  built="$(find "${work}/rpm/RPMS" -name '*.rpm' | head -1)"
  [[ -n "$built" ]] || die "rpmbuild produced no package, see ${work}/rpmbuild.log"
  mv "$built" "${out}"
}

for i in "${!JOB_CXX[@]}"; do
  c="${JOB_C[$i]}"; cxx="${JOB_CXX[$i]}"; label="${JOB_LABEL[$i]}"; isa="${JOB_ISA[$i]}"; std="${JOB_STD[$i]}"
  kind="${JOB_KIND[$i]}"; stdtag="${JOB_STDTAG[$i]}"
  isa_tag=""; [[ "$isa" == "x86" ]] && isa_tag="_x86"
  tag="${label}${isa_tag}_c++${std:-default}"
  warn_log="${WARN_DIR}/${tag}_warn.log"
  err_log="${WARN_DIR}/${tag}_err.log"
  [[ "${DRY_RUN}" -eq 1 ]] || rm -f "${warn_log}" "${err_log}"
  if [[ "$kind" == "windows" ]]; then
    base="LumexLib-${VERSION}_win_x64_${label}_cxx${stdtag}"
    INSTALL_PREFIX="/LumexLib"
  else
    base="LumexLib-${VERSION}_linux_${isa}_${label}_glibc${GLIBC_VERSION}_cxx${stdtag}"
    INSTALL_PREFIX="/opt/LumexLib/${VERSION}_${label}"
  fi
  work="${WORK_ROOT}/${label}_${isa}_cxx${stdtag}"
  build="${work}/build"
  stage="${work}/stage"
  if [[ "${DRY_RUN}" -eq 0 ]]; then
    rm -rf "${work}"
    mkdir -p "${build}" "${stage}"
  fi
  echo
  echo "${C_BOLD}== [$((i + 1))/${#JOB_CXX[@]}] ${base} (C++${std:-default})${C_RESET}"

  flags="$(arch_flags "$isa")"
  cmake_args=()
  [[ "${USE_NINJA}" -eq 0 ]] || cmake_args+=(-G Ninja)
  cmake_args+=(
    -S "${BUILD_ROOT}" -B "${build}" --no-warn-unused-cli
    -DCMAKE_BUILD_TYPE=Release
    -DLUMEX_BUILD_SHARED_LIBS=ON -DLUMEX_BUILD_TESTS=OFF -DLUMEX_BUILD_DOCUMENTATION=OFF
    -DLUMEX_INSTALL=ON
    "-DCMAKE_INSTALL_PREFIX=${INSTALL_PREFIX}" -DCMAKE_INSTALL_LIBDIR=lib
  )
  if [[ "$kind" == "windows" ]]; then
    if [[ "${DRY_RUN}" -eq 1 ]]; then
      # Still checks that the tools are there; the file itself is only described.
      write_mingw_toolchain /dev/null "${c}" "${cxx}"
      note "mingw-toolchain.cmake (compilers, windres, find-root path) is written to the build's work directory"
    else
      write_mingw_toolchain "${work}/mingw-toolchain.cmake" "${c}" "${cxx}"
    fi
    cmake_args+=("-DCMAKE_TOOLCHAIN_FILE=${work}/mingw-toolchain.cmake")
  else
    cmake_args+=("-DCMAKE_C_COMPILER=${c}" "-DCMAKE_CXX_COMPILER=${cxx}" '-DCMAKE_INSTALL_RPATH=$ORIGIN')
  fi
  [[ "${WERROR}" -eq 1 ]] && cmake_args+=(-DLUMEX_WERROR=ON)
  [[ -n "$std" ]] && cmake_args+=("-DCMAKE_CXX_STANDARD=${std}" -DCMAKE_CXX_STANDARD_REQUIRED=ON)
  [[ -n "$flags" ]] && cmake_args+=("-DCMAKE_C_FLAGS=${flags}" "-DCMAKE_CXX_FLAGS=${flags}")
  if ! run_logged "${work}/configure.log" cmake "${cmake_args[@]}"; then
    tail -n 40 "${work}/configure.log" >"${err_log}"
    echo "  ${C_RED}configure FAILED${C_RESET}: see ${err_log}"
    SUMMARY+=("${tag}: ${C_RED}configure failed${C_RESET}")
    FAILED_JOBS+=("${tag}")
    cleanup_work "${work}"
    continue
  fi

  # The generator that configured the tree decides how its targets are listed
  # and how the build tool keeps going after an error.
  if [[ "${DRY_RUN}" -eq 1 ]]; then
    generator="$([[ "${USE_NINJA}" -eq 1 ]] && echo Ninja || echo "${CMAKE_GENERATOR:-Unix Makefiles}")"
  else
    generator="$(sed -n 's/^CMAKE_GENERATOR:INTERNAL=//p' "${build}/CMakeCache.txt")"
  fi
  # Library targets only: publish_distr and lumex_copy_compile_commands
  # write into the checkout (x64/, compile_commands.json); package and
  # package_source run CPack, which a library build does not need.
  not_library='^(all|clean|depend|help|edit_cache|rebuild_cache|install|install/local|install/strip|list_install_components|package|package_source|publish_distr|lumex_copy_compile_commands|generate_documentation|test)$'
  case "${generator}" in
    Ninja*)
      # -k 0: keep going after an error, so one run lists every diagnostic.
      build_tool_args=(-k 0)
      if [[ "${DRY_RUN}" -eq 1 ]]; then
        targets=("<library-targets>")
      else
        mapfile -t targets < <(cd "${build}" && ninja -t targets all | grep -oE '^[A-Za-z0-9_]+: phony' | sed 's/: phony//' \
          | grep -vE "${not_library}" | grep -v '^cmake_object_order_depends_target_' | sort -u)
      fi
      ;;
    *Makefiles)
      # -O target: a job's output is printed whole, parallel jobs do not
      # interleave the lines of a diagnostic.
      build_tool_args=(-k -Otarget)
      if [[ "${DRY_RUN}" -eq 1 ]]; then
        targets=("<library-targets>")
      else
        mapfile -t targets < <(cmake --build "${build}" --target help | sed -n 's/^\.\.\. \([A-Za-z0-9_]*\)$/\1/p' \
          | grep -vE "${not_library}" | sort -u)
      fi
      ;;
    *) die "the generator '${generator}' is not supported: use --use-ninja or Unix Makefiles" ;;
  esac
  [[ "${DRY_RUN}" -eq 1 || ${#targets[@]} -gt 0 ]] || die "no targets found in ${build}"
  build_rc=0
  run_logged "${work}/build.log" cmake --build "${build}" --parallel "${JOBS}" --target "${targets[@]}" -- "${build_tool_args[@]}" \
    || build_rc=$?
  if [[ "${DRY_RUN}" -eq 1 ]]; then
    note "warnings and errors are sorted into ${WARN_DIR}/${tag}_warn.log and _err.log, written only when there are any"
  else
    extract_args=()
    [[ "${build_rc}" -eq 0 ]] || extract_args+=(--failed)
    counts="$(python3 "${EXTRACTOR}" "${work}/build.log" --warn-out "${warn_log}" --err-out "${err_log}" "${extract_args[@]}")"
    n_warn="${counts#warnings=}"; n_warn="${n_warn%% *}"
    n_err="${counts##* errors=}"
    dcolor="$(diag_color "${n_warn}" "${n_err}")"
    SUMMARY+=("${tag}: ${dcolor}${n_warn} warning(s), ${n_err} error(s)${C_RESET}")
    echo "  ${dcolor}${n_warn} warning(s), ${n_err} error(s)${C_RESET}"
    if [[ "${build_rc}" -ne 0 ]]; then
      # -Werror turns warnings into the errors of a build: then the warnings are the news.
      if [[ -f "${err_log}" ]]; then
        echo "  ${C_RED}build FAILED${C_RESET}: see ${err_log}"
      else
        echo "  ${C_RED}build FAILED because of the warnings${C_RESET}: see ${warn_log}"
      fi
      FAILED_JOBS+=("${tag}")
      cleanup_work "${work}"
      continue
    fi
  fi

  if [[ "${NO_PACKAGE}" -eq 1 ]]; then
    cleanup_work "${work}"
    continue
  fi

  run_logged "${work}/install.log" env DESTDIR="${stage}" cmake --install "${build}" \
    || die "install failed, see ${work}/install.log"

  prefix_dir="${stage}${INSTALL_PREFIX}"
  if [[ "$kind" == "windows" ]]; then
    [[ "${DRY_RUN}" -eq 1 || -d "${prefix_dir}/bin" ]] || die "install produced no ${INSTALL_PREFIX}/bin"
    stage_mingw_runtime "${cxx}" "${prefix_dir}/bin"
    out="${OUTPUT_DIR}/${base}.zip"
    tree="${work}/zip/${base}"
    if [[ "${DRY_RUN}" -eq 0 ]]; then
      rm -f "${out}"
      rm -rf "${work}/zip"; mkdir -p "${tree}"
      cp -a "${prefix_dir}/." "${tree}/"
    fi
    run_in "${work}/zip" zip -qrX "${out}" "${base}"
    [[ "${DRY_RUN}" -eq 1 ]] || rm -rf "${work}/zip"
    remove_x64
    ARTIFACTS+=("${out}")
    result_line "${out}"
    cleanup_work "${work}"
    continue
  fi

  [[ "${DRY_RUN}" -eq 1 || -d "${prefix_dir}/lib" ]] || die "install produced no ${INSTALL_PREFIX}/lib"
  stage_runtime "${build}" "${cxx}" "${isa}" "${prefix_dir}/lib"

  pkg="lumexlib-${label}"
  for fmt in "${FORMATS[@]}"; do
    out="${OUTPUT_DIR}/${base}.${fmt}"
    [[ "${DRY_RUN}" -eq 1 ]] || rm -f "${out}"
    case "$fmt" in
      tar.gz | tar.xz)
        tree="${work}/tar/${base}"
        taropt="-czf"; [[ "$fmt" == "tar.xz" ]] && taropt="-cJf"
        if [[ "${DRY_RUN}" -eq 0 ]]; then
          rm -rf "${work}/tar"; mkdir -p "${tree}"
          cp -a "${prefix_dir}/." "${tree}/"
        fi
        run_plain tar --owner=0 --group=0 --numeric-owner -C "${work}/tar" "${taropt}" "${out}" "${base}"
        [[ "${DRY_RUN}" -eq 1 ]] || rm -rf "${work}/tar"
        ;;
      deb) write_deb "${stage}" "${pkg}" "${isa}" "${out}" ;;
      rpm) write_rpm "${stage}" "${pkg}" "${isa}" "${out}" "${work}" ;;
    esac
    remove_x64
    ARTIFACTS+=("${out}")
    result_line "${out}"
  done

  cleanup_work "${work}"
done
echo
if [[ -n "${TAG_WORKTREE}" && "${DRY_RUN}" -eq 0 ]]; then
  if [[ "${KEEP_WORK}" -eq 1 ]]; then
    echo "Tag worktree kept (--keep-work): ${TAG_WORKTREE}"
  else
    git -C "${REPO_ROOT}" worktree remove --force "${TAG_WORKTREE}" 2>/dev/null \
      || warn "could not remove the tag worktree ${TAG_WORKTREE}; remove it with: git worktree remove --force <path>; git worktree prune"
    git -C "${REPO_ROOT}" worktree prune
    rm -f "${OUTPUT_DIR}/.work/worktree.log" "${OUTPUT_DIR}/.work/worktree-submodule.log"
  fi
fi
[[ "${DRY_RUN}" -eq 1 || "${KEEP_WORK}" -eq 1 ]] || rmdir "${WORK_ROOT}" 2>/dev/null || true

# A release needs a dated [vX.Y.Z.W] section in CHANGELOG.md.
# The text goes through a variable: grep -q would close the pipe early and
# pipefail would turn the SIGPIPE of the reader into a "no match".
changelog_text="$(tree_file_text CHANGELOG.md 2>/dev/null || true)"
if grep -qE "^## \[v${VERSION//./\\.}\].*в разработке" <<<"${changelog_text}"; then
  warn "CHANGELOG.md section [v${VERSION}] is still marked as in development (not dated)"
fi

if [[ "${DRY_RUN}" -eq 1 ]]; then
  echo "${C_BOLD}Dry run finished:${C_RESET} ${#JOB_CXX[@]} build(s) shown$([[ "${NO_PACKAGE}" -eq 1 ]] || echo ", ${#ARTIFACTS[@]} package(s) would be created in ${OUTPUT_DIR}")."
  if [[ ${#ARTIFACTS[@]} -gt 0 ]]; then
    printf '  %s\n' "${ARTIFACTS[@]##*/}"
  fi
  exit 0
fi

echo "${C_BOLD}Diagnostics${C_RESET} (logs of the builds with something to report are in ${WARN_DIR}):"
printf '  %s\n' "${SUMMARY[@]}"
if [[ ${#ARTIFACTS[@]} -gt 0 ]]; then
  echo "${C_BOLD}Packages${C_RESET} in ${OUTPUT_DIR}:"
  printf '  %s\n' "${ARTIFACTS[@]##*/}"
fi
h=$((SECONDS / 3600)); m=$(((SECONDS % 3600) / 60)); s=$((SECONDS % 60))
printf "Total time: %02dh %02dm %02ds\n" "$h" "$m" "$s"
if [[ ${#FAILED_JOBS[@]} -gt 0 ]]; then
  echo "${E_RED}Failed builds:${E_RESET} ${FAILED_JOBS[*]}" >&2
  exit 1
fi
