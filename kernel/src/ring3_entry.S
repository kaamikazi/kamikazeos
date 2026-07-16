.intel_syntax noprefix

.set GDT_KERNEL_DATA_SELECTOR, 0x10
.set GDT_USER_CODE_SELECTOR,   0x1B
.set GDT_USER_DATA_SELECTOR,   0x23

.section .bss
.align 4
.global ring3_saved_kernel_esp
ring3_saved_kernel_esp:
    .long 0

.section .text
.global ring3_enter_user
.type ring3_enter_user, @function
ring3_enter_user:
    pushfd
    pusha
    mov dword ptr [ring3_saved_kernel_esp], esp

    /* cdecl arguments are 40 and 44 bytes above ESP after pushfd+pusha. */
    mov edx, dword ptr [esp + 40]  /* user EIP */
    mov ecx, dword ptr [esp + 44]  /* user ESP */
    push GDT_USER_DATA_SELECTOR
    push ecx
    push 0x202                     /* IF enabled, IOPL 0, deterministic */
    push GDT_USER_CODE_SELECTOR
    push edx

    mov ax, GDT_USER_DATA_SELECTOR
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    iretd

.global ring3_return_to_kernel
.type ring3_return_to_kernel, @function
ring3_return_to_kernel:
    cli
    mov ax, GDT_KERNEL_DATA_SELECTOR
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov esp, dword ptr [ring3_saved_kernel_esp]
    popa
    popfd
    ret

/* Fixed, build-time CPL3 routines. These pages are never loaded from disk. */
.section .usertext,"ax",@progbits
.balign 4096
.global ring3_user_test_start
.type ring3_user_test_start, @function
ring3_user_test_start:
    mov eax, 2                  /* SYS_GET_TICKS */
    xor ebx, ebx
    xor ecx, ecx
    xor edx, edx
    int 0x80
    test eax, eax
    jne ring3_user_test_failed
    xor ebx, ebx               /* success result */
    xor ecx, ecx
    xor edx, edx
    mov eax, 9                 /* SYS_USER_EXIT */
    int 0x80
    ud2
ring3_user_test_failed:
    mov ebx, 1
    xor ecx, ecx
    xor edx, edx
    mov eax, 9
    int 0x80
    ud2
.size ring3_user_test_start, . - ring3_user_test_start

.global ring3_user_fault_start
.type ring3_user_fault_start, @function
ring3_user_fault_start:
    mov eax, dword ptr [0x00100000] /* supervisor-only kernel text */
    mov ebx, 2
    mov eax, 9
    int 0x80
    ud2
.size ring3_user_fault_start, . - ring3_user_fault_start

.section .userstack,"aw",@nobits
.balign 4096
.global ring3_user_stack_start
ring3_user_stack_start:
    .skip 16384
.global ring3_user_stack_end
ring3_user_stack_end:

.section .note.GNU-stack,"",@progbits
