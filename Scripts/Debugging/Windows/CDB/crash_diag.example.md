# crash_diag - Universal CDB diagnosis (Windows equivalent of GDB hang_diag)

## Prerequisites

Debugging Tools for Windows must be installed (ships with WDK or Windows SDK).
CDB is expected at one of:

- `C:\Program Files (x86)\Windows Kits\10\Debuggers\x64\cdb.exe`
- `C:\Program Files\Windows Kits\10\Debuggers\x64\cdb.exe`

---

### Mode 1 - Run EXE and catch crash

```powershell
.\Scripts\CDB\crash_diag.ps1 -Exe path\to\test.exe
```

With extra PDB path and Microsoft public symbol server:

```powershell
.\Scripts\CDB\crash_diag.ps1 -Exe path\to\test.exe -SymPath D:\build\lib -SymSrv
```

Pass arguments to the EXE:

```powershell
.\Scripts\CDB\crash_diag.ps1 -Exe path\to\test.exe -ExeArgs '--test','myTest'
```

Catch the crash and save a heap dump automatically:

```powershell
.\Scripts\CDB\crash_diag.ps1 -Exe path\to\test.exe -WriteDump -DumpType heap
```

The script:

1. Starts the EXE under CDB (skips the initial loader breakpoint with `-g`).
2. Suppresses first-chance C++ exception stops (`sxd e06d7363`).
3. Stops on STATUS_BREAKPOINT (`sxe bpe`) - the crash.
4. Optionally saves a dump (mini / heap / full) at the crash point.
5. Dumps modules, threads, registers, exception record, automated crash analysis
   (`!analyze -v`), full stacks, locks, and heap summary to the log file.

---

### Mode 2 - Attach to a hanging process

Auto-detect by process name:

```powershell
.\Scripts\CDB\crash_diag.ps1 -ProcessName myapp
```

Wait up to 60 seconds for the process to appear, then auto-attach:

```powershell
.\Scripts\CDB\crash_diag.ps1 -ProcessName myapp -Timeout 60 -Force
```

Explicit PID:

```powershell
.\Scripts\CDB\crash_diag.ps1 -PidArg 5432
```

Attach to an elevated process:

```powershell
.\Scripts\CDB\crash_diag.ps1 -PidArg 5432 -Admin
```

Attach and capture a full dump before analysis:

```powershell
.\Scripts\CDB\crash_diag.ps1 -ProcessName myapp -WriteDump -DumpType full -DumpOut C:\Dumps\hang.dmp
```

---

### Mode 3 - Analyze an existing dump (.dmp / .dump / .mdmp)

Single file:

```powershell
.\Scripts\CDB\crash_diag.ps1 -Dump C:\Temp\crash.dmp
```

Newest dump in a directory:

```powershell
.\Scripts\CDB\crash_diag.ps1 -Dump C:\CrashDumps\
```

Glob pattern:

```powershell
.\Scripts\CDB\crash_diag.ps1 -Dump "C:\Temp\myapp*.dmp"
```

With the original binary (for symbol resolution) and Microsoft symbols:

```powershell
.\Scripts\CDB\crash_diag.ps1 -Dump C:\Temp\crash.dmp -DumpExe path\to\test.exe -SymSrv
```

---

### Mode 4 - Auto-find the most recent WER dump

```powershell
.\Scripts\CDB\crash_diag.ps1 -FindRecentDump
```

Filter by process name and add an extra search directory:

```powershell
.\Scripts\CDB\crash_diag.ps1 -FindRecentDump -ProcessName myapp -DumpSearchDir C:\MyDumps
```

Searches these locations by default:

- `%LOCALAPPDATA%\CrashDumps`
- `%LOCALAPPDATA%\Microsoft\Windows\WER\ReportQueue`
- `%LOCALAPPDATA%\Microsoft\Windows\WER\ReportArchive`
- `%ProgramData%\Microsoft\Windows\WER\ReportQueue`
- `%SystemRoot%\Minidump`
- `%SystemRoot%\LiveKernelReports`
- `%TEMP%`

---

### Options reference

| Flag                         | Env var                  | Default                    | Description                                     |
| ---------------------------- | ------------------------ | -------------------------- | ----------------------------------------------- |
| `-Exe path`                  | `CRASH_EXE`              | -                          | Executable to launch                            |
| `-ExeArgs array`             | -                        | -                          | Arguments forwarded to the EXE                  |
| `-PidArg pid`                | `HANG_PID` / `CRASH_PID` | -                          | PID to attach to                                |
| `-Dump path`                 | `CRASH_DUMP`             | -                          | Dump file, directory, or glob                   |
| `-DumpExe path`              | -                        | -                          | Image hint for dump analysis                    |
| `-FindRecentDump`            | -                        | off                        | Auto-find most recent dump                      |
| `-DumpSearchDir paths`       | -                        | -                          | Extra dirs for dump search (`;`-separated)      |
| `-Log path`                  | `CRASH_LOG`              | `%TEMP%\cdb-*.log`         | Log file                                        |
| `-SymPath path`              | `CRASH_SYMPATH`          | -                          | Extra PDB / symbol path                         |
| `-SymSrv`                    | -                        | off                        | Add Microsoft public symbol server              |
| `-SymCache path`             | `CRASH_SYMCACHE`         | `C:\SymCache`              | Local cache for `-SymSrv`                       |
| `-ProcessName name`          | `CRASH_PROCESS_NAME`     | -                          | Process name for auto-detect                    |
| `-Timeout N`                 | `CRASH_TIMEOUT`          | 0 (unlimited)              | Seconds to wait for process                     |
| `-Arch x64\|x86\|auto`       | `CRASH_ARCH`             | `x64`                      | Debugger bit-ness                               |
| `-Verbose`                   | `CRASH_VERBOSE=1`        | off                        | Extra locals, full stacks, all registers        |
| `-BtDepth N`                 | `CRASH_BT_DEPTH`         | 20                         | Frames in summary backtraces                    |
| `-NoAnalyze`                 | -                        | off                        | Skip `!analyze -v` (faster for known crashes)   |
| `-ExtraCmd cmds`             | -                        | -                          | Extra CDB commands before the main script       |
| `-ExtraScripts paths`        | -                        | -                          | Chain extra .cdb files in one CDB session       |
| `-WriteDump`                 | -                        | off                        | Save a dump file during analysis                |
| `-DumpType mini\|heap\|full` | -                        | `mini`                     | Dump content level                              |
| `-DumpOut path`              | -                        | `%TEMP%\cdb-capture-*.dmp` | Output dump path                                |
| `-Admin`                     | -                        | off                        | Re-launch CDB via RunAs for elevated targets    |
| `-DryRun`                    | -                        | off                        | Print CDB command but do NOT run it             |
| `-Force`                     | -                        | off                        | Skip interactive prompts, pick first/best match |
| `-CdbScript path`            | -                        | `crash_diag.cdb`           | Override the default CDB script                 |

---

### What the log contains

**Always:**

- CDB version
- Target OS / machine info (`vertarget`)
- Process info and command line
- Process times (`.time`)
- All loaded modules (base address, size, path, timestamp)
- Virtual memory summary
- Thread list
- Exception filters
- Last event
- Exception record and context
- Current thread registers
- Call stack with parameters (40 frames)
- Automated crash analysis (`!analyze -v`) - omitted with `-NoAnalyze`
- All thread stacks summary (20 frames)
- All thread full stacks (40 frames)
- Locals at faulting frame
- Critical sections / locks (`!locks`)
- Heap summary (`!heap -s`)
- All thread registers

---

### Vtable diagnostics (vtable_diag.cdb and slots_diag.cdb)

These companion scripts diagnose vtable slot mismatches - typically caused by a
MSVC/clang-cl codegen bug where a derived class in an EXE fails to override a
pure virtual from a `__declspec(dllimport)` base class in a DLL.

**vtable_diag.cdb** - dump vtable contents and slot names.

Run inside CDB after the crash stops, setting aliases first:

```powershell
as BaseModule teuchoscore
as BaseMethod Teuchos::UnitTestBase::runUnitTest
$$<D:\Develop\Trilinos\Scripts\CDB\vtable_diag.cdb
```

Or inject via crash_diag.ps1 using `-ExtraCmd`:

```powershell
.\Scripts\CDB\crash_diag.ps1 -Exe .\test.exe `
    -ExtraCmd "as BaseModule teuchoscore; as BaseMethod Teuchos::UnitTestBase::runUnitTest" `
    -CdbScript .\Scripts\CDB\vtable_diag.cdb
```

**slots_diag.cdb** - list all symbols for a class and disassemble suspect slots.

```powershell
as TargetExe   TeuchosCore_TypeConversions_UnitTest
as TargetClass `anonymous namespace'::asSafe_realToUnsignedIntTypeOverflow_UnitTest<double,unsigned int>
as SlotAddr    00007ff6466b4ba0
$$<D:\Develop\Trilinos\Scripts\CDB\slots_diag.cdb
```

### One-shot: all 3 logs from a single run (-ExtraScripts)

Chain crash_diag.cdb + vtable_diag.cdb + slots_diag.cdb in one CDB session.
Each script writes to its own log. ${SlotAddr} is automatically replaced with @rip
(the crash rip IS the purecall-thunk slot address - no need to specify it manually).

```powershell
.\Scripts\CDB\crash_diag.ps1 `
    -Exe .\btomp\packages\teuchos\core\test\TypeConversions\TeuchosCore_TypeConversions_UnitTest.exe `
    -SymPath D:\Develop\Trilinos\btomp\lib `
    -SymSrv `
    -ExtraCmd "as BaseModule teuchoscore; as BaseMethod Teuchos::UnitTestBase::runUnitTest; as TargetExe TeuchosCore_TypeConversions_UnitTest; as TargetClass ``anonymous namespace'``::asSafe_realToUnsignedIntTypeOverflow_UnitTest<double,unsigned int>" `
    -ExtraScripts @(".\Scripts\CDB\vtable_diag.cdb",".\Scripts\CDB\slots_diag.cdb") `
    -Log .\log1-crash.log
```

Produces (CDB appends pid+timestamp to each log name via `.logopen /t`):

- `log1-crash_PID_TIMESTAMP.log`           -- crash_diag.cdb output
- `log1-crash_vtable_diag_PID_TIMESTAMP.log`  -- vtable_diag.cdb output
- `log1-crash_slots_diag_PID_TIMESTAMP.log`   -- slots_diag.cdb output

---

### Quick start for the btomp TypeConversions crash

```powershell
cd D:\Develop\Trilinos
$env:PATH = "D:\Develop\Trilinos\btomp\lib;$env:PATH"

.\Scripts\CDB\crash_diag.ps1 `
    -Exe .\btomp\packages\teuchos\core\test\TypeConversions\TeuchosCore_TypeConversions_UnitTest.exe `
    -SymPath .\btomp\lib `
    -SymSrv
```

Setting `PATH` before the script ensures btomp DLLs load (not btserial ones).
