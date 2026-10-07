#!/usr/bin/env bash
# create_release.sh - build LumexLib with every given compiler and ISA and
# package each build as
#   LumexLib-<version>_linux_<ISA>_<compiler>_glibc<glibc>.<deb|rpm|tar.xz|tar.gz>
# or, for a MinGW cross compiler,
#   LumexLib-<version>_win_x64_<compiler>.zip
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
  --std N            C++ standard for every build (11, 14, 17, 20, 23).
                     Default: 20 where the compiler supports it (probe:
                     -std=c++20 gives __cplusplus >= 202002L), otherwise the
                     compiler's default standard. GCC 8 and MinGW 8.3 have no
                     C++20 (only the draft -std=c++2a, __cplusplus 201709L):
                     --std 20 builds that draft there and says so.
  --no-werror        Build without -Werror. By default the script passes
                     -DLUMEX_WERROR=ON, so a warning fails the build. A --tag
                     older than 2.0.0.0 has no such option and builds as it
                     was released.
  --no-package       Build only: no install, no packages, no packaging tools
                     needed. The warnings of the build are still collected.
                     Run it once per --std to check all four standards.
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
  --jobs N           Parallel build jobs (default: nproc).
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

die() {
  echo "Error: $*" >&2
  exit 1
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
    --jobs) need_value "$@"; JOBS="$2"; shift 2 ;;
    --jobs=*) JOBS="${1#*=}"; shift ;;
    --keep-work) KEEP_WORK=1; shift ;;
    --no-werror) WERROR=0; shift ;;
    --no-package) NO_PACKAGE=1; shift ;;
    *) die "unknown parameter: $1 (see --help)" ;;
  esac
done

[[ -n "${COMPILERS_ARG}" ]] || { usage >&2; die "--compilers is required"; }
[[ "${JOBS}" =~ ^[1-9][0-9]*$ ]] || die "--jobs must be a positive number, got '${JOBS}'"
if [[ -n "${STD_ARG}" && ! "${STD_ARG}" =~ ^(11|14|17|20|23)$ ]]; then
  die "--std must be one of 11, 14, 17, 20, 23, got '${STD_ARG}'"
fi

split_list() { # split_list <comma list> -> one item per line, no empties
  tr ',' '\n' <<<"$1" | sed 's/^[[:space:]]*//; s/[[:space:]]*$//' | sed '/^$/d'
}

mapfile -t ARCHES < <(split_list "${ARCH_ARG}")
mapfile -t FORMATS < <(split_list "${FORMATS_ARG}")
mapfile -t COMPILER_PATHS < <(split_list "${COMPILERS_ARG}")
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

for tool in cmake ninja python3; do
  command -v "$tool" >/dev/null || die "'$tool' is not in PATH"
done
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
  mkdir -p "${OUTPUT_DIR}"
  OUTPUT_DIR="$(cd "${OUTPUT_DIR}" && pwd)"
  mkdir -p "${OUTPUT_DIR}/.work"
  TAG_WORKTREE="${OUTPUT_DIR}/.work/src-$(printf '%s' "${TAG_ARG}" | tr -c 'A-Za-z0-9._-' '_')"
  [[ ! -e "${TAG_WORKTREE}" ]] \
    || die "the tag worktree path already exists: ${TAG_WORKTREE} (remove it with: git worktree remove --force <path>)"
  # An interrupted earlier run can leave a worktree registration whose
  # directory is gone; git refuses to add one on top of it.
  git -C "${REPO_ROOT}" worktree prune
  # This script drives the build; the sources, compile.py and the CMakeLists
  # come from the tag, so the packages follow the tag's project(LumexLib
  # VERSION).
  git -C "${REPO_ROOT}" worktree add --detach "${TAG_WORKTREE}" "refs/tags/${TAG_ARG}" \
    >"${OUTPUT_DIR}/.work/worktree.log" 2>&1 \
    || die "git worktree add failed for tag '${TAG_ARG}', see ${OUTPUT_DIR}/.work/worktree.log"
  # The submodule comes from this checkout's copy, so a tag build needs no
  # network: its pinned commit is already here. The file transport needs the
  # explicit allow since Git 2.38.1.
  git -C "${TAG_WORKTREE}" -c protocol.file.allow=always \
    -c "submodule.CMakeRoutines.url=${REPO_ROOT}/CMakeRoutines" \
    submodule update --init --recursive >>"${OUTPUT_DIR}/.work/worktree.log" 2>&1 \
    || die "git submodule update failed in the tag worktree, see ${OUTPUT_DIR}/.work/worktree.log"
  BUILD_ROOT="${TAG_WORKTREE}"
  echo "Tag: ${TAG_ARG} (build tree: ${BUILD_ROOT}; removed at the end unless --keep-work)"
fi

# project(LumexLib VERSION x.y.z.w) may span several lines.
CMAKE_VERSION_FOUND="$(tr '\n' ' ' <"${BUILD_ROOT}/CMakeLists.txt" \
  | grep -oE 'project\([[:space:]]*LumexLib[^)]*VERSION[[:space:]]+[0-9]+(\.[0-9]+){1,3}' \
  | grep -oE '[0-9]+(\.[0-9]+){1,3}$' | head -1 || true)"
[[ -n "${CMAKE_VERSION_FOUND}" ]] || die "cannot read project(LumexLib VERSION ...) from CMakeLists.txt"
VERSION="${VERSION_ARG:-${CMAKE_VERSION_FOUND}}"
if [[ -n "${VERSION_ARG}" && "${VERSION_ARG}" != "${CMAKE_VERSION_FOUND}" ]]; then
  echo "Warning: --version ${VERSION_ARG} differs from CMakeLists.txt (${CMAKE_VERSION_FOUND}); the libraries keep ${CMAKE_VERSION_FOUND}" >&2
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

# std_for <c++ compiler> <isa> -> the -DCMAKE_CXX_STANDARD value, or empty
std_for() {
  local cxx="$1" isa="$2" flags
  if [[ -n "${STD_ARG}" ]]; then echo "${STD_ARG}"; return; fi
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

declare -a JOB_C JOB_CXX JOB_LABEL JOB_ISA JOB_STD JOB_KIND JOB_NOTE
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
    JOB_C+=("$c"); JOB_CXX+=("$cxx"); JOB_LABEL+=("$label"); JOB_ISA+=("$isa")
    JOB_KIND+=("$kind")
    job_std="$(std_for "$cxx" "$isa")"
    JOB_STD+=("${job_std}")
    JOB_NOTE+=("$([[ -n "${STD_ARG}" ]] && std_note "$cxx" "$isa" "${STD_ARG}" || true)")
  done
done

# ---------------------------------------------------------------------------
# Build and package
# ---------------------------------------------------------------------------

mkdir -p "${OUTPUT_DIR}"
OUTPUT_DIR="$(cd "${OUTPUT_DIR}" && pwd)"
WORK_ROOT="${OUTPUT_DIR}/.work"
WARN_DIR="${OUTPUT_DIR}/warns"
mkdir -p "${WARN_DIR}"
SECONDS=0
ARTIFACTS=()
SUMMARY=()
FAILED_JOBS=()

# The compilers' messages are parsed for the logs of warns/: keep them in English.
export LC_ALL=C

echo "LumexLib ${VERSION}, glibc ${GLIBC_VERSION}, formats: ${FORMATS[*]}$([[ "${NO_PACKAGE}" -eq 1 ]] && echo ', no packages')$([[ "${WERROR}" -eq 1 ]] && echo ', -Werror')"
for i in "${!JOB_CXX[@]}"; do
  echo "  ${JOB_LABEL[$i]} ${JOB_ISA[$i]} C++${JOB_STD[$i]:-default}: ${JOB_C[$i]} / ${JOB_CXX[$i]}"
  [[ -z "${JOB_NOTE[$i]}" ]] || echo "    note: ${JOB_NOTE[$i]}"
done

remove_x64() {
  # Build output published into the checkout must not reach a later package.
  rm -rf "${BUILD_ROOT}/x64"
}

# stage_runtime <build dir> <c++ compiler> <isa> <lib dir>
# Runs cmake/PublishDistr.cmake (CopyRuntimeDependencies + the compiler's own
# runtime files) into a scratch directory and keeps the compiler runtime that
# does not come from the system library directories.
stage_runtime() {
  local build="$1" cxx="$2" isa="$3" libdir="$4" distr flags file name origin
  distr="$(mktemp -d)"
  flags="$(arch_flags "$isa")"
  cmake -Dbin_dir="${build}/bin" -Ddistr_dir="${distr}" \
    -Dcopy_runtime_script="${BUILD_ROOT}/CMakeRoutines/deployment/CopyRuntimeDependencies.cmake" \
    -Dcxx_compiler="${cxx}" -P "${BUILD_ROOT}/cmake/PublishDistr.cmake" >"${build}/publish-runtime.log" 2>&1 \
    || die "cmake/PublishDistr.cmake failed, see ${build}/publish-runtime.log"
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
    echo "    runtime: ${name}${origin:+ (from ${origin})}"
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
        echo "    runtime: ${dep} (from ${found})"
        settled=0
      done < <("$objdump" -p "$dll" | sed -n 's/^[[:space:]]*DLL Name: //p')
    done
    [[ "$settled" -eq 1 ]] && return 0
  done
  die "the MinGW runtime of '$cxx' does not settle after 6 passes"
}

write_deb() { # write_deb <stage root> <package name> <isa> <out file>
  local stage="$1" pkg="$2" isa="$3" out="$4" debarch size
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
  fakeroot dpkg-deb --build "${stage}" "${out}" >/dev/null
  rm -rf "${stage}/DEBIAN"
}

write_rpm() { # write_rpm <stage root> <package name> <isa> <out file> <work dir>
  local stage="$1" pkg="$2" isa="$3" out="$4" work="$5" rpmarch built
  rpmarch="x86_64"; [[ "$isa" == "x86" ]] && rpmarch="i686"
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
  rpmbuild -bb --target "${rpmarch}" --define "_topdir ${work}/rpm" \
    "${work}/rpm/SPECS/${pkg}.spec" >"${work}/rpmbuild.log" 2>&1 \
    || die "rpmbuild failed, see ${work}/rpmbuild.log"
  built="$(find "${work}/rpm/RPMS" -name '*.rpm' | head -1)"
  [[ -n "$built" ]] || die "rpmbuild produced no package, see ${work}/rpmbuild.log"
  mv "$built" "${out}"
}

for i in "${!JOB_CXX[@]}"; do
  c="${JOB_C[$i]}"; cxx="${JOB_CXX[$i]}"; label="${JOB_LABEL[$i]}"; isa="${JOB_ISA[$i]}"; std="${JOB_STD[$i]}"
  kind="${JOB_KIND[$i]}"
  isa_tag=""; [[ "$isa" == "x86" ]] && isa_tag="_x86"
  tag="${label}${isa_tag}_c++${std:-default}"
  warn_log="${WARN_DIR}/${tag}_warn.log"
  err_log="${WARN_DIR}/${tag}_err.log"
  rm -f "${warn_log}" "${err_log}"
  if [[ "$kind" == "windows" ]]; then
    base="LumexLib-${VERSION}_win_x64_${label}"
    INSTALL_PREFIX="/LumexLib"
  else
    base="LumexLib-${VERSION}_linux_${isa}_${label}_glibc${GLIBC_VERSION}"
    INSTALL_PREFIX="/opt/LumexLib/${VERSION}_${label}"
  fi
  work="${WORK_ROOT}/${label}_${isa}"
  build="${work}/build"
  stage="${work}/stage"
  rm -rf "${work}"
  mkdir -p "${build}" "${stage}"
  echo "== ${base} (C++${std:-default})"

  flags="$(arch_flags "$isa")"
  cmake_args=(
    -G Ninja -S "${BUILD_ROOT}" -B "${build}" --no-warn-unused-cli
    -DCMAKE_BUILD_TYPE=Release
    -DLUMEX_BUILD_SHARED_LIBS=ON -DLUMEX_BUILD_TESTS=OFF -DLUMEX_BUILD_DOCUMENTATION=OFF
    -DLUMEX_INSTALL=ON
    "-DCMAKE_INSTALL_PREFIX=${INSTALL_PREFIX}" -DCMAKE_INSTALL_LIBDIR=lib
  )
  if [[ "$kind" == "windows" ]]; then
    write_mingw_toolchain "${work}/mingw-toolchain.cmake" "${c}" "${cxx}"
    cmake_args+=("-DCMAKE_TOOLCHAIN_FILE=${work}/mingw-toolchain.cmake")
  else
    cmake_args+=("-DCMAKE_C_COMPILER=${c}" "-DCMAKE_CXX_COMPILER=${cxx}" '-DCMAKE_INSTALL_RPATH=$ORIGIN')
  fi
  [[ "${WERROR}" -eq 1 ]] && cmake_args+=(-DLUMEX_WERROR=ON)
  [[ -n "$std" ]] && cmake_args+=("-DCMAKE_CXX_STANDARD=${std}" -DCMAKE_CXX_STANDARD_REQUIRED=ON)
  [[ -n "$flags" ]] && cmake_args+=("-DCMAKE_C_FLAGS=${flags}" "-DCMAKE_CXX_FLAGS=${flags}")
  if ! cmake "${cmake_args[@]}" >"${work}/configure.log" 2>&1; then
    tail -n 40 "${work}/configure.log" >"${err_log}"
    echo "  configure FAILED: see ${err_log}"
    SUMMARY+=("${tag}: configure failed")
    FAILED_JOBS+=("${tag}")
    [[ "${KEEP_WORK}" -eq 1 ]] || rm -rf "${work}"
    continue
  fi

  # Library targets only: publish_distr and lumex_copy_compile_commands
  # write into the checkout (x64/, compile_commands.json); package and
  # package_source run CPack, which a library build does not need.
  mapfile -t targets < <(cd "${build}" && ninja -t targets all | grep -oE '^[A-Za-z0-9_]+: phony' | sed 's/: phony//' \
    | grep -vE '^(all|clean|help|edit_cache|rebuild_cache|install|install/local|install/strip|list_install_components|package|package_source|publish_distr|lumex_copy_compile_commands|generate_documentation|test)$' | sort -u)
  [[ ${#targets[@]} -gt 0 ]] || die "no targets found in ${build}"
  # -k 0: keep going after an error, so one run lists every diagnostic.
  build_rc=0
  cmake --build "${build}" --parallel "${JOBS}" --target "${targets[@]}" -- -k 0 >"${work}/build.log" 2>&1 \
    || build_rc=$?
  extract_args=()
  [[ "${build_rc}" -eq 0 ]] || extract_args+=(--failed)
  counts="$(python3 "${EXTRACTOR}" "${work}/build.log" --warn-out "${warn_log}" --err-out "${err_log}" "${extract_args[@]}")"
  n_warn="${counts#warnings=}"; n_warn="${n_warn%% *}"
  n_err="${counts##* errors=}"
  SUMMARY+=("${tag}: ${n_warn} warning(s), ${n_err} error(s)")
  echo "  ${n_warn} warning(s), ${n_err} error(s)"
  if [[ "${build_rc}" -ne 0 ]]; then
    # -Werror turns warnings into the errors of a build: then the warnings are the news.
    if [[ -f "${err_log}" ]]; then
      echo "  build FAILED: see ${err_log}"
    else
      echo "  build FAILED because of the warnings: see ${warn_log}"
    fi
    FAILED_JOBS+=("${tag}")
    [[ "${KEEP_WORK}" -eq 1 ]] || rm -rf "${work}"
    continue
  fi

  if [[ "${NO_PACKAGE}" -eq 1 ]]; then
    [[ "${KEEP_WORK}" -eq 1 ]] || rm -rf "${work}"
    continue
  fi

  DESTDIR="${stage}" cmake --install "${build}" >"${work}/install.log" 2>&1 \
    || die "install failed, see ${work}/install.log"

  prefix_dir="${stage}${INSTALL_PREFIX}"
  if [[ "$kind" == "windows" ]]; then
    [[ -d "${prefix_dir}/bin" ]] || die "install produced no ${INSTALL_PREFIX}/bin"
    stage_mingw_runtime "${cxx}" "${prefix_dir}/bin"
    out="${OUTPUT_DIR}/${base}.zip"
    rm -f "${out}"
    tree="${work}/zip/${base}"
    rm -rf "${work}/zip"; mkdir -p "${tree}"
    cp -a "${prefix_dir}/." "${tree}/"
    (cd "${work}/zip" && zip -qrX "${out}" "${base}")
    rm -rf "${work}/zip"
    remove_x64
    ARTIFACTS+=("${out}")
    echo "  -> $(basename "${out}")"
    [[ "${KEEP_WORK}" -eq 1 ]] || rm -rf "${work}"
    continue
  fi

  [[ -d "${prefix_dir}/lib" ]] || die "install produced no ${INSTALL_PREFIX}/lib"
  stage_runtime "${build}" "${cxx}" "${isa}" "${prefix_dir}/lib"

  pkg="lumexlib-${label}"
  for fmt in "${FORMATS[@]}"; do
    out="${OUTPUT_DIR}/${base}.${fmt}"
    rm -f "${out}"
    case "$fmt" in
      tar.gz | tar.xz)
        tree="${work}/tar/${base}"
        rm -rf "${work}/tar"; mkdir -p "${tree}"
        cp -a "${prefix_dir}/." "${tree}/"
        taropt="-czf"; [[ "$fmt" == "tar.xz" ]] && taropt="-cJf"
        tar --owner=0 --group=0 --numeric-owner -C "${work}/tar" "${taropt}" "${out}" "${base}"
        rm -rf "${work}/tar"
        ;;
      deb) write_deb "${stage}" "${pkg}" "${isa}" "${out}" ;;
      rpm) write_rpm "${stage}" "${pkg}" "${isa}" "${out}" "${work}" ;;
    esac
    remove_x64
    ARTIFACTS+=("${out}")
    echo "  -> $(basename "${out}")"
  done

  [[ "${KEEP_WORK}" -eq 1 ]] || rm -rf "${work}"
done
if [[ -n "${TAG_WORKTREE}" ]]; then
  if [[ "${KEEP_WORK}" -eq 1 ]]; then
    echo "Tag worktree kept (--keep-work): ${TAG_WORKTREE}"
  else
    git -C "${REPO_ROOT}" worktree remove --force "${TAG_WORKTREE}" 2>/dev/null \
      || echo "Warning: could not remove the tag worktree ${TAG_WORKTREE}; remove it with: git worktree remove --force <path>; git worktree prune" >&2
    git -C "${REPO_ROOT}" worktree prune
    rm -f "${OUTPUT_DIR}/.work/worktree.log"
  fi
fi
[[ "${KEEP_WORK}" -eq 1 ]] || rmdir "${WORK_ROOT}" 2>/dev/null || true

# A release needs a dated [vX.Y.Z.W] section in CHANGELOG.md.
if grep -qE "^## \[v${VERSION//./\\.}\].*в разработке" "${BUILD_ROOT}/CHANGELOG.md" 2>/dev/null; then
  echo "Warning: CHANGELOG.md section [v${VERSION}] is still marked as in development (not dated)" >&2
fi

echo "Diagnostics (logs of the builds with something to report are in ${WARN_DIR}):"
printf '  %s\n' "${SUMMARY[@]}"
if [[ ${#ARTIFACTS[@]} -gt 0 ]]; then
  echo "Packages in ${OUTPUT_DIR}:"
  printf '  %s\n' "${ARTIFACTS[@]##*/}"
fi
h=$((SECONDS / 3600)); m=$(((SECONDS % 3600) / 60)); s=$((SECONDS % 60))
printf "Total time: %02dh %02dm %02ds\n" "$h" "$m" "$s"
if [[ ${#FAILED_JOBS[@]} -gt 0 ]]; then
  echo "Failed builds: ${FAILED_JOBS[*]}" >&2
  exit 1
fi
