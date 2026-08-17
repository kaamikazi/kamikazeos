#include "interrupts.h"
#include "errors.h"
#include "gdt.h"
#include "idt.h"
#include "keyboard.h"
#include "panic.h"
#include "pic.h"
#include "ring3.h"
#include "timer.h"
#include "task.h"
#include "syscall.h"

typedef char interrupt_frame_size_must_be_68[
    sizeof(struct interrupt_frame) == 68u ? 1 : -1];
typedef char interrupt_frame_gs_offset_must_be_0[
    __builtin_offsetof(struct interrupt_frame, gs) == 0u ? 1 : -1];
typedef char interrupt_frame_edi_offset_must_be_16[
    __builtin_offsetof(struct interrupt_frame, edi) == 16u ? 1 : -1];
typedef char interrupt_frame_vector_offset_must_be_48[
    __builtin_offsetof(struct interrupt_frame, vector) == 48u ? 1 : -1];
typedef char interrupt_frame_eip_offset_must_be_56[
    __builtin_offsetof(struct interrupt_frame, eip) == 56u ? 1 : -1];

static volatile uint32_t handled;
static volatile uint32_t last_vector = 0xFFFFFFFFu;
static volatile uint32_t last_exception = 0xFFFFFFFFu;
static volatile uint32_t last_error;
static struct interrupt_exception_evidence exception_evidence;
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

uint8_t interrupt_frame_caller_cpl(const struct interrupt_frame *frame)
{
    return frame ? (uint8_t)(frame->cs & 3u) : 0xFFu;
}

int interrupt_frame_user_stack(const struct interrupt_frame *frame,
                               uint32_t *user_esp, uint32_t *user_ss)
{
    if (!frame || interrupt_frame_caller_cpl(frame) != 3u)
        return 0;
    const struct interrupt_user_stack *tail =
        (const struct interrupt_user_stack *)((const uint8_t *)frame +
                                               sizeof(*frame));
    if (user_esp)
        *user_esp = tail->esp;
    if (user_ss)
        *user_ss = tail->ss;
    return 1;
}

int interrupt_frame_layout_self_test(void)
{
    struct {
        struct interrupt_frame frame;
        struct interrupt_user_stack user;
    } sample = {0};
    uint32_t esp = 0xAAAAAAAAu;
    uint32_t ss = 0xBBBBBBBBu;

    sample.frame.cs = 0x08u;
    sample.user.esp = 0x12345000u;
    sample.user.ss = 0x23u;
    if (interrupt_frame_caller_cpl(&sample.frame) != 0u ||
        interrupt_frame_user_stack(&sample.frame, &esp, &ss) ||
        esp != 0xAAAAAAAAu || ss != 0xBBBBBBBBu)
        return 0;

    sample.frame.cs = 0x1Bu;
    return interrupt_frame_caller_cpl(&sample.frame) == 3u &&
           interrupt_frame_user_stack(&sample.frame, &esp, &ss) &&
           esp == sample.user.esp && ss == sample.user.ss;
}

static int exception_origin_valid(const struct interrupt_frame *frame,
                                  int user_stack_present, uint32_t user_ss)
{
    uint8_t cpl = interrupt_frame_caller_cpl(frame);
    if (cpl == 0u)
        return frame->cs == GDT_KERNEL_CODE_SELECTOR && !user_stack_present;
    return cpl == 3u && frame->cs == GDT_USER_CODE_SELECTOR &&
           user_stack_present && user_ss == GDT_USER_DATA_SELECTOR;
}

void interrupt_exception_evidence(struct interrupt_exception_evidence *out)
{
    if (out)
        *out = exception_evidence;
}

void interrupts_init(void)
{
    interrupts_disable();
    handled = 0;
    last_vector = 0xFFFFFFFFu;
    last_exception = 0xFFFFFFFFu;
    last_error = 0;
    exception_evidence.present = 0;
    if (!interrupt_frame_layout_self_test()) {
        error_log(SEV_ERROR, "INT-102", "interrupts",
                  "Interrupt frame ABI self-test failed",
                  "keep Ring 3 execution disabled and inspect entry stubs");
    }
    idt_install_irq_gates();
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
        uint32_t user_esp = 0;
        uint32_t user_ss = 0;
        int user_stack_present =
            interrupt_frame_user_stack(frame, &user_esp, &user_ss);
        exception_evidence.present = 1;
        exception_evidence.vector = frame->vector;
        exception_evidence.error_code = frame->error_code;
        exception_evidence.eip = frame->eip;
        exception_evidence.cs = frame->cs;
        exception_evidence.eflags = frame->eflags;
        exception_evidence.caller_cpl = interrupt_frame_caller_cpl(frame);
        exception_evidence.user_stack_present = user_stack_present;
        exception_evidence.user_esp = user_stack_present ? user_esp : 0;
        exception_evidence.user_ss = user_stack_present ? user_ss : 0;
        exception_evidence.origin_valid =
            exception_origin_valid(frame, user_stack_present, user_ss);
        kpanic_exception(interrupt_exception_name(frame->vector), frame);
    }

    if (frame->vector == SYSCALL_VECTOR) {
        syscall_dispatch(frame);
        return;
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
