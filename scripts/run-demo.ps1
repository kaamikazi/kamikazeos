# Build if needed, then launch KamikazeOS in QEMU for the public demo.
$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$elf = Join-Path $root 'build\kamikaze.elf'

if (-not (Test-Path $elf)) {
    & (Join-Path $PSScriptRoot 'build.ps1')
}

Write-Host 'Launching KamikazeOS demo in QEMU.' -ForegroundColor Cyan
Write-Host 'Inside the shell, run: health, store, snapshots, trust, packages, manifests, export-logs'
Write-Host 'After export-logs, close QEMU and run: .\scripts\generate-dashboard.ps1 -Serve'

& (Join-Path $PSScriptRoot 'run.ps1')
