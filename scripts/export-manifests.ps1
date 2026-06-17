# Export package manifests from build\recovery.img to build\exported-manifests.json.
$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$python = (Get-Command python -ErrorAction SilentlyContinue).Source
if (-not $python) { $python = 'python' }

& $python `
  (Join-Path $root 'tools\package-builder\export_manifests.py') `
  (Join-Path $root 'build\recovery.img') `
  --out (Join-Path $root 'build\exported-manifests.json')
if ($LASTEXITCODE -ne 0) { throw 'exporting manifests failed' }
