/* Phase A: explicit page-isolated layout. Guard reservations are NOT unmapped.
   ELF flags describe intent; the existing legacy paging policy stays active. */
ENTRY(_start)
SECTIONS
{
    . = 1M;
    _kernel_start = .;
    .text : ALIGN(4K) {
        _kernel_text_start = .;
        KEEP(*(.multiboot))
        *(.text .text.*)
        . = ALIGN(4K); _kernel_text_end = .;
    }
    .rodata : ALIGN(4K) {
        _kernel_rodata_start = .; *(.rodata .rodata.*)
        . = ALIGN(4K); _kernel_rodata_end = .;
    }
    .data : ALIGN(4K) {
        _kernel_data_start = .; *(.data .data.*)
        . = ALIGN(4K); _kernel_data_end = .;
    }
    .bss (NOLOAD) : ALIGN(4K) {
        _kernel_bss_start = .; *(COMMON) *(.bss .bss.*)
        . = ALIGN(4K); _kernel_bss_end = .;
    }
    .page_directory (NOLOAD) : ALIGN(4K) {
        _page_directory_start = .; KEEP(*(.paging.directory)) _page_directory_end = .;
    }
    .page_tables (NOLOAD) : ALIGN(4K) {
        _page_tables_start = .; KEEP(*(.paging.tables)) _page_tables_end = .;
    }
    .bootstrap_stack (NOLOAD) : ALIGN(4K) {
        _bootstrap_guard_low_start = .; . += 4K; _bootstrap_guard_low_end = .;
        _bootstrap_stack_start = .; KEEP(*(.bootstrap_stack)) _bootstrap_stack_end = .;
        _bootstrap_guard_high_start = .; . += 4K; _bootstrap_guard_high_end = .;
    }
    .privilege_stack (NOLOAD) : ALIGN(4K) {
        _privilege_guard_low_start = .; . += 4K; _privilege_guard_low_end = .;
        _privilege_stack_start = .; KEEP(*(.privilege_stack)) _privilege_stack_end = .;
        _privilege_guard_high_start = .; . += 4K; _privilege_guard_high_end = .;
    }
    _kernel_privileged_end = .;
    .usertext : ALIGN(4K) {
        _user_code_start = .; KEEP(*(.usertext .usertext.*))
        . = ALIGN(4K); _user_code_end = .;
    }
    .userstack (NOLOAD) : ALIGN(4K) {
        _user_guard_low_start = .; . += 4K; _user_guard_low_end = .;
        _user_stack_start = .; KEEP(*(.userstack .userstack.*))
        . = ALIGN(4K); _user_stack_end = .;
        _user_guard_high_start = .; . += 4K; _user_guard_high_end = .;
    }
    _kernel_end = .;
    _heap_candidate_start = .;
    _heap_candidate_end = . + 64K; /* not loaded: memory_init validates RAM */

    ASSERT(_kernel_text_start == 1M, "kernel must start at 1 MiB")
    ASSERT(_kernel_text_end == _kernel_rodata_start, "text/rodata gap or overlap")
    ASSERT(_kernel_rodata_end == _kernel_data_start, "rodata/data gap or overlap")
    ASSERT(_kernel_data_end == _kernel_bss_start, "data/bss gap or overlap")
    ASSERT(_kernel_bss_end == _page_directory_start, "bss/directory gap or overlap")
    ASSERT(_page_directory_end == _page_tables_start, "directory/tables gap or overlap")
    ASSERT(SIZEOF(.page_directory) == 4K, "directory must occupy one page")
    ASSERT(SIZEOF(.page_tables) == 16K, "four page tables required")
    ASSERT(_page_tables_end == _bootstrap_guard_low_start, "tables/stack gap")
    ASSERT(stack_bottom == _bootstrap_stack_start && stack_top == _bootstrap_stack_end,
           "bootstrap assembly bounds differ")
    ASSERT(_bootstrap_stack_end - _bootstrap_stack_start == 16K, "bootstrap stack size")
    ASSERT(_bootstrap_guard_high_end == _privilege_guard_low_start, "stack region gap")
    ASSERT(_privilege_stack_end - _privilege_stack_start == 16K, "privilege stack size")
    ASSERT(_user_code_start == _kernel_privileged_end, "privileged/user boundary")
    ASSERT(_user_code_end - _user_code_start == 4K, "exactly one user code page")
    ASSERT(_user_code_end == _user_guard_low_start, "user code/guard gap")
    ASSERT(_user_stack_end - _user_stack_start == 16K, "exactly four user stack pages")
    ASSERT(ring3_user_stack_start == _user_stack_start && ring3_user_stack_end == _user_stack_end,
           "user assembly bounds differ")
    ASSERT((_kernel_end & 4095) == 0 && (_heap_candidate_end & 4095) == 0,
           "kernel/heap bounds must be page aligned")
    ASSERT(_heap_candidate_end <= 16M, "runtime layout exceeds 16 MiB aperture")
}
