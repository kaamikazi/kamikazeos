# KamikazeOS — Technical Roadmap

Each phase ships something runnable and demo-able. Don't start a phase until
the previous one's exit criteria pass.

---

## Phase 0 — Research & Architecture ✅ (done)
**Deliverable:** [00-architecture.md](00-architecture.md)
- Goals, path decision (hybrid: focused research kernel + host-side AI core), toolchain,
  safety invariants, honest capability ledger.

---

## Phase 1 — Minimal Real OS ✅ (shipped as v0.1.0)
**Goal:** a real kernel boots in QEMU and talks to you.

| Item | Detail |
|---|---|
| Boot | Multiboot v1 header + asm entry (`kernel/boot/boot.S`), loaded by `qemu-system-i386 -kernel` |
| Output | VGA text mode (80×25 @ 0xB8000), colors, scrolling |
| Input | PS/2 keyboard, polling mode (interrupts come in Phase 2) |
| Shell | `help` `clear` `about` `devices` `errors` `log` `reboot` `panic-test` |
| Logging seed | In-memory structured log ring buffer (klog) — seeds Phase 2 |
| Panic seed | `kpanic()` red screen with error code + halt — seeds Phase 2 |

**Exit criteria:** boots in QEMU, every shell command works, `panic-test`
shows a structured panic screen, code is readable by a beginner.

---

## Phase 2 — Error Logging & Recovery Foundation  ◀ current (v0.2.0 shipped the first half)
**Goal:** the OS produces machine-parseable diagnostics. This is the bedrock
of the AI layer — do it carefully.

**Done in v0.2.0:** error schema with recovery hints (`code/severity/
component/message/hint`), live serial streaming of every event, `logs-ai`
export command, `recovery` status command, overflow-safe 128-char shell,
spec'd error codes (BOOT-000, DRV-101/102, AI-000, TEST-PANIC-001).
**Remaining:** IDT + CPU exception handlers, IRQ-driven keyboard, safe-mode
boot flag, error-code taxonomy doc.

- Interrupt descriptor table (IDT) + CPU exception handlers (divide-by-zero,
  page fault, GPF…) routed into the panic/log system with register dumps.
- Keyboard moves from polling to IRQ1.
- **Error code taxonomy:** `KMZ-<subsystem>-<number>` (e.g. `KMZ-KBD-0002`),
  documented in `docs/error-codes.md`.
- **AI-readable log format** (one JSON object per line on the serial port):
  `{"seq":41,"lvl":"ERROR","code":"KMZ-DRV-0007","mod":"ghostattach","msg":"...","ctx":{...}}`
- Log export over QEMU serial → host file (`-serial file:...`). This is the
  pipe the AI Recovery Core will read in Phase 4.
- Safe-mode concept: a boot flag (Multiboot cmdline) that skips non-essential
  init; panic handler records "last boot failed" so next boot can offer it.
- `recover` shell command: show last panic, boot count, safe-mode status.

**Exit criteria:** trigger a divide-by-zero → structured panic logged over
serial → host file contains valid JSON lines describing it.

---

## Phase 3 — GhostAttach  ◀ current (v0.3.0 shipped the architecture)
**Goal:** deterministic driver attach policy. No AI in this layer, ever.

**Done in v0.3.0:** device/driver structs + enums, fixed-size registries,
the full attach policy engine (exact → fallback → block/missing/isolate),
GA-* structured log codes, `devices`/`drivers`/`ghostattach` shell commands.
PS/2 + COM1 entries consume real probe results; other devices simulate the
policy paths. **Remaining:** real PCI config-space scan to fill the device
registry, boot-loop quarantine persistence.

- Device enumeration: PS/2 devices via controller, PCI bus scan (config space
  ports 0xCF8/0xCFC) → vendor/device/class IDs.
- **Driver registry:** static table compiled into the kernel (later: signed
  manifest on disk): `{match: bus+vendor+device | class, driver, verified: true}`.
- Attach pipeline per device:
  `identify → exact match → class fallback → isolate + log`.
- Boot-loop prevention: persist (Phase 2 serial/Phase 5 disk) an attach-attempt
  count per driver; a driver implicated in a crash is quarantined next boot.
- Shell: `devices` now shows real enumerated hardware with attach status
  (`EXACT` / `FALLBACK` / `ISOLATED` / `UNKNOWN`); `ghost log` shows attach history.

**Exit criteria:** plug a virtual device into QEMU (e.g. `-device e1000`),
see it enumerated as UNKNOWN with a structured "no verified driver" log entry;
PS/2 keyboard shows EXACT.

---

## Phase 4 — AI Recovery Core Prototype (host-side)  ◀ current (v0.4.0 shipped the offline core)
**Goal:** real AI analysis of real OS logs — with a human approving everything.

**Done in v0.4.0:** boot report export over serial (report.c), `export-logs`
command, `ai-recovery/analyze_logs.py` — rule-based analyzer that parses the
report, classifies info/warning/critical, explains causes from a stable-code
rule base, suggests safe manual actions, merges post-report events (panics),
and performs no repairs. `analyze_with_ai_later()` marks the model
integration point. **Remaining:** actual Claude API integration with the
fixed-catalog constraint, incident history, approval workflow.

- `ai-recovery/` Python tool on the host:
  1. Reads the serial log file from Phase 2.
  2. Parses + groups incidents (panic + the N log lines before it).
  3. Sends *only structured logs* (no secrets, nothing executable) to the
     Claude API with a strict system prompt: explain the failure, propose
     fixes **chosen from a fixed catalog of repair actions**, assign risk class.
  4. Prints: explanation → ranked fixes → risk → "apply? [y/N]".
  5. "Applying" in Phase 4 = emitting a recovery command/boot flag for the
     emulator (e.g. quarantine driver X next boot) — never patching code.
- Incident history file per "machine" so repeated failures change the advice
  (don't re-propose what already failed).
- **No automatic kernel modification. No auto-apply at all in this phase.**

**Exit criteria:** run `panic-test` in the OS → run the analyzer → get a
correct human-language explanation and a sensible, catalog-constrained fix
proposal that a human approves manually.

---

## Phase 5 — Self-Healing Upgrade System  ◀ current (v0.5.0 shipped the action engine)
**Goal:** closed loop, still policy-gated.

**Done in v0.5.0:** fixed recovery-action catalog (5 actions incl. the
ACT-999 canary), deterministic policy gate (high-risk/irreversible always
blocked), in-kernel suggestion rules, `approve`/`rollback` shell flow with
argument parsing, true rollback (restores recorded prior state), REC-*
structured logging, report sections, analyzer recommends catalog-only with
exact approve commands. **Remaining:** persistence across reboots, signed
update packages, module version tracking, A/B rollback slots.

- Simple persistent storage (virtual disk) for: module version manifest,
  rollback snapshots (A/B module slots), incident history, policy file.
- **Update packages:** versioned, checksummed, signed (ed25519); the kernel
  refuses unsigned packages — including ones the AI recommends.
- **Policy engine** (deterministic, auditable):
  low risk → auto-apply if policy allows; medium → require approval;
  high → approval + rollback point + post-verify; unsafe → block, unappealable.
- Rollback: every change records prior state; failed post-verify → automatic
  rollback; repeated failure → boot to safe mode.
- Upgrade recommendations from **signed release manifests** of configured
  trusted sources only.

**Exit criteria:** approve a module update → watch it apply with a rollback
point → corrupt it deliberately → watch automatic rollback + safe mode work.

---

## Phase 6 (shipped as v0.6.0) — Persistent Recovery Memory
**Goal:** recovery state that survives reboot. ✅

**Done in v0.6.0:** recovery_memory.{c,h} (CMOS NVRAM block, magic+checksum,
9 bytes), persisted quarantines restored at boot (REC-MEM-001/002), real
one-shot safe mode via ACT-004 (SAFE-BOOT-001, essential devices only),
panic detection across reboots (BOOT-WARN-001, evidence cleared only after
export), `memory`/`safe-mode`/`clear-memory`/`schedule-safe-mode` commands,
[RECOVERY_MEMORY] report section, analyzer panic-aware recommendations.
**Limit (honest):** survives in-VM reboots, not QEMU process restarts —
disk-backed persistence is Phase 7.

## Phase 7 (shipped as v0.7.0) — Durable RecoveryStore
**Goal:** recovery state that survives full emulator restarts. ✅

**Done in v0.7.0:** ata.{c,h} PIO LBA28 driver (bounded waits, flush-after-
write), recovery_store.{c,h} (LBA 1 state block + LBA 2–15 append-only
journal, 448 entries, all checksummed), two-tier RecoveryMemory (disk first,
CMOS fallback, dual writes), `store`/`journal`/`clear-store` commands,
[RECOVERY_STORE]/[JOURNAL] report sections, analyzer cross-boot pattern
detection (repeated panics, repeated blocked attempts), Makefile recovery.img
management (`clean` preserves, `reset-store` wipes). Verified across three
separate QEMU processes.

## Phase 8 (shipped as v0.8.0) — Snapshots + Recovery Center
**Goal:** make it legible to humans; this is the portfolio centerpiece. ✅

**Done in v0.8.0:** boot_snapshot.{c,h} (LBA 16–31 ring, deterministic 0–100
health score), `snapshots`/`health`/`timeline` commands,
[BOOT_SNAPSHOTS]/[CURRENT_HEALTH]/[RECOVERY_TIMELINE] report sections,
analyzer health-trend + safe-mode/rollback effect insights,
export_dashboard_json.py, and the read-only Recovery Center static dashboard
(recovery-center/) with copy-only command buttons. Makefile: `analyze`,
`dashboard-json`, `dashboard-open`.

## Phase 9 (shipped as v0.9.0) — Recovery Packages
**Goal:** controlled, verifiable change — the last pillar of the vision. ✅

**Done in v0.9.0:** module_versions.{c,h} (7 components),
recovery_package.{c,h} (fixed 4-package registry incl. PKG-003 unsigned
canary, approve/apply/verify/rollback lifecycle, metadata checksums),
package_policy.{c,h} (unsigned/invalid/high-risk/irreversible/checksum-fail
all blocked), JE_PKG_* durable journal events, four report sections,
analyzer update recommendations + blocked explanations, dashboard package
panels. Apply is simulated (metadata only) and clearly labeled; package
status is runtime-only (journal durable). Plus: fixed the ATA flush-timeout
bug that silently dropped ~1 in 10 journal writes.

## Phase 10 — Real Signed Packages & A/B Slots
**Goal:** make controlled change real.
- Real package manifest format stored in dedicated sectors (LBA 32+).
- Real signature verification (ed25519) over manifests; SIG_TRUSTED earned,
  not declared.
- A/B module slots with verification gates and automatic fall-back to the
  known-good slot on failed verify.
- Persistent package state (survives reboot like quarantines do).

- Web dashboard (local, reads the host-side state from Phases 4–5):
  system health, device list with attach status, AI explanations,
  recommended upgrades, recovery history timeline, one-click rollback
  (which still goes through the policy engine).
- Live demo script: boot OS → inject failure → watch detection → AI
  explanation → approve fix → verify → show rollback history.

---

## Beyond (not promised)
x86_64/UEFI port · userspace + syscalls + ELF loading · in-OS sandboxed
recovery agent · Linux rebase for real hardware. Re-read the "unrealistic or
unsafe" list in Phase 0 before adding anything to this section.
