<#
.SYNOPSIS
    Builds the BeSwarm Vehicle GS Windows portable ZIP and its SHA-256 file.

.DESCRIPTION
    Builds the Windows Release preset (unless -SkipBuild), installs the Runtime component into
    <DistDir>\<Name>, boot-tests the installed executable with no Qt or GStreamer on the PATH, zips it and writes
    <DistDir>\<Name>-win64-portable.zip plus the matching .sha256 (sha256sum format).

.PARAMETER RepoDir
    Repository (or worktree) root to build. Defaults to the root of the repository holding this script.

.PARAMETER BuildDir
    CMake build directory. Defaults to <RepoDir>\build\Windows.

.PARAMETER DistDir
    Output directory. Defaults to C:\gits\qgc-dist.

.PARAMETER Name
    Base name of the staging folder and the ZIP. Defaults to BeSwarmVehicleGS.

.PARAMETER QtRoot
    Qt installation used for the build and the install step. Defaults to C:\Qt\6.12.0\msvc2022_64.

.PARAMETER SkipBuild
    Installs from an existing build without configuring or building.
#>
[CmdletBinding()]
param(
    [string]$RepoDir,
    [string]$BuildDir,
    [string]$DistDir = 'C:\gits\qgc-dist',
    [ValidatePattern('^[A-Za-z0-9_.-]+$')]
    [string]$Name = 'BeSwarmVehicleGS',
    [string]$QtRoot = 'C:\Qt\6.12.0\msvc2022_64',
    [switch]$SkipBuild
)

$ErrorActionPreference = 'Stop'

# Native commands write progress to stderr, which Stop turns into a fatal error when the output is redirected
# (PowerShell 5.1), so only the exit code decides success.
function Invoke-Native {
    param([Parameter(Mandatory)][scriptblock]$Command)

    $savedPreference = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    $global:LASTEXITCODE = 0
    try {
        & $Command
        $exitCode = $LASTEXITCODE
    } finally {
        $ErrorActionPreference = $savedPreference
    }
    if ($exitCode -ne 0) {
        throw "Command failed with exit code ${exitCode}: $Command"
    }
}

function Resolve-FullPath {
    param([Parameter(Mandatory)][string]$Path)

    return $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($Path)
}

# $PSScriptRoot is empty inside the param block on PowerShell 5.1, so the defaults are resolved here
if (-not $RepoDir) {
    $RepoDir = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
}
$RepoDir = Resolve-FullPath $RepoDir
if (-not $BuildDir) {
    $BuildDir = Join-Path $RepoDir 'build\Windows'
}
$BuildDir = Resolve-FullPath $BuildDir
$DistDir = Resolve-FullPath $DistDir

if (-not (Test-Path -LiteralPath $RepoDir -PathType Container)) {
    throw "Repository directory not found: $RepoDir"
}
if (-not (Test-Path -LiteralPath $QtRoot -PathType Container)) {
    throw "Qt installation not found: $QtRoot"
}

# Build environment: VS developer shell (cmake, Ninja, MSVC), Qt, shared CPM cache and tools venv
if ($env:VSCMD_ARG_TGT_ARCH -ne 'x64') {
    $vsInstallPath = 'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools'
    if (-not (Test-Path -LiteralPath $vsInstallPath -PathType Container)) {
        throw "Visual Studio Build Tools not found: $vsInstallPath"
    }
    Import-Module "$vsInstallPath\Common7\Tools\Microsoft.VisualStudio.DevShell.dll"
    Enter-VsDevShell -VsInstallPath $vsInstallPath -DevCmdArguments '-arch=x64 -host_arch=x64' `
        -SkipAutomaticLocation | Out-Null
}
$env:QT_ROOT_DIR = $QtRoot
if (-not $env:CPM_SOURCE_CACHE) {
    $env:CPM_SOURCE_CACHE = Join-Path $RepoDir '.cache\CPM'
}
# Worktrees have no tools\.venv of their own
if (-not $env:QGC_PYTHON_ENV -and -not (Test-Path -LiteralPath (Join-Path $RepoDir 'tools\.venv'))) {
    $env:QGC_PYTHON_ENV = 'C:\gits\qgroundcontrol\tools\.venv'
}

Push-Location -LiteralPath $RepoDir
try {
    if (-not $SkipBuild) {
        if (-not (Test-Path -LiteralPath (Join-Path $BuildDir 'CMakeCache.txt'))) {
            Invoke-Native { cmake --preset Windows }
        }
        Invoke-Native { cmake --build $BuildDir }
    }
    if (-not (Test-Path -LiteralPath (Join-Path $BuildDir 'CMakeCache.txt'))) {
        throw "No CMake build found in $BuildDir"
    }

    New-Item -ItemType Directory -Force -Path $DistDir | Out-Null
    $staging = Join-Path $DistDir $Name
    if (Test-Path -LiteralPath $staging) {
        Remove-Item -LiteralPath $staging -Recurse -Force
    }
    Invoke-Native { cmake --install $BuildDir --config Release --component Runtime --prefix $staging }

    # The package must start without Qt or GStreamer installed
    $stagedExe = Join-Path $staging 'bin\QGroundControl.exe'
    if (-not (Test-Path -LiteralPath $stagedExe)) {
        throw "Installed executable not found: $stagedExe"
    }
    $savedPath = $env:Path
    $env:Path = ($env:Path -split ';' |
            Where-Object { $_ -and ($_ -notlike "$QtRoot*") -and ($_ -notmatch 'gstreamer') }) -join ';'
    try {
        $bootTest = Start-Process -FilePath $stagedExe -ArgumentList '--simple-boot-test', '--allow-multiple' `
            -Wait -PassThru
    } finally {
        $env:Path = $savedPath
    }
    if ($bootTest.ExitCode -ne 0) {
        throw "Boot test of $stagedExe failed with exit code $($bootTest.ExitCode)"
    }

    $zipName = "$Name-win64-portable.zip"
    $zip = Join-Path $DistDir $zipName
    Remove-Item -LiteralPath $zip, "$zip.sha256" -Force -ErrorAction SilentlyContinue
    Compress-Archive -Path $staging -DestinationPath $zip -Force

    $hash = (Get-FileHash -Algorithm SHA256 -LiteralPath $zip).Hash.ToLowerInvariant()
    [IO.File]::WriteAllText("$zip.sha256", "$hash  $zipName`n")

    $sizeMb = [math]::Round((Get-Item -LiteralPath $zip).Length / 1MB, 1)
    Write-Host "Folder : $staging"
    Write-Host "ZIP    : $zip"
    Write-Host "Size   : $sizeMb MB"
    Write-Host "SHA-256: $hash"
} finally {
    Pop-Location
}
