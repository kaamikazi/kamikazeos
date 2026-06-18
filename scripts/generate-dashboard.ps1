param(
    [string]$LogPath,
    [switch]$Serve,
    [int]$Port = 8080
)

# Generate Recovery Center Pro data from build\serial.log.
$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$log = if ($LogPath) { $LogPath } else { Join-Path $root 'build\serial.log' }
$out = Join-Path $root 'recovery-center\data\recovery-report.json'
$dashboard = Join-Path $root 'recovery-center'

function Get-KmzPython {
    $cmd = (Get-Command python -ErrorAction SilentlyContinue).Source
    if ($cmd) { return $cmd }
    $bundled = 'C:\Users\imran\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe'
    if (Test-Path $bundled) { return $bundled }
    $py = (Get-Command py -ErrorAction SilentlyContinue).Source
    if ($py) { return $py }
    throw 'Python not found. Install Python or run inside the Codex workspace runtime.'
}

if (-not (Test-Path $log)) {
    throw "No serial log found at $log. Boot KamikazeOS, run export-logs, then rerun this script."
}

$python = Get-KmzPython

Write-Host 'Running analyzer...' -ForegroundColor Cyan
& $python (Join-Path $root 'ai-recovery\analyze_logs.py') $log
if ($LASTEXITCODE -ne 0) {
    Write-Host "Analyzer returned exit code $LASTEXITCODE. This can happen when the report contains critical events." -ForegroundColor Yellow
}

Write-Host 'Generating Recovery Center JSON...' -ForegroundColor Cyan
& $python (Join-Path $root 'ai-recovery\export_dashboard_json.py') $log $out
if ($LASTEXITCODE -ne 0) { throw 'dashboard JSON export failed' }

if ($Serve) {
    $existing = Get-NetTCPConnection -LocalPort $Port -State Listen -ErrorAction SilentlyContinue
    if (-not $existing) {
        Start-Process -FilePath $python -ArgumentList @('-m', 'http.server', "$Port") -WorkingDirectory $dashboard -WindowStyle Hidden
        Start-Sleep -Milliseconds 600
    }
    Write-Host "Recovery Center: http://127.0.0.1:$Port/" -ForegroundColor Green
} else {
    Write-Host 'Open recovery-center\index.html, or run with -Serve for http://127.0.0.1:8080/' -ForegroundColor Green
}
