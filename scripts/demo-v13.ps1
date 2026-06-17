# KamikazeOS v1.3 public demo helper.
#
# This script prepares the host-side demo state and prints the commands to run
# inside KamikazeOS. It does not automate shell input inside QEMU.
$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$python = (Get-Command python -ErrorAction SilentlyContinue).Source
if (-not $python) { $python = 'python' }

Write-Host '== KamikazeOS v1.3 demo prep ==' -ForegroundColor Cyan

& (Join-Path $PSScriptRoot 'build.ps1')

Write-Host ''
Write-Host 'Validating safe package PKG-101' -ForegroundColor Cyan
& $python (Join-Path $root 'tools\package-builder\validate_manifest.py') `
  (Join-Path $root 'packages\examples\PKG-101-ghostattach-policy.json')
if ($LASTEXITCODE -ne 0) { throw 'PKG-101 should validate for the public demo' }

Write-Host ''
Write-Host 'Validating unsafe canary PKG-199 (expected to fail)' -ForegroundColor Cyan
& $python (Join-Path $root 'tools\package-builder\validate_manifest.py') `
  (Join-Path $root 'packages\examples\PKG-199-blocked-driver-canary.json')
if ($LASTEXITCODE -eq 0) { throw 'PKG-199 unexpectedly passed validation' }

$store = Join-Path $root 'build\recovery.img'
if (-not (Test-Path $store)) {
  $bytes = New-Object byte[] (512 * 2048)
  [IO.File]::WriteAllBytes($store, $bytes)
  Write-Host "created durable recovery image: $store"
}

Write-Host ''
Write-Host 'Importing PKG-101 into manifest slot 0' -ForegroundColor Cyan
& (Join-Path $PSScriptRoot 'import-example-package.ps1')

Write-Host ''
Write-Host 'Next: boot the OS' -ForegroundColor Green
Write-Host '  .\scripts\run.ps1'
Write-Host ''
Write-Host 'Inside KamikazeOS, run:'
Write-Host '  about'
Write-Host '  health'
Write-Host '  store'
Write-Host '  journal'
Write-Host '  snapshots'
Write-Host '  devices'
Write-Host '  ghostattach'
Write-Host '  trust'
Write-Host '  manifests'
Write-Host '  packages'
Write-Host '  package PKG-101'
Write-Host '  approve-pkg PKG-101'
Write-Host '  apply-pkg PKG-101'
Write-Host '  slots'
Write-Host '  verify-pkg PKG-101'
Write-Host '  export-logs'
Write-Host ''
Write-Host 'After export-logs, run:'
Write-Host '  python ai-recovery\analyze_logs.py build\serial.log'
Write-Host '  python ai-recovery\export_dashboard_json.py build\serial.log recovery-center\data\recovery-report.json'
