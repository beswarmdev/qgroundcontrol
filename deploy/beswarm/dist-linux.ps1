<#
.SYNOPSIS
Builds the BeSwarm Vehicle GS AppImage in WSL and stores it, with its .sha256, in the distribution folder.

.DESCRIPTION
Resolves the commit with the Windows git, runs deploy/beswarm/build-appimage.sh inside the WSL distro (clone, build,
AppImage and boot tests all happen there) and writes the .sha256 on the Windows side.
Artifact: <DistDir>\<Name>-linux-x86_64.AppImage (+ .sha256).

.PARAMETER Ref
Git ref to build. Defaults to the HEAD of the checkout or worktree that holds this script.

.PARAMETER DistDir
Distribution folder. Defaults to C:\gits\qgc-dist.

.PARAMETER Name
Artifact base name. Defaults to BeSwarmVehicleGS.

.PARAMETER Jobs
Parallel build jobs inside WSL. Defaults to the logical processor count minus 2.

.PARAMETER Distro
WSL distribution that builds the AppImage. Defaults to Ubuntu-26.04.

.PARAMETER SkipBuild
Reuse the existing build tree in WSL and only repackage the AppImage.
#>
[CmdletBinding()]
param(
    [string]$Ref = 'HEAD',
    [string]$DistDir = 'C:\gits\qgc-dist',
    [ValidatePattern('^(?!\.+$)[A-Za-z0-9_.-]+$')]
    [string]$Name = 'BeSwarmVehicleGS',
    [int]$Jobs = [Math]::Max(1, [Environment]::ProcessorCount - 2),
    [string]$Distro = 'Ubuntu-26.04',
    [switch]$SkipBuild
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

# PowerShell 5.1 turns native stderr (git/cmake/wsl progress) into fatal errors under 'Stop' once the output is
# redirected, so native commands run with 'Continue' and are judged by their exit code only.
function Invoke-Native([scriptblock]$Command) {
    $previous = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try {
        $global:LASTEXITCODE = 0
        & $Command
        if ($LASTEXITCODE -ne 0) {
            throw "Command failed with exit code ${LASTEXITCODE}: $Command"
        }
    }
    finally {
        $ErrorActionPreference = $previous
    }
}

function ConvertTo-WslPath([string]$Path) {
    if ($Path -notmatch '^[A-Za-z]:[\\/]') {
        throw "Not an absolute drive-letter path: $Path"
    }
    $full = [IO.Path]::GetFullPath($Path)
    '/mnt/' + $full.Substring(0, 1).ToLowerInvariant() + '/' + $full.Substring(3).Replace('\', '/')
}

$sha = ([string](Invoke-Native { git -C $PSScriptRoot rev-parse --verify "${Ref}^{commit}" })).Trim()

# The main repository holds the commits of every branch, even when this script runs from a worktree.
# Git 2.30 has no --path-format=absolute, and a relative result is relative to the -C directory.
$commonDir = ([string](Invoke-Native { git -C $PSScriptRoot rev-parse --git-common-dir })).Trim()
$commonDir = [IO.Path]::GetFullPath([IO.Path]::Combine($PSScriptRoot, $commonDir))
$repoDir = Split-Path -Parent $commonDir

New-Item -ItemType Directory -Force -Path $DistDir | Out-Null
$DistDir = (Resolve-Path -LiteralPath $DistDir).ProviderPath
$artifactName = "$Name-linux-x86_64.AppImage"
$out = Join-Path $DistDir $artifactName
# Build into a side file so a failed build leaves the previous AppImage in place
$partial = "$out.partial"
if (Test-Path -LiteralPath $partial) {
    Remove-Item -LiteralPath $partial -Force
}

$wslArgs = @(
    '-d', $Distro, '--cd', '~', '--',
    'bash', '-l', (ConvertTo-WslPath (Join-Path $PSScriptRoot 'build-appimage.sh')),
    '--origin', (ConvertTo-WslPath $repoDir),
    '--sha', $sha,
    '--jobs', $Jobs,
    '--out', (ConvertTo-WslPath $partial)
)
if ($SkipBuild) {
    $wslArgs += '--skip-build'
}
Invoke-Native { & wsl.exe @wslArgs }

if (-not (Test-Path -LiteralPath $partial)) {
    throw "AppImage was not produced: $partial"
}
Move-Item -LiteralPath $partial -Destination $out -Force
$hash = (Get-FileHash -Algorithm SHA256 -LiteralPath $out).Hash.ToLowerInvariant()
[IO.File]::WriteAllText("$out.sha256", "$hash  $artifactName`n")

$sizeMb = [Math]::Round((Get-Item -LiteralPath $out).Length / 1MB, 1)
Write-Host "Commit : $sha"
Write-Host "File   : $out"
Write-Host "Size   : $sizeMb MB"
Write-Host "SHA-256: $hash"
