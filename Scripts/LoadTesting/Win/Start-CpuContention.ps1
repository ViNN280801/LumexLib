<#
.SYNOPSIS
Generates synthetic CPU load in one or more background worker processes, to
create CPU contention alongside whatever else is running on this machine.

.DESCRIPTION
Start-CpuContention.ps1 is a pure-PowerShell stand-in for stress-ng's
"--cpu N --cpu-load X" mode (stress-ng itself has no native Windows build).
It starts -WorkerCount independent background powershell.exe processes,
each repeating a duty-cycle loop: busy for a fixed slice, then sleep just
long enough that busy-time / (busy-time + idle-time) matches -LoadPercent.

Use this to test whether CPU contention from unrelated work on the same
machine causes visible problems in another process you care about (e.g. a
thread that reads a serial/USB device missing its scheduling window) -
this generates *competition* for CPU time, it does not throttle any
specific process the way Limit-ProcessCpu.ps1 does. The two are meant to be
combined: restrict your target process to N cores with Limit-ProcessCpu.ps1,
then load those same N cores with this script's -PinToCores.

Each run is a named "session" (-Name, default "default") so multiple
independent contention profiles can run at once without colliding; workers
are tracked in a small PID file (cpu-contention.<name>.pids) next to this
script, read by the companion Stop-CpuContention.ps1.

.PARAMETER WorkerCount
Number of contending worker processes to start. Defaults to this machine's
logical core count - be aware this default means "load every core" if you
do not also lower -LoadPercent or pass -PinToCores with a smaller count;
oversubscribing (WorkerCount greater than the logical core count) is
allowed and is itself a valid stress scenario (thread-scheduling pressure,
not just raw CPU cycles), just be deliberate about it.

.PARAMETER LoadPercent
Target duty-cycle CPU load per worker, 1-100. 100 (the default) means each
worker runs flat out with no idle time.

.PARAMETER DurationSeconds
Workers self-stop after this many seconds. 0 (the default) means run
indefinitely, until Stop-CpuContention.ps1 is used or the machine reboots -
do not forget to stop an indefinite session.

.PARAMETER PinToCores
Pin each worker to one specific logical core (round-robin across
[Environment]::ProcessorCount) instead of leaving core placement to the
Windows scheduler. Use this when you need to guarantee specific cores are
loaded, e.g. to match cores you have restricted another process to via
Limit-ProcessCpu.ps1's -CoreCount/-CoreIndices.

.PARAMETER Name
Session name, used to build this run's PID file
(cpu-contention.<name>.pids) so multiple independent contention profiles
can run concurrently and be stopped independently. Must not contain path
separators or wildcard characters.

.PARAMETER Force
Start a new session even if a PID file for -Name already lists workers
that are still running (normally this is refused, since the old workers'
PIDs would be silently orphaned - no longer stoppable via
Stop-CpuContention.ps1 - once this run's PID file overwrites theirs).

.PARAMETER PassThru
Emit the started System.Diagnostics.Process object(s) to the pipeline.

.PARAMETER SelfTest
Runs this script's own built-in test suite instead of starting real
contention workers: exercises every parameter combination and error path
below (including the companion Stop-CpuContention.ps1) under uniquely
named, short-lived sessions, reports PASS/FAIL per case, and cleans up
after itself. A lone flag - combining it with any other parameter is
rejected immediately, before anything runs. Exits with a non-zero code if
any case fails, so it can be used as a CI/smoke-test gate.

.PARAMETER Help
Prints this full help text (equivalent to `Get-Help <script> -Full`) and
exits without starting any workers. Always takes priority over every other
parameter, including -SelfTest.

.EXAMPLE
.\Start-CpuContention.ps1 -Help
Prints this help text and exits.

.EXAMPLE
.\Start-CpuContention.ps1 -SelfTest
Runs the built-in test suite and reports pass/fail for every case.

.EXAMPLE
.\Start-CpuContention.ps1 -WorkerCount 2 -LoadPercent 80 -PinToCores
Two workers, each busy 80% of the time, pinned to logical cores 0 and 1.

.EXAMPLE
.\Start-CpuContention.ps1 -Name background-load -LoadPercent 50 -DurationSeconds 300
Loads every logical core at ~50% for 5 minutes, tracked under the session
name "background-load" so it can be stopped independently of any other
running session.

.EXAMPLE
$workers = .\Start-CpuContention.ps1 -WorkerCount 1 -LoadPercent 100 -DurationSeconds 60 -PassThru
One worker at full load for 60 seconds, capturing the started process
object in $workers for further scripting (e.g. Wait-Process).

.NOTES
Workers that reach -DurationSeconds stop themselves, but do not clean up
their own entry in the PID file; running Stop-CpuContention.ps1 afterward
is still safe and correct (it silently skips PIDs that are no longer
running).
#>

#Requires -Version 5.0

[CmdletBinding()]
[OutputType([System.Diagnostics.Process])]
param(
    [ValidateRange(1, 256)]
    [int]$WorkerCount = [Environment]::ProcessorCount,

    [ValidateRange(1, 100)]
    [int]$LoadPercent = 100,

    [int]$DurationSeconds = 0,

    [switch]$PinToCores,

    [ValidatePattern('^[A-Za-z0-9_-]+$')]
    [string]$Name = "default",

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
    $spawnedPids = New-Object System.Collections.Generic.List[int]
    $sessionNames = New-Object System.Collections.Generic.List[string]
    $stopScript = Join-Path $PSScriptRoot "Stop-CpuContention.ps1"
    $runId = [Guid]::NewGuid().ToString('N').Substring(0, 8)

    function Add-Result([string]$Case, [bool]$Passed, [string]$Detail) {
        $results.Add([pscustomobject]@{ Case = $Case; Passed = $Passed; Detail = $Detail })
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

    function New-SessionName([string]$suffix) {
        $n = "selftest-$runId-$suffix"
        $sessionNames.Add($n)
        return $n
    }

    try {
        Write-Host "Running self-test for $PSCommandPath ..." -ForegroundColor Cyan
        $logicalCores = [Environment]::ProcessorCount

        # --- the -SelfTest exclusivity guarantee itself ---
        Test-Throws "-SelfTest rejects being combined with another parameter" `
            { & $PSCommandPath -SelfTest -WorkerCount 1 } `
            "cannot be combined"

        # --- -Help always short-circuits, even ahead of -SelfTest's own exclusivity check ---
        $helpOutput = & $PSCommandPath -Help -SelfTest -WorkerCount 1 2>&1 | Out-String
        Add-Result "-Help takes priority over every other parameter, including -SelfTest" ($helpOutput -match 'SYNOPSIS') "Output $(if ($helpOutput -match 'SYNOPSIS') { 'contains' } else { 'does not contain' }) a SYNOPSIS section"

        # --- parameter validation paths ---
        Test-Throws "-Name rejects invalid characters" { & $PSCommandPath -Name "bad name!" -DurationSeconds 1 } "Cannot validate argument"

        # --- basic start/stop lifecycle ---
        $basicName = New-SessionName "basic"
        & $PSCommandPath -Name $basicName -WorkerCount 1 -LoadPercent 100 -DurationSeconds 20 | Out-Null
        $pidFile = Join-Path $PSScriptRoot "cpu-contention.$basicName.pids"
        $pidsInFile = @(Get-Content -LiteralPath $pidFile -ErrorAction SilentlyContinue | Where-Object { $_ -match '^\d+$' })
        $pidsInFile | ForEach-Object { $spawnedPids.Add([int]$_) }
        Add-Result "start creates a PID file with WorkerCount PID(s)" ($pidsInFile.Count -eq 1) "Found $($pidsInFile.Count) PID(s) in $pidFile"

        $workerAlive = $pidsInFile.Count -gt 0 -and (Get-Process -Id $pidsInFile[0] -ErrorAction SilentlyContinue)
        Add-Result "worker process is actually running" ([bool]$workerAlive) "PID $($pidsInFile[0])"

        if ($workerAlive) {
            $before = (Get-Process -Id $pidsInFile[0]).TotalProcessorTime
            Start-Sleep -Seconds 1
            $procNow = Get-Process -Id $pidsInFile[0] -ErrorAction SilentlyContinue
            if ($procNow) {
                $delta = ($procNow.TotalProcessorTime - $before).TotalMilliseconds
                Add-Result "worker at LoadPercent=100 actually burns CPU" ($delta -gt 300) "CPU time advanced $([Math]::Round($delta))ms over ~1000ms wall time"
            } else {
                Add-Result "worker at LoadPercent=100 actually burns CPU" $false "Worker exited before the CPU-usage sample could be taken."
            }
        }

        # --- collision detection ---
        Test-Throws "starting the same session name again without -Force is refused" `
            { & $PSCommandPath -Name $basicName -WorkerCount 1 -DurationSeconds 20 } `
            "already has"

        $forceName = $basicName
        $beforeForce = @(Get-Content -LiteralPath $pidFile -ErrorAction SilentlyContinue | Where-Object { $_ -match '^\d+$' })
        & $PSCommandPath -Name $forceName -WorkerCount 1 -LoadPercent 100 -DurationSeconds 20 -Force | Out-Null
        $afterForce = @(Get-Content -LiteralPath $pidFile -ErrorAction SilentlyContinue | Where-Object { $_ -match '^\d+$' })
        $afterForce | ForEach-Object { $spawnedPids.Add([int]$_) }
        $oldOrphaned = $beforeForce.Count -gt 0 -and ($afterForce -notcontains $beforeForce[0])
        Add-Result "-Force starts a fresh batch, orphaning the old PID file entry" $oldOrphaned "Before: $($beforeForce -join ','); After: $($afterForce -join ',')"

        # --- -PinToCores ---
        if ($logicalCores -ge 2) {
            $pinName = New-SessionName "pin"
            & $PSCommandPath -Name $pinName -WorkerCount 2 -LoadPercent 100 -DurationSeconds 20 -PinToCores | Out-Null
            $pinPidFile = Join-Path $PSScriptRoot "cpu-contention.$pinName.pids"
            $pinPids = @(Get-Content -LiteralPath $pinPidFile -ErrorAction SilentlyContinue | Where-Object { $_ -match '^\d+$' })
            $pinPids | ForEach-Object { $spawnedPids.Add([int]$_) }
            $masks = @($pinPids | ForEach-Object { (Get-Process -Id $_ -ErrorAction SilentlyContinue).ProcessorAffinity.ToInt64() })
            $pinOk = ($pinPids.Count -eq 2) -and ($masks -contains 1) -and ($masks -contains 2)
            Add-Result "-PinToCores pins workers to cores 0 and 1 round-robin" $pinOk "Masks: $($masks -join ', ')"
            & $stopScript -Name $pinName | Out-Null
        } else {
            Add-Result "-PinToCores pins workers to cores 0 and 1 round-robin" $true "Skipped - this machine only has $logicalCores logical core(s)."
        }

        # --- Stop-CpuContention.ps1 integration ---
        & $stopScript -Name $basicName | Out-Null
        $stillTracked = Test-Path -LiteralPath $pidFile
        $stillRunning = @($afterForce | Where-Object { Get-Process -Id $_ -ErrorAction SilentlyContinue })
        Add-Result "Stop-CpuContention.ps1 removes the PID file and stops workers" ((-not $stillTracked) -and ($stillRunning.Count -eq 0)) "PID file present: $stillTracked; still-running workers: $($stillRunning -join ', ')"

        # --- stopping an already-stopped session is a graceful no-op, not an error ---
        $warnOutput = & $stopScript -Name $basicName 3>&1
        Add-Result "stopping an already-stopped session does not throw" $true "$warnOutput"

        # --- self-stop after DurationSeconds ---
        $selfStopName = New-SessionName "selfstop"
        & $PSCommandPath -Name $selfStopName -WorkerCount 1 -LoadPercent 100 -DurationSeconds 3 | Out-Null
        $selfStopPidFile = Join-Path $PSScriptRoot "cpu-contention.$selfStopName.pids"
        $selfStopPids = @(Get-Content -LiteralPath $selfStopPidFile -ErrorAction SilentlyContinue | Where-Object { $_ -match '^\d+$' })
        $selfStopPids | ForEach-Object { $spawnedPids.Add([int]$_) }
        Start-Sleep -Seconds 5
        $stillAliveAfterDuration = @($selfStopPids | Where-Object { Get-Process -Id $_ -ErrorAction SilentlyContinue })
        Add-Result "worker self-stops after -DurationSeconds elapses" ($stillAliveAfterDuration.Count -eq 0) "Still alive after duration + buffer: $($stillAliveAfterDuration -join ', ')"
        Remove-Item -LiteralPath $selfStopPidFile -Force -ErrorAction SilentlyContinue
    } finally {
        foreach ($procId in $spawnedPids) {
            Stop-Process -Id $procId -Force -ErrorAction SilentlyContinue
        }
        foreach ($n in $sessionNames) {
            Remove-Item -LiteralPath (Join-Path $PSScriptRoot "cpu-contention.$n.pids") -Force -ErrorAction SilentlyContinue
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

$pidFile = Join-Path $PSScriptRoot "cpu-contention.$Name.pids"

if ((Test-Path -LiteralPath $pidFile) -and -not $Force) {
    $existingPids = @(Get-Content -LiteralPath $pidFile -ErrorAction SilentlyContinue | Where-Object { $_ -match '^\d+$' })
    $stillAlive = @($existingPids | Where-Object { Get-Process -Id $_ -ErrorAction SilentlyContinue })
    if ($stillAlive.Count -gt 0) {
        throw "Session '$Name' already has $($stillAlive.Count) worker(s) running (PIDs: $($stillAlive -join ', ')). Stop it first with '.\Stop-CpuContention.ps1 -Name $Name', or pass -Force to start a new batch anyway (the old workers' PIDs will no longer be tracked, so Stop-CpuContention.ps1 will not be able to find them)."
    }
}

# Duty-cycle model: busy for a fixed slice, then sleep just long enough that
# busy/(busy+idle) equals the requested LoadPercent. A short slice (50 ms)
# keeps the load responsive without adding scheduler overhead from
# excessively fine-grained sleeps.
$busyMs = 50
$idleMs = [Math]::Max(0, [Math]::Round($busyMs * (100 - $LoadPercent) / [Math]::Max($LoadPercent, 1)))

$workerBody = @'
param($busyMs, $idleMs, $durationSeconds)
$sw = [Diagnostics.Stopwatch]::StartNew()
$x = 0.0
while ($durationSeconds -le 0 -or $sw.Elapsed.TotalSeconds -lt $durationSeconds) {
    $burst = [Diagnostics.Stopwatch]::StartNew()
    while ($burst.ElapsedMilliseconds -lt $busyMs) { $x = [Math]::Sqrt($x + 1.0) }
    if ($idleMs -gt 0) { Start-Sleep -Milliseconds $idleMs }
}
'@

$logicalCores = [Environment]::ProcessorCount
$started = @()
try {
    for ($i = 0; $i -lt $WorkerCount; $i++) {
        $argList = @(
            "-NoProfile", "-NonInteractive", "-Command",
            "& { $workerBody } $busyMs $idleMs $DurationSeconds"
        )
        $p = Start-Process -FilePath "powershell.exe" -ArgumentList $argList -PassThru -WindowStyle Hidden
        if ($PinToCores) {
            $core = $i % $logicalCores
            try {
                $p.ProcessorAffinity = [IntPtr](1 -shl $core)
            } catch [System.ComponentModel.Win32Exception] {
                Write-Warning "Worker PID $($p.Id) - failed to pin to core $core (Win32 error $($_.Exception.NativeErrorCode)); it will run unpinned instead."
            }
        }
        $started += $p
    }
} catch {
    if ($started.Count -gt 0) {
        Write-Warning "Startup failed partway through; stopping the $($started.Count) worker(s) already started."
        $started | ForEach-Object { Stop-Process -Id $_.Id -Force -ErrorAction SilentlyContinue }
    }
    throw
}

$started.Id | Set-Content -Path $pidFile
Write-Host "Session '$Name': $WorkerCount worker(s) started at ~$LoadPercent% duty cycle each (PIDs: $($started.Id -join ', '))."
Write-Host "Stop early with: .\Stop-CpuContention.ps1 -Name $Name"
if ($DurationSeconds -gt 0) {
    Write-Host "Workers self-stop after $DurationSeconds second(s)."
}

if ($PassThru) {
    $started
}
