# Import the safe PKG-101 example into build\recovery.img.
$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$python = (Get-Command python -ErrorAction SilentlyContinue).Source
if (-not $python) { $python = 'python' }

& $python `
  (Join-Path $root 'tools\package-builder\import_manifest.py') `
  (Join-Path $root 'packages\examples\PKG-101-ghostattach-policy.json') `
  (Join-Path $root 'build\recovery.img') `
  --slot 0
if ($LASTEXITCODE -ne 0) { throw 'importing PKG-101 failed' }
