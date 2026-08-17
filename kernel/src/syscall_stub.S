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

    /* Match interrupt_frame exactly with full 32-bit selector copies. */
    xor eax, eax
    mov ax, ds
    push eax
    mov ax, es
    push eax
    mov ax, fs
    push eax
    mov ax, gs
    push eax
    mov ax, GDT_KERNEL_DATA_SELECTOR
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    mov eax, esp
    push eax
    call interrupt_dispatch
    add esp, 4
    pop eax
    mov gs, ax
    pop eax
    mov fs, ax
    pop eax
    mov es, ax
    pop eax
    mov ds, ax
    popa
    add esp, 8
    iretd

.section .note.GNU-stack,"",@progbits
