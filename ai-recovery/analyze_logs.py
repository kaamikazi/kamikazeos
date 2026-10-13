#!/usr/bin/env python3
"""
KamikazeOS AI Recovery Core - host-side log analyzer (v2.6, offline/rule-based)

This is the first working incarnation of the "maintenance brain". It runs on
the HOST, outside the kernel, and its powers are deliberately limited:

    READ logs  ->  EXPLAIN problems  ->  RANK risk  ->  SUGGEST next actions

It never patches binaries, installs drivers, downloads code, or modifies the
OS. Every suggestion is for a HUMAN to approve and perform. v2.6 is
rule-based (no AI API yet) so the pipeline can be built and trusted first;
see analyze_with_ai_later() at the bottom for where a real model plugs in.

Usage:
    python analyze_logs.py [path/to/serial.log]     (default: build/serial.log)

Input format (produced by the kernel's report.c and errors.c):
    [KAMIKAZEOS_BOOT_REPORT_START] ... [KAMIKAZEOS_BOOT_REPORT_END]
    with [DEVICES] / [DRIVERS] / [ERROR_LOG] sections of
    key=value;key=value;... lines.
"""

import sys
from pathlib import Path

# --------------------------------------------------------------------------
# The rule base: stable error codes -> likely cause + safe manual next action.
# This table IS the offline "intelligence". A stable code from the kernel gives
# us ground truth; free-text guessing is exactly what we avoid.
# --------------------------------------------------------------------------
RULES = {
    "GA-SEC-001": {
        "cause":  "GhostAttach found a fallback driver for this device, but the "
                  "driver is not signed/verified. Policy rule 3 blocked it.",
        "action": "Do NOT force-load the driver. Obtain a verified build of it "
                  "(or sign it through the future update channel), then reboot.",
    },
    "GA-DRV-101": {
        "cause":  "No verified driver exists for this device, and no safe "
                  "fallback was available. The device is disabled.",
        "action": "The device stays off - that is the safe state. Add a "
                  "verified driver to the registry in a future update.",
    },
    "GA-DRV-201": {
        "cause":  "A verified driver tried to attach but failed its probe. The "
                  "device was quarantined (isolated) to prevent boot loops.",
        "action": "Check device/driver compatibility. The quarantine is "
                  "correct behavior - lift it only after fixing the driver.",
    },
    "GA-TEST-201": {
        "cause":  "The compiled-in virtual GhostAttach canary failed its "
                  "attach probe as designed.",
        "action": "No repair is required. This is expected self-test "
                  "evidence, not a production device failure.",
    },
    "KBD-001": {
        "cause":  "The PS/2 keyboard controller did not respond to the boot "
                  "probe. The interactive shell has no input.",
        "action": "Check the emulator's input configuration (PS/2 device "
                  "present?). On real hardware: check the controller.",
    },
    "EXP-001": {
        "cause":  "Log export was requested but the COM1 UART failed its "
                  "loopback self-test at boot.",
        "action": "Check the emulator's -serial flag. Without serial, use the "
                  "on-screen logs-ai output instead.",
    },
    "TEST-PANIC-001": {
        "cause":  "A deliberate test panic was requested via the panic-test "
                  "shell command. This is an exercise, not a real failure.",
        "action": "None needed. Reboot the VM. If you did NOT run panic-test, "
                  "treat this as a real incident and investigate who did.",
    },
    "REC-BLOCK-999": {
        "cause":  "Someone (or the boot self-check) attempted a high-risk or "
                  "irreversible recovery action. The policy gate refused it.",
        "action": "None - the block is correct and unconditional. If you need "
                  "this capability, it must arrive as a verified, signed, "
                  "reversible action in a future catalog, never as an override.",
    },
    "REC-ACT-004": {
        "cause":  "A recovery action passed approval but its apply step "
                  "failed. State was left unchanged.",
        "action": "Report this as a RecoveryEngine bug; include the log.",
    },
    "PKG-SUG-001": {
        "cause":  "A verified manifest matches the currently active module "
                  "version, so the package is available for the human-driven "
                  "approve/apply/verify flow.",
        "action": "Review package details inside KamikazeOS, then run "
                  "approve-pkg/apply-pkg/verify-pkg only if you accept the risk.",
    },
    "PKG-BLOCK-001": {
        "cause":  "A recovery package failed the deterministic verification "
                  "chain: unsigned, invalid, unknown trust anchor, high-risk, "
                  "irreversible, checksum mismatch, or blocked payload type.",
        "action": "Do not override the block. Use only a future verified, "
                  "reversible manifest that passes the trust-anchor policy.",
    },
    "PKG-ACT-001": {
        "cause":  "A human approved a package that passed the manifest and "
                  "policy checks.",
        "action": "Continue with apply-pkg, then verify-pkg. Approval alone "
                  "does not change module state.",
    },
    "PKG-ACT-002": {
        "cause":  "The package switched metadata to the inactive A/B module "
                  "slot and marked it pending verification.",
        "action": "Run verify-pkg before trusting the new slot. Use rollback-pkg "
                  "if verification fails or confidence drops.",
    },
    "PKG-ACT-003": {
        "cause":  "Post-apply verification passed: expected slot, expected "
                  "version, no new criticals, and acceptable health delta.",
        "action": "No immediate action required. rollback-pkg remains the "
                  "manual escape hatch while rollback metadata exists.",
    },
    "PKG-ACT-004": {
        "cause":  "Post-apply verification failed after a package apply.",
        "action": "Run rollback-pkg for the failed package to return to the "
                  "known-good slot.",
    },
    "PKG-PENDING-VERIFY": {
        "cause":  "A package was applied before a reboot or crash and its new "
                  "slot has not been verified yet.",
        "action": "Run verify-pkg for the applied package, or rollback-pkg to "
                  "return to the known-good slot.",
    },
    "PKG-RB-001": {
        "cause":  "A package rollback switched active module metadata back to "
                  "the recorded known-good slot.",
        "action": "Confirm health on the next boot and investigate why the new "
                  "slot was not kept.",
    },
    "MAN-000": {
        "cause":  "Package manifests were loaded from disk sectors and checked "
                  "against the simulated trust-anchor policy.",
        "action": "No action needed unless a specific package is blocked.",
    },
    "MAN-001": {
        "cause":  "No valid disk manifests were present, so KamikazeOS fell "
                  "back to the built-in seed manifest set.",
        "action": "Run seed-manifests confirm inside KamikazeOS if you want the "
                  "seed manifests persisted to disk.",
    },
    "MAN-002": {
        "cause":  "The built-in seed manifest set was written to the manifest "
                  "sectors.",
        "action": "No action needed. Future boots should report disk-sourced "
                  "manifests.",
    },
    "PST-000": {
        "cause":  "Persistent package lifecycle state was restored from disk.",
        "action": "No action needed. Package status now survives QEMU restarts.",
    },
    "PST-100": {
        "cause":  "A human reset package lifecycle state and A/B slot metadata.",
        "action": "Review packages and slots after reboot; journal history was "
                  "preserved.",
    },
    "SLOT-000": {
        "cause":  "Module A/B slot metadata was restored from disk and now "
                  "defines the running module versions.",
        "action": "No action needed unless a slot is pending verification.",
    },
    "BOOT-001": {
        "cause":  "The kernel was started by something that is not a "
                  "Multiboot-compliant loader; boot information is untrusted.",
        "action": "Boot via GRUB or QEMU's -kernel option.",
    },
    "CPU-EXCEPTION": {
        "cause":  "The CPU raised a synchronous exception. KamikazeOS captured "
                  "the vector, error code, and instruction pointer before halting.",
        "action": "Do not attempt automatic recovery. Preserve the serial log, "
                  "reboot, and investigate the recorded exception evidence.",
    },
    "PAGE-FAULT": {
        "cause":  "The CPU raised a page fault. KamikazeOS captured the CR2 "
                  "fault address, error code, and instruction pointer.",
        "action": "Do not attempt automatic recovery. Preserve the serial log, "
                  "reboot, and inspect the paging report and PAGE-FAULT evidence.",
    },
    "PAGE-101": {
        "cause":  "Strict paging validation rejected at least one required "
                  "identity-mapped runtime range or prerequisite.",
        "action": "Keep paging disabled. Inspect `paging` and `page-map`; do "
                  "not bypass the enable gate.",
    },
    "PAGE-102": {
        "cause":  "The CPU paging enable state or active CR3 did not match "
                  "the validated page directory.",
        "action": "Stop paging tests and inspect CR0/CR3 handling manually.",
    },
    "SYS-ERR-001": {
        "cause":  "The fixed syscall dispatcher rejected an unknown syscall ID.",
        "action": "Use only IDs listed by `syscalls`. The rejection is the "
                  "expected fail-closed behavior.",
    },
    "SYS-ERR-002": {
        "cause":  "A syscall rejected an invalid bounded argument or an "
                  "unavailable kernel service.",
        "action": "Inspect `syscalls` and the target subsystem. Do not pass "
                  "raw addresses; v2.6 exposes no pointer ABI.",
    },
    "SYS-ERR-003": {
        "cause":  "The kernel could not install its dedicated syscall IDT gate.",
        "action": "Keep syscall use disabled and inspect IDT initialization. "
                  "Do not bypass the gate check.",
    },
    "TSS-101": {
        "cause":  "The TSS descriptor or bounded SS0:ESP0 stack failed "
                  "validation.",
        "action": "Keep the syscall gate at DPL0. Inspect `gdt`, `tss`, and "
                  "`ring3-readiness`; do not enable Ring 3 execution.",
    },
    "TSS-102": {
        "cause":  "The CPU task register did not retain the expected TSS "
                  "selector after ltr.",
        "action": "Keep the user syscall gate disabled and inspect the TSS "
                  "descriptor and selector manually.",
    },
    "PRIV-101": {
        "cause":  "One or more privilege-boundary prerequisites failed, so "
                  "the Ring 3 foundation is incomplete.",
        "action": "Inspect GDT, TSS, ESP0, paging, and syscall DPL evidence. "
                  "Do not bypass the readiness gate.",
    },
    "RING3-101": {
        "cause":  "The fixed CPL3 execution gate rejected incomplete user-memory or privilege validation.",
        "action": "Keep Ring 3 execution disabled. Inspect `ring3-readiness` and `user-memory`; never broaden the identity map.",
    },
    "RING3-MEM-CRITICAL": {
        "cause":  "The report indicates kernel or paging structures may be user-accessible, or the user mapping is broader than the fixed test pages.",
        "action": "Stop CPL3 testing. Keep execution disabled and inspect page-table USER bits manually.",
    },
    "RING3-VALIDATION": {
        "cause":  "Ring 3 execution was enabled without every fixed user-memory validation result passing.",
        "action": "Disable CPL3 execution and repair the readiness gate. Do not bypass validation.",
    },
    "APP-BLOCK-001": {
        "cause":  "The app policy gate refused a requested app name that is not "
                  "in the fixed compile-time registry, or an app without a "
                  "registered entry point.",
        "action": "This is the correct safety behavior. Run only names listed "
                  "by `apps`; TinyFS files and package payloads are never "
                  "executed.",
    },
    "HEAP-101": {
        "cause":  "A kernel heap allocation request was invalid because the "
                  "heap was unavailable or the requested size was zero.",
        "action": "No automatic repair. Check `heap` output and retry only with "
                  "a nonzero bounded size.",
    },
    "HEAP-102": {
        "cause":  "The fixed bump heap rejected an allocation that would exceed "
                  "the bounded heap region.",
        "action": "This is the correct safety behavior. Request less memory or "
                  "reboot the demo to reset the bump heap.",
    },
    # v2.6.1 Integrity Gate: durable store integrity events.
    "STORE-WARN-004": {
        "cause":  "The 448-entry append-only recovery journal is full. New "
                  "events are refused (counted as journal_dropped) instead of "
                  "overwriting history. Logged once per boot.",
        "action": "Export and archive the journal, then decide manually "
                  "whether to run `clear-store confirm`. Nothing is erased "
                  "automatically.",
    },
    "STORE-WARN-008": {
        "cause":  "The journal append position was rebuilt from the journal's "
                  "own entries because the state block recorded fewer entries "
                  "(torn append) or no valid state existed.",
        "action": "No repair needed: existing entries were preserved and new "
                  "entries go after them. Investigate why the state write was lost.",
    },
    "STORE-WARN-009": {
        "cause":  "The primary recovery state sector (LBA 1) failed validation; "
                  "the mirror copy (LBA 50) was loaded instead.",
        "action": "The next save rewrites both copies. Repeated occurrences "
                  "point at unreliable storage - inspect the disk image.",
    },
    "STORE-WARN-010": {
        "cause":  "The state block records more journal entries than the disk "
                  "holds. The missing slots stay reserved and are never reused.",
        "action": "Treat the missing entries as lost evidence and inspect how "
                  "the journal sectors were modified.",
    },
    "STORE-WARN-011": {
        "cause":  "One or more occupied journal slots fail their checksum. They "
                  "are kept in place as evidence and skipped when reading.",
        "action": "Do not clear the store until the damaged entries are "
                  "examined with an exported image.",
    },
    "STORE-WARN-012": {
        "cause":  "Only one of the two recovery state copies could be written.",
        "action": "State is durable but not redundant. Check the disk image.",
    },
    "STORE-WARN-013": {
        "cause":  "A journal append was refused because the store is read-only "
                  "this boot (see STORE-ERR-011/012/014).",
        "action": "Events from this boot exist only in the serial log - keep it.",
    },
    "STORE-ERR-010": {
        "cause":  "Both recovery state copies are damaged. Flags were taken from "
                  "the CMOS fallback and the journal position from the journal.",
        "action": "Review quarantines and safe-mode flags manually; history "
                  "entries were preserved.",
    },
    "STORE-ERR-011": {
        "cause":  "Both state copies are internally valid with the same "
                  "generation but different contents, so neither can be trusted.",
        "action": "The store is read-only this boot. Export the image and decide "
                  "manually which copy is correct before any `clear-store`.",
    },
    "STORE-ERR-012": {
        "cause":  "A recovery state or journal sector could not be read, so the "
                  "append position is unknown.",
        "action": "The journal is read-only this boot. Check the disk attachment.",
    },
    "STORE-ERR-014": {
        "cause":  "The journal slot chosen for an append already held data, which "
                  "means something wrote the journal outside the kernel.",
        "action": "Appends stopped for this boot; inspect the disk image.",
    },
}

# severity (kernel) -> classification (report)
SEVERITY_CLASS = {
    "info":    "info",
    "warning": "warning",
    "error":   "critical",
    "panic":   "critical",
}


def parse_kv_line(line: str) -> dict:
    """Parse 'a=1;b=two words;c=3' into a dict. Values never contain ';'/'='
    (that is part of the kernel's format contract)."""
    out = {}
    for field in line.strip().split(";"):
        if "=" in field:
            key, _, value = field.partition("=")
            out[key] = value
    return out


def extract_last_boot_report(text: str):
    """Return (report_lines, after_lines, found). Uses the LAST report in the
    file — the serial log accumulates across boots and re-exports.

    after_lines matters more than it looks: the kernel streams every event
    live, so anything that happened AFTER the report was sent (a panic, a
    failed command) exists only as loose lines after the END marker. Those
    are usually the most important events in the whole file.
    """
    start = text.rfind("[KAMIKAZEOS_BOOT_REPORT_START]")
    if start == -1:
        return [], [], False
    end = text.find("[KAMIKAZEOS_BOOT_REPORT_END]", start)
    if end == -1:
        return [], [], False  # truncated report (kernel died mid-export?)
    body = text[start:end]
    after = text[end:]
    report_lines = [l for l in body.splitlines()[1:] if l.strip()]
    after_lines = [l for l in after.splitlines() if l.startswith("code=")]
    return report_lines, after_lines, True


def parse_page_fault_evidence(text: str):
    """Return the last PAGE_FAULT section, including panic evidence emitted
    after a completed boot report."""
    latest = {}
    current = None
    for raw in text.splitlines():
        line = raw.strip()
        if line == "[PAGE_FAULT]":
            if current:
                latest = current
            current = {}
        elif current is not None and line.startswith("["):
            if current:
                latest = current
            current = None
        elif current is not None and line:
            current.update(parse_kv_line(line))
    if current:
        latest = current
    return latest


def parse_section_evidence(text: str, header: str):
    """Return the last named key/value section, including post-report evidence."""
    latest, current = {}, None
    for raw in text.splitlines():
        line = raw.strip()
        if line == header:
            if current:
                latest = current
            current = {}
        elif current is not None and line.startswith("["):
            if current:
                latest = current
            current = None
        elif current is not None and line:
            current.update(parse_kv_line(line))
    if current:
        latest = current
    return latest


def parse_report(lines):
    """Split the report into its sections."""
    meta, devices, drivers, events = {}, [], [], []
    policy, actions, suggestions = [], [], []
    memory = {}
    section = "meta"
    section_map = {
        "[DEVICES]": "devices",
        "[SYSTEM_STATUS]": "system_status",
        "[DIAGNOSTICS_SUMMARY]": "diagnostics",
        "[DRIVERS]": "drivers",
        "[RECOVERY_POLICY]": "policy",
        "[RECOVERY_ACTIONS]": "actions",
        "[RECOVERY_SUGGESTIONS]": "suggestions",
        "[RECOVERY_MEMORY]": "memory",
        "[RECOVERY_STORE]": "store",
        "[JOURNAL]": "journal",
        "[BOOT_SNAPSHOTS]": "snapshots",
        "[CURRENT_HEALTH]": "health",
        "[RECOVERY_TIMELINE]": "timeline",
        "[MODULE_VERSIONS]": "modules",
        "[RECOVERY_PACKAGES]": "packages",
        "[PACKAGE_POLICY]": "pkg_policy",
        "[PACKAGE_HISTORY]": "pkg_history",
        "[TRUST_ANCHOR]": "trust",
        "[PACKAGE_MANIFESTS]": "pkg_manifests",
        "[PACKAGE_STATES]": "pkg_states",
        "[MODULE_SLOTS]": "slots",
        "[PENDING_VERIFICATION]": "pending",
        "[MEMORY]": "kernel_memory",
        "[MEMORY_MAP]": "memory_map",
        "[PAGES]": "pages",
        "[KERNEL_HEAP]": "kernel_heap",
        "[PAGING]": "paging",
        "[PAGE_MAP]": "page_map",
        "[PAGE_FAULT]": "paging",
        "[SYSCALLS]": "syscalls",
        "[SYSCALL_TABLE]": "syscall_table",
        "[PRIVILEGE]": "privilege",
        "[GDT]": "gdt",
        "[TSS]": "tss",
        "[RING3_READINESS]": "ring3_readiness",
        "[RING3]": "ring3",
        "[USER_MEMORY]": "user_memory",
        "[USER_EXECUTION]": "user_execution",
        "[USER_FAULT]": "user_fault",
        "[KERNEL_INTERRUPTS]": "kernel_interrupts",
        "[KERNEL_TIMING]": "kernel_timing",
        "[KERNEL_EXCEPTIONS]": "kernel_exceptions",
        "[KERNEL_TASKS]": "kernel_tasks",
        "[TINYFS]": "tinyfs",
        "[TINYFS_FILES]": "tinyfs_files",
        "[APPS]": "apps",
        "[APP_LOG]": "app_log",
        "[ERROR_LOG]": "events",
    }
    store, journal = {}, []
    snapshots, health, timeline = [], {}, []
    modules, pkgs, pkg_policy, pkg_history = [], [], [], []
    trust, pkg_manifests, pkg_states, mod_slots, pending = {}, [], [], [], {}
    kernel_interrupts, kernel_timing, kernel_exceptions = {}, {}, {}
    kernel_memory, memory_map, pages, kernel_heap = {}, [], {}, {}
    paging, page_map = {}, []
    syscalls, syscall_rows = {}, []
    privilege, gdt, tss, ring3_readiness = {}, {}, {}, {}
    ring3, user_memory, user_execution, user_fault = {}, {}, {}, {}
    kernel_tasks, task_rows = {}, []
    tinyfs, tinyfs_files = {}, []
    apps, app_rows, app_log = {}, [], []
    system_status, diagnostics = {}, {}
    for line in lines:
        if line in section_map:
            section = section_map[line]
        elif section == "meta":
            meta.update(parse_kv_line(line))
        elif section == "devices":
            devices.append(parse_kv_line(line))
        elif section == "system_status":
            system_status.update(parse_kv_line(line))
        elif section == "diagnostics":
            diagnostics.update(parse_kv_line(line))
        elif section == "drivers":
            drivers.append(parse_kv_line(line))
        elif section == "policy":
            policy.append(line.strip())
        elif section == "actions":
            actions.append(parse_kv_line(line))
        elif section == "suggestions":
            suggestions.append(parse_kv_line(line))
        elif section == "memory":
            memory.update(parse_kv_line(line))
        elif section == "store":
            store.update(parse_kv_line(line))
        elif section == "journal":
            journal.append(parse_kv_line(line))
        elif section == "snapshots":
            snapshots.append(parse_kv_line(line))
        elif section == "health":
            health.update(parse_kv_line(line))
        elif section == "timeline":
            timeline.append(parse_kv_line(line))
        elif section == "modules":
            modules.append(parse_kv_line(line))
        elif section == "packages":
            pkgs.append(parse_kv_line(line))
        elif section == "pkg_policy":
            pkg_policy.append(line.strip())
        elif section == "pkg_history":
            if line.strip() != "none":
                pkg_history.append(parse_kv_line(line))
        elif section == "trust":
            trust.update(parse_kv_line(line))
        elif section == "pkg_manifests":
            pkg_manifests.append(parse_kv_line(line))
        elif section == "pkg_states":
            pkg_states.append(parse_kv_line(line))
        elif section == "slots":
            mod_slots.append(parse_kv_line(line))
        elif section == "pending":
            pending.update(parse_kv_line(line))
        elif section == "kernel_memory":
            kernel_memory.update(parse_kv_line(line))
        elif section == "memory_map":
            memory_map.append(parse_kv_line(line))
        elif section == "pages":
            pages.update(parse_kv_line(line))
        elif section == "kernel_heap":
            kernel_heap.update(parse_kv_line(line))
        elif section == "paging":
            paging.update(parse_kv_line(line))
        elif section == "page_map":
            page_map.append(parse_kv_line(line))
        elif section == "syscalls":
            syscalls.update(parse_kv_line(line))
        elif section == "syscall_table":
            syscall_rows.append(parse_kv_line(line))
        elif section == "privilege":
            privilege.update(parse_kv_line(line))
        elif section == "gdt":
            gdt.update(parse_kv_line(line))
        elif section == "tss":
            tss.update(parse_kv_line(line))
        elif section == "ring3_readiness":
            ring3_readiness.update(parse_kv_line(line))
        elif section == "ring3":
            ring3.update(parse_kv_line(line))
        elif section == "user_memory":
            user_memory.update(parse_kv_line(line))
        elif section == "user_execution":
            user_execution.update(parse_kv_line(line))
        elif section == "user_fault":
            user_fault.update(parse_kv_line(line))
        elif section == "kernel_interrupts":
            kernel_interrupts.update(parse_kv_line(line))
        elif section == "kernel_timing":
            kernel_timing.update(parse_kv_line(line))
        elif section == "kernel_exceptions":
            kernel_exceptions.update(parse_kv_line(line))
        elif section == "kernel_tasks":
            values = parse_kv_line(line)
            if "task_id" in values:
                task_rows.append(values)
            else:
                kernel_tasks.update(values)
        elif section == "tinyfs":
            tinyfs.update(parse_kv_line(line))
        elif section == "tinyfs_files":
            tinyfs_files.append(parse_kv_line(line))
        elif section == "apps":
            values = parse_kv_line(line)
            if "app_id" in values:
                app_rows.append(values)
            else:
                apps.update(values)
        elif section == "app_log":
            app_log.append(parse_kv_line(line))
        elif section == "events":
            events.append(parse_kv_line(line))
    return (meta, devices, drivers, events, policy, actions, suggestions,
            memory, store, journal, snapshots, health, timeline,
            modules, pkgs, pkg_policy, pkg_history,
            trust, pkg_manifests, pkg_states, mod_slots, pending,
            kernel_memory, memory_map, pages, kernel_heap,
            paging, page_map, syscalls, syscall_rows,
            privilege, gdt, tss, ring3_readiness,
            ring3, user_memory, user_execution, user_fault,
            kernel_interrupts, kernel_timing, kernel_exceptions,
            kernel_tasks, task_rows, tinyfs, tinyfs_files,
            apps, app_rows, app_log, system_status, diagnostics)


def classify(events, intentional_user_fault=False):
    """Bucket events into info / warning / critical."""
    buckets = {"info": [], "warning": [], "critical": []}
    for e in events:
        expected_canary = e.get("code") == "GA-TEST-201" or (
            e.get("code") == "GA-DRV-201" and
            e.get("message", "").startswith("Self-Test Device ")
        ) or (intentional_user_fault and
              e.get("code") in ("PAGE-FAULT", "CPU-EXCEPTION"))
        cls = "info" if expected_canary else \
            SEVERITY_CLASS.get(e.get("severity", ""), "warning")
        buckets[cls].append(e)
    return buckets


def print_issue(e):
    code = e.get("code", "?")
    print(f"  [{e.get('severity', '?').upper()}] {code}  ({e.get('component', '?')})")
    print(f"    event   : {e.get('message', '?')}")
    print(f"    hint    : {e.get('hint', '?')}")
    rule = RULES.get(code)
    if rule:
        print(f"    cause   : {rule['cause']}")
        print(f"    action  : {rule['action']}")
    else:
        print( "    cause   : unknown code - not in the offline rule base.")
        print( "    action  : investigate manually; consider adding a rule for it.")
    print()


def main():
    log_path = Path(sys.argv[1]) if len(sys.argv) > 1 else Path("build/serial.log")
    if not log_path.exists():
        print(f"error: log file not found: {log_path}")
        print("hint : boot the OS first (make run) so QEMU writes the serial log,")
        print("       or pass the path explicitly: python analyze_logs.py <file>")
        return 2

    text = log_path.read_text(errors="replace")
    lines, after_lines, found = extract_last_boot_report(text)

    print("=" * 72)
    print(" KamikazeOS AI RECOVERY CORE - recovery report (v2.6, rule-based)")
    print("=" * 72)

    if not found:
        # No complete report: fall back to analyzing loose log lines — this
        # still catches a panic that killed the kernel before report_send().
        print("\nNo complete boot report found - analyzing loose log lines instead.")
        events = [parse_kv_line(l) for l in text.splitlines() if l.startswith("code=")]
        meta, devices, drivers = {}, [], []
        policy, actions, suggestions, memory = [], [], [], {}
        store, journal = {}, []
        snapshots, health, timeline = [], {}, []
        modules, pkgs, pkg_policy, pkg_history = [], [], [], []
        trust, pkg_manifests, pkg_states, mod_slots, pending = {}, [], [], [], {}
        kernel_memory, memory_map, pages, kernel_heap = {}, [], {}, {}
        paging, page_map = {}, []
        syscalls, syscall_rows = {}, []
        privilege, gdt, tss, ring3_readiness = {}, {}, {}, {}
        ring3, user_memory, user_execution, user_fault = {}, {}, {}, {}
        kernel_interrupts, kernel_timing, kernel_exceptions = {}, {}, {}
        kernel_tasks, task_rows = {}, []
        tinyfs, tinyfs_files = {}, []
        apps, app_rows, app_log = {}, [], []
        system_status, diagnostics = {}, {}
    else:
        (meta, devices, drivers, events, policy, actions, suggestions,
         memory, store, journal, snapshots, health, timeline,
         modules, pkgs, pkg_policy, pkg_history,
         trust, pkg_manifests, pkg_states, mod_slots, pending,
         kernel_memory, memory_map, pages, kernel_heap,
         paging, page_map, syscalls, syscall_rows,
         privilege, gdt, tss, ring3_readiness,
         ring3, user_memory, user_execution, user_fault,
         kernel_interrupts, kernel_timing, kernel_exceptions,
         kernel_tasks, task_rows, tinyfs, tinyfs_files,
         apps, app_rows, app_log, system_status, diagnostics) = parse_report(lines)
        if after_lines:
            # Events streamed after the report (e.g. a panic) — merge them in.
            print(f"\nNote: {len(after_lines)} event(s) occurred AFTER the boot "
                  "report was exported (merged below).")
            events.extend(parse_kv_line(l) for l in after_lines)

    page_fault = parse_page_fault_evidence(text)
    if page_fault:
        paging.update(page_fault)
    post_user_fault = parse_section_evidence(text, "[USER_FAULT]")
    if post_user_fault:
        user_fault.update(post_user_fault)

    memory_safe = user_memory and all(user_memory.get(key) == "yes" for key in (
        "validated", "code_user_accessible", "code_read_only",
        "stack_user_accessible", "stack_writable", "kernel_supervisor_only",
        "page_structures_supervisor_only", "no_broad_user_exposure"))
    if user_memory and (user_memory.get("kernel_supervisor_only") != "yes" or
                        user_memory.get("page_structures_supervisor_only") != "yes" or
                        user_memory.get("no_broad_user_exposure") != "yes"):
        events.append({"code": "RING3-MEM-CRITICAL", "severity": "panic",
                       "component": "Ring3", "message": "Unsafe CPL3 page permissions reported",
                       "hint": "disable Ring 3 and inspect USER bits"})
    if ring3.get("execution_enabled") == "yes" and not memory_safe:
        events.append({"code": "RING3-VALIDATION", "severity": "panic",
                       "component": "Ring3", "message": "CPL3 enabled without complete validation",
                       "hint": "keep execution disabled until every check passes"})
    roundtrip_claimed = ring3.get("test_status") == "passed"
    roundtrip_valid = roundtrip_claimed and all((
        ring3.get("approval_consumed") == "yes",
        ring3.get("returned_to_kernel") == "yes",
        ring3.get("esp0_switch_observed") == "yes",
        ring3.get("context_preservation_verified") == "yes",
        user_execution.get("last_cpl") == "3",
        user_execution.get("last_syscall") == "9",
        user_execution.get("exit_result") == "0",
        user_fault.get("present") != "yes",
        memory_safe,
    ))
    if roundtrip_claimed and not roundtrip_valid:
        events.append({"code": "RING3-VALIDATION", "severity": "panic",
                       "component": "Ring3", "message": "Incomplete CPL3 round-trip success evidence",
                       "hint": "keep execution disabled and inspect saved origin, ESP0, return, and mapping evidence"})
    if roundtrip_valid:
        ring3_readiness["readiness"] = "roundtrip_verified"

    if meta:
        print(f"\nKernel version : {meta.get('version', '?')}")
        print(f"GhostAttach    : {meta.get('scan_total', '?')} devices - "
              f"{meta.get('scan_attached', '?')} attached, "
              f"{meta.get('scan_fallback', '?')} fallback, "
              f"{meta.get('scan_missing', '?')} missing, "
              f"{meta.get('scan_isolated', '?')} isolated "
              f"({meta.get('scan_expected_self_tests', '0')} expected test)")

    if system_status:
        print("\nv2.6 integrated system status:")
        print(f"  * Kernel: {system_status.get('kernel_version', '?')}")
        print(f"  * Interrupts: IDT={system_status.get('idt_loaded', '?')} "
              f"PIC={system_status.get('pic_remapped', '?')} "
              f"keyboard={system_status.get('keyboard_mode', '?')}")
        print(f"  * Memory: map={system_status.get('memory_map', '?')} "
              f"free_pages={system_status.get('free_pages', '?')}")
        print(f"  * Heap: available={system_status.get('heap_available', '?')} "
              f"remaining={system_status.get('heap_remaining', '?')} bytes")
        print(f"  * Paging: prepared={system_status.get('paging_prepared', '?')} "
              f"enabled={system_status.get('paging_enabled', '?')} "
              f"mode={system_status.get('paging_mode', '?')}")
        print(f"  * Storage: {system_status.get('recovery_backend', '?')} "
              f"TinyFS={system_status.get('tinyfs_status', '?')} "
              f"files={system_status.get('tinyfs_files', '?')}")
        print(f"  * Apps: {system_status.get('app_registry', '?')} fixed built-ins")
        print(f"  * Health: {system_status.get('health_score', '?')}/100")
        print(f"  * Package trust: {system_status.get('package_trust_mode', '?')}")
        print(f"  * Last critical: {system_status.get('last_critical', 'none')}")
        if diagnostics.get("recommended_command"):
            print(f"  * Recommended manual command: "
                  f"{diagnostics.get('recommended_command')}")
        print("  * Automatic repair: no")

    if kernel_memory:
        print("\nKernel memory foundation:")
        print(f"  * Map available: {kernel_memory.get('map_available', '?')}")
        print(f"  * Fallback used: {kernel_memory.get('fallback_used', '?')}")
        print(f"  * Regions: {kernel_memory.get('region_count', '?')}")
        print(f"  * Total: {kernel_memory.get('total_kib', '?')} KiB")
        print(f"  * Usable: {kernel_memory.get('usable_kib', '?')} KiB")
        print(f"  * Allocation mode: {kernel_memory.get('allocation_mode', '?')}")
        print(f"  * Map validated/normalized: "
              f"{kernel_memory.get('map_validated', '?')}/"
              f"{kernel_memory.get('normalized', '?')}")
        print(f"  * Defensive parser test: "
              f"{kernel_memory.get('hardening_self_test', '?')}")
        print(f"  * Bitmap/critical reservations: "
              f"{kernel_memory.get('bitmap_consistent', '?')}/"
              f"{kernel_memory.get('critical_ranges_reserved', '?')}")
        print(f"  * Map validation result: "
              f"{kernel_memory.get('validation_error', '?')}")
        if (kernel_memory.get("hardening_self_test") != "pass" or
                kernel_memory.get("bitmap_consistent") != "yes" or
                kernel_memory.get("critical_ranges_reserved") != "yes"):
            print("  !! WARNING: physical memory tracking is not safe for allocation.")
        print(f"  * Page tracker: {pages.get('free_pages', '?')} free / "
              f"{pages.get('total_pages', '?')} tracked pages")
        if kernel_memory.get("map_available") != "yes":
            print("  !! WARNING: Multiboot memory map missing; kernel used fallback.")
        try:
            free_pages = int(pages.get("free_pages", "0") or 0)
            if free_pages < 128:
                print("  !! WARNING: free physical pages are very low.")
        except ValueError:
            pass

    if paging:
        print("\nPaging foundation:")
        print(f"  * Prepared: {paging.get('prepared', '?')}")
        print(f"  * Enabled: {paging.get('enabled', '?')}")
        print(f"  * Mode: {paging.get('mode', '?')}")
        print(f"  * Identity map valid: {paging.get('identity_map_valid', '?')}")
        print(f"  * Required ranges valid: {paging.get('required_ranges_valid', '?')}")
        print(f"  * Page directory: {paging.get('page_directory', '?')}")
        print(f"  * Active CR3: {paging.get('active_cr3', '?')}")
        print(f"  * Mapped range: {paging.get('mapped_start', '?')} - "
              f"{paging.get('mapped_end', '?')}")
        print(f"  * Page fault handler: {paging.get('page_fault_handler', '?')}")
        if paging.get("enabled") == "yes":
            print("  * CR0.PG is enabled with the validated identity map.")
        else:
            print("  * Enable gate held; kernel continued with paging disabled.")
        if paging.get("identity_map_valid") != "yes":
            print("  !! WARNING: identity map validation failed.")
        if paging.get("present") == "yes":
            print(f"  !! Page fault evidence: address "
                  f"{paging.get('fault_address')} error "
                  f"{paging.get('error_code', '?')} eip {paging.get('eip', '?')}")

    if syscalls:
        print("\nKernel syscall foundation:")
        print(f"  * Initialized: {syscalls.get('initialized', '?')}")
        print(f"  * IDT vector: {syscalls.get('vector', '?')} (0x80)")
        print(f"  * Registered calls: {syscalls.get('registered_count', '?')}")
        print(f"  * Calls at export: {syscalls.get('total_calls', '?')}")
        print(f"  * Failed calls: {syscalls.get('failed_calls', '?')}")
        print(f"  * Last call: {syscalls.get('last_id', '?')} "
              f"{syscalls.get('last_name', '?')} "
              f"({syscalls.get('last_result', '?')})")
        print(f"  * Argument model: {syscalls.get('argument_model', '?')}")
        print(f"  * User mode available: "
              f"{syscalls.get('user_mode_available', 'no')}")
        print(f"  * Gate DPL: {syscalls.get('syscall_gate_dpl', '?')}")
        print(f"  * User-callable gate: {syscalls.get('syscall_user_gate', 'disabled')}")
        print(f"  * Last caller CPL: {syscalls.get('last_caller_cpl', '?')}")
        print(f"  * Last validated origin: {syscalls.get('last_origin', '?')} "
              f"(valid={syscalls.get('last_origin_valid', '?')})")
        print(f"  * CPL0/CPL3 calls: {syscalls.get('cpl0_calls', '0')}/"
              f"{syscalls.get('cpl3_calls', '0')}")
        print(f"  * ESP0 switch observed: {syscalls.get('esp0_switch_observed', 'no')}")
        print(f"  * Origin policy self-test: "
              f"{syscalls.get('origin_policy_self_test', '?')}")
        print(f"  * Register preservation test: "
              f"{syscalls.get('register_preservation_test', '?')}")
        print("  * Boundary: a fixed CPL3 routine and narrow allowlist are prepared, but execution remains approval-gated; no pointer ABI exists.")
        if syscalls.get("initialized") != "yes":
            print("  !! WARNING: syscall gate is not initialized.")
        if (syscalls.get("origin_policy_self_test") != "pass" or
                syscalls.get("register_preservation_test") != "pass"):
            print("  !! WARNING: syscall boundary defensive self-tests failed.")
        try:
            if int(syscalls.get("failed_calls", "0") or 0) > 0:
                print("  !! WARNING: failed or unknown syscall attempts were recorded.")
        except ValueError:
            print("  !! WARNING: invalid syscall failure counter in report.")
        for row in syscall_rows:
            print(f"    - {row.get('id', '?'):>2} "
                  f"{row.get('name', '?'):<16} "
                  f"args={row.get('argument_mode', '?')} "
                  f"caller={row.get('caller_mode', '?')}")

    if privilege or tss or ring3_readiness:
        print("\nPrivilege and TSS foundation:")
        print(f"  * Current CPL: {privilege.get('current_cpl', '?')} "
              "(kernel shell must remain 0)")
        print(f"  * User segments valid: "
              f"{privilege.get('user_segments_valid', '?')}")
        print(f"  * TSS loaded: {tss.get('tss_loaded', '?')}")
        print(f"  * TSS selector: {tss.get('selector', '?')}")
        print(f"  * SS0:ESP0: {tss.get('ss0', '?')}:{tss.get('esp0', '?')}")
        print(f"  * Transition stack: {tss.get('stack_bottom', '?')} - "
              f"{tss.get('stack_top', '?')}")
        print(f"  * Guard policy: {tss.get('guard_policy', '?')}")
        print(f"  * Syscall gate DPL: "
              f"{privilege.get('syscall_gate_dpl', '?')}")
        print(f"  * Foundation readiness: "
              f"{ring3_readiness.get('readiness', '?')}")
        print(f"  * Ring 3 execution enabled: "
              f"{ring3_readiness.get('execution_enabled', 'no')}")
        print("  * Still missing: ELF loading, process isolation, multiple processes, and stable userspace ABI.")
        if tss.get("tss_loaded") != "yes" or \
                tss.get("kernel_stack_valid") != "yes":
            print("  !! WARNING: TSS or ESP0 kernel stack is invalid.")
        if privilege.get("current_cpl") not in (None, "0"):
            print("  !! WARNING: exported kernel shell CPL is not Ring 0.")
        if ring3_readiness.get("readiness") not in (
                "foundation_ready", "controlled_test_ready", "roundtrip_verified"):
            print("  !! WARNING: Ring 3 controlled-test readiness is incomplete.")

    if ring3 or user_memory or user_execution:
        print("\nControlled Ring 3 seed:")
        print(f"  * Execution enabled: {ring3.get('execution_enabled', 'no')}")
        print(f"  * Approval consumed: {ring3.get('approval_consumed', 'no')}")
        print(f"  * Fault test approved: {ring3.get('fault_test_approved', 'no')}")
        print(f"  * Test status: {ring3.get('test_status', '?')}")
        print(f"  * User code: {user_memory.get('user_code_start', '?')} - {user_memory.get('user_code_end', '?')} (read-only)")
        print(f"  * User stack: {user_memory.get('user_stack_start', '?')} - {user_memory.get('user_stack_end', '?')} (fixed)")
        print(f"  * Kernel supervisor-only: {user_memory.get('kernel_supervisor_only', '?')}")
        print(f"  * Broad user exposure: {'no' if user_memory.get('no_broad_user_exposure') == 'yes' else 'WARNING'}")
        print(f"  * Last CPL/syscall: {user_execution.get('last_cpl', '?')}/{user_execution.get('last_syscall', '?')}")
        print(f"  * ESP0 switch: {user_execution.get('esp0_switch_observed', '?')}")
        print(f"  * Returned to kernel: {user_execution.get('returned_to_kernel', '?')}")
        print(f"  * Context preserved: {user_execution.get('context_preservation_verified', '?')}")
        if roundtrip_valid:
            print("  * Boundary: the fixed linked routine completed one verified round trip; its approval was consumed and the gate is held.")
        else:
            print("  * Boundary: one linked trusted routine is prepared and held; no ELF, TinyFS execution, or untrusted code.")

    if user_fault.get("present") == "yes":
        intentional = user_fault.get("intentional_test") == "yes"
        print("\nCPL3 fault evidence:")
        print(f"  * Origin: {user_fault.get('fault_origin', '?')}")
        print(f"  * Vector/address: {user_fault.get('vector', '?')}/{user_fault.get('fault_address', '?')}")
        print(f"  * User ESP/SS: {user_fault.get('user_esp', '?')}/{user_fault.get('user_ss', '?')}")
        print(f"  * Intentional disposable test: {'yes' if intentional else 'no'}")
        print("  * Recovery attempted: no; exception evidence was preserved before halt.")

    if kernel_heap:
        print("\nKernel heap foundation:")
        print(f"  * Available: {kernel_heap.get('available', '?')}")
        print(f"  * Total: {kernel_heap.get('total_bytes', '?')} bytes")
        print(f"  * Used: {kernel_heap.get('used_bytes', '?')} bytes")
        print(f"  * Remaining: {kernel_heap.get('remaining_bytes', '?')} bytes")
        print(f"  * Allocations: {kernel_heap.get('allocation_count', '?')}")
        print(f"  * Failed allocations: {kernel_heap.get('failed_count', '?')}")
        print(f"  * free(): {kernel_heap.get('free_supported', 'no')}")
        print("  * Boundary: fixed kernel bump heap only; no userspace heap or VM.")
        if kernel_heap.get("available") != "yes":
            print("  !! WARNING: kernel heap unavailable.")
        try:
            remaining = int(kernel_heap.get("remaining_bytes", "0") or 0)
            total = int(kernel_heap.get("total_bytes", "0") or 0)
            if total and remaining * 10 < total:
                print("  !! WARNING: remaining heap is below 10%.")
        except ValueError:
            pass

    if kernel_interrupts:
        print("\nKernel interrupt foundation:")
        print(f"  * GDT loaded: {kernel_interrupts.get('gdt_loaded', '?')}")
        print(f"  * IDT loaded: {kernel_interrupts.get('idt_loaded', '?')}")
        print(f"  * IRQ gates installed: "
              f"{kernel_interrupts.get('irq_gates_installed', '?')}")
        print(f"  * PIC remapped: {kernel_interrupts.get('pic_remapped', '?')}")
        print(f"  * Timer IRQ enabled: {kernel_interrupts.get('timer_enabled', '?')}")
        print(f"  * Keyboard mode: {kernel_interrupts.get('keyboard_mode', '?')}")
        print(f"  * Interrupts handled at export: "
              f"{kernel_interrupts.get('interrupts_handled', '?')}")
        for key, label in (("gdt_loaded", "GDT"), ("idt_loaded", "IDT"),
                           ("irq_gates_installed", "IRQ gates"),
                           ("pic_remapped", "PIC remap"),
                           ("timer_enabled", "PIT timer IRQ")):
            if kernel_interrupts.get(key) != "yes":
                print(f"  !! WARNING: {label} is not operational.")
        if kernel_interrupts.get("keyboard_mode") == "polling_fallback":
            print("  * Keyboard IRQ was unavailable; polling fallback preserved input.")

    if kernel_timing:
        print(f"\nKernel timing: {kernel_timing.get('tick_count', '?')} ticks at "
              f"{kernel_timing.get('frequency_hz', '?')} Hz "
              f"({kernel_timing.get('timer_irq', '?')})")

    if kernel_exceptions and kernel_exceptions.get("last_exception") != "none":
        print("\nCPU exception evidence:")
        print(f"  * Exception: {kernel_exceptions.get('last_exception', '?')}")
        print(f"  * Vector: {kernel_exceptions.get('last_vector', '?')}")
        print(f"  * Error code: {kernel_exceptions.get('last_error_code', '?')}")
        print(f"  * CS:EIP / EFLAGS: {kernel_exceptions.get('last_cs', '?')}:"
              f"{kernel_exceptions.get('last_eip', '?')} / "
              f"{kernel_exceptions.get('last_eflags', '?')}")
        print(f"  * Caller CPL: {kernel_exceptions.get('caller_cpl', '?')}")
        print(f"  * User stack present: "
              f"{kernel_exceptions.get('user_stack_present', 'no')}")
        if kernel_exceptions.get("user_stack_present") == "yes":
            print(f"  * User SS:ESP: {kernel_exceptions.get('user_ss', '?')}:"
                  f"{kernel_exceptions.get('user_esp', '?')}")
        if kernel_exceptions.get("origin_valid") != "yes":
            print("  !! WARNING: saved exception selectors do not match the expected origin.")
        print("  * Recovery attempted: no (intentional v2.0 safety policy)")
    if kernel_exceptions and kernel_exceptions.get("frame_layout_valid") != "yes":
        print("\n  !! WARNING: interrupt frame ABI self-test did not pass.")

    if kernel_tasks:
        print("\nKernel task foundation:")
        print(f"  * Scheduler mode: {kernel_tasks.get('scheduler_mode', '?')}")
        print(f"  * Preemptive switching: "
              f"{kernel_tasks.get('preemptive_switching', '?')}")
        print(f"  * Current task: {kernel_tasks.get('running_task', '?')}")
        print(f"  * Tasks: {kernel_tasks.get('total_tasks', '?')} total, "
              f"{kernel_tasks.get('ready_count', '?')} ready, "
              f"{kernel_tasks.get('sleeping_count', '?')} sleeping, "
              f"{kernel_tasks.get('blocked_count', '?')} blocked, "
              f"{kernel_tasks.get('crashed_count', '?')} crashed")
        print("  * Timer IRQ accounts ticks only; task switches require an "
              "explicit cooperative yield.")
        blocked_count = int(kernel_tasks.get("blocked_count", "0") or 0)
        crashed_count = int(kernel_tasks.get("crashed_count", "0") or 0)
        if blocked_count:
            print("  !! WARNING: blocked kernel task records require manual inspection.")
        if crashed_count:
            print("  !! WARNING: crashed kernel task records require manual inspection.")
        if task_rows:
            for task in task_rows:
                print(f"    - {task.get('task_id', '?'):>2} "
                      f"{task.get('name', '?'):<18} "
                      f"{task.get('state', '?'):<9} "
                      f"ticks={task.get('ticks_run', '?')}")

    if tinyfs:
        print("\nTinyFS status:")
        print(f"  * Available: {tinyfs.get('available', '?')}")
        print(f"  * Formatted: {tinyfs.get('formatted', '?')}")
        print(f"  * Files: {tinyfs.get('file_count', '?')} / "
              f"{tinyfs.get('max_files', '?')}")
        print(f"  * Data starts at LBA {tinyfs.get('data_start_lba', '?')}")
        print(f"  * Last error: {tinyfs.get('last_error', '?')}")
        if tinyfs.get("available") != "yes":
            print("  !! WARNING: TinyFS is unavailable because the ATA disk is missing.")
        elif tinyfs.get("formatted") != "yes":
            print("  !! WARNING: TinyFS is not formatted. Formatting requires the "
                  "manual `fs-format confirm` command inside KamikazeOS.")
        for file in tinyfs_files:
            print(f"    - {file.get('name', '?'):<24} "
                  f"{file.get('size', '?')} bytes")

    if apps:
        print("\nKernel-managed app model:")
        print(f"  * Userland mode: {apps.get('userland_mode', '?')}")
        print("  * Boundary: not real Ring 3 userspace, no ELF loader, no "
              "process isolation, no arbitrary code execution.")
        print(f"  * Apps: {apps.get('total_apps', '?')} fixed built-ins")
        print(f"  * Last app: {apps.get('last_app', '?')} "
              f"({apps.get('last_result', '?')})")
        print(f"  * Blocked run attempts: {apps.get('blocked_attempts', '0')}")
        for app in app_rows:
            print(f"    - {app.get('name', '?'):<16} "
                  f"{app.get('type', '?'):<13} "
                  f"runs={app.get('run_count', '?')} "
                  f"status={app.get('status', '?')} "
                  f"last={app.get('last_result', '?')}")
        failed = [a for a in app_rows if a.get("status") == "failed"]
        blocked = [a for a in app_rows if a.get("status") == "blocked"]
        blocked_events = [e for e in app_log if e.get("result") == "blocked"]
        if failed:
            print("  !! WARNING: one or more fixed apps failed during this boot.")
        if blocked or blocked_events or apps.get("blocked_attempts", "0") not in ("0", ""):
            print("  !! WARNING: app policy blocked at least one run attempt. "
                  "This is correct for unknown or unregistered apps.")

    if devices:
        print("\nDevice summary:")
        for d in devices:
            print(f"  - {d.get('name', '?'):<20} {d.get('status', '?'):<15} "
                  f"driver: {d.get('driver', '?')}")

    blocked = [d for d in drivers if d.get("status") == "blocked_unverified"]
    if blocked:
        print("\nBlocked drivers (unverified - the block is correct and must stay):")
        for d in blocked:
            print(f"  - {d.get('name', '?')} (risk: {d.get('risk', '?')})")

    intentional_user_fault = (user_fault.get("present") == "yes" and
                              user_fault.get("fault_origin") == "cpl3" and
                              user_fault.get("intentional_test") == "yes")
    buckets = classify(events, intentional_user_fault)
    print(f"\nIssue counts: {len(buckets['critical'])} critical, "
          f"{len(buckets['warning'])} warning, {len(buckets['info'])} info")

    if buckets["critical"]:
        print("\n--- CRITICAL ---------------------------------------------------\n")
        for e in buckets["critical"]:
            print_issue(e)
    if buckets["warning"]:
        print("--- WARNINGS ---------------------------------------------------\n")
        for e in buckets["warning"]:
            print_issue(e)
    if not buckets["critical"] and not buckets["warning"]:
        print("\nNo warnings or errors. System healthy.")

    # ----- RecoveryEngine integration (v0.5) ---------------------------
    # Recommendations come ONLY from the kernel's own fixed catalog as
    # reported in [RECOVERY_ACTIONS]/[RECOVERY_SUGGESTIONS]. The analyzer
    # cannot invent an action ID: anything not in the catalog is ignored.
    # ----- Health trend + cross-boot diffing (v0.8) ---------------------
    catalog = {a.get("id"): a for a in actions if a.get("id")}
    if snapshots:
        print("Health trend:")
        for s in snapshots:
            notes = []
            if s.get("safe_mode") == "1":
                notes.append("safe mode active")
            if s.get("panic") == "1":
                notes.append("previous panic detected")
            if int(s.get("missing", 0)) > 0:
                notes.append(f"{s.get('missing')} missing driver(s)")
            note_str = f"  ({', '.join(notes)})" if notes else ""
            print(f"  * Boot {s.get('boot', '?')}: "
                  f"{s.get('health', '?')}/100{note_str}")

        # Insight: did safe mode help? Compare each safe-mode boot with
        # the boot before it.
        for i, s in enumerate(snapshots):
            if s.get("safe_mode") == "1" and i > 0:
                delta = int(s.get("health", 0)) - int(snapshots[i-1].get("health", 0))
                if delta > 0:
                    print(f"\nInsight: safe mode improved health by +{delta} points "
                          "(boot {} vs {}) by skipping".format(
                              s.get('boot'), snapshots[i-1].get('boot')))
                    print("non-essential devices. Consider it again after failures.")
                elif delta < 0:
                    print(f"\nInsight: health declined {delta} during the safe-mode "
                          "boot - the remaining issues are in essential components.")

        # Insight: persistent problems that show up in EVERY snapshot.
        if len(snapshots) >= 2:
            if all(int(s.get("missing", 0)) > 0 for s in snapshots):
                print("\nInsight: missing drivers appear in every recorded boot -")
                print("this is a permanent gap, not a transient failure. Only a")
                print("verified driver added to the registry will close it.")
            if all(int(s.get("isolated", 0)) > 0 for s in snapshots):
                print("\nInsight: at least one device is isolated in every boot -")
                print("check the journal for which driver keeps failing.")

        # Insight: rollback effect — find a ROLLED_BACK boot in the
        # timeline and compare its health with the following boot.
        for i, t in enumerate(timeline):
            if "ROLLED_BACK" in t.get("events", "") and i + 1 < len(timeline):
                d = int(timeline[i+1].get("health", 0)) - int(t.get("health", 0))
                word = "improved" if d > 0 else ("worsened" if d < 0 else "left unchanged")
                print(f"\nInsight: the rollback in boot {t.get('boot')} {word} "
                      f"the next boot's health ({d:+d}).")
        print()

    if health:
        cur  = health.get("score", "?")
        prev = health.get("prev_score", "none")
        delta = health.get("health_delta", "none")
        print(f"Current health: {cur}/100", end="")
        if prev != "none":
            print(f"  (previous boot: {prev}/100, delta {delta})")
        else:
            print("  (no previous snapshot to compare)")
        print()

    if store:
        backend = store.get("backend", "?")
        print("Durable Recovery Store:")
        print(f"  * Backend: {backend}", end="")
        if backend == "disk+CMOS":
            print("  (disk-backed - survives full emulator restarts)")
        elif backend == "CMOS_only":
            print("  (CMOS only - survives reboots, NOT closing the emulator)")
        else:
            print("  (fresh - no prior state found)")
        print(f"  * State block valid: "
              f"{'yes' if store.get('state_valid') == '1' else 'no'}")
        print(f"  * Journal entries: {store.get('journal_count', '?')}")
        print(f"  * Durable boot count: {store.get('durable_boot_count', '?')}")
        if store.get("cmos_fallback_used") == "1":
            print("  * NOTE: state was hydrated from the CMOS fallback this boot.")
        if "state_source" in store:
            print(f"  * State copy used: {store.get('state_source')} "
                  f"(generation {store.get('state_generation', '?')}, "
                  f"{store.get('state_redundancy', '?')})")
            print(f"  * Journal: {store.get('journal_count', '?')}/"
                  f"{store.get('journal_capacity', '?')} slots, "
                  f"full={store.get('journal_full', '?')}, "
                  f"appends refused this boot={store.get('journal_dropped', '?')}, "
                  f"corrupt entries preserved={store.get('journal_corrupt_entries', '?')}")
            if store.get("state_locked") == "1" or store.get("journal_locked") == "1":
                print("  * WARNING: store is READ-ONLY this boot (ambiguous or "
                      "unreadable state) - nothing was overwritten.")
        print()

    if journal:
        print("Durable recovery history (latest journal entries):")
        for j in journal:
            act = j.get("action", "none")
            act_str = f" {act}" if act != "none" else ""
            print(f"  boot {j.get('boot', '?'):>3}  "
                  f"{j.get('event', '?'):<15}{act_str}  "
                  f"[{j.get('result', '?')}]")

        # Cross-boot pattern detection — only possible because the journal
        # is durable. These are the analyzer's most valuable signals.
        panics  = [j for j in journal if j.get("event") == "PANIC"]
        blocked = [j for j in journal if j.get("event") == "BLOCKED"]
        if len(panics) >= 2:
            boots = sorted({j.get("boot", "?") for j in panics})
            print(f"\n  !! PATTERN: {len(panics)} panics across boots "
                  f"{', '.join(boots)} - this is recurring, not a one-off.")
            print("     Investigate before relying on this machine.")
        if len(blocked) >= 2:
            print(f"\n  !! PATTERN: {len(blocked)} blocked high-risk attempts "
                  "recorded. The policy gate held every time, but repeated")
            print("     attempts are themselves worth investigating.")
        print()
    if memory:
        yn = lambda k: "yes" if memory.get(k) == "1" else "no"
        print("Recovery Memory (persistent, survives reboot):")
        print(f"  * Boot count: {memory.get('boot_count', '?')}")
        print(f"  * Previous panic detected: {yn('last_boot_panic')}")
        print(f"  * Safe mode active this boot: {yn('safe_mode_active')}")
        print(f"  * Safe mode scheduled: {yn('safe_mode_next_boot')}")
        print(f"  * Network quarantine persisted: {yn('network_quarantined')}")
        print(f"  * Audio quarantine persisted: {yn('audio_quarantined')}")
        print(f"  * Rollback available: {yn('rollback_available')}")

        if memory.get("network_quarantined") == "1" or \
           memory.get("audio_quarantined") == "1":
            print("\n  Persisted quarantines are INTENTIONAL: a human approved")
            print("  them via the RecoveryEngine, and the kernel restores them")
            print("  at every boot. Undo inside KamikazeOS with rollback <id>.")

        if memory.get("rollback_available") == "1":
            print("\n  Note: at least one reversible action is in effect and can")
            print("  be rolled back inside KamikazeOS.")

        # Previous panic is the strongest signal we have: recommend safe
        # mode — but ONLY by pointing at the catalog's own ACT-004, and
        # only if the kernel reports it as approvable.
        if memory.get("last_boot_panic") == "1":
            act4 = catalog.get("ACT-004")
            if act4 and act4.get("status") not in ("blocked", "applied"):
                print("\nRecommended manual action:")
                print(f"  * ACT-004 {act4.get('name', 'boot_safe_mode_next')}")
                print("    Reason: previous boot ended in panic")
                print("    Approve inside KamikazeOS with: approve ACT-004")
            elif act4 and act4.get("status") == "applied":
                print("\n  Previous panic detected and safe mode is already")
                print("  scheduled - good. Reboot to enter it.")
        print()

    if catalog:
        suggested_ids = [s.get("suggested") for s in suggestions
                         if s.get("suggested") in catalog]   # catalog-only
        applied = [a for a in actions if a.get("status") == "applied"]
        blocked_actions = [a for a in actions if a.get("status") == "blocked"]

        print("Recommended manual recovery actions:")
        if suggested_ids:
            for aid in suggested_ids:
                a = catalog[aid]
                print(f"\n  * {aid} {a.get('name', '?')}")
                print(f"    Risk: {a.get('risk', '?')}")
                print(f"    Reversible: {a.get('reversible', '?')}")
                print(f"    Approve inside KamikazeOS with: approve {aid}")
        else:
            print("  none - the kernel's suggestion rules matched nothing.")

        if applied:
            print("\nAlready applied:")
            for a in applied:
                # Only user-approved reversible actions can be rolled back;
                # policy-enforced ones (requires_approval=no) cannot.
                undoable = (a.get("reversible") == "yes"
                            and a.get("requires_approval") == "yes")
                suffix = (f"  (undo with: rollback {a.get('id')})" if undoable
                          else "  (policy-enforced, not user-undoable)")
                print(f"  * {a.get('id')} {a.get('name')}{suffix}")

        if blocked_actions:
            print("\nBlocked by policy (cannot be approved by anyone - correct):")
            for a in blocked_actions:
                print(f"  * {a.get('id')} {a.get('name')}  "
                      f"(risk: {a.get('risk')}, reversible: {a.get('reversible')})")

        flag = next((parse_kv_line(p).get("safe_mode_next_boot")
                     for p in policy if "safe_mode_next_boot" in p), None)
        if flag == "1":
            print("\nNote: safe-mode-next-boot flag is SET (simulated in v0.5).")
        print()

    # ----- Trust + slots + pending verification -------------------------
    if trust:
        print(f"Trust anchor: {trust.get('anchor_id', '?')}  "
              f"fingerprint {trust.get('fingerprint', '?')}")
        print(f"  Signature verification mode: "
              f"{trust.get('signature_verification', '?')} in v1.2 "
              "- host signing is real; kernel Ed25519 is simulated. "
              "Unknown anchors are blocked.")
        print()

    if mod_slots:
        print("Module A/B slots:")
        for s in mod_slots:
            line = (f"  * {s.get('module', '?'):<24} active {s.get('active', '?')}"
                    f"  A={s.get('slot_a', '?')}"
                    f"{'(ok)' if s.get('slot_a_verified') == '1' else '(?)'}"
                    f"  B={s.get('slot_b', '?')}"
                    f"{'(ok)' if s.get('slot_b_verified') == '1' else '(?)'}")
            if s.get("pending_verify") == "1":
                line += "  << PENDING VERIFY"
            print(line)
        print()

    if pending.get("pending") == "1":
        # The single most important package recommendation: an applied update
        # was never verified (possibly across a reboot or crash).
        applied_unverified = [p for p in pkgs if p.get("status") == "applied"]
        print("!! An applied update is PENDING VERIFICATION.")
        for p in applied_unverified:
            print(f"   Recommended now: verify-pkg {p.get('id')}   "
                  f"(or rollback-pkg {p.get('id')} to return to known-good)")
        if not applied_unverified:
            print("   Run `slots` inside KamikazeOS to find it, then verify-pkg.")
        print()

    staged_states = [s for s in pkg_states
                     if s.get("transaction_phase", "committed") not in
                     ("committed", "legacy_unbound")]
    if staged_states:
        print("!! Package transaction evidence remains staged:")
        for state in staged_states:
            print(f"  * {state.get('id', '?')}: "
                  f"{state.get('transaction_phase', '?')}")
        print("   No automatic slot change is recommended. Reboot once to let the")
        print("   kernel reconcile metadata, then inspect `packages` and `slots`.\n")

    # ----- RecoveryPackage analysis ------------------------------------
    if modules:
        print("Module versions:")
        for m in modules:
            tag = "  [updated this session]" if m.get("updated") == "1" else ""
            print(f"  * {m.get('module', '?'):<26} {m.get('version', '?')}{tag}")
        print()

    if pkgs:
        # Recommend updates: a non-blocked package whose from-version
        # matches the running module version. Package IDs come ONLY from
        # the kernel's own reported registry — inventing one is impossible.
        mod_ver = {m.get("module"): m.get("version") for m in modules}
        target_to_module = {
            "GhostAttach": "GhostAttach",
            "AI Recovery Core": "AI Recovery Core Rules",
            "RecoveryEngine": "RecoveryEngine",
            "Recovery Center": "Recovery Center Schema",
        }
        updates, blocked_pkgs, failed_pkgs = [], [], []
        for p in pkgs:
            if not p.get("id"):
                continue
            if p.get("status") == "blocked":
                blocked_pkgs.append(p)
            elif p.get("status") == "failed":
                failed_pkgs.append(p)
            elif p.get("policy_result") == "allowed" and \
                    p.get("signature_valid") == "yes" and \
                    p.get("status") in ("available", "suggested"):
                running = mod_ver.get(target_to_module.get(p.get("target"), ""))
                if running == p.get("from"):
                    updates.append(p)

        if updates:
            print("Available updates (run the full flow for each):")
            for p in updates:
                src = p.get("source", "builtin_seed")
                print(f"\n  * {p.get('id')} {p.get('name')}  "
                      f"({p.get('from')} -> {p.get('to')}, risk {p.get('risk')}, "
                      f"signature {p.get('signature')}, source {src})")
                print(f"    approve-pkg {p.get('id')}")
                print(f"    apply-pkg {p.get('id')}")
                print(f"    verify-pkg {p.get('id')}")
            print()

        if failed_pkgs:
            print("FAILED packages (verification did not pass):")
            for p in failed_pkgs:
                print(f"  * {p.get('id')} {p.get('name')}")
                print(f"    Recommended: rollback-pkg {p.get('id')}")
            print()

        if blocked_pkgs:
            print("Blocked packages (the block is correct and unconditional):")
            for p in blocked_pkgs:
                why = []
                if p.get("signature") in ("unsigned", "invalid"):
                    why.append(f"signature is {p.get('signature')}")
                if p.get("risk") == "high":
                    why.append("high risk")
                if p.get("reversible") == "no":
                    why.append("irreversible")
                if p.get("checksum_valid") == "no":
                    why.append("checksum mismatch")
                if p.get("trust_valid") == "no":
                    why.append("trust invalid")
                if p.get("block_reason") and p.get("block_reason") != "none":
                    why.append(p.get("block_reason"))
                print(f"  * {p.get('id')} {p.get('name')} - {', '.join(why)}.")
                print("    No approval can override this; a future verified, "
                      "signed, reversible")
                print("    package is the only path.")
            print()

        imported = [p for p in pkgs if p.get("source") == "disk_imported"]
        if imported:
            print("Imported disk manifests:")
            for p in imported:
                print(f"  * {p.get('id')} source={p.get('source')} "
                      f"checksum={p.get('checksum_valid', '?')} "
                      f"trust={p.get('trust_valid', '?')} "
                      f"signature={p.get('signature_valid', '?')} "
                      f"policy={p.get('policy_result', '?')}")
                if p.get("policy_result") == "blocked":
                    print(f"    blocked: {p.get('block_reason', '?')}")
            print()

        verified = [p for p in pkgs if p.get("status") == "verified"]
        applied = [p for p in pkgs if p.get("status") == "applied"]
        for p in verified:
            print(f"Applied & verified: {p.get('id')} {p.get('name')} "
                  f"({p.get('from')} -> {p.get('to')}).")
        for p in applied:
            print(f"Applied but NOT yet verified: {p.get('id')} - "
                  f"run: verify-pkg {p.get('id')}")
        if verified or applied:
            print()

    if pkg_history:
        print("Package history (durable journal):")
        for h in pkg_history:
            print(f"  boot {h.get('boot', '?'):>3}  {h.get('event', '?'):<16} "
                  f"{h.get('package', '?')}  [{h.get('result', '?')}]")
        print()

    print("-" * 72)
    print("AI Recovery Core analyzed durable recovery history.")
    print("Suggestions come only from the verified manifest registry.")
    print("AI Recovery Core cannot apply packages.")
    print("Human approval inside KamikazeOS is required.")
    print("Signature verification mode: host Ed25519 real, kernel Ed25519 simulated in v1.2.")
    print("-" * 72)

    return 1 if buckets["critical"] else 0


# --------------------------------------------------------------------------
# FUTURE AI INTEGRATION POINT (deliberately not implemented in v2.6)
# --------------------------------------------------------------------------
def analyze_with_ai_later(log_text: str):
    """
    Placeholder for real AI analysis in a later phase.

    The plan (and its guardrails):
      * Call a language model (e.g. Anthropic's Claude via the `anthropic`
        package, or a local model) with ONLY the structured log text —
        never binaries, never secrets, never executable content.
      * The system prompt will constrain the model to:
          - explain failures in plain language,
          - choose suggested fixes ONLY from a fixed catalog of known-safe
            recovery actions (quarantine driver X, boot safe mode, ...),
          - assign each suggestion a risk class (low/medium/high).
      * Model output is ADVICE. It flows into the same human-approval path
        as this rule-based analyzer - the policy engine, not the model,
        decides what may ever be applied automatically.

    Keeping this stub un-implemented in v2.6 is intentional: the transport,
    format, and approval workflow get built and trusted first.
    """
    raise NotImplementedError("AI analysis arrives in a later phase - see docstring")


if __name__ == "__main__":
    sys.exit(main())
