# KamikazeOS build script (Windows, no make needed).
#
# Pipeline:  *.c, *.S  --clang-->  *.o  --ld.lld + linker.ld-->  kamikaze.elf
#
# Key flags explained:
#   --target=i686-pc-none-elf  cross-compile: 32-bit x86, NO operating system,
#                              ELF output (clang is a cross-compiler natively)
#   -ffreestanding             "there is no C standard library or OS below us"
#   -fno-stack-protector       the stack-guard runtime doesn't exist here
#   -nostdlib (link)           don't link any libc/startup files
#   -static (link)             no dynamic linking machinery
$ErrorActionPreference = 'Stop'

$root   = Split-Path -Parent $PSScriptRoot
$kernel = Join-Path $root 'kernel'
$build  = Join-Path $root 'build'

# Locate LLVM (PATH first, then default install dir).
$clang = (Get-Command clang -ErrorAction SilentlyContinue).Source
$lld   = (Get-Command ld.lld -ErrorAction SilentlyContinue).Source
if (-not $clang) { $clang = 'C:\Program Files\LLVM\bin\clang.exe' }
if (-not $lld)   { $lld   = 'C:\Program Files\LLVM\bin\ld.lld.exe' }
if (-not (Test-Path $clang)) { throw 'clang not found. Install with: winget install LLVM.LLVM' }

New-Item -ItemType Directory -Force $build | Out-Null

$cflags = @(
    '--target=i686-pc-none-elf', '-m32',
    # Plain i686, NO SSE/MMX/float: the kernel hasn't enabled those CPU
    # features (CR4 setup), so any vectorized instruction clang emits at -O2
    # would be an invalid opcode -> triple fault -> silent reboot at boot.
    '-march=i686', '-mno-sse', '-mno-sse2', '-mno-mmx', '-msoft-float',
    '-ffreestanding', '-fno-stack-protector', '-fno-builtin',
    '-nostdlib',
    '-Wall', '-Wextra',
    '-O2', '-g',
    '-c'
)

$objects = @()

# 1. Assemble the boot stub.
& $clang @cflags (Join-Path $kernel 'boot\boot.S') -o (Join-Path $build 'boot.o')
if ($LASTEXITCODE -ne 0) { throw 'assembling boot.S failed' }
$objects += Join-Path $build 'boot.o'

# 2. Compile every C file in kernel/src.
foreach ($src in Get-ChildItem (Join-Path $kernel 'src') -Filter *.c) {
    $obj = Join-Path $build ($src.BaseName + '.o')
    Write-Host "  CC  $($src.Name)"
    & $clang @cflags $src.FullName -o $obj
    if ($LASTEXITCODE -ne 0) { throw "compiling $($src.Name) failed" }
    $objects += $obj
}

# 3. Link with our memory-layout script into the final Multiboot ELF.
$elf = Join-Path $build 'kamikaze.elf'
Write-Host '  LD  kamikaze.elf'
& $lld -T (Join-Path $kernel 'linker.ld') -nostdlib -static -o $elf @objects
if ($LASTEXITCODE -ne 0) { throw 'linking failed' }

Write-Host "Build OK -> $elf" -ForegroundColor Green
