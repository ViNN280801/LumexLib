#!/usr/bin/env bash
# =============================================================================
# Script: bincompat.sh
# Purpose: Comprehensive portability audit for Linux ELF binaries/bundles
# Author: Semykin Vladislav <vladislav_semykin01@mail.ru|SemykinVD@Lumex.ru>
# Version: 0.2.0
# License: No license
# =============================================================================
# CHANGELOG v0.1.0:
#   + Added CPP_STDLIB detection (libstdc++/libc++/none)
#   + Added MIN_GLIBC_FUNCTIONS and MIN_GLIBCXX_FUNCTIONS (shows requiring symbols)
#   + Improved INTERPRETER handling for shared objects ("none (shared object)")
#   + Enhanced ISA detection with x86-64 v1-v4 inference from .note.gnu.property
#   + Expanded compatibility summary with OS-specific guidance and toolchain notes
# CHANGELOG v0.2.0:
#   + Added artifact type compatibility gate with three-tier system (INCOMPATIBLE/PARTIAL/COMPATIBLE)
#   + Added --force-analysis flag to override INCOMPATIBLE artifact type checks
#   + Added check_artifact_compatibility() function for intelligent type comparison
#   + Improved handling of incompatible comparisons (shared-object<--->executable, executable<--->static-archive, etc.)
#   + Enhanced user feedback with clear error messages for meaningless comparisons
# -----------------------------------------------------------------------------
# =============================================================================

##!
## @file bincompat.sh
## @brief Full-spectrum analyzer for Linux ELF portability and bundle hygiene.
## @details
##   Performs static ELF inspection, dependency walks, ABI heuristics, security
##   posture checks, bundle policy enforcement, and scenario tips. Designed for
##   CI usage as well as manual audits with configurable sections and comparison
##   mode. Logging is color-aware and resilient to missing tooling.
## @note
##   Requires Bash 4+, standard GNU userland, and optional pax-utils/patchelf
##   for deeper insights. All sections can be toggled through config or CLI.

set -Euo pipefail
IFS=$'\n\t'

SCRIPT_BASENAME="$(basename "$0")"
readonly SCRIPT_BASENAME

DEFAULT_CONFIG="${XDG_CONFIG_HOME:-$HOME/.config}/bincompat.conf"
readonly DEFAULT_CONFIG

DEFAULT_SECTIONS="interpreter dynamic deps versions arch abi security graphics nss bundle tips"
readonly DEFAULT_SECTIONS
DEFAULT_COMPARE_MODE="full"
readonly DEFAULT_COMPARE_MODE

declare -A SECTION_ENABLED=()
declare -A TOOL_STATUS=()
declare -A TOOL_PACKAGES=(
    [patchelf]="patchelf"
    [readelf]="binutils (readelf)"
    [objdump]="binutils (objdump)"
    [ldd]="glibc (ldd)"
    [lddtree]="pax-utils"
    [strings]="binutils (strings)"
    [nm]="binutils (nm)"
    [ar]="binutils (ar)"
    [file]="file"
    [awk]="gawk"
    [sed]="sed"
    [grep]="grep"
    [find]="findutils"
    [tput]="ncurses-bin"
)

USE_COLOR=1
COMPARE_MODE="$DEFAULT_COMPARE_MODE"
COMPARE_LEFT=""
COMPARE_RIGHT=""
DO_COMPARE=0
FORCE_ANALYSIS=0
__bincompat_compare_tmpdir=""
declare -A COLOR_MAP=(
    [TRACE]="\033[38;5;246m"
    [INFO]="\033[38;5;39m"
    [WARN]="\033[38;5;214m"
    [ERROR]="\033[38;5;196m"
    [SECTION]="\033[1;38;5;81m"
    [RESET]="\033[0m"
)

# Semantic constants for consistent field values
readonly UNKNOWN="unknown"
readonly NONE="none"

trap 'on_error $LINENO' ERR

##!
## @brief Fatal error helper - logs error and exits.
## @param message Error message to display.
die() {
    log ERROR "$*"
    exit 1
}

##!
## @brief Non-fatal warning helper.
## @param message Warning message to display.
warn() {
    log WARN "$*"
}

##!
## @brief Check if required command exists, error if missing.
## @param cmd Command name to check.
require_cmd() {
    local cmd="$1"
    if ! command -v "$cmd" >/dev/null 2>&1; then
        die "Required command '$cmd' not found. Please install it."
    fi
}

##!
## @brief Provide a human-readable compatibility verdict for two binaries.
## @param meta_a_name Name of associative array describing binary A.
## @param meta_b_name Name of associative array describing binary B.
## @details
##   Compares GLIBC/GLIBCXX minimums, ISA levels, and basic hardening flags to
##   highlight portability blockers and actionable warnings.
summarize_compatibility() {
    local meta_a_name="$1"
    local meta_b_name="$2"
    declare -n META_A="$meta_a_name"
    declare -n META_B="$meta_b_name"
    local reasons=()
    local warnings=()
    local verdict="high"

    local min_glibc_a="${META_A[MIN_GLIBC]:-}"
    local min_glibc_b="${META_B[MIN_GLIBC]:-}"
    local min_glibcxx_a="${META_A[MIN_GLIBCXX]:-}"
    local min_glibcxx_b="${META_B[MIN_GLIBCXX]:-}"
    local min_cxxabi_a="${META_A[MIN_CXXABI]:-}"
    local min_cxxabi_b="${META_B[MIN_CXXABI]:-}"
    local min_glibc_funcs_a="${META_A[MIN_GLIBC_FUNCTIONS]:-}"
    local min_glibc_funcs_b="${META_B[MIN_GLIBC_FUNCTIONS]:-}"
    local min_glibcxx_funcs_a="${META_A[MIN_GLIBCXX_FUNCTIONS]:-}"
    local min_glibcxx_funcs_b="${META_B[MIN_GLIBCXX_FUNCTIONS]:-}"
    local min_cxxabi_funcs_a="${META_A[MIN_CXXABI_FUNCTIONS]:-}"
    local min_cxxabi_funcs_b="${META_B[MIN_CXXABI_FUNCTIONS]:-}"
    local cpp_stdlib_a="${META_A[CPP_STDLIB]:-unknown}"
    local cpp_stdlib_b="${META_B[CPP_STDLIB]:-unknown}"
    local isa_a="${META_A[ISA_LEVEL]:-}"
    local isa_b="${META_B[ISA_LEVEL]:-}"
    local stack_a="${META_A[STACK_PERMS]:-}"
    local relro_a="${META_A[RELRO_STATUS]:-}"
    local relro_b="${META_B[RELRO_STATUS]:-}"
    local kind_a="${META_A[ARTIFACT_KIND]:-unknown}"
    local kind_b="${META_B[ARTIFACT_KIND]:-unknown}"
    local path_a="${META_A[PATH]:-unknown}"
    local path_b="${META_B[PATH]:-unknown}"
    local compiler_a="${META_A[COMPILER_TAG]:-unknown}"
    local compiler_b="${META_B[COMPILER_TAG]:-unknown}"

    # Check artifact type compatibility before proceeding with detailed analysis
    local compat_status
    compat_status="$(check_artifact_compatibility "$kind_a" "$kind_b")"

    case "$compat_status" in
    INCOMPATIBLE)
        if [[ "$FORCE_ANALYSIS" -ne 1 ]]; then
            # Print basic info and critical error, then return without further analysis
            log SECTION "Compatibility Summary" ""
            log INFO "Binary A: ${path_a}"
            log INFO "Binary B: ${path_b}"
            log INFO ""
            log INFO "Artifact types A/B: ${kind_a} / ${kind_b}"
            log INFO ""
            log ERROR "⚠⚠⚠ CRITICAL INCOMPATIBILITY ⚠⚠⚠"
            log ERROR "Comparing ${kind_a} and ${kind_b} is meaningless."
            log ERROR "These are fundamentally different object types."
            log ERROR "Compatibility analysis cannot be performed."
            log ERROR ""
            log ERROR "Use --force-analysis flag to override this check if you understand the implications."
            return
        else
            log WARN "Forcing analysis despite incompatible artifact types (${kind_a} vs ${kind_b})"
        fi
        ;;
    PARTIAL)
        # Warning about partial type mismatch, but analysis continues
        log ERROR "⚠ ARTIFACT TYPE MISMATCH ⚠"
        log ERROR "Binary A: ${kind_a}"
        log ERROR "Binary B: ${kind_b}"
        log ERROR "One is shared library, other is static archive."
        log ERROR "This represents fundamentally different linking approaches."
        log ERROR "Continuing compatibility analysis for reference only."
        log ERROR ""
        ;;
    COMPATIBLE)
        # No additional action needed
        :
        ;;
    esac

    if version_gt "$min_glibc_a" "$min_glibc_b"; then
        verdict="low"
        reasons+=("GLIBC ${min_glibc_a} exceeds ${min_glibc_b}")
    fi
    if version_gt "$min_glibcxx_a" "$min_glibcxx_b"; then
        verdict="low"
        reasons+=("GLIBCXX ${min_glibcxx_a} exceeds ${min_glibcxx_b}")
    fi
    if version_gt "$min_cxxabi_a" "$min_cxxabi_b"; then
        verdict="low"
        reasons+=("CXXABI ${min_cxxabi_a} exceeds ${min_cxxabi_b}")
    fi
    if [[ -n "$isa_a" && -n "$isa_b" ]]; then
        if version_gt "$isa_a" "$isa_b"; then
            verdict="low"
            reasons+=("ISA level ${isa_a} requires a newer CPU than ${isa_b}")
        fi
    fi
    if [[ "$stack_a" == *E* && -n "$stack_a" ]]; then
        warnings+=("Bin A allows an executable stack (${stack_a}).")
    fi
    if [[ "$relro_a" == "Disabled" && "$relro_b" != "Disabled" && -n "$relro_b" ]]; then
        warnings+=("Bin A ships without RELRO whereas Bin B enables hardening.")
    fi
    # Note: Artifact type compatibility is now handled by check_artifact_compatibility()
    # at the beginning of this function, so no need to check here again.

    log SECTION "Compatibility Summary" ""
    log INFO "Binary A: ${path_a}"
    log INFO "Binary B: ${path_b}"
    log INFO "Compiler A: ${compiler_a}"
    log INFO "Compiler B: ${compiler_b}"
    log INFO ""
    log INFO "C++ Standard Library A/B: ${cpp_stdlib_a} / ${cpp_stdlib_b}"

    # Explain C++ stdlib differences
    if [[ "$cpp_stdlib_a" != "$cpp_stdlib_b" ]]; then
        if [[ "$cpp_stdlib_a" == "libc++ (LLVM)" && "$cpp_stdlib_b" == "libstdc++ (GNU)" ]]; then
            log WARN "Different C++ runtimes: A uses LLVM libc++, B uses GNU libstdc++."
            log WARN "These have different ABIs and are not binary-compatible."
        elif [[ "$cpp_stdlib_a" == "libstdc++ (GNU)" && "$cpp_stdlib_b" == "libc++ (LLVM)" ]]; then
            log WARN "Different C++ runtimes: A uses GNU libstdc++, B uses LLVM libc++."
            log WARN "These have different ABIs and are not binary-compatible."
        fi
    fi

    log INFO ""
    log INFO "Min GLIBC A/B: ${min_glibc_a:-unknown} / ${min_glibc_b:-unknown}"

    # Show which functions require these versions
    if [[ -n "$min_glibc_funcs_a" && "$min_glibc_funcs_a" != "n/a" ]]; then
        log INFO "  A requires ${min_glibc_a} for: ${min_glibc_funcs_a}"
    fi
    if [[ -n "$min_glibc_funcs_b" && "$min_glibc_funcs_b" != "n/a" ]]; then
        log INFO "  B requires ${min_glibc_b} for: ${min_glibc_funcs_b}"
    fi

    log INFO "Min GLIBCXX A/B: ${min_glibcxx_a:-unknown} / ${min_glibcxx_b:-unknown}"

    # Show which functions require these versions
    if [[ -n "$min_glibcxx_funcs_a" && "$min_glibcxx_funcs_a" == "n/a"* ]]; then
        log INFO "  A: ${min_glibcxx_funcs_a}"
    elif [[ -n "$min_glibcxx_funcs_a" && "$min_glibcxx_funcs_a" != "n/a" ]]; then
        log INFO "  A requires ${min_glibcxx_a} for: ${min_glibcxx_funcs_a}"
    fi

    if [[ -n "$min_glibcxx_funcs_b" && "$min_glibcxx_funcs_b" == "n/a"* ]]; then
        log INFO "  B: ${min_glibcxx_funcs_b}"
    elif [[ -n "$min_glibcxx_funcs_b" && "$min_glibcxx_funcs_b" != "n/a" ]]; then
        log INFO "  B requires ${min_glibcxx_b} for: ${min_glibcxx_funcs_b}"
    fi

    log INFO ""
    log INFO "Min CXXABI A/B: ${min_cxxabi_a:-unknown} / ${min_cxxabi_b:-unknown}"

    # Show which functions require these versions
    if [[ -n "$min_cxxabi_funcs_a" && "$min_cxxabi_funcs_a" != "n/a" ]]; then
        log INFO "  A requires ${min_cxxabi_a} for: ${min_cxxabi_funcs_a}"
    fi

    if [[ -n "$min_cxxabi_funcs_b" && "$min_cxxabi_funcs_b" != "n/a" ]]; then
        log INFO "  B requires ${min_cxxabi_b} for: ${min_cxxabi_funcs_b}"
    fi

    log INFO ""
    log INFO "ISA level A/B: ${isa_a:-unknown} / ${isa_b:-unknown}"
    log INFO "Artifact types A/B: ${kind_a} / ${kind_b}"
    # Detailed OS compatibility analysis
    log INFO ""
    log SECTION "Detailed Compatibility Analysis" ""

    # GLIBC Analysis
    if [[ "$min_glibc_a" != "unknown" && "$min_glibc_b" != "unknown" ]]; then
        log INFO "GLIBC Runtime Requirements:"
        log INFO ""

        local os_a os_b
        os_a="$(map_glibc_to_os "$min_glibc_a")"
        os_b="$(map_glibc_to_os "$min_glibc_b")"

        log INFO "  Binary A (${min_glibc_a}):"
        log INFO "    Compatible with: ${os_a}"
        log INFO ""
        log INFO "  Binary B (${min_glibc_b}):"
        log INFO "    Compatible with: ${os_b}"
        log INFO ""

        local glibc_compat
        glibc_compat="$(analyze_glibc_compatibility "$min_glibc_a" "$min_glibc_b")"

        case "$glibc_compat" in
        "BINARY_A_NEWER")
            log WARN "  ⚠ Binary A requires NEWER glibc than B"
            log WARN "    - A will NOT run on systems where B works"
            log WARN "    - Example: A won't run on RHEL 7 if it needs GLIBC > 2.17"
            log WARN "    - Recommendation: Build A on older system or use older toolchain"
            ;;
        "BINARY_B_NEWER")
            log WARN "  ⚠ Binary B requires NEWER glibc than A"
            log WARN "    - B will NOT run on systems where A works"
            log WARN "    - Example: If B needs GLIBC 2.38, it won't run on Ubuntu 22.04 (has 2.35)"
            log WARN "    - Recommendation: Build B on older system or use older toolchain"
            log INFO ""
            log INFO "  Forward Compatibility:"
            log INFO "    - A can run on newer systems (where B runs)"
            log INFO "    - Both can coexist on modern systems (Ubuntu 24.04, Fedora 40+)"
            ;;
        "SAME")
            log INFO "  ✓ Both binaries have SAME glibc requirements"
            log INFO "    - Will run on the same set of systems"
            ;;
        esac
    fi

    log INFO ""

    # GLIBCXX/C++ Runtime Analysis
    if [[ "$cpp_stdlib_a" != "none" || "$cpp_stdlib_b" != "none" ]]; then
        log INFO "C++ Runtime Requirements:"
        log INFO ""

        if [[ "$cpp_stdlib_a" == "libstdc++ (GNU)" && "$min_glibcxx_a" != "unknown" ]]; then
            local compiler_version_a="${META_A[COMPILER_VERSION]:-unknown}"
            local compiler_tag_a="${META_A[COMPILER_TAG]:-unknown}"
            
            local gcc_a
            gcc_a="$(map_glibcxx_to_gcc "$min_glibcxx_a")"
            log INFO "  Binary A (${cpp_stdlib_a}, ${min_glibcxx_a}):"
            log INFO "    Requires: ${gcc_a}"
            if [[ "$min_cxxabi_a" != "unknown" ]]; then
                local cxxabi_gcc_a
                cxxabi_gcc_a="$(map_cxxabi_to_gcc "$min_cxxabi_a")"
                log INFO "    CXXABI ${min_cxxabi_a} suggests: ${cxxabi_gcc_a}"
                local combined_gcc_a
                combined_gcc_a="$(determine_gcc_version_range "$min_glibcxx_a" "$min_cxxabi_a" "$compiler_version_a")"
                if [[ "$compiler_tag_a" != "unknown" ]]; then
                    log INFO "    Actual compiler: ${compiler_tag_a}"
                fi
                log INFO "    Most likely GCC range: ${combined_gcc_a}"
            fi
        elif [[ "$cpp_stdlib_a" == "libc++ (LLVM)" ]]; then
            log INFO "  Binary A (${cpp_stdlib_a}):"
            log INFO "    Uses LLVM libc++ (typically Clang-built)"
            log INFO "    GLIBCXX/CXXABI versions do not apply"
        else
            log INFO "  Binary A: No C++ runtime dependency"
        fi

        log INFO ""

        if [[ "$cpp_stdlib_b" == "libstdc++ (GNU)" && "$min_glibcxx_b" != "unknown" ]]; then
            local compiler_version_b="${META_B[COMPILER_VERSION]:-unknown}"
            local compiler_tag_b="${META_B[COMPILER_TAG]:-unknown}"
            
            local gcc_b
            gcc_b="$(map_glibcxx_to_gcc "$min_glibcxx_b")"
            log INFO "  Binary B (${cpp_stdlib_b}, ${min_glibcxx_b}):"
            log INFO "    Requires: ${gcc_b}"
            if [[ "$min_cxxabi_b" != "unknown" ]]; then
                local cxxabi_gcc_b
                cxxabi_gcc_b="$(map_cxxabi_to_gcc "$min_cxxabi_b")"
                log INFO "    CXXABI ${min_cxxabi_b} suggests: ${cxxabi_gcc_b}"
                local combined_gcc_b
                combined_gcc_b="$(determine_gcc_version_range "$min_glibcxx_b" "$min_cxxabi_b" "$compiler_version_b")"
                if [[ "$compiler_tag_b" != "unknown" ]]; then
                    log INFO "    Actual compiler: ${compiler_tag_b}"
                fi
                log INFO "    Most likely GCC range: ${combined_gcc_b}"
            fi
        elif [[ "$cpp_stdlib_b" == "libc++ (LLVM)" ]]; then
            log INFO "  Binary B (${cpp_stdlib_b}):"
            log INFO "    Uses LLVM libc++ (typically Clang-built)"
            log INFO "    GLIBCXX/CXXABI versions do not apply"
        else
            log INFO "  Binary B: No C++ runtime dependency"
        fi

        # Cross-stdlib compatibility warning
        if [[ "$cpp_stdlib_a" != "$cpp_stdlib_b" && "$cpp_stdlib_a" != "none" && "$cpp_stdlib_b" != "none" ]]; then
            log INFO ""
            log ERROR "  ⚠⚠⚠ CRITICAL: Different C++ Standard Libraries ⚠⚠⚠"
            log ERROR "    - A uses: ${cpp_stdlib_a}"
            log ERROR "    - B uses: ${cpp_stdlib_b}"
            log ERROR "    - These are NOT ABI-compatible!"
            log ERROR "    - Cannot link/load together in same process"
            log ERROR "    - Must rebuild with same C++ stdlib"
            log ERROR ""
            log ERROR "  Resolution:"
            log ERROR "    1. Rebuild both with libstdc++ (GCC): g++ -stdlib=libstdc++"
            log ERROR "    2. OR rebuild both with libc++ (Clang): clang++ -stdlib=libc++"
            log ERROR "    3. Keep them in separate processes if mixing is required"
        fi
    fi

    log INFO ""

    # CXXABI Compatibility Analysis
    if [[ "$cpp_stdlib_a" == "libstdc++ (GNU)" || "$cpp_stdlib_b" == "libstdc++ (GNU)" ]]; then
        log INFO "CXXABI Compatibility:"
        log INFO ""
        
        if [[ "$min_cxxabi_a" != "unknown" && "$min_cxxabi_b" != "unknown" ]]; then
            local cxxabi_compat
            if version_gt "$min_cxxabi_a" "$min_cxxabi_b"; then
                cxxabi_compat="BINARY_A_NEWER"
            elif version_gt "$min_cxxabi_b" "$min_cxxabi_a"; then
                cxxabi_compat="BINARY_B_NEWER"
            else
                cxxabi_compat="SAME"
            fi
            
            case "$cxxabi_compat" in
            "BINARY_A_NEWER")
                log ERROR "  ⚠⚠⚠ CRITICAL: CXXABI Incompatibility ⚠⚠⚠"
                log ERROR "    - Binary A requires CXXABI ${min_cxxabi_a}"
                log ERROR "    - Binary B environment provides only CXXABI ${min_cxxabi_b}"
                log ERROR "    - Binary A will NOT run on systems where B runs"
                log ERROR "    - Missing symbol version will cause immediate runtime failure"
                log ERROR ""
                log ERROR "  Resolution:"
                log ERROR "    1. Rebuild A with GCC $(map_cxxabi_to_gcc "$min_cxxabi_b") or older"
                log ERROR "    2. OR ensure target system has libstdc++.so.6 with CXXABI ${min_cxxabi_a}+"
                log ERROR "    3. Check bundled libstdc++.so.6 version matches requirements"
                ;;
            "BINARY_B_NEWER")
                log WARN "  ⚠ Binary B requires NEWER CXXABI than A"
                log WARN "    - B requires CXXABI ${min_cxxabi_b}"
                log WARN "    - A requires only CXXABI ${min_cxxabi_a}"
                log WARN "    - B will NOT run on systems where A runs"
                log INFO ""
                log INFO "  Forward Compatibility:"
                log INFO "    - A can run on newer systems (where B runs)"
                ;;
            "SAME")
                log INFO "  ✓ Both binaries have SAME CXXABI requirements (${min_cxxabi_a})"
                log INFO "    - Full CXXABI compatibility"
                ;;
            esac
        elif [[ "$min_cxxabi_a" != "unknown" && "$min_cxxabi_b" == "unknown" ]]; then
            log WARN "  Binary A requires CXXABI ${min_cxxabi_a}, Binary B has no CXXABI symbols"
            log WARN "  Binary B might be C-only or statically linked C++ runtime"
        elif [[ "$min_cxxabi_a" == "unknown" && "$min_cxxabi_b" != "unknown" ]]; then
            log WARN "  Binary B requires CXXABI ${min_cxxabi_b}, Binary A has no CXXABI symbols"
            log WARN "  Binary A might be C-only or statically linked C++ runtime"
        else
            log INFO "  Both binaries have no CXXABI requirements (C-only or static C++ runtime)"
        fi
    fi

    log INFO ""

    # Overall verdict
    log SECTION "Overall Verdict" ""
    if [[ "$verdict" == "high" && ${#reasons[@]} -eq 0 ]]; then
        log INFO "✓ Binary A launch probability on Binary B environment: HIGH"
        log INFO "  - No blocking incompatibilities detected"
        log INFO "  - A should run successfully where B runs"
    else
        log WARN "⚠ Binary A launch probability on Binary B environment: $verdict"
        if ((${#reasons[@]})); then
            log_block WARN "Blocking Issues:" "$(printf '%s\n' "${reasons[@]/#/ - }")"
        fi
    fi

    if ((${#warnings[@]})); then
        log_block WARN "Additional Concerns:" "$(printf '%s\n' "${warnings[@]/#/ - }")"
    fi

    # Deployment recommendations
    log INFO ""
    log INFO "Deployment Recommendations:"

    # Check CXXABI compatibility first (most critical for C++ binaries)
    local cxxabi_blocking=0
    if [[ "$min_cxxabi_a" != "unknown" && "$min_cxxabi_b" != "unknown" ]]; then
        if version_gt "$min_cxxabi_a" "$min_cxxabi_b"; then
            cxxabi_blocking=1
            log ERROR "  ⚠ CRITICAL: CXXABI incompatibility detected!"
            log ERROR "  1. Binary A (${path_a}) requires CXXABI ${min_cxxabi_a}"
            log ERROR "  2. Binary B environment provides only CXXABI ${min_cxxabi_b}"
            log ERROR "  3. Binary A will FAIL to start with 'version not found' error"
            log ERROR "  4. MUST rebuild A with older GCC ($(map_cxxabi_to_gcc "$min_cxxabi_b"))"
            log ERROR "     OR ensure bundled/system libstdc++.so.6 has CXXABI ${min_cxxabi_a}+"
            log INFO ""
        elif version_gt "$min_cxxabi_b" "$min_cxxabi_a"; then
            log WARN "  NOTE: Binary B requires newer CXXABI (${min_cxxabi_b} vs ${min_cxxabi_a})"
            log WARN "        B will not run where A runs, but A will run where B runs"
            log INFO ""
        fi
    fi

    # GLIBC recommendations (if no CXXABI blocking issue)
    if [[ "$cxxabi_blocking" -eq 0 ]]; then
        if [[ "$glibc_compat" == "BINARY_B_NEWER" ]]; then
            log INFO "  1. Binary A: Deploy to older systems (broader compatibility)"
            log INFO "  2. Binary B: Deploy only to newer systems meeting GLIBC ${min_glibc_b}+"
            log INFO "  3. Consider building B with older toolchain for broader support"
        elif [[ "$glibc_compat" == "BINARY_A_NEWER" ]]; then
            log INFO "  1. Binary B: Deploy to older systems (broader compatibility)"
            log INFO "  2. Binary A: Deploy only to newer systems meeting GLIBC ${min_glibc_a}+"
            log INFO "  3. Consider building A with older toolchain for broader support"
        else
            log INFO "  1. Both binaries have similar runtime requirements"
            log INFO "  2. Deploy to systems meeting GLIBC ${min_glibc_a:-2.17}+ requirements"
        fi
    fi

    if [[ "$cpp_stdlib_a" != "$cpp_stdlib_b" && "$cpp_stdlib_a" != "none" && "$cpp_stdlib_b" != "none" ]]; then
        log WARN ""
        log WARN "  CRITICAL: Rebuild one or both binaries with matching C++ stdlib!"
    fi
}

##!
## @brief Execute metadata collection and diffing between two binaries.
## @details
##   Generates normalized reports, emits unified diff, and calls
##   summarize_compatibility for qualitative assessment. Aborts on collection
##   failures to keep CI signals crisp.
run_compare() {
    if [[ -z "$COMPARE_LEFT" || -z "$COMPARE_RIGHT" ]]; then
        log ERROR "The --compare option requires two paths."
        exit 1
    fi
    local mode="${COMPARE_MODE:-$DEFAULT_COMPARE_MODE}"
    local tmpdir
    tmpdir="$(mktemp -d)"
    __bincompat_compare_tmpdir="$tmpdir"
    trap 'rm -rf "$__bincompat_compare_tmpdir"' EXIT
    # shellcheck disable=SC2034
    declare -A META_LEFT META_RIGHT
    local report_left="$tmpdir/left.meta"
    local report_right="$tmpdir/right.meta"

    if ! collect_metadata "$COMPARE_LEFT" META_LEFT "$report_left" "$mode"; then
        log ERROR "Failed to collect metadata for $COMPARE_LEFT"
        exit 1
    fi
    if ! collect_metadata "$COMPARE_RIGHT" META_RIGHT "$report_right" "$mode"; then
        log ERROR "Failed to collect metadata for $COMPARE_RIGHT"
        exit 1
    fi

    log SECTION "Normalized Metadata (A)" ""
    sed 's/^/  /' "$report_left"
    log SECTION "Normalized Metadata (B)" ""
    sed 's/^/  /' "$report_right"

    local diff_file="$tmpdir/diff.txt"
    if ! diff -u "$report_left" "$report_right" >"$diff_file"; then
        log SECTION "Metadata Diff" ""
        sed 's/^/  /' "$diff_file"
    else
        log SECTION "Metadata Diff" ""
        log INFO "Reports are identical."
    fi

    summarize_compatibility META_LEFT META_RIGHT
    rm -rf "$tmpdir"
    __bincompat_compare_tmpdir=""
    trap - EXIT
}

##!
## @brief Normalize ELF facts into an associative array and optional text file.
## @param path Target binary or archive.
## @param meta_name Name for the associative array receiver (nameref).
## @param outfile Optional path to store textual report.
## @param mode Collection mode (full|fast).
## @details
##   Captures interpreter, RPATH/RUNPATH, dependency graph, versioned symbols,
##   ISA hints, security posture, build IDs, and compiler fingerprints. Designed
##   to be replayable for diffing.
collect_metadata() {
    local path="$1"
    local meta_name="$2"
    local outfile="${3:-}"
    local mode="${4:-full}"
    # shellcheck disable=SC2034
    declare -n meta="$meta_name"
    meta=()
    [[ -n "$outfile" ]] && : >"$outfile"

    record_metadata_entry() {
        local key="$1"
        local value="$2"
        meta["$key"]="$value"
        if [[ -n "$outfile" ]]; then
            printf "%s: %s\n" "$key" "$value" >>"$outfile"
        fi
    }

    record_metadata_block() {
        local key="$1"
        local body="$2"
        # shellcheck disable=SC2034
        meta["$key"]="$body"
        if [[ -n "$outfile" ]]; then
            printf "%s:\n" "$key" >>"$outfile"
            printf '%s\n' "$body" | sed 's/^/  /' >>"$outfile"
        fi
    }

    record_metadata_entry "PATH" "$path"
    if [[ ! -e "$path" ]]; then
        record_metadata_entry "ERROR" "Path not found"
        return 1
    fi
    record_metadata_entry "SHA256" "$(sha256sum "$path" 2>/dev/null | awk '{print $1}')"
    local file_desc
    file_desc="$(file -b "$path" 2>/dev/null || echo "unknown")"
    record_metadata_entry "FILE_DESC" "$file_desc"
    record_metadata_entry "MTIME" "$(stat -c %y "$path" 2>/dev/null || echo "unknown")"

    local artifact_kind="unknown"
    if is_archive "$path"; then
        artifact_kind="static-archive"
    elif is_elf "$path"; then
        if [[ "$file_desc" == *"shared object"* ]]; then
            artifact_kind="shared-object"
        elif [[ "$file_desc" == *"executable"* ]]; then
            artifact_kind="executable"
        else
            artifact_kind="elf-object"
        fi
    fi
    record_metadata_entry "ARTIFACT_KIND" "$artifact_kind"

    if is_elf "$path"; then
        # Use improved interpreter detection
        local interp
        interp="$(detect_interpreter "$path")"
        record_metadata_entry "INTERPRETER" "$interp"

        local rpath_raw=""
        if tool_available patchelf; then
            rpath_raw="$(patchelf --print-rpath "$path" 2>/dev/null || true)"
        fi
        if [[ -z "$rpath_raw" ]]; then
            if tool_available readelf; then
                rpath_raw="$(readelf -d "$path" 2>/dev/null | awk -F'[][]' '/RUNPATH/ {print $2; exit}')"
            fi
        fi
        if [[ -n "$rpath_raw" ]]; then
            record_metadata_block "RUNPATH" "$(printf '%s\n' "${rpath_raw//:/$'\n'}")"
        else
            record_metadata_entry "RUNPATH" "none"
        fi

        if tool_available readelf; then
            local needed=""
            needed="$(readelf -d "$path" 2>/dev/null | awk -F'[][]' '/NEEDED/ {print $2}')"
            if [[ -n "$needed" ]]; then
                record_metadata_block "NEEDED" "$needed"
            else
                record_metadata_entry "NEEDED" "unknown"
            fi
        fi

        if [[ "$mode" != "fast" ]]; then
            local deps deps_source
            if collect_dependency_tree "$path" deps deps_source; then
                record_metadata_entry "DEPS_SOURCE" "${deps_source:-unknown}"
                record_metadata_block "DEPENDENCY_TREE" "$deps"
                local gpu_refs
                gpu_refs="$(printf '%s\n' "$deps" | grep -E 'libcuda|libnvidia|libOpenCL' || true)"
                [[ -n "$gpu_refs" ]] && record_metadata_block "GPU_DEPENDENCIES" "$gpu_refs"
            fi
        fi

        # Detect C++ standard library type first
        local cpp_stdlib
        cpp_stdlib="$(detect_cpp_stdlib "$path")"
        record_metadata_entry "CPP_STDLIB" "$cpp_stdlib"

        # Gather versioned symbols and build version->functions maps
        declare -A GLIBC_VER_MAP=()
        declare -A GLIBCXX_VER_MAP=()
        declare -A CXXABI_VER_MAP=()
        gather_versioned_symbols "$path" GLIBC_VER_MAP GLIBCXX_VER_MAP CXXABI_VER_MAP

        # Extract versions and find minimum (maximum version required)
        local glibc_versions=""
        local glibcxx_versions=""
        local cxxabi_versions=""

        # Build version lists from maps
        for ver in "${!GLIBC_VER_MAP[@]}"; do
            glibc_versions="${glibc_versions}${ver}"$'\n'
        done

        for ver in "${!GLIBCXX_VER_MAP[@]}"; do
            glibcxx_versions="${glibcxx_versions}${ver}"$'\n'
        done

        for ver in "${!CXXABI_VER_MAP[@]}"; do
            cxxabi_versions="${cxxabi_versions}${ver}"$'\n'
        done

        # Fallback to old method if gather_versioned_symbols found nothing
        if [[ -z "$glibc_versions" ]]; then
            if tool_available objdump; then
                glibc_versions="$(objdump -T "$path" 2>/dev/null | grep -o 'GLIBC_[0-9.]*' | sort -V | uniq || true)"
            fi
            if [[ -z "$glibc_versions" ]] && tool_available strings; then
                glibc_versions="$(strings "$path" 2>/dev/null | grep -E 'GLIBC_[0-9.]+' | sort -V | uniq || true)"
            fi
        fi

        if [[ -z "$glibcxx_versions" ]]; then
            if tool_available objdump; then
                glibcxx_versions="$(objdump -T "$path" 2>/dev/null | grep -o 'GLIBCXX_[0-9.]*' | sort -V | uniq || true)"
            fi
            if [[ -z "$glibcxx_versions" ]] && tool_available strings; then
                glibcxx_versions="$(strings "$path" 2>/dev/null | grep -E 'GLIBCXX_[0-9.]+' | sort -V | uniq || true)"
            fi
        fi

        # Extract CXXABI versions (fallback methods)
        if [[ -z "$cxxabi_versions" ]]; then
            if tool_available readelf; then
                cxxabi_versions="$(readelf -V "$path" 2>/dev/null | grep -o 'CXXABI_[0-9.]*' | sort -V | uniq || true)"
            fi
            if [[ -z "$cxxabi_versions" ]] && tool_available objdump; then
                cxxabi_versions="$(objdump -T "$path" 2>/dev/null | grep -o 'CXXABI_[0-9.]*' | sort -V | uniq || true)"
            fi
            if [[ -z "$cxxabi_versions" ]] && tool_available strings; then
                cxxabi_versions="$(strings "$path" 2>/dev/null | grep -E 'CXXABI_[0-9.]+' | sort -V | uniq || true)"
            fi
        fi

        local min_glibc="unknown"
        local min_glibcxx="unknown"
        local min_cxxabi="unknown"
        local min_glibc_functions="n/a"
        local min_glibcxx_functions="n/a"
        local min_cxxabi_functions="n/a"

        if [[ -n "$glibc_versions" ]]; then
            min_glibc="$(printf '%s\n' "$glibc_versions" | sort -V | tail -n1)"
            record_metadata_block "GLIBC_SYMBOLS" "$glibc_versions"

            # Get functions for this version (up to 3)
            if [[ -n "${GLIBC_VER_MAP[$min_glibc]:-}" ]]; then
                min_glibc_functions="$(printf '%s' "${GLIBC_VER_MAP[$min_glibc]}" | tr ',' '\n' | head -3 | paste -sd,)"
            fi
        fi

        # For GLIBCXX: check if we're using libc++ instead
        if [[ "$cpp_stdlib" == "libc++ (LLVM)" || "$cpp_stdlib" == "none" ]]; then
            min_glibcxx_functions="n/a (uses ${cpp_stdlib})"
        elif [[ -n "$glibcxx_versions" ]]; then
            min_glibcxx="$(printf '%s\n' "$glibcxx_versions" | sort -V | tail -n1)"
            record_metadata_block "GLIBCXX_SYMBOLS" "$glibcxx_versions"

            # Get functions for this version (up to 3)
            if [[ -n "${GLIBCXX_VER_MAP[$min_glibcxx]:-}" ]]; then
                min_glibcxx_functions="$(printf '%s' "${GLIBCXX_VER_MAP[$min_glibcxx]}" | tr ',' '\n' | head -3 | paste -sd,)"
            fi
        fi

        # Extract CXXABI minimum version
        if [[ -n "$cxxabi_versions" ]]; then
            min_cxxabi="$(printf '%s\n' "$cxxabi_versions" | sort -V | tail -n1)"
            record_metadata_block "CXXABI_SYMBOLS" "$cxxabi_versions"

            # Get functions for this version (up to 3)
            if [[ -n "${CXXABI_VER_MAP[$min_cxxabi]:-}" ]]; then
                min_cxxabi_functions="$(printf '%s' "${CXXABI_VER_MAP[$min_cxxabi]}" | tr ',' '\n' | head -3 | paste -sd,)"
            fi
        fi

        record_metadata_entry "MIN_GLIBC" "$min_glibc"
        record_metadata_entry "MIN_GLIBC_FUNCTIONS" "$min_glibc_functions"
        record_metadata_entry "MIN_GLIBCXX" "$min_glibcxx"
        record_metadata_entry "MIN_GLIBCXX_FUNCTIONS" "$min_glibcxx_functions"
        record_metadata_entry "MIN_CXXABI" "$min_cxxabi"
        record_metadata_entry "MIN_CXXABI_FUNCTIONS" "$min_cxxabi_functions"

        # Use improved ISA level detection
        local isa_level
        isa_level="$(detect_isa_level "$path")"
        record_metadata_entry "ISA_LEVEL" "$isa_level"

        # Keep old ISA_PROPERTIES for detailed info if readelf -A provides any
        if tool_available readelf; then
            local isa
            isa="$(readelf -A "$path" 2>/dev/null || true)"
            if [[ -n "$isa" ]]; then
                record_metadata_block "ISA_PROPERTIES" "$isa"
            else
                record_metadata_entry "ISA_PROPERTIES" "$isa_level"
            fi
        fi

        declare -A SEC_CAP=()
        if gather_security_info "$path" "$mode" SEC_CAP; then
            record_metadata_entry "ELF_TYPE" "${SEC_CAP[elf_type]:-unknown}"
            record_metadata_entry "STACK_PERMS" "${SEC_CAP[stack_perms]:-unknown}"
            record_metadata_entry "RELRO_STATUS" "${SEC_CAP[relro_status]:-unknown}"
            record_metadata_entry "BIND_NOW" "${SEC_CAP[bind_now]:-unknown}"
            record_metadata_entry "TEXTREL" "${SEC_CAP[textrel]:-unknown}"
        fi

        if tool_available readelf; then
            local note_output
            note_output="$(readelf -n "$path" 2>/dev/null || true)"
            local build_id
            build_id="$(printf '%s\n' "$note_output" | awk '/Build ID:/ {print $3; exit}')"
            [[ -n "$build_id" ]] && record_metadata_entry "BUILD_ID" "$build_id"
            local abi_tag
            abi_tag="$(printf '%s\n' "$note_output" | awk '/NT_GNU_ABI_TAG/{flag=1; next} /^$/ {flag=0} flag {print}' | xargs)"
            [[ -n "$abi_tag" ]] && record_metadata_entry "ABI_TAG" "$abi_tag"
        fi

        # Extract compiler version with multiple methods for accuracy
        local compiler_tag=""
        local compiler_version=""
        local compiler_name=""
        
        # Method 1: Read from .comment section (most reliable)
        if tool_available readelf; then
            local comment_section
            comment_section="$(readelf -p .comment "$path" 2>/dev/null || true)"
            
            # Extract GCC version: "GCC: (GNU) 14.2.0" -> "14.2.0"
            if echo "$comment_section" | grep -q "GCC:"; then
                compiler_name="GCC"
                compiler_version="$(echo "$comment_section" | grep -oE 'GCC:.*[0-9]+\.[0-9]+\.[0-9]+' | grep -oE '[0-9]+\.[0-9]+\.[0-9]+' | head -1)"
                compiler_tag="GCC: (GNU) ${compiler_version}"
            # Extract Clang version
            elif echo "$comment_section" | grep -q "clang version"; then
                compiler_name="Clang"
                compiler_version="$(echo "$comment_section" | grep -oE 'clang version [0-9]+\.[0-9]+\.[0-9]+' | grep -oE '[0-9]+\.[0-9]+\.[0-9]+' | head -1)"
                compiler_tag="$(echo "$comment_section" | grep -m1 "clang version" | sed 's/.*\[\(.*\)\].*/\1/' | head -1)"
            fi
        fi
        
        # Method 2: Extract from strings (fallback)
        if [[ -z "$compiler_tag" ]] && tool_available strings; then
            local gcc_string
            gcc_string="$(strings "$path" 2>/dev/null | grep -m1 -E '^GCC: \(GNU\) [0-9]+\.[0-9]+' || true)"
            if [[ -n "$gcc_string" ]]; then
                compiler_name="GCC"
                compiler_version="$(echo "$gcc_string" | grep -oE '[0-9]+\.[0-9]+(\.[0-9]+)?' | head -1)"
                compiler_tag="$gcc_string"
            fi
        fi
        
        # Method 3: Extract from include paths in strings (additional verification)
        if [[ -z "$compiler_version" ]] && tool_available strings; then
            local gcc_path
            gcc_path="$(strings "$path" 2>/dev/null | grep -m1 -E '/gcc[0-9]+|/c\+\+/[0-9]+\.[0-9]+' || true)"
            if [[ -n "$gcc_path" ]]; then
                # Extract version from path like /opt/gcc14.2.0/include/c++/14.2.0
                local path_version
                path_version="$(echo "$gcc_path" | grep -oE '[0-9]+\.[0-9]+(\.[0-9]+)?' | head -1)"
                if [[ -n "$path_version" ]]; then
                    if [[ -z "$compiler_version" ]]; then
                        compiler_name="GCC"
                        compiler_version="$path_version"
                        compiler_tag="GCC: (GNU) ${compiler_version} (detected from include path)"
                    elif [[ "$compiler_version" != "$path_version" ]]; then
                        # Version mismatch - use the more specific one
                        compiler_tag="${compiler_tag} (path suggests ${path_version})"
                    fi
                fi
            fi
        fi
        
        record_metadata_entry "COMPILER_TAG" "${compiler_tag:-unknown}"
        record_metadata_entry "COMPILER_NAME" "${compiler_name:-unknown}"
        record_metadata_entry "COMPILER_VERSION" "${compiler_version:-unknown}"

        local comment_blob=""
        if tool_available readelf; then
            comment_blob="$(readelf -p .comment "$path" 2>/dev/null || true)"
        fi
        [[ -n "$comment_blob" ]] && record_metadata_block "COMMENT_SECTION" "$comment_blob"
    fi
    unset -f record_metadata_entry record_metadata_block
    return 0
}

##!
## @brief Extract PIE/stack/RELRO/BIND_NOW/TEXTREL signals via readelf.
## @param path ELF object to inspect.
## @param mode Collection aggressiveness (currently informational).
## @param sec_ref Name of associative array receiving findings.
## @return 0 on success, non-zero if readelf missing.
gather_security_info() {
    local path="$1"
    local mode="${2:-full}"
    declare -n sec_ref="$3"
    sec_ref=()
    if ! tool_available readelf; then
        return 1
    fi
    local elf_header
    elf_header="$(readelf -h "$path" 2>/dev/null || true)"
    if [[ -n "$elf_header" ]]; then
        local type_line
        type_line="$(printf '%s\n' "$elf_header" | grep 'Type:' || true)"
        if [[ -n "$type_line" ]]; then
            local elf_type="${type_line#*:}"
            elf_type="${elf_type#"${elf_type%%[![:space:]]*}"}"
            sec_ref[elf_type]="$elf_type"
        fi
    fi
    local prog_headers
    prog_headers="$(readelf -W -l "$path" 2>/dev/null || true)"
    if [[ -n "$prog_headers" ]]; then
        local stack_line
        stack_line="$(printf '%s\n' "$prog_headers" | grep 'GNU_STACK' || true)"
        if [[ -n "$stack_line" ]]; then
            local perms
            perms="$(printf '%s\n' "$stack_line" | awk '{for(i=1;i<=NF;i++) if($i ~ /^[RWE]+$/){print $i; exit}}')"
            sec_ref[stack_perms]="${perms:-unknown}"
        fi
        local relro_line
        relro_line="$(printf '%s\n' "$prog_headers" | grep 'GNU_RELRO' || true)"
        local relro_present="no"
        if [[ -n "$relro_line" ]]; then
            relro_present="yes"
        fi
        sec_ref[relro_present]="$relro_present"
    fi
    local dyn_tags
    dyn_tags="$(readelf -d "$path" 2>/dev/null || true)"
    local bind_now="no"
    local textrel="no"
    if [[ -n "$dyn_tags" ]]; then
        if printf '%s\n' "$dyn_tags" | grep -q 'BIND_NOW'; then
            bind_now="yes"
        fi
        if printf '%s\n' "$dyn_tags" | grep -q 'TEXTREL'; then
            textrel="yes"
        fi
    fi
    sec_ref[bind_now]="$bind_now"
    sec_ref[textrel]="$textrel"
    local relro_status="disabled"
    if [[ "${sec_ref[relro_present]}" == "yes" && "$bind_now" == "yes" ]]; then
        relro_status="full"
    elif [[ "${sec_ref[relro_present]}" == "yes" ]]; then
        relro_status="partial"
    fi
    sec_ref[relro_status]="$relro_status"
    return 0
}

on_error() {
    local line="$1"
    log ERROR "Unhandled error near line ${line}. Aborting."
    exit 70
}

init_colors() {
    if [[ ! -t 1 ]] || ! command -v tput >/dev/null 2>&1 || [[ "$(tput colors 2>/dev/null || echo 0)" -lt 8 ]]; then
        USE_COLOR=0
    fi
}

colorize() {
    local level="$1"
    local text="$2"
    if [[ "$USE_COLOR" -eq 1 && -n "${COLOR_MAP[$level]:-}" ]]; then
        printf "%b%s%b" "${COLOR_MAP[$level]}" "$text" "${COLOR_MAP[RESET]}"
    else
        printf "%s" "$text"
    fi
}

##!
## @brief Central logging helper with timestamp and optional coloring.
## @param level Log severity (INFO, WARN, ERROR, SECTION, TRACE).
## @param message Variadic message payload.
log() {
    local level="$1"
    shift || true
    local timestamp
    timestamp="$(date '+%Y-%m-%d %H:%M:%S')"
    local prefix
    prefix="[$timestamp] [$level]"
    if [[ "$level" == "SECTION" ]]; then
        printf "\n%s %s\n" "$(colorize SECTION "$prefix")" "$*"
    else
        printf "%s %s\n" "$(colorize "$level" "$prefix")" "$*"
    fi
}

##!
## @brief Emit multiline payloads with consistent indentation.
## @param level Log severity.
## @param title Heading prepended before the block.
## @param body Body text to indent (optional).
log_block() {
    local level="$1"
    local title="$2"
    local body="${3:-}"
    log "$level" "$title"
    if [[ -n "$body" ]]; then
        printf '%s\n' "$body" | sed 's/^/    /'
    fi
}

##!
## @brief Render multiline payload with numeric bullets.
## @param level Log severity.
## @param title Header to announce block.
## @param body Multiline content enumerated line-by-line.
log_numbered_block() {
    local level="$1"
    local title="$2"
    local body="${3:-}"
    log "$level" "$title"
    local idx=1
    local line
    while IFS= read -r line; do
        [[ -z "$line" ]] && continue
        printf "    [%d] %s\n" "$idx" "$line"
        ((idx += 1))
    done <<<"$body"
}

##!
## @brief Convert colon-separated entries (e.g., RPATH) into enumerated list.
## @param level Log severity.
## @param title Heading for the list.
## @param raw Colon-separated string.
log_colon_list() {
    local level="$1"
    local title="$2"
    local raw="${3:-}"
    log "$level" "$title"
    local IFS=':'
    read -r -a entries <<<"$raw"
    local idx=1
    local entry
    for entry in "${entries[@]}"; do
        [[ -z "$entry" ]] && continue
        printf "    [%d] %s\n" "$idx" "$entry"
        ((idx += 1))
    done
}

version_gt() {
    local a="$1"
    local b="$2"
    [[ -z "$a" || -z "$b" || "$a" == "unknown" || "$b" == "unknown" ]] && return 1
    [[ "$a" == "$b" ]] && return 1
    local highest
    highest="$(printf '%s\n%s\n' "$a" "$b" | sort -V | tail -n1)"
    [[ "$highest" == "$a" ]]
}

version_lt() {
    local a="$1"
    local b="$2"
    [[ -z "$a" || -z "$b" || "$a" == "unknown" || "$b" == "unknown" ]] && return 1
    [[ "$a" == "$b" ]] && return 1
    local lowest
    lowest="$(printf '%s\n%s\n' "$a" "$b" | sort -V | head -n1)"
    [[ "$lowest" == "$a" ]]
}

##!
## @brief Detect C++ standard library type from DT_NEEDED entries.
## @param path ELF binary to inspect.
## @return CPP_STDLIB value via stdout: "libstdc++ (GNU)", "libc++ (LLVM)", or "none".
detect_cpp_stdlib() {
    local path="$1"
    local needed=""

    if tool_available readelf; then
        needed="$(readelf -d "$path" 2>/dev/null | awk -F'[][]' '/NEEDED/ {print $2}')"
    fi

    # Check for libstdc++ first (GNU)
    if printf '%s\n' "$needed" | grep -q 'libstdc++\.so'; then
        echo "libstdc++ (GNU)"
        return 0
    fi

    # Check for libc++ (LLVM)
    if printf '%s\n' "$needed" | grep -q 'libc++\.so'; then
        echo "libc++ (LLVM)"
        return 0
    fi

    # No C++ stdlib detected
    echo "none"
}

##!
## @brief Gather versioned symbols (GLIBC/GLIBCXX/CXXABI) and map version -> functions.
## @param path ELF binary to inspect.
## @param glibc_map_name Name of associative array for GLIBC version->functions map.
## @param glibcxx_map_name Name of associative array for GLIBCXX version->functions map.
## @param cxxabi_map_name Name of associative array for CXXABI version->functions map.
## @details Extracts UND (undefined) symbols with version strings.
gather_versioned_symbols() {
    local path="$1"
    local glibc_map_name="$2"
    local glibcxx_map_name="$3"
    local cxxabi_map_name="${4:-}"
    declare -n glibc_map="$glibc_map_name"
    declare -n glibcxx_map="$glibcxx_map_name"
    declare -n cxxabi_map="${cxxabi_map_name}"

    if ! tool_available objdump; then
        return 1
    fi

    # Parse objdump -T output
    # Format: 0000000000000000      DF *UND*	0000000000000000 (GLIBC_2.17) clock_gettime
    # or: 0000000000000000          DF *UND*	0000000000000000 (GLIBCXX_3.4.21) _ZNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEE9_M_createERmm
    # or: 0000000000000000          DF *UND*	0000000000000000 (CXXABI_1.3.15) __cxa_call_terminate

    while IFS= read -r line; do
        [[ -z "$line" ]] && continue

        # Extract version and function name from lines like:
        # ... *UND* ... (GLIBC_2.17) function_name
        if [[ "$line" =~ \*UND\*.*\((GLIBC_[0-9.]+)\)[[:space:]]+([^[:space:]]+)$ ]]; then
            local version="${BASH_REMATCH[1]}"
            local func_name="${BASH_REMATCH[2]}"

            if [[ -n "${glibc_map[$version]:-}" ]]; then
                glibc_map[$version]="${glibc_map[$version]},${func_name}"
            else
                glibc_map[$version]="$func_name"
            fi
        elif [[ "$line" =~ \*UND\*.*\((GLIBCXX_[0-9.]+)\)[[:space:]]+([^[:space:]]+)$ ]]; then
            local version="${BASH_REMATCH[1]}"
            local func_name="${BASH_REMATCH[2]}"

            if [[ -n "${glibcxx_map[$version]:-}" ]]; then
                glibcxx_map[$version]="${glibcxx_map[$version]},${func_name}"
            else
                glibcxx_map[$version]="$func_name"
            fi
        elif [[ -n "$cxxabi_map_name" && "$line" =~ \*UND\*.*\((CXXABI_[0-9.]+)\)[[:space:]]+([^[:space:]]+)$ ]]; then
            local version="${BASH_REMATCH[1]}"
            local func_name="${BASH_REMATCH[2]}"

            if [[ -n "${cxxabi_map[$version]:-}" ]]; then
                cxxabi_map[$version]="${cxxabi_map[$version]},${func_name}"
            else
                cxxabi_map[$version]="$func_name"
            fi
        fi
    done < <(objdump -T "$path" 2>/dev/null)
}

##!
## @brief Find maximum version from a list of version strings.
## @param versions List of versions (one per line).
## @return Maximum version via stdout.
find_max_version() {
    local versions="$1"
    [[ -z "$versions" ]] && return 1

    # Use sort -V for version sorting
    printf '%s\n' "$versions" | sort -V | tail -n1
}

##!
## @brief Detect interpreter path with proper handling for shared objects.
## @param path ELF binary to inspect.
## @return Interpreter path, "none (shared object)", or "unknown".
detect_interpreter() {
    local path="$1"

    if ! tool_available readelf; then
        echo "$UNKNOWN"
        return 1
    fi

    # Check if there's a PT_INTERP segment
    local interp_path
    interp_path="$(readelf -l "$path" 2>/dev/null | awk '/Requesting program interpreter:/ {gsub(/\[|\]/,"",$4); print $4; exit}')"

    if [[ -n "$interp_path" ]]; then
        echo "$interp_path"
        return 0
    fi

    # Check if it's a shared object (ET_DYN without PT_INTERP)
    local elf_type
    elf_type="$(readelf -h "$path" 2>/dev/null | awk '/Type:/ {print $2; exit}')"

    if [[ "$elf_type" == "DYN" ]]; then
        echo "$NONE (shared object)"
        return 0
    fi

    # Executable without interpreter (unusual)
    echo "$UNKNOWN"
    return 1
}

##!
## @brief Detect ISA level (x86-64-v1/v2/v3/v4) with fallbacks.
## @param path ELF binary to inspect.
## @return ISA level description via stdout.
detect_isa_level() {
    local path="$1"
    local isa_info=""

    if ! tool_available readelf; then
        echo "$UNKNOWN"
        return 1
    fi

    # Try readelf -n for .note.gnu.property
    local notes
    notes="$(readelf -n "$path" 2>/dev/null || true)"

    # Look for explicit x86-64-vN markers
    if printf '%s\n' "$notes" | grep -q 'x86-64-v[234]'; then
        local v_level
        v_level="$(printf '%s\n' "$notes" | grep -o 'x86-64-v[234]' | head -1)"
        echo "$v_level"
        return 0
    fi

    # Check architecture from file output
    local file_info
    file_info="$(file -b "$path" 2>/dev/null || echo "")"

    if [[ "$file_info" == *"x86-64"* || "$file_info" == *"x86_64"* ]]; then
        # No special features detected, assume baseline
        echo "x86-64 baseline (v1)"
        return 0
    fi

    echo "$UNKNOWN"
    return 1
}

##!
## @brief Map GLIBC version to compatible OS distributions.
## @param glibc_ver GLIBC version string (e.g., "GLIBC_2.17").
## @return OS compatibility description via stdout.
map_glibc_to_os() {
    local glibc_ver="$1"

    case "$glibc_ver" in
    "GLIBC_2.11" | "GLIBC_2.12")
        echo "RHEL/CentOS 6 (2010-2020, EOL), Debian 6 (EOL), Ubuntu 10.04-12.04 (EOL)"
        ;;
    "GLIBC_2.17")
        echo "RHEL/CentOS/Oracle Linux 7 (2014-2024), AstraLinux SE 1.7, SLES 12 (2014-2027)"
        ;;
    "GLIBC_2.23" | "GLIBC_2.24")
        echo "Ubuntu 16.04 LTS (2016-2026 ESM), Debian 9 (2017-2022)"
        ;;
    "GLIBC_2.27" | "GLIBC_2.28")
        echo "Ubuntu 18.04 LTS (2018-2028 ESM), Debian 10 (2019-2024), RHEL/CentOS 8 (2019-2029)"
        ;;
    "GLIBC_2.31" | "GLIBC_2.32")
        echo "Ubuntu 20.04 LTS (2020-2030 ESM), Debian 11 (2021-2026), RHEL/CentOS/Rocky/Alma 8.4+ (2021)"
        ;;
    "GLIBC_2.33" | "GLIBC_2.34")
        echo "Fedora 34-35 (2021-2022), Ubuntu 21.10-22.04 (2021-2022), RHEL/Rocky/Alma 9 (2022-2032)"
        ;;
    "GLIBC_2.35" | "GLIBC_2.36")
        echo "Ubuntu 22.04 LTS (2022-2032 ESM), Debian 12 Bookworm (2023-2028), Fedora 36-37"
        ;;
    "GLIBC_2.37" | "GLIBC_2.38")
        echo "Ubuntu 23.10+ (2023), Fedora 38-39 (2023-2024), Debian Testing/Sid (2023+)"
        ;;
    "GLIBC_2.39" | "GLIBC_2.40")
        echo "Ubuntu 24.04 LTS (2024-2034 ESM), Fedora 40+ (2024+), Debian Testing/Trixie (2025)"
        ;;
    *)
        if [[ "$glibc_ver" =~ GLIBC_2\.[0-9]+ ]]; then
            echo "Modern Linux distributions (recent releases)"
        else
            echo "Unknown or future release"
        fi
        ;;
    esac
}

##!
## @brief Map GLIBCXX version to GCC version and OS compatibility.
## @param glibcxx_ver GLIBCXX version string (e.g., "GLIBCXX_3.4.21").
## @return GCC/OS compatibility description via stdout.
map_glibcxx_to_gcc() {
    local glibcxx_ver="$1"

    case "$glibcxx_ver" in
    "GLIBCXX_3.4.19" | "GLIBCXX_3.4.20")
        echo "GCC 4.8-4.9 (RHEL/CentOS 7 devtoolset-4/6)"
        ;;
    "GLIBCXX_3.4.21")
        echo "GCC 5.x (Ubuntu 16.04, Debian 9, RHEL/CentOS 7 devtoolset-7)"
        ;;
    "GLIBCXX_3.4.22" | "GLIBCXX_3.4.23")
        echo "GCC 6.x (Ubuntu 16.10-17.04, Debian 9 backports)"
        ;;
    "GLIBCXX_3.4.24" | "GLIBCXX_3.4.25")
        echo "GCC 7.x-8.x (Ubuntu 18.04, Debian 10, RHEL/CentOS 8)"
        ;;
    "GLIBCXX_3.4.26" | "GLIBCXX_3.4.27" | "GLIBCXX_3.4.28")
        echo "GCC 9.x-10.x (Ubuntu 20.04, Debian 11, RHEL/CentOS/Rocky 8.5+)"
        ;;
    "GLIBCXX_3.4.29" | "GLIBCXX_3.4.30")
        echo "GCC 11.x-12.x (Ubuntu 22.04, Debian 12, RHEL/Rocky/Alma 9)"
        ;;
    "GLIBCXX_3.4.31" | "GLIBCXX_3.4.32")
        echo "GCC 13.x+ (Ubuntu 23.10/24.04, Debian Testing, Fedora 38+)"
        ;;
    *)
        if [[ "$glibcxx_ver" =~ GLIBCXX_3\.4\.[0-9]+ ]]; then
            echo "GCC 13+ or newer (recent distributions)"
        else
            echo "Unknown GCC version or future release"
        fi
        ;;
    esac
}

##!
## @brief Map CXXABI version to GCC version range.
## @param cxxabi_ver CXXABI version string (e.g., "CXXABI_1.3.15").
## @return GCC version range via stdout.
map_cxxabi_to_gcc() {
    local cxxabi_ver="$1"

    case "$cxxabi_ver" in
    "CXXABI_1.3")
        echo "GCC 3.4.x"
        ;;
    "CXXABI_1.3.1")
        echo "GCC 4.0.x"
        ;;
    "CXXABI_1.3.2")
        echo "GCC 4.1.x"
        ;;
    "CXXABI_1.3.3")
        echo "GCC 4.2.x"
        ;;
    "CXXABI_1.3.4")
        echo "GCC 4.3.x"
        ;;
    "CXXABI_1.3.5")
        echo "GCC 4.4.x"
        ;;
    "CXXABI_1.3.6")
        echo "GCC 4.5.x"
        ;;
    "CXXABI_1.3.7")
        echo "GCC 4.6.x"
        ;;
    "CXXABI_1.3.8")
        echo "GCC 4.7.x"
        ;;
    "CXXABI_1.3.9")
        echo "GCC 4.8.x"
        ;;
    "CXXABI_1.3.10")
        echo "GCC 4.9.x"
        ;;
    "CXXABI_1.3.11")
        echo "GCC 5.x"
        ;;
    "CXXABI_1.3.12")
        echo "GCC 6.x"
        ;;
    "CXXABI_1.3.13")
        echo "GCC 7.x-8.x"
        ;;
    "CXXABI_1.3.14")
        echo "GCC 9.x-11.0"
        ;;
    "CXXABI_1.3.15")
        echo "GCC 11.1-11.2"
        ;;
    "CXXABI_1.3.16")
        echo "GCC 12.x"
        ;;
    "CXXABI_1.3.17")
        echo "GCC 13.x"
        ;;
    "CXXABI_1.3.18")
        echo "GCC 14.x+"
        ;;
    *)
        if [[ "$cxxabi_ver" =~ CXXABI_1\.3\.[0-9]+ ]]; then
            # Extract minor version number
            local minor_ver
            minor_ver="$(echo "$cxxabi_ver" | grep -oE '[0-9]+$')"
            if [[ -n "$minor_ver" ]]; then
                if [[ $minor_ver -ge 18 ]]; then
                    echo "GCC 14.x+ (future versions)"
                elif [[ $minor_ver -ge 17 ]]; then
                    echo "GCC 13.x"
                elif [[ $minor_ver -ge 16 ]]; then
                    echo "GCC 12.x"
                elif [[ $minor_ver -ge 15 ]]; then
                    echo "GCC 11.1-11.2"
                elif [[ $minor_ver -ge 14 ]]; then
                    echo "GCC 9.x-11.0"
                else
                    echo "GCC < 9.x (legacy)"
                fi
            else
                echo "Unknown GCC version"
            fi
        else
            echo "Unknown CXXABI version"
        fi
        ;;
    esac
}

##!
## @brief Extract GCC major version from version string.
## @param version Version string (e.g., "14.2.0" or "11.1").
## @return Major version number via stdout.
extract_gcc_major_version() {
    local version="$1"
    if [[ "$version" =~ ^([0-9]+)\. ]]; then
        echo "${BASH_REMATCH[1]}"
    else
        echo ""
    fi
}

##!
## @brief Determine most likely GCC version range based on GLIBCXX, CXXABI, and actual compiler version.
## @param glibcxx_ver GLIBCXX version string (e.g., "GLIBCXX_3.4.26").
## @param cxxabi_ver CXXABI version string (e.g., "CXXABI_1.3.15").
## @param compiler_version Actual compiler version from .comment section (e.g., "14.2.0").
## @return GCC version range via stdout with confidence level.
determine_gcc_version_range() {
    local glibcxx_ver="$1"
    local cxxabi_ver="$2"
    local compiler_version="${3:-}"
    local glibcxx_gcc=""
    local cxxabi_gcc=""
    local actual_gcc_major=""
    local actual_gcc_range=""

    # Extract actual GCC version if available (HIGHEST PRIORITY)
    if [[ -n "$compiler_version" && "$compiler_version" != "unknown" ]]; then
        actual_gcc_major="$(extract_gcc_major_version "$compiler_version")"
        if [[ -n "$actual_gcc_major" ]]; then
            case "$actual_gcc_major" in
            14)
                actual_gcc_range="GCC 14.x"
                ;;
            13)
                actual_gcc_range="GCC 13.x"
                ;;
            12)
                actual_gcc_range="GCC 12.x"
                ;;
            11)
                actual_gcc_range="GCC 11.x"
                ;;
            10)
                actual_gcc_range="GCC 10.x"
                ;;
            9)
                actual_gcc_range="GCC 9.x"
                ;;
            *)
                actual_gcc_range="GCC ${actual_gcc_major}.x"
                ;;
            esac
        fi
    fi

    # Get GCC range from GLIBCXX
    if [[ -n "$glibcxx_ver" && "$glibcxx_ver" != "unknown" ]]; then
        glibcxx_gcc="$(map_glibcxx_to_gcc "$glibcxx_ver")"
    fi

    # Get GCC range from CXXABI
    if [[ -n "$cxxabi_ver" && "$cxxabi_ver" != "unknown" ]]; then
        cxxabi_gcc="$(map_cxxabi_to_gcc "$cxxabi_ver")"
    fi

    # PRIORITY 1: Use actual compiler version if available (100% confidence)
    if [[ -n "$actual_gcc_range" ]]; then
        local mismatch_warnings=()
        
        # Verify consistency with symbol versions
        if [[ -n "$glibcxx_gcc" ]]; then
            local glibcxx_major
            glibcxx_major="$(echo "$glibcxx_gcc" | grep -oE 'GCC [0-9]+' | grep -oE '[0-9]+' || echo "")"
            if [[ -n "$glibcxx_major" && "$glibcxx_major" != "$actual_gcc_major" ]]; then
                # Check if GLIBCXX requires newer GCC than actual
                if [[ $glibcxx_major -gt $actual_gcc_major ]]; then
                    mismatch_warnings+=("WARNING: GLIBCXX ${glibcxx_ver} suggests GCC ${glibcxx_major}.x, but binary was compiled with GCC ${actual_gcc_major}.x. This may indicate missing symbols at runtime.")
                fi
            fi
        fi
        
        if [[ -n "$cxxabi_gcc" ]]; then
            local cxxabi_major
            cxxabi_major="$(echo "$cxxabi_gcc" | grep -oE 'GCC [0-9]+' | grep -oE '[0-9]+' || echo "")"
            if [[ -n "$cxxabi_major" && "$cxxabi_major" != "$actual_gcc_major" ]]; then
                # CXXABI is usually backward compatible, so only warn if it requires much newer
                if [[ $cxxabi_major -gt $actual_gcc_major ]]; then
                    mismatch_warnings+=("NOTE: CXXABI ${cxxabi_ver} suggests GCC ${cxxabi_major}.x, but binary was compiled with GCC ${actual_gcc_major}.x. This is usually OK due to backward compatibility.")
                fi
            fi
        fi
        
        if ((${#mismatch_warnings[@]})); then
            echo "${actual_gcc_range} (${compiler_version} - CONFIRMED from .comment section)"
            printf '%s\n' "${mismatch_warnings[@]}" | sed 's/^/  ⚠ /'
        else
            echo "${actual_gcc_range} (${compiler_version} - CONFIRMED from .comment section, consistent with symbol versions)"
        fi
        return 0
    fi

    # PRIORITY 2: If both GLIBCXX and CXXABI are available, find intersection
    if [[ -n "$glibcxx_gcc" && -n "$cxxabi_gcc" ]]; then
        # Try to find common range
        if [[ "$glibcxx_gcc" == *"GCC 11"* && "$cxxabi_gcc" == *"GCC 11"* ]]; then
            echo "GCC 11.1-11.2 (determined by both GLIBCXX and CXXABI)"
        elif [[ "$glibcxx_gcc" == *"GCC 12"* && "$cxxabi_gcc" == *"GCC 12"* ]]; then
            echo "GCC 12.x (determined by both GLIBCXX and CXXABI)"
        elif [[ "$glibcxx_gcc" == *"GCC 13"* && "$cxxabi_gcc" == *"GCC 13"* ]]; then
            echo "GCC 13.x (determined by both GLIBCXX and CXXABI)"
        elif [[ "$glibcxx_gcc" == *"GCC 14"* && "$cxxabi_gcc" == *"GCC 14"* ]]; then
            echo "GCC 14.x+ (determined by both GLIBCXX and CXXABI)"
        else
            # Return both ranges with note about conflict
            echo "GLIBCXX suggests: ${glibcxx_gcc}; CXXABI suggests: ${cxxabi_gcc}"
            echo "  ⚠ WARNING: Conflicting version hints. Actual compiler version could not be determined from .comment section."
        fi
    elif [[ -n "$glibcxx_gcc" ]]; then
        echo "$glibcxx_gcc (from GLIBCXX, compiler version not found in .comment)"
    elif [[ -n "$cxxabi_gcc" ]]; then
        echo "$cxxabi_gcc (from CXXABI, compiler version not found in .comment)"
    else
        echo "Unknown (no version information available)"
    fi
}

##!
## @brief Analyze GLIBC version compatibility and provide detailed guidance.
## @param ver_a GLIBC version for binary A.
## @param ver_b GLIBC version for binary B.
## @return Compatibility analysis via stdout.
analyze_glibc_compatibility() {
    local ver_a="$1"
    local ver_b="$2"

    # Determine which needs newer runtime
    if version_gt "$ver_a" "$ver_b"; then
        echo "BINARY_A_NEWER"
    elif version_gt "$ver_b" "$ver_a"; then
        echo "BINARY_B_NEWER"
    else
        echo "SAME"
    fi
}

##!
## @brief Check artifact type compatibility for comparison analysis.
## @param a Artifact type of binary A.
## @param b Artifact type of binary B.
## @return Status via stdout: INCOMPATIBLE, PARTIAL, or COMPATIBLE.
## @details
##   Returns INCOMPATIBLE for fundamentally different types (e.g., shared-object vs executable),
##   PARTIAL for different but analyzable types (e.g., shared-object vs static-archive),
##   COMPATIBLE for same types or unknown types.
check_artifact_compatibility() {
    local a="$1"
    local b="$2"

    # Защита от пустых/unknown: не блокируем анализ
    if [[ -z "$a" || -z "$b" || "$a" == "unknown" || "$b" == "unknown" ]]; then
        echo "COMPATIBLE"
        return 0
    fi

    # Одинаковые классы — совместимы
    if [[ "$a" == "$b" ]]; then
        echo "COMPATIBLE"
        return 0
    fi

    # Частично совместимая пара (симметрично)
    if { [[ "$a" == "shared-object" && "$b" == "static-archive" ]] ||
        [[ "$a" == "static-archive" && "$b" == "shared-object" ]]; }; then
        echo "PARTIAL"
        return 0
    fi

    # Критически несовместимые пары (симметрично)
    if { [[ "$a" == "shared-object" && "$b" == "executable" ]] ||
        [[ "$a" == "executable" && "$b" == "shared-object" ]]; } ||
        { [[ "$a" == "shared-object" && "$b" == "elf-object" ]] ||
            [[ "$a" == "elf-object" && "$b" == "shared-object" ]]; } ||
        { [[ "$a" == "executable" && "$b" == "static-archive" ]] ||
            [[ "$a" == "static-archive" && "$b" == "executable" ]]; } ||
        { [[ "$a" == "executable" && "$b" == "elf-object" ]] ||
            [[ "$a" == "elf-object" && "$b" == "executable" ]]; } ||
        { [[ "$a" == "static-archive" && "$b" == "elf-object" ]] ||
            [[ "$a" == "elf-object" && "$b" == "static-archive" ]]; }; then
        echo "INCOMPATIBLE"
        return 0
    fi

    # На всякий случай — по умолчанию совместимо
    echo "COMPATIBLE"
    return 0
}

##!
## @brief Display CLI synopsis, section descriptions, and config samples.
print_usage() {
    local exe="./${SCRIPT_BASENAME}"
    cat <<EOF
Usage: ${SCRIPT_BASENAME} [options] <path>

Options:
  -c, --config FILE        Load configuration overrides from FILE
      --enable SECTION     Enable specific analysis section (can repeat)
      --disable SECTION    Disable specific analysis section (can repeat)
      --list-sections      Show available sections and exit
      --no-color           Disable ANSI colors
      --tips-only          Skip per-file details, print aggregated hints
      --compare BIN_A BIN_B
                           Compare two binaries and show diff/summary
      --compare-mode MODE  Comparison depth (full|fast), default: ${DEFAULT_COMPARE_MODE}
      --force-analysis     Force compatibility analysis even for incompatible artifact types
  -h, --help               Show this help and exit

Sections:
  interpreter  - Inspect PT_INTERP / dynamic loader hints
  dynamic      - Report DT_NEEDED / RPATH / RUNPATH
  deps         - Resolve dependency tree via lddtree/ldd
  versions     - Detect GLIBC / GLIBCXX version requirements
  arch         - Identify ISA flags and ABI bits
  abi          - Inspect C++ ABI markers (\`std::__cxx11\`, \`_GLIBCXX_USE_CXX11_ABI\`)
  security     - Check GNU_STACK/RELRO/BIND_NOW hardening signals
  graphics     - Flag GL/X11/Wayland/QT multimedia dependencies
  nss          - Flag NSS/TLS/system libs that must stay system-provided
  bundle       - Audit entire directory for forbidden/duplicate runtimes
  tips         - Emit dynamic test matrix and LD_DEBUG guidance

Examples:
  ${exe} ./bin/my_app
  ${exe} --disable graphics --config ./audit.conf ./dist

Config samples:
  # ~/.config/bincompat.conf  (desktop bundle default)
  enable_sections="interpreter dynamic deps versions arch abi security bundle tips"
  disable_sections="graphics nss"
  tips_only=0
  color=1

  # ci/bincompat-fast.conf    (CI smoke-only)
  enable_sections="deps versions tips"
  disable_sections="graphics nss bundle abi arch interpreter dynamic security"
  tips_only=1
  color=0

  # configs/bincompat-deep.conf (graphics + NSS deep dive)
  enable_sections="interpreter dynamic deps versions arch abi graphics nss bundle tips"
  disable_sections=""
  tips_only=0
  color=1

Notes:
  - Default config path: ${DEFAULT_CONFIG} (loaded automatically when present).
  - Lists in enable/disable entries are whitespace-separated; later CLI flags override config.
  - Use \$ORIGIN in RPATH / RUNPATH to keep bundles relocatable.
  - Compare mode enforces artifact-type parity (e.g., .so vs .a counts as incompatibility).
EOF
}

list_sections() {
    printf "Available sections:\n"
    local section
    for section in ${DEFAULT_SECTIONS// /$'\n'}; do
        printf "  - %s\n" "$section"
    done
}

init_sections() {
    local section
    for section in ${DEFAULT_SECTIONS// /$'\n'}; do
        SECTION_ENABLED["$section"]=1
    done
}

enable_section() {
    local section="$1"
    SECTION_ENABLED["$section"]=1
}

disable_section() {
    local section="$1"
    SECTION_ENABLED["$section"]=0
}

section_enabled() {
    local section="$1"
    [[ "${SECTION_ENABLED[$section]:-0}" -eq 1 ]]
}

##!
## @brief Load INI-style overrides controlling sections, colors, and tips.
## @param config_file Path to config file, ignored when absent.
load_config() {
    local config_file="$1"
    if [[ -z "$config_file" ]]; then
        return
    fi
    if [[ ! -f "$config_file" ]]; then
        log WARN "Config file '$config_file' not found; skipping."
        return
    fi
    log INFO "Loading config from ${config_file}"
    while IFS='=' read -r key value; do
        key="${key#"${key%%[![:space:]]*}"}"
        key="${key%"${key##*[![:space:]]}"}"
        value="${value#"${value%%[![:space:]]*}"}"
        value="${value%"${value##*[![:space:]]}"}"
        [[ -z "$key" || "$key" == \#* ]] && continue
        case "$key" in
        enable_sections)
            for sec in ${value// /$'\n'}; do
                enable_section "$sec"
            done
            ;;
        disable_sections)
            for sec in ${value// /$'\n'}; do
                disable_section "$sec"
            done
            ;;
        tips_only)
            [[ "$value" =~ ^(1|true|yes)$ ]] && TIPS_ONLY=1
            ;;
        color)
            [[ "$value" =~ ^(0|false|no)$ ]] && USE_COLOR=0
            ;;
        *)
            log WARN "Unknown config key: $key"
            ;;
        esac
    done <"$config_file"
}

check_tool() {
    local tool="$1"
    if command -v "$tool" >/dev/null 2>&1; then
        TOOL_STATUS["$tool"]=1
        return 0
    fi
    TOOL_STATUS["$tool"]=0
    local pkg="${TOOL_PACKAGES[$tool]:-package not documented}"
    log WARN "Missing tool '${tool}'. Install package: ${pkg}."
    return 1
}

tool_available() {
    local tool="$1"
    [[ "${TOOL_STATUS[$tool]:-0}" -eq 1 ]]
}

##!
## @brief Populate TOOL_STATUS map based on availability of helper binaries.
preflight_tools() {
    local tool
    for tool in readelf objdump strings file awk sed grep find; do
        check_tool "$tool" || true
    done
    check_tool patchelf || true
    check_tool lddtree || true
    check_tool nm || true
    check_tool ar || true
    check_tool tput || true
}

##!
## @brief Expand input path into a NUL-delimited list of auditable files.
## @details
##   Directories are scanned for executables, .so*, and .a archives. Regular
##   files are returned verbatim to allow per-file auditing.
detect_targets() {
    local input="$1"
    if [[ -d "$input" ]]; then
        find "$input" -type f \( -perm -111 -o -name '*.so*' -o -name '*.a' \) -print0
    else
        printf "%s\0" "$input"
    fi
}

is_elf() {
    local path="$1"
    file -b "$path" 2>/dev/null | grep -q 'ELF'
}

is_archive() {
    local path="$1"
    file -b "$path" 2>/dev/null | grep -q 'current ar archive'
}

get_interpreter_path() {
    local path="$1"
    tool_available readelf || return 1
    readelf -l "$path" 2>/dev/null | awk '/Requesting program interpreter:/ {gsub(/\[|\]/,"",$4); print $4; exit}'
}

collect_dependency_tree() {
    local path="$1"
    local __out_var="${2:-}"
    local __src_var="${3:-}"
    local data=""
    local source=""
    if tool_available lddtree; then
        source="lddtree"
        data="$(lddtree "$path" 2>/dev/null || true)"
    else
        local interp_path
        interp_path="$(get_interpreter_path "$path" || true)"
        if [[ -n "$interp_path" && -x "$interp_path" ]]; then
            source="${interp_path} --list"
            data="$("$interp_path" --list "$path" 2>&1 || true)"
        fi
    fi
    [[ -n "$__out_var" ]] && printf -v "$__out_var" '%s' "$data"
    [[ -n "$__src_var" ]] && printf -v "$__src_var" '%s' "$source"
    [[ -n "$data" ]] && return 0
    return 1
}

analyze_interpreter() {
    local path="$1"
    section_enabled interpreter || return 0
    section_header "Interpreter / Loader" "$path"
    if ! tool_available readelf; then
        log WARN "readelf not available; skipping interpreter analysis."
        return 0
    fi
    local interp
    if ! interp=$(readelf -l "$path" 2>/dev/null | sed -n '/INTERP/,+1p'); then
        log WARN "Cannot read PT_INTERP for $path"
        return 0
    fi
    log_block INFO "PT_INTERP info:" "$interp"
    if tool_available patchelf; then
        local rpath
        rpath="$(patchelf --print-rpath "$path" 2>/dev/null || true)"
        if [[ -n "$rpath" ]]; then
            log_colon_list INFO "patchelf RPATH entries:" "$rpath"
        fi
    fi
}

analyze_dynamic() {
    local path="$1"
    section_enabled dynamic || return 0
    section_header "Dynamic Tags" "$path"
    if ! tool_available readelf; then
        log WARN "readelf not available; skipping DT_* analysis."
        return 0
    fi
    local tags
    if ! tags=$(readelf -d "$path" 2>/dev/null | grep -E 'NEEDED|RPATH|RUNPATH' || true); then
        log WARN "Failed to read dynamic tags"
        return 0
    fi
    if [[ -z "$tags" ]]; then
        log INFO "No dynamic tags found (likely static binary)."
    else
        log_block INFO "Dynamic tags:" "$tags"
        if echo "$tags" | grep -q 'RPATH'; then
            if ! echo "$tags" | grep -q "\$ORIGIN"; then
                log WARN "RPATH missing \$ORIGIN — may break portability."
            fi
        fi
    fi
}

analyze_deps() {
    local path="$1"
    section_enabled deps || return 0
    section_header "Dependency Tree" "$path"
    local output=""
    local source_label=""
    if collect_dependency_tree "$path" output source_label; then
        if [[ -n "$output" ]]; then
            log_block INFO "Dependencies via ${source_label:-unknown}:" "$output"
        else
            log WARN "Dependency resolver produced no output."
        fi
    else
        log WARN "Unable to resolve dependencies; install pax-utils or ensure loader executable."
    fi
    if [[ -n "$output" ]]; then
        local forbidden
        forbidden="$(echo "$output" | grep -E 'libc\.so\.6|ld-linux|libnss_|libGL' || true)"
        if [[ -n "$forbidden" ]]; then
            log_block WARN "Detected system-sensitive libs (ensure not bundled):" "$forbidden"
        fi
    fi
}

analyze_versions() {
    local path="$1"
    section_enabled versions || return 0
    section_header "GLIBC / GLIBCXX / CXXABI Versions" "$path"
    
    # Use metadata collection for comprehensive analysis
    declare -A META=()
    if collect_metadata "$path" META "" "fast"; then
        local min_glibc="${META[MIN_GLIBC]:-unknown}"
        local min_glibcxx="${META[MIN_GLIBCXX]:-unknown}"
        local min_cxxabi="${META[MIN_CXXABI]:-unknown}"
        local min_glibc_functions="${META[MIN_GLIBC_FUNCTIONS]:-n/a}"
        local min_glibcxx_functions="${META[MIN_GLIBCXX_FUNCTIONS]:-n/a}"
        local min_cxxabi_functions="${META[MIN_CXXABI_FUNCTIONS]:-n/a}"
        local cpp_stdlib="${META[CPP_STDLIB]:-unknown}"

        # GLIBC
        if [[ "$min_glibc" != "unknown" ]]; then
            log_block INFO "GLIBC symbols:" "${META[GLIBC_SYMBOLS]:-}"
            log INFO "GLIBC minimum requirement: $min_glibc"
            if [[ "$min_glibc_functions" != "n/a" ]]; then
                log INFO "  Requires $min_glibc for: $min_glibc_functions"
            fi
        else
            log INFO "No GLIBC versioned symbols exposed."
        fi

        # GLIBCXX
        if [[ "$cpp_stdlib" == "libc++ (LLVM)" ]]; then
            log INFO "C++ Standard Library: libc++ (LLVM) - GLIBCXX versions do not apply"
        elif [[ "$min_glibcxx" != "unknown" ]]; then
            log_block INFO "GLIBCXX symbols:" "${META[GLIBCXX_SYMBOLS]:-}"
            log INFO "GLIBCXX minimum requirement: $min_glibcxx"
            if [[ "$min_glibcxx_functions" != "n/a" && "$min_glibcxx_functions" != "n/a (uses"* ]]; then
                log INFO "  Requires $min_glibcxx for: $min_glibcxx_functions"
            fi
            local gcc_range
            gcc_range="$(map_glibcxx_to_gcc "$min_glibcxx")"
            log INFO "  Likely compiled with: $gcc_range"
        else
            log INFO "No GLIBCXX versioned symbols detected."
        fi

        # CXXABI
        if [[ "$cpp_stdlib" == "libc++ (LLVM)" ]]; then
            log INFO "CXXABI: Not applicable (uses libc++)"
        elif [[ "$min_cxxabi" != "unknown" ]]; then
            log_block INFO "CXXABI symbols:" "${META[CXXABI_SYMBOLS]:-}"
            log INFO "CXXABI minimum requirement: $min_cxxabi"
            if [[ "$min_cxxabi_functions" != "n/a" ]]; then
                log INFO "  Requires $min_cxxabi for: $min_cxxabi_functions"
            fi
            local cxxabi_gcc_range
            cxxabi_gcc_range="$(map_cxxabi_to_gcc "$min_cxxabi")"
            log INFO "  Likely compiled with: $cxxabi_gcc_range"
        else
            log INFO "No CXXABI versioned symbols detected."
        fi

        # Combined GCC version determination
        if [[ "$cpp_stdlib" == "libstdc++ (GNU)" ]]; then
            local compiler_version="${META[COMPILER_VERSION]:-unknown}"
            local compiler_name="${META[COMPILER_NAME]:-unknown}"
            local compiler_tag="${META[COMPILER_TAG]:-unknown}"
            
            log INFO ""
            log SECTION "Compiler Information" ""
            
            # Show actual compiler version if available
            if [[ "$compiler_tag" != "unknown" ]]; then
                log INFO "Compiler tag: $compiler_tag"
                if [[ "$compiler_name" != "unknown" && "$compiler_version" != "unknown" ]]; then
                    log INFO "Compiler: $compiler_name $compiler_version"
                fi
            else
                log WARN "Compiler version not found in .comment section"
            fi
            
            log INFO ""
            log SECTION "Most Likely GCC Version Range" ""
            
            local combined_gcc_range
            combined_gcc_range="$(determine_gcc_version_range "$min_glibcxx" "$min_cxxabi" "$compiler_version")"
            
            # Split output if it contains warnings
            if echo "$combined_gcc_range" | grep -q "⚠"; then
                local main_line
                main_line="$(echo "$combined_gcc_range" | head -1)"
                local warnings
                warnings="$(echo "$combined_gcc_range" | tail -n +2)"
                log INFO "$main_line"
                if [[ -n "$warnings" ]]; then
                    echo "$warnings" | while IFS= read -r warning; do
                        [[ -n "$warning" ]] && log WARN "$warning"
                    done
                fi
            else
                log INFO "$combined_gcc_range"
            fi
        fi
    else
        # Fallback to old method
        if tool_available objdump; then
            local glibc glibcxx cxxabi
            glibc="$(objdump -T "$path" 2>/dev/null | grep -o 'GLIBC_[0-9.]*' | sort -V | uniq || true)"
            glibcxx="$(objdump -T "$path" 2>/dev/null | grep -o 'GLIBCXX_[0-9.]*' | sort -V | uniq || true)"
            cxxabi="$(objdump -T "$path" 2>/dev/null | grep -o 'CXXABI_[0-9.]*' | sort -V | uniq || true)"
            if [[ -n "$glibc" ]]; then
                log_block INFO "GLIBC symbols:" "$glibc"
                log INFO "GLIBC minimum requirement: $(echo "$glibc" | tail -n1)"
            else
                log INFO "No GLIBC versioned symbols exposed."
            fi
            if [[ -n "$glibcxx" ]]; then
                log_block INFO "GLIBCXX symbols:" "$glibcxx"
                log INFO "GLIBCXX minimum requirement: $(echo "$glibcxx" | tail -n1)"
            else
                log INFO "No GLIBCXX versioned symbols detected."
            fi
            if [[ -n "$cxxabi" ]]; then
                log_block INFO "CXXABI symbols:" "$cxxabi"
                log INFO "CXXABI minimum requirement: $(echo "$cxxabi" | tail -n1)"
            else
                log INFO "No CXXABI versioned symbols detected."
            fi
        else
            log WARN "objdump missing; attempting strings-based heuristic."
            if tool_available strings; then
                local versions
                versions="$(strings "$path" 2>/dev/null | grep -E 'GLIBC(X|XX|ABI)_[0-9.]+' | sort -V | uniq || true)"
                if [[ -n "$versions" ]]; then
                    log_block INFO "Detected versions via strings:" "$versions"
                else
                    log WARN "No version hints found."
                fi
            fi
        fi
    fi
}

analyze_arch() {
    local path="$1"
    section_enabled arch || return 0
    section_header "Architecture / ISA" "$path"
    if ! tool_available file; then
        log WARN "file command unavailable; cannot inspect ABI."
        return 0
    fi
    local info
    info="$(file -b "$path" 2>/dev/null || true)"
    [[ -n "$info" ]] && log_block INFO "file(1) description:" "$info"
    if tool_available readelf; then
        local props
        props="$(readelf -A "$path" 2>/dev/null | grep -E 'ISA|FEATURE' || true)"
        [[ -n "$props" ]] && log_block INFO "GNU_PROPERTY entries:" "$props"
        if echo "$props" | grep -q 'x86-64-v3'; then
            log WARN "Requires x86-64-v3 baseline; older CPUs will fail."
        fi
    fi
}

analyze_abi() {
    local path="$1"
    section_enabled abi || return 0
    section_header "C++ ABI Consistency" "$path"
    local has_new_abi=0
    local detection_method=""

    # Method 1: Check mangled symbols via nm (MOST RELIABLE)
    # Use explicit check to avoid SIGPIPE issues with grep -q
    if tool_available nm; then
        local nm_result
        nm_result="$(nm -D "$path" 2>/dev/null | grep '_ZNSt7__cxx11' | head -1 || true)"
        if [[ -n "$nm_result" ]]; then
            has_new_abi=1
            detection_method="mangled symbols (nm)"
        else
            nm_result="$(nm "$path" 2>/dev/null | grep '_ZNSt7__cxx11' | head -1 || true)"
            if [[ -n "$nm_result" ]]; then
                has_new_abi=1
                detection_method="mangled symbols (nm, all)"
            fi
        fi
    fi

    # Method 2: Check via objdump (also reliable)
    if [[ "$has_new_abi" -eq 0 ]] && tool_available objdump; then
        local objdump_result
        objdump_result="$(objdump -T "$path" 2>/dev/null | grep '_ZNSt7__cxx11' | head -1 || true)"
        if [[ -n "$objdump_result" ]]; then
            has_new_abi=1
            detection_method="mangled symbols (objdump)"
        fi
    fi

    # Method 3: Check strings (less reliable, but can help)
    if [[ "$has_new_abi" -eq 0 ]] && tool_available strings; then
        local strings_result
        strings_result="$(strings "$path" 2>/dev/null | grep 'std::__cxx11' | head -1 || true)"
        if [[ -n "$strings_result" ]]; then
            has_new_abi=1
            detection_method="string literals"
        fi
    fi

    # Check for legacy ABI markers
    local has_legacy_abi=0
    if tool_available nm; then
        local legacy_result
        legacy_result="$(nm -D "$path" 2>/dev/null | grep -E '_ZNSs|_ZNSaIcE' | head -1 || true)"
        if [[ -n "$legacy_result" ]]; then
            has_legacy_abi=1
        fi
    fi

    # Report findings
    if tool_available strings; then
        local abi_ref
        abi_ref="$(strings "$path" 2>/dev/null | grep '_GLIBCXX_USE_CXX11_ABI' | head -1 || true)"
        if [[ -n "$abi_ref" ]]; then
            log INFO "Binary embeds _GLIBCXX_USE_CXX11_ABI references (likely built with header toggles)."
        fi
    fi

    if [[ "$has_new_abi" -eq 1 ]]; then
        log INFO "Symbols reference std::__cxx11 namespace (detected via ${detection_method}) ---> ABI=1 (C++11 ABI)."
    elif [[ "$has_legacy_abi" -eq 1 ]]; then
        log INFO "Legacy ABI symbols detected (_ZNSs, _ZNSaIcE) ---> ABI=0 (legacy)."
    else
        log INFO "No std::__cxx11 markers detected ---> ABI likely 0 (legacy)."
    fi

    if tool_available nm; then
        local undefined
        undefined="$(nm -An --undefined-only "$path" 2>/dev/null | grep 'GLIBCXX' || true)"
        if [[ -n "$undefined" ]]; then
            log_block WARN "Undefined libstdc++ symbols (unresolved references that must be provided by libstdc++ at runtime, that means that the binary is not fully statically linked and their addresses are 0x00000000):" "$undefined"
        fi
    fi
}

analyze_security() {
    local path="$1"
    section_enabled security || return 0
    section_header "Security Flags" "$path"
    declare -A SECINFO=()
    if ! gather_security_info "$path" "full" SECINFO; then
        log WARN "readelf unavailable; cannot inspect GNU_STACK/RELRO."
        return 0
    fi
    if [[ -n "${SECINFO[elf_type]:-}" ]]; then
        if echo "${SECINFO[elf_type]}" | grep -q 'EXEC'; then
            log WARN "ELF type: ${SECINFO[elf_type]} (PIE disabled / ET_EXEC)."
        elif echo "${SECINFO[elf_type]}" | grep -q 'DYN'; then
            log INFO "ELF type: ${SECINFO[elf_type]} (PIE-friendly / ET_DYN)."
        else
            log INFO "ELF type: ${SECINFO[elf_type]}"
        fi
    fi
    if [[ -n "${SECINFO[stack_perms]:-}" ]]; then
        if [[ "${SECINFO[stack_perms]}" == *E* ]]; then
            log WARN "GNU_STACK is executable (${SECINFO[stack_perms]}); stack NX is disabled."
        else
            log INFO "GNU_STACK permissions: ${SECINFO[stack_perms]} (non-executable stack)."
        fi
    else
        log WARN "GNU_STACK entry missing — toolchains on Debian-based systems expect NX annotation."
    fi
    if [[ "${SECINFO[relro_status]}" == "full" ]]; then
        log INFO "RELRO status: Full (RELRO + BIND_NOW)."
    elif [[ "${SECINFO[relro_status]}" == "partial" ]]; then
        log WARN "RELRO status: Partial (missing BIND_NOW)."
    else
        log WARN "RELRO status: Disabled."
    fi
    if [[ "${SECINFO[bind_now]}" == "yes" ]]; then
        log INFO "BIND_NOW present — immediate symbol resolution enforced."
    else
        log WARN "BIND_NOW absent — lazy binding allowed."
    fi
    if [[ "${SECINFO[textrel]}" == "yes" ]]; then
        log WARN "DT_TEXTREL detected — binary writes to text segments."
    fi
}

analyze_graphics() {
    local path="$1"
    section_enabled graphics || return 0
    section_header "Graphics / Multimedia deps" "$path"
    local deps
    if ! collect_dependency_tree "$path" deps _; then
        log WARN "Cannot inspect graphics deps; install pax-utils or ensure loader executable."
        return 0
    fi
    if [[ -z "$deps" ]]; then
        log INFO "No dependency data available."
        return 0
    fi
    local gl
    gl="$(echo "$deps" | grep -E 'lib(GL|GLX|EGL|gbm|X11|xcb|wayland|pulse|asound|freetype|fontconfig)' || true)"
    if [[ -n "$gl" ]]; then
        log_block INFO "Graphics/Audio libs detected:" "$gl"
        if echo "$gl" | grep -q 'libGL\.so'; then
            log WARN "libGL bundled/required — ensure compatibility with vendor drivers."
        fi
    else
        log INFO "No graphics/multimedia libs referenced."
    fi
}

analyze_nss_tls() {
    local path="$1"
    section_enabled nss || return 0
    section_header "NSS / TLS / Critical System Libs" "$path"
    local deps
    if ! collect_dependency_tree "$path" deps _; then
        log WARN "Cannot inspect libs; dependency resolver unavailable."
        return 0
    fi
    if [[ -z "$deps" ]]; then
        log INFO "No dependency info available."
        return 0
    fi
    local sensitive
    sensitive="$(echo "$deps" | grep -E 'libnss|libssl|libcrypto|libpthread|libgcc_s' || true)"
    if [[ -n "$sensitive" ]]; then
        log_block INFO "Sensitive system libs:" "$sensitive"
        if echo "$sensitive" | grep -q 'libnss'; then
            log WARN "Do NOT bundle NSS modules; rely on system copies."
        fi
    else
        log INFO "No NSS/TLS-specific libs detected."
    fi
}

analyze_bundle() {
    local root="$1"
    section_enabled bundle || return 0
    [[ ! -d "$root" ]] && return 0
    section_header "Bundle Inventory" "$root"
    local forbidden
    forbidden="$(find "$root" -maxdepth 2 -type f \( -name 'libc.so*' -o -name 'ld-linux*.so*' -o -name 'libnss_*.so*' -o -name 'libGL*.so*' \) 2>/dev/null || true)"
    if [[ -n "$forbidden" ]]; then
        log_block WARN "Potentially dangerous bundled system libs:" "$forbidden"
    else
        log INFO "No forbidden glibc/NSS/GL loaders bundled."
    fi
}

##!
## @brief Print dynamic-validation playbook for Docker-based smoke tests.
emit_tips() {
    section_enabled tips || return 0
    section_header "Dynamic Validation Tips" ""
    cat <<'EOF'
- Recommended clean-env smoke tests:
    docker run --rm -v "$PWD:/t" -w /t ubuntu:22.04 bash -lc \
      'apt-get update && apt-get install -y libx11-6 libxcb1 && LD_DEBUG=libs,versions ./your_app'
    docker run --rm -v "$PWD:/t" -w /t quay.io/centos/centos:7 bash -lc \
      'yum install -y glibc-locale-source && LD_DEBUG=libs ./your_app'
- Capture loader diagnostics with: LD_DEBUG=libs,versions,reloc ./your_app 2>&1 | tee lddebug.log
- Negative test: unset LD_LIBRARY_PATH and ensure bundled RUNPATH is sufficient.
- Verify `_GLIBCXX_USE_CXX11_ABI` alignment for every plugin/so before shipping.
EOF
}

section_header() {
    local title="$1"
    local target="$2"
    if [[ -n "$target" ]]; then
        log SECTION "== ${title}: ${target} =="
    else
        log SECTION "== ${title} =="
    fi
}

##!
## @brief Drive section-by-section analysis for binaries, archives, or bundles.
## @param path Filesystem path selected for auditing.
## @details
##   Detects type (ELF/archive/dir) and dispatches to specialized analyzers.
analyze_target() {
    local path="$1"
    if [[ ! -e "$path" ]]; then
        log ERROR "Path not found: $path"
        return 1
    fi
    if [[ -d "$path" ]]; then
        log INFO "Entering bundle directory $path"
        analyze_bundle "$path"
        return 0
    fi
    if is_elf "$path"; then
        section_header "ELF Overview" "$path"
        log INFO "SHA256: $(sha256sum "$path" 2>/dev/null | awk '{print $1}')"
        analyze_interpreter "$path"
        analyze_dynamic "$path"
        analyze_deps "$path"
        analyze_versions "$path"
        analyze_arch "$path"
        analyze_abi "$path"
        analyze_security "$path"
        analyze_graphics "$path"
        analyze_nss_tls "$path"
    elif is_archive "$path"; then
        section_header "Static Archive" "$path"
        if tool_available ar; then
            log INFO "Members: $(ar t "$path" 2>/dev/null | wc -l | xargs) object files"
        fi
        if tool_available nm; then
            local glibc
            glibc="$(nm -An "$path" 2>/dev/null | grep GLIBC || true)"
            [[ -n "$glibc" ]] && log INFO "GLIBC references inside archive:\n${glibc}"
        fi
    else
        log WARN "Skipping unsupported file type: $path"
    fi
}

##!
## @brief Application entry point coordinating CLI parsing and processing loop.
main() {
    init_colors
    init_sections
    local config_file=""
    TIPS_ONLY=0

    if [[ ${BASH_VERSINFO[0]} -lt 4 ]]; then
        log ERROR "Bash 4+ required. Current: ${BASH_VERSION}"
        exit 2
    fi

    local args=("$@")
    local positional=()
    local idx=0
    while [[ $idx -lt ${#args[@]} ]]; do
        local arg="${args[$idx]}"
        case "$arg" in
        -c | --config)
            ((idx += 1))
            config_file="${args[$idx]:-}"
            ;;
        --enable)
            ((idx += 1))
            enable_section "${args[$idx]:-}"
            ;;
        --disable)
            ((idx += 1))
            disable_section "${args[$idx]:-}"
            ;;
        --list-sections)
            list_sections
            exit 0
            ;;
        --no-color)
            USE_COLOR=0
            ;;
        --tips-only)
            TIPS_ONLY=1
            ;;
        --force-analysis)
            FORCE_ANALYSIS=1
            ;;
        --compare-mode)
            ((idx += 1))
            COMPARE_MODE="${args[$idx]:-$DEFAULT_COMPARE_MODE}"
            ;;
        --compare)
            DO_COMPARE=1
            ((idx += 1))
            COMPARE_LEFT="${args[$idx]:-}"
            if [[ -z "$COMPARE_LEFT" ]]; then
                log ERROR "Option --compare requires two paths."
                exit 1
            fi
            ((idx += 1))
            COMPARE_RIGHT="${args[$idx]:-}"
            if [[ -z "$COMPARE_RIGHT" ]]; then
                log ERROR "Option --compare requires two paths."
                exit 1
            fi
            ;;
        -h | --help)
            print_usage
            exit 0
            ;;
        --)
            ((idx += 1))
            while [[ $idx -lt ${#args[@]} ]]; do
                positional+=("${args[$idx]}")
                ((idx += 1))
            done
            break
            ;;
        -*)
            log ERROR "Unknown option: $arg"
            print_usage
            exit 1
            ;;
        *)
            positional+=("$arg")
            ;;
        esac
        ((idx += 1))
    done

    if [[ "$DO_COMPARE" -eq 0 && ${#positional[@]} -eq 0 ]]; then
        print_usage
        exit 1
    fi

    local cfg_source="$config_file"
    if [[ -z "$cfg_source" && -f "$DEFAULT_CONFIG" ]]; then
        cfg_source="$DEFAULT_CONFIG"
    fi
    load_config "$cfg_source"
    preflight_tools

    if ((DO_COMPARE)); then
        case "$COMPARE_MODE" in
        full | fast) ;;
        *)
            log WARN "Unknown comparison mode '${COMPARE_MODE}', switching to ${DEFAULT_COMPARE_MODE}."
            COMPARE_MODE="$DEFAULT_COMPARE_MODE"
            ;;
        esac
        run_compare
        exit 0
    fi

    local target
    for target in "${positional[@]}"; do
        if [[ "$TIPS_ONLY" -ne 1 ]]; then
            while IFS= read -r -d '' item; do
                analyze_target "$item"
            done < <(detect_targets "$target")
        else
            log INFO "Tips-only mode: skipping per-file analysis for $target"
        fi
    done

    emit_tips
    log INFO "Audit complete."
}

main "$@"
