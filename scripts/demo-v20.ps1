# KamikazeOS v2.0 integrated demo preparation.
$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot

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

Write-Host '== KamikazeOS v2.0 Real Kernel Demo Prep ==' -ForegroundColor Cyan
& (Join-Path $PSScriptRoot 'build.ps1')

Write-Host ''
Write-Host 'Validating safe signed package PKG-101' -ForegroundColor Cyan
& $python (Join-Path $root 'tools\package-builder\validate_manifest.py') `
    (Join-Path $root 'packages\examples\PKG-101-ghostattach-policy.json')
if ($LASTEXITCODE -ne 0) { throw 'PKG-101 should validate for the v2.0 demo' }

Write-Host ''
Write-Host 'Validating unsafe canary PKG-199 (expected to fail)' -ForegroundColor Cyan
& $python (Join-Path $root 'tools\package-builder\validate_manifest.py') `
    (Join-Path $root 'packages\examples\PKG-199-blocked-driver-canary.json')
if ($LASTEXITCODE -eq 0) { throw 'PKG-199 unexpectedly passed validation' }

Write-Host ''
Write-Host 'Demo boot command:' -ForegroundColor Green
Write-Host '  .\scripts\run-demo.ps1'
Write-Host ''
Write-Host 'Inside KamikazeOS:'
Write-Host '  system'
Write-Host '  diagnose'
Write-Host '  health'
Write-Host '  tasks'
Write-Host '  fs'
Write-Host '  apps'
Write-Host '  run tinyfs-demo'
Write-Host '  cat appdemo.txt'
Write-Host '  packages'
Write-Host '  trust'
Write-Host '  export-logs'
Write-Host ''
Write-Host 'After export-logs:'
Write-Host '  .\scripts\generate-dashboard.ps1 -Serve'
