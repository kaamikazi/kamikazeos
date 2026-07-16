.intel_syntax noprefix

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
    mov eax, 9                 /* SYS_USER_EXIT */
    int 0x80
    ud2
ring3_user_test_failed:
    mov ebx, 1
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
