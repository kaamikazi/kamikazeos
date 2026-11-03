.intel_syntax noprefix

.section .text
.global paging_enable_hardware
.type paging_enable_hardware, @function
paging_enable_hardware:
    mov eax, [esp + 4]
    mov cr3, eax
    mov eax, cr0
    or eax, 0x80000000
    mov cr0, eax
    jmp .paging_enabled
.paging_enabled:
    ret

.section .note.GNU-stack,"",@progbits
