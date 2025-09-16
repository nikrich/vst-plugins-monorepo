Param(
  [Parameter(Mandatory=$true)][string]$FilePath
)
$ErrorActionPreference = 'Stop'

if (-not $env:WINDOWS_SIGNING_CERT_BASE64 -or -not $env:WINDOWS_SIGNING_CERT_PASSWORD -or -not $env:WINDOWS_SIGNING_TIMESTAMP_URL) {
  Write-Host "Signing skipped: missing env vars. Required: WINDOWS_SIGNING_CERT_BASE64, WINDOWS_SIGNING_CERT_PASSWORD, WINDOWS_SIGNING_TIMESTAMP_URL"
  exit 0
}

$certPath = Join-Path $env:TEMP 'codesign.pfx'
[System.IO.File]::WriteAllBytes($certPath, [Convert]::FromBase64String($env:WINDOWS_SIGNING_CERT_BASE64))

& signtool.exe sign /fd sha256 /tr $env:WINDOWS_SIGNING_TIMESTAMP_URL /td sha256 /f $certPath /p $env:WINDOWS_SIGNING_CERT_PASSWORD $FilePath
if ($LASTEXITCODE -ne 0) { throw "signtool sign failed" }