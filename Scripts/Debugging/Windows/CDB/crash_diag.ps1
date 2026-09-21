#requires -Version 5.1
<#
.SYNOPSIS
Universal CDB crash / hang / dump diagnosis for Windows.
Drop-in equivalent of hang_diag.sh + hang_diag.gdb from the GDB world.

.DESCRIPTION
MODES

  Run EXE and catch first STATUS_BREAKPOINT crash:
      .\crash_diag.ps1 -Exe path\to\test.exe [-ExeArgs arg1,arg2]
      $env:CRASH_EXE = "path\to\exe"; .\crash_diag.ps1

  Attach to a hanging / live process:
      .\crash_diag.ps1 -PidArg 1234
      .\crash_diag.ps1 -ProcessName myapp    # auto-detect by name
      $env:HANG_PID = "1234"; .\crash_diag.ps1

  Analyze an existing dump (.dmp / .dump / .mdmp):
      .\crash_diag.ps1 -Dump path\to\crash.dmp [-DumpExe path\to\exe]
      .\crash_diag.ps1 -Dump C:\CrashDumps\  # pick newest in directory
      $env:CRASH_DUMP = "path\to\crash.dmp"; .\crash_diag.ps1

  Auto-find the most recent WER / CrashDumps file:
      .\crash_diag.ps1 -FindRecentDump
      .\crash_diag.ps1 -FindRecentDump -ProcessName myapp

ENVIRONMENT VARIABLE EQUIVALENTS
  CRASH_EXE, HANG_PID, CRASH_PID, CRASH_DUMP
  CRASH_LOG, CRASH_SYMPATH, CRASH_SYMCACHE
  CRASH_PROCESS_NAME, CRASH_ARCH, CRASH_VERBOSE=1
  CRASH_BT_DEPTH, CRASH_TIMEOUT

EXAMPLES
  # Catch crash and save a heap dump when it fires
  .\crash_diag.ps1 -Exe .\mytest.exe -WriteDump -DumpType heap

  # Analyze the most recent WER dump with Microsoft public symbols
  .\crash_diag.ps1 -FindRecentDump -SymSrv

  # Attach to a hanging process, time out after 30 seconds if not found
  .\crash_diag.ps1 -ProcessName myapp -Timeout 30

  # Dry-run: print the exact CDB command that would be executed
  .\crash_diag.ps1 -Exe .\mytest.exe -SymSrv -DryRun
#>

param(
    # target
    [string]    $Exe             = "",       # path to executable to launch
    [string[]]  $ExeArgs         = @(),      # arguments forwarded to the EXE
    [string]    $PidArg          = "",       # attach to this PID (string for env-var passthrough)
    [string]    $Dump            = "",       # dump file / directory / glob to analyse
    [string]    $DumpExe         = "",       # image hint for dump analysis (-i flag)

    # output
    [string]    $Log             = "",       # log file path (auto-named under %TEMP% if omitted)

    # symbols
    [string]    $SymPath         = "",       # extra symbol path (appended)
    [switch]    $SymSrv,                     # add Microsoft public symbol server
    [string]    $SymCache        = "C:\SymCache", # local cache dir used with -SymSrv

    # process selection
    [string]    $ProcessName     = "",       # name for auto-detect / WER dump filter
    [int]       $Timeout         = 0,        # seconds to wait for process; 0 = no limit

    # architecture
    [ValidateSet("x64","x86","auto")]
    [string]    $Arch            = "x64",    # debugger bit-ness (match target)

    # analysis
    [switch]    $Verbose,                    # extra locals, full stacks, all registers
    [int]       $BtDepth         = 0,        # frames in summary stacks (default 20)
    [switch]    $NoAnalyze,                  # skip !analyze -v (faster for known crashes)
    [string]    $ExtraCmd        = "",       # extra CDB commands appended after the script

    # dump generation
    [switch]    $WriteDump,                  # capture a dump file during analysis
    [ValidateSet("mini","heap","full")]
    [string]    $DumpType        = "mini",   # mini=/m  heap=/mh  full=/f
    [string]    $DumpOut         = "",       # explicit output path (auto-named if omitted)

    # dump search
    [switch]    $FindRecentDump,             # auto-detect most recent .dmp file
    [string]    $DumpSearchDir   = "",       # extra dir(s) to search, semicolon-separated

    # misc
    [switch]    $Admin,                      # re-launch CDB elevated on attach failure
    [switch]    $DryRun,                     # print CDB command but do NOT run it
    [switch]    $Force,                      # skip interactive prompts (pick first/best match)
    [string]    $CdbScript       = "",       # override the default crash_diag.cdb path
    [string[]]  $ExtraScripts    = @()      # additional .cdb scripts chained after the main one
)

Set-StrictMode -Version 2
$ErrorActionPreference = "Stop"

# resolve parameters from environment variables (same pattern as hang_diag.sh)
if (-not $Exe          -and $env:CRASH_EXE)           { $Exe          = $env:CRASH_EXE }
if (-not $PidArg       -and $env:HANG_PID)             { $PidArg       = $env:HANG_PID }
if (-not $PidArg       -and $env:CRASH_PID)            { $PidArg       = $env:CRASH_PID }
if (-not $Dump         -and $env:CRASH_DUMP)           { $Dump         = $env:CRASH_DUMP }
if (-not $Log          -and $env:CRASH_LOG)            { $Log          = $env:CRASH_LOG }
if (-not $SymPath      -and $env:CRASH_SYMPATH)        { $SymPath      = $env:CRASH_SYMPATH }
if ($SymCache -eq "C:\SymCache" -and $env:CRASH_SYMCACHE) { $SymCache  = $env:CRASH_SYMCACHE }
if (-not $ProcessName  -and $env:CRASH_PROCESS_NAME)   { $ProcessName  = $env:CRASH_PROCESS_NAME }
if (-not $Verbose      -and $env:CRASH_VERBOSE -eq "1"){ $Verbose      = $true }
if ($BtDepth -le 0     -and $env:CRASH_BT_DEPTH)      { $BtDepth      = [int]$env:CRASH_BT_DEPTH }
if ($Timeout -le 0     -and $env:CRASH_TIMEOUT)        { $Timeout      = [int]$env:CRASH_TIMEOUT }
if ($Arch -eq "x64"    -and $env:CRASH_ARCH)           { $Arch         = $env:CRASH_ARCH }
if ($BtDepth -le 0) { $BtDepth = 20 }

# ---------------------------------------------------------------------------
# helpers
# ---------------------------------------------------------------------------

function Find-Cdb {
    param([string]$TargetArch = "x64")
    $candidates = @(
        "$env:ProgramFiles\Windows Kits\10\Debuggers\$TargetArch\cdb.exe",
        "${env:ProgramFiles(x86)}\Windows Kits\10\Debuggers\$TargetArch\cdb.exe"
    )
    foreach ($c in $candidates) {
        if (Test-Path $c -ErrorAction SilentlyContinue) { return $c }
    }
    $from_path = Get-Command "cdb.exe" -ErrorAction SilentlyContinue
    if ($from_path) { return $from_path.Source }
    throw "cdb.exe not found for arch='$TargetArch'. Install 'Debugging Tools for Windows' (WDK or Windows SDK)."
}

function Find-MatchingPids {
    param([string]$Name)
    $bare  = $Name -replace '\.exe$', ''
    $procs = Get-Process -Name $bare -ErrorAction SilentlyContinue
    if (-not $procs) {
        $procs = Get-Process -ErrorAction SilentlyContinue |
                 Where-Object { $_.Path -and ($_.Path -like "*$Name*") }
    }
    if (-not $procs) { return @() }
    return @($procs | Select-Object -ExpandProperty Id | Sort-Object -Unique)
}

function Write-ProcessLine {
    param([int]$Pid_)
    $p = Get-Process -Id $Pid_ -ErrorAction SilentlyContinue
    if ($p) {
        $path = if ($p.Path) { $p.Path } else { $p.Name }
        Write-Host "    PID $Pid_  $path" -ForegroundColor Cyan
    } else {
        Write-Host "    PID $Pid_ (details unavailable)" -ForegroundColor Cyan
    }
}

function Resolve-PidAuto {
    param([string]$Name, [int]$TimeoutSec, [switch]$AutoSelect)
    if (-not $Name) {
        Write-Host "No process name given. Use -PidArg <pid>, -ProcessName <name>, or CRASH_PROCESS_NAME." -ForegroundColor Red
        exit 1
    }
    $deadline = if ($TimeoutSec -gt 0) { [DateTime]::Now.AddSeconds($TimeoutSec) } else { $null }
    $pids_ = @()
    while (@($pids_).Count -eq 0) {
        # Normalize to array because a single PID may come back as scalar.
        $pids_ = @(Find-MatchingPids $Name)
        if (@($pids_).Count -gt 0) { break }
        if ($deadline -and [DateTime]::Now -gt $deadline) {
            Write-Host "Timeout: no process matching '$Name' found within ${TimeoutSec}s." -ForegroundColor Red
            exit 1
        }
        Write-Host "Waiting for process '$Name' ..." -ForegroundColor DarkGray
        Start-Sleep -Seconds 2
    }
    if (@($pids_).Count -eq 1 -or $AutoSelect) {
        Write-Host "Found: $Name" -ForegroundColor Cyan
        Write-ProcessLine $pids_[0]
        return $pids_[0]
    }
    Write-Warning "Multiple processes match '$Name' ($(@($pids_).Count) total):"
    for ($i = 0; $i -lt @($pids_).Count; $i++) {
        Write-Host "  [$($i+1)]"; Write-ProcessLine $pids_[$i]
    }
    if ([Console]::IsInputRedirected) {
        Write-Host "Multiple matches; pass -PidArg explicitly or use -Force." -ForegroundColor Red
        exit 1
    }
    do {
        $choice = Read-Host "Enter number [1-$(@($pids_).Count)] or raw PID"
        $n = $choice -as [int]
        if ($n -ge 1 -and $n -le @($pids_).Count) { return $pids_[$n - 1] }
        if ($pids_ -contains $n)               { return $n }
        Write-Host "Invalid choice." -ForegroundColor Red
    } while ($true)
}

function Find-RecentDumps {
    param([string]$FilterName, [string]$ExtraDir)
    $dirs = [System.Collections.Generic.List[string]]::new()
    # Standard Windows crash dump locations
    $dirs.Add([Environment]::GetFolderPath('LocalApplicationData') + '\CrashDumps')
    $dirs.Add("$env:LOCALAPPDATA\Microsoft\Windows\WER\ReportQueue")
    $dirs.Add("$env:LOCALAPPDATA\Microsoft\Windows\WER\ReportArchive")
    $dirs.Add([Environment]::GetFolderPath('CommonApplicationData') + '\Microsoft\Windows\WER\ReportQueue')
    $dirs.Add("$env:SystemRoot\Minidump")
    $dirs.Add("$env:SystemRoot\LiveKernelReports")
    $dirs.Add([IO.Path]::GetTempPath())
    if ($ExtraDir) {
        foreach ($d in $ExtraDir.Split(';')) {
            $d = $d.Trim(); if ($d) { $dirs.Add($d) }
        }
    }
    $dumps = @()
    foreach ($dir in $dirs) {
        if (-not (Test-Path $dir -ErrorAction SilentlyContinue)) { continue }
        $found = Get-ChildItem -Path $dir -Recurse `
                               -Include "*.dmp","*.mdmp","*.dump" `
                               -ErrorAction SilentlyContinue
        if ($FilterName) {
            $bare  = $FilterName -replace '\.exe$', ''
            $found = $found | Where-Object { $_.Name -like "*$bare*" }
        }
        $dumps += $found
    }
    return @($dumps | Sort-Object LastWriteTime -Descending)
}

function Resolve-DumpPath {
    param([string]$DumpArg)
    # directory -> pick newest dump inside
    if (Test-Path $DumpArg -PathType Container -ErrorAction SilentlyContinue) {
        $files = @(Get-ChildItem -Path $DumpArg -Recurse `
                                 -Include "*.dmp","*.mdmp","*.dump" `
                                 -ErrorAction SilentlyContinue |
                   Sort-Object LastWriteTime -Descending)
        if ($files.Count -eq 0) {
            Write-Host "No .dmp/.mdmp/.dump files found in: $DumpArg" -ForegroundColor Red; exit 1
        }
        Write-Host "Found $($files.Count) dump(s) in directory, using newest:" -ForegroundColor Yellow
        Write-Host "  $($files[0].FullName)  [$($files[0].LastWriteTime)]" -ForegroundColor Cyan
        return $files[0].FullName
    }
    # wildcard / glob
    if ($DumpArg -match '[*?]') {
        $files = @(Resolve-Path $DumpArg -ErrorAction SilentlyContinue |
                   ForEach-Object { Get-Item $_.Path } |
                   Sort-Object LastWriteTime -Descending)
        if ($files.Count -eq 0) {
            Write-Host "No files matched pattern: $DumpArg" -ForegroundColor Red; exit 1
        }
        if ($files.Count -gt 1) {
            Write-Host "Pattern matched $($files.Count) files, using newest:" -ForegroundColor Yellow
            Write-Host "  $($files[0].FullName)" -ForegroundColor Cyan
        }
        return $files[0].FullName
    }
    # plain path
    if (-not (Test-Path $DumpArg -ErrorAction SilentlyContinue)) {
        Write-Host "Dump file not found: $DumpArg" -ForegroundColor Red; exit 1
    }
    return (Resolve-Path $DumpArg).Path
}

function Build-SymPath {
    param([string]$UserPath, [switch]$UseSymSrv, [string]$CacheDir)
    $parts = [System.Collections.Generic.List[string]]::new()
    $parts.Add(".")
    if ($UserPath) { $parts.Add($UserPath) }
    if ($UseSymSrv) {
        if ($CacheDir -and -not (Test-Path $CacheDir -ErrorAction SilentlyContinue)) {
            New-Item -ItemType Directory -Force -Path $CacheDir | Out-Null
        }
        $srv = if ($CacheDir) {
            "srv*$CacheDir*https://msdl.microsoft.com/download/symbols"
        } else {
            "srv*https://msdl.microsoft.com/download/symbols"
        }
        $parts.Add($srv)
    }
    return $parts -join ";"
}

function Get-DumpFlag {
    param([string]$Type)
    switch ($Type) {
        "full"  { return "/f"  }
        "heap"  { return "/mh" }
        default { return "/m"  }
    }
}

function Invoke-Cdb {
    param([string]$CdbPath, [string[]]$CdbArgs, [switch]$IsDryRun)
    $display = "`"$CdbPath`" " + (
        $CdbArgs | ForEach-Object { if ($_ -match '\s') { "`"$_`"" } else { $_ } }
    ) -join " "
    Write-Host "Command:" -ForegroundColor DarkGray
    Write-Host "  $display" -ForegroundColor DarkGray
    if ($IsDryRun) {
        Write-Host "[DryRun] Command printed but NOT executed." -ForegroundColor Yellow
        return
    }
    & $CdbPath @CdbArgs
}

<#
.SYNOPSIS
Resolves the on-disk path after CDB ".logopen /t" (appends _<pid>_<timestamp> before .log).
#>
function Resolve-LogopenTPath {
    param(
        [string]$RequestedPath,
        [switch]$IsDryRun
    )
    if ($IsDryRun -or [string]::IsNullOrWhiteSpace($RequestedPath)) {
        return $RequestedPath
    }
    $logDir = [IO.Path]::GetDirectoryName($RequestedPath)
    if ([string]::IsNullOrWhiteSpace($logDir)) {
        $logDir = (Get-Location).Path
    }
    $stem = [IO.Path]::GetFileNameWithoutExtension($RequestedPath)
    $pattern = Join-Path $logDir ($stem + "*.log")
    $found = @(Get-ChildItem -Path $pattern -File -ErrorAction SilentlyContinue |
              Sort-Object LastWriteTime -Descending)
    if ($found.Count -gt 0) {
        return $found[0].FullName
    }
    return $RequestedPath
}

# ---------------------------------------------------------------------------
# helpers for combined multi-script mode
# ---------------------------------------------------------------------------

function Get-ScriptBody {
    param([string]$Path)
    $lines = @(Get-Content $Path)
    $i = $lines.Count - 1
    while ($i -ge 0) {
        $t = $lines[$i].Trim()
        if ($t -eq '' -or $t -eq 'q' -or $t -eq '.logclose') { $i-- }
        else { break }
    }
    if ($i -lt 0) { return @() }
    return @($lines[0..$i])
}

function Build-CombinedScript {
    param(
        [string]   $MainScript,
        [string[]] $Extras,
        [string[]] $ExtraLogs,
        [string]   $Preamble    # semicolon-separated CDB commands; inserted as individual lines before the main script body (backticks are literal in .cdb files, unlike in -c strings)
    )
    $combined = [System.Collections.Generic.List[string]]::new()
    # Preamble aliases go first, split on ; so each becomes its own CDB command line
    if ($Preamble) {
        foreach ($cmd in ($Preamble -split ';')) {
            $c = $cmd.Trim()
            if ($c) { $combined.Add($c) }
        }
    }
    # Main script body (init command already opened the first log via .logopen)
    $combined.AddRange([string[]](Get-ScriptBody $MainScript))
    $combined.Add('.logclose')
    # Extra scripts, each with its own log section
    for ($i = 0; $i -lt $Extras.Count; $i++) {
        $combined.Add('.logopen /t "' + $ExtraLogs[$i] + '"')
        # Replace ${SlotAddr} with @rip: in a combined session the crash rip IS the slot address
        $body = @(@(Get-ScriptBody $Extras[$i]) |
                  ForEach-Object { $_ -replace '\$\{SlotAddr\}', '@rip' })
        $combined.AddRange([string[]]$body)
        $combined.Add('.logclose')
    }
    $combined.Add('q')
    return $combined
}

# ---------------------------------------------------------------------------
# resolve architecture and CDB path
# ---------------------------------------------------------------------------

if ($Arch -eq "auto") {
    $Arch = if ([Environment]::Is64BitProcess) { "x64" } else { "x86" }
}
$CDB_PATH = Find-Cdb -TargetArch $Arch

# ---------------------------------------------------------------------------
# resolve CDB script path; handle -NoAnalyze by filtering into a temp file
# ---------------------------------------------------------------------------

$SCRIPT_DIR = $PSScriptRoot
if (-not $CdbScript) { $CdbScript = Join-Path $SCRIPT_DIR "crash_diag.cdb" }
if (-not (Test-Path $CdbScript)) {
    Write-Host "CDB script not found: $CdbScript" -ForegroundColor Red; exit 1
}

$_active_script    = $CdbScript
$_temps_to_cleanup = [System.Collections.Generic.List[string]]::new()
$_extra_logs       = [System.Collections.Generic.List[string]]::new()

if ($NoAnalyze) {
    $filtered = Get-Content $CdbScript | Where-Object { $_ -notmatch '^\s*!analyze' }
    $tmp_noa  = [IO.Path]::GetTempFileName()
    $filtered | Set-Content $tmp_noa -Encoding ASCII
    $_active_script = $tmp_noa
    $_temps_to_cleanup.Add($tmp_noa)
}
# $load_script is resolved below after log path and extra-script handling

# ---------------------------------------------------------------------------
# FindRecentDump: resolve dump path before mode detection
# ---------------------------------------------------------------------------

if ($FindRecentDump -and -not $Dump) {
    $recent = Find-RecentDumps -FilterName $ProcessName -ExtraDir $DumpSearchDir
    if ($recent.Count -eq 0) {
        Write-Host "No .dmp/.mdmp/.dump files found in known WER / CrashDumps locations." -ForegroundColor Red
        Write-Host "Tip: -DumpSearchDir <path>  or  -Dump <explicit_path>" -ForegroundColor Yellow
        exit 1
    }
    $top = [Math]::Min($recent.Count, 8)
    Write-Host "Recent dumps ($($recent.Count) total, top $top):" -ForegroundColor Cyan
    for ($i = 0; $i -lt $top; $i++) {
        $r = $recent[$i]
        Write-Host ("  [{0}] {1}  [{2}]" -f ($i + 1), $r.FullName, $r.LastWriteTime) -ForegroundColor Cyan
    }
    if ($Force -or [Console]::IsInputRedirected -or $recent.Count -eq 1) {
        $Dump = $recent[0].FullName
    } else {
        $raw = Read-Host "Select [1-$top] or Enter for #1"
        $n   = if ($raw) { $raw -as [int] } else { 1 }
        $idx = if ($n -ge 1 -and $n -le $recent.Count) { $n - 1 } else { 0 }
        $Dump = $recent[$idx].FullName
    }
    Write-Host "Using: $Dump" -ForegroundColor Yellow
}

# ---------------------------------------------------------------------------
# mode detection
# ---------------------------------------------------------------------------

$mode = "auto"    # auto = attach by process name
if ($Dump)        { $mode = "dump" }
elseif ($PidArg)  { $mode = "pid"  }
elseif ($Exe)     { $mode = "exe"  }

# ---------------------------------------------------------------------------
# log file
# ---------------------------------------------------------------------------

$timestamp = Get-Date -Format "yyyyMMdd-HHmmss"
if (-not $Log) {
    $prefix = switch ($mode) {
        "dump"  { "cdb-dump"   }
        "pid"   { "cdb-attach" }
        "exe"   { "cdb-crash"  }
        default { "cdb-attach" }
    }
    $Log = [IO.Path]::Combine([IO.Path]::GetTempPath(), "$prefix-$timestamp.log")
}
$log_dir = [IO.Path]::GetDirectoryName($Log)
if ($log_dir -and -not (Test-Path $log_dir -ErrorAction SilentlyContinue)) {
    New-Item -ItemType Directory -Force -Path $log_dir | Out-Null
}

# ---------------------------------------------------------------------------
# combined multi-script mode: chain extra .cdb files in one CDB session
# ---------------------------------------------------------------------------

if ($ExtraScripts.Count -gt 0) {
    $log_base = [IO.Path]::GetFileNameWithoutExtension($Log)
    $log_dir2 = [IO.Path]::GetDirectoryName($Log)
    $resolved_extras = [System.Collections.Generic.List[string]]::new()
    foreach ($es in $ExtraScripts) {
        $es_abs = if ([IO.Path]::IsPathRooted($es)) { $es } else { Join-Path (Get-Location) $es }
        if (-not (Test-Path $es_abs)) {
            Write-Host "Extra script not found: $es_abs" -ForegroundColor Red; exit 1
        }
        $resolved_extras.Add($es_abs)
        $es_stem   = [IO.Path]::GetFileNameWithoutExtension($es_abs)
        $extra_log = if ($log_dir2) { [IO.Path]::Combine($log_dir2, "${log_base}_${es_stem}.log") } `
                     else           { "${log_base}_${es_stem}.log" }
        $_extra_logs.Add($extra_log)
    }
    $combined_path = [IO.Path]::Combine([IO.Path]::GetTempPath(), "cdb_combined_$timestamp.cdb")
    Build-CombinedScript -MainScript $_active_script `
                         -Extras     ([string[]]$resolved_extras) `
                         -ExtraLogs  ([string[]]$_extra_logs) `
                         -Preamble   $ExtraCmd |
        Set-Content $combined_path -Encoding ASCII
    $_active_script = $combined_path
    $_temps_to_cleanup.Add($combined_path)
}
$load_script = '$$<' + $_active_script

# ---------------------------------------------------------------------------
# dump output path (auto-named when -WriteDump is set without -DumpOut)
# ---------------------------------------------------------------------------

if ($WriteDump -and -not $DumpOut) {
    $DumpOut = [IO.Path]::Combine([IO.Path]::GetTempPath(), "cdb-capture-$timestamp.dmp")
}

# ---------------------------------------------------------------------------
# symbol path
# ---------------------------------------------------------------------------

$full_sym_path = Build-SymPath -UserPath $SymPath -UseSymSrv:$SymSrv -CacheDir $SymCache
$has_custom_sym = ($full_sym_path -ne ".")
# -y is a separate CDB flag for symbol path - avoids the ; ambiguity in -c strings
$sym_args = if ($has_custom_sym) { [string[]]@("-y", $full_sym_path) } else { [string[]]@() }

# ---------------------------------------------------------------------------
# init command builder
# ---------------------------------------------------------------------------

function New-InitCmd {
    return '.logopen /t "' + $Log + '"'
}

# ---------------------------------------------------------------------------
# print summary
# ---------------------------------------------------------------------------

Write-Host ""
Write-Host "crash_diag.ps1 -- CDB diagnosis wrapper" -ForegroundColor Cyan
Write-Host "  CDB        : $CDB_PATH"
Write-Host "  Script     : $CdbScript"
Write-Host "  Log (base) : $Log  (.logopen /t adds _pid_timestamp)"
Write-Host "  Mode       : $mode  |  Arch: $Arch"
if ($has_custom_sym) { Write-Host "  SymPath    : $full_sym_path" }
if ($NoAnalyze)      { Write-Host "  NoAnalyze  : on (filtered temp script)" -ForegroundColor DarkYellow }
if ($WriteDump)      { Write-Host "  WriteDump  : $DumpType -> $DumpOut" }
if ($DryRun)         { Write-Host "  DryRun     : on -- command printed but NOT executed" -ForegroundColor Yellow }
for ($i = 0; $i -lt $_extra_logs.Count; $i++) {
    Write-Host ("  ExtraScript: {0}  ->  {1}" -f $ExtraScripts[$i], $_extra_logs[$i])
}
Write-Host ""

# ---------------------------------------------------------------------------
# run CDB
# ---------------------------------------------------------------------------

switch ($mode) {

    "dump" {
        $resolved = Resolve-DumpPath $Dump
        Write-Host "Analysing dump: $resolved" -ForegroundColor Yellow
        $init = New-InitCmd
        $cmd_parts = [System.Collections.Generic.List[string]]::new()
        $cmd_parts.Add($init)
        if ($ExtraCmd -and $ExtraScripts.Count -eq 0) { $cmd_parts.Add($ExtraCmd) }
        $cmd_parts.Add($load_script)
        $final_cmd = $cmd_parts -join "; "

        $cdb_args = [System.Collections.Generic.List[string]]::new()
        $cdb_args.Add("-z"); $cdb_args.Add($resolved)
        if ($DumpExe) { $cdb_args.Add("-i"); $cdb_args.Add($DumpExe) }
        foreach ($a in $sym_args) { $cdb_args.Add($a) }
        $cdb_args.Add("-c"); $cdb_args.Add($final_cmd)
        Invoke-Cdb -CdbPath $CDB_PATH -CdbArgs $cdb_args -IsDryRun:$DryRun
    }

    "pid" {
        Write-Host "Attaching to PID: $PidArg" -ForegroundColor Yellow
        $init = New-InitCmd
        $cmd_parts = [System.Collections.Generic.List[string]]::new()
        $cmd_parts.Add($init)
        if ($WriteDump) { $cmd_parts.Add(".dump $(Get-DumpFlag $DumpType) /u `"$DumpOut`"") }
        if ($ExtraCmd -and $ExtraScripts.Count -eq 0) { $cmd_parts.Add($ExtraCmd) }
        $cmd_parts.Add($load_script)
        $final_cmd = $cmd_parts -join "; "

        $cdb_args = [System.Collections.Generic.List[string]]::new()
        $cdb_args.Add("-p"); $cdb_args.Add($PidArg)
        foreach ($a in $sym_args) { $cdb_args.Add($a) }
        $cdb_args.Add("-c"); $cdb_args.Add($final_cmd)
        if ($Admin) {
            Start-Process $CDB_PATH -ArgumentList $cdb_args -Verb RunAs -Wait
        } else {
            Invoke-Cdb -CdbPath $CDB_PATH -CdbArgs $cdb_args -IsDryRun:$DryRun
        }
    }

    "exe" {
        if (-not (Test-Path $Exe -ErrorAction SilentlyContinue)) {
            Write-Host "Executable not found: $Exe" -ForegroundColor Red; exit 1
        }
        Write-Host "Launching: $Exe" -ForegroundColor Yellow
        $init = New-InitCmd
        # sxd e06d7363 : ignore first-chance C++ exceptions (handled by the runtime)
        # sxe bpe      : stop on STATUS_BREAKPOINT (int3 / pure-virtual trap / __debugbreak)
        $exe_init = "$init; sxd e06d7363; sxe bpe"
        $cmd_parts = [System.Collections.Generic.List[string]]::new()
        $cmd_parts.Add($exe_init)
        $cmd_parts.Add("g")
        if ($WriteDump) { $cmd_parts.Add(".dump $(Get-DumpFlag $DumpType) /u `"$DumpOut`"") }
        if ($ExtraCmd -and $ExtraScripts.Count -eq 0) { $cmd_parts.Add($ExtraCmd) }
        $cmd_parts.Add($load_script)
        $final_cmd = $cmd_parts -join "; "

        $cdb_args = [System.Collections.Generic.List[string]]::new()
        $cdb_args.Add("-g")
        foreach ($a in $sym_args) { $cdb_args.Add($a) }
        $cdb_args.Add("-c"); $cdb_args.Add($final_cmd)
        $cdb_args.Add($Exe)
        foreach ($a in $ExeArgs) { $cdb_args.Add($a) }
        Invoke-Cdb -CdbPath $CDB_PATH -CdbArgs $cdb_args -IsDryRun:$DryRun
    }

    default {
        $found_pid = Resolve-PidAuto -Name $ProcessName -TimeoutSec $Timeout -AutoSelect:$Force
        Write-Host "Attaching to PID: $found_pid ($ProcessName)" -ForegroundColor Yellow
        $init = New-InitCmd
        $cmd_parts = [System.Collections.Generic.List[string]]::new()
        $cmd_parts.Add($init)
        if ($WriteDump) { $cmd_parts.Add(".dump $(Get-DumpFlag $DumpType) /u `"$DumpOut`"") }
        if ($ExtraCmd -and $ExtraScripts.Count -eq 0) { $cmd_parts.Add($ExtraCmd) }
        $cmd_parts.Add($load_script)
        $final_cmd = $cmd_parts -join "; "

        $cdb_args = [System.Collections.Generic.List[string]]::new()
        $cdb_args.Add("-p"); $cdb_args.Add([string]$found_pid)
        foreach ($a in $sym_args) { $cdb_args.Add($a) }
        $cdb_args.Add("-c"); $cdb_args.Add($final_cmd)
        if ($Admin) {
            Start-Process $CDB_PATH -ArgumentList $cdb_args -Verb RunAs -Wait
        } else {
            Invoke-Cdb -CdbPath $CDB_PATH -CdbArgs $cdb_args -IsDryRun:$DryRun
        }
    }
}

# ---------------------------------------------------------------------------
# cleanup and summary
# ---------------------------------------------------------------------------

foreach ($tmp in $_temps_to_cleanup) {
    Remove-Item $tmp -ErrorAction SilentlyContinue
}

Write-Host ""
$resolvedLog = Resolve-LogopenTPath -RequestedPath $Log -IsDryRun:$DryRun
Write-Host "Log: $resolvedLog" -ForegroundColor Green
if (-not $DryRun -and $resolvedLog -ne $Log) {
    Write-Host "  (requested base: $Log)" -ForegroundColor DarkGray
}
for ($i = 0; $i -lt $_extra_logs.Count; $i++) {
    $resolvedExtra = Resolve-LogopenTPath -RequestedPath $_extra_logs[$i] -IsDryRun:$DryRun
    Write-Host ("ExtraLog {0}: {1}" -f ($i + 1), $resolvedExtra) -ForegroundColor Green
    if (-not $DryRun -and $resolvedExtra -ne $_extra_logs[$i]) {
        Write-Host ("  (requested base: {0})" -f $_extra_logs[$i]) -ForegroundColor DarkGray
    }
}
if ($WriteDump -and (Test-Path $DumpOut -ErrorAction SilentlyContinue)) {
    Write-Host "Dump: $DumpOut" -ForegroundColor Green
}
