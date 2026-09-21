# hang_diag - usage examples

Universal wrapper: `Scripts/hang_diag.sh`  
Debugger command files: `Scripts/GDB/hang_diag.gdb`, `Scripts/LLDB/hang_diag.lldb`

Legacy shortcuts: `Scripts/GDB/hang_diag.sh` -> `--gdb`, `Scripts/LLDB/hang_diag.sh` -> `--lldb`.

## Quick reference

| Mode                       | Command                                                                       |
| -------------------------- | ----------------------------------------------------------------------------- |
| Live attach (GDB, default) | `./Scripts/hang_diag.sh [pid] [log] [solib]`                                  |
| Live attach (LLDB)         | `./Scripts/hang_diag.sh --lldb [pid] [log] [solib]`                           |
| Core dump                  | `./Scripts/hang_diag.sh [--gdb\|--lldb] --core <binary> <core> [log] [solib]` |
| Run until crash            | `./Scripts/hang_diag.sh [--lldb] --run <binary> [log] [solib] [-- args...]`   |

Environment: `HANG_DEBUGGER=gdb|lldb`, `HANG_PID`, `HANG_VERBOSE=1`, `HANG_BT_DEPTH=20`, `HANG_SOLIB`, `USE_SUDO=1`, `HANG_PROCESS_NAME` (live auto-detect).

Logging: GDB writes to the log file internally; with `USE_SUDO=1` live attach, GDB stdout is teed to the log (internal `set logging` is disabled to avoid permission errors). LLDB always uses shell tee.

---

## PeakExpertNoGUI - live attach

### Find PID

```bash
ps aux | grep PeakExpertNoGUI
```

### Wrapper (auto-detect PID)

```bash
./Scripts/hang_diag.sh
# or explicitly GDB:
./Scripts/hang_diag.sh --gdb
```

Explicit PID:

```bash
./Scripts/hang_diag.sh 150167
```

Custom log and solib (default solib for `PeakExpertNoGUI` is `~/.peakexpertweb/PeakExpertNoGUI/`):

```bash
./Scripts/hang_diag.sh 150167 "$HOME/Downloads/my-hang.log" "$HOME/.peakexpertweb/PeakExpertNoGUI/"
```

Attach as root when needed:

```bash
USE_SUDO=1 ./Scripts/hang_diag.sh
```

Verbose log (locals, args, all thread registers):

```bash
HANG_VERBOSE=1 ./Scripts/hang_diag.sh
```

### Raw GDB (without wrapper)

```bash
mkdir -pv "$HOME/Downloads"
sudo gdb -batch -p 150167 \
  -ex 'set $hang_shell_log = 1' \
  -ex 'set $hang_solib = "$HOME/.peakexpertweb/PeakExpertNoGUI/"' \
  -x ./Scripts/GDB/hang_diag.gdb \
  2>&1 | tee "$HOME/Downloads/gdb-live-hang.log"
```

---

## PeakExpertNoGUI - core dump

Core dumps: `~/.peakexpertweb/PeakExpertNoGUI/dumps/`  
Binary and libraries: `~/.peakexpertweb/PeakExpertNoGUI/`

```bash
./Scripts/hang_diag.sh --core \
  ~/.peakexpertweb/PeakExpertNoGUI/PeakExpertNoGUI \
  ~/.peakexpertweb/PeakExpertNoGUI/dumps/core_dump_full_<timestamp>_<pid>_PeakExpertNoGUI.core
```

Custom log and solib:

```bash
./Scripts/hang_diag.sh --core \
  ~/.peakexpertweb/PeakExpertNoGUI/PeakExpertNoGUI \
  ~/.peakexpertweb/PeakExpertNoGUI/dumps/core_dump_full_1778840301_321240_PeakExpertNoGUI.core \
  ~/Downloads/my-crash.log \
  ~/.peakexpertweb/PeakExpertNoGUI/
```

Via environment variables:

```bash
HANG_BINARY=~/.peakexpertweb/PeakExpertNoGUI/PeakExpertNoGUI \
HANG_CORE=~/.peakexpertweb/PeakExpertNoGUI/dumps/core_dump_full_1778840301_321240_PeakExpertNoGUI.core \
./Scripts/hang_diag.sh --gdb
```

LLDB core analysis:

```bash
./Scripts/hang_diag.sh --lldb --core \
  ~/.peakexpertweb/PeakExpertNoGUI/PeakExpertNoGUI \
  ~/.peakexpertweb/PeakExpertNoGUI/dumps/core_dump_full_<timestamp>_<pid>_PeakExpertNoGUI.core \
  /tmp/lldb-core.log \
  ~/.peakexpertweb/PeakExpertNoGUI/
```

### Core log extras (GDB)

Compared to live attach, core mode additionally includes:

- **crash: signal table** - signal disposition (SIGSEGV, SIGFPE, SIGABRT, …)
- **crash: faulting frame** - frame, locals, arguments at crash site
- **crash: faulting thread bt full** - full backtrace of crashing thread

`info proc stat` / `info proc status` are skipped for cores (no live `/proc/<pid>/`).

---

## Trilinos - run under debugger (MSan / fast crash)

Use `--run` when the process exits too quickly for manual attach. Solib is usually unnecessary if the binary is from your build tree.

```bash
cd /path/to/Trilinos

BIN=build_teuchos/packages/teuchos/core/test/TypeConversions/TeuchosCore_TypeConversions_UnitTest.exe
LOG=/tmp/typeconv-msan.log

./Scripts/hang_diag.sh --lldb --run "$BIN" "$LOG" -- \
  --filter="*realToUnsignedIntTypeOverflow*"
```

GDB equivalent:

```bash
./Scripts/hang_diag.sh --gdb --run "$BIN" "$LOG" -- \
  --filter="*realToUnsignedIntTypeOverflow*"
```

---

## Example terminal output (core, GDB)

```log
Core dump mode (--core)
Debugger: gdb
Mode: core (hang_mode=1)
Log: /tmp/gdb-core-20260515-132900.log
Solib: /home/user/.peakexpertweb/PeakExpertNoGUI/
Binary: /home/user/.peakexpertweb/PeakExpertNoGUI/PeakExpertNoGUI
Core:   /home/user/.peakexpertweb/PeakExpertNoGUI/dumps/core_dump_full_1778840301_321240_PeakExpertNoGUI.core
...
Program terminated with signal SIGSEGV, Segmentation fault.
Log: /tmp/gdb-core-20260515-132900.log
```
