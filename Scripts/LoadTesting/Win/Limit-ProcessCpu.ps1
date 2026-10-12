<#
.SYNOPSIS
Restricts how much CPU a process is allowed to use, via core affinity and/or
a Windows Job Object hard CPU-rate cap.

.DESCRIPTION
Limit-ProcessCpu.ps1 is a general-purpose CPU-throttling tool, not tied to
any specific application. It supports two independent, combinable
restriction mechanisms:

  - Core affinity (-CoreCount / -CoreIndices): restricts the process to a
    subset of this machine's logical cores, via .NET's
    Process.ProcessorAffinity. Pure managed code, no elevated privileges
    required (beyond whatever is needed to touch the target process at
    all), reversible at any time, including back to "all cores"
    (-ResetAffinity).

  - Job Object CPU rate control (-CpuRatePercent): hard-caps the process's
    total CPU consumption to a percentage of the machine's overall CPU
    capacity, using the Win32 Job Object API
    (JobObjectCpuRateControlInformation). Implemented via P/Invoke, since
    no built-in PowerShell cmdlet or .NET API exposes it. This is a
    scheduler-level limit: once the process's CPU-cycle budget for the
    current interval is used up, none of its threads run again until the
    next interval - conceptually similar to how a cgroup CPU quota works
    on Linux.

Combine both to approximate a resource-constrained machine: e.g. 2 cores at
a 40% overall CPU cap is a tighter constraint than either alone, and is
closer to a genuinely weak/old machine's *available CPU budget* for one
process than either mechanism by itself - though neither changes the
underlying CPU's microarchitecture (per-core IPC, cache size, memory
latency), so this only approximates "weak hardware", it does not reproduce
it.

You can target an already-running process (-ProcessName or -Id) or launch a
new one and immediately apply limits to it (-ExePath, optionally
-ArgumentList). Limits are applied as soon as possible after the process
handle is available - there is a small unavoidable window between process
creation and the first P/Invoke call where the process runs unconstrained;
this is inherent to Windows (there is no supported way to pre-set affinity
or a Job Object before the first instruction of a new process executes
without also suspending it, which this script does not do to keep the
common case simple).

.PARAMETER ProcessName
Name of an already-running process to target (same value Get-Process
-Name accepts, without the .exe extension). If more than one process
shares this name, the limit is applied to all of them and each is reported
individually; use -Id instead to target exactly one.

.PARAMETER Id
Process ID (PID) of an already-running process to target. Use this to
disambiguate when multiple processes share the same name.

.PARAMETER ExePath
Path to an executable to launch and immediately apply limits to. Mutually
exclusive with -ProcessName and -Id.

.PARAMETER ArgumentList
Command-line arguments to pass to the process started via -ExePath. Ignored
(and rejected) if -ExePath is not also given.

.PARAMETER CoreCount
Restrict the process to the first N logical cores (0..N-1). Mutually
exclusive with -CoreIndices. Range 1-64 - Windows expresses affinity as a
single 64-bit mask per processor group, so machines with more than 64
logical cores (multiple processor groups) are only partially addressable
by this script; see NOTES.

.PARAMETER CoreIndices
Restrict the process to exactly these logical core indices (0-based), e.g.
-CoreIndices 2,3 to use the third and fourth logical cores specifically
instead of "the first N". Mutually exclusive with -CoreCount.

.PARAMETER ResetAffinity
Reset the process's affinity back to all logical cores on this machine,
undoing a previous -CoreCount/-CoreIndices restriction. Can be combined
with -CpuRatePercent in the same call (e.g. drop a core restriction while
still applying a CPU-rate cap).

.PARAMETER CpuRatePercent
Hard-cap total CPU usage to this percentage (1-100) of the whole machine's
capacity, via a Windows Job Object. THIS CANNOT BE UNDONE without
restarting the target process - per Microsoft's own documentation, a
process cannot be removed from a Job Object once assigned, only
terminated. Because of this, applying this specific limit prompts for
confirmation by default (standard PowerShell -Confirm/-WhatIf support);
pass -Confirm:$false to suppress the prompt in an unattended script.

.PARAMETER Force
Suppresses the multi-match warning when -ProcessName matches more than one
running process (the limit is still applied to all of them either way;
this only silences the warning for scripted/unattended use).

.PARAMETER PassThru
Emit the affected System.Diagnostics.Process object(s) to the pipeline
after applying limits, for further scripting.

.PARAMETER SelfTest
Runs this script's own built-in test suite instead of limiting anything:
spins up disposable throwaway processes, exercises every parameter
combination and error path below against them, reports PASS/FAIL per case,
and cleans up after itself. A lone flag - combining it with any other
parameter is rejected immediately, before anything runs. Exits with a
non-zero code if any case fails, so it can be used as a CI/smoke-test gate.

.PARAMETER Help
Prints this full help text (equivalent to `Get-Help <script> -Full`) and
exits without touching any process. Always takes priority over every other
parameter, including -SelfTest - combine it with anything and only the
help text is shown.

.EXAMPLE
.\Limit-ProcessCpu.ps1 -Help
Prints this help text and exits.

.EXAMPLE
.\Limit-ProcessCpu.ps1 -SelfTest
Runs the built-in test suite and reports pass/fail for every case; does not
touch any real process you care about.

.EXAMPLE
.\Limit-ProcessCpu.ps1 -ProcessName PeakExpertNoGUI -CoreCount 2
Restricts every currently-running PeakExpertNoGUI process to its first 2
logical cores.

.EXAMPLE
.\Limit-ProcessCpu.ps1 -ProcessName PeakExpertNoGUI -CoreIndices 2,3 -CpuRatePercent 30
Restricts PeakExpertNoGUI to logical cores 2 and 3 specifically, AND caps
its total CPU usage at 30% of the whole machine - prompts for confirmation
because of the irreversible CPU-rate cap.

.EXAMPLE
.\Limit-ProcessCpu.ps1 -ExePath "C:\Program Files\PeakExpertWeb\PeakExpertNoGUI\PeakExpertNoGUI.exe" -CoreCount 2 -CpuRatePercent 40 -Confirm:$false
Launches PeakExpertNoGUI fresh and immediately restricts it to 2 cores and
40% overall CPU, without an interactive confirmation prompt (for use in an
automated test harness).

.EXAMPLE
.\Limit-ProcessCpu.ps1 -Id 12345 -ResetAffinity
Restores full-machine affinity for the process with PID 12345, undoing an
earlier -CoreCount/-CoreIndices restriction. Does not affect an existing
-CpuRatePercent cap, which cannot be lifted this way.

.EXAMPLE
.\Limit-ProcessCpu.ps1 -ProcessName notepad -CoreCount 1 -PassThru | Select-Object Id, ProcessName, ProcessorAffinity
Applies the limit and pipes the affected process object(s) onward for
inspection or further scripting.

.NOTES
Job Object lifetime (per Microsoft's own Job Objects documentation): "The
job is destroyed when its last handle has been closed and all associated
processes have been terminated." This script does not keep its own handle
open once it exits, but the -CpuRatePercent cap still persists for the
target process's entire remaining lifetime, because the process itself
keeps the job alive as long as it runs - you do not need to keep this
script running for the cap to hold.

Requires running in the same user session as the target process (or
elevated, if the target runs elevated) - AssignProcessToJobObject needs
PROCESS_SET_QUOTA and PROCESS_TERMINATE rights on the target's process
handle, which .NET's Process.Handle property requests via OpenProcess; a
cross-user or cross-elevation-boundary target will fail with an
access-denied Win32 error, reported with its numeric code so it can be
looked up if it isn't self-explanatory.

Machines with more than 64 logical cores (multiple Windows processor
groups) are only partially addressable: both Process.ProcessorAffinity and
this script's -CoreCount/-CoreIndices only reach the process's initial
processor group (bits 0-63), the same limitation .NET itself has.

Win32 API references consulted while writing the CPU-rate-control code path
(verified against the live docs the day this script was written, not
recalled from memory alone):
https://learn.microsoft.com/en-us/windows/win32/api/winnt/ns-winnt-jobobject_cpu_rate_control_information
https://learn.microsoft.com/en-us/windows/win32/api/jobapi2/nf-jobapi2-setinformationjobobject
https://learn.microsoft.com/en-us/windows/win32/api/jobapi2/nf-jobapi2-assignprocesstojobobject
https://learn.microsoft.com/en-us/windows/win32/procthread/job-objects

Known gap: this script throttles CPU only. It does not restrict a
process's available RAM (there is no equivalent one-liner - the closest
Win32 mechanism is JOBOBJECT_EXTENDED_LIMIT_INFORMATION's
ProcessMemoryLimit/JobMemoryLimit, not implemented here).
#>

#Requires -Version 5.0

[CmdletBinding(SupportsShouldProcess, ConfirmImpact = 'High')]
[OutputType([System.Diagnostics.Process])]
param(
    [string]$ProcessName,

    [int]$Id,

    [string]$ExePath,

    [string[]]$ArgumentList,

    [ValidateRange(1, 64)]
    [int]$CoreCount,

    [ValidateRange(0, 63)]
    [int[]]$CoreIndices,

    [switch]$ResetAffinity,

    [ValidateRange(1, 100)]
    [int]$CpuRatePercent,

    [switch]$Force,

    [switch]$PassThru,

    [switch]$SelfTest,

    [switch]$Help
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

if ($Help) {
    Get-Help $PSCommandPath -Full
    return
}

function Invoke-SelfTest {
    $results = New-Object System.Collections.Generic.List[object]
    $spawned = New-Object System.Collections.Generic.List[int]
    $tempFiles = New-Object System.Collections.Generic.List[string]

    # Real Windows PowerShell executable, resolved via PATH rather than
    # assumed from $PSHOME - $PSHOME points at pwsh's own install directory
    # (which contains pwsh.exe, not powershell.exe) when this self-test is
    # run under PowerShell 7+, not under Windows PowerShell 5.1.
    $realPowerShellExe = (Get-Command powershell.exe -ErrorAction Stop).Source

    # A uniquely-named copy of powershell.exe (+ its .config sidecar, so its
    # .NET Framework runtime behavior is unaffected), used ONLY for the
    # -ProcessName multi-match test below. This deliberately avoids ever
    # calling -ProcessName with a common real-world name like "powershell" -
    # doing so during development matched and mutated the affinity of
    # unrelated leftover processes on the test machine that this self-test
    # did not spawn. Copying (not moving/modifying) System32's own
    # powershell.exe into %TEMP% under a private name is the safe way to get
    # two processes that share an exact name only this self-test could ever
    # match.
    $uniqueBaseName = "pew-selftest-$([Guid]::NewGuid().ToString('N').Substring(0, 8))"
    $uniqueExePath = Join-Path ([System.IO.Path]::GetTempPath()) "$uniqueBaseName.exe"
    $uniqueConfigPath = "$uniqueExePath.config"

    function Add-Result([string]$Case, [bool]$Passed, [string]$Detail) {
        $results.Add([pscustomobject]@{ Case = $Case; Passed = $Passed; Detail = $Detail })
    }

    function New-DisposableProcess {
        param([switch]$Unique)
        if ($Unique) {
            if (-not (Test-Path -LiteralPath $uniqueExePath)) {
                Copy-Item -LiteralPath $realPowerShellExe -Destination $uniqueExePath -Force
                $tempFiles.Add($uniqueExePath)
                $realConfigPath = "$realPowerShellExe.config"
                if (Test-Path -LiteralPath $realConfigPath) {
                    Copy-Item -LiteralPath $realConfigPath -Destination $uniqueConfigPath -Force
                    $tempFiles.Add($uniqueConfigPath)
                }
            }
            $p = Start-Process -FilePath $uniqueExePath -ArgumentList "-NoProfile", "-NonInteractive", "-Command", "Start-Sleep -Seconds 90" -PassThru
        } else {
            $p = Start-Process -FilePath $realPowerShellExe -ArgumentList "-NoProfile", "-NonInteractive", "-Command", "Start-Sleep -Seconds 90" -PassThru
        }
        Start-Sleep -Milliseconds 300
        $spawned.Add($p.Id)
        return $p
    }

    function Test-Throws([string]$Case, [scriptblock]$Block, [string]$ExpectedSubstring) {
        try {
            & $Block | Out-Null
            Add-Result $Case $false "Expected an exception containing '$ExpectedSubstring' but none was thrown."
        } catch {
            if ($_.Exception.Message -like "*$ExpectedSubstring*") {
                Add-Result $Case $true $_.Exception.Message
            } else {
                Add-Result $Case $false "Threw, but message did not match expected substring. Got: $($_.Exception.Message)"
            }
        }
    }

    try {
        Write-Host "Running self-test for $PSCommandPath ..." -ForegroundColor Cyan
        $logicalCores = [Environment]::ProcessorCount

        # --- the -SelfTest exclusivity guarantee itself ---
        Test-Throws "-SelfTest rejects being combined with another parameter" `
            { & $PSCommandPath -SelfTest -CoreCount 1 } `
            "cannot be combined"

        # --- -Help always short-circuits, even ahead of -SelfTest's own exclusivity check ---
        $helpOutput = & $PSCommandPath -Help -SelfTest -CoreCount 1 2>&1 | Out-String
        Add-Result "-Help takes priority over every other parameter, including -SelfTest" ($helpOutput -match 'SYNOPSIS') "Output $(if ($helpOutput -match 'SYNOPSIS') { 'contains' } else { 'does not contain' }) a SYNOPSIS section"

        # --- pure validation paths (fail before touching any process, safe with placeholder IDs) ---
        Test-Throws "no selector, no action"          { & $PSCommandPath }                                               "exactly one of"
        Test-Throws "both -ProcessName and -Id"       { & $PSCommandPath -ProcessName powershell -Id 1 -CoreCount 1 }    "exactly one of"
        Test-Throws "-CoreCount and -CoreIndices"     { & $PSCommandPath -Id 1 -CoreCount 1 -CoreIndices 0 }             "only one of"
        Test-Throws "-ArgumentList without -ExePath"  { & $PSCommandPath -Id 1 -ArgumentList "x" -CoreCount 1 }          "only makes sense together"
        Test-Throws "no action specified"             { & $PSCommandPath -Id 1 }                                        "Specify at least one action"
        Test-Throws "-CoreIndices above attribute range (0-63)" { & $PSCommandPath -Id 1 -CoreIndices 99 }               "Cannot validate argument"
        Test-Throws "-CoreIndices within 0-63 but beyond this machine's real core count" `
            { & $PSCommandPath -Id 1 -CoreIndices ($logicalCores + 1) } `
            "logical cores"
        Test-Throws "-CoreCount exceeding this machine's real core count" `
            { & $PSCommandPath -Id 1 -CoreCount ($logicalCores + 1) } `
            "logical cores"
        Test-Throws "no such PID"                     { & $PSCommandPath -Id 999999 -CoreCount 1 }                       "No running process with PID"
        Test-Throws "no such process name"            { & $PSCommandPath -ProcessName "definitely-not-a-real-process-xyz" -CoreCount 1 } "No running process named"
        Test-Throws "nonexistent -ExePath"            { & $PSCommandPath -ExePath "C:\does\not\exist.exe" -CoreCount 1 } "does not exist"

        # --- functional paths against a real, disposable throwaway process ---
        $t1 = New-DisposableProcess

        & $PSCommandPath -Id $t1.Id -CoreCount 2 -Confirm:$false | Out-Null
        $mask = (Get-Process -Id $t1.Id).ProcessorAffinity.ToInt64()
        Add-Result "-CoreCount 2 sets mask 0x3" ($mask -eq 3) "Got mask 0x$($mask.ToString('X'))"

        & $PSCommandPath -Id $t1.Id -CoreIndices 1 -Confirm:$false | Out-Null
        $mask = (Get-Process -Id $t1.Id).ProcessorAffinity.ToInt64()
        Add-Result "-CoreIndices 1 sets mask 0x2" ($mask -eq 2) "Got mask 0x$($mask.ToString('X'))"

        & $PSCommandPath -Id $t1.Id -ResetAffinity -Confirm:$false | Out-Null
        $mask = (Get-Process -Id $t1.Id).ProcessorAffinity.ToInt64()
        $expected = [int64]([Math]::Pow(2, $logicalCores)) - 1
        Add-Result "-ResetAffinity restores the full-machine mask" ($mask -eq $expected) "Got mask 0x$($mask.ToString('X')), expected 0x$($expected.ToString('X'))"

        $u1 = New-DisposableProcess -Unique
        $u2 = New-DisposableProcess -Unique
        & $PSCommandPath -ProcessName $uniqueBaseName -CoreCount 1 -Confirm:$false -WarningAction SilentlyContinue | Out-Null
        $bothRestricted = ((Get-Process -Id $u1.Id).ProcessorAffinity.ToInt64() -eq 1) -and ((Get-Process -Id $u2.Id).ProcessorAffinity.ToInt64() -eq 1)
        Add-Result "-ProcessName multi-match applies to every match" $bothRestricted "u1 mask=0x$((Get-Process -Id $u1.Id).ProcessorAffinity.ToInt64().ToString('X')), u2 mask=0x$((Get-Process -Id $u2.Id).ProcessorAffinity.ToInt64().ToString('X'))"

        & $PSCommandPath -Id $t1.Id -CpuRatePercent 30 -WhatIf | Out-Null
        Add-Result "-WhatIf on -CpuRatePercent does not throw" $true "(WhatIf output is not independently re-parsed here; absence of an exception is the pass condition)"

        & $PSCommandPath -Id $t1.Id -CpuRatePercent 30 -Confirm:$false | Out-Null
        Add-Result "-CpuRatePercent applies without throwing" $true "Job Object assignment reported success (this self-test does not independently verify the CPU cap actually holds under load)"

        $launched = & $PSCommandPath -ExePath $realPowerShellExe -ArgumentList "-NoProfile", "-NonInteractive", "-Command", "Start-Sleep -Seconds 90" -CoreCount 1 -Confirm:$false -PassThru
        if ($launched) { $spawned.Add($launched.Id) }
        $launchedOk = [bool]$launched -and ($launched.ProcessorAffinity.ToInt64() -eq 1)
        Add-Result "-ExePath launches a new process and restricts it" $launchedOk $(if ($launched) { "PID $($launched.Id), mask 0x$($launched.ProcessorAffinity.ToInt64().ToString('X'))" } else { "no process object returned" })
    } finally {
        foreach ($procId in $spawned) {
            Stop-Process -Id $procId -Force -ErrorAction SilentlyContinue
        }
        if ($spawned.Count -gt 0) {
            Start-Sleep -Milliseconds 300
        }
        foreach ($file in $tempFiles) {
            Remove-Item -LiteralPath $file -Force -ErrorAction SilentlyContinue
        }
    }

    $results | Format-Table -AutoSize -Property Case, Passed, Detail
    $failed = @($results | Where-Object { -not $_.Passed })
    Write-Host ""
    if ($failed.Count -gt 0) {
        Write-Error "$($failed.Count) of $($results.Count) self-test case(s) FAILED."
        exit 1
    }
    Write-Host "All $($results.Count) self-test case(s) passed." -ForegroundColor Green
}

if ($SelfTest) {
    $others = @($PSBoundParameters.Keys | Where-Object { $_ -ne 'SelfTest' })
    if ($others.Count -gt 0) {
        throw "-SelfTest cannot be combined with any other parameter (got: $($others -join ', '))."
    }
    Invoke-SelfTest
    return
}

# --- validate the combination of arguments up front, before touching anything ---
$selectors = @(@('ProcessName', 'Id', 'ExePath') | Where-Object { $PSBoundParameters.ContainsKey($_) })
if ($selectors.Count -ne 1) {
    throw "Specify exactly one of -ProcessName, -Id, or -ExePath to select the target process. Got: $(if ($selectors.Count -eq 0) { '(none)' } else { $selectors -join ', ' })."
}
if ($PSBoundParameters.ContainsKey('ArgumentList') -and -not $PSBoundParameters.ContainsKey('ExePath')) {
    throw "-ArgumentList only makes sense together with -ExePath."
}
if ($PSBoundParameters.ContainsKey('CoreCount') -and $PSBoundParameters.ContainsKey('CoreIndices')) {
    throw "Specify only one of -CoreCount or -CoreIndices, not both."
}
if (-not $PSBoundParameters.ContainsKey('CoreCount') -and -not $PSBoundParameters.ContainsKey('CoreIndices') -and
    -not $ResetAffinity -and -not $PSBoundParameters.ContainsKey('CpuRatePercent')) {
    throw "Specify at least one action: -CoreCount, -CoreIndices, -ResetAffinity, and/or -CpuRatePercent."
}

$logicalCores = [Environment]::ProcessorCount
if ($CoreIndices) {
    $badIndex = @($CoreIndices | Where-Object { $_ -ge $logicalCores })
    if ($badIndex) {
        throw "-CoreIndices contains $($badIndex -join ', ') but this machine only has $logicalCores logical cores (valid range 0..$($logicalCores - 1))."
    }
}
if ($CoreCount -gt $logicalCores) {
    throw "-CoreCount $CoreCount exceeds this machine's $logicalCores logical cores."
}

# --- resolve the target process(es), or launch one ---
$targets = @()
if ($PSBoundParameters.ContainsKey('ExePath')) {
    if (-not (Test-Path -LiteralPath $ExePath -PathType Leaf)) {
        throw "-ExePath '$ExePath' does not exist or is not a file."
    }
    $startArgs = @{ FilePath = $ExePath; PassThru = $true }
    if ($ArgumentList) { $startArgs.ArgumentList = $ArgumentList }
    try {
        $targets = @(Start-Process @startArgs)
    } catch {
        throw "Failed to start '$ExePath': $($_.Exception.Message)"
    }
    Write-Verbose "Started '$ExePath' as PID $($targets[0].Id)."
} elseif ($PSBoundParameters.ContainsKey('Id')) {
    $p = Get-Process -Id $Id -ErrorAction SilentlyContinue
    if (-not $p) {
        throw "No running process with PID $Id was found."
    }
    $targets = @($p)
} else {
    $targets = @(Get-Process -Name $ProcessName -ErrorAction SilentlyContinue)
    if ($targets.Count -eq 0) {
        throw "No running process named '$ProcessName' was found. Use -ExePath to launch one, or -Id if you already know its process ID."
    }
    if ($targets.Count -gt 1 -and -not $Force) {
        Write-Warning "$($targets.Count) processes named '$ProcessName' are running (PIDs: $($targets.Id -join ', ')). Applying the requested limit(s) to all of them; use -Id to target just one, or -Force to silence this warning."
    }
}

# --- Job Object P/Invoke declarations, loaded once per session ---
if ($CpuRatePercent -and -not ('PewLoadTesting.JobObject' -as [type])) {
    Add-Type -Namespace PewLoadTesting -Name JobObject -MemberDefinition @'
        [DllImport("kernel32.dll", SetLastError = true, CharSet = CharSet.Unicode)]
        public static extern IntPtr CreateJobObject(IntPtr lpJobAttributes, string lpName);

        [DllImport("kernel32.dll", SetLastError = true)]
        public static extern bool AssignProcessToJobObject(IntPtr hJob, IntPtr hProcess);

        [DllImport("kernel32.dll", SetLastError = true)]
        public static extern bool SetInformationJobObject(IntPtr hJob, int JobObjectInfoClass, IntPtr lpJobObjectInfo, uint cbJobObjectInfoLength);

        [DllImport("kernel32.dll", SetLastError = true)]
        public static extern bool CloseHandle(IntPtr hObject);

        [StructLayout(LayoutKind.Sequential)]
        public struct JOBOBJECT_CPU_RATE_CONTROL_INFORMATION
        {
            public uint ControlFlags;
            public uint CpuRate;
        }
'@
}

# JOBOBJECTINFOCLASS.JobObjectCpuRateControlInformation = 15 and the
# ControlFlags bit values below are confirmed against the structure's own
# Microsoft Learn doc page (see .NOTES for the exact URLs).
$JobObjectCpuRateControlInformation = 15
$JOB_OBJECT_CPU_RATE_CONTROL_ENABLE = 0x1
$JOB_OBJECT_CPU_RATE_CONTROL_HARD_CAP = 0x4

foreach ($proc in $targets) {
    $label = "$($proc.ProcessName) (PID $($proc.Id))"

    if ($ResetAffinity) {
        try {
            $allCoresMask = [IntPtr]([int64]([math]::Pow(2, $logicalCores)) - 1)
            $proc.Refresh()
            $proc.ProcessorAffinity = $allCoresMask
            Write-Host "$label - affinity reset to all $logicalCores logical core(s)."
        } catch [System.ComponentModel.Win32Exception] {
            Write-Error "$label - failed to reset affinity (Win32 error $($_.Exception.NativeErrorCode)): $($_.Exception.Message). Likely cause: this script is not running as the same user (or elevated) as the target process."
            continue
        } catch [System.InvalidOperationException] {
            Write-Error "$label - process exited before affinity could be reset."
            continue
        }
    } elseif ($CoreCount -or $CoreIndices) {
        try {
            if ($CoreCount) {
                $mask = [IntPtr](([int64]1 -shl $CoreCount) - 1)
                $description = "first $CoreCount logical core(s)"
            } else {
                $bits = 0L
                foreach ($idx in $CoreIndices) { $bits = $bits -bor ([int64]1 -shl $idx) }
                $mask = [IntPtr]$bits
                $description = "logical core(s) $($CoreIndices -join ', ')"
            }
            $proc.Refresh()
            $proc.ProcessorAffinity = $mask
            Write-Host "$label - affinity restricted to $description (mask 0x$($mask.ToInt64().ToString('X')))."
        } catch [System.ComponentModel.Win32Exception] {
            Write-Error "$label - failed to set affinity (Win32 error $($_.Exception.NativeErrorCode)): $($_.Exception.Message). Likely cause: this script is not running as the same user (or elevated) as the target process."
            continue
        } catch [System.InvalidOperationException] {
            Write-Error "$label - process exited before affinity could be set."
            continue
        }
    }

    if ($CpuRatePercent) {
        $actionDescription = "Hard-cap CPU to $CpuRatePercent% via Job Object (cannot be undone without restarting the process)"
        if (-not $PSCmdlet.ShouldProcess($label, $actionDescription)) {
            continue
        }

        $hJob = [IntPtr]::Zero
        try {
            $hJob = [PewLoadTesting.JobObject]::CreateJobObject([IntPtr]::Zero, $null)
            if ($hJob -eq [IntPtr]::Zero) {
                $err = [System.Runtime.InteropServices.Marshal]::GetLastWin32Error()
                Write-Error "$label - CreateJobObject failed, Win32 error $err."
                continue
            }

            $info = New-Object -TypeName "PewLoadTesting.JobObject+JOBOBJECT_CPU_RATE_CONTROL_INFORMATION"
            $info.ControlFlags = $JOB_OBJECT_CPU_RATE_CONTROL_ENABLE -bor $JOB_OBJECT_CPU_RATE_CONTROL_HARD_CAP
            # CpuRate is "a percentage times 100" per the structure's doc page, e.g. 30% -> 3000. Must never be 0.
            $info.CpuRate = [uint32]($CpuRatePercent * 100)

            $size = [System.Runtime.InteropServices.Marshal]::SizeOf($info)
            $buf = [System.Runtime.InteropServices.Marshal]::AllocHGlobal($size)
            try {
                [System.Runtime.InteropServices.Marshal]::StructureToPtr($info, $buf, $false)
                $ok = [PewLoadTesting.JobObject]::SetInformationJobObject($hJob, $JobObjectCpuRateControlInformation, $buf, $size)
                if (-not $ok) {
                    $err = [System.Runtime.InteropServices.Marshal]::GetLastWin32Error()
                    Write-Error "$label - SetInformationJobObject failed, Win32 error $err."
                    continue
                }
            } finally {
                [System.Runtime.InteropServices.Marshal]::FreeHGlobal($buf)
            }

            $procHandle = $proc.Handle
            $ok = [PewLoadTesting.JobObject]::AssignProcessToJobObject($hJob, $procHandle)
            if (-not $ok) {
                $err = [System.Runtime.InteropServices.Marshal]::GetLastWin32Error()
                Write-Error "$label - AssignProcessToJobObject failed, Win32 error $err. Likely cause: the process is already assigned to an incompatible job, or this script lacks PROCESS_SET_QUOTA/PROCESS_TERMINATE rights on it (try running elevated)."
                continue
            }
            Write-Host "$label - CPU rate hard-capped at $CpuRatePercent% via Job Object (persists for the rest of this process's lifetime)."
        } catch [System.InvalidOperationException] {
            Write-Error "$label - process exited before the CPU-rate cap could be applied."
            continue
        } finally {
            # AssignProcessToJobObject keeps the job alive for as long as the
            # target process runs (see .NOTES); this script's own handle is
            # no longer needed once that association exists, on any path.
            if ($hJob -ne [IntPtr]::Zero) {
                [PewLoadTesting.JobObject]::CloseHandle($hJob) | Out-Null
            }
        }
    }

    if ($PassThru) {
        $proc
    }
}
