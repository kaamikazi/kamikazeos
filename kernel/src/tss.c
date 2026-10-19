#include "tss.h"
#include "errors.h"
#include "gdt.h"
#include "string.h"

struct tss32 {
    uint32_t previous;
    uint32_t esp0;
    uint32_t ss0;
    uint32_t esp1;
    uint32_t ss1;
    uint32_t esp2;
    uint32_t ss2;
    uint32_t cr3;
    uint32_t eip;
    uint32_t eflags;
    uint32_t eax, ecx, edx, ebx;
    uint32_t esp, ebp, esi, edi;
    uint32_t es, cs, ss, ds, fs, gs;
    uint32_t ldt;
    uint16_t trap;
    uint16_t io_map_base;
} __attribute__((packed));

typedef char tss32_size_must_be_104[(sizeof(struct tss32) == 104u) ? 1 : -1];

static struct tss32 kernel_tss __attribute__((aligned(16)));
static uint8_t privilege_stack[TSS_PRIVILEGE_STACK_SIZE]
    __attribute__((section(".privilege_stack"), aligned(4096)));
static int initialized;
static int loaded;

static uint16_t read_task_register(void)
{
    uint16_t selector = 0;
    __asm__ volatile ("str %0" : "=r"(selector));
    return selector;
}

static void load_task_register(uint16_t selector)
{
    __asm__ volatile ("ltr %0" : : "r"(selector) : "memory");
}

uint32_t tss_privilege_stack_bottom(void)
{
    return (uint32_t)&privilege_stack[0];
}

uint32_t tss_privilege_stack_top(void)
{
    return (uint32_t)&privilege_stack[TSS_PRIVILEGE_STACK_SIZE];
}

int tss_kernel_stack_valid(void)
{
    uint32_t bottom = tss_privilege_stack_bottom();
    uint32_t top = tss_privilege_stack_top();
    return initialized && kernel_tss.ss0 == GDT_KERNEL_DATA_SELECTOR &&
           kernel_tss.esp0 > bottom && kernel_tss.esp0 <= top &&
           (kernel_tss.esp0 & 0x0Fu) == 0;
}

int tss_set_esp0(uint32_t esp0)
{
    if (esp0 <= tss_privilege_stack_bottom() ||
        esp0 > tss_privilege_stack_top() || (esp0 & 0x0Fu) != 0)
        return 0;
    kernel_tss.esp0 = esp0;
    return 1;
}

void tss_init(void)
{
    initialized = 0;
    loaded = 0;
    kmemset(&kernel_tss, 0, sizeof(kernel_tss));
    kmemset(privilege_stack, 0, sizeof(privilege_stack));
    kernel_tss.ss0 = GDT_KERNEL_DATA_SELECTOR;
    kernel_tss.io_map_base = (uint16_t)sizeof(kernel_tss);
    initialized = 1;

    if (!tss_set_esp0(tss_privilege_stack_top()) ||
        !gdt_install_tss_descriptor((uint32_t)&kernel_tss,
                                    (uint32_t)sizeof(kernel_tss) - 1u)) {
        error_log(SEV_ERROR, "TSS-101", "TSS",
                  "TSS descriptor or esp0 validation failed",
                  "keep user syscall gate disabled");
        return;
    }

    load_task_register(GDT_TSS_SELECTOR);
    loaded = read_task_register() == GDT_TSS_SELECTOR &&
             gdt_tss_descriptor_valid() && tss_kernel_stack_valid();
    error_log(loaded ? SEV_INFO : SEV_ERROR,
              loaded ? "TSS-000" : "TSS-102", "TSS",
              loaded ? "32-bit TSS loaded for privilege stack selection"
                     : "Task register did not retain the kernel TSS",
              loaded ? "hardware task switching remains disabled"
                     : "keep user syscall gate disabled");
}

int tss_is_loaded(void)
{
    loaded = loaded && read_task_register() == GDT_TSS_SELECTOR;
    return loaded;
}

void tss_status(struct tss_status *out)
{
    if (!out)
        return;
    out->initialized = initialized;
    out->descriptor_valid = gdt_tss_descriptor_valid();
    out->loaded = tss_is_loaded();
    out->kernel_stack_valid = tss_kernel_stack_valid();
    out->selector = GDT_TSS_SELECTOR;
    out->ss0 = (uint16_t)kernel_tss.ss0;
    out->esp0 = kernel_tss.esp0;
    out->stack_bottom = tss_privilege_stack_bottom();
    out->stack_top = tss_privilege_stack_top();
    out->stack_size = TSS_PRIVILEGE_STACK_SIZE;
    out->guard_policy = "bounds_checked_no_guard_page";
}
