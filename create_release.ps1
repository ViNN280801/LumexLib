# create_release.ps1
#Requires -Version 5.1
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)] [string] $Version,      
    [Parameter(Mandatory = $true)] [string] $CompilerName,
    [ValidateSet('v141', 'v142', 'v143')] [string] $Toolset = 'v142'
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$sw = [System.Diagnostics.Stopwatch]::StartNew()

$InstallPrefix = 'D:\local\LumexCore'
$CMakeValue = '-DLUMEX_BUILD_XML=ON'
$CMakeArgToken = "--cmake-args=$CMakeValue"

function Invoke-Build([string]$arch) {
    foreach ($std in 11, 14, 17, 20) {
        & python 'compile.py' 'Release' `
            '-m' $arch '--shared-libs' '--toolset' $Toolset '--std' $std '--clean' `
            '--install-prefix' $InstallPrefix $CMakeArgToken
    }
}

function Pack-Arch([string]$arch) {
    $dirs = @()
    foreach ($std in 11, 14, 17, 20) {
        $pattern = "{0}_win_{1}_*_cpp{2}" -f $Version, $arch, $std
        $match = @(Get-ChildItem -LiteralPath $InstallPrefix -Directory -Filter $pattern)
        if ($match.Count -ne 1) {
            throw ("Expected 1 folder for arch={0} cpp{1}, found {2}. Pattern: {3}" -f $arch, $std, $match.Count, $pattern)
        }
        $dirs += $match[0].FullName
    }

    $zip = Join-Path $InstallPrefix ("{0}_win_{1}_{2}.zip" -f $Version, $arch, $CompilerName)
    if (Test-Path -LiteralPath $zip) { Remove-Item -LiteralPath $zip -Force }
    Compress-Archive -Path $dirs -DestinationPath $zip -Force
}

Invoke-Build 'x86'
Invoke-Build 'x64'

Pack-Arch 'x86'
Pack-Arch 'x64'

$sw.Stop()
Write-Host ("Total build time: {0}h {1}m {2}s" -f [int]$sw.Elapsed.TotalHours, $sw.Elapsed.Minutes, $sw.Elapsed.Seconds)
