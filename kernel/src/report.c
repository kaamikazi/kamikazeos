/* report.c — emit the machine-readable boot report over serial.
 *
 * Format contract (the Python analyzer in ai-recovery/ depends on this):
 *   - the report sits between [KAMIKAZEOS_BOOT_REPORT_START] and ..._END]
 *   - section headers are [DEVICES], [DRIVERS], [ERROR_LOG] on their own line
 *   - data lines are key=value pairs joined with ';' — same convention as
 *     the live log stream, and the same rule: values never contain ';'/'='.
 */
#include "report.h"
#include "boot_snapshot.h"
#include "errors.h"
#include "ghostattach.h"
#include "module_slots.h"
#include "module_versions.h"
#include "package_manifest.h"
#include "package_state.h"
#include "recovery_package.h"
#include "recovery_actions.h"
#include "recovery_memory.h"
#include "recovery_store.h"
#include "serial.h"
#include "string.h"

#define KMZ_VERSION "1.5.0"

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

void report_send(void)
{
    serial_print("[KAMIKAZEOS_BOOT_REPORT_START]\n");
    emit_kv("version", KMZ_VERSION);
    serial_print("\n");

    /* --- GhostAttach scan summary ---------------------------------- */
    struct ghostattach_stats st;
    ghostattach_get_stats(&st);
    serial_print("scan_total=");    serial_print_dec(st.total);
    serial_print(";scan_attached=");serial_print_dec(st.attached);
    serial_print(";scan_fallback=");serial_print_dec(st.fallback);
    serial_print(";scan_missing="); serial_print_dec(st.missing);
    serial_print(";scan_isolated=");serial_print_dec(st.isolated);
    serial_print("\n");

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
            serial_print(";code=");       serial_print(e.short_code);
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
        serial_print("module=");  serial_print(mv->name);
        serial_print(";version=");serial_print(mv->version);
        serial_print(";base=");   serial_print(mv->base_version);
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
        serial_print("id=");        serial_print(p->package_id);
        serial_print(";name=");     serial_print(p->name);
        serial_print(";target=");   serial_print(p->target_component);
        serial_print(";from=");     serial_print(p->from_version);
        serial_print(";to=");       serial_print(p->to_version);
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
        serial_print(p->signer_key_hash[0] ? p->signer_key_hash : "none");
        serial_print(";signed_payload=");
        serial_print(p->signed_payload_hash[0] ? p->signed_payload_hash : "none");
        serial_print(";policy_result=");
        serial_print(manifest_policy_result(p));
        serial_print(";block_reason=");
        serial_print(manifest_block_reason(p));
        serial_print(";status=");
        serial_print(pkg_status_name(recovery_package_status(p)));
        serial_print("\n");
    }

    serial_print("[PACKAGE_MANIFESTS]\n");
    for (uint32_t i = 0; i < manifest_count(); i++) {
        const struct pkg_manifest *p = manifest_get(i);
        const char *reason;
        int ok = manifest_verify(p, &reason);
        serial_print("id=");          serial_print(p->package_id);
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
        serial_print(p->signer_key_hash[0] ? p->signer_key_hash : "none");
        serial_print(";signed_payload=");
        serial_print(p->signed_payload_hash[0] ? p->signed_payload_hash : "none");
        serial_print(";verifies=");   serial_print(ok ? "yes" : "no");
        serial_print(";policy_result=");
        serial_print(manifest_policy_result(p));
        serial_print(";reason=");     serial_print(reason);
        serial_print(";block_reason=");
        serial_print(manifest_block_reason(p));
        serial_print("\n");
    }

    serial_print("[PACKAGE_STATES]\n");
    for (uint32_t i = 0; i < package_state_count(); i++) {
        const struct pkg_pstate *s = package_state_get(i);
        serial_print("id=");        serial_print(s->package_id);
        serial_print(";approved="); serial_print(s->approved ? "1" : "0");
        serial_print(";applied=");  serial_print(s->applied ? "1" : "0");
        serial_print(";verified="); serial_print(s->verified ? "1" : "0");
        serial_print(";failed=");   serial_print(s->failed ? "1" : "0");
        serial_print(";rolled_back=");serial_print(s->rolled_back ? "1" : "0");
        serial_print(";from=");     serial_print(s->from_version);
        serial_print(";to=");       serial_print(s->to_version);
        serial_print("\n");
    }

    serial_print("[MODULE_SLOTS]\n");
    for (uint32_t i = 0; i < module_slot_count(); i++) {
        const struct module_slot *s = module_slot_get(i);
        serial_print("module=");    serial_print(module_get((uint32_t)s->module)->name);
        serial_print(";active=");   serial_print(s->active_slot == SLOT_A ? "A" : "B");
        serial_print(";slot_a=");   serial_print(s->slot_a_version);
        serial_print(";slot_a_verified="); serial_print(s->slot_a_verified ? "1" : "0");
        serial_print(";slot_b=");   serial_print(s->slot_b_version);
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
        serial_print("code=");       serial_print(e.code);
        serial_print(";severity=");  serial_print(severity_name(e.severity));
        serial_print(";component="); serial_print(e.component);
        serial_print(";message=");   serial_print(e.message);
        serial_print(";hint=");      serial_print(e.hint);
        serial_print("\n");
    }

    serial_print("[KAMIKAZEOS_BOOT_REPORT_END]\n");
}
