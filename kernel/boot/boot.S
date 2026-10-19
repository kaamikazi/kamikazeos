/*
 * KamikazeOS — boot.S — the very first code of the OS.
 *
 * HOW THE OS GETS LOADED
 * ----------------------
 * We don't write a disk bootloader in Phase 1. Instead we follow the
 * "Multiboot" standard (v1): we put a small magic header at the start of our
 * kernel binary. Any Multiboot loader (GRUB, or QEMU's built-in `-kernel`
 * option, which is what we use) finds this header, loads the kernel into
 * memory at the address our linker script asked for (1 MiB), switches the
 * CPU into 32-bit protected mode, and jumps to our `_start` symbol.
 *
 * So by the time the first instruction below runs, we are already in 32-bit
 * protected mode with paging OFF. What we do NOT have yet: a stack, and any
 * C runtime. This file's only job is to provide both and call kmain().
 *
 * SYNTAX NOTE: this file is assembled by clang. The directive below switches
 * to Intel syntax ("mov dest, src") which most tutorials and manuals use.
 */
.intel_syntax noprefix

/* ------------------------------------------------------------------ */
/* Multiboot v1 header — must appear in the first 8 KiB of the kernel. */
/* ------------------------------------------------------------------ */

.set MB_MAGIC,    0x1BADB002          /* fixed magic number from the spec   */
.set MB_FLAGS,    0x00000003          /* bit0: align modules on pages,      */
                                      /* bit1: please pass us a memory map  */
.set MB_CHECKSUM, -(MB_MAGIC + MB_FLAGS) /* magic+flags+checksum must == 0  */

.section .multiboot, "a"             /* "a" = allocatable: keep it in RAM  */
.align 4
.long MB_MAGIC
.long MB_FLAGS
.long MB_CHECKSUM

/* ------------------------------------------------------------------ */
/* Stack — C code cannot run without one. 16 KiB is plenty for now.    */
/* .bss takes no space in the binary; the loader zeroes it for us.     */
/* ------------------------------------------------------------------ */

.section .bootstrap_stack, "aw", @nobits
.balign 4096
.global stack_bottom
.global stack_top
stack_bottom:
.skip 16384                          /* 16 KiB */
stack_top:                           /* stacks grow DOWN, so we start here */

/* ------------------------------------------------------------------ */
/* Entry point                                                          */
/* ------------------------------------------------------------------ */

.section .text
.global _start
.type _start, @function
_start:
    /* No device interrupt may arrive before the kernel installs its IDT. */
    cli

    /* Give C code a stack. */
    lea esp, stack_top

    /*
     * The Multiboot loader left two values for us in registers:
     *   EAX = 0x2BADB002 (proof a Multiboot loader booted us)
     *   EBX = physical address of the "multiboot info" struct (memory map…)
     * Pass them to kmain(magic, info) using the cdecl calling convention:
     * arguments are pushed right-to-left, so push info first, magic last.
     */
    push ebx
    push eax
    call kmain

    /*
     * kmain should never return. If it somehow does, stop the CPU cleanly:
     * cli = disable interrupts, hlt = sleep until interrupt (which can now
     * never come), and loop in case of a stray NMI waking us up.
     */
hang:
    cli
    hlt
    jmp hang
.size _start, . - _start
