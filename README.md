# KamikazeOS

**An experimental AI-native, self-healing operating system.**

KamikazeOS is a learning + portfolio project built in phases. The end vision is an
OS that can detect errors, missing drivers, broken components, and security risks —
and instead of just printing error messages, uses an integrated AI recovery layer to
explain the problem, suggest fixes ranked by safety, apply *trusted, signed,
reversible* repairs with permission, and recommend upgrades from verified sources.

**The AI never gets magic powers.** Every automated action is policy-gated,
logged, verified, and reversible. See [docs/00-architecture.md](docs/00-architecture.md)
for the safety model and the honest list of what is possible now vs. later vs. never.

## The three layers

| Layer | What it does | Where it lives |
|---|---|---|
| **Kernel Layer** | Boot, memory, interrupts, processes, syscalls, safe mode | `kernel/` (C + assembly, runs in QEMU) |
| **GhostAttach** | Device detection, verified driver matching, safe fallbacks, isolation | `ghostattach/` (Phase 3) |
| **AI Recovery Core** | Log analysis, human-language explanations, risk-ranked fixes, rollback | `ai-recovery/` (Phase 4 — runs *outside* the kernel) |

## Current status: v0.9.0 "Controlled Burn"

**The controlled-change layer.** KamikazeOS can now update itself — through
the narrowest possible gate, with every step human-driven:

- **[module_versions.c](kernel/src/module_versions.c)** — the OS knows what
  it's running (7 tracked components). **[recovery_package.c](kernel/src/recovery_package.c)**
  — a fixed 4-package registry; there is *no API* that loads a package from
  disk or anywhere else, so recommending an unknown package is impossible by
  construction. **[package_policy.c](kernel/src/package_policy.c)** —
  unsigned/invalid signature, high-risk, irreversible, or checksum-mismatch
  packages are blocked unconditionally. PKG-003 (unsigned high-risk driver
  install) exists as the canary: if it ever applies, three rules failed.
- **The lifecycle:** `approve-pkg` → `apply-pkg` → `verify-pkg` (version
  changed + no new criticals, else `failed` → `rollback-pkg`). Every step is
  durably journaled (`PKG-*` events). New commands: `versions`, `packages`,
  `package <id>`, plus the four verbs.
- **Honestly simulated in v0.9:** apply updates version *metadata* only (no
  binary patching), the signature is a labeled placeholder (real signing is
  Phase 10), package status is runtime-only (the journal is durable).
- Report sections `[MODULE_VERSIONS]` `[RECOVERY_PACKAGES]`
  `[PACKAGE_POLICY]` `[PACKAGE_HISTORY]`; analyzer recommends available
  updates with the exact three-command flow; dashboard gained module
  versions, package registry/policy/history panels — copy-only, as always.
- **Real driver bug found & fixed during verification:** QEMU implements ATA
  cache-flush as a host fsync, which can outlast a fast poll loop — about one
  journal write in ten silently vanished (also affecting v0.7/v0.8!). Fixed
  with a patient flush wait + retry in [ata.c](kernel/src/ata.c), and journal
  write failures now log `STORE-WARN-005/006` instead of being silent.

## Previous milestone: v0.8.0 "Vital Signs"

**KamikazeOS now compares itself across time — and shows you.**

- **[boot_snapshot.c](kernel/src/boot_snapshot.c)** — one health record per
  boot (LBA 16–31, 16-boot ring, checksummed): device counts, error counts,
  safe mode, panic evidence, and a deterministic **0–100 health score**
  (100, −15 prev panic, −10/missing driver, −8/isolated, −5/warning,
  −25/critical — simple enough to recompute by hand, which is the point).
- New commands: `snapshots` (per-boot table), `health` (score + reason
  breakdown + trend vs last boot), `timeline` (journal events grouped under
  each boot's health line). New report sections: `[BOOT_SNAPSHOTS]`,
  `[CURRENT_HEALTH]`, `[RECOVERY_TIMELINE]`.
- **Analyzer diffing:** health trend across boots, "safe mode improved
  health by +N points", permanent-gap detection (missing drivers in every
  boot), rollback effect measurement, repeated panic/blocked patterns.
- **Recovery Center** ([recovery-center/](recovery-center/index.html)) — a
  static, **read-only** dashboard: health hero + trend bars, insights,
  recommended actions (copy-the-command only — no apply path exists in the
  code), device/driver/action tables, timeline, snapshots, and explained
  warnings. Generate data with `make dashboard-json`, then open
  `recovery-center/index.html` (works from file:// — no server needed).

## Previous milestone: v0.7.0 "Black Box"

**Recovery memory is now fully durable** — verified across three separate
QEMU processes sharing one disk image:

- **[ata.c](kernel/src/ata.c)** — minimal ATA PIO LBA28 driver (ports
  0x1F0–0x1F7, polled, bounded waits so a missing disk can never hang boot,
  cache-flush after every write). The kernel's first real storage driver.
- **[recovery_store.c](kernel/src/recovery_store.c)** — fixed layout on a raw
  1 MiB image: LBA 1 = checksummed state block, LBA 2–15 = **append-only
  journal** (448 × 16-byte entries: boot number, event, action, result, risk).
  Append-only is a design statement: recovery history is evidence, and when
  the journal fills, appends stop rather than rewrite history.
- **RecoveryMemory is two-tier:** disk first, CMOS NVRAM fallback; every save
  writes both. `memory`/`store`/reports show the backend honestly:
  `disk+CMOS` / `CMOS_only` / `fresh`.
- New commands: `store`, `journal`, `clear-store confirm`. New report
  sections: `[RECOVERY_STORE]`, `[JOURNAL]`. The analyzer reads durable
  history and detects **cross-boot patterns** (repeated panics, repeated
  blocked high-risk attempts).

**What survives what:** shell reboot ✓ · QEMU reset ✓ · full QEMU
close/restart ✓ (verified: a new process booted into scheduled safe mode
with the quarantine and panic evidence intact) · deleting `recovery.img` ✗
(that's `make reset-store`; `make clean` deliberately preserves the image).

## Previous milestone: v0.6.0 "Long Memory"

**The OS now remembers what hurt it.** RecoveryMemory
([recovery_memory.c](kernel/src/recovery_memory.c)) persists recovery state
in **real CMOS/RTC NVRAM** (ports 0x70/0x71, 9 bytes at offsets 0x60–0x68,
magic + version + checksum — corrupt data is never trusted):

- **Persisted quarantines:** `approve ACT-001` survives `reboot` — the next
  boot logs `REC-MEM-001 Restored persisted quarantine` and the catalog entry
  resurrects as `applied` with its recorded prior state, so rollback still
  works across the reboot.
- **Safe mode is real:** `approve ACT-004` sets an NVRAM flag; the next boot
  shows `SAFE MODE BOOT ACTIVE`, attaches only essential devices (display,
  keyboard, serial), isolates everything else without a driver attempt
  (`GA-SAFE-001`), and clears the flag (one-shot).
- **Panic detection:** `kpanic()` burns the evidence into NVRAM before
  halting; the next boot warns `previous boot ended in PANIC`, suggests
  ACT-004, and the analyzer recommends it with the reason. The stored flag is
  cleared only *after* the report exports — crash evidence can't be lost.
- New commands: `memory`, `safe-mode`, `clear-memory confirm`,
  `schedule-safe-mode`. New report section: `[RECOVERY_MEMORY]`.

**Persistence honesty:** survives reboots and panics within a QEMU session
(verified across 3 boots in one process); QEMU recreates CMOS when the
*process* restarts, so closing the emulator resets state. Disk-backed
persistence is the Phase 7 upgrade — only this module's load/save changes.

## Previous milestone: v0.5.0 "Closed Loop"

**The self-healing loop is closed — with a human in the middle:**

```
kernel logs issue -> suggestion rules / host analyzer point at a catalog
action -> human types `approve ACT-001` -> deterministic policy gate
judges it -> reversible state change applied + logged -> `rollback ACT-001`
```

The **RecoveryEngine** ([recovery_actions.c](kernel/src/recovery_actions.c))
holds a fixed five-action catalog — the complete vocabulary of what recovery
can *ever* do. The **policy gate**
([recovery_policy.c](kernel/src/recovery_policy.c)) is ~20 auditable lines:
high-risk → blocked, irreversible → blocked, everything else → human approval.
ACT-999 (`install_unknown_driver`) exists as a canary: if it ever applies,
the gate is broken. New shell commands: `actions`, `suggestions`,
`approve <id>`, `rollback <id>`, `policy`. The boot report gained
`[RECOVERY_POLICY]` / `[RECOVERY_ACTIONS]` / `[RECOVERY_SUGGESTIONS]`
sections, and the analyzer now recommends *only* catalog actions, prints the
exact approve command, and never applies anything. Sample:
[docs/sample-recovery-report-v05.txt](docs/sample-recovery-report-v05.txt).

## Previous milestone: v0.4.0 "Open Channel"

**The AI Recovery Core is online** (as a host-side, rule-based analyzer —
real model integration comes later, the guardrails come first):

1. The kernel exports a machine-readable **boot report** over COM1 at every
   boot ([report.c](kernel/src/report.c)) — version, GhostAttach scan summary,
   all device/driver statuses, full error log — between
   `[KAMIKAZEOS_BOOT_REPORT_START]` / `END` markers. Re-export any time with
   the `export-logs` shell command.
2. QEMU captures it to `build/serial.log` (`make run` / `run.ps1` do this
   automatically; `make run-serial` for headless).
3. The analyzer reads, explains, ranks, and suggests:
   ```
   python ai-recovery/analyze_logs.py build/serial.log
   ```
   It classifies issues (info/warning/critical), maps stable error codes to
   likely causes and safe manual next actions, catches events streamed after
   the report (e.g. panics), and always ends with **"NO AUTOMATIC REPAIR WAS
   PERFORMED"** — the AI reads and advises; a human decides. Sample output:
   [docs/sample-recovery-report-v04.txt](docs/sample-recovery-report-v04.txt).

## Previous milestone: v0.3.0 "Ghost in the Machine"

A real 32-bit x86 kernel that boots in QEMU with an interactive shell, a
structured AI-exportable error log, and the first real **GhostAttach** —
the driver recovery layer (screenshots in [docs/screenshots](docs/screenshots)):

```
help  clear  about  devices  drivers  ghostattach  errors  log  logs-ai
recovery  reboot  panic-test
```

GhostAttach v0.3 ([ghostattach.c](kernel/src/ghostattach.c)) is a deterministic
attach-policy engine over real device/driver registries
([device.h](kernel/src/device.h), [driver.h](kernel/src/driver.h)):
exact verified driver → verified fallback → otherwise the device stays off.
Unverified drivers are **always blocked** (logged as `GA-SEC-001`), failed
drivers get their device **isolated** (`GA-DRV-201`), and every decision lands
in the structured log with a `GA-*` code and recovery hint. The registries are
static in v0.3 (PS/2 + COM1 entries use real probe results; the rest simulate
the policy paths) — a real PCI scan replaces them without touching the engine.

What makes it AI-native already:
- Every kernel event is logged with **code / severity / component / message /
  recovery hint** (e.g. `DRV-101 warning GhostAttach "Storage driver missing"
  hint="fallback unavailable"`).
- Every event is **streamed live over the COM1 serial port** in a
  machine-parseable format the moment it happens — run QEMU with
  `-serial file:build/serial.log` and the host has the evidence even if the
  kernel panics one instruction later.
- `logs-ai` prints the whole log between `[AI_LOG_START]` / `[AI_LOG_END]`
  markers for external AI analysis (Phase 4 plugs in here).
- `recovery` reports the honest status of the self-healing system
  (AI core offline, recovery manual, safe mode/rollback/isolation planned).

## Quick start

Requirements: **LLVM (clang + lld)** and **QEMU** (and optionally GNU make):

```powershell
winget install LLVM.LLVM
winget install SoftwareFreedomConservancy.QEMU
winget install ezwinports.make    # optional, for the Makefile path
```

Build and run — either way works:

```powershell
.\scripts\build.ps1     # PowerShell path (no make needed)
.\scripts\run.ps1       # boots in QEMU, serial log -> build\serial.log
```

```bash
make clean && make      # Makefile path (Linux/macOS/WSL/Git Bash)
make run
```

No GRUB, no ISO, no WSL needed — QEMU's `-kernel` flag loads the Multiboot ELF
directly.

## Repository layout

```
docs/              Architecture decisions, roadmap, screenshots
kernel/
  boot/boot.S      Multiboot header + assembly entry point
  src/
    kernel.c       kmain: subsystem init in dependency order + boot banner
    vga.{c,h}      80x25 VGA text console driver
    keyboard.{c,h} PS/2 keyboard, polling, scancode set 1 -> ASCII
    serial.{c,h}   COM1 UART output - the AI log export channel
    errors.{c,h}   structured error log (code/severity/component/msg/hint)
    panic.{c,h}    controlled-crash handler with diagnostic screen
    shell.{c,h}    interactive shell + all commands
    string.{c,h}   freestanding string/memory helpers (incl. ABI memcpy/memset)
    io.h           inb/outb port I/O inline assembly
  linker.ld        memory layout (kernel at 1 MiB)
Makefile           make / make run / make clean
scripts/           build.ps1, run.ps1, dev-qemu-session.ps1 (automated testing)
ghostattach/       driver attach layer (Phase 3 - placeholder)
ai-recovery/       host-side AI log analyzer (Phase 4 - placeholder)
dashboard/         health/recovery UI (Phase 6 - placeholder)
```

## Roadmap

See [docs/01-roadmap.md](docs/01-roadmap.md). Next: **Phase 3 — GhostAttach
v0.1**: real device registry structs, driver states, fallback matching, PCI
bus scan, and boot-time driver status checks.
