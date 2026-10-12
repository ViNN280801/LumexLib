#!/usr/bin/env bash
# Universal hang / crash snapshot wrapper (GDB or LLDB).
#
# Live attach (default):
#   ./hang_diag.sh [--gdb|--lldb] [pid] [log_file] [solib_dir]
#
# Core dump:
#   ./hang_diag.sh [--gdb|--lldb] --core <binary> <corefile> [log_file] [solib_dir]
#
# Run under debugger (stop on crash):
#   ./hang_diag.sh [--lldb] --run <binary> [log_file] [solib_dir] [-- program_args...]
#
# See Scripts/hang_diag.example.md for Trilinos MSan and PeakExpert examples.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
GDB_SCRIPT="${SCRIPT_DIR}/GDB/hang_diag.gdb"
LLDB_SCRIPT="${SCRIPT_DIR}/LLDB/hang_diag.lldb"
PROCESS_NAME="${HANG_PROCESS_NAME:-PeakExpertNoGUI}"

usage() {
  cat <<EOF >&2
Usage:
  $(basename "$0") [--gdb|--lldb] [pid] [log_file] [solib_dir]
  $(basename "$0") [--gdb|--lldb] --core <binary> <corefile> [log_file] [solib_dir]
  $(basename "$0") [--gdb|--lldb] --run <binary> [log_file] [solib_dir] [-- program_args...]
EOF
}

die() {
  echo "$*" >&2
  exit 1
}

user_home() {
  local u="${SUDO_USER:-$USER}"
  local home
  home="$(getent passwd "$u" 2>/dev/null | cut -d: -f6)"
  if [[ -n "$home" ]]; then
    echo "$home"
  else
    echo "$HOME"
  fi
}

find_matching_pids() {
  local -a pids=()
  local pid

  while IFS= read -r pid; do
    [[ -n "$pid" ]] && pids+=("$pid")
  done < <(pgrep -x "$PROCESS_NAME" 2>/dev/null || true)

  if [[ ${#pids[@]} -eq 0 ]]; then
    while IFS= read -r pid; do
      [[ -n "$pid" ]] && pids+=("$pid")
    done < <(pgrep -f "[./]${PROCESS_NAME}\$" 2>/dev/null || true)
  fi

  if [[ ${#pids[@]} -eq 0 ]]; then
    return 0
  fi

  printf '%s\n' "${pids[@]}" | sort -nu
}

print_process_line() {
  local pid="$1"
  local info
  info="$(ps -p "$pid" -o pid=,user=,etime=,args= 2>/dev/null | sed 's/^[[:space:]]*//')"
  if [[ -n "$info" ]]; then
    echo "    $info" >&2
  else
    echo "    PID $pid (details unavailable)" >&2
  fi
}

resolve_pid_auto() {
  local -a pids=()
  local pid choice i

  while IFS= read -r pid; do
    [[ -n "$pid" ]] && pids+=("$pid")
  done < <(find_matching_pids)

  if [[ ${#pids[@]} -eq 0 ]]; then
    echo "No running process matching '$PROCESS_NAME' found." >&2
    echo "Pass PID, set HANG_PID, or use --run / --core." >&2
    exit 1
  fi

  if [[ ${#pids[@]} -eq 1 ]]; then
    pid="${pids[0]}"
    echo "Found $PROCESS_NAME:" >&2
    print_process_line "$pid"
    echo "Using PID $pid" >&2
    echo "$pid"
    return 0
  fi

  echo "WARNING: Found ${#pids[@]} processes matching '$PROCESS_NAME':" >&2
  for i in "${!pids[@]}"; do
    echo "  [$((i + 1))] PID ${pids[$i]}" >&2
    print_process_line "${pids[$i]}"
  done

  if [[ ! -t 0 ]]; then
    die "Multiple processes; pass PID explicitly or set HANG_PID."
  fi

  while true; do
    read -r -p "Enter number [1-${#pids[@]}] or PID: " choice
    if [[ "$choice" =~ ^[0-9]+$ ]]; then
      if [[ "$choice" -ge 1 && "$choice" -le ${#pids[@]} ]]; then
        pid="${pids[$((choice - 1))]}"
        echo "Using PID $pid" >&2
        echo "$pid"
        return 0
      fi
      for pid in "${pids[@]}"; do
        if [[ "$choice" == "$pid" ]]; then
          echo "Using PID $pid" >&2
          echo "$pid"
          return 0
        fi
      done
    fi
    echo "Invalid choice." >&2
  done
}

# ============= Debugger flag =============

DEBUGGER="${HANG_DEBUGGER:-gdb}"
while [[ $# -gt 0 ]]; do
  case "$1" in
  --gdb)
    DEBUGGER=gdb
    shift
    ;;
  --lldb)
    DEBUGGER=lldb
    shift
    ;;
  -h | --help)
    usage
    exit 0
    ;;
  *) break ;;
  esac
done

case "$DEBUGGER" in
gdb | lldb) ;;
*) die "Invalid HANG_DEBUGGER='$DEBUGGER' (use gdb or lldb)" ;;
esac

[[ "$DEBUGGER" != gdb || -f "$GDB_SCRIPT" ]] || die "GDB script not found: $GDB_SCRIPT"
[[ "$DEBUGGER" != lldb || -f "$LLDB_SCRIPT" ]] || die "LLDB script not found: $LLDB_SCRIPT"

# ============= Mode =============

MODE=live
HANG_MODE=0
BINARY=""
CORE_FILE=""
PID=""
RUN_ARGS=()
LOG=""
SOLIB=""

if [[ -n "${HANG_BINARY:-}" && -n "${HANG_CORE:-}" ]]; then
  MODE=core
  HANG_MODE=1
  BINARY="$HANG_BINARY"
  CORE_FILE="$HANG_CORE"
  echo "Core dump mode (HANG_BINARY / HANG_CORE)" >&2
fi

if [[ $# -gt 0 && "$1" == "--core" ]]; then
  MODE=core
  HANG_MODE=1
  shift
  [[ $# -ge 2 ]] || die "$(usage)"
  BINARY="$1"
  CORE_FILE="$2"
  shift 2
  echo "Core dump mode (--core)" >&2
fi

if [[ $# -gt 0 && "$1" == "--run" ]]; then
  MODE=run
  HANG_MODE=2
  shift
  [[ $# -ge 1 ]] || die "$(usage)"
  BINARY="$1"
  shift
  echo "Run-under-debugger mode (--run)" >&2

  if [[ $# -gt 0 && "$1" != "--" ]]; then
    if [[ -d "$1" ]]; then
      SOLIB="$1"
      shift
    else
      LOG="$1"
      shift
      if [[ $# -gt 0 && "$1" != "--" && -d "$1" ]]; then
        SOLIB="$1"
        shift
      fi
    fi
  fi

  if [[ $# -gt 0 && "$1" == "--" ]]; then
    shift
  fi
  RUN_ARGS=("$@")
fi

LEGACY_DEFAULT_SOLIB="$(user_home)/.peakexpertweb/PeakExpertNoGUI/"

if [[ -z "$LOG" ]]; then
  case "$MODE" in
  live) LOG="/tmp/${DEBUGGER}-hang-$(date +%Y%m%d-%H%M%S).log" ;;
  core) LOG="/tmp/${DEBUGGER}-core-$(date +%Y%m%d-%H%M%S).log" ;;
  run) LOG="/tmp/${DEBUGGER}-run-$(date +%Y%m%d-%H%M%S).log" ;;
  esac
fi

if [[ $MODE == live ]]; then
  if [[ -n "${HANG_PID:-}" ]]; then
    PID="$HANG_PID"
    echo "Using PID $PID (HANG_PID)" >&2
  elif [[ $# -gt 0 && "$1" =~ ^[0-9]+$ ]]; then
    PID="$1"
    echo "Using PID $PID (argument)" >&2
    shift
  fi

  if [[ $# -gt 0 ]]; then
    LOG="$1"
    shift
  fi
  if [[ $# -gt 0 ]]; then
    SOLIB="$1"
    shift
  fi
elif [[ $MODE == core ]]; then
  if [[ $# -gt 0 ]]; then
    LOG="$1"
    shift
  fi
  if [[ $# -gt 0 ]]; then
    SOLIB="$1"
    shift
  fi
fi

if [[ -z "$SOLIB" ]]; then
  SOLIB="${HANG_SOLIB:-}"
fi
if [[ -z "$SOLIB" && $MODE == live && "$PROCESS_NAME" == "PeakExpertNoGUI" ]]; then
  SOLIB="$LEGACY_DEFAULT_SOLIB"
fi

HANG_VERBOSE_FLAG=0
[[ -n "${HANG_VERBOSE:-}" ]] && HANG_VERBOSE_FLAG=1
HANG_BT_DEPTH="${HANG_BT_DEPTH:-20}"

mkdir -p "$(dirname "$LOG")"

# ============= Validate =============

if [[ $MODE == core || $MODE == run ]]; then
  [[ -f "$BINARY" ]] || die "Binary not found: $BINARY"
fi
if [[ $MODE == core ]]; then
  [[ -f "$CORE_FILE" ]] || die "Core file not found: $CORE_FILE"
fi
if [[ $MODE == live ]]; then
  [[ -z "${PID:-}" ]] && PID="$(resolve_pid_auto)"
  kill -0 "$PID" 2>/dev/null || die "Process $PID not found or not accessible."
fi

export HANG_LOG="$LOG"
export HANG_SOLIB="$SOLIB"
export HANG_VERBOSE="$HANG_VERBOSE_FLAG"
export HANG_BT_DEPTH="$HANG_BT_DEPTH"
export HANG_MODE="$HANG_MODE"
export HANG_LLDB_DIR="${SCRIPT_DIR}/LLDB"

append_proc_environ() {
  local log="$1"
  local pid="$2"

  [[ -r "/proc/${pid}/environ" ]] || return 0
  if ! { echo "" >>"$log"; } 2>/dev/null; then
    echo "WARNING: cannot append proc environ to log (permission denied): $log" >&2
    return 1
  fi

  {
    echo "========================================================================"
    echo " proc environ (from /proc/${pid}/environ)"
    echo "========================================================================"
    tr '\0' '\n' <"/proc/${pid}/environ"
  } >>"$log"
}

publish_gdb_log() {
  local src="$1"
  local dest="$2"

  [[ -f "$src" ]] || die "GDB temp log missing: $src"
  cat "$src" >"$dest"
  rm -f "$src"
}

make_gdb_ex() {
  local gdb_log="$1"
  local shell_log="${2:-0}"
  gdb_ex=(
    -ex "set \$hang_shell_log = ${shell_log}"
    -ex "set \$hang_log = \"${gdb_log}\""
    -ex "set \$hang_solib = \"${SOLIB}\""
    -ex "set \$hang_verbose = ${HANG_VERBOSE_FLAG}"
    -ex "set \$hang_bt_depth = ${HANG_BT_DEPTH}"
    -ex "set \$hang_mode = ${HANG_MODE}"
  )
}

finalize_log() {
  local log="$1"
  local err_sidecar="${log%.*}.inferior.err"
  {
    echo ""
    echo "========================================================================"
    echo " sanitizer excerpt (auto-extracted)"
    echo "========================================================================"
    {
      grep -E 'MemorySanitizer|AddressSanitizer|UndefinedBehaviorSanitizer|ThreadSanitizer|SUMMARY:|ERROR:|DEADLYSIGNAL' "$log" 2>/dev/null || true
      if [[ -f "$err_sidecar" ]]; then
        grep -E 'MemorySanitizer|AddressSanitizer|UndefinedBehaviorSanitizer|ThreadSanitizer|SUMMARY:|ERROR:|DEADLYSIGNAL' "$err_sidecar" 2>/dev/null || true
      fi
    } | head -100
    if ! grep -qE 'MemorySanitizer|AddressSanitizer|UndefinedBehaviorSanitizer|ThreadSanitizer|ERROR:' "$log" "$err_sidecar" 2>/dev/null; then
      echo "(no sanitizer lines matched in log or ${err_sidecar})"
    fi
    echo ""
    echo "========================================================================"
    echo " end of hang_diag log — $(date -u +%Y-%m-%dT%H:%M:%SZ)"
    echo "========================================================================"
  } >>"$log"
}

echo "Debugger: $DEBUGGER" >&2
echo "Mode: $MODE (hang_mode=$HANG_MODE)" >&2
echo "Log: $LOG" >&2
echo "Solib: ${SOLIB:-"(none)"}" >&2
echo "Verbose: $HANG_VERBOSE_FLAG, bt depth: $HANG_BT_DEPTH" >&2

run_gdb() {
  local gdb_log="$LOG"
  local gdb_log_temp=""
  local -a gdb_ex=()

  echo "GDB script: $GDB_SCRIPT" >&2

  if [[ $MODE == core ]]; then
    make_gdb_ex "$gdb_log"
    echo "Binary: $BINARY" >&2
    echo "Core:   $CORE_FILE" >&2
    gdb -batch "$BINARY" "$CORE_FILE" "${gdb_ex[@]}" -x "$GDB_SCRIPT"
  elif [[ $MODE == run ]]; then
    make_gdb_ex "$gdb_log"
    echo "Binary: $BINARY" >&2
    if [[ ${#RUN_ARGS[@]} -gt 0 ]]; then
      echo "Args:   ${RUN_ARGS[*]}" >&2
      gdb -batch "$BINARY" "${gdb_ex[@]}" -ex "set args ${RUN_ARGS[*]}" -x "$GDB_SCRIPT"
    else
      gdb -batch "$BINARY" "${gdb_ex[@]}" -x "$GDB_SCRIPT"
    fi
  else
    echo "Attaching gdb to PID $PID ..." >&2
    local proc_owner use_sudo=0
    proc_owner="$(stat -c '%U' "/proc/${PID}" 2>/dev/null || echo "")"
    if [[ -n "${USE_SUDO:-}" ]] || [[ "$EUID" -ne 0 && -n "$proc_owner" && "$proc_owner" != "$(id -un)" ]]; then
      use_sudo=1
      echo "Using sudo attach; GDB output via tee -> $LOG" >&2
    fi
    if [[ $use_sudo -eq 1 ]]; then
      : >"$LOG" 2>/dev/null || true
      make_gdb_ex "$LOG" 1
      sudo gdb -batch -p "$PID" "${gdb_ex[@]}" -x "$GDB_SCRIPT" 2>&1 | tee -a "$LOG"
    else
      make_gdb_ex "$LOG" 0
      gdb -batch -p "$PID" "${gdb_ex[@]}" -x "$GDB_SCRIPT"
    fi
    append_proc_environ "$LOG" "$PID"
  fi
  finalize_log "$LOG"
}

run_lldb() {
  echo "LLDB script: $LLDB_SCRIPT" >&2
  local -a lldb_cmd=(lldb -b -s "$LLDB_SCRIPT")

  if [[ $MODE == core ]]; then
    echo "Binary: $BINARY" >&2
    echo "Core:   $CORE_FILE" >&2
    lldb_cmd+=(-c "$CORE_FILE" -- "$BINARY")
  elif [[ $MODE == run ]]; then
    echo "Binary: $BINARY" >&2
    lldb_cmd+=(-- "$BINARY")
    if [[ ${#RUN_ARGS[@]} -gt 0 ]]; then
      echo "Args:   ${RUN_ARGS[*]}" >&2
      lldb_cmd+=("${RUN_ARGS[@]}")
    fi
  else
    echo "Attaching lldb to PID $PID ..." >&2
    local proc_owner
    proc_owner="$(stat -c '%U' "/proc/${PID}" 2>/dev/null || echo "")"
    if [[ -n "${USE_SUDO:-}" ]] || [[ "$EUID" -ne 0 && -n "$proc_owner" && "$proc_owner" != "$(id -un)" ]]; then
      sudo env HANG_LLDB_DIR="$HANG_LLDB_DIR" HANG_LOG="$HANG_LOG" HANG_SOLIB="$HANG_SOLIB" \
        HANG_VERBOSE="$HANG_VERBOSE" HANG_BT_DEPTH="$HANG_BT_DEPTH" HANG_MODE="$HANG_MODE" \
        lldb -b -p "$PID" -s "$LLDB_SCRIPT" 2>&1 | tee "$LOG"
      finalize_log "$LOG"
      return
    fi
    lldb_cmd+=(-p "$PID")
  fi

  "${lldb_cmd[@]}" 2>&1 | tee "$LOG"
  finalize_log "$LOG"
}

if [[ $DEBUGGER == gdb ]]; then
  run_gdb
else
  run_lldb
fi

echo "Log: ${LOG}"
