# Export package manifests from build\recovery.img to build\exported-manifests.json.
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

& $python `
  (Join-Path $root 'tools\package-builder\export_manifests.py') `
  (Join-Path $root 'build\recovery.img') `
  --out (Join-Path $root 'build\exported-manifests.json')
if ($LASTEXITCODE -ne 0) { throw 'exporting manifests failed' }
