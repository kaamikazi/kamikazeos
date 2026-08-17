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

/*
 * Ring 0 ABI regression: EAX and EDX are documented syscall outputs. This
 * helper verifies every other GPR, ESP, and all data selectors survive the
 * interrupt round trip. It never changes privilege level.
 */
.global syscall_register_preservation_test_asm
.type syscall_register_preservation_test_asm, @function
syscall_register_preservation_test_asm:
    push ebp
    push ebx
    push esi
    push edi
    sub esp, 8
    mov dword ptr [esp], esp
    mov dword ptr [esp + 4], 1

    mov ebp, 0x13579BDF
    xor ebx, ebx
    xor ecx, ecx
    mov esi, 0x2468ACE0
    mov edi, 0x55AA55AA
    xor edx, edx
    mov eax, 2                 /* SYS_GET_TICKS */
    int 0x80

    cmp eax, 0                /* SYSCALL_OK */
    jne syscall_preservation_failed
    cmp ebx, 0
    jne syscall_preservation_failed
    cmp ecx, 0
    jne syscall_preservation_failed
    cmp esi, 0x2468ACE0
    jne syscall_preservation_failed
    cmp edi, 0x55AA55AA
    jne syscall_preservation_failed
    cmp ebp, 0x13579BDF
    jne syscall_preservation_failed
    cmp esp, dword ptr [esp]
    jne syscall_preservation_failed
    mov ax, ds
    cmp ax, GDT_KERNEL_DATA_SELECTOR
    jne syscall_preservation_failed
    mov ax, es
    cmp ax, GDT_KERNEL_DATA_SELECTOR
    jne syscall_preservation_failed
    mov ax, fs
    cmp ax, GDT_KERNEL_DATA_SELECTOR
    jne syscall_preservation_failed
    mov ax, gs
    cmp ax, GDT_KERNEL_DATA_SELECTOR
    jne syscall_preservation_failed
    jmp syscall_preservation_done

syscall_preservation_failed:
    mov dword ptr [esp + 4], 0
syscall_preservation_done:
    mov eax, dword ptr [esp + 4]
    add esp, 8
    pop edi
    pop esi
    pop ebx
    pop ebp
    ret
.size syscall_register_preservation_test_asm, . - syscall_register_preservation_test_asm

.section .note.GNU-stack,"",@progbits
