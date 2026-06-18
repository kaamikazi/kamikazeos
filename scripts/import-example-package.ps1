# Import the safe PKG-101 example into build\recovery.img.
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
  (Join-Path $root 'tools\package-builder\import_manifest.py') `
  (Join-Path $root 'packages\examples\PKG-101-ghostattach-policy.json') `
  (Join-Path $root 'build\recovery.img') `
  --slot 0
if ($LASTEXITCODE -ne 0) { throw 'importing PKG-101 failed' }
