/* report.c — emit the machine-readable boot report over serial.
 *
 * Format contract (the Python analyzer in ai-recovery/ depends on this):
 *   - the report sits between [KAMIKAZEOS_BOOT_REPORT_START] and ..._END]
 *   - section headers are [DEVICES], [DRIVERS], [ERROR_LOG] on their own line
 *   - data lines are key=value pairs joined with ';' — same convention as
 *     the live log stream, and the same rule: values never contain ';'/'='.
 */
#include "report.h"
#include "app.h"
#include "ata.h"
#include "boot_snapshot.h"
#include "errors.h"
#include "ghostattach.h"
#include "gdt.h"
#include "idt.h"
#include "interrupts.h"
#include "kheap.h"
#include "keyboard.h"
#include "memory.h"
#include "module_slots.h"
#include "module_versions.h"
#include "package_manifest.h"
#include "package_state.h"
#include "paging.h"
#include "recovery_package.h"
#include "recovery_actions.h"
#include "recovery_memory.h"
#include "recovery_store.h"
#include "serial.h"
#include "string.h"
#include "syscall.h"
#include "pic.h"
#include "privilege.h"
#include "timer.h"
#include "task.h"
#include "tinyfs.h"
#include "tss.h"

#define KMZ_VERSION "2.4.0"

/* serial_print for numbers: render via kitoa, then send. */
static void serial_print_dec(uint32_t value)
{
    char buf[12];
    kitoa(value, buf, 10);
    serial_print(buf);
}

static void serial_print_pkg_ref(uint8_t id)
{
    serial_print("PKG-");
    if (id < 100)
        serial_print("0");
    if (id < 10)
        serial_print("0");
    serial_print_dec(id);
}

static void emit_kv(const char *key, const char *value)
{
    serial_print(key);
    serial_print("=");
    serial_print(value);
}

static int report_last_critical(struct kerror *out)
{
    struct kerror e;
    int found = 0;
    for (uint32_t i = 0; error_get(i, &e); i++) {
        if (e.severity >= SEV_ERROR) {
            if (out)
                *out = e;
            found = 1;
        }
    }
    return found;
}

static const char *report_next_command(void)
{
    if (module_slots_pending_verify())
        return "verify-pkg <id>";
    for (uint32_t i = 0; i < recovery_action_count(); i++) {
        const struct recovery_action *a = recovery_action(i);
        if (a->status != ACTST_SUGGESTED)
            continue;
        if (a->action_id[0])
            return a->action_id;
    }
    return "none";
}

void report_send(void)
{
    serial_print("[KAMIKAZEOS_BOOT_REPORT_START]\n");
    emit_kv("version", KMZ_VERSION);
    serial_print("\n");

    /* Keep summary fields in report metadata before the first section. */
    struct ghostattach_stats st;
    ghostattach_get_stats(&st);
    serial_print("scan_total=");    serial_print_dec(st.total);
    serial_print(";scan_attached=");serial_print_dec(st.attached);
    serial_print(";scan_fallback=");serial_print_dec(st.fallback);
    serial_print(";scan_missing="); serial_print_dec(st.missing);
    serial_print(";scan_isolated=");serial_print_dec(st.isolated);
    serial_print(";scan_expected_self_tests=");
    serial_print_dec(st.expected_test_failures);
    serial_print("\n");

    struct health_breakdown system_health;
    boot_snapshot_health_now(&system_health);
    const struct tinyfs_status_info *fs_status = tinyfs_status();
    const struct memory_status *mem_status = memory_status();
    struct page_status page_status;
    memory_pages(&page_status);
    struct kheap_stats heap_status;
    kheap_stats(&heap_status);
    const struct paging_status *pg_status = paging_status();
    struct syscall_stats syscall_status;
    syscall_stats(&syscall_status);
    struct privilege_status privilege_state;
    privilege_status(&privilege_state);
    struct gdt_status gdt_state;
    gdt_status(&gdt_state);
    struct tss_status tss_state;
    tss_status(&tss_state);
    struct kerror last_critical;

    serial_print("[SYSTEM_STATUS]\n");
    serial_print("kernel_version="); serial_print(module_version_of(MOD_KERNEL));
    serial_print("\ngdt_loaded="); serial_print(gdt_is_loaded() ? "yes" : "no");
    serial_print("\nidt_loaded="); serial_print(idt_is_loaded() ? "yes" : "no");
    serial_print("\nirq_gates_installed="); serial_print(idt_irq_gates_installed() ? "yes" : "no");
    serial_print("\npic_remapped="); serial_print(pic_is_remapped() ? "yes" : "no");
    serial_print("\ntimer_ticks="); serial_print_dec(timer_ticks());
    serial_print("\nkeyboard_mode="); serial_print(keyboard_mode_name());
    serial_print("\nmemory_map="); serial_print(mem_status->map_available ? "multiboot" : "fallback");
    serial_print("\nfree_pages="); serial_print_dec(page_status.free_pages);
    serial_print("\nheap_available="); serial_print(heap_status.available ? "yes" : "no");
    serial_print("\nheap_remaining="); serial_print_dec(heap_status.remaining_bytes);
    serial_print("\npaging_prepared="); serial_print(pg_status->prepared ? "yes" : "no");
    serial_print("\npaging_enabled="); serial_print(pg_status->enabled ? "yes" : "no");
    serial_print("\npaging_mode="); serial_print(pg_status->mode);
    serial_print("\nsyscall_initialized="); serial_print(syscall_status.initialized ? "yes" : "no");
    serial_print("\nsyscall_vector="); serial_print_dec(syscall_status.vector);
    serial_print("\nsyscall_total_calls="); serial_print_dec(syscall_status.total_calls);
    serial_print("\nsyscall_failed_calls="); serial_print_dec(syscall_status.failed_calls);
    serial_print("\nsyscall_gate_dpl="); serial_print_dec(syscall_status.gate_dpl);
    serial_print("\nprivilege_foundation="); serial_print(privilege_state.foundation_ready ? "ready" : "not_ready");
    serial_print("\nring3_execution_enabled=no");
    serial_print("\ntask_count="); serial_print_dec(task_count());
    serial_print("\ntinyfs_status=");
    serial_print(fs_status->available ? (fs_status->formatted ? "formatted" : "not_formatted") : "unavailable");
    serial_print("\ntinyfs_files="); serial_print_dec(fs_status->file_count);
    serial_print("\napp_registry="); serial_print_dec(app_count());
    serial_print("\nrecovery_backend="); serial_print(recovery_memory_backend());
    serial_print("\nhealth_score="); serial_print_dec((uint32_t)system_health.score);
    serial_print("\nexpected_self_test_failures=");
    serial_print_dec(st.expected_test_failures);
    serial_print("\npackage_trust_mode=host_ed25519_real_kernel_simulated");
    serial_print("\nlast_critical=");
    if (report_last_critical(&last_critical)) {
        serial_print_field(last_critical.code);
        serial_print(":");
        serial_print_field(last_critical.component);
    } else {
        serial_print("none");
    }
    serial_print("\n");

    serial_print("[DIAGNOSTICS_SUMMARY]\n");
    serial_print("devices=total:"); serial_print_dec(st.total);
    serial_print(",attached:"); serial_print_dec(st.attached);
    serial_print(",fallback:"); serial_print_dec(st.fallback);
    serial_print(",missing:"); serial_print_dec(st.missing);
    serial_print(",isolated:"); serial_print_dec(st.isolated);
    serial_print(",expected_test:"); serial_print_dec(st.expected_test_failures);
    serial_print("\ndrivers=verified_policy_active");
    serial_print("\nmemory=map:");
    serial_print(mem_status->map_available ? "multiboot" : "fallback");
    serial_print(",usable_kib:"); serial_print_dec(mem_status->usable_kib);
    serial_print(",free_pages:"); serial_print_dec(page_status.free_pages);
    serial_print("\nheap=available:");
    serial_print(heap_status.available ? "yes" : "no");
    serial_print(",used:"); serial_print_dec(heap_status.used_bytes);
    serial_print(",remaining:"); serial_print_dec(heap_status.remaining_bytes);
    serial_print(",free_supported:no");
    serial_print("\npaging=prepared:");
    serial_print(pg_status->prepared ? "yes" : "no");
    serial_print(",enabled:"); serial_print(pg_status->enabled ? "yes" : "no");
    serial_print(",identity:"); serial_print(pg_status->identity_map_valid ? "yes" : "no");
    serial_print(",mode:"); serial_print(pg_status->mode);
    serial_print("\nsyscalls=initialized:");
    serial_print(syscall_status.initialized ? "yes" : "no");
    serial_print(",registered:"); serial_print_dec(syscall_status.registered_count);
    serial_print(",failed:"); serial_print_dec(syscall_status.failed_calls);
    serial_print("\ntasks=total:"); serial_print_dec(task_count());
    serial_print(",ready:"); serial_print_dec(task_count_state(TASK_READY));
    serial_print(",crashed:"); serial_print_dec(task_count_state(TASK_CRASHED));
    serial_print("\ntinyfs=status:");
    serial_print(fs_status->formatted ? "formatted" : "not_ready");
    serial_print(",files:"); serial_print_dec(fs_status->file_count);
    serial_print(",last_error:"); serial_print(fs_status->last_error);
    serial_print("\napps=total:"); serial_print_dec(app_count());
    serial_print(",blocked_attempts:"); serial_print_dec(app_blocked_attempts());
    serial_print("\nrecovery=backend:"); serial_print(recovery_memory_backend());
    serial_print(",journal:"); serial_print_dec(recovery_journal_count());
    serial_print("\npackages=manifests:"); serial_print_dec(manifest_count());
    serial_print(",pending_verify:"); serial_print(module_slots_pending_verify() ? "yes" : "no");
    serial_print("\nhealth=score:"); serial_print_dec((uint32_t)system_health.score);
    serial_print("\nrecommended_command=");
    const char *next = report_next_command();
    if (next[0] == 'A')
        serial_print("approve ");
    serial_print(next);
    serial_print("\nauto_repair=no\n");

    serial_print("[MEMORY]\n");
    serial_print("map_available="); serial_print(mem_status->map_available ? "yes" : "no");
    serial_print("\nfallback_used="); serial_print(mem_status->fallback_used ? "yes" : "no");
    serial_print("\nregion_count="); serial_print_dec(mem_status->region_count);
    serial_print("\ntotal_kib="); serial_print_dec(mem_status->total_kib);
    serial_print("\nusable_kib="); serial_print_dec(mem_status->usable_kib);
    serial_print("\nreserved_kib="); serial_print_dec(mem_status->reserved_kib);
    serial_print("\nkernel_start="); serial_print_dec(mem_status->kernel_start);
    serial_print("\nkernel_end="); serial_print_dec(mem_status->kernel_end);
    serial_print("\nheap_start="); serial_print_dec(mem_status->heap_start);
    serial_print("\nheap_end="); serial_print_dec(mem_status->heap_end);
    serial_print("\nallocation_mode="); serial_print(mem_status->allocation_mode);
    serial_print("\nvirtual_memory=no\nheap_allocator=");
    serial_print(heap_status.available ? "bounded_bump" : "no");
    serial_print("\n");

    serial_print("[MEMORY_MAP]\n");
    for (uint32_t i = 0; i < memory_region_count(); i++) {
        struct memory_region r;
        if (!memory_region_get(i, &r))
            continue;
        serial_print("index="); serial_print_dec(i);
        serial_print(";base="); serial_print_dec(r.base);
        serial_print(";length="); serial_print_dec(r.length);
        serial_print(";kib="); serial_print_dec(r.length / 1024u);
        serial_print(";type="); serial_print(memory_region_type_name(r.type));
        serial_print("\n");
    }

    serial_print("[PAGES]\n");
    serial_print("page_size="); serial_print_dec(page_status.page_size);
    serial_print("\ntotal_pages="); serial_print_dec(page_status.total_pages);
    serial_print("\nfree_pages="); serial_print_dec(page_status.free_pages);
    serial_print("\nused_pages="); serial_print_dec(page_status.used_pages);
    serial_print("\nreserved_low_pages="); serial_print_dec(page_status.reserved_low_pages);
    serial_print("\nreserved_kernel_pages="); serial_print_dec(page_status.reserved_kernel_pages);
    serial_print("\nreserved_heap_pages="); serial_print_dec(page_status.reserved_heap_pages);
    serial_print("\ntracked_limit_kib="); serial_print_dec(page_status.tracked_limit_kib);
    serial_print("\nvirtual_memory=no\n");

    serial_print("[KERNEL_HEAP]\n");
    serial_print("available="); serial_print(heap_status.available ? "yes" : "no");
    serial_print("\nheap_start="); serial_print_dec(heap_status.heap_start);
    serial_print("\nheap_end="); serial_print_dec(heap_status.heap_end);
    serial_print("\ntotal_bytes="); serial_print_dec(heap_status.total_bytes);
    serial_print("\nused_bytes="); serial_print_dec(heap_status.used_bytes);
    serial_print("\nremaining_bytes="); serial_print_dec(heap_status.remaining_bytes);
    serial_print("\nallocation_count="); serial_print_dec(heap_status.allocation_count);
    serial_print("\nfailed_count="); serial_print_dec(heap_status.failed_count);
    serial_print("\nhigh_water_mark="); serial_print_dec(heap_status.high_water_mark);
    serial_print("\nfree_supported=no\n");

    serial_print("[PAGING]\n");
    serial_print("prepared="); serial_print(pg_status->prepared ? "yes" : "no");
    serial_print("\nenabled="); serial_print(pg_status->enabled ? "yes" : "no");
    serial_print("\nmode="); serial_print(pg_status->mode);
    serial_print("\nidentity_map_valid="); serial_print(pg_status->identity_map_valid ? "yes" : "no");
    serial_print("\nrequired_ranges_valid="); serial_print(pg_status->required_ranges_valid ? "yes" : "no");
    serial_print("\nprerequisites_valid="); serial_print(pg_status->prerequisites_valid ? "yes" : "no");
    serial_print("\npage_directory="); serial_print_dec(pg_status->page_directory);
    serial_print("\nactive_cr3="); serial_print_dec(pg_status->active_cr3);
    serial_print("\nmapped_start="); serial_print_dec(pg_status->mapped_start);
    serial_print("\nmapped_end="); serial_print_dec(pg_status->mapped_end);
    serial_print("\npage_fault_handler="); serial_print(pg_status->page_fault_handler);
    serial_print("\nvalidation_error="); serial_print(pg_status->validation_error);
    serial_print("\nlast_fault_address=");
    if (pg_status->last_fault_present) serial_print_dec(pg_status->last_fault_address);
    else serial_print("none");
    serial_print("\nlast_fault_error="); serial_print_dec(pg_status->last_fault_error);
    serial_print("\nlast_fault_eip="); serial_print_dec(pg_status->last_fault_eip);
    serial_print("\n");

    serial_print("[PAGE_MAP]\n");
    serial_print("type=identity");
    serial_print(";virtual_start="); serial_print_dec(pg_status->mapped_start);
    serial_print(";virtual_end="); serial_print_dec(pg_status->mapped_end);
    serial_print(";physical_start="); serial_print_dec(pg_status->mapped_start);
    serial_print(";physical_end="); serial_print_dec(pg_status->mapped_end);
    serial_print(";flags=present,writable,supervisor");
    serial_print(";validated="); serial_print(pg_status->identity_map_valid ? "yes" : "no");
    serial_print("\n");
    for (uint32_t i = 0; i < paging_range_count(); i++) {
        struct paging_range range;
        if (!paging_range_get(i, &range))
            continue;
        serial_print("type=required_identity;name="); serial_print_field(range.name);
        serial_print(";virtual_start="); serial_print_dec(range.start);
        serial_print(";virtual_end="); serial_print_dec(range.end);
        serial_print(";physical_start="); serial_print_dec(range.start);
        serial_print(";physical_end="); serial_print_dec(range.end);
        serial_print(";flags=present,writable,supervisor");
        serial_print(";validated="); serial_print(range.validated ? "yes" : "no");
        serial_print("\n");
    }

    serial_print("[PAGE_FAULT]\n");
    serial_print("present="); serial_print(pg_status->last_fault_present ? "yes" : "no");
    serial_print("\nfault_address=");
    if (pg_status->last_fault_present) serial_print_dec(pg_status->last_fault_address);
    else serial_print("none");
    serial_print("\nerror_code="); serial_print_dec(pg_status->last_fault_error);
    serial_print("\neip="); serial_print_dec(pg_status->last_fault_eip);
    serial_print("\nrecovery_attempted=no\n");

    serial_print("[SYSCALLS]\n");
    serial_print("initialized="); serial_print(syscall_status.initialized ? "yes" : "no");
    serial_print("\nvector="); serial_print_dec(syscall_status.vector);
    serial_print("\ntotal_calls="); serial_print_dec(syscall_status.total_calls);
    serial_print("\nfailed_calls="); serial_print_dec(syscall_status.failed_calls);
    serial_print("\nregistered_count="); serial_print_dec(syscall_status.registered_count);
    serial_print("\nlast_id=");
    if (syscall_status.last_id == 0xFFFFFFFFu) serial_print("none");
    else serial_print_dec(syscall_status.last_id);
    serial_print("\nlast_name="); serial_print(syscall_name(syscall_status.last_id));
    serial_print("\nlast_result=");
    if (syscall_status.last_id == 0xFFFFFFFFu) serial_print("none");
    else serial_print(syscall_result_name(syscall_status.last_result));
    serial_print("\nlast_value="); serial_print_dec(syscall_status.last_value);
    serial_print("\nsyscall_gate_dpl="); serial_print_dec(syscall_status.gate_dpl);
    serial_print("\nsyscall_user_gate="); serial_print(syscall_status.syscall_user_gate ? "enabled" : "disabled");
    serial_print("\nuser_mode_available=no");
    serial_print("\nargument_model=bounded_scalars_and_kernel_owned_buffers");
    serial_print("\npointer_arguments=no\n");

    serial_print("[PRIVILEGE]\n");
    serial_print("gdt_loaded="); serial_print(privilege_state.gdt_loaded ? "yes" : "no");
    serial_print("\nuser_segments_valid="); serial_print(privilege_state.user_segments_valid ? "yes" : "no");
    serial_print("\ntss_loaded="); serial_print(privilege_state.tss_loaded ? "yes" : "no");
    serial_print("\nkernel_stack_valid="); serial_print(privilege_state.kernel_stack_valid ? "yes" : "no");
    serial_print("\npaging_enabled="); serial_print(privilege_state.paging_enabled ? "yes" : "no");
    serial_print("\nsyscall_gate_installed="); serial_print(privilege_state.syscall_gate_installed ? "yes" : "no");
    serial_print("\nsyscall_gate_dpl="); serial_print_dec(privilege_state.syscall_gate_dpl);
    serial_print("\ncurrent_cpl="); serial_print_dec(privilege_state.current_cpl);
    serial_print("\nbootstrap_stack_bottom="); serial_print_dec(privilege_state.bootstrap_stack_bottom);
    serial_print("\nbootstrap_stack_top="); serial_print_dec(privilege_state.bootstrap_stack_top);
    serial_print("\ntransition_stack_bottom="); serial_print_dec(privilege_state.transition_stack_bottom);
    serial_print("\ntransition_stack_top="); serial_print_dec(privilege_state.transition_stack_top);
    serial_print("\nesp0="); serial_print_dec(privilege_state.esp0);
    serial_print("\nguard_policy="); serial_print(privilege_state.guard_policy);
    serial_print("\nreadiness="); serial_print(privilege_state.foundation_ready ? "foundation_ready" : "not_ready");
    serial_print("\nexecution_enabled=no\n");

    serial_print("[GDT]\n");
    serial_print("loaded="); serial_print(gdt_state.loaded ? "yes" : "no");
    serial_print("\nkernel_code_selector="); serial_print_dec(gdt_state.kernel_code_selector);
    serial_print("\nkernel_code_dpl="); serial_print_dec(gdt_state.kernel_code_dpl);
    serial_print("\nkernel_data_selector="); serial_print_dec(gdt_state.kernel_data_selector);
    serial_print("\nkernel_data_dpl="); serial_print_dec(gdt_state.kernel_data_dpl);
    serial_print("\nuser_code_selector="); serial_print_dec(gdt_state.user_code_selector);
    serial_print("\nuser_code_dpl="); serial_print_dec(gdt_state.user_code_dpl);
    serial_print("\nuser_data_selector="); serial_print_dec(gdt_state.user_data_selector);
    serial_print("\nuser_data_dpl="); serial_print_dec(gdt_state.user_data_dpl);
    serial_print("\ntss_selector="); serial_print_dec(gdt_state.tss_selector);
    serial_print("\nuser_segments_valid="); serial_print(gdt_state.user_segments_valid ? "yes" : "no");
    serial_print("\ntss_descriptor_valid="); serial_print(gdt_state.tss_descriptor_valid ? "yes" : "no");
    serial_print("\n");

    serial_print("[TSS]\n");
    serial_print("initialized="); serial_print(tss_state.initialized ? "yes" : "no");
    serial_print("\nselector="); serial_print_dec(tss_state.selector);
    serial_print("\ndescriptor_valid="); serial_print(tss_state.descriptor_valid ? "yes" : "no");
    serial_print("\ntss_loaded="); serial_print(tss_state.loaded ? "yes" : "no");
    serial_print("\nss0="); serial_print_dec(tss_state.ss0);
    serial_print("\nesp0="); serial_print_dec(tss_state.esp0);
    serial_print("\nstack_bottom="); serial_print_dec(tss_state.stack_bottom);
    serial_print("\nstack_top="); serial_print_dec(tss_state.stack_top);
    serial_print("\nstack_size="); serial_print_dec(tss_state.stack_size);
    serial_print("\nkernel_stack_valid="); serial_print(tss_state.kernel_stack_valid ? "yes" : "no");
    serial_print("\nguard_policy="); serial_print(tss_state.guard_policy);
    serial_print("\nhardware_task_switching=no\n");

    serial_print("[RING3_READINESS]\n");
    serial_print("gdt_ready="); serial_print(privilege_state.gdt_loaded ? "yes" : "no");
    serial_print("\nuser_segments_ready="); serial_print(privilege_state.user_segments_valid ? "yes" : "no");
    serial_print("\ntss_ready="); serial_print(privilege_state.tss_loaded ? "yes" : "no");
    serial_print("\nesp0_ready="); serial_print(privilege_state.kernel_stack_valid ? "yes" : "no");
    serial_print("\npaging_ready="); serial_print(privilege_state.paging_enabled ? "yes" : "no");
    serial_print("\nsyscall_gate_dpl="); serial_print_dec(privilege_state.syscall_gate_dpl);
    serial_print("\nreadiness="); serial_print(privilege_state.foundation_ready ? "foundation_ready" : "not_ready");
    serial_print("\nexecution_enabled=no");
    serial_print("\nuser_stack=no\nuser_page_mappings=no\nelf_loader=no\nprocess_isolation=no\n");

    serial_print("[SYSCALL_TABLE]\n");
    for (uint32_t i = 0; i < syscall_table_count(); i++) {
        const struct syscall_descriptor *entry = syscall_table_get(i);
        serial_print("id="); serial_print_dec(entry->id);
        serial_print(";name="); serial_print_field(entry->name);
        serial_print(";argument_mode="); serial_print_field(entry->argument_mode);
        serial_print(";caller_mode=kernel_only;pointer_arguments=no\n");
    }

    serial_print("[KERNEL_INTERRUPTS]\n");
    serial_print("gdt_loaded="); serial_print(gdt_is_loaded() ? "yes" : "no");
    serial_print("\nidt_loaded="); serial_print(idt_is_loaded() ? "yes" : "no");
    serial_print("\nirq_gates_installed="); serial_print(idt_irq_gates_installed() ? "yes" : "no");
    serial_print("\npic_remapped="); serial_print(pic_is_remapped() ? "yes" : "no");
    serial_print("\ntimer_enabled="); serial_print(timer_is_enabled() ? "yes" : "no");
    serial_print("\nkeyboard_mode="); serial_print(keyboard_mode_name());
    serial_print("\ninterrupts_enabled="); serial_print(interrupts_are_enabled() ? "yes" : "no");
    serial_print("\ninterrupts_handled="); serial_print_dec(interrupts_handled());
    serial_print("\nlast_vector=");
    if (interrupts_last_vector() == 0xFFFFFFFFu) serial_print("none");
    else serial_print_dec(interrupts_last_vector());
    serial_print("\n");

    serial_print("[KERNEL_TIMING]\n");
    serial_print("tick_count="); serial_print_dec(timer_ticks());
    serial_print("\ntimer_irq="); serial_print(timer_is_enabled() ? "enabled" : "disabled");
    serial_print("\nfrequency_hz=100\n");

    serial_print("[KERNEL_EXCEPTIONS]\n");
    serial_print("last_exception=");
    if (interrupts_last_exception() == 0xFFFFFFFFu) serial_print("none");
    else serial_print(interrupt_exception_name(interrupts_last_exception()));
    serial_print("\nlast_vector=");
    if (interrupts_last_exception() == 0xFFFFFFFFu) serial_print("none");
    else serial_print_dec(interrupts_last_exception());
    serial_print("\nlast_error_code="); serial_print_dec(interrupts_last_error_code());
    serial_print("\nrecovery_attempted=no\n");

    serial_print("[KERNEL_TASKS]\n");
    serial_print("total_tasks="); serial_print_dec(task_count());
    serial_print("\nrunning_task=");
    const struct task *current = task_current();
    serial_print(current ? current->name : "none");
    serial_print("\nready_count="); serial_print_dec(task_count_state(TASK_READY));
    serial_print("\nsleeping_count="); serial_print_dec(task_count_state(TASK_SLEEPING));
    serial_print("\nblocked_count="); serial_print_dec(task_count_state(TASK_BLOCKED));
    serial_print("\ncrashed_count="); serial_print_dec(task_count_state(TASK_CRASHED));
    serial_print("\nscheduler_mode=cooperative\npreemptive_switching=no\n");
    const struct task *task_rows[TASK_MAX];
    uint32_t task_rows_count = task_list(task_rows, TASK_MAX);
    for (uint32_t i = 0; i < task_rows_count; i++) {
        const struct task *t = task_rows[i];
        serial_print("task_id="); serial_print_dec(t->task_id);
        serial_print(";name="); serial_print(t->name);
        serial_print(";state="); serial_print(task_state_name(t->state));
        serial_print(";priority="); serial_print_dec(t->priority);
        serial_print(";ticks_run="); serial_print_dec(t->ticks_run);
        serial_print(";created_at_tick="); serial_print_dec(t->created_at_tick);
        serial_print(";last_run_tick="); serial_print_dec(t->last_run_tick);
        serial_print(";flags="); serial_print_dec(t->flags);
        serial_print("\n");
    }

    tinyfs_export_report();
    app_export_report();

    /* --- Every device, one line each ------------------------------- */
    serial_print("[DEVICES]\n");
    for (uint32_t i = 0; i < ghostattach_device_count(); i++) {
        const struct device *d = ghostattach_device(i);
        serial_print("id=");      serial_print_dec(d->device_id);
        serial_print(";name=");   serial_print(d->name);
        serial_print(";type=");   serial_print(device_type_name(d->device_type));
        serial_print(";bus=");    serial_print(bus_type_name(d->bus_type));
        serial_print(";driver="); serial_print(d->attached_driver);
        serial_print(";status="); serial_print(device_status_name(d->status));
        serial_print("\n");
    }

    /* --- Every driver, one line each ------------------------------- */
    serial_print("[DRIVERS]\n");
    for (uint32_t i = 0; i < ghostattach_driver_count(); i++) {
        const struct driver *d = ghostattach_driver(i);
        serial_print("id=");        serial_print_dec(d->driver_id);
        serial_print(";name=");     serial_print(d->name);
        serial_print(";supports="); serial_print(device_type_name(d->supports_device_type));
        serial_print(";version=");  serial_print(d->version);
        serial_print(";verified="); serial_print(d->verified ? "yes" : "no");
        serial_print(";risk=");     serial_print(risk_level_name(d->risk_level));
        serial_print(";status=");   serial_print(driver_status_name(d->status));
        serial_print("\n");
    }

    /* --- Recovery policy (stated so the analyzer can echo it) ------ */
    serial_print("[RECOVERY_POLICY]\n");
    serial_print("rule=low_risk_requires_approval\n");
    serial_print("rule=medium_risk_requires_approval\n");
    serial_print("rule=high_risk_blocked\n");
    serial_print("rule=irreversible_blocked\n");
    serial_print("rule=unverified_driver_install_blocked\n");
    serial_print("rule=ai_direct_repair_blocked\n");
    serial_print("safe_mode_next_boot=");
    serial_print(recovery_safe_mode_next() ? "1" : "0");
    serial_print("\n");

    /* --- The fixed action catalog with live states ------------------ */
    serial_print("[RECOVERY_ACTIONS]\n");
    for (uint32_t i = 0; i < recovery_action_count(); i++) {
        const struct recovery_action *a = recovery_action(i);
        serial_print("id=");          serial_print(a->action_id);
        serial_print(";name=");       serial_print(a->name);
        serial_print(";target=");     serial_print(a->target_component);
        serial_print(";risk=");       serial_print(risk_level_name(a->risk_level));
        serial_print(";reversible="); serial_print(a->reversible ? "yes" : "no");
        serial_print(";requires_approval=");
        serial_print(a->requires_approval ? "yes" : "no");
        serial_print(";status=");     serial_print(action_status_name(a->status));
        serial_print("\n");
    }

    /* --- Currently suggested action IDs ----------------------------- */
    serial_print("[RECOVERY_SUGGESTIONS]\n");
    for (uint32_t i = 0; i < recovery_action_count(); i++) {
        const struct recovery_action *a = recovery_action(i);
        if (a->status == ACTST_SUGGESTED) {
            serial_print("suggested=");
            serial_print(a->action_id);
            serial_print(";hint=");
            serial_print(a->suggest_hint);
            serial_print("\n");
        }
    }

    /* --- Persistent recovery memory (v0.6) -------------------------- */
    const struct recovery_memory *m = recovery_memory_get();
    serial_print("[RECOVERY_MEMORY]\n");
    serial_print("persistence_mode=cmos_nvram_survives_reboot_not_qemu_restart\n");
    serial_print("boot_count=");           serial_print_dec(m->boot_count);
    serial_print("\nsafe_mode_next_boot=");serial_print(m->safe_mode_next_boot ? "1" : "0");
    serial_print("\nsafe_mode_active=");   serial_print(m->safe_mode_active ? "1" : "0");
    serial_print("\nnetwork_quarantined=");serial_print(m->network_quarantined ? "1" : "0");
    serial_print("\naudio_quarantined=");  serial_print(m->audio_quarantined ? "1" : "0");
    serial_print("\nlast_boot_panic=");    serial_print(m->prev_panic_detected ? "1" : "0");
    serial_print("\nrollback_available="); serial_print(m->rollback_available ? "1" : "0");
    serial_print("\nlast_action_id=");
    switch (m->last_action_id) {
    case 1:  serial_print("ACT-001"); break;
    case 2:  serial_print("ACT-002"); break;
    case 3:  serial_print("ACT-003"); break;
    case 4:  serial_print("ACT-004"); break;
    default: serial_print("none");    break;
    }
    serial_print("\n");

    /* --- Durable store status (v0.7) -------------------------------- */
    serial_print("[RECOVERY_STORE]\n");
    serial_print("backend=");            serial_print(recovery_memory_backend());
    serial_print("\ndisk_available=");   serial_print(recovery_store_available() ? "1" : "0");
    serial_print("\ndisk_capacity_sectors="); serial_print_dec(ata_capacity_sectors());
    serial_print("\nstate_valid=");      serial_print(recovery_store_valid() ? "1" : "0");
    serial_print("\njournal_count=");    serial_print_dec(recovery_journal_count());
    serial_print("\ndurable_boot_count=");serial_print_dec(m->boot_count);
    serial_print("\ncmos_fallback_used=");
    serial_print(recovery_memory_used_cmos_fallback() ? "1" : "0");
    serial_print("\n");

    /* --- Durable journal (latest 12 entries) ------------------------ */
    serial_print("[JOURNAL]\n");
    {
        uint32_t total = recovery_journal_count();
        uint32_t first = (total > 12) ? total - 12 : 0;
        struct journal_entry e;
        for (uint32_t i = first; i < total; i++) {
            if (!recovery_journal_get(i, &e))
                continue;
            serial_print("boot=");      serial_print_dec(e.boot_number);
            serial_print(";event=");
            serial_print(journal_event_name((enum journal_event)e.event_type));
            serial_print(";action=");
            /* The id byte means ACT-xxx for action events but PKG-xxx
               for package events — label it by event type. */
            if (e.action_id == 0) {
                serial_print("none");
            } else if (e.action_id == 255) {
                serial_print("ACT-999");
            } else if (e.event_type >= JE_PKG_SUGGESTED) {
                serial_print_pkg_ref(e.action_id);
            } else {
                serial_print("ACT-00");
                serial_print_dec(e.action_id);
            }
            serial_print(";result="); serial_print(e.result ? "ok" : "refused");
            serial_print(";risk=");
            serial_print(e.risk_level == 0 ? "low" :
                         e.risk_level == 1 ? "medium" : "high");
            serial_print(";reversible="); serial_print(e.reversible ? "yes" : "no");
            serial_print(";code=");       serial_print_field(e.short_code);
            serial_print("\n");
        }
    }

    /* --- Boot snapshots + health (v0.8) ----------------------------- */
    struct boot_snapshot snaps[SNAPSHOT_SLOTS];
    int nsnaps = boot_snapshot_list(snaps);

    serial_print("[BOOT_SNAPSHOTS]\n");
    for (int i = 0; i < nsnaps; i++) {
        struct boot_snapshot *s = &snaps[i];
        serial_print("boot=");      serial_print_dec(s->boot_number);
        serial_print(";health=");   serial_print_dec(s->health_score);
        serial_print(";safe_mode=");serial_print(s->safe_mode_active ? "1" : "0");
        serial_print(";panic=");    serial_print(s->prev_panic_detected ? "1" : "0");
        serial_print(";attached="); serial_print_dec(s->attached_count);
        serial_print(";fallback="); serial_print_dec(s->fallback_count);
        serial_print(";missing="); serial_print_dec(s->missing_count);
        serial_print(";isolated=");serial_print_dec(s->isolated_count);
        serial_print(";warnings=");serial_print_dec(s->warning_count);
        serial_print(";criticals=");serial_print_dec(s->critical_count);
        serial_print(";applied="); serial_print_dec(s->actions_applied);
        serial_print("\n");
    }

    serial_print("[CURRENT_HEALTH]\n");
    {
        struct health_breakdown hb;
        boot_snapshot_health_now(&hb);
        serial_print("score=");           serial_print_dec((uint32_t)hb.score);
        serial_print("\npanic_penalty="); serial_print_dec((uint32_t)(-hb.panic_penalty));
        serial_print("\nmissing_penalty=");serial_print_dec((uint32_t)(-hb.missing_penalty));
        serial_print("\nisolated_penalty=");serial_print_dec((uint32_t)(-hb.isolated_penalty));
        serial_print("\nwarning_penalty=");serial_print_dec((uint32_t)(-hb.warning_penalty));
        serial_print("\ncritical_penalty=");serial_print_dec((uint32_t)(-hb.critical_penalty));
        /* previous boot's score + delta, when a prior snapshot exists */
        int prev = -1;
        for (int i = nsnaps - 1; i >= 0; i--)
            if (snaps[i].boot_number < m->boot_count) { prev = snaps[i].health_score; break; }
        serial_print("\nprev_score=");
        if (prev >= 0) {
            serial_print_dec((uint32_t)prev);
            serial_print("\nhealth_delta=");
            int d = hb.score - prev;
            if (d < 0) { serial_print("-"); d = -d; }
            serial_print_dec((uint32_t)d);
        } else {
            serial_print("none\nhealth_delta=none");
        }
        serial_print("\n");
    }

    serial_print("[RECOVERY_TIMELINE]\n");
    for (int i = 0; i < nsnaps; i++) {
        struct boot_snapshot *s = &snaps[i];
        serial_print("boot=");       serial_print_dec(s->boot_number);
        serial_print(";health=");    serial_print_dec(s->health_score);
        serial_print(";safe_mode="); serial_print(s->safe_mode_active ? "1" : "0");
        serial_print(";panic=");     serial_print(s->prev_panic_detected ? "1" : "0");
        serial_print(";events=");
        /* journal events of this boot, comma-separated (no ';'/'=') */
        struct journal_entry e;
        int emitted = 0;
        for (uint32_t j = 0; j < recovery_journal_count(); j++) {
            if (!recovery_journal_get(j, &e) || e.boot_number != s->boot_number)
                continue;
            if (e.event_type == JE_CLEAN_BOOT)
                continue;
            if (emitted++)
                serial_print(",");
            serial_print(journal_event_name((enum journal_event)e.event_type));
            if (e.event_type >= JE_PKG_SUGGESTED) {
                serial_print(" ");
                serial_print_pkg_ref(e.action_id);
            } else {
                switch (e.action_id) {
                case 1:   serial_print(" ACT-001"); break;
                case 2:   serial_print(" ACT-002"); break;
                case 3:   serial_print(" ACT-003"); break;
                case 4:   serial_print(" ACT-004"); break;
                case 255: serial_print(" ACT-999"); break;
                default:                            break;
                }
            }
        }
        if (!emitted)
            serial_print("none");
        serial_print("\n");
    }

    /* --- Module versions + packages (v0.9) --------------------------- */
    serial_print("[MODULE_VERSIONS]\n");
    for (uint32_t i = 0; i < module_count(); i++) {
        const struct module_version *mv = module_get(i);
        serial_print("module=");  serial_print_field(mv->name);
        serial_print(";version=");serial_print_field(mv->version);
        serial_print(";base=");   serial_print_field(mv->base_version);
        serial_print(";updated=");serial_print(mv->updated ? "1" : "0");
        serial_print("\n");
    }

    serial_print("[TRUST_ANCHOR]\n");
    serial_print("anchor_id=" TRUST_ANCHOR_NAME "\n");
    serial_print("fingerprint=" TRUST_ANCHOR_FINGERPRINT "\n");
    serial_print("signature_verification=host_ed25519_real_kernel_simulated\n");
    serial_print("note=host tools verify Ed25519 - kernel enforces metadata and policy gates\n");

    serial_print("[RECOVERY_PACKAGES]\n");
    serial_print("package_state=persistent_on_disk\n");
    serial_print("manifest_source=");
    serial_print(manifest_source_is_disk() ? "disk" : "builtin_seed");
    serial_print("\n");
    for (uint32_t i = 0; i < manifest_count(); i++) {
        const struct pkg_manifest *p = manifest_get(i);
        serial_print("id=");        serial_print_field(p->package_id);
        serial_print(";name=");     serial_print_field(p->name);
        serial_print(";target=");   serial_print_field(p->target_component);
        serial_print(";from=");     serial_print_field(p->from_version);
        serial_print(";to=");       serial_print_field(p->to_version);
        serial_print(";risk=");     serial_print(risk_level_name(p->risk_level));
        serial_print(";reversible=");serial_print(p->reversible ? "yes" : "no");
        serial_print(";signature=");serial_print(signature_status_name(p->signature_status));
        serial_print(";checksum=");serial_print(p->checksum_ok ? "ok" : "mismatch");
        serial_print(";source=");  serial_print(manifest_source_name());
        serial_print(";checksum_valid=");
        serial_print(manifest_checksum_valid(p) ? "yes" : "no");
        serial_print(";trust_valid=");
        serial_print(manifest_trust_valid(p) ? "yes" : "no");
        serial_print(";signature_valid=");
        serial_print(manifest_signature_valid(p) ? "yes" : "no");
        serial_print(";signature_mode=");
        serial_print(manifest_signature_mode(p));
        serial_print(";signer_key=");
        serial_print_field(p->signer_key_hash[0] ? p->signer_key_hash : "none");
        serial_print(";signed_payload=");
        serial_print_field(p->signed_payload_hash[0] ? p->signed_payload_hash : "none");
        serial_print(";policy_result=");
        serial_print(manifest_policy_result(p));
        serial_print(";block_reason=");
        serial_print_field(manifest_block_reason(p));
        serial_print(";status=");
        serial_print(pkg_status_name(recovery_package_status(p)));
        serial_print("\n");
    }

    serial_print("[PACKAGE_MANIFESTS]\n");
    for (uint32_t i = 0; i < manifest_count(); i++) {
        const struct pkg_manifest *p = manifest_get(i);
        const char *reason;
        int ok = manifest_verify(p, &reason);
        serial_print("id=");          serial_print_field(p->package_id);
        serial_print(";payload=");    serial_print(payload_type_name(p->payload_type));
        serial_print(";trust_anchor=");
        serial_print(p->trust_anchor_id == TRUST_ANCHOR_ID ? TRUST_ANCHOR_NAME
                                                           : "unknown");
        serial_print(";source=");      serial_print(manifest_source_name());
        serial_print(";checksum_valid=");
        serial_print(manifest_checksum_valid(p) ? "yes" : "no");
        serial_print(";trust_valid=");
        serial_print(manifest_trust_valid(p) ? "yes" : "no");
        serial_print(";signature_valid=");
        serial_print(manifest_signature_valid(p) ? "yes" : "no");
        serial_print(";signature_mode=");
        serial_print(manifest_signature_mode(p));
        serial_print(";signer_key=");
        serial_print_field(p->signer_key_hash[0] ? p->signer_key_hash : "none");
        serial_print(";signed_payload=");
        serial_print_field(p->signed_payload_hash[0] ? p->signed_payload_hash : "none");
        serial_print(";verifies=");   serial_print(ok ? "yes" : "no");
        serial_print(";policy_result=");
        serial_print(manifest_policy_result(p));
        serial_print(";reason=");     serial_print_field(reason);
        serial_print(";block_reason=");
        serial_print_field(manifest_block_reason(p));
        serial_print("\n");
    }

    serial_print("[PACKAGE_STATES]\n");
    for (uint32_t i = 0; i < package_state_count(); i++) {
        const struct pkg_pstate *s = package_state_get(i);
        serial_print("id=");        serial_print_field(s->package_id);
        serial_print(";approved="); serial_print(s->approved ? "1" : "0");
        serial_print(";applied=");  serial_print(s->applied ? "1" : "0");
        serial_print(";verified="); serial_print(s->verified ? "1" : "0");
        serial_print(";failed=");   serial_print(s->failed ? "1" : "0");
        serial_print(";rolled_back=");serial_print(s->rolled_back ? "1" : "0");
        serial_print(";from=");     serial_print_field(s->from_version);
        serial_print(";to=");       serial_print_field(s->to_version);
        serial_print(";manifest_identity="); serial_print_dec(s->manifest_identity);
        serial_print(";transaction_phase=");
        serial_print(package_transaction_phase_name(s->transaction_phase));
        serial_print(";binding_valid="); serial_print(s->extension_valid ? "1" : "0");
        serial_print("\n");
    }

    serial_print("[MODULE_SLOTS]\n");
    for (uint32_t i = 0; i < module_slot_count(); i++) {
        const struct module_slot *s = module_slot_get(i);
        serial_print("module=");    serial_print_field(module_get((uint32_t)s->module)->name);
        serial_print(";active=");   serial_print(s->active_slot == SLOT_A ? "A" : "B");
        serial_print(";slot_a=");   serial_print_field(s->slot_a_version);
        serial_print(";slot_a_verified="); serial_print(s->slot_a_verified ? "1" : "0");
        serial_print(";slot_b=");   serial_print_field(s->slot_b_version);
        serial_print(";slot_b_verified="); serial_print(s->slot_b_verified ? "1" : "0");
        serial_print(";rollback=");
        serial_print(s->rollback_slot == SLOT_NONE ? "none" :
                     s->rollback_slot == SLOT_A    ? "A" : "B");
        serial_print(";pending_verify="); serial_print(s->pending_verify ? "1" : "0");
        serial_print("\n");
    }

    serial_print("[PENDING_VERIFICATION]\n");
    serial_print("pending=");
    serial_print(module_slots_pending_verify() ? "1" : "0");
    serial_print("\n");

    serial_print("[PACKAGE_POLICY]\n");
    serial_print("rule=unsigned_blocked\n");
    serial_print("rule=invalid_signature_blocked\n");
    serial_print("rule=high_risk_blocked\n");
    serial_print("rule=irreversible_blocked\n");
    serial_print("rule=checksum_mismatch_blocked\n");
    serial_print("rule=medium_risk_requires_approval\n");
    serial_print("rule=low_risk_requires_approval\n");
    serial_print("rule=fixed_registry_only_no_disk_loading\n");
    serial_print("rule=no_code_patching_apply_is_metadata_only\n");

    serial_print("[PACKAGE_HISTORY]\n");
    {
        /* Durable package events from the journal (latest 10). */
        struct journal_entry je;
        uint32_t total = recovery_journal_count();
        uint32_t emitted = 0;
        for (uint32_t i = 0; i < total; i++) {
            if (!recovery_journal_get(i, &je))
                continue;
            if (je.event_type < JE_PKG_SUGGESTED)
                continue;
            serial_print("boot=");   serial_print_dec(je.boot_number);
            serial_print(";event=");
            serial_print(journal_event_name((enum journal_event)je.event_type));
            serial_print(";package=");
            serial_print_pkg_ref(je.action_id);
            serial_print(";result="); serial_print(je.result ? "ok" : "refused");
            serial_print("\n");
            emitted++;
        }
        if (emitted == 0)
            serial_print("none\n");
    }

    /* --- The full structured error log ----------------------------- */
    serial_print("[ERROR_LOG]\n");
    struct kerror e;
    for (uint32_t i = 0; error_get(i, &e); i++) {
        serial_print("code=");       serial_print_field(e.code);
        serial_print(";severity=");  serial_print(severity_name(e.severity));
        serial_print(";component="); serial_print_field(e.component);
        serial_print(";message=");   serial_print_field(e.message);
        serial_print(";hint=");      serial_print_field(e.hint);
        serial_print("\n");
    }

    serial_print("[KAMIKAZEOS_BOOT_REPORT_END]\n");
}
