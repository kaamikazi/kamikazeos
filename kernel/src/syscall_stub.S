.intel_syntax noprefix

.extern interrupt_dispatch

.set GDT_KERNEL_DATA_SELECTOR, 0x10

.section .text
.global syscall_isr
.type syscall_isr, @function
syscall_isr:
    push 0
    push 0x80
    cld
    pusha

    /* Preserve caller data selectors. A future CPL3 caller enters on the
       TSS-selected stack but retains its user data selectors until we load
       the kernel data selector for the C dispatcher. */
    push ds
    push es
    push fs
    push gs
    mov ax, GDT_KERNEL_DATA_SELECTOR
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    /* Segment-register pushes are 16-bit in this encoding: four saves use
       eight bytes, so the normalized interrupt_frame begins at esp+8. */
    lea eax, [esp + 8]
    push eax
    call interrupt_dispatch
    add esp, 4
    pop gs
    pop fs
    pop es
    pop ds
    popa
    add esp, 8
    iretd

.section .note.GNU-stack,"",@progbits
