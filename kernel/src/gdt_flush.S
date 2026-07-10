.intel_syntax noprefix

.global gdt_load
.type gdt_load, @function
gdt_load:
    mov eax, dword ptr [esp + 4]
    lgdt [eax]

    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    push 0x08
    lea eax, [gdt_loaded]
    push eax
    retf
gdt_loaded:
    ret
.size gdt_load, . - gdt_load

.section .note.GNU-stack,"",@progbits
