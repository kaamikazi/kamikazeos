#include "interrupts.h"
#include "errors.h"
#include "idt.h"
#include "keyboard.h"
#include "panic.h"
#include "pic.h"
#include "timer.h"
#include "task.h"

static volatile uint32_t handled;
static volatile uint32_t last_vector = 0xFFFFFFFFu;
static volatile uint32_t last_exception = 0xFFFFFFFFu;
static volatile uint32_t last_error;
static int enabled;

static const char *const exception_names[32] = {
    "Divide by zero", "Debug", "Non-maskable interrupt", "Breakpoint",
    "Overflow", "Bound range exceeded", "Invalid opcode", "Device unavailable",
    "Double fault", "Coprocessor segment overrun", "Invalid TSS",
    "Segment not present", "Stack-segment fault", "General protection fault",
    "Page fault", "Reserved", "x87 floating-point exception", "Alignment check",
    "Machine check", "SIMD floating-point exception", "Virtualization exception",
    "Control protection exception", "Reserved", "Reserved", "Reserved", "Reserved",
    "Reserved", "Reserved", "Hypervisor injection exception", "VMM communication exception",
    "Security exception", "Reserved"
};

const char *interrupt_exception_name(uint32_t vector)
{
    return vector < 32 ? exception_names[vector] : "Unknown exception";
}

void interrupts_init(void)
{
    interrupts_disable();
    handled = 0;
    last_vector = 0xFFFFFFFFu;
    last_exception = 0xFFFFFFFFu;
    last_error = 0;
    idt_init();
    pic_init();
    timer_init(100);
    error_log(SEV_INFO, "INT-000", "interrupts",
              "IDT loaded PIC remapped PIT configured", "no action needed");
}

void interrupts_enable(void)
{
    enabled = 1;
    __asm__ volatile ("sti");
}

void interrupts_disable(void)
{
    __asm__ volatile ("cli");
    enabled = 0;
}

int interrupts_are_enabled(void)
{
    return enabled;
}

void interrupt_dispatch(struct interrupt_frame *frame)
{
    handled++;
    last_vector = frame->vector;

    if (frame->vector < 32) {
        last_exception = frame->vector;
        last_error = frame->error_code;
        kpanic_exception(frame->vector, frame->error_code,
                         interrupt_exception_name(frame->vector), frame->eip);
    }

    if (frame->vector == 32) {
        timer_handle_irq();
        task_tick();
    } else if (frame->vector == 33)
        keyboard_handle_irq();

    if (frame->vector >= 32 && frame->vector <= 47)
        pic_send_eoi((uint8_t)(frame->vector - 32));
}

uint32_t interrupts_handled(void) { return handled; }
uint32_t interrupts_last_vector(void) { return last_vector; }
uint32_t interrupts_last_exception(void) { return last_exception; }
uint32_t interrupts_last_error_code(void) { return last_error; }

void interrupts_trigger_divide_test(void)
{
    uint32_t dividend = 1;
    uint32_t remainder = 0;
    uint32_t divisor = 0;
    __asm__ volatile ("divl %2"
                      : "+a"(dividend), "+d"(remainder)
                      : "r"(divisor)
                      : "cc");
}

void interrupts_trigger_invalid_opcode_test(void)
{
    __asm__ volatile ("ud2");
}
