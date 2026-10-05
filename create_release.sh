#!/usr/bin/env bash
# create_release.sh - build LumexLib with every given compiler and ISA and
# package each build as
#   LumexLib-<version>_linux_<ISA>_<compiler>_glibc<glibc>.<deb|rpm|tar.xz|tar.gz>
#
# Run with --help for the options. Every check (tools, compilers, the
# 32-bit toolchain) runs before the first build, so a missing piece stops
# the script before any work is done.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="${SCRIPT_DIR}"

usage() {
  cat <<'EOF'
Usage: ./create_release.sh --compilers <path>[,<path>...] [options]

Builds LumexLib (Release, shared libraries, no tests) once per compiler and
ISA, installs it under /opt/LumexLib/<version>_<compiler>, and packages it.

Required:
  --compilers LIST   Comma-separated compiler paths. Each entry may name the
                     C or the C++ compiler; the other one must lie next to it
                     (gcc-13 <-> g++-13, clang <-> clang++, cc <-> c++).
                     Example: /usr/bin/gcc-13,/usr/bin/gcc-8,/opt/llvm-23.1.0/bin/clang++

Options:
  --arch LIST        x86-64 (default), x86. Comma-separated, for example
                     --arch x86,x86-64. x86 builds with -m32 and needs the
                     32-bit glibc and C++ library of every listed compiler.
  --formats LIST     deb,rpm,tar.xz,tar.gz (default: all four). rpm needs
                     rpmbuild (package "rpm"), deb needs dpkg-deb and fakeroot.
  --std N            C++ standard for every build (11, 14, 17, 20, 23).
                     Default: 20 where the compiler supports it (probe:
                     -std=c++20 gives __cplusplus >= 202002L), otherwise the
                     compiler's default standard.
  --output-dir DIR   Where the packages go (default: <repo>/release).
  --version V        Override the version read from project(LumexLib VERSION).
  --jobs N           Parallel build jobs (default: nproc).
  --keep-work        Keep the build, staging and packaging trees under
                     <output-dir>/.work (removed by default).
  -h, --help         Show this help.

Notes:
  * The compiler's own C++ runtime (libstdc++ / libgcc_s, or libc++ /
    libc++abi / libunwind) goes into lib/ of the package when it is not the
    system one (/lib, /usr/lib), selected by cmake/PublishDistr.cmake; the
    libraries get RUNPATH $ORIGIN.
  * <repo>/x64 is removed after every package, so that published build
    output never lands in a later archive.
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
JOBS="$(nproc 2>/dev/null || echo 4)"
KEEP_WORK=0

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
    --jobs) need_value "$@"; JOBS="$2"; shift 2 ;;
    --jobs=*) JOBS="${1#*=}"; shift ;;
    --keep-work) KEEP_WORK=1; shift ;;
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

for tool in cmake ninja tar readelf; do
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

# project(LumexLib VERSION x.y.z.w) may span several lines.
CMAKE_VERSION_FOUND="$(tr '\n' ' ' <"${REPO_ROOT}/CMakeLists.txt" \
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

# compiler_label <compiler> -> gcc13.2.0 / clang23.1.0 (from the predefined macros)
compiler_label() {
  local macros
  macros="$("$1" -dM -E -x c /dev/null 2>/dev/null)" || die "'$1' does not run"
  macro() { awk -v m="$1" '$1 == "#define" && $2 == m {print $3}' <<<"$macros"; }
  if [[ -n "$(macro __clang__)" ]]; then
    echo "clang$(macro __clang_major__).$(macro __clang_minor__).$(macro __clang_patchlevel__)"
  elif [[ -n "$(macro __GNUC__)" ]]; then
    echo "gcc$(macro __GNUC__).$(macro __GNUC_MINOR__).$(macro __GNUC_PATCHLEVEL__)"
  else
    die "'$1' is neither GCC nor Clang"
  fi
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

declare -a JOB_C JOB_CXX JOB_LABEL JOB_ISA JOB_STD
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
    probe_link "$cxx" "$isa"
    JOB_C+=("$c"); JOB_CXX+=("$cxx"); JOB_LABEL+=("$label"); JOB_ISA+=("$isa")
    JOB_STD+=("$(std_for "$cxx" "$isa")")
  done
done

# ---------------------------------------------------------------------------
# Build and package
# ---------------------------------------------------------------------------

mkdir -p "${OUTPUT_DIR}"
OUTPUT_DIR="$(cd "${OUTPUT_DIR}" && pwd)"
WORK_ROOT="${OUTPUT_DIR}/.work"
SECONDS=0
ARTIFACTS=()

echo "LumexLib ${VERSION}, glibc ${GLIBC_VERSION}, formats: ${FORMATS[*]}"
for i in "${!JOB_CXX[@]}"; do
  echo "  ${JOB_LABEL[$i]} ${JOB_ISA[$i]} C++${JOB_STD[$i]:-default}: ${JOB_C[$i]} / ${JOB_CXX[$i]}"
done

remove_x64() {
  # Build output published into the checkout must not reach a later package.
  rm -rf "${REPO_ROOT}/x64"
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
    -Dcopy_runtime_script="${REPO_ROOT}/CMakeRoutines/deployment/CopyRuntimeDependencies.cmake" \
    -Dcxx_compiler="${cxx}" -P "${REPO_ROOT}/cmake/PublishDistr.cmake" >"${build}/publish-runtime.log" 2>&1 \
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
  base="LumexLib-${VERSION}_linux_${isa}_${label}_glibc${GLIBC_VERSION}"
  INSTALL_PREFIX="/opt/LumexLib/${VERSION}_${label}"
  work="${WORK_ROOT}/${label}_${isa}"
  build="${work}/build"
  stage="${work}/stage"
  rm -rf "${work}"
  mkdir -p "${build}" "${stage}"
  echo "== ${base} (C++${std:-default})"

  flags="$(arch_flags "$isa")"
  cmake_args=(
    -G Ninja -S "${REPO_ROOT}" -B "${build}" --no-warn-unused-cli
    -DCMAKE_BUILD_TYPE=Release
    "-DCMAKE_C_COMPILER=${c}" "-DCMAKE_CXX_COMPILER=${cxx}"
    -DLUMEX_BUILD_SHARED_LIBS=ON -DLUMEX_BUILD_TESTS=OFF -DLUMEX_BUILD_DOCUMENTATION=OFF
    -DLUMEX_INSTALL=ON
    "-DCMAKE_INSTALL_PREFIX=${INSTALL_PREFIX}" -DCMAKE_INSTALL_LIBDIR=lib
    '-DCMAKE_INSTALL_RPATH=$ORIGIN'
  )
  [[ -n "$std" ]] && cmake_args+=("-DCMAKE_CXX_STANDARD=${std}" -DCMAKE_CXX_STANDARD_REQUIRED=ON)
  [[ -n "$flags" ]] && cmake_args+=("-DCMAKE_C_FLAGS=${flags}" "-DCMAKE_CXX_FLAGS=${flags}")
  cmake "${cmake_args[@]}" >"${work}/configure.log" 2>&1 \
    || die "configure failed, see ${work}/configure.log"

  # Library targets only: publish_distr and lumex_copy_compile_commands
  # write into the checkout (x64/, compile_commands.json).
  mapfile -t targets < <(cd "${build}" && ninja -t targets all | grep -oE '^[A-Za-z0-9_]+: phony' | sed 's/: phony//' \
    | grep -vE '^(all|clean|help|edit_cache|rebuild_cache|install|install/local|install/strip|list_install_components|publish_distr|lumex_copy_compile_commands|generate_documentation|test)$' | sort -u)
  [[ ${#targets[@]} -gt 0 ]] || die "no targets found in ${build}"
  cmake --build "${build}" --parallel "${JOBS}" --target "${targets[@]}" >"${work}/build.log" 2>&1 \
    || die "build failed, see ${work}/build.log"
  DESTDIR="${stage}" cmake --install "${build}" >"${work}/install.log" 2>&1 \
    || die "install failed, see ${work}/install.log"

  prefix_dir="${stage}${INSTALL_PREFIX}"
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
[[ "${KEEP_WORK}" -eq 1 ]] || rmdir "${WORK_ROOT}" 2>/dev/null || true

# A release needs a dated [vX.Y.Z.W] section in CHANGELOG.md.
if grep -qE "^## \[v${VERSION//./\\.}\].*в разработке" "${REPO_ROOT}/CHANGELOG.md" 2>/dev/null; then
  echo "Warning: CHANGELOG.md section [v${VERSION}] is still marked as in development (not dated)" >&2
fi

echo "Packages in ${OUTPUT_DIR}:"
printf '  %s\n' "${ARTIFACTS[@]##*/}"
h=$((SECONDS / 3600)); m=$(((SECONDS % 3600) / 60)); s=$((SECONDS % 60))
printf "Total time: %02dh %02dm %02ds\n" "$h" "$m" "$s"
