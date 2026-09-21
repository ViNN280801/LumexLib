<#
.SYNOPSIS
Stops the background worker processes started by Start-CpuContention.ps1.

.DESCRIPTION
Reads the PID file for a given session name (cpu-contention.<name>.pids,
next to this script), stops every worker process still running, and
removes the PID file. Safe to run even if some or all workers already
self-stopped (via -DurationSeconds) or were killed manually - already-dead
PIDs are silently skipped, not treated as errors.

.PARAMETER Name
Session name to stop, matching the -Name given to Start-CpuContention.ps1
(default "default").

.PARAMETER All
Stop every contention session found in this script's directory
(cpu-contention.*.pids), regardless of name. Use this when you are not
sure which session name(s) are still active.

.PARAMETER Help
Prints this full help text (equivalent to `Get-Help <script> -Full`) and
exits without stopping anything. Always takes priority over every other
parameter.

.EXAMPLE
.\Stop-CpuContention.ps1
Stops the default session.

.EXAMPLE
.\Stop-CpuContention.ps1 -Name background-load
Stops only the session named "background-load".

.EXAMPLE
.\Stop-CpuContention.ps1 -All
Stops every active contention session, whatever it is named.

.EXAMPLE
.\Stop-CpuContention.ps1 -Help
Prints this help text and exits without stopping anything.
#>

#Requires -Version 5.0

[CmdletBinding()]
[OutputType([void])]
param(
    [ValidatePattern('^[A-Za-z0-9_-]+$')]
    [string]$Name = "default",

    [switch]$All,

    [switch]$Help
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

if ($Help) {
    Get-Help $PSCommandPath -Full
    return
}

function Stop-Session {
    param([string]$SessionName)

    $pidFile = Join-Path $PSScriptRoot "cpu-contention.$SessionName.pids"
    if (-not (Test-Path -LiteralPath $pidFile)) {
        Write-Warning "No session named '$SessionName' found (expected $pidFile)."
        return
    }

    $trackedPids = @(Get-Content -LiteralPath $pidFile -ErrorAction SilentlyContinue | Where-Object { $_ -match '^\d+$' })
    $stopped = 0
    foreach ($procId in $trackedPids) {
        $p = Get-Process -Id $procId -ErrorAction SilentlyContinue
        if ($p) {
            Stop-Process -Id $procId -Force -ErrorAction SilentlyContinue
            $stopped++
        }
    }
    Remove-Item -LiteralPath $pidFile -Force
    Write-Host "Session '$SessionName': stopped $stopped worker(s) (of $($trackedPids.Count) tracked, the rest had already exited)."
}

if ($All) {
    $files = @(Get-ChildItem -Path $PSScriptRoot -Filter "cpu-contention.*.pids" -ErrorAction SilentlyContinue)
    if ($files.Count -eq 0) {
        Write-Host "No active contention sessions found."
        return
    }
    foreach ($f in $files) {
        $sessionName = $f.BaseName -replace '^cpu-contention\.', ''
        Stop-Session -SessionName $sessionName
    }
} else {
    Stop-Session -SessionName $Name
}
