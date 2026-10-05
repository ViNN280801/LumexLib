# create_release.ps1 - build LumexLib with every given compiler and
# architecture and package each build as
#   LumexLib-<version>_win_<ISA>_<compiler>.<zip|tar.gz|exe>
# where <compiler> is msvc<year> (the MSVC toolset of that Visual Studio,
# for example msvc2026) or clang-cl-<major.minor.patch>; the exe is the NSIS
# installer CPack builds through compile.py -i.
#
# The Windows counterpart of create_release.sh: the same options where the
# platform has them, the same order of work and the same package layout.
# Every check (python, cmake, ninja for clang-cl, the installed toolsets, the
# clang-cl compilers, NSIS for exe, tar for tar.gz) runs before the first
# build, so a missing piece stops the script before any work is done.
#
# Run with -Help for the options.
#Requires -Version 5.1
[CmdletBinding()]
param(
    # List parameters take the usual PowerShell comma lists (-Formats zip,exe)
    # as well as single comma-separated strings (-Formats 'zip,exe').
    [string[]] $Compilers = @(),
    [string[]] $Arch      = @('x64'),
    [string[]] $Formats   = @('zip'),
    [int]    $Std       = 0,
    [string] $OutputDir = '',
    [string] $Version   = '',
    [string] $Tag       = '',
    [switch] $KeepWork,
    [switch] $Help
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
# Compress-Archive writes a per-file progress bar by default: it floods the
# console and slows the archive down.
$ProgressPreference = 'SilentlyContinue'

$RepoRoot = $PSScriptRoot

function Write-Usage {
    @'
Usage: ./create_release.ps1 -Compilers <v143,D:\local\LLVM\bin\clang-cl.exe> [options]

Builds LumexLib (Release, shared libraries, no tests, no documentation) once
per compiler and ISA, installs it under <output-dir>/.work and packages each
build as LumexLib-<version>_win_<ISA>_<compiler>.<format>, for example
LumexLib-1.0.3.1_win_x64_msvc2026.zip or
LumexLib-1.0.3.1_win_x64_clang-cl-21.1.5.zip.

Required:
  -Compilers LIST    Comma-separated compilers. Each entry is either
                     * an MSVC toolset id - v120 (VS 2013), v140 (VS 2015),
                       v141 (VS 2017), v142 (VS 2019), v143 (VS 2022),
                       v144 (VS 2025), v145 (VS 2026+) - installed with the
                       Desktop C++ workload of a Visual Studio on this
                       machine; the artifact is labeled msvc<year of that
                       Visual Studio> (v145 -> msvc2026);
                     * or a clang-cl compiler: the name clang-cl (found in
                       PATH) or a path to clang-cl.exe; built with Ninja,
                       labeled clang-cl-<major.minor.patch> from the
                       compiler's own --version.
                     Example: -Compilers v143,D:\local\LLVM\bin\clang-cl.exe

Options:
  -Arch LIST         x64 (default), x86. Comma-separated, for example
                     -Arch x64,x86. x86 needs the same compiler's x86 tools.
  -Formats LIST      zip (default), tar.gz, exe. tar.gz needs tar in PATH
                     (shipped with Windows 10 and later); exe is the NSIS
                     installer built by CPack (compile.py -i) and needs NSIS
                     (makensis): MAKENSIS_EXE, PATH, the NSIS registry key,
                     the machine/user PATH values or the usual install
                     directories.
  -Std N             C++ standard for every build (11, 14, 17, 20, 23).
                     Default: the compile.py default (20); pass -Std 17 for a
                     compiler without C++20, for example the v141 toolset.
  -OutputDir DIR     Where the packages go (default: <repo>/release).
  -Version V         Override the version read from project(LumexLib
                     VERSION); it changes the artifact names only, the
                     libraries keep the CMakeLists.txt version.
  -Tag TAG           Build the sources of a git tag instead of the working
                     tree: a detached worktree of the tag is created under
                     <output-dir>/.work/src-<TAG>, its CMakeRoutines
                     submodule is initialized from this checkout's copy (no
                     network), and the packages are named after the tag's
                     project(LumexLib VERSION). This script drives the
                     build; compile.py and the CMakeLists come from the
                     tag. The worktree is removed at the end (kept with
                     -KeepWork; on an early stop remove it with:
                     git worktree remove --force <path>; git worktree prune).
  -KeepWork          Keep the build, staging and packaging trees under
                     <output-dir>/.work (removed by default).
  -Help              Show this help.

Notes:
  * The MSVC runtime (msvcp140.dll, vcruntime140*.dll) ships in bin/ next to
    the libraries: the install stages it with the pack's
    CopyRuntimeDependencies (the same set <platform>/Distr<Config> carries),
    so the consumer needs no separately installed redistributable.
  * The exe installs per machine into
    <Program Files>\LumexLib\<version>_<compiler>, with an uninstall entry
    in Add/Remove Programs; zip and tar.gz carry the same install tree as
    archives.
  * dev/ carries the developer files of every library: the import .lib
    files, the .exp files and the PDBs next to the release binaries.
  * -Tag builds a tag's sources in a worktree under <output-dir>/.work and
    removes it when the run ends; the tag's CMakeLists sets the version.
  * The found makensis directory goes in front of PATH, so CPack runs the
    same makensis; MAKENSIS_EXE overrides the search (a wrong value stops
    the script).
  * <repo>/x64 is removed after every package, so that published build
    output never lands in a later archive.
  * A release needs the dated [vX.Y.Z.W] section in CHANGELOG.md; the script
    warns when it is still marked "в разработке" (not dated).

The Linux counterpart is ./create_release.sh; -Compilers is its --compilers
(the entries are MSVC toolset ids or clang-cl instead of gcc/clang paths).
'@ | Write-Host
}

function Die([string] $Message) {
    [Console]::Error.WriteLine("Error: $Message")
    exit 1
}

# ---------------------------------------------------------------------------
# Arguments
# ---------------------------------------------------------------------------

if ($Help) {
    Write-Usage
    exit 0
}
function Split-List([string[]] $Value) {
    @(($Value -join ',') -split ',' | ForEach-Object { $_.Trim() } | Where-Object { $_ -ne '' })
}

$CompilerList = @(Split-List $Compilers)
$ArchList     = @(Split-List $Arch)
$FormatList   = @(Split-List $Formats)

if ($CompilerList.Count -eq 0) {
    Write-Usage
    Die '-Compilers is required'
}
if ($ArchList.Count -eq 0) { Die '-Arch is empty' }
if ($FormatList.Count -eq 0) { Die '-Formats is empty' }

$KnownToolsets = @('v120', 'v140', 'v141', 'v142', 'v143', 'v144', 'v145')
foreach ($isa in $ArchList) {
    if ($isa -ne 'x64' -and $isa -ne 'x86') {
        Die "unknown ISA '$isa' (use x64 or x86)"
    }
}
foreach ($format in $FormatList) {
    if ($format -notin @('zip', 'tar.gz', 'exe')) {
        Die "unknown format '$format' (use zip, tar.gz, exe)"
    }
}
if ($Std -ne 0 -and @(11, 14, 17, 20, 23) -notcontains $Std) {
    Die "-Std must be one of 11, 14, 17, 20, 23, got '$Std'"
}

# ---------------------------------------------------------------------------
# Tools and the installed toolsets
# ---------------------------------------------------------------------------

if (-not (Get-Command python -ErrorAction SilentlyContinue)) { Die "'python' is not in PATH" }
if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) { Die "'cmake' is not in PATH" }
if ($FormatList -contains 'tar.gz') {
    if (-not (Get-Command tar -ErrorAction SilentlyContinue)) {
        Die "tar.gz needs 'tar' in PATH (shipped with Windows 10 and later)"
    }
}

# makensis, in the order a release machine uses it: the explicit
# MAKENSIS_EXE override, PATH, the NSIS registry key, the machine and user
# PATH values from the registry (a session whose PATH is stale after
# installing NSIS), the usual installation directories and the package
# managers' locations. The found directory goes in front of PATH, so CPack
# runs the same makensis this check approved.
function Get-NSISMaker {
    if ($env:MAKENSIS_EXE) {
        if (Test-Path -LiteralPath $env:MAKENSIS_EXE -PathType Leaf) {
            return (Resolve-Path -LiteralPath $env:MAKENSIS_EXE).Path
        }
        Die "MAKENSIS_EXE points to '$env:MAKENSIS_EXE', which does not exist"
    }

    $command = Get-Command makensis -ErrorAction SilentlyContinue
    if ($command) { return $command.Source }

    foreach ($key in @('HKLM:\SOFTWARE\NSIS', 'HKLM:\SOFTWARE\WOW6432Node\NSIS', 'HKCU:\SOFTWARE\NSIS')) {
        try {
            $installDir = (Get-ItemProperty -LiteralPath $key -ErrorAction Stop).InstallDir
            if ($installDir) {
                $candidate = Join-Path $installDir 'makensis.exe'
                if (Test-Path -LiteralPath $candidate) { return $candidate }
            }
        }
        catch { }
    }

    $registeredPath = @()
    foreach ($key in @('HKLM:\SYSTEM\CurrentControlSet\Control\Session Manager\Environment', 'HKCU:\Environment')) {
        try {
            $value = (Get-ItemProperty -LiteralPath $key -ErrorAction Stop).Path
            if ($value) { $registeredPath += $value }
        }
        catch { }
    }
    foreach ($entry in (($registeredPath -join ';') -split ';')) {
        $dir = $entry.Trim()
        if (-not $dir) { continue }
        $candidate = Join-Path $dir 'makensis.exe'
        if (Test-Path -LiteralPath $candidate) { return $candidate }
    }

    $programFiles = [Environment]::GetEnvironmentVariable('ProgramFiles')
    $programFilesX86 = [Environment]::GetEnvironmentVariable('ProgramFiles(x86)')
    $userProfile = [Environment]::GetEnvironmentVariable('USERPROFILE')
    $chocolatey = [Environment]::GetEnvironmentVariable('ChocolateyInstall')
    if (-not $chocolatey) { $chocolatey = 'C:\ProgramData\chocolatey' }

    $usualDirs = @()
    if ($programFilesX86) { $usualDirs += (Join-Path $programFilesX86 'NSIS') }
    if ($programFiles) { $usualDirs += (Join-Path $programFiles 'NSIS') }
    $usualDirs += (Join-Path $chocolatey 'bin')
    $usualDirs += (Join-Path $chocolatey 'lib\nsis\tools')
    if ($userProfile) {
        $usualDirs += (Join-Path $userProfile 'scoop\shims')
        $usualDirs += (Join-Path $userProfile 'scoop\apps\nsis\current')
    }
    foreach ($dir in $usualDirs) {
        $candidate = Join-Path $dir 'makensis.exe'
        if (Test-Path -LiteralPath $candidate) { return $candidate }
    }
    return $null
}

$NSISMaker = $null
if ($FormatList -contains 'exe') {
    $NSISMaker = Get-NSISMaker
    if (-not $NSISMaker) {
        Die "exe needs NSIS ('makensis'): install NSIS, set MAKENSIS_EXE or drop exe from -Formats"
    }
    $makensisDir = (Split-Path -Parent $NSISMaker).TrimEnd('\')
    $pathEntries = @($env:PATH -split ';' | ForEach-Object { $_.Trim().TrimEnd('\') })
    if ($pathEntries -notcontains $makensisDir) {
        $env:PATH = "$makensisDir;$env:PATH"
    }
}

# Installed MSVC toolsets, found the way compile.py finds them: VC\Tools\MSVC
# under the Visual Studio roots, mapped to a toolset id by the toolkit
# version (12.x -> v120; 14.0 -> v140; < 14.20 -> v141; < 14.30 -> v142;
# < 14.40 -> v143; < 14.50 -> v144; else v145).
function Get-InstalledToolsets {
    $programFiles = [Environment]::GetEnvironmentVariable('ProgramFiles')
    if (-not $programFiles) { $programFiles = 'C:\Program Files' }
    $programFilesX86 = [Environment]::GetEnvironmentVariable('ProgramFiles(x86)')
    if (-not $programFilesX86) { $programFilesX86 = 'C:\Program Files (x86)' }
    $roots = @(
        (Join-Path $programFiles 'Microsoft Visual Studio'),
        (Join-Path $programFilesX86 'Microsoft Visual Studio')
    )

    $installed = @{}
    foreach ($root in $roots) {
        if (-not (Test-Path -LiteralPath $root)) { continue }
        foreach ($year in @(Get-ChildItem -LiteralPath $root -Directory -ErrorAction SilentlyContinue)) {
            foreach ($edition in @('Community', 'Professional', 'Enterprise', 'BuildTools')) {
                $msvcBase = Join-Path (Join-Path $year.FullName $edition) 'VC\Tools\MSVC'
                if (-not (Test-Path -LiteralPath $msvcBase)) { continue }
                foreach ($verDir in @(Get-ChildItem -LiteralPath $msvcBase -Directory -ErrorAction SilentlyContinue)) {
                    $parts = $verDir.Name.Split('.')
                    if ($parts.Count -lt 2) { continue }
                    $major = 0
                    $minor = 0
                    if (-not [int]::TryParse($parts[0], [ref] $major)) { continue }
                    if (-not [int]::TryParse($parts[1], [ref] $minor)) { continue }

                    $toolset = $null
                    if ($major -eq 12) {
                        $toolset = 'v120'
                    }
                    elseif ($major -eq 14) {
                        if ($minor -eq 0) { $toolset = 'v140' }
                        elseif ($minor -lt 20) { $toolset = 'v141' }
                        elseif ($minor -lt 30) { $toolset = 'v142' }
                        elseif ($minor -lt 40) { $toolset = 'v143' }
                        elseif ($minor -lt 50) { $toolset = 'v144' }
                        else { $toolset = 'v145' }
                    }
                    if ($toolset) {
                        if (-not $installed.ContainsKey($toolset)) { $installed[$toolset] = @() }
                        $installed[$toolset] += $verDir.FullName
                    }
                }
            }
        }
    }
    return $installed
}

# The artifact label of an MSVC toolset is msvc<year of its Visual Studio>.
function Get-MsvcLabel([string] $Toolset) {
    switch ($Toolset) {
        'v120' { return 'msvc2013' }
        'v140' { return 'msvc2015' }
        'v141' { return 'msvc2017' }
        'v142' { return 'msvc2019' }
        'v143' { return 'msvc2022' }
        'v144' { return 'msvc2025' }
        'v145' { return 'msvc2026' }
    }
    return $null
}

# clang-cl has no macro dump in CL mode (-dM is ignored); its --version line
# is stable: "clang version <major>.<minor>.<patch>".
function Get-ClangClVersion([string] $CompilerPath) {
    $output = & $CompilerPath --version 2>&1
    if ($LASTEXITCODE -ne 0) {
        Die "'$CompilerPath' does not run"
    }
    $match = [regex]::Match(($output | Out-String), 'clang version ([0-9]+(?:\.[0-9]+){1,2})')
    if (-not $match.Success) {
        Die "cannot read the clang version from '$CompilerPath'"
    }
    return $match.Groups[1].Value
}

# One job per -Compilers entry: an MSVC toolset or a clang-cl compiler.
function Resolve-CompilerJobs([string[]] $Entries) {
    $jobs = @()
    foreach ($entry in $Entries) {
        if ($KnownToolsets -contains $entry) {
            if (-not $InstalledToolsets.ContainsKey($entry)) {
                Die "toolset '$entry' is not installed (no VC\Tools\MSVC version for it under the Visual Studio roots)"
            }
            foreach ($isa in $ArchList) {
                $hasCompiler = $false
                foreach ($toolsetDir in $InstalledToolsets[$entry]) {
                    foreach ($hostDir in @('Hostx64', 'Hostx86')) {
                        if (Test-Path -LiteralPath (Join-Path $toolsetDir "bin\$hostDir\$isa\cl.exe")) {
                            $hasCompiler = $true
                        }
                    }
                }
                if (-not $hasCompiler) {
                    Die "toolset '$entry' has no $isa compiler (bin\Host*\$isa\cl.exe); install that architecture with the toolset"
                }
            }
            $jobs += [pscustomobject]@{ Kind = 'msvc'; Toolset = $entry; Compiler = ''; Label = (Get-MsvcLabel $entry) }
            continue
        }

        $compilerPath = $entry
        if ($entry -eq 'clang-cl') {
            $command = Get-Command clang-cl -ErrorAction SilentlyContinue
            if (-not $command) {
                Die "'clang-cl' is not in PATH and is not a path to clang-cl.exe"
            }
            $compilerPath = $command.Source
        }
        elseif (Test-Path -LiteralPath $entry) {
            $compilerPath = (Resolve-Path -LiteralPath $entry).Path
        }
        else {
            Die "unknown compiler '$entry': an MSVC toolset ($($KnownToolsets -join ', ')), or 'clang-cl' in PATH, or a path to clang-cl.exe"
        }
        if ((Split-Path -Leaf $compilerPath).ToLower() -notlike '*clang-cl*') {
            Die "'$entry' is neither an MSVC toolset nor a clang-cl compiler"
        }
        $jobs += [pscustomobject]@{
            Kind     = 'clang-cl'
            Toolset  = ''
            Compiler = $compilerPath
            Label    = "clang-cl-$(Get-ClangClVersion $compilerPath)"
        }
    }
    return $jobs
}

$InstalledToolsets = Get-InstalledToolsets
$Jobs = @(Resolve-CompilerJobs $CompilerList)

$needsNinja = $false
foreach ($job in $Jobs) {
    if ($job.Kind -eq 'clang-cl') { $needsNinja = $true }
}
if ($needsNinja -and -not (Get-Command ninja -ErrorAction SilentlyContinue)) {
    Die "a clang-cl build needs 'ninja' in PATH"
}

# ---------------------------------------------------------------------------
# Build and package
# ---------------------------------------------------------------------------

if ([string]::IsNullOrWhiteSpace($OutputDir)) {
    $OutputDir = Join-Path $RepoRoot 'release'
}
New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null
$OutputDir = (Resolve-Path -LiteralPath $OutputDir).Path
$WorkRoot = Join-Path $OutputDir '.work'

$Stopwatch = [System.Diagnostics.Stopwatch]::StartNew()
$Artifacts = @()

function Remove-X64 {
    # Build output published into the checkout must not reach a later package.
    $x64 = Join-Path $BuildRoot 'x64'
    if (Test-Path -LiteralPath $x64) {
        Remove-Item -LiteralPath $x64 -Recurse -Force
    }
}

function Invoke-NativeRedirected([string] $FilePath, [string[]] $ArgumentList, [string] $LogPath) {
    # Windows PowerShell turns a native command's stderr output into a
    # terminating error while ErrorActionPreference is Stop, and compile.py
    # writes its INFO lines to stderr; the preference is relaxed around the
    # call and everything still lands in the log file.
    $previous = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try {
        & $FilePath @ArgumentList *> $LogPath
        return $LASTEXITCODE
    }
    finally {
        $ErrorActionPreference = $previous
    }
}

# ---------------------------------------------------------------------------
# Tag mode: build the sources of a git tag in a detached worktree
# ---------------------------------------------------------------------------

$BuildRoot = $RepoRoot
$TagWorktree = ''
if (-not [string]::IsNullOrWhiteSpace($Tag)) {
    if ($Tag -notmatch '^[A-Za-z0-9][A-Za-z0-9._/-]*$') {
        Die "-Tag '$Tag' is not a plain tag name"
    }
    # The log and the worktree live under the work root, which the per-job
    # staging would create only later.
    New-Item -ItemType Directory -Force -Path $WorkRoot | Out-Null
    $tagLog = Join-Path $WorkRoot 'worktree.log'
    if ((Invoke-NativeRedirected 'git' @('-C', $RepoRoot, 'rev-parse', '--verify', '--quiet', "refs/tags/$Tag^{commit}") $tagLog) -ne 0) {
        Die "tag '$Tag' is not in this repository; fetch it first (git fetch --tags)"
    }
    $TagWorktree = Join-Path $WorkRoot ('src-' + ($Tag -replace '[^A-Za-z0-9._-]', '_'))
    if (Test-Path -LiteralPath $TagWorktree) {
        Die "the tag worktree path already exists: $TagWorktree (remove it with: git worktree remove --force <path>; git worktree prune)"
    }
    # An interrupted earlier run can leave a worktree registration whose
    # directory is gone; git refuses to add one on top of it.
    $null = Invoke-NativeRedirected 'git' @('-C', $RepoRoot, 'worktree', 'prune') $tagLog
    # This script drives the build; the sources, compile.py and the CMakeLists
    # come from the tag, so the packages follow the tag's project(LumexLib
    # VERSION).
    if ((Invoke-NativeRedirected 'git' @('-C', $RepoRoot, 'worktree', 'add', '--detach', $TagWorktree, "refs/tags/$Tag") $tagLog) -ne 0) {
        Die "git worktree add failed for tag '$Tag', see $tagLog"
    }
    # The submodule comes from this checkout's copy, so a tag build needs no
    # network: its pinned commit is already here. The file transport needs the
    # explicit allow since Git 2.38.1.
    $submoduleUrl = 'submodule.CMakeRoutines.url=' + ($RepoRoot -replace '\\', '/') + '/CMakeRoutines'
    if ((Invoke-NativeRedirected 'git' @('-C', $TagWorktree, '-c', 'protocol.file.allow=always', '-c', $submoduleUrl, 'submodule', 'update', '--init', '--recursive') $tagLog) -ne 0) {
        Die "git submodule update failed in the tag worktree, see $tagLog"
    }
    $BuildRoot = $TagWorktree
    Write-Host "Tag: $Tag (build tree: $BuildRoot; removed at the end unless -KeepWork)"
}

if (-not (Test-Path -LiteralPath (Join-Path $BuildRoot 'compile.py'))) {
    Die "compile.py is not in the build tree ($BuildRoot)"
}

# ---------------------------------------------------------------------------
# Version
# ---------------------------------------------------------------------------

$CmakeText = Get-Content -LiteralPath (Join-Path $BuildRoot 'CMakeLists.txt') -Raw -Encoding UTF8
$versionMatch = [regex]::Match($CmakeText, 'project\(\s*LumexLib[^)]*?VERSION\s+([0-9]+(?:\.[0-9]+){1,3})', 'Singleline')
if (-not $versionMatch.Success) {
    Die 'cannot read project(LumexLib VERSION ...) from CMakeLists.txt'
}
$CmakeVersion = $versionMatch.Groups[1].Value
$EffectiveVersion = $CmakeVersion
if (-not [string]::IsNullOrWhiteSpace($Version)) {
    $EffectiveVersion = $Version
    if ($Version -ne $CmakeVersion) {
        [Console]::Error.WriteLine("Warning: -Version $Version differs from CMakeLists.txt ($CmakeVersion); the libraries keep $CmakeVersion")
    }
}

$stdTextForBanner = if ($Std -ne 0) { "C++$Std" } else { 'C++default' }
Write-Host "LumexLib $EffectiveVersion, formats: $($FormatList -join ', ')"
if ($null -ne $NSISMaker) {
    Write-Host "  NSIS: $NSISMaker"
}
foreach ($job in $Jobs) {
    foreach ($isa in $ArchList) {
        $detail = if ($job.Kind -eq 'msvc') { $job.Toolset } else { $job.Compiler }
        Write-Host "  $($job.Label) $isa $stdTextForBanner ($detail)"
    }
}

foreach ($job in $Jobs) {
    foreach ($isa in $ArchList) {
        $base = "LumexLib-${EffectiveVersion}_win_${isa}_$($job.Label)"
        $work = Join-Path $WorkRoot "$($job.Label)_${isa}"
        if (Test-Path -LiteralPath $work) {
            Remove-Item -LiteralPath $work -Recurse -Force
        }
        New-Item -ItemType Directory -Force -Path $work | Out-Null
        $prefix = Join-Path $work 'prefix'
        New-Item -ItemType Directory -Force -Path $prefix | Out-Null

        Write-Host "== $base ($stdTextForBanner)"

        $compileArgs = @(
            'compile.py', 'Release',
            '-m', $isa,
            '--shared-libs',
            '--clean',
            '--install-prefix', $prefix
        )
        if ($job.Kind -eq 'msvc') {
            $compileArgs += @('--toolset', $job.Toolset)
        }
        else {
            $compileArgs += @('--compiler-c', $job.Compiler, '--compiler-cpp', $job.Compiler, '--use-ninja')
        }
        if ($FormatList -contains 'exe') {
            # CPack builds the NSIS installer from the same build; the compiler
            # suffix keeps installs of several compilers side by side, like
            # /opt/LumexLib/<version>_<compiler> on Linux.
            $compileArgs += '-i'
            $compileArgs += (
                '--cmake-args=-DCPACK_PACKAGE_INSTALL_DIRECTORY=LumexLib/' + $EffectiveVersion + '_' + $job.Label +
                ';-DCPACK_PACKAGE_INSTALL_REGISTRY_KEY=LumexLib_' + $EffectiveVersion + '_' + $job.Label +
                ';-DCPACK_NSIS_DISPLAY_NAME=LumexLib ' + $EffectiveVersion + ' (' + $job.Label + ')'
            )
        }
        if ($Std -ne 0) {
            $compileArgs += @('--stdcxx', $Std)
        }
        $buildLog = Join-Path $work 'build.log'
        Push-Location -LiteralPath $BuildRoot
        try {
            $buildExit = Invoke-NativeRedirected 'python' $compileArgs $buildLog
        }
        finally {
            Pop-Location
        }
        if ($buildExit -ne 0) {
            Die "build failed, see $buildLog"
        }

        $installerPath = ''
        if ($FormatList -contains 'exe') {
            $installer = @(
                Get-ChildItem -LiteralPath (Join-Path $BuildRoot 'build') -File -ErrorAction SilentlyContinue |
                    Where-Object { $_.Name -like "LumexLib-${EffectiveVersion}-win_${isa}_*_cpp*.exe" }
            )
            if ($installer.Count -ne 1) {
                Die "expected 1 NSIS installer in $RepoRoot\build, found $($installer.Count); see $buildLog"
            }
            $installerPath = $installer[0].FullName
        }

        $installedDirs = @(Get-ChildItem -LiteralPath $prefix -Directory -ErrorAction SilentlyContinue)
        if ($installedDirs.Count -ne 1) {
            Die "expected 1 install folder under $prefix, found $($installedDirs.Count); see $buildLog"
        }
        $installDir = $installedDirs[0].FullName
        if (-not (Test-Path -LiteralPath (Join-Path $installDir 'lib'))) {
            Die "the install produced no lib/ under $installDir; see $buildLog"
        }

        # One top folder per package, named after the artifact, exactly like
        # the Linux script stages its tar tree.
        $stageRoot = Join-Path $work 'stage'
        $tree = Join-Path $stageRoot $base
        if ($FormatList -contains 'zip' -or $FormatList -contains 'tar.gz') {
            New-Item -ItemType Directory -Force -Path $tree | Out-Null
            Copy-Item -Path (Join-Path $installDir '*') -Destination $tree -Recurse -Force
        }

        foreach ($format in $FormatList) {
            $out = Join-Path $OutputDir "$base.$format"
            if (Test-Path -LiteralPath $out) {
                Remove-Item -LiteralPath $out -Force
            }
            switch ($format) {
                'zip' {
                    Compress-Archive -Path $tree -DestinationPath $out -Force
                }
                'tar.gz' {
                    $tarLog = Join-Path $work 'tar.log'
                    Push-Location -LiteralPath $stageRoot
                    try {
                        $tarExit = Invoke-NativeRedirected 'tar' @('-czf', $out, $base) $tarLog
                    }
                    finally {
                        Pop-Location
                    }
                    if ($tarExit -ne 0) {
                        Die "tar failed for $out, see $tarLog"
                    }
                }
                'exe' {
                    Copy-Item -LiteralPath $installerPath -Destination $out -Force
                }
            }
            Remove-X64
            $Artifacts += $out
            Write-Host "  -> $(Split-Path -Leaf $out)"
        }

        if (-not $KeepWork) {
            Remove-Item -LiteralPath $work -Recurse -Force
        }
    }
}
$keepWorkRoot = $false
if ($TagWorktree) {
    if ($KeepWork) {
        Write-Host "Tag worktree kept (-KeepWork): $TagWorktree"
    }
    else {
        $tagRemoveLog = Join-Path $WorkRoot 'worktree-remove.log'
        $tagRemoved = (Invoke-NativeRedirected 'git' @('-C', $RepoRoot, 'worktree', 'remove', '--force', $TagWorktree) $tagRemoveLog) -eq 0
        $null = Invoke-NativeRedirected 'git' @('-C', $RepoRoot, 'worktree', 'prune') $tagRemoveLog
        if ($tagRemoved) {
            Remove-Item -LiteralPath (Join-Path $WorkRoot 'worktree.log'), $tagRemoveLog -Force -ErrorAction SilentlyContinue
        }
        else {
            # Wiping the work root would delete the worktree directory but
            # keep its registration: the next run then sees a missing but
            # registered worktree and git refuses to add it again.
            $keepWorkRoot = $true
            [Console]::Error.WriteLine("Warning: could not remove the tag worktree $TagWorktree, see $tagRemoveLog; remove it with: git -C $RepoRoot worktree remove --force $TagWorktree")
        }
    }
}
if (-not $KeepWork -and -not $keepWorkRoot -and (Test-Path -LiteralPath $WorkRoot)) {
    Remove-Item -LiteralPath $WorkRoot -Recurse -Force -ErrorAction SilentlyContinue
}

# A release needs the dated [vX.Y.Z.W] section in CHANGELOG.md; warn while
# the section is still marked as in development.
$changelog = Join-Path $BuildRoot 'CHANGELOG.md'
if (Test-Path -LiteralPath $changelog) {
    $escapedVersion = [regex]::Escape($EffectiveVersion)
    $changelogText = Get-Content -LiteralPath $changelog -Raw -Encoding UTF8
    if ($changelogText -match "(?m)^## \[v$escapedVersion\].*в разработке") {
        [Console]::Error.WriteLine("Warning: CHANGELOG.md section [v$EffectiveVersion] is still marked as in development (not dated)")
    }
}

Write-Host "Packages in ${OutputDir}:"
foreach ($artifact in $Artifacts) {
    Write-Host "  $(Split-Path -Leaf $artifact)"
}
$elapsed = $Stopwatch.Elapsed
Write-Host ("Total time: {0:00}h {1:00}m {2:00}s" -f [int]$elapsed.TotalHours, $elapsed.Minutes, $elapsed.Seconds)



