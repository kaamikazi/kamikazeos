#!/usr/bin/env python3
"""
KamikazeOS AI Recovery Core — host-side log analyzer (v1.2, offline/rule-based)

This is the first working incarnation of the "maintenance brain". It runs on
the HOST, outside the kernel, and its powers are deliberately limited:

    READ logs  ->  EXPLAIN problems  ->  RANK risk  ->  SUGGEST next actions

It never patches binaries, installs drivers, downloads code, or modifies the
OS. Every suggestion is for a HUMAN to approve and perform. v1.2 is
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


def parse_report(lines):
    """Split the report into its sections."""
    meta, devices, drivers, events = {}, [], [], []
    policy, actions, suggestions = [], [], []
    memory = {}
    section = "meta"
    section_map = {
        "[DEVICES]": "devices",
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
        "[ERROR_LOG]": "events",
    }
    store, journal = {}, []
    snapshots, health, timeline = [], {}, []
    modules, pkgs, pkg_policy, pkg_history = [], [], [], []
    trust, pkg_manifests, pkg_states, mod_slots, pending = {}, [], [], [], {}
    for line in lines:
        if line in section_map:
            section = section_map[line]
        elif section == "meta":
            meta.update(parse_kv_line(line))
        elif section == "devices":
            devices.append(parse_kv_line(line))
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
        elif section == "events":
            events.append(parse_kv_line(line))
    return (meta, devices, drivers, events, policy, actions, suggestions,
            memory, store, journal, snapshots, health, timeline,
            modules, pkgs, pkg_policy, pkg_history,
            trust, pkg_manifests, pkg_states, mod_slots, pending)


def classify(events):
    """Bucket events into info / warning / critical."""
    buckets = {"info": [], "warning": [], "critical": []}
    for e in events:
        cls = SEVERITY_CLASS.get(e.get("severity", ""), "warning")
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
    print(" KamikazeOS AI RECOVERY CORE - recovery report (v1.2, rule-based)")
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
    else:
        (meta, devices, drivers, events, policy, actions, suggestions,
         memory, store, journal, snapshots, health, timeline,
         modules, pkgs, pkg_policy, pkg_history,
         trust, pkg_manifests, pkg_states, mod_slots, pending) = parse_report(lines)
        if after_lines:
            # Events streamed after the report (e.g. a panic) — merge them in.
            print(f"\nNote: {len(after_lines)} event(s) occurred AFTER the boot "
                  "report was exported (merged below).")
            events.extend(parse_kv_line(l) for l in after_lines)

    if meta:
        print(f"\nKernel version : {meta.get('version', '?')}")
        print(f"GhostAttach    : {meta.get('scan_total', '?')} devices - "
              f"{meta.get('scan_attached', '?')} attached, "
              f"{meta.get('scan_fallback', '?')} fallback, "
              f"{meta.get('scan_missing', '?')} missing, "
              f"{meta.get('scan_isolated', '?')} isolated")

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

    buckets = classify(events)
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
            elif p.get("policy_result", "allowed") == "allowed" and \
                    p.get("signature_valid", "yes") == "yes" and \
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
# FUTURE AI INTEGRATION POINT (deliberately not implemented in v1.2)
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

    Keeping this stub un-implemented in v1.2 is intentional: the transport,
    format, and approval workflow get built and trusted first.
    """
    raise NotImplementedError("AI analysis arrives in a later phase - see docstring")


if __name__ == "__main__":
    sys.exit(main())
