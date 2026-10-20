.intel_syntax noprefix
#include "emergency_abi.h"
.section .text
.extern emergency_entry_state
.extern emergency_entry_busy
.extern emergency_reentry_record
.extern emergency_reentry_count
.extern _fatal_stack_end
.extern _df_stack_start
.extern _df_stack_end

/* Called by fatal ISR labels and kpanic only after an atomic first-entry claim.
   No stack access or push/call occurs until ESP selects the dedicated stack. */
.global emergency_exception_common
.type emergency_exception_common,@function
emergency_exception_common:
    cld
    mov dword ptr [emergency_entry_state+16],edi
    mov dword ptr [emergency_entry_state+20],esi
    mov dword ptr [emergency_entry_state+24],ebp
    mov dword ptr [emergency_entry_state+32],ebx
    mov dword ptr [emergency_entry_state+36],edx
    mov dword ptr [emergency_entry_state+40],ecx
    mov dword ptr [emergency_entry_state+44],eax
    mov dword ptr [emergency_entry_state+EE_CPU_ESP],esp
    mov eax,cr2
    mov dword ptr [emergency_entry_state+EE_CR2],eax
    mov eax,cr0
    mov dword ptr [emergency_entry_state+EE_CR0],eax
    mov eax,cr3
    mov dword ptr [emergency_entry_state+EE_CR3],eax
    xor eax,eax
    mov ax,gs
    mov dword ptr [emergency_entry_state],eax
    mov ax,fs
    mov dword ptr [emergency_entry_state+4],eax
    mov ax,es
    mov dword ptr [emergency_entry_state+8],eax
    mov ax,ds
    mov dword ptr [emergency_entry_state+12],eax
    mov dword ptr [emergency_entry_state+28],esp
    mov dword ptr [emergency_entry_state+52],0
    mov dword ptr [emergency_entry_state+56],0
    mov dword ptr [emergency_entry_state+60],0
    mov dword ptr [emergency_entry_state+64],0
    mov dword ptr [emergency_entry_state+68],0
    mov dword ptr [emergency_entry_state+72],0
    cmp dword ptr [emergency_entry_state+EE_KIND],EM_PANIC
    jne .selectors
    xor eax,eax
    mov ax,cs
    mov dword ptr [emergency_entry_state+60],eax
.selectors:
    mov ax,0x10
    mov ds,ax
    mov es,ax
    mov fs,ax
    mov gs,ax
    mov esp,offset _fatal_stack_end
    and esp,-16
    call emergency_handle_entry
    jmp emergency_reentry_halt
.size emergency_exception_common,.-emergency_exception_common

.global kpanic
.type kpanic,@function
kpanic:
    cli
    lock bts dword ptr [emergency_entry_busy],0
    jc .panic_reentry
    mov dword ptr [emergency_entry_state+EE_VECTOR],-1
    mov dword ptr [emergency_entry_state+EE_HAS_ERROR],0
    mov dword ptr [emergency_entry_state+EE_KIND],EM_PANIC
    jmp emergency_exception_common
.panic_reentry:
    lock inc dword ptr [emergency_reentry_count]
    cmp dword ptr [emergency_reentry_record],0
    jne emergency_reentry_halt
    mov dword ptr [emergency_reentry_record+ER_VECTOR],-1
    jmp emergency_reentry_capture
.size kpanic,.-kpanic

/* Reentry never resets or overwrites the primary crash record, never dereferences
   the interrupted stack and never calls C. Its only I/O is bounded COM1 output. */
.global emergency_reentry_capture
.type emergency_reentry_capture,@function
emergency_reentry_capture:
    mov dword ptr [emergency_reentry_record+4],EM_VERSION
    mov dword ptr [emergency_reentry_record+ER_KIND],EM_REENTRANT
    mov dword ptr [emergency_reentry_record+12],64
    mov eax,cr2
    mov dword ptr [emergency_reentry_record+ER_CR2],eax
    mov eax,cr0
    mov dword ptr [emergency_reentry_record+ER_CR0],eax
    mov eax,cr3
    mov dword ptr [emergency_reentry_record+ER_CR3],eax
    mov dword ptr [emergency_reentry_record],EM_MAGIC
.size emergency_reentry_capture,.-emergency_reentry_capture
.global emergency_reentry_halt
.type emergency_reentry_halt,@function
emergency_reentry_halt:
    cli
    cld
    mov ax,0x10
    mov ds,ax
    mov esi,offset .reentry_message
    mov ecx,64
.next_char:
    mov bl,byte ptr [esi]
    test bl,bl
    jz .halt
    inc esi
    mov edi,4096
    mov dx,0x3FD
.poll:
    in al,dx
    test al,0x20
    jnz .send
    dec edi
    jnz .poll
    jmp .halt
.send:
    mov dx,0x3F8
    mov al,bl
    out dx,al
    dec ecx
    jnz .next_char
.halt:
    cli
    hlt
    jmp .halt
.size emergency_reentry_halt,.-emergency_reentry_halt

.balign 16
.global emergency_double_fault_entry
.type emergency_double_fault_entry,@function
emergency_double_fault_entry:
    cli
    cld
    mov eax,cr2
    mov dword ptr [emergency_df_cr2],eax
    mov eax,cr0
    mov dword ptr [emergency_df_cr0],eax
    mov eax,cr3
    mov dword ptr [emergency_df_cr3],eax
    lock bts dword ptr [emergency_entry_busy],0
    mov dword ptr [emergency_df_error],0
    cmp esp,offset _df_stack_start
    jb .df_stack
    mov eax,offset _df_stack_end
    sub eax,4
    cmp esp,eax
    ja .df_stack
    mov eax,dword ptr [esp] /* hardware #DF error code, on the new task stack */
    mov dword ptr [emergency_df_error],eax
.df_stack:
    mov esp,offset _df_stack_end
    and esp,-16
    call emergency_double_fault_c
    jmp emergency_reentry_halt
.size emergency_double_fault_entry,.-emergency_double_fault_entry
.section .rodata
.reentry_message:
    .asciz "[EMERGENCY_REENTRY]\r\npersistence=skipped\r\n"
.section .note.GNU-stack,"",@progbits
