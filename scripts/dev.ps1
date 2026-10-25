<#
.SYNOPSIS
  Nexus Windows dev wrapper: loads the VS 2022 x64 environment, provisions the
  pinned vcpkg, sets VCPKG_ROOT and QT_ROOT, then forwards to cmake / ctest.

.EXAMPLE
  ./scripts/dev.ps1 configure debug
  ./scripts/dev.ps1 build debug
  ./scripts/dev.ps1 test debug
  ./scripts/dev.ps1 all asan        # configure + build + test
  ./scripts/dev.ps1 clean debug     # delete build/<preset>
  ./scripts/dev.ps1 shell           # print resolved environment
  ./scripts/dev.ps1 format          # clang-format -i on all C++ sources
  ./scripts/dev.ps1 format-check    # fail if any C++ source is not formatted
  ./scripts/dev.ps1 configure tidy  # preset with clang-tidy enabled (zero warnings required)

.NOTES
  Overridable environment variables (set before calling):
    NEXUS_VS_ROOT  VS 2022 install dir (default: found via vswhere)
    VCPKG_ROOT     vcpkg dir          (default: clone managed in %LOCALAPPDATA%/nexus/vcpkg,
                                       pinned to the builtin-baseline in vcpkg.json)
    NEXUS_LLVM_BIN LLVM tools dir     (default: <VS>/VC/Tools/Llvm/x64/bin; the 32-bit one
                                       in .../Llvm/bin crashes clang-tidy)
    QT_ROOT        Qt kit dir         (default: C:\Qt\6.8.3\msvc2022_64)
  Extra arguments after the preset are passed through to the underlying tool,
  e.g. ./scripts/dev.ps1 test debug -R Smoke
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true, Position = 0)]
    [ValidateSet('configure', 'build', 'test', 'all', 'clean', 'shell', 'format', 'format-check')]
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

function Invoke-Git([string]$workDir, [string[]]$gitArgs) {
    & git -C $workDir @gitArgs
    if ($LASTEXITCODE -ne 0) { throw "git $($gitArgs -join ' ') failed (exit $LASTEXITCODE)" }
}

# Standalone vcpkg pinned to vcpkg.json "builtin-baseline" (ADR 0003): cloned into
# %LOCALAPPDATA%\nexus\vcpkg, checked out at the baseline commit and bootstrapped
# on first use / whenever the baseline changes. vcpkg itself verifies the SHA512
# of everything it downloads (vcpkg.exe, tools, sources). If VCPKG_ROOT is
# already set by the caller it is used as-is and not managed here. (vcvars64.bat
# itself exports VCPKG_ROOT pointing at the VS-bundled vcpkg, so the caller's value
# is captured before the VS environment is imported and a VS-owned one is ignored.)
function Initialize-Vcpkg([string]$callerVcpkgRoot, [string]$vsRoot) {
    if ($callerVcpkgRoot -and -not $callerVcpkgRoot.StartsWith($vsRoot, [StringComparison]::OrdinalIgnoreCase)) {
        $env:VCPKG_ROOT = $callerVcpkgRoot
        return
    }
    $manifest = Get-Content -Raw (Join-Path $repoRoot 'vcpkg.json') | ConvertFrom-Json
    $baseline = $manifest.'builtin-baseline'
    if ($baseline -notmatch '^[0-9a-f]{40}$') { throw "vcpkg.json: invalid builtin-baseline '$baseline'" }

    $root = Join-Path $env:LOCALAPPDATA 'nexus\vcpkg'
    if (-not (Test-Path (Join-Path $root '.git'))) {
        Write-Host "[dev] cloning vcpkg into $root (one-time)"
        New-Item -ItemType Directory -Force (Split-Path -Parent $root) | Out-Null
        Invoke-Git $repoRoot @('clone', 'https://github.com/microsoft/vcpkg.git', $root)
    }
    $head = (& git -C $root rev-parse HEAD).Trim()
    if ($head -ne $baseline) {
        Write-Host "[dev] checking out vcpkg baseline $baseline"
        & git -C $root cat-file -e "$baseline^{commit}" 2>$null
        if ($LASTEXITCODE -ne 0) { Invoke-Git $root @('fetch', 'origin') }
        Invoke-Git $root @('checkout', '--quiet', '--detach', $baseline)
    }
    $stamp = Join-Path $root '.nexus-bootstrapped'
    $stampOk = (Test-Path $stamp) -and ((Get-Content $stamp -ErrorAction SilentlyContinue) -eq $baseline) -and
        (Test-Path (Join-Path $root 'vcpkg.exe'))
    if (-not $stampOk) {
        Write-Host '[dev] bootstrapping vcpkg'
        & cmd.exe /c "`"$(Join-Path $root 'bootstrap-vcpkg.bat')`" -disableMetrics"
        if ($LASTEXITCODE -ne 0) { throw 'bootstrap-vcpkg.bat failed' }
        Set-Content -Path $stamp -Value $baseline
    }
    $env:VCPKG_ROOT = $root
}

$vsRoot = Find-VsRoot
$callerVcpkgRoot = $env:VCPKG_ROOT
Import-VsEnvironment $vsRoot

Initialize-Vcpkg $callerVcpkgRoot $vsRoot
if (-not $env:QT_ROOT) { $env:QT_ROOT = 'C:\Qt\6.8.3\msvc2022_64' }
if (-not $env:NEXUS_LLVM_BIN) { $env:NEXUS_LLVM_BIN = Join-Path $vsRoot 'VC/Tools/Llvm/x64/bin' }
if (-not $env:VCPKG_DISABLE_METRICS) { $env:VCPKG_DISABLE_METRICS = '1' }

if (-not (Test-Path (Join-Path $env:VCPKG_ROOT 'scripts\buildsystems\vcpkg.cmake'))) {
    throw "VCPKG_ROOT is invalid (vcpkg.cmake not found): $env:VCPKG_ROOT"
}
if (-not (Test-Path (Join-Path $env:QT_ROOT 'lib\cmake\Qt6'))) {
    throw "QT_ROOT is invalid (lib\cmake\Qt6 not found): $env:QT_ROOT"
}

# Guarantee that Qt DLLs are resolvable by tools launched from here as well.
$qtBin = Join-Path $env:QT_ROOT 'bin'
if (($env:PATH -split ';') -notcontains $qtBin) { $env:PATH = "$qtBin;$env:PATH" }

function Invoke-Native([string]$exe, [string[]]$arguments) {
    Write-Host "[dev] $exe $($arguments -join ' ')"
    & $exe @arguments
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

function Invoke-ClangFormat([bool]$checkOnly) {
    $cf = Join-Path $env:NEXUS_LLVM_BIN 'clang-format.exe'
    if (-not (Test-Path $cf)) { throw "clang-format not found: $cf" }
    $files = & git ls-files '*.cpp' '*.h' '*.hpp' '*.cc'
    if (-not $files) { return }
    if ($checkOnly) { Invoke-Native $cf (@('--dry-run', '-Werror') + $files) }
    else { Invoke-Native $cf (@('-i') + $files) }
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
        'format' { Invoke-ClangFormat $false }
        'format-check' { Invoke-ClangFormat $true }
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
