/* panic.c — diagnostic crash screen. See panic.h. */
#include "panic.h"
#include "errors.h"
#include "recovery_memory.h"
#include "recovery_store.h"
#include "vga.h"

void kpanic(const char *code, const char *msg, const char *hint)
{
    /* 1. Record the event FIRST — diagnostics before cosmetics. This also
          streams the entry out the serial port to the host. */
    error_log(SEV_PANIC, code, "kernel", msg, hint);

    /* 2. Burn the evidence into NVRAM + disk so the NEXT boot knows this
          one died — that's what triggers the safe-mode suggestion. The
          journal gets a durable PANIC record too. */
    recovery_memory_mark_panic();
    recovery_journal_append(JE_PANIC, 0, 0, 2 /*high*/, 0, "PANIC0");

    /* 3. Paint the panic screen. White on red, the universal "stop". */
    vga_set_color(VGA_WHITE, VGA_RED);
    vga_clear();
    vga_print("\n");
    vga_print("  ==========================================\n");
    vga_print("   KAMIKAZEOS KERNEL PANIC\n");
    vga_print("  ==========================================\n\n");
    vga_print("  Error code    : ");
    vga_print(code);
    vga_print("\n  Detail        : ");
    vga_print(msg);
    vga_print("\n  Recovery hint : ");
    vga_print(hint);
    vga_print("\n\n");
    vga_print("  The kernel reached a state it cannot safely continue from,\n");
    vga_print("  so it stopped on purpose to protect your data.\n\n");
    vga_print("  This event was recorded in the structured log AND streamed\n");
    vga_print("  to the serial port, so it can be analyzed after reboot\n");
    vga_print("  (later: by the AI Recovery Core).\n\n");
    vga_print("  It is now safe to close the emulator.\n");

    /* 4. Stop the CPU safely: interrupts off, halt forever. The loop matters:
          a stray non-maskable interrupt can wake `hlt`, so we re-halt. */
    for (;;)
        __asm__ volatile ("cli; hlt");
}
