<#
.SYNOPSIS
  Nexus Windows dev wrapper: loads the VS 2022 x64 environment, sets VCPKG_ROOT
  and QT_ROOT, then forwards to cmake / ctest.

.EXAMPLE
  ./scripts/dev.ps1 configure debug
  ./scripts/dev.ps1 build debug
  ./scripts/dev.ps1 test debug
  ./scripts/dev.ps1 all asan        # configure + build + test
  ./scripts/dev.ps1 clean debug     # delete build/<preset>
  ./scripts/dev.ps1 shell           # print resolved environment

.NOTES
  Overridable environment variables (set before calling):
    NEXUS_VS_ROOT  VS 2022 install dir (default: found via vswhere)
    VCPKG_ROOT     vcpkg dir          (default: <VS>\VC\vcpkg)
    QT_ROOT        Qt kit dir         (default: C:\Qt\6.8.3\msvc2022_64)
  Extra arguments after the preset are passed through to the underlying tool,
  e.g. ./scripts/dev.ps1 test debug -R Smoke
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true, Position = 0)]
    [ValidateSet('configure', 'build', 'test', 'all', 'clean', 'shell')]
    [string]$Command,

    [Parameter(Position = 1)]
    [string]$Preset = 'debug',

    [Parameter(ValueFromRemainingArguments = $true)]
    [string[]]$Rest
)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot

function Find-VsRoot {
    if ($env:NEXUS_VS_ROOT) { return $env:NEXUS_VS_ROOT }
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (Test-Path $vswhere) {
        $p = & $vswhere -latest -products '*' -version '[17.0,18.0)' `
            -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
            -property installationPath | Select-Object -First 1
        if ($p) { return $p }
    }
    $fallback = 'C:\Program Files\Microsoft Visual Studio\2022\Community'
    if (Test-Path $fallback) { return $fallback }
    throw 'Visual Studio 2022 with the C++ workload was not found. Set NEXUS_VS_ROOT.'
}

function Import-VsEnvironment([string]$vsRoot) {
    # Already inside a developer environment for x64? Nothing to do.
    if ($env:VSCMD_ARG_TGT_ARCH -eq 'x64' -and (Get-Command cl.exe -ErrorAction SilentlyContinue)) { return }
    $vcvars = Join-Path $vsRoot 'VC\Auxiliary\Build\vcvars64.bat'
    if (-not (Test-Path $vcvars)) { throw "vcvars64.bat not found: $vcvars" }
    Write-Host "[dev] loading VS environment: $vcvars"
    $dump = & cmd.exe /c "`"$vcvars`" >nul 2>&1 && set"
    if ($LASTEXITCODE -ne 0) { throw 'vcvars64.bat failed' }
    foreach ($line in $dump) {
        $i = $line.IndexOf('=')
        if ($i -gt 0) {
            [Environment]::SetEnvironmentVariable($line.Substring(0, $i), $line.Substring($i + 1), 'Process')
        }
    }
}

function Ensure-PkgConfig {
    # vcpkg (VS-bundled 2024-09 scripts) downloads pkgconf from an MSYS2 mirror URL that
    # has since been removed (404), which breaks ports such as gtest. vcpkg honours
    # PKG_CONFIG, so provide a pkgconf ourselves once and cache it in %LOCALAPPDATA%.
    if ($env:PKG_CONFIG -and (Test-Path $env:PKG_CONFIG)) { return }
    $existing = Get-Command pkg-config.exe -ErrorAction SilentlyContinue
    if ($existing) { $env:PKG_CONFIG = $existing.Source; return }
    $dir = Join-Path $env:LOCALAPPDATA 'nexus\tools\pkgconf'
    $exe = Join-Path $dir 'mingw64\bin\pkg-config.exe'
    if (-not (Test-Path $exe)) {
        Write-Host '[dev] provisioning pkgconf (one-time, from repo.msys2.org)'
        [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
        New-Item -ItemType Directory -Force $dir | Out-Null
        $base = 'https://repo.msys2.org/mingw/mingw64/'
        $name = 'mingw-w64-x86_64-pkgconf-1~2.5.1-2-any.pkg.tar.zst'
        try { Invoke-WebRequest -UseBasicParsing "$base$name" -OutFile (Join-Path $dir $name) }
        catch {
            # Pinned file is gone: fall back to the newest one listed.
            $html = (Invoke-WebRequest -UseBasicParsing $base).Content
            $m = [regex]::Matches($html, 'mingw-w64-x86_64-pkgconf-1~([0-9.]+)-([0-9]+)-any\.pkg\.tar\.zst') |
                Sort-Object { [version]("$($_.Groups[1].Value).$($_.Groups[2].Value)") } |
                Select-Object -Last 1
            if (-not $m) { throw 'could not find a pkgconf package on repo.msys2.org' }
            $name = $m.Value
            Invoke-WebRequest -UseBasicParsing "$base$name" -OutFile (Join-Path $dir $name)
        }
        Push-Location $dir
        try { & cmake.exe -E tar xf $name; if ($LASTEXITCODE -ne 0) { throw 'failed to extract pkgconf' } }
        finally { Pop-Location }
        if (-not (Test-Path $exe)) { throw "pkg-config.exe not found after extraction: $exe" }
    }
    $env:PKG_CONFIG = $exe
}

$vsRoot = Find-VsRoot
Import-VsEnvironment $vsRoot

if (-not $env:VCPKG_ROOT) { $env:VCPKG_ROOT = Join-Path $vsRoot 'VC\vcpkg' }
if (-not $env:QT_ROOT) { $env:QT_ROOT = 'C:\Qt\6.8.3\msvc2022_64' }
# The VS-bundled vcpkg lives under Program Files (not writable without admin),
# so keep its download / registry caches in the user profile.
if (-not $env:VCPKG_DOWNLOADS) { $env:VCPKG_DOWNLOADS = Join-Path $env:LOCALAPPDATA 'vcpkg\downloads' }
if (-not $env:VCPKG_DISABLE_METRICS) { $env:VCPKG_DISABLE_METRICS = '1' }

if (-not (Test-Path (Join-Path $env:VCPKG_ROOT 'scripts\buildsystems\vcpkg.cmake'))) {
    throw "VCPKG_ROOT is invalid (vcpkg.cmake not found): $env:VCPKG_ROOT"
}
if (-not (Test-Path (Join-Path $env:QT_ROOT 'lib\cmake\Qt6'))) {
    throw "QT_ROOT is invalid (lib\cmake\Qt6 not found): $env:QT_ROOT"
}

Ensure-PkgConfig
# vcpkg scrubs the environment of port builds; let PKG_CONFIG through.
if (-not $env:VCPKG_KEEP_ENV_VARS) { $env:VCPKG_KEEP_ENV_VARS = 'PKG_CONFIG' }

# Guarantee that Qt DLLs are resolvable by tools launched from here as well.
$qtBin = Join-Path $env:QT_ROOT 'bin'
if (($env:PATH -split ';') -notcontains $qtBin) { $env:PATH = "$qtBin;$env:PATH" }

function Invoke-Native([string]$exe, [string[]]$arguments) {
    Write-Host "[dev] $exe $($arguments -join ' ')"
    & $exe @arguments
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

Push-Location $repoRoot
try {
    switch ($Command) {
        'shell' {
            Write-Host "VS_ROOT    = $vsRoot"
            Write-Host "VCPKG_ROOT = $env:VCPKG_ROOT"
            Write-Host "QT_ROOT    = $env:QT_ROOT"
            Write-Host "cl         = $((Get-Command cl.exe).Source)"
            Write-Host "cmake      = $((Get-Command cmake.exe).Source)"
            Write-Host "ninja      = $((Get-Command ninja.exe).Source)"
        }
        'configure' { Invoke-Native 'cmake' (@('--preset', $Preset) + $Rest) }
        'build' { Invoke-Native 'cmake' (@('--build', '--preset', $Preset) + $Rest) }
        'test' { Invoke-Native 'ctest' (@('--preset', $Preset) + $Rest) }
        'all' {
            Invoke-Native 'cmake' (@('--preset', $Preset))
            Invoke-Native 'cmake' (@('--build', '--preset', $Preset))
            Invoke-Native 'ctest' (@('--preset', $Preset))
        }
        'clean' {
            $dir = Join-Path $repoRoot "build\$Preset"
            if (Test-Path $dir) { Remove-Item -Recurse -Force $dir; Write-Host "[dev] removed $dir" }
        }
    }
}
finally {
    Pop-Location
}
