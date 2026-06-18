# KamikazeOS v1.5 public demo preparation.
#
# This script prepares host-side demo state and prints the manual QEMU shell
# flow. It does not automate keyboard input inside the OS.
$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$store = Join-Path $root 'build\recovery.img'

function Get-KmzPython {
    $cmd = (Get-Command python -ErrorAction SilentlyContinue).Source
    if ($cmd) { return $cmd }
    $bundled = 'C:\Users\imran\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe'
    if (Test-Path $bundled) { return $bundled }
    $py = (Get-Command py -ErrorAction SilentlyContinue).Source
    if ($py) { return $py }
    throw 'Python not found. Install Python or run inside the Codex workspace runtime.'
}

$python = Get-KmzPython

Write-Host '== KamikazeOS v1.5 Public Demo Prep ==' -ForegroundColor Cyan

& (Join-Path $PSScriptRoot 'build.ps1')

if (-not (Test-Path $store)) {
    New-Item -ItemType Directory -Force (Split-Path $store) | Out-Null
    $bytes = New-Object byte[] (512 * 2048)
    [IO.File]::WriteAllBytes($store, $bytes)
    Write-Host "created durable recovery image: $store"
}

Write-Host ''
Write-Host 'Validating safe signed package PKG-101' -ForegroundColor Cyan
& $python (Join-Path $root 'tools\package-builder\validate_manifest.py') `
    (Join-Path $root 'packages\examples\PKG-101-ghostattach-policy.json')
if ($LASTEXITCODE -ne 0) { throw 'PKG-101 should validate for the public demo' }

Write-Host ''
Write-Host 'Validating unsafe canary PKG-199 (expected to fail)' -ForegroundColor Cyan
& $python (Join-Path $root 'tools\package-builder\validate_manifest.py') `
    (Join-Path $root 'packages\examples\PKG-199-blocked-driver-canary.json')
if ($LASTEXITCODE -eq 0) { throw 'PKG-199 unexpectedly passed validation' }

Write-Host ''
Write-Host 'Importing safe PKG-101 manifest into recovery.img' -ForegroundColor Cyan
& (Join-Path $PSScriptRoot 'import-example-package.ps1')

$latestSerial = Get-ChildItem (Join-Path $root 'build') -Filter 'serial*.log' -ErrorAction SilentlyContinue |
    Sort-Object LastWriteTime -Descending |
    Select-Object -First 1

if ($latestSerial) {
    Write-Host ''
    Write-Host "Serial log found; generating dashboard data from $($latestSerial.Name)." -ForegroundColor Cyan
    & (Join-Path $PSScriptRoot 'generate-dashboard.ps1') -LogPath $latestSerial.FullName
} else {
    Write-Host ''
    Write-Host 'No serial log found yet. Boot QEMU and run export-logs before generating the dashboard.' -ForegroundColor Yellow
}

Write-Host ''
Write-Host 'Demo commands:' -ForegroundColor Green
Write-Host '  .\scripts\run-demo.ps1'
Write-Host ''
Write-Host 'Inside KamikazeOS:'
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
Write-Host 'After export-logs:'
Write-Host '  .\scripts\generate-dashboard.ps1 -Serve'
