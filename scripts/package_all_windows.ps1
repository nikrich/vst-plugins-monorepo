Param(
  [string]$Version,
  [string[]]$Plugins = @('limiter','astral')
)

$ErrorActionPreference = 'Stop'
$Root = Split-Path -Parent $PSScriptRoot

function Build-Plugin($subdir, $target) {
  & cmake -S (Join-Path $Root $subdir) -B (Join-Path $Root "$subdir/build/vs") -G "Visual Studio 17 2022" -A x64
  if ($LASTEXITCODE -ne 0) { throw "cmake configure failed for $subdir" }
  & cmake --build (Join-Path $Root "$subdir/build/vs") --config Release --target $target
  if ($LASTEXITCODE -ne 0) { throw "cmake build failed for $subdir" }
}

# Build Release VST3s
Build-Plugin 'src/HungryGhostLimiter' 'HungryGhostLimiter_VST3'
Build-Plugin 'src/HungryGhostReverb' 'HungryGhostReverb_VST3'

# Ensure WiX is available
if (-not (Get-Command candle.exe -ErrorAction SilentlyContinue)) {
  Write-Host 'WiX candle.exe not found on PATH. Install WiX Toolset 3.x and reopen shell.'
  exit 1
}

# Package MSIs
$pkgRoot = Join-Path $Root 'packaging\windows\scripts'
foreach ($p in $Plugins) {
  & (Join-Path $pkgRoot 'build_msi.ps1') -PluginId $p -Version $Version
}

# Build bundle
& (Join-Path $pkgRoot 'build_bundle.ps1') -BundleVersion $Version