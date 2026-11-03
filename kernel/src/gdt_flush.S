.intel_syntax noprefix

.set GDT_KERNEL_CODE_SELECTOR, 0x08
.set GDT_KERNEL_DATA_SELECTOR, 0x10

.global gdt_load
.type gdt_load, @function
gdt_load:
    mov eax, dword ptr [esp + 4]
    lgdt [eax]

    mov ax, GDT_KERNEL_DATA_SELECTOR
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    push GDT_KERNEL_CODE_SELECTOR
    lea eax, [gdt_loaded]
    push eax
    retf
gdt_loaded:
    ret
.size gdt_load, . - gdt_load

.section .note.GNU-stack,"",@progbits
