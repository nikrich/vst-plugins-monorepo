Param(
  [string]$BundleVersion,
  [string]$Manufacturer = 'Hungry Ghost'
)

$ErrorActionPreference = 'Stop'
$Root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$Templates = Join-Path $PSScriptRoot '..\templates'
$DistDir = Join-Path $Root 'dist'
$SharedJson = Join-Path $Root '..\shared\plugins.json'

$meta = Get-Content $SharedJson -Raw | ConvertFrom-Json
$bundleUpgrade = $meta.bundle.win.upgradeCode
$license = Join-Path (Join-Path $Root '..\shared') 'EULA.rtf'

if (-not $BundleVersion) {
  $BundleVersion = '0.1.0'
}

# find MSIs
$limiterMsi = Get-ChildItem -Path $DistDir -Filter 'HungryGhostSpectralLimiter-*-win-x64.msi' | Select-Object -First 1
$astralMsi = Get-ChildItem -Path $DistDir -Filter 'HungryGhostAstralHalls-*-win-x64.msi' | Select-Object -First 1
if (-not $limiterMsi -or -not $astralMsi) { throw "Per-plugin MSIs not found in $DistDir" }

$wxs = Join-Path $Templates 'bundle.wxs'
$wixobj = Join-Path $DistDir 'bundle.wixobj'
$outExe = Join-Path $DistDir ("HungryGhostPlugins-{0}-win-x64.exe" -f $BundleVersion)

& candle.exe -ext WixBalExtension -dBundleVersion=$BundleVersion -dBundleUpgradeCode=$bundleUpgrade -dLimiterMsiPath=$($limiterMsi.FullName) -dAstralMsiPath=$($astralMsi.FullName) -dLicenseRtf=$license -out $wixobj $wxs
if ($LASTEXITCODE -ne 0) { throw "candle.exe bundle.wxs failed" }

& light.exe -ext WixBalExtension -out $outExe $wixobj
if ($LASTEXITCODE -ne 0) { throw "light.exe failed" }

if ($env:SIGN_WINDOWS -eq '1') {
  $sign = Join-Path $PSScriptRoot 'sign.ps1'
  & $sign -FilePath $outExe
}

Write-Host "Built: $outExe"