/*
 * KamikazeOS linker script — tells the linker how to lay out the kernel
 * in memory. The Multiboot loader reads this layout from the ELF headers
 * and copies each section to the address we choose here.
 */

ENTRY(_start)                /* execution begins at _start (see boot.S) */

SECTIONS
{
    /*
     * Start at 1 MiB. Everything below 1 MiB on x86 is a minefield of
     * legacy regions (real-mode IVT, BIOS data, VGA memory at 0xB8000,
     * BIOS ROM). 1 MiB is the traditional safe home for kernels.
     */
    . = 1M;

    .text : ALIGN(4K)
    {
        KEEP(*(.multiboot))  /* the Multiboot header MUST come first so the
                                loader finds it within the first 8 KiB.
                                KEEP() stops the linker discarding it as
                                "unused" (nothing references it by name). */
        *(.text)             /* all executable code */
    }

    .rodata : ALIGN(4K) { *(.rodata) }          /* read-only data (strings) */
    .data   : ALIGN(4K) { *(.data) }            /* initialised globals */
    .bss    : ALIGN(4K) { *(COMMON) *(.bss) }   /* zeroed globals + stack */
}
