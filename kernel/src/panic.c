/* panic.c — diagnostic crash screen. See panic.h. */
#include "panic.h"
#include "errors.h"
#include "interrupts.h"
#include "paging.h"
#include "recovery_memory.h"
#include "recovery_store.h"
#include "ring3.h"
#include "serial.h"
#include "vga.h"

static void serial_print_hex32(uint32_t value)
{
    static const char digits[] = "0123456789ABCDEF";
    serial_print("0x");
    for (int shift = 28; shift >= 0; shift -= 4)
        serial_putc(digits[(value >> shift) & 0xFu]);
}

static void serial_print_u32(uint32_t value)
{
    char digits[11];
    uint32_t count = 0;
    if (value == 0) {
        serial_putc('0');
        return;
    }
    while (value && count < sizeof(digits)) {
        digits[count++] = (char)('0' + (value % 10u));
        value /= 10u;
    }
    while (count)
        serial_putc(digits[--count]);
}

static void record_panic(void)
{
    recovery_memory_mark_panic();
    recovery_journal_append(JE_PANIC, 0, 0, 2, 0, "PANIC0");
}

static void halt_forever(void)
{
    for (;;)
        __asm__ volatile ("cli; hlt");
}

void kpanic(const char *code, const char *msg, const char *hint)
{
    /* 1. Record the event FIRST — diagnostics before cosmetics. This also
          streams the entry out the serial port to the host. */
    error_log(SEV_PANIC, code, "kernel", msg, hint);

    /* 2. Burn the evidence into NVRAM + disk so the NEXT boot knows this
          one died — that's what triggers the safe-mode suggestion. The
          journal gets a durable PANIC record too. */
    record_panic();

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
    halt_forever();
}

void kpanic_exception(const char *name, const struct interrupt_frame *frame)
{
    if (!frame)
        kpanic("CPU-EXCEPTION", "Exception entry supplied no saved frame",
               "halt and inspect the interrupt entry ABI");
    uint32_t vector = frame->vector;
    uint32_t error_code = frame->error_code;
    uint32_t eip = frame->eip;
    uint32_t cs = frame->cs;
    uint32_t eflags = frame->eflags;
    uint32_t fault_address = 0;
    uint32_t caller_cpl = interrupt_frame_caller_cpl(frame);
    uint32_t user_esp = 0;
    uint32_t user_ss = 0;
    int user_stack_present =
        interrupt_frame_user_stack(frame, &user_esp, &user_ss);
    if (vector == 14)
        __asm__ volatile ("mov %%cr2, %0" : "=r"(fault_address));

    __asm__ volatile ("cli");
    if (user_stack_present)
        ring3_record_user_fault(vector, fault_address, eip, user_esp);
    if (vector == 14) {
        paging_record_page_fault(fault_address, error_code, eip);
        error_log(SEV_PANIC, "PAGE-FAULT", "Paging", name,
                  "inspect CR2 fault address and error code");
    } else {
        error_log(SEV_PANIC, "CPU-EXCEPTION", "interrupts", name,
                  "inspect vector error code and instruction pointer");
    }

    serial_print("[KERNEL_EXCEPTION_PANIC]\nvector=");
    serial_print_hex32(vector);
    serial_print(";name="); serial_print(name);
    serial_print(";error_code="); serial_print_hex32(error_code);
    serial_print(";eip="); serial_print_hex32(eip);
    serial_print(";cs="); serial_print_hex32(cs);
    serial_print(";eflags="); serial_print_hex32(eflags);
    serial_print(";caller_cpl="); serial_print_u32(caller_cpl);
    serial_print(";user_stack_present=");
    serial_print(user_stack_present ? "yes" : "no");
    if (user_stack_present) {
        serial_print(";user_esp="); serial_print_hex32(user_esp);
        serial_print(";user_ss="); serial_print_hex32(user_ss);
    }
    if (vector == 14) {
        serial_print(";fault_address=");
        serial_print_hex32(fault_address);
    }
    serial_print("\n");
    if (vector == 14) {
        serial_print("[PAGE_FAULT]\npresent=yes\nfault_address=");
        serial_print_hex32(fault_address);
        serial_print("\nerror_code="); serial_print_hex32(error_code);
        serial_print("\neip="); serial_print_hex32(eip);
        serial_print("\nrecovery_attempted=no\n");
    }
    if (user_stack_present) {
        struct ring3_status ring3;
        ring3_status(&ring3);
        serial_print("[USER_FAULT]\npresent=yes\nfault_origin=cpl3");
        serial_print("\nvector="); serial_print_u32(vector);
        serial_print("\nerror_code="); serial_print_hex32(error_code);
        serial_print("\nfault_address="); serial_print_hex32(fault_address);
        serial_print("\neip="); serial_print_hex32(eip);
        serial_print("\ncs="); serial_print_hex32(cs);
        serial_print("\neflags="); serial_print_hex32(eflags);
        serial_print("\nuser_esp="); serial_print_hex32(user_esp);
        serial_print("\nuser_ss="); serial_print_hex32(user_ss);
        serial_print("\nintentional_test=");
        serial_print(ring3.last_fault_intentional ? "yes" : "no");
        serial_print("\nkernel_stack_corrupted=no\nrecovery_attempted=no\n");
    }
    record_panic();

    vga_set_color(VGA_WHITE, VGA_RED);
    vga_clear();
    vga_print("\n  ==========================================\n");
    vga_print("   KAMIKAZEOS CPU EXCEPTION\n");
    vga_print("  ==========================================\n\n");
    vga_print("  Exception     : "); vga_print(name);
    vga_print("\n  Vector        : "); vga_print_dec(vector);
    vga_print("\n  Error code    : "); vga_print_hex(error_code);
    vga_print("\n  Instruction   : "); vga_print_hex(eip);
    vga_print("\n  Caller CPL    : "); vga_print_dec(caller_cpl);
    vga_print("\n  Saved CS      : "); vga_print_hex(cs);
    vga_print("\n  Saved EFLAGS  : "); vga_print_hex(eflags);
    vga_print("\n  User stack    : ");
    vga_print(user_stack_present ? "present" : "not present (same CPL)");
    if (user_stack_present) {
        vga_print("\n  User ESP      : "); vga_print_hex(user_esp);
        vga_print("\n  User SS       : "); vga_print_hex(user_ss);
    }
    if (vector == 14) {
        vga_print("\n  Fault address : "); vga_print_hex(fault_address);
    }
    vga_print("\n\n  Structured evidence was streamed to serial and persisted.\n");
    vga_print("  CPU exceptions are diagnostic-only; the kernel halted safely.\n");
    halt_forever();
}
