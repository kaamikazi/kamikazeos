# Build the kernel, then run the headless QEMU regression suite.
# Every test uses its own disposable disk image under build\test-runs\;
# build\recovery.img is never touched (the runner fails if it changes).
#   .\scripts\run-tests.ps1              # everything
#   .\scripts\run-tests.ps1 -k F2        # tests whose id contains "F2"
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$python = (Get-Command python -ErrorAction SilentlyContinue).Source
if (-not $python) { throw 'Python not found.' }
& $python (Join-Path $root 'tests\run_regression.py') @args
exit $LASTEXITCODE
