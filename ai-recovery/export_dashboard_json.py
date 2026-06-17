#!/usr/bin/env python3
"""
export_dashboard_json.py — convert the latest KamikazeOS boot report into
the JSON the Recovery Center dashboard reads.

Usage:
    python export_dashboard_json.py [serial.log] [output.json]
    defaults: build/serial.log -> recovery-center/data/recovery-report.json

Besides recovery-report.json this also writes recovery-data.js next to it
(`window.RECOVERY_DATA = {...}`), so the dashboard works when opened as a
local file:// page, where browsers block fetch() of local JSON.

SAFETY: this script READS the serial log and WRITES one JSON file. It has
no path to the OS, the disk image, or any repair mechanism — by design.
"""

import json
import sys
from pathlib import Path

# Reuse the analyzer's parsing — one parser, one format contract.
from analyze_logs import RULES, extract_last_boot_report, parse_kv_line, parse_report


def build_recommendations(memory, actions, suggestions):
    """Catalog-only recommendations with the exact shell command to type."""
    catalog = {a.get("id"): a for a in actions if a.get("id")}
    recs = []
    for s in suggestions:
        aid = s.get("suggested")
        if aid not in catalog:            # never invent an action ID
            continue
        a = catalog[aid]
        recs.append({
            "id": aid,
            "name": a.get("name", ""),
            "risk": a.get("risk", ""),
            "reversible": a.get("reversible", ""),
            "reason": s.get("hint", ""),
            "command": f"approve {aid}",
        })
    # Panic-driven recommendation, same rule the analyzer applies.
    if memory.get("last_boot_panic") == "1":
        a = catalog.get("ACT-004")
        if a and a.get("status") not in ("blocked", "applied") and \
                not any(r["id"] == "ACT-004" for r in recs):
            recs.append({
                "id": "ACT-004", "name": a.get("name", ""),
                "risk": a.get("risk", ""), "reversible": a.get("reversible", ""),
                "reason": "previous boot ended in panic",
                "command": "approve ACT-004",
            })
    return recs


def build_insights(snapshots, timeline, journal):
    """Plain-English findings mirrored from the analyzer's rules."""
    insights = []
    for i, s in enumerate(snapshots):
        if s.get("safe_mode") == "1" and i > 0:
            d = int(s.get("health", 0)) - int(snapshots[i - 1].get("health", 0))
            if d > 0:
                insights.append(f"Safe mode improved health by +{d} points "
                                f"(boot {s.get('boot')}).")
    if len(snapshots) >= 2 and all(int(s.get("missing", 0)) > 0 for s in snapshots):
        insights.append("Missing drivers appear in every recorded boot - a "
                        "permanent gap, not a transient failure.")
    panics = [j for j in journal if j.get("event") == "PANIC"]
    if len(panics) >= 2:
        insights.append(f"{len(panics)} panics recorded across boots - "
                        "a recurring pattern, not a one-off.")
    blocked = [j for j in journal if j.get("event") == "BLOCKED"]
    if blocked:
        insights.append(f"{len(blocked)} high-risk action attempt(s) were "
                        "blocked by policy. The gate held every time.")
    return insights


def main():
    log_path = Path(sys.argv[1]) if len(sys.argv) > 1 else Path("build/serial.log")
    out_path = (Path(sys.argv[2]) if len(sys.argv) > 2
                else Path("recovery-center/data/recovery-report.json"))

    if not log_path.exists():
        print(f"error: log file not found: {log_path} (boot the OS first)")
        return 2

    lines, _after, found = extract_last_boot_report(log_path.read_text(errors="replace"))
    if not found:
        print("error: no complete boot report in the log - run export-logs in the OS")
        return 2

    (meta, devices, drivers, events, policy, actions, suggestions,
     memory, store, journal, snapshots, health, timeline,
     modules, pkgs, pkg_policy, pkg_history,
     trust, pkg_manifests, pkg_states, mod_slots, pending) = parse_report(lines)

    # Package recommendations: same catalog-only rule as actions.
    pkg_recs = []
    mod_ver = {m.get("module"): m.get("version") for m in modules}
    target_to_module = {"GhostAttach": "GhostAttach",
                        "AI Recovery Core": "AI Recovery Core Rules",
                        "RecoveryEngine": "RecoveryEngine",
                        "Recovery Center": "Recovery Center Schema"}
    for p in pkgs:
        pid = p.get("id")
        if not pid:
            continue
        if p.get("policy_result", "allowed") == "allowed" and \
           p.get("signature_valid", "yes") == "yes" and \
           p.get("status") in ("available", "suggested") and \
           mod_ver.get(target_to_module.get(p.get("target"), "")) == p.get("from"):
            pkg_recs.append({
                "id": pid, "name": p.get("name", ""),
                "risk": p.get("risk", ""), "reversible": p.get("reversible", ""),
                "reason": f"update available {p.get('from')} -> {p.get('to')}",
                "commands": [f"approve-pkg {pid}", f"apply-pkg {pid}",
                             f"verify-pkg {pid}"],
            })
        elif p.get("status") == "failed":
            pkg_recs.append({
                "id": pid, "name": p.get("name", ""),
                "risk": p.get("risk", ""), "reversible": p.get("reversible", ""),
                "reason": "verification failed - restore previous version",
                "commands": [f"rollback-pkg {pid}"],
            })
        elif p.get("status") == "applied":
            pkg_recs.append({
                "id": pid, "name": p.get("name", ""),
                "risk": p.get("risk", ""), "reversible": p.get("reversible", ""),
                "reason": "applied update is PENDING VERIFICATION",
                "commands": [f"verify-pkg {pid}", f"rollback-pkg {pid}"],
            })

    data = {
        "version": meta.get("version", "?"),
        "backend": store.get("backend", memory.get("persistence_mode", "?")),
        "store": store,
        "memory": memory,
        "health": health,
        "snapshots": snapshots,
        "timeline": timeline,
        "devices": devices,
        "drivers": drivers,
        "actions": actions,
        "journal": journal,
        "events": [e for e in events
                   if e.get("severity") in ("warning", "error", "panic")],
        "modules": modules,
        "packages": pkgs,
        "trust": trust,
        "manifests": pkg_manifests,
        "package_states": pkg_states,
        "module_slots": mod_slots,
        "pending_verification": pending.get("pending") == "1",
        "package_policy": pkg_policy,
        "package_history": pkg_history,
        "package_recommendations": pkg_recs,
        "rules": {code: r for code, r in RULES.items()},
        "recommendations": build_recommendations(memory, actions, suggestions),
        "insights": build_insights(snapshots, timeline, journal),
        "safety_notice": ("Recovery Center is read-only. It does not apply "
                          "repairs, write recovery.img, call OS APIs, download "
                          "packages, or change OS state. Buttons only copy "
                          "shell commands for a human to run inside KamikazeOS."),
        "dashboard_contract": {
            "mode": "read_only",
            "source": "exported_serial_report_json",
            "allowed_interaction": "copy_shell_command_text",
        },
    }

    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_text(json.dumps(data, indent=2))
    # file:// companion so the dashboard needs no web server:
    js_path = out_path.with_name("recovery-data.js")
    js_path.write_text("window.RECOVERY_DATA = " + json.dumps(data) + ";\n")

    print(f"wrote {out_path}")
    print(f"wrote {js_path}")
    print("open recovery-center/index.html in a browser to view the dashboard")
    return 0


if __name__ == "__main__":
    sys.exit(main())
