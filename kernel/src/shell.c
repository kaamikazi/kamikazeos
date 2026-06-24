/* shell.c — interactive command loop + all KamikazeOS v0.2 commands.
 *
 * In this phase the shell runs INSIDE the kernel (there's no userspace yet):
 * read a line from the keyboard, match it against a command table, run the
 * handler. Real OSes run shells as unprivileged processes — that separation
 * arrives when we have processes and syscalls.
 */
#include "shell.h"
#include "app.h"
#include "boot_snapshot.h"
#include "errors.h"
#include "module_slots.h"
#include "module_versions.h"
#include "package_manifest.h"
#include "package_state.h"
#include "recovery_package.h"
#include "ghostattach.h"
#include "idt.h"
#include "interrupts.h"
#include "io.h"
#include "keyboard.h"
#include "panic.h"
#include "recovery_actions.h"
#include "recovery_memory.h"
#include "recovery_policy.h"
#include "recovery_store.h"
#include "report.h"
#include "serial.h"
#include "string.h"
#include "pic.h"
#include "timer.h"
#include "task.h"
#include "tinyfs.h"
#include "vga.h"

/*
 * Max command length. The buffer is LINE_MAX bytes; read_line() refuses to
 * store more than LINE_MAX-1 characters (the last byte is the '\0'), so the
 * user can hammer the keyboard forever without overflowing kernel memory.
 */
#define LINE_MAX 128

/* ------------------------------------------------------------------ */
/* Small helpers                                                        */
/* ------------------------------------------------------------------ */

/*
 * Ask the CPU to identify itself with the `cpuid` instruction.
 * With EAX=0, the vendor string ("GenuineIntel", "AuthenticAMD", …)
 * comes back packed in EBX,EDX,ECX — 4 ASCII bytes per register.
 */
static void cpu_vendor(char out[13])
{
    uint32_t b, c, d;
    __asm__ volatile ("cpuid"
                      : "=b"(b), "=c"(c), "=d"(d)
                      : "a"(0));
    for (int i = 0; i < 4; i++) {
        out[i]     = (char)(b >> (i * 8));
        out[i + 4] = (char)(d >> (i * 8));
        out[i + 8] = (char)(c >> (i * 8));
    }
    out[12] = '\0';
}

/*
 * Print a string padded with spaces to a fixed column width — our poor
 * man's printf("%-Ns"). If the string is longer than the column, print
 * it anyway (a misaligned row beats silently lost information).
 */
static void print_col(const char *s, int width)
{
    int n = 0;
    while (s[n] != '\0') {
        vga_putc(s[n]);
        n++;
    }
    while (n < width) {
        vga_putc(' ');
        n++;
    }
}

/* Two-digit ID column: "01 ", "02 ", … keeps the table aligned. */
static void print_id(uint32_t id)
{
    if (id < 10)
        vga_putc('0');
    vga_print_dec(id);
    vga_print("   ");
}

/* Print one log entry for humans:
 *   [3] warning DRV-101 GhostAttach: Storage driver missing
 *       hint: fallback unavailable                              */
static void print_entry(const struct kerror *e)
{
    vga_print("  [");
    vga_print_dec(e->seq);
    vga_print("] ");
    vga_set_color(e->severity >= SEV_ERROR   ? VGA_LIGHT_RED :
                  e->severity == SEV_WARNING ? VGA_YELLOW    : VGA_LIGHT_GREY,
                  VGA_BLACK);
    vga_print(severity_name(e->severity));
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
    vga_print(" ");
    vga_print(e->code);
    vga_print(" ");
    vga_print(e->component);
    vga_print(": ");
    vga_print(e->message);
    vga_print("\n        hint: ");
    vga_set_color(VGA_DARK_GREY, VGA_BLACK);
    vga_print(e->hint);
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
    vga_print("\n");
}

/* ------------------------------------------------------------------ */
/* Commands                                                             */
/* ------------------------------------------------------------------ */

static void cmd_help(void)
{
    vga_print("KamikazeOS shell commands:\n");
    vga_print("  help        show this list\n");
    vga_print("  clear       clear the screen\n");
    vga_print("  about       OS version, mission, CPU info\n");
    vga_print("  devices     GhostAttach device status table\n");
    vga_print("  drivers     GhostAttach driver registry table\n");
    vga_print("  ghostattach explain the driver recovery layer + scan status\n");
    vga_print("  errors      show warning/error/panic log entries\n");
    vga_print("  log         show the full structured log\n");
    vga_print("  logs-ai     print logs in machine-readable AI format\n");
    vga_print("  export-logs send full boot report to serial for AI analysis\n");
    vga_print("  interrupts  show IDT PIC IRQ and exception status\n");
    vga_print("  ticks       show the live PIT timer tick count\n");
    vga_print("  tasks       show the fixed kernel task table\n");
    vga_print("  task-current show the current logical kernel task\n");
    vga_print("  task-yield  cooperatively run one ready task step\n");
    vga_print("  task-demo   run one round of safe built-in task steps\n");
    vga_print("  apps        show fixed kernel-managed app registry\n");
    vga_print("  app <name>  show details for one registered app\n");
    vga_print("  run <name>  run a fixed built-in app through policy\n");
    vga_print("  app-log     show app run and block history\n");
    vga_print("  fs          show TinyFS status and fixed LBA range\n");
    vga_print("  fs-format confirm   format only TinyFS LBA 64-127\n");
    vga_print("  ls          list TinyFS files\n");
    vga_print("  touch <name> create an empty TinyFS file\n");
    vga_print("  write <name> <text> store bounded text\n");
    vga_print("  cat <name>  read and verify a TinyFS text file\n");
    vga_print("  rm <name>   delete a TinyFS file\n");
    vga_print("  exceptions-test <type> confirm  trigger a controlled CPU fault\n");
    vga_print("  recovery    show recovery system status\n");
    vga_print("  actions     show the fixed recovery action catalog\n");
    vga_print("  suggestions show currently suggested recovery actions\n");
    vga_print("  approve <id>  approve + apply a recovery action (e.g. ACT-001)\n");
    vga_print("  rollback <id> undo a reversible applied action\n");
    vga_print("  policy      show the recovery policy rules\n");
    vga_print("  memory      show persistent recovery memory (NVRAM)\n");
    vga_print("  safe-mode   show safe mode status and scheduling\n");
    vga_print("  store       show durable RecoveryStore status (disk)\n");
    vga_print("  journal     show the durable recovery action journal\n");
    vga_print("  snapshots   show per-boot health snapshots\n");
    vga_print("  health      current health score + reasons + trend\n");
    vga_print("  timeline    journal + snapshots as a recovery timeline\n");
    vga_print("  versions    show module versions and update status\n");
    vga_print("  packages    show the fixed recovery package registry\n");
    vga_print("  package <id>      details + policy verdict for one package\n");
    vga_print("  approve-pkg <id>  approve a package (policy-gated)\n");
    vga_print("  apply-pkg <id>    apply approved package (simulated update)\n");
    vga_print("  verify-pkg <id>   verify health after apply\n");
    vga_print("  rollback-pkg <id> switch back to the known-good slot\n");
    vga_print("  manifests   show package manifests + their source\n");
    vga_print("  slots       show module A/B slot metadata\n");
    vga_print("  trust       show the trust anchor + verification mode\n");
    vga_print("  seed-manifests confirm   persist built-in manifests to disk\n");
    vga_print("  reset-packages confirm   clear package states + slots\n");
    vga_print("  clear-memory confirm   erase CMOS recovery state\n");
    vga_print("  clear-store confirm    erase durable store + journal + CMOS\n");
    vga_print("  reboot      restart the machine\n");
    vga_print("  panic-test  deliberately crash to test the panic handler\n");
}

static void cmd_about(void)
{
    char vendor[13];
    cpu_vendor(vendor);

    vga_set_color(VGA_LIGHT_CYAN, VGA_BLACK);
    vga_print("KamikazeOS v1.9.0 \"Userland Spark\"\n");
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
    vga_print("Experimental OS research prototype for auditable recovery.\n");
    vga_print("v1.9: fixed kernel-managed apps via compile-time registry.\n");
    vga_print("This is not Ring 3 userspace or executable loading.\n\n");
    vga_print("  Architecture : x86 (i686), 32-bit protected mode\n");
    vga_print("  CPU vendor   : ");
    vga_print(vendor);
    vga_print("\n  Boot         : Multiboot v1 (QEMU -kernel)\n");
    vga_print("  AI core      : offline (logs are exported for later analysis)\n");
}

/*
 * devices — the live GhostAttach device registry as a table.
 * Column layout fits 80 chars: 5+10+9+21+21+14 = 80.
 */
static void cmd_devices(void)
{
    vga_set_color(VGA_LIGHT_CYAN, VGA_BLACK);
    vga_print("GhostAttach Device Status:\n");
    vga_set_color(VGA_DARK_GREY, VGA_BLACK);
    vga_print("ID   ");
    print_col("Type", 10);
    print_col("Bus", 9);
    print_col("Device Name", 21);
    print_col("Driver", 21);
    vga_print("Status\n");
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);

    for (uint32_t i = 0; i < ghostattach_device_count(); i++) {
        const struct device *d = ghostattach_device(i);

        print_id(d->device_id);
        print_col(device_type_name(d->device_type), 10);
        print_col(bus_type_name(d->bus_type), 9);
        print_col(d->name, 21);
        print_col(d->attached_driver, 21);

        /* Color the status so problems jump out at a glance. */
        vga_set_color(d->status == DEVST_ATTACHED ? VGA_LIGHT_GREEN  :
                      d->status == DEVST_FALLBACK ? VGA_LIGHT_CYAN   :
                      d->status == DEVST_MISSING_DRIVER ? VGA_YELLOW :
                                                          VGA_LIGHT_RED,
                      VGA_BLACK);
        vga_print(device_status_name(d->status));
        vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
        vga_print("\n");
    }

    struct ghostattach_stats st;
    ghostattach_get_stats(&st);
    vga_print("\nTotal: ");        vga_print_dec(st.total);
    vga_print("   attached: ");    vga_print_dec(st.attached);
    vga_print("   fallback: ");    vga_print_dec(st.fallback);
    vga_print("   missing: ");     vga_print_dec(st.missing);
    vga_print("   isolated: ");    vga_print_dec(st.isolated);
    vga_print("\n");
}

/* drivers — the local driver registry as a table. */
static void cmd_drivers(void)
{
    vga_set_color(VGA_LIGHT_CYAN, VGA_BLACK);
    vga_print("GhostAttach Driver Registry:\n");
    vga_set_color(VGA_DARK_GREY, VGA_BLACK);
    vga_print("ID   ");
    print_col("Driver Name", 22);
    print_col("Supports", 10);
    print_col("Version", 9);
    print_col("Verified", 10);
    print_col("Risk", 8);
    vga_print("Status\n");
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);

    for (uint32_t i = 0; i < ghostattach_driver_count(); i++) {
        const struct driver *d = ghostattach_driver(i);

        print_id(d->driver_id);
        print_col(d->name, 22);
        print_col(device_type_name(d->supports_device_type), 10);
        print_col(d->version, 9);
        print_col(d->verified ? "yes" : "NO", 10);
        print_col(risk_level_name(d->risk_level), 8);

        vga_set_color(d->status == DRVST_LOADED    ? VGA_LIGHT_GREEN :
                      d->status == DRVST_AVAILABLE ? VGA_LIGHT_GREY  :
                                                     VGA_LIGHT_RED,
                      VGA_BLACK);
        vga_print(driver_status_name(d->status));
        vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
        vga_print("\n");
    }
    vga_print("\nPolicy: unverified drivers are never loaded, regardless of risk level.\n");
}

/* ghostattach — explain the layer and report scan status. */
static void cmd_ghostattach(void)
{
    struct ghostattach_stats st;
    ghostattach_get_stats(&st);

    vga_set_color(VGA_LIGHT_CYAN, VGA_BLACK);
    vga_print("GhostAttach - the driver recovery layer\n\n");
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
    vga_print("GhostAttach decides, for every device at every boot, which driver\n");
    vga_print("may safely operate it. The decision is a fixed policy, not a guess:\n\n");
    vga_print("  1. EXACT MATCH    the device names its required driver; if that\n");
    vga_print("                    driver is in the registry and verified, attach it.\n");
    vga_print("  2. FALLBACK       exact driver missing? a verified generic driver\n");
    vga_print("                    for the same device class may stand in (reduced\n");
    vga_print("                    functionality, but safe).\n");
    vga_print("  3. BLOCK          unverified drivers are NEVER loaded. An unsigned\n");
    vga_print("                    driver in kernel space can corrupt anything it\n");
    vga_print("                    likes - no convenience is worth that.\n");
    vga_print("  4. ISOLATE        a driver that fails gets its device quarantined,\n");
    vga_print("                    so retries cannot turn into boot loops.\n\n");

    vga_print("Boot scan result: ");
    vga_print_dec(st.total);     vga_print(" devices - ");
    vga_print_dec(st.attached);  vga_print(" attached, ");
    vga_print_dec(st.fallback);  vga_print(" fallback, ");
    vga_print_dec(st.missing);   vga_print(" missing, ");
    vga_print_dec(st.isolated);  vga_print(" isolated\n\n");

    vga_set_color(VGA_DARK_GREY, VGA_BLACK);
    vga_print("v0.3 honesty: devices 4-7 are simulated registry entries; the PS/2\n");
    vga_print("and COM1 results come from real probes. Next upgrade: a real PCI\n");
    vga_print("bus scan (config ports 0xCF8/0xCFC) fills this registry instead.\n");
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
}

/* Shared by `errors` (warnings and worse only) and `log` (everything). */
static void dump_log(int min_severity)
{
    uint32_t shown = 0;
    struct kerror e;

    for (uint32_t i = 0; error_get(i, &e); i++) {
        if ((int)e.severity >= min_severity) {
            print_entry(&e);
            shown++;
        }
    }
    if (shown == 0)
        vga_print(min_severity > SEV_INFO
                  ? "No errors recorded. System healthy.\n"
                  : "Log is empty.\n");
}

static void cmd_errors(void) { dump_log(SEV_WARNING); }
static void cmd_log(void)    { dump_log(SEV_INFO); }

/*
 * logs-ai — print every log entry in the machine-readable format between
 * [AI_LOG_START] / [AI_LOG_END] markers, on BOTH the screen and the serial
 * port. An external tool (or a human copy-pasting from QEMU) can hand this
 * block straight to an AI analyzer. The format is one event per line:
 *   code=...;severity=...;component=...;message=...;hint=...
 */
static void cmd_logs_ai(void)
{
    struct kerror e;

    vga_print("[AI_LOG_START]\n");
    serial_print("[AI_LOG_START]\n");

    for (uint32_t i = 0; error_get(i, &e); i++) {
        /* Build each line once, send to both outputs field by field. */
        vga_print("code=");          serial_print("code=");
        vga_print(e.code);           serial_print(e.code);
        vga_print(";severity=");     serial_print(";severity=");
        vga_print(severity_name(e.severity));
        serial_print(severity_name(e.severity));
        vga_print(";component=");    serial_print(";component=");
        vga_print(e.component);      serial_print(e.component);
        vga_print(";message=");      serial_print(";message=");
        vga_print(e.message);        serial_print(e.message);
        vga_print(";hint=");         serial_print(";hint=");
        vga_print(e.hint);           serial_print(e.hint);
        vga_print("\n");             serial_print("\n");
    }

    vga_print("[AI_LOG_END]\n");
    serial_print("[AI_LOG_END]\n");

    if (serial_present())
        vga_print("(also written to COM1 - run QEMU with -serial file:... to capture)\n");
}

/* recovery — honest status report of the self-healing system. */
static void cmd_recovery(void)
{
    vga_set_color(VGA_LIGHT_CYAN, VGA_BLACK);
    const struct recovery_memory *m = recovery_memory_get();

    vga_print("KamikazeOS Recovery System - status in v0.6\n\n");
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
    vga_print("  AI Recovery Core   : PARTIAL (host-side analyzer, outside the kernel)\n");
    vga_print("  RecoveryEngine     : WORKING (fixed catalog, approve/rollback)\n");
    vga_print("  RecoveryMemory     : WORKING (CMOS NVRAM, survives reboot)\n");
    vga_print("  Safe mode          : WORKING (schedule via ACT-004, one-shot)\n");
    vga_print("  Persisted quarantine: WORKING (restored at every boot)\n");
    vga_print("  Panic detection    : WORKING (previous boot's panic is flagged)\n");
    vga_print("  Auto-fix           : DISABLED (every action needs human approval)\n");
    vga_print("  Driver installs    : BLOCKED  (high-risk, irreversible - see policy)\n\n");

    if (m->prev_panic_detected) {
        vga_set_color(VGA_YELLOW, VGA_BLACK);
        vga_print("  !! Previous boot ended in a PANIC.");
        vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
        vga_print(" Consider `approve ACT-004`\n");
        vga_print("     to investigate the next boot in safe mode.\n\n");
    }

    vga_print("The loop: errors -> `suggestions` (or host analyzer) -> `approve <id>`\n");
    vga_print("-> applied + persisted -> survives reboot -> `rollback <id>` to undo.\n\n");
    vga_set_color(VGA_DARK_GREY, VGA_BLACK);
    vga_print("Safety rule: the analyzer reads logs and suggests. It never patches,\n");
    vga_print("installs, downloads, or touches the kernel. By design.\n");
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
}

/* export-logs — send the full machine-readable boot report to the host. */
static void cmd_export_logs(void)
{
    if (!serial_present()) {
        vga_print("COM1 serial port not available - cannot export.\n");
        error_log(SEV_ERROR, "EXP-001", "kernel",
                  "Log export failed - no serial port", "check emulator serial config");
        return;
    }
    report_send();
    error_log(SEV_INFO, "EXP-000", "kernel",
              "Boot report exported via serial", "no action needed");
    vga_print("Full boot report sent to serial output (COM1).\n");
    vga_print("On the host it lands in the file QEMU was started with, e.g.\n");
    vga_print("  -serial file:build/serial.log\n");
    vga_print("Analyze it with:  python ai-recovery/analyze_logs.py build/serial.log\n");
}

static void cmd_reboot(void)
{
    vga_print("Rebooting...\n");
    error_log(SEV_INFO, "KRN-002", "kernel",
              "Reboot requested via shell", "no action needed");

    /*
     * Classic x86 reset: the PS/2 controller (yes, the keyboard chip!) has
     * a spare output pin wired to the CPU's reset line. Command 0xFE on
     * port 0x64 pulses it. Wait until the controller is ready first.
     */
    while (inb(0x64) & 0x02)
        io_wait();
    outb(0x64, 0xFE);

    /* If that somehow failed, halt rather than continue in unknown state. */
    for (;;)
        __asm__ volatile ("cli; hlt");
}

static void cmd_panic_test(void)
{
    vga_print("Triggering a deliberate kernel panic in 3.. 2.. 1..\n");
    /* No ';' or '=' in log strings — they would corrupt the AI format. */
    kpanic("TEST-PANIC-001",
           "Deliberate test panic requested via panic-test",
           "test only - reboot the VM, no data affected");
}

static void cmd_interrupts(void)
{
    uint32_t vector = interrupts_last_vector();
    uint32_t exception = interrupts_last_exception();
    vga_set_color(VGA_LIGHT_CYAN, VGA_BLACK);
    vga_print("Kernel Interrupt Status:\n");
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
    vga_print("  IDT loaded          : "); vga_print(idt_is_loaded() ? "yes\n" : "no\n");
    vga_print("  PIC remapped        : "); vga_print(pic_is_remapped() ? "yes\n" : "no\n");
    vga_print("  Timer IRQ enabled   : "); vga_print(timer_is_enabled() ? "yes\n" : "no\n");
    vga_print("  Keyboard mode       : "); vga_print(keyboard_mode_name()); vga_print("\n");
    vga_print("  Interrupts enabled  : "); vga_print(interrupts_are_enabled() ? "yes\n" : "no\n");
    vga_print("  Interrupts handled  : "); vga_print_dec(interrupts_handled()); vga_print("\n");
    vga_print("  Last vector         : ");
    if (vector == 0xFFFFFFFFu) vga_print("none\n");
    else { vga_print_dec(vector); vga_print("\n"); }
    vga_print("  Last exception      : ");
    if (exception == 0xFFFFFFFFu) vga_print("none\n");
    else { vga_print(interrupt_exception_name(exception)); vga_print("\n"); }
}

static void cmd_ticks(void)
{
    vga_print("PIT tick count: ");
    vga_print_dec(timer_ticks());
    vga_print(" (100 Hz, timer IRQ ");
    vga_print(timer_is_enabled() ? "enabled" : "disabled");
    vga_print(")\n");
}

static void cmd_tasks(void)
{
    const struct task *list[TASK_MAX];
    uint32_t count = task_list(list, TASK_MAX);
    vga_set_color(VGA_LIGHT_CYAN, VGA_BLACK);
    vga_print("Kernel Tasks (cooperative steps - no context switching):\n");
    vga_set_color(VGA_DARK_GREY, VGA_BLACK);
    print_col("ID", 5);
    print_col("Name", 19);
    print_col("State", 11);
    print_col("Prio", 7);
    print_col("TicksRun", 12);
    vga_print("LastRun\n");
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
    for (uint32_t i = 0; i < count; i++) {
        vga_print_dec(list[i]->task_id); vga_print("    ");
        print_col(list[i]->name, 19);
        print_col(task_state_name(list[i]->state), 11);
        vga_print_dec(list[i]->priority); vga_print("      ");
        vga_print_dec(list[i]->ticks_run); vga_print("           ");
        vga_print_dec(list[i]->last_run_tick); vga_print("\n");
    }
}

static void cmd_task_current(void)
{
    const struct task *t = task_current();
    if (!t) {
        vga_print("No current task.\n");
        return;
    }
    vga_print("Current task: "); vga_print_dec(t->task_id);
    vga_print(" "); vga_print(t->name);
    vga_print(" state="); vga_print(task_state_name(t->state));
    vga_print(" ticks_run="); vga_print_dec(t->ticks_run);
    vga_print("\n");
}

static void cmd_task_yield(void)
{
    task_yield();
    vga_print("Cooperative yield completed one ready task step.\n");
    vga_print("No stack switch or preemptive context switch occurred.\n");
}

static void cmd_task_demo(void)
{
    uint32_t ran = task_demo_round();
    vga_print("Task demo round completed "); vga_print_dec(ran);
    vga_print(" safe steps.\n  idle="); vga_print_dec(task_idle_steps());
    vga_print(" health_monitor="); vga_print_dec(task_health_steps());
    vga_print(" recovery_watch="); vga_print_dec(task_recovery_steps());
    vga_print("\n");
}

static void cmd_apps(void)
{
    vga_set_color(VGA_LIGHT_CYAN, VGA_BLACK);
    vga_print("Kernel-Managed Apps (fixed registry, not Ring 3 userspace):\n");
    vga_set_color(VGA_DARK_GREY, VGA_BLACK);
    print_col("Name", 18);
    print_col("Type", 15);
    print_col("Version", 9);
    print_col("Status", 12);
    vga_print("Runs\n");
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);

    for (uint32_t i = 0; i < app_count(); i++) {
        const struct app *a = app_get(i);
        print_col(a->name, 18);
        print_col(app_type_name(a->type), 15);
        print_col(a->version, 9);
        print_col(app_status_name(a->status), 12);
        vga_print_dec(a->run_count);
        vga_print("\n");
    }
    vga_print("\nPolicy: only compile-time built-in apps can run.\n");
    vga_print("No TinyFS code loading, package payload execution, or Ring 3 isolation.\n");
}

static void cmd_app_detail(const char *arg)
{
    if (!arg[0]) {
        vga_print("Usage: app <name>\n");
        return;
    }
    const struct app *a = app_find(arg);
    if (!a) {
        vga_print("Unknown app. Run `apps` for the fixed registry.\n");
        return;
    }
    vga_set_color(VGA_LIGHT_CYAN, VGA_BLACK);
    vga_print(a->name);
    vga_print("\n");
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
    vga_print("  id          : "); vga_print_dec(a->app_id); vga_print("\n");
    vga_print("  description : "); vga_print(a->description); vga_print("\n");
    vga_print("  version     : "); vga_print(a->version); vga_print("\n");
    vga_print("  type        : "); vga_print(app_type_name(a->type)); vga_print("\n");
    vga_print("  status      : "); vga_print(app_status_name(a->status)); vga_print("\n");
    vga_print("  run_count   : "); vga_print_dec(a->run_count); vga_print("\n");
    vga_print("  last_result : "); vga_print(a->last_result); vga_print("\n");
    vga_print("  isolation   : none yet - kernel-managed builtin function\n");
}

static void cmd_run_app(const char *arg)
{
    if (!arg[0]) {
        vga_print("Usage: run <name>\n");
        return;
    }
    int result = app_run(arg);
    if (result > 0) {
        vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
        vga_print("App completed.\n");
        vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
    } else if (result == 0) {
        vga_set_color(VGA_YELLOW, VGA_BLACK);
        vga_print("App failed.\n");
        vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
    } else {
        vga_set_color(VGA_LIGHT_RED, VGA_BLACK);
        vga_print("App blocked by policy.\n");
        vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
        vga_print("Only fixed built-in apps can run; no arbitrary code loading exists.\n");
    }
}

static void cmd_app_log(void)
{
    const struct app_log_entry *rows[APP_LOG_MAX];
    uint32_t count = app_log_list(rows, APP_LOG_MAX);
    vga_set_color(VGA_LIGHT_CYAN, VGA_BLACK);
    vga_print("App Run Log:\n");
    vga_set_color(VGA_DARK_GREY, VGA_BLACK);
    print_col("Seq", 6);
    print_col("Name", 18);
    print_col("Result", 12);
    vga_print("Message\n");
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
    if (!count) {
        vga_print("  (no app runs yet)\n");
    }
    for (uint32_t i = 0; i < count; i++) {
        vga_print_dec(rows[i]->seq);
        vga_print("     ");
        print_col(rows[i]->name, 18);
        print_col(rows[i]->result, 12);
        vga_print(rows[i]->message);
        vga_print("\n");
    }
    vga_print("Blocked attempts: ");
    vga_print_dec(app_blocked_attempts());
    vga_print("\n");
}

static void cmd_fs(void)
{
    const struct tinyfs_status_info *s = tinyfs_status();
    vga_set_color(VGA_LIGHT_CYAN, VGA_BLACK);
    vga_print("TinyFS Status:\n");
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
    vga_print("  Available      : "); vga_print(s->available ? "yes\n" : "no\n");
    vga_print("  Formatted      : "); vga_print(s->formatted ? "yes\n" : "no\n");
    vga_print("  Files          : "); vga_print_dec(s->file_count); vga_print(" / ");
    vga_print_dec(s->max_files); vga_print("\n");
    vga_print("  Max file size  : "); vga_print_dec(s->max_file_size); vga_print(" bytes\n");
    vga_print("  Metadata       : LBA 64-72\n");
    vga_print("  Data           : LBA 73-127\n");
    vga_print("  Last error     : "); vga_print(s->last_error); vga_print("\n");
}

static void cmd_fs_format(const char *arg)
{
    if (kstrcmp(arg, "confirm") != 0) {
        vga_print("Usage: fs-format confirm\n");
        vga_print("This wipes only TinyFS sectors LBA 64-127.\n");
        return;
    }
    vga_print("Formatting TinyFS LBA 64-127...\n");
    vga_print(tinyfs_format() ? "TinyFS format complete.\n"
                              : "TinyFS format failed; run fs for details.\n");
}

static void cmd_ls(void)
{
    const struct tinyfs_status_info *s = tinyfs_status();
    if (!s->formatted) {
        vga_print("TinyFS is not formatted or unavailable. Run fs for details.\n");
        return;
    }
    const struct tinyfs_file *list[TINYFS_MAX_FILES];
    uint32_t count = tinyfs_list(list, TINYFS_MAX_FILES);
    vga_set_color(VGA_LIGHT_CYAN, VGA_BLACK);
    vga_print("TinyFS Files:\n");
    vga_set_color(VGA_DARK_GREY, VGA_BLACK);
    print_col("Name", 28); print_col("Size", 12); vga_print("LBA\n");
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
    if (count == 0) {
        vga_print("  (empty)\n");
        return;
    }
    for (uint32_t i = 0; i < count; i++) {
        print_col(list[i]->name, 28);
        vga_print_dec(list[i]->size); vga_print("           ");
        if (list[i]->sector_count) vga_print_dec(list[i]->start_lba);
        else vga_print("-");
        vga_print("\n");
    }
}

static void cmd_touch(const char *arg)
{
    if (!arg[0]) { vga_print("Usage: touch <name>\n"); return; }
    vga_print(tinyfs_create(arg) ? "File created.\n"
                                 : "Create failed; run fs for details.\n");
}

static void cmd_write(const char *arg)
{
    if (!arg[0]) { vga_print("Usage: write <name> <text>\n"); return; }
    char *text = (char *)arg;
    while (*text && *text != ' ')
        text++;
    if (!*text) { vga_print("Usage: write <name> <text>\n"); return; }
    *text++ = '\0';
    while (*text == ' ')
        text++;
    if (!*text) { vga_print("Write text must not be empty.\n"); return; }
    vga_print(tinyfs_write_text(arg, text) ? "File written.\n"
                                           : "Write failed; run fs for details.\n");
}

static void cmd_cat(const char *arg)
{
    static char text[TINYFS_MAX_FILE_SIZE + 1];
    uint32_t size;
    if (!arg[0]) { vga_print("Usage: cat <name>\n"); return; }
    if (!tinyfs_read_text(arg, text, sizeof(text), &size)) {
        vga_print("Read failed; run fs for details.\n");
        return;
    }
    vga_print(text);
    vga_print("\n");
}

static void cmd_rm(const char *arg)
{
    if (!arg[0]) { vga_print("Usage: rm <name>\n"); return; }
    vga_print(tinyfs_delete(arg) ? "File deleted.\n"
                                 : "Delete failed; run fs for details.\n");
}

static void cmd_exceptions_test(const char *arg)
{
    if (kstrcmp(arg, "divide confirm") == 0) {
        vga_print("Triggering controlled divide-by-zero exception. The VM will halt.\n");
        interrupts_trigger_divide_test();
    } else if (kstrcmp(arg, "invalid-opcode confirm") == 0) {
        vga_print("Triggering controlled invalid-opcode exception. The VM will halt.\n");
        interrupts_trigger_invalid_opcode_test();
    } else {
        vga_print("This command deliberately halts the kernel after exporting evidence.\n");
        vga_print("Usage: exceptions-test divide confirm\n");
        vga_print("   or: exceptions-test invalid-opcode confirm\n");
    }
}

/* ------------------------------------------------------------------ */
/* RecoveryEngine commands (v0.5)                                       */
/* ------------------------------------------------------------------ */

/* actions — the full fixed catalog with live states. */
static void cmd_actions(void)
{
    vga_set_color(VGA_LIGHT_CYAN, VGA_BLACK);
    vga_print("RecoveryEngine Action Catalog (fixed - this is ALL it can ever do):\n");
    vga_set_color(VGA_DARK_GREY, VGA_BLACK);
    print_col("ID", 9);
    print_col("Name", 34);
    print_col("Risk", 8);
    print_col("Rev", 5);
    print_col("Appr", 6);
    vga_print("Status\n");
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);

    for (uint32_t i = 0; i < recovery_action_count(); i++) {
        const struct recovery_action *a = recovery_action(i);
        print_col(a->action_id, 9);
        print_col(a->name, 34);
        print_col(risk_level_name(a->risk_level), 8);
        print_col(a->reversible ? "yes" : "no", 5);
        print_col(a->requires_approval ? "yes" : "no", 6);

        vga_set_color(a->status == ACTST_APPLIED   ? VGA_LIGHT_GREEN :
                      a->status == ACTST_SUGGESTED ? VGA_YELLOW      :
                      a->status == ACTST_BLOCKED   ? VGA_LIGHT_RED   :
                                                     VGA_LIGHT_GREY,
                      VGA_BLACK);
        vga_print(action_status_name(a->status));
        vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
        vga_print("\n");
    }
    vga_print("\nUse: approve <id>   rollback <id>   (example: approve ACT-001)\n");
}

/* suggestions — only the actions the rules currently point at. */
static void cmd_suggestions(void)
{
    uint32_t shown = 0;

    vga_set_color(VGA_LIGHT_CYAN, VGA_BLACK);
    vga_print("Suggested recovery actions (rule-based, from the fixed catalog):\n\n");
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);

    for (uint32_t i = 0; i < recovery_action_count(); i++) {
        const struct recovery_action *a = recovery_action(i);
        if (a->status != ACTST_SUGGESTED)
            continue;
        shown++;
        vga_print("  ");
        vga_set_color(VGA_YELLOW, VGA_BLACK);
        vga_print(a->action_id);
        vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
        vga_print("  ");
        vga_print(a->name);
        vga_print("\n      why    : ");
        vga_print(a->description);
        vga_print("\n      risk   : ");
        vga_print(risk_level_name(a->risk_level));
        vga_print("   reversible: ");
        vga_print(a->reversible ? "yes" : "no");
        vga_print("\n      apply  : approve ");
        vga_print(a->action_id);
        vga_print("\n\n");
    }

    if (shown == 0)
        vga_print("  none right now - system state matches no suggestion rule.\n");
    vga_print("Nothing is ever applied without your approval.\n");
}

/* policy — the rules of the gate, stated plainly. */
static void cmd_policy(void)
{
    vga_set_color(VGA_LIGHT_CYAN, VGA_BLACK);
    vga_print("Recovery Policy (deterministic - nothing can widen it at runtime):\n\n");
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
    vga_print("  low risk    : human approval required\n");
    vga_print("                (auto-allow may become an explicit opt-in later)\n");
    vga_print("  medium risk : human approval required\n");
    vga_print("  high risk   : BLOCKED - not approvable\n");
    vga_print("  irreversible: BLOCKED - not approvable\n");
    vga_print("  unverified driver install: BLOCKED forever\n");
    vga_print("  AI direct repair         : BLOCKED - the AI suggests catalog\n");
    vga_print("                             actions only and cannot apply them\n\n");
    vga_print("  safe-mode-next-boot flag : ");
    vga_print(recovery_safe_mode_next() ? "SET (simulated)\n" : "not set\n");
}

/* approve <id> — the human says yes; the policy gate has the last word. */
static void cmd_approve(const char *arg)
{
    if (arg[0] == '\0') {
        vga_print("Usage: approve <action-id>     (see `actions` for the list)\n");
        return;
    }

    int r = recovery_approve(arg);
    switch (r) {
    case 1:
        vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
        vga_print("Action approved and applied.");
        vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
        vga_print(" State change is logged and reversible (`rollback`).\n");
        vga_print("Run `devices` / `actions` to see the effect.\n");
        break;
    case 0:
        vga_print("Unknown action id: `");
        vga_print(arg);
        vga_print("`. Run `actions` for the catalog.\n");
        break;
    case -1:
        vga_set_color(VGA_LIGHT_RED, VGA_BLACK);
        vga_print("BLOCKED by recovery policy.");
        vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
        vga_print(" High-risk/irreversible actions cannot be\n");
        vga_print("approved by anyone - that is the point. See `policy`.\n");
        break;
    case -2:
        vga_print("Already applied. Nothing to do.\n");
        break;
    default:
        vga_print("Apply failed - state unchanged. See `errors`.\n");
        break;
    }
}

/* rollback <id> — undo a reversible applied action. */
static void cmd_rollback(const char *arg)
{
    if (arg[0] == '\0') {
        vga_print("Usage: rollback <action-id>\n");
        return;
    }

    int r = recovery_rollback(arg);
    switch (r) {
    case 1:
        vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
        vga_print("Rolled back.");
        vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
        vga_print(" Prior state restored and logged.\n");
        break;
    case 0:
        vga_print("Unknown action id: `");
        vga_print(arg);
        vga_print("`. Run `actions` for the catalog.\n");
        break;
    case -1:
        vga_print("Nothing to roll back - action is not in `applied` state.\n");
        break;
    case -2:
        vga_print("This action is enforced by policy, not user-undoable.\n");
        break;
    default:
        vga_print("Rollback failed - state unchanged. See `errors`.\n");
        break;
    }
}

/* ------------------------------------------------------------------ */
/* RecoveryMemory commands (v0.6)                                       */
/* ------------------------------------------------------------------ */

/* memory — show the persistent recovery state. */
static void cmd_memory(void)
{
    recovery_memory_print();
}

/* safe-mode — status + how to schedule it. */
static void cmd_safe_mode(void)
{
    const struct recovery_memory *m = recovery_memory_get();

    vga_set_color(VGA_LIGHT_CYAN, VGA_BLACK);
    vga_print("Safe Mode status:\n\n");
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);

    vga_print("  active NOW            : ");
    if (m->safe_mode_active) {
        vga_set_color(VGA_YELLOW, VGA_BLACK);
        vga_print("YES - this is a safe-mode boot\n");
        vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
    } else {
        vga_print("no\n");
    }
    vga_print("  scheduled next boot   : ");
    vga_print(m->safe_mode_next_boot ? "YES (one-shot, clears on use)\n" : "no\n");

    vga_print("\nWhat safe mode changes:\n");
    vga_print("  - GhostAttach attaches ESSENTIAL devices only:\n");
    vga_print("      display, keyboard, serial (so logging still works)\n");
    vga_print("  - non-essential devices (network, audio, storage, virtual)\n");
    vga_print("    are isolated WITHOUT any driver attempt\n");
    vga_print("  - the unverified storage fallback stays blocked, as always\n");
    vga_print("\nSchedule it: approve ACT-004   (or `schedule-safe-mode`)\n");
    vga_print("Cancel it  : rollback ACT-004 before rebooting\n");
}

/* clear-memory — explicit wipe of persisted recovery state. */
static void cmd_clear_memory(const char *arg)
{
    /* Deliberate friction: state that survives reboots should not die to
       a typo. Require the explicit word `confirm` as the argument. */
    if (kstrcmp(arg, "confirm") != 0) {
        vga_print("This erases ALL persisted recovery state (quarantines,\n");
        vga_print("safe-mode schedule, panic evidence). Boot count is kept.\n\n");
        vga_print("To proceed, type exactly:  clear-memory confirm\n");
        return;
    }
    recovery_memory_clear();
    vga_print("Recovery memory cleared. Quarantines already applied to this\n");
    vga_print("session's devices remain until reboot or rollback.\n");
}

/* schedule-safe-mode — convenience wrapper, clearly labeled as such. */
static void cmd_schedule_safe_mode(void)
{
    vga_print("(convenience wrapper for: approve ACT-004 - same policy gate)\n");
    cmd_approve("ACT-004");
}

/* ------------------------------------------------------------------ */
/* RecoveryStore commands (v0.7)                                        */
/* ------------------------------------------------------------------ */

/* store — durable persistence status. */
static void cmd_store(void)
{
    vga_set_color(VGA_LIGHT_CYAN, VGA_BLACK);
    vga_print("RecoveryStore - durable recovery persistence:\n\n");
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);

    vga_print("  disk available        : ");
    vga_print(recovery_store_available() ? "yes (ATA primary master, PIO)\n"
                                         : "NO - CMOS fallback only\n");
    vga_print("  backend mode          : ");
    vga_print(recovery_memory_backend());
    vga_print("\n  state block valid     : ");
    vga_print(recovery_store_valid() ? "yes (magic + checksum ok)\n" : "no\n");
    vga_print("  journal entries       : ");
    vga_print_dec(recovery_journal_count());
    vga_print(" of 448\n");
    vga_print("  last persisted boot   : ");
    vga_print_dec(recovery_memory_get()->boot_count);
    vga_print("\n  CMOS fallback used    : ");
    vga_print(recovery_memory_used_cmos_fallback() ? "yes\n" : "no\n");

    vga_set_color(VGA_DARK_GREY, VGA_BLACK);
    vga_print("\nDisk layout: LBA 1 = state block, LBA 2-15 = append-only journal.\n");
    vga_print("Survives: shell reboot, QEMU reset, AND full QEMU close/restart.\n");
    vga_print("Does not survive: deleting build/recovery.img (make reset-store).\n");
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
}

/* journal — the latest durable recovery history entries. */
static void cmd_journal(void)
{
    uint32_t total = recovery_journal_count();

    vga_set_color(VGA_LIGHT_CYAN, VGA_BLACK);
    vga_print("Recovery Journal (append-only, durable) - ");
    vga_print_dec(total);
    vga_print(" entries:\n");
    vga_set_color(VGA_DARK_GREY, VGA_BLACK);
    vga_print("Boot  ");
    print_col("Event", 16);
    print_col("Action", 9);
    print_col("Result", 8);
    print_col("Risk", 6);
    print_col("Rev", 5);
    vga_print("Code\n");
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);

    if (!recovery_store_available()) {
        vga_print("  no disk attached - journal unavailable (CMOS-only mode)\n");
        return;
    }

    /* Show the most recent entries that fit on screen (last 16). */
    uint32_t first = (total > 16) ? total - 16 : 0;
    struct journal_entry e;
    for (uint32_t i = first; i < total; i++) {
        if (!recovery_journal_get(i, &e))
            continue;
        vga_print("  ");
        vga_print_dec(e.boot_number);
        vga_print("   ");
        print_col(journal_event_name((enum journal_event)e.event_type), 16);
        {
            /* id byte 1..4 means ACT-xxx for action events, PKG-xxx for
               package events; 255 is always the ACT-999 canary. */
            char idbuf[10];
            if (e.action_id == 0) {
                kstrcpy(idbuf, "-", sizeof(idbuf));
            } else if (e.action_id == 255) {
                kstrcpy(idbuf, "ACT-999", sizeof(idbuf));
            } else {
                kstrcpy(idbuf, e.event_type >= JE_PKG_SUGGESTED
                                ? "PKG-00x" : "ACT-00x", sizeof(idbuf));
                idbuf[6] = (char)('0' + e.action_id);
            }
            print_col(idbuf, 9);
        }
        print_col(e.result ? "ok" : "refused", 8);
        print_col(e.risk_level == 0 ? "low" :
                  e.risk_level == 1 ? "med" : "high", 6);
        print_col(e.reversible ? "yes" : "no", 5);
        vga_print(e.short_code);
        vga_print("\n");
    }
    if (total == 0)
        vga_print("  journal is empty.\n");
}

/* clear-store confirm — wipe durable state + journal + CMOS fallback. */
static void cmd_clear_store(const char *arg)
{
    if (kstrcmp(arg, "confirm") != 0) {
        vga_print("This erases the DURABLE recovery store: state block, the entire\n");
        vga_print("append-only journal, and the CMOS fallback. History is evidence -\n");
        vga_print("be sure.\n\nTo proceed, type exactly:  clear-store confirm\n");
        return;
    }
    recovery_store_clear();      /* disk: state + journal */
    recovery_memory_clear();     /* RAM + CMOS (+ re-saves cleared state) */
    vga_print("Durable store, journal, and CMOS fallback cleared.\n");
}

/* ------------------------------------------------------------------ */
/* BootSnapshot commands (v0.8)                                         */
/* ------------------------------------------------------------------ */

static void print_score_colored(uint8_t score)
{
    vga_set_color(score >= 70 ? VGA_LIGHT_GREEN :
                  score >= 40 ? VGA_YELLOW      : VGA_LIGHT_RED, VGA_BLACK);
    vga_print_dec(score);
    vga_print("/100");
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
}

/* snapshots — per-boot health records from disk. */
static void cmd_snapshots(void)
{
    struct boot_snapshot list[SNAPSHOT_SLOTS];
    int n = boot_snapshot_list(list);

    vga_set_color(VGA_LIGHT_CYAN, VGA_BLACK);
    vga_print("Boot Snapshots (health memory across boots):\n");
    vga_set_color(VGA_DARK_GREY, VGA_BLACK);
    vga_print("Boot  Safe  Panic  Att  Fall  Miss  Isol  Warn  Crit  Health\n");
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);

    if (n == 0) {
        vga_print("  none on disk (no disk attached, or first boot not finished).\n");
        return;
    }
    for (int i = 0; i < n; i++) {
        struct boot_snapshot *s = &list[i];
        vga_print("  ");   vga_print_dec(s->boot_number);
        vga_print("    "); vga_print(s->safe_mode_active ? "yes " : "no  ");
        vga_print("  ");   vga_print(s->prev_panic_detected ? "yes " : "no  ");
        vga_print("  ");   vga_print_dec(s->attached_count);
        vga_print("    "); vga_print_dec(s->fallback_count);
        vga_print("     ");vga_print_dec(s->missing_count);
        vga_print("     ");vga_print_dec(s->isolated_count);
        vga_print("     ");vga_print_dec(s->warning_count);
        vga_print("     ");vga_print_dec(s->critical_count);
        vga_print("     ");print_score_colored(s->health_score);
        vga_print("\n");
    }
}

/* health — current score with full reasoning, plus delta vs last boot. */
static void cmd_health(void)
{
    struct health_breakdown hb;
    boot_snapshot_health_now(&hb);

    vga_set_color(VGA_LIGHT_CYAN, VGA_BLACK);
    vga_print("Current boot health: ");
    print_score_colored((uint8_t)hb.score);
    vga_print("\n\n");
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);

    vga_print("  base score                  : 100\n");
    if (hb.panic_penalty)    { vga_print("  previous boot panicked      : -15\n"); }
    if (hb.missing_penalty)  { vga_print("  missing drivers (-10 each)  : -");
                               vga_print_dec((uint32_t)(-hb.missing_penalty));  vga_print("\n"); }
    if (hb.isolated_penalty) { vga_print("  isolated devices (-8 each)  : -");
                               vga_print_dec((uint32_t)(-hb.isolated_penalty)); vga_print("\n"); }
    if (hb.warning_penalty)  { vga_print("  warnings logged (-5 each)   : -");
                               vga_print_dec((uint32_t)(-hb.warning_penalty));  vga_print("\n"); }
    if (hb.critical_penalty) { vga_print("  criticals logged (-25 each) : -");
                               vga_print_dec((uint32_t)(-hb.critical_penalty)); vga_print("\n"); }

    /* Compare with the previous boot's snapshot, if we have one. */
    struct boot_snapshot list[SNAPSHOT_SLOTS];
    int n = boot_snapshot_list(list);
    uint8_t this_boot = recovery_memory_get()->boot_count;
    for (int i = n - 1; i >= 0; i--) {
        if (list[i].boot_number < this_boot) {
            int delta = hb.score - (int)list[i].health_score;
            vga_print("\n  previous boot (");
            vga_print_dec(list[i].boot_number);
            vga_print(") scored ");
            vga_print_dec(list[i].health_score);
            vga_print("/100 -> trend: ");
            if (delta > 0) {
                vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
                vga_print("+"); vga_print_dec((uint32_t)delta);
                vga_print(" improving");
            } else if (delta < 0) {
                vga_set_color(VGA_LIGHT_RED, VGA_BLACK);
                vga_print("-"); vga_print_dec((uint32_t)(-delta));
                vga_print(" declining");
            } else {
                vga_print("unchanged");
            }
            vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
            vga_print("\n");
            break;
        }
    }
}

/* timeline — journal events grouped under each boot's snapshot line. */
static void cmd_timeline(void)
{
    struct boot_snapshot list[SNAPSHOT_SLOTS];
    int n = boot_snapshot_list(list);

    vga_set_color(VGA_LIGHT_CYAN, VGA_BLACK);
    vga_print("Recovery Timeline:\n\n");
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);

    if (n == 0) {
        vga_print("  no snapshots on disk yet.\n");
        return;
    }

    struct journal_entry e;
    uint32_t jtotal = recovery_journal_count();

    for (int i = 0; i < n; i++) {
        struct boot_snapshot *s = &list[i];
        vga_print("boot ");
        vga_print_dec(s->boot_number);
        vga_print("  health ");
        print_score_colored(s->health_score);
        if (s->safe_mode_active)    vga_print("  [SAFE MODE]");
        if (s->prev_panic_detected) vga_print("  [AFTER PANIC]");
        vga_print("\n");

        /* The journal events that happened during this boot. */
        for (uint32_t j = 0; j < jtotal; j++) {
            if (!recovery_journal_get(j, &e) || e.boot_number != s->boot_number)
                continue;
            if (e.event_type == JE_CLEAN_BOOT)
                continue;                       /* noise on a timeline */
            vga_print("    ");
            vga_print(journal_event_name((enum journal_event)e.event_type));
            if (e.action_id == 255) {
                vga_print(" ACT-999");
            } else if (e.action_id != 0) {
                vga_print(e.event_type >= JE_PKG_SUGGESTED
                          ? " PKG-00" : " ACT-00");
                vga_print_dec(e.action_id);
            }
            if (!e.result)
                vga_print(" (refused)");
            vga_print("\n");
        }
    }
}

/* ------------------------------------------------------------------ */
/* RecoveryPackage commands (v0.9)                                      */
/* ------------------------------------------------------------------ */

/* versions — what is this OS actually running? */
static void cmd_versions(void)
{
    vga_set_color(VGA_LIGHT_CYAN, VGA_BLACK);
    vga_print("Module Versions:\n");
    vga_set_color(VGA_DARK_GREY, VGA_BLACK);
    print_col("Module", 28);
    print_col("Version", 10);
    vga_print("Status\n");
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);

    for (uint32_t i = 0; i < module_count(); i++) {
        const struct module_version *m = module_get(i);
        print_col(m->name, 28);
        print_col(m->version, 10);
        if (m->updated) {
            vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
            vga_print("updated this session (base ");
            vga_print(m->base_version);
            vga_print(")");
            vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
        } else {
            vga_print("base");
        }
        vga_print("\n");
    }
    vga_set_color(VGA_DARK_GREY, VGA_BLACK);
    vga_print("\nVersions reflect the ACTIVE A/B slot and persist across\n");
    vga_print("restarts (see `slots`). Still metadata-level - no binary loading.\n");
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
}

/* packages — the manifest registry with persistent lifecycle status. */
static void cmd_packages(void)
{
    vga_set_color(VGA_LIGHT_CYAN, VGA_BLACK);
    vga_print("Recovery Packages (manifests: ");
    vga_print(manifest_source_name());
    vga_print("):\n");
    vga_set_color(VGA_DARK_GREY, VGA_BLACK);
    print_col("ID", 9);
    print_col("Target", 16);
    print_col("Ver", 11);
    print_col("Risk", 8);
    print_col("Source", 14);
    print_col("Policy", 9);
    vga_print("Status\n");
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);

    for (uint32_t i = 0; i < manifest_count(); i++) {
        const struct pkg_manifest *m = manifest_get(i);
        enum pkg_status st = recovery_package_status(m);
        print_col(m->package_id, 9);
        print_col(m->target_component, 16);
        char ver[12];
        kstrcpy(ver, m->from_version, sizeof(ver));
        size_t l = kstrlen(ver);
        if (l < sizeof(ver) - 2) { ver[l] = '>'; ver[l+1] = '\0';
            kstrcpy(ver + l + 1, m->to_version, sizeof(ver) - l - 1); }
        print_col(ver, 11);
        print_col(risk_level_name(m->risk_level), 8);
        print_col(manifest_source_name(), 14);
        print_col(manifest_policy_result(m), 9);

        vga_set_color(st == PKGST_VERIFIED  ? VGA_LIGHT_GREEN :
                      st == PKGST_APPLIED   ? VGA_LIGHT_CYAN  :
                      st == PKGST_SUGGESTED ? VGA_YELLOW      :
                      st == PKGST_BLOCKED ||
                      st == PKGST_FAILED    ? VGA_LIGHT_RED   :
                                              VGA_LIGHT_GREY,
                      VGA_BLACK);
        vga_print(pkg_status_name(st));
        vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
        vga_print("\n");
    }
    vga_print("\nFlow: approve-pkg <id> -> apply-pkg <id> -> verify-pkg <id>\n");
    vga_print("Undo: rollback-pkg <id>   Details: package <id>   Slots: slots\n");
}

/* package <id> — manifest + trust verdict + slot plan + persisted state. */
static void cmd_package(const char *arg)
{
    const struct pkg_manifest *m = manifest_find(arg);
    if (!m) {
        vga_print("Unknown package id. Run `packages` for the registry.\n");
        return;
    }

    vga_set_color(VGA_LIGHT_CYAN, VGA_BLACK);
    vga_print(m->package_id);
    vga_print("  ");
    vga_print(m->name);
    vga_print("\n\n");
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
    vga_print("  description : "); vga_print(m->description);      vga_print("\n");
    vga_print("  target      : "); vga_print(m->target_component); vga_print("\n");
    vga_print("  versions    : "); vga_print(m->from_version);
    vga_print(" -> ");             vga_print(m->to_version);       vga_print("\n");
    vga_print("  payload     : "); vga_print(payload_type_name(m->payload_type)); vga_print("\n");
    vga_print("  source      : "); vga_print(manifest_source_name());             vga_print("\n");
    vga_print("  risk        : "); vga_print(risk_level_name(m->risk_level));     vga_print("\n");
    vga_print("  reversible  : "); vga_print(m->reversible ? "yes" : "no");       vga_print("\n");
    vga_print("  signature   : "); vga_print(signature_status_name(m->signature_status));
    vga_print(" (kernel verification simulated in v1.2)\n");
    vga_print("  trust anchor: ");
    if (m->trust_anchor_id == TRUST_ANCHOR_ID)
        vga_print(TRUST_ANCHOR_NAME "\n");
    else
        vga_print("UNKNOWN - not vouched for\n");
    vga_print("  checksum    : "); vga_print(m->checksum_ok ? "verified" : "MISMATCH"); vga_print("\n");
    vga_print("  checksum ok : "); vga_print(manifest_checksum_valid(m) ? "yes" : "no"); vga_print("\n");
    vga_print("  trust ok    : "); vga_print(manifest_trust_valid(m) ? "yes" : "no");    vga_print("\n");
    vga_print("  signature ok: "); vga_print(manifest_signature_valid(m) ? "yes" : "no"); vga_print("\n");
    vga_print("  sig mode    : "); vga_print(manifest_signature_mode(m)); vga_print("\n");
    vga_print("  signer key  : "); vga_print(m->signer_key_hash[0] ? m->signer_key_hash : "none"); vga_print("\n");
    vga_print("  payload hash: "); vga_print(m->signed_payload_hash[0] ? m->signed_payload_hash : "none"); vga_print("\n");

    const char *reason;
    int ok = manifest_verify(m, &reason);
    vga_print("  policy      : ");
    vga_set_color(ok ? VGA_LIGHT_GREEN : VGA_LIGHT_RED, VGA_BLACK);
    vga_print(manifest_policy_result(m));
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
    vga_print("\n");
    vga_print("  verification: ");
    vga_set_color(ok ? VGA_LIGHT_GREEN : VGA_LIGHT_RED, VGA_BLACK);
    vga_print(reason);
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
    vga_print("\n  block reason: ");
    vga_print(manifest_block_reason(m));
    vga_print("\n  status      : ");
    vga_print(pkg_status_name(recovery_package_status(m)));
    vga_print("\n");

    /* Slot plan + persisted lifecycle, when they exist. */
    const struct module_slot *slot = module_slot_for(m->target_module);
    if (slot && ok) {
        vga_print("  slot plan   : active ");
        vga_print(slot->active_slot == SLOT_A ? "A" : "B");
        vga_print(" -> apply switches to ");
        vga_print(slot->active_slot == SLOT_A ? "B" : "A");
        vga_print("\n");
    }
    const struct pkg_pstate *s = package_state_find(m->package_id);
    if (s) {
        vga_print("  persisted   : approved=");
        vga_print(s->approved ? "1" : "0");
        vga_print(" applied=");  vga_print(s->applied ? "1" : "0");
        vga_print(" verified="); vga_print(s->verified ? "1" : "0");
        vga_print(" failed=");   vga_print(s->failed ? "1" : "0");
        vga_print(" rb=");       vga_print(s->rolled_back ? "1" : "0");
        vga_print("\n");
    }
}

/* trust — the root of update trust, honestly labeled. */
static void cmd_trust(void)
{
    vga_set_color(VGA_LIGHT_CYAN, VGA_BLACK);
    vga_print("TrustAnchor - the root of update trust:\n\n");
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
    vga_print("  anchor id    : " TRUST_ANCHOR_NAME "\n");
    vga_print("  fingerprint  : " TRUST_ANCHOR_FINGERPRINT "\n");
    vga_print("  verification : ");
    vga_set_color(VGA_YELLOW, VGA_BLACK);
    vga_print("HOST Ed25519 real, kernel Ed25519 simulated in v1.2\n");
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
    vga_print("\n  Policy: a manifest is trusted only if its trust_anchor_id\n");
    vga_print("  matches this anchor AND its signature status is acceptable.\n");
    vga_print("  Unknown anchors are blocked - no override exists.\n");
    vga_print("  (The fingerprint prefix SIM: marks it as a stand-in. Real\n");
    vga_print("  Ed25519 manifest verification is the Phase 11 upgrade; the\n");
    vga_print("  enforcement path it plugs into is what you see working now.)\n");
}

/* manifests — what's loaded, from where. */
static void cmd_manifests(void)
{
    vga_set_color(VGA_LIGHT_CYAN, VGA_BLACK);
    vga_print("Package Manifests - source: ");
    vga_print(manifest_source_is_disk()
              ? "DISK sectors (LBA 32-47)\n"
              : "built-in seed (not yet on disk - see seed-manifests)\n");
    vga_set_color(VGA_DARK_GREY, VGA_BLACK);
    print_col("ID", 9);
    print_col("Name", 24);
    print_col("Source", 14);
    print_col("Chk", 5);
    print_col("Trust", 7);
    print_col("Sig", 5);
    vga_print("Policy\n");
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);

    for (uint32_t i = 0; i < manifest_count(); i++) {
        const struct pkg_manifest *m = manifest_get(i);
        print_col(m->package_id, 9);
        print_col(m->name, 24);
        print_col(manifest_source_name(), 14);
        print_col(manifest_checksum_valid(m) ? "yes" : "no", 5);
        print_col(manifest_trust_valid(m) ? "yes" : "no", 7);
        print_col(manifest_signature_valid(m) ? "yes" : "no", 5);
        const char *policy = manifest_policy_result(m);
        vga_set_color(kstrcmp(policy, "allowed") == 0 ? VGA_LIGHT_GREEN : VGA_LIGHT_RED,
                      VGA_BLACK);
        vga_print(policy);
        vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
        vga_print("\n");
        if (kstrcmp(policy, "blocked") == 0) {
            vga_set_color(VGA_DARK_GREY, VGA_BLACK);
            vga_print("      block: ");
            vga_print(manifest_block_reason(m));
            vga_print("\n");
            vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
        }
    }
    vga_print("\nPersist the seed set to disk:  seed-manifests confirm\n");
}

/* slots — the A/B module slot table. */
static void cmd_slots(void)
{
    vga_set_color(VGA_LIGHT_CYAN, VGA_BLACK);
    vga_print("Module A/B Slots (metadata-level - no binary loading):\n");
    vga_set_color(VGA_DARK_GREY, VGA_BLACK);
    print_col("Module", 24);
    print_col("Active", 8);
    print_col("Slot A", 12);
    print_col("Slot B", 12);
    print_col("Rollback", 10);
    vga_print("Pending\n");
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);

    for (uint32_t i = 0; i < module_slot_count(); i++) {
        const struct module_slot *s = module_slot_get(i);
        print_col(module_get((uint32_t)s->module)->name, 24);
        vga_set_color(VGA_LIGHT_CYAN, VGA_BLACK);
        print_col(s->active_slot == SLOT_A ? "A" : "B", 8);
        vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);

        char a[14], b[14];
        kstrcpy(a, s->slot_a_version, 9);
        size_t la = kstrlen(a);
        kstrcpy(a + la, s->slot_a_verified ? " ok" : " ?", 14 - la);
        kstrcpy(b, s->slot_b_version, 9);
        size_t lb = kstrlen(b);
        kstrcpy(b + lb, s->slot_b_verified ? " ok" : " ?", 14 - lb);
        print_col(a, 12);
        print_col(b, 12);
        print_col(s->rollback_slot == SLOT_NONE ? "-" :
                  s->rollback_slot == SLOT_A    ? "A" : "B", 10);
        if (s->pending_verify) {
            vga_set_color(VGA_YELLOW, VGA_BLACK);
            vga_print("YES - verify-pkg!");
            vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
        } else {
            vga_print("no");
        }
        vga_print("\n");
    }
}

/* seed-manifests confirm — write the built-in set to disk. */
static void cmd_seed_manifests(const char *arg)
{
    if (kstrcmp(arg, "confirm") != 0) {
        vga_print("Writes the built-in SAFE manifest set to disk sectors\n");
        vga_print("(LBA 32-47) so manifests persist across emulator restarts.\n");
        vga_print("Only the fixed seed set is ever written - the PKG-003 canary\n");
        vga_print("stays unsigned and blocked, on disk as in RAM.\n\n");
        vga_print("To proceed, type exactly:  seed-manifests confirm\n");
        return;
    }
    int n = manifest_seed_to_disk();
    if (n > 0) {
        vga_print("Seeded ");
        vga_print_dec((uint32_t)n);
        vga_print(" manifests to disk. They now persist across restarts.\n");
    } else {
        vga_print("No disk available - cannot seed manifests.\n");
    }
}

/* reset-packages confirm — clear persistent package state + slots. */
static void cmd_reset_packages(const char *arg)
{
    if (kstrcmp(arg, "confirm") != 0) {
        vga_print("Clears persistent package lifecycle state and resets module\n");
        vga_print("A/B slots to factory defaults. The recovery journal is NOT\n");
        vga_print("touched - history stays. Manifests on disk stay too.\n\n");
        vga_print("To proceed, type exactly:  reset-packages confirm\n");
        return;
    }
    package_state_clear();
    module_slots_clear();
    error_log(SEV_INFO, "PST-100", "PackageState",
              "Package states and module slots reset by user",
              "journal history preserved");
    vga_print("Package states and slots reset. Reboot to re-evaluate suggestions.\n");
}

/* The four lifecycle verbs share result reporting. */
static void report_pkg_result(int r, const char *ok_msg)
{
    switch (r) {
    case 1:
        vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
        vga_print(ok_msg);
        vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
        vga_print("\n");
        break;
    case 0:
        vga_print("Unknown package id. Run `packages` for the registry.\n");
        break;
    case -1:
        vga_set_color(VGA_LIGHT_RED, VGA_BLACK);
        vga_print("Refused.");
        vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
        vga_print(" Blocked by policy or not in a state this verb applies to.\n");
        vga_print("Run `package <id>` to see the policy verdict.\n");
        break;
    case -2:
        vga_print("Wrong order or failed check - see `package <id>` and `errors`.\n");
        break;
    case -3:
        vga_print("Version mismatch: the running module is not this package's\n");
        vga_print("from_version. Nothing was changed.\n");
        break;
    }
}

static void cmd_approve_pkg(const char *arg)
{
    if (arg[0] == '\0') { vga_print("Usage: approve-pkg <package-id>\n"); return; }
    report_pkg_result(pkg_approve(arg),
                      "Package approved. Next: apply-pkg, then verify-pkg.");
}

static void cmd_apply_pkg(const char *arg)
{
    if (arg[0] == '\0') { vga_print("Usage: apply-pkg <package-id>\n"); return; }
    report_pkg_result(pkg_apply(arg),
                      "Package applied (SIMULATED - metadata only). Run verify-pkg.");
}

static void cmd_verify_pkg(const char *arg)
{
    if (arg[0] == '\0') { vga_print("Usage: verify-pkg <package-id>\n"); return; }
    int r = pkg_verify(arg);
    if (r == -2) {
        vga_set_color(VGA_LIGHT_RED, VGA_BLACK);
        vga_print("VERIFICATION FAILED.");
        vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
        vga_print(" The package is marked failed.\n");
        vga_print("Recommended: rollback-pkg to restore the previous version.\n");
        return;
    }
    report_pkg_result(r, "Package verified healthy. rollback-pkg remains available.");
}

static void cmd_rollback_pkg(const char *arg)
{
    if (arg[0] == '\0') { vga_print("Usage: rollback-pkg <package-id>\n"); return; }
    report_pkg_result(pkg_rollback(arg),
                      "Package rolled back. Previous version restored.");
}

/* ------------------------------------------------------------------ */
/* Command table + main loop                                            */
/* ------------------------------------------------------------------ */

struct command {
    const char *name;
    void (*handler)(void);
};

static const struct command commands[] = {
    { "help",       cmd_help       },
    { "clear",      vga_clear      },
    { "about",      cmd_about      },
    { "devices",    cmd_devices    },
    { "drivers",    cmd_drivers    },
    { "ghostattach",cmd_ghostattach},
    { "errors",     cmd_errors     },
    { "log",        cmd_log        },
    { "logs-ai",    cmd_logs_ai    },
    { "export-logs",cmd_export_logs},
    { "interrupts", cmd_interrupts },
    { "ticks",      cmd_ticks      },
    { "tasks",      cmd_tasks      },
    { "task-current", cmd_task_current },
    { "task-yield", cmd_task_yield },
    { "task-demo",  cmd_task_demo  },
    { "apps",       cmd_apps       },
    { "app-log",    cmd_app_log    },
    { "fs",         cmd_fs         },
    { "ls",         cmd_ls         },
    { "recovery",   cmd_recovery   },
    { "actions",    cmd_actions    },
    { "suggestions",cmd_suggestions},
    { "policy",     cmd_policy     },
    { "memory",     cmd_memory     },
    { "safe-mode",  cmd_safe_mode  },
    { "store",      cmd_store      },
    { "journal",    cmd_journal    },
    { "snapshots",  cmd_snapshots  },
    { "health",     cmd_health     },
    { "timeline",   cmd_timeline   },
    { "versions",   cmd_versions   },
    { "packages",   cmd_packages   },
    { "manifests",  cmd_manifests  },
    { "slots",      cmd_slots      },
    { "trust",      cmd_trust      },
    { "schedule-safe-mode", cmd_schedule_safe_mode },
    { "reboot",     cmd_reboot     },
    { "panic-test", cmd_panic_test },
};

/*
 * Read one line from the keyboard, echoing as the user types.
 * Overflow-safe: stores at most cap-1 chars, always 0-terminates.
 */
static void read_line(char *buf, int cap)
{
    int len = 0;
    for (;;) {
        char c = keyboard_getchar();

        if (c == '\n') {                  /* Enter: line complete */
            vga_putc('\n');
            buf[len] = '\0';
            return;
        }
        if (c == '\b') {                  /* Backspace: shrink the line */
            if (len > 0) {
                len--;
                vga_putc('\b');
            }
            continue;
        }
        if (c == 27)                      /* Escape: ignored for now */
            continue;
        if (len < cap - 1) {              /* normal char, if room remains */
            buf[len++] = c;
            vga_putc(c);
        }
        /* else: buffer full — swallow the character instead of overflowing */
    }
}

void shell_run(void)
{
    char line[LINE_MAX];

    vga_print("Type `help` to see available commands.\n\n");

    for (;;) {
        vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
        vga_print("kamikaze> ");
        vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);

        read_line(line, LINE_MAX);

        if (line[0] == '\0')              /* empty line: just re-prompt */
            continue;

        /*
         * Split "command argument" at the first space. `line` keeps the
         * command (we 0-terminate at the space); `arg` points to the rest
         * with leading spaces skipped. No copying, no allocation.
         */
        char *arg = line;
        while (*arg != '\0' && *arg != ' ')
            arg++;
        if (*arg == ' ') {
            *arg++ = '\0';                /* terminate the command word */
            while (*arg == ' ')           /* skip extra spaces */
                arg++;
        }

        /* Commands that take an argument are dispatched explicitly. */
        if (kstrcmp(line, "approve") == 0)      { cmd_approve(arg);      continue; }
        if (kstrcmp(line, "rollback") == 0)     { cmd_rollback(arg);     continue; }
        if (kstrcmp(line, "clear-memory") == 0) { cmd_clear_memory(arg); continue; }
        if (kstrcmp(line, "clear-store") == 0)  { cmd_clear_store(arg);  continue; }
        if (kstrcmp(line, "package") == 0)      { cmd_package(arg);      continue; }
        if (kstrcmp(line, "approve-pkg") == 0)  { cmd_approve_pkg(arg);  continue; }
        if (kstrcmp(line, "apply-pkg") == 0)    { cmd_apply_pkg(arg);    continue; }
        if (kstrcmp(line, "verify-pkg") == 0)   { cmd_verify_pkg(arg);   continue; }
        if (kstrcmp(line, "rollback-pkg") == 0) { cmd_rollback_pkg(arg); continue; }
        if (kstrcmp(line, "seed-manifests") == 0) { cmd_seed_manifests(arg); continue; }
        if (kstrcmp(line, "reset-packages") == 0) { cmd_reset_packages(arg); continue; }
        if (kstrcmp(line, "exceptions-test") == 0) { cmd_exceptions_test(arg); continue; }
        if (kstrcmp(line, "app") == 0) { cmd_app_detail(arg); continue; }
        if (kstrcmp(line, "run") == 0) { cmd_run_app(arg); continue; }
        if (kstrcmp(line, "fs-format") == 0) { cmd_fs_format(arg); continue; }
        if (kstrcmp(line, "touch") == 0) { cmd_touch(arg); continue; }
        if (kstrcmp(line, "write") == 0) { cmd_write(arg); continue; }
        if (kstrcmp(line, "cat") == 0) { cmd_cat(arg); continue; }
        if (kstrcmp(line, "rm") == 0) { cmd_rm(arg); continue; }

        int found = 0;
        for (unsigned i = 0; i < sizeof(commands) / sizeof(commands[0]); i++) {
            if (kstrcmp(line, commands[i].name) == 0) {
                commands[i].handler();
                found = 1;
                break;
            }
        }

        if (!found) {
            /* Rule 5: unknown input is surfaced AND logged, never ignored. */
            vga_print("Unknown command: `");
            vga_print(line);
            vga_print("`. Type `help` for the command list.\n");
            error_log(SEV_WARNING, "SHL-001", "shell",
                      "Unknown command entered", "type `help` for valid commands");
        }
    }
}
