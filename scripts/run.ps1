# Boot KamikazeOS in QEMU.
#
#   -kernel       QEMU contains a Multiboot loader: it loads our ELF directly.
#                 No GRUB, no ISO image, no disk needed in Phase 1.
#   -m 64         64 MB of RAM (plenty).
#   -no-reboot    a triple-fault halts QEMU instead of silently rebooting,
#                 so kernel bugs are visible instead of becoming boot loops.
$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$elf  = Join-Path $root 'build\kamikaze.elf'
if (-not (Test-Path $elf)) { throw "kernel not built yet - run scripts\build.ps1 first" }

$qemu = (Get-Command qemu-system-i386 -ErrorAction SilentlyContinue).Source
if (-not $qemu) { $qemu = 'C:\Program Files\qemu\qemu-system-i386.exe' }
if (-not (Test-Path $qemu)) { throw 'QEMU not found. Install with: winget install SoftwareFreedomConservancy.QEMU' }

# The durable recovery disk: created once, then preserved across builds.
# The kernel's RecoveryStore (state + journal) lives in it, so recovery
# memory survives full QEMU restarts.
$store = Join-Path $root 'build\recovery.img'
if (-not (Test-Path $store)) {
    $bytes = New-Object byte[] (512 * 2048)   # 1 MiB of zeros
    [IO.File]::WriteAllBytes($store, $bytes)
    Write-Host "created durable recovery image: $store"
}

# -serial file:... captures the kernel's live AI-readable log stream:
# every error_log() call in the kernel lands in build\serial.log on the host.
# (No -no-reboot: the shell's `reboot` command should actually reboot, so
# CMOS/disk persistence can be observed across restarts.)
& $qemu -kernel $elf -m 64 -serial "file:$root\build\serial.log" `
        -drive "file=$store,format=raw,if=ide" @args
