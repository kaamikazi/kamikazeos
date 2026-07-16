.intel_syntax noprefix

.extern interrupt_dispatch

.section .text
.global syscall_isr
.type syscall_isr, @function
syscall_isr:
    push 0
    push 0x80
    cld
    pusha
    push esp
    call interrupt_dispatch
    add esp, 4
    popa
    add esp, 8
    iretd

.section .note.GNU-stack,"",@progbits
