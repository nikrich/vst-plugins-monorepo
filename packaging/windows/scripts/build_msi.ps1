Param(
  [Parameter(Mandatory=$true)][ValidateSet('limiter','astral')] [string]$PluginId,
  [string]$Version,
  [string]$Manufacturer = 'Hungry Ghost'
)

$ErrorActionPreference = 'Stop'
$Root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$SharedJson = Join-Path $Root 'shared\plugins.json'
$Templates = Join-Path $PSScriptRoot '..\templates'
$ObjDir = Join-Path $Root 'obj'
$DistDir = Join-Path $Root 'dist'
New-Item -ItemType Directory -Force -Path $ObjDir, $DistDir | Out-Null

# Read metadata
$meta = Get-Content $SharedJson -Raw | ConvertFrom-Json
$plugin = $meta.plugins | Where-Object { $_.id -eq $PluginId }
if (-not $plugin) { throw "Unknown pluginId: $PluginId" }
$display = $plugin.displayName
$srcDir = Join-Path (Split-Path -Parent $Root) $plugin.srcDir
$upgradeCode = $plugin.win.upgradeCode
$license = Join-Path (Join-Path $Root '..\shared') 'EULA.rtf'

# Resolve version
if (-not $Version) {
  $cmake = Join-Path $srcDir 'CMakeLists.txt'
  if (Test-Path $cmake) {
    $match = Select-String -Path $cmake -Pattern 'project\([^\)]*VERSION ([0-9]+\.[0-9]+\.[0-9]+)' | Select-Object -First 1
    if ($match) { $Version = $match.Matches[0].Groups[1].Value }
  }
  if (-not $Version) { $Version = '0.1.0' }
}

# Locate built VST3 directory
function Find-Vst3Dir {
  param([string]$Base)
  $glob = Get-ChildItem -Path (Join-Path $Base '*_artefacts/Release/VST3/*.vst3') -Directory -Recurse -ErrorAction SilentlyContinue | Select-Object -First 1
  if ($glob) { return $glob.FullName }
  $glob = Get-ChildItem -Path (Join-Path $srcDir 'build') -Recurse -Include *.vst3 -Directory -ErrorAction SilentlyContinue | Where-Object { $_.FullName -match 'Release' } | Select-Object -First 1
  if ($glob) { return $glob.FullName }
  return $null
}

$vst3Dir = Find-Vst3Dir -Base (Join-Path $srcDir 'build')
if (-not $vst3Dir) { throw "Could not find built .vst3 directory for $display. Build Release first." }

# Harvest payload
$pluginObj = Join-Path $ObjDir $PluginId
New-Item -ItemType Directory -Force -Path $pluginObj | Out-Null
$fragWxs = Join-Path $pluginObj 'plugin_frag.wxs'
& heat.exe dir $vst3Dir -cg PluginComponents -gg -srd -dr VST3DIR -var var.PluginPayloadDir -out $fragWxs | Write-Verbose
if ($LASTEXITCODE -ne 0) { throw "heat.exe failed" }

# Prepare candle inputs
$wxs = Join-Path $Templates 'plugin.msi.wxs'
$wixobj1 = Join-Path $pluginObj 'plugin.wixobj'
$wixobj2 = Join-Path $pluginObj 'plugin_frag.wixobj'
& candle.exe -arch x64 -ext WixUIExtension -dProductName="$display" -dProductVersion="$Version" -dManufacturer="$Manufacturer" -dUpgradeCode="$upgradeCode" -dPluginPayloadDir=(Split-Path -Parent $vst3Dir) -dLicenseRtf=$license -out $wixobj1 $wxs
if ($LASTEXITCODE -ne 0) { throw "candle.exe plugin.msi.wxs failed" }
& candle.exe -arch x64 -ext WixUIExtension -dPluginPayloadDir=(Split-Path -Parent $vst3Dir) -out $wixobj2 $fragWxs
if ($LASTEXITCODE -ne 0) { throw "candle.exe plugin_frag.wxs failed" }

# Link MSI
$outMsi = Join-Path $DistDir ("{0}-{1}-win-x64.msi" -f ($display -replace ' ', ''), $Version)
& light.exe -ext WixUIExtension -cultures:en-us -out $outMsi $wixobj1 $wixobj2
if ($LASTEXITCODE -ne 0) { throw "light.exe failed" }

# Optional sign
if ($env:SIGN_WINDOWS -eq '1') {
  $sign = Join-Path $PSScriptRoot 'sign.ps1'
  & $sign -FilePath $outMsi
}

Write-Host "Built: $outMsi"