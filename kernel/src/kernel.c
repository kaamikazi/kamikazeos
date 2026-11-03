/*
 * kernel.c — kmain(), the C entry point of KamikazeOS.
 *
 * boot.S hands control here after setting up the stack. Our job:
 * initialize each subsystem in dependency order, log every step in the
 * structured error log (self-diagnosis starts at boot, not after), draw
 * the boot screen, and hand off to the shell.
 */
#include <stdint.h>
#include "app.h"
#include "boot_snapshot.h"
#include "errors.h"
#include "gdt.h"
#include "ghostattach.h"
#include "idt.h"
#include "interrupts.h"
#include "kheap.h"
#include "keyboard.h"
#include "memory.h"
#include "paging.h"
#include "panic.h"
#include "recovery_actions.h"
#include "module_slots.h"
#include "recovery_memory.h"
#include "recovery_package.h"
#include "recovery_store.h"
#include "report.h"
#include "ring3.h"
#include "serial.h"
#include "shell.h"
#include "pic.h"
#include "privilege.h"
#include "timer.h"
#include "task.h"
#include "syscall.h"
#include "tinyfs.h"
#include "tss.h"
#include "vga.h"

/* The value a Multiboot loader leaves in EAX to prove it booted us. */
#define MULTIBOOT_BOOTLOADER_MAGIC 0x2BADB002u

static void boot_banner(void)
{
    vga_set_color(VGA_LIGHT_CYAN, VGA_BLACK);
    vga_print("\n");
    vga_print("  _  __               _ _                    ___  ____  \n");
    vga_print(" | |/ /__ _ _ __ ___ (_) | ____ _ _______   / _ \\/ ___| \n");
    vga_print(" | ' // _` | '_ ` _ \\| | |/ / _` |_  / _ \\ | | | \\___ \\ \n");
    vga_print(" | . \\ (_| | | | | | | |   < (_| |/ /  __/ | |_| |___) |\n");
    vga_print(" |_|\\_\\__,_|_| |_| |_|_|_|\\_\\__,_/___\\___|  \\___/|____/ \n");
    vga_print("\n");
    vga_set_color(VGA_DARK_GREY, VGA_BLACK);
    vga_print("  v2.6.1 \"Integrity Gate\"  --  Ring 3 held, evidence never overwritten\n\n");
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
}

/* Print one " [ OK ] subsystem" boot line, like classic Unix boots. */
static void report_ok(const char *what)
{
    vga_print("  [ ");
    vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
    vga_print("OK");
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
    vga_print(" ] ");
    vga_print(what);
    vga_print("\n");
}

static void report_warn(const char *what)
{
    vga_print("  [");
    vga_set_color(VGA_YELLOW, VGA_BLACK);
    vga_print("WARN");
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
    vga_print("] ");
    vga_print(what);
    vga_print("\n");
}

/*
 * kmain — called from boot.S with the two values the Multiboot loader
 * left for us (see boot.S for how they travel through the stack).
 * `mb_info` points at boot facts (memory map etc.).
 */
void kmain(uint32_t mb_magic, uint32_t mb_info)
{
    /* Display first: until vga_init runs we are blind. */
    vga_init();
    boot_banner();

    /*
     * Serial before the error log: error_log() streams every event out
     * COM1, so the UART must be configured before the first log call.
     */
    serial_init();
    errors_init();
    gdt_init();
    idt_init_exceptions();
    tss_init();
    serial_print("\n=== KamikazeOS v2.6.1 serial log ===\n");

    error_log(SEV_INFO, "BOOT-000", "kernel",
              "Kernel booted successfully", "no action needed");
    report_ok("vga text console");
    report_ok(serial_present() ? "com1 serial (AI log export)"
                               : "com1 serial NOT detected");
    report_ok("structured error log");
    report_ok(gdt_is_loaded() ? "kernel-owned GDT loaded"
                              : "kernel GDT unavailable");
    report_ok(tss_is_loaded() ? "32-bit TSS and privilege stack loaded"
                              : "TSS unavailable; user gate must stay disabled");
    report_ok(idt_is_loaded() ? "early CPU exception IDT loaded"
                              : "early exception IDT unavailable");

    /*
     * RecoveryMemory: read what previous boots left in NVRAM, BEFORE any
     * device scanning — the safe-mode flag changes how the scan runs.
     */
    recovery_memory_init();
    const struct recovery_memory *rmem = recovery_memory_get();
    /* valid_at_boot = state was actually HYDRATED from somewhere; the
       store may have become valid afterwards via the first save. */
    if (rmem->valid_at_boot && !recovery_memory_used_cmos_fallback())
        report_ok("recovery memory restored from DISK (durable)");
    else if (rmem->valid_at_boot)
        report_ok("recovery memory restored from CMOS NVRAM");
    else
        report_ok("recovery memory initialized (fresh)");

    /* Previous boot died? Log it loudly FIRST so the evidence is in this
       boot's log and report before anything clears the stored flag. */
    if (rmem->prev_panic_detected) {
        error_log_class(SEV_WARNING,
                        rmem->prev_panic_intentional ? HEALTH_INTENTIONAL_TEST
                                                     : HEALTH_PRODUCTION,
                        "BOOT-WARN-001", "kernel",
                  "Previous boot ended in panic",
                  "approve ACT-004 to investigate in safe mode");
        report_warn("previous boot ended in PANIC (see `recovery`)");
    }

    /* Safe-mode scheduled? Enter it, and clear the flag immediately —
       safe mode is one-shot, the next boot is normal again. */
    if (rmem->safe_mode_next_boot) {
        recovery_memory_set_safe_mode_active(1);
        recovery_memory_set_safe_mode_next_boot(0);   /* saves to NVRAM */
        error_log(SEV_INFO, "SAFE-BOOT-001", "kernel",
                  "SAFE MODE BOOT ACTIVE - non-essential devices skipped",
                  "reboot for a normal boot");
        recovery_journal_append(JE_SAFE_MODE_BOOT, 4, 1, 1, 1, "SAFEBT");
        vga_set_color(VGA_BLACK, VGA_YELLOW);
        vga_print(" SAFE MODE BOOT ACTIVE ");
        vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
        vga_print(" minimal drivers - see `safe-mode`\n");
    }

    /*
     * Sanity check: were we actually loaded by a Multiboot loader?
     * If not, the boot facts in mb_info are garbage and continuing would
     * mean guessing — rule 5 says we stop loudly instead.
     */
    if (mb_magic != MULTIBOOT_BOOTLOADER_MAGIC) {
        kpanic("BOOT-001",
               "Bootloader is not Multiboot-compliant; boot facts untrusted",
               "boot via a Multiboot loader (GRUB or QEMU -kernel)");
    }
    report_ok("multiboot handshake");

    memory_init(mb_magic, mb_info);
    report_ok(memory_map_available() ? "multiboot memory map parsed"
                                     : "memory map fallback active");

    kheap_init();
    report_ok(kheap_available() ? "fixed kernel bump heap initialized"
                                : "kernel heap unavailable");

    paging_init_structures();
    if (paging_enable())
        report_ok("identity paging enabled after strict validation");
    else
        report_warn("paging prepared but not enabled (validation gate held)");

    tinyfs_init();
    if (tinyfs_status()->formatted)
        report_ok("TinyFS metadata loaded from LBA 64-127");
    else if (tinyfs_status()->available)
        report_warn("TinyFS not formatted (run `fs-format confirm`)");
    else
        report_warn("TinyFS unavailable - kernel continues without files");

    interrupts_init();
    report_ok(idt_irq_gates_installed() ? "hardware IRQ gates installed"
                                        : "hardware IRQ gates unavailable");
    report_ok(pic_is_remapped() ? "8259 PIC remapped to IRQ vectors 32-47"
                                : "8259 PIC remap failed");
    report_ok(timer_is_enabled() ? "PIT timer IRQ0 configured at 100 Hz"
                                 : "PIT timer unavailable");

    task_init();
    report_ok("fixed cooperative task table (no stack switching)");

    apps_init();
    report_ok("kernel-managed app registry (not Ring 3 userspace)");

    syscall_init();
    privilege_init();
    ring3_init();
    struct syscall_stats syscall_boot_stats;
    syscall_stats(&syscall_boot_stats);
    report_ok(syscall_boot_stats.initialized
              ? (syscall_boot_stats.gate_dpl == 3u
                 ? "validated syscall gate (int 0x80 DPL3; fixed CPL3 test gated)"
                 : "syscall gate fallback (int 0x80 DPL0)")
              : "syscall gate unavailable");

    /* Keyboard: may legitimately fail; it logs its own error and we keep
       booting (a server without a keyboard is still a working server). */
    keyboard_init();
    keyboard_enable_irq();
    if (keyboard_irq_enabled())
        report_ok("ps/2 keyboard (IRQ1)");
    else if (keyboard_present())
        report_warn("ps/2 keyboard (polling fallback)");
    else
        report_warn("ps/2 keyboard missing - shell will be unusable");

    interrupts_enable();
    report_ok("hardware interrupts enabled");

    /*
     * GhostAttach boot scan: walk the device registry, apply the attach
     * policy (exact verified -> verified fallback -> block/missing/isolate),
     * and log every decision with a GA-* code. Runs AFTER keyboard_init()
     * because the scan consumes the real PS/2 probe result.
     */
    ghostattach_scan(recovery_memory_get()->safe_mode_active);

    struct ghostattach_stats st;
    ghostattach_get_stats(&st);
    if (st.attached + st.fallback == st.total) {
        report_ok("ghostattach: all devices have verified drivers");
    } else {
        /* Some devices have no driver — that's a warning, not a failure:
           the system runs, but degraded. The details live in `devices`. */
        report_warn("ghostattach: some devices lack drivers (see `devices`)");
    }

    /*
     * RecoveryEngine: load the fixed action catalog (which immediately
     * blocks ACT-999 via the policy gate), then run the suggestion rules
     * against the GhostAttach scan results we just produced.
     */
    recovery_init();
    recovery_suggest_scan();
    report_ok("recovery engine (fixed catalog, policy-gated)");

    /* RecoveryPackage v1.0: manifests (disk or seed), A/B slots, and
       persistent lifecycle state. Blocks the unsigned canary, suggests
       applicable updates, and flags pending verification from before. */
    recovery_package_init();
    report_ok("recovery packages (manifests + A/B slots, policy-gated)");
    if (module_slots_pending_verify())
        report_warn("an applied update awaits verification (see `slots`)");

    error_log(SEV_INFO, "AI-000", "AI Recovery Core",
              "Host-side analyzer available", "run analyze_logs.py on serial log");

    error_log(SEV_INFO, "KRN-003", "kernel",
              "Boot complete, starting shell", "no action needed");

    /*
     * Send the machine-readable boot report to the host. This is the
     * moment KamikazeOS becomes analyzable from outside: the host file
     * now contains everything the AI Recovery Core needs to reason about
     * this boot — without the AI ever entering the kernel.
     */
    report_send();
    report_ok("boot report exported to host (serial)");

    /*
     * ONLY NOW mark this boot clean. Ordering is deliberate: the panic
     * evidence was logged (BOOT-WARN-001) and exported in the report
     * above, so clearing the stored flag no longer destroys anything.
     * If we crashed before this line, the flag would survive — correct.
     */
    recovery_memory_mark_clean_boot();
    recovery_journal_append(JE_CLEAN_BOOT, 0, 1, 0, 1, "CLEANB");

    /*
     * Capture this boot's health snapshot LAST, when the state is final:
     * scan done, persisted actions restored, report exported. This is the
     * record future boots (and the analyzer) compare themselves against.
     */
    if (boot_snapshot_save())
        report_ok("boot health snapshot persisted");
    vga_print("\n");

    /* Hand off to the shell forever. */
    shell_run();

    /* shell_run never returns; if it ever does, that's a bug worth a panic. */
    kpanic("KRN-004", "Shell main loop returned unexpectedly",
           "reboot; report this as a kernel bug");
}
