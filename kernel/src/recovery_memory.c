/* recovery_memory.c — CMOS/NVRAM-backed persistent recovery state.
 * Read recovery_memory.h first for the design and the honesty notes. */
#include "recovery_memory.h"
#include "errors.h"
#include "io.h"
#include "recovery_store.h"
#include "string.h"
#include "vga.h"

/*
 * v0.7 LAYERING: RecoveryMemory is now a two-tier system.
 *   Tier 1 (durable) : RecoveryStore on disk — survives everything,
 *                      including closing QEMU. Preferred when present.
 *   Tier 2 (fallback): CMOS NVRAM — survives in-VM reboots only.
 * Load: disk first; if absent/invalid, CMOS. Save: ALWAYS both, so the
 * tiers never disagree for long and losing the disk degrades gracefully.
 */
static const char *backend = "fresh";
static int cmos_fallback_used;

/* ------------------------------------------------------------------ */
/* CMOS access — the RTC chip's battery-backed RAM.                     */
/* Write the cell index to port 0x70, then read/write data on 0x71.    */
/* We only ever touch indexes 0x60..0x68 — far away from the clock     */
/* registers (0x00-0x0D) and the cells BIOS/QEMU are known to use.     */
/* ------------------------------------------------------------------ */

static uint8_t cmos_read(uint8_t index)
{
    outb(0x70, index);
    io_wait();
    return inb(0x71);
}

static void cmos_write(uint8_t index, uint8_t value)
{
    outb(0x70, index);
    io_wait();
    outb(0x71, value);
}

/* The persistent block layout. NEVER reorder existing fields — stored
 * bytes from an older kernel would be misread. Append + bump VERSION. */
#define CMOS_BASE    0x60
#define MAGIC0       0x4B      /* 'K' */
#define MAGIC1       0x5A      /* 'Z' */
#define VERSION      1
/* offsets within the block (CMOS index = CMOS_BASE + offset)           */
#define OFF_MAGIC0   0
#define OFF_MAGIC1   1
#define OFF_VERSION  2
#define OFF_FLAGS    3         /* bitfield, see FLAG_* below            */
#define OFF_BOOTCNT  4
#define OFF_LASTACT  5
#define OFF_PRIORNET 6
#define OFF_PRIORAUD 7
#define OFF_CHECKSUM 8         /* two's complement: block bytes sum to 0 */
#define BLOCK_SIZE   9

#define FLAG_SAFE_MODE_NEXT  (1u << 0)
#define FLAG_NET_QUARANTINE  (1u << 1)
#define FLAG_AUD_QUARANTINE  (1u << 2)
#define FLAG_LAST_PANIC      (1u << 3)
#define FLAG_ROLLBACK_AVAIL  (1u << 4)
/* v2.6.1. Older kernels ignore this bit and drop it when they save, which
   degrades safely: the panic is then treated as a real fault. */
#define FLAG_PANIC_INTENTIONAL (1u << 5)

static struct recovery_memory mem;   /* the in-RAM mirror */

/* ------------------------------------------------------------------ */
/* Load / save                                                          */
/* ------------------------------------------------------------------ */

void recovery_memory_save(void)
{
    /* rollback_available is derived, not independently set: it is true
       exactly when some reversible action is currently in effect. */
    mem.rollback_available = (mem.network_quarantined ||
                              mem.audio_quarantined ||
                              mem.safe_mode_next_boot) ? 1 : 0;

    uint8_t flags = 0;
    if (mem.safe_mode_next_boot) flags |= FLAG_SAFE_MODE_NEXT;
    if (mem.network_quarantined) flags |= FLAG_NET_QUARANTINE;
    if (mem.audio_quarantined)   flags |= FLAG_AUD_QUARANTINE;
    if (mem.last_boot_panic)     flags |= FLAG_LAST_PANIC;
    if (mem.rollback_available)  flags |= FLAG_ROLLBACK_AVAIL;
    if (mem.last_boot_panic && mem.last_panic_intentional)
        flags |= FLAG_PANIC_INTENTIONAL;

    uint8_t block[BLOCK_SIZE];
    block[OFF_MAGIC0]   = MAGIC0;
    block[OFF_MAGIC1]   = MAGIC1;
    block[OFF_VERSION]  = VERSION;
    block[OFF_FLAGS]    = flags;
    block[OFF_BOOTCNT]  = mem.boot_count;
    block[OFF_LASTACT]  = mem.last_action_id;
    block[OFF_PRIORNET] = mem.prior_network_status;
    block[OFF_PRIORAUD] = mem.prior_audio_status;

    /* Checksum: pick the byte that makes everything sum to 0 (mod 256).
       A single corrupted cell then fails validation on load. */
    uint8_t sum = 0;
    for (int i = 0; i < OFF_CHECKSUM; i++)
        sum = (uint8_t)(sum + block[i]);
    block[OFF_CHECKSUM] = (uint8_t)(0u - sum);

    for (int i = 0; i < BLOCK_SIZE; i++)
        cmos_write((uint8_t)(CMOS_BASE + i), block[i]);

    /* Tier 1: mirror to the durable disk store (no-op without a disk). */
    if (mem.boot_order_trusted)
        recovery_store_save(&mem);
}

void recovery_memory_load(void)
{
    uint8_t block[BLOCK_SIZE];
    uint8_t sum = 0;
    for (int i = 0; i < BLOCK_SIZE; i++) {
        block[i] = cmos_read((uint8_t)(CMOS_BASE + i));
        sum = (uint8_t)(sum + block[i]);
    }

    kmemset(&mem, 0, sizeof(mem));

    /* Trust NOTHING unless magic, version, and checksum all agree. A
       fresh VM, an older kernel's block, or corruption all land here. */
    if (block[OFF_MAGIC0] != MAGIC0 || block[OFF_MAGIC1] != MAGIC1 ||
        block[OFF_VERSION] != VERSION || sum != 0) {
        mem.valid_at_boot = 0;
        return;
    }

    uint8_t flags = block[OFF_FLAGS];
    mem.safe_mode_next_boot  = (flags & FLAG_SAFE_MODE_NEXT) ? 1 : 0;
    mem.network_quarantined  = (flags & FLAG_NET_QUARANTINE) ? 1 : 0;
    mem.audio_quarantined    = (flags & FLAG_AUD_QUARANTINE) ? 1 : 0;
    mem.last_boot_panic      = (flags & FLAG_LAST_PANIC) ? 1 : 0;
    mem.rollback_available   = (flags & FLAG_ROLLBACK_AVAIL) ? 1 : 0;
    mem.last_panic_intentional = (mem.last_boot_panic &&
                                  (flags & FLAG_PANIC_INTENTIONAL)) ? 1 : 0;
    mem.boot_count           = block[OFF_BOOTCNT];
    mem.last_action_id       = block[OFF_LASTACT];
    mem.prior_network_status = block[OFF_PRIORNET];
    mem.prior_audio_status   = block[OFF_PRIORAUD];
    mem.valid_at_boot        = 1;

    /* Remember "the previous boot panicked" for the WHOLE session, even
       after mark_clean_boot() clears the stored flag — the shell and the
       report must keep showing the evidence until the next boot. */
    mem.prev_panic_detected = mem.last_boot_panic;
    mem.prev_panic_intentional = mem.last_panic_intentional;
}

void recovery_memory_init(void)
{
    /* Tier 2 first: read CMOS (cheap, always present). */
    recovery_memory_load();
    int cmos_valid = mem.valid_at_boot;

    /* Tier 1: probe the disk and try the durable store. */
    recovery_store_init();
    if (recovery_store_valid()) {
        /* Disk wins: hydrate from the durable state block. */
        recovery_store_load(&mem);
        mem.valid_at_boot = 1;
        mem.prev_panic_detected = mem.last_boot_panic;
        mem.prev_panic_intentional = mem.last_boot_panic &&
                                     mem.last_panic_intentional;
        backend = "disk+CMOS";
        cmos_fallback_used = 0;
    } else if (recovery_store_available()) {
        /* Disk exists but holds no valid state (fresh image, or both
           copies damaged). Whatever CMOS knew carries over and the first
           save makes it durable. Boot numbers must keep moving forward
           past every boot the journal already recorded, or new evidence
           would be filed under an old boot's number. */
        backend = "disk+CMOS";
        cmos_fallback_used = cmos_valid;
        if (recovery_store_max_journal_boot() > mem.boot_count)
            mem.boot_count = recovery_store_max_journal_boot();
    } else {
        backend = cmos_valid ? "CMOS_only" : "fresh";
        cmos_fallback_used = cmos_valid;
        if (!cmos_valid)
            backend = "fresh";
    }

    /* Readable journal evidence is a lower bound even when state is locked.
       Never turn that diagnostic bound into permission to persist. */
    if (recovery_store_boot_lower_bound() > mem.boot_count)
        mem.boot_count = recovery_store_boot_lower_bound();
    struct recovery_store_integrity integrity;
    recovery_store_integrity(&integrity);
    mem.boot_order_trusted = recovery_store_available() &&
        integrity.journal_scan_valid && !integrity.state_locked &&
        !integrity.journal_locked && mem.boot_count < 255;
    if (mem.boot_count < 255)
        mem.boot_count++;        /* saturate rather than reuse boot zero */
    recovery_memory_save();      /* writes BOTH tiers */
}

void recovery_memory_clear(void)
{
    uint8_t keep_count = mem.boot_count;   /* boot count survives a clear */
    uint8_t keep_order = mem.boot_order_trusted;
    kmemset(&mem, 0, sizeof(mem));
    mem.boot_count = keep_count;
    mem.boot_order_trusted = keep_order;
    recovery_memory_save();
    error_log(SEV_INFO, "REC-MEM-100", "RecoveryMemory",
              "Recovery memory cleared by user", "all persisted flags reset");
}

/* ------------------------------------------------------------------ */
/* Mutators (each saves immediately — a panic can come at any time)     */
/* ------------------------------------------------------------------ */

void recovery_memory_set_safe_mode_next_boot(int enabled)
{
    mem.safe_mode_next_boot = enabled ? 1 : 0;
    recovery_memory_save();
}

/* Map "ACT-001"-style ids to the single byte we persist. */
static uint8_t action_byte(const char *action_id)
{
    if (kstricmp(action_id, "ACT-001") == 0) return 1;
    if (kstricmp(action_id, "ACT-002") == 0) return 2;
    if (kstricmp(action_id, "ACT-003") == 0) return 3;
    if (kstricmp(action_id, "ACT-004") == 0) return 4;
    return 0;
}

void recovery_memory_mark_action_applied(const char *action_id,
                                         enum device_status prior)
{
    uint8_t b = action_byte(action_id);
    if (b == 1) { mem.network_quarantined = 1;
                  mem.prior_network_status = (uint8_t)prior; }
    if (b == 2) { mem.audio_quarantined = 1;
                  mem.prior_audio_status = (uint8_t)prior; }
    mem.last_action_id = b;
    recovery_memory_save();
}

void recovery_memory_mark_action_rolled_back(const char *action_id)
{
    uint8_t b = action_byte(action_id);
    if (b == 1) mem.network_quarantined = 0;
    if (b == 2) mem.audio_quarantined = 0;
    mem.last_action_id = b;
    recovery_memory_save();
}

void recovery_memory_mark_panic(int intentional)
{
    mem.last_boot_panic = 1;
    mem.last_panic_intentional = intentional ? 1 : 0;
    recovery_memory_save();      /* MUST hit NVRAM before the CPU halts */
}

void recovery_memory_mark_clean_boot(void)
{
    /* Clears only the STORED flag. prev_panic_detected (runtime) stays,
       so this session keeps reporting the evidence. Call this only after
       the panic has been logged and exported — see kmain()'s ordering. */
    mem.last_boot_panic = 0;
    mem.last_panic_intentional = 0;
    recovery_memory_save();
}

void recovery_memory_set_safe_mode_active(int active)
{
    mem.safe_mode_active = active ? 1 : 0;
}

const struct recovery_memory *recovery_memory_get(void)
{
    return &mem;
}

const char *recovery_memory_backend(void)
{
    return backend;
}

int recovery_memory_used_cmos_fallback(void)
{
    return cmos_fallback_used;
}

/* ------------------------------------------------------------------ */
/* Human view (`memory` command)                                        */
/* ------------------------------------------------------------------ */

static void print_flag(const char *label, int value)
{
    vga_print("  ");
    vga_print(label);
    vga_set_color(value ? VGA_YELLOW : VGA_LIGHT_GREY, VGA_BLACK);
    vga_print(value ? "yes" : "no");
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
    vga_print("\n");
}

void recovery_memory_print(void)
{
    vga_set_color(VGA_LIGHT_CYAN, VGA_BLACK);
    vga_print("RecoveryMemory - persistent recovery state:\n\n");
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);

    vga_print("  persistence backend      : ");
    vga_set_color(VGA_LIGHT_CYAN, VGA_BLACK);
    vga_print(backend);
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
    if (cmos_fallback_used)
        vga_print("  (hydrated from CMOS fallback)");
    vga_print("\n");

    vga_print("  boot count               : ");
    vga_print_dec(mem.boot_count);
    vga_print(mem.valid_at_boot ? "  (memory restored from NVRAM)\n"
                                : "  (fresh memory - none found at boot)\n");
    print_flag("safe mode active NOW     : ", mem.safe_mode_active);
    print_flag("safe mode next boot      : ", mem.safe_mode_next_boot);
    print_flag("network quarantined      : ", mem.network_quarantined);
    print_flag("audio quarantined        : ", mem.audio_quarantined);
    print_flag("previous boot panicked   : ", mem.prev_panic_detected);
    print_flag("rollback available       : ", mem.rollback_available);

    vga_print("  last action              : ");
    switch (mem.last_action_id) {
    case 1:  vga_print("ACT-001\n"); break;
    case 2:  vga_print("ACT-002\n"); break;
    case 3:  vga_print("ACT-003\n"); break;
    case 4:  vga_print("ACT-004\n"); break;
    default: vga_print("none\n");    break;
    }

    vga_set_color(VGA_DARK_GREY, VGA_BLACK);
    if (recovery_store_available()) {
        vga_print("\nPersistence: disk-backed RecoveryStore (survives full QEMU\n");
        vga_print("restarts) + CMOS NVRAM fallback. See `store` and `journal`.\n");
    } else {
        vga_print("\nPersistence: CMOS NVRAM only - survives reboots inside this QEMU\n");
        vga_print("session, NOT closing QEMU. Attach build/recovery.img for the\n");
        vga_print("durable disk store (make run does this automatically).\n");
    }
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
}
